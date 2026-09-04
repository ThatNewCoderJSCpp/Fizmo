#ifndef FIZMO_TIME_CAST_OPERATORS_HPP
#define FIZMO_TIME_CAST_OPERATORS_HPP

#include "duration.hpp"  

namespace fizmo {
namespace time {

// nanosecond conversion implementations
constexpr inline nanosecond::operator microsecond() const noexcept { return conversions::nanoseconds_to_microseconds(*this); }
constexpr inline nanosecond::operator millisecond() const noexcept { return conversions::nanoseconds_to_milliseconds(*this); }
constexpr inline nanosecond::operator centisecond() const noexcept { return conversions::nanoseconds_to_centiseconds(*this); }
constexpr inline nanosecond::operator decisecond() const noexcept { return conversions::nanoseconds_to_deciseconds(*this); }
constexpr inline nanosecond::operator second() const noexcept { return conversions::nanoseconds_to_seconds(*this); }
constexpr inline nanosecond::operator minute() const noexcept { return conversions::nanoseconds_to_minutes(*this); }
constexpr inline nanosecond::operator hour() const noexcept { return conversions::nanoseconds_to_hours(*this); }
constexpr inline nanosecond::operator day() const noexcept { return conversions::nanoseconds_to_days(*this); }
constexpr inline nanosecond::operator week() const noexcept { return conversions::nanoseconds_to_weeks(*this); }
constexpr inline nanosecond::operator year() const noexcept { return conversions::nanoseconds_to_years(*this); }
constexpr inline nanosecond::operator decade() const noexcept { return conversions::nanoseconds_to_decades(*this); }
constexpr inline nanosecond::operator century() const noexcept { return conversions::nanoseconds_to_centuries(*this); }
constexpr inline nanosecond::operator millennium() const noexcept { return conversions::nanoseconds_to_millennia(*this); }

// microsecond conversion implementations
constexpr inline microsecond::operator nanosecond() const noexcept { return conversions::microseconds_to_nanoseconds(*this); }
constexpr inline microsecond::operator millisecond() const noexcept { return conversions::microseconds_to_milliseconds(*this); }
constexpr inline microsecond::operator centisecond() const noexcept { return conversions::microseconds_to_centiseconds(*this); }
constexpr inline microsecond::operator decisecond() const noexcept { return conversions::microseconds_to_deciseconds(*this); }
constexpr inline microsecond::operator second() const noexcept { return conversions::microseconds_to_seconds(*this); }
constexpr inline microsecond::operator minute() const noexcept { return conversions::microseconds_to_minutes(*this); }
constexpr inline microsecond::operator hour() const noexcept { return conversions::microseconds_to_hours(*this); }
constexpr inline microsecond::operator day() const noexcept { return conversions::microseconds_to_days(*this); }
constexpr inline microsecond::operator week() const noexcept { return conversions::microseconds_to_weeks(*this); }
constexpr inline microsecond::operator year() const noexcept { return conversions::microseconds_to_years(*this); }
constexpr inline microsecond::operator decade() const noexcept { return conversions::microseconds_to_decades(*this); }
constexpr inline microsecond::operator century() const noexcept { return conversions::microseconds_to_centuries(*this); }
constexpr inline microsecond::operator millennium() const noexcept { return conversions::microseconds_to_millennia(*this); }

// millisecond conversion implementations
constexpr inline millisecond::operator nanosecond() const noexcept { return conversions::milliseconds_to_nanoseconds(*this); }
constexpr inline millisecond::operator microsecond() const noexcept { return conversions::milliseconds_to_microseconds(*this); }
constexpr inline millisecond::operator centisecond() const noexcept { return conversions::milliseconds_to_centiseconds(*this); }
constexpr inline millisecond::operator decisecond() const noexcept { return conversions::milliseconds_to_deciseconds(*this); }
constexpr inline millisecond::operator second() const noexcept { return conversions::milliseconds_to_seconds(*this); }
constexpr inline millisecond::operator minute() const noexcept { return conversions::milliseconds_to_minutes(*this); }
constexpr inline millisecond::operator hour() const noexcept { return conversions::milliseconds_to_hours(*this); }
constexpr inline millisecond::operator day() const noexcept { return conversions::milliseconds_to_days(*this); }
constexpr inline millisecond::operator week() const noexcept { return conversions::milliseconds_to_weeks(*this); }
constexpr inline millisecond::operator year() const noexcept { return conversions::milliseconds_to_years(*this); }
constexpr inline millisecond::operator decade() const noexcept { return conversions::milliseconds_to_decades(*this); }
constexpr inline millisecond::operator century() const noexcept { return conversions::milliseconds_to_centuries(*this); }
constexpr inline millisecond::operator millennium() const noexcept { return conversions::milliseconds_to_millennia(*this); }

// centisecond conversion implementations
constexpr inline centisecond::operator nanosecond() const noexcept { return conversions::centiseconds_to_nanoseconds(*this); }
constexpr inline centisecond::operator microsecond() const noexcept { return conversions::centiseconds_to_microseconds(*this); }
constexpr inline centisecond::operator millisecond() const noexcept { return conversions::centiseconds_to_milliseconds(*this); }
constexpr inline centisecond::operator decisecond() const noexcept { return conversions::centiseconds_to_deciseconds(*this); }
constexpr inline centisecond::operator second() const noexcept { return conversions::centiseconds_to_seconds(*this); }
constexpr inline centisecond::operator minute() const noexcept { return conversions::centiseconds_to_minutes(*this); }
constexpr inline centisecond::operator hour() const noexcept { return conversions::centiseconds_to_hours(*this); }
constexpr inline centisecond::operator day() const noexcept { return conversions::centiseconds_to_days(*this); }
constexpr inline centisecond::operator week() const noexcept { return conversions::centiseconds_to_weeks(*this); }
constexpr inline centisecond::operator year() const noexcept { return conversions::centiseconds_to_years(*this); }
constexpr inline centisecond::operator decade() const noexcept { return conversions::centiseconds_to_decades(*this); }
constexpr inline centisecond::operator century() const noexcept { return conversions::centiseconds_to_centuries(*this); }
constexpr inline centisecond::operator millennium() const noexcept { return conversions::centiseconds_to_millennia(*this); }

// decisecond conversion implementations
constexpr inline decisecond::operator nanosecond() const noexcept { return conversions::deciseconds_to_nanoseconds(*this); }
constexpr inline decisecond::operator microsecond() const noexcept { return conversions::deciseconds_to_microseconds(*this); }
constexpr inline decisecond::operator millisecond() const noexcept { return conversions::deciseconds_to_milliseconds(*this); }
constexpr inline decisecond::operator centisecond() const noexcept { return conversions::deciseconds_to_centiseconds(*this); }
constexpr inline decisecond::operator second() const noexcept { return conversions::deciseconds_to_seconds(*this); }
constexpr inline decisecond::operator minute() const noexcept { return conversions::deciseconds_to_minutes(*this); }
constexpr inline decisecond::operator hour() const noexcept { return conversions::deciseconds_to_hours(*this); }
constexpr inline decisecond::operator day() const noexcept { return conversions::deciseconds_to_days(*this); }
constexpr inline decisecond::operator week() const noexcept { return conversions::deciseconds_to_weeks(*this); }
constexpr inline decisecond::operator year() const noexcept { return conversions::deciseconds_to_years(*this); }
constexpr inline decisecond::operator decade() const noexcept { return conversions::deciseconds_to_decades(*this); }
constexpr inline decisecond::operator century() const noexcept { return conversions::deciseconds_to_centuries(*this); }
constexpr inline decisecond::operator millennium() const noexcept { return conversions::deciseconds_to_millennia(*this); }

// second conversion implementations
constexpr inline second::operator nanosecond() const noexcept { return conversions::seconds_to_nanoseconds(*this); }
constexpr inline second::operator microsecond() const noexcept { return conversions::seconds_to_microseconds(*this); }
constexpr inline second::operator millisecond() const noexcept { return conversions::seconds_to_milliseconds(*this); }
constexpr inline second::operator centisecond() const noexcept { return conversions::seconds_to_centiseconds(*this); }
constexpr inline second::operator decisecond() const noexcept { return conversions::seconds_to_deciseconds(*this); }
constexpr inline second::operator minute() const noexcept { return conversions::seconds_to_minutes(*this); }
constexpr inline second::operator hour() const noexcept { return conversions::seconds_to_hours(*this); }
constexpr inline second::operator day() const noexcept { return conversions::seconds_to_days(*this); }
constexpr inline second::operator week() const noexcept { return conversions::seconds_to_weeks(*this); }
constexpr inline second::operator year() const noexcept { return conversions::seconds_to_years(*this); }
constexpr inline second::operator decade() const noexcept { return conversions::seconds_to_decades(*this); }
constexpr inline second::operator century() const noexcept { return conversions::seconds_to_centuries(*this); }
constexpr inline second::operator millennium() const noexcept { return conversions::seconds_to_millennia(*this); }

// minute conversion implementations
constexpr inline minute::operator nanosecond() const noexcept { return conversions::minutes_to_nanoseconds(*this); }
constexpr inline minute::operator microsecond() const noexcept { return conversions::minutes_to_microseconds(*this); }
constexpr inline minute::operator millisecond() const noexcept { return conversions::minutes_to_milliseconds(*this); }
constexpr inline minute::operator centisecond() const noexcept { return conversions::minutes_to_centiseconds(*this); }
constexpr inline minute::operator decisecond() const noexcept { return conversions::minutes_to_deciseconds(*this); }
constexpr inline minute::operator second() const noexcept { return conversions::minutes_to_seconds(*this); }
constexpr inline minute::operator hour() const noexcept { return conversions::minutes_to_hours(*this); }
constexpr inline minute::operator day() const noexcept { return conversions::minutes_to_days(*this); }
constexpr inline minute::operator week() const noexcept { return conversions::minutes_to_weeks(*this); }
constexpr inline minute::operator year() const noexcept { return conversions::minutes_to_years(*this); }
constexpr inline minute::operator decade() const noexcept { return conversions::minutes_to_decades(*this); }
constexpr inline minute::operator century() const noexcept { return conversions::minutes_to_centuries(*this); }
constexpr inline minute::operator millennium() const noexcept { return conversions::minutes_to_millennia(*this); }

// hour conversion implementations
constexpr inline hour::operator nanosecond() const noexcept { return conversions::hours_to_nanoseconds(*this); }
constexpr inline hour::operator microsecond() const noexcept { return conversions::hours_to_microseconds(*this); }
constexpr inline hour::operator millisecond() const noexcept { return conversions::hours_to_milliseconds(*this); }
constexpr inline hour::operator centisecond() const noexcept { return conversions::hours_to_centiseconds(*this); }
constexpr inline hour::operator decisecond() const noexcept { return conversions::hours_to_deciseconds(*this); }
constexpr inline hour::operator second() const noexcept { return conversions::hours_to_seconds(*this); }
constexpr inline hour::operator minute() const noexcept { return conversions::hours_to_minutes(*this); }
constexpr inline hour::operator day() const noexcept { return conversions::hours_to_days(*this); }
constexpr inline hour::operator week() const noexcept { return conversions::hours_to_weeks(*this); }
constexpr inline hour::operator year() const noexcept { return conversions::hours_to_years(*this); }
constexpr inline hour::operator decade() const noexcept { return conversions::hours_to_decades(*this); }
constexpr inline hour::operator century() const noexcept { return conversions::hours_to_centuries(*this); }
constexpr inline hour::operator millennium() const noexcept { return conversions::hours_to_millennia(*this); }

// day conversion implementations
constexpr inline day::operator nanosecond() const noexcept { return conversions::days_to_nanoseconds(*this); }
constexpr inline day::operator microsecond() const noexcept { return conversions::days_to_microseconds(*this); }
constexpr inline day::operator millisecond() const noexcept { return conversions::days_to_milliseconds(*this); }
constexpr inline day::operator centisecond() const noexcept { return conversions::days_to_centiseconds(*this); }
constexpr inline day::operator decisecond() const noexcept { return conversions::days_to_deciseconds(*this); }
constexpr inline day::operator second() const noexcept { return conversions::days_to_seconds(*this); }
constexpr inline day::operator minute() const noexcept { return conversions::days_to_minutes(*this); }
constexpr inline day::operator hour() const noexcept { return conversions::days_to_hours(*this); }
constexpr inline day::operator week() const noexcept { return conversions::day_to_week(*this); }
constexpr inline day::operator year() const noexcept { return conversions::days_to_years(*this); }
constexpr inline day::operator decade() const noexcept { return conversions::days_to_decades(*this); }
constexpr inline day::operator century() const noexcept { return conversions::days_to_centuries(*this); }
constexpr inline day::operator millennium() const noexcept { return conversions::days_to_millennia(*this); }

// week conversion implementations
constexpr inline week::operator nanosecond() const noexcept { return conversions::weeks_to_nanoseconds(*this); }
constexpr inline week::operator microsecond() const noexcept { return conversions::weeks_to_microseconds(*this); }
constexpr inline week::operator millisecond() const noexcept { return conversions::weeks_to_milliseconds(*this); }
constexpr inline week::operator centisecond() const noexcept { return conversions::weeks_to_centiseconds(*this); }
constexpr inline week::operator decisecond() const noexcept { return conversions::weeks_to_deciseconds(*this); }
constexpr inline week::operator second() const noexcept { return conversions::weeks_to_seconds(*this); }
constexpr inline week::operator minute() const noexcept { return conversions::weeks_to_minutes(*this); }
constexpr inline week::operator hour() const noexcept { return conversions::weeks_to_hours(*this); }
constexpr inline week::operator day() const noexcept { return conversions::weeks_to_days(*this); }
constexpr inline week::operator year() const noexcept { return conversions::weeks_to_years(*this); }
constexpr inline week::operator decade() const noexcept { return conversions::weeks_to_decades(*this); }
constexpr inline week::operator century() const noexcept { return conversions::weeks_to_centuries(*this); }
constexpr inline week::operator millennium() const noexcept { return conversions::weeks_to_millennia(*this); }

// year conversion implementations
constexpr inline year::operator nanosecond() const noexcept { return conversions::years_to_nanoseconds(*this); }
constexpr inline year::operator microsecond() const noexcept { return conversions::years_to_microseconds(*this); }
constexpr inline year::operator millisecond() const noexcept { return conversions::years_to_milliseconds(*this); }
constexpr inline year::operator centisecond() const noexcept { return conversions::years_to_centiseconds(*this); }
constexpr inline year::operator decisecond() const noexcept { return conversions::years_to_deciseconds(*this); }
constexpr inline year::operator second() const noexcept { return conversions::years_to_seconds(*this); }
constexpr inline year::operator minute() const noexcept { return conversions::years_to_minutes(*this); }
constexpr inline year::operator hour() const noexcept { return conversions::years_to_hours(*this); }
constexpr inline year::operator day() const noexcept { return conversions::years_to_days(*this); }
constexpr inline year::operator week() const noexcept { return conversions::years_to_weeks(*this); }
constexpr inline year::operator decade() const noexcept { return conversions::years_to_decades(*this); }
constexpr inline year::operator century() const noexcept { return conversions::years_to_centuries(*this); }
constexpr inline year::operator millennium() const noexcept { return conversions::years_to_millennia(*this); }

// decade conversion implementations
constexpr inline decade::operator nanosecond() const noexcept { return conversions::decades_to_nanoseconds(*this); }
constexpr inline decade::operator microsecond() const noexcept { return conversions::decades_to_microseconds(*this); }
constexpr inline decade::operator millisecond() const noexcept { return conversions::decades_to_milliseconds(*this); }
constexpr inline decade::operator centisecond() const noexcept { return conversions::decades_to_centiseconds(*this); }
constexpr inline decade::operator decisecond() const noexcept { return conversions::decades_to_deciseconds(*this); }
constexpr inline decade::operator second() const noexcept { return conversions::decades_to_seconds(*this); }
constexpr inline decade::operator minute() const noexcept { return conversions::decades_to_minutes(*this); }
constexpr inline decade::operator hour() const noexcept { return conversions::decades_to_hours(*this); }
constexpr inline decade::operator day() const noexcept { return conversions::decades_to_days(*this); }
constexpr inline decade::operator week() const noexcept { return conversions::decades_to_weeks(*this); }
constexpr inline decade::operator year() const noexcept { return conversions::decades_to_years(*this); }
constexpr inline decade::operator century() const noexcept { return conversions::decades_to_centuries(*this); }
constexpr inline decade::operator millennium() const noexcept { return conversions::decades_to_millennia(*this); }

// century conversion implementations
constexpr inline century::operator nanosecond() const noexcept { return conversions::centuries_to_nanoseconds(*this); }
constexpr inline century::operator microsecond() const noexcept { return conversions::centuries_to_microseconds(*this); }
constexpr inline century::operator millisecond() const noexcept { return conversions::centuries_to_milliseconds(*this); }
constexpr inline century::operator centisecond() const noexcept { return conversions::centuries_to_centiseconds(*this); }
constexpr inline century::operator decisecond() const noexcept { return conversions::centuries_to_deciseconds(*this); }
constexpr inline century::operator second() const noexcept { return conversions::centuries_to_seconds(*this); }
constexpr inline century::operator minute() const noexcept { return conversions::centuries_to_minutes(*this); }
constexpr inline century::operator hour() const noexcept { return conversions::centuries_to_hours(*this); }
constexpr inline century::operator day() const noexcept { return conversions::centuries_to_days(*this); }
constexpr inline century::operator week() const noexcept { return conversions::centuries_to_weeks(*this); }
constexpr inline century::operator year() const noexcept { return conversions::centuries_to_years(*this); }
constexpr inline century::operator decade() const noexcept { return conversions::centuries_to_decades(*this); }
constexpr inline century::operator millennium() const noexcept { return conversions::centuries_to_millennia(*this); }

// millennium conversion implementations
constexpr inline millennium::operator nanosecond() const noexcept { return conversions::millennia_to_nanoseconds(*this); }
constexpr inline millennium::operator microsecond() const noexcept { return conversions::millennia_to_microseconds(*this); }
constexpr inline millennium::operator millisecond() const noexcept { return conversions::millennia_to_milliseconds(*this); }
constexpr inline millennium::operator centisecond() const noexcept { return conversions::millennia_to_centiseconds(*this); }
constexpr inline millennium::operator decisecond() const noexcept { return conversions::millennia_to_deciseconds(*this); }
constexpr inline millennium::operator second() const noexcept { return conversions::millennia_to_seconds(*this); }
constexpr inline millennium::operator minute() const noexcept { return conversions::millennia_to_minutes(*this); }
constexpr inline millennium::operator hour() const noexcept { return conversions::millennia_to_hours(*this); }
constexpr inline millennium::operator day() const noexcept { return conversions::millennia_to_days(*this); }
constexpr inline millennium::operator week() const noexcept { return conversions::millennia_to_weeks(*this); }
constexpr inline millennium::operator year() const noexcept { return conversions::millennia_to_years(*this); }
constexpr inline millennium::operator decade() const noexcept { return conversions::millennia_to_decades(*this); }
constexpr inline millennium::operator century() const noexcept { return conversions::millennia_to_centuries(*this); }

} // namespace time
} // namespace fizmo

#endif // FIZMO_TIME_CAST_OPERATORS_HPP