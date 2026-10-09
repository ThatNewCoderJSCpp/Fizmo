#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat min(const BigFloat& a, const BigFloat& b) {
    const BigFloat::ordering c = BigFloat::compare(a, b);

    if (c == BigFloat::ordering::unordered) {
        if (a.is_undefined() || b.is_undefined()) return BigFloat::undefined();
        return BigFloat::nan();
    }

    return c == BigFloat::ordering::greater ? b : a;
}

BigFloat max(const BigFloat& a, const BigFloat& b) {
    const BigFloat::ordering c = BigFloat::compare(a, b);

    if (c == BigFloat::ordering::unordered) {
        if (a.is_undefined() || b.is_undefined()) return BigFloat::undefined();
        return BigFloat::nan();
    }

    return c == BigFloat::ordering::less ? b : a;
}

BigFloat min(std::initializer_list<BigFloat> values) {
    if (values.size() == 0) return BigFloat::undefined();
    auto it = values.begin();
    BigFloat result = *it++;
    for (; it != values.end(); ++it) result = min(result, *it);
    return result;
}

BigFloat max(std::initializer_list<BigFloat> values) {
    if (values.size() == 0) return BigFloat::undefined();
    auto it = values.begin();
    BigFloat result = *it++;
    for (; it != values.end(); ++it) result = max(result, *it);
    return result;
}

BigFloat clamp(
    const BigFloat& x,
    const BigFloat& lo,
    const BigFloat& hi
) {
    const BigFloat::ordering bounds = BigFloat::compare(lo, hi);

    if (bounds == BigFloat::ordering::unordered) {
        if (lo.is_undefined() || hi.is_undefined()) return BigFloat::undefined();
        return BigFloat::nan();
    }

    if (bounds == BigFloat::ordering::greater) return BigFloat::undefined();
    const BigFloat::ordering lower = BigFloat::compare(x, lo);

    if (lower == BigFloat::ordering::unordered) {
        if (x.is_undefined()) return BigFloat::undefined();
        return BigFloat::nan();
    }

    if (lower == BigFloat::ordering::less) return lo;
    const BigFloat::ordering upper = BigFloat::compare(x, hi);

    if (upper == BigFloat::ordering::unordered) {
        if (x.is_undefined()) return BigFloat::undefined();
        return BigFloat::nan();
    }

    if (upper == BigFloat::ordering::greater) return hi;
    return x;
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
