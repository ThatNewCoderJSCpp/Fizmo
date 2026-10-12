#include "fizmo_library.hpp"
#include "format_util.hpp"

#include <unordered_map>

namespace fizmo {
namespace images {

namespace {

bool lzw_decode(const std::vector<std::uint8_t>& in, unsigned min_size, std::vector<std::uint8_t>& out, std::size_t expected) {
    if (min_size < 2 || min_size > 11) return false;
    out.clear();
    out.reserve(expected);
    const unsigned clear = 1u << min_size, eoi = clear + 1;
    std::vector<std::uint16_t> prefix(4096);
    std::vector<std::uint8_t> suffix(4096), first(4096);
    std::vector<std::uint8_t> stack;
    stack.reserve(4096);
    for (unsigned i = 0; i < clear; ++i) { suffix[i] = static_cast<std::uint8_t>(i); first[i] = static_cast<std::uint8_t>(i); prefix[i] = 0xFFFF; }
    unsigned size = min_size + 1, next = eoi + 1;
    int prev = -1;
    std::uint32_t acc = 0;
    unsigned nbits = 0;
    std::size_t pos = 0;

    while (out.size() < expected) {
        while (nbits < size && pos < in.size()) {
            acc |= static_cast<std::uint32_t>(in[pos++]) << nbits;
            nbits += 8;
        }

        if (nbits < size) break;
        const unsigned code = acc & ((1u << size) - 1u);
        acc >>= size;
        nbits -= size;

        if (code == clear) {
            size = min_size + 1;
            next = eoi + 1;
            prev = -1;
            continue;
        }

        if (code == eoi) break;

        if (prev < 0) {
            if (code >= clear) return false;
            out.push_back(static_cast<std::uint8_t>(code));
            prev = static_cast<int>(code);
            continue;
        }

        unsigned c = code;
        std::uint8_t head;

        if (code < next) {
            head = first[code];
        } else if (code == next) {
            head = first[prev];
            stack.push_back(head);
            c = static_cast<unsigned>(prev);
        } else {
            return false;
        }

        while (c >= clear) {
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
            if (next == (1u << size) && size < 12) ++size;
        }

        prev = static_cast<int>(code);
    }

    if (out.size() > expected) out.resize(expected);
    return true;
}

void lzw_encode(const std::vector<std::uint8_t>& idx, unsigned min_size, std::vector<std::uint8_t>& out) {
    const unsigned clear = 1u << min_size, eoi = clear + 1;
    std::vector<std::uint8_t> bytes;
    std::uint32_t acc = 0;
    unsigned nbits = 0;
    unsigned size = min_size + 1, next = eoi + 1;

    auto emit = [&](unsigned code) {
        acc |= static_cast<std::uint32_t>(code) << nbits;
        nbits += size;
        while (nbits >= 8) {
            bytes.push_back(static_cast<std::uint8_t>(acc));
            acc >>= 8;
            nbits -= 8;
        }
    };

    const std::size_t hsize = 8209;
    std::vector<std::uint32_t> keys(hsize, 0xFFFFFFFFu);
    std::vector<std::uint16_t> codes(hsize, 0);

    auto slot = [&](std::uint32_t key) {
        std::size_t h = (key * 2654435761u) % hsize;
        while (keys[h] != 0xFFFFFFFFu && keys[h] != key) h = h + 1 == hsize ? 0 : h + 1;
        return h;
    };

    emit(clear);

    if (!idx.empty()) {
        unsigned cur = idx[0];

        for (std::size_t i = 1; i < idx.size(); ++i) {
            const unsigned k = idx[i];
            const std::uint32_t key = (static_cast<std::uint32_t>(cur) << 8) | k;
            const std::size_t h = slot(key);

            if (keys[h] == key) {
                cur = codes[h];
                continue;
            }

            emit(cur);

            if (next < 4096) {
                keys[h] = key;
                codes[h] = static_cast<std::uint16_t>(next);
                if (next == (1u << size) && size < 12) ++size;
                ++next;
            } else {
                emit(clear);
                std::fill(keys.begin(), keys.end(), 0xFFFFFFFFu);
                size = min_size + 1;
                next = eoi + 1;
            }

            cur = k;
        }

        emit(cur);
    }

    emit(eoi);
    if (nbits) bytes.push_back(static_cast<std::uint8_t>(acc));
    out.push_back(static_cast<std::uint8_t>(min_size));

    for (std::size_t i = 0; i < bytes.size(); i += 255) {
        const std::size_t n = std::min<std::size_t>(255, bytes.size() - i);
        out.push_back(static_cast<std::uint8_t>(n));
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(i), bytes.begin() + static_cast<std::ptrdiff_t>(i + n));
    }

