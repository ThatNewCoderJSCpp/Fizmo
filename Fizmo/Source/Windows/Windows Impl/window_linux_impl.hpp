#ifndef FIZMO_WINDOW_LINUX_IMPL_HPP
#define FIZMO_WINDOW_LINUX_IMPL_HPP

#include "../Windows Impl/window_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_LINUX

#include "../../x11_compat.hpp"
#include "../../Input/keys.hpp"
#include "x11_extensions.hpp"
#include <bitset>
#include <cctype>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include <dlfcn.h>

namespace fizmo {
namespace windows {
namespace detail {

inline int x11_quiet_error(::Display*, ::XErrorEvent*) { return 0; }

class X11ErrorTrap {
private:
    ::Display* m_display;
    int (*m_old)(::Display*, ::XErrorEvent*);

public:
    explicit X11ErrorTrap(::Display* d) noexcept : m_display(d) {
        XSync(m_display, 0);
        m_old = XSetErrorHandler(&x11_quiet_error);
    }

    ~X11ErrorTrap() {
        XSync(m_display, 0);
        XSetErrorHandler(m_old);
    }
};

class WindowImpl : public ImplBase {
private:
    struct Atoms {
        x11::XAtomId wm_delete = 0, net_wm_name = 0, utf8 = 0, net_wm_state = 0, state_fullscreen = 0, state_max_v = 0, state_max_h = 0;
        x11::XAtomId state_hidden = 0, motif_hints = 0, bypass_compositor = 0, supporting_wm = 0, active_window = 0, wm_change_state = 0;
        x11::XAtomId clipboard = 0, targets = 0, text = 0, string = 0, clip_property = 0, incr = 0;
        x11::XAtomId xdnd_aware = 0, xdnd_enter = 0, xdnd_position = 0, xdnd_status = 0, xdnd_leave = 0, xdnd_drop = 0, xdnd_finished = 0;
        x11::XAtomId xdnd_selection = 0, xdnd_type_list = 0, xdnd_action_copy = 0, uri_list = 0, text_plain = 0, drop_property = 0;
        x11::XAtomId resource_manager = 0;
    };

    struct PenDevice {
        int  deviceid = 0;
        int  pressure = -1;
        int  tilt_x   = -1;
        int  tilt_y   = -1;
        int  rotation = -1;
        double p_min = 0.0, p_max = 1.0;
        bool eraser = false;
        bool down   = false;
    };

    x11::XDisplay*  m_display = nullptr;
    x11::XWindowId  m_window  = 0;
    Atoms           m_atoms;
    x11::Handle     m_handle;
    int             m_screen  = 0;
    bool            m_open    = false;
    bool            m_erase   = true;

    std::function<void(void*)> m_paint_callback;
    fizmo::graphics::Color     m_background;

    unsigned int m_last_width  = 0;
    unsigned int m_last_height = 0;
    int          m_last_x      = 0;
    int          m_last_y      = 0;

    static constexpr unsigned long kDoubleClickMs   = 400;
    static constexpr int           kDoubleClickSlop = 4;
    unsigned long m_last_click_time   = 0;
    unsigned int  m_last_click_button = 0;
    int m_last_click_x = 0;
    int m_last_click_y = 0;

    bool          m_locked         = false;
    bool          m_cursor_visible = true;
    ::Cursor      m_blank_cursor   = 0;
    ::Cursor      m_cursor         = 0;
    bool          m_cursor_owned   = false;
    std::map<int, ::Cursor> m_system_cursors;
    bool          m_have_ref       = false;
    int           m_ref_x = 0, m_ref_y = 0;
    int           m_pre_x = 0, m_pre_y = 0;
    unsigned long m_warp_serial    = 0;

    x11ext::XInput2 m_xi;
    bool            m_xi_raw = false;
    bool            m_xi_touch = false;
    double          m_raw_rem_x = 0.0, m_raw_rem_y = 0.0;
    std::vector<PenDevice> m_pens;
    int             m_touch_primary = -1;
    bool            m_touch_emulate = true;

    x11ext::XRandR  m_rr;
    bool            m_rr_ok = false;
    x11ext::Xcursor m_xcursor;
    bool            m_xcursor_ok = false;

    XIM             m_im = nullptr;
    XIC             m_ic = nullptr;
    bool            m_text_active = true;
    std::u32string  m_preedit;
    int             m_preedit_caret = 0;
    XIMCallback     m_cb_start{}, m_cb_done{}, m_cb_draw{}, m_cb_caret{};

    std::bitset<256> m_keys_down;

    std::string     m_clipboard;
    bool            m_own_clipboard = false;

    bool            m_drop_enabled = true;
    x11::XWindowId  m_dnd_source = 0;
    int             m_dnd_version = 0;
    x11::XAtomId    m_dnd_type = 0;
    bool            m_dnd_inside = false;
    int             m_dnd_x = 0, m_dnd_y = 0;

    WindowMode      m_mode = WindowMode::Windowed;
    Rect            m_windowed{};
    bool            m_decorated = true;
    bool            m_resizable = true;
    unsigned int    m_min_w = 0, m_min_h = 0;
    bool            m_maximized = false;
    bool            m_minimized = false;
    float           m_dpi = 96.0f;

    x11::XAtomId atom(const char* name) noexcept { return XInternAtom(m_display, name, 0); }

    void init_atoms() noexcept {
        Atoms& a = m_atoms;
        a.wm_delete         = atom("WM_DELETE_WINDOW");
        a.net_wm_name       = atom("_NET_WM_NAME");
        a.utf8              = atom("UTF8_STRING");
        a.net_wm_state      = atom("_NET_WM_STATE");
        a.state_fullscreen  = atom("_NET_WM_STATE_FULLSCREEN");
        a.state_max_v       = atom("_NET_WM_STATE_MAXIMIZED_VERT");
        a.state_max_h       = atom("_NET_WM_STATE_MAXIMIZED_HORZ");
        a.state_hidden      = atom("_NET_WM_STATE_HIDDEN");
        a.motif_hints       = atom("_MOTIF_WM_HINTS");
        a.bypass_compositor = atom("_NET_WM_BYPASS_COMPOSITOR");
        a.supporting_wm     = atom("_NET_SUPPORTING_WM_CHECK");
        a.active_window     = atom("_NET_ACTIVE_WINDOW");
        a.wm_change_state   = atom("WM_CHANGE_STATE");
        a.clipboard         = atom("CLIPBOARD");
        a.targets           = atom("TARGETS");
        a.text              = atom("TEXT");
        a.string            = XA_STRING;
        a.clip_property     = atom("FIZMO_CLIPBOARD");
        a.incr              = atom("INCR");
        a.xdnd_aware        = atom("XdndAware");
        a.xdnd_enter        = atom("XdndEnter");
        a.xdnd_position     = atom("XdndPosition");
        a.xdnd_status       = atom("XdndStatus");
        a.xdnd_leave        = atom("XdndLeave");
        a.xdnd_drop         = atom("XdndDrop");
        a.xdnd_finished     = atom("XdndFinished");
        a.xdnd_selection    = atom("XdndSelection");
        a.xdnd_type_list    = atom("XdndTypeList");
        a.xdnd_action_copy  = atom("XdndActionCopy");
        a.uri_list          = atom("text/uri-list");
        a.text_plain        = atom("text/plain");
        a.drop_property     = atom("FIZMO_DROP");
        a.resource_manager  = atom("RESOURCE_MANAGER");
    }

    void dispatch(WindowEvent& e) noexcept { m_event_handler->dispatch_event(e); }

public:
    WindowImpl(IWindowEventHandler* handler, const fizmo::graphics::Color& color) noexcept : ImplBase(handler), m_background(color) {
        XInitThreads();
        m_display = XOpenDisplay(nullptr);

        if (m_display) {
            m_screen = DefaultScreen(m_display);
            XkbSetDetectableAutoRepeat(m_display, x11::kTrue, nullptr);
            init_atoms();
            m_dpi = x11ext::xft_dpi(m_display);
        }

        m_handle.display = m_display;
    }

    ~WindowImpl() noexcept override {
        if (!m_display) return;
        if (m_locked) XUngrabPointer(m_display, CurrentTime);
        if (m_ic) XDestroyIC(m_ic);
        if (m_im) XCloseIM(m_im);
        if (m_blank_cursor) XFreeCursor(m_display, m_blank_cursor);
        if (m_cursor && m_cursor_owned) XFreeCursor(m_display, m_cursor);
        for (auto& c : m_system_cursors) if (c.second) XFreeCursor(m_display, c.second);
        if (m_window) XDestroyWindow(m_display, m_window);
        m_xi.unload();
        m_rr.unload();
        XCloseDisplay(m_display);
        m_display = nullptr;
        m_window  = 0;
    }

