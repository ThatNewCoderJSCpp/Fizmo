#include "fizmo_library.hpp"

namespace fizmo {
namespace math {

BigFloat fractional_part(const BigInt& x) { 
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_indeterminate()) return BigFloat::indeterminate();
    if (x.is_nan()) return BigFloat::nan();
    if (x.is_indeterminate()) return BigFloat::indeterminate();
    return BigFloat::zero(); 
}

} // namespace math
} // namespace fizmo
