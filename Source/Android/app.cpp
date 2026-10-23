#include "app.hpp"
#include "keys_android.hpp"
#include "jni_android.hpp"
#include "../System/paths.hpp"

#if defined(OS_ANDROID)

#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/sensor.h>
#include <android/window.h>
#include <jni.h>

#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace fizmo {
namespace android {

namespace {

enum Command : std::int8_t {
    CmdStart = 1,
    CmdResume,
    CmdPause,
    CmdStop,
    CmdDestroy,
    CmdWindowCreated,
    CmdWindowDestroyed,
    CmdWindowResized,
    CmdRedraw,
    CmdFocus,
    CmdBlur,
    CmdContentRect,
    CmdConfig,
    CmdLowMemory,
    CmdSaveState,
    CmdInputQueue,
    CmdExternal
};

constexpr int kLooperMain   = 1;
constexpr int kLooperInput  = 2;
constexpr int kLooperSensor = 3;

std::int64_t now_ns() noexcept {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

int sensor_kind(SensorType t) noexcept {
    switch (t) {
        case SensorType::Accelerometer:      return ASENSOR_TYPE_ACCELEROMETER;
        case SensorType::Gyroscope:          return ASENSOR_TYPE_GYROSCOPE;
        case SensorType::Magnetometer:       return ASENSOR_TYPE_MAGNETIC_FIELD;
        case SensorType::Light:              return ASENSOR_TYPE_LIGHT;
        case SensorType::Proximity:          return ASENSOR_TYPE_PROXIMITY;
        case SensorType::Gravity:            return ASENSOR_TYPE_GRAVITY;
        case SensorType::LinearAcceleration: return ASENSOR_TYPE_LINEAR_ACCELERATION;
        case SensorType::RotationVector:     return ASENSOR_TYPE_ROTATION_VECTOR;
        default:                             return 0;
    }
}

bool sensor_from_kind(int kind, SensorType& out) noexcept {
    for (int i = 0; i < static_cast<int>(SensorType::Count); ++i) {
        if (sensor_kind(static_cast<SensorType>(i)) == kind) { out = static_cast<SensorType>(i); return true; }
    }
    return false;
}

int orientation_value(Orientation o) noexcept {
    switch (o) {
        case Orientation::Landscape:        return 0;
        case Orientation::Portrait:         return 1;
        case Orientation::Sensor:           return 4;
        case Orientation::SensorLandscape:  return 6;
        case Orientation::SensorPortrait:   return 7;
        case Orientation::ReverseLandscape: return 8;
        case Orientation::ReversePortrait:  return 9;
        case Orientation::Locked:           return 14;
        default:                            return -1;
    }
}

input::PointerType pointer_of_tool(int tool) noexcept {
    switch (tool) {
        case AMOTION_EVENT_TOOL_TYPE_STYLUS:
        case AMOTION_EVENT_TOOL_TYPE_ERASER: return input::PointerType::Pen;
        case AMOTION_EVENT_TOOL_TYPE_MOUSE:  return input::PointerType::Mouse;
        default:                             return input::PointerType::Touch;
    }
}

struct External {
    enum Kind : std::uint8_t { Text, Erase, Key, Insets, DeviceRemoved } kind = Text;
    std::string text;
    int a = 0, b = 0, c = 0, d = 0, e = 0;
    bool down = false;
};

std::atomic<AppState*> g_state{ nullptr };
std::once_flag g_stdio_once;

} // namespace

struct AppState {
    ANativeActivity*        activity = nullptr;
    std::unique_ptr<App>    app;
    std::mutex              mutex;
    std::condition_variable cond;
    std::thread             thread;
    int                     msg_read  = -1;
    int                     msg_write = -1;
    ALooper*                looper = nullptr;
    JNIEnv*                 env = nullptr;

    AInputQueue*            input_queue = nullptr;
    AInputQueue*            pending_input_queue = nullptr;
    bool                    input_ack = false;
    ANativeWindow*          pending_window = nullptr;
    bool                    window_ack = false;
    int                     activity_state = 0;

    bool                    running = false;
    bool                    destroy_requested = false;
    bool                    destroyed = false;
    bool                    state_saved = false;
    std::vector<std::uint8_t> saved_state;
    std::vector<std::uint8_t> restored_state;

    ARect                   content_rect{};
    bool                    content_rect_known = false;
    ARect                   pending_content_rect{};
    AConfiguration*         config = nullptr;

    Surface                 surface;
    Assets                  assets;
    std::deque<Event>       events;

    std::mutex              external_mutex;
    std::vector<External>   external;

    bool                    started = false;
    bool                    resumed = false;
    bool                    focused = false;

    Insets                  java_safe;
    int                     java_ime = 0;
    bool                    java_insets = false;
    Insets                  reported;
    bool                    reported_once = false;
    bool                    keyboard_requested = false;

    detail::JavaHooks       java;

    ASensorManager*         sensor_manager = nullptr;
    ASensorEventQueue*      sensor_queue = nullptr;
    bool                    sensor_on[static_cast<int>(SensorType::Count)] = {};
    unsigned int            sensor_rate[static_cast<int>(SensorType::Count)] = {};

    std::int32_t            mouse_buttons = 0;
    bool                    mouse_virtual = false;
    std::vector<GamepadState> pads;

    std::string             package;

    static App* make_app(AppState* s) { return new App(s); }

    void push(Event e) {
        if (e.time_ns == 0) e.time_ns = now_ns();
        events.push_back(std::move(e));
    }

    void push_type(EventType t) {
        Event e;
        e.type = t;
        push(std::move(e));
    }

    void write_cmd(std::int8_t cmd) noexcept {
        if (msg_write < 0) return;
        while (::write(msg_write, &cmd, 1) < 0 && errno == EINTR) {}
    }

    void wait_for_state(int cmd) {
        std::unique_lock<std::mutex> lock(mutex);
        write_cmd(static_cast<std::int8_t>(cmd));
        cond.wait(lock, [&] { return activity_state == cmd || destroyed; });
    }

    void push_external(External x) {
        {
            std::lock_guard<std::mutex> lock(external_mutex);
            external.push_back(std::move(x));
        }
        write_cmd(CmdExternal);
    }

    Insets compute_safe(int& keyboard) const noexcept {
        Insets safe;
        Insets content;
        bool have_content = false;
        if (content_rect_known && surface.valid()) {
            const int w = static_cast<int>(surface.width());
            const int h = static_cast<int>(surface.height());
            content.left   = std::max(0, content_rect.left);
            content.top    = std::max(0, content_rect.top);
            content.right  = std::max(0, w - content_rect.right);
            content.bottom = std::max(0, h - content_rect.bottom);
            have_content = content_rect.right > content_rect.left && content_rect.bottom > content_rect.top;
        }
        keyboard = java_insets ? java_ime : 0;
        if (java_insets) {
            safe = java_safe;
            if (have_content) {
                safe.left  = std::max(safe.left, content.left);
                safe.top   = std::max(safe.top, content.top);
                safe.right = std::max(safe.right, content.right);
                if (content.bottom > safe.bottom) keyboard = std::max(keyboard, content.bottom - safe.bottom);
            }
        } else if (have_content) {
            safe = content;
        }
        return safe;
    }

