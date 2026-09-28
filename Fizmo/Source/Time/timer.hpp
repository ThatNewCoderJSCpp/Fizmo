#ifndef FIZMO_TIME_TIMER_HPP
#define FIZMO_TIME_TIMER_HPP

#include "ticks.hpp"
#include "duration.hpp"
#include "time_unit.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <thread>
#include <utility>

namespace fizmo {
namespace time {

class DurationSpec {
private:
    std::uint64_t ns_;

public:
    constexpr DurationSpec() noexcept : ns_(0) {}

    template<typename... Args, typename = typename std::enable_if<(sizeof...(Args) > 0) && (is_time_unit_v<Args> && ...)>::type>
    constexpr DurationSpec(const Args&... durations) noexcept : ns_(detail::sum_ns(durations...)) {}

    template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
    DurationSpec(const T& value, Unit unit) noexcept : ns_(detail::runtime_to_ns(value, unit)) {}

    template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
    DurationSpec(std::initializer_list<std::pair<T, Unit>> pairs) noexcept : ns_(detail::sum_pairs_ns(pairs)) {}

    template<typename D, typename = typename std::enable_if<is_time_unit_v<D>>::type>
    DurationSpec(std::initializer_list<D> list) noexcept : ns_(0) {
        for (const auto& d : list) ns_ += detail::sum_ns(d);
    }

    static constexpr DurationSpec from_ns(std::uint64_t ns) noexcept {
        DurationSpec s;
        s.ns_ = ns;
        return s;
    }

    constexpr std::uint64_t ns() const noexcept { return ns_; }
    constexpr bool is_zero()   const noexcept { return ns_ == 0; }

    std::chrono::nanoseconds chrono() const noexcept {
        return std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(ns_));
    }
};

namespace detail {

class TimerBase {
protected:
    using steady = std::chrono::steady_clock;

    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::thread             worker_;

    bool                running_;
    bool                paused_;
    bool                stop_requested_;
    std::exception_ptr  error_;

    TimerBase() noexcept : running_(false), paused_(false), stop_requested_(false) {}

    ~TimerBase() { shutdown(); }

    TimerBase(const TimerBase&)            = delete;
    TimerBase& operator=(const TimerBase&) = delete;
    TimerBase(TimerBase&&)                 = delete;
    TimerBase& operator=(TimerBase&&)      = delete;

    template<typename Fn>
    void invoke_unlocked(std::unique_lock<std::mutex>& lock, Fn&& fn) {
        lock.unlock();

        try {
            fn();
        } catch (...) {
            lock.lock();
            error_ = std::current_exception();
            stop_requested_ = true;
            return;
        }

        lock.lock();
    }

    void shutdown() noexcept {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_requested_ = true;
            paused_ = false;
        }

        cv_.notify_all();

        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            worker_.join();
        }
    }

public:
    bool is_running() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return running_;
    }

    bool is_paused() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return paused_;
    }

    void pause() noexcept {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!running_ || paused_) return;
            paused_ = true;
        }

        cv_.notify_all();
    }

    void resume() noexcept {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!paused_) return;
            paused_ = false;
        }

        cv_.notify_all();
    }

    void stop() noexcept { shutdown(); }

    void join() {
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            worker_.join();
        }
    }

    bool has_error() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return static_cast<bool>(error_);
    }

    void rethrow_if_error() const {
        std::exception_ptr e;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            e = error_;
        }

        if (e) std::rethrow_exception(e);
    }
};

} // namespace detail

template<typename Int = default_wide_int, typename = typename std::enable_if<detail::is_signed_integer_like_v<Int>>::type>
class CountdownTimer : public detail::TimerBase {
public:
    using int_type      = Int;
    using callback_type = std::function<void()>;
    using duration_type = Duration<Unit::nanosecond, Int>;

private:
    DurationSpec  interval_;
    callback_type callback_;
    std::uint64_t remaining_ns_;
    bool          fired_;

public:
    CountdownTimer() noexcept : remaining_ns_(0), fired_(false) {}

    explicit CountdownTimer(DurationSpec interval, callback_type cb = nullptr) : interval_(interval), callback_(std::move(cb)), remaining_ns_(interval.ns()), fired_(false) {}

    template<typename... Args, typename = typename std::enable_if<(sizeof...(Args) > 1) && (is_time_unit_v<Args> && ...)>::type>
    explicit CountdownTimer(const Args&... durations) : interval_(durations...), callback_(nullptr), remaining_ns_(interval_.ns()), fired_(false) {}

    template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
    CountdownTimer(const T& value, Unit unit) : interval_(value, unit), callback_(nullptr), remaining_ns_(interval_.ns()), fired_(false) {}

