#ifndef FIZMO_SIMULATION_3D_HPP
#define FIZMO_SIMULATION_3D_HPP

#include "octree.hpp"
#include "dynamic_tree.hpp"
#include "convex_3d.hpp"
#include "joints_3d.hpp"
#include "cloth_3d.hpp"

namespace fizmo {
namespace physics {

struct ContactPoint3D {
    RVec3 position{};           // world-space point midway between the two surfaces (m)
    Real separation = 0.0_r;       // negative = penetrating (m)
    Real normal_impulse = 0.0_r;   // accumulated normal impulse (N*s), filled in after the velocity solve
    RVec3 friction_impulse{};   // accumulated friction impulse, world space (N*s), filled in after the velocity solve
    RVec3 local_anchor_a{};     // point on A's surface, in A's body frame (m)
    RVec3 local_anchor_b{};     // point on B's surface, in B's body frame (m)
};

struct ContactManifold3D {
    static constexpr int MAX_POINTS = 4;

    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    RVec3 normal{}; // unit vector from A toward B
    ContactPoint3D points[MAX_POINTS];
    int point_count = 0;
    Real friction    = 0.0_r;
    Real restitution = 0.0_r;
    Real restitution_threshold = 1.0_r; // m/s
    std::size_t collider_a = 0; // index into body_a->colliders()
    std::size_t collider_b = 0; // index into body_b->colliders()
    std::uint32_t feature = 0;
};

namespace narrowphase3d {

struct Candidate {
    RVec3 position;
    Real   separation;
};

RVec3 closest_point_on_segment(const RVec3& p, const RVec3& a, const RVec3& b);

struct SegmentPair {
    RVec3 closest1{};
    RVec3 closest2{};
    Real fraction1 = 0.0_r;
    Real fraction2 = 0.0_r;
    Real distance_squared = 0.0_r;
};

SegmentPair closest_segment_points(const RVec3& p1, const RVec3& q1, const RVec3& p2, const RVec3& q2);

void reduce_contacts(ContactManifold3D& m, const std::vector<Candidate>& c, const RVec3& normal);

bool point_in_face(const HullShape& h, const HullFace& f, const RVec3& q);

RVec3 closest_point_on_face(const HullShape& h, const HullFace& f, const RVec3& p);

struct PointHullResult {
    bool     inside = false;
    RVec3 point{};        // closest surface point (m)
    RVec3 normal{};       // outward from the hull toward p
    Real   distance = 0.0_r; // signed: negative when p is inside
};

PointHullResult closest_point_on_hull(const HullShape& h, const RVec3& p);

bool segment_intersects_hull(const HullShape& h, const RVec3& p1, const RVec3& p2);

bool clip_segment_to_face(const HullShape& h, const HullFace& f, RVec3& a, RVec3& b);

int support_edge(const HullShape& h, const RVec3& dir, const RVec3& axis, Real sign);

void project_hull(const HullShape& h, const RVec3& axis, Real& lo, Real& hi);

void project_points(const std::vector<RVec3>& pts, const RVec3& axis, Real& lo, Real& hi);

bool collide_sphere_sphere(
    const SphereShape& sa, const Transform3D& xa,
    const SphereShape& sb, const Transform3D& xb,
    ContactManifold3D& m, Real margin = 0.0_r
);

bool collide_sphere_point(const RVec3& c, Real ra, const RVec3& p, Real rb, ContactManifold3D& m, Real margin);

bool collide_sphere_capsule(
    const SphereShape& s, const Transform3D& xs,
    const CapsuleShape3D& k, const Transform3D& xk,
    ContactManifold3D& m, Real margin = 0.0_r
);

bool collide_sphere_hull(
    const SphereShape& s, const Transform3D& xs,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, Real margin = 0.0_r
);

inline void plane_in_world(const PlaneShape& p, const Transform3D& xp, RVec3& n, Real& d) {
    n = xp.rotate(p.normal);
    d = p.offset + vec3::dot(n, xp.position);
}

bool collide_sphere_plane(
    const SphereShape& s, const Transform3D& xs,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, Real margin = 0.0_r
);

bool collide_capsule_capsule(
    const CapsuleShape3D& ka, const Transform3D& xa,
    const CapsuleShape3D& kb, const Transform3D& xb,
    ContactManifold3D& m, Real margin = 0.0_r
);

bool collide_capsule_plane(
    const CapsuleShape3D& k, const Transform3D& xk,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, Real margin = 0.0_r
);

bool collide_hull_plane(
    const HullShape& h, const Transform3D& xh,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, Real margin = 0.0_r
);

bool collide_capsule_hull(
    const CapsuleShape3D& k, const Transform3D& xk,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, Real margin = 0.0_r
);

namespace detail_sat3d {

struct FaceQuery {
    Real separation = -std::numeric_limits<Real>::max();
    int    index = -1;
};

struct EdgeQuery {
    Real   separation = -std::numeric_limits<Real>::max();
    int      dir_a = -1;
    int      dir_b = -1;
    RVec3 axis{}; 
};

FaceQuery query_faces(const HullShape& a, const std::vector<RVec3>& other);

EdgeQuery query_edges(const HullShape& a, const std::vector<RVec3>& b_vertices, const std::vector<RVec3>& b_dirs);

void clip_polygon(std::vector<RVec3>& poly, const RVec3& n, Real d, std::vector<RVec3>& scratch);

void face_contact(
    const HullShape& r, const Transform3D& xr, int rf,
    const HullShape& inc, const Transform3D& xi,
    std::vector<Candidate>& out, RVec3& normal, Real margin
);

} // namespace detail_sat3d

bool collide_hull_hull(
    const HullShape& a, const Transform3D& xa,
    const HullShape& b, const Transform3D& xb,
    ContactManifold3D& m, Real margin = 0.0_r
);

} // namespace narrowphase3d

bool collide_shapes(
    const Shape3D& sa, const Transform3D& xa,
    const Shape3D& sb, const Transform3D& xb,
    ContactManifold3D& manifold, Real margin = 0.0_r
);

struct VelocityConstraintPoint3D {
    RVec3 rA{}; // contact − center_A (m)
    RVec3 rB{}; // contact − center_B (m)
    Real normal_mass = 0.0_r;
    Real tangent_mass[2] = { 0.0_r, 0.0_r };
    Real velocity_bias = 0.0_r;
    Real normal_impulse = 0.0_r;                
    Real tangent_impulse[2] = { 0.0_r, 0.0_r };
};

struct VelocityConstraint3D {
    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    std::size_t index_a = 0;
    std::size_t index_b = 0;
    bool responds_a = false;
    bool responds_b = false;
    Real friction2 = 0.0_r;
    Real rolling_friction = 0.0_r;
    Real spinning_friction = 0.0_r;
    Real spin_mass = 0.0_r;
    Real roll_mass[2] = { 0.0_r, 0.0_r };
    Real spin_impulse = 0.0_r;
    Real roll_impulse[2] = { 0.0_r, 0.0_r };
    Real relative_normal_velocity[4] = { 0.0_r, 0.0_r, 0.0_r, 0.0_r };
    Real base_separation[4] = { 0.0_r, 0.0_r, 0.0_r, 0.0_r };
    std::size_t island = 0;
    RVec3 normal{};
    RVec3 tangent1{};
    RVec3 tangent2{};
    Real friction = 0.0_r;
    Real restitution = 0.0_r;
    Real restitution_threshold = 1.0_r;
    VelocityConstraintPoint3D points[ContactManifold3D::MAX_POINTS];
    int point_count = 0;
    std::size_t manifold_index = 0;
    Real normal_block[ContactManifold3D::MAX_POINTS][ContactManifold3D::MAX_POINTS] = {}; 
};

struct ContactEvent3D {
    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    RVec3 normal{};
    RVec3 point{};
    Real impulse = 0.0_r; 
    bool sensor = false;  
};

struct TreeVec3F {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float float_below(Real v) noexcept {
    float f = static_cast<float>(v);
    if (static_cast<Real>(f) > v) f = std::nextafter(f, -std::numeric_limits<float>::infinity());
    return f;
}

inline float float_above(Real v) noexcept {
    float f = static_cast<float>(v);
    if (static_cast<Real>(f) < v) f = std::nextafter(f, std::numeric_limits<float>::infinity());
    return f;
}

struct TreeBox3F {
    TreeVec3F min{};
    TreeVec3F max{};