    Insets content_insets() const noexcept {
        int keyboard = 0;
        Insets c = compute_safe(keyboard);
        c.bottom = std::max(c.bottom, keyboard);
        return c;
    }

    void check_insets() {
        const Insets now = content_insets();
        if (reported_once && now == reported) return;
        reported_once = true;
        reported = now;
        Event e;
        e.type = EventType::InsetsChanged;
        e.insets = now;
        e.width = surface.width();
        e.height = surface.height();
        push(std::move(e));
    }

    void check_surface_size() {
        if (!surface.valid()) return;
        if (!surface.refresh_size()) return;
        surface.notify(SurfaceChange::Resized);
        Event e;
        e.type = EventType::SurfaceResized;
        e.width = surface.width();
        e.height = surface.height();
        push(std::move(e));
        check_insets();
    }

    void set_sensors_running(bool on) noexcept {
        if (!sensor_manager || !sensor_queue) return;
        for (int i = 0; i < static_cast<int>(SensorType::Count); ++i) {
            if (!sensor_on[i]) continue;
            const ASensor* sensor = ASensorManager_getDefaultSensor(sensor_manager, sensor_kind(static_cast<SensorType>(i)));
            if (!sensor) continue;
            if (on) {
                ASensorEventQueue_enableSensor(sensor_queue, sensor);
                const unsigned int rate = sensor_rate[i] ? sensor_rate[i] : 60u;
                ASensorEventQueue_setEventRate(sensor_queue, sensor, static_cast<std::int32_t>(1000000u / rate));
            } else {
                ASensorEventQueue_disableSensor(sensor_queue, sensor);
            }
        }
    }

    GamepadState* pad_for(int device_id, bool create) {
        for (GamepadState& p : pads) if (p.connected && p.device_id == device_id) return &p;
        if (!create) return nullptr;
        GamepadState* slot = nullptr;
        for (GamepadState& p : pads) if (!p.connected) { slot = &p; break; }
        if (!slot) {
            pads.emplace_back();
            slot = &pads.back();
            slot->slot = static_cast<int>(pads.size() - 1);
        }
        const int keep = slot->slot;
        *slot = GamepadState{};
        slot->slot = keep;
        slot->device_id = device_id;
        slot->connected = true;
        detail::DeviceDetails info = detail::device_details(env, device_id);
        slot->name = info.name.empty() ? std::string("Gamepad") : info.name;
        slot->vendor = info.vendor;
        slot->product = info.product;
        Event e;
        e.type = EventType::GamepadConnected;
        e.gamepad = slot->slot;
        e.device_id = device_id;
        e.text = slot->name;
        push(std::move(e));
        return slot;
    }

    void pad_button(GamepadState& pad, input::GamepadButton b, bool down) {
        const std::size_t i = static_cast<std::size_t>(b);
        if (pad.buttons[i] == down) return;
        pad.buttons[i] = down;
        Event e;
        e.type = down ? EventType::GamepadButtonDown : EventType::GamepadButtonUp;
        e.gamepad = pad.slot;
        e.device_id = pad.device_id;
        e.gamepad_button = b;
        push(std::move(e));
    }

    void pad_axis(GamepadState& pad, input::GamepadAxis a, float v) {
        const std::size_t i = static_cast<std::size_t>(a);
        if (std::fabs(pad.axes[i] - v) < 0.0005f) return;
        pad.axes[i] = v;
        Event e;
        e.type = EventType::GamepadAxisMotion;
        e.gamepad = pad.slot;
        e.device_id = pad.device_id;
        e.gamepad_axis = a;
        e.axis_value = v;
        push(std::move(e));
    }

    void remove_pad(int device_id) {
        GamepadState* p = pad_for(device_id, false);
        if (!p) return;
        p->connected = false;
        Event e;
        e.type = EventType::GamepadDisconnected;
        e.gamepad = p->slot;
        e.device_id = device_id;
        e.text = p->name;
        push(std::move(e));
    }

