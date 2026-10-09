#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace vectors {

SymbolicBivector::SymbolicBivector(std::size_t dimension, std::vector<cas::Expression> components) : dimension_(dimension), data_(std::move(components)) {
    if (data_.size() != num_components_for(dimension_))
        throw std::invalid_argument("SymbolicBivector: expected "
            + std::to_string(num_components_for(dimension_)) + " components, got "
            + std::to_string(data_.size())
        );
}

auto SymbolicBivector::zero(std::size_t dimension) -> SymbolicBivector {
    std::size_t n = num_components_for(dimension);
    std::vector<cas::Expression> d(n);
    for (auto& e : d) e = cas::Const(0.0);
    return { dimension, std::move(d) };
}

auto SymbolicBivector::operator+(const SymbolicBivector& o) const -> SymbolicBivector {
    assert_same(o);
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] + o.data_[i];
    return r;
}

auto SymbolicBivector::operator-(const SymbolicBivector& o) const -> SymbolicBivector {
    assert_same(o);
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] - o.data_[i];
    return r;
}

auto SymbolicBivector::operator-() const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = -data_[i];
    return r;
}

auto SymbolicBivector::operator*(const cas::Expression& s) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
    return r;
}

auto SymbolicBivector::operator*(double s) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
    return r;
}

auto SymbolicBivector::operator/(const cas::Expression& s) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
    return r;
}

auto SymbolicBivector::operator/(double s) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
    return r;
}

auto SymbolicBivector::dot(const SymbolicBivector& o) const -> cas::Expression {
    assert_same(o);
    cas::Expression s = data_[0] * o.data_[0];
    for (std::size_t i = 1; i < data_.size(); ++i) s = s + data_[i] * o.data_[i];
    return s;
}

auto SymbolicBivector::differentiate(const std::string& var) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
    return r;
}

auto SymbolicBivector::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
    return r;
}

auto SymbolicBivector::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
    return r;
}

auto SymbolicBivector::substitute(const std::string& var, double val) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
    return r;
}

auto SymbolicBivector::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
    return r;
}

auto SymbolicBivector::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicBivector {
    SymbolicBivector r(dimension_, data_);
    for (auto& [n, v] : pairs)
        for (std::size_t i = 0; i < r.data_.size(); ++i)
            r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
    return r;
}

auto SymbolicBivector::partial_evaluate(const std::string& var, double val) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
    return r;
}

auto SymbolicBivector::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
    return r;
}

auto SymbolicBivector::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicBivector {
    SymbolicBivector r(dimension_, data_);
    for (auto& [n, v] : pairs)
        for (std::size_t i = 0; i < r.data_.size(); ++i)
            r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
    return r;
}

auto SymbolicBivector::simplify() const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
    return r;
}

auto SymbolicBivector::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
    return r;
}

auto SymbolicBivector::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicBivector {
    SymbolicBivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
    return r;
}

std::vector<double> SymbolicBivector::evaluate(const std::unordered_map<std::string, double>& vals) const {
    std::vector<double> out(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
    return out;
}

std::vector<double> SymbolicBivector::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
    std::vector<double> out(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
    return out;
}

void SymbolicBivector::assert_same(const SymbolicBivector& o) const {
    if (dimension_ != o.dimension_)
        throw std::invalid_argument("SymbolicBivector: dimension mismatch (" + std::to_string(dimension_) + " vs " + std::to_string(o.dimension_) + ")");
}

SymbolicTrivector::SymbolicTrivector(std::size_t dimension, std::vector<cas::Expression> components) : dimension_(dimension), data_(std::move(components)) {
    if (data_.size() != num_components_for(dimension_))
        throw std::invalid_argument("SymbolicTrivector: expected "
            + std::to_string(num_components_for(dimension_)) + " components, got "
            + std::to_string(data_.size()));
}

auto SymbolicTrivector::zero(std::size_t dimension) -> SymbolicTrivector {
    std::size_t n = num_components_for(dimension);
    std::vector<cas::Expression> d(n);
    for (auto& e : d) e = cas::Const(0.0);
    return { dimension, std::move(d) };
}

auto SymbolicTrivector::operator+(const SymbolicTrivector& o) const -> SymbolicTrivector {
    assert_same(o);
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] + o.data_[i];
    return r;
}

auto SymbolicTrivector::operator-(const SymbolicTrivector& o) const -> SymbolicTrivector {
    assert_same(o);
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] - o.data_[i];
    return r;
}

auto SymbolicTrivector::operator-() const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = -data_[i];
    return r;
}

auto SymbolicTrivector::operator*(const cas::Expression& s) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
    return r;
}

auto SymbolicTrivector::operator*(double s) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
    return r;
}

auto SymbolicTrivector::operator/(const cas::Expression& s) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
    return r;
}

