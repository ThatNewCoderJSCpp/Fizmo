#include "fizmo_library.hpp"
#include "window_macos_impl.hpp"
#include "window_factory.hpp"
#include "../../macos_compat.hpp"
#include "../../Input/keys.hpp"

#ifdef OS_MACOS

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Carbon/Carbon.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <string>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {
class WindowImpl;
} // namespace detail
} // namespace windows
} // namespace fizmo

@class FizmoView;

namespace fizmo {
namespace windows {
namespace detail {

struct MacWindowState {
    WindowImpl*            owner    = nullptr;
    NSWindow*              window   = nil;
    FizmoView*             view     = nil;
    id                     delegate = nil;
    CAMetalLayer*          layer    = nil;
    macos::Handle          handle;
    bool                   open     = false;
    bool                   expose_pending = false;
    std::function<void(void*)> paint;
    fizmo::colors::Color   background;
    unsigned int           last_w = 0;
    unsigned int           last_h = 0;
    int                    last_x = 0;
    int                    last_y = 0;
    bool                   locked = false;
    bool                   cursor_visible = true;
    bool                   cursor_hidden = false;
    NSCursor*              cursor = nil;
    bool                   text_active = true;
    int                    text_x = 0;
    int                    text_y = 0;
    unsigned int           text_w = 0;
    unsigned int           text_h = 0;
    std::string            marked;
    WindowMode             mode = WindowMode::Windowed;
    NSRect                 windowed_frame = NSZeroRect;
    NSWindowStyleMask      windowed_style = 0;
    bool                   resizable = true;
    unsigned int           min_w = 0;
    unsigned int           min_h = 0;
    bool                   maximized = false;
    bool                   minimized = false;
    float                  scale = 1.0f;
    std::bitset<256>       keys_down;
    bool                   drop_enabled = true;
    bool                   drag_inside = false;
};

} // namespace detail
} // namespace windows
} // namespace fizmo

using fizmo::windows::WindowEvent;
using fizmo::windows::WindowEventType;
using fizmo::windows::detail::MacWindowState;

