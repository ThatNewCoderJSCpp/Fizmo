#ifndef FIZMO_TIME_DATE_POINT_CLASSES_HPP
#define FIZMO_TIME_DATE_POINT_CLASSES_HPP

#include "time_unit.hpp"
#include "duration.hpp"
#include "calendar.hpp"
#include <tuple>
#include <type_traits>

namespace fizmo {

template<temp_time::Unit U>
constexpr bool is_calendar_point_unit_v = static_cast<std::uint8_t>(U) >= static_cast<std::uint8_t>(temp_time::Unit::day);

template<temp_time::Unit U>
constexpr bool is_time_point_unit_v = static_cast<std::uint8_t>(U) < static_cast<std::uint8_t>(temp_time::Unit::day);

template<typename... Tags>
constexpr bool all_calendar_point_units_v = (is_calendar_point_unit_v<Tags::tag> && ...);

template<typename... Tags>
constexpr bool all_time_point_units_v = (is_time_point_unit_v<Tags::tag> && ...);

namespace temp_time {

#define FIZMO_FOR_EACH_UNIT(OP) \
    OP(planck_second) \
    OP(quectosecond)  \
    OP(rontosecond)   \
    OP(yoctosecond)   \
    OP(zeptosecond)   \
    OP(attosecond)    \
    OP(femtosecond)   \
    OP(picosecond)    \
    OP(nanosecond)    \
    OP(microsecond)   \
    OP(millisecond)   \
    OP(centisecond)   \
    OP(decisecond)    \
    OP(second)        \
    OP(minute)        \
    OP(hour)          \
    OP(day)           \
    OP(week)          \
    OP(month)         \
    OP(year)          \
    OP(decade)        \
    OP(century)       \
    OP(millennium)

template <typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
constexpr T planck_per_unit(Unit u) noexcept {
    constexpr T table[] = {
    #define FIZMO_GEN_TABLE_ENTRY(tag) static_cast<T>(unit_traits<Unit::tag>::planck_per_unit()),
        FIZMO_FOR_EACH_UNIT(FIZMO_GEN_TABLE_ENTRY)
    #undef FIZMO_GEN_TABLE_ENTRY
    };

    return table[static_cast<std::uint8_t>(u)];
}

template<typename T = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<T>>::type>
class CalendarPoint {
public:
    using value_type = T;

    struct CivilDate {
        T             year;
        std::uint8_t  month; // 1-based, January = 1
        std::uint8_t  day;   // 1-based
    };

private:
    T m_days; // signed day offset from Jan 1, Year 1 AD

    static constexpr T days_from_civil(const T& y, unsigned int m1, const T& d) noexcept {
        T yw = y - (m1 <= 2u ? T(1) : T(0));
        const T era = (yw >= T(0) ? yw : yw - T(399)) / T(400);
        const T yoe = yw - era * T(400);
        const T doy = T((153 * (m1 > 2u ? m1 - 3u : m1 + 9u) + 2) / 5) + d - T(1);
        const T doe = yoe * T(365) + yoe / T(4) - yoe / T(100) + doy;
        return era * T(146097) + doe - T(306);
    }

    static constexpr CivilDate civil_from_days(const T& z) noexcept {
        const T zw  = z + T(306); 
        const T era = (zw >= T(0) ? zw : zw - T(146096)) / T(146097);
        const T doe = zw - era * T(146097);                      
        const T yoe = (doe - doe / T(1460) + doe / T(36524) - doe / T(146096)) / T(365);            
        T y         = yoe + era * T(400);
        const T doy = doe - (T(365) * yoe + yoe / T(4) - yoe / T(100));
        const T mp  = (T(5) * doy + T(2)) / T(153);
        const std::uint8_t d = static_cast<std::uint8_t>(doy - (T(153) * mp + T(2)) / T(5) + T(1));
        const std::uint8_t m = static_cast<std::uint8_t>(mp < T(10) ? mp + T(3) : mp - T(9));
        y += (m <= 2u ? T(1) : T(0));
        return { y, m, d };
    }

