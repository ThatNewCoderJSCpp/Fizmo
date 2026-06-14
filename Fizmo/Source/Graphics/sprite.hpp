#ifndef FIZMO_SPRITE_HPP
#define FIZMO_SPRITE_HPP

#include "texture.hpp"
#include "canvas.hpp"
#include "paint.hpp"
#include "../Matrices/square_matrices.hpp"
#include <cmath>
#include <algorithm>
#include "animation_controller.hpp"

namespace fizmo {
namespace graphics {

class Sprite {
private:
    Texture m_texture;
    double m_x = 0.0;
    double m_y = 0.0;
    double m_rotation = 0.0;        
    double m_scale_x = 1.0;
    double m_scale_y = 1.0;
    float m_opacity = 1.0f;
    bool m_visible = true;
    bool m_flip_h = false;
    bool m_flip_v = false;
    RectOrigin m_origin = RectOrigin::Center;
    TextureRect m_source_rect;       
    int m_z_order = 0;

public:
    Sprite() noexcept = default;
    explicit Sprite(const Texture& tex) noexcept : m_texture(tex), m_source_rect(tex.full_rect()) {}
    explicit Sprite(Texture&& tex) noexcept : m_texture(std::move(tex)), m_source_rect(m_texture.full_rect()) {}
    Sprite(const Texture& tex, double x, double y) noexcept : m_texture(tex), m_x(x), m_y(y), m_source_rect(tex.full_rect()) {}
    Sprite(Texture&& tex, double x, double y) noexcept : m_texture(std::move(tex)), m_x(x), m_y(y), m_source_rect(m_texture.full_rect()) {}
    Sprite(const Sprite&) = default;
    Sprite(Sprite&&) noexcept = default;
    Sprite& operator=(const Sprite&) = default;
    Sprite& operator=(Sprite&&) noexcept = default;

    double x() const noexcept { return m_x; }
    double y() const noexcept { return m_y; }
    void set_x(double x) noexcept { m_x = x; }
    void set_y(double y) noexcept { m_y = y; }

    void set_position(double x, double y) noexcept { m_x = x; m_y = y; }
    void translate(double dx, double dy) noexcept { m_x += dx; m_y += dy; }

    double rotation() const noexcept { return m_rotation; }
    void set_rotation(double degrees) noexcept { m_rotation = degrees; }
    void rotate(double degrees) noexcept { m_rotation += degrees; }

    double scale_x() const noexcept { return m_scale_x; }
    double scale_y() const noexcept { return m_scale_y; }
    void set_scale_x(double sx) noexcept { m_scale_x = sx; }
    void set_scale_y(double sy) noexcept { m_scale_y = sy; }
    void set_scale(double sx, double sy) noexcept { m_scale_x = sx; m_scale_y = sy; }
    void set_scale(double s) noexcept { m_scale_x = s; m_scale_y = s; }

    bool flipped_h() const noexcept { return m_flip_h; }
    bool flipped_v() const noexcept { return m_flip_v; }
    void set_flip_h(bool f) noexcept { m_flip_h = f; }
    void set_flip_v(bool f) noexcept { m_flip_v = f; }
    void flip_h() noexcept { m_flip_h = !m_flip_h; }
    void flip_v() noexcept { m_flip_v = !m_flip_v; }

    float opacity() const noexcept { return m_opacity; }
    void set_opacity(float o) noexcept { m_opacity = std::max(0.0f, std::min(1.0f, o)); }

    bool visible() const noexcept { return m_visible; }
    void set_visible(bool v) noexcept { m_visible = v; }

    RectOrigin origin() const noexcept { return m_origin; }
    void set_origin(RectOrigin o) noexcept { m_origin = o; }

    int z_order() const noexcept { return m_z_order; }
    void set_z_order(int z) noexcept { m_z_order = z; }

    const Texture& texture() const noexcept { return m_texture; }
    Texture& texture() noexcept { return m_texture; }
    void set_texture(const Texture& tex) noexcept { m_texture = tex; m_source_rect = tex.full_rect(); }
    void set_texture(Texture&& tex) noexcept { m_texture = std::move(tex); m_source_rect = m_texture.full_rect(); }

