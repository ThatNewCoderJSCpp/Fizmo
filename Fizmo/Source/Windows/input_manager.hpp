#ifndef FIZMO_INPUT_MANAGER_HPP
#define FIZMO_INPUT_MANAGER_HPP

#include "window_events.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cstdint>

namespace fizmo {
namespace windows {

class InputManager {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr double kDoubleTapWindow = 0.35;  // seconds

public:
    InputManager() = default;
    ~InputManager() { disconnect(); }
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void connect(WindowEventHandler& handler) noexcept {
        disconnect();
        m_handler = &handler;

        m_lid_key_press = handler.add_event_listener(
            WindowEventType::KeyPress,
            [this](const WindowEvent& e) { on_key_press(e); }
        );
        m_lid_key_release = handler.add_event_listener(
            WindowEventType::KeyRelease,
            [this](const WindowEvent& e) { on_key_release(e); }
        );
        m_lid_mouse_click = handler.add_event_listener(
            WindowEventType::MouseClick,
            [this](const WindowEvent& e) { on_mouse_click(e); }
        );
        m_lid_mouse_release = handler.add_event_listener(
            WindowEventType::MouseRelease,
            [this](const WindowEvent& e) { on_mouse_release(e); }
        );
        m_lid_mouse_move = handler.add_event_listener(
            WindowEventType::MouseMove,
            [this](const WindowEvent& e) { on_mouse_move(e); }
        );
        m_lid_mouse_scroll = handler.add_event_listener(
            WindowEventType::MouseScroll,
            [this](const WindowEvent& e) { on_mouse_scroll(e); }
        );
        m_lid_mouse_dblclick = handler.add_event_listener(
            WindowEventType::MouseDoubleClick,
            [this](const WindowEvent& e) { on_mouse_double_click(e); }
        );
        m_lid_focus = handler.add_event_listener(
            WindowEventType::WindowBlur,
            [this](const WindowEvent&) { reset(); }
        );
    }

    void disconnect() noexcept {
        if (!m_handler) return;
        m_handler->remove_event_listener(m_lid_key_press);
        m_handler->remove_event_listener(m_lid_key_release);
        m_handler->remove_event_listener(m_lid_mouse_click);
        m_handler->remove_event_listener(m_lid_mouse_release);
        m_handler->remove_event_listener(m_lid_mouse_move);
        m_handler->remove_event_listener(m_lid_mouse_scroll);
        m_handler->remove_event_listener(m_lid_mouse_dblclick);
        m_handler->remove_event_listener(m_lid_focus);
        m_handler = nullptr;
    }

    void end_frame() noexcept {
        m_just_pressed.clear();
        m_just_released.clear();
        m_double_pressed.clear();
        m_mouse_just_pressed.clear();
        m_mouse_just_released.clear();
        m_mouse_just_double_clicked.clear();
        m_scroll_delta = 0;
        m_mouse_dx = 0;
        m_mouse_dy = 0;
    }

    bool is_key_down(const std::string& name) const noexcept { return m_keys_down.count(name) > 0; }
    bool is_key_just_pressed(const std::string& name) const noexcept { return m_just_pressed.count(name) > 0; }
    bool is_key_just_released(const std::string& name) const noexcept { return m_just_released.count(name) > 0; }

    double key_hold_duration(const std::string& name) const noexcept {
        auto it = m_key_press_time.find(name);
        if (it == m_key_press_time.end()) return 0.0;
        return seconds_since(it->second);
    }

    bool is_key_held(const std::string& name, double min_seconds) const noexcept { return key_hold_duration(name) >= min_seconds; }
    bool is_key_double_pressed(const std::string& name) const noexcept { return m_double_pressed.count(name) > 0; }
    int mouse_x() const noexcept { return m_mouse_x; }
    int mouse_y() const noexcept { return m_mouse_y; }
    bool is_mouse_button_down(unsigned int btn) const noexcept { return m_mouse_down.count(btn) > 0; }
    bool is_mouse_button_just_pressed(unsigned int btn) const noexcept { return m_mouse_just_pressed.count(btn) > 0; }
    bool is_mouse_button_just_released(unsigned int btn) const noexcept { return m_mouse_just_released.count(btn) > 0; }

    double mouse_hold_duration(unsigned int btn) const noexcept {
        auto it = m_mouse_press_time.find(btn);
        if (it == m_mouse_press_time.end()) return 0.0;
        return seconds_since(it->second);
    }

