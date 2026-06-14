#ifndef FIZMO_SPECIALIZED_SQUARE_MATRICES_HPP
#define FIZMO_SPECIALIZED_SQUARE_MATRICES_HPP

#include "matrix.hpp"

namespace fizmo {
namespace math {

template <typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
struct Matrix2x2 {
    using value_type = T;
    static constexpr std::size_t size = 2;

    // [r0c0, r0c1, r1c0, r1c1]
    std::array<T, 4> data{};

    constexpr Matrix2x2() noexcept = default;
    constexpr Matrix2x2(T m00, T m01, T m10, T m11) noexcept : data{m00, m01, m10, m11} {}
    constexpr explicit Matrix2x2(const std::array<T, 4>& d) noexcept : data(d) {}

    constexpr explicit Matrix2x2(const Matrix<2, T>& m) noexcept
        : data{m.data[0][0], m.data[0][1],
               m.data[1][0], m.data[1][1]} {}

    constexpr explicit operator Matrix<2, T>() const noexcept {
        return Matrix<2, T>(std::array<std::array<T, 2>, 2>{{
            {data[0], data[1]},
            {data[2], data[3]}
        }});
    }

    static constexpr Matrix2x2 identity() noexcept { return { T(1), T(0), T(0), T(1) }; }
    static constexpr Matrix2x2 filled(T v) noexcept { return { v, v, v, v }; }
    static constexpr Matrix2x2 diagonal(T d0 = T(1), T d1 = T(1)) noexcept { return { d0, T(0), T(0), d1 }; }
    static constexpr Matrix2x2 diagonal(const std::array<T, 2>& d) noexcept { return diagonal(d[0], d[1]); }
    static constexpr Matrix2x2 scaling(T s) noexcept { return diagonal(s, s); }
    static constexpr Matrix2x2 scaling(T sx, T sy) noexcept { return diagonal(sx, sy); }

    static constexpr Matrix2x2 rotation(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);
        return { c, -s, s,  c };
    }

    static constexpr Matrix2x2 shear(T kx, T ky) noexcept {
        return { T(1), kx, ky, T(1) };
    }

    static constexpr Matrix2x2 reflection(T nx, T ny) noexcept {
        return { T(1) - T(2)*nx*nx, -T(2)*nx*ny, -T(2)*nx*ny, T(1) - T(2)*ny*ny };
    }

    static constexpr Matrix2x2 outer(const std::array<T, 2>& a, const std::array<T, 2>& b) noexcept {
        return { a[0]*b[0], a[0]*b[1], a[1]*b[0], a[1]*b[1] };
    }

    constexpr T*       operator[](std::size_t r) noexcept       { return &data[r * 2]; }
    constexpr const T* operator[](std::size_t r) const noexcept { return &data[r * 2]; }
    constexpr T&       at(std::size_t r, std::size_t c) noexcept       { return data[r * 2 + c]; }
    constexpr const T& at(std::size_t r, std::size_t c) const noexcept { return data[r * 2 + c]; }
    constexpr std::array<T, 2> row(std::size_t r) const noexcept { return { data[r*2], data[r*2+1] }; }
    constexpr std::array<T, 2> col(std::size_t c) const noexcept { return { data[c], data[2+c] }; }
    constexpr bool operator==(const Matrix2x2& o) const noexcept { return data == o.data; }
    constexpr bool operator!=(const Matrix2x2& o) const noexcept { return data != o.data; }

    constexpr bool approx_equal(const Matrix2x2& o, T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < 4; ++i) if (abs_constexpr(data[i] - o.data[i]) > eps) return false;
        return true;
    }

    constexpr Matrix2x2 operator+(T s) const noexcept { return { data[0]+s, data[1]+s, data[2]+s, data[3]+s }; }
    constexpr Matrix2x2 operator-(T s) const noexcept { return { data[0]-s, data[1]-s, data[2]-s, data[3]-s }; }
    constexpr Matrix2x2 operator*(T s) const noexcept { return { data[0]*s, data[1]*s, data[2]*s, data[3]*s }; }
    constexpr Matrix2x2 operator/(T s) const noexcept { return { data[0]/s, data[1]/s, data[2]/s, data[3]/s }; }
    constexpr Matrix2x2& operator+=(T s) noexcept { for (auto& v : data) v += s; return *this; }
    constexpr Matrix2x2& operator-=(T s) noexcept { for (auto& v : data) v -= s; return *this; }
    constexpr Matrix2x2& operator*=(T s) noexcept { for (auto& v : data) v *= s; return *this; }
    constexpr Matrix2x2& operator/=(T s) noexcept { for (auto& v : data) v /= s; return *this; }

    constexpr Matrix2x2 operator+(const Matrix2x2& o) const noexcept {
        return { data[0]+o.data[0], data[1]+o.data[1], data[2]+o.data[2], data[3]+o.data[3] };
    }

    constexpr Matrix2x2 operator-(const Matrix2x2& o) const noexcept {
        return { data[0]-o.data[0], data[1]-o.data[1], data[2]-o.data[2], data[3]-o.data[3] };
    }

