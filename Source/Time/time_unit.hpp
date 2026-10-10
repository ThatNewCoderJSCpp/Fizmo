#ifndef FIZMO_TIME_UNIT_HPP
#define FIZMO_TIME_UNIT_HPP

#include "../Basic/fizmo_defines.hpp"
#include "../Standard Overloads/abs.hpp"
#include "../Multiprecision/Fixed Width int/type_traits.hpp"
#include <cstdint>
#include <limits>
#include <type_traits>
#include <string>
#include <ostream>

namespace fizmo {
namespace time {

using default_wide_int = multiprecision::int512;
using default_wide_uint = multiprecision::uint512;
using default_storage_uint = multiprecision::uint256;
using default_std_int = std::int64_t;

namespace detail {

template<typename T>
struct is_integer_like : std::integral_constant<bool, is_fizmo_static_int_v<T>> {};

template<typename T>
struct is_unsigned_integer_like : std::integral_constant<bool, is_fizmo_static_int_v<T> && is_fizmo_unsigned_v<T>> {};

template<typename T>
struct is_signed_integer_like : std::integral_constant<bool, is_fizmo_static_int_v<T> && is_fizmo_signed_v<T>> {};

template<typename T>
constexpr bool is_integer_like_v = is_integer_like<T>::value;

template<typename T>
constexpr bool is_unsigned_integer_like_v = is_unsigned_integer_like<T>::value;

template<typename T>
constexpr bool is_signed_integer_like_v = is_signed_integer_like<T>::value;

template<typename A, typename B>
using wider_t = fizmo_common_type_t<A, B>;

template<typename T>
using widen_t = fizmo_promote_t<T>;

template<typename V1, typename V2>
using comparison_wide_t = widen_t<wider_t<V1, V2>>;

template<typename V, typename = typename std::enable_if<std::is_integral<V>::value>::type>
inline std::string value_to_string(const V v) { return std::to_string(v); }

template<typename V, typename = typename std::enable_if<is_fizmo_static_int_v<V>>::type, typename = void>
inline std::string value_to_string(const V& v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

template<typename V, typename = void>
struct value_limits {
    static constexpr V max() noexcept { return std::numeric_limits<V>::max(); }
    static constexpr V zero() noexcept { return V(0); }
    static constexpr V min() noexcept { return std::numeric_limits<V>::lowest(); }
};

template<typename V>
struct value_limits<V, typename std::enable_if<is_unsigned_integer_like_v<V>>::type> {
    static constexpr V max() noexcept { return V::max(); }
    static constexpr V min() noexcept { return V(); }      
    static constexpr V zero() noexcept { return V(); }
};

template<typename V>
struct value_limits<V, typename std::enable_if<is_signed_integer_like_v<V>>::type> {
    static constexpr V max() noexcept { return V::max(); }
    static constexpr V min() noexcept { return V::min(); }
    static constexpr V zero() noexcept { return V(); }
};

constexpr default_storage_uint p10_table[] = {
    default_storage_uint(1ULL),                      // 10^0
    default_storage_uint(10ULL),                     // 10^1
    default_storage_uint(100ULL),                    // 10^2
    default_storage_uint(1000ULL),                   // 10^3
    default_storage_uint(10000ULL),                  // 10^4
    default_storage_uint(100000ULL),                 // 10^5
    default_storage_uint(1000000ULL),                // 10^6
    default_storage_uint(10000000ULL),               // 10^7
    default_storage_uint(100000000ULL),              // 10^8
    default_storage_uint(1000000000ULL),             // 10^9
    default_storage_uint(10000000000ULL),            // 10^10
    default_storage_uint(100000000000ULL),           // 10^11
    default_storage_uint(1000000000000ULL),          // 10^12
    default_storage_uint(10000000000000ULL),         // 10^13
    default_storage_uint(100000000000000ULL),        // 10^14
    default_storage_uint(1000000000000000ULL),       // 10^15
    default_storage_uint(10000000000000000ULL),      // 10^16
    default_storage_uint(100000000000000000ULL),     // 10^17
    default_storage_uint(1000000000000000000ULL),    // 10^18
    default_storage_uint(10000000000000000000ULL),   // 10^19
};

constexpr int p10_table_size = 20;

constexpr default_storage_uint p10(int n) noexcept {
    if (n < p10_table_size) return p10_table[n];
    default_storage_uint result = p10_table[p10_table_size - 1];
    for (int i = p10_table_size - 1; i < n; ++i) result = result * default_storage_uint(10);
    return result;
}

} // namespace detail

enum class Unit : std::uint8_t {
    planck_second = 0,
    quectosecond,
    rontosecond,
    yoctosecond,
    zeptosecond,
    attosecond,
    femtosecond,
    picosecond,
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
    month,
    year,
    decade,
    century,
    millennium
};

template<Unit> struct unit_traits;

#define FIZMO_DEFINE_UNIT_TRAITS(TAG, PLANCK_EXPR, COEFF_VAL, EXP_VAL,     \
                                 NAME_STR, PLURAL_STR, ABBREV_STR)          \
    template<> struct unit_traits<Unit::TAG> {                               \
        static constexpr default_storage_uint planck_per_unit() noexcept  \
            { return PLANCK_EXPR; }                                          \
        static constexpr default_storage_uint coeff() noexcept            \
            { return default_storage_uint(COEFF_VAL); }                   \
        static constexpr int exponent() noexcept { return EXP_VAL; }        \
        static constexpr const char* name()   { return NAME_STR; }          \
        static constexpr const char* plural() { return PLURAL_STR; }        \
        static constexpr const char* abbrev() { return ABBREV_STR; }        \
    };

FIZMO_DEFINE_UNIT_TRAITS(
    planck_second,
    default_storage_uint(1),
    5391247ULL, -50,
    "planck second", "planck seconds", "tP"
)

FIZMO_DEFINE_UNIT_TRAITS(
    quectosecond,
    default_storage_uint(1854870ULL) * detail::p10(7),
    1ULL, -30,
    "quectosecond", "quectoseconds", "qs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    rontosecond,
    default_storage_uint(1854870ULL) * detail::p10(10),
    1ULL, -27,
    "rontosecond", "rontoseconds", "rs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    yoctosecond,
    default_storage_uint(1854870ULL) * detail::p10(13),
    1ULL, -24,
    "yoctosecond", "yoctoseconds", "ys"
)

FIZMO_DEFINE_UNIT_TRAITS(
    zeptosecond,
    default_storage_uint(1854870ULL) * detail::p10(16),
    1ULL, -21,
    "zeptosecond", "zeptoseconds", "zs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    attosecond,
    default_storage_uint(1854870ULL) * detail::p10(19),
    1ULL, -18,
    "attosecond", "attoseconds", "as"
)

FIZMO_DEFINE_UNIT_TRAITS(
    femtosecond,
    default_storage_uint(1854870ULL) * detail::p10(22),
    1ULL, -15,
    "femtosecond", "femtoseconds", "fs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    picosecond,
    default_storage_uint(1854870ULL) * detail::p10(25),
    1ULL, -12,
    "picosecond", "picoseconds", "ps"
)

FIZMO_DEFINE_UNIT_TRAITS(
    nanosecond,
    default_storage_uint(1854870ULL) * detail::p10(28),
    1ULL, -9,
    "nanosecond", "nanoseconds", "ns"
)

FIZMO_DEFINE_UNIT_TRAITS(
    microsecond,
    default_storage_uint(1854870ULL) * detail::p10(31),
    1ULL, -6,
    "microsecond", "microseconds", "us"
)

FIZMO_DEFINE_UNIT_TRAITS(
    millisecond,
    default_storage_uint(1854870ULL) * detail::p10(34),
    1ULL, -3,
    "millisecond", "milliseconds", "ms"
)

FIZMO_DEFINE_UNIT_TRAITS(
    centisecond,
    default_storage_uint(1854870ULL) * detail::p10(35),
    1ULL, -2,
    "centisecond", "centiseconds", "cs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    decisecond,
    default_storage_uint(1854870ULL) * detail::p10(36),
    1ULL, -1,
    "decisecond", "deciseconds", "ds"
)

FIZMO_DEFINE_UNIT_TRAITS(
    second,
    default_storage_uint(1854870ULL) * detail::p10(37),
    1ULL, 0,
    "second", "seconds", "s"
)

FIZMO_DEFINE_UNIT_TRAITS(
    minute,
    default_storage_uint(111292200ULL) * detail::p10(37),
    60ULL, 0,
    "minute", "minutes", "min"
)

FIZMO_DEFINE_UNIT_TRAITS(
    hour,
    default_storage_uint(6677532000ULL) * detail::p10(37),
    3600ULL, 0,
    "hour", "hours", "hr"
)

FIZMO_DEFINE_UNIT_TRAITS(
    day,
    default_storage_uint(160260768000ULL) * detail::p10(37),
    86400ULL, 0,
    "day", "days", "d"
)

FIZMO_DEFINE_UNIT_TRAITS(
    week,
    default_storage_uint(1121825376000ULL) * detail::p10(37),
    604800ULL, 0,
    "week", "weeks", "wk"
)

FIZMO_DEFINE_UNIT_TRAITS(
    month,
    unit_traits<Unit::day>::planck_per_unit() * default_storage_uint(30),
    2592000ULL, 0,
    "month", "months", "mo"
)

FIZMO_DEFINE_UNIT_TRAITS(
    year,
    default_storage_uint(58495180320000ULL) * detail::p10(37),
    31536000ULL, 0,
    "year", "years", "yr"
)

FIZMO_DEFINE_UNIT_TRAITS(
    decade,
    default_storage_uint(584951803200000ULL) * detail::p10(37),
    315360000ULL, 0,
    "decade", "decades", "dec"
)

FIZMO_DEFINE_UNIT_TRAITS(
    century,
    default_storage_uint(5849518032000000ULL) * detail::p10(37),
    3153600000ULL, 0,
    "century", "centuries", "c"
)

FIZMO_DEFINE_UNIT_TRAITS(
    millennium,
    default_storage_uint(58495180320000000ULL) * detail::p10(37),
    31536000000ULL, 0,
    "millennium", "millennia", "mil"
)

template<Unit Tag, typename ValueType, typename = typename std::enable_if<time::detail::is_unsigned_integer_like_v<ValueType>>::type>
class TimeUnit;

} // namespace time

template<typename T> struct is_time_unit : std::false_type {};
template<time::Unit Tag, typename V> struct is_time_unit<time::TimeUnit<Tag, V>> : std::true_type {};

template<typename T>
constexpr bool is_time_unit_v = is_time_unit<T>::value;

namespace time {

enum class LabelStyle {
    none,       // "300"
    abbrev,     // "300 ms"
    singular,   // "300 millisecond"
    plural      // "300 milliseconds"
};

template<Unit Tag, typename ValueType, typename>
class TimeUnit {
private:
    ValueType m_value;

public:
    static constexpr Unit tag = Tag;
    using value_type = ValueType;
    using traits = unit_traits<Tag>;

public:
    constexpr TimeUnit() noexcept : m_value(0) {}
    constexpr explicit TimeUnit(const ValueType& v) noexcept : m_value(v) {}
    constexpr explicit TimeUnit(ValueType&& v) noexcept : m_value(std::move(v)) {}

