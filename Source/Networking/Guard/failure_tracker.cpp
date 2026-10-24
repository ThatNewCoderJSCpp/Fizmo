#include "fizmo_library.hpp"
#include "failure_tracker.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace networking {
namespace guard {

FailureTracker::FailureTracker(FailurePolicy policy, NowFn now)
    : m_policy(policy), m_now(now ? std::move(now) : default_now()) {}

void FailureTracker::trim(Entry& e, SteadyTime now) const {
    while (!e.events.empty() && now - e.events.front().first >= m_policy.window) {
        if (e.events.front().second) --e.successes;
        else --e.failures;
        e.events.pop_front();
    }

    if (e.strikes && now - e.last_strike >= m_policy.strike_memory && now >= e.locked_until) e.strikes = 0;
}

bool FailureTracker::should_lock(const Entry& e) const {
    if (m_policy.max_failures && e.failures >= m_policy.max_failures) return true;
    const std::uint32_t total = e.failures + e.successes;
    if (m_policy.min_samples && total >= m_policy.min_samples) {
        return static_cast<double>(e.failures) / static_cast<double>(total) >= m_policy.max_failure_ratio;
    }
    return false;
}

FailureStatus FailureTracker::make_status(const Entry& e, SteadyTime now) const {
    FailureStatus s;
    s.failures = e.failures;
    s.successes = e.successes;
    const std::uint32_t total = e.failures + e.successes;
    s.failure_ratio = total ? static_cast<double>(e.failures) / static_cast<double>(total) : 0.0;
    s.strikes = e.strikes;
    s.locked = now < e.locked_until;
    if (s.locked) s.lock_remaining = std::chrono::duration_cast<std::chrono::milliseconds>(e.locked_until - now);

    if (m_policy.slowdown_after && e.failures >= m_policy.slowdown_after) {
        const auto steps = static_cast<std::int64_t>(e.failures - m_policy.slowdown_after + 1);
        s.suggested_delay = std::min(m_policy.max_slowdown, std::chrono::milliseconds(m_policy.slowdown_step.count() * steps));
    }

    if (s.locked && s.lock_remaining > s.suggested_delay) s.suggested_delay = s.lock_remaining;
    return s;
}

void FailureTracker::evict_locked(SteadyTime now) {
    if (m_entries.size() < m_policy.max_keys) return;

    for (auto it = m_entries.begin(); it != m_entries.end();) {
        trim(it->second, now);
        if (it->second.events.empty() && now >= it->second.locked_until && it->second.strikes == 0) it = m_entries.erase(it);
        else ++it;
    }

    if (m_entries.size() < m_policy.max_keys) return;
    auto oldest = m_entries.begin();

    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (now < it->second.locked_until) continue;
        if (now < oldest->second.locked_until || it->second.last_seen < oldest->second.last_seen) oldest = it;
    }

    if (oldest != m_entries.end()) m_entries.erase(oldest);
}

FailureStatus FailureTracker::record_failure(const std::string& key) {
    const SteadyTime now = m_now();
    FailureStatus status;
    LockoutCallback cb;
    bool fired = false;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_entries.find(key) == m_entries.end()) evict_locked(now);
        Entry& e = m_entries[key];
        trim(e, now);
        e.last_seen = now;
        e.events.emplace_back(now, false);
        ++e.failures;

        if (now >= e.locked_until && should_lock(e)) {
            ++e.strikes;
            e.last_strike = now;
            const double factor = std::pow(m_policy.lockout_multiplier, static_cast<double>(e.strikes - 1));
            const double secs = std::min(static_cast<double>(m_policy.max_lockout.count()), static_cast<double>(m_policy.lockout.count()) * factor);
            e.locked_until = now + std::chrono::duration_cast<SteadyClock::duration>(std::chrono::duration<double>(secs));
            status = make_status(e, now);
            e.events.clear();
            e.failures = 0;
            e.successes = 0;
            cb = m_on_lockout;
            fired = true;
        } else {
            status = make_status(e, now);
        }
    }

    if (fired && cb) cb(key, status);
    return status;
}

FailureStatus FailureTracker::record_success(const std::string& key, bool forgive) {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_entries.find(key);

    if (it == m_entries.end()) {
        if (forgive) return FailureStatus();
        evict_locked(now);
        it = m_entries.emplace(key, Entry()).first;
    }

    Entry& e = it->second;
    trim(e, now);
    e.last_seen = now;

    if (forgive) {
        e.events.clear();
        e.failures = 0;
        e.successes = 0;
        if (now >= e.locked_until) e.strikes = 0;
        return make_status(e, now);
    }

    e.events.emplace_back(now, true);
    ++e.successes;
    return make_status(e, now);
}

FailureStatus FailureTracker::status(const std::string& key) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(key);
    if (it == m_entries.end()) return FailureStatus();
    trim(it->second, now);
    return make_status(it->second, now);
}

bool FailureTracker::is_locked(const std::string& key) const {
    return status(key).locked;
}

void FailureTracker::unlock(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(key);
    if (it != m_entries.end()) it->second.locked_until = SteadyTime{};
}

void FailureTracker::reset(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_entries.erase(key);
}

void FailureTracker::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_entries.clear();
}

std::size_t FailureTracker::sweep() {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::size_t removed = 0;

    for (auto it = m_entries.begin(); it != m_entries.end();) {
        trim(it->second, now);

        if (it->second.events.empty() && now >= it->second.locked_until && it->second.strikes == 0) {
            it = m_entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

std::vector<std::string> FailureTracker::locked_keys() const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::string> out;

    for (const auto& kv : m_entries) {
        if (now < kv.second.locked_until) out.push_back(kv.first);
    }

    return out;
}

void FailureTracker::on_lockout(LockoutCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_lockout = std::move(cb);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