    static constexpr std::uint8_t clamp_day(const T& y, std::uint8_t m1, std::uint8_t d) noexcept {
        const std::uint8_t max_d = Calendar::days_in_month(Month(static_cast<Months>(m1 - 1)), y);
        return d > max_d ? max_d : d;
    }

public:
    constexpr CalendarPoint() noexcept : m_days(T(0)) {}

    constexpr CalendarPoint(const T& year, Month month, const std::uint8_t day) noexcept
        : m_days(
            days_from_civil(
                year,
                static_cast<unsigned>(month.value()) + 1u,
                T(clamp_day(year, month.as_int() + 1, day))
            )
        ) {}

    constexpr explicit CalendarPoint(const T& raw_days) noexcept : m_days(raw_days) {}
    constexpr CalendarPoint(const CalendarPoint&) noexcept            = default;
    constexpr CalendarPoint(CalendarPoint&&) noexcept                 = default;
    constexpr CalendarPoint& operator=(const CalendarPoint&) noexcept = default;
    constexpr CalendarPoint& operator=(CalendarPoint&&) noexcept      = default;
    constexpr CivilDate   to_civil()  const noexcept { return civil_from_days(m_days); }
    constexpr T           year()      const noexcept { return to_civil().year; }
    constexpr Month       month()     const noexcept { return Month(static_cast<Months>(to_civil().month - 1)); }
    constexpr std::uint8_t day()      const noexcept { return to_civil().day; }
    constexpr const T&    raw_days()  const noexcept { return m_days; }
    constexpr       T&    raw_days()        noexcept { return m_days; }

    constexpr Weekday weekday() const noexcept {
        auto c = to_civil();
        return Calendar::get_weekday(c.year, Month(static_cast<Months>(c.month - 1)), c.day);
    }

    constexpr bool          is_leap_year()          const noexcept { return Calendar::is_leap_year(year()); }
    constexpr std::uint8_t  days_in_current_month() const noexcept { return Calendar::days_in_month(month(), year()); }
    constexpr std::uint16_t days_in_current_year()  const noexcept { return Calendar::days_in_year(year()); }

    static constexpr CalendarPoint beginning_of_ad() noexcept { return CalendarPoint(T(1), Month(Months::january), 1); }
    static constexpr CalendarPoint gregorian_start() noexcept { return CalendarPoint(T(1582), Month(Months::october), 15); }
    static constexpr CalendarPoint unix_epoch() noexcept { return CalendarPoint(T(1970), Month(Months::january), 1); }
    static constexpr CalendarPoint windows_epoch() noexcept { return CalendarPoint(T(1601), Month(Months::january), 1); }

    constexpr CalendarPoint& add_days(const T& n) noexcept {
        m_days += n;
        return *this;
    }

    constexpr CalendarPoint& add_months(const T& n) noexcept {
        auto c = to_civil();
        T total_m = T(c.month - 1) + n;          
        T new_year = c.year + total_m / T(12);
        T new_m0   = total_m % T(12);
        if (new_m0 < T(0)) { new_m0 += T(12); new_year -= T(1); }
        const std::uint8_t m1 = static_cast<std::uint8_t>(new_m0) + 1;
        const std::uint8_t d  = clamp_day(new_year, m1, c.day);
        m_days = days_from_civil(new_year, m1, T(d));
        return *this;
    }

    constexpr CalendarPoint& add_years(const T& n) noexcept {
        auto c = to_civil();
        T new_year = c.year + n;
        const std::uint8_t d = clamp_day(new_year, c.month, c.day);
        m_days = days_from_civil(new_year, c.month, T(d));
        return *this;
    }

