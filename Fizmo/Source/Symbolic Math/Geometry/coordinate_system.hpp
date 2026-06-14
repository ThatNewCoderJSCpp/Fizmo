#ifndef FIZMO_COORDINATE_SYSTEM_HPP
#define FIZMO_COORDINATE_SYSTEM_HPP

#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <sstream>
#include <unordered_map>
#include <initializer_list>
#include <algorithm>

#include "../Common/main_convenience.hpp"
#include "../Geometry/riemann_manifold.hpp"

namespace fizmo {
namespace math {
namespace geometry {

class CoordinateSystem2D {
public:
    CoordinateSystem2D(cas::Expression expr, std::string x_str = "x", std::string y_str = "y") : m_expr(std::move(expr)), m_x_str(std::move(x_str)), m_y_str(std::move(y_str)) {}

    const cas::Expression& expression() const noexcept { return m_expr; }
    const std::string& x_var() const noexcept { return m_x_str; }
    const std::string& y_var() const noexcept { return m_y_str; }
    const matrices::SymbolicMatrix2x2* jacobian() const { return m_jac.get(); }

    const matrices::SymbolicMatrix2x2* backward_jacobian() const {
        if (!m_jac_inv && m_jac) m_jac_inv = std::make_unique<matrices::SymbolicMatrix2x2>(m_jac->inverse());
        return m_jac_inv.get();
    }

    const cas::Expression* jacobian_determinant() const {
        if (!m_jac_det && m_jac) m_jac_det = std::make_unique<cas::Expression>(m_jac->determinant());
        return m_jac_det.get();
    }

    const matrices::SymbolicMatrix2x2* metric() const {
        if (!m_metric && m_jac) m_metric = std::make_unique<matrices::SymbolicMatrix2x2>(m_jac->transpose() * (*m_jac));
        return m_metric.get();
    }

public:
    bool has_jacobian() const noexcept { return m_jac != nullptr; }

    CoordinateSystem2D transform_no_jacobian(cas::Expression new_x_expr, cas::Expression new_y_expr, std::string u_var = "u", std::string v_var = "v") const {
        cas::Expression new_expr = m_expr;
        
        new_expr.substitute({
            { m_x_str, new_x_expr },
            { m_y_str, new_y_expr }
        });

        return CoordinateSystem2D(new_expr, u_var, v_var);
    }

    CoordinateSystem2D transform(cas::Expression new_x_expr, cas::Expression new_y_expr, std::string u_var = "u", std::string v_var = "v") const {
        CoordinateSystem2D result = std::move(transform_no_jacobian(new_x_expr, new_y_expr, u_var, v_var));

        result.m_jac = std::make_unique<matrices::SymbolicMatrix2x2>(
            new_x_expr.differentiate(u_var), new_x_expr.differentiate(v_var),
            new_y_expr.differentiate(u_var), new_y_expr.differentiate(v_var)
        );

        return result;
    }

    RiemannianManifold make_manifold() {
        if (!metric()) throw std::runtime_error("Coordinate system has no Jacobian");
        tensors::SymbolicSquareTensor g = tensors::SymbolicSquareTensor::from(*metric());
        return RiemannianManifold(g, {x_var(), y_var()});
    }

private:
    std::string m_x_str;
    std::string m_y_str;
    cas::Expression  m_expr;
    std::unique_ptr<matrices::SymbolicMatrix2x2>         m_jac = nullptr;
    mutable std::unique_ptr<cas::Expression>        m_jac_det = nullptr;
    mutable std::unique_ptr<matrices::SymbolicMatrix2x2> m_jac_inv = nullptr;
    mutable std::unique_ptr<matrices::SymbolicMatrix2x2> m_metric = nullptr;
};

class CoordinateSystem3D {
public:
    CoordinateSystem3D(
        cas::Expression expr, 
        std::string x_str = "x", std::string y_str = "y", std::string z_str = "z"
    ) : m_expr(std::move(expr)), m_x_str(std::move(x_str)), m_y_str(std::move(y_str)), m_z_str(std::move(z_str)) {}

    const cas::Expression& expression() const noexcept { return m_expr; }
    const std::string& x_var() const noexcept { return m_x_str; }
    const std::string& y_var() const noexcept { return m_y_str; }
    const std::string& z_var() const noexcept { return m_z_str; }
    const matrices::SymbolicMatrix3x3* jacobian() const { return m_jac.get(); }
    
    const matrices::SymbolicMatrix3x3* backward_jacobian() const {
        if (!m_jac_inv && m_jac) m_jac_inv = std::make_unique<matrices::SymbolicMatrix3x3>(m_jac->inverse());
        return m_jac_inv.get();
    }

    const cas::Expression* jacobian_determinant() const {
        if (!m_jac_det && m_jac) m_jac_det = std::make_unique<cas::Expression>(m_jac->determinant());
        return m_jac_det.get();
    }

    const matrices::SymbolicMatrix3x3* metric() const {
        if (!m_metric && m_jac) m_metric = std::make_unique<matrices::SymbolicMatrix3x3>(m_jac->transpose() * (*m_jac));
        return m_metric.get();
    }

public:
    bool has_jacobian() const noexcept { return m_jac != nullptr; }

