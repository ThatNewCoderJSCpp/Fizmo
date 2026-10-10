#include "fizmo_library.hpp"
#include "../Windows/Renderer Impl/gpu_text.hpp"
#include "../Windows/Renderer Impl/gpu_common.hpp"

namespace fizmo {
namespace text {

struct TextRasterizer::Impl {
    const windows::detail::gfx::TextDrawList* layout(const RichText& rt) {
        if (!ready() || rt.empty()) return nullptr;
        std::uint64_t key = 0x51ED270B27u;
        for (const TextSpan& sp : rt.spans()) {
            key = windows::detail::gfx::hash_bytes(sp.text.data(), sp.text.size(), key);
            key = windows::detail::gfx::hash_bytes(sp.family.data(), sp.family.size(), key ^ 0x9E37u);
        }
        const double size = rt.base().size();
        key = windows::detail::gfx::hash_bytes(&size, sizeof(size), key);
        auto& bucket = m_layouts[key];
        for (const auto& entry : bucket) if (entry.first == rt) return &entry.second;
        if (m_cached > 8192) {
            m_layouts.clear();
            m_cached = 0;
            return layout(rt);
        }
        windows::detail::gfx::TextDrawList list;
        if (!windows::detail::gfx::build_text_draw_list(m_engine, rt, 0.0f, 0.0f, list)) return nullptr;
        bucket.emplace_back(rt, std::move(list));
        ++m_cached;
        return &bucket.back().second;
    }

    const windows::detail::gfx::GlyphImage& glyph(std::uint32_t face, std::uint32_t index) {
        const std::uint64_t key = (static_cast<std::uint64_t>(face) << 32) | index;
        auto it = m_glyphs.find(key);
        if (it != m_glyphs.end()) return it->second;
        windows::detail::gfx::GlyphImage img;
        if (!m_engine.rasterize(face, index, img)) img = windows::detail::gfx::GlyphImage{};
        return m_glyphs.emplace(key, std::move(img)).first->second;
    }

    TextInk measure(const RichText& rt) {
        TextInk ink;
        const windows::detail::gfx::TextDrawList* list = layout(rt);
        if (!list) return ink;
        ink.advance = list->width;
        ink.line_ascent = list->ascent;
        ink.line_descent = list->descent;
        float top = 1e30f, bottom = -1e30f;
        for (const windows::detail::gfx::TextCmd& c : list->cmds) {
            if (c.kind != windows::detail::gfx::TextCmd::Kind::Glyph) continue;
            const windows::detail::gfx::GlyphImage& g = glyph(c.face, c.glyph);
            if (g.width <= 0 || g.height <= 0) continue;
            const float gy = std::round(c.y) - static_cast<float>(g.top);
            top = std::min(top, gy + 1.0f);
            bottom = std::max(bottom, gy + static_cast<float>(g.height) - 1.0f);
        }
        if (top <= bottom) {
            ink.has_ink = true;
            ink.ascent = std::max(0.0f, list->first_baseline - top);
            ink.descent = std::max(0.0f, bottom - list->first_baseline);
        }
        return ink;
    }

