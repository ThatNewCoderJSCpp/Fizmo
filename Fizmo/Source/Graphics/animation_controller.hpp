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

    void play() noexcept {
        if (!m_animation || m_animation->empty()) return;

        if (m_state == PlaybackState::Stopped) {
            m_index     = 0;
            m_elapsed   = 0.0;
            m_direction = 1;
            apply_frame();
        }

        m_state = PlaybackState::Playing;
    }

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

    void set_frame(std::size_t idx) noexcept {
        if (!m_animation || m_animation->empty()) return;
        m_index   = (idx < m_animation->frame_count()) ? idx : m_animation->frame_count() - 1;
        m_elapsed = 0.0;
        apply_frame();
    }

    void on_frame_changed(FrameCallback cb)  { m_on_frame_changed = std::move(cb); }
    void on_finished(FinishCallback cb)      { m_on_finished = std::move(cb); }

    void update(double dt) noexcept {
        if (m_state != PlaybackState::Playing) return;
        if (!m_animation || m_animation->empty()) return;
        if (m_speed <= 0.0) return;
        m_elapsed += dt * m_speed;
        const double frame_dur = m_animation->frame(m_index).duration;

        while (m_elapsed >= frame_dur) {
            m_elapsed -= frame_dur;
            advance();

            if (m_state != PlaybackState::Playing) {
                m_elapsed = 0.0;
                return;
            }

            if (m_animation->frame(m_index).duration <= 0.0) {
                m_elapsed = 0.0;
                return;
            }
        }
    }

    TextureRect current_rect() const noexcept {
        if (!m_animation || m_animation->empty()) return {};
        return m_animation->frame(m_index).rect;
    }

private:
    inline void apply_frame() noexcept;

    void advance() noexcept {
        const std::size_t count = m_animation->frame_count();
        if (count <= 1) return;

        switch (m_animation->loop_mode()) {
            case LoopMode::Once: {
                if (m_index + 1 < count) {
                    ++m_index;
                    apply_frame();
                } else {
                    m_state = PlaybackState::Stopped;
                    if (m_on_finished) m_on_finished();
                }
                break;
            }
            case LoopMode::Loop: {
                m_index = (m_index + 1) % count;
                apply_frame();
                break;
            }
            case LoopMode::PingPong: {
                auto next = static_cast<int>(m_index) + m_direction;
                
                if (next < 0) {
                    m_direction = 1;
                    next = 1;
                } else if (static_cast<std::size_t>(next) >= count) {
                    m_direction = -1;
                    next = static_cast<int>(count) - 2;
                }

                m_index = static_cast<std::size_t>(std::max(next, 0));
                apply_frame();
                break;
            }
        }
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_ANIMATION_CONTROLLER_HPP