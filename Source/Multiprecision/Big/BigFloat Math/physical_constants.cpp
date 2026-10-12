#include "fizmo_library.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <string>

namespace fizmo {
namespace multiprecision {
namespace constants {
namespace pcdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;
using CD  = ConstantDefinition;

enum class atom_kind : std::uint8_t { measured = 0, pi, wien_wavelength, wien_frequency, wien_entropy, weinberg };

struct Atom {
    atom_kind     kind = atom_kind::measured;
    BigInt        num;
    BigInt        den;
    BigInt        unc_num;
    BigInt        unc_den;
    std::uint32_t source = 0;
};

std::mutex& registry_mutex() {
    static std::mutex m;
    return m;
}

std::deque<Atom>& registry() {
    static std::deque<Atom> atoms;
    return atoms;
}

std::uint32_t add_atom(const Atom& a) {
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry().push_back(a);
    return static_cast<std::uint32_t>(registry().size() - 1);
}

Atom get_atom(std::uint32_t id) {
    std::lock_guard<std::mutex> lock(registry_mutex());
    return registry()[id];
}

std::uint32_t special_atom(atom_kind kind) {
    Atom a;
    a.kind = kind;
    a.num  = BigInt::one();
    a.den  = BigInt::one();
    return add_atom(a);
}

std::uint32_t pi_atom() {
    static const std::uint32_t id = special_atom(atom_kind::pi);
    return id;
}

BigInt ipow(const BigInt& b, unsigned k) {
    BigInt r = BigInt::one();
    for (unsigned i = 0; i < k; ++i) r = r * b;
    return r;
}

BigInt pow10(int k) { return ipow(BigInt(10LL), static_cast<unsigned>(k)); }

void set_decimal(std::int64_t mantissa, int exp10, BigInt& num, BigInt& den) {
    num = BigInt(static_cast<long long>(mantissa));
    den = BigInt::one();
    if (exp10 >= 0) num = num * pow10(exp10);
    else            den = pow10(-exp10);
}

BFC wctx(std::size_t p) { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }

BF ratio(const BigInt& num, const BigInt& den, const BFC& c) { return BF::div(BF(num), BF(den), c); }

BF ipow_bf(const BF& x, int k, const BFC& c) {
    if (k == 0) return BF::one();
    const unsigned m = static_cast<unsigned>(k < 0 ? -k : k);
    BF y = x;
    for (unsigned i = 1; i < m; ++i) y = BF::mul(y, x, c);
    return k < 0 ? BF::div(BF::one(), y, c) : y;
}

BF wien_root(atom_kind kind, std::size_t wp) {
    const BFC c = wctx(wp + 16);
    if (kind != atom_kind::wien_entropy) {
        const std::uint64_t n = kind == atom_kind::wien_wavelength ? 5 : 3;
        const BF z = BF::mul(math::exp(BF(n).with_sign(true), c), BF(n), c).with_sign(true);
        return BF::add(BF(n), math::lambert_w0(z, c), c);
    }
    const BF one = BF::one();
    const BF four(std::uint64_t(4));
    const BF six(std::uint64_t(6));
    BF x(4.79126735780448);
    for (int it = 0; it < 200; ++it) {
        const BF E    = math::exp(x, c);
        const BF em1  = BF::sub(E, one, c);
        const BF r    = BF::div(one, em1, c);
        const BF q    = BF::add(one, r, c);
        const BF L    = math::ln(em1, c);
        const BF rq   = BF::mul(r, q, c);
        const BF f    = BF::sub(BF::mul(four, BF::sub(BF::mul(x, q, c), L, c), c), BF::mul(BF::mul(x, x, c), rq, c), c);
        const BF fp   = BF::mul(BF::mul(rq, x, c), BF::sub(BF::mul(x, BF::add(q, r, c), c), six, c), c);
        const BF step = BF::div(f, fp, c);
        x = BF::sub(x, step, c);
        if (step.is_zero() || step.get_exp_base2() < x.get_exp_base2() - static_cast<std::int64_t>(wp) - 12) break;
    }
    return x;
}

BF atom_value(const Atom& a, std::size_t wp) {
    switch (a.kind) {
        case atom_kind::pi: return constants::pi(wctx(wp));
        case atom_kind::weinberg: {
            const BFC  c = wctx(wp + 8);
            const Atom s = get_atom(a.source);
            return math::arcsin(math::sqrt(ratio(s.num, s.den, c), c), c);
        }
        case atom_kind::measured: return ratio(a.num, a.den, wctx(wp));
        default: return wien_root(a.kind, wp);
    }
}

std::size_t atom_error_ulps(atom_kind k) {
    switch (k) {
        case atom_kind::pi:       return 1;
        case atom_kind::weinberg: return 8;
        default:                  return 4;
    }
}

template <class Raw>
BF drive(const BFC& ctx, std::size_t err_ulps, Raw raw) {
    const std::size_t err_bits = static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(err_ulps) + 1.0))) + 1;
    std::size_t       guard    = 32;
    for (;;) {
        const std::size_t wp = BFC::clamp_precision(ctx.precision + guard);
        const BF          v  = raw(wp);
        if (!v.is_finite() || v.is_zero()) return v;
        BigUInt           sig = v.significand();
        const std::size_t L   = sig.bit_length();
        if (L < wp) sig.shift_left_mutable(wp - L);
        const std::size_t err = err_bits + ((L > wp) ? (L - wp) : 0);
        if (constants::bfdetail::round_is_safe(sig, ctx.precision, err + 2)) return v.rounded(ctx);
        if (guard >= 8192) return v.rounded(ctx);
        guard *= 2;
    }
}

