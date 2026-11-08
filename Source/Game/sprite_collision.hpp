#ifndef FIZMO_SPRITE_COLLISION_HPP
#define FIZMO_SPRITE_COLLISION_HPP

#include "../Graphics/sprite.hpp"
#include "../Physics/Rigid Body/physics_2d.hpp"
#include "../Physics/Rigid Body/quadtree.hpp"
#include <unordered_map>
#include <vector>
#include <functional>
#include <utility>

namespace fizmo {
namespace game {

enum class SpriteCollisionKind : std::uint8_t {
    None = 0,   
    AutoBox,    // AABB derived from display bounds
    AutoCircle, // bounding circle from display bounds
    Custom      // user-supplied Shape2D
};

struct SpriteCollisionDef {
    SpriteCollisionKind kind = SpriteCollisionKind::AutoBox;
    physics::Shape2D custom_shape;
    physics::Real padding = 0;
};

inline physics::AABB sprite_aabb(const graphics::Sprite& s, physics::Real pad) noexcept {
    auto b = s.bounds();
    const auto x = static_cast<physics::Real>(b.x), y = static_cast<physics::Real>(b.y);
    const auto w = static_cast<physics::Real>(b.w), h = static_cast<physics::Real>(b.h);
    return {
        { x - pad,     y - pad },
        { x + w + pad, y + h + pad }
    };
}

inline physics::AABB sprite_aabb(const graphics::Sprite& s) noexcept {
    return sprite_aabb(s, physics::Real(0));
}

inline std::pair<physics::RVec2, physics::Real> sprite_circle(const graphics::Sprite& s) noexcept {
    auto b = s.bounds();
    const auto x = static_cast<physics::Real>(b.x), y = static_cast<physics::Real>(b.y);
    const auto w = static_cast<physics::Real>(b.w), h = static_cast<physics::Real>(b.h);
    physics::RVec2 c{ x + w * physics::Real(0.5), y + h * physics::Real(0.5) };
    physics::Real r = std::max(w, h) * physics::Real(0.5);
    return { c, r };
}

inline std::pair<physics::RVec2, physics::Real> sprite_circle(const graphics::Sprite& s, physics::Real pad) noexcept {
    auto [c, r] = sprite_circle(s);
    return { c, r + pad };
}

inline physics::AABB sprite_collision_aabb(
    const graphics::Sprite& s,
    const SpriteCollisionDef& def
) noexcept {
    switch (def.kind) {
        case SpriteCollisionKind::None:
            return {};
        case SpriteCollisionKind::AutoBox:
            return sprite_aabb(s, def.padding);
        case SpriteCollisionKind::AutoCircle: {
            auto [c, r] = sprite_circle(s, def.padding);
            return { c - physics::RVec2(r, r), c + physics::RVec2(r, r) };
        }
        case SpriteCollisionKind::Custom:
            return def.custom_shape.compute_aabb(physics::Transform2D({ static_cast<physics::Real>(s.x()), static_cast<physics::Real>(s.y()) }, physics::Real(0)));
    }
    return {};
}

inline bool collides(const graphics::Sprite& a, const graphics::Sprite& b) noexcept {
    return sprite_aabb(a).overlaps(sprite_aabb(b));
}

inline physics::OverlapResult collides(
    const graphics::Sprite& a, const SpriteCollisionDef& da,
    const graphics::Sprite& b, const SpriteCollisionDef& db
) noexcept {
    if (da.kind == SpriteCollisionKind::None || db.kind == SpriteCollisionKind::None) return {};
    physics::AABB aa = sprite_collision_aabb(a, da);
    physics::AABB ab = sprite_collision_aabb(b, db);
    if (!aa.overlaps(ab)) return {};
    bool a_box = (da.kind == SpriteCollisionKind::AutoBox || da.kind == SpriteCollisionKind::Custom);
    bool b_box = (db.kind == SpriteCollisionKind::AutoBox || db.kind == SpriteCollisionKind::Custom);
    if (a_box && b_box) return physics::overlap_aabb_mtv(aa, ab);

    if (!a_box && !b_box) {
        auto [ca, ra] = sprite_circle(a, da.padding);
        auto [cb, rb] = sprite_circle(b, db.padding);
        return physics::overlap_circles_mtv(ca, ra, cb, rb);
    }

    if (a_box) {
        auto [cb, rb] = sprite_circle(b, db.padding);
        return physics::overlap_aabb_circle_mtv(aa, cb, rb);
    } else {
        auto [ca, ra] = sprite_circle(a, da.padding);
        auto r = physics::overlap_aabb_circle_mtv(ab, ca, ra);
        r.normal = r.normal * physics::Real(-1); 
        return r;
    }
}

class SpriteCollisionWorld {
public:
    struct Registration {
        graphics::Sprite*             sprite;
        SpriteCollisionDef  def;
    };

