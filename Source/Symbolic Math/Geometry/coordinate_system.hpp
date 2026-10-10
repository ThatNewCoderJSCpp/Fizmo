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

    const matrices::SymbolicMatrix2x2* backward_jacobian() const;

    const cas::Expression* jacobian_determinant() const;

    const matrices::SymbolicMatrix2x2* metric() const;

public:
    bool has_jacobian() const noexcept { return m_jac != nullptr; }

    CoordinateSystem2D transform_no_jacobian(cas::Expression new_x_expr, cas::Expression new_y_expr, std::string u_var = "u", std::string v_var = "v") const;

    CoordinateSystem2D transform(cas::Expression new_x_expr, cas::Expression new_y_expr, std::string u_var = "u", std::string v_var = "v") const;

    RiemannianManifold make_manifold();

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
    );

    const cas::Expression& expression() const noexcept { return m_expr; }
    const std::string& x_var() const noexcept { return m_x_str; }
    const std::string& y_var() const noexcept { return m_y_str; }
    const std::string& z_var() const noexcept { return m_z_str; }
    const matrices::SymbolicMatrix3x3* jacobian() const { return m_jac.get(); }
    
    const matrices::SymbolicMatrix3x3* backward_jacobian() const;

    const cas::Expression* jacobian_determinant() const;

    const matrices::SymbolicMatrix3x3* metric() const;

public:
    bool has_jacobian() const noexcept { return m_jac != nullptr; }

    CoordinateSystem3D transform_no_jacobian(
        cas::Expression new_x_expr, cas::Expression new_y_expr, cas::Expression new_z_expr,
        std::string u_var = "u", std::string v_var = "v", std::string w_var = "w"
    ) const;

    CoordinateSystem3D transform(
        cas::Expression new_x_expr, cas::Expression new_y_expr, cas::Expression new_z_expr,
        std::string u_var = "u", std::string v_var = "v", std::string w_var = "w"
    ) const;

    RiemannianManifold make_manifold();

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
    );

    const cas::Expression&  expression() const noexcept { return m_expr;  }
    const std::string& x_var()      const noexcept { return m_x_str; }
    const std::string& y_var()      const noexcept { return m_y_str; }
    const std::string& z_var()      const noexcept { return m_z_str; }
    const std::string& w_var()      const noexcept { return m_w_str; }
    const matrices::SymbolicMatrix4x4* jacobian() const { return m_jac.get(); }

    const matrices::SymbolicMatrix4x4* backward_jacobian() const;

    const cas::Expression* jacobian_determinant() const;

    const matrices::SymbolicMatrix4x4* metric() const;

    bool has_jacobian() const noexcept { return m_jac != nullptr; }

    CoordinateSystem4D transform_no_jacobian(
        cas::Expression new_x, cas::Expression new_y, cas::Expression new_z, cas::Expression new_w,
        std::string u0 = "u", std::string u1 = "v", std::string u2 = "s", std::string u3 = "t"
    ) const;

    CoordinateSystem4D transform(
        cas::Expression new_x, cas::Expression new_y, cas::Expression new_z, cas::Expression new_w,
        std::string u0 = "u", std::string u1 = "v", std::string u2 = "s", std::string u3 = "t"
    ) const;

    RiemannianManifold make_manifold();

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