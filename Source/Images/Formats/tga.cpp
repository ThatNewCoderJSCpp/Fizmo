#include "fizmo_library.hpp"
#include "format_util.hpp"

namespace fizmo {
namespace images {

namespace {

void tga_color(const std::uint8_t* p, unsigned bits, bool gray, bool use_alpha16, std::uint8_t* d) {
    switch (bits) {
        case 8:
            d[0] = d[1] = d[2] = p[0];
            d[3] = 255;
            break;
        case 15:
        case 16: {
            const unsigned v = p[0] | (p[1] << 8);
            if (gray) {
                d[0] = d[1] = d[2] = p[0];
                d[3] = p[1];
                break;
            }
            d[0] = fmt::scale_to8((v >> 10) & 31, 31);
            d[1] = fmt::scale_to8((v >> 5) & 31, 31);
            d[2] = fmt::scale_to8(v & 31, 31);
            d[3] = (bits == 16 && use_alpha16) ? ((v & 0x8000) ? 255 : 0) : 255;
            break;
        }
        case 24:
            d[0] = p[2];
            d[1] = p[1];
            d[2] = p[0];
            d[3] = 255;
            break;
        default:
            d[0] = p[2];
            d[1] = p[1];
            d[2] = p[0];
            d[3] = p[3];
            break;
    }
}

} // namespace

bool is_tga(const std::uint8_t* data, std::size_t size) noexcept {
    if (size < 18) return false;
    static const char footer[] = "TRUEVISION-XFILE.";
    if (size >= 44 && std::memcmp(data + size - 18, footer, 18) == 0) return true;
    const unsigned cmap = data[1], type = data[2], bits = data[16];
    if (cmap > 1) return false;
    if (type != 1 && type != 2 && type != 3 && type != 9 && type != 10 && type != 11) return false;
    if ((type == 1 || type == 9) != (cmap == 1)) return false;
    if (bits != 8 && bits != 15 && bits != 16 && bits != 24 && bits != 32) return false;
    if (cmap == 1 && data[7] != 15 && data[7] != 16 && data[7] != 24 && data[7] != 32) return false;
    const unsigned w = data[12] | (data[13] << 8), h = data[14] | (data[15] << 8);
    if (!w || !h) return false;
    if (data[17] & 0xC0) return false;
    return true;
}

bool decode_tga(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    if (!is_tga(data, size)) return fmt::fail(error, "not a TGA file");
    fmt::Reader r(data, size);
    const unsigned id_len = r.u8(), cmap_type = r.u8(), type = r.u8();
    const unsigned cmap_first = r.le16(), cmap_len = r.le16(), cmap_bits = r.u8();
    r.le16();
    r.le16();
    const unsigned w = r.le16(), h = r.le16(), bits = r.u8(), desc = r.u8();
    r.skip(id_len);
    const unsigned alpha_bits = desc & 15;
    const bool gray = type == 3 || type == 11;
    const bool rle = type >= 9;
    std::vector<std::array<std::uint8_t, 4>> palette;

    if (cmap_type == 1) {
        const unsigned eb = (cmap_bits + 7) / 8;
        if (!r.need(static_cast<std::size_t>(cmap_len) * eb)) return fmt::fail(error, "truncated TGA palette");
        palette.resize(cmap_first + cmap_len, { 0, 0, 0, 255 });

        for (unsigned i = 0; i < cmap_len; ++i) {
            tga_color(r.cur(), cmap_bits, false, alpha_bits > 0 || cmap_bits == 16, palette[cmap_first + i].data());
            r.skip(eb);
        }
    }

    const unsigned bpp = (bits + 7) / 8;
    const bool indexed = type == 1 || type == 9;
    if (indexed && bits != 8 && bits != 16) return fmt::fail(error, "unsupported TGA index depth");
    if (gray && bits != 8 && bits != 16) return fmt::fail(error, "unsupported TGA gray depth");
    fmt::Rgba img(w, h, 0);
    const std::size_t total = static_cast<std::size_t>(w) * h;
    std::vector<std::uint8_t> raw(total * bpp);

    if (rle) {
        std::size_t n = 0;
        while (n < total) {
            if (!r.need(1)) return fmt::fail(error, "truncated TGA data");
            const unsigned hdr = r.u8();
            const unsigned count = (hdr & 127) + 1;
            if (n + count > total) return fmt::fail(error, "corrupt TGA RLE");

            if (hdr & 128) {
                if (!r.need(bpp)) return fmt::fail(error, "truncated TGA data");
                for (unsigned k = 0; k < count; ++k) std::memcpy(&raw[(n + k) * bpp], r.cur(), bpp);
                r.skip(bpp);
            } else {
                if (!r.need(static_cast<std::size_t>(count) * bpp)) return fmt::fail(error, "truncated TGA data");
                std::memcpy(&raw[n * bpp], r.cur(), static_cast<std::size_t>(count) * bpp);
                r.skip(static_cast<std::size_t>(count) * bpp);
            }

            n += count;
        }
    } else {
        if (!r.need(raw.size())) return fmt::fail(error, "truncated TGA data");
        std::memcpy(raw.data(), r.cur(), raw.size());
    }

    const bool right_to_left = desc & 0x10;
    const bool top = desc & 0x20;
    const bool use_alpha16 = alpha_bits > 0;

    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            const std::uint8_t* p = &raw[(static_cast<std::size_t>(y) * w + x) * bpp];
            std::uint8_t* d = img.at(right_to_left ? w - 1 - x : x, top ? y : h - 1 - y);

            if (indexed) {
                const unsigned i = bits == 8 ? p[0] : (p[0] | (p[1] << 8));
                if (i < palette.size()) std::memcpy(d, palette[i].data(), 4);
                else d[3] = 255;
            } else {
                tga_color(p, bits, gray, use_alpha16, d);
                if (bits == 32 && alpha_bits == 0) d[3] = 255;
            }
        }