    bool create(unsigned int width, unsigned int height, const std::string& title) noexcept override {
        if (!m_display) return false;
        x11::XWindowId root = RootWindow(m_display, m_screen);
        x11::XVisual*  vis  = DefaultVisual(m_display, m_screen);
        XSetWindowAttributes swa = {};
        swa.background_pixel = x11::pack_color(vis, m_background.red(), m_background.green(), m_background.blue());

        swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask
                       | ButtonPressMask | ButtonReleaseMask
                       | PointerMotionMask | StructureNotifyMask
                       | FocusChangeMask | PropertyChangeMask;

        m_window = XCreateWindow(
            m_display, root, 0, 0, width, height, 0,
            DefaultDepth(m_display, m_screen), InputOutput, vis,
            CWBackPixel | CWEventMask, &swa
        );

        if (!m_window) return false;
        XSetWMProtocols(m_display, m_window, &m_atoms.wm_delete, 1);
        apply_title(title);
        if (!m_erase) XSetWindowBackgroundPixmap(m_display, m_window, x11::kNone);
        init_input_method();
        if (m_drop_enabled) set_drop_enabled(true);
        XMapWindow(m_display, m_window);
        XFlush(m_display);
        m_handle.display = m_display;
        m_handle.window  = m_window;
        m_last_width  = width;
        m_last_height = height;
        m_windowed = Rect{ 0, 0, width, height };
        m_open = true;
        init_xinput2();
        init_randr();
        m_xcursor_ok = m_xcursor.load();
        XSelectInput(m_display, root, PropertyChangeMask);
        return true;
    }

    void poll_events() noexcept override {
        if (!m_display) return;
        XEvent ev;

        while (m_open && XPending(m_display) > 0) {
            XNextEvent(m_display, &ev);
            if (XFilterEvent(&ev, x11::kNone)) continue;
            if (ev.type == GenericEvent) { handle_generic(ev); continue; }
            handle_event(ev);
        }

        if (m_open && m_locked && !m_xi_raw) {
            const int cx = static_cast<int>(m_last_width / 2), cy = static_cast<int>(m_last_height / 2);
            if (m_ref_x != cx || m_ref_y != cy) { warp_to_center(); XFlush(m_display); }
        }
    }

    bool is_open() const noexcept override { return m_open; }

    void set_background_color(const fizmo::graphics::Color& color) noexcept override {
        m_background = color;
        if (!m_display || !m_window || !m_erase) return;
        XSetWindowBackground(m_display, m_window, x11::pack_color(DefaultVisual(m_display, m_screen), color.red(), color.green(), color.blue()));
        if (!m_paint_callback) XClearWindow(m_display, m_window);
        invalidate();
    }

    void set_background_erase(bool enabled) noexcept override {
        m_erase = enabled;
        if (!m_display || !m_window) return;

        if (enabled) {
            XSetWindowBackground(
                m_display, m_window,
                x11::pack_color(DefaultVisual(m_display, m_screen), m_background.red(), m_background.green(), m_background.blue())
            );
        } else {
            XSetWindowBackgroundPixmap(m_display, m_window, x11::kNone);
        }

        XFlush(m_display);
    }

    void invalidate() noexcept override {
        if (!m_display || !m_window) return;
        XEvent ev = {};
        ev.type = x11::kExpose;
        ev.xexpose.window = m_window;
        ev.xexpose.count  = 0;
        XSendEvent(m_display, m_window, x11::kFalse, ExposureMask, &ev);
        XFlush(m_display);
    }

    void* native_handle() const noexcept override { return static_cast<void*>(const_cast<x11::Handle*>(&m_handle)); }
    void set_paint_callback(std::function<void(void*)> cb) noexcept override { m_paint_callback = std::move(cb); }

    bool set_cursor_locked(bool locked) noexcept override {
        if (!m_display || !m_window) return false;
        if (locked == m_locked) return true;

        if (locked) {
            const int r = XGrabPointer(
                m_display, m_window, x11::kTrue,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, m_window, blank_cursor(), CurrentTime
            );

            if (r != GrabSuccess) return false;
            m_locked = true;
            m_raw_rem_x = m_raw_rem_y = 0.0;
            warp_to_center();
        } else {
            XUngrabPointer(m_display, CurrentTime);
            m_locked = false;
            m_warp_serial = 0;
            apply_cursor();
        }

        XFlush(m_display);
        return true;
    }

    bool cursor_locked() const noexcept override { return m_locked; }

    void set_cursor_visible(bool visible) noexcept override {
        m_cursor_visible = visible;
        apply_cursor();
        if (m_display) XFlush(m_display);
    }

    bool cursor_visible() const noexcept override { return m_cursor_visible; }

    void set_title(const std::string& title) noexcept override {
        if (!m_display || !m_window) return;
        apply_title(title);
        XFlush(m_display);
    }

    bool position(int& x, int& y) const noexcept override {
        x = y = 0;
        if (!m_display || !m_window) return false;
        ::Window child = 0;
        return XTranslateCoordinates(m_display, m_window, DefaultRootWindow(m_display), 0, 0, &x, &y, &child) != 0;
    }

    bool set_position(int x, int y) noexcept override {
        if (!m_display || !m_window) return false;
        XMoveWindow(m_display, m_window, x, y);
        XFlush(m_display);
        return true;
    }

    bool set_size(unsigned int w, unsigned int h) noexcept override {
        if (!m_display || !m_window || w == 0 || h == 0) return false;
        if (!m_resizable) apply_size_hints(w, h);
        XResizeWindow(m_display, m_window, w, h);
        XFlush(m_display);
        return true;
    }

    void set_resizable(bool resizable) noexcept override {
        m_resizable = resizable;
        if (m_display && m_window) { apply_size_hints(m_last_width, m_last_height); XFlush(m_display); }
    }

    void set_min_size(unsigned int w, unsigned int h) noexcept override {
        m_min_w = w;
        m_min_h = h;
        if (m_display && m_window) { apply_size_hints(m_last_width, m_last_height); XFlush(m_display); }
    }

    void minimize() noexcept override {
        if (!m_display || !m_window) return;
        XIconifyWindow(m_display, m_window, m_screen);
        XFlush(m_display);
    }

    void maximize() noexcept override {
        if (!m_display || !m_window) return;
        send_state(1, m_atoms.state_max_v, m_atoms.state_max_h);
        XFlush(m_display);
    }

    void restore() noexcept override {
        if (!m_display || !m_window) return;
        send_state(0, m_atoms.state_max_v, m_atoms.state_max_h);
        XMapRaised(m_display, m_window);
        XFlush(m_display);
    }

