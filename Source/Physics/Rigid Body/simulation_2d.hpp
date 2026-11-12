#ifndef FIZMO_SIMULATION_2D_HPP
#define FIZMO_SIMULATION_2D_HPP

#include "quadtree.hpp"
#include "dynamic_tree.hpp"
#include "gjk_2d.hpp"

#include <functional>
#include <limits>
#include <optional>

namespace fizmo {
namespace physics {

struct ContactPoint {
    RVec2 position{};          // world-space point midway between the two surfaces (m)
    Real separation  = 0.0_r;     // negative = penetrating (m)
    Real normal_impulse  = 0.0_r; // accumulated normal impulse (N*s), filled in after the velocity solve
    Real tangent_impulse = 0.0_r; // accumulated tangent impulse (N*s), filled in after the velocity solve
    RVec2 local_anchor_a{};    // point on A's surface, in A's body frame (m)
    RVec2 local_anchor_b{};    // point on B's surface, in B's body frame (m)
};

struct ContactManifold {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    RVec2 normal{}; // unit vector from A toward B
    ContactPoint points[2];
    int point_count = 0;
    Real friction    = 0.0_r;
    Real restitution = 0.0_r;
    Real restitution_threshold = 1.0_r; // m/s
    std::size_t collider_a = 0; // index into body_a->colliders()
    std::size_t collider_b = 0; // index into body_b->colliders()
};

namespace narrowphase {

void project_polygon(const PolygonShape& poly, const Transform2D& xf, const RVec2& axis, Real& lo, Real& hi);

inline void project_circle(const CircleShape& circ, const Transform2D& xf, const RVec2& axis, Real& lo, Real& hi) {
    RVec2 c = xf.apply(circ.center);
    Real p = fast::dot(axis, c);
    lo = p - circ.radius;
    hi = p + circ.radius;
}

void project_capsule(const CapsuleShape& cap, const Transform2D& xf, const RVec2& axis, Real& lo, Real& hi);

RVec2 closest_point_on_segment(const RVec2& p, const RVec2& a, const RVec2& b);

bool collide_circle_circle(
    const CircleShape& ca, const Transform2D& xa,
    const CircleShape& cb, const Transform2D& xb,
    ContactManifold& m
);

bool collide_circle_polygon(
    const CircleShape& circ, const Transform2D& xc,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
);

namespace detail_sat {

struct RoundPolygon {
    std::array<RVec2, MAX_POLYGON_VERTICES> vertices{};
    std::array<RVec2, MAX_POLYGON_VERTICES> normals{};
    std::size_t count = 0;
    Real radius = 0.0_r;
};

RoundPolygon make_round_polygon(const PolygonShape& p, const Transform2D& xf);

RoundPolygon make_round_segment(const RVec2& a, const RVec2& b, Real radius);

struct AxisResult {
    Real separation;
    std::size_t edge_index;
};

AxisResult find_max_separation(const RoundPolygon& a, const RoundPolygon& b);

struct SegmentDistance {
    RVec2 closest1{};
    RVec2 closest2{};
    Real fraction1 = 0.0_r; 
    Real fraction2 = 0.0_r;
    Real distance_squared = 0.0_r;
};

SegmentDistance segment_distance(const RVec2& p1, const RVec2& q1, const RVec2& p2, const RVec2& q2);

bool collide_round_polygons(const RoundPolygon& a, const RoundPolygon& b, ContactManifold& m);

} // namespace detail_sat

bool collide_polygon_polygon(
    const PolygonShape& pa, const Transform2D& xa,
    const PolygonShape& pb, const Transform2D& xb,
    ContactManifold& m
);

bool collide_circle_capsule(
    const CircleShape& circ, const Transform2D& xc,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
);

bool collide_polygon_capsule(
    const PolygonShape& poly, const Transform2D& xp,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
);

bool collide_capsule_capsule(
    const CapsuleShape& ca, const Transform2D& xa,
    const CapsuleShape& cb, const Transform2D& xb,
    ContactManifold& m
);

bool collide_edge_circle(
    const EdgeShape& edge, const Transform2D& xe,
    const CircleShape& circ, const Transform2D& xc,
    ContactManifold& m, bool flip
);

bool collide_edge_polygon(
    const EdgeShape& edge, const Transform2D& xe,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
);

bool collide_edge_capsule(
    const EdgeShape& edge, const Transform2D& xe,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
);

} // namespace narrowphase

bool collide_shapes(
    const Shape2D& sa, const Transform2D& xa,
    const Shape2D& sb, const Transform2D& xb,
    ContactManifold& manifold
);

struct VelocityConstraintPoint {
    RVec2 rA{}; // contact − center_A (m)
    RVec2 rB{}; // contact − center_B (m)
    Real normal_mass  = 0.0_r;
    Real tangent_mass = 0.0_r;
    Real velocity_bias = 0.0_r;
    Real normal_impulse  = 0.0_r; 
    Real tangent_impulse = 0.0_r;
};

struct VelocityConstraint {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    RVec2 normal{};
    Real friction = 0.0_r;
    Real restitution = 0.0_r;
    Real restitution_threshold = 1.0_r;
    VelocityConstraintPoint points[2];
    int point_count = 0;
    std::size_t manifold_index = 0;
};

struct ContactEvent {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    RVec2 normal{};
    RVec2 point{};
    Real impulse = 0.0_r; 
    bool sensor = false;  
};

struct Simulation2DConfig {
    RVec2 gravity           = to_real(default_axes2d().gravity(constants::gravity())); // m/s2
    Real   fixed_dt          = 1.0_r / 60.0_r;     // s
    int      velocity_iters    = 8;
    int      position_iters    = 3;
    Real   baumgarte_factor  = 0.2_r;            // dimensionless
    Real   slop              = 0.005_r;          // m (allowed penetration)
    Real   max_correction    = 0.2_r;            // m (largest position correction per contact per iteration)
    Real   broadphase_margin = 0.1_r;            // m (AABB fattening)
    Real   warm_start_match_distance = 0.1_r;    // m (how far a contact may drift and still reuse last step's impulse)
    bool     warm_starting     = true;
    bool     allow_sleeping    = true;
    Real   world_half_extent = 500.0_r;          // m
    CombineMode friction_combine = CombineMode::GeometricMean;
    CombineMode restitution_combine = CombineMode::Maximum;
    int      substeps          = 1;
    bool     dynamic_tree      = true;
    bool     continuous        = true;
    bool     continuous_all    = false;
    Real   max_continuous_distance = 50.0_r;     // m
    bool     event_callbacks   = true;
};

template <>
struct BoxTraits<AABB> {
    using Vector = RVec2;
    static bool overlaps(const AABB& a, const AABB& b) noexcept { return a.overlaps(b); }
    static bool contains(const AABB& outer, const AABB& inner) noexcept { return outer.contains(inner); }
    static AABB merge(const AABB& a, const AABB& b) noexcept { return AABB::merge(a, b); }
    static AABB fatten(const AABB& a, Real m) noexcept { return a.fatten(m); }
    static Real cost(const AABB& a) noexcept { return std::min(a.perimeter(), 1e12_r); }
    static AABB translate(const AABB& a, const Vector& v) noexcept { return { a.min + v, a.max + v }; }
};

struct QueryFilter2D {
    std::uint64_t layers = std::numeric_limits<std::uint64_t>::max();
    bool include_sensors = false;
    bool include_static = true;
    bool include_dynamic = true;
    bool include_kinematic = true;
    const RigidBody2D* ignore = nullptr;
    std::function<bool(const RigidBody2D&, const Collider2D&)> predicate;
};

struct RayHit2D {
    RigidBody2D* body = nullptr;
    std::size_t collider = 0;
    Real distance = 0.0_r;
    Real fraction = 0.0_r;
    RVec2 point{};
    RVec2 normal{};
};

struct ShapeHit2D {
    RigidBody2D* body = nullptr;
    std::size_t collider = 0;
    Real fraction = 1.0_r;
    RVec2 point{};
    RVec2 normal{};
};

struct Simulation2DStats {
    std::size_t bodies = 0;
    std::size_t proxies = 0;
    std::size_t pairs = 0;
    std::size_t manifolds = 0;
    std::size_t layer_rejects = 0;
    std::size_t continuous_hits = 0;
    int tree_height = 0;
};

class Simulation2D {
public:
    using Config = Simulation2DConfig;

