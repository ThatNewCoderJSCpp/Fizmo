#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {

vectors::SymbolicVector4 GRADIENT(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    return { DIFFERENTIATE(f, xvar), DIFFERENTIATE(f, yvar), DIFFERENTIATE(f, zvar), DIFFERENTIATE(f, wvar) };
}

vectors::SymbolicVectorN GRADIENT(const Expression& f, const std::vector<std::string>& vars) {
    std::vector<Expression> partials;
    partials.reserve(vars.size());
    for (auto& v : vars) partials.push_back(DIFFERENTIATE(f, v));
    return vectors::SymbolicVectorN(std::move(partials));
}

matrices::SymbolicMatrix2x2 HESSIAN(const Expression& f, const std::string& xvar, const std::string& yvar) {
    Expression dx = DIFFERENTIATE(f, xvar);
    Expression dy = DIFFERENTIATE(f, yvar);
    return {
        DIFFERENTIATE(dx, xvar), DIFFERENTIATE(dx, yvar),
        DIFFERENTIATE(dy, xvar), DIFFERENTIATE(dy, yvar)
    };
}

matrices::SymbolicMatrix3x3 HESSIAN(const Expression& f, const std::string& xvar, const std::string& yvar, const std::string& zvar) {
    Expression dx = DIFFERENTIATE(f, xvar);
    Expression dy = DIFFERENTIATE(f, yvar);
    Expression dz = DIFFERENTIATE(f, zvar);
    matrices::SymbolicMatrix3x3 H;
    auto& d = H.get_data();
    d[0][0] = DIFFERENTIATE(dx, xvar); d[0][1] = DIFFERENTIATE(dx, yvar); d[0][2] = DIFFERENTIATE(dx, zvar);
    d[1][0] = DIFFERENTIATE(dy, xvar); d[1][1] = DIFFERENTIATE(dy, yvar); d[1][2] = DIFFERENTIATE(dy, zvar);
    d[2][0] = DIFFERENTIATE(dz, xvar); d[2][1] = DIFFERENTIATE(dz, yvar); d[2][2] = DIFFERENTIATE(dz, zvar);
    return H;
}

matrices::SymbolicMatrix4x4 HESSIAN(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    Expression dx = DIFFERENTIATE(f, xvar);
    Expression dy = DIFFERENTIATE(f, yvar);
    Expression dz = DIFFERENTIATE(f, zvar);
    Expression dw = DIFFERENTIATE(f, wvar);
    matrices::SymbolicMatrix4x4 H;
    auto& d = H.get_data();
    d[0][0] = DIFFERENTIATE(dx, xvar); d[0][1] = DIFFERENTIATE(dx, yvar);
    d[0][2] = DIFFERENTIATE(dx, zvar); d[0][3] = DIFFERENTIATE(dx, wvar);
    d[1][0] = DIFFERENTIATE(dy, xvar); d[1][1] = DIFFERENTIATE(dy, yvar);
    d[1][2] = DIFFERENTIATE(dy, zvar); d[1][3] = DIFFERENTIATE(dy, wvar);
    d[2][0] = DIFFERENTIATE(dz, xvar); d[2][1] = DIFFERENTIATE(dz, yvar);
    d[2][2] = DIFFERENTIATE(dz, zvar); d[2][3] = DIFFERENTIATE(dz, wvar);
    d[3][0] = DIFFERENTIATE(dw, xvar); d[3][1] = DIFFERENTIATE(dw, yvar);
    d[3][2] = DIFFERENTIATE(dw, zvar); d[3][3] = DIFFERENTIATE(dw, wvar);
    return H;
}