    out.push_back(0);
}

struct Box {
    std::size_t lo, hi;
    std::uint64_t weight;
    int axis;
    int range;
};

struct HistColor {
    std::uint8_t c[3];
    std::uint32_t count;
};

std::vector<std::array<std::uint8_t, 3>> median_cut(std::vector<HistColor>& colors, std::size_t max_colors) {
    std::vector<std::array<std::uint8_t, 3>> palette;
    if (colors.empty()) return palette;

    auto measure = [&](Box& b) {
        int lo[3] = { 255, 255, 255 }, hi[3] = { 0, 0, 0 };
        b.weight = 0;
        for (std::size_t i = b.lo; i < b.hi; ++i) {
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min<int>(lo[k], colors[i].c[k]);
                hi[k] = std::max<int>(hi[k], colors[i].c[k]);
            }
            b.weight += colors[i].count;
        }
        static const int lum[3] = { 3, 4, 2 };
        b.axis = 0;
        b.range = -1;
        for (int k = 0; k < 3; ++k) {
            const int r = (hi[k] - lo[k]) * lum[k];
            if (r > b.range) { b.range = r; b.axis = k; }
        }
    };

    std::vector<Box> boxes;
    Box first{ 0, colors.size(), 0, 0, 0 };
    measure(first);
    boxes.push_back(first);

    while (boxes.size() < max_colors) {
        std::size_t best = boxes.size();
        double best_score = 0;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if (boxes[i].hi - boxes[i].lo < 2 || boxes[i].range <= 0) continue;
            const double score = static_cast<double>(boxes[i].range) * std::sqrt(static_cast<double>(boxes[i].weight));
            if (score > best_score) { best_score = score; best = i; }
        }
        if (best == boxes.size()) break;
        Box b = boxes[best];
        const int ax = b.axis;
        std::sort(colors.begin() + static_cast<std::ptrdiff_t>(b.lo), colors.begin() + static_cast<std::ptrdiff_t>(b.hi), [ax](const HistColor& x, const HistColor& y) { return x.c[ax] < y.c[ax]; });
        std::uint64_t half = b.weight / 2, acc = 0;
        std::size_t split = b.lo;
        while (split < b.hi - 1 && acc + colors[split].count <= half) acc += colors[split++].count;
        if (split == b.lo) split = b.lo + 1;
        Box l{ b.lo, split, 0, 0, 0 }, r{ split, b.hi, 0, 0, 0 };
        measure(l);
        measure(r);
        boxes[best] = l;
        boxes.push_back(r);
    }

    for (const Box& b : boxes) {
        std::uint64_t s[3] = { 0, 0, 0 }, w = 0;
        for (std::size_t i = b.lo; i < b.hi; ++i) {
            for (int k = 0; k < 3; ++k) s[k] += static_cast<std::uint64_t>(colors[i].c[k]) * colors[i].count;
            w += colors[i].count;
        }
        if (!w) continue;
        palette.push_back({ static_cast<std::uint8_t>((s[0] + w / 2) / w), static_cast<std::uint8_t>((s[1] + w / 2) / w), static_cast<std::uint8_t>((s[2] + w / 2) / w) });
    }

    return palette;
}

struct Quantized {
    std::vector<std::array<std::uint8_t, 3>> palette;
    std::vector<std::uint8_t>                indices;
    int                                      transparent = -1;
};

