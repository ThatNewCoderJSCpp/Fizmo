#include "fizmo_library.hpp"
#include "gamepad.hpp"

#if defined(OS_LINUX)
    #include <dirent.h>
    #include <fcntl.h>
    #include <linux/input.h>
    #include <sys/inotify.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
    #include <cerrno>
#endif

namespace fizmo {
namespace input {

#if defined(OS_LINUX)
namespace detail {

class LinuxGamepadBackend : public GamepadBackend {
private:
    struct Device : GamepadDevice {
        int fd = -1;
        int ff_id = -1;
        std::vector<int> key_index;
        std::vector<int> abs_index;
        std::vector<int> hat_index;
        std::vector<int> abs_min, abs_max;

        ~Device() override { if (fd >= 0) ::close(fd); }

        bool set_rumble(float low, float high, unsigned int ms) noexcept override;
    };

    std::vector<std::unique_ptr<Device>> m_devices;
    int  m_inotify = -1;
    int  m_watch   = -1;
    int  m_next_key = 1;
    bool m_rescan  = true;
    std::chrono::steady_clock::time_point m_last_scan{};
    std::string m_root;

    static bool test_bit(const unsigned long* bits, unsigned int bit) noexcept {
        constexpr unsigned int per = sizeof(unsigned long) * 8;
        return (bits[bit / per] >> (bit % per)) & 1ul;
    }

    static float normalize(int v, int lo, int hi) noexcept;

    bool known(const std::string& path) const noexcept {
        for (const auto& d : m_devices) if (d->info.path == path) return true;
        return false;
    }

    std::unique_ptr<Device> open_device(const std::string& path);

    static void bind_button(GamepadMapping& m, GamepadButton target, const Device& d, unsigned int code);

    static void bind_axis(GamepadMapping& m, GamepadAxis target, const Device& d, unsigned int code);

    static void build_fallback(Device& d, const unsigned long* key_bits, const unsigned long* abs_bits);

    static void set_hat(Device& d, unsigned int code, int value) noexcept;

    static void sync(Device& d) noexcept;

    bool read_device(Device& d) noexcept;

    void scan(std::vector<GamepadDevice*>& added);

public:
    explicit LinuxGamepadBackend(const std::string& root = std::string());

    ~LinuxGamepadBackend() override { if (m_inotify >= 0) ::close(m_inotify); }

    void poll(std::vector<GamepadDevice*>& added, std::vector<int>& removed) override;
};

} // namespace detail
#endif

#if defined(OS_WINDOWS)
namespace detail {

class WindowsGamepadBackend : public GamepadBackend {
private:
    struct XGamepad {
        WORD  wButtons;
        BYTE  bLeftTrigger;
        BYTE  bRightTrigger;
        SHORT sThumbLX;
        SHORT sThumbLY;
        SHORT sThumbRX;
        SHORT sThumbRY;
    };

    struct XState {
        DWORD    dwPacketNumber;
        XGamepad Gamepad;
    };

    struct XVibration {
        WORD wLeftMotorSpeed;
        WORD wRightMotorSpeed;
    };

    using GetStateFn = DWORD (WINAPI*)(DWORD, XState*);
    using SetStateFn = DWORD (WINAPI*)(DWORD, XVibration*);

    struct XDevice : GamepadDevice {
        DWORD      slot = 0;
        SetStateFn set_state = nullptr;

        bool set_rumble(float low, float high, unsigned int) noexcept override {
            if (!set_state) return false;
            XVibration v{ static_cast<WORD>(low * 65535.0f), static_cast<WORD>(high * 65535.0f) };
            return set_state(slot, &v) == ERROR_SUCCESS;
        }
    };

    struct HidCaps {
        USHORT Usage;
        USHORT UsagePage;
        USHORT InputReportByteLength;
        USHORT OutputReportByteLength;
        USHORT FeatureReportByteLength;
        USHORT Reserved[17];
        USHORT NumberLinkCollectionNodes;
        USHORT NumberInputButtonCaps;
        USHORT NumberInputValueCaps;
        USHORT NumberInputDataIndices;
        USHORT NumberOutputButtonCaps;
        USHORT NumberOutputValueCaps;
        USHORT NumberOutputDataIndices;
        USHORT NumberFeatureButtonCaps;
        USHORT NumberFeatureValueCaps;
        USHORT NumberFeatureDataIndices;
    };

    struct HidRange {
        USHORT UsageMin, UsageMax, StringMin, StringMax, DesignatorMin, DesignatorMax, DataIndexMin, DataIndexMax;
    };

    struct HidButtonCaps {
        USHORT  UsagePage;
        UCHAR   ReportID;
        BOOLEAN IsAlias;
        USHORT  BitField;
        USHORT  LinkCollection;
        USHORT  LinkUsage;
        USHORT  LinkUsagePage;
        BOOLEAN IsRange;
        BOOLEAN IsStringRange;
        BOOLEAN IsDesignatorRange;
        BOOLEAN IsAbsolute;
        USHORT  ReportCount;
        USHORT  Reserved2;
        ULONG   Reserved[9];
        HidRange Range;
    };

    struct HidValueCaps {
        USHORT  UsagePage;
        UCHAR   ReportID;
        BOOLEAN IsAlias;
        USHORT  BitField;
        USHORT  LinkCollection;
        USHORT  LinkUsage;
        USHORT  LinkUsagePage;
        BOOLEAN IsRange;
        BOOLEAN IsStringRange;
        BOOLEAN IsDesignatorRange;
        BOOLEAN IsAbsolute;
        BOOLEAN HasNull;
        UCHAR   Reserved;
        USHORT  BitSize;
        USHORT  ReportCount;
        USHORT  Reserved2[5];
        ULONG   UnitsExp;
        ULONG   Units;
        LONG    LogicalMin, LogicalMax;
        LONG    PhysicalMin, PhysicalMax;
        HidRange Range;
    };

