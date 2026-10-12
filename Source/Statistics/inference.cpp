#include "fizmo_library.hpp"
#include "internal.hpp"

namespace fizmo {
namespace statistics {
namespace core {
namespace idetail {

namespace T = ::fizmo::math::tracked;

using BF  = BigFloat;
using BFC = BigFloatContext;
using RV  = T::RV;

BFC exact_ctx() { return BFC(BFC::max_prec, math::RoundingMode::nearest_even); }
BF  eadd(const BF& a, const BF& b) { return BF::add(a, b, exact_ctx()); }
BF  esub(const BF& a, const BF& b) { return BF::sub(a, b, exact_ctx()); }
BF  emul(const BF& a, const BF& b) { return BF::mul(a, b, exact_ctx()); }
BF  N(std::uint64_t v) { return BF(v); }
RV  X(const BF& v) { return T::exact(v); }

int cmp(const BF& a, const BF& b) {
    const BF::ordering o = BF::compare(a, b);
    if (o == BF::ordering::less) return -1;
    if (o == BF::ordering::greater) return 1;
    if (o == BF::ordering::equal) return 0;
    return 2;
}

bool positive(const BF& v) { return v.is_finite() && !v.signbit() && !v.is_zero(); }
bool probability_open(const BF& v) { return positive(v) && cmp(v, BF::one()) < 0; }

BF scaled_square(const Sample& s) { return esub(emul(N(s.n), s.sum_squares), emul(s.sum, s.sum)); }

bool usable(const Sample& s, bool need_sd) {
    if (s.n == 0) return false;
    if (s.from_data) return s.sum.is_finite() && (!need_sd || (s.n >= 2 && positive(scaled_square(s))));
    return s.mean.is_finite() && (!need_sd || positive(s.sd));
}

RV mean_rv(const Sample& s, std::size_t w) {
    if (s.from_data) return T::div(X(s.sum), X(N(s.n)), w);
    return X(s.mean);
}

RV variance_rv(const Sample& s, std::size_t w) {
    if (s.from_data) return T::div(X(scaled_square(s)), X(emul(N(s.n), N(s.n - 1))), w);
    return T::sqr(X(s.sd), w);
}

RV variance_over_n(const Sample& s, std::size_t w) {
    if (s.from_data) return T::div(X(scaled_square(s)), X(emul(emul(N(s.n), N(s.n)), N(s.n - 1))), w);
    return T::div(T::sqr(X(s.sd), w), X(N(s.n)), w);
}

RV known_over_n(const BF& sigma, std::uint64_t n, std::size_t w) { return T::div(T::sqr(X(sigma), w), X(N(n)), w); }

RV p_from_z(const RV& z, Tail tail, std::size_t w) {
    switch (tail) {
        case Tail::lower: return ddetail::phi(z, false, w);
        case Tail::upper: return ddetail::phi(z, true, w);
        default: {
            const RV p = T::scale2(ddetail::phi(T::abs(z), true, w), 1);
            return p;
        }
    }
}

RV p_from_t(const BF& df, const RV& t, Tail tail, std::size_t w) {
    switch (tail) {
        case Tail::lower: return ddetail::student(df, t, false, w);
        case Tail::upper: return ddetail::student(df, t, true, w);
        default:          return T::scale2(ddetail::student(df, T::abs(t), true, w), 1);
    }
}

template <class Fn>
BF drive(const BFC& ctx, Fn&& fn) { return T::drive(ctx, std::forward<Fn>(fn)); }

Dist standard_normal() { return Dist(Kind::normal, BF::zero(), BF::one()); }

RV z_crit(const BF& confidence, std::size_t w) {
    const BF tail = esub(BF::one(), confidence).scaled_pow2(-1);
    return ddetail::continuous_quantile(standard_normal(), tail, true, w);
}

RV t_crit(const BF& confidence, const BF& df, std::size_t w) {
    const BF tail = esub(BF::one(), confidence).scaled_pow2(-1);
    return ddetail::continuous_quantile(Dist(Kind::student_t, df), tail, true, w);
}

BF high_precision_df(const Sample& a, const Sample& b, bool pooled, const BFC& ctx) {
    if (pooled) return N(a.n + b.n - 2);
    const std::size_t w  = ctx.precision + 160;
    const RV          v1 = variance_over_n(a, w);
    const RV          v2 = variance_over_n(b, w);
    const RV          s  = T::add(v1, v2, w);
    const RV          d1 = T::div(T::sqr(v1, w), X(N(a.n - 1)), w);
    const RV          d2 = T::div(T::sqr(v2, w), X(N(b.n - 1)), w);
    return T::div(T::sqr(s, w), T::add(d1, d2, w), w).v.rounded(BFC(ctx.precision + 128, math::RoundingMode::nearest_even));
}

RV pooled_variance(const Sample& a, const Sample& b, std::size_t w) {
    const RV num = T::add(T::mul(variance_rv(a, w), X(N(a.n - 1)), w), T::mul(variance_rv(b, w), X(N(b.n - 1)), w), w);
    return T::div(num, X(N(a.n + b.n - 2)), w);
}

RV two_sample_se(const Sample& a, const Sample& b, bool pooled, std::size_t w) {
    if (pooled) {
        const RV inv = T::div(X(N(a.n + b.n)), X(emul(N(a.n), N(b.n))), w);
        return T::sqrt(T::mul(pooled_variance(a, b, w), inv, w), w);
    }
    return T::sqrt(T::add(variance_over_n(a, w), variance_over_n(b, w), w), w);
}

BF value_or_nan(bool ok, const BF& v) { return ok ? v : BF::nan(); }

void add_population(std::vector<Condition>& out, std::uint64_t n, std::uint64_t population, const std::string& label) {
    if (population > 0) out.push_back(ten_percent_condition(n, population, label));
}

void add_normality(std::vector<Condition>& out, std::uint64_t n, const std::string& label) {
    out.push_back(large_sample_condition(n, label));
}

} // namespace idetail

using namespace idetail;

Condition ten_percent_condition(std::uint64_t n, std::uint64_t population, const std::string& label) {
    Condition c;
    c.name = "10% condition (" + label + ")";
    if (population == 0) {
        c.met    = true;
        c.detail = "population size not given";
        return c;
    }
    c.met    = static_cast<long double>(n) * 10.0L <= static_cast<long double>(population);
    c.detail = "n = " + std::to_string(n) + (c.met ? " <= " : " > ") + "10% of N = " + std::to_string(population);
    return c;
}

Condition large_counts_condition(std::uint64_t n, const BigFloat& p, const std::string& label, std::uint64_t threshold) {
    Condition c;
    c.name = "Large counts (" + label + ")";
    const BF np  = emul(N(n), p);
    const BF nq  = emul(N(n), esub(BF::one(), p));
    const BF thr = N(threshold);
    c.met    = cmp(np, thr) >= 0 && cmp(nq, thr) >= 0;
    c.detail = "n*p = " + np.to_string(8) + ", n*(1-p) = " + nq.to_string(8) + (c.met ? " both >= " : ", need both >= ") + std::to_string(threshold);
    return c;
}

Condition large_sample_condition(std::uint64_t n, const std::string& label, std::uint64_t threshold) {
    Condition c;
    c.name   = "Normal/large sample (" + label + ")";
    c.met    = n >= threshold;
    c.detail = c.met ? "n = " + std::to_string(n) + " >= " + std::to_string(threshold)
                     : "n = " + std::to_string(n) + " < " + std::to_string(threshold) + ": the population should be approximately normal";
    return c;
}

Condition expected_counts_condition(const std::vector<BigFloat>& expected, const BigFloat& threshold) {
    Condition c;
    c.name = "Large expected counts";
    std::size_t low = 0;
    for (const BF& e : expected) {
        if (cmp(e, threshold) < 0) ++low;
    }
    c.met    = low == 0;
    c.detail = c.met ? "all expected counts >= " + threshold.to_string(8)
                     : std::to_string(low) + " expected count(s) below " + threshold.to_string(8);
    return c;
}

BigFloat critical_z(const BigFloat& alpha, Tail tail, const BigFloatContext& ctx) {
    if (!probability_open(alpha)) return BF::nan();
    switch (tail) {
        case Tail::lower: return quantile(standard_normal(), alpha, false, ctx);
        case Tail::upper: return quantile(standard_normal(), alpha, true, ctx);
        default:          return quantile(standard_normal(), alpha.scaled_pow2(-1), true, ctx);
    }
}

BigFloat critical_t(const BigFloat& alpha, const BigFloat& df, Tail tail, const BigFloatContext& ctx) {
    if (!probability_open(alpha) || !positive(df)) return BF::nan();
    const Dist d(Kind::student_t, df);
    switch (tail) {
        case Tail::lower: return quantile(d, alpha, false, ctx);
        case Tail::upper: return quantile(d, alpha, true, ctx);
        default:          return quantile(d, alpha.scaled_pow2(-1), true, ctx);
    }
}

BigFloat critical_chi_squared(const BigFloat& alpha, const BigFloat& df, const BigFloatContext& ctx) {
    if (!probability_open(alpha) || !positive(df)) return BF::nan();
    return quantile(Dist(Kind::chi_squared, df), alpha, true, ctx);
}

BigFloat critical_f(const BigFloat& alpha, const BigFloat& d1, const BigFloat& d2, const BigFloatContext& ctx) {
    if (!probability_open(alpha) || !positive(d1) || !positive(d2)) return BF::nan();
    return quantile(Dist(Kind::fisher_f, d1, d2), alpha, true, ctx);
}

BigFloat z_star(const BigFloat& confidence, const BigFloatContext& ctx) {
    if (!probability_open(confidence)) return BF::nan();
    return quantile(standard_normal(), esub(BF::one(), confidence).scaled_pow2(-1), true, ctx);
}

BigFloat t_star(const BigFloat& confidence, const BigFloat& df, const BigFloatContext& ctx) {
    if (!probability_open(confidence) || !positive(df)) return BF::nan();
    return quantile(Dist(Kind::student_t, df), esub(BF::one(), confidence).scaled_pow2(-1), true, ctx);
}

BigFloat welch_degrees_of_freedom(const Sample& a, const Sample& b, const BigFloatContext& ctx) {
    if (!usable(a, true) || !usable(b, true)) return BF::nan();
    return drive(ctx, [&](std::size_t w) {
        const RV v1 = variance_over_n(a, w + 16);
        const RV v2 = variance_over_n(b, w + 16);
        const RV d1 = T::div(T::sqr(v1, w + 16), X(N(a.n - 1)), w + 16);
        const RV d2 = T::div(T::sqr(v2, w + 16), X(N(b.n - 1)), w + 16);
        return T::div(T::sqr(T::add(v1, v2, w + 16), w + 16), T::add(d1, d2, w + 16), w);
    });
}

BigFloat t_statistic(const Sample& s, const BigFloat& mu0, const BigFloatContext& ctx) {
    if (!usable(s, true)) return BF::nan();
    return drive(ctx, [&](std::size_t w) {
        return T::div(T::sub(mean_rv(s, w + 8), X(mu0), w + 8), T::sqrt(variance_over_n(s, w + 8), w + 8), w);
    });
}

BigFloat z_statistic(const BigFloat& estimate, const BigFloat& null_value, const BigFloat& standard_error, const BigFloatContext& ctx) {
    if (!positive(standard_error)) return BF::nan();
    return BF::div(esub(estimate, null_value), standard_error, ctx);
}

namespace idetail {

template <class Stat>
void finish_z(Test& t, const BFC& ctx, Stat&& stat) {
    t.statistic = drive(ctx, [&](std::size_t w) { return stat(w); });
    t.p_value   = drive(ctx, [&](std::size_t w) { return p_from_z(stat(w + 8), t.tail, w); });
}

template <class Stat>
void finish_t(Test& t, const BFC& ctx, const BF& df, Stat&& stat) {
    t.statistic = drive(ctx, [&](std::size_t w) { return stat(w); });
    t.p_value   = drive(ctx, [&](std::size_t w) { return p_from_t(df, stat(w + 8), t.tail, w); });
}

template <class Est, class Se, class Crit>
void finish_interval(Interval& iv, const BFC& ctx, Est&& est, Se&& se, Crit&& crit) {
    iv.estimate        = drive(ctx, [&](std::size_t w) { return est(w); });
    iv.standard_error  = drive(ctx, [&](std::size_t w) { return se(w); });
    iv.critical_value  = drive(ctx, [&](std::size_t w) { return crit(w); });
    iv.margin_of_error = drive(ctx, [&](std::size_t w) { return T::mul(crit(w + 8), se(w + 8), w); });
    iv.lower           = drive(ctx, [&](std::size_t w) { return T::sub(est(w + 8), T::mul(crit(w + 8), se(w + 8), w + 8), w); });
    iv.upper           = drive(ctx, [&](std::size_t w) { return T::add(est(w + 8), T::mul(crit(w + 8), se(w + 8), w + 8), w); });
}

} // namespace idetail

Test one_proportion_z_test(std::uint64_t successes, std::uint64_t n, const BigFloat& p0, Tail tail, std::uint64_t population, const BigFloatContext& ctx) {
    Test t;
    t.name       = "One-proportion z-test";
    t.tail       = tail;
    t.null_value = p0.rounded(ctx);
    if (n == 0 || successes > n || !probability_open(p0)) return t;
    t.conditions.push_back(large_counts_condition(n, p0, "null proportion"));
    add_population(t.conditions, n, population, "sample");
    const BF x  = N(successes);
    const BF nn = N(n);
    const BF q0 = esub(BF::one(), p0);
    t.estimate       = BF::div(x, nn, ctx);
    t.standard_error = drive(ctx, [&](std::size_t w) { return T::sqrt(T::div(X(emul(p0, q0)), X(nn), w + 8), w); });
    const BF num = esub(x, emul(nn, p0));
    const BF rad = emul(emul(nn, p0), q0);
    finish_z(t, ctx, [&](std::size_t w) { return T::div(X(num), T::sqrt(X(rad), w + 8), w); });
    return t;
}

Test two_proportion_z_test(std::uint64_t x1, std::uint64_t n1, std::uint64_t x2, std::uint64_t n2, Tail tail,
                           std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx) {
    Test t;
    t.name       = "Two-proportion z-test";
    t.tail       = tail;
    t.null_value = BF::zero();
    if (n1 == 0 || n2 == 0 || x1 > n1 || x2 > n2) return t;
    const std::uint64_t xs = x1 + x2;
    const std::uint64_t ns = n1 + n2;
    const BF pooled = BF::div(N(xs), N(ns), BFC(256, math::RoundingMode::nearest_even));
    t.conditions.push_back(large_counts_condition(n1, pooled, "sample 1, pooled proportion"));
    t.conditions.push_back(large_counts_condition(n2, pooled, "sample 2, pooled proportion"));
    add_population(t.conditions, n1, population1, "sample 1");
    add_population(t.conditions, n2, population2, "sample 2");
    const BF diff_num = esub(emul(N(x1), N(n2)), emul(N(x2), N(n1)));
    const BF n12      = emul(N(n1), N(n2));
    t.estimate = BF::div(diff_num, n12, ctx);
    if (xs == 0 || xs == ns) return t;
    const BF se_num = emul(N(xs), N(ns - xs));
    const BF se_den = emul(N(ns), n12);
    t.standard_error = drive(ctx, [&](std::size_t w) { return T::sqrt(T::div(X(se_num), X(se_den), w + 8), w); });
    const BF rad_den = emul(n12, se_num);
    finish_z(t, ctx, [&](std::size_t w) { return T::mul(X(diff_num), T::sqrt(T::div(X(N(ns)), X(rad_den), w + 8), w + 8), w); });
    return t;
}

Test one_mean_z_test(const Sample& s, const BigFloat& sigma, const BigFloat& mu0, Tail tail, std::uint64_t population, const BigFloatContext& ctx) {
    Test t;
    t.name       = "One-sample z-test for a mean";
    t.tail       = tail;
    t.null_value = mu0.rounded(ctx);
    if (!usable(s, false) || !positive(sigma)) return t;
    add_normality(t.conditions, s.n, "sample");
    add_population(t.conditions, s.n, population, "sample");
    t.estimate       = drive(ctx, [&](std::size_t w) { return mean_rv(s, w); });
    t.standard_error = drive(ctx, [&](std::size_t w) { return T::sqrt(known_over_n(sigma, s.n, w + 8), w); });
    finish_z(t, ctx, [&](std::size_t w) {
        return T::div(T::sub(mean_rv(s, w + 8), X(mu0), w + 8), T::sqrt(known_over_n(sigma, s.n, w + 8), w + 8), w);
    });
    return t;
}

Test two_mean_z_test(const Sample& a, const BigFloat& sigma1, const Sample& b, const BigFloat& sigma2, const BigFloat& delta0, Tail tail,
                     std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx) {
    Test t;
    t.name       = "Two-sample z-test for means";
    t.tail       = tail;
    t.null_value = delta0.rounded(ctx);
    if (!usable(a, false) || !usable(b, false) || !positive(sigma1) || !positive(sigma2)) return t;
    add_normality(t.conditions, a.n, "sample 1");
    add_normality(t.conditions, b.n, "sample 2");
    add_population(t.conditions, a.n, population1, "sample 1");
    add_population(t.conditions, b.n, population2, "sample 2");
    auto diff = [&](std::size_t w) { return T::sub(mean_rv(a, w), mean_rv(b, w), w); };
    auto se   = [&](std::size_t w) { return T::sqrt(T::add(known_over_n(sigma1, a.n, w + 8), known_over_n(sigma2, b.n, w + 8), w + 8), w); };
    t.estimate       = drive(ctx, diff);
    t.standard_error = drive(ctx, se);
    finish_z(t, ctx, [&](std::size_t w) { return T::div(T::sub(diff(w + 8), X(delta0), w + 8), se(w + 8), w); });
    return t;
}

Test one_mean_t_test(const Sample& s, const BigFloat& mu0, Tail tail, std::uint64_t population, const BigFloatContext& ctx) {
    Test t;
    t.name       = "One-sample t-test for a mean";
    t.tail       = tail;
    t.null_value = mu0.rounded(ctx);
    if (!usable(s, true) || s.n < 2) return t;
    add_normality(t.conditions, s.n, "sample");
    add_population(t.conditions, s.n, population, "sample");
    const BF df = N(s.n - 1);
    t.degrees_of_freedom = df.rounded(ctx);
    t.estimate           = drive(ctx, [&](std::size_t w) { return mean_rv(s, w); });
    t.standard_error     = drive(ctx, [&](std::size_t w) { return T::sqrt(variance_over_n(s, w + 8), w); });
    finish_t(t, ctx, df, [&](std::size_t w) {
        return T::div(T::sub(mean_rv(s, w + 8), X(mu0), w + 8), T::sqrt(variance_over_n(s, w + 8), w + 8), w);
    });
    return t;
}

Test two_mean_t_test(const Sample& a, const Sample& b, const BigFloat& delta0, Tail tail, bool pooled,
                     std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx) {
    Test t;
    t.name       = pooled ? "Two-sample pooled t-test for means" : "Two-sample t-test for means (Welch)";
    t.tail       = tail;
    t.null_value = delta0.rounded(ctx);
    if (!usable(a, true) || !usable(b, true) || a.n < 2 || b.n < 2) return t;
    add_normality(t.conditions, a.n, "sample 1");
    add_normality(t.conditions, b.n, "sample 2");
    add_population(t.conditions, a.n, population1, "sample 1");
    add_population(t.conditions, b.n, population2, "sample 2");
    const BF df = high_precision_df(a, b, pooled, ctx);
    t.degrees_of_freedom = df.rounded(ctx);
    auto diff = [&](std::size_t w) { return T::sub(mean_rv(a, w), mean_rv(b, w), w); };
    auto se   = [&](std::size_t w) { return two_sample_se(a, b, pooled, w); };
    t.estimate       = drive(ctx, diff);
    t.standard_error = drive(ctx, se);
    finish_t(t, ctx, df, [&](std::size_t w) { return T::div(T::sub(diff(w + 8), X(delta0), w + 8), se(w + 8), w); });
    return t;
}

Test paired_t_test(const Sample& d, const BigFloat& delta0, Tail tail, std::uint64_t population, const BigFloatContext& ctx) {
    Test t = one_mean_t_test(d, delta0, tail, population, ctx);
    t.name = "Paired t-test";
    return t;
}

Test chi_squared_goodness_of_fit(const std::vector<BigFloat>& observed, const std::vector<BigFloat>& expected, bool expected_are_proportions,
                                 const BigFloatContext& ctx) {
    Test t;
    t.name = "Chi-squared goodness-of-fit test";
    t.tail = Tail::upper;
    const std::size_t k = observed.size();
    if (k < 2 || expected.size() != k) return t;
    BF total = BF::zero();
    BF etot  = BF::zero();
    for (const BF& o : observed) {
        if (!o.is_finite() || o.signbit()) return t;
        total = eadd(total, o);
    }
    std::vector<BF> e(k);
    for (std::size_t i = 0; i < k; ++i) {
        if (!positive(expected[i])) return t;
        etot = eadd(etot, expected[i]);
    }

    for (std::size_t i = 0; i < k; ++i) e[i] = expected_are_proportions ? emul(total, expected[i]) : expected[i];

    if (expected_are_proportions) {
        Condition c;
        c.name   = "Proportions sum to 1";
        c.met    = cmp(esub(etot, BF::one()).abs(), BF(1.0e-9)) <= 0;
        c.detail = "sum = " + etot.to_string(17);
        t.conditions.push_back(c);
    } else {
        Condition c;
        c.name   = "Expected total matches observed total";
        c.met    = cmp(esub(etot, total).abs(), emul(total, BF(1.0e-9))) <= 0;
        c.detail = "observed " + total.to_string(17) + ", expected " + etot.to_string(17);
        t.conditions.push_back(c);
    }

    t.conditions.push_back(expected_counts_condition(e, N(5)));
    const BF df = N(k - 1);
    t.degrees_of_freedom = df.rounded(ctx);
    auto stat = [&](std::size_t w) {
        RV acc = X(BF::zero());
        for (std::size_t i = 0; i < k; ++i) {
            const BF d = esub(observed[i], e[i]);
            acc = T::add(acc, T::div(X(emul(d, d)), X(e[i]), w + 8), w + 8);
        }
        return acc;
    };
    const Dist chi(Kind::chi_squared, df);
    t.statistic = drive(ctx, stat);
    t.p_value   = drive(ctx, [&](std::size_t w) { return ddetail::cdf_rv(chi, stat(w + 8), true, w); });
    return t;
}

Test chi_squared_independence(const std::vector<std::vector<BigFloat>>& table, const BigFloatContext& ctx) {
    Test t;
    t.name = "Chi-squared test for independence/homogeneity";
    t.tail = Tail::upper;
    const std::size_t r = table.size();
    if (r < 2) return t;
    const std::size_t c = table[0].size();
    if (c < 2) return t;
    std::vector<BF> rows(r, BF::zero()), cols(c, BF::zero());
    BF total = BF::zero();

    for (std::size_t i = 0; i < r; ++i) {
        if (table[i].size() != c) return t;
        for (std::size_t j = 0; j < c; ++j) {
            const BF& v = table[i][j];
            if (!v.is_finite() || v.signbit()) return t;
            rows[i] = eadd(rows[i], v);
            cols[j] = eadd(cols[j], v);
            total   = eadd(total, v);
        }
    }

    for (const BF& v : rows) if (v.is_zero()) return t;
    for (const BF& v : cols) if (v.is_zero()) return t;
    std::vector<BF> expected;
    for (std::size_t i = 0; i < r; ++i) {
        for (std::size_t j = 0; j < c; ++j) expected.push_back(BF::div(emul(rows[i], cols[j]), total, BFC(128, math::RoundingMode::nearest_even)));
    }
    t.conditions.push_back(expected_counts_condition(expected, N(5)));
    const BF df = N(static_cast<std::uint64_t>((r - 1) * (c - 1)));
    t.degrees_of_freedom = df.rounded(ctx);
    auto stat = [&](std::size_t w) {
        RV acc = X(BF::zero());
        for (std::size_t i = 0; i < r; ++i) {
            for (std::size_t j = 0; j < c; ++j) {
                const BF rc = emul(rows[i], cols[j]);
                const BF d  = esub(emul(total, table[i][j]), rc);
                acc = T::add(acc, T::div(X(emul(d, d)), X(emul(total, rc)), w + 8), w + 8);
            }
        }
        return acc;
    };
    const Dist chi(Kind::chi_squared, df);
    t.statistic = drive(ctx, stat);
    t.p_value   = drive(ctx, [&](std::size_t w) { return ddetail::cdf_rv(chi, stat(w + 8), true, w); });
    return t;
}

namespace idetail {

struct RegSums {
    BF            n, sx, sy, sxx, sxy, syy, SXX, SXY, SYY;
    std::uint64_t count = 0;
    bool          ok    = false;
};

RegSums reg_sums(const std::vector<BF>& x, const std::vector<BF>& y) {
    RegSums s;
    s.n = s.sx = s.sy = s.sxx = s.sxy = s.syy = s.SXX = s.SXY = s.SYY = BF::zero();
    if (x.size() != y.size() || x.size() < 3) return s;
    s.count = x.size();
    s.n     = N(s.count);
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!x[i].is_finite() || !y[i].is_finite()) return s;
        s.sx  = eadd(s.sx, x[i]);
        s.sy  = eadd(s.sy, y[i]);
        s.sxx = eadd(s.sxx, emul(x[i], x[i]));
        s.syy = eadd(s.syy, emul(y[i], y[i]));
        s.sxy = eadd(s.sxy, emul(x[i], y[i]));
    }
    s.SXX = esub(emul(s.n, s.sxx), emul(s.sx, s.sx));
    s.SYY = esub(emul(s.n, s.syy), emul(s.sy, s.sy));
    s.SXY = esub(emul(s.n, s.sxy), emul(s.sx, s.sy));
    s.ok  = positive(s.SXX);
    return s;
}

RV slope_rv(const RegSums& s, std::size_t w) { return T::div(X(s.SXY), X(s.SXX), w); }

RV slope_se_rv(const RegSums& s, std::size_t w) {
    const BF sse = esub(emul(s.SYY, s.SXX), emul(s.SXY, s.SXY));
    return T::sqrt(T::div(X(sse), X(emul(N(s.count - 2), emul(s.SXX, s.SXX))), w + 8), w);
}

} // namespace idetail

Test slope_t_test(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloat& beta0, Tail tail, const BigFloatContext& ctx) {
    Test t;
    t.name       = "t-test for the slope of a regression line";
    t.tail       = tail;
    t.null_value = beta0.rounded(ctx);
    const RegSums s = reg_sums(x, y);
    if (!s.ok) return t;
    const BF sse = esub(emul(s.SYY, s.SXX), emul(s.SXY, s.SXY));
    if (!positive(sse)) return t;
    add_normality(t.conditions, s.count, "residuals");
    const BF df = N(s.count - 2);
    t.degrees_of_freedom = df.rounded(ctx);
    t.estimate           = drive(ctx, [&](std::size_t w) { return slope_rv(s, w); });
    t.standard_error     = drive(ctx, [&](std::size_t w) { return slope_se_rv(s, w); });
    finish_t(t, ctx, df, [&](std::size_t w) { return T::div(T::sub(slope_rv(s, w + 8), X(beta0), w + 8), slope_se_rv(s, w + 8), w); });
    return t;
}

Interval one_proportion_interval(std::uint64_t successes, std::uint64_t n, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "One-proportion z-interval";
    iv.confidence = confidence.rounded(ctx);
    if (n == 0 || successes > n || !probability_open(confidence)) return iv;
    Condition c;
    c.name   = "Large counts (successes and failures)";
    c.met    = successes >= 10 && n - successes >= 10;
    c.detail = std::to_string(successes) + " successes, " + std::to_string(n - successes) + " failures";
    iv.conditions.push_back(c);
    add_population(iv.conditions, n, population, "sample");
    const BF x  = N(successes);
    const BF nn = N(n);
    const BF se_num = emul(x, N(n - successes));
    const BF se_den = emul(nn, emul(nn, nn));
    finish_interval(iv, ctx,
        [&](std::size_t w) { return T::div(X(x), X(nn), w); },
        [&](std::size_t w) { return T::sqrt(T::div(X(se_num), X(se_den), w + 8), w); },
        [&](std::size_t w) { return z_crit(confidence, w); });
    return iv;
}

Interval two_proportion_interval(std::uint64_t x1, std::uint64_t n1, std::uint64_t x2, std::uint64_t n2, const BigFloat& confidence,
                                 std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "Two-proportion z-interval";
    iv.confidence = confidence.rounded(ctx);
    if (n1 == 0 || n2 == 0 || x1 > n1 || x2 > n2 || !probability_open(confidence)) return iv;
    Condition c;
    c.name   = "Large counts (successes and failures in both samples)";
    c.met    = x1 >= 10 && n1 - x1 >= 10 && x2 >= 10 && n2 - x2 >= 10;
    c.detail = std::to_string(x1) + "/" + std::to_string(n1 - x1) + " and " + std::to_string(x2) + "/" + std::to_string(n2 - x2);
    iv.conditions.push_back(c);
    add_population(iv.conditions, n1, population1, "sample 1");
    add_population(iv.conditions, n2, population2, "sample 2");
    const BF diff_num = esub(emul(N(x1), N(n2)), emul(N(x2), N(n1)));
    const BF n12      = emul(N(n1), N(n2));
    const BF v1n      = emul(N(x1), N(n1 - x1));
    const BF v1d      = emul(N(n1), emul(N(n1), N(n1)));
    const BF v2n      = emul(N(x2), N(n2 - x2));
    const BF v2d      = emul(N(n2), emul(N(n2), N(n2)));
    finish_interval(iv, ctx,
        [&](std::size_t w) { return T::div(X(diff_num), X(n12), w); },
        [&](std::size_t w) { return T::sqrt(T::add(T::div(X(v1n), X(v1d), w + 8), T::div(X(v2n), X(v2d), w + 8), w + 8), w); },
        [&](std::size_t w) { return z_crit(confidence, w); });
    return iv;
}

Interval one_mean_z_interval(const Sample& s, const BigFloat& sigma, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "One-sample z-interval for a mean";
    iv.confidence = confidence.rounded(ctx);
    if (!usable(s, false) || !positive(sigma) || !probability_open(confidence)) return iv;
    add_normality(iv.conditions, s.n, "sample");
    add_population(iv.conditions, s.n, population, "sample");
    finish_interval(iv, ctx,
        [&](std::size_t w) { return mean_rv(s, w); },
        [&](std::size_t w) { return T::sqrt(known_over_n(sigma, s.n, w + 8), w); },
        [&](std::size_t w) { return z_crit(confidence, w); });
    return iv;
}

Interval one_mean_t_interval(const Sample& s, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "One-sample t-interval for a mean";
    iv.confidence = confidence.rounded(ctx);
    if (!usable(s, true) || s.n < 2 || !probability_open(confidence)) return iv;
    add_normality(iv.conditions, s.n, "sample");
    add_population(iv.conditions, s.n, population, "sample");
    const BF df = N(s.n - 1);
    iv.degrees_of_freedom = df.rounded(ctx);
    finish_interval(iv, ctx,
        [&](std::size_t w) { return mean_rv(s, w); },
        [&](std::size_t w) { return T::sqrt(variance_over_n(s, w + 8), w); },
        [&](std::size_t w) { return t_crit(confidence, df, w); });
    return iv;
}

Interval two_mean_z_interval(const Sample& a, const BigFloat& sigma1, const Sample& b, const BigFloat& sigma2, const BigFloat& confidence,
                             std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "Two-sample z-interval for means";
    iv.confidence = confidence.rounded(ctx);
    if (!usable(a, false) || !usable(b, false) || !positive(sigma1) || !positive(sigma2) || !probability_open(confidence)) return iv;
    add_normality(iv.conditions, a.n, "sample 1");
    add_normality(iv.conditions, b.n, "sample 2");
    add_population(iv.conditions, a.n, population1, "sample 1");
    add_population(iv.conditions, b.n, population2, "sample 2");
    finish_interval(iv, ctx,
        [&](std::size_t w) { return T::sub(mean_rv(a, w), mean_rv(b, w), w); },
        [&](std::size_t w) { return T::sqrt(T::add(known_over_n(sigma1, a.n, w + 8), known_over_n(sigma2, b.n, w + 8), w + 8), w); },
        [&](std::size_t w) { return z_crit(confidence, w); });
    return iv;
}

Interval two_mean_t_interval(const Sample& a, const Sample& b, const BigFloat& confidence, bool pooled,
                             std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = pooled ? "Two-sample pooled t-interval for means" : "Two-sample t-interval for means (Welch)";
    iv.confidence = confidence.rounded(ctx);
    if (!usable(a, true) || !usable(b, true) || a.n < 2 || b.n < 2 || !probability_open(confidence)) return iv;
    add_normality(iv.conditions, a.n, "sample 1");
    add_normality(iv.conditions, b.n, "sample 2");
    add_population(iv.conditions, a.n, population1, "sample 1");
    add_population(iv.conditions, b.n, population2, "sample 2");
    const BF df = high_precision_df(a, b, pooled, ctx);
    iv.degrees_of_freedom = df.rounded(ctx);
    finish_interval(iv, ctx,
        [&](std::size_t w) { return T::sub(mean_rv(a, w), mean_rv(b, w), w); },
        [&](std::size_t w) { return two_sample_se(a, b, pooled, w); },
        [&](std::size_t w) { return t_crit(confidence, df, w); });
    return iv;
}

Interval paired_t_interval(const Sample& d, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx) {
    Interval iv = one_mean_t_interval(d, confidence, population, ctx);
    iv.name = "Paired t-interval";
    return iv;
}

Interval variance_interval(const Sample& s, const BigFloat& confidence, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "Chi-squared interval for a variance";
    iv.confidence = confidence.rounded(ctx);
    if (!usable(s, true) || s.n < 2 || !probability_open(confidence)) return iv;
    add_normality(iv.conditions, s.n, "sample");
    const BF   df   = N(s.n - 1);
    const BF   tail = esub(BF::one(), confidence).scaled_pow2(-1);
    const Dist chi(Kind::chi_squared, df);
    iv.degrees_of_freedom = df.rounded(ctx);
    iv.estimate           = drive(ctx, [&](std::size_t w) { return variance_rv(s, w); });
    auto ss = [&](std::size_t w) { return T::mul(variance_rv(s, w), X(df), w); };
    iv.lower = drive(ctx, [&](std::size_t w) { return T::div(ss(w + 8), ddetail::continuous_quantile(chi, tail, true, w + 8), w); });
    iv.upper = drive(ctx, [&](std::size_t w) { return T::div(ss(w + 8), ddetail::continuous_quantile(chi, tail, false, w + 8), w); });
    iv.margin_of_error = drive(ctx, [&](std::size_t w) {
        const RV lo = T::div(ss(w + 8), ddetail::continuous_quantile(chi, tail, true, w + 8), w + 8);
        const RV hi = T::div(ss(w + 8), ddetail::continuous_quantile(chi, tail, false, w + 8), w + 8);
        return T::scale2(T::sub(hi, lo, w), -1);
    });
    return iv;
}

Interval slope_interval(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloat& confidence, const BigFloatContext& ctx) {
    Interval iv;
    iv.name       = "t-interval for the slope of a regression line";
    iv.confidence = confidence.rounded(ctx);
    const RegSums s = reg_sums(x, y);
    if (!s.ok || !probability_open(confidence)) return iv;
    const BF sse = esub(emul(s.SYY, s.SXX), emul(s.SXY, s.SXY));
    if (!positive(sse)) return iv;
    add_normality(iv.conditions, s.count, "residuals");
    const BF df = N(s.count - 2);
    iv.degrees_of_freedom = df.rounded(ctx);
    finish_interval(iv, ctx,
        [&](std::size_t w) { return slope_rv(s, w); },
        [&](std::size_t w) { return slope_se_rv(s, w); },
        [&](std::size_t w) { return t_crit(confidence, df, w); });
    return iv;
}

namespace idetail {

BF alpha_tail(const BF& alpha, Tail tail) { return tail == Tail::two ? alpha.scaled_pow2(-1) : alpha; }

RV z_upper(const BF& a, std::size_t w) { return ddetail::continuous_quantile(standard_normal(), a, true, w); }

RV z_power(const RV& shift, const BF& alpha, Tail tail, std::size_t w) {
    const RV zc = z_upper(alpha_tail(alpha, tail), w + 8);
    switch (tail) {
        case Tail::upper: return ddetail::phi(T::sub(zc, shift, w + 8), true, w);
        case Tail::lower: return ddetail::phi(T::sub(T::neg(zc), shift, w + 8), false, w);
        default:
            return T::add(ddetail::phi(T::sub(zc, shift, w + 8), true, w + 4), ddetail::phi(T::sub(T::neg(zc), shift, w + 8), false, w + 4), w);
    }
}

RV proportion_power(const RV& se0, const RV& sea, const RV& delta, const BF& alpha, Tail tail, std::size_t w) {
    const RV zc = z_upper(alpha_tail(alpha, tail), w + 8);
    const RV cut = T::mul(zc, se0, w + 8);
    auto upper = [&](std::size_t ww) { return ddetail::phi(T::div(T::sub(cut, delta, ww + 8), sea, ww + 8), true, ww); };
    auto lower = [&](std::size_t ww) { return ddetail::phi(T::div(T::sub(T::neg(cut), delta, ww + 8), sea, ww + 8), false, ww); };
    switch (tail) {
        case Tail::upper: return upper(w);
        case Tail::lower: return lower(w);
        default:          return T::add(upper(w + 4), lower(w + 4), w);
    }
}

BF t_power(const BF& df, const BF& ncp, const BF& alpha, Tail tail, const BFC& ctx) {
    const std::size_t w  = ctx.precision + 32;
    const BFC         c  = BFC(w, math::RoundingMode::nearest_even);
    const BF          tc = quantile(Dist(Kind::student_t, df), alpha_tail(alpha, tail), true, c);
    const BF          hi = BF::sub(BF::one(), ddetail::nct_cdf_bf(tc, df, ncp, w), c);
    const BF          lo = ddetail::nct_cdf_bf(-tc, df, ncp, w);
    BF r;
    switch (tail) {
        case Tail::upper: r = hi; break;
        case Tail::lower: r = lo; break;
        default:          r = BF::add(hi, lo, c); break;
    }
    return r.rounded(ctx);
}

} // namespace idetail

BigFloat one_mean_z_power(const BigFloat& mu0, const BigFloat& mu_a, const BigFloat& sigma, std::uint64_t n, const BigFloat& alpha, Tail tail,
                          const BigFloatContext& ctx) {
    if (n == 0 || !positive(sigma) || !probability_open(alpha)) return BF::nan();
    return drive(ctx, [&](std::size_t w) {
        const RV shift = T::div(X(esub(mu_a, mu0)), T::sqrt(known_over_n(sigma, n, w + 16), w + 16), w + 16);
        return z_power(shift, alpha, tail, w);
    });
}

BigFloat two_mean_z_power(const BigFloat& delta0, const BigFloat& delta_a, const BigFloat& sigma1, std::uint64_t n1, const BigFloat& sigma2,
                          std::uint64_t n2, const BigFloat& alpha, Tail tail, const BigFloatContext& ctx) {
    if (n1 == 0 || n2 == 0 || !positive(sigma1) || !positive(sigma2) || !probability_open(alpha)) return BF::nan();
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 16;
        const RV se    = T::sqrt(T::add(known_over_n(sigma1, n1, wi), known_over_n(sigma2, n2, wi), wi), wi);
        const RV shift = T::div(X(esub(delta_a, delta0)), se, wi);
        return z_power(shift, alpha, tail, w);
    });
}

