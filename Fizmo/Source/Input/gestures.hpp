#ifndef FIZMO_INPUT_GESTURES_HPP
#define FIZMO_INPUT_GESTURES_HPP

#include "input_types.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace fizmo {
namespace input {

struct GestureSettings {
    float  slop_px            = 10.0f;
    double tap_seconds        = 0.30;
    double double_tap_seconds = 0.30;
    float  double_tap_px      = 30.0f;
    double long_press_seconds = 0.50;
    float  pinch_threshold    = 0.06f;
    float  rotate_threshold   = 0.08f;
    float  swipe_velocity     = 600.0f;
    float  swipe_distance     = 50.0f;
};

class GestureRecognizer {
public:
    using Emit = std::function<void(const GestureData&)>;

private:
    struct Touch {
        std::int64_t id = 0;
        float  x = 0.0f, y = 0.0f;
        float  start_x = 0.0f, start_y = 0.0f;
        double start_time = 0.0;
    };

    GestureSettings    m_settings;
    std::vector<Touch> m_touches;
    double m_last_tap_time = -1.0;
    float  m_last_tap_x = 0.0f, m_last_tap_y = 0.0f;
    bool   m_moved = false;
    bool   m_long_fired = false;
    bool   m_multi = false;
    bool   m_pan = false, m_pinch = false, m_rotate = false;
    float  m_pan_x = 0.0f, m_pan_y = 0.0f, m_pan_start_x = 0.0f, m_pan_start_y = 0.0f;
    float  m_base_distance = 0.0f, m_base_angle = 0.0f;
    float  m_scale = 1.0f, m_rotation = 0.0f;
    float  m_vx = 0.0f, m_vy = 0.0f;
    double m_last_move_time = 0.0;
    std::chrono::steady_clock::time_point m_epoch = std::chrono::steady_clock::now();

    double now() const noexcept { return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_epoch).count(); }

    Touch* find(std::int64_t id) noexcept {
        for (Touch& t : m_touches) if (t.id == id) return &t;
        return nullptr;
    }

    void centroid(float& x, float& y) const noexcept {
        x = y = 0.0f;
        if (m_touches.empty()) return;
        for (const Touch& t : m_touches) { x += t.x; y += t.y; }
        x /= static_cast<float>(m_touches.size());
        y /= static_cast<float>(m_touches.size());
    }

    float distance() const noexcept {
        if (m_touches.size() < 2) return 0.0f;
        return std::hypot(m_touches[1].x - m_touches[0].x, m_touches[1].y - m_touches[0].y);
    }

    float angle() const noexcept {
        if (m_touches.size() < 2) return 0.0f;
        return std::atan2(m_touches[1].y - m_touches[0].y, m_touches[1].x - m_touches[0].x);
    }

    static float wrap(float a) noexcept {
        const float pi = 3.14159265358979f;
        while (a > pi) a -= 2.0f * pi;
        while (a < -pi) a += 2.0f * pi;
        return a;
    }

    GestureData base(GestureType type, GesturePhase phase) const noexcept {
        GestureData g;
        g.type = type;
        g.phase = phase;
        g.touches = static_cast<int>(m_touches.size());
        centroid(g.x, g.y);
        return g;
    }

    void reset_multi() noexcept {
        float cx = 0.0f, cy = 0.0f;
        centroid(cx, cy);
        m_pan_x = m_pan_start_x = cx;
        m_pan_y = m_pan_start_y = cy;
        m_base_distance = distance();
        m_base_angle = angle();
    }

    void end_continuous(const Emit& emit, GesturePhase phase) {
        if (m_pan) {
            GestureData g = base(GestureType::Pan, phase);
            g.x = m_pan_x;
            g.y = m_pan_y;
            g.total_dx = m_pan_x - m_pan_start_x;
            g.total_dy = m_pan_y - m_pan_start_y;
            g.velocity_x = m_vx;
            g.velocity_y = m_vy;
            emit(g);
        }
        if (m_pinch) { GestureData g = base(GestureType::Pinch, phase); g.scale = m_scale; emit(g); }
        if (m_rotate) { GestureData g = base(GestureType::Rotate, phase); g.rotation = m_rotation; emit(g); }
        m_pan = m_pinch = m_rotate = false;
    }

public:
    GestureRecognizer() = default;
    explicit GestureRecognizer(const GestureSettings& s) : m_settings(s) {}

    GestureSettings& settings() noexcept { return m_settings; }
    std::size_t active_touches() const noexcept { return m_touches.size(); }

    void touch_down(std::int64_t id, float x, float y, const Emit& emit) { touch_down(id, x, y, now(), emit); }
    void touch_move(std::int64_t id, float x, float y, const Emit& emit) { touch_move(id, x, y, now(), emit); }
    void touch_up(std::int64_t id, float x, float y, const Emit& emit) { touch_up(id, x, y, now(), emit, false); }
    void touch_cancel(std::int64_t id, const Emit& emit) { touch_up(id, 0.0f, 0.0f, now(), emit, true); }
    void update(const Emit& emit) { update(now(), emit); }

    void touch_down(std::int64_t id, float x, float y, double t, const Emit& emit) {
        if (find(id)) return;
        if (m_touches.size() >= 2) { Touch extra{ id, x, y, x, y, t }; m_touches.push_back(extra); return; }
        if (!m_touches.empty()) {
            end_continuous(emit, GesturePhase::End);
            m_multi = true;
        } else {
            m_moved = false;
            m_long_fired = false;
            m_multi = false;
            m_vx = m_vy = 0.0f;
        }
        m_touches.push_back(Touch{ id, x, y, x, y, t });
        reset_multi();
        m_scale = 1.0f;
        m_rotation = 0.0f;
        m_last_move_time = t;
    }

