#ifndef FIZMO_SPRITE_BATCH_HPP
#define FIZMO_SPRITE_BATCH_HPP

#include "sprite.hpp"
#include "texture.hpp"
#include "color.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {

enum class SpriteSortMode : std::uint8_t {
    Deferred = 0,
    Texture,
    ZOrder,
    ZOrderThenTexture
};

struct SpriteBatchItem {
    float         x[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
    float         y[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
    float         u0      = 0.0f;
    float         v0      = 0.0f;
    float         u1      = 1.0f;
    float         v1      = 1.0f;
    TextureRect   source;
    Color         tint    = Color(255, 255, 255, 255);
    float         opacity = 1.0f;
    float         depth   = 0.0f;
    std::uint32_t texture = 0;
};

class SpriteBatch {
public:
    static constexpr std::uint32_t npos = std::numeric_limits<std::uint32_t>::max();

private:
    struct TextureKey {
        const images::BitmapImage* image;
        SampleFilter               filter;
        WrapMode                   wrap;

        bool operator==(const TextureKey& o) const noexcept { return image == o.image && filter == o.filter && wrap == o.wrap; }
    };

    struct KeyHash {
        std::size_t operator()(const TextureKey& k) const noexcept;
    };

    std::vector<Texture>                                    m_textures;
    std::vector<TextureKey>                                 m_keys;
    std::unordered_map<TextureKey, std::uint32_t, KeyHash>  m_lookup;
    std::vector<SpriteBatchItem>                            m_items;
    SpriteSortMode                                          m_sort = SpriteSortMode::Deferred;
    std::uint32_t                                           m_last = npos;

    static constexpr std::size_t kLinearLimit = 8;

    std::uint32_t texture_index(const Texture& tex);

    static void set_uv(SpriteBatchItem& item, const Texture& tex, const TextureRect& src) noexcept;

    bool push(const Texture& tex, const TextureRect& src, const double* xs, const double* ys, const Color& tint, float opacity, float depth);

public:
    SpriteBatch() = default;
    explicit SpriteBatch(SpriteSortMode mode) noexcept : m_sort(mode) {}

    void begin(SpriteSortMode mode = SpriteSortMode::Deferred) noexcept {
        clear();
        m_sort = mode;
    }

    void clear() noexcept {
        m_items.clear();
        m_textures.clear();
        m_keys.clear();
        m_lookup.clear();
        m_last = npos;
    }

    void reserve(std::size_t sprites) { m_items.reserve(sprites); }

    std::size_t size() const noexcept { return m_items.size(); }
    bool empty() const noexcept { return m_items.empty(); }
    std::size_t texture_count() const noexcept { return m_textures.size(); }

    SpriteSortMode sort_mode() const noexcept { return m_sort; }
    void set_sort_mode(SpriteSortMode mode) noexcept { m_sort = mode; }

    const std::vector<SpriteBatchItem>& items() const noexcept { return m_items; }
    std::vector<SpriteBatchItem>& items() noexcept { return m_items; }
    const std::vector<Texture>& textures() const noexcept { return m_textures; }

    bool add(const Sprite& sprite, double offset_x = 0.0, double offset_y = 0.0) {
        return add(sprite, sprite.tint(), offset_x, offset_y);
    }

    bool add(const Sprite& sprite, const Color& tint, double offset_x = 0.0, double offset_y = 0.0);

    std::size_t add(const Sprite* const* sprites, std::size_t count);

    std::size_t add(const std::vector<Sprite>& sprites) {
        std::size_t added = 0;
        for (const Sprite& s : sprites) if (add(s)) ++added;
        return added;
    }

    bool add(const Texture& tex, double x, double y, const Color& tint = Color(255, 255, 255, 255)) {
        return add(tex, tex.full_rect(), x, y, tex.width(), tex.height(), tint);
    }

    bool add(const Texture& tex, double x, double y, double w, double h, const Color& tint = Color(255, 255, 255, 255)) {
        return add(tex, tex.full_rect(), x, y, w, h, tint);
    }

    bool add(
        const Texture& tex, const TextureRect& src,
        double x, double y, double w, double h,
        const Color& tint = Color(255, 255, 255, 255),
        double rotation_degrees = 0.0,
        double pivot_x = 0.0, double pivot_y = 0.0,
        float depth = 0.0f, float opacity = 1.0f
    );

    bool add_quad(
        const Texture& tex, const TextureRect& src,
        const vector2d& top_left, const vector2d& top_right, const vector2d& bottom_right, const vector2d& bottom_left,
        const Color& tint = Color(255, 255, 255, 255), float depth = 0.0f, float opacity = 1.0f
    );

    void draw_order(std::vector<std::uint32_t>& out) const;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SPRITE_BATCH_HPP
