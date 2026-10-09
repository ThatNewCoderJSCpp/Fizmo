#ifndef FIZMO_IMAGE_FUNCTIONS_BLUR_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_BLUR_IMAGE_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

BitmapImage blur(const BitmapImage& source, const unsigned int radius) noexcept;

inline void blur_original(BitmapImage& source, const int radius) noexcept {
    source = blur(source, radius);
}

BitmapImage blur(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const unsigned int radius
) noexcept;

inline void blur_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const unsigned int radius
) noexcept {
    source = blur(source, x1, y1, x2, y2, radius);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_BLUR_IMAGE_HPP