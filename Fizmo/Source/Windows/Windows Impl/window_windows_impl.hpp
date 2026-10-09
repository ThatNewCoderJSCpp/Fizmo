#ifndef FIZMO_WINDOW_WINDOWS_IMPL_HPP
#define FIZMO_WINDOW_WINDOWS_IMPL_HPP

#include "window_base.hpp"
#include "../../Input/keybord.hpp"
#include "../../Input/keys.hpp"
#include "../../Input/raw_input_hook.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef OS_WINDOWS
#include <shellapi.h>
#include <ole2.h>
#endif

namespace fizmo {
namespace windows {
namespace detail {

#ifdef OS_WINDOWS

namespace win32 {

inline std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

inline std::string narrow(const wchar_t* s, int len = -1) {
    if (!s) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, len, &out[0], n, nullptr, nullptr);
    if (len < 0 && !out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

template <typename F>
inline F proc(const wchar_t* dll, const char* name) noexcept {
    HMODULE m = GetModuleHandleW(dll);
    if (!m) m = LoadLibraryExW(dll, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m) return nullptr;
    FARPROC p = GetProcAddress(m, name);
    F f = nullptr;
    std::memcpy(&f, &p, sizeof(f));
    return f;
}

struct PointerInfo {
    DWORD   pointerType;
    UINT32  pointerId;
    UINT32  frameId;
    DWORD   pointerFlags;
    HANDLE  sourceDevice;
    HWND    hwndTarget;
    POINT   ptPixelLocation;
    POINT   ptHimetricLocation;
    POINT   ptPixelLocationRaw;
    POINT   ptHimetricLocationRaw;
    DWORD   dwTime;
    UINT32  historyCount;
    INT32   InputData;
    DWORD   dwKeyStates;
    UINT64  PerformanceCount;
    INT32   ButtonChangeType;
};

struct PointerPenInfo {
    PointerInfo pointerInfo;
    DWORD       penFlags;
    DWORD       penMask;
    UINT32      pressure;
    UINT32      rotation;
    INT32       tiltX;
    INT32       tiltY;
};

using GetPointerTypeFn    = BOOL (WINAPI*)(UINT32, DWORD*);
using GetPointerInfoFn    = BOOL (WINAPI*)(UINT32, PointerInfo*);
using GetPointerPenInfoFn = BOOL (WINAPI*)(UINT32, PointerPenInfo*);
using GetDpiForWindowFn   = UINT (WINAPI*)(HWND);
using SetDpiContextFn     = BOOL (WINAPI*)(HANDLE);
using GetDpiForMonitorFn  = HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*);
using ImmGetContextFn     = HANDLE (WINAPI*)(HWND);
using ImmReleaseContextFn = BOOL (WINAPI*)(HWND, HANDLE);
using ImmGetCompStrFn     = LONG (WINAPI*)(HANDLE, DWORD, LPVOID, DWORD);
using ImmSetCompWindowFn  = BOOL (WINAPI*)(HANDLE, void*);
using ImmSetCandWindowFn  = BOOL (WINAPI*)(HANDLE, void*);
using ImmAssociateExFn    = BOOL (WINAPI*)(HWND, HANDLE, DWORD);

struct ImeCompositionForm {
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT  rcArea;
};

struct ImeCandidateForm {
    DWORD dwIndex;
    DWORD dwStyle;
    POINT ptCurrentPos;
    RECT  rcArea;
};

inline void enable_dpi_awareness() noexcept {
    static bool done = false;
    if (done) return;
    done = true;
    const SetDpiContextFn set = proc<SetDpiContextFn>(L"user32.dll", "SetProcessDpiAwarenessContext");
    if (set && set(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-4)))) return;
    if (set && set(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-3)))) return;
    using SetAwareFn = BOOL (WINAPI*)();
    const SetAwareFn legacy = proc<SetAwareFn>(L"user32.dll", "SetProcessDPIAware");
    if (legacy) legacy();
}

inline float monitor_scale(HMONITOR m, float& dpi) noexcept {
    dpi = 96.0f;
    const GetDpiForMonitorFn get = proc<GetDpiForMonitorFn>(L"shcore.dll", "GetDpiForMonitor");
    UINT x = 96, y = 96;
    if (get && SUCCEEDED(get(m, 0, &x, &y)) && x > 0) dpi = static_cast<float>(x);
    else {
        HDC dc = GetDC(nullptr);
        if (dc) { dpi = static_cast<float>(GetDeviceCaps(dc, LOGPIXELSX)); ReleaseDC(nullptr, dc); }
    }
    return dpi / 96.0f;
}

