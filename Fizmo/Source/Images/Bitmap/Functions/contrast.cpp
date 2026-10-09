#include "fizmo_library.hpp"
#include "contrast.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_contrast(const BitmapImage& source, double factor) noexcept {
    BitmapImage result(source.width(), source.height());
    fizmo::clamp_value(factor, 0.0, 1.0);
    const int midpoint = 128;
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int newRed = static_cast<unsigned int>(midpoint + (pixel.red() - midpoint) * factor);
            const unsigned int newGreen = static_cast<unsigned int>(midpoint + (pixel.green() - midpoint) * factor);
            const unsigned int newBlue = static_cast<unsigned int>(midpoint + (pixel.blue() - midpoint) * factor);
            result.set_pixel(x, y, fizmo::graphics::Color(newRed, newGreen, newBlue));
        }
    }
    
    return result;
}

BitmapImage adjust_contrast(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    double factor
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    fizmo::clamp_value(factor, 0.0, 1.0);
    const int midpoint = 128;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int newRed = static_cast<unsigned int>(midpoint + (pixel.red() - midpoint) * factor);
            const unsigned int newGreen = static_cast<unsigned int>(midpoint + (pixel.green() - midpoint) * factor);
            const unsigned int newBlue = static_cast<unsigned int>(midpoint + (pixel.blue() - midpoint) * factor);
            
            result.set_pixel(x, y, fizmo::graphics::Color(
                newRed, newGreen, newBlue
            ));
        }
    }
    
    return result;
}

} // namespace images
} // namespace fizmo
