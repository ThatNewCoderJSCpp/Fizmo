#include "vorbis.hpp"

#include "ogg.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <deque>
#include <new>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;
const std::uint64_t k_entry_budget = std::uint64_t(1) << 22;
const std::uint64_t k_vq_budget = std::uint64_t(1) << 22;
const int k_fast_bits = 10;
const std::size_t k_max_group = 4096;
const std::size_t k_npos = static_cast<std::size_t>(-1);

int ilog(std::uint64_t v) noexcept {
    int n = 0;
    while (v) {
        ++n;
        v >>= 1;
    }
    return n;
}

std::uint32_t reverse32(std::uint32_t v) noexcept {
    v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
    v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
    v = ((v >> 4) & 0x0F0F0F0Fu) | ((v & 0x0F0F0F0Fu) << 4);
    v = ((v >> 8) & 0x00FF00FFu) | ((v & 0x00FF00FFu) << 8);
    return (v >> 16) | (v << 16);
}

std::uint32_t reverse_bits(std::uint32_t v, int bits) noexcept {
    return bits <= 0 ? 0 : reverse32(v) >> (32 - bits);
}

float float32_unpack(std::uint32_t x) noexcept {
    const double mantissa = static_cast<double>(x & 0x1FFFFFu);
    const int exponent = static_cast<int>((x & 0x7FE00000u) >> 21);
    double v = std::ldexp(mantissa, exponent - 788);
    if (v > static_cast<double>(FLT_MAX)) v = static_cast<double>(FLT_MAX);
    if (x & 0x80000000u) v = -v;
    return static_cast<float>(v);
}

bool magic_is(const std::uint8_t* p, std::size_t size, std::uint8_t type) noexcept {
    return size >= 7 && p[0] == type && std::memcmp(p + 1, "vorbis", 6) == 0;
}

std::uint32_t le32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept
        : m_data(data), m_size(size), m_bits(static_cast<std::uint64_t>(size) * 8) {}

    std::uint32_t read(int n) noexcept {
        if (n <= 0) return 0;
        if (m_pos + static_cast<std::uint64_t>(n) > m_bits) {
            m_pos = m_bits;
            m_eop = true;
            return 0;
        }
        const std::uint32_t v = gather(n);
        m_pos += static_cast<std::uint64_t>(n);
        return v;
    }

    std::uint64_t read64(int n) noexcept {
        if (n <= 32) return read(n);
        const std::uint64_t lo = read(32);
        const std::uint64_t hi = read(n - 32);
        return lo | (hi << 32);
    }

    std::uint32_t peek(int n) const noexcept { return gather(n); }
    void skip(int n) noexcept { m_pos += static_cast<std::uint64_t>(n); }
    std::uint64_t left() const noexcept { return m_bits - m_pos; }
    bool eop() const noexcept { return m_eop; }
    void set_eop() noexcept {
        m_pos = m_bits;
        m_eop = true;
    }

private:
    std::uint32_t gather(int n) const noexcept {
        const std::size_t byte = static_cast<std::size_t>(m_pos >> 3);
        const int shift = static_cast<int>(m_pos & 7);
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < 5 && byte + i < m_size; ++i) v |= std::uint64_t(m_data[byte + i]) << (8 * i);
        v >>= shift;
        return n >= 32 ? static_cast<std::uint32_t>(v) : static_cast<std::uint32_t>(v & ((std::uint64_t(1) << n) - 1));
    }

    const std::uint8_t* m_data;
    std::size_t m_size;
    std::uint64_t m_bits;
    std::uint64_t m_pos = 0;
    bool m_eop = false;
};

struct Codebook {
    std::uint32_t dims = 0;
    std::uint32_t entries = 0;
    std::vector<std::uint8_t> lengths;
    int fast_bits = 0;
    std::vector<std::int32_t> fast;
    std::vector<std::uint32_t> long_codes;
    std::vector<std::int32_t> long_entries;
    std::vector<std::uint8_t> long_lengths;
    std::int32_t single = -1;
    int lookup = 0;
    float minimum = 0.0f;
    float delta = 0.0f;
    bool sequence = false;
    std::uint32_t lookup_values = 0;
    std::vector<float> multiplicands;
    std::vector<float> vq;

    std::int32_t decode(BitReader& br) const noexcept {
        if (single >= 0) {
            const int len = lengths[static_cast<std::size_t>(single)];
            if (br.left() < static_cast<std::uint64_t>(len)) {
                br.set_eop();
                return -1;
            }
            br.skip(len);
            return single;
        }
        if (fast.empty()) {
            br.set_eop();
            return -1;
        }
        const std::int32_t e = fast[br.peek(fast_bits)];
        if (e >= 0) {
            const int len = lengths[static_cast<std::size_t>(e)];
            if (br.left() < static_cast<std::uint64_t>(len)) {
                br.set_eop();
                return -1;
            }
            br.skip(len);
            return e;
        }
        if (long_codes.empty()) {
            br.set_eop();
            return -1;
        }
        const std::uint32_t x = reverse32(br.peek(32));
        const auto it = std::upper_bound(long_codes.begin(), long_codes.end(), x);
        if (it == long_codes.begin()) {
            br.set_eop();
            return -1;
        }
        const std::size_t idx = static_cast<std::size_t>(it - long_codes.begin()) - 1;
        const int len = long_lengths[idx];
        if (len < 32 && ((x ^ long_codes[idx]) >> (32 - len)) != 0) {
            br.set_eop();
            return -1;
        }
        if (br.left() < static_cast<std::uint64_t>(len)) {
            br.set_eop();
            return -1;
        }
        br.skip(len);
        return long_entries[idx];
    }

    void compute(std::uint32_t entry, float* out) const noexcept {
        float last = 0.0f;
        std::uint32_t offset = entry;
        for (std::uint32_t i = 0; i < dims; ++i) {
            std::size_t m;
            if (lookup == 1) {
                m = offset % lookup_values;
                offset /= lookup_values;
            } else {
                m = static_cast<std::size_t>(entry) * dims + i;
            }
            const float v = multiplicands[m] * delta + minimum + last;
            out[i] = v;
            if (sequence) last = v;
        }
    }

    const float* vector(std::uint32_t entry, float* scratch) const noexcept {
        if (!vq.empty()) return &vq[static_cast<std::size_t>(entry) * dims];
        compute(entry, scratch);
        return scratch;
    }
};

std::uint32_t lookup1_values(std::uint32_t entries, std::uint32_t dims) {
    if (dims == 0 || entries == 0) return 0;
    auto power_fits = [&](std::uint64_t r) {
        std::uint64_t acc = 1;
        for (std::uint32_t i = 0; i < dims; ++i) {
            acc *= r;
            if (acc > entries) return false;
        }
        return true;
    };
    std::uint64_t r = static_cast<std::uint64_t>(std::floor(std::pow(static_cast<double>(entries), 1.0 / dims)));
    while (r > 0 && !power_fits(r)) --r;
    while (power_fits(r + 1)) ++r;
    return static_cast<std::uint32_t>(r);
}

bool build_huffman(Codebook& cb) {
    std::uint32_t used = 0;
    int max_len = 0;
    std::int32_t last_used = -1;
    for (std::uint32_t e = 0; e < cb.entries; ++e) {
        if (cb.lengths[e]) {
            ++used;
            last_used = static_cast<std::int32_t>(e);
            max_len = std::max<int>(max_len, cb.lengths[e]);
        }
    }
    if (used == 0) return true;
    if (used == 1) {
        cb.single = last_used;
        return true;
    }
    std::vector<std::uint32_t> codes(cb.entries, 0);
    std::uint32_t marker[33] = {};
    for (std::uint32_t e = 0; e < cb.entries; ++e) {
        const int len = cb.lengths[e];
        if (!len) continue;
        std::uint32_t entry = marker[len];
        if (len < 32 && (entry >> len)) return false;
        codes[e] = entry;
        for (int j = len; j > 0; --j) {
            if (marker[j] & 1) {
                if (j == 1) {
                    marker[1]++;
                } else {
                    marker[j] = marker[j - 1] << 1;
                }
                break;
            }
            marker[j]++;
        }
        for (int j = len + 1; j < 33; ++j) {
            if ((marker[j] >> 1) == entry) {
                entry = marker[j];
                marker[j] = marker[j - 1] << 1;
            } else {
                break;
            }
        }
    }
    cb.fast_bits = std::min(k_fast_bits, max_len);
    cb.fast.assign(std::size_t(1) << cb.fast_bits, -1);
    std::vector<std::uint32_t> order;
    for (std::uint32_t e = 0; e < cb.entries; ++e) {
        const int len = cb.lengths[e];
        if (!len) continue;
        if (len <= cb.fast_bits) {
            const std::uint32_t rev = reverse_bits(codes[e], len);
            for (std::uint32_t k = rev; k < cb.fast.size(); k += (std::uint32_t(1) << len)) cb.fast[k] = static_cast<std::int32_t>(e);
        } else {
            order.push_back(e);
        }
    }
    auto left_aligned = [&](std::uint32_t e) {
        const int len = cb.lengths[e];
        return len >= 32 ? codes[e] : codes[e] << (32 - len);
    };
    std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) { return left_aligned(a) < left_aligned(b); });
    cb.long_codes.reserve(order.size());
    cb.long_entries.reserve(order.size());
    cb.long_lengths.reserve(order.size());
    for (std::uint32_t e : order) {
        cb.long_codes.push_back(left_aligned(e));
        cb.long_entries.push_back(static_cast<std::int32_t>(e));
        cb.long_lengths.push_back(cb.lengths[e]);
    }
    return true;
}

