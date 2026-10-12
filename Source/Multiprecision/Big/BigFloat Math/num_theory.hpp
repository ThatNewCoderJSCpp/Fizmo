#ifndef FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP
#define FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP

#include "abs_min_max.hpp"

namespace fizmo {
namespace math {

BigFloat fmod(const BigFloat& x, const BigFloat& y);

BigUInt gcd(const BigUInt& a, const BigUInt& b);

BigUInt lcm(const BigUInt& a, const BigUInt& b);

BigFloat trunc(const BigFloat& x);

BigFloat round(const BigFloat& x);

BigFloat floor(const BigFloat& x);

BigFloat ceiling(const BigFloat& x);

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP