#ifndef FIZMO_OCTONION_HPP
#define FIZMO_OCTONION_HPP

#include "quaternion.hpp"

namespace fizmo {
namespace linalg {

template <class T = long double> class Octonion;
class BigOctonion;

namespace hypercomplex_detail {

template <class Derived, class S, class Q>
class OctonionImpl : public detail::VectorOps<Derived, 8, S> {
private:
    using Base = detail::VectorOps<Derived, 8, S>;
    using F    = scalar::field<S>;
    using Vec  = scalar::kernel::Vec;

public:
    using context_type = typename Base::context_type;
    using Base::operator*;
    using Base::operator/;
    using Base::dot;

    OctonionImpl() { m_data.fill(F::zero()); }

    explicit OctonionImpl(const S& e0, const S& e1 = F::zero(), const S& e2 = F::zero(), const S& e3 = F::zero(), const S& e4 = F::zero(), const S& e5 = F::zero(), const S& e6 = F::zero(), const S& e7 = F::zero())
        : m_data{ { e0, e1, e2, e3, e4, e5, e6, e7 } } {}

    explicit OctonionImpl(const std::array<S, 8>& arr) : m_data(arr) {}
    explicit OctonionImpl(const Q& q) : m_data{ { q.w(), q.x(), q.y(), q.z(), F::zero(), F::zero(), F::zero(), F::zero() } } {}

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value && !std::is_same<U, S>::value, int>::type = 0>
    OctonionImpl(const U& val) : m_data{ { F::from(val), F::zero(), F::zero(), F::zero(), F::zero(), F::zero(), F::zero(), F::zero() } } {}

    S&       component(std::size_t i) { return m_data[i]; }
    const S& component(std::size_t i) const { return m_data[i]; }

    const std::array<S, 8>& data() const { return m_data; }

    static Derived identity() { return Derived(F::one()); }

    Derived conjugate() const {
        std::array<S, 8> a = m_data;
        for (std::size_t i = 1; i < 8; ++i) a[i] = F::neg(a[i]);
        return Derived(a);
    }

    static Derived conjugate(const Derived& o) { return o.conjugate(); }

    Derived inverse(const context_type& c = F::default_context()) const {
        if (!this->all_finite()) return nan_value();
        const Vec q = this->big();
        const BigFloat nn = scalar::kernel::exact_dot(q, q);
        if (nn.is_zero()) return nan_value();
        std::array<S, 8> out;
        for (std::size_t i = 0; i < 8; ++i) out[i] = F::ratio(i == 0 ? q[i] : -q[i], nn, c);
        return Derived(out);
    }

    static Derived inverse(const Derived& o) { return o.inverse(); }

    Derived unit(const context_type& c = F::default_context()) const { return this->unit_vector(c); }

    bool is_unit() const {
        if (!this->all_finite()) return false;
        const Vec q = this->big();
        return BigFloat::compare(scalar::kernel::exact_dot(q, q), BigFloat::one()) == BigFloat::ordering::equal;
    }

    bool is_unit(const S& eps) const {
        if (!this->all_finite()) return false;
        const Vec q = this->big();
        return scalar::exact_sub(scalar::kernel::exact_dot(q, q), BigFloat::one()).abs() <= to_bigfloat(eps);
    }

    bool is_pure() const { return F::is_zero(m_data[0]); }
    bool is_pure(const S& eps) const { return F::abs(m_data[0]) <= eps; }
    bool is_nan() const { return this->has_nan(); }

    bool is_infinite() const {
        for (const auto& v : m_data) if (F::is_inf(v)) return true;
        return false;
    }

    static S dot(const Derived& a, const Derived& b) { return a.dot(b); }

    const S& scalar_part() const { return m_data[0]; }

    Derived vector_part() const {
        std::array<S, 8> a = m_data;
        a[0] = F::zero();
        return Derived(a);
    }

    Derived mul(const Derived& o, const context_type& c = F::default_context()) const { return Derived(product<S, 8>(m_data, o.m_data, c)); }
    Derived operator*(const Derived& o) const { return mul(o); }
    Derived& operator*=(const Derived& o) { return this->self() = mul(o); }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    Derived& operator*=(const U& s) { return this->self() = this->scaled(s); }

    Q lo() const { return Q(m_data[0], m_data[1], m_data[2], m_data[3]); }
    Q hi() const { return Q(m_data[4], m_data[5], m_data[6], m_data[7]); }

    static Derived from_quaternion_pair(const Q& q0, const Q& q1) { return Derived(q0.w(), q0.x(), q0.y(), q0.z(), q1.w(), q1.x(), q1.y(), q1.z()); }

    Derived natural_log(const context_type& c = F::default_context()) const { return transcendental([](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::ln(q, w); }, c); }
    Derived natural_exponential(const context_type& c = F::default_context()) const { return transcendental([](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::exp(q, w); }, c); }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    Derived power(const U& t, const context_type& c = F::default_context()) const {
        const BigFloat tt = to_bigfloat(F::from(t));
        return transcendental([&](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::power(q, tt, w); }, c);
    }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    static Derived slerp(const Derived& a, const Derived& b, const U& t, const context_type& c = F::default_context()) {
        if (!a.all_finite() || !b.all_finite()) return nan_value();
        const Vec av = a.big();
        const Vec bv = b.big();
        const BigFloat tt = to_bigfloat(F::from(t));
        return Derived(F::template eval_n<8>(c, [&](const BigFloatContext& w) { return detail::to_fixed<8>(hypercomplex_detail::slerp(av, bv, tt, w)); }));
    }

