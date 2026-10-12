#include "fizmo_library.hpp"
#include "webp_internal.hpp"

#include <cmath>
#include <map>
#include <queue>

namespace fizmo {
namespace images {
namespace webp_detail {

namespace {

const std::uint8_t kCodeLengthOrder[19] = { 17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

const std::uint8_t kPlaneCodes[120] = {
    0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a, 0x26, 0x2a, 0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a,
    0x25, 0x2b, 0x48, 0x04, 0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b, 0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45, 0x4b, 0x34, 0x3c, 0x03,
    0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d, 0x44, 0x4c, 0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
    0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b, 0x32, 0x3e, 0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f,
    0x64, 0x6c, 0x42, 0x4e, 0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b, 0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e, 0x00, 0x74, 0x7c, 0x41,
    0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d, 0x51, 0x5f, 0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70
};

class BitWriter {
public:
    void put(std::uint32_t value, unsigned n) {
        if (!n) return;
        m_acc |= static_cast<std::uint64_t>(value & ((n == 32) ? 0xFFFFFFFFu : ((1u << n) - 1))) << m_bits;
        m_bits += n;
        while (m_bits >= 8) {
            m_out.push_back(static_cast<std::uint8_t>(m_acc));
            m_acc >>= 8;
            m_bits -= 8;
        }
    }

    void append_bytes(const std::vector<std::uint8_t>& bytes) {
        for (std::uint8_t b : bytes) put(b, 8);
    }

    std::vector<std::uint8_t> finish() {
        if (m_bits) m_out.push_back(static_cast<std::uint8_t>(m_acc));
        m_acc = 0;
        m_bits = 0;
        return std::move(m_out);
    }

private:
    std::vector<std::uint8_t> m_out;
    std::uint64_t             m_acc = 0;
    unsigned                  m_bits = 0;
};

void limited_lengths(const std::vector<std::uint32_t>& freq, int limit, std::vector<int>& lengths) {
    const std::size_t n = freq.size();
    lengths.assign(n, 0);
    std::vector<std::uint64_t> f(freq.begin(), freq.end());
    std::size_t used = 0;
    for (auto v : f) if (v) ++used;
    if (used == 0) return;

    if (used == 1) {
        for (std::size_t i = 0; i < n; ++i) if (f[i]) lengths[i] = 1;
        return;
    }

    for (int attempt = 0; attempt < 64; ++attempt) {
        struct Node {
            std::uint64_t weight;
            int           left, right, symbol;
        };
        std::vector<Node> nodes;
        using Item = std::pair<std::uint64_t, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;

        for (std::size_t i = 0; i < n; ++i)
            if (f[i]) {
                nodes.push_back(Node{ f[i], -1, -1, static_cast<int>(i) });
                pq.push({ f[i], static_cast<int>(nodes.size() - 1) });
            }

        while (pq.size() > 1) {
            const Item a = pq.top(); pq.pop();
            const Item b = pq.top(); pq.pop();
            nodes.push_back(Node{ a.first + b.first, a.second, b.second, -1 });
            pq.push({ a.first + b.first, static_cast<int>(nodes.size() - 1) });
        }

        std::vector<std::pair<int, int>> stack = { { pq.top().second, 0 } };
        int maxlen = 0;
        std::fill(lengths.begin(), lengths.end(), 0);

        while (!stack.empty()) {
            const auto [idx, depth] = stack.back();
            stack.pop_back();
            const Node& nd = nodes[static_cast<std::size_t>(idx)];
            if (nd.symbol >= 0) {
                lengths[static_cast<std::size_t>(nd.symbol)] = depth;
                maxlen = std::max(maxlen, depth);
            } else {
                stack.push_back({ nd.left, depth + 1 });
                stack.push_back({ nd.right, depth + 1 });
            }
        }

        if (maxlen <= limit) return;
        for (auto& v : f) if (v) v = (v >> 1) | 1;
    }
}

struct Code {
    std::vector<int>           lengths;
    std::vector<std::uint32_t> codes;
    int                        used = 0;

    void build(const std::vector<std::uint32_t>& freq, int limit) {
        limited_lengths(freq, limit, lengths);
        assign();
    }

    void assign() {
        codes.assign(lengths.size(), 0);
        used = 0;
        int count[16] = {};
        for (int l : lengths) if (l) { ++count[l]; ++used; }
        int next[16] = {};
        int code = 0;
        for (int l = 1; l <= 15; ++l) {
            next[l] = code;
            code = (code + count[l]) << 1;
        }
        for (std::size_t s = 0; s < lengths.size(); ++s) {
            const int l = lengths[s];
            if (!l) continue;
            const std::uint32_t c = static_cast<std::uint32_t>(next[l]++);
            std::uint32_t r = 0;
            for (int b = 0; b < l; ++b) r |= ((c >> b) & 1u) << (l - 1 - b);
            codes[s] = r;
        }
    }

    void write(BitWriter& bw, int symbol) const {
        if (used <= 1) return;
        bw.put(codes[static_cast<std::size_t>(symbol)], static_cast<unsigned>(lengths[static_cast<std::size_t>(symbol)]));
    }

    double cost(const std::vector<std::uint32_t>& freq) const {
        if (used <= 1) return 0;
        double c = 0;
        for (std::size_t i = 0; i < freq.size(); ++i) c += static_cast<double>(freq[i]) * lengths[i];
        return c;
    }
};

void write_code(BitWriter& bw, const std::vector<std::uint32_t>& freq, Code& code) {
    std::vector<int> syms;
    for (std::size_t i = 0; i < freq.size(); ++i) if (freq[i]) syms.push_back(static_cast<int>(i));

    if (syms.empty()) syms.push_back(0);

    if (syms.size() <= 2 && syms.back() < 256) {
        bw.put(1, 1);
        bw.put(static_cast<std::uint32_t>(syms.size() - 1), 1);
        if (syms[0] < 2) {
            bw.put(0, 1);
            bw.put(static_cast<std::uint32_t>(syms[0]), 1);
        } else {
            bw.put(1, 1);
            bw.put(static_cast<std::uint32_t>(syms[0]), 8);
        }
        if (syms.size() == 2) bw.put(static_cast<std::uint32_t>(syms[1]), 8);
        code.lengths.assign(freq.size(), 0);
        for (int s : syms) code.lengths[static_cast<std::size_t>(s)] = 1;
        code.assign();
        return;
    }

    code.build(freq, 15);

    struct Token {
        int code;
        int extra;
    };
    std::vector<Token> tokens;
    const std::vector<int>& L = code.lengths;
    int prev = 8;
    std::size_t i = 0;

    while (i < L.size()) {
        const int v = L[i];
        std::size_t run = 1;
        while (i + run < L.size() && L[i + run] == v) ++run;

        if (v == 0) {
            std::size_t r = run;
            while (r >= 11) {
                const int k = static_cast<int>(std::min<std::size_t>(r, 138));
                tokens.push_back({ 18, k - 11 });
                r -= static_cast<std::size_t>(k);
            }
            if (r >= 3) {
                tokens.push_back({ 17, static_cast<int>(r) - 3 });
                r = 0;
            }
            while (r-- > 0) tokens.push_back({ 0, 0 });
        } else {
            std::size_t r = run;
            if (v != prev) {
                tokens.push_back({ v, 0 });
                --r;
            }
            while (r >= 3) {
                const int k = static_cast<int>(std::min<std::size_t>(r, 6));
                tokens.push_back({ 16, k - 3 });
                r -= static_cast<std::size_t>(k);
            }
            while (r-- > 0) tokens.push_back({ v, 0 });
            prev = v;
        }

        i += run;
    }

    std::vector<std::uint32_t> cl_freq(19, 0);
    for (const auto& t : tokens) ++cl_freq[static_cast<std::size_t>(t.code)];
    Code cl;
    cl.build(cl_freq, 7);
    int num = 19;
    while (num > 4 && cl.lengths[kCodeLengthOrder[num - 1]] == 0) --num;
    bw.put(0, 1);
    bw.put(static_cast<std::uint32_t>(num - 4), 4);
    for (int k = 0; k < num; ++k) bw.put(static_cast<std::uint32_t>(cl.lengths[kCodeLengthOrder[k]]), 3);
    bw.put(0, 1);

    for (const auto& t : tokens) {
        cl.write(bw, t.code);
        if (t.code == 16) bw.put(static_cast<std::uint32_t>(t.extra), 2);
        else if (t.code == 17) bw.put(static_cast<std::uint32_t>(t.extra), 3);
        else if (t.code == 18) bw.put(static_cast<std::uint32_t>(t.extra), 7);
    }
}

void prefix_encode(std::uint32_t v, int& prefix, int& extra_bits, std::uint32_t& extra_value) {
    const std::uint32_t d = v - 1;
    if (d < 4) {
        prefix = static_cast<int>(d);
        extra_bits = 0;
        extra_value = 0;
        return;
    }
    int hb = 31;
    while (!((d >> hb) & 1u)) --hb;
    const int second = static_cast<int>((d >> (hb - 1)) & 1u);
    extra_bits = hb - 1;
    extra_value = d & ((1u << extra_bits) - 1);
    prefix = 2 * hb + second;
}

struct Symbol {
    enum Kind : std::uint8_t { Literal, Cache, Copy } kind;
    std::uint32_t argb;
    std::uint32_t length;
    std::uint32_t dist_code;
};

class PlaneMap {
public:
    PlaneMap() {
        std::fill(std::begin(m_inv), std::end(m_inv), -1);
        for (int i = 0; i < 120; ++i) m_inv[kPlaneCodes[i]] = i;
    }

    std::uint32_t code(unsigned xsize, std::uint32_t dist) const {
        const std::uint32_t yoff = dist / xsize;
        const std::int64_t xoff = static_cast<std::int64_t>(dist) - static_cast<std::int64_t>(yoff) * xsize;

        if (xoff <= 8 && yoff < 8) {
            const int plane = static_cast<int>(yoff * 16 + 8 - xoff);
            if (plane >= 0 && plane < 128 && m_inv[plane] >= 0) return static_cast<std::uint32_t>(m_inv[plane]) + 1;
        }

        if (xoff > static_cast<std::int64_t>(xsize) - 8 && yoff < 7) {
            const std::int64_t plane = (yoff + 1) * 16 + 8 + (static_cast<std::int64_t>(xsize) - xoff);
            if (plane >= 0 && plane < 128 && m_inv[plane] >= 0) {
                const int c = m_inv[plane];
                const int p = kPlaneCodes[c];
                const std::int64_t d = static_cast<std::int64_t>(p >> 4) * xsize + (8 - (p & 15));
                if (d == static_cast<std::int64_t>(dist)) return static_cast<std::uint32_t>(c) + 1;
            }
        }

        return dist + 120;
    }

private:
    int m_inv[128];
};

const PlaneMap& plane_map() {
    static const PlaneMap m;
    return m;
}

std::vector<Symbol> backward_refs(const std::vector<std::uint32_t>& px, unsigned xsize, int effort) {
    std::vector<Symbol> out;
    const std::size_t n = px.size();
    out.reserve(n);
    const unsigned hash_bits = 18;
    std::vector<std::int32_t> head(1u << hash_bits, -1), chain(n, -1);
    const int max_chain = effort >= 6 ? 128 : effort >= 3 ? 48 : 16;
    const std::size_t max_dist = (1u << 20) - 120;
    const std::uint32_t max_len = 4096;

    auto hash_at = [&](std::size_t i) -> std::uint32_t {
        const std::uint64_t k = (static_cast<std::uint64_t>(px[i]) << 32) | px[i + 1];
        return static_cast<std::uint32_t>((k * 0x9E3779B97F4A7C15ull) >> (64 - hash_bits));
    };

    auto insert = [&](std::size_t i) {
        if (i + 1 >= n) return;
        const std::uint32_t h = hash_at(i);
        chain[i] = head[h];
        head[h] = static_cast<std::int32_t>(i);
    };

    auto match_len = [&](std::size_t a, std::size_t b, std::uint32_t limit) {
        std::uint32_t l = 0;
        while (l < limit && px[a + l] == px[b + l]) ++l;
        return l;
    };

    std::size_t i = 0;
    while (i < n) {
        std::uint32_t best_len = 0;
        std::size_t best_dist = 0;
        const std::uint32_t limit = static_cast<std::uint32_t>(std::min<std::size_t>(max_len, n - i));

        if (limit >= 2) {
            if (i >= 1) {
                const std::uint32_t l = match_len(i, i - 1, limit);
                if (l > best_len) { best_len = l; best_dist = 1; }
            }
            if (i >= xsize && best_len < limit) {
                const std::uint32_t l = match_len(i, i - xsize, limit);
                if (l > best_len) { best_len = l; best_dist = xsize; }
            }

            std::int32_t cand = head[hash_at(i)];
            int steps = 0;
            while (cand >= 0 && steps++ < max_chain && best_len < limit) {
                const std::size_t d = i - static_cast<std::size_t>(cand);
                if (d > max_dist) break;
                if (px[static_cast<std::size_t>(cand) + best_len] == px[i + best_len]) {
                    const std::uint32_t l = match_len(i, static_cast<std::size_t>(cand), limit);
                    if (l > best_len) { best_len = l; best_dist = d; }
                }
                cand = chain[static_cast<std::size_t>(cand)];
            }
        }

        if (best_len >= 3 || (best_len == 2 && (best_dist == 1 || best_dist == xsize))) {
            out.push_back(Symbol{ Symbol::Copy, 0, best_len, plane_map().code(xsize, static_cast<std::uint32_t>(best_dist)) });
            for (std::uint32_t k = 0; k < best_len; ++k) insert(i + k);
            i += best_len;
        } else {
            out.push_back(Symbol{ Symbol::Literal, px[i], 1, 0 });
            insert(i);
            ++i;
        }
    }

    return out;
}

void apply_cache(std::vector<Symbol>& syms, const std::vector<std::uint32_t>& px, unsigned cache_bits) {
    if (!cache_bits) return;
    std::vector<std::uint32_t> cache(1u << cache_bits, 0);
    std::vector<bool> valid(1u << cache_bits, false);
    std::size_t pos = 0;

    for (auto& s : syms) {
        if (s.kind == Symbol::Literal) {
            const std::uint32_t key = (0x1E35A7BDu * s.argb) >> (32 - cache_bits);
            if (valid[key] && cache[key] == s.argb) {
                s.kind = Symbol::Cache;
                s.dist_code = key;
            }
            cache[key] = s.argb;
            valid[key] = true;
            ++pos;
        } else {
            for (std::uint32_t k = 0; k < s.length; ++k, ++pos) {
                const std::uint32_t key = (0x1E35A7BDu * px[pos]) >> (32 - cache_bits);
                cache[key] = px[pos];
                valid[key] = true;
            }
        }
    }
}

struct Histograms {
    std::vector<std::uint32_t> h[5];

    explicit Histograms(unsigned cache_bits) {
        h[0].assign(256 + 24 + (cache_bits ? (1u << cache_bits) : 0), 0);
        h[1].assign(256, 0);
        h[2].assign(256, 0);
        h[3].assign(256, 0);
        h[4].assign(40, 0);
    }

    void add(const Symbol& s) {
        if (s.kind == Symbol::Literal) {
            ++h[0][(s.argb >> 8) & 255];
            ++h[1][(s.argb >> 16) & 255];
            ++h[2][s.argb & 255];
            ++h[3][s.argb >> 24];
        } else if (s.kind == Symbol::Cache) {
            ++h[0][280 + s.dist_code];
        } else {
            int p, eb;
            std::uint32_t ev;
            prefix_encode(s.length, p, eb, ev);
            ++h[0][256 + static_cast<std::size_t>(p)];
            prefix_encode(s.dist_code, p, eb, ev);
            ++h[4][static_cast<std::size_t>(p)];
        }
    }

    double entropy_bits() const {
        double total = 0;
        for (const auto& v : h) {
            std::uint64_t sum = 0;
            for (auto c : v) sum += c;
            if (!sum) continue;
            for (auto c : v) if (c) total += static_cast<double>(c) * std::log2(static_cast<double>(sum) / c);
        }
        return total;
    }
};

double extra_bits_cost(const std::vector<Symbol>& syms) {
    double bits = 0;
    for (const auto& s : syms) {
        if (s.kind != Symbol::Copy) continue;
        int p, eb;
        std::uint32_t ev;
        prefix_encode(s.length, p, eb, ev);
        bits += eb;
        prefix_encode(s.dist_code, p, eb, ev);
        bits += eb;
    }
    return bits;
}

void write_entropy_image(BitWriter& bw, const std::vector<std::uint32_t>& px, unsigned xsize, bool level0, int effort) {
    std::vector<Symbol> base = backward_refs(px, xsize, effort);
    unsigned best_bits = 0;
    double best_cost = -1;
    std::vector<Symbol> best_syms;
    const unsigned candidates[] = { 0, 4, 7, 10 };

    for (unsigned cb : candidates) {
        if (cb && effort < 2 && cb != 10) continue;
        std::vector<Symbol> syms = base;
        apply_cache(syms, px, cb);
        Histograms hist(cb);
        for (const auto& s : syms) hist.add(s);
        const double cost = hist.entropy_bits() + extra_bits_cost(syms) + (cb ? 0.5 * (1u << cb) : 0.0);
        if (best_cost < 0 || cost < best_cost) {
            best_cost = cost;
            best_bits = cb;
            best_syms = std::move(syms);
        }
    }

    if (best_bits) {
        bw.put(1, 1);
        bw.put(best_bits, 4);
    } else {
        bw.put(0, 1);
    }

    if (level0) bw.put(0, 1);
    Histograms hist(best_bits);
    for (const auto& s : best_syms) hist.add(s);
    Code codes[5];
    for (int k = 0; k < 5; ++k) write_code(bw, hist.h[k], codes[k]);

    for (const auto& s : best_syms) {
        if (s.kind == Symbol::Literal) {
            codes[0].write(bw, static_cast<int>((s.argb >> 8) & 255));
            codes[1].write(bw, static_cast<int>((s.argb >> 16) & 255));
            codes[2].write(bw, static_cast<int>(s.argb & 255));
            codes[3].write(bw, static_cast<int>(s.argb >> 24));
        } else if (s.kind == Symbol::Cache) {
            codes[0].write(bw, static_cast<int>(280 + s.dist_code));
        } else {
            int p, eb;
            std::uint32_t ev;
            prefix_encode(s.length, p, eb, ev);
            codes[0].write(bw, 256 + p);
            bw.put(ev, static_cast<unsigned>(eb));
            prefix_encode(s.dist_code, p, eb, ev);
            codes[4].write(bw, p);
            bw.put(ev, static_cast<unsigned>(eb));
        }
    }
}

inline std::uint32_t average2(std::uint32_t a, std::uint32_t b) noexcept { return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b); }
inline int clip255(int v) noexcept { return v < 0 ? 0 : v > 255 ? 255 : v; }

std::uint32_t predict(unsigned mode, std::uint32_t L, std::uint32_t T, std::uint32_t TL, std::uint32_t TR) {
    switch (mode) {
        case 0: return 0xFF000000u;
        case 1: return L;
        case 2: return T;
        case 3: return TR;
        case 4: return TL;
        case 5: return average2(average2(L, TR), T);
        case 6: return average2(L, TL);
        case 7: return average2(L, T);
        case 8: return average2(TL, T);
        case 9: return average2(T, TR);
        case 10: return average2(average2(L, TL), average2(T, TR));
        case 11: {
            int pl = 0, pt = 0;
            for (int s = 0; s < 32; s += 8) {
                const int l = (L >> s) & 255, t = (T >> s) & 255, tl = (TL >> s) & 255;
                pl += std::abs(t - tl);
                pt += std::abs(l - tl);
            }
            return pl < pt ? L : T;
        }
        case 12: {
            std::uint32_t r = 0;
            for (int s = 0; s < 32; s += 8) r |= static_cast<std::uint32_t>(clip255(static_cast<int>((L >> s) & 255) + static_cast<int>((T >> s) & 255) - static_cast<int>((TL >> s) & 255))) << s;
            return r;
        }
        default: {
            const std::uint32_t a = average2(L, T);
            std::uint32_t r = 0;
            for (int s = 0; s < 32; s += 8) {
                const int x = (a >> s) & 255, y = (TL >> s) & 255;
                r |= static_cast<std::uint32_t>(clip255(x + (x - y) / 2)) << s;
            }
            return r;
        }
    }
}

inline std::uint32_t sub_px(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint32_t ag = 0x00FF00FFu + (a & 0xFF00FF00u) - (b & 0xFF00FF00u);
    const std::uint32_t rb = 0xFF00FF00u + (a & 0x00FF00FFu) - (b & 0x00FF00FFu);
    return (ag & 0xFF00FF00u) | (rb & 0x00FF00FFu);
}

inline int abs_residual(std::uint32_t r) noexcept {
    int s = 0;
    for (int k = 0; k < 32; k += 8) s += std::abs(static_cast<int>(static_cast<std::int8_t>((r >> k) & 255)));
    return s;
}

inline unsigned subsample(unsigned size, unsigned bits) noexcept { return (size + (1u << bits) - 1) >> bits; }

void predictor_transform(std::vector<std::uint32_t>& px, unsigned w, unsigned h, unsigned bits, std::vector<std::uint32_t>& modes_img) {
    const unsigned tw = subsample(w, bits), th = subsample(h, bits);
    modes_img.assign(static_cast<std::size_t>(tw) * th, 0xFF000000u | (11u << 8));
    const std::vector<std::uint32_t> orig = px;

    auto pred_at = [&](unsigned mode, unsigned x, unsigned y) -> std::uint32_t {
        const std::size_t i = static_cast<std::size_t>(y) * w + x;
        if (y == 0) return x == 0 ? 0xFF000000u : orig[i - 1];
        if (x == 0) return orig[i - w];
        return predict(mode, orig[i - 1], orig[i - w], orig[i - w - 1], orig[i - w + 1]);
    };

    for (unsigned ty = 0; ty < th; ++ty)
        for (unsigned tx = 0; tx < tw; ++tx) {
            const unsigned x0 = tx << bits, y0 = ty << bits;
            const unsigned x1 = std::min(w, x0 + (1u << bits)), y1 = std::min(h, y0 + (1u << bits));
            std::int64_t best = -1;
            unsigned best_mode = 0;

            for (unsigned m = 0; m < 14; ++m) {
                std::int64_t cost = 0;
                for (unsigned y = y0; y < y1 && (best < 0 || cost < best); ++y)
                    for (unsigned x = x0; x < x1; ++x) cost += abs_residual(sub_px(orig[static_cast<std::size_t>(y) * w + x], pred_at(m, x, y)));
                if (best < 0 || cost < best) {
                    best = cost;
                    best_mode = m;
                }
            }

            modes_img[static_cast<std::size_t>(ty) * tw + tx] = 0xFF000000u | (best_mode << 8);
            for (unsigned y = y0; y < y1; ++y)
                for (unsigned x = x0; x < x1; ++x) px[static_cast<std::size_t>(y) * w + x] = sub_px(orig[static_cast<std::size_t>(y) * w + x], pred_at(best_mode, x, y));
        }
}

inline int color_delta(std::int8_t t, std::int8_t c) noexcept { return (static_cast<int>(t) * static_cast<int>(c)) >> 5; }

void color_transform(std::vector<std::uint32_t>& px, unsigned w, unsigned h, unsigned bits, std::vector<std::uint32_t>& img) {
    const unsigned tw = subsample(w, bits), th = subsample(h, bits);
    img.assign(static_cast<std::size_t>(tw) * th, 0xFF000000u);

    for (unsigned ty = 0; ty < th; ++ty)
        for (unsigned tx = 0; tx < tw; ++tx) {
            const unsigned x0 = tx << bits, y0 = ty << bits;
            const unsigned x1 = std::min(w, x0 + (1u << bits)), y1 = std::min(h, y0 + (1u << bits));

            auto red_cost = [&](int g2r) {
                std::int64_t c = 0;
                for (unsigned y = y0; y < y1; ++y)
                    for (unsigned x = x0; x < x1; ++x) {
                        const std::uint32_t p = px[static_cast<std::size_t>(y) * w + x];
                        const int r = static_cast<int>((p >> 16) & 255) - color_delta(static_cast<std::int8_t>(g2r), static_cast<std::int8_t>((p >> 8) & 255));
                        c += std::abs(static_cast<int>(static_cast<std::int8_t>(r & 255)));
                    }
                return c;
            };

            auto blue_cost = [&](int g2b, int r2b) {
                std::int64_t c = 0;
                for (unsigned y = y0; y < y1; ++y)
                    for (unsigned x = x0; x < x1; ++x) {
                        const std::uint32_t p = px[static_cast<std::size_t>(y) * w + x];
                        const int b = static_cast<int>(p & 255) - color_delta(static_cast<std::int8_t>(g2b), static_cast<std::int8_t>((p >> 8) & 255)) - color_delta(static_cast<std::int8_t>(r2b), static_cast<std::int8_t>((p >> 16) & 255));
                        c += std::abs(static_cast<int>(static_cast<std::int8_t>(b & 255)));
                    }
                return c;
            };

            int best_r = 0;
            std::int64_t best = red_cost(0);
            for (int v = -128; v < 128; v += 4) {
                const std::int64_t c = red_cost(v);
                if (c < best) { best = c; best_r = v; }
            }
            for (int v = best_r - 3; v <= best_r + 3; ++v) {
                if (v < -128 || v > 127) continue;
                const std::int64_t c = red_cost(v);
                if (c < best) { best = c; best_r = v; }
            }

            int best_gb = 0, best_rb = 0;
            best = blue_cost(0, 0);
            for (int v = -128; v < 128; v += 8) {
                const std::int64_t c = blue_cost(v, 0);
                if (c < best) { best = c; best_gb = v; }
            }
            for (int v = -128; v < 128; v += 8) {
                const std::int64_t c = blue_cost(best_gb, v);
                if (c < best) { best = c; best_rb = v; }
            }
            for (int a = best_gb - 7; a <= best_gb + 7; a += 1)
                for (int b = best_rb - 7; b <= best_rb + 7; b += 7) {
                    if (a < -128 || a > 127 || b < -128 || b > 127) continue;
                    const std::int64_t c = blue_cost(a, b);
                    if (c < best) { best = c; best_gb = a; best_rb = b; }
                }

            const std::uint32_t m = 0xFF000000u | (static_cast<std::uint32_t>(best_rb & 255) << 16) | (static_cast<std::uint32_t>(best_gb & 255) << 8) | static_cast<std::uint32_t>(best_r & 255);
            img[static_cast<std::size_t>(ty) * tw + tx] = m;

            for (unsigned y = y0; y < y1; ++y)
                for (unsigned x = x0; x < x1; ++x) {
                    std::uint32_t& p = px[static_cast<std::size_t>(y) * w + x];
                    const std::int8_t g = static_cast<std::int8_t>((p >> 8) & 255);
                    const std::int8_t r8 = static_cast<std::int8_t>((p >> 16) & 255);
                    const int nr = (static_cast<int>((p >> 16) & 255) - color_delta(static_cast<std::int8_t>(best_r), g)) & 255;
                    const int nb = (static_cast<int>(p & 255) - color_delta(static_cast<std::int8_t>(best_gb), g) - color_delta(static_cast<std::int8_t>(best_rb), r8)) & 255;
                    p = (p & 0xFF00FF00u) | (static_cast<std::uint32_t>(nr) << 16) | static_cast<std::uint32_t>(nb);
                }
        }
}

void encode_stream(BitWriter& bw, const std::vector<std::uint32_t>& argb, unsigned w, unsigned h, int effort) {
    std::map<std::uint32_t, int> colors;
    for (auto p : argb) {
        colors.emplace(p, 0);
        if (colors.size() > 256) break;
    }

    if (colors.size() <= 256) {
        std::vector<std::uint32_t> palette;
        for (auto& kv : colors) palette.push_back(kv.first);
        std::sort(palette.begin(), palette.end(), [](std::uint32_t a, std::uint32_t b) {
            const int la = static_cast<int>(((a >> 16) & 255) * 2 + ((a >> 8) & 255) * 4 + (a & 255) + (a >> 24) * 8);
            const int lb = static_cast<int>(((b >> 16) & 255) * 2 + ((b >> 8) & 255) * 4 + (b & 255) + (b >> 24) * 8);
            return la != lb ? la < lb : a < b;
        });
        for (std::size_t i = 0; i < palette.size(); ++i) colors[palette[i]] = static_cast<int>(i);
        const std::size_t n = palette.size();
        const unsigned bits = n > 16 ? 0 : n > 4 ? 1 : n > 2 ? 2 : 3;
        bw.put(1, 1);
        bw.put(3, 2);
        bw.put(static_cast<std::uint32_t>(n - 1), 8);
        std::vector<std::uint32_t> delta(n);
        for (std::size_t i = 0; i < n; ++i) delta[i] = i ? sub_px(palette[i], palette[i - 1]) : palette[0];
        write_entropy_image(bw, delta, static_cast<unsigned>(n), false, effort);

        const unsigned pw = subsample(w, bits);
        const unsigned per = 1u << bits, bw_bits = 8u >> bits;
        std::vector<std::uint32_t> packed(static_cast<std::size_t>(pw) * h, 0);
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x) {
                const std::uint32_t idx = static_cast<std::uint32_t>(colors[argb[static_cast<std::size_t>(y) * w + x]]);
                packed[static_cast<std::size_t>(y) * pw + (x >> bits)] |= idx << (8 + (x & (per - 1)) * bw_bits);
            }
        for (auto& p : packed) p |= 0xFF000000u;
        bw.put(0, 1);
        write_entropy_image(bw, packed, pw, true, effort);
        return;
    }

