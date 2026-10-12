#include "fizmo_library.hpp"
#include "format_util.hpp"

namespace fizmo {
namespace images {

namespace {

inline unsigned qoi_hash(const std::uint8_t* p) noexcept { return (p[0] * 3u + p[1] * 5u + p[2] * 7u + p[3] * 11u) % 64u; }

} // namespace

bool is_qoi(const std::uint8_t* data, std::size_t size) noexcept { return size >= 14 && std::memcmp(data, "qoif", 4) == 0; }

bool decode_qoi(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    if (!is_qoi(data, size)) return fmt::fail(error, "not a QOI file");
    fmt::Reader r(data, size);
    r.skip(4);
    const std::uint32_t w = r.be32(), h = r.be32();
    const unsigned channels = r.u8();
    r.u8();
    if (channels != 3 && channels != 4) return fmt::fail(error, "bad QOI channel count");
    if (!fmt::sane_dimensions(w, h)) return fmt::fail(error, "bad QOI dimensions");
    fmt::Rgba img(w, h);
    std::uint8_t index[64][4] = {};
    std::uint8_t px[4] = { 0, 0, 0, 255 };
    const std::size_t total = static_cast<std::size_t>(w) * h;
    const std::size_t end = size >= 8 ? size - 8 : 0;
    std::size_t p = 14;
    unsigned run = 0;

    for (std::size_t i = 0; i < total; ++i) {
        if (run) {
            --run;
        } else if (p < end) {
            const std::uint8_t b = data[p++];

            if (b == 0xFE) {
                if (p + 3 > size) return fmt::fail(error, "truncated QOI");
                px[0] = data[p];
                px[1] = data[p + 1];
                px[2] = data[p + 2];
                p += 3;
            } else if (b == 0xFF) {
                if (p + 4 > size) return fmt::fail(error, "truncated QOI");
                std::memcpy(px, data + p, 4);
                p += 4;
            } else if ((b & 0xC0) == 0x00) {
                std::memcpy(px, index[b], 4);
            } else if ((b & 0xC0) == 0x40) {
                px[0] = static_cast<std::uint8_t>(px[0] + ((b >> 4) & 3) - 2);
                px[1] = static_cast<std::uint8_t>(px[1] + ((b >> 2) & 3) - 2);
                px[2] = static_cast<std::uint8_t>(px[2] + (b & 3) - 2);
            } else if ((b & 0xC0) == 0x80) {
                if (p >= size) return fmt::fail(error, "truncated QOI");
                const std::uint8_t b2 = data[p++];
                const int vg = (b & 0x3F) - 32;
                px[0] = static_cast<std::uint8_t>(px[0] + vg - 8 + ((b2 >> 4) & 15));
                px[1] = static_cast<std::uint8_t>(px[1] + vg);
                px[2] = static_cast<std::uint8_t>(px[2] + vg - 8 + (b2 & 15));
            } else {
                run = b & 0x3F;
            }

            std::memcpy(index[qoi_hash(px)], px, 4);
        } else {
            return fmt::fail(error, "truncated QOI");
        }

        std::memcpy(img.px.data() + i * 4, px, 4);
        if (channels == 3) img.px[i * 4 + 3] = 255;
    }

    out = img.to_bitmap();
    return true;
}

bool encode_qoi(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha) {
    out.clear();
    if (!img.is_valid_image()) return false;
    std::vector<std::uint8_t> rgba = img.to_rgba();
    if (!alpha) for (std::size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    out.insert(out.end(), { 'q', 'o', 'i', 'f' });
    fmt::put_be32(out, img.width());
    fmt::put_be32(out, img.height());
    out.push_back(alpha ? 4 : 3);
    out.push_back(0);
    std::uint8_t index[64][4] = {};
    std::uint8_t prev[4] = { 0, 0, 0, 255 };
    unsigned run = 0;
    const std::size_t total = rgba.size() / 4;

    for (std::size_t i = 0; i < total; ++i) {
        const std::uint8_t* px = rgba.data() + i * 4;

        if (std::memcmp(px, prev, 4) == 0) {
            ++run;
            if (run == 62 || i + 1 == total) {
                out.push_back(static_cast<std::uint8_t>(0xC0 | (run - 1)));
                run = 0;
            }
            continue;
        }

        if (run) {
            out.push_back(static_cast<std::uint8_t>(0xC0 | (run - 1)));
            run = 0;
        }

        const unsigned h = qoi_hash(px);

        if (std::memcmp(index[h], px, 4) == 0) {
            out.push_back(static_cast<std::uint8_t>(h));
        } else {
            std::memcpy(index[h], px, 4);

            if (px[3] == prev[3]) {
                const int vr = static_cast<std::int8_t>(px[0] - prev[0]);
                const int vg = static_cast<std::int8_t>(px[1] - prev[1]);
                const int vb = static_cast<std::int8_t>(px[2] - prev[2]);
                const int vgr = vr - vg, vgb = vb - vg;

                if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 && vb >= -2 && vb <= 1) {
                    out.push_back(static_cast<std::uint8_t>(0x40 | ((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2)));
                } else if (vgr >= -8 && vgr <= 7 && vg >= -32 && vg <= 31 && vgb >= -8 && vgb <= 7) {
                    out.push_back(static_cast<std::uint8_t>(0x80 | (vg + 32)));
                    out.push_back(static_cast<std::uint8_t>(((vgr + 8) << 4) | (vgb + 8)));
                } else {
                    out.push_back(0xFE);
                    out.insert(out.end(), px, px + 3);
                }
            } else {
                out.push_back(0xFF);
                out.insert(out.end(), px, px + 4);
            }
        }

        std::memcpy(prev, px, 4);
    }

    out.insert(out.end(), { 0, 0, 0, 0, 0, 0, 0, 1 });
    return true;
}

} // namespace images
} // namespace fizmo
