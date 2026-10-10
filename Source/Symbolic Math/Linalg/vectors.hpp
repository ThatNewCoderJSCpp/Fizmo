#ifndef FIZMO_SYMBOLIC_VECTORS_MATRICES_HPP
#define FIZMO_SYMBOLIC_VECTORS_MATRICES_HPP

#include <array>
#include <string>
#include <sstream>
#include <ostream>
#include <unordered_map>
#include <initializer_list>
#include <utility>

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../Common/main_convenience.hpp"
#include "../../Vectors/vectors.hpp"
#include "../../Matrices/square_matrices.hpp"

namespace fizmo {
namespace math {
namespace vectors {

class SymbolicBivector;
class SymbolicTrivector;
class SymbolicKVector;
class SymbolicVector3;

struct SymbolicVector2 {
    cas::Expression x, y;
    SymbolicVector2() = default;
    SymbolicVector2(cas::Expression x_, cas::Expression y_) : x(std::move(x_)), y(std::move(y_)) {}
    SymbolicVector2(const std::string& xn, const std::string& yn) : x(cas::VARIABLE(xn)), y(cas::VARIABLE(yn)) {}
    static SymbolicVector2 constant(double cx, double cy) { return { cas::Const(cx), cas::Const(cy) }; }
    static SymbolicVector2 zero() { return constant(0.0, 0.0); }
    SymbolicVector2 operator+(const SymbolicVector2& v) const { return { x + v.x, y + v.y }; }
    SymbolicVector2 operator-(const SymbolicVector2& v) const { return { x - v.x, y - v.y }; }
    SymbolicVector2 operator-()                         const { return { -x, -y }; }
    SymbolicVector2 operator*(const cas::Expression& s) const { return { x * s, y * s }; }
    SymbolicVector2 operator*(double s)                 const { return { x * s, y * s }; }
    SymbolicVector2 operator/(const cas::Expression& s) const { return { x / s, y / s }; }
    SymbolicVector2 operator/(double s)                 const { return { x / s, y / s }; }
    SymbolicVector2& operator+=(const SymbolicVector2& v) { x = x + v.x; y = y + v.y; return *this; }
    SymbolicVector2& operator-=(const SymbolicVector2& v) { x = x - v.x; y = y - v.y; return *this; }
    SymbolicVector2& operator*=(const cas::Expression& s) { x = x * s;   y = y * s;   return *this; }
    SymbolicVector2& operator*=(double s)                 { x = x * s;   y = y * s;   return *this; }
    SymbolicVector2& operator/=(const cas::Expression& s) { x = x / s;   y = y / s;   return *this; }
    SymbolicVector2& operator/=(double s)                 { x = x / s;   y = y / s;   return *this; }
    cas::Expression dot(const SymbolicVector2& v)   const { return x * v.x + y * v.y; }
    SymbolicVector3 cross(const SymbolicVector2& v) const;
    SymbolicBivector wedge(const SymbolicVector2& v) const;
    SymbolicKVector  wedge(const SymbolicBivector& o) const;
    SymbolicKVector  wedge(const SymbolicTrivector& o) const;
    SymbolicKVector  wedge(const SymbolicKVector& o) const;
    cas::Expression magnitude_squared()             const { return x * x + y * y; }
    cas::Expression magnitude()                     const { return SQRT(magnitude_squared()); }
    cas::Expression length()                        const { return magnitude(); }
    SymbolicVector2  hadamard(const SymbolicVector2& v) const { return { x * v.x, y * v.y }; }
    SymbolicVector2  unit_vector()                      const { auto m = magnitude(); return { x / m, y / m }; }
    SymbolicVector2  normalized()                       const { return unit_vector(); }
    SymbolicVector2  direction()                        const { return unit_vector(); }
    SymbolicVector2& normalize()                              { auto m = magnitude(); x = x / m; y = y / m; return *this; }
    cas::Expression      scalar_project(const SymbolicVector2& onto) const { return dot(onto) / onto.magnitude(); }
    SymbolicVector2 project_onto(const SymbolicVector2& onto)   const { auto s = dot(onto) / onto.magnitude_squared(); return { onto.x * s, onto.y * s }; }
    SymbolicVector2 reject_from(const SymbolicVector2& onto)    const { return *this - project_onto(onto); }
    SymbolicVector2 differentiate(const std::string& var)                       const { return { DIFFERENTIATE(x, var),            DIFFERENTIATE(y, var)            }; }
    SymbolicVector2 differentiate_iterative(const std::string& var, unsigned n) const { return { DIFFERENTIATE_ITERATIVE(x, var, n), DIFFERENTIATE_ITERATIVE(y, var, n) }; }
    SymbolicVector2 differentiate_recursive(const std::string& var, unsigned n) const { return { DIFFERENTIATE_RECURSIVE(x, var, n), DIFFERENTIATE_RECURSIVE(y, var, n) }; }
    SymbolicVector2 substitute(const std::string& var, double val)           const { return { SUBSTITUTE(x, var, val),  SUBSTITUTE(y, var, val)  }; }
    SymbolicVector2 substitute(const std::string& var, const cas::Expression& r)  const { return { SUBSTITUTE(x, var, r),    SUBSTITUTE(y, var, r)    }; }
    SymbolicVector2 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicVector2 substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const;
    SymbolicVector2 partial_evaluate(const std::string& var, double val)          const { return { PARTIAL_EVALUATE(x, var, val),  PARTIAL_EVALUATE(y, var, val)  }; }
    SymbolicVector2 partial_evaluate(const std::string& var, const cas::Expression& r) const { return { PARTIAL_EVALUATE(x, var, r),    PARTIAL_EVALUATE(y, var, r)    }; }
    SymbolicVector2 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicVector2 partial_evaluate(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const;
    SymbolicVector2 rewrite(const cas::RewriterConfig& cfg = {}) const { return { REWRITE(x, cfg),       REWRITE(y, cfg)       }; }
    SymbolicVector2 simplify()                                                        const { return { SIMPLIFY(x),           SIMPLIFY(y)           }; }
    SymbolicVector2 full_simplify(const cas::RewriterConfig& cfg = {}) const { return { FULL_SIMPLIFY(x, cfg), FULL_SIMPLIFY(y, cfg) }; }
    Vector2D<double, double> evaluate(const std::unordered_map<std::string, double>& vals) const {
        return { x.evaluate(vals), y.evaluate(vals) };
    }
    Vector2D<double, double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        return { x.evaluate(vals), y.evaluate(vals) };
    }
    friend std::ostream& operator<<(std::ostream& os, const SymbolicVector2& v) {
        os << "<\n    " << v.x.full_simplify() << ",\n    " << v.y.full_simplify() << "\n>";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }
};

inline SymbolicVector2 operator*(const cas::Expression& s, const SymbolicVector2& v) { return v * s; }
inline SymbolicVector2 operator*(double s,            const SymbolicVector2& v) { return v * s; }

struct SymbolicVector3 {
    cas::Expression x, y, z;
    SymbolicVector3() = default;
    SymbolicVector3(cas::Expression x_, cas::Expression y_, cas::Expression z_) : x(std::move(x_)), y(std::move(y_)), z(std::move(z_)) {}
    SymbolicVector3(const std::string& xn, const std::string& yn, const std::string& zn) : x(cas::VARIABLE(xn)), y(cas::VARIABLE(yn)), z(cas::VARIABLE(zn)) {}
    static SymbolicVector3 constant(double cx, double cy, double cz) { return { cas::Const(cx), cas::Const(cy), cas::Const(cz) }; }
    static SymbolicVector3 zero() { return constant(0.0, 0.0, 0.0); }
    SymbolicVector3 operator+(const SymbolicVector3& v)  const { return { x + v.x, y + v.y, z + v.z }; }
    SymbolicVector3 operator-(const SymbolicVector3& v)  const { return { x - v.x, y - v.y, z - v.z }; }
    SymbolicVector3 operator-()                          const { return { -x, -y, -z }; }
    SymbolicVector3 operator*(const cas::Expression& s)  const { return { x * s, y * s, z * s }; }
    SymbolicVector3 operator*(double s)                  const { return { x * s, y * s, z * s }; }
    SymbolicVector3 operator/(const cas::Expression& s)  const { return { x / s, y / s, z / s }; }
    SymbolicVector3 operator/(double s)                  const { return { x / s, y / s, z / s }; }
    SymbolicVector3& operator+=(const SymbolicVector3& v) { x = x + v.x; y = y + v.y; z = z + v.z;      return *this; }
    SymbolicVector3& operator-=(const SymbolicVector3& v) { x = x - v.x; y = y - v.y; z = z - v.z;      return *this; }
    SymbolicVector3& operator*=(const cas::Expression& s)      { x = x * s;   y = y * s;   z = z * s;   return *this; }
    SymbolicVector3& operator*=(double s)                 { x = x * s;   y = y * s;   z = z * s;        return *this; }
    SymbolicVector3& operator/=(const cas::Expression& s)      { x = x / s;   y = y / s;   z = z / s;   return *this; }
    SymbolicVector3& operator/=(double s)                 { x = x / s;   y = y / s;   z = z / s;        return *this; }
    cas::Expression      dot(const SymbolicVector3& v) const { return x * v.x + y * v.y + z * v.z; }
    SymbolicVector3 cross(const SymbolicVector3& v)    const { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x }; }
    SymbolicBivector wedge(const SymbolicVector3& v)   const;
    SymbolicKVector  wedge(const SymbolicBivector& o)  const;
    SymbolicKVector  wedge(const SymbolicTrivector& o) const;
    SymbolicKVector  wedge(const SymbolicKVector& o) const;
    cas::Expression      magnitude_squared()           const { return x * x + y * y + z * z; }
    cas::Expression      magnitude()                   const { return SQRT(magnitude_squared()); }
    cas::Expression      length()                      const { return magnitude(); }
    SymbolicVector3 hadamard(const SymbolicVector3& v) const { return { x * v.x, y * v.y, z * v.z }; }
    SymbolicVector3& normalize()                             { auto m = magnitude(); x = x / m; y = y / m; z = z / m; return *this; }
    SymbolicVector3 unit_vector()                      const { auto m = magnitude(); return { x / m, y / m, z / m }; }
    SymbolicVector3 normalized()                       const { return unit_vector(); }
    SymbolicVector3 direction()                        const { return unit_vector(); }
    cas::Expression scalar_project(const SymbolicVector3& onto) const { return dot(onto) / onto.magnitude(); }
    SymbolicVector3 project_onto(const SymbolicVector3& onto)   const { auto s = dot(onto) / onto.magnitude_squared(); return { onto.x * s, onto.y * s, onto.z * s }; }
    SymbolicVector3 reject_from(const SymbolicVector3& onto)    const { return *this - project_onto(onto); }
    SymbolicVector3 reflect(const SymbolicVector3& n)           const { auto s = dot(n) * 2.0; return *this - n * s; }
    SymbolicVector3 differentiate(const std::string& var)       const { return { DIFFERENTIATE(x, var), DIFFERENTIATE(y, var), DIFFERENTIATE(z, var) }; }
    SymbolicVector3 differentiate_iterative(const std::string& var, unsigned n)  const;
    SymbolicVector3 differentiate_recursive(const std::string& var, unsigned n)  const;
    SymbolicVector3 substitute(const std::string& var, double val)               const { return { SUBSTITUTE(x, var, val), SUBSTITUTE(y, var, val), SUBSTITUTE(z, var, val) }; }
    SymbolicVector3 substitute(const std::string& var, const cas::Expression& r) const { return { SUBSTITUTE(x, var, r),   SUBSTITUTE(y, var, r),   SUBSTITUTE(z, var, r)   }; }
    SymbolicVector3 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicVector3 substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const;
    SymbolicVector3 partial_evaluate(const std::string& var, double val)                          const;
    SymbolicVector3 partial_evaluate(const std::string& var, const cas::Expression& r)            const { return { PARTIAL_EVALUATE(x, var, r),   PARTIAL_EVALUATE(y, var, r),   PARTIAL_EVALUATE(z, var, r)   }; }
    SymbolicVector3 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicVector3 partial_evaluate(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const;
    SymbolicVector3 rewrite(const cas::RewriterConfig& cfg = {})      const { return { REWRITE(x, cfg),       REWRITE(y, cfg),       REWRITE(z, cfg)       }; }
    SymbolicVector3 simplify()                                                             const { return { SIMPLIFY(x),           SIMPLIFY(y),           SIMPLIFY(z)           }; }
    SymbolicVector3 full_simplify(const cas::RewriterConfig& cfg = {}) const { return { FULL_SIMPLIFY(x, cfg), FULL_SIMPLIFY(y, cfg), FULL_SIMPLIFY(z, cfg) }; }
    Vector3D<double, double> evaluate(const std::unordered_map<std::string, double>& vals) const {
        return { x.evaluate(vals), y.evaluate(vals), z.evaluate(vals) };
    }
    Vector3D<double, double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        return { x.evaluate(vals), y.evaluate(vals), z.evaluate(vals) };
    }
    friend std::ostream& operator<<(std::ostream& os, const SymbolicVector3& v) {
        os << "<\n    " << v.x.full_simplify() << ",\n    " << v.y.full_simplify() << ",\n    " << v.z.full_simplify() << "\n>";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }
};

inline SymbolicVector3 operator*(const cas::Expression& s, const SymbolicVector3& v) { return v * s; }
inline SymbolicVector3 operator*(double s,                 const SymbolicVector3& v) { return v * s; }

inline SymbolicVector3 SymbolicVector2::cross(const SymbolicVector2& v) const { return SymbolicVector3(cas::Const(0), cas::Const(0), x * v.y - y * v.x); }

struct SymbolicVector4 {
    cas::Expression x, y, z, w;

