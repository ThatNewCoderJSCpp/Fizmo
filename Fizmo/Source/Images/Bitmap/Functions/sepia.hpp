#ifndef FIZMO_IMAGE_FUNCTIONS_SEPIA_HPP
#define FIZMO_IMAGE_FUNCTIONS_SEPIA_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

BitmapImage apply_sepia(const BitmapImage& source, const double intensity_percent = 100.0) noexcept {
    const double intensity = fizmo::clamp(intensity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const unsigned int sepiaRed = static_cast<unsigned int>(
                (pixel.red() * 0.393) + 
                (pixel.green() * 0.769) + 
                (pixel.blue() * 0.189)
            );
            
            const unsigned int sepiaGreen = static_cast<unsigned int>(
                (pixel.red() * 0.349) + 
                (pixel.green() * 0.686) + 
                (pixel.blue() * 0.168)
            );
            
            const unsigned int sepiaBlue = static_cast<unsigned int>(
                (pixel.red() * 0.272) + 
                (pixel.green() * 0.534) + 
                (pixel.blue() * 0.131)
            );
            
            const unsigned int finalRed = static_cast<unsigned int>(pixel.red() * (1 - intensity) + sepiaRed * intensity);
            const unsigned int finalGreen = static_cast<unsigned int>(pixel.green() * (1 - intensity) + sepiaGreen * intensity);
            const unsigned int finalBlue = static_cast<unsigned int>(pixel.blue() * (1 - intensity) + sepiaBlue * intensity);
            result.set_pixel(x, y, fizmo::graphics::Color(finalRed, finalGreen, finalBlue));
        }
    }
    
    return result;
}

void apply_sepia_original(BitmapImage& source, const double intensity_percent = 100.0) noexcept {
    source = apply_sepia(source, intensity_percent);
}

BitmapImage apply_sepia(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double intensity_percent = 100.0
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    const double intensity = fizmo::clamp(intensity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const unsigned int sepiaRed = static_cast<unsigned int>(
                (pixel.red() * 0.393) + 
                (pixel.green() * 0.769) + 
                (pixel.blue() * 0.189)
            );
            
            const unsigned int sepiaGreen = static_cast<unsigned int>(
                (pixel.red() * 0.349) + 
                (pixel.green() * 0.686) + 
                (pixel.blue() * 0.168)
            );
            
            const unsigned int sepiaBlue = static_cast<unsigned int>(
                (pixel.red() * 0.272) + 
                (pixel.green() * 0.534) + 
                (pixel.blue() * 0.131)
            );
            
            const unsigned int finalRed = static_cast<unsigned int>(pixel.red() * (1 - intensity) + sepiaRed * intensity);
            const unsigned int finalGreen = static_cast<unsigned int>(pixel.green() * (1 - intensity) + sepiaGreen * intensity);
            const unsigned int finalBlue = static_cast<unsigned int>(pixel.blue() * (1 - intensity) + sepiaBlue * intensity);
            result.set_pixel(x, y, fizmo::graphics::Color(finalRed, finalGreen, finalBlue));
        }
    }
    
    return result;
}

void apply_sepia_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double intensity_percent = 100.0
) noexcept {
    source = apply_sepia(source, x1, y1, x2, y2, intensity_percent);
}

BitmapImage apply_custom_sepia(
    const BitmapImage& source,
    const fizmo::graphics::Color& light_tone = fizmo::graphics::Color(255, 240, 192),  // Highlight tone
    const fizmo::graphics::Color& dark_tone = fizmo::graphics::Color(112, 66, 20),     // Shadow tone
    const double intensity_percent = 100.0
) noexcept {
    const double intensity = fizmo::clamp(intensity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const double gray = 
                0.299 * pixel.red() +
                0.587 * pixel.green() +
                0.114 * pixel.blue();
            
            const double t = gray / 255.0;
            unsigned int red = static_cast<unsigned int>(dark_tone.red() * (1 - t) + light_tone.red() * t);
            unsigned int green = static_cast<unsigned int>(dark_tone.green() * (1 - t) + light_tone.green() * t);
            unsigned int blue = static_cast<unsigned int>(dark_tone.blue() * (1 - t) + light_tone.blue() * t);
            red = static_cast<unsigned int>(pixel.red() * (1 - intensity) + red * intensity);
            green = static_cast<unsigned int>(pixel.green() * (1 - intensity) + green * intensity);
            blue = static_cast<unsigned int>(pixel.blue() * (1 - intensity) + blue * intensity);
            result.set_pixel(x, y, fizmo::graphics::Color(red, green, blue));
        }
    }
    
    return result;
}

void apply_custom_sepia_original(
    BitmapImage& source,
    const fizmo::graphics::Color& light_tone = fizmo::graphics::Color(255, 240, 192),
    const fizmo::graphics::Color& dark_tone = fizmo::graphics::Color(112, 66, 20),
    const double intensity_percent = 100.0
) noexcept {
    source = apply_custom_sepia(source, light_tone, dark_tone, intensity_percent);
}

BitmapImage apply_custom_sepia(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const fizmo::graphics::Color& light_tone = fizmo::graphics::Color(255, 240, 192),
    const fizmo::graphics::Color& dark_tone = fizmo::graphics::Color(112, 66, 20),
    const double intensity_percent = 100.0
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    const double intensity = fizmo::clamp(intensity_percent, 0.0, 100.0) / 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const double gray = 
                0.299 * pixel.red() +
                0.587 * pixel.green() +
                0.114 * pixel.blue();
            
            const double t = gray / 255.0;
            unsigned int red = static_cast<unsigned int>(dark_tone.red() * (1 - t) + light_tone.red() * t);
            unsigned int green = static_cast<unsigned int>(dark_tone.green() * (1 - t) + light_tone.green() * t);
            unsigned int blue = static_cast<unsigned int>(dark_tone.blue() * (1 - t) + light_tone.blue() * t);
            red = static_cast<unsigned int>(pixel.red() * (1 - intensity) + red * intensity);
            green = static_cast<unsigned int>(pixel.green() * (1 - intensity) + green * intensity);
            blue = static_cast<unsigned int>(pixel.blue() * (1 - intensity) + blue * intensity);
            result.set_pixel(x, y, fizmo::graphics::Color(red, green, blue));
        }
    }
    
    return result;
}

void apply_custom_sepia_original(
    BitmapImage& source,
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2,
    const fizmo::graphics::Color& light_tone = fizmo::graphics::Color(255, 240, 192),
    const fizmo::graphics::Color& dark_tone = fizmo::graphics::Color(112, 66, 20),
    const double intensity_percent = 100.0
) noexcept {
    source = apply_custom_sepia(source, x1, y1, x2, y2, light_tone, dark_tone, intensity_percent);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_SEPIA_HPP