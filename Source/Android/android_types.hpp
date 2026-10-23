#ifndef FIZMO_ANDROID_TYPES_HPP
#define FIZMO_ANDROID_TYPES_HPP

#include "../Basic/fizmo_defines.hpp"
#include "../Input/keys.hpp"
#include "../Input/input_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace android {

enum class EventType : std::uint8_t {
    None = 0,
    Start,
    Resume,
    Pause,
    Stop,
    Destroy,
    SurfaceCreated,
    SurfaceDestroyed,
    SurfaceResized,
    RedrawNeeded,
    FocusGained,
    FocusLost,
    InsetsChanged,
    ConfigChanged,
    LowMemory,
    SaveState,
    TouchDown,
    TouchMove,
    TouchUp,
    TouchCancel,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseScroll,
    KeyDown,
    KeyUp,
    TextInput,
    Back,
    SensorChanged,
    GamepadConnected,
    GamepadDisconnected,
    GamepadButtonDown,
    GamepadButtonUp,
    GamepadAxisMotion,
    Count
};

const char* event_type_name(EventType type) noexcept;

enum class Orientation : std::uint8_t {
    Unspecified = 0,
    Portrait,
    Landscape,
    ReversePortrait,
    ReverseLandscape,
    SensorPortrait,
    SensorLandscape,
    Sensor,
    Locked
};

enum class KeyboardKind : std::uint8_t {
    Text = 0,
    Multiline,
    Number,
    Decimal,
    Email,
    Url,
    Password,
    Phone
};

enum class SensorType : std::uint8_t {
    Accelerometer = 0,
    Gyroscope,
    Magnetometer,
    Light,
    Proximity,
    Gravity,
    LinearAcceleration,
    RotationVector,
    Count
};

enum class MouseButton : std::uint8_t {
    Left = 0,
    Right,
    Middle,
    Back,
    Forward
};

struct Insets {
    int left   = 0;
    int top    = 0;
    int right  = 0;
    int bottom = 0;

    bool empty() const noexcept { return left == 0 && top == 0 && right == 0 && bottom == 0; }
    bool operator==(const Insets& o) const noexcept { return left == o.left && top == o.top && right == o.right && bottom == o.bottom; }
    bool operator!=(const Insets& o) const noexcept { return !(*this == o); }
};

struct DisplayInfo {
    unsigned int width        = 0;
    unsigned int height       = 0;
    int          density_dpi  = 160;
    float        density      = 1.0f;
    float        font_scale   = 1.0f;
    Orientation  orientation  = Orientation::Unspecified;
    bool         night_mode   = false;
    bool         round        = false;
    int          screen_width_dp  = 0;
    int          screen_height_dp = 0;
    int          smallest_width_dp = 0;
    std::string  language;
    std::string  country;
    int          sdk_version  = 0;
};

struct Pointer {
    std::int32_t       id       = 0;
    float              x        = 0.0f;
    float              y        = 0.0f;
    float              pressure = 0.0f;
    float              size     = 0.0f;
    input::PointerType type     = input::PointerType::Touch;
};

struct Event {
    EventType          type      = EventType::None;
    std::int64_t       time_ns   = 0;

    float              x         = 0.0f;
    float              y         = 0.0f;
    std::int32_t       pointer_id = 0;
    input::PointerType pointer   = input::PointerType::Touch;
    float              pressure  = 0.0f;
    float              size      = 0.0f;
    std::vector<Pointer> pointers;

    MouseButton        button    = MouseButton::Left;
    unsigned int       buttons   = 0;
    float              scroll_x  = 0.0f;
    float              scroll_y  = 0.0f;

    input::Key         key       = input::Key::Unknown;
    int                key_code  = 0;
    int                scan_code = 0;
    input::Modifiers   mods      = input::Modifiers::None;
    bool               repeat    = false;
    std::string        text;

    unsigned int       width     = 0;
    unsigned int       height    = 0;
    Insets             insets;

    SensorType         sensor    = SensorType::Accelerometer;
    float              values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    int                  device_id      = 0;
    int                  gamepad        = -1;
    input::GamepadButton gamepad_button = input::GamepadButton::South;
    input::GamepadAxis   gamepad_axis   = input::GamepadAxis::LeftX;
    float                axis_value     = 0.0f;
};

struct GamepadState {
    int                                          slot      = -1;
    int                                          device_id = 0;
    bool                                         connected = false;
    std::string                                  name;
    std::uint16_t                                vendor    = 0;
    std::uint16_t                                product   = 0;
    bool                                         buttons[input::kGamepadButtonCount] = {};
    float                                        axes[input::kGamepadAxisCount]      = {};
};

} // namespace android
} // namespace fizmo

#endif // FIZMO_ANDROID_TYPES_HPP
