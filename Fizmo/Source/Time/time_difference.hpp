#ifndef FIZMO_TIME_DIFFERENCE_CLASSES_HPP
#define FIZMO_TIME_DIFFERENCE_CLASSES_HPP

#include "complete_duration.hpp"

namespace fizmo {
namespace time {

class DateTimeDifference;

class TimeDifference {
private:
    std::int64_t m_h;
    std::int64_t m_m;
    std::int64_t m_s;
    std::int64_t m_ds;
    std::int64_t m_cs;
    std::int64_t m_ms;
    std::int64_t m_us;
    std::int64_t m_ns;

public:
    constexpr TimeDifference() noexcept : 
        m_h(0),
        m_m(0),
        m_s(0),
        m_ds(0),
        m_cs(0),
        m_ms(0),
        m_us(0),
        m_ns(0)
    {}

    constexpr TimeDifference(const TimeDifference& td) noexcept :
        m_h(td.m_h),
        m_m(td.m_m),
        m_s(td.m_s),
        m_ds(td.m_ds),
        m_cs(td.m_cs),
        m_ms(td.m_ms),
        m_us(td.m_us),
        m_ns(td.m_ns)
    {}

    constexpr TimeDifference(TimeDifference&& td) noexcept :
        m_h(td.m_h),
        m_m(td.m_m),
        m_s(td.m_s),
        m_ds(td.m_ds),
        m_cs(td.m_cs),
        m_ms(td.m_ms),
        m_us(td.m_us),
        m_ns(td.m_ns)
    {
        td.m_h = 0;
        td.m_m = 0;
        td.m_s = 0;
        td.m_ds = 0;
        td.m_cs = 0;
        td.m_ms = 0;
        td.m_us = 0;
        td.m_ns = 0;
    }

