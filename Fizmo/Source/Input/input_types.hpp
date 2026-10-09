#ifndef FIZMO_INPUT_TYPES_HPP
#define FIZMO_INPUT_TYPES_HPP

#include "keys.hpp"
#include <cstdint>

namespace fizmo {
namespace input {

enum class MouseButton : std::uint8_t {
    None   = 0,
    Left   = 1,
    Right  = 2,
    Middle = 3,
    X1     = 4,
    X2     = 5
};

enum class PointerType : std::uint8_t {
    Mouse = 0,
    Touch,
    Pen
};

enum class GestureType : std::uint8_t {
    None = 0,
    Tap,
    DoubleTap,
    LongPress,
    Pan,
    Pinch,
    Rotate,
    Swipe
};

enum class GesturePhase : std::uint8_t {
    Begin = 0,
    Update,
    End,
    Cancel
};

enum class SwipeDirection : std::uint8_t {
    None = 0,
    Left,
    Right,
    Up,
    Down
};

struct GestureData {
    GestureType    type      = GestureType::None;
    GesturePhase   phase     = GesturePhase::Begin;
    SwipeDirection direction = SwipeDirection::None;
    int            touches   = 0;
    float          x         = 0.0f;
    float          y         = 0.0f;
    float          dx        = 0.0f;
    float          dy        = 0.0f;
    float          total_dx  = 0.0f;
    float          total_dy  = 0.0f;
    float          scale       = 1.0f;
    float          scale_delta = 1.0f;
    float          rotation       = 0.0f;
    float          rotation_delta = 0.0f;
    float          velocity_x = 0.0f;
    float          velocity_y = 0.0f;
};

enum class GamepadButton : std::uint8_t {
    South = 0,
    East,
    West,
    North,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Misc1,
    Paddle1,
    Paddle2,
    Paddle3,
    Paddle4,
    Touchpad,
    Count,
    A = South,
    B = East,
    X = West,
    Y = North
};

enum class GamepadAxis : std::uint8_t {
    LeftX = 0,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
    Count
};

inline constexpr std::size_t kGamepadButtonCount = static_cast<std::size_t>(GamepadButton::Count);
inline constexpr std::size_t kGamepadAxisCount   = static_cast<std::size_t>(GamepadAxis::Count);

constexpr const char* gamepad_button_name(GamepadButton b) noexcept {
    switch (b) {
        case GamepadButton::South:         return "a";
        case GamepadButton::East:          return "b";
        case GamepadButton::West:          return "x";
        case GamepadButton::North:         return "y";
        case GamepadButton::Back:          return "back";
        case GamepadButton::Guide:         return "guide";
        case GamepadButton::Start:         return "start";
        case GamepadButton::LeftStick:     return "leftstick";
        case GamepadButton::RightStick:    return "rightstick";
        case GamepadButton::LeftShoulder:  return "leftshoulder";
        case GamepadButton::RightShoulder: return "rightshoulder";
        case GamepadButton::DpadUp:        return "dpup";
        case GamepadButton::DpadDown:      return "dpdown";
        case GamepadButton::DpadLeft:      return "dpleft";
        case GamepadButton::DpadRight:     return "dpright";
        case GamepadButton::Misc1:         return "misc1";
        case GamepadButton::Paddle1:       return "paddle1";
        case GamepadButton::Paddle2:       return "paddle2";
        case GamepadButton::Paddle3:       return "paddle3";
        case GamepadButton::Paddle4:       return "paddle4";
        case GamepadButton::Touchpad:      return "touchpad";
        default:                           return "";
    }
}

constexpr const char* gamepad_axis_name(GamepadAxis a) noexcept {
    switch (a) {
        case GamepadAxis::LeftX:        return "leftx";
        case GamepadAxis::LeftY:        return "lefty";
        case GamepadAxis::RightX:       return "rightx";
        case GamepadAxis::RightY:       return "righty";
        case GamepadAxis::LeftTrigger:  return "lefttrigger";
        case GamepadAxis::RightTrigger: return "righttrigger";
        default:                        return "";
    }
}

} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_TYPES_HPP