    void focus() noexcept override {
        if (!m_display || !m_window) return;
        XEvent ev = {};
        ev.xclient.type = x11::kClientMessage;
        ev.xclient.window = m_window;
        ev.xclient.message_type = m_atoms.active_window;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(m_display, DefaultRootWindow(m_display), x11::kFalse, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
        XRaiseWindow(m_display, m_window);
        XFlush(m_display);
    }

    bool set_mode(WindowMode mode, int monitor) noexcept override {
        if (!m_display || !m_window) return false;
        const std::vector<MonitorInfo> mons = x11ext::query_monitors(m_display);
        if (mons.empty()) return false;
        const int idx = monitor >= 0 && monitor < static_cast<int>(mons.size()) ? monitor : monitor_index();
        const Rect target = mons[static_cast<std::size_t>(idx < 0 ? 0 : idx)].bounds;
        if (m_mode == WindowMode::Windowed && mode != WindowMode::Windowed) {
            int x = 0, y = 0;
            position(x, y);
            m_windowed = Rect{ x, y, m_last_width, m_last_height };
        }

        const bool wm = has_window_manager();

        if (mode == WindowMode::Windowed) {
            send_state(0, m_atoms.state_fullscreen, 0);
            set_bypass(false);
            set_decorated(true);
            XMoveResizeWindow(m_display, m_window, m_windowed.x, m_windowed.y, m_windowed.width, m_windowed.height);
        } else if (mode == WindowMode::Borderless) {
            send_state(0, m_atoms.state_fullscreen, 0);
            set_bypass(false);
            set_decorated(false);
            XMoveResizeWindow(m_display, m_window, target.x, target.y, target.width, target.height);
            XRaiseWindow(m_display, m_window);
        } else {
            XMoveWindow(m_display, m_window, target.x, target.y);
            if (wm) send_state(1, m_atoms.state_fullscreen, 0);
            else { set_decorated(false); XMoveResizeWindow(m_display, m_window, target.x, target.y, target.width, target.height); }
            set_bypass(true);
            XRaiseWindow(m_display, m_window);
        }

        m_mode = mode;
        XFlush(m_display);
        return true;
    }

    WindowMode mode() const noexcept override { return m_mode; }
    float dpi_scale() const noexcept override { return m_dpi / 96.0f; }

    int monitor_index() const noexcept override {
        if (!m_display) return 0;
        const std::vector<MonitorInfo> mons = x11ext::query_monitors(m_display);
        int x = 0, y = 0;
        position(x, y);
        const int cx = x + static_cast<int>(m_last_width / 2), cy = y + static_cast<int>(m_last_height / 2);
        for (std::size_t i = 0; i < mons.size(); ++i) if (mons[i].bounds.contains(cx, cy)) return static_cast<int>(i);
        return 0;
    }

    bool set_system_cursor(SystemCursor shape) noexcept override {
        if (!m_display || !m_window) return false;
        const int key = static_cast<int>(shape);
        ::Cursor c = 0;
        auto it = m_system_cursors.find(key);

        if (it != m_system_cursors.end()) c = it->second;
        else {
            static const char* names[] = { "default", "text", "pointer", "crosshair", "wait", "progress", "not-allowed", "move", "ew-resize", "ns-resize", "nwse-resize", "nesw-resize", "help" };
            static const unsigned int fonts[] = { 68, 152, 60, 34, 150, 150, 0, 52, 108, 116, 14, 12, 92 };
            if (m_xcursor_ok && m_xcursor.load_named && key < 13) c = m_xcursor.load_named(m_display, names[key]);
            if (!c && key < 13) c = XCreateFontCursor(m_display, fonts[key]);
            m_system_cursors[key] = c;
        }

        if (m_cursor && m_cursor_owned) XFreeCursor(m_display, m_cursor);
        m_cursor = c;
        m_cursor_owned = false;
        apply_cursor();
        XFlush(m_display);
        return c != 0;
    }

    bool set_cursor_image(const images::BitmapImage& img, int hot_x, int hot_y) noexcept override {
        if (!m_display || !m_window || !img.is_valid_image() || !m_xcursor_ok || !m_xcursor.create || !m_xcursor.from_image) return false;
        x11ext::XcursorImage* xi = m_xcursor.create(static_cast<int>(img.width()), static_cast<int>(img.height()));
        if (!xi) return false;
        xi->xhot = static_cast<unsigned int>(std::max(0, std::min(hot_x, static_cast<int>(img.width()) - 1)));
        xi->yhot = static_cast<unsigned int>(std::max(0, std::min(hot_y, static_cast<int>(img.height()) - 1)));
        const graphics::Color* px = img.pixels().data();

        for (std::size_t i = 0, n = static_cast<std::size_t>(img.width()) * img.height(); i < n; ++i) {
            const unsigned int a = px[i].alpha();
            const unsigned int r = (px[i].red() * a + 127) / 255, g = (px[i].green() * a + 127) / 255, b = (px[i].blue() * a + 127) / 255;
            xi->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        const ::Cursor c = m_xcursor.from_image(m_display, xi);
        if (m_xcursor.destroy) m_xcursor.destroy(xi);
        if (!c) return false;
        if (m_cursor && m_cursor_owned) XFreeCursor(m_display, m_cursor);
        m_cursor = c;
        m_cursor_owned = true;
        apply_cursor();
        XFlush(m_display);
        return true;
    }

    std::string clipboard_text() noexcept override {
        if (!m_display || !m_window) return {};
        if (m_own_clipboard && XGetSelectionOwner(m_display, m_atoms.clipboard) == m_window) return m_clipboard;
        if (XGetSelectionOwner(m_display, m_atoms.clipboard) == 0) return {};
        std::string out;
        if (convert_selection(m_atoms.clipboard, m_atoms.utf8, out)) return out;
        convert_selection(m_atoms.clipboard, m_atoms.string, out);
        return out;
    }

    bool set_clipboard_text(const std::string& text) noexcept override {
        if (!m_display || !m_window) return false;
        m_clipboard = text;
        XSetSelectionOwner(m_display, m_atoms.clipboard, m_window, CurrentTime);
        m_own_clipboard = XGetSelectionOwner(m_display, m_atoms.clipboard) == m_window;
        XFlush(m_display);
        return m_own_clipboard;
    }

    void set_drop_enabled(bool enabled) noexcept override {
        m_drop_enabled = enabled;
        if (!m_display || !m_window) return;

        if (enabled) {
            const unsigned long version = 5;
            XChangeProperty(m_display, m_window, m_atoms.xdnd_aware, XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&version), 1);
        } else {
            XDeleteProperty(m_display, m_window, m_atoms.xdnd_aware);
        }

        XFlush(m_display);
    }

    void start_text_input() noexcept override {
        m_text_active = true;
        if (m_ic) XSetICFocus(m_ic);
    }

    void stop_text_input() noexcept override {
        m_text_active = false;
        if (m_ic) {
            XUnsetICFocus(m_ic);
            char* rest = Xutf8ResetIC(m_ic);
            if (rest) XFree(rest);
        }
        if (!m_preedit.empty()) { m_preedit.clear(); emit_preedit(); }
    }

    bool text_input_active() const noexcept override { return m_text_active; }

    void set_text_input_rect(int x, int y, unsigned int, unsigned int h) noexcept override {
        if (!m_ic) return;
        XPoint spot;
        spot.x = static_cast<short>(x);
        spot.y = static_cast<short>(y + static_cast<int>(h));
        XVaNestedList list = XVaCreateNestedList(0, XNSpotLocation, &spot, static_cast<void*>(nullptr));
        if (!list) return;
        XSetICValues(m_ic, XNPreeditAttributes, list, static_cast<void*>(nullptr));
        XFree(list);
    }

    void set_touch_mouse_emulation(bool enabled) noexcept { m_touch_emulate = enabled; }

private:
    bool has_window_manager() noexcept {
        ::Atom type = 0;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char* data = nullptr;
        const int r = XGetWindowProperty(m_display, DefaultRootWindow(m_display), m_atoms.supporting_wm, 0, 1, 0, XA_WINDOW, &type, &format, &count, &after, &data);
        const bool ok = r == 0 && data && count == 1;
        if (data) XFree(data);
        return ok;
    }

    void send_state(long action, x11::XAtomId a, x11::XAtomId b) noexcept {
        XEvent ev = {};
        ev.xclient.type = x11::kClientMessage;
        ev.xclient.window = m_window;
        ev.xclient.message_type = m_atoms.net_wm_state;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = action;
        ev.xclient.data.l[1] = static_cast<long>(a);
        ev.xclient.data.l[2] = static_cast<long>(b);
        ev.xclient.data.l[3] = 1;
        XSendEvent(m_display, DefaultRootWindow(m_display), x11::kFalse, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    }

    void set_bypass(bool on) noexcept {
        const unsigned long v = on ? 1 : 0;
        XChangeProperty(m_display, m_window, m_atoms.bypass_compositor, XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&v), 1);
    }

    void set_decorated(bool on) noexcept {
        m_decorated = on;
        const unsigned long hints[5] = { 2, 0, on ? 1ul : 0ul, 0, 0 };
        XChangeProperty(m_display, m_window, m_atoms.motif_hints, m_atoms.motif_hints, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(hints), 5);
    }

    void apply_size_hints(unsigned int w, unsigned int h) noexcept {
        XSizeHints* hints = XAllocSizeHints();
        if (!hints) return;
        if (!m_resizable) {
            hints->flags = PMinSize | PMaxSize;
            hints->min_width = hints->max_width = static_cast<int>(w);
            hints->min_height = hints->max_height = static_cast<int>(h);
        } else if (m_min_w || m_min_h) {
            hints->flags = PMinSize;
            hints->min_width = static_cast<int>(m_min_w);
            hints->min_height = static_cast<int>(m_min_h);
        }
        XSetWMNormalHints(m_display, m_window, hints);
        XFree(hints);
    }

    ::Cursor blank_cursor() noexcept {
        if (m_blank_cursor || !m_display || !m_window) return m_blank_cursor;
        static const char zero[1] = { 0 };
        const x11::XPixmapId pm = XCreateBitmapFromData(m_display, m_window, zero, 1, 1);
        if (!pm) return 0;
        XColor black = {};
        m_blank_cursor = XCreatePixmapCursor(m_display, pm, pm, &black, &black, 0, 0);
        XFreePixmap(m_display, pm);
        return m_blank_cursor;
    }

    void apply_cursor() noexcept {
        if (!m_display || !m_window) return;
        if (!m_cursor_visible && blank_cursor()) XDefineCursor(m_display, m_window, m_blank_cursor);
        else if (m_cursor) XDefineCursor(m_display, m_window, m_cursor);
        else XUndefineCursor(m_display, m_window);
    }

    void warp_to_center() noexcept {
        const int cx = static_cast<int>(m_last_width / 2), cy = static_cast<int>(m_last_height / 2);
        m_pre_x = m_ref_x;
        m_pre_y = m_ref_y;
        m_warp_serial = NextRequest(m_display);
        XWarpPointer(m_display, x11::kNone, m_window, 0, 0, 0, 0, cx, cy);
        m_ref_x = cx;
        m_ref_y = cy;
        m_have_ref = true;
    }

    bool relative_motion(const XMotionEvent& m, int& dx, int& dy) noexcept {
        if (m_locked && m_xi_raw) { m_ref_x = m.x; m_ref_y = m.y; return false; }

        if (m_locked && m_warp_serial != 0 && m.serial < m_warp_serial) {
            dx = m.x - m_pre_x; dy = m.y - m_pre_y;
            m_pre_x = m.x; m_pre_y = m.y;
            return dx != 0 || dy != 0;
        }

        if (!m_have_ref) { m_ref_x = m.x; m_ref_y = m.y; m_have_ref = true; }
        dx = m.x - m_ref_x; dy = m.y - m_ref_y;
        m_ref_x = m.x; m_ref_y = m.y;

        if (m_locked) return dx != 0 || dy != 0;
        return true;
    }

    void init_xinput2() noexcept {
        if (!m_xi.load(m_display)) return;
        X11ErrorTrap trap(m_display);
        unsigned char root_bits[x11ext::mask_len(x11ext::kXI_LastEvent)] = {};
        x11ext::set_mask(root_bits, x11ext::kXI_RawMotion);
        x11ext::XIEventMask root_mask{ x11ext::kXIAllMasterDevices, static_cast<int>(sizeof(root_bits)), root_bits };
        m_xi_raw = m_xi.select(m_display, RootWindow(m_display, m_screen), &root_mask, 1) == 0;
        unsigned char hier_bits[x11ext::mask_len(x11ext::kXI_LastEvent)] = {};
        x11ext::set_mask(hier_bits, x11ext::kXI_HierarchyChanged);
        x11ext::XIEventMask hier{ x11ext::kXIAllDevices, static_cast<int>(sizeof(hier_bits)), hier_bits };
        m_xi.select(m_display, RootWindow(m_display, m_screen), &hier, 1);

        if (m_xi.supports_touch()) {
            unsigned char bits[x11ext::mask_len(x11ext::kXI_LastEvent)] = {};
            x11ext::set_mask(bits, x11ext::kXI_TouchBegin);
            x11ext::set_mask(bits, x11ext::kXI_TouchUpdate);
            x11ext::set_mask(bits, x11ext::kXI_TouchEnd);
            x11ext::XIEventMask mask{ x11ext::kXIAllMasterDevices, static_cast<int>(sizeof(bits)), bits };
            m_xi_touch = m_xi.select(m_display, m_window, &mask, 1) == 0;
        }

        refresh_pens();
    }

    static bool label_is(::Display* d, ::Atom label, const char* want) noexcept {
        if (!label) return false;
        char* name = XGetAtomName(d, label);
        if (!name) return false;
        const bool eq = std::strcmp(name, want) == 0;
        XFree(name);
        return eq;
    }

    void refresh_pens() noexcept {
        m_pens.clear();
        if (!m_xi.query || !m_xi.free_info || !m_xi.select) return;
        X11ErrorTrap trap(m_display);
        int count = 0;
        x11ext::XIDeviceInfo* devices = m_xi.query(m_display, x11ext::kXIAllDevices, &count);
        if (!devices) return;

        for (int i = 0; i < count; ++i) {
            const x11ext::XIDeviceInfo& d = devices[i];
            if (d.use != x11ext::kXISlavePointer && d.use != x11ext::kXIFloatingSlave) continue;
            PenDevice pen;
            pen.deviceid = d.deviceid;

            for (int c = 0; c < d.num_classes; ++c) {
                if (d.classes[c]->type != x11ext::kXIValuatorClass) continue;
                const auto* v = reinterpret_cast<const x11ext::XIValuatorClassInfo*>(d.classes[c]);
                if (label_is(m_display, v->label, "Abs Pressure")) { pen.pressure = v->number; pen.p_min = v->min; pen.p_max = v->max; }
                else if (label_is(m_display, v->label, "Abs Tilt X")) pen.tilt_x = v->number;
                else if (label_is(m_display, v->label, "Abs Tilt Y")) pen.tilt_y = v->number;
                else if (label_is(m_display, v->label, "Abs Wheel")) pen.rotation = v->number;
            }

            if (pen.pressure < 0) continue;
            std::string name = d.name ? d.name : "";
            for (char& ch : name) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            pen.eraser = name.find("eraser") != std::string::npos;
            unsigned char bits[x11ext::mask_len(x11ext::kXI_LastEvent)] = {};
            x11ext::set_mask(bits, x11ext::kXI_Motion);
            x11ext::set_mask(bits, x11ext::kXI_ButtonPress);
            x11ext::set_mask(bits, x11ext::kXI_ButtonRelease);
            x11ext::XIEventMask mask{ d.deviceid, static_cast<int>(sizeof(bits)), bits };
            if (m_xi.select(m_display, m_window, &mask, 1) == 0) m_pens.push_back(pen);
        }

        m_xi.free_info(devices);
    }

    void init_randr() noexcept {
        m_rr_ok = m_rr.load(m_display);
        if (m_rr_ok && m_rr.select_input) m_rr.select_input(m_display, DefaultRootWindow(m_display), 1);
    }

    static double valuator(const x11ext::XIValuatorState& vs, int index, bool& found) noexcept {
        found = false;
        if (index < 0 || index >= vs.mask_len * 8 || !x11ext::mask_is_set(vs.mask, index)) return 0.0;
        int pos = 0;
        for (int i = 0; i < index; ++i) if (x11ext::mask_is_set(vs.mask, i)) ++pos;
        found = true;
        return vs.values[pos];
    }

    void handle_generic(XEvent& ev) noexcept {
        XGenericEventCookie* cookie = &ev.xcookie;
        if (!m_xi.lib || cookie->extension != m_xi.opcode || !XGetEventData(m_display, cookie)) return;

        if (cookie->evtype == x11ext::kXI_RawMotion && m_locked && m_xi_raw) {
            const auto* raw = static_cast<const x11ext::XIRawEvent*>(cookie->data);
            const double* values = raw->raw_values;
            double mx = 0.0, my = 0.0;

            for (int axis = 0; axis < 2 && axis < raw->valuators.mask_len * 8; ++axis) {
                if (!x11ext::mask_is_set(raw->valuators.mask, axis)) continue;
                (axis == 0 ? mx : my) = *values++;
            }

            m_raw_rem_x += mx;
            m_raw_rem_y += my;
            WindowEvent e;
            e.type = WindowEventType::MouseMove;
            e.dx = static_cast<int>(std::trunc(m_raw_rem_x));
            e.dy = static_cast<int>(std::trunc(m_raw_rem_y));
            m_raw_rem_x -= e.dx;
            m_raw_rem_y -= e.dy;
            e.x = clamp_coord(m_ref_x);
            e.y = clamp_coord(m_ref_y);
            e.fx = static_cast<float>(m_ref_x);
            e.fy = static_cast<float>(m_ref_y);
            if (e.dx != 0 || e.dy != 0) dispatch(e);
        } else if (cookie->evtype == x11ext::kXI_TouchBegin || cookie->evtype == x11ext::kXI_TouchUpdate || cookie->evtype == x11ext::kXI_TouchEnd) {
            handle_touch(cookie->evtype, *static_cast<const x11ext::XIDeviceEvent*>(cookie->data));
        } else if (cookie->evtype == x11ext::kXI_Motion || cookie->evtype == x11ext::kXI_ButtonPress || cookie->evtype == x11ext::kXI_ButtonRelease) {
            handle_pen(cookie->evtype, *static_cast<const x11ext::XIDeviceEvent*>(cookie->data));
        } else if (cookie->evtype == x11ext::kXI_HierarchyChanged) {
            XFreeEventData(m_display, cookie);
            refresh_pens();
            return;
        }

        XFreeEventData(m_display, cookie);
    }

    void handle_touch(int type, const x11ext::XIDeviceEvent& d) noexcept {
        WindowEvent e;
        e.type = type == x11ext::kXI_TouchBegin ? WindowEventType::TouchDown : (type == x11ext::kXI_TouchEnd ? WindowEventType::TouchUp : WindowEventType::TouchMove);
        e.pointer = input::PointerType::Touch;
        e.pointer_id = d.detail;
        e.fx = static_cast<float>(d.event_x);
        e.fy = static_cast<float>(d.event_y);
        e.x = clamp_coord(static_cast<int>(d.event_x));
        e.y = clamp_coord(static_cast<int>(d.event_y));
        e.pressure = type == x11ext::kXI_TouchEnd ? 0.0f : 1.0f;
        e.in_contact = type != x11ext::kXI_TouchEnd;
        dispatch(e);

        if (!m_touch_emulate) return;
        if (type == x11ext::kXI_TouchBegin && m_touch_primary < 0) m_touch_primary = d.detail;
        if (d.detail != m_touch_primary) return;
        WindowEvent m;
        m.x = e.x;
        m.y = e.y;
        m.fx = e.fx;
        m.fy = e.fy;
        m.pointer = input::PointerType::Touch;

        if (type == x11ext::kXI_TouchUpdate) {
            m.type = WindowEventType::MouseMove;
            if (!m_have_ref) { m_ref_x = static_cast<int>(e.x); m_ref_y = static_cast<int>(e.y); m_have_ref = true; }
            m.dx = static_cast<int>(e.x) - m_ref_x;
            m.dy = static_cast<int>(e.y) - m_ref_y;
            m_ref_x = static_cast<int>(e.x);
            m_ref_y = static_cast<int>(e.y);
            dispatch(m);
            return;
        }

        m.button = 1;
        m.key = 1;
        m.key_name = "LeftMouse";
        m.type = type == x11ext::kXI_TouchBegin ? WindowEventType::MouseClick : WindowEventType::MouseRelease;
        m_ref_x = static_cast<int>(e.x);
        m_ref_y = static_cast<int>(e.y);
        m_have_ref = true;
        dispatch(m);
        if (type == x11ext::kXI_TouchEnd) m_touch_primary = -1;
    }

    void handle_pen(int type, const x11ext::XIDeviceEvent& d) noexcept {
        PenDevice* pen = nullptr;
        for (PenDevice& p : m_pens) if (p.deviceid == d.sourceid || p.deviceid == d.deviceid) pen = &p;
        if (!pen) return;
        WindowEvent e;
        e.pointer = input::PointerType::Pen;
        e.pointer_id = pen->deviceid;
        e.fx = static_cast<float>(d.event_x);
        e.fy = static_cast<float>(d.event_y);
        e.x = clamp_coord(static_cast<int>(d.event_x));
        e.y = clamp_coord(static_cast<int>(d.event_y));
        e.eraser = pen->eraser;
        bool found = false;
        const double p = valuator(d.valuators, pen->pressure, found);
        if (found && pen->p_max > pen->p_min) e.pressure = static_cast<float>((p - pen->p_min) / (pen->p_max - pen->p_min));
        const double tx = valuator(d.valuators, pen->tilt_x, found);
        if (found) e.tilt_x = static_cast<float>(std::max(-90.0, std::min(90.0, tx)));
        const double ty = valuator(d.valuators, pen->tilt_y, found);
        if (found) e.tilt_y = static_cast<float>(std::max(-90.0, std::min(90.0, ty)));
        const double tw = valuator(d.valuators, pen->rotation, found);
        if (found) e.twist = static_cast<float>(tw);
        e.barrel = d.buttons.mask_len > 0 && (x11ext::mask_is_set(d.buttons.mask, 2) || x11ext::mask_is_set(d.buttons.mask, 3));

        if (type == x11ext::kXI_ButtonPress && d.detail == 1) {
            pen->down = true;
            e.type = WindowEventType::PenDown;
        } else if (type == x11ext::kXI_ButtonRelease && d.detail == 1) {
            pen->down = false;
            e.type = WindowEventType::PenUp;
            e.pressure = 0.0f;
        } else if (type == x11ext::kXI_Motion) {
            e.type = WindowEventType::PenMove;
        } else {
            return;
        }

        e.in_contact = pen->down;
        dispatch(e);
    }

    void apply_title(const std::string& title) noexcept {
        XStoreName(m_display, m_window, title.c_str());
        XChangeProperty(
            m_display, m_window, m_atoms.net_wm_name, m_atoms.utf8, 8, PropModeReplace,
            reinterpret_cast<const unsigned char*>(title.c_str()), static_cast<int>(title.size())
        );
    }

    static int preedit_start(XIC, XPointer client, XPointer) {
        WindowImpl* self = reinterpret_cast<WindowImpl*>(client);
        self->m_preedit.clear();
        self->m_preedit_caret = 0;
        return -1;
    }

    static void preedit_done(XIC, XPointer client, XPointer) {
        WindowImpl* self = reinterpret_cast<WindowImpl*>(client);
        self->m_preedit.clear();
        self->m_preedit_caret = 0;
        self->emit_preedit();
    }

    static void preedit_draw(XIC, XPointer client, XPointer call) {
        WindowImpl* self = reinterpret_cast<WindowImpl*>(client);
        const auto* data = reinterpret_cast<const XIMPreeditDrawCallbackStruct*>(call);
        if (!data) return;
        std::u32string& s = self->m_preedit;
        const std::size_t first = std::min<std::size_t>(static_cast<std::size_t>(std::max(0, data->chg_first)), s.size());
        const std::size_t len = std::min<std::size_t>(static_cast<std::size_t>(std::max(0, data->chg_length)), s.size() - first);
        std::u32string insert;

        if (data->text) {
            if (data->text->encoding_is_wchar) {
                if (data->text->string.wide_char)
                    for (unsigned short i = 0; i < data->text->length; ++i) insert.push_back(static_cast<char32_t>(data->text->string.wide_char[i]));
            } else if (data->text->string.multi_byte) {
                insert = utf8_to_u32(data->text->string.multi_byte, std::strlen(data->text->string.multi_byte));
            }
        }

        s.replace(first, len, insert);
        self->m_preedit_caret = std::max(0, std::min(data->caret, static_cast<int>(s.size())));
        self->emit_preedit();
    }

    static void preedit_caret(XIC, XPointer client, XPointer call) {
        WindowImpl* self = reinterpret_cast<WindowImpl*>(client);
        const auto* data = reinterpret_cast<const XIMPreeditCaretCallbackStruct*>(call);
        if (!data) return;
        self->m_preedit_caret = std::max(0, std::min(data->position, static_cast<int>(self->m_preedit.size())));
        self->emit_preedit();
    }

    template <typename F>
    static XIMProc to_proc(F fn) noexcept {
        XIMProc p = nullptr;
        static_assert(sizeof(p) == sizeof(fn), "callback size");
        std::memcpy(&p, &fn, sizeof(p));
        return p;
    }

    void emit_preedit() noexcept {
        WindowEvent e;
        e.type = WindowEventType::TextEditing;
        e.text = u32_to_utf8(m_preedit);
        e.text_cursor = m_preedit_caret;
        dispatch(e);
    }

    static std::u32string utf8_to_u32(const char* s, std::size_t n) {
        std::u32string out;
        std::size_t i = 0;

        while (i < n) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            char32_t cp = 0;
            int extra = 0;
            if (c < 0x80) { cp = c; extra = 0; }
            else if ((c >> 5) == 0x6) { cp = c & 0x1F; extra = 1; }
            else if ((c >> 4) == 0xE) { cp = c & 0x0F; extra = 2; }
            else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
            else { ++i; continue; }
            if (i + static_cast<std::size_t>(extra) >= n) break;
            for (int k = 1; k <= extra; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0x3F);
            out.push_back(cp);
            i += static_cast<std::size_t>(extra) + 1;
        }

        return out;
    }

