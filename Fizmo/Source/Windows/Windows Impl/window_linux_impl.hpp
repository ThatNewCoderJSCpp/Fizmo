#ifndef FIZMO_WINDOW_LINUX_IMPL_HPP
#define FIZMO_WINDOW_LINUX_IMPL_HPP

#include "../Windows Impl/window_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_LINUX

#include "../../x11_compat.hpp"
#include <string>
#include <cstdlib>
#include <cmath>
#include <dlfcn.h>

namespace fizmo {
namespace windows {
namespace detail {

class WindowImpl : public ImplBase {
private:
    x11::XDisplay*  m_display = nullptr;
    x11::XWindowId  m_window  = 0;
    x11::XAtomId    m_wm_delete = 0;
    x11::Handle     m_handle;
    int             m_screen  = 0;
    bool            m_open    = false;
    bool            m_erase   = true;          

    std::function<void(void*)> m_paint_callback;
    fizmo::graphics::Color     m_background;

    unsigned int m_last_width  = 0;
    unsigned int m_last_height = 0;

    static constexpr unsigned long kDoubleClickMs   = 400;
    static constexpr int           kDoubleClickSlop = 4;
    unsigned long m_last_click_time   = 0;
    unsigned int  m_last_click_button = 0;
    int m_last_click_x = 0;
    int m_last_click_y = 0;

    bool          m_locked         = false;
    bool          m_cursor_visible = true;
    ::Cursor      m_blank_cursor   = 0;
    bool          m_have_ref       = false;
    int           m_ref_x = 0, m_ref_y = 0;    
    int           m_pre_x = 0, m_pre_y = 0;    
    unsigned long m_warp_serial    = 0;        

    void*  m_xi_lib    = nullptr;
    int    m_xi_opcode = -1;
    bool   m_xi_raw    = false;
    double m_raw_rem_x = 0.0, m_raw_rem_y = 0.0;

public:
    WindowImpl(IWindowEventHandler* handler, const fizmo::graphics::Color& color) noexcept : ImplBase(handler), m_background(color) {
        XInitThreads();
        m_display = XOpenDisplay(nullptr);

        if (m_display) {
            m_screen = DefaultScreen(m_display);
            XkbSetDetectableAutoRepeat(m_display, x11::kTrue, nullptr);
        }

        m_handle.display = m_display;
    }

    ~WindowImpl() noexcept override {
        if (!m_display) return;
        if (m_locked) XUngrabPointer(m_display, CurrentTime);
        if (m_blank_cursor) XFreeCursor(m_display, m_blank_cursor);
        if (m_window) XDestroyWindow(m_display, m_window);
        XCloseDisplay(m_display);  
        m_display = nullptr;
        m_window  = 0;
        if (m_xi_lib) { dlclose(m_xi_lib); m_xi_lib = nullptr; }
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
                       | FocusChangeMask;

        m_window = XCreateWindow(
            m_display, root, 0, 0, width, height, 0,
            DefaultDepth(m_display, m_screen), InputOutput, vis,
            CWBackPixel | CWEventMask, &swa
        );

        if (!m_window) return false;
        m_wm_delete = XInternAtom(m_display, "WM_DELETE_WINDOW", 0);
        XSetWMProtocols(m_display, m_window, &m_wm_delete, 1);
        apply_title(title);
        if (!m_erase) XSetWindowBackgroundPixmap(m_display, m_window, x11::kNone);
        XMapWindow(m_display, m_window);
        XFlush(m_display);
        m_handle.display = m_display;
        m_handle.window  = m_window;
        m_last_width  = width;
        m_last_height = height;
        m_open = true;
        init_raw_motion();
        return true;
    }

