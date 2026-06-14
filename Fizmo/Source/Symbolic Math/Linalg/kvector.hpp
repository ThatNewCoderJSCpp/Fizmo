#ifndef FIZMO_SYMBOLIC_KVECTORS_HPP
#define FIZMO_SYMBOLIC_KVECTORS_HPP

#include "vectors.hpp"
#include "matrices.hpp"

namespace fizmo {
namespace math {
namespace vectors {

class SymbolicBivector {
public:
    SymbolicBivector() = default;
    explicit SymbolicBivector(std::size_t dimension) : dimension_(dimension), data_(num_components_for(dimension)) {}

    SymbolicBivector(std::size_t dimension, std::vector<cas::Expression> components) : dimension_(dimension), data_(std::move(components)) {
        if (data_.size() != num_components_for(dimension_))
            throw std::invalid_argument("SymbolicBivector: expected "
                + std::to_string(num_components_for(dimension_)) + " components, got "
                + std::to_string(data_.size())
            );
    }
    static SymbolicBivector zero(std::size_t dimension) {
        std::size_t n = num_components_for(dimension);
        std::vector<cas::Expression> d(n);
        for (auto& e : d) e = cas::Const(0.0);
        return { dimension, std::move(d) };
    }
    static SymbolicBivector basis(std::size_t dimension, std::size_t i, std::size_t j) {
        auto bv = zero(dimension);
        bv.at(i, j) = cas::Const(1.0);
        return bv;
    }
    static constexpr std::size_t grade() noexcept { return 2; }
    std::size_t dimension()      const noexcept { return dimension_; }
    std::size_t num_components() const noexcept { return data_.size(); }
    cas::Expression&       operator[](std::size_t i)       { return data_[i]; }
    const cas::Expression& operator[](std::size_t i) const { return data_[i]; }
    const std::vector<cas::Expression>& components() const noexcept { return data_; }
    std::vector<cas::Expression>&       components()       noexcept { return data_; }
    cas::Expression& at(std::size_t i, std::size_t j) { return data_[bivector_index(i, j, dimension_)]; }
    const cas::Expression& at(std::size_t i, std::size_t j) const { return data_[bivector_index(i, j, dimension_)]; }
    SymbolicBivector operator+(const SymbolicBivector& o) const {
        assert_same(o);
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] + o.data_[i];
        return r;
    }
    SymbolicBivector operator-(const SymbolicBivector& o) const {
        assert_same(o);
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] - o.data_[i];
        return r;
    }
    SymbolicBivector operator-() const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = -data_[i];
        return r;
    }
    SymbolicBivector operator*(const cas::Expression& s) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }
    SymbolicBivector operator*(double s) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }
    SymbolicBivector operator/(const cas::Expression& s) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }
    SymbolicBivector operator/(double s) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }
    SymbolicBivector& operator+=(const SymbolicBivector& o) { return *this = *this + o; }
    SymbolicBivector& operator-=(const SymbolicBivector& o) { return *this = *this - o; }
    SymbolicBivector& operator*=(const cas::Expression& s)       { return *this = *this * s; }
    SymbolicBivector& operator*=(double s)                   { return *this = *this * s; }
    SymbolicBivector& operator/=(const cas::Expression& s)       { return *this = *this / s; }
    SymbolicBivector& operator/=(double s)                   { return *this = *this / s; }

    cas::Expression dot(const SymbolicBivector& o) const {
        assert_same(o);
        cas::Expression s = data_[0] * o.data_[0];
        for (std::size_t i = 1; i < data_.size(); ++i) s = s + data_[i] * o.data_[i];
        return s;
    }

    cas::Expression magnitude_squared() const { return dot(*this); }
    cas::Expression magnitude()         const { return SQRT(magnitude_squared()); }
    cas::Expression norm()              const { return magnitude(); }
    SymbolicBivector unit() const { auto m = magnitude(); return *this / m; }
    SymbolicBivector grade_involution() const { return *this; }        
    SymbolicBivector reverse()          const { return -(*this); }   

    SymbolicKVector wedge(const SymbolicBivector& o)  const;
    SymbolicKVector wedge(const SymbolicTrivector& o) const;
    SymbolicKVector wedge(const SymbolicKVector& o)   const;
    SymbolicKVector wedge(const SymbolicVector2& o)   const;
    SymbolicKVector wedge(const SymbolicVector3& o)   const;
    SymbolicKVector wedge(const SymbolicVector4& o)   const;
    SymbolicKVector wedge(const SymbolicVectorN& o)   const;

    SymbolicBivector differentiate(const std::string& var) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
        return r;
    }
    SymbolicBivector differentiate_iterative(const std::string& var, unsigned n) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
        return r;
    }
    SymbolicBivector differentiate_recursive(const std::string& var, unsigned n) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
        return r;
    }
    SymbolicBivector substitute(const std::string& var, double val) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
        return r;
    }
    SymbolicBivector substitute(const std::string& var, const cas::Expression& repl) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
        return r;
    }
    SymbolicBivector substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicBivector r(dimension_, data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < r.data_.size(); ++i)
                r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
        return r;
    }
    SymbolicBivector partial_evaluate(const std::string& var, double val) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
        return r;
    }
    SymbolicBivector partial_evaluate(const std::string& var, const cas::Expression& repl) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
        return r;
    }
    SymbolicBivector partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicBivector r(dimension_, data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < r.data_.size(); ++i)
                r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
        return r;
    }
    SymbolicBivector simplify() const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
        return r;
    }
    SymbolicBivector full_simplify(const cas::RewriterConfig& cfg = {}) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
        return r;
    }
    SymbolicBivector rewrite(const cas::RewriterConfig& cfg = {}) const {
        SymbolicBivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
        return r;
    }
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const {
        std::vector<double> out(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        std::vector<double> out(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }

    friend std::ostream& operator<<(std::ostream& os, const SymbolicBivector& bv) {
        os << "Bivector<" << bv.dimension_ << ">{";
        for (std::size_t i = 0; i < bv.data_.size(); ++i) {
            if (i > 0) os << ", ";
            os << bv.data_[i].full_simplify();
        }
        os << "}";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    std::size_t dimension_ = 0;
    std::vector<cas::Expression> data_;

    static std::size_t num_components_for(std::size_t N) noexcept { return N * (N - 1) / 2; }
    static std::size_t bivector_index(std::size_t i, std::size_t j, std::size_t N) noexcept { return i * (2 * N - i - 1) / 2 + (j - i - 1); }

    void assert_same(const SymbolicBivector& o) const {
        if (dimension_ != o.dimension_)
            throw std::invalid_argument("SymbolicBivector: dimension mismatch (" + std::to_string(dimension_) + " vs " + std::to_string(o.dimension_) + ")");
    }
};

inline SymbolicBivector operator*(const cas::Expression& s, const SymbolicBivector& bv) { return bv * s; }
inline SymbolicBivector operator*(double s,             const SymbolicBivector& bv) { return bv * s; }

class SymbolicTrivector {
public:
    SymbolicTrivector() = default;

    explicit SymbolicTrivector(std::size_t dimension)
        : dimension_(dimension), data_(num_components_for(dimension)) {}

    SymbolicTrivector(std::size_t dimension, std::vector<cas::Expression> components) : dimension_(dimension), data_(std::move(components)) {
        if (data_.size() != num_components_for(dimension_))
            throw std::invalid_argument("SymbolicTrivector: expected "
                + std::to_string(num_components_for(dimension_)) + " components, got "
                + std::to_string(data_.size()));
    }
    static SymbolicTrivector zero(std::size_t dimension) {
        std::size_t n = num_components_for(dimension);
        std::vector<cas::Expression> d(n);
        for (auto& e : d) e = cas::Const(0.0);
        return { dimension, std::move(d) };
    }
    static SymbolicTrivector basis(std::size_t dimension, std::size_t i, std::size_t j, std::size_t k) {
        auto tv = zero(dimension);
        tv.at(i, j, k) = cas::Const(1.0);
        return tv;
    }
    static constexpr std::size_t grade() noexcept { return 3; }
    std::size_t dimension()      const noexcept { return dimension_; }
    std::size_t num_components() const noexcept { return data_.size(); }
    cas::Expression&       operator[](std::size_t i)       { return data_[i]; }
    const cas::Expression& operator[](std::size_t i) const { return data_[i]; }
    const std::vector<cas::Expression>& components() const noexcept { return data_; }
    std::vector<cas::Expression>&       components()       noexcept { return data_; }
    cas::Expression& at(std::size_t i, std::size_t j, std::size_t k) { return data_[trivector_index(i, j, k, dimension_)]; }
    const cas::Expression& at(std::size_t i, std::size_t j, std::size_t k) const { return data_[trivector_index(i, j, k, dimension_)]; }
    SymbolicTrivector operator+(const SymbolicTrivector& o) const {
        assert_same(o);
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] + o.data_[i];
        return r;
    }
    SymbolicTrivector operator-(const SymbolicTrivector& o) const {
        assert_same(o);
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] - o.data_[i];
        return r;
    }
    SymbolicTrivector operator-() const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = -data_[i];
        return r;
    }
    SymbolicTrivector operator*(const cas::Expression& s) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }
    SymbolicTrivector operator*(double s) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }
    SymbolicTrivector operator/(const cas::Expression& s) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }
    SymbolicTrivector operator/(double s) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }
    SymbolicTrivector& operator+=(const SymbolicTrivector& o) { return *this = *this + o; }
    SymbolicTrivector& operator-=(const SymbolicTrivector& o) { return *this = *this - o; }
    SymbolicTrivector& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicTrivector& operator*=(double s)                    { return *this = *this * s; }
    SymbolicTrivector& operator/=(const cas::Expression& s)        { return *this = *this / s; }
    SymbolicTrivector& operator/=(double s)                    { return *this = *this / s; }
    cas::Expression dot(const SymbolicTrivector& o) const {
        assert_same(o);
        cas::Expression s = data_[0] * o.data_[0];
        for (std::size_t i = 1; i < data_.size(); ++i) s = s + data_[i] * o.data_[i];
        return s;
    }
    cas::Expression magnitude_squared() const { return dot(*this); }
    cas::Expression magnitude()         const { return SQRT(magnitude_squared()); }
    cas::Expression norm()              const { return magnitude(); }
    SymbolicTrivector unit() const { auto m = magnitude(); return *this / m; }
    SymbolicTrivector grade_involution() const { return -(*this); }     
    SymbolicTrivector reverse()          const { return -(*this); } 
    
    SymbolicKVector wedge(const SymbolicTrivector& o) const;
    SymbolicKVector wedge(const SymbolicBivector& o)  const;
    SymbolicKVector wedge(const SymbolicKVector& o)   const;
    SymbolicKVector wedge(const SymbolicVector2& o)   const;
    SymbolicKVector wedge(const SymbolicVector3& o)   const;
    SymbolicKVector wedge(const SymbolicVector4& o)   const;
    SymbolicKVector wedge(const SymbolicVectorN& o)   const;

    SymbolicTrivector differentiate(const std::string& var) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
        return r;
    }
    SymbolicTrivector differentiate_iterative(const std::string& var, unsigned n) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
        return r;
    }
    SymbolicTrivector differentiate_recursive(const std::string& var, unsigned n) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
        return r;
    }
    SymbolicTrivector substitute(const std::string& var, double val) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
        return r;
    }
    SymbolicTrivector substitute(const std::string& var, const cas::Expression& repl) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
        return r;
    }
    SymbolicTrivector substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicTrivector r(dimension_, data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < r.data_.size(); ++i)
                r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
        return r;
    }
    SymbolicTrivector partial_evaluate(const std::string& var, double val) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
        return r;
    }
    SymbolicTrivector partial_evaluate(const std::string& var, const cas::Expression& repl) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
        return r;
    }
    SymbolicTrivector partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicTrivector r(dimension_, data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < r.data_.size(); ++i)
                r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
        return r;
    }
    SymbolicTrivector simplify() const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
        return r;
    }
    SymbolicTrivector full_simplify(const cas::RewriterConfig& cfg = {}) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
        return r;
    }
    SymbolicTrivector rewrite(const cas::RewriterConfig& cfg = {}) const {
        SymbolicTrivector r(dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
        return r;
    }
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const {
        std::vector<double> out(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        std::vector<double> out(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }

    friend std::ostream& operator<<(std::ostream& os, const SymbolicTrivector& tv) {
        os << "Trivector<" << tv.dimension_ << ">{";
        for (std::size_t i = 0; i < tv.data_.size(); ++i) {
            if (i > 0) os << ", ";
            os << tv.data_[i].full_simplify();
        }
        os << "}";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    std::size_t dimension_ = 0;
    std::vector<cas::Expression> data_;

    static std::size_t binomial3(std::size_t n) noexcept { return n >= 3 ? n * (n - 1) * (n - 2) / 6 : 0; }
    static std::size_t num_components_for(std::size_t N) noexcept { return binomial3(N); }

    static std::size_t trivector_index(std::size_t i, std::size_t j, std::size_t k, std::size_t N) noexcept {
        std::size_t idx = 0;
        for (std::size_t a = 0; a < i; ++a) idx += (N - a - 1) * (N - a - 2) / 2;
        for (std::size_t b = i + 1; b < j; ++b) idx += (N - b - 1);
        idx += (k - j - 1);
        return idx;
    }

    void assert_same(const SymbolicTrivector& o) const {
        if (dimension_ != o.dimension_)
            throw std::invalid_argument("SymbolicTrivector: dimension mismatch (" + std::to_string(dimension_) + " vs " + std::to_string(o.dimension_) + ")");
    }
};

inline SymbolicTrivector operator*(const cas::Expression& s, const SymbolicTrivector& tv) { return tv * s; }
inline SymbolicTrivector operator*(double s,             const SymbolicTrivector& tv) { return tv * s; }

class SymbolicKVector {
public:
    SymbolicKVector() = default;
    SymbolicKVector(std::size_t grade, std::size_t dimension) : grade_(grade), dimension_(dimension), data_(binomial(dimension, grade)) {}

    SymbolicKVector(std::size_t grade, std::size_t dimension, std::vector<cas::Expression> components) : grade_(grade), dimension_(dimension), data_(std::move(components)) {
        if (data_.size() != binomial(dimension_, grade_))
            throw std::invalid_argument(
                "SymbolicKVector: expected "
                + std::to_string(binomial(dimension_, grade_)) + " components, got "
                + std::to_string(data_.size())
            );
    }

    SymbolicKVector(const SymbolicBivector& bv)  : SymbolicKVector(SymbolicKVector::from(bv)) {}
    SymbolicKVector(const SymbolicTrivector& tv) : SymbolicKVector(SymbolicKVector::from(tv)) {}
    SymbolicKVector(const SymbolicVector2& v2)   : SymbolicKVector(SymbolicKVector::from_vector(v2)) {}
    SymbolicKVector(const SymbolicVector3& v3)   : SymbolicKVector(SymbolicKVector::from_vector(v3)) {}
    SymbolicKVector(const SymbolicVector4& v4)   : SymbolicKVector(SymbolicKVector::from_vector(v4)) {}
    SymbolicKVector(const SymbolicVectorN& vn)   : SymbolicKVector(SymbolicKVector::from_vector(vn)) {}

    static SymbolicKVector zero(std::size_t grade, std::size_t dimension) {
        std::size_t n = binomial(dimension, grade);
        std::vector<cas::Expression> d(n);
        for (auto& e : d) e = cas::Const(0.0);
        return { grade, dimension, std::move(d) };
    }
    static SymbolicKVector basis(std::size_t grade, std::size_t dimension, std::size_t flat_idx) {
        auto kv = zero(grade, dimension);
        if (flat_idx < kv.data_.size()) kv.data_[flat_idx] = cas::Const(1.0);
        return kv;
    }
    static SymbolicKVector scalar(const cas::Expression& val, std::size_t dimension) { return { 0, dimension, { val } }; }
    static SymbolicKVector pseudoscalar(std::size_t dimension) { return { dimension, dimension, { cas::Const(1.0) } }; }
    std::size_t grade()          const noexcept { return grade_; }
    std::size_t dimension()      const noexcept { return dimension_; }
    std::size_t num_components() const noexcept { return data_.size(); }
    cas::Expression&       operator[](std::size_t i)       { return data_[i]; }
    const cas::Expression& operator[](std::size_t i) const { return data_[i]; }
    const std::vector<cas::Expression>& components() const noexcept { return data_; }
    std::vector<cas::Expression>&       components()       noexcept { return data_; }
    cas::Expression& at(const std::vector<std::size_t>& indices) { return data_[compute_flat_index(indices, dimension_)]; }
    const cas::Expression& at(const std::vector<std::size_t>& indices) const { return data_[compute_flat_index(indices, dimension_)]; }

    SymbolicKVector operator+(const SymbolicKVector& o) const {
        assert_same(o);
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] + o.data_[i];
        return r;
    }
    SymbolicKVector operator-(const SymbolicKVector& o) const {
        assert_same(o);
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] - o.data_[i];
        return r;
    }
    SymbolicKVector operator-() const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = -data_[i];
        return r;
    }
    SymbolicKVector operator*(const cas::Expression& s) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }
    SymbolicKVector operator*(double s) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }
    SymbolicKVector operator/(const cas::Expression& s) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }
    SymbolicKVector operator/(double s) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }
    SymbolicKVector& operator+=(const SymbolicKVector& o) { return *this = *this + o; }
    SymbolicKVector& operator-=(const SymbolicKVector& o) { return *this = *this - o; }
    SymbolicKVector& operator*=(const cas::Expression& s)      { return *this = *this * s; }
    SymbolicKVector& operator*=(double s)                  { return *this = *this * s; }
    SymbolicKVector& operator/=(const cas::Expression& s)      { return *this = *this / s; }
    SymbolicKVector& operator/=(double s)                  { return *this = *this / s; }

    cas::Expression dot(const SymbolicKVector& o) const {
        assert_same(o);
        cas::Expression s = data_[0] * o.data_[0];
        for (std::size_t i = 1; i < data_.size(); ++i) s = s + data_[i] * o.data_[i];
        return s;
    }

    cas::Expression magnitude_squared() const { return dot(*this); }
    cas::Expression magnitude()         const { return SQRT(magnitude_squared()); }
    cas::Expression norm()              const { return magnitude(); }
    SymbolicKVector unit() const { auto m = magnitude(); return *this / m; }

    SymbolicKVector wedge(const SymbolicKVector& o) const {
        if (dimension_ != o.dimension_)
            throw std::invalid_argument("SymbolicKVector::wedge: dimension mismatch (" + std::to_string(dimension_) + " vs " + std::to_string(o.dimension_) + ")");
        std::size_t K  = grade_ + o.grade_;
        std::size_t N  = dimension_;
        if (K > N) return zero(0, N);
        auto result = zero(K, N);
        auto blades_a = enumerate_blades(grade_, N);
        auto blades_b = enumerate_blades(o.grade_, N);
        std::vector<std::size_t> merged;
        for (std::size_t a = 0; a < blades_a.size(); ++a) {
            for (std::size_t b = 0; b < blades_b.size(); ++b) {
                int sign = merge_sign(blades_a[a], blades_b[b], merged);
                if (sign == 0) continue;
                std::size_t idx = compute_flat_index(merged, N);
                cas::Expression term = data_[a] * o.data_[b];
                if (sign < 0) term = -term;
                result.data_[idx] = result.data_[idx] + term;
            }
        }
        return result;
    }

    SymbolicKVector grade_involution() const { return (grade_ % 2 == 0) ? *this : -(*this); }

    SymbolicKVector reverse() const {
        std::size_t s = grade_ * (grade_ - 1) / 2;
        return (s % 2 == 0) ? *this : -(*this);
    }

    SymbolicKVector wedge(const SymbolicBivector& o)  const { return wedge(SymbolicKVector(o)); }
    SymbolicKVector wedge(const SymbolicTrivector& o) const { return wedge(SymbolicKVector(o)); }
    SymbolicKVector wedge(const SymbolicVector2& o)   const { return wedge(SymbolicKVector(o)); }
    SymbolicKVector wedge(const SymbolicVector3& o)   const { return wedge(SymbolicKVector(o)); }
    SymbolicKVector wedge(const SymbolicVector4& o)   const { return wedge(SymbolicKVector(o)); }
    SymbolicKVector wedge(const SymbolicVectorN& o)   const { return wedge(SymbolicKVector(o)); }

    static SymbolicKVector from(const SymbolicBivector& bv) { return { 2, bv.dimension(), std::vector<cas::Expression>(bv.components()) }; }
    static SymbolicKVector from(const SymbolicTrivector& tv) { return { 3, tv.dimension(), std::vector<cas::Expression>(tv.components()) }; }
    static SymbolicKVector from_vector(const SymbolicVector2& v) { return { 1, 2, { v.x, v.y } }; }
    static SymbolicKVector from_vector(const SymbolicVector3& v) { return { 1, 3, { v.x, v.y, v.z } }; }
    static SymbolicKVector from_vector(const SymbolicVector4& v) { return { 1, 4, { v.x, v.y, v.z, v.w } }; }
    static SymbolicKVector from_vector(const SymbolicVectorN& v) { return { 1, v.dimension(), std::vector<cas::Expression>(v.components()) }; }

    SymbolicBivector to_bivector() const {
        if (grade_ != 2) throw std::domain_error("SymbolicKVector::to_bivector: grade is " + std::to_string(grade_) + ", not 2");
        return { dimension_, std::vector<cas::Expression>(data_) };
    }
    SymbolicTrivector to_trivector() const {
        if (grade_ != 3) throw std::domain_error("SymbolicKVector::to_trivector: grade is " + std::to_string(grade_) + ", not 3");
        return { dimension_, std::vector<cas::Expression>(data_) };
    }
    SymbolicKVector differentiate(const std::string& var) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
        return r;
    }
    SymbolicKVector differentiate_iterative(const std::string& var, unsigned n) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
        return r;
    }
    SymbolicKVector differentiate_recursive(const std::string& var, unsigned n) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
        return r;
    }
    SymbolicKVector substitute(const std::string& var, double val) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
        return r;
    }
    SymbolicKVector substitute(const std::string& var, const cas::Expression& repl) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
        return r;
    }
    SymbolicKVector substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicKVector r(grade_, dimension_, data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < r.data_.size(); ++i)
                r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
        return r;
    }
    SymbolicKVector partial_evaluate(const std::string& var, double val) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
        return r;
    }
    SymbolicKVector partial_evaluate(const std::string& var, const cas::Expression& repl) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
        return r;
    }
    SymbolicKVector partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicKVector r(grade_, dimension_, data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < r.data_.size(); ++i)
                r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
        return r;
    }
    SymbolicKVector simplify() const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
        return r;
    }
    SymbolicKVector full_simplify(const cas::RewriterConfig& cfg = {}) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
        return r;
    }
    SymbolicKVector rewrite(const cas::RewriterConfig& cfg = {}) const {
        SymbolicKVector r(grade_, dimension_);
        for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
        return r;
    }
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const {
        std::vector<double> out(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        std::vector<double> out(data_.size());
        for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }
    friend std::ostream& operator<<(std::ostream& os, const SymbolicKVector& kv) {
        os << "KVector<" << kv.grade_ << "," << kv.dimension_ << ">{";
        for (std::size_t i = 0; i < kv.data_.size(); ++i) {
            if (i > 0) os << ", ";
            os << kv.data_[i].full_simplify();
        }
        os << "}";
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

    static std::size_t binomial(std::size_t n, std::size_t k) noexcept {
        if (k > n) return 0;
        if (k == 0 || k == n) return 1;
        if (k > n - k) k = n - k;
        std::size_t r = 1;
        for (std::size_t i = 0; i < k; ++i) r = r * (n - i) / (i + 1);
        return r;
    }

    static std::size_t compute_flat_index(const std::vector<std::size_t>& indices, std::size_t N) noexcept {
        std::size_t K = indices.size();
        std::size_t idx = 0;
        for (std::size_t pos = 0; pos < K; ++pos) {
            std::size_t start = (pos == 0) ? 0 : indices[pos - 1] + 1;
            for (std::size_t c = start; c < indices[pos]; ++c) idx += binomial(N - c - 1, K - pos - 1);
        }
        return idx;
    }

private:
    std::size_t grade_     = 0;
    std::size_t dimension_ = 0;
    std::vector<cas::Expression> data_;

    void assert_same(const SymbolicKVector& o) const {
        if (grade_ != o.grade_ || dimension_ != o.dimension_)
            throw std::invalid_argument("SymbolicKVector: grade/dimension mismatch ("
                + std::to_string(grade_) + "," + std::to_string(dimension_) + " vs "
                + std::to_string(o.grade_) + "," + std::to_string(o.dimension_) + ")");
    }

    static void enumerate_blades_impl(std::size_t K, std::size_t N, std::size_t start, std::vector<std::size_t>& current, std::vector<std::vector<std::size_t>>& result) {
        if (current.size() == K) { result.push_back(current); return; }
        std::size_t remaining = K - current.size();
        for (std::size_t i = start; i + remaining <= N; ++i) {
            current.push_back(i);
            enumerate_blades_impl(K, N, i + 1, current, result);
            current.pop_back();
        }
    }

    static std::vector<std::vector<std::size_t>> enumerate_blades(std::size_t K, std::size_t N) {
        std::vector<std::vector<std::size_t>> result;
        result.reserve(binomial(N, K));
        std::vector<std::size_t> current;
        current.reserve(K);
        enumerate_blades_impl(K, N, 0, current, result);
        return result;
    }

    static int merge_sign(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, std::vector<std::size_t>& merged) {
        merged.clear();
        merged.reserve(I.size() + J.size());
        std::size_t a = 0, b = 0;
        int swaps = 0;
        while (a < I.size() && b < J.size()) {
            if (I[a] < J[b]) {
                merged.push_back(I[a++]);
            } else if (I[a] > J[b]) {
                merged.push_back(J[b++]);
                swaps += static_cast<int>(I.size() - a);
            } else {
                return 0;
            }
        }
        while (a < I.size()) merged.push_back(I[a++]);
        while (b < J.size()) merged.push_back(J[b++]);
        return (swaps % 2 == 0) ? 1 : -1;
    }
};

inline SymbolicKVector operator*(const cas::Expression& s, const SymbolicKVector& kv) { return kv * s; }
inline SymbolicKVector operator*(double s,             const SymbolicKVector& kv) { return kv * s; }

inline SymbolicBivector SymbolicVector2::wedge(const SymbolicVector2& o) const {
    return SymbolicBivector(2, { x * o.y - y * o.x });
}

inline SymbolicBivector SymbolicVector3::wedge(const SymbolicVector3& o) const {
    return SymbolicBivector(3, {
        x * o.y - y * o.x,   // e01
        x * o.z - z * o.x,   // e02
        y * o.z - z * o.y    // e12
    });
}

inline SymbolicBivector SymbolicVector4::wedge(const SymbolicVector4& o) const {
    return SymbolicBivector(4, {
        x * o.y - y * o.x,   // e01
        x * o.z - z * o.x,   // e02
        x * o.w - w * o.x,   // e03
        y * o.z - z * o.y,   // e12
        y * o.w - w * o.y,   // e13
        z * o.w - w * o.z    // e23
    });
}

inline SymbolicBivector SymbolicVectorN::wedge(const SymbolicVectorN& o) const {
    assert_same_dim(o);
    std::size_t N = dimension();
    auto result = SymbolicBivector::zero(N);
    std::size_t idx = 0;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            result[idx++] = data_[i] * o.data_[j] - data_[j] * o.data_[i];
    return result;
}

inline SymbolicKVector SymbolicBivector::wedge(const SymbolicBivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicBivector::wedge(const SymbolicTrivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicBivector::wedge(const SymbolicKVector& o) const { return SymbolicKVector(*this).wedge(o); }
inline SymbolicKVector SymbolicBivector::wedge(const SymbolicVector2& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicBivector::wedge(const SymbolicVector3& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicBivector::wedge(const SymbolicVector4& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicBivector::wedge(const SymbolicVectorN& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }

inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicTrivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicBivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicKVector& o) const { return SymbolicKVector(*this).wedge(o); }
inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicVector2& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicVector3& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicVector4& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicTrivector::wedge(const SymbolicVectorN& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }

inline SymbolicKVector SymbolicVector2::wedge(const SymbolicBivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVector2::wedge(const SymbolicTrivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVector2::wedge(const SymbolicKVector& o) const { return SymbolicKVector(*this).wedge(o); }

inline SymbolicKVector SymbolicVector3::wedge(const SymbolicBivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVector3::wedge(const SymbolicTrivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVector3::wedge(const SymbolicKVector& o) const { return SymbolicKVector(*this).wedge(o); }

inline SymbolicKVector SymbolicVector4::wedge(const SymbolicBivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVector4::wedge(const SymbolicTrivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVector4::wedge(const SymbolicKVector& o) const { return SymbolicKVector(*this).wedge(o); }

inline SymbolicKVector SymbolicVectorN::wedge(const SymbolicBivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVectorN::wedge(const SymbolicTrivector& o) const { return SymbolicKVector(*this).wedge(SymbolicKVector(o)); }
inline SymbolicKVector SymbolicVectorN::wedge(const SymbolicKVector& o) const { return SymbolicKVector(*this).wedge(o); }

} // namespace vectors
} // namespace math
} // namespace fizmo

#endif // FIZMO_SYMBOLIC_KVECTORS_HPP