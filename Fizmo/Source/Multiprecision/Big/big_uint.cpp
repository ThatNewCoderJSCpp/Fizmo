#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace budetail {

std::uint64_t div_wide(std::uint64_t hi, std::uint64_t lo, std::uint64_t d, std::uint64_t& rem) noexcept {
    if (d <= mask32) {
        std::uint64_t cur = (hi << 32) | (lo >> 32);
        const std::uint64_t qh = cur / d; cur %= d;
        cur = (cur << 32) | (lo & mask32);
        const std::uint64_t ql = cur / d; rem = cur % d;
        return (qh << 32) | ql;
    }

    const unsigned s = static_cast<unsigned>(clz64(d));
    d <<= s;
    if (s != 0) { hi = (hi << s) | (lo >> (64 - s)); lo <<= s; }
    const std::uint64_t dh = d >> 32, dl = d & mask32;
    const std::uint64_t un1 = lo >> 32, un0 = lo & mask32;
    std::uint64_t q1 = hi / dh;
    std::uint64_t rh = hi - q1 * dh;

    while (q1 > mask32 || q1 * dl > ((rh << 32) | un1)) {
        --q1; rh += dh;
        if (rh > mask32) break;
    }

    const std::uint64_t u21 = ((hi << 32) | un1) - q1 * d;
    std::uint64_t q0 = u21 / dh;
    rh = u21 - q0 * dh;

    while (q0 > mask32 || q0 * dl > ((rh << 32) | un0)) {
        --q0; rh += dh;
        if (rh > mask32) break;
    }

    rem = (((u21 << 32) | un0) - q0 * d) >> s;
    return (q1 << 32) | q0;
}

} // namespace budetail
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {

auto BigUInt::grow_to(std::size_t n) -> bool {
        if (n > max_limbs) { set_undefined(); return false; }
        if (m_data.size() < n) m_data.resize(n);
        return true;
    }

auto BigUInt::push_limb(std::uint64_t v) -> bool {
        if (m_data.size() >= max_limbs) { set_undefined(); return false; }
        m_data.push_back(v);
        return true;
    }

auto BigUInt::operator=(const limb_store& v) -> BigUInt& {
        if (v.empty() || v.size() > max_limbs) { set_undefined(); return *this; }
        m_data = v;
        trim();
        return *this;
    }

auto BigUInt::operator=(limb_store&& v) noexcept -> BigUInt& {
        if (v.empty() || v.size() > max_limbs) { set_undefined(); return *this; }
        m_data = std::move(v);
        trim();
        return *this;
    }

auto BigUInt::from_limbs(const std::uint64_t* p, std::size_t n) -> BigUInt {
        BigUInt r;
        if (n == 0 || n > max_limbs) { r.set_undefined(); return r; }
        if (!r.m_data.resize_uninit(n)) { r.set_undefined(); return r; }
        for (std::size_t i = 0; i < n; ++i) r.m_data[i] = p[i];
        r.trim();
        return r;
    }

auto BigUInt::max() -> BigUInt {
        BigUInt r;

        if (!r.m_data.resize_uninit(max_limbs)) {
            r.set_undefined();
            return r;
        }

        for (std::size_t i = 0; i < max_limbs; ++i) r.m_data[i] = ~std::uint64_t(0); // fill with 0xFFFFFFFFFFFFFFFF
        return r;
    }

auto BigUInt::compare(const BigUInt& o) const noexcept -> int {
        const std::size_t a = m_data.size();
        const std::size_t b = o.m_data.size();
        if (a != b) return (a < b) ? -1 : 1;

        for (std::size_t i = a; i > 0; --i) {
            const std::uint64_t x = m_data[i - 1];
            const std::uint64_t y = o.m_data[i - 1];
            if (x != y) return (x < y) ? -1 : 1;
        }

        return 0;
    }

auto BigUInt::equals(const BigUInt& o) const noexcept -> bool {
        if (m_data.size() != o.m_data.size()) return false;
        for (std::size_t i = 0; i < m_data.size(); ++i) { if (m_data[i] != o.m_data[i]) return false; }
        return true;
    }

auto BigUInt::bit_length() const noexcept -> std::size_t {
        if (m_data.empty()) return 0;
        const std::size_t top = m_data.size() - 1;
        return top * 64 + static_cast<std::size_t>(64 - clz64(m_data[top]));
    }

auto BigUInt::get_bit(std::size_t i) const noexcept -> bool {
        const std::size_t w = i / 64;
        return w < m_data.size() && ((m_data[w] >> (i % 64)) & std::uint64_t(1)) != 0;
    }

auto BigUInt::set_bit(std::size_t i, bool b) -> void {
        const std::size_t w = i / 64;

        if (!b) {
            if (w >= m_data.size()) return;
            m_data[w] &= ~(std::uint64_t(1) << (i % 64));
            if (w + 1 == m_data.size()) trim();
            return;
        }

        if (!grow_to(w + 1)) return;
        m_data[w] |= (std::uint64_t(1) << (i % 64));
    }

auto BigUInt::clear_bit(std::size_t i) noexcept -> void {
        const std::size_t w = i / 64;
        if (w >= m_data.size()) return;
        m_data[w] &= ~(std::uint64_t(1) << (i % 64));
        if (w + 1 == m_data.size()) trim();
    }

auto BigUInt::flip_bit(std::size_t i) -> bool {
        const std::size_t w = i / 64;
        const std::uint64_t mask = std::uint64_t(1) << (i % 64);
        if (w >= m_data.size()) { if (!grow_to(w + 1)) return false; m_data[w] = mask; return true; }
        m_data[w] ^= mask;
        const bool now = (m_data[w] & mask) != 0;
        if (!now && w + 1 == m_data.size()) trim();
        return now;
    }

auto BigUInt::shifted_limbs_mutable(std::size_t n) -> BigUInt& {
        if (n == 0 || m_data.empty() || is_zero()) return *this;
        const std::size_t old = m_data.size();
        if (!grow_to(old + n)) return *this;
        for (std::size_t i = old; i > 0; --i) m_data[i - 1 + n] = m_data[i - 1];
        for (std::size_t i = 0; i < n; ++i) m_data[i] = 0;
        return *this;
    }

auto BigUInt::shifted_limbs(std::size_t n) const & -> BigUInt {
        if (n == 0 || m_data.empty() || is_zero()) return *this;
        if (m_data.size() + n > max_limbs) return undefined();
        BigUInt r;
        r.m_data.resize(m_data.size() + n);
        for (std::size_t i = 0; i < m_data.size(); ++i) r.m_data[i + n] = m_data[i];
        return r;
    }

auto BigUInt::add_shifted_mutable(const BigUInt& src, std::size_t offset) -> BigUInt& {
        if (m_data.empty() || src.m_data.empty()) { set_undefined(); return *this; }
        if (&src == this) { const BigUInt tmp(src); return add_shifted_mutable(tmp, offset); }
        if (src.is_zero()) return *this;
        const std::size_t ns = src.m_data.size();
        if (!grow_to(offset + ns)) return *this;
        std::uint64_t carry = 0;

        for (std::size_t i = 0; i < ns; ++i) {
            const std::size_t w = i + offset;
            const std::uint64_t a  = m_data[w];
            const std::uint64_t s  = a + src.m_data[i];
            const std::uint64_t c1 = (s < a) ? 1u : 0u;
            const std::uint64_t t  = s + carry;
            const std::uint64_t c2 = (t < s) ? 1u : 0u;
            m_data[w] = t;
            carry = c1 | c2;
        }

        for (std::size_t w = offset + ns; carry != 0; ++w) {
            if (w == m_data.size()) { push_limb(carry); break; }
            const std::uint64_t s = m_data[w] + carry;
            carry = (s < carry) ? 1u : 0u;
            m_data[w] = s;
        }

        return *this;
    }

auto BigUInt::get_exponent_base10() const noexcept -> long long {
        if (m_data.empty() || is_zero()) return no_bit;

        if (m_data.size() == 1) {
            std::uint64_t v = m_data[0];
            long long e = 0;
            while (v >= 10) { v /= 10; ++e; }
            return e;
        }

        BigUInt t(*this);
        long long e = 0;
        constexpr std::uint64_t p19 = 10000000000000000000ull;

        while (t.limb_count() > 1) {
            t.div_small_mutable(p19);
            if (t.is_zero()) break;
            e += 19;
        }

        std::uint64_t v = t.limb(0);
        while (v >= 10) { v /= 10; ++e; }
        return e;
    }

auto BigUInt::count_leading_zeros() const noexcept -> long long {
        if (m_data.empty()) return no_bit;
        return static_cast<long long>(m_data.size() * 64 - bit_length());
    }

auto BigUInt::count_leading_zeros(std::size_t width) const noexcept -> long long {
        if (m_data.empty()) return no_bit;
        const std::size_t len = bit_length();
        if (len > width) return no_bit;
        return static_cast<long long>(width - len);
    }

auto BigUInt::count_trailing_zeros() const noexcept -> long long {
        if (m_data.empty()) return no_bit;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t w = m_data[i];
            if (w != 0) return static_cast<long long>(i * 64) + ctz64(w);
        }

        return no_bit;
    }

