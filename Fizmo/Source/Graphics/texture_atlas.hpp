#ifndef FIZMO_TEXTURE_ATLAS_HPP
#define FIZMO_TEXTURE_ATLAS_HPP

#include "texture.hpp"
#include "sprite.hpp"
#include "../Images/Bitmap/image.hpp"
#include "../Util Hpp/json.hpp"
#include "../System/paths.hpp"
#include "../System/hot_reload.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {

class MaxRectsPacker {
public:
    struct Rect { int x = 0, y = 0, w = 0, h = 0; };

private:
    int               m_w = 0, m_h = 0;
    std::vector<Rect> m_free;
    std::vector<Rect> m_used;

    static bool contains(const Rect& a, const Rect& b) noexcept { return b.x >= a.x && b.y >= a.y && b.x + b.w <= a.x + a.w && b.y + b.h <= a.y + a.h; }

    void split(const Rect& used);

public:
    MaxRectsPacker() = default;
    MaxRectsPacker(int w, int h) { reset(w, h); }

    void reset(int w, int h) {
        m_w = w;
        m_h = h;
        m_free.assign(1, { 0, 0, w, h });
        m_used.clear();
    }

    bool insert(int w, int h, Rect& out);

    double occupancy() const noexcept;

    int width() const noexcept { return m_w; }
    int height() const noexcept { return m_h; }
};

struct AtlasRegion {
    std::uint32_t page = 0;
    TextureRect   rect;
};

class TextureAtlas {
private:
    std::vector<Texture>                         m_pages;
    std::unordered_map<std::string, AtlasRegion> m_regions;

    friend class AtlasBuilder;

public:
    std::size_t page_count() const noexcept { return m_pages.size(); }
    const Texture& page(std::size_t i) const noexcept { static const Texture none; return i < m_pages.size() ? m_pages[i] : none; }
    const std::unordered_map<std::string, AtlasRegion>& regions() const noexcept { return m_regions; }
    bool has(const std::string& name) const noexcept { return m_regions.count(name) != 0; }

    const AtlasRegion* find(const std::string& name) const noexcept {
        const auto it = m_regions.find(name);
        return it == m_regions.end() ? nullptr : &it->second;
    }

    TextureRect rect(const std::string& name) const noexcept {
        const AtlasRegion* r = find(name);
        return r ? r->rect : TextureRect();
    }

    const Texture& texture_of(const std::string& name) const noexcept {
        const AtlasRegion* r = find(name);
        return page(r ? r->page : m_pages.size());
    }

    bool apply(const std::string& name, Sprite& sprite) const;

    json::Value manifest(const std::string& page_prefix) const;

    bool save(const std::filesystem::path& json_path) const;

    static bool load(const std::filesystem::path& json_path, TextureAtlas& out, std::string* error = nullptr, SampleFilter filter = SampleFilter::Nearest);
};

class AtlasBuilder {
private:
    struct Item {
        std::string                                name;
        std::shared_ptr<const images::BitmapImage> image;
        TextureRect                                source;
    };

    std::vector<Item> m_items;
    unsigned int      m_max_size = 2048;
    unsigned int      m_padding = 2;
    unsigned int      m_extrude = 1;
    bool              m_power_of_two = true;
    bool              m_trim_pages = true;
    SampleFilter      m_filter = SampleFilter::Nearest;
    std::string       m_error;

    static unsigned int next_pow2(unsigned int v) noexcept {
        unsigned int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

public:
    AtlasBuilder& set_max_size(unsigned int s) noexcept { m_max_size = std::max(16u, s); return *this; }
    AtlasBuilder& set_padding(unsigned int p) noexcept { m_padding = p; return *this; }
    AtlasBuilder& set_extrude(unsigned int e) noexcept { m_extrude = e; return *this; }
    AtlasBuilder& set_power_of_two(bool p) noexcept { m_power_of_two = p; return *this; }
    AtlasBuilder& set_trim_pages(bool t) noexcept { m_trim_pages = t; return *this; }
    AtlasBuilder& set_filter(SampleFilter f) noexcept { m_filter = f; return *this; }
    const std::string& error() const noexcept { return m_error; }
    std::size_t size() const noexcept { return m_items.size(); }

    AtlasBuilder& add(const std::string& name, const images::BitmapImage& image);

    AtlasBuilder& add(const std::string& name, std::shared_ptr<const images::BitmapImage> image, const TextureRect& source = TextureRect());

    AtlasBuilder& add_file(const std::filesystem::path& path, const std::string& name = std::string());

    AtlasBuilder& add_directory(const std::filesystem::path& dir, bool recursive = false);

    bool build(TextureAtlas& out);
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TEXTURE_ATLAS_HPP
