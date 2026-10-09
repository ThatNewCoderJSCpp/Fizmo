#ifndef FIZMO_X11_COMPAT_HPP
#define FIZMO_X11_COMPAT_HPP

#include <cstdint>
#include "Basic/platform_includes.hpp"

#ifdef OS_LINUX

namespace fizmo {
namespace x11 {

using XDisplay    = ::Display;
using XWindowId   = ::XID;
using XPixmapId   = ::XID;
using XDrawableId = ::XID;
using XColormapId = ::XID;
using XAtomId     = ::Atom;
using XKeySym     = ::KeySym;
using XGCHandle   = ::GC;
using XVisual     = ::Visual;

inline constexpr unsigned long kNone      = None;
inline constexpr unsigned long kAllPlanes = AllPlanes;
inline constexpr int kUnsorted            = Unsorted;
inline constexpr int kZPixmap             = ZPixmap;
inline constexpr int kArcPieSlice         = ArcPieSlice;
inline constexpr int kArcChord            = ArcChord;
inline constexpr int kLineSolid           = LineSolid;
inline constexpr int kCapButt             = CapButt;
inline constexpr int kCapRound            = CapRound;
inline constexpr int kCapProjecting       = CapProjecting;
inline constexpr int kJoinMiter           = JoinMiter;
inline constexpr int kJoinRound           = JoinRound;
inline constexpr int kJoinBevel           = JoinBevel;

inline constexpr int kKeyPress        = KeyPress;
inline constexpr int kKeyRelease      = KeyRelease;
inline constexpr int kButtonPress     = ButtonPress;
inline constexpr int kButtonRelease   = ButtonRelease;
inline constexpr int kMotionNotify    = MotionNotify;
inline constexpr int kEnterNotify     = EnterNotify;
inline constexpr int kLeaveNotify     = LeaveNotify;
inline constexpr int kFocusIn         = FocusIn;
inline constexpr int kFocusOut        = FocusOut;
inline constexpr int kExpose          = Expose;
inline constexpr int kConfigureNotify = ConfigureNotify;
inline constexpr int kDestroyNotify   = DestroyNotify;
inline constexpr int kMapNotify       = MapNotify;
inline constexpr int kClientMessage   = ClientMessage;
inline constexpr int kTrue            = True;
inline constexpr int kFalse           = False;

inline unsigned long pack_color(XVisual* v, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    if (!v) return 0;
    auto ch = [](unsigned long mask, std::uint8_t value) -> unsigned long {
        if (mask == 0) return 0;
        int shift = 0;
        unsigned long m = mask;
        while ((m & 1UL) == 0UL) { m >>= 1; ++shift; }
        return ((static_cast<unsigned long>(value) * m) / 255UL) << shift;
    };

    return ch(v->red_mask, r) | ch(v->green_mask, g) | ch(v->blue_mask, b);
}

struct Handle {
    XDisplay* display = nullptr;
    XWindowId window  = 0;
};

} // namespace x11
} // namespace fizmo

#undef None
#undef Status
#undef Success
#undef Bool
#undef True
#undef False
#undef Always
#undef Above
#undef Below
#undef Complex
#undef Convex
#undef Unsorted
#undef ZPixmap
#undef AllPlanes
#undef KeyPress
#undef KeyRelease
#undef ButtonPress
#undef ButtonRelease
#undef MotionNotify
#undef EnterNotify
#undef LeaveNotify
#undef FocusIn
#undef FocusOut
#undef Expose
#undef ConfigureNotify
#undef DestroyNotify
#undef CursorShape

#endif // OS_LINUX
#endif // FIZMO_X11_COMPAT_HPP