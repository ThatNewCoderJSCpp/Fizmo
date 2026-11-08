#include "fizmo_library.hpp"
#include "texture_library.hpp"
#include "../Images/Formats/image_formats.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace fizmo {
namespace graphics {

namespace fs = std::filesystem;

ColorSpace default_color_space(TextureRole role) noexcept {
    return role == TextureRole::Color || role == TextureRole::Emissive ? ColorSpace::SRGB : ColorSpace::Linear;
}

std::uint32_t TextureSettings::key() const noexcept {
    return static_cast<std::uint32_t>(color_space) | (static_cast<std::uint32_t>(filter) << 2) | (static_cast<std::uint32_t>(wrap) << 4)
         | (mipmaps ? 1u << 6 : 0u) | (flip_y ? 1u << 7 : 0u);
}

lighting::ColorSpace to_lighting(ColorSpace space) noexcept {
    return space == ColorSpace::SRGB ? lighting::ColorSpace::srgb : lighting::ColorSpace::linear;
}

lighting::TextureFilter to_lighting(SampleFilter filter) noexcept {
    return filter == SampleFilter::Nearest ? lighting::TextureFilter::nearest : lighting::TextureFilter::bilinear;
}

lighting::TextureWrap to_lighting(WrapMode wrap) noexcept {
    switch (wrap) {
    case WrapMode::Repeat: return lighting::TextureWrap::repeat;
    case WrapMode::MirrorRepeat: return lighting::TextureWrap::mirror;
    case WrapMode::Clamp: break;
    }
    return lighting::TextureWrap::clamp;
}

lighting::UvTransform to_lighting(const UvTransform& t) noexcept {
    return lighting::UvTransform::make({ t.scale_u, t.scale_v }, { t.offset_u, t.offset_v }, t.rotation);
}

std::shared_ptr<lighting::Texture> to_lighting_texture(const Texture& texture) {
    auto out = std::make_shared<lighting::Texture>();
    const images::BitmapImage* img = texture.image();
    if (!img || !img->is_valid_image()) return out;
    out->set_color_space(to_lighting(texture.color_space()));
    out->assign_rgba8(img->width(), img->height(), img->to_rgba());
    return out;
}

lighting::TextureMap to_lighting_map(const std::shared_ptr<const lighting::Texture>& texture, const Texture& settings_from, int channel) {
    lighting::TextureMap m(texture);
    m.filter = to_lighting(settings_from.filter());
    m.wrap = to_lighting(settings_from.wrap());
    m.channel = channel;
    return m;
}

std::string TextureLibrary::normalize_path(const std::string& path) {
    std::error_code ec;
    fs::path p = fs::absolute(fs::path(path), ec);
    if (ec) return fs::path(path).lexically_normal().generic_string();
    fs::path canon = fs::weakly_canonical(p, ec);
    return (ec ? p.lexically_normal() : canon).generic_string();
}

std::string TextureLibrary::key_of(const std::string& normalized, const TextureSettings& settings) {
    return normalized + '|' + std::to_string(settings.key());
}

bool TextureLibrary::file_stamp(const std::string& path, std::int64_t& stamp, std::uint64_t& size) {
    std::error_code ec;
    const auto t = fs::last_write_time(fs::path(path), ec);
    if (ec) return false;
    stamp = static_cast<std::int64_t>(t.time_since_epoch().count());
    const auto bytes = fs::file_size(fs::path(path), ec);
    size = ec ? 0 : static_cast<std::uint64_t>(bytes);
    return true;
}

images::BitmapImage TextureLibrary::flip_vertical(const images::BitmapImage& image) {
    const unsigned int w = image.width(), h = image.height();
    std::vector<colors::Color> px(image.pixels().size());
    for (unsigned int y = 0; y < h; ++y)
        std::copy_n(image.pixels().begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(h - 1 - y) * w), w, px.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * w));
    return images::BitmapImage(w, h, std::move(px));
}

images::BitmapImage TextureLibrary::solid(const Color& color, unsigned int size) {
    size = std::max(size, 1u);
    return images::BitmapImage(size, size, std::vector<colors::Color>(static_cast<std::size_t>(size) * size, color));
}

