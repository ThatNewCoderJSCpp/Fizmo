#ifndef FIZMO_IMAGE_FUNCTIONS_BRIGHTNESS_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_BRIGHTNESS_IMAGE_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

inline BitmapImage adjust_brightness(const BitmapImage& source, const int adjustment) noexcept {
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            result.set_pixel(x, y, fizmo::graphics::Color(
                pixel.red() + adjustment,
                pixel.green() + adjustment,
                pixel.blue() + adjustment
            ));
        }
    }
    
    return result;
}

inline void adjust_brightness_original(BitmapImage& source, const int adjustment) noexcept {
    source = adjust_brightness(source, adjustment);
}

inline BitmapImage adjust_brightness(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const int adjustment
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
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            result.set_pixel(x, y, fizmo::graphics::Color(
                pixel.red() + adjustment,
                pixel.green() + adjustment,
                pixel.blue() + adjustment
            ));
        }
    }
    
    return result;
}

inline void adjust_brightness_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const int adjustment
) noexcept {
    source = adjust_brightness(source, x1, y1, x2, y2, adjustment);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_BRIGHTNESS_IMAGE_HPP