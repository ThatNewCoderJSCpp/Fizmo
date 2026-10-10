#include "fizmo_library.hpp"
#include "animation_controller.hpp"

namespace fizmo {
namespace graphics {

void AnimationController::play() noexcept {
    if (!m_animation || m_animation->empty()) return;

    if (m_state == PlaybackState::Stopped) {
        m_index     = 0;
        m_elapsed   = 0.0;
        m_direction = 1;
        apply_frame();
    }

    m_state = PlaybackState::Playing;
}

void AnimationController::set_frame(std::size_t idx) noexcept {
    if (!m_animation || m_animation->empty()) return;
    m_index   = (idx < m_animation->frame_count()) ? idx : m_animation->frame_count() - 1;
    m_elapsed = 0.0;
    apply_frame();
}

void AnimationController::update(double dt) noexcept {
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

void AnimationController::advance() noexcept {
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

} // namespace graphics
} // namespace fizmo