inline BOOL CALLBACK collect_monitor(HMONITOR m, HDC, LPRECT, LPARAM data) {
    auto* out = reinterpret_cast<std::vector<MonitorInfo>*>(data);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(m, &mi)) return TRUE;
    MonitorInfo info;
    info.bounds = Rect{ mi.rcMonitor.left, mi.rcMonitor.top, static_cast<unsigned int>(mi.rcMonitor.right - mi.rcMonitor.left), static_cast<unsigned int>(mi.rcMonitor.bottom - mi.rcMonitor.top) };
    info.work_area = Rect{ mi.rcWork.left, mi.rcWork.top, static_cast<unsigned int>(mi.rcWork.right - mi.rcWork.left), static_cast<unsigned int>(mi.rcWork.bottom - mi.rcWork.top) };
    info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.handle = reinterpret_cast<std::uintptr_t>(m);
    info.scale = monitor_scale(m, info.dpi);
    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);
    info.name = EnumDisplayDevicesW(mi.szDevice, 0, &dd, 0) ? narrow(dd.DeviceString) : narrow(mi.szDevice);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) info.current = DisplayMode{ dm.dmPelsWidth, dm.dmPelsHeight, static_cast<double>(dm.dmDisplayFrequency), dm.dmBitsPerPel };

    for (DWORD i = 0; ; ++i) {
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (!EnumDisplaySettingsW(mi.szDevice, i, &mode)) break;
        if (mode.dmBitsPerPel < 24) continue;
        bool dup = false;
        for (const DisplayMode& e : info.modes) if (e.width == mode.dmPelsWidth && e.height == mode.dmPelsHeight && e.refresh_rate == static_cast<double>(mode.dmDisplayFrequency)) dup = true;
        if (!dup) info.modes.push_back(DisplayMode{ mode.dmPelsWidth, mode.dmPelsHeight, static_cast<double>(mode.dmDisplayFrequency), mode.dmBitsPerPel });
    }

    HDC dc = CreateDCW(mi.szDevice, nullptr, nullptr, nullptr);
    if (dc) {
        info.physical_width_mm = static_cast<unsigned int>(GetDeviceCaps(dc, HORZSIZE));
        info.physical_height_mm = static_cast<unsigned int>(GetDeviceCaps(dc, VERTSIZE));
        DeleteDC(dc);
    }

    out->push_back(std::move(info));
    return TRUE;
}

} // namespace win32

std::vector<MonitorInfo> platform_monitors() {
    win32::enable_dpi_awareness();
    std::vector<MonitorInfo> out;
    EnumDisplayMonitors(nullptr, nullptr, &win32::collect_monitor, reinterpret_cast<LPARAM>(&out));
    std::stable_sort(out.begin(), out.end(), [](const MonitorInfo& a, const MonitorInfo& b) { return a.primary && !b.primary; });
    return out;
}

class WindowImpl;

class DropTarget : public IDropTarget {
private:
    LONG        m_refs = 1;
    WindowImpl* m_owner;

public:
    explicit DropTarget(WindowImpl* owner) noexcept : m_owner(owner) {}
    virtual ~DropTarget() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
        if (!out) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropTarget) { *out = static_cast<IDropTarget*>(this); AddRef(); return S_OK; }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_refs)); }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG r = InterlockedDecrement(&m_refs);
        if (r == 0) delete this;
        return static_cast<ULONG>(r);
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL pt, DWORD* effect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL pt, DWORD* effect) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL pt, DWORD* effect) override;
};

class WindowImpl : public ImplBase {
private:
    friend class DropTarget;

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

    HCURSOR m_cursor       = nullptr;
    HCURSOR m_owned_cursor = nullptr;
    WindowMode m_mode      = WindowMode::Windowed;
    LONG_PTR   m_saved_style = 0;
    LONG_PTR   m_saved_ex_style = 0;
    RECT       m_saved_rect{};
    bool       m_resizable = true;
    unsigned int m_min_w = 0, m_min_h = 0;
    float      m_scroll_remainder = 0.0f;
    wchar_t    m_high_surrogate = 0;
    bool       m_text_active = true;
    POINT      m_ime_spot{ 0, 0 };
    int        m_ime_height = 0;
    DropTarget* m_drop = nullptr;
    bool       m_ole = false;
    bool       m_drop_enabled = true;
    bool       m_drag_files = false;
    UINT       m_dpi = 96;

    win32::GetPointerTypeFn    m_pointer_type = nullptr;
    win32::GetPointerInfoFn    m_pointer_info = nullptr;
    win32::GetPointerPenInfoFn m_pointer_pen  = nullptr;

    void dispatch(WindowEvent& e) noexcept { m_event_handler->dispatch_event(e); }

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
        e.fx = static_cast<float>(m_last_x);
        e.fy = static_cast<float>(m_last_y);
        dispatch(e);
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

