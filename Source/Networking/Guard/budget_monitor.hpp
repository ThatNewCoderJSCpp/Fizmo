#ifndef FIZMO_GUARD_BUDGET_MONITOR_HPP
#define FIZMO_GUARD_BUDGET_MONITOR_HPP

#include "guard_types.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct Budget {
    std::string          metric;
    double               limit      = 0.0;
    std::chrono::seconds period     = std::chrono::hours(24);
    double               warn_ratio = 0.8;
    bool                 hard_stop  = false;
    std::string          unit;
};

enum class BudgetLevel {
    Ok,
    Warning,
    Exceeded
};

const char* to_string(BudgetLevel level) noexcept;

struct BudgetAlert {
    std::string metric;
    BudgetLevel level = BudgetLevel::Ok;
    double      used = 0.0;
    double      limit = 0.0;
    std::string unit;
    std::chrono::milliseconds resets_in{0};
};

struct BudgetUsage {
    std::string               metric;
    double                    used = 0.0;
    double                    limit = 0.0;
    double                    ratio = 0.0;
    BudgetLevel               level = BudgetLevel::Ok;
    std::string               unit;
    std::chrono::milliseconds resets_in{0};
};

class BudgetMonitor {
public:
    using AlertCallback = std::function<void(const BudgetAlert& alert)>;

    explicit BudgetMonitor(NowFn now = default_now());

    void set_budget(const Budget& budget);
    bool remove_budget(const std::string& metric);
    BudgetLevel add(const std::string& metric, double amount = 1.0);
    bool allows(const std::string& metric, double amount = 1.0) const;
    bool consume(const std::string& metric, double amount = 1.0);
    std::optional<BudgetUsage> usage(const std::string& metric) const;
    std::vector<BudgetUsage> usages() const;
    void reset(const std::string& metric);

    void set_gauge_threshold(const std::string& metric, double warn, double critical);
    BudgetLevel set_gauge(const std::string& metric, double value);

    void on_alert(AlertCallback cb);

private:
    struct Counter {
        Budget      budget;
        double      used = 0.0;
        SteadyTime  period_start{};
        BudgetLevel notified = BudgetLevel::Ok;
    };

    struct Gauge {
        double      warn = 0.0;
        double      critical = 0.0;
        double      value = 0.0;
        BudgetLevel notified = BudgetLevel::Ok;
    };

    NowFn                                    m_now;
    mutable std::mutex                       m_mutex;
    mutable std::unordered_map<std::string, Counter> m_counters;
    std::unordered_map<std::string, Gauge>   m_gauges;
    AlertCallback                            m_on_alert;

    void roll(Counter& c, SteadyTime now) const;
    BudgetLevel level_of(const Counter& c) const noexcept;
    BudgetUsage usage_of(const std::string& metric, const Counter& c, SteadyTime now) const;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_BUDGET_MONITOR_HPP
