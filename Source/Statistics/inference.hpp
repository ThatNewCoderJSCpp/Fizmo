#ifndef FIZMO_STATISTICS_INFERENCE_HPP
#define FIZMO_STATISTICS_INFERENCE_HPP

#include "descriptive.hpp"

namespace fizmo {
namespace statistics {

enum class Expected : std::uint8_t { automatic, counts, proportions };

enum class Variances : std::uint8_t { unequal, equal, welch = unequal, pooled = equal };

template <class T>
struct TestResult {
    std::string            name;
    T                      statistic;
    T                      p_value;
    T                      degrees_of_freedom;
    T                      estimate;
    T                      null_value;
    T                      standard_error;
    Tail                   tail = Tail::two;
    std::vector<Condition> conditions;

    bool reject(const T& alpha) const         { return sdetail::less(p_value, alpha); }
    bool fail_to_reject(const T& alpha) const { return !reject(alpha); }
    bool conditions_met() const               { return all_met(conditions); }
};

template <class T>
struct ConfidenceInterval {
    std::string            name;
    T                      estimate;
    T                      critical_value;
    T                      standard_error;
    T                      margin_of_error;
    T                      lower;
    T                      upper;
    T                      confidence;
    T                      degrees_of_freedom;
    std::vector<Condition> conditions;

    bool contains(const T& value) const { return !sdetail::less(value, lower) && !sdetail::less(upper, value); }
    bool conditions_met() const         { return all_met(conditions); }

    T width() const {
        return sdetail::compute<T>([&](const BigFloatContext& c) { return BigFloat::sub(sdetail::big(upper), sdetail::big(lower), c); });
    }
};

namespace sdetail {

template <class T>
TestResult<T> convert(const core::Test& t) {
    TestResult<T> r;
    r.name               = t.name;
    r.statistic          = from_big<T>(t.statistic);
    r.p_value            = from_big<T>(t.p_value);
    r.degrees_of_freedom = from_big<T>(t.degrees_of_freedom);
    r.estimate           = from_big<T>(t.estimate);
    r.null_value         = from_big<T>(t.null_value);
    r.standard_error     = from_big<T>(t.standard_error);
    r.tail               = t.tail;
    r.conditions         = t.conditions;
    return r;
}

template <class T>
ConfidenceInterval<T> convert(const core::Interval& i) {
    ConfidenceInterval<T> r;
    r.name               = i.name;
    r.estimate           = from_big<T>(i.estimate);
    r.critical_value     = from_big<T>(i.critical_value);
    r.standard_error     = from_big<T>(i.standard_error);
    r.margin_of_error    = from_big<T>(i.margin_of_error);
    r.lower              = from_big<T>(i.lower);
    r.upper              = from_big<T>(i.upper);
    r.confidence         = from_big<T>(i.confidence);
    r.degrees_of_freedom = from_big<T>(i.degrees_of_freedom);
    r.conditions         = i.conditions;
    return r;
}

template <class T>
core::Sample sample(const T& mean, const T& sd, std::uint64_t n) { return core::summary(big(mean), big(sd), n); }

template <class C>
core::Sample sample(const C& data) { return core::summary(values(data)); }

template <class T>
using enable_real = typename std::enable_if<is_real<T>::value, int>::type;

template <class C, class = void>
struct is_table : std::false_type {};

template <class C>
struct is_table<C, typename std::enable_if<is_range<C>::value && is_range<element_t<C>>::value>::type>
    : std::integral_constant<bool, is_real<element_t<element_t<C>>>::value> {};

template <class C>
using enable_table = typename std::enable_if<is_table<C>::value, int>::type;

inline bool proportions_guess(const std::vector<BigFloat>& expected) {
    const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
    BigFloat              total = BigFloat::zero();
    for (const BigFloat& e : expected) {
        if (BigFloat::compare(e, BigFloat::one()) == BigFloat::ordering::greater) return false;
        total = BigFloat::add(total, e, exact);
    }
    const BigFloat gap = BigFloat::sub(total, BigFloat::one(), exact).abs();
    return gap.is_zero() || BigFloat::compare(gap, BigFloat(1.0e-9)) == BigFloat::ordering::less;
}

} // namespace sdetail

inline Condition ten_percent_condition(std::uint64_t n, std::uint64_t population) { return core::ten_percent_condition(n, population); }

template <class T, sdetail::enable_real<T> = 0>
Condition large_counts_condition(std::uint64_t n, const T& p, std::uint64_t threshold = 10) {
    return core::large_counts_condition(n, sdetail::big(p), "sample", threshold);
}

inline Condition large_sample_condition(std::uint64_t n, std::uint64_t threshold = 30) { return core::large_sample_condition(n, "sample", threshold); }

template <class C, sdetail::enable_range<C> = 0>
Condition expected_counts_condition(const C& expected, double threshold = 5.0) {
    return core::expected_counts_condition(sdetail::values(expected), BigFloat(threshold));
}

inline std::uint64_t degrees_of_freedom(std::uint64_t n) { return n == 0 ? 0 : n - 1; }
inline std::uint64_t pooled_degrees_of_freedom(std::uint64_t n1, std::uint64_t n2) { return n1 + n2 < 2 ? 0 : n1 + n2 - 2; }
inline std::uint64_t chi_squared_degrees_of_freedom(std::uint64_t categories) { return categories == 0 ? 0 : categories - 1; }
inline std::uint64_t chi_squared_degrees_of_freedom(std::uint64_t rows, std::uint64_t columns) {
    return (rows == 0 || columns == 0) ? 0 : (rows - 1) * (columns - 1);
}

template <class T, sdetail::enable_real<T> = 0>
T welch_degrees_of_freedom(const T& sd1, std::uint64_t n1, const sdetail::same_t<T>& sd2, std::uint64_t n2) {
    const core::Sample a = sdetail::sample(T(sd1), sd1, n1);
    const core::Sample b = sdetail::sample(T(sd2), sd2, n2);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::welch_degrees_of_freedom(a, b, c); });
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
sdetail::element_t<C> welch_degrees_of_freedom(const C& data1, const D& data2) {
    const core::Sample a = sdetail::sample(data1);
    const core::Sample b = sdetail::sample(data2);
    return sdetail::compute<sdetail::element_t<C>>([&](const BigFloatContext& c) { return core::welch_degrees_of_freedom(a, b, c); });
}

template <class T, sdetail::enable_real<T> = 0>
T z_statistic(const T& estimate, const sdetail::same_t<T>& null_value, const sdetail::same_t<T>& standard_error) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::z_statistic(sdetail::big(estimate), sdetail::big(null_value), sdetail::big(standard_error), c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T t_statistic(const T& mean, const sdetail::same_t<T>& sd, std::uint64_t n, const sdetail::same_t<T>& mu0) {
    const core::Sample s = sdetail::sample(mean, sd, n);
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::t_statistic(s, sdetail::big(mu0), c); });
}

