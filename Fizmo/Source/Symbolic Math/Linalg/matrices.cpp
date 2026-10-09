#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace matrices {

SymbolicMatrix2x2::SymbolicMatrix2x2(cas::Expression a, cas::Expression b, cas::Expression c, cas::Expression d) { data[0][0]=std::move(a); data[0][1]=std::move(b); data[1][0]=std::move(c); data[1][1]=std::move(d); }

auto SymbolicMatrix2x2::operator+(const SymbolicMatrix2x2& o) const -> SymbolicMatrix2x2 { return { data[0][0]+o.data[0][0], data[0][1]+o.data[0][1], data[1][0]+o.data[1][0], data[1][1]+o.data[1][1] }; }

auto SymbolicMatrix2x2::operator-(const SymbolicMatrix2x2& o) const -> SymbolicMatrix2x2 { return { data[0][0]-o.data[0][0], data[0][1]-o.data[0][1], data[1][0]-o.data[1][0], data[1][1]-o.data[1][1] }; }

auto SymbolicMatrix2x2::operator*(const SymbolicMatrix2x2& o) const -> SymbolicMatrix2x2 {
        return { data[0][0]*o.data[0][0]+data[0][1]*o.data[1][0], data[0][0]*o.data[0][1]+data[0][1]*o.data[1][1],
                 data[1][0]*o.data[0][0]+data[1][1]*o.data[1][0], data[1][0]*o.data[0][1]+data[1][1]*o.data[1][1] };
    }

auto SymbolicMatrix2x2::hadamard(const SymbolicMatrix2x2& o) const -> SymbolicMatrix2x2 { return { data[0][0]*o.data[0][0], data[0][1]*o.data[0][1], data[1][0]*o.data[1][0], data[1][1]*o.data[1][1] }; }

auto SymbolicMatrix2x2::differentiate(const std::string& var) const -> SymbolicMatrix2x2 { return { DIFFERENTIATE(data[0][0],var),            DIFFERENTIATE(data[0][1],var),            DIFFERENTIATE(data[1][0],var),            DIFFERENTIATE(data[1][1],var)            }; }

auto SymbolicMatrix2x2::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicMatrix2x2 { return { DIFFERENTIATE_ITERATIVE(data[0][0],var,n), DIFFERENTIATE_ITERATIVE(data[0][1],var,n), DIFFERENTIATE_ITERATIVE(data[1][0],var,n), DIFFERENTIATE_ITERATIVE(data[1][1],var,n) }; }

auto SymbolicMatrix2x2::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicMatrix2x2 { return { DIFFERENTIATE_RECURSIVE(data[0][0],var,n), DIFFERENTIATE_RECURSIVE(data[0][1],var,n), DIFFERENTIATE_RECURSIVE(data[1][0],var,n), DIFFERENTIATE_RECURSIVE(data[1][1],var,n) }; }

auto SymbolicMatrix2x2::substitute(const std::string& var, double val) const -> SymbolicMatrix2x2 { return { SUBSTITUTE(data[0][0],var,val), SUBSTITUTE(data[0][1],var,val), SUBSTITUTE(data[1][0],var,val), SUBSTITUTE(data[1][1],var,val) }; }

auto SymbolicMatrix2x2::substitute(const std::string& var, const cas::Expression& r) const -> SymbolicMatrix2x2 { return { SUBSTITUTE(data[0][0],var,r),   SUBSTITUTE(data[0][1],var,r),   SUBSTITUTE(data[1][0],var,r),   SUBSTITUTE(data[1][1],var,r)   }; }

auto SymbolicMatrix2x2::partial_evaluate(const std::string& var, double val) const -> SymbolicMatrix2x2 { return { PARTIAL_EVALUATE(data[0][0],var,val), PARTIAL_EVALUATE(data[0][1],var,val), PARTIAL_EVALUATE(data[1][0],var,val), PARTIAL_EVALUATE(data[1][1],var,val) }; }