Quantized quantize(const fmt::Rgba& img, unsigned x0, unsigned y0, unsigned w, unsigned h, bool dither) {
    Quantized q;
    bool has_transparent = false;
    std::unordered_map<std::uint32_t, std::uint32_t> hist;

    for (unsigned y = y0; y < y0 + h; ++y)
        for (unsigned x = x0; x < x0 + w; ++x) {
            const std::uint8_t* p = img.at(x, y);
            if (p[3] < 128) { has_transparent = true; continue; }
            ++hist[(static_cast<std::uint32_t>(p[0]) << 16) | (p[1] << 8) | p[2]];
        }

    const std::size_t max_colors = has_transparent ? 255 : 256;
    bool exact = hist.size() <= max_colors;
    std::vector<HistColor> colors;
    colors.reserve(hist.size());
    for (const auto& kv : hist) colors.push_back(HistColor{ { static_cast<std::uint8_t>(kv.first >> 16), static_cast<std::uint8_t>(kv.first >> 8), static_cast<std::uint8_t>(kv.first) }, kv.second });

    if (exact) {
        std::sort(colors.begin(), colors.end(), [](const HistColor& a, const HistColor& b) { return a.count > b.count; });
        for (const auto& c : colors) q.palette.push_back({ c.c[0], c.c[1], c.c[2] });
    } else {
        q.palette = median_cut(colors, max_colors);
    }

    if (has_transparent) {
        q.transparent = static_cast<int>(q.palette.size());
        q.palette.push_back({ 0, 0, 0 });
    }

    if (q.palette.empty()) q.palette.push_back({ 0, 0, 0 });
    const std::size_t opaque_count = has_transparent ? q.palette.size() - 1 : q.palette.size();
    std::unordered_map<std::uint32_t, std::uint8_t> cache;

    auto nearest = [&](int r, int g, int b) -> std::uint8_t {
        const std::uint32_t key = (static_cast<std::uint32_t>(r) << 16) | (g << 8) | b;
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        long best = -1;
        std::uint8_t bi = 0;
        for (std::size_t i = 0; i < opaque_count; ++i) {
            const long dr = r - q.palette[i][0], dg = g - q.palette[i][1], db = b - q.palette[i][2];
            const long d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
            if (best < 0 || d < best) { best = d; bi = static_cast<std::uint8_t>(i); if (!d) break; }
        }
        cache.emplace(key, bi);
        return bi;
    };

    q.indices.assign(static_cast<std::size_t>(w) * h, 0);
    const bool do_dither = dither && !exact;
    std::vector<int> err_cur(do_dither ? (w + 2) * 3 : 0, 0), err_next(do_dither ? (w + 2) * 3 : 0, 0);

    for (unsigned y = 0; y < h; ++y) {
        if (do_dither) {
            std::swap(err_cur, err_next);
            std::fill(err_next.begin(), err_next.end(), 0);
        }

        for (unsigned x = 0; x < w; ++x) {
            const std::uint8_t* p = img.at(x0 + x, y0 + y);
            std::uint8_t& out = q.indices[static_cast<std::size_t>(y) * w + x];

            if (p[3] < 128) {
                out = static_cast<std::uint8_t>(q.transparent);
                continue;
            }

            if (!do_dither) {
                out = nearest(p[0], p[1], p[2]);
                continue;
            }

            int c[3];
            for (int k = 0; k < 3; ++k) {
                const int v = p[k] + (err_cur[(x + 1) * 3 + k] + 8) / 16;
                c[k] = v < 0 ? 0 : v > 255 ? 255 : v;
            }

            out = nearest(c[0], c[1], c[2]);

            for (int k = 0; k < 3; ++k) {
                const int e = c[k] - q.palette[out][k];
                err_cur[(x + 2) * 3 + k] += e * 7;
                err_next[x * 3 + k] += e * 3;
                err_next[(x + 1) * 3 + k] += e * 5;
                err_next[(x + 2) * 3 + k] += e;
            }
        }
    }

    return q;
}

} // namespace

bool is_gif(const std::uint8_t* data, std::size_t size) noexcept { return size >= 13 && (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0); }

