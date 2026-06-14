#ifndef FIZMO_IMAGE_FUNCTIONS_HUE_ROTATE_HPP
#define FIZMO_IMAGE_FUNCTIONS_HUE_ROTATE_HPP

#include "../image.hpp"
#include "../../../Graphics/hsl_color.hpp"
#include "../../../Basic/constants.hpp"

namespace fizmo {
namespace images {

BitmapImage rotate_hue(const BitmapImage& source, const double degrees) noexcept {
    double adjusted_degrees = std::fmod(degrees, 360.0);
    if (adjusted_degrees < 0) { adjusted_degrees += 360.0; }
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            fizmo::graphics::HSLColor hsl(pixel);
            const double new_hue = hsl.hue() + adjusted_degrees;
            hsl.set_hue(new_hue); 
            const fizmo::graphics::Color& rotated(hsl);
            result.set_pixel(x, y, rotated);
        }
    }
    
    return result;
}

void rotate_hue_original(BitmapImage& source, const double degrees) noexcept {
    source = rotate_hue(source, degrees);
}

BitmapImage rotate_hue(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double degrees
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    double adjusted_degrees = std::fmod(degrees, 360.0);
    if (adjusted_degrees < 0) { adjusted_degrees += 360.0; }
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            fizmo::graphics::HSLColor hsl(pixel);
            const double new_hue = hsl.hue() + adjusted_degrees;
            hsl.set_hue(new_hue); 
            const fizmo::graphics::Color& rotated(hsl);
            result.set_pixel(x, y, rotated);
        }
    }
    
    return result;
}

void rotate_hue_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double degrees
) noexcept {
    source = rotate_hue(source, x1, y1, x2, y2, degrees);
}

BitmapImage create_hue_wheel(const unsigned int size) noexcept {
    BitmapImage wheel(size, size);
    const unsigned int center = size / 2;
    const unsigned int radius = center > 0 ? center - 1 : 0;
    
    for (unsigned int y = 0; y < size; y++) {
        for (unsigned int x = 0; x < size; x++) {
            const double dx = static_cast<double>(x) - center;
            const double dy = static_cast<double>(y) - center;
            const double distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance <= radius) {
                double angle = std::atan2(dy, dx) * (180.0 / fizmo::constants::PI<double>);
                if (angle < 0) angle += 360.0;
                const double saturation = (distance / radius) * 100.0;
                const fizmo::graphics::HSLColor hsl(angle, saturation, 50.0);
                wheel.set_pixel(x, y, fizmo::graphics::Color(hsl));
            }
        }
    }
    
    return wheel;
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_HUE_ROTATE_HPP