    constexpr TimeDifference(
        const std::int64_t h, const std::int64_t m, const std::int64_t s,
        const std::int64_t ds, const std::int64_t cs, const std::int64_t ms, 
        const std::int64_t us, const std::int64_t ns
    ) noexcept :
        m_h(normalize_values(h, m, s, ds, cs, ms, us, ns).m_h),
        m_m(normalize_values(h, m, s, ds, cs, ms, us, ns).m_m),
        m_s(normalize_values(h, m, s, ds, cs, ms, us, ns).m_s),
        m_ds(normalize_values(h, m, s, ds, cs, ms, us, ns).m_ds),
        m_cs(normalize_values(h, m, s, ds, cs, ms, us, ns).m_cs),
        m_ms(normalize_values(h, m, s, ds, cs, ms, us, ns).m_ms),
        m_us(normalize_values(h, m, s, ds, cs, ms, us, ns).m_us),
        m_ns(normalize_values(h, m, s, ds, cs, ms, us, ns).m_ns)
    {}

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator=(const TimeDifference& td) noexcept {
        if (this != &td) {
            m_h = td.m_h;
            m_m = td.m_m;
            m_s = td.m_s;
            m_ds = td.m_ds;
            m_cs = td.m_cs;
            m_ms = td.m_ms;
            m_us = td.m_us;
            m_ns = td.m_ns;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator=(TimeDifference&& td) noexcept {
        if (this != &td) {
            m_h = td.m_h;
            m_m = td.m_m;
            m_s = td.m_s;
            m_ds = td.m_ds;
            m_cs = td.m_cs;
            m_ms = td.m_ms;
            m_us = td.m_us;
            m_ns = td.m_ns;
            td.m_h = 0;
            td.m_m = 0;
            td.m_s = 0;
            td.m_ds = 0;
            td.m_cs = 0;
            td.m_ms = 0;
            td.m_us = 0;
            td.m_ns = 0;
        }
        return *this;
    }

public:
    static constexpr TimeDifference zero_difference() noexcept {
        TimeDifference td;
        td.m_h = 0;
        td.m_m = 0;
        td.m_s = 0;
        td.m_ds = 0;
        td.m_cs = 0;
        td.m_ms = 0;
        td.m_us = 0;
        td.m_ns = 0;
        return td;
    }

    static constexpr TimeDifference absolute_largest_difference(const bool negative = false) noexcept {
        TimeDifference td;
        td.m_h = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_m = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_s = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_ds = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_cs = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_ms = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_us = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        td.m_ns = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        return td;
    }

    static constexpr TimeDifference reasonable_largest_difference(const bool negative = false) noexcept {
        const int s = negative ? -1 : 1;
        TimeDifference td;
        td.m_h = s * 23;
        td.m_m = s * 59;
        td.m_s = s * 59;
        td.m_ds = s * 9;
        td.m_cs = s * 9;
        td.m_ms = s * 9;
        td.m_us = s * 999;
        td.m_ns = s * 999;
        return td;
    }

public:
    constexpr bool operator==(const TimeDifference& other) const noexcept {
        return m_h == other.m_h && m_m == other.m_m && m_s == other.m_s && 
               m_ds == other.m_ds && m_cs == other.m_cs && m_ms == other.m_ms && 
               m_us == other.m_us && m_ns == other.m_ns;
    }

    constexpr bool operator!=(const TimeDifference& other) const noexcept { return !(*this == other); }

    constexpr bool operator<(const TimeDifference& other) const noexcept {
        if (m_h != other.m_h) return m_h < other.m_h;
        if (m_m != other.m_m) return m_m < other.m_m;
        if (m_s != other.m_s) return m_s < other.m_s;
        if (m_ds != other.m_ds) return m_ds < other.m_ds;
        if (m_cs != other.m_cs) return m_cs < other.m_cs;
        if (m_ms != other.m_ms) return m_ms < other.m_ms;
        if (m_us != other.m_us) return m_us < other.m_us;
        return m_ns < other.m_ns;
    }

    constexpr bool operator<=(const TimeDifference& other) const noexcept { return *this < other || *this == other; }
    constexpr bool operator>(const TimeDifference& other) const noexcept { return !(*this <= other); }
    constexpr bool operator>=(const TimeDifference& other) const noexcept { return !(*this < other); }

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
                    result += (value == 1 || value == -1) ? verbose_singular : verbose_plural;
                } else {
                    result += shorthand;
                }
            }
        };

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

    friend std::ostream& operator<<(std::ostream& os, const TimeDifference& td) {
        os << td.to_string(true);
        return os;
    }

public:
    constexpr std::int64_t hours() const noexcept { return m_h; }
    constexpr std::int64_t minutes() const noexcept { return m_m; }
    constexpr std::int64_t seconds() const noexcept { return m_s; }
    constexpr std::int64_t deciseconds() const noexcept { return m_ds; }
    constexpr std::int64_t centiseconds() const noexcept { return m_cs; }
    constexpr std::int64_t milliseconds() const noexcept { return m_ms; }
    constexpr std::int64_t microseconds() const noexcept { return m_us; }
    constexpr std::int64_t nanoseconds() const noexcept { return m_ns; }