    void emit_key(int code, int meta, bool down, bool repeat, int scan, int source, int device) {
        if (code == AKEYCODE_BACK) {
            if (!down) push_type(EventType::Back);
            return;
        }
        input::GamepadButton gb{};
        const bool pad_source = (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD || (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK;
        if (pad_source && detail::gamepad_button_from_android(code, gb)) {
            if (GamepadState* pad = pad_for(device, true)) pad_button(*pad, gb, down);
            return;
        }
        Event e;
        e.type = down ? EventType::KeyDown : EventType::KeyUp;
        e.key = detail::key_from_android(code);
        e.key_code = code;
        e.scan_code = scan;
        e.mods = detail::mods_from_android(meta);
        e.repeat = repeat;
        e.device_id = device;
        push(e);
        if (!down) return;
        const bool chord = input::has(e.mods, input::Modifiers::Control) || input::has(e.mods, input::Modifiers::Meta) ||
                           (input::has(e.mods, input::Modifiers::Alt) && !input::has(e.mods, input::Modifiers::AltGr));
        if (chord || code == AKEYCODE_DEL || code == AKEYCODE_FORWARD_DEL || code == AKEYCODE_ENTER || code == AKEYCODE_TAB || code == AKEYCODE_NUMPAD_ENTER) return;
        const std::uint32_t cp = detail::unicode_char(env, java, code, meta);
        if (cp < 0x20 || cp == 0x7F || (cp & 0x80000000u) != 0 || cp > 0x10FFFF) return;
        Event t;
        t.type = EventType::TextInput;
        t.text = detail::utf8_of(cp);
        t.key_code = code;
        t.device_id = device;
        push(std::move(t));
    }

    void erase(int before, int after) {
        for (int i = 0; i < before; ++i) {
            emit_key(AKEYCODE_DEL, 0, true, false, 0, AINPUT_SOURCE_KEYBOARD, -1);
            emit_key(AKEYCODE_DEL, 0, false, false, 0, AINPUT_SOURCE_KEYBOARD, -1);
        }
        for (int i = 0; i < after; ++i) {
            emit_key(AKEYCODE_FORWARD_DEL, 0, true, false, 0, AINPUT_SOURCE_KEYBOARD, -1);
            emit_key(AKEYCODE_FORWARD_DEL, 0, false, false, 0, AINPUT_SOURCE_KEYBOARD, -1);
        }
    }

    void drain_external() {
        std::vector<External> items;
        {
            std::lock_guard<std::mutex> lock(external_mutex);
            items.swap(external);
        }
        for (External& x : items) {
            switch (x.kind) {
                case External::Text: {
                    Event e;
                    e.type = EventType::TextInput;
                    e.text = std::move(x.text);
                    push(std::move(e));
                    break;
                }
                case External::Erase:
                    erase(x.a, x.b);
                    break;
                case External::Key:
                    emit_key(x.a, x.b, x.down, false, 0, AINPUT_SOURCE_KEYBOARD, -1);
                    break;
                case External::Insets:
                    java_safe = Insets{ x.a, x.b, x.c, x.d };
                    java_ime = x.e;
                    java_insets = true;
                    check_insets();
                    break;
                case External::DeviceRemoved:
                    remove_pad(x.a);
                    break;
            }
        }
    }

    int translate_key(const AInputEvent* ev) {
        const int action = AKeyEvent_getAction(ev);
        const int code = AKeyEvent_getKeyCode(ev);
        const int meta = AKeyEvent_getMetaState(ev);
        const int source = AInputEvent_getSource(ev);
        const int device = AInputEvent_getDeviceId(ev);
        const int scan = AKeyEvent_getScanCode(ev);
        if (detail::is_system_key(code)) {
            Event e;
            e.type = action == AKEY_EVENT_ACTION_UP ? EventType::KeyUp : EventType::KeyDown;
            e.key = detail::key_from_android(code);
            e.key_code = code;
            e.mods = detail::mods_from_android(meta);
            push(std::move(e));
            return 0;
        }
        if (action == AKEY_EVENT_ACTION_MULTIPLE) {
            if (code == AKEYCODE_UNKNOWN) return 0;
            const int count = std::max(1, AKeyEvent_getRepeatCount(ev));
            for (int i = 0; i < count; ++i) {
                emit_key(code, meta, true, i > 0, scan, source, device);
                emit_key(code, meta, false, false, scan, source, device);
            }
            return 1;
        }
        emit_key(code, meta, action == AKEY_EVENT_ACTION_DOWN, AKeyEvent_getRepeatCount(ev) > 0, scan, source, device);
        return 1;
    }

    void fill_pointers(const AInputEvent* ev, Event& e) {
        const std::size_t n = AMotionEvent_getPointerCount(ev);
        e.pointers.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            Pointer& p = e.pointers[i];
            p.id = AMotionEvent_getPointerId(ev, i);
            p.x = AMotionEvent_getX(ev, i);
            p.y = AMotionEvent_getY(ev, i);
            p.pressure = AMotionEvent_getPressure(ev, i);
            p.size = AMotionEvent_getSize(ev, i);
            p.type = pointer_of_tool(AMotionEvent_getToolType(ev, i));
        }
    }

    void touch_event(const AInputEvent* ev, EventType type, std::size_t index, const Event& base) {
        Event e = base;
        e.type = type;
        if (index < e.pointers.size()) {
            const Pointer& p = e.pointers[index];
            e.pointer_id = p.id;
            e.x = p.x;
            e.y = p.y;
            e.pressure = p.pressure;
            e.size = p.size;
            e.pointer = p.type;
        }
        (void)ev;
        push(std::move(e));
    }

    void mouse_buttons_changed(std::int32_t now, float x, float y, input::Modifiers mods) {
        static const std::pair<std::int32_t, MouseButton> bits[] = {
            { AMOTION_EVENT_BUTTON_PRIMARY, MouseButton::Left },
            { AMOTION_EVENT_BUTTON_SECONDARY, MouseButton::Right },
            { AMOTION_EVENT_BUTTON_TERTIARY, MouseButton::Middle },
            { AMOTION_EVENT_BUTTON_BACK, MouseButton::Back },
            { AMOTION_EVENT_BUTTON_FORWARD, MouseButton::Forward },
        };
        for (const auto& [bit, button] : bits) {
            const bool was = (mouse_buttons & bit) != 0;
            const bool is = (now & bit) != 0;
            if (was == is) continue;
            Event e;
            e.type = is ? EventType::MouseDown : EventType::MouseUp;
            e.button = button;
            e.x = x;
            e.y = y;
            e.pointer = input::PointerType::Mouse;
            e.buttons = static_cast<unsigned int>(now);
            e.mods = mods;
            push(std::move(e));
        }
        mouse_buttons = now;
    }

    int translate_joystick(const AInputEvent* ev) {
        const int device = AInputEvent_getDeviceId(ev);
        GamepadState* pad = pad_for(device, true);
        if (!pad) return 0;
        auto axis = [&](int a) { return AMotionEvent_getAxisValue(ev, a, 0); };
        const float z = axis(AMOTION_EVENT_AXIS_Z);
        const float rz = axis(AMOTION_EVENT_AXIS_RZ);
        const float rx = axis(AMOTION_EVENT_AXIS_RX);
        const float ry = axis(AMOTION_EVENT_AXIS_RY);
        const bool use_r = std::fabs(rx) + std::fabs(ry) > std::fabs(z) + std::fabs(rz);
        pad_axis(*pad, input::GamepadAxis::LeftX, axis(AMOTION_EVENT_AXIS_X));
        pad_axis(*pad, input::GamepadAxis::LeftY, axis(AMOTION_EVENT_AXIS_Y));
        pad_axis(*pad, input::GamepadAxis::RightX, use_r ? rx : z);
        pad_axis(*pad, input::GamepadAxis::RightY, use_r ? ry : rz);
        pad_axis(*pad, input::GamepadAxis::LeftTrigger, std::max(axis(AMOTION_EVENT_AXIS_LTRIGGER), axis(AMOTION_EVENT_AXIS_BRAKE)));
        pad_axis(*pad, input::GamepadAxis::RightTrigger, std::max(axis(AMOTION_EVENT_AXIS_RTRIGGER), axis(AMOTION_EVENT_AXIS_GAS)));
        const float hx = axis(AMOTION_EVENT_AXIS_HAT_X);
        const float hy = axis(AMOTION_EVENT_AXIS_HAT_Y);
        pad_button(*pad, input::GamepadButton::DpadLeft, hx < -0.5f);
        pad_button(*pad, input::GamepadButton::DpadRight, hx > 0.5f);
        pad_button(*pad, input::GamepadButton::DpadUp, hy < -0.5f);
        pad_button(*pad, input::GamepadButton::DpadDown, hy > 0.5f);
        return 1;
    }

    int translate_motion(const AInputEvent* ev) {
        const int source = AInputEvent_getSource(ev);
        if ((source & AINPUT_SOURCE_CLASS_JOYSTICK) != 0) return translate_joystick(ev);
        const int action = AMotionEvent_getAction(ev);
        const int masked = action & AMOTION_EVENT_ACTION_MASK;
        const std::size_t index = static_cast<std::size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        const int tool = AMotionEvent_getToolType(ev, index);
        const input::Modifiers mods = detail::mods_from_android(AMotionEvent_getMetaState(ev));
        const bool mouse = (source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE || tool == AMOTION_EVENT_TOOL_TYPE_MOUSE;
        if (mouse) {
            const float x = AMotionEvent_getX(ev, 0);
            const float y = AMotionEvent_getY(ev, 0);
            if (masked == AMOTION_EVENT_ACTION_SCROLL) {
                Event e;
                e.type = EventType::MouseScroll;
                e.x = x;
                e.y = y;
                e.scroll_x = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HSCROLL, 0);
                e.scroll_y = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_VSCROLL, 0);
                e.pointer = input::PointerType::Mouse;
                e.mods = mods;
                push(std::move(e));
                return 1;
            }
            if (masked == AMOTION_EVENT_ACTION_MOVE || masked == AMOTION_EVENT_ACTION_HOVER_MOVE || masked == AMOTION_EVENT_ACTION_HOVER_ENTER) {
                Event e;
                e.type = EventType::MouseMove;
                e.x = x;
                e.y = y;
                e.pointer = input::PointerType::Mouse;
                e.buttons = static_cast<unsigned int>(mouse_buttons);
                e.mods = mods;
                push(std::move(e));
            }
            std::int32_t buttons = AMotionEvent_getButtonState(ev);
            if (masked == AMOTION_EVENT_ACTION_DOWN && buttons == 0) {
                mouse_virtual = true;
                buttons = AMOTION_EVENT_BUTTON_PRIMARY;
            } else if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_CANCEL) {
                if (mouse_virtual) buttons = 0;
                mouse_virtual = false;
            } else if (mouse_virtual) {
                buttons |= AMOTION_EVENT_BUTTON_PRIMARY;
            }
            if (buttons != mouse_buttons) mouse_buttons_changed(buttons, x, y, mods);
            return 1;
        }
        Event base;
        base.mods = mods;
        fill_pointers(ev, base);
        switch (masked) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                touch_event(ev, EventType::TouchDown, index, base);
                break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                touch_event(ev, EventType::TouchUp, index, base);
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                for (std::size_t i = 0; i < base.pointers.size(); ++i) touch_event(ev, EventType::TouchMove, i, base);
                break;
            case AMOTION_EVENT_ACTION_CANCEL:
                for (std::size_t i = 0; i < base.pointers.size(); ++i) touch_event(ev, EventType::TouchCancel, i, base);
                break;
            case AMOTION_EVENT_ACTION_HOVER_MOVE: {
                Event e;
                e.type = EventType::MouseMove;
                e.x = AMotionEvent_getX(ev, 0);
                e.y = AMotionEvent_getY(ev, 0);
                e.pointer = pointer_of_tool(tool);
                e.mods = mods;
                push(std::move(e));
                break;
            }
            default:
                return 0;
        }
        return 1;
    }