    static std::string u32_to_utf8(const std::u32string& s) {
        std::string out;
        for (char32_t cp : s) append_utf8(out, cp);
        return out;
    }

    static void append_utf8(std::string& out, char32_t cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    }

    void init_input_method() noexcept {
        const char* current = std::setlocale(LC_CTYPE, nullptr);
        if (current && std::strcmp(current, "C") == 0) std::setlocale(LC_CTYPE, "");
        if (!XSupportsLocale()) return;
        XSetLocaleModifiers("");
        m_im = XOpenIM(m_display, nullptr, nullptr, nullptr);
        if (!m_im) { XSetLocaleModifiers("@im=none"); m_im = XOpenIM(m_display, nullptr, nullptr, nullptr); }
        if (!m_im) return;
        XIMStyles* styles = nullptr;
        if (XGetIMValues(m_im, XNQueryInputStyle, &styles, static_cast<void*>(nullptr)) != nullptr || !styles) { XCloseIM(m_im); m_im = nullptr; return; }
        bool callbacks = false, nothing = false;

        for (unsigned short i = 0; i < styles->count_styles; ++i) {
            const XIMStyle s = styles->supported_styles[i];
            if (s == (XIMPreeditCallbacks | XIMStatusNothing) || s == (XIMPreeditCallbacks | XIMStatusNone)) callbacks = true;
            if (s == (XIMPreeditNothing | XIMStatusNothing) || s == (XIMPreeditNothing | XIMStatusNone)) nothing = true;
        }

        XFree(styles);

        if (callbacks) {
            m_cb_start.client_data = reinterpret_cast<XPointer>(this);
            m_cb_start.callback = to_proc(&preedit_start);
            m_cb_done.client_data = reinterpret_cast<XPointer>(this);
            m_cb_done.callback = to_proc(&preedit_done);
            m_cb_draw.client_data = reinterpret_cast<XPointer>(this);
            m_cb_draw.callback = to_proc(&preedit_draw);
            m_cb_caret.client_data = reinterpret_cast<XPointer>(this);
            m_cb_caret.callback = to_proc(&preedit_caret);
            XVaNestedList list = XVaCreateNestedList(0,
                XNPreeditStartCallback, &m_cb_start, XNPreeditDoneCallback, &m_cb_done,
                XNPreeditDrawCallback, &m_cb_draw, XNPreeditCaretCallback, &m_cb_caret, static_cast<void*>(nullptr));
            m_ic = XCreateIC(m_im, XNInputStyle, XIMPreeditCallbacks | XIMStatusNothing, XNClientWindow, m_window, XNFocusWindow, m_window,
                             XNPreeditAttributes, list, static_cast<void*>(nullptr));
            if (list) XFree(list);
        }

        if (!m_ic && nothing) m_ic = XCreateIC(m_im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, m_window, XNFocusWindow, m_window, static_cast<void*>(nullptr));
        if (!m_ic) { XCloseIM(m_im); m_im = nullptr; return; }
        long filter = 0;
        if (XGetICValues(m_ic, XNFilterEvents, &filter, static_cast<void*>(nullptr)) == nullptr && filter) {
            XWindowAttributes wa;
            XGetWindowAttributes(m_display, m_window, &wa);
            XSelectInput(m_display, m_window, wa.your_event_mask | filter);
        }
        if (m_text_active) XSetICFocus(m_ic);
    }