auto BigUInt::add_small_mutable(std::uint64_t v) -> BigUInt& {
        if (m_data.empty() || v == 0) return *this;
        std::uint64_t carry = v;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n && carry != 0; ++i) {
            const std::uint64_t s = m_data[i] + carry;
            carry = (s < carry) ? 1u : 0u;
            m_data[i] = s;
        }

        if (carry != 0) push_limb(carry);
        return *this;
    }

auto BigUInt::sub_small_mutable(std::uint64_t v) noexcept -> BigUInt& {
        if (m_data.empty() || v == 0) return *this;
        std::uint64_t borrow = v;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n && borrow != 0; ++i) {
            const std::uint64_t a = m_data[i];
            m_data[i] = a - borrow;
            borrow = (a < borrow) ? 1u : 0u;
        }

        if (borrow != 0) { set_undefined(); return *this; }
        trim();
        return *this;
    }

auto BigUInt::mul_small_mutable(std::uint64_t v) -> BigUInt& {
        if (m_data.empty() || v == 1) return *this;
        if (v == 0) { set_zero(); return *this; }
        std::uint64_t carry = 0;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n; ++i) {
            std::uint64_t hi, lo;
            budetail::mul_wide(m_data[i], v, hi, lo);
            const std::uint64_t s = lo + carry;
            carry = hi + ((s < carry) ? 1u : 0u);
            m_data[i] = s;
        }

        if (carry != 0) push_limb(carry);
        return *this;
    }

