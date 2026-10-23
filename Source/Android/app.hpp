#ifndef FIZMO_ANDROID_APP_HPP
#define FIZMO_ANDROID_APP_HPP

#include "android_types.hpp"

#if defined(OS_ANDROID)

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct ANativeActivity;
struct ANativeWindow;
struct AAssetManager;

namespace fizmo {
namespace android {

struct AppState;
class App;

enum class SurfaceChange : std::uint8_t {
    Created = 0,
    Destroyed,
    Resized
};

class Surface {
public:
    using PaintCallback  = std::function<void(void*)>;
    using ResizeCallback = std::function<void(unsigned int, unsigned int)>;
    using ChangeCallback = std::function<void(SurfaceChange)>;

private:
    friend class App;
    friend struct AppState;

    ANativeWindow*  m_window     = nullptr;
    unsigned int    m_width      = 0;
    unsigned int    m_height     = 0;
    std::uint64_t   m_generation = 0;
    bool            m_redraw     = false;
    PaintCallback   m_paint;
    struct Listener {
        std::uint64_t  id = 0;
        ResizeCallback resize;
        ChangeCallback change;
    };
    std::vector<Listener> m_listeners;
    std::uint64_t         m_next_listener = 1;

    void attach(ANativeWindow* window) noexcept;
    void detach() noexcept;
    bool refresh_size() noexcept;
    void notify(SurfaceChange change) noexcept;

public:
    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    bool valid() const noexcept { return m_window != nullptr; }
    ANativeWindow* native_window() const noexcept { return m_window; }
    void* native_handle() const noexcept { return m_window; }
    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    std::uint64_t generation() const noexcept { return m_generation; }

    bool redraw_requested() const noexcept { return m_redraw; }
    void clear_redraw() noexcept { m_redraw = false; }
    void request_redraw() noexcept { m_redraw = true; }

    void set_paint_callback(PaintCallback cb) { m_paint = std::move(cb); }
    void paint() noexcept;

    std::uint64_t add_listener(ResizeCallback on_resize, ChangeCallback on_change);
    void remove_listener(std::uint64_t id) noexcept;
};

bool present_pixels(ANativeWindow* window, const std::uint32_t* bgra, unsigned int width, unsigned int height, unsigned int stride_bytes) noexcept;

class Assets {
private:
    AAssetManager* m_manager = nullptr;

public:
    explicit Assets(AAssetManager* manager = nullptr) noexcept : m_manager(manager) {}

    AAssetManager* native() const noexcept { return m_manager; }
    bool available() const noexcept { return m_manager != nullptr; }
    bool exists(const std::string& path) const noexcept;
    std::optional<std::string> read(const std::string& path) const;
    std::vector<std::string> list(const std::string& directory = std::string()) const;
    bool extract(const std::string& path, const std::string& destination) const;
};

enum class LogLevel : std::uint8_t {
    Verbose = 2,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

enum class PickKind : std::uint8_t {
    Open = 0,
    Save,
    Folder
};

struct PickOptions {
    PickKind                 kind     = PickKind::Open;
    bool                     multiple = false;
    std::string              title;
    std::string              suggested_name;
    std::string              start;
    std::vector<std::string> patterns;
};

struct PickResult {
    std::vector<std::string> paths;
    std::string              error;
    bool                     cancelled = false;
};

void log(LogLevel level, const std::string& tag, const std::string& message) noexcept;
void log(const std::string& message) noexcept;
bool redirect_stdio(const std::string& tag);

class App {
private:
    friend struct AppState;
    AppState* m_state;

    explicit App(AppState* state) noexcept : m_state(state) {}

public:
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    static App* current() noexcept;

    ANativeActivity* activity() const noexcept;
    void* java_vm() const noexcept;
    void* activity_object() const noexcept;
    void* jni_env() const noexcept;
    bool has_fizmo_activity() const noexcept;

    bool poll(Event& out, int timeout_ms = 0);
    bool wait(Event& out) { return poll(out, -1); }
    void wake() noexcept;

    bool running() const noexcept;
    bool started() const noexcept;
    bool resumed() const noexcept;
    bool focused() const noexcept;
    bool visible() const noexcept { return started() && surface().valid(); }

    void finish() noexcept;
    void move_to_back() noexcept;

    Surface& surface() noexcept;
    const Surface& surface() const noexcept;

    DisplayInfo display() const;
    float density() const noexcept;
    Insets insets() const noexcept;
    int keyboard_height() const noexcept;
    Insets content_insets() const noexcept;

    void set_orientation(Orientation orientation) noexcept;
    void set_keep_screen_on(bool on) noexcept;
    void set_immersive(bool immersive) noexcept;

    void show_keyboard(KeyboardKind kind = KeyboardKind::Text) noexcept;
    void hide_keyboard() noexcept;
    bool keyboard_requested() const noexcept;

    std::string clipboard_text();
    bool set_clipboard_text(const std::string& text);

    PickResult pick_files(const PickOptions& options);
    bool all_files_access() const noexcept;
    bool declares_all_files_access() const noexcept;

    bool vibrate(unsigned int milliseconds) noexcept;
    bool open_url(const std::string& url) noexcept;
    bool share_text(const std::string& text, const std::string& title = std::string()) noexcept;

    std::string internal_data_path() const;
    std::string external_data_path() const;
    std::string cache_path() const;
    std::string obb_path() const;
    std::string native_library_dir() const;
    std::string package_name() const;
    std::string locale() const;
    int sdk_version() const noexcept;

    const Assets& assets() const noexcept;

    bool has_sensor(SensorType type) const noexcept;
    bool enable_sensor(SensorType type, unsigned int rate_hz = 60) noexcept;
    void disable_sensor(SensorType type) noexcept;

    const std::vector<GamepadState>& gamepads() const noexcept;

    const std::vector<std::uint8_t>& restored_state() const noexcept;
    void set_saved_state(std::vector<std::uint8_t> bytes);
};

} // namespace android
} // namespace fizmo

int fizmo_android_main(fizmo::android::App& app);

#endif // OS_ANDROID
#endif // FIZMO_ANDROID_APP_HPP