bool decode_gif(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error) {
    if (!is_gif(data, size)) return fmt::fail(error, "not a GIF file");
    fmt::Reader r(data, size);
    r.skip(6);
    const unsigned W = r.le16(), H = r.le16();
    const unsigned packed = r.u8();
    r.u8();
    r.u8();
    if (!W || !H) return fmt::fail(error, "bad GIF dimensions");
    std::vector<std::array<std::uint8_t, 3>> gct;

    auto read_table = [&](unsigned n, std::vector<std::array<std::uint8_t, 3>>& t) {
        t.resize(n);
        for (unsigned i = 0; i < n; ++i) { t[i][0] = r.u8(); t[i][1] = r.u8(); t[i][2] = r.u8(); }
        return r.ok();
    };

    if ((packed & 0x80) && !read_table(2u << (packed & 7), gct)) return fmt::fail(error, "truncated GIF palette");
    AnimatedImage anim;
    anim.set_size(W, H);
    anim.set_loop_count(1);
    fmt::Rgba canvas(W, H, 0);
    fmt::Rgba saved;
    unsigned disposal = 0, delay = 0;
    int transparent = -1;
    std::vector<std::uint8_t> lzw, idx;

    auto read_sub_blocks = [&](std::vector<std::uint8_t>* into) {
        while (true) {
            const unsigned n = r.u8();
            if (!r.ok()) return false;
            if (n == 0) return true;
            if (!r.need(n)) return false;
            if (into) into->insert(into->end(), r.cur(), r.cur() + n);
            r.skip(n);
        }
    };

    while (r.ok() && r.remaining()) {
        const unsigned tag = r.u8();

        if (tag == 0x3B) break;

        if (tag == 0x21) {
            const unsigned label = r.u8();
            std::vector<std::uint8_t> body;
            if (!read_sub_blocks(&body)) break;

            if (label == 0xF9 && body.size() >= 4) {
                disposal = (body[0] >> 2) & 7;
                delay = (body[1] | (body[2] << 8)) * 10u;
                transparent = (body[0] & 1) ? body[3] : -1;
            } else if (label == 0xFF && body.size() >= 14 && (std::memcmp(body.data(), "NETSCAPE2.0", 11) == 0 || std::memcmp(body.data(), "ANIMEXTS1.0", 11) == 0) && body[11] == 1) {
                const unsigned loops = body[12] | (body[13] << 8);
                anim.set_loop_count(loops == 0 ? 0 : loops + 1);
            }

            continue;
        }

        if (tag != 0x2C) {
            if (anim.empty()) return fmt::fail(error, "corrupt GIF block");
            break;
        }

        const unsigned fx = r.le16(), fy = r.le16(), fw = r.le16(), fh = r.le16();
        const unsigned ip = r.u8();
        std::vector<std::array<std::uint8_t, 3>> lct;
        if ((ip & 0x80) && !read_table(2u << (ip & 7), lct)) return fmt::fail(error, "truncated GIF palette");
        const auto& pal = (ip & 0x80) ? lct : gct;
        const unsigned min_size = r.u8();
        lzw.clear();
        if (!read_sub_blocks(&lzw)) {
            if (anim.empty() && lzw.empty()) return fmt::fail(error, "truncated GIF image");
        }

        const std::size_t count = static_cast<std::size_t>(fw) * fh;
        if (!lzw_decode(lzw, min_size, idx, count) && anim.empty()) return fmt::fail(error, "corrupt GIF LZW data");
        if (disposal == 3) saved = canvas;

        std::vector<unsigned> rows(fh);
        if (ip & 0x40) {
            unsigned k = 0;
            for (unsigned y = 0; y < fh; y += 8) rows[k++] = y;
            for (unsigned y = 4; y < fh; y += 8) rows[k++] = y;
            for (unsigned y = 2; y < fh; y += 4) rows[k++] = y;
            for (unsigned y = 1; y < fh; y += 2) rows[k++] = y;
        } else {
            for (unsigned y = 0; y < fh; ++y) rows[y] = y;
        }

        for (unsigned i = 0; i < fh; ++i) {
            const unsigned cy = fy + rows[i];
            if (cy >= H) continue;
            for (unsigned x = 0; x < fw; ++x) {
                const std::size_t k = static_cast<std::size_t>(i) * fw + x;
                if (k >= idx.size()) break;
                const unsigned cx = fx + x;
                if (cx >= W) continue;
                const unsigned c = idx[k];
                if (static_cast<int>(c) == transparent) continue;
                std::uint8_t* d = canvas.at(cx, cy);
                if (c < pal.size()) { d[0] = pal[c][0]; d[1] = pal[c][1]; d[2] = pal[c][2]; }
                else { d[0] = d[1] = d[2] = 0; }
                d[3] = 255;
            }
        }

        anim.add_frame(canvas.to_bitmap(), delay);

        if (disposal == 2) canvas.clear_rect(fx, fy, fw, fh);
        else if (disposal == 3 && !saved.px.empty()) canvas = saved;

        disposal = 0;
        delay = 0;
        transparent = -1;
    }

    if (anim.empty()) return fmt::fail(error, "GIF has no images");
    out = std::move(anim);
    return true;
}