auto SymbolicMatrix2x2::partial_evaluate(const std::string& var, const cas::Expression& r) const -> SymbolicMatrix2x2 { return { PARTIAL_EVALUATE(data[0][0],var,r),   PARTIAL_EVALUATE(data[0][1],var,r),   PARTIAL_EVALUATE(data[1][0],var,r),   PARTIAL_EVALUATE(data[1][1],var,r)   }; }

auto SymbolicMatrix2x2::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicMatrix2x2 { return { REWRITE(data[0][0],cfg),       REWRITE(data[0][1],cfg),       REWRITE(data[1][0],cfg),       REWRITE(data[1][1],cfg)       }; }

auto SymbolicMatrix2x2::simplify() const -> SymbolicMatrix2x2 { return { SIMPLIFY(data[0][0]),          SIMPLIFY(data[0][1]),          SIMPLIFY(data[1][0]),          SIMPLIFY(data[1][1])          }; }

auto SymbolicMatrix2x2::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicMatrix2x2 { return { FULL_SIMPLIFY(data[0][0],cfg), FULL_SIMPLIFY(data[0][1],cfg), FULL_SIMPLIFY(data[1][0],cfg), FULL_SIMPLIFY(data[1][1],cfg) }; }

auto SymbolicMatrix2x2::evaluate(const std::unordered_map<std::string, double>& vals) const -> Matrix2d {
        Matrix2d m;
        for (int r = 0; r < 2; ++r) for (int c = 0; c < 2; ++c) m.at(r, c) = data[r][c].evaluate(vals);
        return m;
    }

auto SymbolicMatrix2x2::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const -> Matrix2d {
        Matrix2d m;
        for (int r = 0; r < 2; ++r) for (int c = 0; c < 2; ++c) m.at(r, c) = data[r][c].evaluate(vals);
        return m;
    }

SymbolicMatrix3x3::SymbolicMatrix3x3(cas::Expression a00, cas::Expression a01, cas::Expression a02,
                      cas::Expression a10, cas::Expression a11, cas::Expression a12,
                      cas::Expression a20, cas::Expression a21, cas::Expression a22) {
        data[0]={std::move(a00),std::move(a01),std::move(a02)};
        data[1]={std::move(a10),std::move(a11),std::move(a12)};
        data[2]={std::move(a20),std::move(a21),std::move(a22)};
    }

SymbolicMatrix3x3::SymbolicMatrix3x3(const std::string& a00, const std::string& a01, const std::string& a02,
                      const std::string& a10, const std::string& a11, const std::string& a12,
                      const std::string& a20, const std::string& a21, const std::string& a22) : SymbolicMatrix3x3(cas::VARIABLE(a00),cas::VARIABLE(a01),cas::VARIABLE(a02),
                            cas::VARIABLE(a10),cas::VARIABLE(a11),cas::VARIABLE(a12),
                            cas::VARIABLE(a20),cas::VARIABLE(a21),cas::VARIABLE(a22)) {}

auto SymbolicMatrix3x3::identity() -> SymbolicMatrix3x3 { return { cas::Const(1),cas::Const(0),cas::Const(0), cas::Const(0),cas::Const(1),cas::Const(0), cas::Const(0),cas::Const(0),cas::Const(1) }; }

auto SymbolicMatrix3x3::zero() -> SymbolicMatrix3x3 { return { cas::Const(0),cas::Const(0),cas::Const(0), cas::Const(0),cas::Const(0),cas::Const(0), cas::Const(0),cas::Const(0),cas::Const(0) }; }

auto SymbolicMatrix3x3::diagonal(const cas::Expression& a, const cas::Expression& b, const cas::Expression& c) -> SymbolicMatrix3x3 { return { a,cas::Const(0),cas::Const(0), cas::Const(0),b,cas::Const(0), cas::Const(0),cas::Const(0),c }; }