    void process_input() {
        if (!input_queue) return;
        AInputEvent* ev = nullptr;
        while (AInputQueue_getEvent(input_queue, &ev) >= 0) {
            if (AInputQueue_preDispatchEvent(input_queue, ev)) continue;
            int handled = 0;
            const int type = AInputEvent_getType(ev);
            if (type == AINPUT_EVENT_TYPE_KEY) handled = translate_key(ev);
            else if (type == AINPUT_EVENT_TYPE_MOTION) handled = translate_motion(ev);
            AInputQueue_finishEvent(input_queue, ev, handled);
        }
    }

    void process_sensors() {
        if (!sensor_queue) return;
        ASensorEvent ev{};
        while (ASensorEventQueue_getEvents(sensor_queue, &ev, 1) > 0) {
            SensorType t{};
            if (!sensor_from_kind(ev.type, t)) continue;
            Event e;
            e.type = EventType::SensorChanged;
            e.sensor = t;
            e.time_ns = ev.timestamp;
            for (int i = 0; i < 4; ++i) e.values[i] = ev.data[i];
            push(std::move(e));
        }
    }

    void set_activity_state(int cmd) {
        std::lock_guard<std::mutex> lock(mutex);
        activity_state = cmd;
        cond.notify_all();
    }

    void process_cmd(std::int8_t cmd) {
        switch (cmd) {
            case CmdInputQueue: {
                std::lock_guard<std::mutex> lock(mutex);
                if (input_queue) AInputQueue_detachLooper(input_queue);
                input_queue = pending_input_queue;
                if (input_queue) AInputQueue_attachLooper(input_queue, looper, kLooperInput, nullptr, nullptr);
                input_ack = true;
                cond.notify_all();
                break;
            }
            case CmdWindowCreated: {
                ANativeWindow* w = nullptr;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    w = pending_window;
                    window_ack = true;
                    cond.notify_all();
                }
                if (surface.valid()) {
                    surface.notify(SurfaceChange::Destroyed);
                    surface.detach();
                }
                if (w) {
                    surface.attach(w);
                    surface.notify(SurfaceChange::Created);
                    Event e;
                    e.type = EventType::SurfaceCreated;
                    e.width = surface.width();
                    e.height = surface.height();
                    push(std::move(e));
                    check_insets();
                }
                break;
            }
            case CmdWindowDestroyed: {
                if (surface.valid()) {
                    surface.notify(SurfaceChange::Destroyed);
                    surface.detach();
                    push_type(EventType::SurfaceDestroyed);
                }
                std::lock_guard<std::mutex> lock(mutex);
                window_ack = true;
                cond.notify_all();
                break;
            }
            case CmdWindowResized:
                check_surface_size();
                break;
            case CmdRedraw:
                check_surface_size();
                surface.request_redraw();
                push_type(EventType::RedrawNeeded);
                break;
            case CmdFocus:
                focused = true;
                push_type(EventType::FocusGained);
                break;
            case CmdBlur:
                focused = false;
                push_type(EventType::FocusLost);
                break;
            case CmdContentRect: {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    content_rect = pending_content_rect;
                    content_rect_known = true;
                }
                check_insets();
                break;
            }
            case CmdConfig:
                if (config && activity) AConfiguration_fromAssetManager(config, activity->assetManager);
                push_type(EventType::ConfigChanged);
                check_surface_size();
                break;
            case CmdLowMemory:
                push_type(EventType::LowMemory);
                break;
            case CmdSaveState: {
                push_type(EventType::SaveState);
                std::lock_guard<std::mutex> lock(mutex);
                state_saved = true;
                cond.notify_all();
                break;
            }
            case CmdStart:
                started = true;
                push_type(EventType::Start);
                set_activity_state(cmd);
                break;
            case CmdResume:
                resumed = true;
                set_sensors_running(true);
                push_type(EventType::Resume);
                set_activity_state(cmd);
                break;
            case CmdPause:
                resumed = false;
                set_sensors_running(false);
                push_type(EventType::Pause);
                set_activity_state(cmd);
                break;
            case CmdStop:
                started = false;
                push_type(EventType::Stop);
                set_activity_state(cmd);
                break;
            case CmdDestroy:
                destroy_requested = true;
                push_type(EventType::Destroy);
                break;
            case CmdExternal:
                drain_external();
                break;
            default:
                break;
        }
    }

    void process_commands() {
        std::int8_t cmd = 0;
        while (true) {
            const ssize_t got = ::read(msg_read, &cmd, 1);
            if (got == 1) { process_cmd(cmd); continue; }
            if (got < 0 && errno == EINTR) continue;
            break;
        }
    }

    int poll_once(int timeout_ms) {
        int fd = 0;
        int ev = 0;
        void* data = nullptr;
        const int ident = ALooper_pollOnce(timeout_ms, &fd, &ev, &data);
        if (ident == kLooperMain) process_commands();
        else if (ident == kLooperInput) process_input();
        else if (ident == kLooperSensor) process_sensors();
        return ident;
    }

    void release_surface() noexcept {
        if (!surface.valid()) return;
        surface.notify(SurfaceChange::Destroyed);
        surface.detach();
    }

    void drain_ready() {
        for (int guard = 0; guard < 256; ++guard) {
            if (poll_once(0) < 0) break;
        }
        check_surface_size();
    }
};

