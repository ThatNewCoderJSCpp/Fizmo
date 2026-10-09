#ifndef FIZMO_FANCY_MATH_HPP
#define FIZMO_FANCY_MATH_HPP

#include "fancy_text_render.hpp"
#include "../MathText/mathtext.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fizmo {
namespace text {

struct MathTheme {
    std::string     math_family = "Latin Modern Math, STIX Two Math, Cambria Math, DejaVu Math TeX Gyre";
    std::string     text_family = "Latin Modern Roman, Cambria, Times New Roman, DejaVu Serif";
    graphics::Color color = graphics::Color(20, 20, 20, 255);
    graphics::Color error_color = graphics::Color(211, 47, 47, 255);
    std::array<graphics::Color, 10> role_colors{};
    std::array<bool, 10>            role_color_set{};

    MathTheme& set_role_color(mathtext::GlyphRole role, const graphics::Color& c) noexcept {
        role_colors[static_cast<std::size_t>(role)] = c;
        role_color_set[static_cast<std::size_t>(role)] = true;
        return *this;
    }

    graphics::Color color_for(mathtext::GlyphRole role) const noexcept {
        const std::size_t i = static_cast<std::size_t>(role);
        if (i < role_color_set.size() && role_color_set[i]) return role_colors[i];
        return role == mathtext::GlyphRole::Error ? error_color : color;
    }

    const std::string& family_for(mathtext::GlyphRole role) const noexcept {
        return role == mathtext::GlyphRole::Text || role == mathtext::GlyphRole::Error ? text_family : math_family;
    }
};

struct MathRenderOptions {
    mathtext::LayoutOptions layout;
    mathtext::ParseOptions  parse;
    MathTheme               theme;
    bool                    brace_syntax = false;
};

inline RichText math_run(std::string text, double size, mathtext::GlyphRole role, const MathTheme& theme, bool synthetic_italic) {
    TextStyle style(size, theme.color_for(role));
    if (synthetic_italic && role == mathtext::GlyphRole::Variable) style.set_slant(FontSlant::Italic);
    RichText rt(style);
    rt.add(std::move(text), TextStyle{}, theme.family_for(role));
    return rt;
}

class FancyMathMetrics : public mathtext::FontMetrics {
public:
    FancyMathMetrics(TextRasterizer& raster, const MathTheme& theme, bool synthetic_italic) noexcept : m_raster(raster), m_theme(theme), m_italic(synthetic_italic) {}

    mathtext::TextExtent measure(std::string_view utf8, double size, mathtext::GlyphRole role) override {
        std::string key(utf8);
        key.push_back('\x1F');
        key += std::to_string(size);
        key.push_back(static_cast<char>('A' + static_cast<int>(role)));
        auto it = m_cache.find(key);
        if (it != m_cache.end()) return it->second;
        mathtext::TextExtent e;
        if (m_raster.ready()) {
            const TextInk ink = m_raster.measure(math_run(std::string(utf8), size, role, m_theme, m_italic));
            e.advance = ink.advance;
            e.ascent = ink.has_ink ? ink.ascent : 0.0;
            e.descent = ink.has_ink ? ink.descent : 0.0;
        } else {
            e = m_fallback.measure(utf8, size, role);
        }
        m_cache.emplace(std::move(key), e);
        return e;
    }

private:
    TextRasterizer&                                       m_raster;
    const MathTheme&                                      m_theme;
    bool                                                  m_italic;
    mathtext::ApproximateMetrics                          m_fallback;
    std::unordered_map<std::string, mathtext::TextExtent> m_cache;
};

inline FancyText to_fancy_text(const mathtext::MathBox& box, const MathTheme& theme, bool synthetic_italic = false) {
    FancyText out;
    for (const mathtext::LayoutText& t : box.texts) {
        out.add_run(static_cast<float>(t.x), static_cast<float>(t.baseline), math_run(t.text, t.size, t.role, theme, synthetic_italic), t.source);
    }
    for (const mathtext::LayoutRule& r : box.rules) {
        out.add_rect(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.width), static_cast<float>(r.height), theme.color_for(r.role), r.source);
    }
    for (const mathtext::LayoutPath& p : box.paths) {
        std::vector<FancyPoint> pts;
        pts.reserve(p.points.size());
        for (const mathtext::LayoutPoint& q : p.points) pts.push_back(FancyPoint{ static_cast<float>(q.x), static_cast<float>(q.y) });
        out.add_path(std::move(pts), static_cast<float>(p.thickness), theme.color_for(p.role), p.closed, p.filled, p.source);
    }
    out.set_box(static_cast<float>(box.width), static_cast<float>(box.ascent), static_cast<float>(box.descent));
    return out;
}