    constexpr Matrix2x2 operator-() const noexcept { return { -data[0], -data[1], -data[2], -data[3] }; }
    constexpr Matrix2x2& operator+=(const Matrix2x2& o) noexcept { for (std::size_t i = 0; i < 4; ++i) data[i] += o.data[i]; return *this; }
    constexpr Matrix2x2& operator-=(const Matrix2x2& o) noexcept { for (std::size_t i = 0; i < 4; ++i) data[i] -= o.data[i]; return *this; }

    constexpr Matrix2x2 operator*(const Matrix2x2& o) const noexcept {
        return { data[0]*o.data[0] + data[1]*o.data[2],
                 data[0]*o.data[1] + data[1]*o.data[3],
                 data[2]*o.data[0] + data[3]*o.data[2],
                 data[2]*o.data[1] + data[3]*o.data[3] };
    }

    constexpr Matrix2x2& operator*=(const Matrix2x2& o) noexcept { return *this = *this * o; }

    constexpr std::array<T, 2> operator*(const std::array<T, 2>& v) const noexcept {
        return { data[0]*v[0] + data[1]*v[1], data[2]*v[0] + data[3]*v[1] };
    }

    constexpr Vector2D<T> operator*(const Vector2D<T>& v) const noexcept {
        return { data[0]*v.x + data[1]*v.y, data[2]*v.x + data[3]*v.y };
    }

    constexpr Matrix2x2 hadamard(const Matrix2x2& o) const noexcept {
        return { data[0]*o.data[0], data[1]*o.data[1], data[2]*o.data[2], data[3]*o.data[3] };
    }

    constexpr T trace() const noexcept { return data[0] + data[3]; }
    constexpr T determinant() const noexcept { return data[0]*data[3] - data[1]*data[2]; }

    constexpr Matrix2x2 transpose() const noexcept {
        return { data[0], data[2], data[1], data[3] };
    }

    constexpr Matrix2x2 adjugate() const noexcept {
        return {  data[3], -data[1], -data[2],  data[0] };
    }

    constexpr Matrix2x2 inverse() const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) <= std::numeric_limits<T>::epsilon() * T(1e3)) return identity();
        const T inv = T(1) / det;
        return {  data[3]*inv, -data[1]*inv, -data[2]*inv,  data[0]*inv };
    }

    constexpr Matrix2x2 cofactor_matrix() const noexcept {
        return {  data[3], -data[2], -data[1],  data[0] };
    }

    constexpr T frobenius_norm() const noexcept {
        return sqrt_constexpr(data[0]*data[0] + data[1]*data[1] + data[2]*data[2] + data[3]*data[3]);
    }

    constexpr T max_abs() const noexcept {
        T m = abs_constexpr(data[0]);
        for (std::size_t i = 1; i < 4; ++i) { T v = abs_constexpr(data[i]); if (v > m) m = v; }
        return m;
    }

    constexpr bool is_symmetric() const noexcept { return data[1] == data[2]; }
    constexpr bool is_diagonal()  const noexcept { return data[1] == T(0) && data[2] == T(0); }
    constexpr bool is_identity(T eps = constants::epsilon<T>()) const noexcept { return approx_equal(identity(), eps); }
    constexpr bool is_zero(T eps = constants::epsilon<T>())     const noexcept { return approx_equal(Matrix2x2{}, eps); }
    constexpr T sum()     const noexcept { return data[0] + data[1] + data[2] + data[3]; }
    constexpr T product() const noexcept { return data[0] * data[1] * data[2] * data[3]; }

    // Solve Ax = b (Cramer's rule) 
    constexpr std::array<T, 2> solve(const std::array<T, 2>& b) const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) <= std::numeric_limits<T>::epsilon() * T(1e3)) return {T(0), T(0)};
        const T inv = T(1) / det;
        return { (data[3]*b[0] - data[1]*b[1]) * inv, (data[0]*b[1] - data[2]*b[0]) * inv };
    }

    constexpr std::array<T, 2> eigenvalues() const noexcept {
        const T tr = trace();
        const T det = determinant();
        const T disc = tr*tr - T(4)*det;

        if (disc < T(0)) {
            // Complex eigenvalues — return real parts
            const T real = tr * T(0.5);
            return { real, real };
        }

        const T sq = sqrt_constexpr(disc);
        return { (tr + sq) * T(0.5), (tr - sq) * T(0.5) };
    }

    constexpr Matrix2x2 apply(T (*fn)(T)) const noexcept {
        return { fn(data[0]), fn(data[1]), fn(data[2]), fn(data[3]) };
    }

    constexpr Matrix2x2 abs_matrix() const noexcept {
        return { abs_constexpr(data[0]), abs_constexpr(data[1]), abs_constexpr(data[2]), abs_constexpr(data[3]) };
    }

    constexpr Matrix2x2 swap_rows() const noexcept { return { data[2], data[3], data[0], data[1] }; }
    constexpr Matrix2x2 swap_cols() const noexcept { return { data[1], data[0], data[3], data[2] }; }

    constexpr Matrix2x2 scale_row(std::size_t r, T s) const noexcept {
        Matrix2x2 m = *this;
        m.data[r*2]   *= s;
        m.data[r*2+1] *= s;
        return m;
    }

    constexpr Matrix2x2 scale_col(std::size_t c, T s) const noexcept {
        Matrix2x2 m = *this;
        m.data[c]   *= s;
        m.data[2+c] *= s;
        return m;
    }

    constexpr std::size_t rank() const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) > std::numeric_limits<T>::epsilon() * T(1e3)) return 2;
        for (std::size_t i = 0; i < 4; ++i) if (abs_constexpr(data[i]) > std::numeric_limits<T>::epsilon() * T(1e3)) return 1;
        return 0;
    }
};

