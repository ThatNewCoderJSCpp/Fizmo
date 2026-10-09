#ifndef FIZMO_IMAGE_FUNCTIONS_HUE_ROTATE_HPP
#define FIZMO_IMAGE_FUNCTIONS_HUE_ROTATE_HPP

#include "../image.hpp"
#include "../../../Graphics/hsl_color.hpp"
#include "../../../Basic/constants.hpp"

namespace fizmo {
namespace images {

BitmapImage rotate_hue(const BitmapImage& source, const double degrees) noexcept;

inline void rotate_hue_original(BitmapImage& source, const double degrees) noexcept {
    source = rotate_hue(source, degrees);
}

BitmapImage rotate_hue(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double degrees
) noexcept;

inline void rotate_hue_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double degrees
) noexcept {
    source = rotate_hue(source, x1, y1, x2, y2, degrees);
}

BitmapImage create_hue_wheel(const unsigned int size) noexcept;

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_HUE_ROTATE_HPP