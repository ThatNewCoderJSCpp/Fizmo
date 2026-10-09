#include "fizmo_library.hpp"
#include "grayscale.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_grayscale(const BitmapImage& source, double percentage) noexcept {
    fizmo::clamp_value(percentage, 0.0, 100.0);
    double factor = percentage / 100.0;
    
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const unsigned int gray = static_cast<int>(
                0.299 * pixel.red() +
                0.587 * pixel.green() +
                0.114 * pixel.blue()
            );
            
            const unsigned int newRed = static_cast<unsigned int>(pixel.red() * (1 - factor) + gray * factor);
            const unsigned int newGreen = static_cast<unsigned int>(pixel.green() * (1 - factor) + gray * factor);
            const unsigned int newBlue = static_cast<unsigned int>(pixel.blue() * (1 - factor) + gray * factor);
            
            result.set_pixel(x, y, fizmo::graphics::Color(newRed, newGreen, newBlue));
        }
    }
    
    return result;
}

BitmapImage adjust_grayscale(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    double percentage
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    fizmo::clamp_value(percentage, 0.0, 100.0);
    double factor = percentage / 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const unsigned int gray = static_cast<int>(
                0.299 * pixel.red() +
                0.587 * pixel.green() +
                0.114 * pixel.blue()
            );
            
            const unsigned int newRed = static_cast<unsigned int>(pixel.red() * (1 - factor) + gray * factor);
            const unsigned int newGreen = static_cast<unsigned int>(pixel.green() * (1 - factor) + gray * factor);
            const unsigned int newBlue = static_cast<unsigned int>(pixel.blue() * (1 - factor) + gray * factor);
            
            result.set_pixel(x, y, fizmo::graphics::Color(newRed, newGreen, newBlue));
        }
    }
    
    return result;
}

} // namespace images
} // namespace fizmo