bool parse_codebook(BitReader& br, Codebook& cb, std::uint64_t& entry_budget, std::uint64_t& vq_budget) {
    if (br.read(24) != 0x564342u) return false;
    cb.dims = br.read(16);
    cb.entries = br.read(24);
    if (br.eop()) return false;
    if (ilog(cb.dims) + ilog(cb.entries) > 24) return false;
    if (cb.entries > entry_budget) return false;
    entry_budget -= cb.entries;
    const bool ordered = br.read(1) != 0;
    if (!ordered) {
        const bool sparse = br.read(1) != 0;
        if (br.left() < static_cast<std::uint64_t>(cb.entries) * (sparse ? 1u : 5u)) return false;
        cb.lengths.assign(cb.entries, 0);
        for (std::uint32_t i = 0; i < cb.entries; ++i) {
            if (!sparse || br.read(1)) cb.lengths[i] = static_cast<std::uint8_t>(br.read(5) + 1);
        }
    } else {
        cb.lengths.assign(cb.entries, 0);
        std::uint32_t current = 0;
        std::uint32_t length = br.read(5) + 1;
        while (current < cb.entries) {
            const std::uint32_t number = br.read(ilog(cb.entries - current));
            if (br.eop()) return false;
            if (number > cb.entries - current) return false;
            if (number && length > 32) return false;
            for (std::uint32_t i = 0; i < number; ++i) cb.lengths[current + i] = static_cast<std::uint8_t>(length);
            current += number;
            ++length;
            if (length > 33 && current < cb.entries) return false;
        }
    }
    if (br.eop()) return false;
    cb.lookup = static_cast<int>(br.read(4));
    if (cb.lookup == 1 || cb.lookup == 2) {
        if (cb.dims == 0 || cb.entries == 0) return false;
        cb.minimum = float32_unpack(br.read(32));
        cb.delta = float32_unpack(br.read(32));
        const int value_bits = static_cast<int>(br.read(4)) + 1;
        cb.sequence = br.read(1) != 0;
        if (br.eop()) return false;
        const std::uint64_t count = cb.lookup == 1 ? lookup1_values(cb.entries, cb.dims) : static_cast<std::uint64_t>(cb.entries) * cb.dims;
        if (count == 0) return false;
        if (count * static_cast<std::uint64_t>(value_bits) > br.left()) return false;
        cb.lookup_values = static_cast<std::uint32_t>(count);
        cb.multiplicands.resize(static_cast<std::size_t>(count));
        for (std::size_t i = 0; i < cb.multiplicands.size(); ++i) cb.multiplicands[i] = static_cast<float>(br.read(value_bits));
        if (br.eop()) return false;
    } else if (cb.lookup != 0) {
        return false;
    }
    if (!build_huffman(cb)) return false;
    if (cb.lookup) {
        const std::uint64_t total = static_cast<std::uint64_t>(cb.entries) * cb.dims;
        if (total <= vq_budget) {
            vq_budget -= total;
            cb.vq.resize(static_cast<std::size_t>(total));
            for (std::uint32_t e = 0; e < cb.entries; ++e) cb.compute(e, &cb.vq[static_cast<std::size_t>(e) * cb.dims]);
        }
    }
    return true;
}

struct Floor {
    int type = 0;
    int order = 0;
    std::uint32_t rate = 0;
    std::uint32_t bark_map_size = 0;
    int amp_bits = 0;
    std::uint32_t amp_offset = 0;
    std::vector<std::uint8_t> books0;
    std::vector<std::int32_t> map[2];
    int partitions = 0;
    std::uint8_t partition_class[32] = {};
    std::uint8_t class_dims[16] = {};
    std::uint8_t class_sub[16] = {};
    std::int16_t class_master[16] = {};
    std::int16_t sub_books[16][8] = {};
    int multiplier = 1;
    int values = 0;
    int x[65] = {};
    std::uint8_t sorted[65] = {};
    std::uint8_t low[65] = {};
    std::uint8_t high[65] = {};
};

struct Residue {
    int type = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    std::uint32_t partition_size = 1;
    int classifications = 1;
    int classbook = 0;
    std::int16_t books[64][8] = {};
};

struct Mapping {
    int submaps = 1;
    std::vector<std::pair<std::uint8_t, std::uint8_t>> coupling;
    std::vector<std::uint8_t> mux;
    std::uint8_t floor[16] = {};
    std::uint8_t residue[16] = {};
};

struct Mode {
    bool blockflag = false;
    int mapping = 0;
};

class Imdct {
public:
    void init(int n) {
        m_n = n;
        const int m = n / 2;
        const int l = n / 4;
        m_pre_c.resize(static_cast<std::size_t>(l));
        m_pre_s.resize(static_cast<std::size_t>(l));
        m_post_c.resize(static_cast<std::size_t>(l));
        m_post_s.resize(static_cast<std::size_t>(l));
        for (int j = 0; j < l; ++j) {
            const double a = k_pi * (j + 0.25) / m;
            m_pre_c[static_cast<std::size_t>(j)] = std::cos(a);
            m_pre_s[static_cast<std::size_t>(j)] = std::sin(a);
            const double b = k_pi * j / m;
            m_post_c[static_cast<std::size_t>(j)] = std::cos(b);
            m_post_s[static_cast<std::size_t>(j)] = std::sin(b);
        }
        m_tw_c.resize(static_cast<std::size_t>(l / 2));
        m_tw_s.resize(static_cast<std::size_t>(l / 2));
        for (int k = 0; k < l / 2; ++k) {
            const double a = 2.0 * k_pi * k / l;
            m_tw_c[static_cast<std::size_t>(k)] = std::cos(a);
            m_tw_s[static_cast<std::size_t>(k)] = std::sin(a);
        }
        const int bits = ilog(static_cast<std::uint64_t>(l)) - 1;
        m_rev.resize(static_cast<std::size_t>(l));
        for (int j = 0; j < l; ++j) m_rev[static_cast<std::size_t>(j)] = reverse_bits(static_cast<std::uint32_t>(j), bits);
        m_re.resize(static_cast<std::size_t>(l));
        m_im.resize(static_cast<std::size_t>(l));
        m_u.resize(static_cast<std::size_t>(m));
    }

    void run(const float* in, float* out) {
        const std::size_t n = static_cast<std::size_t>(m_n);
        const std::size_t m = n / 2;
        const std::size_t l = n / 4;
        for (std::size_t j = 0; j < l; ++j) {
            const double a = in[2 * j];
            const double b = in[m - 1 - 2 * j];
            const double c = m_pre_c[j];
            const double s = m_pre_s[j];
            const std::size_t r = m_rev[j];
            m_re[r] = a * c + b * s;
            m_im[r] = b * c - a * s;
        }
        for (std::size_t len = 2; len <= l; len <<= 1) {
            const std::size_t half = len / 2;
            const std::size_t step = l / len;
            for (std::size_t i = 0; i < l; i += len) {
                for (std::size_t k = 0; k < half; ++k) {
                    const double wr = m_tw_c[k * step];
                    const double wi = -m_tw_s[k * step];
                    const std::size_t a = i + k;
                    const std::size_t b = a + half;
                    const double tr = m_re[b] * wr - m_im[b] * wi;
                    const double ti = m_re[b] * wi + m_im[b] * wr;
                    m_re[b] = m_re[a] - tr;
                    m_im[b] = m_im[a] - ti;
                    m_re[a] += tr;
                    m_im[a] += ti;
                }
            }
        }
        for (std::size_t p = 0; p < l; ++p) {
            const double c = m_post_c[p];
            const double s = m_post_s[p];
            const double re = m_re[p] * c + m_im[p] * s;
            const double im = m_im[p] * c - m_re[p] * s;
            m_u[2 * p] = re;
            m_u[m - 1 - 2 * p] = -im;
        }
        const std::size_t q = l;
        for (std::size_t t = 0; t < q; ++t) out[t] = static_cast<float>(m_u[t + q]);
        for (std::size_t t = q; t < 3 * q; ++t) out[t] = static_cast<float>(-m_u[3 * q - 1 - t]);
        for (std::size_t t = 3 * q; t < n; ++t) out[t] = static_cast<float>(-m_u[t - 3 * q]);
    }

private:
    int m_n = 0;
    std::vector<double> m_pre_c;
    std::vector<double> m_pre_s;
    std::vector<double> m_post_c;
    std::vector<double> m_post_s;
    std::vector<double> m_tw_c;
    std::vector<double> m_tw_s;
    std::vector<std::uint32_t> m_rev;
    std::vector<double> m_re;
    std::vector<double> m_im;
    std::vector<double> m_u;
};

