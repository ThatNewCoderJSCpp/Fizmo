#include "fizmo_library.hpp"
#include "webp_internal.hpp"

namespace fizmo {
namespace images {

namespace {

struct Chunk {
    char                fourcc[5];
    const std::uint8_t* data;
    std::size_t         size;
};

bool read_chunks(const std::uint8_t* p, std::size_t n, std::vector<Chunk>& out) {
    std::size_t pos = 0;

    while (pos + 8 <= n) {
        Chunk c;
        std::memcpy(c.fourcc, p + pos, 4);
        c.fourcc[4] = 0;
        const std::uint32_t len = p[pos + 4] | (p[pos + 5] << 8) | (static_cast<std::uint32_t>(p[pos + 6]) << 16) | (static_cast<std::uint32_t>(p[pos + 7]) << 24);
        if (len > n - pos - 8) {
            c.data = p + pos + 8;
            c.size = n - pos - 8;
            out.push_back(c);
            return true;
        }
        c.data = p + pos + 8;
        c.size = len;
        out.push_back(c);
        pos += 8 + static_cast<std::size_t>(len) + (len & 1u);
    }

    return true;
}

bool is(const Chunk& c, const char* id) { return std::memcmp(c.fourcc, id, 4) == 0; }

bool decode_alpha(const std::uint8_t* data, std::size_t size, unsigned w, unsigned h, std::vector<std::uint8_t>& alpha, std::string* error) {
    if (size < 1) return fmt::fail(error, "empty ALPH chunk");
    const unsigned method = data[0] & 3, filter = (data[0] >> 2) & 3;
    if (method > 1 || (data[0] >> 6) != 0) return fmt::fail(error, "bad ALPH header");
    const std::size_t total = static_cast<std::size_t>(w) * h;
    std::vector<std::uint8_t> deltas(total);

    if (method == 0) {
        if (size - 1 < total) return fmt::fail(error, "truncated ALPH data");
        std::memcpy(deltas.data(), data + 1, total);
    } else {
        std::vector<std::uint32_t> argb;
        if (!webp_detail::vp8l_decode_stream(data + 1, size - 1, w, h, argb, error)) return false;
        for (std::size_t i = 0; i < total; ++i) deltas[i] = static_cast<std::uint8_t>((argb[i] >> 8) & 255);
    }

    alpha.assign(total, 0);

    for (unsigned y = 0; y < h; ++y) {
        const std::uint8_t* in = deltas.data() + static_cast<std::size_t>(y) * w;
        std::uint8_t* out = alpha.data() + static_cast<std::size_t>(y) * w;
        const std::uint8_t* prev = y ? out - w : nullptr;

        if (filter == 0) {
            std::memcpy(out, in, w);
        } else if (filter == 1 || !prev) {
            std::uint8_t pred = prev ? prev[0] : 0;
            for (unsigned i = 0; i < w; ++i) {
                out[i] = static_cast<std::uint8_t>(pred + in[i]);
                pred = out[i];
            }
        } else if (filter == 2) {
            for (unsigned i = 0; i < w; ++i) out[i] = static_cast<std::uint8_t>(prev[i] + in[i]);
        } else {
            int top_left = prev[0], left = prev[0];
            for (unsigned i = 0; i < w; ++i) {
                const int top = prev[i];
                int g = left + top - top_left;
                g = g < 0 ? 0 : g > 255 ? 255 : g;
                left = static_cast<std::uint8_t>(in[i] + g);
                top_left = top;
                out[i] = static_cast<std::uint8_t>(left);
            }
        }
    }

    return true;
}

bool decode_frame(const std::vector<Chunk>& chunks, std::size_t begin, fmt::Rgba& out, std::string* error) {
    const Chunk* alph = nullptr;

    for (std::size_t i = begin; i < chunks.size(); ++i) {
        const Chunk& c = chunks[i];

        if (is(c, "ALPH")) {
            alph = &c;
        } else if (is(c, "VP8L")) {
            return webp_detail::vp8l_decode(c.data, c.size, out, error);
        } else if (is(c, "VP8 ")) {
            if (!webp_detail::vp8_decode(c.data, c.size, out, error)) return false;
            if (alph) {
                std::vector<std::uint8_t> a;
                if (!decode_alpha(alph->data, alph->size, out.width, out.height, a, error)) return false;
                for (std::size_t k = 0; k < a.size(); ++k) out.px[k * 4 + 3] = a[k];
            }
            return true;
        }
    }

    return fmt::fail(error, "WebP has no image data");
}

void put_chunk(std::vector<std::uint8_t>& out, const char* id, const std::vector<std::uint8_t>& payload) {
    out.insert(out.end(), id, id + 4);
    fmt::put_le32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    if (payload.size() & 1) out.push_back(0);
}

void finish_riff(std::vector<std::uint8_t>& out) { fmt::set_le32(out, 4, static_cast<std::uint32_t>(out.size() - 8)); }

std::vector<std::uint8_t> riff_start() { return { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P' }; }

void vp8x_payload(std::vector<std::uint8_t>& p, std::uint8_t flags, unsigned w, unsigned h) {
    p.clear();
    p.push_back(flags);
    p.push_back(0);
    p.push_back(0);
    p.push_back(0);
    fmt::put_le24(p, w - 1);
    fmt::put_le24(p, h - 1);
}

bool encode_alpha(const fmt::Rgba& img, int effort, std::vector<std::uint8_t>& out) {
    const unsigned w = img.width, h = img.height;
    std::vector<std::uint8_t> a(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = img.px[i * 4 + 3];
    std::vector<std::uint8_t> best;

    for (unsigned filter = 0; filter < 4; ++filter) {
        std::vector<std::uint32_t> argb(a.size());

        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * w + x;
                int pred = 0;
                if (filter == 0) pred = 0;
                else if (filter == 1 || y == 0) pred = x ? a[i - 1] : (y ? a[i - w] : 0);
                else if (filter == 2) pred = a[i - w];
                else {
                    if (x == 0) pred = a[i - w];
                    else {
                        const int g = a[i - 1] + a[i - w] - a[i - w - 1];
                        pred = g < 0 ? 0 : g > 255 ? 255 : g;
                    }
                }
                argb[i] = 0xFF000000u | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a[i] - pred)) << 8);
            }

        std::vector<std::uint8_t> stream;
        if (!webp_detail::vp8l_encode_stream(argb, w, h, stream, effort)) return false;
        if (best.empty() || stream.size() + 1 < best.size()) {
            best.assign(1, static_cast<std::uint8_t>(1 | (filter << 2)));
            best.insert(best.end(), stream.begin(), stream.end());
        }
        if (effort < 4) break;
    }