    using HidGetCapsFn        = LONG (WINAPI*)(void*, HidCaps*);
    using HidGetButtonCapsFn  = LONG (WINAPI*)(int, HidButtonCaps*, USHORT*, void*);
    using HidGetValueCapsFn   = LONG (WINAPI*)(int, HidValueCaps*, USHORT*, void*);
    using HidGetUsagesFn      = LONG (WINAPI*)(int, USHORT, USHORT, USHORT*, ULONG*, void*, char*, ULONG);
    using HidGetUsageValueFn  = LONG (WINAPI*)(int, USHORT, USHORT, USHORT, ULONG*, void*, char*, ULONG);
    using HidProductStringFn  = BOOLEAN (WINAPI*)(HANDLE, PVOID, ULONG);

    static constexpr LONG kHidSuccess = 0x00110000;

    struct HidValue {
        USHORT usage = 0;
        USHORT page = 0;
        LONG   min = 0, max = 0;
        USHORT bits = 0;
        int    axis = -1;
        int    hat = -1;
    };

    struct HidDevice : GamepadDevice {
        HANDLE                     handle = nullptr;
        std::vector<unsigned char> preparsed;
        std::vector<HidValue>      values;
        USHORT                     button_page = 9;
        USHORT                     button_min = 1;
        USHORT                     button_max = 0;
    };

    HMODULE    m_xinput = nullptr;
    HMODULE    m_hid = nullptr;
    GetStateFn m_get_state = nullptr;
    SetStateFn m_set_state = nullptr;
    HidGetCapsFn       m_get_caps = nullptr;
    HidGetButtonCapsFn m_get_button_caps = nullptr;
    HidGetValueCapsFn  m_get_value_caps = nullptr;
    HidGetUsagesFn     m_get_usages = nullptr;
    HidGetUsageValueFn m_get_usage_value = nullptr;
    HidProductStringFn m_product_string = nullptr;

    std::unique_ptr<XDevice> m_slots[4];
    std::chrono::steady_clock::time_point m_last_probe[4]{};
    std::vector<std::unique_ptr<HidDevice>> m_hid_devices;
    std::vector<GamepadDevice*> m_pending_added;
    std::vector<int>            m_pending_removed;
    HWND m_hwnd = nullptr;
    int  m_next_key = 1;
    bool m_enumerated = false;

    template <typename F>
    static F proc(HMODULE m, const char* name) noexcept {
        if (!m) return nullptr;
        FARPROC p = GetProcAddress(m, name);
        F f = nullptr;
        std::memcpy(&f, &p, sizeof(f));
        return f;
    }

    static void fill_xinput(XDevice& d, const XState& s) noexcept {
        const WORD b = s.Gamepad.wButtons;
        const WORD bits[11] = { 0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0020, 0x0010, 0x0040, 0x0080, 0x0400 };
        for (int i = 0; i < 11; ++i) d.raw.buttons[static_cast<std::size_t>(i)] = (b & bits[i]) != 0;
        std::uint8_t hat = 0;
        if (b & 0x0001) hat |= 1;
        if (b & 0x0008) hat |= 2;
        if (b & 0x0002) hat |= 4;
        if (b & 0x0004) hat |= 8;
        d.raw.hats[0] = hat;
        auto stick = [](SHORT v) { return std::max(-1.0f, static_cast<float>(v) / 32767.0f); };
        d.raw.axes[0] = stick(s.Gamepad.sThumbLX);
        d.raw.axes[1] = -stick(s.Gamepad.sThumbLY);
        d.raw.axes[2] = stick(s.Gamepad.sThumbRX);
        d.raw.axes[3] = -stick(s.Gamepad.sThumbRY);
        d.raw.axes[4] = s.Gamepad.bLeftTrigger / 127.5f - 1.0f;
        d.raw.axes[5] = s.Gamepad.bRightTrigger / 127.5f - 1.0f;
    }

    void poll_xinput() {
        if (!m_get_state) return;
        const auto now = std::chrono::steady_clock::now();

        for (DWORD i = 0; i < 4; ++i) {
            if (!m_slots[i] && now - m_last_probe[i] < std::chrono::seconds(1)) continue;
            XState s{};
            const DWORD r = m_get_state(i, &s);

            if (r != ERROR_SUCCESS) {
                m_last_probe[i] = now;
                if (m_slots[i]) { m_pending_removed.push_back(m_slots[i]->key); m_slots[i].reset(); }
                continue;
            }

            if (!m_slots[i]) {
                auto d = std::make_unique<XDevice>();
                d->key = m_next_key++;
                d->slot = i;
                d->set_state = m_set_state;
                d->info.name = "XInput Controller #" + std::to_string(i + 1);
                d->info.guid = "xinput";
                d->info.xinput = true;
                d->info.rumble = m_set_state != nullptr;
                d->raw.buttons.assign(11, false);
                d->raw.axes.assign(6, 0.0f);
                d->raw.hats.assign(1, 0);
                parse_gamepad_mapping("xinput,XInput Controller,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,leftstick:b8,rightstick:b9,guide:b10,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,", d->fallback);
                d->has_fallback = true;
                m_pending_added.push_back(d.get());
                m_slots[i] = std::move(d);
            }

            fill_xinput(*m_slots[i], s);
            if (m_slots[i]->rumbling && now >= m_slots[i]->rumble_until) { m_slots[i]->set_rumble(0.0f, 0.0f, 0); m_slots[i]->rumbling = false; }
        }
    }

    HidDevice* find_hid(HANDLE h) noexcept {
        for (auto& d : m_hid_devices) if (d->handle == h) return d.get();
        return nullptr;
    }

