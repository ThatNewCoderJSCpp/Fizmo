#include "fizmo_library.hpp"
#include "sprite_batch.hpp"

namespace fizmo {
namespace graphics {

std::size_t SpriteBatch::KeyHash::operator()(const TextureKey& k) const noexcept {
    const std::size_t h = std::hash<const void*>()(k.image);
    return h ^ (static_cast<std::size_t>(k.filter) << 1) ^ (static_cast<std::size_t>(k.wrap) << 3);
}

std::uint32_t SpriteBatch::texture_index(const Texture& tex) {
    const TextureKey key{ tex.image(), tex.filter(), tex.wrap() };
    if (m_last != npos && m_keys[m_last] == key) return m_last;

    if (m_keys.size() <= kLinearLimit) {
        for (std::size_t i = 0; i < m_keys.size(); ++i) if (m_keys[i] == key) return m_last = static_cast<std::uint32_t>(i);
    } else {
        const auto it = m_lookup.find(key);
        if (it != m_lookup.end()) return m_last = it->second;
    }

    const std::uint32_t idx = static_cast<std::uint32_t>(m_textures.size());
    m_textures.push_back(tex);
    m_keys.push_back(key);

    if (m_keys.size() > kLinearLimit) {
        if (m_lookup.empty()) for (std::size_t i = 0; i < m_keys.size(); ++i) m_lookup.emplace(m_keys[i], static_cast<std::uint32_t>(i));
        else m_lookup.emplace(key, idx);
    }

    return m_last = idx;
}

void SpriteBatch::set_uv(SpriteBatchItem& item, const Texture& tex, const TextureRect& src) noexcept {
    const float tw = static_cast<float>(tex.width()), th = static_cast<float>(tex.height());
    item.source = src;
    item.u0 = src.x / tw;
    item.v0 = src.y / th;
    item.u1 = (src.x + static_cast<float>(src.w)) / tw;
    item.v1 = (src.y + static_cast<float>(src.h)) / th;
}

bool SpriteBatch::push(const Texture& tex, const TextureRect& src, const double* xs, const double* ys, const Color& tint, float opacity, float depth) {
    if (!tex.valid() || src.is_empty() || opacity <= 0.0f || tint.alpha() == 0) return false;
    SpriteBatchItem item;
    for (int i = 0; i < 4; ++i) {
        item.x[i] = static_cast<float>(xs[i]);
        item.y[i] = static_cast<float>(ys[i]);
    }
    set_uv(item, tex, src);
    item.tint    = tint;
    item.opacity = std::min(opacity, 1.0f);
    item.depth   = depth;
    item.texture = texture_index(tex);
    m_items.push_back(item);
    return true;
}

bool SpriteBatch::add(const Sprite& sprite, const Color& tint, double offset_x, double offset_y) {
    if (!sprite.drawable()) return false;
    const SpriteQuad q = sprite.quad();
    const double xs[4] = { q.x[0] + offset_x, q.x[1] + offset_x, q.x[2] + offset_x, q.x[3] + offset_x };
    const double ys[4] = { q.y[0] + offset_y, q.y[1] + offset_y, q.y[2] + offset_y, q.y[3] + offset_y };
    return push(sprite.texture(), sprite.source_rect(), xs, ys, tint, sprite.opacity(), static_cast<float>(sprite.z_order()));
}

std::size_t SpriteBatch::add(const Sprite* const* sprites, std::size_t count) {
    if (!sprites) return 0;
    std::size_t added = 0;
    for (std::size_t i = 0; i < count; ++i) if (sprites[i] && add(*sprites[i])) ++added;
    return added;
}

bool SpriteBatch::add(
    const Texture& tex, const TextureRect& src,
    double x, double y, double w, double h,
    const Color& tint,
    double rotation_degrees,
    double pivot_x, double pivot_y,
    float depth, float opacity 
) {
    const double px = pivot_x * w, py = pivot_y * h;

    if (rotation_degrees == 0.0) {
        const double l = x - px, t = y - py;
        const double xs[4] = { l, l + w, l + w, l };
        const double ys[4] = { t, t, t + h, t + h };
        return push(tex, src, xs, ys, tint, opacity, depth);
    }

    const double rad = rotation_degrees * constants::pi_180();
    const double c = std::cos(rad), s = std::sin(rad);
    auto at = [&](double lx, double ly, double& ox, double& oy) {
        const double dx = lx - px, dy = ly - py;
        ox = x + dx * c - dy * s;
        oy = y + dx * s + dy * c;
    };
    double xs[4], ys[4];
    at(0.0, 0.0, xs[0], ys[0]);
    at(w, 0.0, xs[1], ys[1]);
    at(w, h, xs[2], ys[2]);
    at(0.0, h, xs[3], ys[3]);
    return push(tex, src, xs, ys, tint, opacity, depth);
}

bool SpriteBatch::add_quad(
    const Texture& tex, const TextureRect& src,
    const vector2d& top_left, const vector2d& top_right, const vector2d& bottom_right, const vector2d& bottom_left,
    const Color& tint, float depth, float opacity 
) {
    const double xs[4] = { top_left.x, top_right.x, bottom_right.x, bottom_left.x };
    const double ys[4] = { top_left.y, top_right.y, bottom_right.y, bottom_left.y };
    return push(tex, src, xs, ys, tint, opacity, depth);
}

void SpriteBatch::draw_order(std::vector<std::uint32_t>& out) const {
    const std::size_t n = m_items.size();
    out.resize(n);

    switch (m_sort) {
        case SpriteSortMode::Texture: {
            std::vector<std::uint32_t> counts(m_textures.size() + 1, 0);
            for (const SpriteBatchItem& it : m_items) ++counts[it.texture + 1];
            for (std::size_t t = 1; t < counts.size(); ++t) counts[t] += counts[t - 1];
            for (std::size_t i = 0; i < n; ++i) out[counts[m_items[i].texture]++] = static_cast<std::uint32_t>(i);
            return;
        }
        case SpriteSortMode::ZOrder:
            std::iota(out.begin(), out.end(), 0u);
            std::stable_sort(out.begin(), out.end(), [this](std::uint32_t a, std::uint32_t b) { return m_items[a].depth < m_items[b].depth; });
            return;
        case SpriteSortMode::ZOrderThenTexture:
            std::iota(out.begin(), out.end(), 0u);
            std::stable_sort(out.begin(), out.end(), [this](std::uint32_t a, std::uint32_t b) {
                const SpriteBatchItem& x = m_items[a];
                const SpriteBatchItem& y = m_items[b];
                return x.depth < y.depth || (x.depth == y.depth && x.texture < y.texture);
            });
            return;
        default:
            std::iota(out.begin(), out.end(), 0u);
            return;
    }
}

} // namespace graphics
} // namespace fizmo