BigFloat one_proportion_z_power(const BigFloat& p0, const BigFloat& p_a, std::uint64_t n, const BigFloat& alpha, Tail tail, const BigFloatContext& ctx) {
    if (n == 0 || !probability_open(p0) || !probability_open(p_a) || !probability_open(alpha)) return BF::nan();
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi  = w + 16;
        const RV          se0 = T::sqrt(T::div(X(emul(p0, esub(BF::one(), p0))), X(N(n)), wi), wi);
        const RV          sea = T::sqrt(T::div(X(emul(p_a, esub(BF::one(), p_a))), X(N(n)), wi), wi);
        return proportion_power(se0, sea, X(esub(p_a, p0)), alpha, tail, w);
    });
}

BigFloat two_proportion_z_power(const BigFloat& p1, const BigFloat& p2, std::uint64_t n1, std::uint64_t n2, const BigFloat& alpha, Tail tail,
                                const BigFloatContext& ctx) {
    if (n1 == 0 || n2 == 0 || !probability_open(p1) || !probability_open(p2) || !probability_open(alpha)) return BF::nan();
    const BF ns  = N(n1 + n2);
    const BF pbn = eadd(emul(N(n1), p1), emul(N(n2), p2));
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi  = w + 16;
        const RV          pb  = T::div(X(pbn), X(ns), wi);
        const RV          inv = T::div(X(ns), X(emul(N(n1), N(n2))), wi);
        const RV          se0 = T::sqrt(T::mul(T::mul(pb, T::one_minus(pb, wi), wi), inv, wi), wi);
        const RV          va  = T::add(T::div(X(emul(p1, esub(BF::one(), p1))), X(N(n1)), wi), T::div(X(emul(p2, esub(BF::one(), p2))), X(N(n2)), wi), wi);
        return proportion_power(se0, T::sqrt(va, wi), X(esub(p1, p2)), alpha, tail, w);
    });
}