auto BigUInt::divmod_small_mutable(std::uint64_t d) noexcept -> std::uint64_t {
        if (m_data.empty()) return no_remainder;
        if (d == 0) { set_undefined(); return no_remainder; }
        if (d == 1) return 0;
        std::uint64_t rem = 0;
        for (std::size_t i = m_data.size(); i > 0; --i) { m_data[i - 1] = budetail::div_wide(rem, m_data[i - 1], d, rem); }
        trim();
        return rem;
    }

auto BigUInt::mod_small_mutable(std::uint64_t d) noexcept -> BigUInt& {
        const std::uint64_t r = divmod_small_mutable(d);
        if (m_data.empty()) return *this;
        set_limb_unchecked(r);
        return *this;
    }

auto BigUInt::mod_small(std::uint64_t d) const noexcept -> std::uint64_t {
        if (m_data.empty() || d == 0) return no_remainder;
        if (d == 1) return 0;
        std::uint64_t rem = 0;
        for (std::size_t i = m_data.size(); i > 0; --i) budetail::div_wide(rem, m_data[i - 1], d, rem);
        return rem;
    }

auto BigUInt::add_mutable(const BigUInt& o) -> BigUInt& {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) return shift_left_mutable(1);
        const std::size_t bn   = o.m_data.size();
        const std::size_t maxn = (m_data.size() > bn) ? m_data.size() : bn;
        if (!grow_to(maxn)) return *this;
        std::uint64_t carry = mdetail::add_n(m_data.data(), m_data.data(), o.m_data.data(), bn);
        if (bn < maxn) carry = mdetail::add_1(m_data.data() + bn, m_data.data() + bn, maxn - bn, carry);
        if (carry != 0) push_limb(carry);
        return *this;
    }

auto BigUInt::sub_mutable(const BigUInt& o) noexcept -> BigUInt& {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) { set_zero(); return *this; }
        if (compare(o) < 0) { set_undefined(); return *this; }
        const std::size_t an = m_data.size();
        const std::size_t bn = o.m_data.size();
        std::uint64_t borrow = mdetail::sub_n(m_data.data(), m_data.data(), o.m_data.data(), bn);
        if (bn < an) borrow = mdetail::sub_1(m_data.data() + bn, m_data.data() + bn, an - bn, borrow);
        (void)borrow;
        trim();
        return *this;
    }

