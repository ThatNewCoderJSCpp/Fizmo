#ifndef FIZMO_IMAGE_FUNCTIONS_CONTRAST_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_CONTRAST_IMAGE_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"
#include <cmath>

namespace fizmo {
namespace images {

BitmapImage adjust_contrast(const BitmapImage& source, double factor) noexcept;

inline void adjust_contrast_original(BitmapImage& source, const double factor) {
    source = adjust_contrast(source, factor);
}

BitmapImage adjust_contrast(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    double factor
) noexcept;

inline void adjust_contrast_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double factor
) noexcept {
    source = adjust_contrast(source, x1, y1, x2, y2, factor);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_CONTRAST_IMAGE_HPP