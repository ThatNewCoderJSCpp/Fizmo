#include "fizmo_library.hpp"
#include "text.hpp"
#include "fancy_text_render.hpp"

#include <cmath>
#include <string>
#include <unordered_map>

namespace fizmo {
namespace text {
namespace detail {

struct TextLayoutEntry {
    FancyText     layout;
    FancyTexture  texture;
    bool          has_texture = false;
    std::uint64_t used = 0;
};

struct TextLayoutCache {
    std::unordered_map<std::string, TextLayoutEntry> entries;
    std::uint64_t                                    tick = 0;
    std::size_t                                      capacity = 256;

    void trim() {
        while (entries.size() > capacity && !entries.empty()) {
            auto oldest = entries.begin();
            for (auto it = entries.begin(); it != entries.end(); ++it)
                if (it->second.used < oldest->second.used) oldest = it;
            entries.erase(oldest);
        }
    }

    TextLayoutEntry& get(const Text& t, bool texture) {
        std::string key = t.fingerprint();
        auto it = entries.find(key);
        if (it == entries.end()) {
            TextLayoutEntry e;
            e.layout = t.layout(shared_text_rasterizer());
            it = entries.emplace(std::move(key), std::move(e)).first;
        }
        if (texture && !it->second.has_texture) {
            it->second.texture = fancy_text_texture(it->second.layout);
            it->second.has_texture = true;
        }
        it->second.used = ++tick;
        return it->second;
    }
};

} // namespace detail
} // namespace text

namespace windows {

namespace {

text::detail::TextLayoutCache& layout_cache(std::shared_ptr<void>& slot) {
    if (!slot) slot = std::make_shared<text::detail::TextLayoutCache>();
    return *static_cast<text::detail::TextLayoutCache*>(slot.get());
}

} // namespace

void Renderer::draw_text(float x, float y, const text::Text& t, text::FancyAnchor anchor) {
    if (!m_impl || t.empty()) return;
    try {
        if (t.is_simple()) {
            const text::RichText rt = t.to_rich_text();
            float top = y;
            if (anchor == text::FancyAnchor::Baseline) top -= static_cast<float>(m_impl->measure_rich_text(rt, 0).ascent);
            const int ix = static_cast<int>(std::lround(x)), iy = static_cast<int>(std::lround(top));
            draw_rich_text(ix, iy, 0, 0, rt);
            return;
        }

        text::detail::TextLayoutCache& cache = layout_cache(m_fancy_cache);
        const bool texture = !is_gpu();
        text::detail::TextLayoutEntry& e = cache.get(t, texture);
        if (e.has_texture && texture) {
            text::draw_fancy_texture(*this, e.texture, x, y, anchor);
            text::draw_fancy_images_native(*this, e.layout, x, y, anchor);
        } else {
            text::draw_fancy_text_native(*this, e.layout, x, y, anchor);
        }
        cache.trim();
    } catch (...) {
    }
}

void Renderer::draw_text(const text::Text& t) { draw_text(t.x(), t.y(), t, t.anchor()); }

void Renderer::draw_text(int x, int y, unsigned int w, unsigned int h, const text::Text& t) {
    (void)h;
    if (w == 0 || t.max_width() > 0.0f) {
        draw_text(static_cast<float>(x), static_cast<float>(y), t, text::FancyAnchor::TopLeft);
        return;
    }
    text::Text wrapped = t;
    wrapped.max_width(static_cast<float>(w));
    draw_text(static_cast<float>(x), static_cast<float>(y), wrapped, text::FancyAnchor::TopLeft);
}

text::FancyText Renderer::layout_text(const text::Text& t) {
    text::detail::TextLayoutCache& cache = layout_cache(m_fancy_cache);
    text::FancyText out = cache.get(t, false).layout;
    cache.trim();
    return out;
}

void Renderer::set_text_cache_capacity(std::size_t entries) {
    text::detail::TextLayoutCache& cache = layout_cache(m_fancy_cache);
    cache.capacity = entries == 0 ? 1 : entries;
    cache.trim();
}

text::TextMetrics Renderer::measure_text(const text::Text& t, unsigned int max_width) {
    if (t.empty()) return {};
    try {
        if (t.is_simple() && m_impl) return m_impl->measure_rich_text(t.to_rich_text(), max_width);
        text::Text copy = t;
        if (max_width > 0 && t.max_width() <= 0.0f) copy.max_width(static_cast<float>(max_width));
        const text::FancyText ft = layout_text(copy);
        text::TextMetrics m;
        m.width = static_cast<unsigned int>(std::ceil(ft.width()));
        m.height = static_cast<unsigned int>(std::ceil(ft.height()));
        m.ascent = static_cast<int>(std::ceil(ft.ascent()));
        m.descent = static_cast<int>(std::ceil(ft.descent()));
        return m;
    } catch (...) {
        return {};
    }
}

} // namespace windows
} // namespace fizmo
