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

    void split(const Rect& used) {
        std::vector<Rect> next;
        next.reserve(m_free.size() + 4);

        for (const Rect& f : m_free) {
            if (used.x >= f.x + f.w || used.x + used.w <= f.x || used.y >= f.y + f.h || used.y + used.h <= f.y) { next.push_back(f); continue; }
            if (used.x > f.x) next.push_back({ f.x, f.y, used.x - f.x, f.h });
            if (used.x + used.w < f.x + f.w) next.push_back({ used.x + used.w, f.y, f.x + f.w - (used.x + used.w), f.h });
            if (used.y > f.y) next.push_back({ f.x, f.y, f.w, used.y - f.y });
            if (used.y + used.h < f.y + f.h) next.push_back({ f.x, used.y + used.h, f.w, f.y + f.h - (used.y + used.h) });
        }

        m_free.clear();
        for (std::size_t i = 0; i < next.size(); ++i) {
            bool redundant = false;
            for (std::size_t j = 0; j < next.size() && !redundant; ++j) {
                if (i == j) continue;
                if (contains(next[j], next[i]) && !(contains(next[i], next[j]) && j > i)) redundant = true;
            }
            if (!redundant && next[i].w > 0 && next[i].h > 0) m_free.push_back(next[i]);
        }
    }

