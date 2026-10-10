#ifndef FIZMO_CALENDAR_UNITS_CLASSES_HPP
#define FIZMO_CALENDAR_UNITS_CLASSES_HPP

#include "points.hpp"

namespace fizmo {
namespace time {

template <typename T = default_wide_int, typename = typename std::enable_if<detail::is_signed_integer_like_v<T>>::type> 
class DateTimeDifference;

template <typename T = default_wide_int, typename = typename std::enable_if<detail::is_signed_integer_like_v<T>>::type>
class DateTime {
public:
    using value_type = T;
    using calendar_type = CalendarPoint<T>;
    using time_type = TimePoint<T>;
    using difference_type = DateTimeDifference<T>;

private:
    T m_days;   
    T m_planck; 

    static constexpr T ppd() noexcept { return planck_per_unit<T>(Unit::day); }

    static constexpr T floor_div(const T& a, const T& b) noexcept {
        T q = a / b;
        T r = a - q * b;
        if ((r != T(0)) && ((r < T(0)) != (b < T(0)))) --q;
        return q;
    }

    static constexpr T floor_mod(const T& a, const T& b) noexcept {
        return a - floor_div(a, b) * b;
    }

    constexpr void normalize() noexcept {
        T carry = floor_div(m_planck, ppd());
        m_planck = floor_mod(m_planck, ppd());
        m_days += carry;
    }

public:
    constexpr DateTime() noexcept : m_days(T(0)), m_planck(T(0)) {}
    constexpr DateTime(const calendar_type& cal, const time_type& tp) noexcept : m_days(cal.raw_days()), m_planck(tp.raw_planck()) { normalize(); }

    constexpr DateTime(
        const T& year, Month month, std::uint8_t day,
        const T& hours       = T(0),
        std::uint8_t minutes = 0,
        std::uint8_t seconds = 0,
        std::uint16_t ms     = 0,
        std::uint16_t us     = 0,
        std::uint16_t ns     = 0
    ) noexcept
        : m_days(calendar_type(year, month, day).raw_days())
        , m_planck(time_type(hours, minutes, seconds, ms, us, ns).raw_planck())
    { normalize(); }

    constexpr DateTime(const T& raw_days, const T& raw_planck) noexcept : m_days(raw_days), m_planck(raw_planck) { normalize(); }

    constexpr DateTime(const DateTime&) noexcept            = default;
    constexpr DateTime(DateTime&&) noexcept                 = default;
    constexpr DateTime& operator=(const DateTime&) noexcept = default;
    constexpr DateTime& operator=(DateTime&&) noexcept      = default;

    constexpr calendar_type calendar() const noexcept { return calendar_type(m_days); }
    constexpr time_type     time()     const noexcept { return time_type(m_planck); }

    constexpr const T& raw_days()   const noexcept { return m_days; }
    constexpr       T& raw_days()         noexcept { return m_days; }
    constexpr const T& raw_planck() const noexcept { return m_planck; }
    constexpr       T& raw_planck()       noexcept { return m_planck; }

    constexpr T              year()        const noexcept { return calendar().year(); }
    constexpr Month          month()       const noexcept { return calendar().month(); }
    constexpr std::uint8_t   day()         const noexcept { return calendar().day(); }
    constexpr Weekday        weekday()     const noexcept { return calendar().weekday(); }
    constexpr T              hour()        const noexcept { return time().hour(); }
    constexpr std::uint8_t   minute()      const noexcept { return time().minute(); }
    constexpr std::uint8_t   second()      const noexcept { return time().second(); }
    constexpr std::uint16_t  millisecond() const noexcept { return time().millisecond(); }
    constexpr std::uint16_t  microsecond() const noexcept { return time().microsecond(); }
    constexpr std::uint16_t  nanosecond()  const noexcept { return time().nanosecond(); }

    constexpr bool          is_leap_year()          const noexcept { return calendar().is_leap_year(); }
    constexpr std::uint8_t  days_in_current_month() const noexcept { return calendar().days_in_current_month(); }
    constexpr std::uint16_t days_in_current_year()  const noexcept { return calendar().days_in_current_year(); }

    constexpr T to_total_planck() const noexcept { return m_days * ppd() + m_planck; }

    template<Unit U>
    constexpr Duration<U, T> total_to() const noexcept { return Duration<U, T>(floor_div(m_days * ppd() + m_planck, planck_per_unit<T>(U))); }
    
    constexpr T total_to(Unit u) const noexcept { return floor_div(m_days * ppd() + m_planck, planck_per_unit<T>(u)); }

    static constexpr DateTime from_total_planck(const T& total) noexcept {
        DateTime dt;
        dt.m_days   = floor_div(total, ppd());
        dt.m_planck = floor_mod(total, ppd());
        return dt;
    }

