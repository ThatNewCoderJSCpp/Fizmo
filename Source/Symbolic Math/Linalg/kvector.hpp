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

    SymbolicBivector(std::size_t dimension, std::vector<cas::Expression> components);
    static SymbolicBivector zero(std::size_t dimension);
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
    SymbolicBivector operator+(const SymbolicBivector& o) const;
    SymbolicBivector operator-(const SymbolicBivector& o) const;
    SymbolicBivector operator-() const;
    SymbolicBivector operator*(const cas::Expression& s) const;
    SymbolicBivector operator*(double s) const;
    SymbolicBivector operator/(const cas::Expression& s) const;
    SymbolicBivector operator/(double s) const;
    SymbolicBivector& operator+=(const SymbolicBivector& o) { return *this = *this + o; }
    SymbolicBivector& operator-=(const SymbolicBivector& o) { return *this = *this - o; }
    SymbolicBivector& operator*=(const cas::Expression& s)       { return *this = *this * s; }
    SymbolicBivector& operator*=(double s)                   { return *this = *this * s; }
    SymbolicBivector& operator/=(const cas::Expression& s)       { return *this = *this / s; }
    SymbolicBivector& operator/=(double s)                   { return *this = *this / s; }

    cas::Expression dot(const SymbolicBivector& o) const;

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

    SymbolicBivector differentiate(const std::string& var) const;
    SymbolicBivector differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicBivector differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicBivector substitute(const std::string& var, double val) const;
    SymbolicBivector substitute(const std::string& var, const cas::Expression& repl) const;
    SymbolicBivector substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicBivector partial_evaluate(const std::string& var, double val) const;
    SymbolicBivector partial_evaluate(const std::string& var, const cas::Expression& repl) const;
    SymbolicBivector partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicBivector simplify() const;
    SymbolicBivector full_simplify(const cas::RewriterConfig& cfg = {}) const;
    SymbolicBivector rewrite(const cas::RewriterConfig& cfg = {}) const;
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const;
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;

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

    void assert_same(const SymbolicBivector& o) const;
};

inline SymbolicBivector operator*(const cas::Expression& s, const SymbolicBivector& bv) { return bv * s; }
inline SymbolicBivector operator*(double s,             const SymbolicBivector& bv) { return bv * s; }

class SymbolicTrivector {
public:
    SymbolicTrivector() = default;

    explicit SymbolicTrivector(std::size_t dimension)
        : dimension_(dimension), data_(num_components_for(dimension)) {}

    SymbolicTrivector(std::size_t dimension, std::vector<cas::Expression> components);
    static SymbolicTrivector zero(std::size_t dimension);
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
    SymbolicTrivector operator+(const SymbolicTrivector& o) const;
    SymbolicTrivector operator-(const SymbolicTrivector& o) const;
    SymbolicTrivector operator-() const;
    SymbolicTrivector operator*(const cas::Expression& s) const;
    SymbolicTrivector operator*(double s) const;
    SymbolicTrivector operator/(const cas::Expression& s) const;
    SymbolicTrivector operator/(double s) const;
    SymbolicTrivector& operator+=(const SymbolicTrivector& o) { return *this = *this + o; }
    SymbolicTrivector& operator-=(const SymbolicTrivector& o) { return *this = *this - o; }
    SymbolicTrivector& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicTrivector& operator*=(double s)                    { return *this = *this * s; }
    SymbolicTrivector& operator/=(const cas::Expression& s)        { return *this = *this / s; }
    SymbolicTrivector& operator/=(double s)                    { return *this = *this / s; }
    cas::Expression dot(const SymbolicTrivector& o) const;
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

    SymbolicTrivector differentiate(const std::string& var) const;
    SymbolicTrivector differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicTrivector differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicTrivector substitute(const std::string& var, double val) const;
    SymbolicTrivector substitute(const std::string& var, const cas::Expression& repl) const;
    SymbolicTrivector substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicTrivector partial_evaluate(const std::string& var, double val) const;
    SymbolicTrivector partial_evaluate(const std::string& var, const cas::Expression& repl) const;
    SymbolicTrivector partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicTrivector simplify() const;
    SymbolicTrivector full_simplify(const cas::RewriterConfig& cfg = {}) const;
    SymbolicTrivector rewrite(const cas::RewriterConfig& cfg = {}) const;
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const;
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;

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

    static std::size_t trivector_index(std::size_t i, std::size_t j, std::size_t k, std::size_t N) noexcept;