public:
    MaxRectsPacker() = default;
    MaxRectsPacker(int w, int h) { reset(w, h); }

    void reset(int w, int h) {
        m_w = w;
        m_h = h;
        m_free.assign(1, { 0, 0, w, h });
        m_used.clear();
    }

    bool insert(int w, int h, Rect& out) {
        if (w <= 0 || h <= 0) return false;
        int best_short = std::numeric_limits<int>::max(), best_long = std::numeric_limits<int>::max();
        bool found = false;

        for (const Rect& f : m_free) {
            if (f.w < w || f.h < h) continue;
            const int lw = f.w - w, lh = f.h - h;
            const int s = std::min(lw, lh), l = std::max(lw, lh);
            if (s < best_short || (s == best_short && l < best_long)) {
                best_short = s;
                best_long = l;
                out = { f.x, f.y, w, h };
                found = true;
            }
        }

        if (!found) return false;
        split(out);
        m_used.push_back(out);
        return true;
    }

    double occupancy() const noexcept {
        if (m_w <= 0 || m_h <= 0) return 0.0;
        double used = 0.0;
        for (const Rect& r : m_used) used += static_cast<double>(r.w) * r.h;
        return used / (static_cast<double>(m_w) * m_h);
    }

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

    bool apply(const std::string& name, Sprite& sprite) const {
        const AtlasRegion* r = find(name);
        if (!r || r->page >= m_pages.size()) return false;
        sprite.set_texture(m_pages[r->page], true);
        sprite.set_source_rect(r->rect);
        return true;
    }

    json::Value manifest(const std::string& page_prefix) const {
        json::Value doc = json::Value::object();
        json::Value pages = json::Value::array();
        for (std::size_t i = 0; i < m_pages.size(); ++i) pages.push_back(page_prefix + std::to_string(i) + ".png");
        doc.set("pages", pages);
        json::Value regions = json::Value::object();
        std::vector<std::string> names;
        for (const auto& kv : m_regions) names.push_back(kv.first);
        std::sort(names.begin(), names.end());
        for (const std::string& n : names) {
            const AtlasRegion& r = m_regions.at(n);
            json::Value v = json::Value::object();
            v.set("page", r.page);
            v.set("x", r.rect.x);
            v.set("y", r.rect.y);
            v.set("w", r.rect.w);
            v.set("h", r.rect.h);
            regions.set(n, v);
        }
        doc.set("regions", regions);
        return doc;
    }

    bool save(const std::filesystem::path& json_path) const {
        const std::string stem = json_path.stem().string();
        const std::filesystem::path dir = json_path.parent_path();
        for (std::size_t i = 0; i < m_pages.size(); ++i) {
            if (!m_pages[i].image()) return false;
            if (!system::save_image_png(*m_pages[i].image(), dir / (stem + "_" + std::to_string(i) + ".png"))) return false;
        }
        return system::paths::write_file(json_path, manifest(stem + "_").dump(2));
    }

    static bool load(const std::filesystem::path& json_path, TextureAtlas& out, std::string* error = nullptr, SampleFilter filter = SampleFilter::Nearest) {
        auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
        const auto text = system::paths::read_text(json_path);
        if (!text) return fail("cannot read " + system::paths::to_utf8(json_path));
        json::Value doc;
        std::string err;
        if (!json::parse(*text, doc, &err)) return fail(err);
        TextureAtlas atlas;
        for (const json::Value& p : doc["pages"].items()) {
            auto img = system::load_image(json_path.parent_path() / system::paths::from_utf8(p.as_string()), &err);
            if (!img) return fail(err);
            atlas.m_pages.emplace_back(std::move(*img), filter);
        }
        for (const auto& m : doc["regions"].members()) {
            const json::Value& v = m.second;
            atlas.m_regions[m.first] = { v["page"].as_uint(), TextureRect(static_cast<int>(v["x"].as_int()), static_cast<int>(v["y"].as_int()), v["w"].as_uint(), v["h"].as_uint()) };
        }
        out = std::move(atlas);
        return true;
    }
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

    AtlasBuilder& add(const std::string& name, const images::BitmapImage& image) {
        m_items.push_back({ name, std::make_shared<images::BitmapImage>(image), TextureRect(0, 0, image.width(), image.height()) });
        return *this;
    }

    AtlasBuilder& add(const std::string& name, std::shared_ptr<const images::BitmapImage> image, const TextureRect& source = TextureRect()) {
        if (!image) return *this;
        const TextureRect src = source.is_empty() ? TextureRect(0, 0, image->width(), image->height()) : source;
        m_items.push_back({ name, std::move(image), src });
        return *this;
    }

    AtlasBuilder& add_file(const std::filesystem::path& path, const std::string& name = std::string()) {
        std::string err;
        auto img = system::load_image(path, &err);
        if (!img) { m_error = err; return *this; }
        return add(name.empty() ? path.stem().string() : name, std::make_shared<images::BitmapImage>(std::move(*img)));
    }

    AtlasBuilder& add_directory(const std::filesystem::path& dir, bool recursive = false) {
        std::error_code ec;
        auto visit = [&](const std::filesystem::directory_entry& e) {
            if (!e.is_regular_file()) return;
            const std::string ext = e.path().extension().string();
            if (ext != ".png" && ext != ".bmp" && ext != ".jpg" && ext != ".jpeg") return;
            std::filesystem::path rel = std::filesystem::relative(e.path(), dir, ec);
            rel.replace_extension();
            add_file(e.path(), rel.generic_string());
        };
        if (recursive) { for (const auto& e : std::filesystem::recursive_directory_iterator(dir, ec)) visit(e); }
        else { for (const auto& e : std::filesystem::directory_iterator(dir, ec)) visit(e); }
        return *this;
    }

    bool build(TextureAtlas& out) {
        m_error.clear();
        const unsigned int border = m_padding + m_extrude;
        std::vector<std::size_t> order(m_items.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const auto& sa = m_items[a].source; const auto& sb = m_items[b].source;
            return std::max(sa.w, sa.h) > std::max(sb.w, sb.h) || (std::max(sa.w, sa.h) == std::max(sb.w, sb.h) && sa.w * sa.h > sb.w * sb.h);
        });

        struct Placed { std::size_t item; std::uint32_t page; MaxRectsPacker::Rect at; };
        std::vector<Placed> placed;
        std::vector<MaxRectsPacker> packers;
        std::vector<std::pair<int, int>> extents;

        for (std::size_t idx : order) {
            const Item& it = m_items[idx];
            const int w = static_cast<int>(it.source.w + 2 * border), h = static_cast<int>(it.source.h + 2 * border);
            if (w > static_cast<int>(m_max_size) || h > static_cast<int>(m_max_size)) { m_error = "image " + it.name + " is larger than the atlas page"; return false; }
            MaxRectsPacker::Rect r;
            std::uint32_t page = 0;
            for (; page < packers.size(); ++page) if (packers[page].insert(w, h, r)) break;
            if (page == packers.size()) {
                packers.emplace_back(static_cast<int>(m_max_size), static_cast<int>(m_max_size));
                extents.emplace_back(0, 0);
                packers.back().insert(w, h, r);
            }
            extents[page].first = std::max(extents[page].first, r.x + r.w);
            extents[page].second = std::max(extents[page].second, r.y + r.h);
            placed.push_back({ idx, page, r });
        }

        TextureAtlas atlas;
        std::vector<images::BitmapImage> pages;

        for (std::size_t p = 0; p < packers.size(); ++p) {
            unsigned int pw = m_trim_pages ? static_cast<unsigned int>(extents[p].first) : m_max_size;
            unsigned int ph = m_trim_pages ? static_cast<unsigned int>(extents[p].second) : m_max_size;
            if (m_power_of_two) { pw = std::min(m_max_size, next_pow2(pw)); ph = std::min(m_max_size, next_pow2(ph)); }
            pages.emplace_back(std::max(1u, pw), std::max(1u, ph));
            pages.back().change_background(Color(0, 0, 0, 0));
        }

        for (const Placed& pl : placed) {
            const Item& it = m_items[pl.item];
            images::BitmapImage& dst = pages[pl.page];
            const int ox = pl.at.x + static_cast<int>(m_padding), oy = pl.at.y + static_cast<int>(m_padding);
            const int iw = static_cast<int>(it.source.w + 2 * m_extrude), ih = static_cast<int>(it.source.h + 2 * m_extrude);

            for (int y = 0; y < ih; ++y) {
                for (int x = 0; x < iw; ++x) {
                    const int sx = it.source.x + std::max(0, std::min(static_cast<int>(it.source.w) - 1, x - static_cast<int>(m_extrude)));
                    const int sy = it.source.y + std::max(0, std::min(static_cast<int>(it.source.h) - 1, y - static_cast<int>(m_extrude)));
                    dst.set_pixel(static_cast<unsigned int>(ox + x), static_cast<unsigned int>(oy + y), it.image->get_pixel(static_cast<unsigned int>(sx), static_cast<unsigned int>(sy)));
                }
            }

            atlas.m_regions[it.name] = { pl.page, TextureRect(ox + static_cast<int>(m_extrude), oy + static_cast<int>(m_extrude), it.source.w, it.source.h) };
        }

        for (images::BitmapImage& img : pages) atlas.m_pages.emplace_back(std::move(img), m_filter);
        out = std::move(atlas);
        return true;
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TEXTURE_ATLAS_HPP
