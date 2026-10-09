#ifndef FIZMO_RIEMANNIAN_MANIFOLD_HPP
#define FIZMO_RIEMANNIAN_MANIFOLD_HPP

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../Common/main_convenience.hpp"
#include "../Linalg/tensors.hpp"
#include "../Linalg/vectors.hpp"
#include "../Linalg/matrices.hpp"
#include "indexed_tensor.hpp"

namespace fizmo {
namespace math {

namespace tensors {

 IndexedTensor make_metric(const SymbolicSquareTensor& g, const std::string& idx_a = "mu", const std::string& idx_b = "nu");

 IndexedTensor make_metric_inverse(const SymbolicSquareTensor& g_inv, const std::string& idx_a = "mu", const std::string& idx_b = "nu");

inline SymbolicSquareTensor invert_metric(const SymbolicSquareTensor& g) {
    auto mat = g.to_matrix();
    auto inv = mat.inverse();
    return SymbolicSquareTensor::from(inv);
}

} // namespace tensors

namespace geometry {

class RiemannianManifold {
public:
    RiemannianManifold(const tensors::SymbolicSquareTensor& g_lower, std::vector<std::string> coord_names);

    std::size_t dim() const noexcept { return dim_; }
    const std::vector<std::string>& coords() const noexcept { return coords_; }
    const tensors::SymbolicSquareTensor& metric() const noexcept { return g_; }
    const tensors::SymbolicSquareTensor& metric_inverse() const noexcept { return g_inv_; }

public:
    tensors::IndexedTensor metric_tensor(const std::string& a="mu", const std::string& b="nu") const {
        return make_metric(g_, a, b);
    }

    tensors::IndexedTensor metric_inverse_tensor(const std::string& a="mu", const std::string& b="nu") const {
        return make_metric_inverse(g_inv_, a, b);
    }

    cas::Expression determinant() const {
        if (!det_) compute_determinant();
        return *det_;
    }

    tensors::IndexedTensor christoffel_first(
        const std::string& a="alpha",
        const std::string& b="beta",
        const std::string& c="gamma"
    ) const {
        if (!Gamma1_) compute_christoffel1();
        return Gamma1_->as_indexed(a, b, c);
    }

    tensors::IndexedTensor christoffel_second(
        const std::string& a="alpha",
        const std::string& b="beta",
        const std::string& c="gamma"
    ) const {
        if (!Gamma2_) compute_christoffel2();
        return Gamma2_->as_indexed(a, b, c);
    }

    tensors::IndexedTensor riemann(
        const std::string& a="rho",
        const std::string& b="sigma",
        const std::string& c="mu",
        const std::string& d="nu"
    ) const {
        if (!Riem_) compute_riemann();
        return Riem_->as_indexed_up_down_down_down(a, b, c, d);
    }

    tensors::IndexedTensor riemann_lower(
        const std::string& a="rho",
        const std::string& b="sigma",
        const std::string& c="mu",
        const std::string& d="nu"
    ) const;

    tensors::IndexedTensor ricci(const std::string& a="mu", const std::string& b="nu") const {
        if (!Ric_) compute_ricci();
        return Ric_->as_indexed_down_down(a, b);
    }

    cas::Expression ricci_scalar() const {
        if (!RicciScalar_) compute_ricci_scalar();
        return *RicciScalar_;
    }

    tensors::IndexedTensor einstein(
        const std::string& a="mu",
        const std::string& b="nu"
    ) const;

    tensors::IndexedTensor schouten(
        const std::string& a="mu",
        const std::string& b="nu"
    ) const;

    tensors::IndexedTensor weyl(
        const std::string& a="rho",
        const std::string& b="sigma",
        const std::string& c="mu",
        const std::string& d="nu"
    ) const;

    tensors::IndexedTensor cotton(
        const std::string& a="alpha",
        const std::string& b="beta",
        const std::string& c="gamma"
    ) const;

