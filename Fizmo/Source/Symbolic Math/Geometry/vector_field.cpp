#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace geometry {

auto VectorField1D::divergence() const -> ScalarField1D {
        if (!cache_div_) cache_div_ = std::make_unique<ScalarField1D>(cas::DIFFERENTIATE(f_, xv_), xv_);
        return *cache_div_;
    }

auto VectorField1D::laplacian() const -> VectorField1D {
        if (!cache_lap_) {
            cas::Expression lap = cas::DIFFERENTIATE(cas::DIFFERENTIATE(f_, xv_), xv_);
            cache_lap_ = std::make_unique<VectorField1D>(std::move(lap), xv_);
        }

        return *cache_lap_;
    }

auto VectorField1D::flow_integrand(const cas::Expression& x_t, const std::string& param) const -> cas::Expression {
        cas::Expression F_on_curve = parameterize(x_t);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        return F_on_curve * xp;
    }

auto VectorField1D::line_integral(const cas::Expression& x_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(x_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto VectorField2D::operator=(const VectorField2D& o) -> VectorField2D& { v_ = o.v_; xv_ = o.xv_; yv_ = o.yv_; cache_div_.reset(); cache_curl_.reset(); cache_jac_.reset(); cache_lap_.reset(); return *this; }

auto VectorField2D::divergence() const -> ScalarField2D {
        if (!cache_div_) cache_div_ = std::make_unique<ScalarField2D>(cas::DIFFERENTIATE(v_.x, xv_) + cas::DIFFERENTIATE(v_.y, yv_), xv_, yv_);
        return *cache_div_;
    }

auto VectorField2D::curl() const -> ScalarField2D {
        if (!cache_curl_) cache_curl_ = std::make_unique<ScalarField2D>(cas::DIFFERENTIATE(v_.y, xv_) - cas::DIFFERENTIATE(v_.x, yv_), xv_, yv_);
        return *cache_curl_;
    }

auto VectorField2D::is_conservative() const -> bool {
        cas::Expression dP_dy = cas::DIFFERENTIATE(v_.x, yv_);
        cas::Expression dQ_dx = cas::DIFFERENTIATE(v_.y, xv_);
        return dP_dy.symbolic_equals(dQ_dx);
    }

auto VectorField2D::jacobian() const -> matrices::SymbolicMatrix2x2 {
        if (!cache_jac_) {
            matrices::SymbolicMatrix2x2 J;
            J.get_data()[0][0] = cas::DIFFERENTIATE(v_.x, xv_); J.get_data()[0][1] = cas::DIFFERENTIATE(v_.x, yv_);
            J.get_data()[1][0] = cas::DIFFERENTIATE(v_.y, xv_); J.get_data()[1][1] = cas::DIFFERENTIATE(v_.y, yv_);
            cache_jac_ = std::make_unique<matrices::SymbolicMatrix2x2>(J);
        }

        return *cache_jac_;
    }

auto VectorField2D::laplacian() const -> VectorField2D {
        if (!cache_lap_) {
            auto lap = [&](const cas::Expression& f) {
                return cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, xv_), xv_) + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, yv_), yv_);
            };
            cache_lap_ = std::make_unique<VectorField2D>(vectors::SymbolicVector2{ lap(v_.x), lap(v_.y) }, xv_, yv_);
        }

        return *cache_lap_;
    }

auto VectorField2D::directional_derivative(const cas::Expression& dx, const cas::Expression& dy) const -> VectorField2D {
        auto dd = [&](const cas::Expression& f) {
            return dx * cas::DIFFERENTIATE(f, xv_) + dy * cas::DIFFERENTIATE(f, yv_);
        };

        return VectorField2D({ dd(v_.x), dd(v_.y) }, xv_, yv_);
    }

auto VectorField2D::line_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto VectorField2D::line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto VectorField2D::flux_integrand(const ParametricCurve2D& curve) const -> cas::Expression {
        auto F_on_curve = parameterize(curve);         
        auto& rp = curve.r_prime();                   
        return F_on_curve.x * rp.y - F_on_curve.y * rp.x;  
    }

auto VectorField2D::flow_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param) const -> cas::Expression {
        auto F_on_curve = parameterize(x_t, y_t, param);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        return F_on_curve.x * xp + F_on_curve.y * yp;
    }

