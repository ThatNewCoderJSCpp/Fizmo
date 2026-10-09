#ifndef FIZMO_IMAGE_FUNCTIONS_OPACITY_HPP
#define FIZMO_IMAGE_FUNCTIONS_OPACITY_HPP

#include "../image.hpp"
#include "../../../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace images {

BitmapImage adjust_opacity(const BitmapImage& source, const double opacity_percent) noexcept;

inline void adjust_opacity_original(BitmapImage& source, const double opacity_percent) noexcept {
    source = adjust_opacity(source, opacity_percent);
}

BitmapImage adjust_opacity(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1,
    unsigned int x2, unsigned int y2,
    const double opacity_percent
) noexcept;

inline void adjust_opacity_original(
    BitmapImage& source,
    const unsigned int x1, const unsigned int y1,
    const unsigned int x2, const unsigned int y2,
    const double opacity_percent
) noexcept {
    source = adjust_opacity(source, x1, y1, x2, y2, opacity_percent);
}

BitmapImage adjust_opacity(
    const BitmapImage& source,
    const double opacity_percent,
    const fizmo::graphics::Color& background_color
) noexcept;

inline void adjust_opacity_original(
    BitmapImage& source,
    const double opacity_percent,
    const fizmo::graphics::Color& background_color
) noexcept {
    source = adjust_opacity(source, opacity_percent, background_color);
}

BitmapImage adjust_opacity(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1,
    unsigned int x2, unsigned int y2,
    const double opacity_percent,
    const fizmo::graphics::Color& background_color
) noexcept;

inline void adjust_opacity_original(
    BitmapImage& source,
    const unsigned int x1, const unsigned int y1,
    const unsigned int x2, const unsigned int y2,
    const double opacity_percent,
    const fizmo::graphics::Color& background_color
) noexcept {
    source = adjust_opacity(source, x1, y1, x2, y2, opacity_percent, background_color);
}

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_OPACITY_HPP