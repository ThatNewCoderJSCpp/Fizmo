#include "fizmo_library.hpp"
#include "timer.hpp"

#include <thread>

namespace fizmo {
namespace networking {
namespace async {

SteadyTimer::SteadyTimer(IOContext& ctx) noexcept : m_ctx(&ctx), m_id(ctx.new_timer_id()), m_expiry(clock::now()) {}

SteadyTimer::SteadyTimer(IOContext& ctx, time_point expiry) noexcept : m_ctx(&ctx), m_id(ctx.new_timer_id()), m_expiry(expiry) {}

SteadyTimer::~SteadyTimer() {
    if (m_ctx && m_id != 0) m_ctx->cancel_timer(m_id);
}

SteadyTimer::SteadyTimer(SteadyTimer&& other) noexcept : m_ctx(other.m_ctx), m_id(other.m_id), m_expiry(other.m_expiry) {
    other.m_id = 0;
}

SteadyTimer& SteadyTimer::operator=(SteadyTimer&& other) noexcept {
    if (this != &other) {
        if (m_ctx && m_id != 0) m_ctx->cancel_timer(m_id);
        m_ctx = other.m_ctx;
        m_id = other.m_id;
        m_expiry = other.m_expiry;
        other.m_id = 0;
    }

    return *this;
}

std::size_t SteadyTimer::expires_at(time_point t) {
    const std::size_t n = m_id != 0 ? m_ctx->cancel_timer(m_id) : 0;
    if (m_id == 0) m_id = m_ctx->new_timer_id();
    m_expiry = t;
    return n;
}

void SteadyTimer::async_wait(WaitHandler handler) {
    if (m_id == 0) m_id = m_ctx->new_timer_id();
    m_ctx->schedule_timer(m_id, m_expiry, std::move(handler));
}

void SteadyTimer::wait() {
    std::error_code ec;
    wait(ec);
}

void SteadyTimer::wait(std::error_code& ec) {
    ec.clear();
    const auto now = clock::now();
    if (m_expiry > now) std::this_thread::sleep_until(m_expiry);
}

std::size_t SteadyTimer::cancel() {
    return m_id != 0 ? m_ctx->cancel_timer(m_id) : 0;
}

std::size_t SteadyTimer::cancel_one() {
    return m_id != 0 ? m_ctx->cancel_timer(m_id, 1) : 0;
}

std::size_t SteadyTimer::pending() const {
    return m_id != 0 ? m_ctx->pending_timer_waits(m_id) : 0;
}

} // namespace async
} // namespace networking
} // namespace fizmo
