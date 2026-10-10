#ifndef FIZMO_SYMBOLIC_MATRICES_HPP
#define FIZMO_SYMBOLIC_MATRICES_HPP

#include "vectors.hpp"

namespace fizmo {
namespace math {
namespace matrices {

struct SymbolicMatrix2x2 {
private:
    std::array<std::array<cas::Expression, 2>, 2> data{};

public:
    SymbolicMatrix2x2() = default;
    SymbolicMatrix2x2(cas::Expression a, cas::Expression b, cas::Expression c, cas::Expression d);
    SymbolicMatrix2x2(const std::string& a, const std::string& b, const std::string& c, const std::string& d) : SymbolicMatrix2x2(cas::VARIABLE(a), cas::VARIABLE(b), cas::VARIABLE(c), cas::VARIABLE(d)) {}
    const std::array<std::array<cas::Expression, 2>, 2>& get_data() const noexcept { return data; }
    std::array<std::array<cas::Expression, 2>, 2>& get_data() noexcept { return data; }
    static SymbolicMatrix2x2 identity() { return { cas::Const(1), cas::Const(0), cas::Const(0), cas::Const(1) }; }
    static SymbolicMatrix2x2 zero()     { return { cas::Const(0), cas::Const(0), cas::Const(0), cas::Const(0) }; }
    static SymbolicMatrix2x2 diagonal(const cas::Expression& a, const cas::Expression& b)  { return { a, cas::Const(0), cas::Const(0), b }; }
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data[r][c]; }
    SymbolicMatrix2x2 operator+(const SymbolicMatrix2x2& o) const;
    SymbolicMatrix2x2 operator-(const SymbolicMatrix2x2& o) const;
    SymbolicMatrix2x2 operator-()                           const { return { -data[0][0], -data[0][1], -data[1][0], -data[1][1] }; }
    SymbolicMatrix2x2 operator*(const cas::Expression& s)        const { return { data[0][0]*s, data[0][1]*s, data[1][0]*s, data[1][1]*s }; }
    SymbolicMatrix2x2 operator*(double s)                   const { return { data[0][0]*s, data[0][1]*s, data[1][0]*s, data[1][1]*s }; }
    SymbolicMatrix2x2 operator/(const cas::Expression& s)        const { return { data[0][0]/s, data[0][1]/s, data[1][0]/s, data[1][1]/s }; }
    SymbolicMatrix2x2 operator/(double s)                   const { return { data[0][0]/s, data[0][1]/s, data[1][0]/s, data[1][1]/s }; }
    SymbolicMatrix2x2& operator+=(const SymbolicMatrix2x2& o) { return *this = *this + o; }
    SymbolicMatrix2x2& operator-=(const SymbolicMatrix2x2& o) { return *this = *this - o; }
    SymbolicMatrix2x2& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicMatrix2x2& operator*=(double s)                   { return *this = *this * s; }
    SymbolicMatrix2x2 operator*(const SymbolicMatrix2x2& o) const;
    SymbolicMatrix2x2& operator*=(const SymbolicMatrix2x2& o) { return *this = *this * o; }
    vectors::SymbolicVector2 operator*(const vectors::SymbolicVector2& v) const { return { data[0][0]*v.x + data[0][1]*v.y, data[1][0]*v.x + data[1][1]*v.y }; }
    SymbolicMatrix2x2 transpose()    const { return { data[0][0], data[1][0], data[0][1], data[1][1] }; }
    cas::Expression        determinant()  const { return data[0][0]*data[1][1] - data[0][1]*data[1][0]; }
    cas::Expression        trace()        const { return data[0][0] + data[1][1]; }
    SymbolicMatrix2x2 inverse()      const { auto d = determinant(); return {  data[1][1]/d, -data[0][1]/d, -data[1][0]/d,  data[0][0]/d }; }
    SymbolicMatrix2x2 hadamard(const SymbolicMatrix2x2& o) const;
    SymbolicMatrix2x2 differentiate(const std::string& var)                       const;
    SymbolicMatrix2x2 differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicMatrix2x2 differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicMatrix2x2 substitute(const std::string& var, double val)           const;
    SymbolicMatrix2x2 substitute(const std::string& var, const cas::Expression& r)  const;
    SymbolicMatrix2x2 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m = *this; for (auto& [n,v] : pairs) m = m.substitute(n, v); return m; }
    SymbolicMatrix2x2 partial_evaluate(const std::string& var, double val)          const;
    SymbolicMatrix2x2 partial_evaluate(const std::string& var, const cas::Expression& r) const;
    SymbolicMatrix2x2 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m = *this; for (auto& [n,v] : pairs) m = m.partial_evaluate(n, v); return m; }
    SymbolicMatrix2x2 rewrite(const cas::RewriterConfig& cfg = {})        const;
    SymbolicMatrix2x2 simplify()                                          const;
    SymbolicMatrix2x2 full_simplify(const cas::RewriterConfig& cfg = {})  const;
    Matrix2d evaluate(const std::unordered_map<std::string, double>& vals) const;
    Matrix2d evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;
    friend std::ostream& operator<<(std::ostream& os, const SymbolicMatrix2x2& m) {
        os << "[\n  [ " << m.data[0][0].full_simplify() << ",  " << m.data[0][1].full_simplify() << " ]\n  [ " << m.data[1][0].full_simplify() << ",  " << m.data[1][1].full_simplify() << " ]\n]";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }
};

