#include "fizmo_library.hpp"
#include "format_util.hpp"
#include "../Normal/jpeg_codec.hpp"

#include <cmath>
#include <map>

namespace fizmo {
namespace images {

namespace {

struct TiffReader {
    const std::uint8_t* p;
    std::size_t         n;
    bool                le;
    bool                big;

    std::uint64_t u(std::size_t at, unsigned bytes) const {
        if (at > n || bytes > n - at) return 0;
        std::uint64_t v = 0;
        if (le) for (unsigned i = bytes; i-- > 0;) v = (v << 8) | p[at + i];
        else for (unsigned i = 0; i < bytes; ++i) v = (v << 8) | p[at + i];
        return v;
    }
};

struct Tag {
    std::uint16_t              type = 0;
    std::vector<std::uint64_t> values;
    std::vector<double>        reals;
    std::size_t                offset = 0;
    std::uint64_t              count = 0;
};

unsigned type_size(unsigned t) {
    switch (t) {
        case 1: case 2: case 6: case 7: return 1;
        case 3: case 8: return 2;
        case 4: case 9: case 11: case 13: return 4;
        case 5: case 10: case 12: case 16: case 17: case 18: return 8;
        default: return 0;
    }
}

bool read_ifd(const TiffReader& r, std::uint64_t off, std::map<int, Tag>& tags, std::uint64_t& next) {
    const unsigned entry = r.big ? 20 : 12;
    const std::uint64_t count = r.big ? r.u(off, 8) : r.u(off, 2);
    const std::size_t base = static_cast<std::size_t>(off + (r.big ? 8 : 2));
    if (off >= r.n || count > 4096 || base + count * entry > r.n) return false;

    for (std::uint64_t i = 0; i < count; ++i) {
        const std::size_t e = base + static_cast<std::size_t>(i) * entry;
        Tag t;
        const int id = static_cast<int>(r.u(e, 2));
        t.type = static_cast<std::uint16_t>(r.u(e + 2, 2));
        t.count = r.big ? r.u(e + 4, 8) : r.u(e + 4, 4);
        const unsigned ts = type_size(t.type);
        if (!ts) continue;
        const std::uint64_t bytes = t.count * ts;
        const unsigned inline_cap = r.big ? 8 : 4;
        std::size_t at = e + (r.big ? 12 : 8);
        if (bytes > inline_cap) at = static_cast<std::size_t>(r.big ? r.u(at, 8) : r.u(at, 4));
        if (at > r.n || bytes > r.n - at) continue;
        t.offset = at;
        const std::uint64_t keep = std::min<std::uint64_t>(t.count, 1u << 24);

        for (std::uint64_t k = 0; k < keep; ++k) {
            const std::size_t a = at + static_cast<std::size_t>(k) * ts;
            switch (t.type) {
                case 5: case 10: {
                    const std::uint64_t num = r.u(a, 4), den = r.u(a + 4, 4);
                    t.reals.push_back(den ? static_cast<double>(num) / static_cast<double>(den) : 0.0);
                    t.values.push_back(den ? num / den : 0);
                    break;
                }
                default: t.values.push_back(r.u(a, ts)); break;
            }
        }

        tags[id] = std::move(t);
    }

    next = r.big ? r.u(base + static_cast<std::size_t>(count) * entry, 8) : r.u(base + static_cast<std::size_t>(count) * entry, 4);
    return true;
}

std::uint64_t tag1(const std::map<int, Tag>& tags, int id, std::uint64_t def) {
    auto it = tags.find(id);
    if (it == tags.end() || it->second.values.empty()) return def;
    return it->second.values[0];
}

const std::vector<std::uint64_t>* tagv(const std::map<int, Tag>& tags, int id) {
    auto it = tags.find(id);
    return it == tags.end() ? nullptr : &it->second.values;
}

bool unpack_bits(const std::uint8_t* in, std::size_t n, std::vector<std::uint8_t>& out, std::size_t expected) {
    out.clear();
    std::size_t i = 0;
    while (i < n && out.size() < expected) {
        const int c = static_cast<std::int8_t>(in[i++]);
        if (c >= 0) {
            const std::size_t k = static_cast<std::size_t>(c) + 1;
            if (i + k > n) return false;
            out.insert(out.end(), in + i, in + i + k);
            i += k;
        } else if (c != -128) {
            if (i >= n) return false;
            out.insert(out.end(), static_cast<std::size_t>(1 - c), in[i++]);
        }
    }
    return true;
}

bool lzw_tiff(const std::uint8_t* in, std::size_t n, std::vector<std::uint8_t>& out, std::size_t expected) {
    out.clear();
    out.reserve(expected);
    std::vector<std::uint16_t> prefix(4096);
    std::vector<std::uint8_t> suffix(4096), first(4096), stack;
    for (unsigned i = 0; i < 256; ++i) { suffix[i] = first[i] = static_cast<std::uint8_t>(i); prefix[i] = 0xFFFF; }
    unsigned width = 9, next = 258;
    int prev = -1;
    std::uint32_t acc = 0;
    unsigned bits = 0;
    std::size_t pos = 0;

    while (out.size() < expected) {
        while (bits < width && pos < n) {
            acc = (acc << 8) | in[pos++];
            bits += 8;
        }
        if (bits < width) break;
        const unsigned code = (acc >> (bits - width)) & ((1u << width) - 1);
        bits -= width;

        if (code == 256) {
            width = 9;
            next = 258;
            prev = -1;
            continue;
        }
        if (code == 257) break;

        if (prev < 0) {
            if (code > 255) return false;
            out.push_back(static_cast<std::uint8_t>(code));
            prev = static_cast<int>(code);
            continue;
        }

        unsigned c = code;
        std::uint8_t head;
        if (code < next) head = first[code];
        else if (code == next) {
            head = first[prev];
            stack.push_back(head);
            c = static_cast<unsigned>(prev);
        } else {
            return false;
        }

        while (c > 255) {
            stack.push_back(suffix[c]);
            c = prefix[c];
        }
        stack.push_back(static_cast<std::uint8_t>(c));
        while (!stack.empty()) {
            out.push_back(stack.back());
            stack.pop_back();
        }

        if (next < 4096) {
            prefix[next] = static_cast<std::uint16_t>(prev);
            suffix[next] = head;
            first[next] = first[prev];
            ++next;
        }
        if (next + 1 >= (1u << width) && width < 12) ++width;
        prev = static_cast<int>(code);
    }

    return true;
}

struct Layout {
    unsigned w = 0, h = 0, spp = 1, bps = 8, photometric = 1, planar = 1, compression = 1, predictor = 1, sample_format = 1, fill_order = 1, orientation = 1;
    int      alpha_channel = -1;
    bool     premultiplied = false;
    std::vector<std::uint64_t> colormap;
};

double sample_value(const std::uint8_t* row, std::size_t index, const Layout& L, bool le) {
    const unsigned b = L.bps;
    if (b == 8) return L.sample_format == 2 ? static_cast<std::int8_t>(row[index]) + 128.0 : row[index];
    if (b == 16) {
        const std::uint8_t* q = row + index * 2;
        const std::uint16_t v = le ? static_cast<std::uint16_t>(q[0] | (q[1] << 8)) : static_cast<std::uint16_t>((q[0] << 8) | q[1]);
        return L.sample_format == 2 ? static_cast<std::int16_t>(v) + 32768.0 : v;
    }
    if (b == 32) {
        const std::uint8_t* q = row + index * 4;
        const std::uint32_t v = le ? (q[0] | (q[1] << 8) | (static_cast<std::uint32_t>(q[2]) << 16) | (static_cast<std::uint32_t>(q[3]) << 24))
                                   : ((static_cast<std::uint32_t>(q[0]) << 24) | (static_cast<std::uint32_t>(q[1]) << 16) | (q[2] << 8) | q[3]);
        if (L.sample_format == 3) {
            float f;
            std::memcpy(&f, &v, 4);
            return f;
        }
        return L.sample_format == 2 ? static_cast<std::int32_t>(v) + 2147483648.0 : v;
    }
    const std::size_t bit = index * b;
    std::uint8_t byte = row[bit / 8];
    if (L.fill_order == 2) {
        std::uint8_t r = 0;
        for (int k = 0; k < 8; ++k) r |= static_cast<std::uint8_t>(((byte >> k) & 1u) << (7 - k));
        byte = r;
    }
    return (byte >> (8 - b - bit % 8)) & ((1u << b) - 1);
}

std::uint8_t to8(double v, const Layout& L) {
    if (L.sample_format == 3) {
        if (!(v > 0)) return 0;
        if (v >= 1) return 255;
        return static_cast<std::uint8_t>(std::lround(v * 255.0));
    }
    const double max = L.bps >= 32 ? 4294967295.0 : static_cast<double>((1ull << L.bps) - 1);
    return static_cast<std::uint8_t>(std::lround(v * 255.0 / max));
}

void undo_predictor(std::vector<std::uint8_t>& buf, std::size_t rows, std::size_t row_bytes, unsigned width, unsigned spp, const Layout& L, bool le) {
    if (L.predictor == 2) {
        for (std::size_t y = 0; y < rows; ++y) {
            std::uint8_t* row = buf.data() + y * row_bytes;
            if (L.bps == 8) {
                for (std::size_t i = spp; i < static_cast<std::size_t>(width) * spp; ++i) row[i] = static_cast<std::uint8_t>(row[i] + row[i - spp]);
            } else if (L.bps == 16) {
                auto get = [&](std::size_t i) -> unsigned { return le ? (row[2 * i] | (row[2 * i + 1] << 8)) : ((row[2 * i] << 8) | row[2 * i + 1]); };
                auto put = [&](std::size_t i, unsigned v) {
                    if (le) { row[2 * i] = static_cast<std::uint8_t>(v); row[2 * i + 1] = static_cast<std::uint8_t>(v >> 8); }
                    else { row[2 * i] = static_cast<std::uint8_t>(v >> 8); row[2 * i + 1] = static_cast<std::uint8_t>(v); }
                };
                for (std::size_t i = spp; i < static_cast<std::size_t>(width) * spp; ++i) put(i, (get(i) + get(i - spp)) & 0xFFFF);
            } else if (L.bps == 32) {
                auto get = [&](std::size_t i) -> std::uint32_t {
                    const std::uint8_t* q = row + 4 * i;
                    return le ? (q[0] | (q[1] << 8) | (static_cast<std::uint32_t>(q[2]) << 16) | (static_cast<std::uint32_t>(q[3]) << 24)) : ((static_cast<std::uint32_t>(q[0]) << 24) | (static_cast<std::uint32_t>(q[1]) << 16) | (q[2] << 8) | q[3]);
                };
                auto put = [&](std::size_t i, std::uint32_t v) {
                    std::uint8_t* q = row + 4 * i;
                    for (int k = 0; k < 4; ++k) q[le ? k : 3 - k] = static_cast<std::uint8_t>(v >> (8 * k));
                };
                for (std::size_t i = spp; i < static_cast<std::size_t>(width) * spp; ++i) put(i, get(i) + get(i - spp));
            }
        }
    } else if (L.predictor == 3 && L.sample_format == 3) {
        const unsigned bytes = L.bps / 8;
        const std::size_t count = static_cast<std::size_t>(width) * spp;
        std::vector<std::uint8_t> tmp(count * bytes);
        for (std::size_t y = 0; y < rows; ++y) {
            std::uint8_t* row = buf.data() + y * row_bytes;
            for (std::size_t i = spp; i < count * bytes; ++i) row[i] = static_cast<std::uint8_t>(row[i] + row[i - spp]);
            for (std::size_t i = 0; i < count; ++i)
                for (unsigned b = 0; b < bytes; ++b) {
                    const std::uint8_t v = row[b * count + i];
                    tmp[i * bytes + (le ? bytes - 1 - b : b)] = v;
                }
            std::memcpy(row, tmp.data(), tmp.size());
        }
    }
}

bool decompress(const std::uint8_t* data, std::size_t size, const Layout& L, std::size_t expected, std::vector<std::uint8_t>& out) {
    switch (L.compression) {
        case 1:
            out.assign(data, data + std::min(size, expected));
            out.resize(expected, 0);
            return true;
        case 5:
            if (!lzw_tiff(data, size, out, expected)) return false;
            out.resize(expected, 0);
            return true;
        case 8:
        case 32946:
            fmt::zlib_inflate(data, size, out, expected);
            out.resize(expected, 0);
            return true;
        case 32773:
            if (!unpack_bits(data, size, out, expected)) return false;
            out.resize(expected, 0);
            return true;
        default:
            return false;
    }
}

bool decode_page(const TiffReader& r, const std::map<int, Tag>& tags, BitmapImage& out, std::string* error) {
    Layout L;
    L.w = static_cast<unsigned>(tag1(tags, 256, 0));
    L.h = static_cast<unsigned>(tag1(tags, 257, 0));
    L.spp = static_cast<unsigned>(tag1(tags, 277, 1));
    L.bps = static_cast<unsigned>(tag1(tags, 258, 1));
    L.compression = static_cast<unsigned>(tag1(tags, 259, 1));
    L.photometric = static_cast<unsigned>(tag1(tags, 262, L.spp >= 3 ? 2 : 1));
    L.planar = static_cast<unsigned>(tag1(tags, 284, 1));
    L.predictor = static_cast<unsigned>(tag1(tags, 317, 1));
    L.sample_format = static_cast<unsigned>(tag1(tags, 339, 1));
    L.fill_order = static_cast<unsigned>(tag1(tags, 266, 1));
    L.orientation = static_cast<unsigned>(tag1(tags, 274, 1));
    if (const auto* cm = tagv(tags, 320)) L.colormap = *cm;
    if (!fmt::sane_dimensions(L.w, L.h)) return fmt::fail(error, "bad TIFF dimensions");
    if (L.spp < 1 || L.spp > 16) return fmt::fail(error, "bad TIFF samples per pixel");
    if (L.bps != 1 && L.bps != 2 && L.bps != 4 && L.bps != 8 && L.bps != 16 && L.bps != 32) return fmt::fail(error, "unsupported TIFF bit depth");
    if (L.sample_format == 3 && L.bps != 32) return fmt::fail(error, "unsupported TIFF float depth");

    unsigned color_channels = L.photometric == 2 ? 3 : L.photometric == 5 ? 4 : L.photometric == 6 ? 3 : 1;
    if (L.spp < color_channels) return fmt::fail(error, "TIFF has too few samples");
    if (const auto* extra = tagv(tags, 338)) {
        if (!extra->empty() && (*extra)[0] != 0) {
            L.alpha_channel = static_cast<int>(color_channels);
            L.premultiplied = (*extra)[0] == 1;
        }
    } else if (L.spp > color_channels && L.photometric == 2 && L.spp == 4) {
        L.alpha_channel = 3;
    }

    if (L.photometric == 6 && L.compression != 7) {
        const auto* sub = tagv(tags, 530);
        if (sub && sub->size() >= 2 && ((*sub)[0] != 1 || (*sub)[1] != 1)) return fmt::fail(error, "subsampled YCbCr TIFF is not supported");
    }

    const bool tiled = tags.count(322) && tags.count(324);
    const unsigned tw = tiled ? static_cast<unsigned>(tag1(tags, 322, 0)) : L.w;
    const unsigned th = tiled ? static_cast<unsigned>(tag1(tags, 323, 0)) : static_cast<unsigned>(std::min<std::uint64_t>(tag1(tags, 278, L.h), L.h));
    if (!tw || !th) return fmt::fail(error, "bad TIFF strip layout");
    const auto* offsets = tagv(tags, tiled ? 324 : 273);
    const auto* counts = tagv(tags, tiled ? 325 : 279);
    if (!offsets || offsets->empty()) return fmt::fail(error, "TIFF has no image data");

    const unsigned planes = L.planar == 2 ? L.spp : 1;
    const unsigned spp_plane = L.planar == 2 ? 1 : L.spp;
    const unsigned across = (L.w + tw - 1) / tw, down = (L.h + th - 1) / th;
    const std::size_t blocks_per_plane = static_cast<std::size_t>(across) * down;
    if (offsets->size() < blocks_per_plane * planes) return fmt::fail(error, "TIFF strip table is truncated");
    const std::size_t row_bytes = (static_cast<std::size_t>(tw) * spp_plane * L.bps + 7) / 8;

    std::vector<std::vector<std::uint8_t>> plane_data(planes, std::vector<std::uint8_t>(((static_cast<std::size_t>(L.w) * spp_plane * L.bps + 7) / 8) * L.h, 0));
    const std::size_t full_row = (static_cast<std::size_t>(L.w) * spp_plane * L.bps + 7) / 8;
    fmt::Rgba img(L.w, L.h);
    bool jpeg_rgb_done = false;

    for (unsigned pl = 0; pl < planes; ++pl)
        for (std::size_t bi = 0; bi < blocks_per_plane; ++bi) {
            const std::size_t idx = pl * blocks_per_plane + bi;
            const std::uint64_t off = (*offsets)[idx];
            std::uint64_t cnt = counts && idx < counts->size() ? (*counts)[idx] : r.n - std::min<std::uint64_t>(off, r.n);
            if (off >= r.n) continue;
            cnt = std::min<std::uint64_t>(cnt, r.n - off);
            const unsigned bx = static_cast<unsigned>(bi % across), by = static_cast<unsigned>(bi / across);
            const unsigned rows = tiled ? th : std::min(th, L.h - by * th);

            if (L.compression == 7) {
                std::vector<std::uint8_t> jpg;
                const auto it = tags.find(347);
                const std::uint8_t* d = r.p + off;
                if (it != tags.end() && it->second.count > 4) {
                    const std::uint8_t* t = r.p + it->second.offset;
                    jpg.assign(t, t + it->second.count - 2);
                    jpg.insert(jpg.end(), d + 2, d + cnt);
                } else {
                    jpg.assign(d, d + cnt);
                }
                BitmapImage tile;
                if (!decode_jpeg(jpg.data(), jpg.size(), tile, error)) return false;
                for (unsigned y = 0; y < rows && by * th + y < L.h; ++y)
                    for (unsigned x = 0; x < tw && bx * tw + x < L.w; ++x) {
                        const colors::Color c = tile.get_pixel(x, y);
                        std::uint8_t* dst = img.at(bx * tw + x, by * th + y);
                        dst[0] = c.red(); dst[1] = c.green(); dst[2] = c.blue(); dst[3] = 255;
                    }
                jpeg_rgb_done = true;
                continue;
            }

            std::vector<std::uint8_t> raw;
            if (!decompress(r.p + off, static_cast<std::size_t>(cnt), L, row_bytes * rows, raw)) return fmt::fail(error, "unsupported or corrupt TIFF compression");
            undo_predictor(raw, rows, row_bytes, tw, spp_plane, L, r.le);

            for (unsigned y = 0; y < rows && by * th + y < L.h; ++y) {
                const std::size_t dst_row = static_cast<std::size_t>(by * th + y) * full_row;
                if (L.bps % 8 == 0) {
                    const std::size_t bpp = static_cast<std::size_t>(spp_plane) * L.bps / 8;
                    const std::size_t n = static_cast<std::size_t>(std::min(tw, L.w - bx * tw)) * bpp;
                    std::memcpy(&plane_data[pl][dst_row + static_cast<std::size_t>(bx) * tw * bpp], raw.data() + y * row_bytes, n);
                } else {
                    for (unsigned x = 0; x < tw && bx * tw + x < L.w; ++x)
                        for (unsigned s = 0; s < spp_plane; ++s) {
                            const std::size_t src_bit = (static_cast<std::size_t>(x) * spp_plane + s) * L.bps;
                            const std::size_t dst_bit = (static_cast<std::size_t>(bx * tw + x) * spp_plane + s) * L.bps;
                            const unsigned v = (raw[y * row_bytes + src_bit / 8] >> (8 - L.bps - src_bit % 8)) & ((1u << L.bps) - 1);
                            plane_data[pl][dst_row + dst_bit / 8] |= static_cast<std::uint8_t>(v << (8 - L.bps - dst_bit % 8));
                        }
                }
            }
        }

    int cmap_mode = 2;
    if (!L.colormap.empty()) {
        bool small = true, low_zero = true;
        for (auto v : L.colormap) {
            if (v > 255) small = false;
            if (v & 255) low_zero = false;
        }
        cmap_mode = small ? 0 : low_zero ? 1 : 2;
    }

    if (!jpeg_rgb_done) {
        Layout LL = L;
        if (L.compression != 1 && L.fill_order == 2) LL.fill_order = 1;
        std::vector<double> s(L.spp);

        for (unsigned y = 0; y < L.h; ++y)
            for (unsigned x = 0; x < L.w; ++x) {
                for (unsigned c = 0; c < L.spp; ++c) {
                    if (planes == 1) s[c] = sample_value(plane_data[0].data() + y * full_row, static_cast<std::size_t>(x) * L.spp + c, LL, r.le);
                    else s[c] = sample_value(plane_data[c].data() + y * full_row, x, LL, r.le);
                }

                std::uint8_t* d = img.at(x, y);
                const double max = L.sample_format == 3 ? 1.0 : (L.bps >= 32 ? 4294967295.0 : static_cast<double>((1ull << L.bps) - 1));

                switch (L.photometric) {
                    case 0: d[0] = d[1] = d[2] = static_cast<std::uint8_t>(255 - to8(s[0], LL)); break;
                    case 1: d[0] = d[1] = d[2] = to8(s[0], LL); break;
                    case 3: {
                        const std::size_t n = std::size_t(1) << L.bps;
                        const std::size_t i = static_cast<std::size_t>(s[0]);
                        if (L.colormap.size() >= 3 * n && i < n) {
                            for (int c = 0; c < 3; ++c) {
                                const std::uint64_t v = L.colormap[static_cast<std::size_t>(c) * n + i];
                                if (cmap_mode == 0) d[c] = static_cast<std::uint8_t>(v);
                                else if (cmap_mode == 1) d[c] = static_cast<std::uint8_t>(v >> 8);
                                else d[c] = static_cast<std::uint8_t>(std::lround(static_cast<double>(v) * 255.0 / 65535.0));
                            }
                        }
                        break;
                    }
                    case 5: {
                        const double C = s[0] / max, M = s[1] / max, Y = s[2] / max, K = s[3] / max;
                        d[0] = static_cast<std::uint8_t>(std::lround(255.0 * (1 - C) * (1 - K)));
                        d[1] = static_cast<std::uint8_t>(std::lround(255.0 * (1 - M) * (1 - K)));
                        d[2] = static_cast<std::uint8_t>(std::lround(255.0 * (1 - Y) * (1 - K)));
                        break;
                    }
                    case 6: {
                        const double Y = s[0] * 255.0 / max, Cb = s[1] * 255.0 / max - 128.0, Cr = s[2] * 255.0 / max - 128.0;
                        d[0] = static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, std::round(Y + 1.402 * Cr))));
                        d[1] = static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, std::round(Y - 0.344136 * Cb - 0.714136 * Cr))));
                        d[2] = static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, std::round(Y + 1.772 * Cb))));
                        break;
                    }
                    default: d[0] = to8(s[0], LL); d[1] = to8(s[1], LL); d[2] = to8(s[2], LL); break;
                }

                d[3] = 255;
                if (L.alpha_channel >= 0 && static_cast<unsigned>(L.alpha_channel) < L.spp) {
                    const double a = L.sample_format == 3 ? std::max(0.0, std::min(1.0, s[static_cast<std::size_t>(L.alpha_channel)])) : s[static_cast<std::size_t>(L.alpha_channel)] / max;
                    d[3] = static_cast<std::uint8_t>(std::lround(a * 255.0));
                    if (L.premultiplied && L.photometric == 2) {
                        for (int c = 0; c < 3; ++c) {
                            const double v = L.sample_format == 3 ? s[static_cast<std::size_t>(c)] : s[static_cast<std::size_t>(c)] / max;
                            d[c] = a > 0 ? static_cast<std::uint8_t>(std::lround(std::min(1.0, std::max(0.0, v / a)) * 255.0)) : 0;
                        }
                    }
                }
            }
    }

    if (L.orientation > 1 && L.orientation <= 8) {
        const bool swap = L.orientation >= 5;
        const unsigned ow = swap ? L.h : L.w, oh = swap ? L.w : L.h;
        fmt::Rgba o(ow, oh);
        for (unsigned y = 0; y < L.h; ++y)
            for (unsigned x = 0; x < L.w; ++x) {
                unsigned nx = x, ny = y;
                switch (L.orientation) {
                    case 2: nx = L.w - 1 - x; break;
                    case 3: nx = L.w - 1 - x; ny = L.h - 1 - y; break;
                    case 4: ny = L.h - 1 - y; break;
                    case 5: nx = y; ny = x; break;
                    case 6: nx = L.h - 1 - y; ny = x; break;
                    case 7: nx = L.h - 1 - y; ny = L.w - 1 - x; break;
                    case 8: nx = y; ny = L.w - 1 - x; break;
                    default: break;
                }
                std::memcpy(o.at(nx, ny), img.at(x, y), 4);
            }
        img = std::move(o);
    }

    out = img.to_bitmap();
    return true;
}

