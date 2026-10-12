#ifndef FIZMO_ASYNC_TIMER_HPP
#define FIZMO_ASYNC_TIMER_HPP

#include "io_context.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace fizmo {
namespace networking {
namespace async {

class SteadyTimer {
public:
    using clock      = IOContext::clock;
    using time_point = IOContext::time_point;
    using duration   = IOContext::duration;
    using WaitHandler = UniqueFunction<void(std::error_code)>;

    explicit SteadyTimer(IOContext& ctx) noexcept;
    SteadyTimer(IOContext& ctx, time_point expiry) noexcept;

    template <typename Rep, typename Period>
    SteadyTimer(IOContext& ctx, const std::chrono::duration<Rep, Period>& d) noexcept : SteadyTimer(ctx) { expires_after(d); }

    ~SteadyTimer();

    SteadyTimer(const SteadyTimer&) = delete;
    SteadyTimer& operator=(const SteadyTimer&) = delete;
    SteadyTimer(SteadyTimer&& other) noexcept;
    SteadyTimer& operator=(SteadyTimer&& other) noexcept;

public:
    std::size_t expires_at(time_point t);

    template <typename Rep, typename Period>
    std::size_t expires_after(const std::chrono::duration<Rep, Period>& d) {
        return expires_at(clock::now() + std::chrono::duration_cast<duration>(d));
    }

    time_point expiry() const noexcept { return m_expiry; }
    bool       expired() const noexcept { return clock::now() >= m_expiry; }

    void async_wait(WaitHandler handler);
    void wait();
    void wait(std::error_code& ec);

    std::size_t cancel();
    std::size_t cancel_one();
    std::size_t pending() const;

    IOContext& context() const noexcept { return *m_ctx; }

private:
    IOContext*    m_ctx;
    std::uint64_t m_id;
    time_point    m_expiry;
};

using DeadlineTimer = SteadyTimer;

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_TIMER_HPP