auto SymbolicMatrix3x3::operator+(const SymbolicMatrix3x3& o) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]+o.data[r][c]; return m; }

auto SymbolicMatrix3x3::operator-(const SymbolicMatrix3x3& o) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]-o.data[r][c]; return m; }

auto SymbolicMatrix3x3::operator-() const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=-data[r][c]; return m; }

auto SymbolicMatrix3x3::operator*(const cas::Expression& s) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]*s; return m; }

auto SymbolicMatrix3x3::operator*(double s) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]*s; return m; }

auto SymbolicMatrix3x3::operator/(const cas::Expression& s) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]/s; return m; }

auto SymbolicMatrix3x3::operator/(double s) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]/s; return m; }

auto SymbolicMatrix3x3::operator*(const SymbolicMatrix3x3& o) const -> SymbolicMatrix3x3 {
        SymbolicMatrix3x3 m;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
            m.data[r][c] = data[r][0]*o.data[0][c];
            for (int k=1;k<3;++k) m.data[r][c] = m.data[r][c] + data[r][k]*o.data[k][c];
        }
        return m;
    }

auto SymbolicMatrix3x3::operator*(const vectors::SymbolicVector3& v) const -> vectors::SymbolicVector3 {
        return { data[0][0]*v.x+data[0][1]*v.y+data[0][2]*v.z,
                 data[1][0]*v.x+data[1][1]*v.y+data[1][2]*v.z,
                 data[2][0]*v.x+data[2][1]*v.y+data[2][2]*v.z };
    }

auto SymbolicMatrix3x3::transpose() const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[c][r]=data[r][c]; return m; }

auto SymbolicMatrix3x3::cofactor(int row, int col) const -> cas::Expression {
        std::array<std::array<cas::Expression,2>,2> sub{};
        int sr=0;
        for (int r=0;r<3;++r) { if(r==row) continue; int sc=0; for(int c=0;c<3;++c) { if(c==col) continue; sub[sr][sc]=data[r][c]; ++sc; } ++sr; }
        cas::Expression minor_det = sub[0][0]*sub[1][1] - sub[0][1]*sub[1][0];
        return ((row+col)%2==0) ? minor_det : -minor_det;
    }

auto SymbolicMatrix3x3::cofactor_matrix() const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=cofactor(r,c); return m; }

auto SymbolicMatrix3x3::hadamard(const SymbolicMatrix3x3& o) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=data[r][c]*o.data[r][c]; return m; }

auto SymbolicMatrix3x3::differentiate(const std::string& var) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=DIFFERENTIATE(data[r][c],var);            return m; }

auto SymbolicMatrix3x3::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=DIFFERENTIATE_ITERATIVE(data[r][c],var,n); return m; }

auto SymbolicMatrix3x3::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=DIFFERENTIATE_RECURSIVE(data[r][c],var,n); return m; }

auto SymbolicMatrix3x3::substitute(const std::string& var, double val) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=SUBSTITUTE(data[r][c],var,val); return m; }

auto SymbolicMatrix3x3::substitute(const std::string& var, const cas::Expression& e) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=SUBSTITUTE(data[r][c],var,e);   return m; }

auto SymbolicMatrix3x3::partial_evaluate(const std::string& var, double val) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=PARTIAL_EVALUATE(data[r][c],var,val); return m; }

auto SymbolicMatrix3x3::partial_evaluate(const std::string& var, const cas::Expression& e) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=PARTIAL_EVALUATE(data[r][c],var,e);   return m; }

auto SymbolicMatrix3x3::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=REWRITE(data[r][c],cfg);       return m; }

auto SymbolicMatrix3x3::simplify() const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=SIMPLIFY(data[r][c]);           return m; }

auto SymbolicMatrix3x3::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicMatrix3x3 { SymbolicMatrix3x3 m; for(int r=0;r<3;++r) for(int c=0;c<3;++c) m.data[r][c]=FULL_SIMPLIFY(data[r][c],cfg);  return m; }