const float* floor1_table() {
    static const struct Table {
        float v[256];
        Table() {
            for (int i = 0; i < 256; ++i) v[i] = static_cast<float>(std::pow(10.0, (i - 255) * 7.0 / 256.0));
        }
    } table;
    return table.v;
}

int render_point(int x0, int y0, int x1, int y1, int x) noexcept {
    const long long dy = static_cast<long long>(y1) - y0;
    const long long adx = static_cast<long long>(x1) - x0;
    const long long ady = dy < 0 ? -dy : dy;
    const long long err = ady * (static_cast<long long>(x) - x0);
    const long long off = adx ? err / adx : 0;
    return static_cast<int>(dy < 0 ? y0 - off : y0 + off);
}

void render_line(int x0, int y0, int x1, int y1, float* v, int n, const float* table) noexcept {
    if (x1 <= x0) return;
    const int dy = y1 - y0;
    const int adx = x1 - x0;
    int ady = dy < 0 ? -dy : dy;
    const int base = dy / adx;
    const int sy = dy < 0 ? base - 1 : base + 1;
    const int abase = base < 0 ? -base : base;
    ady -= abase * adx;
    int y = y0;
    int err = 0;
    const int stop = std::min(x1, n);
    if (x0 < n) v[x0] = table[std::min(std::max(y, 0), 255)];
    for (int x = x0 + 1; x < stop; ++x) {
        err += ady;
        if (err >= adx) {
            err -= adx;
            y += sy;
        } else {
            y += base;
        }
        v[x] = table[std::min(std::max(y, 0), 255)];
    }
}

double bark(double x) noexcept {
    return 13.1 * std::atan(0.00074 * x) + 2.24 * std::atan(0.0000000185 * x * x) + 0.0001 * x;
}

struct Identification {
    int channels = 0;
    std::uint32_t rate = 0;
    std::int32_t bitrate_max = 0;
    std::int32_t bitrate_nominal = 0;
    std::int32_t bitrate_min = 0;
    int blocksize[2] = { 0, 0 };
};

bool parse_identification(const std::uint8_t* p, std::size_t size, Identification& id) {
    if (size < 30 || !magic_is(p, size, 1)) return false;
    if (le32(p + 7) != 0) return false;
    id.channels = p[11];
    id.rate = le32(p + 12);
    id.bitrate_max = static_cast<std::int32_t>(le32(p + 16));
    id.bitrate_nominal = static_cast<std::int32_t>(le32(p + 20));
    id.bitrate_min = static_cast<std::int32_t>(le32(p + 24));
    const int b0 = p[28] & 15;
    const int b1 = p[28] >> 4;
    if (id.channels == 0 || id.rate == 0) return false;
    if (b0 < 6 || b0 > 13 || b1 < 6 || b1 > 13 || b0 > b1) return false;
    if ((p[29] & 1) == 0) return false;
    id.blocksize[0] = 1 << b0;
    id.blocksize[1] = 1 << b1;
    return true;
}

void parse_comments(const std::uint8_t* p, std::size_t size, std::string& vendor, std::vector<std::pair<std::string, std::string>>& tags) {
    if (!magic_is(p, size, 3)) return;
    std::size_t pos = 7;
    if (size - pos < 4) return;
    std::uint32_t len = le32(p + pos);
    pos += 4;
    if (len > size - pos) return;
    vendor.assign(reinterpret_cast<const char*>(p + pos), len);
    pos += len;
    if (size - pos < 4) return;
    const std::uint32_t count = le32(p + pos);
    pos += 4;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (size - pos < 4) return;
        len = le32(p + pos);
        pos += 4;
        if (len > size - pos) return;
        const std::string entry(reinterpret_cast<const char*>(p + pos), len);
        pos += len;
        const std::size_t eq = entry.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string key = entry.substr(0, eq);
        for (char& c : key) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        tags.emplace_back(std::move(key), entry.substr(eq + 1));
    }
}

class Decoder {
public:
    Identification id;

    bool setup(const std::uint8_t* ident, std::size_t ident_size, const std::uint8_t* data, std::size_t size, std::string* error);
    int packet_blocksize(const std::uint8_t* data, std::size_t size) const noexcept;
    bool decode(const std::uint8_t* data, std::size_t size, std::size_t& frames);
    void reset() noexcept { m_prev_n = 0; }
    const float* output(int channel) const noexcept { return m_out[static_cast<std::size_t>(channel)].data(); }

private:
    bool parse_floor(BitReader& br, Floor& f);
    bool parse_residue(BitReader& br, Residue& r);
    bool parse_mapping(BitReader& br, Mapping& m);
    bool decode_floor1(const Floor& f, BitReader& br, int n2, float* out);
    bool decode_floor0(const Floor& f, BitReader& br, int blockflag, int n2, float* out);
    void decode_residue(const Residue& r, BitReader& br, float* const* vecs, const bool* skip, int count, int n2);
    void decode_partitions(const Residue& r, BitReader& br, float* const* vecs, const bool* skip, int count, std::size_t size, int format);
    bool usable_vq_book(int index) const noexcept;

    std::vector<Codebook> m_books;
    std::vector<Floor> m_floors;
    std::vector<Residue> m_residues;
    std::vector<Mapping> m_mappings;
    std::vector<Mode> m_modes;
    Imdct m_imdct[2];
    std::vector<float> m_slope[2];
    std::vector<std::vector<float>> m_floor;
    std::vector<std::vector<float>> m_res;
    std::vector<std::vector<float>> m_raw;
    std::vector<std::vector<float>> m_prev;
    std::vector<std::vector<float>> m_out;
    std::vector<std::vector<std::uint8_t>> m_class;
    std::vector<float> m_interleave;
    std::vector<float> m_scratch;
    std::vector<float> m_coeff;
    std::vector<double> m_cos;
    std::vector<std::uint8_t> m_unused;
    std::vector<std::uint8_t> m_no_residue;
    std::vector<float*> m_vec_ptr;
    std::vector<std::uint8_t> m_skip;
    int m_prev_n = 0;
};

bool Decoder::usable_vq_book(int index) const noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= m_books.size()) return false;
    const Codebook& b = m_books[static_cast<std::size_t>(index)];
    return b.lookup != 0 && b.dims > 0;
}