    void draw(images::BitmapImage& img, const RichText& rt, float x, float baseline) {
        const windows::detail::gfx::TextDrawList* list = layout(rt);
        if (!list) return;
        const float ox = x;
        const float oy = baseline - list->first_baseline;
        for (const windows::detail::gfx::TextCmd& c : list->cmds) {
            switch (c.kind) {
                case windows::detail::gfx::TextCmd::Kind::Rect:
                    fancy_detail::fill_rect(img, ox + c.x, oy + c.y, c.w, c.h, c.color);
                    break;
                case windows::detail::gfx::TextCmd::Kind::Wave: {
                    std::vector<FancyPoint> pts;
                    for (const windows::detail::gfx::TextPoint& p : list->waves[c.wave]) pts.push_back(FancyPoint{ ox + p.x, oy + p.y });
                    fancy_detail::stroke_path(img, pts, false, c.w, c.color);
                    break;
                }
                case windows::detail::gfx::TextCmd::Kind::Glyph: {
                    if (c.color.alpha() == 0) break;
                    const windows::detail::gfx::GlyphImage& g = glyph(c.face, c.glyph);
                    if (g.width <= 0 || g.height <= 0) break;
                    const int gx = static_cast<int>(std::lround(ox + c.x)) + g.left;
                    const int gy = static_cast<int>(std::lround(oy + c.y)) - g.top;
                    for (int row = 0; row < g.height; ++row) {
                        for (int col = 0; col < g.width; ++col) {
                            if (g.color) {
                                fancy_detail::blend_premultiplied(img, gx + col, gy + row, &g.pixels[(static_cast<std::size_t>(row) * g.width + col) * 4], c.color.alpha() / 255.0f);
                            } else {
                                const std::uint8_t a = g.pixels[static_cast<std::size_t>(row) * g.width + col];
                                if (a) fancy_detail::blend(img, gx + col, gy + row, c.color, a / 255.0f);
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    bool ready() const noexcept { return m_ok && m_engine.ready(); }

    windows::detail::gfx::TextEngine                                                         m_engine;
    bool                                                                                     m_ok = false;
    std::unordered_map<std::uint64_t, std::vector<std::pair<RichText, windows::detail::gfx::TextDrawList>>> m_layouts;
    std::unordered_map<std::uint64_t, windows::detail::gfx::GlyphImage>                      m_glyphs;
    std::size_t                                                                              m_cached = 0;
};

TextRasterizer::TextRasterizer() : m_impl(std::make_unique<Impl>()) { m_impl->m_ok = m_impl->m_engine.initialize(); }

TextRasterizer::~TextRasterizer() = default;

bool TextRasterizer::ready() const noexcept { return m_impl->ready(); }

bool TextRasterizer::load_font_file(const std::string& utf8_path) {
    if (!ready()) return false;
    const bool ok = m_impl->m_engine.load_font_file(utf8_path);
    if (ok) {
        m_impl->m_layouts.clear();
        m_impl->m_glyphs.clear();
    }
    return ok;
}

void TextRasterizer::clear_cache() {
    m_impl->m_layouts.clear();
    m_impl->m_glyphs.clear();
    m_impl->m_cached = 0;
}

TextInk TextRasterizer::measure(const RichText& rt) { return m_impl->measure(rt); }

void TextRasterizer::draw(images::BitmapImage& img, const RichText& rt, float x, float baseline) { m_impl->draw(img, rt, x, baseline); }

TextRasterizer& shared_text_rasterizer() {
    static TextRasterizer r;
    return r;
}

} // namespace text
} // namespace fizmo

namespace fizmo {
namespace text {
namespace fancy_detail {

void blend(images::BitmapImage& img, int x, int y, const graphics::Color& c, float coverage) {
    if (x < 0 || y < 0 || x >= static_cast<int>(img.width()) || y >= static_cast<int>(img.height())) return;
    const float a = clamp01(coverage) * (c.alpha() / 255.0f);
    if (a <= 0.0f) return;
    const graphics::Color d = img.get_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y));
    const float da = d.alpha() / 255.0f;
    const float oa = a + da * (1.0f - a);
    if (oa <= 0.0f) return;
    auto mix = [&](std::uint8_t s, std::uint8_t t) { return static_cast<std::uint8_t>(std::lround((s * a + t * da * (1.0f - a)) / oa)); };
    img.set_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y), graphics::Color(mix(c.red(), d.red()), mix(c.green(), d.green()), mix(c.blue(), d.blue()), static_cast<std::uint8_t>(std::lround(oa * 255.0f))));
}

void blend_premultiplied(images::BitmapImage& img, int x, int y, const std::uint8_t* rgba, float opacity) {
    const float sa = rgba[3] / 255.0f * opacity;
    if (sa <= 0.0f) return;
    const float k = rgba[3] > 0 ? 255.0f / rgba[3] : 0.0f;
    const graphics::Color c(static_cast<std::uint8_t>(std::min(255.0f, rgba[0] * k)), static_cast<std::uint8_t>(std::min(255.0f, rgba[1] * k)), static_cast<std::uint8_t>(std::min(255.0f, rgba[2] * k)), 255);
    blend(img, x, y, c, sa);
}

void fill_rect(images::BitmapImage& img, float x, float y, float w, float h, const graphics::Color& c) {
    if (w <= 0.0f || h <= 0.0f) return;
    const int x0 = static_cast<int>(std::floor(x)), x1 = static_cast<int>(std::ceil(x + w));
    const int y0 = static_cast<int>(std::floor(y)), y1 = static_cast<int>(std::ceil(y + h));
    for (int py = std::max(0, y0); py < std::min(static_cast<int>(img.height()), y1); ++py) {
        const float cy = std::min(y + h, py + 1.0f) - std::max(y, static_cast<float>(py));
        if (cy <= 0.0f) continue;
        for (int px = std::max(0, x0); px < std::min(static_cast<int>(img.width()), x1); ++px) {
            const float cx = std::min(x + w, px + 1.0f) - std::max(x, static_cast<float>(px));
            if (cx > 0.0f) blend(img, px, py, c, cx * cy);
        }
    }
}

float segment_distance(float px, float py, const FancyPoint& a, const FancyPoint& b) noexcept {
    const float vx = b.x - a.x, vy = b.y - a.y;
    const float wx = px - a.x, wy = py - a.y;
    const float len2 = vx * vx + vy * vy;
    float t = len2 > 0.0f ? (wx * vx + wy * vy) / len2 : 0.0f;
    t = clamp01(t);
    const float dx = wx - t * vx, dy = wy - t * vy;
    return std::sqrt(dx * dx + dy * dy);
}

void stroke_path(images::BitmapImage& img, const std::vector<FancyPoint>& pts, bool closed, float thickness, const graphics::Color& c) {
    if (pts.size() < 2) return;
    const float half = std::max(0.5f, thickness * 0.5f);
    float minx = pts[0].x, maxx = pts[0].x, miny = pts[0].y, maxy = pts[0].y;
    for (const FancyPoint& p : pts) {
        minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
    }
    const int x0 = std::max(0, static_cast<int>(std::floor(minx - half - 1.0f)));
    const int x1 = std::min(static_cast<int>(img.width()) - 1, static_cast<int>(std::ceil(maxx + half + 1.0f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(miny - half - 1.0f)));
    const int y1 = std::min(static_cast<int>(img.height()) - 1, static_cast<int>(std::ceil(maxy + half + 1.0f)));
    const std::size_t n = pts.size();
    const std::size_t segs = closed ? n : n - 1;
    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            const float cx = px + 0.5f, cy = py + 0.5f;
            float d = 1e30f;
            for (std::size_t i = 0; i < segs; ++i) d = std::min(d, segment_distance(cx, cy, pts[i], pts[(i + 1) % n]));
            const float cov = clamp01(half - d + 0.5f);
            if (cov > 0.0f) blend(img, px, py, c, cov);
        }
    }
}

void fill_polygon(images::BitmapImage& img, const std::vector<FancyPoint>& pts, const graphics::Color& c) {
    if (pts.size() < 3) return;
    float miny = pts[0].y, maxy = pts[0].y, minx = pts[0].x, maxx = pts[0].x;
    for (const FancyPoint& p : pts) {
        miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
        minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
    }
    const int y0 = std::max(0, static_cast<int>(std::floor(miny)));
    const int y1 = std::min(static_cast<int>(img.height()) - 1, static_cast<int>(std::ceil(maxy)));
    const int x0 = std::max(0, static_cast<int>(std::floor(minx)));
    const int x1 = std::min(static_cast<int>(img.width()) - 1, static_cast<int>(std::ceil(maxx)));
    constexpr int S = 4;
    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            int inside = 0;
            for (int sy = 0; sy < S; ++sy) {
                for (int sx = 0; sx < S; ++sx) {
                    const float x = px + (sx + 0.5f) / S, y = py + (sy + 0.5f) / S;
                    bool in = false;
                    for (std::size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
                        const FancyPoint& a = pts[i];
                        const FancyPoint& b = pts[j];
                        if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) in = !in;
                    }
                    inside += in ? 1 : 0;
                }
            }
            if (inside) blend(img, px, py, c, static_cast<float>(inside) / (S * S));
        }
    }
}

} // namespace fancy_detail
} // namespace text
} // namespace fizmo

