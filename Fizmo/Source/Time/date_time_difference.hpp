#ifndef FIZMO_DATE_TIME_DIFFERENCE_CLASSES_HPP
#define FIZMO_DATE_TIME_DIFFERENCE_CLASSES_HPP

#include "time_difference.hpp"

namespace fizmo {
namespace time {

class DateTimeDifference {
private:
    std::int64_t m_mil;
    std::int64_t m_c;
    std::int64_t m_de;
    std::int64_t m_y;
    std::int64_t m_d;
    std::int64_t m_h;
    std::int64_t m_m;
    std::int64_t m_s;
    std::int64_t m_ds;
    std::int64_t m_cs;
    std::int64_t m_ms;
    std::int64_t m_us;
    std::int64_t m_ns;

public:
    constexpr DateTimeDifference() noexcept 
        : m_mil(0), m_c(0), m_de(0),
          m_y(0), m_d(0),
          m_h(0), m_m(0), m_s(0),
          m_ds(0), m_cs(0), m_ms(0), m_us(0), m_ns(0)
    {} 

    constexpr DateTimeDifference(const DateTimeDifference& dt) noexcept 
        : m_mil(dt.m_mil), m_c(dt.m_c), m_de(dt.m_de),
          m_y(dt.m_y), m_d(dt.m_d),
          m_h(dt.m_h), m_m(dt.m_m), m_s(dt.m_s),
          m_ds(dt.m_ds), m_cs(dt.m_cs), m_ms(dt.m_ms), m_us(dt.m_us), m_ns(dt.m_ns)
    {}

    constexpr DateTimeDifference(DateTimeDifference&& dt) noexcept 
        : m_mil(dt.m_mil), m_c(dt.m_c), m_de(dt.m_de),
          m_y(dt.m_y), m_d(dt.m_d),
          m_h(dt.m_h), m_m(dt.m_m), m_s(dt.m_s),
          m_ds(dt.m_ds), m_cs(dt.m_cs), m_ms(dt.m_ms), m_us(dt.m_us), m_ns(dt.m_ns)
        {
            dt.m_mil = 0;
            dt.m_c = 0;
            dt.m_de = 0;
            dt.m_y = 0;
            dt.m_d = 0;
            dt.m_h = 0;
            dt.m_m = 0;
            dt.m_s = 0;
            dt.m_ds = 0;
            dt.m_cs = 0;
            dt.m_ms = 0;
            dt.m_us = 0;
            dt.m_ns = 0;
        }

    constexpr DateTimeDifference(
        const std::int64_t mil, const std::int64_t cen, const std::int64_t de, 
        const std::int64_t y, const std::int64_t day,
        const std::int64_t h = 0, const std::int64_t m = 0, const std::int64_t s = 0,
        const std::int64_t ds = 0, const std::int64_t cs = 0, const std::int64_t ms = 0, 
        const std::int64_t us = 0, const std::int64_t ns = 0
    ) noexcept : DateTimeDifference(normalize_values(mil, cen, de, y, day, h, m, s, ds, cs, ms, us, ns)) {}

    constexpr DateTimeDifference(const DateDifference& d) noexcept 
        : DateTimeDifference(
            d.millennia(),
            d.centuries(),
            d.decades(),
            d.years(),
            d.days(),
            0, 0, 0, 0, 0, 0, 0, 0
        )
    {}

    constexpr DateTimeDifference(DateDifference&& d) noexcept 
        : DateTimeDifference(
            d.millennia(),
            d.centuries(),
            d.decades(),
            d.years(),
            d.days(),
            0, 0, 0, 0, 0, 0, 0, 0
        )
    {
        d.millennia() = 0;
        d.centuries() = 0;
        d.decades() = 0;
        d.years() = 0;
        d.days() = 0;
    }

    constexpr DateTimeDifference(const TimeDifference& t) noexcept 
        : DateTimeDifference(
            0, 0, 0, 0, 0,
            t.hours(), t.minutes(), t.seconds(),
            t.deciseconds(), t.centiseconds(),
            t.milliseconds(), t.microseconds(), t.nanoseconds()
        )
    {}

