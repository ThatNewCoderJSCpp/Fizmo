#ifndef FIZMO_MATRIX_HPP
#define FIZMO_MATRIX_HPP

#include <array>
#include <type_traits>
#include <limits>
#include <utility>
#include <stdexcept>

#include "../Standard Overloads/trig.hpp"

namespace fizmo {
namespace math {

template <std::size_t N, typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
struct Matrix;

namespace detail {

template <std::size_t... Is>
struct index_sequence {};

template <std::size_t N, std::size_t... Is>
struct make_index_sequence_impl : make_index_sequence_impl<N - 1, N - 1, Is...> {};

template <std::size_t... Is>
struct make_index_sequence_impl<0, Is...> { using type = index_sequence<Is...>; };

template <std::size_t N>
using make_index_seq = typename make_index_sequence_impl<N>::type;

template <typename T, std::size_t N>
struct DeterminantHelper {
    static constexpr T compute(const std::array<std::array<T, N>, N>& m) noexcept {
        T det = T(0);
        for (std::size_t col = 0; col < N; ++col) {
            std::array<std::array<T, N - 1>, N - 1> sub{};
            for (std::size_t r = 1; r < N; ++r) {
                std::size_t sc = 0;
                for (std::size_t c = 0; c < N; ++c) {
                    if (c == col) continue;
                    sub[r - 1][sc++] = m[r][c];
                }
            }
            T cofactor = DeterminantHelper<T, N - 1>::compute(sub);
            det += ((col % 2 == 0) ? T(1) : T(-1)) * m[0][col] * cofactor;
        }
        return det;
    }
};

template <typename T>
struct DeterminantHelper<T, 1> {
    static constexpr T compute(const std::array<std::array<T, 1>, 1>& m) noexcept { return m[0][0]; }
};

template <typename T>
struct DeterminantHelper<T, 2> {
    static constexpr T compute(const std::array<std::array<T, 2>, 2>& m) noexcept { return m[0][0] * m[1][1] - m[0][1] * m[1][0]; }
};

template <typename T>
struct DeterminantHelper<T, 3> {
    static constexpr T compute(const std::array<std::array<T, 3>, 3>& m) noexcept {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }
};

} // namespace detail

template <std::size_t N, typename T, typename>
struct Matrix {
    using value_type = T;
    static constexpr std::size_t size = N;
    std::array<std::array<T, N>, N> data{};
    constexpr Matrix() noexcept = default;
    constexpr explicit Matrix(const std::array<std::array<T, N>, N>& d) noexcept : data(d) {}
    static constexpr Matrix filled(T val) noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = val;
        return m;
    }
    static constexpr Matrix identity() noexcept {
        Matrix m;
        for (std::size_t i = 0; i < N; ++i) m.data[i][i] = T(1);
        return m;
    }
    static constexpr Matrix diagonal(const std::array<T, N>& d) noexcept {
        Matrix m;
        for (std::size_t i = 0; i < N; ++i) m.data[i][i] = d[i];
        return m;
    }
    constexpr T& at(std::size_t r, std::size_t c) noexcept { return data[r][c]; }
    constexpr const T& at(std::size_t r, std::size_t c) const noexcept { return data[r][c]; }
    constexpr std::array<T, N>& operator[](std::size_t r) noexcept { return data[r]; }
    constexpr const std::array<T, N>& operator[](std::size_t r) const noexcept { return data[r]; }
    constexpr bool operator==(const Matrix& o) const noexcept {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (data[r][c] != o.data[r][c]) return false;
        return true;
    }
    constexpr bool operator!=(const Matrix& o) const noexcept { return !(*this == o); }
    constexpr bool approx_equal(const Matrix& o, T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (abs_constexpr(data[r][c] - o.data[r][c]) > eps) return false;
        return true;
    }
    constexpr Matrix operator+(T s) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] + s;
        return m;
    }
    constexpr Matrix operator-(T s) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] - s;
        return m;
    }
    constexpr Matrix operator*(T s) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] * s;
        return m;
    }
    constexpr Matrix operator/(T s) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] / s;
        return m;
    }
    constexpr Matrix& operator+=(T s) noexcept { return *this = *this + s; }
    constexpr Matrix& operator-=(T s) noexcept { return *this = *this - s; }
    constexpr Matrix& operator*=(T s) noexcept { return *this = *this * s; }
    constexpr Matrix& operator/=(T s) noexcept { return *this = *this / s; }
    constexpr Matrix operator+(const Matrix& o) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] + o.data[r][c];
        return m;
    }
    constexpr Matrix operator-(const Matrix& o) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] - o.data[r][c];
        return m;
    }
    constexpr Matrix operator-() const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = -data[r][c];
        return m;
    }
    constexpr Matrix& operator+=(const Matrix& o) noexcept { return *this = *this + o; }
    constexpr Matrix& operator-=(const Matrix& o) noexcept { return *this = *this - o; }
    constexpr Matrix operator*(const Matrix& o) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) for (std::size_t k = 0; k < N; ++k) m.data[r][c] += data[r][k] * o.data[k][c];
        return m;
    }
    constexpr Matrix& operator*=(const Matrix& o) noexcept { return *this = *this * o; }
    constexpr Matrix hadamard(const Matrix& o) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r][c] * o.data[r][c];
        return m;
    }
    constexpr std::array<T, N> operator*(const std::array<T, N>& v) const noexcept {
        std::array<T, N> res{};
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) res[r] += data[r][c] * v[c];
        return res;
    }
    constexpr T trace() const noexcept {
        T t = T(0);
        for (std::size_t i = 0; i < N; ++i) t += data[i][i];
        return t;
    }
    constexpr T determinant() const noexcept { return detail::DeterminantHelper<T, N>::compute(data); }
    constexpr Matrix transpose() const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[c][r] = data[r][c];
        return m;
    }
    constexpr T frobenius_norm() const noexcept {
        T sum = T(0);
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) sum += data[r][c] * data[r][c];
        return sqrt_constexpr(sum);
    }
    constexpr T max_abs() const noexcept {
        T m = T(0);
        for (std::size_t r = 0; r < N; ++r)
            for (std::size_t c = 0; c < N; ++c) {
                T v = abs_constexpr(data[r][c]);
                if (v > m) m = v;
            }
        return m;
    }
    constexpr bool is_symmetric() const noexcept {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = r + 1; c < N; ++c) if (data[r][c] != data[c][r]) return false;
        return true;
    }
    constexpr bool is_diagonal() const noexcept {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (r != c && data[r][c] != T(0)) return false;
        return true;
    }
    constexpr bool is_identity(T eps = constants::epsilon<T>()) const noexcept { return approx_equal(identity(), eps); }
    constexpr bool is_zero(T eps = constants::epsilon<T>()) const noexcept { return approx_equal(Matrix{}, eps); }
    constexpr std::array<T, N> row(std::size_t r) const noexcept { return data[r]; }
    constexpr std::array<T, N> col(std::size_t c) const noexcept {
        std::array<T, N> v{};
        for (std::size_t r = 0; r < N; ++r) v[r] = data[r][c];
        return v;
    }
    constexpr Matrix set_row(std::size_t r, const std::array<T, N>& v) const noexcept {
        Matrix m = *this;
        m.data[r] = v;
        return m;
    }
    constexpr Matrix set_col(std::size_t c, const std::array<T, N>& v) const noexcept {
        Matrix m = *this;
        for (std::size_t r = 0; r < N; ++r) m.data[r][c] = v[r];
        return m;
    }
    constexpr Matrix swap_rows(std::size_t r1, std::size_t r2) const noexcept {
        Matrix m = *this;
        auto tmp = m.data[r1];
        m.data[r1] = m.data[r2];
        m.data[r2] = tmp;
        return m;
    }
    constexpr Matrix swap_cols(std::size_t c1, std::size_t c2) const noexcept {
        Matrix m = *this;
        for (std::size_t r = 0; r < N; ++r) {
            T tmp = m.data[r][c1];
            m.data[r][c1] = m.data[r][c2];
            m.data[r][c2] = tmp;
        }
        return m;
    }
    constexpr Matrix scale_row(std::size_t r, T s) const noexcept {
        Matrix m = *this;
        for (std::size_t c = 0; c < N; ++c) m.data[r][c] *= s;
        return m;
    }
    constexpr Matrix scale_col(std::size_t c, T s) const noexcept {
        Matrix m = *this;
        for (std::size_t r = 0; r < N; ++r) m.data[r][c] *= s;
        return m;
    }
    constexpr Matrix add_scaled_row(std::size_t dst, std::size_t src, T s) const noexcept {
        Matrix m = *this;
        for (std::size_t c = 0; c < N; ++c) m.data[dst][c] += s * m.data[src][c];
        return m;
    }
    constexpr Matrix<(N > 1 ? N - 1 : 1), T> submatrix(std::size_t dr, std::size_t dc) const noexcept {
        Matrix<(N > 1 ? N - 1 : 1), T> m;
        std::size_t mr = 0;
        for (std::size_t r = 0; r < N; ++r) {
            if (r == dr) continue;
            std::size_t mc = 0;
            for (std::size_t c = 0; c < N; ++c) {
                if (c == dc) continue;
                m.data[mr][mc++] = data[r][c];
            }
            ++mr;
        }
        return m;
    }
    constexpr Matrix cofactor_matrix() const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r)
            for (std::size_t c = 0; c < N; ++c) {
                T minor_det = submatrix(r, c).determinant();
                m.data[r][c] = ((r + c) % 2 == 0 ? T(1) : T(-1)) * minor_det;
            }
        return m;
    }
    constexpr Matrix adjugate() const noexcept { return cofactor_matrix().transpose(); }
    constexpr Matrix inverse() const noexcept {
        T det = determinant();
        if (abs_constexpr(det) <= std::numeric_limits<T>::epsilon() * T(1e3)) return identity(); 
        return adjugate() / det;
    }
    constexpr Matrix pinverse() const noexcept { return inverse(); }
    struct LUResult { Matrix L, U, P; int sign; }; 
    LUResult lu() const noexcept {
        Matrix L = identity();
        Matrix U = *this;
        Matrix P = identity();
        int sign = 1;
        for (std::size_t col = 0; col < N; ++col) {
            std::size_t pivot = col;
            T best = abs_constexpr(U.data[col][col]);
            for (std::size_t r = col + 1; r < N; ++r) {
                T v = abs_constexpr(U.data[r][col]);
                if (v > best) { best = v; pivot = r; }
            }
            if (pivot != col) {
                U = U.swap_rows(col, pivot);
                P = P.swap_rows(col, pivot);
                for (std::size_t k = 0; k < col; ++k) {
                    T tmp = L.data[col][k];
                    L.data[col][k] = L.data[pivot][k];
                    L.data[pivot][k] = tmp;
                }
                sign = -sign;
            }
            if (abs_constexpr(U.data[col][col]) < std::numeric_limits<T>::epsilon()) continue;
            for (std::size_t r = col + 1; r < N; ++r) {
                T factor = U.data[r][col] / U.data[col][col];
                L.data[r][col] = factor;
                for (std::size_t c = col; c < N; ++c) U.data[r][c] -= factor * U.data[col][c];
            }
        }
        return { L, U, P, sign };
    }
    struct QRResult { Matrix Q, R; };
    QRResult qr() const noexcept {
        Matrix Q;
        Matrix R;
        std::array<std::array<T, N>, N> basis{};
        for (std::size_t j = 0; j < N; ++j) {
            for (std::size_t i = 0; i < N; ++i) basis[j][i] = data[i][j];
            for (std::size_t k = 0; k < j; ++k) {
                T dot = T(0);
                for (std::size_t i = 0; i < N; ++i) dot += basis[k][i] * basis[j][i];
                R.data[k][j] = dot;
                for (std::size_t i = 0; i < N; ++i) basis[j][i] -= dot * basis[k][i];
            }
            T norm = T(0);
            for (std::size_t i = 0; i < N; ++i) norm += basis[j][i] * basis[j][i];
            norm = sqrt_constexpr(norm);
            R.data[j][j] = norm;
            if (norm > std::numeric_limits<T>::epsilon()) for (std::size_t i = 0; i < N; ++i) basis[j][i] /= norm;
            for (std::size_t i = 0; i < N; ++i) Q.data[i][j] = basis[j][i];
        }
        return { Q, R };
    }
    constexpr Matrix pow(int exp) const noexcept {
        if (exp == 0) return identity();
        if (exp < 0) return inverse().pow(-exp);
        Matrix result = identity();
        Matrix base = *this;
        int e = exp;
        while (e > 0) {
            if (e & 1) result = result * base;
            base = base * base;
            e >>= 1;
        }
        return result;
    }
    constexpr Matrix apply(T (*fn)(T)) const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = fn(data[r][c]);
        return m;
    }
    constexpr Matrix abs_matrix() const noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = abs_constexpr(data[r][c]);
        return m;
    }
    constexpr T sum() const noexcept {
        T s = T(0);
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) s += data[r][c];
        return s;
    }
    constexpr T product() const noexcept {
        T p = T(1);
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) p *= data[r][c];
        return p;
    }
    constexpr std::array<T, N> col_sums() const noexcept {
        std::array<T, N> s{};
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) s[c] += data[r][c];
        return s;
    }
    constexpr std::array<T, N> row_sums() const noexcept {
        std::array<T, N> s{};
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) s[r] += data[r][c];
        return s;
    }
    T log_det() const noexcept {
        auto [L, U, P, sgn] = lu();
        T ld = T(0);
        for (std::size_t i = 0; i < N; ++i) ld += log_constexpr(abs_constexpr(U.data[i][i]));
        return ld;
    }
    static std::array<T, N> back_substitute(const Matrix& U, const std::array<T, N>& b) noexcept {
        std::array<T, N> x{};
        for (int r = static_cast<int>(N) - 1; r >= 0; --r) {
            T sum = b[r];
            for (std::size_t c = r + 1; c < N; ++c) sum -= U.data[r][c] * x[c];
            x[r] = (abs_constexpr(U.data[r][r]) > std::numeric_limits<T>::epsilon()) ? sum / U.data[r][r] : T(0);
        }
        return x;
    }
    static std::array<T, N> forward_substitute(const Matrix& L, const std::array<T, N>& b) noexcept {
        std::array<T, N> x{};
        for (std::size_t r = 0; r < N; ++r) {
            T sum = b[r];
            for (std::size_t c = 0; c < r; ++c) sum -= L.data[r][c] * x[c];
            x[r] = sum; 
        }
        return x;
    }
    std::array<T, N> solve(const std::array<T, N>& b) const noexcept {
        auto res = lu();
        std::array<T, N> pb = res.P * b;
        std::array<T, N> y  = forward_substitute(res.L, pb);
        return back_substitute(res.U, y);
    }
    std::size_t rank() const noexcept {
        Matrix m = *this;
        std::size_t r = 0;
        for (std::size_t col = 0; col < N && r < N; ++col) {
            std::size_t pivot = N;
            for (std::size_t i = r; i < N; ++i) if (abs_constexpr(m.data[i][col]) > std::numeric_limits<T>::epsilon() * T(1e3)) { pivot = i; break; }
            if (pivot == N) continue;
            m = m.swap_rows(r, pivot);
            T inv = T(1) / m.data[r][col];
            for (std::size_t i = 0; i < N; ++i) {
                if (i == r) continue;
                T factor = m.data[i][col] * inv;
                for (std::size_t c = 0; c < N; ++c) m.data[i][c] -= factor * m.data[r][c];
            }
            ++r;
        }
        return r;
    }
    static constexpr Matrix translation_2d(T tx, T ty) noexcept {
        static_assert(N >= 3, "translation_2d requires N >= 3 (homogeneous)");
        Matrix m = identity();
        m.data[0][N - 1] = tx;
        m.data[1][N - 1] = ty;
        return m;
    }
    static constexpr Matrix translation_3d(T tx, T ty, T tz) noexcept {
        static_assert(N >= 4, "translation_3d requires N >= 4 (homogeneous)");
        Matrix m = identity();
        m.data[0][N - 1] = tx;
        m.data[1][N - 1] = ty;
        m.data[2][N - 1] = tz;
        return m;
    }
    static constexpr Matrix rotation_2d(T angle) noexcept {
        static_assert(N >= 2, "rotation_2d requires N >= 2");
        Matrix m = identity();
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);
        m.data[0][0] =  c;  m.data[0][1] = -s;
        m.data[1][0] =  s;  m.data[1][1] =  c;
        return m;
    }
    static constexpr Matrix rotation_x(T angle) noexcept {
        static_assert(N >= 3, "rotation_x requires N >= 3");
        Matrix m = identity();
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);
        m.data[1][1] =  c;  m.data[1][2] = -s;
        m.data[2][1] =  s;  m.data[2][2] =  c;
        return m;
    }
    static constexpr Matrix rotation_y(T angle) noexcept {
        static_assert(N >= 3, "rotation_y requires N >= 3");
        Matrix m = identity();
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);
        m.data[0][0] =  c;  m.data[0][2] =  s;
        m.data[2][0] = -s;  m.data[2][2] =  c;
        return m;
    }
    static constexpr Matrix rotation_z(T angle) noexcept {
        static_assert(N >= 3, "rotation_z requires N >= 3");
        Matrix m = identity();
        const T c = cos_constexpr(angle);
        const T s = sin_constexpr(angle);
        m.data[0][0] =  c;  m.data[0][1] = -s;
        m.data[1][0] =  s;  m.data[1][1] =  c;
        return m;
    }
    static constexpr Matrix rotation_axis(T ax, T ay, T az, T angle) noexcept {
        static_assert(N >= 3, "rotation_axis requires N >= 3");
        T len = sqrt_constexpr(ax * ax + ay * ay + az * az);
        if (abs_constexpr(len) < std::numeric_limits<T>::epsilon()) return identity();
        ax /= len; ay /= len; az /= len;
        const T c  = cos_constexpr(angle);
        const T s  = sin_constexpr(angle);
        const T ic = T(1) - c;
        Matrix m = identity();
        m.data[0][0] = c  + ax * ax * ic;
        m.data[0][1] = ax * ay * ic - az * s;
        m.data[0][2] = ax * az * ic + ay * s;
        m.data[1][0] = ay * ax * ic + az * s;
        m.data[1][1] = c  + ay * ay * ic;
        m.data[1][2] = ay * az * ic - ax * s;
        m.data[2][0] = az * ax * ic - ay * s;
        m.data[2][1] = az * ay * ic + ax * s;
        m.data[2][2] = c  + az * az * ic;
        return m;
    }
    static constexpr Matrix scaling(T s) noexcept {
        Matrix m = identity();
        for (std::size_t i = 0; i < N; ++i) m.data[i][i] = s;
        return m;
    }
    static constexpr Matrix scaling(const std::array<T, N>& s) noexcept { return diagonal(s); }
    static constexpr Matrix shear(std::size_t along, std::size_t by, T k) noexcept {
        Matrix m = identity();
        if (along != by && along < N && by < N) m.data[along][by] = k;
        return m;
    }
    static constexpr Matrix reflection(const std::array<T, N>& n) noexcept {
        Matrix m = identity();
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] -= T(2) * n[r] * n[c];
        return m;
    }
    static constexpr Matrix projection(const std::array<T, N>& n) noexcept {
        Matrix m = identity();
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] -= n[r] * n[c];
        return m;
    }
    static constexpr Matrix outer(const std::array<T, N>& a, const std::array<T, N>& b) noexcept {
        Matrix m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = a[r] * b[c];
        return m;
    }
    static constexpr Matrix perspective(T fov_y, T aspect, T fov_near, T fov_far) noexcept {
        static_assert(N == 4, "perspective requires N == 4");
        Matrix m;
        const T f = T(1) / sin_constexpr(fov_y * T(0.5)) * cos_constexpr(fov_y * T(0.5));
        m.data[0][0] = f / aspect;
        m.data[1][1] = f;
        m.data[2][2] = (fov_far + fov_near) / (fov_near - fov_far);
        m.data[2][3] = (T(2) * fov_far * fov_near) / (fov_near - fov_far);
        m.data[3][2] = T(-1);
        return m;
    }
    static constexpr Matrix ortho(T l, T r, T b, T t, T fov_near, T fov_far) noexcept {
        static_assert(N == 4, "ortho requires N == 4");
        Matrix m = identity();
        m.data[0][0] =  T(2) / (r - l);
        m.data[1][1] =  T(2) / (t - b);
        m.data[2][2] = -T(2) / (fov_far - fov_near);
        m.data[0][3] = -(r + l) / (r - l);
        m.data[1][3] = -(t + b) / (t - b);
        m.data[2][3] = -(fov_far + fov_near) / (fov_far - fov_near);
        return m;
    }
    static Matrix look_at(
        T ex, T ey, T ez,  
        T cx, T cy, T cz,   
        T ux, T uy, T uz    
    ) noexcept {
        static_assert(N == 4, "look_at requires N == 4");
        T fx = cx - ex, fy = cy - ey, fz = cz - ez;
        T fl = sqrt_constexpr(fx*fx + fy*fy + fz*fz);
        fx /= fl; fy /= fl; fz /= fl;
        T rx = fy*uz - fz*uy, ry = fz*ux - fx*uz, rz = fx*uy - fy*ux;
        T rl = sqrt_constexpr(rx*rx + ry*ry + rz*rz);
        rx /= rl; ry /= rl; rz /= rl;
        T vx = ry*fz - rz*fy, vy = rz*fx - rx*fz, vz = rx*fy - ry*fx;
        Matrix m = identity();
        m.data[0][0] = rx; m.data[0][1] = ry; m.data[0][2] = rz;
        m.data[1][0] = vx; m.data[1][1] = vy; m.data[1][2] = vz;
        m.data[2][0] =-fx; m.data[2][1] =-fy; m.data[2][2] =-fz;
        m.data[0][3] = -(rx*ex + ry*ey + rz*ez);
        m.data[1][3] = -(vx*ex + vy*ey + vz*ez);
        m.data[2][3] =   fx*ex + fy*ey + fz*ez;
        return m;
    }

}; // struct Matrix

template <typename T, std::size_t N>
constexpr Matrix<N, T> operator*(T s, const Matrix<N, T>& m) noexcept { return m * s; }

template <typename T, std::size_t N>
constexpr Matrix<N, T> commutator(const Matrix<N, T>& A, const Matrix<N, T>& B) noexcept { return A * B - B * A; }

template <typename T, std::size_t N, std::size_t M>
constexpr Matrix<N * M, T> kronecker(const Matrix<N, T>& A, const Matrix<M, T>& B) noexcept {
    Matrix<N * M, T> K;
    for (std::size_t ar = 0; ar < N; ++ar)
        for (std::size_t ac = 0; ac < N; ++ac)
            for (std::size_t br = 0; br < M; ++br)
                for (std::size_t bc = 0; bc < M; ++bc)
                    K.data[ar * M + br][ac * M + bc] = A.data[ar][ac] * B.data[br][bc];
    return K;
}

template <typename T, std::size_t N>
constexpr T inner_product(const Matrix<N, T>& A, const Matrix<N, T>& B) noexcept {
    T s = T(0);
    for (std::size_t r = 0; r < N; ++r)
        for (std::size_t c = 0; c < N; ++c)
            s += A.data[r][c] * B.data[r][c];
    return s;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATRIX_HPP