    void add_hid(HANDLE h) {
        if (!m_get_caps || find_hid(h)) return;
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfoW(h, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1) || info.dwType != RIM_TYPEHID) return;
        if (info.hid.usUsagePage != 1 || (info.hid.usUsage != 4 && info.hid.usUsage != 5)) return;
        UINT name_len = 0;
        GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, nullptr, &name_len);
        std::wstring path(name_len + 1, L'\0');
        if (name_len && GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, &path[0], &name_len) != static_cast<UINT>(-1)) {
            if (path.find(L"IG_") != std::wstring::npos) return;
        }
        UINT pre = 0;
        GetRawInputDeviceInfoW(h, RIDI_PREPARSEDDATA, nullptr, &pre);
        if (pre == 0) return;
        auto d = std::make_unique<HidDevice>();
        d->handle = h;
        d->preparsed.resize(pre);
        if (GetRawInputDeviceInfoW(h, RIDI_PREPARSEDDATA, d->preparsed.data(), &pre) == static_cast<UINT>(-1)) return;
        void* pp = d->preparsed.data();
        HidCaps caps{};
        if (m_get_caps(pp, &caps) != kHidSuccess) return;

        if (caps.NumberInputButtonCaps > 0) {
            std::vector<HidButtonCaps> bc(caps.NumberInputButtonCaps);
            USHORT n = caps.NumberInputButtonCaps;
            if (m_get_button_caps(0, bc.data(), &n, pp) == kHidSuccess) {
                for (USHORT i = 0; i < n; ++i) {
                    if (bc[i].UsagePage != 9) continue;
                    const USHORT lo = bc[i].IsRange ? bc[i].Range.UsageMin : bc[i].Range.UsageMin;
                    const USHORT hi = bc[i].IsRange ? bc[i].Range.UsageMax : bc[i].Range.UsageMin;
                    d->button_min = d->button_max == 0 ? lo : std::min(d->button_min, lo);
                    d->button_max = std::max(d->button_max, hi);
                }
            }
        }

        if (caps.NumberInputValueCaps > 0) {
            std::vector<HidValueCaps> vc(caps.NumberInputValueCaps);
            USHORT n = caps.NumberInputValueCaps;
            if (m_get_value_caps(0, vc.data(), &n, pp) == kHidSuccess) {
                for (USHORT i = 0; i < n; ++i) {
                    if (vc[i].UsagePage != 1) continue;
                    const USHORT usage = vc[i].IsRange ? vc[i].Range.UsageMin : vc[i].Range.UsageMin;
                    if (usage < 0x30 || usage > 0x39) continue;
                    HidValue v;
                    v.usage = usage;
                    v.page = 1;
                    v.min = vc[i].LogicalMin;
                    v.max = vc[i].LogicalMax;
                    v.bits = vc[i].BitSize;
                    d->values.push_back(v);
                }
            }
        }

        std::sort(d->values.begin(), d->values.end(), [](const HidValue& a, const HidValue& b) { return a.usage < b.usage; });
        int axes = 0, hats = 0;
        for (HidValue& v : d->values) { if (v.usage == 0x39) v.hat = hats++; else v.axis = axes++; }
        d->raw.buttons.assign(d->button_max >= d->button_min && d->button_max ? static_cast<std::size_t>(d->button_max - d->button_min + 1) : 0, false);
        d->raw.axes.assign(static_cast<std::size_t>(axes), 0.0f);
        d->raw.hats.assign(static_cast<std::size_t>(hats), 0);
        d->key = m_next_key++;
        d->info.vendor = static_cast<std::uint16_t>(info.hid.dwVendorId);
        d->info.product = static_cast<std::uint16_t>(info.hid.dwProductId);
        d->info.version = static_cast<std::uint16_t>(info.hid.dwVersionNumber);
        d->info.bus = 3;
        d->info.guid = make_guid(3, d->info.vendor, d->info.product, 0);
        d->info.name = "HID Gamepad";

        if (m_product_string && name_len) {
            HANDLE file = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                wchar_t product[128] = {};
                if (m_product_string(file, product, sizeof(product))) {
                    char utf8[256] = {};
                    WideCharToMultiByte(CP_UTF8, 0, product, -1, utf8, sizeof(utf8) - 1, nullptr, nullptr);
                    if (utf8[0]) d->info.name = utf8;
                }
                CloseHandle(file);
            }
        }

