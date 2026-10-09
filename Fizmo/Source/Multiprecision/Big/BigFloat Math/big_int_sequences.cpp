#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace sequences {
namespace seqdetail {

std::uint64_t seq_cap(std::uint64_t milli_bits_per_index) {
    const std::uint64_t avail = (BigUInt::max_bits > 256) ? (BigUInt::max_bits - 256) : 1;
    return avail * 1000ull / milli_bits_per_index;
}

bool seq_extend(std::vector<BigUInt>& c, std::uint64_t m, std::uint64_t mul) {
    if (m >= seq_table_cap) return false;

    if (c.empty()) {
        c.reserve(128);
        c.push_back(BigUInt::zero());                 
        c.push_back(BigUInt::one());                  
    }

    if (m < c.size()) return true;
    std::uint64_t target = (c.size() < 128) ? 128 : c.size();
    while (target <= m) target *= 2;
    if (target > seq_table_cap) target = seq_table_cap;
    c.reserve(static_cast<std::size_t>(target));

    while (c.size() <= static_cast<std::size_t>(target)) {
        const std::size_t s = c.size();
        BigUInt t = c[s - 1];
        if (mul != 1) t.mul_small_mutable(mul);       
        t.add_mutable(c[s - 2]);
        c.push_back(std::move(t));
    }

    return true;
}

void fib_double(std::uint64_t n, BigUInt& F, BigUInt& L) {
    if (n == 0) { F = BigUInt::zero(); L = BigUInt(static_cast<std::uint64_t>(2)); return; }
    F = BigUInt::one();                              
    L = BigUInt::one();                              
    bool k_even = false;                             

    for (int i = seq_msb(n) - 1; i >= 0; --i) {
        BigUInt nF = F * L;                          
        BigUInt nL = L * L;                          
        if (k_even) nL.sub_small_mutable(2); else nL.add_small_mutable(2);
        F = std::move(nF);
        L = std::move(nL);
        k_even = true;

        if ((n >> i) & 1ull) {
            BigUInt t = F;
            t.add_mutable(L);
            t.shift_right_mutable(1);                
            L = F;
            L.shift_left_mutable(1);
            L.add_mutable(t);                        
            F = std::move(t);
            k_even = false;
        }
    }
}

void pell_double(std::uint64_t n, BigUInt& P, BigUInt& H) {
    if (n == 0) { P = BigUInt::zero(); H = BigUInt::one(); return; }
    P = BigUInt::one();                               
    H = BigUInt::one();                               
    bool k_even = false;

    for (int i = seq_msb(n) - 1; i >= 0; --i) {
        BigUInt nP = P * H;
        nP.shift_left_mutable(1);                     
        BigUInt nH = H * H;
        nH.shift_left_mutable(1);                     
        if (k_even) nH.sub_small_mutable(1); else nH.add_small_mutable(1);
        P = std::move(nP);
        H = std::move(nH);
        k_even = true;

        if ((n >> i) & 1ull) {
            BigUInt t = P;
            t.add_mutable(H);                         
            H.add_mutable(P);
            H.add_mutable(P);                         
            P = std::move(t);
            k_even = false;
        }
    }
}

} // namespace seqdetail
} // namespace sequences
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace sequences {

bool fibonacci_pair(std::uint64_t n, BigUInt& F, BigUInt& L) {
    if (n > seqdetail::fib_n_cap()) { F = BigUInt::undefined(); L = BigUInt::undefined(); return false; }
    std::vector<BigUInt>& c = seqdetail::fib_cache();

    if (seqdetail::seq_extend(c, n + 1, 1)) {
        F = c[static_cast<std::size_t>(n)];
        L = c[static_cast<std::size_t>(n) + 1];
        L.shift_left_mutable(1);
        L.sub_mutable(c[static_cast<std::size_t>(n)]);         
        return true;
    }

    seqdetail::pair_memo& m = seqdetail::fib_memo();
    if (m.get(n, F, L)) return true;
    seqdetail::fib_double(n, F, L);
    m.put(n, F, L);
    return true;
}

bool pell_pair(std::uint64_t n, BigUInt& P, BigUInt& H) {
    if (n > seqdetail::pell_n_cap()) { P = BigUInt::undefined(); H = BigUInt::undefined(); return false; }
    std::vector<BigUInt>& c = seqdetail::pell_cache();

    if (seqdetail::seq_extend(c, n + 1, 2)) {
        P = c[static_cast<std::size_t>(n)];
        H = c[static_cast<std::size_t>(n) + 1];
        H.sub_mutable(c[static_cast<std::size_t>(n)]);          
        return true;
    }

    seqdetail::pair_memo& m = seqdetail::pell_memo();
    if (m.get(n, P, H)) return true;
    seqdetail::pell_double(n, P, H);
    m.put(n, P, H);
    return true;
}

BigUInt fibonacci(std::uint64_t n) {
    if (n > seqdetail::fib_n_cap()) return BigUInt::undefined();
    std::vector<BigUInt>& c = seqdetail::fib_cache();
    if (seqdetail::seq_extend(c, n, 1)) return c[static_cast<std::size_t>(n)];
    BigUInt F, L;
    fibonacci_pair(n, F, L);
    return F;
}

BigUInt pell(std::uint64_t n) {
    if (n > seqdetail::pell_n_cap()) return BigUInt::undefined();
    std::vector<BigUInt>& c = seqdetail::pell_cache();
    if (seqdetail::seq_extend(c, n, 2)) return c[static_cast<std::size_t>(n)];
    BigUInt P, H;
    pell_pair(n, P, H);
    return P;
}

} // namespace sequences
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace sequences {
namespace seqdetail {

BigInt seq_signed(BigUInt m, bool neg) {
    if (m.is_undefined()) return BigInt::undefined();
    const bool negative = neg && !m.is_zero();
    return BigInt::from_magnitude(std::move(m), negative);
}

} // namespace seqdetail
} // namespace sequences
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace sequences {

BigInt fibonacci(std::int64_t n) {
    const std::uint64_t k = seqdetail::seq_abs(n);
    return seqdetail::seq_signed(fibonacci(k), n < 0 && (k % 2 == 0));
}

BigInt lucas(std::int64_t n) {
    const std::uint64_t k = seqdetail::seq_abs(n);
    return seqdetail::seq_signed(lucas(k), n < 0 && (k % 2 == 1));
}

BigInt pell(std::int64_t n) {
    const std::uint64_t k = seqdetail::seq_abs(n);
    return seqdetail::seq_signed(pell(k), n < 0 && (k % 2 == 0));
}

BigInt pell_companion(std::int64_t n) {
    const std::uint64_t k = seqdetail::seq_abs(n);
    return seqdetail::seq_signed(pell_companion(k), n < 0 && (k % 2 == 1));
}

} // namespace sequences
} // namespace multiprecision
} // namespace fizmo
