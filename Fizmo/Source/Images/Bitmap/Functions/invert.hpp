#ifndef FIZMO_IMAGE_FUNCTIONS_INVERT_HPP
#define FIZMO_IMAGE_FUNCTIONS_INVERT_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

inline BitmapImage invert(const BitmapImage& source) noexcept {
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const fizmo::graphics::Color inverted(
                255 - pixel.red(),
                255 - pixel.green(),
                255 - pixel.blue()
            );
            
            result.set_pixel(x, y, inverted);
        }
    }
    
    return result;
}

inline void invert_original(BitmapImage& source) noexcept {
    source = invert(source);
}

inline BitmapImage invert(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            
            const fizmo::graphics::Color inverted(
                255 - pixel.red(),
                255 - pixel.green(),
                255 - pixel.blue()
            );
            
            result.set_pixel(x, y, inverted);
        }
    }
    
    return result;
}

inline void invert_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2
) noexcept {
    source = invert(source, x1, y1, x2, y2);
}

inline BitmapImage invert_partial(const BitmapImage& source, const double percentage) noexcept {
    double factor = percentage;
    fizmo::clamp_value(factor, 0.0, 100.0);
    factor /= 100.0;
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int inverted_red = 255 - pixel.red();
            const unsigned int inverted_green = 255 - pixel.green();
            const unsigned int inverted_blue = 255 - pixel.blue();
            const unsigned int new_red = static_cast<unsigned int>(pixel.red() * (1 - factor) + inverted_red * factor);
            const unsigned int new_green = static_cast<unsigned int>(pixel.green() * (1 - factor) + inverted_green * factor);
            const unsigned int new_blue = static_cast<unsigned int>(pixel.blue() * (1 - factor) + inverted_blue * factor);
            result.set_pixel(x, y, fizmo::graphics::Color(new_red, new_green, new_blue));
        }
    }
    
    return result;
}

inline void invert_partial_original(BitmapImage& source, const double percentage) noexcept {
    source = invert_partial(source, percentage);
}

inline BitmapImage invert_partial(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double percentage
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    double factor = percentage;
    fizmo::clamp_value(factor, 0.0, 100.0);
    factor /= 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int inverted_red = 255 - pixel.red();
            const unsigned int inverted_green = 255 - pixel.green();
            const unsigned int inverted_blue = 255 - pixel.blue();
            const unsigned int new_red = static_cast<unsigned int>(pixel.red() * (1 - factor) + inverted_red * factor);
            const unsigned int new_green = static_cast<unsigned int>(pixel.green() * (1 - factor) + inverted_green * factor);
            const unsigned int new_blue = static_cast<unsigned int>(pixel.blue() * (1 - factor) + inverted_blue * factor);
            result.set_pixel(x, y, fizmo::graphics::Color(new_red, new_green, new_blue));
        }
    }
    
    return result;
}

inline void invert_partial_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double percentage
) noexcept {
    source = invert_partial(source, x1, y1, x2, y2, percentage);
}

inline BitmapImage invert_channels(const BitmapImage& source, const bool invert_red, const bool invert_green, const bool invert_blue) noexcept {
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int new_red = invert_red ? 255 - pixel.red() : pixel.red();
            const unsigned int new_green = invert_green ? 255 - pixel.green() : pixel.green();
            const unsigned int new_blue = invert_blue ? 255 - pixel.blue() : pixel.blue();
            result.set_pixel(x, y, fizmo::graphics::Color(new_red, new_green, new_blue));
        }
    }
    
    return result;
}

inline void invert_channels_original(BitmapImage& source, const bool invert_red, const bool invert_green, const bool invert_blue) noexcept {
    source = invert_channels(source, invert_red, invert_green, invert_blue);
}

