#ifndef FIZMO_NODE_LOOP_HPP
#define FIZMO_NODE_LOOP_HPP

#include "events.hpp"
#include "../Async/io_context.hpp"
#include "../Async/resolver.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace fizmo {
namespace networking {

async::IOContext& loop();

std::size_t run();
std::size_t run_once();
std::size_t poll();
std::size_t run_for(std::chrono::milliseconds duration);
void stop();
bool in_loop_thread() noexcept;

void start_background(std::size_t threads = 1);
void stop_background();
bool background_running() noexcept;

void next_tick(std::function<void()> fn);

void set_unhandled_error_handler(std::function<void(const std::string& source, const std::error_code& ec)> handler);

class Timer {
public:
    Timer() noexcept = default;

    void cancel() const;
    void clear() const { cancel(); }
    Timer& refresh();
    bool active() const noexcept;
    bool repeating() const noexcept;
    std::chrono::milliseconds delay() const noexcept;
    explicit operator bool() const noexcept { return active(); }

    struct State;
    explicit Timer(std::shared_ptr<State> state) noexcept : m_state(std::move(state)) {}

private:
    std::shared_ptr<State> m_state;
};

Timer set_timeout(std::function<void()> fn, std::chrono::milliseconds delay);
Timer set_interval(std::function<void()> fn, std::chrono::milliseconds interval);
Timer set_immediate(std::function<void()> fn);

inline Timer set_timeout(std::function<void()> fn, std::int64_t delay_ms) { return set_timeout(std::move(fn), std::chrono::milliseconds(delay_ms)); }
inline Timer set_interval(std::function<void()> fn, std::int64_t interval_ms) { return set_interval(std::move(fn), std::chrono::milliseconds(interval_ms)); }

inline void clear_timeout(const Timer& t) { t.cancel(); }
inline void clear_interval(const Timer& t) { t.cancel(); }
inline void clear_immediate(const Timer& t) { t.cancel(); }

namespace detail {

async::Resolver& default_resolver();

} // namespace detail

} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_LOOP_HPP
