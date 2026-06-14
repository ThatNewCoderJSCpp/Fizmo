#ifndef TIME_POINT_UTIL_FUNCTIONS_HPP
#define TIME_POINT_UTIL_FUNCTIONS_HPP

#include "time_point.hpp"

namespace fizmo {
namespace time {

constexpr CompleteDuration calculate_timepoint_difference(const TimePoint& tp1, const TimePoint& tp2) noexcept { return tp2.to_duration() - tp1.to_duration(); }

constexpr TimePoint operator+(const TimePoint& tp, const CompleteDuration& d) noexcept { return tp + TimePoint(d); }
constexpr TimePoint operator+(const CompleteDuration& d, const TimePoint& tp) noexcept { return TimePoint(d) + tp; }
constexpr TimePoint operator+(const TimePoint& tp, DURATION_PARAM d) noexcept { return tp + TimePoint(d); }
constexpr TimePoint operator+(DURATION_PARAM d, const TimePoint& tp) noexcept { return TimePoint(d) + tp; }

constexpr TimePoint operator-(const TimePoint& tp, const CompleteDuration& d) noexcept { return tp - TimePoint(d); }
constexpr TimePoint operator-(const CompleteDuration& d, const TimePoint& tp) noexcept { return TimePoint(d) - tp; }
constexpr TimePoint operator-(const TimePoint& tp, DURATION_PARAM d) noexcept { return tp - TimePoint(d); }
constexpr TimePoint operator-(DURATION_PARAM d, const TimePoint& tp) noexcept { return TimePoint(d) - tp; }

OPTIONAL_CPP14_CONSTEXPR TimePoint& TimePoint::operator+=(const CompleteDuration& d) noexcept {
    *this = *this + d;
    return *this;
}

OPTIONAL_CPP14_CONSTEXPR TimePoint& TimePoint::operator-=(const CompleteDuration& d) noexcept {
    *this = *this + d;
    return *this;
}

OPTIONAL_CPP14_CONSTEXPR TimePoint& TimePoint::operator+=(DURATION_PARAM d) noexcept {
    *this = *this + d;
    return *this;
}

OPTIONAL_CPP14_CONSTEXPR TimePoint& TimePoint::operator-=(DURATION_PARAM d) noexcept {
    *this = *this + d;
    return *this;
}

} // namespace time
} // namespace fizmo

#endif // TIME_POINT_UTIL_FUNCTIONS_HPP