bool Decoder::parse_floor(BitReader& br, Floor& f) {
    f.type = static_cast<int>(br.read(16));
    if (f.type == 0) {
        f.order = static_cast<int>(br.read(8));
        f.rate = br.read(16);
        f.bark_map_size = br.read(16);
        f.amp_bits = static_cast<int>(br.read(6));
        f.amp_offset = br.read(8);
        const int count = static_cast<int>(br.read(4)) + 1;
        for (int i = 0; i < count; ++i) {
            const int b = static_cast<int>(br.read(8));
            if (!usable_vq_book(b)) return false;
            f.books0.push_back(static_cast<std::uint8_t>(b));
        }
        if (br.eop() || f.order < 1 || f.rate < 1 || f.bark_map_size < 1) return false;
        for (int k = 0; k < 2; ++k) {
            const int n2 = id.blocksize[k] / 2;
            std::vector<std::int32_t>& map = f.map[k];
            map.resize(static_cast<std::size_t>(n2) + 1);
            const double scale = f.bark_map_size / bark(0.5 * f.rate);
            for (int i = 0; i < n2; ++i) {
                double v = std::floor(bark(static_cast<double>(f.rate) * i / (2.0 * n2)) * scale);
                if (!(v >= 0.0)) v = 0.0;
                if (v > f.bark_map_size - 1.0) v = f.bark_map_size - 1.0;
                map[static_cast<std::size_t>(i)] = static_cast<std::int32_t>(v);
            }
            map[static_cast<std::size_t>(n2)] = -1;
        }
        return true;
    }
    if (f.type != 1) return false;
    f.partitions = static_cast<int>(br.read(5));
    int max_class = -1;
    for (int i = 0; i < f.partitions; ++i) {
        f.partition_class[i] = static_cast<std::uint8_t>(br.read(4));
        max_class = std::max<int>(max_class, f.partition_class[i]);
    }
    for (int i = 0; i <= max_class; ++i) {
        f.class_dims[i] = static_cast<std::uint8_t>(br.read(3) + 1);
        f.class_sub[i] = static_cast<std::uint8_t>(br.read(2));
        f.class_master[i] = -1;
        if (f.class_sub[i]) {
            f.class_master[i] = static_cast<std::int16_t>(br.read(8));
            if (static_cast<std::size_t>(f.class_master[i]) >= m_books.size()) return false;
        }
        for (int j = 0; j < (1 << f.class_sub[i]); ++j) {
            const int b = static_cast<int>(br.read(8)) - 1;
            if (b >= static_cast<int>(m_books.size())) return false;
            f.sub_books[i][j] = static_cast<std::int16_t>(b);
        }
    }
    f.multiplier = static_cast<int>(br.read(2)) + 1;
    const int range_bits = static_cast<int>(br.read(4));
    f.x[0] = 0;
    f.x[1] = 1 << range_bits;
    f.values = 2;
    for (int i = 0; i < f.partitions; ++i) {
        const int cls = f.partition_class[i];
        for (int j = 0; j < f.class_dims[cls]; ++j) {
            if (f.values >= 65) return false;
            f.x[f.values++] = static_cast<int>(br.read(range_bits));
        }
    }
    if (br.eop()) return false;
    for (int i = 0; i < f.values; ++i) f.sorted[i] = static_cast<std::uint8_t>(i);
    std::stable_sort(f.sorted, f.sorted + f.values, [&](std::uint8_t a, std::uint8_t b) { return f.x[a] < f.x[b]; });
    for (int i = 1; i < f.values; ++i) {
        if (f.x[f.sorted[i]] == f.x[f.sorted[i - 1]]) return false;
    }
    for (int i = 2; i < f.values; ++i) {
        int lo = 0;
        int hi = 1;
        int lx = -1;
        int hx = 1 << 30;
        for (int j = 0; j < i; ++j) {
            if (f.x[j] < f.x[i] && f.x[j] > lx) {
                lx = f.x[j];
                lo = j;
            }
            if (f.x[j] > f.x[i] && f.x[j] < hx) {
                hx = f.x[j];
                hi = j;
            }
        }
        f.low[i] = static_cast<std::uint8_t>(lo);
        f.high[i] = static_cast<std::uint8_t>(hi);
    }
    return true;
}

bool Decoder::parse_residue(BitReader& br, Residue& r) {
    r.type = static_cast<int>(br.read(16));
    if (r.type > 2) return false;
    r.begin = br.read(24);
    r.end = br.read(24);
    r.partition_size = br.read(24) + 1;
    r.classifications = static_cast<int>(br.read(6)) + 1;
    r.classbook = static_cast<int>(br.read(8));
    if (static_cast<std::size_t>(r.classbook) >= m_books.size() || m_books[static_cast<std::size_t>(r.classbook)].dims == 0) return false;
    int cascade[64];
    for (int i = 0; i < r.classifications; ++i) {
        const int low = static_cast<int>(br.read(3));
        const int high = br.read(1) ? static_cast<int>(br.read(5)) : 0;
        cascade[i] = high * 8 + low;
    }
    for (int i = 0; i < r.classifications; ++i) {
        for (int j = 0; j < 8; ++j) {
            r.books[i][j] = -1;
            if (cascade[i] & (1 << j)) {
                const int b = static_cast<int>(br.read(8));
                if (!usable_vq_book(b)) return false;
                r.books[i][j] = static_cast<std::int16_t>(b);
            }
        }
    }
    return !br.eop();
}