struct access {
    static CD with_atom(std::uint32_t id) {
        CD d;
        d.m_factors.push_back({id, 1});
        return d;
    }

    static CD multiply(const CD& a, const CD& b, int sign) {
        CD r;
        r.m_defined = a.m_defined && b.m_defined;
        if (sign > 0) {
            r.m_num = a.m_num * b.m_num;
            r.m_den = a.m_den * b.m_den;
        } else {
            r.m_num = a.m_num * b.m_den;
            r.m_den = a.m_den * b.m_num;
        }
        if (r.m_den.is_negative()) {
            r.m_num = r.m_num * BigInt(-1LL);
            r.m_den = r.m_den * BigInt(-1LL);
        }
        std::size_t i = 0, j = 0;
        while (i < a.m_factors.size() || j < b.m_factors.size()) {
            if (j == b.m_factors.size() || (i < a.m_factors.size() && a.m_factors[i].atom < b.m_factors[j].atom)) {
                r.m_factors.push_back(a.m_factors[i++]);
            } else if (i == a.m_factors.size() || b.m_factors[j].atom < a.m_factors[i].atom) {
                r.m_factors.push_back({b.m_factors[j].atom, sign * b.m_factors[j].power});
                ++j;
            } else {
                const int p = a.m_factors[i].power + sign * b.m_factors[j].power;
                if (p != 0) r.m_factors.push_back({a.m_factors[i].atom, p});
                ++i;
                ++j;
            }
        }
        return r;
    }

    static CD power(const CD& a, int k) {
        CD r;
        r.m_defined = a.m_defined;
        if (k == 0) return r;
        const unsigned m = static_cast<unsigned>(k < 0 ? -k : k);
        BigInt n = ipow(a.m_num, m);
        BigInt d = ipow(a.m_den, m);
        if (k < 0) std::swap(n, d);
        if (d.is_negative()) {
            n = n * BigInt(-1LL);
            d = d * BigInt(-1LL);
        }
        r.m_num = n;
        r.m_den = d;
        for (const auto& f : a.m_factors) r.m_factors.push_back({f.atom, f.power * k});
        return r;
    }

    static BF value(const CD& d, const BFC& ctx) {
        if (!d.m_defined || d.m_den.is_zero()) return BF::undefined();
        BigInt num = d.m_num;
        BigInt den = d.m_den;
        std::vector<std::pair<Atom, int>> rest;
        std::size_t ulps = 4;
        for (const auto& f : d.m_factors) {
            const Atom a = get_atom(f.atom);
            const unsigned m = static_cast<unsigned>(f.power < 0 ? -f.power : f.power);
            if (a.kind == atom_kind::measured) {
                if (f.power > 0) {
                    num = num * ipow(a.num, m);
                    den = den * ipow(a.den, m);
                } else {
                    num = num * ipow(a.den, m);
                    den = den * ipow(a.num, m);
                }
            } else {
                rest.push_back({a, f.power});
                ulps += m * (atom_error_ulps(a.kind) + 2) + 1;
            }
        }
        if (den.is_negative()) {
            num = num * BigInt(-1LL);
            den = den * BigInt(-1LL);
        }
        if (rest.empty()) return ratio(num, den, ctx);
        return drive(ctx, ulps, [&](std::size_t wp) {
            const BFC c = wctx(wp);
            BF v = ratio(num, den, c);
            for (const auto& t : rest) v = BF::mul(v, ipow_bf(atom_value(t.first, wp), t.second, c), c);
            return v;
        });
    }

