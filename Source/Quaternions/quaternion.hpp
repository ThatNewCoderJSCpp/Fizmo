#ifndef FIZMO_QUATERNION_HPP
#define FIZMO_QUATERNION_HPP

#include <array>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "hypercomplex_kernels.hpp"
#include "../Matrices/square_matrices.hpp"

namespace fizmo {
namespace linalg {

template <class T = long double> class Quaternion;
class BigQuaternion;

namespace hypercomplex_detail {

using detail::is_scalar_operand;
using detail::to_bigfloat;

template <class S, class = void>
struct quaternion_family;

template <class T>
struct quaternion_family<T, typename std::enable_if<scalar::is_real<T>::value>::type> {
    using Q  = Quaternion<T>;
    using V3 = Vector3D<T>;
    using M3 = Matrix3x3<T>;
};

template <>
struct quaternion_family<scalar::BigFloat, void> {
    using Q  = BigQuaternion;
    using V3 = BigVector3D;
    using M3 = BigMatrix3x3;
};

template <class S>
void write_hypercomplex(std::ostream& os, const S* v, std::size_t n, const char* const* units) {
    using F = scalar::field<S>;
    bool any = false;

    for (std::size_t i = 0; i < n; ++i) {
        if (F::is_zero(v[i])) continue;
        const bool neg = F::signbit(v[i]) && !F::is_nan(v[i]);
        if (any) os << (neg ? " - " : " + ");
        else if (neg) os << "-";
        const S av = F::abs(v[i]);
        if (i == 0 || !(av == F::one())) detail::write_scalar(os, av);
        os << units[i];
        any = true;
    }

    if (!any) os << "0";
}

template <class Derived, class S>
class QuaternionImpl : public detail::VectorOps<Derived, 4, S> {
private:
    using Base = detail::VectorOps<Derived, 4, S>;
    using F    = scalar::field<S>;
    using Fam  = quaternion_family<S>;
    using V3   = typename Fam::V3;
    using M3   = typename Fam::M3;

public:
    using context_type = typename Base::context_type;
    using Base::operator*;
    using Base::dot;

    QuaternionImpl() : m_w(F::zero()), m_x(F::zero()), m_y(F::zero()), m_z(F::zero()) {}
    explicit QuaternionImpl(const S& w, const S& x = F::zero(), const S& y = F::zero(), const S& z = F::zero()) : m_w(w), m_x(x), m_y(y), m_z(z) {}
    explicit QuaternionImpl(const std::array<S, 4>& a) : m_w(a[0]), m_x(a[1]), m_y(a[2]), m_z(a[3]) {}

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value && !std::is_same<U, S>::value, int>::type = 0>
    QuaternionImpl(const U& val) : m_w(F::from(val)), m_x(F::zero()), m_y(F::zero()), m_z(F::zero()) {}

    const S& w() const { return m_w; }
    const S& x() const { return m_x; }
    const S& y() const { return m_y; }
    const S& z() const { return m_z; }
    S&       w() { return m_w; }
    S&       x() { return m_x; }
    S&       y() { return m_y; }
    S&       z() { return m_z; }

    S&       component(std::size_t i) { return i == 0 ? m_w : (i == 1 ? m_x : (i == 2 ? m_y : m_z)); }
    const S& component(std::size_t i) const { return i == 0 ? m_w : (i == 1 ? m_x : (i == 2 ? m_y : m_z)); }

    static Derived identity() { return Derived(F::one()); }

    Derived conjugate() const { return Derived(m_w, F::neg(m_x), F::neg(m_y), F::neg(m_z)); }
    static Derived conjugate(const Derived& q) { return q.conjugate(); }

    Derived inverse(const context_type& c = F::default_context()) const {
        if (!this->all_finite()) return nan_value();
        const Vec q = this->big();
        const BigFloat nn = scalar::kernel::exact_dot(q, q);
        if (nn.is_zero()) return nan_value();
        return Derived(F::ratio(q[0], nn, c), F::ratio(-q[1], nn, c), F::ratio(-q[2], nn, c), F::ratio(-q[3], nn, c));
    }