namespace {

AppState* state_of(ANativeActivity* a) noexcept { return a ? static_cast<AppState*>(a->instance) : nullptr; }

void on_start(ANativeActivity* a) { if (AppState* s = state_of(a)) s->wait_for_state(CmdStart); }
void on_resume(ANativeActivity* a) { if (AppState* s = state_of(a)) s->wait_for_state(CmdResume); }
void on_pause(ANativeActivity* a) { if (AppState* s = state_of(a)) s->wait_for_state(CmdPause); }
void on_stop(ANativeActivity* a) { if (AppState* s = state_of(a)) s->wait_for_state(CmdStop); }

void on_destroy(ANativeActivity* a) {
    AppState* s = state_of(a);
    if (!s) return;
    {
        std::unique_lock<std::mutex> lock(s->mutex);
        s->write_cmd(CmdDestroy);
        s->cond.wait(lock, [&] { return s->destroyed; });
    }
    if (s->thread.joinable()) s->thread.join();
    if (s->msg_read >= 0) ::close(s->msg_read);
    if (s->msg_write >= 0) ::close(s->msg_write);
    AppState* expected = s;
    g_state.compare_exchange_strong(expected, nullptr);
    a->instance = nullptr;
    delete s;
}

void* on_save_state(ANativeActivity* a, size_t* out_size) {
    *out_size = 0;
    AppState* s = state_of(a);
    if (!s) return nullptr;
    std::unique_lock<std::mutex> lock(s->mutex);
    s->state_saved = false;
    s->write_cmd(CmdSaveState);
    s->cond.wait(lock, [&] { return s->state_saved || s->destroyed; });
    if (s->saved_state.empty()) return nullptr;
    void* copy = std::malloc(s->saved_state.size());
    if (!copy) return nullptr;
    std::memcpy(copy, s->saved_state.data(), s->saved_state.size());
    *out_size = s->saved_state.size();
    return copy;
}

void on_focus(ANativeActivity* a, int focused) { if (AppState* s = state_of(a)) s->write_cmd(focused ? CmdFocus : CmdBlur); }
void on_config(ANativeActivity* a) { if (AppState* s = state_of(a)) s->write_cmd(CmdConfig); }
void on_low_memory(ANativeActivity* a) { if (AppState* s = state_of(a)) s->write_cmd(CmdLowMemory); }

void on_window_created(ANativeActivity* a, ANativeWindow* w) {
    AppState* s = state_of(a);
    if (!s) return;
    std::unique_lock<std::mutex> lock(s->mutex);
    s->pending_window = w;
    s->window_ack = false;
    s->write_cmd(CmdWindowCreated);
    s->cond.wait(lock, [&] { return s->window_ack || s->destroyed; });
}

void on_window_destroyed(ANativeActivity* a, ANativeWindow*) {
    AppState* s = state_of(a);
    if (!s) return;
    std::unique_lock<std::mutex> lock(s->mutex);
    s->pending_window = nullptr;
    s->window_ack = false;
    s->write_cmd(CmdWindowDestroyed);
    s->cond.wait(lock, [&] { return s->window_ack || s->destroyed; });
}

void on_window_resized(ANativeActivity* a, ANativeWindow*) { if (AppState* s = state_of(a)) s->write_cmd(CmdWindowResized); }
void on_window_redraw(ANativeActivity* a, ANativeWindow*) { if (AppState* s = state_of(a)) s->write_cmd(CmdRedraw); }

void on_content_rect(ANativeActivity* a, const ARect* r) {
    AppState* s = state_of(a);
    if (!s || !r) return;
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->pending_content_rect = *r;
    }
    s->write_cmd(CmdContentRect);
}

void set_input_queue(ANativeActivity* a, AInputQueue* q) {
    AppState* s = state_of(a);
    if (!s) return;
    std::unique_lock<std::mutex> lock(s->mutex);
    s->pending_input_queue = q;
    s->input_ack = false;
    s->write_cmd(CmdInputQueue);
    s->cond.wait(lock, [&] { return s->input_ack || s->destroyed; });
}

void on_input_created(ANativeActivity* a, AInputQueue* q) { set_input_queue(a, q); }
void on_input_destroyed(ANativeActivity* a, AInputQueue*) { set_input_queue(a, nullptr); }

void JNICALL native_text(JNIEnv* env, jobject, jstring text) {
    AppState* s = g_state.load();
    if (!s || !text) return;
    External x;
    x.kind = External::Text;
    x.text = detail::utf8_of(env, text);
    if (!x.text.empty()) s->push_external(std::move(x));
}

void JNICALL native_erase(JNIEnv*, jobject, jint before, jint after) {
    AppState* s = g_state.load();
    if (!s) return;
    External x;
    x.kind = External::Erase;
    x.a = before;
    x.b = after;
    s->push_external(std::move(x));
}

void JNICALL native_key(JNIEnv*, jobject, jint code, jint meta, jboolean down) {
    AppState* s = g_state.load();
    if (!s) return;
    External x;
    x.kind = External::Key;
    x.a = code;
    x.b = meta;
    x.down = down == JNI_TRUE;
    s->push_external(std::move(x));
}

void JNICALL native_insets(JNIEnv*, jobject, jint left, jint top, jint right, jint bottom, jint ime) {
    AppState* s = g_state.load();
    if (!s) return;
    External x;
    x.kind = External::Insets;
    x.a = left;
    x.b = top;
    x.c = right;
    x.d = bottom;
    x.e = ime;
    s->push_external(std::move(x));
}

