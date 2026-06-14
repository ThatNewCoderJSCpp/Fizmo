#ifndef FIZMO_TIME_UNIT_HPP
#define FIZMO_TIME_UNIT_HPP

#include "../Basic/fizmo_defines.hpp"
#include "../Standard Overloads/abs.hpp"
#include "../Multiprecision/Fixed Width int/type_traits.hpp"
#include "../Multiprecision/Big/big_int.hpp"
#include <cstdint>
#include <limits>
#include <type_traits>
#include <string>
#include <ostream>

namespace fizmo {
namespace temp_time {

namespace detail {

using uint128 = multiprecision::uint128;
using int128  = multiprecision::int128;
using uint256 = multiprecision::uint256;
using int256  = multiprecision::int256;
using uint512 = multiprecision::uint512;
using int512  = multiprecision::int512;
using uint1024 = multiprecision::uint1024;
using int1024  = multiprecision::int1024;
using uint2048 = multiprecision::uint2048;
using int2048  = multiprecision::int2048;
using uint4096 = multiprecision::uint4096;
using int4096  = multiprecision::int4096;

template<typename T>
struct is_integer_like : std::integral_constant<bool, is_fizmo_static_int_v<T>> {};

template<typename T>
struct is_unsigned_integer_like : std::integral_constant<bool, is_fizmo_static_int_v<T> && is_fizmo_unsigned_v<T>> {};

template<typename T>
constexpr bool is_integer_like_v = is_integer_like<T>::value;

template<typename T>
constexpr bool is_unsigned_integer_like_v = is_unsigned_integer_like<T>::value;

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

#define FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(TYPE)                            \
    template<> struct value_limits<TYPE> {                                   \
        static constexpr TYPE max() noexcept { return TYPE::max(); }        \
        static constexpr TYPE zero() noexcept { return TYPE(); }            \
        static constexpr TYPE min() noexcept { return TYPE(); }             \
    };

#define FIZMO_DEFINE_SIGNED_VALUE_LIMITS(TYPE)                              \
    template<> struct value_limits<TYPE> {                                   \
        static constexpr TYPE max() noexcept { return TYPE::max(); }        \
        static constexpr TYPE zero() noexcept { return TYPE(); }            \
        static constexpr TYPE min() noexcept { return TYPE::min(); }        \
    };

FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(uint128)
FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(uint256)
FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(uint512)
FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(uint1024)
FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(uint2048)
FIZMO_DEFINE_UNSIGNED_VALUE_LIMITS(uint4096)

FIZMO_DEFINE_SIGNED_VALUE_LIMITS(int128)
FIZMO_DEFINE_SIGNED_VALUE_LIMITS(int256)
FIZMO_DEFINE_SIGNED_VALUE_LIMITS(int512)
FIZMO_DEFINE_SIGNED_VALUE_LIMITS(int1024)
FIZMO_DEFINE_SIGNED_VALUE_LIMITS(int2048)
FIZMO_DEFINE_SIGNED_VALUE_LIMITS(int4096)

constexpr multiprecision::uint256 p10_table[] = {
    multiprecision::uint256(1ULL),                      // 10^0
    multiprecision::uint256(10ULL),                     // 10^1
    multiprecision::uint256(100ULL),                    // 10^2
    multiprecision::uint256(1000ULL),                   // 10^3
    multiprecision::uint256(10000ULL),                  // 10^4
    multiprecision::uint256(100000ULL),                 // 10^5
    multiprecision::uint256(1000000ULL),                // 10^6
    multiprecision::uint256(10000000ULL),               // 10^7
    multiprecision::uint256(100000000ULL),              // 10^8
    multiprecision::uint256(1000000000ULL),             // 10^9
    multiprecision::uint256(10000000000ULL),            // 10^10
    multiprecision::uint256(100000000000ULL),           // 10^11
    multiprecision::uint256(1000000000000ULL),          // 10^12
    multiprecision::uint256(10000000000000ULL),         // 10^13
    multiprecision::uint256(100000000000000ULL),        // 10^14
    multiprecision::uint256(1000000000000000ULL),       // 10^15
    multiprecision::uint256(10000000000000000ULL),      // 10^16
    multiprecision::uint256(100000000000000000ULL),     // 10^17
    multiprecision::uint256(1000000000000000000ULL),    // 10^18
    multiprecision::uint256(10000000000000000000ULL),   // 10^19
};

constexpr int p10_table_size = 20;

constexpr multiprecision::uint256 p10(int n) noexcept {
    if (n < p10_table_size) return p10_table[n];
    multiprecision::uint256 result = p10_table[p10_table_size - 1];
    for (int i = p10_table_size - 1; i < n; ++i) result = result * multiprecision::uint256(10);
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
        static constexpr multiprecision::uint256 planck_per_unit() noexcept  \
            { return PLANCK_EXPR; }                                          \
        static constexpr multiprecision::uint256 coeff() noexcept            \
            { return multiprecision::uint256(COEFF_VAL); }                   \
        static constexpr int exponent() noexcept { return EXP_VAL; }        \
        static constexpr const char* name()   { return NAME_STR; }          \
        static constexpr const char* plural() { return PLURAL_STR; }        \
        static constexpr const char* abbrev() { return ABBREV_STR; }        \
    };

FIZMO_DEFINE_UNIT_TRAITS(
    planck_second,
    multiprecision::uint256(1),
    5391247ULL, -50,
    "planck second", "planck seconds", "tP"
)

FIZMO_DEFINE_UNIT_TRAITS(
    quectosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(7),
    1ULL, -30,
    "quectosecond", "quectoseconds", "qs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    rontosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(10),
    1ULL, -27,
    "rontosecond", "rontoseconds", "rs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    yoctosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(13),
    1ULL, -24,
    "yoctosecond", "yoctoseconds", "ys"
)

FIZMO_DEFINE_UNIT_TRAITS(
    zeptosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(16),
    1ULL, -21,
    "zeptosecond", "zeptoseconds", "zs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    attosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(19),
    1ULL, -18,
    "attosecond", "attoseconds", "as"
)

FIZMO_DEFINE_UNIT_TRAITS(
    femtosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(22),
    1ULL, -15,
    "femtosecond", "femtoseconds", "fs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    picosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(25),
    1ULL, -12,
    "picosecond", "picoseconds", "ps"
)

FIZMO_DEFINE_UNIT_TRAITS(
    nanosecond,
    multiprecision::uint256(1854870ULL) * detail::p10(28),
    1ULL, -9,
    "nanosecond", "nanoseconds", "ns"
)

FIZMO_DEFINE_UNIT_TRAITS(
    microsecond,
    multiprecision::uint256(1854870ULL) * detail::p10(31),
    1ULL, -6,
    "microsecond", "microseconds", "us"
)

FIZMO_DEFINE_UNIT_TRAITS(
    millisecond,
    multiprecision::uint256(1854870ULL) * detail::p10(34),
    1ULL, -3,
    "millisecond", "milliseconds", "ms"
)

FIZMO_DEFINE_UNIT_TRAITS(
    centisecond,
    multiprecision::uint256(1854870ULL) * detail::p10(35),
    1ULL, -2,
    "centisecond", "centiseconds", "cs"
)

FIZMO_DEFINE_UNIT_TRAITS(
    decisecond,
    multiprecision::uint256(1854870ULL) * detail::p10(36),
    1ULL, -1,
    "decisecond", "deciseconds", "ds"
)

FIZMO_DEFINE_UNIT_TRAITS(
    second,
    multiprecision::uint256(1854870ULL) * detail::p10(37),
    1ULL, 0,
    "second", "seconds", "s"
)

FIZMO_DEFINE_UNIT_TRAITS(
    minute,
    multiprecision::uint256(111292200ULL) * detail::p10(37),
    60ULL, 0,
    "minute", "minutes", "min"
)

FIZMO_DEFINE_UNIT_TRAITS(
    hour,
    multiprecision::uint256(6677532000ULL) * detail::p10(37),
    3600ULL, 0,
    "hour", "hours", "hr"
)

FIZMO_DEFINE_UNIT_TRAITS(
    day,
    multiprecision::uint256(160260768000ULL) * detail::p10(37),
    86400ULL, 0,
    "day", "days", "d"
)

FIZMO_DEFINE_UNIT_TRAITS(
    week,
    multiprecision::uint256(1121825376000ULL) * detail::p10(37),
    604800ULL, 0,
    "week", "weeks", "wk"
)

FIZMO_DEFINE_UNIT_TRAITS(
    month,
    unit_traits<Unit::day>::planck_per_unit() * multiprecision::uint256(30),
    2592000ULL, 0,
    "month", "months", "mo"
)

FIZMO_DEFINE_UNIT_TRAITS(
    year,
    multiprecision::uint256(58495180320000ULL) * detail::p10(37),
    31536000ULL, 0,
    "year", "years", "yr"
)

FIZMO_DEFINE_UNIT_TRAITS(
    decade,
    multiprecision::uint256(584951803200000ULL) * detail::p10(37),
    315360000ULL, 0,
    "decade", "decades", "dec"
)

FIZMO_DEFINE_UNIT_TRAITS(
    century,
    multiprecision::uint256(5849518032000000ULL) * detail::p10(37),
    3153600000ULL, 0,
    "century", "centuries", "c"
)

FIZMO_DEFINE_UNIT_TRAITS(
    millennium,
    multiprecision::uint256(58495180320000000ULL) * detail::p10(37),
    31536000000ULL, 0,
    "millennium", "millennia", "mil"
)

template<Unit Tag, typename ValueType, typename = typename std::enable_if<temp_time::detail::is_unsigned_integer_like_v<ValueType>>::type>
class TimeUnit;

} // namespace temp_time

template<typename T> struct is_time_unit : std::false_type {};
template<temp_time::Unit Tag, typename V> struct is_time_unit<temp_time::TimeUnit<Tag, V>> : std::true_type {};

template<typename T>
constexpr bool is_time_unit_v = is_time_unit<T>::value;

namespace temp_time {

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

