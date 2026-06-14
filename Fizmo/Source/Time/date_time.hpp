#ifndef DATE_Time_CLASSES_HPP
#define DATE_Time_CLASSES_HPP

#include "date_time_difference.hpp"

namespace fizmo {
namespace time {

class Time {
public:
    constexpr Time() noexcept : m_hr(0), m_min(0), m_sec(0), m_mil(0), m_utc_hr(0), m_utc_min(0) {}

    constexpr Time(const hour h, const minute m, const second s, const millisecond ms = 0, const std::int8_t utc_offset_hours = 0, const std::int8_t utc_offset_minutes = 0) noexcept 
        : m_hr(init_time(h, m, s, ms).hour_), 
          m_min(init_time(h, m, s, ms).minute_), 
          m_sec(init_time(h, m, s, ms).second_), 
          m_mil(init_time(h, m, s, ms).millisecond_), 
          m_utc_hr(fizmo::clamp(utc_offset_hours, static_cast<std::int8_t>(-12), static_cast<int8_t>(14))), 
          m_utc_min(fizmo::clamp(utc_offset_minutes, static_cast<std::int8_t>(-59), static_cast<int8_t>(59))) 
    {} 

    constexpr Time(const Time& t) noexcept 
        : m_hr(t.m_hr), m_min(t.m_min), m_sec(t.m_sec), m_mil(t.m_mil),
          m_utc_hr(t.m_utc_hr), m_utc_min(t.m_utc_min) {}

    constexpr Time(Time&& t) noexcept
        : m_hr(t.m_hr), m_min(t.m_min), m_sec(t.m_sec), m_mil(t.m_mil),
          m_utc_hr(t.m_utc_hr), m_utc_min(t.m_utc_min) {
            t.m_hr = 0;
            t.m_min = 0;
            t.m_sec = 0;
            t.m_mil = 0;
    }

    OPTIONAL_CPP14_CONSTEXPR Time& operator=(const Time& t) noexcept {
        if (this != &t) {
            m_hr = t.m_hr;
            m_min = t.m_min;
            m_sec = t.m_sec;
            m_mil = t.m_mil;
            m_utc_hr = t.m_utc_hr;
            m_utc_min = t.m_utc_min;
        }

        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR Time& operator=(Time&& t) noexcept {
        if (this != &t) {
            m_hr = t.m_hr;
            m_min = t.m_min;
            m_sec = t.m_sec;
            m_mil = t.m_mil;
            m_utc_hr = t.m_utc_hr;
            m_utc_min = t.m_utc_min;

            t.m_hr = 0;
            t.m_min = 0;
            t.m_sec = 0;
            t.m_mil = 0;
        }

        return *this;
    }

public:
    constexpr bool operator==(const Time& t) const noexcept { return to_utc_milliseconds() == t.to_utc_milliseconds(); }
    constexpr bool operator!=(const Time& t) const noexcept { return !(*this == t); }
    constexpr bool operator<(const Time& t) const noexcept { return to_utc_milliseconds() < t.to_utc_milliseconds(); }
    constexpr bool operator<=(const Time& t) const noexcept { return to_utc_milliseconds() <= t.to_utc_milliseconds(); }
    constexpr bool operator>(const Time& t) const noexcept { return to_utc_milliseconds() > t.to_utc_milliseconds(); }
    constexpr bool operator>=(const Time& t) const noexcept { return to_utc_milliseconds() >= t.to_utc_milliseconds(); }

public:
    constexpr hour hours() const noexcept { return m_hr; }
    constexpr minute minutes() const noexcept { return m_min; }
    constexpr second seconds() const noexcept { return m_sec; }
    constexpr millisecond millseconds() const noexcept { return m_mil; }
    constexpr std::int8_t utc_offset_hours() const noexcept { return m_utc_hr; }
    constexpr std::int8_t& utc_offset_hours() noexcept { return m_utc_hr; }
    constexpr std::int8_t utc_offset_minutes() const noexcept { return m_utc_min; }
    constexpr std::int8_t& utc_offset_minutes() noexcept { return m_utc_min; }

public:
    constexpr bool is_pm() const noexcept { return m_hr.value() >= 12; }
    constexpr bool is_am() const noexcept { return !is_pm(); }

public:
    enum class format {
        ISO_8601_BASIC,     // 14:30:45
        ISO_8601_EXTENDED,  // 14:30:45.123
        TWELVE_HOUR,        // 02:30:45 PM
        TWELVE_HOUR_SHORT,  // 2:30 PM (no seconds)
        TWENTY_FOUR_HOUR,   // 14:30:45
        TWENTY_FOUR_SHORT,  // 14:30 (no seconds)
        MILITARY,           // 143045
        COMPACT,            // 143045123 (with milliseconds)
        CLOCK_12,           // 2:30:45.123 PM
        CLOCK_24,           // 14:30:45.123
        EUROPEAN,           // 14.30.45
        AMERICAN_LONG,      // 2:30:45.123 PM
        MINIMAL_12,         // 2:30p
        MINIMAL_24,         // 14:30
        VERBOSE_12          // 2 hours, 30 minutes, 45 seconds PM
    };

    std::string to_string(const format format = format::CLOCK_24, const bool show_milliseconds = true, const bool show_timezone = false) const {
        std::string result;
        
        auto get_12_hour = [&]() -> int {
            int display_hour = m_hr.value();
            if (display_hour == 0) {
                display_hour = 12; 
            } else if (display_hour > 12) {
                display_hour -= 12; 
            }
            return display_hour;
        };
        
        auto format_timezone = [&]() -> std::string {
            std::string tz;
            if (m_utc_hr >= 0) {
                tz += "+";
            } else {
                tz += "-";
            }
            
            const int abs_hr = std::abs(m_utc_hr);
            const int abs_min = std::abs(m_utc_min);
            if (abs_hr < 10) tz += "0";
            tz += std::to_string(abs_hr);
            tz += ":";
            if (abs_min < 10) tz += "0";
            tz += std::to_string(abs_min);
            return tz;
        };
        
        auto add_milliseconds = [&]() {
            if (show_milliseconds) {
                result += ".";
                if (m_mil.value() < 100) result += "0";
                if (m_mil.value() < 10) result += "0";
                result += std::to_string(m_mil.value());
            }
        };
        
        switch (format) {
            case format::ISO_8601_BASIC: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                add_milliseconds();
                break;
            }
            
            case format::ISO_8601_EXTENDED: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                result += ".";
                if (m_mil.value() < 100) result += "0";
                if (m_mil.value() < 10) result += "0";
                result += std::to_string(m_mil.value());
                break;
            }
            
            case format::TWELVE_HOUR: {
                int display_hour = get_12_hour();
                if (display_hour < 10) result += "0";
                result += std::to_string(display_hour);
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                add_milliseconds();
                result += is_pm() ? " PM" : " AM";
                break;
            }
            
            case format::TWELVE_HOUR_SHORT: {
                int display_hour = get_12_hour();
                if (display_hour < 10) result += "0";
                result += std::to_string(display_hour);
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += is_pm() ? " PM" : " AM";
                break;
            }
            
            case format::TWENTY_FOUR_HOUR: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                add_milliseconds();
                break;
            }
            
