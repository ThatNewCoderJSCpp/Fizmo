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

    void connect(WindowEventHandler& handler) noexcept {
        disconnect();
        m_handler = &handler;
        listen(WindowEventType::KeyPress,         [this](const WindowEvent& e) { on_key_press(e); });
        listen(WindowEventType::KeyRelease,       [this](const WindowEvent& e) { on_key_release(e); });
        listen(WindowEventType::MouseClick,       [this](const WindowEvent& e) { on_mouse_click(e); });
        listen(WindowEventType::MouseRelease,     [this](const WindowEvent& e) { on_mouse_release(e); });
        listen(WindowEventType::MouseMove,        [this](const WindowEvent& e) { on_mouse_move(e); });
        listen(WindowEventType::MouseScroll,      [this](const WindowEvent& e) { on_mouse_scroll(e); });
        listen(WindowEventType::MouseDoubleClick, [this](const WindowEvent& e) { on_mouse_double_click(e); });
        listen(WindowEventType::WindowBlur,       [this](const WindowEvent&) { reset(); });
        listen(WindowEventType::TextInput,        [this](const WindowEvent& e) { m_text += e.text; });
        listen(WindowEventType::TextEditing,      [this](const WindowEvent& e) { m_composition = e.text; m_composition_cursor = e.text_cursor; });
        listen(WindowEventType::TouchDown,        [this](const WindowEvent& e) { on_touch(e, 0); });
        listen(WindowEventType::TouchMove,        [this](const WindowEvent& e) { on_touch(e, 1); });
        listen(WindowEventType::TouchUp,          [this](const WindowEvent& e) { on_touch(e, 2); });
        listen(WindowEventType::TouchCancel,      [this](const WindowEvent& e) { on_touch(e, 2); });
        listen(WindowEventType::PenDown,          [this](const WindowEvent& e) { on_touch(e, 0); });
        listen(WindowEventType::PenMove,          [this](const WindowEvent& e) { if (e.in_contact) on_touch(e, 1); });
        listen(WindowEventType::PenUp,            [this](const WindowEvent& e) { on_touch(e, 2); });
        listen(WindowEventType::Gesture,          [this](const WindowEvent& e) { m_gestures.push_back(e.gesture); });
        listen(WindowEventType::FilesDropped,     [this](const WindowEvent& e) { m_dropped.insert(m_dropped.end(), e.paths.begin(), e.paths.end()); });
    }

    void disconnect() noexcept {
        if (!m_handler) return;
        for (std::uint64_t id : m_listener_ids) m_handler->remove_event_listener(id);
        m_listener_ids.clear();
        m_handler = nullptr;
    }

    void end_frame() noexcept {
        m_just_pressed.reset();
        m_just_released.reset();
        m_double_pressed.reset();
        m_logical_just_pressed.reset();
        m_logical_just_released.reset();
        m_mouse_just_pressed.reset();
        m_mouse_just_released.reset();
        m_mouse_double.reset();
        m_scroll_delta = 0;
        m_scroll_x = 0.0f;
        m_scroll_y = 0.0f;
        m_mouse_dx = 0;
        m_mouse_dy = 0;
        m_text.clear();
        m_gestures.clear();
        m_dropped.clear();
    }

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
    double key_hold_duration(const std::string& name) const noexcept {
        const input::Key logical = input::key_from_name(name);
        if (!is_logical_key_down(logical)) return 0.0;
        for (std::size_t i = 1; i < kKeys; ++i) if (m_down.test(i) && m_logical_of[i] == logical) return seconds_since(m_press_time[i]);
        return 0.0;
    }
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

    void reset() noexcept {
        m_down.reset();
        m_logical_down.reset();
        m_mouse_down.reset();
        m_touches.clear();
        m_composition.clear();
        m_mods = input::Modifiers::None;
        m_last_release.fill(TimePoint{});
        end_frame();
    }

