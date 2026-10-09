#ifndef FIZMO_SPRITE_HPP
#define FIZMO_SPRITE_HPP

#include "texture.hpp"
#include "paint.hpp"
#include "draw_types_2d.hpp"
#include "../Matrices/square_matrices.hpp"
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>
#include "animation_controller.hpp"

namespace fizmo {
namespace graphics {

// Corners in draw order: they map to the source rect's top-left, top-right, bottom-right, bottom-left.
struct SpriteQuad {
    double x[4];
    double y[4];
};

// A textured, transformable quad drawn with Renderer::draw_sprite (GPU on the Vulkan path).
//
// Geometry: position -> rotation (degrees, clockwise in screen space) -> scale -> pivot.
// The pivot is a normalized point on the source frame (0,0 = top-left, 1,1 = bottom-right) that sits at
// the sprite's position and that rotation/scale happen around. set_origin() picks one of the nine presets.
// A negative scale mirrors the geometry around the pivot; flip_h/flip_v mirror only the texture, in place.
class Sprite {
private:
    Texture     m_texture;
    double      m_x        = 0.0;
    double      m_y        = 0.0;
    double      m_rotation = 0.0;
    double      m_scale_x  = 1.0;
    double      m_scale_y  = 1.0;
    double      m_pivot_x  = 0.5;
    double      m_pivot_y  = 0.5;
    float       m_opacity  = 1.0f;
    bool        m_visible  = true;
    bool        m_flip_h   = false;
    bool        m_flip_v   = false;
    RectOrigin  m_origin   = RectOrigin::Center;
    TextureRect m_source_rect;
    int         m_z_order  = 0;
    Color       m_tint     = Color(255, 255, 255, 255);
    BlendMode   m_blend    = BlendMode::Normal;

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

    // ---- position -------------------------------------------------------------------------------
    double x() const noexcept { return m_x; }
    double y() const noexcept { return m_y; }
    vector2d position() const noexcept { return { m_x, m_y }; }
    void set_x(double x) noexcept { m_x = x; }
    void set_y(double y) noexcept { m_y = y; }
    void set_position(double x, double y) noexcept { m_x = x; m_y = y; }
    void set_position(const vector2d& p) noexcept { m_x = p.x; m_y = p.y; }
    void translate(double dx, double dy) noexcept { m_x += dx; m_y += dy; }

    // Moves in the sprite's own facing (local +x is "forward" at rotation 0).
    void move_local(double forward, double sideways) noexcept {
        const double rad = m_rotation * constants::pi_180();
        const double c = std::cos(rad), s = std::sin(rad);
        m_x += forward * c - sideways * s;
        m_y += forward * s + sideways * c;
    }

    // ---- rotation -------------------------------------------------------------------------------
    double rotation() const noexcept { return m_rotation; }
    void set_rotation(double degrees) noexcept { m_rotation = degrees; }
    void rotate(double degrees) noexcept { m_rotation += degrees; }

    // Rotation (degrees) that points local +x at (tx, ty), plus an optional offset for art that faces another way.
    double angle_to(double tx, double ty) const noexcept { return std::atan2(ty - m_y, tx - m_x) / constants::pi_180(); }
    void look_at(double tx, double ty, double art_offset_degrees = 0.0) noexcept { m_rotation = angle_to(tx, ty) + art_offset_degrees; }

    // ---- scale / size ---------------------------------------------------------------------------
    double scale_x() const noexcept { return m_scale_x; }
    double scale_y() const noexcept { return m_scale_y; }
    void set_scale_x(double sx) noexcept { m_scale_x = sx; }
    void set_scale_y(double sy) noexcept { m_scale_y = sy; }
    void set_scale(double sx, double sy) noexcept { m_scale_x = sx; m_scale_y = sy; }
    void set_scale(double s) noexcept { m_scale_x = s; m_scale_y = s; }
    void scale_by(double fx, double fy) noexcept { m_scale_x *= fx; m_scale_y *= fy; }

    // Sets the scale so the current frame is drawn at w x h (keeps the sign of the current scale).
    void set_size(double w, double h) noexcept {
        if (m_source_rect.w) m_scale_x = std::copysign(w / m_source_rect.w, m_scale_x);
        if (m_source_rect.h) m_scale_y = std::copysign(h / m_source_rect.h, m_scale_y);
    }

