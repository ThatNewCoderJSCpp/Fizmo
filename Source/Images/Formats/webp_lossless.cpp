#include "fizmo_library.hpp"
#include "webp_internal.hpp"

namespace fizmo {
namespace images {
namespace webp_detail {

namespace {

const std::uint8_t kCodeLengthCodeOrder[19] = { 17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

const std::uint8_t kCodeToPlane[120] = {
    0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a, 0x26, 0x2a, 0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a,
    0x25, 0x2b, 0x48, 0x04, 0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b, 0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45, 0x4b, 0x34, 0x3c, 0x03,
    0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d, 0x44, 0x4c, 0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
    0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b, 0x32, 0x3e, 0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f,
    0x64, 0x6c, 0x42, 0x4e, 0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b, 0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e, 0x00, 0x74, 0x7c, 0x41,
    0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d, 0x51, 0x5f, 0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70
};

class BitReader {
public:
    BitReader(const std::uint8_t* p, std::size_t n) : m_p(p), m_n(n) { refill(); }

    std::uint32_t peek(unsigned n) {
        if (m_bits < n) refill();
        return static_cast<std::uint32_t>(m_acc & ((1ull << n) - 1));
    }

    void consume(unsigned n) {
        if (m_bits < n) refill();
        m_acc >>= n;
        m_bits -= n;
        m_consumed += n;
        if (m_consumed > static_cast<std::uint64_t>(m_n) * 8) m_eos = true;
    }

    std::uint32_t read(unsigned n) {
        if (n == 0) return 0;
        const std::uint32_t v = peek(n);
        consume(n);
        return v;
    }

    bool eos() const noexcept { return m_eos; }

private:
    void refill() {
        while (m_bits <= 56) {
            if (m_pos < m_n) m_acc |= static_cast<std::uint64_t>(m_p[m_pos++]) << m_bits;
            m_bits += 8;
        }
    }

    const std::uint8_t* m_p;
    std::size_t         m_n;
    std::size_t         m_pos = 0;
    std::uint64_t       m_acc = 0;
    unsigned            m_bits = 0;
    std::uint64_t       m_consumed = 0;
    bool                m_eos = false;
};

class Huffman {
public:
    bool build(const int* lengths, int n) {
        m_table.clear();
        int count[16] = {};
        int used = 0, single = -1;

        for (int s = 0; s < n; ++s) {
            if (lengths[s] < 0 || lengths[s] > 15) return false;
            ++count[lengths[s]];
            if (lengths[s]) {
                ++used;
                single = s;
            }
        }

        if (used == 0) return false;

        if (used == 1) {
            m_table.assign(1u << kRoot, Entry{ 0, static_cast<std::uint16_t>(single) });
            return true;
        }

        std::int64_t space = 1ll << 15;
        for (int l = 1; l <= 15; ++l) space -= static_cast<std::int64_t>(count[l]) << (15 - l);
        if (space != 0) return false;

        int next[16] = {};
        int code = 0;
        for (int l = 1; l <= 15; ++l) {
            next[l] = code;
            code = (code + count[l]) << 1;
        }

        std::vector<std::uint32_t> rev(static_cast<std::size_t>(n), 0);
        int sub_max[1 << kRoot] = {};

        for (int s = 0; s < n; ++s) {
            const int l = lengths[s];
            if (!l) continue;
            const std::uint32_t c = static_cast<std::uint32_t>(next[l]++);
            std::uint32_t r = 0;
            for (int b = 0; b < l; ++b) r |= ((c >> b) & 1u) << (l - 1 - b);
            rev[static_cast<std::size_t>(s)] = r;
            if (l > static_cast<int>(kRoot)) {
                const std::uint32_t low = r & ((1u << kRoot) - 1);
                sub_max[low] = std::max(sub_max[low], l - static_cast<int>(kRoot));
            }
        }

        m_table.assign(1u << kRoot, Entry{ 0, 0 });

        for (std::uint32_t low = 0; low < (1u << kRoot); ++low) {
            if (!sub_max[low]) continue;
            const std::size_t off = m_table.size();
            m_table[low] = Entry{ static_cast<std::uint8_t>(0x80 | sub_max[low]), static_cast<std::uint16_t>(off - (1u << kRoot)) };
            m_table.resize(off + (1u << sub_max[low]), Entry{ 0, 0 });
        }

        for (int s = 0; s < n; ++s) {
            const int l = lengths[s];
            if (!l) continue;
            const std::uint32_t r = rev[static_cast<std::size_t>(s)];

            if (l <= static_cast<int>(kRoot)) {
                for (std::uint32_t j = r; j < (1u << kRoot); j += 1u << l) m_table[j] = Entry{ static_cast<std::uint8_t>(l), static_cast<std::uint16_t>(s) };
            } else {
                const std::uint32_t low = r & ((1u << kRoot) - 1);
                const Entry root = m_table[low];
                const std::size_t base = (1u << kRoot) + root.value;
                const unsigned sub_bits = root.bits & 0x7F;
                const unsigned rest = static_cast<unsigned>(l) - kRoot;
                for (std::uint32_t j = r >> kRoot; j < (1u << sub_bits); j += 1u << rest) m_table[base + j] = Entry{ static_cast<std::uint8_t>(rest), static_cast<std::uint16_t>(s) };
            }
        }

        return true;
    }

    int decode(BitReader& br) const {
        const std::uint32_t bits = br.peek(15);
        const Entry& e = m_table[bits & ((1u << kRoot) - 1)];

        if (!(e.bits & 0x80)) {
            br.consume(e.bits);
            return e.value;
        }

        const unsigned sub_bits = e.bits & 0x7F;
        const Entry& s = m_table[(1u << kRoot) + e.value + ((bits >> kRoot) & ((1u << sub_bits) - 1))];
        br.consume(kRoot + s.bits);
        return s.value;
    }

    bool trivial(int& symbol) const {
        if (m_table.empty() || m_table[0].bits != 0 || (m_table[0].bits & 0x80)) return false;
        for (const auto& e : m_table) if (e.bits != 0 || e.value != m_table[0].value) return false;
        symbol = m_table[0].value;
        return true;
    }

private:
    static constexpr unsigned kRoot = 8;

    struct Entry {
        std::uint8_t  bits;
        std::uint16_t value;
    };

    std::vector<Entry> m_table;
};

struct HuffGroup {
    Huffman codes[5];
};

inline std::uint32_t average2(std::uint32_t a, std::uint32_t b) noexcept { return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b); }

inline int clip255(int v) noexcept { return v < 0 ? 0 : v > 255 ? 255 : v; }

inline std::uint32_t select_px(std::uint32_t L, std::uint32_t T, std::uint32_t TL) noexcept {
    int pl = 0, pt = 0;
    for (int s = 0; s < 32; s += 8) {
        const int l = (L >> s) & 255, t = (T >> s) & 255, tl = (TL >> s) & 255;
        pl += std::abs(t - tl);
        pt += std::abs(l - tl);
    }
    return pl < pt ? L : T;
}

inline std::uint32_t clamp_add_sub_full(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    std::uint32_t r = 0;
    for (int s = 0; s < 32; s += 8) r |= static_cast<std::uint32_t>(clip255(static_cast<int>((a >> s) & 255) + static_cast<int>((b >> s) & 255) - static_cast<int>((c >> s) & 255))) << s;
    return r;
}

inline std::uint32_t clamp_add_sub_half(std::uint32_t a, std::uint32_t b) noexcept {
    std::uint32_t r = 0;
    for (int s = 0; s < 32; s += 8) {
        const int x = (a >> s) & 255, y = (b >> s) & 255;
        r |= static_cast<std::uint32_t>(clip255(x + (x - y) / 2)) << s;
    }
    return r;
}

inline std::uint32_t add_px(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint32_t ag = (a & 0xFF00FF00u) + (b & 0xFF00FF00u);
    const std::uint32_t rb = (a & 0x00FF00FFu) + (b & 0x00FF00FFu);
    return (ag & 0xFF00FF00u) | (rb & 0x00FF00FFu);
}

inline int color_delta(std::int8_t t, std::int8_t c) noexcept { return (static_cast<int>(t) * static_cast<int>(c)) >> 5; }

inline unsigned subsample(unsigned size, unsigned bits) noexcept { return (size + (1u << bits) - 1) >> bits; }

struct Transform {
    int                        type;
    unsigned                   bits;
    unsigned                   xsize;
    std::vector<std::uint32_t> data;
};

class Vp8lDecoder {
public:
    Vp8lDecoder(const std::uint8_t* p, std::size_t n, std::string* error) : m_br(p, n), m_error(error) {}

