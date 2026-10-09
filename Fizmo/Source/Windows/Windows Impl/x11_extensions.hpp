#ifndef FIZMO_X11_EXTENSIONS_HPP
#define FIZMO_X11_EXTENSIONS_HPP

#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_LINUX

#include "../../x11_compat.hpp"
#include "../window_types.hpp"
#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {
namespace x11ext {

template <typename F>
inline F symbol(void* lib, const char* name) noexcept {
    if (!lib) return nullptr;
    void* p = dlsym(lib, name);
    F f = nullptr;
    std::memcpy(&f, &p, sizeof(f));
    return f;
}

inline constexpr int kXIAllDevices       = 0;
inline constexpr int kXIAllMasterDevices = 1;
inline constexpr int kXI_HierarchyChanged = 11;
inline constexpr int kXI_ButtonPress     = 4;
inline constexpr int kXI_ButtonRelease   = 5;
inline constexpr int kXI_Motion          = 6;
inline constexpr int kXI_RawMotion       = 17;
inline constexpr int kXI_TouchBegin      = 18;
inline constexpr int kXI_TouchUpdate     = 19;
inline constexpr int kXI_TouchEnd        = 20;
inline constexpr int kXI_LastEvent       = 26;
inline constexpr int kXIValuatorClass    = 2;
inline constexpr int kXITouchClass       = 8;
inline constexpr int kXIMasterPointer    = 1;
inline constexpr int kXISlavePointer     = 3;
inline constexpr int kXIFloatingSlave    = 5;
inline constexpr int kXITouchEmulatingPointer = 1 << 16;

inline constexpr int mask_len(int event) noexcept { return (event >> 3) + 1; }
inline void set_mask(unsigned char* mask, int event) noexcept { mask[event >> 3] |= static_cast<unsigned char>(1 << (event & 7)); }
inline bool mask_is_set(const unsigned char* mask, int bit) noexcept { return (mask[bit >> 3] & (1 << (bit & 7))) != 0; }

struct XIEventMask {
    int            deviceid;
    int            mask_len;
    unsigned char* mask;
};

struct XIValuatorState {
    int            mask_len;
    unsigned char* mask;
    double*        values;
};

struct XIButtonState {
    int            mask_len;
    unsigned char* mask;
};

struct XIModifierState {
    int base;
    int latched;
    int locked;
    int effective;
};

using XIGroupState = XIModifierState;

struct XIRawEvent {
    int             type;
    unsigned long   serial;
    int             send_event;
    ::Display*      display;
    int             extension;
    int             evtype;
    ::Time          time;
    int             deviceid;
    int             sourceid;
    int             detail;
    int             flags;
    XIValuatorState valuators;
    double*         raw_values;
};

struct XIDeviceEvent {
    int             type;
    unsigned long   serial;
    int             send_event;
    ::Display*      display;
    int             extension;
    int             evtype;
    ::Time          time;
    int             deviceid;
    int             sourceid;
    int             detail;
    ::Window        root;
    ::Window        event;
    ::Window        child;
    double          root_x;
    double          root_y;
    double          event_x;
    double          event_y;
    int             flags;
    XIButtonState   buttons;
    XIValuatorState valuators;
    XIModifierState mods;
    XIGroupState    group;
};

struct XIAnyClassInfo {
    int type;
    int sourceid;
};

struct XIValuatorClassInfo {
    int    type;
    int    sourceid;
    int    number;
    ::Atom label;
    double min;
    double max;
    double value;
    int    resolution;
    int    mode;
};

struct XITouchClassInfo {
    int type;
    int sourceid;
    int mode;
    int num_touches;
};

struct XIDeviceInfo {
    int              deviceid;
    char*            name;
    int              use;
    int              attachment;
    int              enabled;
    int              num_classes;
    XIAnyClassInfo** classes;
};

class XInput2 {
public:
    using QueryVersionFn = int (*)(::Display*, int*, int*);
    using SelectEventsFn = int (*)(::Display*, ::Window, XIEventMask*, int);
    using QueryDeviceFn  = XIDeviceInfo* (*)(::Display*, int, int*);
    using FreeDeviceFn   = void (*)(XIDeviceInfo*);

    void*          lib    = nullptr;
    int            opcode = -1;
    int            major  = 0;
    int            minor  = 0;
    SelectEventsFn select = nullptr;
    QueryDeviceFn  query  = nullptr;
    FreeDeviceFn   free_info = nullptr;