auto SymbolicMatrix3x3::evaluate(const std::unordered_map<std::string, double>& vals) const -> Matrix3d {
        Matrix3d m;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) m.at(r, c)=data[r][c].evaluate(vals);
        return m;
    }

auto SymbolicMatrix3x3::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const -> Matrix3d {
        Matrix3d m;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) m.at(r, c)=data[r][c].evaluate(vals);
        return m;
    }

SymbolicMatrix4x4::SymbolicMatrix4x4(cas::Expression a00, cas::Expression a01, cas::Expression a02, cas::Expression a03,
                      cas::Expression a10, cas::Expression a11, cas::Expression a12, cas::Expression a13,
                      cas::Expression a20, cas::Expression a21, cas::Expression a22, cas::Expression a23,
                      cas::Expression a30, cas::Expression a31, cas::Expression a32, cas::Expression a33) {
        data[0] = { std::move(a00), std::move(a01), std::move(a02), std::move(a03) };
        data[1] = { std::move(a10), std::move(a11), std::move(a12), std::move(a13) };
        data[2] = { std::move(a20), std::move(a21), std::move(a22), std::move(a23) };
        data[3] = { std::move(a30), std::move(a31), std::move(a32), std::move(a33) };
    }

auto SymbolicMatrix4x4::identity() -> SymbolicMatrix4x4 {
        return {
            cas::Const(1),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(1),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(1),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(1)
        };
    }

auto SymbolicMatrix4x4::zero() -> SymbolicMatrix4x4 {
        return {
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0),
            cas::Const(0),cas::Const(0),cas::Const(0),cas::Const(0)
        };
    }

auto SymbolicMatrix4x4::operator+(const SymbolicMatrix4x4& o) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] + o.data[i][j];
        return r;
    }

auto SymbolicMatrix4x4::operator-(const SymbolicMatrix4x4& o) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] - o.data[i][j];
        return r;
    }

auto SymbolicMatrix4x4::operator-() const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = -data[i][j];
        return r;
    }

auto SymbolicMatrix4x4::operator*(const cas::Expression& s) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] * s;
        return r;
    }

auto SymbolicMatrix4x4::operator*(const double s) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] * cas::Const(s);
        return r;
    }

auto SymbolicMatrix4x4::operator/(const cas::Expression& s) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[i][j] = data[i][j] / s;
        return r;
    }

auto SymbolicMatrix4x4::operator*(const SymbolicMatrix4x4& o) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r = zero();
        for (int i=0;i<4;i++)
            for (int j=0;j<4;j++)
                for (int k=0;k<4;k++)
                    r.data[i][j] = r.data[i][j] + data[i][k] * o.data[k][j];
        return r;
    }

auto SymbolicMatrix4x4::operator*(const vectors::SymbolicVector4& v) const -> vectors::SymbolicVector4 {
        return {
            data[0][0]*v.x + data[0][1]*v.y + data[0][2]*v.z + data[0][3]*v.w,
            data[1][0]*v.x + data[1][1]*v.y + data[1][2]*v.z + data[1][3]*v.w,
            data[2][0]*v.x + data[2][1]*v.y + data[2][2]*v.z + data[2][3]*v.w,
            data[3][0]*v.x + data[3][1]*v.y + data[3][2]*v.z + data[3][3]*v.w
        };
    }

auto SymbolicMatrix4x4::transpose() const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++) r.data[j][i] = data[i][j];
        return r;
    }

auto SymbolicMatrix4x4::determinant() const -> cas::Expression {
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

auto SymbolicMatrix4x4::inverse() const -> SymbolicMatrix4x4 {
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

auto SymbolicMatrix4x4::differentiate(const std::string& var) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = DIFFERENTIATE(data[i][j], var);
        return r;
    }

auto SymbolicMatrix4x4::substitute(const std::string& var, double val) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = SUBSTITUTE(data[i][j], var, val);
        return r;
    }

