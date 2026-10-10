#include "fizmo_library.hpp"
#include "opacity.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_opacity(const BitmapImage& source, const double opacity_percent) noexcept {
    const double opacity = fizmo::clamp(opacity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source.width(), source.height());
    const fizmo::graphics::Color background(255, 255, 255);
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const unsigned int red = static_cast<unsigned int>(background.red() * (1 - opacity) + pixel.red() * opacity);
            const unsigned int green = static_cast<unsigned int>(background.green() * (1 - opacity) + pixel.green() * opacity);
            const unsigned int blue = static_cast<unsigned int>(background.blue() * (1 - opacity) + pixel.blue() * opacity);
            
            result.set_pixel(x, y, fizmo::graphics::Color(red, green, blue));
        }
    }
    
    return result;
}

BitmapImage adjust_opacity(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1,
    unsigned int x2, unsigned int y2,
    const double opacity_percent
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    const double opacity = fizmo::clamp(opacity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source);
    const fizmo::graphics::Color background(255, 255, 255);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int red = static_cast<unsigned int>(background.red() * (1 - opacity) + pixel.red() * opacity);
            const unsigned int green = static_cast<unsigned int>(background.green() * (1 - opacity) + pixel.green() * opacity);
            const unsigned int blue = static_cast<unsigned int>(background.blue() * (1 - opacity) + pixel.blue() * opacity);
            result.set_pixel(x, y, fizmo::graphics::Color(red, green, blue));
        }
    }
    
    return result;
}

BitmapImage adjust_opacity(
    const BitmapImage& source,
    const double opacity_percent,
    const fizmo::graphics::Color& background_color
) noexcept {
    const double opacity = fizmo::clamp(opacity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int red = static_cast<unsigned int>(background_color.red() * (1 - opacity) + pixel.red() * opacity);
            const unsigned int green = static_cast<unsigned int>(background_color.green() * (1 - opacity) + pixel.green() * opacity);
            const unsigned int blue = static_cast<unsigned int>(background_color.blue() * (1 - opacity) + pixel.blue() * opacity);
            result.set_pixel(x, y, fizmo::graphics::Color(red, green, blue));
        }
    }
    
    return result;
}

BitmapImage adjust_opacity(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1,
    unsigned int x2, unsigned int y2,
    const double opacity_percent,
    const fizmo::graphics::Color& background_color
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    const double opacity = fizmo::clamp(opacity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int red = static_cast<unsigned int>(background_color.red() * (1 - opacity) + pixel.red() * opacity);
            const unsigned int green = static_cast<unsigned int>(background_color.green() * (1 - opacity) + pixel.green() * opacity);
            const unsigned int blue = static_cast<unsigned int>(background_color.blue() * (1 - opacity) + pixel.blue() * opacity);
            result.set_pixel(x, y, fizmo::graphics::Color(red, green, blue));
        }
    }
    
    return result;
}

} // namespace images
} // namespace fizmo
