#ifndef FIZMO_STATISTICS_DISTRIBUTIONS_HPP
#define FIZMO_STATISTICS_DISTRIBUTIONS_HPP

#include "core.hpp"

namespace fizmo {
namespace statistics {

template <class T>
class Distribution {
protected:
    core::Dist m_dist;

    explicit Distribution(core::Dist d) : m_dist(std::move(d)) {}

    template <class Fn>
    static T run(Fn&& fn) { return sdetail::compute<T>(std::forward<Fn>(fn)); }

    static BigFloat b(const T& v) { return sdetail::big(v); }

public:
    using value_type = T;

    virtual ~Distribution() = default;

    bool valid() const { return core::valid(m_dist); }
    bool is_discrete() const { return core::discrete(m_dist); }
    const core::Dist& core_distribution() const { return m_dist; }

    T mean() const               { return run([&](const BigFloatContext& c) { return core::mean(m_dist, c); }); }
    T variance() const           { return run([&](const BigFloatContext& c) { return core::variance(m_dist, c); }); }
    T standard_deviation() const { return run([&](const BigFloatContext& c) { return core::standard_deviation(m_dist, c); }); }

    T quantile(const T& p) const       { return run([&](const BigFloatContext& c) { return core::quantile(m_dist, b(p), false, c); }); }
    T upper_quantile(const T& p) const { return run([&](const BigFloatContext& c) { return core::quantile(m_dist, b(p), true, c); }); }
    T inverse_cdf(const T& p) const    { return quantile(p); }
    T median() const                   { return quantile(sdetail::from_big<T>(BigFloat::one().scaled_pow2(-1))); }
};

template <class T>
class ContinuousDistribution : public Distribution<T> {
protected:
    using Distribution<T>::m_dist;
    using Distribution<T>::run;
    using Distribution<T>::b;

    explicit ContinuousDistribution(core::Dist d) : Distribution<T>(std::move(d)) {}

public:
    T pdf(const T& x) const     { return run([&](const BigFloatContext& c) { return core::pdf(m_dist, b(x), c); }); }
    T log_pdf(const T& x) const { return run([&](const BigFloatContext& c) { return core::log_pdf(m_dist, b(x), c); }); }
    T cdf(const T& x) const     { return run([&](const BigFloatContext& c) { return core::cdf(m_dist, b(x), false, c); }); }
    T sf(const T& x) const      { return run([&](const BigFloatContext& c) { return core::cdf(m_dist, b(x), true, c); }); }

    T below(const T& x) const { return cdf(x); }
    T above(const T& x) const { return sf(x); }

    T between(const T& lo, const T& hi) const { return run([&](const BigFloatContext& c) { return core::between(m_dist, b(lo), b(hi), c); }); }
    T outside(const T& lo, const T& hi) const { return run([&](const BigFloatContext& c) { return core::outside(m_dist, b(lo), b(hi), c); }); }
};

template <class T>
class DiscreteDistribution : public Distribution<T> {
protected:
    using Distribution<T>::m_dist;
    using Distribution<T>::run;

    explicit DiscreteDistribution(core::Dist d) : Distribution<T>(std::move(d)) {}

    static BigFloat k_of(std::int64_t k) { return BigFloat(k); }

public:
    T pmf(std::int64_t k) const     { return run([&](const BigFloatContext& c) { return core::pdf(m_dist, k_of(k), c); }); }
    T log_pmf(std::int64_t k) const { return run([&](const BigFloatContext& c) { return core::log_pdf(m_dist, k_of(k), c); }); }
    T exactly(std::int64_t k) const { return pmf(k); }

    T cdf(std::int64_t k) const          { return run([&](const BigFloatContext& c) { return core::cdf(m_dist, k_of(k), false, c); }); }
    T sf(std::int64_t k) const           { return run([&](const BigFloatContext& c) { return core::cdf(m_dist, k_of(k), true, c); }); }
    T at_most(std::int64_t k) const      { return cdf(k); }
    T less_than(std::int64_t k) const    { return cdf(k - 1); }
    T at_least(std::int64_t k) const     { return sf(k - 1); }
    T greater_than(std::int64_t k) const { return sf(k); }

    T between(std::int64_t lo, std::int64_t hi) const {
        return run([&](const BigFloatContext& c) { return core::between(m_dist, k_of(lo), k_of(hi), c); });
    }

    T outside(std::int64_t lo, std::int64_t hi) const {
        return run([&](const BigFloatContext& c) { return core::outside(m_dist, k_of(lo), k_of(hi), c); });
    }
};

template <class T = double>
class Normal : public ContinuousDistribution<T> {
public:
    Normal() : ContinuousDistribution<T>(core::Dist(core::Kind::normal, BigFloat::zero(), BigFloat::one())) {}
    Normal(const T& mean, const T& standard_deviation)
        : ContinuousDistribution<T>(core::Dist(core::Kind::normal, sdetail::big(mean), sdetail::big(standard_deviation))) {}