struct PickWaiters {
    std::mutex                mutex;
    std::condition_variable   cond;
    std::set<int>             waiting;
    std::map<int, PickResult> done;
    int                       next = 1;
};

PickWaiters& pick_waiters() {
    static PickWaiters w;
    return w;
}

void JNICALL native_picked(JNIEnv* env, jobject, jint id, jobjectArray paths, jstring error, jboolean cancelled) {
    PickResult r;
    if (paths) {
        const jsize n = env->GetArrayLength(paths);
        for (jsize i = 0; i < n; ++i) {
            jstring p = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
            if (detail::clear_exception(env)) break;
            std::string path = detail::utf8_of(env, p);
            if (p) env->DeleteLocalRef(p);
            if (!path.empty()) r.paths.push_back(std::move(path));
        }
    }
    r.error = detail::utf8_of(env, error);
    r.cancelled = cancelled == JNI_TRUE;
    PickWaiters& w = pick_waiters();
    {
        std::lock_guard<std::mutex> lock(w.mutex);
        if (!w.waiting.count(id)) return;
        w.waiting.erase(id);
        w.done[id] = std::move(r);
    }
    w.cond.notify_all();
}

void JNICALL native_device_removed(JNIEnv*, jobject, jint id) {
    AppState* s = g_state.load();
    if (!s) return;
    External x;
    x.kind = External::DeviceRemoved;
    x.a = id;
    s->push_external(std::move(x));
}

bool register_natives(JNIEnv* env, jobject activity) {
    static const JNINativeMethod methods[] = {
        { "nativeText", "(Ljava/lang/String;)V", reinterpret_cast<void*>(native_text) },
        { "nativeErase", "(II)V", reinterpret_cast<void*>(native_erase) },
        { "nativeKey", "(IIZ)V", reinterpret_cast<void*>(native_key) },
        { "nativeInsets", "(IIIII)V", reinterpret_cast<void*>(native_insets) },
        { "nativeDeviceRemoved", "(I)V", reinterpret_cast<void*>(native_device_removed) },
        { "nativePicked", "(I[Ljava/lang/String;Ljava/lang/String;Z)V", reinterpret_cast<void*>(native_picked) },
    };
    jclass cls = detail::find_class_in_hierarchy(env, activity, "fizmo.android.FizmoActivity");
    if (!cls) return false;
    const bool ok = env->RegisterNatives(cls, methods, static_cast<jint>(sizeof(methods) / sizeof(methods[0]))) == JNI_OK;
    detail::clear_exception(env);
    env->DeleteLocalRef(cls);
    return ok;
}

void app_thread(AppState* s) {
    pthread_setname_np(pthread_self(), "fizmo-main");
    JNIEnv* env = nullptr;
    s->activity->vm->AttachCurrentThread(&env, nullptr);
    s->env = env;
    s->config = AConfiguration_new();
    AConfiguration_fromAssetManager(s->config, s->activity->assetManager);
    s->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    ALooper_addFd(s->looper, s->msg_read, kLooperMain, ALOOPER_EVENT_INPUT, nullptr, nullptr);
    s->java = detail::load_java_hooks(env, s->activity->clazz);
    s->package = detail::package_name(env, s->activity->clazz);
    {
        const std::string data = s->activity->internalDataPath ? s->activity->internalDataPath : "";
        const std::string external = s->activity->externalDataPath ? s->activity->externalDataPath : "";
        std::string cache = detail::cache_dir(env, s->activity->clazz);
        if (cache.empty()) cache = data;
        try { fizmo::system::paths::set_app_directories(data, cache, external); } catch (...) {}
    }
    s->sensor_manager = ASensorManager_getInstanceForPackage(s->package.empty() ? nullptr : s->package.c_str());
    if (s->sensor_manager) s->sensor_queue = ASensorManager_createEventQueue(s->sensor_manager, s->looper, kLooperSensor, nullptr, nullptr);
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->running = true;
        s->cond.notify_all();
    }

    fizmo_android_main(*s->app);

    if (!s->destroy_requested) ANativeActivity_finish(s->activity);
    while (!s->destroy_requested) {
        s->poll_once(-1);
        s->events.clear();
    }

    s->release_surface();
    if (s->sensor_queue && s->sensor_manager) ASensorManager_destroyEventQueue(s->sensor_manager, s->sensor_queue);
    s->sensor_queue = nullptr;
    detail::release_java_hooks(env, s->java);
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        if (s->input_queue) AInputQueue_detachLooper(s->input_queue);
        s->input_queue = nullptr;
        if (s->config) AConfiguration_delete(s->config);
        s->config = nullptr;
        s->destroyed = true;
        s->cond.notify_all();
    }
    s->env = nullptr;
    s->activity->vm->DetachCurrentThread();
}

} // namespace

const char* event_type_name(EventType t) noexcept {
    switch (t) {
        case EventType::None:                return "None";
        case EventType::Start:               return "Start";
        case EventType::Resume:              return "Resume";
        case EventType::Pause:               return "Pause";
        case EventType::Stop:                return "Stop";
        case EventType::Destroy:             return "Destroy";
        case EventType::SurfaceCreated:      return "SurfaceCreated";
        case EventType::SurfaceDestroyed:    return "SurfaceDestroyed";
        case EventType::SurfaceResized:      return "SurfaceResized";
        case EventType::RedrawNeeded:        return "RedrawNeeded";
        case EventType::FocusGained:         return "FocusGained";
        case EventType::FocusLost:           return "FocusLost";
        case EventType::InsetsChanged:       return "InsetsChanged";
        case EventType::ConfigChanged:       return "ConfigChanged";
        case EventType::LowMemory:           return "LowMemory";
        case EventType::SaveState:           return "SaveState";
        case EventType::TouchDown:           return "TouchDown";
        case EventType::TouchMove:           return "TouchMove";
        case EventType::TouchUp:             return "TouchUp";
        case EventType::TouchCancel:         return "TouchCancel";
        case EventType::MouseMove:           return "MouseMove";
        case EventType::MouseDown:           return "MouseDown";
        case EventType::MouseUp:             return "MouseUp";
        case EventType::MouseScroll:         return "MouseScroll";
        case EventType::KeyDown:             return "KeyDown";
        case EventType::KeyUp:               return "KeyUp";
        case EventType::TextInput:           return "TextInput";
        case EventType::Back:                return "Back";
        case EventType::SensorChanged:       return "SensorChanged";
        case EventType::GamepadConnected:    return "GamepadConnected";
        case EventType::GamepadDisconnected: return "GamepadDisconnected";
        case EventType::GamepadButtonDown:   return "GamepadButtonDown";
        case EventType::GamepadButtonUp:     return "GamepadButtonUp";
        case EventType::GamepadAxisMotion:   return "GamepadAxisMotion";
        default:                             return "Unknown";
    }
}

