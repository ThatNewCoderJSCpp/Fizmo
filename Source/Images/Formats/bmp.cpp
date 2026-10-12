#include "fizmo_library.hpp"
#include "format_util.hpp"
#include "../Normal/jpeg_codec.hpp"
#include "../Normal/png_codec.hpp"

namespace fizmo {
namespace images {
namespace fmt {

namespace {

struct Channel {
    std::uint32_t mask = 0;
    unsigned      shift = 0;
    std::uint64_t max = 0;

    explicit Channel(std::uint32_t m = 0) : mask(m) {
        if (!m) return;
        while (!((m >> shift) & 1u)) ++shift;
        max = static_cast<std::uint64_t>(m >> shift);
    }

    std::uint8_t get(std::uint32_t px, std::uint8_t fallback) const noexcept {
        if (!mask) return fallback;
        return scale_to8((px & mask) >> shift, max);
    }
};

bool rle_decode(const std::uint8_t* p, std::size_t n, unsigned w, unsigned h, bool four, std::vector<std::uint8_t>& idx) {
    idx.assign(static_cast<std::size_t>(w) * h, 0);
    std::size_t i = 0;
    unsigned x = 0, y = 0;

    auto put = [&](std::uint8_t v) {
        if (x < w && y < h) idx[static_cast<std::size_t>(y) * w + x] = v;
        ++x;
    };

    while (i + 1 < n && y < h) {
        const std::uint8_t count = p[i], val = p[i + 1];
        i += 2;

        if (count) {
            for (unsigned k = 0; k < count; ++k) put(four ? static_cast<std::uint8_t>((k & 1) ? (val & 15) : (val >> 4)) : val);
            continue;
        }

        if (val == 0) {
            x = 0;
            ++y;
        } else if (val == 1) {
            break;
        } else if (val == 2) {
            if (i + 1 >= n) return false;
            x += p[i];
            y += p[i + 1];
            i += 2;
        } else {
            const unsigned bytes = four ? (val + 1u) / 2u : val;
            if (i + bytes > n) return false;
            for (unsigned k = 0; k < val; ++k) put(four ? static_cast<std::uint8_t>((k & 1) ? (p[i + k / 2] & 15) : (p[i + k / 2] >> 4)) : p[i + k]);
            i += bytes + (bytes & 1u);
        }
    }

    return true;
}

} // namespace

bool decode_dib(const std::uint8_t* file, std::size_t file_size, std::size_t dib_offset, std::size_t pixel_offset, bool icon, Rgba& out, std::string* error) {
    Reader r(file, file_size);
    r.seek(dib_offset);
    const std::uint32_t hsize = r.le32();
    if (!r.ok() || hsize < 12 || dib_offset + hsize > file_size) return fail(error, "bad DIB header");
    std::int64_t w = 0, h = 0;
    unsigned bits = 0;
    std::uint32_t comp = 0, image_size = 0, colors_used = 0;
    std::size_t entry = 4;

    if (hsize == 12) {
        w = r.le16();
        h = static_cast<std::int16_t>(r.le16());
        r.le16();
        bits = r.le16();
        entry = 3;
    } else {
        w = r.le32s();
        h = r.le32s();
        r.le16();
        bits = r.le16();
        if (hsize >= 20) comp = r.le32();
        if (hsize >= 24) image_size = r.le32();
        if (hsize >= 36) {
            r.le32();
            r.le32();
            colors_used = r.le32();
        }
    }

    if (hsize == 64 && (comp == 3 || comp == 4)) return fail(error, "OS/2 Huffman/RLE24 BMP is not supported");
    const bool top_down = h < 0;
    if (top_down) h = -h;
    if (icon) h /= 2;
    if (!sane_dimensions(static_cast<std::uint64_t>(w < 0 ? 0 : w), static_cast<std::uint64_t>(h))) return fail(error, "bad BMP dimensions");
    const unsigned W = static_cast<unsigned>(w), H = static_cast<unsigned>(h);

    std::uint32_t masks[4] = { 0, 0, 0, 0 };
    std::size_t pal_off = dib_offset + hsize;

    if (comp == 3 || comp == 6) {
        const unsigned count = comp == 6 ? 4u : 3u;
        if (hsize >= 52) {
            r.seek(dib_offset + 40);
            for (unsigned k = 0; k < 3; ++k) masks[k] = r.le32();
            if (hsize >= 56) masks[3] = r.le32();
        } else {
            r.seek(dib_offset + hsize);
            for (unsigned k = 0; k < count; ++k) masks[k] = r.le32();
            pal_off += count * 4;
        }
    }

    if (comp == 4 || comp == 5) {
        const std::size_t at = pixel_offset ? pixel_offset : pal_off;
        if (at > file_size) return fail(error, "truncated BMP");
        const std::size_t len = image_size && image_size <= file_size - at ? image_size : file_size - at;
        BitmapImage tmp;
        const bool ok = comp == 4 ? decode_jpeg(file + at, len, tmp, error) : decode_png(file + at, len, tmp, error);
        if (!ok) return false;
        out = Rgba::from(tmp);
        return true;
    }

    std::vector<std::array<std::uint8_t, 4>> palette;

    if (bits <= 8) {
        if (bits != 1 && bits != 2 && bits != 4 && bits != 8) return fail(error, "bad BMP bit depth");
        std::size_t n = colors_used ? colors_used : (1u << bits);
        if (n > 256) n = 256;
        r.seek(pal_off);

        for (std::size_t k = 0; k < n && r.remaining() >= entry; ++k) {
            const std::uint8_t b = r.u8(), g = r.u8(), rr = r.u8();
            if (entry == 4) r.u8();
            palette.push_back({ rr, g, b, 255 });
        }

        pal_off += n * entry;
    } else if (bits != 16 && bits != 24 && bits != 32) {
        return fail(error, "bad BMP bit depth");
    }

    const std::size_t data_off = pixel_offset ? pixel_offset : pal_off;
    if (data_off > file_size) return fail(error, "truncated BMP");
    out = Rgba(W, H, 0);

    auto color_of = [&](unsigned i) -> std::array<std::uint8_t, 4> {
        if (i < palette.size()) return palette[i];
        return { 0, 0, 0, 255 };
    };

    if (comp == 1 || comp == 2) {
        if (top_down) return fail(error, "top-down RLE BMP is invalid");
        std::vector<std::uint8_t> idx;
        if (!rle_decode(file + data_off, file_size - data_off, W, H, comp == 2, idx)) return fail(error, "corrupt BMP RLE data");

        for (unsigned y = 0; y < H; ++y)
            for (unsigned x = 0; x < W; ++x) {
                const auto c = color_of(idx[static_cast<std::size_t>(y) * W + x]);
                std::memcpy(out.at(x, H - 1 - y), c.data(), 4);
            }

        return true;
    }

    if (comp != 0 && comp != 3 && comp != 6) return fail(error, "unsupported BMP compression");
    const std::size_t stride = ((static_cast<std::size_t>(W) * bits + 31) / 32) * 4;
    if (stride * H > file_size - data_off) return fail(error, "truncated BMP");

    if (comp == 0 && bits == 16) {
        masks[0] = 0x7C00;
        masks[1] = 0x03E0;
        masks[2] = 0x001F;
    } else if (comp == 0 && bits == 32) {
        masks[0] = 0x00FF0000;
        masks[1] = 0x0000FF00;
        masks[2] = 0x000000FF;
        masks[3] = 0xFF000000;
    } else if (comp == 0 && bits == 24) {
        masks[0] = 0xFF0000;
        masks[1] = 0x00FF00;
        masks[2] = 0x0000FF;
    }

    const Channel cr(masks[0]), cg(masks[1]), cb(masks[2]), ca(masks[3]);
    bool any_alpha = false;

    for (unsigned y = 0; y < H; ++y) {
        const std::uint8_t* row = file + data_off + stride * (top_down ? y : H - 1 - y);

        for (unsigned x = 0; x < W; ++x) {
            std::uint8_t* d = out.at(x, y);

            if (bits <= 8) {
                const std::size_t bit = static_cast<std::size_t>(x) * bits;
                const unsigned i = (row[bit / 8] >> (8 - bits - bit % 8)) & ((1u << bits) - 1u);
                const auto c = color_of(i);
                std::memcpy(d, c.data(), 4);
                continue;
            }

            std::uint32_t px = 0;
            if (bits == 16) px = row[x * 2] | (row[x * 2 + 1] << 8);
            else if (bits == 24) px = row[x * 3] | (row[x * 3 + 1] << 8) | (static_cast<std::uint32_t>(row[x * 3 + 2]) << 16);
            else px = row[x * 4] | (row[x * 4 + 1] << 8) | (static_cast<std::uint32_t>(row[x * 4 + 2]) << 16) | (static_cast<std::uint32_t>(row[x * 4 + 3]) << 24);
            d[0] = cr.get(px, 0);
            d[1] = cg.get(px, 0);
            d[2] = cb.get(px, 0);
            d[3] = ca.get(px, 255);
            if (ca.mask && (px & ca.mask)) any_alpha = true;
        }
    }

    const bool alpha_is_real = ca.mask && (comp != 0 || any_alpha);
    if (ca.mask && !alpha_is_real)
        for (std::size_t i = 3; i < out.px.size(); i += 4) out.px[i] = 255;

    if (icon && !(bits == 32 && alpha_is_real)) {
        const std::size_t mstride = ((static_cast<std::size_t>(W) + 31) / 32) * 4;
        const std::size_t moff = data_off + stride * H;

        if (moff + mstride * H <= file_size) {
            for (unsigned y = 0; y < H; ++y) {
                const std::uint8_t* row = file + moff + mstride * (top_down ? y : H - 1 - y);
                for (unsigned x = 0; x < W; ++x)
                    if ((row[x / 8] >> (7 - x % 8)) & 1u) out.at(x, y)[3] = 0;
            }
        }
    }

    return true;
}

} // namespace fmt

bool decode_bmp(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    if (size < 26 || data[0] != 'B' || data[1] != 'M') return fmt::fail(error, "not a BMP file");
    fmt::Reader r(data, size);
    r.seek(10);
    const std::uint32_t offset = r.le32();
    fmt::Rgba img;
    if (!fmt::decode_dib(data, size, 14, offset, false, img, error)) return false;
    out = img.to_bitmap();
    return true;
}

bool encode_bmp(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha) {
    out.clear();
    if (!img.is_valid_image()) return false;
    const unsigned w = img.width(), h = img.height();
    const unsigned bpp = alpha ? 4 : 3;
    const std::size_t stride = (static_cast<std::size_t>(w) * bpp + 3) & ~std::size_t(3);
    const std::uint32_t hsize = alpha ? 108 : 40;
    const std::uint32_t offset = 14 + hsize;
    const std::uint64_t total = offset + stride * h;
    if (total > 0xFFFFFFFFull) return false;
    out.reserve(static_cast<std::size_t>(total));
    out.push_back('B');
    out.push_back('M');
    fmt::put_le32(out, static_cast<std::uint32_t>(total));
    fmt::put_le32(out, 0);
    fmt::put_le32(out, offset);
    fmt::put_le32(out, hsize);
    fmt::put_le32(out, w);
    fmt::put_le32(out, h);
    fmt::put_le16(out, 1);
    fmt::put_le16(out, bpp * 8);
    fmt::put_le32(out, alpha ? 3 : 0);
    fmt::put_le32(out, static_cast<std::uint32_t>(stride * h));
    fmt::put_le32(out, 2835);
    fmt::put_le32(out, 2835);
    fmt::put_le32(out, 0);
    fmt::put_le32(out, 0);

    if (alpha) {
        fmt::put_le32(out, 0x00FF0000);
        fmt::put_le32(out, 0x0000FF00);
        fmt::put_le32(out, 0x000000FF);
        fmt::put_le32(out, 0xFF000000);
        fmt::put_le32(out, 0x73524742);
        for (int k = 0; k < 12; ++k) fmt::put_le32(out, 0);
    }

    const auto& px = img.pixels();
    std::vector<std::uint8_t> row(stride, 0);

    for (unsigned y = h; y-- > 0;) {
        for (unsigned x = 0; x < w; ++x) {
            const graphics::Color& c = px[static_cast<std::size_t>(y) * w + x];
            std::uint8_t* p = row.data() + static_cast<std::size_t>(x) * bpp;
            p[0] = c.blue();
            p[1] = c.green();
            p[2] = c.red();
            if (alpha) p[3] = c.alpha();
        }

        out.insert(out.end(), row.begin(), row.end());
    }

    return true;
}

} // namespace images
} // namespace fizmo
