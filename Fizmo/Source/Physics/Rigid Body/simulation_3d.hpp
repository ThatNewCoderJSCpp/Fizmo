#ifndef FIZMO_SIMULATION_3D_HPP
#define FIZMO_SIMULATION_3D_HPP

#include "octree.hpp"

namespace fizmo {
namespace physics {

struct ContactPoint3D {
    vector3d position{};           // world-space point midway between the two surfaces (m)
    double separation = 0.0;       // negative = penetrating (m)
    double normal_impulse = 0.0;   // accumulated normal impulse (N*s), filled in after the velocity solve
    vector3d friction_impulse{};   // accumulated friction impulse, world space (N*s), filled in after the velocity solve
    vector3d local_anchor_a{};     // point on A's surface, in A's body frame (m)
    vector3d local_anchor_b{};     // point on B's surface, in B's body frame (m)
};

struct ContactManifold3D {
    static constexpr int MAX_POINTS = 4;

    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    vector3d normal{}; // unit vector from A toward B
    ContactPoint3D points[MAX_POINTS];
    int point_count = 0;
    double friction    = 0.0;
    double restitution = 0.0;
    double restitution_threshold = 1.0; // m/s
    std::size_t collider_a = 0; // index into body_a->colliders()
    std::size_t collider_b = 0; // index into body_b->colliders()
};

namespace narrowphase3d {

struct Candidate {
    vector3d position;
    double   separation;
};

 vector3d closest_point_on_segment(const vector3d& p, const vector3d& a, const vector3d& b);

struct SegmentPair {
    vector3d closest1{};
    vector3d closest2{};
    double fraction1 = 0.0;
    double fraction2 = 0.0;
    double distance_squared = 0.0;
};

 SegmentPair closest_segment_points(const vector3d& p1, const vector3d& q1, const vector3d& p2, const vector3d& q2);

 void reduce_contacts(ContactManifold3D& m, const std::vector<Candidate>& c, const vector3d& normal);

 bool point_in_face(const HullShape& h, const HullFace& f, const vector3d& q);

 vector3d closest_point_on_face(const HullShape& h, const HullFace& f, const vector3d& p);

struct PointHullResult {
    bool     inside = false;
    vector3d point{};        // closest surface point (m)
    vector3d normal{};       // outward from the hull toward p
    double   distance = 0.0; // signed: negative when p is inside
};

 PointHullResult closest_point_on_hull(const HullShape& h, const vector3d& p);

 bool segment_intersects_hull(const HullShape& h, const vector3d& p1, const vector3d& p2);

 bool clip_segment_to_face(const HullShape& h, const HullFace& f, vector3d& a, vector3d& b);

 int support_edge(const HullShape& h, const vector3d& dir, const vector3d& axis, double sign);

 void project_hull(const HullShape& h, const vector3d& axis, double& lo, double& hi);

 void project_points(const std::vector<vector3d>& pts, const vector3d& axis, double& lo, double& hi);

 bool collide_sphere_sphere(
    const SphereShape& sa, const Transform3D& xa,
    const SphereShape& sb, const Transform3D& xb,
    ContactManifold3D& m, double margin = 0.0
);

 bool collide_sphere_point(const vector3d& c, double ra, const vector3d& p, double rb, ContactManifold3D& m, double margin);

 bool collide_sphere_capsule(
    const SphereShape& s, const Transform3D& xs,
    const CapsuleShape3D& k, const Transform3D& xk,
    ContactManifold3D& m, double margin = 0.0
);

 bool collide_sphere_hull(
    const SphereShape& s, const Transform3D& xs,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, double margin = 0.0
);

inline void plane_in_world(const PlaneShape& p, const Transform3D& xp, vector3d& n, double& d) {
    n = xp.rotate(p.normal);
    d = p.offset + vec3::dot(n, xp.position);
}

 bool collide_sphere_plane(
    const SphereShape& s, const Transform3D& xs,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, double margin = 0.0
);

 bool collide_capsule_capsule(
    const CapsuleShape3D& ka, const Transform3D& xa,
    const CapsuleShape3D& kb, const Transform3D& xb,
    ContactManifold3D& m, double margin = 0.0
);

 bool collide_capsule_plane(
    const CapsuleShape3D& k, const Transform3D& xk,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, double margin = 0.0
);

 bool collide_hull_plane(
    const HullShape& h, const Transform3D& xh,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, double margin = 0.0
);

 bool collide_capsule_hull(
    const CapsuleShape3D& k, const Transform3D& xk,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, double margin = 0.0
);

namespace detail_sat3d {

struct FaceQuery {
    double separation = -std::numeric_limits<double>::max();
    int    index = -1;
};

struct EdgeQuery {
    double   separation = -std::numeric_limits<double>::max();
    int      dir_a = -1;
    int      dir_b = -1;
    vector3d axis{}; 
};