    static bool exact(const CD& d) {
        for (const auto& f : d.m_factors) {
            const Atom a = get_atom(f.atom);
            if (a.kind == atom_kind::weinberg) return false;
            if (a.kind == atom_kind::measured && !a.unc_num.is_zero()) return false;
        }
        return true;
    }

    static BF relative_uncertainty(const CD& d, const BFC& c) {
        if (!d.m_defined) return BF::undefined();
        BF sum = BF::zero();
        for (const auto& f : d.m_factors) {
            const Atom a = get_atom(f.atom);
            BF rel = BF::zero();
            if (a.kind == atom_kind::measured && !a.unc_num.is_zero()) {
                rel = BF::div(ratio(a.unc_num, a.unc_den, c), ratio(a.num, a.den, c).abs(), c);
            } else if (a.kind == atom_kind::weinberg) {
                const Atom s  = get_atom(a.source);
                const BF   sv = ratio(s.num, s.den, c);
                const BF   us = ratio(s.unc_num, s.unc_den, c);
                const BF   th = math::arcsin(math::sqrt(sv, c), c);
                const BF   dd = BF::mul(math::sqrt(BF::mul(sv, BF::sub(BF::one(), sv, c), c), c), th, c).scaled_pow2(1);
                rel = BF::div(us, dd, c);
            } else {
                continue;
            }
            const BF term = BF::mul(rel, BF(static_cast<std::uint64_t>(f.power < 0 ? -f.power : f.power)), c);
            sum = BF::add(sum, BF::mul(term, term, c), c);
        }
        return sum.is_zero() ? sum : math::sqrt(sum, c);
    }

    static CD make_weinberg(const CD& sin2) {
        Atom a;
        a.kind   = atom_kind::weinberg;
        a.num    = BigInt::one();
        a.den    = BigInt::one();
        a.source = sin2.m_factors.front().atom;
        return with_atom(add_atom(a));
    }
};

PhysicalConstant<BF> combine(const BF& v, const BF& rel, const BFC& ctx) {
    PhysicalConstant<BF> r;
    r.value = v;
    r.exact = rel.is_zero();
    if (r.exact || !v.is_finite() || !rel.is_finite()) {
        r.uncertainty          = r.exact ? BF::zero() : BF::undefined();
        r.relative_uncertainty = r.exact ? BF::zero() : BF::undefined();
        return r;
    }
    r.relative_uncertainty = rel.rounded(ctx);
    r.uncertainty          = BF::mul(v.abs(), rel, ctx);
    return r;
}

BF quadrature(const BF& a, const BF& b, const BFC& c) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
    return math::sqrt(BF::add(BF::mul(a, a, c), BF::mul(b, b, c), c), c);
}

struct Dec {
    std::int64_t m;
    int          e;
};

struct ParticleRow {
    const char* name;
    bool        codata;
    Dec         kg, kg_unc, mev, mev_unc, dalton, dalton_unc, g, g_unc;
};