        d->info.path.assign(path.begin(), path.end());
        m_pending_added.push_back(d.get());
        m_hid_devices.push_back(std::move(d));
    }

    void remove_hid(HANDLE h) {
        for (auto it = m_hid_devices.begin(); it != m_hid_devices.end(); ++it) {
            if ((*it)->handle != h) continue;
            m_pending_removed.push_back((*it)->key);
            m_hid_devices.erase(it);
            return;
        }
    }

    void read_hid(HANDLE h, const RAWHID& hid) {
        HidDevice* d = find_hid(h);
        if (!d) { add_hid(h); d = find_hid(h); }
        if (!d || !m_get_usages || !m_get_usage_value) return;
        void* pp = d->preparsed.data();

        for (DWORD r = 0; r < hid.dwCount; ++r) {
            char* report = const_cast<char*>(reinterpret_cast<const char*>(hid.bRawData) + r * hid.dwSizeHid);
            const ULONG len = hid.dwSizeHid;

            if (!d->raw.buttons.empty()) {
                USHORT usages[128];
                ULONG count = 128;
                std::fill(d->raw.buttons.begin(), d->raw.buttons.end(), false);
                if (m_get_usages(0, d->button_page, 0, usages, &count, pp, report, len) == kHidSuccess) {
                    for (ULONG i = 0; i < count; ++i) {
                        if (usages[i] < d->button_min) continue;
                        const std::size_t idx = static_cast<std::size_t>(usages[i] - d->button_min);
                        if (idx < d->raw.buttons.size()) d->raw.buttons[idx] = true;
                    }
                }
            }

            for (const HidValue& v : d->values) {
                ULONG value = 0;
                if (m_get_usage_value(0, v.page, 0, v.usage, &value, pp, report, len) != kHidSuccess) continue;
                LONG sv = static_cast<LONG>(value);
                if (v.min < 0 && v.bits > 0 && v.bits < 32 && (value & (1ul << (v.bits - 1)))) sv = static_cast<LONG>(value | (~0ul << v.bits));

                if (v.hat >= 0) {
                    const LONG range = v.max - v.min + 1;
                    const LONG pos = sv - v.min;
                    static const std::uint8_t dirs[8] = { 1, 1 | 2, 2, 2 | 4, 4, 4 | 8, 8, 8 | 1 };
                    std::uint8_t mask = 0;
                    if (pos >= 0 && pos < range && range >= 4) mask = dirs[(pos * 8 / range) & 7];
                    d->raw.hats[static_cast<std::size_t>(v.hat)] = mask;
                } else if (v.axis >= 0 && v.max > v.min) {
                    const float f = 2.0f * static_cast<float>(sv - v.min) / static_cast<float>(v.max - v.min) - 1.0f;
                    d->raw.axes[static_cast<std::size_t>(v.axis)] = std::max(-1.0f, std::min(1.0f, f));
                }
            }
        }
    }

    static bool hook(void* ctx, unsigned int message, std::uintptr_t wparam, std::intptr_t lparam) {
        auto* self = static_cast<WindowsGamepadBackend*>(ctx);

        if (message == WM_INPUT_DEVICE_CHANGE) {
            HANDLE h = reinterpret_cast<HANDLE>(lparam);
            if (wparam == GIDC_ARRIVAL) self->add_hid(h);
            else if (wparam == GIDC_REMOVAL) self->remove_hid(h);
            return true;
        }

        if (message != WM_INPUT) return false;
        UINT size = 0;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) return false;
        std::vector<unsigned char> buf(size);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) return false;
        const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buf.data());
        if (raw->header.dwType != RIM_TYPEHID) return false;
        self->read_hid(raw->header.hDevice, raw->data.hid);
        return true;
    }

    void enumerate_hid() {
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) return;
        std::vector<RAWINPUTDEVICELIST> list(count);
        if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) return;
        for (UINT i = 0; i < count; ++i) if (list[i].dwType == RIM_TYPEHID) add_hid(list[i].hDevice);
    }

public:
    WindowsGamepadBackend() {
        const wchar_t* dlls[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
        for (const wchar_t* n : dlls) { m_xinput = LoadLibraryExW(n, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32); if (m_xinput) break; }
        if (m_xinput) {
            m_get_state = proc<GetStateFn>(m_xinput, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(100)));
            if (!m_get_state) m_get_state = proc<GetStateFn>(m_xinput, "XInputGetState");
            m_set_state = proc<SetStateFn>(m_xinput, "XInputSetState");
        }
        m_hid = LoadLibraryExW(L"hid.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m_hid) {
            m_get_caps        = proc<HidGetCapsFn>(m_hid, "HidP_GetCaps");
            m_get_button_caps = proc<HidGetButtonCapsFn>(m_hid, "HidP_GetButtonCaps");
            m_get_value_caps  = proc<HidGetValueCapsFn>(m_hid, "HidP_GetValueCaps");
            m_get_usages      = proc<HidGetUsagesFn>(m_hid, "HidP_GetUsages");
            m_get_usage_value = proc<HidGetUsageValueFn>(m_hid, "HidP_GetUsageValue");
            m_product_string  = proc<HidProductStringFn>(m_hid, "HidD_GetProductString");
        }
        ::fizmo::input::detail::add_raw_input_hook(&hook, this);
    }

    ~WindowsGamepadBackend() override {
        ::fizmo::input::detail::remove_raw_input_hook(this);
        for (auto& s : m_slots) if (s && s->rumbling) s->set_rumble(0.0f, 0.0f, 0);
        m_hid_devices.clear();
        if (m_xinput) FreeLibrary(m_xinput);
        if (m_hid) FreeLibrary(m_hid);
    }

    void set_window(void* hwnd) noexcept override {
        m_hwnd = static_cast<HWND>(hwnd);
        if (!m_hwnd || !m_get_caps) return;
        RAWINPUTDEVICE rid[2] = {};
        rid[0].usUsagePage = 1; rid[0].usUsage = 4; rid[0].dwFlags = RIDEV_DEVNOTIFY | RIDEV_INPUTSINK; rid[0].hwndTarget = m_hwnd;
        rid[1].usUsagePage = 1; rid[1].usUsage = 5; rid[1].dwFlags = RIDEV_DEVNOTIFY | RIDEV_INPUTSINK; rid[1].hwndTarget = m_hwnd;
        RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
        if (!m_enumerated) { m_enumerated = true; enumerate_hid(); }
    }

    void poll(std::vector<GamepadDevice*>& added, std::vector<int>& removed) override {
        poll_xinput();
        added.insert(added.end(), m_pending_added.begin(), m_pending_added.end());
        removed.insert(removed.end(), m_pending_removed.begin(), m_pending_removed.end());
        m_pending_added.clear();
        m_pending_removed.clear();
    }
};

} // namespace detail
#endif

} // namespace input
} // namespace fizmo