    out = img.to_bitmap();
    return true;
}

bool encode_tga(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha, bool rle) {
    out.clear();
    if (!img.is_valid_image() || img.width() > 65535 || img.height() > 65535) return false;
    const unsigned w = img.width(), h = img.height(), bpp = alpha ? 4 : 3;
    out.push_back(0);
    out.push_back(0);
    out.push_back(rle ? 10 : 2);
    for (int i = 0; i < 9; ++i) out.push_back(0);
    fmt::put_le16(out, w);
    fmt::put_le16(out, h);
    out.push_back(static_cast<std::uint8_t>(bpp * 8));
    out.push_back(static_cast<std::uint8_t>(0x20 | (alpha ? 8 : 0)));
    const auto& px = img.pixels();

    auto pixel = [&](std::size_t i, std::uint8_t* p) {
        p[0] = px[i].blue();
        p[1] = px[i].green();
        p[2] = px[i].red();
        p[3] = px[i].alpha();
    };

    for (unsigned y = 0; y < h; ++y) {
        const std::size_t base = static_cast<std::size_t>(y) * w;

        if (!rle) {
            for (unsigned x = 0; x < w; ++x) {
                std::uint8_t p[4];
                pixel(base + x, p);
                out.insert(out.end(), p, p + bpp);
            }
            continue;
        }

        unsigned x = 0;
        while (x < w) {
            std::uint8_t a[4], b[4];
            pixel(base + x, a);
            unsigned run = 1;

            while (x + run < w && run < 128) {
                pixel(base + x + run, b);
                if (std::memcmp(a, b, bpp) != 0) break;
                ++run;
            }

            if (run > 1) {
                out.push_back(static_cast<std::uint8_t>(0x80 | (run - 1)));
                out.insert(out.end(), a, a + bpp);
                x += run;
                continue;
            }

            unsigned lit = 1;
            while (x + lit < w && lit < 128) {
                pixel(base + x + lit, a);
                if (x + lit + 1 < w) {
                    pixel(base + x + lit + 1, b);
                    if (std::memcmp(a, b, bpp) == 0) break;
                }
                ++lit;
            }

            out.push_back(static_cast<std::uint8_t>(lit - 1));
            for (unsigned k = 0; k < lit; ++k) {
                pixel(base + x + k, a);
                out.insert(out.end(), a, a + bpp);
            }
            x += lit;
        }
    }

    for (int i = 0; i < 8; ++i) out.push_back(0);
    static const char footer[] = "TRUEVISION-XFILE.";
    out.insert(out.end(), footer, footer + 18);
    return true;
}

} // namespace images
} // namespace fizmo