template <typename T>
constexpr Matrix2x2<T> operator*(T s, const Matrix2x2<T>& m) noexcept { return m * s; }

template <typename T>
constexpr Matrix2x2<T> commutator(const Matrix2x2<T>& A, const Matrix2x2<T>& B) noexcept { return A * B - B * A; }

template <typename T>
constexpr T inner_product(const Matrix2x2<T>& A, const Matrix2x2<T>& B) noexcept {
    return A.data[0]*B.data[0] + A.data[1]*B.data[1] + A.data[2]*B.data[2] + A.data[3]*B.data[3];
}

template <typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
struct Matrix3x3 {
    using value_type = T;
    static constexpr std::size_t size = 3;

    // [r0c0, r0c1, r0c2, r1c0, r1c1, r1c2, r2c0, r2c1, r2c2]
    std::array<T, 9> data{};

    constexpr Matrix3x3() noexcept = default;

    constexpr Matrix3x3(T m00, T m01, T m02,
                        T m10, T m11, T m12,
                        T m20, T m21, T m22) noexcept
        : data{m00, m01, m02, m10, m11, m12, m20, m21, m22} {}

    constexpr explicit Matrix3x3(const std::array<T, 9>& d) noexcept : data(d) {}

    constexpr explicit Matrix3x3(const Matrix<3, T>& m) noexcept
        : data{m.data[0][0], m.data[0][1], m.data[0][2],
               m.data[1][0], m.data[1][1], m.data[1][2],
               m.data[2][0], m.data[2][1], m.data[2][2]} {}

    constexpr explicit operator Matrix<3, T>() const noexcept {
        return Matrix<3, T>(std::array<std::array<T, 3>, 3>{{
            {data[0], data[1], data[2]},
            {data[3], data[4], data[5]},
            {data[6], data[7], data[8]}
        }});
    }

    static constexpr Matrix3x3 identity() noexcept {
        return { T(1), T(0), T(0),
                 T(0), T(1), T(0),
                 T(0), T(0), T(1) };
    }

    static constexpr Matrix3x3 filled(T v) noexcept { return { v, v, v, v, v, v, v, v, v }; }

    static constexpr Matrix3x3 diagonal(T d0 = T(1), T d1 = T(1), T d2 = T(1)) noexcept {
        return { d0,   T(0), T(0),
                 T(0), d1,   T(0),
                 T(0), T(0), d2 };
    }

    static constexpr Matrix3x3 diagonal(const std::array<T, 3>& d) noexcept { return diagonal(d[0], d[1], d[2]); }

    static constexpr Matrix3x3 scaling(T s) noexcept { return diagonal(s, s, s); }
    static constexpr Matrix3x3 scaling(T sx, T sy, T sz) noexcept { return diagonal(sx, sy, sz); }

    static constexpr Matrix3x3 rotation_x(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return { T(1), T(0), T(0),
                 T(0),  c,   -s,
                 T(0),  s,    c };
    }

    static constexpr Matrix3x3 rotation_y(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {  c,   T(0),  s,
                 T(0), T(1), T(0),
                 -s,   T(0),  c };
    }

    static constexpr Matrix3x3 rotation_z(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {  c,   -s,   T(0),
                  s,    c,   T(0),
                 T(0), T(0), T(1) };
    }

    static constexpr Matrix3x3 rotation_axis(T ax, T ay, T az, T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        T len = sqrt_constexpr(ax*ax + ay*ay + az*az);
        if (abs_constexpr(len) < std::numeric_limits<T>::epsilon()) return identity();
        ax /= len; ay /= len; az /= len;
        const T c  = cos_constexpr(angle);
        const T s  = sin_constexpr(angle);
        const T ic = T(1) - c;

        return { c + ax*ax*ic,     ax*ay*ic - az*s,  ax*az*ic + ay*s,
                 ay*ax*ic + az*s,  c + ay*ay*ic,     ay*az*ic - ax*s,
                 az*ax*ic - ay*s,  az*ay*ic + ax*s,  c + az*az*ic };
    }

    static constexpr Matrix3x3 shear(std::size_t along, std::size_t by, T k) noexcept {
        Matrix3x3 m = identity();
        if (along != by && along < 3 && by < 3) m.data[along * 3 + by] = k;
        return m;
    }

    static constexpr Matrix3x3 reflection(T nx, T ny, T nz) noexcept {
        return { T(1)-T(2)*nx*nx,     -T(2)*nx*ny,      -T(2)*nx*nz,
                     -T(2)*ny*nx, T(1)-T(2)*ny*ny,      -T(2)*ny*nz,
                     -T(2)*nz*nx,     -T(2)*nz*ny,  T(1)-T(2)*nz*nz };
    }

    static constexpr Matrix3x3 outer(const std::array<T, 3>& a, const std::array<T, 3>& b) noexcept {
        return { a[0]*b[0], a[0]*b[1], a[0]*b[2],
                 a[1]*b[0], a[1]*b[1], a[1]*b[2],
                 a[2]*b[0], a[2]*b[1], a[2]*b[2] };
    }

