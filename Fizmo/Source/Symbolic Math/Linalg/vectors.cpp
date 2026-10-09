#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace vectors {

auto SymbolicVector2::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVector2 {
        auto sx = x, sy = y;
        for (auto& [n, v] : pairs) { sx = SUBSTITUTE(sx, n, v); sy = SUBSTITUTE(sy, n, v); }
        return { sx, sy };
    }

auto SymbolicVector2::substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const -> SymbolicVector2 {
        auto sx = x, sy = y;
        for (auto& [n, v] : pairs) { sx = SUBSTITUTE(sx, n, v); sy = SUBSTITUTE(sy, n, v); }
        return { sx, sy };
    }

auto SymbolicVector2::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVector2 {
        auto sx = x, sy = y;
        for (auto& [n, v] : pairs) { sx = PARTIAL_EVALUATE(sx, n, v); sy = PARTIAL_EVALUATE(sy, n, v); }
        return { sx, sy };
    }

auto SymbolicVector2::partial_evaluate(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const -> SymbolicVector2 {
        auto sx = x, sy = y;
        for (auto& [n, v] : pairs) { sx = PARTIAL_EVALUATE(sx, n, v); sy = PARTIAL_EVALUATE(sy, n, v); }
        return { sx, sy };
    }

auto SymbolicVector3::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicVector3 { return { DIFFERENTIATE_ITERATIVE(x, var, n), DIFFERENTIATE_ITERATIVE(y, var, n), DIFFERENTIATE_ITERATIVE(z, var, n) }; }

auto SymbolicVector3::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicVector3 { return { DIFFERENTIATE_RECURSIVE(x, var, n), DIFFERENTIATE_RECURSIVE(y, var, n), DIFFERENTIATE_RECURSIVE(z, var, n) }; }

auto SymbolicVector3::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVector3 {
        auto sx = x, sy = y, sz = z;
        for (auto& [n, v] : pairs) { sx = SUBSTITUTE(sx, n, v); sy = SUBSTITUTE(sy, n, v); sz = SUBSTITUTE(sz, n, v); }
        return { sx, sy, sz };
    }

auto SymbolicVector3::substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const -> SymbolicVector3 {
        auto sx = x, sy = y, sz = z;
        for (auto& [n, v] : pairs) { sx = SUBSTITUTE(sx, n, v); sy = SUBSTITUTE(sy, n, v); sz = SUBSTITUTE(sz, n, v); }
        return { sx, sy, sz };
    }

auto SymbolicVector3::partial_evaluate(const std::string& var, double val) const -> SymbolicVector3 { return { PARTIAL_EVALUATE(x, var, val), PARTIAL_EVALUATE(y, var, val), PARTIAL_EVALUATE(z, var, val) }; }

auto SymbolicVector3::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVector3 {
        auto sx = x, sy = y, sz = z;
        for (auto& [n, v] : pairs) { sx = PARTIAL_EVALUATE(sx, n, v); sy = PARTIAL_EVALUATE(sy, n, v); sz = PARTIAL_EVALUATE(sz, n, v); }
        return { sx, sy, sz };
    }

auto SymbolicVector3::partial_evaluate(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const -> SymbolicVector3 {
        auto sx = x, sy = y, sz = z;
        for (auto& [n, v] : pairs) { sx = PARTIAL_EVALUATE(sx, n, v); sy = PARTIAL_EVALUATE(sy, n, v); sz = PARTIAL_EVALUATE(sz, n, v); }
        return { sx, sy, sz };
    }

auto SymbolicVector4::operator[](std::size_t i) -> cas::Expression& {
        switch (i) { case 0: return x; case 1: return y; case 2: return z; case 3: return w;
                     default: throw std::out_of_range("SymbolicVector4: index " + std::to_string(i)); }
    }

auto SymbolicVector4::operator[](std::size_t i) const -> const cas::Expression& {
        switch (i) { case 0: return x; case 1: return y; case 2: return z; case 3: return w;
                     default: throw std::out_of_range("SymbolicVector4: index " + std::to_string(i)); }
    }

auto SymbolicVector4::symbolic_equals(const SymbolicVector4& o) const -> bool {
        return x.symbolic_equals(o.x) && y.symbolic_equals(o.y) &&
               z.symbolic_equals(o.z) && w.symbolic_equals(o.w);
    }

auto SymbolicVector4::differentiate(const std::string& var) const -> SymbolicVector4 {
        return { DIFFERENTIATE(x, var), DIFFERENTIATE(y, var),
                 DIFFERENTIATE(z, var), DIFFERENTIATE(w, var) };
    }

auto SymbolicVector4::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicVector4 {
        return { DIFFERENTIATE_ITERATIVE(x, var, n), DIFFERENTIATE_ITERATIVE(y, var, n),
                 DIFFERENTIATE_ITERATIVE(z, var, n), DIFFERENTIATE_ITERATIVE(w, var, n) };
    }

auto SymbolicVector4::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicVector4 {
        return { DIFFERENTIATE_RECURSIVE(x, var, n), DIFFERENTIATE_RECURSIVE(y, var, n),
                 DIFFERENTIATE_RECURSIVE(z, var, n), DIFFERENTIATE_RECURSIVE(w, var, n) };
    }

auto SymbolicVector4::substitute(const std::string& var, double val) const -> SymbolicVector4 {
        return { SUBSTITUTE(x, var, val), SUBSTITUTE(y, var, val),
                 SUBSTITUTE(z, var, val), SUBSTITUTE(w, var, val) };
    }

auto SymbolicVector4::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicVector4 {
        return { SUBSTITUTE(x, var, repl), SUBSTITUTE(y, var, repl),
                 SUBSTITUTE(z, var, repl), SUBSTITUTE(w, var, repl) };
    }

auto SymbolicVector4::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVector4 {
        SymbolicVector4 r = *this;

        for (auto& [n, v] : pairs) {
            r.x = SUBSTITUTE(r.x, n, v); r.y = SUBSTITUTE(r.y, n, v);
            r.z = SUBSTITUTE(r.z, n, v); r.w = SUBSTITUTE(r.w, n, v);
        }

        return r;
    }

auto SymbolicVector4::substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const -> SymbolicVector4 {
        SymbolicVector4 r = *this;

        for (auto& [n, v] : pairs) {
            r.x = SUBSTITUTE(r.x, n, v); r.y = SUBSTITUTE(r.y, n, v);
            r.z = SUBSTITUTE(r.z, n, v); r.w = SUBSTITUTE(r.w, n, v);
        }

        return r;
    }

auto SymbolicVector4::partial_evaluate(const std::string& var, double val) const -> SymbolicVector4 {
        return { PARTIAL_EVALUATE(x, var, val), PARTIAL_EVALUATE(y, var, val),
                 PARTIAL_EVALUATE(z, var, val), PARTIAL_EVALUATE(w, var, val) };
    }

auto SymbolicVector4::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicVector4 {
        return { PARTIAL_EVALUATE(x, var, repl), PARTIAL_EVALUATE(y, var, repl),
                 PARTIAL_EVALUATE(z, var, repl), PARTIAL_EVALUATE(w, var, repl) };
    }

auto SymbolicVector4::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVector4 {
        SymbolicVector4 r = *this;

        for (auto& [n, v] : pairs) {
            r.x = PARTIAL_EVALUATE(r.x, n, v); r.y = PARTIAL_EVALUATE(r.y, n, v);
            r.z = PARTIAL_EVALUATE(r.z, n, v); r.w = PARTIAL_EVALUATE(r.w, n, v);
        }

        return r;
    }

auto SymbolicVector4::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicVector4 {
        return { FULL_SIMPLIFY(x, cfg), FULL_SIMPLIFY(y, cfg),
                 FULL_SIMPLIFY(z, cfg), FULL_SIMPLIFY(w, cfg) };
    }

auto SymbolicVectorN::constant(const std::vector<double>& vals) -> SymbolicVectorN {
        SymbolicVectorN v(vals.size());
        for (std::size_t i = 0; i < vals.size(); ++i) v.data_[i] = cas::Const(vals[i]);
        return v;
    }

auto SymbolicVectorN::operator+(const SymbolicVectorN& o) const -> SymbolicVectorN {
        assert_same_dim(o);
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] + o.data_[i];
        return r;
    }

auto SymbolicVectorN::operator-(const SymbolicVectorN& o) const -> SymbolicVectorN {
        assert_same_dim(o);
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] - o.data_[i];
        return r;
    }

