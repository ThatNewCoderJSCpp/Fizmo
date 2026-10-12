#ifndef FIZMO_STATISTICS_INTERNAL_HPP
#define FIZMO_STATISTICS_INTERNAL_HPP

#include "core.hpp"

namespace fizmo {
namespace statistics {
namespace core {
namespace ddetail {

using RV = math::tracked::RV;

RV phi(const RV& z, bool upper, std::size_t w);
RV student(const BigFloat& nu, const RV& t, bool upper, std::size_t w);
RV cdf_rv(const Dist& d, const RV& x, bool upper, std::size_t w);
RV continuous_quantile(const Dist& d, const BigFloat& p, bool upper, std::size_t w);
BigFloat nct_cdf_bf(const BigFloat& t, const BigFloat& nu, const BigFloat& delta, std::size_t w);

} // namespace ddetail
} // namespace core
} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_INTERNAL_HPP
