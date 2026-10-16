#ifndef FIZMO_STATISTICS_DESCRIPTIVE_HPP
#define FIZMO_STATISTICS_DESCRIPTIVE_HPP

#include "core.hpp"

#include <algorithm>

namespace fizmo {
namespace statistics {

namespace sdetail {

template <class C>
using element_t = typename std::decay<decltype(*std::begin(std::declval<const C&>()))>::type;

template <class C, class = void>
struct is_range : std::false_type {};

template <class C>
struct is_range<C, decltype(void(std::begin(std::declval<const C&>())), void(std::end(std::declval<const C&>())))> : std::true_type {};

template <class T>
struct identity { using type = T; };

template <class T>
using same_t = typename identity<T>::type;

template <class C>
using enable_range = typename std::enable_if<is_range<C>::value && is_real<element_t<C>>::value, int>::type;

template <class C>
std::vector<BigFloat> values(const C& data) { return to_big_vector(data); }

} // namespace sdetail

template <class T>
struct Quartiles {
    T q1;
    T median;
    T q3;
};

template <class T>
struct FiveNumberSummary {
    T minimum;
    T q1;
    T median;
    T q3;
    T maximum;
};

template <class T>
struct LinearRegression {
    T             slope;
    T             intercept;
    T             r;
    T             r_squared;
    T             residual_standard_error;
    T             slope_standard_error;
    T             intercept_standard_error;
    std::uint64_t n = 0;

    T predict(const T& x) const {
        return sdetail::compute<T>([&](const BigFloatContext& c) {
            const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
            return BigFloat::add(sdetail::big(intercept), BigFloat::mul(sdetail::big(slope), sdetail::big(x), exact), c);
        });
    }

