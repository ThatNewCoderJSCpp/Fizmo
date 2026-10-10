#ifndef DYNAMIC_COLOR_CLASS_HPP
#define DYNAMIC_COLOR_CLASS_HPP

#include <type_traits>
#include <limits>
#include <cstdint>
#include <algorithm>
#include <string>
#include <sstream>
#include <ostream>
#include "../Multiprecision/Fixed Width Float/traits.hpp"
#include "color.hpp"

namespace fizmo {
namespace graphics {

template <
    typename R = std::uint16_t, typename G = R, typename B = R, typename A = R,
    typename = typename std::enable_if<
        (std::is_integral<R>::value && std::is_unsigned<R>::value) &&
        (std::is_integral<G>::value && std::is_unsigned<G>::value) &&
        (std::is_integral<B>::value && std::is_unsigned<B>::value) &&
        (std::is_integral<A>::value && std::is_unsigned<A>::value)
    >::type
>
class FixedColor {
private:
    R m_r;
    G m_g;
    B m_b;
    A m_a;

public:
    using red_type   = R;
    using green_type = G;
    using blue_type  = B;
    using alpha_type = A;

public:
    constexpr FixedColor() noexcept : m_r(0), m_g(0), m_b(0), m_a(std::numeric_limits<A>::max()) {}
    constexpr FixedColor(R r, G g, B b, A a = std::numeric_limits<A>::max()) noexcept : m_r(r), m_g(g), m_b(b), m_a(a) {}
    constexpr FixedColor(const FixedColor&) noexcept = default;
    constexpr FixedColor(FixedColor&& other) noexcept = default;

