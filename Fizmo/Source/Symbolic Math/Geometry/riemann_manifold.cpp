#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace tensors {

IndexedTensor make_metric(const SymbolicSquareTensor& g, const std::string& idx_a, const std::string& idx_b) {
    if (g.rank() != 2) throw std::invalid_argument("make_metric: need rank-2 tensor");
    return IndexedTensor(g, { IndexSlot::down(idx_a), IndexSlot::down(idx_b) });
}

IndexedTensor make_metric_inverse(const SymbolicSquareTensor& g_inv, const std::string& idx_a, const std::string& idx_b) {
    if (g_inv.rank() != 2) throw std::invalid_argument("make_metric_inverse: need rank-2 tensor");
    return IndexedTensor(g_inv, { IndexSlot::up(idx_a), IndexSlot::up(idx_b) });
}

} // namespace tensors
} // namespace math
} // namespace fizmo

namespace fizmo {
namespace math {
namespace geometry {

RiemannianManifold::RiemannianManifold(const tensors::SymbolicSquareTensor& g_lower, std::vector<std::string> coord_names) : dim_(g_lower.dim()), coords_(std::move(coord_names)), g_(g_lower) {
        if (g_.rank() != 2) throw std::invalid_argument("RiemannianManifold: metric must be rank 2");
        if (coords_.size() != dim_) throw std::invalid_argument("RiemannianManifold: need " + std::to_string(dim_) + " coordinate names");
        g_inv_ = tensors::invert_metric(g_);
    }

auto RiemannianManifold::riemann_lower(
        const std::string& a,
        const std::string& b,
        const std::string& c,
        const std::string& d
) const -> tensors::IndexedTensor {
        if (!RiemLower_) compute_riemann_lower();
        return RiemLower_->as_indexed_down_down_down_down(a, b, c, d);
    }

auto RiemannianManifold::einstein(
        const std::string& a,
        const std::string& b
) const -> tensors::IndexedTensor {
        if (!Einstein_) compute_einstein();
        return Einstein_->relabel({ tensors::IndexSlot::down(a), tensors::IndexSlot::down(b) });
    }

auto RiemannianManifold::schouten(
        const std::string& a,
        const std::string& b
) const -> tensors::IndexedTensor {
        if (!Schouten_) compute_schouten();
        return Schouten_->relabel({ tensors::IndexSlot::down(a), tensors::IndexSlot::down(b) });
    }

auto RiemannianManifold::weyl(
        const std::string& a,
        const std::string& b,
        const std::string& c,
        const std::string& d
) const -> tensors::IndexedTensor {
        if (!Weyl_) compute_weyl();
        return Weyl_->relabel({
            tensors::IndexSlot::down(a),
            tensors::IndexSlot::down(b),
            tensors::IndexSlot::down(c),
            tensors::IndexSlot::down(d)
        });
    }

auto RiemannianManifold::cotton(
        const std::string& a,
        const std::string& b,
        const std::string& c
) const -> tensors::IndexedTensor {
        if (!Cotton_) compute_cotton();
        return Cotton_->relabel({
            tensors::IndexSlot::down(a),
            tensors::IndexSlot::down(b),
            tensors::IndexSlot::down(c)
        });
    }

auto RiemannianManifold::volume_element() const -> cas::Expression {
        if (!volume_) {
            cas::Expression d = determinant();
            volume_ = std::make_unique<cas::Expression>(FULL_SIMPLIFY(SQRT(ABS(d))));
        }
        return *volume_;
    }

auto RiemannianManifold::vielbein(const std::string& a, const std::string& mu) const -> tensors::IndexedTensor {
        if (!vielbein_) compute_vielbein();
        return vielbein_->relabel({ tensors::IndexSlot::up(a), tensors::IndexSlot::down(mu) });
    }

auto RiemannianManifold::inverse_vielbein(const std::string& a, const std::string& mu) const -> tensors::IndexedTensor {
        if (!inv_vielbein_) compute_vielbein();
        return inv_vielbein_->relabel({ tensors::IndexSlot::down(mu), tensors::IndexSlot::up(a) });
    }

auto RiemannianManifold::connection_1form(
        const std::string& a,
        const std::string& b,
        const std::string& mu
) const -> tensors::IndexedTensor {
        if (!Conn1_) {
            if (!Gamma2_) compute_christoffel2();
            if (!vielbein_) compute_vielbein();
            auto e = vielbein("a", "nu");                
            auto G = christoffel_second("nu", "b", "mu");
            auto w = CONTRACT(e, G);                    

            Conn1_ = std::make_unique<tensors::IndexedTensor>(
                w.relabel({
                    tensors::IndexSlot::up("a"),
                    tensors::IndexSlot::down("b"),
                    tensors::IndexSlot::down("mu")
                })
            );
        }

        return Conn1_->relabel({
            tensors::IndexSlot::up(a),
            tensors::IndexSlot::down(b),
            tensors::IndexSlot::down(mu)
        });
    }

auto RiemannianManifold::curvature_2form(
        const std::string& a,
        const std::string& b,
        const std::string& mu,
        const std::string& nu
) const -> tensors::IndexedTensor {
        if (!Curv2_) {
            auto w = connection_1form("a", "b", "rho");
            auto raw = tensors::SymbolicSquareTensor::zero(dim_, 4);

            for (std::size_t A=0; A<dim_; ++A)
                for (std::size_t B=0; B<dim_; ++B)
                    for (std::size_t M=0; M<dim_; ++M)
                        for (std::size_t N=0; N<dim_; ++N) {
                            cas::Expression dM = DIFFERENTIATE(w.at({A,B,M}), coords_[N]);
                            cas::Expression dN = DIFFERENTIATE(w.at({A,B,N}), coords_[M]);
                            raw.at({A,B,M,N}) = dM - dN;
                        }

            tensors::IndexedTensor dW(raw, {
                tensors::IndexSlot::up("a"),
                tensors::IndexSlot::down("b"),
                tensors::IndexSlot::down("mu"),
                tensors::IndexSlot::down("nu")
            });

            auto w1 = connection_1form("a", "c", "mu");
            auto w2 = connection_1form("c", "b", "nu");
            auto term1 = CONTRACT(w1, w2);                      
            auto w1_swapped = w1.swap_indices("mu", "nu");
            auto term2 = CONTRACT(w1_swapped, w2);
            auto total = dW + term1 - term2;

            Curv2_ = std::make_unique<tensors::IndexedTensor>(
                total.relabel({
                    tensors::IndexSlot::up("a"),
                    tensors::IndexSlot::down("b"),
                    tensors::IndexSlot::down("mu"),
                    tensors::IndexSlot::down("nu")
                })
            );
        }

        return Curv2_->relabel({
            tensors::IndexSlot::up(a),
            tensors::IndexSlot::down(b),
            tensors::IndexSlot::down(mu),
            tensors::IndexSlot::down(nu)
        });
    }

auto RiemannianManifold::torsion_2form(
        const std::string& a,
        const std::string& mu,
        const std::string& nu 
) const -> tensors::IndexedTensor {
        if (!Torsion2_) {
            if (!vielbein_) compute_vielbein();
            auto e = vielbein("a", "rho"); 
            auto raw = tensors::SymbolicSquareTensor::zero(dim_, 3);

            for (std::size_t A=0; A<dim_; ++A)
                for (std::size_t M=0; M<dim_; ++M)
                    for (std::size_t N=0; N<dim_; ++N) {
                        cas::Expression dM = DIFFERENTIATE(e.at({A,N}), coords_[M]);
                        cas::Expression dN = DIFFERENTIATE(e.at({A,M}), coords_[N]);
                        raw.at({A,M,N}) = dM - dN;
                    }

            tensors::IndexedTensor dE(raw, {
                tensors::IndexSlot::up("a"),
                tensors::IndexSlot::down("mu"),
                tensors::IndexSlot::down("nu")
            });

            auto w_mu = connection_1form("a", "b", "mu");
            auto w_nu = connection_1form("a", "b", "nu");
            auto e_nu = vielbein("b", "nu");
            auto e_mu = vielbein("b", "mu");
            auto term1 = CONTRACT(w_mu, e_nu); 
            auto term2 = CONTRACT(w_nu, e_mu);
            auto T = dE + term1 - term2;

            Torsion2_ = std::make_unique<tensors::IndexedTensor>(
                T.relabel({
                    tensors::IndexSlot::up("a"),
                    tensors::IndexSlot::down("mu"),
                    tensors::IndexSlot::down("nu")
                })
            );
        }

        return Torsion2_->relabel({
            tensors::IndexSlot::up(a),
            tensors::IndexSlot::down(mu),
            tensors::IndexSlot::down(nu)
        });
    }

auto RiemannianManifold::ricci_rotation_coeffs(
        const std::string& a,
        const std::string& b,
        const std::string& c 
) const -> tensors::IndexedTensor {
        if (!RicciRot_) {
            auto w = connection_1form("a", "b", "mu");
            auto e = inverse_vielbein("c", "mu"); 
            auto omega_abc_mu = CONTRACT(w, e); 
            auto eta = tensors::SymbolicSquareTensor::zero(dim_, 2);
            for (std::size_t i=0; i<dim_; ++i) eta.at({i,i}) = cas::Const(1.0); 
            auto omega_down = LOWER(omega_abc_mu, "a", eta);

            RicciRot_ = std::make_unique<tensors::IndexedTensor>(
                omega_down.relabel({
                    tensors::IndexSlot::down("a"),
                    tensors::IndexSlot::down("b"),
                    tensors::IndexSlot::down("c")
                })
            );
        }

        return RicciRot_->relabel({
            tensors::IndexSlot::down(a),
            tensors::IndexSlot::down(b),
            tensors::IndexSlot::down(c)
        });
    }

auto RiemannianManifold::compute_determinant() const -> void {
        auto mat = g_.to_matrix();
        det_ = std::make_unique<cas::Expression>(FULL_SIMPLIFY(mat.determinant()));
    }

auto RiemannianManifold::compute_christoffel1() const -> void {
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 3);

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b)
                for (std::size_t c=0; c<dim_; ++c) {
                    cas::Expression dg_ac_db = DIFFERENTIATE(g_.at({a,c}), coords_[b]);
                    cas::Expression dg_ab_dc = DIFFERENTIATE(g_.at({a,b}), coords_[c]);
                    cas::Expression dg_bc_da = DIFFERENTIATE(g_.at({b,c}), coords_[a]);
                    result.at({a,b,c}) = FULL_SIMPLIFY((dg_ac_db + dg_ab_dc - dg_bc_da) / 2.0);
                }