auto VectorField2D::flux_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param) const -> cas::Expression {
        auto F_on_curve = parameterize(x_t, y_t, param);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        return F_on_curve.x * yp - F_on_curve.y * xp;
    }

auto VectorField2D::flow_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto VectorField2D::flow_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto VectorField2D::flux_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flux_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto VectorField2D::flux_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flux_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

VectorField3D::VectorField3D(cas::Expression fx, cas::Expression fy, cas::Expression fz, std::string xvar, std::string yvar, std::string zvar) : v_({ std::move(fx), std::move(fy), std::move(fz) }) , xv_(std::move(xvar)), yv_(std::move(yvar)), zv_(std::move(zvar)) {}

auto VectorField3D::operator=(const VectorField3D& o) -> VectorField3D& { v_ = o.v_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; cache_div_.reset(); cache_curl_.reset(); cache_jac_.reset(); cache_lap_.reset(); return *this; }

auto VectorField3D::divergence() const -> ScalarField3D {
        if (!cache_div_) cache_div_ = std::make_unique<ScalarField3D>(cas::DIFFERENTIATE(v_.x, xv_) + cas::DIFFERENTIATE(v_.y, yv_) + cas::DIFFERENTIATE(v_.z, zv_), xv_, yv_, zv_);
        return *cache_div_;
    }

auto VectorField3D::curl() const -> VectorField3D {
        if (!cache_curl_) cache_curl_ = std::make_unique<VectorField3D>(vectors::SymbolicVector3{
            cas::DIFFERENTIATE(v_.z, yv_) - cas::DIFFERENTIATE(v_.y, zv_),
            cas::DIFFERENTIATE(v_.x, zv_) - cas::DIFFERENTIATE(v_.z, xv_),
            cas::DIFFERENTIATE(v_.y, xv_) - cas::DIFFERENTIATE(v_.x, yv_)
        }, xv_, yv_, zv_);
        return *cache_curl_;
    }

auto VectorField3D::is_conservative() const -> bool {
        cas::Expression dP_dy = cas::DIFFERENTIATE(v_.x, yv_);
        cas::Expression dQ_dx = cas::DIFFERENTIATE(v_.y, xv_);
        if (!dP_dy.symbolic_equals(dQ_dx)) return false;
        cas::Expression dP_dz = cas::DIFFERENTIATE(v_.x, zv_);
        cas::Expression dR_dx = cas::DIFFERENTIATE(v_.z, xv_);
        if (!dP_dz.symbolic_equals(dR_dx)) return false;
        cas::Expression dQ_dz = cas::DIFFERENTIATE(v_.y, zv_);
        cas::Expression dR_dy = cas::DIFFERENTIATE(v_.z, yv_);
        return dQ_dz.symbolic_equals(dR_dy);
    }

auto VectorField3D::jacobian() const -> matrices::SymbolicMatrix3x3 {
        if (!cache_jac_) {
            matrices::SymbolicMatrix3x3 J;
            J.get_data()[0][0] = cas::DIFFERENTIATE(v_.x, xv_); J.get_data()[0][1] = cas::DIFFERENTIATE(v_.x, yv_); J.get_data()[0][2] = cas::DIFFERENTIATE(v_.x, zv_);
            J.get_data()[1][0] = cas::DIFFERENTIATE(v_.y, xv_); J.get_data()[1][1] = cas::DIFFERENTIATE(v_.y, yv_); J.get_data()[1][2] = cas::DIFFERENTIATE(v_.y, zv_);
            J.get_data()[2][0] = cas::DIFFERENTIATE(v_.z, xv_); J.get_data()[2][1] = cas::DIFFERENTIATE(v_.z, yv_); J.get_data()[2][2] = cas::DIFFERENTIATE(v_.z, zv_);
            cache_jac_ = std::make_unique<matrices::SymbolicMatrix3x3>(J);
        }

        return *cache_jac_;
    }

auto VectorField3D::laplacian() const -> VectorField3D {
        if (!cache_lap_) {
            auto lap = [&](const cas::Expression& f) {
                return cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, xv_), xv_) + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, yv_), yv_) + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, zv_), zv_);
            };
            cache_lap_ = std::make_unique<VectorField3D>(vectors::SymbolicVector3{ lap(v_.x), lap(v_.y), lap(v_.z) }, xv_, yv_, zv_);
        }

        return *cache_lap_;
    }