const ParticleRow particle_rows[static_cast<std::size_t>(particle::count)] = {
    {"electron", true, {91093837139, -41}, {28, -41}, {51099895069, -11}, {16, -11}, {5485799090441, -16}, {97, -16}, {-200231930436092, -14}, {36, -14}},
    {"muon", true, {1883531627, -37}, {42, -37}, {1056583755, -7}, {23, -7}, {1134289257, -10}, {25, -10}, {-200233184123, -11}, {82, -11}},
    {"tau", true, {316754, -32}, {21, -32}, {177686, -2}, {12, -2}, {190754, -5}, {13, -5}, {0, 0}, {0, 0}},
    {"proton", true, {167262192595, -38}, {52, -38}, {93827208943, -8}, {29, -8}, {10072764665789, -13}, {83, -13}, {55856946893, -10}, {16, -10}},
    {"neutron", true, {167492750056, -38}, {85, -38}, {93956542194, -8}, {48, -8}, {100866491606, -11}, {40, -11}, {-382608552, -8}, {90, -8}},
    {"deuteron", true, {33435837768, -37}, {10, -37}, {187561294500, -8}, {58, -8}, {2013553212544, -12}, {15, -12}, {8574382335, -10}, {22, -10}},
    {"triton", true, {50073567512, -37}, {16, -37}, {280892113668, -8}, {88, -8}, {301550071597, -11}, {10, -11}, {5957924930, -9}, {12, -9}},
    {"helion", true, {50064127862, -37}, {16, -37}, {280839161112, -8}, {88, -8}, {3014932246932, -12}, {74, -12}, {-42552506995, -10}, {34, -10}},
    {"alpha particle", true, {66446573450, -37}, {21, -37}, {37273794118, -7}, {12, -7}, {4001506179129, -12}, {62, -12}, {0, 0}, {0, 0}},
    {"W boson", false, {0, 0}, {0, 0}, {80362, 0}, {8, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"Z boson", false, {0, 0}, {0, 0}, {911879, -1}, {20, -1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"Higgs boson", false, {0, 0}, {0, 0}, {125130, 0}, {110, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"up quark", false, {0, 0}, {0, 0}, {216, -2}, {7, -2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"down quark", false, {0, 0}, {0, 0}, {470, -2}, {7, -2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"strange quark", false, {0, 0}, {0, 0}, {929, -1}, {7, -1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"charm quark", false, {0, 0}, {0, 0}, {1273, 0}, {5, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"bottom quark", false, {0, 0}, {0, 0}, {4186, 0}, {6, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"top quark", false, {0, 0}, {0, 0}, {172600, 0}, {270, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"charged pion", false, {0, 0}, {0, 0}, {13957039, -5}, {18, -5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"neutral pion", false, {0, 0}, {0, 0}, {1349768, -4}, {5, -4}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"charged kaon", false, {0, 0}, {0, 0}, {493677, -3}, {15, -3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"neutral kaon", false, {0, 0}, {0, 0}, {497611, -3}, {13, -3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"eta meson", false, {0, 0}, {0, 0}, {547862, -3}, {17, -3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"lambda baryon", false, {0, 0}, {0, 0}, {1115683, -3}, {6, -3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"sigma plus", false, {0, 0}, {0, 0}, {118937, -2}, {7, -2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"sigma zero", false, {0, 0}, {0, 0}, {1192642, -3}, {24, -3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"sigma minus", false, {0, 0}, {0, 0}, {1197449, -3}, {29, -3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"xi zero", false, {0, 0}, {0, 0}, {131486, -2}, {20, -2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"xi minus", false, {0, 0}, {0, 0}, {132171, -2}, {7, -2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {"omega minus", false, {0, 0}, {0, 0}, {167245, -2}, {29, -2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};

CD from(const Dec& v, const Dec& u) { return CD::measured(v.m, v.e, static_cast<std::uint64_t>(u.m), u.e); }

struct ParticleTable {
    std::vector<std::string> labels;
    std::vector<CD>          masses;
    std::vector<CD>          g_factors;
};

const ParticleTable& particle_table();

const CD& omega_lambda() {
    static const CD v = CD::measured(6847, -4, 73, -4);
    return v;
}

const CD& hubble_constant() {
    static const CD v = CD::measured(6736, -2, 54, -2);
    return v;
}

const CD& sin2_weinberg() {
    static const CD v = CD::measured(22305, -5, 23, -5);
    return v;
}

const CD& wien_x(atom_kind k) {
    static const CD wl = access::with_atom(special_atom(atom_kind::wien_wavelength));
    static const CD fr = access::with_atom(special_atom(atom_kind::wien_frequency));
    static const CD en = access::with_atom(special_atom(atom_kind::wien_entropy));
    return k == atom_kind::wien_wavelength ? wl : k == atom_kind::wien_frequency ? fr : en;
}

} // namespace pcdetail

ConstantDefinition::ConstantDefinition() : m_num(BigInt::one()), m_den(BigInt::one()) {}

ConstantDefinition ConstantDefinition::integer(std::int64_t value) {
    ConstantDefinition d;
    d.m_num = BigInt(static_cast<long long>(value));
    return d;
}

ConstantDefinition ConstantDefinition::ratio(std::int64_t num, std::int64_t den) {
    ConstantDefinition d;
    if (den == 0) return undefined();
    d.m_num = BigInt(static_cast<long long>(den < 0 ? -num : num));
    d.m_den = BigInt(static_cast<long long>(den < 0 ? -den : den));
    return d;
}

ConstantDefinition ConstantDefinition::decimal(std::int64_t mantissa, int exp10) {
    ConstantDefinition d;
    pcdetail::set_decimal(mantissa, exp10, d.m_num, d.m_den);
    return d;
}

ConstantDefinition ConstantDefinition::measured(std::int64_t mantissa, int exp10, std::uint64_t uncertainty, int uncertainty_exp10) {
    if (uncertainty == 0) return decimal(mantissa, exp10);
    pcdetail::Atom a;
    pcdetail::set_decimal(mantissa, exp10, a.num, a.den);
    pcdetail::set_decimal(static_cast<std::int64_t>(uncertainty), uncertainty_exp10, a.unc_num, a.unc_den);
    return pcdetail::access::with_atom(pcdetail::add_atom(a));
}

ConstantDefinition ConstantDefinition::pi() { return pcdetail::access::with_atom(pcdetail::pi_atom()); }

ConstantDefinition ConstantDefinition::undefined() {
    ConstantDefinition d;
    d.m_defined = false;
    return d;
}

ConstantDefinition ConstantDefinition::labeled(const char* name, const char* symbol, const char* unit) const {
    ConstantDefinition d(*this);
    d.m_name   = name ? name : "";
    d.m_symbol = symbol ? symbol : "";
    d.m_unit   = unit ? unit : "";
    return d;
}

bool ConstantDefinition::is_exact() const { return m_defined && pcdetail::access::exact(*this); }

ConstantDefinition ConstantDefinition::pow(int k) const { return pcdetail::access::power(*this, k); }

BigFloat ConstantDefinition::value(const BigFloatContext& ctx) const { return pcdetail::access::value(*this, ctx); }

BigFloat ConstantDefinition::relative_uncertainty(const BigFloatContext& ctx) const {
    const BigFloatContext wc = pcdetail::wctx(ctx.precision + 16);
    return pcdetail::access::relative_uncertainty(*this, wc).rounded(ctx);
}

PhysicalConstant<BigFloat> ConstantDefinition::evaluate(const BigFloatContext& ctx) const {
    const BigFloatContext wc = pcdetail::wctx(ctx.precision + 16);
    PhysicalConstant<BigFloat> r = pcdetail::combine(value(ctx), pcdetail::access::relative_uncertainty(*this, wc), ctx);
    r.name   = m_name;
    r.symbol = m_symbol;
    r.unit   = m_unit;
    return r;
}

ConstantDefinition operator*(const ConstantDefinition& a, const ConstantDefinition& b) { return pcdetail::access::multiply(a, b, 1); }
ConstantDefinition operator/(const ConstantDefinition& a, const ConstantDefinition& b) { return pcdetail::access::multiply(a, b, -1); }

PhysicalConstant<BigFloat> operator*(const PhysicalConstant<BigFloat>& a, const PhysicalConstant<BigFloat>& b) {
    const BigFloatContext& ctx = BigFloatContext::current();
    const BigFloatContext  wc  = pcdetail::wctx(ctx.precision + 16);
    return pcdetail::combine(BigFloat::mul(a.value, b.value, ctx), pcdetail::quadrature(a.relative_uncertainty, b.relative_uncertainty, wc), ctx);
}

PhysicalConstant<BigFloat> operator/(const PhysicalConstant<BigFloat>& a, const PhysicalConstant<BigFloat>& b) {
    const BigFloatContext& ctx = BigFloatContext::current();
    const BigFloatContext  wc  = pcdetail::wctx(ctx.precision + 16);
    return pcdetail::combine(BigFloat::div(a.value, b.value, ctx), pcdetail::quadrature(a.relative_uncertainty, b.relative_uncertainty, wc), ctx);
}

PhysicalConstant<BigFloat> operator*(const PhysicalConstant<BigFloat>& a, const BigFloat& b) {
    const BigFloatContext& ctx = BigFloatContext::current();
    return pcdetail::combine(BigFloat::mul(a.value, b, ctx), a.relative_uncertainty, ctx);
}

PhysicalConstant<BigFloat> operator*(const BigFloat& a, const PhysicalConstant<BigFloat>& b) { return b * a; }

PhysicalConstant<BigFloat> operator/(const PhysicalConstant<BigFloat>& a, const BigFloat& b) {
    const BigFloatContext& ctx = BigFloatContext::current();
    return pcdetail::combine(BigFloat::div(a.value, b, ctx), a.relative_uncertainty, ctx);
}

PhysicalConstant<BigFloat> pow(const PhysicalConstant<BigFloat>& a, int k) {
    const BigFloatContext& ctx = BigFloatContext::current();
    const BigFloatContext  wc  = pcdetail::wctx(ctx.precision + 16);
    const BigFloat         v   = pcdetail::ipow_bf(a.value, k, wc).rounded(ctx);
    const BigFloat         rel = BigFloat::mul(a.relative_uncertainty, BigFloat(static_cast<std::uint64_t>(k < 0 ? -k : k)), wc);
    return pcdetail::combine(v, rel, ctx);
}

#define FIZMO_DEFINE_CONSTANT(FN, NAME, SYMBOL, UNIT, EXPR)                \
    const ConstantDefinition& FN() {                                       \
        static const ConstantDefinition v = (EXPR).labeled(NAME, SYMBOL, UNIT); \
        return v;                                                          \
    }

using CD = ConstantDefinition;

FIZMO_DEFINE_CONSTANT(speed_of_light, "speed of light in vacuum", "c", "m s^-1", CD::decimal(299792458, 0))
FIZMO_DEFINE_CONSTANT(planck_constant, "Planck constant", "h", "J Hz^-1", CD::decimal(662607015, -42))
FIZMO_DEFINE_CONSTANT(reduced_planck_constant, "reduced Planck constant", "ħ", "J s", h() / (2 * CD::pi()))
FIZMO_DEFINE_CONSTANT(boltzmann_constant, "Boltzmann constant", "k", "J K^-1", CD::decimal(1380649, -29))
FIZMO_DEFINE_CONSTANT(elementary_charge, "elementary charge", "e", "C", CD::decimal(1602176634, -28))
FIZMO_DEFINE_CONSTANT(avogadro_constant, "Avogadro constant", "N_A", "mol^-1", CD::decimal(602214076, 15))
FIZMO_DEFINE_CONSTANT(astronomical_unit, "astronomical unit", "au", "m", CD::decimal(149597870700, 0))
FIZMO_DEFINE_CONSTANT(gravitational_constant, "Newtonian constant of gravitation", "G", "m^3 kg^-1 s^-2", CD::measured(667430, -16, 15, -16))
FIZMO_DEFINE_CONSTANT(fine_structure_constant, "fine-structure constant", "α", "", CD::measured(72973525643, -13, 11, -13))
FIZMO_DEFINE_CONSTANT(rydberg_constant, "Rydberg constant", "R_∞", "m^-1", CD::measured(10973731568157, -6, 12, -6))
FIZMO_DEFINE_CONSTANT(fermi_coupling_constant, "Fermi coupling constant", "G_F", "GeV^-2", CD::measured(11663787, -12, 6, -12))
FIZMO_DEFINE_CONSTANT(weak_mixing_angle, "weak mixing angle", "sin²θ_W", "", pcdetail::sin2_weinberg())
FIZMO_DEFINE_CONSTANT(weinberg_angle, "Weinberg angle", "θ_W", "rad", pcdetail::access::make_weinberg(pcdetail::sin2_weinberg()))
FIZMO_DEFINE_CONSTANT(atomic_mass_constant, "atomic mass constant", "m_u", "kg", CD::measured(166053906892, -38, 52, -38))
FIZMO_DEFINE_CONSTANT(electron_mass, "electron mass", "m_e", "kg", mass(particle::electron, mass_unit::kg))
FIZMO_DEFINE_CONSTANT(proton_mass, "proton mass", "m_p", "kg", mass(particle::proton, mass_unit::kg))
FIZMO_DEFINE_CONSTANT(neutron_mass, "neutron mass", "m_n", "kg", mass(particle::neutron, mass_unit::kg))
FIZMO_DEFINE_CONSTANT(parsec, "parsec", "pc", "m", astronomical_unit() * 648000 / CD::pi())
FIZMO_DEFINE_CONSTANT(cosmological_constant, "cosmological constant", "Λ", "m^-2",
                      3 * pcdetail::omega_lambda() * (pcdetail::hubble_constant() * 1000 / (CD::integer(1000000) * parsec())).squared() / c().squared())
FIZMO_DEFINE_CONSTANT(first_radiation_constant, "first radiation constant", "c_1", "W m^2", 2 * CD::pi() * h() * c().squared())
FIZMO_DEFINE_CONSTANT(first_radiation_constant_spectral, "first radiation constant for spectral radiance", "c_1L", "W m^2 sr^-1", 2 * h() * c().squared())
FIZMO_DEFINE_CONSTANT(second_radiation_constant, "second radiation constant", "c_2", "m K", h() * c() / k_B())
FIZMO_DEFINE_CONSTANT(wien_wavelength_constant, "Wien wavelength displacement law constant", "b", "m K", second_radiation_constant() / pcdetail::wien_x(pcdetail::atom_kind::wien_wavelength))
FIZMO_DEFINE_CONSTANT(wien_frequency_constant, "Wien frequency displacement law constant", "b'", "Hz K^-1", pcdetail::wien_x(pcdetail::atom_kind::wien_frequency) * k_B() / h())
FIZMO_DEFINE_CONSTANT(wien_entropy_constant, "Wien entropy displacement law constant", "b_entropy", "m K", second_radiation_constant() / pcdetail::wien_x(pcdetail::atom_kind::wien_entropy))
FIZMO_DEFINE_CONSTANT(stefan_boltzmann_constant, "Stefan-Boltzmann constant", "σ", "W m^-2 K^-4", 2 * CD::pi().pow(5) * k_B().pow(4) / (15 * h().pow(3) * c().squared()))
FIZMO_DEFINE_CONSTANT(conductance_quantum, "conductance quantum", "G_0", "S", 2 * elementary_charge().squared() / h())
FIZMO_DEFINE_CONSTANT(von_klitzing_constant, "von Klitzing constant", "R_K", "ohm", h() / elementary_charge().squared())
FIZMO_DEFINE_CONSTANT(josephson_constant, "Josephson constant", "K_J", "Hz V^-1", 2 * elementary_charge() / h())
FIZMO_DEFINE_CONSTANT(magnetic_flux_quantum, "magnetic flux quantum", "Φ_0", "Wb", h() / (2 * elementary_charge()))
FIZMO_DEFINE_CONSTANT(vacuum_permeability, "vacuum magnetic permeability", "μ_0", "N A^-2", 2 * alpha() * h() / (elementary_charge().squared() * c()))
FIZMO_DEFINE_CONSTANT(vacuum_impedance, "characteristic impedance of vacuum", "Z_0", "ohm", mu_0() * c())
FIZMO_DEFINE_CONSTANT(vacuum_permittivity, "vacuum electric permittivity", "ε_0", "F m^-1", 1 / (mu_0() * c().squared()))
FIZMO_DEFINE_CONSTANT(coulomb_constant, "Coulomb constant", "k_e", "N m^2 C^-2", 1 / (4 * CD::pi() * epsilon_0()))
FIZMO_DEFINE_CONSTANT(quantum_of_circulation, "quantum of circulation", "κ", "m^2 s^-1", h() / (2 * m_e()))
FIZMO_DEFINE_CONSTANT(nuclear_magneton, "nuclear magneton", "μ_N", "J T^-1", elementary_charge() * hbar() / (2 * m_p()))
FIZMO_DEFINE_CONSTANT(bohr_magneton, "Bohr magneton", "μ_B", "J T^-1", elementary_charge() * hbar() / (2 * m_e()))
FIZMO_DEFINE_CONSTANT(bohr_radius, "Bohr radius", "a_0", "m", alpha() / (4 * CD::pi() * rydberg_constant()))
FIZMO_DEFINE_CONSTANT(rydberg_energy, "Rydberg unit of energy", "R_y", "J", rydberg_constant() * h() * c())
FIZMO_DEFINE_CONSTANT(hartree_energy, "Hartree energy", "E_h", "J", 2 * rydberg_energy())
FIZMO_DEFINE_CONSTANT(molar_gas_constant, "molar gas constant", "R", "J mol^-1 K^-1", N_A() * k_B())
FIZMO_DEFINE_CONSTANT(faraday_constant, "Faraday constant", "F", "C mol^-1", N_A() * elementary_charge())
FIZMO_DEFINE_CONSTANT(molar_planck_constant, "molar Planck constant", "N_A h", "J Hz^-1 mol^-1", N_A() * h())

#undef FIZMO_DEFINE_CONSTANT

namespace pcdetail {

const ParticleTable& particle_table() {
    static const ParticleTable table = [] {
        ParticleTable t;
        const std::size_t n = static_cast<std::size_t>(particle::count);
        t.labels.reserve(n * 4);
        t.masses.reserve(n * 3);
        t.g_factors.reserve(n);
        const CD mev_to_kg = CD::integer(1000000) * elementary_charge() / c().squared();
        for (std::size_t i = 0; i < n; ++i) {
            const ParticleRow& r = particle_rows[i];
            t.labels.push_back(std::string(r.name) + " mass");
            t.labels.push_back(std::string(r.name) + " g-factor");
            const char* mass_name = t.labels[t.labels.size() - 2].c_str();
            const char* g_name    = t.labels.back().c_str();
            CD kg, mev, da;
            if (r.codata) {
                kg  = from(r.kg, r.kg_unc);
                mev = from(r.mev, r.mev_unc);
                da  = from(r.dalton, r.dalton_unc);
            } else {
                mev = from(r.mev, r.mev_unc);
                kg  = mev * mev_to_kg;
                da  = kg / atomic_mass_constant();
            }
            t.masses.push_back(kg.labeled(mass_name, "", "kg"));
            t.masses.push_back(mev.labeled(mass_name, "", "MeV/c^2"));
            t.masses.push_back(da.labeled(mass_name, "", "u"));
            t.g_factors.push_back(r.g.m != 0 ? from(r.g, r.g_unc).labeled(g_name, "g", "") : CD::undefined());
        }
        return t;
    }();
    return table;
}

} // namespace pcdetail

const char* name(particle p) noexcept {
    if (p >= particle::count) return "";
    return pcdetail::particle_rows[static_cast<std::size_t>(p)].name;
}

const char* unit(mass_unit u) noexcept {
    switch (u) {
        case mass_unit::kg:  return "kg";
        case mass_unit::MeV: return "MeV/c^2";
        default:             return "u";
    }
}

bool has_g_factor(particle p) noexcept {
    if (p >= particle::count) return false;
    return pcdetail::particle_rows[static_cast<std::size_t>(p)].g.m != 0;
}

const ConstantDefinition& mass(particle p, mass_unit u) {
    static const ConstantDefinition none = ConstantDefinition::undefined();
    if (p >= particle::count || static_cast<int>(u) > 2) return none;
    return pcdetail::particle_table().masses[static_cast<std::size_t>(p) * 3 + static_cast<std::size_t>(u)];
}

const ConstantDefinition& g_factor(particle p) {
    static const ConstantDefinition none = ConstantDefinition::undefined();
    if (p >= particle::count) return none;
    return pcdetail::particle_table().g_factors[static_cast<std::size_t>(p)];
}

ParticleProperties<BigFloat> properties(particle p, const BigFloatContext& ctx) {
    ParticleProperties<BigFloat> r;
    r.id           = p;
    r.name         = name(p);
    r.mass_kg      = mass(p, mass_unit::kg).evaluate(ctx);
    r.mass_mev     = mass(p, mass_unit::MeV).evaluate(ctx);
    r.mass_dalton  = mass(p, mass_unit::dalton).evaluate(ctx);
    r.has_g_factor = has_g_factor(p);
    r.g_factor     = g_factor(p).evaluate(ctx);
    return r;
}

} // namespace constants
} // namespace multiprecision
} // namespace fizmo