    explicit Simulation2D(const Config& cfg = Config())
;

    Simulation2D(const Simulation2D&) = delete;
    Simulation2D& operator=(const Simulation2D&) = delete;

    RigidBody2D& add_body(PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
        m_bodies.push_back(std::make_unique<RigidBody2D>(preset));
        m_bodies.back()->m_handle = m_handles.add(m_bodies.back().get());
        return *m_bodies.back();
    }

    BodyHandle handle_of(const RigidBody2D& body) const noexcept { return body.handle(); }
    RigidBody2D* get(BodyHandle h) const noexcept { return m_handles.get(h); }
    bool alive(BodyHandle h) const noexcept { return m_handles.alive(h); }

    RigidBody2D& add_body(const RVec2& pos, PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
        auto& b = add_body(preset);
        b.set_position(pos);
        return b;
    }

    void remove_body(std::size_t index) {
        if (index < m_bodies.size()) remove_body(*m_bodies[index]);
    }

    void remove_body(RigidBody2D& body);

    std::size_t body_count() const noexcept { return m_bodies.size(); }
          RigidBody2D& body(std::size_t i)       { return *m_bodies[i]; }
    const RigidBody2D& body(std::size_t i) const { return *m_bodies[i]; }

    BodyRange<RigidBody2D>       bodies()       noexcept { return BodyRange<RigidBody2D>(&m_bodies); }
    BodyRange<const RigidBody2D> bodies() const noexcept { return BodyRange<const RigidBody2D>(&m_bodies); }

