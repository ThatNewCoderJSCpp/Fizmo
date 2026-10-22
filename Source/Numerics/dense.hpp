#ifndef FIZMO_NUMERICS_DENSE_HPP
#define FIZMO_NUMERICS_DENSE_HPP

#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fizmo {
namespace numerics {

template <class T>
class DenseMatrix {
public:
    DenseMatrix() = default;
    DenseMatrix(std::size_t rows, std::size_t cols, const T& fill = T(0)) : m_rows(rows), m_cols(cols), m_data(rows * cols, fill) {}

    static DenseMatrix identity(std::size_t n) {
        DenseMatrix m(n, n);
        for (std::size_t i = 0; i < n; ++i) m(i, i) = T(1);
        return m;
    }

    std::size_t rows() const { return m_rows; }
    std::size_t cols() const { return m_cols; }
    T& operator()(std::size_t r, std::size_t c) { return m_data[r * m_cols + c]; }
    const T& operator()(std::size_t r, std::size_t c) const { return m_data[r * m_cols + c]; }

    std::vector<T> operator*(const std::vector<T>& x) const {
        std::vector<T> y(m_rows, T(0));
        for (std::size_t r = 0; r < m_rows; ++r) {
            T s = T(0);
            for (std::size_t c = 0; c < m_cols; ++c) s += (*this)(r, c) * x[c];
            y[r] = s;
        }
        return y;
    }

private:
    std::size_t m_rows = 0;
    std::size_t m_cols = 0;
    std::vector<T> m_data;
};

template <class T>
class DenseLU {
public:
    bool factor(const DenseMatrix<T>& a) {
        m_ready = false;
        m_n = a.rows();
        if (a.cols() != m_n) return false;
        m_lu = a;
        m_piv.resize(m_n);
        for (std::size_t k = 0; k < m_n; ++k) {
            std::size_t p = k;
            T best = magnitude(m_lu(k, k));
            for (std::size_t i = k + 1; i < m_n; ++i) {
                const T v = magnitude(m_lu(i, k));
                if (v > best) {
                    best = v;
                    p = i;
                }
            }
            if (!(best > T(0))) return false;
            m_piv[k] = p;
            if (p != k) {
                for (std::size_t j = 0; j < m_n; ++j) std::swap(m_lu(k, j), m_lu(p, j));
            }
            for (std::size_t i = k + 1; i < m_n; ++i) {
                m_lu(i, k) /= m_lu(k, k);
                const T f = m_lu(i, k);
                if (f == T(0)) continue;
                for (std::size_t j = k + 1; j < m_n; ++j) m_lu(i, j) -= f * m_lu(k, j);
            }
        }
        m_ready = true;
        return true;
    }

    void solve(std::vector<T>& b) const {
        for (std::size_t k = 0; k < m_n; ++k) {
            if (m_piv[k] != k) std::swap(b[k], b[m_piv[k]]);
        }
        for (std::size_t i = 0; i < m_n; ++i) {
            T s = b[i];
            for (std::size_t j = 0; j < i; ++j) s -= m_lu(i, j) * b[j];
            b[i] = s;
        }
        for (std::size_t i = m_n; i-- > 0;) {
            T s = b[i];
            for (std::size_t j = i + 1; j < m_n; ++j) s -= m_lu(i, j) * b[j];
            b[i] = s / m_lu(i, i);
        }
    }

    T determinant() const {
        T d = T(1);
        for (std::size_t i = 0; i < m_n; ++i) {
            d *= m_lu(i, i);
            if (m_piv[i] != i) d = -d;
        }
        return d;
    }

    bool ready() const { return m_ready; }

private:
    static T magnitude(const T& v) { return v < T(0) ? -v : v; }

    std::size_t m_n = 0;
    DenseMatrix<T> m_lu;
    std::vector<std::size_t> m_piv;
    bool m_ready = false;
};

template <class T>
bool solve_dense(DenseMatrix<T> a, std::vector<T>& b) {
    DenseLU<T> lu;
    if (!lu.factor(a)) return false;
    lu.solve(b);
    return true;
}

template <class T>
using VectorFunction = std::function<void(const std::vector<T>& x, std::vector<T>& out)>;

template <class T>
using JacobianFunction = std::function<void(const std::vector<T>& x, DenseMatrix<T>& out)>;

template <class T>
void numeric_jacobian(const VectorFunction<T>& f, const std::vector<T>& x, const std::vector<T>& fx, DenseMatrix<T>& out, const T& relative_step = T(1e-7)) {
    const std::size_t n = x.size();
    const std::size_t m = fx.size();
    out = DenseMatrix<T>(m, n);
    std::vector<T> xp = x;
    std::vector<T> fp;
    for (std::size_t j = 0; j < n; ++j) {
        const T scale = x[j] < T(0) ? -x[j] : x[j];
        const T h = relative_step * (T(1) + scale);
        xp[j] = x[j] + h;
        f(xp, fp);
        for (std::size_t i = 0; i < m; ++i) out(i, j) = (fp[i] - fx[i]) / h;
        xp[j] = x[j];
    }
}

template <class T>
struct NewtonResult {
    bool converged = false;
    int iterations = 0;
    T residual = T(0);
};

template <class T>
NewtonResult<T> newton_dense(const VectorFunction<T>& f, std::vector<T>& x, const JacobianFunction<T>& jacobian = nullptr, const T& tolerance = T(1e-12), int max_iterations = 50) {
    NewtonResult<T> out;
    std::vector<T> fx;
    DenseMatrix<T> J;
    auto norm = [](const std::vector<T>& v) {
        T best = T(0);
        for (const T& a : v) {
            const T m = a < T(0) ? -a : a;
            if (m > best) best = m;
        }
        return best;
    };
    f(x, fx);
    T rn = norm(fx);
    for (int it = 0; it < max_iterations; ++it) {
        out.iterations = it;
        out.residual = rn;
        if (rn <= tolerance) {
            out.converged = true;
            return out;
        }
        if (jacobian) jacobian(x, J);
        else numeric_jacobian(f, x, fx, J);
        std::vector<T> step = fx;
        for (T& s : step) s = -s;
        if (!solve_dense(J, step)) return out;
        T lambda = T(1);
        std::vector<T> trial(x.size());
        std::vector<T> ft;
        bool accepted = false;
        for (int tries = 0; tries < 12; ++tries) {
            for (std::size_t i = 0; i < x.size(); ++i) trial[i] = x[i] + lambda * step[i];
            f(trial, ft);
            const T tn = norm(ft);
            if (tn < rn || tries == 11) {
                x = trial;
                fx = ft;
                rn = tn;
                accepted = true;
                break;
            }
            lambda = lambda / T(2);
        }
        if (!accepted) return out;
        if (norm(step) * lambda <= tolerance * (T(1) + norm(x))) {
            out.iterations = it + 1;
            out.residual = rn;
            out.converged = true;
            return out;
        }
    }
    out.iterations = max_iterations;
    out.residual = rn;
    out.converged = rn <= tolerance;
    return out;
}

} // namespace numerics
} // namespace fizmo

#endif // FIZMO_NUMERICS_DENSE_HPP