    static Derived inverse(const Derived& q) { return q.inverse(); }

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

    bool is_pure() const { return F::is_zero(m_w); }
    bool is_pure(const S& eps) const { return F::abs(m_w) <= eps; }
    bool is_real() const { return F::is_zero(m_x) && F::is_zero(m_y) && F::is_zero(m_z); }
    bool is_real(const S& eps) const { return F::abs(m_x) <= eps && F::abs(m_y) <= eps && F::abs(m_z) <= eps; }
    bool is_nan() const { return this->has_nan(); }
    bool is_infinite() const { return F::is_inf(m_w) || F::is_inf(m_x) || F::is_inf(m_y) || F::is_inf(m_z); }

    static S dot(const Derived& a, const Derived& b) { return a.dot(b); }

    const S& scalar_part() const { return m_w; }
    Derived  vector_part() const { return Derived(F::zero(), m_x, m_y, m_z); }
    V3       vector() const { return V3(m_x, m_y, m_z); }

    Derived mul(const Derived& o, const context_type& c = F::default_context()) const { return Derived(product<S, 4>(this->to_array(), o.to_array(), c)); }
    Derived operator*(const Derived& o) const { return mul(o); }
    Derived& operator*=(const Derived& o) { return this->self() = mul(o); }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    Derived& operator*=(const U& s) { return this->self() = this->scaled(s); }

    Derived div(const Derived& o, const context_type& c = F::default_context()) const {
        if (!this->all_finite() || !o.all_finite()) return nan_value();
        const Vec a = this->big();
        const Vec b = o.big();
        const BigFloat nn = scalar::kernel::exact_dot(b, b);
        if (nn.is_zero()) return nan_value();
        const Vec num = exact_product(a, hypercomplex_detail::conjugate(b));
        return Derived(F::ratio(num[0], nn, c), F::ratio(num[1], nn, c), F::ratio(num[2], nn, c), F::ratio(num[3], nn, c));
    }

    Derived operator/(const Derived& o) const { return div(o); }
    using Base::operator/;

    Derived natural_log(const context_type& c = F::default_context()) const { return transcendental([](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::ln(q, w); }, c); }
    Derived natural_exponential(const context_type& c = F::default_context()) const { return transcendental([](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::exp(q, w); }, c); }

    Derived power(const Derived& p, const context_type& c = F::default_context()) const {
        const Vec pp = p.big();
        return transcendental([&](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::power(q, pp, w); }, c);
    }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    Derived power(const U& t, const context_type& c = F::default_context()) const {
        const BigFloat tt = to_bigfloat(F::from(t));
        if (std::is_integral<U>::value && !tt.is_zero() && tt.is_integer() && tt.abs() <= BigFloat(64)) return integer_power(t, c);
        return transcendental([&](const Vec& q, const BigFloatContext& w) { return hypercomplex_detail::power(q, tt, w); }, c);
    }

    static Derived from_axis_angle(const S& ax, const S& ay, const S& az, const S& angle, bool degrees = false, const context_type& c = F::default_context()) {
        if constexpr (scalar::dd::eligible<S>::value) {
            std::array<S, 4> f;
            if (!(ax == S(0) && ay == S(0) && az == S(0)) && scalar::dd::fast_axis_angle(ax, ay, az, angle, degrees, f)) return Derived(f);
        }

        const Vec a{ to_bigfloat(ax), to_bigfloat(ay), to_bigfloat(az) };
        const BigFloat t = to_bigfloat(angle);
        if (!scalar::kernel::all_finite(a) || !t.is_finite()) return nan_value();
        const BigFloat l2 = scalar::kernel::exact_dot(a, a);
        if (l2.is_zero()) return identity();

        return Derived(F::template eval_n<4>(c, [&](const BigFloatContext& w) {
            const BigFloatContext ww = scalar::kernel::widen(w, 16);
            const scalar::kernel::CosSin cs = scalar::kernel::cos_sin(BigFloat::ldexp(t, -1), degrees, ww);
            const BigFloat s = BigFloat::div(cs.sin, math::sqrt(l2, ww), ww);
            return std::array<BigFloat, 4>{ cs.cos.rounded(w), BigFloat::mul(a[0], s, w), BigFloat::mul(a[1], s, w), BigFloat::mul(a[2], s, w) };
        }));
    }