auto SymbolicMatrix4x4::substitute(const std::string& var, const cas::Expression& e) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = SUBSTITUTE(data[i][j], var, e);
        return r;
    }

auto SymbolicMatrix4x4::partial_evaluate(const std::string& var, double val) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = PARTIAL_EVALUATE(data[i][j], var, val);
        return r;
    }

auto SymbolicMatrix4x4::simplify() const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = SIMPLIFY(data[i][j]);
        return r;
    }

auto SymbolicMatrix4x4::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = FULL_SIMPLIFY(data[i][j], cfg);
        return r;
    }

auto SymbolicMatrix4x4::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicMatrix4x4 {
        SymbolicMatrix4x4 r;
        for (int i=0;i<4;i++) for(int j=0;j<4;j++)
            r.data[i][j] = REWRITE(data[i][j], cfg);
        return r;
    }

auto SymbolicMatrixN::identity(std::size_t n) -> SymbolicMatrixN {
        SymbolicMatrixN m(n);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j)
                m.data_[i][j] = cas::Const(i == j ? 1.0 : 0.0);
        }
        return m;
    }

auto SymbolicMatrixN::zero(std::size_t n) -> SymbolicMatrixN {
        SymbolicMatrixN m(n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                m.data_[i][j] = cas::Const(0.0);
        return m;
    }

auto SymbolicMatrixN::diagonal(const std::vector<cas::Expression>& diag) -> SymbolicMatrixN {
        std::size_t n = diag.size();
        auto m = zero(n);
        for (std::size_t i = 0; i < n; ++i) m.data_[i][i] = diag[i];
        return m;
    }

auto SymbolicMatrixN::outer(const vectors::SymbolicVectorN& a, const vectors::SymbolicVectorN& b) -> SymbolicMatrixN {
        std::size_t n = a.dimension();
        SymbolicMatrixN m(n);
        for (std::size_t r = 0; r < n; ++r)
            for (std::size_t c = 0; c < n; ++c)
                m.data_[r][c] = a[r] * b[c];
        return m;
    }

auto SymbolicMatrixN::operator+(const SymbolicMatrixN& o) const -> SymbolicMatrixN {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] + o.data_[i][j];
        return r;
    }

auto SymbolicMatrixN::operator-(const SymbolicMatrixN& o) const -> SymbolicMatrixN {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] - o.data_[i][j];
        return r;
    }

auto SymbolicMatrixN::operator-() const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = -data_[i][j];
        return r;
    }

auto SymbolicMatrixN::operator*(const cas::Expression& s) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }

auto SymbolicMatrixN::operator*(double s) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }

auto SymbolicMatrixN::operator/(const cas::Expression& s) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }

auto SymbolicMatrixN::operator/(double s) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }

auto SymbolicMatrixN::operator*(const SymbolicMatrixN& o) const -> SymbolicMatrixN {
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

auto SymbolicMatrixN::operator*(const vectors::SymbolicVectorN& v) const -> vectors::SymbolicVectorN {
        assert(v.dimension() == n_);
        vectors::SymbolicVectorN r(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            r[i] = data_[i][0] * v[0];
            for (std::size_t j = 1; j < n_; ++j)
                r[i] = r[i] + data_[i][j] * v[j];
        }
        return r;
    }

auto SymbolicMatrixN::hadamard(const SymbolicMatrixN& o) const -> SymbolicMatrixN {
        assert_same_dim(o);
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = data_[i][j] * o.data_[i][j];
        return r;
    }

auto SymbolicMatrixN::transpose() const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[j][i] = data_[i][j];
        return r;
    }