          Config& config()       noexcept { return m_cfg; }
    const Config& config() const noexcept { return m_cfg; }

    void set_axes(const Axes2D& axes) noexcept { m_cfg.gravity = to_real(axes.gravity(fast::length(m_cfg.gravity))); }

    void set_gravity(const RVec2& g) noexcept { m_cfg.gravity = g; }
    RVec2 gravity() const noexcept { return m_cfg.gravity; }

    void on_contact_begin(std::function<void(const ContactEvent&)> cb) { m_on_begin   = std::move(cb); }
    void on_contact_end(std::function<void(const ContactEvent&)> cb)   { m_on_end     = std::move(cb); }
    void on_pre_solve(std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> cb)    { m_pre_solve  = std::move(cb); }
    void on_post_solve(std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> cb)  { m_post_solve = std::move(cb); }
    using CombineCallback = std::function<Real(const Collider2D& a, const Collider2D& b)>;
    void set_friction_combiner(CombineCallback cb) { m_friction_combiner = std::move(cb); }
    void set_restitution_combiner(CombineCallback cb) { m_restitution_combiner = std::move(cb); }
    Real combined_friction(const Collider2D& a, const Collider2D& b) const;
    Real combined_restitution(const Collider2D& a, const Collider2D& b) const;

    const std::vector<ContactManifold>& contacts() const noexcept { return m_manifolds; }

    void step(Real dt);

    void step() { step(m_cfg.fixed_dt); }

    std::vector<RigidBody2D*> query_aabb(const AABB& region);

    RigidBody2D* point_query(const RVec2& point);

    std::vector<RigidBody2D*> query_aabb(const AABB& region, const QueryFilter2D& filter);
    std::optional<RayHit2D> raycast(const RVec2& origin, const RVec2& direction, Real max_distance, const QueryFilter2D& filter = {});
    std::vector<RayHit2D> raycast_all(const RVec2& origin, const RVec2& direction, Real max_distance, const QueryFilter2D& filter = {});
    std::optional<ShapeHit2D> shape_cast(const Shape2D& shape, const Transform2D& start, const RVec2& translation, const QueryFilter2D& filter = {});
    std::vector<RigidBody2D*> overlap(const Shape2D& shape, const Transform2D& xf, const QueryFilter2D& filter = {});
    std::vector<RigidBody2D*> overlap_circle(const RVec2& center, Real radius, const QueryFilter2D& filter = {});

    const std::vector<ContactEvent>& begin_events() const noexcept { return m_last_begin; }
    const std::vector<ContactEvent>& end_events() const noexcept { return m_last_end; }
    void clear_events() noexcept {
        m_last_begin.clear();
        m_last_end.clear();
    }