template <class C, sdetail::enable_range<C> = 0>
sdetail::element_t<C> t_statistic(const C& data, const sdetail::same_t<sdetail::element_t<C>>& mu0) {
    const core::Sample s = sdetail::sample(data);
    return sdetail::compute<sdetail::element_t<C>>([&](const BigFloatContext& c) { return core::t_statistic(s, sdetail::big(mu0), c); });
}

template <class T, sdetail::enable_real<T> = 0>
T critical_z(const T& alpha, Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::critical_z(sdetail::big(alpha), tail, c); });
}

template <class T, sdetail::enable_real<T> = 0>
T critical_t(const T& alpha, const sdetail::same_t<T>& df, Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::critical_t(sdetail::big(alpha), sdetail::big(df), tail, c); });
}

template <class T, sdetail::enable_real<T> = 0>
T critical_chi_squared(const T& alpha, const sdetail::same_t<T>& df) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::critical_chi_squared(sdetail::big(alpha), sdetail::big(df), c); });
}

template <class T, sdetail::enable_real<T> = 0>
T critical_f(const T& alpha, const sdetail::same_t<T>& numerator_df, const sdetail::same_t<T>& denominator_df) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::critical_f(sdetail::big(alpha), sdetail::big(numerator_df), sdetail::big(denominator_df), c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T z_star(const T& confidence) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::z_star(sdetail::big(confidence), c); });
}

template <class T, sdetail::enable_real<T> = 0>
T t_star(const T& confidence, const sdetail::same_t<T>& df) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::t_star(sdetail::big(confidence), sdetail::big(df), c); });
}

template <class T = double, sdetail::enable_real<T> = 0>
TestResult<T> proportion_z_test(std::uint64_t successes, std::uint64_t n, const sdetail::same_t<T>& p0, Tail tail = Tail::two, std::uint64_t population = 0) {
    return sdetail::convert<T>(core::one_proportion_z_test(successes, n, sdetail::big(p0), tail, population, sdetail::context_for<T>()));
}

