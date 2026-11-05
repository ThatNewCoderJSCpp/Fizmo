#include "flac.hpp"

#include "file_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fizmo {
namespace audio {

namespace {

const unsigned k_flac_max_channels = 8;
const std::size_t k_flac_window = std::size_t(1) << 16;
const std::size_t k_flac_max_window = std::size_t(1) << 25;
const std::uint64_t k_flac_placeholder = 0xFFFFFFFFFFFFFFFFull;
const double k_flac_pi = 3.14159265358979323846;

struct FlacTables {
    std::uint8_t crc8[256];
    std::uint16_t crc16[256];
    std::uint8_t clz8[256];

    FlacTables() {
        for (unsigned i = 0; i < 256; ++i) {
            unsigned c = i;
            for (int b = 0; b < 8; ++b) c = (c & 0x80u) ? ((c << 1) ^ 0x07u) : (c << 1);
            crc8[i] = static_cast<std::uint8_t>(c & 0xFFu);
            unsigned d = i << 8;
            for (int b = 0; b < 8; ++b) d = (d & 0x8000u) ? ((d << 1) ^ 0x8005u) : (d << 1);
            crc16[i] = static_cast<std::uint16_t>(d & 0xFFFFu);
            unsigned z = 0;
            while (z < 8 && !(i & (0x80u >> z))) ++z;
            clz8[i] = static_cast<std::uint8_t>(z);
        }
    }
};

const FlacTables& flac_tables() {
    static const FlacTables t;
    return t;
}

std::uint8_t flac_crc8(const std::uint8_t* p, std::size_t n) {
    const FlacTables& t = flac_tables();
    unsigned c = 0;
    for (std::size_t i = 0; i < n; ++i) c = t.crc8[(c ^ p[i]) & 0xFFu];
    return static_cast<std::uint8_t>(c);
}

std::uint16_t flac_crc16(const std::uint8_t* p, std::size_t n) {
    const FlacTables& t = flac_tables();
    unsigned c = 0;
    for (std::size_t i = 0; i < n; ++i) c = ((c << 8) ^ t.crc16[((c >> 8) ^ p[i]) & 0xFFu]) & 0xFFFFu;
    return static_cast<std::uint16_t>(c);
}

std::int64_t shift_down(std::int64_t v, unsigned s) noexcept {
    return v >= 0 ? (v >> s) : -(((-v) - 1) >> s) - 1;
}

bool fits_bits(std::int64_t v, unsigned bits) noexcept {
    const std::int64_t lim = std::int64_t(1) << (bits - 1);
    return v >= -lim && v < lim;
}

std::uint64_t zigzag(std::int64_t r) noexcept {
    return r >= 0 ? static_cast<std::uint64_t>(r) << 1 : (static_cast<std::uint64_t>(-(r + 1)) << 1) | 1u;
}

unsigned bit_length(std::uint64_t v) noexcept {
    unsigned n = 0;
    while (v) {
        v >>= 1;
        ++n;
    }
    return n;
}

std::uint32_t read_be24(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 16) | (static_cast<std::uint32_t>(p[1]) << 8) | p[2];
}

std::uint64_t read_be64(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint64_t>(detail::read_be32(p)) << 32) | detail::read_be32(p + 4);
}

void put_be(std::uint8_t* p, std::uint64_t v, unsigned bytes) noexcept {
    for (unsigned i = 0; i < bytes; ++i) p[i] = static_cast<std::uint8_t>((v >> (8 * (bytes - 1 - i))) & 0xFFu);
}

class FlacBitReader {
public:
    FlacBitReader(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size), m_clz(flac_tables().clz8) {}

    bool overrun() const noexcept { return m_overrun; }

    std::uint32_t bits(unsigned n) noexcept {
        if (n == 0) return 0;
        if (m_count < n) {
            refill();
            if (m_count < n) {
                m_overrun = true;
                m_cache = 0;
                m_count = 0;
                return 0;
            }
        }
        const std::uint32_t v = static_cast<std::uint32_t>(m_cache >> (64 - n));
        m_cache <<= n;
        m_count -= n;
        return v;
    }

    std::int64_t sbits(unsigned n) noexcept {
        if (n == 0) return 0;
        std::uint64_t u;
        if (n > 32) {
            const std::uint64_t hi = bits(n - 32);
            u = (hi << 32) | bits(32);
        } else {
            u = bits(n);
        }
        const std::uint64_t sign = std::uint64_t(1) << (n - 1);
        return static_cast<std::int64_t>(u ^ sign) - static_cast<std::int64_t>(sign);
    }

    std::uint32_t unary() noexcept {
        std::uint32_t n = 0;
        for (;;) {
            if (m_count == 0) {
                refill();
                if (m_count == 0) {
                    m_overrun = true;
                    return 0;
                }
            }
            if (m_cache == 0) {
                n += m_count;
                m_count = 0;
                continue;
            }
            unsigned z = 0;
            std::uint64_t v = m_cache;
            while (!(v >> 56)) {
                v <<= 8;
                z += 8;
            }
            z += m_clz[v >> 56];
            m_cache <<= z;
            m_cache <<= 1;
            m_count -= z + 1;
            return n + z;
        }
    }

    void align() noexcept {
        const unsigned drop = m_count & 7u;
        m_cache <<= drop;
        m_count -= drop;
    }

    std::size_t byte_pos() const noexcept { return m_pos - m_count / 8; }

private:
    void refill() noexcept {
        while (m_count <= 56 && m_pos < m_size) {
            m_cache |= static_cast<std::uint64_t>(m_data[m_pos++]) << (56 - m_count);
            m_count += 8;
        }
    }

    const std::uint8_t* m_data;
    std::size_t m_size;
    const std::uint8_t* m_clz;
    std::size_t m_pos = 0;
    std::uint64_t m_cache = 0;
    unsigned m_count = 0;
    bool m_overrun = false;
};

class FlacBitWriter {
public:
    void clear() {
        m_out.clear();
        m_acc = 0;
        m_count = 0;
    }

    void put(std::uint32_t v, unsigned n) {
        if (n == 0) return;
        const std::uint64_t mask = n >= 32 ? 0xFFFFFFFFull : ((std::uint64_t(1) << n) - 1);
        m_acc = (m_acc << n) | (v & mask);
        m_count += n;
        while (m_count >= 8) {
            m_count -= 8;
            m_out.push_back(static_cast<std::uint8_t>((m_acc >> m_count) & 0xFFu));
        }
        m_acc &= (std::uint64_t(1) << m_count) - 1;
    }

    void sput(std::int64_t v, unsigned n) {
        const std::uint64_t u = static_cast<std::uint64_t>(v);
        if (n > 32) {
            put(static_cast<std::uint32_t>((u >> 32) & ((std::uint64_t(1) << (n - 32)) - 1)), n - 32);
            put(static_cast<std::uint32_t>(u & 0xFFFFFFFFull), 32);
        } else {
            put(static_cast<std::uint32_t>(u & 0xFFFFFFFFull), n);
        }
    }

    void unary(std::uint64_t q) {
        while (q >= 32) {
            put(0, 32);
            q -= 32;
        }
        put(1, static_cast<unsigned>(q) + 1);
    }

    void align() {
        if (m_count) put(0, 8 - m_count);
    }

    const std::vector<std::uint8_t>& bytes() const noexcept { return m_out; }

private:
    std::vector<std::uint8_t> m_out;
    std::uint64_t m_acc = 0;
    unsigned m_count = 0;
};

class FlacMd5 {
public:
    FlacMd5() { reset(); }

    void reset() {
        m_state[0] = 0x67452301u;
        m_state[1] = 0xEFCDAB89u;
        m_state[2] = 0x98BADCFEu;
        m_state[3] = 0x10325476u;
        m_length = 0;
        m_used = 0;
    }

    void update(const std::uint8_t* data, std::size_t n) {
        m_length += n;
        if (m_used) {
            const std::size_t take = std::min<std::size_t>(64 - m_used, n);
            std::memcpy(m_buffer + m_used, data, take);
            m_used += take;
            data += take;
            n -= take;
            if (m_used == 64) {
                transform(m_buffer);
                m_used = 0;
            }
        }
        while (n >= 64) {
            transform(data);
            data += 64;
            n -= 64;
        }
        if (n) {
            std::memcpy(m_buffer, data, n);
            m_used = n;
        }
    }

    void finish(std::uint8_t out[16]) {
        const std::uint64_t bits = m_length * 8;
        std::uint8_t pad[64] = {};
        pad[0] = 0x80;
        const std::size_t padlen = m_used < 56 ? 56 - m_used : 120 - m_used;
        update(pad, padlen);
        std::uint8_t len[8];
        detail::put_le64(len, bits);
        update(len, 8);
        for (int i = 0; i < 4; ++i) detail::put_le32(out + 4 * i, m_state[i]);
    }

private:
    void transform(const std::uint8_t* block) {
        static const std::uint32_t k[64] = {
            0xD76AA478u, 0xE8C7B756u, 0x242070DBu, 0xC1BDCEEEu,
            0xF57C0FAFu, 0x4787C62Au, 0xA8304613u, 0xFD469501u,
            0x698098D8u, 0x8B44F7AFu, 0xFFFF5BB1u, 0x895CD7BEu,
            0x6B901122u, 0xFD987193u, 0xA679438Eu, 0x49B40821u,
            0xF61E2562u, 0xC040B340u, 0x265E5A51u, 0xE9B6C7AAu,
            0xD62F105Du, 0x02441453u, 0xD8A1E681u, 0xE7D3FBC8u,
            0x21E1CDE6u, 0xC33707D6u, 0xF4D50D87u, 0x455A14EDu,
            0xA9E3E905u, 0xFCEFA3F8u, 0x676F02D9u, 0x8D2A4C8Au,
            0xFFFA3942u, 0x8771F681u, 0x6D9D6122u, 0xFDE5380Cu,
            0xA4BEEA44u, 0x4BDECFA9u, 0xF6BB4B60u, 0xBEBFBC70u,
            0x289B7EC6u, 0xEAA127FAu, 0xD4EF3085u, 0x04881D05u,
            0xD9D4D039u, 0xE6DB99E5u, 0x1FA27CF8u, 0xC4AC5665u,
            0xF4292244u, 0x432AFF97u, 0xAB9423A7u, 0xFC93A039u,
            0x655B59C3u, 0x8F0CCC92u, 0xFFEFF47Du, 0x85845DD1u,
            0x6FA87E4Fu, 0xFE2CE6E0u, 0xA3014314u, 0x4E0811A1u,
            0xF7537E82u, 0xBD3AF235u, 0x2AD7D2BBu, 0xEB86D391u,
        };
        static const unsigned s[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };
        std::uint32_t m[16];
        for (int i = 0; i < 16; ++i) m[i] = detail::read_le32(block + 4 * i);
        std::uint32_t a = m_state[0];
        std::uint32_t b = m_state[1];
        std::uint32_t c = m_state[2];
        std::uint32_t d = m_state[3];
        for (unsigned i = 0; i < 64; ++i) {
            std::uint32_t f;
            unsigned g;
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) & 15u;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) & 15u;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) & 15u;
            }
            f = f + a + k[i] + m[g];
            a = d;
            d = c;
            c = b;
            const unsigned r = s[((i >> 4) << 2) | (i & 3u)];
            b = b + ((f << r) | (f >> (32 - r)));
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
    }

    std::uint32_t m_state[4];
    std::uint64_t m_length = 0;
    std::uint8_t m_buffer[64] = {};
    std::size_t m_used = 0;
};

