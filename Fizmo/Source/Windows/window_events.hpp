#ifndef FIZMO_WINDOW_EVENTS_HPP
#define FIZMO_WINDOW_EVENTS_HPP

#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <string>

class Window;

namespace fizmo {
namespace windows {

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
    WindowRenderRequest
};

std::ostream& operator<<(std::ostream& os, WindowEventType et) {
    switch (et) {
        case WindowEventType::MouseMove:           os << "MouseMove";           break;
        case WindowEventType::MouseClick:          os << "MouseClick";          break;
        case WindowEventType::MouseRelease:        os << "MouseRelease";        break;
        case WindowEventType::MouseScroll:         os << "MouseScroll";         break;
        case WindowEventType::MouseDoubleClick:    os << "MouseDoubleClick";    break;
        case WindowEventType::KeyPress:            os << "KeyPress";            break;
        case WindowEventType::KeyRelease:          os << "KeyRelease";          break;
        case WindowEventType::WindowResize:        os << "WindowResize";        break;
        case WindowEventType::WindowClose:         os << "WindowClose";         break;
        case WindowEventType::WindowFocus:         os << "WindowFocus";         break;
        case WindowEventType::WindowBlur:          os << "WindowBlur";          break;
        case WindowEventType::WindowExpose:        os << "WindowExpose";        break;
        case WindowEventType::WindowRenderRequest: os << "WindowRenderRequest"; break;
        default: os << "unhandled event type"; break;
    }
    return os;
}

struct WindowEvent {
    WindowEventType type;
    unsigned int x = 0;          // Mouse x position or window width
    unsigned int y = 0;          // Mouse y position or window height
    unsigned int button = 0;     // Mouse button or key code
    unsigned int key = 0;        // Key code
    std::string key_name = "";
    int scroll_delta = 0;        // positive = up/away, negative = down/toward
    
    friend std::ostream& operator<<(std::ostream& os, const WindowEvent& e) {
        os << "WindowEvent[\n"
           << "    Type: " << e.type << "\n"
           << "    x: " << e.x << "\n" 
           << "    y: " << e.y << "\n"
           << "    Button: " << e.button << "\n"
           << "    Key Code: " << e.key << "\n" 
           << "    Key Name: " << e.key_name << "\n"
           << "    Scroll Delta: " << e.scroll_delta << "\n"
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
        m_listeners[type].push_back({ id, std::move(callback) });
        return id;
    }

    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept {
        std::uint64_t id = hash_event_id(id_str);
        remove_event_listener(id);
        m_listeners[type].push_back({ id, std::move(callback) });
        return id;
    }

    bool remove_event_listener(std::uint64_t id) noexcept {
        for (auto& [type, vec] : m_listeners) {
            auto it = std::find_if(vec.begin(), vec.end(), [id](const Entry& e) { return e.id == id; });

            if (it != vec.end()) {
                vec.erase(it);
                return true;
            }
        }

        return false;
    }

    bool remove_event_listener(const std::string& id_str) noexcept { return remove_event_listener(hash_event_id(id_str)); }
    void remove_event_listeners(WindowEventType type) noexcept { m_listeners[type].clear(); }

    void dispatch_event(const WindowEvent& event) noexcept {
        auto it = m_listeners.find(event.type);
        if (it != m_listeners.end()) { for (const auto& entry : it->second) { entry.callback(event); } }
    }

private:
    struct Entry {
        std::uint64_t id;
        std::function<void(const WindowEvent&)> callback;
    };

    std::uint64_t m_next_id = 1;
    std::map<WindowEventType, std::vector<Entry>> m_listeners;
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_EVENTS_HPP