    template<typename DV>
    constexpr CalendarPoint& operator+=(const Duration<Unit::day, DV>& rhs) noexcept {
        m_days += static_cast<T>(rhs.count());
        return *this;
    }

    template<typename DV>
    constexpr CalendarPoint& operator-=(const Duration<Unit::day, DV>& rhs) noexcept {
        m_days -= static_cast<T>(rhs.count());
        return *this;
    }

    template<typename DV>
    constexpr CalendarPoint& operator+=(const Duration<Unit::week, DV>& rhs) noexcept {
        m_days += static_cast<T>(rhs.count()) * T(7);
        return *this;
    }

    template<typename DV>
    constexpr CalendarPoint& operator-=(const Duration<Unit::week, DV>& rhs) noexcept {
        m_days -= static_cast<T>(rhs.count()) * T(7);
        return *this;
    }

    template<typename DV>
    friend constexpr CalendarPoint operator+(CalendarPoint lhs, const Duration<Unit::day, DV>& rhs) noexcept { return lhs += rhs; }

    template<typename DV>
    friend constexpr CalendarPoint operator+( const Duration<Unit::day, DV>& lhs, CalendarPoint rhs) noexcept { return rhs += lhs; }

    template<typename DV>
    friend constexpr CalendarPoint operator-(CalendarPoint lhs, const Duration<Unit::day, DV>& rhs) noexcept { return lhs -= rhs; }

    template<typename DV>
    friend constexpr CalendarPoint operator+(CalendarPoint lhs, const Duration<Unit::week, DV>& rhs) noexcept { return lhs += rhs; }

    template<typename DV>
    friend constexpr CalendarPoint operator+(const Duration<Unit::week, DV>& lhs, CalendarPoint rhs) noexcept { return rhs += lhs; }

    template<typename DV>
    friend constexpr CalendarPoint operator-(CalendarPoint lhs, const Duration<Unit::week, DV>& rhs) noexcept { return lhs -= rhs; }

    friend constexpr T operator-(const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days - b.m_days; }
    constexpr CalendarPoint& operator++()    noexcept { ++m_days; return *this; }
    constexpr CalendarPoint  operator++(int) noexcept { CalendarPoint t(*this); ++m_days; return t; }
    constexpr CalendarPoint& operator--()    noexcept { --m_days; return *this; }
    constexpr CalendarPoint  operator--(int) noexcept { CalendarPoint t(*this); --m_days; return t; }
    friend constexpr bool operator==(const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days == b.m_days; }
    friend constexpr bool operator!=(const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days != b.m_days; }
    friend constexpr bool operator< (const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days <  b.m_days; }
    friend constexpr bool operator<=(const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days <= b.m_days; }
    friend constexpr bool operator> (const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days >  b.m_days; }
    friend constexpr bool operator>=(const CalendarPoint& a, const CalendarPoint& b) noexcept { return a.m_days >= b.m_days; }

    template<typename U, typename E>
    friend constexpr bool operator==(const CalendarPoint& a, const CalendarPoint<U, E>& b) noexcept {
        return a.m_days == static_cast<T>(b.raw_days());
    }

    template<typename U, typename E>
    friend constexpr bool operator!=(const CalendarPoint& a, const CalendarPoint<U, E>& b) noexcept {
        return a.m_days != static_cast<T>(b.raw_days());
    }

    template<typename U, typename E>
    friend constexpr bool operator<(const CalendarPoint& a, const CalendarPoint<U, E>& b) noexcept {
        return a.m_days < static_cast<T>(b.raw_days());
    }

    template<typename U, typename E>
    friend constexpr bool operator<=(const CalendarPoint& a, const CalendarPoint<U, E>& b) noexcept {
        return a.m_days <= static_cast<T>(b.raw_days());
    }

    template<typename U, typename E>
    friend constexpr bool operator>(const CalendarPoint& a, const CalendarPoint<U, E>& b) noexcept {
        return a.m_days > static_cast<T>(b.raw_days());
    }