namespace {

void ensure_application() {
    static bool ready = false;
    if (ready) return;
    ready = true;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSMenu* bar = [[NSMenu alloc] init];
    NSMenuItem* app_item = [[NSMenuItem alloc] init];
    [bar addItem:app_item];
    NSMenu* app_menu = [[NSMenu alloc] init];
    NSString* name = [[NSProcessInfo processInfo] processName];
    [app_menu addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
    [app_menu addItem:[NSMenuItem separatorItem]];
    [app_menu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(fizmoQuit:) keyEquivalent:@"q"];
    [app_item setSubmenu:app_menu];
    NSMenuItem* window_item = [[NSMenuItem alloc] init];
    [bar addItem:window_item];
    NSMenu* window_menu = [[NSMenu alloc] initWithTitle:@"Window"];
    [window_menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [window_item setSubmenu:window_menu];
    [NSApp setMainMenu:bar];
    [NSApp setWindowsMenu:window_menu];
    [NSApp finishLaunching];
}

NSString* ns_string(const std::string& s) {
    NSString* out = [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding];
    return out ? out : @"";
}

std::string std_string(NSString* s) {
    if (!s) return {};
    const char* utf8 = [s UTF8String];
    return utf8 ? std::string(utf8) : std::string();
}

CGFloat primary_height() {
    NSArray<NSScreen*>* screens = [NSScreen screens];
    if ([screens count] == 0) return 0;
    return NSMaxY([[screens objectAtIndex:0] frame]);
}

unsigned int clamp_coord(CGFloat v) { return v < 0 ? 0u : static_cast<unsigned int>(v); }

fizmo::input::Modifiers modifiers_of(NSEventModifierFlags f) {
    using fizmo::input::Modifiers;
    Modifiers m = Modifiers::None;
    if (f & NSEventModifierFlagShift)    m |= Modifiers::Shift;
    if (f & NSEventModifierFlagControl)  m |= Modifiers::Control;
    if (f & NSEventModifierFlagOption)   m |= Modifiers::Alt;
    if (f & NSEventModifierFlagCommand)  m |= Modifiers::Meta;
    if (f & NSEventModifierFlagCapsLock) m |= Modifiers::CapsLock;
    return m;
}

fizmo::input::Key key_from_mac(unsigned short code) {
    using K = fizmo::input::Key;
    switch (code) {
        case kVK_ANSI_A: return K::A;
        case kVK_ANSI_B: return K::B;
        case kVK_ANSI_C: return K::C;
        case kVK_ANSI_D: return K::D;
        case kVK_ANSI_E: return K::E;
        case kVK_ANSI_F: return K::F;
        case kVK_ANSI_G: return K::G;
        case kVK_ANSI_H: return K::H;
        case kVK_ANSI_I: return K::I;
        case kVK_ANSI_J: return K::J;
        case kVK_ANSI_K: return K::K;
        case kVK_ANSI_L: return K::L;
        case kVK_ANSI_M: return K::M;
        case kVK_ANSI_N: return K::N;
        case kVK_ANSI_O: return K::O;
        case kVK_ANSI_P: return K::P;
        case kVK_ANSI_Q: return K::Q;
        case kVK_ANSI_R: return K::R;
        case kVK_ANSI_S: return K::S;
        case kVK_ANSI_T: return K::T;
        case kVK_ANSI_U: return K::U;
        case kVK_ANSI_V: return K::V;
        case kVK_ANSI_W: return K::W;
        case kVK_ANSI_X: return K::X;
        case kVK_ANSI_Y: return K::Y;
        case kVK_ANSI_Z: return K::Z;
        case kVK_ANSI_0: return K::Num0;
        case kVK_ANSI_1: return K::Num1;
        case kVK_ANSI_2: return K::Num2;
        case kVK_ANSI_3: return K::Num3;
        case kVK_ANSI_4: return K::Num4;
        case kVK_ANSI_5: return K::Num5;
        case kVK_ANSI_6: return K::Num6;
        case kVK_ANSI_7: return K::Num7;
        case kVK_ANSI_8: return K::Num8;
        case kVK_ANSI_9: return K::Num9;
        case kVK_F1: return K::F1;
        case kVK_F2: return K::F2;
        case kVK_F3: return K::F3;
        case kVK_F4: return K::F4;
        case kVK_F5: return K::F5;
        case kVK_F6: return K::F6;
        case kVK_F7: return K::F7;
        case kVK_F8: return K::F8;
        case kVK_F9: return K::F9;
        case kVK_F10: return K::F10;
        case kVK_F11: return K::F11;
        case kVK_F12: return K::F12;
        case kVK_F13: return K::F13;
        case kVK_F14: return K::F14;
        case kVK_F15: return K::F15;
        case kVK_F16: return K::F16;
        case kVK_F17: return K::F17;
        case kVK_F18: return K::F18;
        case kVK_F19: return K::F19;
        case kVK_F20: return K::F20;
        case kVK_Escape: return K::Escape;
        case kVK_Return: return K::Enter;
        case kVK_Tab: return K::Tab;
        case kVK_Delete: return K::Backspace;
        case kVK_Space: return K::Space;
        case kVK_ANSI_Minus: return K::Minus;
        case kVK_ANSI_Equal: return K::Equals;
        case kVK_ANSI_LeftBracket: return K::LeftBracket;
        case kVK_ANSI_RightBracket: return K::RightBracket;
        case kVK_ANSI_Backslash: return K::Backslash;
        case kVK_ANSI_Semicolon: return K::Semicolon;
        case kVK_ANSI_Quote: return K::Quote;
        case kVK_ANSI_Grave: return K::Grave;
        case kVK_ANSI_Comma: return K::Comma;
        case kVK_ANSI_Period: return K::Period;
        case kVK_ANSI_Slash: return K::Slash;
        case kVK_ISO_Section: return K::NonUSBackslash;
        case kVK_CapsLock: return K::CapsLock;
        case kVK_Help: return K::Insert;
        case kVK_ForwardDelete: return K::Delete;
        case kVK_Home: return K::Home;
        case kVK_End: return K::End;
        case kVK_PageUp: return K::PageUp;
        case kVK_PageDown: return K::PageDown;
        case kVK_LeftArrow: return K::Left;
        case kVK_RightArrow: return K::Right;
        case kVK_UpArrow: return K::Up;
        case kVK_DownArrow: return K::Down;
        case kVK_ANSI_Keypad0: return K::Numpad0;
        case kVK_ANSI_Keypad1: return K::Numpad1;
        case kVK_ANSI_Keypad2: return K::Numpad2;
        case kVK_ANSI_Keypad3: return K::Numpad3;
        case kVK_ANSI_Keypad4: return K::Numpad4;
        case kVK_ANSI_Keypad5: return K::Numpad5;
        case kVK_ANSI_Keypad6: return K::Numpad6;
        case kVK_ANSI_Keypad7: return K::Numpad7;
        case kVK_ANSI_Keypad8: return K::Numpad8;
        case kVK_ANSI_Keypad9: return K::Numpad9;
        case kVK_ANSI_KeypadDivide: return K::NumpadDivide;
        case kVK_ANSI_KeypadMultiply: return K::NumpadMultiply;
        case kVK_ANSI_KeypadMinus: return K::NumpadSubtract;
        case kVK_ANSI_KeypadPlus: return K::NumpadAdd;
        case kVK_ANSI_KeypadEnter: return K::NumpadEnter;
        case kVK_ANSI_KeypadDecimal: return K::NumpadDecimal;
        case kVK_ANSI_KeypadEquals: return K::NumpadEquals;
        case kVK_ANSI_KeypadClear: return K::NumLock;
        case kVK_Shift: return K::LeftShift;
        case kVK_RightShift: return K::RightShift;
        case kVK_Control: return K::LeftControl;
        case kVK_RightControl: return K::RightControl;
        case kVK_Option: return K::LeftAlt;
        case kVK_RightOption: return K::RightAlt;
        case kVK_Command: return K::LeftMeta;
        case 0x36: return K::RightMeta;
        case 0x6E: return K::Menu;
        case kVK_Mute: return K::VolumeMute;
        case kVK_VolumeDown: return K::VolumeDown;
        case kVK_VolumeUp: return K::VolumeUp;
        default: return K::Unknown;
    }
}

NSEventModifierFlags flag_for_key(unsigned short code) {
    switch (code) {
        case kVK_Shift:
        case kVK_RightShift:   return NSEventModifierFlagShift;
        case kVK_Control:
        case kVK_RightControl: return NSEventModifierFlagControl;
        case kVK_Option:
        case kVK_RightOption:  return NSEventModifierFlagOption;
        case kVK_Command:
        case 0x36:             return NSEventModifierFlagCommand;
        case kVK_CapsLock:     return NSEventModifierFlagCapsLock;
        case kVK_Function:     return NSEventModifierFlagFunction;
        default:               return 0;
    }
}

NSImage* image_from_bitmap(const fizmo::images::BitmapImage& img) {
    if (!img.is_valid_image()) return nil;
    const NSInteger w = static_cast<NSInteger>(img.width());
    const NSInteger h = static_cast<NSInteger>(img.height());
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:w * 4 bitsPerPixel:32];
    if (!rep) return nil;
    unsigned char* out = [rep bitmapData];
    const fizmo::colors::Color* px = img.pixels().data();
    for (NSInteger i = 0; i < w * h; ++i) {
        out[i * 4 + 0] = px[i].red();
        out[i * 4 + 1] = px[i].green();
        out[i * 4 + 2] = px[i].blue();
        out[i * 4 + 3] = px[i].alpha();
    }
    NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(static_cast<CGFloat>(w), static_cast<CGFloat>(h))];
    [image addRepresentation:rep];
    return image;
}

std::string key_label(NSEvent* ev, fizmo::input::Key key) {
    using K = fizmo::input::Key;
    switch (key) {
        case K::Space: return "Space";
        case K::Enter:
        case K::NumpadEnter: return "Enter";
        case K::Tab: return "Tab";
        case K::Backspace: return "Backspace";
        case K::Escape: return "Escape";
        case K::LeftShift: return "LeftShift";
        case K::RightShift: return "RightShift";
        case K::LeftControl: return "LeftControl";
        case K::RightControl: return "RightControl";
        case K::LeftAlt: return "LeftAlt";
        case K::RightAlt: return "RightAlt";
        case K::Left: return "LeftArrow";
        case K::Right: return "RightArrow";
        case K::Up: return "UpArrow";
        case K::Down: return "DownArrow";
        default: break;
    }
    if ([ev type] == NSEventTypeKeyDown || [ev type] == NSEventTypeKeyUp) {
        NSString* chars = [ev charactersIgnoringModifiers];
        if ([chars length] > 0) {
            const unichar c = [chars characterAtIndex:0];
            if (c >= 0x20 && c < 0xF700 && c != 0x7F) return std_string(chars);
        }
    }
    const char* name = fizmo::input::key_name(key);
    return name ? std::string(name) : std::string();
}

} // namespace