    bool is_mouse_held(unsigned int btn, double min_seconds) const noexcept { return mouse_hold_duration(btn) >= min_seconds; }
    bool is_mouse_double_clicked(unsigned int btn) const noexcept { return m_mouse_just_double_clicked.count(btn) > 0; }
    int scroll_delta()  const noexcept { return m_scroll_delta; }
    int mouse_delta_x() const noexcept { return m_mouse_dx; }
    int mouse_delta_y() const noexcept { return m_mouse_dy; }

    void reset() noexcept {
        m_keys_down.clear();
        m_just_pressed.clear();
        m_just_released.clear();
        m_double_pressed.clear();
        m_key_press_time.clear();
        m_key_last_release.clear();
        m_mouse_down.clear();
        m_mouse_just_pressed.clear();
        m_mouse_just_released.clear();
        m_mouse_just_double_clicked.clear();
        m_mouse_press_time.clear();
        m_scroll_delta = 0;
        m_mouse_dx = 0;
        m_mouse_dy = 0;
    }

private:
    static double seconds_since(TimePoint t) noexcept {
        return std::chrono::duration<double>(Clock::now() - t).count();
    }

    void on_key_press(const WindowEvent& e) {
        auto now = Clock::now();

        if (m_keys_down.insert(e.key_name).second) {
            m_just_pressed.insert(e.key_name);
            m_key_press_time[e.key_name] = now;
            auto it = m_key_last_release.find(e.key_name);

            if (it != m_key_last_release.end()) {
                double gap = std::chrono::duration<double>(now - it->second).count();

                if (gap <= kDoubleTapWindow) {
                    m_double_pressed.insert(e.key_name);
                    m_key_last_release.erase(it);   
                }
            }
        }
    }

    void on_key_release(const WindowEvent& e) {
        if (m_keys_down.erase(e.key_name)) {
            m_just_released.insert(e.key_name);
            m_key_press_time.erase(e.key_name);
            m_key_last_release[e.key_name] = Clock::now();
        }
    }

    void on_mouse_click(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);

        if (m_mouse_down.insert(e.button).second) {
            m_mouse_just_pressed.insert(e.button);
            m_mouse_press_time[e.button] = Clock::now();
        }
    }

    void on_mouse_release(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);

        if (m_mouse_down.erase(e.button)) {
            m_mouse_just_released.insert(e.button);
            m_mouse_press_time.erase(e.button);
        }
    }

    void on_mouse_move(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);
        m_mouse_dx += e.dx;
        m_mouse_dy += e.dy;
    }

    void on_mouse_scroll(const WindowEvent& e) {
        m_scroll_delta += e.scroll_delta;
    }

    void on_mouse_double_click(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);
        m_mouse_just_double_clicked.insert(e.button);
    }

private:
    WindowEventHandler* m_handler = nullptr;

    std::uint64_t m_lid_key_press      = 0;
    std::uint64_t m_lid_key_release    = 0;
    std::uint64_t m_lid_mouse_click    = 0;
    std::uint64_t m_lid_mouse_release  = 0;
    std::uint64_t m_lid_mouse_move     = 0;
    std::uint64_t m_lid_mouse_scroll   = 0;
    std::uint64_t m_lid_mouse_dblclick = 0;
    std::uint64_t m_lid_focus          = 0;

    std::unordered_set<std::string> m_keys_down;
    std::unordered_set<std::string> m_just_pressed;
    std::unordered_set<std::string> m_just_released;
    std::unordered_set<std::string> m_double_pressed;

    std::unordered_map<std::string, TimePoint> m_key_press_time;     
    std::unordered_map<std::string, TimePoint> m_key_last_release;   

    int m_mouse_x = 0;
    int m_mouse_y = 0;

    std::unordered_set<unsigned int> m_mouse_down;
    std::unordered_set<unsigned int> m_mouse_just_pressed;
    std::unordered_set<unsigned int> m_mouse_just_released;
    std::unordered_set<unsigned int> m_mouse_just_double_clicked;

    std::unordered_map<unsigned int, TimePoint> m_mouse_press_time;

    int m_scroll_delta = 0;
    int m_mouse_dx = 0;
    int m_mouse_dy = 0;
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_INPUT_MANAGER_HPP