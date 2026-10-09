#ifndef FIZMO_WINDOW_TYPES_HPP
#define FIZMO_WINDOW_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace windows {

enum class WindowMode : std::uint8_t {
    Windowed = 0,
    Borderless,
    Fullscreen
};

enum class SystemCursor : std::uint8_t {
    Arrow = 0,
    IBeam,
    Hand,
    Crosshair,
    Wait,
    Progress,
    NotAllowed,
    Move,
    ResizeHorizontal,
    ResizeVertical,
    ResizeDiagonalNWSE,
    ResizeDiagonalNESW,
    Help,
    Count
};

struct Rect {
    int          x      = 0;
    int          y      = 0;
    unsigned int width  = 0;
    unsigned int height = 0;

    bool contains(int px, int py) const noexcept {
        return px >= x && py >= y && px < x + static_cast<int>(width) && py < y + static_cast<int>(height);
    }
};

struct DisplayMode {
    unsigned int width        = 0;
    unsigned int height       = 0;
    double       refresh_rate = 0.0;
    unsigned int bits_per_pixel = 32;
};

struct MonitorInfo {
    std::string              name;
    Rect                     bounds;
    Rect                     work_area;
    DisplayMode              current;
    std::vector<DisplayMode> modes;
    float                    scale       = 1.0f;
    float                    dpi         = 96.0f;
    unsigned int             physical_width_mm  = 0;
    unsigned int             physical_height_mm = 0;
    bool                     primary     = false;
    std::uintptr_t           handle      = 0;
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_TYPES_HPP
