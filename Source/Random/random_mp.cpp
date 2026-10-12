#include "fizmo_library.hpp"
#include "random_mp.hpp"

#include <algorithm>
#include <vector>

namespace fizmo {

multiprecision::BigUInt random_biguint_bits(std::size_t bits) {
    if (bits == 0) return multiprecision::BigUInt::zero();
    const std::size_t limbs = (bits + 63) / 64;
    std::vector<std::uint64_t> buf(limbs);
    random_bytes(buf.data(), limbs * sizeof(std::uint64_t));
    const std::size_t top = bits % 64;
    if (top != 0) buf[limbs - 1] &= (std::uint64_t(1) << top) - 1;
    return multiprecision::BigUInt::from_limbs(buf.data(), limbs);
}

multiprecision::BigUInt random_biguint_below(const multiprecision::BigUInt& bound) {
    if (bound.is_undefined() || bound.is_zero()) throw std::domain_error("random_biguint_below: bound must be positive");
    if (bound.is_one()) return multiprecision::BigUInt::zero();
    const multiprecision::BigUInt span = bound - multiprecision::BigUInt::one();
    const std::size_t bits = span.bit_length();

    for (;;) {
        multiprecision::BigUInt v = random_biguint_bits(bits);
        if (v.compare(span) <= 0) return v;
    }
}

multiprecision::BigUInt random_big(const multiprecision::BigUInt& a, const multiprecision::BigUInt& b) {
    if (a.is_undefined() || b.is_undefined()) throw std::domain_error("random_big: undefined bound");
    const bool swap = b.compare(a) < 0;
    const multiprecision::BigUInt& lo = swap ? b : a;
    const multiprecision::BigUInt& hi = swap ? a : b;
    const multiprecision::BigUInt span = hi - lo;
    if (span.is_zero()) return lo;
    const std::size_t bits = span.bit_length();

    for (;;) {
        multiprecision::BigUInt v = random_biguint_bits(bits);
        if (v.compare(span) <= 0) return lo + v;
    }
}

multiprecision::BigInt random_big(const multiprecision::BigInt& a, const multiprecision::BigInt& b) {
    if (!a.is_finite() || !b.is_finite()) throw std::domain_error("random_big: bounds must be finite");
    const bool swap = b < a;
    const multiprecision::BigInt& lo = swap ? b : a;
    const multiprecision::BigInt& hi = swap ? a : b;
    const multiprecision::BigInt span = hi - lo;
    return lo + multiprecision::BigInt(random_big(multiprecision::BigUInt::zero(), span.magnitude()));
}

namespace detail {

namespace {

ExactBinary make_exact(multiprecision::BigInt m, std::int64_t e) {
    ExactBinary r;
    r.mantissa = std::move(m);
    r.exponent = r.mantissa.is_zero() ? 0 : e;
    return r;
}

ExactBinary exact_add(const ExactBinary& x, const ExactBinary& y) {
    if (x.mantissa.is_zero()) return y;
    if (y.mantissa.is_zero()) return x;

    if (x.exponent >= y.exponent) {
        multiprecision::BigInt m = x.mantissa << static_cast<std::size_t>(x.exponent - y.exponent);
        m += y.mantissa;
        return make_exact(std::move(m), y.exponent);
    }

    multiprecision::BigInt m = y.mantissa << static_cast<std::size_t>(y.exponent - x.exponent);
    m += x.mantissa;
    return make_exact(std::move(m), x.exponent);
}

ExactBinary exact_neg(const ExactBinary& x) {
    return make_exact(-x.mantissa, x.exponent);
}

ExactBinary exact_mul(const ExactBinary& x, const ExactBinary& y) {
    return make_exact(x.mantissa * y.mantissa, x.exponent + y.exponent);
}

int exact_sign(const ExactBinary& x) {
    if (x.mantissa.is_zero()) return 0;
    return x.mantissa.is_negative() ? -1 : 1;
}

int exact_compare(const ExactBinary& x, const ExactBinary& y) {
    return exact_sign(exact_add(x, exact_neg(y)));
}

std::int64_t top_exponent(const multiprecision::BigUInt& mag, std::int64_t e) {
    return e + static_cast<std::int64_t>(mag.bit_length()) - 1;
}

bool is_power_of_two(const multiprecision::BigUInt& mag) {
    const std::size_t n = mag.bit_length();
    if (n == 0) return false;
    multiprecision::BigUInt p = multiprecision::BigUInt::one().shifted_left(n - 1);
    return p.compare(mag) == 0;
}

std::int64_t quantum_of(std::int64_t top, const BinaryFormat& fmt) {
    return std::max<std::int64_t>(top - static_cast<std::int64_t>(fmt.precision) + 1, fmt.min_quantum);
}

ExactBinary floor_to_format(const ExactBinary& v, const BinaryFormat& fmt) {
    if (v.mantissa.is_zero()) return v;
    const multiprecision::BigUInt& mag = v.mantissa.magnitude();
    const bool neg = v.mantissa.is_negative();
    const std::int64_t q = quantum_of(top_exponent(mag, v.exponent), fmt);
    if (q <= v.exponent) return v;
    const std::size_t shift = static_cast<std::size_t>(q - v.exponent);
    multiprecision::BigUInt kept = mag.shifted_right(shift);
    const bool exact = kept.shifted_left(shift).compare(mag) == 0;
    if (neg && !exact) kept += multiprecision::BigUInt::one();
    return make_exact(multiprecision::BigInt(std::move(kept), neg), q);
}

bool next_up(const ExactBinary& f, const BinaryFormat& fmt, ExactBinary& out) {
    if (f.mantissa.is_zero()) {
        if (fmt.min_quantum == kUnboundedQuantum) return false;
        out = make_exact(multiprecision::BigInt(1), fmt.min_quantum);
        return true;
    }

    const multiprecision::BigUInt& mag = f.mantissa.magnitude();
    const std::int64_t top = top_exponent(mag, f.exponent);
    std::int64_t q = quantum_of(top, fmt);

    if (f.mantissa.is_negative() && is_power_of_two(mag) && top - static_cast<std::int64_t>(fmt.precision) + 1 > fmt.min_quantum) {
        q -= 1;
    }

    out = exact_add(f, make_exact(multiprecision::BigInt(1), q));
    return true;
}

} // namespace

ExactBinary exact_from(const multiprecision::BigFloat& v) {
    if (v.is_zero()) return make_exact(multiprecision::BigInt(0), 0);
    return make_exact(v.mantissa(), v.exponent());
}

multiprecision::BigFloat exact_to_bigfloat(const ExactBinary& v) {
    if (v.mantissa.is_zero()) return multiprecision::BigFloat(0);
    return multiprecision::BigFloat(v.mantissa, v.exponent);
}

ExactBinary uniform_floor(const ExactBinary& a, const ExactBinary& b, const BinaryFormat& fmt) {
    const ExactBinary width = exact_add(b, exact_neg(a));
    std::size_t k = std::max<std::size_t>(64, fmt.precision + 16);
    multiprecision::BigUInt r = random_biguint_bits(k);

    for (;;) {
        const ExactBinary step = make_exact(width.mantissa, width.exponent - static_cast<std::int64_t>(k));
        const ExactBinary low  = exact_add(a, exact_mul(step, make_exact(multiprecision::BigInt(r), 0)));
        const ExactBinary high = exact_add(low, step);
        const ExactBinary f    = floor_to_format(low, fmt);
        ExactBinary up;
        if (next_up(f, fmt, up) && exact_compare(high, up) <= 0) return f;
        r.shift_left_mutable(32);
        r += multiprecision::BigUInt(static_cast<std::uint64_t>(random_int<std::uint32_t>()));
        k += 32;
    }
}

} // namespace detail

multiprecision::BigFloat random_real(const multiprecision::BigFloat& a, const multiprecision::BigFloat& b, std::size_t precision) {
    if (!a.is_finite() || !b.is_finite()) throw std::domain_error("random_real: bounds must be finite");
    if (a == b) return a;
    const bool swap = b < a;
    const detail::ExactBinary lo = detail::exact_from(swap ? b : a);
    const detail::ExactBinary hi = detail::exact_from(swap ? a : b);
    std::size_t p = std::max<std::size_t>(precision, 2);
    p = std::max(p, lo.mantissa.bit_length());
    p = std::max(p, hi.mantissa.bit_length());
    const detail::BinaryFormat fmt { p, detail::kUnboundedQuantum };
    return detail::exact_to_bigfloat(detail::uniform_floor(lo, hi, fmt));
}

} // namespace fizmo