    bool load(::Display* display) noexcept {
        int event = 0, error = 0;
        if (!display || !XQueryExtension(display, "XInputExtension", &opcode, &event, &error)) return false;
        lib = dlopen("libXi.so.6", RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
        if (!lib) return false;
        const QueryVersionFn version = symbol<QueryVersionFn>(lib, "XIQueryVersion");
        select    = symbol<SelectEventsFn>(lib, "XISelectEvents");
        query     = symbol<QueryDeviceFn>(lib, "XIQueryDevice");
        free_info = symbol<FreeDeviceFn>(lib, "XIFreeDeviceInfo");
        major = 2;
        minor = 2;
        if (!version || !select || version(display, &major, &minor) != 0) {
            major = 2;
            minor = 0;
            if (!version || !select || version(display, &major, &minor) != 0) { unload(); return false; }
        }
        return true;
    }

    bool supports_touch() const noexcept { return lib && (major > 2 || (major == 2 && minor >= 2)); }

    void unload() noexcept {
        if (lib) dlclose(lib);
        lib = nullptr;
        select = nullptr;
        query = nullptr;
        free_info = nullptr;
    }

    ~XInput2() { unload(); }
};

struct XRRMonitorInfo {
    ::Atom name;
    int    primary;
    int    automatic;
    int    noutput;
    int    x;
    int    y;
    int    width;
    int    height;
    int    mwidth;
    int    mheight;
    ::XID* outputs;
};

struct XRRModeInfo {
    ::XID         id;
    unsigned int  width;
    unsigned int  height;
    unsigned long dotClock;
    unsigned int  hSyncStart;
    unsigned int  hSyncEnd;
    unsigned int  hTotal;
    unsigned int  hSkew;
    unsigned int  vSyncStart;
    unsigned int  vSyncEnd;
    unsigned int  vTotal;
    char*         name;
    unsigned int  nameLength;
    unsigned long modeFlags;
};

struct XRRScreenResources {
    ::Time       timestamp;
    ::Time       configTimestamp;
    int          ncrtc;
    ::XID*       crtcs;
    int          noutput;
    ::XID*       outputs;
    int          nmode;
    XRRModeInfo* modes;
};

struct XRROutputInfo {
    ::Time         timestamp;
    ::XID          crtc;
    char*          name;
    int            nameLen;
    unsigned long  mm_width;
    unsigned long  mm_height;
    unsigned short connection;
    unsigned short subpixel_order;
    int            ncrtc;
    ::XID*         crtcs;
    int            nclone;
    ::XID*         clones;
    int            nmode;
    int            npreferred;
    ::XID*         modes;
};

struct XRRCrtcInfo {
    ::Time         timestamp;
    int            x;
    int            y;
    unsigned int   width;
    unsigned int   height;
    ::XID          mode;
    unsigned short rotation;
    int            noutput;
    ::XID*         outputs;
    unsigned short rotations;
    int            npossible;
    ::XID*         possible;
};

class XRandR {
public:
    using QueryExtensionFn = int (*)(::Display*, int*, int*);
    using QueryVersionFn   = int (*)(::Display*, int*, int*);
    using GetMonitorsFn    = XRRMonitorInfo* (*)(::Display*, ::Window, int, int*);
    using FreeMonitorsFn   = void (*)(XRRMonitorInfo*);
    using GetResourcesFn   = XRRScreenResources* (*)(::Display*, ::Window);
    using FreeResourcesFn  = void (*)(XRRScreenResources*);
    using GetOutputFn      = XRROutputInfo* (*)(::Display*, XRRScreenResources*, ::XID);
    using FreeOutputFn     = void (*)(XRROutputInfo*);
    using GetCrtcFn        = XRRCrtcInfo* (*)(::Display*, XRRScreenResources*, ::XID);
    using FreeCrtcFn       = void (*)(XRRCrtcInfo*);
    using GetPrimaryFn     = ::XID (*)(::Display*, ::Window);
    using SelectInputFn    = void (*)(::Display*, ::Window, int);

    void*           lib = nullptr;
    int             event_base = 0;
    int             error_base = 0;
    GetMonitorsFn   get_monitors   = nullptr;
    FreeMonitorsFn  free_monitors  = nullptr;
    GetResourcesFn  get_resources  = nullptr;
    FreeResourcesFn free_resources = nullptr;
    GetOutputFn     get_output     = nullptr;
    FreeOutputFn    free_output    = nullptr;
    GetCrtcFn       get_crtc       = nullptr;
    FreeCrtcFn      free_crtc      = nullptr;
    GetPrimaryFn    get_primary    = nullptr;
    SelectInputFn   select_input   = nullptr;