    template<typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U> && !std::is_same<U, ValueType>::value>::type>
    constexpr explicit TimeUnit(const U& v) noexcept : m_value(static_cast<ValueType>(v)) {}

    template <typename U, typename = typename std::enable_if<std::is_integral<U>::value && std::is_unsigned<U>::value>::type, typename = void>
    constexpr explicit TimeUnit(const U v) noexcept : m_value(ValueType(v)) {}

    constexpr TimeUnit(const TimeUnit&) noexcept = default;
    constexpr TimeUnit(TimeUnit&&) noexcept = default;
    constexpr TimeUnit& operator=(const TimeUnit&) noexcept = default;
    constexpr TimeUnit& operator=(TimeUnit&&) noexcept = default;

    template<Unit OTag, typename OV, typename = typename std::enable_if<(!(OTag == Tag && std::is_same<OV, ValueType>::value))>::type>
    constexpr explicit TimeUnit(const TimeUnit<OTag, OV>& other) noexcept : m_value(convert_from(other)) {}

    template<Unit OTag, typename OV, typename = typename std::enable_if<(!(OTag == Tag && std::is_same<OV, ValueType>::value))>::type>
    constexpr TimeUnit& operator=(const TimeUnit<OTag, OV>& other) noexcept {
        m_value = convert_from(other);
        return *this;
    }

