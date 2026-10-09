#ifndef FIZMO_IMAGE_FUNCTIONS_GRAYSCALE_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_GRAYSCALE_IMAGE_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

inline BitmapImage adjust_grayscale(const BitmapImage& source, double percentage) noexcept {
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

inline BitmapImage adjust_greyscale(const BitmapImage& source, const double percentage) noexcept { 
    return adjust_grayscale(source, percentage); 
}

inline void adjust_grayscale_original(BitmapImage& source, const double percentage) noexcept { 
    source = adjust_grayscale(source, percentage); 
}

inline void adjust_greyscale_original(BitmapImage& source, const double percentage) noexcept { 
    source = adjust_grayscale(source, percentage); 
}

inline BitmapImage adjust_grayscale(
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

inline BitmapImage adjust_greyscale(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double percentage
) noexcept {
    return adjust_grayscale(source, x1, y1, x2, y2, percentage);
}

inline void adjust_grayscale_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double percentage
) noexcept {
    source = adjust_grayscale(source, x1, y1, x2, y2, percentage);
}

inline void adjust_greyscale_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double percentage
) noexcept {
    source = adjust_grayscale(source, x1, y1, x2, y2, percentage);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_GRAYSCALE_IMAGE_HPP