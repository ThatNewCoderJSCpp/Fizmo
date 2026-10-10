#include "fizmo_library.hpp"
#include "framebuffer.hpp"

namespace fizmo {
namespace graphics {

void Framebuffer::resize(unsigned int w, unsigned int h, const Color& fill) {
    m_width  = w;
    m_height = h;
    m_pixels.assign(static_cast<std::size_t>(w) * h, fill);
    m_version.touch();
}

void Framebuffer::set_pixel(int x, int y, const Color& c) noexcept {
    if (!in_bounds(x, y)) return;
    m_pixels[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)] = c;
    m_version.touch();
}

auto Framebuffer::get_pixel(int x, int y) const noexcept -> Color {
    if (in_bounds(x, y)) return m_pixels[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)];
    return {};
}

auto Framebuffer::to_bitmap_image() const -> images::BitmapImage {
    images::BitmapImage img(m_width, m_height);
    for (unsigned int y = 0; y < m_height; ++y)
        for (unsigned int x = 0; x < m_width; ++x)
            img.set_pixel(x, y, m_pixels[static_cast<std::size_t>(y) * m_width + x]);
    return img;
}

} // namespace graphics
} // namespace fizmo