    // Uniform scale that fits the frame inside w x h.
    void fit_size(double w, double h) noexcept {
        if (!m_source_rect.w || !m_source_rect.h) return;
        set_scale(std::min(w / m_source_rect.w, h / m_source_rect.h));
    }

    // ---- flipping (texture only; geometry and hit area stay put) ----------------------------------
    bool flipped_h() const noexcept { return m_flip_h; }
    bool flipped_v() const noexcept { return m_flip_v; }
    void set_flip_h(bool f) noexcept { m_flip_h = f; }
    void set_flip_v(bool f) noexcept { m_flip_v = f; }
    void flip_h() noexcept { m_flip_h = !m_flip_h; }
    void flip_v() noexcept { m_flip_v = !m_flip_v; }

    // ---- pivot ----------------------------------------------------------------------------------
    RectOrigin origin() const noexcept { return m_origin; }

    void set_origin(RectOrigin o) noexcept {
        m_origin = o;
        const int col = static_cast<int>(o) % 3, row = static_cast<int>(o) / 3;
        m_pivot_x = col * 0.5;
        m_pivot_y = row * 0.5;
    }

    double pivot_x() const noexcept { return m_pivot_x; }
    double pivot_y() const noexcept { return m_pivot_y; }
    void set_pivot(double nx, double ny) noexcept { m_pivot_x = nx; m_pivot_y = ny; }

    // Pivot in source-frame pixels (e.g. a character's feet at (16, 30) in a 32x32 frame).
    void set_pivot_pixels(double px, double py) noexcept {
        m_pivot_x = m_source_rect.w ? px / m_source_rect.w : 0.0;
        m_pivot_y = m_source_rect.h ? py / m_source_rect.h : 0.0;
    }

    // ---- appearance -----------------------------------------------------------------------------
    float opacity() const noexcept { return m_opacity; }
    void set_opacity(float o) noexcept { m_opacity = std::max(0.0f, std::min(1.0f, o)); }

    bool visible() const noexcept { return m_visible; }
    void set_visible(bool v) noexcept { m_visible = v; }

    int z_order() const noexcept { return m_z_order; }
    void set_z_order(int z) noexcept { m_z_order = z; }

    const Color& tint() const noexcept { return m_tint; }
    void set_tint(const Color& c) noexcept { m_tint = c; }
    void clear_tint() noexcept { m_tint = Color(255, 255, 255, 255); }
    bool tinted() const noexcept { return !is_white(m_tint); }

    BlendMode blend_mode() const noexcept { return m_blend; }
    void set_blend_mode(BlendMode m) noexcept { m_blend = m; }

    // ---- texture & frames -----------------------------------------------------------------------
    const Texture& texture() const noexcept { return m_texture; }
    Texture& texture() noexcept { return m_texture; }
    void set_texture(const Texture& tex) noexcept { m_texture = tex; m_source_rect = tex.full_rect(); }
    void set_texture(Texture&& tex) noexcept { m_texture = std::move(tex); m_source_rect = m_texture.full_rect(); }
    void set_texture(const Texture& tex, bool keep_source_rect) noexcept { m_texture = tex; if (!keep_source_rect) m_source_rect = tex.full_rect(); }

    const TextureRect& source_rect() const noexcept { return m_source_rect; }
    void set_source_rect(const TextureRect& r) noexcept { m_source_rect = r; }
    void set_source_rect(int sx, int sy, unsigned int sw, unsigned int sh) noexcept { m_source_rect = TextureRect(sx, sy, sw, sh); }
    void reset_source_rect() noexcept { m_source_rect = m_texture.full_rect(); }