    template<typename U, typename E>
    friend constexpr bool operator>=(const CalendarPoint& a, const CalendarPoint<U, E>& b) noexcept {
        return a.m_days >= static_cast<T>(b.raw_days());
    }

    std::string to_string() const {
        auto c = to_civil();
        std::ostringstream oss;
        oss << month_name(static_cast<Months>(c.month - 1))
            << " " << static_cast<int>(c.day)
            << ", " << c.year;
        return oss.str();
    }

    std::string to_iso_string() const {
        auto c = to_civil();
        std::ostringstream oss;
        oss << c.year << "-";
        if (c.month < 10) oss << "0";
        oss << static_cast<int>(c.month) << "-";
        if (c.day < 10) oss << "0";
        oss << static_cast<int>(c.day);
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const CalendarPoint& cp) { return os << cp.to_string(); }
};

template<typename V = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<V>>::type>
using calendar_point_t = CalendarPoint<V>;

using calendar_point = CalendarPoint<>;

template<typename T = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<T>>::type>
class TimePoint {
public:
    using value_type = T;

    struct CivilTime {
        T             hours;        
        std::uint8_t  minutes;      
        std::uint8_t  seconds;      
        std::uint16_t milliseconds; 
        std::uint16_t microseconds; 
        std::uint16_t nanoseconds;  
        T             sub_nanosecond_planck; 
    };

private:
    T m_planck; 

    static constexpr T floor_div(const T& a, const T& b) noexcept {
        T q = a / b;
        T r = a - q * b;
        if ((r != T(0)) && ((r < T(0)) != (b < T(0)))) --q;
        return q;
    }

    static constexpr T floor_mod(const T& a, const T& b) noexcept {
        T r = a - floor_div(a, b) * b;
        return r;
    }

    static constexpr CivilTime decompose(const T& total) noexcept {
        const T pph  = planck_per_unit<T>(Unit::hour);
        const T ppm  = planck_per_unit<T>(Unit::minute);
        const T pps  = planck_per_unit<T>(Unit::second);
        const T ppms = planck_per_unit<T>(Unit::millisecond);
        const T ppus = planck_per_unit<T>(Unit::microsecond);
        const T ppns = planck_per_unit<T>(Unit::nanosecond);
        T h   = floor_div(total, pph);
        T rem = floor_mod(total, pph);
        T min_v = rem / ppm;
        rem     = rem % ppm;
        T s   = rem / pps;
        rem   = rem % pps;
        T ms  = rem / ppms;
        rem   = rem % ppms;
        T us  = rem / ppus;
        rem   = rem % ppus;
        T ns  = rem / ppns;
        rem   = rem % ppns;

        return {
            h,
            static_cast<std::uint8_t>(min_v),
            static_cast<std::uint8_t>(s),
            static_cast<std::uint16_t>(ms),
            static_cast<std::uint16_t>(us),
            static_cast<std::uint16_t>(ns),
            rem
        };
    }

    static constexpr T compose(
        const T& hours,
        std::uint8_t  minutes,
        std::uint8_t  seconds,
        std::uint16_t milliseconds = 0,
        std::uint16_t microseconds = 0,
        std::uint16_t nanoseconds  = 0
    ) noexcept {
        return hours          * planck_per_unit<T>(Unit::hour)
             + T(minutes)     * planck_per_unit<T>(Unit::minute)
             + T(seconds)     * planck_per_unit<T>(Unit::second)
             + T(milliseconds)* planck_per_unit<T>(Unit::millisecond)
             + T(microseconds)* planck_per_unit<T>(Unit::microsecond)
             + T(nanoseconds) * planck_per_unit<T>(Unit::nanosecond);
    }

public:
    constexpr TimePoint() noexcept : m_planck(T(0)) {}