    bool load(::Display* display) noexcept {
        lib = dlopen("libXrandr.so.2", RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
        if (!lib) return false;
        const QueryExtensionFn query = symbol<QueryExtensionFn>(lib, "XRRQueryExtension");
        if (!query || !query(display, &event_base, &error_base)) { unload(); return false; }
        get_monitors   = symbol<GetMonitorsFn>(lib, "XRRGetMonitors");
        free_monitors  = symbol<FreeMonitorsFn>(lib, "XRRFreeMonitors");
        get_resources  = symbol<GetResourcesFn>(lib, "XRRGetScreenResourcesCurrent");
        free_resources = symbol<FreeResourcesFn>(lib, "XRRFreeScreenResources");
        get_output     = symbol<GetOutputFn>(lib, "XRRGetOutputInfo");
        free_output    = symbol<FreeOutputFn>(lib, "XRRFreeOutputInfo");
        get_crtc       = symbol<GetCrtcFn>(lib, "XRRGetCrtcInfo");
        free_crtc      = symbol<FreeCrtcFn>(lib, "XRRFreeCrtcInfo");
        get_primary    = symbol<GetPrimaryFn>(lib, "XRRGetOutputPrimary");
        select_input   = symbol<SelectInputFn>(lib, "XRRSelectInput");
        return true;
    }

    void unload() noexcept {
        if (lib) dlclose(lib);
        lib = nullptr;
    }

    ~XRandR() { unload(); }
};

struct XcursorImage {
    unsigned int  version;
    unsigned int  size;
    unsigned int  width;
    unsigned int  height;
    unsigned int  xhot;
    unsigned int  yhot;
    unsigned int  delay;
    unsigned int* pixels;
};

class Xcursor {
public:
    using LoadFn    = ::Cursor (*)(::Display*, const char*);
    using CreateFn  = XcursorImage* (*)(int, int);
    using DestroyFn = void (*)(XcursorImage*);
    using ImageFn   = ::Cursor (*)(::Display*, const XcursorImage*);

    void*     lib     = nullptr;
    LoadFn    load_named = nullptr;
    CreateFn  create  = nullptr;
    DestroyFn destroy = nullptr;
    ImageFn   from_image = nullptr;

