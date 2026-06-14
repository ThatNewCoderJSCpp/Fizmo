#ifndef FIZMO_IMAGE_FUNCTIONS_COLOR_REPLACEMENT_HPP
#define FIZMO_IMAGE_FUNCTIONS_COLOR_REPLACEMENT_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

BitmapImage replace_color(const BitmapImage& source, const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color) noexcept {
    BitmapImage result(source);
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            if (pixel == old_color) {
                result.set_pixel(x, y, new_color);
            }
        }
    }
    
    return result;
}

void replace_color_original(BitmapImage& source, const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color) noexcept {
    source = replace_color(source, old_color, new_color);
}

BitmapImage replace_color(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            fizmo::graphics::Color pixel = source.get_pixel(x, y);
            if (pixel == old_color) {
                result.set_pixel(x, y, new_color);
            }
        }
    }
    
    return result;
}

void replace_color_original(BitmapImage& source, const unsigned int x1, const unsigned int y1, const unsigned int x2, const unsigned int y2, const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color) noexcept {
    source = replace_color(source, x1, y1, x2, y2, old_color, new_color);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_COLOR_REPLACEMENT_HPP