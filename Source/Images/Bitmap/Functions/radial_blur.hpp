#ifndef FIZMO_IMAGE_FUNCTIONS_RADIAL_BLUR_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_RADIAL_BLUR_IMAGE_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"
#include <cmath>
#include <vector>

namespace fizmo {
namespace images {

BitmapImage radial_blur(const BitmapImage& source, const unsigned int centerX, const unsigned int centerY, const double maxBlurRadius) noexcept;

inline BitmapImage radial_blur(const BitmapImage& source, const double maxBlurRadius) noexcept {
    return radial_blur(
        source,
        source.width() / 2,
        source.height() / 2,
        maxBlurRadius
    );
}

inline void radial_blur_original(BitmapImage& source, const unsigned int centerX, const unsigned int centerY, const double maxBlurRadius) noexcept {
    source = radial_blur(source, centerX, centerY, maxBlurRadius);
}

inline void radial_blur_original(BitmapImage& source, const double maxBlurRadius) noexcept {
    source = radial_blur(source, maxBlurRadius);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_RADIAL_BLUR_IMAGE_HPP