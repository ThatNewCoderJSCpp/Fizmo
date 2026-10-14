#ifndef FIZMO_MULTIPRECISION_BIG_TRACKED_HPP
#define FIZMO_MULTIPRECISION_BIG_TRACKED_HPP

#include "../BigComplex Math/special_detail.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace fizmo {
namespace math {
namespace tracked {

using BF  = BigFloat;
using BFC = BigFloatContext;

constexpr double ninf = -std::numeric_limits<double>::infinity();
constexpr double pinf = std::numeric_limits<double>::infinity();

struct RV {
    BF     v;
    double e = ninf;

    RV() : v(BF::zero()) {}
    RV(BF value, double err = ninf) : v(std::move(value)), e(err) {}

    bool ok() const { return e != pinf && !v.is_unordered(); }
};

inline RV failed() { return RV(BF::zero(), pinf); }

inline RV exact(const BF& x) { return RV(x, ninf); }

BFC ctx(std::size_t w);

double l2(const BF& x);
double l2(const RV& x);
double lsum(double a, double b);
double lsum(double a, double b, double c);
double lsum(double a, double b, double c, double d);
double rel(const RV& x);
double to_double(const BF& x);

BF exact_add(const BF& a, const BF& b);
BF exact_sub(const BF& a, const BF& b);

RV rounded(const BF& v, std::size_t w);
RV add(const RV& a, const RV& b, std::size_t w);
RV sub(const RV& a, const RV& b, std::size_t w);
RV mul(const RV& a, const RV& b, std::size_t w);
RV div(const RV& a, const RV& b, std::size_t w);
RV neg(const RV& a);
RV abs(const RV& a);
RV scale2(const RV& a, std::int64_t k);
RV sqr(const RV& a, std::size_t w);
RV sqrt(const RV& a, std::size_t w);
RV exp(const RV& a, std::size_t w);
RV ln(const RV& a, std::size_t w);
RV ln1p(const RV& a, std::size_t w);
RV expm1(const RV& a, std::size_t w);
RV pow(const RV& x, const RV& y, std::size_t w);
RV recip(const RV& a, std::size_t w);
RV one_minus(const RV& a, std::size_t w);
RV pi(std::size_t w);
RV log_gamma(const RV& a, std::size_t w);
RV log_beta(const RV& a, const RV& b, std::size_t w);
RV erfc(const RV& z, std::size_t w);
RV cospi(const RV& x, std::size_t w);
RV through(const RV& a, const BF& value, double l2_slope, std::size_t w);

bool certain_positive(const RV& a);
bool certain_negative(const RV& a);
bool separated(const RV& a, const RV& b);

BF finish(const RV& r, const BFC& c, bool last, bool& done);

double log2_gamma_density(const BF& a, const BF& x);
double log2_beta_density(const BF& x, const BF& a, const BF& b);

RV gamma_pq_exact(const BF& a, const BF& x, std::size_t w, bool upper);
RV gamma_pq(const BF& a, const RV& x, std::size_t w, bool upper);
RV beta_i_exact(const BF& x, const BF& a, const BF& b, std::size_t w, bool complement);
RV beta_i(const RV& x, const BF& a, const BF& b, std::size_t w, bool complement);

template <class Eval>
BF drive(const BFC& c, Eval&& eval, std::size_t guard = 24, std::size_t cap = 8192) {
    for (;;) {
        const std::size_t w    = BFC::clamp_precision(c.precision + guard);
        const bool        last = guard >= cap || w >= BFC::max_prec / 4;
        const RV          r    = eval(w);
        bool              done = false;
        const BF          out  = finish(r, c, last, done);
        if (done || last) return out;
        guard *= 2;
    }
}

} // namespace tracked
} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_TRACKED_HPP
