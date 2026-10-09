#ifndef FIZMO_SYMBOLIC_TENSORS_HPP
#define FIZMO_SYMBOLIC_TENSORS_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <numeric>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../Common/main_convenience.hpp"
#include "vectors.hpp"
#include "matrices.hpp"

namespace fizmo {
namespace math {
namespace tensors {

class SymbolicTensor {
public:
    SymbolicTensor() = default;
    explicit SymbolicTensor(std::vector<std::size_t> shape) : shape_(std::move(shape)), data_(total_size(shape_)) {}
    
    SymbolicTensor(std::vector<std::size_t> shape, std::vector<cas::Expression> data);

    static SymbolicTensor zero(const std::vector<std::size_t>& shape);

    static SymbolicTensor constant(const std::vector<std::size_t>& shape, double val);

    static SymbolicTensor kronecker(const std::vector<std::size_t>& shape);

    static SymbolicTensor outer(const SymbolicTensor& a, const SymbolicTensor& b);

    std::size_t                     rank()  const noexcept { return shape_.size(); }
    std::size_t                     size()  const noexcept { return data_.size();  }
    const std::vector<std::size_t>& shape() const noexcept { return shape_; }
    std::vector<std::size_t>&       shape() noexcept { return shape_; }
    std::size_t                     dim(std::size_t axis) const { return shape_.at(axis); }
    const std::vector<cas::Expression>&  data()  const noexcept { return data_;  }
    std::vector<cas::Expression>&        data()        noexcept { return data_;  }
    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return data_[flat(idx)]; }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return data_[flat(idx)]; }
    cas::Expression&       flat_at(std::size_t i)       { return data_[i]; }
    const cas::Expression& flat_at(std::size_t i) const { return data_[i]; }

    SymbolicTensor operator+(const SymbolicTensor& o) const;

    SymbolicTensor operator-(const SymbolicTensor& o) const;

    SymbolicTensor operator-() const {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = -data_[i];
        return r;
    }

    SymbolicTensor operator*(const cas::Expression& s) const {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }

    SymbolicTensor operator*(double s) const {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }

    SymbolicTensor operator/(const cas::Expression& s) const {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }

    SymbolicTensor operator/(double s) const {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }

    SymbolicTensor& operator+=(const SymbolicTensor& o) { return *this = *this + o; }
    SymbolicTensor& operator-=(const SymbolicTensor& o) { return *this = *this - o; }
    SymbolicTensor& operator*=(const cas::Expression& s)     { return *this = *this * s; }
    SymbolicTensor& operator*=(double s)                { return *this = *this * s; }
    SymbolicTensor& operator/=(const cas::Expression& s)     { return *this = *this / s; }
    SymbolicTensor& operator/=(double s)                { return *this = *this / s; }

    SymbolicTensor hadamard(const SymbolicTensor& o) const;

    SymbolicTensor contract(std::size_t axis_a, std::size_t axis_b) const;

    static SymbolicTensor contract(const SymbolicTensor& a, std::size_t a_axis, const SymbolicTensor& b, std::size_t b_axis);
    SymbolicTensor transpose(const std::vector<std::size_t>& perm) const;

    SymbolicTensor swap_axes(std::size_t a, std::size_t b) const;

    SymbolicTensor slice(std::size_t axis, std::size_t index) const;
    SymbolicTensor reshape(const std::vector<std::size_t>& new_shape) const;

    cas::Expression frobenius_norm_squared() const;

    cas::Expression frobenius_norm() const { return SQRT(frobenius_norm_squared()); }

    SymbolicTensor differentiate(const std::string& var) const;

    SymbolicTensor differentiate_iterative(const std::string& var, unsigned n) const;

    SymbolicTensor differentiate_recursive(const std::string& var, unsigned n) const;

    SymbolicTensor substitute(const std::string& var, double val) const;

    SymbolicTensor substitute(const std::string& var, const cas::Expression& repl) const;

    SymbolicTensor substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;

    SymbolicTensor partial_evaluate(const std::string& var, double val) const;

