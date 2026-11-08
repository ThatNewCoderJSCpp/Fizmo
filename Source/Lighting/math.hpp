#ifndef FIZMO_LIGHTING_MATH_HPP
#define FIZMO_LIGHTING_MATH_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace lighting {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kInvPi = 0.31830988618379067154f;
constexpr float kInvTwoPi = 0.15915494309189533577f;
constexpr float kInvFourPi = 0.07957747154594766788f;
constexpr float kDegToRad = 0.01745329251994329577f;
constexpr float kInfinity = std::numeric_limits<float>::infinity();

template <class T, class = void>
struct has_xyz : std::false_type {};

template <class T>
struct has_xyz<T, std::void_t<decltype(std::declval<const T&>().x), decltype(std::declval<const T&>().y), decltype(std::declval<const T&>().z)>> : std::true_type {};

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3() noexcept = default;
    constexpr Vec3(double ax, double ay, double az) noexcept : x(ax), y(ay), z(az) {}

    template <class V, class = std::enable_if_t<has_xyz<V>::value && !std::is_same<std::decay_t<V>, Vec3>::value>>
    Vec3(const V& v) noexcept : x(static_cast<double>(v.x)), y(static_cast<double>(v.y)), z(static_cast<double>(v.z)) {}

    template <class V>
    V to() const { return V{ static_cast<decltype(V{}.x)>(x), static_cast<decltype(V{}.y)>(y), static_cast<decltype(V{}.z)>(z) }; }

    constexpr Vec3 operator+(const Vec3& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Vec3 operator-(const Vec3& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Vec3 operator*(double s) const noexcept { return { x * s, y * s, z * s }; }
    constexpr Vec3 operator/(double s) const noexcept { return { x / s, y / s, z / s }; }
    constexpr Vec3 operator-() const noexcept { return { -x, -y, -z }; }
    constexpr bool operator==(const Vec3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
    constexpr bool operator!=(const Vec3& o) const noexcept { return !(*this == o); }
    constexpr double dot(const Vec3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const noexcept { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
    double length() const noexcept { return std::sqrt(dot(*this)); }
    Vec3 normalized() const noexcept {
        const double l = length();
        return l > 0.0 ? *this / l : Vec3{ 0.0, 0.0, 0.0 };
    }
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3f() noexcept = default;
    constexpr Vec3f(float ax, float ay, float az) noexcept : x(ax), y(ay), z(az) {}
    explicit constexpr Vec3f(const Vec3& v) noexcept : x(static_cast<float>(v.x)), y(static_cast<float>(v.y)), z(static_cast<float>(v.z)) {}

    constexpr float operator[](int i) const noexcept { return i == 0 ? x : (i == 1 ? y : z); }
    float& operator[](int i) noexcept { return i == 0 ? x : (i == 1 ? y : z); }
    constexpr Vec3f operator+(const Vec3f& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Vec3f operator-(const Vec3f& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Vec3f operator*(float s) const noexcept { return { x * s, y * s, z * s }; }
    constexpr Vec3f operator*(const Vec3f& o) const noexcept { return { x * o.x, y * o.y, z * o.z }; }
    constexpr Vec3f operator/(float s) const noexcept { return { x / s, y / s, z / s }; }
    constexpr Vec3f operator-() const noexcept { return { -x, -y, -z }; }
    Vec3f& operator+=(const Vec3f& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3f& operator-=(const Vec3f& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3f& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
    Vec3 to_double() const noexcept { return { x, y, z }; }
};

constexpr Vec3f operator*(float s, const Vec3f& v) noexcept { return v * s; }
constexpr float dot(const Vec3f& a, const Vec3f& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3f cross(const Vec3f& a, const Vec3f& b) noexcept { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float length(const Vec3f& v) noexcept { return std::sqrt(dot(v, v)); }
constexpr float length_squared(const Vec3f& v) noexcept { return dot(v, v); }
inline Vec3f normalize(const Vec3f& v) noexcept {
    const float l = length(v);
    return l > 0.0f ? v / l : Vec3f{ 0.0f, 0.0f, 0.0f };
}
inline Vec3f vmin(const Vec3f& a, const Vec3f& b) noexcept { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
inline Vec3f vmax(const Vec3f& a, const Vec3f& b) noexcept { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }
inline Vec3f reflect(const Vec3f& v, const Vec3f& n) noexcept { return v - n * (2.0f * dot(v, n)); }
inline Vec3f lerp(const Vec3f& a, const Vec3f& b, float t) noexcept { return a + (b - a) * t; }
inline float clamp01(float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float smoothstep(float a, float b, float x) noexcept {
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

struct Frame {
    Vec3f t;
    Vec3f b;
    Vec3f n;

    Frame() = default;
    explicit Frame(const Vec3f& normal) noexcept : n(normal) {
        const float sign = std::copysign(1.0f, n.z);
        const float a = -1.0f / (sign + n.z);
        const float c = n.x * n.y * a;
        t = { 1.0f + sign * n.x * n.x * a, sign * c, -sign * n.x };
        b = { c, sign + n.y * n.y * a, -n.y };
    }

    Vec3f to_world(const Vec3f& v) const noexcept { return t * v.x + b * v.y + n * v.z; }
    Vec3f to_local(const Vec3f& v) const noexcept { return { dot(v, t), dot(v, b), dot(v, n) }; }
};

struct Transform {
    std::array<float, 12> m{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };

    static Transform identity() noexcept { return {}; }

    static Transform translate(const Vec3& p) noexcept {
        Transform t;
        t.m[3] = static_cast<float>(p.x);
        t.m[7] = static_cast<float>(p.y);
        t.m[11] = static_cast<float>(p.z);
        return t;
    }

    static Transform scale(const Vec3& s) noexcept {
        Transform t;
        t.m[0] = static_cast<float>(s.x);
        t.m[5] = static_cast<float>(s.y);
        t.m[10] = static_cast<float>(s.z);
        return t;
    }

    static Transform from_quaternion(double qx, double qy, double qz, double qw) noexcept {
        Transform t;
        const double xx = qx * qx, yy = qy * qy, zz = qz * qz;
        const double xy = qx * qy, xz = qx * qz, yz = qy * qz;
        const double wx = qw * qx, wy = qw * qy, wz = qw * qz;
        t.m[0] = static_cast<float>(1.0 - 2.0 * (yy + zz));
        t.m[1] = static_cast<float>(2.0 * (xy - wz));
        t.m[2] = static_cast<float>(2.0 * (xz + wy));
        t.m[4] = static_cast<float>(2.0 * (xy + wz));
        t.m[5] = static_cast<float>(1.0 - 2.0 * (xx + zz));
        t.m[6] = static_cast<float>(2.0 * (yz - wx));
        t.m[8] = static_cast<float>(2.0 * (xz - wy));
        t.m[9] = static_cast<float>(2.0 * (yz + wx));
        t.m[10] = static_cast<float>(1.0 - 2.0 * (xx + yy));
        return t;
    }

    static Transform trs(const Vec3& position, double qx, double qy, double qz, double qw, const Vec3& s) noexcept {
        return translate(position) * from_quaternion(qx, qy, qz, qw) * scale(s);
    }

    static Transform from_rows(const float* rows12) noexcept {
        Transform t;
        std::copy(rows12, rows12 + 12, t.m.begin());
        return t;
    }

    Transform operator*(const Transform& o) const noexcept {
        Transform r;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                float v = m[i * 4 + 0] * o.m[0 * 4 + j] + m[i * 4 + 1] * o.m[1 * 4 + j] + m[i * 4 + 2] * o.m[2 * 4 + j];
                if (j == 3) v += m[i * 4 + 3];
                r.m[i * 4 + j] = v;
            }
        }
        return r;
    }

    Vec3f point(const Vec3f& p) const noexcept {
        return { m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11] };
    }

    Vec3f vector(const Vec3f& v) const noexcept {
        return { m[0] * v.x + m[1] * v.y + m[2] * v.z, m[4] * v.x + m[5] * v.y + m[6] * v.z, m[8] * v.x + m[9] * v.y + m[10] * v.z };
    }

    Vec3f normal(const Vec3f& n) const noexcept {
        const float a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9], i = m[10];
        const Vec3f c0{ e * i - f * h, f * g - d * i, d * h - e * g };
        const Vec3f c1{ c * h - b * i, a * i - c * g, b * g - a * h };
        const Vec3f c2{ b * f - c * e, c * d - a * f, a * e - b * d };
        const float det = a * c0.x + b * c0.y + c * c0.z;
        const Vec3f r{ c0.x * n.x + c0.y * n.y + c0.z * n.z, c1.x * n.x + c1.y * n.y + c1.z * n.z, c2.x * n.x + c2.y * n.y + c2.z * n.z };
        return normalize(det < 0.0f ? -r : r);
    }
};

struct Rng {
    std::uint64_t state = 0x853c49e6748fea9bULL;
    std::uint64_t inc = 0xda3e39cb94b95bdbULL;

    Rng() = default;
    Rng(std::uint64_t seed, std::uint64_t stream) noexcept {
        state = 0u;
        inc = (stream << 1u) | 1u;
        next();
        state += seed;
        next();
    }

    std::uint32_t next() noexcept {
        const std::uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        const std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    float uniform() noexcept { return std::min(static_cast<float>(next() >> 8) * (1.0f / 16777216.0f), 0.99999994f); }
};

inline std::uint64_t hash_combine(std::uint64_t a, std::uint64_t b) noexcept {
    std::uint64_t h = a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    return h;
}

inline Vec3f sample_cosine_hemisphere(float u1, float u2) noexcept {
    const float r = std::sqrt(u1);
    const float phi = kTwoPi * u2;
    return { r * std::cos(phi), r * std::sin(phi), std::sqrt(std::max(0.0f, 1.0f - u1)) };
}

inline Vec3f sample_uniform_sphere(float u1, float u2) noexcept {
    const float z = 1.0f - 2.0f * u1;
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = kTwoPi * u2;
    return { r * std::cos(phi), r * std::sin(phi), z };
}

inline Vec3f sample_uniform_cone(float u1, float u2, float cos_max) noexcept {
    const float cos_t = 1.0f - u1 * (1.0f - cos_max);
    const float sin_t = std::sqrt(std::max(0.0f, 1.0f - cos_t * cos_t));
    const float phi = kTwoPi * u2;
    return { sin_t * std::cos(phi), sin_t * std::sin(phi), cos_t };
}

inline void sample_uniform_disk(float u1, float u2, float& x, float& y) noexcept {
    const float a = 2.0f * u1 - 1.0f, b = 2.0f * u2 - 1.0f;
    if (a == 0.0f && b == 0.0f) {
        x = y = 0.0f;
        return;
    }
    float r, theta;
    if (std::fabs(a) > std::fabs(b)) {
        r = a;
        theta = (kPi / 4.0f) * (b / a);
    } else {
        r = b;
        theta = (kPi / 2.0f) - (kPi / 4.0f) * (a / b);
    }
    x = r * std::cos(theta);
    y = r * std::sin(theta);
}

inline float power_heuristic(float a, float b) noexcept {
    const float a2 = a * a, b2 = b * b;
    return a2 + b2 > 0.0f ? a2 / (a2 + b2) : 0.0f;
}

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_MATH_HPP
