#ifndef FIZMO_WIDGETS_HPP
#define FIZMO_WIDGETS_HPP

#include "window_events.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <chrono>

namespace fizmo {
namespace windows {

class Widget {
protected:
    int m_x = 0;
    int m_y = 0;
    unsigned int m_width = 0;
    unsigned int m_height = 0;
    bool m_focused = false;
    bool m_visible = true;
    Widget* m_parent = nullptr;
    std::vector<std::unique_ptr<Widget>> m_children;
    WindowEventHandler m_event_handler;

    bool m_hovered = false;

    bool m_hover_delay_active  = false;
    bool m_hover_delay_fired   = false;
    double m_hover_delay_ms    = 0.0;
    std::chrono::steady_clock::time_point m_hover_enter_time;
    std::function<void()>                 m_hover_delay_callback;

public:
    virtual ~Widget() = default;

    virtual bool handle_event(const WindowEvent& event) {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
            if ((*it)->visible() && (*it)->handle_event(event)) { return true; }
        }
        dispatch_to_listeners(event);
        return false;
    }

    virtual bool hit_test(int px, int py) const noexcept {
        return px >= m_x && px < m_x + static_cast<int>(m_width) && py >= m_y && py < m_y + static_cast<int>(m_height);
    }

    void set_position(int x, int y) noexcept { m_x = x; m_y = y; }
    void set_size(unsigned int w, unsigned int h) noexcept { m_width = w; m_height = h; }
    void set_bounds(int x, int y, unsigned int w, unsigned int h) noexcept { m_x = x; m_y = y; m_width = w; m_height = h; }
    int x() const noexcept { return m_x; }
    int y() const noexcept { return m_y; }
    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    bool focused() const noexcept { return m_focused; }
    void set_focused(bool f) noexcept { m_focused = f; }
    bool visible() const noexcept { return m_visible; }
    void set_visible(bool v) noexcept { m_visible = v; }
    Widget* parent() const noexcept { return m_parent; }

    Widget* add_child(std::unique_ptr<Widget> child) noexcept {
        child->m_parent = this;
        m_children.push_back(std::move(child));
        return m_children.back().get();
    }

    void remove_child(Widget* child) noexcept {
        m_children.erase(
            std::remove_if(
                m_children.begin(), m_children.end(),
                [child](const std::unique_ptr<Widget>& w) { return w.get() == child; }
            ),
            m_children.end()
        );
    }

    const std::vector<std::unique_ptr<Widget>>& children() const noexcept { return m_children; }
    std::uint64_t add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_event_handler.add_event_listener(type, std::move(cb)); }
    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_event_handler.add_event_listener(id_str, type, std::move(cb)); }
    bool remove_event_listener(std::uint64_t id) noexcept { return m_event_handler.remove_event_listener(id); }
    bool remove_event_listener(const std::string& id_str) noexcept { return m_event_handler.remove_event_listener(id_str); }
    void remove_event_listeners(WindowEventType type) noexcept { m_event_handler.remove_event_listeners(type); }

public:
    std::uint64_t on_click(std::function<void(const WindowEvent&)> cb) noexcept {
        return m_event_handler.add_event_listener(
            WindowEventType::MouseClick,
            [this, cb = std::move(cb)](const WindowEvent& e) {
                if (hit_test(static_cast<int>(e.x), static_cast<int>(e.y))) { cb(e); }
            }
        );
    }

    std::uint64_t on_click(const std::string& id_str, std::function<void(const WindowEvent&)> cb) noexcept {
        return m_event_handler.add_event_listener(
            id_str, WindowEventType::MouseClick,
            [this, cb = std::move(cb)](const WindowEvent& e) {
                if (hit_test(static_cast<int>(e.x), static_cast<int>(e.y))) { cb(e); }
            }
        );
    }

    std::uint64_t on_hover(std::function<void(bool entered)> cb) noexcept {
        return m_event_handler.add_event_listener(
            WindowEventType::MouseMove,
            [this, cb = std::move(cb)](const WindowEvent& e) {
                bool inside = hit_test(static_cast<int>(e.x), static_cast<int>(e.y));

                if (inside && !m_hovered) {
                    m_hovered = true;
                    cb(true);
                } else if (!inside && m_hovered) {
                    m_hovered = false;
                    cb(false);
                }
            }
        );
    }

    std::uint64_t on_hover(const std::string& id_str, std::function<void(bool entered)> cb) noexcept {
        return m_event_handler.add_event_listener(
            id_str, WindowEventType::MouseMove,
            [this, cb = std::move(cb)](const WindowEvent& e) {
                bool inside = hit_test(static_cast<int>(e.x), static_cast<int>(e.y));

                if (inside && !m_hovered) {
                    m_hovered = true;
                    cb(true);
                } else if (!inside && m_hovered) {
                    m_hovered = false;
                    cb(false);
                }
            }
        );
    }

    std::uint64_t on_hover(std::function<void()> cb, double delay_ms) noexcept {
        m_hover_delay_callback = std::move(cb);
        m_hover_delay_ms       = delay_ms;
        m_hover_delay_active   = false;
        m_hover_delay_fired    = false;

        return m_event_handler.add_event_listener(
            WindowEventType::MouseMove,
            [this](const WindowEvent& e) {
                bool inside = hit_test(static_cast<int>(e.x), static_cast<int>(e.y));
                handle_hover_delay(inside);
            }
        );
    }

    std::uint64_t on_hover(const std::string& id_str, std::function<void()> cb, double delay_ms) noexcept {
        m_hover_delay_callback = std::move(cb);
        m_hover_delay_ms       = delay_ms;
        m_hover_delay_active   = false;
        m_hover_delay_fired    = false;

        return m_event_handler.add_event_listener(
            id_str, WindowEventType::MouseMove,
            [this](const WindowEvent& e) {
                bool inside = hit_test(static_cast<int>(e.x), static_cast<int>(e.y));
                handle_hover_delay(inside);
            }
        );
    }

    void check_hover_delay() noexcept {
        if (m_hover_delay_active && !m_hover_delay_fired) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - m_hover_enter_time).count();

            if (elapsed >= m_hover_delay_ms) {
                m_hover_delay_fired = true;
                if (m_hover_delay_callback) m_hover_delay_callback();
            }
        }
    }

private:
    void handle_hover_delay(bool inside) noexcept {
        if (inside && !m_hover_delay_active) {
            m_hover_delay_active = true;
            m_hover_delay_fired  = false;
            m_hover_enter_time   = std::chrono::steady_clock::now();
        } else if (!inside && m_hover_delay_active) {
            m_hover_delay_active = false;
            m_hover_delay_fired  = false;
        }

        if (m_hover_delay_active && !m_hover_delay_fired) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - m_hover_enter_time).count();

            if (elapsed >= m_hover_delay_ms) {
                m_hover_delay_fired = true;
                if (m_hover_delay_callback) m_hover_delay_callback();
            }
        }
    }

protected:
    void dispatch_to_listeners(const WindowEvent& event) noexcept { m_event_handler.dispatch_event(event); }
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WIDGETS_HPP