    ~CountdownTimer() { shutdown(); }

public:
    CountdownTimer& set_interval(DurationSpec interval) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);

        if (!running_) {
            interval_     = interval;
            remaining_ns_ = interval.ns();
        }

        return *this;
    }

    CountdownTimer& set_callback(callback_type cb) {
        std::lock_guard<std::mutex> lock(mtx_);
        callback_ = std::move(cb);
        return *this;
    }

    duration_type interval() const noexcept {
        return duration_type(static_cast<Int>(interval_.ns()));
    }

    duration_type remaining() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return duration_type(static_cast<Int>(remaining_ns_));
    }

    duration_type elapsed() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        const std::uint64_t total = interval_.ns();
        return duration_type(static_cast<Int>(remaining_ns_ >= total ? 0 : total - remaining_ns_));
    }

    bool has_fired() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return fired_;
    }

public:
    // Non-blocking
    bool start() {
        std::unique_lock<std::mutex> lock(mtx_);
        if (running_ || !callback_ || interval_.is_zero()) return false;
        if (worker_.joinable()) { lock.unlock(); worker_.join(); lock.lock(); }
        running_        = true;
        paused_         = false;
        stop_requested_ = false;
        fired_          = false;
        error_          = nullptr;
        remaining_ns_   = interval_.ns();
        worker_ = std::thread([this] { loop_(); });
        return true;
    }

    bool start(callback_type cb) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (running_) return false;
            callback_ = std::move(cb);
        }

        return start();
    }

    // Blocking
    bool run() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            if (running_ || !callback_ || interval_.is_zero()) return false;
            running_        = true;
            paused_         = false;
            stop_requested_ = false;
            fired_          = false;
            error_          = nullptr;
            remaining_ns_   = interval_.ns();
        }

        loop_();
        rethrow_if_error();
        return has_fired();
    }

    bool run(callback_type cb) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (running_) return false;
            callback_ = std::move(cb);
        }

        return run();
    }

    void cancel() noexcept {
        shutdown();
        std::lock_guard<std::mutex> lock(mtx_);
        remaining_ns_ = interval_.ns();
        fired_        = false;
    }

    bool restart() {
        cancel();
        return start();
    }

private:
    void loop_() {
        std::unique_lock<std::mutex> lock(mtx_);
        steady::time_point deadline = steady::now() + std::chrono::nanoseconds(remaining_ns_);

        for (;;) {
            if (stop_requested_) break;

            if (paused_) {
                cv_.wait(lock, [this] { return !paused_ || stop_requested_; });
                if (stop_requested_) break;
                deadline = steady::now() + std::chrono::nanoseconds(remaining_ns_);
                continue;
            }

            if (cv_.wait_until(lock, deadline, [this] { return stop_requested_ || paused_; })) {
                if (stop_requested_) break;
                const steady::time_point now = steady::now();
                remaining_ns_ = (now >= deadline) ? 0 : static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count());
                continue;
            }

            remaining_ns_ = 0;
            fired_        = true;
            callback_type cb = callback_;
            invoke_unlocked(lock, [&cb] { cb(); });
            break;
        }

        running_ = false;
        cv_.notify_all();
    }
};

template<typename Int = default_wide_int, typename = typename std::enable_if<detail::is_signed_integer_like_v<Int>>::type>
class RepeatingTimer : public detail::TimerBase {
public:
    using int_type       = Int;
    using callback_type  = std::function<void()>;
    using indexed_type   = std::function<void(std::size_t)>;
    using duration_type  = Duration<Unit::nanosecond, Int>;

private:
    DurationSpec  interval_;
    DurationSpec  limit_;          
    std::size_t   max_ticks_;      
    indexed_type  callback_;
    std::size_t   ticks_;
    std::size_t   missed_;
    bool          skip_missed_;

public:
    RepeatingTimer() noexcept : max_ticks_(0), ticks_(0), missed_(0), skip_missed_(true) {}

    explicit RepeatingTimer(DurationSpec interval, indexed_type cb = nullptr) : interval_(interval), max_ticks_(0), callback_(std::move(cb)), ticks_(0), missed_(0), skip_missed_(true) {}

    RepeatingTimer(DurationSpec interval, callback_type cb) : interval_(interval), max_ticks_(0), ticks_(0), missed_(0), skip_missed_(true) {
        set_callback(std::move(cb));
    }

    template<typename... Args, typename = typename std::enable_if<(sizeof...(Args) > 1) && (is_time_unit_v<Args> && ...)>::type>
    explicit RepeatingTimer(const Args&... durations) : interval_(durations...), max_ticks_(0), callback_(nullptr), ticks_(0), missed_(0), skip_missed_(true) {}

    template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
    RepeatingTimer(const T& value, Unit unit) : interval_(value, unit), max_ticks_(0), callback_(nullptr), ticks_(0), missed_(0), skip_missed_(true) {}