    TreeBox3F() = default;
    TreeBox3F(const AABB3D& b) noexcept
        : min{ float_below(b.min.x), float_below(b.min.y), float_below(b.min.z) },
          max{ float_above(b.max.x), float_above(b.max.y), float_above(b.max.z) } {}

    bool overlaps(const TreeBox3F& o) const noexcept {
        return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y && min.z <= o.max.z && max.z >= o.min.z;
    }
    bool contains(const TreeBox3F& o) const noexcept {
        return min.x <= o.min.x && min.y <= o.min.y && min.z <= o.min.z && max.x >= o.max.x && max.y >= o.max.y && max.z >= o.max.z;
    }
    AABB3D to_aabb() const noexcept { return AABB3D(RVec3{ min.x, min.y, min.z }, RVec3{ max.x, max.y, max.z }); }
};

template <>
struct BoxTraits<TreeBox3F> {
    using Vector = RVec3;
    static bool overlaps(const TreeBox3F& a, const TreeBox3F& b) noexcept { return a.overlaps(b); }
    static bool contains(const TreeBox3F& outer, const TreeBox3F& inner) noexcept { return outer.contains(inner); }
    static TreeBox3F merge(const TreeBox3F& a, const TreeBox3F& b) noexcept {
        TreeBox3F m;
        m.min = { std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z) };
        m.max = { std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z) };
        return m;
    }
    static TreeBox3F fatten(const TreeBox3F& a, Real m) noexcept { return TreeBox3F(a.to_aabb().fatten(m)); }
    static Real cost(const TreeBox3F& a) noexcept {
        const Real w = std::min<Real>(a.max.x - a.min.x, 1e12_r), h = std::min<Real>(a.max.y - a.min.y, 1e12_r), d = std::min<Real>(a.max.z - a.min.z, 1e12_r);
        return 2.0_r * (w * h + h * d + d * w);
    }
    static TreeBox3F translate(const TreeBox3F& a, const Vector& v) noexcept {
        const AABB3D b = a.to_aabb();
        return TreeBox3F(AABB3D(b.min + v, b.max + v));
    }
};

struct BodySnapshot3F {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float qx = 0.0f;
    float qy = 0.0f;
    float qz = 0.0f;
    float qw = 1.0f;
    BodyHandle handle{};
    bool awake = false;
};

template <>
struct BoxTraits<AABB3D> {
    using Vector = RVec3;
    static bool overlaps(const AABB3D& a, const AABB3D& b) noexcept { return a.overlaps(b); }
    static bool contains(const AABB3D& outer, const AABB3D& inner) noexcept { return outer.contains(inner); }
    static AABB3D merge(const AABB3D& a, const AABB3D& b) noexcept { return AABB3D::merge(a, b); }
    static AABB3D fatten(const AABB3D& a, Real m) noexcept { return a.fatten(m); }
    static Real cost(const AABB3D& a) noexcept {
        const Real w = std::min(a.max.x - a.min.x, 1e12_r), h = std::min(a.max.y - a.min.y, 1e12_r), d = std::min(a.max.z - a.min.z, 1e12_r);
        return 2.0_r * (w * h + h * d + d * w);
    }
    static AABB3D translate(const AABB3D& a, const Vector& v) noexcept { return { a.min + v, a.max + v }; }
};

enum class Broadphase3D : std::uint8_t {
    DynamicTree = 0,
    Octree
};

enum class Solver3D : std::uint8_t {
    Classic = 0,
    SoftStep
};

enum class ConvexMethod3D : std::uint8_t {
    Sat = 0,
    Gjk
};

struct QueryFilter3D {
    std::uint64_t layers = std::numeric_limits<std::uint64_t>::max();
    bool include_sensors = false;
    bool include_static = true;
    bool include_dynamic = true;
    bool include_kinematic = true;
    const RigidBody3D* ignore = nullptr;
    std::function<bool(const RigidBody3D&, const Collider3D&)> predicate;
};

struct RayHit3D {
    RigidBody3D* body = nullptr;
    std::size_t collider = 0;
    Real distance = 0.0_r;
    Real fraction = 0.0_r;
    RVec3 point{};
    RVec3 normal{};
    std::uint32_t feature = 0;
};

struct ShapeHit3D {
    RigidBody3D* body = nullptr;
    std::size_t collider = 0;
    Real fraction = 0.0_r;
    RVec3 point{};
    RVec3 normal{};
};

struct OverlapHit3D {
    RigidBody3D* body = nullptr;
    std::size_t collider = 0;
};

struct Simulation3DStats {
    std::size_t bodies = 0;
    std::size_t awake = 0;
    std::size_t proxies = 0;
    std::size_t proxies_moved = 0;
    std::size_t pairs = 0;
    std::size_t manifolds = 0;
    std::size_t reused_manifolds = 0;
    std::size_t layer_rejects = 0;
    std::size_t islands = 0;
    std::size_t joints = 0;
    std::size_t colors = 0;
    int tree_height = 0;
    int threads = 1;
};

struct Simulation3DConfig {
    RVec3 gravity           = to_real(default_axes3d().gravity(constants::gravity())); // m/s2
    Real   fixed_dt          = 1.0_r / 60.0_r;        // s
    int      velocity_iters    = 10;
    int      position_iters    = 3;
    Real   baumgarte_factor  = 0.2_r;               // dimensionless
    Real   slop              = 0.002_r;             // m (allowed penetration; a resting box can lean by up to 2*slop across its diagonal)
    Real   max_correction    = 0.2_r;               // m (largest position correction per contact per iteration)
    Real   broadphase_margin = 0.1_r;               // m (AABB fattening)
    Real   speculative_distance = 0.02_r;           // m (points this far apart are kept so contacts persist; must be < broadphase_margin)
    Real   warm_start_match_distance = 0.1_r;       // m (how far a contact may drift and still reuse last step's impulse)
    bool     warm_starting     = true;
    bool     allow_sleeping    = true;
    bool     gyroscopic        = true;              // integrate w x Iw for non-spherical bodies
    Real   world_half_extent = 500.0_r;             // m
    CombineMode friction_combine = CombineMode::GeometricMean;
    CombineMode restitution_combine = CombineMode::Maximum;
    int      substeps          = 1;
    bool     continuous        = true;
    bool     continuous_all    = false;
    Real   max_continuous_distance = 8.0_r;
    Broadphase3D broadphase    = Broadphase3D::DynamicTree;
    Solver3D solver            = Solver3D::Classic;
    int      soft_substeps     = 4;
    Real   contact_hertz     = 30.0_r;
    Real   contact_damping_ratio = 10.0_r;
    Real   joint_hertz       = 60.0_r;
    Real   joint_damping_ratio = 2.0_r;
    Real   max_push_speed    = 3.0_r;
    Real   joint_baumgarte   = 0.2_r;
    ConvexMethod3D hull_collisions = ConvexMethod3D::Sat;
    int      threads           = 1;
    bool     simd              = false;
    bool     deterministic     = false;
    bool     reuse_contacts    = true;
    Real   reuse_distance    = 0.0005_r;
    Real   reuse_angle       = 0.002_r;
    bool     event_callbacks   = true;
    bool     rotation_aware_ccd = true;
    std::size_t parallel_threshold = 32;
    Real   rebase_distance   = 0.0_r;
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
        m_bodies.back()->set_id(m_next_id++);
        m_bodies.back()->m_handle = m_handles.add(m_bodies.back().get());
        return *m_bodies.back();
    }