inline SymbolicMatrix2x2 operator*(const cas::Expression& s, const SymbolicMatrix2x2& m) { return m * s; }
inline SymbolicMatrix2x2 operator*(double s,            const SymbolicMatrix2x2& m) { return m * s; }

struct SymbolicMatrix3x3 {
private:
    std::array<std::array<cas::Expression, 3>, 3> data{};

public:
    SymbolicMatrix3x3() = default;
    SymbolicMatrix3x3(cas::Expression a00, cas::Expression a01, cas::Expression a02,
                      cas::Expression a10, cas::Expression a11, cas::Expression a12,
                      cas::Expression a20, cas::Expression a21, cas::Expression a22);
    SymbolicMatrix3x3(const std::string& a00, const std::string& a01, const std::string& a02,
                      const std::string& a10, const std::string& a11, const std::string& a12,
                      const std::string& a20, const std::string& a21, const std::string& a22)
;
    const std::array<std::array<cas::Expression, 3>, 3>& get_data() const noexcept { return data; }
    std::array<std::array<cas::Expression, 3>, 3>& get_data() noexcept { return data; }
    static SymbolicMatrix3x3 identity();
    static SymbolicMatrix3x3 zero();
    static SymbolicMatrix3x3 diagonal(const cas::Expression& a, const cas::Expression& b, const cas::Expression& c);
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data[r][c]; }
    SymbolicMatrix3x3 operator+(const SymbolicMatrix3x3& o) const;
    SymbolicMatrix3x3 operator-(const SymbolicMatrix3x3& o) const;
    SymbolicMatrix3x3 operator-()                           const;
    SymbolicMatrix3x3 operator*(const cas::Expression& s)        const;
    SymbolicMatrix3x3 operator*(double s)                   const;
    SymbolicMatrix3x3 operator/(const cas::Expression& s)        const;
    SymbolicMatrix3x3 operator/(double s)                   const;
    SymbolicMatrix3x3& operator+=(const SymbolicMatrix3x3& o) { return *this = *this + o; }
    SymbolicMatrix3x3& operator-=(const SymbolicMatrix3x3& o) { return *this = *this - o; }
    SymbolicMatrix3x3& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicMatrix3x3& operator*=(double s)                   { return *this = *this * s; }
    SymbolicMatrix3x3 operator*(const SymbolicMatrix3x3& o) const;
    SymbolicMatrix3x3& operator*=(const SymbolicMatrix3x3& o) { return *this = *this * o; }
    vectors::SymbolicVector3 operator*(const vectors::SymbolicVector3& v) const;
    SymbolicMatrix3x3 transpose() const;
    cas::Expression        trace()     const { return data[0][0]+data[1][1]+data[2][2]; }
    cas::Expression cofactor(int row, int col) const;
    cas::Expression        determinant()    const { return data[0][0]*cofactor(0,0)+data[0][1]*cofactor(0,1)+data[0][2]*cofactor(0,2); }
    SymbolicMatrix3x3 cofactor_matrix() const;
    SymbolicMatrix3x3 adjugate()        const { return cofactor_matrix().transpose(); }
    SymbolicMatrix3x3 inverse()         const { return adjugate() / determinant(); }
    SymbolicMatrix3x3 hadamard(const SymbolicMatrix3x3& o) const;
    SymbolicMatrix3x3 differentiate(const std::string& var)                       const;
    SymbolicMatrix3x3 differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicMatrix3x3 differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicMatrix3x3 substitute(const std::string& var, double val)          const;
    SymbolicMatrix3x3 substitute(const std::string& var, const cas::Expression& e) const;
    SymbolicMatrix3x3 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m=*this; for(auto&[n,v]:pairs) m=m.substitute(n,v); return m; }
    SymbolicMatrix3x3 partial_evaluate(const std::string& var, double val)          const;
    SymbolicMatrix3x3 partial_evaluate(const std::string& var, const cas::Expression& e) const;
    SymbolicMatrix3x3 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m=*this; for(auto&[n,v]:pairs) m=m.partial_evaluate(n,v); return m; }
    SymbolicMatrix3x3 rewrite(const cas::RewriterConfig& cfg = {})       const;
    SymbolicMatrix3x3 simplify()                                                              const;
    SymbolicMatrix3x3 full_simplify(const cas::RewriterConfig& cfg = {}) const;
    Matrix3d evaluate(const std::unordered_map<std::string, double>& vals) const;
    Matrix3d evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;
    friend std::ostream& operator<<(std::ostream& os, const SymbolicMatrix3x3& m) {
        os << "[\n";
        for (int r=0;r<3;++r) { os << "  [ " << m.data[r][0].full_simplify() << ",  " << m.data[r][1].full_simplify() << ",  " << m.data[r][2].full_simplify() << " ]"; if(r<2) os << "\n"; }
        os << "\n]";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }
};