    T residual(const T& x, const T& y) const {
        return sdetail::compute<T>([&](const BigFloatContext& c) {
            const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
            const BigFloat        fit = BigFloat::add(sdetail::big(intercept), BigFloat::mul(sdetail::big(slope), sdetail::big(x), exact), exact);
            return BigFloat::sub(sdetail::big(y), fit, c);
        });
    }
};

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> sum(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::sum(v, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> mean(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::mean(v, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> variance(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::variance(v, true, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> sample_variance(const C& data) {
    return variance(data);
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> population_variance(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::variance(v, false, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> standard_deviation(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::standard_deviation(v, true, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> sample_standard_deviation(const C& data) {
    return standard_deviation(data);
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> population_standard_deviation(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::standard_deviation(v, false, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> standard_error(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::standard_error(v, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> quantile(const C& data, const sdetail::same_t<sdetail::element_t<C>>& p) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::quantile(v, sdetail::big(p), c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> percentile(const C& data, const sdetail::same_t<sdetail::element_t<C>>& percent) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::quantile(v, BigFloat::div(sdetail::big(percent), BigFloat(100), BigFloatContext(c.precision + 64)), c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> median(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::median(v, c); });
}

template <class C, sdetail::enable_range<C> = 0>
std::vector<sdetail::element_t<C>> modes(const C& data) {
    return sdetail::from_big_vector<sdetail::element_t<C>>(core::modes(sdetail::values(data)));
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> minimum(const C& data) {
    using T = sdetail::element_t<C>;
    auto it = std::begin(data);
    if (it == std::end(data)) return sdetail::nan_value<T>();
    T best = *it;
    for (++it; it != std::end(data); ++it) {
        if (sdetail::less(*it, best)) best = *it;
    }
    return best;
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> maximum(const C& data) {
    using T = sdetail::element_t<C>;
    auto it = std::begin(data);
    if (it == std::end(data)) return sdetail::nan_value<T>();
    T best = *it;
    for (++it; it != std::end(data); ++it) {
        if (sdetail::less(best, *it)) best = *it;
    }
    return best;
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> range(const C& data) {
    using T = sdetail::element_t<C>;
    const BigFloat lo = sdetail::big(minimum(data));
    const BigFloat hi = sdetail::big(maximum(data));
    return sdetail::compute<T>([&](const BigFloatContext& c) { return BigFloat::sub(hi, lo, c); });
}

template <class C, sdetail::enable_range<C> = 0>
Quartiles<sdetail::element_t<C>> quartiles(const C& data) {
    using T = sdetail::element_t<C>;
    std::vector<BigFloat> v = sdetail::values(data);
    std::sort(v.begin(), v.end(), [](const BigFloat& a, const BigFloat& b) { return BigFloat::compare(a, b) == BigFloat::ordering::less; });
    const std::size_t     n    = v.size();
    const std::size_t     half = n / 2;
    std::vector<BigFloat> lower(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(half));
    std::vector<BigFloat> upper(v.begin() + static_cast<std::ptrdiff_t>(n - half), v.end());
    Quartiles<T>          q;
    q.median = sdetail::compute<T>([&](const BigFloatContext& c) { return core::median(v, c); });
    q.q1     = n < 2 ? q.median : sdetail::compute<T>([&](const BigFloatContext& c) { return core::median(lower, c); });
    q.q3     = n < 2 ? q.median : sdetail::compute<T>([&](const BigFloatContext& c) { return core::median(upper, c); });
    return q;
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> interquartile_range(const C& data) {
    using T = sdetail::element_t<C>;
    const Quartiles<T> q = quartiles(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return BigFloat::sub(sdetail::big(q.q3), sdetail::big(q.q1), c); });
}

template <class C, sdetail::enable_range<C> = 0>
FiveNumberSummary<sdetail::element_t<C>> five_number_summary(const C& data) {
    using T = sdetail::element_t<C>;
    const Quartiles<T>   q = quartiles(data);
    FiveNumberSummary<T> s{minimum(data), q.q1, q.median, q.q3, maximum(data)};
    return s;
}

template <class C, sdetail::enable_range<C> = 0>
std::vector<sdetail::element_t<C>> outliers(const C& data, const sdetail::same_t<sdetail::element_t<C>>& fence_multiplier = 1.5) {
    using T = sdetail::element_t<C>;
    const Quartiles<T>    q     = quartiles(data);
    const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
    const BigFloat        iqr   = BigFloat::sub(sdetail::big(q.q3), sdetail::big(q.q1), exact);
    const BigFloat        reach = BigFloat::mul(iqr, sdetail::big(fence_multiplier), exact);
    const BigFloat        lo    = BigFloat::sub(sdetail::big(q.q1), reach, exact);
    const BigFloat        hi    = BigFloat::add(sdetail::big(q.q3), reach, exact);
    std::vector<T>        out;
    for (const auto& x : data) {
        const BigFloat v = sdetail::big(x);
        if (BigFloat::compare(v, lo) == BigFloat::ordering::less || BigFloat::compare(v, hi) == BigFloat::ordering::greater) out.push_back(x);
    }
    return out;
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> skewness(const C& data, bool sample = true) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::skewness(v, sample, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> excess_kurtosis(const C& data, bool sample = true) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::kurtosis_excess(v, sample, c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> pearson_kurtosis(const C& data, bool sample = true) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        return BigFloat::add(core::kurtosis_excess(v, sample, hc), BigFloat(3), c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> kurtosis(const C& data, bool sample = true) {
    return pearson_kurtosis(data, sample);
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> mode(const C& data) {
    using T = sdetail::element_t<C>;
    const std::vector<T> all = modes(data);
    return all.empty() ? sdetail::nan_value<T>() : all.front();
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> geometric_mean(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        if (v.empty()) return BigFloat::nan();
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        BigFloat              acc = BigFloat::zero();
        for (const BigFloat& x : v) {
            if (x.is_zero()) return BigFloat::zero();
            if (x.is_negative()) return BigFloat::nan();
            acc = BigFloat::add(acc, math::ln(x, hc), hc);
        }
        return math::exp(BigFloat::div(acc, BigFloat(static_cast<std::uint64_t>(v.size())), hc), c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> harmonic_mean(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        if (v.empty()) return BigFloat::nan();
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        BigFloat              acc = BigFloat::zero();
        for (const BigFloat& x : v) {
            if (x.is_zero()) return BigFloat::zero();
            acc = BigFloat::add(acc, BigFloat::div(BigFloat::one(), x, hc), hc);
        }
        return BigFloat::div(BigFloat(static_cast<std::uint64_t>(v.size())), acc, c);
    });
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
sdetail::element_t<C> weighted_mean(const C& data, const D& weights) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    const auto w = sdetail::values(weights);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        if (v.empty() || v.size() != w.size()) return BigFloat::nan();
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        BigFloat              top = BigFloat::zero();
        BigFloat              bottom = BigFloat::zero();
        for (std::size_t i = 0; i < v.size(); ++i) {
            top    = BigFloat::add(top, BigFloat::mul(v[i], w[i], exact), exact);
            bottom = BigFloat::add(bottom, w[i], exact);
        }
        if (bottom.is_zero()) return BigFloat::nan();
        return BigFloat::div(top, bottom, c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> mean_absolute_deviation(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        if (v.empty()) return BigFloat::nan();
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        const BigFloat        m   = core::mean(v, hc);
        BigFloat              acc = BigFloat::zero();
        for (const BigFloat& x : v) acc = BigFloat::add(acc, BigFloat::sub(x, m, hc).abs(), hc);
        return BigFloat::div(acc, BigFloat(static_cast<std::uint64_t>(v.size())), c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> median_absolute_deviation(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        if (v.empty()) return BigFloat::nan();
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        const BigFloat        m = core::median(v, hc);
        std::vector<BigFloat> gaps;
        gaps.reserve(v.size());
        for (const BigFloat& x : v) gaps.push_back(BigFloat::sub(x, m, exact).abs());
        return core::median(gaps, c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> coefficient_of_variation(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        return BigFloat::div(core::standard_deviation(v, true, hc), core::mean(v, hc), c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> sum_of_squares(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        BigFloat              acc = BigFloat::zero();
        for (const BigFloat& x : v) acc = BigFloat::add(acc, BigFloat::mul(x, x, exact), exact);
        return acc.rounded(c);
    });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> root_mean_square(const C& data) {
    using T = sdetail::element_t<C>;
    const auto v = sdetail::values(data);
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        if (v.empty()) return BigFloat::nan();
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
        BigFloat              acc = BigFloat::zero();
        for (const BigFloat& x : v) acc = BigFloat::add(acc, BigFloat::mul(x, x, exact), exact);
        return math::sqrt(BigFloat::div(acc, BigFloat(static_cast<std::uint64_t>(v.size())), hc), c);
    });
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
sdetail::element_t<C> covariance(const C& x, const D& y, bool sample = true) {
    using T = sdetail::element_t<C>;
    const auto a = sdetail::values(x);
    const auto b = sdetail::values(y);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::covariance(a, b, sample, c); });
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
sdetail::element_t<C> correlation(const C& x, const D& y) {
    using T = sdetail::element_t<C>;
    const auto a = sdetail::values(x);
    const auto b = sdetail::values(y);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::correlation(a, b, c); });
}

template <class T, typename std::enable_if<sdetail::is_real<T>::value, int>::type = 0>
T z_score(const T& x, const sdetail::same_t<T>& mean, const sdetail::same_t<T>& standard_deviation) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::z_score(sdetail::big(x), sdetail::big(mean), sdetail::big(standard_deviation), c); });
}

template <class C, sdetail::enable_range<C> = 0>
std::vector<sdetail::element_t<C>> z_scores(const C& data, bool sample = true) {
    using T = sdetail::element_t<C>;
    const auto            v = sdetail::values(data);
    std::vector<T>        out;
    if (v.size() < 2) return out;
    const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
    const BigFloatContext hc(sdetail::context_for<T>().precision + 64, math::RoundingMode::nearest_even);
    const BigFloat        n   = BigFloat(static_cast<std::uint64_t>(v.size()));
    const BigFloat        sum = core::sum(v, exact);
    const BigFloat        sd  = core::standard_deviation(v, sample, hc);
    for (const BigFloat& x : v) {
        const BigFloat num = BigFloat::sub(BigFloat::mul(x, n, exact), sum, exact);
        out.push_back(sdetail::compute<T>([&](const BigFloatContext& c) { return BigFloat::div(BigFloat::div(num, n, hc), sd, c); }));
    }
    return out;
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
LinearRegression<sdetail::element_t<C>> linear_regression(const C& x, const D& y) {
    using T = sdetail::element_t<C>;
    const auto              a = sdetail::values(x);
    const auto              b = sdetail::values(y);
    const core::Regression  r = core::regression(a, b, sdetail::context_for<T>());
    LinearRegression<T>     out;
    out.slope                    = sdetail::from_big<T>(r.slope);
    out.intercept                = sdetail::from_big<T>(r.intercept);
    out.r                        = sdetail::from_big<T>(r.r);
    out.r_squared                = sdetail::from_big<T>(r.r_squared);
    out.residual_standard_error  = sdetail::from_big<T>(r.residual_standard_error);
    out.slope_standard_error     = sdetail::from_big<T>(r.slope_standard_error);
    out.intercept_standard_error = sdetail::from_big<T>(r.intercept_standard_error);
    out.n                        = r.n;
    return out;
}

} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_DESCRIPTIVE_HPP
