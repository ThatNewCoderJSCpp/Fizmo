#include "fizmo_library.hpp"
#include "physics_2d.hpp"

namespace fizmo {
namespace physics {

auto Sweep2D::get_transform(Real t) const noexcept -> Transform2D { // t: unitless [0,1]
    RVec2 p = position0 * (1.0_r - t) + position1 * t;
    Real a   = angle0 * (1.0_r - t) + angle1 * t;
    Transform2D xf(p, a);
    xf.position = p - xf.rotate(center_local);
    return xf;
}

void PolygonShape::set(const RVec2* pts, std::size_t n) {
    assert(n >= 3 && n <= MAX_POLYGON_VERTICES);
    count = n;
    for (std::size_t i = 0; i < n; ++i) vertices[i] = pts[i];

    for (std::size_t i = 0; i < n; ++i) {
        const RVec2& v0 = vertices[i];
        const RVec2& v1 = vertices[(i + 1) % n];
        RVec2 edge = v1 - v0;
        normals[i] = fast::normalize(RVec2(edge.y, -edge.x));
    }

    Real area_sum = 0.0_r; 
    RVec2 c{};

    for (std::size_t i = 0; i < n; ++i) {
        const RVec2& v0 = vertices[i];
        const RVec2& v1 = vertices[(i + 1) % n];
        Real cross = fast::perp_dot(v0, v1); 
        area_sum += cross;
        c += (v0 + v1) * cross;
    }

    Real inv6A = 1.0_r / (3.0_r * area_sum);
    centroid = c * inv6A;
}

auto PolygonShape::make_box(Real hx, Real hy) -> PolygonShape { // hx, hy: meters (half-extents)
    PolygonShape s;
    RVec2 pts[4] = { {-hx, -hy}, { hx, -hy}, { hx,  hy}, {-hx,  hy} };
    s.set(pts, 4);
    return s;
}

auto PolygonShape::make_box(Real hx, Real hy, const RVec2& center, Real angle_rad) -> PolygonShape {
    PolygonShape s = make_box(hx, hy);
    Transform2D xf(center, angle_rad);

    for (std::size_t i = 0; i < s.count; ++i) {
        s.vertices[i] = xf.apply(s.vertices[i]);
        s.normals[i]  = xf.rotate(s.normals[i]);
    }
        
    s.centroid = xf.apply(s.centroid);
    return s;
}

auto Shape2D::make_circle(const RVec2& center, Real radius) -> Shape2D {
    Shape2D s;
    s.type = ShapeType::Circle;
    s.circle = { center, radius };
    s.bounding_radius = fast::length(center) + radius;
    s.local_aabb = { center - RVec2(radius, radius), center + RVec2(radius, radius) };
    return s;
}

auto Shape2D::make_polygon(const RVec2* pts, std::size_t n) -> Shape2D {
    Shape2D s;
    s.type = ShapeType::Polygon;
    s.polygon.set(pts, n);
    s.compute_polygon_bounds();
    return s;
}

auto Shape2D::make_box(Real hx, Real hy) -> Shape2D { // meter half-extents
    Shape2D s;
    s.type = ShapeType::Polygon;
    s.polygon = PolygonShape::make_box(hx, hy);
    s.compute_polygon_bounds();
    return s;
}

auto Shape2D::make_capsule(const RVec2& p1, const RVec2& p2, Real radius) -> Shape2D {
    Shape2D s;
    s.type = ShapeType::Capsule;
    s.capsule = { p1, p2, radius };
    Real r1 = fast::length(p1) + radius;
    Real r2 = fast::length(p2) + radius;
    s.bounding_radius = r1 > r2 ? r1 : r2;
    Real minx = (p1.x < p2.x ? p1.x : p2.x) - radius;
    Real miny = (p1.y < p2.y ? p1.y : p2.y) - radius;
    Real maxx = (p1.x > p2.x ? p1.x : p2.x) + radius;
    Real maxy = (p1.y > p2.y ? p1.y : p2.y) + radius;
    s.local_aabb = { {minx, miny}, {maxx, maxy} };
    return s;
}

auto Shape2D::make_edge(const RVec2& v1, const RVec2& v2) -> Shape2D {
    Shape2D s;
    s.type = ShapeType::Edge;
    s.edge = { v1, v2, {}, {}, false, false };
    Real r1 = fast::length(v1), r2 = fast::length(v2);
    s.bounding_radius = r1 > r2 ? r1 : r2;

    s.local_aabb = {
        { v1.x < v2.x ? v1.x : v2.x,  v1.y < v2.y ? v1.y : v2.y },
        { v1.x > v2.x ? v1.x : v2.x,  v1.y > v2.y ? v1.y : v2.y }
    };

    return s;
}

auto Shape2D::compute_aabb(const Transform2D& xf) const noexcept -> AABB {
    switch (type) {
        case ShapeType::Circle: {
            RVec2 c = xf.apply(circle.center);
            Real r = circle.radius;
            return { c - RVec2(r, r), c + RVec2(r, r) };
        }
        case ShapeType::Polygon: {
            RVec2 v = xf.apply(polygon.vertices[0]);
            AABB box = { v, v };

            for (std::size_t i = 1; i < polygon.count; ++i) {
                v = xf.apply(polygon.vertices[i]);
                if (v.x < box.min.x) box.min.x = v.x;
                if (v.y < box.min.y) box.min.y = v.y;
                if (v.x > box.max.x) box.max.x = v.x;
                if (v.y > box.max.y) box.max.y = v.y;
            }

            return box;
        }
        case ShapeType::Capsule: {
            RVec2 p1 = xf.apply(capsule.point1);
            RVec2 p2 = xf.apply(capsule.point2);
            Real r = capsule.radius;

            return {
                {
                    (p1.x < p2.x ? p1.x : p2.x) - r,
                    (p1.y < p2.y ? p1.y : p2.y) - r 
                },
                { 
                    (p1.x > p2.x ? p1.x : p2.x) + r,
                    (p1.y > p2.y ? p1.y : p2.y) + r 
                }
            };
        }
        case ShapeType::Edge: {
            RVec2 v1 = xf.apply(edge.v1);
            RVec2 v2 = xf.apply(edge.v2);

            return {
                { v1.x < v2.x ? v1.x : v2.x, v1.y < v2.y ? v1.y : v2.y },
                { v1.x > v2.x ? v1.x : v2.x, v1.y > v2.y ? v1.y : v2.y }
            };
        }
    }
    return {};
}

void Shape2D::compute_polygon_bounds() {
    bounding_radius = 0.0_r;
    RVec2 lo = polygon.vertices[0], hi = lo;

    for (std::size_t i = 0; i < polygon.count; ++i) {
        Real r = fast::length(polygon.vertices[i]);
        if (r > bounding_radius) bounding_radius = r;
        if (polygon.vertices[i].x < lo.x) lo.x = polygon.vertices[i].x;
        if (polygon.vertices[i].y < lo.y) lo.y = polygon.vertices[i].y;
        if (polygon.vertices[i].x > hi.x) hi.x = polygon.vertices[i].x;
        if (polygon.vertices[i].y > hi.y) hi.y = polygon.vertices[i].y;
    }

    local_aabb = { lo, hi };
}

void Shape2D::copy_from(const Shape2D& o) noexcept {
    type = o.type;
    bounding_radius = o.bounding_radius;
    local_aabb = o.local_aabb;

    switch (type) {
        case ShapeType::Circle:  circle  = o.circle;  break;
        case ShapeType::Polygon: polygon = o.polygon; break;
        case ShapeType::Capsule: capsule = o.capsule; break;
        case ShapeType::Edge:    edge    = o.edge;    break;
    }
}

auto MassData::combine(const MassData* parts, std::size_t n) -> MassData {
    MassData total;
    if (n == 0) return total;
    for (std::size_t i = 0; i < n; ++i) total.mass += parts[i].mass;
    if (total.mass <= 0.0_r) return total;
    for (std::size_t i = 0; i < n; ++i) total.center += parts[i].center * parts[i].mass;
    total.center = total.center / total.mass;

    for (std::size_t i = 0; i < n; ++i) {
        RVec2 d = parts[i].center - total.center;
        total.inertia += parts[i].inertia + parts[i].mass * fast::length_squared(d);
    }

    return total;
}

} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace detail {

Real polygon_area(const PolygonShape& p) {
    Real area = 0.0_r;

    for (std::size_t i = 0; i < p.count; ++i) {
        const RVec2& v0 = p.vertices[i];
        const RVec2& v1 = p.vertices[(i + 1) % p.count];
        area += fast::perp_dot(v0, v1);
    }

    return std::abs(area) * 0.5_r;
}

Real capsule_area(const CapsuleShape& c) {
    Real r = c.radius;
    Real d = fast::length(c.point2 - c.point1);
    return r * (2.0_r * d + constants::pi() * r);
}

} // namespace detail
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {

Real compute_shape_area(const Shape2D& shape) {
    switch (shape.type) {
        case ShapeType::Circle:  return detail::circle_area(shape.circle);
        case ShapeType::Polygon: return detail::polygon_area(shape.polygon);
        case ShapeType::Capsule: return detail::capsule_area(shape.capsule);
        case ShapeType::Edge:    return detail::edge_area(shape.edge);
    }
    return 0.0_r;
}

} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace detail {

MassData compute_circle_mass(const CircleShape& c, Real density) { 
    Real area = constants::pi() * c.radius * c.radius; // m2
    Real mass = density * area; // kg
    Real I_local = 0.5_r * mass * c.radius * c.radius; // kg*m2
    return { mass, I_local, c.center };
}

MassData compute_polygon_mass(const PolygonShape& p, Real density) { 
    Real area = 0.0_r;    // m2
    Real inertia = 0.0_r; // kg*m2
    RVec2 center{};

    for (std::size_t i = 0; i < p.count; ++i) {
        const RVec2& v0 = p.vertices[i];
        const RVec2& v1 = p.vertices[(i + 1) % p.count];
        Real cross = fast::perp_dot(v0, v1); // m2
        area += cross;
        center += (v0 + v1) * cross;
        Real tri_I = fast::dot(v0, v0) + fast::dot(v0, v1) + fast::dot(v1, v1); // m2
        inertia += cross * tri_I;
    }

    area *= 0.5_r;
    Real mass = density * std::abs(area); // kg
    Real inv6A = 1.0_r / (3.0_r * area);
    center = center * inv6A;
    inertia = density * std::abs(inertia) / 12.0_r; // kg*m2
    inertia -= mass * fast::length_squared(center);
    return { mass, inertia, center };
}

MassData compute_capsule_mass(const CapsuleShape& c, Real density) { 
    Real r = c.radius; // m
    Real d = fast::length(c.point2 - c.point1); // m
    Real rect_area = 2.0_r * r * d; // m2
    Real rect_mass = density * rect_area; // kg
    Real rect_I = rect_mass * (d * d / 12.0_r + r * r / 3.0_r); // kg*m2
    Real circ_area = constants::pi() * r * r; // m2
    Real circ_mass = density * circ_area; // kg
    Real circ_I = 0.5_r * circ_mass * r * r + circ_mass * (d * 0.5_r) * (d * 0.5_r); // kg*m2
    RVec2 center = c.center();
    Real total_mass = rect_mass + circ_mass; // kg
    Real total_I = rect_I + circ_I; // kg*m2
    return { total_mass, total_I, center };
}

} // namespace detail
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {

MassData compute_mass(const Shape2D& shape, Real density) {
    switch (shape.type) {
        case ShapeType::Circle:  return detail::compute_circle_mass(shape.circle, density);
        case ShapeType::Polygon: return detail::compute_polygon_mass(shape.polygon, density);
        case ShapeType::Capsule: return detail::compute_capsule_mass(shape.capsule, density);
        case ShapeType::Edge:    return detail::compute_edge_mass(shape.edge, density);
    }
    return {};
}

Real MassSpec::resolve_density(Real shape_area_m2, Real gravity) const noexcept {
    switch (mode) {
        case MassMode::Density: return value;
        case MassMode::Mass:    return (shape_area_m2 > 0.0_r) ? value / shape_area_m2 : 0.0_r;
        case MassMode::Weight:  return (shape_area_m2 > 0.0_r && gravity > 0.0_r) ? (value / gravity) / shape_area_m2 : 0.0_r;
    }
    return 0.0_r;
}

void Collider2D::recompute_mass(Real gravity) {
    const Real area    = compute_shape_area(shape);
    const Real density = material.mass_spec.resolve_density(area, gravity);
    mass_data = compute_mass(shape, density);
    mass_data.center = local_offset.apply(mass_data.center);
}

void Collider2D::update_world_aabb(const Transform2D& body_xf) {
    Transform2D world_xf = body_xf.compose(local_offset);
    world_aabb = shape.compute_aabb(world_xf);
}

bool RigidBody2D::freeze(FrozenProperty prop) noexcept {
    FrozenProperty candidate = m_frozen | prop;
    if (!validate_frozen(candidate)) return false;
    m_frozen = candidate;
    return true;
}

bool RigidBody2D::can_collide_with(const RigidBody2D& other) const noexcept {
    if (!has(PhysicsFlags::CollisionResponse) && !other.has(PhysicsFlags::CollisionResponse)) return false;
    if (has(PhysicsFlags::Intangible) && other.has(PhysicsFlags::Intangible)) return false;
    if ((m_collision_layer & other.m_collision_mask) == 0) return false;
    if ((other.m_collision_layer & m_collision_mask) == 0) return false;
    return true;
}

auto RigidBody2D::add_collider(
    Shape2D shape,
    Material mat,
    Transform2D offset,
    CollisionFilter filt,
    bool sensor 
) -> Collider2D& {
    m_colliders.emplace_back(Collider2D{
        std::move(shape),
        std::move(offset),
        std::move(mat),
        std::move(filt),
        sensor,
        this
    });

    Collider2D& c = m_colliders.back();
    m_mass_dirty = true;
    return c;
}

void RigidBody2D::set_density(Real d) noexcept {
    if (is_frozen(FrozenProperty::Density)) return;
    refresh_shape_area();
    m_density = d;
    resolve_mass_constraint_from(FrozenProperty::Density);
    m_mass_dirty = false;
}

void RigidBody2D::set_mass(Real kg) noexcept {
    if (is_frozen(FrozenProperty::Mass)) return;
    refresh_shape_area();
    m_mass     = kg;
    m_inv_mass = (kg > 0.0_r) ? 1.0_r / kg : 0.0_r;
    resolve_mass_constraint_from(FrozenProperty::Mass);
    m_mass_dirty = false;
}

void RigidBody2D::apply_force_at(const RVec2& f, const RVec2& world_point) {
    if (!has(PhysicsFlags::LinearForces)) return;
    ensure_mass_uptodate();
    m_force += f;

    if (has(PhysicsFlags::Torque)) {
        RVec2 r = world_point - m_transform.apply(m_local_center);
        m_torque += fast::perp_dot(r, f);
    }

    wake();
}

void RigidBody2D::apply_linear_impulse(const RVec2& impulse) {
    if (!has(PhysicsFlags::LinearImpulses)) return;
    ensure_mass_uptodate();
    m_linear_velocity += impulse * m_inv_mass;
    wake();
}

void RigidBody2D::apply_impulse_at(const RVec2& impulse, const RVec2& world_point) {
    if (!has(PhysicsFlags::LinearImpulses)) return;
    ensure_mass_uptodate();
    m_linear_velocity += impulse * m_inv_mass;
        
    if (has(PhysicsFlags::AngularImpulses)) {
        RVec2 r = world_point - m_transform.apply(m_local_center);
        m_angular_velocity += m_inv_inertia * fast::perp_dot(r, impulse);
    }
        
    wake();
}

void RigidBody2D::apply_angular_impulse(Real impulse) {
    if (!has(PhysicsFlags::AngularImpulses)) return;
    ensure_mass_uptodate();
    m_angular_velocity += m_inv_inertia * impulse;
    wake();
}

void RigidBody2D::integrate_forces(Real dt, const RVec2& gravity) {
    ensure_mass_uptodate();
    if (has(PhysicsFlags::Gravity))        m_linear_velocity += gravity * m_gravity_scale * dt;
    if (has(PhysicsFlags::LinearForces))   m_linear_velocity += m_force * m_inv_mass * dt;
    if (has(PhysicsFlags::Torque))         m_angular_velocity += m_torque * m_inv_inertia * dt;
    if (has(PhysicsFlags::LinearDamping))  m_linear_velocity *= 1.0_r / (1.0_r + dt * m_linear_damping);
    if (has(PhysicsFlags::AngularDamping)) m_angular_velocity *= 1.0_r / (1.0_r + dt * m_angular_damping);

    if (has(PhysicsFlags::VelocityClamping)) {
        Real speed_sq = fast::length_squared(m_linear_velocity);
        if (speed_sq > m_max_linear_speed * m_max_linear_speed) m_linear_velocity = m_linear_velocity * (m_max_linear_speed / std::sqrt(speed_sq));
    }

    if (has(PhysicsFlags::AngularClamping)) {
        if (std::abs(m_angular_velocity) > m_max_angular_speed) m_angular_velocity = (m_angular_velocity > 0.0_r ? 1.0_r : -1.0_r) * m_max_angular_speed;
    }
}

void RigidBody2D::integrate_velocities(Real dt) {
    ensure_mass_uptodate();
    RVec2 center = m_transform.apply(m_local_center);
    m_sweep.position0 = center;
    m_sweep.angle0    = m_transform.angle;

    if (has(PhysicsFlags::LinearVelocity)) {
        RVec2 dv = m_linear_velocity * dt;
        if (has(PhysicsFlags::LockPositionX)) dv.x = 0.0_r;
        if (has(PhysicsFlags::LockPositionY)) dv.y = 0.0_r;
        center += dv;
    }

    if (has(PhysicsFlags::AngularVelocity) && !rotation_locked()) m_transform.set_angle(m_transform.angle + m_angular_velocity * dt);
    m_transform.position = center - m_transform.rotate(m_local_center);
    m_sweep.position1 = center;
    m_sweep.angle1    = m_transform.angle;
}

void RigidBody2D::apply_position_correction(const RVec2& d_center, Real d_angle) {
    if (d_center.x == 0.0_r && d_center.y == 0.0_r && d_angle == 0.0_r) return;
    ensure_mass_uptodate();
    RVec2 dc = d_center;
    if (has(PhysicsFlags::LockPositionX)) dc.x = 0.0_r;
    if (has(PhysicsFlags::LockPositionY)) dc.y = 0.0_r;
    RVec2 center = m_transform.apply(m_local_center) + dc;
    if (d_angle != 0.0_r) m_transform.set_angle(m_transform.angle + d_angle);
    m_transform.position = center - m_transform.rotate(m_local_center);
}

void RigidBody2D::put_to_sleep() {
    m_is_awake         = false;
    m_sleep_timer      = 0.0_r;
    m_linear_velocity  = {};
    m_angular_velocity = 0.0_r;
    m_force            = {};
    m_torque           = 0.0_r;
}

Real RigidBody2D::advance_sleep_timer(Real dt) {
    if (!has(PhysicsFlags::AllowSleep)) { m_sleep_timer = 0.0_r; return 0.0_r; }

    if (fast::length_squared(m_linear_velocity) > LINEAR_SLEEP_TOLERANCE * LINEAR_SLEEP_TOLERANCE ||
        std::abs(m_angular_velocity) > ANGULAR_SLEEP_TOLERANCE) {
        m_sleep_timer = 0.0_r;
    } else {
        m_sleep_timer += dt;
    }

    return m_sleep_timer;
}

void RigidBody2D::update_sleep(Real dt) {
    if (!has(PhysicsFlags::AllowSleep)) return;
    if (advance_sleep_timer(dt) >= SLEEP_TIME_THRESHOLD) put_to_sleep();
}

auto RigidBody2D::compute_body_aabb() const -> AABB {
    if (m_colliders.empty()) return {};
    AABB result = m_colliders[0].world_aabb;
    for (std::size_t i = 1; i < m_colliders.size(); ++i) result = AABB::merge(result, m_colliders[i].world_aabb);
    return result;
}

auto RigidBody2D::get_velocity_at(const RVec2& world_point) const noexcept -> RVec2 {
    ensure_mass_uptodate();
    RVec2 r = world_point - m_transform.apply(m_local_center);
    return m_linear_velocity + RVec2(-m_angular_velocity * r.y, m_angular_velocity * r.x);
}

Real RigidBody2D::kinetic_energy() const noexcept {
    ensure_mass_uptodate();
    Real lin = 0.5_r * m_mass * fast::length_squared(m_linear_velocity);
    Real rot = 0.5_r * m_inertia * m_angular_velocity * m_angular_velocity;
    return lin + rot;
}

bool RigidBody2D::validate_frozen(FrozenProperty f) noexcept {
    int count = 0;
    if (has_frozen(f, FrozenProperty::Mass))    ++count;
    if (has_frozen(f, FrozenProperty::Density)) ++count;
    bool area_or_vol = has_frozen(f, FrozenProperty::Area) || has_frozen(f, FrozenProperty::Volume);
    if (area_or_vol) ++count;
    return count <= 2;
}

void RigidBody2D::recompute_mass_data(Real gravity) const {
    m_mass = 0.0_r; m_inv_mass = 0.0_r;
    m_inertia = 0.0_r; m_inv_inertia = 0.0_r;
    m_local_center = {};

    if (!has(PhysicsFlags::LinearForces) && !has(PhysicsFlags::LinearImpulses)) {
        m_sweep.center_local = {};
        return;
    }

    refresh_shape_area();
    std::vector<MassData> parts;
    parts.reserve(m_colliders.size());

    for (auto& c : m_colliders) {
        if (c.is_sensor) continue;
        const Real area    = compute_shape_area(c.shape);
        const Real density = c.material.mass_spec.resolve_density(area, gravity);
        MassData md = compute_mass(c.shape, density);
        md.center   = c.local_offset.apply(md.center);
        parts.push_back(md);
    }

    MassData total = MassData::combine(parts.data(), parts.size());

    if (total.mass > 0.0_r) {
        m_mass     = total.mass;
        m_inv_mass = 1.0_r / m_mass;
        m_local_center = total.center;
    } else {
        m_mass     = 1.0_r;
        m_inv_mass = 1.0_r;
    }

    if (total.inertia > 0.0_r && !rotation_locked()) {
        m_inertia     = total.inertia;
        m_inv_inertia = 1.0_r / m_inertia;
    } else {
        m_inertia     = 0.0_r;
        m_inv_inertia = 0.0_r;
    }

    m_sweep.center_local = m_local_center;
    m_density = (m_shape_area > 0.0_r) ? m_mass / m_shape_area : 0.0_r;
}

void RigidBody2D::resolve_mass_constraint_from(FrozenProperty just_set) noexcept {
    const bool mass_frozen    = is_frozen(FrozenProperty::Mass);
    const bool density_frozen = is_frozen(FrozenProperty::Density);
    const bool area_frozen    = is_frozen(FrozenProperty::Area) || is_frozen(FrozenProperty::Volume);

    if (just_set == FrozenProperty::Mass) {
        if (area_frozen || !density_frozen) {
            m_density = (m_shape_area > 0.0_r) ? m_mass / m_shape_area : 0.0_r;
        } else {
            derive_area_from_mass_density();
        }
            
        recompute_inertia_from_density();
    } else if (just_set == FrozenProperty::Density) {
        if (area_frozen || !mass_frozen) {
            derive_mass_from_density_area();
        } else {
            derive_area_from_mass_density();
        }
    } else {
        if (density_frozen || !mass_frozen) {
            derive_mass_from_density_area();
        } else {
            m_density = (m_shape_area > 0.0_r) ? m_mass / m_shape_area : 0.0_r;
            recompute_inertia_from_density();
        }
    }
}

void RigidBody2D::derive_mass_from_density_area() noexcept {
    m_mass     = m_density * m_shape_area;
    m_inv_mass = (m_mass > 0.0_r) ? 1.0_r / m_mass : 0.0_r;
    recompute_inertia_from_density();
}

void RigidBody2D::derive_area_from_mass_density() noexcept {
    if (m_density <= 0.0_r || m_colliders.empty()) return;
    Real target_area = m_mass / m_density;
    Real current     = m_shape_area;
    if (current <= 0.0_r) return;
    Real linear_scale = std::sqrt(target_area / current);  
    scale_collider_geometry(linear_scale);
    refresh_shape_area();
    recompute_inertia_from_density();
}

void RigidBody2D::scale_collider_geometry(Real s) noexcept {
    for (auto& c : m_colliders) {
        switch (c.shape.type) {
            case ShapeType::Circle:
                c.shape.circle.center = c.shape.circle.center * s;
                c.shape.circle.radius *= s;
                break;
            case ShapeType::Polygon:
                for (std::size_t i = 0; i < c.shape.polygon.count; ++i) c.shape.polygon.vertices[i] = c.shape.polygon.vertices[i] * s;
                c.shape.polygon.centroid = c.shape.polygon.centroid * s;
                break;
            case ShapeType::Capsule:
                c.shape.capsule.point1 = c.shape.capsule.point1 * s;
                c.shape.capsule.point2 = c.shape.capsule.point2 * s;
                c.shape.capsule.radius *= s;
                break;
            case ShapeType::Edge:
                c.shape.edge.v1 = c.shape.edge.v1 * s;
                c.shape.edge.v2 = c.shape.edge.v2 * s;
                if (c.shape.edge.has_ghost1) c.shape.edge.ghost1 = c.shape.edge.ghost1 * s;
                if (c.shape.edge.has_ghost2) c.shape.edge.ghost2 = c.shape.edge.ghost2 * s;
                break;
        }
            
        c.shape = rebuild_shape_bounds(c.shape);
    }
}

auto RigidBody2D::rebuild_shape_bounds(const Shape2D& src) noexcept -> Shape2D {
    Shape2D s = src;

    switch (s.type) {
        case ShapeType::Circle:
            s.bounding_radius = fast::length(s.circle.center) + s.circle.radius;
            s.local_aabb = { s.circle.center - RVec2(s.circle.radius, s.circle.radius), s.circle.center + RVec2(s.circle.radius, s.circle.radius) };
            break;
        case ShapeType::Polygon: {
            s.bounding_radius = 0.0_r;
            RVec2 lo = s.polygon.vertices[0], hi = lo;

            for (std::size_t i = 0; i < s.polygon.count; ++i) {
                Real r = fast::length(s.polygon.vertices[i]);
                if (r > s.bounding_radius) s.bounding_radius = r;
                if (s.polygon.vertices[i].x < lo.x) lo.x = s.polygon.vertices[i].x;
                if (s.polygon.vertices[i].y < lo.y) lo.y = s.polygon.vertices[i].y;
                if (s.polygon.vertices[i].x > hi.x) hi.x = s.polygon.vertices[i].x;
                if (s.polygon.vertices[i].y > hi.y) hi.y = s.polygon.vertices[i].y;
            }

            s.local_aabb = { lo, hi };
            break;
        }
        case ShapeType::Capsule: {
            auto& c = s.capsule;
            Real r1 = fast::length(c.point1) + c.radius;
            Real r2 = fast::length(c.point2) + c.radius;
            s.bounding_radius = r1 > r2 ? r1 : r2;

            s.local_aabb = {
                { std::min(c.point1.x, c.point2.x) - c.radius, std::min(c.point1.y, c.point2.y) - c.radius },
                { std::max(c.point1.x, c.point2.x) + c.radius, std::max(c.point1.y, c.point2.y) + c.radius }
            };

            break;
        }
        case ShapeType::Edge: {
            auto& e = s.edge;
            Real r1 = fast::length(e.v1), r2 = fast::length(e.v2);
            s.bounding_radius = r1 > r2 ? r1 : r2;

            s.local_aabb = {
                { std::min(e.v1.x, e.v2.x), std::min(e.v1.y, e.v2.y) },
                { std::max(e.v1.x, e.v2.x), std::max(e.v1.y, e.v2.y) }
            };

            break;
        }
    }

    return s;
}

void RigidBody2D::refresh_shape_area() const noexcept {
    m_shape_area = 0.0_r;
    for (const auto& c : m_colliders) { if (!c.is_sensor) m_shape_area += compute_shape_area(c.shape); }
}

void RigidBody2D::recompute_inertia_from_density() noexcept {
    std::vector<MassData> parts;
    parts.reserve(m_colliders.size());

    for (auto& c : m_colliders) {
        if (c.is_sensor) continue;
        MassData md = compute_mass(c.shape, m_density);
        md.center = c.local_offset.apply(md.center);
        parts.push_back(md);
    }

    MassData combined = MassData::combine(parts.data(), parts.size());
    m_local_center = combined.center;

    if (combined.inertia > 0.0_r && !rotation_locked()) {
        m_inertia     = combined.inertia;
        m_inv_inertia = 1.0_r / m_inertia;
    } else {
        m_inertia     = 0.0_r;
        m_inv_inertia = 0.0_r;
    }

    m_sweep.center_local = m_local_center;
}

void* RigidBody2D::operator new(std::size_t size) {
    if (size != sizeof(RigidBody2D)) return ::operator new(size);
    return detail::ObjectPool<sizeof(RigidBody2D), alignof(RigidBody2D)>::instance().allocate();
}

void RigidBody2D::operator delete(void* p, std::size_t size) noexcept {
    if (size != sizeof(RigidBody2D)) {
        ::operator delete(p);
        return;
    }
    detail::ObjectPool<sizeof(RigidBody2D), alignof(RigidBody2D)>::instance().release(p);
}

std::size_t RigidBody2D::pooled_bodies() {
    return detail::ObjectPool<sizeof(RigidBody2D), alignof(RigidBody2D)>::instance().live();
}

} // namespace physics
} // namespace fizmo