    void touch_move(std::int64_t id, float x, float y, double t, const Emit& emit) {
        Touch* touch = find(id);
        if (!touch) return;
        touch->x = x;
        touch->y = y;
        if (std::hypot(x - touch->start_x, y - touch->start_y) > m_settings.slop_px) m_moved = true;
        float cx = 0.0f, cy = 0.0f;
        centroid(cx, cy);
        const double dt = t - m_last_move_time;

        if (dt > 1e-4) {
            const float ivx = (cx - m_pan_x) / static_cast<float>(dt), ivy = (cy - m_pan_y) / static_cast<float>(dt);
            m_vx = m_vx * 0.4f + ivx * 0.6f;
            m_vy = m_vy * 0.4f + ivy * 0.6f;
            m_last_move_time = t;
        }

        if (m_moved) {
            GestureData g = base(GestureType::Pan, m_pan ? GesturePhase::Update : GesturePhase::Begin);
            g.dx = cx - m_pan_x;
            g.dy = cy - m_pan_y;
            g.total_dx = cx - m_pan_start_x;
            g.total_dy = cy - m_pan_start_y;
            g.velocity_x = m_vx;
            g.velocity_y = m_vy;
            m_pan = true;
            emit(g);
        }

        m_pan_x = cx;
        m_pan_y = cy;

        if (m_touches.size() == 2 && m_base_distance > 1.0f) {
            const float scale = distance() / m_base_distance;

            if (m_pinch || std::abs(scale - 1.0f) > m_settings.pinch_threshold) {
                GestureData g = base(GestureType::Pinch, m_pinch ? GesturePhase::Update : GesturePhase::Begin);
                g.scale = scale;
                g.scale_delta = m_scale > 0.0f ? scale / m_scale : 1.0f;
                m_scale = scale;
                m_pinch = true;
                emit(g);
            }

            const float rotation = wrap(angle() - m_base_angle);

            if (m_rotate || std::abs(rotation) > m_settings.rotate_threshold) {
                GestureData g = base(GestureType::Rotate, m_rotate ? GesturePhase::Update : GesturePhase::Begin);
                g.rotation = rotation;
                g.rotation_delta = wrap(rotation - m_rotation);
                m_rotation = rotation;
                m_rotate = true;
                emit(g);
            }
        }
    }

    void touch_up(std::int64_t id, float x, float y, double t, const Emit& emit, bool cancelled) {
        Touch* touch = find(id);
        if (!touch) return;
        if (!cancelled) { touch->x = x; touch->y = y; }
        const Touch released = *touch;
        const bool last = m_touches.size() == 1;

        if (cancelled) {
            end_continuous(emit, GesturePhase::Cancel);
        } else if (last) {
            const bool was_pan = m_pan;
            const float vx = m_vx, vy = m_vy;
            end_continuous(emit, GesturePhase::End);
            const float dist = std::hypot(released.x - released.start_x, released.y - released.start_y);

            if (!m_multi && was_pan && dist >= m_settings.swipe_distance && std::hypot(vx, vy) >= m_settings.swipe_velocity) {
                GestureData g = base(GestureType::Swipe, GesturePhase::End);
                g.velocity_x = vx;
                g.velocity_y = vy;
                g.total_dx = released.x - released.start_x;
                g.total_dy = released.y - released.start_y;
                g.direction = std::abs(g.total_dx) >= std::abs(g.total_dy) ? (g.total_dx < 0 ? SwipeDirection::Left : SwipeDirection::Right)
                                                                            : (g.total_dy < 0 ? SwipeDirection::Up : SwipeDirection::Down);
                emit(g);
            } else if (!m_multi && !m_moved && !m_long_fired && t - released.start_time <= m_settings.tap_seconds) {
                GestureData g = base(GestureType::Tap, GesturePhase::End);
                g.x = released.x;
                g.y = released.y;
                const bool dbl = m_last_tap_time >= 0.0 && t - m_last_tap_time <= m_settings.double_tap_seconds
                              && std::hypot(released.x - m_last_tap_x, released.y - m_last_tap_y) <= m_settings.double_tap_px;
                emit(g);

                if (dbl) {
                    g.type = GestureType::DoubleTap;
                    emit(g);
                    m_last_tap_time = -1.0;
                } else {
                    m_last_tap_time = t;
                    m_last_tap_x = released.x;
                    m_last_tap_y = released.y;
                }
            }
        } else {
            end_continuous(emit, GesturePhase::End);
        }

        for (auto it = m_touches.begin(); it != m_touches.end(); ++it) if (it->id == id) { m_touches.erase(it); break; }
        if (!m_touches.empty()) { reset_multi(); m_scale = 1.0f; m_rotation = 0.0f; }
    }

    void update(double t, const Emit& emit) {
        if (m_touches.size() != 1 || m_moved || m_long_fired || m_multi) return;
        if (t - m_touches[0].start_time < m_settings.long_press_seconds) return;
        m_long_fired = true;
        GestureData g = base(GestureType::LongPress, GesturePhase::Begin);
        emit(g);
    }

    void reset() noexcept {
        m_touches.clear();
        m_pan = m_pinch = m_rotate = false;
        m_multi = false;
    }
};

} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_GESTURES_HPP