void md5_samples(FlacMd5& md5, const std::int32_t* samples, std::size_t count, unsigned bps, std::vector<std::uint8_t>& scratch) {
    const unsigned bytes = (bps + 7) / 8;
    scratch.resize(count * bytes);
    std::uint8_t* p = scratch.data();
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t u = static_cast<std::uint32_t>(samples[i]);
        for (unsigned b = 0; b < bytes; ++b) *p++ = static_cast<std::uint8_t>((u >> (8 * b)) & 0xFFu);
    }
    md5.update(scratch.data(), scratch.size());
}

struct StreamParams {
    std::uint32_t rate = 0;
    unsigned channels = 0;
    unsigned bps = 0;
    unsigned min_block = 0;
    unsigned max_block = 0;
};

struct FrameHeader {
    unsigned block = 0;
    std::uint32_t rate = 0;
    unsigned channels = 0;
    unsigned assignment = 0;
    unsigned bps = 0;
    bool variable = false;
    std::uint64_t number = 0;
    std::uint64_t first_sample = 0;
    std::uint64_t offset = 0;
};

enum class FrameStatus {
    ok,
    not_frame,
    need_more,
    corrupt,
    crc,
    truncated
};

struct FrameWorkspace {
    std::vector<std::int64_t> chan[k_flac_max_channels];
    std::vector<std::int32_t> out;
};

FrameStatus parse_frame_header(const std::uint8_t* p, std::size_t avail, const StreamParams& sp, FrameHeader& h, std::size_t& len) {
    if (avail < 2) return FrameStatus::need_more;
    if (p[0] != 0xFF || (p[1] & 0xFEu) != 0xF8u) return FrameStatus::not_frame;
    if (avail < 5) return FrameStatus::need_more;
    const unsigned bs_code = p[2] >> 4;
    const unsigned sr_code = p[2] & 15u;
    const unsigned ch_code = p[3] >> 4;
    const unsigned ss_code = (p[3] >> 1) & 7u;
    if (bs_code == 0 || sr_code == 15 || ch_code > 10 || ss_code == 3 || (p[3] & 1u)) return FrameStatus::not_frame;
    h.variable = (p[1] & 1u) != 0;
    std::size_t i = 4;
    const unsigned b0 = p[i++];
    unsigned extra = 0;
    std::uint64_t v = 0;
    if (b0 < 0x80u) {
        v = b0;
    } else if ((b0 & 0xE0u) == 0xC0u) {
        extra = 1;
        v = b0 & 0x1Fu;
    } else if ((b0 & 0xF0u) == 0xE0u) {
        extra = 2;
        v = b0 & 0x0Fu;
    } else if ((b0 & 0xF8u) == 0xF0u) {
        extra = 3;
        v = b0 & 0x07u;
    } else if ((b0 & 0xFCu) == 0xF8u) {
        extra = 4;
        v = b0 & 0x03u;
    } else if ((b0 & 0xFEu) == 0xFCu) {
        extra = 5;
        v = b0 & 0x01u;
    } else if (b0 == 0xFEu) {
        extra = 6;
    } else {
        return FrameStatus::not_frame;
    }
    if (!h.variable && extra > 5) return FrameStatus::not_frame;
    const std::size_t tail = (bs_code == 6 ? 1u : bs_code == 7 ? 2u : 0u) + (sr_code == 12 ? 1u : (sr_code == 13 || sr_code == 14) ? 2u : 0u);
    if (avail < i + extra + tail + 1) return FrameStatus::need_more;
    for (unsigned k = 0; k < extra; ++k) {
        const unsigned b = p[i++];
        if ((b & 0xC0u) != 0x80u) return FrameStatus::not_frame;
        v = (v << 6) | (b & 0x3Fu);
    }
    h.number = v;
    if (bs_code == 1) {
        h.block = 192;
    } else if (bs_code <= 5) {
        h.block = 576u << (bs_code - 2);
    } else if (bs_code == 6) {
        h.block = static_cast<unsigned>(p[i++]) + 1;
    } else if (bs_code == 7) {
        h.block = ((static_cast<unsigned>(p[i]) << 8) | p[i + 1]) + 1;
        i += 2;
    } else {
        h.block = 256u << (bs_code - 8);
    }
    static const std::uint32_t rates[12] = { 0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };
    if (sr_code == 0) {
        h.rate = sp.rate;
    } else if (sr_code < 12) {
        h.rate = rates[sr_code];
    } else if (sr_code == 12) {
        h.rate = static_cast<std::uint32_t>(p[i++]) * 1000u;
    } else {
        h.rate = (static_cast<std::uint32_t>(p[i]) << 8) | p[i + 1];
        if (sr_code == 14) h.rate *= 10;
        i += 2;
    }
    if (flac_crc8(p, i) != p[i]) return FrameStatus::not_frame;
    len = i + 1;
    if (h.rate == 0) return FrameStatus::not_frame;
    if (ch_code < 8) {
        h.channels = ch_code + 1;
        h.assignment = 0;
    } else {
        h.channels = 2;
        h.assignment = ch_code;
    }
    static const unsigned sizes[8] = { 0, 8, 12, 0, 16, 20, 24, 32 };
    h.bps = ss_code == 0 ? sp.bps : sizes[ss_code];
    if (h.bps == 0) return FrameStatus::not_frame;
    if (sp.channels && h.channels != sp.channels) return FrameStatus::not_frame;
    if (sp.bps && h.bps != sp.bps) return FrameStatus::not_frame;
    if (h.variable) {
        h.first_sample = v;
    } else {
        const unsigned nominal = (sp.min_block == sp.max_block && sp.max_block >= 16) ? sp.max_block : h.block;
        h.first_sample = v * nominal;
    }
    return FrameStatus::ok;
}

FrameStatus decode_residual(FlacBitReader& br, unsigned block, unsigned order, std::int64_t* out) {
    const unsigned method = br.bits(2);
    if (method > 1) return FrameStatus::corrupt;
    const unsigned porder = br.bits(4);
    const unsigned parts = 1u << porder;
    const unsigned psize = block >> porder;
    if ((psize << porder) != block || psize < order) return FrameStatus::corrupt;
    const unsigned pbits = method ? 5u : 4u;
    const unsigned escape = method ? 31u : 15u;
    std::size_t idx = order;
    for (unsigned p = 0; p < parts; ++p) {
        const unsigned count = psize - (p == 0 ? order : 0u);
        const unsigned k = br.bits(pbits);
        if (k == escape) {
            const unsigned w = br.bits(5);
            for (unsigned i = 0; i < count; ++i) out[idx++] = br.sbits(w);
        } else {
            for (unsigned i = 0; i < count; ++i) {
                const std::uint64_t q = br.unary();
                const std::uint64_t u = (q << k) | br.bits(k);
                if (u > 0xFFFFFFFFull) return br.overrun() ? FrameStatus::need_more : FrameStatus::corrupt;
                out[idx++] = static_cast<std::int64_t>(u >> 1) ^ -static_cast<std::int64_t>(u & 1u);
            }
        }
        if (br.overrun()) return FrameStatus::need_more;
    }
    return FrameStatus::ok;
}

FrameStatus decode_subframe(FlacBitReader& br, unsigned block, unsigned bps, std::int64_t* out) {
    if (br.bits(1) != 0) return br.overrun() ? FrameStatus::need_more : FrameStatus::corrupt;
    const unsigned type = br.bits(6);
    unsigned wasted = 0;
    if (br.bits(1)) {
        wasted = br.unary() + 1;
        if (br.overrun()) return FrameStatus::need_more;
        if (wasted >= bps) return FrameStatus::corrupt;
    }
    const unsigned b = bps - wasted;
    if (type == 0) {
        const std::int64_t v = br.sbits(b);
        for (unsigned i = 0; i < block; ++i) out[i] = v;
    } else if (type == 1) {
        for (unsigned i = 0; i < block; ++i) out[i] = br.sbits(b);
    } else if (type >= 8 && type <= 12) {
        const unsigned order = type - 8;
        if (order > block) return FrameStatus::corrupt;
        for (unsigned i = 0; i < order; ++i) out[i] = br.sbits(b);
        const FrameStatus st = decode_residual(br, block, order, out);
        if (st != FrameStatus::ok) return st;
        for (unsigned i = order; i < block; ++i) {
            std::int64_t v = out[i];
            switch (order) {
                case 1: v += out[i - 1]; break;
                case 2: v += 2 * out[i - 1] - out[i - 2]; break;
                case 3: v += 3 * out[i - 1] - 3 * out[i - 2] + out[i - 3]; break;
                case 4: v += 4 * out[i - 1] - 6 * out[i - 2] + 4 * out[i - 3] - out[i - 4]; break;
                default: break;
            }
            if (!fits_bits(v, b)) return FrameStatus::corrupt;
            out[i] = v;
        }
    } else if (type >= 32) {
        const unsigned order = type - 31;
        if (order > block) return FrameStatus::corrupt;
        for (unsigned i = 0; i < order; ++i) out[i] = br.sbits(b);
        const unsigned precision = br.bits(4) + 1;
        const std::int64_t shift = br.sbits(5);
        if (br.overrun()) return FrameStatus::need_more;
        if (precision == 16 || shift < 0) return FrameStatus::corrupt;
        std::int64_t coefs[32];
        for (unsigned j = 0; j < order; ++j) coefs[j] = br.sbits(precision);
        const FrameStatus st = decode_residual(br, block, order, out);
        if (st != FrameStatus::ok) return st;
        const unsigned sh = static_cast<unsigned>(shift);
        for (unsigned i = order; i < block; ++i) {
            std::int64_t sum = 0;
            const std::int64_t* hist = out + i;
            for (unsigned j = 0; j < order; ++j) sum += coefs[j] * hist[-1 - static_cast<std::ptrdiff_t>(j)];
            const std::int64_t v = out[i] + shift_down(sum, sh);
            if (!fits_bits(v, b)) return FrameStatus::corrupt;
            out[i] = v;
        }
    } else {
        return br.overrun() ? FrameStatus::need_more : FrameStatus::corrupt;
    }
    if (br.overrun()) return FrameStatus::need_more;
    if (wasted) {
        const std::int64_t mul = std::int64_t(1) << wasted;
        for (unsigned i = 0; i < block; ++i) out[i] *= mul;
    }
    return FrameStatus::ok;
}