auto SymbolicTrivector::operator/(double s) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
    return r;
}

auto SymbolicTrivector::dot(const SymbolicTrivector& o) const -> cas::Expression {
    assert_same(o);
    cas::Expression s = data_[0] * o.data_[0];
    for (std::size_t i = 1; i < data_.size(); ++i) s = s + data_[i] * o.data_[i];
    return s;
}

auto SymbolicTrivector::differentiate(const std::string& var) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
    return r;
}

auto SymbolicTrivector::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
    return r;
}

auto SymbolicTrivector::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
    return r;
}

auto SymbolicTrivector::substitute(const std::string& var, double val) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
    return r;
}

auto SymbolicTrivector::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
    return r;
}

auto SymbolicTrivector::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_, data_);
    for (auto& [n, v] : pairs)
        for (std::size_t i = 0; i < r.data_.size(); ++i)
            r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
    return r;
}

auto SymbolicTrivector::partial_evaluate(const std::string& var, double val) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
    return r;
}

auto SymbolicTrivector::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
    return r;
}

auto SymbolicTrivector::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_, data_);
    for (auto& [n, v] : pairs)
        for (std::size_t i = 0; i < r.data_.size(); ++i)
            r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
    return r;
}

auto SymbolicTrivector::simplify() const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
    return r;
}

auto SymbolicTrivector::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
    return r;
}

auto SymbolicTrivector::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicTrivector {
    SymbolicTrivector r(dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
    return r;
}

std::vector<double> SymbolicTrivector::evaluate(const std::unordered_map<std::string, double>& vals) const {
    std::vector<double> out(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
    return out;
}

std::vector<double> SymbolicTrivector::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
    std::vector<double> out(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
    return out;
}

std::size_t SymbolicTrivector::trivector_index(std::size_t i, std::size_t j, std::size_t k, std::size_t N) noexcept {
    std::size_t idx = 0;
    for (std::size_t a = 0; a < i; ++a) idx += (N - a - 1) * (N - a - 2) / 2;
    for (std::size_t b = i + 1; b < j; ++b) idx += (N - b - 1);
    idx += (k - j - 1);
    return idx;
}

void SymbolicTrivector::assert_same(const SymbolicTrivector& o) const {
    if (dimension_ != o.dimension_)
        throw std::invalid_argument("SymbolicTrivector: dimension mismatch (" + std::to_string(dimension_) + " vs " + std::to_string(o.dimension_) + ")");
}

SymbolicKVector::SymbolicKVector(std::size_t grade, std::size_t dimension, std::vector<cas::Expression> components) : grade_(grade), dimension_(dimension), data_(std::move(components)) {
    if (data_.size() != binomial(dimension_, grade_))
        throw std::invalid_argument(
            "SymbolicKVector: expected "
            + std::to_string(binomial(dimension_, grade_)) + " components, got "
            + std::to_string(data_.size())
        );
}

auto SymbolicKVector::zero(std::size_t grade, std::size_t dimension) -> SymbolicKVector {
    std::size_t n = binomial(dimension, grade);
    std::vector<cas::Expression> d(n);
    for (auto& e : d) e = cas::Const(0.0);
    return { grade, dimension, std::move(d) };
}

auto SymbolicKVector::basis(std::size_t grade, std::size_t dimension, std::size_t flat_idx) -> SymbolicKVector {
    auto kv = zero(grade, dimension);
    if (flat_idx < kv.data_.size()) kv.data_[flat_idx] = cas::Const(1.0);
    return kv;
}

auto SymbolicKVector::operator+(const SymbolicKVector& o) const -> SymbolicKVector {
    assert_same(o);
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] + o.data_[i];
    return r;
}

auto SymbolicKVector::operator-(const SymbolicKVector& o) const -> SymbolicKVector {
    assert_same(o);
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] - o.data_[i];
    return r;
}

auto SymbolicKVector::operator-() const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = -data_[i];
    return r;
}

auto SymbolicKVector::operator*(const cas::Expression& s) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
    return r;
}

auto SymbolicKVector::operator*(double s) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] * s;
    return r;
}

auto SymbolicKVector::operator/(const cas::Expression& s) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
    return r;
}

auto SymbolicKVector::operator/(double s) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = data_[i] / s;
    return r;
}

auto SymbolicKVector::dot(const SymbolicKVector& o) const -> cas::Expression {
    assert_same(o);
    cas::Expression s = data_[0] * o.data_[0];
    for (std::size_t i = 1; i < data_.size(); ++i) s = s + data_[i] * o.data_[i];
    return s;
}

