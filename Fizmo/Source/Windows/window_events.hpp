#ifndef FIZMO_WINDOW_EVENTS_HPP
#define FIZMO_WINDOW_EVENTS_HPP

#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <ostream>
#include <algorithm>
#include "../Basic/fizmo_defines.hpp"
#include "../Input/input_types.hpp"
#include <cstdint>

#ifdef OS_LINUX
#include "../x11_compat.hpp"
#endif 

namespace fizmo {
namespace windows {

class Window;

enum class WindowEventType {
    MouseMove = 0,
    MouseClick,
    MouseRelease,
    MouseScroll,
    MouseDoubleClick,
    KeyPress,
    KeyRelease,
    WindowResize,
    WindowClose,
    WindowFocus,
    WindowBlur, 
    WindowExpose,
    WindowRenderRequest,
    TextInput,
    TextEditing,
    TouchDown,
    TouchMove,
    TouchUp,
    TouchCancel,
    PenDown,
    PenMove,
    PenUp,
    Gesture,
    FilesDropped,
    DragEnter,
    DragOver,
    DragLeave,
    WindowMove,
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    DpiChanged,
    MonitorsChanged,
    GamepadConnected,
    GamepadDisconnected,
    GamepadButtonDown,
    GamepadButtonUp,
    GamepadAxisMotion,
    Count
};

inline const char* event_type_name(WindowEventType et) noexcept {
    switch (et) {
        case WindowEventType::MouseMove:           return "MouseMove";
        case WindowEventType::MouseClick:          return "MouseClick";
        case WindowEventType::MouseRelease:        return "MouseRelease";
        case WindowEventType::MouseScroll:         return "MouseScroll";
        case WindowEventType::MouseDoubleClick:    return "MouseDoubleClick";
        case WindowEventType::KeyPress:            return "KeyPress";
        case WindowEventType::KeyRelease:          return "KeyRelease";
        case WindowEventType::WindowResize:        return "WindowResize";
        case WindowEventType::WindowClose:         return "WindowClose";
        case WindowEventType::WindowFocus:         return "WindowFocus";
        case WindowEventType::WindowBlur:          return "WindowBlur";
        case WindowEventType::WindowExpose:        return "WindowExpose";
        case WindowEventType::WindowRenderRequest: return "WindowRenderRequest";
        case WindowEventType::TextInput:           return "TextInput";
        case WindowEventType::TextEditing:         return "TextEditing";
        case WindowEventType::TouchDown:           return "TouchDown";
        case WindowEventType::TouchMove:           return "TouchMove";
        case WindowEventType::TouchUp:             return "TouchUp";
        case WindowEventType::TouchCancel:         return "TouchCancel";
        case WindowEventType::PenDown:             return "PenDown";
        case WindowEventType::PenMove:             return "PenMove";
        case WindowEventType::PenUp:               return "PenUp";
        case WindowEventType::Gesture:             return "Gesture";
        case WindowEventType::FilesDropped:        return "FilesDropped";
        case WindowEventType::DragEnter:           return "DragEnter";
        case WindowEventType::DragOver:            return "DragOver";
        case WindowEventType::DragLeave:           return "DragLeave";
        case WindowEventType::WindowMove:          return "WindowMove";
        case WindowEventType::WindowMinimize:      return "WindowMinimize";
        case WindowEventType::WindowMaximize:      return "WindowMaximize";
        case WindowEventType::WindowRestore:       return "WindowRestore";
        case WindowEventType::DpiChanged:          return "DpiChanged";
        case WindowEventType::MonitorsChanged:     return "MonitorsChanged";
        case WindowEventType::GamepadConnected:    return "GamepadConnected";
        case WindowEventType::GamepadDisconnected: return "GamepadDisconnected";
        case WindowEventType::GamepadButtonDown:   return "GamepadButtonDown";
        case WindowEventType::GamepadButtonUp:     return "GamepadButtonUp";
        case WindowEventType::GamepadAxisMotion:   return "GamepadAxisMotion";
        default:                                   return "unhandled event type";
    }
}

inline std::ostream& operator<<(std::ostream& os, WindowEventType et) { return os << event_type_name(et); }

struct WindowEvent {
    WindowEventType type;
    unsigned int x = 0;          // Mouse x position or window width
    unsigned int y = 0;          // Mouse y position or window height
    unsigned int button = 0;     // Mouse button or key code
    unsigned int key = 0;        // Key code
    unsigned int scancode = 0; 
    std::string key_name = "";
    int scroll_delta = 0;        // positive = up/away, negative = down/toward
    int dx = 0;                  // MouseMove: pixels moved since the previous MouseMove 
    int dy = 0;

