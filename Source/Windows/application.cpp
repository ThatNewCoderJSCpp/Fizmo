#include "fizmo_library.hpp"
#include "application.hpp"

namespace fizmo {
namespace windows {

bool Application::create() noexcept {
    if (!m_window.create()) return false;
    if (!m_renderer.bind()) return false;
    install_internal_events();
    m_input.connect(m_window.event_handler());
    m_gamepads.attach(m_window.event_handler());
#if defined(OS_WINDOWS)
    m_gamepads.set_window(m_window.native_handle());
#endif
    return true;
}

int Application::run() noexcept {
    if (!m_window.is_open()) { if (!create()) return -1; }
    m_running = true;
    if (m_on_startup) m_on_startup(*this);
    m_time_start = Clock::now();
    m_time_last  = m_time_start;
    m_delta       = 0.0;
    m_fps         = 0.0;
    m_fps_avg     = 0.0;
    m_frame_count = 0;
    m_next_frame = m_time_start;

    while (m_running && m_window.is_open()) {
        auto now = Clock::now();
        m_delta  = Duration(now - m_time_last).count();
        m_time_last = now;
        if (m_delta > kMaxDelta) m_delta = kMaxDelta;
        m_fps = (m_delta > 0.0) ? 1.0 / m_delta : 0.0;
            
        if (m_frame_count == 0) {
            m_fps_avg = m_fps;
        } else {
            m_fps_avg += kSmoothing * (m_fps - m_fps_avg);
        }

        ++m_frame_count;
        m_window.poll_events();
        m_gamepads.update();
        double dt = m_delta;
        if (m_on_update) m_on_update(dt);
        m_renderer.begin_frame();
        if (m_auto_clear) m_renderer.clear(m_clear_color);
        if (m_on_render) m_on_render(m_renderer);
        m_renderer.present();
        m_input.end_frame();
        limit_frame_rate();
    }

    m_running = false;
    m_input.disconnect();
    if (m_on_shutdown) m_on_shutdown();
    m_renderer.unbind();
    return 0;
}

void Application::install_internal_events() noexcept {
    m_window.add_event_listener(WindowEventType::WindowClose, [this](const WindowEvent&) { m_running = false; });
    auto forward = [this](const WindowEvent& e) { if (m_on_event) m_on_event(e); };
    for (int t = 0; t < static_cast<int>(WindowEventType::Count); ++t) {
        const WindowEventType type = static_cast<WindowEventType>(t);
        if (type == WindowEventType::WindowExpose || type == WindowEventType::WindowRenderRequest) continue;
        m_window.add_event_listener(type, forward);
    }
}

void Application::limit_frame_rate() noexcept {
    if (m_max_fps <= 0.0) return;
    const auto period = std::chrono::duration_cast<Clock::duration>(Duration(1.0 / m_max_fps));
    m_next_frame += period;
    const auto now = Clock::now();
    if (m_next_frame < now - period) { m_next_frame = now; return; }
    const auto spin_margin = std::chrono::microseconds(1500);
    if (m_next_frame - now > spin_margin) std::this_thread::sleep_until(m_next_frame - spin_margin);
    while (Clock::now() < m_next_frame) std::this_thread::yield();
}

} // namespace windows
} // namespace fizmo