    SymbolicTensor partial_evaluate(const std::string& var, const cas::Expression& repl) const;

    SymbolicTensor partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;

    SymbolicTensor simplify() const;

    SymbolicTensor full_simplify(const cas::RewriterConfig& cfg = {}) const;

    SymbolicTensor rewrite(const cas::RewriterConfig& cfg = {}) const;

    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const;

    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;

    static SymbolicTensor from(const vectors::SymbolicVector2& v) { return { {2}, { v.x, v.y } }; }
    static SymbolicTensor from(const vectors::SymbolicVector3& v) { return { {3}, { v.x, v.y, v.z } }; }
    static SymbolicTensor from(const vectors::SymbolicVector4& v) { return { {4}, { v.x, v.y, v.z, v.w } }; }

    static SymbolicTensor from(const vectors::SymbolicVectorN& v) {
        std::vector<cas::Expression> d(v.components());
        return { { v.dimension() }, std::move(d) };
    }

    static SymbolicTensor from(const matrices::SymbolicMatrix2x2& m) {
        return {
            {2, 2},
            {
                m.at(0,0), m.at(0,1),
                m.at(1,0), m.at(1,1)
            }
        };
    }

    static SymbolicTensor from(const matrices::SymbolicMatrix3x3& m);

    static SymbolicTensor from(const matrices::SymbolicMatrix4x4& m);

    static SymbolicTensor from(const matrices::SymbolicMatrixNM& m);

    static SymbolicTensor from(const matrices::SymbolicMatrixN& m);

    matrices::SymbolicMatrixNM to_matrix() const;

    vectors::SymbolicVectorN to_vector() const;

    cas::Expression to_value() const;

    friend std::ostream& operator<<(std::ostream& os, const SymbolicTensor& t) {
        os << "Tensor(";
        for (std::size_t i = 0; i < t.shape_.size(); ++i) {
            os << t.shape_[i];
            if (i + 1 < t.shape_.size()) os << " x ";
        }
        os << ")";
        if (t.rank() <= 3) {
            os << " {\n";
            t.print_recursive(os, {}, 0, 1);
            os << "}";
        }
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

    static std::size_t total_size(const std::vector<std::size_t>& shape) {
        if (shape.empty()) return 0;
        std::size_t s = 1;
        for (auto d : shape) s *= d;
        return s;
    }

    static std::vector<std::size_t> compute_strides(const std::vector<std::size_t>& shape);

private:
    std::size_t flat(const std::vector<std::size_t>& idx) const;

    void unflatten(std::size_t fi, std::vector<std::size_t>& idx) const {
        idx.resize(rank());

        for (std::size_t i = rank(); i-- > 0;) {
            idx[i] = fi % shape_[i];
            fi /= shape_[i];
        }
    }

    void assert_same_shape(const SymbolicTensor& o) const;

    void print_recursive(std::ostream& os, std::vector<std::size_t> prefix, std::size_t depth, std::size_t indent) const;

private:
    std::vector<std::size_t> shape_;
    std::vector<cas::Expression>  data_;
};

inline SymbolicTensor operator*(const cas::Expression& s, const SymbolicTensor& t) { return t * s; }
inline SymbolicTensor operator*(double s,            const SymbolicTensor& t) { return t * s; }

class SymbolicSquareTensor {
public:
    SymbolicSquareTensor() = default;
    SymbolicSquareTensor(std::size_t dim, std::size_t rank) : dim_(dim), rank_(rank), inner_(make_shape(dim, rank)) {}
    SymbolicSquareTensor(std::size_t dim, std::size_t rank, std::vector<cas::Expression> data) : dim_(dim), rank_(rank), inner_(make_shape(dim, rank), std::move(data)) {}
    static SymbolicSquareTensor zero(std::size_t dim, std::size_t rank) { return { dim, rank, std::vector<cas::Expression>(pow_size(dim, rank), cas::Const(0.0)) }; }
    static SymbolicSquareTensor constant(std::size_t dim, std::size_t rank, double val) { return { dim, rank, std::vector<cas::Expression>(pow_size(dim, rank), cas::Const(val)) }; }
    