bool Decoder::parse_mapping(BitReader& br, Mapping& m) {
    if (br.read(16) != 0) return false;
    m.submaps = br.read(1) ? static_cast<int>(br.read(4)) + 1 : 1;
    if (br.read(1)) {
        const int steps = static_cast<int>(br.read(8)) + 1;
        const int bits = ilog(static_cast<std::uint64_t>(id.channels - 1));
        for (int i = 0; i < steps; ++i) {
            const int mag = static_cast<int>(br.read(bits));
            const int ang = static_cast<int>(br.read(bits));
            if (mag == ang || mag >= id.channels || ang >= id.channels) return false;
            m.coupling.emplace_back(static_cast<std::uint8_t>(mag), static_cast<std::uint8_t>(ang));
        }
    }
    if (br.read(2) != 0) return false;
    m.mux.assign(static_cast<std::size_t>(id.channels), 0);
    if (m.submaps > 1) {
        for (int c = 0; c < id.channels; ++c) {
            const int v = static_cast<int>(br.read(4));
            if (v >= m.submaps) return false;
            m.mux[static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(v);
        }
    }
    for (int i = 0; i < m.submaps; ++i) {
        br.read(8);
        const int fl = static_cast<int>(br.read(8));
        const int re = static_cast<int>(br.read(8));
        if (static_cast<std::size_t>(fl) >= m_floors.size() || static_cast<std::size_t>(re) >= m_residues.size()) return false;
        m.floor[i] = static_cast<std::uint8_t>(fl);
        m.residue[i] = static_cast<std::uint8_t>(re);
    }
    return !br.eop();
}

bool Decoder::setup(const std::uint8_t* ident, std::size_t ident_size, const std::uint8_t* data, std::size_t size, std::string* error) {
    auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    m_books.clear();
    m_floors.clear();
    m_residues.clear();
    m_mappings.clear();
    m_modes.clear();
    m_prev_n = 0;
    if (!parse_identification(ident, ident_size, id)) return fail("the Vorbis identification header is invalid");
    if (!magic_is(data, size, 5)) return fail("the Vorbis setup header is missing");
    BitReader br(data + 7, size - 7);
    const int book_count = static_cast<int>(br.read(8)) + 1;
    std::uint64_t entry_budget = k_entry_budget;
    std::uint64_t vq_budget = k_vq_budget;
    m_books.assign(static_cast<std::size_t>(book_count), Codebook());
    for (Codebook& cb : m_books) {
        if (!parse_codebook(br, cb, entry_budget, vq_budget)) return fail("a Vorbis codebook is invalid");
    }
    const int times = static_cast<int>(br.read(6)) + 1;
    for (int i = 0; i < times; ++i) {
        if (br.read(16) != 0) return fail("the Vorbis setup header has an unknown time transform");
    }
    const int floors = static_cast<int>(br.read(6)) + 1;
    m_floors.assign(static_cast<std::size_t>(floors), Floor());
    for (Floor& f : m_floors) {
        if (!parse_floor(br, f)) return fail("a Vorbis floor is invalid");
    }
    const int residues = static_cast<int>(br.read(6)) + 1;
    m_residues.assign(static_cast<std::size_t>(residues), Residue());
    for (Residue& r : m_residues) {
        if (!parse_residue(br, r)) return fail("a Vorbis residue is invalid");
    }
    const int mappings = static_cast<int>(br.read(6)) + 1;
    m_mappings.assign(static_cast<std::size_t>(mappings), Mapping());
    for (Mapping& m : m_mappings) {
        if (!parse_mapping(br, m)) return fail("a Vorbis mapping is invalid");
    }
    const int modes = static_cast<int>(br.read(6)) + 1;
    m_modes.assign(static_cast<std::size_t>(modes), Mode());
    for (Mode& m : m_modes) {
        m.blockflag = br.read(1) != 0;
        const std::uint32_t window = br.read(16);
        const std::uint32_t transform = br.read(16);
        m.mapping = static_cast<int>(br.read(8));
        if (window != 0 || transform != 0 || m.mapping >= mappings) return fail("a Vorbis mode is invalid");
    }
    if (br.read(1) != 1 || br.eop()) return fail("the Vorbis setup header is damaged");
    const std::size_t channels = static_cast<std::size_t>(id.channels);
    const std::size_t n = static_cast<std::size_t>(id.blocksize[1]);
    for (int k = 0; k < 2; ++k) {
        m_imdct[k].init(id.blocksize[k]);
        const int s = id.blocksize[k] / 2;
        m_slope[k].resize(static_cast<std::size_t>(s));
        for (int i = 0; i < s; ++i) {
            const double t = std::sin((i + 0.5) / s * k_pi / 2.0);
            m_slope[k][static_cast<std::size_t>(i)] = static_cast<float>(std::sin(k_pi / 2.0 * t * t));
        }
    }
    m_floor.assign(channels, std::vector<float>(n / 2));
    m_res.assign(channels, std::vector<float>(n / 2));
    m_raw.assign(channels, std::vector<float>(n));
    m_prev.assign(channels, std::vector<float>(n / 2));
    m_out.assign(channels, std::vector<float>(n / 2));
    m_class.assign(channels, std::vector<std::uint8_t>());
    m_unused.assign(channels, 0);
    m_no_residue.assign(channels, 0);
    m_vec_ptr.assign(channels, nullptr);
    m_skip.assign(channels, 0);
    std::uint32_t max_dims = 1;
    for (const Codebook& cb : m_books) max_dims = std::max(max_dims, cb.dims);
    m_scratch.resize(max_dims);
    m_prev_n = 0;
    return true;
}

int Decoder::packet_blocksize(const std::uint8_t* data, std::size_t size) const noexcept {
    if (size == 0 || m_modes.empty()) return 0;
    BitReader br(data, size);
    if (br.read(1) != 0) return 0;
    const std::uint32_t mode = br.read(ilog(m_modes.size() - 1));
    if (br.eop() || mode >= m_modes.size()) return 0;
    const bool flag = m_modes[mode].blockflag;
    if (flag) {
        br.read(2);
        if (br.eop()) return 0;
    }
    return id.blocksize[flag ? 1 : 0];
}

bool Decoder::decode_floor1(const Floor& f, BitReader& br, int n2, float* out) {
    if (br.read(1) == 0) return false;
    static const int ranges[4] = { 256, 128, 86, 64 };
    const int range = ranges[f.multiplier - 1];
    const int bits = ilog(static_cast<std::uint64_t>(range - 1));
    int y[65];
    y[0] = static_cast<int>(br.read(bits));
    y[1] = static_cast<int>(br.read(bits));
    int offset = 2;
    for (int i = 0; i < f.partitions; ++i) {
        const int cls = f.partition_class[i];
        const int cdim = f.class_dims[cls];
        const int cbits = f.class_sub[cls];
        const int csub = (1 << cbits) - 1;
        int cval = 0;
        if (cbits) {
            cval = m_books[static_cast<std::size_t>(f.class_master[cls])].decode(br);
            if (cval < 0) return false;
        }
        for (int j = 0; j < cdim; ++j) {
            const int book = f.sub_books[cls][cval & csub];
            cval >>= cbits;
            if (book >= 0) {
                const int v = m_books[static_cast<std::size_t>(book)].decode(br);
                if (v < 0) return false;
                y[offset + j] = v;
            } else {
                y[offset + j] = 0;
            }
        }
        offset += cdim;
    }
    if (br.eop()) return false;
    bool step2[65];
    int fy[65];
    step2[0] = step2[1] = true;
    fy[0] = y[0];
    fy[1] = y[1];
    for (int i = 2; i < f.values; ++i) {
        const int lo = f.low[i];
        const int hi = f.high[i];
        const int predicted = render_point(f.x[lo], fy[lo], f.x[hi], fy[hi], f.x[i]);
        const int val = y[i];
        const int highroom = range - predicted;
        const int lowroom = predicted;
        const int room = (highroom < lowroom ? highroom : lowroom) * 2;
        if (val != 0) {
            step2[lo] = step2[hi] = step2[i] = true;
            if (val >= room) {
                fy[i] = highroom > lowroom ? val - lowroom + predicted : predicted - val + highroom - 1;
            } else if (val & 1) {
                fy[i] = predicted - (val + 1) / 2;
            } else {
                fy[i] = predicted + val / 2;
            }
        } else {
            step2[i] = false;
            fy[i] = predicted;
        }
    }
    const float* table = floor1_table();
    int lx = 0;
    int ly = fy[f.sorted[0]] * f.multiplier;
    int hx = 0;
    int hy = 0;
    for (int k = 1; k < f.values; ++k) {
        const int i = f.sorted[k];
        if (step2[i]) {
            hy = fy[i] * f.multiplier;
            hx = f.x[i];
            render_line(lx, ly, hx, hy, out, n2, table);
            lx = hx;
            ly = hy;
        }
    }
    if (hx < n2) render_line(hx, hy, n2, hy, out, n2, table);
    return true;
}

bool Decoder::decode_floor0(const Floor& f, BitReader& br, int blockflag, int n2, float* out) {
    const std::uint64_t amplitude = br.read64(f.amp_bits);
    if (br.eop() || amplitude == 0) return false;
    const std::uint32_t booknum = br.read(ilog(f.books0.size()));
    if (br.eop() || booknum >= f.books0.size()) return false;
    const Codebook& cb = m_books[f.books0[booknum]];
    m_coeff.resize(static_cast<std::size_t>(f.order) + cb.dims);
    std::size_t count = 0;
    float last = 0.0f;
    while (count < static_cast<std::size_t>(f.order)) {
        const std::int32_t e = cb.decode(br);
        if (e < 0) return false;
        const float* v = cb.vector(static_cast<std::uint32_t>(e), m_scratch.data());
        for (std::uint32_t d = 0; d < cb.dims; ++d) m_coeff[count + d] = v[d] + last;
        last = m_coeff[count + cb.dims - 1];
        count += cb.dims;
    }
    m_cos.resize(static_cast<std::size_t>(f.order));
    for (int j = 0; j < f.order; ++j) m_cos[static_cast<std::size_t>(j)] = std::cos(static_cast<double>(m_coeff[static_cast<std::size_t>(j)]));
    const std::vector<std::int32_t>& map = f.map[blockflag];
    const double amp_scale = std::ldexp(1.0, f.amp_bits) - 1.0;
    int i = 0;
    while (i < n2) {
        const double w = k_pi * map[static_cast<std::size_t>(i)] / f.bark_map_size;
        const double cw = std::cos(w);
        double p;
        double q;
        if (f.order & 1) {
            p = 1.0 - cw * cw;
            q = 0.25;
            for (int j = 0; j * 2 + 1 < f.order; ++j) {
                const double d = m_cos[static_cast<std::size_t>(2 * j + 1)] - cw;
                p *= 4.0 * d * d;
            }
            for (int j = 0; j * 2 < f.order; ++j) {
                const double d = m_cos[static_cast<std::size_t>(2 * j)] - cw;
                q *= 4.0 * d * d;
            }
        } else {
            p = (1.0 - cw) / 2.0;
            q = (1.0 + cw) / 2.0;
            for (int j = 0; j * 2 + 1 < f.order; ++j) {
                const double d = m_cos[static_cast<std::size_t>(2 * j + 1)] - cw;
                p *= 4.0 * d * d;
            }
            for (int j = 0; j * 2 < f.order; ++j) {
                const double d = m_cos[static_cast<std::size_t>(2 * j)] - cw;
                q *= 4.0 * d * d;
            }
        }
        double val = std::exp(0.11512925 * (static_cast<double>(amplitude) * f.amp_offset / (amp_scale * std::sqrt(p + q)) - f.amp_offset));
        if (!std::isfinite(val)) val = 0.0;
        const std::int32_t key = map[static_cast<std::size_t>(i)];
        do {
            out[i] = static_cast<float>(val);
            ++i;
        } while (i < n2 && map[static_cast<std::size_t>(i)] == key);
    }
    return true;
}

void Decoder::decode_partitions(const Residue& r, BitReader& br, float* const* vecs, const bool* skip, int count, std::size_t size, int format) {
    const std::size_t begin = std::min<std::size_t>(r.begin, size);
    const std::size_t end = std::min<std::size_t>(r.end, size);
    if (end <= begin) return;
    const std::size_t psize = r.partition_size;
    const std::size_t parts = (end - begin) / psize;
    if (parts == 0) return;
    const Codebook& classbook = m_books[static_cast<std::size_t>(r.classbook)];
    const std::size_t cpc = classbook.dims;
    for (int j = 0; j < count; ++j) m_class[static_cast<std::size_t>(j)].assign(parts + cpc, 0);
    for (int pass = 0; pass < 8; ++pass) {
        std::size_t pc = 0;
        while (pc < parts) {
            if (pass == 0) {
                for (int j = 0; j < count; ++j) {
                    if (skip[j]) continue;
                    std::int32_t temp = classbook.decode(br);
                    if (temp < 0) return;
                    std::uint8_t* cls = m_class[static_cast<std::size_t>(j)].data();
                    for (std::size_t i = cpc; i-- > 0;) {
                        cls[i + pc] = static_cast<std::uint8_t>(temp % r.classifications);
                        temp /= r.classifications;
                    }
                }
            }
            for (std::size_t i = 0; i < cpc && pc < parts; ++i, ++pc) {
                for (int j = 0; j < count; ++j) {
                    if (skip[j]) continue;
                    const int vqclass = m_class[static_cast<std::size_t>(j)][pc];
                    const int book = r.books[vqclass][pass];
                    if (book < 0) continue;
                    const Codebook& cb = m_books[static_cast<std::size_t>(book)];
                    const std::size_t dims = cb.dims;
                    float* v = vecs[j] + begin + pc * psize;
                    if (format == 0) {
                        const std::size_t step = psize / dims;
                        for (std::size_t k = 0; k < step; ++k) {
                            const std::int32_t e = cb.decode(br);
                            if (e < 0) return;
                            const float* q = cb.vector(static_cast<std::uint32_t>(e), m_scratch.data());
                            for (std::size_t d = 0; d < dims; ++d) v[k + d * step] += q[d];
                        }
                    } else {
                        const std::size_t room = size - (begin + pc * psize);
                        std::size_t k = 0;
                        while (k < psize) {
                            const std::int32_t e = cb.decode(br);
                            if (e < 0) return;
                            const float* q = cb.vector(static_cast<std::uint32_t>(e), m_scratch.data());
                            for (std::size_t d = 0; d < dims && k < room; ++d) v[k++] += q[d];
                            if (k >= room) break;
                        }
                    }
                }
            }
        }
    }
}

void Decoder::decode_residue(const Residue& r, BitReader& br, float* const* vecs, const bool* skip, int count, int n2) {
    if (r.type != 2) {
        decode_partitions(r, br, vecs, skip, count, static_cast<std::size_t>(n2), r.type);
        return;
    }
    bool any = false;
    for (int j = 0; j < count; ++j) {
        if (!skip[j]) any = true;
    }
    if (!any) return;
    const bool none = false;
    if (count == 1) {
        decode_partitions(r, br, vecs, &none, 1, static_cast<std::size_t>(n2), 1);
        return;
    }
    const std::size_t total = static_cast<std::size_t>(n2) * static_cast<std::size_t>(count);
    m_interleave.assign(total, 0.0f);
    float* flat = m_interleave.data();
    decode_partitions(r, br, &flat, &none, 1, total, 1);
    for (int i = 0; i < n2; ++i) {
        for (int j = 0; j < count; ++j) vecs[j][i] = flat[static_cast<std::size_t>(i) * static_cast<std::size_t>(count) + static_cast<std::size_t>(j)];
    }
}

bool Decoder::decode(const std::uint8_t* data, std::size_t size, std::size_t& frames) {
    frames = 0;
    if (size == 0 || m_modes.empty()) return false;
    BitReader br(data, size);
    if (br.read(1) != 0) return false;
    const std::uint32_t mode_index = br.read(ilog(m_modes.size() - 1));
    if (br.eop() || mode_index >= m_modes.size()) return false;
    const Mode& mode = m_modes[mode_index];
    const int blockflag = mode.blockflag ? 1 : 0;
    if (blockflag) {
        br.read(2);
        if (br.eop()) return false;
    }
    const int n = id.blocksize[blockflag];
    const int n2 = n / 2;
    const Mapping& map = m_mappings[static_cast<std::size_t>(mode.mapping)];
    const int channels = id.channels;
    for (int c = 0; c < channels; ++c) {
        const std::size_t cs = static_cast<std::size_t>(c);
        const Floor& f = m_floors[map.floor[map.mux[cs]]];
        float* out = m_floor[cs].data();
        const bool used = f.type == 1 ? decode_floor1(f, br, n2, out) : decode_floor0(f, br, blockflag, n2, out);
        m_unused[cs] = used ? 0 : 1;
        m_no_residue[cs] = m_unused[cs];
    }
    for (const auto& step : map.coupling) {
        if (!m_no_residue[step.first] || !m_no_residue[step.second]) {
            m_no_residue[step.first] = 0;
            m_no_residue[step.second] = 0;
        }
    }
    for (int c = 0; c < channels; ++c) std::fill(m_res[static_cast<std::size_t>(c)].begin(), m_res[static_cast<std::size_t>(c)].begin() + n2, 0.0f);
    for (int s = 0; s < map.submaps; ++s) {
        int count = 0;
        bool skips[256];
        for (int c = 0; c < channels; ++c) {
            const std::size_t cs = static_cast<std::size_t>(c);
            if (map.mux[cs] != s) continue;
            m_vec_ptr[static_cast<std::size_t>(count)] = m_res[cs].data();
            skips[count] = m_no_residue[cs] != 0;
            ++count;
        }
        if (count) decode_residue(m_residues[map.residue[s]], br, m_vec_ptr.data(), skips, count, n2);
    }
    for (std::size_t i = map.coupling.size(); i-- > 0;) {
        float* mag = m_res[map.coupling[i].first].data();
        float* ang = m_res[map.coupling[i].second].data();
        for (int j = 0; j < n2; ++j) {
            const float m = mag[j];
            const float a = ang[j];
            float nm;
            float na;
            if (m > 0.0f) {
                if (a > 0.0f) {
                    nm = m;
                    na = m - a;
                } else {
                    na = m;
                    nm = m + a;
                }
            } else {
                if (a > 0.0f) {
                    nm = m;
                    na = m + a;
                } else {
                    na = m;
                    nm = m - a;
                }
            }
            mag[j] = nm;
            ang[j] = na;
        }
    }
    for (int c = 0; c < channels; ++c) {
        const std::size_t cs = static_cast<std::size_t>(c);
        float* raw = m_raw[cs].data();
        if (m_unused[cs]) {
            std::fill(raw, raw + n, 0.0f);
            continue;
        }
        float* res = m_res[cs].data();
        const float* fl = m_floor[cs].data();
        for (int j = 0; j < n2; ++j) res[j] *= fl[j];
        m_imdct[blockflag].run(res, raw);
    }
    if (m_prev_n) {
        const int pn = m_prev_n;
        const int s = std::min(pn, n) / 2;
        const float* w = m_slope[s == id.blocksize[0] / 2 ? 0 : 1].data();
        const int a = pn / 4 - s / 2;
        const int b = pn / 4 + s / 2;
        const int len = pn / 4 + n / 4;
        const int off = n / 4 - pn / 4;
        for (int c = 0; c < channels; ++c) {
            const std::size_t cs = static_cast<std::size_t>(c);
            float* out = m_out[cs].data();
            const float* prev = m_prev[cs].data();
            const float* raw = m_raw[cs].data();
            for (int k = 0; k < a; ++k) out[k] = prev[k];
            for (int k = a; k < b; ++k) {
                const int i = k - a;
                out[k] = prev[k] * w[s - 1 - i] + raw[k + off] * w[i];
            }
            for (int k = b; k < len; ++k) out[k] = raw[k + off];
        }
        frames = static_cast<std::size_t>(len);
    }
    for (int c = 0; c < channels; ++c) {
        const std::size_t cs = static_cast<std::size_t>(c);
        std::copy(m_raw[cs].begin() + n2, m_raw[cs].begin() + n, m_prev[cs].begin());
    }
    m_prev_n = n;
    return true;
}

void wave_order(int channels, std::vector<int>& map) {
    map.resize(static_cast<std::size_t>(channels));
    for (int i = 0; i < channels; ++i) map[static_cast<std::size_t>(i)] = i;
    static const int o3[] = { 0, 2, 1 };
    static const int o5[] = { 0, 2, 1, 3, 4 };
    static const int o6[] = { 0, 2, 1, 5, 3, 4 };
    static const int o7[] = { 0, 2, 1, 6, 5, 3, 4 };
    static const int o8[] = { 0, 2, 1, 7, 5, 6, 3, 4 };
    const int* table = nullptr;
    switch (channels) {
        case 3: table = o3; break;
        case 5: table = o5; break;
        case 6: table = o6; break;
        case 7: table = o7; break;
        case 8: table = o8; break;
        default: break;
    }
    if (table) {
        for (int i = 0; i < channels; ++i) map[static_cast<std::size_t>(i)] = table[i];
    }
}

struct Link {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t audio_start = 0;
    std::uint32_t serial = 0;
    bool vorbis = false;
    std::int64_t last_granule = -1;
    std::int64_t begin = 0;
    std::uint64_t first_frame = 0;
    std::uint64_t frames = 0;
};

struct Queued {
    std::vector<std::uint8_t> data;
    bool reset = false;
    std::int64_t anchor = 0;
};

std::int64_t first_end(const Decoder& dec, const std::vector<OggPacket>& group, std::size_t from, std::int64_t granule, bool lone_final, std::size_t& first) {
    first = k_npos;
    std::int64_t sum = 0;
    int prev = 0;
    for (std::size_t i = from; i < group.size(); ++i) {
        const int bs = dec.packet_blocksize(group[i].data.data(), group[i].data.size());
        if (!bs) continue;
        if (first == k_npos) {
            first = i;
        } else {
            sum += prev / 4 + bs / 4;
        }
        prev = bs;
    }
    if (lone_final) return 0;
    return granule - sum;
}

bool read_headers(OggReader& ogg, const Link& link, Decoder& dec, std::string* error, std::string* vendor, std::vector<std::pair<std::string, std::string>>* tags, std::uint64_t* audio_start) {
    if (!ogg.seek(link.start)) {
        if (error) *error = "can't seek in the Ogg file";
        return false;
    }
    ogg.set_limit(link.end);
    ogg.select_stream(link.serial);
    OggPacket packets[3];
    for (OggPacket& p : packets) {
        if (!ogg.read_packet(p)) {
            if (error) *error = "the Ogg Vorbis headers are incomplete";
            return false;
        }
    }
    if (!magic_is(packets[1].data.data(), packets[1].data.size(), 3)) {
        if (error) *error = "the Vorbis comment header is missing";
        return false;
    }
    if (!dec.setup(packets[0].data.data(), packets[0].data.size(), packets[2].data.data(), packets[2].data.size(), error)) return false;
    if (vendor && tags) parse_comments(packets[1].data.data(), packets[1].data.size(), *vendor, *tags);
    if (audio_start) *audio_start = ogg.tell();
    return true;
}

} // namespace