    SymbolicVector4() = default;
    SymbolicVector4(cas::Expression x_, cas::Expression y_, cas::Expression z_, cas::Expression w_) : x(std::move(x_)), y(std::move(y_)), z(std::move(z_)), w(std::move(w_)) {}

    static SymbolicVector4 zero() {
        return { cas::Const(0.0), cas::Const(0.0), cas::Const(0.0), cas::Const(0.0) };
    }

    static SymbolicVector4 basis_x() { return { cas::Const(1.0), cas::Const(0.0), cas::Const(0.0), cas::Const(0.0) }; }
    static SymbolicVector4 basis_y() { return { cas::Const(0.0), cas::Const(1.0), cas::Const(0.0), cas::Const(0.0) }; }
    static SymbolicVector4 basis_z() { return { cas::Const(0.0), cas::Const(0.0), cas::Const(1.0), cas::Const(0.0) }; }
    static SymbolicVector4 basis_w() { return { cas::Const(0.0), cas::Const(0.0), cas::Const(0.0), cas::Const(1.0) }; }

    cas::Expression& operator[](std::size_t i);

    const cas::Expression& operator[](std::size_t i) const;

    static constexpr std::size_t dimension() noexcept { return 4; }

    SymbolicVector4 operator+(const SymbolicVector4& o) const { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
    SymbolicVector4 operator-(const SymbolicVector4& o) const { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
    SymbolicVector4 operator-()                         const { return { -x, -y, -z, -w }; }
    SymbolicVector4 operator*(const cas::Expression& s) const { return { x * s, y * s, z * s, w * s }; }
    SymbolicVector4 operator*(double s)                 const { return { x * s, y * s, z * s, w * s }; }
    SymbolicVector4 operator/(const cas::Expression& s) const { return { x / s, y / s, z / s, w / s }; }
    SymbolicVector4 operator/(double s)                 const { return { x / s, y / s, z / s, w / s }; }

    SymbolicVector4& operator+=(const SymbolicVector4& o) { return *this = *this + o; }
    SymbolicVector4& operator-=(const SymbolicVector4& o) { return *this = *this - o; }
    SymbolicVector4& operator*=(const cas::Expression& s) { return *this = *this * s; }
    SymbolicVector4& operator*=(double s)                 { return *this = *this * s; }
    SymbolicVector4& operator/=(const cas::Expression& s) { return *this = *this / s; }
    SymbolicVector4& operator/=(double s)                 { return *this = *this / s; }


    cas::Expression dot(const SymbolicVector4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    cas::Expression magnitude_squared()           const { return dot(*this); }
    cas::Expression magnitude()                   const { return SQRT(magnitude_squared()); }
    cas::Expression norm()                        const { return magnitude(); }

    SymbolicVector4 normalized()  const { auto m = magnitude(); return { x / m, y / m, z / m, w / m }; }
    SymbolicVector4 unit_vector() const { return normalized(); }

    bool symbolic_equals(const SymbolicVector4& o) const;

    SymbolicVector4 differentiate(const std::string& var) const;

    SymbolicVector4 differentiate_iterative(const std::string& var, unsigned n) const;

    SymbolicVector4 differentiate_recursive(const std::string& var, unsigned n) const;

    SymbolicVector4 substitute(const std::string& var, double val) const;

    SymbolicVector4 substitute(const std::string& var, const cas::Expression& repl) const;

    SymbolicVector4 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;

    SymbolicVector4 substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const;

    SymbolicVector4 partial_evaluate(const std::string& var, double val) const;

    SymbolicVector4 partial_evaluate(const std::string& var, const cas::Expression& repl) const;

    SymbolicVector4 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;

    SymbolicVector4 simplify() const {
        return { SIMPLIFY(x), SIMPLIFY(y), SIMPLIFY(z), SIMPLIFY(w) };
    }

    SymbolicVector4 full_simplify(const cas::RewriterConfig& cfg = {}) const;

    SymbolicVector4 rewrite(const cas::RewriterConfig& cfg = {}) const {
        return { REWRITE(x, cfg), REWRITE(y, cfg), REWRITE(z, cfg), REWRITE(w, cfg) };
    }

    vector4d evaluate(const std::unordered_map<std::string, double>& vals) const {
        return { x.evaluate(vals), y.evaluate(vals), z.evaluate(vals), w.evaluate(vals) };
    }

    vector4d evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        return { x.evaluate(vals), y.evaluate(vals), z.evaluate(vals), w.evaluate(vals) };
    }

    SymbolicBivector wedge(const SymbolicVector4& o)  const;
    SymbolicKVector wedge(const SymbolicBivector& o)  const;
    SymbolicKVector wedge(const SymbolicTrivector& o) const;
    SymbolicKVector wedge(const SymbolicKVector& o)   const;

    friend std::ostream& operator<<(std::ostream& os, const SymbolicVector4& v) {
        os << "<" << v.x.full_simplify() << ", " << v.y.full_simplify()
           << ", " << v.z.full_simplify() << ", " << v.w.full_simplify() << ">";
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }
};

inline SymbolicVector4 operator*(const cas::Expression& s, const SymbolicVector4& v) { return v * s; }
inline SymbolicVector4 operator*(double s,                 const SymbolicVector4& v) { return v * s; }

class SymbolicVectorN {
public:
    SymbolicVectorN() = default;
    explicit SymbolicVectorN(std::size_t n) : data_(n) {}
    explicit SymbolicVectorN(std::vector<cas::Expression> components) : data_(std::move(components)) {}
    SymbolicVectorN(std::initializer_list<cas::Expression> il) : data_(il) {}
    explicit SymbolicVectorN(const std::vector<std::string>& names) {
        data_.reserve(names.size());
        for (auto& n : names) data_.push_back(cas::VARIABLE(n));
    }
    static SymbolicVectorN constant(const std::vector<double>& vals);
    static SymbolicVectorN zero(std::size_t n) {
        SymbolicVectorN v(n);
        for (std::size_t i = 0; i < n; ++i) v.data_[i] = cas::Const(0.0);
        return v;
    }
    static SymbolicVectorN basis(std::size_t n, std::size_t axis) {
        auto v = zero(n);
        if (axis < n) v.data_[axis] = cas::Const(1.0);
        return v;
    }
    std::size_t dimension() const noexcept { return data_.size(); }
    cas::Expression&       operator[](std::size_t i)       { return data_[i]; }
    const cas::Expression& operator[](std::size_t i) const { return data_[i]; }
    cas::Expression&       at(std::size_t i)       { return data_.at(i); }
    const cas::Expression& at(std::size_t i) const { return data_.at(i); }
    const std::vector<cas::Expression>& components() const noexcept { return data_; }
    std::vector<cas::Expression>&       components()       noexcept { return data_; }
    SymbolicVectorN operator+(const SymbolicVectorN& o) const;
    SymbolicVectorN operator-(const SymbolicVectorN& o) const;
    SymbolicVectorN operator-() const;
    SymbolicVectorN operator*(const cas::Expression& s) const;
    SymbolicVectorN operator*(double s) const;
    SymbolicVectorN operator/(const cas::Expression& s) const;
    SymbolicVectorN operator/(double s) const;
    SymbolicVectorN& operator+=(const SymbolicVectorN& o) { return *this = *this + o; }
    SymbolicVectorN& operator-=(const SymbolicVectorN& o) { return *this = *this - o; }
    SymbolicVectorN& operator*=(const cas::Expression& s)      { return *this = *this * s; }
    SymbolicVectorN& operator*=(double s)                  { return *this = *this * s; }
    SymbolicVectorN& operator/=(const cas::Expression& s)      { return *this = *this / s; }
    SymbolicVectorN& operator/=(double s)                  { return *this = *this / s; }
    cas::Expression dot(const SymbolicVectorN& o) const;
    cas::Expression magnitude_squared() const { return dot(*this); }
    cas::Expression magnitude()         const { return SQRT(magnitude_squared()); }
    cas::Expression length()            const { return magnitude(); }
    SymbolicVectorN hadamard(const SymbolicVectorN& o) const;
    SymbolicVectorN& normalize()        { auto m = magnitude(); *this = *this / m; return *this; }
    SymbolicVectorN unit_vector() const { auto m = magnitude(); return *this / m; }
    SymbolicVectorN normalized()  const { return unit_vector(); }
    SymbolicVectorN direction()   const { return unit_vector(); }
    cas::Expression scalar_project(const SymbolicVectorN& onto) const { return dot(onto) / onto.magnitude(); }
    SymbolicVectorN project_onto(const SymbolicVectorN& onto) const {
        auto s = dot(onto) / onto.magnitude_squared();
        return onto * s;
    }
    SymbolicVectorN project_from(const SymbolicVectorN& onto) const { return *this - project_onto(onto); }
    SymbolicVectorN reflect(const SymbolicVectorN& n) const { return *this - n * (dot(n) * 2.0); }
    SymbolicBivector wedge(const SymbolicVectorN& o) const;
    SymbolicKVector  wedge(const SymbolicBivector& o) const;
    SymbolicKVector  wedge(const SymbolicTrivector& o) const;
    SymbolicKVector  wedge(const SymbolicKVector& o) const;
    SymbolicVectorN cross(const SymbolicVectorN& o) const;
    SymbolicVectorN differentiate(const std::string& var) const;
    SymbolicVectorN differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicVectorN differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicVectorN substitute(const std::string& var, double val) const;
    SymbolicVectorN substitute(const std::string& var, const cas::Expression& repl) const;
    SymbolicVectorN substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicVectorN partial_evaluate(const std::string& var, double val) const;
    SymbolicVectorN partial_evaluate(const std::string& var, const cas::Expression& repl) const;
    SymbolicVectorN partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicVectorN simplify() const;
    SymbolicVectorN full_simplify(const cas::RewriterConfig& cfg = {}) const;
    SymbolicVectorN rewrite(const cas::RewriterConfig& cfg = {}) const;
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals) const;
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;
    static SymbolicVectorN from(const SymbolicVector2& v) { return SymbolicVectorN({ v.x, v.y }); }
    static SymbolicVectorN from(const SymbolicVector3& v) { return SymbolicVectorN({ v.x, v.y, v.z }); }
    SymbolicVector2 to_2d() const { assert(dimension() >= 2); return { data_[0], data_[1] }; }
    SymbolicVector3 to_3d() const { assert(dimension() >= 3); return { data_[0], data_[1], data_[2] }; }
    SymbolicVector4 to_4d() const { assert(dimension() >= 4); return { data_[0], data_[1], data_[2], data_[3] }; }
    friend std::ostream& operator<<(std::ostream& os, const SymbolicVectorN& v) {
        os << "<";
        for (std::size_t i = 0; i < v.dimension(); ++i) {
            os << "\n    " << v.data_[i].full_simplify();
            if (i + 1 < v.dimension()) os << ",";
        }
        os << "\n>";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    std::vector<cas::Expression> data_;

    void assert_same_dim(const SymbolicVectorN& o) const;
};

inline SymbolicVectorN operator*(const cas::Expression& s, const SymbolicVectorN& v) { return v * s; }
inline SymbolicVectorN operator*(double s,                 const SymbolicVectorN& v) { return v * s; }

} // namespace vectors
} // namespace math

template <typename T> struct is_symbolic_vector : std::false_type {};

template <> struct is_symbolic_vector<math::vectors::SymbolicVector2> : std::true_type {};
template <> struct is_symbolic_vector<math::vectors::SymbolicVector3> : std::true_type {};
template <> struct is_symbolic_vector<math::vectors::SymbolicVector4> : std::true_type {};
template <> struct is_symbolic_vector<math::vectors::SymbolicVectorN> : std::true_type {};

template <typename T> inline constexpr bool is_symbolic_vector_v = is_symbolic_vector<T>::value;

} // namespace fizmo

#endif // FIZMO_SYMBOLIC_VECTORS_MATRICES_HPP