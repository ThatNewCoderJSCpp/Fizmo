#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "game_loop.hpp"

namespace fizmo {
namespace windows {
namespace detail {

ViewRect resolve_layout(const ViewLayout& layout, unsigned int win_w, unsigned int win_h) noexcept {
    ViewRect r;
    const bool has_fixed = layout.fixed_w > 0 && layout.fixed_h > 0;

    if (layout.mode == ViewLayout::Mode::fill || !has_fixed || win_w == 0 || win_h == 0) {
        r.w = win_w; r.h = win_h;
        return r;
    }

    switch (layout.mode) {
        case ViewLayout::Mode::fixed:
            r.w = layout.fixed_w; r.h = layout.fixed_h;
            break;
        case ViewLayout::Mode::stretch:
            r.w = win_w; r.h = win_h;
            return r;
        case ViewLayout::Mode::fit:
        default: {
            double s = std::min(static_cast<double>(win_w) / layout.fixed_w, static_cast<double>(win_h) / layout.fixed_h);
            if (layout.integer_scale && s >= 1.0) s = std::floor(s);
            r.w = std::max(1u, static_cast<unsigned int>(std::lround(layout.fixed_w * s)));
            r.h = std::max(1u, static_cast<unsigned int>(std::lround(layout.fixed_h * s)));
            break;
        }
    }

    const int iw = static_cast<int>(win_w), ih = static_cast<int>(win_h);
    const int cw = static_cast<int>(r.w),   ch = static_cast<int>(r.h);
    const int col = static_cast<int>(layout.anchor) % 3, row = static_cast<int>(layout.anchor) / 3;
    r.x = col == 0 ? 0 : (col == 1 ? (iw - cw) / 2 : iw - cw);
    r.y = row == 0 ? 0 : (row == 1 ? (ih - ch) / 2 : ih - ch);
    return r;
}

} // namespace detail
} // namespace windows
} // namespace fizmo

namespace fizmo {
namespace windows {

auto GameLoop::render_frame(Renderer& r) -> void {
        if (!uses_logical_size()) {
            if (m_auto_clear) r.clear(m_clear);
            if (m_on_render) m_on_render(r);
            return;
        }

        r.clear(m_letterbox);
        r.push_transform();
        r.reset_transform();
        r.set_clip_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);
        r.translate(m_rect.x, m_rect.y);
        r.scale(static_cast<double>(m_rect.w) / view_width(), static_cast<double>(m_rect.h) / view_height());
        if (m_auto_clear) r.draw_rect(0, 0, view_width(), view_height(), graphics::Paint::fill(m_clear));
        if (m_on_render) m_on_render(r);
        r.pop_transform();
        r.reset_clip_rect();
    }

auto GameLoop::set_logical_size(unsigned int w, unsigned int h, bool integer_scale) noexcept -> void {
        ViewLayout l = m_layout;
        l.mode = ViewLayout::Mode::fit;
        l.fixed_w = w; l.fixed_h = h;
        l.integer_scale = integer_scale;
        set_view_layout(l);
    }

auto GameLoop::window_to_view(double wx, double wy) const noexcept -> vector2d {
        if (m_rect.w == 0 || m_rect.h == 0) return { wx, wy };
        return {
            (wx - m_rect.x) * static_cast<double>(view_width())  / m_rect.w,
            (wy - m_rect.y) * static_cast<double>(view_height()) / m_rect.h
        };
    }

auto GameLoop::view_to_window(double vx, double vy) const noexcept -> vector2d {
        if (view_width() == 0 || view_height() == 0) return { vx, vy };
        return {
            m_rect.x + vx * static_cast<double>(m_rect.w) / view_width(),
            m_rect.y + vy * static_cast<double>(m_rect.h) / view_height()
        };
    }

auto GameLoop::is_inside_view(int wx, int wy) const noexcept -> bool {
        return wx >= m_rect.x && wx < m_rect.x + static_cast<int>(m_rect.w) && wy >= m_rect.y && wy < m_rect.y + static_cast<int>(m_rect.h);
    }

auto GameLoop::run() noexcept -> int {
        m_app.set_auto_clear(false);

        m_app.on_startup([this](Application&) {
            rebuild_view();
            if (m_on_startup) m_on_startup(*this);
        });

        m_app.on_event([this](const WindowEvent& e) {
            if (e.type == WindowEventType::WindowResize) rebuild_view();
            if (m_on_event) m_on_event(e);
        });

        m_app.on_update([this](double dt) { if (m_on_update) m_on_update(dt); });
        m_app.on_render([this](Renderer& r) { try { render_frame(r); } catch (...) {} });
        m_app.on_shutdown([this]() { if (m_on_shutdown) m_on_shutdown(); });
        return m_app.run();
    }

} // namespace windows
} // namespace fizmo