    BitReader& br() { return m_br; }

    bool fail(const char* why) {
        if (m_error && m_error->empty()) *m_error = why;
        return false;
    }

    bool decode_image(unsigned xsize, unsigned ysize, bool level0, std::vector<std::uint32_t>& out) {
        std::vector<Transform> transforms;
        unsigned width = xsize;

        if (level0) {
            unsigned seen = 0;

            while (m_br.read(1)) {
                Transform t;
                t.type = static_cast<int>(m_br.read(2));
                if (seen & (1u << t.type)) return fail("duplicate VP8L transform");
                seen |= 1u << t.type;
                t.xsize = width;
                t.bits = 0;

                if (t.type == 0 || t.type == 1) {
                    t.bits = m_br.read(3) + 2;
                    if (!decode_image(subsample(width, t.bits), subsample(ysize, t.bits), false, t.data)) return false;
                } else if (t.type == 3) {
                    const unsigned n = m_br.read(8) + 1;
                    t.bits = n > 16 ? 0 : n > 4 ? 1 : n > 2 ? 2 : 3;
                    std::vector<std::uint32_t> pal;
                    if (!decode_image(n, 1, false, pal)) return false;
                    for (unsigned i = 1; i < n; ++i) pal[i] = add_px(pal[i], pal[i - 1]);
                    pal.resize(256, 0);
                    t.data = std::move(pal);
                    width = subsample(width, t.bits);
                }

                transforms.push_back(std::move(t));
                if (m_br.eos()) return fail("truncated VP8L transforms");
            }
        }

        unsigned cache_bits = 0;
        if (m_br.read(1)) {
            cache_bits = m_br.read(4);
            if (cache_bits < 1 || cache_bits > 11) return fail("bad VP8L color cache size");
        }

        unsigned huff_bits = 0;
        std::vector<std::uint32_t> huff_image;
        unsigned groups = 1;

        if (level0 && m_br.read(1)) {
            huff_bits = m_br.read(3) + 2;
            if (!decode_image(subsample(width, huff_bits), subsample(ysize, huff_bits), false, huff_image)) return false;
            for (auto& v : huff_image) {
                v = (v >> 8) & 0xFFFF;
                groups = std::max(groups, v + 1);
            }
        }

        if (m_br.eos()) return fail("truncated VP8L stream");
        std::vector<HuffGroup> hg(groups);
        const int alphabet[5] = { 256 + 24 + (cache_bits ? (1 << cache_bits) : 0), 256, 256, 256, 40 };

        for (unsigned g = 0; g < groups; ++g)
            for (int k = 0; k < 5; ++k)
                if (!read_code(alphabet[k], hg[g].codes[k])) return false;

        out.assign(static_cast<std::size_t>(width) * ysize, 0);
        if (!decode_pixels(width, cache_bits, huff_bits, huff_image, hg, out)) return false;

        for (std::size_t i = transforms.size(); i-- > 0;) apply_inverse(transforms[i], ysize, out);
        return true;
    }

private:
    bool read_code(int alphabet, Huffman& h) {
        std::vector<int> lengths(static_cast<std::size_t>(std::max(alphabet, 256)), 0);

        if (m_br.read(1)) {
            const unsigned num = m_br.read(1) + 1;
            const unsigned first8 = m_br.read(1);
            lengths[m_br.read(first8 ? 8 : 1)] = 1;
            if (num == 2) lengths[m_br.read(8)] = 1;
        } else {
            int cl_lengths[19] = {};
            const unsigned num = m_br.read(4) + 4;
            for (unsigned i = 0; i < num; ++i) cl_lengths[kCodeLengthCodeOrder[i]] = static_cast<int>(m_br.read(3));
            Huffman cl;
            if (!cl.build(cl_lengths, 19)) return fail("bad VP8L code length code");
            int max_symbol = alphabet;

            if (m_br.read(1)) {
                const unsigned nbits = 2 + 2 * m_br.read(3);
                max_symbol = 2 + static_cast<int>(m_br.read(nbits));
                if (max_symbol > alphabet) return fail("bad VP8L max symbol");
            }

            int prev = 8, sym = 0;
            while (sym < alphabet) {
                if (max_symbol-- == 0) break;
                const int c = cl.decode(m_br);
                if (c < 16) {
                    lengths[static_cast<std::size_t>(sym++)] = c;
                    if (c) prev = c;
                } else {
                    static const int extra[3] = { 2, 3, 7 }, offset[3] = { 3, 3, 11 };
                    const int repeat = static_cast<int>(m_br.read(static_cast<unsigned>(extra[c - 16]))) + offset[c - 16];
                    if (sym + repeat > alphabet) return fail("bad VP8L code lengths");
                    const int v = c == 16 ? prev : 0;
                    for (int k = 0; k < repeat; ++k) lengths[static_cast<std::size_t>(sym++)] = v;
                }
                if (m_br.eos()) return fail("truncated VP8L code lengths");
            }
        }

        if (m_br.eos()) return fail("truncated VP8L codes");
        if (!h.build(lengths.data(), alphabet)) return fail("bad VP8L Huffman code");
        return true;
    }