    static constexpr DateTime unix_epoch() noexcept { return DateTime(calendar_type::unix_epoch(), time_type()); }
    static constexpr DateTime windows_epoch() noexcept { return DateTime(calendar_type::windows_epoch(), time_type()); }
    static constexpr DateTime gregorian_start() noexcept { return DateTime(calendar_type::gregorian_start(), time_type()); }

    constexpr DateTime& add_days(const T& n) noexcept {
        m_days += n;
        return *this;
    }

    constexpr DateTime& add_months(const T& n) noexcept {
        auto cal = calendar();
        cal.add_months(n);
        m_days = cal.raw_days();
        return *this;
    }

    constexpr DateTime& add_years(const T& n) noexcept {
        auto cal = calendar();
        cal.add_years(n);
        m_days = cal.raw_days();
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    constexpr DateTime& operator+=(const Duration<DTag, DV>& rhs) noexcept {
        m_planck += static_cast<T>(rhs.count()) * planck_per_unit<T>(DTag);
        normalize();
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    constexpr DateTime& operator-=(const Duration<DTag, DV>& rhs) noexcept {
        m_planck -= static_cast<T>(rhs.count()) * planck_per_unit<T>(DTag);
        normalize();
        return *this;
    }

    template<typename DV>
    constexpr DateTime& operator+=(const Duration<Unit::day, DV>& rhs) noexcept {
        m_days += static_cast<T>(rhs.count());
        return *this;
    }

    template<typename DV>
    constexpr DateTime& operator-=(const Duration<Unit::day, DV>& rhs) noexcept {
        m_days -= static_cast<T>(rhs.count());
        return *this;
    }

    template<typename DV>
    constexpr DateTime& operator+=(const Duration<Unit::week, DV>& rhs) noexcept {
        m_days += static_cast<T>(rhs.count()) * T(7);
        return *this;
    }

    template<typename DV>
    constexpr DateTime& operator-=(const Duration<Unit::week, DV>& rhs) noexcept {
        m_days -= static_cast<T>(rhs.count()) * T(7);
        return *this;
    }

    template<
        Unit DTag, typename DV,
        typename = typename std::enable_if<is_month_based_unit_v<DTag>>::type,
        typename = void
    >
    constexpr DateTime& operator+=(const Duration<DTag, DV>& rhs) noexcept {
        return add_months(static_cast<T>(rhs.count()) * T(months_per_unit_v<DTag>));
    }

    template<
        Unit DTag, typename DV,
        typename = typename std::enable_if<is_month_based_unit_v<DTag>>::type,
        typename = void
    >
    constexpr DateTime& operator-=(const Duration<DTag, DV>& rhs) noexcept {
        return add_months(-(static_cast<T>(rhs.count()) * T(months_per_unit_v<DTag>)));
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr DateTime operator+(DateTime lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs += rhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr DateTime operator+(const Duration<DTag, DV>& lhs, DateTime rhs) noexcept { return rhs += lhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr DateTime operator-(DateTime lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs -= rhs; }

    template<typename DV>
    friend constexpr DateTime operator+(DateTime lhs, const Duration<Unit::day, DV>& rhs) noexcept { return lhs += rhs; }

    template<typename DV>
    friend constexpr DateTime operator+(const Duration<Unit::day, DV>& lhs, DateTime rhs) noexcept { return rhs += lhs; }

    template<typename DV>
    friend constexpr DateTime operator-(DateTime lhs, const Duration<Unit::day, DV>& rhs) noexcept { return lhs -= rhs; }

    template<typename DV>
    friend constexpr DateTime operator+(DateTime lhs, const Duration<Unit::week, DV>& rhs) noexcept { return lhs += rhs; }

    template<typename DV>
    friend constexpr DateTime operator+(const Duration<Unit::week, DV>& lhs, DateTime rhs) noexcept { return rhs += lhs; }

    template<typename DV>
    friend constexpr DateTime operator-(DateTime lhs, const Duration<Unit::week, DV>& rhs) noexcept { return lhs -= rhs; }

    template<
        Unit DTag, typename DV,
        typename = typename std::enable_if<is_month_based_unit_v<DTag>>::type,
        typename = void
    >
    friend constexpr DateTime operator+(DateTime lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs += rhs; }

    template<
        Unit DTag, typename DV,
        typename = typename std::enable_if<is_month_based_unit_v<DTag>>::type,
        typename = void
    >
    friend constexpr DateTime operator+(const Duration<DTag, DV>& lhs, DateTime rhs) noexcept { return rhs += lhs; }

    template<
        Unit DTag, typename DV,
        typename = typename std::enable_if<is_month_based_unit_v<DTag>>::type,
        typename = void
    >
    friend constexpr DateTime operator-(DateTime lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs -= rhs; }

    inline constexpr difference_type operator-(const DateTime& other) const noexcept;
    inline constexpr difference_type difference(const DateTime& other) const noexcept;

    constexpr DateTime& operator++() noexcept {
        ++m_planck;
        if (m_planck >= ppd()) { m_planck = T(0); ++m_days; }
        return *this;
    }

    constexpr DateTime operator++(int) noexcept { DateTime t(*this); ++(*this); return t; }

    constexpr DateTime& operator--() noexcept {
        if (m_planck == T(0)) { m_planck = ppd() - T(1); --m_days; }
        else --m_planck;
        return *this;
    }

    constexpr DateTime operator--(int) noexcept { DateTime t(*this); --(*this); return t; }

    friend constexpr bool operator==(const DateTime& a, const DateTime& b) noexcept { return a.m_days == b.m_days && a.m_planck == b.m_planck; }
    friend constexpr bool operator!=(const DateTime& a, const DateTime& b) noexcept { return !(a == b); }
    friend constexpr bool operator<(const DateTime& a, const DateTime& b) noexcept { return a.m_days < b.m_days || (a.m_days == b.m_days && a.m_planck < b.m_planck); }
    friend constexpr bool operator<=(const DateTime& a, const DateTime& b) noexcept { return !(b < a); }
    friend constexpr bool operator>(const DateTime& a, const DateTime& b) noexcept { return b < a; }
    friend constexpr bool operator>=(const DateTime& a, const DateTime& b) noexcept { return !(a < b); }

    template<typename U, typename E>
    friend constexpr bool operator==(const DateTime& a, const DateTime<U, E>& b) noexcept {
        return a.m_days == static_cast<T>(b.raw_days()) && a.m_planck == static_cast<T>(b.raw_planck());
    }

    template<typename U, typename E>
    friend constexpr bool operator!=(const DateTime& a, const DateTime<U, E>& b) noexcept {
        return !(a == b);
    }

    template<typename U, typename E>
    friend constexpr bool operator<(const DateTime& a, const DateTime<U, E>& b) noexcept {
        T bd = static_cast<T>(b.raw_days());
        return a.m_days < bd || (a.m_days == bd && a.m_planck < static_cast<T>(b.raw_planck()));
    }

    template<typename U, typename E>
    friend constexpr bool operator<=(const DateTime& a, const DateTime<U, E>& b) noexcept {
        T bd = static_cast<T>(b.raw_days());
        return a.m_days < bd || (a.m_days == bd && a.m_planck <= static_cast<T>(b.raw_planck()));
    }

    template<typename U, typename E>
    friend constexpr bool operator>(const DateTime& a, const DateTime<U, E>& b) noexcept {
        T bd = static_cast<T>(b.raw_days());
        return a.m_days > bd || (a.m_days == bd && a.m_planck > static_cast<T>(b.raw_planck()));
    }

    template<typename U, typename E>
    friend constexpr bool operator>=(const DateTime& a, const DateTime<U, E>& b) noexcept {
        T bd = static_cast<T>(b.raw_days());
        return a.m_days > bd || (a.m_days == bd && a.m_planck >= static_cast<T>(b.raw_planck()));
    }

private:
    template<Unit U>
    static constexpr bool contains_unit() noexcept { return false; }

    template<Unit U, Unit First, Unit... Rest>
    static constexpr bool contains_unit() noexcept { return U == First || contains_unit<U, Rest...>(); }

    template<Unit U>
    static void emit_value(std::ostringstream& oss, const T& val, LabelStyle style, bool& first) {
        if (!first) oss << " ";
        first = false;
        oss << val;

        switch (style) {
            case LabelStyle::none: break;
            case LabelStyle::abbrev:   oss << " " << unit_traits<U>::abbrev(); break;
            case LabelStyle::singular: oss << " " << unit_traits<U>::name();   break;
            case LabelStyle::plural:   oss << " " << unit_traits<U>::plural(); break;
        }
    }

    template<Unit U, typename V, typename = typename std::enable_if<!std::is_same<V, T>::value>::type>
    static void emit_value(std::ostringstream& oss, V val, LabelStyle style, bool& first) {
        emit_value<U>(oss, T(val), style, first);
    }

    static void emit_value_runtime(Unit u, std::ostringstream& oss, const T& val, LabelStyle style, bool& first) {
        switch (u) {
            case Unit::planck_second: emit_value<Unit::planck_second>(oss, val, style, first); break;
            case Unit::quectosecond:  emit_value<Unit::quectosecond>(oss, val, style, first);  break;
            case Unit::rontosecond:   emit_value<Unit::rontosecond>(oss, val, style, first);   break;
            case Unit::yoctosecond:   emit_value<Unit::yoctosecond>(oss, val, style, first);   break;
            case Unit::zeptosecond:   emit_value<Unit::zeptosecond>(oss, val, style, first);   break;
            case Unit::attosecond:    emit_value<Unit::attosecond>(oss, val, style, first);    break;
            case Unit::femtosecond:   emit_value<Unit::femtosecond>(oss, val, style, first);   break;
            case Unit::picosecond:    emit_value<Unit::picosecond>(oss, val, style, first);    break;
            case Unit::nanosecond:    emit_value<Unit::nanosecond>(oss, val, style, first);    break;
            case Unit::microsecond:   emit_value<Unit::microsecond>(oss, val, style, first);   break;
            case Unit::millisecond:   emit_value<Unit::millisecond>(oss, val, style, first);   break;
            case Unit::centisecond:   emit_value<Unit::centisecond>(oss, val, style, first);   break;
            case Unit::decisecond:    emit_value<Unit::decisecond>(oss, val, style, first);    break;
            case Unit::second:        emit_value<Unit::second>(oss, val, style, first);        break;
            case Unit::minute:        emit_value<Unit::minute>(oss, val, style, first);        break;
            case Unit::hour:          emit_value<Unit::hour>(oss, val, style, first);          break;
            case Unit::day:           emit_value<Unit::day>(oss, val, style, first);           break;
            case Unit::week:          emit_value<Unit::week>(oss, val, style, first);          break;
            case Unit::month:         emit_value<Unit::month>(oss, val, style, first);         break;
            case Unit::year:          emit_value<Unit::year>(oss, val, style, first);          break;
            case Unit::decade:        emit_value<Unit::decade>(oss, val, style, first);        break;
            case Unit::century:       emit_value<Unit::century>(oss, val, style, first);       break;
            case Unit::millennium:    emit_value<Unit::millennium>(oss, val, style, first);    break;
        }
    }

    static bool has_unit(const Unit* units, std::size_t n, Unit u) noexcept {
        for (std::size_t i = 0; i < n; ++i) { if (units[i] == u) return true; }
        return false;
    }

    std::string to_string_impl(const Unit* units, std::size_t n, LabelStyle style) const {
        const bool HAS_MIL = has_unit(units, n, Unit::millennium);
        const bool HAS_CEN = has_unit(units, n, Unit::century);
        const bool HAS_DEC = has_unit(units, n, Unit::decade);
        const bool HAS_YR  = has_unit(units, n, Unit::year);
        const bool HAS_MO  = has_unit(units, n, Unit::month);
        const bool HAS_WK  = has_unit(units, n, Unit::week);
        const bool HAS_DY  = has_unit(units, n, Unit::day);
        const bool ANY_YEAR_LEVEL = HAS_MIL || HAS_CEN || HAS_DEC || HAS_YR;
        const bool ANY_CALENDAR   = ANY_YEAR_LEVEL || HAS_MO || HAS_WK || HAS_DY;

        auto c = calendar().to_civil();
        std::ostringstream oss;
        bool first = true;
        T yr = c.year;

        if (HAS_MIL) {
            T mil = floor_div(yr, T(1000));
            yr -= mil * T(1000);
            emit_value<Unit::millennium>(oss, mil, style, first);
        }
        if (HAS_CEN) {
            T cen = floor_div(yr, T(100));
            yr -= cen * T(100);
            emit_value<Unit::century>(oss, cen, style, first);
        }
        if (HAS_DEC) {
            T dec = floor_div(yr, T(10));
            yr -= dec * T(10);
            emit_value<Unit::decade>(oss, dec, style, first);
        }
        if (HAS_YR) {
            T yr_out = (HAS_MIL || HAS_CEN || HAS_DEC) ? yr : c.year;
            emit_value<Unit::year>(oss, yr_out, style, first);
        }

        if (HAS_MO) {
            if (ANY_YEAR_LEVEL) {
                emit_value<Unit::month>(oss, static_cast<int>(c.month), style, first);
            } else {
                T total_mo = (c.year - T(1)) * T(12) + T(c.month);
                emit_value<Unit::month>(oss, total_mo, style, first);
            }
        }

        if (HAS_WK || HAS_DY) {
            T day_count(0);

            if (HAS_MO) {
                day_count = T(c.day);
            } else if (ANY_YEAR_LEVEL) {
                auto jan1 = calendar_type(c.year, Month(Months::january), 1);
                day_count = m_days - jan1.raw_days() + T(1);
            } else {
                day_count = m_days;
            }

            if (HAS_WK) {
                T weeks(0);
                if (HAS_MO || ANY_YEAR_LEVEL) {
                    weeks     = (day_count - T(1)) / T(7);
                    day_count = (day_count - T(1)) % T(7) + T(1);
                } else {
                    weeks     = day_count / T(7);
                    day_count = day_count % T(7);
                }
                emit_value<Unit::week>(oss, weeks, style, first);
            }

            if (HAS_DY) { emit_value<Unit::day>(oss, day_count, style, first); }
        }

        T rem = m_planck;
        if (!ANY_CALENDAR) { rem = m_days * ppd() + m_planck; }

        constexpr Unit time_units[] = {
            Unit::hour, Unit::minute, Unit::second,
            Unit::decisecond, Unit::centisecond, Unit::millisecond,
            Unit::microsecond, Unit::nanosecond, Unit::picosecond,
            Unit::femtosecond, Unit::attosecond, Unit::zeptosecond,
            Unit::yoctosecond, Unit::rontosecond, Unit::quectosecond
        };

        for (Unit u : time_units) {
            if (!has_unit(units, n, u)) continue;
            T ppu = planck_per_unit<T>(u);
            T val = floor_div(rem, ppu);
            rem   = floor_mod(rem, ppu);
            emit_value_runtime(u, oss, val, style, first);
        }

        if (has_unit(units, n, Unit::planck_second)) { emit_value<Unit::planck_second>(oss, rem, style, first); }
        return oss.str();
    }

public:
    std::string to_string() const {
        auto c = calendar().to_civil();
        auto t = time().to_civil();
        std::ostringstream oss;

        oss << month_name(static_cast<Months>(c.month - 1))
            << " " << static_cast<int>(c.day)
            << ", " << c.year
            << " at ";

        if (t.hours < T(0)) oss << "-";
        T abs_h = t.hours < T(0) ? -t.hours : t.hours;
        if (abs_h < T(10)) oss << "0";
        oss << abs_h;
        oss << ":" << (t.minutes < 10 ? "0" : "") << static_cast<int>(t.minutes) << ":" << (t.seconds < 10 ? "0" : "") << static_cast<int>(t.seconds);
        bool has_ns  = t.nanoseconds != 0 || t.sub_nanosecond_planck != T(0);
        bool has_us  = t.microseconds != 0 || has_ns;
        bool has_ms  = t.milliseconds != 0 || has_us;

        if (has_ms) {
            oss << ".";
            if (t.milliseconds < 100) oss << "0";
            if (t.milliseconds < 10)  oss << "0";
            oss << static_cast<int>(t.milliseconds);

            if (has_us) {
                if (t.microseconds < 100) oss << "0";
                if (t.microseconds < 10)  oss << "0";
                oss << static_cast<int>(t.microseconds);

                if (has_ns) {
                    if (t.nanoseconds < 100) oss << "0";
                    if (t.nanoseconds < 10)  oss << "0";
                    oss << static_cast<int>(t.nanoseconds);
                }
            }
        }

        return oss.str();
    }

    std::string to_iso_string() const { return calendar().to_iso_string() + "T" + time().to_iso_string(); }
    friend std::ostream& operator<<(std::ostream& os, const DateTime& dt) { return os << dt.to_string(); }

    std::string to_string(std::initializer_list<Unit> units, LabelStyle style = LabelStyle::abbrev) const {
        return to_string_impl(units.begin(), units.size(), style);
    }

    template<typename Container, typename = typename std::enable_if<!std::is_same<typename std::decay<Container>::type, LabelStyle>::value>::type>
    std::string to_string(const Container& units, LabelStyle style = LabelStyle::abbrev) const {
        return to_string_impl(units.data(), units.size(), style);
    }

    template<Unit... Tags>
    std::string to_string(LabelStyle style = LabelStyle::abbrev) const {
        static_assert(sizeof...(Tags) > 0, "Provide at least one Unit tag");
        constexpr bool HAS_MIL = contains_unit<Unit::millennium, Tags...>();
        constexpr bool HAS_CEN = contains_unit<Unit::century,    Tags...>();
        constexpr bool HAS_DEC = contains_unit<Unit::decade,     Tags...>();
        constexpr bool HAS_YR  = contains_unit<Unit::year,       Tags...>();
        constexpr bool HAS_MO  = contains_unit<Unit::month,      Tags...>();
        constexpr bool HAS_WK  = contains_unit<Unit::week,       Tags...>();
        constexpr bool HAS_DY  = contains_unit<Unit::day,        Tags...>();
        constexpr bool HAS_HR  = contains_unit<Unit::hour,       Tags...>();
        constexpr bool HAS_MIN = contains_unit<Unit::minute,     Tags...>();
        constexpr bool HAS_SEC = contains_unit<Unit::second,     Tags...>();
        constexpr bool HAS_DS  = contains_unit<Unit::decisecond, Tags...>();
        constexpr bool HAS_CS  = contains_unit<Unit::centisecond,Tags...>();
        constexpr bool HAS_MS  = contains_unit<Unit::millisecond,Tags...>();
        constexpr bool HAS_US  = contains_unit<Unit::microsecond,Tags...>();
        constexpr bool HAS_NS  = contains_unit<Unit::nanosecond, Tags...>();
        constexpr bool HAS_PS  = contains_unit<Unit::picosecond, Tags...>();
        constexpr bool HAS_FS  = contains_unit<Unit::femtosecond,Tags...>();
        constexpr bool HAS_AS  = contains_unit<Unit::attosecond, Tags...>();
        constexpr bool HAS_ZS  = contains_unit<Unit::zeptosecond,Tags...>();
        constexpr bool HAS_YS  = contains_unit<Unit::yoctosecond,Tags...>();
        constexpr bool HAS_RS  = contains_unit<Unit::rontosecond,Tags...>();
        constexpr bool HAS_QS  = contains_unit<Unit::quectosecond,Tags...>();
        constexpr bool HAS_TP  = contains_unit<Unit::planck_second,Tags...>();
        constexpr bool ANY_YEAR_LEVEL = HAS_MIL || HAS_CEN || HAS_DEC || HAS_YR;
        constexpr bool ANY_CALENDAR   = ANY_YEAR_LEVEL || HAS_MO || HAS_WK || HAS_DY;
        auto c = calendar().to_civil();   
        std::ostringstream oss;
        bool first = true;
        T yr = c.year;

        if (HAS_MIL) {
            T mil = floor_div(yr, T(1000));
            yr -= mil * T(1000);
            emit_value<Unit::millennium>(oss, mil, style, first);
        }
        if (HAS_CEN) {
            T cen = floor_div(yr, T(100));
            yr -= cen * T(100);
            emit_value<Unit::century>(oss, cen, style, first);
        }
        if (HAS_DEC) {
            T dec = floor_div(yr, T(10));
            yr -= dec * T(10);
            emit_value<Unit::decade>(oss, dec, style, first);
        }
        if (HAS_YR) {
            T yr_out = (HAS_MIL || HAS_CEN || HAS_DEC) ? yr : c.year;
            emit_value<Unit::year>(oss, yr_out, style, first);
        }

        if (HAS_MO) {
            if (ANY_YEAR_LEVEL) {
                emit_value<Unit::month>(oss, static_cast<int>(c.month), style, first);
            } else {
                T total_mo = (c.year - T(1)) * T(12) + T(c.month);
                emit_value<Unit::month>(oss, total_mo, style, first);
            }
        }

        if (HAS_WK || HAS_DY) {
            T day_count(0);

            if (HAS_MO) {
                day_count = T(c.day);
            } else if (ANY_YEAR_LEVEL) {
                auto jan1 = calendar_type(c.year, Month(Months::january), 1);
                day_count = m_days - jan1.raw_days() + T(1);  
            } else {
                day_count = m_days;
            }

            if (HAS_WK) {
                T weeks(0);
                if (HAS_MO || ANY_YEAR_LEVEL) {
                    weeks     = (day_count - T(1)) / T(7);
                    day_count = (day_count - T(1)) % T(7) + T(1);
                } else {
                    weeks     = day_count / T(7);
                    day_count = day_count % T(7);
                }

                emit_value<Unit::week>(oss, weeks, style, first);
            }

            if (HAS_DY) { emit_value<Unit::day>(oss, day_count, style, first); }
        }

        T rem = m_planck;
        if (!ANY_CALENDAR) { rem = m_days * ppd() + m_planck; }

        #define FIZMO_DT_EMIT_TIME_UNIT(FLAG, UNIT_TAG)                    \
            if (FLAG) {                                                    \
                T val = floor_div(rem, planck_per_unit<T>(Unit::UNIT_TAG));\
                rem   = floor_mod(rem, planck_per_unit<T>(Unit::UNIT_TAG));\
                emit_value<Unit::UNIT_TAG>(oss, val, style, first);        \
            }

        FIZMO_DT_EMIT_TIME_UNIT(HAS_HR,  hour)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_MIN, minute)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_SEC, second)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_DS,  decisecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_CS,  centisecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_MS,  millisecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_US,  microsecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_NS,  nanosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_PS,  picosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_FS,  femtosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_AS,  attosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_ZS,  zeptosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_YS,  yoctosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_RS,  rontosecond)
        FIZMO_DT_EMIT_TIME_UNIT(HAS_QS,  quectosecond)
        if (HAS_TP) { emit_value<Unit::planck_second>(oss, rem, style, first); }
        #undef FIZMO_DT_EMIT_TIME_UNIT
        return oss.str();
    }
};

template <typename T, typename>
class DateTimeDifference {
public:
    using value_type = T;
    using datetime_type = DateTime<T>;

private:
    datetime_type m_dt;

    static constexpr T ppd() noexcept { return datetime_type::ppd(); }
    static constexpr T floor_div(const T& a, const T& b) noexcept { return datetime_type::floor_div(a, b); }
    static constexpr T floor_mod(const T& a, const T& b) noexcept { return datetime_type::floor_mod(a, b); }

public:
    constexpr DateTimeDifference() noexcept : m_dt() {}
    constexpr DateTimeDifference(const T& days, const T& planck) noexcept : m_dt(days, planck) {}
    constexpr explicit DateTimeDifference(const datetime_type& dt) noexcept : m_dt(dt) {}
    constexpr DateTimeDifference(const DateTimeDifference&) noexcept            = default;
    constexpr DateTimeDifference(DateTimeDifference&&) noexcept                 = default;
    constexpr DateTimeDifference& operator=(const DateTimeDifference&) noexcept = default;
    constexpr DateTimeDifference& operator=(DateTimeDifference&&) noexcept      = default;

    constexpr const T& raw_days()   const noexcept { return m_dt.raw_days(); }
    constexpr       T& raw_days()         noexcept { return m_dt.raw_days(); }
    constexpr const T& raw_planck() const noexcept { return m_dt.raw_planck(); }
    constexpr       T& raw_planck()       noexcept { return m_dt.raw_planck(); }

    constexpr const datetime_type& as_datetime() const noexcept { return m_dt; }
    constexpr       datetime_type& as_datetime()       noexcept { return m_dt; }

    constexpr T              days()        const noexcept { return m_dt.raw_days(); }
    constexpr T              hour()        const noexcept { return m_dt.hour(); }
    constexpr std::uint8_t   minute()      const noexcept { return m_dt.minute(); }
    constexpr std::uint8_t   second()      const noexcept { return m_dt.second(); }
    constexpr std::uint16_t  millisecond() const noexcept { return m_dt.millisecond(); }
    constexpr std::uint16_t  microsecond() const noexcept { return m_dt.microsecond(); }
    constexpr std::uint16_t  nanosecond()  const noexcept { return m_dt.nanosecond(); }

    constexpr T to_total_planck() const noexcept { return m_dt.to_total_planck(); }

    template<Unit U>
    constexpr Duration<U, T> total_to() const noexcept { return m_dt.template total_to<U>(); }

    constexpr T total_to(Unit u) const noexcept { return m_dt.total_to(u); }

    constexpr DateTimeDifference operator-() const noexcept { return DateTimeDifference(-raw_days(), -raw_planck()); }
    constexpr DateTimeDifference operator+() const noexcept { return *this; }

    constexpr DateTimeDifference& operator+=(const DateTimeDifference& rhs) noexcept {
        m_dt.raw_days()   += rhs.raw_days();
        m_dt.raw_planck() += rhs.raw_planck();
        m_dt.normalize();
        return *this;
    }

    constexpr DateTimeDifference& operator-=(const DateTimeDifference& rhs) noexcept {
        m_dt.raw_days()   -= rhs.raw_days();
        m_dt.raw_planck() -= rhs.raw_planck();
        m_dt.normalize();
        return *this;
    }

    friend constexpr DateTimeDifference operator+(DateTimeDifference lhs, const DateTimeDifference& rhs) noexcept { return lhs += rhs; }
    friend constexpr DateTimeDifference operator-(DateTimeDifference lhs, const DateTimeDifference& rhs) noexcept { return lhs -= rhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    constexpr DateTimeDifference& operator+=(const Duration<DTag, DV>& rhs) noexcept {
        m_dt += rhs;
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    constexpr DateTimeDifference& operator-=(const Duration<DTag, DV>& rhs) noexcept {
        m_dt -= rhs;
        return *this;
    }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr DateTimeDifference operator+(DateTimeDifference lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs += rhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr DateTimeDifference operator+(const Duration<DTag, DV>& lhs, DateTimeDifference rhs) noexcept { return rhs += lhs; }

    template<Unit DTag, typename DV, typename = typename std::enable_if<(static_cast<std::uint8_t>(DTag) <= static_cast<std::uint8_t>(Unit::hour))>::type>
    friend constexpr DateTimeDifference operator-(DateTimeDifference lhs, const Duration<DTag, DV>& rhs) noexcept { return lhs -= rhs; }

    template<typename DV>
    constexpr DateTimeDifference& operator+=(const Duration<Unit::day, DV>& rhs) noexcept { m_dt += rhs; return *this; }

    template<typename DV>
    constexpr DateTimeDifference& operator-=(const Duration<Unit::day, DV>& rhs) noexcept { m_dt -= rhs; return *this; }

    template<typename DV>
    friend constexpr DateTimeDifference operator+(DateTimeDifference lhs, const Duration<Unit::day, DV>& rhs) noexcept { return lhs += rhs; }

    template<typename DV>
    friend constexpr DateTimeDifference operator+(const Duration<Unit::day, DV>& lhs, DateTimeDifference rhs) noexcept { return rhs += lhs; }

    template<typename DV>
    friend constexpr DateTimeDifference operator-(DateTimeDifference lhs, const Duration<Unit::day, DV>& rhs) noexcept { return lhs -= rhs; }

    template<typename DV>
    constexpr DateTimeDifference& operator+=(const Duration<Unit::week, DV>& rhs) noexcept { m_dt += rhs; return *this; }

    template<typename DV>
    constexpr DateTimeDifference& operator-=(const Duration<Unit::week, DV>& rhs) noexcept { m_dt -= rhs; return *this; }

    template<typename DV>
    friend constexpr DateTimeDifference operator+(DateTimeDifference lhs, const Duration<Unit::week, DV>& rhs) noexcept { return lhs += rhs; }

    template<typename DV>
    friend constexpr DateTimeDifference operator+(const Duration<Unit::week, DV>& lhs, DateTimeDifference rhs) noexcept { return rhs += lhs; }

    template<typename DV>
    friend constexpr DateTimeDifference operator-(DateTimeDifference lhs, const Duration<Unit::week, DV>& rhs) noexcept { return lhs -= rhs; }

    template<typename S, typename = typename std::enable_if<detail::is_integer_like_v<S>>::type>
    constexpr DateTimeDifference& operator*=(const S& s) noexcept {
        T total = to_total_planck() * static_cast<T>(s);
        m_dt.raw_days()   = floor_div(total, ppd());
        m_dt.raw_planck() = floor_mod(total, ppd());
        return *this;
    }

    template<typename S, typename = typename std::enable_if<detail::is_integer_like_v<S>>::type>
    constexpr DateTimeDifference& operator/=(const S& s) noexcept {
        T total = floor_div(to_total_planck(), static_cast<T>(s));
        m_dt.raw_days()   = floor_div(total, ppd());
        m_dt.raw_planck() = floor_mod(total, ppd());
        return *this;
    }

    template<typename S, typename = typename std::enable_if<detail::is_integer_like_v<S>>::type>
    friend constexpr DateTimeDifference operator*(DateTimeDifference lhs, const S& rhs) noexcept { return lhs *= rhs; }

    template<typename S, typename = typename std::enable_if<detail::is_integer_like_v<S>>::type>
    friend constexpr DateTimeDifference operator*(const S& lhs, DateTimeDifference rhs) noexcept { return rhs *= lhs; }

    template<typename S, typename = typename std::enable_if<detail::is_integer_like_v<S>>::type>
    friend constexpr DateTimeDifference operator/(DateTimeDifference lhs, const S& rhs) noexcept { return lhs /= rhs; }

    friend constexpr bool operator==(const DateTimeDifference& a, const DateTimeDifference& b) noexcept {
        return a.raw_days() == b.raw_days() && a.raw_planck() == b.raw_planck();
    }
    friend constexpr bool operator!=(const DateTimeDifference& a, const DateTimeDifference& b) noexcept { return !(a == b); }
    friend constexpr bool operator<(const DateTimeDifference& a, const DateTimeDifference& b) noexcept {
        return a.raw_days() < b.raw_days() || (a.raw_days() == b.raw_days() && a.raw_planck() < b.raw_planck());
    }
    friend constexpr bool operator<=(const DateTimeDifference& a, const DateTimeDifference& b) noexcept { return !(b < a); }
    friend constexpr bool operator>(const DateTimeDifference& a, const DateTimeDifference& b) noexcept { return b < a; }
    friend constexpr bool operator>=(const DateTimeDifference& a, const DateTimeDifference& b) noexcept { return !(a < b); }

    std::string to_string(LabelStyle style = LabelStyle::abbrev) const {
        return m_dt.to_string(
            {Unit::day, Unit::hour, Unit::minute, Unit::second, Unit::millisecond, Unit::microsecond, Unit::nanosecond},
            style
        );
    }

    std::string to_string(std::initializer_list<Unit> units, LabelStyle style = LabelStyle::abbrev) const {
        return m_dt.to_string(units, style);
    }

    template<typename Container, typename = typename std::enable_if<!std::is_same<typename std::decay<Container>::type, LabelStyle>::value>::type>
    std::string to_string(const Container& units, LabelStyle style = LabelStyle::abbrev) const {
        return m_dt.to_string(units, style);
    }

    template<Unit... Tags>
    std::string to_string(LabelStyle style = LabelStyle::abbrev) const {
        return m_dt.template to_string<Tags...>(style);
    }

    friend std::ostream& operator<<(std::ostream& os, const DateTimeDifference& d) { return os << d.to_string(); }
};

template <typename T, typename E>
inline constexpr typename DateTime<T, E>::difference_type
DateTime<T, E>::operator-(const DateTime& other) const noexcept {
    return difference_type(m_days - other.m_days, m_planck - other.m_planck);
}

template <typename T, typename E>
inline constexpr typename DateTime<T, E>::difference_type
DateTime<T, E>::difference(const DateTime& other) const noexcept {
    return difference_type(m_days - other.m_days, m_planck - other.m_planck);
}

} // namespace time
} // namespace fizmo

#endif // FIZMO_CALENDAR_UNITS_CLASSES_HPP