    constexpr TimeUnit& operator=(const ValueType& v) noexcept { m_value = v; return *this; }
    constexpr TimeUnit& operator=(ValueType&& v) noexcept { m_value = std::move(v); return *this; }

    template<typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U> && !std::is_same<U, ValueType>::value>::type>
    constexpr TimeUnit& operator=(const U& v) noexcept { m_value = static_cast<ValueType>(v); return *this; }

    template <typename U, typename = typename std::enable_if<std::is_integral<U>::value && std::is_unsigned<U>::value>::type, typename = void>
    constexpr TimeUnit& operator=(const U v) noexcept { m_value = ValueType(v); return *this; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }

public:
    constexpr const ValueType& count() const noexcept { return m_value; }
    constexpr       ValueType& count()       noexcept { return m_value; }

public:
    static constexpr TimeUnit zero() noexcept { return TimeUnit(detail::value_limits<ValueType>::zero()); }
    static constexpr TimeUnit max()  noexcept { return TimeUnit(detail::value_limits<ValueType>::max());  }
    static constexpr TimeUnit min()  noexcept { return TimeUnit(detail::value_limits<ValueType>::min());  }

public:
    constexpr TimeUnit& operator+=(const TimeUnit& rhs) noexcept {
        const ValueType mx = detail::value_limits<ValueType>::max();
        if (rhs.m_value > mx - m_value) m_value = mx;
        else m_value += rhs.m_value;
        return *this;
    }

