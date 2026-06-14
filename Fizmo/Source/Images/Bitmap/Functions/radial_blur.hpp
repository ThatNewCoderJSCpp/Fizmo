#ifndef FIZMO_IMAGE_FUNCTIONS_RADIAL_BLUR_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_RADIAL_BLUR_IMAGE_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"
#include <cmath>
#include <vector>

namespace fizmo {
namespace images {

BitmapImage radial_blur(const BitmapImage& source, const unsigned int centerX, const unsigned int centerY, const double maxBlurRadius) noexcept {
    BitmapImage result(source.width(), source.height());
    const unsigned int numSamples = 20;  
    const double maxDistance = std::sqrt(source.width() * source.width() + source.height() * source.height()) / 2;
    
    for (unsigned int y = 0; y < source.height(); y++) {
        for (unsigned int x = 0; x < source.width(); x++) {
            const double dx = static_cast<double>(x) - centerX;
            const double dy = static_cast<double>(y) - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);
            const double blurAmount = (distance / maxDistance) * maxBlurRadius;
            double totalRed = 0, totalGreen = 0, totalBlue = 0;
            double totalWeight = 0;
            
            for (unsigned int i = 0; i < numSamples; i++) {
                const double t = i / static_cast<double>(numSamples - 1);
                const double sampleRadius = blurAmount * t;
                const double angle = std::atan2(dy, dx);
                const int sampleX = static_cast<int>(x - sampleRadius * std::cos(angle));
                const int sampleY = static_cast<int>(y - sampleRadius * std::sin(angle));
                const unsigned int safeX = std::min(std::max(0, sampleX), static_cast<int>(source.width() - 1));
                const unsigned int safeY = std::min(std::max(0, sampleY), static_cast<int>(source.height() - 1));
                const fizmo::graphics::Color& color = source.get_pixel(safeX, safeY);
                const double weight = 1.0 - (t * 0.8);
                totalRed += color.red() * weight;
                totalGreen += color.green() * weight;
                totalBlue += color.blue() * weight;
                totalWeight += weight;
            }
            
            const unsigned int finalRed = static_cast<unsigned int>(totalRed / totalWeight);
            const unsigned int finalGreen = static_cast<unsigned int>(totalGreen / totalWeight);
            const unsigned int finalBlue = static_cast<unsigned int>(totalBlue / totalWeight);
            result.set_pixel(x, y, fizmo::graphics::Color(finalRed, finalGreen, finalBlue));
        }
    }
    
    return result;
}

BitmapImage radial_blur(const BitmapImage& source, const double maxBlurRadius) noexcept {
    return radial_blur(
        source,
        source.width() / 2,
        source.height() / 2,
        maxBlurRadius
    );
}

void radial_blur_original(BitmapImage& source, const unsigned int centerX, const unsigned int centerY, const double maxBlurRadius) noexcept {
    source = radial_blur(source, centerX, centerY, maxBlurRadius);
}

void radial_blur_original(BitmapImage& source, const double maxBlurRadius) noexcept {
    source = radial_blur(source, maxBlurRadius);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_RADIAL_BLUR_IMAGE_HPP