@interface FizmoView : NSView <NSTextInputClient> {
@public
    MacWindowState* state;
    NSTrackingArea* tracking;
    CALayer*        softwareLayer;
}
- (CALayer*)softwareLayerCreate;
@end

@interface FizmoWindowDelegate : NSObject <NSWindowDelegate> {
@public
    MacWindowState* state;
}
@end

@interface NSApplication (FizmoQuit)
- (void)fizmoQuit:(id)sender;
@end

@implementation NSApplication (FizmoQuit)
- (void)fizmoQuit:(id)sender {
    (void)sender;
    for (NSWindow* w in [[self windows] copy]) {
        if ([[w contentView] isKindOfClass:[FizmoView class]]) [w performClose:nil];
    }
}
@end

static void dispatch_event(MacWindowState* s, WindowEvent& e) {
    if (s && s->owner) s->owner->dispatch(e);
}

static CGFloat scale_of(const MacWindowState* s) {
    return s && s->scale > 0.0f ? static_cast<CGFloat>(s->scale) : 1.0;
}

static void set_point(MacWindowState* s, NSPoint p, WindowEvent& e) {
    const CGFloat k = scale_of(s);
    e.fx = static_cast<float>(p.x * k);
    e.fy = static_cast<float>(p.y * k);
    e.x = clamp_coord(p.x * k);
    e.y = clamp_coord(p.y * k);
}

static void fill_position(FizmoView* view, NSEvent* ev, WindowEvent& e) {
    set_point(view->state, [view convertPoint:[ev locationInWindow] fromView:nil], e);
    e.mods = modifiers_of([ev modifierFlags]);
}

static void update_layer_size(MacWindowState* s) {
    if (!s || !s->view || !s->layer) return;
    const NSSize size = [s->view bounds].size;
    const CGFloat k = scale_of(s);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    s->layer.contentsScale = k;
    s->layer.drawableSize = CGSizeMake(std::max<CGFloat>(1, std::round(size.width * k)), std::max<CGFloat>(1, std::round(size.height * k)));
    if (s->view->softwareLayer) {
        s->view->softwareLayer.contentsScale = k;
        s->view->softwareLayer.frame = [s->view bounds];
    }
    [CATransaction commit];
}

static void report_size(MacWindowState* s) {
    if (!s || !s->view) return;
    update_layer_size(s);
    const NSSize size = [s->view bounds].size;
    const CGFloat k = scale_of(s);
    const unsigned int w = static_cast<unsigned int>(std::lround(size.width * k));
    const unsigned int h = static_cast<unsigned int>(std::lround(size.height * k));
    if (s->window) s->maximized = [s->window isZoomed];
    if (w == s->last_w && h == s->last_h) return;
    s->last_w = w;
    s->last_h = h;
    if (s->owner) s->owner->resized(w, h);
    WindowEvent e;
    e.type = WindowEventType::WindowResize;
    e.x = w;
    e.y = h;
    dispatch_event(s, e);
    s->expose_pending = true;
}

static void apply_cursor(MacWindowState* s) {
    if (!s) return;
    if (!s->cursor_visible || s->locked) {
        if (!s->cursor_hidden) { [NSCursor hide]; s->cursor_hidden = true; }
        return;
    }
    if (s->cursor_hidden) { [NSCursor unhide]; s->cursor_hidden = false; }
    [(s->cursor ? s->cursor : [NSCursor arrowCursor]) set];
}

@implementation FizmoView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        state = nullptr;
        tracking = nil;
        softwareLayer = nil;
        [self setWantsLayer:YES];
        [self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawDuringViewResize];
    }
    return self;
}

- (CALayer*)makeBackingLayer {
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.contentsScale = 1.0;
    layer.opaque = YES;
    return layer;
}

- (BOOL)wantsUpdateLayer { return YES; }
- (void)updateLayer {}
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
- (BOOL)canBecomeKeyView { return YES; }

- (CALayer*)softwareLayerCreate {
    if (!softwareLayer) {
        softwareLayer = [CALayer layer];
        softwareLayer.contentsGravity = kCAGravityTopLeft;
        softwareLayer.contentsScale = scale_of(state);
        softwareLayer.geometryFlipped = YES;
        softwareLayer.frame = [self bounds];
        [[self layer] addSublayer:softwareLayer];
    }
    return softwareLayer;
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    if (!state) return;
    const float scale = state->window ? static_cast<float>([state->window backingScaleFactor]) : 1.0f;
    const bool changed = std::abs(scale - state->scale) > 0.001f;
    state->scale = scale;
    report_size(state);
    if (changed) {
        WindowEvent e;
        e.type = WindowEventType::DpiChanged;
        e.dpi_scale = scale;
        dispatch_event(state, e);
    }
    state->expose_pending = true;
}

- (void)updateTrackingAreas {
    if (tracking) [self removeTrackingArea:tracking];
    tracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect | NSTrackingCursorUpdate owner:self userInfo:nil];
    [self addTrackingArea:tracking];
    [super updateTrackingAreas];
}

- (void)cursorUpdate:(NSEvent*)event {
    (void)event;
    apply_cursor(state);
}

- (void)moved:(NSEvent*)event {
    if (!state) return;
    WindowEvent e;
    e.type = WindowEventType::MouseMove;
    fill_position(self, event, e);
    e.dx = static_cast<int>(std::lround([event deltaX] * scale_of(state)));
    e.dy = static_cast<int>(std::lround([event deltaY] * scale_of(state)));
    if (state->locked && e.dx == 0 && e.dy == 0) return;
    dispatch_event(state, e);
}

