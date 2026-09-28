#ifndef FIZMO_INTERVAL_HPP
#define FIZMO_INTERVAL_HPP

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "../Multiprecision/Big/big_traits.hpp"

namespace fizmo {

enum class InequalityType {
    LessThan = 0,
    GreaterThan,
    LEQ,
    GEQ
};

namespace ivdetail {

template <int N> struct iv_rank : iv_rank<N - 1> {};
template <>      struct iv_rank<0> {};

template <typename T> auto iv_nan(const T& v, iv_rank<1>) -> decltype(static_cast<bool>(v.is_nan())) { return v.is_nan(); }
template <typename T> auto iv_nan(const T& v, iv_rank<1>) -> typename std::enable_if<std::is_floating_point<T>::value, bool>::type { return std::isnan(v); }
template <typename T> bool iv_nan(const T&, iv_rank<0>) { return false; }

template <typename T> auto iv_undef(const T& v, iv_rank<1>) -> decltype(static_cast<bool>(v.is_undefined())) { return v.is_undefined(); }
template <typename T> bool iv_undef(const T&, iv_rank<0>) { return false; }

template <typename T> auto iv_inf(const T& v, iv_rank<1>) -> decltype(static_cast<bool>(v.is_infinite())) { return v.is_infinite(); }
template <typename T> auto iv_inf(const T& v, iv_rank<1>) -> typename std::enable_if<std::is_floating_point<T>::value, bool>::type { return std::isinf(v); }
template <typename T> bool iv_inf(const T&, iv_rank<0>) { return false; }

template <typename T> auto iv_neg(const T& v, iv_rank<3>) -> decltype(static_cast<bool>(v.signbit())) { return v.signbit(); }
template <typename T> auto iv_neg(const T& v, iv_rank<2>) -> decltype(static_cast<bool>(v.is_negative())) { return v.is_negative(); }
template <typename T> auto iv_neg(const T& v, iv_rank<1>) -> typename std::enable_if<std::is_floating_point<T>::value, bool>::type { return std::signbit(v); }
template <typename T> bool iv_neg(const T& v, iv_rank<0>) { return v < T(); }

template <typename T> inline bool is_invalid(const T& v)  { return iv_nan(v, iv_rank<1>()) || iv_undef(v, iv_rank<1>()); }
template <typename T> inline bool is_infinite(const T& v) { return iv_inf(v, iv_rank<1>()); }
template <typename T> inline bool is_negative(const T& v) { return iv_neg(v, iv_rank<3>()); }

template <typename T>
struct iv_valid_value : std::integral_constant<bool,
    (fizmo::is_fizmo_arithmetic<T>::value || std::is_arithmetic<T>::value) &&
    !std::is_same<T, bool>::value &&
    std::is_same<T, typename std::remove_cv<T>::type>::value> {};

template <typename T>
constexpr bool iv_valid_value_v = iv_valid_value<T>::value;

template <typename T>
inline void iv_prepare(std::ostream& os) {
    if (std::is_floating_point<T>::value) os.precision(std::numeric_limits<T>::digits10);
}

template <typename T>
inline std::string iv_str(const T& v) {
    std::ostringstream ss;
    iv_prepare<T>(ss);
    ss << v;
    return ss.str();
}

template <typename T> inline T iv_zero() { return T(0u); }
template <typename T> inline T iv_one()  { return T(1u); }

} // namespace ivdetail

template <typename T, typename = typename std::enable_if<ivdetail::iv_valid_value_v<T>>::type>
class Endpoint {
public:
    enum class Kind : unsigned char { finite = 0, negative_infinity, positive_infinity, undefined };

private:
    T    m_value;
    Kind m_kind;
    bool m_closed;

    Endpoint(T v, Kind k, bool c) : m_value(std::move(v)), m_kind(k), m_closed(c) {}

public:
    Endpoint() : m_value(), m_kind(Kind::finite), m_closed(false) {}