    input::Key       physical_key = input::Key::Unknown;
    input::Key       logical_key  = input::Key::Unknown;
    input::Modifiers mods         = input::Modifiers::None;
    bool             repeat       = false;

    std::string text;
    int         text_cursor    = 0;
    int         text_selection = 0;

    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    float fx       = 0.0f;
    float fy       = 0.0f;

    input::PointerType pointer      = input::PointerType::Mouse;
    std::int64_t       pointer_id   = 0;
    float              pressure     = 0.0f;
    float              tilt_x       = 0.0f;
    float              tilt_y       = 0.0f;
    float              twist        = 0.0f;
    bool               eraser       = false;
    bool               barrel       = false;
    bool               in_contact   = false;
    input::GestureData gesture;

    std::vector<std::string> paths;

    int                  gamepad        = -1;
    input::GamepadButton gamepad_button = input::GamepadButton::South;
    input::GamepadAxis   gamepad_axis   = input::GamepadAxis::LeftX;
    float                axis_value     = 0.0f;

    int   window_x  = 0;
    int   window_y  = 0;
    float dpi_scale = 1.0f;

    inline friend std::ostream& operator<<(std::ostream& os, const WindowEvent& e) {
        os << "WindowEvent[\n"
           << "    Type: " << e.type << "\n"
           << "    x: " << e.x << "\n" 
           << "    y: " << e.y << "\n"
           << "    Button: " << e.button << "\n"
           << "    Key Code: " << e.key << "\n" 
           << "    Key Name: " << e.key_name << "\n"
           << "    Scroll Delta: " << e.scroll_delta << "\n"
           << "    Mouse Delta: " << e.dx << ", " << e.dy << "\n"
           << "]";
        return os;
    }
};

inline constexpr std::uint64_t hash_event_id(const char* str, std::size_t len) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;

    for (std::size_t i = 0; i < len; ++i) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(str[i]));
        hash *= 1099511628211ULL;
    }
    
    return hash | (std::uint64_t(1) << 63);
}

inline std::uint64_t hash_event_id(const std::string& str) noexcept { return hash_event_id(str.data(), str.size()); }

class WindowEventHandler {
public:
    std::uint64_t add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept {
        std::uint64_t id = m_next_id++;
        m_listeners[index(type)].push_back({ id, std::move(callback) });
        return id;
    }

    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept {
        std::uint64_t id = hash_event_id(id_str);
        remove_event_listener(id);
        m_listeners[index(type)].push_back({ id, std::move(callback) });
        return id;
    }

    bool remove_event_listener(std::uint64_t id) noexcept {
        for (auto& vec : m_listeners) {
            auto it = std::find_if(vec.begin(), vec.end(), [id](const Entry& e) { return e.id == id; });

            if (it != vec.end()) {
                vec.erase(it);
                return true;
            }
        }

        return false;
    }

    bool remove_event_listener(const std::string& id_str) noexcept { return remove_event_listener(hash_event_id(id_str)); }
    void remove_event_listeners(WindowEventType type) noexcept { m_listeners[index(type)].clear(); }

    bool has_listeners(WindowEventType type) const noexcept { return !m_listeners[index(type)].empty(); }

    void dispatch_event(const WindowEvent& event) noexcept {
        const std::vector<Entry>& vec = m_listeners[index(event.type)];
        for (std::size_t i = 0; i < vec.size(); ++i) {
            const std::uint64_t id = vec[i].id;
            auto cb = vec[i].callback;
            if (cb) cb(event);
            if (i >= vec.size() || vec[i].id != id) {
                std::size_t j = 0;
                while (j < vec.size() && vec[j].id != id) ++j;
                i = j < vec.size() ? j : (i == 0 ? static_cast<std::size_t>(-1) : i - 1);
            }
        }
    }

private:
    struct Entry {
        std::uint64_t id;
        std::function<void(const WindowEvent&)> callback;
    };

    std::uint64_t m_next_id = 1;
    static std::size_t index(WindowEventType t) noexcept {
        const std::size_t i = static_cast<std::size_t>(t);
        return i < kTypes ? i : kTypes - 1;
    }

    static constexpr std::size_t kTypes = static_cast<std::size_t>(WindowEventType::Count) + 1;
    std::vector<Entry> m_listeners[kTypes];
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_EVENTS_HPP