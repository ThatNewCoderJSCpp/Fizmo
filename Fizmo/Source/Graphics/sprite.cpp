#include "fizmo_library.hpp"
#include "sprite.hpp"

namespace fizmo {
namespace graphics {

void Sprite::move_local(double forward, double sideways) noexcept {
    const double rad = m_rotation * constants::pi_180();
    const double c = std::cos(rad), s = std::sin(rad);
    m_x += forward * c - sideways * s;
    m_y += forward * s + sideways * c;
}

void Sprite::set_size(double w, double h) noexcept {
    if (m_source_rect.w) m_scale_x = std::copysign(w / m_source_rect.w, m_scale_x);
    if (m_source_rect.h) m_scale_y = std::copysign(h / m_source_rect.h, m_scale_y);
}

void Sprite::fit_size(double w, double h) noexcept {
    if (!m_source_rect.w || !m_source_rect.h) return;
    set_scale(std::min(w / m_source_rect.w, h / m_source_rect.h));
}

void Sprite::set_origin(RectOrigin o) noexcept {
    m_origin = o;
    const int col = static_cast<int>(o) % 3, row = static_cast<int>(o) / 3;
    m_pivot_x = col * 0.5;
    m_pivot_y = row * 0.5;
}

void Sprite::set_pivot_pixels(double px, double py) noexcept {
    m_pivot_x = m_source_rect.w ? px / m_source_rect.w : 0.0;
    m_pivot_y = m_source_rect.h ? py / m_source_rect.h : 0.0;
}

bool Sprite::set_frame(
    unsigned int index,
    unsigned int frame_w, unsigned int frame_h,
    unsigned int columns,
    unsigned int margin, unsigned int spacing 
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

auto Sprite::transform() const noexcept -> math::Matrix3d {
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

auto Sprite::local_to_world(double lx, double ly) const noexcept -> vector2d {
    const math::Matrix3d m = transform();
    return { m.data[0] * lx + m.data[1] * ly + m.data[2], m.data[3] * lx + m.data[4] * ly + m.data[5] };
}

bool Sprite::world_to_local(double wx, double wy, double& lx, double& ly) const noexcept {
    if (m_scale_x == 0.0 || m_scale_y == 0.0) return false;
    const double rad = m_rotation * constants::pi_180();
    const double c = std::cos(rad), s = std::sin(rad);
    const double dx = wx - m_x, dy = wy - m_y;
    lx = ( dx * c + dy * s) / m_scale_x + m_pivot_x * m_source_rect.w;
    ly = (-dx * s + dy * c) / m_scale_y + m_pivot_y * m_source_rect.h;
    return true;
}

bool Sprite::texel_at(double wx, double wy, int& tx, int& ty) const noexcept {
    double lx = 0.0, ly = 0.0;
    if (!world_to_local(wx, wy, lx, ly)) return false;
    if (lx < 0.0 || ly < 0.0 || lx >= m_source_rect.w || ly >= m_source_rect.h) return false;
    if (m_flip_h) lx = m_source_rect.w - lx;
    if (m_flip_v) ly = m_source_rect.h - ly;
    tx = m_source_rect.x + std::min(static_cast<int>(lx), static_cast<int>(m_source_rect.w) - 1);
    ty = m_source_rect.y + std::min(static_cast<int>(ly), static_cast<int>(m_source_rect.h) - 1);
    return true;
}

auto Sprite::bounds() const noexcept -> Bounds {
    const double px = m_pivot_x * m_source_rect.w, py = m_pivot_y * m_source_rect.h;
    const double ax = -px * m_scale_x, bx = (m_source_rect.w - px) * m_scale_x;
    const double ay = -py * m_scale_y, by = (m_source_rect.h - py) * m_scale_y;
    return { m_x + std::min(ax, bx), m_y + std::min(ay, by), std::abs(bx - ax), std::abs(by - ay) };
}

auto Sprite::rotated_bounds() const noexcept -> Bounds {
    const SpriteQuad q = quad();
    double x0 = q.x[0], x1 = q.x[0], y0 = q.y[0], y1 = q.y[0];

    for (int i = 1; i < 4; ++i) {
        x0 = std::min(x0, q.x[i]); x1 = std::max(x1, q.x[i]);
        y0 = std::min(y0, q.y[i]); y1 = std::max(y1, q.y[i]);
    }

    return { x0, y0, x1 - x0, y1 - y0 };
}

bool Sprite::contains(double px, double py) const noexcept {
    if (m_source_rect.is_empty()) return false;
    double lx = 0.0, ly = 0.0;
    if (!world_to_local(px, py, lx, ly)) return false;
    return lx >= 0.0 && lx < m_source_rect.w && ly >= 0.0 && ly < m_source_rect.h;
}

bool Sprite::contains_opaque(double px, double py, std::uint8_t alpha_threshold) const noexcept {
    int tx = 0, ty = 0;
    if (!m_texture.valid() || !texel_at(px, py, tx, ty)) return false;
    return m_texture.sample(tx + 0.5, ty + 0.5).alpha() >= alpha_threshold;
}

auto Sprite::quad() const noexcept -> SpriteQuad {
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

bool Sprite::drawable() const noexcept {
    return m_visible && m_texture.valid() && !m_source_rect.is_empty() && m_opacity > 0.0f && m_tint.alpha() > 0
        && m_scale_x != 0.0 && m_scale_y != 0.0;
}

void sort_by_z(std::vector<Sprite*>& sprites) {
    std::stable_sort(sprites.begin(), sprites.end(), [](const Sprite* a, const Sprite* b) { return a->z_order() < b->z_order(); });
}

void AnimationController::apply_frame() noexcept {
    if (!m_animation || m_animation->empty()) return;
    const auto& rect = m_animation->frame(m_index).rect;
    if (m_target) { m_target->set_source_rect(rect); }
    if (m_on_frame_changed) { m_on_frame_changed(m_index); }
}

} // namespace graphics
} // namespace fizmo