auto SymbolicMatrixN::determinant() const -> cas::Expression {
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

auto SymbolicMatrixN::submatrix(std::size_t dr, std::size_t dc) const -> SymbolicMatrixN {
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

auto SymbolicMatrixN::cofactor(std::size_t r, std::size_t c) const -> cas::Expression {
        cas::Expression minor_det = submatrix(r, c).determinant();
        return ((r + c) % 2 == 0) ? minor_det : -minor_det;
    }

auto SymbolicMatrixN::cofactor_matrix() const -> SymbolicMatrixN {
        SymbolicMatrixN m(n_);
        for (std::size_t r = 0; r < n_; ++r)
            for (std::size_t c = 0; c < n_; ++c)
                m.data_[r][c] = cofactor(r, c);
        return m;
    }

auto SymbolicMatrixN::differentiate(const std::string& var) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = DIFFERENTIATE(data_[i][j], var);
        return r;
    }

auto SymbolicMatrixN::substitute(const std::string& var, double val) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, val);
        return r;
    }

auto SymbolicMatrixN::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, repl);
        return r;
    }

auto SymbolicMatrixN::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_, data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < n_; ++i)
                for (std::size_t j = 0; j < n_; ++j)
                    r.data_[i][j] = SUBSTITUTE(r.data_[i][j], nm, v);
        return r;
    }

auto SymbolicMatrixN::simplify() const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = SIMPLIFY(data_[i][j]);
        return r;
    }

auto SymbolicMatrixN::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = FULL_SIMPLIFY(data_[i][j], cfg);
        return r;
    }

auto SymbolicMatrixN::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicMatrixN {
        SymbolicMatrixN r(n_);
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                r.data_[i][j] = REWRITE(data_[i][j], cfg);
        return r;
    }

auto SymbolicMatrixN::evaluate(const std::unordered_map<std::string, double>& vals) const -> std::vector<std::vector<double>> {
        std::vector<std::vector<double>> out(n_, std::vector<double>(n_));
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }

auto SymbolicMatrixN::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const -> std::vector<std::vector<double>> {
        std::vector<std::vector<double>> out(n_, std::vector<double>(n_));
        for (std::size_t i = 0; i < n_; ++i)
            for (std::size_t j = 0; j < n_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }

auto SymbolicMatrixN::from(const SymbolicMatrix2x2& m) -> SymbolicMatrixN {
        SymbolicMatrixN r(2);
        for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) r.data_[i][j] = m.get_data()[i][j];
        return r;
    }

auto SymbolicMatrixN::from(const SymbolicMatrix3x3& m) -> SymbolicMatrixN {
        SymbolicMatrixN r(3);
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) r.data_[i][j] = m.get_data()[i][j];
        return r;
    }

auto SymbolicMatrixN::from(const SymbolicMatrix4x4& m) -> SymbolicMatrixN {
        SymbolicMatrixN r(4);
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = 0; j < 4; ++j)
                r.at(i, j) = m.at(i, j);
        return r;
    }

auto SymbolicMatrixN::assert_same_dim(const SymbolicMatrixN& o) const -> void {
        if (n_ != o.n_)
            throw std::invalid_argument("SymbolicMatrixN: dimension mismatch ("
                + std::to_string(n_) + " vs " + std::to_string(o.n_) + ")");
    }