    void poll_events() noexcept override {
        if (!m_display) return;
        XEvent ev;

        while (m_open && XPending(m_display) > 0) {
            XNextEvent(m_display, &ev);
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

        XSetWindowBackground(
            m_display, m_window,
            x11::pack_color(
                DefaultVisual(m_display, m_screen),
                color.red(), color.green(), color.blue()
            )
        );

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

    void* native_handle() const noexcept override {
        return static_cast<void*>(const_cast<x11::Handle*>(&m_handle));
    }

    void set_paint_callback(std::function<void(void*)> cb) noexcept override {
        m_paint_callback = std::move(cb);
    }

    bool set_cursor_locked(bool locked) noexcept override {
        if (!m_display || !m_window) return false;
        if (locked == m_locked) return true;

        if (locked) {
            const int r = XGrabPointer(
                m_display, m_window, x11::kTrue,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync,
                m_window,            
                blank_cursor(),      
                CurrentTime
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

private:
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

    void init_raw_motion() noexcept {
    #ifdef FIZMO_HAS_XINPUT2
        int event = 0, error = 0;
        if (!XQueryExtension(m_display, "XInputExtension", &m_xi_opcode, &event, &error)) return;
        m_xi_lib = dlopen("libXi.so.6", RTLD_NOW | RTLD_LOCAL);
        if (!m_xi_lib) return;
        using QueryVersion = int (*)(::Display*, int*, int*);
        using SelectEvents = int (*)(::Display*, ::Window, XIEventMask*, int);
        auto query  = reinterpret_cast<QueryVersion>(dlsym(m_xi_lib, "XIQueryVersion"));
        auto select = reinterpret_cast<SelectEvents>(dlsym(m_xi_lib, "XISelectEvents"));
        int major = 2, minor = 0;
        if (!query || !select || query(m_display, &major, &minor) != 0) return;   // 0 == Success
        unsigned char mask_bits[XIMaskLen(XI_LASTEVENT)] = {};
        XISetMask(mask_bits, XI_RawMotion);
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = sizeof(mask_bits);
        mask.mask     = mask_bits;
        select(m_display, RootWindow(m_display, m_screen), &mask, 1);
        XFlush(m_display);
        m_xi_raw = true;
    #endif
    }

    void handle_generic(XEvent& ev) noexcept {
    #ifdef FIZMO_HAS_XINPUT2
        XGenericEventCookie* cookie = &ev.xcookie;
        if (!m_xi_raw || cookie->extension != m_xi_opcode || !XGetEventData(m_display, cookie)) return;

        if (cookie->evtype == XI_RawMotion && m_locked) {
            const XIRawEvent* raw = static_cast<const XIRawEvent*>(cookie->data);
            const double* values = raw->raw_values;
            double mx = 0.0, my = 0.0;

            for (int axis = 0; axis < 2 && axis < raw->valuators.mask_len * 8; ++axis) {
                if (!XIMaskIsSet(raw->valuators.mask, axis)) continue;
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
            if (e.dx != 0 || e.dy != 0) m_event_handler->dispatch_event(e);
        }

        XFreeEventData(m_display, cookie);
    #else
        (void)ev;
    #endif
    }

    void apply_title(const std::string& title) noexcept {
        XStoreName(m_display, m_window, title.c_str());
        x11::XAtomId net_name = XInternAtom(m_display, "_NET_WM_NAME", 0);
        x11::XAtomId utf8     = XInternAtom(m_display, "UTF8_STRING", 0);

        XChangeProperty(
            m_display, m_window, net_name, utf8, 8, PropModeReplace,
            reinterpret_cast<const unsigned char*>(title.c_str()),
            static_cast<int>(title.size())
        );
    }

    void handle_event(XEvent& ev) noexcept {
        WindowEvent e;

        if (ev.type == x11::kExpose) {
            if (ev.xexpose.count != 0) return;

            if (m_paint_callback) m_paint_callback(nullptr);
            else                  XClearWindow(m_display, m_window);

            e.type = WindowEventType::WindowExpose;
            m_event_handler->dispatch_event(e);
            return;
        }

        if (ev.type == x11::kConfigureNotify) {
            const unsigned int w = static_cast<unsigned int>(ev.xconfigure.width);
            const unsigned int h = static_cast<unsigned int>(ev.xconfigure.height);
            if (w == m_last_width && h == m_last_height) return;
            m_last_width = w;
            m_last_height = h;
            e.type = WindowEventType::WindowResize;
            e.x = w;
            e.y = h;
            m_event_handler->update_size(w, h);
            m_event_handler->dispatch_event(e);
            return;
        }

        if (ev.type == x11::kMotionNotify) {
            if (!relative_motion(ev.xmotion, e.dx, e.dy)) return;
            e.type = WindowEventType::MouseMove;
            e.x = clamp_coord(ev.xmotion.x);
            e.y = clamp_coord(ev.xmotion.y);
            m_event_handler->dispatch_event(e);
            return;
        }

        if (ev.type == x11::kButtonPress || ev.type == x11::kButtonRelease) {
            const bool pressed = (ev.type == x11::kButtonPress);
            const unsigned int xb = ev.xbutton.button;

            if (xb == 4 || xb == 5) {
                if (!pressed) return;
                e.type = WindowEventType::MouseScroll;
                e.x = clamp_coord(ev.xbutton.x);
                e.y = clamp_coord(ev.xbutton.y);
                e.scroll_delta = (xb == 4) ? 1 : -1;
                m_event_handler->dispatch_event(e);
                return;
            }

            if (xb > 3) return;
            unsigned int button = 0;
            const char* name = "";
            if (!map_button(xb, button, name)) return;
            e.x = clamp_coord(ev.xbutton.x);
            e.y = clamp_coord(ev.xbutton.y);
            e.button   = button;
            e.key      = button;
            e.key_name = name;
            e.type = pressed ? WindowEventType::MouseClick : WindowEventType::MouseRelease;
            m_event_handler->dispatch_event(e);

            if (pressed && detect_double_click(ev.xbutton.time, button, ev.xbutton.x, ev.xbutton.y)) {
                e.type = WindowEventType::MouseDoubleClick;
                m_event_handler->dispatch_event(e);
            }

            return;
        }

        if (ev.type == x11::kKeyPress || ev.type == x11::kKeyRelease) {
            dispatch_key(ev.xkey, ev.type == x11::kKeyPress);
            return;
        }

        if (ev.type == x11::kFocusIn) {
            e.type = WindowEventType::WindowFocus;
            m_event_handler->dispatch_event(e);
            return;
        }

        if (ev.type == x11::kFocusOut) {
            if (ev.xfocus.mode == NotifyGrab || ev.xfocus.mode == NotifyUngrab) return;
            if (m_locked) set_cursor_locked(false);
            e.type = WindowEventType::WindowBlur;
            m_event_handler->dispatch_event(e);
            return;
        }

        if (ev.type == x11::kClientMessage) {
            if (static_cast<x11::XAtomId>(ev.xclient.data.l[0]) != m_wm_delete) return;
            e.type = WindowEventType::WindowClose;
            m_event_handler->dispatch_event(e);
            m_open = false;

            if (m_window) {
                XDestroyWindow(m_display, m_window);
                m_window = 0;
                m_handle.window = 0;
            }

            return;
        }

        if (ev.type == x11::kDestroyNotify) {
            m_open = false;
            m_window = 0;
            m_handle.window = 0;
        }
    }

    static unsigned int clamp_coord(int v) noexcept {
        return v < 0 ? 0u : static_cast<unsigned int>(v);
    }

    static bool map_button(unsigned int x_btn, unsigned int& out, const char*& name) noexcept {
        switch (x_btn) {
            case 1: out = 1; name = "LeftMouse";   return true;
            case 2: out = 3; name = "MiddleMouse"; return true;
            case 3: out = 2; name = "RightMouse";  return true;
            default: return false;
        }
    }

    bool detect_double_click(unsigned long t, unsigned int button, int x, int y) noexcept {
        const int dx = (x > m_last_click_x) ? x - m_last_click_x : m_last_click_x - x;
        const int dy = (y > m_last_click_y) ? y - m_last_click_y : m_last_click_y - y;

        const bool hit = m_last_click_button == button
                      && m_last_click_time != 0
                      && (t - m_last_click_time) <= kDoubleClickMs
                      && dx <= kDoubleClickSlop
                      && dy <= kDoubleClickSlop;

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
        char buf[32];
        x11::XKeySym ks = 0;
        int n = XLookupString(&ke, buf, sizeof(buf) - 1, &ks, nullptr);
        if (n < 0) n = 0;
        buf[n] = '\0';
        WindowEvent e;
        e.type = pressed ? WindowEventType::KeyPress : WindowEventType::KeyRelease;
        e.key  = static_cast<unsigned int>(ks);
        e.key_name = key_name(ks, buf, n);
        m_event_handler->dispatch_event(e);
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

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_WINDOW_LINUX_IMPL_HPP