            case format::TWENTY_FOUR_SHORT: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                break;
            }
            
            case format::MILITARY: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                break;
            }
            
            case format::COMPACT: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                if (show_milliseconds) {
                    if (m_mil.value() < 100) result += "0";
                    if (m_mil.value() < 10) result += "0";
                    result += std::to_string(m_mil.value());
                }
                break;
            }
            
            case format::CLOCK_12: {
                int display_hour = get_12_hour();
                result += std::to_string(display_hour);
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                add_milliseconds();
                result += is_pm() ? " PM" : " AM";
                break;
            }
            
            case format::CLOCK_24: {
                result += std::to_string(m_hr.value());
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                add_milliseconds();
                break;
            }
            
            case format::EUROPEAN: {
                if (m_hr.value() < 10) result += "0";
                result += std::to_string(m_hr.value());
                result += ".";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ".";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                add_milliseconds();
                break;
            }
            
            case format::AMERICAN_LONG: {
                int display_hour = get_12_hour();
                result += std::to_string(display_hour);
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += ":";
                if (m_sec.value() < 10) result += "0";
                result += std::to_string(m_sec.value());
                result += ".";
                if (m_mil.value() < 100) result += "0";
                if (m_mil.value() < 10) result += "0";
                result += std::to_string(m_mil.value());
                result += is_pm() ? " PM" : " AM";
                break;
            }
            
            case format::MINIMAL_12: {
                int display_hour = get_12_hour();
                result += std::to_string(display_hour);
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                result += is_pm() ? "p" : "a";
                break;
            }
            
            case format::MINIMAL_24: {
                result += std::to_string(m_hr.value());
                result += ":";
                if (m_min.value() < 10) result += "0";
                result += std::to_string(m_min.value());
                break;
            }
            
            case format::VERBOSE_12: {
                int display_hour = get_12_hour();
                result += std::to_string(display_hour);
                result += (display_hour == 1) ? " hour, " : " hours, ";
                result += std::to_string(m_min.value());
                result += (m_min.value() == 1) ? " minute, " : " minutes, ";
                result += std::to_string(m_sec.value());
                result += (m_sec.value() == 1) ? " second " : " seconds ";
                result += is_pm() ? "PM" : "AM";
                break;
            }
        }
        
        if (show_timezone) {
            result += " ";
            result += format_timezone();
        }
        
        return result;
    }

    std::string to_string(const bool show_timezone) const { return to_string(format::CLOCK_24, true, show_timezone); }
    std::string to_iso_string() const { return to_string(format::ISO_8601_BASIC); }
    std::string to_iso_extended_string() const { return to_string(format::ISO_8601_EXTENDED); }
    std::string to_12_hour_string() const { return to_string(format::TWELVE_HOUR); }
    std::string to_24_hour_string() const { return to_string(format::TWENTY_FOUR_HOUR); }
    std::string to_military_string() const { return to_string(format::MILITARY); }
    std::string to_compact_string() const { return to_string(format::COMPACT); }
    std::string to_clock_12_string() const { return to_string(format::CLOCK_12); }
    std::string to_clock_24_string() const { return to_string(format::CLOCK_24); }
    std::string to_european_string() const { return to_string(format::EUROPEAN); }
    std::string to_american_long_string() const { return to_string(format::AMERICAN_LONG); }
    std::string to_minimal_12_string() const { return to_string(format::MINIMAL_12); }
    std::string to_minimal_24_string() const { return to_string(format::MINIMAL_24); }
    std::string to_verbose_12_string() const { return to_string(format::VERBOSE_12); }

    friend std::ostream& operator<<(std::ostream& os, const Time& t) {
        os << t.to_string();
        return os;
    }

public:
    template <
        typename H, typename M, typename S, typename MS, 
        typename = typename std::enable_if<
            std::is_integral<H>::value &&
            std::is_integral<M>::value &&
            std::is_integral<S>::value &&
            std::is_integral<MS>::value
        >::type
    >
    constexpr Time offset_time(const H hour_offset, const M minute_offset, const S second_offset, const MS millisecond_offset) const noexcept {
        const std::int64_t current_ms = static_cast<std::int64_t>(m_hr.value()) * 3600000 + 
                                        static_cast<std::int64_t>(m_min.value()) * 60000 +
                                        static_cast<std::int64_t>(m_sec.value()) * 1000 +
                                        static_cast<std::int64_t>(m_mil.value());
    
        const std::int64_t total_offset_ms = static_cast<std::int64_t>(hour_offset) * 3600000 +
                                            static_cast<std::int64_t>(minute_offset) * 60000 +
                                            static_cast<std::int64_t>(second_offset) * 1000 +
                                            static_cast<std::int64_t>(millisecond_offset);
        
        std::int64_t new_ms = current_ms + total_offset_ms;
        constexpr std::int64_t DAY_MS = 86400000;
        new_ms = ((new_ms % DAY_MS) + DAY_MS) % DAY_MS;
        const hour new_hours = new_ms / 3600000;
        new_ms %= 3600000;
        const minute new_minutes = new_ms / 60000;
        new_ms %= 60000;
        const second new_seconds = new_ms / 1000;
        const millisecond new_milliseconds = new_ms % 1000;
        return Time(new_hours, new_minutes, new_seconds, new_milliseconds, m_utc_hr, m_utc_min);
    }

    template <
        typename H, typename M, typename S, typename MS, 
        typename = typename std::enable_if<
            std::is_integral<H>::value &&
            std::is_integral<M>::value &&
            std::is_integral<S>::value &&
            std::is_integral<MS>::value
        >::type
    >
    OPTIONAL_CPP14_CONSTEXPR Time& offset_this_Time(const H hour_offset, const M minute_offset, const S second_offset, const MS millisecond_offset) noexcept {
        *this = offset_time(hour_offset, minute_offset, second_offset, millisecond_offset);
        return *this;
    }

