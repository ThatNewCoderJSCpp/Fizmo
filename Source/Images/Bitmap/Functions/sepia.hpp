#ifndef FIZMO_IMAGE_FUNCTIONS_SEPIA_HPP
#define FIZMO_IMAGE_FUNCTIONS_SEPIA_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

BitmapImage apply_sepia(const BitmapImage& source, const double intensity_percent = 100.0) noexcept;

inline void apply_sepia_original(BitmapImage& source, const double intensity_percent = 100.0) noexcept {
    source = apply_sepia(source, intensity_percent);
}

BitmapImage apply_sepia(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double intensity_percent = 100.0
) noexcept;

inline void apply_sepia_original(
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
) noexcept;

inline void apply_custom_sepia_original(
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
) noexcept;

inline void apply_custom_sepia_original(
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