    static input::Modifiers modifiers_of(unsigned int state) noexcept {
        input::Modifiers m = input::Modifiers::None;
        if (state & ShiftMask)   m |= input::Modifiers::Shift;
        if (state & ControlMask) m |= input::Modifiers::Control;
        if (state & Mod1Mask)    m |= input::Modifiers::Alt;
        if (state & Mod4Mask)    m |= input::Modifiers::Meta;
        if (state & LockMask)    m |= input::Modifiers::CapsLock;
        if (state & Mod2Mask)    m |= input::Modifiers::NumLock;
        if (state & Mod5Mask)    m |= input::Modifiers::AltGr;
        return m;
    }

    void handle_event(XEvent& ev) noexcept {
        WindowEvent e;

        if (ev.type == x11::kExpose) {
            if (ev.xexpose.count != 0) return;
            if (m_paint_callback) m_paint_callback(nullptr);
            else XClearWindow(m_display, m_window);
            e.type = WindowEventType::WindowExpose;
            dispatch(e);
            return;
        }

        if (m_rr_ok && ev.type == m_rr.event_base) {
            e.type = WindowEventType::MonitorsChanged;
            dispatch(e);
            return;
        }

        if (ev.type == x11::kConfigureNotify) {
            if (ev.xconfigure.window != m_window) return;
            const unsigned int w = static_cast<unsigned int>(ev.xconfigure.width);
            const unsigned int h = static_cast<unsigned int>(ev.xconfigure.height);
            int px = 0, py = 0;
            position(px, py);

            if (px != m_last_x || py != m_last_y) {
                m_last_x = px;
                m_last_y = py;
                WindowEvent mv;
                mv.type = WindowEventType::WindowMove;
                mv.window_x = px;
                mv.window_y = py;
                dispatch(mv);
            }

            if (w == m_last_width && h == m_last_height) return;
            m_last_width = w;
            m_last_height = h;
            e.type = WindowEventType::WindowResize;
            e.x = w;
            e.y = h;
            m_event_handler->update_size(w, h);
            dispatch(e);
            return;
        }

        if (ev.type == x11::kMotionNotify) {
            if (!relative_motion(ev.xmotion, e.dx, e.dy)) return;
            e.type = WindowEventType::MouseMove;
            e.x = clamp_coord(ev.xmotion.x);
            e.y = clamp_coord(ev.xmotion.y);
            e.fx = static_cast<float>(ev.xmotion.x);
            e.fy = static_cast<float>(ev.xmotion.y);
            e.mods = modifiers_of(ev.xmotion.state);
            dispatch(e);
            return;
        }

        if (ev.type == x11::kButtonPress || ev.type == x11::kButtonRelease) {
            const bool pressed = (ev.type == x11::kButtonPress);
            const unsigned int xb = ev.xbutton.button;
            e.x = clamp_coord(ev.xbutton.x);
            e.y = clamp_coord(ev.xbutton.y);
            e.fx = static_cast<float>(ev.xbutton.x);
            e.fy = static_cast<float>(ev.xbutton.y);
            e.mods = modifiers_of(ev.xbutton.state);

            if (xb >= 4 && xb <= 7) {
                if (!pressed) return;
                e.type = WindowEventType::MouseScroll;
                if (xb == 4 || xb == 5) { e.scroll_delta = (xb == 4) ? 1 : -1; e.scroll_y = static_cast<float>(e.scroll_delta); }
                else e.scroll_x = (xb == 6) ? -1.0f : 1.0f;
                dispatch(e);
                return;
            }

            unsigned int button = 0;
            const char* name = "";
            if (!map_button(xb, button, name)) return;
            e.button   = button;
            e.key      = button;
            e.key_name = name;
            e.type = pressed ? WindowEventType::MouseClick : WindowEventType::MouseRelease;
            dispatch(e);

            if (pressed && detect_double_click(ev.xbutton.time, button, ev.xbutton.x, ev.xbutton.y)) {
                e.type = WindowEventType::MouseDoubleClick;
                dispatch(e);
            }

            return;
        }

        if (ev.type == x11::kKeyPress || ev.type == x11::kKeyRelease) {
            dispatch_key(ev.xkey, ev.type == x11::kKeyPress);
            return;
        }

        if (ev.type == x11::kFocusIn) {
            if (m_ic && m_text_active) XSetICFocus(m_ic);
            e.type = WindowEventType::WindowFocus;
            dispatch(e);
            return;
        }

        if (ev.type == x11::kFocusOut) {
            if (ev.xfocus.mode == NotifyGrab || ev.xfocus.mode == NotifyUngrab) return;
            if (m_ic) XUnsetICFocus(m_ic);
            if (m_locked) set_cursor_locked(false);
            m_keys_down.reset();
            e.type = WindowEventType::WindowBlur;
            dispatch(e);
            return;
        }

        if (ev.type == x11::kClientMessage) {
            const x11::XAtomId type = ev.xclient.message_type;

            if (type == m_atoms.xdnd_enter || type == m_atoms.xdnd_position || type == m_atoms.xdnd_leave || type == m_atoms.xdnd_drop) {
                handle_xdnd(ev.xclient);
                return;
            }

            if (static_cast<x11::XAtomId>(ev.xclient.data.l[0]) != m_atoms.wm_delete) return;
            e.type = WindowEventType::WindowClose;
            dispatch(e);
            m_open = false;

            if (m_window) {
                XDestroyWindow(m_display, m_window);
                m_window = 0;
                m_handle.window = 0;
            }

            return;
        }

        if (ev.type == SelectionRequest) { answer_selection(ev.xselectionrequest); return; }
        if (ev.type == SelectionClear) { if (ev.xselectionclear.selection == m_atoms.clipboard) m_own_clipboard = false; return; }
        if (ev.type == SelectionNotify) { if (ev.xselection.selection == m_atoms.xdnd_selection) finish_drop(ev.xselection); return; }

        if (ev.type == PropertyNotify) {
            if (ev.xproperty.window == m_window && ev.xproperty.atom == m_atoms.net_wm_state) update_wm_state();
            else if (ev.xproperty.atom == m_atoms.resource_manager) update_dpi();
            return;
        }

        if (ev.type == UnmapNotify && ev.xunmap.window == m_window) {
            if (!m_minimized) { m_minimized = true; e.type = WindowEventType::WindowMinimize; dispatch(e); }
            return;
        }

        if (ev.type == x11::kMapNotify && ev.xmap.window == m_window) {
            if (m_minimized) { m_minimized = false; e.type = WindowEventType::WindowRestore; dispatch(e); }
            return;
        }

        if (ev.type == x11::kDestroyNotify) {
            if (ev.xdestroywindow.window != m_window) return;
            m_open = false;
            m_window = 0;
            m_handle.window = 0;
        }
    }