    out = std::move(best);
    return true;
}

bool encode_frame(const fmt::Rgba& img, const EncodeOptions& options, std::vector<std::uint8_t>& out, bool& has_alpha) {
    out.clear();
    has_alpha = img.has_alpha();
    const int effort = std::max(0, std::min(9, options.compression));

    if (options.lossless) {
        std::vector<std::uint8_t> vp8l;
        if (!webp_detail::vp8l_encode(img, vp8l, effort)) return false;
        put_chunk(out, "VP8L", vp8l);
        return true;
    }

    if (has_alpha && options.alpha) {
        std::vector<std::uint8_t> alph;
        if (!encode_alpha(img, effort, alph)) return false;
        put_chunk(out, "ALPH", alph);
    } else {
        has_alpha = false;
    }

    std::vector<std::uint8_t> vp8;
    if (!webp_detail::vp8_encode(img, options.quality, vp8)) return false;
    put_chunk(out, "VP8 ", vp8);
    return true;
}

} // namespace

bool is_webp(const std::uint8_t* data, std::size_t size) noexcept { return size >= 16 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0; }

bool decode_webp(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error) {
    if (!is_webp(data, size)) return fmt::fail(error, "not a WebP file");
    std::uint32_t riff = data[4] | (data[5] << 8) | (static_cast<std::uint32_t>(data[6]) << 16) | (static_cast<std::uint32_t>(data[7]) << 24);
    std::size_t n = std::min<std::size_t>(size, static_cast<std::size_t>(riff) + 8);
    if (n < 12) return fmt::fail(error, "truncated WebP");
    std::vector<Chunk> chunks;
    read_chunks(data + 12, n - 12, chunks);
    if (chunks.empty()) return fmt::fail(error, "empty WebP");

    if (!is(chunks[0], "VP8X")) {
        fmt::Rgba img;
        if (!decode_frame(chunks, 0, img, error)) return false;
        out = AnimatedImage(img.to_bitmap());
        return true;
    }

    if (chunks[0].size < 10) return fmt::fail(error, "bad VP8X chunk");
    const std::uint8_t flags = chunks[0].data[0];
    const unsigned cw = (chunks[0].data[4] | (chunks[0].data[5] << 8) | (static_cast<unsigned>(chunks[0].data[6]) << 16)) + 1;
    const unsigned ch = (chunks[0].data[7] | (chunks[0].data[8] << 8) | (static_cast<unsigned>(chunks[0].data[9]) << 16)) + 1;
    if (!fmt::sane_dimensions(cw, ch)) return fmt::fail(error, "bad WebP canvas size");

    if (!(flags & 0x02)) {
        fmt::Rgba img;
        if (!decode_frame(chunks, 1, img, error)) return false;
        out = AnimatedImage(img.to_bitmap());
        return true;
    }

    AnimatedImage anim;
    anim.set_size(cw, ch);
    fmt::Rgba canvas(cw, ch, 0);
    bool prev_dispose = false;
    unsigned px0 = 0, py0 = 0, pw = 0, ph = 0;

    for (const Chunk& c : chunks) {
        if (is(c, "ANIM") && c.size >= 6) {
            anim.set_loop_count(c.data[4] | (c.data[5] << 8));
            continue;
        }
        if (!is(c, "ANMF") || c.size < 16) continue;
        const std::uint8_t* f = c.data;
        const unsigned fx = 2 * (f[0] | (f[1] << 8) | (static_cast<unsigned>(f[2]) << 16));
        const unsigned fy = 2 * (f[3] | (f[4] << 8) | (static_cast<unsigned>(f[5]) << 16));
        const unsigned fw = (f[6] | (f[7] << 8) | (static_cast<unsigned>(f[8]) << 16)) + 1;
        const unsigned fh = (f[9] | (f[10] << 8) | (static_cast<unsigned>(f[11]) << 16)) + 1;
        const unsigned dur = f[12] | (f[13] << 8) | (static_cast<unsigned>(f[14]) << 16);
        const bool no_blend = (f[15] >> 1) & 1;
        const bool dispose = f[15] & 1;
        if (static_cast<std::uint64_t>(fx) + fw > cw || static_cast<std::uint64_t>(fy) + fh > ch) return fmt::fail(error, "WebP frame outside canvas");

        std::vector<Chunk> sub;
        read_chunks(f + 16, c.size - 16, sub);
        fmt::Rgba frame;
        if (!decode_frame(sub, 0, frame, error)) return false;
        if (frame.width != fw || frame.height != fh) return fmt::fail(error, "WebP frame size mismatch");

        if (prev_dispose) canvas.clear_rect(px0, py0, pw, ph);

        for (unsigned y = 0; y < fh; ++y)
            for (unsigned x = 0; x < fw; ++x) {
                const std::uint8_t* s = frame.at(x, y);
                if (no_blend) std::memcpy(canvas.at(fx + x, fy + y), s, 4);
                else canvas.blend_over(fx + x, fy + y, s);
            }

        anim.add_frame(canvas.to_bitmap(), dur);
        prev_dispose = dispose;
        px0 = fx; py0 = fy; pw = fw; ph = fh;
    }

    if (anim.empty()) return fmt::fail(error, "animated WebP has no frames");
    out = std::move(anim);
    return true;
}

bool decode_webp(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    AnimatedImage anim;
    if (!decode_webp(data, size, anim, error)) return false;
    out = anim.frame(0).image;
    return true;
}

bool encode_webp(const BitmapImage& img, std::vector<std::uint8_t>& out, const EncodeOptions& options, std::string* error) {
    out.clear();
    if (!img.is_valid_image()) return fmt::fail(error, "empty image");
    if (img.width() > 16384 || img.height() > 16384) return fmt::fail(error, "WebP is limited to 16384x16384");
    fmt::Rgba rgba = fmt::Rgba::from(img);
    if (!options.alpha) for (std::size_t i = 3; i < rgba.px.size(); i += 4) rgba.px[i] = 255;
    std::vector<std::uint8_t> frame;
    bool has_alpha = false;
    if (!encode_frame(rgba, options, frame, has_alpha)) return fmt::fail(error, "WebP encode failed");
    out = riff_start();

    if (!options.lossless && has_alpha) {
        std::vector<std::uint8_t> x;
        vp8x_payload(x, 0x10, img.width(), img.height());
        put_chunk(out, "VP8X", x);
    }

    out.insert(out.end(), frame.begin(), frame.end());
    finish_riff(out);
    return true;
}

bool encode_webp(const AnimatedImage& anim, std::vector<std::uint8_t>& out, const EncodeOptions& options, std::string* error) {
    out.clear();
    if (anim.empty()) return fmt::fail(error, "empty animation");
    if (anim.frame_count() == 1 && anim.width() == anim.frame(0).image.width() && anim.height() == anim.frame(0).image.height()) return encode_webp(anim.frame(0).image, out, options, error);
    const unsigned W = anim.width() ? anim.width() : anim.frame(0).image.width();
    const unsigned H = anim.height() ? anim.height() : anim.frame(0).image.height();
    if (!W || !H || W > 16384 || H > 16384) return fmt::fail(error, "bad WebP canvas size");
    std::vector<std::uint8_t> body;
    bool any_alpha = false;
    fmt::Rgba prev;

    for (std::size_t f = 0; f < anim.frame_count(); ++f) {
        fmt::Rgba cur(W, H, 0);
        const fmt::Rgba src = fmt::Rgba::from(anim.frame(f).image);
        for (unsigned y = 0; y < std::min(H, src.height); ++y)
            std::memcpy(cur.at(0, y), src.at(0, y), static_cast<std::size_t>(std::min(W, src.width)) * 4);
        if (!options.alpha) for (std::size_t i = 3; i < cur.px.size(); i += 4) cur.px[i] = 255;

        unsigned x0 = 0, y0 = 0, x1 = W, y1 = H;
        if (f > 0) {
            x0 = W; y0 = H; x1 = 0; y1 = 0;
            for (unsigned y = 0; y < H; ++y)
                for (unsigned x = 0; x < W; ++x)
                    if (std::memcmp(cur.at(x, y), prev.at(x, y), 4) != 0) {
                        x0 = std::min(x0, x); y0 = std::min(y0, y);
                        x1 = std::max(x1, x + 1); y1 = std::max(y1, y + 1);
                    }
            if (x0 >= x1) { x0 = 0; y0 = 0; x1 = 1; y1 = 1; }
            x0 &= ~1u;
            y0 &= ~1u;
        }

        const unsigned fw = x1 - x0, fh = y1 - y0;
        fmt::Rgba sub(fw, fh);
        for (unsigned y = 0; y < fh; ++y) std::memcpy(sub.at(0, y), cur.at(x0, y0 + y), static_cast<std::size_t>(fw) * 4);
        std::vector<std::uint8_t> frame;
        bool has_alpha = false;
        if (!encode_frame(sub, options, frame, has_alpha)) return fmt::fail(error, "WebP encode failed");
        any_alpha = any_alpha || has_alpha || sub.has_alpha();

        std::vector<std::uint8_t> anmf;
        fmt::put_le24(anmf, x0 / 2);
        fmt::put_le24(anmf, y0 / 2);
        fmt::put_le24(anmf, fw - 1);
        fmt::put_le24(anmf, fh - 1);
        fmt::put_le24(anmf, std::min<std::uint32_t>(anim.frame(f).delay_ms, 0xFFFFFF));
        anmf.push_back(0x02);
        anmf.insert(anmf.end(), frame.begin(), frame.end());
        put_chunk(body, "ANMF", anmf);
        prev = std::move(cur);
    }

    out = riff_start();
    std::vector<std::uint8_t> x;
    vp8x_payload(x, static_cast<std::uint8_t>(0x02 | (any_alpha ? 0x10 : 0)), W, H);
    put_chunk(out, "VP8X", x);
    std::vector<std::uint8_t> animc;
    fmt::put_le32(animc, 0);
    fmt::put_le16(animc, std::min<std::uint32_t>(anim.loop_count(), 65535));
    put_chunk(out, "ANIM", animc);
    out.insert(out.end(), body.begin(), body.end());
    finish_riff(out);
    return true;
}

} // namespace images
} // namespace fizmo
