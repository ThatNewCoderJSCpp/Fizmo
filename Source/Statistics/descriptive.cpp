#include "fizmo_library.hpp"
#include "internal.hpp"

#include <algorithm>

namespace fizmo {
namespace statistics {
namespace core {
namespace desc {

namespace T = ::fizmo::math::tracked;

using BF  = BigFloat;
using BFC = BigFloatContext;
using RV  = T::RV;

BFC exact_ctx() { return BFC(BFC::max_prec, math::RoundingMode::nearest_even); }

BF eadd(const BF& a, const BF& b) { return BF::add(a, b, exact_ctx()); }
BF esub(const BF& a, const BF& b) { return BF::sub(a, b, exact_ctx()); }
BF emul(const BF& a, const BF& b) { return BF::mul(a, b, exact_ctx()); }

bool has_bad(const std::vector<BF>& d) {
    for (const BF& v : d) {
        if (!v.is_finite()) return true;
    }
    return false;
}

struct Sums {
    BF n  = BF::zero();
    BF s1 = BF::zero();
    BF s2 = BF::zero();
    BF s3 = BF::zero();
    BF s4 = BF::zero();
};

Sums sums(const std::vector<BF>& d, int order) {
    Sums s;
    s.n = BF(static_cast<std::uint64_t>(d.size()));

    for (const BF& v : d) {
        s.s1 = eadd(s.s1, v);
        if (order < 2) continue;
        const BF v2 = emul(v, v);
        s.s2 = eadd(s.s2, v2);
        if (order < 3) continue;
        const BF v3 = emul(v2, v);
        s.s3 = eadd(s.s3, v3);
        if (order < 4) continue;
        s.s4 = eadd(s.s4, emul(v3, v));
    }

    return s;
}

BF ratio(const BF& num, const BF& den, const BFC& c) {
    if (den.is_zero()) return num.is_zero() ? BF::nan() : BF::infinity(num.signbit());
    return BF::div(num, den, c);
}

BF sqrt_ratio(const BF& num, const BF& den, const BFC& c) {
    if (den.is_zero() || num.signbit() || den.signbit()) return num.is_zero() && !den.is_zero() ? BF::zero() : BF::nan();
    if (num.is_zero()) return BF::zero();
    return T::drive(c, [&](std::size_t w) { return T::sqrt(T::div(T::exact(num), T::exact(den), w + 8), w); });
}

BF scaled_square(const Sums& s) { return esub(emul(s.n, s.s2), emul(s.s1, s.s1)); }

BF cross(const std::vector<BF>& x, const std::vector<BF>& y) {
    BF acc = BF::zero();
    for (std::size_t i = 0; i < x.size(); ++i) acc = eadd(acc, emul(x[i], y[i]));
    return acc;
}

void sort_values(std::vector<BF>& d) {
    std::sort(d.begin(), d.end(), [](const BF& a, const BF& b) { return BF::compare(a, b) == BF::ordering::less; });
}

} // namespace desc

BigFloat sum(const std::vector<BigFloat>& data, const BigFloatContext& ctx) {
    BigFloat acc = BigFloat::zero();
    for (const BigFloat& v : data) {
        if (v.is_unordered()) return BigFloat::nan();
        acc = desc::eadd(acc, v);
    }
    return acc.rounded(ctx);
}

BigFloat mean(const std::vector<BigFloat>& data, const BigFloatContext& ctx) {
    if (data.empty() || desc::has_bad(data)) return BigFloat::nan();
    const desc::Sums s = desc::sums(data, 1);
    return desc::ratio(s.s1, s.n, ctx);
}

BigFloat variance(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx) {
    const std::size_t n = data.size();
    if (n == 0 || (sample && n < 2) || desc::has_bad(data)) return BigFloat::nan();
    const desc::Sums s   = desc::sums(data, 2);
    const BigFloat   den = sample ? desc::emul(s.n, BigFloat(static_cast<std::uint64_t>(n - 1))) : desc::emul(s.n, s.n);
    return desc::ratio(desc::scaled_square(s), den, ctx);
}

BigFloat standard_deviation(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx) {
    const std::size_t n = data.size();
    if (n == 0 || (sample && n < 2) || desc::has_bad(data)) return BigFloat::nan();
    const desc::Sums s   = desc::sums(data, 2);
    const BigFloat   den = sample ? desc::emul(s.n, BigFloat(static_cast<std::uint64_t>(n - 1))) : desc::emul(s.n, s.n);
    return desc::sqrt_ratio(desc::scaled_square(s), den, ctx);
}

BigFloat standard_error(const std::vector<BigFloat>& data, const BigFloatContext& ctx) {
    const std::size_t n = data.size();
    if (n < 2 || desc::has_bad(data)) return BigFloat::nan();
    const desc::Sums s   = desc::sums(data, 2);
    const BigFloat   den = desc::emul(desc::emul(s.n, s.n), BigFloat(static_cast<std::uint64_t>(n - 1)));
    return desc::sqrt_ratio(desc::scaled_square(s), den, ctx);
}

BigFloat quantile(std::vector<BigFloat> data, const BigFloat& p, const BigFloatContext& ctx) {
    if (data.empty() || desc::has_bad(data) || !p.is_finite() || p.signbit() || BigFloat::compare(p, BigFloat::one()) == BigFloat::ordering::greater) {
        return BigFloat::nan();
    }
    desc::sort_values(data);
    const BigFloat      h    = desc::emul(BigFloat(static_cast<std::uint64_t>(data.size() - 1)), p);
    const BigFloat      fl   = math::floor(h);
    const std::uint64_t lo   = static_cast<std::uint64_t>(fl.to_double());
    const BigFloat      frac = desc::esub(h, fl);
    if (frac.is_zero() || lo + 1 >= data.size()) return data[lo].rounded(ctx);
    return desc::eadd(data[lo], desc::emul(frac, desc::esub(data[lo + 1], data[lo]))).rounded(ctx);
}

BigFloat median(std::vector<BigFloat> data, const BigFloatContext& ctx) {
    return quantile(std::move(data), BigFloat::one().scaled_pow2(-1), ctx);
}

std::vector<BigFloat> modes(std::vector<BigFloat> data) {
    std::vector<BigFloat> out;
    if (data.empty() || desc::has_bad(data)) return out;
    desc::sort_values(data);
    std::size_t best = 0;
    std::size_t i    = 0;

    while (i < data.size()) {
        std::size_t j = i + 1;
        while (j < data.size() && BigFloat::compare(data[j], data[i]) == BigFloat::ordering::equal) ++j;
        const std::size_t run = j - i;
        if (run > best) {
            best = run;
            out.clear();
        }
        if (run == best) out.push_back(data[i]);
        i = j;
    }

    return out;
}

BigFloat skewness(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx) {
    const std::size_t n = data.size();
    if (n < 3 || desc::has_bad(data)) return BigFloat::nan();
    using namespace desc;
    const Sums s   = sums(data, 3);
    const BF   M2n = scaled_square(s);
    if (M2n.is_zero()) return BF::nan();
    const BF   M3n = eadd(esub(emul(emul(s.n, s.n), s.s3), emul(emul(BF(3), s.n), emul(s.s1, s.s2))), emul(BF(2), emul(s.s1, emul(s.s1, s.s1))));
    if (M3n.is_zero()) return BF::zero();
    const BF nm1 = BF(static_cast<std::uint64_t>(n - 1));
    const BF nm2 = BF(static_cast<std::uint64_t>(n - 2));

    return T::drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const RV          g1 = T::div(T::exact(M3n), T::mul(T::exact(M2n), T::sqrt(T::exact(M2n), wi), wi), wi);
        if (!sample) return g1;
        return T::mul(T::div(g1, T::exact(nm2), wi), T::sqrt(T::exact(emul(s.n, nm1)), wi), w);
    });
}

BigFloat kurtosis_excess(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx) {
    const std::size_t n = data.size();
    if (n < 4 || desc::has_bad(data)) return BigFloat::nan();
    using namespace desc;
    const Sums s   = sums(data, 4);
    const BF   M2n = scaled_square(s);
    if (M2n.is_zero()) return BF::nan();
    const BF n2  = emul(s.n, s.n);
    const BF s12 = emul(s.s1, s.s1);
    const BF M4n = esub(eadd(esub(emul(emul(n2, s.n), s.s4), emul(emul(BF(4), n2), emul(s.s1, s.s3))), emul(emul(BF(6), s.n), emul(s12, s.s2))),
                        emul(BF(3), emul(s12, s12)));
    const BF M22 = emul(M2n, M2n);
    const BF g2n = esub(M4n, emul(BF(3), M22));
    if (!sample) return ratio(g2n, M22, ctx);
    const BF np1 = BF(static_cast<std::uint64_t>(n + 1));
    const BF nm1 = BF(static_cast<std::uint64_t>(n - 1));
    const BF nm2 = BF(static_cast<std::uint64_t>(n - 2));
    const BF nm3 = BF(static_cast<std::uint64_t>(n - 3));
    const BF num = emul(eadd(emul(np1, g2n), emul(BF(6), M22)), nm1);
    const BF den = emul(emul(nm2, nm3), M22);
    return ratio(num, den, ctx);
}

BigFloat covariance(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, bool sample, const BigFloatContext& ctx) {
    const std::size_t n = x.size();
    if (n == 0 || y.size() != n || (sample && n < 2) || desc::has_bad(x) || desc::has_bad(y)) return BigFloat::nan();
    using namespace desc;
    const Sums sx  = sums(x, 1);
    const Sums sy  = sums(y, 1);
    const BF   sxy = esub(emul(sx.n, cross(x, y)), emul(sx.s1, sy.s1));
    const BF   den = sample ? emul(sx.n, BF(static_cast<std::uint64_t>(n - 1))) : emul(sx.n, sx.n);
    return ratio(sxy, den, ctx);
}

BigFloat correlation(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloatContext& ctx) {
    const std::size_t n = x.size();
    if (n < 2 || y.size() != n || desc::has_bad(x) || desc::has_bad(y)) return BigFloat::nan();
    using namespace desc;
    const Sums sx  = sums(x, 2);
    const Sums sy  = sums(y, 2);
    const BF   SXY = esub(emul(sx.n, cross(x, y)), emul(sx.s1, sy.s1));
    const BF   SXX = scaled_square(sx);
    const BF   SYY = scaled_square(sy);
    if (SXX.is_zero() || SYY.is_zero()) return BF::nan();
    if (SXY.is_zero()) return BF::zero();
    return T::drive(ctx, [&](std::size_t w) {
        return T::div(T::exact(SXY), T::sqrt(T::exact(emul(SXX, SYY)), w + 8), w);
    });
}

BigFloat z_score(const BigFloat& x, const BigFloat& mean, const BigFloat& sd, const BigFloatContext& ctx) {
    return desc::ratio(desc::esub(x, mean), sd, ctx);
}

Regression regression(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloatContext& ctx) {
    Regression r;
    r.slope = r.intercept = r.r = r.r_squared = r.residual_standard_error = r.slope_standard_error = r.intercept_standard_error = BigFloat::nan();
    const std::size_t n = x.size();
    r.n = n;
    if (n < 2 || y.size() != n || desc::has_bad(x) || desc::has_bad(y)) return r;
    using namespace desc;
    const Sums sx  = sums(x, 2);
    const Sums sy  = sums(y, 2);
    const BF   SXY = esub(emul(sx.n, cross(x, y)), emul(sx.s1, sy.s1));
    const BF   SXX = scaled_square(sx);
    const BF   SYY = scaled_square(sy);
    if (SXX.is_zero()) return r;
    r.slope     = ratio(SXY, SXX, ctx);
    r.intercept = ratio(esub(emul(sy.s1, SXX), emul(SXY, sx.s1)), emul(sx.n, SXX), ctx);

    if (!SYY.is_zero()) {
        r.r_squared = ratio(emul(SXY, SXY), emul(SXX, SYY), ctx);
        r.r         = SXY.is_zero() ? BF::zero() : T::drive(ctx, [&](std::size_t w) {
            return T::div(T::exact(SXY), T::sqrt(T::exact(emul(SXX, SYY)), w + 8), w);
        });
    }

    if (n > 2) {
        const BF nm2 = BF(static_cast<std::uint64_t>(n - 2));
        const BF sse = esub(emul(SYY, SXX), emul(SXY, SXY));
        r.residual_standard_error  = sqrt_ratio(sse, emul(emul(sx.n, nm2), SXX), ctx);
        r.slope_standard_error     = sqrt_ratio(sse, emul(nm2, emul(SXX, SXX)), ctx);
        r.intercept_standard_error = sqrt_ratio(emul(sse, sx.s2), emul(emul(sx.n, nm2), emul(SXX, SXX)), ctx);
    }

    return r;
}

Sample summary(const BigFloat& mean, const BigFloat& sd, std::uint64_t n) {
    Sample s;
    s.n         = n;
    s.from_data = false;
    s.mean      = mean;
    s.sd        = sd;
    return s;
}

Sample summary(const std::vector<BigFloat>& data) {
    Sample s;
    s.n         = data.size();
    s.from_data = true;
    const desc::Sums t = desc::sums(data, 2);
    s.sum         = t.s1;
    s.sum_squares = t.s2;
    if (desc::has_bad(data)) {
        s.sum = s.sum_squares = BigFloat::nan();
    }
    s.mean = data.empty() ? BigFloat::nan() : desc::ratio(t.s1, t.n, BigFloatContext(64, math::RoundingMode::nearest_even));
    s.sd   = data.size() < 2 ? BigFloat::nan()
                             : desc::sqrt_ratio(desc::scaled_square(t), desc::emul(t.n, BigFloat(static_cast<std::uint64_t>(data.size() - 1))),
                                                BigFloatContext(64, math::RoundingMode::nearest_even));
    return s;
}

Sample differences(const std::vector<BigFloat>& a, const std::vector<BigFloat>& b) {
    std::vector<BigFloat> d;
    if (a.size() != b.size()) return summary(d);
    d.reserve(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) d.push_back(desc::esub(a[i], b[i]));
    return summary(d);
}

} // namespace core
} // namespace statistics
} // namespace fizmo