    T z_score(const T& x) const {
        return sdetail::compute<T>([&](const BigFloatContext& c) { return core::z_score(sdetail::big(x), this->m_dist.a, this->m_dist.b, c); });
    }

    T value_at(const T& z) const {
        return sdetail::compute<T>([&](const BigFloatContext& c) {
            const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
            return BigFloat::add(this->m_dist.a, BigFloat::mul(sdetail::big(z), this->m_dist.b, exact), c);
        });
    }
};

template <class T = double>
class StudentT : public ContinuousDistribution<T> {
public:
    explicit StudentT(const T& degrees_of_freedom) : ContinuousDistribution<T>(core::Dist(core::Kind::student_t, sdetail::big(degrees_of_freedom))) {}
    T degrees_of_freedom() const { return sdetail::from_big<T>(this->m_dist.a); }
};

template <class T = double>
class ChiSquared : public ContinuousDistribution<T> {
public:
    explicit ChiSquared(const T& degrees_of_freedom) : ContinuousDistribution<T>(core::Dist(core::Kind::chi_squared, sdetail::big(degrees_of_freedom))) {}
    T degrees_of_freedom() const { return sdetail::from_big<T>(this->m_dist.a); }
};

template <class T = double>
class FisherF : public ContinuousDistribution<T> {
public:
    FisherF(const T& numerator_df, const T& denominator_df)
        : ContinuousDistribution<T>(core::Dist(core::Kind::fisher_f, sdetail::big(numerator_df), sdetail::big(denominator_df))) {}
};

template <class T = double>
class Exponential : public ContinuousDistribution<T> {
public:
    explicit Exponential(const T& rate) : ContinuousDistribution<T>(core::Dist(core::Kind::exponential, sdetail::big(rate))) {}
};

template <class T = double>
class Uniform : public ContinuousDistribution<T> {
public:
    Uniform() : ContinuousDistribution<T>(core::Dist(core::Kind::uniform, BigFloat::zero(), BigFloat::one())) {}
    Uniform(const T& lo, const T& hi) : ContinuousDistribution<T>(core::Dist(core::Kind::uniform, sdetail::big(lo), sdetail::big(hi))) {}
};

template <class T = double>
class Gamma : public ContinuousDistribution<T> {
public:
    Gamma(const T& shape, const T& scale)
        : ContinuousDistribution<T>(core::Dist(core::Kind::gamma, sdetail::big(shape), sdetail::big(scale))) {}
};

template <class T = double>
class Beta : public ContinuousDistribution<T> {
public:
    Beta(const T& alpha, const T& beta)
        : ContinuousDistribution<T>(core::Dist(core::Kind::beta, sdetail::big(alpha), sdetail::big(beta))) {}
};

template <class T = double>
class NoncentralT : public ContinuousDistribution<T> {
public:
    NoncentralT(const T& degrees_of_freedom, const T& noncentrality)
        : ContinuousDistribution<T>(core::Dist(core::Kind::noncentral_t, sdetail::big(degrees_of_freedom), sdetail::big(noncentrality))) {}
};

template <class T = double>
class Binomial : public DiscreteDistribution<T> {
public:
    Binomial(std::uint64_t trials, const T& probability)
        : DiscreteDistribution<T>(core::Dist(core::Kind::binomial, BigFloat(trials), sdetail::big(probability))) {}
};

template <class T = double>
class Geometric : public DiscreteDistribution<T> {
public:
    explicit Geometric(const T& probability) : DiscreteDistribution<T>(core::Dist(core::Kind::geometric, sdetail::big(probability))) {}
};

template <class T = double>
class Poisson : public DiscreteDistribution<T> {
public:
    explicit Poisson(const T& rate) : DiscreteDistribution<T>(core::Dist(core::Kind::poisson, sdetail::big(rate))) {}
};

template <class T = double>
class Hypergeometric : public DiscreteDistribution<T> {
public:
    Hypergeometric(std::uint64_t population, std::uint64_t successes, std::uint64_t draws)
        : DiscreteDistribution<T>(core::Dist(core::Kind::hypergeometric, BigFloat(population), BigFloat(successes), BigFloat(draws))) {}
};

template <class T = double>
class NegativeBinomial : public DiscreteDistribution<T> {
public:
    NegativeBinomial(const T& successes, const T& probability)
        : DiscreteDistribution<T>(core::Dist(core::Kind::negative_binomial, sdetail::big(successes), sdetail::big(probability))) {}
};

} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_DISTRIBUTIONS_HPP