    bool load() noexcept {
        lib = dlopen("libXcursor.so.1", RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
        if (!lib) return false;
        load_named = symbol<LoadFn>(lib, "XcursorLibraryLoadCursor");
        create     = symbol<CreateFn>(lib, "XcursorImageCreate");
        destroy    = symbol<DestroyFn>(lib, "XcursorImageDestroy");
        from_image = symbol<ImageFn>(lib, "XcursorImageLoadCursor");
        return true;
    }

    ~Xcursor() { if (lib) dlclose(lib); }
};

inline double refresh_of(const XRRModeInfo& m) noexcept {
    if (m.hTotal == 0 || m.vTotal == 0) return 0.0;
    double v = static_cast<double>(m.vTotal);
    if (m.modeFlags & 0x20u) v *= 2.0;
    if (m.modeFlags & 0x10u) v /= 2.0;
    return static_cast<double>(m.dotClock) / (static_cast<double>(m.hTotal) * v);
}

inline float xft_dpi(::Display* display) noexcept {
    const char* rms = display ? XResourceManagerString(display) : nullptr;
    if (rms) {
        const char* p = std::strstr(rms, "Xft.dpi:");
        if (p) {
            const double dpi = std::strtod(p + 8, nullptr);
            if (dpi > 10.0 && dpi < 1000.0) return static_cast<float>(dpi);
        }
    }
    const char* env = std::getenv("FIZMO_DPI_SCALE");
    if (env) {
        const double s = std::strtod(env, nullptr);
        if (s > 0.1 && s < 10.0) return static_cast<float>(96.0 * s);
    }
    return 96.0f;
}

inline bool work_area(::Display* display, Rect& out) noexcept {
    const ::Atom atom = XInternAtom(display, "_NET_WORKAREA", 1);
    if (atom == 0) return false;
    ::Atom type = 0;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char* data = nullptr;
    const int r = XGetWindowProperty(display, DefaultRootWindow(display), atom, 0, 4, 0, XA_CARDINAL, &type, &format, &count, &after, &data);
    bool ok = false;
    if (r == 0 && data && format == 32 && count >= 4) {
        const long* v = reinterpret_cast<const long*>(data);
        out = Rect{ static_cast<int>(v[0]), static_cast<int>(v[1]), static_cast<unsigned int>(v[2]), static_cast<unsigned int>(v[3]) };
        ok = true;
    }
    if (data) XFree(data);
    return ok;
}

inline Rect intersect(const Rect& a, const Rect& b) noexcept {
    const int x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const int x1 = std::min(a.x + static_cast<int>(a.width), b.x + static_cast<int>(b.width));
    const int y1 = std::min(a.y + static_cast<int>(a.height), b.y + static_cast<int>(b.height));
    if (x1 <= x0 || y1 <= y0) return a;
    return Rect{ x0, y0, static_cast<unsigned int>(x1 - x0), static_cast<unsigned int>(y1 - y0) };
}

inline std::vector<MonitorInfo> query_monitors(::Display* display) {
    std::vector<MonitorInfo> out;
    if (!display) return out;
    const ::Window root = DefaultRootWindow(display);
    const float dpi = xft_dpi(display);
    Rect desktop_work;
    const bool have_work = work_area(display, desktop_work);
    XRandR rr;

    if (rr.load(display) && rr.get_monitors && rr.get_resources) {
        int count = 0;
        XRRMonitorInfo* mons = rr.get_monitors(display, root, 1, &count);
        XRRScreenResources* res = rr.get_resources(display, root);

        for (int i = 0; mons && i < count; ++i) {
            const XRRMonitorInfo& m = mons[i];
            MonitorInfo info;
            char* name = m.name ? XGetAtomName(display, m.name) : nullptr;
            info.name = name ? name : "Monitor";
            if (name) XFree(name);
            info.bounds = Rect{ m.x, m.y, static_cast<unsigned int>(m.width), static_cast<unsigned int>(m.height) };
            info.work_area = have_work ? intersect(info.bounds, desktop_work) : info.bounds;
            info.primary = m.primary != 0;
            info.physical_width_mm = static_cast<unsigned int>(m.mwidth);
            info.physical_height_mm = static_cast<unsigned int>(m.mheight);
            info.dpi = dpi;
            info.scale = dpi / 96.0f;
            info.current = DisplayMode{ info.bounds.width, info.bounds.height, 0.0, 32 };
            info.handle = m.noutput > 0 && m.outputs ? static_cast<std::uintptr_t>(m.outputs[0]) : 0;

            if (res && rr.get_output && m.noutput > 0 && m.outputs) {
                XRROutputInfo* oi = rr.get_output(display, res, m.outputs[0]);

                if (oi) {
                    for (int k = 0; k < oi->nmode; ++k) {
                        for (int j = 0; j < res->nmode; ++j) {
                            if (res->modes[j].id != oi->modes[k]) continue;
                            DisplayMode dm{ res->modes[j].width, res->modes[j].height, refresh_of(res->modes[j]), 32 };
                            bool dup = false;
                            for (const DisplayMode& e : info.modes) if (e.width == dm.width && e.height == dm.height && std::abs(e.refresh_rate - dm.refresh_rate) < 0.01) dup = true;
                            if (!dup) info.modes.push_back(dm);
                        }
                    }

                    if (oi->crtc && rr.get_crtc) {
                        XRRCrtcInfo* ci = rr.get_crtc(display, res, oi->crtc);
                        if (ci) {
                            for (int j = 0; j < res->nmode; ++j) if (res->modes[j].id == ci->mode) info.current.refresh_rate = refresh_of(res->modes[j]);
                            if (rr.free_crtc) rr.free_crtc(ci);
                        }
                    }

                    if (rr.free_output) rr.free_output(oi);
                }
            }

            out.push_back(std::move(info));
        }

        if (res && rr.free_resources) rr.free_resources(res);
        if (mons && rr.free_monitors) rr.free_monitors(mons);
    }

    if (out.empty()) {
        MonitorInfo info;
        const int screen = DefaultScreen(display);
        info.name = "Screen";
        info.bounds = Rect{ 0, 0, static_cast<unsigned int>(DisplayWidth(display, screen)), static_cast<unsigned int>(DisplayHeight(display, screen)) };
        info.work_area = have_work ? intersect(info.bounds, desktop_work) : info.bounds;
        info.physical_width_mm = static_cast<unsigned int>(DisplayWidthMM(display, screen));
        info.physical_height_mm = static_cast<unsigned int>(DisplayHeightMM(display, screen));
        info.primary = true;
        info.dpi = dpi;
        info.scale = dpi / 96.0f;
        info.current = DisplayMode{ info.bounds.width, info.bounds.height, 0.0, static_cast<unsigned int>(DefaultDepth(display, screen)) };
        out.push_back(info);
    }

    bool any_primary = false;
    for (const MonitorInfo& m : out) any_primary = any_primary || m.primary;
    if (!any_primary && !out.empty()) out.front().primary = true;
    std::stable_sort(out.begin(), out.end(), [](const MonitorInfo& a, const MonitorInfo& b) { return a.primary && !b.primary; });
    return out;
}

} // namespace x11ext
} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_X11_EXTENSIONS_HPP