inline FancyText layout_math(const mathtext::Document& doc, mathtext::FontMetrics& metrics, const MathRenderOptions& options = MathRenderOptions()) {
    const mathtext::MathBox box = mathtext::layout_math(doc, metrics, options.layout);
    return to_fancy_text(box, options.theme, !options.layout.math_italic_alphabet);
}

inline FancyText layout_math(const mathtext::Document& doc, TextRasterizer& raster, const MathRenderOptions& options = MathRenderOptions()) {
    FancyMathMetrics metrics(raster, options.theme, !options.layout.math_italic_alphabet);
    return layout_math(doc, metrics, options);
}

inline mathtext::Document parse_math_source(std::string_view source, const MathRenderOptions& options) {
    return options.brace_syntax ? mathtext::parse(source, options.parse) : mathtext::parse_math(source, options.parse);
}

inline FancyText make_fancy_math(std::string_view source, TextRasterizer& raster, const MathRenderOptions& options = MathRenderOptions()) { return layout_math(parse_math_source(source, options), raster, options); }

inline FancyText make_fancy_math(std::string_view source, const MathRenderOptions& options = MathRenderOptions()) { return make_fancy_math(source, shared_text_rasterizer(), options); }

inline images::BitmapImage math_image(std::string_view source, const MathRenderOptions& options = MathRenderOptions(), float padding = 6.0f, const graphics::Color& background = graphics::Color(255, 255, 255, 0)) {
    return fancy_text_image(make_fancy_math(source, options), padding, background);
}

namespace detail {

inline void fingerprint_bytes(std::string& out, const void* data, std::size_t size) { out.append(static_cast<const char*>(data), size); }

template <typename T>
inline void fingerprint_value(std::string& out, const T& v) { fingerprint_bytes(out, &v, sizeof(v)); }

inline void fingerprint_color(std::string& out, const graphics::Color& c) {
    const std::uint8_t rgba[4] = { c.red(), c.green(), c.blue(), c.alpha() };
    fingerprint_bytes(out, rgba, 4);
}

inline void fingerprint_string(std::string& out, const std::string& s) {
    fingerprint_value(out, s.size());
    out += s;
}

inline std::string math_cache_key(std::string_view source, const MathRenderOptions& o, bool texture) {
    std::string k;
    k.reserve(source.size() + 256);
    const mathtext::LayoutOptions& l = o.layout;
    fingerprint_value(k, l.font_size);
    fingerprint_value(k, l.script_ratio);
    fingerprint_value(k, l.script_script_ratio);
    fingerprint_value(k, l.fraction_ratio);
    fingerprint_value(k, l.min_size_ratio);
    fingerprint_value(k, l.line_gap);
    const std::uint8_t flags[] = {
        static_cast<std::uint8_t>(l.display), static_cast<std::uint8_t>(l.numbers), static_cast<std::uint8_t>(l.math_italic_alphabet),
        static_cast<std::uint8_t>(l.italic_variables), static_cast<std::uint8_t>(l.italic_greek_lowercase), static_cast<std::uint8_t>(l.limits_above_below),
        static_cast<std::uint8_t>(l.integral_limits_above_below), static_cast<std::uint8_t>(l.center_lines), static_cast<std::uint8_t>(o.brace_syntax),
        static_cast<std::uint8_t>(o.parse.newline_separates), static_cast<std::uint8_t>(texture)
    };
    fingerprint_bytes(k, flags, sizeof(flags));
    fingerprint_value(k, o.parse.max_depth);
    fingerprint_string(k, o.theme.math_family);
    fingerprint_string(k, o.theme.text_family);
    fingerprint_color(k, o.theme.color);
    fingerprint_color(k, o.theme.error_color);
    for (std::size_t i = 0; i < o.theme.role_colors.size(); ++i) {
        k.push_back(o.theme.role_color_set[i] ? '\1' : '\0');
        if (o.theme.role_color_set[i]) fingerprint_color(k, o.theme.role_colors[i]);
    }
    k.append(source.data(), source.size());
    return k;
}

struct MathCacheEntry {
    FancyText     text;
    FancyTexture  texture;
    bool          has_texture = false;
    std::uint64_t used = 0;
};

struct MathCache {
    std::unordered_map<std::string, MathCacheEntry> entries;
    std::uint64_t                                   tick = 0;
    std::size_t                                     capacity = 128;