    static SymbolicSquareTensor kronecker(std::size_t dim, std::size_t rank);

    static SymbolicSquareTensor levi_civita(std::size_t n);

    static SymbolicSquareTensor outer(const SymbolicSquareTensor& a, const SymbolicSquareTensor& b);

    std::size_t dim()   const noexcept { return dim_;  }
    std::size_t rank()  const noexcept { return rank_; }
    std::size_t size()  const noexcept { return inner_.size(); }
    const std::vector<std::size_t>& shape() const noexcept { return inner_.shape(); }
    std::vector<std::size_t>&       shape()       noexcept { return inner_.shape(); }
    const std::vector<cas::Expression>&  data()  const noexcept { return inner_.data();  }
    std::vector<cas::Expression>&        data()        noexcept { return inner_.data();  }
    const SymbolicTensor&           tensor() const noexcept { return inner_; }
    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return inner_.at(idx); }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return inner_.at(idx); }
    cas::Expression&       flat_at(std::size_t i)       { return inner_.flat_at(i); }
    const cas::Expression& flat_at(std::size_t i) const { return inner_.flat_at(i); }
    SymbolicSquareTensor operator+(const SymbolicSquareTensor& o) const { assert_same(o); return wrap(inner_ + o.inner_); }
    SymbolicSquareTensor operator-(const SymbolicSquareTensor& o) const { assert_same(o); return wrap(inner_ - o.inner_); }
    SymbolicSquareTensor operator-()                              const { return wrap(-inner_); }
    SymbolicSquareTensor operator*(const cas::Expression& s)           const { return wrap(inner_ * s); }
    SymbolicSquareTensor operator*(double s)                      const { return wrap(inner_ * s); }
    SymbolicSquareTensor operator/(const cas::Expression& s)           const { return wrap(inner_ / s); }
    SymbolicSquareTensor operator/(double s)                      const { return wrap(inner_ / s); }
    SymbolicSquareTensor& operator+=(const SymbolicSquareTensor& o) { return *this = *this + o; }
    SymbolicSquareTensor& operator-=(const SymbolicSquareTensor& o) { return *this = *this - o; }
    SymbolicSquareTensor& operator*=(const cas::Expression& s)           { return *this = *this * s; }
    SymbolicSquareTensor& operator*=(double s)                      { return *this = *this * s; }
    SymbolicSquareTensor& operator/=(const cas::Expression& s)           { return *this = *this / s; }
    SymbolicSquareTensor& operator/=(double s)                      { return *this = *this / s; }
    SymbolicSquareTensor hadamard(const SymbolicSquareTensor& o) const { assert_same(o); return wrap(inner_.hadamard(o.inner_)); }

    SymbolicSquareTensor contract(std::size_t axis_a, std::size_t axis_b) const;

    SymbolicSquareTensor trace() const;

    SymbolicSquareTensor transpose(const std::vector<std::size_t>& perm) const { return wrap(inner_.transpose(perm)); }
    SymbolicSquareTensor swap_axes(std::size_t a, std::size_t b)         const { return wrap(inner_.swap_axes(a, b)); }

    SymbolicSquareTensor symmetrise() const;

    SymbolicSquareTensor antisymmetrise() const;

    SymbolicSquareTensor slice(std::size_t axis, std::size_t index) const;

