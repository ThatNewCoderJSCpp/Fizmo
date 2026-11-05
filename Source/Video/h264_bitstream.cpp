#include "h264_internal.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace video {
namespace h264_internal {

void BitReader::init(const std::uint8_t* data, std::size_t size) noexcept {
    m_data = data;
    m_size_bits = size * 8;
    m_pos = 0;
    m_error = false;
    m_trailing = 0;
    std::size_t i = size;
    while (i > 0 && data[i - 1] == 0) --i;
    if (i > 0) {
        std::uint8_t b = data[i - 1];
        int tz = 0;
        while (!(b & (1u << tz))) ++tz;
        m_trailing = (i - 1) * 8 + static_cast<std::size_t>(7 - tz);
    }
}

std::uint32_t BitReader::peek32() const noexcept {
    if (m_pos > m_size_bits) return 0;
    const std::uint8_t* p = m_data + (m_pos >> 3);
    std::uint64_t v = (static_cast<std::uint64_t>(p[0]) << 32) | (static_cast<std::uint64_t>(p[1]) << 24) |
                      (static_cast<std::uint64_t>(p[2]) << 16) | (static_cast<std::uint64_t>(p[3]) << 8) |
                      static_cast<std::uint64_t>(p[4]);
    return static_cast<std::uint32_t>(v >> (8 - (m_pos & 7)));
}

std::uint32_t BitReader::u(int n) noexcept {
    if (n <= 0) return 0;
    std::uint32_t v = peek32();
    m_pos += static_cast<std::size_t>(n);
    if (n >= 32) return v;
    return v >> (32 - n);
}

std::uint32_t BitReader::ue() noexcept {
    std::uint32_t v = peek32();
    if (v == 0) {
        m_error = true;
        m_pos += 32;
        return 0;
    }
    int lz = count_leading_zeros32(v);
    if (lz < 16) {
        m_pos += static_cast<std::size_t>(2 * lz + 1);
        return (v >> (31 - 2 * lz)) - 1;
    }
    m_pos += static_cast<std::size_t>(lz);
    return u(lz + 1) - 1;
}

std::int32_t BitReader::se() noexcept {
    std::uint32_t k = ue();
    if (k & 1) return static_cast<std::int32_t>((k >> 1) + 1);
    return -static_cast<std::int32_t>(k >> 1);
}

void Vlc::build(const std::uint8_t* lengths, const std::uint8_t* codes, const std::int16_t* symbols, int count) {
    const int first_size = 1 << k_bits;
    m_table.assign(static_cast<std::size_t>(first_size), VlcEntry{-1, 0});
    int sub_bits[1 << k_bits];
    for (int i = 0; i < first_size; ++i) sub_bits[i] = 0;
    for (int i = 0; i < count; ++i) {
        int len = lengths[i];
        if (len <= k_bits) continue;
        int prefix = codes[i] >> (len - k_bits);
        sub_bits[prefix] = std::max(sub_bits[prefix], len - k_bits);
    }
    for (int p = 0; p < first_size; ++p) {
        if (sub_bits[p] == 0) continue;
        std::size_t offset = m_table.size();
        m_table.resize(offset + (static_cast<std::size_t>(1) << sub_bits[p]), VlcEntry{-1, 0});
        m_table[static_cast<std::size_t>(p)].symbol = static_cast<std::int16_t>(offset);
        m_table[static_cast<std::size_t>(p)].length = static_cast<std::int8_t>(-sub_bits[p]);
    }
    for (int i = 0; i < count; ++i) {
        int len = lengths[i];
        if (len == 0) continue;
        int code = codes[i];
        if (len <= k_bits) {
            int start = code << (k_bits - len);
            int n = 1 << (k_bits - len);
            for (int j = 0; j < n; ++j) {
                m_table[static_cast<std::size_t>(start + j)] = VlcEntry{symbols[i], static_cast<std::int8_t>(len)};
            }
        } else {
            int prefix = code >> (len - k_bits);
            int sb = sub_bits[prefix];
            std::size_t offset = static_cast<std::size_t>(m_table[static_cast<std::size_t>(prefix)].symbol);
            int rest = len - k_bits;
            int low = code & ((1 << rest) - 1);
            int start = low << (sb - rest);
            int n = 1 << (sb - rest);
            for (int j = 0; j < n; ++j) {
                m_table[offset + static_cast<std::size_t>(start + j)] = VlcEntry{symbols[i], static_cast<std::int8_t>(rest)};
            }
        }
    }
}

int Vlc::read(BitReader& br) const noexcept {
    std::uint32_t v = br.peek32();
    const VlcEntry& e = m_table[v >> (32 - k_bits)];
    if (e.length > 0) {
        br.skip(static_cast<std::size_t>(e.length));
        return e.symbol;
    }
    if (e.length == 0) {
        br.set_error();
        return -1;
    }
    int sb = -e.length;
    std::size_t idx = static_cast<std::size_t>(e.symbol) + ((v << k_bits) >> (32 - sb));
    const VlcEntry& s = m_table[idx];
    if (s.length <= 0) {
        br.set_error();
        return -1;
    }
    br.skip(static_cast<std::size_t>(k_bits + s.length));
    return s.symbol;
}

void Cabac::init(const std::uint8_t* data, std::size_t size) noexcept {
    m_data = data;
    m_size = size;
    m_pos = 0;
    m_over = 0;
    m_value = 0;
    for (int i = 0; i < 7; ++i) {
        std::uint8_t b = 0;
        if (m_pos < m_size) b = m_data[m_pos];
        ++m_pos;
        m_value = (m_value << 8) | b;
    }
    if (m_pos > m_size) m_over = static_cast<int>(m_pos - m_size) > 6 ? 3 : 0;
    m_bits = 47;
    m_range = 510;
}

void Cabac::refill() noexcept {
    while (m_bits <= 40) {
        std::uint8_t b = 0;
        if (m_pos < m_size) {
            b = m_data[m_pos];
        } else if (m_pos > m_size + 8) {
            ++m_over;
        }
        ++m_pos;
        m_value = (m_value << 8) | b;
        m_bits += 8;
    }
}

int Cabac::terminate() noexcept {
    m_range -= 2;
    std::uint64_t scaled = static_cast<std::uint64_t>(m_range) << m_bits;
    if (m_value >= scaled) return 1;
    if (m_range < 256) {
        m_range <<= 1;
        --m_bits;
    }
    if (m_bits < 16) refill();
    return 0;
}

std::size_t Cabac::pcm_start() const noexcept {
    std::size_t consumed_bits = m_pos * 8 - static_cast<std::size_t>(m_bits);
    return (consumed_bits + 7) >> 3;
}

std::size_t unescape_nal(const std::uint8_t* src, std::size_t size, std::vector<std::uint8_t>& out) {
    out.resize(size + 16);
    std::uint8_t* dst = out.data();
    std::size_t n = 0;
    int zeros = 0;
    for (std::size_t i = 0; i < size; ++i) {
        std::uint8_t b = src[i];
        if (zeros >= 2 && b == 3) {
            zeros = 0;
            continue;
        }
        dst[n++] = b;
        zeros = (b == 0) ? zeros + 1 : 0;
    }
    std::memset(dst + n, 0, 16);
    return n;
}

} // namespace h264_internal
} // namespace video
} // namespace fizmo