    static input::Modifiers current_mods() noexcept {
        input::Modifiers m = input::Modifiers::None;
        if (GetKeyState(VK_SHIFT) & 0x8000)   m |= input::Modifiers::Shift;
        if (GetKeyState(VK_CONTROL) & 0x8000) m |= input::Modifiers::Control;
        if (GetKeyState(VK_MENU) & 0x8000)    m |= input::Modifiers::Alt;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) m |= input::Modifiers::Meta;
        if (GetKeyState(VK_CAPITAL) & 1)      m |= input::Modifiers::CapsLock;
        if (GetKeyState(VK_NUMLOCK) & 1)      m |= input::Modifiers::NumLock;
        if ((GetKeyState(VK_RMENU) & 0x8000) && (GetKeyState(VK_LCONTROL) & 0x8000)) m |= input::Modifiers::AltGr;
        return m;
    }

    static unsigned int button_of(UINT msg, WPARAM wParam) noexcept {
        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: return 1;
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK: return 2;
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK: return 3;
            default: return HIWORD(wParam) == XBUTTON1 ? 4 : 5;
        }
    }

    static const char* button_name(unsigned int b) noexcept {
        switch (b) {
            case 1: return "LeftMouse";
            case 2: return "RightMouse";
            case 3: return "MiddleMouse";
            case 4: return "X1Mouse";
            default: return "X2Mouse";
        }
    }

    void mouse_button(WindowEventType type, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
        WindowEvent event;
        event.type = type;
        event.x = static_cast<unsigned int>(GET_X_LPARAM(lParam) < 0 ? 0 : GET_X_LPARAM(lParam));
        event.y = static_cast<unsigned int>(GET_Y_LPARAM(lParam) < 0 ? 0 : GET_Y_LPARAM(lParam));
        event.fx = static_cast<float>(GET_X_LPARAM(lParam));
        event.fy = static_cast<float>(GET_Y_LPARAM(lParam));
        event.button = button_of(msg, wParam);
        event.key = event.button;
        event.key_name = button_name(event.button);
        event.mods = current_mods();
        if ((GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700) event.pointer = (GetMessageExtraInfo() & 0x80) ? input::PointerType::Touch : input::PointerType::Pen;
        dispatch(event);
    }

    void key_event(UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept {
        WindowEvent event;
        const bool down = uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN;
        event.type = down ? WindowEventType::KeyPress : WindowEventType::KeyRelease;
        event.key = static_cast<unsigned int>(wParam);
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        UINT scancode = (lParam >> 16) & 0xFF;
        const bool extended = (lParam >> 24) & 1;
        event.scancode = scancode | (extended ? 0x100u : 0u);
        event.repeat = down && ((lParam >> 30) & 1);
        event.mods = current_mods();
        if (scancode == 0) scancode = MapVirtualKeyW(static_cast<UINT>(wParam), 0);
        event.physical_key = input::key_from_set1_scancode(scancode, extended);
        unsigned int vk = static_cast<unsigned int>(wParam);
        if (vk == VK_SHIFT) vk = scancode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
        else if (vk == VK_CONTROL) vk = extended ? VK_RCONTROL : VK_LCONTROL;
        else if (vk == VK_MENU) vk = extended ? VK_RMENU : VK_LMENU;
        else if (vk == VK_RETURN && extended) event.logical_key = input::Key::NumpadEnter;
        if (event.logical_key == input::Key::Unknown) event.logical_key = input::key_from_windows_vk(vk);
        if (event.physical_key == input::Key::Unknown) event.physical_key = event.logical_key;
        if (event.logical_key == input::Key::Unknown) event.logical_key = event.physical_key;

        switch (wParam) {
            case VK_SPACE:   event.key_name = "Space"; break;
            case VK_RETURN:  event.key_name = "Enter"; break;
            case VK_TAB:     event.key_name = "Tab"; break;
            case VK_BACK:    event.key_name = "Backspace"; break;
            case VK_ESCAPE:  event.key_name = "Escape"; break;
            case VK_SHIFT:   event.key_name = scancode == 0x36 ? "RightShift" : "LeftShift"; break;
            case VK_CONTROL: event.key_name = extended ? "RightControl" : "LeftControl"; break;
            case VK_MENU:    event.key_name = extended ? "RightAlt" : "LeftAlt"; break;
            case VK_LEFT:    event.key_name = "LeftArrow"; break;
            case VK_RIGHT:   event.key_name = "RightArrow"; break;
            case VK_UP:      event.key_name = "UpArrow"; break;
            case VK_DOWN:    event.key_name = "DownArrow"; break;
            default:
                if (wParam >= 'A' && wParam <= 'Z') {
                    event.key_name = std::string(1, static_cast<char>((shift ^ caps) ? wParam : (wParam + 32)));
                } else {
                    BYTE keyboardState[256];
                    GetKeyboardState(keyboardState);
                    WCHAR buff[4] = {};
                    if (ToUnicode(static_cast<UINT>(wParam), scancode, keyboardState, buff, 3, 4) > 0) {
                        event.key_name = win32::narrow(buff, 1);
                    } else {
                        auto keyInfo = ::fizmo::input::Keyboard::get_key_info(scancode, extended);
                        event.key_name = keyInfo.name;
                    }
                }
        }

        dispatch(event);
    }

    void text_char(WPARAM ch) noexcept {
        if (!m_text_active) return;
        wchar_t units[2] = {};
        int count = 0;

        if (ch >= 0xD800 && ch <= 0xDBFF) { m_high_surrogate = static_cast<wchar_t>(ch); return; }
        if (ch >= 0xDC00 && ch <= 0xDFFF) {
            if (!m_high_surrogate) return;
            units[0] = m_high_surrogate;
            units[1] = static_cast<wchar_t>(ch);
            count = 2;
            m_high_surrogate = 0;
        } else {
            if (ch < 0x20 || ch == 0x7F) return;
            units[0] = static_cast<wchar_t>(ch);
            count = 1;
        }

        WindowEvent e;
        e.type = WindowEventType::TextInput;
        e.text = win32::narrow(units, count);
        if (!e.text.empty()) dispatch(e);
    }

    void ime_composition(LPARAM lParam) noexcept {
        const auto get = win32::proc<win32::ImmGetContextFn>(L"imm32.dll", "ImmGetContext");
        const auto release = win32::proc<win32::ImmReleaseContextFn>(L"imm32.dll", "ImmReleaseContext");
        const auto str = win32::proc<win32::ImmGetCompStrFn>(L"imm32.dll", "ImmGetCompositionStringW");
        if (!get || !release || !str) return;
        HANDLE ctx = get(hwnd);
        if (!ctx) return;

        if (lParam & 0x0008) {
            const LONG bytes = str(ctx, 0x0008, nullptr, 0);
            std::wstring w(static_cast<std::size_t>(bytes > 0 ? bytes / 2 : 0), L'\0');
            if (bytes > 0) str(ctx, 0x0008, &w[0], static_cast<DWORD>(bytes));
            const LONG cursor = (lParam & 0x0080) ? str(ctx, 0x0080, nullptr, 0) : static_cast<LONG>(w.size());
            WindowEvent e;
            e.type = WindowEventType::TextEditing;
            e.text = win32::narrow(w.c_str(), static_cast<int>(w.size()));
            e.text_cursor = static_cast<int>(cursor < 0 ? 0 : cursor);
            dispatch(e);
        }

        release(hwnd, ctx);
    }

    void apply_ime_position() noexcept {
        const auto get = win32::proc<win32::ImmGetContextFn>(L"imm32.dll", "ImmGetContext");
        const auto release = win32::proc<win32::ImmReleaseContextFn>(L"imm32.dll", "ImmReleaseContext");
        const auto setcomp = win32::proc<win32::ImmSetCompWindowFn>(L"imm32.dll", "ImmSetCompositionWindow");
        const auto setcand = win32::proc<win32::ImmSetCandWindowFn>(L"imm32.dll", "ImmSetCandidateWindow");
        if (!get || !release || !hwnd) return;
        HANDLE ctx = get(hwnd);
        if (!ctx) return;
        win32::ImeCompositionForm cf{};
        cf.dwStyle = 0x0002;
        cf.ptCurrentPos = m_ime_spot;
        if (setcomp) setcomp(ctx, &cf);
        win32::ImeCandidateForm cand{};
        cand.dwIndex = 0;
        cand.dwStyle = 0x0080;
        cand.ptCurrentPos = m_ime_spot;
        cand.rcArea = RECT{ m_ime_spot.x, m_ime_spot.y - m_ime_height, m_ime_spot.x + 1, m_ime_spot.y };
        if (setcand) setcand(ctx, &cand);
        release(hwnd, ctx);
    }

    bool pointer_event(UINT msg, WPARAM wParam) noexcept {
        if (!m_pointer_type || !m_pointer_info) return false;
        const UINT32 id = LOWORD(wParam);
        DWORD type = 0;
        if (!m_pointer_type(id, &type) || (type != 2 && type != 3)) return false;
        win32::PointerPenInfo pen{};
        win32::PointerInfo& info = pen.pointerInfo;
        const bool is_pen = type == 3;
        if (is_pen) { if (!m_pointer_pen || !m_pointer_pen(id, &pen)) return false; }
        else if (!m_pointer_info(id, &info)) return false;
        POINT p = info.ptPixelLocation;
        ScreenToClient(hwnd, &p);
        WindowEvent e;
        e.pointer = is_pen ? input::PointerType::Pen : input::PointerType::Touch;
        e.pointer_id = id;
        e.fx = static_cast<float>(p.x);
        e.fy = static_cast<float>(p.y);
        e.x = static_cast<unsigned int>(p.x < 0 ? 0 : p.x);
        e.y = static_cast<unsigned int>(p.y < 0 ? 0 : p.y);
        e.in_contact = (info.pointerFlags & 0x0004) != 0;
        e.mods = current_mods();

        if (is_pen) {
            e.pressure = (pen.penMask & 1) ? static_cast<float>(pen.pressure) / 1024.0f : (e.in_contact ? 1.0f : 0.0f);
            if (pen.penMask & 4) { e.tilt_x = static_cast<float>(pen.tiltX); e.tilt_y = static_cast<float>(pen.tiltY); }
            if (pen.penMask & 2) e.twist = static_cast<float>(pen.rotation);
            e.barrel = (pen.penFlags & 1) != 0;
            e.eraser = (pen.penFlags & 6) != 0;
            e.type = msg == 0x0246 ? WindowEventType::PenDown : (msg == 0x0247 ? WindowEventType::PenUp : WindowEventType::PenMove);
        } else {
            e.pressure = e.in_contact ? 1.0f : 0.0f;
            const bool cancelled = (info.pointerFlags & 0x8000) != 0;
            e.type = msg == 0x0246 ? WindowEventType::TouchDown : (msg == 0x0247 ? (cancelled ? WindowEventType::TouchCancel : WindowEventType::TouchUp) : WindowEventType::TouchMove);
            if (e.type == WindowEventType::TouchMove && !e.in_contact) return true;
        }

        dispatch(e);
        return true;
    }

    void update_cursor() noexcept {
        if (m_locked || !m_cursor_visible) SetCursor(nullptr);
        else SetCursor(m_cursor ? m_cursor : LoadCursorW(nullptr, IDC_ARROW));
    }

    void set_drop_target(bool enabled) noexcept {
        if (!hwnd) return;

        if (!enabled) {
            if (m_drop) { RevokeDragDrop(hwnd); m_drop->Release(); m_drop = nullptr; }
            if (m_drag_files) { DragAcceptFiles(hwnd, FALSE); m_drag_files = false; }
            return;
        }

        if (m_drop || m_drag_files) return;
        if (!m_ole) { const HRESULT hr = OleInitialize(nullptr); m_ole = SUCCEEDED(hr); }

        if (m_ole) {
            m_drop = new DropTarget(this);
            if (SUCCEEDED(RegisterDragDrop(hwnd, m_drop))) return;
            m_drop->Release();
            m_drop = nullptr;
        }

        DragAcceptFiles(hwnd, TRUE);
        m_drag_files = true;
    }

    static std::vector<std::string> drop_paths(HDROP drop) {
        std::vector<std::string> out;
        const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i) {
            const UINT len = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring w(len + 1, L'\0');
            DragQueryFileW(drop, i, &w[0], len + 1);
            w.resize(len);
            out.push_back(win32::narrow(w.c_str(), static_cast<int>(w.size())));
        }
        return out;
    }

    void drag_event(WindowEventType type, POINTL pt) noexcept {
        POINT p{ pt.x, pt.y };
        ScreenToClient(hwnd, &p);
        WindowEvent e;
        e.type = type;
        e.fx = static_cast<float>(p.x);
        e.fy = static_cast<float>(p.y);
        e.x = static_cast<unsigned int>(p.x < 0 ? 0 : p.x);
        e.y = static_cast<unsigned int>(p.y < 0 ? 0 : p.y);
        dispatch(e);
    }

    void drop_event(std::vector<std::string> paths, POINT p) noexcept {
        WindowEvent e;
        e.type = WindowEventType::FilesDropped;
        e.paths = std::move(paths);
        e.fx = static_cast<float>(p.x);
        e.fy = static_cast<float>(p.y);
        e.x = static_cast<unsigned int>(p.x < 0 ? 0 : p.x);
        e.y = static_cast<unsigned int>(p.y < 0 ? 0 : p.y);
        if (!e.paths.empty()) dispatch(e);
    }

    LRESULT handle(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WindowEvent event;

        switch (uMsg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                if (m_paint_callback) m_paint_callback(static_cast<void*>(hdc));
                else { RECT rect; GetClientRect(hwnd, &rect); FillRect(hdc, &rect, backgroundBrush); }
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_ERASEBKGND: return 1;

            case WM_MOUSEMOVE:
                if (!relative_motion(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), event.dx, event.dy)) break;
                event.type = WindowEventType::MouseMove;
                event.x = static_cast<unsigned int>(GET_X_LPARAM(lParam) < 0 ? 0 : GET_X_LPARAM(lParam));
                event.y = static_cast<unsigned int>(GET_Y_LPARAM(lParam) < 0 ? 0 : GET_Y_LPARAM(lParam));
                event.fx = static_cast<float>(GET_X_LPARAM(lParam));
                event.fy = static_cast<float>(GET_Y_LPARAM(lParam));
                event.mods = current_mods();
                dispatch(event);
                break;

            case WM_INPUT:
                if (input::detail::run_raw_input_hooks(uMsg, wParam, lParam)) break;
                if (m_locked && m_raw_registered) handle_raw_input(lParam);
                break;

            case WM_INPUT_DEVICE_CHANGE:
                input::detail::run_raw_input_hooks(uMsg, wParam, lParam);
                return 0;

            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTCLIENT) { update_cursor(); return TRUE; }
                break;

            case WM_MOVE: {
                if (m_locked) apply_clip();
                POINT p{ 0, 0 };
                ClientToScreen(hwnd, &p);
                event.type = WindowEventType::WindowMove;
                event.window_x = p.x;
                event.window_y = p.y;
                dispatch(event);
                break;
            }

            case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN:
                if (uMsg != WM_XBUTTONDOWN || HIWORD(wParam) <= 2) mouse_button(WindowEventType::MouseClick, uMsg, wParam, lParam);
                if (uMsg == WM_XBUTTONDOWN) return TRUE;
                break;

            case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP:
                if (uMsg != WM_XBUTTONUP || HIWORD(wParam) <= 2) mouse_button(WindowEventType::MouseRelease, uMsg, wParam, lParam);
                if (uMsg == WM_XBUTTONUP) return TRUE;
                break;

            case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK: case WM_XBUTTONDBLCLK:
                mouse_button(WindowEventType::MouseClick, uMsg, wParam, lParam);
                mouse_button(WindowEventType::MouseDoubleClick, uMsg, wParam, lParam);
                if (uMsg == WM_XBUTTONDBLCLK) return TRUE;
                break;

            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL: {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hwnd, &pt);
                event.type = WindowEventType::MouseScroll;
                event.x = static_cast<unsigned int>(pt.x < 0 ? 0 : pt.x);
                event.y = static_cast<unsigned int>(pt.y < 0 ? 0 : pt.y);
                event.fx = static_cast<float>(pt.x);
                event.fy = static_cast<float>(pt.y);
                event.mods = current_mods();
                const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);

                if (uMsg == WM_MOUSEWHEEL) {
                    event.scroll_y = notches;
                    m_scroll_remainder += notches;
                    event.scroll_delta = static_cast<int>(m_scroll_remainder);
                    m_scroll_remainder -= static_cast<float>(event.scroll_delta);
                } else {
                    event.scroll_x = notches;
                }

                dispatch(event);
                return 0;
            }

            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP: {
                key_event(uMsg, wParam, lParam);
                if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_TAB || wParam == VK_BACK || wParam == VK_ESCAPE) return 0;
                if ((uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP) && (wParam == VK_MENU || wParam == VK_F10)) return 0;
                break;
            }

            case WM_CHAR:
                text_char(wParam);
                return 0;

            case WM_SYSCHAR:
                return 0;

            case WM_UNICHAR:
                if (wParam == 0xFFFF) return TRUE;
                if (wParam > 0xFFFF) {
                    const unsigned int cp = static_cast<unsigned int>(wParam) - 0x10000;
                    text_char(0xD800 + (cp >> 10));
                    text_char(0xDC00 + (cp & 0x3FF));
                } else text_char(wParam);
                return 0;

            case WM_IME_STARTCOMPOSITION:
                apply_ime_position();
                break;

            case WM_IME_COMPOSITION:
                ime_composition(lParam);
                break;

            case WM_IME_ENDCOMPOSITION:
                event.type = WindowEventType::TextEditing;
                dispatch(event);
                break;

            case 0x0245:
            case 0x0246:
            case 0x0247:
                pointer_event(uMsg, wParam);
                break;

            case WM_DROPFILES: {
                HDROP drop = reinterpret_cast<HDROP>(wParam);
                POINT p{ 0, 0 };
                DragQueryPoint(drop, &p);
                drop_event(drop_paths(drop), p);
                DragFinish(drop);
                return 0;
            }

            case WM_GETMINMAXINFO: {
                if (m_min_w == 0 && m_min_h == 0) break;
                RECT r{ 0, 0, static_cast<LONG>(m_min_w), static_cast<LONG>(m_min_h) };
                AdjustWindowRectEx(&r, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
                mmi->ptMinTrackSize.x = r.right - r.left;
                mmi->ptMinTrackSize.y = r.bottom - r.top;
                return 0;
            }

            case 0x02E0: {
                m_dpi = HIWORD(wParam);
                const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
                if (suggested && m_mode == WindowMode::Windowed)
                    SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
                event.type = WindowEventType::DpiChanged;
                event.dpi_scale = static_cast<float>(m_dpi) / 96.0f;
                dispatch(event);
                return 0;
            }

            case WM_DISPLAYCHANGE:
                event.type = WindowEventType::MonitorsChanged;
                dispatch(event);
                break;

            case WM_SIZE:
                if (m_locked) apply_clip();
                if (wParam == SIZE_MINIMIZED) { event.type = WindowEventType::WindowMinimize; dispatch(event); break; }
                if (wParam == SIZE_MAXIMIZED) { WindowEvent m; m.type = WindowEventType::WindowMaximize; dispatch(m); }
                else if (wParam == SIZE_RESTORED) { WindowEvent r; r.type = WindowEventType::WindowRestore; dispatch(r); }
                event.type = WindowEventType::WindowResize;
                event.x = LOWORD(lParam);
                event.y = HIWORD(lParam);
                m_event_handler->update_size(event.x, event.y);
                dispatch(event);
                break;

            case WM_SETFOCUS:
                event.type = WindowEventType::WindowFocus;
                dispatch(event);
                break;

            case WM_KILLFOCUS:
                if (m_locked) set_cursor_locked(false);
                event.type = WindowEventType::WindowBlur;
                dispatch(event);
                break;

            case WM_CLOSE:
                event.type = WindowEventType::WindowClose;
                dispatch(event);
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                set_drop_target(false);
                m_open = false;
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WindowImpl* impl = nullptr;

        if (uMsg == WM_NCCREATE) {
            CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            impl = static_cast<WindowImpl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
            if (impl) impl->hwnd = window;
        } else {
            impl = reinterpret_cast<WindowImpl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        }

        if (impl && impl->hwnd == window) return impl->handle(uMsg, wParam, lParam);
        return DefWindowProcW(window, uMsg, wParam, lParam);
    }

    RECT monitor_rect(int monitor) const noexcept {
        if (monitor >= 0) {
            const std::vector<MonitorInfo> mons = platform_monitors();
            if (monitor < static_cast<int>(mons.size())) {
                const Rect& b = mons[static_cast<std::size_t>(monitor)].bounds;
                return RECT{ b.x, b.y, b.x + static_cast<LONG>(b.width), b.y + static_cast<LONG>(b.height) };
            }
        }
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        return mi.rcMonitor;
    }