SymbolicMatrixNM::SymbolicMatrixNM(const std::vector<std::vector<cas::Expression>>& d) {
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

SymbolicMatrixNM::SymbolicMatrixNM(
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

SymbolicMatrixNM::SymbolicMatrixNM(std::initializer_list<std::initializer_list<cas::Expression>> rows) {
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

auto SymbolicMatrixNM::zero(std::size_t rows, std::size_t cols) -> SymbolicMatrixNM {
        SymbolicMatrixNM m(rows, cols);
        for (std::size_t i = 0; i < rows; ++i)
            for (std::size_t j = 0; j < cols; ++j)
                m.data_[i][j] = cas::Const(0.0);
        return m;
    }

auto SymbolicMatrixNM::identity(std::size_t n) -> SymbolicMatrixNM {
        SymbolicMatrixNM m(n, n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                m.data_[i][j] = cas::Const(i == j ? 1.0 : 0.0);
        return m;
    }

auto SymbolicMatrixNM::outer(const vectors::SymbolicVectorN& a, const vectors::SymbolicVectorN& b) -> SymbolicMatrixNM {
        std::size_t r = a.dimension(), c = b.dimension();
        SymbolicMatrixNM m(r, c);
        for (std::size_t i = 0; i < r; ++i)
            for (std::size_t j = 0; j < c; ++j)
                m.data_[i][j] = a[i] * b[j];
        return m;
    }

auto SymbolicMatrixNM::col(std::size_t c) const -> vectors::SymbolicVectorN {
        vectors::SymbolicVectorN v(rows_);
        for (std::size_t r = 0; r < rows_; ++r) v[r] = data_[r][c];
        return v;
    }

auto SymbolicMatrixNM::transpose() const -> SymbolicMatrixNM {
        SymbolicMatrixNM m(cols_, rows_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                m.data_[j][i] = data_[i][j];
        return m;
    }

auto SymbolicMatrixNM::operator+(const SymbolicMatrixNM& o) const -> SymbolicMatrixNM {
        assert_same_shape(o);
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] + o.data_[i][j];
        return r;
    }

auto SymbolicMatrixNM::operator-(const SymbolicMatrixNM& o) const -> SymbolicMatrixNM {
        assert_same_shape(o);
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] - o.data_[i][j];
        return r;
    }

auto SymbolicMatrixNM::operator-() const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = -data_[i][j];
        return r;
    }

auto SymbolicMatrixNM::operator*(const cas::Expression& s) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }

auto SymbolicMatrixNM::operator*(double s) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] * s;
        return r;
    }

auto SymbolicMatrixNM::operator/(const cas::Expression& s) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }

auto SymbolicMatrixNM::operator/(double s) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] / s;
        return r;
    }

auto SymbolicMatrixNM::operator*(const SymbolicMatrixNM& o) const -> SymbolicMatrixNM {
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

auto SymbolicMatrixNM::operator*(const vectors::SymbolicVectorN& v) const -> vectors::SymbolicVectorN {
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

auto SymbolicMatrixNM::hadamard(const SymbolicMatrixNM& o) const -> SymbolicMatrixNM {
        assert_same_shape(o);
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = data_[i][j] * o.data_[i][j];
        return r;
    }

auto SymbolicMatrixNM::trace() const -> cas::Expression {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::trace requires a square matrix");
        cas::Expression t = data_[0][0];
        for (std::size_t i = 1; i < rows_; ++i) t = t + data_[i][i];
        return t;
    }

auto SymbolicMatrixNM::frobenius_norm_squared() const -> cas::Expression {
        cas::Expression s = data_[0][0] * data_[0][0];
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                if (i > 0 || j > 0)
                    s = s + data_[i][j] * data_[i][j];
        return s;
    }

auto SymbolicMatrixNM::submatrix(std::size_t dr, std::size_t dc) const -> SymbolicMatrixNM {
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

auto SymbolicMatrixNM::determinant() const -> cas::Expression {
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

auto SymbolicMatrixNM::cofactor(std::size_t r, std::size_t c) const -> cas::Expression {
        cas::Expression minor_det = submatrix(r, c).determinant();
        return ((r + c) % 2 == 0) ? minor_det : -minor_det;
    }

auto SymbolicMatrixNM::cofactor_matrix() const -> SymbolicMatrixNM {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::cofactor_matrix requires a square matrix");
        SymbolicMatrixNM m(rows_, cols_);
        for (std::size_t r = 0; r < rows_; ++r)
            for (std::size_t c = 0; c < cols_; ++c)
                m.data_[r][c] = cofactor(r, c);
        return m;
    }

auto SymbolicMatrixNM::differentiate(const std::string& var) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = DIFFERENTIATE(data_[i][j], var);
        return r;
    }

auto SymbolicMatrixNM::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = DIFFERENTIATE_ITERATIVE(data_[i][j], var, n);
        return r;
    }

auto SymbolicMatrixNM::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = DIFFERENTIATE_RECURSIVE(data_[i][j], var, n);
        return r;
    }

auto SymbolicMatrixNM::substitute(const std::string& var, double val) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, val);
        return r;
    }