FrameStatus decode_frame(const std::uint8_t* p, std::size_t avail, const StreamParams& sp, FrameHeader& h, FrameWorkspace& ws, std::size_t& frame_len) {
    std::size_t hl = 0;
    FrameStatus st = parse_frame_header(p, avail, sp, h, hl);
    if (st != FrameStatus::ok) return st;
    const unsigned n = h.block;
    const unsigned nch = h.channels;
    for (unsigned c = 0; c < nch; ++c) {
        if (ws.chan[c].size() < n) ws.chan[c].resize(n);
    }
    FlacBitReader br(p + hl, avail - hl);
    for (unsigned c = 0; c < nch; ++c) {
        unsigned bps = h.bps;
        if ((h.assignment == 8 && c == 1) || (h.assignment == 9 && c == 0) || (h.assignment == 10 && c == 1)) ++bps;
        st = decode_subframe(br, n, bps, ws.chan[c].data());
        if (st != FrameStatus::ok) return st;
    }
    br.align();
    if (br.overrun()) return FrameStatus::need_more;
    const std::size_t end = hl + br.byte_pos();
    if (end + 2 > avail) return FrameStatus::need_more;
    frame_len = end + 2;
    const std::uint16_t want = static_cast<std::uint16_t>((static_cast<unsigned>(p[end]) << 8) | p[end + 1]);
    if (flac_crc16(p, end) != want) return FrameStatus::crc;
    ws.out.resize(static_cast<std::size_t>(n) * nch);
    std::int32_t* out = ws.out.data();
    const std::int64_t* a = ws.chan[0].data();
    if (h.assignment >= 8) {
        const std::int64_t* b = ws.chan[1].data();
        for (unsigned i = 0; i < n; ++i) {
            std::int64_t l;
            std::int64_t r;
            if (h.assignment == 8) {
                l = a[i];
                r = a[i] - b[i];
            } else if (h.assignment == 9) {
                r = b[i];
                l = a[i] + b[i];
            } else {
                const std::int64_t mid = a[i] * 2 + static_cast<std::int64_t>(static_cast<std::uint64_t>(b[i]) & 1u);
                l = shift_down(mid + b[i], 1);
                r = shift_down(mid - b[i], 1);
            }
            if (!fits_bits(l, h.bps) || !fits_bits(r, h.bps)) return FrameStatus::corrupt;
            out[2 * i] = static_cast<std::int32_t>(l);
            out[2 * i + 1] = static_cast<std::int32_t>(r);
        }
    } else {
        for (unsigned c = 0; c < nch; ++c) {
            const std::int64_t* s = ws.chan[c].data();
            for (unsigned i = 0; i < n; ++i) {
                if (!fits_bits(s[i], h.bps)) return FrameStatus::corrupt;
                out[static_cast<std::size_t>(i) * nch + c] = static_cast<std::int32_t>(s[i]);
            }
        }
    }
    return FrameStatus::ok;
}

std::size_t frame_bound(const FrameHeader& h) {
    const std::size_t verbatim = 16 + static_cast<std::size_t>(h.channels) * ((static_cast<std::size_t>(h.block) * 33 + 7) / 8 + 8) + 2;
    return std::min(k_flac_max_window, verbatim * 2 + k_flac_window);
}

std::size_t skip_id3(const std::uint8_t* p, std::size_t size) {
    std::size_t pos = 0;
    for (int n = 0; n < 16; ++n) {
        if (size - pos < 10 || std::memcmp(p + pos, "ID3", 3) != 0) break;
        const std::uint8_t* h = p + pos;
        const std::uint64_t len = (static_cast<std::uint64_t>(h[6] & 0x7Fu) << 21) | (static_cast<std::uint64_t>(h[7] & 0x7Fu) << 14) | (static_cast<std::uint64_t>(h[8] & 0x7Fu) << 7) | (h[9] & 0x7Fu);
        const std::uint64_t total = 10 + len + ((h[5] & 0x10u) ? 10u : 0u);
        if (total > size - pos) return size;
        pos += static_cast<std::size_t>(total);
    }
    return pos;
}

std::string lower_ascii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

void parse_vorbis_comment(const std::vector<std::uint8_t>& d, FlacInfo& info) {
    const std::size_t size = d.size();
    if (size < 8) return;
    const std::uint32_t vendor = detail::read_le32(d.data());
    if (vendor > size - 8) return;
    std::size_t p = 4 + vendor;
    const std::uint32_t count = detail::read_le32(d.data() + p);
    p += 4;
    for (std::uint32_t i = 0; i < count && size - p >= 4; ++i) {
        const std::uint32_t n = detail::read_le32(d.data() + p);
        p += 4;
        if (n > size - p) break;
        const std::string entry(reinterpret_cast<const char*>(d.data() + p), n);
        p += n;
        const std::size_t eq = entry.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        info.tags.emplace_back(lower_ascii(entry.substr(0, eq)), entry.substr(eq + 1));
    }
}

struct FlacSource {
    virtual ~FlacSource() = default;
    virtual std::size_t read(void* data, std::size_t bytes) = 0;
    virtual bool seek(std::uint64_t offset) = 0;
    virtual std::uint64_t size() = 0;
};

struct FlacFileSource : FlacSource {
    detail::File file;
    std::uint64_t cached_size = 0;
    std::size_t read(void* data, std::size_t bytes) override { return file.read(data, bytes); }
    bool seek(std::uint64_t offset) override { return file.seek(static_cast<std::int64_t>(offset)); }
    std::uint64_t size() override { return cached_size; }
};

struct FlacMemorySource : FlacSource {
    const std::uint8_t* data = nullptr;
    std::size_t length = 0;
    std::size_t pos = 0;
    std::size_t read(void* out, std::size_t bytes) override {
        const std::size_t n = std::min(bytes, length - std::min(pos, length));
        if (n) std::memcpy(out, data + pos, n);
        pos += n;
        return n;
    }
    bool seek(std::uint64_t offset) override {
        if (offset > length) return false;
        pos = static_cast<std::size_t>(offset);
        return true;
    }
    std::uint64_t size() override { return length; }
};

struct FlacSeekPoint {
    std::uint64_t sample = 0;
    std::uint64_t offset = 0;
    std::uint32_t frames = 0;
};

const char* status_message(FrameStatus st) {
    switch (st) {
        case FrameStatus::crc: return "FLAC frame CRC mismatch at byte ";
        case FrameStatus::truncated: return "truncated FLAC frame at byte ";
        default: return "corrupt FLAC frame at byte ";
    }
}

} // namespace

struct FlacReader::Impl {
    std::unique_ptr<FlacSource> src;
    FlacInfo info;
    StreamParams sp;
    std::vector<FlacSeekPoint> points;
    std::uint64_t first_frame = 0;
    std::uint64_t file_size = 0;
    std::uint64_t base = 0;
    std::vector<std::uint8_t> buf;
    std::size_t head = 0;
    std::size_t tail = 0;
    std::uint64_t buf_base = 0;
    bool eof = false;
    bool junk = false;
    FrameWorkspace ws;
    std::vector<std::int32_t> out;
    std::size_t out_frames = 0;
    std::size_t out_pos = 0;
    std::uint64_t silence = 0;
    std::uint64_t expected = 0;
    std::uint64_t position = 0;
    bool end_checked = false;
    std::size_t errors = 0;
    std::string last_error;
    std::vector<std::int32_t> scratch;

    std::size_t avail() const noexcept { return tail - head; }
    std::uint64_t tell() const noexcept { return buf_base + head; }

    bool fill(std::size_t want) {
        if (tail - head >= want) return true;
        if (eof) return false;
        if (head > 0) {
            if (tail > head) std::memmove(buf.data(), buf.data() + head, tail - head);
            tail -= head;
            buf_base += head;
            head = 0;
        }
        const std::size_t cap = std::max(want, k_flac_window);
        if (buf.size() < cap) buf.resize(cap);
        while (tail < buf.size() && !eof) {
            const std::size_t ask = buf.size() - tail;
            const std::size_t got = src->read(buf.data() + tail, ask);
            tail += got;
            if (got < ask) eof = true;
        }
        return tail - head >= want;
    }

    void reposition(std::uint64_t offset) {
        head = 0;
        tail = 0;
        buf_base = offset;
        eof = false;
        if (offset > file_size || !src->seek(offset)) {
            buf_base = file_size;
            eof = true;
        }
    }

    void note(const std::string& message) {
        ++errors;
        last_error = message;
    }

    FrameStatus try_frame(FrameHeader& h, std::size_t& len) {
        fill(64);
        std::size_t hl = 0;
        FrameStatus st = parse_frame_header(buf.data() + head, avail(), sp, h, hl);
        if (st == FrameStatus::need_more) return FrameStatus::not_frame;
        if (st != FrameStatus::ok) return st;
        const std::size_t cap = frame_bound(h);
        std::size_t want = std::min(cap, std::max(avail(), k_flac_window / 2));
        for (;;) {
            fill(want);
            st = decode_frame(buf.data() + head, avail(), sp, h, ws, len);
            if (st != FrameStatus::need_more) return st;
            if (eof || avail() >= cap) return FrameStatus::truncated;
            want = std::min(cap, avail() * 2);
        }
    }

    bool next_frame(bool count, FrameHeader& h) {
        const std::uint64_t start = tell();
        for (;;) {
            if (avail() < 16) fill(k_flac_window);
            if (avail() < 2) {
                head = tail;
                return false;
            }
            const std::uint8_t* b = buf.data();
            std::size_t i = head;
            const std::size_t last = tail - 1;
            while (i < last && !(b[i] == 0xFF && (b[i + 1] & 0xFEu) == 0xF8u)) ++i;
            if (i >= last) {
                head = last;
                if (eof) {
                    head = tail;
                    return false;
                }
                fill(k_flac_window);
                continue;
            }
            head = i;
            const std::uint64_t at = tell();
            std::size_t len = 0;
            const FrameStatus st = try_frame(h, len);
            if (st == FrameStatus::ok) {
                head += len;
                h.offset = at;
                junk = at != start;
                return true;
            }
            if (count && st != FrameStatus::not_frame) note(status_message(st) + std::to_string(at));
            head += 1;
        }
    }

    void take_frame(const FrameHeader& h, std::uint64_t skip) {
        out.swap(ws.out);
        out_frames = h.block;
        out_pos = static_cast<std::size_t>(skip);
        expected = h.first_sample + h.block;
    }