        Gamma1_ = std::make_unique<tensors::ChristoffelSymbolsFirst>(std::move(result));
    }

auto RiemannianManifold::compute_christoffel2() const -> void {
        if (!Gamma1_) compute_christoffel1();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 3);

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b)
                for (std::size_t c=0; c<dim_; ++c) {
                cas::Expression sum = cas::Const(0.0);
                for (std::size_t d=0; d<dim_; ++d) sum = sum + g_inv_.at({a,d}) * Gamma1_->at({d,b,c});
                result.at({a,b,c}) = FULL_SIMPLIFY(sum);
            }

        Gamma2_ = std::make_unique<tensors::ChristoffelSymbolsSecond>(std::move(result));
    }

auto RiemannianManifold::compute_riemann() const -> void {
        if (!Gamma2_) compute_christoffel2();
        const auto& G = Gamma2_->raw();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 4);

        for (std::size_t rho=0; rho<dim_; ++rho)
            for (std::size_t sig=0; sig<dim_; ++sig)
                for (std::size_t mu=0; mu<dim_; ++mu)
                    for (std::size_t nu=0; nu<dim_; ++nu) {
                        cas::Expression dG_mu = cas::DIFFERENTIATE(G.at({rho,nu,sig}), coords_[mu]);
                        cas::Expression dG_nu = cas::DIFFERENTIATE(G.at({rho,mu,sig}), coords_[nu]);
                        cas::Expression sum = dG_mu - dG_nu;

                        for (std::size_t e=0; e<dim_; ++e)
                            sum = sum
                                + G.at({rho,mu,e}) * G.at({e,nu,sig})
                                - G.at({rho,nu,e}) * G.at({e,mu,sig});

                        result.at({rho,sig,mu,nu}) = FULL_SIMPLIFY(sum);
                    }

        Riem_ = std::make_unique<tensors::RiemannTensor>(std::move(result));
    }

