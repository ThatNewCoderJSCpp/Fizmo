#ifndef FIZMO_ANIMATION_CONTROLLER_HPP
#define FIZMO_ANIMATION_CONTROLLER_HPP

#include "animation.hpp"
#include <functional>

namespace fizmo {
namespace graphics {

class Sprite;

enum class PlaybackState : std::uint8_t {
    Stopped = 0,
    Playing,
    Paused
};

class AnimationController {
public:
    using FrameCallback  = std::function<void(std::size_t /*frame_index*/)>;
    using FinishCallback = std::function<void()>;

private:
    const Animation*  m_animation  = nullptr;
    Sprite*           m_target     = nullptr;

    PlaybackState     m_state      = PlaybackState::Stopped;
    std::size_t       m_index      = 0;       // current frame index
    double            m_elapsed    = 0.0;     // seconds into current frame
    double            m_speed      = 1.0;     // playback rate multiplier
    int               m_direction  = 1;       // +1 forward, -1 reverse

    FrameCallback     m_on_frame_changed;
    FinishCallback    m_on_finished;

public:
    AnimationController() noexcept = default;
    explicit AnimationController(const Animation* anim, Sprite* target = nullptr) noexcept : m_animation(anim), m_target(target) {}
    const Animation* animation() const noexcept { return m_animation; }

    void set_animation(const Animation* anim) noexcept {
        m_animation = anim;
        stop();
    }

    Sprite* target() const noexcept { return m_target; }
    void set_target(Sprite* s) noexcept { m_target = s; }

    double speed() const noexcept { return m_speed; }
    void set_speed(double s) noexcept { m_speed = (s > 0.0) ? s : 0.0; }

    PlaybackState state()     const noexcept { return m_state; }
    bool is_playing()         const noexcept { return m_state == PlaybackState::Playing; }
    bool is_paused()          const noexcept { return m_state == PlaybackState::Paused; }
    bool is_stopped()         const noexcept { return m_state == PlaybackState::Stopped; }
    std::size_t frame_index() const noexcept { return m_index; }

    void play() noexcept;

    void pause() noexcept {
        if (m_state == PlaybackState::Playing) m_state = PlaybackState::Paused;
    }

    void resume() noexcept {
        if (m_state == PlaybackState::Paused) m_state = PlaybackState::Playing;
    }

    void stop() noexcept {
        m_state     = PlaybackState::Stopped;
        m_index     = 0;
        m_elapsed   = 0.0;
        m_direction = 1;
    }

    void restart() noexcept {
        stop();
        play();
    }

    void set_frame(std::size_t idx) noexcept;

    void on_frame_changed(FrameCallback cb)  { m_on_frame_changed = std::move(cb); }
    void on_finished(FinishCallback cb)      { m_on_finished = std::move(cb); }

    void update(double dt) noexcept;

    TextureRect current_rect() const noexcept {
        if (!m_animation || m_animation->empty()) return {};
        return m_animation->frame(m_index).rect;
    }

private:
    void apply_frame() noexcept;

    void advance() noexcept;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_ANIMATION_CONTROLLER_HPP