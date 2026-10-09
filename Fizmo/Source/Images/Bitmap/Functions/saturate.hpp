#ifndef FIZMO_IMAGE_FUNCTIONS_SATURATION_HPP
#define FIZMO_IMAGE_FUNCTIONS_SATURATION_HPP

#include "../image.hpp"
#include "../../../Graphics/hsl_color.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

inline BitmapImage adjust_saturation(const BitmapImage& source, const double saturation_percentage) noexcept {
    const double factor = fizmo::clamp(saturation_percentage, 0.0, 100.0) / 100.0;
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            fizmo::graphics::HSLColor hsl(pixel);
            const double new_saturation = hsl.saturation() * factor;
            hsl.set_saturation(new_saturation);
            result.set_pixel(x, y, fizmo::graphics::Color(hsl));
        }
    }
    
    return result;
}

inline void adjust_saturation_original(BitmapImage& source, const double saturation_percentage) noexcept {
    source = adjust_saturation(source, saturation_percentage);
}

inline BitmapImage adjust_saturation(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double saturation_percentage
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    const double factor = fizmo::clamp(saturation_percentage, 0.0, 100.0) / 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            fizmo::graphics::HSLColor hsl(pixel);
            const double new_saturation = hsl.saturation() * factor;
            hsl.set_saturation(new_saturation);
            result.set_pixel(x, y, fizmo::graphics::Color(hsl));
        }
    }
    
    return result;
}

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