auto RiemannianManifold::compute_riemann_lower() const -> void {
        if (!Riem_) compute_riemann();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 4);

        for (std::size_t r=0; r<dim_; ++r)
            for (std::size_t s=0; s<dim_; ++s)
                for (std::size_t m=0; m<dim_; ++m)
                    for (std::size_t n=0; n<dim_; ++n) {
                        cas::Expression sum = cas::Const(0.0);
                        for (std::size_t a=0; a<dim_; ++a) sum = sum + g_.at({r,a}) * Riem_->raw().at({a,s,m,n});
                        result.at({r,s,m,n}) = FULL_SIMPLIFY(sum);
                    }

        RiemLower_ = std::make_unique<tensors::RiemannTensor>(std::move(result));
    }

auto RiemannianManifold::compute_ricci() const -> void {
        if (!Riem_) compute_riemann();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 2);

        for (std::size_t mu=0; mu<dim_; ++mu)
            for (std::size_t nu=0; nu<dim_; ++nu) {
                cas::Expression sum = cas::Const(0.0);
                for (std::size_t sig=0; sig<dim_; ++sig) sum = sum + Riem_->raw().at({sig,mu,sig,nu});
                result.at({mu,nu}) = FULL_SIMPLIFY(sum);
            }

        Ric_ = std::make_unique<tensors::RicciTensor>(std::move(result));
    }