    constexpr TimePoint(
        const T&      hours,
        std::uint8_t  minutes,
        std::uint8_t  seconds,
        std::uint16_t milliseconds = 0,
        std::uint16_t microseconds = 0,
        std::uint16_t nanoseconds  = 0
    ) noexcept
        : m_planck(compose(hours, minutes, seconds, milliseconds, microseconds, nanoseconds)) {}

    constexpr explicit TimePoint(const T& raw_planck) noexcept : m_planck(raw_planck) {}
    constexpr TimePoint(const TimePoint&) noexcept            = default;
    constexpr TimePoint(TimePoint&&) noexcept                 = default;
    constexpr TimePoint& operator=(const TimePoint&) noexcept = default;
    constexpr TimePoint& operator=(TimePoint&&) noexcept      = default;

    constexpr CivilTime    to_civil()           const noexcept { return decompose(m_planck); }
    constexpr T            hour()               const noexcept { return to_civil().hours; }
    constexpr std::uint8_t minute()             const noexcept { return to_civil().minutes; }
    constexpr std::uint8_t second()             const noexcept { return to_civil().seconds; }
    constexpr std::uint16_t millisecond()       const noexcept { return to_civil().milliseconds; }
    constexpr std::uint16_t microsecond()       const noexcept { return to_civil().microseconds; }
    constexpr std::uint16_t nanosecond()        const noexcept { return to_civil().nanoseconds; }
    constexpr T            sub_nanosecond_planck() const noexcept { return to_civil().sub_nanosecond_planck; }
    constexpr const T&     raw_planck()         const noexcept { return m_planck; }
    constexpr       T&     raw_planck()               noexcept { return m_planck; }

    constexpr TimePoint& add_hours(const T& n) noexcept {
        m_planck += n * planck_per_unit<T>(Unit::hour);
        return *this;
    }

    constexpr TimePoint& add_minutes(const T& n) noexcept {
        m_planck += n * planck_per_unit<T>(Unit::minute);
        return *this;
    }

    constexpr TimePoint& add_seconds(const T& n) noexcept {
        m_planck += n * planck_per_unit<T>(Unit::second);
        return *this;
    }

    constexpr TimePoint& add_milliseconds(const T& n) noexcept {
        m_planck += n * planck_per_unit<T>(Unit::millisecond);
        return *this;
    }

    constexpr TimePoint& add_microseconds(const T& n) noexcept {
        m_planck += n * planck_per_unit<T>(Unit::microsecond);
        return *this;
    }