    std::uint32_t read_lz_value(int prefix) {
        if (prefix < 4) return static_cast<std::uint32_t>(prefix) + 1;
        const unsigned extra = static_cast<unsigned>(prefix - 2) >> 1;
        const std::uint32_t offset = (2u + (static_cast<unsigned>(prefix) & 1u)) << extra;
        return offset + m_br.read(extra) + 1;
    }

    bool decode_pixels(unsigned width, unsigned cache_bits, unsigned huff_bits, const std::vector<std::uint32_t>& huff_image, const std::vector<HuffGroup>& hg, std::vector<std::uint32_t>& out) {
        const std::size_t total = out.size();
        std::vector<std::uint32_t> cache(cache_bits ? (1u << cache_bits) : 0, 0);
        const unsigned huff_xsize = huff_bits ? subsample(width, huff_bits) : 0;
        const unsigned huff_mask = huff_bits ? (1u << huff_bits) - 1 : ~0u;
        std::size_t pos = 0, cached_upto = 0;
        unsigned x = 0, y = 0;
        const HuffGroup* g = &hg[0];

        auto group_at = [&](unsigned gx, unsigned gy) -> const HuffGroup* {
            if (!huff_bits) return &hg[0];
            return &hg[huff_image[static_cast<std::size_t>(gy >> huff_bits) * huff_xsize + (gx >> huff_bits)]];
        };

        auto flush_cache = [&]() {
            if (!cache_bits) {
                cached_upto = pos;
                return;
            }
            for (; cached_upto < pos; ++cached_upto) cache[(0x1E35A7BDu * out[cached_upto]) >> (32 - cache_bits)] = out[cached_upto];
        };

        while (pos < total) {
            if ((x & huff_mask) == 0) g = group_at(x, y);
            const int s = g->codes[0].decode(m_br);

            if (s < 256) {
                const int r = g->codes[1].decode(m_br);
                const int b = g->codes[2].decode(m_br);
                const int a = g->codes[3].decode(m_br);
                if (m_br.eos()) return fail("truncated VP8L pixels");
                out[pos++] = (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(s) << 8) | static_cast<std::uint32_t>(b);
                if (++x == width) { x = 0; ++y; }
            } else if (s < 256 + 24) {
                const std::uint32_t length = read_lz_value(s - 256);
                const int ds = g->codes[4].decode(m_br);
                const std::uint32_t code = read_lz_value(ds);
                std::size_t dist;
                if (code > 120) {
                    dist = code - 120;
                } else {
                    const int plane = kCodeToPlane[code - 1];
                    const std::int64_t d = static_cast<std::int64_t>(plane >> 4) * width + (8 - (plane & 15));
                    dist = d >= 1 ? static_cast<std::size_t>(d) : 1;
                }
                if (m_br.eos()) return fail("truncated VP8L pixels");
                if (dist > pos || length > total - pos) return fail("bad VP8L back reference");
                for (std::uint32_t k = 0; k < length; ++k, ++pos) out[pos] = out[pos - dist];
                x += length % width;
                y += length / width;
                if (x >= width) { x -= width; ++y; }
                if (pos < total) g = group_at(x, y);
            } else {
                const unsigned idx = static_cast<unsigned>(s - 280);
                if (idx >= cache.size()) return fail("bad VP8L cache index");
                flush_cache();
                out[pos++] = cache[idx];
                if (++x == width) { x = 0; ++y; }
            }

            flush_cache();
        }

        return true;
    }