    BodyHandle handle_of(const RigidBody3D& body) const noexcept { return body.handle(); }
    RigidBody3D* get(BodyHandle h) const noexcept { return m_handles.get(h); }
    bool alive(BodyHandle h) const noexcept { return m_handles.alive(h); }

    RigidBody3D& add_body(const RVec3& pos, PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
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

    void set_axes(const Axes3D& axes) noexcept { m_cfg.gravity = to_real(axes.gravity(fast::length(m_cfg.gravity))); }

    void set_gravity(const RVec3& g) noexcept { m_cfg.gravity = g; }
    RVec3 gravity() const noexcept { return m_cfg.gravity; }

    void on_contact_begin(std::function<void(const ContactEvent3D&)> cb) { m_on_begin   = std::move(cb); }
    void on_contact_end(std::function<void(const ContactEvent3D&)> cb)   { m_on_end     = std::move(cb); }
    void on_pre_solve(std::function<bool(RigidBody3D& a, RigidBody3D& b, const ContactManifold3D&)> cb)   { m_pre_solve  = std::move(cb); }
    void on_post_solve(std::function<bool(RigidBody3D& a, RigidBody3D& b, const ContactManifold3D&)> cb) { m_post_solve = std::move(cb); }
    using CombineCallback = std::function<Real(const Collider3D& a, const Collider3D& b)>;
    void set_friction_combiner(CombineCallback cb) { m_friction_combiner = std::move(cb); }
    void set_restitution_combiner(CombineCallback cb) { m_restitution_combiner = std::move(cb); }
    Real combined_friction(const Collider3D& a, const Collider3D& b) const;
    Real combined_restitution(const Collider3D& a, const Collider3D& b) const;

    const std::vector<ContactManifold3D>& contacts() const noexcept { return m_manifolds; }

    void step(Real dt);

    void step() { step(m_cfg.fixed_dt); }

    std::vector<RigidBody3D*> query_aabb(const AABB3D& region);

    RigidBody3D* point_query(const RVec3& point);

    std::optional<RayHit3D> raycast(const RVec3& origin, const RVec3& direction, Real max_distance, const QueryFilter3D& filter = {});
    std::vector<RayHit3D> raycast_all(const RVec3& origin, const RVec3& direction, Real max_distance, const QueryFilter3D& filter = {});
    std::optional<ShapeHit3D> shape_cast(const Shape3D& shape, const Transform3D& start, const RVec3& translation, const QueryFilter3D& filter = {});
    std::vector<OverlapHit3D> overlap(const Shape3D& shape, const Transform3D& xf, const QueryFilter3D& filter = {});
    std::vector<RigidBody3D*> overlap_sphere(const RVec3& center, Real radius, const QueryFilter3D& filter = {});
    std::vector<RigidBody3D*> overlap_box(const RVec3& center, const RVec3& half_extents, const RQuat& rotation, const QueryFilter3D& filter = {});
    std::vector<RigidBody3D*> query_aabb(const AABB3D& region, const QueryFilter3D& filter);

    Joint3D& add_joint(const JointDef3D& def);
    Joint3D& add_ball_joint(RigidBody3D& a, RigidBody3D* b, const RVec3& world_anchor);
    Joint3D& add_hinge_joint(RigidBody3D& a, RigidBody3D* b, const RVec3& world_anchor, const RVec3& world_axis);
    Joint3D& add_slider_joint(RigidBody3D& a, RigidBody3D* b, const RVec3& world_anchor, const RVec3& world_axis);
    Joint3D& add_fixed_joint(RigidBody3D& a, RigidBody3D* b);
    Joint3D& add_distance_joint(RigidBody3D& a, RigidBody3D* b, const RVec3& world_anchor_a, const RVec3& world_anchor_b, Real min_length = -1.0_r, Real max_length = -1.0_r);
    Joint3D& add_spring_joint(RigidBody3D& a, RigidBody3D* b, const RVec3& world_anchor_a, const RVec3& world_anchor_b, Real rest_length, Real frequency, Real damping_ratio = 0.5_r);
    Joint3D& add_cone_twist_joint(RigidBody3D& a, RigidBody3D* b, const RVec3& world_anchor, const RVec3& world_axis, Real swing_limit, Real twist_lower, Real twist_upper);
    void remove_joint(Joint3D& joint);
    std::size_t joint_count() const noexcept { return m_joints.size(); }
    Joint3D& joint(std::size_t i) { return *m_joints[i]; }
    const Joint3D& joint(std::size_t i) const { return *m_joints[i]; }

    const std::vector<ContactEvent3D>& begin_events() const noexcept { return m_last_begin; }
    const std::vector<ContactEvent3D>& end_events() const noexcept { return m_last_end; }
    void clear_events() noexcept {
        m_last_begin.clear();
        m_last_end.clear();
    }

    void shift_origin(const RVec3& offset);
    Cloth3D& add_cloth(const ClothDef3D& def) {
        m_cloths.push_back(std::make_unique<Cloth3D>(def));
        return *m_cloths.back();
    }
    Cloth3D& add_rope(const RVec3& start, const RVec3& direction, int segments, Real length) { return add_cloth(ClothDef3D::rope(start, direction, segments, length)); }
    void remove_cloth(Cloth3D& cloth) {
        m_cloths.erase(std::remove_if(m_cloths.begin(), m_cloths.end(), [&cloth](const std::unique_ptr<Cloth3D>& c) { return c.get() == &cloth; }), m_cloths.end());
    }
    std::size_t cloth_count() const noexcept { return m_cloths.size(); }
    Cloth3D& cloth(std::size_t i) { return *m_cloths.at(i); }
    const Cloth3D& cloth(std::size_t i) const { return *m_cloths.at(i); }
    void set_focus(const RVec3& point) noexcept { m_focus = point; }
    const RVec3& focus() const noexcept { return m_focus; }
    const RVec3& origin() const noexcept { return m_origin; }
    RVec3 absolute_position(const RigidBody3D& body) const noexcept { return m_origin + body.position(); }
    void snapshot(std::vector<BodySnapshot3F>& out) const;
    bool rebase_if_needed();
    const Simulation3DStats& stats() const noexcept { return m_stats; }
    bool can_collide(const RigidBody3D& a, const RigidBody3D& b) const noexcept;

private:
    using Key     = detail::ContactKey<RigidBody3D>;
    using KeyHash = detail::ContactKeyHash<RigidBody3D>;

    struct CachedContact {
        RigidBody3D* body_a = nullptr;
        RigidBody3D* body_b = nullptr;
        bool sensor = false;
        bool events = false;
        bool touching = false; 
        RVec3 normal{};
        RVec3 point{};
        int point_count = 0;
        std::array<RVec3, ContactManifold3D::MAX_POINTS> local_anchor_a{};
        std::array<Real,   ContactManifold3D::MAX_POINTS> normal_impulse{};
        std::array<RVec3, ContactManifold3D::MAX_POINTS> friction_impulse{};
    };

    using ContactCache = std::unordered_map<Key, CachedContact, KeyHash>;

    static AABB3D world_bounds_of(const Config& cfg) noexcept {
        const Real h = cfg.world_half_extent;
        return { { -h, -h, -h }, { h, h, h } };
    }

    static Key key_of(const ContactManifold3D& mf) noexcept { return { mf.body_a, mf.body_b, mf.collider_a, mf.collider_b }; }

    static bool is_enabled(const RigidBody3D& b) noexcept { return has_flag(b.flags(), PhysicsFlags::Enabled); }
    static bool is_dynamic(const RigidBody3D& b) noexcept { return b.inv_mass() > 0.0_r; }

    static bool is_active(const RigidBody3D& b) noexcept;

    static bool responds(const RigidBody3D& a, const RigidBody3D& b) noexcept;

    static bool wants_events(const RigidBody3D& a, const RigidBody3D& b, bool sensor) noexcept;

    static RVec3 point_velocity(const RigidBody3D& b, const RVec3& r) noexcept {
        return b.linear_velocity() + vec3::cross(b.angular_velocity(), r);
    }

    static void apply_impulse(RigidBody3D& a, RigidBody3D& b, const RVec3& rA, const RVec3& rB, const RVec3& P);

    static Real effective_inv_mass(const RigidBody3D& a, const RigidBody3D& b, const RVec3& rA, const RVec3& rB, const RVec3& u);

    void synchronize_all_colliders() {
        for (auto& b : m_bodies) {
            if (is_enabled(*b)) b->synchronize_colliders();
        }
    }

    void step_substep(Real dt);

    bool sweeps(const RigidBody3D& b) const noexcept;

    Real sweep_margin(const RigidBody3D& a, const RigidBody3D& b) const noexcept;

    void broadphase();

    void narrowphase();

    void generate_contacts(RigidBody3D* a, RigidBody3D* b);

    void build_islands_and_wake();

    void sleep_islands(Real dt);

    void build_velocity_constraints(Real dt);

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

    struct PairResult {
        std::vector<ContactManifold3D> manifolds;
        std::vector<std::pair<Key, CachedContact>> touches;
        bool reused = false;
    };

    struct ReuseEntry {
        RVec3 relative_position{};
        RQuat relative_orientation{ 1.0_r, 0.0_r, 0.0_r, 0.0_r };
        std::vector<ContactManifold3D> manifolds;
        std::vector<RVec3> local_normals;
        std::vector<std::pair<Key, CachedContact>> touches;
        std::uint64_t step = 0;
    };

    struct Island {
        std::vector<std::size_t> bodies;
        std::vector<std::size_t> constraints;
        std::vector<Joint3D*> joints;
        int velocity_iterations = 0;
        int position_iterations = 0;
        int flip = 0;
    };

    void tree_broadphase();
    void sync_tree(bool for_query);
    AABB3D proxy_box(const RigidBody3D& b) const;
    void compute_pair(RigidBody3D* a, RigidBody3D* b, PairResult& out);
    void commit_pair(PairResult& result);
    bool try_reuse(RigidBody3D* a, RigidBody3D* b, PairResult& out);
    void remember_pair(RigidBody3D* a, RigidBody3D* b, const PairResult& result);
    void build_solver_bodies();
    void build_islands();
    void solve_island_classic(Island& island, Real dt);
    void solve_island_soft(Island& island, Real dt);
    void solve_contacts(Island& island, bool use_bias, bool soft, Real inv_h);
    void solve_contact(VelocityConstraint3D& vc, bool reverse, bool use_bias, bool soft, Real inv_h);
    void solve_contacts_wide(Island& island);
    bool solve_normal_block_solver(VelocityConstraint3D& vc);
    void warm_start_island(Island& island);
    void apply_restitution(Island& island);
    void integrate_island(Island& island, Real dt);
    void position_correct_island(Island& island);
    void write_back(Island& island);
    void prepare_joints(Real dt);
    void finish_joints(Real dt);
    bool body_sweeps(const RigidBody3D& b) const noexcept;
    void wake_joint_bodies();
    bool joined_without_collision(const RigidBody3D* a, const RigidBody3D* b) const;
    bool accepts(const RigidBody3D& b, const Collider3D& c, const QueryFilter3D& f) const;

private:
    Config m_cfg;
    Real m_substep_dt = 0.0_r;

    std::vector<std::unique_ptr<RigidBody3D>> m_bodies;
    detail::HandleTable<RigidBody3D> m_handles;

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
    CombineCallback m_friction_combiner;
    CombineCallback m_restitution_combiner;

    DynamicTree<TreeBox3F, RigidBody3D*> m_dtree;
    std::vector<std::unique_ptr<Cloth3D>> m_cloths;
    RVec3 m_focus{};
    RVec3 m_origin{};
    std::vector<std::pair<std::int32_t, std::int32_t>> m_tree_pairs;
    std::unordered_map<std::uint64_t, std::size_t> m_tree_pair_index;
    std::vector<SolverBody3D> m_solver;
    std::vector<Island> m_island_list;
    std::vector<std::size_t> m_body_island;
    std::unordered_map<std::uint64_t, ReuseEntry> m_reuse;
    std::vector<PairResult> m_pair_results;
    std::vector<std::unique_ptr<Joint3D>> m_joints;
    std::vector<ContactEvent3D> m_last_begin;
    std::vector<ContactEvent3D> m_last_end;
    detail::TaskPool m_pool;
    Simulation3DStats m_stats;
    std::uint64_t m_next_id = 1;
    std::uint64_t m_next_joint_id = 1;
    std::uint64_t m_step_count = 0;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_SIMULATION_3D_HPP