struct VorbisReader::Impl {
    OggReader ogg;
    std::vector<Link> links;
    std::size_t link = 0;
    bool loaded = false;
    Decoder dec;
    std::deque<Queued> queue;
    OggPacket ahead;
    bool have_ahead = false;
    bool at_link_start = true;
    bool continuity = false;
    std::int64_t pos_end = 0;
    std::int64_t next_pos = 0;
    std::vector<float> out;
    std::size_t out_frames = 0;
    std::size_t out_read = 0;
    bool done = false;
    int channels = 0;
    std::vector<int> order;

    bool next_packet(OggPacket& p) {
        if (have_ahead) {
            p = std::move(ahead);
            have_ahead = false;
            return true;
        }
        return ogg.read_packet(p);
    }

    bool peek_packet() {
        if (!have_ahead) have_ahead = ogg.read_packet(ahead);
        return have_ahead;
    }

    void reset_state() {
        queue.clear();
        have_ahead = false;
        continuity = false;
        dec.reset();
        out_frames = 0;
        out_read = 0;
        done = false;
    }

    bool load_link(std::size_t index) {
        loaded = false;
        link = index;
        reset_state();
        if (!read_headers(ogg, links[index], dec, nullptr, nullptr, nullptr, nullptr)) return false;
        loaded = true;
        at_link_start = true;
        next_pos = links[index].begin;
        return true;
    }