auto SymbolicVectorN::operator-() const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = -data_[i];
        return r;
    }

auto SymbolicVectorN::operator*(const cas::Expression& s) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }

auto SymbolicVectorN::operator*(double s) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] * s;
        return r;
    }

auto SymbolicVectorN::operator/(const cas::Expression& s) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }

auto SymbolicVectorN::operator/(double s) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] / s;
        return r;
    }

auto SymbolicVectorN::dot(const SymbolicVectorN& o) const -> cas::Expression {
        assert_same_dim(o);
        cas::Expression s = data_[0] * o.data_[0];
        for (std::size_t i = 1; i < dimension(); ++i) s = s + data_[i] * o.data_[i];
        return s;
    }

auto SymbolicVectorN::hadamard(const SymbolicVectorN& o) const -> SymbolicVectorN {
        assert_same_dim(o);
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = data_[i] * o.data_[i];
        return r;
    }

auto SymbolicVectorN::cross(const SymbolicVectorN& o) const -> SymbolicVectorN {
        if (dimension() == 3 && o.dimension() == 3) {
            return SymbolicVectorN({
                data_[1] * o.data_[2] - data_[2] * o.data_[1],
                data_[2] * o.data_[0] - data_[0] * o.data_[2],
                data_[0] * o.data_[1] - data_[1] * o.data_[0]
            });
        }
        if (dimension() == 7 && o.dimension() == 7) {
            const auto& a = data_;
            const auto& b = o.data_;
            return SymbolicVectorN({
                /* e0 */ a[1]*b[3] - a[3]*b[1] + a[2]*b[6] - a[6]*b[2] + a[4]*b[5] - a[5]*b[4],
                /* e1 */ a[2]*b[4] - a[4]*b[2] + a[3]*b[0] - a[0]*b[3] + a[5]*b[6] - a[6]*b[5],
                /* e2 */ a[3]*b[5] - a[5]*b[3] + a[4]*b[1] - a[1]*b[4] + a[6]*b[0] - a[0]*b[6],
                /* e3 */ a[4]*b[6] - a[6]*b[4] + a[5]*b[2] - a[2]*b[5] + a[0]*b[1] - a[1]*b[0],
                /* e4 */ a[5]*b[0] - a[0]*b[5] + a[6]*b[3] - a[3]*b[6] + a[1]*b[2] - a[2]*b[1],
                /* e5 */ a[6]*b[1] - a[1]*b[6] + a[0]*b[4] - a[4]*b[0] + a[2]*b[3] - a[3]*b[2],
                /* e6 */ a[0]*b[2] - a[2]*b[0] + a[1]*b[5] - a[5]*b[1] + a[3]*b[4] - a[4]*b[3]
            });
        }
        throw std::domain_error("SymbolicVectorN::cross requires dimension 3 or 7");
    }

