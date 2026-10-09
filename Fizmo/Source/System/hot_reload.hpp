#ifndef FIZMO_SYSTEM_HOT_RELOAD_HPP
#define FIZMO_SYSTEM_HOT_RELOAD_HPP

#include "file_watcher.hpp"
#include "paths.hpp"
#include "../Images/Bitmap/image.hpp"
#include "../Images/Normal/png.hpp"
#include "../Images/Normal/jpg.hpp"
#include "../Images/Normal/png_codec.hpp"
#include "../Images/Normal/jpeg_codec.hpp"
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace system {

enum class ImageFormat : std::uint8_t { Unknown = 0, Bmp, Png, Jpeg };

 ImageFormat detect_image_format(const std::vector<std::uint8_t>& bytes) noexcept;

 bool image_file_complete(const std::vector<std::uint8_t>& bytes, ImageFormat format) noexcept;

 std::optional<images::BitmapImage> load_image_memory(const std::uint8_t* data, std::size_t size, std::string* error = nullptr);

 std::optional<images::BitmapImage> load_image(const std::filesystem::path& path, std::string* error = nullptr);

 bool save_image_png(const images::BitmapImage& img, const std::filesystem::path& path, bool alpha = true);

struct ReloadEvent {
    std::filesystem::path path;
    FileAction            action = FileAction::Modified;
    bool                  ok     = true;
    std::string           error;
};

class HotReloader {
public:
    using WatchId  = FileWatcher::WatchId;
    using Reload   = std::function<bool(const std::filesystem::path&, std::string&)>;
    using Listener = std::function<void(const ReloadEvent&)>;

private:
    FileWatcher m_watcher;
    bool        m_enabled = true;
    Listener    m_listener;
    std::size_t m_reloads = 0;
    std::size_t m_failures = 0;

    void report(const ReloadEvent& e) {
        if (e.ok) ++m_reloads;
        else ++m_failures;
        if (m_listener) m_listener(e);
    }

public:
    explicit HotReloader(WatchBackend backend = WatchBackend::Native) : m_watcher(backend) {}

    FileWatcher& watcher() noexcept { return m_watcher; }
    void set_enabled(bool enabled) noexcept { m_enabled = enabled; }
    bool enabled() const noexcept { return m_enabled; }
    void on_reload(Listener l) { m_listener = std::move(l); }
    std::size_t reload_count() const noexcept { return m_reloads; }
    std::size_t failure_count() const noexcept { return m_failures; }

    WatchId watch(const std::filesystem::path& path, Reload reload);

    WatchId watch(const std::filesystem::path& path, std::function<void(const std::filesystem::path&)> reload);

    WatchId watch_directory(const std::filesystem::path& dir, Reload reload, bool recursive = true);

    WatchId watch_image(images::BitmapImage& image, const std::filesystem::path& path);

    WatchId watch_image(const std::shared_ptr<images::BitmapImage>& image, const std::filesystem::path& path);

    WatchId watch_text(std::string& text, const std::filesystem::path& path);

    bool unwatch(WatchId id) { return m_watcher.unwatch(id); }
    void clear() { m_watcher.clear(); }

    std::size_t update() { return m_enabled ? m_watcher.poll() : 0; }
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_HOT_RELOAD_HPP
