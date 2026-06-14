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
    SymbolicMatrix2x2(cas::Expression a, cas::Expression b, cas::Expression c, cas::Expression d) { data[0][0]=std::move(a); data[0][1]=std::move(b); data[1][0]=std::move(c); data[1][1]=std::move(d); }
    SymbolicMatrix2x2(const std::string& a, const std::string& b, const std::string& c, const std::string& d) : SymbolicMatrix2x2(cas::VARIABLE(a), cas::VARIABLE(b), cas::VARIABLE(c), cas::VARIABLE(d)) {}
    const std::array<std::array<cas::Expression, 2>, 2>& get_data() const noexcept { return data; }
    std::array<std::array<cas::Expression, 2>, 2>& get_data() noexcept { return data; }
    static SymbolicMatrix2x2 identity() { return { cas::Const(1), cas::Const(0), cas::Const(0), cas::Const(1) }; }
    static SymbolicMatrix2x2 zero()     { return { cas::Const(0), cas::Const(0), cas::Const(0), cas::Const(0) }; }
    static SymbolicMatrix2x2 diagonal(const cas::Expression& a, const cas::Expression& b)  { return { a, cas::Const(0), cas::Const(0), b }; }
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data[r][c]; }
    SymbolicMatrix2x2 operator+(const SymbolicMatrix2x2& o) const { return { data[0][0]+o.data[0][0], data[0][1]+o.data[0][1], data[1][0]+o.data[1][0], data[1][1]+o.data[1][1] }; }
    SymbolicMatrix2x2 operator-(const SymbolicMatrix2x2& o) const { return { data[0][0]-o.data[0][0], data[0][1]-o.data[0][1], data[1][0]-o.data[1][0], data[1][1]-o.data[1][1] }; }
    SymbolicMatrix2x2 operator-()                           const { return { -data[0][0], -data[0][1], -data[1][0], -data[1][1] }; }
    SymbolicMatrix2x2 operator*(const cas::Expression& s)        const { return { data[0][0]*s, data[0][1]*s, data[1][0]*s, data[1][1]*s }; }
    SymbolicMatrix2x2 operator*(double s)                   const { return { data[0][0]*s, data[0][1]*s, data[1][0]*s, data[1][1]*s }; }
    SymbolicMatrix2x2 operator/(const cas::Expression& s)        const { return { data[0][0]/s, data[0][1]/s, data[1][0]/s, data[1][1]/s }; }
    SymbolicMatrix2x2 operator/(double s)                   const { return { data[0][0]/s, data[0][1]/s, data[1][0]/s, data[1][1]/s }; }
    SymbolicMatrix2x2& operator+=(const SymbolicMatrix2x2& o) { return *this = *this + o; }
    SymbolicMatrix2x2& operator-=(const SymbolicMatrix2x2& o) { return *this = *this - o; }
    SymbolicMatrix2x2& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicMatrix2x2& operator*=(double s)                   { return *this = *this * s; }
    SymbolicMatrix2x2 operator*(const SymbolicMatrix2x2& o) const {
        return { data[0][0]*o.data[0][0]+data[0][1]*o.data[1][0], data[0][0]*o.data[0][1]+data[0][1]*o.data[1][1],
                 data[1][0]*o.data[0][0]+data[1][1]*o.data[1][0], data[1][0]*o.data[0][1]+data[1][1]*o.data[1][1] };
    }
    SymbolicMatrix2x2& operator*=(const SymbolicMatrix2x2& o) { return *this = *this * o; }
    vectors::SymbolicVector2 operator*(const vectors::SymbolicVector2& v) const { return { data[0][0]*v.x + data[0][1]*v.y, data[1][0]*v.x + data[1][1]*v.y }; }
    SymbolicMatrix2x2 transpose()    const { return { data[0][0], data[1][0], data[0][1], data[1][1] }; }
    cas::Expression        determinant()  const { return data[0][0]*data[1][1] - data[0][1]*data[1][0]; }
    cas::Expression        trace()        const { return data[0][0] + data[1][1]; }
    SymbolicMatrix2x2 inverse()      const { auto d = determinant(); return {  data[1][1]/d, -data[0][1]/d, -data[1][0]/d,  data[0][0]/d }; }
    SymbolicMatrix2x2 hadamard(const SymbolicMatrix2x2& o) const { return { data[0][0]*o.data[0][0], data[0][1]*o.data[0][1], data[1][0]*o.data[1][0], data[1][1]*o.data[1][1] }; }
    SymbolicMatrix2x2 differentiate(const std::string& var)                       const { return { DIFFERENTIATE(data[0][0],var),            DIFFERENTIATE(data[0][1],var),            DIFFERENTIATE(data[1][0],var),            DIFFERENTIATE(data[1][1],var)            }; }
    SymbolicMatrix2x2 differentiate_iterative(const std::string& var, unsigned n) const { return { DIFFERENTIATE_ITERATIVE(data[0][0],var,n), DIFFERENTIATE_ITERATIVE(data[0][1],var,n), DIFFERENTIATE_ITERATIVE(data[1][0],var,n), DIFFERENTIATE_ITERATIVE(data[1][1],var,n) }; }
    SymbolicMatrix2x2 differentiate_recursive(const std::string& var, unsigned n) const { return { DIFFERENTIATE_RECURSIVE(data[0][0],var,n), DIFFERENTIATE_RECURSIVE(data[0][1],var,n), DIFFERENTIATE_RECURSIVE(data[1][0],var,n), DIFFERENTIATE_RECURSIVE(data[1][1],var,n) }; }
    SymbolicMatrix2x2 substitute(const std::string& var, double val)           const { return { SUBSTITUTE(data[0][0],var,val), SUBSTITUTE(data[0][1],var,val), SUBSTITUTE(data[1][0],var,val), SUBSTITUTE(data[1][1],var,val) }; }
    SymbolicMatrix2x2 substitute(const std::string& var, const cas::Expression& r)  const { return { SUBSTITUTE(data[0][0],var,r),   SUBSTITUTE(data[0][1],var,r),   SUBSTITUTE(data[1][0],var,r),   SUBSTITUTE(data[1][1],var,r)   }; }
    SymbolicMatrix2x2 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m = *this; for (auto& [n,v] : pairs) m = m.substitute(n, v); return m; }
    SymbolicMatrix2x2 partial_evaluate(const std::string& var, double val)          const { return { PARTIAL_EVALUATE(data[0][0],var,val), PARTIAL_EVALUATE(data[0][1],var,val), PARTIAL_EVALUATE(data[1][0],var,val), PARTIAL_EVALUATE(data[1][1],var,val) }; }
    SymbolicMatrix2x2 partial_evaluate(const std::string& var, const cas::Expression& r) const { return { PARTIAL_EVALUATE(data[0][0],var,r),   PARTIAL_EVALUATE(data[0][1],var,r),   PARTIAL_EVALUATE(data[1][0],var,r),   PARTIAL_EVALUATE(data[1][1],var,r)   }; }
    SymbolicMatrix2x2 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m = *this; for (auto& [n,v] : pairs) m = m.partial_evaluate(n, v); return m; }
    SymbolicMatrix2x2 rewrite(const cas::RewriterConfig& cfg = {})        const { return { REWRITE(data[0][0],cfg),       REWRITE(data[0][1],cfg),       REWRITE(data[1][0],cfg),       REWRITE(data[1][1],cfg)       }; }
    SymbolicMatrix2x2 simplify()                                          const { return { SIMPLIFY(data[0][0]),          SIMPLIFY(data[0][1]),          SIMPLIFY(data[1][0]),          SIMPLIFY(data[1][1])          }; }
    SymbolicMatrix2x2 full_simplify(const cas::RewriterConfig& cfg = {})  const { return { FULL_SIMPLIFY(data[0][0],cfg), FULL_SIMPLIFY(data[0][1],cfg), FULL_SIMPLIFY(data[1][0],cfg), FULL_SIMPLIFY(data[1][1],cfg) }; }
    Matrix2d evaluate(const std::unordered_map<std::string, double>& vals) const {
        Matrix2d m;
        for (int r = 0; r < 2; ++r) for (int c = 0; c < 2; ++c) m.at(r, c) = data[r][c].evaluate(vals);
        return m;
    }
    Matrix2d evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        Matrix2d m;
        for (int r = 0; r < 2; ++r) for (int c = 0; c < 2; ++c) m.at(r, c) = data[r][c].evaluate(vals);
        return m;
    }
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
                      cas::Expression a20, cas::Expression a21, cas::Expression a22) {
        data[0]={std::move(a00),std::move(a01),std::move(a02)};
        data[1]={std::move(a10),std::move(a11),std::move(a12)};
        data[2]={std::move(a20),std::move(a21),std::move(a22)};
    }
    SymbolicMatrix3x3(const std::string& a00, const std::string& a01, const std::string& a02,
                      const std::string& a10, const std::string& a11, const std::string& a12,
                      const std::string& a20, const std::string& a21, const std::string& a22)
        : SymbolicMatrix3x3(cas::VARIABLE(a00),cas::VARIABLE(a01),cas::VARIABLE(a02),
                            cas::VARIABLE(a10),cas::VARIABLE(a11),cas::VARIABLE(a12),
                            cas::VARIABLE(a20),cas::VARIABLE(a21),cas::VARIABLE(a22)) {}
    const std::array<std::array<cas::Expression, 3>, 3>& get_data() const noexcept { return data; }
    std::array<std::array<cas::Expression, 3>, 3>& get_data() noexcept { return data; }
    static SymbolicMatrix3x3 identity() { return { cas::Const(1),cas::Const(0),cas::Const(0), cas::Const(0),cas::Const(1),cas::Const(0), cas::Const(0),cas::Const(0),cas::Const(1) }; }
    static SymbolicMatrix3x3 zero()     { return { cas::Const(0),cas::Const(0),cas::Const(0), cas::Const(0),cas::Const(0),cas::Const(0), cas::Const(0),cas::Const(0),cas::Const(0) }; }
    static SymbolicMatrix3x3 diagonal(const cas::Expression& a, const cas::Expression& b, const cas::Expression& c) { return { a,cas::Const(0),cas::Const(0), cas::Const(0),b,cas::Const(0), cas::Const(0),cas::Const(0),c }; }
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data[r][c]; }
    SymbolicMatrix3x3 operator+(const SymbolicMatrix3x3& o) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]+o.data[r][c]; return m; }
    SymbolicMatrix3x3 operator-(const SymbolicMatrix3x3& o) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]-o.data[r][c]; return m; }
    SymbolicMatrix3x3 operator-()                           const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=-data[r][c]; return m; }
    SymbolicMatrix3x3 operator*(const cas::Expression& s)        const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]*s; return m; }
    SymbolicMatrix3x3 operator*(double s)                   const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]*s; return m; }
    SymbolicMatrix3x3 operator/(const cas::Expression& s)        const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]/s; return m; }
    SymbolicMatrix3x3 operator/(double s)                   const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]/s; return m; }
    SymbolicMatrix3x3& operator+=(const SymbolicMatrix3x3& o) { return *this = *this + o; }
    SymbolicMatrix3x3& operator-=(const SymbolicMatrix3x3& o) { return *this = *this - o; }
    SymbolicMatrix3x3& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicMatrix3x3& operator*=(double s)                   { return *this = *this * s; }
    SymbolicMatrix3x3 operator*(const SymbolicMatrix3x3& o) const {
        SymbolicMatrix3x3 m;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
            m.data[r][c] = data[r][0]*o.data[0][c];
            for (int k=1;k<3;++k) m.data[r][c] = m.data[r][c] + data[r][k]*o.data[k][c];
        }
        return m;
    }
    SymbolicMatrix3x3& operator*=(const SymbolicMatrix3x3& o) { return *this = *this * o; }
    vectors::SymbolicVector3 operator*(const vectors::SymbolicVector3& v) const {
        return { data[0][0]*v.x+data[0][1]*v.y+data[0][2]*v.z,
                 data[1][0]*v.x+data[1][1]*v.y+data[1][2]*v.z,
                 data[2][0]*v.x+data[2][1]*v.y+data[2][2]*v.z };
    }
    SymbolicMatrix3x3 transpose() const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[c][r]=data[r][c]; return m; }
    cas::Expression        trace()     const { return data[0][0]+data[1][1]+data[2][2]; }
    cas::Expression cofactor(int row, int col) const {
        std::array<std::array<cas::Expression,2>,2> sub{};
        int sr=0;
        for (int r=0;r<3;++r) { if(r==row) continue; int sc=0; for(int c=0;c<3;++c) { if(c==col) continue; sub[sr][sc]=data[r][c]; ++sc; } ++sr; }
        cas::Expression minor_det = sub[0][0]*sub[1][1] - sub[0][1]*sub[1][0];
        return ((row+col)%2==0) ? minor_det : -minor_det;
    }
    cas::Expression        determinant()    const { return data[0][0]*cofactor(0,0)+data[0][1]*cofactor(0,1)+data[0][2]*cofactor(0,2); }
    SymbolicMatrix3x3 cofactor_matrix() const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=cofactor(r,c); return m; }
    SymbolicMatrix3x3 adjugate()        const { return cofactor_matrix().transpose(); }
    SymbolicMatrix3x3 inverse()         const { return adjugate() / determinant(); }
    SymbolicMatrix3x3 hadamard(const SymbolicMatrix3x3& o) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]*o.data[r][c]; return m; }
    SymbolicMatrix3x3 differentiate(const std::string& var)                       const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=DIFFERENTIATE(data[r][c],var);            return m; }
    SymbolicMatrix3x3 differentiate_iterative(const std::string& var, unsigned n) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=DIFFERENTIATE_ITERATIVE(data[r][c],var,n); return m; }
    SymbolicMatrix3x3 differentiate_recursive(const std::string& var, unsigned n) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=DIFFERENTIATE_RECURSIVE(data[r][c],var,n); return m; }
    SymbolicMatrix3x3 substitute(const std::string& var, double val)          const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=SUBSTITUTE(data[r][c],var,val); return m; }
    SymbolicMatrix3x3 substitute(const std::string& var, const cas::Expression& e) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=SUBSTITUTE(data[r][c],var,e);   return m; }
    SymbolicMatrix3x3 substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m=*this; for(auto&[n,v]:pairs) m=m.substitute(n,v); return m; }
    SymbolicMatrix3x3 partial_evaluate(const std::string& var, double val)          const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=PARTIAL_EVALUATE(data[r][c],var,val); return m; }
    SymbolicMatrix3x3 partial_evaluate(const std::string& var, const cas::Expression& e) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=PARTIAL_EVALUATE(data[r][c],var,e);   return m; }
    SymbolicMatrix3x3 partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const { auto m=*this; for(auto&[n,v]:pairs) m=m.partial_evaluate(n,v); return m; }
    SymbolicMatrix3x3 rewrite(const cas::RewriterConfig& cfg = {})       const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=REWRITE(data[r][c],cfg);       return m; }
    SymbolicMatrix3x3 simplify()                                                              const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=SIMPLIFY(data[r][c]);           return m; }
    SymbolicMatrix3x3 full_simplify(const cas::RewriterConfig& cfg = {}) const { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=FULL_SIMPLIFY(data[r][c],cfg);  return m; }
    Matrix3d evaluate(const std::unordered_map<std::string, double>& vals) const {
        Matrix3d m;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) m.at(r, c)=data[r][c].evaluate(vals);
        return m;
    }
    Matrix3d evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        Matrix3d m;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) m.at(r, c)=data[r][c].evaluate(vals);
        return m;
    }
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
    {
        data[0] = { std::move(a00), std::move(a01), std::move(a02), std::move(a03) };
        data[1] = { std::move(a10), std::move(a11), std::move(a12), std::move(a13) };
        data[2] = { std::move(a20), std::move(a21), std::move(a22), std::move(a23) };
        data[3] = { std::move(a30), std::move(a31), std::move(a32), std::move(a33) };
    }

    static SymbolicMatrix4x4 identity() {
        return {
            cas::Const(1),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(1),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(1),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(1)
        };
    }

    static SymbolicMatrix4x4 zero() {
        return {
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0)
        };
    }

    cas::Expression& at(std::size_t r, std::size_t c)       { return data[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data[r][c]; }

    const auto& get_data() const noexcept { return data; }
    auto&       get_data()       noexcept { return data; }

    SymbolicMatrix4x4 operator+(const SymbolicMatrix4x4& o) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] + o.data[i][j];
        return r;
    }

    SymbolicMatrix4x4 operator-(const SymbolicMatrix4x4& o) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] - o.data[i][j];
        return r;
    }

    SymbolicMatrix4x4 operator-() const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = -data[i][j];
        return r;
    }

    SymbolicMatrix4x4 operator*(const cas::Expression& s) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] * s;
        return r;
    }

    SymbolicMatrix4x4 operator*(const double s) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] * cas::Const(s);
        return r;
    }

    SymbolicMatrix4x4 operator/(const cas::Expression& s) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] / s;
        return r;
    }

    SymbolicMatrix4x4& operator+=(const SymbolicMatrix4x4& o) { return *this = *this + o; }
    SymbolicMatrix4x4& operator-=(const SymbolicMatrix4x4& o) { return *this = *this - o; }
    SymbolicMatrix4x4& operator*=(const cas::Expression& s)        { return *this = *this * s; }
    SymbolicMatrix4x4& operator/=(const cas::Expression& s)        { return *this = *this / s; }

    SymbolicMatrix4x4 operator*(const SymbolicMatrix4x4& o) const {
        SymbolicMatrix4x4 r = zero();
        for (int i=0;i<4;i++)
            for (int j=0;j<4;j++)
                for (int k=0;k<4;k++)
                    r.data[i][j] = r.data[i][j] + data[i][k] * o.data[k][j];
        return r;
    }

    vectors::SymbolicVector4 operator*(const vectors::SymbolicVector4& v) const {
        return {
            data[0][0]*v.x + data[0][1]*v.y + data[0][2]*v.z + data[0][3]*v.w,
            data[1][0]*v.x + data[1][1]*v.y + data[1][2]*v.z + data[1][3]*v.w,
            data[2][0]*v.x + data[2][1]*v.y + data[2][2]*v.z + data[2][3]*v.w,
            data[3][0]*v.x + data[3][1]*v.y + data[3][2]*v.z + data[3][3]*v.w
        };
    }

    SymbolicMatrix4x4 transpose() const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[j][i] = data[i][j];
        return r;
    }

    cas::Expression determinant() const {
        auto minor2 = [&](int r0,int r1,int r2,int c0,int c1,int c2){
            return data[r0][c0]*(data[r1][c1]*data[r2][c2] - data[r1][c2]*data[r2][c1])
                 - data[r0][c1]*(data[r1][c0]*data[r2][c2] - data[r1][c2]*data[r2][c0])
                 + data[r0][c2]*(data[r1][c0]*data[r2][c1] - data[r1][c1]*data[r2][c0]);
        };

        return
            data[0][0] * minor2(1,2,3,1,2,3)
          - data[0][1] * minor2(1,2,3,0,2,3)
          + data[0][2] * minor2(1,2,3,0,1,3)
          - data[0][3] * minor2(1,2,3,0,1,2);
    }

    cas::Expression trace() const {
        return data[0][0] + data[1][1] + data[2][2] + data[3][3];
    }

    SymbolicMatrix4x4 inverse() const {
        cas::Expression det = determinant();
        SymbolicMatrix4x4 adj;

        for (int r=0;r<4;r++) {
            for (int c=0;c<4;c++) {
                std::array<std::array<cas::Expression,3>,3> m{};
                int rr=0;
                for (int i=0;i<4;i++) if (i!=r) {
                    int cc=0;
                    for (int j=0;j<4;j++) if (j!=c) {
                        m[rr][cc] = data[i][j];
                        cc++;
                    }
                    rr++;
                }

                cas::Expression minor =
                    m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1])
                  - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0])
                  + m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);

                cas::Expression cofactor = ((r+c)%2==0 ? minor : -minor);
                adj.data[c][r] = cofactor;
            }
        }

        return adj / det;
    }

    SymbolicMatrix4x4 differentiate(const std::string& var) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = DIFFERENTIATE(data[i][j], var);
        return r;
    }

    SymbolicMatrix4x4 substitute(const std::string& var, double val) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = SUBSTITUTE(data[i][j], var, val);
        return r;
    }

    SymbolicMatrix4x4 substitute(const std::string& var, const cas::Expression& e) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = SUBSTITUTE(data[i][j], var, e);
        return r;
    }

    SymbolicMatrix4x4 partial_evaluate(const std::string& var, double val) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = PARTIAL_EVALUATE(data[i][j], var, val);
        return r;
    }

    SymbolicMatrix4x4 simplify() const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = SIMPLIFY(data[i][j]);
        return r;
    }

    SymbolicMatrix4x4 full_simplify(const cas::RewriterConfig& cfg = {}) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = FULL_SIMPLIFY(data[i][j], cfg);
        return r;
    }

    SymbolicMatrix4x4 rewrite(const cas::RewriterConfig& cfg = {}) const {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = REWRITE(data[i][j], cfg);
        return r;
    }

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
    static SymbolicMatrixN identity(std::size_t n) {
        SymbolicMatrixN m(n);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j)
                m.data_[i][j] = cas::Const(i == j ? 1.0 : 0.0);
        }
        return m;
    }
    static SymbolicMatrixN zero(std::size_t n) {
        SymbolicMatrixN m(n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                m.data_[i][j] = cas::Const(0.0);
        return m;
    }
    static SymbolicMatrixN diagonal(const std::vector<cas::Expression>& diag) {
        std::size_t n = diag.size();
        auto m = zero(n);
        for (std::size_t i = 0; i < n; ++i) m.data_[i][i] = diag[i];
        return m;
    }
    static SymbolicMatrixN outer(const vectors::SymbolicVectorN& a, const vectors::SymbolicVectorN& b) {
        std::size_t n = a.dimension();
        SymbolicMatrixN m(n);
        for (std::size_t r = 0; r < n; ++r)
            for (std::size_t c = 0; c < n; ++c)
                m.data_[r][c] = a[r] * b[c];
        return m;
    }
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
    SymbolicMatrixN operator+(const SymbolicMatrixN& o) const {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] + o.data_[i][j];
        return r;
    }
    SymbolicMatrixN operator-(const SymbolicMatrixN& o) const {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] - o.data_[i][j];
        return r;
    }
    SymbolicMatrixN operator-() const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = -data_[i][j];
        return r;
    }
    SymbolicMatrixN operator*(const cas::Expression& s) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }
    SymbolicMatrixN operator*(double s) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }
    SymbolicMatrixN operator/(const cas::Expression& s) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }
    SymbolicMatrixN operator/(double s) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }
    SymbolicMatrixN& operator+=(const SymbolicMatrixN& o) { return *this = *this + o; }
    SymbolicMatrixN& operator-=(const SymbolicMatrixN& o) { return *this = *this - o; }
    SymbolicMatrixN& operator*=(const cas::Expression& s)      { return *this = *this * s; }
    SymbolicMatrixN& operator*=(double s)                  { return *this = *this * s; }
    SymbolicMatrixN operator*(const SymbolicMatrixN& o) const {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j) {
                r.data_[i][j] = data_[i][0] * o.data_[0][j];
                for (std::size_t k = 1; k < n_; ++k)
                    r.data_[i][j] = r.data_[i][j] + data_[i][k] * o.data_[k][j];
            }
        return r;
    }
    SymbolicMatrixN& operator*=(const SymbolicMatrixN& o) { return *this = *this * o; }
    vectors::SymbolicVectorN operator*(const vectors::SymbolicVectorN& v) const {
        assert(v.dimension() == n_);
        vectors::SymbolicVectorN r(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            r[i] = data_[i][0] * v[0];
            for (std::size_t j = 1; j < n_; ++j)
                r[i] = r[i] + data_[i][j] * v[j];
        }
        return r;
    }
    SymbolicMatrixN hadamard(const SymbolicMatrixN& o) const {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] * o.data_[i][j];
        return r;
    }
    SymbolicMatrixN transpose() const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[j][i] = data_[i][j];
        return r;
    }
    cas::Expression trace() const {
        cas::Expression t = data_[0][0];
        for (std::size_t i = 1; i < n_; ++i) t = t + data_[i][i];
        return t;
    }
    cas::Expression determinant() const {
        if (n_ == 1) return data_[0][0];
        if (n_ == 2) return data_[0][0] * data_[1][1] - data_[0][1] * data_[1][0];
        cas::Expression det = cas::Const(0.0);
        for (std::size_t c = 0; c < n_; ++c) {
            cas::Expression cofactor = submatrix(0, c).determinant();
            cas::Expression term = data_[0][c] * cofactor;
            det = (c % 2 == 0) ? det + term : det - term;
        }
        return det;
    }
    SymbolicMatrixN submatrix(std::size_t dr, std::size_t dc) const {
        SymbolicMatrixN m(n_ - 1);
        std::size_t mr = 0;
        for (std::size_t r = 0; r < n_; ++r) {
            if (r == dr) continue;
            std::size_t mc = 0;
            for (std::size_t c = 0; c < n_; ++c) {
                if (c == dc) continue;
                m.data_[mr][mc++] = data_[r][c];
            }
            ++mr;
        }
        return m;
    }
    cas::Expression cofactor(std::size_t r, std::size_t c) const {
        cas::Expression minor_det = submatrix(r, c).determinant();
        return ((r + c) % 2 == 0) ? minor_det : -minor_det;
    }
    SymbolicMatrixN cofactor_matrix() const {
        SymbolicMatrixN m(n_);
        for (std::size_t r = 0; r < n_; ++r)
            for (std::size_t c = 0; c < n_; ++c)
                m.data_[r][c] = cofactor(r, c);
        return m;
    }
    SymbolicMatrixN adjugate() const { return cofactor_matrix().transpose(); }
    SymbolicMatrixN inverse() const {
        cas::Expression det = determinant();
        return adjugate() / det;
    }
    SymbolicMatrixN differentiate(const std::string& var) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = DIFFERENTIATE(data_[i][j], var);
        return r;
    }
    SymbolicMatrixN substitute(const std::string& var, double val) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, val);
        return r;
    }
    SymbolicMatrixN substitute(const std::string& var, const cas::Expression& repl) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, repl);
        return r;
    }
    SymbolicMatrixN substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicMatrixN r(n_, data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < n_; ++i)
                for (std::size_t j = 0; j < n_; ++j)
                    r.data_[i][j] = SUBSTITUTE(r.data_[i][j], nm, v);
        return r;
    }
    SymbolicMatrixN simplify() const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = SIMPLIFY(data_[i][j]);
        return r;
    }
    SymbolicMatrixN full_simplify(const cas::RewriterConfig& cfg = {}) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = FULL_SIMPLIFY(data_[i][j], cfg);
        return r;
    }
    SymbolicMatrixN rewrite(const cas::RewriterConfig& cfg = {}) const {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = REWRITE(data_[i][j], cfg);
        return r;
    }
    std::vector<std::vector<double>> evaluate(const std::unordered_map<std::string, double>& vals) const {
        std::vector<std::vector<double>> out(n_, std::vector<double>(n_));
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }
    std::vector<std::vector<double>> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        std::vector<std::vector<double>> out(n_, std::vector<double>(n_));
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }
    static SymbolicMatrixN from(const SymbolicMatrix2x2& m) {
        SymbolicMatrixN r(2);
        for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) r.data_[i][j] = m.get_data()[i][j];
        return r;
    }
    static SymbolicMatrixN from(const SymbolicMatrix3x3& m) {
        SymbolicMatrixN r(3);
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) r.data_[i][j] = m.get_data()[i][j];
        return r;
    }
    static SymbolicMatrixN from(const SymbolicMatrix4x4& m) {
        SymbolicMatrixN r(4);
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j)
                r.at(i, j) = m.at(i, j);
        return r;
    }
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

    void assert_same_dim(const SymbolicMatrixN& o) const {
        if (n_ != o.n_)
            throw std::invalid_argument("SymbolicMatrixN: dimension mismatch ("
                + std::to_string(n_) + " vs " + std::to_string(o.n_) + ")");
    }
};

