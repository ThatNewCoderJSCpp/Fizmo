#ifndef FIZMO_GUARD_FAILURE_TRACKER_HPP
#define FIZMO_GUARD_FAILURE_TRACKER_HPP

#include "guard_types.hpp"

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct FailurePolicy {
    std::chrono::seconds      window               = std::chrono::minutes(15);
    std::uint32_t             max_failures         = 10;
    std::uint32_t             min_samples          = 20;
    double                    max_failure_ratio    = 0.5;
    std::chrono::seconds      lockout              = std::chrono::minutes(1);
    double                    lockout_multiplier   = 2.0;
    std::chrono::seconds      max_lockout          = std::chrono::hours(24);
    std::chrono::seconds      strike_memory        = std::chrono::hours(24);
    std::uint32_t             slowdown_after       = 3;
    std::chrono::milliseconds slowdown_step{250};
    std::chrono::milliseconds max_slowdown{5000};
    std::size_t               max_keys             = 100000;
};

struct FailureStatus {
    std::uint32_t             failures     = 0;
    std::uint32_t             successes    = 0;
    double                    failure_ratio = 0.0;
    bool                      locked       = false;
    std::uint32_t             strikes      = 0;
    std::chrono::milliseconds lock_remaining{0};
    std::chrono::milliseconds suggested_delay{0};

    explicit operator bool() const noexcept { return !locked; }
};

class FailureTracker {
public:
    using LockoutCallback = std::function<void(const std::string& key, const FailureStatus& status)>;

    explicit FailureTracker(FailurePolicy policy = FailurePolicy(), NowFn now = default_now());

    FailureStatus record_failure(const std::string& key);
    FailureStatus record_success(const std::string& key, bool forgive = false);
    FailureStatus status(const std::string& key) const;
    bool is_locked(const std::string& key) const;
    void unlock(const std::string& key);
    void reset(const std::string& key);
    void clear();
    std::size_t sweep();
    std::vector<std::string> locked_keys() const;

    void on_lockout(LockoutCallback cb);
    const FailurePolicy& policy() const noexcept { return m_policy; }

private:
    struct Entry {
        std::deque<std::pair<SteadyTime, bool>> events;
        std::uint32_t                           failures = 0;
        std::uint32_t                           successes = 0;
        std::uint32_t                           strikes = 0;
        SteadyTime                              locked_until{};
        SteadyTime                              last_strike{};
        SteadyTime                              last_seen{};
    };

    FailurePolicy                          m_policy;
    NowFn                                  m_now;
    mutable std::mutex                     m_mutex;
    mutable std::unordered_map<std::string, Entry> m_entries;
    LockoutCallback                        m_on_lockout;

    void trim(Entry& e, SteadyTime now) const;
    FailureStatus make_status(const Entry& e, SteadyTime now) const;
    bool should_lock(const Entry& e) const;
    void evict_locked(SteadyTime now);
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_FAILURE_TRACKER_HPP
