#include "opus_internal.hpp"

namespace fizmo {
namespace audio {
namespace opus_detail {

int ilog(std::uint32_t x) noexcept {
    int n = 0;
    while (x) {
        ++n;
        x >>= 1;
    }
    return n;
}

std::uint32_t RangeDecoder::front_byte() noexcept {
    return m_front < m_storage ? m_data[m_front++] : 0u;
}

std::uint32_t RangeDecoder::back_byte() noexcept {
    if (m_back < m_storage) {
        ++m_back;
        return m_data[m_storage - m_back];
    }
    return 0u;
}

void RangeDecoder::normalize() noexcept {
    while (m_rng <= (1u << 23)) {
        m_total += 8;
        m_rng <<= 8;
        const std::uint32_t b = front_byte();
        const std::uint32_t sym = ((m_rem << 8) | b) >> 1 & 0xFFu;
        m_rem = b;
        m_val = ((m_val << 8) + (255u - sym)) & 0x7FFFFFFFu;
    }
}

void RangeDecoder::init(const std::uint8_t* data, std::uint32_t size) noexcept {
    m_data = data;
    m_storage = data ? size : 0;
    m_front = 0;
    m_back = 0;
    m_window = 0;
    m_window_bits = 0;
    m_error = false;
    m_ext = 1;
    const std::uint32_t b0 = front_byte();
    m_rng = 128;
    m_val = 127 - (b0 >> 1);
    m_rem = b0;
    m_total = 9;
    normalize();
}

std::uint32_t RangeDecoder::decode(std::uint32_t ft) noexcept {
    if (ft == 0) ft = 1;
    m_ext = m_rng / ft;
    if (m_ext == 0) m_ext = 1;
    const std::uint32_t q = m_val / m_ext + 1;
    return ft - (q < ft ? q : ft);
}

std::uint32_t RangeDecoder::decode_bin(unsigned bits) noexcept {
    const std::uint32_t ft = 1u << bits;
    m_ext = m_rng >> bits;
    if (m_ext == 0) m_ext = 1;
    const std::uint32_t q = m_val / m_ext + 1;
    return ft - (q < ft ? q : ft);
}

void RangeDecoder::update(std::uint32_t fl, std::uint32_t fh, std::uint32_t ft) noexcept {
    const std::uint32_t s = m_ext * (ft - fh);
    m_val -= s;
    m_rng = fl > 0 ? m_ext * (fh - fl) : m_rng - s;
    normalize();
}

bool RangeDecoder::bit_logp(unsigned logp) noexcept {
    const std::uint32_t r = m_rng >> logp;
    const bool one = m_val < r;
    if (one) {
        m_rng = r;
    } else {
        m_val -= r;
        m_rng -= r;
    }
    normalize();
    return one;
}

int RangeDecoder::pdf(const std::uint8_t* freqs, int count, unsigned ftb) noexcept {
    const std::uint32_t ft = 1u << ftb;
    const std::uint32_t fs = decode_bin(ftb);
    std::uint32_t fl = 0;
    for (int k = 0; k < count; ++k) {
        const std::uint32_t fh = fl + freqs[k];
        if (fs < fh) {
            update(fl, fh, ft);
            return k;
        }
        fl = fh;
    }
    int last = count - 1;
    while (last > 0 && freqs[last] == 0) --last;
    std::uint32_t lo = 0;
    for (int k = 0; k < last; ++k) lo += freqs[k];
    update(lo, ft, ft);
    m_error = true;
    return last;
}

int RangeDecoder::icdf(const std::uint8_t* table, unsigned ftb) noexcept {
    const std::uint32_t ft = 1u << ftb;
    const std::uint32_t fs = decode_bin(ftb);
    std::uint32_t fl = 0;
    for (int k = 0;; ++k) {
        const std::uint32_t fh = ft - table[k];
        if (fs < fh) {
            update(fl, fh, ft);
            return k;
        }
        fl = fh;
    }
}

std::uint32_t RangeDecoder::uint(std::uint32_t ft) noexcept {
    if (ft <= 1) return 0;
    const int ftb = ilog(ft - 1);
    if (ftb > 8) {
        const int shift = ftb - 8;
        const std::uint32_t top = ((ft - 1) >> shift) + 1;
        const std::uint32_t s = decode(top);
        update(s, s + 1, top);
        const std::uint32_t t = (s << shift) | bits(static_cast<unsigned>(shift));
        if (t >= ft) {
            m_error = true;
            return ft - 1;
        }
        return t;
    }
    const std::uint32_t s = decode(ft);
    update(s, s + 1, ft);
    return s;
}

std::uint32_t RangeDecoder::bits(unsigned count) noexcept {
    if (count == 0) return 0;
    while (m_window_bits < static_cast<int>(count)) {
        m_window |= back_byte() << m_window_bits;
        m_window_bits += 8;
    }
    const std::uint32_t mask = count >= 32 ? 0xFFFFFFFFu : ((1u << count) - 1u);
    const std::uint32_t v = m_window & mask;
    m_window = count >= 32 ? 0 : (m_window >> count);
    m_window_bits -= static_cast<int>(count);
    m_total += static_cast<int>(count);
    return v;
}

int RangeDecoder::tell() const noexcept {
    return m_total - ilog(m_rng);
}

std::uint32_t RangeDecoder::tell_frac() const noexcept {
    int lg = ilog(m_rng);
    std::uint32_t r = m_rng >> (lg - 16);
    for (int i = 0; i < 3; ++i) {
        r = (r * r) >> 15;
        const int bit = static_cast<int>(r >> 16);
        lg = 2 * lg + bit;
        if (bit) r >>= 1;
    }
    return static_cast<std::uint32_t>(m_total * 8 - lg);
}

void RangeDecoder::shrink(std::uint32_t bytes) noexcept {
    m_storage = bytes < m_storage ? m_storage - bytes : 0;
    if (m_front > m_storage) m_front = m_storage;
}

void RangeDecoder::skip_to_end() noexcept {
    m_total += static_cast<int>(m_storage) * 8 - tell();
}

} // namespace opus_detail
} // namespace audio
} // namespace fizmo
