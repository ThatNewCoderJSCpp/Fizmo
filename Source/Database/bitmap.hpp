#ifndef FIZMO_DATABASE_BITMAP_HPP
#define FIZMO_DATABASE_BITMAP_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif // defined(_MSC_VER)

namespace fizmo {
namespace database {

inline unsigned count_trailing_zeros(std::uint64_t x) noexcept {
#if defined(_MSC_VER)
    unsigned long i;
    _BitScanForward64(&i, x);
    return static_cast<unsigned>(i);
#else
    return static_cast<unsigned>(__builtin_ctzll(x));
#endif // defined(_MSC_VER)
}

inline unsigned popcount64(std::uint64_t x) noexcept {
#if defined(_MSC_VER)
    return static_cast<unsigned>(__popcnt64(x));
#else
    return static_cast<unsigned>(__builtin_popcountll(x));
#endif // defined(_MSC_VER)
}

class Bitmap {
public:
    Bitmap() = default;
    explicit Bitmap(std::size_t bits, bool value = false) { resize(bits, value); }

    void resize(std::size_t bits, bool value = false) {
        const std::size_t old = m_bits;
        m_words.resize((bits + 63) / 64, value ? ~std::uint64_t(0) : 0);

        if (value && old < bits && old % 64) m_words[old / 64] |= ~std::uint64_t(0) << (old % 64);
        m_bits = bits;
        trim();
    }

    std::size_t size() const noexcept { return m_bits; }
    bool test(std::size_t i) const noexcept { return (m_words[i >> 6] >> (i & 63)) & 1u; }
    void set(std::size_t i) noexcept { m_words[i >> 6] |= std::uint64_t(1) << (i & 63); }
    void reset(std::size_t i) noexcept { m_words[i >> 6] &= ~(std::uint64_t(1) << (i & 63)); }
    void assign(std::size_t i, bool v) noexcept { if (v) set(i); else reset(i); }
    void clear() noexcept { for (auto& w : m_words) w = 0; }
    void fill() noexcept { for (auto& w : m_words) w = ~std::uint64_t(0); trim(); }

    std::size_t count() const noexcept {
        std::size_t n = 0;
        for (auto w : m_words) n += popcount64(w);
        return n;
    }

    bool any() const noexcept {
        for (auto w : m_words) if (w) return true;
        return false;
    }

    Bitmap& operator&=(const Bitmap& o) noexcept {
        const std::size_t n = m_words.size() < o.m_words.size() ? m_words.size() : o.m_words.size();
        for (std::size_t i = 0; i < n; ++i) m_words[i] &= o.m_words[i];
        for (std::size_t i = n; i < m_words.size(); ++i) m_words[i] = 0;
        return *this;
    }

    Bitmap& operator|=(const Bitmap& o) {
        if (o.m_bits > m_bits) resize(o.m_bits);
        for (std::size_t i = 0; i < o.m_words.size(); ++i) m_words[i] |= o.m_words[i];
        return *this;
    }

    void and_not(const Bitmap& o) noexcept {
        const std::size_t n = m_words.size() < o.m_words.size() ? m_words.size() : o.m_words.size();
        for (std::size_t i = 0; i < n; ++i) m_words[i] &= ~o.m_words[i];
    }

    template <typename Fn>
    void for_each(Fn&& fn) const {
        for (std::size_t w = 0; w < m_words.size(); ++w) {
            std::uint64_t bits = m_words[w];

            while (bits) {
                const unsigned b = count_trailing_zeros(bits);
                fn(static_cast<std::uint32_t>(w * 64 + b));
                bits &= bits - 1;
            }
        }
    }

    template <typename Fn>
    bool for_each_until(Fn&& fn) const {
        for (std::size_t w = 0; w < m_words.size(); ++w) {
            std::uint64_t bits = m_words[w];

            while (bits) {
                const unsigned b = count_trailing_zeros(bits);
                if (!fn(static_cast<std::uint32_t>(w * 64 + b))) return false;
                bits &= bits - 1;
            }
        }

        return true;
    }

    std::vector<std::uint32_t> to_vector() const {
        std::vector<std::uint32_t> out;
        out.reserve(count());
        for_each([&](std::uint32_t i) { out.push_back(i); });
        return out;
    }

    const std::vector<std::uint64_t>& words() const noexcept { return m_words; }
    std::vector<std::uint64_t>& words() noexcept { return m_words; }

private:
    std::vector<std::uint64_t> m_words;
    std::size_t                m_bits = 0;

    void trim() noexcept {
        if (m_bits % 64 && !m_words.empty()) m_words.back() &= (std::uint64_t(1) << (m_bits % 64)) - 1;
    }
};

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_BITMAP_HPP
