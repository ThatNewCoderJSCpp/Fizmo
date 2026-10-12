#include "fizmo_library.hpp"
#include "format_util.hpp"
#include "../Normal/png_codec.hpp"

namespace fizmo {
namespace images {

namespace {

struct PngChunk {
    const std::uint8_t* type;
    const std::uint8_t* body;
    std::uint32_t       len;
};

bool png_chunks(const std::uint8_t* data, std::size_t size, std::vector<PngChunk>& out) {
    if (!is_png(data, size)) return false;
    std::size_t pos = 8;

    while (pos + 12 <= size) {
        fmt::Reader r(data, size);
        r.seek(pos);
        const std::uint32_t len = r.be32();
        if (len > size - pos - 12) break;
        out.push_back(PngChunk{ data + pos + 4, data + pos + 8, len });
        if (std::memcmp(data + pos + 4, "IEND", 4) == 0) break;
        pos += 12 + static_cast<std::size_t>(len);
    }

    return !out.empty();
}

std::uint32_t be32_at(const std::uint8_t* p) { return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (p[2] << 8) | p[3]; }
std::uint16_t be16_at(const std::uint8_t* p) { return static_cast<std::uint16_t>((p[0] << 8) | p[1]); }

void put_chunk(std::vector<std::uint8_t>& out, const char* type, const std::uint8_t* body, std::size_t len) {
    fmt::put_be32(out, static_cast<std::uint32_t>(len));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    if (len) out.insert(out.end(), body, body + len);
    fmt::put_be32(out, fmt::crc32(out.data() + start, len + 4));
}

struct FrameControl {
    std::uint32_t w = 0, h = 0, x = 0, y = 0;
    std::uint32_t delay_ms = 0;
    unsigned      dispose = 0, blend = 0;
};

} // namespace

bool is_apng(const std::uint8_t* data, std::size_t size) noexcept {
    if (!is_png(data, size)) return false;
    std::size_t pos = 8;

    while (pos + 12 <= size) {
        const std::uint32_t len = be32_at(data + pos);
        if (len > size - pos - 12) return false;
        const std::uint8_t* t = data + pos + 4;
        if (std::memcmp(t, "acTL", 4) == 0) return true;
        if (std::memcmp(t, "IDAT", 4) == 0 || std::memcmp(t, "IEND", 4) == 0) return false;
        pos += 12 + static_cast<std::size_t>(len);
    }

    return false;
}

bool decode_apng(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error) {
    std::vector<PngChunk> chunks;
    if (!png_chunks(data, size, chunks)) return fmt::fail(error, "not a PNG file");
    std::vector<std::uint8_t> ihdr, header_extra;
    bool have_actl = false;
    std::uint32_t plays = 0;

    for (const auto& c : chunks) {
        if (std::memcmp(c.type, "IHDR", 4) == 0 && c.len >= 13) ihdr.assign(c.body, c.body + 13);
        else if (std::memcmp(c.type, "acTL", 4) == 0 && c.len >= 8) { have_actl = true; plays = be32_at(c.body + 4); }
        else if (std::memcmp(c.type, "PLTE", 4) == 0 || std::memcmp(c.type, "tRNS", 4) == 0 || std::memcmp(c.type, "gAMA", 4) == 0 || std::memcmp(c.type, "sBIT", 4) == 0) {
            fmt::put_be32(header_extra, c.len);
            const std::size_t start = header_extra.size();
            header_extra.insert(header_extra.end(), c.type, c.type + 4);
            header_extra.insert(header_extra.end(), c.body, c.body + c.len);
            fmt::put_be32(header_extra, fmt::crc32(header_extra.data() + start, c.len + 4));
        }
    }

    if (ihdr.empty()) return fmt::fail(error, "missing IHDR");

    if (!have_actl) {
        BitmapImage img;
        if (!decode_png(data, size, img, error)) return false;
        out = AnimatedImage(std::move(img));
        return true;
    }

    const std::uint32_t W = be32_at(ihdr.data()), H = be32_at(ihdr.data() + 4);
    if (!fmt::sane_dimensions(W, H)) return fmt::fail(error, "bad APNG dimensions");
    AnimatedImage anim;
    anim.set_size(W, H);
    anim.set_loop_count(plays);
    fmt::Rgba canvas(W, H, 0);
    FrameControl fc;
    bool have_fc = false, seen_idat = false;
    std::vector<std::uint8_t> frame_data;

    auto flush = [&]() -> bool {
        if (!have_fc || frame_data.empty()) return true;
        if (fc.w == 0 || fc.h == 0 || fc.x + static_cast<std::uint64_t>(fc.w) > W || fc.y + static_cast<std::uint64_t>(fc.h) > H) return fmt::fail(error, "bad APNG frame region");
        std::vector<std::uint8_t> png = { 137, 80, 78, 71, 13, 10, 26, 10 };
        std::vector<std::uint8_t> hdr = ihdr;
        hdr[0] = static_cast<std::uint8_t>(fc.w >> 24); hdr[1] = static_cast<std::uint8_t>(fc.w >> 16); hdr[2] = static_cast<std::uint8_t>(fc.w >> 8); hdr[3] = static_cast<std::uint8_t>(fc.w);
        hdr[4] = static_cast<std::uint8_t>(fc.h >> 24); hdr[5] = static_cast<std::uint8_t>(fc.h >> 16); hdr[6] = static_cast<std::uint8_t>(fc.h >> 8); hdr[7] = static_cast<std::uint8_t>(fc.h);
        put_chunk(png, "IHDR", hdr.data(), 13);
        png.insert(png.end(), header_extra.begin(), header_extra.end());
        put_chunk(png, "IDAT", frame_data.data(), frame_data.size());
        put_chunk(png, "IEND", nullptr, 0);
        BitmapImage sub;
        if (!decode_png(png.data(), png.size(), sub, error)) return false;
        const fmt::Rgba s = fmt::Rgba::from(sub);
        const bool first = anim.empty();
        unsigned dispose = fc.dispose;
        if (first && dispose == 2) dispose = 1;
        fmt::Rgba saved;
        if (dispose == 2) saved = canvas;

        for (unsigned y = 0; y < fc.h; ++y)
            for (unsigned x = 0; x < fc.w; ++x) {
                const std::uint8_t* p = s.at(x, y);
                if (fc.blend == 1) canvas.blend_over(fc.x + x, fc.y + y, p);
                else std::memcpy(canvas.at(fc.x + x, fc.y + y), p, 4);
            }

        anim.add_frame(canvas.to_bitmap(), fc.delay_ms);
        if (dispose == 1) canvas.clear_rect(fc.x, fc.y, fc.w, fc.h);
        else if (dispose == 2) canvas = std::move(saved);
        frame_data.clear();
        have_fc = false;
        return true;
    };

    for (const auto& c : chunks) {
        if (std::memcmp(c.type, "fcTL", 4) == 0 && c.len >= 26) {
            if (!flush()) return false;
            fc.w = be32_at(c.body + 4);
            fc.h = be32_at(c.body + 8);
            fc.x = be32_at(c.body + 12);
            fc.y = be32_at(c.body + 16);
            const std::uint32_t num = be16_at(c.body + 20);
            std::uint32_t den = be16_at(c.body + 22);
            if (den == 0) den = 100;
            fc.delay_ms = (num * 2000u + den) / (2u * den);
            fc.dispose = c.body[24];
            fc.blend = c.body[25];
            have_fc = true;
        } else if (std::memcmp(c.type, "IDAT", 4) == 0) {
            seen_idat = true;
            if (have_fc) frame_data.insert(frame_data.end(), c.body, c.body + c.len);
        } else if (std::memcmp(c.type, "fdAT", 4) == 0 && c.len >= 4) {
            if (have_fc) frame_data.insert(frame_data.end(), c.body + 4, c.body + c.len);
        }
    }

    (void)seen_idat;
    if (!flush()) return false;

    if (anim.empty()) {
        BitmapImage img;
        if (!decode_png(data, size, img, error)) return false;
        out = AnimatedImage(std::move(img));
        return true;
    }

    out = std::move(anim);
    return true;
}

bool encode_apng(const AnimatedImage& anim, std::vector<std::uint8_t>& out, std::string* error) {
    out.clear();
    if (anim.empty()) return fmt::fail(error, "empty animation");
    const unsigned W = anim.width() ? anim.width() : anim.frame(0).image.width();
    const unsigned H = anim.height() ? anim.height() : anim.frame(0).image.height();
    if (!W || !H) return fmt::fail(error, "bad APNG dimensions");
    out = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::uint8_t ihdr[13] = {};
    for (int i = 0; i < 4; ++i) { ihdr[i] = static_cast<std::uint8_t>(W >> (24 - 8 * i)); ihdr[4 + i] = static_cast<std::uint8_t>(H >> (24 - 8 * i)); }
    ihdr[8] = 8;
    ihdr[9] = 6;
    put_chunk(out, "IHDR", ihdr, 13);
    std::vector<std::uint8_t> actl;
    fmt::put_be32(actl, static_cast<std::uint32_t>(anim.frame_count()));
    fmt::put_be32(actl, anim.loop_count());
    put_chunk(out, "acTL", actl.data(), actl.size());
    std::uint32_t seq = 0;
    fmt::Rgba prev;

    for (std::size_t f = 0; f < anim.frame_count(); ++f) {
        fmt::Rgba cur(W, H, 0);
        const fmt::Rgba src = fmt::Rgba::from(anim.frame(f).image);
        for (unsigned y = 0; y < std::min(H, src.height); ++y)
            std::memcpy(cur.at(0, y), src.at(0, y), static_cast<std::size_t>(std::min(W, src.width)) * 4);

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
        }

        const unsigned fw = x1 - x0, fh = y1 - y0;
        fmt::Rgba sub(fw, fh);
        for (unsigned y = 0; y < fh; ++y) std::memcpy(sub.at(0, y), cur.at(x0, y0 + y), static_cast<std::size_t>(fw) * 4);
        std::vector<std::uint8_t> png;
        if (!encode_png(sub.to_bitmap(), png, true, 9)) return fmt::fail(error, "PNG encode failed");
        std::vector<PngChunk> chunks;
        png_chunks(png.data(), png.size(), chunks);

        std::vector<std::uint8_t> fctl;
        fmt::put_be32(fctl, seq++);
        fmt::put_be32(fctl, fw);
        fmt::put_be32(fctl, fh);
        fmt::put_be32(fctl, x0);
        fmt::put_be32(fctl, y0);
        std::uint32_t num = anim.frame(f).delay_ms, den = 1000;
        while (num > 65535 && den > 1) { num = (num + 5) / 10; den /= 10; }
        num = std::min<std::uint32_t>(num, 65535);
        fmt::put_be16(fctl, num);
        fmt::put_be16(fctl, den);
        fctl.push_back(0);
        fctl.push_back(0);
        put_chunk(out, "fcTL", fctl.data(), fctl.size());

        for (const auto& c : chunks) {
            if (std::memcmp(c.type, "IDAT", 4) != 0) continue;
            if (f == 0) {
                put_chunk(out, "IDAT", c.body, c.len);
            } else {
                std::vector<std::uint8_t> fd;
                fmt::put_be32(fd, seq++);
                fd.insert(fd.end(), c.body, c.body + c.len);
                put_chunk(out, "fdAT", fd.data(), fd.size());
            }
        }

        prev = std::move(cur);
    }

    put_chunk(out, "IEND", nullptr, 0);
    return true;
}

} // namespace images
} // namespace fizmo