bool encode_gif(const AnimatedImage& anim, std::vector<std::uint8_t>& out, bool dither, std::string* error) {
    out.clear();
    if (anim.empty()) return fmt::fail(error, "empty animation");
    const unsigned W = anim.width() ? anim.width() : anim.frame(0).image.width();
    const unsigned H = anim.height() ? anim.height() : anim.frame(0).image.height();
    if (!W || !H || W > 65535 || H > 65535) return fmt::fail(error, "bad GIF dimensions");
    out.insert(out.end(), { 'G', 'I', 'F', '8', '9', 'a' });
    fmt::put_le16(out, W);
    fmt::put_le16(out, H);
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);

    if (anim.frame_count() > 1 && anim.loop_count() != 1) {
        out.insert(out.end(), { 0x21, 0xFF, 11, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 3, 1 });
        const std::uint32_t loops = anim.loop_count() == 0 ? 0 : std::min<std::uint32_t>(anim.loop_count() - 1, 65535);
        fmt::put_le16(out, loops);
        out.push_back(0);
    }

    fmt::Rgba prev;
    bool prev_opaque = false;

    for (std::size_t f = 0; f < anim.frame_count(); ++f) {
        const AnimatedFrame& frame = anim.frame(f);
        fmt::Rgba cur(W, H, 0);
        const fmt::Rgba src = fmt::Rgba::from(frame.image);
        for (unsigned y = 0; y < std::min(H, src.height); ++y)
            std::memcpy(cur.at(0, y), src.at(0, y), static_cast<std::size_t>(std::min(W, src.width)) * 4);

        bool opaque = true;
        for (std::size_t i = 3; i < cur.px.size() && opaque; i += 4) opaque = cur.px[i] >= 128;
        unsigned x0 = 0, y0 = 0, x1 = W, y1 = H;

        if (f > 0 && opaque && prev_opaque) {
            x0 = W; y0 = H; x1 = 0; y1 = 0;
            for (unsigned y = 0; y < H; ++y)
                for (unsigned x = 0; x < W; ++x)
                    if (std::memcmp(cur.at(x, y), prev.at(x, y), 3) != 0) {
                        x0 = std::min(x0, x); y0 = std::min(y0, y);
                        x1 = std::max(x1, x + 1); y1 = std::max(y1, y + 1);
                    }
            if (x0 >= x1) { x0 = 0; y0 = 0; x1 = 1; y1 = 1; }
        }

        const unsigned fw = x1 - x0, fh = y1 - y0;
        const Quantized q = quantize(cur, x0, y0, fw, fh, dither);
        const unsigned disposal = opaque ? 1 : 2;
        const unsigned delay = (frame.delay_ms + 5) / 10;
        out.insert(out.end(), { 0x21, 0xF9, 4 });
        out.push_back(static_cast<std::uint8_t>((disposal << 2) | (q.transparent >= 0 ? 1 : 0)));
        fmt::put_le16(out, std::min<unsigned>(delay, 65535));
        out.push_back(static_cast<std::uint8_t>(q.transparent >= 0 ? q.transparent : 0));
        out.push_back(0);
        out.push_back(0x2C);
        fmt::put_le16(out, x0);
        fmt::put_le16(out, y0);
        fmt::put_le16(out, fw);
        fmt::put_le16(out, fh);
        unsigned bits = 1;
        while ((1u << bits) < q.palette.size()) ++bits;
        out.push_back(static_cast<std::uint8_t>(0x80 | (bits - 1)));
        for (unsigned i = 0; i < (1u << bits); ++i) {
            if (i < q.palette.size()) out.insert(out.end(), q.palette[i].begin(), q.palette[i].end());
            else out.insert(out.end(), { 0, 0, 0 });
        }
        lzw_encode(q.indices, std::max(2u, bits), out);

        if (opaque) prev = std::move(cur);
        prev_opaque = opaque;
    }

    out.push_back(0x3B);
    return true;
}

} // namespace images
} // namespace fizmo
