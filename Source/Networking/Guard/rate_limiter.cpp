#include "fizmo_library.hpp"
#include "rate_limiter.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

std::chrono::milliseconds seconds_to_ms(double s) {
    if (!(s > 0.0)) return std::chrono::milliseconds(0);
    if (s > 1e9) s = 1e9;
    return std::chrono::milliseconds(static_cast<std::int64_t>(std::ceil(s * 1000.0)));
}

} // namespace

RateLimitPolicy RateLimitPolicy::per_second(double rate, double burst) {
    RateLimitPolicy p;
    p.capacity = burst;
    p.refill_per_second = rate;
    return p;
}

RateLimitPolicy RateLimitPolicy::per_minute(double rate, double burst) {
    return per_second(rate / 60.0, burst);
}

RateLimitPolicy RateLimitPolicy::per_hour(double rate, double burst) {
    return per_second(rate / 3600.0, burst);
}

TokenBucket::TokenBucket(RateLimitPolicy policy, SteadyTime now) noexcept
    : m_policy(policy), m_tokens(policy.starting_tokens()), m_updated(now), m_last_used(now) {}

double TokenBucket::level_at(SteadyTime now) const noexcept {
    if (now <= m_updated) return m_tokens;
    const double elapsed = std::chrono::duration<double>(now - m_updated).count();
    return std::min(m_policy.capacity, m_tokens + elapsed * m_policy.refill_per_second);
}

RateLimitDecision TokenBucket::decide(double level, double cost, bool take) const noexcept {
    RateLimitDecision d;
    d.limit = m_policy.capacity;
    const double rate = m_policy.refill_per_second;

    if (take) {
        d.allowed = true;
        d.remaining = level - cost;
    } else {
        d.allowed = false;
        d.remaining = level;
        const double need = std::min(cost, m_policy.capacity) - level;
        d.retry_after = rate > 0.0 ? seconds_to_ms(need / rate) : std::chrono::milliseconds(std::chrono::hours(24));
        if (d.retry_after.count() == 0) d.retry_after = std::chrono::milliseconds(1);
    }

    const double missing = m_policy.capacity - d.remaining;
    d.reset_after = rate > 0.0 ? seconds_to_ms(missing / rate) : std::chrono::milliseconds(0);
    if (d.remaining < 0.0) d.remaining = 0.0;
    return d;
}

RateLimitDecision TokenBucket::consume(double cost, SteadyTime now) noexcept {
    const double level = level_at(now);
    m_tokens = level;
    if (now > m_updated) m_updated = now;
    m_last_used = now;
    const bool take = cost <= m_policy.capacity && level + 1e-9 >= cost;
    if (take) m_tokens = level - cost;
    return decide(level, cost, take);
}

RateLimitDecision TokenBucket::peek(double cost, SteadyTime now) const noexcept {
    const double level = level_at(now);
    const bool take = cost <= m_policy.capacity && level + 1e-9 >= cost;
    return decide(level, cost, take);
}

void TokenBucket::refund(double amount, SteadyTime now) noexcept {
    m_tokens = std::min(m_policy.capacity, level_at(now) + amount);
    if (now > m_updated) m_updated = now;
}

void TokenBucket::reset(SteadyTime now) noexcept {
    m_tokens = m_policy.starting_tokens();
    m_updated = now;
    m_last_used = now;
}

double TokenBucket::tokens(SteadyTime now) const noexcept {
    return level_at(now);
}

RateLimiter::RateLimiter(RateLimitPolicy policy, NowFn now)
    : m_policy(policy), m_now(now ? std::move(now) : default_now()) {}

const RateLimitPolicy& RateLimiter::policy_for(const std::string& key) const {
    const auto it = m_overrides.find(key);
    return it == m_overrides.end() ? m_policy : it->second;
}

void RateLimiter::evict_locked(SteadyTime now) {
    if (m_buckets.size() < m_policy.max_keys) return;

    for (auto it = m_buckets.begin(); it != m_buckets.end();) {
        const auto& p = it->second.policy();
        const bool idle = now - it->second.last_used() > p.idle_expiry;
        const bool full = it->second.tokens(now) >= p.capacity;
        if (idle || full) it = m_buckets.erase(it);
        else ++it;
    }

    if (m_buckets.size() < m_policy.max_keys) return;
    auto oldest = m_buckets.begin();

    for (auto it = m_buckets.begin(); it != m_buckets.end(); ++it) {
        if (it->second.last_used() < oldest->second.last_used()) oldest = it;
    }

    if (oldest != m_buckets.end()) m_buckets.erase(oldest);
}