auto VectorField3D::cross(const VectorField3D& g) const -> VectorField3D {
        return VectorField3D(
            {
                v_.y * g.v_.z - v_.z * g.v_.y,
                v_.z * g.v_.x - v_.x * g.v_.z,
                v_.x * g.v_.y - v_.y * g.v_.x
            },
            xv_, yv_, zv_
        );
    }

auto VectorField3D::directional_derivative(const cas::Expression& dx, const cas::Expression& dy, const cas::Expression& dz) const -> VectorField3D {
        auto dd = [&](const cas::Expression& f) {
            return dx * cas::DIFFERENTIATE(f, xv_) + dy * cas::DIFFERENTIATE(f, yv_) + dz * cas::DIFFERENTIATE(f, zv_);
        };
        return VectorField3D({ dd(v_.x), dd(v_.y), dd(v_.z) }, xv_, yv_, zv_);
    }

auto VectorField3D::flow_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param) const -> cas::Expression {
        auto F_on_curve = parameterize(x_t, y_t, z_t, param);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        cas::Expression zp = cas::DIFFERENTIATE(z_t, param);
        return F_on_curve.x * xp + F_on_curve.y * yp + F_on_curve.z * zp;
    }

auto VectorField3D::flux_integrand(const ParametricSurface& surface) const -> cas::Expression {
        vectors::SymbolicVector3 F_on_surface = v_.substitute({
            { xv_, surface.r().x },
            { yv_, surface.r().y },
            { zv_, surface.r().z }
        });
        const auto& n = surface.normal();
        return F_on_surface.x * n.x + F_on_surface.y * n.y + F_on_surface.z * n.z;
    }

auto VectorField3D::line_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto VectorField3D::line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(x_t, y_t, z_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto VectorField3D::flow_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto VectorField3D::flow_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = flow_integrand(x_t, y_t, z_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto VectorField3D::flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            flux_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto VectorField3D::flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        cas::Expression integrand = flux_integrand(surface);
        const std::string& ov = outer;
        const std::string& iv = inner;

        auto f2d = [integrand, ov, iv](double o, double i) -> double {
            return integrand.evaluate({{ ov, o }, { iv, i }});
        };

        return integration::Integrator2D::integrate(
            f2d, outer_lo, outer_hi,
            std::move(inner_lo), std::move(inner_hi), cfg
        );
    }

auto VectorField3D::flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            flux_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

VectorField4D::VectorField4D(cas::Expression fx, cas::Expression fy, cas::Expression fz, cas::Expression fw,
                  std::string xvar, std::string yvar,
                  std::string zvar, std::string wvar) : v_({ std::move(fx), std::move(fy), std::move(fz), std::move(fw) }),
          xv_(std::move(xvar)), yv_(std::move(yvar)),
          zv_(std::move(zvar)), wv_(std::move(wvar)) {}

VectorField4D::VectorField4D(vectors::SymbolicVector4 v,
                  std::string xvar, std::string yvar,
                  std::string zvar, std::string wvar) : v_(std::move(v)), xv_(std::move(xvar)), yv_(std::move(yvar)),
          zv_(std::move(zvar)), wv_(std::move(wvar)) {}

auto VectorField4D::operator=(const VectorField4D& o) -> VectorField4D& {
        v_ = o.v_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; wv_ = o.wv_;
        cache_div_.reset(); cache_jac_.reset(); cache_lap_.reset();
        return *this;
    }

auto VectorField4D::divergence() const -> ScalarField4D {
        if (!cache_div_) {
            cache_div_ = std::make_unique<ScalarField4D>(
                cas::DIFFERENTIATE(v_.x, xv_) + cas::DIFFERENTIATE(v_.y, yv_) +
                cas::DIFFERENTIATE(v_.z, zv_) + cas::DIFFERENTIATE(v_.w, wv_),
                xv_, yv_, zv_, wv_
            );
        }
        return *cache_div_;
    }

auto VectorField4D::jacobian() const -> matrices::SymbolicMatrix4x4 {
        if (!cache_jac_) {
            matrices::SymbolicMatrix4x4 J;
            auto& d = J.get_data();
            d[0][0] = cas::DIFFERENTIATE(v_.x, xv_); d[0][1] = cas::DIFFERENTIATE(v_.x, yv_);
            d[0][2] = cas::DIFFERENTIATE(v_.x, zv_); d[0][3] = cas::DIFFERENTIATE(v_.x, wv_);
            d[1][0] = cas::DIFFERENTIATE(v_.y, xv_); d[1][1] = cas::DIFFERENTIATE(v_.y, yv_);
            d[1][2] = cas::DIFFERENTIATE(v_.y, zv_); d[1][3] = cas::DIFFERENTIATE(v_.y, wv_);
            d[2][0] = cas::DIFFERENTIATE(v_.z, xv_); d[2][1] = cas::DIFFERENTIATE(v_.z, yv_);
            d[2][2] = cas::DIFFERENTIATE(v_.z, zv_); d[2][3] = cas::DIFFERENTIATE(v_.z, wv_);
            d[3][0] = cas::DIFFERENTIATE(v_.w, xv_); d[3][1] = cas::DIFFERENTIATE(v_.w, yv_);
            d[3][2] = cas::DIFFERENTIATE(v_.w, zv_); d[3][3] = cas::DIFFERENTIATE(v_.w, wv_);
            cache_jac_ = std::make_unique<matrices::SymbolicMatrix4x4>(J);
        }
        return *cache_jac_;
    }

auto VectorField4D::laplacian() const -> VectorField4D {
        if (!cache_lap_) {
            auto lap = [&](const cas::Expression& f) {
                return cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, xv_), xv_)
                     + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, yv_), yv_)
                     + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, zv_), zv_)
                     + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, wv_), wv_);
            };
            cache_lap_ = std::make_unique<VectorField4D>(
                vectors::SymbolicVector4{ lap(v_.x), lap(v_.y), lap(v_.z), lap(v_.w) },
                xv_, yv_, zv_, wv_
            );
        }
        return *cache_lap_;
    }

