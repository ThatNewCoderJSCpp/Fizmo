#ifndef FIZMO_MULTIPRECISION_DECIMAL_CONVERSION_HPP
#define FIZMO_MULTIPRECISION_DECIMAL_CONVERSION_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace fdetail {
namespace decimal {

class bignum {
public:
    std::vector<std::uint32_t> limbs;

    bignum() = default;

    explicit bignum(std::uint64_t v) {
        while (v) { limbs.push_back(static_cast<std::uint32_t>(v & 0xFFFFFFFFull)); v >>= 32; }
    }

    bool is_zero() const noexcept { return limbs.empty(); }

    void trim() noexcept { while (!limbs.empty() && limbs.back() == 0) limbs.pop_back(); }

    long long bit_length() const noexcept;

    bool get_bit(long long i) const noexcept;

    void set_bit(long long i);

    void mul_small(std::uint32_t k);

    void add_small(std::uint32_t k);

    void shl(long long bits);

    static int compare(const bignum& a, const bignum& b) noexcept;

    void add(const bignum& b);

    void sub(const bignum& b);

    static bignum mul(const bignum& a, const bignum& b);

    static bignum pow5(long long n);

    static bignum pow10(long long n) {
        bignum r = pow5(n);
        r.shl(n);
        return r;
    }

    static bignum div_bits(bignum a, const bignum& b, long long qbits, bool& remainder_nonzero);

    std::uint32_t div_digit(const bignum& s) {
        std::uint32_t d = 0;
        while (compare(*this, s) >= 0) { sub(s); ++d; }
        return d;
    }
};

struct digits_result {
    std::string digits;
    long long   exponent10 = 0;
};

 digits_result shortest(const bignum& f, long long e, bool lower_gap_half, bool even);

 digits_result fixed_digits(const bignum& f, long long e, long long count);

 std::string format_fixed(const std::string& digits, long long k);

 std::string format_scientific(const std::string& digits, long long k);

struct parsed_decimal {
    bool      ok = false;
    bool      negative = false;
    bool      is_nan = false;
    bool      is_inf = false;
    bignum    digits;
    long long digit_count = 0;
    long long exponent10 = 0;
};

 parsed_decimal parse(const char* s);

} // namespace decimal
} // namespace fdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_DECIMAL_CONVERSION_HPP