RateLimitDecision RateLimiter::consume(const std::string& key, double cost) {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_buckets.find(key);

    if (it == m_buckets.end()) {
        evict_locked(now);
        it = m_buckets.emplace(key, TokenBucket(policy_for(key), now)).first;
    }

    return it->second.consume(cost, now);
}

RateLimitDecision RateLimiter::peek(const std::string& key, double cost) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_buckets.find(key);
    if (it != m_buckets.end()) return it->second.peek(cost, now);
    return TokenBucket(policy_for(key), now).peek(cost, now);
}

void RateLimiter::refund(const std::string& key, double amount) {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_buckets.find(key);
    if (it != m_buckets.end()) it->second.refund(amount, now);
}

void RateLimiter::reset(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_buckets.erase(key);
}

void RateLimiter::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_buckets.clear();
}

std::size_t RateLimiter::sweep() {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::size_t removed = 0;

    for (auto it = m_buckets.begin(); it != m_buckets.end();) {
        const auto& p = it->second.policy();

        if (now - it->second.last_used() > p.idle_expiry || it->second.tokens(now) >= p.capacity) {
            it = m_buckets.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

std::size_t RateLimiter::size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_buckets.size();
}

void RateLimiter::set_policy(const RateLimitPolicy& policy) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_policy = policy;

    for (auto& kv : m_buckets) {
        if (m_overrides.find(kv.first) == m_overrides.end()) kv.second.set_policy(policy);
    }
}

RateLimitPolicy RateLimiter::policy() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_policy;
}

void RateLimiter::set_key_policy(const std::string& key, const RateLimitPolicy& policy) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_overrides[key] = policy;
    const auto it = m_buckets.find(key);
    if (it != m_buckets.end()) it->second.set_policy(policy);
}

void RateLimiter::clear_key_policy(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_overrides.erase(key);
    const auto it = m_buckets.find(key);
    if (it != m_buckets.end()) it->second.set_policy(m_policy);
}

SlidingWindowLimiter::SlidingWindowLimiter(WindowPolicy policy, NowFn now)
    : m_policy(policy), m_now(now ? std::move(now) : default_now()) {}

void SlidingWindowLimiter::trim(std::deque<SteadyTime>& q, SteadyTime now) const {
    while (!q.empty() && now - q.front() >= m_policy.window) q.pop_front();
}

RateLimitDecision SlidingWindowLimiter::decide(const std::deque<SteadyTime>& q, SteadyTime now, bool allowed) const {
    RateLimitDecision d;
    d.allowed = allowed;
    d.limit = m_policy.limit;
    d.remaining = q.size() >= m_policy.limit ? 0.0 : static_cast<double>(m_policy.limit - q.size());

    if (!q.empty()) {
        const auto until = std::chrono::duration_cast<std::chrono::milliseconds>(q.front() + m_policy.window - now);
        d.reset_after = std::chrono::duration_cast<std::chrono::milliseconds>(q.back() + m_policy.window - now);
        if (!allowed) d.retry_after = until.count() > 0 ? until : std::chrono::milliseconds(1);
    }

    return d;
}

RateLimitDecision SlidingWindowLimiter::hit(const std::string& key) {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);

    if (m_hits.size() >= m_policy.max_keys && m_hits.find(key) == m_hits.end()) {
        for (auto it = m_hits.begin(); it != m_hits.end();) {
            trim(it->second, now);
            if (it->second.empty()) it = m_hits.erase(it);
            else ++it;
        }

        if (m_hits.size() >= m_policy.max_keys) m_hits.erase(m_hits.begin());
    }

    auto& q = m_hits[key];
    trim(q, now);

    if (q.size() >= m_policy.limit) return decide(q, now, false);
    q.push_back(now);
    return decide(q, now, true);
}

RateLimitDecision SlidingWindowLimiter::peek(const std::string& key) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_hits.find(key);
    if (it == m_hits.end()) return decide(std::deque<SteadyTime>(), now, true);
    trim(it->second, now);
    return decide(it->second, now, it->second.size() < m_policy.limit);
}

std::uint32_t SlidingWindowLimiter::count(const std::string& key) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_hits.find(key);
    if (it == m_hits.end()) return 0;
    trim(it->second, now);
    return static_cast<std::uint32_t>(it->second.size());
}

void SlidingWindowLimiter::reset(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_hits.erase(key);
}

void SlidingWindowLimiter::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_hits.clear();
}

std::size_t SlidingWindowLimiter::sweep() {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::size_t removed = 0;

    for (auto it = m_hits.begin(); it != m_hits.end();) {
        trim(it->second, now);

        if (it->second.empty()) {
            it = m_hits.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

} // namespace guard
} // namespace networking
} // namespace fizmo