template <class T = double, sdetail::enable_real<T> = 0>
TestResult<T> two_proportion_z_test(std::uint64_t successes1, std::uint64_t n1, std::uint64_t successes2, std::uint64_t n2, Tail tail = Tail::two,
                                    std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    return sdetail::convert<T>(core::two_proportion_z_test(successes1, n1, successes2, n2, tail, population1, population2, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
TestResult<T> mean_z_test(const T& mean, const sdetail::same_t<T>& sigma, std::uint64_t n, const sdetail::same_t<T>& mu0, Tail tail = Tail::two,
                          std::uint64_t population = 0) {
    return sdetail::convert<T>(core::one_mean_z_test(sdetail::sample(mean, T(sigma), n), sdetail::big(sigma), sdetail::big(mu0), tail, population,
                                                     sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
TestResult<sdetail::element_t<C>> mean_z_test(const C& data, const sdetail::same_t<sdetail::element_t<C>>& sigma,
                                              const sdetail::same_t<sdetail::element_t<C>>& mu0, Tail tail = Tail::two, std::uint64_t population = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::one_mean_z_test(sdetail::sample(data), sdetail::big(sigma), sdetail::big(mu0), tail, population, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
TestResult<T> two_mean_z_test(const T& mean1, const sdetail::same_t<T>& sigma1, std::uint64_t n1, const sdetail::same_t<T>& mean2,
                              const sdetail::same_t<T>& sigma2, std::uint64_t n2, const sdetail::same_t<T>& delta0 = T(0), Tail tail = Tail::two,
                              std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    return sdetail::convert<T>(core::two_mean_z_test(sdetail::sample(mean1, sigma1, n1), sdetail::big(sigma1), sdetail::sample(mean2, sigma2, n2),
                                                     sdetail::big(sigma2), sdetail::big(delta0), tail, population1, population2, sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> two_mean_z_test(const C& data1, const sdetail::same_t<sdetail::element_t<C>>& sigma1, const D& data2,
                                                  const sdetail::same_t<sdetail::element_t<C>>& sigma2,
                                                  const sdetail::same_t<sdetail::element_t<C>>& delta0 = sdetail::element_t<C>(0),
                                                  Tail tail = Tail::two, std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::two_mean_z_test(sdetail::sample(data1), sdetail::big(sigma1), sdetail::sample(data2), sdetail::big(sigma2),
                                                     sdetail::big(delta0), tail, population1, population2, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
TestResult<T> mean_t_test(const T& mean, const sdetail::same_t<T>& sd, std::uint64_t n, const sdetail::same_t<T>& mu0, Tail tail = Tail::two,
                          std::uint64_t population = 0) {
    return sdetail::convert<T>(core::one_mean_t_test(sdetail::sample(mean, sd, n), sdetail::big(mu0), tail, population, sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
TestResult<sdetail::element_t<C>> mean_t_test(const C& data, const sdetail::same_t<sdetail::element_t<C>>& mu0, Tail tail = Tail::two,
                                              std::uint64_t population = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::one_mean_t_test(sdetail::sample(data), sdetail::big(mu0), tail, population, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
TestResult<T> two_mean_t_test(const T& mean1, const sdetail::same_t<T>& sd1, std::uint64_t n1, const sdetail::same_t<T>& mean2,
                              const sdetail::same_t<T>& sd2, std::uint64_t n2, const sdetail::same_t<T>& delta0 = T(0), Tail tail = Tail::two,
                              bool pooled = false, std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    return sdetail::convert<T>(core::two_mean_t_test(sdetail::sample(mean1, sd1, n1), sdetail::sample(mean2, sd2, n2), sdetail::big(delta0), tail, pooled,
                                                     population1, population2, sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> two_mean_t_test(const C& data1, const D& data2, const sdetail::same_t<sdetail::element_t<C>>& delta0 = sdetail::element_t<C>(0),
                                                  Tail tail = Tail::two, bool pooled = false, std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::two_mean_t_test(sdetail::sample(data1), sdetail::sample(data2), sdetail::big(delta0), tail, pooled, population1,
                                                     population2, sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> paired_t_test(const C& before, const D& after, const sdetail::same_t<sdetail::element_t<C>>& delta0 = sdetail::element_t<C>(0),
                                                Tail tail = Tail::two, std::uint64_t population = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::paired_t_test(core::differences(sdetail::values(before), sdetail::values(after)), sdetail::big(delta0), tail, population,
                                                   sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
TestResult<T> paired_t_test(const T& mean_difference, const sdetail::same_t<T>& sd_difference, std::uint64_t n, const sdetail::same_t<T>& delta0 = T(0),
                            Tail tail = Tail::two, std::uint64_t population = 0) {
    return sdetail::convert<T>(core::paired_t_test(sdetail::sample(mean_difference, sd_difference, n), sdetail::big(delta0), tail, population,
                                                   sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> chi_squared_gof_test(const C& observed, const D& expected, Expected kind = Expected::automatic) {
    using T = sdetail::element_t<C>;
    const std::vector<BigFloat> o = sdetail::values(observed);
    const std::vector<BigFloat> e = sdetail::values(expected);
    const bool props = kind == Expected::proportions || (kind == Expected::automatic && sdetail::proportions_guess(e));
    return sdetail::convert<T>(core::chi_squared_goodness_of_fit(o, e, props, sdetail::context_for<T>()));
}

template <class C, sdetail::enable_table<C> = 0>
TestResult<sdetail::element_t<sdetail::element_t<C>>> chi_squared_independence_test(const C& table) {
    using T = sdetail::element_t<sdetail::element_t<C>>;
    return sdetail::convert<T>(core::chi_squared_independence(sdetail::to_big_table(table), sdetail::context_for<T>()));
}

template <class C, sdetail::enable_table<C> = 0>
TestResult<sdetail::element_t<sdetail::element_t<C>>> chi_squared_homogeneity_test(const C& table) {
    auto r = chi_squared_independence_test(table);
    r.name = "Chi-squared test for homogeneity";
    return r;
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> slope_t_test(const C& x, const D& y, const sdetail::same_t<sdetail::element_t<C>>& beta0 = sdetail::element_t<C>(0),
                                               Tail tail = Tail::two) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::slope_t_test(sdetail::values(x), sdetail::values(y), sdetail::big(beta0), tail, sdetail::context_for<T>()));
}

template <class T = double, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> proportion_interval(std::uint64_t successes, std::uint64_t n, const sdetail::same_t<T>& confidence, std::uint64_t population = 0) {
    return sdetail::convert<T>(core::one_proportion_interval(successes, n, sdetail::big(confidence), population, sdetail::context_for<T>()));
}

template <class T = double, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> two_proportion_interval(std::uint64_t successes1, std::uint64_t n1, std::uint64_t successes2, std::uint64_t n2,
                                              const sdetail::same_t<T>& confidence, std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    return sdetail::convert<T>(core::two_proportion_interval(successes1, n1, successes2, n2, sdetail::big(confidence), population1, population2,
                                                             sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> mean_z_interval(const T& mean, const sdetail::same_t<T>& sigma, std::uint64_t n, const sdetail::same_t<T>& confidence,
                                      std::uint64_t population = 0) {
    return sdetail::convert<T>(core::one_mean_z_interval(sdetail::sample(mean, sigma, n), sdetail::big(sigma), sdetail::big(confidence), population,
                                                         sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
ConfidenceInterval<sdetail::element_t<C>> mean_z_interval(const C& data, const sdetail::same_t<sdetail::element_t<C>>& sigma,
                                                          const sdetail::same_t<sdetail::element_t<C>>& confidence, std::uint64_t population = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::one_mean_z_interval(sdetail::sample(data), sdetail::big(sigma), sdetail::big(confidence), population,
                                                         sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> mean_interval(const T& mean, const sdetail::same_t<T>& sd, std::uint64_t n, const sdetail::same_t<T>& confidence,
                                    std::uint64_t population = 0) {
    return sdetail::convert<T>(core::one_mean_t_interval(sdetail::sample(mean, sd, n), sdetail::big(confidence), population, sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
ConfidenceInterval<sdetail::element_t<C>> mean_interval(const C& data, const sdetail::same_t<sdetail::element_t<C>>& confidence, std::uint64_t population = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::one_mean_t_interval(sdetail::sample(data), sdetail::big(confidence), population, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> two_mean_z_interval(const T& mean1, const sdetail::same_t<T>& sigma1, std::uint64_t n1, const sdetail::same_t<T>& mean2,
                                          const sdetail::same_t<T>& sigma2, std::uint64_t n2, const sdetail::same_t<T>& confidence,
                                          std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    return sdetail::convert<T>(core::two_mean_z_interval(sdetail::sample(mean1, sigma1, n1), sdetail::big(sigma1), sdetail::sample(mean2, sigma2, n2),
                                                         sdetail::big(sigma2), sdetail::big(confidence), population1, population2, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> two_mean_interval(const T& mean1, const sdetail::same_t<T>& sd1, std::uint64_t n1, const sdetail::same_t<T>& mean2,
                                        const sdetail::same_t<T>& sd2, std::uint64_t n2, const sdetail::same_t<T>& confidence, bool pooled = false,
                                        std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    return sdetail::convert<T>(core::two_mean_t_interval(sdetail::sample(mean1, sd1, n1), sdetail::sample(mean2, sd2, n2), sdetail::big(confidence), pooled,
                                                         population1, population2, sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> two_mean_interval(const C& data1, const D& data2, const sdetail::same_t<sdetail::element_t<C>>& confidence,
                                                            bool pooled = false, std::uint64_t population1 = 0, std::uint64_t population2 = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::two_mean_t_interval(sdetail::sample(data1), sdetail::sample(data2), sdetail::big(confidence), pooled, population1,
                                                         population2, sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> paired_interval(const C& before, const D& after, const sdetail::same_t<sdetail::element_t<C>>& confidence,
                                                          std::uint64_t population = 0) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::paired_t_interval(core::differences(sdetail::values(before), sdetail::values(after)), sdetail::big(confidence), population,
                                                       sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> variance_interval(const T& sd, std::uint64_t n, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::variance_interval(sdetail::sample(T(0), sd, n), sdetail::big(confidence), sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
ConfidenceInterval<sdetail::element_t<C>> variance_interval(const C& data, const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::variance_interval(sdetail::sample(data), sdetail::big(confidence), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> slope_interval(const C& x, const D& y, const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::slope_interval(sdetail::values(x), sdetail::values(y), sdetail::big(confidence), sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
T mean_z_test_power(const T& mu0, const sdetail::same_t<T>& mu_a, const sdetail::same_t<T>& sigma, std::uint64_t n, const sdetail::same_t<T>& alpha,
                    Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::one_mean_z_power(sdetail::big(mu0), sdetail::big(mu_a), sdetail::big(sigma), n, sdetail::big(alpha), tail, c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T two_mean_z_test_power(const T& delta0, const sdetail::same_t<T>& delta_a, const sdetail::same_t<T>& sigma1, std::uint64_t n1,
                        const sdetail::same_t<T>& sigma2, std::uint64_t n2, const sdetail::same_t<T>& alpha, Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::two_mean_z_power(sdetail::big(delta0), sdetail::big(delta_a), sdetail::big(sigma1), n1, sdetail::big(sigma2), n2, sdetail::big(alpha),
                                      tail, c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T proportion_z_test_power(const T& p0, const sdetail::same_t<T>& p_a, std::uint64_t n, const sdetail::same_t<T>& alpha, Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::one_proportion_z_power(sdetail::big(p0), sdetail::big(p_a), n, sdetail::big(alpha), tail, c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T two_proportion_z_test_power(const T& p1, const sdetail::same_t<T>& p2, std::uint64_t n1, std::uint64_t n2, const sdetail::same_t<T>& alpha,
                              Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::two_proportion_z_power(sdetail::big(p1), sdetail::big(p2), n1, n2, sdetail::big(alpha), tail, c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T mean_t_test_power(const T& mu0, const sdetail::same_t<T>& mu_a, const sdetail::same_t<T>& sd, std::uint64_t n, const sdetail::same_t<T>& alpha,
                    Tail tail = Tail::two) {
    return sdetail::from_big<T>(core::one_mean_t_power(sdetail::big(mu0), sdetail::big(mu_a), sdetail::big(sd), n, sdetail::big(alpha), tail,
                                                       sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
T paired_t_test_power(const T& delta0, const sdetail::same_t<T>& delta_a, const sdetail::same_t<T>& sd_difference, std::uint64_t n,
                      const sdetail::same_t<T>& alpha, Tail tail = Tail::two) {
    return mean_t_test_power(delta0, delta_a, sd_difference, n, alpha, tail);
}

template <class T, sdetail::enable_real<T> = 0>
T two_mean_t_test_power(const T& delta0, const sdetail::same_t<T>& delta_a, const sdetail::same_t<T>& sd1, std::uint64_t n1, const sdetail::same_t<T>& sd2,
                        std::uint64_t n2, const sdetail::same_t<T>& alpha, Tail tail = Tail::two, bool pooled = false) {
    return sdetail::from_big<T>(core::two_mean_t_power(sdetail::big(delta0), sdetail::big(delta_a), sdetail::big(sd1), n1, sdetail::big(sd2), n2,
                                                       sdetail::big(alpha), tail, pooled, sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
std::uint64_t sample_size_for_mean(const T& margin, const sdetail::same_t<T>& sigma, const sdetail::same_t<T>& confidence) {
    return core::sample_size_for_mean(sdetail::big(margin), sdetail::big(sigma), sdetail::big(confidence));
}

template <class T, sdetail::enable_real<T> = 0>
std::uint64_t sample_size_for_proportion(const T& margin, const sdetail::same_t<T>& confidence, const sdetail::same_t<T>& p_guess = T(0.5)) {
    return core::sample_size_for_proportion(sdetail::big(margin), sdetail::big(confidence), sdetail::big(p_guess));
}

template <class T>
class Sample {
    core::Sample  m_core;
    std::uint64_t m_population = 0;

    explicit Sample(core::Sample s) : m_core(std::move(s)) {}

public:
    using value_type = T;

    Sample() = default;

    static Sample from_summary(const T& mean, const T& standard_deviation, std::uint64_t size) {
        return Sample(core::summary(sdetail::big(mean), sdetail::big(standard_deviation), size));
    }

    template <class C, sdetail::enable_range<C> = 0>
    static Sample from_data(const C& data) { return Sample(core::summary(sdetail::values(data))); }

    template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
    static Sample from_differences(const C& before, const D& after) {
        return Sample(core::differences(sdetail::values(before), sdetail::values(after)));
    }

    Sample& from_population(std::uint64_t population_size) {
        m_population = population_size;
        return *this;
    }

    Sample with_population(std::uint64_t population_size) const {
        Sample out = *this;
        out.m_population = population_size;
        return out;
    }

    std::uint64_t size() const       { return m_core.n; }
    std::uint64_t population() const { return m_population; }
    bool          has_population() const { return m_population != 0; }
    bool          from_raw_data() const  { return m_core.from_data; }
    T             mean() const       { return sdetail::from_big<T>(m_core.mean); }
    T             standard_deviation() const { return sdetail::from_big<T>(m_core.sd); }
    std::uint64_t degrees_of_freedom() const { return m_core.n == 0 ? 0 : m_core.n - 1; }

    T standard_error() const {
        return sdetail::compute<T>([&](const BigFloatContext& c) {
            if (m_core.n == 0) return BigFloat::nan();
            const BigFloatContext hc(c.precision + 64, math::RoundingMode::nearest_even);
            return BigFloat::div(m_core.sd, math::sqrt(BigFloat(static_cast<std::uint64_t>(m_core.n)), hc), c);
        });
    }

    const core::Sample& core_sample() const { return m_core; }
};

template <class C, sdetail::enable_range<C> = 0>
Sample<sdetail::element_t<C>> sample_of(const C& data) { return Sample<sdetail::element_t<C>>::from_data(data); }

template <class T, sdetail::enable_real<T> = 0>
Sample<T> sample_summary(const T& mean, const sdetail::same_t<T>& standard_deviation, std::uint64_t size) {
    return Sample<T>::from_summary(mean, standard_deviation, size);
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
Sample<sdetail::element_t<C>> differences_of(const C& before, const D& after) { return Sample<sdetail::element_t<C>>::from_differences(before, after); }

struct Proportion {
    std::uint64_t successes  = 0;
    std::uint64_t size       = 0;
    std::uint64_t population = 0;

    Proportion() = default;
    Proportion(std::uint64_t success_count, std::uint64_t sample_size, std::uint64_t population_size = 0)
        : successes(success_count), size(sample_size), population(population_size) {}

    Proportion& from_population(std::uint64_t population_size) {
        population = population_size;
        return *this;
    }

    Proportion with_population(std::uint64_t population_size) const {
        Proportion out = *this;
        out.population = population_size;
        return out;
    }

    std::uint64_t failures() const { return successes > size ? 0 : size - successes; }

    template <class T = double>
    T value() const {
        return sdetail::compute<T>([&](const BigFloatContext& c) {
            if (size == 0) return BigFloat::nan();
            return BigFloat::div(BigFloat(successes), BigFloat(size), c);
        });
    }
};

template <class T>
T welch_degrees_of_freedom(const Sample<T>& a, const Sample<T>& b) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::welch_degrees_of_freedom(a.core_sample(), b.core_sample(), c); });
}

template <class T>
T t_statistic(const Sample<T>& s, const sdetail::same_t<T>& null_mean) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return core::t_statistic(s.core_sample(), sdetail::big(null_mean), c); });
}

template <class T = double, sdetail::enable_real<T> = 0>
TestResult<T> one_proportion_z_test(const Proportion& p, const sdetail::same_t<T>& null_proportion, Tail tail = Tail::two) {
    return sdetail::convert<T>(core::one_proportion_z_test(p.successes, p.size, sdetail::big(null_proportion), tail, p.population, sdetail::context_for<T>()));
}

template <class T = double, sdetail::enable_real<T> = 0>
TestResult<T> two_proportion_z_test(const Proportion& a, const Proportion& b, Tail tail = Tail::two) {
    return sdetail::convert<T>(core::two_proportion_z_test(a.successes, a.size, b.successes, b.size, tail, a.population, b.population, sdetail::context_for<T>()));
}

template <class T>
TestResult<T> one_mean_z_test(const Sample<T>& s, const sdetail::same_t<T>& sigma, const sdetail::same_t<T>& null_mean, Tail tail = Tail::two) {
    return sdetail::convert<T>(core::one_mean_z_test(s.core_sample(), sdetail::big(sigma), sdetail::big(null_mean), tail, s.population(), sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
TestResult<sdetail::element_t<C>> one_mean_z_test(const C& data, const sdetail::same_t<sdetail::element_t<C>>& sigma,
                                                  const sdetail::same_t<sdetail::element_t<C>>& null_mean, Tail tail = Tail::two) {
    return one_mean_z_test(sample_of(data), sigma, null_mean, tail);
}

template <class T>
TestResult<T> two_mean_z_test(const Sample<T>& a, const sdetail::same_t<T>& sigma1, const Sample<T>& b, const sdetail::same_t<T>& sigma2,
                              const sdetail::same_t<T>& null_difference = T(0), Tail tail = Tail::two) {
    return sdetail::convert<T>(core::two_mean_z_test(a.core_sample(), sdetail::big(sigma1), b.core_sample(), sdetail::big(sigma2), sdetail::big(null_difference),
                                                     tail, a.population(), b.population(), sdetail::context_for<T>()));
}

template <class T>
TestResult<T> one_mean_t_test(const Sample<T>& s, const sdetail::same_t<T>& null_mean, Tail tail = Tail::two) {
    return sdetail::convert<T>(core::one_mean_t_test(s.core_sample(), sdetail::big(null_mean), tail, s.population(), sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
TestResult<sdetail::element_t<C>> one_mean_t_test(const C& data, const sdetail::same_t<sdetail::element_t<C>>& null_mean, Tail tail = Tail::two) {
    return one_mean_t_test(sample_of(data), null_mean, tail);
}

template <class T>
TestResult<T> two_mean_t_test(const Sample<T>& a, const Sample<T>& b, const sdetail::same_t<T>& null_difference = T(0), Tail tail = Tail::two,
                              Variances variances = Variances::unequal) {
    return sdetail::convert<T>(core::two_mean_t_test(a.core_sample(), b.core_sample(), sdetail::big(null_difference), tail, variances == Variances::equal,
                                                     a.population(), b.population(), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> two_mean_t_test(const C& data1, const D& data2, const sdetail::same_t<sdetail::element_t<C>>& null_difference,
                                                  Tail tail, Variances variances) {
    return two_mean_t_test(sample_of(data1), sample_of(data2), null_difference, tail, variances);
}

template <class T>
TestResult<T> paired_t_test(const Sample<T>& differences, const sdetail::same_t<T>& null_difference = T(0), Tail tail = Tail::two) {
    return sdetail::convert<T>(core::paired_t_test(differences.core_sample(), sdetail::big(null_difference), tail, differences.population(), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
TestResult<sdetail::element_t<C>> chi_squared_goodness_of_fit_test(const C& observed, const D& expected, Expected kind = Expected::automatic) {
    using T = sdetail::element_t<C>;
    const std::vector<BigFloat> o = sdetail::values(observed);
    const std::vector<BigFloat> e = sdetail::values(expected);
    const bool props = kind == Expected::proportions || (kind == Expected::automatic && sdetail::proportions_guess(e));
    return sdetail::convert<T>(core::chi_squared_goodness_of_fit(o, e, props, sdetail::context_for<T>()));
}

template <class T = double, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> one_proportion_z_interval(const Proportion& p, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::one_proportion_interval(p.successes, p.size, sdetail::big(confidence), p.population, sdetail::context_for<T>()));
}

template <class T = double, sdetail::enable_real<T> = 0>
ConfidenceInterval<T> two_proportion_z_interval(const Proportion& a, const Proportion& b, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::two_proportion_interval(a.successes, a.size, b.successes, b.size, sdetail::big(confidence), a.population, b.population,
                                                             sdetail::context_for<T>()));
}

template <class T>
ConfidenceInterval<T> one_mean_z_interval(const Sample<T>& s, const sdetail::same_t<T>& sigma, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::one_mean_z_interval(s.core_sample(), sdetail::big(sigma), sdetail::big(confidence), s.population(), sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
ConfidenceInterval<sdetail::element_t<C>> one_mean_z_interval(const C& data, const sdetail::same_t<sdetail::element_t<C>>& sigma,
                                                              const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    return one_mean_z_interval(sample_of(data), sigma, confidence);
}

template <class T>
ConfidenceInterval<T> one_mean_t_interval(const Sample<T>& s, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::one_mean_t_interval(s.core_sample(), sdetail::big(confidence), s.population(), sdetail::context_for<T>()));
}

template <class C, sdetail::enable_range<C> = 0>
ConfidenceInterval<sdetail::element_t<C>> one_mean_t_interval(const C& data, const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    return one_mean_t_interval(sample_of(data), confidence);
}

template <class T>
ConfidenceInterval<T> two_mean_z_interval(const Sample<T>& a, const sdetail::same_t<T>& sigma1, const Sample<T>& b, const sdetail::same_t<T>& sigma2,
                                          const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::two_mean_z_interval(a.core_sample(), sdetail::big(sigma1), b.core_sample(), sdetail::big(sigma2), sdetail::big(confidence),
                                                         a.population(), b.population(), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> two_mean_z_interval(const C& data1, const sdetail::same_t<sdetail::element_t<C>>& sigma1, const D& data2,
                                                              const sdetail::same_t<sdetail::element_t<C>>& sigma2, const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    return two_mean_z_interval(sample_of(data1), sigma1, sample_of(data2), sigma2, confidence);
}

template <class T>
ConfidenceInterval<T> two_mean_t_interval(const Sample<T>& a, const Sample<T>& b, const sdetail::same_t<T>& confidence, Variances variances = Variances::unequal) {
    return sdetail::convert<T>(core::two_mean_t_interval(a.core_sample(), b.core_sample(), sdetail::big(confidence), variances == Variances::equal, a.population(),
                                                         b.population(), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> two_mean_t_interval(const C& data1, const D& data2, const sdetail::same_t<sdetail::element_t<C>>& confidence,
                                                              Variances variances = Variances::unequal) {
    return two_mean_t_interval(sample_of(data1), sample_of(data2), confidence, variances);
}

template <class T>
ConfidenceInterval<T> paired_t_interval(const Sample<T>& differences, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::paired_t_interval(differences.core_sample(), sdetail::big(confidence), differences.population(), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> paired_t_interval(const C& before, const D& after, const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    return paired_t_interval(differences_of(before, after), confidence);
}

template <class T>
ConfidenceInterval<T> variance_interval(const Sample<T>& s, const sdetail::same_t<T>& confidence) {
    return sdetail::convert<T>(core::variance_interval(s.core_sample(), sdetail::big(confidence), sdetail::context_for<T>()));
}

template <class C, class D, sdetail::enable_range<C> = 0, sdetail::enable_range<D> = 0>
ConfidenceInterval<sdetail::element_t<C>> slope_t_interval(const C& x, const D& y, const sdetail::same_t<sdetail::element_t<C>>& confidence) {
    using T = sdetail::element_t<C>;
    return sdetail::convert<T>(core::slope_interval(sdetail::values(x), sdetail::values(y), sdetail::big(confidence), sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
T one_mean_z_test_power(const T& null_mean, const sdetail::same_t<T>& true_mean, const sdetail::same_t<T>& sigma, std::uint64_t n, const sdetail::same_t<T>& alpha,
                        Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::one_mean_z_power(sdetail::big(null_mean), sdetail::big(true_mean), sdetail::big(sigma), n, sdetail::big(alpha), tail, c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T one_proportion_z_test_power(const T& null_proportion, const sdetail::same_t<T>& true_proportion, std::uint64_t n, const sdetail::same_t<T>& alpha,
                              Tail tail = Tail::two) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        return core::one_proportion_z_power(sdetail::big(null_proportion), sdetail::big(true_proportion), n, sdetail::big(alpha), tail, c);
    });
}

template <class T, sdetail::enable_real<T> = 0>
T one_mean_t_test_power(const T& null_mean, const sdetail::same_t<T>& true_mean, const sdetail::same_t<T>& sd, std::uint64_t n, const sdetail::same_t<T>& alpha,
                        Tail tail = Tail::two) {
    return sdetail::from_big<T>(core::one_mean_t_power(sdetail::big(null_mean), sdetail::big(true_mean), sdetail::big(sd), n, sdetail::big(alpha), tail,
                                                       sdetail::context_for<T>()));
}

template <class T, sdetail::enable_real<T> = 0>
T two_mean_t_test_power(const T& null_difference, const sdetail::same_t<T>& true_difference, const sdetail::same_t<T>& sd1, std::uint64_t n1,
                        const sdetail::same_t<T>& sd2, std::uint64_t n2, const sdetail::same_t<T>& alpha, Tail tail,
                        Variances variances) {
    return sdetail::from_big<T>(core::two_mean_t_power(sdetail::big(null_difference), sdetail::big(true_difference), sdetail::big(sd1), n1, sdetail::big(sd2), n2,
                                                       sdetail::big(alpha), tail, variances == Variances::equal, sdetail::context_for<T>()));
}

} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_INFERENCE_HPP