    const TextureRect& source_rect() const noexcept { return m_source_rect; }
    void set_source_rect(const TextureRect& r) noexcept { m_source_rect = r; }
    void set_source_rect(int sx, int sy, unsigned int sw, unsigned int sh) noexcept { m_source_rect = TextureRect(sx, sy, sw, sh); }
    void reset_source_rect() noexcept { m_source_rect = m_texture.full_rect(); }

    unsigned int frame_width()  const noexcept { return m_source_rect.w; }
    unsigned int frame_height() const noexcept { return m_source_rect.h; }
    
    double display_width()  const noexcept { return m_source_rect.w * std::abs(m_scale_x); }
    double display_height() const noexcept { return m_source_rect.h * std::abs(m_scale_y); }

    struct Bounds {
        double x, y, w, h;
    };

    Bounds bounds() const noexcept {
        double dw = display_width();
        double dh = display_height();
        double ox = 0.0, oy = 0.0;
        origin_offset(dw, dh, ox, oy);
        return { m_x - ox, m_y - oy, dw, dh };
    }

    bool contains(double px, double py) const noexcept {
        Bounds b = bounds();
        return px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h;
    }

    void draw(Canvas& canvas) const noexcept {
        if (!m_visible || !m_texture.valid() || m_source_rect.is_empty()) return;
        if (m_opacity <= 0.0f) return;
        unsigned int dw = static_cast<unsigned int>(std::round(display_width()));
        unsigned int dh = static_cast<unsigned int>(std::round(display_height()));
        if (dw == 0 || dh == 0) return;
        double ox = 0.0, oy = 0.0;
        origin_offset(static_cast<double>(dw), static_cast<double>(dh), ox, oy);
        canvas.save();
        canvas.translate(m_x, m_y);
        if (std::abs(m_rotation) >= constants::middle_epsilon()) { canvas.rotate(m_rotation, true); }
        double eff_sx = m_flip_h ? -1.0 : 1.0;
        double eff_sy = m_flip_v ? -1.0 : 1.0;
        if (eff_sx != 1.0 || eff_sy != 1.0) { canvas.scale(eff_sx, eff_sy); }
        canvas.translate(-ox, -oy);
        canvas.draw_texture(m_texture, 0, 0, dw, dh, m_source_rect, m_opacity, RectOrigin::TopLeft);
        canvas.restore();
    }

    bool operator<(const Sprite& o) const noexcept { return m_z_order < o.m_z_order; }
    bool operator>(const Sprite& o) const noexcept { return m_z_order > o.m_z_order; }

private:
    void origin_offset(double w, double h, double& ox, double& oy) const noexcept {
        switch (m_origin) {
            case RectOrigin::TopLeft:      ox = 0.0;     oy = 0.0;     break;
            case RectOrigin::TopCenter:    ox = w * 0.5; oy = 0.0;     break;
            case RectOrigin::TopRight:     ox = w;       oy = 0.0;     break;
            case RectOrigin::CenterLeft:   ox = 0.0;     oy = h * 0.5; break;
            case RectOrigin::Center:       ox = w * 0.5; oy = h * 0.5; break;
            case RectOrigin::CenterRight:  ox = w;       oy = h * 0.5; break;
            case RectOrigin::BottomLeft:   ox = 0.0;     oy = h;       break;
            case RectOrigin::BottomCenter: ox = w * 0.5; oy = h;       break;
            case RectOrigin::BottomRight:  ox = w;       oy = h;       break;
        }
    }
};

inline void AnimationController::apply_frame() noexcept {
    if (!m_animation || m_animation->empty()) return;
    const auto& rect = m_animation->frame(m_index).rect;
    if (m_target) { m_target->set_source_rect(rect); }
    if (m_on_frame_changed) { m_on_frame_changed(m_index); }
}

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SPRITE_HPP