    static constexpr Matrix3x3 skew_symmetric(T x, T y, T z) noexcept {
        return { T(0), -z,     y,
                  z,   T(0),  -x,
                 -y,    x,   T(0) };
    }

    static constexpr Matrix3x3 translation_2d(T tx, T ty) noexcept {
        return { T(1), T(0), tx,
                 T(0), T(1), ty,
                 T(0), T(0), T(1) };
    }

    static constexpr Matrix3x3 rotation_2d(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {  c,   -s,   T(0),
                  s,    c,   T(0),
                 T(0), T(0), T(1) };
    }

    constexpr T*       operator[](std::size_t r) noexcept       { return &data[r * 3]; }
    constexpr const T* operator[](std::size_t r) const noexcept { return &data[r * 3]; }
    constexpr T&       at(std::size_t r, std::size_t c) noexcept       { return data[r * 3 + c]; }
    constexpr const T& at(std::size_t r, std::size_t c) const noexcept { return data[r * 3 + c]; }
    constexpr std::array<T, 3> row(std::size_t r) const noexcept { return { data[r*3], data[r*3+1], data[r*3+2] }; }
    constexpr std::array<T, 3> col(std::size_t c) const noexcept { return { data[c], data[3+c], data[6+c] }; }
    constexpr bool operator==(const Matrix3x3& o) const noexcept { return data == o.data; }
    constexpr bool operator!=(const Matrix3x3& o) const noexcept { return data != o.data; }