    static Derived from_axis_angle(const V3& axis, const S& angle, bool degrees = false, const context_type& c = F::default_context()) { return from_axis_angle(axis.x, axis.y, axis.z, angle, degrees, c); }

    std::array<S, 3> to_axis(const context_type& c = F::default_context()) const {
        const V3 v(m_x, m_y, m_z);
        if (v.is_zero()) return { { F::one(), F::zero(), F::zero() } };
        const V3 u = v.unit_vector(c);
        return { { u.x, u.y, u.z } };
    }

    S to_angle(const context_type& c = F::default_context()) const {
        if (!this->all_finite()) return F::nan();
        const Vec q = this->big();
        const BigFloat nv = vector_norm_squared(q);
        if (nv.is_zero() && q[0].is_zero()) return F::nan();
        return F::eval(c, [&](const BigFloatContext& w) { return BigFloat::ldexp(math::atan2(math::sqrt(nv, scalar::kernel::widen(w, 8)), q[0], w), 1); });
    }

    std::array<S, 9> to_rotation_matrix(const context_type& c = F::default_context()) const {
        std::array<S, 9> out;
        if (!this->all_finite()) { out.fill(F::nan()); return out; }

        if constexpr (std::is_floating_point<S>::value) {
            const S w = m_w, x = m_x, y = m_y, z = m_z;
            const S x2 = S(2) * x, y2 = S(2) * y, w2 = S(2) * w;
            const S q[4] = { w, x, y, z };
            const S na[9][4] = {
                { w, x, -y, -z }, { x2, -w2 }, { x2, w2 },
                { x2, w2 },       { w, -x, y, -z }, { y2, -w2 },
                { x2, -w2 },      { y2, w2 },  { w, -x, -y, z }
            };
            const S nb[9][4] = {
                { w, x, y, z }, { y, z }, { z, y },
                { y, z },       { w, x, y, z }, { z, x },
                { z, y },       { z, x }, { w, x, y, z }
            };
            const std::size_t len[9] = { 4, 2, 2, 2, 4, 2, 2, 2, 4 };
            bool ok = true;
            for (std::size_t i = 0; i < 9 && ok; ++i) ok = scalar::detail::fast_ratio(na[i], nb[i], len[i], q, q, 4, out[i]);
            if (ok) return out;
        }

        const Vec q = this->big();
        const BigFloat n = scalar::kernel::exact_dot(q, q);
        if (n.is_zero()) { out.fill(F::nan()); return out; }

        using scalar::exact_add;
        using scalar::exact_mul;
        using scalar::exact_sub;
        const BigFloat ww = exact_mul(q[0], q[0]), xx = exact_mul(q[1], q[1]), yy = exact_mul(q[2], q[2]), zz = exact_mul(q[3], q[3]);
        const BigFloat wx = exact_mul(q[0], q[1]), wy = exact_mul(q[0], q[2]), wz = exact_mul(q[0], q[3]);
        const BigFloat xy = exact_mul(q[1], q[2]), xz = exact_mul(q[1], q[3]), yz = exact_mul(q[2], q[3]);
        const auto two = [](const BigFloat& v) { return BigFloat::ldexp(v, 1); };

        const BigFloat num[9] = {
            exact_sub(exact_add(ww, xx), exact_add(yy, zz)), two(exact_sub(xy, wz)),                         two(exact_add(xz, wy)),
            two(exact_add(xy, wz)),                         exact_sub(exact_add(ww, yy), exact_add(xx, zz)), two(exact_sub(yz, wx)),
            two(exact_sub(xz, wy)),                         two(exact_add(yz, wx)),                         exact_sub(exact_add(ww, zz), exact_add(xx, yy))
        };

        for (std::size_t i = 0; i < 9; ++i) out[i] = F::ratio(num[i], n, c);
        return out;
    }

