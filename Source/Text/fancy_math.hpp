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

    MathTheme& set_role_color(mathtext::GlyphRole role, const graphics::Color& c) noexcept;

    graphics::Color color_for(mathtext::GlyphRole role) const noexcept;

    const std::string& family_for(mathtext::GlyphRole role) const noexcept;
};

struct MathRenderOptions {
    mathtext::LayoutOptions layout;
    mathtext::ParseOptions  parse;
    MathTheme               theme;
    bool                    brace_syntax = false;
};

RichText math_run(std::string text, double size, mathtext::GlyphRole role, const MathTheme& theme, bool synthetic_italic);

class FancyMathMetrics : public mathtext::FontMetrics {
public:
    FancyMathMetrics(TextRasterizer& raster, const MathTheme& theme, bool synthetic_italic) noexcept : m_raster(raster), m_theme(theme), m_italic(synthetic_italic) {}

    mathtext::TextExtent measure(std::string_view utf8, double size, mathtext::GlyphRole role) override;

private:
    TextRasterizer&                                       m_raster;
    const MathTheme&                                      m_theme;
    bool                                                  m_italic;
    mathtext::ApproximateMetrics                          m_fallback;
    std::unordered_map<std::string, mathtext::TextExtent> m_cache;
};

FancyText to_fancy_text(const mathtext::MathBox& box, const MathTheme& theme, bool synthetic_italic = false);

FancyText layout_math(const mathtext::Document& doc, mathtext::FontMetrics& metrics, const MathRenderOptions& options = MathRenderOptions());

FancyText layout_math(const mathtext::Document& doc, TextRasterizer& raster, const MathRenderOptions& options = MathRenderOptions());

mathtext::Document parse_math_source(std::string_view source, const MathRenderOptions& options);

inline FancyText make_fancy_math(std::string_view source, TextRasterizer& raster, const MathRenderOptions& options = MathRenderOptions()) { return layout_math(parse_math_source(source, options), raster, options); }

inline FancyText make_fancy_math(std::string_view source, const MathRenderOptions& options = MathRenderOptions()) { return make_fancy_math(source, shared_text_rasterizer(), options); }

inline images::BitmapImage math_image(std::string_view source, const MathRenderOptions& options = MathRenderOptions(), float padding = 6.0f, const graphics::Color& background = graphics::Color(255, 255, 255, 0)) {
    return fancy_text_image(make_fancy_math(source, options), padding, background);
}

namespace detail {

inline void fingerprint_bytes(std::string& out, const void* data, std::size_t size) { out.append(static_cast<const char*>(data), size); }

template <typename T>
inline void fingerprint_value(std::string& out, const T& v) { fingerprint_bytes(out, &v, sizeof(v)); }

void fingerprint_color(std::string& out, const graphics::Color& c);

inline void fingerprint_string(std::string& out, const std::string& s) {
    fingerprint_value(out, s.size());
    out += s;
}

std::string math_cache_key(std::string_view source, const MathRenderOptions& o, bool texture);

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

    void trim();

    MathCacheEntry& get(std::string_view source, const MathRenderOptions& options, bool texture);
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



inline void Renderer::draw_math(std::string_view source, float x, float y, float font_size, const graphics::Color& color, text::FancyAnchor anchor) {
    draw_math(source, x, y, text::detail::simple_math_options(font_size, color), anchor);
}



inline text::FancyText Renderer::math_text(std::string_view source, float font_size, const graphics::Color& color) {
    return math_text(source, text::detail::simple_math_options(font_size, color));
}



} // namespace windows
} // namespace fizmo

#endif // FIZMO_FANCY_MATH_HPP