    cas::Expression kretschmann_scalar() const {
        if (!Kretsch_) compute_kretschmann();
        return *Kretsch_;
    }

    cas::Expression volume_element() const;

public:
    tensors::IndexedTensor coordinate_basis_vector(const std::string& a="mu") const { return tensors::IndexedTensor::kronecker_delta(dim_, a, "mu"); }
    tensors::IndexedTensor coordinate_basis_covector(const std::string& a="mu") const { return tensors::IndexedTensor::kronecker_delta(dim_, "mu", a); }
    tensors::IndexedTensor dual_basis_covector(const std::string& a="mu") const { return coordinate_basis_covector(a); }
    tensors::IndexedTensor dual_basis_vector(const std::string& a="mu") const { return coordinate_basis_vector(a); }

    tensors::IndexedTensor vielbein(const std::string& a, const std::string& mu) const;

    tensors::IndexedTensor inverse_vielbein(const std::string& a, const std::string& mu) const;

    tensors::IndexedTensor connection_1form(
        const std::string& a,
        const std::string& b,
        const std::string& mu
    ) const;

    tensors::IndexedTensor curvature_2form(
        const std::string& a,
        const std::string& b,
        const std::string& mu,
        const std::string& nu
    ) const;

    tensors::IndexedTensor torsion_2form(
        const std::string& a = "a",
        const std::string& mu = "mu",
        const std::string& nu = "nu"
    ) const;

    tensors::IndexedTensor ricci_rotation_coeffs(
        const std::string& a = "a",
        const std::string& b = "b",
        const std::string& c = "c"
    ) const;

private:
    void compute_determinant() const;

    void compute_christoffel1() const;

    void compute_christoffel2() const;

    void compute_riemann() const;

    void compute_riemann_lower() const;

    void compute_ricci() const;

    void compute_ricci_scalar() const;

    void compute_einstein() const;

    void compute_schouten() const;

    void compute_weyl() const;

    void compute_cotton() const;

    void compute_kretschmann() const;

    void compute_vielbein() const;

private:
    std::size_t dim_;
    std::vector<std::string> coords_;
    tensors::SymbolicSquareTensor g_;
    tensors::SymbolicSquareTensor g_inv_;

    mutable std::unique_ptr<tensors::ChristoffelSymbolsFirst>  Gamma1_;
    mutable std::unique_ptr<tensors::ChristoffelSymbolsSecond> Gamma2_;
    mutable std::unique_ptr<tensors::RiemannTensor>            Riem_;
    mutable std::unique_ptr<tensors::RiemannTensor>            RiemLower_;
    mutable std::unique_ptr<tensors::RicciTensor>              Ric_;

    mutable std::unique_ptr<cas::Expression> det_;
    mutable std::unique_ptr<cas::Expression> RicciScalar_;
    mutable std::unique_ptr<cas::Expression> Kretsch_;
    mutable std::unique_ptr<cas::Expression> volume_;

