#ifndef TIME_POINT_CLASS_HPP
#define TIME_POINT_CLASS_HPP

#include "util_functions.hpp"

namespace fizmo {
namespace time {

class TimePoint {
private:
    static constexpr DateTime EPOCH = DateTime(Date(1582, months::October, 15));

    nanosecond m_ns;
    microsecond m_us;
    millisecond m_ms;
    centisecond m_cs;
    decisecond m_ds;
    second m_s;
    minute m_m;
    hour m_h;
    day m_d;
    year m_y;
    decade m_de;
    century m_c;
    millennium m_mil;
    std::int8_t m_utc_hour_offset;
    std::int8_t m_utc_minute_offset;

private:
    struct TimeValues {
        millennium mil;
        century c;
        decade de;
        year y;
        day d;
        hour h;
        minute m;
        second s;
        decisecond ds;
        centisecond cs;
        millisecond ms;
        microsecond us;
        nanosecond ns;
        
        constexpr TimeValues() noexcept 
            : mil(0), c(0), de(0), y(0), d(0), h(0), m(0), s(0),
              ds(0), cs(0), ms(0), us(0), ns(0) {}
    };

    static constexpr TimeValues extract_values(TimeValues values) noexcept {
        return values;
    }

    template<typename T, typename... Rest>
    static constexpr TimeValues extract_values(TimeValues values, T first, Rest... rest) noexcept {
        return extract_values(add_value_to_struct(values, first), rest...);
    }