    using PairCallback = std::function<
        void(
            graphics::Sprite& a, const SpriteCollisionDef& da,
            graphics::Sprite& b, const SpriteCollisionDef& db,
            const physics::OverlapResult& result
        )
    >;

    explicit SpriteCollisionWorld(
        const physics::AABB& world_bounds,
        std::size_t          max_per_node = 8,
        std::size_t          max_depth    = 6,
        physics::Real               looseness    = physics::Quadtree<graphics::Sprite>::RECOMMENDED_LOOSENESS
    ) noexcept : m_tree(world_bounds, max_per_node, max_depth, looseness) {}

    std::size_t add(graphics::Sprite& s, SpriteCollisionDef def = {}) noexcept {
        std::size_t id = m_regs.size();
        m_regs.push_back({ &s, std::move(def) });
        return id;
    }

    Registration&       get(std::size_t id)       noexcept { return m_regs[id]; }
    const Registration& get(std::size_t id) const noexcept { return m_regs[id]; }

    void remove(std::size_t id) noexcept { m_regs[id].sprite = nullptr;  }

    void clear() noexcept {
        m_regs.clear();
        m_tree.clear();
    }

    void update(PairCallback on_pair) {
        m_tree.clear();
        m_defs.clear();

        for (auto& reg : m_regs) {
            if (!reg.sprite) continue;
            if (!reg.sprite->visible()) continue;
            if (reg.def.kind == SpriteCollisionKind::None) continue;
            if (!m_defs.emplace(reg.sprite, &reg.def).second) continue;
            physics::AABB aabb = sprite_collision_aabb(*reg.sprite, reg.def);
            m_tree.insert(reg.sprite, aabb);
        }

        m_tree.find_pairs([&](graphics::Sprite* a, graphics::Sprite* b) {
            auto ia = m_defs.find(a);
            auto ib = m_defs.find(b);
            if (ia == m_defs.end() || ib == m_defs.end()) return;
            const SpriteCollisionDef* da = ia->second;
            const SpriteCollisionDef* db = ib->second;
            physics::OverlapResult result = collides(*a, *da, *b, *db);
            if (result.hit && on_pair) on_pair(*a, *da, *b, *db, result);
        });
    }

    std::vector<graphics::Sprite*> query_point(physics::Real px, physics::Real py) const {
        physics::AABB point_box{
            { px, py }, { px, py }
        };
        return m_tree.query(point_box);
    }

    std::vector<graphics::Sprite*> query_region(const physics::AABB& region) const {
        return m_tree.query(region);
    }

    void set_world_bounds(const physics::AABB& bounds) noexcept {
        m_tree = physics::Quadtree<graphics::Sprite>(bounds, m_tree.max_per_node(), m_tree.max_depth(), m_tree.looseness());
    }

    std::size_t registration_count() const noexcept { return m_regs.size(); }
    const physics::Quadtree<graphics::Sprite>& tree() const noexcept { return m_tree; }

private:
    std::vector<Registration>                                              m_regs;
    physics::Quadtree<graphics::Sprite>                                    m_tree;
    std::unordered_map<const graphics::Sprite*, const SpriteCollisionDef*> m_defs;
};

} // namespace game
} // namespace fizmo

#endif // FIZMO_SPRITE_COLLISION_HPP