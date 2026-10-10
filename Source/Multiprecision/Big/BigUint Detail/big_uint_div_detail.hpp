#ifndef FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_DIV_HPP
#define FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_DIV_HPP

#include <cstdint>
#include <cstddef>
#include <utility>

#include "../big_uint.hpp"
#include "../../limb_kernels.hpp"

namespace fizmo {
namespace multiprecision {
namespace mdetail {

inline void divmod_knuth(
    std::uint64_t* qp, std::uint64_t* rp,
    const std::uint64_t* up, std::size_t un,
    const std::uint64_t* vp, std::size_t vn
) {
    const unsigned s = static_cast<unsigned>(64 - bitlen64(vp[vn - 1]));
    budetail::limb_vec v_buf, u_buf;
    if (!v_buf.resize_uninit(vn))     return;
    if (!u_buf.resize_uninit(un + 1)) return;
    std::uint64_t* v = v_buf.data();
    std::uint64_t* u = u_buf.data();

    if (s == 0) {
        copy_limbs(v, vp, vn);
        copy_limbs(u, up, un);
        u[un] = 0;
    } else {
        lshift_limbs(v, vp, vn, s);
        u[un] = lshift_limbs(u, up, un, s);
    }

    const std::size_t m   = un - vn;
    const std::uint64_t vhi = v[vn - 1];
    const std::uint64_t vlo = v[vn - 2];

    for (std::size_t jj = 0; jj <= m; ++jj) {
        const std::size_t j  = m - jj;            
        const std::uint64_t u2 = u[j + vn];
        const std::uint64_t u1 = u[j + vn - 1];
        const std::uint64_t u0 = u[j + vn - 2];
        std::uint64_t qhat, rhat;
        bool rhat_overflowed;

        if (u2 == vhi) {
            qhat = ~std::uint64_t(0);              
            rhat = u1 + vhi;
            rhat_overflowed = (rhat < u1);
        } else {
            qhat = budetail::div_wide(u2, u1, vhi, rhat);
            rhat_overflowed = false;
        }

        while (!rhat_overflowed) {
            std::uint64_t phi, plo;
            mul_wide64(qhat, vlo, phi, plo);
            const bool too_big = (phi > rhat) || (phi == rhat && plo > u0);
            if (!too_big) break;
            --qhat;
            const std::uint64_t next_rhat = rhat + vhi;
            rhat_overflowed = (next_rhat < rhat);   
            rhat = next_rhat;
        }

        const std::uint64_t borrow = submul_1(u + j, v, vn, qhat);
        const std::uint64_t top    = u[j + vn];
        const bool negative = top < borrow;
        u[j + vn] = top - borrow;

        if (negative) {
            --qhat;
            const std::uint64_t carry = add_n(u + j, u + j, v, vn);
            u[j + vn] += carry;
        }

        qp[j] = qhat;
    }

    if (s == 0) copy_limbs(rp, u, vn);
    else        rshift_limbs(rp, u, vn, s);
}

inline BigUInt divmod(const BigUInt& a, const BigUInt& b, BigUInt& rem) {
    if (a.is_undefined() || b.is_undefined() || b.is_zero()) {
        rem = BigUInt::undefined();
        return BigUInt::undefined();
    }

    if (a.is_zero() || a.compare(b) < 0) { rem = a; return BigUInt::zero(); }
    if (a.compare(b) == 0)               { rem = BigUInt::zero(); return BigUInt::one(); }
    const std::size_t bn = b.limb_count();

    if (bn == 1) {
        BigUInt q(a);
        const std::uint64_t r = q.divmod_small_mutable(b.limb(0));
        rem = BigUInt(r);
        return q;
    }

    const std::size_t an = a.limb_count();
    budetail::limb_vec q, r;
    if (!q.resize_uninit(an - bn + 1)) { rem = BigUInt::undefined(); return BigUInt::undefined(); }
    if (!r.resize_uninit(bn))          { rem = BigUInt::undefined(); return BigUInt::undefined(); }
    divmod_knuth(q.data(), r.data(), a.limbs(), an, b.limbs(), bn);
    rem = BigUInt(std::move(r));
    return BigUInt(std::move(q));
}

inline BigUInt div(const BigUInt& a, const BigUInt& b) { BigUInt r; return divmod(a, b, r); }
inline BigUInt mod(const BigUInt& a, const BigUInt& b) { BigUInt r; divmod(a, b, r); return r; }

} // namespace mdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_DIV_HPP