    bool open(std::unique_ptr<FlacSource> s, std::string* error) {
        src = std::move(s);
        file_size = src->size();
        auto fail = [&](const char* message) {
            if (error) *error = message;
            src.reset();
            return false;
        };
        std::uint64_t pos = 0;
        std::uint8_t b[10];
        for (int n = 0; n < 16; ++n) {
            if (!src->seek(pos)) return fail("not a FLAC file");
            const std::size_t got = src->read(b, 10);
            if (got < 4) return fail("not a FLAC file");
            if (got == 10 && std::memcmp(b, "ID3", 3) == 0) {
                const std::uint64_t len = (static_cast<std::uint64_t>(b[6] & 0x7Fu) << 21) | (static_cast<std::uint64_t>(b[7] & 0x7Fu) << 14) | (static_cast<std::uint64_t>(b[8] & 0x7Fu) << 7) | (b[9] & 0x7Fu);
                pos += 10 + len + ((b[5] & 0x10u) ? 10u : 0u);
                continue;
            }
            break;
        }
        if (std::memcmp(b, "fLaC", 4) != 0) return fail("not a FLAC file");
        pos += 4;
        bool have_info = false;
        bool last = false;
        std::vector<std::uint8_t> block;
        while (!last) {
            std::uint8_t hb[4];
            if (!src->seek(pos) || src->read(hb, 4) != 4) return fail("the FLAC metadata is truncated");
            last = (hb[0] & 0x80u) != 0;
            const unsigned type = hb[0] & 0x7Fu;
            const std::uint32_t len = read_be24(hb + 1);
            pos += 4;
            if (len > file_size - std::min(pos, file_size)) return fail("the FLAC metadata is truncated");
            if (type == 127) return fail("the FLAC metadata is damaged");
            if (type == 0 || type == 3 || type == 4) {
                block.resize(len);
                if (len && src->read(block.data(), len) != len) return fail("the FLAC metadata is truncated");
            }
            if (type == 0) {
                if (len < 34 || have_info) return fail("the FLAC stream info block is damaged");
                const std::uint8_t* d = block.data();
                info.min_block_size = detail::read_be16(d);
                info.max_block_size = detail::read_be16(d + 2);
                info.min_frame_size = read_be24(d + 4);
                info.max_frame_size = read_be24(d + 7);
                info.sample_rate = (static_cast<std::uint32_t>(d[10]) << 12) | (static_cast<std::uint32_t>(d[11]) << 4) | (d[12] >> 4);
                info.channels = static_cast<std::uint16_t>(((d[12] >> 1) & 7u) + 1);
                info.bits_per_sample = static_cast<std::uint16_t>((((d[12] & 1u) << 4) | (d[13] >> 4)) + 1);
                info.frames = (static_cast<std::uint64_t>(d[13] & 15u) << 32) | detail::read_be32(d + 14);
                std::memcpy(info.md5, d + 18, 16);
                have_info = true;
            } else if (type == 3) {
                const std::size_t count = len / 18;
                points.clear();
                points.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    FlacSeekPoint pt;
                    pt.sample = read_be64(block.data() + 18 * i);
                    pt.offset = read_be64(block.data() + 18 * i + 8);
                    pt.frames = detail::read_be16(block.data() + 18 * i + 16);
                    if (pt.sample != k_flac_placeholder) points.push_back(pt);
                }
                info.has_seek_table = true;
            } else if (type == 4) {
                parse_vorbis_comment(block, info);
            }
            pos += len;
        }
        if (!have_info) return fail("the FLAC file has no stream info block");
        if (info.sample_rate == 0) return fail("the FLAC file has no sample rate");
        if (info.bits_per_sample < 4) return fail("unsupported FLAC sample size");
        sp.rate = info.sample_rate;
        sp.channels = info.channels;
        sp.bps = info.bits_per_sample;
        sp.min_block = info.min_block_size;
        sp.max_block = info.max_block_size;
        first_frame = pos;
        reposition(first_frame);
        FrameHeader h;
        base = 0;
        if (next_frame(false, h) && h.offset == first_frame) base = h.first_sample;
        reposition(first_frame);
        expected = base;
        position = 0;
        return true;
    }

    std::size_t read_pcm(std::int32_t* dst, std::size_t frames) {
        if (!src || !dst) return 0;
        const std::size_t nch = info.channels;
        std::size_t done = 0;
        while (done < frames) {
            if (silence) {
                const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(silence, frames - done));
                std::fill(dst + done * nch, dst + (done + n) * nch, 0);
                silence -= n;
                done += n;
                position += n;
                continue;
            }
            if (out_pos < out_frames) {
                const std::size_t n = std::min(out_frames - out_pos, frames - done);
                std::memcpy(dst + done * nch, out.data() + out_pos * nch, n * nch * sizeof(std::int32_t));
                out_pos += n;
                done += n;
                position += n;
                continue;
            }
            FrameHeader h;
            if (!next_frame(true, h)) {
                if (!end_checked) {
                    end_checked = true;
                    if (info.frames && position < info.frames) note("the FLAC stream ends early (" + std::to_string(position) + " of " + std::to_string(info.frames) + " frames)");
                }
                break;
            }
            if (junk && h.first_sample > expected) {
                const std::uint64_t gap = h.first_sample - expected;
                const bool inside = info.frames ? (h.first_sample + h.block <= base + info.frames) : gap <= (std::uint64_t(1) << 24);
                if (inside) silence = gap;
            }
            take_frame(h, 0);
        }
        return done;
    }

    bool scan_to(std::uint64_t offset, std::uint64_t target, std::size_t limit) {
        reposition(offset);
        FrameHeader h;
        for (std::size_t count = 0; count < limit; ++count) {
            if (!next_frame(false, h)) return false;
            if (h.first_sample > target) {
                if (count == 0) return false;
                take_frame(h, 0);
                silence = h.first_sample - target;
                return true;
            }
            if (target < h.first_sample + h.block) {
                take_frame(h, target - h.first_sample);
                return true;
            }
        }
        return false;
    }

    bool seek(std::uint64_t frame) {
        if (!src) return false;
        silence = 0;
        out_frames = 0;
        out_pos = 0;
        end_checked = false;
        if (info.frames && frame >= info.frames) {
            reposition(file_size);
            position = info.frames;
            expected = base + info.frames;
            return true;
        }
        const std::uint64_t target = base + frame;
        if (!points.empty()) {
            const FlacSeekPoint* best = nullptr;
            for (const FlacSeekPoint& p : points) {
                if (p.sample <= target && (!best || p.sample >= best->sample)) best = &p;
            }
            if (best && best->offset < file_size - first_frame && scan_to(first_frame + best->offset, target, 1024)) {
                position = frame;
                return true;
            }
        }
        std::uint64_t lo = first_frame;
        std::uint64_t hi = file_size;
        const std::uint64_t threshold = std::max<std::uint64_t>(k_flac_window, 2ull * info.max_frame_size);
        for (int iter = 0; iter < 64 && hi > lo && hi - lo > threshold; ++iter) {
            const std::uint64_t mid = lo + (hi - lo) / 2;
            reposition(mid);
            FrameHeader h;
            if (!next_frame(false, h) || h.offset >= hi) {
                hi = mid;
                continue;
            }
            if (h.first_sample <= target) {
                if (target < h.first_sample + h.block) {
                    take_frame(h, target - h.first_sample);
                    position = frame;
                    return true;
                }
                lo = h.offset;
            } else {
                hi = mid;
            }
        }
        if (scan_to(lo, target, std::numeric_limits<std::size_t>::max())) {
            position = frame;
            return true;
        }
        silence = 0;
        out_frames = 0;
        out_pos = 0;
        reposition(file_size);
        position = frame;
        return false;
    }
};

FlacReader::FlacReader() = default;
FlacReader::~FlacReader() = default;
FlacReader::FlacReader(FlacReader&&) noexcept = default;
FlacReader& FlacReader::operator=(FlacReader&&) noexcept = default;

bool FlacReader::open(const std::string& path, std::string* error) {
    close();
    auto src = std::make_unique<FlacFileSource>();
    if (!src->file.open(path, "rb")) {
        if (error) *error = "can't open " + path;
        return false;
    }
    const std::int64_t size = src->file.size();
    src->cached_size = size > 0 ? static_cast<std::uint64_t>(size) : 0;
    auto impl = std::make_unique<Impl>();
    if (!impl->open(std::move(src), error)) return false;
    m_impl = std::move(impl);
    return true;
}

bool FlacReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    if (!data && size) {
        if (error) *error = "not a FLAC file";
        return false;
    }
    auto src = std::make_unique<FlacMemorySource>();
    src->data = static_cast<const std::uint8_t*>(data);
    src->length = size;
    auto impl = std::make_unique<Impl>();
    if (!impl->open(std::move(src), error)) return false;
    m_impl = std::move(impl);
    return true;
}

void FlacReader::close() {
    m_impl.reset();
}

bool FlacReader::is_open() const noexcept {
    return m_impl != nullptr;
}

const FlacInfo& FlacReader::info() const noexcept {
    static const FlacInfo empty;
    return m_impl ? m_impl->info : empty;
}

std::size_t FlacReader::read_pcm(std::int32_t* interleaved, std::size_t frames) {
    return m_impl ? m_impl->read_pcm(interleaved, frames) : 0;
}

std::size_t FlacReader::read(float* interleaved, std::size_t frames) {
    if (!m_impl || !interleaved) return 0;
    Impl& im = *m_impl;
    const std::size_t nch = im.info.channels;
    const double scale = 1.0 / static_cast<double>(std::uint64_t(1) << (im.info.bits_per_sample - 1));
    std::size_t done = 0;
    while (done < frames) {
        const std::size_t n = std::min<std::size_t>(frames - done, 4096);
        im.scratch.resize(n * nch);
        const std::size_t got = im.read_pcm(im.scratch.data(), n);
        float* d = interleaved + done * nch;
        for (std::size_t i = 0; i < got * nch; ++i) d[i] = static_cast<float>(im.scratch[i] * scale);
        done += got;
        if (got < n) break;
    }
    return done;
}

std::size_t FlacReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_impl) return 0;
    const FlacInfo& inf = m_impl->info;
    if (out.channels() != inf.channels || out.sample_rate() != inf.sample_rate) out = AudioBuffer(inf.channels, inf.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool FlacReader::seek(std::uint64_t frame) {
    return m_impl ? m_impl->seek(frame) : false;
}

std::uint64_t FlacReader::position() const noexcept {
    return m_impl ? m_impl->position : 0;
}

std::size_t FlacReader::error_count() const noexcept {
    return m_impl ? m_impl->errors : 0;
}

const std::string& FlacReader::last_error() const noexcept {
    static const std::string none;
    return m_impl ? m_impl->last_error : none;
}

bool is_flac(const void* data, std::size_t size) noexcept {
    if (!data) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    const std::size_t at = skip_id3(p, size);
    return size - at >= 4 && std::memcmp(p + at, "fLaC", 4) == 0;
}

namespace {

bool read_all(FlacReader& r, AudioBuffer& out, std::string* error, FlacInfo* info) {
    const FlacInfo& inf = r.info();
    out = AudioBuffer(inf.channels, inf.sample_rate);
    out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(inf.frames, std::uint64_t(1) << 22)));
    const std::size_t nch = inf.channels;
    const std::size_t chunk = 16384;
    std::vector<std::int32_t> pcm(chunk * nch);
    std::vector<float> tmp(chunk * nch);
    std::vector<std::uint8_t> bytes;
    FlacMd5 md5;
    const double scale = 1.0 / static_cast<double>(std::uint64_t(1) << (inf.bits_per_sample - 1));
    for (;;) {
        const std::size_t got = r.read_pcm(pcm.data(), chunk);
        if (got == 0) break;
        md5_samples(md5, pcm.data(), got * nch, inf.bits_per_sample, bytes);
        for (std::size_t i = 0; i < got * nch; ++i) tmp[i] = static_cast<float>(pcm[i] * scale);
        out.append(tmp.data(), got);
    }
    if (info) *info = inf;
    if (r.error_count()) {
        if (error) *error = r.last_error();
        return false;
    }
    bool have_md5 = false;
    for (std::uint8_t b : inf.md5) have_md5 = have_md5 || b != 0;
    if (have_md5) {
        std::uint8_t sum[16];
        md5.finish(sum);
        if (std::memcmp(sum, inf.md5, 16) != 0) {
            if (error) *error = "the decoded FLAC audio doesn't match its MD5 signature";
            return false;
        }
    }
    return true;
}

} // namespace

bool read_flac(const std::string& path, AudioBuffer& out, std::string* error, FlacInfo* info) {
    FlacReader r;
    if (!r.open(path, error)) return false;
    return read_all(r, out, error, info);
}

bool read_flac(const void* data, std::size_t size, AudioBuffer& out, std::string* error, FlacInfo* info) {
    FlacReader r;
    if (!r.open(data, size, error)) return false;
    return read_all(r, out, error, info);
}

