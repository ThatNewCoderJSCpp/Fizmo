#ifndef FIZMO_MATH_STRUCTURE_CONVENIENCE_HPP
#define FIZMO_MATH_STRUCTURE_CONVENIENCE_HPP

#include "main_convenience.hpp"
#include "../Linalg/vectors.hpp"
#include "../Linalg/matrices.hpp"

namespace fizmo {
namespace math {
namespace cas {

inline vectors::SymbolicVector2 GRADIENT(const Expression& f, const std::string& xvar, const std::string& yvar) { return { DIFFERENTIATE(f, xvar), DIFFERENTIATE(f, yvar) }; }
inline vectors::SymbolicVector3 GRADIENT(const Expression& f, const std::string& xvar, const std::string& yvar, const std::string& zvar) { return { DIFFERENTIATE(f, xvar), DIFFERENTIATE(f, yvar), DIFFERENTIATE(f, zvar) }; }

 vectors::SymbolicVector4 GRADIENT(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
);

 vectors::SymbolicVectorN GRADIENT(const Expression& f, const std::vector<std::string>& vars);

 matrices::SymbolicMatrix2x2 HESSIAN(const Expression& f, const std::string& xvar, const std::string& yvar);

 matrices::SymbolicMatrix3x3 HESSIAN(const Expression& f, const std::string& xvar, const std::string& yvar, const std::string& zvar);

 matrices::SymbolicMatrix4x4 HESSIAN(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
);

 matrices::SymbolicMatrixN HESSIAN(const Expression& f, const std::vector<std::string>& vars);

 Expression LAPLACIAN(const Expression& f, const std::string& xvar, const std::string& yvar);

 Expression LAPLACIAN(const Expression& f, const std::string& xvar, const std::string& yvar, const std::string& zvar);

 Expression LAPLACIAN(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
);

 Expression LAPLACIAN(const Expression& f, const std::vector<std::string>& vars);

 matrices::SymbolicMatrix2x2 JACOBIAN(const Expression& fx, const Expression& fy, const std::string& xvar, const std::string& yvar);

inline matrices::SymbolicMatrix2x2 JACOBIAN(const vectors::SymbolicVector2& F, const std::string& xvar, const std::string& yvar) { return JACOBIAN(F.x, F.y, xvar, yvar); }

inline vectors::SymbolicVector2 JACOBIAN(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar
) {
    vectors::SymbolicVector2 v;
    v.x = DIFFERENTIATE(f, xvar);
    v.y = DIFFERENTIATE(f, yvar);
    return v;
}

 matrices::SymbolicMatrix3x3 JACOBIAN(
    const Expression& fx,
    const Expression& fy,
    const Expression& fz,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
);

inline matrices::SymbolicMatrix3x3 JACOBIAN(
    const vectors::SymbolicVector3& F,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
) {
    return JACOBIAN(F.x, F.y, F.z, xvar, yvar, zvar);
}

 vectors::SymbolicVector3 JACOBIAN(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
);

 matrices::SymbolicMatrix4x4 JACOBIAN(
    const Expression& fx, const Expression& fy,
    const Expression& fz, const Expression& fw,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
);

inline matrices::SymbolicMatrix4x4 JACOBIAN(
    const vectors::SymbolicVector4& F,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    return JACOBIAN(F.x, F.y, F.z, F.w, xvar, yvar, zvar, wvar);
}

 vectors::SymbolicVector4 JACOBIAN(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
);

 matrices::SymbolicMatrixN JACOBIAN(const vectors::SymbolicVectorN& F, const std::vector<std::string>& vars);

 matrices::SymbolicMatrixNM JACOBIAN(const std::vector<Expression>& components, const std::vector<std::string>& vars);

 vectors::SymbolicVectorN JACOBIAN(
    const Expression& f,
    const std::vector<std::string>& vars
);

inline Expression DIRECTIONAL_DERIVATIVE(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const vectors::SymbolicVector2& direction
) {
    return direction.x * DIFFERENTIATE(f, xvar) + direction.y * DIFFERENTIATE(f, yvar);
}

 Expression DIRECTIONAL_DERIVATIVE(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar,
    const vectors::SymbolicVector3& direction
);

 Expression DIRECTIONAL_DERIVATIVE(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar,
    const vectors::SymbolicVector4& direction
);

 Expression DIRECTIONAL_DERIVATIVE(const Expression& f, const std::vector<std::string>& vars, const vectors::SymbolicVectorN& direction);

 Expression DIRECTIONAL_DERIVATIVE_UNIT(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const vectors::SymbolicVector2& direction
);

 Expression DIRECTIONAL_DERIVATIVE_UNIT(
    const Expression& f,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar,
    const vectors::SymbolicVector3& direction
);

 Expression DIRECTIONAL_DERIVATIVE_UNIT(
    const Expression& f,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar,
    const vectors::SymbolicVector4& direction
);

 Expression DIRECTIONAL_DERIVATIVE_UNIT(const Expression& f, const std::vector<std::string>& vars, const vectors::SymbolicVectorN& direction);

inline Expression DIVERGENCE(const Expression& fx, const Expression& fy, const std::string& xvar, const std::string& yvar) { return DIFFERENTIATE(fx, xvar) + DIFFERENTIATE(fy, yvar); }
inline Expression DIVERGENCE(const vectors::SymbolicVector2& F, const std::string& xvar, const std::string& yvar) { return DIVERGENCE(F.x, F.y, xvar, yvar); }

inline Expression DIVERGENCE(
    const Expression& fx,
    const Expression& fy,
    const Expression& fz,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
) {
    return DIFFERENTIATE(fx, xvar) + DIFFERENTIATE(fy, yvar) + DIFFERENTIATE(fz, zvar);
}

inline Expression DIVERGENCE(const vectors::SymbolicVector3& F, const std::string& xvar, const std::string& yvar, const std::string& zvar) { return DIVERGENCE(F.x, F.y, F.z, xvar, yvar, zvar); }

 Expression DIVERGENCE(
    const Expression& fx, const Expression& fy,
    const Expression& fz, const Expression& fw,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
);

inline Expression DIVERGENCE(
    const vectors::SymbolicVector4& F,
    const std::string& xvar, const std::string& yvar,
    const std::string& zvar, const std::string& wvar
) {
    return DIVERGENCE(F.x, F.y, F.z, F.w, xvar, yvar, zvar, wvar);
}

 Expression DIVERGENCE(const vectors::SymbolicVectorN& F, const std::vector<std::string>& vars);

inline Expression CURL(const Expression& fx, const Expression& fy, const std::string& xvar, const std::string& yvar) { return DIFFERENTIATE(fy, xvar) - DIFFERENTIATE(fx, yvar); }
inline Expression CURL(const vectors::SymbolicVector2& F, const std::string& xvar, const std::string& yvar) { return CURL(F.x, F.y, xvar, yvar); }

 vectors::SymbolicVector3 CURL(
    const Expression& fx,
    const Expression& fy,
    const Expression& fz,
    const std::string& xvar,
    const std::string& yvar,
    const std::string& zvar
);

inline vectors::SymbolicVector3 CURL(const vectors::SymbolicVector3& F, const std::string& xvar, const std::string& yvar, const std::string& zvar) { return CURL(F.x, F.y, F.z, xvar, yvar, zvar); }

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_STRUCTURE_CONVENIENCE_HPP