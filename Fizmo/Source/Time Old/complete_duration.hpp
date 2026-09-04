#ifndef FIZMO_COMPLETE_Duration_CLASS_HPP
#define FIZMO_COMPLETE_Duration_CLASS_HPP

#include "explicit_casts_impl.hpp"

namespace fizmo {
namespace time {

class CompleteDuration {
private:
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

public:
    constexpr CompleteDuration() noexcept 
        : m_mil(0), m_c(0), m_de(0),
          m_y(0), m_d(0),
          m_h(0), m_m(0), m_s(0),
          m_ds(0), m_cs(0), m_ms(0), m_us(0), m_ns(0)
    {} 

    constexpr CompleteDuration(const CompleteDuration& dt) noexcept 
        : m_mil(dt.m_mil), m_c(dt.m_c), m_de(dt.m_de),
          m_y(dt.m_y), m_d(dt.m_d),
          m_h(dt.m_h), m_m(dt.m_m), m_s(dt.m_s),
          m_ds(dt.m_ds), m_cs(dt.m_cs), m_ms(dt.m_ms), m_us(dt.m_us), m_ns(dt.m_ns)
    {}

    constexpr CompleteDuration(CompleteDuration&& dt) noexcept 
        : m_mil(std::move(dt.m_mil)), m_c(std::move(dt.m_c)), m_de(std::move(dt.m_de)),
          m_y(std::move(dt.m_y)), m_d(std::move(dt.m_d)),
          m_h(std::move(dt.m_h)), m_m(std::move(dt.m_m)), m_s(std::move(dt.m_s)),
          m_ds(std::move(dt.m_ds)), m_cs(std::move(dt.m_cs)), m_ms(std::move(dt.m_ms)), m_us(std::move(dt.m_us)), m_ns(std::move(dt.m_ns))
    {}

    template<typename... Args, typename = typename std::enable_if<all_are_time_units<Args...>::value>::type>
    constexpr CompleteDuration(Args... args) noexcept : CompleteDuration(extract_values(TimeValues{}, args...)) {}