public:
    constexpr std::uint32_t to_utc_milliseconds() const noexcept {
        const std::uint32_t local_ms = m_hr.value() * 3600000 + 
                               m_min.value() * 60000 +
                               m_sec.value() * 1000 +
                               m_mil.value();

        const std::int32_t utc_offset_ms = m_utc_hr * 3600000 + m_utc_min * 60000;
        std::int32_t utc_ms = local_ms - utc_offset_ms;
        constexpr std::uint32_t DAY_MS = 86400000;
        utc_ms = ((utc_ms % DAY_MS) + DAY_MS) % DAY_MS;
        return static_cast<std::uint32_t>(utc_ms);
    }

private:
    struct time_inits {
        hour hour_;
        minute minute_;
        second second_;
        millisecond millisecond_;
    };

    static constexpr time_inits init_time(const hour h, const minute m, const second s, const millisecond ms) noexcept {
        time_inits inits;
        inits.hour_ = ((h.value() + (m.value() + (s.value() + ms.value() / 1000) / 60) / 60) % 24);
        inits.minute_ = ((m.value() + (s.value() + ms.value() / 1000) / 60) % 60);
        inits.second_ = ((s.value() + ms.value() / 1000) % 60);
        inits.millisecond_ = (ms.value() % 1000);
        return inits;
    }

private:
    hour m_hr;
    minute m_min;
    second m_sec;
    millisecond m_mil;
    std::int8_t m_utc_hr;
    std::int8_t m_utc_min;
};

class Date {
private:
    year m_yr;
    month m_m;
    day m_d;
    bool m_is_ad;

public:
    constexpr Date() noexcept : m_yr(1600), m_m(months::January), m_d(1), m_is_ad(true) {}

    constexpr Date(const year y, const month m, const day d, const bool is_ad = true) noexcept 
        : m_yr(init_date(y, m, d).y_), 
          m_m(init_date(y, m, d).m_),
          m_d(init_date(y, m, d).d_),
          m_is_ad(is_ad)
    {}

    constexpr Date(const Date& d) noexcept : m_yr(d.m_yr), m_m(d.m_m), m_d(d.m_d), m_is_ad(d.m_is_ad) {}

    constexpr Date(Date&& d) noexcept : m_yr(d.m_yr), m_m(d.m_m), m_d(d.m_d), m_is_ad(d.m_is_ad) {
        d.m_yr = 0;
        d.m_m = months::January;
        d.m_d = 1;
        d.m_is_ad = true;
    }