matrices::SymbolicMatrixN HESSIAN(const Expression& f, const std::vector<std::string>& vars) {
    std::size_t n = vars.size();
    std::vector<Expression> partials;
    partials.reserve(n);
    for (auto& v : vars) partials.push_back(DIFFERENTIATE(f, v));
    matrices::SymbolicMatrixN H(n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
            H.at(i, j) = DIFFERENTIATE(partials[i], vars[j]);
    return H;
}

Expression LAPLACIAN(const Expression& f, const std::string& xvar, const std::string& yvar) {
    return DIFFERENTIATE(DIFFERENTIATE(f, xvar), xvar) + DIFFERENTIATE(DIFFERENTIATE(f, yvar), yvar);
}

Expression LAPLACIAN(const Expression& f, const std::string& xvar, const std::string& yvar, const std::string& zvar) {
    return DIFFERENTIATE(DIFFERENTIATE(f, xvar), xvar) + DIFFERENTIATE(DIFFERENTIATE(f, yvar), yvar) + DIFFERENTIATE(DIFFERENTIATE(f, zvar), zvar);
}

Expression LAPLACIAN(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    return DIFFERENTIATE(DIFFERENTIATE(f, xvar), xvar) + DIFFERENTIATE(DIFFERENTIATE(f, yvar), yvar) + DIFFERENTIATE(DIFFERENTIATE(f, zvar), zvar) + DIFFERENTIATE(DIFFERENTIATE(f, wvar), wvar);
}

Expression LAPLACIAN(const Expression& f, const std::vector<std::string>& vars) {
    Expression lap = DIFFERENTIATE(DIFFERENTIATE(f, vars[0]), vars[0]);
    for (std::size_t i = 1; i < vars.size(); ++i) lap = lap + DIFFERENTIATE(DIFFERENTIATE(f, vars[i]), vars[i]);
    return lap;
}

matrices::SymbolicMatrix2x2 JACOBIAN(const Expression& fx, const Expression& fy, const std::string& xvar, const std::string& yvar) {
    return {
        DIFFERENTIATE(fx, xvar), DIFFERENTIATE(fx, yvar),
        DIFFERENTIATE(fy, xvar), DIFFERENTIATE(fy, yvar)
    };
}

matrices::SymbolicMatrix3x3 JACOBIAN(
    const Expression& fx,
    const Expression& fy,
    const Expression& fz,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
) {
    matrices::SymbolicMatrix3x3 J;
    auto& d = J.get_data();
    d[0][0] = DIFFERENTIATE(fx, xvar); d[0][1] = DIFFERENTIATE(fx, yvar); d[0][2] = DIFFERENTIATE(fx, zvar);
    d[1][0] = DIFFERENTIATE(fy, xvar); d[1][1] = DIFFERENTIATE(fy, yvar); d[1][2] = DIFFERENTIATE(fy, zvar);
    d[2][0] = DIFFERENTIATE(fz, xvar); d[2][1] = DIFFERENTIATE(fz, yvar); d[2][2] = DIFFERENTIATE(fz, zvar);
    return J;
}

vectors::SymbolicVector3 JACOBIAN(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
) {
    vectors::SymbolicVector3 v;
    v.x = DIFFERENTIATE(f, xvar);
    v.y = DIFFERENTIATE(f, yvar);
    v.z = DIFFERENTIATE(f, zvar);
    return v;
}

matrices::SymbolicMatrix4x4 JACOBIAN(
    const Expression& fx, const Expression& fy,
    const Expression& fz, const Expression& fw,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    matrices::SymbolicMatrix4x4 J;
    auto& d = J.get_data();
    d[0][0] = DIFFERENTIATE(fx, xvar); d[0][1] = DIFFERENTIATE(fx, yvar);
    d[0][2] = DIFFERENTIATE(fx, zvar); d[0][3] = DIFFERENTIATE(fx, wvar);
    d[1][0] = DIFFERENTIATE(fy, xvar); d[1][1] = DIFFERENTIATE(fy, yvar);
    d[1][2] = DIFFERENTIATE(fy, zvar); d[1][3] = DIFFERENTIATE(fy, wvar);
    d[2][0] = DIFFERENTIATE(fz, xvar); d[2][1] = DIFFERENTIATE(fz, yvar);
    d[2][2] = DIFFERENTIATE(fz, zvar); d[2][3] = DIFFERENTIATE(fz, wvar);
    d[3][0] = DIFFERENTIATE(fw, xvar); d[3][1] = DIFFERENTIATE(fw, yvar);
    d[3][2] = DIFFERENTIATE(fw, zvar); d[3][3] = DIFFERENTIATE(fw, wvar);
    return J;
}

vectors::SymbolicVector4 JACOBIAN(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    return {
        DIFFERENTIATE(f, xvar), DIFFERENTIATE(f, yvar),
        DIFFERENTIATE(f, zvar), DIFFERENTIATE(f, wvar)
    };
}

matrices::SymbolicMatrixN JACOBIAN(const vectors::SymbolicVectorN& F, const std::vector<std::string>& vars) {
    std::size_t n = F.dimension();
    matrices::SymbolicMatrixN J(n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < vars.size(); ++j)
            J.at(i, j) = DIFFERENTIATE(F[i], vars[j]);
    return J;
}

matrices::SymbolicMatrixNM JACOBIAN(const std::vector<Expression>& components, const std::vector<std::string>& vars) {
    std::size_t m = components.size();
    std::size_t n = vars.size();
    matrices::SymbolicMatrixNM J(m, n);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < n; ++j)
            J.at(i, j) = DIFFERENTIATE(components[i], vars[j]);
    return J;
}

