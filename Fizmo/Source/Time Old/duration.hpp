#ifndef FIZMO_DURATION_CLASS_HPP
#define FIZMO_DURATION_CLASS_HPP

#include "chrono_defines.hpp"
#include "conversions.hpp"

namespace fizmo {
namespace time {

class CompleteDuration;
class Duration;

#if defined(ARCH_ARM32)
    #define DURATION_PARAM const Duration&
#else
    #define DURATION_PARAM const Duration
#endif

class Duration {
public:
    enum class unit : std::uint8_t {
        nanosecond,
        microsecond,
        millisecond,
        centisecond,
        decisecond,
        second,
        minute,
        hour,
        day,
        week,
        year,
        decade,
        century,
        millennium
    };

private:
    std::uint64_t value_;
    unit unit_;

public:
    constexpr Duration() noexcept : value_(0), unit_(unit::second) {}
    constexpr Duration(const unit u) noexcept : value_(0), unit_(u) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr Duration(const T v, const unit u) noexcept : value_(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))), unit_(u) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr Duration(const T v) noexcept : value_(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))), unit_(unit::millisecond) {}

    constexpr explicit Duration(const nanosecond ns) noexcept : value_(ns.value()), unit_(unit::nanosecond) {}
    constexpr explicit Duration(const microsecond us) noexcept : value_(us.value()), unit_(unit::microsecond) {}
    constexpr explicit Duration(const millisecond ms) noexcept : value_(ms.value()), unit_(unit::millisecond) {}
    constexpr explicit Duration(const centisecond cs) noexcept : value_(cs.value()), unit_(unit::centisecond) {}
    constexpr explicit Duration(const decisecond ds) noexcept : value_(ds.value()), unit_(unit::decisecond) {}
    constexpr explicit Duration(const second s) noexcept : value_(s.value()), unit_(unit::second) {}
    constexpr explicit Duration(const minute m) noexcept : value_(m.value()), unit_(unit::minute) {}
    constexpr explicit Duration(const hour h) noexcept : value_(h.value()), unit_(unit::hour) {}
    constexpr explicit Duration(const day d) noexcept : value_(d.value()), unit_(unit::day) {}
    constexpr explicit Duration(const week w) noexcept : value_(w.value()), unit_(unit::week) {}
    constexpr explicit Duration(const year y) noexcept : value_(y.value()), unit_(unit::year) {}
    constexpr explicit Duration(const decade d) noexcept : value_(d.value()), unit_(unit::decade) {}
    constexpr explicit Duration(const century c) noexcept : value_(c.value()), unit_(unit::century) {}
    constexpr explicit Duration(const millennium m) noexcept : value_(m.value()), unit_(unit::millennium) {}
    constexpr Duration(const Duration& d) noexcept : value_(d.value_), unit_(d.unit_) {}
    constexpr Duration(Duration&& d) noexcept : value_(d.value_), unit_(d.unit_) { d.value_ = 0; }
    constexpr Duration(const CompleteDuration& d, unit un = unit::nanosecond) noexcept;
    OPTIONAL_CPP14_CONSTEXPR Duration& operator=(const CompleteDuration& d) noexcept;
    
    OPTIONAL_CPP14_CONSTEXPR Duration& operator=(const Duration& d) noexcept {
        if (this != &d) { 
            value_ = d.value_; 
            unit_ = d.unit_;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR Duration& operator=(Duration&& d) noexcept {
        if (this != &d) {
            value_ = d.value_;
            unit_ = d.unit_;
            d.value_ = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR Duration& operator=(const T v) noexcept {
        *this = Duration(v);
        return *this;
    }

public:
    constexpr std::uint64_t count() const noexcept { return value_; }
    constexpr std::uint64_t& count() noexcept { return value_; }
    constexpr unit get_unit() const noexcept { return unit_; }
    constexpr unit& get_unit() noexcept { return unit_; }
    
public:
    constexpr nanosecond to_nanoseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return nanosecond(value_);
            case unit::microsecond: return conversions::microseconds_to_nanoseconds(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_nanoseconds(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_nanoseconds(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_nanoseconds(decisecond(value_));
            case unit::second: return conversions::seconds_to_nanoseconds(second(value_));
            case unit::minute: return conversions::minutes_to_nanoseconds(minute(value_));
            case unit::hour: return conversions::hours_to_nanoseconds(hour(value_));
            case unit::day: return conversions::days_to_nanoseconds(day(value_));
            case unit::week: return conversions::weeks_to_nanoseconds(week(value_));
            case unit::year: return conversions::years_to_nanoseconds(year(value_));
            case unit::decade: return conversions::decades_to_nanoseconds(decade(value_));
            case unit::century: return conversions::centuries_to_nanoseconds(century(value_));
            case unit::millennium: return conversions::millennia_to_nanoseconds(millennium(value_));
            default: return nanosecond(0);
        }
    }
    
    constexpr microsecond to_microseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_microseconds(nanosecond(value_));
            case unit::microsecond: return microsecond(value_);
            case unit::millisecond: return conversions::milliseconds_to_microseconds(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_microseconds(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_microseconds(decisecond(value_));
            case unit::second: return conversions::seconds_to_microseconds(second(value_));
            case unit::minute: return conversions::minutes_to_microseconds(minute(value_));
            case unit::hour: return conversions::hours_to_microseconds(hour(value_));
            case unit::day: return conversions::days_to_microseconds(day(value_));
            case unit::week: return conversions::weeks_to_microseconds(week(value_));
            case unit::year: return conversions::years_to_microseconds(year(value_));
            case unit::decade: return conversions::decades_to_microseconds(decade(value_));
            case unit::century: return conversions::centuries_to_microseconds(century(value_));
            case unit::millennium: return conversions::millennia_to_microseconds(millennium(value_));
            default: return microsecond(0);
        }
    }
    
    constexpr millisecond to_milliseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_milliseconds(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_milliseconds(microsecond(value_));
            case unit::millisecond: return millisecond(value_);
            case unit::centisecond: return conversions::centiseconds_to_milliseconds(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_milliseconds(decisecond(value_));
            case unit::second: return conversions::seconds_to_milliseconds(second(value_));
            case unit::minute: return conversions::minutes_to_milliseconds(minute(value_));
            case unit::hour: return conversions::hours_to_milliseconds(hour(value_));
            case unit::day: return conversions::days_to_milliseconds(day(value_));
            case unit::week: return conversions::weeks_to_milliseconds(week(value_));
            case unit::year: return conversions::years_to_milliseconds(year(value_));
            case unit::decade: return conversions::decades_to_milliseconds(decade(value_));
            case unit::century: return conversions::centuries_to_milliseconds(century(value_));
            case unit::millennium: return conversions::millennia_to_milliseconds(millennium(value_));
            default: return millisecond(0);
        }
    }
    
    constexpr centisecond to_centiseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_centiseconds(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_centiseconds(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_centiseconds(millisecond(value_));
            case unit::centisecond: return centisecond(value_);
            case unit::decisecond: return conversions::deciseconds_to_centiseconds(decisecond(value_));
            case unit::second: return conversions::seconds_to_centiseconds(second(value_));
            case unit::minute: return conversions::minutes_to_centiseconds(minute(value_));
            case unit::hour: return conversions::hours_to_centiseconds(hour(value_));
            case unit::day: return conversions::days_to_centiseconds(day(value_));
            case unit::week: return conversions::weeks_to_centiseconds(week(value_));
            case unit::year: return conversions::years_to_centiseconds(year(value_));
            case unit::decade: return conversions::decades_to_centiseconds(decade(value_));
            case unit::century: return conversions::centuries_to_centiseconds(century(value_));
            case unit::millennium: return conversions::millennia_to_centiseconds(millennium(value_));
            default: return centisecond(0);
        }
    }

    constexpr decisecond to_deciseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_deciseconds(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_deciseconds(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_deciseconds(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_deciseconds(centisecond(value_));
            case unit::decisecond: return decisecond(value_);
            case unit::second: return conversions::seconds_to_deciseconds(second(value_));
            case unit::minute: return conversions::minutes_to_deciseconds(minute(value_));
            case unit::hour: return conversions::hours_to_deciseconds(hour(value_));
            case unit::day: return conversions::days_to_deciseconds(day(value_));
            case unit::week: return conversions::weeks_to_deciseconds(week(value_));
            case unit::year: return conversions::years_to_deciseconds(year(value_));
            case unit::decade: return conversions::decades_to_deciseconds(decade(value_));
            case unit::century: return conversions::centuries_to_deciseconds(century(value_));
            case unit::millennium: return conversions::millennia_to_deciseconds(millennium(value_));
            default: return decisecond(0);
        }
    }
    
    constexpr second to_seconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_seconds(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_seconds(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_seconds(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_seconds(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_seconds(decisecond(value_));
            case unit::second: return second(value_);
            case unit::minute: return conversions::minutes_to_seconds(minute(value_));
            case unit::hour: return conversions::hours_to_seconds(hour(value_));
            case unit::day: return conversions::days_to_seconds(day(value_));
            case unit::week: return conversions::weeks_to_seconds(week(value_));
            case unit::year: return conversions::years_to_seconds(year(value_));
            case unit::decade: return conversions::decades_to_seconds(decade(value_));
            case unit::century: return conversions::centuries_to_seconds(century(value_));
            case unit::millennium: return conversions::millennia_to_seconds(millennium(value_));
            default: return second(0);
        }
    }
    
    constexpr minute to_minutes() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_minutes(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_minutes(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_minutes(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_minutes(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_minutes(decisecond(value_));
            case unit::second: return conversions::seconds_to_minutes(second(value_));
            case unit::minute: return minute(value_);
            case unit::hour: return conversions::hours_to_minutes(hour(value_));
            case unit::day: return conversions::days_to_minutes(day(value_));
            case unit::week: return conversions::weeks_to_minutes(week(value_));
            case unit::year: return conversions::years_to_minutes(year(value_));
            case unit::decade: return conversions::decades_to_minutes(decade(value_));
            case unit::century: return conversions::centuries_to_minutes(century(value_));
            case unit::millennium: return conversions::millennia_to_minutes(millennium(value_));
            default: return minute(0);
        }
    }
    
    constexpr hour to_hours() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_hours(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_hours(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_hours(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_hours(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_hours(decisecond(value_));
            case unit::second: return conversions::seconds_to_hours(second(value_));
            case unit::minute: return conversions::minutes_to_hours(minute(value_));
            case unit::hour: return hour(value_);
            case unit::day: return conversions::days_to_hours(day(value_));
            case unit::week: return conversions::weeks_to_hours(week(value_));
            case unit::year: return conversions::years_to_hours(year(value_));
            case unit::decade: return conversions::decades_to_hours(decade(value_));
            case unit::century: return conversions::centuries_to_hours(century(value_));
            case unit::millennium: return conversions::millennia_to_hours(millennium(value_));
            default: return hour(0);
        }
    }
    
    constexpr day to_days() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_days(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_days(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_days(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_days(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_days(decisecond(value_));
            case unit::second: return conversions::seconds_to_days(second(value_));
            case unit::minute: return conversions::minutes_to_days(minute(value_));
            case unit::hour: return conversions::hours_to_days(hour(value_));
            case unit::day: return day(value_);
            case unit::week: return conversions::weeks_to_days(week(value_));
            case unit::year: return conversions::years_to_days(year(value_));
            case unit::decade: return conversions::decades_to_days(decade(value_));
            case unit::century: return conversions::centuries_to_days(century(value_));
            case unit::millennium: return conversions::millennia_to_days(millennium(value_));
            default: return day(0);
        }
    }
    
    constexpr week to_weeks() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_weeks(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_weeks(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_weeks(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_weeks(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_weeks(decisecond(value_));
            case unit::second: return conversions::seconds_to_weeks(second(value_));
            case unit::minute: return conversions::minutes_to_weeks(minute(value_));
            case unit::hour: return conversions::hours_to_weeks(hour(value_));
            case unit::day: return conversions::day_to_week(day(value_));
            case unit::week: return week(value_);
            case unit::year: return conversions::years_to_weeks(year(value_));
            case unit::decade: return conversions::decades_to_weeks(decade(value_));
            case unit::century: return conversions::centuries_to_weeks(century(value_));
            case unit::millennium: return conversions::millennia_to_weeks(millennium(value_));
            default: return week(0);
        }
    }
    
    constexpr year to_years() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_years(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_years(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_years(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_years(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_years(decisecond(value_));
            case unit::second: return conversions::seconds_to_years(second(value_));
            case unit::minute: return conversions::minutes_to_years(minute(value_));
            case unit::hour: return conversions::hours_to_years(hour(value_));
            case unit::day: return conversions::days_to_years(day(value_));
            case unit::week: return conversions::weeks_to_years(week(value_));
            case unit::year: return year(value_);
            case unit::decade: return conversions::decades_to_years(decade(value_));
            case unit::century: return conversions::centuries_to_years(century(value_));
            case unit::millennium: return conversions::millennia_to_years(millennium(value_));
            default: return year(0);
        }
    }
    
    constexpr decade to_decades() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_decades(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_decades(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_decades(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_decades(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_decades(decisecond(value_));
            case unit::second: return conversions::seconds_to_decades(second(value_));
            case unit::minute: return conversions::minutes_to_decades(minute(value_));
            case unit::hour: return conversions::hours_to_decades(hour(value_));
            case unit::day: return conversions::days_to_decades(day(value_));
            case unit::week: return conversions::weeks_to_decades(week(value_));
            case unit::year: return conversions::years_to_decades(year(value_));
            case unit::decade: return decade(value_);
            case unit::century: return conversions::centuries_to_decades(century(value_));
            case unit::millennium: return conversions::millennia_to_decades(millennium(value_));
            default: return decade(0);
        }
    }
    
    constexpr century to_centuries() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_centuries(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_centuries(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_centuries(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_centuries(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_centuries(decisecond(value_));
            case unit::second: return conversions::seconds_to_centuries(second(value_));
            case unit::minute: return conversions::minutes_to_centuries(minute(value_));
            case unit::hour: return conversions::hours_to_centuries(hour(value_));
            case unit::day: return conversions::days_to_centuries(day(value_));
            case unit::week: return conversions::weeks_to_centuries(week(value_));
            case unit::year: return conversions::years_to_centuries(year(value_));
            case unit::decade: return conversions::decades_to_centuries(decade(value_));
            case unit::century: return century(value_);
            case unit::millennium: return conversions::millennia_to_centuries(millennium(value_));
            default: return century(0);
        }
    }
    
    constexpr millennium to_millennia() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return conversions::nanoseconds_to_millennia(nanosecond(value_));
            case unit::microsecond: return conversions::microseconds_to_millennia(microsecond(value_));
            case unit::millisecond: return conversions::milliseconds_to_millennia(millisecond(value_));
            case unit::centisecond: return conversions::centiseconds_to_millennia(centisecond(value_));
            case unit::decisecond: return conversions::deciseconds_to_millennia(decisecond(value_));
            case unit::second: return conversions::seconds_to_millennia(second(value_));
            case unit::minute: return conversions::minutes_to_millennia(minute(value_));
            case unit::hour: return conversions::hours_to_millennia(hour(value_));
            case unit::day: return conversions::days_to_millennia(day(value_));
            case unit::week: return conversions::weeks_to_millennia(week(value_));
            case unit::year: return conversions::years_to_millennia(year(value_));
            case unit::decade: return conversions::decades_to_millennia(decade(value_));
            case unit::century: return conversions::centuries_to_millennia(century(value_));
            case unit::millennium: return millennium(value_);
            default: return millennium(0);
        }
    }

public:
    constexpr double to_exact_nanoseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return static_cast<double>(value_);
            case unit::microsecond: return exact_conversions::microseconds_to_nanoseconds(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_nanoseconds(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_nanoseconds(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_nanoseconds(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_nanoseconds(second(value_));
            case unit::minute: return exact_conversions::minutes_to_nanoseconds(minute(value_));
            case unit::hour: return exact_conversions::hours_to_nanoseconds(hour(value_));
            case unit::day: return exact_conversions::days_to_nanoseconds(day(value_));
            case unit::week: return exact_conversions::weeks_to_nanoseconds(week(value_));
            case unit::year: return exact_conversions::years_to_nanoseconds(year(value_));
            case unit::decade: return exact_conversions::decades_to_nanoseconds(decade(value_));
            case unit::century: return exact_conversions::centuries_to_nanoseconds(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_nanoseconds(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_microseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_microseconds(nanosecond(value_));
            case unit::microsecond: return static_cast<double>(value_);
            case unit::millisecond: return exact_conversions::milliseconds_to_microseconds(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_microseconds(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_microseconds(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_microseconds(second(value_));
            case unit::minute: return exact_conversions::minutes_to_microseconds(minute(value_));
            case unit::hour: return exact_conversions::hours_to_microseconds(hour(value_));
            case unit::day: return exact_conversions::days_to_microseconds(day(value_));
            case unit::week: return exact_conversions::weeks_to_microseconds(week(value_));
            case unit::year: return exact_conversions::years_to_microseconds(year(value_));
            case unit::decade: return exact_conversions::decades_to_microseconds(decade(value_));
            case unit::century: return exact_conversions::centuries_to_microseconds(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_microseconds(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_milliseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_milliseconds(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_milliseconds(microsecond(value_));
            case unit::millisecond: return static_cast<double>(value_);
            case unit::centisecond: return exact_conversions::centiseconds_to_milliseconds(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_milliseconds(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_milliseconds(second(value_));
            case unit::minute: return exact_conversions::minutes_to_milliseconds(minute(value_));
            case unit::hour: return exact_conversions::hours_to_milliseconds(hour(value_));
            case unit::day: return exact_conversions::days_to_milliseconds(day(value_));
            case unit::week: return exact_conversions::weeks_to_milliseconds(week(value_));
            case unit::year: return exact_conversions::years_to_milliseconds(year(value_));
            case unit::decade: return exact_conversions::decades_to_milliseconds(decade(value_));
            case unit::century: return exact_conversions::centuries_to_milliseconds(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_milliseconds(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_centiseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_centiseconds(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_centiseconds(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_centiseconds(millisecond(value_));
            case unit::centisecond: return static_cast<double>(value_);
            case unit::decisecond: return exact_conversions::deciseconds_to_centiseconds(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_centiseconds(second(value_));
            case unit::minute: return exact_conversions::minutes_to_centiseconds(minute(value_));
            case unit::hour: return exact_conversions::hours_to_centiseconds(hour(value_));
            case unit::day: return exact_conversions::days_to_centiseconds(day(value_));
            case unit::week: return exact_conversions::weeks_to_centiseconds(week(value_));
            case unit::year: return exact_conversions::years_to_centiseconds(year(value_));
            case unit::decade: return exact_conversions::decades_to_centiseconds(decade(value_));
            case unit::century: return exact_conversions::centuries_to_centiseconds(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_centiseconds(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_deciseconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_deciseconds(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_deciseconds(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_deciseconds(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_deciseconds(centisecond(value_));
            case unit::decisecond: return static_cast<double>(value_);
            case unit::second: return exact_conversions::seconds_to_deciseconds(second(value_));
            case unit::minute: return exact_conversions::minutes_to_deciseconds(minute(value_));
            case unit::hour: return exact_conversions::hours_to_deciseconds(hour(value_));
            case unit::day: return exact_conversions::days_to_deciseconds(day(value_));
            case unit::week: return exact_conversions::weeks_to_deciseconds(week(value_));
            case unit::year: return exact_conversions::years_to_deciseconds(year(value_));
            case unit::decade: return exact_conversions::decades_to_deciseconds(decade(value_));
            case unit::century: return exact_conversions::centuries_to_deciseconds(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_deciseconds(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_seconds() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_seconds(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_seconds(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_seconds(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_seconds(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_seconds(decisecond(value_));
            case unit::second: return static_cast<double>(value_);
            case unit::minute: return exact_conversions::minutes_to_seconds(minute(value_));
            case unit::hour: return exact_conversions::hours_to_seconds(hour(value_));
            case unit::day: return exact_conversions::days_to_seconds(day(value_));
            case unit::week: return exact_conversions::weeks_to_seconds(week(value_));
            case unit::year: return exact_conversions::years_to_seconds(year(value_));
            case unit::decade: return exact_conversions::decades_to_seconds(decade(value_));
            case unit::century: return exact_conversions::centuries_to_seconds(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_seconds(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_minutes() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_minutes(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_minutes(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_minutes(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_minutes(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_minutes(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_minutes(second(value_));
            case unit::minute: return static_cast<double>(value_);
            case unit::hour: return exact_conversions::hours_to_minutes(hour(value_));
            case unit::day: return exact_conversions::days_to_minutes(day(value_));
            case unit::week: return exact_conversions::weeks_to_minutes(week(value_));
            case unit::year: return exact_conversions::years_to_minutes(year(value_));
            case unit::decade: return exact_conversions::decades_to_minutes(decade(value_));
            case unit::century: return exact_conversions::centuries_to_minutes(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_minutes(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_hours() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_hours(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_hours(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_hours(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_hours(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_hours(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_hours(second(value_));
            case unit::minute: return exact_conversions::minutes_to_hours(minute(value_));
            case unit::hour: return static_cast<double>(value_);
            case unit::day: return exact_conversions::days_to_hours(day(value_));
            case unit::week: return exact_conversions::weeks_to_hours(week(value_));
            case unit::year: return exact_conversions::years_to_hours(year(value_));
            case unit::decade: return exact_conversions::decades_to_hours(decade(value_));
            case unit::century: return exact_conversions::centuries_to_hours(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_hours(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_days() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_days(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_days(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_days(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_days(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_days(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_days(second(value_));
            case unit::minute: return exact_conversions::minutes_to_days(minute(value_));
            case unit::hour: return exact_conversions::hours_to_days(hour(value_));
            case unit::day: return static_cast<double>(value_);
            case unit::week: return exact_conversions::weeks_to_days(week(value_));
            case unit::year: return exact_conversions::years_to_days(year(value_));
            case unit::decade: return exact_conversions::decades_to_days(decade(value_));
            case unit::century: return exact_conversions::centuries_to_days(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_days(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_weeks() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_weeks(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_weeks(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_weeks(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_weeks(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_weeks(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_weeks(second(value_));
            case unit::minute: return exact_conversions::minutes_to_weeks(minute(value_));
            case unit::hour: return exact_conversions::hours_to_weeks(hour(value_));
            case unit::day: return exact_conversions::days_to_weeks(day(value_));
            case unit::week: return static_cast<double>(value_);
            case unit::year: return exact_conversions::years_to_weeks(year(value_));
            case unit::decade: return exact_conversions::decades_to_weeks(decade(value_));
            case unit::century: return exact_conversions::centuries_to_weeks(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_weeks(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_years() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_years(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_years(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_years(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_years(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_years(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_years(second(value_));
            case unit::minute: return exact_conversions::minutes_to_years(minute(value_));
            case unit::hour: return exact_conversions::hours_to_years(hour(value_));
            case unit::day: return exact_conversions::days_to_years(day(value_));
            case unit::week: return exact_conversions::weeks_to_years(week(value_));
            case unit::year: return static_cast<double>(value_);
            case unit::decade: return exact_conversions::decades_to_years(decade(value_));
            case unit::century: return exact_conversions::centuries_to_years(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_years(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_decades() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_decades(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_decades(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_decades(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_decades(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_decades(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_decades(second(value_));
            case unit::minute: return exact_conversions::minutes_to_decades(minute(value_));
            case unit::hour: return exact_conversions::hours_to_decades(hour(value_));
            case unit::day: return exact_conversions::days_to_decades(day(value_));
            case unit::week: return exact_conversions::weeks_to_decades(week(value_));
            case unit::year: return exact_conversions::years_to_decades(year(value_));
            case unit::decade: return static_cast<double>(value_);
            case unit::century: return exact_conversions::centuries_to_decades(century(value_));
            case unit::millennium: return exact_conversions::millennia_to_decades(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_centuries() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_centuries(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_centuries(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_centuries(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_centuries(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_centuries(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_centuries(second(value_));
            case unit::minute: return exact_conversions::minutes_to_centuries(minute(value_));
            case unit::hour: return exact_conversions::hours_to_centuries(hour(value_));
            case unit::day: return exact_conversions::days_to_centuries(day(value_));
            case unit::week: return exact_conversions::weeks_to_centuries(week(value_));
            case unit::year: return exact_conversions::years_to_centuries(year(value_));
            case unit::decade: return exact_conversions::decades_to_centuries(decade(value_));
            case unit::century: return static_cast<double>(value_);
            case unit::millennium: return exact_conversions::millennia_to_centuries(millennium(value_));
            default: return 0.0;
        }
    }
    
    constexpr double to_exact_millennia() const noexcept {
        switch (unit_) {
            case unit::nanosecond: return exact_conversions::nanoseconds_to_millennia(nanosecond(value_));
            case unit::microsecond: return exact_conversions::microseconds_to_millennia(microsecond(value_));
            case unit::millisecond: return exact_conversions::milliseconds_to_millennia(millisecond(value_));
            case unit::centisecond: return exact_conversions::centiseconds_to_millennia(centisecond(value_));
            case unit::decisecond: return exact_conversions::deciseconds_to_millennia(decisecond(value_));
            case unit::second: return exact_conversions::seconds_to_millennia(second(value_));
            case unit::minute: return exact_conversions::minutes_to_millennia(minute(value_));
            case unit::hour: return exact_conversions::hours_to_millennia(hour(value_));
            case unit::day: return exact_conversions::days_to_millennia(day(value_));
            case unit::week: return exact_conversions::weeks_to_millennia(week(value_));
            case unit::year: return exact_conversions::years_to_millennia(year(value_));
            case unit::decade: return exact_conversions::decades_to_millennia(decade(value_));
            case unit::century: return exact_conversions::centuries_to_millennia(century(value_));
            case unit::millennium: return static_cast<double>(value_);
            default: return 0.0;
        }
    }
    
public:
    OPTIONAL_CPP14_CONSTEXPR Duration& convert_to(const unit u) noexcept {
        if (u == unit_) return *this;
        
        switch (u) {
            case unit::nanosecond: {
                nanosecond ns = to_nanoseconds();
                value_ = ns.value();
                unit_ = unit::nanosecond;
                break;
            }
            case unit::microsecond: {
                microsecond us = to_microseconds();
                value_ = us.value();
                unit_ = unit::microsecond;
                break;
            }
            case unit::millisecond: {
                millisecond ms = to_milliseconds();
                value_ = ms.value();
                unit_ = unit::millisecond;
                break;
            }
            case unit::centisecond: {
                centisecond cs = to_centiseconds();
                value_ = cs.value();
                unit_ = unit::centisecond;
                break;
            }
            case unit::decisecond: {
                decisecond ds = to_deciseconds();
                value_ = ds.value();
                unit_ = unit::decisecond;
                break;
            }
            case unit::second: {
                second s = to_seconds();
                value_ = s.value();
                unit_ = unit::second;
                break;
            }
            case unit::minute: {
                minute m = to_minutes();
                value_ = m.value();
                unit_ = unit::minute;
                break;
            }
            case unit::hour: {
                hour h = to_hours();
                value_ = h.value();
                unit_ = unit::hour;
                break;
            }
            case unit::day: {
                day d = to_days();
                value_ = d.value();
                unit_ = unit::day;
                break;
            }
            case unit::week: {
                week w = to_weeks();
                value_ = w.value();
                unit_ = unit::week;
                break;
            }
            case unit::year: {
                year y = to_years();
                value_ = y.value();
                unit_ = unit::year;
                break;
            }
            case unit::decade: {
                decade d = to_decades();
                value_ = d.value();
                unit_ = unit::decade;
                break;
            }
            case unit::century: {
                century c = to_centuries();
                value_ = c.value();
                unit_ = unit::century;
                break;
            }
            case unit::millennium: {
                millennium m = to_millennia();
                value_ = m.value();
                unit_ = unit::millennium;
                break;
            }
            default:
                break;
        }
        
        return *this;
    }
    
    constexpr Duration as(const unit u) const noexcept {
        Duration result(*this);
        result.convert_to(u);
        return result;
    }

    constexpr CompleteDuration to_complete_duration() const noexcept;

public:
    constexpr bool operator==(DURATION_PARAM d) const noexcept {
        if (unit_ == d.unit_) return value_ == d.value_;
        return to_exact_millennia() == d.to_exact_millennia();
    }
    
    constexpr bool operator!=(DURATION_PARAM d) const noexcept { return !(*this == d); }
    
    constexpr bool operator<(DURATION_PARAM d) const noexcept {
        if (unit_ == d.unit_) return value_ < d.value_;
        return to_exact_millennia() < d.to_exact_millennia();
    }
    
    constexpr bool operator<=(DURATION_PARAM d) const noexcept { return (*this < d) || (*this == d); }
    constexpr bool operator>(DURATION_PARAM d) const noexcept { return !(*this <= d); }
    constexpr bool operator>=(DURATION_PARAM d) const noexcept { return !(*this < d); }

public:
    constexpr Duration operator+(DURATION_PARAM d) const noexcept {
        if (unit_ == d.unit_) {
            std::uint64_t result_value = 0;

            if (d.value_ > 0 && value_ > std::numeric_limits<std::uint64_t>::max() - d.value_) {
                result_value = std::numeric_limits<std::uint64_t>::max();
            } else {
                result_value = value_ + d.value_;
            }

            return Duration(result_value, unit_);
        }
        
        nanosecond result = to_nanoseconds() + d.to_nanoseconds();
        return Duration(result).convert_to(unit_);
    }
    
    OPTIONAL_CPP14_CONSTEXPR Duration& operator+=(DURATION_PARAM d) noexcept {
        *this = *this + d;
        return *this;
    }
    
    constexpr Duration operator-(DURATION_PARAM d) const noexcept {
        if (unit_ == d.unit_) {
            return Duration(value_ >= d.value_ ? value_ - d.value_ : 0, unit_);
        }
        
        nanosecond this_ns = to_nanoseconds();
        nanosecond d_ns = d.to_nanoseconds();
        nanosecond result(this_ns.value() >= d_ns.value() ? this_ns.value() - d_ns.value() : 0);
        return Duration(result).convert_to(unit_);
    }
    
    OPTIONAL_CPP14_CONSTEXPR Duration& operator-=(DURATION_PARAM d) noexcept {
        *this = *this - d;
        return *this;
    }

    constexpr CompleteDuration operator+(const CompleteDuration& cd) const noexcept;
    constexpr CompleteDuration operator-(const CompleteDuration& cd) const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr Duration operator*(const T scalar) const noexcept {
        if (scalar <= 0) return Duration(0, unit_);
        double scalar_abs = fizmo::abs_constexpr(static_cast<double>(scalar));

        if (scalar_abs >= static_cast<double>(std::numeric_limits<std::uint64_t>::max()) / static_cast<double>(value_)) {
            return Duration(std::numeric_limits<std::uint64_t>::max(), unit_);
        }
        
        return Duration(static_cast<std::uint64_t>(static_cast<double>(value_) * scalar_abs), unit_);
    }
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Duration& operator*=(const T scalar) noexcept {
        *this = *this * scalar;
        return *this;
    }
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr Duration operator/(const T scalar) const noexcept {
        if (scalar == 0) return Duration(0, unit_); 
        typename std::common_type<double, T>::type scalar_abs = fizmo::abs_constexpr(static_cast<typename std::common_type<double, T>::type>(scalar));
        return Duration(static_cast<std::uint64_t>(static_cast<typename std::common_type<double, T>::typee>(value_) / scalar_abs), unit_);
    }
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Duration& operator/=(const T scalar) noexcept {
        *this = *this / scalar;
        return *this;
    }
    
    constexpr double operator/(const Duration& d) const noexcept {
        if (d.value_ == 0) return 0.0; 
        if (unit_ == d.unit_) return static_cast<double>(value_) / static_cast<double>(d.value_);
        return to_exact_nanoseconds() / d.to_exact_nanoseconds();
    }

public:
    constexpr explicit operator nanosecond() const noexcept { return to_nanoseconds(); }
    constexpr explicit operator microsecond() const noexcept { return to_microseconds(); }
    constexpr explicit operator millisecond() const noexcept { return to_milliseconds(); }
    constexpr explicit operator centisecond() const noexcept { return to_centiseconds(); }
    constexpr explicit operator decisecond() const noexcept { return to_deciseconds(); }
    constexpr explicit operator second() const noexcept { return to_seconds(); }
    constexpr explicit operator minute() const noexcept { return to_minutes(); }
    constexpr explicit operator hour() const noexcept { return to_hours(); }
    constexpr explicit operator day() const noexcept { return to_days(); }
    constexpr explicit operator week() const noexcept { return to_weeks(); }
    constexpr explicit operator year() const noexcept { return to_years(); }
    constexpr explicit operator decade() const noexcept { return to_decades(); }
    constexpr explicit operator century() const noexcept { return to_centuries(); }
    constexpr explicit operator millennium() const noexcept { return to_millennia(); }
    constexpr explicit operator double() const noexcept { return static_cast<double>(value_); }
    constexpr explicit operator CompleteDuration() const noexcept;
};

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr Duration operator*(const T scalar, DURATION_PARAM d) noexcept { return d * scalar; }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr Duration operator/(const T scalar, DURATION_PARAM d) noexcept { return Duration(scalar / d.count()); }

constexpr Duration nanoseconds(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::nanosecond); }
constexpr Duration microseconds(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::microsecond); }
constexpr Duration milliseconds(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::millisecond); }
constexpr Duration centiseconds(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::centisecond); }
constexpr Duration deciseconds(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::decisecond); }
constexpr Duration seconds(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::second); }
constexpr Duration minutes(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::minute); }
constexpr Duration hours(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::hour); }
constexpr Duration days(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::day); }
constexpr Duration weeks(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::week); }
constexpr Duration years(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::year); }
constexpr Duration decades(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::decade); }
constexpr Duration centuries(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::century); }
constexpr Duration millennia(const std::uint64_t value) noexcept { return Duration(value, Duration::unit::millennium); }

template<Duration::unit U> struct fizmo_time_type;
template<> struct fizmo_time_type<Duration::unit::nanosecond> { typedef nanosecond type; };
template<> struct fizmo_time_type<Duration::unit::microsecond> { typedef microsecond type; };
template<> struct fizmo_time_type<Duration::unit::millisecond> { typedef millisecond type; };
template<> struct fizmo_time_type<Duration::unit::centisecond> { typedef centisecond type; };
template<> struct fizmo_time_type<Duration::unit::decisecond> { typedef decisecond type; };
template<> struct fizmo_time_type<Duration::unit::second> { typedef second type; };
template<> struct fizmo_time_type<Duration::unit::minute> { typedef minute type; };
template<> struct fizmo_time_type<Duration::unit::hour> { typedef hour type; };
template<> struct fizmo_time_type<Duration::unit::day> { typedef day type; };
template<> struct fizmo_time_type<Duration::unit::week> { typedef week type; };
template<> struct fizmo_time_type<Duration::unit::year> { typedef year type; };
template<> struct fizmo_time_type<Duration::unit::decade> { typedef decade type; };
template<> struct fizmo_time_type<Duration::unit::century> { typedef century type; };
template<> struct fizmo_time_type<Duration::unit::millennium> { typedef millennium type; };

template<Duration::unit U>
using fizmo_time_type_t = typename fizmo_time_type<U>::type;

} // namespace time
} // namespace fizmo

#endif // FIZMO_DURATION_CLASS_HPP