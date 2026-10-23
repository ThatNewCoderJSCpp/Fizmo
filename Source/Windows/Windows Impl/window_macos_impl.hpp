#ifndef FIZMO_WINDOW_MACOS_IMPL_HPP
#define FIZMO_WINDOW_MACOS_IMPL_HPP

#include "../Windows Impl/window_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_MACOS

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {

struct MacWindowState;

class WindowImpl : public ImplBase {
private:
    std::unique_ptr<MacWindowState> m_state;

public:
    WindowImpl(IWindowEventHandler* handler, const fizmo::colors::Color& color) noexcept;
    ~WindowImpl() noexcept override;

    bool create(unsigned int width, unsigned int height, const std::string& title) noexcept override;
    void poll_events() noexcept override;
    bool is_open() const noexcept override;
    void close() noexcept override;
    void set_background_color(const fizmo::colors::Color& color) noexcept override;
    void set_background_erase(bool enabled) noexcept override;
    void invalidate() noexcept override;
    void* native_handle() const noexcept override;
    void set_paint_callback(std::function<void(void*)> cb) noexcept override;
    bool set_cursor_locked(bool locked) noexcept override;
    bool cursor_locked() const noexcept override;
    void set_cursor_visible(bool visible) noexcept override;
    bool cursor_visible() const noexcept override;

    void set_title(const std::string& title) noexcept override;
    bool set_position(int x, int y) noexcept override;
    bool position(int& x, int& y) const noexcept override;
    bool set_size(unsigned int w, unsigned int h) noexcept override;
    void set_resizable(bool resizable) noexcept override;
    void set_min_size(unsigned int w, unsigned int h) noexcept override;
    void minimize() noexcept override;
    void maximize() noexcept override;
    void restore() noexcept override;
    void focus() noexcept override;
    bool set_mode(WindowMode mode, int monitor) noexcept override;
    WindowMode mode() const noexcept override;
    float dpi_scale() const noexcept override;
    int monitor_index() const noexcept override;

    bool set_system_cursor(SystemCursor shape) noexcept override;
    bool set_cursor_image(const images::BitmapImage& img, int hot_x, int hot_y) noexcept override;
    bool set_icons(const std::vector<const images::BitmapImage*>& images) noexcept override;
    void set_app_id(const std::string& id) noexcept override;

    std::string clipboard_text() noexcept override;
    bool set_clipboard_text(const std::string& text) noexcept override;
    void set_drop_enabled(bool enabled) noexcept override;

    void start_text_input() noexcept override;
    void stop_text_input() noexcept override;
    bool text_input_active() const noexcept override;
    void set_text_input_rect(int x, int y, unsigned int w, unsigned int h) noexcept override;

    void dispatch(WindowEvent& e) noexcept { m_event_handler->dispatch_event(e); }
    void resized(unsigned int w, unsigned int h) noexcept { m_event_handler->update_size(w, h); }
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_MACOS
#endif // FIZMO_WINDOW_MACOS_IMPL_HPP
