#ifndef FIZMO_WINDOW_WINDOWS_IMPL_HPP
#define FIZMO_WINDOW_WINDOWS_IMPL_HPP

#include "window_base.hpp"
#include "../../Input/keybord.hpp"
#include <cstdlib>

namespace fizmo {
namespace windows {
namespace detail {

#ifdef OS_WINDOWS
class WindowImpl : public ImplBase {
private:
    HWND hwnd;
    WNDCLASSEXW wc;
    HBRUSH backgroundBrush;
    bool m_open = false;
    std::function<void(void*)> m_paint_callback;

    bool m_locked         = false;
    bool m_cursor_visible = true;
    bool m_have_last      = false;
    int  m_last_x = 0, m_last_y = 0;
    bool m_raw_registered = false;   
    bool m_raw_relative   = false;   

    void register_raw_mouse() noexcept {
        if (m_raw_registered || !hwnd) return;
        RAWINPUTDEVICE rid{};
        rid.usUsagePage = 0x01;   
        rid.usUsage     = 0x02;   
        rid.dwFlags     = 0;
        rid.hwndTarget  = hwnd;
        m_raw_registered = RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
    }

    void handle_raw_input(LPARAM lParam) noexcept {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) return;
        if (raw.header.dwType != RIM_TYPEMOUSE || (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) return;
        m_raw_relative = true;
        const LONG dx = raw.data.mouse.lLastX, dy = raw.data.mouse.lLastY;
        if (dx == 0 && dy == 0) return;
        WindowEvent e;
        e.type = WindowEventType::MouseMove;
        e.dx = static_cast<int>(dx);
        e.dy = static_cast<int>(dy);
        e.x  = static_cast<unsigned int>(m_last_x < 0 ? 0 : m_last_x);
        e.y  = static_cast<unsigned int>(m_last_y < 0 ? 0 : m_last_y);
        m_event_handler->dispatch_event(e);
    }