 FaceQuery query_faces(const HullShape& a, const std::vector<vector3d>& other);

 EdgeQuery query_edges(const HullShape& a, const std::vector<vector3d>& b_vertices, const std::vector<vector3d>& b_dirs);

 void clip_polygon(std::vector<vector3d>& poly, const vector3d& n, double d, std::vector<vector3d>& scratch);

 void face_contact(
    const HullShape& r, const Transform3D& xr, int rf,
    const HullShape& inc, const Transform3D& xi,
    std::vector<Candidate>& out, vector3d& normal, double margin
);

} // namespace detail_sat3d

 bool collide_hull_hull(
    const HullShape& a, const Transform3D& xa,
    const HullShape& b, const Transform3D& xb,
    ContactManifold3D& m, double margin = 0.0
);

} // namespace narrowphase3d

 bool collide_shapes(
    const Shape3D& sa, const Transform3D& xa,
    const Shape3D& sb, const Transform3D& xb,
    ContactManifold3D& manifold, double margin = 0.0
);

struct VelocityConstraintPoint3D {
    vector3d rA{}; // contact − center_A (m)
    vector3d rB{}; // contact − center_B (m)
    double normal_mass = 0.0;
    double tangent_mass[2] = { 0.0, 0.0 };
    double velocity_bias = 0.0;
    double normal_impulse = 0.0;                
    double tangent_impulse[2] = { 0.0, 0.0 };
};

struct VelocityConstraint3D {
    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    vector3d normal{};
    vector3d tangent1{};
    vector3d tangent2{};
    double friction = 0.0;
    double restitution = 0.0;
    double restitution_threshold = 1.0;
    VelocityConstraintPoint3D points[ContactManifold3D::MAX_POINTS];
    int point_count = 0;
    std::size_t manifold_index = 0;
    double normal_block[ContactManifold3D::MAX_POINTS][ContactManifold3D::MAX_POINTS] = {}; 
};

struct ContactEvent3D {
    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    vector3d normal{};
    vector3d point{};
    double impulse = 0.0; 
    bool sensor = false;  
};

struct Simulation3DConfig {
    vector3d gravity           = {0.0, -constants::gravity(), 0.0}; // m/s2
    double   fixed_dt          = 1.0 / 60.0;        // s
    int      velocity_iters    = 10;
    int      position_iters    = 3;
    double   baumgarte_factor  = 0.2;               // dimensionless
    double   slop              = 0.002;             // m (allowed penetration; a resting box can lean by up to 2*slop across its diagonal)
    double   max_correction    = 0.2;               // m (largest position correction per contact per iteration)
    double   broadphase_margin = 0.1;               // m (AABB fattening)
    double   speculative_distance = 0.02;           // m (points this far apart are kept so contacts persist; must be < broadphase_margin)
    double   warm_start_match_distance = 0.1;       // m (how far a contact may drift and still reuse last step's impulse)
    bool     warm_starting     = true;
    bool     allow_sleeping    = true;
    bool     gyroscopic        = true;              // integrate w x Iw for non-spherical bodies
    double   world_half_extent = 500.0;             // m
};

class Simulation3D {
public:
    using Config = Simulation3DConfig;

    explicit Simulation3D(const Config& cfg = Config())
        : m_cfg(cfg),
          m_tree(world_bounds_of(cfg))
    {}

    Simulation3D(const Simulation3D&) = delete;
    Simulation3D& operator=(const Simulation3D&) = delete;

    RigidBody3D& add_body(PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
        m_bodies.push_back(std::make_unique<RigidBody3D>(preset));
        return *m_bodies.back();
    }

    RigidBody3D& add_body(const vector3d& pos, PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
        auto& b = add_body(preset);
        b.set_position(pos);
        return b;
    }

    void remove_body(std::size_t index) {
        if (index < m_bodies.size()) remove_body(*m_bodies[index]);
    }

    void remove_body(RigidBody3D& body);

    std::size_t body_count() const noexcept { return m_bodies.size(); }
          RigidBody3D& body(std::size_t i)       { return *m_bodies[i]; }
    const RigidBody3D& body(std::size_t i) const { return *m_bodies[i]; }

    BodyRange<RigidBody3D>       bodies() noexcept       { return BodyRange<RigidBody3D>(&m_bodies); }
    BodyRange<const RigidBody3D> bodies() const noexcept { return BodyRange<const RigidBody3D>(&m_bodies); }

          Config& config()       noexcept { return m_cfg; }
    const Config& config() const noexcept { return m_cfg; }

    void set_gravity(const vector3d& g) noexcept { m_cfg.gravity = g; }
    vector3d gravity() const noexcept { return m_cfg.gravity; }

