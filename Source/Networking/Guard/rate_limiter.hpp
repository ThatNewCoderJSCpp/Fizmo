#ifndef FIZMO_GUARD_RATE_LIMITER_HPP
#define FIZMO_GUARD_RATE_LIMITER_HPP

#include "guard_types.hpp"

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace guard {

struct RateLimitPolicy {
    double                    capacity          = 60.0;
    double                    initial_tokens    = -1.0;
    double                    refill_per_second = 1.0;
    std::chrono::seconds      idle_expiry       = std::chrono::minutes(10);
    std::size_t               max_keys          = 100000;

    static RateLimitPolicy per_second(double rate, double burst);
    static RateLimitPolicy per_minute(double rate, double burst);
    static RateLimitPolicy per_hour(double rate, double burst);

    double starting_tokens() const noexcept { return initial_tokens < 0.0 ? capacity : (initial_tokens > capacity ? capacity : initial_tokens); }
};

struct RateLimitDecision {
    bool                      allowed     = true;
    double                    remaining   = 0.0;
    double                    limit       = 0.0;
    std::chrono::milliseconds retry_after{0};
    std::chrono::milliseconds reset_after{0};

    explicit operator bool() const noexcept { return allowed; }
};

class TokenBucket {
public:
    explicit TokenBucket(RateLimitPolicy policy = RateLimitPolicy(), SteadyTime now = SteadyClock::now()) noexcept;

    RateLimitDecision consume(double cost, SteadyTime now) noexcept;
    RateLimitDecision peek(double cost, SteadyTime now) const noexcept;
    void refund(double amount, SteadyTime now) noexcept;
    void reset(SteadyTime now) noexcept;
    double tokens(SteadyTime now) const noexcept;
    SteadyTime last_used() const noexcept { return m_last_used; }
    const RateLimitPolicy& policy() const noexcept { return m_policy; }
    void set_policy(const RateLimitPolicy& policy) noexcept { m_policy = policy; }

private:
    RateLimitPolicy m_policy;
    double          m_tokens;
    SteadyTime      m_updated;
    SteadyTime      m_last_used;

    double level_at(SteadyTime now) const noexcept;
    RateLimitDecision decide(double level, double cost, bool take) const noexcept;
};

class RateLimiter {
public:
    explicit RateLimiter(RateLimitPolicy policy = RateLimitPolicy(), NowFn now = default_now());

    RateLimitDecision consume(const std::string& key, double cost = 1.0);
    RateLimitDecision peek(const std::string& key, double cost = 1.0) const;
    void refund(const std::string& key, double amount = 1.0);
    void reset(const std::string& key);
    void clear();
    std::size_t sweep();
    std::size_t size() const;

    void set_policy(const RateLimitPolicy& policy);
    RateLimitPolicy policy() const;
    void set_key_policy(const std::string& key, const RateLimitPolicy& policy);
    void clear_key_policy(const std::string& key);

private:
    RateLimitPolicy                                  m_policy;
    NowFn                                            m_now;
    mutable std::mutex                               m_mutex;
    std::unordered_map<std::string, TokenBucket>     m_buckets;
    std::unordered_map<std::string, RateLimitPolicy> m_overrides;

    const RateLimitPolicy& policy_for(const std::string& key) const;
    void evict_locked(SteadyTime now);
};

struct WindowPolicy {
    std::uint32_t        limit  = 10;
    std::chrono::seconds window = std::chrono::minutes(1);
    std::size_t          max_keys = 100000;
};

class SlidingWindowLimiter {
public:
    explicit SlidingWindowLimiter(WindowPolicy policy = WindowPolicy(), NowFn now = default_now());

    RateLimitDecision hit(const std::string& key);
    RateLimitDecision peek(const std::string& key) const;
    std::uint32_t count(const std::string& key) const;
    void reset(const std::string& key);
    void clear();
    std::size_t sweep();

    const WindowPolicy& policy() const noexcept { return m_policy; }

private:
    WindowPolicy                                         m_policy;
    NowFn                                                m_now;
    mutable std::mutex                                   m_mutex;
    mutable std::unordered_map<std::string, std::deque<SteadyTime>> m_hits;

    void trim(std::deque<SteadyTime>& q, SteadyTime now) const;
    RateLimitDecision decide(const std::deque<SteadyTime>& q, SteadyTime now, bool allowed) const;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_RATE_LIMITER_HPP
