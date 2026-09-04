#ifndef DATE_TIME_UTIL_FUNCTIONS_HPP
#define DATE_TIME_UTIL_FUNCTIONS_HPP

#include "date_time.hpp"

namespace fizmo {
namespace time {

inline constexpr std::int64_t days_since_epoch(const Date& date) noexcept {
    std::int64_t total_days = 0;
    std::uint64_t target_year = date.get_year().value();
    bool is_ad = date.is_ad_year();
    
    if (is_ad) {
        for (std::uint64_t y = 1; y < target_year; ++y) {
            total_days += Calendar::is_leap_year(year(y)) ? 366 : 365;
        }
    } else {
        for (std::uint64_t y = 1; y <= target_year; ++y) {
            total_days -= Calendar::is_leap_year(year(y)) ? 366 : 365;
        }
    }
    
    const month target_month = date.get_month();

    for (std::uint8_t m = 0; m < target_month.to_int(); ++m) {
        total_days += Calendar::days_in_month(month(static_cast<months>(m)), date.get_year());
    }
    
    total_days += date.get_day().value() - 1;
    return total_days;
}

inline constexpr std::int64_t time_to_utc_milliseconds(const Time& time) noexcept {
    const std::int64_t ms = time.hours().value() * 3600000LL +
                      time.minutes().value() * 60000LL +
                      time.seconds().value() * 1000LL +
                      time.millseconds().value();
    
    const std::int64_t utc_offset_ms = time.utc_offset_hours() * 3600000LL + time.utc_offset_minutes() * 60000LL;
    return ms - utc_offset_ms;
}

/*
DIFFERENCES
*/

inline constexpr DateTimeDifference calculate_datetime_difference(const DateTime& dt1, const DateTime& dt2) noexcept {
    std::int64_t days1 = days_since_epoch(dt1.get_date());
    std::int64_t days2 = days_since_epoch(dt2.get_date());
    std::int64_t total_day_diff = days2 - days1;
    std::int64_t ms1 = time_to_utc_milliseconds(dt1.get_time());
    std::int64_t ms2 = time_to_utc_milliseconds(dt2.get_time());
    std::int64_t ms_diff = ms2 - ms1;
    
    if (ms_diff < 0) {
        total_day_diff -= 1;
        ms_diff += 86400000LL; 
    } else if (ms_diff >= 86400000LL) {
        total_day_diff += ms_diff / 86400000LL;
        ms_diff %= 86400000LL;
    }
    
    std::int64_t hours = ms_diff / 3600000LL;
    ms_diff %= 3600000LL;
    
    std::int64_t minutes = ms_diff / 60000LL;
    ms_diff %= 60000LL;
    
    std::int64_t seconds = ms_diff / 1000LL;
    std::int64_t milliseconds = ms_diff % 1000LL;
    
    return DateTimeDifference(
        0,              // millennia
        0,              // centuries
        0,              // decades
        0,              // years
        total_day_diff, // days
        hours,          // hours
        minutes,        // minutes
        seconds,        // seconds
        0,              // deciseconds
        0,              // centiseconds
        milliseconds,   // milliseconds
        0,              // microseconds
        0               // nanoseconds
    );
}

inline constexpr DateTimeDifference operator-(const DateTime& dt2, const DateTime& dt1) noexcept { 
    if (dt1 > dt2) { return -calculate_datetime_difference(dt2, dt1); }
    return calculate_datetime_difference(dt1, dt2); 
}

inline constexpr DateDifference calculate_date_difference(const Date& date1, const Date& date2) noexcept {
    const std::int64_t days1 = days_since_epoch(date1);
    const std::int64_t days2 = days_since_epoch(date2);
    const std::int64_t total_day_diff = days2 - days1;
    
    return DateDifference(
        0,              // millennia
        0,              // centuries
        0,              // decades
        0,              // years 
        total_day_diff  // days
    );
}

inline constexpr DateDifference operator-(const Date& date2, const Date& date1) noexcept { 
    if (date1 > date2) { return -calculate_date_difference(date2, date1); }
    return calculate_date_difference(date1, date2); 
}

inline constexpr TimeDifference calculate_time_difference(const Time& time1, const Time& time2) noexcept {
    std::int64_t ms1 = time_to_utc_milliseconds(time1);
    std::int64_t ms2 = time_to_utc_milliseconds(time2);
    std::int64_t total_ms_diff = ms2 - ms1;
    std::int64_t day_overflow = 0;
    
    if (total_ms_diff < 0) {
        // Time went backwards across midnight 
    } else if (total_ms_diff >= 86400000LL) {
        day_overflow = total_ms_diff / 86400000LL;
        total_ms_diff %= 86400000LL;
    }
    
    std::int64_t hours = total_ms_diff / 3600000LL;
    total_ms_diff %= 3600000LL;
    
    if (total_ms_diff < 0 && hours == 0) {
        hours = total_ms_diff / 3600000LL; 
        total_ms_diff = total_ms_diff % 3600000LL;

        if (total_ms_diff != 0) {
            hours -= 1; 
            total_ms_diff += 3600000LL;
        }
    }
    
    std::int64_t minutes = total_ms_diff / 60000LL;
    total_ms_diff %= 60000LL;
    std::int64_t seconds = total_ms_diff / 1000LL;
    std::int64_t milliseconds = total_ms_diff % 1000LL;
    hours += day_overflow * 24;
    
    return TimeDifference(
        hours,        // hours 
        minutes,      // minutes
        seconds,      // seconds
        0,            // deciseconds
        0,            // centiseconds
        milliseconds, // milliseconds
        0,            // microseconds
        0             // nanoseconds
    );
}

inline constexpr TimeDifference operator-(const Time& time2, const Time& time1) noexcept { 
    if (time1 > time2) { return -calculate_time_difference(time2, time1); }
    return calculate_time_difference(time1, time2); 
}

/*
FUTURE TIME
*/

inline constexpr Date get_future_date(const Date& base_date, DURATION_PARAM duration) noexcept {
    switch (duration.get_unit()) {
        case Duration::unit::day:
            return base_date.offset_date(0, 0, 0, static_cast<std::int64_t>(duration.count()));
        case Duration::unit::week:
            return base_date.offset_date(0, 0, static_cast<std::int64_t>(duration.count()), 0);
        case Duration::unit::year:
            return base_date.offset_date(static_cast<std::int64_t>(duration.count()), 0, 0, 0);
        case Duration::unit::decade:
            return base_date.offset_date(static_cast<std::int64_t>(duration.count()) * 10, 0, 0, 0);
        case Duration::unit::century:
            return base_date.offset_date(static_cast<std::int64_t>(duration.count()) * 100, 0, 0, 0);
        case Duration::unit::millennium:
            return base_date.offset_date(static_cast<std::int64_t>(duration.count()) * 1000, 0, 0, 0);
        default:
            return base_date.offset_date(0, 0, 0, static_cast<std::int64_t>(duration.to_days().value()));
    }
}

inline constexpr Date operator+(const Date& base_date, DURATION_PARAM duration) noexcept { return get_future_date(base_date, duration); }

inline constexpr Date get_future_date(const Date& base_date, const CompleteDuration& duration) noexcept {
    const std::int64_t total_years = static_cast<std::int64_t>(duration.millennia().value()) * 1000 +
                                    static_cast<std::int64_t>(duration.centuries().value()) * 100 +
                                    static_cast<std::int64_t>(duration.decades().value()) * 10 +
                                    static_cast<std::int64_t>(duration.years().value());
    
    const std::int64_t total_days = static_cast<std::int64_t>(duration.days().value()) +
                                   static_cast<std::int64_t>(duration.hours().value()) / 24 +
                                   static_cast<std::int64_t>(duration.minutes().value()) / (24 * 60) +
                                   static_cast<std::int64_t>(duration.seconds().value()) / (24 * 3600);
    
    return base_date.offset_date(total_years, 0, 0, total_days);
}

inline constexpr Date operator+(const Date& base_date, const CompleteDuration& duration) noexcept { return get_future_date(base_date, duration); }

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Date get_future_date(const Date& base_date, const T time_value) noexcept {
    return get_future_date(base_date, Duration(time_value));
}

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Date operator+(const Date& base_date, const T time_value) noexcept { return get_future_date(base_date, time_value); }

inline constexpr Time get_future_time(const Time& base_time, DURATION_PARAM duration) noexcept {
    switch (duration.get_unit()) {
        case Duration::unit::nanosecond:
            return base_time.offset_time(0, 0, 0, static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::microsecond:
            return base_time.offset_time(0, 0, 0, static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::millisecond:
            return base_time.offset_time(0, 0, 0, static_cast<std::int64_t>(duration.count()));
        case Duration::unit::centisecond:
            return base_time.offset_time(0, 0, 0, static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::decisecond:
            return base_time.offset_time(0, 0, 0, static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::second:
            return base_time.offset_time(0, 0, static_cast<std::int64_t>(duration.count()), 0);
        case Duration::unit::minute:
            return base_time.offset_time(0, static_cast<std::int64_t>(duration.count()), 0, 0);
        case Duration::unit::hour:
            return base_time.offset_time(static_cast<std::int64_t>(duration.count()), 0, 0, 0);
        default:
            return base_time.offset_time(static_cast<std::int64_t>(duration.to_hours().value()), 0, 0, 0);
    }
}

inline constexpr Time operator+(const Time& base_time, DURATION_PARAM duration) noexcept { return get_future_time(base_time, duration); }

inline constexpr Time get_future_time(const Time& base_time, const CompleteDuration& duration) noexcept {
    const std::int64_t total_hours = static_cast<std::int64_t>(duration.millennia().value()) * 365000 * 24 +
                                    static_cast<std::int64_t>(duration.centuries().value()) * 36500 * 24 +
                                    static_cast<std::int64_t>(duration.decades().value()) * 3650 * 24 +
                                    static_cast<std::int64_t>(duration.years().value()) * 365 * 24 +
                                    static_cast<std::int64_t>(duration.days().value()) * 24 +
                                    static_cast<std::int64_t>(duration.hours().value());
    
    const std::int64_t total_minutes = static_cast<std::int64_t>(duration.minutes().value());
    
    const std::int64_t total_seconds = static_cast<std::int64_t>(duration.seconds().value()) +
                                      static_cast<std::int64_t>(duration.deciseconds().value()) / 10 +
                                      static_cast<std::int64_t>(duration.centiseconds().value()) / 100;
    
    const std::int64_t total_milliseconds = static_cast<std::int64_t>(duration.milliseconds().value()) +
                                           static_cast<std::int64_t>(duration.microseconds().value()) / 1000 +
                                           static_cast<std::int64_t>(duration.nanoseconds().value()) / 1000000;
    
    return base_time.offset_time(total_hours, total_minutes, total_seconds, total_milliseconds);
}

inline constexpr Time operator+(const Time& base_time, const CompleteDuration& duration) noexcept { return get_future_time(base_time, duration); }

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Time get_future_time(const Time& base_time, const T time_value) noexcept {
    return get_future_time(base_time, Duration(time_value));
}

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Time operator+(const Time& base_time, const T time_value) noexcept { return get_future_time(base_time, time_value); }

inline constexpr DateTime get_future_datetime(const DateTime& base_datetime, DURATION_PARAM duration) noexcept {
    switch (duration.get_unit()) {
        case Duration::unit::nanosecond:
        case Duration::unit::microsecond:
        case Duration::unit::millisecond:
        case Duration::unit::centisecond:
        case Duration::unit::decisecond:
        case Duration::unit::second:
        case Duration::unit::minute:
        case Duration::unit::hour:
            return base_datetime.offset_date_time(0, 0, 0, 0, 
                                                static_cast<std::int64_t>(duration.to_hours().value()),
                                                static_cast<std::int64_t>(duration.to_minutes().value() % 60),
                                                static_cast<std::int64_t>(duration.to_seconds().value() % 60),
                                                static_cast<std::int64_t>(duration.to_milliseconds().value() % 1000));
        case Duration::unit::day:
            return base_datetime.offset_date_time(0, 0, 0, static_cast<std::int64_t>(duration.count()), 0, 0, 0, 0);
        case Duration::unit::week:
            return base_datetime.offset_date_time(0, 0, static_cast<std::int64_t>(duration.count()), 0, 0, 0, 0, 0);
        case Duration::unit::year:
            return base_datetime.offset_date_time(static_cast<std::int64_t>(duration.count()), 0, 0, 0, 0, 0, 0, 0);
        case Duration::unit::decade:
            return base_datetime.offset_date_time(static_cast<std::int64_t>(duration.count()) * 10, 0, 0, 0, 0, 0, 0, 0);
        case Duration::unit::century:
            return base_datetime.offset_date_time(static_cast<std::int64_t>(duration.count()) * 100, 0, 0, 0, 0, 0, 0, 0);
        case Duration::unit::millennium:
            return base_datetime.offset_date_time(static_cast<std::int64_t>(duration.count()) * 1000, 0, 0, 0, 0, 0, 0, 0);
        default:
            return base_datetime;
    }
}

inline constexpr DateTime operator+(const DateTime& base_date_time, DURATION_PARAM duration) noexcept { return get_future_datetime(base_date_time, duration); }

inline constexpr DateTime get_future_datetime(const DateTime& base_datetime, const CompleteDuration& duration) noexcept {
    const std::int64_t total_years = static_cast<std::int64_t>(duration.millennia().value()) * 1000 +
                                    static_cast<std::int64_t>(duration.centuries().value()) * 100 +
                                    static_cast<std::int64_t>(duration.decades().value()) * 10 +
                                    static_cast<std::int64_t>(duration.years().value());
    
    const std::int64_t total_days = static_cast<std::int64_t>(duration.days().value());
    const std::int64_t total_hours = static_cast<std::int64_t>(duration.hours().value());
    const std::int64_t total_minutes = static_cast<std::int64_t>(duration.minutes().value());
    
    const std::int64_t total_seconds = static_cast<std::int64_t>(duration.seconds().value()) +
                                      static_cast<std::int64_t>(duration.deciseconds().value()) / 10 +
                                      static_cast<std::int64_t>(duration.centiseconds().value()) / 100;
    
    const std::int64_t total_milliseconds = static_cast<std::int64_t>(duration.milliseconds().value()) +
                                           static_cast<std::int64_t>(duration.microseconds().value()) / 1000 +
                                           static_cast<std::int64_t>(duration.nanoseconds().value()) / 1000000;
    
    return base_datetime.offset_date_time(total_years, 0, 0, total_days, 
                                        total_hours, total_minutes, total_seconds, total_milliseconds);
}

inline constexpr DateTime operator+(const DateTime& base_date_time, const CompleteDuration& duration) noexcept { return get_future_datetime(base_date_time, duration); }

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr DateTime get_future_datetime(const DateTime& base_datetime, const T time_value) noexcept {
    return get_future_datetime(base_datetime, Duration(time_value));
}

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr DateTime operator+(const DateTime& base_date_time, const T time_value) noexcept { return get_future_datetime(base_date_time, time_value); }

/*
PAST TIME
*/

inline constexpr Date get_past_date(const Date& base_date, DURATION_PARAM duration) noexcept {
    switch (duration.get_unit()) {
        case Duration::unit::day:
            return base_date.offset_date(0, 0, 0, -static_cast<std::int64_t>(duration.count()));
        case Duration::unit::week:
            return base_date.offset_date(0, 0, -static_cast<std::int64_t>(duration.count()), 0);
        case Duration::unit::year:
            return base_date.offset_date(-static_cast<std::int64_t>(duration.count()), 0, 0, 0);
        case Duration::unit::decade:
            return base_date.offset_date(-static_cast<std::int64_t>(duration.count()) * 10, 0, 0, 0);
        case Duration::unit::century:
            return base_date.offset_date(-static_cast<std::int64_t>(duration.count()) * 100, 0, 0, 0);
        case Duration::unit::millennium:
            return base_date.offset_date(-static_cast<std::int64_t>(duration.count()) * 1000, 0, 0, 0);
        default:
            return base_date.offset_date(0, 0, 0, -static_cast<std::int64_t>(duration.to_days().value()));
    }
}

inline constexpr Date operator-(const Date& base_date, DURATION_PARAM duration) noexcept { return get_past_date(base_date, duration); }

inline constexpr Date get_past_date(const Date& base_date, const CompleteDuration& duration) noexcept {
    const std::int64_t total_years = static_cast<std::int64_t>(duration.millennia().value()) * 1000 +
                                    static_cast<std::int64_t>(duration.centuries().value()) * 100 +
                                    static_cast<std::int64_t>(duration.decades().value()) * 10 +
                                    static_cast<std::int64_t>(duration.years().value());
    
    const std::int64_t total_days = static_cast<std::int64_t>(duration.days().value()) +
                                   static_cast<std::int64_t>(duration.hours().value()) / 24 +
                                   static_cast<std::int64_t>(duration.minutes().value()) / (24 * 60) +
                                   static_cast<std::int64_t>(duration.seconds().value()) / (24 * 3600);
    
    return base_date.offset_date(-total_years, 0, 0, -total_days);
}

inline constexpr Date operator-(const Date& base_date, const CompleteDuration& duration) noexcept { return get_past_date(base_date, duration); }

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Date get_past_date(const Date& base_date, const T time_value) noexcept {
    return get_past_date(base_date, Duration(time_value));
}

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Date operator-(const Date& base_date, const T time_value) noexcept { return get_past_date(base_date, time_value); }

inline constexpr Time get_past_time(const Time& base_time, DURATION_PARAM duration) noexcept {
    switch (duration.get_unit()) {
        case Duration::unit::nanosecond:
            return base_time.offset_time(0, 0, 0, -static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::microsecond:
            return base_time.offset_time(0, 0, 0, -static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::millisecond:
            return base_time.offset_time(0, 0, 0, -static_cast<std::int64_t>(duration.count()));
        case Duration::unit::centisecond:
            return base_time.offset_time(0, 0, 0, -static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::decisecond:
            return base_time.offset_time(0, 0, 0, -static_cast<std::int64_t>(duration.to_milliseconds().value()));
        case Duration::unit::second:
            return base_time.offset_time(0, 0, -static_cast<std::int64_t>(duration.count()), 0);
        case Duration::unit::minute:
            return base_time.offset_time(0, -static_cast<std::int64_t>(duration.count()), 0, 0);
        case Duration::unit::hour:
            return base_time.offset_time(-static_cast<std::int64_t>(duration.count()), 0, 0, 0);
        default:
            return base_time.offset_time(-static_cast<std::int64_t>(duration.to_hours().value()), 0, 0, 0);
    }
}

inline constexpr Time operator-(const Time& base_time, DURATION_PARAM duration) noexcept { return get_past_time(base_time, duration); }

inline constexpr Time get_past_time(const Time& base_time, const CompleteDuration& duration) noexcept {
    const std::int64_t total_hours = static_cast<std::int64_t>(duration.millennia().value()) * 365000 * 24 +
                                    static_cast<std::int64_t>(duration.centuries().value()) * 36500 * 24 +
                                    static_cast<std::int64_t>(duration.decades().value()) * 3650 * 24 +
                                    static_cast<std::int64_t>(duration.years().value()) * 365 * 24 +
                                    static_cast<std::int64_t>(duration.days().value()) * 24 +
                                    static_cast<std::int64_t>(duration.hours().value());
    
    const std::int64_t total_minutes = static_cast<std::int64_t>(duration.minutes().value());
    
    const std::int64_t total_seconds = static_cast<std::int64_t>(duration.seconds().value()) +
                                      static_cast<std::int64_t>(duration.deciseconds().value()) / 10 +
                                      static_cast<std::int64_t>(duration.centiseconds().value()) / 100;
    
    const std::int64_t total_milliseconds = static_cast<std::int64_t>(duration.milliseconds().value()) +
                                           static_cast<std::int64_t>(duration.microseconds().value()) / 1000 +
                                           static_cast<std::int64_t>(duration.nanoseconds().value()) / 1000000;
    
    return base_time.offset_time(-total_hours, -total_minutes, -total_seconds, -total_milliseconds);
}

inline constexpr Time operator-(const Time& base_time, const CompleteDuration& duration) noexcept { return get_past_time(base_time, duration); }

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Time get_past_time(const Time& base_time, const T time_value) noexcept {
    return get_past_time(base_time, Duration(time_value));
}

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr Time operator-(const Time& base_time, const T time_value) noexcept { return get_past_time(base_time, time_value); }

inline constexpr DateTime get_past_datetime(const DateTime& base_datetime, DURATION_PARAM duration) noexcept {
    switch (duration.get_unit()) {
        case Duration::unit::nanosecond:
        case Duration::unit::microsecond:
        case Duration::unit::millisecond:
        case Duration::unit::centisecond:
        case Duration::unit::decisecond:
        case Duration::unit::second:
        case Duration::unit::minute:
        case Duration::unit::hour:
            return base_datetime.offset_date_time(0, 0, 0, 0, 
                                                -static_cast<std::int64_t>(duration.to_hours().value()),
                                                -static_cast<std::int64_t>(duration.to_minutes().value() % 60),
                                                -static_cast<std::int64_t>(duration.to_seconds().value() % 60),
                                                -static_cast<std::int64_t>(duration.to_milliseconds().value() % 1000));
        case Duration::unit::day:
            return base_datetime.offset_date_time(0, 0, 0, -static_cast<std::int64_t>(duration.count()), 0, 0, 0, 0);
        case Duration::unit::week:
            return base_datetime.offset_date_time(0, 0, -static_cast<std::int64_t>(duration.count()), 0, 0, 0, 0, 0);
        case Duration::unit::year:
            return base_datetime.offset_date_time(-static_cast<std::int64_t>(duration.count()), 0, 0, 0, 0, 0, 0, 0);
        case Duration::unit::decade:
            return base_datetime.offset_date_time(-static_cast<std::int64_t>(duration.count()) * 10, 0, 0, 0, 0, 0, 0, 0);
        case Duration::unit::century:
            return base_datetime.offset_date_time(-static_cast<std::int64_t>(duration.count()) * 100, 0, 0, 0, 0, 0, 0, 0);
        case Duration::unit::millennium:
            return base_datetime.offset_date_time(-static_cast<std::int64_t>(duration.count()) * 1000, 0, 0, 0, 0, 0, 0, 0);
        default:
            return base_datetime;
    }
}

inline constexpr DateTime operator-(const DateTime& base_date_time, DURATION_PARAM duration) noexcept { return get_past_datetime(base_date_time, duration); }

inline constexpr DateTime get_past_datetime(const DateTime& base_datetime, const CompleteDuration& duration) noexcept {
    const std::int64_t total_years = static_cast<std::int64_t>(duration.millennia().value()) * 1000 +
                                    static_cast<std::int64_t>(duration.centuries().value()) * 100 +
                                    static_cast<std::int64_t>(duration.decades().value()) * 10 +
                                    static_cast<std::int64_t>(duration.years().value());
    
    const std::int64_t total_days = static_cast<std::int64_t>(duration.days().value());
    const std::int64_t total_hours = static_cast<std::int64_t>(duration.hours().value());
    const std::int64_t total_minutes = static_cast<std::int64_t>(duration.minutes().value());
    
    const std::int64_t total_seconds = static_cast<std::int64_t>(duration.seconds().value()) +
                                      static_cast<std::int64_t>(duration.deciseconds().value()) / 10 +
                                      static_cast<std::int64_t>(duration.centiseconds().value()) / 100;
    
    const std::int64_t total_milliseconds = static_cast<std::int64_t>(duration.milliseconds().value()) +
                                           static_cast<std::int64_t>(duration.microseconds().value()) / 1000 +
                                           static_cast<std::int64_t>(duration.nanoseconds().value()) / 1000000;
    
    return base_datetime.offset_date_time(-total_years, 0, 0, -total_days, 
                                        -total_hours, -total_minutes, -total_seconds, -total_milliseconds);
}

inline constexpr DateTime operator-(const DateTime& base_date_time, const CompleteDuration& duration) noexcept { return get_past_datetime(base_date_time, duration); }

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr DateTime get_past_datetime(const DateTime& base_datetime, const T time_value) noexcept {
    return get_past_datetime(base_datetime, Duration(time_value));
}

template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline constexpr DateTime operator-(const DateTime& base_date_time, const T time_value) noexcept { return get_past_datetime(base_date_time, time_value); }

} // namespace time
} // namespace fizmo

#endif // DATE_TIME_UTIL_FUNCTIONS_HPP