    constexpr DateTimeDifference(TimeDifference&& t) noexcept 
        : DateTimeDifference(
            0, 0, 0, 0, 0,
            t.hours(), t.minutes(), t.seconds(),
            t.deciseconds(), t.centiseconds(),
            t.milliseconds(), t.microseconds(), t.nanoseconds()
        )
    {
        t.hours() = 0;
        t.minutes() = 0;
        t.seconds() = 0;
        t.deciseconds() = 0;
        t.centiseconds() = 0;
        t.milliseconds() = 0;
        t.microseconds() = 0;
        t.nanoseconds() = 0;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator=(const DateTimeDifference& dt) noexcept {
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
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator=(DateTimeDifference&& dt) noexcept {
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
            dt.m_mil = 0;
            dt.m_c = 0;
            dt.m_de = 0;
            dt.m_y = 0;
            dt.m_d = 0;
            dt.m_h = 0;
            dt.m_m = 0;
            dt.m_s = 0;
            dt.m_ds = 0;
            dt.m_cs = 0;
            dt.m_ms = 0;
            dt.m_us = 0;
            dt.m_ns = 0;
        }
        return *this;
    }

public:
    constexpr DateTimeDifference(const DateDifference& d, const TimeDifference& t) noexcept 
        : m_mil(d.millennia()),
          m_c(d.centuries()),
          m_de(d.decades()),
          m_y(d.years()),
          m_d(d.days()),
          m_h(t.hours()),
          m_m(t.minutes()),
          m_s(t.seconds()),
          m_ds(t.deciseconds()),
          m_cs(t.centiseconds()),
          m_ms(t.milliseconds()),
          m_us(t.microseconds()),
          m_ns(t.nanoseconds())
    {}

    constexpr DateTimeDifference(const DateDifference& d, TimeDifference&& t) noexcept 
        : m_mil(d.millennia()),
          m_c(d.centuries()),
          m_de(d.decades()),
          m_y(d.years()),
          m_d(d.days()),
          m_h(t.hours()),
          m_m(t.minutes()),
          m_s(t.seconds()),
          m_ds(t.deciseconds()),
          m_cs(t.centiseconds()),
          m_ms(t.milliseconds()),
          m_us(t.microseconds()),
          m_ns(t.nanoseconds()) {
            t.hours() = 0;
            t.minutes() = 0;
            t.seconds() = 0;
            t.deciseconds() = 0;
            t.centiseconds() = 0;
            t.milliseconds() = 0;
            t.microseconds() = 0;
            t.nanoseconds() = 0;
        }

    constexpr DateTimeDifference(DateDifference&& d, const TimeDifference& t) noexcept 
          : m_mil(d.millennia()),
          m_c(d.centuries()),
          m_de(d.decades()),
          m_y(d.years()),
          m_d(d.days()),
          m_h(t.hours()),
          m_m(t.minutes()),
          m_s(t.seconds()),
          m_ds(t.deciseconds()),
          m_cs(t.centiseconds()),
          m_ms(t.milliseconds()),
          m_us(t.microseconds()),
          m_ns(t.nanoseconds()) {
            d.millennia() = 0;
            d.centuries() = 0;
            d.decades() = 0;
            d.years() = 0;
            d.days() = 0;
        }

    constexpr DateTimeDifference(DateDifference&& d, TimeDifference&& t) noexcept 
        : m_mil(d.millennia()),
          m_c(d.centuries()),
          m_de(d.decades()),
          m_y(d.years()),
          m_d(d.days()),
          m_h(t.hours()),
          m_m(t.minutes()),
          m_s(t.seconds()),
          m_ds(t.deciseconds()),
          m_cs(t.centiseconds()),
          m_ms(t.milliseconds()),
          m_us(t.microseconds()),
          m_ns(t.nanoseconds()) {
            d.millennia() = 0;
            d.centuries() = 0;
            d.decades() = 0;
            d.years() = 0;
            d.days() = 0;
            t.hours() = 0;
            t.minutes() = 0;
            t.seconds() = 0;
            t.deciseconds() = 0;
            t.centiseconds() = 0;
            t.milliseconds() = 0;
            t.microseconds() = 0;
            t.nanoseconds() = 0;
        }

public:
    constexpr DateTimeDifference(const TimeDifference& t, const DateDifference& d) noexcept : DateTimeDifference(d, t) {}
    constexpr DateTimeDifference(const TimeDifference& t, DateDifference&& d) noexcept : DateTimeDifference(d, t) {}
    constexpr DateTimeDifference(TimeDifference&& t, const DateDifference& d) noexcept : DateTimeDifference(d, t) {}
    constexpr DateTimeDifference(TimeDifference&& t, DateDifference&& d) noexcept : DateTimeDifference(d, t) {}

public:
    constexpr DateDifference get_date_difference() const noexcept { return DateDifference(m_mil, m_c, m_de, m_y, m_d); }
    constexpr TimeDifference get_time_difference() const noexcept { return TimeDifference(m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns); }
    static constexpr DateTimeDifference zero_difference() noexcept { return DateTimeDifference(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
    
    static constexpr DateTimeDifference absolute_largest_difference(const bool negative = false) noexcept {
        const std::int64_t MAX = negative ? std::numeric_limits<std::int64_t>::min() : std::numeric_limits<std::int64_t>::max();
        return DateTimeDifference(MAX, MAX, MAX, MAX, MAX, MAX, MAX, MAX, MAX, MAX, MAX, MAX, MAX);
    }

    static constexpr DateTimeDifference reasonable_largest_difference(const bool negative = false) noexcept {
        const int s = negative ? -1 : 1;
        return DateTimeDifference(
            negative ? std::numeric_limits<std::int64_t>::min() : std::numeric_limits<std::int64_t>::max(),
            s * 9,
            s * 9,
            s * 9,
            s * 364,
            s * 23,
            s * 59,
            s * 59,
            s * 9,
            s * 9,
            s * 9,
            s * 999,
            s * 999
        );
    }

public:
    constexpr bool operator==(const DateTimeDifference& dt) const noexcept {
        return (m_mil == dt.m_mil &&
               m_c == dt.m_c &&
               m_de == dt.m_de &&
               m_y == dt.m_y && 
               m_d == dt.m_d &&
               m_h == dt.m_h &&
               m_m == dt.m_m && 
               m_s == dt.m_s &&
               m_ds == dt.m_ds &&
               m_cs == dt.m_cs &&
               m_ms == dt.m_ms &&
               m_us == dt.m_us &&
               m_ns == dt.m_ns
        );
    }

    constexpr bool operator!=(const DateTimeDifference& dt) const noexcept { return !(*this == dt); }

    constexpr bool operator<(const DateTimeDifference& dt) const noexcept {
        if (m_mil != dt.m_mil) return m_mil < dt.m_mil;
        if (m_c != dt.m_c) return m_c < dt.m_c;
        if (m_de != dt.m_de) return m_de < dt.m_de;
        if (m_y != dt.m_y) return m_y < dt.m_y;
        if (m_d != dt.m_d) return m_d < dt.m_d;
        if (m_h != dt.m_h) return m_h < dt.m_h;
        if (m_m != dt.m_m) return m_m < dt.m_m;
        if (m_s != dt.m_s) return m_s < dt.m_s;
        if (m_ds != dt.m_ds) return m_ds < dt.m_ds;
        if (m_cs != dt.m_cs) return m_cs < dt.m_cs;
        if (m_ms != dt.m_ms) return m_ms < dt.m_ms;
        if (m_us != dt.m_us) return m_us < dt.m_us;
        return m_ns < dt.m_ns;
    }

    constexpr bool operator<=(const DateTimeDifference& other) const noexcept { return *this < other || *this == other; }
    constexpr bool operator>(const DateTimeDifference& other) const noexcept { return !(*this <= other); }
    constexpr bool operator>=(const DateTimeDifference& other) const noexcept { return !(*this < other); }

public:
    std::string to_string(const bool verbose = true) const {
        std::string result;
        bool first = true;

        auto add_unit = [&](std::int64_t value, const char* verbose_singular, const char* verbose_plural, const char* shorthand) {
            if (value != 0) {
                if (!first) result += ", ";
                first = false;
                result += std::to_string(value) + " ";

                if (verbose) {
                    result += (value == 1) ? verbose_singular : verbose_plural;
                } else {
                    result += shorthand;
                }
            }
        };

        add_unit(m_mil, "millennium", "millennia", "mil");
        add_unit(m_c, "century", "centuries", "c");
        add_unit(m_de, "decade", "decades", "dec");
        add_unit(m_y, "year", "years", "yr");
        add_unit(m_d, "day", "days", "d");
        add_unit(m_h, "hour", "hours", "hr");
        add_unit(m_m, "minute", "minutes", "min");
        add_unit(m_s, "second", "seconds", "sec");
        add_unit(m_ds, "decisecond", "deciseconds", "ds");
        add_unit(m_cs, "centisecond", "centiseconds", "cs");
        add_unit(m_ms, "millisecond", "milliseconds", "ms");
        add_unit(m_us, "microsecond", "microseconds", "μs");
        add_unit(m_ns, "nanosecond", "nanoseconds", "ns");
        return result.empty() ? (verbose ? "0 nanoseconds" : "0 ns") : result;
    }

    friend std::ostream& operator<<(std::ostream& os, const DateTimeDifference& dd) {
        os << dd.to_string(true);
        return os;
    }

public:
    constexpr std::int64_t millennia() const noexcept { return m_mil; }
    constexpr std::int64_t centuries() const noexcept { return m_c; }
    constexpr std::int64_t decades() const noexcept { return m_de; }
    constexpr std::int64_t years() const noexcept { return m_y; }
    constexpr std::int64_t days() const noexcept { return m_d; }
    constexpr std::int64_t hours() const noexcept { return m_h; }
    constexpr std::int64_t minutes() const noexcept { return m_m; }
    constexpr std::int64_t seconds() const noexcept { return m_s; }
    constexpr std::int64_t deciseconds() const noexcept { return m_ds; }
    constexpr std::int64_t centiseconds() const noexcept { return m_cs; }
    constexpr std::int64_t milliseconds() const noexcept { return m_ms; }
    constexpr std::int64_t microseconds() const noexcept { return m_us; }
    constexpr std::int64_t nanoseconds() const noexcept { return m_ns; }

    constexpr std::int64_t& millennia() noexcept { return m_mil; }
    constexpr std::int64_t& centuries() noexcept { return m_c; }
    constexpr std::int64_t& decades() noexcept { return m_de; }
    constexpr std::int64_t& years() noexcept { return m_y; }
    constexpr std::int64_t& days() noexcept { return m_d; }
    constexpr std::int64_t& hours() noexcept { return m_h; }
    constexpr std::int64_t& minutes() noexcept { return m_m; }
    constexpr std::int64_t& seconds() noexcept { return m_s; }
    constexpr std::int64_t& deciseconds() noexcept { return m_ds; }
    constexpr std::int64_t& centiseconds() noexcept { return m_cs; }
    constexpr std::int64_t& milliseconds() noexcept { return m_ms; }
    constexpr std::int64_t& microseconds() noexcept { return m_us; }
    constexpr std::int64_t& nanoseconds() noexcept { return m_ns; }

public:
    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_millennia(const std::int64_t m) noexcept {
        m_mil = m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_centuries(const std::int64_t c) noexcept {
        m_c = c;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_decades(const std::int64_t d) noexcept {
        m_de = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_years(const std::int64_t y) noexcept {
        m_y = y;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_days(const std::int64_t d) noexcept {
        m_d = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_hours(const std::int64_t h) noexcept {
        m_h = h;
        return normalize();
    }
    
    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_minutes(const std::int64_t m) noexcept {
        m_m = m;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_seconds(const std::int64_t s) noexcept {
        m_s = s;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_deciseconds(const std::int64_t ds) noexcept {
        m_ds = ds;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_centiseconds(const std::int64_t cs) noexcept {
        m_cs = cs;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_milliseconds(const std::int64_t ms) noexcept {
        m_ms = ms;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_microseconds(const std::int64_t us) noexcept {
        m_us = us;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_nanoseconds(const std::int64_t ns) noexcept {
        m_ns = ns;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& set_values(
        const std::int64_t mil, const std::int64_t c, const std::int64_t de, const std::int64_t y, const std::int64_t d,
        const std::int64_t h, const std::int64_t m, const std::int64_t s,
        const std::int64_t ds, const std::int64_t cs,
        const std::int64_t ms, const std::int64_t us, const std::int64_t ns
    ) noexcept {
        *this = normalize_values(mil, c, de, y, d, h, m, s, ds, cs, ms, us, ns);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& normalize() noexcept {
        *this = normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
        return *this;
    }

public: 
    constexpr DateTimeDifference operator+(const DateTimeDifference& dt) const noexcept {
        return DateTimeDifference(
            safe_signed_add(m_mil, dt.m_mil),
            safe_signed_add(m_c, dt.m_c),
            safe_signed_add(m_de, dt.m_de),
            safe_signed_add(m_y, dt.m_y),
            safe_signed_add(m_d, dt.m_d),
            safe_signed_add(m_h, dt.m_h),
            safe_signed_add(m_m, dt.m_m),
            safe_signed_add(m_s, dt.m_s),
            safe_signed_add(m_ds, dt.m_ds),
            safe_signed_add(m_cs, dt.m_cs),
            safe_signed_add(m_ms, dt.m_ms),
            safe_signed_add(m_us, dt.m_us),
            safe_signed_add(m_ns, dt.m_ns)
        );
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const DateTimeDifference& dt) noexcept {
        *this = *this + dt;
        return *this;
    }

    constexpr DateTimeDifference operator-(const DateTimeDifference& dt) const noexcept {
        return DateTimeDifference(
            safe_signed_subtract(m_mil, dt.m_mil),
            safe_signed_subtract(m_c, dt.m_c),
            safe_signed_subtract(m_de, dt.m_de),
            safe_signed_subtract(m_y, dt.m_y),
            safe_signed_subtract(m_d, dt.m_d),
            safe_signed_subtract(m_h, dt.m_h),
            safe_signed_subtract(m_m, dt.m_m),
            safe_signed_subtract(m_s, dt.m_s),
            safe_signed_subtract(m_ds, dt.m_ds),
            safe_signed_subtract(m_cs, dt.m_cs),
            safe_signed_subtract(m_ms, dt.m_ms),
            safe_signed_subtract(m_us, dt.m_us),
            safe_signed_subtract(m_ns, dt.m_ns)
        );
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const DateTimeDifference& dt) noexcept {
        *this = *this - dt;
        return *this;
    }

    constexpr DateTimeDifference operator+(DURATION_PARAM dur) const noexcept {
        switch (dur.get_unit()) {
            case Duration::unit::nanosecond:
                return *this + nanosecond(dur.count());
            case Duration::unit::microsecond:
                return *this + microsecond(dur.count());
            case Duration::unit::millisecond:
                return *this + millisecond(dur.count());
            case Duration::unit::centisecond:
                return *this + centisecond(dur.count());
            case Duration::unit::decisecond:
                return *this + decisecond(dur.count());
            case Duration::unit::second:
                return *this + second(dur.count());
            case Duration::unit::minute:
                return *this + minute(dur.count());
            case Duration::unit::hour:
                return *this + hour(dur.count());
            case Duration::unit::day:
                return *this + day(dur.count());
            case Duration::unit::week:
                return *this + week(dur.count());
            case Duration::unit::year:
                return *this + year(dur.count());
            case Duration::unit::decade:
                return *this + decade(dur.count());
            case Duration::unit::century:
                return *this + century(dur.count());
            case Duration::unit::millennium:
                return *this + millennium(dur.count());
            default:
                return *this;
        }
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference operator+=(DURATION_PARAM d) noexcept {
        *this = *this + d;
        return *this;
    }

    constexpr DateTimeDifference operator-(DURATION_PARAM dur) const noexcept {
        switch (dur.get_unit()) {
            case Duration::unit::nanosecond:
                return *this - nanosecond(dur.count());
            case Duration::unit::microsecond:
                return *this - microsecond(dur.count());
            case Duration::unit::millisecond:
                return *this - millisecond(dur.count());
            case Duration::unit::centisecond:
                return *this - centisecond(dur.count());
            case Duration::unit::decisecond:
                return *this - decisecond(dur.count());
            case Duration::unit::second:
                return *this - second(dur.count());
            case Duration::unit::minute:
                return *this - minute(dur.count());
            case Duration::unit::hour:
                return *this - hour(dur.count());
            case Duration::unit::day:
                return *this - day(dur.count());
            case Duration::unit::week:
                return *this - week(dur.count());
            case Duration::unit::year:
                return *this - year(dur.count());
            case Duration::unit::decade:
                return *this - decade(dur.count());
            case Duration::unit::century:
                return *this - century(dur.count());
            case Duration::unit::millennium:
                return *this - millennium(dur.count());
            default:
                return *this;
        }
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference operator-=(DURATION_PARAM d) noexcept {
        *this = *this - d;
        return *this;
    }

    constexpr DateTimeDifference operator+(const CompleteDuration& cd) const noexcept {
        return DateTimeDifference(
            safe_add(m_mil, cd.millennia().value()),
            safe_add(m_c, cd.centuries().value()),
            safe_add(m_de, cd.decades().value()),
            safe_add(m_y, cd.years().value()),
            safe_add(m_d, cd.days().value()),
            safe_add(m_h, cd.hours().value()),
            safe_add(m_m, cd.minutes().value()),
            safe_add(m_s, cd.seconds().value()),
            safe_add(m_ds, cd.deciseconds().value()),
            safe_add(m_cs, cd.centiseconds().value()),
            safe_add(m_ms, cd.milliseconds().value()),
            safe_add(m_us, cd.microseconds().value()),
            safe_add(m_ns, cd.nanoseconds().value())
        );
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const CompleteDuration& cd) noexcept {
        *this = *this + cd;
        return *this;
    }

    constexpr DateTimeDifference operator-(const CompleteDuration& cd) const noexcept {
        return DateTimeDifference(
            safe_subtract(m_mil, cd.millennia().value()),
            safe_subtract(m_c, cd.centuries().value()),
            safe_subtract(m_de, cd.decades().value()),
            safe_subtract(m_y, cd.years().value()),
            safe_subtract(m_d, cd.days().value()),
            safe_subtract(m_h, cd.hours().value()),
            safe_subtract(m_m, cd.minutes().value()),
            safe_subtract(m_s, cd.seconds().value()),
            safe_subtract(m_ds, cd.deciseconds().value()),
            safe_subtract(m_cs, cd.centiseconds().value()),
            safe_subtract(m_ms, cd.milliseconds().value()),
            safe_subtract(m_us, cd.microseconds().value()),
            safe_subtract(m_ns, cd.nanoseconds().value())
        );
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const CompleteDuration& cd) noexcept {
        *this = *this - cd;
        return *this;
    }

public:
    constexpr DateTimeDifference operator+(const nanosecond ns) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, safe_add(m_ns, ns.value()));
    }

    constexpr DateTimeDifference operator+(const microsecond us) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, safe_add(m_us, us.value()), m_ns);
    }

    constexpr DateTimeDifference operator+(const millisecond ms) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, safe_add(m_ms, ms.value()), m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const centisecond cs) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, safe_add(m_cs, cs.value()), m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const decisecond ds) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, safe_add(m_ds, ds.value()), m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const second s) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, safe_add(m_s, s.value()), m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const minute m) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, safe_add(m_m, m.value()), m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const hour h) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, safe_add(m_h, h.value()), m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const day d) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, safe_add(m_d, d.value()), m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const week w) const noexcept {
        day days_from_weeks = static_cast<day>(w);
        return normalize_values(m_mil, m_c, m_de, m_y, safe_add(m_d, days_from_weeks.value()), m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const year y) const noexcept {
        return normalize_values(m_mil, m_c, m_de, safe_add(m_y, y.value()), m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const decade de) const noexcept {
        return normalize_values(m_mil, m_c, safe_add(m_de, de.value()), m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const century c) const noexcept {
        return normalize_values(m_mil, safe_add(m_c, c.value()), m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator+(const millennium mil) const noexcept {
        return normalize_values(safe_add(m_mil, mil.value()), m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const nanosecond ns) noexcept {
        *this = *this + ns;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const microsecond us) noexcept {
        *this = *this + us;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const millisecond ms) noexcept {
        *this = *this + ms;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const centisecond cs) noexcept {
        *this = *this + cs;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const decisecond ds) noexcept {
        *this = *this + ds;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const second s) noexcept {
        *this = *this + s;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const minute m) noexcept {
        *this = *this + m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const hour h) noexcept {
        *this = *this + h;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const day d) noexcept {
        *this = *this + d;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const week w) noexcept {
        *this = *this + w;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const year y) noexcept {
        *this = *this + y;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const decade de) noexcept {
        *this = *this + de;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const century c) noexcept {
        *this = *this + c;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator+=(const millennium mil) noexcept {
        *this = *this + mil;
        return *this;
    }

public:
    constexpr DateTimeDifference operator-(const nanosecond ns) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, safe_subtract(m_ns, ns.value()));
    }

    constexpr DateTimeDifference operator-(const microsecond us) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, safe_subtract(m_us, us.value()), m_ns);
    }

    constexpr DateTimeDifference operator-(const millisecond ms) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, safe_subtract(m_ms, ms.value()), m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const centisecond cs) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, safe_subtract(m_cs, cs.value()), m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const decisecond ds) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, safe_subtract(m_ds, ds.value()), m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const second s) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, safe_subtract(m_s, s.value()), m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const minute m) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, safe_subtract(m_m, m.value()), m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const hour h) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, m_d, safe_subtract(m_h, h.value()), m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const day d) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, safe_subtract(m_d, d.value()), m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const week w) const noexcept {
        day days_from_weeks = static_cast<day>(w);
        return normalize_values(m_mil, m_c, m_de, m_y, safe_subtract(m_d, days_from_weeks.value()), m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const year y) const noexcept {
        return normalize_values(m_mil, m_c, m_de, safe_subtract(m_y, y.value()), m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const decade de) const noexcept {
        return normalize_values(m_mil, m_c, safe_subtract(m_de, de.value()), m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const century c) const noexcept {
        return normalize_values(m_mil, safe_subtract(m_c, c.value()), m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr DateTimeDifference operator-(const millennium mil) const noexcept {
        return normalize_values(safe_subtract(m_mil, mil.value()), m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const nanosecond ns) noexcept {
        *this = *this - ns;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const microsecond us) noexcept {
        *this = *this - us;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const millisecond ms) noexcept {
        *this = *this - ms;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const centisecond cs) noexcept {
        *this = *this - cs;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const decisecond ds) noexcept {
        *this = *this - ds;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const second s) noexcept {
        *this = *this - s;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const minute m) noexcept {
        *this = *this - m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const hour h) noexcept {
        *this = *this - h;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const day d) noexcept {
        *this = *this - d;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const week w) noexcept {
        *this = *this - w;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const year y) noexcept {
        *this = *this - y;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const decade de) noexcept {
        *this = *this - de;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const century c) noexcept {
        *this = *this - c;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateTimeDifference& operator-=(const millennium mil) noexcept {
        *this = *this - mil;
        return *this;
    }

    constexpr DateTimeDifference operator-() const noexcept {
        return DateTimeDifference(
            -m_mil, -m_c, -m_de, 
            -m_y, -m_d, 
            -m_h, -m_m, -m_s, 
            -m_ds, -m_cs, -m_ms, -m_us, -m_ns
        );
    }

public:
    constexpr double total_millennia() const noexcept {
        double m = 0.0;
        m += static_cast<double>(m_mil);
        m += static_cast<double>(m_c) / 10.0;
        m += static_cast<double>(m_de) / 100.0;
        m += static_cast<double>(m_y) / 1000.0;
        m += static_cast<double>(m_d) / 365000.0;
        m += static_cast<double>(m_h) / (365000.0 * 24.0);
        m += static_cast<double>(m_m) / (365000.0 * 24.0 * 60.0);
        m += static_cast<double>(m_s) / (365000.0 * 24.0 * 3600.0);
        m += static_cast<double>(m_ds) / (365000.0 * 24.0 * 36000.0);
        m += static_cast<double>(m_cs) / (365000.0 * 24.0 * 360000.0);
        m += static_cast<double>(m_ms) / (365000.0 * 24.0 * 3.6e6);
        m += static_cast<double>(m_us) / (365000.0 * 24.0 * 3.6e9);
        m += static_cast<double>(m_ns) / (365000.0 * 24.0 * 3.6e12);
        return m;
    }

    constexpr double total_centuries() const noexcept {
        double c = 0.0;
        c += static_cast<double>(m_mil) * 10.0;
        c += static_cast<double>(m_c);
        c += static_cast<double>(m_de) / 10.0;
        c += static_cast<double>(m_y) / 100.0;
        c += static_cast<double>(m_d) / 36500.0;
        c += static_cast<double>(m_h) / (36500.0 * 24.0);
        c += static_cast<double>(m_m) / (36500.0 * 24.0 * 60.0);
        c += static_cast<double>(m_s) / (36500.0 * 24.0 * 3600.0);
        c += static_cast<double>(m_ds) / (36500.0 * 24.0 * 36000.0);
        c += static_cast<double>(m_cs) / (36500.0 * 24.0 * 360000.0);
        c += static_cast<double>(m_ms) / (36500.0 * 24.0 * 3.6e6);
        c += static_cast<double>(m_us) / (36500.0 * 24.0 * 3.6e9);
        c += static_cast<double>(m_ns) / (36500.0 * 24.0 * 3.6e12);
        return c;
    }

    constexpr double total_decades() const noexcept {
        double d = 0.0;
        d += static_cast<double>(m_mil) * 100.0;
        d += static_cast<double>(m_c) * 10.0;
        d += static_cast<double>(m_de);
        d += static_cast<double>(m_y) / 10.0;
        d += static_cast<double>(m_d) / 3650.0;
        d += static_cast<double>(m_h) / (3650.0 * 24.0);
        d += static_cast<double>(m_m) / (3650.0 * 24.0 * 60.0);
        d += static_cast<double>(m_s) / (3650.0 * 24.0 * 3600.0);
        d += static_cast<double>(m_ds) / (3650.0 * 24.0 * 36000.0);
        d += static_cast<double>(m_cs) / (3650.0 * 24.0 * 360000.0);
        d += static_cast<double>(m_ms) / (3650.0 * 24.0 * 3.6e6);
        d += static_cast<double>(m_us) / (3650.0 * 24.0 * 3.6e9);
        d += static_cast<double>(m_ns) / (3650.0 * 24.0 * 3.6e12);
        return d;
    }

    constexpr double total_years() const noexcept {
        double y = 0.0;
        y += static_cast<double>(m_mil) * 1000.0;
        y += static_cast<double>(m_c) * 100.0;
        y += static_cast<double>(m_de) * 10.0;
        y += static_cast<double>(m_y);
        y += static_cast<double>(m_d) / 365.0;
        y += static_cast<double>(m_h) / (365.0 * 24.0);
        y += static_cast<double>(m_m) / (365.0 * 24.0 * 60.0);
        y += static_cast<double>(m_s) / (365.0 * 24.0 * 3600.0);
        y += static_cast<double>(m_ds) / (365.0 * 24.0 * 36000.0);
        y += static_cast<double>(m_cs) / (365.0 * 24.0 * 360000.0);
        y += static_cast<double>(m_ms) / (365.0 * 24.0 * 3.6e6);
        y += static_cast<double>(m_us) / (365.0 * 24.0 * 3.6e9);
        y += static_cast<double>(m_ns) / (365.0 * 24.0 * 3.6e12);
        return y;
    }

    constexpr double total_days() const noexcept {
        double d = 0.0;
        d += static_cast<double>(m_mil) * 365000.0;
        d += static_cast<double>(m_c) * 36500.0;
        d += static_cast<double>(m_de) * 3650.0;
        d += static_cast<double>(m_y) * 365.0;
        d += static_cast<double>(m_d);
        d += static_cast<double>(m_h) / 24.0;
        d += static_cast<double>(m_m) / (24.0 * 60.0);
        d += static_cast<double>(m_s) / (24.0 * 3600.0);
        d += static_cast<double>(m_ds) / (24.0 * 36000.0);
        d += static_cast<double>(m_cs) / (24.0 * 360000.0);
        d += static_cast<double>(m_ms) / (24.0 * 3.6e6);
        d += static_cast<double>(m_us) / (24.0 * 3.6e9);
        d += static_cast<double>(m_ns) / (24.0 * 3.6e12);
        return d;
    }

    constexpr double total_hours() const noexcept {
        double h = 0.0;
        h += static_cast<double>(m_mil) * 365000.0 * 24.0;
        h += static_cast<double>(m_c) * 36500.0 * 24.0;
        h += static_cast<double>(m_de) * 3650.0 * 24.0;
        h += static_cast<double>(m_y) * 365.0 * 24.0;
        h += static_cast<double>(m_d) * 24.0;
        h += static_cast<double>(m_h);
        h += static_cast<double>(m_m) / 60.0;
        h += static_cast<double>(m_s) / 3600.0;
        h += static_cast<double>(m_ds) / 36000.0;
        h += static_cast<double>(m_cs) / 360000.0;
        h += static_cast<double>(m_ms) / 3.6e6;
        h += static_cast<double>(m_us) / 3.6e9;
        h += static_cast<double>(m_ns) / 3.6e12;
        return h;
    }

    constexpr double total_minutes() const noexcept {
        double m = 0.0;
        m += static_cast<double>(m_mil) * 365000.0 * 24.0 * 60.0;
        m += static_cast<double>(m_c) * 36500.0 * 24.0 * 60.0;
        m += static_cast<double>(m_de) * 3650.0 * 24.0 * 60.0;
        m += static_cast<double>(m_y) * 365.0 * 24.0 * 60.0;
        m += static_cast<double>(m_d) * 24.0 * 60.0;
        m += static_cast<double>(m_h) * 60.0;
        m += static_cast<double>(m_m);
        m += static_cast<double>(m_s) / 60.0;
        m += static_cast<double>(m_ds) / 600.0;
        m += static_cast<double>(m_cs) / 6000.0;
        m += static_cast<double>(m_ms) / 60000.0;
        m += static_cast<double>(m_us) / 6e7;
        m += static_cast<double>(m_ns) / 6e10;
        return m;
    }

    constexpr double total_seconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil) * 365000.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_c) * 36500.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_de) * 3650.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_y) * 365.0 * 24.0 * 3600.0;
        s += static_cast<double>(m_d) * 24.0 * 3600.0;
        s += static_cast<double>(m_h) * 3600.0;
        s += static_cast<double>(m_m) * 60.0;
        s += static_cast<double>(m_s);
        s += static_cast<double>(m_ds) / 10.0;
        s += static_cast<double>(m_cs) / 100.0;
        s += static_cast<double>(m_ms) / 1000.0;
        s += static_cast<double>(m_us) / 1e6;
        s += static_cast<double>(m_ns) / 1e9;
        return s;
    }

    constexpr double total_deciseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil) * 365000.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_c) * 36500.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_de) * 3650.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_y) * 365.0 * 24.0 * 36000.0;
        s += static_cast<double>(m_d) * 24.0 * 36000.0;
        s += static_cast<double>(m_h) * 36000.0;
        s += static_cast<double>(m_m) * 600.0;
        s += static_cast<double>(m_s) * 10.0;
        s += static_cast<double>(m_ds);
        s += static_cast<double>(m_cs) / 10.0;
        s += static_cast<double>(m_ms) / 100.0;
        s += static_cast<double>(m_us) / 100000.0;
        s += static_cast<double>(m_ns) / 1e8;
        return s;
    }