namespace {

const unsigned k_window_tukey = 1u;
const unsigned k_window_hann = 2u;
const unsigned k_window_flat = 4u;
const unsigned k_window_halves = 8u;
const unsigned k_window_thirds = 16u;
const unsigned k_window_edges = 32u;

struct FlacLevel {
    unsigned block;
    unsigned max_lpc;
    unsigned max_porder;
    unsigned windows;
    unsigned candidates;
    unsigned precision_span;
};

const FlacLevel k_flac_levels[9] = {
    { 2048, 0, 4, k_window_tukey, 0, 0 },
    { 2048, 4, 4, k_window_tukey, 1, 0 },
    { 2048, 6, 4, k_window_tukey, 1, 0 },
    { 2048, 8, 5, k_window_tukey, 1, 0 },
    { 2048, 10, 5, k_window_tukey, 1, 0 },
    { 2048, 12, 5, k_window_tukey, 1, 0 },
    { 2048, 12, 6, k_window_tukey, 2, 0 },
    { 2048, 12, 6, k_window_tukey | k_window_halves, 2, 0 },
    { 2048, 12, 6, k_window_tukey | k_window_halves | k_window_thirds, 2, 1 },
};

FlacLevel flac_level(int compression) {
    const int c = std::max(0, std::min(8, compression));
    return k_flac_levels[c];
}

unsigned qlp_precision(unsigned bps, unsigned n, unsigned order, double gain, double cmax) {
    int log2c = 0;
    std::frexp(cmax, &log2c);
    const double c = std::ldexp(1.0, log2c - 1);
    const double count = static_cast<double>(n > order ? n - order : 1u);
    const double ideal = 0.5 * std::log2(4.0 / 3.0 * count * c * c * std::max(gain, 1.0));
    const double cap = static_cast<double>(std::min(15u, bps + 4u));
    const double v = std::floor(std::min(cap, std::max(4.0, ideal)) + 0.5);
    return static_cast<unsigned>(v);
}

struct RicePlan {
    unsigned method = 0;
    unsigned porder = 0;
    std::vector<std::uint8_t> param;
    std::vector<std::uint8_t> width;
};

struct EncSignal {
    std::vector<std::int64_t> x;
    unsigned bps = 0;
    unsigned b = 0;
    unsigned wasted = 0;
    int type = 1;
    unsigned order = 0;
    unsigned precision = 0;
    int shift = 0;
    std::int32_t coefs[32] = {};
    RicePlan rice;
    std::vector<std::int64_t> res;
    std::uint64_t bits = 0;
};

unsigned rice_guess(std::uint64_t sum, unsigned count) {
    unsigned k = 0;
    while (k < 30 && (static_cast<std::uint64_t>(count) << (k + 1)) < sum) ++k;
    return k;
}

std::uint64_t rice_estimate(std::uint64_t sum, unsigned count, unsigned k) {
    std::uint64_t body = sum >> k;
    if (k) body = body > count / 2 ? body - count / 2 : 0;
    return static_cast<std::uint64_t>(count) * (k + 1) + body;
}

class FlacAnalyzer {
public:
    void configure(const FlacLevel& level, unsigned stream_bps) {
        m_level = level;
        m_stream_bps = stream_bps;
        m_window_n = 0;
    }

    void analyze(EncSignal& s, unsigned n) {
        std::int64_t* x = s.x.data();
        s.wasted = 0;
        s.b = s.bps;
        bool constant = true;
        for (unsigned i = 1; i < n && constant; ++i) constant = x[i] == x[0];
        if (constant) {
            s.type = 0;
            s.order = 0;
            s.bits = 8 + s.bps;
            return;
        }
        std::uint64_t all = 0;
        for (unsigned i = 0; i < n; ++i) all |= static_cast<std::uint64_t>(x[i]);
        unsigned w = 0;
        while (!(all & 1u) && w + 1 < s.bps) {
            all >>= 1;
            ++w;
        }
        if (w) {
            for (unsigned i = 0; i < n; ++i) x[i] = shift_down(x[i], w);
        }
        s.wasted = w;
        s.b = s.bps - w;
        const std::uint64_t head = 8 + w;
        s.type = 1;
        s.order = 0;
        s.bits = head + static_cast<std::uint64_t>(n) * s.b;
        for (unsigned order = 0; order <= 4 && order < n; ++order) {
            if (!fixed_residual(x, n, order, m_res)) continue;
            const std::uint64_t bits = head + static_cast<std::uint64_t>(order) * s.b + plan_rice(m_res.data(), n, order, m_rice);
            if (bits < s.bits) {
                s.bits = bits;
                s.type = 2;
                s.order = order;
                s.res.swap(m_res);
                std::swap(s.rice, m_rice);
            }
        }
        if (m_level.max_lpc && n > 16) try_lpc(s, n, head);
    }

    std::uint64_t plan_rice(const std::int64_t* res, unsigned n, unsigned order, RicePlan& plan) {
        const unsigned count = n - order;
        m_u.resize(count);
        for (unsigned i = 0; i < count; ++i) m_u[i] = zigzag(res[i]);
        unsigned pmax = m_level.max_porder;
        while (pmax > 0 && (((n >> pmax) << pmax) != n || (n >> pmax) <= order)) --pmax;
        const unsigned total = (2u << pmax) - 1;
        m_sum.assign(total, 0);
        m_or.assign(total, 0);
        {
            const unsigned parts = 1u << pmax;
            const unsigned psize = n >> pmax;
            std::uint64_t* sum = m_sum.data() + parts - 1;
            std::uint64_t* orv = m_or.data() + parts - 1;
            unsigned idx = 0;
            for (unsigned j = 0; j < parts; ++j) {
                const unsigned cnt = psize - (j == 0 ? order : 0u);
                std::uint64_t s = 0;
                std::uint64_t o = 0;
                for (unsigned k = 0; k < cnt; ++k) {
                    s += m_u[idx];
                    o |= m_u[idx];
                    ++idx;
                }
                sum[j] = s;
                orv[j] = o;
            }
        }
        for (int p = static_cast<int>(pmax) - 1; p >= 0; --p) {
            const unsigned parts = 1u << p;
            std::uint64_t* sum = m_sum.data() + parts - 1;
            std::uint64_t* orv = m_or.data() + parts - 1;
            const std::uint64_t* sum2 = m_sum.data() + 2 * parts - 1;
            const std::uint64_t* or2 = m_or.data() + 2 * parts - 1;
            for (unsigned j = 0; j < parts; ++j) {
                sum[j] = sum2[2 * j] + sum2[2 * j + 1];
                orv[j] = or2[2 * j] | or2[2 * j + 1];
            }
        }
        std::uint64_t best_bits = std::numeric_limits<std::uint64_t>::max();
        unsigned best_p = 0;
        for (unsigned p = 0; p <= pmax; ++p) {
            const unsigned parts = 1u << p;
            const unsigned psize = n >> p;
            const std::uint64_t* sum = m_sum.data() + parts - 1;
            const std::uint64_t* orv = m_or.data() + parts - 1;
            std::uint64_t bits = 6;
            bool wide = false;
            for (unsigned j = 0; j < parts; ++j) {
                const unsigned cnt = psize - (j == 0 ? order : 0u);
                if (cnt == 0) continue;
                const unsigned w = std::max(1u, bit_length(orv[j]));
                std::uint64_t cost = w <= 31 ? 5 + static_cast<std::uint64_t>(cnt) * w : std::numeric_limits<std::uint64_t>::max();
                unsigned kbest = 0;
                const unsigned k0 = rice_guess(sum[j], cnt);
                for (unsigned k = k0 ? k0 - 1 : 0; k <= std::min(30u, k0 + 1); ++k) {
                    const std::uint64_t c = rice_estimate(sum[j], cnt, k);
                    if (c < cost) {
                        cost = c;
                        kbest = k;
                    }
                }
                if (kbest > 14) wide = true;
                bits += cost;
            }
            bits += static_cast<std::uint64_t>(parts) * (wide ? 5u : 4u);
            if (bits < best_bits) {
                best_bits = bits;
                best_p = p;
            }
        }
        const unsigned parts = 1u << best_p;
        const unsigned psize = n >> best_p;
        const std::uint64_t* orv = m_or.data() + parts - 1;
        const std::uint64_t* sum = m_sum.data() + parts - 1;
        plan.porder = best_p;
        plan.param.assign(parts, 0);
        plan.width.assign(parts, 0);
        std::uint64_t bits = 6;
        bool wide = false;
        unsigned idx = 0;
        for (unsigned j = 0; j < parts; ++j) {
            const unsigned cnt = psize - (j == 0 ? order : 0u);
            if (cnt == 0) continue;
            const unsigned k0 = rice_guess(sum[j], cnt);
            const unsigned klo = k0 ? k0 - 1 : 0;
            const unsigned khi = std::min(30u, k0 + 1);
            std::uint64_t acc[3] = { 0, 0, 0 };
            const std::uint64_t* u = m_u.data() + idx;
            for (unsigned i = 0; i < cnt; ++i) {
                for (unsigned k = klo; k <= khi; ++k) acc[k - klo] += u[i] >> k;
            }
            idx += cnt;
            const unsigned w = std::max(1u, bit_length(orv[j]));
            std::uint64_t cost = w <= 31 ? 5 + static_cast<std::uint64_t>(cnt) * w : std::numeric_limits<std::uint64_t>::max();
            unsigned choice = 255;
            for (unsigned k = klo; k <= khi; ++k) {
                const std::uint64_t c = static_cast<std::uint64_t>(cnt) * (k + 1) + acc[k - klo];
                if (c < cost) {
                    cost = c;
                    choice = k;
                }
            }
            plan.param[j] = static_cast<std::uint8_t>(choice);
            if (choice == 255) plan.width[j] = static_cast<std::uint8_t>(w);
            else if (choice > 14) wide = true;
            bits += cost;
        }
        plan.method = wide ? 1u : 0u;
        bits += static_cast<std::uint64_t>(parts) * (wide ? 5u : 4u);
        return bits;
    }

private:
    static bool fixed_residual(const std::int64_t* x, unsigned n, unsigned order, std::vector<std::int64_t>& res) {
        res.resize(n - order);
        std::int64_t* r = res.data();
        for (unsigned i = order; i < n; ++i) {
            std::int64_t v = x[i];
            switch (order) {
                case 1: v -= x[i - 1]; break;
                case 2: v -= 2 * x[i - 1] - x[i - 2]; break;
                case 3: v -= 3 * x[i - 1] - 3 * x[i - 2] + x[i - 3]; break;
                case 4: v -= 4 * x[i - 1] - 6 * x[i - 2] + 4 * x[i - 3] - x[i - 4]; break;
                default: break;
            }
            if (!fits_bits(v, 32)) return false;
            r[i - order] = v;
        }
        return true;
    }

    static void tukey_into(std::vector<double>& w, unsigned start, unsigned end, double p) {
        const unsigned len = end - start;
        const unsigned taper = static_cast<unsigned>(p / 2.0 * len);
        for (unsigned i = 0; i < len; ++i) {
            double v = 1.0;
            if (taper > 0 && i < taper) v = 0.5 - 0.5 * std::cos(k_flac_pi * i / taper);
            else if (taper > 0 && i >= len - taper) v = 0.5 - 0.5 * std::cos(k_flac_pi * (len - 1 - i) / taper);
            w[start + i] = v;
        }
    }

    void add_window(unsigned n, unsigned start, unsigned end, double taper) {
        std::vector<double> w(n, 0.0);
        tukey_into(w, start, end, taper);
        m_windows.push_back(std::move(w));
    }