    void apply_inverse(const Transform& t, unsigned ysize, std::vector<std::uint32_t>& px) {
        const unsigned w = t.xsize;

        if (t.type == 2) {
            for (auto& p : px) {
                const std::uint32_t g = (p >> 8) & 255;
                p = (p & 0xFF00FF00u) | ((((p >> 16) & 255) + g) & 255) << 16 | (((p & 255) + g) & 255);
            }
            return;
        }

        if (t.type == 3) {
            const unsigned bits = t.bits;
            std::vector<std::uint32_t> outp(static_cast<std::size_t>(w) * ysize);
            const unsigned packed_w = subsample(w, bits);
            const unsigned per = 1u << bits, bw = 8u >> bits, mask = (1u << bw) - 1;

            for (unsigned y = 0; y < ysize; ++y)
                for (unsigned x = 0; x < w; ++x) {
                    const std::uint32_t v = px[static_cast<std::size_t>(y) * packed_w + (x >> bits)];
                    const unsigned idx = (((v >> 8) & 255) >> ((x & (per - 1)) * bw)) & mask;
                    outp[static_cast<std::size_t>(y) * w + x] = t.data[idx];
                }

            px.swap(outp);
            return;
        }

        const unsigned tw = subsample(w, t.bits);

        if (t.type == 1) {
            for (unsigned y = 0; y < ysize; ++y)
                for (unsigned x = 0; x < w; ++x) {
                    const std::uint32_t m = t.data[static_cast<std::size_t>(y >> t.bits) * tw + (x >> t.bits)];
                    const std::int8_t g2r = static_cast<std::int8_t>(m & 255), g2b = static_cast<std::int8_t>((m >> 8) & 255), r2b = static_cast<std::int8_t>((m >> 16) & 255);
                    std::uint32_t& p = px[static_cast<std::size_t>(y) * w + x];
                    const std::int8_t g = static_cast<std::int8_t>((p >> 8) & 255);
                    int r = static_cast<int>((p >> 16) & 255), b = static_cast<int>(p & 255);
                    r = (r + color_delta(g2r, g)) & 255;
                    b = (b + color_delta(g2b, g)) & 255;
                    b = (b + color_delta(r2b, static_cast<std::int8_t>(r))) & 255;
                    p = (p & 0xFF00FF00u) | (static_cast<std::uint32_t>(r) << 16) | static_cast<std::uint32_t>(b);
                }
            return;
        }

        for (unsigned y = 0; y < ysize; ++y)
            for (unsigned x = 0; x < w; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * w + x;
                std::uint32_t pred;

                if (y == 0) {
                    pred = x == 0 ? 0xFF000000u : px[i - 1];
                } else if (x == 0) {
                    pred = px[i - w];
                } else {
                    const unsigned mode = (t.data[static_cast<std::size_t>(y >> t.bits) * tw + (x >> t.bits)] >> 8) & 15;
                    const std::uint32_t L = px[i - 1], T = px[i - w], TL = px[i - w - 1], TR = px[i - w + 1];

                    switch (mode) {
                        case 0: pred = 0xFF000000u; break;
                        case 1: pred = L; break;
                        case 2: pred = T; break;
                        case 3: pred = TR; break;
                        case 4: pred = TL; break;
                        case 5: pred = average2(average2(L, TR), T); break;
                        case 6: pred = average2(L, TL); break;
                        case 7: pred = average2(L, T); break;
                        case 8: pred = average2(TL, T); break;
                        case 9: pred = average2(T, TR); break;
                        case 10: pred = average2(average2(L, TL), average2(T, TR)); break;
                        case 11: pred = select_px(L, T, TL); break;
                        case 12: pred = clamp_add_sub_full(L, T, TL); break;
                        case 13: pred = clamp_add_sub_half(average2(L, T), TL); break;
                        default: pred = 0xFF000000u; break;
                    }
                }

                px[i] = add_px(px[i], pred);
            }
    }

