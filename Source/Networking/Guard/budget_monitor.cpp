#include "fizmo_library.hpp"
#include "budget_monitor.hpp"

#include <algorithm>

namespace fizmo {
namespace networking {
namespace guard {

const char* to_string(BudgetLevel level) noexcept {
    switch (level) {
        case BudgetLevel::Warning:  return "warning";
        case BudgetLevel::Exceeded: return "exceeded";
        default:                    return "ok";
    }
}

BudgetMonitor::BudgetMonitor(NowFn now) : m_now(now ? std::move(now) : default_now()) {}

void BudgetMonitor::roll(Counter& c, SteadyTime now) const {
    if (c.budget.period.count() <= 0) return;
    const auto period = std::chrono::duration_cast<SteadyClock::duration>(c.budget.period);

    if (now - c.period_start >= period) {
        const auto periods = (now - c.period_start) / period;
        c.period_start += period * periods;
        c.used = 0.0;
        c.notified = BudgetLevel::Ok;
    }
}

BudgetLevel BudgetMonitor::level_of(const Counter& c) const noexcept {
    if (c.budget.limit <= 0.0) return BudgetLevel::Ok;
    if (c.used >= c.budget.limit) return BudgetLevel::Exceeded;
    if (c.used >= c.budget.limit * c.budget.warn_ratio) return BudgetLevel::Warning;
    return BudgetLevel::Ok;
}

BudgetUsage BudgetMonitor::usage_of(const std::string& metric, const Counter& c, SteadyTime now) const {
    BudgetUsage u;
    u.metric = metric;
    u.used = c.used;
    u.limit = c.budget.limit;
    u.ratio = c.budget.limit > 0.0 ? c.used / c.budget.limit : 0.0;
    u.level = level_of(c);
    u.unit = c.budget.unit;

    if (c.budget.period.count() > 0) {
        const auto end = c.period_start + std::chrono::duration_cast<SteadyClock::duration>(c.budget.period);
        u.resets_in = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
    }

    return u;
}

void BudgetMonitor::set_budget(const Budget& budget) {
    std::lock_guard<std::mutex> lk(m_mutex);
    Counter& c = m_counters[budget.metric];
    const bool fresh = c.budget.metric.empty();
    c.budget = budget;
    if (fresh) c.period_start = m_now();
}

bool BudgetMonitor::remove_budget(const std::string& metric) {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_counters.erase(metric) != 0;
}

BudgetLevel BudgetMonitor::add(const std::string& metric, double amount) {
    const SteadyTime now = m_now();
    std::optional<BudgetAlert> alert;
    AlertCallback cb;
    BudgetLevel level;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_counters.find(metric);
        if (it == m_counters.end()) return BudgetLevel::Ok;
        Counter& c = it->second;
        roll(c, now);
        c.used += amount;
        level = level_of(c);

        if (level > c.notified) {
            c.notified = level;
            const BudgetUsage u = usage_of(metric, c, now);
            alert = BudgetAlert{ metric, level, u.used, u.limit, u.unit, u.resets_in };
            cb = m_on_alert;
        }
    }

    if (alert && cb) cb(*alert);
    return level;
}

bool BudgetMonitor::allows(const std::string& metric, double amount) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_counters.find(metric);
    if (it == m_counters.end()) return true;
    Counter& c = it->second;
    roll(c, now);
    if (!c.budget.hard_stop || c.budget.limit <= 0.0) return true;
    return c.used + amount <= c.budget.limit;
}

bool BudgetMonitor::consume(const std::string& metric, double amount) {
    if (!allows(metric, amount)) {
        add(metric, 0.0);
        return false;
    }

    add(metric, amount);
    return true;
}

std::optional<BudgetUsage> BudgetMonitor::usage(const std::string& metric) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_counters.find(metric);
    if (it == m_counters.end()) return std::nullopt;
    roll(it->second, now);
    return usage_of(metric, it->second, now);
}

std::vector<BudgetUsage> BudgetMonitor::usages() const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<BudgetUsage> out;

    for (auto& kv : m_counters) {
        roll(kv.second, now);
        out.push_back(usage_of(kv.first, kv.second, now));
    }

    std::sort(out.begin(), out.end(), [](const BudgetUsage& a, const BudgetUsage& b) { return a.ratio > b.ratio; });
    return out;
}

void BudgetMonitor::reset(const std::string& metric) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_counters.find(metric);
    if (it == m_counters.end()) return;
    it->second.used = 0.0;
    it->second.notified = BudgetLevel::Ok;
    it->second.period_start = m_now();
}

void BudgetMonitor::set_gauge_threshold(const std::string& metric, double warn, double critical) {
    std::lock_guard<std::mutex> lk(m_mutex);
    Gauge& g = m_gauges[metric];
    g.warn = warn;
    g.critical = critical;
}

BudgetLevel BudgetMonitor::set_gauge(const std::string& metric, double value) {
    std::optional<BudgetAlert> alert;
    AlertCallback cb;
    BudgetLevel level = BudgetLevel::Ok;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_gauges.find(metric);
        if (it == m_gauges.end()) return BudgetLevel::Ok;
        Gauge& g = it->second;
        g.value = value;
        if (g.critical > 0.0 && value >= g.critical) level = BudgetLevel::Exceeded;
        else if (g.warn > 0.0 && value >= g.warn) level = BudgetLevel::Warning;

        if (level > g.notified) {
            alert = BudgetAlert{ metric, level, value, g.critical, std::string(), std::chrono::milliseconds(0) };
            cb = m_on_alert;
        }

        if (level < g.notified && (g.warn <= 0.0 || value < g.warn * 0.9)) g.notified = level;
        else if (level > g.notified) g.notified = level;
    }

    if (alert && cb) cb(*alert);
    return level;
}

void BudgetMonitor::on_alert(AlertCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_alert = std::move(cb);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