    constexpr bool approx_equal(const Matrix3x3& o, T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < 9; ++i) if (abs_constexpr(data[i] - o.data[i]) > eps) return false;
        return true;
    }

    constexpr Matrix3x3 operator+(T s) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] + s; return m;
    }
    constexpr Matrix3x3 operator-(T s) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] - s; return m;
    }
    constexpr Matrix3x3 operator*(T s) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] * s; return m;
    }
    constexpr Matrix3x3 operator/(T s) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] / s; return m;
    }
    constexpr Matrix3x3& operator+=(T s) noexcept { for (auto& v : data) v += s; return *this; }
    constexpr Matrix3x3& operator-=(T s) noexcept { for (auto& v : data) v -= s; return *this; }
    constexpr Matrix3x3& operator*=(T s) noexcept { for (auto& v : data) v *= s; return *this; }
    constexpr Matrix3x3& operator/=(T s) noexcept { for (auto& v : data) v /= s; return *this; }

    constexpr Matrix3x3 operator+(const Matrix3x3& o) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] + o.data[i]; return m;
    }

    constexpr Matrix3x3 operator-(const Matrix3x3& o) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] - o.data[i]; return m;
    }

    constexpr Matrix3x3 operator-() const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = -data[i]; return m;
    }

    constexpr Matrix3x3& operator+=(const Matrix3x3& o) noexcept { for (std::size_t i = 0; i < 9; ++i) data[i] += o.data[i]; return *this; }
    constexpr Matrix3x3& operator-=(const Matrix3x3& o) noexcept { for (std::size_t i = 0; i < 9; ++i) data[i] -= o.data[i]; return *this; }

    constexpr Matrix3x3 operator*(const Matrix3x3& o) const noexcept {
        const T* a = data.data();
        const T* b = o.data.data();
        return {
            a[0]*b[0] + a[1]*b[3] + a[2]*b[6],
            a[0]*b[1] + a[1]*b[4] + a[2]*b[7],
            a[0]*b[2] + a[1]*b[5] + a[2]*b[8],

            a[3]*b[0] + a[4]*b[3] + a[5]*b[6],
            a[3]*b[1] + a[4]*b[4] + a[5]*b[7],
            a[3]*b[2] + a[4]*b[5] + a[5]*b[8],

            a[6]*b[0] + a[7]*b[3] + a[8]*b[6],
            a[6]*b[1] + a[7]*b[4] + a[8]*b[7],
            a[6]*b[2] + a[7]*b[5] + a[8]*b[8]
        };
    }

    constexpr Matrix3x3& operator*=(const Matrix3x3& o) noexcept { return *this = *this * o; }

    constexpr std::array<T, 3> operator*(const std::array<T, 3>& v) const noexcept {
        return { data[0]*v[0] + data[1]*v[1] + data[2]*v[2],
                 data[3]*v[0] + data[4]*v[1] + data[5]*v[2],
                 data[6]*v[0] + data[7]*v[1] + data[8]*v[2] };
    }

    constexpr Vector3D<T> operator*(const Vector3D<T>& v) const noexcept {
        return { data[0]*v.x + data[1]*v.y + data[2]*v.z,
                 data[3]*v.x + data[4]*v.y + data[5]*v.z,
                 data[6]*v.x + data[7]*v.y + data[8]*v.z };
    }

    constexpr Matrix3x3 hadamard(const Matrix3x3& o) const noexcept {
        Matrix3x3 m; for (std::size_t i = 0; i < 9; ++i) m.data[i] = data[i] * o.data[i]; return m;
    }

    constexpr T trace() const noexcept { return data[0] + data[4] + data[8]; }

    constexpr T determinant() const noexcept {
        return data[0] * (data[4]*data[8] - data[5]*data[7])
             - data[1] * (data[3]*data[8] - data[5]*data[6])
             + data[2] * (data[3]*data[7] - data[4]*data[6]);
    }

    constexpr Matrix3x3 transpose() const noexcept {
        return { data[0], data[3], data[6],
                 data[1], data[4], data[7],
                 data[2], data[5], data[8] };
    }

    constexpr Matrix3x3 cofactor_matrix() const noexcept {
        return {
             (data[4]*data[8] - data[5]*data[7]),
            -(data[3]*data[8] - data[5]*data[6]),
             (data[3]*data[7] - data[4]*data[6]),

            -(data[1]*data[8] - data[2]*data[7]),
             (data[0]*data[8] - data[2]*data[6]),
            -(data[0]*data[7] - data[1]*data[6]),

             (data[1]*data[5] - data[2]*data[4]),
            -(data[0]*data[5] - data[2]*data[3]),
             (data[0]*data[4] - data[1]*data[3])
        };
    }

    constexpr Matrix3x3 adjugate() const noexcept { return cofactor_matrix().transpose(); }

    constexpr Matrix3x3 inverse() const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) <= std::numeric_limits<T>::epsilon() * T(1e3)) return identity();
        return adjugate() / det;
    }

    constexpr T frobenius_norm() const noexcept {
        T s = T(0);
        for (std::size_t i = 0; i < 9; ++i) s += data[i] * data[i];
        return sqrt_constexpr(s);
    }

    constexpr T max_abs() const noexcept {
        T m = abs_constexpr(data[0]);
        for (std::size_t i = 1; i < 9; ++i) { T v = abs_constexpr(data[i]); if (v > m) m = v; }
        return m;
    }

    constexpr bool is_symmetric() const noexcept {
        return data[1] == data[3] && data[2] == data[6] && data[5] == data[7];
    }

    constexpr bool is_diagonal() const noexcept {
        return data[1] == T(0) && data[2] == T(0) && data[3] == T(0) &&
               data[5] == T(0) && data[6] == T(0) && data[7] == T(0);
    }

    constexpr bool is_identity(T eps = constants::epsilon<T>()) const noexcept { return approx_equal(identity(), eps); }
    constexpr bool is_zero(T eps = constants::epsilon<T>())     const noexcept { return approx_equal(Matrix3x3{}, eps); }

    constexpr T sum() const noexcept {
        T s = T(0); for (std::size_t i = 0; i < 9; ++i) s += data[i]; return s;
    }

    constexpr T product() const noexcept {
        T p = T(1); for (std::size_t i = 0; i < 9; ++i) p *= data[i]; return p;
    }

    constexpr std::array<T, 3> row_sums() const noexcept {
        return { data[0]+data[1]+data[2],
                 data[3]+data[4]+data[5],
                 data[6]+data[7]+data[8] };
    }

    constexpr std::array<T, 3> col_sums() const noexcept {
        return { data[0]+data[3]+data[6],
                 data[1]+data[4]+data[7],
                 data[2]+data[5]+data[8] };
    }

    // solve Ax = b 
    constexpr std::array<T, 3> solve(const std::array<T, 3>& b) const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) <= std::numeric_limits<T>::epsilon() * T(1e3)) return {T(0), T(0), T(0)};
        const T inv = T(1) / det;
        
        const T d0 = b[0]*(data[4]*data[8] - data[5]*data[7])
                   - data[1]*(b[1]*data[8] - data[5]*b[2])
                   + data[2]*(b[1]*data[7] - data[4]*b[2]);

        const T d1 = data[0]*(b[1]*data[8] - data[5]*b[2])
                   - b[0]*(data[3]*data[8] - data[5]*data[6])
                   + data[2]*(data[3]*b[2] - b[1]*data[6]);

        const T d2 = data[0]*(data[4]*b[2] - b[1]*data[7])
                   - data[1]*(data[3]*b[2] - b[1]*data[6])
                   + b[0]*(data[3]*data[7] - data[4]*data[6]);
        return { d0*inv, d1*inv, d2*inv };
    }

    constexpr Matrix2x2<T> submatrix(std::size_t dr, std::size_t dc) const noexcept {
        Matrix2x2<T> m;
        std::size_t idx = 0;

        for (std::size_t r = 0; r < 3; ++r) {
            if (r == dr) continue;

            for (std::size_t c = 0; c < 3; ++c) {
                if (c == dc) continue;
                m.data[idx++] = data[r * 3 + c];
            }
        }

        return m;
    }

    constexpr std::size_t rank() const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) > std::numeric_limits<T>::epsilon() * T(1e3)) return 3;
   
        for (std::size_t r = 0; r < 3; ++r)
            for (std::size_t c = 0; c < 3; ++c)
                if (abs_constexpr(submatrix(r, c).determinant()) > std::numeric_limits<T>::epsilon() * T(1e3))
                    return 2;

        for (std::size_t i = 0; i < 9; ++i) if (abs_constexpr(data[i]) > std::numeric_limits<T>::epsilon() * T(1e3)) return 1;
        return 0;
    }

    constexpr Matrix3x3 swap_rows(std::size_t r1, std::size_t r2) const noexcept {
        Matrix3x3 m = *this;
        for (std::size_t c = 0; c < 3; ++c) {
            T tmp = m.data[r1*3+c];
            m.data[r1*3+c] = m.data[r2*3+c];
            m.data[r2*3+c] = tmp;
        }
        return m;
    }

    constexpr Matrix3x3 swap_cols(std::size_t c1, std::size_t c2) const noexcept {
        Matrix3x3 m = *this;

        for (std::size_t r = 0; r < 3; ++r) {
            T tmp = m.data[r*3+c1];
            m.data[r*3+c1] = m.data[r*3+c2];
            m.data[r*3+c2] = tmp;
        }

        return m;
    }

    constexpr Matrix3x3 scale_row(std::size_t r, T s) const noexcept {
        Matrix3x3 m = *this;
        m.data[r*3] *= s; m.data[r*3+1] *= s; m.data[r*3+2] *= s;
        return m;
    }

    constexpr Matrix3x3 scale_col(std::size_t c, T s) const noexcept {
        Matrix3x3 m = *this;
        m.data[c] *= s; m.data[3+c] *= s; m.data[6+c] *= s;
        return m;
    }

    constexpr Matrix3x3 apply(T (*fn)(T)) const noexcept {
        Matrix3x3 m;
        for (std::size_t i = 0; i < 9; ++i) m.data[i] = fn(data[i]);
        return m;
    }

    constexpr Matrix3x3 abs_matrix() const noexcept {
        Matrix3x3 m;
        for (std::size_t i = 0; i < 9; ++i) m.data[i] = abs_constexpr(data[i]);
        return m;
    }

    static constexpr std::array<T, 3> cross(const std::array<T, 3>& a, const std::array<T, 3>& b) noexcept {
        return { a[1]*b[2] - a[2]*b[1],
                 a[2]*b[0] - a[0]*b[2],
                 a[0]*b[1] - a[1]*b[0] };
    }
};

