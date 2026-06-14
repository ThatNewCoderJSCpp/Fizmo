#ifndef FIZMO_POINTS_CLASSES_HPP
#define FIZMO_POINTS_CLASSES_HPP

#include <type_traits>
#include <algorithm>
#include "../../Basic/fizmo_defines.hpp"
#include "../../Vectors/vectors.hpp"

namespace fizmo {

template <typename X = double, typename Y = X, typename = typename std::enable_if<std::is_arithmetic<X>::value && std::is_arithmetic<Y>::value>::type>
class Point2D {
public:
    X x;
    Y y;
    using common_t = typename std::common_type<X, Y, float>::type;

public:
    constexpr Point2D() noexcept : x(0), y(0) {}
    constexpr Point2D(const Point2D& p) noexcept = default;
    constexpr Point2D(Point2D&& p) noexcept = default;
    constexpr Point2D(const X x_, const Y y_) noexcept : x(x_), y(y_) {}
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator=(const Point2D& p) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator=(Point2D&& p) noexcept = default;

    template <typename OX, typename OY>
    constexpr Point2D(const Point2D<OX, OY>& p) noexcept : x(static_cast<X>(p.x)), y(static_cast<Y>(p.y)) {}

    template <typename OX, typename OY>
    constexpr Point2D(Point2D<OX, OY>&& p) noexcept : x(static_cast<X>(p.x)), y(static_cast<Y>(p.y)) {}

    template <typename OX, typename OY>
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator=(const Point2D& p) noexcept {
        x = static_cast<X>(p.x);
        y = static_cast<Y>(p.y);
    }

    template <typename OX, typename OY>
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator=(Point2D&& p) {
        x = static_cast<X>(p.x);
        y = static_cast<Y>(p.y);
    }

    template <typename OX, typename OY>
    constexpr Point2D(const Vector2D<OX, OY>& v) noexcept : x(static_cast<X>(v.x)), y(static_cast<Y>(v.y)) {}

public:
    constexpr explicit operator Vector2D<X, Y>() const noexcept { return Vector2D<X, Y>(x, y); }
    constexpr explicit operator Vector3D<X, Y, common_t>() const noexcept { return Vector3D<X, Y, common_t>(x, y, static_cast<common_t>(0)); }

public:
    constexpr bool operator==(const Point2D& p) const noexcept { return x == p.x && y == p.y; }
    constexpr bool operator!=(const Point2D& p) const noexcept { return !(*this == p); }

public:
    OPTIONAL_CPP14_CONSTEXPR Point2D& negate() noexcept {
        x = -x;
        y = -y;
        return *this;
    }

    constexpr Point2D operator-() const noexcept { return Point2D(-x, -y); }

    template <typename OX, typename OY>
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator+=(const Point2D<OX, OY>& p) noexcept {
        x += static_cast<X>(p.x);
        y += static_cast<Y>(p.y);
        return *this;
    }

