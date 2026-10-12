#ifndef FIZMO_IMAGES_FORMAT_UTIL_HPP
#define FIZMO_IMAGES_FORMAT_UTIL_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "image_formats.hpp"

namespace fizmo {
namespace images {
namespace fmt {

struct Rgba {
    unsigned int              width = 0;
    unsigned int              height = 0;
    std::vector<std::uint8_t> px;

    Rgba() = default;
    Rgba(unsigned int w, unsigned int h, std::uint8_t fill = 0) : width(w), height(h), px(static_cast<std::size_t>(w) * h * 4, fill) {}

    std::uint8_t* at(unsigned int x, unsigned int y) noexcept { return px.data() + (static_cast<std::size_t>(y) * width + x) * 4; }
    const std::uint8_t* at(unsigned int x, unsigned int y) const noexcept { return px.data() + (static_cast<std::size_t>(y) * width + x) * 4; }

    BitmapImage to_bitmap() const { return BitmapImage::from_rgba(width, height, px.data()); }

    static Rgba from(const BitmapImage& img) {
        Rgba r;
        r.width = img.width();
        r.height = img.height();
        r.px = img.to_rgba();
        return r;
    }

    bool has_alpha() const noexcept {
        for (std::size_t i = 3; i < px.size(); i += 4) if (px[i] != 255) return true;
        return false;
    }

    void clear_rect(unsigned int x0, unsigned int y0, unsigned int w, unsigned int h) noexcept {
        for (unsigned int y = y0; y < y0 + h && y < height; ++y)
            for (unsigned int x = x0; x < x0 + w && x < width; ++x) std::memset(at(x, y), 0, 4);
    }

    void blend_over(unsigned int x, unsigned int y, const std::uint8_t* src) noexcept {
        std::uint8_t* d = at(x, y);
        const unsigned sa = src[3];
        if (sa == 255) { std::memcpy(d, src, 4); return; }
        if (sa == 0) return;
        const unsigned da = d[3];
        const unsigned oa = sa * 255 + da * (255 - sa);
        if (oa == 0) { std::memset(d, 0, 4); return; }

        for (int c = 0; c < 3; ++c) {
            const unsigned v = (src[c] * sa * 255 + d[c] * da * (255 - sa) + oa / 2) / oa;
            d[c] = static_cast<std::uint8_t>(v > 255 ? 255 : v);
        }

        d[3] = static_cast<std::uint8_t>((oa + 127) / 255);
    }
};

class Reader {
public:
    Reader(const std::uint8_t* p, std::size_t n) noexcept : m_p(p), m_n(n) {}

    bool ok() const noexcept { return m_ok; }
    std::size_t pos() const noexcept { return m_pos; }
    std::size_t size() const noexcept { return m_n; }
    std::size_t remaining() const noexcept { return m_pos <= m_n ? m_n - m_pos : 0; }
    const std::uint8_t* data() const noexcept { return m_p; }
    const std::uint8_t* cur() const noexcept { return m_p + m_pos; }

    void seek(std::size_t p) noexcept {
        if (p > m_n) m_ok = false;
        else m_pos = p;
    }

    void skip(std::size_t n) noexcept {
        if (n > remaining()) {
            m_ok = false;
            m_pos = m_n;
        } else {
            m_pos += n;
        }
    }

    bool need(std::size_t n) noexcept {
        if (n > remaining()) m_ok = false;
        return m_ok;
    }

    std::uint8_t u8() noexcept {
        if (!need(1)) return 0;
        return m_p[m_pos++];
    }

    std::uint16_t le16() noexcept {
        if (!need(2)) return 0;
        const std::uint16_t v = static_cast<std::uint16_t>(m_p[m_pos] | (m_p[m_pos + 1] << 8));
        m_pos += 2;
        return v;
    }

    std::uint32_t le24() noexcept {
        if (!need(3)) return 0;
        const std::uint32_t v = m_p[m_pos] | (m_p[m_pos + 1] << 8) | (static_cast<std::uint32_t>(m_p[m_pos + 2]) << 16);
        m_pos += 3;
        return v;
    }

    std::uint32_t le32() noexcept {
        if (!need(4)) return 0;
        const std::uint32_t v = m_p[m_pos] | (m_p[m_pos + 1] << 8) | (static_cast<std::uint32_t>(m_p[m_pos + 2]) << 16) | (static_cast<std::uint32_t>(m_p[m_pos + 3]) << 24);
        m_pos += 4;
        return v;
    }

    std::uint16_t be16() noexcept {
        if (!need(2)) return 0;
        const std::uint16_t v = static_cast<std::uint16_t>((m_p[m_pos] << 8) | m_p[m_pos + 1]);
        m_pos += 2;
        return v;
    }

    std::uint32_t be32() noexcept {
        if (!need(4)) return 0;
        const std::uint32_t v = (static_cast<std::uint32_t>(m_p[m_pos]) << 24) | (static_cast<std::uint32_t>(m_p[m_pos + 1]) << 16) | (m_p[m_pos + 2] << 8) | m_p[m_pos + 3];
        m_pos += 4;
        return v;
    }

    std::int32_t le32s() noexcept { return static_cast<std::int32_t>(le32()); }

private:
    const std::uint8_t* m_p;
    std::size_t         m_n;
    std::size_t         m_pos = 0;
    bool                m_ok = true;
};

inline void put_le16(std::vector<std::uint8_t>& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v));
    o.push_back(static_cast<std::uint8_t>(v >> 8));
}

inline void put_le24(std::vector<std::uint8_t>& o, std::uint32_t v) {
    put_le16(o, v);
    o.push_back(static_cast<std::uint8_t>(v >> 16));
}

inline void put_le32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    put_le16(o, v);
    put_le16(o, v >> 16);
}

inline void put_be16(std::vector<std::uint8_t>& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v >> 8));
    o.push_back(static_cast<std::uint8_t>(v));
}

inline void put_be32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    put_be16(o, v >> 16);
    put_be16(o, v);
}

inline void set_le32(std::vector<std::uint8_t>& o, std::size_t at, std::uint32_t v) {
    o[at] = static_cast<std::uint8_t>(v);
    o[at + 1] = static_cast<std::uint8_t>(v >> 8);
    o[at + 2] = static_cast<std::uint8_t>(v >> 16);
    o[at + 3] = static_cast<std::uint8_t>(v >> 24);
}

inline bool fail(std::string* error, const char* why) {
    if (error) *error = why;
    return false;
}

inline bool sane_dimensions(std::uint64_t w, std::uint64_t h) noexcept { return w > 0 && h > 0 && w <= 65535 * 4 && h <= 65535 * 4 && w * h <= (1ull << 30); }

inline std::uint8_t scale_to8(std::uint64_t v, std::uint64_t max) noexcept {
    if (max == 0) return 255;
    if (v >= max) return 255;
    return static_cast<std::uint8_t>((v * 510 + max) / (2 * max));
}

bool decode_dib(const std::uint8_t* file, std::size_t file_size, std::size_t dib_offset, std::size_t pixel_offset, bool icon, Rgba& out, std::string* error);

bool zlib_inflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out, std::size_t expected = 0);
bool zlib_deflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out, int level = 6);
std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0);

} // namespace fmt
} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGES_FORMAT_UTIL_HPP
