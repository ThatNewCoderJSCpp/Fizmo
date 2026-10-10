#ifndef FIZMO_INPUT_GAMEPAD_HPP
#define FIZMO_INPUT_GAMEPAD_HPP

#include "../Basic/fizmo_defines.hpp"
#include "input_types.hpp"
#include "gamepad_mapping.hpp"
#include "raw_input_hook.hpp"
#include "../Windows/window_events.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace input {

struct GamepadInfo {
    std::string   name;
    std::string   guid;
    std::string   path;
    std::uint16_t bus     = 0;
    std::uint16_t vendor  = 0;
    std::uint16_t product = 0;
    std::uint16_t version = 0;
    bool          rumble  = false;
    bool          xinput  = false;
};

namespace detail {

struct GamepadDevice {
    int            key = 0;
    GamepadInfo    info;
    RawJoystick    raw;
    GamepadMapping fallback;
    bool           has_fallback = false;
    bool           alive = true;
    std::chrono::steady_clock::time_point rumble_until{};
    bool           rumbling = false;

    virtual ~GamepadDevice() = default;
    virtual bool set_rumble(float, float, unsigned int) noexcept { return false; }
};

class GamepadBackend {
public:
    virtual ~GamepadBackend() = default;
    virtual void poll(std::vector<GamepadDevice*>& added, std::vector<int>& removed) = 0;
    virtual void set_window(void*) noexcept {}
};

} // namespace detail

class GamepadManager;

class Gamepad {
private:
    friend class GamepadManager;

    int                                      m_slot = -1;
    int                                      m_key  = 0;
    bool                                     m_connected = false;
    bool                                     m_mapped = false;
    GamepadInfo                              m_info;
    detail::GamepadDevice*                   m_device = nullptr;
    const GamepadMapping*                    m_mapping = nullptr;
    std::array<bool, kGamepadButtonCount>    m_buttons{};
    std::array<bool, kGamepadButtonCount>    m_previous{};
    std::array<float, kGamepadAxisCount>     m_axes{};
    std::array<float, kGamepadAxisCount>     m_last_sent{};
    float                                    m_stick_deadzone   = 0.12f;
    float                                    m_trigger_deadzone = 0.04f;
    RawJoystick                              m_empty_raw;

    static float shape(float v, float dz) noexcept;

public:
    int slot() const noexcept { return m_slot; }
    bool connected() const noexcept { return m_connected; }
    bool mapped() const noexcept { return m_mapped; }
    const GamepadInfo& info() const noexcept { return m_info; }
    const std::string& name() const noexcept { return m_info.name; }
    const std::string& guid() const noexcept { return m_info.guid; }

    bool button(GamepadButton b) const noexcept { return m_buttons[static_cast<std::size_t>(b)]; }
    bool just_pressed(GamepadButton b) const noexcept { const std::size_t i = static_cast<std::size_t>(b); return m_buttons[i] && !m_previous[i]; }
    bool just_released(GamepadButton b) const noexcept { const std::size_t i = static_cast<std::size_t>(b); return !m_buttons[i] && m_previous[i]; }

    float raw_axis(GamepadAxis a) const noexcept { return m_axes[static_cast<std::size_t>(a)]; }

    float axis(GamepadAxis a) const noexcept;

    void set_deadzones(float stick, float trigger) noexcept;

    bool has_rumble() const noexcept { return m_connected && m_info.rumble; }

    bool rumble(float low_frequency, float high_frequency, unsigned int duration_ms) noexcept;

    bool stop_rumble() noexcept { return rumble(0.0f, 0.0f, 0); }

    const RawJoystick& raw() const noexcept { return m_device ? m_device->raw : m_empty_raw; }
};

class GamepadManager {
public:
    using Callback = std::function<void(Gamepad&)>;

private:
    std::unique_ptr<detail::GamepadBackend> m_backend;
    std::deque<Gamepad>                     m_pads;
    GamepadDatabase                         m_db;
    windows::WindowEventHandler*            m_events = nullptr;
    Callback                                m_on_connected;
    Callback                                m_on_disconnected;
    std::vector<detail::GamepadDevice*>     m_added;
    std::vector<int>                        m_removed;
    float                                   m_stick_dz = 0.12f;
    float                                   m_trigger_dz = 0.04f;
    float                                   m_axis_event_threshold = 0.01f;

    Gamepad& claim_slot();

    void resolve_mapping(Gamepad& g);

    void emit(windows::WindowEventType type, const Gamepad& g, int index, float value);

public:
    GamepadManager();

    explicit GamepadManager(std::unique_ptr<detail::GamepadBackend> backend) : m_backend(std::move(backend)) {}

    GamepadManager(const GamepadManager&) = delete;
    GamepadManager& operator=(const GamepadManager&) = delete;

    const GamepadDatabase& database() const noexcept { return m_db; }

    bool add_mapping(const std::string& line);

    bool remove_mapping(const std::string& guid);
    void attach(windows::WindowEventHandler& events) noexcept { m_events = &events; }
    void detach() noexcept { m_events = nullptr; }
    void set_window(void* native_handle) noexcept { if (m_backend) m_backend->set_window(native_handle); }
    void on_connected(Callback cb) { m_on_connected = std::move(cb); }
    void on_disconnected(Callback cb) { m_on_disconnected = std::move(cb); }

    void set_deadzones(float stick, float trigger) noexcept;

    std::size_t add_mappings(const std::string& text);

    std::size_t load_mappings(const std::string& path);

    void update();

    std::size_t count() const noexcept {
        std::size_t n = 0;
        for (const Gamepad& g : m_pads) if (g.m_connected) ++n;
        return n;
    }

    std::size_t slots() const noexcept { return m_pads.size(); }
    Gamepad* get(int slot) noexcept;
    const Gamepad* get(int slot) const noexcept;

    Gamepad* primary() noexcept {
        for (Gamepad& g : m_pads) if (g.m_connected) return &g;
        return nullptr;
    }

    std::vector<Gamepad*> connected();

    bool any_button(GamepadButton b) const noexcept {
        for (const Gamepad& g : m_pads) if (g.m_connected && g.button(b)) return true;
        return false;
    }
};

} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_GAMEPAD_HPP