auto RiemannianManifold::compute_ricci_scalar() const -> void {
        if (!Ric_) compute_ricci();
        cas::Expression R = cas::Const(0.0);

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b)
                R = R + g_inv_.at({a,b}) * Ric_->raw().at({a,b});

        RicciScalar_ = std::make_unique<cas::Expression>(FULL_SIMPLIFY(R));
    }

auto RiemannianManifold::compute_einstein() const -> void {
        if (!Ric_) compute_ricci();
        if (!RicciScalar_) compute_ricci_scalar();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 2);
        cas::Expression R = *RicciScalar_;

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b) {
                cas::Expression val = Ric_->raw().at({a,b}) - cas::Const(0.5) * R * g_.at({a,b});
                result.at({a,b}) = FULL_SIMPLIFY(val);
            }

        tensors::IndexedTensor G(result, { tensors::IndexSlot::down("mu"), tensors::IndexSlot::down("nu") });
        Einstein_ = std::make_unique<tensors::IndexedTensor>(std::move(G));
    }

auto RiemannianManifold::compute_schouten() const -> void {
        if (!Ric_) compute_ricci();
        if (!RicciScalar_) compute_ricci_scalar();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 2);
        double n = static_cast<double>(dim_);

        if (dim_ <= 2) {
            Schouten_ = std::make_unique<tensors::IndexedTensor>(
                tensors::IndexedTensor(result, { tensors::IndexSlot::down("mu"), tensors::IndexSlot::down("nu") })
            );
            return;
        }

        cas::Expression R = *RicciScalar_;
        cas::Expression factor1 = cas::Const(1.0 / (n - 2.0));
        cas::Expression factor2 = cas::Const(1.0 / (2.0 * (n - 1.0)));

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b) {
                cas::Expression term = Ric_->raw().at({a,b}) - factor2 * R * g_.at({a,b});
                result.at({a,b}) = FULL_SIMPLIFY(factor1 * term);
            }

        tensors::IndexedTensor P(result, { tensors::IndexSlot::down("mu"), tensors::IndexSlot::down("nu") });
        Schouten_ = std::make_unique<tensors::IndexedTensor>(std::move(P));
    }

auto RiemannianManifold::compute_weyl() const -> void {
        if (!RiemLower_) compute_riemann_lower();
        if (!Schouten_) compute_schouten();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 4);
        const auto& R = RiemLower_->raw();
        const auto& P = Schouten_->tensor();

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b)
                for (std::size_t c=0; c<dim_; ++c)
                    for (std::size_t d=0; d<dim_; ++d) {
                        cas::Expression val = R.at({a,b,c,d})
                            - g_.at({a,c}) * P.at({b,d})
                            + g_.at({a,d}) * P.at({b,c})
                            + g_.at({b,c}) * P.at({a,d})
                            - g_.at({b,d}) * P.at({a,c});
                        result.at({a,b,c,d}) = FULL_SIMPLIFY(val);
                    }

        tensors::IndexedTensor C(result, {
            tensors::IndexSlot::down("rho"),
            tensors::IndexSlot::down("sigma"),
            tensors::IndexSlot::down("mu"),
            tensors::IndexSlot::down("nu")
        });

        Weyl_ = std::make_unique<tensors::IndexedTensor>(std::move(C));
    }

