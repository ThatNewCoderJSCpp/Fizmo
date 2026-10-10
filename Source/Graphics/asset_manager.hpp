#ifndef FIZMO_ASSET_MANAGER_HPP
#define FIZMO_ASSET_MANAGER_HPP

#include "texture.hpp"
#include "texture_atlas.hpp"
#include "../Util Hpp/json.hpp"
#include "../System/paths.hpp"
#include "../System/hot_reload.hpp"
#include "../System/jobs.hpp"
#include <optional>
#include <type_traits>
#include <typeinfo>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {

struct AssetStats {
    std::size_t images = 0;
    std::size_t texts = 0;
    std::size_t custom = 0;
    std::size_t image_bytes = 0;
    std::size_t reloads = 0;
};

class AssetManager {
public:
    using ImagePtr = std::shared_ptr<images::BitmapImage>;
    using TextPtr  = std::shared_ptr<std::string>;
    template <typename T>
    using Loader = std::function<std::shared_ptr<T>(const std::filesystem::path&, std::string&)>;

private:
    struct CustomEntry {
        std::shared_ptr<void>                                     value;
        std::function<bool(std::shared_ptr<void>&, std::string&)> reload;
        system::HotReloader::WatchId                              watch = 0;
    };

    system::paths::AssetPaths                                       m_paths;
    mutable std::recursive_mutex                                    m_lock;
    std::unordered_map<std::string, ImagePtr>                       m_images;
    std::unordered_map<std::string, TextPtr>                        m_texts;
    std::unordered_map<std::string, CustomEntry>                    m_custom;
    std::unordered_map<std::string, system::HotReloader::WatchId>   m_watches;
    system::HotReloader*                                            m_reloader = nullptr;
    std::string                                                     m_error;
    std::size_t                                                     m_reloads = 0;

    std::filesystem::path locate(const std::string& name) const {
        auto r = m_paths.resolve(name);
        return r ? *r : system::paths::from_utf8(name);
    }

    static std::string key_of(const std::filesystem::path& p);

    void watch_path(const std::string& key, std::function<bool(const std::filesystem::path&, std::string&)> fn);

    void watch_image(const std::string& key);

    void watch_text(const std::string& key);

public:
    AssetManager() { m_paths.add_defaults(); }
    explicit AssetManager(const system::paths::AssetPaths& paths) : m_paths(paths) {}

    system::paths::AssetPaths& paths() noexcept { return m_paths; }
    const std::string& last_error() const noexcept { return m_error; }

    void enable_hot_reload(system::HotReloader& reloader);

    void disable_hot_reload();

    ImagePtr image(const std::string& name);

    Texture texture(const std::string& name, SampleFilter filter = SampleFilter::Nearest, WrapMode wrap = WrapMode::Clamp);

    TextPtr text(const std::string& name);

    std::shared_ptr<json::Value> json_file(const std::string& name);

    std::shared_ptr<TextureAtlas> atlas(const std::string& name, SampleFilter filter = SampleFilter::Nearest);

    template <typename T>
    std::shared_ptr<T> load(const std::string& name, Loader<T> loader) {
        const std::filesystem::path path = locate(name);
        const std::string key = std::string(typeid(T).name()) + "|" + key_of(path);
        std::lock_guard<std::recursive_mutex> g(m_lock);
        const auto it = m_custom.find(key);
        if (it != m_custom.end()) return std::static_pointer_cast<T>(it->second.value);
        std::string err;
        std::shared_ptr<T> value = loader(path, err);
        if (!value) { m_error = err.empty() ? "cannot load " + name : err; return nullptr; }
        CustomEntry e;
        e.value = value;
        e.reload = [loader, path](std::shared_ptr<void>& slot, std::string& error) {
            std::shared_ptr<T> fresh = loader(path, error);
            if (!fresh) return false;
            if constexpr (std::is_move_assignable<T>::value) *std::static_pointer_cast<T>(slot) = std::move(*fresh);
            else slot = fresh;
            return true;
        };
        if (m_reloader) {
            e.watch = m_reloader->watch(path, system::HotReloader::Reload([this, key](const std::filesystem::path&, std::string& error) {
                std::lock_guard<std::recursive_mutex> g2(m_lock);
                auto f = m_custom.find(key);
                if (f == m_custom.end()) return true;
                const bool ok = f->second.reload(f->second.value, error);
                if (ok) ++m_reloads;
                return ok;
            }));
        }
        m_custom[key] = std::move(e);
        return value;
    }

    std::vector<std::string> preload_images(const std::vector<std::string>& names);

    bool loaded(const std::string& name) const;

    std::size_t collect();

    bool unload(const std::string& name);

    void clear();

    AssetStats stats() const;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_ASSET_MANAGER_HPP