    static constexpr TimeValues add_value_to_struct(TimeValues values, millennium val) noexcept {
        values.mil = millennium(values.mil.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, century val) noexcept {
        values.c = century(values.c.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, decade val) noexcept {
        values.de = decade(values.de.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, year val) noexcept {
        values.y = year(values.y.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, day val) noexcept {
        values.d = day(values.d.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, hour val) noexcept {
        values.h = hour(values.h.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, minute val) noexcept {
        values.m = minute(values.m.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, second val) noexcept {
        values.s = second(values.s.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, decisecond val) noexcept {
        values.ds = decisecond(values.ds.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, centisecond val) noexcept {
        values.cs = centisecond(values.cs.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, millisecond val) noexcept {
        values.ms = millisecond(values.ms.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, microsecond val) noexcept {
        values.us = microsecond(values.us.value() + val.value());
        return values;
    }
    
    static constexpr TimeValues add_value_to_struct(TimeValues values, nanosecond val) noexcept {
        values.ns = nanosecond(values.ns.value() + val.value());
        return values;
    }

    constexpr TimePoint(const TimeValues& values) noexcept
        : TimePoint(normalize_values(values.mil, 
                                     values.c, values.de, values.y, 
                                     values.d, values.h, values.m, values.s,
                                     values.ds, values.cs, values.ms, values.us, values.ns
                                )) {}

private:
    constexpr TimePoint(const TimeValues& values, const std::int8_t utc_hour_offset, const std::int8_t utc_minute_offset) noexcept
        : TimePoint(normalize_values(values.mil, 
                                     values.c, values.de, values.y, 
                                     values.d, values.h, values.m, values.s,
                                     values.ds, values.cs, values.ms, values.us, values.ns
                                ))
    {
        m_utc_hour_offset = fizmo::clamp(utc_hour_offset, static_cast<std::int8_t>(-12), static_cast<std::int8_t>(14));
        m_utc_minute_offset = fizmo::clamp(utc_minute_offset, static_cast<std::int8_t>(-59), static_cast<std::int8_t>(59));
    }

public:
    constexpr TimePoint() noexcept 
        : m_mil(0), m_c(0), m_de(0),
          m_y(0), m_d(0),
          m_h(0), m_m(0), m_s(0),
          m_ds(0), m_cs(0), m_ms(0), m_us(0), m_ns(0),
          m_utc_hour_offset(0), m_utc_minute_offset(0)
    {} 

    constexpr TimePoint(const TimePoint& dt) noexcept 
        : m_mil(dt.m_mil), m_c(dt.m_c), m_de(dt.m_de),
          m_y(dt.m_y), m_d(dt.m_d),
          m_h(dt.m_h), m_m(dt.m_m), m_s(dt.m_s),
          m_ds(dt.m_ds), m_cs(dt.m_cs), m_ms(dt.m_ms), m_us(dt.m_us), m_ns(dt.m_ns),
          m_utc_hour_offset(dt.m_utc_hour_offset), m_utc_minute_offset(dt.m_utc_minute_offset)
    {}

    constexpr TimePoint(TimePoint&& dt) noexcept 
        : m_mil(std::move(dt.m_mil)), m_c(std::move(dt.m_c)), m_de(std::move(dt.m_de)),
          m_y(std::move(dt.m_y)), m_d(std::move(dt.m_d)),
          m_h(std::move(dt.m_h)), m_m(std::move(dt.m_m)), m_s(std::move(dt.m_s)),
          m_ds(std::move(dt.m_ds)), m_cs(std::move(dt.m_cs)), m_ms(std::move(dt.m_ms)), m_us(std::move(dt.m_us)), m_ns(std::move(dt.m_ns)),
          m_utc_hour_offset(dt.m_utc_hour_offset), m_utc_minute_offset(dt.m_utc_minute_offset)
    {
        dt.m_utc_hour_offset = 0;
        dt.m_utc_minute_offset = 0;
    }

    explicit constexpr TimePoint(const Date& date) noexcept 
        : TimePoint(date < EPOCH.get_date() ? TimePoint() : from_date_impl(date))
    {}
    
    explicit constexpr TimePoint(const Time& time) noexcept 
        : TimePoint(
            hour(time.hours().value()),
            minute(time.minutes().value()),
            second(time.seconds().value()),
            millisecond(time.millseconds().value())
        )
    {}
    
    explicit constexpr TimePoint(const DateTime& datetime) noexcept
        : TimePoint(datetime < EPOCH ? TimePoint() : from_datetime_impl(datetime))
    {}

    template<typename... Args, typename = typename std::enable_if<all_are_time_units_v<Args...>>::type>
    constexpr TimePoint(Args... args) noexcept : TimePoint(extract_values(TimeValues{}, args...)) {}

    template<typename... Args, typename = typename std::enable_if<all_are_time_units_v<Args...>>::type>
    constexpr TimePoint(Args... args, const std::int8_t utc_hour_offset, const std::int8_t utc_minute_offset = 0) noexcept
        : TimePoint(extract_values(TimeValues{}, args...), utc_hour_offset, utc_minute_offset)
    {}

    constexpr TimePoint(const CompleteDuration& d, const std::int8_t utc_hour_offset = 0, const std::int8_t utc_minute_offset = 0) noexcept 
        : m_mil(d.millennia()),
          m_c(d.centuries()),
          m_de(d.decades()),
          m_y(d.years()),
          m_d(d.days()),
          m_h(d.hours()),
          m_m(d.minutes()),
          m_s(d.seconds()),
          m_ds(d.deciseconds()),
          m_cs(d.centiseconds()),
          m_ms(d.milliseconds()),
          m_us(d.microseconds()),
          m_ns(d.nanoseconds()),
          m_utc_hour_offset(fizmo::clamp(utc_hour_offset, static_cast<std::int8_t>(-12), static_cast<std::int8_t>(14))),
          m_utc_minute_offset(fizmo::clamp(utc_minute_offset, static_cast<std::int8_t>(-59), static_cast<std::int8_t>(59)))
    {}

    constexpr TimePoint(CompleteDuration&& d, const std::int8_t utc_hour_offset = 0, const std::int8_t utc_minute_offset = 0) noexcept 
        : m_mil(std::move(d.millennia())),
          m_c(std::move(d.centuries())),
          m_de(std::move(d.decades())),
          m_y(std::move(d.years())),
          m_d(std::move(d.days())),
          m_h(std::move(d.hours())),
          m_m(std::move(d.minutes())),
          m_s(std::move(d.seconds())),
          m_ds(std::move(d.deciseconds())),
          m_cs(std::move(d.centiseconds())),
          m_ms(std::move(d.milliseconds())),
          m_us(std::move(d.microseconds())),
          m_ns(std::move(d.nanoseconds())),
          m_utc_hour_offset(fizmo::clamp(utc_hour_offset, static_cast<std::int8_t>(-12), static_cast<std::int8_t>(14))),
          m_utc_minute_offset(fizmo::clamp(utc_minute_offset, static_cast<std::int8_t>(-59), static_cast<std::int8_t>(59)))
    {}

    constexpr TimePoint(DURATION_PARAM d, const std::int8_t utc_hour_offset = 0, const std::int8_t utc_minute_offset = 0) noexcept : TimePoint(d.to_complete_duration(), utc_hour_offset, utc_minute_offset) {} 

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(const TimePoint& dt) noexcept {
        if (this != &dt) {
            m_mil = dt.m_mil;
            m_c = dt.m_c;
            m_de = dt.m_de;
            m_y = dt.m_y;
            m_d = dt.m_d;
            m_h = dt.m_h;
            m_m = dt.m_m;
            m_s = dt.m_s;
            m_ds = dt.m_ds;
            m_cs = dt.m_cs;
            m_ms = dt.m_ms;
            m_us = dt.m_us;
            m_ns = dt.m_ns;
            m_utc_hour_offset = dt.m_utc_hour_offset;
            m_utc_minute_offset = dt.m_utc_minute_offset;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(TimePoint&& dt) noexcept {
        if (this != &dt) {
            m_mil = std::move(dt.m_mil);
            m_c = std::move(dt.m_c);
            m_de = std::move(dt.m_de);
            m_y = std::move(dt.m_y);
            m_d = std::move(dt.m_d);
            m_h = std::move(dt.m_h);
            m_m = std::move(dt.m_m);
            m_s = std::move(dt.m_s);
            m_ds = std::move(dt.m_ds);
            m_cs = std::move(dt.m_cs);
            m_ms = std::move(dt.m_ms);
            m_us = std::move(dt.m_us);
            m_ns = std::move(dt.m_ns);
            m_utc_hour_offset = dt.m_utc_hour_offset;
            m_utc_minute_offset = dt.m_utc_minute_offset;
            dt.m_utc_hour_offset = 0;
            dt.m_utc_minute_offset = 0;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(const CompleteDuration& d) noexcept {
        *this = TimePoint(d);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(CompleteDuration&& d) noexcept {
        *this = TimePoint(std::move(d));
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(DURATION_PARAM d) noexcept {
        *this = TimePoint(d);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(Duration&& d) noexcept {
        *this = TimePoint(std::move(d));
        return *this;
    }

public:
    constexpr TimePoint(const nanosecond ns) noexcept : TimePoint(ns, microsecond(0)) {}
    constexpr TimePoint(const microsecond us) noexcept : TimePoint(us, nanosecond(0)) {}
    constexpr TimePoint(const millisecond ms) noexcept : TimePoint(ms, nanosecond(0)) {}
    constexpr TimePoint(const centisecond cs) noexcept : TimePoint(cs, nanosecond(0)) {}
    constexpr TimePoint(const decisecond ds) noexcept : TimePoint(ds, nanosecond(0)) {}
    constexpr TimePoint(const second s) noexcept : TimePoint(s, nanosecond(0)) {}
    constexpr TimePoint(const minute m) noexcept : TimePoint(m, nanosecond(0)) {}
    constexpr TimePoint(const hour h) noexcept : TimePoint(h, nanosecond(0)) {}
    constexpr TimePoint(const day d) noexcept : TimePoint(d, nanosecond(0)) {}
    constexpr TimePoint(const year y) noexcept : TimePoint(y, nanosecond(0)) {}
    constexpr TimePoint(const decade d) noexcept : TimePoint(d, nanosecond(0)) {}
    constexpr TimePoint(const century c) noexcept : TimePoint(c, nanosecond(0)) {}
    constexpr TimePoint(const millennium m) noexcept : TimePoint(m, nanosecond(0)) {}

    constexpr TimePoint(const nanosecond ns, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(ns), utc_hour, utc_minute) {}
    constexpr TimePoint(const microsecond us, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(us), utc_hour, utc_minute) {}
    constexpr TimePoint(const millisecond ms, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(ms), utc_hour, utc_minute) {}
    constexpr TimePoint(const centisecond cs, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(cs), utc_hour, utc_minute) {}
    constexpr TimePoint(const decisecond ds, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(ds), utc_hour, utc_minute) {}
    constexpr TimePoint(const second s, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(s), utc_hour, utc_minute) {}
    constexpr TimePoint(const minute m, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(m), utc_hour, utc_minute) {}
    constexpr TimePoint(const hour h, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(h), utc_hour, utc_minute) {}
    constexpr TimePoint(const day d, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(d), utc_hour, utc_minute) {}
    constexpr TimePoint(const year y, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(y), utc_hour, utc_minute) {}
    constexpr TimePoint(const decade d, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(d), utc_hour, utc_minute) {}
    constexpr TimePoint(const century c, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(c), utc_hour, utc_minute) {}
    constexpr TimePoint(const millennium m, const std::uint8_t utc_hour, const std::uint8_t utc_minute = 0) noexcept : TimePoint(CompleteDuration(m), utc_hour, utc_minute) {}

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator=(const T v) noexcept {
        const std::uint8_t saved_off_h = m_utc_hour_offset;
        const std::uint8_t saved_off_m = m_utc_minute_offset;
        *this = TimePoint(v);
        m_utc_hour_offset = saved_off_h;
        m_utc_minute_offset = saved_off_m;
        return *this;
    }

public:
    constexpr bool operator==(const TimePoint& dt) const noexcept {
        const TimePoint& p1 = this->to_utc();
        const TimePoint& p2 = dt.to_utc();

        return (p1.m_mil == p2.m_mil &&
               p1.m_c == p2.m_c &&
               p1.m_de == p2.m_de &&
               p1.m_y == p2.m_y && 
               p1.m_d == p2.m_d &&
               p1.m_h == p2.m_h &&
               p1.m_m == p2.m_m && 
               p1.m_s == p2.m_s &&
               p1.m_ds == p2.m_ds &&
               p1.m_cs == p2.m_cs &&
               p1.m_ms == p2.m_ms &&
               p1.m_us == p2.m_us &&
               p1.m_ns == p2.m_ns
        );
    }

    constexpr bool operator!=(const TimePoint& dt) const noexcept { return !(*this == dt); }

    constexpr bool operator<(const TimePoint& dt) const noexcept {
        const TimePoint& p1 = this->to_utc();
        const TimePoint& p2 = dt.to_utc();

        if (p1.m_mil != p2.m_mil) return m_mil < p2.m_mil;
        if (p1.m_c != p2.m_c) return m_c < p2.m_c;
        if (p1.m_de != p2.m_de) return m_de < p2.m_de;
        if (p1.m_y != p2.m_y) return m_y < p2.m_y;
        if (p1.m_d != p2.m_d) return m_d < p2.m_d;
        if (p1.m_h != p2.m_h) return m_h < p2.m_h;
        if (p1.m_m != p2.m_m) return m_m < p2.m_m;
        if (p1.m_s != p2.m_s) return m_s < p2.m_s;
        if (p1.m_ds != p2.m_ds) return m_ds < p2.m_ds;
        if (p1.m_cs != p2.m_cs) return m_cs < p2.m_cs;
        if (p1.m_ms != p2.m_ms) return m_ms < p2.m_ms;
        if (p1.m_us != p2.m_us) return m_us < p2.m_us;
        return p1.m_ns < p2.m_ns;
    }

    constexpr bool operator<=(const TimePoint& other) const noexcept { return *this < other || *this == other; }
    constexpr bool operator>(const TimePoint& other) const noexcept { return !(*this <= other); }
    constexpr bool operator>=(const TimePoint& other) const noexcept { return !(*this < other); }

public:
    constexpr millennium millennia() const noexcept { return m_mil; }
    constexpr century centuries() const noexcept { return m_c; }
    constexpr decade decades() const noexcept { return m_de; }
    constexpr year years() const noexcept { return m_y; }
    constexpr day days() const noexcept { return m_d; }
    constexpr hour hours() const noexcept { return m_h; }
    constexpr minute minutes() const noexcept { return m_m; }
    constexpr second seconds() const noexcept { return m_s; }
    constexpr decisecond deciseconds() const noexcept { return m_ds; }
    constexpr centisecond centiseconds() const noexcept { return m_cs; }
    constexpr millisecond milliseconds() const noexcept { return m_ms; }
    constexpr microsecond microseconds() const noexcept { return m_us; }
    constexpr nanosecond nanoseconds() const noexcept { return m_ns; }

    constexpr millennium& millennia() noexcept { return m_mil; }
    constexpr century& centuries() noexcept { return m_c; }
    constexpr decade& decades() noexcept { return m_de; }
    constexpr year& years() noexcept { return m_y; }
    constexpr day& days() noexcept { return m_d; }
    constexpr hour& hours() noexcept { return m_h; }
    constexpr minute& minutes() noexcept { return m_m; }
    constexpr second& seconds() noexcept { return m_s; }
    constexpr decisecond& deciseconds() noexcept { return m_ds; }
    constexpr centisecond& centiseconds() noexcept { return m_cs; }
    constexpr millisecond& milliseconds() noexcept { return m_ms; }
    constexpr microsecond& microseconds() noexcept { return m_us; }
    constexpr nanosecond& nanoseconds() noexcept { return m_ns; }

    constexpr std::int8_t utc_hour_offset() const noexcept { return m_utc_hour_offset; }
    constexpr std::int8_t& utc_hour_offset() noexcept { return m_utc_hour_offset; }
    constexpr std::int8_t utc_minute_offset() const noexcept { return m_utc_minute_offset; }
    constexpr std::int8_t& utc_minute_offset() noexcept { return m_utc_minute_offset; }

public:
    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_millennia(const millennium m) noexcept {
        m_mil = m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_centuries(const century c) noexcept {
        m_c = c;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_decades(const decade d) noexcept {
        m_de = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_years(const year y) noexcept {
        m_y = y;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_days(const day d) noexcept {
        m_d = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_hours(const hour h) noexcept {
        m_h = h;
        return normalize();
    }
    
    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_minutes(const minute m) noexcept {
        m_m = m;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_seconds(const second s) noexcept {
        m_s = s;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_deciseconds(const decisecond ds) noexcept {
        m_ds = ds;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_centiseconds(const centisecond cs) noexcept {
        m_cs = cs;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_milliseconds(const millisecond ms) noexcept {
        m_ms = ms;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_microseconds(const microsecond us) noexcept {
        m_us = us;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_nanoseconds(const nanosecond ns) noexcept {
        m_ns = ns;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_values(
        const millennium mil, const century c, const decade de, const year y, const day d,
        const hour h, const minute m, const second s,
        const decisecond ds, const centisecond cs,
        const millisecond ms, const microsecond us, const nanosecond ns
    ) noexcept {
        *this = normalize_values(mil, c, de, y, d, h, m, s, ds, cs, ms, us, ns);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& normalize() noexcept {
        *this = normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& set_utc_offset(const std::int8_t hour_offset, const std::int8_t minute_offset = 0) noexcept {
        m_utc_hour_offset = fizmo::clamp(hour_offset, static_cast<std::int8_t>(-12), static_cast<std::int8_t>(14));
        m_utc_minute_offset = fizmo::clamp(minute_offset, static_cast<std::int8_t>(-59), static_cast<std::int8_t>(59));
        return *this;
    }

public:
    constexpr double total_millennia() const noexcept {
        double m = 0.0;
        m += static_cast<double>(m_mil.value());
        m += static_cast<double>(m_c.value()) / 10.0;
        m += static_cast<double>(m_de.value()) / 100.0;
        m += static_cast<double>(m_y.value()) / 1000.0;
        m += static_cast<double>(m_d.value()) / 365000.0;
        m += static_cast<double>(m_h.value()) / (365000.0 * 24.0);
        m += static_cast<double>(m_m.value()) / (365000.0 * 24.0 * 60.0);
        m += static_cast<double>(m_s.value()) / (365000.0 * 24.0 * 3600.0);
        m += static_cast<double>(m_ds.value()) / (365000.0 * 24.0 * 36000.0);
        m += static_cast<double>(m_cs.value()) / (365000.0 * 24.0 * 360000.0);
        m += static_cast<double>(m_ms.value()) / (365000.0 * 24.0 * 3.6e6);
        m += static_cast<double>(m_us.value()) / (365000.0 * 24.0 * 3.6e9);
        m += static_cast<double>(m_ns.value()) / (365000.0 * 24.0 * 3.6e12);
        return m;
    }

    constexpr double total_centuries() const noexcept {
        double c = 0.0;
        c += static_cast<double>(m_mil.value()) * 10.0;
        c += static_cast<double>(m_c.value());
        c += static_cast<double>(m_de.value()) / 10.0;
        c += static_cast<double>(m_y.value()) / 100.0;
        c += static_cast<double>(m_d.value()) / 36500.0;
        c += static_cast<double>(m_h.value()) / (36500.0 * 24.0);
        c += static_cast<double>(m_m.value()) / (36500.0 * 24.0 * 60.0);
        c += static_cast<double>(m_s.value()) / (36500.0 * 24.0 * 3600.0);
        c += static_cast<double>(m_ds.value()) / (36500.0 * 24.0 * 36000.0);
        c += static_cast<double>(m_cs.value()) / (36500.0 * 24.0 * 360000.0);
        c += static_cast<double>(m_ms.value()) / (36500.0 * 24.0 * 3.6e6);
        c += static_cast<double>(m_us.value()) / (36500.0 * 24.0 * 3.6e9);
        c += static_cast<double>(m_ns.value()) / (36500.0 * 24.0 * 3.6e12);
        return c;
    }

    constexpr double total_decades() const noexcept {
        double d = 0.0;
        d += static_cast<double>(m_mil.value()) * 100.0;
        d += static_cast<double>(m_c.value()) * 10.0;
        d += static_cast<double>(m_de.value());
        d += static_cast<double>(m_y.value()) / 10.0;
        d += static_cast<double>(m_d.value()) / 3650.0;
        d += static_cast<double>(m_h.value()) / (3650.0 * 24.0);
        d += static_cast<double>(m_m.value()) / (3650.0 * 24.0 * 60.0);
        d += static_cast<double>(m_s.value()) / (3650.0 * 24.0 * 3600.0);
        d += static_cast<double>(m_ds.value()) / (3650.0 * 24.0 * 36000.0);
        d += static_cast<double>(m_cs.value()) / (3650.0 * 24.0 * 360000.0);
        d += static_cast<double>(m_ms.value()) / (3650.0 * 24.0 * 3.6e6);
        d += static_cast<double>(m_us.value()) / (3650.0 * 24.0 * 3.6e9);
        d += static_cast<double>(m_ns.value()) / (3650.0 * 24.0 * 3.6e12);
        return d;
    }

    constexpr double total_years() const noexcept {
        double y = 0.0;
        y += static_cast<double>(m_mil.value()) * 1000.0;
        y += static_cast<double>(m_c.value()) * 100.0;
        y += static_cast<double>(m_de.value()) * 10.0;
        y += static_cast<double>(m_y.value());
        y += static_cast<double>(m_d.value()) / 365.0;
        y += static_cast<double>(m_h.value()) / (365.0 * 24.0);
        y += static_cast<double>(m_m.value()) / (365.0 * 24.0 * 60.0);
        y += static_cast<double>(m_s.value()) / (365.0 * 24.0 * 3600.0);
        y += static_cast<double>(m_ds.value()) / (365.0 * 24.0 * 36000.0);
        y += static_cast<double>(m_cs.value()) / (365.0 * 24.0 * 360000.0);
        y += static_cast<double>(m_ms.value()) / (365.0 * 24.0 * 3.6e6);
        y += static_cast<double>(m_us.value()) / (365.0 * 24.0 * 3.6e9);
        y += static_cast<double>(m_ns.value()) / (365.0 * 24.0 * 3.6e12);
        return y;
    }

    constexpr double total_days() const noexcept {
        double d = 0.0;
        d += static_cast<double>(m_mil.value()) * 365000.0;
        d += static_cast<double>(m_c.value()) * 36500.0;
        d += static_cast<double>(m_de.value()) * 3650.0;
        d += static_cast<double>(m_y.value()) * 365.0;
        d += static_cast<double>(m_d.value());
        d += static_cast<double>(m_h.value()) / 24.0;
        d += static_cast<double>(m_m.value()) / (24.0 * 60.0);
        d += static_cast<double>(m_s.value()) / (24.0 * 3600.0);
        d += static_cast<double>(m_ds.value()) / (24.0 * 36000.0);
        d += static_cast<double>(m_cs.value()) / (24.0 * 360000.0);
        d += static_cast<double>(m_ms.value()) / (24.0 * 3.6e6);
        d += static_cast<double>(m_us.value()) / (24.0 * 3.6e9);
        d += static_cast<double>(m_ns.value()) / (24.0 * 3.6e12);
        return d;
    }

    constexpr double total_hours() const noexcept {
        double h = 0.0;
        h += static_cast<double>(m_mil.value()) * 365000.0 * 24.0;
        h += static_cast<double>(m_c.value()) * 36500.0 * 24.0;
        h += static_cast<double>(m_de.value()) * 3650.0 * 24.0;
        h += static_cast<double>(m_y.value()) * 365.0 * 24.0;
        h += static_cast<double>(m_d.value()) * 24.0;
        h += static_cast<double>(m_h.value());
        h += static_cast<double>(m_m.value()) / 60.0;
        h += static_cast<double>(m_s.value()) / 3600.0;
        h += static_cast<double>(m_ds.value()) / 36000.0;
        h += static_cast<double>(m_cs.value()) / 360000.0;
        h += static_cast<double>(m_ms.value()) / 3.6e6;
        h += static_cast<double>(m_us.value()) / 3.6e9;
        h += static_cast<double>(m_ns.value()) / 3.6e12;
        return h;
    }

    constexpr double total_minutes() const noexcept {
        double m = 0.0;
        m += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 60.0;
        m += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 60.0;
        m += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 60.0;
        m += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 60.0;
        m += static_cast<double>(m_d.value()) * 24.0 * 60.0;
        m += static_cast<double>(m_h.value()) * 60.0;
        m += static_cast<double>(m_m.value());
        m += static_cast<double>(m_s.value()) / 60.0;
        m += static_cast<double>(m_ds.value()) / 600.0;
        m += static_cast<double>(m_cs.value()) / 6000.0;
        m += static_cast<double>(m_ms.value()) / 60000.0;
        m += static_cast<double>(m_us.value()) / 6e7;
        m += static_cast<double>(m_ns.value()) / 6e10;
        return m;
    }

    constexpr double total_seconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_d.value()) * 24.0 * 3600.0;
        s += static_cast<double>(m_h.value()) * 3600.0;
        s += static_cast<double>(m_m.value()) * 60.0;
        s += static_cast<double>(m_s.value());
        s += static_cast<double>(m_ds.value()) / 10.0;
        s += static_cast<double>(m_cs.value()) / 100.0;
        s += static_cast<double>(m_ms.value()) / 1000.0;
        s += static_cast<double>(m_us.value()) / 1e6;
        s += static_cast<double>(m_ns.value()) / 1e9;
        return s;
    }

    constexpr double total_deciseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_d.value()) * 24.0 * 36000.0;
        s += static_cast<double>(m_h.value()) * 36000.0;
        s += static_cast<double>(m_m.value()) * 600.0;
        s += static_cast<double>(m_s.value()) * 10.0;
        s += static_cast<double>(m_ds.value());
        s += static_cast<double>(m_cs.value()) / 10.0;
        s += static_cast<double>(m_ms.value()) / 100.0;
        s += static_cast<double>(m_us.value()) / 100000.0;
        s += static_cast<double>(m_ns.value()) / 1e8;
        return s;
    }

    constexpr double total_centiseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_d.value()) * 24.0 * 360000.0;
        s += static_cast<double>(m_h.value()) * 360000.0;
        s += static_cast<double>(m_m.value()) * 6000.0;
        s += static_cast<double>(m_s.value()) * 100.0;
        s += static_cast<double>(m_ds.value()) * 10;
        s += static_cast<double>(m_cs.value());
        s += static_cast<double>(m_ms.value()) / 10.0;
        s += static_cast<double>(m_us.value()) / 10000.0;
        s += static_cast<double>(m_ns.value()) / 1e7;
        return s;
    }

    constexpr double total_milliseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_d.value()) * 24.0 * 3.6e6;
        s += static_cast<double>(m_h.value()) * 3.6e6;
        s += static_cast<double>(m_m.value()) * 60000.0;
        s += static_cast<double>(m_s.value()) * 1000.0;
        s += static_cast<double>(m_ds.value()) * 100.0;
        s += static_cast<double>(m_cs.value()) * 10.0;
        s += static_cast<double>(m_ms.value());
        s += static_cast<double>(m_us.value()) / 1000.0;
        s += static_cast<double>(m_ns.value()) / 1e6;
        return s;
    }

    constexpr double total_microseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_d.value()) * 24.0 * 3.6e9;
        s += static_cast<double>(m_h.value()) * 3.6e9;
        s += static_cast<double>(m_m.value()) * 6e7;
        s += static_cast<double>(m_s.value()) * 1e6;
        s += static_cast<double>(m_ds.value()) * 100000.0;
        s += static_cast<double>(m_cs.value()) * 10000.0;
        s += static_cast<double>(m_ms.value()) * 1000.0;
        s += static_cast<double>(m_us.value());
        s += static_cast<double>(m_ns.value()) / 1000.0;
        return s;
    }

    constexpr double total_nanoseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil.value()) * 365000.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_c.value()) * 36500.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_de.value()) * 3650.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_y.value()) * 365.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_d.value()) * 24.0 * 3.6e12;
        s += static_cast<double>(m_h.value()) * 3.6e12;
        s += static_cast<double>(m_m.value()) * 6e10;
        s += static_cast<double>(m_s.value()) * 1e9;
        s += static_cast<double>(m_ds.value()) * 1e8;
        s += static_cast<double>(m_cs.value()) * 1e7;
        s += static_cast<double>(m_ms.value()) * 1e6;
        s += static_cast<double>(m_us.value()) * 1000.0;
        s += static_cast<double>(m_ns.value());
        return s;
    }

public:
    constexpr weekday get_weekday() const noexcept { return to_date_time().get_weekday(); }
    constexpr month get_month() const noexcept { return to_date_time().get_month(); }
    constexpr day get_day_of_month() const noexcept { return to_date_time().get_day(); }
    constexpr year get_year() const noexcept { return to_date_time().get_year(); }
    constexpr bool is_leap_year() const noexcept { return to_date_time().is_leap_year(); }

public:
    constexpr nanosecond to_nanosecond() const noexcept { return nanosecond(static_cast<std::uint64_t>(total_nanoseconds())); }
    constexpr microsecond to_microsecond() const noexcept { return microsecond(static_cast<std::uint64_t>(total_microseconds())); }
    constexpr millisecond to_millisecond() const noexcept { return millisecond(static_cast<std::uint64_t>(total_milliseconds())); }
    constexpr centisecond to_centisecond() const noexcept { return centisecond(static_cast<std::uint64_t>(total_centiseconds())); }
    constexpr decisecond to_decisecond() const noexcept { return decisecond(static_cast<std::uint64_t>(total_deciseconds())); }
    constexpr second to_second() const noexcept { return second(static_cast<std::uint64_t>(total_seconds())); }
    constexpr minute to_minute() const noexcept { return minute(static_cast<std::uint64_t>(total_minutes())); }
    constexpr hour to_hour() const noexcept { return hour(static_cast<std::uint64_t>(total_hours())); }
    constexpr day to_day() const noexcept { return day(static_cast<std::uint64_t>(total_days())); }
    constexpr year to_year() const noexcept { return year(static_cast<std::uint64_t>(total_years())); }
    constexpr decade to_decade() const noexcept { return decade(static_cast<std::uint64_t>(total_decades())); }
    constexpr century to_century() const noexcept { return century(static_cast<std::uint64_t>(total_centuries())); }
    constexpr millennium to_millennium() const noexcept { return millennium(static_cast<std::uint64_t>(total_millennia())); }

    constexpr CompleteDuration to_duration() const noexcept { 
        return CompleteDuration(
            m_mil, m_c, m_de, m_y,
            m_d, m_h, m_m, m_s,
            m_ds, m_cs, m_ms, m_us, m_ns
        );
    }

    constexpr DateTime to_date_time() const noexcept {
        DateTime result = EPOCH;
        const double total_ms = total_milliseconds();
        const std::uint64_t ms_per_day = 86400000ULL; 
        const std::uint64_t total_days = static_cast<std::uint64_t>(total_ms / ms_per_day);
        const std::uint64_t remaining_ms = static_cast<std::uint64_t>(total_ms) % ms_per_day;
        const std::uint64_t ms_component = remaining_ms % 1000;
        const std::uint64_t total_seconds = remaining_ms / 1000;
        const std::uint64_t sec_component = total_seconds % 60;
        const std::uint64_t total_minutes = total_seconds / 60;
        const std::uint64_t min_component = total_minutes % 60;
        const std::uint64_t hour_component = total_minutes / 60;
        result = result.add_days(static_cast<std::int64_t>(total_days));
        
        result.get_time() = Time(
            hour(hour_component),
            minute(min_component),
            second(sec_component),
            millisecond(ms_component), 
            m_utc_hour_offset,
            m_utc_minute_offset
        );
        
        return result;
    }

    constexpr Date to_date() const noexcept {
        const double total_days_double = total_days();
        const std::uint64_t total_days_int = static_cast<std::uint64_t>(total_days_double);
        const Date epoch_date = EPOCH.get_date();
        return epoch_date.offset_date(0, 0, 0, static_cast<std::int64_t>(total_days_int));
    }

    constexpr Time to_time() const noexcept {
        const double total_ms = total_milliseconds();
        const std::uint64_t ms_per_day = 86400000ULL;
        const std::uint64_t remaining_ms = static_cast<std::uint64_t>(total_ms) % ms_per_day;        
        const std::uint64_t ms_component = remaining_ms % 1000;
        const std::uint64_t total_seconds = remaining_ms / 1000;
        const std::uint64_t sec_component = total_seconds % 60;
        const std::uint64_t total_minutes = total_seconds / 60;
        const std::uint64_t min_component = total_minutes % 60;
        const std::uint64_t hour_component = total_minutes / 60;
        
        return Time(
            hour(hour_component),
            minute(min_component), 
            second(sec_component),
            millisecond(ms_component),
            m_utc_hour_offset,
            m_utc_minute_offset
        );
    }

    constexpr TimePoint to_utc() const noexcept {
        if (m_utc_hour_offset == 0 && m_utc_minute_offset == 0) { return *this; }
        std::int64_t offset_ns = static_cast<std::int64_t>(m_utc_hour_offset) * 3600000LL + static_cast<std::int64_t>(m_utc_minute_offset) * 60000LL;
        double current_ns = total_nanoseconds();
        double utc_ns = current_ns - offset_ns;
        
        if (utc_ns < 0) {
            std::int64_t days_to_subtract = (-static_cast<std::int64_t>(utc_ns) / 86400000000000LL) + 1;
            utc_ns += days_to_subtract * 86400000000000.0;
        }
        
        return TimePoint(CompleteDuration(nanosecond(static_cast<std::uint64_t>(utc_ns))), 0, 0);
    }

    constexpr TimePoint from_utc_to_timezone(const std::int8_t hour_offset, const std::int8_t minute_offset = 0) const noexcept {
        std::int64_t offset_ns = static_cast<std::int64_t>(hour_offset) * 3600000000000LL + static_cast<std::int64_t>(minute_offset) * 60000000000LL;
        const double utc_ns = total_nanoseconds();
        double local_ns = utc_ns + offset_ns;
        
        if (local_ns >= 86400000000000.0) {
            std::int64_t days_to_add = static_cast<std::int64_t>(local_ns) / 86400000000000LL;
            local_ns = local_ns - (days_to_add * 86400000.0);
        }
        
        return TimePoint(CompleteDuration(nanosecond(static_cast<std::uint64_t>(local_ns))), hour_offset, minute_offset);
    }

    constexpr TimePoint to_new_timezone(const std::int8_t hour_offset, const std::int8_t minute_offset = 0) const noexcept {
        return to_utc().from_utc_to_timezone(hour_offset, minute_offset);
    }

public:
    constexpr explicit operator nanosecond() const noexcept { return to_nanosecond(); }
    constexpr explicit operator microsecond() const noexcept { return to_microsecond(); }
    constexpr explicit operator millisecond() const noexcept { return to_millisecond(); }
    constexpr explicit operator centisecond() const noexcept { return to_centisecond(); }
    constexpr explicit operator decisecond() const noexcept { return to_decisecond(); }
    constexpr explicit operator second() const noexcept { return to_second(); }
    constexpr explicit operator minute() const noexcept { return to_minute(); }
    constexpr explicit operator hour() const noexcept { return to_hour(); }
    constexpr explicit operator day() const noexcept { return to_day(); }
    constexpr explicit operator year() const noexcept { return to_year(); }
    constexpr explicit operator decade() const noexcept { return to_decade(); }
    constexpr explicit operator century() const noexcept { return to_century(); }
    constexpr explicit operator millennium() const noexcept { return to_millennium(); }
    constexpr explicit operator CompleteDuration() const noexcept { return to_duration(); }
    constexpr explicit operator Duration() const noexcept { return to_duration().to_duration(); }
    constexpr explicit operator DateTime() const noexcept { return to_date_time(); }
    constexpr explicit operator Date() const noexcept { return to_date(); }
    constexpr explicit operator Time() const noexcept { return to_time(); }
    static constexpr TimePoint epoch() noexcept { return TimePoint(); }
    static constexpr TimePoint windows_epoch() noexcept { return TimePoint(day(6653)); }
    static constexpr TimePoint unix_epoch() noexcept { return TimePoint(day(141427));  }

public:
    constexpr TimePoint operator+(const TimePoint& d) const noexcept {
        return TimePoint(
            millennium(safe_add(m_mil.value(), d.m_mil.value())),
            century(safe_add(m_c.value(), d.m_c.value())),
            decade(safe_add(m_de.value(), d.m_de.value())),
            year(safe_add(m_y.value(), d.m_y.value())),
            day(safe_add(m_d.value(), d.m_d.value())),
            hour(safe_add(m_h.value(), d.m_h.value())),
            minute(safe_add(m_m.value(), d.m_m.value())),
            second(safe_add(m_s.value(), d.m_s.value())),
            decisecond(safe_add(m_ds.value(), d.m_ds.value())),
            centisecond(safe_add(m_cs.value(), d.m_cs.value())),
            millisecond(safe_add(m_ms.value(), d.m_ms.value())),
            microsecond(safe_add(m_us.value(), d.m_us.value())),
            nanosecond(safe_add(m_ns.value(), d.m_ns.value()))
        );
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator+=(const TimePoint& d) noexcept {
        *this = *this + d;
        return *this;
    }

    constexpr TimePoint operator-(const TimePoint& d) const noexcept {
        return TimePoint(
            millennium(safe_subtract(m_mil.value(), d.m_mil.value())),
            century(safe_subtract(m_c.value(), d.m_c.value())),
            decade(safe_subtract(m_de.value(), d.m_de.value())),
            year(safe_subtract(m_y.value(), d.m_y.value())),
            day(safe_subtract(m_d.value(), d.m_d.value())),
            hour(safe_subtract(m_h.value(), d.m_h.value())),
            minute(safe_subtract(m_m.value(), d.m_m.value())),
            second(safe_subtract(m_s.value(), d.m_s.value())),
            decisecond(safe_subtract(m_ds.value(), d.m_ds.value())),
            centisecond(safe_subtract(m_cs.value(), d.m_cs.value())),
            millisecond(safe_subtract(m_ms.value(), d.m_ms.value())),
            microsecond(safe_subtract(m_us.value(), d.m_us.value())),
            nanosecond(safe_subtract(m_ns.value(), d.m_ns.value()))
        );
    }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator-=(const TimePoint& d) noexcept {
        *this = *this - d;
        return *this;
    }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    constexpr TimePoint operator+(const T value) const noexcept { return *this + TimePoint(value); }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    constexpr TimePoint operator-(const T value) const noexcept { return *this - TimePoint(value); }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator+=(const T value) noexcept { *this = *this + TimePoint(value); return *this; }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator-=(const T value) noexcept { *this = *this - TimePoint(value); return *this; }

    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator+=(const CompleteDuration& d) noexcept;
    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator-=(const CompleteDuration& d) noexcept;
    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator+=(DURATION_PARAM d) noexcept;
    OPTIONAL_CPP14_CONSTEXPR TimePoint& operator-=(DURATION_PARAM d) noexcept;

public:
    enum class format_type : std::uint8_t {
        DURATION_ONLY,       // Just the duration components
        DATE_TIME,           // Convert to DateTime and format as date/time
        MIXED,               // Combination of both
        SCIENTIFIC,          // Years with fractional part
        COMPACT_YEARS,       // Just total years as decimal
    };

    enum class duration_format : std::uint8_t {
        VERBOSE,             // "1 millennium, 2 centuries, 3 decades"
        COMPACT,             // "1mil 2c 3dec"
        ABBREVIATED,         // "1k 2c 3d"
        COLON_SEPARATED,     // "1:2:3:4:5:6:7:8:9:10:11:12:13" (mil:c:dec:y:d:h:m:s:ds:cs:ms:us:ns)
        DECIMAL_YEARS,       // "1234.567 years"
        SCIENTIFIC_NOTATION, // "1.234567e+3 years"
        HIERARCHICAL         // "1 millennium + 234 years + 5 days + 12:34:56.789"
    };

    enum class datetime_format : std::uint8_t {
        ISO_8601,            // 2024-01-15T14:30:45.123
        US_LONG,             // January 15, 2024 at 2:30:45 PM
        EUROPEAN,            // 15/01/2024 14:30:45
        DATE_AT_TIME,        // "January 15, 2024 at 2:30 PM"
        TIME_ON_DATE,        // "2:30 PM on January 15, 2024"
        CALENDAR,            // "Monday, January 15th, 2024 at 2:30:45.123 PM"
        CASUAL               // "Jan 15, 2024 2:30pm"
    };

private:
    std::string format_duration_verbose(bool include_zero_units = false) const {
        std::string result;
        bool first = true;

        auto add_unit = [&](std::uint64_t value, const char* singular, const char* plural) {
            if (value != 0 || include_zero_units) {
                if (!first) result += ", ";
                first = false;
                result += std::to_string(value) + " ";
                result += (value == 1) ? singular : plural;
            }
        };

        add_unit(m_mil.value(), "millennium", "millennia");
        add_unit(m_c.value(), "century", "centuries");
        add_unit(m_de.value(), "decade", "decades");
        add_unit(m_y.value(), "year", "years");
        add_unit(m_d.value(), "day", "days");
        add_unit(m_h.value(), "hour", "hours");
        add_unit(m_m.value(), "minute", "minutes");
        add_unit(m_s.value(), "second", "seconds");
        add_unit(m_ds.value(), "decisecond", "deciseconds");
        add_unit(m_cs.value(), "centisecond", "centiseconds");
        add_unit(m_ms.value(), "millisecond", "milliseconds");
        add_unit(m_us.value(), "microsecond", "microseconds");
        add_unit(m_ns.value(), "nanosecond", "nanoseconds");

        return result.empty() ? "0 nanoseconds" : result;
    }

    std::string format_duration_compact() const {
        std::string result;
        bool first = true;

        auto add_unit = [&](std::uint64_t value, const char* unit) {
            if (value != 0) {
                if (!first) result += " ";
                first = false;
                result += std::to_string(value) + unit;
            }
        };

        add_unit(m_mil.value(), "mil");
        add_unit(m_c.value(), "c");
        add_unit(m_de.value(), "dec");
        add_unit(m_y.value(), "y");
        add_unit(m_d.value(), "d");
        add_unit(m_h.value(), "h");
        add_unit(m_m.value(), "m");
        add_unit(m_s.value(), "s");
        add_unit(m_ds.value(), "ds");
        add_unit(m_cs.value(), "cs");
        add_unit(m_ms.value(), "ms");
        add_unit(m_us.value(), "μs");
        add_unit(m_ns.value(), "ns");

        return result.empty() ? "0ns" : result;
    }

    std::string format_duration_abbreviated() const {
        std::string result;
        bool first = true;

        auto add_unit = [&](std::uint64_t value, const char* unit) {
            if (value != 0) {
                if (!first) result += " ";
                first = false;
                result += std::to_string(value) + unit;
            }
        };

        // Use single letter abbreviations where sensible
        add_unit(m_mil.value(), "k");  // k for kilo-year (millennium)
        add_unit(m_c.value(), "C");    // capital C for century
        add_unit(m_de.value(), "D");   // capital D for decade
        add_unit(m_y.value(), "y");
        add_unit(m_d.value(), "d");
        add_unit(m_h.value(), "h");
        add_unit(m_m.value(), "m");
        add_unit(m_s.value(), "s");

        // For sub-second precision, use decimal notation
        if (m_ds.value() || m_cs.value() || m_ms.value() || m_us.value() || m_ns.value()) {
            if (!first) result += " ";
            double subsecond = static_cast<double>(m_ds.value()) * 0.1 +
                              static_cast<double>(m_cs.value()) * 0.01 +
                              static_cast<double>(m_ms.value()) * 0.001 +
                              static_cast<double>(m_us.value()) * 0.000001 +
                              static_cast<double>(m_ns.value()) * 0.000000001;
            
            // Format with appropriate precision
            std::ostringstream oss;
            oss << std::fixed;
            if (subsecond >= 0.1) oss << std::setprecision(1);
            else if (subsecond >= 0.01) oss << std::setprecision(2);
            else if (subsecond >= 0.001) oss << std::setprecision(3);
            else if (subsecond >= 0.000001) oss << std::setprecision(6);
            else oss << std::setprecision(9);
            
            oss << subsecond;
            result += oss.str() + "s";
        }

        return result.empty() ? "0s" : result;
    }

    std::string format_duration_colon_separated() const {
        return std::to_string(m_mil.value()) + ":" +
               std::to_string(m_c.value()) + ":" +
               std::to_string(m_de.value()) + ":" +
               std::to_string(m_y.value()) + ":" +
               std::to_string(m_d.value()) + ":" +
               std::to_string(m_h.value()) + ":" +
               std::to_string(m_m.value()) + ":" +
               std::to_string(m_s.value()) + ":" +
               std::to_string(m_ds.value()) + ":" +
               std::to_string(m_cs.value()) + ":" +
               std::to_string(m_ms.value()) + ":" +
               std::to_string(m_us.value()) + ":" +
               std::to_string(m_ns.value());
    }

    std::string format_duration_decimal_years() const {
        const double years = total_years();
        std::ostringstream oss;
        
        if (years >= 1000000) {
            oss << std::scientific << std::setprecision(6) << years;
        } else if (years >= 1000) {
            oss << std::fixed << std::setprecision(3) << years;
        } else if (years >= 1) {
            oss << std::fixed << std::setprecision(6) << years;
        } else {
            oss << std::scientific << std::setprecision(6) << years;
        }
        
        oss << " years";
        return oss.str();
    }

    std::string format_duration_scientific() const {
        const double years = total_years();
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(9) << years << " years";
        return oss.str();
    }

    std::string format_duration_hierarchical() const {
        std::string result;
        bool has_large_units = false;

        // Large time units
        if (m_mil.value() > 0) {
            result += std::to_string(m_mil.value()) + " millennium";
            if (m_mil.value() > 1) result += "a";
            has_large_units = true;
        }
        
        if (m_c.value() > 0) {
            if (has_large_units) result += " + ";
            result += std::to_string(m_c.value()) + " centur";
            result += (m_c.value() == 1) ? "y" : "ies";
            has_large_units = true;
        }
        
        if (m_de.value() > 0) {
            if (has_large_units) result += " + ";
            result += std::to_string(m_de.value()) + " decade";
            if (m_de.value() > 1) result += "s";
            has_large_units = true;
        }
        
        if (m_y.value() > 0) {
            if (has_large_units) result += " + ";
            result += std::to_string(m_y.value()) + " year";
            if (m_y.value() > 1) result += "s";
            has_large_units = true;
        }
        
        if (m_d.value() > 0) {
            if (has_large_units) result += " + ";
            result += std::to_string(m_d.value()) + " day";
            if (m_d.value() > 1) result += "s";
            has_large_units = true;
        }

        // Time component (H:M:S.fraction)
        bool has_time = (m_h.value() || m_m.value() || m_s.value() || 
                        m_ds.value() || m_cs.value() || m_ms.value() || 
                        m_us.value() || m_ns.value());
        
        if (has_time) {
            if (has_large_units) result += " + ";
            
            // Hours:Minutes:Seconds
            if (m_h.value() < 10) result += "0";
            result += std::to_string(m_h.value()) + ":";
            
            if (m_m.value() < 10) result += "0";
            result += std::to_string(m_m.value()) + ":";
            
            if (m_s.value() < 10) result += "0";
            result += std::to_string(m_s.value());
            
            // Fractional seconds
            if (m_ds.value() || m_cs.value() || m_ms.value() || m_us.value() || m_ns.value()) {
                result += ".";
                result += std::to_string(m_ds.value());
                result += std::to_string(m_cs.value());
                
                // Format milliseconds with leading zeros
                if (m_ms.value() < 100) result += "0";
                if (m_ms.value() < 10) result += "0";
                result += std::to_string(m_ms.value());
                
                // Add microseconds and nanoseconds if present
                if (m_us.value() || m_ns.value()) {
                    if (m_us.value() < 100) result += "0";
                    if (m_us.value() < 10) result += "0";
                    result += std::to_string(m_us.value());
                    
                    if (m_ns.value()) {
                        if (m_ns.value() < 100) result += "0";
                        if (m_ns.value() < 10) result += "0";
                        result += std::to_string(m_ns.value());
                    }
                }
            }
        }
        
        return result.empty() ? "0" : result;
    }

public:
    std::string to_string(format_type type = format_type::DURATION_ONLY, duration_format dur_fmt = duration_format::VERBOSE, datetime_format dt_fmt = datetime_format::US_LONG) const {
        switch (type) {
            case format_type::DURATION_ONLY: {
                switch (dur_fmt) {
                    case duration_format::VERBOSE:
                        return format_duration_verbose();
                    case duration_format::COMPACT:
                        return format_duration_compact();
                    case duration_format::ABBREVIATED:
                        return format_duration_abbreviated();
                    case duration_format::COLON_SEPARATED:
                        return format_duration_colon_separated();
                    case duration_format::DECIMAL_YEARS:
                        return format_duration_decimal_years();
                    case duration_format::SCIENTIFIC_NOTATION:
                        return format_duration_scientific();
                    case duration_format::HIERARCHICAL:
                        return format_duration_hierarchical();
                    default:
                        return format_duration_verbose();
                }
            }
            
            case format_type::DATE_TIME: {
                const DateTime dt = to_date_time();
                switch (dt_fmt) {
                    case datetime_format::ISO_8601:
                        return dt.to_iso_string();
                    case datetime_format::US_LONG:
                        return dt.to_us_long_string();
                    case datetime_format::EUROPEAN:
                        return dt.to_european_string();
                    case datetime_format::DATE_AT_TIME:
                        return dt.to_string(DateTime::format::DATE_AT_TIME);
                    case datetime_format::TIME_ON_DATE:
                        return dt.to_string(DateTime::format::TIME_ON_DATE);
                    case datetime_format::CALENDAR:
                        return dt.to_verbose_string();
                    case datetime_format::CASUAL:
                        return dt.to_casual_string();
                    default:
                        return dt.to_iso_string();
                }
            }
            
            case format_type::MIXED: {
                std::string result;
                
                if (m_mil.value() || m_c.value() || m_de.value()) {
                    TimePoint large_units;
                    large_units.m_mil = m_mil;
                    large_units.m_c = m_c;
                    large_units.m_de = m_de;
                    result += large_units.format_duration_verbose();
                    result += " + ";
                }
                
                TimePoint remainder;
                remainder.m_y = m_y;
                remainder.m_d = m_d;
                remainder.m_h = m_h;
                remainder.m_m = m_m;
                remainder.m_s = m_s;
                remainder.m_ds = m_ds;
                remainder.m_cs = m_cs;
                remainder.m_ms = m_ms;
                remainder.m_us = m_us;
                remainder.m_ns = m_ns;
                
                const DateTime dt = remainder.to_date_time();
                result += dt.to_string(DateTime::format::DATE_AT_TIME);
                
                return result;
            }
            
            case format_type::SCIENTIFIC: {
                return format_duration_scientific();
            }
            
            case format_type::COMPACT_YEARS: {
                return format_duration_decimal_years();
            }
            
            default:
                return format_duration_verbose();
        }
    }

    std::string to_duration_string(duration_format fmt = duration_format::VERBOSE) const { return to_string(format_type::DURATION_ONLY, fmt); }
    std::string to_datetime_string(datetime_format fmt = datetime_format::DATE_AT_TIME) const { return to_string(format_type::DATE_TIME, duration_format::VERBOSE, fmt); }

private:
    static constexpr std::uint64_t safe_add(const std::uint64_t a, const std::uint64_t b) noexcept {
        if (
            a > std::numeric_limits<std::uint64_t>::max() - b ||
            b > std::numeric_limits<std::uint64_t>::max() - a
        ) { return std::numeric_limits<std::uint64_t>::max(); }
        return a + b;
    }

    static constexpr std::uint64_t safe_subtract(const std::uint64_t a, const std::uint64_t b) noexcept { return a < b ? 0ULL : a - b; }

    static constexpr void normalize_unit_pair(std::uint64_t& lower_unit, std::uint64_t& upper_unit, const std::uint64_t conversion_factor) noexcept {
        if (lower_unit < conversion_factor) { return; }
        const std::uint64_t upper_remaining = std::numeric_limits<std::uint64_t>::max() - upper_unit;
        const std::uint64_t max_convertible = lower_unit / conversion_factor;
        
        if (max_convertible <= upper_remaining) {
            upper_unit += max_convertible;
            lower_unit %= conversion_factor;
        } else {
            upper_unit = std::numeric_limits<std::uint64_t>::max();
            lower_unit -= upper_remaining * conversion_factor;
        }
    }

    static constexpr TimePoint normalize_values(
        const millennium mil, const century c, const decade de, const year y,
        const day d, const hour h, const minute m, const second s,
        const decisecond ds, const centisecond cs, const millisecond ms,
        const microsecond us, const nanosecond ns, const bool leap_year = false
    ) noexcept {
        TimePoint dt;
        dt.m_mil = mil;
        dt.m_c = c;
        dt.m_de = de;
        dt.m_y = y;
        dt.m_d = d;
        dt.m_h = h;
        dt.m_m = m;
        dt.m_s = s;
        dt.m_ds = ds;
        dt.m_cs = cs;
        dt.m_ms = ms;
        dt.m_us = us;
        dt.m_ns = ns;
        normalize_unit_pair(dt.m_ns.value(), dt.m_us.value(), 1000);   
        normalize_unit_pair(dt.m_us.value(), dt.m_ms.value(), 1000);   
        normalize_unit_pair(dt.m_ms.value(), dt.m_cs.value(), 10);     
        normalize_unit_pair(dt.m_cs.value(), dt.m_ds.value(), 10);     
        normalize_unit_pair(dt.m_ds.value(), dt.m_s.value(), 10);      
        normalize_unit_pair(dt.m_s.value(), dt.m_m.value(), 60);       
        normalize_unit_pair(dt.m_m.value(), dt.m_h.value(), 60); 
        normalize_unit_pair(dt.m_h.value(), dt.m_d.value(), 24);
        normalize_unit_pair(dt.m_d.value(), dt.m_y.value(), leap_year ? 366 : 365);
        normalize_unit_pair(dt.m_y.value(), dt.m_de.value(), 10);
        normalize_unit_pair(dt.m_de.value(), dt.m_c.value(), 10);
        normalize_unit_pair(dt.m_c.value(), dt.m_mil.value(), 10);

        normalize_unit_pair(dt.m_ns.value(), dt.m_us.value(), 1000);   
        normalize_unit_pair(dt.m_us.value(), dt.m_ms.value(), 1000);   
        normalize_unit_pair(dt.m_ms.value(), dt.m_cs.value(), 10);     
        normalize_unit_pair(dt.m_cs.value(), dt.m_ds.value(), 10);     
        normalize_unit_pair(dt.m_ds.value(), dt.m_s.value(), 10);      
        normalize_unit_pair(dt.m_s.value(), dt.m_m.value(), 60);       
        normalize_unit_pair(dt.m_m.value(), dt.m_h.value(), 60); 
        normalize_unit_pair(dt.m_h.value(), dt.m_d.value(), 24);
        normalize_unit_pair(dt.m_d.value(), dt.m_y.value(), leap_year ? 366 : 365);
        normalize_unit_pair(dt.m_y.value(), dt.m_de.value(), 10);
        normalize_unit_pair(dt.m_de.value(), dt.m_c.value(), 10);
        normalize_unit_pair(dt.m_c.value(), dt.m_mil.value(), 10);
        return dt;
    }

private:
    static constexpr TimePoint from_date_impl(const Date& date) noexcept {
        return TimePoint(day(calculate_days_from_epoch(date)));
    }
    
    static constexpr TimePoint from_datetime_impl(const DateTime& datetime) noexcept {
        const Date date = datetime.get_date();
        const Time time = datetime.get_time();
        const std::int64_t days = calculate_days_from_epoch(date);
        
        return TimePoint(
            day(days),
            hour(time.hours().value()),
            minute(time.minutes().value()),
            second(time.seconds().value()),
            millisecond(time.millseconds().value())
        );
    }
    
    static constexpr std::int64_t calculate_days_from_epoch(const Date& date) noexcept {
        const Date epoch_date = EPOCH.get_date();
        std::int64_t total_days = 0;
        
        const year from_year = epoch_date.get_year();
        const year to_year = date.get_year();
        const month from_month = epoch_date.get_month();
        const month to_month = date.get_month();
        const day from_day = epoch_date.get_day();
        const day to_day = date.get_day();

        for (std::uint64_t y = from_year.value(); y < to_year.value(); ++y) {
            total_days += Calendar::days_in_year(year(y));
        }
        
        for (std::uint8_t m = 0; m < from_month.to_int() - 1; ++m) {
            total_days -= Calendar::days_in_month(month(static_cast<months>(m)), from_year);
        }
        total_days -= (from_day.value() - 1);
        
        for (std::uint8_t m = 0; m < to_month.to_int() - 1; ++m) {
            total_days += Calendar::days_in_month(month(static_cast<months>(m)), to_year);
        }
        total_days += (to_day.value() - 1);
        
        return total_days;
    }
};

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
constexpr TimePoint operator+(const T value, const TimePoint& cd) noexcept { return cd + TimePoint(value); }

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
constexpr TimePoint operator-(const T value, const TimePoint& cd) noexcept { return TimePoint(value) - cd; }

} // namespace time
} // namespace fizmo

#endif // TIME_POINT_CLASS_HPP