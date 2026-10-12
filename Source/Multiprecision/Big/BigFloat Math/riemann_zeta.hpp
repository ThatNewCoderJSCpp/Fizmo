#ifndef FIZMO_MULTIPRECISION_BIG_RIEMANN_ZETA_HPP
#define FIZMO_MULTIPRECISION_BIG_RIEMANN_ZETA_HPP

#include "gamma.hpp"
#include "htrig.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace math {

namespace ztdetail {

struct zt_val {
    BigFloat v;
    double   lost;                  
};

inline std::size_t zt_bitlen(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

double zt_to_double(const BigFloat& x);

BigFloat zt_one_minus(const BigFloat& x);

void zt_reduce_half(const BigFloat& y, BigFloat& f, bool& flip);

BigFloat zt_sticky(const BigFloat& base, bool neg, std::size_t gap, const BigFloatContext& ctx);

bool zt_safe(const BigFloat& v, std::size_t want, double lost, std::size_t prec);

inline bool zt_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

std::size_t zt_guard0(const BigFloat& s);

std::vector<BigFloat> zt_powers(const BigFloat& s, std::uint64_t M, const BigFloatContext& wc);

void zt_borwein_step(BigUInt& c, std::uint64_t n, std::uint64_t i);

zt_val zt_core(const BigFloat& s, const BigFloat& om, std::size_t want);

class zt_shift {
public:
    zt_shift(const BigFloat& s, std::size_t want);
    zt_val at(std::uint64_t k);

private:
    BigFloat              s_;
    std::size_t           want_;
    std::size_t           wb_;
    std::uint64_t         n_;
    std::uint64_t         k_;
    bool                  started_ = false;
    std::vector<BigFloat> p_;
    std::vector<BigFloat> inv_;
    std::vector<BigFloat> bern_;
};

zt_val zt_chi_low(const BigFloat& s, const BigFloat& om, std::size_t want);

zt_val zt_fe(const BigFloat& s, const BigFloat& om, std::size_t want);

bool zt_neg_int(const BigFloat& s, const BigFloatContext& ctx, BigFloat& out);

} // namespace ztdetail

BigFloat riemann_zeta(const BigFloat& s, const BigFloatContext& ctx);

BigFloat riemann_xi(const BigFloat& s, const BigFloatContext& ctx);

BigFloat riemann_chi(const BigFloat& s, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(riemann_zeta)
FIZMO_MP_TRIG_FORWARD(riemann_xi)
FIZMO_MP_TRIG_FORWARD(riemann_chi)

#define FIZMO_MP_ZETA_ARITH_FORWARD(FN)                                                                 \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>          \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(x), c); }                     \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>          \
    inline BigFloat FN(T x) { return FN(BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_ZETA_ARITH_FORWARD(riemann_zeta)
FIZMO_MP_ZETA_ARITH_FORWARD(riemann_xi)
FIZMO_MP_ZETA_ARITH_FORWARD(riemann_chi)

#undef FIZMO_MP_ZETA_ARITH_FORWARD

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_RIEMANN_ZETA_HPP