    void assert_same(const SymbolicTrivector& o) const;
};

inline SymbolicTrivector operator*(const cas::Expression& s, const SymbolicTrivector& tv) { return tv * s; }
inline SymbolicTrivector operator*(double s,             const SymbolicTrivector& tv) { return tv * s; }

class SymbolicKVector {
public:
    SymbolicKVector() = default;
    SymbolicKVector(std::size_t grade, std::size_t dimension) : grade_(grade), dimension_(dimension), data_(binomial(dimension, grade)) {}

    SymbolicKVector(std::size_t grade, std::size_t dimension, std::vector<cas::Expression> components);

    SymbolicKVector(const SymbolicBivector& bv)  : SymbolicKVector(SymbolicKVector::from(bv)) {}
    SymbolicKVector(const SymbolicTrivector& tv) : SymbolicKVector(SymbolicKVector::from(tv)) {}
    SymbolicKVector(const SymbolicVector2& v2)   : SymbolicKVector(SymbolicKVector::from_vector(v2)) {}
    SymbolicKVector(const SymbolicVector3& v3)   : SymbolicKVector(SymbolicKVector::from_vector(v3)) {}
    SymbolicKVector(const SymbolicVector4& v4)   : SymbolicKVector(SymbolicKVector::from_vector(v4)) {}
    SymbolicKVector(const SymbolicVectorN& vn)   : SymbolicKVector(SymbolicKVector::from_vector(vn)) {}

    static SymbolicKVector zero(std::size_t grade, std::size_t dimension);
    static SymbolicKVector basis(std::size_t grade, std::size_t dimension, std::size_t flat_idx);
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

    SymbolicKVector operator+(const SymbolicKVector& o) const;
    SymbolicKVector operator-(const SymbolicKVector& o) const;
    SymbolicKVector operator-() const;
    SymbolicKVector operator*(const cas::Expression& s) const;
    SymbolicKVector operator*(double s) const;
    SymbolicKVector operator/(const cas::Expression& s) const;
    SymbolicKVector operator/(double s) const;
    SymbolicKVector& operator+=(const SymbolicKVector& o) { return *this = *this + o; }
    SymbolicKVector& operator-=(const SymbolicKVector& o) { return *this = *this - o; }
    SymbolicKVector& operator*=(const cas::Expression& s)      { return *this = *this * s; }
    SymbolicKVector& operator*=(double s)                  { return *this = *this * s; }
    SymbolicKVector& operator/=(const cas::Expression& s)      { return *this = *this / s; }
    SymbolicKVector& operator/=(double s)                  { return *this = *this / s; }

    cas::Expression dot(const SymbolicKVector& o) const;

    cas::Expression magnitude_squared() const { return dot(*this); }
    cas::Expression magnitude()         const { return SQRT(magnitude_squared()); }
    cas::Expression norm()              const { return magnitude(); }
    SymbolicKVector unit() const { auto m = magnitude(); return *this / m; }

    SymbolicKVector wedge(const SymbolicKVector& o) const;

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

    SymbolicBivector to_bivector() const;
    SymbolicTrivector to_trivector() const;
    SymbolicKVector differentiate(const std::string& var) const;
    SymbolicKVector differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicKVector differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicKVector substitute(const std::string& var, double val) const;
    SymbolicKVector substitute(const std::string& var, const cas::Expression& repl) const;
    SymbolicKVector substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicKVector partial_evaluate(const std::string& var, double val) const;
    SymbolicKVector partial_evaluate(const std::string& var, const cas::Expression& repl) const;
    SymbolicKVector partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicKVector simplify() const;
    SymbolicKVector full_simplify(const cas::RewriterConfig& cfg = {}) const;
    SymbolicKVector rewrite(const cas::RewriterConfig& cfg = {}) const;
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const;
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;
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

    static std::size_t binomial(std::size_t n, std::size_t k) noexcept;

    static std::size_t compute_flat_index(const std::vector<std::size_t>& indices, std::size_t N) noexcept;

private:
    std::size_t grade_     = 0;
    std::size_t dimension_ = 0;
    std::vector<cas::Expression> data_;

    void assert_same(const SymbolicKVector& o) const;

    static void enumerate_blades_impl(std::size_t K, std::size_t N, std::size_t start, std::vector<std::size_t>& current, std::vector<std::vector<std::size_t>>& result);

    static std::vector<std::vector<std::size_t>> enumerate_blades(std::size_t K, std::size_t N);

    static int merge_sign(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, std::vector<std::size_t>& merged);
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