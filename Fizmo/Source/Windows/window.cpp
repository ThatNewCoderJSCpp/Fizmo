#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "window.hpp"

namespace fizmo {
namespace windows {

auto Window::emit_gesture(const input::GestureData& g) noexcept -> void {
        WindowEvent e;
        e.type = WindowEventType::Gesture;
        e.gesture = g;
        e.fx = g.x;
        e.fy = g.y;
        e.x = g.x < 0.0f ? 0u : static_cast<unsigned int>(g.x);
        e.y = g.y < 0.0f ? 0u : static_cast<unsigned int>(g.y);
        e.pointer = input::PointerType::Touch;
        m_event_handler.dispatch_event(e);
    }

auto Window::feed_gestures(const WindowEvent& e) noexcept -> void {
        if (!m_gestures_enabled || e.pointer != input::PointerType::Touch) return;
        auto emit = [this](const input::GestureData& g) { emit_gesture(g); };
        try {
            switch (e.type) {
                case WindowEventType::TouchDown:   m_gestures.touch_down(e.pointer_id, e.fx, e.fy, emit); break;
                case WindowEventType::TouchMove:   m_gestures.touch_move(e.pointer_id, e.fx, e.fy, emit); break;
                case WindowEventType::TouchUp:     m_gestures.touch_up(e.pointer_id, e.fx, e.fy, emit); break;
                case WindowEventType::TouchCancel: m_gestures.touch_cancel(e.pointer_id, emit); break;
                default: break;
            }
        } catch (...) {}
    }

Window::Window(unsigned int w, unsigned int h, const std::string& t, const graphics::Color& background_color) noexcept : m_width(w), m_height(h), m_title(t), m_color(background_color) {
        try { m_impl = detail::make_window_impl(this, background_color); } catch (...) {}
    }

auto Window::poll_events() noexcept -> void {
        m_impl->poll_events();
        if (m_gestures_enabled && m_gestures.active_touches() > 0) {
            try { m_gestures.update([this](const input::GestureData& g) { emit_gesture(g); }); } catch (...) {}
        }
    }

std::vector<MonitorInfo> monitors() {
#if defined(OS_WINDOWS) || defined(OS_LINUX)
    try { return detail::platform_monitors(); } catch (...) { return {}; }
#else
    return {};
#endif
}

MonitorInfo primary_monitor() {
    const std::vector<MonitorInfo> list = monitors();
    for (const MonitorInfo& m : list) if (m.primary) return m;
    return list.empty() ? MonitorInfo{} : list.front();
}

} // namespace windows
} // namespace fizmo
