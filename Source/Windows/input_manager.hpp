#ifndef FIZMO_INPUT_MANAGER_HPP
#define FIZMO_INPUT_MANAGER_HPP

#include "window_events.hpp"
#include "../Input/keys.hpp"
#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace windows {

struct TouchPoint {
    std::int64_t       id       = 0;
    input::PointerType type     = input::PointerType::Touch;
    float              x        = 0.0f;
    float              y        = 0.0f;
    float              start_x  = 0.0f;
    float              start_y  = 0.0f;
    float              pressure = 0.0f;
};

class InputManager {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr double kDoubleTapWindow = 0.35;
    static constexpr std::size_t kKeys = input::kKeyCount;
    static constexpr std::size_t kButtons = 8;

public:
    InputManager() = default;
    ~InputManager() { disconnect(); }
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void connect(WindowEventHandler& handler) noexcept;

    void disconnect() noexcept;

    void end_frame() noexcept;

    bool is_key_down(input::Key k) const noexcept { return test(m_down, k); }
    bool is_key_just_pressed(input::Key k) const noexcept { return test(m_just_pressed, k); }
    bool is_key_just_released(input::Key k) const noexcept { return test(m_just_released, k); }
    bool is_key_double_pressed(input::Key k) const noexcept { return test(m_double_pressed, k); }
    double key_hold_duration(input::Key k) const noexcept { return is_key_down(k) ? seconds_since(m_press_time[index(k)]) : 0.0; }
    bool is_key_held(input::Key k, double min_seconds) const noexcept { return is_key_down(k) && key_hold_duration(k) >= min_seconds; }

    bool is_logical_key_down(input::Key k) const noexcept { return test(m_logical_down, k); }
    bool is_logical_key_just_pressed(input::Key k) const noexcept { return test(m_logical_just_pressed, k); }
    bool is_logical_key_just_released(input::Key k) const noexcept { return test(m_logical_just_released, k); }

    bool is_key_down(const std::string& name) const noexcept { return is_logical_key_down(input::key_from_name(name)); }
    bool is_key_just_pressed(const std::string& name) const noexcept { return is_logical_key_just_pressed(input::key_from_name(name)); }
    bool is_key_just_released(const std::string& name) const noexcept { return is_logical_key_just_released(input::key_from_name(name)); }
    bool is_key_double_pressed(const std::string& name) const noexcept { return physical_of_name(name, &InputManager::is_key_double_pressed); }
    double key_hold_duration(const std::string& name) const noexcept;
    bool is_key_held(const std::string& name, double min_seconds) const noexcept { return is_key_down(name) && key_hold_duration(name) >= min_seconds; }

    input::Modifiers modifiers() const noexcept { return m_mods; }
    bool shift_down() const noexcept { return is_key_down(input::Key::LeftShift) || is_key_down(input::Key::RightShift); }
    bool control_down() const noexcept { return is_key_down(input::Key::LeftControl) || is_key_down(input::Key::RightControl); }
    bool alt_down() const noexcept { return is_key_down(input::Key::LeftAlt) || is_key_down(input::Key::RightAlt); }
    bool meta_down() const noexcept { return is_key_down(input::Key::LeftMeta) || is_key_down(input::Key::RightMeta); }

    const std::string& text_input() const noexcept { return m_text; }
    const std::string& composition() const noexcept { return m_composition; }
    int composition_cursor() const noexcept { return m_composition_cursor; }

    int mouse_x() const noexcept { return m_mouse_x; }
    int mouse_y() const noexcept { return m_mouse_y; }
    bool is_mouse_button_down(unsigned int btn) const noexcept { return btn < kButtons && m_mouse_down.test(btn); }
    bool is_mouse_button_just_pressed(unsigned int btn) const noexcept { return btn < kButtons && m_mouse_just_pressed.test(btn); }
    bool is_mouse_button_just_released(unsigned int btn) const noexcept { return btn < kButtons && m_mouse_just_released.test(btn); }
    bool is_mouse_double_clicked(unsigned int btn) const noexcept { return btn < kButtons && m_mouse_double.test(btn); }
    bool is_mouse_button_down(input::MouseButton b) const noexcept { return is_mouse_button_down(static_cast<unsigned int>(b)); }
    bool is_mouse_button_just_pressed(input::MouseButton b) const noexcept { return is_mouse_button_just_pressed(static_cast<unsigned int>(b)); }
    bool is_mouse_button_just_released(input::MouseButton b) const noexcept { return is_mouse_button_just_released(static_cast<unsigned int>(b)); }
    bool is_mouse_double_clicked(input::MouseButton b) const noexcept { return is_mouse_double_clicked(static_cast<unsigned int>(b)); }