inline BitmapImage invert_channels(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const bool invert_red, const bool invert_green, const bool invert_blue
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int new_red = invert_red ? 255 - pixel.red() : pixel.red();
            const unsigned int new_green = invert_green ? 255 - pixel.green() : pixel.green();
            const unsigned int new_blue = invert_blue ? 255 - pixel.blue() : pixel.blue();
            result.set_pixel(x, y, fizmo::graphics::Color(new_red, new_green, new_blue));
        }
    }
    
    return result;
}

inline void invert_channels_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const bool invert_red, const bool invert_green, const bool invert_blue
) noexcept {
    source = invert_channels(source, x1, y1, x2, y2, invert_red, invert_green, invert_blue);
}

inline BitmapImage invert_channels_partial(
    const BitmapImage& source,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept {
    const double red_factor = fizmo::clamp(red_percentage, 0.0, 100.0) / 100.0;
    const double green_factor = fizmo::clamp(green_percentage, 0.0, 100.0) / 100.0;
    const double blue_factor = fizmo::clamp(blue_percentage, 0.0, 100.0) / 100.0;
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int inverted_red = 255 - pixel.red();
            const unsigned int inverted_green = 255 - pixel.green();
            const unsigned int inverted_blue = 255 - pixel.blue();
            const unsigned int new_red = static_cast<unsigned int>(pixel.red() * (1 - red_factor) + inverted_red * red_factor);
            const unsigned int new_green = static_cast<unsigned int>(pixel.green() * (1 - green_factor) + inverted_green * green_factor);
            const unsigned int new_blue = static_cast<unsigned int>(pixel.blue() * (1 - blue_factor) + inverted_blue * blue_factor);
            result.set_pixel(x, y, fizmo::graphics::Color(new_red, new_green, new_blue));
        }
    }
    
    return result;
}

inline void invert_channels_partial_original(
    BitmapImage& source,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept {
    source = invert_channels_partial(source, red_percentage, green_percentage, blue_percentage);
}

inline BitmapImage invert_channels_partial(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1,
    unsigned int x2, unsigned int y2,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept {
    if (x1 > x2) { std::swap(x1, x2); }
    if (y1 > y2) { std::swap(y1, y2); }
    const unsigned int x_1 = fizmo::min_constexpr(x1, source.width());
    const unsigned int x_2 = fizmo::clamp(x2, x_1, source.width());
    const unsigned int y_1 = fizmo::min_constexpr(y1, source.height());
    const unsigned int y_2 = fizmo::clamp(y2, y_1, source.height());
    const double red_factor = fizmo::clamp(red_percentage, 0.0, 100.0) / 100.0;
    const double green_factor = fizmo::clamp(green_percentage, 0.0, 100.0) / 100.0;
    const double blue_factor = fizmo::clamp(blue_percentage, 0.0, 100.0) / 100.0;
    BitmapImage result(source);
    
    for (unsigned int y = y_1; y < y_2; y++) {
        for (unsigned int x = x_1; x < x_2; x++) {
            const fizmo::graphics::Color& pixel = source.get_pixel(x, y);
            const unsigned int inverted_red = 255 - pixel.red();
            const unsigned int inverted_green = 255 - pixel.green();
            const unsigned int inverted_blue = 255 - pixel.blue();
            const unsigned int new_red = static_cast<unsigned int>(pixel.red() * (1 - red_factor) + inverted_red * red_factor);
            const unsigned int new_green = static_cast<unsigned int>(pixel.green() * (1 - green_factor) + inverted_green * green_factor);
            const unsigned int new_blue = static_cast<unsigned int>(pixel.blue() * (1 - blue_factor) + inverted_blue * blue_factor);
            result.set_pixel(x, y, fizmo::graphics::Color(new_red, new_green, new_blue));
        }
    }
    
    return result;
}

inline void invert_channels_partial_original(
    BitmapImage& source,
    const unsigned int x1, const unsigned int y1,
    const unsigned int x2, const unsigned int y2,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept {
    source = invert_channels_partial(source, x1, y1, x2, y2, red_percentage, green_percentage, blue_percentage);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_INVERT_HPP