    struct Config {
        unsigned pbits;
        bool     color;
    };
    static const Config all_configs[6] = { { 5, false }, { 4, true }, { 4, false }, { 3, false }, { 5, true }, { 3, true } };
    const std::size_t config_count = effort <= 2 ? 1 : effort <= 5 ? 2 : 6;
    std::vector<std::uint8_t> best;

    for (std::size_t ci = 0; ci < config_count; ++ci) {
        const Config& cfg = all_configs[ci];
        BitWriter cw;
        std::vector<std::uint32_t> px = argb;
        cw.put(1, 1);
        cw.put(2, 2);
        for (auto& p : px) {
            const std::uint32_t g = (p >> 8) & 255;
            p = (p & 0xFF00FF00u) | ((((p >> 16) & 255) - g) & 255) << 16 | (((p & 255) - g) & 255);
        }

        std::vector<std::uint32_t> modes;
        predictor_transform(px, w, h, cfg.pbits, modes);
        cw.put(1, 1);
        cw.put(0, 2);
        cw.put(cfg.pbits - 2, 3);
        write_entropy_image(cw, modes, subsample(w, cfg.pbits), false, effort);

        if (cfg.color) {
            std::vector<std::uint32_t> cimg;
            color_transform(px, w, h, cfg.pbits, cimg);
            cw.put(1, 1);
            cw.put(1, 2);
            cw.put(cfg.pbits - 2, 3);
            write_entropy_image(cw, cimg, subsample(w, cfg.pbits), false, effort);
        }

        cw.put(0, 1);
        write_entropy_image(cw, px, w, true, effort);
        std::vector<std::uint8_t> bytes = cw.finish();
        if (best.empty() || bytes.size() < best.size()) best = std::move(bytes);
    }