    void update_dpi() noexcept {
        ::Atom type = 0;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char* data = nullptr;
        float dpi = 96.0f;

        if (XGetWindowProperty(m_display, DefaultRootWindow(m_display), m_atoms.resource_manager, 0, 1 << 20, 0, XA_STRING, &type, &format, &count, &after, &data) == 0 && data) {
            const char* p = std::strstr(reinterpret_cast<const char*>(data), "Xft.dpi:");
            if (p) { const double v = std::strtod(p + 8, nullptr); if (v > 10.0 && v < 1000.0) dpi = static_cast<float>(v); }
        }

        if (data) XFree(data);
        if (std::abs(dpi - m_dpi) < 0.01f) return;
        m_dpi = dpi;
        WindowEvent e;
        e.type = WindowEventType::DpiChanged;
        e.dpi_scale = m_dpi / 96.0f;
        dispatch(e);
    }

    void update_wm_state() noexcept {
        ::Atom type = 0;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char* data = nullptr;
        bool max_v = false, max_h = false, hidden = false;

        if (XGetWindowProperty(m_display, m_window, m_atoms.net_wm_state, 0, 64, 0, XA_ATOM, &type, &format, &count, &after, &data) == 0 && data) {
            const ::Atom* atoms = reinterpret_cast<const ::Atom*>(data);
            for (unsigned long i = 0; i < count; ++i) {
                if (atoms[i] == m_atoms.state_max_v) max_v = true;
                if (atoms[i] == m_atoms.state_max_h) max_h = true;
                if (atoms[i] == m_atoms.state_hidden) hidden = true;
            }
        }

        if (data) XFree(data);
        const bool maximized = max_v && max_h;
        WindowEvent e;

        if (hidden != m_minimized) {
            m_minimized = hidden;
            e.type = hidden ? WindowEventType::WindowMinimize : WindowEventType::WindowRestore;
            dispatch(e);
        }

        if (maximized != m_maximized) {
            m_maximized = maximized;
            e.type = maximized ? WindowEventType::WindowMaximize : WindowEventType::WindowRestore;
            dispatch(e);
        }
    }

