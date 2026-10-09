#ifndef FIZMO_IMAGE_FUNCTIONS_COLOR_REPLACEMENT_HPP
#define FIZMO_IMAGE_FUNCTIONS_COLOR_REPLACEMENT_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

BitmapImage replace_color(const BitmapImage& source, const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color) noexcept;

inline void replace_color_original(BitmapImage& source, const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color) noexcept {
    source = replace_color(source, old_color, new_color);
}

BitmapImage replace_color(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color
) noexcept;

inline void replace_color_original(BitmapImage& source, const unsigned int x1, const unsigned int y1, const unsigned int x2, const unsigned int y2, const fizmo::graphics::Color& old_color, const fizmo::graphics::Color& new_color) noexcept {
    source = replace_color(source, x1, y1, x2, y2, old_color, new_color);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_COLOR_REPLACEMENT_HPP