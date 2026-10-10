#ifndef FIZMO_SIMULATION_2D_HPP
#define FIZMO_SIMULATION_2D_HPP

#include "quadtree.hpp"

namespace fizmo {
namespace physics {

struct ContactPoint {
    vector2d position{};          // world-space point midway between the two surfaces (m)
    double separation  = 0.0;     // negative = penetrating (m)
    double normal_impulse  = 0.0; // accumulated normal impulse (N*s), filled in after the velocity solve
    double tangent_impulse = 0.0; // accumulated tangent impulse (N*s), filled in after the velocity solve
    vector2d local_anchor_a{};    // point on A's surface, in A's body frame (m)
    vector2d local_anchor_b{};    // point on B's surface, in B's body frame (m)
};

struct ContactManifold {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    vector2d normal{}; // unit vector from A toward B
    ContactPoint points[2];
    int point_count = 0;
    double friction    = 0.0;
    double restitution = 0.0;
    double restitution_threshold = 1.0; // m/s
    std::size_t collider_a = 0; // index into body_a->colliders()
    std::size_t collider_b = 0; // index into body_b->colliders()
};

namespace narrowphase {

void project_polygon(const PolygonShape& poly, const Transform2D& xf, const vector2d& axis, double& lo, double& hi);

inline void project_circle(const CircleShape& circ, const Transform2D& xf, const vector2d& axis, double& lo, double& hi) {
    vector2d c = xf.apply(circ.center);
    double p = axis.dot(c);
    lo = p - circ.radius;
    hi = p + circ.radius;
}

void project_capsule(const CapsuleShape& cap, const Transform2D& xf, const vector2d& axis, double& lo, double& hi);

vector2d closest_point_on_segment(const vector2d& p, const vector2d& a, const vector2d& b);

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
    std::array<vector2d, MAX_POLYGON_VERTICES> vertices{};
    std::array<vector2d, MAX_POLYGON_VERTICES> normals{};
    std::size_t count = 0;
    double radius = 0.0;
};

RoundPolygon make_round_polygon(const PolygonShape& p, const Transform2D& xf);

RoundPolygon make_round_segment(const vector2d& a, const vector2d& b, double radius);

struct AxisResult {
    double separation;
    std::size_t edge_index;
};

AxisResult find_max_separation(const RoundPolygon& a, const RoundPolygon& b);

struct SegmentDistance {
    vector2d closest1{};
    vector2d closest2{};
    double fraction1 = 0.0; 
    double fraction2 = 0.0;
    double distance_squared = 0.0;
};

SegmentDistance segment_distance(const vector2d& p1, const vector2d& q1, const vector2d& p2, const vector2d& q2);

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
    vector2d rA{}; // contact − center_A (m)
    vector2d rB{}; // contact − center_B (m)
    double normal_mass  = 0.0;
    double tangent_mass = 0.0;
    double velocity_bias = 0.0;
    double normal_impulse  = 0.0; 
    double tangent_impulse = 0.0;
};

struct VelocityConstraint {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    vector2d normal{};
    double friction = 0.0;
    double restitution = 0.0;
    double restitution_threshold = 1.0;
    VelocityConstraintPoint points[2];
    int point_count = 0;
    std::size_t manifold_index = 0;
};

struct ContactEvent {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    vector2d normal{};
    vector2d point{};
    double impulse = 0.0; 
    bool sensor = false;  
};

struct Simulation2DConfig {
    vector2d gravity           = {0.0, -constants::gravity()}; // m/s2
    double   fixed_dt          = 1.0 / 60.0;     // s
    int      velocity_iters    = 8;
    int      position_iters    = 3;
    double   baumgarte_factor  = 0.2;            // dimensionless
    double   slop              = 0.005;          // m (allowed penetration)
    double   max_correction    = 0.2;            // m (largest position correction per contact per iteration)
    double   broadphase_margin = 0.1;            // m (AABB fattening)
    double   warm_start_match_distance = 0.1;    // m (how far a contact may drift and still reuse last step's impulse)
    bool     warm_starting     = true;
    bool     allow_sleeping    = true;
    double   world_half_extent = 500.0;          // m
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
        return *m_bodies.back();
    }

    RigidBody2D& add_body(const vector2d& pos, PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
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

    void set_gravity(const vector2d& g) noexcept { m_cfg.gravity = g; }
    vector2d gravity() const noexcept { return m_cfg.gravity; }

    void on_contact_begin(std::function<void(const ContactEvent&)> cb) { m_on_begin   = std::move(cb); }
    void on_contact_end(std::function<void(const ContactEvent&)> cb)   { m_on_end     = std::move(cb); }
    void on_pre_solve(std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> cb)    { m_pre_solve  = std::move(cb); }
    void on_post_solve(std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> cb)  { m_post_solve = std::move(cb); }

    const std::vector<ContactManifold>& contacts() const noexcept { return m_manifolds; }

    void step(double dt);

    void step() { step(m_cfg.fixed_dt); }

    std::vector<RigidBody2D*> query_aabb(const AABB& region);

    RigidBody2D* point_query(const vector2d& point);

private:
    using Key     = detail::ContactKey<RigidBody2D>;
    using KeyHash = detail::ContactKeyHash<RigidBody2D>;

    struct CachedContact {
        RigidBody2D* body_a = nullptr;
        RigidBody2D* body_b = nullptr;
        bool sensor = false;
        bool events = false;
        vector2d normal{};
        vector2d point{};
        int point_count = 0;
        std::array<vector2d, 2> local_anchor_a{};
        std::array<double, 2> normal_impulse{};
        std::array<double, 2> tangent_impulse{};
    };

    using ContactCache = std::unordered_map<Key, CachedContact, KeyHash>;

    static Key key_of(const ContactManifold& mf) noexcept { return { mf.body_a, mf.body_b, mf.collider_a, mf.collider_b }; }

    static bool is_enabled(const RigidBody2D& b) noexcept { return has_flag(b.flags(), PhysicsFlags::Enabled); }
    static bool is_dynamic(const RigidBody2D& b) noexcept { return b.inv_mass() > 0.0; }

    static bool is_active(const RigidBody2D& b) noexcept;

    static bool responds(const RigidBody2D& a, const RigidBody2D& b) noexcept;

    static bool wants_events(const RigidBody2D& a, const RigidBody2D& b, bool sensor) noexcept;

    void synchronize_all_colliders() {
        for (auto& b : m_bodies) {
            if (is_enabled(*b)) b->synchronize_colliders();
        }
    }

    void broadphase();

    void narrowphase();

    void generate_contacts(RigidBody2D* a, RigidBody2D* b);

    void build_islands_and_wake();

    void sleep_islands(double dt);

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

private:
    Config m_cfg;

    std::vector<std::unique_ptr<RigidBody2D>> m_bodies;

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
    std::unordered_map<const RigidBody2D*, std::size_t> m_body_index;

    std::vector<ContactEvent> m_begin_events;
    std::vector<ContactEvent> m_end_events;
    std::vector<RigidBody2D*> m_pending_removals;
    bool m_locked = false;

    std::function<void(const ContactEvent&)> m_on_begin;
    std::function<void(const ContactEvent&)> m_on_end;
    std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> m_pre_solve;
    std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> m_post_solve;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_SIMULATION_2D_HPP