    mutable std::unique_ptr<tensors::IndexedTensor> Einstein_;
    mutable std::unique_ptr<tensors::IndexedTensor> Schouten_;
    mutable std::unique_ptr<tensors::IndexedTensor> Weyl_;
    mutable std::unique_ptr<tensors::IndexedTensor> Cotton_;
    mutable std::unique_ptr<tensors::IndexedTensor> vielbein_;
    mutable std::unique_ptr<tensors::IndexedTensor> inv_vielbein_;
    mutable std::unique_ptr<tensors::IndexedTensor> Conn1_;      
    mutable std::unique_ptr<tensors::IndexedTensor> Curv2_;      
    mutable std::unique_ptr<tensors::IndexedTensor> Torsion2_;  
    mutable std::unique_ptr<tensors::IndexedTensor> RicciRot_;   
};

inline tensors::IndexedTensor CHRISTOFFEL_FIRST(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "alpha",
    const std::string& b = "beta",
    const std::string& c = "gamma"
) {
    RiemannianManifold M(g, coords);
    return M.christoffel_first(a, b, c);
}

inline tensors::IndexedTensor CHRISTOFFEL_SECOND(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "alpha",
    const std::string& b = "beta",
    const std::string& c = "gamma"
) {
    RiemannianManifold M(g, coords);
    return M.christoffel_second(a, b, c);
}

inline tensors::IndexedTensor RIEMANN_TENSOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "rho",
    const std::string& b = "sigma",
    const std::string& c = "mu",
    const std::string& d = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.riemann(a, b, c, d);
}

inline tensors::IndexedTensor RIEMANN_TENSOR_LOWER(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "rho",
    const std::string& b = "sigma",
    const std::string& c = "mu",
    const std::string& d = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.riemann_lower(a, b, c, d);
}

inline tensors::IndexedTensor RICCI_TENSOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu",
    const std::string& b = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.ricci(a, b);
}

inline cas::Expression RICCI_SCALAR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords
) {
    RiemannianManifold M(g, coords);
    return M.ricci_scalar();
}

inline tensors::IndexedTensor EINSTEIN_TENSOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu",
    const std::string& b = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.einstein(a, b);
}

inline tensors::IndexedTensor SCHOUTEN_TENSOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu",
    const std::string& b = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.schouten(a, b);
}

inline tensors::IndexedTensor WEYL_TENSOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "rho",
    const std::string& b = "sigma",
    const std::string& c = "mu",
    const std::string& d = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.weyl(a, b, c, d);
}

inline tensors::IndexedTensor COTTON_TENSOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "lambda",
    const std::string& b = "mu",
    const std::string& c = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.cotton(a, b, c);
}

inline cas::Expression KRETSCHMANN_SCALAR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords
) {
    RiemannianManifold M(g, coords);
    return M.kretschmann_scalar();
}

inline cas::Expression VOLUME_ELEMENT(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords
) {
    RiemannianManifold M(g, coords);
    return M.volume_element();
}

inline tensors::IndexedTensor COORDINATE_BASIS_VECTOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.coordinate_basis_vector(a);
}

inline tensors::IndexedTensor COORDINATE_BASIS_COVECTOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.coordinate_basis_covector(a);
}

inline tensors::IndexedTensor DUAL_BASIS_VECTOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.dual_basis_vector(a);
}

inline tensors::IndexedTensor DUAL_BASIS_COVECTOR(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.dual_basis_covector(a);
}

inline tensors::IndexedTensor VIELBEIN(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "a",
    const std::string& mu = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.vielbein(a, mu);
}

inline tensors::IndexedTensor INVERSE_VIELBEIN(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "a",
    const std::string& mu = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.inverse_vielbein(a, mu);
}

inline tensors::IndexedTensor CONNECTION_1FORM(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "a",
    const std::string& b = "b",
    const std::string& mu = "mu"
) {
    RiemannianManifold M(g, coords);
    return M.connection_1form(a, b, mu);
}

inline tensors::IndexedTensor CURVATURE_2FORM(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "a",
    const std::string& b = "b",
    const std::string& mu = "mu",
    const std::string& nu = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.curvature_2form(a, b, mu, nu);
}

inline tensors::IndexedTensor TORSION_2FORM(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "a",
    const std::string& mu = "mu",
    const std::string& nu = "nu"
) {
    RiemannianManifold M(g, coords);
    return M.torsion_2form(a, mu, nu);
}

inline tensors::IndexedTensor RICCI_ROTATION_COEFFS(
    const tensors::SymbolicSquareTensor& g,
    const std::vector<std::string>& coords,
    const std::string& a = "a",
    const std::string& b = "b",
    const std::string& c = "c"
) {
    RiemannianManifold M(g, coords);
    return M.ricci_rotation_coeffs(a, b, c);
}

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_RIEMANNIAN_MANIFOLD_HPP