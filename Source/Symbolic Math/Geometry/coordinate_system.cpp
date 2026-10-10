#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace geometry {

auto CoordinateSystem2D::backward_jacobian() const -> const matrices::SymbolicMatrix2x2* {
    if (!m_jac_inv && m_jac) m_jac_inv = std::make_unique<matrices::SymbolicMatrix2x2>(m_jac->inverse());
    return m_jac_inv.get();
}

auto CoordinateSystem2D::jacobian_determinant() const -> const cas::Expression* {
    if (!m_jac_det && m_jac) m_jac_det = std::make_unique<cas::Expression>(m_jac->determinant());
    return m_jac_det.get();
}

auto CoordinateSystem2D::metric() const -> const matrices::SymbolicMatrix2x2* {
    if (!m_metric && m_jac) m_metric = std::make_unique<matrices::SymbolicMatrix2x2>(m_jac->transpose() * (*m_jac));
    return m_metric.get();
}

auto CoordinateSystem2D::transform_no_jacobian(cas::Expression new_x_expr, cas::Expression new_y_expr, std::string u_var, std::string v_var) const -> CoordinateSystem2D {
    cas::Expression new_expr = m_expr;
        
    new_expr.substitute({
        { m_x_str, new_x_expr },
        { m_y_str, new_y_expr }
    });

    return CoordinateSystem2D(new_expr, u_var, v_var);
}

auto CoordinateSystem2D::transform(cas::Expression new_x_expr, cas::Expression new_y_expr, std::string u_var, std::string v_var) const -> CoordinateSystem2D {
    CoordinateSystem2D result = std::move(transform_no_jacobian(new_x_expr, new_y_expr, u_var, v_var));

    result.m_jac = std::make_unique<matrices::SymbolicMatrix2x2>(
        new_x_expr.differentiate(u_var), new_x_expr.differentiate(v_var),
        new_y_expr.differentiate(u_var), new_y_expr.differentiate(v_var)
    );

    return result;
}

auto CoordinateSystem2D::make_manifold() -> RiemannianManifold {
    if (!metric()) throw std::runtime_error("Coordinate system has no Jacobian");
    tensors::SymbolicSquareTensor g = tensors::SymbolicSquareTensor::from(*metric());
    return RiemannianManifold(g, {x_var(), y_var()});
}

CoordinateSystem3D::CoordinateSystem3D(
        cas::Expression expr, 
        std::string x_str, std::string y_str, std::string z_str 
) : m_expr(std::move(expr)), m_x_str(std::move(x_str)), m_y_str(std::move(y_str)), m_z_str(std::move(z_str)) {}

auto CoordinateSystem3D::backward_jacobian() const -> const matrices::SymbolicMatrix3x3* {
    if (!m_jac_inv && m_jac) m_jac_inv = std::make_unique<matrices::SymbolicMatrix3x3>(m_jac->inverse());
    return m_jac_inv.get();
}

auto CoordinateSystem3D::jacobian_determinant() const -> const cas::Expression* {
    if (!m_jac_det && m_jac) m_jac_det = std::make_unique<cas::Expression>(m_jac->determinant());
    return m_jac_det.get();
}

auto CoordinateSystem3D::metric() const -> const matrices::SymbolicMatrix3x3* {
    if (!m_metric && m_jac) m_metric = std::make_unique<matrices::SymbolicMatrix3x3>(m_jac->transpose() * (*m_jac));
    return m_metric.get();
}

auto CoordinateSystem3D::transform_no_jacobian(
    cas::Expression new_x_expr, cas::Expression new_y_expr, cas::Expression new_z_expr,
    std::string u_var, std::string v_var, std::string w_var 
) const -> CoordinateSystem3D {
    cas::Expression new_expr = m_expr;
        
    new_expr.substitute({
        { m_x_str, new_x_expr },
        { m_y_str, new_y_expr },
        { m_z_str, new_z_expr }
    });

    return CoordinateSystem3D(new_expr, u_var, v_var, w_var);
}