    BitReader    m_br;
    std::string* m_error;
};

} // namespace

bool vp8l_decode_stream(const std::uint8_t* data, std::size_t size, unsigned width, unsigned height, std::vector<std::uint32_t>& argb, std::string* error) {
    Vp8lDecoder dec(data, size, error);
    return dec.decode_image(width, height, true, argb);
}

bool vp8l_decode(const std::uint8_t* data, std::size_t size, fmt::Rgba& out, std::string* error) {
    if (size < 5 || data[0] != 0x2F) return fmt::fail(error, "bad VP8L signature");
    const std::uint32_t hdr = data[1] | (data[2] << 8) | (static_cast<std::uint32_t>(data[3]) << 16) | (static_cast<std::uint32_t>(data[4]) << 24);
    const unsigned w = (hdr & 0x3FFF) + 1, h = ((hdr >> 14) & 0x3FFF) + 1;
    if ((hdr >> 29) != 0) return fmt::fail(error, "bad VP8L version");
    Vp8lDecoder dec(data + 5, size - 5, error);
    std::vector<std::uint32_t> argb;
    if (!dec.decode_image(w, h, true, argb)) return false;
    out = fmt::Rgba(w, h);

    for (std::size_t i = 0; i < argb.size(); ++i) {
        const std::uint32_t p = argb[i];
        out.px[i * 4] = static_cast<std::uint8_t>(p >> 16);
        out.px[i * 4 + 1] = static_cast<std::uint8_t>(p >> 8);
        out.px[i * 4 + 2] = static_cast<std::uint8_t>(p);
        out.px[i * 4 + 3] = static_cast<std::uint8_t>(p >> 24);
    }

    return true;
}

} // namespace webp_detail
} // namespace images
} // namespace fizmo