    M3 to_matrix(const context_type& c = F::default_context()) const { return M3(to_rotation_matrix(c)); }

    static Derived from_euler(const S& roll, const S& pitch, const S& yaw, bool degrees = false, const context_type& c = F::default_context()) {
        if constexpr (scalar::dd::eligible<S>::value) {
            std::array<S, 4> f;
            if (scalar::dd::fast_euler(roll, pitch, yaw, degrees, f)) return Derived(f);
        }

        const BigFloat r = to_bigfloat(roll), p = to_bigfloat(pitch), y = to_bigfloat(yaw);
        if (!r.is_finite() || !p.is_finite() || !y.is_finite()) return nan_value();

        return Derived(F::template eval_n<4>(c, [&](const BigFloatContext& w) {
            const BigFloatContext ww = scalar::kernel::widen(w, 16);
            const scalar::kernel::CosSin a = scalar::kernel::cos_sin(BigFloat::ldexp(r, -1), degrees, ww);
            const scalar::kernel::CosSin b = scalar::kernel::cos_sin(BigFloat::ldexp(p, -1), degrees, ww);
            const scalar::kernel::CosSin d = scalar::kernel::cos_sin(BigFloat::ldexp(y, -1), degrees, ww);
            const auto m3 = [&](const BigFloat& u, const BigFloat& v, const BigFloat& t) { return BigFloat::mul(BigFloat::mul(u, v, ww), t, ww); };
            return std::array<BigFloat, 4>{
                BigFloat::add(m3(a.cos, b.cos, d.cos), m3(a.sin, b.sin, d.sin), w),
                BigFloat::sub(m3(a.sin, b.cos, d.cos), m3(a.cos, b.sin, d.sin), w),
                BigFloat::add(m3(a.cos, b.sin, d.cos), m3(a.sin, b.cos, d.sin), w),
                BigFloat::sub(m3(a.cos, b.cos, d.sin), m3(a.sin, b.sin, d.cos), w)
            };
        }));
    }

    std::array<S, 3> to_euler(bool degrees = false, const context_type& c = F::default_context()) const {
        std::array<S, 3> out;
        if (!this->all_finite()) { out.fill(F::nan()); return out; }
        const Vec q = this->big();
        const BigFloat n = scalar::kernel::exact_dot(q, q);
        if (n.is_zero()) { out.fill(F::nan()); return out; }

        using scalar::exact_add;
        using scalar::exact_mul;
        using scalar::exact_sub;
        const BigFloat ww = exact_mul(q[0], q[0]), xx = exact_mul(q[1], q[1]), yy = exact_mul(q[2], q[2]), zz = exact_mul(q[3], q[3]);
        const BigFloat sr = BigFloat::ldexp(exact_add(exact_mul(q[0], q[1]), exact_mul(q[2], q[3])), 1);
        const BigFloat cr = exact_sub(exact_add(ww, zz), exact_add(xx, yy));
        const BigFloat sp = BigFloat::ldexp(exact_sub(exact_mul(q[0], q[2]), exact_mul(q[3], q[1])), 1);
        const BigFloat sy = BigFloat::ldexp(exact_add(exact_mul(q[0], q[3]), exact_mul(q[1], q[2])), 1);
        const BigFloat cy = exact_sub(exact_add(ww, xx), exact_add(yy, zz));
        const bool gimbal = sp.abs() >= n;

        const std::array<S, 3> r = F::template eval_n<3>(c, [&](const BigFloatContext& w) {
            const BigFloatContext ww2 = scalar::kernel::widen(w, 8);
            const BigFloat pitch = gimbal ? BigFloat::copysign(multiprecision::constants::half_pi(ww2), sp) : math::arcsin(BigFloat::div(sp, n, ww2), ww2);
            BigFloat a = math::atan2(sr, cr, ww2);
            BigFloat b = pitch;
            BigFloat d = math::atan2(sy, cy, ww2);

            if (degrees) {
                a = scalar::kernel::radians_to_degrees(a, w);
                b = scalar::kernel::radians_to_degrees(b, w);
                d = scalar::kernel::radians_to_degrees(d, w);
            }

            return std::array<BigFloat, 3>{ a.rounded(w), b.rounded(w), d.rounded(w) };
        });

        return r;
    }