auto SymbolicMatrixNM::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = SUBSTITUTE(data_[i][j], var, repl);
        return r;
    }

auto SymbolicMatrixNM::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < rows_; ++i)
                for (std::size_t j = 0; j < cols_; ++j)
                    r.data_[i][j] = SUBSTITUTE(r.data_[i][j], nm, v);
        return r;
    }

auto SymbolicMatrixNM::partial_evaluate(const std::string& var, double val) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = PARTIAL_EVALUATE(data_[i][j], var, val);
        return r;
    }

auto SymbolicMatrixNM::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = PARTIAL_EVALUATE(data_[i][j], var, repl);
        return r;
    }

auto SymbolicMatrixNM::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < rows_; ++i)
                for (std::size_t j = 0; j < cols_; ++j)
                    r.data_[i][j] = PARTIAL_EVALUATE(r.data_[i][j], nm, v);
        return r;
    }

auto SymbolicMatrixNM::simplify() const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = SIMPLIFY(data_[i][j]);
        return r;
    }

auto SymbolicMatrixNM::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = FULL_SIMPLIFY(data_[i][j], cfg);
        return r;
    }

auto SymbolicMatrixNM::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicMatrixNM {
        SymbolicMatrixNM r(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                r.data_[i][j] = REWRITE(data_[i][j], cfg);
        return r;
    }

auto SymbolicMatrixNM::evaluate(const std::unordered_map<std::string, double>& vals) const -> std::vector<std::vector<double>> {
        std::vector<std::vector<double>> out(rows_, std::vector<double>(cols_));
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }

auto SymbolicMatrixNM::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const -> std::vector<std::vector<double>> {
        std::vector<std::vector<double>> out(rows_, std::vector<double>(cols_));
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                out[i][j] = data_[i][j].evaluate(vals);
        return out;
    }

auto SymbolicMatrixNM::from(const SymbolicMatrix2x2& m) -> SymbolicMatrixNM {
        SymbolicMatrixNM r(2, 2);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }

auto SymbolicMatrixNM::from(const SymbolicMatrix3x3& m) -> SymbolicMatrixNM {
        SymbolicMatrixNM r(3, 3);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }

auto SymbolicMatrixNM::from(const SymbolicMatrix4x4& m) -> SymbolicMatrixNM {
        SymbolicMatrixNM r(4, 4);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }

auto SymbolicMatrixNM::from(const SymbolicMatrixN& m) -> SymbolicMatrixNM {
        std::size_t n = m.dimension();
        SymbolicMatrixNM r(n, n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                r.data_[i][j] = m.get_data()[i][j];
        return r;
    }

auto SymbolicMatrixNM::to_square() const -> SymbolicMatrixN {
        if (!is_square()) throw std::domain_error("SymbolicMatrixNM::to_square requires a square matrix");
        return SymbolicMatrixN(rows_, data_);
    }

auto SymbolicMatrixNM::assert_same_shape(const SymbolicMatrixNM& o) const -> void {
        if (rows_ != o.rows_ || cols_ != o.cols_)
            throw std::invalid_argument(
                "SymbolicMatrixNM: shape mismatch ("
                + std::to_string(rows_) + "x" + std::to_string(cols_)
                + " vs "
                + std::to_string(o.rows_) + "x" + std::to_string(o.cols_) + ")");
    }

} // namespace matrices
} // namespace math
} // namespace fizmo
