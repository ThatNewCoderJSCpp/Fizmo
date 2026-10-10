#include "fizmo_library.hpp"
#include "hot_reload.hpp"

namespace fizmo {
namespace system {

ImageFormat detect_image_format(const std::vector<std::uint8_t>& bytes) noexcept {
    static const std::uint8_t png[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (bytes.size() >= 8 && std::memcmp(bytes.data(), png, 8) == 0) return ImageFormat::Png;
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) return ImageFormat::Jpeg;
    if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') return ImageFormat::Bmp;
    return ImageFormat::Unknown;
}

bool image_file_complete(const std::vector<std::uint8_t>& bytes, ImageFormat format) noexcept {
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

std::optional<images::BitmapImage> load_image_memory(const std::uint8_t* data, std::size_t size, std::string* error) {
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

std::optional<images::BitmapImage> load_image(const std::filesystem::path& path, std::string* error) {
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

bool save_image_png(const images::BitmapImage& img, const std::filesystem::path& path, bool alpha) {
    std::vector<std::uint8_t> bytes;
    if (!images::encode_png(img, bytes, alpha)) return false;
    return paths::write_file(path, bytes);
}

auto HotReloader::watch(const std::filesystem::path& path, Reload reload) -> WatchId {
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

auto HotReloader::watch(const std::filesystem::path& path, std::function<void(const std::filesystem::path&)> reload) -> WatchId {
    auto fn = std::make_shared<std::function<void(const std::filesystem::path&)>>(std::move(reload));
    return watch(path, Reload([fn](const std::filesystem::path& p, std::string&) { (*fn)(p); return true; }));
}

auto HotReloader::watch_directory(const std::filesystem::path& dir, Reload reload, bool recursive) -> WatchId {
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

auto HotReloader::watch_image(images::BitmapImage& image, const std::filesystem::path& path) -> WatchId {
    images::BitmapImage* target = &image;
    return watch(path, Reload([target](const std::filesystem::path& p, std::string& error) {
        auto loaded = load_image(p, &error);
        if (!loaded) return false;
        *target = std::move(*loaded);
        return true;
    }));
}

auto HotReloader::watch_image(const std::shared_ptr<images::BitmapImage>& image, const std::filesystem::path& path) -> WatchId {
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

auto HotReloader::watch_text(std::string& text, const std::filesystem::path& path) -> WatchId {
    std::string* target = &text;
    return watch(path, Reload([target](const std::filesystem::path& p, std::string& error) {
        auto loaded = paths::read_text(p);
        if (!loaded) { error = "cannot read " + paths::to_utf8(p); return false; }
        *target = std::move(*loaded);
        return true;
    }));
}

} // namespace system
} // namespace fizmo