    static Endpoint at(T v, bool closed) {
        if (ivdetail::is_invalid(v))  return Endpoint(T(), Kind::undefined, false);
        if (ivdetail::is_infinite(v)) return Endpoint(T(), ivdetail::is_negative(v) ? Kind::negative_infinity : Kind::positive_infinity, closed);
        return Endpoint(std::move(v), Kind::finite, closed);
    }

    static Endpoint closed(T v)                            { return at(std::move(v), true); }
    static Endpoint open(T v)                              { return at(std::move(v), false); }
    static Endpoint negative_infinity(bool closed = false) { return Endpoint(T(), Kind::negative_infinity, closed); }
    static Endpoint positive_infinity(bool closed = false) { return Endpoint(T(), Kind::positive_infinity, closed); }
    static Endpoint undefined()                            { return Endpoint(T(), Kind::undefined, false); }

    constexpr Kind kind()                   const noexcept { return m_kind; }
    constexpr bool is_finite()              const noexcept { return m_kind == Kind::finite; }
    constexpr bool is_negative_infinity()   const noexcept { return m_kind == Kind::negative_infinity; }
    constexpr bool is_positive_infinity()   const noexcept { return m_kind == Kind::positive_infinity; }
    constexpr bool is_infinite()            const noexcept { return m_kind == Kind::negative_infinity || m_kind == Kind::positive_infinity; }
    constexpr bool is_valid()               const noexcept { return m_kind != Kind::undefined; }
    constexpr bool is_closed()              const noexcept { return m_closed && m_kind != Kind::undefined; }
    constexpr bool is_open()                const noexcept { return !m_closed && m_kind != Kind::undefined; }
    const T&       value()                  const noexcept { return m_value; }

    Endpoint with_closed(bool c) const { Endpoint e(*this); if (e.is_valid()) e.m_closed = c; return e; }
    Endpoint toggled()           const { return with_closed(!m_closed); }

    friend bool operator==(const Endpoint& a, const Endpoint& b) {
        if (a.m_kind != b.m_kind) return false;
        if (a.m_kind == Kind::undefined) return true;
        if (a.m_closed != b.m_closed) return false;
        return a.m_kind != Kind::finite || (!(a.m_value < b.m_value) && !(b.m_value < a.m_value));
    }

    friend bool operator!=(const Endpoint& a, const Endpoint& b) { return !(a == b); }

    friend std::ostream& operator<<(std::ostream& os, const Endpoint& e) {
        switch (e.m_kind) {
            case Kind::negative_infinity: return os << "-\u221e";
            case Kind::positive_infinity: return os << "+\u221e";
            case Kind::undefined:         return os << "undefined";
            default:                      return os << e.m_value;
        }
    }
};

template <typename T>
inline int compare_positions(const Endpoint<T>& a, const Endpoint<T>& b) {
    auto rank = [](const Endpoint<T>& e) { return e.is_negative_infinity() ? -1 : (e.is_positive_infinity() ? 1 : 0); };
    const int ra = rank(a);
    const int rb = rank(b);
    if (ra != rb) return (ra < rb) ? -1 : 1;
    if (ra != 0)  return 0;
    if (a.value() < b.value()) return -1;
    if (b.value() < a.value()) return 1;
    return 0;
}

template <typename T>
inline int compare_lower_bounds(const Endpoint<T>& a, const Endpoint<T>& b) {
    const int p = compare_positions(a, b);
    if (p != 0 || a.is_closed() == b.is_closed()) return p;
    return a.is_closed() ? -1 : 1;
}

template <typename T>
inline int compare_upper_bounds(const Endpoint<T>& a, const Endpoint<T>& b) {
    const int p = compare_positions(a, b);
    if (p != 0 || a.is_closed() == b.is_closed()) return p;
    return a.is_closed() ? 1 : -1;
}

template <typename T, typename = typename std::enable_if<ivdetail::iv_valid_value_v<T>>::type>
class Interval {
private:
    Endpoint<T> m_lower;
    Endpoint<T> m_upper;

public:
    Interval() : m_lower(Endpoint<T>::open(T())), m_upper(Endpoint<T>::open(T())) {}
    Interval(Endpoint<T> lower, Endpoint<T> upper) : m_lower(std::move(lower)), m_upper(std::move(upper)) {}

