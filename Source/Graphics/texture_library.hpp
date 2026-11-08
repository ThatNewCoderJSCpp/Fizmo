#ifndef FIZMO_TEXTURE_LIBRARY_HPP
#define FIZMO_TEXTURE_LIBRARY_HPP

#include "texture.hpp"
#include "../Lighting/texture.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {

enum class TextureRole : std::uint8_t {
    Color = 0,
    Emissive,
    Normal,
    Roughness,
    Metallic,
    Occlusion,
    Packed,
    Opacity,
    Data
};

ColorSpace default_color_space(TextureRole role) noexcept;

struct TextureSettings {
    ColorSpace   color_space = ColorSpace::SRGB;
    SampleFilter filter      = SampleFilter::Bilinear;
    WrapMode     wrap        = WrapMode::Repeat;
    bool         mipmaps     = true;
    bool         flip_y      = false;

    static TextureSettings color() noexcept { return {}; }
    static TextureSettings data() noexcept { TextureSettings s; s.color_space = ColorSpace::Linear; return s; }
    static TextureSettings for_role(TextureRole role) noexcept { TextureSettings s; s.color_space = default_color_space(role); return s; }
    static TextureSettings pixel_art() noexcept { TextureSettings s; s.filter = SampleFilter::Nearest; s.mipmaps = false; return s; }

    TextureSettings& with_wrap(WrapMode w) noexcept { wrap = w; return *this; }
    TextureSettings& with_filter(SampleFilter f) noexcept { filter = f; return *this; }
    TextureSettings& with_mipmaps(bool on) noexcept { mipmaps = on; return *this; }
    TextureSettings& with_flip_y(bool on) noexcept { flip_y = on; return *this; }
    TextureSettings& with_color_space(ColorSpace c) noexcept { color_space = c; return *this; }

    std::uint32_t key() const noexcept;
    bool operator==(const TextureSettings& o) const noexcept { return key() == o.key(); }
    bool operator!=(const TextureSettings& o) const noexcept { return key() != o.key(); }
};

lighting::ColorSpace to_lighting(ColorSpace space) noexcept;
lighting::TextureFilter to_lighting(SampleFilter filter) noexcept;
lighting::TextureWrap to_lighting(WrapMode wrap) noexcept;
lighting::UvTransform to_lighting(const UvTransform& t) noexcept;
std::shared_ptr<lighting::Texture> to_lighting_texture(const Texture& texture);
lighting::TextureMap to_lighting_map(const std::shared_ptr<const lighting::Texture>& texture, const Texture& settings_from, int channel = 0);

class TextureLibrary {
public:
    struct Stats {
        std::size_t entries = 0;
        std::size_t hits    = 0;
        std::size_t misses  = 0;
        std::size_t reloads = 0;
        std::size_t failures = 0;
        std::uint64_t bytes = 0;
    };

    TextureLibrary() = default;
    TextureLibrary(const TextureLibrary&) = delete;
    TextureLibrary& operator=(const TextureLibrary&) = delete;

    std::shared_ptr<const Texture> load(const std::string& path, const TextureSettings& settings = TextureSettings::color());
    std::shared_ptr<const Texture> load(const std::string& path, TextureRole role);
    std::shared_ptr<const Texture> find(const std::string& path, const TextureSettings& settings = TextureSettings::color()) const;
    std::shared_ptr<const Texture> add(const std::string& name, fizmo::images::BitmapImage image, const TextureSettings& settings = TextureSettings::color());
    bool contains(const std::string& path, const TextureSettings& settings = TextureSettings::color()) const;

    std::shared_ptr<const lighting::Texture> load_lighting(const std::string& path, const TextureSettings& settings = TextureSettings::color());
    std::shared_ptr<const lighting::Texture> load_lighting(const std::string& path, TextureRole role);
    lighting::TextureMap lighting_map(const std::string& path, TextureRole role, int channel = 0);

    bool changed(const std::string& path) const;
    bool reload(const std::string& path);
    std::size_t reload_changed();

    void remove(const std::string& path);
    std::size_t prune();
    void clear();

    std::size_t size() const;
    Stats stats() const;
    std::string last_error() const;

    static std::string normalize_path(const std::string& path);
    static fizmo::images::BitmapImage pack_orm(const fizmo::images::BitmapImage* occlusion, const fizmo::images::BitmapImage* roughness, const fizmo::images::BitmapImage* metallic);
    static fizmo::images::BitmapImage flip_vertical(const fizmo::images::BitmapImage& image);
    static fizmo::images::BitmapImage solid(const Color& color, unsigned int size = 1);

private:
    struct Entry {
        std::string                         path;
        TextureSettings                     settings;
        std::shared_ptr<Texture>            texture;
        std::shared_ptr<lighting::Texture>  lighting;
        std::int64_t                        stamp = 0;
        std::uint64_t                       file_size = 0;
        bool                                file = true;
    };

    static std::string key_of(const std::string& normalized, const TextureSettings& settings);
    static bool file_stamp(const std::string& path, std::int64_t& stamp, std::uint64_t& size);
    bool read_image(const std::string& path, const TextureSettings& settings, fizmo::images::BitmapImage& out);
    Entry* entry_locked(const std::string& path, const TextureSettings& settings, bool count);
    bool refresh_locked(Entry& e);
    static void fill_lighting(Entry& e);

    mutable std::mutex                       m_mutex;
    std::unordered_map<std::string, Entry>   m_entries;
    mutable Stats                            m_stats;
    std::string                              m_error;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TEXTURE_LIBRARY_HPP