App* App::current() noexcept {
    AppState* s = g_state.load();
    return s ? s->app.get() : nullptr;
}

ANativeActivity* App::activity() const noexcept { return m_state->activity; }
void* App::java_vm() const noexcept { return m_state->activity ? m_state->activity->vm : nullptr; }
void* App::activity_object() const noexcept { return m_state->activity ? m_state->activity->clazz : nullptr; }
void* App::jni_env() const noexcept { return detail::env_for_thread(m_state->activity ? m_state->activity->vm : nullptr); }
bool App::has_fizmo_activity() const noexcept { return m_state->java.fizmo; }

bool App::poll(Event& out, int timeout_ms) {
    AppState* s = m_state;
    if (s->events.empty()) s->drain_ready();
    if (s->events.empty() && timeout_ms != 0) {
        const auto start = std::chrono::steady_clock::now();
        while (s->events.empty()) {
            int wait = -1;
            if (timeout_ms > 0) {
                const auto spent = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (spent >= timeout_ms) break;
                wait = static_cast<int>(timeout_ms - spent);
            }
            const int ident = s->poll_once(wait);
            if (ident == ALOOPER_POLL_WAKE || ident == ALOOPER_POLL_TIMEOUT || ident == ALOOPER_POLL_ERROR) {
                s->drain_ready();
                break;
            }
            s->drain_ready();
        }
    }
    if (s->events.empty()) return false;
    out = std::move(s->events.front());
    s->events.pop_front();
    return true;
}

void App::wake() noexcept { if (m_state->looper) ALooper_wake(m_state->looper); }

bool App::running() const noexcept { return !m_state->destroy_requested; }
bool App::started() const noexcept { return m_state->started; }
bool App::resumed() const noexcept { return m_state->resumed; }
bool App::focused() const noexcept { return m_state->focused; }

void App::finish() noexcept { if (m_state->activity && !m_state->destroy_requested) ANativeActivity_finish(m_state->activity); }

void App::move_to_back() noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    if (!detail::move_task_to_back(env, m_state->java, m_state->activity->clazz)) finish();
}

Surface& App::surface() noexcept { return m_state->surface; }
const Surface& App::surface() const noexcept { return m_state->surface; }

DisplayInfo App::display() const {
    DisplayInfo d;
    AppState* s = m_state;
    d.width = s->surface.width();
    d.height = s->surface.height();
    d.sdk_version = s->activity ? s->activity->sdkVersion : 0;
    if (s->config) {
        const int density = AConfiguration_getDensity(s->config);
        d.density_dpi = (density == ACONFIGURATION_DENSITY_DEFAULT || density == ACONFIGURATION_DENSITY_NONE || density == ACONFIGURATION_DENSITY_ANY) ? 160 : density;
        d.density = static_cast<float>(d.density_dpi) / 160.0f;
        const int o = AConfiguration_getOrientation(s->config);
        d.orientation = o == ACONFIGURATION_ORIENTATION_LAND ? Orientation::Landscape : o == ACONFIGURATION_ORIENTATION_PORT ? Orientation::Portrait : Orientation::Unspecified;
        d.night_mode = AConfiguration_getUiModeNight(s->config) == ACONFIGURATION_UI_MODE_NIGHT_YES;
        d.screen_width_dp = AConfiguration_getScreenWidthDp(s->config);
        d.screen_height_dp = AConfiguration_getScreenHeightDp(s->config);
        d.smallest_width_dp = AConfiguration_getSmallestScreenWidthDp(s->config);
        char lang[3] = {};
        char country[3] = {};
        AConfiguration_getLanguage(s->config, lang);
        AConfiguration_getCountry(s->config, country);
        d.language = lang;
        d.country = country;
    }
    JNIEnv* env = detail::env_for_thread(s->activity ? s->activity->vm : nullptr);
    d.font_scale = detail::font_scale(env, s->activity ? s->activity->clazz : nullptr);
    return d;
}

float App::density() const noexcept {
    if (!m_state->config) return 1.0f;
    const int density = AConfiguration_getDensity(m_state->config);
    if (density == ACONFIGURATION_DENSITY_DEFAULT || density == ACONFIGURATION_DENSITY_NONE || density == ACONFIGURATION_DENSITY_ANY) return 1.0f;
    return static_cast<float>(density) / 160.0f;
}

Insets App::insets() const noexcept {
    int keyboard = 0;
    return m_state->compute_safe(keyboard);
}

int App::keyboard_height() const noexcept {
    int keyboard = 0;
    m_state->compute_safe(keyboard);
    return keyboard;
}

Insets App::content_insets() const noexcept { return m_state->content_insets(); }

void App::set_orientation(Orientation o) noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    detail::set_orientation(env, m_state->java, m_state->activity->clazz, orientation_value(o));
}

void App::set_keep_screen_on(bool on) noexcept {
    if (!m_state->activity) return;
    if (on) ANativeActivity_setWindowFlags(m_state->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    else ANativeActivity_setWindowFlags(m_state->activity, 0, AWINDOW_FLAG_KEEP_SCREEN_ON);
}

void App::set_immersive(bool immersive) noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    if (detail::set_immersive(env, m_state->java, m_state->activity->clazz, immersive)) return;
    if (immersive) ANativeActivity_setWindowFlags(m_state->activity, AWINDOW_FLAG_FULLSCREEN, 0);
    else ANativeActivity_setWindowFlags(m_state->activity, 0, AWINDOW_FLAG_FULLSCREEN);
}

void App::show_keyboard(KeyboardKind kind) noexcept {
    m_state->keyboard_requested = true;
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    if (detail::show_keyboard(env, m_state->java, m_state->activity->clazz, static_cast<int>(kind))) return;
    ANativeActivity_showSoftInput(m_state->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_IMPLICIT);
}

void App::hide_keyboard() noexcept {
    m_state->keyboard_requested = false;
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    if (detail::hide_keyboard(env, m_state->java, m_state->activity->clazz)) return;
    ANativeActivity_hideSoftInput(m_state->activity, 0);
}

bool App::keyboard_requested() const noexcept { return m_state->keyboard_requested; }

std::string App::clipboard_text() {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::clipboard_text(env, m_state->java, m_state->activity->clazz);
}

bool App::set_clipboard_text(const std::string& text) {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::set_clipboard_text(env, m_state->java, m_state->activity->clazz, text);
}

bool App::vibrate(unsigned int ms) noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::vibrate(env, m_state->java, m_state->activity->clazz, ms);
}