    bool restart_link() {
        const Link& l = links[link];
        reset_state();
        if (!ogg.seek(l.start)) return false;
        ogg.set_limit(l.end);
        ogg.select_stream(l.serial);
        OggPacket p;
        for (int i = 0; i < 3; ++i) {
            if (!ogg.read_packet(p)) return false;
        }
        at_link_start = true;
        return true;
    }

    bool load_group() {
        std::vector<OggPacket> group;
        OggPacket p;
        while (group.size() < k_max_group && next_packet(p)) {
            const bool has = p.granule >= 0;
            group.push_back(std::move(p));
            if (has) break;
        }
        if (group.empty()) return false;
        const bool final_group = group.back().eos || !peek_packet();
        const std::int64_t granule = group.back().granule;
        std::size_t reset_from = k_npos;
        for (std::size_t i = 0; i < group.size(); ++i) {
            if (group[i].after_gap) reset_from = i;
        }
        if (!continuity && reset_from == k_npos) reset_from = 0;
        std::size_t start = 0;
        std::size_t first = k_npos;
        std::int64_t anchor = 0;
        if (reset_from != k_npos) {
            if (!continuity) start = reset_from;
            anchor = first_end(dec, group, reset_from, granule, at_link_start && final_group, first);
            if (granule < 0 && !(at_link_start && final_group)) anchor = continuity ? pos_end : next_pos;
            at_link_start = false;
            continuity = true;
        }
        for (std::size_t i = start; i < group.size(); ++i) {
            Queued q;
            q.data = std::move(group[i].data);
            if (i == first) {
                q.reset = true;
                q.anchor = anchor;
            }
            queue.push_back(std::move(q));
        }
        return true;
    }

    bool produce() {
        while (!done) {
            if (queue.empty()) {
                if (load_group()) continue;
                if (link + 1 < links.size() && load_link(link + 1)) continue;
                done = true;
                return false;
            }
            Queued q = std::move(queue.front());
            queue.pop_front();
            if (q.reset) {
                dec.reset();
                pos_end = q.anchor;
            }
            std::size_t n = 0;
            if (!dec.decode(q.data.data(), q.data.size(), n) || n == 0) continue;
            const std::int64_t s = pos_end;
            pos_end += static_cast<std::int64_t>(n);
            const Link& l = links[link];
            const std::int64_t lo = std::max(s, next_pos);
            const std::int64_t hi = std::min(pos_end, l.last_granule);
            if (hi <= lo) continue;
            const std::size_t count = static_cast<std::size_t>(hi - lo);
            const std::size_t skip = static_cast<std::size_t>(lo - s);
            const std::size_t ch = static_cast<std::size_t>(channels);
            out.resize(count * ch);
            for (std::size_t c = 0; c < ch; ++c) {
                const float* src = dec.output(order[c]) + skip;
                float* dst = out.data() + c;
                for (std::size_t i = 0; i < count; ++i) dst[i * ch] = src[i];
            }
            out_frames = count;
            out_read = 0;
            next_pos = hi;
            return true;
        }
        return false;
    }
};

bool is_ogg_vorbis(const void* data, std::size_t size) noexcept {
    if (!is_ogg(data, size)) return false;
    try {
        OggReader r;
        if (!r.open(data, size)) return false;
        OggPage page;
        while (r.read_page(page)) {
            if (!page.bos()) break;
            if (magic_is(page.body.data(), page.body.size(), 1)) return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

VorbisReader::VorbisReader() = default;
VorbisReader::~VorbisReader() = default;
VorbisReader::VorbisReader(VorbisReader&&) noexcept = default;
VorbisReader& VorbisReader::operator=(VorbisReader&&) noexcept = default;

bool VorbisReader::open(const std::string& path, std::string* error) {
    close();
    m_impl = std::make_unique<Impl>();
    if (!m_impl->ogg.open(path, error)) {
        m_impl.reset();
        return false;
    }
    return finish_open(error);
}

bool VorbisReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    m_impl = std::make_unique<Impl>();
    if (!m_impl->ogg.open(data, size, error)) {
        m_impl.reset();
        return false;
    }
    return finish_open(error);
}

bool VorbisReader::finish_open(std::string* error) {
    Impl& m = *m_impl;
    auto fail = [&](const char* message) {
        if (error && message) *error = message;
        m_impl.reset();
        m_info = VorbisInfo();
        return false;
    };
    std::vector<Link> found;
    {
        OggPage page;
        bool in_bos = false;
        m.ogg.select_all_streams();
        while (m.ogg.read_page(page)) {
            if (page.bos()) {
                if (found.empty() || !in_bos) {
                    found.emplace_back();
                    found.back().start = page.offset;
                }
                in_bos = true;
                Link& l = found.back();
                if (!l.vorbis && !page.continued() && magic_is(page.body.data(), page.body.size(), 1)) {
                    l.vorbis = true;
                    l.serial = page.serial;
                }
            } else {
                if (found.empty()) continue;
                in_bos = false;
            }
            Link& l = found.back();
            if (l.vorbis && page.serial == l.serial && page.granule >= 0) l.last_granule = page.granule;
        }
        for (std::size_t i = 0; i < found.size(); ++i) found[i].end = i + 1 < found.size() ? found[i + 1].start : m.ogg.size();
    }
    if (found.empty()) return fail("not an Ogg file");
    if (!found[0].vorbis) return fail("the Ogg file has no Vorbis stream");
    m_info = VorbisInfo();
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < found.size(); ++i) {
        Link& l = found[i];
        if (!l.vorbis) {
            m_info.chain_stopped = true;
            break;
        }
        Decoder dec;
        std::string err;
        if (!read_headers(m.ogg, l, dec, &err, i == 0 ? &m_info.vendor : nullptr, i == 0 ? &m_info.tags : nullptr, &l.audio_start)) {
            if (i == 0) return fail(err.c_str());
            m_info.chain_stopped = true;
            break;
        }
        if (i == 0) {
            m_info.channels = static_cast<std::uint16_t>(dec.id.channels);
            m_info.sample_rate = dec.id.rate;
            m_info.bitrate_nominal = dec.id.bitrate_nominal > 0 ? static_cast<std::uint32_t>(dec.id.bitrate_nominal) : 0;
        } else if (dec.id.channels != m_info.channels || dec.id.rate != m_info.sample_rate) {
            m_info.chain_stopped = true;
            break;
        }
        std::vector<OggPacket> group;
        OggPacket p;
        while (group.size() < k_max_group && m.ogg.read_packet(p)) {
            const bool has = p.granule >= 0;
            group.push_back(std::move(p));
            if (has) break;
        }
        std::int64_t begin = 0;
        if (!group.empty() && group.back().granule >= 0) {
            const bool final_group = group.back().eos || !m.ogg.read_packet(p);
            std::size_t first = k_npos;
            begin = first_end(dec, group, 0, group.back().granule, final_group, first);
            if (begin < 0) begin = 0;
        }
        l.begin = begin;
        l.first_frame = total;
        l.frames = l.last_granule > begin ? static_cast<std::uint64_t>(l.last_granule - begin) : 0;
        total += l.frames;
        m.links.push_back(l);
    }
    m_info.frames = total;
    m_info.links = static_cast<std::uint32_t>(m.links.size());
    m.channels = m_info.channels;
    wave_order(m.channels, m.order);
    if (!m.load_link(0)) return fail("the Ogg Vorbis headers can't be read");
    m_position = 0;
    return true;
}

void VorbisReader::close() {
    m_impl.reset();
    m_info = VorbisInfo();
    m_position = 0;
}

bool VorbisReader::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t VorbisReader::read(float* interleaved, std::size_t frames) {
    if (!m_impl || !interleaved) return 0;
    Impl& m = *m_impl;
    const std::size_t ch = static_cast<std::size_t>(m.channels);
    std::size_t done = 0;
    while (done < frames) {
        if (m.out_read == m.out_frames) {
            if (!m.produce()) break;
        }
        const std::size_t n = std::min(frames - done, m.out_frames - m.out_read);
        std::memcpy(interleaved + done * ch, m.out.data() + m.out_read * ch, n * ch * sizeof(float));
        m.out_read += n;
        done += n;
    }
    m_position += done;
    return done;
}

std::size_t VorbisReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_impl) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool VorbisReader::seek(std::uint64_t frame) {
    if (!m_impl) return false;
    Impl& m = *m_impl;
    frame = std::min(frame, m_info.frames);
    std::size_t index = m.links.size();
    for (std::size_t i = 0; i < m.links.size(); ++i) {
        if (frame >= m.links[i].first_frame && frame < m.links[i].first_frame + m.links[i].frames) {
            index = i;
            break;
        }
    }
    if (index == m.links.size()) {
        m.reset_state();
        m.done = true;
        m_position = frame;
        return true;
    }
    if (index != m.link || !m.loaded) {
        if (!m.load_link(index)) {
            m.done = true;
            return false;
        }
    }
    const Link& l = m.links[index];
    const std::int64_t target_pos = l.begin + static_cast<std::int64_t>(frame - l.first_frame);
    std::int64_t target = target_pos - m.dec.id.blocksize[1];
    bool positioned = false;
    OggPage page;
    while (target >= 0) {
        if (!m.ogg.find_granule(l.serial, target, l.audio_start, l.end, page)) break;
        const std::size_t complete = page.packets_completed() - (page.continued() && page.packets_completed() ? 1 : 0);
        if (complete >= 1) {
            m.reset_state();
            m.ogg.set_limit(l.end);
            m.ogg.select_stream(l.serial);
            positioned = m.ogg.seek(page.offset);
            m.at_link_start = false;
            break;
        }
        target = page.granule - 1;
    }
    if (!positioned && !m.restart_link()) {
        m.done = true;
        return false;
    }
    m.next_pos = target_pos;
    m_position = frame;
    return true;
}

