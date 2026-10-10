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
    void move_local(double forward, double sideways) noexcept;

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
    void set_size(double w, double h) noexcept;

    // Uniform scale that fits the frame inside w x h.
    void fit_size(double w, double h) noexcept;

    // ---- flipping (texture only; geometry and hit area stay put) ----------------------------------
    bool flipped_h() const noexcept { return m_flip_h; }
    bool flipped_v() const noexcept { return m_flip_v; }
    void set_flip_h(bool f) noexcept { m_flip_h = f; }
    void set_flip_v(bool f) noexcept { m_flip_v = f; }
    void flip_h() noexcept { m_flip_h = !m_flip_h; }
    void flip_v() noexcept { m_flip_v = !m_flip_v; }

    // ---- pivot ----------------------------------------------------------------------------------
    RectOrigin origin() const noexcept { return m_origin; }

    void set_origin(RectOrigin o) noexcept;

    double pivot_x() const noexcept { return m_pivot_x; }
    double pivot_y() const noexcept { return m_pivot_y; }
    void set_pivot(double nx, double ny) noexcept { m_pivot_x = nx; m_pivot_y = ny; }

    // Pivot in source-frame pixels (e.g. a character's feet at (16, 30) in a 32x32 frame).
    void set_pivot_pixels(double px, double py) noexcept;

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
    ) noexcept;

    unsigned int frame_width()  const noexcept { return m_source_rect.w; }
    unsigned int frame_height() const noexcept { return m_source_rect.h; }

    double display_width()  const noexcept { return m_source_rect.w * std::abs(m_scale_x); }
    double display_height() const noexcept { return m_source_rect.h * std::abs(m_scale_y); }

    // ---- transforms -----------------------------------------------------------------------------
    // Maps source-frame pixel coordinates (0..frame_w, 0..frame_h) to world coordinates.
    math::Matrix3d transform() const noexcept;

    vector2d local_to_world(double lx, double ly) const noexcept;

    // Inverse of local_to_world. Returns false when the sprite has zero scale.
    bool world_to_local(double wx, double wy, double& lx, double& ly) const noexcept;

    // Texel under a world point, honouring flips; false when the point is outside the sprite.
    bool texel_at(double wx, double wy, int& tx, int& ty) const noexcept;

    // ---- bounds & hit testing -------------------------------------------------------------------
    struct Bounds {
        double x, y, w, h;
        bool contains(double px, double py) const noexcept { return px >= x && px < x + w && py >= y && py < y + h; }
        bool intersects(const Bounds& o) const noexcept { return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h; }
    };

    // Axis-aligned bounds ignoring rotation (position, pivot and scale only).
    Bounds bounds() const noexcept;

    // Axis-aligned bounds of the rotated quad.
    Bounds rotated_bounds() const noexcept;

    // Exact test against the rotated, scaled quad.
    bool contains(double px, double py) const noexcept;

    // Like contains(), but also requires the texel under the point to have alpha >= threshold.
    bool contains_opaque(double px, double py, std::uint8_t alpha_threshold = 1) const noexcept;

    // World-space corners, ordered to match the source rect (see SpriteQuad). Flips permute the corners
    // so the texture is mirrored in place.
    SpriteQuad quad() const noexcept;

    bool drawable() const noexcept;

    bool operator<(const Sprite& o) const noexcept { return m_z_order < o.m_z_order; }
    bool operator>(const Sprite& o) const noexcept { return m_z_order > o.m_z_order; }
};

// Stable sort by z_order (lowest first) for a list of sprites about to be drawn.
void sort_by_z(std::vector<Sprite*>& sprites);



} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SPRITE_HPP