auto VectorField4D::is_conservative() const -> bool {
        const cas::Expression* comps[4] = { &v_.x, &v_.y, &v_.z, &v_.w };
        const std::string* vars[4] = { &xv_,  &yv_,  &zv_,  &wv_  };
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = i + 1; j < 4; ++j)
                if (!cas::DIFFERENTIATE(*comps[i], *vars[j]).symbolic_equals(
                     cas::DIFFERENTIATE(*comps[j], *vars[i])))
                    return false;
        return true;
    }

auto VectorField4D::directional_derivative(const cas::Expression& dx, const cas::Expression& dy,
                                         const cas::Expression& dz, const cas::Expression& dw) const -> VectorField4D {
        auto dd = [&](const cas::Expression& f) {
            return dx * cas::DIFFERENTIATE(f, xv_) + dy * cas::DIFFERENTIATE(f, yv_)
                 + dz * cas::DIFFERENTIATE(f, zv_) + dw * cas::DIFFERENTIATE(f, wv_);
        };
        return VectorField4D({ dd(v_.x), dd(v_.y), dd(v_.z), dd(v_.w) }, xv_, yv_, zv_, wv_);
    }

VectorFieldN::VectorFieldN(vectors::SymbolicVectorN v, std::vector<std::string> vars) : v_(std::move(v)), vars_(std::move(vars)) {
        if (v_.dimension() != vars_.size())
            throw std::invalid_argument("VectorFieldN: component count ("
                + std::to_string(v_.dimension()) + ") != variable count (" + std::to_string(vars_.size()) + ")");
    }

auto VectorFieldN::operator=(const VectorFieldN& o) -> VectorFieldN& {
        v_ = o.v_; vars_ = o.vars_;
        cache_div_.reset(); cache_jac_.reset(); cache_lap_.reset();
        return *this;
    }

auto VectorFieldN::divergence() const -> ScalarFieldN {
        if (!cache_div_) {
            cas::Expression d = cas::DIFFERENTIATE(v_[0], vars_[0]);
            for (std::size_t i = 1; i < dimension(); ++i) d = d + cas::DIFFERENTIATE(v_[i], vars_[i]);
            cache_div_ = std::make_unique<ScalarFieldN>(std::move(d), vars_);
        }

        return *cache_div_;
    }