    bool convert_selection(x11::XAtomId selection, x11::XAtomId target, std::string& out) noexcept {
        XDeleteProperty(m_display, m_window, m_atoms.clip_property);
        XConvertSelection(m_display, selection, target, m_atoms.clip_property, m_window, CurrentTime);
        XFlush(m_display);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
        XEvent ev;

        while (std::chrono::steady_clock::now() < deadline) {
            if (XCheckTypedWindowEvent(m_display, m_window, SelectionNotify, &ev)) {
                if (ev.xselection.selection != selection) continue;
                if (ev.xselection.property == 0) return false;
                return read_property(m_atoms.clip_property, out);
            }

            if (XCheckTypedWindowEvent(m_display, m_window, SelectionRequest, &ev)) { answer_selection(ev.xselectionrequest); continue; }
            struct timespec ts{ 0, 2000000 };
            nanosleep(&ts, nullptr);
        }

        return false;
    }

    bool read_property(x11::XAtomId property, std::string& out) noexcept {
        ::Atom type = 0;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char* data = nullptr;
        out.clear();
        if (XGetWindowProperty(m_display, m_window, property, 0, 1 << 24, 1, AnyPropertyType, &type, &format, &count, &after, &data) != 0) return false;
        const bool incr = type == m_atoms.incr;
        if (data && !incr && format == 8) out.assign(reinterpret_cast<const char*>(data), count);
        if (data) XFree(data);
        return !incr;
    }

    void answer_selection(const XSelectionRequestEvent& req) noexcept {
        XEvent reply = {};
        reply.xselection.type = SelectionNotify;
        reply.xselection.requestor = req.requestor;
        reply.xselection.selection = req.selection;
        reply.xselection.target = req.target;
        reply.xselection.time = req.time;
        reply.xselection.property = 0;
        const x11::XAtomId property = req.property ? req.property : req.target;

        if (req.selection == m_atoms.clipboard && m_own_clipboard) {
            if (req.target == m_atoms.targets) {
                const ::Atom list[] = { m_atoms.targets, m_atoms.utf8, m_atoms.string, m_atoms.text, m_atoms.text_plain };
                XChangeProperty(m_display, req.requestor, property, XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(list), 5);
                reply.xselection.property = property;
            } else if (req.target == m_atoms.utf8 || req.target == m_atoms.string || req.target == m_atoms.text || req.target == m_atoms.text_plain) {
                const x11::XAtomId type = req.target == m_atoms.string ? m_atoms.string : m_atoms.utf8;
                XChangeProperty(m_display, req.requestor, property, type, 8, PropModeReplace,
                                reinterpret_cast<const unsigned char*>(m_clipboard.data()), static_cast<int>(m_clipboard.size()));
                reply.xselection.property = property;
            }
        }

        XSendEvent(m_display, req.requestor, x11::kFalse, 0, &reply);
        XFlush(m_display);
    }

    void send_client(x11::XWindowId to, x11::XAtomId type, long l0, long l1, long l2, long l3, long l4) noexcept {
        XEvent ev = {};
        ev.xclient.type = x11::kClientMessage;
        ev.xclient.window = to;
        ev.xclient.message_type = type;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = l0;
        ev.xclient.data.l[1] = l1;
        ev.xclient.data.l[2] = l2;
        ev.xclient.data.l[3] = l3;
        ev.xclient.data.l[4] = l4;
        XSendEvent(m_display, to, x11::kFalse, NoEventMask, &ev);
        XFlush(m_display);
    }