    void build_windows(unsigned n) {
        if (m_window_n == n) return;
        m_window_n = n;
        m_windows.clear();
        const unsigned mask = m_level.windows ? m_level.windows : k_window_tukey;
        if (mask & k_window_tukey) add_window(n, 0, n, 0.5);
        if (mask & k_window_hann) add_window(n, 0, n, 1.0);
        if (mask & k_window_flat) add_window(n, 0, n, 0.1);
        if (mask & k_window_halves) {
            add_window(n, 0, n / 2, 0.3);
            add_window(n, n / 2, n, 0.3);
        }
        if (mask & k_window_thirds) {
            for (unsigned part = 0; part < 3; ++part) add_window(n, n * part / 3, n * (part + 1) / 3, 0.3);
        }
        if (mask & k_window_edges) {
            add_window(n, 0, n * 3 / 4, 0.3);
            add_window(n, n / 4, n, 0.3);
        }
    }

    static unsigned levinson(const double* r, unsigned max_order, double lp[32][32], double* err) {
        double a[33] = {};
        double tmp[33] = {};
        double e = r[0];
        for (unsigned m = 1; m <= max_order; ++m) {
            double acc = r[m];
            for (unsigned j = 1; j < m; ++j) acc -= a[j] * r[m - j];
            const double k = acc / e;
            if (!std::isfinite(k)) return m - 1;
            for (unsigned j = 1; j < m; ++j) tmp[j] = a[j] - k * a[m - j];
            for (unsigned j = 1; j < m; ++j) a[j] = tmp[j];
            a[m] = k;
            e *= 1.0 - k * k;
            for (unsigned j = 0; j < m; ++j) lp[m - 1][j] = a[j + 1];
            err[m - 1] = e;
            if (!(e > 0.0)) return m;
        }
        return max_order;
    }

    static bool quantize(const double* lp, unsigned order, unsigned precision, std::int32_t* qc, int& shift) {
        double cmax = 0.0;
        for (unsigned j = 0; j < order; ++j) cmax = std::max(cmax, std::fabs(lp[j]));
        if (!(cmax > 0.0) || !std::isfinite(cmax)) return false;
        int log2cmax = 0;
        std::frexp(cmax, &log2cmax);
        --log2cmax;
        int sh = static_cast<int>(precision) - 2 - log2cmax;
        if (sh > 15) sh = 15;
        if (sh < 0) return false;
        const double qmax = static_cast<double>((1 << (precision - 1)) - 1);
        const double qmin = -static_cast<double>(1 << (precision - 1));
        const double scale = std::ldexp(1.0, sh);
        double error = 0.0;
        for (unsigned j = 0; j < order; ++j) {
            error += lp[j] * scale;
            double q = std::floor(error + 0.5);
            if (q > qmax) q = qmax;
            if (q < qmin) q = qmin;
            error -= q;
            qc[j] = static_cast<std::int32_t>(q);
        }
        shift = sh;
        return true;
    }

    static bool lpc_residual(const std::int64_t* x, unsigned n, unsigned order, const std::int32_t* qc, int shift, std::vector<std::int64_t>& res) {
        res.resize(n - order);
        std::int64_t* r = res.data();
        const unsigned sh = static_cast<unsigned>(shift);
        for (unsigned i = order; i < n; ++i) {
            std::int64_t sum = 0;
            const std::int64_t* hist = x + i;
            for (unsigned j = 0; j < order; ++j) sum += static_cast<std::int64_t>(qc[j]) * hist[-1 - static_cast<std::ptrdiff_t>(j)];
            const std::int64_t v = x[i] - shift_down(sum, sh);
            if (!fits_bits(v, 32)) return false;
            r[i - order] = v;
        }
        return true;
    }

    void try_lpc(EncSignal& s, unsigned n, std::uint64_t head) {
        const unsigned max_order = std::min(m_level.max_lpc, n - 1);
        if (max_order == 0) return;
        build_windows(n);
        const std::int64_t* x = s.x.data();
        m_wd.resize(n);
        double lp[32][32];
        double err[32];
        double autoc[33];
        for (const std::vector<double>& win : m_windows) {
            for (unsigned i = 0; i < n; ++i) m_wd[i] = static_cast<double>(x[i]) * win[i];
            for (unsigned l = 0; l <= max_order; ++l) {
                double acc = 0.0;
                for (unsigned i = l; i < n; ++i) acc += m_wd[i] * m_wd[i - l];
                autoc[l] = acc;
            }
            if (!(autoc[0] > 0.0) || !std::isfinite(autoc[0])) continue;
            const unsigned usable = levinson(autoc, max_order, lp, err);
            if (usable == 0) continue;
            double estimate[32];
            unsigned centre[32];
            for (unsigned o = 1; o <= usable; ++o) {
                const double e = err[o - 1];
                double cmax = 0.0;
                for (unsigned j = 0; j < o; ++j) cmax = std::max(cmax, std::fabs(lp[o - 1][j]));
                centre[o - 1] = cmax > 0.0 && std::isfinite(cmax) ? qlp_precision(s.b, n, o, e > 0.0 ? autoc[0] / e : 1e12, cmax) : 0u;
                const double per_sample = e > 0.0 ? std::max(1.0, 0.5 * std::log2(e / n) + 1.5) : 1.0;
                estimate[o - 1] = per_sample * (n - o) + static_cast<double>(o) * (s.b + centre[o - 1]);
            }
            for (unsigned c = 0; c < m_level.candidates && c < usable; ++c) {
                unsigned pick = 0;
                double pv = std::numeric_limits<double>::infinity();
                for (unsigned o = 0; o < usable; ++o) {
                    if (estimate[o] < pv) {
                        pv = estimate[o];
                        pick = o;
                    }
                }
                if (!std::isfinite(pv)) break;
                estimate[pick] = std::numeric_limits<double>::infinity();
                const unsigned order = pick + 1;
                if (centre[pick] == 0) continue;
                const unsigned span = m_level.precision_span;
                const unsigned lo = centre[pick] > 4u + span ? centre[pick] - span : 4u;
                const unsigned hi = std::min(15u, centre[pick] + span);
                for (unsigned precision = lo; precision <= hi; ++precision) {
                    std::int32_t qc[32];
                    int shift = 0;
                    if (!quantize(lp[pick], order, precision, qc, shift)) continue;
                    unsigned used = 1;
                    for (unsigned j = 0; j < order; ++j) {
                        while (!fits_bits(qc[j], used)) ++used;
                    }
                    if (used > 15) continue;
                    if (!lpc_residual(x, n, order, qc, shift, m_res)) continue;
                    const std::uint64_t bits = head + static_cast<std::uint64_t>(order) * s.b + 9 + static_cast<std::uint64_t>(order) * used + plan_rice(m_res.data(), n, order, m_rice);
                    if (bits < s.bits) {
                        s.bits = bits;
                        s.type = 3;
                        s.order = order;
                        s.precision = used;
                        s.shift = shift;
                        std::copy(qc, qc + order, s.coefs);
                        s.res.swap(m_res);
                        std::swap(s.rice, m_rice);
                    }
                }
            }
        }
    }

    FlacLevel m_level = flac_level(5);
    unsigned m_stream_bps = 16;
    std::vector<std::uint64_t> m_u;
    std::vector<std::uint64_t> m_sum;
    std::vector<std::uint64_t> m_or;
    std::vector<std::int64_t> m_res;
    RicePlan m_rice;
    std::vector<double> m_wd;
    std::vector<std::vector<double>> m_windows;
    unsigned m_window_n = 0;
};

void put_residual(FlacBitWriter& w, const EncSignal& s, unsigned n) {
    const RicePlan& plan = s.rice;
    w.put(plan.method, 2);
    w.put(plan.porder, 4);
    const unsigned pb = plan.method ? 5u : 4u;
    const unsigned escape = plan.method ? 31u : 15u;
    const unsigned parts = 1u << plan.porder;
    const unsigned psize = n >> plan.porder;
    const std::int64_t* r = s.res.data();
    std::size_t idx = 0;
    for (unsigned j = 0; j < parts; ++j) {
        const unsigned cnt = psize - (j == 0 ? s.order : 0u);
        const unsigned k = plan.param[j];
        if (k == 255) {
            const unsigned width = plan.width[j];
            w.put(escape, pb);
            w.put(width, 5);
            for (unsigned i = 0; i < cnt; ++i) w.sput(r[idx++], width);
        } else {
            w.put(k, pb);
            const std::uint64_t mask = (std::uint64_t(1) << k) - 1;
            for (unsigned i = 0; i < cnt; ++i) {
                const std::uint64_t u = zigzag(r[idx++]);
                w.unary(u >> k);
                w.put(static_cast<std::uint32_t>(u & mask), k);
            }
        }
    }
}

void put_subframe(FlacBitWriter& w, const EncSignal& s, unsigned n) {
    w.put(0, 1);
    unsigned code = 1;
    if (s.type == 0) code = 0;
    else if (s.type == 2) code = 8 + s.order;
    else if (s.type == 3) code = 32 + s.order - 1;
    w.put(code, 6);
    if (s.wasted) {
        w.put(1, 1);
        w.unary(s.wasted - 1);
    } else {
        w.put(0, 1);
    }
    const std::int64_t* x = s.x.data();
    if (s.type == 0) {
        w.sput(x[0], s.b);
        return;
    }
    if (s.type == 1) {
        for (unsigned i = 0; i < n; ++i) w.sput(x[i], s.b);
        return;
    }
    for (unsigned i = 0; i < s.order; ++i) w.sput(x[i], s.b);
    if (s.type == 3) {
        w.put(s.precision - 1, 4);
        w.sput(s.shift, 5);
        for (unsigned j = 0; j < s.order; ++j) w.sput(s.coefs[j], s.precision);
    }
    put_residual(w, s, n);
}

void put_utf8(FlacBitWriter& w, std::uint64_t v) {
    if (v < 0x80) {
        w.put(static_cast<std::uint32_t>(v), 8);
        return;
    }
    unsigned bytes = 7;
    if (v < 0x800) bytes = 2;
    else if (v < 0x10000) bytes = 3;
    else if (v < 0x200000) bytes = 4;
    else if (v < 0x4000000) bytes = 5;
    else if (v < 0x80000000ull) bytes = 6;
    if (bytes == 7) {
        w.put(0xFE, 8);
    } else {
        const unsigned prefix = (0xFF00u >> bytes) & 0xFFu;
        w.put(static_cast<std::uint32_t>(prefix | (v >> (6 * (bytes - 1)))), 8);
    }
    for (unsigned k = bytes - 1; k-- > 0;) w.put(static_cast<std::uint32_t>(0x80u | ((v >> (6 * k)) & 0x3Fu)), 8);
}

std::string tag_key(const std::string& key) {
    std::string out;
    for (char c : key) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7D || c == '=') out += '_';
        else if (c >= 'a' && c <= 'z') out += static_cast<char>(c - 'a' + 'A');
        else out += c;
    }
    if (out == "TRACK") out = "TRACKNUMBER";
    return out;
}

} // namespace

