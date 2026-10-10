#include "fizmo_library.hpp"
#include "input_manager.hpp"

namespace fizmo {
namespace windows {

void InputManager::connect(WindowEventHandler& handler) noexcept {
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

void InputManager::disconnect() noexcept {
    if (!m_handler) return;
    for (std::uint64_t id : m_listener_ids) m_handler->remove_event_listener(id);
    m_listener_ids.clear();
    m_handler = nullptr;
}

void InputManager::end_frame() noexcept {
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

double InputManager::key_hold_duration(const std::string& name) const noexcept {
    const input::Key logical = input::key_from_name(name);
    if (!is_logical_key_down(logical)) return 0.0;
    for (std::size_t i = 1; i < kKeys; ++i) if (m_down.test(i) && m_logical_of[i] == logical) return seconds_since(m_press_time[i]);
    return 0.0;
}

void InputManager::reset() noexcept {
    m_down.reset();
    m_logical_down.reset();
    m_mouse_down.reset();
    m_touches.clear();
    m_composition.clear();
    m_mods = input::Modifiers::None;
    m_last_release.fill(TimePoint{});
    end_frame();
}

bool InputManager::physical_of_name(const std::string& name, bool (InputManager::*query)(input::Key) const noexcept) const noexcept {
    const input::Key logical = input::key_from_name(name);
    if (logical == input::Key::Unknown) return false;
    for (std::size_t i = 1; i < kKeys; ++i) if (m_logical_of[i] == logical && (this->*query)(static_cast<input::Key>(i))) return true;
    return (this->*query)(logical);
}

auto InputManager::resolve(input::Key k, const WindowEvent& e) noexcept -> input::Key {
    if (k != input::Key::Unknown) return k;
    if (e.physical_key != input::Key::Unknown) return e.physical_key;
    if (e.logical_key != input::Key::Unknown) return e.logical_key;
    return input::key_from_name(e.key_name);
}

void InputManager::on_key_press(const WindowEvent& e) {
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

void InputManager::on_key_release(const WindowEvent& e) {
    m_mods = e.mods;
    const std::size_t p = index(resolve(e.physical_key, e));
    if (p == 0 || !m_down.test(p)) return;
    m_down.reset(p);
    m_just_released.set(p);
    m_last_release[p] = Clock::now();
    release_logical(m_logical_of[p]);
    m_logical_of[p] = input::Key::Unknown;
}

void InputManager::press_logical(input::Key logical, std::size_t physical) {
    m_logical_of[physical] = logical;
    const std::size_t l = index(logical);
    if (l == 0) return;
    if (m_logical_count[l]++ == 0) {
        m_logical_down.set(l);
        m_logical_just_pressed.set(l);
    }
}

void InputManager::release_logical(input::Key logical) {
    const std::size_t l = index(logical);
    if (l == 0 || m_logical_count[l] == 0) return;
    if (--m_logical_count[l] == 0) {
        m_logical_down.reset(l);
        m_logical_just_released.set(l);
    }
}

void InputManager::on_mouse_click(const WindowEvent& e) {
    m_mouse_x = static_cast<int>(e.x);
    m_mouse_y = static_cast<int>(e.y);
    if (e.button >= kButtons) return;
    if (!m_mouse_down.test(e.button)) {
        m_mouse_down.set(e.button);
        m_mouse_just_pressed.set(e.button);
        m_mouse_press_time[e.button] = Clock::now();
    }
}

void InputManager::on_mouse_release(const WindowEvent& e) {
    m_mouse_x = static_cast<int>(e.x);
    m_mouse_y = static_cast<int>(e.y);
    if (e.button >= kButtons) return;
    if (m_mouse_down.test(e.button)) {
        m_mouse_down.reset(e.button);
        m_mouse_just_released.set(e.button);
    }
}

void InputManager::on_mouse_move(const WindowEvent& e) {
    m_mouse_x = static_cast<int>(e.x);
    m_mouse_y = static_cast<int>(e.y);
    m_mouse_dx += e.dx;
    m_mouse_dy += e.dy;
}

void InputManager::on_mouse_scroll(const WindowEvent& e) {
    m_scroll_delta += e.scroll_delta;
    m_scroll_x += e.scroll_x;
    m_scroll_y += e.scroll_y != 0.0f ? e.scroll_y : static_cast<float>(e.scroll_delta);
}

void InputManager::on_mouse_double_click(const WindowEvent& e) {
    m_mouse_x = static_cast<int>(e.x);
    m_mouse_y = static_cast<int>(e.y);
    if (e.button < kButtons) m_mouse_double.set(e.button);
}

void InputManager::on_touch(const WindowEvent& e, int phase) {
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

} // namespace windows
} // namespace fizmo
