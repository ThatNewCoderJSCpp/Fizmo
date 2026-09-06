#ifndef FIZMO_WINDOW_WINDOWS_IMPL_HPP
#define FIZMO_WINDOW_WINDOWS_IMPL_HPP

#include "window_base.hpp"
#include "../../Input/keybord.hpp"

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
                    event.type = WindowEventType::MouseMove;
                    event.x = GET_X_LPARAM(lParam);
                    event.y = GET_Y_LPARAM(lParam);
                    impl->m_event_handler->dispatch_event(event);
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
};
#endif

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_WINDOWS_IMPL_HPP