images::BitmapImage TextureLibrary::pack_orm(const images::BitmapImage* occlusion, const images::BitmapImage* roughness, const images::BitmapImage* metallic) {
    unsigned int w = 1, h = 1;
    for (const images::BitmapImage* img : { occlusion, roughness, metallic }) {
        if (!img || !img->is_valid_image()) continue;
        w = std::max(w, img->width());
        h = std::max(h, img->height());
    }
    auto channel = [w, h](const images::BitmapImage* img, unsigned int x, unsigned int y) -> std::uint8_t {
        if (!img || !img->is_valid_image()) return 255;
        const unsigned int sx = std::min(img->width() - 1, static_cast<unsigned int>(static_cast<std::uint64_t>(x) * img->width() / w));
        const unsigned int sy = std::min(img->height() - 1, static_cast<unsigned int>(static_cast<std::uint64_t>(y) * img->height() / h));
        return img->get_pixel(sx, sy).red();
    };
    std::vector<colors::Color> px(static_cast<std::size_t>(w) * h);
    for (unsigned int y = 0; y < h; ++y)
        for (unsigned int x = 0; x < w; ++x)
            px[static_cast<std::size_t>(y) * w + x] = colors::Color(channel(occlusion, x, y), channel(roughness, x, y), channel(metallic, x, y), 255);
    return images::BitmapImage(w, h, std::move(px));
}

bool TextureLibrary::read_image(const std::string& path, const TextureSettings& settings, images::BitmapImage& out) {
    std::string err;
    images::BitmapImage img;
    if (!images::load_image(path, img, &err) || !img.is_valid_image()) {
        m_error = path + ": " + (err.empty() ? std::string("could not decode image") : err);
        ++m_stats.failures;
        return false;
    }
    out = settings.flip_y ? flip_vertical(img) : std::move(img);
    return true;
}

void TextureLibrary::fill_lighting(Entry& e) {
    if (!e.lighting) e.lighting = std::make_shared<lighting::Texture>();
    const images::BitmapImage* img = e.texture ? e.texture->image() : nullptr;
    e.lighting->set_color_space(to_lighting(e.settings.color_space));
    if (img && img->is_valid_image()) e.lighting->assign_rgba8(img->width(), img->height(), img->to_rgba());
    else e.lighting->assign_rgba8(0, 0, {});
}

TextureLibrary::Entry* TextureLibrary::entry_locked(const std::string& path, const TextureSettings& settings, bool count) {
    auto named = m_entries.find(key_of(path, settings));
    if (named != m_entries.end() && !named->second.file) {
        if (count) ++m_stats.hits;
        return &named->second;
    }
    const std::string norm = normalize_path(path);
    const std::string key = key_of(norm, settings);
    auto it = m_entries.find(key);
    if (it != m_entries.end()) {
        if (count) ++m_stats.hits;
        return &it->second;
    }
    if (count) ++m_stats.misses;
    images::BitmapImage img;
    if (!read_image(norm, settings, img)) return nullptr;
    Entry e;
    e.path = norm;
    e.settings = settings;
    file_stamp(norm, e.stamp, e.file_size);
    auto tex = std::make_shared<Texture>(std::move(img), settings.filter, settings.wrap);
    tex->set_color_space(settings.color_space);
    tex->set_mipmaps(settings.mipmaps);
    e.texture = std::move(tex);
    return &m_entries.emplace(key, std::move(e)).first->second;
}

std::shared_ptr<const Texture> TextureLibrary::load(const std::string& path, const TextureSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Entry* e = entry_locked(path, settings, true);
    return e ? e->texture : nullptr;
}

std::shared_ptr<const Texture> TextureLibrary::load(const std::string& path, TextureRole role) {
    return load(path, TextureSettings::for_role(role));
}

std::shared_ptr<const Texture> TextureLibrary::find(const std::string& path, const TextureSettings& settings) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto named = m_entries.find(key_of(path, settings));
    if (named != m_entries.end() && !named->second.file) return named->second.texture;
    auto it = m_entries.find(key_of(normalize_path(path), settings));
    return it == m_entries.end() ? nullptr : it->second.texture;
}