    void trim() {
        while (entries.size() > capacity && !entries.empty()) {
            auto oldest = entries.begin();
            for (auto it = entries.begin(); it != entries.end(); ++it) if (it->second.used < oldest->second.used) oldest = it;
            entries.erase(oldest);
        }
    }

    MathCacheEntry& get(std::string_view source, const MathRenderOptions& options, bool texture) {
        std::string key = math_cache_key(source, options, texture);
        auto it = entries.find(key);
        if (it == entries.end()) {
            MathCacheEntry e;
            e.text = make_fancy_math(source, options);
            if (texture) {
                e.texture = fancy_text_texture(e.text);
                e.has_texture = true;
            }
            it = entries.emplace(std::move(key), std::move(e)).first;
        }
        it->second.used = ++tick;
        return it->second;
    }
};

inline MathRenderOptions simple_math_options(float font_size, const graphics::Color& color) {
    MathRenderOptions o;
    o.layout.font_size = font_size;
    o.theme.color = color;
    return o;
}

} // namespace detail

} // namespace text

namespace windows {

inline void Renderer::draw_math(std::string_view source, float x, float y, const text::MathRenderOptions& options, text::FancyAnchor anchor) {
    if (!m_fancy_cache) m_fancy_cache = std::make_shared<text::detail::MathCache>();
    text::detail::MathCache& cache = *static_cast<text::detail::MathCache*>(m_fancy_cache.get());
    const bool texture = !is_gpu();
    const text::detail::MathCacheEntry& e = cache.get(source, options, texture);
    if (e.has_texture) text::draw_fancy_texture(*this, e.texture, x, y, anchor);
    else text::draw_fancy_text_native(*this, e.text, x, y, anchor);
    cache.trim();
}

inline void Renderer::draw_math(std::string_view source, float x, float y, float font_size, const graphics::Color& color, text::FancyAnchor anchor) {
    draw_math(source, x, y, text::detail::simple_math_options(font_size, color), anchor);
}

inline text::FancyText Renderer::math_text(std::string_view source, const text::MathRenderOptions& options) {
    if (!m_fancy_cache) m_fancy_cache = std::make_shared<text::detail::MathCache>();
    text::detail::MathCache& cache = *static_cast<text::detail::MathCache*>(m_fancy_cache.get());
    text::FancyText out = cache.get(source, options, false).text;
    cache.trim();
    return out;
}

inline text::FancyText Renderer::math_text(std::string_view source, float font_size, const graphics::Color& color) {
    return math_text(source, text::detail::simple_math_options(font_size, color));
}

inline void Renderer::set_math_cache_capacity(std::size_t entries) {
    if (!m_fancy_cache) m_fancy_cache = std::make_shared<text::detail::MathCache>();
    text::detail::MathCache& cache = *static_cast<text::detail::MathCache*>(m_fancy_cache.get());
    cache.capacity = entries == 0 ? 1 : entries;
    cache.trim();
}

} // namespace windows
} // namespace fizmo

#endif // FIZMO_FANCY_MATH_HPP