template <typename T>
constexpr Matrix3x3<T> operator*(T s, const Matrix3x3<T>& m) noexcept { return m * s; }

template <typename T>
constexpr Matrix3x3<T> commutator(const Matrix3x3<T>& A, const Matrix3x3<T>& B) noexcept { return A * B - B * A; }

template <typename T>
constexpr T inner_product(const Matrix3x3<T>& A, const Matrix3x3<T>& B) noexcept {
    T s = T(0);
    for (std::size_t i = 0; i < 9; ++i) s += A.data[i] * B.data[i];
    return s;
}

template <typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
struct Matrix4x4 {
    using value_type = T;
    static constexpr std::size_t size = 4;

    // [ r0c0 r0c1 r0c2 r0c3
    //   r1c0 r1c1 r1c2 r1c3
    //   r2c0 r2c1 r2c2 r2c3
    //   r3c0 r3c1 r3c2 r3c3 ]
    std::array<T, 16> data{};

    constexpr Matrix4x4() noexcept = default;

    constexpr Matrix4x4(
        T m00, T m01, T m02, T m03,
        T m10, T m11, T m12, T m13,
        T m20, T m21, T m22, T m23,
        T m30, T m31, T m32, T m33
    ) noexcept
        : data{ m00,m01,m02,m03,
                m10,m11,m12,m13,
                m20,m21,m22,m23,
                m30,m31,m32,m33 } {}

    constexpr explicit Matrix4x4(const std::array<T,16>& d) noexcept : data(d) {}

    constexpr explicit Matrix4x4(const Matrix<4,T>& m) noexcept
        : data{
            m.data[0][0], m.data[0][1], m.data[0][2], m.data[0][3],
            m.data[1][0], m.data[1][1], m.data[1][2], m.data[1][3],
            m.data[2][0], m.data[2][1], m.data[2][2], m.data[2][3],
            m.data[3][0], m.data[3][1], m.data[3][2], m.data[3][3]
        } {}

    constexpr explicit operator Matrix<4,T>() const noexcept {
        return Matrix<4,T>(std::array<std::array<T,4>,4>{{
            { data[0], data[1], data[2], data[3] },
            { data[4], data[5], data[6], data[7] },
            { data[8], data[9], data[10], data[11] },
            { data[12],data[13],data[14],data[15] }
        }});
    }

    static constexpr Matrix4x4 identity() noexcept {
        return {
            T(1),T(0),T(0),T(0),
            T(0),T(1),T(0),T(0),
            T(0),T(0),T(1),T(0),
            T(0),T(0),T(0),T(1)
        };
    }

    static constexpr Matrix4x4 filled(T v) noexcept {
        return { v,v,v,v, v,v,v,v, v,v,v,v, v,v,v,v };
    }

    static constexpr Matrix4x4 diagonal(T d0 = T(1), T d1 = T(1), T d2 = T(1), T d3 = T(1)) noexcept {
        return {
            d0, T(0),T(0),T(0),
            T(0), d1, T(0),T(0),
            T(0), T(0), d2, T(0),
            T(0), T(0), T(0), d3
        };
    }

    static constexpr Matrix4x4 diagonal(const std::array<T,4>& d) noexcept {
        return diagonal(d[0], d[1], d[2], d[3]);
    }

    static constexpr Matrix4x4 scaling(T s) noexcept {
        return diagonal(s,s,s,s);
    }

    static constexpr Matrix4x4 scaling(T sx, T sy, T sz, T sw = T(1)) noexcept {
        return diagonal(sx,sy,sz,sw);
    }