BigFloat one_mean_t_power(const BigFloat& mu0, const BigFloat& mu_a, const BigFloat& sd, std::uint64_t n, const BigFloat& alpha, Tail tail,
                          const BigFloatContext& ctx) {
    if (n < 2 || !positive(sd) || !probability_open(alpha)) return BF::nan();
    const BFC hc(ctx.precision + 64, math::RoundingMode::nearest_even);
    const BF  ncp = BF::div(BF::mul(esub(mu_a, mu0), math::sqrt(N(n), hc), hc), sd, hc);
    return t_power(N(n - 1), ncp, alpha, tail, ctx);
}

BigFloat two_mean_t_power(const BigFloat& delta0, const BigFloat& delta_a, const BigFloat& sd1, std::uint64_t n1, const BigFloat& sd2, std::uint64_t n2,
                          const BigFloat& alpha, Tail tail, bool pooled, const BigFloatContext& ctx) {
    if (n1 < 2 || n2 < 2 || !positive(sd1) || !positive(sd2) || !probability_open(alpha)) return BF::nan();
    const BFC    hc = BFC(ctx.precision + 64, math::RoundingMode::nearest_even);
    const Sample a  = summary(BF::zero(), sd1, n1);
    const Sample b  = summary(BF::zero(), sd2, n2);
    const BF     df = high_precision_df(a, b, pooled, hc);
    const BF     se = two_sample_se(a, b, pooled, hc.precision).v;
    const BF     ncp = BF::div(esub(delta_a, delta0), se, hc);
    return t_power(df, ncp, alpha, tail, ctx);
}

std::uint64_t sample_size_for_mean(const BigFloat& margin, const BigFloat& sigma, const BigFloat& confidence) {
    if (!positive(margin) || !positive(sigma) || !probability_open(confidence)) return 0;
    const BFC c(192, math::RoundingMode::nearest_even);
    const BF  z = z_star(confidence, c);
    const BF  r = BF::div(BF::mul(z, sigma, c), margin, c);
    return static_cast<std::uint64_t>(math::ceiling(BF::mul(r, r, c)).to_double());
}

std::uint64_t sample_size_for_proportion(const BigFloat& margin, const BigFloat& confidence, const BigFloat& p_guess) {
    if (!positive(margin) || !probability_open(confidence) || p_guess.signbit() || cmp(p_guess, BF::one()) > 0) return 0;
    const BFC c(192, math::RoundingMode::nearest_even);
    const BF  z = z_star(confidence, c);
    const BF  v = BF::div(BF::mul(BF::mul(z, z, c), emul(p_guess, esub(BF::one(), p_guess)), c), BF::mul(margin, margin, c), c);
    return static_cast<std::uint64_t>(math::ceiling(v).to_double());
}

} // namespace core
} // namespace statistics
} // namespace fizmo