    constexpr TimeUnit& operator-=(const TimeUnit& rhs) noexcept {
        if (rhs.m_value > m_value) m_value = detail::value_limits<ValueType>::zero();
        else m_value -= rhs.m_value;
        return *this;
    }

    constexpr TimeUnit& operator*=(const TimeUnit& rhs) noexcept {
        const ValueType mx = detail::value_limits<ValueType>::max();
        const ValueType z  = detail::value_limits<ValueType>::zero();
        if (m_value == z || rhs.m_value == z) { m_value = z; }
        else if (rhs.m_value > mx / m_value) { m_value = mx; }
        else { m_value *= rhs.m_value; }
        return *this;
    }

    constexpr TimeUnit& operator/=(const TimeUnit& rhs) noexcept {
        if (rhs.m_value == detail::value_limits<ValueType>::zero()) m_value = detail::value_limits<ValueType>::zero();
        else m_value /= rhs.m_value;
        return *this;
    }

    constexpr TimeUnit& operator%=(const TimeUnit& rhs) noexcept {
        if (rhs.m_value == detail::value_limits<ValueType>::zero()) m_value = detail::value_limits<ValueType>::zero();
        else m_value %= rhs.m_value;
        return *this;
    }

public:
    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr TimeUnit& operator+=(const TimeUnit<OTag, OV>& rhs) noexcept { return *this += TimeUnit(convert_from(rhs)); }

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr TimeUnit& operator-=(const TimeUnit<OTag, OV>& rhs) noexcept { return *this -= TimeUnit(convert_from(rhs)); }

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr TimeUnit& operator*=(const TimeUnit<OTag, OV>& rhs) noexcept { return *this *= TimeUnit(convert_from(rhs)); }

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr TimeUnit& operator/=(const TimeUnit<OTag, OV>& rhs) noexcept { return *this /= TimeUnit(convert_from(rhs)); }

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr TimeUnit& operator%=(const TimeUnit<OTag, OV>& rhs) noexcept { return *this %= TimeUnit(convert_from(rhs)); }

public:
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr TimeUnit& operator+=(const T& rhs) noexcept {
        const ValueType v  = static_cast<ValueType>(rhs);
        const ValueType mx = detail::value_limits<ValueType>::max();
        if (v > mx - m_value) m_value = mx;
        else m_value += v;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr TimeUnit& operator-=(const T& rhs) noexcept {
        const ValueType v = static_cast<ValueType>(rhs);
        if (v > m_value) m_value = detail::value_limits<ValueType>::zero();
        else m_value -= v;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr TimeUnit& operator*=(const T& rhs) noexcept {
        const ValueType v  = static_cast<ValueType>(rhs);
        const ValueType mx = detail::value_limits<ValueType>::max();
        const ValueType z  = detail::value_limits<ValueType>::zero();
        if (m_value == z || v == z) { m_value = z; }
        else if (v > mx / m_value) { m_value = mx; }
        else { m_value *= v; }
        return *this;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr TimeUnit& operator/=(const T& rhs) noexcept {
        const ValueType v = static_cast<ValueType>(rhs);
        if (v == detail::value_limits<ValueType>::zero()) m_value = detail::value_limits<ValueType>::zero();
        else m_value /= v;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    constexpr TimeUnit& operator%=(const T& rhs) noexcept {
        const ValueType v = static_cast<ValueType>(rhs);
        if (v == detail::value_limits<ValueType>::zero()) m_value = detail::value_limits<ValueType>::zero();
        else m_value %= v;
        return *this;
    }

public:
    constexpr TimeUnit& operator++() noexcept {
        if (m_value < detail::value_limits<ValueType>::max()) ++m_value;
        return *this;
    }

    constexpr TimeUnit operator++(int) noexcept {
        TimeUnit tmp(*this);
        ++(*this);
        return tmp;
    }

    constexpr TimeUnit& operator--() noexcept {
        if (m_value > detail::value_limits<ValueType>::zero()) --m_value;
        return *this;
    }

    constexpr TimeUnit operator--(int) noexcept {
        TimeUnit tmp(*this);
        --(*this);
        return tmp;
    }

public:
    friend constexpr TimeUnit operator+(TimeUnit lhs, const TimeUnit& rhs) noexcept { return lhs += rhs; }
    friend constexpr TimeUnit operator-(TimeUnit lhs, const TimeUnit& rhs) noexcept { return lhs -= rhs; }
    friend constexpr TimeUnit operator*(TimeUnit lhs, const TimeUnit& rhs) noexcept { return lhs *= rhs; }
    friend constexpr TimeUnit operator/(TimeUnit lhs, const TimeUnit& rhs) noexcept { return lhs /= rhs; }
    friend constexpr TimeUnit operator%(TimeUnit lhs, const TimeUnit& rhs) noexcept { return lhs %= rhs; }

public:
    template<Unit OTag, typename OV>
    friend constexpr TimeUnit operator+(TimeUnit lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs += rhs; }

    template<Unit OTag, typename OV>
    friend constexpr TimeUnit operator-(TimeUnit lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs -= rhs; }

    template<Unit OTag, typename OV>
    friend constexpr TimeUnit operator*(TimeUnit lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs *= rhs; }

    template<Unit OTag, typename OV>
    friend constexpr TimeUnit operator/(TimeUnit lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs /= rhs; }

    template<Unit OTag, typename OV>
    friend constexpr TimeUnit operator%(TimeUnit lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs %= rhs; }

public:
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator+(TimeUnit lhs, const T& rhs) noexcept { return lhs += rhs; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator-(TimeUnit lhs, const T& rhs) noexcept { return lhs -= rhs; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator*(TimeUnit lhs, const T& rhs) noexcept { return lhs *= rhs; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator/(TimeUnit lhs, const T& rhs) noexcept { return lhs /= rhs; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator%(TimeUnit lhs, const T& rhs) noexcept { return lhs %= rhs; }

public:
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator+(const T& lhs, const TimeUnit& rhs) noexcept {
        TimeUnit tmp(static_cast<ValueType>(lhs));
        return tmp += rhs;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator-(const T& lhs, const TimeUnit& rhs) noexcept {
        TimeUnit tmp(static_cast<ValueType>(lhs));
        return tmp -= rhs;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator*(const T& lhs, const TimeUnit& rhs) noexcept {
        TimeUnit tmp(static_cast<ValueType>(lhs));
        return tmp *= rhs;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator/(const T& lhs, const TimeUnit& rhs) noexcept {
        TimeUnit tmp(static_cast<ValueType>(lhs));
        return tmp /= rhs;
    }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr TimeUnit operator%(const T& lhs, const TimeUnit& rhs) noexcept {
        TimeUnit tmp(static_cast<ValueType>(lhs));
        return tmp %= rhs;
    }

public:
    friend constexpr bool operator==(const TimeUnit& lhs, const TimeUnit& rhs) noexcept { return lhs.m_value == rhs.m_value; }
    friend constexpr bool operator!=(const TimeUnit& lhs, const TimeUnit& rhs) noexcept { return lhs.m_value != rhs.m_value; }
    friend constexpr bool operator<(const TimeUnit& lhs, const TimeUnit& rhs) noexcept { return lhs.m_value < rhs.m_value; }
    friend constexpr bool operator<=(const TimeUnit& lhs, const TimeUnit& rhs) noexcept { return lhs.m_value <= rhs.m_value; }
    friend constexpr bool operator>(const TimeUnit& lhs, const TimeUnit& rhs) noexcept { return lhs.m_value > rhs.m_value; }
    friend constexpr bool operator>=(const TimeUnit& lhs, const TimeUnit& rhs) noexcept { return lhs.m_value >= rhs.m_value; }

public:
    template<Unit OTag, typename OV>
    friend constexpr bool operator==(const TimeUnit& lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs.compare_cross(rhs) == 0; }

    template<Unit OTag, typename OV>
    friend constexpr bool operator!=(const TimeUnit& lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs.compare_cross(rhs) != 0; }

    template<Unit OTag, typename OV>
    friend constexpr bool operator<(const TimeUnit& lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs.compare_cross(rhs) < 0; }

    template<Unit OTag, typename OV>
    friend constexpr bool operator<=(const TimeUnit& lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs.compare_cross(rhs) <= 0; }

    template<Unit OTag, typename OV>
    friend constexpr bool operator>(const TimeUnit& lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs.compare_cross(rhs) > 0; }

    template<Unit OTag, typename OV>
    friend constexpr bool operator>=(const TimeUnit& lhs, const TimeUnit<OTag, OV>& rhs) noexcept { return lhs.compare_cross(rhs) >= 0; }

public:
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator==(const TimeUnit& lhs, const T& rhs) noexcept { return lhs.m_value == static_cast<ValueType>(rhs); }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator!=(const TimeUnit& lhs, const T& rhs) noexcept { return lhs.m_value != static_cast<ValueType>(rhs); }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator<(const TimeUnit& lhs, const T& rhs) noexcept { return lhs.m_value < static_cast<ValueType>(rhs); }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator<=(const TimeUnit& lhs, const T& rhs) noexcept { return lhs.m_value <= static_cast<ValueType>(rhs); }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator>(const TimeUnit& lhs, const T& rhs) noexcept { return lhs.m_value > static_cast<ValueType>(rhs); }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator>=(const TimeUnit& lhs, const T& rhs) noexcept { return lhs.m_value >= static_cast<ValueType>(rhs); }

public:
    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator==(const T& lhs, const TimeUnit& rhs) noexcept { return static_cast<ValueType>(lhs) == rhs.m_value; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator!=(const T& lhs, const TimeUnit& rhs) noexcept { return static_cast<ValueType>(lhs) != rhs.m_value; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator<(const T& lhs, const TimeUnit& rhs) noexcept { return static_cast<ValueType>(lhs) < rhs.m_value; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator<=(const T& lhs, const TimeUnit& rhs) noexcept { return static_cast<ValueType>(lhs) <= rhs.m_value; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator>(const T& lhs, const TimeUnit& rhs) noexcept { return static_cast<ValueType>(lhs) > rhs.m_value; }

    template<typename T, typename = typename std::enable_if<detail::is_integer_like_v<T>>::type>
    friend constexpr bool operator>=(const T& lhs, const TimeUnit& rhs) noexcept { return static_cast<ValueType>(lhs) >= rhs.m_value; }

public:
    std::string to_string(bool comma_separators = true, LabelStyle label_style = LabelStyle::abbrev, unsigned int spacing = 1) const {
        std::string num;
        num = format_value_decimal(m_value, comma_separators);
        if (label_style == LabelStyle::none) { return num; }
        const char* label = 0;
        switch (label_style) {
            case LabelStyle::abbrev:   label = traits::abbrev(); break;
            case LabelStyle::singular: label = traits::name();   break;
            case LabelStyle::plural:   label = traits::plural(); break;
            default: break;
        }
        std::string result = num;
        result.append(spacing, ' ');
        result += label;
        return result;
    }

    std::string to_scientific_string(unsigned int precision = 15, LabelStyle style = LabelStyle::abbrev, unsigned spacing = 1) const {
        std::string num = format_value_scientific(m_value, precision);
        if (style == LabelStyle::none) return num;
        const char* label = 0;
        switch (style) {
            case LabelStyle::abbrev:   label = traits::abbrev(); break;
            case LabelStyle::singular: label = traits::name();   break;
            case LabelStyle::plural:   label = traits::plural(); break;
            default: break;
        }
        std::string result = num;
        result.append(spacing, ' ');
        result += label;
        return result;
    }

private:
    template<Unit FromTag, typename FromV>
    static constexpr ValueType convert_from(const TimeUnit<FromTag, FromV>& other) noexcept {
        using planck_val_t = multiprecision::uint256;
        using wide = detail::widen_t<detail::comparison_wide_t<detail::comparison_wide_t<ValueType, FromV>, planck_val_t>>;
        bool of = false;
        wide planck_count = safe_mul(wide(other.count()), wide(unit_traits<FromTag>::planck_per_unit()), &of);
        bool dz = false;
        wide result = safe_div(wide(planck_count), wide(traits::planck_per_unit()), &dz);
        return static_cast<ValueType>(result);
    }

    template<typename A, typename B>
    using wide_t = detail::comparison_wide_t<A, B>;

    template<typename W>
    static constexpr W safe_mul(W a, W b, bool* overflow) noexcept {
        if (b != 0 && a > W::max() / b) {
            *overflow = true;
            return W::max();
        }
        return a * b;
    }

    template<typename W>
    static constexpr W safe_div(const W& a, const W& b, bool* div_by_zero = nullptr) noexcept {
        if (b == 0) {
            *div_by_zero = true;
            return W();
        }
        return a / b;
    }

    template<Unit OTag, typename OV>
    constexpr int compare_cross(const TimeUnit<OTag, OV>& other) const noexcept {
        using planck_val_t = multiprecision::uint256;
        using wide = detail::comparison_wide_t<detail::comparison_wide_t<ValueType, OV>, planck_val_t>;
        wide lhs_planck = wide(m_value) * wide(traits::planck_per_unit());
        wide rhs_planck = wide(other.count()) * wide(unit_traits<OTag>::planck_per_unit());
        if (lhs_planck < rhs_planck) return -1;
        if (lhs_planck > rhs_planck) return  1;
        return 0;
    }

    template<typename V>
    typename std::enable_if<std::is_integral<V>::value, std::string>::type
    format_value_decimal(const V& v, bool /*comma*/) const { return std::to_string(v); }

    template <typename V, typename = typename std::enable_if<is_fizmo_static_int_v<V>>::type, typename = void>
    std::string format_value_decimal(const V& v, bool comma) const { return v.to_string(comma); }

    template<typename V>
    typename std::enable_if<std::is_integral<V>::value, std::string>::type
    format_value_scientific(const V v, unsigned precision) const {
        long double ld = static_cast<long double>(v);
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(precision) << ld;
        return oss.str();
    }

    template <typename V, typename = typename std::enable_if<is_fizmo_static_int_v<V>>::type, typename = void>
    std::string format_value_scientific(const V& v, unsigned precision) const { return v.to_string_scientific(precision); }
};

#define DEFINE_TIME_UNIT(NAME, TAG, DEFAULT_TYPE)                           \
    template<typename V = DEFAULT_TYPE,                                     \
             typename = typename std::enable_if<detail::is_unsigned_integer_like_v<V>>::type> \
    using NAME##_t = TimeUnit<Unit::TAG, V>;                                \
                                                                             \
    using NAME = NAME##_t<>;

DEFINE_TIME_UNIT(planck_second, planck_second, default_storage_uint)
DEFINE_TIME_UNIT(quectosecond,  quectosecond,  default_storage_uint)
DEFINE_TIME_UNIT(rontosecond,   rontosecond,   default_storage_uint)
DEFINE_TIME_UNIT(yoctosecond,   yoctosecond,   default_storage_uint)
DEFINE_TIME_UNIT(zeptosecond,   zeptosecond,   default_storage_uint)
DEFINE_TIME_UNIT(attosecond,    attosecond,    default_storage_uint)
DEFINE_TIME_UNIT(femtosecond,   femtosecond,   default_storage_uint)
DEFINE_TIME_UNIT(picosecond,    picosecond,    default_storage_uint)

DEFINE_TIME_UNIT(nanosecond,    nanosecond,    default_storage_uint)
DEFINE_TIME_UNIT(microsecond,   microsecond,   default_storage_uint)
DEFINE_TIME_UNIT(millisecond,   millisecond,   default_storage_uint)
DEFINE_TIME_UNIT(centisecond,   centisecond,   default_storage_uint)
DEFINE_TIME_UNIT(decisecond,    decisecond,    default_storage_uint)

DEFINE_TIME_UNIT(second,        second,        default_storage_uint)
DEFINE_TIME_UNIT(minute,        minute,        default_storage_uint)
DEFINE_TIME_UNIT(hour,          hour,          default_storage_uint)
DEFINE_TIME_UNIT(day,           day,           default_storage_uint)
DEFINE_TIME_UNIT(week,          week,          default_storage_uint)
DEFINE_TIME_UNIT(month,         month,         default_storage_uint)
DEFINE_TIME_UNIT(year,          year,          default_storage_uint)
DEFINE_TIME_UNIT(decade,        decade,        default_storage_uint)
DEFINE_TIME_UNIT(century,       century,       default_storage_uint)
DEFINE_TIME_UNIT(millennium,    millennium,    default_storage_uint)

} // namespace time

#define FIZMO_DEFINE_TIME_CATEGORY_ENTRY(TRAIT_NAME, TAG)                   \
    template<typename V>                                                     \
    struct TRAIT_NAME<time::TimeUnit<time::Unit::TAG, V>>          \
        : std::true_type {};

template<typename T> struct is_sub_nanosecond_time : std::false_type {};
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, planck_second)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, quectosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, rontosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, yoctosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, zeptosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, attosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, femtosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_sub_nanosecond_time, picosecond)

template<typename T> struct is_small_time : std::false_type {};
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_small_time, nanosecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_small_time, microsecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_small_time, millisecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_small_time, centisecond)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_small_time, decisecond)

template<typename T> struct is_medium_time : std::false_type {};
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_medium_time, second)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_medium_time, minute)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_medium_time, hour)

template<typename T> struct is_large_time : std::false_type {};
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, day)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, week)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, month)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, year)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, decade)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, century)
FIZMO_DEFINE_TIME_CATEGORY_ENTRY(is_large_time, millennium)

template<typename T>
struct is_fizmo_time : std::integral_constant<bool,
    is_sub_nanosecond_time<T>::value ||
    is_small_time<T>::value ||
    is_medium_time<T>::value ||
    is_large_time<T>::value> {};

template<typename T>
constexpr bool is_fizmo_time_v = is_fizmo_time<T>::value;

template<typename...> struct all_are_time_units : std::true_type {};
template<typename T, typename... Rest>
struct all_are_time_units<T, Rest...> : std::integral_constant<bool, is_time_unit_v<T> && all_are_time_units<Rest...>::value> {};

template<typename... Args>
constexpr bool all_are_time_units_v = all_are_time_units<Args...>::value;

namespace time {

template<typename To, Unit FromTag, typename FromV, typename = typename std::enable_if<is_time_unit<To>::value>::type>
constexpr To time_cast(const TimeUnit<FromTag, FromV>& from) noexcept { return static_cast<To>(from); }

} // namespace time
} // namespace fizmo

#endif // FIZMO_TIME_UNIT_HPP