    bw.append_bytes(best);
}

} // namespace

bool vp8l_encode_stream(const std::vector<std::uint32_t>& argb, unsigned width, unsigned height, std::vector<std::uint8_t>& out, int effort) {
    if (!width || !height || argb.size() != static_cast<std::size_t>(width) * height) return false;
    BitWriter bw;
    encode_stream(bw, argb, width, height, effort);
    out = bw.finish();
    return true;
}

bool vp8l_encode(const fmt::Rgba& img, std::vector<std::uint8_t>& out, int effort) {
    if (!img.width || !img.height || img.width > 16384 || img.height > 16384) return false;
    std::vector<std::uint32_t> argb(static_cast<std::size_t>(img.width) * img.height);
    bool alpha = false;

    for (std::size_t i = 0; i < argb.size(); ++i) {
        const std::uint8_t* p = img.px.data() + i * 4;
        argb[i] = (static_cast<std::uint32_t>(p[3]) << 24) | (static_cast<std::uint32_t>(p[0]) << 16) | (static_cast<std::uint32_t>(p[1]) << 8) | p[2];
        if (p[3] != 255) alpha = true;
    }

    BitWriter bw;
    bw.put(0x2F, 8);
    bw.put(img.width - 1, 14);
    bw.put(img.height - 1, 14);
    bw.put(alpha ? 1 : 0, 1);
    bw.put(0, 3);
    encode_stream(bw, argb, img.width, img.height, effort);
    out = bw.finish();
    return true;
}

} // namespace webp_detail
} // namespace images
} // namespace fizmo
