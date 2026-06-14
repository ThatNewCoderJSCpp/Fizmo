#ifndef FIZMO_TIME_DURATION_CLASS_HPP
#define FIZMO_TIME_DURATION_CLASS_HPP

#include "time_unit.hpp"
#include <sstream>

namespace fizmo {
namespace temp_time {

namespace detail {

template <typename ValueType>
static constexpr bool is_signed_int_like_v = is_integer_like_v<ValueType> && !is_unsigned_integer_like_v<ValueType>;

} // namespace detail

template<Unit Tag, typename ValueType, typename = typename std::enable_if<detail::is_signed_int_like_v<ValueType>>::type>
class Duration {
public:
    using value_type = ValueType;
    using traits     = unit_traits<Tag>;
    static constexpr Unit tag = Tag;

private:
    ValueType m_value;

public:
    constexpr Duration() noexcept : m_value(0) {}
    constexpr explicit Duration(const ValueType& v) noexcept : m_value(v) {}

    template<typename U, typename = typename std::enable_if<detail::is_integer_like_v<U> && !std::is_same<U, ValueType>::value>::type>
    constexpr explicit Duration(const U& v) noexcept : m_value(static_cast<ValueType>(v)) {}

    constexpr Duration(const Duration&) noexcept = default;
    constexpr Duration(Duration&&) noexcept = default;
    constexpr Duration& operator=(const Duration&) noexcept = default;
    constexpr Duration& operator=(Duration&&) noexcept = default;

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr explicit Duration(const TimeUnit<OTag, OV>& tu) noexcept { m_value = convert_from(tu); }

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr Duration& operator=(const TimeUnit<OTag, OV>& tu) noexcept {
        m_value = convert_from(tu);
        return *this;
    }

    template<Unit OTag, typename OV>
    constexpr explicit Duration(const Duration<OTag, OV>& other) noexcept : m_value(convert_from(other)) {}

    template<Unit OTag, typename OV>
    OPTIONAL_CPP14_CONSTEXPR Duration& operator=(const Duration<OTag, OV>& other) noexcept {
        m_value = convert_from(other);
        return *this;
    }

    constexpr const ValueType& count() const noexcept { return m_value; }
    constexpr       ValueType& count()       noexcept { return m_value; }
    static constexpr Duration zero() noexcept { return Duration(ValueType(0)); }

    constexpr Duration& operator+=(const Duration& rhs) noexcept {
        m_value += rhs.m_value;
        return *this;
    }
    constexpr Duration& operator-=(const Duration& rhs) noexcept {
        m_value -= rhs.m_value;
        return *this;
    }
    constexpr Duration& operator*=(const Duration& rhs) noexcept {
        m_value *= rhs.m_value;
        return *this;
    }
    constexpr Duration& operator/=(const Duration& rhs) noexcept {
        m_value /= rhs.m_value;
        return *this;
    }
    constexpr Duration& operator%=(const Duration& rhs) noexcept {
        m_value %= rhs.m_value;
        return *this;
    }
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr Duration& operator+=(const T& rhs) noexcept {
        m_value += static_cast<ValueType>(rhs);
        return *this;
    }
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr Duration& operator-=(const T& rhs) noexcept {
        m_value -= static_cast<ValueType>(rhs);
        return *this;
    }
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr Duration& operator*=(const T& rhs) noexcept {
        m_value *= static_cast<ValueType>(rhs);
        return *this;
    }
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr Duration& operator/=(const T& rhs) noexcept {
        m_value /= static_cast<ValueType>(rhs);
        return *this;
    }
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr Duration& operator%=(const T& rhs) noexcept {
        m_value %= static_cast<ValueType>(rhs);
        return *this;
    }
    constexpr Duration& operator++() noexcept {
        ++m_value;
        return *this;
    }
    constexpr Duration operator++(int) noexcept {
        Duration tmp(*this);
        ++(*this);
        return tmp;
    }
    constexpr Duration& operator--() noexcept {
        --m_value;
        return *this;
    }
    constexpr Duration operator--(int) noexcept {
        Duration tmp(*this);
        --(*this);
        return tmp;
    }
    friend constexpr Duration operator+(Duration lhs, const Duration& rhs) noexcept { return lhs += rhs; }
    friend constexpr Duration operator-(Duration lhs, const Duration& rhs) noexcept { return lhs -= rhs; }
    friend constexpr Duration operator*(Duration lhs, const Duration& rhs) noexcept { return lhs *= rhs; }
    friend constexpr Duration operator/(Duration lhs, const Duration& rhs) noexcept { return lhs /= rhs; }
    friend constexpr Duration operator%(Duration lhs, const Duration& rhs) noexcept { return lhs %= rhs; }
    friend constexpr bool operator==(const Duration& a, const Duration& b) noexcept { return a.m_value == b.m_value; }
    friend constexpr bool operator!=(const Duration& a, const Duration& b) noexcept { return a.m_value != b.m_value; }
    friend constexpr bool operator<(const Duration& a, const Duration& b) noexcept { return a.m_value < b.m_value; }
    friend constexpr bool operator<=(const Duration& a, const Duration& b) noexcept { return a.m_value <= b.m_value; }
    friend constexpr bool operator>(const Duration& a, const Duration& b) noexcept { return a.m_value > b.m_value; }
    friend constexpr bool operator>=(const Duration& a, const Duration& b) noexcept { return a.m_value >= b.m_value; }

    std::string to_string(LabelStyle style = LabelStyle::none) const {
        std::ostringstream oss;
        oss << m_value;
        switch (style) {
            case LabelStyle::none: break;
            case LabelStyle::abbrev:   oss << " " << traits::abbrev(); break;
            case LabelStyle::singular: oss << " " << traits::name();   break;
            case LabelStyle::plural:   oss << " " << traits::plural(); break;
        }
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const Duration& d) { return os << d.to_string(LabelStyle::none); }

private:
    template<Unit OTag, typename OV>
    static constexpr ValueType convert_from(const TimeUnit<OTag, OV>& other) noexcept {
        using W1 = detail::wider_t<ValueType, OV>;
        using W2 = detail::wider_t<W1, multiprecision::uint256>;
        const W2 planck = static_cast<W2>(other.count()) * static_cast<W2>(unit_traits<OTag>::planck_per_unit());
        const W2 result = planck / static_cast<W2>(traits::planck_per_unit());
        return static_cast<ValueType>(result);
    }

    template <Unit OTag, typename OV>
    static constexpr ValueType convert_from(const Duration<OTag, OV>& other) noexcept {
        using W1 = detail::wider_t<ValueType, OV>;
        using W2 = detail::wider_t<W1, multiprecision::int256>;
        const W2 raw = static_cast<W2>(other.count());
        const W2 planck = raw * static_cast<W2>(unit_traits<OTag>::planck_per_unit());
        const W2 result = planck / static_cast<W2>(traits::planck_per_unit());
        return static_cast<ValueType>(result);
    }
};

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr TimeUnit<Tag,V> operator+(TimeUnit<Tag,V> lhs, const Duration<DTag,DV>& rhs) noexcept {
    using W = detail::wider_t<V, DV>;
    W wide = static_cast<W>(lhs.count()) + static_cast<W>(rhs.count());
    if (wide < 0) wide = 0;
    if (wide > detail::value_limits<V>::max()) wide = detail::value_limits<V>::max();
    lhs.count() = static_cast<V>(wide);
    return lhs;
}

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr TimeUnit<Tag,V> operator+(const Duration<DTag,DV>& rhs, TimeUnit<Tag,V> lhs) noexcept { return lhs + rhs; }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr TimeUnit<Tag,V> operator-(TimeUnit<Tag,V> lhs, const Duration<DTag,DV>& rhs) noexcept {
    using W = detail::wider_t<V, DV>;
    W wide = static_cast<W>(lhs.count()) - static_cast<W>(rhs.count());
    if (wide < 0) wide = 0;
    if (wide > detail::value_limits<V>::max()) wide = detail::value_limits<V>::max();
    lhs.count() = static_cast<V>(wide);
    return lhs;
}

template<Unit Tag, typename V>
constexpr Duration<Tag, fizmo_make_signed_t<V>>
operator-(const TimeUnit<Tag,V>& a, const TimeUnit<Tag,V>& b) noexcept {
    using Signed = fizmo_make_signed_t<V>;
    return Duration<Tag, Signed>(static_cast<Signed>(a.count()) - static_cast<Signed>(b.count()));
}

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator==(const TimeUnit<Tag,V>& tu, const Duration<DTag,DV>& du) noexcept { return Duration<Tag, DV>(tu).count() == du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator!=(const TimeUnit<Tag,V>& tu, const Duration<DTag,DV>& du) noexcept { return !(tu == du); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator<(const TimeUnit<Tag,V>& tu, const Duration<DTag,DV>& du) noexcept { return Duration<Tag, DV>(tu).count() < du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator<=(const TimeUnit<Tag,V>& tu, const Duration<DTag,DV>& du) noexcept { return Duration<Tag, DV>(tu).count() <= du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator>(const TimeUnit<Tag,V>& tu, const Duration<DTag,DV>& du) noexcept { return Duration<Tag, DV>(tu).count() > du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator>=(const TimeUnit<Tag,V>& tu, const Duration<DTag,DV>& du) noexcept { return Duration<Tag, DV>(tu).count() >= du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator==(const Duration<DTag,DV>& du, const TimeUnit<Tag,V>& tu) noexcept { return tu == du; }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator!=(const Duration<DTag,DV>& du, const TimeUnit<Tag,V>& tu) noexcept { return !(tu == du); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator<(const Duration<DTag,DV>& du, const TimeUnit<Tag,V>& tu) noexcept { return Duration<Tag, DV>(tu).count() > du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator<=(const Duration<DTag,DV>& du, const TimeUnit<Tag,V>& tu) noexcept { return Duration<Tag, DV>(tu).count() >= du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator>(const Duration<DTag,DV>& du, const TimeUnit<Tag,V>& tu) noexcept { return Duration<Tag, DV>(tu).count() < du.count(); }

template<Unit Tag, typename V, Unit DTag, typename DV>
constexpr bool operator>=(const Duration<DTag,DV>& du, const TimeUnit<Tag,V>& tu) noexcept { return Duration<Tag, DV>(tu).count() <= du.count(); }

template<Unit Tag, typename V = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<V>>::type>
using duration_t = Duration<Tag, V>;

template<typename T> struct is_duration : std::false_type {};
template<Unit Tag, typename V> struct is_duration<Duration<Tag, V>> : std::true_type {};

template<typename T>
constexpr bool is_duration_v = is_duration<T>::value;

#define FIZMO_DEFINE_DURATION(NAME, TAG)                                    \
    template<typename V = multiprecision::int256,                           \
             typename = typename std::enable_if<                            \
                 detail::is_signed_int_like_v<V>>::type>                    \
    using NAME##_duration_t = duration_t<Unit::TAG, V>;                     \
                                                                            \
    using NAME##_duration = duration_t<Unit::TAG>;

FIZMO_DEFINE_DURATION(planck_second, planck_second)
FIZMO_DEFINE_DURATION(quectosecond,  quectosecond)
FIZMO_DEFINE_DURATION(rontosecond,   rontosecond)
FIZMO_DEFINE_DURATION(yoctosecond,   yoctosecond)
FIZMO_DEFINE_DURATION(zeptosecond,   zeptosecond)
FIZMO_DEFINE_DURATION(attosecond,    attosecond)
FIZMO_DEFINE_DURATION(femtosecond,   femtosecond)
FIZMO_DEFINE_DURATION(picosecond,    picosecond)
FIZMO_DEFINE_DURATION(nanosecond,    nanosecond)
FIZMO_DEFINE_DURATION(microsecond,   microsecond)
FIZMO_DEFINE_DURATION(millisecond,   millisecond)
FIZMO_DEFINE_DURATION(centisecond,   centisecond)
FIZMO_DEFINE_DURATION(decisecond,    decisecond)
FIZMO_DEFINE_DURATION(second,        second)
FIZMO_DEFINE_DURATION(minute,        minute)
FIZMO_DEFINE_DURATION(hour,          hour)
FIZMO_DEFINE_DURATION(day,           day)
FIZMO_DEFINE_DURATION(week,          week)
FIZMO_DEFINE_DURATION(month,         month)
FIZMO_DEFINE_DURATION(year,          year)
FIZMO_DEFINE_DURATION(decade,        decade)
FIZMO_DEFINE_DURATION(century,       century)
FIZMO_DEFINE_DURATION(millennium,    millennium)

template<typename To, Unit FromTag, typename FromV, typename = typename std::enable_if<is_duration<To>::value>::type>
constexpr To duration_cast(const Duration<FromTag, FromV>& from) noexcept {
    return static_cast<To>(from);
}

} // namespace temp_time
} // namespace fizmo

#endif // FIZMO_TIME_DURATION_CLASS_HPP