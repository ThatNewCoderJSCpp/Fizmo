#ifndef FIZMO_MULTIPRECISION_BIG_QUERIES_HPP
#define FIZMO_MULTIPRECISION_BIG_QUERIES_HPP

#include "../big_float.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

inline BigInt integer_part(const BigFloat& x) { return x.get_integer_part(); }
inline BigInt integer_part(const BigUInt& x)  { return x.is_undefined() ? BigInt::undefined() : BigInt(x); }
inline BigInt integer_part(const BigInt& x)   { return x; }

inline BigFloat fractional_part(const BigFloat& x) { return x.get_fractional_part(); }
inline BigFloat fractional_part(const BigUInt& x)  { return x.is_undefined() ? BigFloat::undefined() : BigFloat::zero(); }

BigFloat fractional_part(const BigInt& x);

inline bool is_integer(const BigUInt& x)  { return !x.is_undefined(); }
inline bool is_integer(const BigInt& x)   { return !(x.is_nan() || x.is_undefined()); }
inline bool is_integer(const BigFloat& x) { return x.is_integer(); }

inline bool is_even(const BigUInt& x) { return x.is_even(); }
inline bool is_even(const BigInt& x)  { return x.is_even(); }

inline bool is_even(const BigFloat& x) { 
    if (!x.is_integer()) return false;
    return x.get_integer_part().is_even();
}

inline bool is_odd(const BigUInt& x) { return x.is_odd(); }
inline bool is_odd(const BigInt& x)  { return x.is_odd(); }

inline bool is_odd(const BigFloat& x) {
    if (!x.is_integer()) return false;
    return x.get_integer_part().is_odd();
}

inline bool is_negative(const BigUInt& x)  { return false; }
inline bool is_negative(const BigInt& x)   { return x.is_negative(); } 
inline bool is_negative(const BigFloat& x) { return x.is_negative(); }

inline bool is_positive(const BigUInt& x)  { return !(x.is_undefined() || x.is_zero()); }
inline bool is_positive(const BigInt& x)   { return x.is_positive(); } 
inline bool is_positive(const BigFloat& x) { return x.is_positive(); }

inline bool is_nonnegative(const BigUInt& x)  { return !x.is_undefined(); }
inline bool is_nonnegative(const BigInt& x)   { return x.is_positive() || x.is_zero(); } 
inline bool is_nonnegative(const BigFloat& x) { return x.is_positive() || x.is_zero(); }

inline bool is_nonpositive(const BigUInt& x)  { return x.is_zero(); }
inline bool is_nonpositive(const BigInt& x)   { return x.is_negative() || x.is_zero(); } 
inline bool is_nonpositive(const BigFloat& x) { return x.is_negative() || x.is_zero(); }

inline bool is_zero(const BigUInt& x)  { return x.is_zero(); }
inline bool is_zero(const BigInt& x)   { return x.is_zero(); }
inline bool is_zero(const BigFloat& x) { return x.is_zero(); }

inline bool is_one(const BigUInt& x) { return x.is_one(); }
inline bool is_one(const BigInt& x)  { return x.is_one(); }

inline bool is_one(const BigFloat& x) { 
    if (!x.is_integer()) { return false; }
    return x.get_integer_part().is_one();
}

inline bool is_undefined(const BigUInt& x)  { return x.is_undefined(); }
inline bool is_undefined(const BigInt& x)   { return x.is_undefined(); }
inline bool is_undefined(const BigFloat& x) { return x.is_undefined(); }

inline bool is_nan(const BigUInt& x)  { return false; }
inline bool is_nan(const BigInt& x)   { return x.is_nan(); }
inline bool is_nan(const BigFloat& x) { return x.is_nan(); }

inline bool is_infinite(const BigUInt& x)  { return false; }
inline bool is_infinite(const BigInt& x)   { return false; }
inline bool is_infinite(const BigFloat& x) { return x.is_infinite(); }

inline bool is_finite(const BigUInt& x)  { return !x.is_undefined(); }
inline bool is_finite(const BigInt& x)   { return !(x.is_nan() || x.is_undefined()); }
inline bool is_finite(const BigFloat& x) { return x.is_finite(); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_QUERIES_HPP