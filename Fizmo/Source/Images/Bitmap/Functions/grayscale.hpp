#ifndef FIZMO_IMAGE_FUNCTIONS_GRAYSCALE_IMAGE_HPP
#define FIZMO_IMAGE_FUNCTIONS_GRAYSCALE_IMAGE_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_grayscale(const BitmapImage& source, double percentage) noexcept;

inline BitmapImage adjust_greyscale(const BitmapImage& source, const double percentage) noexcept { 
    return adjust_grayscale(source, percentage); 
}

inline void adjust_grayscale_original(BitmapImage& source, const double percentage) noexcept { 
    source = adjust_grayscale(source, percentage); 
}

inline void adjust_greyscale_original(BitmapImage& source, const double percentage) noexcept { 
    source = adjust_grayscale(source, percentage); 
}

BitmapImage adjust_grayscale(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    double percentage
) noexcept;

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