    constexpr TimePoint& add_nanoseconds(const T& n) noexcept {
        m_planck += n * planck_per_unit<T>(Unit::nanosecond);
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    constexpr TimePoint& operator+=(const Duration<DTag, DV>& rhs) noexcept {
        m_planck += static_cast<T>(rhs.count()) * planck_per_unit<T>(DTag);
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    constexpr TimePoint& operator-=(const Duration<DTag, DV>& rhs) noexcept {
        m_planck -= static_cast<T>(rhs.count()) * planck_per_unit<T>(DTag);
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr TimePoint operator+(TimePoint lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs += rhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr TimePoint operator+(const Duration<DTag, DV>& lhs, TimePoint rhs) noexcept { return rhs += lhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr TimePoint operator-(TimePoint lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs -= rhs; }

    friend constexpr T operator-(const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck - b.m_planck; }

    constexpr TimePoint& operator++()    noexcept { ++m_planck; return *this; }
    constexpr TimePoint  operator++(int) noexcept { TimePoint t(*this); ++m_planck; return t; }
    constexpr TimePoint& operator--()    noexcept { --m_planck; return *this; }
    constexpr TimePoint  operator--(int) noexcept { TimePoint t(*this); --m_planck; return t; }

    friend constexpr bool operator==(const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck == b.m_planck; }
    friend constexpr bool operator!=(const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck != b.m_planck; }
    friend constexpr bool operator< (const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck <  b.m_planck; }
    friend constexpr bool operator<=(const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck <= b.m_planck; }
    friend constexpr bool operator> (const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck >  b.m_planck; }
    friend constexpr bool operator>=(const TimePoint& a, const TimePoint& b) noexcept { return a.m_planck >= b.m_planck; }

    template<typename U, typename E>
    friend constexpr bool operator==(const TimePoint& a, const TimePoint<U, E>& b) noexcept { return a.m_planck == static_cast<T>(b.raw_planck()); }

    template<typename U, typename E>
    friend constexpr bool operator!=(const TimePoint& a, const TimePoint<U, E>& b) noexcept { return a.m_planck != static_cast<T>(b.raw_planck()); }

    template<typename U, typename E>
    friend constexpr bool operator<(const TimePoint& a, const TimePoint<U, E>& b) noexcept { return a.m_planck < static_cast<T>(b.raw_planck()); }

    template<typename U, typename E>
    friend constexpr bool operator<=(const TimePoint& a, const TimePoint<U, E>& b) noexcept { return a.m_planck <= static_cast<T>(b.raw_planck()); }

    template<typename U, typename E>
    friend constexpr bool operator>(const TimePoint& a, const TimePoint<U, E>& b) noexcept { return a.m_planck > static_cast<T>(b.raw_planck()); }

    template<typename U, typename E>
    friend constexpr bool operator>=(const TimePoint& a, const TimePoint<U, E>& b) noexcept { return a.m_planck >= static_cast<T>(b.raw_planck());  }

    std::string to_string() const {
        auto c = to_civil();
        std::ostringstream oss;
        oss << c.hours << "h "
            << static_cast<int>(c.minutes) << "m "
            << static_cast<int>(c.seconds) << "s "
            << static_cast<int>(c.milliseconds) << "ms "
            << static_cast<int>(c.microseconds) << "us "
            << static_cast<int>(c.nanoseconds) << "ns";
        return oss.str();
    }

    std::string to_iso_string() const {
        auto c = to_civil();
        std::ostringstream oss;
        if (c.hours < T(0)) oss << "-";
        T abs_h = c.hours < T(0) ? -c.hours : c.hours;
        if (abs_h < T(10)) oss << "0";
        oss << abs_h << ":";
        if (c.minutes < 10) oss << "0";
        oss << static_cast<int>(c.minutes) << ":";
        if (c.seconds < 10) oss << "0";
        oss << static_cast<int>(c.seconds) << ".";
        if (c.milliseconds < 100) oss << "0";
        if (c.milliseconds < 10)  oss << "0";
        oss << static_cast<int>(c.milliseconds);
        if (c.microseconds != 0 || c.nanoseconds != 0) {
            if (c.microseconds < 100) oss << "0";
            if (c.microseconds < 10)  oss << "0";
            oss << static_cast<int>(c.microseconds);
        }
        if (c.nanoseconds != 0) {
            if (c.nanoseconds < 100) oss << "0";
            if (c.nanoseconds < 10)  oss << "0";
            oss << static_cast<int>(c.nanoseconds);
        }
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const TimePoint& tp) { return os << tp.to_string(); }
};

template<typename V = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<V>>::type>
using time_point_t = TimePoint<V>;

using time_point = TimePoint<>;

} // namespace temp_time

template<typename T> struct is_calendar_point : std::false_type {};

template<typename T, typename E>
struct is_calendar_point<temp_time::CalendarPoint<T, E>> : std::true_type {};

template<typename T>
constexpr bool is_calendar_point_v = is_calendar_point<T>::value;

template<typename T> struct is_time_point : std::false_type {};

template<typename T, typename E>
struct is_time_point<temp_time::TimePoint<T, E>> : std::true_type {};

template<typename T>
constexpr bool is_time_point_v = is_time_point<T>::value;

} // namespace fizmo

#endif // FIZMO_TIME_DATE_POINT_CLASSES_HPP