    void handle_xdnd(const XClientMessageEvent& cm) noexcept {
        if (!m_drop_enabled) return;
        WindowEvent e;

        if (cm.message_type == m_atoms.xdnd_enter) {
            m_dnd_source = static_cast<x11::XWindowId>(cm.data.l[0]);
            m_dnd_version = static_cast<int>(static_cast<unsigned long>(cm.data.l[1]) >> 24);
            m_dnd_type = 0;
            m_dnd_inside = false;
            std::vector<::Atom> types;

            if (cm.data.l[1] & 1) {
                ::Atom type = 0;
                int format = 0;
                unsigned long count = 0, after = 0;
                unsigned char* data = nullptr;
                if (XGetWindowProperty(m_display, m_dnd_source, m_atoms.xdnd_type_list, 0, 1024, 0, XA_ATOM, &type, &format, &count, &after, &data) == 0 && data) {
                    const ::Atom* a = reinterpret_cast<const ::Atom*>(data);
                    types.assign(a, a + count);
                }
                if (data) XFree(data);
            } else {
                for (int i = 2; i < 5; ++i) if (cm.data.l[i]) types.push_back(static_cast<::Atom>(cm.data.l[i]));
            }

            for (::Atom t : types) if (t == m_atoms.uri_list) m_dnd_type = t;
            return;
        }

        if (cm.message_type == m_atoms.xdnd_position) {
            const int rx = static_cast<int>((static_cast<unsigned long>(cm.data.l[2]) >> 16) & 0xFFFF);
            const int ry = static_cast<int>(static_cast<unsigned long>(cm.data.l[2]) & 0xFFFF);
            ::Window child = 0;
            XTranslateCoordinates(m_display, DefaultRootWindow(m_display), m_window, rx, ry, &m_dnd_x, &m_dnd_y, &child);
            const bool accept = m_dnd_type != 0;
            send_client(m_dnd_source, m_atoms.xdnd_status, static_cast<long>(m_window), accept ? 3 : 2, 0, 0, accept ? static_cast<long>(m_atoms.xdnd_action_copy) : 0);
            e.type = m_dnd_inside ? WindowEventType::DragOver : WindowEventType::DragEnter;
            m_dnd_inside = true;
            e.x = clamp_coord(m_dnd_x);
            e.y = clamp_coord(m_dnd_y);
            e.fx = static_cast<float>(m_dnd_x);
            e.fy = static_cast<float>(m_dnd_y);
            dispatch(e);
            return;
        }

        if (cm.message_type == m_atoms.xdnd_leave) {
            m_dnd_inside = false;
            e.type = WindowEventType::DragLeave;
            dispatch(e);
            return;
        }

        if (cm.message_type == m_atoms.xdnd_drop) {
            if (!m_dnd_type) {
                send_client(m_dnd_source, m_atoms.xdnd_finished, static_cast<long>(m_window), 0, 0, 0, 0);
                return;
            }
            const ::Time t = m_dnd_version >= 1 ? static_cast<::Time>(cm.data.l[2]) : CurrentTime;
            XConvertSelection(m_display, m_atoms.xdnd_selection, m_dnd_type, m_atoms.drop_property, m_window, t);
            XFlush(m_display);
        }
    }

    static int hex_value(char c) noexcept {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static std::vector<std::string> parse_uri_list(const std::string& data) {
        std::vector<std::string> out;
        std::size_t start = 0;

        while (start < data.size()) {
            std::size_t end = data.find('\n', start);
            if (end == std::string::npos) end = data.size();
            std::string line = data.substr(start, end - start);
            start = end + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == '\0')) line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            if (line.compare(0, 7, "file://") == 0) {
                line.erase(0, 7);
                const std::size_t slash = line.find('/');
                if (slash != std::string::npos) line.erase(0, slash);
            }
            std::string path;
            for (std::size_t i = 0; i < line.size(); ++i) {
                if (line[i] == '%' && i + 2 < line.size() && hex_value(line[i + 1]) >= 0 && hex_value(line[i + 2]) >= 0) {
                    path.push_back(static_cast<char>(hex_value(line[i + 1]) * 16 + hex_value(line[i + 2])));
                    i += 2;
                } else path.push_back(line[i]);
            }
            if (!path.empty()) out.push_back(path);
        }

        return out;
    }

    void finish_drop(const XSelectionEvent& sel) noexcept {
        std::string data;
        const bool ok = sel.property != 0 && read_property(m_atoms.drop_property, data);
        send_client(m_dnd_source, m_atoms.xdnd_finished, static_cast<long>(m_window), ok ? 1 : 0, ok ? static_cast<long>(m_atoms.xdnd_action_copy) : 0, 0, 0);
        m_dnd_inside = false;
        if (!ok) return;
        WindowEvent e;
        e.type = WindowEventType::FilesDropped;
        e.paths = parse_uri_list(data);
        e.x = clamp_coord(m_dnd_x);
        e.y = clamp_coord(m_dnd_y);
        e.fx = static_cast<float>(m_dnd_x);
        e.fy = static_cast<float>(m_dnd_y);
        if (!e.paths.empty()) dispatch(e);
    }

    static unsigned int clamp_coord(int v) noexcept { return v < 0 ? 0u : static_cast<unsigned int>(v); }

    static bool map_button(unsigned int x_btn, unsigned int& out, const char*& name) noexcept {
        switch (x_btn) {
            case 1: out = 1; name = "LeftMouse";   return true;
            case 2: out = 3; name = "MiddleMouse"; return true;
            case 3: out = 2; name = "RightMouse";  return true;
            case 8: out = 4; name = "X1Mouse";     return true;
            case 9: out = 5; name = "X2Mouse";     return true;
            default: return false;
        }
    }

    bool detect_double_click(unsigned long t, unsigned int button, int x, int y) noexcept {
        const int dx = (x > m_last_click_x) ? x - m_last_click_x : m_last_click_x - x;
        const int dy = (y > m_last_click_y) ? y - m_last_click_y : m_last_click_y - y;
        const bool hit = m_last_click_button == button && m_last_click_time != 0 && (t - m_last_click_time) <= kDoubleClickMs && dx <= kDoubleClickSlop && dy <= kDoubleClickSlop;

        if (hit) {
            m_last_click_time   = 0;
            m_last_click_button = 0;
        } else {
            m_last_click_time   = t;
            m_last_click_button = button;
            m_last_click_x = x;
            m_last_click_y = y;
        }

        return hit;
    }

    void dispatch_key(XKeyEvent& ke, bool pressed) noexcept {
        char buf[64];
        x11::XKeySym ks = 0;
        int n = XLookupString(&ke, buf, sizeof(buf) - 1, &ks, nullptr);
        if (n < 0) n = 0;
        buf[n] = '\0';
        WindowEvent e;
        e.type = pressed ? WindowEventType::KeyPress : WindowEventType::KeyRelease;
        e.key  = static_cast<unsigned int>(ks);
        e.scancode = ke.keycode;
        e.key_name = key_name(ks, buf, n);
        e.physical_key = input::key_from_x11_keycode(ke.keycode);
        e.logical_key = input::key_from_x11_keysym(XkbKeycodeToKeysym(m_display, static_cast<KeyCode>(ke.keycode), 0, 0));
        if (e.logical_key == input::Key::Unknown) e.logical_key = e.physical_key;
        e.mods = modifiers_of(ke.state);
        const unsigned int code = ke.keycode & 0xFFu;

        if (pressed) {
            e.repeat = m_keys_down.test(code);
            m_keys_down.set(code);
        } else {
            m_keys_down.reset(code);
        }

        dispatch(e);
        if (!pressed || !m_text_active) return;
        std::string text;

        if (m_ic) {
            char tbuf[256];
            int status = 0;
            x11::XKeySym sym = 0;
            const int len = Xutf8LookupString(m_ic, &ke, tbuf, sizeof(tbuf) - 1, &sym, &status);
            if ((status == XLookupChars || status == XLookupBoth) && len > 0) text.assign(tbuf, static_cast<std::size_t>(len));
        } else {
            for (int i = 0; i < n; ++i) append_utf8(text, static_cast<unsigned char>(buf[i]));
        }

        std::string printable;
        for (unsigned char c : text) if (c >= 0x20 && c != 0x7F) printable.push_back(static_cast<char>(c));
        if (printable.empty() || has(e.mods, input::Modifiers::Control)) return;
        WindowEvent t;
        t.type = WindowEventType::TextInput;
        t.text = printable;
        dispatch(t);
    }

    static std::string key_name(x11::XKeySym ks, const char* text, int len) noexcept {
        switch (ks) {
            case XK_space:              return "Space";
            case XK_Return:
            case XK_KP_Enter:           return "Enter";
            case XK_Tab:
            case XK_ISO_Left_Tab:       return "Tab";
            case XK_BackSpace:          return "Backspace";
            case XK_Escape:             return "Escape";
            case XK_Shift_L:            return "LeftShift";
            case XK_Shift_R:            return "RightShift";
            case XK_Control_L:          return "LeftControl";
            case XK_Control_R:          return "RightControl";
            case XK_Alt_L:              return "LeftAlt";
            case XK_Alt_R:
            case XK_ISO_Level3_Shift:   return "RightAlt";
            case XK_Left:               return "LeftArrow";
            case XK_Right:              return "RightArrow";
            case XK_Up:                 return "UpArrow";
            case XK_Down:               return "DownArrow";
            default: break;
        }

        if (len > 0 && static_cast<unsigned char>(text[0]) >= 0x20) { return std::string(text, static_cast<std::size_t>(len)); }
        const char* s = XKeysymToString(ks);
        return s ? std::string(s) : std::string();
    }
};

inline std::vector<MonitorInfo> platform_monitors() {
    ::Display* d = XOpenDisplay(nullptr);
    if (!d) return {};
    std::vector<MonitorInfo> out = x11ext::query_monitors(d);
    XCloseDisplay(d);
    return out;
}

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_WINDOW_LINUX_IMPL_HPP
