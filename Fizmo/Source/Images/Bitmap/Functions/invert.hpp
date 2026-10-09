#ifndef FIZMO_IMAGE_FUNCTIONS_INVERT_HPP
#define FIZMO_IMAGE_FUNCTIONS_INVERT_HPP

#include "../image.hpp"

namespace fizmo {
namespace images {

BitmapImage invert(const BitmapImage& source) noexcept;

inline void invert_original(BitmapImage& source) noexcept {
    source = invert(source);
}

BitmapImage invert(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2
) noexcept;

inline void invert_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2
) noexcept {
    source = invert(source, x1, y1, x2, y2);
}

BitmapImage invert_partial(const BitmapImage& source, const double percentage) noexcept;

inline void invert_partial_original(BitmapImage& source, const double percentage) noexcept {
    source = invert_partial(source, percentage);
}

BitmapImage invert_partial(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const double percentage
) noexcept;

inline void invert_partial_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const double percentage
) noexcept {
    source = invert_partial(source, x1, y1, x2, y2, percentage);
}

BitmapImage invert_channels(const BitmapImage& source, const bool invert_red, const bool invert_green, const bool invert_blue) noexcept;

inline void invert_channels_original(BitmapImage& source, const bool invert_red, const bool invert_green, const bool invert_blue) noexcept {
    source = invert_channels(source, invert_red, invert_green, invert_blue);
}

BitmapImage invert_channels(
    const BitmapImage& source, 
    unsigned int x1, unsigned int y1, 
    unsigned int x2, unsigned int y2,
    const bool invert_red, const bool invert_green, const bool invert_blue
) noexcept;

inline void invert_channels_original(
    BitmapImage& source, 
    const unsigned int x1, const unsigned int y1, 
    const unsigned int x2, const unsigned int y2, 
    const bool invert_red, const bool invert_green, const bool invert_blue
) noexcept {
    source = invert_channels(source, x1, y1, x2, y2, invert_red, invert_green, invert_blue);
}

BitmapImage invert_channels_partial(
    const BitmapImage& source,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept;

inline void invert_channels_partial_original(
    BitmapImage& source,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept {
    source = invert_channels_partial(source, red_percentage, green_percentage, blue_percentage);
}

BitmapImage invert_channels_partial(
    const BitmapImage& source,
    unsigned int x1, unsigned int y1,
    unsigned int x2, unsigned int y2,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept;

void invert_channels_partial_original(
    BitmapImage& source,
    const unsigned int x1, const unsigned int y1,
    const unsigned int x2, const unsigned int y2,
    const double red_percentage,
    const double green_percentage,
    const double blue_percentage
) noexcept;

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGE_FUNCTIONS_INVERT_HPP