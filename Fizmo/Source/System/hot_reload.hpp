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

inline ImageFormat detect_image_format(const std::vector<std::uint8_t>& bytes) noexcept {
    static const std::uint8_t png[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (bytes.size() >= 8 && std::memcmp(bytes.data(), png, 8) == 0) return ImageFormat::Png;
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) return ImageFormat::Jpeg;
    if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') return ImageFormat::Bmp;
    return ImageFormat::Unknown;
}

inline bool image_file_complete(const std::vector<std::uint8_t>& bytes, ImageFormat format) noexcept {
    switch (format) {
        case ImageFormat::Png: {
            static const std::uint8_t iend[4] = { 'I', 'E', 'N', 'D' };
            if (bytes.size() < 20) return false;
            const std::size_t from = bytes.size() > 64 ? bytes.size() - 64 : 8;
            for (std::size_t i = from; i + 4 <= bytes.size(); ++i) if (std::memcmp(bytes.data() + i, iend, 4) == 0) return true;
            return false;
        }
        case ImageFormat::Jpeg: {
            std::size_t n = bytes.size();
            while (n > 2 && bytes[n - 1] == 0) --n;
            return n >= 4 && bytes[n - 2] == 0xFF && bytes[n - 1] == 0xD9;
        }
        case ImageFormat::Bmp: {
            if (bytes.size() < 54) return false;
            std::uint32_t size = 0;
            std::memcpy(&size, bytes.data() + 2, 4);
            return size == 0 || bytes.size() >= size;
        }
        default:
            return false;
    }
}

inline std::optional<images::BitmapImage> load_image_memory(const std::uint8_t* data, std::size_t size, std::string* error = nullptr) {
    images::BitmapImage img;
    std::string err;
    bool ok = false;
    if (images::is_png(data, size)) ok = images::decode_png(data, size, img, &err);
    else if (images::is_jpeg(data, size)) ok = images::decode_jpeg(data, size, img, &err);
    else if (size >= 2 && data[0] == 'B' && data[1] == 'M') ok = images::decode_bmp(data, size, img, &err);
    else err = "unknown image format";
    if (!ok) { if (error) *error = err; return std::nullopt; }
    return img;
}

inline std::optional<images::BitmapImage> load_image(const std::filesystem::path& path, std::string* error = nullptr) {
    const auto bytes = paths::read_bytes(path);
    if (!bytes) { if (error) *error = "cannot read " + paths::to_utf8(path); return std::nullopt; }
    const ImageFormat format = detect_image_format(*bytes);
    if (format == ImageFormat::Unknown) { if (error) *error = "unknown image format: " + paths::to_utf8(path); return std::nullopt; }
    if (!image_file_complete(*bytes, format)) { if (error) *error = "image file is incomplete: " + paths::to_utf8(path); return std::nullopt; }
    std::string err;
    auto img = load_image_memory(bytes->data(), bytes->size(), &err);
    if (!img && error) *error = err + ": " + paths::to_utf8(path);
    return img;
}

inline bool save_image_png(const images::BitmapImage& img, const std::filesystem::path& path, bool alpha = true) {
    std::vector<std::uint8_t> bytes;
    if (!images::encode_png(img, bytes, alpha)) return false;
    return paths::write_file(path, bytes);
}

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

    WatchId watch(const std::filesystem::path& path, Reload reload) {
        auto fn = std::make_shared<Reload>(std::move(reload));
        return m_watcher.watch(path, [this, fn](const FileChange& c) {
            if (c.action == FileAction::Removed) return;
            ReloadEvent e{ c.path, c.action, true, {} };
            try {
                e.ok = (*fn)(c.path, e.error);
            } catch (const std::exception& ex) {
                e.ok = false;
                e.error = ex.what();
            }
            report(e);
        });
    }

    WatchId watch(const std::filesystem::path& path, std::function<void(const std::filesystem::path&)> reload) {
        auto fn = std::make_shared<std::function<void(const std::filesystem::path&)>>(std::move(reload));
        return watch(path, Reload([fn](const std::filesystem::path& p, std::string&) { (*fn)(p); return true; }));
    }

    WatchId watch_directory(const std::filesystem::path& dir, Reload reload, bool recursive = true) {
        auto fn = std::make_shared<Reload>(std::move(reload));
        return m_watcher.watch(dir, [this, fn](const FileChange& c) {
            std::error_code ec;
            if (c.action == FileAction::Removed || std::filesystem::is_directory(c.path, ec)) return;
            ReloadEvent e{ c.path, c.action, true, {} };
            try {
                e.ok = (*fn)(c.path, e.error);
            } catch (const std::exception& ex) {
                e.ok = false;
                e.error = ex.what();
            }
            report(e);
        }, recursive);
    }

    WatchId watch_image(images::BitmapImage& image, const std::filesystem::path& path) {
        images::BitmapImage* target = &image;
        return watch(path, Reload([target](const std::filesystem::path& p, std::string& error) {
            auto loaded = load_image(p, &error);
            if (!loaded) return false;
            *target = std::move(*loaded);
            return true;
        }));
    }

    WatchId watch_image(const std::shared_ptr<images::BitmapImage>& image, const std::filesystem::path& path) {
        std::weak_ptr<images::BitmapImage> weak = image;
        return watch(path, Reload([weak](const std::filesystem::path& p, std::string& error) {
            auto target = weak.lock();
            if (!target) return true;
            auto loaded = load_image(p, &error);
            if (!loaded) return false;
            *target = std::move(*loaded);
            return true;
        }));
    }

    WatchId watch_text(std::string& text, const std::filesystem::path& path) {
        std::string* target = &text;
        return watch(path, Reload([target](const std::filesystem::path& p, std::string& error) {
            auto loaded = paths::read_text(p);
            if (!loaded) { error = "cannot read " + paths::to_utf8(p); return false; }
            *target = std::move(*loaded);
            return true;
        }));
    }

    bool unwatch(WatchId id) { return m_watcher.unwatch(id); }
    void clear() { m_watcher.clear(); }

    std::size_t update() { return m_enabled ? m_watcher.poll() : 0; }
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_HOT_RELOAD_HPP