    double mouse_hold_duration(unsigned int btn) const noexcept { return is_mouse_button_down(btn) ? seconds_since(m_mouse_press_time[btn]) : 0.0; }
    bool is_mouse_held(unsigned int btn, double min_seconds) const noexcept { return is_mouse_button_down(btn) && mouse_hold_duration(btn) >= min_seconds; }

    int scroll_delta()  const noexcept { return m_scroll_delta; }
    float scroll_x() const noexcept { return m_scroll_x; }
    float scroll_y() const noexcept { return m_scroll_y; }
    int mouse_delta_x() const noexcept { return m_mouse_dx; }
    int mouse_delta_y() const noexcept { return m_mouse_dy; }

    const std::vector<TouchPoint>& touches() const noexcept { return m_touches; }
    const std::vector<input::GestureData>& gestures() const noexcept { return m_gestures; }
    const std::vector<std::string>& dropped_files() const noexcept { return m_dropped; }

    void reset() noexcept;

private:
    template <typename F>
    void listen(WindowEventType type, F&& fn) { m_listener_ids.push_back(m_handler->add_event_listener(type, std::forward<F>(fn))); }

    static double seconds_since(TimePoint t) noexcept { return std::chrono::duration<double>(Clock::now() - t).count(); }
    static std::size_t index(input::Key k) noexcept { const std::size_t i = static_cast<std::size_t>(k); return i < kKeys ? i : 0; }
    static bool test(const std::bitset<kKeys>& set, input::Key k) noexcept { const std::size_t i = index(k); return i != 0 && set.test(i); }

    bool physical_of_name(const std::string& name, bool (InputManager::*query)(input::Key) const noexcept) const noexcept;

    static input::Key resolve(input::Key k, const WindowEvent& e) noexcept;

    void on_key_press(const WindowEvent& e);

    void on_key_release(const WindowEvent& e);

    void press_logical(input::Key logical, std::size_t physical);

    void release_logical(input::Key logical);

    void on_mouse_click(const WindowEvent& e);

    void on_mouse_release(const WindowEvent& e);

    void on_mouse_move(const WindowEvent& e);

    void on_mouse_scroll(const WindowEvent& e);

    void on_mouse_double_click(const WindowEvent& e);

    void on_touch(const WindowEvent& e, int phase);

private:
    WindowEventHandler*        m_handler = nullptr;
    std::vector<std::uint64_t> m_listener_ids;

    std::bitset<kKeys> m_down;
    std::bitset<kKeys> m_just_pressed;
    std::bitset<kKeys> m_just_released;
    std::bitset<kKeys> m_double_pressed;
    std::bitset<kKeys> m_logical_down;
    std::bitset<kKeys> m_logical_just_pressed;
    std::bitset<kKeys> m_logical_just_released;
    std::array<input::Key, kKeys>    m_logical_of{};
    std::array<std::uint8_t, kKeys>  m_logical_count{};
    std::array<TimePoint, kKeys>     m_press_time{};
    std::array<TimePoint, kKeys>     m_last_release{};
    input::Modifiers                 m_mods = input::Modifiers::None;

    std::string m_text;
    std::string m_composition;
    int         m_composition_cursor = 0;

    int m_mouse_x = 0;
    int m_mouse_y = 0;
    std::bitset<kButtons>            m_mouse_down;
    std::bitset<kButtons>            m_mouse_just_pressed;
    std::bitset<kButtons>            m_mouse_just_released;
    std::bitset<kButtons>            m_mouse_double;
    std::array<TimePoint, kButtons>  m_mouse_press_time{};

    int   m_scroll_delta = 0;
    float m_scroll_x = 0.0f;
    float m_scroll_y = 0.0f;
    int   m_mouse_dx = 0;
    int   m_mouse_dy = 0;

    std::vector<TouchPoint>         m_touches;
    std::vector<input::GestureData> m_gestures;
    std::vector<std::string>        m_dropped;
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_INPUT_MANAGER_HPP