PickResult App::pick_files(const PickOptions& options) {
    PickResult result;
    if (!m_state->java.fizmo || !m_state->java.pick) {
        result.error = "this app can't show the file picker";
        return result;
    }
    PickWaiters& w = pick_waiters();
    int id = 0;
    {
        std::lock_guard<std::mutex> lock(w.mutex);
        id = w.next++;
        w.waiting.insert(id);
    }
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    const int kind = options.kind == PickKind::Save ? 1 : options.kind == PickKind::Folder ? 2 : 0;
    if (!detail::start_pick(env, m_state->java, m_state->activity->clazz, id, kind, options.multiple, options.title, options.suggested_name, options.patterns, options.start)) {
        std::lock_guard<std::mutex> lock(w.mutex);
        w.waiting.erase(id);
        result.error = "the file picker couldn't start";
        return result;
    }
    std::unique_lock<std::mutex> lock(w.mutex);
    while (!w.done.count(id)) {
        if (m_state->destroy_requested) {
            w.waiting.erase(id);
            result.cancelled = true;
            return result;
        }
        w.cond.wait_for(lock, std::chrono::milliseconds(250));
    }
    result = std::move(w.done[id]);
    w.done.erase(id);
    return result;
}

bool App::all_files_access() const noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::has_all_files_access(env, m_state->java, m_state->activity->clazz);
}

bool App::declares_all_files_access() const noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::declares_all_files_access(env, m_state->java, m_state->activity->clazz);
}

bool App::open_url(const std::string& url) noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::open_url(env, m_state->java, m_state->activity->clazz, url);
}

bool App::share_text(const std::string& text, const std::string& title) noexcept {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::share_text(env, m_state->java, m_state->activity->clazz, text, title);
}

std::string App::internal_data_path() const { return m_state->activity && m_state->activity->internalDataPath ? m_state->activity->internalDataPath : std::string(); }
std::string App::external_data_path() const { return m_state->activity && m_state->activity->externalDataPath ? m_state->activity->externalDataPath : std::string(); }
std::string App::obb_path() const { return m_state->activity && m_state->activity->obbPath ? m_state->activity->obbPath : std::string(); }

std::string App::cache_path() const {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    std::string p = detail::cache_dir(env, m_state->activity->clazz);
    if (p.empty()) p = internal_data_path();
    return p;
}

std::string App::native_library_dir() const {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::native_library_dir(env, m_state->activity->clazz);
}

std::string App::package_name() const { return m_state->package; }

std::string App::locale() const {
    JNIEnv* env = detail::env_for_thread(m_state->activity->vm);
    return detail::locale(env);
}

int App::sdk_version() const noexcept { return m_state->activity ? m_state->activity->sdkVersion : 0; }

const Assets& App::assets() const noexcept { return m_state->assets; }

bool App::has_sensor(SensorType t) const noexcept {
    return m_state->sensor_manager && ASensorManager_getDefaultSensor(m_state->sensor_manager, sensor_kind(t)) != nullptr;
}

bool App::enable_sensor(SensorType t, unsigned int rate_hz) noexcept {
    AppState* s = m_state;
    if (!s->sensor_manager || !s->sensor_queue || t >= SensorType::Count) return false;
    const ASensor* sensor = ASensorManager_getDefaultSensor(s->sensor_manager, sensor_kind(t));
    if (!sensor) return false;
    const int i = static_cast<int>(t);
    s->sensor_on[i] = true;
    s->sensor_rate[i] = std::max(1u, std::min(rate_hz, 1000u));
    if (s->resumed) {
        ASensorEventQueue_enableSensor(s->sensor_queue, sensor);
        ASensorEventQueue_setEventRate(s->sensor_queue, sensor, static_cast<std::int32_t>(1000000u / s->sensor_rate[i]));
    }
    return true;
}

void App::disable_sensor(SensorType t) noexcept {
    AppState* s = m_state;
    if (!s->sensor_manager || !s->sensor_queue || t >= SensorType::Count) return;
    const int i = static_cast<int>(t);
    if (!s->sensor_on[i]) return;
    s->sensor_on[i] = false;
    if (const ASensor* sensor = ASensorManager_getDefaultSensor(s->sensor_manager, sensor_kind(t))) ASensorEventQueue_disableSensor(s->sensor_queue, sensor);
}

const std::vector<GamepadState>& App::gamepads() const noexcept { return m_state->pads; }

const std::vector<std::uint8_t>& App::restored_state() const noexcept { return m_state->restored_state; }

void App::set_saved_state(std::vector<std::uint8_t> bytes) {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->saved_state = std::move(bytes);
}

} // namespace android
} // namespace fizmo

extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void* saved_state, size_t saved_size) {
    using namespace fizmo::android;
    std::call_once(g_stdio_once, [] { redirect_stdio("fizmo"); });

    activity->callbacks->onStart = on_start;
    activity->callbacks->onResume = on_resume;
    activity->callbacks->onPause = on_pause;
    activity->callbacks->onStop = on_stop;
    activity->callbacks->onDestroy = on_destroy;
    activity->callbacks->onSaveInstanceState = on_save_state;
    activity->callbacks->onWindowFocusChanged = on_focus;
    activity->callbacks->onConfigurationChanged = on_config;
    activity->callbacks->onLowMemory = on_low_memory;
    activity->callbacks->onNativeWindowCreated = on_window_created;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    activity->callbacks->onNativeWindowResized = on_window_resized;
    activity->callbacks->onNativeWindowRedrawNeeded = on_window_redraw;
    activity->callbacks->onContentRectChanged = on_content_rect;
    activity->callbacks->onInputQueueCreated = on_input_created;
    activity->callbacks->onInputQueueDestroyed = on_input_destroyed;

    auto* s = new AppState;
    s->activity = activity;
    s->assets = Assets(activity->assetManager);
    if (saved_state && saved_size > 0) {
        const auto* bytes = static_cast<const std::uint8_t*>(saved_state);
        s->restored_state.assign(bytes, bytes + saved_size);
        s->saved_state = s->restored_state;
    }
    s->app.reset(AppState::make_app(s));
    register_natives(activity->env, activity->clazz);

    int fds[2] = { -1, -1 };
    if (pipe2(fds, O_CLOEXEC) != 0) {
        __android_log_write(ANDROID_LOG_ERROR, "fizmo", "couldn't create the message pipe");
        delete s;
        ANativeActivity_finish(activity);
        return;
    }
    s->msg_read = fds[0];
    s->msg_write = fds[1];
    fcntl(s->msg_read, F_SETFL, fcntl(s->msg_read, F_GETFL) | O_NONBLOCK);
    activity->instance = s;
    g_state.store(s);

    s->thread = std::thread(app_thread, s);
    std::unique_lock<std::mutex> lock(s->mutex);
    s->cond.wait(lock, [&] { return s->running; });
}

#endif // OS_ANDROID