    OPTIONAL_CPP14_CONSTEXPR Date& operator=(const Date& d) noexcept {
        if (this != &d) {
            m_yr = d.m_yr;
            m_m = d.m_m;
            m_d = d.m_d;
            m_is_ad = d.m_is_ad;
        }

        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR Date& operator=(Date&& d) noexcept {
        if (this != &d) {
            m_yr = d.m_yr;
            m_m = d.m_m;
            m_d = d.m_d;
            m_is_ad = d.m_is_ad;
            d.m_yr = 0;
            d.m_m = months::January;
            d.m_d = 1;
            d.m_is_ad = true;
        }

        return *this;
    }

public:
    constexpr year get_year() const noexcept { return m_yr; }
    constexpr month get_month() const noexcept { return m_m; }
    constexpr day get_day() const noexcept { return m_d; }
    constexpr bool is_ad_year() const noexcept { return m_is_ad; }
    constexpr bool& is_ad_year() noexcept { return m_is_ad; }
    constexpr bool is_leap_year() const noexcept { return Calendar::is_leap_year(m_yr); }

    constexpr weekday get_weekday() const noexcept {
        // Zeller's congruence algorithm
        // This assumes Gregorian Calendar rules for all dates
        std::int64_t year_val = static_cast<std::int64_t>(m_yr.value());
        std::uint8_t month_val = m_m.to_int() + 1; 
        std::uint64_t day_val = m_d.value();
        if (!m_is_ad) { year_val = -year_val; }
        
        // For Zeller's congruence, January and February are counted as 
        // months 13 and 14 of the previous year
        if (month_val < 3) {
            month_val += 12;
            year_val -= 1;
        }
        
        const std::uint64_t q = day_val;
        const std::uint8_t m = month_val;
        const std::int8_t K = year_val % 100;
        const std::int64_t J = year_val / 100;
        std::int8_t h = (q + (13 * (m + 1)) / 5 + K + K / 4 + J / 4 - 2 * J) % 7;
        if (h < 0) { h += 7; }
        const std::uint8_t weekday_index = (h + 5) % 7;
        return weekday(static_cast<weekdays>(weekday_index));
    }

public:
    constexpr bool operator==(const Date& d) const noexcept {
        return m_is_ad == d.m_is_ad && 
               m_yr.value() == d.m_yr.value() && 
               m_m.value() == d.m_m.value() && 
               m_d.value() == d.m_d.value();
    }

    constexpr bool operator!=(const Date& d) const noexcept { return !(*this == d); }

    constexpr bool operator<(const Date& d) const noexcept {
        if (!m_is_ad && d.m_is_ad) { return true; }
        if (m_is_ad && !d.m_is_ad) { return false; }
        
        if (m_is_ad && d.m_is_ad) {
            if (m_yr.value() != d.m_yr.value()) { return m_yr.value() < d.m_yr.value(); }
            if (m_m.value() != d.m_m.value()) { return m_m.value() < d.m_m.value(); }
            return m_d.value() < d.m_d.value();
        }
        
        if (m_yr.value() != d.m_yr.value()) { return m_yr.value() > d.m_yr.value();  }
        if (m_m.value() != d.m_m.value()) { return m_m.value() < d.m_m.value(); }
        return m_d.value() < d.m_d.value();
    }

    constexpr bool operator<=(const Date& d) const noexcept { return *this < d || *this == d; }
    constexpr bool operator>(const Date& d) const noexcept { return !(*this <= d); }
    constexpr bool operator>=(const Date& d) const noexcept { return !(*this < d); }

public:
    template <
        typename Y, typename M, typename W, typename D,
        typename = typename std::enable_if<
            std::is_integral<Y>::value &&
            std::is_integral<M>::value &&
            std::is_integral<W>::value &&
            std::is_integral<D>::value
        >::type
    >
    constexpr Date offset_date(const Y year_offset, const M month_offset, const W week_offset, const D day_offset) const noexcept {
        std::int64_t new_year = static_cast<std::int64_t>(m_yr.value());
        std::int64_t new_month = static_cast<std::int64_t>(m_m.to_int()) + 1; 
        std::int64_t new_day = static_cast<std::int64_t>(m_d.value());
        bool new_is_ad = m_is_ad;
        
        if (year_offset != 0) {
            if (new_is_ad) {
                new_year += year_offset;

                if (new_year <= 0) {
                    new_is_ad = false;
                    new_year = 1 - new_year; 
                }
            } else {
                new_year -= year_offset; 

                if (new_year <= 0) {
                    new_is_ad = true;
                    new_year = 1 - new_year; 
                }
            }
        }
        
        if (month_offset != 0) {
            new_month += month_offset;
            
            while (new_month > 12) {
                new_month -= 12;

                if (new_is_ad) {
                    new_year++;
                } else {
                    new_year--;

                    if (new_year <= 0) {
                        new_is_ad = true;
                        new_year = 1;
                    }
                }
            }
            
            while (new_month < 1) {
                new_month += 12;

                if (new_is_ad) {
                    new_year--;

                    if (new_year <= 0) {
                        new_is_ad = false;
                        new_year = 1;
                    }
                } else {
                    new_year++;
                }
            }
        }
        
        const year temp_year(static_cast<std::uint64_t>(new_year));
        const month temp_month(static_cast<months>(new_month - 1)); 
        const std::uint8_t max_days = Calendar::days_in_month(temp_month, temp_year);
        if (new_day > max_days) { new_day = max_days; }
        const std::int64_t total_day_offset = static_cast<std::int64_t>(week_offset) * 7 + static_cast<std::int64_t>(day_offset);
        
        if (total_day_offset != 0) {
            new_day += total_day_offset;
            
            while (new_day > Calendar::days_in_month(month(static_cast<months>(new_month - 1)), year(static_cast<std::uint64_t>(new_year)))) {
                new_day -= Calendar::days_in_month(month(static_cast<months>(new_month - 1)), year(static_cast<std::uint64_t>(new_year)));
                new_month++;
                
                if (new_month > 12) {
                    new_month = 1;
                    if (new_is_ad) {
                        new_year++;
                    } else {
                        new_year--;
                        if (new_year <= 0) {
                            new_is_ad = true;
                            new_year = 1;
                        }
                    }
                }
            }
            
            while (new_day < 1) {
                new_month--;
                
                if (new_month < 1) {
                    new_month = 12;

                    if (new_is_ad) {
                        new_year--;
                        if (new_year <= 0) {
                            new_is_ad = false;
                            new_year = 1;
                        }
                    } else {
                        new_year++;
                    }
                }
                
                new_day += Calendar::days_in_month(month(static_cast<months>(new_month - 1)), year(static_cast<std::uint64_t>(new_year)));
            }
        }
        
        return Date(
            year(static_cast<std::uint64_t>(new_year)),
            month(static_cast<months>(new_month - 1)),
            day(static_cast<std::uint64_t>(new_day)),
            new_is_ad
        );
    }

    template <
        typename Y, typename M, typename W, typename D,
        typename = typename std::enable_if<
            std::is_integral<Y>::value &&
            std::is_integral<M>::value &&
            std::is_integral<W>::value &&
            std::is_integral<D>::value
        >::type
    >
    OPTIONAL_CPP14_CONSTEXPR Date& offset_this_date(const Y year_offset, const M month_offset, const W week_offset, const D day_offset) noexcept {
        *this = offset_date(year_offset, month_offset, week_offset, day_offset);
        return *this;
    }

public:
    enum class format {
        ISO_8601,           // 2024-01-15
        US_LONG,            // January 15, 2024
        US_SHORT,           // 01/15/2024
        EUROPEAN,           // 15/01/2024
        MLA,                // 15 January 2024
        APA,                // January 15, 2024
        COMPACT,            // 20240115
        LONG_WITH_DAY,      // Monday, January 15, 2024
        ORDINAL,            // January 15th, 2024
        BRITISH,            // 15th January 2024
        SLASH_YMD,          // 2024/01/15
        DOT_EUROPEAN,       // 15.01.2024
        DOT_US,             // 01.15.2024
        CHICAGO             // Jan. 15, 2024
    };

    std::string to_string(const format format = format::US_LONG, const bool use_bce_ce = false) const {
        std::string result;
        
        auto get_era_suffix = [&](bool use_bce_ce) -> std::string {
            if (m_is_ad) {
                return use_bce_ce ? " CE" : " AD";
            } else {
                return use_bce_ce ? " BCE" : " BC";
            }
        };
        
        switch (format) {
            case format::ISO_8601: {
                if (!m_is_ad) result += "-";
                result += std::to_string(m_yr.value());
                result += "-";
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                result += "-";
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                break;
            }
            
            case format::US_LONG: {
                result += m_m.to_string();
                result += " ";
                result += std::to_string(m_d.value());
                result += ", ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::US_SHORT: {
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                result += "/";
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                result += "/";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::EUROPEAN: {
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                result += "/";
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                result += "/";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::MLA: {
                auto get_abbreviated_month = [&]() -> std::string {
                    switch (m_m.value()) {
                        case months::January: return "Jan";
                        case months::February: return "Feb"; 
                        case months::March: return "Mar";
                        case months::April: return "Apr";
                        case months::May: return "May";
                        case months::June: return "Jun";
                        case months::July: return "Jul";
                        case months::August: return "Aug";
                        case months::September: return "Sep";
                        case months::October: return "Oct";
                        case months::November: return "Nov";
                        case months::December: return "Dec";
                        default: return "Jan";
                    }
                };

                result += std::to_string(m_d.value());
                result += " ";
                result += get_abbreviated_month();
                result += " ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::APA: {
                result += m_m.to_string();
                result += " ";
                result += std::to_string(m_d.value());
                result += ", ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::COMPACT: {
                if (!m_is_ad) result += "-";
                result += std::to_string(m_yr.value());
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                break;
            }
            
            case format::LONG_WITH_DAY: {
                result += m_m.to_string();
                result += " ";
                result += std::to_string(m_d.value());
                result += ", ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::ORDINAL: {
                auto get_ordinal_suffix = [](int day) -> std::string {
                    if (day >= 11 && day <= 13) return "th";
                    switch (day % 10) {
                        case 1: return "st";
                        case 2: return "nd"; 
                        case 3: return "rd";
                        default: return "th";
                    }
                };

                result += m_m.to_string();
                result += " ";
                result += std::to_string(m_d.value());
                result += get_ordinal_suffix(static_cast<int>(m_d.value()));
                result += ", ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::BRITISH: {
                auto get_ordinal_suffix = [](int day) -> std::string {
                    if (day >= 11 && day <= 13) return "th";
                    switch (day % 10) {
                        case 1: return "st";
                        case 2: return "nd"; 
                        case 3: return "rd";
                        default: return "th";
                    }
                };

                result += std::to_string(m_d.value());
                result += get_ordinal_suffix(static_cast<int>(m_d.value()));
                result += " ";
                result += m_m.to_string();
                result += " ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::SLASH_YMD: {
                result += std::to_string(m_yr.value());
                result += "/";
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                result += "/";
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
            
            case format::DOT_EUROPEAN: {
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                result += ".";
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                result += ".";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }

            case format::DOT_US: {
                if (m_m.to_int() + 1 < 10) result += "0";
                result += std::to_string(m_m.to_int() + 1);
                result += ".";
                if (m_d.value() < 10) result += "0";
                result += std::to_string(m_d.value());
                result += ".";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }

            case format::CHICAGO: {
                auto get_chicago_month = [&]() -> std::string {
                    switch (m_m.value()) {
                        case months::January: return "Jan.";
                        case months::February: return "Feb."; 
                        case months::March: return "Mar.";
                        case months::April: return "Apr.";
                        case months::May: return "May";  
                        case months::June: return "June"; 
                        case months::July: return "July"; 
                        case months::August: return "Aug.";
                        case months::September: return "Sept.";
                        case months::October: return "Oct.";
                        case months::November: return "Nov.";
                        case months::December: return "Dec.";
                        default: return "Jan.";
                    }
                };

                result += get_chicago_month();
                result += " ";
                result += std::to_string(m_d.value());
                result += ", ";
                result += std::to_string(m_yr.value());
                if (!m_is_ad) result += get_era_suffix(use_bce_ce);
                break;
            }
        }
        
        return result;
    }

    std::string to_iso_string() const { return to_string(format::ISO_8601); }
    std::string to_us_long_string() const { return to_string(format::US_LONG); }
    std::string to_us_short_string() const { return to_string(format::US_SHORT); }
    std::string to_european_string() const { return to_string(format::EUROPEAN); }
    std::string to_mla_string() const { return to_string(format::MLA); }
    std::string to_apa_string() const { return to_string(format::APA); }
    std::string to_compact_string() const { return to_string(format::COMPACT); }
    std::string to_long_with_day_string() const { return to_string(format::LONG_WITH_DAY); }
    std::string to_ordinal_string() const { return to_string(format::ORDINAL); }
    std::string to_british_string() const { return to_string(format::BRITISH); }
    std::string to_slah_ymd_string() const { return to_string(format::SLASH_YMD); }
    std::string to_european_dot_string() const { return to_string(format::DOT_EUROPEAN); }
    std::string to_us_dot_string() const { return to_string(format::DOT_US); }
    std::string to_chicago_string() const { return to_string(format::CHICAGO); }

    friend std::ostream& operator<<(std::ostream& os, const Date& d) {
        os << d.to_string();
        return os;
    }

private:
    struct date_inits {
        year y_;
        month m_;
        day d_;
    };

    static constexpr date_inits init_date(const year y, const month m, const day d) noexcept {
        date_inits inits;
        inits.y_ = y;
        inits.m_ = m;
        inits.d_ = fizmo::clamp(static_cast<std::uint8_t>(d.value()), static_cast<std::uint8_t>(1), Calendar::days_in_month(m, y));
        return inits;
    }
};

constexpr weekday Calendar::get_weekday(const year y, const month m, const day d, const bool is_ad_year) noexcept { return Date(y, m, d, is_ad_year).get_weekday(); }

class DateTime {
private:
    Date m_date;
    Time m_time;

public:
    constexpr DateTime() noexcept : m_date(), m_time() {}
    constexpr DateTime(const Date& d) noexcept : m_date(d), m_time() {}
    constexpr DateTime(Date&& d) noexcept : m_date(std::move(d)), m_time() {}
    constexpr DateTime(const Time& t) noexcept : m_date(), m_time(t) {}
    constexpr DateTime(Time&& t) noexcept : m_date(), m_time(std::move(t)) {}
    constexpr DateTime(const Date& d, const Time& t) noexcept : m_date(d), m_time(t) {}
    constexpr DateTime(Date&& d, const Time& t) noexcept : m_date(std::move(d)), m_time(t) {}
    constexpr DateTime(const Date& d, Time&& t) noexcept : m_date(d), m_time(std::move(t)) {}
    constexpr DateTime(Date&& d, Time&& t) noexcept : m_date(std::move(d)), m_time(std::move(t)) {}
    constexpr DateTime(const Time& t, const Date& d) noexcept : m_date(d), m_time(t) {}
    constexpr DateTime(Time&& t, const Date& d) noexcept : m_date(d), m_time(std::move(t)) {}
    constexpr DateTime(const Time& t, Date&& d) noexcept : m_date(std::move(d)), m_time(t) {}
    constexpr DateTime(Time&& t, Date&& d) noexcept : m_date(std::move(d)), m_time(std::move(t)) {}

    constexpr DateTime(const year y, const month m, const day d, 
                      const hour h, const minute min, const second s, 
                      const millisecond ms = 0, const bool is_ad = true,
                      const std::int8_t utc_offset_hours = 0, 
                      const std::int8_t utc_offset_minutes = 0) noexcept
        : m_date(y, m, d, is_ad), 
          m_time(h, min, s, ms, utc_offset_hours, utc_offset_minutes) {}

    constexpr DateTime(const DateTime& dt) noexcept : m_date(dt.m_date), m_time(dt.m_time) {}
    constexpr DateTime(DateTime&& dt) noexcept : m_date(std::move(dt.m_date)), m_time(std::move(dt.m_time)) {}

    OPTIONAL_CPP14_CONSTEXPR DateTime& operator=(const DateTime& dt) noexcept {
        if (this != &dt) {
            m_date = dt.m_date;
            m_time = dt.m_time;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTime& operator=(DateTime&& dt) noexcept {
        if (this != &dt) {
            m_date = std::move(dt.m_date);
            m_time = std::move(dt.m_time);
        }
        return *this;
    }

public:
    constexpr bool operator==(const DateTime& dt) const noexcept { return m_date == dt.m_date && m_time == dt.m_time; }
    constexpr bool operator!=(const DateTime& dt) const noexcept { return !(*this == dt); }

    constexpr bool operator<(const DateTime& dt) const noexcept { 
        if (m_date != dt.m_date) return m_date < dt.m_date;
        return m_time < dt.m_time;
    }

    constexpr bool operator<=(const DateTime& dt) const noexcept { return *this < dt || *this == dt; }
    constexpr bool operator>(const DateTime& dt) const noexcept { return !(*this <= dt); }
    constexpr bool operator>=(const DateTime& dt) const noexcept { return !(*this < dt); }

public:
    constexpr Date get_date() const noexcept { return m_date; }
    constexpr Time get_time() const noexcept { return m_time; }
    constexpr Date& get_date() noexcept { return m_date; }
    constexpr Time& get_time() noexcept { return m_time; }

    constexpr year get_year() const noexcept { return m_date.get_year(); }
    constexpr month get_month() const noexcept { return m_date.get_month(); }
    constexpr day get_day() const noexcept { return m_date.get_day(); }

    constexpr bool is_ad_year() const noexcept { return m_date.is_ad_year(); }
    constexpr bool& is_ad_year() noexcept { return m_date.is_ad_year(); }

    constexpr hour get_hour() const noexcept { return m_time.hours(); }
    constexpr minute get_minute() const noexcept { return m_time.minutes(); }
    constexpr second get_second() const noexcept { return m_time.seconds(); }
    constexpr millisecond get_millsecond() const noexcept { return m_time.millseconds(); }

    constexpr std::int8_t utc_offset_hours() const noexcept { return m_time.utc_offset_hours(); }
    constexpr std::int8_t& utc_offset_hours() noexcept { return m_time.utc_offset_hours(); }
    constexpr std::int8_t utc_offset_minutes() const noexcept { return m_time.utc_offset_minutes(); }
    constexpr std::int8_t& utc_offset_minutes() noexcept { return m_time.utc_offset_minutes(); }

public:
    constexpr bool is_pm() const noexcept { return m_time.is_pm(); }
    constexpr bool is_am() const noexcept { return m_time.is_am(); }
    constexpr bool is_leap_year() const noexcept { return m_date.is_leap_year(); }
    constexpr weekday get_weekday() const noexcept { return m_date.get_weekday(); }    

public:
    enum class format {
        ISO_8601,               // 2024-01-15T14:30:45.123
        ISO_8601_BASIC,         // 20240115T143045
        TIME_ON_DATE,           // 14:30:45 on January 15, 2024
        DATE_AT_TIME,           // January 15, 2024 at 14:30:45
        US_LONG,                // January 15, 2024 2:30:45 PM
        US_SHORT,               // 01/15/2024 2:30 PM
        EUROPEAN,               // 15/01/2024 14:30:45
        RFC_2822,               // Mon, 15 Jan 2024 14:30:45 +0000
        COMPACT,                // 20240115143045123
        VERBOSE,                // Monday, January 15th, 2024 at 2:30:45.123 PM
        MILITARY,               // 15JAN2024 143045Z
        CASUAL,                 // Jan 15, 2024 2:30pm
        LOG_FORMAT,             // 2024-01-15 14:30:45.123
        DATABASE,               // 2024-01-15 14:30:45
        TIMESTAMP,              // 15-Jan-2024 14:30:45
        SIMPLE                  // 1/15/24 2:30 PM
    };

    std::string to_string(const format format = format::DATE_AT_TIME, const bool show_milliseconds = true, const bool show_timezone = false) const {
        std::string result;
        
        auto format_timezone = [&]() -> std::string {
            std::string tz;
            const std::int8_t utc_hr = m_time.utc_offset_hours();
            const std::int8_t utc_min = m_time.utc_offset_minutes();
            
            if (utc_hr >= 0) {
                tz += "+";
            } else {
                tz += "-";
            }
            
            const int abs_hr = std::abs(utc_hr);
            const int abs_min = std::abs(utc_min);
            if (abs_hr < 10) tz += "0";
            tz += std::to_string(abs_hr);
            tz += ":";
            if (abs_min < 10) tz += "0";
            tz += std::to_string(abs_min);
            return tz;
        };
        
        switch (format) {
            case format::ISO_8601: {
                result += m_date.to_iso_string();
                result += "T";
                result += m_time.to_string(Time::format::ISO_8601_BASIC, show_milliseconds, false);
                if (show_timezone) { result += format_timezone(); }
                break;
            }
            
            case format::ISO_8601_BASIC: {
                result += m_date.to_compact_string();
                result += "T";
                result += m_time.to_string(Time::format::COMPACT, show_milliseconds, false);
                if (show_timezone) { result += format_timezone(); }
                break;
            }
            
            case format::TIME_ON_DATE: {
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, show_milliseconds, false);
                result += " on ";
                result += m_date.to_string(Date::format::US_LONG);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::DATE_AT_TIME: {
                result += m_date.to_string(Date::format::US_LONG);
                result += " at ";
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, show_milliseconds, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::US_LONG: {
                result += m_date.to_string(Date::format::US_LONG);
                result += " ";
                result += m_time.to_string(Time::format::TWELVE_HOUR, show_milliseconds, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::US_SHORT: {
                result += m_date.to_string(Date::format::US_SHORT);
                result += " ";
                result += m_time.to_string(Time::format::TWELVE_HOUR_SHORT, false, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::EUROPEAN: {
                result += m_date.to_string(Date::format::EUROPEAN);
                result += " ";
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, show_milliseconds, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::RFC_2822: {
                result += m_date.to_string(Date::format::MLA);
                result += " ";
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, false, false);
                result += " ";
                result += format_timezone();
                break;
            }
            
            case format::COMPACT: {
                result += m_date.to_compact_string();
                result += m_time.to_string(Time::format::COMPACT, show_milliseconds, false);
                break;
            }
            
            case format::VERBOSE: {
                result += m_date.to_string(Date::format::ORDINAL);
                result += " at ";
                result += m_time.to_string(Time::format::AMERICAN_LONG, show_milliseconds, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::MILITARY: {
                result += m_date.to_string(Date::format::MLA);
                result += " ";
                result += m_time.to_string(Time::format::MILITARY, false, false);
                result += "Z";
                break;
            }
            
            case format::CASUAL: {
                result += m_date.to_string(Date::format::CHICAGO);
                result += " ";
                result += m_time.to_string(Time::format::MINIMAL_12, false, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::LOG_FORMAT: {
                result += m_date.to_iso_string();
                result += " ";
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, show_milliseconds, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::DATABASE: {
                result += m_date.to_iso_string();
                result += " ";
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, false, false);
                break;
            }
            
            case format::TIMESTAMP: {
                result += m_date.to_string(Date::format::MLA);
                result += " ";
                result += m_time.to_string(Time::format::TWENTY_FOUR_HOUR, false, false);

                if (show_timezone) {
                    result += " ";
                    result += format_timezone();
                }

                break;
            }
            
            case format::SIMPLE: {
                const auto year_str = std::to_string(m_date.get_year().value());
                const auto short_year = year_str.length() >= 2 ? year_str.substr(year_str.length() - 2) : year_str;
                result += std::to_string(m_date.get_month().to_int() + 1);
                result += "/";
                result += std::to_string(m_date.get_day().value());
                result += "/";
                result += short_year;
                result += " ";
                result += m_time.to_string(Time::format::TWELVE_HOUR_SHORT, false, false);
                break;
            }
        }
        
        return result;
    }

    std::string to_iso_string() const { return to_string(format::ISO_8601); }
    std::string to_iso_basic_string() const { return to_string(format::ISO_8601_BASIC); }
    std::string to_us_long_string() const { return to_string(format::US_LONG); }
    std::string to_us_short_string() const { return to_string(format::US_SHORT); }
    std::string to_european_string() const { return to_string(format::EUROPEAN); }
    std::string to_rfc2822_string() const { return to_string(format::RFC_2822, true, true); }
    std::string to_compact_string() const { return to_string(format::COMPACT); }
    std::string to_verbose_string() const { return to_string(format::VERBOSE); }
    std::string to_military_string() const { return to_string(format::MILITARY); }
    std::string to_casual_string() const { return to_string(format::CASUAL); }
    std::string to_log_string() const { return to_string(format::LOG_FORMAT); }
    std::string to_database_string() const { return to_string(format::DATABASE); }
    std::string to_timestamp_string() const { return to_string(format::TIMESTAMP); }
    std::string to_simple_string() const { return to_string(format::SIMPLE); }

    friend std::ostream& operator<<(std::ostream& os, const DateTime& dt) {
        os << dt.to_string();
        return os;
    }

public:
    template <
        typename Y, typename M, typename W, typename D, typename H, typename MIN, typename S, typename MS,
        typename = typename std::enable_if<
            std::is_integral<Y>::value && std::is_integral<M>::value &&
            std::is_integral<W>::value && std::is_integral<D>::value &&
            std::is_integral<H>::value && std::is_integral<MIN>::value &&
            std::is_integral<S>::value && std::is_integral<MS>::value
        >::type
    >
    constexpr DateTime offset_date_time(const Y year_offset, const M month_offset, const W week_offset, 
                                     const D day_offset, const H hour_offset, const MIN minute_offset, 
                                     const S second_offset, const MS millisecond_offset) const noexcept {
        
        const std::int64_t current_ms = static_cast<std::int64_t>(m_time.hours().value()) * 3600000 + 
                                        static_cast<std::int64_t>(m_time.minutes().value()) * 60000 +
                                        static_cast<std::int64_t>(m_time.seconds().value()) * 1000 +
                                        static_cast<std::int64_t>(m_time.millseconds().value());
    
        const std::int64_t Time_offset_ms = static_cast<std::int64_t>(hour_offset) * 3600000 +
                                           static_cast<std::int64_t>(minute_offset) * 60000 +
                                           static_cast<std::int64_t>(second_offset) * 1000 +
                                           static_cast<std::int64_t>(millisecond_offset);
        
        std::int64_t new_ms = current_ms + Time_offset_ms;
        constexpr std::int64_t DAY_MS = 86400000;
        std::int64_t day_overflow = 0;

        if (new_ms >= DAY_MS) {
            day_overflow = new_ms / DAY_MS;
            new_ms %= DAY_MS;
        } else if (new_ms < 0) {
            day_overflow = -(-new_ms + DAY_MS - 1) / DAY_MS; 
            new_ms = ((new_ms % DAY_MS) + DAY_MS) % DAY_MS;
        }
        
        const hour new_hours = new_ms / 3600000;
        new_ms %= 3600000;
        const minute new_minutes = new_ms / 60000;
        new_ms %= 60000;
        const second new_seconds = new_ms / 1000;
        const millisecond new_milliseconds = new_ms % 1000;
        
        const std::int64_t total_day_offset = static_cast<std::int64_t>(day_offset) + 
                                             static_cast<std::int64_t>(week_offset) * 7 + 
                                             day_overflow;
        
        const Date new_date = m_date.offset_date(year_offset, month_offset, 0, total_day_offset);
        const Time new_Time = Time(new_hours, new_minutes, new_seconds, new_milliseconds, m_time.utc_offset_hours(), m_time.utc_offset_minutes());
        return DateTime(new_date, new_Time);
    }

    template <
        typename Y, typename M, typename W, typename D, typename H, typename MIN, typename S, typename MS,
        typename = typename std::enable_if<
            std::is_integral<Y>::value && std::is_integral<M>::value &&
            std::is_integral<W>::value && std::is_integral<D>::value &&
            std::is_integral<H>::value && std::is_integral<MIN>::value &&
            std::is_integral<S>::value && std::is_integral<MS>::value
        >::type
    >
    OPTIONAL_CPP14_CONSTEXPR DateTime& offset_this_date_time(const Y year_offset, const M month_offset, 
                                                           const W week_offset, const D day_offset, 
                                                           const H hour_offset, const MIN minute_offset, 
                                                           const S second_offset, const MS millisecond_offset) noexcept {
        *this = offset_date_time(year_offset, month_offset, week_offset, day_offset, hour_offset, minute_offset, second_offset, millisecond_offset);
        return *this;
    }

public:
    template <typename Y, typename = typename std::enable_if<std::is_integral<Y>::value>::type>
    constexpr DateTime add_years(const Y years) const noexcept { return offset_date_time(years, 0, 0, 0, 0, 0, 0, 0); }

    template <typename M, typename = typename std::enable_if<std::is_integral<M>::value>::type>
    constexpr DateTime add_months(const M months) const noexcept { return offset_date_time(0, months, 0, 0, 0, 0, 0, 0); }

    template <typename W, typename = typename std::enable_if<std::is_integral<W>::value>::type>
    constexpr DateTime add_weeks(const W weeks) const noexcept { return offset_date_time(0, 0, weeks, 0, 0, 0, 0, 0); }

    template <typename D, typename = typename std::enable_if<std::is_integral<D>::value>::type>
    constexpr DateTime add_days(const D days) const noexcept { return offset_date_time(0, 0, 0, days, 0, 0, 0, 0); }

    template <typename H, typename = typename std::enable_if<std::is_integral<H>::value>::type>
    constexpr DateTime add_hours(const H hours) const noexcept { return offset_date_time(0, 0, 0, 0, hours, 0, 0, 0); }

    template <typename MIN, typename = typename std::enable_if<std::is_integral<MIN>::value>::type>
    constexpr DateTime add_minutes(const MIN minutes) const noexcept { return offset_date_time(0, 0, 0, 0, 0, minutes, 0, 0); }

    template <typename S, typename = typename std::enable_if<std::is_integral<S>::value>::type>
    constexpr DateTime add_seconds(const S seconds) const noexcept { return offset_date_time(0, 0, 0, 0, 0, 0, seconds, 0); }

    template <typename MS, typename = typename std::enable_if<std::is_integral<MS>::value>::type>
    constexpr DateTime add_milliseconds(const MS milliseconds) const noexcept { return offset_date_time(0, 0, 0, 0, 0, 0, 0, milliseconds); }

    template <typename Y, typename = typename std::enable_if<std::is_integral<Y>::value>::type>
    constexpr DateTime subtract_years(const Y years) const noexcept { return offset_date_time(-years, 0, 0, 0, 0, 0, 0, 0); }

    template <typename M, typename = typename std::enable_if<std::is_integral<M>::value>::type>
    constexpr DateTime subtract_months(const M months) const noexcept { return offset_date_time(0, -months, 0, 0, 0, 0, 0, 0); }

    template <typename W, typename = typename std::enable_if<std::is_integral<W>::value>::type>
    constexpr DateTime subtract_weeks(const W weeks) const noexcept { return offset_date_time(0, 0, -weeks, 0, 0, 0, 0, 0); }

    template <typename D, typename = typename std::enable_if<std::is_integral<D>::value>::type>
    constexpr DateTime subtract_days(const D days) const noexcept { return offset_date_time(0, 0, 0, -days, 0, 0, 0, 0); }

    template <typename H, typename = typename std::enable_if<std::is_integral<H>::value>::type>
    constexpr DateTime subtract_hours(const H hours) const noexcept { return offset_date_time(0, 0, 0, 0, -hours, 0, 0, 0); }

    template <typename MIN, typename = typename std::enable_if<std::is_integral<MIN>::value>::type>
    constexpr DateTime subtract_minutes(const MIN minutes) const noexcept { return offset_date_time(0, 0, 0, 0, 0, -minutes, 0, 0); }

    template <typename S, typename = typename std::enable_if<std::is_integral<S>::value>::type>
    constexpr DateTime subtract_seconds(const S seconds) const noexcept { return offset_date_time(0, 0, 0, 0, 0, 0, -seconds, 0); }

    template <typename MS, typename = typename std::enable_if<std::is_integral<MS>::value>::type>
    constexpr DateTime subtract_milliseconds(const MS milliseconds) const noexcept { return offset_date_time(0, 0, 0, 0, 0, 0, 0, -milliseconds); }
};

} // namespace time
} // namespace fizmo

#endif // DATE_Time_CLASSES_HPP