    cas::Expression frobenius_norm_squared() const { return inner_.frobenius_norm_squared(); }
    cas::Expression frobenius_norm()         const { return inner_.frobenius_norm(); }
    SymbolicSquareTensor differentiate(const std::string& var)                       const { return wrap(inner_.differentiate(var)); }
    SymbolicSquareTensor differentiate_iterative(const std::string& var, unsigned n) const { return wrap(inner_.differentiate_iterative(var, n)); }
    SymbolicSquareTensor differentiate_recursive(const std::string& var, unsigned n) const { return wrap(inner_.differentiate_recursive(var, n)); }
    SymbolicSquareTensor substitute(const std::string& var, double val)          const { return wrap(inner_.substitute(var, val)); }
    SymbolicSquareTensor substitute(const std::string& var, const cas::Expression& r) const { return wrap(inner_.substitute(var, r)); }
    SymbolicSquareTensor substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { return wrap(inner_.substitute(pairs)); }
    SymbolicSquareTensor partial_evaluate(const std::string& var, double val)          const { return wrap(inner_.partial_evaluate(var, val)); }
    SymbolicSquareTensor partial_evaluate(const std::string& var, const cas::Expression& r) const { return wrap(inner_.partial_evaluate(var, r)); }
    SymbolicSquareTensor partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const { return wrap(inner_.partial_evaluate(pairs)); }
    SymbolicSquareTensor simplify()                                                              const { return wrap(inner_.simplify()); }
    SymbolicSquareTensor full_simplify(const cas::RewriterConfig& cfg = {}) const { return wrap(inner_.full_simplify(cfg)); }
    SymbolicSquareTensor rewrite(const cas::RewriterConfig& cfg = {})       const { return wrap(inner_.rewrite(cfg)); }
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const { return inner_.evaluate(vals); }
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return inner_.evaluate(vals); }
    SymbolicTensor to_tensor() const { return inner_; }

    static SymbolicSquareTensor from(const vectors::SymbolicVector2& v);

    static SymbolicSquareTensor from(const vectors::SymbolicVector3& v);

    static SymbolicSquareTensor from(const vectors::SymbolicVector4& v);

    static SymbolicSquareTensor from(const matrices::SymbolicMatrix2x2& m);

    static SymbolicSquareTensor from(const matrices::SymbolicMatrix3x3& m);

    static SymbolicSquareTensor from(const matrices::SymbolicMatrix4x4& m);

    static SymbolicSquareTensor from(const matrices::SymbolicMatrixN& m) { return { m.dimension(), 2, SymbolicTensor::from(m).data() }; }
    static SymbolicSquareTensor from(const vectors::SymbolicVectorN& v) { return { v.dimension(), 1, std::vector<cas::Expression>(v.components()) }; }

    matrices::SymbolicMatrixN to_matrix() const;

    vectors::SymbolicVectorN to_vector() const;

    friend std::ostream& operator<<(std::ostream& os, const SymbolicSquareTensor& t) {
        os << "SquareTensor(dim=" << t.dim_ << ", rank=" << t.rank_ << ") ";
        os << t.inner_;
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    static std::vector<std::size_t> make_shape(std::size_t dim, std::size_t rank) { return std::vector<std::size_t>(rank, dim); }

    static std::size_t pow_size(std::size_t dim, std::size_t rank) {
        std::size_t s = 1;
        for (std::size_t i = 0; i < rank; ++i) s *= dim;
        return s;
    }

    SymbolicSquareTensor wrap(SymbolicTensor t) const {
        SymbolicSquareTensor r;
        r.dim_   = dim_;
        r.rank_  = rank_;
        r.inner_ = std::move(t);
        return r;
    }

    void assert_same(const SymbolicSquareTensor& o) const;

    static int permutation_sign(const std::vector<std::size_t>& perm);

private:
    std::size_t     dim_  = 0;
    std::size_t     rank_ = 0;
    SymbolicTensor  inner_;
};

inline SymbolicSquareTensor operator*(const cas::Expression& s, const SymbolicSquareTensor& t) { return t * s; }
inline SymbolicSquareTensor operator*(double s,            const SymbolicSquareTensor& t) { return t * s; }

} // namespace tensors
} // namespace math

template <typename T> struct is_symbolic_tensor                            : std::false_type {};
template <> struct is_symbolic_tensor<math::tensors::SymbolicTensor>       : std::true_type {};
template <> struct is_symbolic_tensor<math::tensors::SymbolicSquareTensor> : std::true_type {};
template <typename T> inline constexpr bool is_symbolic_tensor_v = is_symbolic_tensor<T>::value;

} // namespace fizmo

#endif // FIZMO_SYMBOLIC_TENSORS_HPP