auto RiemannianManifold::compute_cotton() const -> void {
        if (!Schouten_) compute_schouten();
        if (!Gamma2_) compute_christoffel2();
        auto result = tensors::SymbolicSquareTensor::zero(dim_, 3);
        const auto& P = Schouten_->tensor();
        const auto& G = Gamma2_->raw();

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t b=0; b<dim_; ++b)
                for (std::size_t c=0; c<dim_; ++c) {
                    cas::Expression dPc = DIFFERENTIATE(P.at({a,b}), coords_[c]);
                    cas::Expression nabla_c = dPc;

                    for (std::size_t d=0; d<dim_; ++d) {
                        nabla_c = nabla_c
                            - G.at({d,a,c}) * P.at({d,b})
                            - G.at({d,b,c}) * P.at({a,d});
                    }

                    cas::Expression dPb = DIFFERENTIATE(P.at({a,c}), coords_[b]);
                    cas::Expression nabla_b = dPb;

                    for (std::size_t d=0; d<dim_; ++d) {
                        nabla_b = nabla_b
                            - G.at({d,a,b}) * P.at({d,c})
                            - G.at({d,c,b}) * P.at({a,d});
                    }

                    cas::Expression C_abc = FULL_SIMPLIFY(nabla_c - nabla_b);
                    result.at({a,b,c}) = C_abc;
                }

        tensors::IndexedTensor C(result, {
            tensors::IndexSlot::down("alpha"),
            tensors::IndexSlot::down("beta"),
            tensors::IndexSlot::down("gamma")
        });

        Cotton_ = std::make_unique<tensors::IndexedTensor>(std::move(C));
    }

auto RiemannianManifold::compute_kretschmann() const -> void {
        if (!RiemLower_) compute_riemann_lower();
        tensors::IndexedTensor R_down = RiemLower_->as_indexed_down_down_down_down("a","b","c","d");
        tensors::IndexedTensor R_up = R_down;
        R_up = RAISE(R_up, "a", g_inv_);
        R_up = RAISE(R_up, "b", g_inv_);
        R_up = RAISE(R_up, "c", g_inv_);
        R_up = RAISE(R_up, "d", g_inv_);
        auto contracted = CONTRACT(R_down, R_up); 
        cas::Expression K = contracted.flat_at(0);
        Kretsch_ = std::make_unique<cas::Expression>(FULL_SIMPLIFY(K));
    }

auto RiemannianManifold::compute_vielbein() const -> void {
        auto e = tensors::SymbolicSquareTensor::zero(dim_, 2);

        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t mu=0; mu<dim_; ++mu)
                e.at({a,mu}) = (a == mu) ? SQRT(ABS(g_.at({mu,mu}))) : cas::Const(0.0);

        vielbein_ = std::make_unique<tensors::IndexedTensor>(
            e,
            std::initializer_list<tensors::IndexSlot>{
                tensors::IndexSlot::up("a"),
                tensors::IndexSlot::down("mu")
            }
        );

        auto inv = tensors::SymbolicSquareTensor::zero(dim_, 2);
        for (std::size_t a=0; a<dim_; ++a)
            for (std::size_t mu=0; mu<dim_; ++mu)
                inv.at({mu,a}) = (a == mu) ? 1.0 / SQRT(ABS(g_.at({mu,mu}))) : cas::Const(0.0);

        inv_vielbein_ = std::make_unique<tensors::IndexedTensor>(
            inv,
            std::initializer_list<tensors::IndexSlot>{
                tensors::IndexSlot::down("mu"),
                tensors::IndexSlot::up("a")
            }
        );
    }

} // namespace geometry
} // namespace math
} // namespace fizmo