auto BigUInt::shift_left_mutable(std::size_t n) -> BigUInt& {
        if (m_data.empty()) return *this;
        if (n == 0 || is_zero()) return *this;
        const std::size_t limbs = n / 64;
        if (limbs >= max_limbs) { set_undefined(); return *this; }
        const unsigned      bits = static_cast<unsigned>(n % 64);
        const std::size_t   old  = m_data.size();
        const std::uint64_t top  = m_data[old - 1];
        const std::size_t   extra = (bits != 0 && (top >> (64 - bits)) != 0) ? 1u : 0u;
        const std::size_t   nsz  = old + limbs + extra;
        if (!grow_to(nsz)) return *this;

        if (bits == 0) {
            for (std::size_t j = nsz; j > limbs; --j) m_data[j - 1] = m_data[j - 1 - limbs];
        } else {
            const unsigned rs = 64 - bits;

            for (std::size_t j = nsz; j > limbs; --j) {
                const std::size_t   k  = j - 1 - limbs;
                const std::uint64_t hi = (k < old) ? (m_data[k] << bits) : std::uint64_t(0);
                const std::uint64_t lo = (k >= 1)  ? (m_data[k - 1] >> rs) : std::uint64_t(0);
                m_data[j - 1] = hi | lo;
            }
        }

        for (std::size_t j = 0; j < limbs; ++j) m_data[j] = 0;
        trim();
        return *this;
    }

auto BigUInt::shift_right_mutable(std::size_t n) noexcept -> BigUInt& {
        if (m_data.empty()) return *this;
        if (n == 0 || is_zero()) return *this;
        const std::size_t limbs = n / 64;
        const std::size_t old   = m_data.size();
        if (limbs >= old) { set_limb_unchecked(0); return *this; }
        const unsigned    bits = static_cast<unsigned>(n % 64);
        const std::size_t nsz  = old - limbs;

        if (bits == 0) {
            for (std::size_t j = 0; j < nsz; ++j) m_data[j] = m_data[j + limbs];
        } else {
            const unsigned ls = 64 - bits;

            for (std::size_t j = 0; j < nsz; ++j) {
                const std::size_t   k  = j + limbs;
                const std::uint64_t lo = m_data[k] >> bits;
                const std::uint64_t hi = (k + 1 < old) ? (m_data[k + 1] << ls) : std::uint64_t(0);
                m_data[j] = lo | hi;
            }
        }

        m_data.set_size_unchecked(nsz);
        trim();
        return *this;
    }

auto BigUInt::and_mutable(const BigUInt& o) noexcept -> BigUInt& {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) return *this;
        const std::size_t n = (m_data.size() < o.m_data.size()) ? m_data.size() : o.m_data.size();
        for (std::size_t i = 0; i < n; ++i) m_data[i] &= o.m_data[i];
        m_data.set_size_unchecked(n);
        trim();
        return *this;
    }

auto BigUInt::or_mutable(const BigUInt& o) -> BigUInt& {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) return *this;
        const std::size_t on = o.m_data.size();
        if (!grow_to(on)) return *this;
        for (std::size_t i = 0; i < on; ++i) m_data[i] |= o.m_data[i];
        return *this;
    }

auto BigUInt::xor_mutable(const BigUInt& o) -> BigUInt& {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) { set_limb_unchecked(0); return *this; }
        const std::size_t on = o.m_data.size();
        if (!grow_to(on)) return *this;
        for (std::size_t i = 0; i < on; ++i) m_data[i] ^= o.m_data[i];
        trim();
        return *this;
    }

auto BigUInt::andn_mutable(const BigUInt& o) noexcept -> BigUInt& {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) { set_limb_unchecked(0); return *this; }
        const std::size_t n  = m_data.size();
        const std::size_t on = o.m_data.size();
        for (std::size_t i = 0; i < n && i < on; ++i) m_data[i] &= ~o.m_data[i];
        trim();
        return *this;
    }

auto BigUInt::complement_mutable() noexcept -> BigUInt& {
        if (m_data.empty()) return *this;
        for (std::size_t i = 0; i < m_data.size(); ++i) m_data[i] = ~m_data[i];
        trim();
        return *this;
    }

auto BigUInt::complement_bits_mutable(std::size_t width) -> BigUInt& {
        if (m_data.empty()) return *this;
        if (width == 0)         { set_limb_unchecked(0); return *this; }
        if (width > max_bits)   { set_undefined();       return *this; }
        const std::size_t nsz = (width + 63) / 64;
        if (!grow_to(nsz)) return *this;
        for (std::size_t i = 0; i < nsz; ++i) m_data[i] = ~m_data[i];
        const unsigned tail = static_cast<unsigned>(width % 64);
        if (tail != 0) m_data[nsz - 1] &= (std::uint64_t(1) << tail) - 1;
        m_data.set_size_unchecked(nsz);
        trim();
        return *this;
    }

} // namespace multiprecision
} // namespace fizmo