bool TextureLibrary::contains(const std::string& path, const TextureSettings& settings) const {
    return find(path, settings) != nullptr;
}

std::shared_ptr<const Texture> TextureLibrary::add(const std::string& name, images::BitmapImage image, const TextureSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string key = key_of(name, settings);
    Entry& e = m_entries[key];
    e.path = name;
    e.settings = settings;
    e.file = false;
    images::BitmapImage img = settings.flip_y ? flip_vertical(image) : std::move(image);
    if (e.texture) {
        e.texture->set_image(std::make_shared<images::BitmapImage>(std::move(img)));
    } else {
        e.texture = std::make_shared<Texture>(std::move(img), settings.filter, settings.wrap);
        e.texture->set_color_space(settings.color_space);
        e.texture->set_mipmaps(settings.mipmaps);
    }
    if (e.lighting) fill_lighting(e);
    return e.texture;
}

std::shared_ptr<const lighting::Texture> TextureLibrary::load_lighting(const std::string& path, const TextureSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Entry* e = entry_locked(path, settings, true);
    if (!e) return nullptr;
    if (!e->lighting) fill_lighting(*e);
    return e->lighting;
}

std::shared_ptr<const lighting::Texture> TextureLibrary::load_lighting(const std::string& path, TextureRole role) {
    return load_lighting(path, TextureSettings::for_role(role));
}

lighting::TextureMap TextureLibrary::lighting_map(const std::string& path, TextureRole role, int channel) {
    const TextureSettings s = TextureSettings::for_role(role);
    lighting::TextureMap m(load_lighting(path, s));
    m.filter = to_lighting(s.filter);
    m.wrap = to_lighting(s.wrap);
    m.channel = channel;
    return m;
}

bool TextureLibrary::refresh_locked(Entry& e) {
    if (!e.file) return false;
    images::BitmapImage img;
    if (!read_image(e.path, e.settings, img)) return false;
    file_stamp(e.path, e.stamp, e.file_size);
    e.texture->set_image(std::make_shared<images::BitmapImage>(std::move(img)));
    if (e.lighting) fill_lighting(e);
    ++m_stats.reloads;
    return true;
}

bool TextureLibrary::changed(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string norm = normalize_path(path);
    for (const auto& kv : m_entries) {
        const Entry& e = kv.second;
        if (!e.file || e.path != norm) continue;
        std::int64_t stamp = 0;
        std::uint64_t size = 0;
        if (file_stamp(e.path, stamp, size) && (stamp != e.stamp || size != e.file_size)) return true;
    }
    return false;
}

bool TextureLibrary::reload(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string norm = normalize_path(path);
    bool any = false;
    for (auto& kv : m_entries)
        if (kv.second.path == norm && refresh_locked(kv.second)) any = true;
    return any;
}

std::size_t TextureLibrary::reload_changed() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t n = 0;
    for (auto& kv : m_entries) {
        Entry& e = kv.second;
        if (!e.file) continue;
        std::int64_t stamp = 0;
        std::uint64_t size = 0;
        if (!file_stamp(e.path, stamp, size) || (stamp == e.stamp && size == e.file_size)) continue;
        if (refresh_locked(e)) ++n;
    }
    return n;
}

void TextureLibrary::remove(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string norm = normalize_path(path);
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it->second.path == norm || it->second.path == path) it = m_entries.erase(it);
        else ++it;
    }
}

std::size_t TextureLibrary::prune() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t n = 0;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        const bool unused = it->second.texture.use_count() <= 1 && (!it->second.lighting || it->second.lighting.use_count() <= 1);
        if (unused) {
            it = m_entries.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    return n;
}

void TextureLibrary::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

std::size_t TextureLibrary::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

TextureLibrary::Stats TextureLibrary::stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Stats s = m_stats;
    s.entries = m_entries.size();
    s.bytes = 0;
    for (const auto& kv : m_entries) {
        const Texture* t = kv.second.texture.get();
        if (t) s.bytes += static_cast<std::uint64_t>(t->width()) * t->height() * 4;
    }
    return s;
}

std::string TextureLibrary::last_error() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_error;
}

} // namespace graphics
} // namespace fizmo