namespace fizmo {
namespace text {

void draw_fancy_text(images::BitmapImage& img, const FancyText& ft, float x, float y, TextRasterizer& raster, FancyAnchor anchor) {
    const float ox = x;
    const float oy = anchor == FancyAnchor::TopLeft ? y + ft.ascent() : y;
    for (const FancyRect& rc : ft.rects()) fancy_detail::fill_rect(img, ox + rc.x, oy + rc.y, rc.w, rc.h, rc.color);
    for (const FancyPath& p : ft.paths()) {
        std::vector<FancyPoint> pts;
        pts.reserve(p.points.size());
        for (const FancyPoint& q : p.points) pts.push_back(FancyPoint{ ox + q.x, oy + q.y });
        if (p.filled) fancy_detail::fill_polygon(img, pts, p.color);
        else fancy_detail::stroke_path(img, pts, p.closed, p.thickness, p.color);
    }
    for (const FancyRun& run : ft.runs()) raster.draw(img, run.text, std::round(ox + run.x), std::round(oy + run.baseline));
}

images::BitmapImage fancy_text_image(const FancyText& ft, TextRasterizer& raster, float padding, const graphics::Color& background) {
    const unsigned w = static_cast<unsigned>(std::max(1.0f, std::ceil(ft.width() + 2.0f * padding)));
    const unsigned h = static_cast<unsigned>(std::max(1.0f, std::ceil(ft.height() + 2.0f * padding)));
    images::BitmapImage img(w, h);
    for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) img.set_pixel(x, y, background);
    draw_fancy_text(img, ft, padding, padding, raster, FancyAnchor::TopLeft);
    return img;
}

void draw_fancy_text_native(windows::Renderer& r, const FancyText& ft, float x, float y, FancyAnchor anchor) {
    const float ox = x;
    const float oy = anchor == FancyAnchor::TopLeft ? y + ft.ascent() : y;
    for (const FancyRect& rc : ft.rects()) {
        const int rx = static_cast<int>(std::lround(ox + rc.x));
        const int ry = static_cast<int>(std::lround(oy + rc.y));
        const int rw = std::max(1, static_cast<int>(std::lround(ox + rc.x + rc.w)) - rx);
        const int rh = std::max(1, static_cast<int>(std::lround(rc.h)));
        r.draw_rect(rx, ry, static_cast<unsigned>(rw), static_cast<unsigned>(rh), graphics::Paint::fill(rc.color));
    }
    for (const FancyPath& p : ft.paths()) {
        std::vector<windows::RenderPoint> pts;
        pts.reserve(p.points.size());
        for (const FancyPoint& q : p.points) pts.emplace_back(ox + q.x, oy + q.y);
        if (p.filled && pts.size() >= 3) r.draw_polygon(pts, graphics::Paint::fill(p.color));
        else r.draw_polyline(pts, graphics::Paint::stroke(p.color, static_cast<unsigned>(std::max(1L, std::lround(p.thickness)))), p.closed);
    }
    for (const FancyRun& run : ft.runs()) {
        const text::TextMetrics m = r.measure_rich_text(run.text);
        const int tx = static_cast<int>(std::lround(ox + run.x));
        const int ty = static_cast<int>(std::lround(oy + run.baseline)) - m.ascent;
        r.draw_rich_text(tx, ty, run.text);
    }
}

FancyTexture fancy_text_texture(const FancyText& ft, TextRasterizer& raster, float padding) {
    FancyTexture t;
    t.padding = std::round(padding);
    t.width = ft.width();
    t.ascent = ft.ascent();
    t.descent = ft.descent();
    const unsigned w = static_cast<unsigned>(std::max(1.0f, std::ceil(ft.width() + 2.0f * t.padding)));
    const unsigned h = static_cast<unsigned>(std::max(1.0f, std::ceil(ft.ascent()) + std::ceil(ft.descent()) + 2.0f * t.padding));
    images::BitmapImage img(w, h);
    for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) img.set_pixel(x, y, graphics::Color(255, 255, 255, 0));
    draw_fancy_text(img, ft, t.padding, t.padding + std::ceil(ft.ascent()), raster, FancyAnchor::Baseline);
    t.texture = graphics::Texture(std::move(img), graphics::SampleFilter::Bilinear);
    return t;
}

void draw_fancy_texture(windows::Renderer& r, const FancyTexture& t, float x, float y, FancyAnchor anchor) {
    if (!t.valid()) return;
    const float baseline = anchor == FancyAnchor::TopLeft ? y + t.ascent : y;
    const int dx = static_cast<int>(std::lround(x - t.padding));
    const int dy = static_cast<int>(std::lround(baseline - std::ceil(t.ascent) - t.padding));
    r.draw_texture(t.texture, dx, dy);
}

void draw_fancy_text(windows::Renderer& r, const FancyText& ft, float x, float y, FancyAnchor anchor, FancyDrawMode mode) {
    const bool raster = mode == FancyDrawMode::Rasterized || (mode == FancyDrawMode::Auto && !r.is_gpu());
    if (!raster) {
        draw_fancy_text_native(r, ft, x, y, anchor);
        return;
    }
    draw_fancy_texture(r, fancy_text_texture(ft), x, y, anchor);
}

} // namespace text
} // namespace fizmo
