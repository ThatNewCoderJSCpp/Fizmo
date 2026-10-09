#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat fractional_part(const BigInt& x) { 
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_nan()) return BigFloat::nan();
    return BigFloat::zero(); 
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