bool open_tiff(const std::uint8_t* data, std::size_t size, TiffReader& r, std::uint64_t& first) {
    if (size < 8) return false;
    r.p = data;
    r.n = size;
    if (data[0] == 'I' && data[1] == 'I') r.le = true;
    else if (data[0] == 'M' && data[1] == 'M') r.le = false;
    else return false;
    const std::uint64_t magic = r.u(2, 2);
    if (magic == 42) {
        r.big = false;
        first = r.u(4, 4);
    } else if (magic == 43) {
        if (size < 16 || r.u(4, 2) != 8) return false;
        r.big = true;
        first = r.u(8, 8);
    } else {
        return false;
    }
    return true;
}

} // namespace

bool is_tiff(const std::uint8_t* data, std::size_t size) noexcept {
    if (size < 8) return false;
    return (data[0] == 'I' && data[1] == 'I' && (data[2] == 42 || data[2] == 43) && data[3] == 0) || (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && (data[3] == 42 || data[3] == 43));
}

bool decode_tiff_pages(const std::uint8_t* data, std::size_t size, std::vector<BitmapImage>& out, std::string* error) {
    out.clear();
    TiffReader r{};
    std::uint64_t off = 0;
    if (!open_tiff(data, size, r, off)) return fmt::fail(error, "not a TIFF file");
    std::vector<std::uint64_t> seen;

    while (off && off < size && seen.size() < 4096) {
        if (std::find(seen.begin(), seen.end(), off) != seen.end()) break;
        seen.push_back(off);
        std::map<int, Tag> tags;
        std::uint64_t next = 0;
        if (!read_ifd(r, off, tags, next)) break;
        BitmapImage page;
        std::string err;
        if (decode_page(r, tags, page, &err)) out.push_back(std::move(page));
        else if (out.empty() && error) *error = err;
        off = next;
    }

    if (out.empty()) return error && !error->empty() ? false : fmt::fail(error, "TIFF has no decodable pages");
    return true;
}