    template <typename OX, typename OY>
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator-=(const Point2D<OX, OY>& p) noexcept {
        x -= static_cast<X>(p.x);
        y -= static_cast<Y>(p.y);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator*=(S s) noexcept {
        x = static_cast<X>(x * s);
        y = static_cast<Y>(y * s);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Point2D& operator/=(S s) noexcept {
        x = static_cast<X>(x / s);
        y = static_cast<Y>(y / s);
        return *this;
    }

    template <typename A, typename B, typename C, typename D>
    friend constexpr auto operator+(const Point2D<A,B>& a, const Point2D<C,D>& b) noexcept {
        using R = typename std::common_type<A,B,C,D>::type;
        return Point2D<R,R>(static_cast<R>(a.x) + static_cast<R>(b.x), static_cast<R>(a.y) + static_cast<R>(b.y));
    }

    template <typename A, typename B, typename C, typename D>
    friend constexpr auto operator-(const Point2D<A,B>& a, const Point2D<C,D>& b) noexcept {
        using R = typename std::common_type<A,B,C,D>::type;
        return Point2D<R,R>(static_cast<R>(a.x) - static_cast<R>(b.x), static_cast<R>(a.y) - static_cast<R>(b.y));
    }

    template <typename A, typename B, typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator*(const Point2D<A,B>& p, S s) noexcept {
        using R = typename std::common_type<A,B,S>::type;
        return Point2D<R,R>(static_cast<R>(p.x) * s, static_cast<R>(p.y) * s);
    }

    template <typename S, typename A, typename B, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator*(S s, const Point2D<A,B>& p) noexcept { return p * s; }

    template <typename A, typename B, typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator/(const Point2D<A,B>& p, S s) noexcept {
        using R = typename std::common_type<A,B,S>::type;
        return Point2D<R,R>(static_cast<R>(p.x) / s, static_cast<R>(p.y) / s);
    }

    template <typename OX, typename OY>
    constexpr common_t distance_to(const Point2D<OX,OY>& p) const noexcept {
        common_t dx = static_cast<common_t>(x) - static_cast<common_t>(p.x);
        common_t dy = static_cast<common_t>(y) - static_cast<common_t>(p.y);
        return math::sqrt_constexpr(dx*dx + dy*dy);
    }

    constexpr common_t length() const noexcept {
        return math::sqrt_constexpr(static_cast<common_t>(x)*static_cast<common_t>(x) + static_cast<common_t>(y)*static_cast<common_t>(y));
    }

    template <typename A, typename B>
    friend std::ostream& operator<<(std::ostream& os, const Point2D<A, B>& p) {
        os << "(" << p.x << ", " << p.y << ")";
        return os;
    }
};

template <
    typename X = double,
    typename Y = X,
    typename Z = X,
    typename = typename std::enable_if<
        std::is_arithmetic<X>::value &&
        std::is_arithmetic<Y>::value &&
        std::is_floating_point<Z>::value
    >::type
>
class Point3D {
public:
    X x;
    Y y;
    Z z;

    using common_t = typename std::common_type<X, Y, Z, float>::type;

public:
    constexpr Point3D() noexcept : x(0), y(0), z(0) {}
    constexpr Point3D(const X& x_, const Y& y_, const Z& z_) noexcept : x(x_), y(y_), z(z_) {}
    constexpr Point3D(const Point3D&) noexcept = default;
    constexpr Point3D(Point3D&&) noexcept = default;
    constexpr Point3D(const X x_, const Y y_, const Z z_) noexcept : x(x_), y(y_), z(z_) {}
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator=(const Point3D&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator=(Point3D&&) noexcept = default;

    template <typename A, typename B, typename C>
    constexpr Point3D(const Point3D<A,B,C>& p) noexcept : x(static_cast<X>(p.x)), y(static_cast<Y>(p.y)), z(static_cast<Z>(p.z)) {}

    template <typename A, typename B, typename C>
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator=(const Point3D<A,B,C>& p) noexcept {
        x = static_cast<X>(p.x);
        y = static_cast<Y>(p.y);
        z = static_cast<Z>(p.z);
        return *this;
    }

    template <typename VX, typename VY>
    constexpr Point3D(const Vector3D<VX, VY>& v) noexcept : x(static_cast<X>(v.x)), y(static_cast<Y>(v.y)), z(static_cast<Z>(v.z)) {}

public:
    constexpr explicit operator Vector3D<X,Y>() const noexcept { return Vector3D<X,Y>(x, y, z); }
    constexpr explicit operator Vector2D<X,Y>() const noexcept { return Vector2D<X,Y>(x, y); }

public:
    constexpr bool operator==(const Point3D& p) const noexcept { return x == p.x && y == p.y && z == p.z; }
    constexpr bool operator!=(const Point3D& p) const noexcept { return !(*this == p); }

public:
    OPTIONAL_CPP14_CONSTEXPR Point3D& negate() noexcept {
        x = -x;
        y = -y;
        z = -z;
        return *this;
    }

    constexpr Point3D operator-() const noexcept { return Point3D(-x, -y, -z); }

    template <typename A, typename B, typename C>
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator+=(const Point3D<A,B,C>& p) noexcept {
        x += static_cast<X>(p.x);
        y += static_cast<Y>(p.y);
        z += static_cast<Z>(p.z);
        return *this;
    }

    template <typename A, typename B, typename C>
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator-=(const Point3D<A,B,C>& p) noexcept {
        x -= static_cast<X>(p.x);
        y -= static_cast<Y>(p.y);
        z -= static_cast<Z>(p.z);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator*=(S s) noexcept {
        x = static_cast<X>(x * s);
        y = static_cast<Y>(y * s);
        z = static_cast<Z>(z * s);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Point3D& operator/=(S s) noexcept {
        x = static_cast<X>(x / s);
        y = static_cast<Y>(y / s);
        z = static_cast<Z>(z / s);
        return *this;
    }

    template <typename A, typename B, typename C>
    friend constexpr auto operator+(const Point3D& a, const Point3D<A,B,C>& b) noexcept {
        using R = typename std::common_type<X,Y,Z,A,B,C>::type;
        return Point3D<R,R,R>(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    template <typename A, typename B, typename C>
    friend constexpr auto operator-(const Point3D& a, const Point3D<A,B,C>& b) noexcept {
        using R = typename std::common_type<X,Y,Z,A,B,C>::type;
        return Point3D<R,R,R>(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator*(const Point3D& p, S s) noexcept {
        using R = typename std::common_type<X,Y,Z,S>::type;
        return Point3D<R,R,R>(p.x * s, p.y * s, p.z * s);
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator/(const Point3D& p, S s) noexcept {
        using R = typename std::common_type<X,Y,Z,S>::type;
        return Point3D<R,R,R>(p.x / s, p.y / s, p.z / s);
    }

public:
    template <typename A, typename B, typename C>
    constexpr common_t distance_to(const Point3D<A,B,C>& p) const noexcept {
        common_t dx = static_cast<common_t>(x) - static_cast<common_t>(p.x);
        common_t dy = static_cast<common_t>(y) - static_cast<common_t>(p.y);
        common_t dz = static_cast<common_t>(z) - static_cast<common_t>(p.z);
        return math::sqrt_constexpr(dx*dx + dy*dy + dz*dz);
    }

    constexpr common_t length() const noexcept {
        return math::sqrt_constexpr(
            static_cast<common_t>(x)*static_cast<common_t>(x) +
            static_cast<common_t>(y)*static_cast<common_t>(y) +
            static_cast<common_t>(z)*static_cast<common_t>(z)
        );
    }

    template <typename A, typename B, typename C>
    friend std::ostream& operator<<(std::ostream& os, const Point3D<A, B, C>& p) {
        os << "(" << p.x << ", " << p.y << ", " << p.z << ")";
        return os;
    }
};

template <
    typename X = double,
    typename Y = X,
    typename Z = Y,
    typename W = Z,
    typename = typename std::enable_if<
        std::is_arithmetic<X>::value &&
        std::is_arithmetic<Y>::value &&
        std::is_arithmetic<Z>::value &&
        std::is_floating_point<W>::value
    >::type
>
class Point4D {
public:
    X x;
    Y y;
    Z z;
    W w;

    using common_t = typename std::common_type<X, Y, Z, W, float>::type;

public:
    constexpr Point4D() noexcept : x(0), y(0), z(0), w(0) {}
    constexpr Point4D(const X& x_, const Y& y_, const Z& z_, const W& w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Point4D(const X x_, const Y y_, const Z z_, const W w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Point4D(const Point4D&) noexcept = default;
    constexpr Point4D(Point4D&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator=(const Point4D&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator=(Point4D&&) noexcept = default;

    template <typename A, typename B, typename C, typename D>
    constexpr Point4D(const Point4D<A,B,C,D>& p) noexcept
        : x(static_cast<X>(p.x)), y(static_cast<Y>(p.y)), z(static_cast<Z>(p.z)), w(static_cast<W>(p.w)) {}

    template <typename A, typename B, typename C, typename D>
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator=(const Point4D<A,B,C,D>& p) noexcept {
        x = static_cast<X>(p.x);
        y = static_cast<Y>(p.y);
        z = static_cast<Z>(p.z);
        w = static_cast<W>(p.w);
        return *this;
    }

    template <typename A, typename B, typename C>
    constexpr Point4D(const Point3D<A,B,C>& p, W w_ = W(0)) noexcept
        : x(static_cast<X>(p.x)), y(static_cast<Y>(p.y)), z(static_cast<Z>(p.z)), w(w_) {}

    template <typename VX, typename VY>
    constexpr Point4D(const Vector3D<VX, VY>& v, W w_ = W(0)) noexcept
        : x(static_cast<X>(v.x)), y(static_cast<Y>(v.y)), z(static_cast<Z>(v.z)), w(w_) {}

public:
    constexpr explicit operator Point3D<X,Y,Z>() const noexcept { return Point3D<X,Y,Z>(x, y, z); }
    constexpr explicit operator Point2D<X,Y>()   const noexcept { return Point2D<X,Y>(x, y); }

public:
    constexpr bool operator==(const Point4D& p) const noexcept { return x == p.x && y == p.y && z == p.z && w == p.w; }
    constexpr bool operator!=(const Point4D& p) const noexcept { return !(*this == p); }

public:
    OPTIONAL_CPP14_CONSTEXPR Point4D& negate() noexcept {
        x = -x; y = -y; z = -z; w = -w;
        return *this;
    }

    constexpr Point4D operator-() const noexcept { return Point4D(-x, -y, -z, -w); }

    template <typename A, typename B, typename C, typename D>
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator+=(const Point4D<A,B,C,D>& p) noexcept {
        x += static_cast<X>(p.x); y += static_cast<Y>(p.y);
        z += static_cast<Z>(p.z); w += static_cast<W>(p.w);
        return *this;
    }

    template <typename A, typename B, typename C, typename D>
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator-=(const Point4D<A,B,C,D>& p) noexcept {
        x -= static_cast<X>(p.x); y -= static_cast<Y>(p.y);
        z -= static_cast<Z>(p.z); w -= static_cast<W>(p.w);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator*=(S s) noexcept {
        x = static_cast<X>(x * s); y = static_cast<Y>(y * s);
        z = static_cast<Z>(z * s); w = static_cast<W>(w * s);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR Point4D& operator/=(S s) noexcept {
        x = static_cast<X>(x / s); y = static_cast<Y>(y / s);
        z = static_cast<Z>(z / s); w = static_cast<W>(w / s);
        return *this;
    }

    template <typename A, typename B, typename C, typename D>
    friend constexpr auto operator+(const Point4D& a, const Point4D<A,B,C,D>& b) noexcept {
        using R = typename std::common_type<X,Y,Z,W,A,B,C,D>::type;
        return Point4D<R,R,R,R>(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
    }

    template <typename A, typename B, typename C, typename D>
    friend constexpr auto operator-(const Point4D& a, const Point4D<A,B,C,D>& b) noexcept {
        using R = typename std::common_type<X,Y,Z,W,A,B,C,D>::type;
        return Point4D<R,R,R,R>(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator*(const Point4D& p, S s) noexcept {
        using R = typename std::common_type<X,Y,Z,W,S>::type;
        return Point4D<R,R,R,R>(p.x * s, p.y * s, p.z * s, p.w * s);
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator*(S s, const Point4D& p) noexcept {
        return p * s;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    friend constexpr auto operator/(const Point4D& p, S s) noexcept {
        using R = typename std::common_type<X,Y,Z,W,S>::type;
        return Point4D<R,R,R,R>(p.x / s, p.y / s, p.z / s, p.w / s);
    }

public:
    template <typename A, typename B, typename C, typename D>
    constexpr common_t distance_to(const Point4D<A,B,C,D>& p) const noexcept {
        common_t dx = static_cast<common_t>(x) - static_cast<common_t>(p.x);
        common_t dy = static_cast<common_t>(y) - static_cast<common_t>(p.y);
        common_t dz = static_cast<common_t>(z) - static_cast<common_t>(p.z);
        common_t dw = static_cast<common_t>(w) - static_cast<common_t>(p.w);
        return math::sqrt_constexpr(dx*dx + dy*dy + dz*dz + dw*dw);
    }

    constexpr common_t length() const noexcept {
        return math::sqrt_constexpr(
            static_cast<common_t>(x)*static_cast<common_t>(x) +
            static_cast<common_t>(y)*static_cast<common_t>(y) +
            static_cast<common_t>(z)*static_cast<common_t>(z) +
            static_cast<common_t>(w)*static_cast<common_t>(w)
        );
    }

    template <typename A, typename B, typename C, typename D>
    friend std::ostream& operator<<(std::ostream& os, const Point4D<A, B, C, D>& p) {
        os << "(" << p.x << ", " << p.y << ", " << p.z << ", " << p.w << ")";
        return os;
    }
};

template <
    std::size_t N,
    typename T = double,
    typename = typename std::enable_if<std::is_arithmetic<T>::value>::type
>
class PointN {
public:
    std::array<T, N> data{};
    using common_t = typename std::common_type<float, T>;

public:
    constexpr PointN() noexcept = default;
    constexpr explicit PointN(const std::array<T,N>& arr) noexcept : data(arr) {}

    template <typename... Args, typename = typename std::enable_if<sizeof...(Args) == N>::type>
    constexpr PointN(Args&&... args) noexcept : data{ static_cast<T>(args)... } {}

    constexpr PointN(const PointN&) noexcept = default;
    constexpr PointN(PointN&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR PointN& operator=(const PointN&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR PointN& operator=(PointN&&) noexcept = default;

    template <typename X, typename Y>
    constexpr PointN(const Point2D<X,Y>& p) noexcept {
        if (N > 0) data[0] = static_cast<T>(p.x);
        if (N > 1) data[1] = static_cast<T>(p.y);
        for (std::size_t i = 2; i < N; ++i) data[i] = T(0);
    }

    template <typename X, typename Y>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator=(const Point2D<X,Y>& p) noexcept {
        if (N > 0) data[0] = static_cast<T>(p.x);
        if (N > 1) data[1] = static_cast<T>(p.y);
        for (std::size_t i = 2; i < N; ++i) data[i] = T(0);
        return *this;
    }

    template <typename X, typename Y, typename Z>
    constexpr PointN(const Point3D<X,Y,Z>& p) noexcept {
        if (N > 0) data[0] = static_cast<T>(p.x);
        if (N > 1) data[1] = static_cast<T>(p.y);
        if (N > 2) data[2] = static_cast<T>(p.z);
        for (std::size_t i = 3; i < N; ++i) data[i] = T(0);
    }

    template <typename X, typename Y, typename Z>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator=(const Point3D<X,Y,Z>& p) noexcept {
        if (N > 0) data[0] = static_cast<T>(p.x);
        if (N > 1) data[1] = static_cast<T>(p.y);
        if (N > 2) data[2] = static_cast<T>(p.z);
        for (std::size_t i = 3; i < N; ++i) data[i] = T(0);
        return *this;
    }

    template <typename X, typename Y, typename Z, typename W>
    constexpr PointN(const Point4D<X,Y,Z,W>& p) noexcept {
        if (N > 0) data[0] = static_cast<T>(p.x);
        if (N > 1) data[1] = static_cast<T>(p.y);
        if (N > 2) data[2] = static_cast<T>(p.z);
        if (N > 3) data[3] = static_cast<T>(p.w);
        for (std::size_t i = 4; i < N; ++i) data[i] = T(0);
    }

    template <typename X, typename Y, typename Z, typename W>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator=(const Point4D<X,Y,Z,W>& p) noexcept {
        if (N > 0) data[0] = static_cast<T>(p.x);
        if (N > 1) data[1] = static_cast<T>(p.y);
        if (N > 2) data[2] = static_cast<T>(p.z);
        if (N > 3) data[3] = static_cast<T>(p.w);
        for (std::size_t i = 4; i < N; ++i) data[i] = T(0);
        return *this;
    }

public:
    constexpr explicit operator VectorN<N,T>() const noexcept { return VectorN<N,T>(data); }

public:
    constexpr T& operator[](std::size_t i) noexcept { return data[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return data[i]; }

public:
    constexpr bool operator==(const PointN& o) const noexcept { return data == o.data; }
    constexpr bool operator!=(const PointN& o) const noexcept { return !(*this == o); }

public:
    OPTIONAL_CPP14_CONSTEXPR PointN& negate() noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] = -data[i];
        return *this;
    }

    template <typename X>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator+=(const PointN<N,X>& o) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] += static_cast<T>(o.data[i]);
        return *this;
    }

    template <typename X>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator-=(const PointN<N,X>& o) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] -= static_cast<T>(o.data[i]);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator*=(S s) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] = static_cast<T>(data[i] * s);
        return *this;
    }

    template <typename S, typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR PointN& operator/=(S s) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] = static_cast<T>(data[i] / s);
        return *this;
    }

    constexpr PointN operator-() const noexcept {
        PointN r(*this);
        for (std::size_t i = 0; i < N; ++i) r.data[i] = -r.data[i];
        return r;
    }

    constexpr PointN operator+(const PointN& o) const noexcept {
        PointN r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] + o.data[i];
        return r;
    }

    constexpr PointN operator-(const PointN& o) const noexcept {
        PointN r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] - o.data[i];
        return r;
    }

    constexpr PointN operator*(T s) const noexcept {
        PointN r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] * s;
        return r;
    }

    constexpr PointN operator/(T s) const noexcept {
        PointN r;
        for (std::size_t i = 0; i < N; ++i) r.data[i] = data[i] / s;
        return r;
    }

public:
    constexpr common_t distance_to(const PointN& o) const noexcept {
        common_t sum = common_t(0);
        for (std::size_t i = 0; i < N; ++i) {
            T d = data[i] - o.data[i];
            sum += d * d;
        }
        return math::sqrt_constexpr(sum);
    }

    constexpr common_t length() const noexcept {
        common_t sum = common_t(0);
        for (std::size_t i = 0; i < N; ++i) sum += data[i] * data[i];
        return math::sqrt_constexpr(sum);
    }

    template <std::size_t _N, typename _T>
    friend std::ostream& operator<<(std::ostream& os, const PointN<_N, _T>& p) {
        os << "(";
        for (unsigned int n = 0; n < _N; ++n) {
            os << p.data[n];
            if (n != N - 1) os << ", ";
        }
        os << ")";
        return os;
    }
};

} // namespace fizmo

#endif // FIZMO_POINTS_CLASSES_HPP