struct FlacWriter::Impl {
    detail::File file;
    std::vector<std::uint8_t>* memory = nullptr;
    std::string path;
    unsigned channels = 0;
    std::uint32_t rate = 0;
    unsigned bps = 16;
    unsigned block = 4096;
    bool verify = false;
    bool dither = true;
    std::uint32_t noise = 0x2545F491u;
    FlacAnalyzer analyzer;
    EncSignal sig[k_flac_max_channels];
    FlacBitWriter bw;
    FlacMd5 md5;
    std::vector<std::uint8_t> md5_bytes;
    std::vector<std::int32_t> pending;
    std::size_t pending_frames = 0;
    std::vector<std::int32_t> converted;
    std::uint64_t frames = 0;
    std::uint64_t frame_number = 0;
    std::uint64_t audio_bytes = 0;
    std::uint32_t min_frame = 0xFFFFFFFFu;
    std::uint32_t max_frame = 0;
    std::uint64_t header_size = 0;
    std::size_t seek_reserved = 0;
    std::uint64_t seek_interval = 0;
    std::uint64_t next_seek = 0;
    std::vector<FlacSeekPoint> seek_points;
    StreamParams sp;
    FrameWorkspace verify_ws;
    bool failed = false;
    std::string error;

    bool emit(const std::uint8_t* data, std::size_t n) {
        if (memory) {
            memory->insert(memory->end(), data, data + n);
            return true;
        }
        return file.write(data, n);
    }

    bool patch(std::uint64_t offset, const std::uint8_t* data, std::size_t n) {
        if (memory) {
            if (offset + n > memory->size()) return false;
            std::memcpy(memory->data() + offset, data, n);
            return true;
        }
        const std::int64_t here = file.tell();
        return file.seek(static_cast<std::int64_t>(offset)) && file.write(data, n) && file.seek(here);
    }

    bool start(std::size_t ch, std::uint32_t sample_rate, const FlacEncodeOptions& options, std::uint64_t expected, std::string* err) {
        auto fail = [&](const char* message) {
            if (err) *err = message;
            return false;
        };
        if (ch == 0 || ch > k_flac_max_channels) return fail("FLAC supports 1 to 8 channels");
        if (sample_rate == 0 || sample_rate > 1048575u) return fail("FLAC sample rates must be between 1 and 1048575 Hz");
        if (options.bits_per_sample < 4 || options.bits_per_sample > 32) return fail("FLAC sample sizes must be between 4 and 32 bits");
        const FlacLevel level = flac_level(options.compression);
        unsigned blk = options.block_size ? options.block_size : level.block;
        if (!options.block_size && sample_rate > 48000) blk *= 2;
        if (blk < 16 || blk > 65535) return fail("FLAC block sizes must be between 16 and 65535");
        channels = static_cast<unsigned>(ch);
        rate = sample_rate;
        bps = options.bits_per_sample;
        block = blk;
        verify = options.verify;
        dither = options.dither;
        analyzer.configure(level, bps);
        sp.rate = rate;
        sp.channels = channels;
        sp.bps = bps;
        sp.min_block = block;
        sp.max_block = block;
        pending.assign(static_cast<std::size_t>(block) * channels, 0);
        pending_frames = 0;
        seek_interval = static_cast<std::uint64_t>(rate) * 10;
        seek_reserved = expected ? static_cast<std::size_t>(std::min<std::uint64_t>(expected / seek_interval + 1, 65536)) : 0;
        std::vector<std::uint8_t> h;
        h.insert(h.end(), { 'f', 'L', 'a', 'C' });
        h.insert(h.end(), { 0x00, 0x00, 0x00, 34 });
        h.insert(h.end(), 34, 0);
        if (seek_reserved) {
            const std::uint32_t len = static_cast<std::uint32_t>(seek_reserved * 18);
            h.push_back(0x03);
            h.push_back(static_cast<std::uint8_t>(len >> 16));
            h.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFFu));
            h.push_back(static_cast<std::uint8_t>(len & 0xFFu));
            for (std::size_t i = 0; i < seek_reserved; ++i) {
                h.insert(h.end(), 8, 0xFF);
                h.insert(h.end(), 10, 0x00);
            }
        }
        std::vector<std::uint8_t> vc;
        const std::string vendor = "fizmo FLAC encoder";
        detail::append_le32(vc, static_cast<std::uint32_t>(vendor.size()));
        vc.insert(vc.end(), vendor.begin(), vendor.end());
        std::vector<std::string> entries;
        std::size_t room = 0xFFFFFFu - vc.size() - 4;
        for (const auto& tag : options.tags) {
            const std::string key = tag_key(tag.first);
            if (key.empty()) continue;
            std::string entry = key + "=" + tag.second;
            if (entry.size() + 4 > room) continue;
            room -= entry.size() + 4;
            entries.push_back(std::move(entry));
        }
        detail::append_le32(vc, static_cast<std::uint32_t>(entries.size()));
        for (const std::string& e : entries) {
            detail::append_le32(vc, static_cast<std::uint32_t>(e.size()));
            vc.insert(vc.end(), e.begin(), e.end());
        }
        h.push_back(0x84);
        h.push_back(static_cast<std::uint8_t>(vc.size() >> 16));
        h.push_back(static_cast<std::uint8_t>((vc.size() >> 8) & 0xFFu));
        h.push_back(static_cast<std::uint8_t>(vc.size() & 0xFFu));
        h.insert(h.end(), vc.begin(), vc.end());
        header_size = h.size();
        write_streaminfo(h.data() + 8);
        if (!emit(h.data(), h.size())) return fail("can't write the FLAC header");
        return true;
    }

    void write_streaminfo(std::uint8_t* d) {
        std::memset(d, 0, 34);
        put_be(d, block, 2);
        put_be(d + 2, block, 2);
        put_be(d + 4, frame_number ? min_frame : 0, 3);
        put_be(d + 7, frame_number ? max_frame : 0, 3);
        const std::uint64_t total = frames < (std::uint64_t(1) << 36) ? frames : 0;
        const std::uint64_t packed = (static_cast<std::uint64_t>(rate) << 44) | (static_cast<std::uint64_t>(channels - 1) << 41) | (static_cast<std::uint64_t>(bps - 1) << 36) | total;
        put_be(d + 10, packed, 8);
    }

    double tpdf() noexcept {
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        const double a = static_cast<double>(noise & 0xFFFFFFu) / 16777216.0;
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        const double b = static_cast<double>(noise & 0xFFFFFFu) / 16777216.0;
        return a - b;
    }

    bool write_float(const float* in, std::size_t n) {
        const double scale = static_cast<double>(std::uint64_t(1) << (bps - 1));
        const double hi = scale - 1.0;
        const double lo = -scale;
        const std::size_t chunk = 8192;
        std::size_t done = 0;
        while (done < n) {
            const std::size_t m = std::min(chunk, n - done);
            converted.resize(m * channels);
            const float* src = in + done * channels;
            for (std::size_t i = 0; i < m * channels; ++i) {
                double v = static_cast<double>(src[i]);
                if (!(v == v)) v = 0.0;
                v = std::max(-1.0, std::min(1.0, v)) * scale;
                if (dither && v != std::floor(v)) v += tpdf();
                v = std::floor(v + 0.5);
                v = std::max(lo, std::min(hi, v));
                converted[i] = static_cast<std::int32_t>(v);
            }
            if (!write_pcm(converted.data(), m)) return false;
            done += m;
        }
        return true;
    }

    bool write_pcm(const std::int32_t* in, std::size_t n) {
        if (failed) return false;
        const std::int64_t lim = std::int64_t(1) << (bps - 1);
        std::size_t done = 0;
        while (done < n) {
            const std::size_t m = std::min<std::size_t>(block - pending_frames, n - done);
            const std::int32_t* src = in + done * channels;
            std::int32_t* dst = pending.data() + pending_frames * channels;
            for (std::size_t i = 0; i < m * channels; ++i) {
                std::int64_t v = src[i];
                if (v < -lim) v = -lim;
                if (v >= lim) v = lim - 1;
                dst[i] = static_cast<std::int32_t>(v);
            }
            pending_frames += m;
            done += m;
            if (pending_frames == block) {
                if (!encode_block(pending.data(), block)) return false;
                pending_frames = 0;
            }
        }
        return true;
    }

    bool encode_block(const std::int32_t* in, unsigned n) {
        if (frame_number >= 0x7FFFFFFFull) {
            failed = true;
            error = "the FLAC stream is too long";
            return false;
        }
        md5_samples(md5, in, static_cast<std::size_t>(n) * channels, bps, md5_bytes);
        for (unsigned c = 0; c < channels; ++c) {
            EncSignal& s = sig[c];
            s.x.resize(n);
            s.bps = bps;
            for (unsigned i = 0; i < n; ++i) s.x[i] = in[static_cast<std::size_t>(i) * channels + c];
        }
        unsigned count = channels;
        if (channels == 2) {
            EncSignal& side = sig[2];
            EncSignal& mid = sig[3];
            side.x.resize(n);
            mid.x.resize(n);
            side.bps = bps + 1;
            mid.bps = bps;
            for (unsigned i = 0; i < n; ++i) {
                const std::int64_t l = sig[0].x[i];
                const std::int64_t r = sig[1].x[i];
                side.x[i] = l - r;
                mid.x[i] = shift_down(l + r, 1);
            }
            count = 4;
        }
        for (unsigned c = 0; c < count; ++c) analyzer.analyze(sig[c], n);
        unsigned assignment = channels - 1;
        const EncSignal* order[k_flac_max_channels];
        for (unsigned c = 0; c < channels; ++c) order[c] = &sig[c];
        if (channels == 2) {
            const std::uint64_t costs[4] = { sig[0].bits + sig[1].bits, sig[0].bits + sig[2].bits, sig[2].bits + sig[1].bits, sig[3].bits + sig[2].bits };
            unsigned best = 0;
            for (unsigned i = 1; i < 4; ++i) {
                if (costs[i] < costs[best]) best = i;
            }
            if (best == 1) {
                assignment = 8;
                order[1] = &sig[2];
            } else if (best == 2) {
                assignment = 9;
                order[0] = &sig[2];
            } else if (best == 3) {
                assignment = 10;
                order[0] = &sig[3];
                order[1] = &sig[2];
            }
        }
        bw.clear();
        bw.put(0xFF, 8);
        bw.put(0xF8, 8);
        unsigned bs_code = 7;
        if (n == 192) bs_code = 1;
        else if (n == 576 || n == 1152 || n == 2304 || n == 4608) bs_code = 2 + static_cast<unsigned>(bit_length(n / 576) - 1);
        else if (n >= 256 && n <= 32768 && (n & (n - 1)) == 0) bs_code = 8 + bit_length(n / 256) - 1;
        else if (n <= 256) bs_code = 6;
        unsigned sr_code = 0;
        static const std::uint32_t rates[12] = { 0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };
        for (unsigned i = 1; i < 12; ++i) {
            if (rates[i] == rate) sr_code = i;
        }
        if (sr_code == 0) {
            if (rate % 1000 == 0 && rate / 1000 <= 255) sr_code = 12;
            else if (rate <= 65535) sr_code = 13;
            else if (rate % 10 == 0 && rate / 10 <= 65535) sr_code = 14;
        }
        unsigned ss_code = 0;
        switch (bps) {
            case 8: ss_code = 1; break;
            case 12: ss_code = 2; break;
            case 16: ss_code = 4; break;
            case 20: ss_code = 5; break;
            case 24: ss_code = 6; break;
            case 32: ss_code = 7; break;
            default: break;
        }
        bw.put(bs_code, 4);
        bw.put(sr_code, 4);
        bw.put(assignment, 4);
        bw.put(ss_code, 3);
        bw.put(0, 1);
        put_utf8(bw, frame_number);
        if (bs_code == 6) bw.put(n - 1, 8);
        else if (bs_code == 7) bw.put(n - 1, 16);
        if (sr_code == 12) bw.put(rate / 1000, 8);
        else if (sr_code == 13) bw.put(rate, 16);
        else if (sr_code == 14) bw.put(rate / 10, 16);
        bw.put(flac_crc8(bw.bytes().data(), bw.bytes().size()), 8);
        for (unsigned c = 0; c < channels; ++c) put_subframe(bw, *order[c], n);
        bw.align();
        bw.put(flac_crc16(bw.bytes().data(), bw.bytes().size()), 16);
        const std::vector<std::uint8_t>& bytes = bw.bytes();
        if (verify) {
            FrameHeader h;
            std::size_t len = 0;
            const FrameStatus st = decode_frame(bytes.data(), bytes.size(), sp, h, verify_ws, len);
            bool same = st == FrameStatus::ok && len == bytes.size() && h.block == n;
            if (same) same = std::memcmp(verify_ws.out.data(), in, static_cast<std::size_t>(n) * channels * sizeof(std::int32_t)) == 0;
            if (!same) {
                failed = true;
                error = "FLAC verification failed at frame " + std::to_string(frame_number);
                return false;
            }
        }
        if (!emit(bytes.data(), bytes.size())) {
            failed = true;
            error = "can't write the FLAC audio";
            return false;
        }
        const std::uint64_t first = frames;
        if (seek_reserved) {
            while (next_seek < first + n) {
                if (next_seek >= first && seek_points.size() < seek_reserved && (seek_points.empty() || seek_points.back().sample != first)) {
                    FlacSeekPoint pt;
                    pt.sample = first;
                    pt.offset = audio_bytes;
                    pt.frames = n;
                    seek_points.push_back(pt);
                }
                next_seek += seek_interval;
            }
        }
        const std::uint32_t size = static_cast<std::uint32_t>(std::min<std::size_t>(bytes.size(), 0xFFFFFFu));
        min_frame = std::min(min_frame, size);
        max_frame = std::max(max_frame, size);
        audio_bytes += bytes.size();
        frames += n;
        ++frame_number;
        return true;
    }

    bool finish(std::string* err) {
        bool ok = !failed;
        if (ok && pending_frames) {
            ok = encode_block(pending.data(), static_cast<unsigned>(pending_frames));
            pending_frames = 0;
        }
        std::uint8_t info[34];
        write_streaminfo(info);
        if (ok) md5.finish(info + 18);
        ok = patch(8, info, 34) && ok;
        if (seek_reserved) {
            std::vector<std::uint8_t> table(seek_reserved * 18, 0);
            for (std::size_t i = 0; i < seek_reserved; ++i) {
                std::uint8_t* p = table.data() + 18 * i;
                if (i < seek_points.size()) {
                    put_be(p, seek_points[i].sample, 8);
                    put_be(p + 8, seek_points[i].offset, 8);
                    put_be(p + 16, seek_points[i].frames, 2);
                } else {
                    put_be(p, k_flac_placeholder, 8);
                }
            }
            ok = patch(8 + 34 + 4, table.data(), table.size()) && ok;
        }
        if (!memory) {
            ok = file.flush() && ok;
            file.close();
        }
        if (!ok && err) *err = error.empty() ? "can't finish writing the FLAC file" : error;
        return ok;
    }
};