    Derived rotate_vector(const S& vx, const S& vy, const S& vz, const context_type& c = F::default_context()) const {
        const V3 r = rotate(V3(vx, vy, vz), c);
        return Derived(F::zero(), r.x, r.y, r.z);
    }

    V3 rotate(const V3& v, const context_type& c = F::default_context()) const {
        if (!this->all_finite() || !v.is_finite()) return V3(F::nan(), F::nan(), F::nan());
        const Vec q = this->big();
        const BigFloat n = scalar::kernel::exact_dot(q, q);
        if (n.is_zero()) return V3(F::nan(), F::nan(), F::nan());
        const Vec p{ BigFloat::zero(), to_bigfloat(v.x), to_bigfloat(v.y), to_bigfloat(v.z) };
        const Vec r = exact_product(exact_product(q, p), hypercomplex_detail::conjugate(q));
        return V3(F::ratio(r[1], n, c), F::ratio(r[2], n, c), F::ratio(r[3], n, c));
    }

    V3 rotate_inverse(const V3& v, const context_type& c = F::default_context()) const { return conjugate().rotate(v, c); }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    static Derived slerp(const Derived& a, const Derived& b, const U& t, const context_type& c = F::default_context()) {
        if (!a.all_finite() || !b.all_finite()) return nan_value();
        const Vec av = a.big();
        const Vec bv = b.big();
        const BigFloat tt = to_bigfloat(F::from(t));
        return Derived(F::template eval_n<4>(c, [&](const BigFloatContext& w) { return detail::to_fixed<4>(hypercomplex_detail::slerp(av, bv, tt, w)); }));
    }

    template <class U, typename std::enable_if<is_scalar_operand<S, U>::value, int>::type = 0>
    static Derived nlerp(const Derived& a, const Derived& b, const U& t, const context_type& c = F::default_context()) {
        if (!a.all_finite() || !b.all_finite()) return nan_value();
        const Vec av = a.big();
        const Vec bv = b.big();
        const BigFloat tt = to_bigfloat(F::from(t));
        return Derived(F::template eval_n<4>(c, [&](const BigFloatContext& w) { return detail::to_fixed<4>(hypercomplex_detail::nlerp(av, bv, tt, w)); }));
    }

    S angle_between(const Derived& other, const context_type& c = F::default_context()) const {
        if (!this->all_finite() || !other.all_finite()) return F::nan();
        const Vec a = this->big();
        const Vec b = other.big();
        return F::eval(c, [&](const BigFloatContext& w) { return hypercomplex_detail::angle_between(a, b, w); });
    }

