#ifndef FIZMO_INPUT_GAMEPAD_MAPPING_HPP
#define FIZMO_INPUT_GAMEPAD_MAPPING_HPP

#include "input_types.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace input {

struct RawJoystick {
    std::vector<bool>         buttons;
    std::vector<float>        axes;
    std::vector<std::uint8_t> hats;
};

enum class HatMask : std::uint8_t { Up = 1, Right = 2, Down = 4, Left = 8 };

struct GamepadBinding {
    enum class Source : std::uint8_t { None = 0, Button, Axis, Hat };
    enum class Half : std::uint8_t { Full = 0, Positive, Negative };

    Source       source      = Source::None;
    int          index       = 0;
    std::uint8_t hat_mask    = 0;
    Half         input_half  = Half::Full;
    bool         invert      = false;
    Half         output_half = Half::Full;
};

struct GamepadMapping {
    std::string guid;
    std::string name;
    std::string platform;
    std::array<GamepadBinding, kGamepadButtonCount> buttons{};
    std::array<GamepadBinding, kGamepadAxisCount>   axes{};
    std::vector<std::pair<GamepadBinding, GamepadAxis>> half_axes;
};

std::string normalize_guid(const std::string& guid);

std::string make_guid(std::uint16_t bus, std::uint16_t vendor, std::uint16_t product, std::uint16_t version, const std::string& name = std::string());

bool parse_binding_source(const std::string& v, GamepadBinding& b);

bool parse_gamepad_mapping(const std::string& line, GamepadMapping& out);

float read_source(const GamepadBinding& b, const RawJoystick& raw) noexcept;

void apply_mapping(const GamepadMapping& m, const RawJoystick& raw, std::array<bool, kGamepadButtonCount>& buttons, std::array<float, kGamepadAxisCount>& axes) noexcept;

class GamepadDatabase {
private:
    std::unordered_map<std::string, GamepadMapping> m_mappings;

    static const char* platform_name() noexcept;

public:
    bool add_mapping(const std::string& line);

    std::size_t add_mappings(const std::string& text);

    std::size_t load_file(const std::string& path);

    std::size_t load_environment();

    const GamepadMapping* find(const std::string& guid) const;

    bool remove(const std::string& guid) { return m_mappings.erase(normalize_guid(guid)) > 0; }
    std::size_t size() const noexcept { return m_mappings.size(); }
    void clear() noexcept { m_mappings.clear(); }
};

} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_GAMEPAD_MAPPING_HPP