- (void)mouseMoved:(NSEvent*)event { [self moved:event]; }
- (void)mouseDragged:(NSEvent*)event { [self moved:event]; }
- (void)rightMouseDragged:(NSEvent*)event { [self moved:event]; }
- (void)otherMouseDragged:(NSEvent*)event { [self moved:event]; }

- (void)button:(NSEvent*)event pressed:(BOOL)pressed {
    if (!state) return;
    unsigned int button = 0;
    const char* name = "";
    switch ([event buttonNumber]) {
        case 0: button = 1; name = "LeftMouse"; break;
        case 1: button = 2; name = "RightMouse"; break;
        case 2: button = 3; name = "MiddleMouse"; break;
        case 3: button = 4; name = "X1Mouse"; break;
        case 4: button = 5; name = "X2Mouse"; break;
        default: return;
    }
    WindowEvent e;
    fill_position(self, event, e);
    e.button = button;
    e.key = button;
    e.key_name = name;
    e.type = pressed ? WindowEventType::MouseClick : WindowEventType::MouseRelease;
    dispatch_event(state, e);
    if (pressed && [event clickCount] == 2) {
        e.type = WindowEventType::MouseDoubleClick;
        dispatch_event(state, e);
    }
}

- (void)mouseDown:(NSEvent*)event { [self button:event pressed:YES]; }
- (void)mouseUp:(NSEvent*)event { [self button:event pressed:NO]; }
- (void)rightMouseDown:(NSEvent*)event { [self button:event pressed:YES]; }
- (void)rightMouseUp:(NSEvent*)event { [self button:event pressed:NO]; }
- (void)otherMouseDown:(NSEvent*)event { [self button:event pressed:YES]; }
- (void)otherMouseUp:(NSEvent*)event { [self button:event pressed:NO]; }

- (void)scrollWheel:(NSEvent*)event {
    if (!state) return;
    CGFloat dx = [event scrollingDeltaX];
    CGFloat dy = [event scrollingDeltaY];
    if ([event hasPreciseScrollingDeltas]) { dx /= 10.0; dy /= 10.0; }
    if (dx == 0 && dy == 0) return;
    WindowEvent e;
    e.type = WindowEventType::MouseScroll;
    fill_position(self, event, e);
    e.scroll_y = static_cast<float>(dy);
    e.scroll_x = static_cast<float>(-dx);
    e.scroll_delta = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    dispatch_event(state, e);
}

- (void)key:(NSEvent*)event pressed:(BOOL)pressed {
    if (!state) return;
    const unsigned short code = [event keyCode];
    WindowEvent e;
    e.type = pressed ? WindowEventType::KeyPress : WindowEventType::KeyRelease;
    e.key = code;
    e.scancode = code;
    e.physical_key = key_from_mac(code);
    e.logical_key = e.physical_key;
    e.mods = modifiers_of([event modifierFlags]);
    e.key_name = key_label(event, e.physical_key);
    if (pressed) {
        e.repeat = [event isARepeat] || state->keys_down.test(code & 0xFF);
        state->keys_down.set(code & 0xFF);
    } else {
        state->keys_down.reset(code & 0xFF);
    }
    dispatch_event(state, e);
}

- (void)keyDown:(NSEvent*)event {
    [self key:event pressed:YES];
    if (!state || !state->text_active) return;
    if ([event modifierFlags] & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) return;
    [self interpretKeyEvents:@[event]];
}

- (void)keyUp:(NSEvent*)event { [self key:event pressed:NO]; }

- (void)flagsChanged:(NSEvent*)event {
    if (!state) return;
    const unsigned short code = [event keyCode];
    const NSEventModifierFlags flag = flag_for_key(code);
    if (flag == 0 || flag == NSEventModifierFlagFunction) return;
    const bool down = flag == NSEventModifierFlagCapsLock ? ([event modifierFlags] & flag) != 0 : !state->keys_down.test(code & 0xFF);
    [self key:event pressed:down ? YES : NO];
}

- (BOOL)performKeyEquivalent:(NSEvent*)event {
    if ([[NSApp mainMenu] performKeyEquivalent:event]) return YES;
    if ([event type] == NSEventTypeKeyDown && ([event modifierFlags] & (NSEventModifierFlagCommand | NSEventModifierFlagControl))) {
        [self keyDown:event];
        return YES;
    }
    return NO;
}

- (void)insertText:(id)string replacementRange:(NSRange)range {
    (void)range;
    if (!state) return;
    NSString* text = [string isKindOfClass:[NSAttributedString class]] ? [(NSAttributedString*)string string] : (NSString*)string;
    std::string raw = std_string(text);
    std::string printable;
    for (unsigned char c : raw) if (c >= 0x20 && c != 0x7F) printable.push_back(static_cast<char>(c));
    if (!state->marked.empty()) {
        state->marked.clear();
        WindowEvent clear;
        clear.type = WindowEventType::TextEditing;
        dispatch_event(state, clear);
    }
    if (printable.empty()) return;
    WindowEvent e;
    e.type = WindowEventType::TextInput;
    e.text = printable;
    dispatch_event(state, e);
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selected replacementRange:(NSRange)range {
    (void)range;
    if (!state) return;
    NSString* text = [string isKindOfClass:[NSAttributedString class]] ? [(NSAttributedString*)string string] : (NSString*)string;
    state->marked = std_string(text);
    WindowEvent e;
    e.type = WindowEventType::TextEditing;
    e.text = state->marked;
    e.text_cursor = static_cast<int>(selected.location);
    e.text_selection = static_cast<int>(selected.length);
    dispatch_event(state, e);
}

- (void)unmarkText {
    if (!state || state->marked.empty()) return;
    state->marked.clear();
    WindowEvent e;
    e.type = WindowEventType::TextEditing;
    dispatch_event(state, e);
}

- (BOOL)hasMarkedText { return state && !state->marked.empty(); }
- (NSRange)markedRange { return [self hasMarkedText] ? NSMakeRange(0, [ns_string(state->marked) length]) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(0, 0); }
- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText { return @[]; }
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual { (void)range; (void)actual; return nil; }
- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return NSNotFound; }
- (void)doCommandBySelector:(SEL)selector { (void)selector; }

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
    (void)range;
    (void)actual;
    if (!state) return NSZeroRect;
    const CGFloat k = scale_of(state);
    const NSRect local = NSMakeRect(state->text_x / k, state->text_y / k, std::max(1u, state->text_w) / k, std::max(1u, state->text_h) / k);
    const NSRect in_window = [self convertRect:local toView:nil];
    return [[self window] convertRectToScreen:in_window];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    if (!state || !state->drop_enabled) return NSDragOperationNone;
    WindowEvent e;
    e.type = state->drag_inside ? WindowEventType::DragOver : WindowEventType::DragEnter;
    state->drag_inside = true;
    set_point(state, [self convertPoint:[sender draggingLocation] fromView:nil], e);
    dispatch_event(state, e);
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender { return [self draggingEntered:sender]; }

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    if (!state) return;
    state->drag_inside = false;
    WindowEvent e;
    e.type = WindowEventType::DragLeave;
    dispatch_event(state, e);
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    if (!state || !state->drop_enabled) return NO;
    state->drag_inside = false;
    NSArray<NSURL*>* urls = [[sender draggingPasteboard] readObjectsForClasses:@[[NSURL class]] options:@{ NSPasteboardURLReadingFileURLsOnlyKey : @YES }];
    WindowEvent e;
    e.type = WindowEventType::FilesDropped;
    for (NSURL* url in urls) {
        const char* path = [url fileSystemRepresentation];
        if (path) e.paths.push_back(path);
    }
    set_point(state, [self convertPoint:[sender draggingLocation] fromView:nil], e);
    if (e.paths.empty()) return NO;
    dispatch_event(state, e);
    return YES;
}

