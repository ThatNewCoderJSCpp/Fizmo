#include "fizmo_library.hpp"
#include "gamepad_mapping.hpp"

namespace fizmo {
namespace input {

std::string normalize_guid(const std::string& guid) {
    std::string g;
    for (char c : guid) if (std::isxdigit(static_cast<unsigned char>(c))) g.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (g.size() == 32) { g[4] = '0'; g[5] = '0'; g[6] = '0'; g[7] = '0'; }
    return g;
}

std::string make_guid(std::uint16_t bus, std::uint16_t vendor, std::uint16_t product, std::uint16_t version, const std::string& name) {
    char buf[40];
    if (vendor == 0 && product == 0) {
        unsigned char bytes[16] = {};
        bytes[0] = static_cast<unsigned char>(bus & 0xFF);
        bytes[1] = static_cast<unsigned char>(bus >> 8);
        for (std::size_t i = 0; i < name.size() && i < 12; ++i) bytes[4 + i] = static_cast<unsigned char>(name[i]);
        for (int i = 0; i < 16; ++i) std::snprintf(buf + i * 2, 3, "%02x", bytes[i]);
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
                  bus & 0xFF, bus >> 8, vendor & 0xFF, vendor >> 8, product & 0xFF, product >> 8, version & 0xFF, version >> 8);
    return buf;
}

bool parse_binding_source(const std::string& v, GamepadBinding& b) {
    std::size_t i = 0;
    b = GamepadBinding{};
    if (i < v.size() && (v[i] == '+' || v[i] == '-')) { b.input_half = v[i] == '+' ? GamepadBinding::Half::Positive : GamepadBinding::Half::Negative; ++i; }
    if (i >= v.size()) return false;
    const char kind = v[i++];
    std::string rest = v.substr(i);
    if (!rest.empty() && rest.back() == '~') { b.invert = true; rest.pop_back(); }

    if (kind == 'b') { b.source = GamepadBinding::Source::Button; b.index = std::atoi(rest.c_str()); return !rest.empty(); }
    if (kind == 'a') { b.source = GamepadBinding::Source::Axis; b.index = std::atoi(rest.c_str()); return !rest.empty(); }

    if (kind == 'h') {
        const std::size_t dot = rest.find('.');
        if (dot == std::string::npos) return false;
        b.source = GamepadBinding::Source::Hat;
        b.index = std::atoi(rest.substr(0, dot).c_str());
        b.hat_mask = static_cast<std::uint8_t>(std::atoi(rest.substr(dot + 1).c_str()));
        return true;
    }

    return false;
}

bool parse_gamepad_mapping(const std::string& line, GamepadMapping& out) {
    std::string s = line;
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
    if (s.empty() || s[0] == '#') return false;
    std::vector<std::string> fields;
    std::size_t start = 0;

    while (start <= s.size()) {
        const std::size_t comma = s.find(',', start);
        if (comma == std::string::npos) { if (start < s.size()) fields.push_back(s.substr(start)); break; }
        fields.push_back(s.substr(start, comma - start));
        start = comma + 1;
    }

    if (fields.size() < 3) return false;
    out = GamepadMapping{};
    out.guid = normalize_guid(fields[0]);
    out.name = fields[1];
    if (out.guid.size() != 32 && fields[0] != "default" && fields[0] != "xinput") return false;
    if (fields[0] == "default" || fields[0] == "xinput") out.guid = fields[0];

    for (std::size_t f = 2; f < fields.size(); ++f) {
        const std::size_t colon = fields[f].find(':');
        if (colon == std::string::npos) continue;
        std::string key = fields[f].substr(0, colon);
        const std::string value = fields[f].substr(colon + 1);
        if (key == "platform") { out.platform = value; continue; }
        GamepadBinding::Half out_half = GamepadBinding::Half::Full;
        if (!key.empty() && (key[0] == '+' || key[0] == '-')) { out_half = key[0] == '+' ? GamepadBinding::Half::Positive : GamepadBinding::Half::Negative; key.erase(0, 1); }
        GamepadBinding b;
        if (!parse_binding_source(value, b)) continue;
        b.output_half = out_half;
        bool matched = false;

        for (std::size_t i = 0; i < kGamepadButtonCount && !matched; ++i) {
            if (key == gamepad_button_name(static_cast<GamepadButton>(i))) { out.buttons[i] = b; matched = true; }
        }

        for (std::size_t i = 0; i < kGamepadAxisCount && !matched; ++i) {
            if (key != gamepad_axis_name(static_cast<GamepadAxis>(i))) continue;
            if (out_half == GamepadBinding::Half::Full) out.axes[i] = b;
            else out.half_axes.emplace_back(b, static_cast<GamepadAxis>(i));
            matched = true;
        }
    }

    return true;
}

float read_source(const GamepadBinding& b, const RawJoystick& raw) noexcept {
    float v = 0.0f;

    switch (b.source) {
        case GamepadBinding::Source::Button:
            v = b.index >= 0 && static_cast<std::size_t>(b.index) < raw.buttons.size() && raw.buttons[static_cast<std::size_t>(b.index)] ? 1.0f : 0.0f;
            break;
        case GamepadBinding::Source::Axis:
            v = b.index >= 0 && static_cast<std::size_t>(b.index) < raw.axes.size() ? raw.axes[static_cast<std::size_t>(b.index)] : 0.0f;
            if (b.invert) v = -v;
            if (b.input_half == GamepadBinding::Half::Positive) v = std::max(0.0f, v);
            else if (b.input_half == GamepadBinding::Half::Negative) v = std::max(0.0f, -v);
            break;
        case GamepadBinding::Source::Hat:
            v = b.index >= 0 && static_cast<std::size_t>(b.index) < raw.hats.size() && (raw.hats[static_cast<std::size_t>(b.index)] & b.hat_mask) ? 1.0f : 0.0f;
            break;
        default:
            break;
    }

    return v;
}

void apply_mapping(const GamepadMapping& m, const RawJoystick& raw, std::array<bool, kGamepadButtonCount>& buttons, std::array<float, kGamepadAxisCount>& axes) noexcept {
    for (std::size_t i = 0; i < kGamepadButtonCount; ++i) {
        const GamepadBinding& b = m.buttons[i];
        if (b.source == GamepadBinding::Source::None) { buttons[i] = false; continue; }
        const float v = read_source(b, raw);
        buttons[i] = v > 0.5f;
    }

    for (std::size_t i = 0; i < kGamepadAxisCount; ++i) {
        const GamepadBinding& b = m.axes[i];
        const bool trigger = i == static_cast<std::size_t>(GamepadAxis::LeftTrigger) || i == static_cast<std::size_t>(GamepadAxis::RightTrigger);
        if (b.source == GamepadBinding::Source::None) { axes[i] = 0.0f; continue; }
        float v = read_source(b, raw);
        if (trigger && b.source == GamepadBinding::Source::Axis && b.input_half == GamepadBinding::Half::Full) v = (v + 1.0f) * 0.5f;
        axes[i] = trigger ? std::max(0.0f, std::min(1.0f, v)) : std::max(-1.0f, std::min(1.0f, v));
    }

    for (const auto& h : m.half_axes) {
        const float v = read_source(h.first, raw);
        if (v == 0.0f) continue;
        float& a = axes[static_cast<std::size_t>(h.second)];
        a = h.first.output_half == GamepadBinding::Half::Negative ? -v : v;
    }
}

const char* GamepadDatabase::platform_name() noexcept {
#if defined(OS_WINDOWS)
    return "Windows";
#elif defined(OS_LINUX)
    return "Linux";
#else
    return "";
#endif
}

bool GamepadDatabase::add_mapping(const std::string& line) {
    GamepadMapping m;
    if (!parse_gamepad_mapping(line, m)) return false;
    if (!m.platform.empty() && m.platform != platform_name()) return false;
    m_mappings[m.guid] = std::move(m);
    return true;
}

std::size_t GamepadDatabase::add_mappings(const std::string& text) {
    std::size_t n = 0;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) if (add_mapping(line)) ++n;
    return n;
}

std::size_t GamepadDatabase::load_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss;
    ss << f.rdbuf();
    return add_mappings(ss.str());
}

std::size_t GamepadDatabase::load_environment() {
    const char* env = std::getenv("SDL_GAMECONTROLLERCONFIG");
    std::size_t n = env ? add_mappings(env) : 0;
    const char* file = std::getenv("SDL_GAMECONTROLLERCONFIG_FILE");
    if (file) n += load_file(file);
    const char* fizmo = std::getenv("FIZMO_GAMEPAD_DB");
    if (fizmo) n += load_file(fizmo);
    return n;
}

auto GamepadDatabase::find(const std::string& guid) const -> const GamepadMapping* {
    const auto it = m_mappings.find(normalize_guid(guid));
    if (it != m_mappings.end()) return &it->second;
    const auto exact = m_mappings.find(guid);
    return exact != m_mappings.end() ? &exact->second : nullptr;
}

} // namespace input
} // namespace fizmo