    constexpr double total_centiseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil) * 365000.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_c) * 36500.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_de) * 3650.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_y) * 365.0 * 24.0 * 360000.0;
        s += static_cast<double>(m_d) * 24.0 * 360000.0;
        s += static_cast<double>(m_h) * 360000.0;
        s += static_cast<double>(m_m) * 6000.0;
        s += static_cast<double>(m_s) * 100.0;
        s += static_cast<double>(m_ds) * 10;
        s += static_cast<double>(m_cs);
        s += static_cast<double>(m_ms) / 10.0;
        s += static_cast<double>(m_us) / 10000.0;
        s += static_cast<double>(m_ns) / 1e7;
        return s;
    }

    constexpr double total_milliseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil) * 365000.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_c) * 36500.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_de) * 3650.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_y) * 365.0 * 24.0 * 3.6e6;
        s += static_cast<double>(m_d) * 24.0 * 3.6e6;
        s += static_cast<double>(m_h) * 3.6e6;
        s += static_cast<double>(m_m) * 60000.0;
        s += static_cast<double>(m_s) * 1000.0;
        s += static_cast<double>(m_ds) * 100.0;
        s += static_cast<double>(m_cs) * 10.0;
        s += static_cast<double>(m_ms);
        s += static_cast<double>(m_us) / 1000.0;
        s += static_cast<double>(m_ns) / 1e6;
        return s;
    }

    constexpr double total_microseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil) * 365000.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_c) * 36500.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_de) * 3650.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_y) * 365.0 * 24.0 * 3.6e9;
        s += static_cast<double>(m_d) * 24.0 * 3.6e9;
        s += static_cast<double>(m_h) * 3.6e9;
        s += static_cast<double>(m_m) * 6e7;
        s += static_cast<double>(m_s) * 1e6;
        s += static_cast<double>(m_ds) * 100000.0;
        s += static_cast<double>(m_cs) * 10000.0;
        s += static_cast<double>(m_ms) * 1000.0;
        s += static_cast<double>(m_us);
        s += static_cast<double>(m_ns) / 1000.0;
        return s;
    }

    constexpr double total_nanoseconds() const noexcept {
        double s = 0.0;
        s += static_cast<double>(m_mil) * 365000.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_c) * 36500.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_de) * 3650.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_y) * 365.0 * 24.0 * 3.6e12;
        s += static_cast<double>(m_d) * 24.0 * 3.6e12;
        s += static_cast<double>(m_h) * 3.6e12;
        s += static_cast<double>(m_m) * 6e10;
        s += static_cast<double>(m_s) * 1e9;
        s += static_cast<double>(m_ds) * 1e8;
        s += static_cast<double>(m_cs) * 1e7;
        s += static_cast<double>(m_ms) * 1e6;
        s += static_cast<double>(m_us) * 1000.0;
        s += static_cast<double>(m_ns);
        return s;
    }