    static Interval closed(T a, T b)      { return Interval(Endpoint<T>::closed(std::move(a)), Endpoint<T>::closed(std::move(b))); }
    static Interval open(T a, T b)        { return Interval(Endpoint<T>::open(std::move(a)),   Endpoint<T>::open(std::move(b)));   }
    static Interval closed_open(T a, T b) { return Interval(Endpoint<T>::closed(std::move(a)), Endpoint<T>::open(std::move(b)));   }
    static Interval open_closed(T a, T b) { return Interval(Endpoint<T>::open(std::move(a)),   Endpoint<T>::closed(std::move(b))); }
    static Interval point(const T& a)     { return closed(a, a); }
    static Interval at_least(T a)         { return Interval(Endpoint<T>::closed(std::move(a)), Endpoint<T>::positive_infinity());  }
    static Interval greater_than(T a)     { return Interval(Endpoint<T>::open(std::move(a)),   Endpoint<T>::positive_infinity());  }
    static Interval at_most(T b)          { return Interval(Endpoint<T>::negative_infinity(),  Endpoint<T>::closed(std::move(b))); }
    static Interval less_than(T b)        { return Interval(Endpoint<T>::negative_infinity(),  Endpoint<T>::open(std::move(b)));   }
    static Interval whole()               { return Interval(Endpoint<T>::negative_infinity(),  Endpoint<T>::positive_infinity());  }
    static Interval extended_whole()      { return Interval(Endpoint<T>::negative_infinity(true), Endpoint<T>::positive_infinity(true)); }
    static Interval empty()               { return Interval(); }

    static Interval all()                 { return whole(); }
    static Interval less_or_equal(T b)    { return at_most(std::move(b)); }
    static Interval greater_or_equal(T a) { return at_least(std::move(a)); }
    static Interval unit_interval()       { return closed(ivdetail::iv_zero<T>(), ivdetail::iv_one<T>()); }

    static Interval between(T lo, T hi, bool lo_closed = true, bool hi_closed = true) {
        return Interval(Endpoint<T>::at(std::move(lo), lo_closed), Endpoint<T>::at(std::move(hi), hi_closed));
    }

    static Interval symmetric(const T& center, const T& half_width, bool lo_closed = true, bool hi_closed = true) {
        return between(center - half_width, center + half_width, lo_closed, hi_closed);
    }

    static Interval span(T a, T b) {
        if (b < a) std::swap(a, b);
        Endpoint<T> lo = Endpoint<T>::closed(std::move(a));
        Endpoint<T> hi = Endpoint<T>::closed(std::move(b));
        if (lo.is_infinite()) lo = lo.with_closed(false);
        if (hi.is_infinite()) hi = hi.with_closed(false);
        return Interval(std::move(lo), std::move(hi));
    }

    static Interval from_inequality(InequalityType type, T v) {
        switch (type) {
            case InequalityType::LessThan:    return less_than(std::move(v));
            case InequalityType::GreaterThan: return greater_than(std::move(v));
            case InequalityType::LEQ:         return at_most(std::move(v));
            default:                          return at_least(std::move(v));
        }
    }

    const Endpoint<T>& lower() const noexcept { return m_lower; }
    const Endpoint<T>& upper() const noexcept { return m_upper; }

    bool is_valid() const noexcept { return m_lower.is_valid() && m_upper.is_valid(); }

    bool is_empty() const {
        if (!is_valid()) return true;
        const int p = compare_positions(m_lower, m_upper);
        if (p != 0) return p > 0;
        return !(m_lower.is_closed() && m_upper.is_closed());
    }

