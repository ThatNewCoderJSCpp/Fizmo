#ifndef FIZMO_FRAMEBUFFER_HPP
#define FIZMO_FRAMEBUFFER_HPP

#include "color.hpp"
#include "content_version.hpp"
#include "../Images/Bitmap/image.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace graphics {

class Framebuffer {
private:
    std::vector<Color>    m_pixels;
    unsigned int          m_width  = 0;
    unsigned int          m_height = 0;
    fizmo::ContentVersion m_version;

public:
    Framebuffer() = default;
    Framebuffer(unsigned int w, unsigned int h, const Color& fill = Color()) : m_pixels(static_cast<std::size_t>(w) * h, fill), m_width(w), m_height(h) {}

    void resize(unsigned int w, unsigned int h, const Color& fill = Color());

    unsigned int width()  const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }

    bool in_bounds(int x, int y) const noexcept {
        return x >= 0 && x < static_cast<int>(m_width)
            && y >= 0 && y < static_cast<int>(m_height);
    }

    void set_pixel(int x, int y, const Color& c) noexcept;

    Color get_pixel(int x, int y) const noexcept;

    void clear(const Color& c = Color()) noexcept {
        std::fill(m_pixels.begin(), m_pixels.end(), c);
        m_version.touch();
    }

    const Color* data() const noexcept { return m_pixels.data(); }
    Color*       data()       noexcept { m_version.touch(); return m_pixels.data(); }
    const std::vector<Color>& pixels() const noexcept { return m_pixels; }

    void mark_dirty() noexcept { m_version.touch(); }
    std::uint64_t version() const noexcept { return m_version.get(); }

    images::BitmapImage to_bitmap_image() const;

    bool save_to_bmp(const std::string& path) const { return to_bitmap_image().save_to_file(path); }
    bool save_to_png(const std::string& path) const { return to_bitmap_image().save_to_png(path); }
    bool save_to_jpeg(const std::string& path, int quality = 90) const { return to_bitmap_image().save_to_jpeg(path, quality); }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_FRAMEBUFFER_HPP