    void shift_origin(const RVec2& offset);
    bool can_collide(const RigidBody2D& a, const RigidBody2D& b) const noexcept;
    const Simulation2DStats& stats() const noexcept { return m_stats; }

private:
    using Key     = detail::ContactKey<RigidBody2D>;
    using KeyHash = detail::ContactKeyHash<RigidBody2D>;

    struct CachedContact {
        RigidBody2D* body_a = nullptr;
        RigidBody2D* body_b = nullptr;
        bool sensor = false;
        bool events = false;
        RVec2 normal{};
        RVec2 point{};
        int point_count = 0;
        std::array<RVec2, 2> local_anchor_a{};
        std::array<Real, 2> normal_impulse{};
        std::array<Real, 2> tangent_impulse{};
    };

    using ContactCache = std::unordered_map<Key, CachedContact, KeyHash>;

    static Key key_of(const ContactManifold& mf) noexcept { return { mf.body_a, mf.body_b, mf.collider_a, mf.collider_b }; }

    static bool is_enabled(const RigidBody2D& b) noexcept { return has_flag(b.flags(), PhysicsFlags::Enabled); }
    static bool is_dynamic(const RigidBody2D& b) noexcept { return b.inv_mass() > 0.0_r; }

    static bool is_active(const RigidBody2D& b) noexcept;

    static bool responds(const RigidBody2D& a, const RigidBody2D& b) noexcept;

    static bool wants_events(const RigidBody2D& a, const RigidBody2D& b, bool sensor) noexcept;

    void synchronize_all_colliders() {
        for (auto& b : m_bodies) {
            if (is_enabled(*b)) b->synchronize_colliders();
        }
    }

    void step_substep(Real dt);

    void broadphase();

    void narrowphase();

    void generate_contacts(RigidBody2D* a, RigidBody2D* b);

    void build_islands_and_wake();

    void sleep_islands(Real dt);

    void build_velocity_constraints();

    void warm_start();

    void solve_velocity_constraints();

    static bool solve_block(VelocityConstraint& vc);

    void store_impulses();

    bool solve_position_constraints();

    void update_contact_cache();

    void dispatch_events();

    void remove_body_now(RigidBody2D* body);

    void flush_pending_removals();

    void cleanup_bodies();

    void tree_broadphase();
    void sync_tree(bool for_query);
    bool sweeps(const RigidBody2D& b) const noexcept;
    void solve_continuous();
    bool accepts(const RigidBody2D& b, const Collider2D& c, const QueryFilter2D& f) const;
    void drop_proxy(RigidBody2D* body);

private:
    Config m_cfg;

    std::vector<std::unique_ptr<RigidBody2D>> m_bodies;
    detail::HandleTable<RigidBody2D> m_handles;

    Quadtree<RigidBody2D>     m_tree;
    std::vector<RigidBody2D*> m_body_ptrs;
    std::vector<AABB>         m_body_aabbs;
    std::vector<std::pair<RigidBody2D*, RigidBody2D*>> m_pairs;

    std::vector<ContactManifold>    m_manifolds;
    std::vector<VelocityConstraint> m_constraints;

    ContactCache     m_contact_cache; 
    ContactCache     m_touching;      
    std::vector<Key> m_carried;       

    detail::UnionFind m_islands;

    std::vector<ContactEvent> m_begin_events;
    std::vector<ContactEvent> m_end_events;
    std::vector<ContactEvent> m_last_begin;
    std::vector<ContactEvent> m_last_end;
    DynamicTree<AABB, RigidBody2D*> m_dtree;
    std::unordered_map<const RigidBody2D*, std::int32_t> m_proxies;
    std::uint64_t m_query_epoch = 0;
    std::vector<std::pair<std::int32_t, std::int32_t>> m_tree_pairs;
    std::unordered_map<std::uint64_t, std::size_t> m_tree_pair_index;
    Simulation2DStats m_stats;
    std::vector<RigidBody2D*> m_pending_removals;
    bool m_locked = false;

    std::function<void(const ContactEvent&)> m_on_begin;
    std::function<void(const ContactEvent&)> m_on_end;
    std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> m_pre_solve;
    std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> m_post_solve;
    CombineCallback m_friction_combiner;
    CombineCallback m_restitution_combiner;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_SIMULATION_2D_HPP