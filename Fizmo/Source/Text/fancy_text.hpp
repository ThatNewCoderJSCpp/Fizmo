#ifndef FIZMO_FANCY_TEXT_HPP
#define FIZMO_FANCY_TEXT_HPP

#include "rich_text.hpp"
#include "../Graphics/color.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace fizmo {
namespace text {

inline constexpr std::uint32_t kNoFancyTag = 0xFFFFFFFFu;

struct FancyPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct FancyRun {
    float         x = 0.0f;
    float         baseline = 0.0f;
    RichText      text;
    std::uint32_t tag = kNoFancyTag;
};

struct FancyRect {
    float           x = 0.0f;
    float           y = 0.0f;
    float           w = 0.0f;
    float           h = 0.0f;
    graphics::Color color;
    std::uint32_t   tag = kNoFancyTag;
};

struct FancyPath {
    std::vector<FancyPoint> points;
    float                   thickness = 1.0f;
    bool                    closed = false;
    bool                    filled = false;
    graphics::Color         color;
    std::uint32_t           tag = kNoFancyTag;
};

struct FancyBounds {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

class FancyText {
public:
    FancyText() = default;

    FancyText& add_run(float x, float baseline, RichText text, std::uint32_t tag = kNoFancyTag) {
        if (!text.empty()) m_runs.push_back(FancyRun{ x, baseline, std::move(text), tag });
        return *this;
    }

    FancyText& add_text(float x, float baseline, std::string text, const TextStyle& style, std::uint32_t tag = kNoFancyTag) { return add_run(x, baseline, RichText(std::move(text), style), tag); }

    FancyText& add_rect(float x, float y, float w, float h, const graphics::Color& color, std::uint32_t tag = kNoFancyTag) {
        if (w > 0.0f && h > 0.0f) m_rects.push_back(FancyRect{ x, y, w, h, color, tag });
        return *this;
    }

    FancyText& add_path(std::vector<FancyPoint> points, float thickness, const graphics::Color& color, bool closed = false, bool filled = false, std::uint32_t tag = kNoFancyTag) {
        if (points.size() >= 2) m_paths.push_back(FancyPath{ std::move(points), thickness, closed, filled, color, tag });
        return *this;
    }

    FancyText& add_line(float x0, float y0, float x1, float y1, float thickness, const graphics::Color& color, std::uint32_t tag = kNoFancyTag) { return add_path({ FancyPoint{ x0, y0 }, FancyPoint{ x1, y1 } }, thickness, color, false, false, tag); }

    void set_box(float width, float ascent, float descent) noexcept {
        m_width = width;
        m_ascent = ascent;
        m_descent = descent;
    }

    float width() const noexcept { return m_width; }
    float ascent() const noexcept { return m_ascent; }
    float descent() const noexcept { return m_descent; }
    float height() const noexcept { return m_ascent + m_descent; }

    FancyBounds box() const noexcept { return FancyBounds{ 0.0f, -m_ascent, m_width, m_ascent + m_descent }; }

    const std::vector<FancyRun>&  runs() const noexcept { return m_runs; }
    const std::vector<FancyRect>& rects() const noexcept { return m_rects; }
    const std::vector<FancyPath>& paths() const noexcept { return m_paths; }
    std::vector<FancyRun>&        runs() noexcept { return m_runs; }
    std::vector<FancyRect>&       rects() noexcept { return m_rects; }
    std::vector<FancyPath>&       paths() noexcept { return m_paths; }

    bool empty() const noexcept { return m_runs.empty() && m_rects.empty() && m_paths.empty(); }

    void clear() noexcept {
        m_runs.clear();
        m_rects.clear();
        m_paths.clear();
        m_width = m_ascent = m_descent = 0.0f;
    }

    FancyText& translate(float dx, float dy) {
        for (FancyRun& r : m_runs) { r.x += dx; r.baseline += dy; }
        for (FancyRect& r : m_rects) { r.x += dx; r.y += dy; }
        for (FancyPath& p : m_paths) for (FancyPoint& q : p.points) { q.x += dx; q.y += dy; }
        return *this;
    }

    FancyText& append(const FancyText& other, float dx, float dy) {
        for (FancyRun r : other.m_runs) { r.x += dx; r.baseline += dy; m_runs.push_back(std::move(r)); }
        for (FancyRect r : other.m_rects) { r.x += dx; r.y += dy; m_rects.push_back(r); }
        for (FancyPath p : other.m_paths) {
            for (FancyPoint& q : p.points) { q.x += dx; q.y += dy; }
            m_paths.push_back(std::move(p));
        }
        m_width = std::max(m_width, dx + other.m_width);
        m_ascent = std::max(m_ascent, other.m_ascent - dy);
        m_descent = std::max(m_descent, other.m_descent + dy);
        return *this;
    }

    FancyText& append(const FancyText& other, float gap = 0.0f) { return append(other, m_width + gap, 0.0f); }

    FancyText& recolor(const graphics::Color& color) {
        for (FancyRun& r : m_runs) {
            r.text.base().set_fg_color(color);
            for (TextSpan& sp : r.text.spans()) if (sp.style.has_fg_color()) sp.style.set_fg_color(color);
        }
        for (FancyRect& r : m_rects) r.color = color;
        for (FancyPath& p : m_paths) p.color = color;
        return *this;
    }

    FancyText& scale(float k) {
        for (FancyRun& r : m_runs) {
            r.x *= k;
            r.baseline *= k;
            TextStyle& b = r.text.base();
            if (b.has_size()) b.set_size(b.size() * k);
            for (TextSpan& sp : r.text.spans()) if (sp.style.has_size()) sp.style.set_size(sp.style.size() * k);
        }
        for (FancyRect& r : m_rects) { r.x *= k; r.y *= k; r.w *= k; r.h *= k; }
        for (FancyPath& p : m_paths) {
            p.thickness *= k;
            for (FancyPoint& q : p.points) { q.x *= k; q.y *= k; }
        }
        m_width *= k;
        m_ascent *= k;
        m_descent *= k;
        return *this;
    }

private:
    std::vector<FancyRun>  m_runs;
    std::vector<FancyRect> m_rects;
    std::vector<FancyPath> m_paths;
    float                  m_width = 0.0f;
    float                  m_ascent = 0.0f;
    float                  m_descent = 0.0f;
};

} // namespace text
} // namespace fizmo

#endif // FIZMO_FANCY_TEXT_HPP
