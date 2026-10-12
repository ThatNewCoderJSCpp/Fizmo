#include "fizmo_library.hpp"
#include "hot_reload.hpp"

namespace fizmo {
namespace system {

ImageFormat detect_image_format(const std::vector<std::uint8_t>& bytes) noexcept { return images::detect_format(bytes.data(), bytes.size()); }

bool image_file_complete(const std::vector<std::uint8_t>& bytes, ImageFormat format) noexcept {
    const std::size_t n = bytes.size();
    auto le32 = [&](std::size_t at) -> std::uint32_t { return at + 4 <= n ? static_cast<std::uint32_t>(bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16) | (static_cast<std::uint32_t>(bytes[at + 3]) << 24)) : 0; };

    switch (format) {
        case ImageFormat::Png: {
            static const std::uint8_t iend[4] = { 'I', 'E', 'N', 'D' };
            if (n < 20) return false;
            const std::size_t from = n > 64 ? n - 64 : 8;
            for (std::size_t i = from; i + 4 <= n; ++i) if (std::memcmp(bytes.data() + i, iend, 4) == 0) return true;
            return false;
        }
        case ImageFormat::Jpeg: {
            std::size_t k = n;
            while (k > 2 && bytes[k - 1] == 0) --k;
            return k >= 4 && bytes[k - 2] == 0xFF && bytes[k - 1] == 0xD9;
        }
        case ImageFormat::Bmp: {
            if (n < 26) return false;
            const std::uint32_t size = le32(2);
            return size == 0 || n >= size;
        }
        case ImageFormat::Gif: {
            std::size_t k = n;
            while (k > 0 && bytes[k - 1] == 0) --k;
            return k > 13 && bytes[k - 1] == 0x3B;
        }
        case ImageFormat::WebP: return n >= 12 && n >= static_cast<std::size_t>(le32(4)) + 8;
        case ImageFormat::Qoi: {
            static const std::uint8_t tail[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
            return n >= 22 && std::memcmp(bytes.data() + n - 8, tail, 8) == 0;
        }
        case ImageFormat::Ico:
        case ImageFormat::Cur: {
            if (n < 6) return false;
            const std::size_t count = bytes[4] | (bytes[5] << 8);
            if (n < 6 + count * 16) return false;
            for (std::size_t i = 0; i < count; ++i) {
                const std::uint64_t end = static_cast<std::uint64_t>(le32(6 + i * 16 + 8)) + le32(6 + i * 16 + 12);
                if (end > n) return false;
            }
            return true;
        }
        case ImageFormat::Svg: {
            const std::size_t from = n > 256 ? n - 256 : 0;
            const std::string tail(reinterpret_cast<const char*>(bytes.data()) + from, n - from);
            return tail.find("</svg>") != std::string::npos || tail.find("</svg:svg>") != std::string::npos || (n >= 2 && bytes[0] == 0x1F && bytes[1] == 0x8B);
        }
        case ImageFormat::Tiff:
        case ImageFormat::Tga:
        case ImageFormat::Pnm: return n > 0;
        default: return false;
    }
}

std::optional<images::BitmapImage> load_image_memory(const std::uint8_t* data, std::size_t size, std::string* error) {
    images::BitmapImage img;
    std::string err;
    if (!images::decode_image(data, size, img, &err)) {
        if (error) *error = err;
        return std::nullopt;
    }
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

std::optional<images::AnimatedImage> load_animation(const std::filesystem::path& path, std::string* error) {
    const auto bytes = paths::read_bytes(path);
    if (!bytes) { if (error) *error = "cannot read " + paths::to_utf8(path); return std::nullopt; }
    const ImageFormat format = detect_image_format(*bytes);
    if (format == ImageFormat::Unknown) { if (error) *error = "unknown image format: " + paths::to_utf8(path); return std::nullopt; }
    if (!image_file_complete(*bytes, format)) { if (error) *error = "image file is incomplete: " + paths::to_utf8(path); return std::nullopt; }
    images::AnimatedImage anim;
    std::string err;
    if (!images::decode_animation(bytes->data(), bytes->size(), anim, &err)) {
        if (error) *error = err + ": " + paths::to_utf8(path);
        return std::nullopt;
    }
    return anim;
}

bool save_image(const images::BitmapImage& img, const std::filesystem::path& path, images::ImageFormat format, const images::EncodeOptions& options, std::string* error) {
    if (format == images::ImageFormat::Unknown) format = images::format_from_extension(paths::to_utf8(path));
    if (format == images::ImageFormat::Unknown) format = images::ImageFormat::Png;
    std::vector<std::uint8_t> bytes;
    if (!images::encode_image(img, format, bytes, options, error)) return false;
    if (!paths::write_file(path, bytes)) {
        if (error) *error = "cannot write " + paths::to_utf8(path);
        return false;
    }
    return true;
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
