#ifndef FIZMO_IMAGE_FUNCTIONS_BRIGHTNESS_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_BRIGHTNESS_IMAGE_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_brightness(const BitmapImage& source, const int adjustment) noexcept;

inline void adjust_brightness_original(BitmapImage& source, const int adjustment) noexcept {
    source = adjust_brightness(source, adjustment);
}

BitmapImage adjust_brightness(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const int adjustment
) noexcept;

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