@end

@implementation FizmoWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (!state) return YES;
    WindowEvent e;
    e.type = WindowEventType::WindowClose;
    dispatch_event(state, e);
    state->open = false;
    return YES;
}

- (void)windowWillClose:(NSNotification*)note {
    (void)note;
    if (!state) return;
    state->open = false;
    if (state->locked) {
        CGAssociateMouseAndMouseCursorPosition(true);
        state->locked = false;
    }
    if (state->cursor_hidden) { [NSCursor unhide]; state->cursor_hidden = false; }
}

- (void)windowDidResize:(NSNotification*)note {
    (void)note;
    report_size(state);
}

- (void)windowDidMove:(NSNotification*)note {
    (void)note;
    if (!state || !state->owner) return;
    int x = 0, y = 0;
    state->owner->position(x, y);
    if (x == state->last_x && y == state->last_y) return;
    state->last_x = x;
    state->last_y = y;
    WindowEvent e;
    e.type = WindowEventType::WindowMove;
    e.window_x = x;
    e.window_y = y;
    dispatch_event(state, e);
}

- (void)windowDidBecomeKey:(NSNotification*)note {
    (void)note;
    if (!state) return;
    WindowEvent e;
    e.type = WindowEventType::WindowFocus;
    dispatch_event(state, e);
}

- (void)windowDidResignKey:(NSNotification*)note {
    (void)note;
    if (!state) return;
    if (state->locked && state->owner) state->owner->set_cursor_locked(false);
    state->keys_down.reset();
    WindowEvent e;
    e.type = WindowEventType::WindowBlur;
    dispatch_event(state, e);
}

- (void)windowDidMiniaturize:(NSNotification*)note {
    (void)note;
    if (!state) return;
    state->minimized = true;
    WindowEvent e;
    e.type = WindowEventType::WindowMinimize;
    dispatch_event(state, e);
}

- (void)windowDidDeminiaturize:(NSNotification*)note {
    (void)note;
    if (!state) return;
    state->minimized = false;
    WindowEvent e;
    e.type = WindowEventType::WindowRestore;
    dispatch_event(state, e);
    state->expose_pending = true;
}

- (void)windowDidChangeScreen:(NSNotification*)note {
    (void)note;
    if (!state) return;
    WindowEvent e;
    e.type = WindowEventType::MonitorsChanged;
    dispatch_event(state, e);
}

@end