auto VectorFieldN::jacobian() const -> matrices::SymbolicMatrixN {
        if (!cache_jac_) {
            std::size_t n = dimension();
            matrices::SymbolicMatrixN J(n);

            for (std::size_t i = 0; i < n; ++i)
                for (std::size_t j = 0; j < n; ++j)
                    J.at(i, j) = cas::DIFFERENTIATE(v_[i], vars_[j]);

            cache_jac_ = std::make_unique<matrices::SymbolicMatrixN>(std::move(J));
        }
        return *cache_jac_;
    }

auto VectorFieldN::laplacian() const -> VectorFieldN {
        if (!cache_lap_) {
            std::size_t n = dimension();
            vectors::SymbolicVectorN lap(n);

            for (std::size_t i = 0; i < n; ++i) {
                cas::Expression l = cas::DIFFERENTIATE(cas::DIFFERENTIATE(v_[i], vars_[0]), vars_[0]);
                for (std::size_t j = 1; j < n; ++j) l = l + cas::DIFFERENTIATE(cas::DIFFERENTIATE(v_[i], vars_[j]), vars_[j]);
                lap[i] = std::move(l);
            }

            cache_lap_ = std::make_unique<VectorFieldN>(std::move(lap), vars_);
        }
        return *cache_lap_;
    }

auto VectorFieldN::curl() const -> VectorFieldN {
        if (dimension() == 3) {
            return VectorFieldN(vectors::SymbolicVectorN({
                cas::DIFFERENTIATE(v_[2], vars_[1]) - cas::DIFFERENTIATE(v_[1], vars_[2]),
                cas::DIFFERENTIATE(v_[0], vars_[2]) - cas::DIFFERENTIATE(v_[2], vars_[0]),
                cas::DIFFERENTIATE(v_[1], vars_[0]) - cas::DIFFERENTIATE(v_[0], vars_[1])
            }), vars_);
        }
        if (dimension() == 7) {
            const auto& F = v_;
            const auto& x = vars_;

            return VectorFieldN(vectors::SymbolicVectorN({
            /* e0 */ cas::DIFFERENTIATE(F[3],x[1]) - cas::DIFFERENTIATE(F[1],x[3])
                    + cas::DIFFERENTIATE(F[6],x[2]) - cas::DIFFERENTIATE(F[2],x[6])
                    + cas::DIFFERENTIATE(F[5],x[4]) - cas::DIFFERENTIATE(F[4],x[5]),

            /* e1 */ cas::DIFFERENTIATE(F[4],x[2]) - cas::DIFFERENTIATE(F[2],x[4])
                    + cas::DIFFERENTIATE(F[0],x[3]) - cas::DIFFERENTIATE(F[3],x[0])
                    + cas::DIFFERENTIATE(F[6],x[5]) - cas::DIFFERENTIATE(F[5],x[6]),

            /* e2 */ cas::DIFFERENTIATE(F[5],x[3]) - cas::DIFFERENTIATE(F[3],x[5])
                    + cas::DIFFERENTIATE(F[1],x[4]) - cas::DIFFERENTIATE(F[4],x[1])
                    + cas::DIFFERENTIATE(F[0],x[6]) - cas::DIFFERENTIATE(F[6],x[0]),

            /* e3 */ cas::DIFFERENTIATE(F[6],x[4]) - cas::DIFFERENTIATE(F[4],x[6])
                    + cas::DIFFERENTIATE(F[2],x[5]) - cas::DIFFERENTIATE(F[5],x[2])
                    + cas::DIFFERENTIATE(F[1],x[0]) - cas::DIFFERENTIATE(F[0],x[1]),

            /* e4 */ cas::DIFFERENTIATE(F[0],x[5]) - cas::DIFFERENTIATE(F[5],x[0])
                    + cas::DIFFERENTIATE(F[3],x[6]) - cas::DIFFERENTIATE(F[6],x[3])
                    + cas::DIFFERENTIATE(F[2],x[1]) - cas::DIFFERENTIATE(F[1],x[2]),

            /* e5 */ cas::DIFFERENTIATE(F[1],x[6]) - cas::DIFFERENTIATE(F[6],x[1])
                    + cas::DIFFERENTIATE(F[4],x[0]) - cas::DIFFERENTIATE(F[0],x[4])
                    + cas::DIFFERENTIATE(F[3],x[2]) - cas::DIFFERENTIATE(F[2],x[3]),

            /* e6 */ cas::DIFFERENTIATE(F[2],x[0]) - cas::DIFFERENTIATE(F[0],x[2])
                    + cas::DIFFERENTIATE(F[5],x[1]) - cas::DIFFERENTIATE(F[1],x[5])
                    + cas::DIFFERENTIATE(F[4],x[3]) - cas::DIFFERENTIATE(F[3],x[4])
            }), vars_);
        }
        throw std::domain_error("VectorFieldN::curl requires dimension 3 or 7");
    }