vectors::SymbolicVectorN JACOBIAN(
    const Expression& f,
    const std::vector<std::string>& vars
) {
    std::size_t n = vars.size();
    vectors::SymbolicVectorN v(n);
    for (std::size_t j = 0; j < n; ++j) v.at(j) = DIFFERENTIATE(f, vars[j]);
    return v;
}

Expression DIRECTIONAL_DERIVATIVE(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar,
    const vectors::SymbolicVector3& direction
) {
    return direction.x * DIFFERENTIATE(f, xvar) + direction.y * DIFFERENTIATE(f, yvar) + direction.z * DIFFERENTIATE(f, zvar);
}

Expression DIRECTIONAL_DERIVATIVE(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar,
    const vectors::SymbolicVector4& direction
) {
    return direction.x * DIFFERENTIATE(f, xvar) + direction.y * DIFFERENTIATE(f, yvar) + direction.z * DIFFERENTIATE(f, zvar) + direction.w * DIFFERENTIATE(f, wvar);
}

Expression DIRECTIONAL_DERIVATIVE(const Expression& f, const std::vector<std::string>& vars, const vectors::SymbolicVectorN& direction) {
    assert(vars.size() == direction.dimension());
    Expression dd = direction[0] * DIFFERENTIATE(f, vars[0]);
    for (std::size_t i = 1; i < vars.size(); ++i) dd = dd + direction[i] * DIFFERENTIATE(f, vars[i]);
    return dd;
}

Expression DIRECTIONAL_DERIVATIVE_UNIT(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const vectors::SymbolicVector2& direction
) {
    const auto dir = direction.normalized();
    return dir.x * DIFFERENTIATE(f, xvar) + dir.y * DIFFERENTIATE(f, yvar);
}

Expression DIRECTIONAL_DERIVATIVE_UNIT(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar,
    const vectors::SymbolicVector3& direction
) {
    const auto dir = direction.normalized();
    return dir.x * DIFFERENTIATE(f, xvar) + dir.y * DIFFERENTIATE(f, yvar) + dir.z * DIFFERENTIATE(f, zvar);
}

Expression DIRECTIONAL_DERIVATIVE_UNIT(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar,
    const vectors::SymbolicVector4& direction
) {
    const auto dir = direction.normalized();
    return dir.x * DIFFERENTIATE(f, xvar) + dir.y * DIFFERENTIATE(f, yvar) + dir.z * DIFFERENTIATE(f, zvar) + dir.w * DIFFERENTIATE(f, wvar);
}

Expression DIRECTIONAL_DERIVATIVE_UNIT(const Expression& f, const std::vector<std::string>& vars, const vectors::SymbolicVectorN& direction) {
    assert(vars.size() == direction.dimension());
    const auto dir = direction.normalized();
    Expression dd = dir[0] * DIFFERENTIATE(f, vars[0]);
    for (std::size_t i = 1; i < vars.size(); ++i) dd = dd + dir[i] * DIFFERENTIATE(f, vars[i]);
    return dd;
}

Expression DIVERGENCE(
    const Expression& fx, const Expression& fy,
    const Expression& fz, const Expression& fw,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    return DIFFERENTIATE(fx, xvar) + DIFFERENTIATE(fy, yvar) + DIFFERENTIATE(fz, zvar) + DIFFERENTIATE(fw, wvar);
}

Expression DIVERGENCE(const vectors::SymbolicVectorN& F, const std::vector<std::string>& vars) {
    assert(F.dimension() == vars.size());
    Expression div = DIFFERENTIATE(F[0], vars[0]);
    for (std::size_t i = 1; i < vars.size(); ++i) div = div + DIFFERENTIATE(F[i], vars[i]);
    return div;
}

vectors::SymbolicVector3 CURL(
    const Expression& fx,
    const Expression& fy,
    const Expression& fz,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
) {
    return {
        DIFFERENTIATE(fz, yvar) - DIFFERENTIATE(fy, zvar),
        DIFFERENTIATE(fx, zvar) - DIFFERENTIATE(fz, xvar),
        DIFFERENTIATE(fy, xvar) - DIFFERENTIATE(fx, yvar)
    };
}

} // namespace cas
} // namespace math
} // namespace fizmo