namespace fizmo {
namespace input {

float Gamepad::shape(float v, float dz) noexcept {
    const float a = std::abs(v);
    if (a <= dz) return 0.0f;
    const float s = (a - dz) / (1.0f - dz);
    return v < 0.0f ? -s : s;
}

float Gamepad::axis(GamepadAxis a) const noexcept {
    const std::size_t i = static_cast<std::size_t>(a);
    if (a == GamepadAxis::LeftTrigger || a == GamepadAxis::RightTrigger) return shape(m_axes[i], m_trigger_deadzone);
    const std::size_t pair = (a == GamepadAxis::LeftX || a == GamepadAxis::LeftY) ? 0 : 2;
    const float x = m_axes[pair], y = m_axes[pair + 1];
    const float len = std::sqrt(x * x + y * y);
    if (len <= m_stick_deadzone) return 0.0f;
    const float scaled = std::min(1.0f, (len - m_stick_deadzone) / (1.0f - m_stick_deadzone));
    return m_axes[i] / len * scaled;
}

void Gamepad::set_deadzones(float stick, float trigger) noexcept {
    m_stick_deadzone = std::max(0.0f, std::min(0.95f, stick));
    m_trigger_deadzone = std::max(0.0f, std::min(0.95f, trigger));
}

bool Gamepad::rumble(float low_frequency, float high_frequency, unsigned int duration_ms) noexcept {
    if (!m_connected || !m_device) return false;
    low_frequency = std::max(0.0f, std::min(1.0f, low_frequency));
    high_frequency = std::max(0.0f, std::min(1.0f, high_frequency));
    if (!m_device->set_rumble(low_frequency, high_frequency, duration_ms)) return false;
    m_device->rumbling = low_frequency > 0.0f || high_frequency > 0.0f;
    m_device->rumble_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
    return true;
}

} // namespace input
} // namespace fizmo

namespace fizmo {
namespace input {
namespace detail {

#if defined(OS_LINUX)
bool LinuxGamepadBackend::Device::set_rumble(float low, float high, unsigned int ms) noexcept {
    if (fd < 0 || !info.rumble) return false;

    if (low <= 0.0f && high <= 0.0f) {
        if (ff_id < 0) return true;
        input_event stop{};
        stop.type = EV_FF;
        stop.code = static_cast<unsigned short>(ff_id);
        stop.value = 0;
        return ::write(fd, &stop, sizeof(stop)) == static_cast<ssize_t>(sizeof(stop));
    }

    ff_effect e{};
    e.type = FF_RUMBLE;
    e.id = static_cast<short>(ff_id);
    e.u.rumble.strong_magnitude = static_cast<unsigned short>(low * 65535.0f);
    e.u.rumble.weak_magnitude = static_cast<unsigned short>(high * 65535.0f);
    e.replay.length = static_cast<unsigned short>(std::min(ms, 65535u));
    e.replay.delay = 0;
    if (::ioctl(fd, EVIOCSFF, &e) < 0) {
        e.id = -1;
        if (::ioctl(fd, EVIOCSFF, &e) < 0) return false;
    }
    ff_id = e.id;
    input_event play{};
    play.type = EV_FF;
    play.code = static_cast<unsigned short>(ff_id);
    play.value = 1;
    return ::write(fd, &play, sizeof(play)) == static_cast<ssize_t>(sizeof(play));
}
#endif

#if defined(OS_LINUX)
float LinuxGamepadBackend::normalize(int v, int lo, int hi) noexcept {
    if (hi <= lo) return 0.0f;
    const float f = 2.0f * static_cast<float>(v - lo) / static_cast<float>(hi - lo) - 1.0f;
    return std::max(-1.0f, std::min(1.0f, f));
}
#endif

#if defined(OS_LINUX)
auto LinuxGamepadBackend::open_device(const std::string& path) -> std::unique_ptr<Device> {
    int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    bool writable = true;
    if (fd < 0) { fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC); writable = false; }
    if (fd < 0) return nullptr;
    constexpr std::size_t per = sizeof(unsigned long) * 8;
    unsigned long ev_bits[(EV_MAX + per) / per] = {};
    unsigned long key_bits[(KEY_MAX + per) / per] = {};
    unsigned long abs_bits[(ABS_MAX + per) / per] = {};
    unsigned long ff_bits[(FF_MAX + per) / per] = {};

    if (::ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) { ::close(fd); return nullptr; }
    ::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
    ::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
    bool joystick_keys = false;
    for (unsigned int k = BTN_JOYSTICK; k < BTN_DIGI; ++k) if (test_bit(key_bits, k)) joystick_keys = true;
    if (test_bit(key_bits, BTN_TRIGGER_HAPPY)) joystick_keys = true;
    const bool pointer = test_bit(key_bits, BTN_TOUCH) || test_bit(key_bits, BTN_TOOL_FINGER) || test_bit(key_bits, BTN_TOOL_PEN) || test_bit(key_bits, BTN_LEFT);
    if (!joystick_keys || pointer || !test_bit(ev_bits, EV_KEY)) { ::close(fd); return nullptr; }

    auto d = std::make_unique<Device>();
    d->fd = fd;
    d->key = m_next_key++;
    d->info.path = path;
    char name[256] = {};
    if (::ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) std::strcpy(name, "Gamepad");
    d->info.name = name;
    input_id id{};
    if (::ioctl(fd, EVIOCGID, &id) == 0) {
        d->info.bus = id.bustype;
        d->info.vendor = id.vendor;
        d->info.product = id.product;
        d->info.version = id.version;
    }
    d->info.guid = make_guid(d->info.bus, d->info.vendor, d->info.product, d->info.version, d->info.name);

