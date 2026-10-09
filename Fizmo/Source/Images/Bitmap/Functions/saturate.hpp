#ifndef FIZMO_IMAGE_FUNCTIONS_SATURATION_HPP
#define FIZMO_IMAGE_FUNCTIONS_SATURATION_HPP

#include "../image.hpp"
#include "../../../Graphics/hsl_color.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_saturation(const BitmapImage& source, const double saturation_percentage) noexcept;

inline void adjust_saturation_original(BitmapImage& source, const double saturation_percentage) noexcept {
    source = adjust_saturation(source, saturation_percentage);
}

BitmapImage adjust_saturation(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double saturation_percentage
) noexcept;

inline void adjust_saturation_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double saturation_percentage
) noexcept {
    source = adjust_saturation(source, x1, y1, x2, y2, saturation_percentage);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_SATURATION_HPP