private:
    template <typename F>
    void listen(WindowEventType type, F&& fn) { m_listener_ids.push_back(m_handler->add_event_listener(type, std::forward<F>(fn))); }

    static double seconds_since(TimePoint t) noexcept { return std::chrono::duration<double>(Clock::now() - t).count(); }
    static std::size_t index(input::Key k) noexcept { const std::size_t i = static_cast<std::size_t>(k); return i < kKeys ? i : 0; }
    static bool test(const std::bitset<kKeys>& set, input::Key k) noexcept { const std::size_t i = index(k); return i != 0 && set.test(i); }

    bool physical_of_name(const std::string& name, bool (InputManager::*query)(input::Key) const noexcept) const noexcept {
        const input::Key logical = input::key_from_name(name);
        if (logical == input::Key::Unknown) return false;
        for (std::size_t i = 1; i < kKeys; ++i) if (m_logical_of[i] == logical && (this->*query)(static_cast<input::Key>(i))) return true;
        return (this->*query)(logical);
    }

    static input::Key resolve(input::Key k, const WindowEvent& e) noexcept {
        if (k != input::Key::Unknown) return k;
        if (e.physical_key != input::Key::Unknown) return e.physical_key;
        if (e.logical_key != input::Key::Unknown) return e.logical_key;
        return input::key_from_name(e.key_name);
    }

    void on_key_press(const WindowEvent& e) {
        m_mods = e.mods;
        const input::Key physical = resolve(e.physical_key, e);
        const input::Key logical = resolve(e.logical_key, e);
        const std::size_t p = index(physical);
        if (p == 0) return;
        const TimePoint now = Clock::now();

        if (m_down.test(p)) {
            if (m_logical_of[p] != logical) {
                release_logical(m_logical_of[p]);
                press_logical(logical, p);
            }
            return;
        }

        m_down.set(p);
        m_just_pressed.set(p);
        m_press_time[p] = now;

        if (m_last_release[p] != TimePoint{} && std::chrono::duration<double>(now - m_last_release[p]).count() <= kDoubleTapWindow) {
            m_double_pressed.set(p);
            m_last_release[p] = TimePoint{};
        }

        press_logical(logical, p);
    }

    void on_key_release(const WindowEvent& e) {
        m_mods = e.mods;
        const std::size_t p = index(resolve(e.physical_key, e));
        if (p == 0 || !m_down.test(p)) return;
        m_down.reset(p);
        m_just_released.set(p);
        m_last_release[p] = Clock::now();
        release_logical(m_logical_of[p]);
        m_logical_of[p] = input::Key::Unknown;
    }

    void press_logical(input::Key logical, std::size_t physical) {
        m_logical_of[physical] = logical;
        const std::size_t l = index(logical);
        if (l == 0) return;
        if (m_logical_count[l]++ == 0) {
            m_logical_down.set(l);
            m_logical_just_pressed.set(l);
        }
    }

    void release_logical(input::Key logical) {
        const std::size_t l = index(logical);
        if (l == 0 || m_logical_count[l] == 0) return;
        if (--m_logical_count[l] == 0) {
            m_logical_down.reset(l);
            m_logical_just_released.set(l);
        }
    }

    void on_mouse_click(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);
        if (e.button >= kButtons) return;
        if (!m_mouse_down.test(e.button)) {
            m_mouse_down.set(e.button);
            m_mouse_just_pressed.set(e.button);
            m_mouse_press_time[e.button] = Clock::now();
        }
    }

    void on_mouse_release(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);
        if (e.button >= kButtons) return;
        if (m_mouse_down.test(e.button)) {
            m_mouse_down.reset(e.button);
            m_mouse_just_released.set(e.button);
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
        m_scroll_x += e.scroll_x;
        m_scroll_y += e.scroll_y != 0.0f ? e.scroll_y : static_cast<float>(e.scroll_delta);
    }

    void on_mouse_double_click(const WindowEvent& e) {
        m_mouse_x = static_cast<int>(e.x);
        m_mouse_y = static_cast<int>(e.y);
        if (e.button < kButtons) m_mouse_double.set(e.button);
    }

    void on_touch(const WindowEvent& e, int phase) {
        auto it = m_touches.begin();
        while (it != m_touches.end() && it->id != e.pointer_id) ++it;

        if (phase == 2) {
            if (it != m_touches.end()) m_touches.erase(it);
            return;
        }

        if (it == m_touches.end()) {
            TouchPoint t;
            t.id = e.pointer_id;
            t.type = e.pointer;
            t.start_x = e.fx;
            t.start_y = e.fy;
            m_touches.push_back(t);
            it = m_touches.end() - 1;
        }

        it->x = e.fx;
        it->y = e.fy;
        it->pressure = e.pressure;
    }

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
