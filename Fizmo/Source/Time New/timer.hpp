#ifndef FIZMO_TIMER_HPP
#define FIZMO_TIMER_HPP

#include "clock.hpp"
#include <cstddef>
#include <type_traits>
#include <limits>
#include <initializer_list>
#include <utility>

namespace fizmo {
namespace temp_time {

namespace detail {

template<typename Func>
bool invoke_and_check(Func&& f) {
    using R = decltype(f());
    
    if (std::is_same<R, bool>::value) {
        return f();
    } else {
        f();
        return true;
    }
}

} // namespace detail

template<typename T = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<T>>::type>
class CountdownTimer {
public:
    using clock_type      = Clock<T>;
    using datetime_type   = typename clock_type::datetime_type;
    using difference_type = typename clock_type::difference_type;
    using value_type      = T;

    enum class State { idle, running, completed };

private:
    typename clock_type::Type clock_source_;
    State                     state_;
    difference_type           elapsed_;

public:
    explicit CountdownTimer(typename clock_type::Type source = clock_type::Type::high_res) noexcept
        : clock_source_(source)
        , state_(State::idle)
        , elapsed_()
    {}

    State state()     const noexcept { return state_; }
    bool  idle()      const noexcept { return state_ == State::idle; }
    bool  completed() const noexcept { return state_ == State::completed; }

    difference_type elapsed() const noexcept { return elapsed_; }

    template<Unit Tag, typename V = T>
    Duration<Tag, V> elapsed_as() const noexcept {
        const T ppd          = planck_per_unit<T>(Unit::day);
        const T total_planck = elapsed_.raw_days() * ppd + elapsed_.raw_planck();
        const T ppu          = planck_per_unit<T>(Tag);
        return Duration<Tag, V>(V(total_planck / ppu));
    }

    void reset() noexcept {
        state_   = State::idle;
        elapsed_ = difference_type();
    }