auto VectorFieldN::dot(const VectorFieldN& g) const -> ScalarFieldN {
        assert(dimension() == g.dimension());
        cas::Expression d = v_[0] * g.v_[0];
        for (std::size_t i = 1; i < dimension(); ++i) d = d + v_[i] * g.v_[i];
        return ScalarFieldN(std::move(d), vars_);
    }

auto VectorFieldN::cross(const VectorFieldN& g) const -> VectorFieldN {
        if (dimension() == 3) {
            return VectorFieldN(vectors::SymbolicVectorN({
                v_[1] * g.v_[2] - v_[2] * g.v_[1],
                v_[2] * g.v_[0] - v_[0] * g.v_[2],
                v_[0] * g.v_[1] - v_[1] * g.v_[0]
            }), vars_);
        }
        if (dimension() == 7) {
            const auto& a = v_;
            const auto& b = g.v_;
            return VectorFieldN(vectors::SymbolicVectorN({
                /* e0 */ a[1]*b[3] - a[3]*b[1] + a[2]*b[6] - a[6]*b[2] + a[4]*b[5] - a[5]*b[4],
                /* e1 */ a[2]*b[4] - a[4]*b[2] + a[3]*b[0] - a[0]*b[3] + a[5]*b[6] - a[6]*b[5],
                /* e2 */ a[3]*b[5] - a[5]*b[3] + a[4]*b[1] - a[1]*b[4] + a[6]*b[0] - a[0]*b[6],
                /* e3 */ a[4]*b[6] - a[6]*b[4] + a[5]*b[2] - a[2]*b[5] + a[0]*b[1] - a[1]*b[0],
                /* e4 */ a[5]*b[0] - a[0]*b[5] + a[6]*b[3] - a[3]*b[6] + a[1]*b[2] - a[2]*b[1],
                /* e5 */ a[6]*b[1] - a[1]*b[6] + a[0]*b[4] - a[4]*b[0] + a[2]*b[3] - a[3]*b[2],
                /* e6 */ a[0]*b[2] - a[2]*b[0] + a[1]*b[5] - a[5]*b[1] + a[3]*b[4] - a[4]*b[3]
            }), vars_);
        }
        throw std::domain_error("VectorFieldN::cross requires dimension 3 or 7");
    }

auto VectorFieldN::is_conservative() const -> bool {
        std::size_t n = dimension();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                cas::Expression dFi_dxj = cas::DIFFERENTIATE(v_[i], vars_[j]);
                cas::Expression dFj_dxi = cas::DIFFERENTIATE(v_[j], vars_[i]);
                if (!dFi_dxj.symbolic_equals(dFj_dxi)) return false;
            }
        }
        return true;
    }

auto VectorFieldN::directional_derivative(const vectors::SymbolicVectorN& d) const -> VectorFieldN {
        assert(d.dimension() == dimension());
        std::size_t n = dimension();
        vectors::SymbolicVectorN result(n);

        for (std::size_t i = 0; i < n; ++i) {
            cas::Expression dd = d[0] * cas::DIFFERENTIATE(v_[i], vars_[0]);
            for (std::size_t j = 1; j < n; ++j)
                dd = dd + d[j] * cas::DIFFERENTIATE(v_[i], vars_[j]);
            result[i] = std::move(dd);
        }

        return VectorFieldN(std::move(result), vars_);
    }

auto VectorFieldN::evaluate(const std::vector<double>& vals) const -> std::vector<double> {
        std::unordered_map<std::string, double> m;
        for (std::size_t i = 0; i < std::min(vals.size(), vars_.size()); ++i) m[vars_[i]] = vals[i];
        return v_.evaluate(m);
    }

} // namespace geometry
} // namespace math
} // namespace fizmo