    POINT client_center() const noexcept {
        RECT rc; GetClientRect(hwnd, &rc);
        return POINT{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
    }

    void warp_to_center() noexcept {
        POINT c = client_center();
        m_last_x = c.x; m_last_y = c.y; m_have_last = true;
        ClientToScreen(hwnd, &c);
        SetCursorPos(c.x, c.y);
    }

    void apply_clip() noexcept {
        RECT rc; GetClientRect(hwnd, &rc);
        POINT tl{ rc.left, rc.top }, br{ rc.right, rc.bottom };
        ClientToScreen(hwnd, &tl);
        ClientToScreen(hwnd, &br);
        const RECT clip{ tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
    }

    
    
    bool relative_motion(int x, int y, int& dx, int& dy) noexcept {
        if (!m_have_last) { m_last_x = x; m_last_y = y; m_have_last = true; }
        dx = x - m_last_x; dy = y - m_last_y;
        m_last_x = x; m_last_y = y;
        if (!m_locked) return true;
        if (m_raw_registered && m_raw_relative) return false;  
        if (dx == 0 && dy == 0) return false;
        const POINT c = client_center();
        if (std::abs(x - c.x) > c.x / 2 || std::abs(y - c.y) > c.y / 2) warp_to_center();
        return true;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WindowImpl* impl = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            impl = static_cast<WindowImpl*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
        } else {
            impl = reinterpret_cast<WindowImpl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (impl) {
            WindowEvent event;

            switch (uMsg) {
                case WM_PAINT: {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);
                    
                    if (impl->m_paint_callback) {
                        impl->m_paint_callback(static_cast<void*>(hdc));
                    } else {
                        RECT rect;
                        GetClientRect(hwnd, &rect);
                        FillRect(hdc, &rect, impl->backgroundBrush);
                    }

                    EndPaint(hwnd, &ps);
                    return 0;
                }

                case WM_ERASEBKGND: return 1;

                case WM_MOUSEMOVE:
                    if (!impl->relative_motion(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), event.dx, event.dy)) break;
                    event.type = WindowEventType::MouseMove;
                    event.x = GET_X_LPARAM(lParam);
                    event.y = GET_Y_LPARAM(lParam);
                    impl->m_event_handler->dispatch_event(event);
                    break;

                case WM_INPUT:
                    if (impl->m_locked && impl->m_raw_registered) impl->handle_raw_input(lParam);
                    break;  

                case WM_SETCURSOR:
                    if ((impl->m_locked || !impl->m_cursor_visible) && LOWORD(lParam) == HTCLIENT) {
                        SetCursor(NULL);
                        return TRUE;
                    }
                    break;

                case WM_MOVE:
                    if (impl->m_locked) impl->apply_clip();
                    break;

                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN:
                    event.type = WindowEventType::MouseClick;
                    event.x = GET_X_LPARAM(lParam);
                    event.y = GET_Y_LPARAM(lParam);
                    event.button = (uMsg == WM_LBUTTONDOWN) ? 1 : (uMsg == WM_RBUTTONDOWN) ? 2 : 3;
                    event.key = event.button;
                    event.key_name = (uMsg == WM_LBUTTONDOWN) ? "LeftMouse" : (uMsg == WM_RBUTTONDOWN) ? "RightMouse" : "MiddleMouse";
                    impl->m_event_handler->dispatch_event(event);
                    break;

                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONUP:
                    event.type = WindowEventType::MouseRelease;
                    event.x = GET_X_LPARAM(lParam);
                    event.y = GET_Y_LPARAM(lParam);
                    event.button = (uMsg == WM_LBUTTONUP) ? 1 : (uMsg == WM_RBUTTONUP) ? 2 : 3;
                    event.key = event.button;
                    event.key_name = (uMsg == WM_LBUTTONUP) ? "LeftMouse" : (uMsg == WM_RBUTTONUP) ? "RightMouse" : "MiddleMouse";
                    impl->m_event_handler->dispatch_event(event);
                    break;

                case WM_MOUSEWHEEL: {
                    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    ScreenToClient(hwnd, &pt);
                    event.type = WindowEventType::MouseScroll;
                    event.x = static_cast<unsigned int>(pt.x);
                    event.y = static_cast<unsigned int>(pt.y);
                    event.scroll_delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                    impl->m_event_handler->dispatch_event(event);
                    return 0;
                }

                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDBLCLK: {
                    event.type = WindowEventType::MouseDoubleClick;
                    event.x = GET_X_LPARAM(lParam);
                    event.y = GET_Y_LPARAM(lParam);
                    event.button = (uMsg == WM_LBUTTONDBLCLK) ? 1 : (uMsg == WM_RBUTTONDBLCLK) ? 2 : 3;
                    event.key = event.button;
                    event.key_name = (uMsg == WM_LBUTTONDBLCLK) ? "LeftMouse" : (uMsg == WM_RBUTTONDBLCLK) ? "RightMouse" : "MiddleMouse";
                    impl->m_event_handler->dispatch_event(event);
                    break;
                }

                case WM_KEYDOWN:
                case WM_KEYUP: {
                    event.type = (uMsg == WM_KEYDOWN) ? WindowEventType::KeyPress : WindowEventType::KeyRelease;
                    event.key = static_cast<unsigned int>(wParam);
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                    UINT scancode = (lParam >> 16) & 0xFF;
                    bool extended = (lParam >> 24) & 1;

                    switch (wParam) {
                        case VK_SPACE:   event.key_name = "Space"; break;
                        case VK_RETURN:  event.key_name = "Enter"; break;
                        case VK_TAB:     event.key_name = "Tab"; break;
                        case VK_BACK:    event.key_name = "Backspace"; break;
                        case VK_ESCAPE:  event.key_name = "Escape"; break;
                        case VK_SHIFT:
                            event.key_name = ((lParam >> 16) & 0xFF) == 0x36 ? "RightShift" : "LeftShift";
                            break;
                        case VK_CONTROL:
                            event.key_name = extended ? "RightControl" : "LeftControl";
                            break;
                        case VK_MENU:
                            event.key_name = extended ? "RightAlt" : "LeftAlt";
                            break;
                        case VK_LEFT:    event.key_name = "LeftArrow"; break;
                        case VK_RIGHT:   event.key_name = "RightArrow"; break;
                        case VK_UP:      event.key_name = "UpArrow"; break;
                        case VK_DOWN:    event.key_name = "DownArrow"; break;
                        default:
                            if (wParam >= 'A' && wParam <= 'Z') {
                                event.key_name = std::string(1, static_cast<char>(
                                    (shift ^ caps) ? wParam : (wParam + 32)
                                ));
                            } else {
                                BYTE keyboardState[256];
                                GetKeyboardState(keyboardState);
                                WCHAR buff[2];

                                if (ToUnicode(wParam, scancode, keyboardState, buff, 2, 0) > 0) {
                                    char mbBuff[3];
                                    WideCharToMultiByte(CP_UTF8, 0, buff, -1, mbBuff, 3, NULL, NULL);
                                    event.key_name = mbBuff;
                                } else {
                                    auto keyInfo = ::fizmo::input::Keyboard::get_key_info(scancode, extended);
                                    event.key_name = keyInfo.name;
                                }
                            }
                    }

                    impl->m_event_handler->dispatch_event(event);

                    if (
                        event.key_name == "Space" || event.key_name == "Enter" ||
                        event.key_name == "Tab" || event.key_name == "Backspace" ||
                        event.key_name == "Escape"
                    ) {
                        return 0;
                    }

                    break;
                }

                case WM_SIZE:
                    if (impl->m_locked) impl->apply_clip();
                    event.type = WindowEventType::WindowResize;
                    event.x = LOWORD(lParam);
                    event.y = HIWORD(lParam);
                    impl->m_event_handler->update_size(event.x, event.y);
                    impl->m_event_handler->dispatch_event(event);
                    break;

                case WM_SETFOCUS:
                    event.type = WindowEventType::WindowFocus;
                    impl->m_event_handler->dispatch_event(event);
                    break;

                case WM_KILLFOCUS:
                    if (impl->m_locked) impl->set_cursor_locked(false);
                    event.type = WindowEventType::WindowBlur;
                    impl->m_event_handler->dispatch_event(event);
                    break;

                case WM_CLOSE:
                    event.type = WindowEventType::WindowClose;
                    impl->m_event_handler->dispatch_event(event);
                    DestroyWindow(hwnd);
                    return 0;

                case WM_DESTROY:
                    impl->m_open = false;
                    PostQuitMessage(0);
                    return 0;
            }
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

public:
    WindowImpl(IWindowEventHandler* handler, const fizmo::graphics::Color& color) : ImplBase(handler), hwnd(NULL) {
        backgroundBrush = CreateSolidBrush(RGB(color.red(), color.green(), color.blue()));
        wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.lpszClassName = L"WindowClass";
        RegisterClassExW(&wc);
    }

    ~WindowImpl() { if (backgroundBrush) DeleteObject(backgroundBrush); }

    bool create(unsigned int width, unsigned int height, const std::string& title) noexcept override {
        std::wstring wideTitle(title.begin(), title.end());

        hwnd = CreateWindowExW(
            0, L"WindowClass", wideTitle.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            NULL, NULL, GetModuleHandleW(NULL), this
        );

        if (!hwnd) return false;

        m_open = true;
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        return true;
    }

    void poll_events() noexcept override {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_open = false;
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    bool is_open() const noexcept override { return m_open; }

    void set_background_color(const fizmo::graphics::Color& color) noexcept override {
        if (backgroundBrush) DeleteObject(backgroundBrush);
        backgroundBrush = CreateSolidBrush(RGB(color.red(), color.green(), color.blue()));
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
    }

    void invalidate() noexcept override { if (hwnd) { InvalidateRect(hwnd, NULL, FALSE); } }
    void* native_handle() const noexcept override { return static_cast<void*>(hwnd); }
    void set_paint_callback(std::function<void(void*)> cb) noexcept override { m_paint_callback = std::move(cb); }

    bool set_cursor_locked(bool locked) noexcept override {
        if (!hwnd) return false;
        if (locked == m_locked) return true;

        if (locked) {
            if (GetForegroundWindow() != hwnd) return false;
            register_raw_mouse();
            m_locked = true;
            apply_clip();
            SetCursor(NULL);
            warp_to_center();
        } else {
            m_locked = false;
            ClipCursor(NULL);
            if (m_cursor_visible) SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
        }

        return true;
    }

    bool cursor_locked() const noexcept override { return m_locked; }

    void set_cursor_visible(bool visible) noexcept override {
        m_cursor_visible = visible;
        if (!m_locked) SetCursor(visible ? LoadCursorW(NULL, (LPCWSTR)IDC_ARROW) : NULL);
    }

    bool cursor_visible() const noexcept override { return m_cursor_visible; }
};
#endif

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_WINDOWS_IMPL_HPP