namespace fizmo {
namespace windows {
namespace detail {

WindowImpl::WindowImpl(IWindowEventHandler* handler, const fizmo::colors::Color& color) noexcept : ImplBase(handler), m_state(new MacWindowState()) {
    m_state->owner = this;
    m_state->background = color;
}

WindowImpl::~WindowImpl() noexcept {
    @autoreleasepool {
        if (m_state->window) {
            m_state->view->state = nullptr;
            ((FizmoWindowDelegate*)m_state->delegate)->state = nullptr;
            [m_state->window setDelegate:nil];
            if (m_state->open) [m_state->window close];
        }
        if (m_state->locked) CGAssociateMouseAndMouseCursorPosition(true);
        if (m_state->cursor_hidden) [NSCursor unhide];
        m_state->window = nil;
        m_state->view = nil;
        m_state->delegate = nil;
        m_state->layer = nil;
    }
}

bool WindowImpl::create(unsigned int width, unsigned int height, const std::string& title) noexcept {
    @autoreleasepool {
        ensure_application();
        NSScreen* main_screen = [NSScreen mainScreen];
        m_state->scale = main_screen ? static_cast<float>([main_screen backingScaleFactor]) : 1.0f;
        const CGFloat k = scale_of(m_state.get());
        const NSRect content = NSMakeRect(0, 0, std::max<CGFloat>(1, std::max(1u, width) / k), std::max<CGFloat>(1, std::max(1u, height) / k));
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
        if (m_state->resizable) style |= NSWindowStyleMaskResizable;
        NSWindow* window = [[NSWindow alloc] initWithContentRect:content styleMask:style backing:NSBackingStoreBuffered defer:NO];
        if (!window) return false;
        [window setReleasedWhenClosed:NO];
        [window setTitle:ns_string(title)];
        [window setAcceptsMouseMovedEvents:YES];
        [window setRestorable:NO];
        [window setTabbingMode:NSWindowTabbingModeDisallowed];
        const fizmo::colors::Color& bg = m_state->background;
        [window setBackgroundColor:[NSColor colorWithSRGBRed:bg.red() / 255.0 green:bg.green() / 255.0 blue:bg.blue() / 255.0 alpha:1.0]];

        FizmoView* view = [[FizmoView alloc] initWithFrame:content];
        view->state = m_state.get();
        FizmoWindowDelegate* delegate = [[FizmoWindowDelegate alloc] init];
        delegate->state = m_state.get();
        [window setContentView:view];
        [window setDelegate:delegate];
        [window makeFirstResponder:view];
        if (m_state->min_w || m_state->min_h) [window setContentMinSize:NSMakeSize(m_state->min_w / k, m_state->min_h / k)];

        m_state->window = window;
        m_state->view = view;
        m_state->delegate = delegate;
        m_state->layer = (CAMetalLayer*)[view layer];
        CGColorRef layer_color = CGColorCreateSRGB(bg.red() / 255.0, bg.green() / 255.0, bg.blue() / 255.0, 1.0);
        m_state->layer.backgroundColor = layer_color;
        CGColorRelease(layer_color);
        m_state->handle.window = (__bridge void*)window;
        m_state->handle.view = (__bridge void*)view;
        m_state->handle.layer = (__bridge void*)m_state->layer;
        m_state->scale = static_cast<float>([window backingScaleFactor]);
        if (m_state->drop_enabled) [view registerForDraggedTypes:@[NSPasteboardTypeFileURL]];

        [window center];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        update_layer_size(m_state.get());
        const NSSize actual = [view bounds].size;
        m_state->last_w = static_cast<unsigned int>(std::lround(actual.width * scale_of(m_state.get())));
        m_state->last_h = static_cast<unsigned int>(std::lround(actual.height * scale_of(m_state.get())));
        if (m_state->last_w != width || m_state->last_h != height) resized(m_state->last_w, m_state->last_h);
        position(m_state->last_x, m_state->last_y);
        m_state->open = true;
        m_state->expose_pending = true;
        return true;
    }
}

void WindowImpl::poll_events() noexcept {
    @autoreleasepool {
        for (;;) {
            NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!ev) break;
            if ([ev type] == NSEventTypeKeyUp && ([ev modifierFlags] & NSEventModifierFlagCommand)) {
                NSView* content = [[ev window] contentView];
                if ([content isKindOfClass:[FizmoView class]]) {
                    [content keyUp:ev];
                    continue;
                }
            }
            [NSApp sendEvent:ev];
        }
        [NSApp updateWindows];
        if (m_state->open && m_state->expose_pending) {
            m_state->expose_pending = false;
            if (m_state->paint) m_state->paint(nullptr);
            WindowEvent e;
            e.type = WindowEventType::WindowExpose;
            dispatch(e);
        }
    }
}

bool WindowImpl::is_open() const noexcept { return m_state->open; }

void WindowImpl::close() noexcept {
    if (!m_state->open) return;
    @autoreleasepool {
        m_state->open = false;
        if (m_state->window) [m_state->window close];
    }
}

void WindowImpl::set_background_color(const fizmo::colors::Color& color) noexcept {
    m_state->background = color;
    if (!m_state->window) return;
    @autoreleasepool {
        [m_state->window setBackgroundColor:[NSColor colorWithSRGBRed:color.red() / 255.0 green:color.green() / 255.0 blue:color.blue() / 255.0 alpha:1.0]];
        if (m_state->layer) {
            CGColorRef c = CGColorCreateSRGB(color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0, 1.0);
            m_state->layer.backgroundColor = c;
            CGColorRelease(c);
        }
    }
    invalidate();
}

void WindowImpl::set_background_erase(bool enabled) noexcept { (void)enabled; }

void WindowImpl::invalidate() noexcept { m_state->expose_pending = true; }

void* WindowImpl::native_handle() const noexcept { return static_cast<void*>(&m_state->handle); }

void WindowImpl::set_paint_callback(std::function<void(void*)> cb) noexcept { m_state->paint = std::move(cb); }

bool WindowImpl::set_cursor_locked(bool locked) noexcept {
    if (!m_state->window) return false;
    if (locked == m_state->locked) return true;
    m_state->locked = locked;
    CGAssociateMouseAndMouseCursorPosition(locked ? false : true);
    if (locked) {
        const NSRect frame = [m_state->window convertRectToScreen:[m_state->view convertRect:[m_state->view bounds] toView:nil]];
        const CGPoint center = CGPointMake(NSMidX(frame), primary_height() - NSMidY(frame));
        CGWarpMouseCursorPosition(center);
    }
    apply_cursor(m_state.get());
    return true;
}

bool WindowImpl::cursor_locked() const noexcept { return m_state->locked; }

void WindowImpl::set_cursor_visible(bool visible) noexcept {
    m_state->cursor_visible = visible;
    apply_cursor(m_state.get());
}

bool WindowImpl::cursor_visible() const noexcept { return m_state->cursor_visible; }

void WindowImpl::set_title(const std::string& title) noexcept {
    if (!m_state->window) return;
    @autoreleasepool { [m_state->window setTitle:ns_string(title)]; }
}

bool WindowImpl::set_position(int x, int y) noexcept {
    if (!m_state->window) return false;
    @autoreleasepool {
        const NSRect content = [m_state->window contentRectForFrameRect:[m_state->window frame]];
        const CGFloat k = scale_of(m_state.get());
        const NSRect target = NSMakeRect(x / k, primary_height() - y / k - content.size.height, content.size.width, content.size.height);
        [m_state->window setFrame:[m_state->window frameRectForContentRect:target] display:YES];
    }
    return true;
}

bool WindowImpl::position(int& x, int& y) const noexcept {
    x = y = 0;
    if (!m_state->window) return false;
    const NSRect content = [m_state->window contentRectForFrameRect:[m_state->window frame]];
    const CGFloat k = scale_of(m_state.get());
    x = static_cast<int>(std::lround(content.origin.x * k));
    y = static_cast<int>(std::lround((primary_height() - (content.origin.y + content.size.height)) * k));
    return true;
}

bool WindowImpl::set_size(unsigned int w, unsigned int h) noexcept {
    if (!m_state->window || w == 0 || h == 0) return false;
    @autoreleasepool {
        const NSRect content = [m_state->window contentRectForFrameRect:[m_state->window frame]];
        const CGFloat top = content.origin.y + content.size.height;
        const CGFloat k = scale_of(m_state.get());
        const NSRect target = NSMakeRect(content.origin.x, top - h / k, w / k, h / k);
        [m_state->window setFrame:[m_state->window frameRectForContentRect:target] display:YES];
    }
    return true;
}

void WindowImpl::set_resizable(bool resizable) noexcept {
    m_state->resizable = resizable;
    if (!m_state->window || m_state->mode != WindowMode::Windowed) return;
    NSWindowStyleMask style = [m_state->window styleMask];
    if (resizable) style |= NSWindowStyleMaskResizable;
    else style &= ~NSWindowStyleMaskResizable;
    [m_state->window setStyleMask:style];
}

void WindowImpl::set_min_size(unsigned int w, unsigned int h) noexcept {
    m_state->min_w = w;
    m_state->min_h = h;
    if (m_state->window) [m_state->window setContentMinSize:NSMakeSize(w / scale_of(m_state.get()), h / scale_of(m_state.get()))];
}

void WindowImpl::minimize() noexcept {
    if (m_state->window) [m_state->window miniaturize:nil];
}

void WindowImpl::maximize() noexcept {
    if (m_state->window && ![m_state->window isZoomed]) [m_state->window zoom:nil];
}

void WindowImpl::restore() noexcept {
    if (!m_state->window) return;
    if ([m_state->window isMiniaturized]) [m_state->window deminiaturize:nil];
    else if ([m_state->window isZoomed]) [m_state->window zoom:nil];
    [m_state->window makeKeyAndOrderFront:nil];
}

void WindowImpl::focus() noexcept {
    if (!m_state->window) return;
    [NSApp activateIgnoringOtherApps:YES];
    [m_state->window makeKeyAndOrderFront:nil];
}

bool WindowImpl::set_mode(WindowMode mode, int monitor) noexcept {
    if (!m_state->window) return false;
    @autoreleasepool {
        NSArray<NSScreen*>* screens = [NSScreen screens];
        if ([screens count] == 0) return false;
        NSScreen* screen = (monitor >= 0 && static_cast<NSUInteger>(monitor) < [screens count]) ? [screens objectAtIndex:static_cast<NSUInteger>(monitor)] : ([m_state->window screen] ? [m_state->window screen] : [screens objectAtIndex:0]);

        if (m_state->mode == WindowMode::Windowed && mode != WindowMode::Windowed) {
            m_state->windowed_frame = [m_state->window frame];
            m_state->windowed_style = [m_state->window styleMask];
        }

        if (mode == WindowMode::Windowed) {
            [NSApp setPresentationOptions:NSApplicationPresentationDefault];
            [m_state->window setLevel:NSNormalWindowLevel];
            if (m_state->windowed_style) [m_state->window setStyleMask:m_state->windowed_style];
            if (!NSIsEmptyRect(m_state->windowed_frame)) [m_state->window setFrame:m_state->windowed_frame display:YES];
        } else {
            [m_state->window setStyleMask:NSWindowStyleMaskBorderless];
            [m_state->window setFrame:[screen frame] display:YES];
            if (mode == WindowMode::Fullscreen) {
                [NSApp setPresentationOptions:NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar];
                [m_state->window setLevel:NSMainMenuWindowLevel + 1];
            } else {
                [NSApp setPresentationOptions:NSApplicationPresentationDefault];
                [m_state->window setLevel:NSNormalWindowLevel];
            }
            [m_state->window makeFirstResponder:m_state->view];
            [m_state->window makeKeyAndOrderFront:nil];
        }

        m_state->mode = mode;
        update_layer_size(m_state.get());
        m_state->expose_pending = true;
    }
    return true;
}

WindowMode WindowImpl::mode() const noexcept { return m_state->mode; }

float WindowImpl::dpi_scale() const noexcept {
    if (!m_state->window) {
        NSScreen* screen = [NSScreen mainScreen];
        return screen ? static_cast<float>([screen backingScaleFactor]) : 1.0f;
    }
    return static_cast<float>([m_state->window backingScaleFactor]);
}

int WindowImpl::monitor_index() const noexcept {
    if (!m_state->window) return 0;
    NSScreen* screen = [m_state->window screen];
    NSArray<NSScreen*>* screens = [NSScreen screens];
    const NSUInteger i = screen ? [screens indexOfObject:screen] : NSNotFound;
    return i == NSNotFound ? 0 : static_cast<int>(i);
}

bool WindowImpl::set_system_cursor(SystemCursor shape) noexcept {
    NSCursor* c = nil;
    switch (shape) {
        case SystemCursor::Arrow:              c = [NSCursor arrowCursor]; break;
        case SystemCursor::IBeam:              c = [NSCursor IBeamCursor]; break;
        case SystemCursor::Hand:               c = [NSCursor pointingHandCursor]; break;
        case SystemCursor::Crosshair:          c = [NSCursor crosshairCursor]; break;
        case SystemCursor::Wait:               c = [NSCursor arrowCursor]; break;
        case SystemCursor::Progress:           c = [NSCursor arrowCursor]; break;
        case SystemCursor::NotAllowed:         c = [NSCursor operationNotAllowedCursor]; break;
        case SystemCursor::Move:               c = [NSCursor openHandCursor]; break;
        case SystemCursor::ResizeHorizontal:   c = [NSCursor resizeLeftRightCursor]; break;
        case SystemCursor::ResizeVertical:     c = [NSCursor resizeUpDownCursor]; break;
        case SystemCursor::ResizeDiagonalNWSE: c = [NSCursor crosshairCursor]; break;
        case SystemCursor::ResizeDiagonalNESW: c = [NSCursor crosshairCursor]; break;
        case SystemCursor::Help:               c = [NSCursor contextualMenuCursor]; break;
        default:                               c = [NSCursor arrowCursor]; break;
    }
    m_state->cursor = c;
    apply_cursor(m_state.get());
    return c != nil;
}

bool WindowImpl::set_cursor_image(const images::BitmapImage& img, int hot_x, int hot_y) noexcept {
    @autoreleasepool {
        NSImage* image = image_from_bitmap(img);
        if (!image) return false;
        const int hx = std::max(0, std::min(hot_x, static_cast<int>(img.width()) - 1));
        const int hy = std::max(0, std::min(hot_y, static_cast<int>(img.height()) - 1));
        m_state->cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(hx, hy)];
        apply_cursor(m_state.get());
        return m_state->cursor != nil;
    }
}

bool WindowImpl::set_icons(const std::vector<const images::BitmapImage*>& images) noexcept {
    @autoreleasepool {
        const images::BitmapImage* best = nullptr;
        for (const images::BitmapImage* img : images) {
            if (!img || !img->is_valid_image()) continue;
            if (!best || img->width() > best->width()) best = img;
        }
        if (!best) return false;
        ensure_application();
        NSImage* image = image_from_bitmap(*best);
        if (!image) return false;
        [NSApp setApplicationIconImage:image];
        return true;
    }
}

void WindowImpl::set_app_id(const std::string& id) noexcept { (void)id; }

std::string WindowImpl::clipboard_text() noexcept {
    @autoreleasepool { return std_string([[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString]); }
}

bool WindowImpl::set_clipboard_text(const std::string& text) noexcept {
    @autoreleasepool {
        NSPasteboard* board = [NSPasteboard generalPasteboard];
        [board clearContents];
        return [board setString:ns_string(text) forType:NSPasteboardTypeString] ? true : false;
    }
}

void WindowImpl::set_drop_enabled(bool enabled) noexcept {
    m_state->drop_enabled = enabled;
    if (!m_state->view) return;
    if (enabled) [m_state->view registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    else [m_state->view unregisterDraggedTypes];
}

void WindowImpl::start_text_input() noexcept { m_state->text_active = true; }

void WindowImpl::stop_text_input() noexcept {
    m_state->text_active = false;
    if (m_state->view) [[m_state->view inputContext] discardMarkedText];
    if (!m_state->marked.empty()) {
        m_state->marked.clear();
        WindowEvent e;
        e.type = WindowEventType::TextEditing;
        dispatch(e);
    }
}

bool WindowImpl::text_input_active() const noexcept { return m_state->text_active; }

void WindowImpl::set_text_input_rect(int x, int y, unsigned int w, unsigned int h) noexcept {
    m_state->text_x = x;
    m_state->text_y = y;
    m_state->text_w = w;
    m_state->text_h = h;
}

std::vector<MonitorInfo> platform_monitors() {
    std::vector<MonitorInfo> out;
    @autoreleasepool {
        NSArray<NSScreen*>* screens = [NSScreen screens];
        const CGFloat top = primary_height();
        for (NSUInteger i = 0; i < [screens count]; ++i) {
            NSScreen* screen = [screens objectAtIndex:i];
            MonitorInfo m;
            const NSRect f = [screen frame];
            const NSRect v = [screen visibleFrame];
            m.scale = static_cast<float>([screen backingScaleFactor]);
            const CGFloat k = m.scale > 0.0f ? m.scale : 1.0;
            m.bounds = Rect{ static_cast<int>(f.origin.x * k), static_cast<int>((top - (f.origin.y + f.size.height)) * k), static_cast<unsigned int>(f.size.width * k), static_cast<unsigned int>(f.size.height * k) };
            m.work_area = Rect{ static_cast<int>(v.origin.x * k), static_cast<int>((top - (v.origin.y + v.size.height)) * k), static_cast<unsigned int>(v.size.width * k), static_cast<unsigned int>(v.size.height * k) };
            m.dpi = 72.0f * m.scale;
            m.primary = i == 0;
            if (@available(macOS 10.15, *)) m.name = std_string([screen localizedName]);
            NSNumber* number = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
            const CGDirectDisplayID display = number ? static_cast<CGDirectDisplayID>([number unsignedIntValue]) : 0;
            m.handle = static_cast<std::uintptr_t>(display);
            if (m.name.empty()) m.name = "Display " + std::to_string(i + 1);
            if (display) {
                const CGSize mm = CGDisplayScreenSize(display);
                m.physical_width_mm = static_cast<unsigned int>(mm.width);
                m.physical_height_mm = static_cast<unsigned int>(mm.height);
                if (CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display)) {
                    m.current.width = static_cast<unsigned int>(CGDisplayModeGetWidth(mode));
                    m.current.height = static_cast<unsigned int>(CGDisplayModeGetHeight(mode));
                    m.current.refresh_rate = CGDisplayModeGetRefreshRate(mode);
                    if (m.current.refresh_rate <= 0.0) m.current.refresh_rate = 60.0;
                    CGDisplayModeRelease(mode);
                }
                if (CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, nullptr)) {
                    for (CFIndex k = 0; k < CFArrayGetCount(modes); ++k) {
                        CGDisplayModeRef mode = static_cast<CGDisplayModeRef>(const_cast<void*>(CFArrayGetValueAtIndex(modes, k)));
                        DisplayMode dm;
                        dm.width = static_cast<unsigned int>(CGDisplayModeGetWidth(mode));
                        dm.height = static_cast<unsigned int>(CGDisplayModeGetHeight(mode));
                        dm.refresh_rate = CGDisplayModeGetRefreshRate(mode);
                        m.modes.push_back(dm);
                    }
                    CFRelease(modes);
                }
            }
            out.push_back(m);
        }
    }
    return out;
}

} // namespace detail
} // namespace windows