bool decode_tiff(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    std::vector<BitmapImage> pages;
    if (!decode_tiff_pages(data, size, pages, error)) return false;
    out = std::move(pages.front());
    return true;
}

bool encode_tiff(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha, bool compress) {
    out.clear();
    if (!img.is_valid_image()) return false;
    const unsigned w = img.width(), h = img.height(), spp = alpha ? 4 : 3;
    const std::size_t row_bytes = static_cast<std::size_t>(w) * spp;
    const unsigned rows_per_strip = static_cast<unsigned>(std::max<std::size_t>(1, std::min<std::size_t>(h, 65536 / std::max<std::size_t>(row_bytes, 1))));
    const unsigned strips = (h + rows_per_strip - 1) / rows_per_strip;
    const std::vector<std::uint8_t> rgba = img.to_rgba();
    std::vector<std::vector<std::uint8_t>> data(strips);

    for (unsigned s = 0; s < strips; ++s) {
        const unsigned y0 = s * rows_per_strip, y1 = std::min(h, y0 + rows_per_strip);
        std::vector<std::uint8_t> raw;
        raw.reserve((y1 - y0) * row_bytes);
        for (unsigned y = y0; y < y1; ++y) {
            const std::size_t start = raw.size();
            for (unsigned x = 0; x < w; ++x) {
                const std::uint8_t* p = rgba.data() + (static_cast<std::size_t>(y) * w + x) * 4;
                raw.insert(raw.end(), p, p + spp);
            }
            if (compress)
                for (std::size_t i = start + row_bytes; i-- > start + spp;) raw[i] = static_cast<std::uint8_t>(raw[i] - raw[i - spp]);
        }
        if (compress) {
            if (!fmt::zlib_deflate(raw.data(), raw.size(), data[s], 6)) return false;
        } else {
            data[s] = std::move(raw);
        }
    }

    out = { 'I', 'I', 42, 0, 0, 0, 0, 0 };
    std::vector<std::uint32_t> offsets(strips), counts(strips);
    for (unsigned s = 0; s < strips; ++s) {
        offsets[s] = static_cast<std::uint32_t>(out.size());
        counts[s] = static_cast<std::uint32_t>(data[s].size());
        out.insert(out.end(), data[s].begin(), data[s].end());
        if (out.size() & 1) out.push_back(0);
    }

    auto put_array = [&](const std::vector<std::uint32_t>& v) -> std::uint32_t {
        const std::uint32_t at = static_cast<std::uint32_t>(out.size());
        for (auto x : v) fmt::put_le32(out, x);
        return at;
    };

    const std::uint32_t bps_at = static_cast<std::uint32_t>(out.size());
    for (unsigned i = 0; i < spp; ++i) fmt::put_le16(out, 8);
    const std::uint32_t off_at = strips > 1 ? put_array(offsets) : offsets[0];
    const std::uint32_t cnt_at = strips > 1 ? put_array(counts) : counts[0];
    const std::uint32_t res_at = static_cast<std::uint32_t>(out.size());
    fmt::put_le32(out, 72);
    fmt::put_le32(out, 1);
    if (out.size() & 1) out.push_back(0);
    const std::uint32_t ifd = static_cast<std::uint32_t>(out.size());
    fmt::set_le32(out, 4, ifd);

    struct Entry { std::uint16_t tag, type; std::uint32_t count, value; };
    std::vector<Entry> e = {
        { 256, 4, 1, w },
        { 257, 4, 1, h },
        { 258, 3, spp, bps_at },
        { 259, 3, 1, compress ? 8u : 1u },
        { 262, 3, 1, 2 },
        { 273, 4, strips, off_at },
        { 277, 3, 1, spp },
        { 278, 4, 1, rows_per_strip },
        { 279, 4, strips, cnt_at },
        { 282, 5, 1, res_at },
        { 283, 5, 1, res_at },
        { 284, 3, 1, 1 },
        { 296, 3, 1, 2 },
    };
    if (compress) e.push_back({ 317, 3, 1, 2 });
    if (alpha) e.push_back({ 338, 3, 1, 2 });
    std::sort(e.begin(), e.end(), [](const Entry& a, const Entry& b) { return a.tag < b.tag; });

    fmt::put_le16(out, static_cast<std::uint32_t>(e.size()));
    for (const Entry& x : e) {
        fmt::put_le16(out, x.tag);
        fmt::put_le16(out, x.type);
        fmt::put_le32(out, x.count);
        if (x.type == 3 && x.count == 1) {
            fmt::put_le16(out, x.value);
            fmt::put_le16(out, 0);
        } else if (x.tag == 258 && spp * 2 <= 4) {
            fmt::put_le32(out, 0x00080008u);
        } else {
            fmt::put_le32(out, x.value);
        }
    }
    fmt::put_le32(out, 0);
    return true;
}

} // namespace images
} // namespace fizmo
