#ifndef FIZMO_MULTIPRECISION_BIG_ALGEBRAIC_CONSTS_HPP
#define FIZMO_MULTIPRECISION_BIG_ALGEBRAIC_CONSTS_HPP

#include "trig.hpp"           

namespace fizmo {
namespace multiprecision {
namespace constants {

namespace acdetail {

enum class ac_kind : std::uint8_t { ratio, mean, beraha };

 BigFloat ac_once(ac_kind k, std::uint64_t n, std::size_t wp, std::size_t& lost);

struct const_cache {
    BigFloat    v;
    std::size_t err   = 0;
    std::size_t good  = 0;
    bool        valid = false;
};

 BigFloat ac_drive(ac_kind k, std::uint64_t n, const BigFloatContext& ctx, const_cache* c);

static const std::uint64_t ac_ratio_cap = 4000000000ull;
static const std::uint64_t ac_mean_cap  = 4000000000000000000ull;

inline const_cache& ac_silver_cache()    { static thread_local const_cache c; return c; }
inline const_cache& ac_bronze_cache()    { static thread_local const_cache c; return c; }
inline const_cache& ac_silver_c_cache()  { static thread_local const_cache c; return c; }

} // namespace acdetail

 BigFloat metallic_ratio(std::uint64_t n, const BigFloatContext& ctx);

 BigFloat inv_metallic_ratio(std::uint64_t n, const BigFloatContext& ctx);

 BigFloat metallic_mean(std::uint64_t n, const BigFloatContext& ctx);

 BigFloat inv_metallic_mean(std::uint64_t n, const BigFloatContext& ctx);

 BigFloat beraha(std::uint64_t n, const BigFloatContext& ctx);

inline BigFloat silver_ratio(const BigFloatContext& ctx) {
    return acdetail::ac_drive(acdetail::ac_kind::ratio, 2, ctx, &acdetail::ac_silver_cache());
}

inline BigFloat bronze_ratio(const BigFloatContext& ctx) {
    return acdetail::ac_drive(acdetail::ac_kind::ratio, 3, ctx, &acdetail::ac_bronze_cache());
}

 BigFloat silver_constant(const BigFloatContext& ctx);

inline BigFloat metallic_ratio(std::uint64_t n)     { return metallic_ratio(n, BigFloatContext::current()); }
inline BigFloat inv_metallic_ratio(std::uint64_t n) { return inv_metallic_ratio(n, BigFloatContext::current()); }
inline BigFloat metallic_mean(std::uint64_t n)      { return metallic_mean(n, BigFloatContext::current()); }
inline BigFloat inv_metallic_mean(std::uint64_t n)  { return inv_metallic_mean(n, BigFloatContext::current()); }
inline BigFloat beraha(std::uint64_t n)             { return beraha(n, BigFloatContext::current()); }
inline BigFloat silver_ratio()                      { return silver_ratio(BigFloatContext::current()); }
inline BigFloat bronze_ratio()                      { return bronze_ratio(BigFloatContext::current()); }
inline BigFloat silver_constant()                   { return silver_constant(BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_ALGEBRAIC_CONSTS_HPP