FlacWriter::FlacWriter() = default;

FlacWriter::~FlacWriter() {
    close();
}

FlacWriter::FlacWriter(FlacWriter&&) noexcept = default;

FlacWriter& FlacWriter::operator=(FlacWriter&& other) noexcept {
    if (this != &other) {
        close();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool FlacWriter::open(const std::string& path, std::size_t channels, std::uint32_t sample_rate, const FlacEncodeOptions& options, std::string* error, std::uint64_t expected_frames) {
    close();
    auto impl = std::make_unique<Impl>();
    if (!impl->file.open(path, "wb")) {
        if (error) *error = "can't create " + path;
        return false;
    }
    impl->path = path;
    if (!impl->start(channels, sample_rate, options, expected_frames, error)) {
        impl->file.close();
        return false;
    }
    m_impl = std::move(impl);
    return true;
}

bool FlacWriter::write(const float* interleaved, std::size_t frames) {
    if (!m_impl || m_impl->failed) return false;
    if (frames == 0) return true;
    if (!interleaved) return false;
    return m_impl->write_float(interleaved, frames);
}

bool FlacWriter::write(const AudioBuffer& buffer) {
    if (!m_impl) return false;
    if (buffer.channels() != m_impl->channels) {
        const AudioBuffer remixed = buffer.with_channels(m_impl->channels);
        return write(remixed.data(), remixed.frames());
    }
    return write(buffer.data(), buffer.frames());
}

bool FlacWriter::write_pcm(const std::int32_t* interleaved, std::size_t frames) {
    if (!m_impl || m_impl->failed) return false;
    if (frames == 0) return true;
    if (!interleaved) return false;
    return m_impl->write_pcm(interleaved, frames);
}

bool FlacWriter::close(std::string* error) {
    if (!m_impl) return true;
    const bool ok = m_impl->finish(error);
    m_impl.reset();
    return ok;
}

bool FlacWriter::is_open() const noexcept {
    return m_impl != nullptr;
}

std::uint64_t FlacWriter::frames_written() const noexcept {
    return m_impl ? m_impl->frames + m_impl->pending_frames : 0;
}

std::vector<std::uint8_t> encode_flac(const AudioBuffer& buffer, const FlacEncodeOptions& options, std::string* error) {
    std::vector<std::uint8_t> out;
    FlacWriter::Impl impl;
    impl.memory = &out;
    if (!impl.start(buffer.channels(), buffer.sample_rate(), options, buffer.frames(), error)) return {};
    if (buffer.frames() && !impl.write_float(buffer.data(), buffer.frames())) {
        if (error) *error = impl.error;
        return {};
    }
    if (!impl.finish(error)) return {};
    return out;
}

std::vector<std::uint8_t> encode_flac(const AudioBuffer& buffer, const FlacEncodeOptions& options) {
    return encode_flac(buffer, options, nullptr);
}

bool write_flac(const std::string& path, const AudioBuffer& buffer, const FlacEncodeOptions& options, std::string* error) {
    if (buffer.channels() == 0 || buffer.sample_rate() == 0) {
        if (error) *error = "the buffer has no channels or no sample rate";
        return false;
    }
    FlacWriter w;
    if (!w.open(path, buffer.channels(), buffer.sample_rate(), options, error, buffer.frames())) return false;
    if (!w.write(buffer)) {
        std::string ignored;
        w.close(&ignored);
        if (error) *error = ignored.empty() ? "can't write " + path : ignored;
        return false;
    }
    return w.close(error);
}

struct FlacDecoder::Impl {
    StreamParams sp;
    std::uint64_t total = 0;
    FrameWorkspace ws;
};

FlacDecoder::FlacDecoder() = default;
FlacDecoder::~FlacDecoder() = default;
FlacDecoder::FlacDecoder(FlacDecoder&&) noexcept = default;
FlacDecoder& FlacDecoder::operator=(FlacDecoder&&) noexcept = default;

bool FlacDecoder::open(const std::uint8_t* config, std::size_t size, std::string* error) {
    close();
    std::size_t pos = 0;
    if (config && size >= 4 && std::memcmp(config, "fLaC", 4) == 0) pos = 4;
    const std::uint8_t* info = nullptr;
    if (config && size >= pos + 4 && (config[pos] & 0x7Fu) == 0 && read_be24(config + pos + 1) >= 34 && size >= pos + 4 + 34) {
        info = config + pos + 4;
    } else if (config && pos == 0 && size == 34) {
        info = config;
    }
    if (!info) {
        if (error) *error = "the FLAC STREAMINFO block is missing";
        return false;
    }
    std::unique_ptr<Impl> impl(new Impl());
    impl->sp.min_block = (static_cast<unsigned>(info[0]) << 8) | info[1];
    impl->sp.max_block = (static_cast<unsigned>(info[2]) << 8) | info[3];
    impl->sp.rate = (static_cast<std::uint32_t>(info[10]) << 12) | (static_cast<std::uint32_t>(info[11]) << 4) | (info[12] >> 4);
    impl->sp.channels = ((info[12] >> 1) & 7u) + 1;
    impl->sp.bps = (((info[12] & 1u) << 4) | (info[13] >> 4)) + 1;
    impl->total = (static_cast<std::uint64_t>(info[13] & 15u) << 32) | (static_cast<std::uint64_t>(info[14]) << 24) | (static_cast<std::uint64_t>(info[15]) << 16) | (static_cast<std::uint64_t>(info[16]) << 8) | info[17];
    if (impl->sp.rate == 0 || impl->sp.bps < 4) {
        if (error) *error = "invalid FLAC STREAMINFO";
        return false;
    }
    m_impl = std::move(impl);
    return true;
}

void FlacDecoder::close() {
    m_impl.reset();
}

bool FlacDecoder::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t FlacDecoder::decode(const std::uint8_t* frame, std::size_t size, std::vector<float>& out, std::string* error) {
    out.clear();
    if (!m_impl) {
        if (error) *error = "the FLAC decoder isn't open";
        return 0;
    }
    if (!frame || size == 0) {
        if (error) *error = "empty FLAC frame";
        return 0;
    }
    if (error) error->clear();
    Impl& m = *m_impl;
    FrameHeader h;
    std::size_t len = 0;
    FrameStatus st = FrameStatus::corrupt;
    try {
        st = decode_frame(frame, size, m.sp, h, m.ws, len);
    } catch (const std::bad_alloc&) {
        if (error) *error = "out of memory";
        return 0;
    }
    if (st != FrameStatus::ok) {
        if (error) *error = st == FrameStatus::crc ? "FLAC frame CRC mismatch" : "corrupt FLAC frame";
        return 0;
    }
    const std::size_t n = h.block;
    const std::size_t count = n * h.channels;
    const double scale = 1.0 / static_cast<double>(std::uint64_t(1) << (h.bps - 1));
    out.resize(count);
    for (std::size_t i = 0; i < count; ++i) out[i] = static_cast<float>(m.ws.out[i] * scale);
    return n;
}

void FlacDecoder::reset() {
}

std::uint32_t FlacDecoder::sample_rate() const noexcept {
    return m_impl ? m_impl->sp.rate : 0;
}

std::uint16_t FlacDecoder::channels() const noexcept {
    return m_impl ? static_cast<std::uint16_t>(m_impl->sp.channels) : 0;
}

std::uint16_t FlacDecoder::bits_per_sample() const noexcept {
    return m_impl ? static_cast<std::uint16_t>(m_impl->sp.bps) : 0;
}

std::uint64_t FlacDecoder::total_frames() const noexcept {
    return m_impl ? m_impl->total : 0;
}

std::uint32_t FlacDecoder::fixed_block_size() const noexcept {
    if (!m_impl || m_impl->sp.min_block != m_impl->sp.max_block) return 0;
    return m_impl->sp.max_block;
}

} // namespace audio
} // namespace fizmo