    // Selects frame `index` from a grid sprite sheet laid out left-to-right, top-to-bottom.
    // columns = 0 derives the column count from the texture width. margin is the border around the
    // sheet, spacing the gap between frames (both in pixels). Returns false if the frame is off the sheet.
    bool set_frame(
        unsigned int index,
        unsigned int frame_w, unsigned int frame_h,
        unsigned int columns = 0,
        unsigned int margin = 0, unsigned int spacing = 0
    ) noexcept {
        if (frame_w == 0 || frame_h == 0) return false;

        if (columns == 0) {
            const unsigned int tw = m_texture.width();
            if (tw < 2 * margin + frame_w) return false;
            columns = (tw - 2 * margin + spacing) / (frame_w + spacing);
            if (columns == 0) return false;
        }

        const unsigned int col = index % columns, row = index / columns;
        const unsigned long long fx = margin + static_cast<unsigned long long>(col) * (frame_w + spacing);
        const unsigned long long fy = margin + static_cast<unsigned long long>(row) * (frame_h + spacing);
        if (m_texture.valid() && (fx + frame_w > m_texture.width() || fy + frame_h > m_texture.height())) return false;
        m_source_rect = TextureRect(static_cast<int>(fx), static_cast<int>(fy), frame_w, frame_h);
        return true;
    }

    unsigned int frame_width()  const noexcept { return m_source_rect.w; }
    unsigned int frame_height() const noexcept { return m_source_rect.h; }

    double display_width()  const noexcept { return m_source_rect.w * std::abs(m_scale_x); }
    double display_height() const noexcept { return m_source_rect.h * std::abs(m_scale_y); }

    // ---- transforms -----------------------------------------------------------------------------
    // Maps source-frame pixel coordinates (0..frame_w, 0..frame_h) to world coordinates.
    math::Matrix3d transform() const noexcept {
        const double rad = m_rotation * constants::pi_180();
        const double c = std::cos(rad), s = std::sin(rad);
        const double px = m_pivot_x * m_source_rect.w, py = m_pivot_y * m_source_rect.h;
        const double a = c * m_scale_x, b = -s * m_scale_y;
        const double d = s * m_scale_x, e =  c * m_scale_y;

        return math::Matrix3d{
            a, b, m_x - (a * px + b * py),
            d, e, m_y - (d * px + e * py),
            0.0, 0.0, 1.0
        };
    }

    vector2d local_to_world(double lx, double ly) const noexcept {
        const math::Matrix3d m = transform();
        return { m.data[0] * lx + m.data[1] * ly + m.data[2], m.data[3] * lx + m.data[4] * ly + m.data[5] };
    }

    // Inverse of local_to_world. Returns false when the sprite has zero scale.
    bool world_to_local(double wx, double wy, double& lx, double& ly) const noexcept {
        if (m_scale_x == 0.0 || m_scale_y == 0.0) return false;
        const double rad = m_rotation * constants::pi_180();
        const double c = std::cos(rad), s = std::sin(rad);
        const double dx = wx - m_x, dy = wy - m_y;
        lx = ( dx * c + dy * s) / m_scale_x + m_pivot_x * m_source_rect.w;
        ly = (-dx * s + dy * c) / m_scale_y + m_pivot_y * m_source_rect.h;
        return true;
    }

    // Texel under a world point, honouring flips; false when the point is outside the sprite.
    bool texel_at(double wx, double wy, int& tx, int& ty) const noexcept {
        double lx = 0.0, ly = 0.0;
        if (!world_to_local(wx, wy, lx, ly)) return false;
        if (lx < 0.0 || ly < 0.0 || lx >= m_source_rect.w || ly >= m_source_rect.h) return false;
        if (m_flip_h) lx = m_source_rect.w - lx;
        if (m_flip_v) ly = m_source_rect.h - ly;
        tx = m_source_rect.x + std::min(static_cast<int>(lx), static_cast<int>(m_source_rect.w) - 1);
        ty = m_source_rect.y + std::min(static_cast<int>(ly), static_cast<int>(m_source_rect.h) - 1);
        return true;
    }

    // ---- bounds & hit testing -------------------------------------------------------------------
    struct Bounds {
        double x, y, w, h;
        bool contains(double px, double py) const noexcept { return px >= x && px < x + w && py >= y && py < y + h; }
        bool intersects(const Bounds& o) const noexcept { return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h; }
    };