    friend std::ostream& operator<<(std::ostream& os, const Derived& o) {
        static const char* const units[] = { "", "e1", "e2", "e3", "e4", "e5", "e6", "e7" };
        write_hypercomplex(os, o.m_data.data(), 8, units);
        return os;
    }

    std::string stringify(unsigned int precision = 8) const {
        std::ostringstream os;
        os.precision(precision);
        os << this->self();
        return os.str();
    }

    std::string to_string() const { return stringify(); }

protected:
    static Derived nan_value() {
        std::array<S, 8> a;
        a.fill(F::nan());
        return Derived(a);
    }

    template <class K>
    Derived transcendental(K&& k, const context_type& c) const {
        if (!this->all_finite()) return nan_value();
        const Vec q = this->big();
        return Derived(F::template eval_n<8>(c, [&](const BigFloatContext& w) { return detail::to_fixed<8>(k(q, w)); }));
    }

private:
    std::array<S, 8> m_data;
};

} // namespace hypercomplex_detail

template <class T>
class Octonion : public hypercomplex_detail::OctonionImpl<Octonion<T>, T, Quaternion<T>> {
    static_assert(scalar::is_real<T>::value, "Octonion requires a standard or fixed-width floating-point type");

public:
    using hypercomplex_detail::OctonionImpl<Octonion<T>, T, Quaternion<T>>::OctonionImpl;

    Octonion() = default;
};

class BigOctonion : public hypercomplex_detail::OctonionImpl<BigOctonion, scalar::BigFloat, BigQuaternion> {
public:
    using hypercomplex_detail::OctonionImpl<BigOctonion, scalar::BigFloat, BigQuaternion>::OctonionImpl;

    BigOctonion() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigOctonion(const Octonion<T>& o) {
        for (std::size_t i = 0; i < 8; ++i) component(i) = scalar::BigFloat(o.data()[i]);
    }

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    Octonion<T> to() const {
        std::array<T, 8> a;
        for (std::size_t i = 0; i < 8; ++i) a[i] = scalar::from_big<T>(data()[i]);
        return Octonion<T>(a);
    }

    BigOctonion rounded(const scalar::BigFloatContext& c) const {
        std::array<scalar::BigFloat, 8> a;
        for (std::size_t i = 0; i < 8; ++i) a[i] = data()[i].rounded(c);
        return BigOctonion(a);
    }
};

} // namespace linalg

namespace constants {
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_IDENTITY(T(1), T(0), T(0), T(0), T(0), T(0), T(0), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E1(T(0), T(1), T(0), T(0), T(0), T(0), T(0), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E2(T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E3(T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E4(T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E5(T(0), T(0), T(0), T(0), T(0), T(1), T(0), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E6(T(0), T(0), T(0), T(0), T(0), T(0), T(1), T(0));
    template <class T = long double> inline const linalg::Octonion<T> OCTONION_E7(T(0), T(0), T(0), T(0), T(0), T(0), T(0), T(1));
} // namespace constants

namespace linalg {

template <class T> struct is_octonion : std::false_type {};
template <class T> struct is_octonion<Octonion<T>> : std::true_type {};
template <> struct is_octonion<BigOctonion> : std::true_type {};
template <class T> constexpr bool is_octonion_v = is_octonion<T>::value;

template <class T> struct is_fizmo_hypercomplex : std::integral_constant<bool, is_quaternion_v<T> || is_octonion_v<T>> {};
template <class T> constexpr bool is_fizmo_hypercomplex_v = is_fizmo_hypercomplex<T>::value;

} // namespace linalg
} // namespace fizmo

inline fizmo::linalg::Quaternion<long double> operator"" _qi(long double v) { return fizmo::linalg::Quaternion<long double>(0.0L, v, 0.0L, 0.0L); }
inline fizmo::linalg::Quaternion<long double> operator"" _qj(long double v) { return fizmo::linalg::Quaternion<long double>(0.0L, 0.0L, v, 0.0L); }
inline fizmo::linalg::Quaternion<long double> operator"" _qk(long double v) { return fizmo::linalg::Quaternion<long double>(0.0L, 0.0L, 0.0L, v); }
inline fizmo::linalg::Quaternion<long double> operator"" _qi(unsigned long long v) { return fizmo::linalg::Quaternion<long double>(0.0L, static_cast<long double>(v), 0.0L, 0.0L); }
inline fizmo::linalg::Quaternion<long double> operator"" _qj(unsigned long long v) { return fizmo::linalg::Quaternion<long double>(0.0L, 0.0L, static_cast<long double>(v), 0.0L); }
inline fizmo::linalg::Quaternion<long double> operator"" _qk(unsigned long long v) { return fizmo::linalg::Quaternion<long double>(0.0L, 0.0L, 0.0L, static_cast<long double>(v)); }

namespace std {
    template <class T> string to_string(const fizmo::linalg::Quaternion<T>& q) { return q.stringify(); }
    template <class T> string to_string(const fizmo::linalg::Octonion<T>& o) { return o.stringify(); }
    inline string to_string(const fizmo::linalg::BigQuaternion& q) { return q.stringify(); }
    inline string to_string(const fizmo::linalg::BigOctonion& o) { return o.stringify(); }
}

#endif // FIZMO_OCTONION_HPP
