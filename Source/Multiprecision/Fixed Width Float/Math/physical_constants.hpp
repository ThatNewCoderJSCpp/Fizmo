#ifndef FIZMO_MULTIPRECISION_FIXED_PHYSICAL_CONSTANTS_HPP
#define FIZMO_MULTIPRECISION_FIXED_PHYSICAL_CONSTANTS_HPP

#include "Details/bigfloat_bridge.hpp"
#include "../../Big/BigFloat Math/physical_constants.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {
namespace fpdetail {

template <class F>
using require_constant_t = typename std::enable_if<math::bdetail::is_mp_float<F>::value, PhysicalConstant<F>>::type;

template <class F>
BigFloatContext context_of() { return math::bdetail::ctx_of(F::mantissa_bits + 1 + 16); }

template <class F>
PhysicalConstant<F> narrow(const PhysicalConstant<BigFloat>& m, const F& value) {
    PhysicalConstant<F> r;
    r.value                = value;
    r.uncertainty          = math::bdetail::exact<F>(m.uncertainty);
    r.relative_uncertainty = math::bdetail::exact<F>(m.relative_uncertainty);
    r.exact                = m.exact;
    r.name                 = m.name;
    r.symbol               = m.symbol;
    r.unit                 = m.unit;
    return r;
}

template <class F>
PhysicalConstant<BigFloat> widen(const PhysicalConstant<F>& m) {
    PhysicalConstant<BigFloat> r;
    r.value                = BigFloat(m.value);
    r.uncertainty          = BigFloat(m.uncertainty);
    r.relative_uncertainty = BigFloat(m.relative_uncertainty);
    r.exact                = m.exact;
    r.name                 = m.name;
    r.symbol               = m.symbol;
    r.unit                 = m.unit;
    return r;
}

template <class F, class Op>
PhysicalConstant<F> apply(Op op) {
    const ScopedContext guard(context_of<F>());
    const PhysicalConstant<BigFloat> r = op();
    return narrow<F>(r, math::bdetail::exact<F>(r.value));
}

} // namespace fpdetail

template <class F>
F ConstantDefinition::value() const {
    static_assert(math::bdetail::is_mp_float<F>::value, "ConstantDefinition::value<F> needs a floatmp type");
    return math::bdetail::eval<F>([this](const BigFloatContext& c) { return value(c); });
}

template <class F>
PhysicalConstant<F> ConstantDefinition::evaluate() const {
    static_assert(math::bdetail::is_mp_float<F>::value, "ConstantDefinition::evaluate<F> needs a floatmp type");
    return fpdetail::narrow<F>(evaluate(fpdetail::context_of<F>()), value<F>());
}

template <class F>
typename std::enable_if<math::bdetail::is_mp_float<F>::value, ParticleProperties<F>>::type properties(particle p) {
    ParticleProperties<F> r;
    r.id           = p;
    r.name         = name(p);
    r.mass_kg      = mass(p, mass_unit::kg).template evaluate<F>();
    r.mass_mev     = mass(p, mass_unit::MeV).template evaluate<F>();
    r.mass_dalton  = mass(p, mass_unit::dalton).template evaluate<F>();
    r.has_g_factor = has_g_factor(p);
    if (r.has_g_factor) r.g_factor = g_factor(p).template evaluate<F>();
    else                r.g_factor = PhysicalConstant<F>{F::undefined(), F::undefined(), F::undefined(), false, "", "", ""};
    return r;
}

template <class F>
fpdetail::require_constant_t<F> operator*(const PhysicalConstant<F>& a, const PhysicalConstant<F>& b) {
    return fpdetail::apply<F>([&] { return fpdetail::widen(a) * fpdetail::widen(b); });
}

template <class F>
fpdetail::require_constant_t<F> operator/(const PhysicalConstant<F>& a, const PhysicalConstant<F>& b) {
    return fpdetail::apply<F>([&] { return fpdetail::widen(a) / fpdetail::widen(b); });
}

template <class F>
fpdetail::require_constant_t<F> operator*(const PhysicalConstant<F>& a, const F& b) {
    return fpdetail::apply<F>([&] { return fpdetail::widen(a) * BigFloat(b); });
}

template <class F>
fpdetail::require_constant_t<F> operator*(const F& a, const PhysicalConstant<F>& b) {
    return fpdetail::apply<F>([&] { return BigFloat(a) * fpdetail::widen(b); });
}

template <class F>
fpdetail::require_constant_t<F> operator/(const PhysicalConstant<F>& a, const F& b) {
    return fpdetail::apply<F>([&] { return fpdetail::widen(a) / BigFloat(b); });
}

template <class F>
fpdetail::require_constant_t<F> pow(const PhysicalConstant<F>& a, int k) {
    return fpdetail::apply<F>([&] { return pow(fpdetail::widen(a), k); });
}

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FIXED_PHYSICAL_CONSTANTS_HPP