    void on_contact_begin(std::function<void(const ContactEvent3D&)> cb) { m_on_begin   = std::move(cb); }
    void on_contact_end(std::function<void(const ContactEvent3D&)> cb)   { m_on_end     = std::move(cb); }
    void on_pre_solve(std::function<bool(RigidBody3D& a, RigidBody3D& b, const ContactManifold3D&)> cb)   { m_pre_solve  = std::move(cb); }
    void on_post_solve(std::function<bool(RigidBody3D& a, RigidBody3D& b, const ContactManifold3D&)> cb) { m_post_solve = std::move(cb); }

    const std::vector<ContactManifold3D>& contacts() const noexcept { return m_manifolds; }

    void step(double dt);

    void step() { step(m_cfg.fixed_dt); }

    std::vector<RigidBody3D*> query_aabb(const AABB3D& region);

    RigidBody3D* point_query(const vector3d& point);

private:
    using Key     = detail::ContactKey<RigidBody3D>;
    using KeyHash = detail::ContactKeyHash<RigidBody3D>;

    struct CachedContact {
        RigidBody3D* body_a = nullptr;
        RigidBody3D* body_b = nullptr;
        bool sensor = false;
        bool events = false;
        bool touching = false; 
        vector3d normal{};
        vector3d point{};
        int point_count = 0;
        std::array<vector3d, ContactManifold3D::MAX_POINTS> local_anchor_a{};
        std::array<double,   ContactManifold3D::MAX_POINTS> normal_impulse{};
        std::array<vector3d, ContactManifold3D::MAX_POINTS> friction_impulse{};
    };

    using ContactCache = std::unordered_map<Key, CachedContact, KeyHash>;

    static AABB3D world_bounds_of(const Config& cfg) noexcept {
        const double h = cfg.world_half_extent;
        return { { -h, -h, -h }, { h, h, h } };
    }

    static Key key_of(const ContactManifold3D& mf) noexcept { return { mf.body_a, mf.body_b, mf.collider_a, mf.collider_b }; }

    static bool is_enabled(const RigidBody3D& b) noexcept { return has_flag(b.flags(), PhysicsFlags::Enabled); }
    static bool is_dynamic(const RigidBody3D& b) noexcept { return b.inv_mass() > 0.0; }

    static bool is_active(const RigidBody3D& b) noexcept;

    static bool responds(const RigidBody3D& a, const RigidBody3D& b) noexcept;

    static bool wants_events(const RigidBody3D& a, const RigidBody3D& b, bool sensor) noexcept;

    static vector3d point_velocity(const RigidBody3D& b, const vector3d& r) noexcept {
        return b.linear_velocity() + vec3::cross(b.angular_velocity(), r);
    }

    static void apply_impulse(RigidBody3D& a, RigidBody3D& b, const vector3d& rA, const vector3d& rB, const vector3d& P);

    static double effective_inv_mass(const RigidBody3D& a, const RigidBody3D& b, const vector3d& rA, const vector3d& rB, const vector3d& u);

    void synchronize_all_colliders() {
        for (auto& b : m_bodies) {
            if (is_enabled(*b)) b->synchronize_colliders();
        }
    }

    void broadphase();

    void narrowphase();

    void generate_contacts(RigidBody3D* a, RigidBody3D* b);

    void build_islands_and_wake();

    void sleep_islands(double dt);

    void build_velocity_constraints(double dt);

    void warm_start();

    void solve_velocity_constraints(bool reverse);

    static bool solve_normal_block(VelocityConstraint3D& vc);

    void store_impulses();

    bool solve_position_constraints(bool reverse);

    void update_contact_cache();

    void dispatch_events();

    void remove_body_now(RigidBody3D* body);

    void flush_pending_removals();

    void cleanup_bodies();

private:
    Config m_cfg;

    std::vector<std::unique_ptr<RigidBody3D>> m_bodies;

    Octree<RigidBody3D>       m_tree;
    std::vector<RigidBody3D*> m_body_ptrs;
    std::vector<AABB3D>       m_body_aabbs;
    std::vector<std::pair<RigidBody3D*, RigidBody3D*>> m_pairs;

    std::vector<ContactManifold3D>    m_manifolds;
    std::vector<VelocityConstraint3D> m_constraints;

    ContactCache     m_contact_cache; 
    ContactCache     m_touching;      
    std::vector<Key> m_carried;       

    detail::UnionFind m_islands;
    std::unordered_map<const RigidBody3D*, std::size_t> m_body_index;

    std::vector<ContactEvent3D> m_begin_events;
    std::vector<ContactEvent3D> m_end_events;
    std::vector<RigidBody3D*>   m_pending_removals;
    bool m_locked = false;

    std::function<void(const ContactEvent3D&)> m_on_begin;
    std::function<void(const ContactEvent3D&)> m_on_end;
    std::function<bool(RigidBody3D& a, RigidBody3D& b, const ContactManifold3D&)> m_pre_solve;
    std::function<bool(RigidBody3D& a, RigidBody3D& b, const ContactManifold3D&)> m_post_solve;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_SIMULATION_3D_HPP