namespace macos {

bool present_pixels(const Handle* handle, const std::uint32_t* bgra, unsigned int width, unsigned int height, unsigned int stride_bytes) noexcept {
    if (!handle || !handle->view || !bgra || width == 0 || height == 0) return false;
    @autoreleasepool {
        FizmoView* view = (__bridge FizmoView*)handle->view;
        const std::size_t bytes = static_cast<std::size_t>(stride_bytes) * height;
        CFDataRef data = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(bgra), static_cast<CFIndex>(bytes));
        if (!data) return false;
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGImageRef image = CGImageCreate(width, height, 8, 32, stride_bytes, space, static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst), provider, nullptr, false, kCGRenderingIntentDefault);
        CGColorSpaceRelease(space);
        CGDataProviderRelease(provider);
        CFRelease(data);
        if (!image) return false;
        void (^apply)(void) = ^{
            CALayer* layer = [view softwareLayerCreate];
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            layer.contentsScale = scale_of(view->state);
            layer.frame = [view bounds];
            layer.contents = (__bridge id)image;
            [CATransaction commit];
            CGImageRelease(image);
        };
        if ([NSThread isMainThread]) apply();
        else dispatch_async(dispatch_get_main_queue(), apply);
    }
    return true;
}

float backing_scale(const Handle* handle) noexcept {
    if (!handle || !handle->window) return 1.0f;
    NSWindow* window = (__bridge NSWindow*)handle->window;
    return static_cast<float>([window backingScaleFactor]);
}

} // namespace macos
} // namespace fizmo

#endif // OS_MACOS