    friend std::ostream& operator<<(std::ostream& os, const Derived& q) {
        static const char* const units[] = { "", "i", "j", "k" };
        const std::array<S, 4> v = q.to_array();
        write_hypercomplex(os, v.data(), 4, units);
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
    using Vec = scalar::kernel::Vec;

    static Derived nan_value() { return Derived(F::nan(), F::nan(), F::nan(), F::nan()); }

    template <class K>
    Derived transcendental(K&& k, const context_type& c) const {
        if (!this->all_finite()) return nan_value();
        const Vec q = this->big();
        return Derived(F::template eval_n<4>(c, [&](const BigFloatContext& w) { return detail::to_fixed<4>(k(q, w)); }));
    }

    template <class U>
    Derived integer_power(const U& t, const context_type& c) const {
        long long e = static_cast<long long>(t);
        if (!this->all_finite()) return nan_value();
        Vec base = this->big();
        Vec result{ BigFloat::one(), BigFloat::zero(), BigFloat::zero(), BigFloat::zero() };

        if (e < 0) {
            const BigFloat n = scalar::kernel::exact_dot(base, base);
            if (n.is_zero()) return nan_value();
            const Vec conj = hypercomplex_detail::conjugate(base);
            Vec num = conj;
            Vec r = result;
            for (long long i = 0; i < -e; ++i) r = exact_product(r, num);
            BigFloat den = BigFloat::one();
            for (long long i = 0; i < -e; ++i) den = scalar::exact_mul(den, n);
            return Derived(F::ratio(r[0], den, c), F::ratio(r[1], den, c), F::ratio(r[2], den, c), F::ratio(r[3], den, c));
        }

        for (long long i = 0; i < e; ++i) result = exact_product(result, base);
        return Derived(F::round(c, result[0]), F::round(c, result[1]), F::round(c, result[2]), F::round(c, result[3]));
    }

private:
    S m_w;
    S m_x;
    S m_y;
    S m_z;
};

} // namespace hypercomplex_detail

template <class T>
class Quaternion : public hypercomplex_detail::QuaternionImpl<Quaternion<T>, T> {
    static_assert(scalar::is_real<T>::value, "Quaternion requires a standard or fixed-width floating-point type");

public:
    using hypercomplex_detail::QuaternionImpl<Quaternion<T>, T>::QuaternionImpl;

    Quaternion() = default;
};

class BigQuaternion : public hypercomplex_detail::QuaternionImpl<BigQuaternion, scalar::BigFloat> {
public:
    using hypercomplex_detail::QuaternionImpl<BigQuaternion, scalar::BigFloat>::QuaternionImpl;

    BigQuaternion() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigQuaternion(const Quaternion<T>& q) : BigQuaternion(scalar::BigFloat(q.w()), scalar::BigFloat(q.x()), scalar::BigFloat(q.y()), scalar::BigFloat(q.z())) {}

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    Quaternion<T> to() const { return Quaternion<T>(scalar::from_big<T>(w()), scalar::from_big<T>(x()), scalar::from_big<T>(y()), scalar::from_big<T>(z())); }

    BigQuaternion rounded(const scalar::BigFloatContext& c) const { return BigQuaternion(w().rounded(c), x().rounded(c), y().rounded(c), z().rounded(c)); }
};

} // namespace linalg

namespace constants {
    template <class T = long double> inline const linalg::Quaternion<T> QUATERNION_IDENTITY(T(1), T(0), T(0), T(0));
    template <class T = long double> inline const linalg::Quaternion<T> QUATERNION_I(T(0), T(1), T(0), T(0));
    template <class T = long double> inline const linalg::Quaternion<T> QUATERNION_J(T(0), T(0), T(1), T(0));
    template <class T = long double> inline const linalg::Quaternion<T> QUATERNION_K(T(0), T(0), T(0), T(1));
} // namespace constants

namespace linalg {

using QuatF = Quaternion<float>;
using QuatD = Quaternion<double>;
using QuatL = Quaternion<long double>;

template <class T> struct is_quaternion : std::false_type {};
template <class T> struct is_quaternion<Quaternion<T>> : std::true_type {};
template <> struct is_quaternion<BigQuaternion> : std::true_type {};
template <class T> constexpr bool is_quaternion_v = is_quaternion<T>::value;

inline vector3d quat_rotate(const QuatD& q, const vector3d& v) { return q.rotate(v); }
inline vector3d quat_rotate_inv(const QuatD& q, const vector3d& v) { return q.rotate_inverse(v); }
inline vector3d mat3_mul(const Matrix3d& m, const vector3d& v) { return m * v; }
inline Matrix3d quat_to_mat3(const QuatD& q) { return q.to_matrix(); }

inline Matrix3d rotate_inertia(const Matrix3d& I, const QuatD& q) {
    const Matrix3d R = quat_to_mat3(q);
    return R * I * R.transpose();
}

inline Matrix3d skew(const vector3d& v) { return Matrix3d::skew_symmetric(v.x, v.y, v.z); }

} // namespace linalg
} // namespace fizmo

#endif // FIZMO_QUATERNION_HPP
