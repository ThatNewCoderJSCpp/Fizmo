#ifndef FIZMO_STOPWATCH_HPP
#define FIZMO_STOPWATCH_HPP

#include "clock.hpp"
#include <vector>
#include <cstddef>

namespace fizmo {
namespace temp_time {

template<typename T = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<T>>::type>
class Stopwatch {
public:
    using clock_type      = Clock<T>;
    using datetime_type   = typename clock_type::datetime_type;
    using difference_type = typename clock_type::difference_type;
    using value_type      = T;

    enum class State { stopped, running };

    struct Lap {
        std::size_t     index;          // 1-based lap number
        difference_type split;          // time since last lap/start
        difference_type elapsed;        // time since first start
    };

private:
    typename clock_type::Type clock_source_;
    State                     state_;
    datetime_type             start_point_;      // when current run began
    difference_type           accumulated_;      // banked time from previous runs
    datetime_type             last_lap_point_;   // snapshot for split 
    std::vector<Lap>          laps_;

    difference_type live_segment() const noexcept {
        if (state_ != State::running) return difference_type();
        return clock_type::elapsed(start_point_, clock_type::now(clock_source_));
    }

    static difference_type add(const difference_type& a, const difference_type& b) noexcept {
        T days   = a.raw_days()   + b.raw_days();
        T planck = a.raw_planck() + b.raw_planck();
        return difference_type(days, planck);
    }

public:
    explicit Stopwatch(typename clock_type::Type source = clock_type::Type::high_res) noexcept
        : clock_source_(source)
        , state_(State::stopped)
        , start_point_()
        , accumulated_()
        , last_lap_point_()
        , laps_()
    {}

    State state()    const noexcept { return state_; }
    bool  running()  const noexcept { return state_ == State::running; }
    bool  stopped()  const noexcept { return state_ == State::stopped; }

    difference_type elapsed() const noexcept { return add(accumulated_, live_segment()); }

    template<Unit Tag, typename V = T>
    Duration<Tag, V> elapsed_as() const noexcept {
        difference_type e = elapsed();
        const T ppd = planck_per_unit<T>(Unit::day);
        const T total_planck = e.raw_days() * ppd + e.raw_planck();
        const T ppu = planck_per_unit<T>(Tag);
        return Duration<Tag, V>(V(total_planck / ppu));
    }

    void start() noexcept {
        if (state_ == State::running) return;
        datetime_type now = clock_type::now(clock_source_);
        start_point_ = now;
        if (laps_.empty()) last_lap_point_ = now;
        state_ = State::running;
    }

    void stop() noexcept {
        if (state_ != State::running) return;
        accumulated_ = add(accumulated_, live_segment());
        state_ = State::stopped;
    }

    void reset() noexcept {
        state_          = State::stopped;
        start_point_    = datetime_type();
        accumulated_    = difference_type();
        last_lap_point_ = datetime_type();
        laps_.clear();
    }

    void restart() noexcept {
        laps_.clear();
        accumulated_ = difference_type();
        datetime_type now = clock_type::now(clock_source_);
        start_point_    = now;
        last_lap_point_ = now;
        state_ = State::running;
    }

    Lap lap() noexcept {
        if (state_ != State::running) {
            Lap zero_lap{laps_.size() + 1, difference_type(), elapsed()};
            return zero_lap;
        }

        datetime_type now     = clock_type::now(clock_source_);
        difference_type split = clock_type::elapsed(last_lap_point_, now);
        difference_type total = add(accumulated_, clock_type::elapsed(start_point_, now));
        Lap l{laps_.size() + 1, split, total};
        laps_.push_back(l);
        last_lap_point_ = now;
        return l;
    }

    difference_type split() const noexcept { return elapsed(); }

    difference_type since_last_lap() const noexcept {
        if (state_ != State::running) {
            if (laps_.empty()) return accumulated_;
            T day_diff   = accumulated_.raw_days()   - laps_.back().elapsed.raw_days();
            T plank_diff = accumulated_.raw_planck() - laps_.back().elapsed.raw_planck();
            return difference_type(day_diff, plank_diff);
        }

        return clock_type::elapsed(last_lap_point_, clock_type::now(clock_source_));
    }

    const std::vector<Lap>& laps()     const noexcept { return laps_; }
    std::size_t             lap_count() const noexcept { return laps_.size(); }
    const Lap& lap_at(std::size_t i) const { return laps_.at(i); }

    const Lap& fastest_lap() const {
        const Lap* best = &laps_.front();
        for (const auto& l : laps_) { if (l.split < best->split) best = &l; }
        return *best;
    }

     const Lap& slowest_lap() const {
        const Lap* best = &laps_.front();
        for (const auto& l : laps_) { if (l.split > best->split) best = &l; }
        return *best;
    }

    std::string to_string(LabelStyle style = LabelStyle::abbrev) const { return elapsed().to_string(style); }
    std::string to_string(std::initializer_list<Unit> units, LabelStyle style = LabelStyle::abbrev) const { return elapsed().to_string(units, style); }

    template<Unit... Tags>
    std::string to_string(LabelStyle style = LabelStyle::abbrev) const { return elapsed().template to_string<Tags...>(style); }

    friend std::ostream& operator<<(std::ostream& os, const Stopwatch& sw) { return os << sw.to_string(); }
};

} // namespace temp_time
} // namespace fizmo

#endif // FIZMO_STOPWATCH_HPP