bool read_vorbis(const std::string& path, AudioBuffer& out, std::string* error, VorbisInfo* info) {
    VorbisReader r;
    if (!r.open(path, error)) return false;
    const std::size_t ch = r.info().channels;
    std::vector<float> data;
    const std::uint64_t expect = std::min<std::uint64_t>(r.info().frames, std::uint64_t(1) << 24);
    data.reserve(static_cast<std::size_t>(expect) * ch);
    std::vector<float> chunk(4096 * ch);
    while (true) {
        const std::size_t got = r.read(chunk.data(), 4096);
        if (got == 0) break;
        data.insert(data.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got * ch));
    }
    if (info) {
        *info = r.info();
        info->frames = data.size() / ch;
    }
    out = AudioBuffer(ch, r.info().sample_rate, std::move(data));
    return true;
}

bool read_vorbis(const void* data, std::size_t size, AudioBuffer& out, std::string* error, VorbisInfo* info) {
    VorbisReader r;
    if (!r.open(data, size, error)) return false;
    const std::size_t ch = r.info().channels;
    std::vector<float> samples;
    const std::uint64_t expect = std::min<std::uint64_t>(r.info().frames, std::uint64_t(1) << 24);
    samples.reserve(static_cast<std::size_t>(expect) * ch);
    std::vector<float> chunk(4096 * ch);
    while (true) {
        const std::size_t got = r.read(chunk.data(), 4096);
        if (got == 0) break;
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got * ch));
    }
    if (info) {
        *info = r.info();
        info->frames = samples.size() / ch;
    }
    out = AudioBuffer(ch, r.info().sample_rate, std::move(samples));
    return true;
}

struct VorbisDecoder::Impl {
    Decoder dec;
    std::vector<int> order;
    int previous = 0;
};

VorbisDecoder::VorbisDecoder() = default;
VorbisDecoder::~VorbisDecoder() = default;
VorbisDecoder::VorbisDecoder(VorbisDecoder&&) noexcept = default;
VorbisDecoder& VorbisDecoder::operator=(VorbisDecoder&&) noexcept = default;

bool VorbisDecoder::open(const std::uint8_t* headers, std::size_t size, std::string* error) {
    close();
    if (!headers || size < 3) {
        if (error) *error = "the Vorbis headers are missing";
        return false;
    }
    const std::size_t count = static_cast<std::size_t>(headers[0]) + 1;
    if (count != 3) {
        if (error) *error = "expected three Vorbis header packets";
        return false;
    }
    std::size_t pos = 1;
    std::size_t sizes[3] = { 0, 0, 0 };
    for (std::size_t i = 0; i + 1 < count; ++i) {
        std::size_t v = 0;
        while (true) {
            if (pos >= size) {
                if (error) *error = "the Vorbis header lacing is truncated";
                return false;
            }
            const std::uint8_t b = headers[pos++];
            v += b;
            if (b != 255) break;
        }
        sizes[i] = v;
    }
    if (sizes[0] + sizes[1] > size - pos) {
        if (error) *error = "the Vorbis headers are truncated";
        return false;
    }
    sizes[2] = size - pos - sizes[0] - sizes[1];
    const std::uint8_t* ident = headers + pos;
    const std::uint8_t* setup = ident + sizes[0] + sizes[1];
    return open(ident, sizes[0], setup, sizes[2], error);
}

bool VorbisDecoder::open(const std::uint8_t* ident, std::size_t ident_size, const std::uint8_t* setup, std::size_t setup_size, std::string* error) {
    close();
    if (!ident || !setup || !magic_is(ident, ident_size, 1) || !magic_is(setup, setup_size, 5)) {
        if (error) *error = "not Vorbis headers";
        return false;
    }
    std::unique_ptr<Impl> impl(new Impl());
    try {
        if (!impl->dec.setup(ident, ident_size, setup, setup_size, error)) return false;
    } catch (const std::bad_alloc&) {
        if (error) *error = "out of memory";
        return false;
    }
    wave_order(impl->dec.id.channels, impl->order);
    m_impl = std::move(impl);
    return true;
}

void VorbisDecoder::close() {
    m_impl.reset();
}

bool VorbisDecoder::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t VorbisDecoder::decode(const std::uint8_t* packet, std::size_t size, std::vector<float>& out, std::string* error) {
    out.clear();
    if (!m_impl) {
        if (error) *error = "the Vorbis decoder isn't open";
        return 0;
    }
    if (!packet || size == 0) {
        if (error) *error = "empty Vorbis packet";
        return 0;
    }
    if (error) error->clear();
    Impl& m = *m_impl;
    std::size_t n = 0;
    bool ok = false;
    try {
        ok = m.dec.decode(packet, size, n);
    } catch (const std::bad_alloc&) {
        if (error) *error = "out of memory";
        return 0;
    }
    if (!ok) {
        if (error) *error = "the Vorbis packet can't be decoded";
        return 0;
    }
    const std::size_t ch = static_cast<std::size_t>(m.dec.id.channels);
    out.resize(n * ch);
    for (std::size_t c = 0; c < ch; ++c) {
        const float* src = m.dec.output(m.order[c]);
        float* dst = out.data() + c;
        for (std::size_t i = 0; i < n; ++i) dst[i * ch] = src[i];
    }
    return n;
}

std::size_t VorbisDecoder::packet_samples(const std::uint8_t* packet, std::size_t size) const noexcept {
    if (!m_impl || !packet) return 0;
    return static_cast<std::size_t>(m_impl->dec.packet_blocksize(packet, size));
}

void VorbisDecoder::reset() {
    if (m_impl) m_impl->dec.reset();
}

std::uint32_t VorbisDecoder::sample_rate() const noexcept {
    return m_impl ? m_impl->dec.id.rate : 0;
}

std::uint16_t VorbisDecoder::channels() const noexcept {
    return m_impl ? static_cast<std::uint16_t>(m_impl->dec.id.channels) : 0;
}

} // namespace audio
} // namespace fizmo
