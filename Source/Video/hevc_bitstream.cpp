#include "hevc_internal.hpp"

namespace fizmo {
namespace video {
namespace h264_internal {
extern const std::uint8_t k_cabac_range_lps[64][4];
extern const std::uint8_t k_cabac_trans_lps[64];
} // namespace h264_internal

namespace hevc_internal {

#if defined(__GNUC__) || defined(__clang__)
#define FIZMO_HEVC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FIZMO_HEVC_UNLIKELY(x) (x)
#endif // __GNUC__

void Bits::init(const std::uint8_t* data, std::size_t size) noexcept {
    m_data = data;
    m_size = data ? size : 0;
    m_bits = m_size * 8;
    m_pos = 0;
    m_error = false;
    m_trailing = 0;
    std::size_t n = m_size;
    while (n > 0 && m_data[n - 1] == 0) --n;
    if (n > 0) {
        const std::uint8_t last = m_data[n - 1];
        int tz = 0;
        while (!((last >> tz) & 1)) ++tz;
        m_trailing = (n - 1) * 8 + static_cast<std::size_t>(7 - tz);
    }
}

std::uint32_t Bits::u(int n) noexcept {
    if (n <= 0) return 0;
    if (n > 32) n = 32;
    if (m_pos + static_cast<std::size_t>(n) > m_bits) {
        m_error = true;
        m_pos = m_bits;
        return 0;
    }
    const std::size_t byte = m_pos >> 3;
    std::uint64_t w = 0;
    for (int i = 0; i < 8; ++i) {
        const std::size_t at = byte + static_cast<std::size_t>(i);
        w = (w << 8) | (at < m_size ? m_data[at] : 0u);
    }
    w <<= (m_pos & 7);
    m_pos += static_cast<std::size_t>(n);
    return static_cast<std::uint32_t>(w >> (64 - n));
}

std::uint32_t Bits::ue() noexcept {
    int zeros = 0;
    while (true) {
        if (m_pos >= m_bits) {
            m_error = true;
            return 0;
        }
        if (u(1)) break;
        if (++zeros > 31) {
            m_error = true;
            return 0;
        }
    }
    if (zeros == 0) return 0;
    const std::uint64_t v = (static_cast<std::uint64_t>(1) << zeros) - 1 + u(zeros);
    if (v > 0xFFFFFFFEu) {
        m_error = true;
        return 0;
    }
    return static_cast<std::uint32_t>(v);
}

std::int32_t Bits::se() noexcept {
    const std::uint32_t k = ue();
    const std::int64_t v = (k & 1) ? static_cast<std::int64_t>((static_cast<std::uint64_t>(k) + 1) / 2) : -static_cast<std::int64_t>(k / 2);
    return static_cast<std::int32_t>(v);
}

void Bits::skip(std::size_t n) noexcept {
    if (m_pos + n > m_bits) {
        m_error = true;
        m_pos = m_bits;
        return;
    }
    m_pos += n;
}

void unescape_nal(const std::uint8_t* src, std::size_t size, std::vector<std::uint8_t>& out, std::vector<std::size_t>& ep) {
    out.clear();
    ep.clear();
    out.reserve(size + 8);
    int zeros = 0;
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint8_t c = src[i];
        if (zeros >= 2 && c == 3) {
            ep.push_back(out.size());
            zeros = 0;
            continue;
        }
        out.push_back(c);
        zeros = c == 0 ? zeros + 1 : 0;
    }
}

CabacOracle*& cabac_oracle() noexcept {
    static CabacOracle* oracle = nullptr;
    return oracle;
}

void Cabac::init(const std::uint8_t* data, std::size_t size) noexcept {
    if (m_oracle) {
        m_oracle->begin_substream();
        return;
    }
    m_start = data;
    m_ptr = data;
    m_end = data + size;
    m_range = 510;
    m_over = 0;
    m_value = 0;
    for (int i = 0; i < 3; ++i) {
        std::uint32_t b = 0;
        if (m_ptr < m_end) {
            b = *m_ptr++;
        } else {
            ++m_over;
        }
        m_value = (m_value << 8) | b;
    }
    m_bits = 15;
}

void Cabac::refill() noexcept {
    std::uint32_t b = 0;
    if (m_ptr < m_end) {
        b = *m_ptr++;
    } else {
        ++m_over;
    }
    m_value = (m_value << 8) | b;
    m_bits += 8;
}

int Cabac::decode(std::uint8_t& ctx) noexcept {
    if (FIZMO_HEVC_UNLIKELY(m_oracle != nullptr)) return m_oracle->decision(ctx, static_cast<int>(&ctx - m_base));
    const int state = ctx >> 1;
    int mps = ctx & 1;
    const std::uint32_t lps = h264_internal::k_cabac_range_lps[state][(m_range >> 6) & 3];
    m_range -= lps;
    const std::uint32_t scaled = m_range << m_bits;
    if (m_value < scaled) {
        ctx = static_cast<std::uint8_t>(((state < 62 ? state + 1 : 62) << 1) | mps);
        if (m_range < 256) {
            m_range <<= 1;
            if (--m_bits < 0) refill();
        }
        return mps;
    }
    m_value -= scaled;
    const int shift = clz32(lps) - 23;
    m_range = lps << shift;
    const int bin = !mps;
    if (state == 0) mps = !mps;
    ctx = static_cast<std::uint8_t>((h264_internal::k_cabac_trans_lps[state] << 1) | mps);
    m_bits -= shift;
    if (m_bits < 0) refill();
    return bin;
}

int Cabac::bypass() noexcept {
    if (FIZMO_HEVC_UNLIKELY(m_oracle != nullptr)) return m_oracle->bypass();
    if (--m_bits < 0) refill();
    const std::uint32_t scaled = m_range << m_bits;
    if (m_value >= scaled) {
        m_value -= scaled;
        return 1;
    }
    return 0;
}

std::uint32_t Cabac::bypass_bits(int n) noexcept {
    std::uint32_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 1) | static_cast<std::uint32_t>(bypass());
    return v;
}

void Cabac::align_bypass() noexcept {
    if (m_oracle) {
        m_oracle->align_bypass();
        return;
    }
    m_range = 256;
}

int Cabac::terminate(int kind) noexcept {
    if (FIZMO_HEVC_UNLIKELY(m_oracle != nullptr)) return m_oracle->terminate(kind);
    m_range -= 2;
    const std::uint32_t scaled = m_range << m_bits;
    if (m_value >= scaled) return 1;
    if (m_range < 256) {
        m_range <<= 1;
        if (--m_bits < 0) refill();
    }
    return 0;
}

std::size_t Cabac::aligned_position() const noexcept {
    if (m_oracle) return 0;
    const std::size_t read_bits = (static_cast<std::size_t>(m_ptr - m_start) + static_cast<std::size_t>(m_over)) * 8;
    const std::size_t used = read_bits - static_cast<std::size_t>(m_bits);
    return (used + 7) / 8;
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