auto SymbolicKVector::wedge(const SymbolicKVector& o) const -> SymbolicKVector {
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

auto SymbolicKVector::to_bivector() const -> SymbolicBivector {
    if (grade_ != 2) throw std::domain_error("SymbolicKVector::to_bivector: grade is " + std::to_string(grade_) + ", not 2");
    return { dimension_, std::vector<cas::Expression>(data_) };
}

auto SymbolicKVector::to_trivector() const -> SymbolicTrivector {
    if (grade_ != 3) throw std::domain_error("SymbolicKVector::to_trivector: grade is " + std::to_string(grade_) + ", not 3");
    return { dimension_, std::vector<cas::Expression>(data_) };
}

auto SymbolicKVector::differentiate(const std::string& var) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
    return r;
}

auto SymbolicKVector::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
    return r;
}

auto SymbolicKVector::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
    return r;
}

auto SymbolicKVector::substitute(const std::string& var, double val) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
    return r;
}

auto SymbolicKVector::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
    return r;
}

auto SymbolicKVector::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_, data_);
    for (auto& [n, v] : pairs)
        for (std::size_t i = 0; i < r.data_.size(); ++i)
            r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
    return r;
}

auto SymbolicKVector::partial_evaluate(const std::string& var, double val) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
    return r;
}

auto SymbolicKVector::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
    return r;
}

auto SymbolicKVector::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_, data_);
    for (auto& [n, v] : pairs)
        for (std::size_t i = 0; i < r.data_.size(); ++i)
            r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
    return r;
}

auto SymbolicKVector::simplify() const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
    return r;
}

auto SymbolicKVector::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
    return r;
}

auto SymbolicKVector::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicKVector {
    SymbolicKVector r(grade_, dimension_);
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
    return r;
}

std::vector<double> SymbolicKVector::evaluate(const std::unordered_map<std::string, double>& vals) const {
    std::vector<double> out(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
    return out;
}

std::vector<double> SymbolicKVector::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
    std::vector<double> out(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) out[i] = data_[i].evaluate(vals);
    return out;
}

std::size_t SymbolicKVector::binomial(std::size_t n, std::size_t k) noexcept {
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    std::size_t r = 1;
    for (std::size_t i = 0; i < k; ++i) r = r * (n - i) / (i + 1);
    return r;
}

std::size_t SymbolicKVector::compute_flat_index(const std::vector<std::size_t>& indices, std::size_t N) noexcept {
    std::size_t K = indices.size();
    std::size_t idx = 0;
    for (std::size_t pos = 0; pos < K; ++pos) {
        std::size_t start = (pos == 0) ? 0 : indices[pos - 1] + 1;
        for (std::size_t c = start; c < indices[pos]; ++c) idx += binomial(N - c - 1, K - pos - 1);
    }
    return idx;
}

void SymbolicKVector::assert_same(const SymbolicKVector& o) const {
    if (grade_ != o.grade_ || dimension_ != o.dimension_)
        throw std::invalid_argument("SymbolicKVector: grade/dimension mismatch ("
            + std::to_string(grade_) + "," + std::to_string(dimension_) + " vs "
            + std::to_string(o.grade_) + "," + std::to_string(o.dimension_) + ")");
}

void SymbolicKVector::enumerate_blades_impl(std::size_t K, std::size_t N, std::size_t start, std::vector<std::size_t>& current, std::vector<std::vector<std::size_t>>& result) {
    if (current.size() == K) { result.push_back(current); return; }
    std::size_t remaining = K - current.size();
    for (std::size_t i = start; i + remaining <= N; ++i) {
        current.push_back(i);
        enumerate_blades_impl(K, N, i + 1, current, result);
        current.pop_back();
    }
}

std::vector<std::vector<std::size_t>> SymbolicKVector::enumerate_blades(std::size_t K, std::size_t N) {
    std::vector<std::vector<std::size_t>> result;
    result.reserve(binomial(N, K));
    std::vector<std::size_t> current;
    current.reserve(K);
    enumerate_blades_impl(K, N, 0, current, result);
    return result;
}

int SymbolicKVector::merge_sign(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, std::vector<std::size_t>& merged) {
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

SymbolicBivector SymbolicVector4::wedge(const SymbolicVector4& o) const {
    return SymbolicBivector(4, {
        x * o.y - y * o.x,   // e01
        x * o.z - z * o.x,   // e02
        x * o.w - w * o.x,   // e03
        y * o.z - z * o.y,   // e12
        y * o.w - w * o.y,   // e13
        z * o.w - w * o.z    // e23
    });
}

SymbolicBivector SymbolicVectorN::wedge(const SymbolicVectorN& o) const {
    assert_same_dim(o);
    std::size_t N = dimension();
    auto result = SymbolicBivector::zero(N);
    std::size_t idx = 0;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            result[idx++] = data_[i] * o.data_[j] - data_[j] * o.data_[i];
    return result;
}

} // namespace vectors
} // namespace math
} // namespace fizmo