    bool is_degenerate()     const { return !is_empty() && compare_positions(m_lower, m_upper) == 0; }
    bool is_bounded_below()  const { return m_lower.is_finite(); }
    bool is_bounded_above()  const { return m_upper.is_finite(); }
    bool is_bounded()        const { return m_lower.is_finite() && m_upper.is_finite(); }

    bool contains(const T& x) const {
        if (is_empty()) return false;
        const Endpoint<T> e = Endpoint<T>::closed(x);
        if (!e.is_valid()) return false;
        const int pl = compare_positions(m_lower, e);
        const int pu = compare_positions(e, m_upper);
        return (pl < 0 || (pl == 0 && m_lower.is_closed())) && (pu < 0 || (pu == 0 && m_upper.is_closed()));
    }

    bool contains(const Interval& other) const {
        if (other.is_empty()) return true;
        if (is_empty())       return false;
        return compare_lower_bounds(m_lower, other.m_lower) <= 0 && compare_upper_bounds(other.m_upper, m_upper) <= 0;
    }

    bool is_subset_of(const Interval& other) const { return other.contains(*this); }

    Interval intersect(const Interval& other) const {
        if (is_empty() || other.is_empty()) return empty();
        const Endpoint<T>& lo = (compare_lower_bounds(m_lower, other.m_lower) >= 0) ? m_lower : other.m_lower;
        const Endpoint<T>& hi = (compare_upper_bounds(m_upper, other.m_upper) <= 0) ? m_upper : other.m_upper;
        return Interval(lo, hi);
    }

    static Interval intersect(const Interval& a, const Interval& b) { return a.intersect(b); }

    bool overlaps(const Interval& other) const { return !intersect(other).is_empty(); }

    Interval hull(const Interval& other) const {
        if (is_empty())       return other.is_valid() ? other : empty();
        if (other.is_empty()) return *this;
        const Endpoint<T>& lo = (compare_lower_bounds(m_lower, other.m_lower) <= 0) ? m_lower : other.m_lower;
        const Endpoint<T>& hi = (compare_upper_bounds(m_upper, other.m_upper) >= 0) ? m_upper : other.m_upper;
        return Interval(lo, hi);
    }

    Interval        merge(const Interval& other) const          { return hull(other); }
    static Interval merge(const Interval& a, const Interval& b) { return a.hull(b); }

    bool is_connected_with(const Interval& other) const {
        if (is_empty() || other.is_empty()) return true;
        auto gap = [](const Endpoint<T>& hi, const Endpoint<T>& lo) {
            const int p = compare_positions(hi, lo);
            return p < 0 || (p == 0 && hi.is_open() && lo.is_open());
        };
        return !gap(m_upper, other.m_lower) && !gap(other.m_upper, m_lower);
    }

    bool overlaps_or_touches(const Interval& other) const { return !is_empty() && !other.is_empty() && is_connected_with(other); }

    bool try_union(const Interval& other, Interval& out) const {
        if (!is_connected_with(other)) return false;
        out = hull(other);
        return true;
    }

    T width() const { return m_upper.value() - m_lower.value(); }

    friend bool operator==(const Interval& a, const Interval& b) {
        const bool ea = a.is_empty();
        const bool eb = b.is_empty();
        if (ea || eb) return ea && eb;
        return a.m_lower == b.m_lower && a.m_upper == b.m_upper;
    }

    friend bool operator!=(const Interval& a, const Interval& b) { return !(a == b); }

    friend std::ostream& operator<<(std::ostream& os, const Interval& iv) {
        if (!iv.is_valid()) return os << "undefined";
        if (iv.is_empty())  return os << "{}";
        return os << (iv.m_lower.is_closed() ? '[' : '(') << iv.m_lower << ", " << iv.m_upper << (iv.m_upper.is_closed() ? ']' : ')');
    }

    std::string to_string() const {
        std::ostringstream ss;
        ivdetail::iv_prepare<T>(ss);
        ss << *this;
        return ss.str();
    }

