#ifndef FIZMO_FANCY_TEXT_RENDER_HPP
#define FIZMO_FANCY_TEXT_RENDER_HPP

#include "fancy_text.hpp"
#include "fancy_text_fwd.hpp"
#include "../Windows/renderer.hpp"
#include "../Images/Bitmap/image.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace text {

struct TextInk {
    float advance = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_ascent = 0.0f;
    float line_descent = 0.0f;
    bool  has_ink = false;
};

namespace fancy_detail {

inline float clamp01(float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

 void blend(images::BitmapImage& img, int x, int y, const graphics::Color& c, float coverage);

 void blend_premultiplied(images::BitmapImage& img, int x, int y, const std::uint8_t* rgba, float opacity);

 void fill_rect(images::BitmapImage& img, float x, float y, float w, float h, const graphics::Color& c);

 float segment_distance(float px, float py, const FancyPoint& a, const FancyPoint& b) noexcept;

 void stroke_path(images::BitmapImage& img, const std::vector<FancyPoint>& pts, bool closed, float thickness, const graphics::Color& c);

 void fill_polygon(images::BitmapImage& img, const std::vector<FancyPoint>& pts, const graphics::Color& c);

} // namespace fancy_detail

class TextRasterizer {
public:
    TextRasterizer();
    ~TextRasterizer();
    TextRasterizer(const TextRasterizer&) = delete;
    TextRasterizer& operator=(const TextRasterizer&) = delete;

    bool ready() const noexcept;
    bool load_font_file(const std::string& utf8_path);
    void clear_cache();
    TextInk measure(const RichText& rt);
    void draw(images::BitmapImage& img, const RichText& rt, float x, float baseline);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

TextRasterizer& shared_text_rasterizer();

 void draw_fancy_text(images::BitmapImage& img, const FancyText& ft, float x, float y, TextRasterizer& raster, FancyAnchor anchor = FancyAnchor::TopLeft);

inline void draw_fancy_text(images::BitmapImage& img, const FancyText& ft, float x, float y, FancyAnchor anchor = FancyAnchor::TopLeft) { draw_fancy_text(img, ft, x, y, shared_text_rasterizer(), anchor); }

 images::BitmapImage fancy_text_image(const FancyText& ft, TextRasterizer& raster, float padding = 4.0f, const graphics::Color& background = graphics::Color(255, 255, 255, 0));

inline images::BitmapImage fancy_text_image(const FancyText& ft, float padding = 4.0f, const graphics::Color& background = graphics::Color(255, 255, 255, 0)) { return fancy_text_image(ft, shared_text_rasterizer(), padding, background); }

 void draw_fancy_text_native(windows::Renderer& r, const FancyText& ft, float x, float y, FancyAnchor anchor = FancyAnchor::TopLeft);

struct FancyTexture {
    graphics::Texture texture;
    float             padding = 0.0f;
    float             width = 0.0f;
    float             ascent = 0.0f;
    float             descent = 0.0f;

    bool valid() const noexcept { return texture.valid(); }
};

 FancyTexture fancy_text_texture(const FancyText& ft, TextRasterizer& raster, float padding = 2.0f);

inline FancyTexture fancy_text_texture(const FancyText& ft, float padding = 2.0f) { return fancy_text_texture(ft, shared_text_rasterizer(), padding); }

 void draw_fancy_texture(windows::Renderer& r, const FancyTexture& t, float x, float y, FancyAnchor anchor = FancyAnchor::TopLeft);

 void draw_fancy_text(windows::Renderer& r, const FancyText& ft, float x, float y, FancyAnchor anchor = FancyAnchor::TopLeft, FancyDrawMode mode = FancyDrawMode::Auto);

} // namespace text

namespace windows {

inline void Renderer::draw_fancy_text(const text::FancyText& ft, float x, float y, text::FancyAnchor anchor, text::FancyDrawMode mode) { text::draw_fancy_text(*this, ft, x, y, anchor, mode); }

inline void Renderer::draw_fancy_texture(const text::FancyTexture& texture, float x, float y, text::FancyAnchor anchor) { text::draw_fancy_texture(*this, texture, x, y, anchor); }

inline text::FancyTexture Renderer::make_fancy_texture(const text::FancyText& ft, float padding) const { return text::fancy_text_texture(ft, padding); }

} // namespace windows
} // namespace fizmo

#endif // FIZMO_FANCY_TEXT_RENDER_HPP