public:
    WindowImpl(IWindowEventHandler* handler, const fizmo::graphics::Color& color) : ImplBase(handler), hwnd(NULL) {
        win32::enable_dpi_awareness();
        backgroundBrush = CreateSolidBrush(RGB(color.red(), color.green(), color.blue()));
        wc = WNDCLASSEXW{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.lpszClassName = L"WindowClass";
        RegisterClassExW(&wc);
        m_pointer_type = win32::proc<win32::GetPointerTypeFn>(L"user32.dll", "GetPointerType");
        m_pointer_info = win32::proc<win32::GetPointerInfoFn>(L"user32.dll", "GetPointerInfo");
        m_pointer_pen  = win32::proc<win32::GetPointerPenInfoFn>(L"user32.dll", "GetPointerPenInfo");
    }

    ~WindowImpl() {
        if (hwnd) SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        if (m_drop) { if (hwnd) RevokeDragDrop(hwnd); m_drop->Release(); m_drop = nullptr; }
        if (m_ole) OleUninitialize();
        if (m_owned_cursor) DestroyIcon(m_owned_cursor);
        if (backgroundBrush) DeleteObject(backgroundBrush);
    }

    bool create(unsigned int width, unsigned int height, const std::string& title) noexcept override {
        const std::wstring wideTitle = win32::widen(title);
        hwnd = CreateWindowExW(
            0, L"WindowClass", wideTitle.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            NULL, NULL, GetModuleHandleW(NULL), this
        );

        if (!hwnd) return false;
        const auto dpi = win32::proc<win32::GetDpiForWindowFn>(L"user32.dll", "GetDpiForWindow");
        if (dpi) m_dpi = dpi(hwnd);
        m_open = true;
        if (m_drop_enabled) set_drop_target(true);
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
            update_cursor();
        }

        return true;
    }

    bool cursor_locked() const noexcept override { return m_locked; }

    void set_cursor_visible(bool visible) noexcept override {
        m_cursor_visible = visible;
        if (!m_locked) update_cursor();
    }

    bool cursor_visible() const noexcept override { return m_cursor_visible; }

    void set_title(const std::string& title) noexcept override {
        if (hwnd) SetWindowTextW(hwnd, win32::widen(title).c_str());
    }

    bool position(int& x, int& y) const noexcept override {
        x = y = 0;
        if (!hwnd) return false;
        POINT p{ 0, 0 };
        if (!ClientToScreen(hwnd, &p)) return false;
        x = p.x;
        y = p.y;
        return true;
    }

    bool set_position(int x, int y) noexcept override {
        if (!hwnd) return false;
        RECT r{ 0, 0, 0, 0 };
        AdjustWindowRectEx(&r, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
        return SetWindowPos(hwnd, nullptr, x + r.left, y + r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    }

    bool set_size(unsigned int w, unsigned int h) noexcept override {
        if (!hwnd || w == 0 || h == 0) return false;
        RECT r{ 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
        AdjustWindowRectEx(&r, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
        return SetWindowPos(hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    }

    void set_resizable(bool resizable) noexcept override {
        m_resizable = resizable;
        if (!hwnd || m_mode != WindowMode::Windowed) return;
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        if (resizable) style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        else style &= ~static_cast<LONG_PTR>(WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }

    void set_min_size(unsigned int w, unsigned int h) noexcept override { m_min_w = w; m_min_h = h; }
    void minimize() noexcept override { if (hwnd) ShowWindow(hwnd, SW_MINIMIZE); }
    void maximize() noexcept override { if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE); }
    void restore() noexcept override { if (hwnd) ShowWindow(hwnd, SW_RESTORE); }
    void focus() noexcept override { if (hwnd) { SetForegroundWindow(hwnd); SetFocus(hwnd); } }

    bool set_mode(WindowMode mode, int monitor) noexcept override {
        if (!hwnd) return false;

        if (m_mode == WindowMode::Windowed && mode != WindowMode::Windowed) {
            m_saved_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
            m_saved_ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            GetWindowRect(hwnd, &m_saved_rect);
        }

        if (mode == WindowMode::Windowed) {
            if (m_mode != WindowMode::Windowed) {
                SetWindowLongPtrW(hwnd, GWL_STYLE, m_saved_style);
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, m_saved_ex_style);
                SetWindowPos(hwnd, HWND_NOTOPMOST, m_saved_rect.left, m_saved_rect.top, m_saved_rect.right - m_saved_rect.left, m_saved_rect.bottom - m_saved_rect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);
            }
        } else {
            const RECT r = monitor_rect(monitor);
            LONG_PTR style = (m_saved_style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) | WS_POPUP | WS_VISIBLE;
            SetWindowLongPtrW(hwnd, GWL_STYLE, style);
            SetWindowPos(hwnd, mode == WindowMode::Fullscreen ? HWND_TOPMOST : HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
        }

        m_mode = mode;
        if (m_locked) apply_clip();
        return true;
    }

    WindowMode mode() const noexcept override { return m_mode; }
    float dpi_scale() const noexcept override { return static_cast<float>(m_dpi) / 96.0f; }

    int monitor_index() const noexcept override {
        if (!hwnd) return 0;
        const HMONITOR m = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        const std::vector<MonitorInfo> mons = platform_monitors();
        for (std::size_t i = 0; i < mons.size(); ++i) if (mons[i].handle == reinterpret_cast<std::uintptr_t>(m)) return static_cast<int>(i);
        return 0;
    }

    bool set_system_cursor(SystemCursor shape) noexcept override {
        static const LPCWSTR ids[] = { IDC_ARROW, IDC_IBEAM, IDC_HAND, IDC_CROSS, IDC_WAIT, IDC_APPSTARTING, IDC_NO, IDC_SIZEALL, IDC_SIZEWE, IDC_SIZENS, IDC_SIZENWSE, IDC_SIZENESW, IDC_HELP };
        const std::size_t i = static_cast<std::size_t>(shape);
        if (i >= sizeof(ids) / sizeof(ids[0])) return false;
        HCURSOR c = LoadCursorW(nullptr, ids[i]);
        if (!c) return false;
        m_cursor = c;
        if (m_owned_cursor) { DestroyIcon(m_owned_cursor); m_owned_cursor = nullptr; }
        update_cursor();
        return true;
    }

    bool set_cursor_image(const images::BitmapImage& img, int hot_x, int hot_y) noexcept override {
        if (!img.is_valid_image()) return false;
        const int w = static_cast<int>(img.width()), h = static_cast<int>(img.height());
        BITMAPV5HEADER bi{};
        bi.bV5Size = sizeof(bi);
        bi.bV5Width = w;
        bi.bV5Height = -h;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;
        void* bits = nullptr;
        HDC dc = GetDC(nullptr);
        HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, dc);
        if (!color || !bits) return false;
        std::uint32_t* dst = static_cast<std::uint32_t*>(bits);
        const graphics::Color* src = img.pixels().data();
        for (int i = 0; i < w * h; ++i) dst[i] = (static_cast<std::uint32_t>(src[i].alpha()) << 24) | (static_cast<std::uint32_t>(src[i].red()) << 16) | (static_cast<std::uint32_t>(src[i].green()) << 8) | src[i].blue();
        HBITMAP mask = CreateBitmap(w, h, 1, 1, nullptr);
        ICONINFO ii{};
        ii.fIcon = FALSE;
        ii.xHotspot = static_cast<DWORD>(hot_x < 0 ? 0 : hot_x);
        ii.yHotspot = static_cast<DWORD>(hot_y < 0 ? 0 : hot_y);
        ii.hbmMask = mask;
        ii.hbmColor = color;
        HCURSOR cursor = reinterpret_cast<HCURSOR>(CreateIconIndirect(&ii));
        DeleteObject(color);
        if (mask) DeleteObject(mask);
        if (!cursor) return false;
        if (m_owned_cursor) DestroyIcon(m_owned_cursor);
        m_owned_cursor = cursor;
        m_cursor = cursor;
        update_cursor();
        return true;
    }

    std::string clipboard_text() noexcept override {
        if (!OpenClipboard(hwnd)) return {};
        std::string out;
        HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (data) {
            const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(data));
            if (w) { out = win32::narrow(w); GlobalUnlock(data); }
        }
        CloseClipboard();
        return out;
    }

    bool set_clipboard_text(const std::string& text) noexcept override {
        const std::wstring w = win32::widen(text);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
        if (!mem) return false;
        void* p = GlobalLock(mem);
        if (!p) { GlobalFree(mem); return false; }
        std::memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(mem);
        if (!OpenClipboard(hwnd)) { GlobalFree(mem); return false; }
        EmptyClipboard();
        const bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
        CloseClipboard();
        if (!ok) GlobalFree(mem);
        return ok;
    }

    void set_drop_enabled(bool enabled) noexcept override {
        m_drop_enabled = enabled;
        set_drop_target(enabled);
    }

    void start_text_input() noexcept override {
        m_text_active = true;
        const auto assoc = win32::proc<win32::ImmAssociateExFn>(L"imm32.dll", "ImmAssociateContextEx");
        if (assoc && hwnd) assoc(hwnd, nullptr, 0x0010);
    }

    void stop_text_input() noexcept override {
        m_text_active = false;
        const auto assoc = win32::proc<win32::ImmAssociateExFn>(L"imm32.dll", "ImmAssociateContextEx");
        if (assoc && hwnd) assoc(hwnd, nullptr, 0);
    }

    bool text_input_active() const noexcept override { return m_text_active; }

    void set_text_input_rect(int x, int y, unsigned int, unsigned int h) noexcept override {
        m_ime_spot = POINT{ x, y + static_cast<int>(h) };
        m_ime_height = static_cast<int>(h);
        apply_ime_position();
    }
};

inline HRESULT STDMETHODCALLTYPE DropTarget::DragEnter(IDataObject* data, DWORD, POINTL pt, DWORD* effect) {
    FORMATETC fmt{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    const bool files = data && SUCCEEDED(data->QueryGetData(&fmt));
    if (effect) *effect = files ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    if (files && m_owner) m_owner->drag_event(WindowEventType::DragEnter, pt);
    return S_OK;
}

inline HRESULT STDMETHODCALLTYPE DropTarget::DragOver(DWORD, POINTL pt, DWORD* effect) {
    if (effect) *effect = DROPEFFECT_COPY;
    if (m_owner) m_owner->drag_event(WindowEventType::DragOver, pt);
    return S_OK;
}

inline HRESULT STDMETHODCALLTYPE DropTarget::DragLeave() {
    if (m_owner) {
        WindowEvent e;
        e.type = WindowEventType::DragLeave;
        m_owner->dispatch(e);
    }
    return S_OK;
}

inline HRESULT STDMETHODCALLTYPE DropTarget::Drop(IDataObject* data, DWORD, POINTL pt, DWORD* effect) {
    if (effect) *effect = DROPEFFECT_NONE;
    if (!data || !m_owner) return S_OK;
    FORMATETC fmt{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium{};
    if (FAILED(data->GetData(&fmt, &medium))) return S_OK;
    HDROP drop = static_cast<HDROP>(GlobalLock(medium.hGlobal));

    if (drop) {
        POINT p{ pt.x, pt.y };
        ScreenToClient(m_owner->hwnd, &p);
        m_owner->drop_event(WindowImpl::drop_paths(drop), p);
        GlobalUnlock(medium.hGlobal);
        if (effect) *effect = DROPEFFECT_COPY;
    }

    ReleaseStgMedium(&medium);
    return S_OK;
}

#endif

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_WINDOWS_IMPL_HPP