    template<typename U, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<U> && !std::is_same<U, ValueType>::value>::type>
    constexpr explicit TimeUnit(const U& v) noexcept : m_value(static_cast<ValueType>(v)) {}

    constexpr TimeUnit(const TimeUnit&) noexcept = default;
    constexpr TimeUnit(TimeUnit&&) noexcept = default;
    constexpr TimeUnit& operator=(const TimeUnit&) noexcept = default;
    constexpr TimeUnit& operator=(TimeUnit&&) noexcept = default;

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr explicit TimeUnit(const TimeUnit<OTag, OV>& other) noexcept : m_value(convert_from(other)) {}

    template<Unit OTag, typename OV, typename = typename std::enable_if<(OTag != Tag || !std::is_same<OV, ValueType>::value)>::type>
    constexpr TimeUnit& operator=(const TimeUnit<OTag, OV>& other) noexcept {
        m_value = convert_from(other);
        return *this;
    }

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

DEFINE_TIME_UNIT(planck_second, planck_second, multiprecision::uint256)
DEFINE_TIME_UNIT(quectosecond,  quectosecond,  multiprecision::uint128)
DEFINE_TIME_UNIT(rontosecond,   rontosecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(yoctosecond,   yoctosecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(zeptosecond,   zeptosecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(attosecond,    attosecond,    multiprecision::uint128)
DEFINE_TIME_UNIT(femtosecond,   femtosecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(picosecond,    picosecond,    multiprecision::uint128)

DEFINE_TIME_UNIT(nanosecond,    nanosecond,    multiprecision::uint128)
DEFINE_TIME_UNIT(microsecond,   microsecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(millisecond,   millisecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(centisecond,   centisecond,   multiprecision::uint128)
DEFINE_TIME_UNIT(decisecond,    decisecond,    multiprecision::uint128)

DEFINE_TIME_UNIT(second,        second,        multiprecision::uint128)
DEFINE_TIME_UNIT(minute,        minute,        multiprecision::uint128)
DEFINE_TIME_UNIT(hour,          hour,          multiprecision::uint128)
DEFINE_TIME_UNIT(day,           day,           multiprecision::uint128)
DEFINE_TIME_UNIT(week,          week,          multiprecision::uint128)
DEFINE_TIME_UNIT(month,         month,         multiprecision::uint128)
DEFINE_TIME_UNIT(year,          year,          multiprecision::uint128)
DEFINE_TIME_UNIT(decade,        decade,        multiprecision::uint128)
DEFINE_TIME_UNIT(century,       century,       multiprecision::uint128)
DEFINE_TIME_UNIT(millennium,    millennium,    multiprecision::uint128)

} // namespace temp_time

#define FIZMO_DEFINE_TIME_CATEGORY_ENTRY(TRAIT_NAME, TAG)                   \
    template<typename V>                                                     \
    struct TRAIT_NAME<temp_time::TimeUnit<temp_time::Unit::TAG, V>>          \
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

namespace temp_time {

template<typename To, Unit FromTag, typename FromV, typename = typename std::enable_if<is_time_unit<To>::value>::type>
constexpr To time_cast(const TimeUnit<FromTag, FromV>& from) noexcept { return static_cast<To>(from); }

} // namespace temp_time
} // namespace fizmo

#endif // FIZMO_TIME_UNIT_HPP