    if (writable && test_bit(ev_bits, EV_FF) && ::ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ff_bits)), ff_bits) >= 0) d->info.rumble = test_bit(ff_bits, FF_RUMBLE);

    d->key_index.assign(KEY_MAX + 1, -1);
    int buttons = 0;
    for (unsigned int k = BTN_JOYSTICK; k < KEY_MAX; ++k) if (test_bit(key_bits, k)) d->key_index[k] = buttons++;
    for (unsigned int k = 0; k < BTN_JOYSTICK; ++k) if (test_bit(key_bits, k)) d->key_index[k] = buttons++;
    d->raw.buttons.assign(static_cast<std::size_t>(buttons), false);

    d->abs_index.assign(ABS_MAX + 1, -1);
    d->hat_index.assign(ABS_MAX + 1, -1);
    d->abs_min.assign(ABS_MAX + 1, -1);
    d->abs_max.assign(ABS_MAX + 1, 1);
    int axes = 0, hats = 0;

    for (unsigned int a = 0; a < ABS_MISC; ++a) {
        if (!test_bit(abs_bits, a)) continue;
        if (a >= ABS_HAT0X && a <= ABS_HAT3Y) continue;
        input_absinfo info{};
        if (::ioctl(fd, EVIOCGABS(a), &info) < 0) continue;
        d->abs_index[a] = axes++;
        d->abs_min[a] = info.minimum;
        d->abs_max[a] = info.maximum;
    }

    for (unsigned int h = ABS_HAT0X; h <= ABS_HAT3Y; h += 2) {
        if (!test_bit(abs_bits, h) && !test_bit(abs_bits, h + 1)) continue;
        d->hat_index[h] = d->hat_index[h + 1] = hats++;
    }

    d->raw.axes.assign(static_cast<std::size_t>(axes), 0.0f);
    d->raw.hats.assign(static_cast<std::size_t>(hats), 0);
    build_fallback(*d, key_bits, abs_bits);
    sync(*d);
    return d;
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::bind_button(GamepadMapping& m, GamepadButton target, const Device& d, unsigned int code) {
    if (code > KEY_MAX || d.key_index[code] < 0) return;
    GamepadBinding b;
    b.source = GamepadBinding::Source::Button;
    b.index = d.key_index[code];
    m.buttons[static_cast<std::size_t>(target)] = b;
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::bind_axis(GamepadMapping& m, GamepadAxis target, const Device& d, unsigned int code) {
    if (code > ABS_MAX || d.abs_index[code] < 0) return;
    GamepadBinding b;
    b.source = GamepadBinding::Source::Axis;
    b.index = d.abs_index[code];
    m.axes[static_cast<std::size_t>(target)] = b;
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::build_fallback(Device& d, const unsigned long* key_bits, const unsigned long* abs_bits) {
    GamepadMapping& m = d.fallback;
    m.guid = d.info.guid;
    m.name = d.info.name;
    bind_button(m, GamepadButton::South, d, BTN_SOUTH);
    bind_button(m, GamepadButton::East, d, BTN_EAST);
    bind_button(m, GamepadButton::North, d, BTN_NORTH);
    bind_button(m, GamepadButton::West, d, BTN_WEST);
    bind_button(m, GamepadButton::LeftShoulder, d, BTN_TL);
    bind_button(m, GamepadButton::RightShoulder, d, BTN_TR);
    bind_button(m, GamepadButton::Back, d, BTN_SELECT);
    bind_button(m, GamepadButton::Start, d, BTN_START);
    bind_button(m, GamepadButton::Guide, d, BTN_MODE);
    bind_button(m, GamepadButton::LeftStick, d, BTN_THUMBL);
    bind_button(m, GamepadButton::RightStick, d, BTN_THUMBR);
    bind_button(m, GamepadButton::DpadUp, d, BTN_DPAD_UP);
    bind_button(m, GamepadButton::DpadDown, d, BTN_DPAD_DOWN);
    bind_button(m, GamepadButton::DpadLeft, d, BTN_DPAD_LEFT);
    bind_button(m, GamepadButton::DpadRight, d, BTN_DPAD_RIGHT);
    bind_button(m, GamepadButton::Paddle1, d, BTN_TRIGGER_HAPPY5);
    bind_button(m, GamepadButton::Paddle2, d, BTN_TRIGGER_HAPPY6);
    bind_button(m, GamepadButton::Paddle3, d, BTN_TRIGGER_HAPPY7);
    bind_button(m, GamepadButton::Paddle4, d, BTN_TRIGGER_HAPPY8);
    bind_axis(m, GamepadAxis::LeftX, d, ABS_X);
    bind_axis(m, GamepadAxis::LeftY, d, ABS_Y);

    if (test_bit(abs_bits, ABS_RX)) {
        bind_axis(m, GamepadAxis::RightX, d, ABS_RX);
        bind_axis(m, GamepadAxis::RightY, d, ABS_RY);
        bind_axis(m, GamepadAxis::LeftTrigger, d, ABS_Z);
        bind_axis(m, GamepadAxis::RightTrigger, d, ABS_RZ);
    } else {
        bind_axis(m, GamepadAxis::RightX, d, ABS_Z);
        bind_axis(m, GamepadAxis::RightY, d, ABS_RZ);
        bind_axis(m, GamepadAxis::LeftTrigger, d, ABS_BRAKE);
        bind_axis(m, GamepadAxis::RightTrigger, d, ABS_GAS);
    }

    for (int t = 0; t < 2; ++t) {
        const GamepadAxis axis = t == 0 ? GamepadAxis::LeftTrigger : GamepadAxis::RightTrigger;
        const unsigned int code = t == 0 ? BTN_TL2 : BTN_TR2;
        if (m.axes[static_cast<std::size_t>(axis)].source != GamepadBinding::Source::None || d.key_index[code] < 0) continue;
        GamepadBinding b;
        b.source = GamepadBinding::Source::Button;
        b.index = d.key_index[code];
        b.input_half = GamepadBinding::Half::Positive;
        m.axes[static_cast<std::size_t>(axis)] = b;
    }

    if (d.hat_index[ABS_HAT0X] >= 0 && m.buttons[static_cast<std::size_t>(GamepadButton::DpadUp)].source == GamepadBinding::Source::None) {
        const std::uint8_t masks[4] = { 1, 4, 8, 2 };
        const GamepadButton targets[4] = { GamepadButton::DpadUp, GamepadButton::DpadDown, GamepadButton::DpadLeft, GamepadButton::DpadRight };
        for (int i = 0; i < 4; ++i) {
            GamepadBinding b;
            b.source = GamepadBinding::Source::Hat;
            b.index = d.hat_index[ABS_HAT0X];
            b.hat_mask = masks[i];
            m.buttons[static_cast<std::size_t>(targets[i])] = b;
        }
    }

    d.has_fallback = m.buttons[0].source != GamepadBinding::Source::None && m.axes[0].source != GamepadBinding::Source::None;
    (void)key_bits;
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::set_hat(Device& d, unsigned int code, int value) noexcept {
    const int h = d.hat_index[code];
    if (h < 0 || static_cast<std::size_t>(h) >= d.raw.hats.size()) return;
    std::uint8_t& hat = d.raw.hats[static_cast<std::size_t>(h)];
    const bool x = ((code - ABS_HAT0X) % 2) == 0;
    if (x) { hat &= static_cast<std::uint8_t>(~(2 | 8)); if (value < 0) hat |= 8; else if (value > 0) hat |= 2; }
    else   { hat &= static_cast<std::uint8_t>(~(1 | 4)); if (value < 0) hat |= 1; else if (value > 0) hat |= 4; }
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::sync(Device& d) noexcept {
    constexpr std::size_t per = sizeof(unsigned long) * 8;
    unsigned long keys[(KEY_MAX + per) / per] = {};
    if (::ioctl(d.fd, EVIOCGKEY(sizeof(keys)), keys) >= 0) {
        for (unsigned int k = 0; k <= KEY_MAX; ++k) {
            const int i = d.key_index[k];
            if (i >= 0) d.raw.buttons[static_cast<std::size_t>(i)] = test_bit(keys, k);
        }
    }

    for (unsigned int a = 0; a <= ABS_MAX; ++a) {
        if (d.abs_index[a] < 0 && d.hat_index[a] < 0) continue;
        input_absinfo info{};
        if (::ioctl(d.fd, EVIOCGABS(a), &info) < 0) continue;
        if (d.abs_index[a] >= 0) d.raw.axes[static_cast<std::size_t>(d.abs_index[a])] = normalize(info.value, d.abs_min[a], d.abs_max[a]);
        else set_hat(d, a, info.value);
    }
}
#endif

#if defined(OS_LINUX)
bool LinuxGamepadBackend::read_device(Device& d) noexcept {
    input_event events[64];

    for (;;) {
        const ssize_t n = ::read(d.fd, events, sizeof(events));
        if (n < 0) return errno == EAGAIN || errno == EINTR;
        if (n == 0) return false;
        const std::size_t count = static_cast<std::size_t>(n) / sizeof(input_event);

        for (std::size_t i = 0; i < count; ++i) {
            const input_event& e = events[i];
            if (e.type == EV_KEY && e.code <= KEY_MAX) {
                const int idx = d.key_index[e.code];
                if (idx >= 0) d.raw.buttons[static_cast<std::size_t>(idx)] = e.value != 0;
            } else if (e.type == EV_ABS && e.code <= ABS_MAX) {
                if (d.abs_index[e.code] >= 0) d.raw.axes[static_cast<std::size_t>(d.abs_index[e.code])] = normalize(e.value, d.abs_min[e.code], d.abs_max[e.code]);
                else set_hat(d, e.code, e.value);
            } else if (e.type == EV_SYN && e.code == SYN_DROPPED) {
                sync(d);
            }
        }
    }
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::scan(std::vector<GamepadDevice*>& added) {
    const std::string dir = m_root + "/dev/input";
    DIR* d = ::opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = ::readdir(d)) if (std::strncmp(e->d_name, "event", 5) == 0) names.push_back(e->d_name);
    ::closedir(d);
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return std::atoi(a.c_str() + 5) < std::atoi(b.c_str() + 5); });

    for (const std::string& n : names) {
        const std::string path = dir + "/" + n;
        if (known(path)) continue;
        auto dev = open_device(path);
        if (!dev) continue;
        added.push_back(dev.get());
        m_devices.push_back(std::move(dev));
    }
}
#endif

#if defined(OS_LINUX)
LinuxGamepadBackend::LinuxGamepadBackend(const std::string& root) : m_root(root) {
    m_inotify = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotify >= 0) m_watch = ::inotify_add_watch(m_inotify, (m_root + "/dev/input").c_str(), IN_CREATE | IN_ATTRIB | IN_DELETE | IN_MOVED_TO);
}
#endif

#if defined(OS_LINUX)
void LinuxGamepadBackend::poll(std::vector<GamepadDevice*>& added, std::vector<int>& removed) {
    if (m_inotify >= 0) {
        alignas(inotify_event) char buf[4096];
        for (;;) {
            const ssize_t n = ::read(m_inotify, buf, sizeof(buf));
            if (n <= 0) break;
            m_rescan = true;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_rescan || now - m_last_scan > std::chrono::seconds(3)) {
        m_rescan = false;
        m_last_scan = now;
        scan(added);
    }

    for (auto it = m_devices.begin(); it != m_devices.end();) {
        Device& d = **it;
        if (!read_device(d)) {
            removed.push_back(d.key);
            it = m_devices.erase(it);
            continue;
        }
        if (d.rumbling && now >= d.rumble_until) { d.set_rumble(0.0f, 0.0f, 0); d.rumbling = false; }
        ++it;
    }
}
#endif

} // namespace detail
} // namespace input
} // namespace fizmo