    std::string to_inequality_string(const std::string& var) const {
        if (!is_valid()) return "undefined";
        if (is_empty())  return "empty";
        std::ostringstream ss;
        ivdetail::iv_prepare<T>(ss);

        if (is_degenerate()) {
            ss << var << " = " << m_lower;
            return ss.str();
        }

        const bool lo_inf = m_lower.is_infinite();
        const bool hi_inf = m_upper.is_infinite();
        if (lo_inf && hi_inf) return "\u2200" + var;

        if (lo_inf) {
            ss << var << (m_upper.is_closed() ? " \u2264 " : " < ") << m_upper;
            return ss.str();
        }

        if (hi_inf) {
            ss << var << (m_lower.is_closed() ? " \u2265 " : " > ") << m_lower;
            return ss.str();
        }

        ss << m_lower << (m_lower.is_closed() ? " \u2264 " : " < ") << var << (m_upper.is_closed() ? " \u2264 " : " < ") << m_upper;
        return ss.str();
    }
};

template <typename T, typename = typename std::enable_if<ivdetail::iv_valid_value_v<T>>::type>
class IntervalSet {
private:
    std::vector<Interval<T>> m_intervals;

    void normalize() {
        m_intervals.erase(std::remove_if(m_intervals.begin(), m_intervals.end(), [](const Interval<T>& iv) { return iv.is_empty(); }),
                          m_intervals.end());
        if (m_intervals.size() < 2) return;

        std::sort(m_intervals.begin(), m_intervals.end(), [](const Interval<T>& a, const Interval<T>& b) {
            return compare_lower_bounds(a.lower(), b.lower()) < 0;
        });

        std::vector<Interval<T>> merged;
        merged.reserve(m_intervals.size());
        merged.push_back(m_intervals[0]);

        for (std::size_t i = 1; i < m_intervals.size(); ++i) {
            if (merged.back().is_connected_with(m_intervals[i])) merged.back() = merged.back().hull(m_intervals[i]);
            else                                                  merged.push_back(m_intervals[i]);
        }

        m_intervals.swap(merged);
    }

public:
    using const_iterator = typename std::vector<Interval<T>>::const_iterator;

    IntervalSet() = default;
    explicit IntervalSet(const Interval<T>& iv) { if (!iv.is_empty()) m_intervals.push_back(iv); }
    explicit IntervalSet(std::vector<Interval<T>> ivs) : m_intervals(std::move(ivs)) { normalize(); }
    IntervalSet(std::initializer_list<Interval<T>> ivs) : m_intervals(ivs) { normalize(); }

    static IntervalSet all()                { return IntervalSet(Interval<T>::whole()); }
    static IntervalSet empty()              { return IntervalSet(); }
    static IntervalSet point(const T& v)    { return IntervalSet(Interval<T>::point(v)); }

    bool        is_empty() const noexcept { return m_intervals.empty(); }
    std::size_t size()     const noexcept { return m_intervals.size(); }

    bool is_all() const {
        return m_intervals.size() == 1 && m_intervals[0].lower().is_negative_infinity() && m_intervals[0].upper().is_positive_infinity();
    }

    const std::vector<Interval<T>>& intervals() const noexcept { return m_intervals; }
    const_iterator begin() const noexcept { return m_intervals.begin(); }
    const_iterator end()   const noexcept { return m_intervals.end(); }

    Interval<T> hull() const {
        if (m_intervals.empty()) return Interval<T>::empty();
        return Interval<T>(m_intervals.front().lower(), m_intervals.back().upper());
    }

    bool contains(const T& x) const {
        return std::any_of(m_intervals.begin(), m_intervals.end(), [&x](const Interval<T>& iv) { return iv.contains(x); });
    }

    bool contains(const Interval<T>& iv) const {                                        
        if (iv.is_empty()) return true;
        return std::any_of(m_intervals.begin(), m_intervals.end(), [&iv](const Interval<T>& c) { return c.contains(iv); });
    }

    bool contains(const IntervalSet& other) const { return other.difference(*this).is_empty(); }