    constexpr std::int64_t& hours() noexcept { return m_h; }
    constexpr std::int64_t& minutes() noexcept { return m_m; }
    constexpr std::int64_t& seconds() noexcept { return m_s; }
    constexpr std::int64_t& deciseconds() noexcept { return m_ds; }
    constexpr std::int64_t& centiseconds() noexcept { return m_cs; }
    constexpr std::int64_t& milliseconds() noexcept { return m_ms; }
    constexpr std::int64_t& microseconds() noexcept { return m_us; }
    constexpr std::int64_t& nanoseconds() noexcept { return m_ns; }

public:
    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_hours(const std::int64_t h) noexcept {
        m_h = h;
        return normalize();
    }
    
    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_minutes(const std::int64_t m) noexcept {
        m_m = m;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_seconds(const std::int64_t s) noexcept {
        m_s = s;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_deciseconds(const std::int64_t ds) noexcept {
        m_ds = ds;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_centiseconds(const std::int64_t cs) noexcept {
        m_cs = cs;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_milliseconds(const std::int64_t ms) noexcept {
        m_ms = ms;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_microseconds(const std::int64_t us) noexcept {
        m_us = us;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_nanoseconds(const std::int64_t ns) noexcept {
        m_ns = ns;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& normalize() noexcept { 
        *this = normalize_values(m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& set_values(
        const std::int64_t h, const std::int64_t m, const std::int64_t s,
        const std::int64_t ds, const std::int64_t cs,
        const std::int64_t ms, const std::int64_t us, const std::int64_t ns
    ) noexcept { 
        *this = normalize_values(h, m, s, ds, cs, ms, us, ns);
        return *this;
    }

public:
    constexpr TimeDifference operator+(const TimeDifference& other) const noexcept {
        return normalize_values(
            safe_signed_add(m_h, other.m_h),
            safe_signed_add(m_m, other.m_m),
            safe_signed_add(m_s, other.m_s),
            safe_signed_add(m_ds, other.m_ds),
            safe_signed_add(m_cs, other.m_cs),
            safe_signed_add(m_ms, other.m_ms),
            safe_signed_add(m_us, other.m_us),
            safe_signed_add(m_ns, other.m_ns)
        );
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const TimeDifference& other) noexcept {
        *this = *this + other;
        return *this;
    }

    constexpr TimeDifference operator-(const TimeDifference& other) const noexcept {
        return normalize_values(
            safe_signed_subtract(m_h, other.m_h),
            safe_signed_subtract(m_m, other.m_m),
            safe_signed_subtract(m_s, other.m_s),
            safe_signed_subtract(m_ds, other.m_ds),
            safe_signed_subtract(m_cs, other.m_cs),
            safe_signed_subtract(m_ms, other.m_ms),
            safe_signed_subtract(m_us, other.m_us),
            safe_signed_subtract(m_ns, other.m_ns)
        );
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const TimeDifference& other) noexcept {
        *this = *this - other;
        return *this;
    }

    constexpr TimeDifference operator+(DURATION_PARAM dur) const noexcept {
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
            default:
                return *this;
        }
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(DURATION_PARAM dur) noexcept {
        *this = *this + dur;
        return *this;
    }

    constexpr TimeDifference operator-(DURATION_PARAM dur) const noexcept {
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
            default:
                return *this;
        }
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(DURATION_PARAM dur) noexcept {
        *this = *this - dur;
        return *this;
    }

public:
    constexpr TimeDifference operator+(const nanosecond ns) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, safe_add(m_ns, ns.value()));
    }

    constexpr TimeDifference operator+(const microsecond us) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, m_cs, m_ms, safe_add(m_us, us.value()), m_ns);
    }

    constexpr TimeDifference operator+(const millisecond ms) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, m_cs, safe_add(m_ms, ms.value()), m_us, m_ns);
    }

    constexpr TimeDifference operator+(const centisecond cs) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, safe_add(m_cs, cs.value()), m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator+(const decisecond ds) const noexcept {
        return normalize_values(m_h, m_m, m_s, safe_add(m_ds, ds.value()), m_cs, m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator+(const second s) const noexcept {
        return normalize_values(m_h, m_m, safe_add(m_s, s.value()), m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator+(const minute m) const noexcept {
        return normalize_values(m_h, safe_add(m_m, m.value()), m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator+(const hour h) const noexcept {
        return normalize_values(safe_add(m_h, h.value()), m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const nanosecond ns) noexcept {
        *this = *this + ns;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const microsecond us) noexcept {
        *this = *this + us;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const millisecond ms) noexcept {
        *this = *this + ms;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const centisecond cs) noexcept {
        *this = *this + cs;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const decisecond ds) noexcept {
        *this = *this + ds;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const second s) noexcept {
        *this = *this + s;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const minute m) noexcept {
        *this = *this + m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator+=(const hour h) noexcept {
        *this = *this + h;
        return *this;
    }

public:
    constexpr TimeDifference operator-(const nanosecond ns) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, safe_subtract(m_ns, ns.value()));
    }

    constexpr TimeDifference operator-(const microsecond us) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, m_cs, m_ms, safe_subtract(m_us, us.value()), m_ns);
    }

    constexpr TimeDifference operator-(const millisecond ms) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, m_cs, safe_subtract(m_ms, ms.value()), m_us, m_ns);
    }

    constexpr TimeDifference operator-(const centisecond cs) const noexcept {
        return normalize_values(m_h, m_m, m_s, m_ds, safe_subtract(m_cs, cs.value()), m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator-(const decisecond ds) const noexcept {
        return normalize_values(m_h, m_m, m_s, safe_subtract(m_ds, ds.value()), m_cs, m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator-(const second s) const noexcept {
        return normalize_values(m_h, m_m, safe_subtract(m_s, s.value()), m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator-(const minute m) const noexcept {
        return normalize_values(m_h, safe_subtract(m_m, m.value()), m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    constexpr TimeDifference operator-(const hour h) const noexcept {
        return normalize_values(safe_subtract(m_h, h.value()), m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const nanosecond ns) noexcept {
        *this = *this - ns;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const microsecond us) noexcept {
        *this = *this - us;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const millisecond ms) noexcept {
        *this = *this - ms;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const centisecond cs) noexcept {
        *this = *this - cs;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const decisecond ds) noexcept {
        *this = *this - ds;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const second s) noexcept {
        *this = *this - s;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const minute m) noexcept {
        *this = *this - m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR TimeDifference& operator-=(const hour h) noexcept {
        *this = *this - h;
        return *this;
    }

    constexpr TimeDifference operator-() const noexcept {
        TimeDifference td = *this;
        td.m_ns = -m_ns;
        td.m_us = -m_us;
        td.m_ms = -m_ms;
        td.m_cs = -m_cs;
        td.m_ds = -m_ds;
        td.m_s = -m_s;
        td.m_m = -m_m;
        td.m_h = -m_h;
        return td;
    }

public:
    constexpr double total_hours() const noexcept {
        double h = 0.0;
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
    explicit constexpr operator DateTimeDifference() const noexcept;

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
        if (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) { return std::numeric_limits<std::int64_t>::max(); }
        if (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) { return std::numeric_limits<std::int64_t>::min(); }
        return a - b;
    }

    static constexpr void normalize_unit_pair(std::int64_t& lower_unit, std::int64_t& upper_unit, const std::int64_t conversion_factor) noexcept {
        if (lower_unit >= conversion_factor) {
            const std::int64_t upper_remaining = std::numeric_limits<std::int64_t>::max() - upper_unit;
            const std::int64_t max_convertible = lower_unit / conversion_factor;
            
            if (max_convertible <= upper_remaining) {
                upper_unit += max_convertible;
                lower_unit %= conversion_factor;
            } else {
                upper_unit = std::numeric_limits<std::int64_t>::max();
                lower_unit -= upper_remaining * conversion_factor;
            }
        } else if (lower_unit <= -conversion_factor) {
            const std::int64_t upper_remaining = upper_unit - std::numeric_limits<std::int64_t>::min();
            const std::int64_t max_convertible = (-lower_unit) / conversion_factor;
            
            if (max_convertible <= upper_remaining) {
                upper_unit -= max_convertible;
                lower_unit = -((-lower_unit) % conversion_factor);
            } else {
                upper_unit = std::numeric_limits<std::int64_t>::min();
                lower_unit += upper_remaining * conversion_factor;
            }
        }
    }

    static constexpr TimeDifference normalize_values(
        const std::int64_t h, const std::int64_t m, const std::int64_t s,
        const std::int64_t ds, const std::int64_t cs, const std::int64_t ms, 
        const std::int64_t us, const std::int64_t ns
    ) noexcept {
        TimeDifference t;
        t.m_ns = ns;
        t.m_us = us;
        t.m_ms = ms;
        t.m_cs = cs;
        t.m_ds = ds;
        t.m_s = s;
        t.m_m = m;
        t.m_h = h;
        normalize_unit_pair(t.m_ns, t.m_us, 1000);   
        normalize_unit_pair(t.m_us, t.m_ms, 1000);   
        normalize_unit_pair(t.m_ms, t.m_cs, 10);     
        normalize_unit_pair(t.m_cs, t.m_ds, 10);     
        normalize_unit_pair(t.m_ds, t.m_s, 10);      
        normalize_unit_pair(t.m_s, t.m_m, 60);       
        normalize_unit_pair(t.m_m, t.m_h, 60);       
        normalize_unit_pair(t.m_ns, t.m_us, 1000);   
        normalize_unit_pair(t.m_us, t.m_ms, 1000);   
        normalize_unit_pair(t.m_ms, t.m_cs, 10);     
        normalize_unit_pair(t.m_cs, t.m_ds, 10);     
        normalize_unit_pair(t.m_ds, t.m_s, 10);      
        normalize_unit_pair(t.m_s, t.m_m, 60);       
        normalize_unit_pair(t.m_m, t.m_h, 60);
        return t;
    }
};

class DateDifference {
private:
    std::int64_t m_mil;
    std::int64_t m_c;
    std::int64_t m_de;
    std::int64_t m_y;
    std::int64_t m_d;

public:
    constexpr DateDifference() noexcept : 
        m_mil(0),
        m_c(0),
        m_de(0),
        m_y(0),
        m_d(0)
    {}

    constexpr DateDifference(const DateDifference& dd) noexcept :
        m_mil(dd.m_mil),
        m_c(dd.m_c),
        m_de(dd.m_de),
        m_y(dd.m_y),
        m_d(dd.m_d)
    {}

    constexpr DateDifference(DateDifference&& dd) noexcept :
        m_mil(dd.m_mil),
        m_c(dd.m_c),
        m_de(dd.m_de),
        m_y(dd.m_y),
        m_d(dd.m_d)
    {
        dd.m_mil = 0;
        dd.m_c = 0;
        dd.m_de = 0;
        dd.m_y = 0;
        dd.m_d = 0;
    }

    constexpr DateDifference(
        const std::int64_t mil, const std::int64_t c, const std::int64_t de, 
        const std::int64_t y, const std::int64_t d
    ) noexcept :
        m_mil(normalize_values(mil, c, de, y, d).m_mil),
        m_c(normalize_values(mil, c, de, y, d).m_c),
        m_de(normalize_values(mil, c, de, y, d).m_de),
        m_y(normalize_values(mil, c, de, y, d).m_y),
        m_d(normalize_values(mil, c, de, y, d).m_d)
    {}

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator=(const DateDifference& dd) noexcept {
        if (this != &dd) {
            m_mil = dd.m_mil;
            m_c = dd.m_c;
            m_de = dd.m_de;
            m_y = dd.m_y;
            m_d = dd.m_d;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator=(DateDifference&& dd) noexcept {
        if (this != &dd) {
            m_mil = dd.m_mil;
            m_c = dd.m_c;
            m_de = dd.m_de;
            m_y = dd.m_y;
            m_d = dd.m_d;
            dd.m_mil = 0;
            dd.m_c = 0;
            dd.m_de = 0;
            dd.m_y = 0;
            dd.m_d = 0;
        }
        return *this;
    }

public:
    static constexpr DateDifference zero_difference() noexcept {
        DateDifference dd;
        dd.m_mil = 0;
        dd.m_c = 0;
        dd.m_de = 0;
        dd.m_y = 0;
        dd.m_d = 0;
        return dd;
    }

    static constexpr DateDifference absolute_largest_difference(const bool negative = false) noexcept {
        DateDifference dd;
        dd.m_mil = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        dd.m_c = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        dd.m_de = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        dd.m_y = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        dd.m_d = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        return dd;
    }

    static constexpr DateDifference reasonable_largest_difference(const bool negative = false) noexcept {
        const int s = negative ? -1 : 1;
        DateDifference dd;
        dd.m_mil = negative ? std::numeric_limits<std::int64_t>::lowest() : std::numeric_limits<std::int64_t>::max();
        dd.m_c = s * 9;
        dd.m_de = s * 9;
        dd.m_y = s * 9;
        dd.m_d = s * 364;
        return dd;
    }

public:
    constexpr bool operator==(const DateDifference& other) const noexcept {
        return m_mil == other.m_mil && m_c == other.m_c && m_de == other.m_de && 
               m_y == other.m_y && m_d == other.m_d;
    }

    constexpr bool operator!=(const DateDifference& other) const noexcept { return !(*this == other); }

    constexpr bool operator<(const DateDifference& other) const noexcept {
        if (m_mil != other.m_mil) return m_mil < other.m_mil;
        if (m_c != other.m_c) return m_c < other.m_c;
        if (m_de != other.m_de) return m_de < other.m_de;
        if (m_y != other.m_y) return m_y < other.m_y;
        return m_d < other.m_d;
    }

    constexpr bool operator<=(const DateDifference& other) const noexcept { return *this < other || *this == other; }
    constexpr bool operator>(const DateDifference& other) const noexcept { return !(*this <= other); }
    constexpr bool operator>=(const DateDifference& other) const noexcept { return !(*this < other); }

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
                    result += (value == 1 || value == -1) ? verbose_singular : verbose_plural;
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
        return result.empty() ? (verbose ? "0 days" : "0 d") : result;
    }

    friend std::ostream& operator<<(std::ostream& os, const DateDifference& dd) {
        os << dd.to_string(true);
        return os;
    }

public:
    constexpr std::int64_t millennia() const noexcept { return m_mil; }
    constexpr std::int64_t centuries() const noexcept { return m_c; }
    constexpr std::int64_t decades() const noexcept { return m_de; }
    constexpr std::int64_t years() const noexcept { return m_y; }
    constexpr std::int64_t days() const noexcept { return m_d; }

    constexpr std::int64_t& millennia() noexcept { return m_mil; }
    constexpr std::int64_t& centuries() noexcept { return m_c; }
    constexpr std::int64_t& decades() noexcept { return m_de; }
    constexpr std::int64_t& years() noexcept { return m_y; }
    constexpr std::int64_t& days() noexcept { return m_d; }

public:
    OPTIONAL_CPP14_CONSTEXPR DateDifference& set_millennia(const std::int64_t m) noexcept {
        m_mil = m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& set_centuries(const std::int64_t c) noexcept {
        m_c = c;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& set_decades(const std::int64_t d) noexcept {
        m_de = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& set_years(const std::int64_t y) noexcept {
        m_y = y;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& set_days(const std::int64_t d) noexcept {
        m_d = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& set_values(const std::int64_t mil, const std::int64_t c, const std::int64_t de, const std::int64_t y, const std::int64_t d) noexcept {
        *this = normalize_values(mil, c, de, y, d);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& normalize() noexcept {
        *this = normalize_values(m_mil, m_c, m_de, m_y, m_d);
        return *this;
    }

public:
    constexpr DateDifference operator+(const DateDifference& other) const noexcept {
        return normalize_values(
            safe_signed_add(m_mil, other.m_mil),
            safe_signed_add(m_c, other.m_c),
            safe_signed_add(m_de, other.m_de),
            safe_signed_add(m_y, other.m_y),
            safe_signed_add(m_d, other.m_d)
        );
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const DateDifference& other) noexcept {
        *this = *this + other;
        return *this;
    }

    constexpr DateDifference operator-(const DateDifference& other) const noexcept {
        return normalize_values(
            safe_signed_subtract(m_mil, other.m_mil),
            safe_signed_subtract(m_c, other.m_c),
            safe_signed_subtract(m_de, other.m_de),
            safe_signed_subtract(m_y, other.m_y),
            safe_signed_subtract(m_d, other.m_d)
        );
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const DateDifference& other) noexcept {
        *this = *this - other;
        return *this;
    }

    constexpr DateDifference operator+(DURATION_PARAM dur) const noexcept {
        switch (dur.get_unit()) {
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

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(DURATION_PARAM dur) noexcept {
        *this = *this + dur;
        return *this;
    }

    constexpr DateDifference operator-(DURATION_PARAM dur) const noexcept {
        switch (dur.get_unit()) {
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

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(DURATION_PARAM dur) noexcept {
        *this = *this - dur;
        return *this;
    }

public:
    constexpr DateDifference operator+(const day d) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, safe_add(m_d, d.value()));
    }

    constexpr DateDifference operator+(const week w) const noexcept {
        day days_from_weeks = static_cast<day>(w);
        return normalize_values(m_mil, m_c, m_de, m_y, safe_add(m_d, days_from_weeks.value()));
    }

    constexpr DateDifference operator+(const year y) const noexcept {
        return normalize_values(m_mil, m_c, m_de, safe_add(m_y, y.value()), m_d);
    }

    constexpr DateDifference operator+(const decade de) const noexcept {
        return normalize_values(m_mil, m_c, safe_add(m_de, de.value()), m_y, m_d);
    }

    constexpr DateDifference operator+(const century c) const noexcept {
        return normalize_values(m_mil, safe_add(m_c, c.value()), m_de, m_y, m_d);
    }

    constexpr DateDifference operator+(const millennium mil) const noexcept {
        return normalize_values(safe_add(m_mil, mil.value()), m_c, m_de, m_y, m_d);
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const day d) noexcept {
        *this = *this + d;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const week w) noexcept {
        *this = *this + w;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const year y) noexcept {
        *this = *this + y;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const decade& de) noexcept {
        *this = *this + de;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const century c) noexcept {
        *this = *this + c;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator+=(const millennium mil) noexcept {
        *this = *this + mil;
        return *this;
    }

public:
    constexpr DateDifference operator-(const day d) const noexcept {
        return normalize_values(m_mil, m_c, m_de, m_y, safe_subtract(m_d, d.value()));
    }

    constexpr DateDifference operator-(const week w) const noexcept {
        day days_from_weeks = static_cast<day>(w);
        return normalize_values(m_mil, m_c, m_de, m_y, safe_subtract(m_d, days_from_weeks.value()));
    }

    constexpr DateDifference operator-(const year y) const noexcept {
        return normalize_values(m_mil, m_c, m_de, safe_subtract(m_y, y.value()), m_d);
    }

    constexpr DateDifference operator-(const decade de) const noexcept {
        return normalize_values(m_mil, m_c, safe_subtract(m_de, de.value()), m_y, m_d);
    }

    constexpr DateDifference operator-(const century c) const noexcept {
        return normalize_values(m_mil, safe_subtract(m_c, c.value()), m_de, m_y, m_d);
    }

    constexpr DateDifference operator-(const millennium mil) const noexcept {
        return normalize_values(safe_subtract(m_mil, mil.value()), m_c, m_de, m_y, m_d);
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const day d) noexcept {
        *this = *this - d;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const week w) noexcept {
        *this = *this - w;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const year y) noexcept {
        *this = *this - y;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const decade de) noexcept {
        *this = *this - de;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const century c) noexcept {
        *this = *this - c;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR DateDifference& operator-=(const millennium mil) noexcept {
        *this = *this - mil;
        return *this;
    }

    constexpr DateDifference operator-() const noexcept {
        DateDifference dd = *this;
        dd.m_mil = -m_mil;
        dd.m_c = -m_c;
        dd.m_de = -m_de;
        dd.m_y = -m_y;
        dd.m_d = -m_d;
        return dd;
    }

public:
    constexpr double total_millenia() const noexcept {
        double m = 0.0;
        m += static_cast<double>(m_mil);
        m += static_cast<double>(m_c) / 10.0;
        m += static_cast<double>(m_de) / 100.0;
        m += static_cast<double>(m_y) / 1000.0;
        m += static_cast<double>(m_d) / 365000.0;
        return m;
    }

    constexpr double total_centuries() const noexcept {
        double c = 0.0;
        c += static_cast<double>(m_mil) * 10.0;
        c += static_cast<double>(m_c);
        c += static_cast<double>(m_de) / 10.0;
        c += static_cast<double>(m_y) / 100.0;
        c += static_cast<double>(m_d) / 36500.0;
        return c;
    }

    constexpr double total_decades() const noexcept {
        double d = 0.0;
        d += static_cast<double>(m_mil) * 100.0;
        d += static_cast<double>(m_c) * 10.0;
        d += static_cast<double>(m_de);
        d += static_cast<double>(m_y) / 10.0;
        d += static_cast<double>(m_d) / 3650.0;
        return d;
    }

    constexpr double total_years() const noexcept {
        double y = 0.0;
        y += static_cast<double>(m_mil) * 1000.0;
        y += static_cast<double>(m_c) * 100.0;
        y += static_cast<double>(m_de) * 10.0;
        y += static_cast<double>(m_y);
        y += static_cast<double>(m_d) / 365.0;
        return y;
    }

    constexpr double total_days() const noexcept {
        double d = 0.0;
        d += static_cast<double>(m_mil) * 365000.0;
        d += static_cast<double>(m_c) * 36500.0;
        d += static_cast<double>(m_de) * 3650.0;
        d += static_cast<double>(m_y) * 365.0;
        d += static_cast<double>(m_d);
        return d;
    }

public:
    explicit constexpr operator DateTimeDifference() const noexcept;

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
        if (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) { return std::numeric_limits<std::int64_t>::max(); }
        if (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) { return std::numeric_limits<std::int64_t>::min(); }
        return a - b;
    }

    static constexpr void normalize_unit_pair(std::int64_t& lower_unit, std::int64_t& upper_unit, const std::int64_t conversion_factor) noexcept {
        if (lower_unit >= conversion_factor) {
            const std::int64_t upper_remaining = std::numeric_limits<std::int64_t>::max() - upper_unit;
            const std::int64_t max_convertible = lower_unit / conversion_factor;
            
            if (max_convertible <= upper_remaining) {
                upper_unit += max_convertible;
                lower_unit %= conversion_factor;
            } else {
                upper_unit = std::numeric_limits<std::int64_t>::max();
                lower_unit -= upper_remaining * conversion_factor;
            }
        } else if (lower_unit <= -conversion_factor) {
            const std::int64_t upper_remaining = upper_unit - std::numeric_limits<std::int64_t>::min();
            const std::int64_t max_convertible = (-lower_unit) / conversion_factor;
            
            if (max_convertible <= upper_remaining) {
                upper_unit -= max_convertible;
                lower_unit = -((-lower_unit) % conversion_factor);
            } else {
                upper_unit = std::numeric_limits<std::int64_t>::min();
                lower_unit += upper_remaining * conversion_factor;
            }
        }
    }

    static constexpr DateDifference normalize_values(const std::int64_t mil, const std::int64_t cen, const std::int64_t de, const std::int64_t y, const std::int64_t day, const bool leap_year = false) noexcept {
        DateDifference d;
        d.m_mil = mil;
        d.m_c = cen;
        d.m_de = de;
        d.m_y = y;
        d.m_d = day;
        normalize_unit_pair(d.m_d, d.m_y, leap_year ? 366 : 365);
        normalize_unit_pair(d.m_y, d.m_de, 10);
        normalize_unit_pair(d.m_de, d.m_c, 10);
        normalize_unit_pair(d.m_c, d.m_mil, 10);
        normalize_unit_pair(d.m_d, d.m_y, leap_year ? 366 : 365);
        normalize_unit_pair(d.m_y, d.m_de, 10);
        normalize_unit_pair(d.m_de, d.m_c, 10);
        normalize_unit_pair(d.m_c, d.m_mil, 10);
        return d;
    }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_TIME_DIFFERENCE_CLASSES_HPP