public:
    explicit constexpr operator TimeDifference() const noexcept { return get_time_difference(); }
    explicit constexpr operator DateDifference() const noexcept { return get_date_difference(); }

private:
    static constexpr std::int64_t safe_add(const std::int64_t a, const std::uint64_t b) noexcept {
        if (b > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return (a >= 0) ? std::numeric_limits<std::int64_t>::max() : 
                            (std::numeric_limits<std::int64_t>::max() - static_cast<std::int64_t>(b - static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())));
        }
        
        std::int64_t b_signed = static_cast<std::int64_t>(b);
        if (a > 0 && b_signed > 0 && a > std::numeric_limits<std::int64_t>::max() - b_signed) { return std::numeric_limits<std::int64_t>::max(); }
        return a + b_signed;
    }

    static constexpr std::int64_t safe_subtract(const std::int64_t a, const std::uint64_t b) noexcept {
        if (b > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) { return std::numeric_limits<std::int64_t>::min(); }
        std::int64_t b_signed = static_cast<std::int64_t>(b);
        if (a < 0 && b_signed > 0 && a < std::numeric_limits<std::int64_t>::min() + b_signed) { return std::numeric_limits<std::int64_t>::min(); }
        if (b_signed > 0 && a < std::numeric_limits<std::int64_t>::min() + b_signed) { return std::numeric_limits<std::int64_t>::min(); }
        return a - b_signed;
    }

    static constexpr std::int64_t safe_signed_add(const std::int64_t a, const std::int64_t b) noexcept {
        if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) { return std::numeric_limits<std::int64_t>::max(); }
        if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) { return std::numeric_limits<std::int64_t>::min(); }
        return a + b;
    }

    static constexpr std::int64_t safe_signed_subtract(const std::int64_t a, const std::int64_t b) noexcept {
        if (b < 0) {
            if (a > std::numeric_limits<std::int64_t>::max() + b) {
                return std::numeric_limits<std::int64_t>::max();
            }
        } else if (b > 0) {
            if (a < std::numeric_limits<std::int64_t>::min() + b) {
                return std::numeric_limits<std::int64_t>::min();
            }
        }
        
        return a - b;
    }

    static constexpr void normalize_unit_pair(std::int64_t& lower_unit, std::int64_t& upper_unit, const std::int64_t conversion_factor) noexcept {
        if (lower_unit >= conversion_factor) {
            if (upper_unit > 0 && lower_unit / conversion_factor > std::numeric_limits<std::int64_t>::max() - upper_unit) {
                upper_unit = std::numeric_limits<std::int64_t>::max();
                lower_unit = lower_unit % conversion_factor;
            } else {
                std::int64_t carry = lower_unit / conversion_factor;
                upper_unit += carry;
                lower_unit %= conversion_factor;
            }
        } else if (lower_unit <= -conversion_factor) {
            std::int64_t borrow = (-lower_unit + conversion_factor - 1) / conversion_factor; 
            
            if (upper_unit < 0 && borrow > upper_unit - std::numeric_limits<std::int64_t>::min()) {
                upper_unit = std::numeric_limits<std::int64_t>::min();
                lower_unit = -((-lower_unit) % conversion_factor);
            } else {
                upper_unit -= borrow;
                lower_unit += borrow * conversion_factor;
            }
        }
    }

    static constexpr DateTimeDifference normalize_values(
        const std::int64_t mil, const std::int64_t cen, const std::int64_t de, 
        const std::int64_t y, const std::int64_t day,
        const std::int64_t h, const std::int64_t m, const std::int64_t s,
        const std::int64_t ds, const std::int64_t cs, const std::int64_t ms, 
        const std::int64_t us, const std::int64_t ns,
        const bool leap_year = false
    ) noexcept {
        DateTimeDifference dt;
        dt.m_mil = mil;
        dt.m_c = cen;
        dt.m_de = de;
        dt.m_y = y;
        dt.m_d = day;
        dt.m_h = h;
        dt.m_m = m;
        dt.m_s = s;
        dt.m_ds = ds;
        dt.m_cs = cs;
        dt.m_ms = ms;
        dt.m_us = us;
        dt.m_ns = ns;
        normalize_unit_pair(dt.m_ns, dt.m_us, 1000);   
        normalize_unit_pair(dt.m_us, dt.m_ms, 1000);   
        normalize_unit_pair(dt.m_ms, dt.m_cs, 10);     
        normalize_unit_pair(dt.m_cs, dt.m_ds, 10);     
        normalize_unit_pair(dt.m_ds, dt.m_s, 10);      
        normalize_unit_pair(dt.m_s, dt.m_m, 60);       
        normalize_unit_pair(dt.m_m, dt.m_h, 60); 
        normalize_unit_pair(dt.m_h, dt.m_d, 24);
        normalize_unit_pair(dt.m_d, dt.m_y, leap_year ? 366 : 365);
        normalize_unit_pair(dt.m_y, dt.m_de, 10);
        normalize_unit_pair(dt.m_de, dt.m_c, 10);
        normalize_unit_pair(dt.m_c, dt.m_mil, 10);

        normalize_unit_pair(dt.m_ns, dt.m_us, 1000);   
        normalize_unit_pair(dt.m_us, dt.m_ms, 1000);   
        normalize_unit_pair(dt.m_ms, dt.m_cs, 10);     
        normalize_unit_pair(dt.m_cs, dt.m_ds, 10);     
        normalize_unit_pair(dt.m_ds, dt.m_s, 10);      
        normalize_unit_pair(dt.m_s, dt.m_m, 60);       
        normalize_unit_pair(dt.m_m, dt.m_h, 60); 
        normalize_unit_pair(dt.m_h, dt.m_d, 24);
        normalize_unit_pair(dt.m_d, dt.m_y, leap_year ? 366 : 365);
        normalize_unit_pair(dt.m_y, dt.m_de, 10);
        normalize_unit_pair(dt.m_de, dt.m_c, 10);
        normalize_unit_pair(dt.m_c, dt.m_mil, 10);
        return dt;
    }
};

constexpr TimeDifference::operator DateTimeDifference() const noexcept { return DateTimeDifference(*this); }
constexpr DateDifference::operator DateTimeDifference() const noexcept { return DateTimeDifference(*this); }

} // namespace time
} // namespace fizmo

#endif // FIZMO_DATE_TIME_DIFFERENCE_CLASSES_HPP