    CoordinateSystem3D transform_no_jacobian(
        cas::Expression new_x_expr, cas::Expression new_y_expr, cas::Expression new_z_expr,
        std::string u_var = "u", std::string v_var = "v", std::string w_var = "w"
    ) const {
        cas::Expression new_expr = m_expr;
        
        new_expr.substitute({
            { m_x_str, new_x_expr },
            { m_y_str, new_y_expr },
            { m_z_str, new_z_expr }
        });

        return CoordinateSystem3D(new_expr, u_var, v_var, w_var);
    }

    CoordinateSystem3D transform(
        cas::Expression new_x_expr, cas::Expression new_y_expr, cas::Expression new_z_expr,
        std::string u_var = "u", std::string v_var = "v", std::string w_var = "w"
    ) const {
        CoordinateSystem3D result = std::move(transform_no_jacobian(new_x_expr, new_y_expr, new_z_expr, u_var, v_var, w_var));

        result.m_jac = std::make_unique<matrices::SymbolicMatrix3x3>(
            new_x_expr.differentiate(u_var), new_x_expr.differentiate(v_var), new_x_expr.differentiate(w_var),
            new_y_expr.differentiate(u_var), new_y_expr.differentiate(v_var), new_y_expr.differentiate(w_var),
            new_z_expr.differentiate(u_var), new_z_expr.differentiate(v_var), new_z_expr.differentiate(w_var)
        );
        
        return result;
    }

    RiemannianManifold make_manifold() {
        if (!metric()) throw std::runtime_error("Coordinate system has no Jacobian");
        tensors::SymbolicSquareTensor g = tensors::SymbolicSquareTensor::from(*metric());
        return RiemannianManifold(g, {x_var(), y_var(), z_var()});
    }

private:
    std::string m_x_str;
    std::string m_y_str;
    std::string m_z_str;
    cas::Expression  m_expr;
    std::unique_ptr<matrices::SymbolicMatrix3x3>         m_jac     = nullptr;
    mutable std::unique_ptr<cas::Expression>        m_jac_det = nullptr;
    mutable std::unique_ptr<matrices::SymbolicMatrix3x3> m_jac_inv = nullptr;
    mutable std::unique_ptr<matrices::SymbolicMatrix3x3> m_metric  = nullptr;
};

class CoordinateSystem4D {
public:
    CoordinateSystem4D(
        cas::Expression expr,
        std::string x_str = "x", std::string y_str = "y",
        std::string z_str = "z", std::string w_str = "w"
    ) : m_expr(std::move(expr)),
        m_x_str(std::move(x_str)), m_y_str(std::move(y_str)),
        m_z_str(std::move(z_str)), m_w_str(std::move(w_str)) {}

    const cas::Expression&  expression() const noexcept { return m_expr;  }
    const std::string& x_var()      const noexcept { return m_x_str; }
    const std::string& y_var()      const noexcept { return m_y_str; }
    const std::string& z_var()      const noexcept { return m_z_str; }
    const std::string& w_var()      const noexcept { return m_w_str; }
    const matrices::SymbolicMatrix4x4* jacobian() const { return m_jac.get(); }

    const matrices::SymbolicMatrix4x4* backward_jacobian() const {
        if (!m_jac_inv && m_jac) m_jac_inv = std::make_unique<matrices::SymbolicMatrix4x4>(m_jac->inverse());
        return m_jac_inv.get();
    }

    const cas::Expression* jacobian_determinant() const {
        if (!m_jac_det && m_jac) m_jac_det = std::make_unique<cas::Expression>(m_jac->determinant());
        return m_jac_det.get();
    }

    const matrices::SymbolicMatrix4x4* metric() const {
        if (!m_metric && m_jac) m_metric = std::make_unique<matrices::SymbolicMatrix4x4>(m_jac->transpose() * (*m_jac));
        return m_metric.get();
    }

    bool has_jacobian() const noexcept { return m_jac != nullptr; }

    CoordinateSystem4D transform_no_jacobian(
        cas::Expression new_x, cas::Expression new_y, cas::Expression new_z, cas::Expression new_w,
        std::string u0 = "u", std::string u1 = "v", std::string u2 = "s", std::string u3 = "t"
    ) const {
        cas::Expression new_expr = m_expr;
        
        new_expr.substitute({
            { m_x_str, new_x },
            { m_y_str, new_y },
            { m_z_str, new_z },
            { m_w_str, new_w }
        });

        return CoordinateSystem4D(new_expr, u0, u1, u2, u3);
    }

    CoordinateSystem4D transform(
        cas::Expression new_x, cas::Expression new_y, cas::Expression new_z, cas::Expression new_w,
        std::string u0 = "u", std::string u1 = "v", std::string u2 = "s", std::string u3 = "t"
    ) const {
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

    RiemannianManifold make_manifold() {
        if (!metric()) throw std::runtime_error("Coordinate system has no Jacobian");
        tensors::SymbolicSquareTensor g = tensors::SymbolicSquareTensor::from(*metric());
        return RiemannianManifold(g, {x_var(), y_var(), z_var(), w_var()});
    }

private:
    std::string m_x_str;
    std::string m_y_str;
    std::string m_z_str;
    std::string m_w_str;
    cas::Expression  m_expr;
    std::unique_ptr<matrices::SymbolicMatrix4x4>         m_jac     = nullptr;
    mutable std::unique_ptr<cas::Expression>        m_jac_det = nullptr;
    mutable std::unique_ptr<matrices::SymbolicMatrix4x4> m_jac_inv = nullptr;
    mutable std::unique_ptr<matrices::SymbolicMatrix4x4> m_metric  = nullptr;
};

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_COORDINATE_SYSTEM_HPP