auto SymbolicVectorN::differentiate(const std::string& var) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
        return r;
    }

auto SymbolicVectorN::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
        return r;
    }

auto SymbolicVectorN::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
        return r;
    }

auto SymbolicVectorN::substitute(const std::string& var, double val) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
        return r;
    }

auto SymbolicVectorN::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
        return r;
    }

auto SymbolicVectorN::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVectorN {
        SymbolicVectorN r(data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = SUBSTITUTE(r.data_[i], n, v);
        return r;
    }

auto SymbolicVectorN::partial_evaluate(const std::string& var, double val) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
        return r;
    }

auto SymbolicVectorN::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
        return r;
    }

auto SymbolicVectorN::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicVectorN {
        SymbolicVectorN r(data_);
        for (auto& [n, v] : pairs)
            for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = PARTIAL_EVALUATE(r.data_[i], n, v);
        return r;
    }

auto SymbolicVectorN::simplify() const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
        return r;
    }

auto SymbolicVectorN::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
        return r;
    }

auto SymbolicVectorN::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicVectorN {
        SymbolicVectorN r(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
        return r;
    }

auto SymbolicVectorN::evaluate(const std::unordered_map<std::string, double>& vals) const -> std::vector<double> {
        std::vector<double> out(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }

auto SymbolicVectorN::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const -> std::vector<double> {
        std::vector<double> out(dimension());
        for (std::size_t i = 0; i < dimension(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }

auto SymbolicVectorN::assert_same_dim(const SymbolicVectorN& o) const -> void {
        if (dimension() != o.dimension()) throw std::invalid_argument("SymbolicVectorN: dimension mismatch (" + std::to_string(dimension()) + " vs " + std::to_string(o.dimension()) + ")");
    }

} // namespace vectors
} // namespace math
} // namespace fizmo