    constexpr T*       operator[](std::size_t r) noexcept       { return &data[r*4]; }
    constexpr const T* operator[](std::size_t r) const noexcept { return &data[r*4]; }

    constexpr T&       at(std::size_t r, std::size_t c) noexcept       { return data[r*4 + c]; }
    constexpr const T& at(std::size_t r, std::size_t c) const noexcept { return data[r*4 + c]; }

    constexpr std::array<T,4> row(std::size_t r) const noexcept {
        return { data[r*4], data[r*4+1], data[r*4+2], data[r*4+3] };
    }

    constexpr std::array<T,4> col(std::size_t c) const noexcept {
        return { data[c], data[4+c], data[8+c], data[12+c] };
    }

    constexpr bool operator==(const Matrix4x4& o) const noexcept { return data == o.data; }
    constexpr bool operator!=(const Matrix4x4& o) const noexcept { return data != o.data; }

    constexpr bool approx_equal(const Matrix4x4& o, T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < 16; ++i)
            if (abs_constexpr(data[i] - o.data[i]) > eps) return false;
        return true;
    }

    constexpr Matrix4x4 operator+(T s) const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = data[i] + s;
        return m;
    }

    constexpr Matrix4x4 operator-(T s) const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = data[i] - s;
        return m;
    }

    constexpr Matrix4x4 operator*(T s) const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = data[i] * s;
        return m;
    }

    constexpr Matrix4x4 operator/(T s) const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = data[i] / s;
        return m;
    }

    constexpr Matrix4x4 operator+(const Matrix4x4& o) const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = data[i] + o.data[i];
        return m;
    }

    constexpr Matrix4x4 operator-(const Matrix4x4& o) const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = data[i] - o.data[i];
        return m;
    }

    constexpr Matrix4x4 operator-() const noexcept {
        Matrix4x4 m;
        for (std::size_t i=0;i<16;++i) m.data[i] = -data[i];
        return m;
    }

    constexpr Matrix4x4 operator*(const Matrix4x4& o) const noexcept {
        Matrix4x4 r;
        for (std::size_t i=0;i<4;++i)
            for (std::size_t j=0;j<4;++j)
                r.data[i*4 + j] =
                    data[i*4 + 0] * o.data[0*4 + j] +
                    data[i*4 + 1] * o.data[1*4 + j] +
                    data[i*4 + 2] * o.data[2*4 + j] +
                    data[i*4 + 3] * o.data[3*4 + j];
        return r;
    }

    constexpr Vector4D<T> operator*(const Vector4D<T>& v) const noexcept {
        return {
            data[0]*v.x + data[1]*v.y + data[2]*v.z + data[3]*v.w,
            data[4]*v.x + data[5]*v.y + data[6]*v.z + data[7]*v.w,
            data[8]*v.x + data[9]*v.y + data[10]*v.z + data[11]*v.w,
            data[12]*v.x + data[13]*v.y + data[14]*v.z + data[15]*v.w
        };
    }

    constexpr T determinant() const noexcept {
        const auto m = [&](std::size_t r, std::size_t c){ return data[r*4+c]; };

        const T a0 = m(0,0), a1 = m(0,1), a2 = m(0,2), a3 = m(0,3);

        Matrix3x3<T> M0{ m(1,1),m(1,2),m(1,3),
                         m(2,1),m(2,2),m(2,3),
                         m(3,1),m(3,2),m(3,3) };

        Matrix3x3<T> M1{ m(1,0),m(1,2),m(1,3),
                         m(2,0),m(2,2),m(2,3),
                         m(3,0),m(3,2),m(3,3) };

        Matrix3x3<T> M2{ m(1,0),m(1,1),m(1,3),
                         m(2,0),m(2,1),m(2,3),
                         m(3,0),m(3,1),m(3,3) };

        Matrix3x3<T> M3{ m(1,0),m(1,1),m(1,2),
                         m(2,0),m(2,1),m(2,2),
                         m(3,0),m(3,1),m(3,2) };

        return a0*M0.determinant()
             - a1*M1.determinant()
             + a2*M2.determinant()
             - a3*M3.determinant();
    }

    constexpr Matrix4x4 adjugate() const noexcept {
        Matrix4x4 r;
        for (std::size_t i=0;i<4;++i)
            for (std::size_t j=0;j<4;++j) {
                Matrix3x3<T> sub;
                std::size_t idx=0;

                for (std::size_t r0=0;r0<4;++r0) {
                    if (r0==i) continue;

                    for (std::size_t c0=0;c0<4;++c0) {
                        if (c0==j) continue;
                        sub.data[idx++] = data[r0*4 + c0];
                    }
                }

                const T cof = ((i+j)%2==0 ? T(1) : T(-1)) * sub.determinant();
                r.data[j*4 + i] = cof; 
            }
        return r;
    }

    constexpr Matrix4x4 inverse() const noexcept {
        const T det = determinant();
        if (abs_constexpr(det) <= std::numeric_limits<T>::epsilon()*T(1e3))
            return identity();
        return adjugate() / det;
    }

    constexpr T trace() const noexcept {
        return data[0] + data[5] + data[10] + data[15];
    }

    constexpr T frobenius_norm() const noexcept {
        T s=T(0);
        for (auto v: data) s += v*v;
        return sqrt_constexpr(s);
    }

    constexpr bool is_zero(T eps = constants::epsilon<T>()) const noexcept {
        for (auto v: data) if (abs_constexpr(v) > eps) return false;
        return true;
    }

    constexpr bool is_identity(T eps = constants::epsilon<T>()) const noexcept {
        return approx_equal(identity(), eps);
    }

