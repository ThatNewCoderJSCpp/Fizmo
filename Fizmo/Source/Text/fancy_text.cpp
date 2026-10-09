#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "fancy_text.hpp"

namespace fizmo {
namespace text {

auto FancyText::add_run(float x, float baseline, RichText text, std::uint32_t tag) -> FancyText& {
        if (!text.empty()) m_runs.push_back(FancyRun{ x, baseline, std::move(text), tag });
        return *this;
    }

auto FancyText::add_path(std::vector<FancyPoint> points, float thickness, const graphics::Color& color, bool closed, bool filled, std::uint32_t tag) -> FancyText& {
        if (points.size() >= 2) m_paths.push_back(FancyPath{ std::move(points), thickness, closed, filled, color, tag });
        return *this;
    }

auto FancyText::translate(float dx, float dy) -> FancyText& {
        for (FancyRun& r : m_runs) { r.x += dx; r.baseline += dy; }
        for (FancyRect& r : m_rects) { r.x += dx; r.y += dy; }
        for (FancyPath& p : m_paths) for (FancyPoint& q : p.points) { q.x += dx; q.y += dy; }
        return *this;
    }

auto FancyText::append(const FancyText& other, float dx, float dy) -> FancyText& {
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

auto FancyText::recolor(const graphics::Color& color) -> FancyText& {
        for (FancyRun& r : m_runs) {
            r.text.base().set_fg_color(color);
            for (TextSpan& sp : r.text.spans()) if (sp.style.has_fg_color()) sp.style.set_fg_color(color);
        }
        for (FancyRect& r : m_rects) r.color = color;
        for (FancyPath& p : m_paths) p.color = color;
        return *this;
    }

auto FancyText::scale(float k) -> FancyText& {
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

} // namespace text
} // namespace fizmo
