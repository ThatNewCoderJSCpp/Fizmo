#include "fizmo_library.hpp"
#include "animation.hpp"

namespace fizmo {
namespace graphics {

auto Animation::from_grid(
    std::string name,
    unsigned int cell_w, unsigned int cell_h,
    unsigned int columns, unsigned int count,
    double frame_duration,
    int start_x, int start_y,
    LoopMode loop 
) -> Animation {
    Animation a(std::move(name), loop);
    a.reserve(count);

    for (unsigned int i = 0; i < count; ++i) {
        int col = static_cast<int>(i % columns);
        int row = static_cast<int>(i / columns);

        a.add_frame(
            start_x + col * static_cast<int>(cell_w),
            start_y + row * static_cast<int>(cell_h),
            cell_w, cell_h, frame_duration
        );
    }
        
    return a;
}

} // namespace graphics
} // namespace fizmo
