#ifndef FIZMO_STATISTICS_CORE_HPP
#define FIZMO_STATISTICS_CORE_HPP

#include "common.hpp"

namespace fizmo {
namespace statistics {
namespace core {

enum class Kind : std::uint8_t {
    normal = 0,
    student_t,
    chi_squared,
    fisher_f,
    exponential,
    uniform,
    gamma,
    beta,
    noncentral_t,
    binomial,
    geometric,
    poisson,
    hypergeometric,
    negative_binomial
};

struct Dist {
    Kind     kind = Kind::normal;
    BigFloat a;
    BigFloat b;
    BigFloat c;

    Dist() : a(BigFloat::zero()), b(BigFloat::one()), c(BigFloat::zero()) {}
    Dist(Kind k, BigFloat x, BigFloat y = BigFloat::zero(), BigFloat z = BigFloat::zero()) : kind(k), a(std::move(x)), b(std::move(y)), c(std::move(z)) {}
};

bool discrete(const Dist& d);
bool valid(const Dist& d);

BigFloat pdf(const Dist& d, const BigFloat& x, const BigFloatContext& ctx);
BigFloat log_pdf(const Dist& d, const BigFloat& x, const BigFloatContext& ctx);
BigFloat cdf(const Dist& d, const BigFloat& x, bool upper, const BigFloatContext& ctx);
BigFloat between(const Dist& d, const BigFloat& lo, const BigFloat& hi, const BigFloatContext& ctx);
BigFloat outside(const Dist& d, const BigFloat& lo, const BigFloat& hi, const BigFloatContext& ctx);
BigFloat quantile(const Dist& d, const BigFloat& p, bool upper, const BigFloatContext& ctx);
BigFloat mean(const Dist& d, const BigFloatContext& ctx);
BigFloat variance(const Dist& d, const BigFloatContext& ctx);
BigFloat standard_deviation(const Dist& d, const BigFloatContext& ctx);

BigFloat sum(const std::vector<BigFloat>& data, const BigFloatContext& ctx);
BigFloat mean(const std::vector<BigFloat>& data, const BigFloatContext& ctx);
BigFloat variance(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx);
BigFloat standard_deviation(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx);
BigFloat standard_error(const std::vector<BigFloat>& data, const BigFloatContext& ctx);
BigFloat quantile(std::vector<BigFloat> data, const BigFloat& p, const BigFloatContext& ctx);
BigFloat median(std::vector<BigFloat> data, const BigFloatContext& ctx);
std::vector<BigFloat> modes(std::vector<BigFloat> data);
BigFloat skewness(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx);
BigFloat kurtosis_excess(const std::vector<BigFloat>& data, bool sample, const BigFloatContext& ctx);
BigFloat covariance(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, bool sample, const BigFloatContext& ctx);
BigFloat correlation(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloatContext& ctx);
BigFloat z_score(const BigFloat& x, const BigFloat& mean, const BigFloat& sd, const BigFloatContext& ctx);

struct Regression {
    BigFloat slope;
    BigFloat intercept;
    BigFloat r;
    BigFloat r_squared;
    BigFloat residual_standard_error;
    BigFloat slope_standard_error;
    BigFloat intercept_standard_error;
    std::uint64_t n = 0;
};

Regression regression(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloatContext& ctx);

struct Sample {
    std::uint64_t n          = 0;
    bool          from_data  = false;
    BigFloat      mean;
    BigFloat      sd;
    BigFloat      sum;
    BigFloat      sum_squares;

    Sample() : mean(BigFloat::zero()), sd(BigFloat::zero()), sum(BigFloat::zero()), sum_squares(BigFloat::zero()) {}
};

Sample summary(const BigFloat& mean, const BigFloat& sd, std::uint64_t n);
Sample summary(const std::vector<BigFloat>& data);
Sample differences(const std::vector<BigFloat>& a, const std::vector<BigFloat>& b);

struct Test {
    std::string            name;
    BigFloat               statistic;
    BigFloat               p_value;
    BigFloat               degrees_of_freedom;
    BigFloat               estimate;
    BigFloat               null_value;
    BigFloat               standard_error;
    Tail                   tail = Tail::two;
    std::vector<Condition> conditions;

    Test() : statistic(BigFloat::nan()), p_value(BigFloat::nan()), degrees_of_freedom(BigFloat::nan()), estimate(BigFloat::nan()),
             null_value(BigFloat::nan()), standard_error(BigFloat::nan()) {}
};

struct Interval {
    std::string            name;
    BigFloat               estimate;
    BigFloat               critical_value;
    BigFloat               standard_error;
    BigFloat               margin_of_error;
    BigFloat               lower;
    BigFloat               upper;
    BigFloat               confidence;
    BigFloat               degrees_of_freedom;
    std::vector<Condition> conditions;

