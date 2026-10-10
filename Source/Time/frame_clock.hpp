#ifndef FIZMO_FRAME_CLOCK_HPP
#define FIZMO_FRAME_CLOCK_HPP

#include "clock.hpp"
#include <cstddef>

namespace fizmo {
namespace time {

template<typename T = default_wide_int, typename = typename std::enable_if<detail::is_signed_integer_like_v<T>>::type>
class FrameClock {
public:
    using clock_type      = Clock<T>;
    using datetime_type   = typename clock_type::datetime_type;
    using difference_type = typename clock_type::difference_type;
    using value_type      = T;

    static constexpr std::size_t default_smoothing_window = 60;

private:
    typename clock_type::Type clock_source_;
    datetime_type             origin_;           // when start() was called
    datetime_type             prev_frame_;       // timestamp of previous tick()
    datetime_type             curr_frame_;       // timestamp of current tick()
    difference_type           delta_;            // curr - prev
    difference_type           elapsed_;          // curr - origin
    std::size_t               frame_count_;
    bool                      started_;

    T                         smoothed_ppf_;     
    std::size_t               smoothing_window_;

    static constexpr T ppd() noexcept { return planck_per_unit<T>(Unit::day); }
    static constexpr T pps() noexcept { return planck_per_unit<T>(Unit::second); }

    static T diff_to_planck(const difference_type& d) noexcept { return d.raw_days() * ppd() + d.raw_planck(); }

public:
    explicit FrameClock(
        typename clock_type::Type source = clock_type::Type::high_res,
        std::size_t smoothing = default_smoothing_window
    ) noexcept
        : clock_source_(source)
        , origin_()
        , prev_frame_()
        , curr_frame_()
        , delta_()
        , elapsed_()
        , frame_count_(0)
        , started_(false)
        , smoothed_ppf_(T(0))
        , smoothing_window_(smoothing > 0 ? smoothing : 1)
    {}

    void start() noexcept {
        datetime_type now = clock_type::now(clock_source_);
        origin_      = now;
        prev_frame_  = now;
        curr_frame_  = now;
        delta_       = difference_type();
        elapsed_     = difference_type();
        frame_count_ = 0;
        smoothed_ppf_ = T(0);
        started_     = true;
    }

    void tick() noexcept {
        if (!started_) { start(); return; }
        prev_frame_ = curr_frame_;
        curr_frame_ = clock_type::now(clock_source_);
        delta_      = clock_type::elapsed(prev_frame_, curr_frame_);
        elapsed_    = clock_type::elapsed(origin_, curr_frame_);
        ++frame_count_;
        T raw_ppf = diff_to_planck(delta_);
        if (raw_ppf <= T(0)) raw_ppf = T(1); 

        if (frame_count_ == 1) {
            smoothed_ppf_ = raw_ppf;
        } else {
            T w = T(smoothing_window_);
            smoothed_ppf_ = smoothed_ppf_ + (raw_ppf - smoothed_ppf_) / w;
        }
    }

    void reset() noexcept {
        started_     = false;
        frame_count_ = 0;
        delta_       = difference_type();
        elapsed_     = difference_type();
        smoothed_ppf_ = T(0);
    }

    const difference_type& delta_time()  const noexcept { return delta_; }
    const difference_type& elapsed()     const noexcept { return elapsed_; }
    std::size_t            frame_count() const noexcept { return frame_count_; }
    bool                   is_started()  const noexcept { return started_; }

    template<Unit Tag, typename V = T>
    Duration<Tag, V> delta_as() const noexcept {
        T total = diff_to_planck(delta_);
        T ppu   = planck_per_unit<T>(Tag);
        return Duration<Tag, V>(V(total / ppu));
    }

    fizmo_float_from_int_t<T> delta_seconds() const noexcept {
        using NT = fizmo_float_from_int_t<T>;
        T total = diff_to_planck(delta_);
        return static_cast<NT>(total) / static_cast<NT>(pps());
    }

    template<Unit Tag, typename V = T>
    Duration<Tag, V> elapsed_as() const noexcept {
        T total = diff_to_planck(elapsed_);
        T ppu   = planck_per_unit<T>(Tag);
        return Duration<Tag, V>(V(total / ppu));
    }

    fizmo_float_from_int_t<T> elapsed_seconds() const noexcept {
        using NT = fizmo_float_from_int_t<T>;
        T total = diff_to_planck(elapsed_);
        return static_cast<NT>(total) / static_cast<NT>(pps());
    }

    fizmo_float_from_int_t<T> fps() const noexcept {
        using NT = fizmo_float_from_int_t<T>;
        T dt = diff_to_planck(delta_);
        if (dt <= T(0)) return NT(0);
        return static_cast<NT>(pps()) / static_cast<NT>(dt);
    }

    fizmo_float_from_int_t<T> fps_smoothed() const noexcept {
        using NT = fizmo_float_from_int_t<T>;
        if (smoothed_ppf_ <= T(0)) return NT(0);
        return static_cast<NT>(pps()) / static_cast<NT>(smoothed_ppf_);
    }

    fizmo_float_from_int_t<T> fps_average() const noexcept {
        using NT = fizmo_float_from_int_t<T>;
        if (frame_count_ == 0) return NT(0);
        T total = diff_to_planck(elapsed_);
        if (total <= T(0)) return NT(0);
        return static_cast<NT>(frame_count_) * static_cast<NT>(pps()) / static_cast<NT>(total);
    }

    void set_smoothing_window(std::size_t n) noexcept { smoothing_window_ = n > 0 ? n : 1; }
    std::size_t smoothing_window() const noexcept { return smoothing_window_; }

    const datetime_type& origin()         const noexcept { return origin_; }
    const datetime_type& previous_frame() const noexcept { return prev_frame_; }
    const datetime_type& current_frame()  const noexcept { return curr_frame_; }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "frame " << frame_count_
            << " | dt " << delta_.to_string()
            << " | fps " << fps_smoothed();
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const FrameClock& fc) { return os << fc.to_string(); }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_FRAME_CLOCK_HPP