    ~RepeatingTimer() { shutdown(); }

public:
    RepeatingTimer& set_interval(DurationSpec interval) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!running_) interval_ = interval;
        return *this;
    }

    RepeatingTimer& for_duration(DurationSpec total) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!running_) limit_ = total;
        return *this;
    }

    RepeatingTimer& repeat_count(std::size_t n) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!running_) max_ticks_ = n;
        return *this;
    }

    RepeatingTimer& run_forever() noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!running_) { limit_ = DurationSpec(); max_ticks_ = 0; }
        return *this;
    }

    RepeatingTimer& set_skip_missed(bool skip) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        skip_missed_ = skip;
        return *this;
    }

    RepeatingTimer& set_callback(indexed_type cb) {
        std::lock_guard<std::mutex> lock(mtx_);
        callback_ = std::move(cb);
        return *this;
    }

    RepeatingTimer& set_callback(callback_type cb) {
        std::lock_guard<std::mutex> lock(mtx_);
        callback_ = [fn = std::move(cb)](std::size_t) { fn(); };
        return *this;
    }

    duration_type interval() const noexcept {
        return duration_type(static_cast<Int>(interval_.ns()));
    }

    std::size_t tick_count() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return ticks_;
    }

    std::size_t missed_count() const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return missed_;
    }

public:
    // Non-blocking
    bool start() {
        std::unique_lock<std::mutex> lock(mtx_);
        if (running_ || !callback_ || interval_.is_zero()) return false;
        if (worker_.joinable()) { lock.unlock(); worker_.join(); lock.lock(); }
        running_        = true;
        paused_         = false;
        stop_requested_ = false;
        error_          = nullptr;
        ticks_          = 0;
        missed_         = 0;
        worker_ = std::thread([this] { loop_(); });
        return true;
    }

    bool start(indexed_type cb) {
        { std::lock_guard<std::mutex> lock(mtx_); if (running_) return false; callback_ = std::move(cb); }
        return start();
    }

    bool start(callback_type cb) {
        { std::lock_guard<std::mutex> lock(mtx_); if (running_) return false; }
        set_callback(std::move(cb));
        return start();
    }

    // Blocking
    std::size_t run() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (running_ || !callback_ || interval_.is_zero()) return 0;
            running_        = true;
            paused_         = false;
            stop_requested_ = false;
            error_          = nullptr;
            ticks_          = 0;
            missed_         = 0;
        }

        loop_();
        rethrow_if_error();
        return tick_count();
    }

    std::size_t run(indexed_type cb) {
        { std::lock_guard<std::mutex> lock(mtx_); if (running_) return 0; callback_ = std::move(cb); }
        return run();
    }

    std::size_t run(callback_type cb) {
        { std::lock_guard<std::mutex> lock(mtx_); if (running_) return 0; }
        set_callback(std::move(cb));
        return run();
    }

    bool restart() {
        shutdown();
        return start();
    }

private:
    void loop_() {
        std::unique_lock<std::mutex> lock(mtx_);
        const std::chrono::nanoseconds step(static_cast<std::chrono::nanoseconds::rep>(interval_.ns()));
        const bool has_limit = !limit_.is_zero();
        steady::time_point next     = steady::now() + step;
        steady::time_point end_at   = steady::now() + limit_.chrono();
        std::uint64_t      hold_ns  = 0;

        for (;;) {
            if (stop_requested_) break;

            if (paused_) {
                const steady::time_point pause_start = steady::now();
                cv_.wait(lock, [this] { return !paused_ || stop_requested_; });
                if (stop_requested_) break;
                const auto held = steady::now() - pause_start;
                next   = steady::now() + std::chrono::nanoseconds(hold_ns);
                end_at += held;
                continue;
            }

            if (cv_.wait_until(lock, next, [this] { return stop_requested_ || paused_; })) {
                if (stop_requested_) break;
                const steady::time_point now = steady::now();
                hold_ns = (now >= next) ? 0 : static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(next - now).count());
                continue;
            }

            ++ticks_;
            const std::size_t index = ticks_;
            indexed_type cb = callback_;
            invoke_unlocked(lock, [&cb, index] { cb(index); });
            if (stop_requested_) break;
            if (max_ticks_ != 0 && ticks_ >= max_ticks_) break;
            if (has_limit && steady::now() >= end_at) break;
            next += step;
            const steady::time_point now = steady::now();
            
            if (next < now && skip_missed_ && step.count() > 0) {
                const auto behind = now - next;
                const auto skips  = behind / step + 1;
                next   += skips * step;
                missed_ += static_cast<std::size_t>(skips);
            }
        }

        running_ = false;
        cv_.notify_all();
    }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_TIME_TIMER_HPP