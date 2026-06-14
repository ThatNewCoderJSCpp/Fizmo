#ifndef FIZMO_ANIMATION_HPP
#define FIZMO_ANIMATION_HPP

#include "texture.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>

namespace fizmo {
namespace graphics {

enum class LoopMode : std::uint8_t {
    Once = 0,       // stop on last frame
    Loop,           // restart from frame 0 after the last frame
    PingPong        // reverse direction at each end
};

struct AnimationFrame {
    TextureRect rect;           // region of the spritesheet
    double      duration = 0.1; // seconds this frame is shown

    constexpr AnimationFrame() noexcept = default;
    constexpr AnimationFrame(const TextureRect& r, double dur) noexcept : rect(r), duration(dur) {}
    constexpr AnimationFrame(int x, int y, unsigned int w, unsigned int h, double dur) noexcept : rect(x, y, w, h), duration(dur) {}
};

class Animation {
private:
    std::string                  m_name;
    std::vector<AnimationFrame>  m_frames;
    LoopMode                     m_loop = LoopMode::Loop;

public:
    Animation() noexcept = default;
    explicit Animation(std::string name, LoopMode loop = LoopMode::Loop) noexcept : m_name(std::move(name)), m_loop(loop) {}
    Animation(std::string name, std::vector<AnimationFrame> frames, LoopMode loop = LoopMode::Loop) noexcept : m_name(std::move(name)), m_frames(std::move(frames)), m_loop(loop) {}

    const std::string& name() const noexcept { return m_name; }
    void set_name(std::string n) noexcept { m_name = std::move(n); }

    LoopMode loop_mode() const noexcept { return m_loop; }
    void set_loop_mode(LoopMode m) noexcept { m_loop = m; }

    std::size_t frame_count() const noexcept { return m_frames.size(); }
    bool empty() const noexcept { return m_frames.empty(); }

    const AnimationFrame& frame(std::size_t i) const noexcept {
        assert(i < m_frames.size());
        return m_frames[i];
    }

    const std::vector<AnimationFrame>& frames() const noexcept { return m_frames; }

    void add_frame(const AnimationFrame& f) { m_frames.push_back(f); }
    void add_frame(AnimationFrame&& f)      { m_frames.push_back(std::move(f)); }

    void add_frame(const TextureRect& r, double dur) {
        m_frames.emplace_back(r, dur);
    }

    void add_frame(int x, int y, unsigned int w, unsigned int h, double dur) {
        m_frames.emplace_back(x, y, w, h, dur);
    }

    void reserve(std::size_t n) { m_frames.reserve(n); }
    void clear() noexcept { m_frames.clear(); }

    double total_duration() const noexcept {
        double sum = 0.0;
        for (const auto& f : m_frames) sum += f.duration;
        return sum;
    }

    static Animation from_grid(
        std::string name,
        unsigned int cell_w, unsigned int cell_h,
        unsigned int columns, unsigned int count,
        double frame_duration,
        int start_x = 0, int start_y = 0,
        LoopMode loop = LoopMode::Loop
    ) {
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
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_ANIMATION_HPP