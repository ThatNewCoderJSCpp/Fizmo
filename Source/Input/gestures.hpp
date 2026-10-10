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

    void centroid(float& x, float& y) const noexcept;

    float distance() const noexcept;

    float angle() const noexcept;

    static float wrap(float a) noexcept {
        const float pi = 3.14159265358979f;
        while (a > pi) a -= 2.0f * pi;
        while (a < -pi) a += 2.0f * pi;
        return a;
    }

    GestureData base(GestureType type, GesturePhase phase) const noexcept;

    void reset_multi() noexcept;

    void end_continuous(const Emit& emit, GesturePhase phase);

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

    void touch_down(std::int64_t id, float x, float y, double t, const Emit& emit);

    void touch_move(std::int64_t id, float x, float y, double t, const Emit& emit);

    void touch_up(std::int64_t id, float x, float y, double t, const Emit& emit, bool cancelled);

    void update(double t, const Emit& emit);

    void reset() noexcept {
        m_touches.clear();
        m_pan = m_pinch = m_rotate = false;
        m_multi = false;
    }
};

} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_GESTURES_HPP