    template<typename Func, typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
    void start(Func&& callback, const Args&... durations) {
        run(std::forward<Func>(callback), detail::sum_ms(durations...), detail::sum_ns(durations...));
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    void start(Func&& callback, const U& value, Unit unit) {
        run(std::forward<Func>(callback), detail::runtime_to_ms(value, unit), detail::runtime_to_ns(value, unit));
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    void start(Func&& callback, std::initializer_list<std::pair<U, Unit>> pairs) {
        run(std::forward<Func>(callback), detail::sum_pairs_ms(pairs), detail::sum_pairs_ns(pairs));
    }

private:
    template<typename Func>
    void run(Func&& callback, std::uint64_t ms, std::uint64_t ns) {
        state_ = State::running;
        datetime_type t0 = clock_type::now(clock_source_);
        impl::do_sleep(ms, ns);
        datetime_type t1 = clock_type::now(clock_source_);
        elapsed_ = clock_type::elapsed(t0, t1);
        state_   = State::completed;
        callback();
    }
};

template<typename T = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<T>>::type>
class RepeatingTimer {
public:
    using clock_type      = Clock<T>;
    using datetime_type   = typename clock_type::datetime_type;
    using difference_type = typename clock_type::difference_type;
    using value_type      = T;

    enum class State { idle, running, completed };

    static constexpr std::size_t infinite = std::size_t(-1);

private:
    typename clock_type::Type clock_source_;
    State                     state_;
    difference_type           elapsed_;
    std::size_t               tick_count_;

public:
    explicit RepeatingTimer(typename clock_type::Type source = clock_type::Type::high_res) noexcept
        : clock_source_(source)
        , state_(State::idle)
        , elapsed_()
        , tick_count_(0)
    {}

    State       state()      const noexcept { return state_; }
    bool        idle()       const noexcept { return state_ == State::idle; }
    bool        completed()  const noexcept { return state_ == State::completed; }
    std::size_t tick_count() const noexcept { return tick_count_; }

    difference_type elapsed() const noexcept { return elapsed_; }

    template<Unit Tag, typename V = T>
    Duration<Tag, V> elapsed_as() const noexcept {
        const T ppd          = planck_per_unit<T>(Unit::day);
        const T total_planck = elapsed_.raw_days() * ppd + elapsed_.raw_planck();
        const T ppu          = planck_per_unit<T>(Tag);
        return Duration<Tag, V>(V(total_planck / ppu));
    }

    void reset() noexcept {
        state_      = State::idle;
        elapsed_    = difference_type();
        tick_count_ = 0;
    }

    template<typename Func, typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
    std::size_t start(Func&& callback, const Args&... durations) {
        return run_loop(
            std::forward<Func>(callback),
            detail::sum_ms(durations...),
            detail::sum_ns(durations...),
            infinite, 0
        );
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    std::size_t start(Func&& callback, const U& value, Unit unit) {
        return run_loop(
            std::forward<Func>(callback),
            detail::runtime_to_ms(value, unit),
            detail::runtime_to_ns(value, unit),
            infinite, 0
        );
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    std::size_t start(Func&& callback, std::initializer_list<std::pair<U, Unit>> pairs) {
        return run_loop(
            std::forward<Func>(callback),
            detail::sum_pairs_ms(pairs),
            detail::sum_pairs_ns(pairs),
            infinite, 0
        );
    }

    template<typename Func, typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
    std::size_t start(std::size_t count, Func&& callback, const Args&... durations) {
        return run_loop(
            std::forward<Func>(callback),
            detail::sum_ms(durations...),
            detail::sum_ns(durations...),
            count, 0
        );
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    std::size_t start(std::size_t count, Func&& callback, const U& value, Unit unit) {
        return run_loop(
            std::forward<Func>(callback),
            detail::runtime_to_ms(value, unit),
            detail::runtime_to_ns(value, unit),
            count, 0
        );
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    std::size_t start(std::size_t count, Func&& callback, std::initializer_list<std::pair<U, Unit>> pairs) {
        return run_loop(
            std::forward<Func>(callback),
            detail::sum_pairs_ms(pairs),
            detail::sum_pairs_ns(pairs),
            count, 0
        );
    }

    template<typename Func, Unit MaxTag, typename MaxV, typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
    std::size_t start(
        Func&& callback,
        const TimeUnit<MaxTag, MaxV>& max_duration,
        const Args&... interval
    ) {
        return run_loop(
            std::forward<Func>(callback),
            detail::sum_ms(interval...),
            detail::sum_ns(interval...),
            infinite,
            detail::to_ns(max_duration)
        );
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    std::size_t start(
        Func&& callback,
        const U& max_value, Unit max_unit,
        const U& interval_value, Unit interval_unit
    ) {
        return run_loop(
            std::forward<Func>(callback),
            detail::runtime_to_ms(interval_value, interval_unit),
            detail::runtime_to_ns(interval_value, interval_unit),
            infinite,
            detail::runtime_to_ns(max_value, max_unit)
        );
    }

    template<typename Func, typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U>>::type>
    std::size_t start(
        Func&& callback,
        std::initializer_list<std::pair<U, Unit>> max_pairs,
        std::initializer_list<std::pair<U, Unit>> interval_pairs
    ) {
        return run_loop(
            std::forward<Func>(callback),
            detail::sum_pairs_ms(interval_pairs),
            detail::sum_pairs_ns(interval_pairs),
            infinite,
            detail::sum_pairs_ns(max_pairs)
        );
    }

private:
    template<typename Func>
    std::size_t run_loop(
        Func&& callback,
        std::uint64_t interval_ms,
        std::uint64_t interval_ns,
        std::size_t   max_ticks,
        std::uint64_t max_ns
    ) {
        state_      = State::running;
        tick_count_  = 0;
        datetime_type origin = clock_type::now(clock_source_);

        for (;;) {
            if (max_ticks != infinite && tick_count_ >= max_ticks) break;

            if (max_ns != 0) {
                difference_type so_far = clock_type::elapsed(origin, clock_type::now(clock_source_));
                const T ppd = planck_per_unit<T>(Unit::day);
                const T total_planck = so_far.raw_days() * ppd + so_far.raw_planck();
                const T ppns = planck_per_unit<T>(Unit::nanosecond);
                const std::uint64_t elapsed_ns = static_cast<std::uint64_t>(total_planck / ppns);
                if (elapsed_ns >= max_ns) break;
            }

            impl::do_sleep(interval_ms, interval_ns);
            ++tick_count_;
            if (!detail::invoke_and_check(callback)) break;
        }

        datetime_type now = clock_type::now(clock_source_);
        elapsed_ = clock_type::elapsed(origin, now);
        state_   = State::completed;
        return tick_count_;
    }
};

} // namespace temp_time
} // namespace fizmo

#endif // FIZMO_TIMER_HPP