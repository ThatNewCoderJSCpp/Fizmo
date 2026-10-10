#ifndef FIZMO_MULTIPRECISION_BIG_STIELTJES_HPP
#define FIZMO_MULTIPRECISION_BIG_STIELTJES_HPP

#include "bernoulli_tangent.hpp"
#include "logarithms.hpp"
#include "euler_mascheroni.hpp"

#include <vector>

namespace fizmo {
namespace multiprecision {
namespace constants {

namespace detail {

struct sg_int {
    BigUInt mag;
    bool    neg;
    sg_int() : mag(BigUInt::zero()), neg(false) {}
};

void sg_add(sg_int& a, const sg_int& b);

sg_int sg_scaled(const sg_int& a, std::uint64_t s, bool flip);

void sg_poly_step(std::vector<sg_int>& c, std::uint64_t m1);

BigFloat sg_poly_eval(const std::vector<sg_int>& c, const BigFloat& L, const BigFloatContext& wc);

BigFloat sg_powi(const BigFloat& b, std::uint64_t e, const BigFloatContext& wc);

static const std::size_t sg_order_cap = 1024;

inline bool sg_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

std::size_t sg_tail_estimate(std::size_t n, std::size_t N, std::size_t w, std::size_t jcap);

BigFloat sg_once(std::size_t n, std::size_t w, std::size_t& cancelled);

} // namespace detail

BigFloat stieltjes(std::size_t n, const BigFloatContext& ctx);

inline BigFloat stieltjes(std::size_t n) { return stieltjes(n, BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_STIELTJES_HPP