inline SymbolicMatrixN operator*(const cas::Expression& s, const SymbolicMatrixN& m) { return m * s; }
inline SymbolicMatrixN operator*(double s,             const SymbolicMatrixN& m) { return m * s; }

class SymbolicMatrixNM {
public:
    SymbolicMatrixNM() = default;
    SymbolicMatrixNM(std::size_t rows, std::size_t cols) : rows_(rows), cols_(cols), data_(rows, std::vector<cas::Expression>(cols)) {}
    SymbolicMatrixNM(const std::vector<std::vector<cas::Expression>>& d) {
        rows_ = d.size();

        if (rows_ == 0) {
            cols_ = 0;
            return;
        }

        cols_ = d[0].size();
        if (cols_ == 0) throw std::invalid_argument("Matrix cannot have zero columns");

        for (const auto& row : d) {
            if (row.size() != cols_) throw std::invalid_argument("All rows must have the same number of columns");
        }

        data_ = d;  
    }
    SymbolicMatrixNM(
        std::size_t rows, std::size_t cols,
        std::initializer_list<cas::Expression> elems
    ) : rows_(rows), cols_(cols) {
        if (elems.size() != rows * cols) throw std::invalid_argument("Wrong number of elements for matrix");
        data_.resize(rows);
        auto it = elems.begin();

        for (std::size_t r = 0; r < rows; ++r) {
            data_[r].reserve(cols);
            for (std::size_t c = 0; c < cols; ++c, ++it) data_[r].emplace_back(std::move(*it));   
        }
    }
    SymbolicMatrixNM(std::initializer_list<std::initializer_list<cas::Expression>> rows) {
        rows_ = rows.size();
        if (rows_ == 0) { cols_ = 0; return; }
        cols_ = rows.begin()->size();
        if (cols_ == 0) throw std::invalid_argument("Matrix cannot have zero columns");
        data_.reserve(rows_);

        for (const auto& r : rows) {
            if (r.size() != cols_) throw std::invalid_argument("All rows must have the same number of columns");
            data_.emplace_back(r.begin(), r.end());
        }
    }
    static SymbolicMatrixNM zero(std::size_t rows, std::size_t cols) {
        SymbolicMatrixNM m(rows, cols);
        for (std::size_t i = 0; i < rows; ++i)
            for (std::size_t j = 0; j < cols; ++j)
                m.data_[i][j] = cas::Const(0.0);
        return m;
    }
    static SymbolicMatrixNM identity(std::size_t n) {
        SymbolicMatrixNM m(n, n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                m.data_[i][j] = cas::Const(i == j ? 1.0 : 0.0);
        return m;
    }
    static SymbolicMatrixNM outer(const vectors::SymbolicVectorN& a, const vectors::SymbolicVectorN& b) {
        std::size_t r = a.dimension(), c = b.dimension();
        SymbolicMatrixNM m(r, c);
        for (std::size_t i = 0; i < r; ++i)
            for (std::size_t j = 0; j < c; ++j)
                m.data_[i][j] = a[i] * b[j];
        return m;
    }
    std::size_t num_rows() const noexcept { return rows_; }
    std::size_t num_cols() const noexcept { return cols_; }
    bool is_square()       const noexcept { return rows_ == cols_; }
    cas::Expression&       at(std::size_t r, std::size_t c)       { return data_[r][c]; }
    const cas::Expression& at(std::size_t r, std::size_t c) const { return data_[r][c]; }
    std::vector<std::vector<cas::Expression>>&       get_data()       noexcept { return data_; }
    const std::vector<std::vector<cas::Expression>>& get_data() const noexcept { return data_; }
    vectors::SymbolicVectorN row(std::size_t r) const { return vectors::SymbolicVectorN(data_[r]); }
    vectors::SymbolicVectorN col(std::size_t c) const {
        vectors::SymbolicVectorN v(rows_);
        for (std::size_t r = 0; r < rows_; ++r) v[r] = data_[r][c];
        return v;
    }
    SymbolicMatrixNM transpose() const {
        SymbolicMatrixNM m(cols_, rows_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                m.data_[j][i] = data_[i][j];
        return m;
    }
    SymbolicMatrixNM operator+(const SymbolicMatrixNM& o) const {
        assert_same_shape(o);
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] + o.data_[i][j];
        return r;
    }
    SymbolicMatrixNM operator-(const SymbolicMatrixNM& o) const {
        assert_same_shape(o);
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] - o.data_[i][j];
        return r;
    }
    SymbolicMatrixNM operator-() const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = -data_[i][j];
        return r;
    }
    SymbolicMatrixNM& operator+=(const SymbolicMatrixNM& o) { return *this = *this + o; }
    SymbolicMatrixNM& operator-=(const SymbolicMatrixNM& o) { return *this = *this - o; }
    SymbolicMatrixNM operator*(const cas::Expression& s) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }
    SymbolicMatrixNM operator*(double s) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }
    SymbolicMatrixNM operator/(const cas::Expression& s) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }
    SymbolicMatrixNM operator/(double s) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }
    SymbolicMatrixNM& operator*=(const cas::Expression& s) { return *this = *this * s; }
    SymbolicMatrixNM& operator*=(double s)             { return *this = *this * s; }
    SymbolicMatrixNM operator*(const SymbolicMatrixNM& o) const {
        if (cols_ != o.rows_)
            throw std::invalid_argument(
                "SymbolicMatrixNM multiply: inner dimensions mismatch ("
                + std::to_string(cols_) + " vs " + std::to_string(o.rows_) + ")"
            );
        SymbolicMatrixNM r(rows_, o.cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < o.cols_; ++j) {
                r.data_[i][j] = data_[i][0] * o.data_[0][j];
                for (std::size_t k = 1; k < cols_; ++k)
                    r.data_[i][j] = r.data_[i][j] + data_[i][k] * o.data_[k][j];
            }
        return r;
    }
    vectors::SymbolicVectorN operator*(const vectors::SymbolicVectorN& v) const {
        if (v.dimension() != cols_)
            throw std::invalid_argument(
                "SymbolicMatrixNM * vector: dimension mismatch ("
                + std::to_string(cols_) + " vs " + std::to_string(v.dimension()) + ")"
            );
        vectors::SymbolicVectorN r(rows_);
        for (std::size_t i = 0; i < rows_; ++i) {
            r[i] = data_[i][0] * v[0];
            for (std::size_t j = 1; j < cols_; ++j)
                r[i] = r[i] + data_[i][j] * v[j];
        }
        return r;
    }
    SymbolicMatrixNM hadamard(const SymbolicMatrixNM& o) const {
        assert_same_shape(o);
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] * o.data_[i][j];
        return r;
    }
    cas::Expression trace() const {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::trace requires a square matrix");
        cas::Expression t = data_[0][0];
        for (std::size_t i = 1; i < rows_; ++i) t = t + data_[i][i];
        return t;
    }
    cas::Expression frobenius_norm_squared() const {
        cas::Expression s = data_[0][0] * data_[0][0];
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                if (i > 0 || j > 0)
                    s = s + data_[i][j] * data_[i][j];
        return s;
    }
    cas::Expression frobenius_norm() const { return SQRT(frobenius_norm_squared()); }
    SymbolicMatrixNM submatrix(std::size_t dr, std::size_t dc) const {
        SymbolicMatrixNM m(rows_ - 1, cols_ - 1);
        std::size_t mr = 0;
        for (std::size_t r = 0; r < rows_; ++r) {
            if (r == dr) continue;
            std::size_t mc = 0;
            for (std::size_t c = 0; c < cols_; ++c) {
                if (c == dc) continue;
                m.data_[mr][mc++] = data_[r][c];
            }
            ++mr;
        }
        return m;
    }
    cas::Expression determinant() const {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::determinant requires a square matrix");
        if (rows_ == 1) return data_[0][0];
        if (rows_ == 2) return data_[0][0] * data_[1][1] - data_[0][1] * data_[1][0];
        cas::Expression det = cas::Const(0.0);
        for (std::size_t c = 0; c < cols_; ++c) {
            cas::Expression cof = submatrix(0, c).determinant();
            cas::Expression term = data_[0][c] * cof;
            det = (c % 2 == 0) ? det + term : det - term;
        }
        return det;
    }
    cas::Expression cofactor(std::size_t r, std::size_t c) const {
        cas::Expression minor_det = submatrix(r, c).determinant();
        return ((r + c) % 2 == 0) ? minor_det : -minor_det;
    }
    SymbolicMatrixNM cofactor_matrix() const {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::cofactor_matrix requires a square matrix");
        SymbolicMatrixNM m(rows_, cols_);
        for (std::size_t r = 0; r < rows_; ++r)
            for (std::size_t c = 0; c < cols_; ++c)
                m.data_[r][c] = cofactor(r, c);
        return m;
    }
    SymbolicMatrixNM adjugate() const { return cofactor_matrix().transpose(); }
    SymbolicMatrixNM inverse()  const { return adjugate() / determinant(); }
    SymbolicMatrixNM differentiate(const std::string& var) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = DIFFERENTIATE(data_[i][j], var);
        return r;
    }
    SymbolicMatrixNM differentiate_iterative(const std::string& var, unsigned n) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = DIFFERENTIATE_ITERATIVE(data_[i][j], var, n);
        return r;
    }
    SymbolicMatrixNM differentiate_recursive(const std::string& var, unsigned n) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = DIFFERENTIATE_RECURSIVE(data_[i][j], var, n);
        return r;
    }
    SymbolicMatrixNM substitute(const std::string& var, double val) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, val);
        return r;
    }
    SymbolicMatrixNM substitute(const std::string& var, const cas::Expression& repl) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, repl);
        return r;
    }
    SymbolicMatrixNM substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicMatrixNM r(data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < rows_; ++i)
                for (std::size_t j = 0; j < cols_; ++j)
                    r.data_[i][j] = SUBSTITUTE(r.data_[i][j], nm, v);
        return r;
    }
    SymbolicMatrixNM partial_evaluate(const std::string& var, double val) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = PARTIAL_EVALUATE(data_[i][j], var, val);
        return r;
    }
    SymbolicMatrixNM partial_evaluate(const std::string& var, const cas::Expression& repl) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = PARTIAL_EVALUATE(data_[i][j], var, repl);
        return r;
    }
    SymbolicMatrixNM partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const {
        SymbolicMatrixNM r(data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < rows_; ++i)
                for (std::size_t j = 0; j < cols_; ++j)
                    r.data_[i][j] = PARTIAL_EVALUATE(r.data_[i][j], nm, v);
        return r;
    }
    SymbolicMatrixNM simplify() const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = SIMPLIFY(data_[i][j]);
        return r;
    }
    SymbolicMatrixNM full_simplify(const cas::RewriterConfig& cfg = {}) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = FULL_SIMPLIFY(data_[i][j], cfg);
        return r;
    }
    SymbolicMatrixNM rewrite(const cas::RewriterConfig& cfg = {}) const {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = REWRITE(data_[i][j], cfg);
        return r;
    }
    std::vector<std::vector<double>> evaluate(const std::unordered_map<std::string, double>& vals) const {
        std::vector<std::vector<double>> out(rows_, std::vector<double>(cols_));
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }
    std::vector<std::vector<double>> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        std::vector<std::vector<double>> out(rows_, std::vector<double>(cols_));
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }
    static SymbolicMatrixNM from(const SymbolicMatrix2x2& m) {
        SymbolicMatrixNM r(2, 2);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }
    static SymbolicMatrixNM from(const SymbolicMatrix3x3& m) {
        SymbolicMatrixNM r(3, 3);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }
    static SymbolicMatrixNM from(const SymbolicMatrix4x4& m) {
        SymbolicMatrixNM r(4, 4);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }
    static SymbolicMatrixNM from(const SymbolicMatrixN& m) {
        std::size_t n = m.dimension();
        SymbolicMatrixNM r(n, n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }
    SymbolicMatrixN to_square() const {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::to_square requires a square matrix");
        return SymbolicMatrixN(rows_, data_);
    }
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

    void assert_same_shape(const SymbolicMatrixNM& o) const {
        if (rows_ != o.rows_ || cols_ != o.cols_)
            throw std::invalid_argument(
                "SymbolicMatrixNM: shape mismatch ("
                + std::to_string(rows_) + "x" + std::to_string(cols_)
                + " vs "
                + std::to_string(o.rows_) + "x" + std::to_string(o.cols_) + ")");
    }
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