inline SymbolicMatrix3x3 operator*(const cas::Expression& s, const SymbolicMatrix3x3& m) { return m * s; }
inline SymbolicMatrix3x3 operator*(double s,             const SymbolicMatrix3x3& m) { return m * s; }

struct SymbolicMatrix4x4 {
private:
    std::array<std::array<cas::Expression, 4>, 4> data{};

public:
    SymbolicMatrix4x4() = default;

    SymbolicMatrix4x4(cas::Expression a00, cas::Expression a01, cas::Expression a02, cas::Expression a03,
                      cas::Expression a10, cas::Expression a11, cas::Expression a12, cas::Expression a13,
                      cas::Expression a20, cas::Expression a21, cas::Expression a22, cas::Expression a23,
                      cas::Expression a30, cas::Expression a31, cas::Expression a32, cas::Expression a33)
;

    static SymbolicMatrix4x4 identity();

    static SymbolicMatrix4x4 zero();

    cas::Expression& at(std::size_t r, std::size_t c)       { return data[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data[r][c]; }

    const auto& get_data() const noexcept { return data; }
    auto&       get_data()       noexcept { return data; }

    SymbolicMatrix4x4 operator+(const SymbolicMatrix4x4& o) const;

    SymbolicMatrix4x4 operator-(const SymbolicMatrix4x4& o) const;

    SymbolicMatrix4x4 operator-() const;

    SymbolicMatrix4x4 operator*(const cas::Expression& s) const;

    SymbolicMatrix4x4 operator*(const double s) const;

    SymbolicMatrix4x4 operator/(const cas::Expression& s) const;

    SymbolicMatrix4x4& operator+=(const SymbolicMatrix4x4& o) { return *this = *this + o; }
    SymbolicMatrix4x4& operator-=(const SymbolicMatrix4x4& o) { return *this = *this - o; }
    SymbolicMatrix4x4& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicMatrix4x4& operator/=(const cas::Expression& s)        { return *this = *this / s; }

    SymbolicMatrix4x4 operator*(const SymbolicMatrix4x4& o) const;

    vectors::SymbolicVector4 operator*(const vectors::SymbolicVector4& v) const;

    SymbolicMatrix4x4 transpose() const;

    cas::Expression determinant() const;

    cas::Expression trace() const {
        return data[0][0] + data[1][1] + data[2][2] + data[3][3];
    }

    SymbolicMatrix4x4 inverse() const;

    SymbolicMatrix4x4 differentiate(const std::string& var) const;

    SymbolicMatrix4x4 substitute(const std::string& var, double val) const;

    SymbolicMatrix4x4 substitute(const std::string& var, const cas::Expression& e) const;

    SymbolicMatrix4x4 partial_evaluate(const std::string& var, double val) const;

    SymbolicMatrix4x4 simplify() const;

    SymbolicMatrix4x4 full_simplify(const cas::RewriterConfig& cfg = {}) const;

    SymbolicMatrix4x4 rewrite(const cas::RewriterConfig& cfg = {}) const;

    friend std::ostream& operator<<(std::ostream& os, const SymbolicMatrix4x4& m) {
        os << "[\n";
        for (int i=0;i<4;i++) {
            os << "  ";
            for (int j=0;j<4;j++) {
                os << m.data[i][j].full_simplify();
                if (j<3) os << ", ";
            }
            os << "\n";
        }
        os << "]";
        return os;
    }
};

inline SymbolicMatrix4x4 operator*(const cas::Expression& s, const SymbolicMatrix4x4& m) { return m * s; }
inline SymbolicMatrix4x4 operator*(double s,                 const SymbolicMatrix4x4& m) { return m * s; }

class SymbolicMatrixN {
public:
    SymbolicMatrixN() = default;
    explicit SymbolicMatrixN(std::size_t n) : n_(n), data_(n, std::vector<cas::Expression>(n)) {}
    SymbolicMatrixN(std::size_t n, const std::vector<std::vector<cas::Expression>>& d) : n_(n), data_(d) {}
    static SymbolicMatrixN identity(std::size_t n);
    static SymbolicMatrixN zero(std::size_t n);
    static SymbolicMatrixN diagonal(const std::vector<cas::Expression>& diag);
    static SymbolicMatrixN outer(const vectors::SymbolicVectorN& a, const vectors::SymbolicVectorN& b);
    std::size_t dimension() const noexcept { return n_; }
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data_[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data_[r][c]; }
    std::vector<std::vector<cas::Expression>>&       get_data()       noexcept { return data_; }
    const std::vector<std::vector<cas::Expression>>& get_data() const noexcept { return data_; }
    vectors::SymbolicVectorN row(std::size_t r) const { return vectors::SymbolicVectorN(data_[r]); }
    vectors::SymbolicVectorN col(std::size_t c) const {
        vectors::SymbolicVectorN v(n_);
        for (std::size_t r = 0; r < n_; ++r) v[r] = data_[r][c];
        return v;
    }
    SymbolicMatrixN operator+(const SymbolicMatrixN& o) const;
    SymbolicMatrixN operator-(const SymbolicMatrixN& o) const;
    SymbolicMatrixN operator-() const;
    SymbolicMatrixN operator*(const cas::Expression& s) const;
    SymbolicMatrixN operator*(double s) const;
    SymbolicMatrixN operator/(const cas::Expression& s) const;
    SymbolicMatrixN operator/(double s) const;
    SymbolicMatrixN& operator+=(const SymbolicMatrixN& o) { return *this = *this + o; }
    SymbolicMatrixN& operator-=(const SymbolicMatrixN& o) { return *this = *this - o; }
    SymbolicMatrixN& operator*=(const cas::Expression& s)      { return *this = *this * s; }
    SymbolicMatrixN& operator*=(double s)                  { return *this = *this * s; }
    SymbolicMatrixN operator*(const SymbolicMatrixN& o) const;
    SymbolicMatrixN& operator*=(const SymbolicMatrixN& o) { return *this = *this * o; }
    vectors::SymbolicVectorN operator*(const vectors::SymbolicVectorN& v) const;
    SymbolicMatrixN hadamard(const SymbolicMatrixN& o) const;
    SymbolicMatrixN transpose() const;
    cas::Expression trace() const {
        cas::Expression t = data_[0][0];
        for (std::size_t i = 1; i < n_; ++i) t = t + data_[i][i];
        return t;
    }
    cas::Expression determinant() const;
    SymbolicMatrixN submatrix(std::size_t dr, std::size_t dc) const;
    cas::Expression cofactor(std::size_t r, std::size_t c) const;
    SymbolicMatrixN cofactor_matrix() const;
    SymbolicMatrixN adjugate() const { return cofactor_matrix().transpose(); }
    SymbolicMatrixN inverse() const {
        cas::Expression det = determinant();
        return adjugate() / det;
    }
    SymbolicMatrixN differentiate(const std::string& var) const;
    SymbolicMatrixN substitute(const std::string& var, double val) const;
    SymbolicMatrixN substitute(const std::string& var, const cas::Expression& repl) const;
    SymbolicMatrixN substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicMatrixN simplify() const;
    SymbolicMatrixN full_simplify(const cas::RewriterConfig& cfg = {}) const;
    SymbolicMatrixN rewrite(const cas::RewriterConfig& cfg = {}) const;
    std::vector<std::vector<double>> evaluate(const std::unordered_map<std::string, double>& vals) const;
    std::vector<std::vector<double>> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;
    static SymbolicMatrixN from(const SymbolicMatrix2x2& m);
    static SymbolicMatrixN from(const SymbolicMatrix3x3& m);
    static SymbolicMatrixN from(const SymbolicMatrix4x4& m);
    friend std::ostream& operator<<(std::ostream& os, const SymbolicMatrixN& m) {
        os << "[\n";
        for (std::size_t r = 0; r < m.n_; ++r) {
            os << "  [ ";
            for (std::size_t c = 0; c < m.n_; ++c) {
                os << m.data_[r][c].full_simplify();
                if (c + 1 < m.n_) os << ",  ";
            }
            os << " ]";
            if (r + 1 < m.n_) os << "\n";
        }
        os << "\n]";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    std::size_t n_ = 0;
    std::vector<std::vector<cas::Expression>> data_;

    void assert_same_dim(const SymbolicMatrixN& o) const;
};

inline SymbolicMatrixN operator*(const cas::Expression& s, const SymbolicMatrixN& m) { return m * s; }
inline SymbolicMatrixN operator*(double s,             const SymbolicMatrixN& m) { return m * s; }

class SymbolicMatrixNM {
public:
    SymbolicMatrixNM() = default;
    SymbolicMatrixNM(std::size_t rows, std::size_t cols) : rows_(rows), cols_(cols), data_(rows, std::vector<cas::Expression>(cols)) {}
    SymbolicMatrixNM(const std::vector<std::vector<cas::Expression>>& d);
    SymbolicMatrixNM(
        std::size_t rows, std::size_t cols,
        std::initializer_list<cas::Expression> elems
    );
    SymbolicMatrixNM(std::initializer_list<std::initializer_list<cas::Expression>> rows);
    static SymbolicMatrixNM zero(std::size_t rows, std::size_t cols);
    static SymbolicMatrixNM identity(std::size_t n);
    static SymbolicMatrixNM outer(const vectors::SymbolicVectorN& a, const vectors::SymbolicVectorN& b);
    std::size_t num_rows() const noexcept { return rows_; }
    std::size_t num_cols() const noexcept { return cols_; }
    bool is_square()       const noexcept { return rows_ == cols_; }
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data_[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data_[r][c]; }
    std::vector<std::vector<cas::Expression>>&       get_data()       noexcept { return data_; }
    const std::vector<std::vector<cas::Expression>>& get_data() const noexcept { return data_; }
    vectors::SymbolicVectorN row(std::size_t r) const { return vectors::SymbolicVectorN(data_[r]); }
    vectors::SymbolicVectorN col(std::size_t c) const;
    SymbolicMatrixNM transpose() const;
    SymbolicMatrixNM operator+(const SymbolicMatrixNM& o) const;
    SymbolicMatrixNM operator-(const SymbolicMatrixNM& o) const;
    SymbolicMatrixNM operator-() const;
    SymbolicMatrixNM& operator+=(const SymbolicMatrixNM& o) { return *this = *this + o; }
    SymbolicMatrixNM& operator-=(const SymbolicMatrixNM& o) { return *this = *this - o; }
    SymbolicMatrixNM operator*(const cas::Expression& s) const;
    SymbolicMatrixNM operator*(double s) const;
    SymbolicMatrixNM operator/(const cas::Expression& s) const;
    SymbolicMatrixNM operator/(double s) const;
    SymbolicMatrixNM& operator*=(const cas::Expression& s) { return *this = *this * s; }
    SymbolicMatrixNM& operator*=(double s)             { return *this = *this * s; }
    SymbolicMatrixNM operator*(const SymbolicMatrixNM& o) const;
    vectors::SymbolicVectorN operator*(const vectors::SymbolicVectorN& v) const;
    SymbolicMatrixNM hadamard(const SymbolicMatrixNM& o) const;
    cas::Expression trace() const;
    cas::Expression frobenius_norm_squared() const;
    cas::Expression frobenius_norm() const { return SQRT(frobenius_norm_squared()); }
    SymbolicMatrixNM submatrix(std::size_t dr, std::size_t dc) const;
    cas::Expression determinant() const;
    cas::Expression cofactor(std::size_t r, std::size_t c) const;
    SymbolicMatrixNM cofactor_matrix() const;
    SymbolicMatrixNM adjugate() const { return cofactor_matrix().transpose(); }
    SymbolicMatrixNM inverse()  const { return adjugate() / determinant(); }
    SymbolicMatrixNM differentiate(const std::string& var) const;
    SymbolicMatrixNM differentiate_iterative(const std::string& var, unsigned n) const;
    SymbolicMatrixNM differentiate_recursive(const std::string& var, unsigned n) const;
    SymbolicMatrixNM substitute(const std::string& var, double val) const;
    SymbolicMatrixNM substitute(const std::string& var, const cas::Expression& repl) const;
    SymbolicMatrixNM substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicMatrixNM partial_evaluate(const std::string& var, double val) const;
    SymbolicMatrixNM partial_evaluate(const std::string& var, const cas::Expression& repl) const;
    SymbolicMatrixNM partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    SymbolicMatrixNM simplify() const;
    SymbolicMatrixNM full_simplify(const cas::RewriterConfig& cfg = {}) const;
    SymbolicMatrixNM rewrite(const cas::RewriterConfig& cfg = {}) const;
    std::vector<std::vector<double>> evaluate(const std::unordered_map<std::string, double>& vals) const;
    std::vector<std::vector<double>> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const;
    static SymbolicMatrixNM from(const SymbolicMatrix2x2& m);
    static SymbolicMatrixNM from(const SymbolicMatrix3x3& m);
    static SymbolicMatrixNM from(const SymbolicMatrix4x4& m);
    static SymbolicMatrixNM from(const SymbolicMatrixN& m);
    SymbolicMatrixN to_square() const;
    friend std::ostream& operator<<(std::ostream& os, const SymbolicMatrixNM& m) {
        os << "[\n";
        for (std::size_t r = 0; r < m.rows_; ++r) {
            os << "  [ ";
            for (std::size_t c = 0; c < m.cols_; ++c) {
                os << m.data_[r][c].full_simplify();
                if (c + 1 < m.cols_) os << ",  ";
            }
            os << " ]";
            if (r + 1 < m.rows_) os << "\n";
        }
        os << "\n]";
        return os;
    }
    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    std::size_t rows_ = 0;
    std::size_t cols_ = 0;
    std::vector<std::vector<cas::Expression>> data_;

    void assert_same_shape(const SymbolicMatrixNM& o) const;
};

inline SymbolicMatrixNM operator*(const cas::Expression& s, const SymbolicMatrixNM& m) { return m * s; }
inline SymbolicMatrixNM operator*(double s,             const SymbolicMatrixNM& m) { return m * s; }

} // namespace matrices
} // namespace math

template <typename T> struct is_symbolic_matrix : std::false_type {};

template <> struct is_symbolic_matrix<math::matrices::SymbolicMatrix2x2> : std::true_type {};
template <> struct is_symbolic_matrix<math::matrices::SymbolicMatrix3x3> : std::true_type {};
template <> struct is_symbolic_matrix<math::matrices::SymbolicMatrix4x4> : std::true_type {};
template <> struct is_symbolic_matrix<math::matrices::SymbolicMatrixN>   : std::true_type {};
template <> struct is_symbolic_matrix<math::matrices::SymbolicMatrixNM>  : std::true_type {};

template <typename T> inline constexpr bool is_symbolic_matrix_v = is_symbolic_matrix<T>::value;

} // namespace fizmo

#endif // FIZMO_SYMBOLIC_MATRICES_HPP