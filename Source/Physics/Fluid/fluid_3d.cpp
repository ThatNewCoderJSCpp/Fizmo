#include "fizmo_library.hpp"
#include "fluid_3d.hpp"

#include <chrono>
#include <cstring>

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr Real kPi = 3.14159265358979323846_r;

Real norm(const V3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

V3 unit_or(const V3& v, const V3& fallback) noexcept {
    const Real l = norm(v);
    return l > 1e-12_r ? v * (1.0_r / l) : fallback;
}

Real aabb_distance(const AABB3D& b, const V3& p) noexcept {
    const Real dx = std::max({ b.min.x - p.x, 0.0_r, p.x - b.max.x });
    const Real dy = std::max({ b.min.y - p.y, 0.0_r, p.y - b.max.y });
    const Real dz = std::max({ b.min.z - p.z, 0.0_r, p.z - b.max.z });
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::uint64_t mix(std::uint64_t h, std::uint64_t v) noexcept {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

std::uint64_t real_bits(Real v) noexcept {
    const double d = static_cast<double>(v);
    std::uint64_t out = 0;
    std::memcpy(&out, &d, sizeof(out));
    return out;
}

Real analytic_volume(const Shape3D& s) noexcept {
    switch (s.type) {
    case ShapeType3D::Sphere: return 4.0_r / 3.0_r * kPi * s.sphere.radius * s.sphere.radius * s.sphere.radius;
    case ShapeType3D::Capsule: {
        const Real r = s.capsule.radius;
        return kPi * r * r * (2.0_r * s.capsule.half_length() + 4.0_r / 3.0_r * r);
    }
    case ShapeType3D::Hull: return s.hull ? s.hull->volume : 0.0_r;
    case ShapeType3D::Cylinder: return kPi * s.cylinder.radius * s.cylinder.radius * 2.0_r * s.cylinder.half_height;
    case ShapeType3D::Cone: return kPi * s.cone.radius * s.cone.radius * 2.0_r * s.cone.half_height / 3.0_r;
    default: return 0.0_r;
    }
}

} // namespace

namespace detail {

bool fluid_point_in_shape(const Shape3D& s, const RVec3& p) {
    switch (s.type) {
    case ShapeType3D::Sphere: {
        const RVec3 d = p - s.sphere.center;
        return vec3::dot(d, d) <= s.sphere.radius * s.sphere.radius;
    }
    case ShapeType3D::Capsule: {
        const V3 q = narrowphase3d::closest_point_on_segment(p, s.capsule.point1, s.capsule.point2);
        const V3 d = p - q;
        return vec3::dot(d, d) <= s.capsule.radius * s.capsule.radius;
    }
    case ShapeType3D::Hull: {
        if (!s.hull) return false;
        for (const HullFace& f : s.hull->faces) {
            if (vec3::dot(f.normal, p) - f.offset > 0.0_r) return false;
        }
        return true;
    }
    default: {
        if (!s.is_convex()) return false;
        ConvexProxy3D proxy;
        if (!make_convex_proxy(s, Transform3D::identity(), proxy)) return false;
        const ConvexSeparation3D sep = convex_separation(ConvexProxy3D::point(p, 0.0_r), proxy);
        return sep.valid && sep.separation <= 0.0_r;
    }
    }
}

} // namespace detail

FluidVolumeDef FluidVolumeDef::box(FluidLevel level, const AABB3D& bounds, const FluidMaterial& material, Real fill) {
    FluidVolumeDef d;
    d.level = level;
    d.bounds = bounds;
    d.material = material;
    d.fill = fill;
    return d;
}

FluidVolume3D::FluidVolume3D(const FluidVolumeDef& def, const RVec3& gravity, const FluidQualitySettings& quality) : m_def(def), m_quality(quality) {
    build_frame(gravity);
    switch (m_def.level) {
    case FluidLevel::blocky:
    case FluidLevel::flowing:
        init_cells();
        break;
    case FluidLevel::waves:
        init_waves();
        break;
    case FluidLevel::realistic:
        init_particles();
        break;
    case FluidLevel::still:
        m_still_level = (m_def.fill > 0.0_r ? std::min(m_def.fill, 1.0_r) : 1.0_r) * m_size.y;
        break;
    }
}

void FluidVolume3D::build_frame(const RVec3& gravity) {
    m_g = std::max(norm(gravity), 1e-6_r);
    m_up = !vec3::is_zero(m_def.up) ? unit_or(m_def.up, V3{ 0.0_r, 1.0_r, 0.0_r }) : unit_or(gravity * -1.0_r, V3{ 0.0_r, 1.0_r, 0.0_r });
    const V3 axes[3] = { V3{ 1.0_r, 0.0_r, 0.0_r }, V3{ 0.0_r, 0.0_r, 1.0_r }, V3{ 0.0_r, 1.0_r, 0.0_r } };
    V3 pick = axes[0];
    Real best = 2.0_r;
    for (const V3& a : axes) {
        const Real d = std::abs(vec3::dot(a, m_up));
        if (d < best - 1e-6_r) {
            best = d;
            pick = a;
        }
    }
    m_u = unit_or(pick - m_up * vec3::dot(pick, m_up), V3{ 1.0_r, 0.0_r, 0.0_r });
    m_w = vec3::cross(m_u, m_up);
    const AABB3D& b = m_def.bounds;
    V3 lo{ std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
    V3 hi = lo * -1.0_r;
    for (int c = 0; c < 8; ++c) {
        const V3 p{ (c & 1) ? b.max.x : b.min.x, (c & 2) ? b.max.y : b.min.y, (c & 4) ? b.max.z : b.min.z };
        const V3 l{ vec3::dot(p, m_u), vec3::dot(p, m_up), vec3::dot(p, m_w) };
        lo = vec3::min(lo, l);
        hi = vec3::max(hi, l);
    }
    m_o = m_u * lo.x + m_up * lo.y + m_w * lo.z;
    m_size = hi - lo;
    m_world_bounds = b;
}

V3 FluidVolume3D::to_local(const V3& world) const noexcept {
    const V3 d = world - m_o;
    return V3{ vec3::dot(d, m_u), vec3::dot(d, m_up), vec3::dot(d, m_w) };
}

V3 FluidVolume3D::to_world(const V3& l) const noexcept { return m_o + m_u * l.x + m_up * l.y + m_w * l.z; }

V3 FluidVolume3D::local_dir(const V3& d) const noexcept { return V3{ vec3::dot(d, m_u), vec3::dot(d, m_up), vec3::dot(d, m_w) }; }

V3 FluidVolume3D::world_dir(const V3& l) const noexcept { return m_u * l.x + m_up * l.y + m_w * l.z; }

void FluidVolume3D::set_still_level(Real height_along_up) {
    m_still_level = std::clamp(height_along_up - vec3::dot(m_o, m_up), 0.0_r, m_size.y);
}

Real FluidVolume3D::surface_height(Real a, Real b) const {
    const Real ax = std::abs(m_up.x), ay = std::abs(m_up.y), az = std::abs(m_up.z);
    V3 p{};
    if (ay >= ax && ay >= az) p = V3{ a, 0.0_r, b };
    else if (az >= ax) p = V3{ a, b, 0.0_r };
    else p = V3{ 0.0_r, a, b };
    return surface_height(p);
}

Real FluidVolume3D::surface_height(const V3& point) const {
    const V3 s = surface_point(point);
    return vec3::dot(s, m_up);
}

V3 FluidVolume3D::surface_point(const V3& point) const {
    V3 l = to_local(point);
    const Real lh = l.y;
    l.y = std::numeric_limits<Real>::max();
    Real top = 0.0_r;
    switch (m_def.level) {
    case FluidLevel::still: {
        top = m_still_level;
        if (m_def.still.bob_amplitude > 0.0_r) {
            const Real k = 2.0_r * kPi / std::max(m_def.still.bob_wavelength, 1e-3_r);
            const Real om = 2.0_r * kPi / std::max(m_def.still.bob_period, 1e-3_r);
            top += m_def.still.bob_amplitude * std::sin(om * m_time + k * (l.x * 0.8_r + l.z * 0.6_r));
        }
        break;
    }
    case FluidLevel::waves: {
        std::vector<Real> const& f = heights();
        top = sample_waves(l.x, l.z, f);
        break;
    }
    case FluidLevel::blocky:
    case FluidLevel::flowing: {
        const int i = std::clamp(static_cast<int>(std::floor(l.x / m_cs)), 0, std::max(0, m_nx - 1));
        const int k = std::clamp(static_cast<int>(std::floor(l.z / m_cs)), 0, std::max(0, m_nz - 1));
        const int j = std::clamp(static_cast<int>(std::floor(lh / m_cs)), 0, std::max(0, m_ny - 1));
        top = cell_surface_local(i, j, k, lh);
        break;
    }
    case FluidLevel::realistic: {
        if (m_sx > 0 && m_scell > 0.0_r) {
            const int i = std::clamp(static_cast<int>(std::floor(l.x / m_scell)), 0, m_sx - 1);
            const int k = std::clamp(static_cast<int>(std::floor(l.z / m_scell)), 0, m_sz - 1);
            top = m_sgrid[static_cast<std::size_t>(k) * static_cast<std::size_t>(m_sx) + static_cast<std::size_t>(i)];
        }
        break;
    }
    }
    l.y = top;
    return to_world(l);
}

V3 FluidVolume3D::surface_normal(const V3& point) const {
    const Real e = std::max(0.05_r, (m_def.level == FluidLevel::waves ? m_dx : m_cs));
    const Real h0 = surface_height(point);
    const Real hu = surface_height(point + m_u * e);
    const Real hw = surface_height(point + m_w * e);
    const V3 n = world_dir(V3{ -(hu - h0) / e, 1.0_r, -(hw - h0) / e });
    return unit_or(n, m_up);
}

Real FluidVolume3D::depth_at(const V3& point) const {
    const V3 l = to_local(point);
    if (l.x < 0.0_r || l.z < 0.0_r || l.x > m_size.x || l.z > m_size.z || l.y < 0.0_r || l.y > m_size.y + m_size.y) return -std::numeric_limits<Real>::max();
    if (m_def.level == FluidLevel::realistic) {
        const Real rho = particle_density_at(point);
        if (rho < 0.3_r * m_def.material.density) return -m_h;
    }
    if (m_def.level == FluidLevel::blocky || m_def.level == FluidLevel::flowing) {
        const int i = std::clamp(static_cast<int>(std::floor(l.x / m_cs)), 0, m_nx - 1);
        const int k = std::clamp(static_cast<int>(std::floor(l.z / m_cs)), 0, m_nz - 1);
        const int j = std::clamp(static_cast<int>(std::floor(l.y / m_cs)), 0, m_ny - 1);
        return cell_surface_local(i, j, k, l.y) - l.y;
    }
    const V3 s = surface_point(point);
    return vec3::dot(s - point, m_up);
}

bool FluidVolume3D::inside(const V3& point) const { return depth_at(point) > 0.0_r; }

V3 FluidVolume3D::velocity_at(const V3& point) const {
    V3 v = m_def.current;
    const V3 l = to_local(point);
    switch (m_def.level) {
    case FluidLevel::still:
        break;
    case FluidLevel::waves: {
        const Real vu = sample_waves(l.x, l.z, m_vel_u);
        const Real vw = sample_waves(l.x, l.z, m_vel_w);
        Real su = 0.0_r, sh = 0.0_r, sw = 0.0_r;
        swell_velocity(l.x, l.z, l.y, su, sh, sw);
        v = v + world_dir(V3{ vu + su, sh, vw + sw });
        break;
    }
    case FluidLevel::flowing: {
        int i = 0, j = 0, k = 0;
        if (cell_of(point, i, j, k)) v = v + world_dir(m_cell_vel[cell_index(i, j, k)]);
        break;
    }
    case FluidLevel::blocky: {
        int i = 0, j = 0, k = 0;
        if (!cell_of(point, i, j, k)) break;
        const Real here = static_cast<Real>(m_block[cell_index(i, j, k)]);
        if (here <= 0.0_r) break;
        Real gu = 0.0_r, gw = 0.0_r;
        const int di[4] = { 1, -1, 0, 0 };
        const int dk[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; ++d) {
            const int a = i + di[d], c = k + dk[d];
            if (!valid_cell(a, j, c) || m_solid[cell_index(a, j, c)]) continue;
            const Real there = static_cast<Real>(m_block[cell_index(a, j, c)]);
            const Real drop = here - there;
            gu += static_cast<Real>(di[d]) * drop;
            gw += static_cast<Real>(dk[d]) * drop;
        }
        const Real speed = 1.4_r * m_def.material.fluidity() / 8.0_r;
        Real fall = 0.0_r;
        if (valid_cell(i, j + 1, k) && m_block[cell_index(i, j + 1, k)] > 0) fall = -2.0_r * m_def.material.fluidity();
        v = v + world_dir(V3{ gu * speed, fall, gw * speed });
        break;
    }
    case FluidLevel::realistic:
        v = v + particle_velocity_at(point);
        break;
    }
    return v;
}

Real FluidVolume3D::total_volume() const {
    switch (m_def.level) {
    case FluidLevel::still:
        return m_size.x * m_size.z * m_still_level;
    case FluidLevel::waves: {
        double s = 0.0;
        for (Real d : m_depth) s += static_cast<double>(d);
        return static_cast<Real>(s * static_cast<double>(m_dx) * static_cast<double>(m_dx));
    }
    case FluidLevel::flowing: {
        double s = 0.0;
        for (Real f : m_fill) s += static_cast<double>(f);
        return static_cast<Real>(s * static_cast<double>(m_cs * m_cs * m_cs));
    }
    case FluidLevel::blocky: {
        double s = 0.0;
        for (std::uint8_t b : m_block) s += static_cast<double>(b) / 8.0;
        return static_cast<Real>(s * static_cast<double>(m_cs * m_cs * m_cs));
    }
    case FluidLevel::realistic:
        return static_cast<Real>(m_px.size()) * m_spacing * m_spacing * m_spacing;
    }
    return 0.0_r;
}

void FluidVolume3D::wake() noexcept {
    m_all_dirty = true;
    m_heights_dirty = true;
    std::fill(m_asleep.begin(), m_asleep.end(), std::uint8_t{ 0 });
    std::fill(m_calm.begin(), m_calm.end(), std::uint16_t{ 0 });
}

bool FluidVolume3D::sleeping() const noexcept {
    switch (m_def.level) {
    case FluidLevel::still: return m_def.still.bob_amplitude <= 0.0_r;
    case FluidLevel::realistic: return !m_px.empty() && m_awake_count == 0;
    default: return !m_all_dirty && active_regions() == 0;
    }
}

std::size_t FluidVolume3D::active_regions() const noexcept {
    switch (m_def.level) {
    case FluidLevel::blocky:
    case FluidLevel::flowing: return m_active_list.size();
    case FluidLevel::waves: return m_tile_list.size();
    case FluidLevel::realistic: return m_awake_count;
    case FluidLevel::still: return 0;
    }
    return 0;
}

std::size_t FluidVolume3D::total_regions() const noexcept {
    switch (m_def.level) {
    case FluidLevel::blocky:
    case FluidLevel::flowing: return m_chunk_active.size();
    case FluidLevel::waves: return m_tile_active.size();
    case FluidLevel::realistic: return m_px.size();
    case FluidLevel::still: return 1;
    }
    return 0;
}

void FluidVolume3D::step_level(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats) {
    if (dt <= 0.0_r) return;
    m_last_dt = dt;
    m_time += dt;
    switch (m_def.level) {
    case FluidLevel::still: break;
    case FluidLevel::blocky: step_blocky(pool, dt, stats); break;
    case FluidLevel::flowing: step_flowing(pool, dt, stats); break;
    case FluidLevel::waves: step_waves(pool, dt, stats); break;
    case FluidLevel::realistic: step_particles(pool, dt, stats); break;
    }
    m_heights_dirty = true;
}

const FluidVolume3D::BodyCache& FluidVolume3D::body_cache(const RigidBody3D& body, int samples) {
    std::uint64_t sig = mix(0, static_cast<std::uint64_t>(samples));
    for (const Collider3D& c : body.colliders()) {
        sig = mix(sig, static_cast<std::uint64_t>(c.shape.type));
        sig = mix(sig, real_bits(c.shape.bounding_radius));
        sig = mix(sig, real_bits(c.local_offset.position.x + c.local_offset.position.y * 3.0_r + c.local_offset.position.z * 7.0_r));
        sig = mix(sig, c.is_sensor ? 1u : 0u);
    }
    BodyCache& cache = m_cache[body.id()];
    cache.seen = m_frame;
    if (cache.signature == sig && !cache.collider_start.empty()) return cache;
    cache.signature = sig;
    cache.points.clear();
    cache.collider_start.clear();
    cache.volume = 0.0_r;
    cache.spacing = std::numeric_limits<Real>::max();
    const int n = std::max(2, samples);
    for (const Collider3D& c : body.colliders()) {
        cache.collider_start.push_back(static_cast<std::uint32_t>(cache.points.size()));
        const Shape3D& s = c.shape;
        if (c.is_sensor || s.is_static_only()) continue;
        if (s.type == ShapeType3D::Sphere) {
            cache.volume += analytic_volume(s);
            cache.spacing = std::min(cache.spacing, s.sphere.radius);
            continue;
        }
        const AABB3D box = s.local_aabb;
        const V3 ext = box.max - box.min;
        const Real longest = std::max({ ext.x, ext.y, ext.z, 1e-6_r });
        const Real step = longest / static_cast<Real>(n);
        const int cx = std::max(1, static_cast<int>(std::ceil(ext.x / step - 1e-3_r)));
        const int cy = std::max(1, static_cast<int>(std::ceil(ext.y / step - 1e-3_r)));
        const int cz = std::max(1, static_cast<int>(std::ceil(ext.z / step - 1e-3_r)));
        const V3 cell{ ext.x / static_cast<Real>(cx), ext.y / static_cast<Real>(cy), ext.z / static_cast<Real>(cz) };
        const Real w = cell.x * cell.y * cell.z;
        const std::size_t first = cache.points.size();
        for (int z = 0; z < cz; ++z) {
            for (int y = 0; y < cy; ++y) {
                for (int x = 0; x < cx; ++x) {
                    const V3 p = box.min + V3{ (static_cast<Real>(x) + 0.5_r) * cell.x, (static_cast<Real>(y) + 0.5_r) * cell.y, (static_cast<Real>(z) + 0.5_r) * cell.z };
                    if (!detail::fluid_point_in_shape(s, p)) continue;
                    cache.points.push_back(SamplePoint{ c.local_offset.apply(p), w });
                }
            }
        }
        if (cache.points.size() == first) cache.points.push_back(SamplePoint{ c.local_offset.apply(box.center()), std::max(w, ext.x * ext.y * ext.z) });
        Real total = 0.0_r;
        for (std::size_t q = first; q < cache.points.size(); ++q) total += cache.points[q].weight;
        const Real exact = analytic_volume(s);
        if (exact > 0.0_r && total > 0.0_r) {
            const Real k = exact / total;
            for (std::size_t q = first; q < cache.points.size(); ++q) cache.points[q].weight *= k;
            total = exact;
        }
        cache.volume += total;
        cache.spacing = std::min(cache.spacing, std::min({ cell.x, cell.y, cell.z }));
    }
    cache.collider_start.push_back(static_cast<std::uint32_t>(cache.points.size()));
    if (cache.spacing == std::numeric_limits<Real>::max()) cache.spacing = 0.1_r;
    return cache;
}

FluidForces3D FluidVolume3D::measure(const RigidBody3D& body, const BodyCache& cache, const RVec3& gravity, Real ambient, const RVec3* flow_override) const {
    FluidForces3D out;
    out.body_volume = cache.volume;
    const Transform3D& xf = body.transform();
    Real vsub = 0.0_r;
    V3 centroid{};
    V3 flow{};
    const bool use_ambient = ambient > -1.0e29_r;
    auto depth = [&](const V3& p) {
        if (!use_ambient) return depth_at(p);
        const V3 l = to_local(p);
        if (l.x < 0.0_r || l.z < 0.0_r || l.x > m_size.x || l.z > m_size.z || l.y < 0.0_r) return -std::numeric_limits<Real>::max();
        return ambient - l.y;
    };
    const auto& colliders = body.colliders();
    for (std::size_t ci = 0; ci < colliders.size() && ci + 1 < cache.collider_start.size(); ++ci) {
        const Collider3D& c = colliders[ci];
        if (c.is_sensor || c.shape.is_static_only()) continue;
        if (c.shape.type == ShapeType3D::Sphere) {
            const Transform3D cx = xf.compose(c.local_offset);
            const V3 center = cx.apply(c.shape.sphere.center);
            const Real r = c.shape.sphere.radius;
            const Real d = depth(center);
            if (d <= -r) continue;
            const Real h = std::clamp(d + r, 0.0_r, 2.0_r * r);
            const Real v = kPi * h * h * (3.0_r * r - h) / 3.0_r;
            if (v <= 0.0_r) continue;
            const Real off = 3.0_r * (2.0_r * r - h) * (2.0_r * r - h) / (4.0_r * (3.0_r * r - h));
            const V3 cb = center - m_up * off;
            vsub += v;
            centroid = centroid + cb * v;
            flow = flow + (flow_override ? *flow_override : velocity_at(cb)) * v;
            continue;
        }
        const Real s = cache.spacing;
        for (std::uint32_t q = cache.collider_start[ci]; q < cache.collider_start[ci + 1]; ++q) {
            const V3 p = xf.apply(cache.points[q].local);
            const Real d = depth(p);
            if (d <= -s) continue;
            const Real frac = std::clamp(d / s + 0.5_r, 0.0_r, 1.0_r);
            if (frac <= 0.0_r) continue;
            const Real v = cache.points[q].weight * frac;
            vsub += v;
            centroid = centroid + p * v;
            flow = flow + (flow_override ? *flow_override : velocity_at(p)) * v;
        }
    }
    if (vsub <= 0.0_r) return out;
    const FluidMaterial& m = m_def.material;
    const FluidCouplingSettings& cs = m_def.coupling;
    out.touching = true;
    out.submerged_volume = vsub;
    out.submerged_fraction = cache.volume > 0.0_r ? std::min(1.0_r, vsub / cache.volume) : 1.0_r;
    out.center_of_buoyancy = centroid * (1.0_r / vsub);
    out.fluid_velocity = flow * (1.0_r / vsub);
    if (cs.buoyancy) out.buoyancy = gravity * (-m.density * vsub * cs.buoyancy_scale);
    if (cs.drag) {
        const V3 rel = body.get_velocity_at(out.center_of_buoyancy) - out.fluid_velocity;
        const Real speed = norm(rel);
        const Real len = std::cbrt(vsub);
        const Real area = len * len;
        const Real mass = body.inv_mass() > 0.0_r ? 1.0_r / body.inv_mass() : 0.0_r;
        const Real displaced = std::min(m.density * vsub, mass);
        const Real c = cs.drag_scale * (m.linear_drag * 6.0_r * kPi * m.viscosity * len + 0.5_r * m.density * m.quadratic_drag * area * speed) + cs.linear_damping * displaced;
        out.drag = rel * -c;
        const Real ratio = mass > 0.0_r ? displaced / mass : 1.0_r;
        const Real k = (cs.drag_scale * m.angular_drag * m.viscosity_factor() + cs.angular_damping) * out.submerged_fraction * std::min(1.0_r, 4.0_r * ratio);
        out.angular_drag = body.angular_velocity() * -k;
    }
    return out;
}

bool FluidVolume3D::local_footprint(const RigidBody3D& body, RVec3& lo, RVec3& hi) const {
    lo = V3{ std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
    hi = lo * -1.0_r;
    bool any = false;
    for (const Collider3D& c : body.colliders()) {
        if (c.is_sensor) continue;
        for (int q = 0; q < 8; ++q) {
            const V3 w{ (q & 1) ? c.world_aabb.max.x : c.world_aabb.min.x, (q & 2) ? c.world_aabb.max.y : c.world_aabb.min.y, (q & 4) ? c.world_aabb.max.z : c.world_aabb.min.z };
            const V3 l = to_local(w);
            lo = vec3::min(lo, l);
            hi = vec3::max(hi, l);
            any = true;
        }
    }
    return any;
}

Real FluidVolume3D::ambient_level(const RigidBody3D& body, RVec3* flow) const {
    switch (m_def.level) {
    case FluidLevel::realistic: return particle_ambient(body);
    case FluidLevel::flowing: return cell_ambient(body, flow);
    default: return -1.0e30_r;
    }
}

void FluidWorld3D::couple(Simulation3D& sim, Real dt) {
    const RVec3 g = sim.config().gravity;
    for (auto& vp : m_volumes) {
        FluidVolume3D& v = *vp;
        if (!v.m_def.coupling.buoyancy && !v.m_def.coupling.drag) continue;
        const bool particles = v.m_def.level == FluidLevel::realistic;
        QueryFilter3D f;
        f.include_static = false;
        f.include_kinematic = false;
        f.layers = v.m_def.coupling.layers;
        m_bodies = sim.query_aabb(v.m_world_bounds.fatten(0.05_r), f);
        for (RigidBody3D* b : m_bodies) {
            if (!b || b->inv_mass() <= 0.0_r) continue;
            if (m_cfg.require_buoyancy_flag && !b->has(PhysicsFlags::Buoyancy)) continue;
            V3 ring_flow{};
            bool has_flow = false;
            const Real ambient = v.ambient_level(*b, &ring_flow);
            if (particles && ambient < -1.0e29_r) continue;
            has_flow = v.m_def.level == FluidLevel::flowing && ambient > -1.0e29_r;
            const FluidVolume3D::BodyCache& cache = v.body_cache(*b, m_quality.buoyancy_samples);
            const FluidForces3D fo = v.measure(*b, cache, g, ambient, has_flow ? &ring_flow : nullptr);
            if (!fo.touching) continue;
            ++m_stats.bodies;
            FluidForces3D& acc = m_body_forces[b->id()];
            acc.touching = true;
            acc.body_volume = std::max(acc.body_volume, fo.body_volume);
            const Real prev = acc.submerged_volume;
            acc.submerged_volume += fo.submerged_volume;
            acc.submerged_fraction = std::min(1.0_r, acc.submerged_fraction + fo.submerged_fraction);
            if (acc.submerged_volume > 0.0_r) {
                acc.center_of_buoyancy = (acc.center_of_buoyancy * prev + fo.center_of_buoyancy * fo.submerged_volume) * (1.0_r / acc.submerged_volume);
                acc.fluid_velocity = (acc.fluid_velocity * prev + fo.fluid_velocity * fo.submerged_volume) * (1.0_r / acc.submerged_volume);
            }
            acc.buoyancy = acc.buoyancy + fo.buoyancy;
            acc.drag = acc.drag + fo.drag;
            acc.angular_drag = acc.angular_drag + fo.angular_drag;
            if (!vec3::is_zero(fo.buoyancy)) b->apply_force_at(fo.buoyancy, fo.center_of_buoyancy);
            const Real mass = 1.0_r / b->inv_mass();
            const V3 rel = b->get_velocity_at(fo.center_of_buoyancy) - fo.fluid_velocity;
            const Real speed = norm(rel);
            if (speed > 1e-9_r && !vec3::is_zero(fo.drag)) {
                const Real c = norm(fo.drag) / speed;
                const Real keep = 1.0_r - std::exp(-c * dt / mass);
                b->apply_impulse_at(rel * (-mass * keep), fo.center_of_buoyancy);
            }
            const V3 w = b->angular_velocity();
            const Real wl = norm(w);
            if (wl > 1e-9_r && !vec3::is_zero(fo.angular_drag)) {
                const Real k = norm(fo.angular_drag) / wl;
                const V3 dw = w * -(1.0_r - std::exp(-k * dt));
                const V3 local = b->transform().rotate_inverse(dw);
                const V3 L = b->transform().rotate(vec3::mul(b->inertia(), local));
                b->apply_angular_impulse(L);
            }
        }
    }
}

FluidWorld3D::FluidWorld3D(const Config& cfg) : m_cfg(cfg), m_quality(FluidQualitySettings::preset(cfg.quality)) {
    set_threads(cfg.threads);
}

void FluidWorld3D::set_quality(FluidQuality q) {
    m_cfg.quality = q;
    m_quality = FluidQualitySettings::preset(q);
    for (auto& v : m_volumes) v->m_quality = m_quality;
}

void FluidWorld3D::set_threads(int threads) {
    if (threads <= 0) threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    m_cfg.threads = threads;
    m_pool.resize(threads);
}

FluidVolume3D& FluidWorld3D::add_volume(const FluidVolumeDef& def) {
    m_volumes.push_back(std::make_unique<FluidVolume3D>(def, m_cfg.gravity, m_quality));
    m_volumes.back()->m_id = m_next_id++;
    return *m_volumes.back();
}

FluidVolume3D& FluidWorld3D::add_volume(const AABB3D& bounds, FluidLevel level, const FluidMaterial& material, Real fill) {
    return add_volume(FluidVolumeDef::box(level, bounds, material, fill));
}

void FluidWorld3D::remove_volume(FluidVolume3D& volume) {
    m_volumes.erase(std::remove_if(m_volumes.begin(), m_volumes.end(), [&volume](const std::unique_ptr<FluidVolume3D>& v) { return v.get() == &volume; }), m_volumes.end());
}

void FluidWorld3D::schedule(FluidVolume3D& v, Real dt, Real& use_dt, bool& run) {
    run = true;
    use_dt = dt;
    if (v.m_frozen) {
        run = false;
        return;
    }
    const Real dist = m_has_focus ? aabb_distance(v.m_world_bounds, m_focus) : 0.0_r;
    if (m_cfg.freeze_distance > 0.0_r && dist > m_cfg.freeze_distance) {
        run = false;
        v.m_lod_accum = 0.0_r;
        return;
    }
    const bool distant = (m_cfg.lod_distance > 0.0_r && dist > m_cfg.lod_distance) || (!v.m_visible && m_cfg.step_offscreen_less);
    if (!distant) {
        use_dt = dt + v.m_lod_accum;
        v.m_lod_accum = 0.0_r;
        v.m_lod_counter = 0;
        return;
    }
    v.m_lod_accum += dt;
    if (++v.m_lod_counter < std::max(1, m_cfg.lod_interval)) {
        run = false;
        return;
    }
    use_dt = v.m_lod_accum;
    v.m_lod_accum = 0.0_r;
    v.m_lod_counter = 0;
}

void FluidWorld3D::step(Simulation3D& sim, Real dt) {
    if (dt <= 0.0_r) return;
    const auto t0 = std::chrono::steady_clock::now();
    m_stats = FluidStats3D{};
    m_stats.volumes = m_volumes.size();
    m_body_forces.clear();
    const RVec3 g = sim.config().gravity;
    for (auto& vp : m_volumes) {
        FluidVolume3D& v = *vp;
        ++v.m_frame;
        v.m_g = std::max(norm(g), 1e-6_r);
        if (v.m_def.level == FluidLevel::realistic) {
            v.gather_bodies(sim);
            continue;
        }
        if (!v.m_def.coupling.push_fluid || v.m_def.level == FluidLevel::still || v.m_def.level == FluidLevel::blocky) continue;
        v.begin_push();
        QueryFilter3D f;
        f.include_static = false;
        f.layers = v.m_def.coupling.layers;
        m_bodies = sim.query_aabb(v.m_world_bounds.fatten(0.05_r), f);
        for (RigidBody3D* b : m_bodies) {
            if (!b) continue;
            const FluidVolume3D::BodyCache& cache = v.body_cache(*b, m_quality.buoyancy_samples);
            v.push_fluid(*b, cache, dt);
        }
    }
    for (auto& vp : m_volumes) {
        FluidVolume3D& v = *vp;
        Real use = dt;
        bool run = true;
        schedule(v, dt, use, run);
        if (run) {
            ++m_stats.stepped;
            if (v.m_def.level == FluidLevel::realistic) {
                v.m_last_dt = use;
                v.m_time += use;
                v.step_particles(m_pool, use, m_stats);
                v.m_near.clear();
            } else {
                v.step_level(m_pool, use, m_stats);
            }
        }
        if (v.sleeping()) ++m_stats.sleeping;
        m_stats.active_regions += v.active_regions();
        m_stats.total_regions += v.total_regions();
        m_stats.particles += v.particle_count();
        m_stats.awake_particles += v.awake_particles();
    }
    couple(sim, dt);
    for (auto& vp : m_volumes) {
        FluidVolume3D& v = *vp;
        if (v.m_frame % 120 != 0) continue;
        for (auto it = v.m_cache.begin(); it != v.m_cache.end();) {
            if (it->second.seen + 240 < v.m_frame) it = v.m_cache.erase(it);
            else ++it;
        }
    }
    m_stats.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void FluidWorld3D::step(Real dt) {
    if (dt <= 0.0_r) return;
    const auto t0 = std::chrono::steady_clock::now();
    m_stats = FluidStats3D{};
    m_stats.volumes = m_volumes.size();
    for (auto& vp : m_volumes) {
        FluidVolume3D& v = *vp;
        ++v.m_frame;
        v.m_g = std::max(norm(m_cfg.gravity), 1e-6_r);
        Real use = dt;
        bool run = true;
        schedule(v, dt, use, run);
        if (run) {
            ++m_stats.stepped;
            if (v.m_def.level == FluidLevel::realistic) {
                v.m_near.clear();
                v.m_last_dt = use;
                v.m_time += use;
                v.step_particles(m_pool, use, m_stats);
            } else {
                if (v.m_def.coupling.push_fluid && (v.m_def.level == FluidLevel::waves || v.m_def.level == FluidLevel::flowing)) v.begin_push();
                v.step_level(m_pool, use, m_stats);
            }
        }
        if (v.sleeping()) ++m_stats.sleeping;
        m_stats.active_regions += v.active_regions();
        m_stats.total_regions += v.total_regions();
        m_stats.particles += v.particle_count();
        m_stats.awake_particles += v.awake_particles();
    }
    m_stats.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

FluidVolume3D* FluidWorld3D::volume_at(const RVec3& point) {
    for (auto& v : m_volumes) {
        if (v->inside(point)) return v.get();
    }
    return nullptr;
}

const FluidVolume3D* FluidWorld3D::volume_at(const RVec3& point) const {
    for (const auto& v : m_volumes) {
        if (v->inside(point)) return v.get();
    }
    return nullptr;
}

Real FluidWorld3D::surface_height(const RVec3& point) const {
    Real best = -std::numeric_limits<Real>::max();
    for (const auto& v : m_volumes) {
        const V3 l = v->to_local(point);
        if (l.x < 0.0_r || l.z < 0.0_r || l.x > v->m_size.x || l.z > v->m_size.z) continue;
        best = std::max(best, v->surface_height(point));
    }
    return best;
}

Real FluidWorld3D::surface_height(Real a, Real b) const {
    Real best = -std::numeric_limits<Real>::max();
    for (const auto& v : m_volumes) best = std::max(best, v->surface_height(a, b));
    return best;
}

Real FluidWorld3D::depth_at(const RVec3& point) const {
    Real best = -std::numeric_limits<Real>::max();
    for (const auto& v : m_volumes) best = std::max(best, v->depth_at(point));
    return best;
}

RVec3 FluidWorld3D::velocity_at(const RVec3& point) const {
    const FluidVolume3D* v = volume_at(point);
    return v ? v->velocity_at(point) : RVec3{};
}

bool FluidWorld3D::inside(const RVec3& point) const { return volume_at(point) != nullptr; }

FluidForces3D FluidWorld3D::forces_on(const RigidBody3D& body) {
    FluidForces3D acc;
    const AABB3D* box = nullptr;
    AABB3D merged{};
    bool any = false;
    for (const Collider3D& c : body.colliders()) {
        merged = any ? AABB3D::merge(merged, c.world_aabb) : c.world_aabb;
        any = true;
    }
    if (any) box = &merged;
    for (auto& vp : m_volumes) {
        FluidVolume3D& v = *vp;
        if (box && !box->overlaps(v.m_world_bounds)) continue;
        V3 ring_flow{};
        const Real ambient = v.ambient_level(body, &ring_flow);
        if (v.m_def.level == FluidLevel::realistic && ambient < -1.0e29_r) continue;
        const bool has_flow = v.m_def.level == FluidLevel::flowing && ambient > -1.0e29_r;
        const FluidVolume3D::BodyCache& cache = v.body_cache(body, m_quality.buoyancy_samples);
        const FluidForces3D fo = v.measure(body, cache, m_cfg.gravity, ambient, has_flow ? &ring_flow : nullptr);
        if (!fo.touching) continue;
        const Real prev = acc.submerged_volume;
        acc.touching = true;
        acc.body_volume = std::max(acc.body_volume, fo.body_volume);
        acc.submerged_volume += fo.submerged_volume;
        acc.submerged_fraction = std::min(1.0_r, acc.submerged_fraction + fo.submerged_fraction);
        acc.center_of_buoyancy = (acc.center_of_buoyancy * prev + fo.center_of_buoyancy * fo.submerged_volume) * (1.0_r / acc.submerged_volume);
        acc.fluid_velocity = (acc.fluid_velocity * prev + fo.fluid_velocity * fo.submerged_volume) * (1.0_r / acc.submerged_volume);
        acc.buoyancy = acc.buoyancy + fo.buoyancy;
        acc.drag = acc.drag + fo.drag;
        acc.angular_drag = acc.angular_drag + fo.angular_drag;
    }
    return acc;
}

const FluidForces3D* FluidWorld3D::body_forces(const RigidBody3D& body) const {
    auto it = m_body_forces.find(body.id());
    return it == m_body_forces.end() ? nullptr : &it->second;
}

} // namespace physics
} // namespace fizmo
