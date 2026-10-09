#ifndef FIZMO_MULTIPRECISION_BIG_INTEGER_SEQUENCES_HPP
#define FIZMO_MULTIPRECISION_BIG_INTEGER_SEQUENCES_HPP

#include "../big_float.hpp"

#include <vector>
#include <cstdint>

namespace fizmo {
namespace multiprecision {
namespace sequences {

namespace seqdetail {

inline int seq_msb(std::uint64_t v) {                 
    int i = 63;
    while (((v >> i) & 1ull) == 0ull) --i;
    return i;
}

 std::uint64_t seq_cap(std::uint64_t milli_bits_per_index);

inline std::uint64_t fib_n_cap()  { static const std::uint64_t c = seq_cap(695);  return c; }
inline std::uint64_t pell_n_cap() { static const std::uint64_t c = seq_cap(1272); return c; }

static const std::uint64_t seq_table_cap = 4096;

inline std::vector<BigUInt>& fib_cache()  { static thread_local std::vector<BigUInt> c; return c; }
inline std::vector<BigUInt>& pell_cache() { static thread_local std::vector<BigUInt> c; return c; }

 bool seq_extend(std::vector<BigUInt>& c, std::uint64_t m, std::uint64_t mul);

struct pair_memo {
    std::uint64_t n[4];
    BigUInt       a[4], b[4];
    bool          used[4];
    std::size_t   next;
    pair_memo() : next(0) { for (int i = 0; i < 4; ++i) { n[i] = 0; used[i] = false; } }

    bool get(std::uint64_t k, BigUInt& x, BigUInt& y) const {
        for (int i = 0; i < 4; ++i) if (used[i] && n[i] == k) { x = a[i]; y = b[i]; return true; }
        return false;
    }

    void put(std::uint64_t k, const BigUInt& x, const BigUInt& y) {
        n[next] = k; a[next] = x; b[next] = y; used[next] = true;
        next = (next + 1) & 3u;
    }
};

inline pair_memo& fib_memo()  { static thread_local pair_memo m; return m; }
inline pair_memo& pell_memo() { static thread_local pair_memo m; return m; }

 void fib_double(std::uint64_t n, BigUInt& F, BigUInt& L);

 void pell_double(std::uint64_t n, BigUInt& P, BigUInt& H);

} // namespace seqdetail

 bool fibonacci_pair(std::uint64_t n, BigUInt& F, BigUInt& L);

 bool pell_pair(std::uint64_t n, BigUInt& P, BigUInt& H);

 BigUInt fibonacci(std::uint64_t n);

inline BigUInt lucas(std::uint64_t n) {
    BigUInt F, L;
    fibonacci_pair(n, F, L);
    return L;
}

 BigUInt pell(std::uint64_t n);

inline BigUInt pell_companion(std::uint64_t n) {
    BigUInt P, H;
    pell_pair(n, P, H);
    return H;
}

namespace seqdetail {

 BigInt seq_signed(BigUInt m, bool neg);

inline std::uint64_t seq_abs(std::int64_t n) {
    return (n < 0) ? (static_cast<std::uint64_t>(-(n + 1)) + 1ull) : static_cast<std::uint64_t>(n);
}

} // namespace seqdetail

 BigInt fibonacci(std::int64_t n);

 BigInt lucas(std::int64_t n);

 BigInt pell(std::int64_t n);

 BigInt pell_companion(std::int64_t n);

} // namespace sequences
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_INTEGER_SEQUENCES_HPP