    IntervalSet union_with(const IntervalSet& other) const {
        std::vector<Interval<T>> combined;
        combined.reserve(m_intervals.size() + other.m_intervals.size());
        combined.insert(combined.end(), m_intervals.begin(), m_intervals.end());
        combined.insert(combined.end(), other.m_intervals.begin(), other.m_intervals.end());
        return IntervalSet(std::move(combined));
    }

    IntervalSet intersect_with(const IntervalSet& other) const {                         
        std::vector<Interval<T>> r;
        std::size_t i = 0;
        std::size_t j = 0;

        while (i < m_intervals.size() && j < other.m_intervals.size()) {
            const Interval<T> x = m_intervals[i].intersect(other.m_intervals[j]);
            if (!x.is_empty()) r.push_back(x);
            if (compare_upper_bounds(m_intervals[i].upper(), other.m_intervals[j].upper()) < 0) ++i; else ++j;
        }

        return IntervalSet(std::move(r));
    }

    IntervalSet complement() const {
        std::vector<Interval<T>> r;
        if (m_intervals.empty()) { r.push_back(Interval<T>::whole()); return IntervalSet(std::move(r)); }
        const Interval<T>& first = m_intervals.front();
        const Interval<T>& last  = m_intervals.back();
        if (!first.lower().is_negative_infinity()) r.emplace_back(Endpoint<T>::negative_infinity(), first.lower().toggled());

        for (std::size_t i = 0; i + 1 < m_intervals.size(); ++i) {
            r.emplace_back(m_intervals[i].upper().toggled(), m_intervals[i + 1].lower().toggled());
        }

        if (!last.upper().is_positive_infinity()) r.emplace_back(last.upper().toggled(), Endpoint<T>::positive_infinity());
        return IntervalSet(std::move(r));
    }

    IntervalSet difference(const IntervalSet& other) const           { return intersect_with(other.complement()); }
    IntervalSet symmetric_difference(const IntervalSet& other) const { return difference(other).union_with(other.difference(*this)); }

    IntervalSet operator|(const IntervalSet& o) const { return union_with(o); }
    IntervalSet operator&(const IntervalSet& o) const { return intersect_with(o); }
    IntervalSet operator-(const IntervalSet& o) const { return difference(o); }
    IntervalSet operator^(const IntervalSet& o) const { return symmetric_difference(o); }
    IntervalSet operator~()                     const { return complement(); }

    IntervalSet& operator|=(const IntervalSet& o) { return *this = union_with(o); }
    IntervalSet& operator&=(const IntervalSet& o) { return *this = intersect_with(o); }
    IntervalSet& operator-=(const IntervalSet& o) { return *this = difference(o); }
    IntervalSet& operator^=(const IntervalSet& o) { return *this = symmetric_difference(o); }

    friend bool operator==(const IntervalSet& a, const IntervalSet& b) {
        if (a.m_intervals.size() != b.m_intervals.size()) return false;
        for (std::size_t i = 0; i < a.m_intervals.size(); ++i) if (a.m_intervals[i] != b.m_intervals[i]) return false;
        return true;
    }

    friend bool operator!=(const IntervalSet& a, const IntervalSet& b) { return !(a == b); }

    friend std::ostream& operator<<(std::ostream& os, const IntervalSet& s) {
        if (s.m_intervals.empty()) return os << "{}";

        for (std::size_t i = 0; i < s.m_intervals.size(); ++i) {
            if (i != 0) os << " \u222a ";
            os << s.m_intervals[i];
        }

        return os;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ivdetail::iv_prepare<T>(ss);
        ss << *this;
        return ss.str();
    }

    std::string to_inequality_string(const std::string& var) const {
        if (m_intervals.empty()) return "empty";
        std::string out;

        for (std::size_t i = 0; i < m_intervals.size(); ++i) {
            if (i != 0) out += " or ";
            out += m_intervals[i].to_inequality_string(var);
        }

        return out;
    }
};

} // namespace fizmo

#endif // FIZMO_INTERVAL_HPP