namespace fizmo {
namespace input {

auto GamepadManager::claim_slot() -> Gamepad& {
    for (Gamepad& g : m_pads) if (!g.m_connected && g.m_key == 0) return g;
    for (Gamepad& g : m_pads) if (!g.m_connected) return g;
    m_pads.emplace_back();
    m_pads.back().m_slot = static_cast<int>(m_pads.size() - 1);
    return m_pads.back();
}

void GamepadManager::resolve_mapping(Gamepad& g) {
    g.m_mapping = m_db.find(g.m_info.guid);
    if (!g.m_mapping && g.m_device && g.m_device->has_fallback) g.m_mapping = &g.m_device->fallback;
    g.m_mapped = g.m_mapping != nullptr;
}

void GamepadManager::emit(windows::WindowEventType type, const Gamepad& g, int index, float value) {
    if (!m_events) return;
    windows::WindowEvent e;
    e.type = type;
    e.gamepad = g.m_slot;
    e.axis_value = value;
    e.text = g.m_info.name;
    if (type == windows::WindowEventType::GamepadButtonDown || type == windows::WindowEventType::GamepadButtonUp) e.gamepad_button = static_cast<GamepadButton>(index);
    if (type == windows::WindowEventType::GamepadAxisMotion) e.gamepad_axis = static_cast<GamepadAxis>(index);
    m_events->dispatch_event(e);
}

GamepadManager::GamepadManager() {
    m_db.load_environment();
#if defined(OS_LINUX)
    m_backend = std::make_unique<detail::LinuxGamepadBackend>();
#elif defined(OS_WINDOWS)
    m_backend = std::make_unique<detail::WindowsGamepadBackend>();
#endif
}

bool GamepadManager::add_mapping(const std::string& line) {
    const bool ok = m_db.add_mapping(line);
    for (Gamepad& g : m_pads) if (g.m_connected) resolve_mapping(g);
    return ok;
}

bool GamepadManager::remove_mapping(const std::string& guid) {
    const bool ok = m_db.remove(guid);
    for (Gamepad& g : m_pads) if (g.m_connected) resolve_mapping(g);
    return ok;
}

void GamepadManager::set_deadzones(float stick, float trigger) noexcept {
    m_stick_dz = stick;
    m_trigger_dz = trigger;
    for (Gamepad& g : m_pads) g.set_deadzones(stick, trigger);
}

std::size_t GamepadManager::add_mappings(const std::string& text) {
    const std::size_t n = m_db.add_mappings(text);
    for (Gamepad& g : m_pads) if (g.m_connected) resolve_mapping(g);
    return n;
}

std::size_t GamepadManager::load_mappings(const std::string& path) {
    const std::size_t n = m_db.load_file(path);
    for (Gamepad& g : m_pads) if (g.m_connected) resolve_mapping(g);
    return n;
}

void GamepadManager::update() {
    if (!m_backend) return;
    m_added.clear();
    m_removed.clear();
    m_backend->poll(m_added, m_removed);

    for (int key : m_removed) {
        for (Gamepad& g : m_pads) {
            if (!g.m_connected || g.m_key != key) continue;
            g.m_connected = false;
            g.m_device = nullptr;
            g.m_buttons.fill(false);
            g.m_axes.fill(0.0f);
            emit(windows::WindowEventType::GamepadDisconnected, g, 0, 0.0f);
            if (m_on_disconnected) m_on_disconnected(g);
            g.m_key = 0;
        }
    }

    for (detail::GamepadDevice* d : m_added) {
        Gamepad& g = claim_slot();
        g.m_key = d->key;
        g.m_device = d;
        g.m_info = d->info;
        g.m_connected = true;
        g.m_buttons.fill(false);
        g.m_previous.fill(false);
        g.m_axes.fill(0.0f);
        g.m_last_sent.fill(0.0f);
        g.set_deadzones(m_stick_dz, m_trigger_dz);
        resolve_mapping(g);
        emit(windows::WindowEventType::GamepadConnected, g, 0, 0.0f);
        if (m_on_connected) m_on_connected(g);
    }

    for (Gamepad& g : m_pads) {
        g.m_previous = g.m_buttons;
        if (!g.m_connected || !g.m_device) continue;
        if (g.m_mapping) apply_mapping(*g.m_mapping, g.m_device->raw, g.m_buttons, g.m_axes);

        for (std::size_t i = 0; i < kGamepadButtonCount; ++i) {
            if (g.m_buttons[i] == g.m_previous[i]) continue;
            emit(g.m_buttons[i] ? windows::WindowEventType::GamepadButtonDown : windows::WindowEventType::GamepadButtonUp, g, static_cast<int>(i), g.m_buttons[i] ? 1.0f : 0.0f);
        }

        for (std::size_t i = 0; i < kGamepadAxisCount; ++i) {
            const float v = g.axis(static_cast<GamepadAxis>(i));
            if (std::abs(v - g.m_last_sent[i]) < m_axis_event_threshold && !(v == 0.0f && g.m_last_sent[i] != 0.0f)) continue;
            g.m_last_sent[i] = v;
            emit(windows::WindowEventType::GamepadAxisMotion, g, static_cast<int>(i), v);
        }
    }
}

auto GamepadManager::get(int slot) noexcept -> Gamepad* { return slot >= 0 && static_cast<std::size_t>(slot) < m_pads.size() ? &m_pads[static_cast<std::size_t>(slot)] : nullptr; }

auto GamepadManager::get(int slot) const noexcept -> const Gamepad* { return slot >= 0 && static_cast<std::size_t>(slot) < m_pads.size() ? &m_pads[static_cast<std::size_t>(slot)] : nullptr; }

auto GamepadManager::connected() -> std::vector<Gamepad*> {
    std::vector<Gamepad*> out;
    for (Gamepad& g : m_pads) if (g.m_connected) out.push_back(&g);
    return out;
}

} // namespace input
} // namespace fizmo