auto CoordinateSystem3D::transform(
    cas::Expression new_x_expr, cas::Expression new_y_expr, cas::Expression new_z_expr,
    std::string u_var, std::string v_var, std::string w_var 
) const -> CoordinateSystem3D {
    CoordinateSystem3D result = std::move(transform_no_jacobian(new_x_expr, new_y_expr, new_z_expr, u_var, v_var, w_var));

    result.m_jac = std::make_unique<matrices::SymbolicMatrix3x3>(
        new_x_expr.differentiate(u_var), new_x_expr.differentiate(v_var), new_x_expr.differentiate(w_var),
        new_y_expr.differentiate(u_var), new_y_expr.differentiate(v_var), new_y_expr.differentiate(w_var),
        new_z_expr.differentiate(u_var), new_z_expr.differentiate(v_var), new_z_expr.differentiate(w_var)
    );
        
    return result;
}

auto CoordinateSystem3D::make_manifold() -> RiemannianManifold {
    if (!metric()) throw std::runtime_error("Coordinate system has no Jacobian");
    tensors::SymbolicSquareTensor g = tensors::SymbolicSquareTensor::from(*metric());
    return RiemannianManifold(g, {x_var(), y_var(), z_var()});
}

CoordinateSystem4D::CoordinateSystem4D(
        cas::Expression expr,
        std::string x_str, std::string y_str,
        std::string z_str, std::string w_str 
) : m_expr(std::move(expr)),
        m_x_str(std::move(x_str)), m_y_str(std::move(y_str)),
        m_z_str(std::move(z_str)), m_w_str(std::move(w_str)) {}

auto CoordinateSystem4D::backward_jacobian() const -> const matrices::SymbolicMatrix4x4* {
    if (!m_jac_inv && m_jac) m_jac_inv = std::make_unique<matrices::SymbolicMatrix4x4>(m_jac->inverse());
    return m_jac_inv.get();
}

auto CoordinateSystem4D::jacobian_determinant() const -> const cas::Expression* {
    if (!m_jac_det && m_jac) m_jac_det = std::make_unique<cas::Expression>(m_jac->determinant());
    return m_jac_det.get();
}

auto CoordinateSystem4D::metric() const -> const matrices::SymbolicMatrix4x4* {
    if (!m_metric && m_jac) m_metric = std::make_unique<matrices::SymbolicMatrix4x4>(m_jac->transpose() * (*m_jac));
    return m_metric.get();
}

auto CoordinateSystem4D::transform_no_jacobian(
    cas::Expression new_x, cas::Expression new_y, cas::Expression new_z, cas::Expression new_w,
    std::string u0, std::string u1, std::string u2, std::string u3 
) const -> CoordinateSystem4D {
    cas::Expression new_expr = m_expr;
        
    new_expr.substitute({
        { m_x_str, new_x },
        { m_y_str, new_y },
        { m_z_str, new_z },
        { m_w_str, new_w }
    });

    return CoordinateSystem4D(new_expr, u0, u1, u2, u3);
}

auto CoordinateSystem4D::transform(
    cas::Expression new_x, cas::Expression new_y, cas::Expression new_z, cas::Expression new_w,
    std::string u0, std::string u1, std::string u2, std::string u3 
) const -> CoordinateSystem4D {
    CoordinateSystem4D result = std::move(
        transform_no_jacobian(new_x, new_y, new_z, new_w, u0, u1, u2, u3)
    );

    matrices::SymbolicMatrix4x4 J;
    auto& d = J.get_data();
    cas::Expression exprs[4] = { new_x, new_y, new_z, new_w };
    std::string vars[4] = { u0, u1, u2, u3 };

    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            d[r][c] = exprs[r].differentiate(vars[c]);

    result.m_jac = std::make_unique<matrices::SymbolicMatrix4x4>(std::move(J));
    return result;
}

auto CoordinateSystem4D::make_manifold() -> RiemannianManifold {
    if (!metric()) throw std::runtime_error("Coordinate system has no Jacobian");
    tensors::SymbolicSquareTensor g = tensors::SymbolicSquareTensor::from(*metric());
    return RiemannianManifold(g, {x_var(), y_var(), z_var(), w_var()});
}

} // namespace geometry
} // namespace math
} // namespace fizmo