    constexpr CompleteDuration(DURATION_PARAM d) noexcept : CompleteDuration(dur_to_cdur(d)) {}
    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator=(DURATION_PARAM d) noexcept { *this = dur_to_cdur(d); return *this; }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator=(const CompleteDuration& dt) noexcept {
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

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator=(CompleteDuration&& dt) noexcept {
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
        }
        return *this;
    }

public:
    constexpr CompleteDuration(const nanosecond ns) noexcept : CompleteDuration(ns, microsecond(0)) {}
    constexpr CompleteDuration(const microsecond us) noexcept : CompleteDuration(us, nanosecond(0)) {}
    constexpr CompleteDuration(const millisecond ms) noexcept : CompleteDuration(ms, nanosecond(0)) {}
    constexpr CompleteDuration(const centisecond cs) noexcept : CompleteDuration(cs, nanosecond(0)) {}
    constexpr CompleteDuration(const decisecond ds) noexcept : CompleteDuration(ds, nanosecond(0)) {}
    constexpr CompleteDuration(const second s) noexcept : CompleteDuration(s, nanosecond(0)) {}
    constexpr CompleteDuration(const minute m) noexcept : CompleteDuration(m, nanosecond(0)) {}
    constexpr CompleteDuration(const hour h) noexcept : CompleteDuration(h, nanosecond(0)) {}
    constexpr CompleteDuration(const day d) noexcept : CompleteDuration(d, nanosecond(0)) {}
    constexpr CompleteDuration(const year y) noexcept : CompleteDuration(y, nanosecond(0)) {}
    constexpr CompleteDuration(const decade d) noexcept : CompleteDuration(d, nanosecond(0)) {}
    constexpr CompleteDuration(const century c) noexcept : CompleteDuration(c, nanosecond(0)) {}
    constexpr CompleteDuration(const millennium m) noexcept : CompleteDuration(m, nanosecond(0)) {}

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator=(const T v) noexcept {
        *this = CompleteDuration(v);
        return *this;
    }

public:
    constexpr bool operator==(const CompleteDuration& dt) const noexcept {
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

    constexpr bool operator!=(const CompleteDuration& dt) const noexcept { return !(*this == dt); }

    constexpr bool operator<(const CompleteDuration& dt) const noexcept {
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

    constexpr bool operator<=(const CompleteDuration& other) const noexcept { return *this < other || *this == other; }
    constexpr bool operator>(const CompleteDuration& other) const noexcept { return !(*this <= other); }
    constexpr bool operator>=(const CompleteDuration& other) const noexcept { return !(*this < other); }

public:
    std::string to_string(const bool verbose = true) const {
        std::string result;
        bool first = true;

        auto add_unit = [&](std::uint64_t value, const char* verbose_singular, const char* verbose_plural, const char* shorthand) {
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

        add_unit(m_mil.value(), "millennium", "millennia", "mil");
        add_unit(m_c.value(), "century", "centuries", "c");
        add_unit(m_de.value(), "decade", "decades", "dec");
        add_unit(m_y.value(), "year", "years", "yr");
        add_unit(m_d.value(), "day", "days", "d");
        add_unit(m_h.value(), "hour", "hours", "hr");
        add_unit(m_m.value(), "minute", "minutes", "min");
        add_unit(m_s.value(), "second", "seconds", "sec");
        add_unit(m_ds.value(), "decisecond", "deciseconds", "ds");
        add_unit(m_cs.value(), "centisecond", "centiseconds", "cs");
        add_unit(m_ms.value(), "millisecond", "milliseconds", "ms");
        add_unit(m_us.value(), "microsecond", "microseconds", "μs");
        add_unit(m_ns.value(), "nanosecond", "nanoseconds", "ns");
        return result.empty() ? (verbose ? "0 nanoseconds" : "0 ns") : result;
    }

    friend std::ostream& operator<<(std::ostream& os, const CompleteDuration& dd) {
        os << dd.to_string(true);
        return os;
    }

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

public:
    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_millennia(const millennium m) noexcept {
        m_mil = m;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_centuries(const century c) noexcept {
        m_c = c;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_decades(const decade d) noexcept {
        m_de = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_years(const year y) noexcept {
        m_y = y;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_days(const day d) noexcept {
        m_d = d;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_hours(const hour h) noexcept {
        m_h = h;
        return normalize();
    }
    
    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_minutes(const minute m) noexcept {
        m_m = m;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_seconds(const second s) noexcept {
        m_s = s;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_deciseconds(const decisecond ds) noexcept {
        m_ds = ds;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_centiseconds(const centisecond cs) noexcept {
        m_cs = cs;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_milliseconds(const millisecond ms) noexcept {
        m_ms = ms;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_microseconds(const microsecond us) noexcept {
        m_us = us;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_nanoseconds(const nanosecond ns) noexcept {
        m_ns = ns;
        return normalize();
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& set_values(
        const millennium mil, const century c, const decade de, const year y, const day d,
        const hour h, const minute m, const second s,
        const decisecond ds, const centisecond cs,
        const millisecond ms, const microsecond us, const nanosecond ns
    ) noexcept {
        *this = normalize_values(mil, c, de, y, d, h, m, s, ds, cs, ms, us, ns);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& normalize() noexcept {
        *this = normalize_values(m_mil, m_c, m_de, m_y, m_d, m_h, m_m, m_s, m_ds, m_cs, m_ms, m_us, m_ns);
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

    constexpr Duration to_duration(Duration::unit target_unit = Duration::unit::nanosecond) const noexcept {
        switch (target_unit) {
            case Duration::unit::nanosecond: return Duration(to_nanosecond());
            case Duration::unit::microsecond: return Duration(to_microsecond());
            case Duration::unit::millisecond: return Duration(to_millisecond());
            case Duration::unit::centisecond: return Duration(to_centisecond());
            case Duration::unit::decisecond: return Duration(to_decisecond());
            case Duration::unit::second: return Duration(to_second());
            case Duration::unit::minute: return Duration(to_minute());
            case Duration::unit::hour: return Duration(to_hour());
            case Duration::unit::day: return Duration(to_day());
            case Duration::unit::year: return Duration(to_year());
            case Duration::unit::decade: return Duration(to_decade());
            case Duration::unit::century: return Duration(to_century());
            case Duration::unit::millennium: return Duration(to_millennium());
            default: return Duration(to_nanosecond());
        }
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
    constexpr explicit operator Duration() const noexcept { return to_duration(); }

public:
    constexpr CompleteDuration operator+(const CompleteDuration& d) const noexcept {
        return CompleteDuration(
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

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator+=(const CompleteDuration& d) noexcept {
        *this = *this + d;
        return *this;
    }

    constexpr CompleteDuration operator-(const CompleteDuration& d) const noexcept {
        return CompleteDuration(
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

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator-=(const CompleteDuration& d) noexcept {
        *this = *this - d;
        return *this;
    }

    constexpr CompleteDuration operator+(DURATION_PARAM d) const noexcept { return *this + d.to_complete_duration(); }
    constexpr CompleteDuration operator-(DURATION_PARAM d) const noexcept { return *this - d.to_complete_duration(); }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator+=(DURATION_PARAM d) noexcept {
        *this = *this + d;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator-=(DURATION_PARAM d) noexcept {
        *this = *this - d;
        return *this;
    }

public:
    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    constexpr CompleteDuration operator+(const T value) const noexcept { return *this + CompleteDuration(value); }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    constexpr CompleteDuration operator-(const T value) const noexcept { return *this - CompleteDuration(value); }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator+=(const T value) noexcept { *this = *this + CompleteDuration(value); return *this; }

    template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    OPTIONAL_CPP14_CONSTEXPR CompleteDuration& operator-=(const T value) noexcept { *this = *this - CompleteDuration(value); return *this; }

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

    static constexpr CompleteDuration normalize_values(
        const millennium mil, const century c, const decade de, const year y,
        const day d, const hour h, const minute m, const second s,
        const decisecond ds, const centisecond cs, const millisecond ms,
        const microsecond us, const nanosecond ns, const bool leap_year = false
    ) noexcept {
        CompleteDuration dt;
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

    static constexpr CompleteDuration dur_to_cdur(DURATION_PARAM d) noexcept {
        switch (d.get_unit()) {
            case Duration::unit::nanosecond: return CompleteDuration(d.to_nanoseconds());
            case Duration::unit::microsecond: return CompleteDuration(d.to_microseconds());
            case Duration::unit::millisecond: return CompleteDuration(d.to_milliseconds());
            case Duration::unit::centisecond: return CompleteDuration(d.to_centiseconds());
            case Duration::unit::decisecond: return CompleteDuration(d.to_deciseconds());
            case Duration::unit::second: return CompleteDuration(d.to_seconds());
            case Duration::unit::minute: return CompleteDuration(d.to_minutes());
            case Duration::unit::hour: return CompleteDuration(d.to_hours());
            case Duration::unit::day: return CompleteDuration(d.to_days());
            case Duration::unit::year: return CompleteDuration(d.to_years());
            case Duration::unit::decade: return CompleteDuration(d.to_decades());
            case Duration::unit::century: return CompleteDuration(d.to_centuries());
            default: return CompleteDuration(d.to_millennia());
        }
    }

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

    constexpr CompleteDuration(const TimeValues& values) noexcept
        : CompleteDuration(normalize_values(values.mil, 
                                            values.c, values.de, values.y, 
                                            values.d, values.h, values.m, values.s,
                                            values.ds, values.cs, values.ms, values.us, values.ns
                                        )) {}
};

constexpr CompleteDuration Duration::to_complete_duration() const noexcept { return CompleteDuration(*this); }
constexpr Duration::operator CompleteDuration() const noexcept { return to_complete_duration(); }
constexpr Duration::Duration(const CompleteDuration& d, Duration::unit un) noexcept : Duration(d.to_duration(un)) {}
OPTIONAL_CPP14_CONSTEXPR Duration& Duration::operator=(const CompleteDuration& d) noexcept { *this = d.to_duration(); return *this; }
constexpr CompleteDuration Duration::operator+(const CompleteDuration& cd) const noexcept { return cd + *this; }
constexpr CompleteDuration Duration::operator-(const CompleteDuration& cd) const noexcept { return to_complete_duration() - cd; }

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
constexpr CompleteDuration operator+(const T value, const CompleteDuration& cd) noexcept { return cd + CompleteDuration(value); }

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
constexpr CompleteDuration operator-(const T value, const CompleteDuration& cd) noexcept { return CompleteDuration(value) - cd; }

} // namespace time
} // namespace fizmo

#endif // FIZMO_COMPLETE_Duration_CLASS_HPP