#include "fizmo_library.hpp"
#include "window_events.hpp"

namespace fizmo {
namespace windows {

const char* event_type_name(WindowEventType et) noexcept {
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

std::uint64_t WindowEventHandler::add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept {
    std::uint64_t id = m_next_id++;
    m_listeners[index(type)].push_back({ id, std::move(callback) });
    return id;
}

std::uint64_t WindowEventHandler::add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept {
    std::uint64_t id = hash_event_id(id_str);
    remove_event_listener(id);
    m_listeners[index(type)].push_back({ id, std::move(callback) });
    return id;
}

bool WindowEventHandler::remove_event_listener(std::uint64_t id) noexcept {
    for (auto& vec : m_listeners) {
        auto it = std::find_if(vec.begin(), vec.end(), [id](const Entry& e) { return e.id == id; });

        if (it != vec.end()) {
            vec.erase(it);
            return true;
        }
    }

    return false;
}

void WindowEventHandler::dispatch_event(const WindowEvent& event) noexcept {
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

} // namespace windows
} // namespace fizmo
