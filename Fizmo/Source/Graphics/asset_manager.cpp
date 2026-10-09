#include "fizmo_library.hpp"
#include "asset_manager.hpp"

namespace fizmo {
namespace graphics {

std::string AssetManager::key_of(const std::filesystem::path& p) {
    std::error_code ec;
    const std::filesystem::path c = std::filesystem::weakly_canonical(p, ec);
    return system::paths::to_utf8(ec ? p : c);
}

void AssetManager::watch_path(const std::string& key, std::function<bool(const std::filesystem::path&, std::string&)> fn) {
    if (!m_reloader || m_watches.count(key)) return;
    const system::HotReloader::WatchId id = m_reloader->watch(system::paths::from_utf8(key), system::HotReloader::Reload([this, fn](const std::filesystem::path& p, std::string& err) {
        std::lock_guard<std::recursive_mutex> g(m_lock);
        const bool ok = fn(p, err);
        if (ok) ++m_reloads;
        return ok;
    }));
    if (id) m_watches[key] = id;
}

void AssetManager::watch_image(const std::string& key) {
    watch_path(key, [this, key](const std::filesystem::path& p, std::string& err) {
        const auto it = m_images.find(key);
        if (it == m_images.end()) return true;
        auto img = system::load_image(p, &err);
        if (!img) return false;
        *it->second = std::move(*img);
        return true;
    });
}

void AssetManager::watch_text(const std::string& key) {
    watch_path(key, [this, key](const std::filesystem::path& p, std::string& err) {
        const auto it = m_texts.find(key);
        if (it == m_texts.end()) return true;
        auto t = system::paths::read_text(p);
        if (!t) { err = "cannot read " + key; return false; }
        *it->second = std::move(*t);
        return true;
    });
}

void AssetManager::enable_hot_reload(system::HotReloader& reloader) {
    std::lock_guard<std::recursive_mutex> g(m_lock);
    m_reloader = &reloader;
    for (const auto& kv : m_images) watch_image(kv.first);
    for (const auto& kv : m_texts) watch_text(kv.first);
}

void AssetManager::disable_hot_reload() {
    std::lock_guard<std::recursive_mutex> g(m_lock);
    if (m_reloader) for (const auto& kv : m_watches) m_reloader->unwatch(kv.second);
    m_watches.clear();
    m_reloader = nullptr;
}

auto AssetManager::image(const std::string& name) -> ImagePtr {
    const std::filesystem::path path = locate(name);
    const std::string key = key_of(path);
    {
        std::lock_guard<std::recursive_mutex> g(m_lock);
        const auto it = m_images.find(key);
        if (it != m_images.end()) return it->second;
    }
    std::string err;
    auto img = system::load_image(path, &err);
    std::lock_guard<std::recursive_mutex> g(m_lock);
    if (!img) { m_error = err; return nullptr; }
    auto it = m_images.find(key);
    if (it != m_images.end()) return it->second;
    ImagePtr ptr = std::make_shared<images::BitmapImage>(std::move(*img));
    m_images[key] = ptr;
    watch_image(key);
    return ptr;
}

auto AssetManager::texture(const std::string& name, SampleFilter filter, WrapMode wrap) -> Texture {
    ImagePtr img = image(name);
    if (!img) return Texture();
    return Texture(std::shared_ptr<const images::BitmapImage>(img), filter, wrap);
}

auto AssetManager::text(const std::string& name) -> TextPtr {
    const std::filesystem::path path = locate(name);
    const std::string key = key_of(path);
    std::lock_guard<std::recursive_mutex> g(m_lock);
    const auto it = m_texts.find(key);
    if (it != m_texts.end()) return it->second;
    auto t = system::paths::read_text(path);
    if (!t) { m_error = "cannot read " + name; return nullptr; }
    TextPtr ptr = std::make_shared<std::string>(std::move(*t));
    m_texts[key] = ptr;
    watch_text(key);
    return ptr;
}

auto AssetManager::json_file(const std::string& name) -> std::shared_ptr<json::Value> {
    return load<json::Value>(name, [](const std::filesystem::path& p, std::string& err) -> std::shared_ptr<json::Value> {
        auto t = system::paths::read_text(p);
        if (!t) { err = "cannot read file"; return nullptr; }
        auto v = std::make_shared<json::Value>();
        if (!json::parse(*t, *v, &err)) return nullptr;
        return v;
    });
}

auto AssetManager::atlas(const std::string& name, SampleFilter filter) -> std::shared_ptr<TextureAtlas> {
    return load<TextureAtlas>(name, [filter](const std::filesystem::path& p, std::string& err) -> std::shared_ptr<TextureAtlas> {
        auto a = std::make_shared<TextureAtlas>();
        if (!TextureAtlas::load(p, *a, &err, filter)) return nullptr;
        return a;
    });
}

std::vector<std::string> AssetManager::preload_images(const std::vector<std::string>& names) {
    std::vector<std::string> failed;
    std::vector<std::filesystem::path> paths;
    for (const std::string& n : names) paths.push_back(locate(n));
    std::vector<std::optional<images::BitmapImage>> results(paths.size());
    std::vector<std::string> errors(paths.size());
    system::JobSystem::instance().parallel_for(std::size_t(0), paths.size(), std::size_t(1), [&](std::size_t i) {
        results[i] = system::load_image(paths[i], &errors[i]);
    });
    std::lock_guard<std::recursive_mutex> g(m_lock);
    for (std::size_t i = 0; i < paths.size(); ++i) {
        if (!results[i]) { failed.push_back(names[i]); m_error = errors[i]; continue; }
        const std::string key = key_of(paths[i]);
        if (m_images.count(key)) continue;
        m_images[key] = std::make_shared<images::BitmapImage>(std::move(*results[i]));
        watch_image(key);
    }
    return failed;
}

bool AssetManager::loaded(const std::string& name) const {
    const std::string key = key_of(locate(name));
    std::lock_guard<std::recursive_mutex> g(m_lock);
    return m_images.count(key) || m_texts.count(key);
}

std::size_t AssetManager::collect() {
    std::lock_guard<std::recursive_mutex> g(m_lock);
    std::size_t freed = 0;
    auto drop_watch = [&](const std::string& key) {
        auto w = m_watches.find(key);
        if (w != m_watches.end()) { if (m_reloader) m_reloader->unwatch(w->second); m_watches.erase(w); }
    };
    for (auto it = m_images.begin(); it != m_images.end();) {
        if (it->second.use_count() == 1) { drop_watch(it->first); it = m_images.erase(it); ++freed; } else ++it;
    }
    for (auto it = m_texts.begin(); it != m_texts.end();) {
        if (it->second.use_count() == 1) { drop_watch(it->first); it = m_texts.erase(it); ++freed; } else ++it;
    }
    for (auto it = m_custom.begin(); it != m_custom.end();) {
        if (it->second.value.use_count() == 1) { if (m_reloader && it->second.watch) m_reloader->unwatch(it->second.watch); it = m_custom.erase(it); ++freed; } else ++it;
    }
    return freed;
}

bool AssetManager::unload(const std::string& name) {
    const std::string key = key_of(locate(name));
    std::lock_guard<std::recursive_mutex> g(m_lock);
    auto w = m_watches.find(key);
    if (w != m_watches.end()) { if (m_reloader) m_reloader->unwatch(w->second); m_watches.erase(w); }
    return m_images.erase(key) + m_texts.erase(key) > 0;
}

void AssetManager::clear() {
    disable_hot_reload();
    std::lock_guard<std::recursive_mutex> g(m_lock);
    m_images.clear();
    m_texts.clear();
    m_custom.clear();
}

auto AssetManager::stats() const -> AssetStats {
    std::lock_guard<std::recursive_mutex> g(m_lock);
    AssetStats s;
    s.images = m_images.size();
    s.texts = m_texts.size();
    s.custom = m_custom.size();
    s.reloads = m_reloads;
    for (const auto& kv : m_images) s.image_bytes += static_cast<std::size_t>(kv.second->width()) * kv.second->height() * 4;
    return s;
}

} // namespace graphics
} // namespace fizmo