    // Axis-aligned bounds ignoring rotation (position, pivot and scale only).
    Bounds bounds() const noexcept {
        const double px = m_pivot_x * m_source_rect.w, py = m_pivot_y * m_source_rect.h;
        const double ax = -px * m_scale_x, bx = (m_source_rect.w - px) * m_scale_x;
        const double ay = -py * m_scale_y, by = (m_source_rect.h - py) * m_scale_y;
        return { m_x + std::min(ax, bx), m_y + std::min(ay, by), std::abs(bx - ax), std::abs(by - ay) };
    }

    // Axis-aligned bounds of the rotated quad.
    Bounds rotated_bounds() const noexcept {
        const SpriteQuad q = quad();
        double x0 = q.x[0], x1 = q.x[0], y0 = q.y[0], y1 = q.y[0];

        for (int i = 1; i < 4; ++i) {
            x0 = std::min(x0, q.x[i]); x1 = std::max(x1, q.x[i]);
            y0 = std::min(y0, q.y[i]); y1 = std::max(y1, q.y[i]);
        }

        return { x0, y0, x1 - x0, y1 - y0 };
    }

    // Exact test against the rotated, scaled quad.
    bool contains(double px, double py) const noexcept {
        if (m_source_rect.is_empty()) return false;
        double lx = 0.0, ly = 0.0;
        if (!world_to_local(px, py, lx, ly)) return false;
        return lx >= 0.0 && lx < m_source_rect.w && ly >= 0.0 && ly < m_source_rect.h;
    }

    // Like contains(), but also requires the texel under the point to have alpha >= threshold.
    bool contains_opaque(double px, double py, std::uint8_t alpha_threshold = 1) const noexcept {
        int tx = 0, ty = 0;
        if (!m_texture.valid() || !texel_at(px, py, tx, ty)) return false;
        return m_texture.sample(tx + 0.5, ty + 0.5).alpha() >= alpha_threshold;
    }

    // World-space corners, ordered to match the source rect (see SpriteQuad). Flips permute the corners
    // so the texture is mirrored in place.
    SpriteQuad quad() const noexcept {
        const math::Matrix3d m = transform();
        const double w = m_source_rect.w, h = m_source_rect.h;
        const double lx[4] = { 0.0, w, w, 0.0 };
        const double ly[4] = { 0.0, 0.0, h, h };
        double gx[4], gy[4];

        for (int i = 0; i < 4; ++i) {
            gx[i] = m.data[0] * lx[i] + m.data[1] * ly[i] + m.data[2];
            gy[i] = m.data[3] * lx[i] + m.data[4] * ly[i] + m.data[5];
        }

        // corner index flips: bit 0 swaps left/right (0<->1, 2<->3), bit 1 swaps top/bottom (0<->3, 1<->2)
        static constexpr int order[4][4] = { { 0, 1, 2, 3 }, { 1, 0, 3, 2 }, { 3, 2, 1, 0 }, { 2, 3, 0, 1 } };
        const int* o = order[(m_flip_h ? 1 : 0) | (m_flip_v ? 2 : 0)];
        SpriteQuad q;

        for (int i = 0; i < 4; ++i) { q.x[i] = gx[o[i]]; q.y[i] = gy[o[i]]; }
        return q;
    }

    bool drawable() const noexcept {
        return m_visible && m_texture.valid() && !m_source_rect.is_empty() && m_opacity > 0.0f && m_tint.alpha() > 0
            && m_scale_x != 0.0 && m_scale_y != 0.0;
    }

    bool operator<(const Sprite& o) const noexcept { return m_z_order < o.m_z_order; }
    bool operator>(const Sprite& o) const noexcept { return m_z_order > o.m_z_order; }
};

// Stable sort by z_order (lowest first) for a list of sprites about to be drawn.
inline void sort_by_z(std::vector<Sprite*>& sprites) {
    std::stable_sort(sprites.begin(), sprites.end(), [](const Sprite* a, const Sprite* b) { return a->z_order() < b->z_order(); });
}

inline void AnimationController::apply_frame() noexcept {
    if (!m_animation || m_animation->empty()) return;
    const auto& rect = m_animation->frame(m_index).rect;
    if (m_target) { m_target->set_source_rect(rect); }
    if (m_on_frame_changed) { m_on_frame_changed(m_index); }
}

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SPRITE_HPP