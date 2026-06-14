#ifndef FIZMO_IMAGE_FUNCTIONS_BLUR_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_BLUR_IMAGE_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

BitmapImage blur(const BitmapImage& source, const unsigned int radius) noexcept {
    BitmapImage result(source.width(), source.height());
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            int totalR = 0, totalG = 0, totalB = 0;
            int count = 0;
            
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < source.width() && ny >= 0 && ny < source.height()) {
                        fizmo::graphics::Color pixel = source.get_pixel(nx, ny);
                        totalR += pixel.red();
                        totalG += pixel.green();
                        totalB += pixel.blue();
                        count++;
                    }
                }
            }
            
            result.set_pixel(x, y, fizmo::graphics::Color(
                totalR / count,
                totalG / count,
                totalB / count
            ));
        }
    }
    
    return result;
}

void blur_original(BitmapImage& source, const int radius) noexcept {
    source = blur(source, radius);
}

BitmapImage blur(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const unsigned int radius
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
            int totalR = 0, totalG = 0, totalB = 0;
            unsigned int count = 0;

            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;

                    if (nx >= 0 && nx < source.width() && ny >= 0 && ny < source.height()) {
                        fizmo::graphics::Color pixel = source.get_pixel(nx, ny);
                        totalR += pixel.red();
                        totalG += pixel.green();
                        totalB += pixel.blue();
                        count++;
                    }
                }
            }

            result.set_pixel(x, y, fizmo::graphics::Color(
                totalR / count,
                totalG / count,
                totalB / count
            ));
        }
    }

    return result;
}

void blur_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const unsigned int radius
) noexcept {
    source = blur(source, x1, y1, x2, y2, radius);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_BLUR_IMAGE_HPP