public:
    static constexpr Matrix4x4 rotation_xy(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {
            c, -s, T(0), T(0),
            s,  c, T(0), T(0),
            T(0),T(0),T(1),T(0),
            T(0),T(0),T(0),T(1)
        };
    }

    static constexpr Matrix4x4 rotation_xz(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {
            c, T(0), -s, T(0),
            T(0),T(1),T(0),T(0),
            s, T(0),  c, T(0),
            T(0),T(0),T(0),T(1)
        };
    }

    static constexpr Matrix4x4 rotation_xw(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {
            c, T(0), T(0), -s,
            T(0),T(1),T(0),T(0),
            T(0),T(0),T(1),T(0),
            s, T(0),T(0),  c
        };
    }

    static constexpr Matrix4x4 rotation_yz(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {
            T(1),T(0),T(0),T(0),
            T(0), c, -s, T(0),
            T(0), s,  c, T(0),
            T(0),T(0),T(0),T(1)
        };
    }

    static constexpr Matrix4x4 rotation_yw(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {
            T(1),T(0),T(0),T(0),
            T(0), c, T(0), -s,
            T(0),T(0),T(1),T(0),
            T(0), s, T(0),  c
        };
    }

    static constexpr Matrix4x4 rotation_zw(T angle, bool angle_in_degrees = true) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);

        return {
            T(1),T(0),T(0),T(0),
            T(0),T(1),T(0),T(0),
            T(0),T(0), c, -s,
            T(0),T(0), s,  c
        };
    }

    static constexpr Matrix4x4 rotation_plane(
        const Vector4D<T>& u,
        const Vector4D<T>& v,
        T angle,
        bool angle_in_degrees = true
    ) noexcept {
        angle = angle_in_degrees ? constants::PI_180<T> * angle : angle;
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);
        Matrix4x4 R = identity();

        for (std::size_t i=0;i<4;++i)
            for (std::size_t j=0;j<4;++j) {
                const T uu = u[i]*u[j];
                const T vv = v[i]*v[j];
                const T vu = v[i]*u[j];
                const T uv = u[i]*v[j];
                R.data[i*4+j] += (c - 1)*(uu + vv) + s*(vu - uv);
            }

        return R;
    }

    static constexpr Matrix4x4 translation(T tx, T ty, T tz, T tw = T(0)) noexcept {
        return {
            T(1),T(0),T(0),tx,
            T(0),T(1),T(0),ty,
            T(0),T(0),T(1),tz,
            T(0),T(0),T(0),T(1)
        };
    }

    static constexpr Matrix4x4 shear(std::size_t along, std::size_t by, T k) noexcept {
        Matrix4x4 m = identity();
        if (along < 4 && by < 4 && along != by)
            m.data[along*4 + by] = k;
        return m;
    }

    static constexpr Matrix4x4 outer(const std::array<T,4>& a, const std::array<T,4>& b) noexcept {
        return {
            a[0]*b[0], a[0]*b[1], a[0]*b[2], a[0]*b[3],
            a[1]*b[0], a[1]*b[1], a[1]*b[2], a[1]*b[3],
            a[2]*b[0], a[2]*b[1], a[2]*b[2], a[2]*b[3],
            a[3]*b[0], a[3]*b[1], a[3]*b[2], a[3]*b[3]
        };
    }

    static constexpr Matrix4x4 skew_symmetric(
        T a01, T a02, T a03,
        T a12, T a13,
        T a23
    ) noexcept {
        return {
            T(0), -a01, -a02, -a03,
            a01, T(0), -a12, -a13,
            a02,  a12, T(0), -a23,
            a03,  a13,  a23, T(0)
        };
    }

    static constexpr Matrix4x4 reflection(const Vector4D<T>& normal) noexcept {
        const T mag = normal.magnitude();
        if (mag == T(0)) return identity();
        const auto n = normal.unit_vector();
        const T nx = n.x / mag;
        const T ny = n.y / mag;
        const T nz = n.z / mag;
        const T nw = n.w / mag;

        return {
            T(1)-T(2)*nx*nx,   -T(2)*nx*ny,     -T(2)*nx*nz,     -T(2)*nx*nw,
            -T(2)*ny*nx,       T(1)-T(2)*ny*ny, -T(2)*ny*nz,     -T(2)*ny*nw,
            -T(2)*nz*nx,       -T(2)*nz*ny,     T(1)-T(2)*nz*nz, -T(2)*nz*nw,
            -T(2)*nw*nx,       -T(2)*nw*ny,     -T(2)*nw*nz,     T(1)-T(2)*nw*nw
        };
    }
};

template <typename T>
constexpr Matrix4x4<T> operator*(T s, const Matrix4x4<T>& m) noexcept {
    return m * s;
}

using Matrix2d = Matrix2x2<double>;
using Matrix3d = Matrix3x3<double>;
using Matrix4d = Matrix4x4<double>;

} // namespace math
} // namespace fizmo

#endif // FIZMO_SPECIALIZED_SQUARE_MATRICES_HPP