    Interval() : estimate(BigFloat::nan()), critical_value(BigFloat::nan()), standard_error(BigFloat::nan()), margin_of_error(BigFloat::nan()),
                 lower(BigFloat::nan()), upper(BigFloat::nan()), confidence(BigFloat::nan()), degrees_of_freedom(BigFloat::nan()) {}
};

BigFloat critical_z(const BigFloat& alpha, Tail tail, const BigFloatContext& ctx);
BigFloat critical_t(const BigFloat& alpha, const BigFloat& df, Tail tail, const BigFloatContext& ctx);
BigFloat critical_chi_squared(const BigFloat& alpha, const BigFloat& df, const BigFloatContext& ctx);
BigFloat critical_f(const BigFloat& alpha, const BigFloat& d1, const BigFloat& d2, const BigFloatContext& ctx);
BigFloat z_star(const BigFloat& confidence, const BigFloatContext& ctx);
BigFloat t_star(const BigFloat& confidence, const BigFloat& df, const BigFloatContext& ctx);

BigFloat welch_degrees_of_freedom(const Sample& a, const Sample& b, const BigFloatContext& ctx);
BigFloat t_statistic(const Sample& s, const BigFloat& mu0, const BigFloatContext& ctx);
BigFloat z_statistic(const BigFloat& estimate, const BigFloat& null_value, const BigFloat& standard_error, const BigFloatContext& ctx);

Condition ten_percent_condition(std::uint64_t n, std::uint64_t population, const std::string& label = "sample");
Condition large_counts_condition(std::uint64_t n, const BigFloat& p, const std::string& label = "sample", std::uint64_t threshold = 10);
Condition large_sample_condition(std::uint64_t n, const std::string& label = "sample", std::uint64_t threshold = 30);
Condition expected_counts_condition(const std::vector<BigFloat>& expected, const BigFloat& threshold);

Test one_proportion_z_test(std::uint64_t successes, std::uint64_t n, const BigFloat& p0, Tail tail, std::uint64_t population, const BigFloatContext& ctx);
Test two_proportion_z_test(std::uint64_t x1, std::uint64_t n1, std::uint64_t x2, std::uint64_t n2, Tail tail,
                           std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx);
Test one_mean_z_test(const Sample& s, const BigFloat& sigma, const BigFloat& mu0, Tail tail, std::uint64_t population, const BigFloatContext& ctx);
Test two_mean_z_test(const Sample& a, const BigFloat& sigma1, const Sample& b, const BigFloat& sigma2, const BigFloat& delta0, Tail tail,
                     std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx);
Test one_mean_t_test(const Sample& s, const BigFloat& mu0, Tail tail, std::uint64_t population, const BigFloatContext& ctx);
Test two_mean_t_test(const Sample& a, const Sample& b, const BigFloat& delta0, Tail tail, bool pooled,
                     std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx);
Test paired_t_test(const Sample& d, const BigFloat& delta0, Tail tail, std::uint64_t population, const BigFloatContext& ctx);
Test chi_squared_goodness_of_fit(const std::vector<BigFloat>& observed, const std::vector<BigFloat>& expected, bool expected_are_proportions,
                                 const BigFloatContext& ctx);
Test chi_squared_independence(const std::vector<std::vector<BigFloat>>& table, const BigFloatContext& ctx);
Test slope_t_test(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloat& beta0, Tail tail, const BigFloatContext& ctx);

Interval one_proportion_interval(std::uint64_t successes, std::uint64_t n, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx);
Interval two_proportion_interval(std::uint64_t x1, std::uint64_t n1, std::uint64_t x2, std::uint64_t n2, const BigFloat& confidence,
                                 std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx);
Interval one_mean_z_interval(const Sample& s, const BigFloat& sigma, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx);
Interval one_mean_t_interval(const Sample& s, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx);
Interval two_mean_z_interval(const Sample& a, const BigFloat& sigma1, const Sample& b, const BigFloat& sigma2, const BigFloat& confidence,
                             std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx);
Interval two_mean_t_interval(const Sample& a, const Sample& b, const BigFloat& confidence, bool pooled,
                             std::uint64_t population1, std::uint64_t population2, const BigFloatContext& ctx);
Interval paired_t_interval(const Sample& d, const BigFloat& confidence, std::uint64_t population, const BigFloatContext& ctx);
Interval variance_interval(const Sample& s, const BigFloat& confidence, const BigFloatContext& ctx);
Interval slope_interval(const std::vector<BigFloat>& x, const std::vector<BigFloat>& y, const BigFloat& confidence, const BigFloatContext& ctx);

BigFloat one_mean_z_power(const BigFloat& mu0, const BigFloat& mu_a, const BigFloat& sigma, std::uint64_t n, const BigFloat& alpha, Tail tail,
                          const BigFloatContext& ctx);
BigFloat two_mean_z_power(const BigFloat& delta0, const BigFloat& delta_a, const BigFloat& sigma1, std::uint64_t n1, const BigFloat& sigma2,
                          std::uint64_t n2, const BigFloat& alpha, Tail tail, const BigFloatContext& ctx);
BigFloat one_proportion_z_power(const BigFloat& p0, const BigFloat& p_a, std::uint64_t n, const BigFloat& alpha, Tail tail, const BigFloatContext& ctx);
BigFloat two_proportion_z_power(const BigFloat& p1, const BigFloat& p2, std::uint64_t n1, std::uint64_t n2, const BigFloat& alpha, Tail tail,
                                const BigFloatContext& ctx);
BigFloat one_mean_t_power(const BigFloat& mu0, const BigFloat& mu_a, const BigFloat& sd, std::uint64_t n, const BigFloat& alpha, Tail tail,
                          const BigFloatContext& ctx);
BigFloat two_mean_t_power(const BigFloat& delta0, const BigFloat& delta_a, const BigFloat& sd1, std::uint64_t n1, const BigFloat& sd2, std::uint64_t n2,
                          const BigFloat& alpha, Tail tail, bool pooled, const BigFloatContext& ctx);

std::uint64_t sample_size_for_mean(const BigFloat& margin, const BigFloat& sigma, const BigFloat& confidence);
std::uint64_t sample_size_for_proportion(const BigFloat& margin, const BigFloat& confidence, const BigFloat& p_guess);

} // namespace core
} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_CORE_HPP