    constexpr FixedColor(const Color& c) noexcept
        : m_r(static_cast<R>(c.red())), m_g(static_cast<G>(c.green())),
          m_b(static_cast<B>(c.blue())), m_a(static_cast<A>(c.alpha())) {}

    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator=(const FixedColor&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator=(FixedColor&& other) noexcept = default;

    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator=(const Color& c) {
        m_r = static_cast<R>(c.red());
        m_g = static_cast<G>(c.green());
        m_b = static_cast<B>(c.blue());
        m_a = static_cast<A>(c.alpha());
        return *this;
    }

    explicit constexpr operator Color() noexcept {
        return Color(
            static_cast<std::uint8_t>(m_r), static_cast<std::uint8_t>(m_g),
            static_cast<std::uint8_t>(m_b), static_cast<std::uint8_t>(m_a)
        );
    }

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            (std::is_integral<R2>::value && std::is_unsigned<R2>::value) &&
            (std::is_integral<G2>::value && std::is_unsigned<G2>::value) &&
            (std::is_integral<B2>::value && std::is_unsigned<B2>::value) &&
            (std::is_integral<A2>::value && std::is_unsigned<A2>::value)
        >::type
    >
    constexpr FixedColor(const FixedColor<R2, G2, B2, A2>& other) noexcept
        : m_r(static_cast<R>(other.red())), m_g(static_cast<G>(other.green())),
          m_b(static_cast<B>(other.blue())), m_a(static_cast<A>(other.alpha())) {}

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            (std::is_integral<R2>::value && std::is_unsigned<R2>::value) &&
            (std::is_integral<G2>::value && std::is_unsigned<G2>::value) &&
            (std::is_integral<B2>::value && std::is_unsigned<B2>::value) &&
            (std::is_integral<A2>::value && std::is_unsigned<A2>::value)
        >::type
    >
    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator=(const FixedColor<R2, G2, B2, A2>& other) noexcept {
        m_r = static_cast<R>(other.red());
        m_g = static_cast<G>(other.green());
        m_b = static_cast<B>(other.blue());
        m_a = static_cast<A>(other.alpha());
        return *this;
    }

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            (std::is_integral<R2>::value && std::is_unsigned<R2>::value) &&
            (std::is_integral<G2>::value && std::is_unsigned<G2>::value) &&
            (std::is_integral<B2>::value && std::is_unsigned<B2>::value) &&
            (std::is_integral<A2>::value && std::is_unsigned<A2>::value)
        >::type
    >
    explicit constexpr operator FixedColor<R2, G2, B2, A2>() const noexcept {
        return to_linear_scaled<R2, G2, B2, A2>();
    }

public:
    constexpr bool operator==(const FixedColor& o) const noexcept { return m_r == o.m_r && m_g == o.m_g && m_b == o.m_b && m_a == o.m_a; }
    constexpr bool operator!=(const FixedColor& o) const noexcept { return !(*this == o); }
    constexpr bool operator<(const FixedColor& o) const noexcept { return magnitude_sq() < o.magnitude_sq(); }
    constexpr bool operator>(const FixedColor& o) const noexcept { return o < *this; }
    constexpr bool operator<=(const FixedColor& o) const noexcept { return !(o < *this); }
    constexpr bool operator>=(const FixedColor& o) const noexcept { return !(*this < o); }

public:
    constexpr FixedColor operator+(const FixedColor& o) const noexcept {
        return FixedColor(
            clamp_r(multiprecision::int128(m_r) + multiprecision::int128(o.m_r)),
            clamp_g(multiprecision::int128(m_g) + multiprecision::int128(o.m_g)),
            clamp_b(multiprecision::int128(m_b) + multiprecision::int128(o.m_b)),
            clamp_a(multiprecision::int128(m_a) + multiprecision::int128(o.m_a))
        );
    }

    constexpr FixedColor operator-(const FixedColor& o) const noexcept {
        return FixedColor(
            clamp_r(multiprecision::int128(m_r) - multiprecision::int128(o.m_r)),
            clamp_g(multiprecision::int128(m_g) - multiprecision::int128(o.m_g)),
            clamp_b(multiprecision::int128(m_b) - multiprecision::int128(o.m_b)),
            clamp_a(multiprecision::int128(m_a) - multiprecision::int128(o.m_a))
        );
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    constexpr FixedColor operator*(S scalar) const noexcept {
        return FixedColor(
            clamp_r(multiprecision::int128(m_r) * multiprecision::int128(scalar)),
            clamp_g(multiprecision::int128(m_g) * multiprecision::int128(scalar)),
            clamp_b(multiprecision::int128(m_b) * multiprecision::int128(scalar)),
            m_a
        );
    }

    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator+=(const FixedColor& o) noexcept {
        m_r = clamp_r(multiprecision::int128(m_r) + multiprecision::int128(o.m_r));
        m_g = clamp_g(multiprecision::int128(m_g) + multiprecision::int128(o.m_g));
        m_b = clamp_b(multiprecision::int128(m_b) + multiprecision::int128(o.m_b));
        m_a = clamp_a(multiprecision::int128(m_a) + multiprecision::int128(o.m_a));
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator-=(const FixedColor& o) noexcept {
        m_r = clamp_r(multiprecision::int128(m_r) - multiprecision::int128(o.m_r));
        m_g = clamp_g(multiprecision::int128(m_g) - multiprecision::int128(o.m_g));
        m_b = clamp_b(multiprecision::int128(m_b) - multiprecision::int128(o.m_b));
        m_a = clamp_a(multiprecision::int128(m_a) - multiprecision::int128(o.m_a));
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR FixedColor& operator*=(S scalar) noexcept {
        m_r = clamp_r(multiprecision::int128(m_r) * multiprecision::int128(scalar));
        m_g = clamp_g(multiprecision::int128(m_g) * multiprecision::int128(scalar));
        m_b = clamp_b(multiprecision::int128(m_b) * multiprecision::int128(scalar));
        return *this;
    }

public:
    constexpr R red()   const noexcept { return m_r; }
    constexpr G green() const noexcept { return m_g; }
    constexpr B blue()  const noexcept { return m_b; }
    constexpr A alpha() const noexcept { return m_a; }

    constexpr R& red()   noexcept { return m_r; }
    constexpr G& green() noexcept { return m_g; }
    constexpr B& blue()  noexcept { return m_b; }
    constexpr A& alpha() noexcept { return m_a; }

    constexpr void set_red(R r)   noexcept { m_r = r; }
    constexpr void set_green(G g) noexcept { m_g = g; }
    constexpr void set_blue(B b)  noexcept { m_b = b; }
    constexpr void set_alpha(A a) noexcept { m_a = a; }
    constexpr void set(R r, G g, B b) noexcept { m_r = r; m_g = g; m_b = b; }
    constexpr void set(R r, G g, B b, A a) noexcept { m_r = r; m_g = g; m_b = b; m_a = a; }

    constexpr bool is_opaque() const noexcept { return m_a == std::numeric_limits<A>::max(); }
    constexpr bool is_transparent() const noexcept { return m_a == 0; }
    constexpr double alpha_normalized() const noexcept { return static_cast<double>(m_a) / max_alpha(); }

    constexpr FixedColor with_alpha(A a) const noexcept { return FixedColor(m_r, m_g, m_b, a); }

public:
    constexpr FixedColor combine_additive(const FixedColor& o) const noexcept { return *this + o; }

    constexpr FixedColor lerp(const FixedColor& o, double t) const noexcept {
        t = fizmo::clamp(t, 0.0, 1.0);

        return FixedColor(
            static_cast<R>(m_r + (o.m_r - static_cast<double>(m_r)) * t),
            static_cast<G>(m_g + (o.m_g - static_cast<double>(m_g)) * t),
            static_cast<B>(m_b + (o.m_b - static_cast<double>(m_b)) * t),
            static_cast<A>(m_a + (o.m_a - static_cast<double>(m_a)) * t)
        );
    }

public:
    template <typename R2, typename G2, typename B2, typename A2 = R2>
    constexpr FixedColor<R2,G2,B2,A2> to_normalized() const noexcept {
        return FixedColor<R2,G2,B2,A2>(
            static_cast<R2>(red_normalized()   * FixedColor<R2,G2,B2,A2>::max_red()),
            static_cast<G2>(green_normalized() * FixedColor<R2,G2,B2,A2>::max_green()),
            static_cast<B2>(blue_normalized()  * FixedColor<R2,G2,B2,A2>::max_blue()),
            static_cast<A2>(alpha_normalized() * FixedColor<R2,G2,B2,A2>::max_alpha())
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    FixedColor<R2,G2,B2,A2> to_gamma_corrected(double gamma = 2.2) const {
        double rn = apply_gamma(red_normalized(), gamma);
        double gn = apply_gamma(green_normalized(), gamma);
        double bn = apply_gamma(blue_normalized(), gamma);

        return FixedColor<R2,G2,B2,A2>(
            static_cast<R2>(unapply_gamma(rn, gamma) * FixedColor<R2,G2,B2,A2>::max_red()),
            static_cast<G2>(unapply_gamma(gn, gamma) * FixedColor<R2,G2,B2,A2>::max_green()),
            static_cast<B2>(unapply_gamma(bn, gamma) * FixedColor<R2,G2,B2,A2>::max_blue()),
            static_cast<A2>(alpha_normalized() * FixedColor<R2,G2,B2,A2>::max_alpha())
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    FixedColor<R2,G2,B2,A2> to_gamma_scaled(double gamma = 2.2) const {
        return FixedColor<R2,G2,B2,A2>(
            static_cast<R2>(apply_gamma(red_normalized(), gamma)   * FixedColor<R2,G2,B2,A2>::max_red()),
            static_cast<G2>(apply_gamma(green_normalized(), gamma) * FixedColor<R2,G2,B2,A2>::max_green()),
            static_cast<B2>(apply_gamma(blue_normalized(), gamma)  * FixedColor<R2,G2,B2,A2>::max_blue()),
            static_cast<A2>(alpha_normalized() * FixedColor<R2,G2,B2,A2>::max_alpha())
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    constexpr FixedColor<R2,G2,B2,A2> to_linear_scaled() const noexcept {
        const double r = static_cast<double>(m_r) / max_red();
        const double g = static_cast<double>(m_g) / max_green();
        const double b = static_cast<double>(m_b) / max_blue();
        const double a = static_cast<double>(m_a) / max_alpha();

        return FixedColor<R2,G2,B2,A2>(
            static_cast<R2>(r * FixedColor<R2,G2,B2,A2>::max_red()),
            static_cast<G2>(g * FixedColor<R2,G2,B2,A2>::max_green()),
            static_cast<B2>(b * FixedColor<R2,G2,B2,A2>::max_blue()),
            static_cast<A2>(a * FixedColor<R2,G2,B2,A2>::max_alpha())
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    FixedColor<R2,G2,B2,A2> to_srgb_corrected() const noexcept {
        double rn = srgb_decode(red_normalized());
        double gn = srgb_decode(green_normalized());
        double bn = srgb_decode(blue_normalized());

        return FixedColor<R2,G2,B2,A2>(
            static_cast<R2>(srgb_encode(rn) * FixedColor<R2,G2,B2,A2>::max_red()),
            static_cast<G2>(srgb_encode(gn) * FixedColor<R2,G2,B2,A2>::max_green()),
            static_cast<B2>(srgb_encode(bn) * FixedColor<R2,G2,B2,A2>::max_blue()),
            static_cast<A2>(alpha_normalized() * FixedColor<R2,G2,B2,A2>::max_alpha())
        );
    }

public:
    std::string to_string() const {
        std::ostringstream ss;
        if (is_opaque()) {
            ss << "RGB(" << +m_r << ", " << +m_g << ", " << +m_b << ")";
        } else {
            ss << "RGBA(" << +m_r << ", " << +m_g << ", " << +m_b << ", " << +m_a << ")";
        }
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const FixedColor& c) {
        os << c.to_string();
        return os;
    }

public:
    static constexpr R max_red()   noexcept { return std::numeric_limits<R>::max(); }
    static constexpr G max_green() noexcept { return std::numeric_limits<G>::max(); }
    static constexpr B max_blue()  noexcept { return std::numeric_limits<B>::max(); }
    static constexpr A max_alpha() noexcept { return std::numeric_limits<A>::max(); }
    constexpr double red_normalized()   const noexcept { return static_cast<double>(m_r) / max_red(); }
    constexpr double green_normalized() const noexcept { return static_cast<double>(m_g) / max_green(); }
    constexpr double blue_normalized()  const noexcept { return static_cast<double>(m_b) / max_blue(); }

public:
    constexpr multiprecision::uint128 magnitude_sq() const noexcept {
        return multiprecision::uint128(m_r) * multiprecision::uint128(m_r) +
               multiprecision::uint128(m_g) * multiprecision::uint128(m_g) +
               multiprecision::uint128(m_b) * multiprecision::uint128(m_b);
    }

    static double srgb_encode(double x) {
        x = std::clamp(x, 0.0, 1.0);
        constexpr double cutoff = 0x1.9f323ecbf984cp-9;
        if (x <= cutoff) return 12.92 * x;
        constexpr double inv_gamma = 1.0 / 2.4;
        double p = std::pow(x, inv_gamma);
        return std::fma(1.055, p, -0.055);
    }

    static double srgb_decode(double x) {
        x = std::clamp(x, 0.0, 1.0);
        constexpr double cutoff = 0x1.4a7ef9db22d0ep-5;
        if (x <= cutoff) return x / 12.92;
        constexpr double gamma = 2.4;
        return std::pow((x + 0.055) / 1.055, gamma);
    }

private:
    static constexpr R clamp_r(multiprecision::int128 v) noexcept {
        if (v.is_negative() || v.is_zero() || v.is_undefined()) { return R(0); }
        return (v >= multiprecision::int128(std::numeric_limits<R>::max())) ? std::numeric_limits<R>::max() : static_cast<R>(v.get_low_bits());
    }

    static constexpr G clamp_g(multiprecision::int128 v) noexcept {
        if (v.is_negative() || v.is_zero() || v.is_undefined()) { return G(0); }
        return (v >= multiprecision::int128(std::numeric_limits<G>::max())) ? std::numeric_limits<G>::max() : static_cast<G>(v.get_low_bits());
    }

    static constexpr B clamp_b(multiprecision::int128 v) noexcept {
        if (v.is_negative() || v.is_zero() || v.is_undefined()) { return B(0); }
        return (v >= multiprecision::int128(std::numeric_limits<B>::max())) ? std::numeric_limits<B>::max() : static_cast<B>(v.get_low_bits());
    }

    static constexpr A clamp_a(multiprecision::int128 v) noexcept {
        if (v.is_negative() || v.is_zero() || v.is_undefined()) { return A(0); }
        return (v >= multiprecision::int128(std::numeric_limits<A>::max())) ? std::numeric_limits<A>::max() : static_cast<A>(v.get_low_bits());
    }

private:
    static double apply_gamma(const double x, const double g) { return std::pow(x, g); }
    static double unapply_gamma(const double x, const double g) { return std::pow(x, 1.0 / g); }
};

template <
    typename R = float, typename G = R, typename B = R, typename A = R,
    typename = typename std::enable_if<
        std::is_floating_point<R>::value &&
        std::is_floating_point<G>::value &&
        std::is_floating_point<B>::value &&
        std::is_floating_point<A>::value
    >::type
>
class FixedFloatColor {
private:
    R m_r;
    G m_g;
    B m_b;
    A m_a;

public:
    using red_type   = R;
    using green_type = G;
    using blue_type  = B;
    using alpha_type = A;

public:
    constexpr FixedFloatColor() noexcept : m_r(0), m_g(0), m_b(0), m_a(1) {}
    constexpr FixedFloatColor(R r, G g, B b, A a = A(1)) noexcept : m_r(r), m_g(g), m_b(b), m_a(a) {}
    constexpr FixedFloatColor(const FixedFloatColor&) noexcept = default;
    constexpr FixedFloatColor(FixedFloatColor&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator=(const FixedFloatColor&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator=(FixedFloatColor&&) noexcept = default;

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            std::is_floating_point<R2>::value &&
            std::is_floating_point<G2>::value &&
            std::is_floating_point<B2>::value &&
            std::is_floating_point<A2>::value
        >::type
    >
    constexpr FixedFloatColor(const FixedFloatColor<R2,G2,B2,A2>& o) noexcept
        : m_r(static_cast<R>(o.red())), m_g(static_cast<G>(o.green())),
          m_b(static_cast<B>(o.blue())), m_a(static_cast<A>(o.alpha())) {}

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            std::is_floating_point<R2>::value &&
            std::is_floating_point<G2>::value &&
            std::is_floating_point<B2>::value &&
            std::is_floating_point<A2>::value
        >::type
    >
    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator=(const FixedFloatColor<R2,G2,B2,A2>& o) noexcept {
        m_r = static_cast<R>(o.red());
        m_g = static_cast<G>(o.green());
        m_b = static_cast<B>(o.blue());
        m_a = static_cast<A>(o.alpha());
        return *this;
    }

    template <typename R2, typename G2, typename B2, typename A2>
    explicit constexpr operator FixedFloatColor<R2,G2,B2,A2>() const noexcept {
        return FixedFloatColor<R2,G2,B2,A2>(
            static_cast<R2>(m_r), static_cast<G2>(m_g),
            static_cast<B2>(m_b), static_cast<A2>(m_a)
        );
    }

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            std::is_integral<R2>::value && std::is_unsigned<R2>::value &&
            std::is_integral<G2>::value && std::is_unsigned<G2>::value &&
            std::is_integral<B2>::value && std::is_unsigned<B2>::value &&
            std::is_integral<A2>::value && std::is_unsigned<A2>::value
        >::type
    >
    constexpr FixedFloatColor(const FixedColor<R2,G2,B2,A2>& o) noexcept
        : m_r(static_cast<R>(o.red())   / R(FixedColor<R2,G2,B2,A2>::max_red())),
          m_g(static_cast<G>(o.green()) / G(FixedColor<R2,G2,B2,A2>::max_green())),
          m_b(static_cast<B>(o.blue())  / B(FixedColor<R2,G2,B2,A2>::max_blue())),
          m_a(static_cast<A>(o.alpha()) / A(FixedColor<R2,G2,B2,A2>::max_alpha()))
    {}

    template <
        typename R2, typename G2, typename B2, typename A2,
        typename = typename std::enable_if<
            std::is_integral<R2>::value && std::is_unsigned<R2>::value &&
            std::is_integral<G2>::value && std::is_unsigned<G2>::value &&
            std::is_integral<B2>::value && std::is_unsigned<B2>::value &&
            std::is_integral<A2>::value && std::is_unsigned<A2>::value
        >::type
    >
    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator=(const FixedColor<R2,G2,B2,A2>& o) noexcept {
        m_r = static_cast<R>(o.red())   / double(FixedColor<R2,G2,B2,A2>::max_red());
        m_g = static_cast<G>(o.green()) / double(FixedColor<R2,G2,B2,A2>::max_green());
        m_b = static_cast<B>(o.blue())  / double(FixedColor<R2,G2,B2,A2>::max_blue());
        m_a = static_cast<A>(o.alpha()) / double(FixedColor<R2,G2,B2,A2>::max_alpha());
        return *this;
    }

public:
    constexpr bool operator==(const FixedFloatColor& o) const noexcept { return m_r == o.m_r && m_g == o.m_g && m_b == o.m_b && m_a == o.m_a; }
    constexpr bool operator!=(const FixedFloatColor& o) const noexcept { return !(*this == o); }
    constexpr bool operator<(const FixedFloatColor& o) const noexcept { return magnitude_sq() < o.magnitude_sq(); }
    constexpr bool operator>(const FixedFloatColor& o) const noexcept { return o < *this; }
    constexpr bool operator<=(const FixedFloatColor& o) const noexcept { return !(o < *this); }
    constexpr bool operator>=(const FixedFloatColor& o) const noexcept { return !(*this < o); }

public:
    constexpr FixedFloatColor operator+(const FixedFloatColor& o) const noexcept {
        return FixedFloatColor(m_r + o.m_r, m_g + o.m_g, m_b + o.m_b, m_a + o.m_a);
    }

    constexpr FixedFloatColor operator-(const FixedFloatColor& o) const noexcept {
        return FixedFloatColor(m_r - o.m_r, m_g - o.m_g, m_b - o.m_b, m_a - o.m_a);
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    constexpr FixedFloatColor operator*(S s) const noexcept {
        return FixedFloatColor(m_r * s, m_g * s, m_b * s, m_a);
    }

    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator+=(const FixedFloatColor& o) noexcept {
        m_r += o.m_r; m_g += o.m_g; m_b += o.m_b; m_a += o.m_a;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator-=(const FixedFloatColor& o) noexcept {
        m_r -= o.m_r; m_g -= o.m_g; m_b -= o.m_b; m_a -= o.m_a;
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR FixedFloatColor& operator*=(S s) noexcept {
        m_r *= s; m_g *= s; m_b *= s;
        return *this;
    }

public:
    constexpr R red()   const noexcept { return m_r; }
    constexpr G green() const noexcept { return m_g; }
    constexpr B blue()  const noexcept { return m_b; }
    constexpr A alpha() const noexcept { return m_a; }

    constexpr R& red()   noexcept { return m_r; }
    constexpr G& green() noexcept { return m_g; }
    constexpr B& blue()  noexcept { return m_b; }
    constexpr A& alpha() noexcept { return m_a; }

    OPTIONAL_CPP14_CONSTEXPR void set(R r, G g, B b) noexcept { m_r = r; m_g = g; m_b = b; }
    OPTIONAL_CPP14_CONSTEXPR void set(R r, G g, B b, A a) noexcept { m_r = r; m_g = g; m_b = b; m_a = a; }

    constexpr bool is_opaque() const noexcept { return m_a >= A(1); }
    constexpr bool is_transparent() const noexcept { return m_a <= A(0); }
    constexpr double alpha_normalized() const noexcept { return static_cast<double>(m_a); }

    constexpr FixedFloatColor with_alpha(A a) const noexcept { return FixedFloatColor(m_r, m_g, m_b, a); }

public:
    constexpr FixedFloatColor lerp(const FixedFloatColor& o, double t) const noexcept {
        return FixedFloatColor(
            m_r + (o.m_r - m_r) * t,
            m_g + (o.m_g - m_g) * t,
            m_b + (o.m_b - m_b) * t,
            m_a + (o.m_a - m_a) * t
        );
    }

    constexpr FixedFloatColor blend_over(const FixedFloatColor& src) const noexcept {
        const double sa = src.m_a;
        const double da = m_a;
        const double inv_sa = 1.0 - sa;
        const double out_a = sa + da * inv_sa;

        if (out_a <= 0.0) return FixedFloatColor(R(0), G(0), B(0), A(0));

        const double inv_out = 1.0 / out_a;
        return FixedFloatColor(
            static_cast<R>((src.m_r * sa + m_r * da * inv_sa) * inv_out),
            static_cast<G>((src.m_g * sa + m_g * da * inv_sa) * inv_out),
            static_cast<B>((src.m_b * sa + m_b * da * inv_sa) * inv_out),
            static_cast<A>(out_a)
        );
    }

    constexpr FixedFloatColor to_premultiplied() const noexcept {
        return FixedFloatColor(
            static_cast<R>(m_r * m_a), static_cast<G>(m_g * m_a),
            static_cast<B>(m_b * m_a), m_a
        );
    }

    constexpr FixedFloatColor to_straight() const noexcept {
        if (m_a <= A(0)) return FixedFloatColor(R(0), G(0), B(0), A(0));
        const double inv = 1.0 / m_a;
        return FixedFloatColor(
            static_cast<R>(m_r * inv), static_cast<G>(m_g * inv),
            static_cast<B>(m_b * inv), m_a
        );
    }

public:
    constexpr double red_normalized()   const noexcept { return static_cast<double>(m_r); }
    constexpr double green_normalized() const noexcept { return static_cast<double>(m_g); }
    constexpr double blue_normalized()  const noexcept { return static_cast<double>(m_b); }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    constexpr FixedFloatColor<R2,G2,B2,A2> to_normalized() const noexcept {
        return FixedFloatColor<R2,G2,B2,A2>(
            static_cast<R2>(m_r), static_cast<G2>(m_g),
            static_cast<B2>(m_b), static_cast<A2>(m_a)
        );
    }

public:
    template <typename R2, typename G2, typename B2, typename A2 = R2>
    constexpr FixedFloatColor<R2,G2,B2,A2> to_linear_scaled() const noexcept {
        return FixedFloatColor<R2,G2,B2,A2>(
            static_cast<R2>(m_r), static_cast<G2>(m_g),
            static_cast<B2>(m_b), static_cast<A2>(m_a)
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    FixedFloatColor<R2,G2,B2,A2> to_gamma_corrected(double gamma = 2.2) const {
        double rn = unapply_gamma(m_r, gamma);
        double gn = unapply_gamma(m_g, gamma);
        double bn = unapply_gamma(m_b, gamma);

        return FixedFloatColor<R2,G2,B2,A2>(
            static_cast<R2>(rn), static_cast<G2>(gn),
            static_cast<B2>(bn), static_cast<A2>(m_a)
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    FixedFloatColor<R2,G2,B2,A2> to_gamma_scaled(double gamma = 2.2) const {
        return FixedFloatColor<R2,G2,B2,A2>(
            static_cast<R2>(apply_gamma(m_r, gamma)),
            static_cast<G2>(apply_gamma(m_g, gamma)),
            static_cast<B2>(apply_gamma(m_b, gamma)),
            static_cast<A2>(m_a)
        );
    }

    template <typename R2, typename G2, typename B2, typename A2 = R2>
    FixedFloatColor<R2,G2,B2,A2> to_srgb_corrected() const noexcept {
        double rn = srgb_decode(m_r);
        double gn = srgb_decode(m_g);
        double bn = srgb_decode(m_b);

        return FixedFloatColor<R2,G2,B2,A2>(
            static_cast<R2>(srgb_encode(rn)),
            static_cast<G2>(srgb_encode(gn)),
            static_cast<B2>(srgb_encode(bn)),
            static_cast<A2>(m_a)
        );
    }

public:
    constexpr double magnitude_sq() const noexcept { return double(m_r) * m_r + double(m_g) * m_g + double(m_b) * m_b; }

    static double srgb_encode(double x) {
        x = std::clamp(x, 0.0, 1.0);
        constexpr double cutoff = 0x1.9f323ecbf984cp-9;
        if (x <= cutoff) return 12.92 * x;
        constexpr double inv_gamma = 1.0 / 2.4;
        double p = std::pow(x, inv_gamma);
        return std::fma(1.055, p, -0.055);
    }

    static double srgb_decode(double x) {
        x = std::clamp(x, 0.0, 1.0);
        constexpr double cutoff = 0x1.4a7ef9db22d0ep-5;
        if (x <= cutoff) return x / 12.92;
        constexpr double gamma = 2.4;
        return std::pow((x + 0.055) / 1.055, gamma);
    }

public:
    std::string to_string() const {
        std::ostringstream ss;
        if (is_opaque()) {
            ss << "RGB(" << m_r << ", " << m_g << ", " << m_b << ")";
        } else {
            ss << "RGBA(" << m_r << ", " << m_g << ", " << m_b << ", " << m_a << ")";
        }
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const FixedFloatColor& c) {
        os << c.to_string();
        return os;
    }

private:
    static double apply_gamma(double x, double g) { return std::pow(std::clamp(x, 0.0, 1.0), g); }
    static double unapply_gamma(double x, double g) { return std::pow(std::clamp(x, 0.0, 1.0), 1.0 / g); }
};

} // namespace graphics
} // namespace fizmo

#endif // DYNAMIC_COLOR_CLASS_HPP