#include "fizmo_library.hpp"
#include "simulation_3d.hpp"

namespace fizmo {
namespace physics {

bool Simulation3D::is_active(const RigidBody3D& b) noexcept {
    if (!b.is_awake()) return false;
    if (b.inv_mass() > 0.0_r) return true;
    return !vec3::is_zero(b.linear_velocity()) || !vec3::is_zero(b.angular_velocity());
}

bool Simulation3D::responds(const RigidBody3D& a, const RigidBody3D& b) noexcept {
    return a.has(PhysicsFlags::CollisionResponse) && b.has(PhysicsFlags::CollisionResponse) &&
          !a.has(PhysicsFlags::Intangible) && !b.has(PhysicsFlags::Intangible);
}

bool Simulation3D::wants_events(const RigidBody3D& a, const RigidBody3D& b, bool sensor) noexcept {
    if (a.has(PhysicsFlags::ContactEvents) || b.has(PhysicsFlags::ContactEvents)) return true;
    return sensor && (a.has(PhysicsFlags::SensorEvents) || b.has(PhysicsFlags::SensorEvents));
}

void Simulation3D::apply_impulse(RigidBody3D& a, RigidBody3D& b, const RVec3& rA, const RVec3& rB, const RVec3& P) {
    a.set_linear_velocity(a.linear_velocity() - P * a.inv_mass());
    a.set_angular_velocity(a.angular_velocity() - a.apply_inv_inertia(vec3::cross(rA, P)));
    b.set_linear_velocity(b.linear_velocity() + P * b.inv_mass());
    b.set_angular_velocity(b.angular_velocity() + b.apply_inv_inertia(vec3::cross(rB, P)));
}

Real Simulation3D::effective_inv_mass(const RigidBody3D& a, const RigidBody3D& b, const RVec3& rA, const RVec3& rB, const RVec3& u) {
    RVec3 ra_u = vec3::cross(rA, u), rb_u = vec3::cross(rB, u);
    return a.inv_mass() + b.inv_mass()
         + vec3::dot(ra_u, a.apply_inv_inertia(ra_u))
         + vec3::dot(rb_u, b.apply_inv_inertia(rb_u));
}


void Simulation3D::narrowphase() {
    m_manifolds.clear();
    m_manifolds.reserve(m_pairs.size());
    m_touching.clear();
    for (auto& [a, b] : m_pairs) { generate_contacts(a, b); }
}

void Simulation3D::generate_contacts(RigidBody3D* a, RigidBody3D* b) {
    const auto& colA = a->colliders();
    const auto& colB = b->colliders();
    const Real reach = m_cfg.speculative_distance + sweep_margin(*a, *b);

    for (std::size_t i = 0; i < colA.size(); ++i) {
        for (std::size_t j = 0; j < colB.size(); ++j) {
            const Collider3D& ca = colA[i];
            const Collider3D& cb = colB[j];
            if (!ca.world_aabb.fatten(reach).overlaps(cb.world_aabb)) continue;
            if (!ca.filter.should_collide(cb.filter)) continue;
            Transform3D wa = a->transform().compose(ca.local_offset);
            Transform3D wb = b->transform().compose(cb.local_offset);
            ContactManifold3D mf;
            mf.body_a = a;
            mf.body_b = b;
            mf.collider_a = i;
            mf.collider_b = j;
            const bool sensor = ca.is_sensor || cb.is_sensor || !responds(*a, *b);
            if (!collide_shapes(ca.shape, wa, cb.shape, wb, mf, sensor ? 0.0_r : reach)) continue;
            mf.friction    = combined_friction(ca, cb);
            mf.restitution = combined_restitution(ca, cb);
            mf.restitution_threshold = std::min(ca.material.restitution_threshold, cb.material.restitution_threshold);

            for (int p = 0; p < mf.point_count; ++p) {
                ContactPoint3D& cp = mf.points[p];
                RVec3 half = mf.normal * (cp.separation * 0.5_r);
                cp.local_anchor_a = a->transform().apply_inverse(cp.position - half);
                cp.local_anchor_b = b->transform().apply_inverse(cp.position + half);
            }

            int deepest = 0;
            for (int p = 1; p < mf.point_count; ++p) if (mf.points[p].separation < mf.points[deepest].separation) deepest = p;
            CachedContact& touch = m_touching[key_of(mf)];
            touch.body_a = a;
            touch.body_b = b;
            touch.sensor = sensor;
            touch.events = wants_events(*a, *b, sensor);
            touch.touching = mf.points[deepest].separation <= 0.0_r;
            touch.normal = mf.normal;
            touch.point  = mf.points[deepest].position;
            if (sensor) continue;
            if (m_pre_solve) { if (!m_pre_solve(*a, *b, mf)) continue; }
            m_manifolds.push_back(mf);
        }
    }
}




void Simulation3D::warm_start() {
    for (auto& vc : m_constraints) {
        for (int p = 0; p < vc.point_count; ++p) {
            const auto& vcp = vc.points[p];
            RVec3 P = vc.normal * vcp.normal_impulse + vc.tangent1 * vcp.tangent_impulse[0] + vc.tangent2 * vcp.tangent_impulse[1];
            apply_impulse(*vc.body_a, *vc.body_b, vcp.rA, vcp.rB, P);
        }
    }
}

void Simulation3D::solve_velocity_constraints(bool reverse) {
    const std::size_t count = m_constraints.size();

    for (std::size_t ci = 0; ci < count; ++ci) {
        auto& vc = m_constraints[reverse ? count - 1 - ci : ci];
        RigidBody3D& a = *vc.body_a;
        RigidBody3D& b = *vc.body_b;

        for (int pi = 0; pi < vc.point_count; ++pi) {
            auto& vcp = vc.points[reverse ? vc.point_count - 1 - pi : pi];
            RVec3 dv = point_velocity(b, vcp.rB) - point_velocity(a, vcp.rA);
            Real old1 = vcp.tangent_impulse[0], old2 = vcp.tangent_impulse[1];
            Real new1 = old1 - vec3::dot(dv, vc.tangent1) * vcp.tangent_mass[0];
            Real new2 = old2 - vec3::dot(dv, vc.tangent2) * vcp.tangent_mass[1];
            Real max_friction = vc.friction * vcp.normal_impulse;
            Real mag_sq = new1 * new1 + new2 * new2;

            if (mag_sq > max_friction * max_friction) {
                Real scale = (mag_sq > 0.0_r) ? max_friction / std::sqrt(mag_sq) : 0.0_r;
                new1 *= scale;
                new2 *= scale;
            }

            vcp.tangent_impulse[0] = new1;
            vcp.tangent_impulse[1] = new2;
            apply_impulse(a, b, vcp.rA, vcp.rB, vc.tangent1 * (new1 - old1) + vc.tangent2 * (new2 - old2));
        }

        if (vc.point_count >= 2 && solve_normal_block(vc)) continue;

        for (int pi = 0; pi < vc.point_count; ++pi) {
            auto& vcp = vc.points[reverse ? vc.point_count - 1 - pi : pi];
            Real vn = vec3::dot(point_velocity(b, vcp.rB) - point_velocity(a, vcp.rA), vc.normal);
            Real lambda = -(vn - vcp.velocity_bias) * vcp.normal_mass;
            Real new_impulse = std::max(vcp.normal_impulse + lambda, 0.0_r);
            lambda = new_impulse - vcp.normal_impulse;
            vcp.normal_impulse = new_impulse;
            apply_impulse(a, b, vcp.rA, vcp.rB, vc.normal * lambda);
        }
    }
}

bool Simulation3D::solve_normal_block(VelocityConstraint3D& vc) {
    constexpr int N = ContactManifold3D::MAX_POINTS;
    const int n = vc.point_count;
    RigidBody3D& a = *vc.body_a;
    RigidBody3D& b = *vc.body_b;
    Real bvec[N], acc[N], x[N];
    Real scale = 0.0_r;

    for (int i = 0; i < n; ++i) {
        const auto& c = vc.points[i];
        acc[i] = c.normal_impulse;
        bvec[i] = vec3::dot(point_velocity(b, c.rB) - point_velocity(a, c.rA), vc.normal) - c.velocity_bias;
        scale = std::max(scale, vc.normal_block[i][i]);
    }

    if (!(scale > 0.0_r)) return false;
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) bvec[i] -= vc.normal_block[i][j] * acc[j];
    const Real reg = 1e-6_r * scale;
    Real b_max = 0.0_r;
    for (int i = 0; i < n; ++i) b_max = std::max(b_max, std::abs(bvec[i]));
    const Real vel_tol = 1e-9_r * (1.0_r + b_max); // m/s
    const Real imp_tol = vel_tol / scale;      // N*s
    const int subsets = 1 << n;

    for (int size = n; size >= 0; --size) {
        for (int mask = subsets - 1; mask >= 0; --mask) {
            int bits = 0;
            for (int i = 0; i < n; ++i) bits += (mask >> i) & 1;
            if (bits != size) continue;
            int idx[N], m = 0;
            for (int i = 0; i < n; ++i) if (mask & (1 << i)) idx[m++] = i;
            Real A[N][N + 1];

            for (int r = 0; r < m; ++r) {
                for (int c = 0; c < m; ++c) A[r][c] = vc.normal_block[idx[r]][idx[c]] + (r == c ? reg : 0.0_r);
                A[r][m] = -bvec[idx[r]];
            }

            bool ok = true;

            for (int col = 0; col < m && ok; ++col) { 
                int piv = col;
                for (int r = col + 1; r < m; ++r) if (std::abs(A[r][col]) > std::abs(A[piv][col])) piv = r;
                if (std::abs(A[piv][col]) <= 1e-300_r) { ok = false; break; }
                if (piv != col) for (int c = 0; c <= m; ++c) std::swap(A[piv][c], A[col][c]);

                for (int r = col + 1; r < m; ++r) {
                    Real f = A[r][col] / A[col][col];
                    for (int c = col; c <= m; ++c) A[r][c] -= f * A[col][c];
                }
            }

            if (!ok) continue;
            for (int i = 0; i < n; ++i) x[i] = 0.0_r;

            for (int r = m - 1; r >= 0; --r) {
                Real v = A[r][m];
                for (int c = r + 1; c < m; ++c) v -= A[r][c] * x[idx[c]];
                x[idx[r]] = v / A[r][r];
            }

            for (int i = 0; i < n && ok; ++i) {
                if (mask & (1 << i)) {
                    if (x[i] < -imp_tol) ok = false;
                } else {
                    Real w = bvec[i];
                    for (int j = 0; j < n; ++j) w += vc.normal_block[i][j] * x[j];
                    if (w < -vel_tol) ok = false;
                }
            }

            if (!ok) continue;
            RVec3 P_lin{}, ang_a{}, ang_b{};

            for (int i = 0; i < n; ++i) {
                x[i] = std::max(x[i], 0.0_r);
                RVec3 P = vc.normal * (x[i] - acc[i]);
                P_lin += P;
                ang_a += vec3::cross(vc.points[i].rA, P);
                ang_b += vec3::cross(vc.points[i].rB, P);
                vc.points[i].normal_impulse = x[i];
            }

            a.set_linear_velocity(a.linear_velocity() - P_lin * a.inv_mass());
            a.set_angular_velocity(a.angular_velocity() - a.apply_inv_inertia(ang_a));
            b.set_linear_velocity(b.linear_velocity() + P_lin * b.inv_mass());
            b.set_angular_velocity(b.angular_velocity() + b.apply_inv_inertia(ang_b));
            return true;
        }
    }

    return false;
}


bool Simulation3D::solve_position_constraints(bool reverse) {
    Real min_separation = 0.0_r;
    const std::size_t count = m_manifolds.size();

    for (std::size_t mi = 0; mi < count; ++mi) {
        auto& mf = m_manifolds[reverse ? count - 1 - mi : mi];
        RigidBody3D& a = *mf.body_a;
        RigidBody3D& b = *mf.body_b;

        for (int pi = 0; pi < mf.point_count; ++pi) {
            const int p = reverse ? mf.point_count - 1 - pi : pi;
            RVec3 pA = a.transform().apply(mf.points[p].local_anchor_a);
            RVec3 pB = b.transform().apply(mf.points[p].local_anchor_b);
            Real separation = vec3::dot(pB - pA, mf.normal);
            min_separation = std::min(min_separation, separation);
            Real C = std::clamp(m_cfg.baumgarte_factor * (separation + m_cfg.slop), -m_cfg.max_correction, 0.0_r);
            if (C == 0.0_r) continue;
            RVec3 point = (pA + pB) * 0.5_r;
            RVec3 rA = point - a.world_center();
            RVec3 rB = point - b.world_center();
            Real K = effective_inv_mass(a, b, rA, rB, mf.normal);
            Real impulse = (K > 0.0_r) ? -C / K : 0.0_r;
            RVec3 P = mf.normal * impulse;
            RVec3 dwA = a.apply_inv_inertia(vec3::cross(rA, P)) * -1.0_r;
            RVec3 dwB = b.apply_inv_inertia(vec3::cross(rB, P));
            a.apply_position_correction(P * -a.inv_mass(), dwA);
            b.apply_position_correction(P *  b.inv_mass(), dwB);
        }
    }

    return min_separation >= -3.0_r * m_cfg.slop;
}




void Simulation3D::flush_pending_removals() {
    while (!m_pending_removals.empty()) {
        std::vector<RigidBody3D*> pending;
        pending.swap(m_pending_removals);
        std::sort(pending.begin(), pending.end(), std::less<RigidBody3D*>());
        pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
        for (RigidBody3D* b : pending) remove_body_now(b);
    }
}


Real Simulation3D::combined_friction(const Collider3D& a, const Collider3D& b) const {
    const CombineMode m = resolve_combine(a.material.friction_combine, b.material.friction_combine, m_cfg.friction_combine);
    if (m == CombineMode::Custom && m_friction_combiner) return m_friction_combiner(a, b);
    return combine_values(m, a.material.friction, b.material.friction);
}

Real Simulation3D::combined_restitution(const Collider3D& a, const Collider3D& b) const {
    const CombineMode m = resolve_combine(a.material.restitution_combine, b.material.restitution_combine, m_cfg.restitution_combine);
    if (m == CombineMode::Custom && m_restitution_combiner) return m_restitution_combiner(a, b);
    return combine_values(m, a.material.restitution, b.material.restitution);
}

namespace {

using V3 = RVec3;

std::uint64_t pair_key(std::int32_t a, std::int32_t b) noexcept {
    const std::uint32_t lo = static_cast<std::uint32_t>(std::min(a, b));
    const std::uint32_t hi = static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

std::uint64_t body_pair_key(const RigidBody3D* a, const RigidBody3D* b) noexcept {
    std::uint64_t x = a->id(), y = b->id();
    if (x > y) std::swap(x, y);
    return x * 0x9E3779B97F4A7C15ULL ^ (y + 0x632BE59BD9B4E019ULL + (x << 6) + (x >> 2));
}

Real angle_between(const RQuat& a, const RQuat& b) noexcept {
    const Real d = std::abs(a.w() * b.w() + a.x() * b.x() + a.y() * b.y() + a.z() * b.z());
    return 2.0_r * std::acos(std::min(1.0_r, d));
}

V3 hadamard(const V3& a, const V3& b) noexcept {
    return { a.x * b.x, a.y * b.y, a.z * b.z };
}

Real side_k(const SolverBody3D& s, bool responds, const V3& lin, const V3& r) noexcept {
    if (!responds) return 0.0_r;
    const V3 rn = vec3::cross(r, lin);
    return vec3::dot(lin, hadamard(s.inv_mass, lin)) + vec3::dot(rn, vec3::mul(s.inv_inertia, rn));
}

Real angular_k(const SolverBody3D& s, bool responds, const V3& axis) noexcept {
    if (!responds) return 0.0_r;
    return vec3::dot(axis, vec3::mul(s.inv_inertia, axis));
}

}

void Simulation3D::remove_body(RigidBody3D& body) {
    if (m_locked) {
        m_pending_removals.push_back(&body);
        return;
    }
    remove_body_now(&body);
    flush_pending_removals();
}

void Simulation3D::step(Real dt) {
    if (dt <= 0.0_r) return;
    rebase_if_needed();
    if (m_cfg.solver == Solver3D::SoftStep) {
        step_substep(dt);
    } else {
        const int n = std::max(1, m_cfg.substeps);
        const Real h = dt / static_cast<Real>(n);
        for (int i = 0; i < n; ++i) step_substep(h);
    }
    for (auto& c : m_cloths) c->step(*this, dt);
}

bool Simulation3D::body_sweeps(const RigidBody3D& b) const noexcept {
    if (!is_dynamic(b)) return false;
    if (b.continuous_mode() == ContinuousMode::Off) return false;
    if (b.continuous_mode() == ContinuousMode::On) return true;
    if (!m_cfg.continuous) return false;
    if (b.quality() == BodyQuality::Ultra) return true;
    return m_cfg.continuous_all || has_flag(b.flags(), PhysicsFlags::Bullet);
}

bool Simulation3D::sweeps(const RigidBody3D& b) const noexcept {
    return body_sweeps(b);
}

Real Simulation3D::sweep_margin(const RigidBody3D& a, const RigidBody3D& b) const noexcept {
    if (!sweeps(a) && !sweeps(b)) return 0.0_r;
    Real travel = fast::length(b.linear_velocity() - a.linear_velocity()) * m_substep_dt;
    if (m_cfg.rotation_aware_ccd) {
        auto spin = [this](const RigidBody3D& body) {
            Real r = 0.0_r;
            for (const Collider3D& c : body.colliders()) r = std::max(r, std::min(c.shape.bounding_radius + vec3::length(c.local_offset.position), 1e3_r));
            return fast::length(body.angular_velocity()) * r * m_substep_dt;
        };
        if (sweeps(a)) travel += spin(a);
        if (sweeps(b)) travel += spin(b);
    }
    return std::min(travel, std::max(0.0_r, m_cfg.max_continuous_distance));
}

bool Simulation3D::can_collide(const RigidBody3D& a, const RigidBody3D& b) const noexcept {
    if (!layers_can_collide(a.collision_layer(), a.collision_mask(), b.collision_layer(), b.collision_mask())) return false;
    if (!a.can_collide_with(b)) return false;
    return !joined_without_collision(&a, &b);
}

bool Simulation3D::joined_without_collision(const RigidBody3D* a, const RigidBody3D* b) const {
    if (m_joints.empty()) return false;
    for (const auto& j : m_joints) {
        if (j->collide_connected() || !j->enabled()) continue;
        if ((j->body_a() == a && j->body_b() == b) || (j->body_a() == b && j->body_b() == a)) return true;
    }
    return false;
}

AABB3D Simulation3D::proxy_box(const RigidBody3D& b) const {
    return b.compute_body_aabb();
}

void Simulation3D::sync_tree(bool for_query) {
    for (auto& bp : m_bodies) {
        RigidBody3D& b = *bp;
        const bool want = is_enabled(b) && !b.colliders().empty() &&
            (has_flag(b.flags(), PhysicsFlags::BroadphaseActive) || has_flag(b.flags(), PhysicsFlags::CollisionResponse)) &&
            b.collision_layer() != 0;
        if (!want) {
            if (b.m_proxy >= 0) {
                m_dtree.remove(b.m_proxy);
                b.m_proxy = -1;
            }
            continue;
        }
        b.ensure_mass_uptodate();
        if (for_query) b.synchronize_colliders();
        const AABB3D tight = proxy_box(b);
        if (b.m_proxy < 0) b.m_proxy = m_dtree.insert(tight.fatten(m_cfg.broadphase_margin), &b);
        else if (!for_query && !b.is_awake() && is_dynamic(b)) continue;
        AABB3D displaced = tight;
        if (!for_query && body_sweeps(b)) {
            const V3 d = b.linear_velocity() * m_substep_dt;
            const Real reach = std::max(0.0_r, m_cfg.max_continuous_distance);
            const V3 move{ std::clamp(d.x, -reach, reach), std::clamp(d.y, -reach, reach), std::clamp(d.z, -reach, reach) };
            displaced = AABB3D(tight.min + move, tight.max + move);
        }
        m_dtree.move(b.m_proxy, tight, m_cfg.broadphase_margin, displaced);
    }
}

void Simulation3D::tree_broadphase() {
    m_pairs.clear();
    synchronize_all_colliders();
    sync_tree(false);
    std::size_t moved = 0;
    m_dtree.for_each_leaf([&](std::int32_t proxy, RigidBody3D* body) {
        if (!m_dtree.moved(proxy)) return;
        ++moved;
        m_dtree.query(m_dtree.box(proxy), [&](std::int32_t other, RigidBody3D* ob) {
            if (other == proxy) return true;
            if (!is_dynamic(*body) && !is_dynamic(*ob)) return true;
            const std::uint64_t key = pair_key(proxy, other);
            if (m_tree_pair_index.count(key)) return true;
            m_tree_pair_index.emplace(key, m_tree_pairs.size());
            m_tree_pairs.push_back({ std::min(proxy, other), std::max(proxy, other) });
            return true;
        });
    });
    m_stats.proxies_moved = moved;
    m_stats.layer_rejects = 0;
    for (std::size_t i = 0; i < m_tree_pairs.size();) {
        const auto pr = m_tree_pairs[i];
        bool keep = m_dtree.valid(pr.first) && m_dtree.valid(pr.second) && m_dtree.box(pr.first).overlaps(m_dtree.box(pr.second));
        if (!keep) {
            m_tree_pair_index.erase(pair_key(pr.first, pr.second));
            if (i + 1 != m_tree_pairs.size()) {
                m_tree_pairs[i] = m_tree_pairs.back();
                m_tree_pair_index[pair_key(m_tree_pairs[i].first, m_tree_pairs[i].second)] = i;
            }
            m_tree_pairs.pop_back();
            continue;
        }
        RigidBody3D* a = m_dtree.item(pr.first);
        RigidBody3D* b = m_dtree.item(pr.second);
        ++i;
        if (!layers_can_collide(a->collision_layer(), a->collision_mask(), b->collision_layer(), b->collision_mask())) {
            ++m_stats.layer_rejects;
            continue;
        }
        if (!is_active(*a) && !is_active(*b)) continue;
        if (!is_dynamic(*a) && !is_dynamic(*b)) continue;
        if (!can_collide(*a, *b)) continue;
        if (std::less<const RigidBody3D*>()(b, a)) std::swap(a, b);
        m_pairs.push_back({ a, b });
    }
    m_dtree.for_each_leaf([&](std::int32_t proxy, RigidBody3D*) { m_dtree.clear_moved(proxy); });
    if (m_cfg.deterministic) {
        for (auto& p : m_pairs) {
            if (p.first->id() > p.second->id()) std::swap(p.first, p.second);
        }
        std::sort(m_pairs.begin(), m_pairs.end(), [](const auto& x, const auto& y) {
            if (x.first->id() != y.first->id()) return x.first->id() < y.first->id();
            return x.second->id() < y.second->id();
        });
    }
}

void Simulation3D::broadphase() {
    m_pairs.clear();
    synchronize_all_colliders();
    m_body_ptrs.clear();
    m_body_aabbs.clear();
    m_body_ptrs.reserve(m_bodies.size());
    m_body_aabbs.reserve(m_bodies.size());

    for (auto& bp : m_bodies) {
        RigidBody3D& b = *bp;
        if (!is_enabled(b)) continue;
        if (!has_flag(b.flags(), PhysicsFlags::BroadphaseActive) && !has_flag(b.flags(), PhysicsFlags::CollisionResponse)) continue;
        if (b.collision_layer() == 0) continue;
        b.ensure_mass_uptodate();
        m_body_ptrs.push_back(&b);
        AABB3D box = b.compute_body_aabb().fatten(m_cfg.broadphase_margin);
        if (sweeps(b)) {
            const RVec3 d = b.linear_velocity() * m_substep_dt;
            const Real reach = std::max(0.0_r, m_cfg.max_continuous_distance);
            const RVec3 move{ std::clamp(d.x, -reach, reach), std::clamp(d.y, -reach, reach), std::clamp(d.z, -reach, reach) };
            box = AABB3D::merge(box, AABB3D(box.min + move, box.max + move));
        }
        m_body_aabbs.push_back(box);
    }

    if (m_body_ptrs.size() < 2) return;
    m_tree.rebuild(world_bounds_of(m_cfg), m_body_ptrs.data(), m_body_aabbs.data(), m_body_ptrs.size());

    m_tree.find_pairs([this](RigidBody3D* a, RigidBody3D* b) {
        if (!layers_can_collide(a->collision_layer(), a->collision_mask(), b->collision_layer(), b->collision_mask())) return;
        if (!is_active(*a) && !is_active(*b)) return;
        if (!is_dynamic(*a) && !is_dynamic(*b)) return;
        if (!can_collide(*a, *b)) return;
        if (std::less<const RigidBody3D*>()(b, a)) std::swap(a, b);
        m_pairs.push_back({a, b});
    });
}

void Simulation3D::compute_pair(RigidBody3D* a, RigidBody3D* b, PairResult& out) {
    out.manifolds.clear();
    out.touches.clear();
    out.reused = false;
    if (m_cfg.reuse_contacts && try_reuse(a, b, out)) return;
    const auto& colA = a->colliders();
    const auto& colB = b->colliders();
    const Real reach = m_cfg.speculative_distance + sweep_margin(*a, *b);
    std::vector<ContactManifold3D> found;
    for (std::size_t i = 0; i < colA.size(); ++i) {
        for (std::size_t j = 0; j < colB.size(); ++j) {
            const Collider3D& ca = colA[i];
            const Collider3D& cb = colB[j];
            if (!ca.filter.should_collide(cb.filter)) continue;
            if (!ca.world_aabb.fatten(reach).overlaps(cb.world_aabb)) continue;
            const Transform3D wa = a->transform().compose(ca.local_offset);
            const Transform3D wb = b->transform().compose(cb.local_offset);
            const bool sensor = ca.is_sensor || cb.is_sensor || !responds(*a, *b);
            const Real margin = sensor ? 0.0_r : reach;
            found.clear();
            bool hit = false;
            if (m_cfg.hull_collisions == ConvexMethod3D::Gjk && ca.shape.type == ShapeType3D::Hull && cb.shape.type == ShapeType3D::Hull) {
                ConvexProxy3D pa, pb;
                ContactManifold3D mf;
                if (make_convex_proxy(ca.shape, wa, pa) && make_convex_proxy(cb.shape, wb, pb) && collide_convex(pa, pb, mf, margin)) {
                    found.push_back(mf);
                    hit = true;
                }
            } else {
                hit = collide_shapes_multi(ca.shape, wa, cb.shape, wb, margin, found);
            }
            if (!hit) continue;
            for (ContactManifold3D& mf : found) {
                mf.body_a = a;
                mf.body_b = b;
                mf.collider_a = i;
                mf.collider_b = j;
                mf.friction = combined_friction(ca, cb);
                mf.restitution = combined_restitution(ca, cb);
                mf.restitution_threshold = std::min(ca.material.restitution_threshold, cb.material.restitution_threshold);
                for (int p = 0; p < mf.point_count; ++p) {
                    ContactPoint3D& cp = mf.points[p];
                    const V3 half = mf.normal * (cp.separation * 0.5_r);
                    cp.local_anchor_a = a->transform().apply_inverse(cp.position - half);
                    cp.local_anchor_b = b->transform().apply_inverse(cp.position + half);
                }
                int deepest = 0;
                for (int p = 1; p < mf.point_count; ++p) {
                    if (mf.points[p].separation < mf.points[deepest].separation) deepest = p;
                }
                CachedContact touch;
                touch.body_a = a;
                touch.body_b = b;
                touch.sensor = sensor;
                touch.events = wants_events(*a, *b, sensor);
                touch.touching = mf.points[deepest].separation <= 0.0_r;
                touch.normal = mf.normal;
                touch.point = mf.points[deepest].position;
                Key key = key_of(mf);
                key.feature = mf.feature;
                out.touches.push_back({ key, touch });
                if (!sensor) out.manifolds.push_back(mf);
            }
        }
    }
}

bool Simulation3D::try_reuse(RigidBody3D* a, RigidBody3D* b, PairResult& out) {
    if (sweeps(*a) || sweeps(*b)) return false;
    auto it = m_reuse.find(body_pair_key(a, b));
    if (it == m_reuse.end()) return false;
    ReuseEntry& e = it->second;
    if (e.step + 1 != m_step_count) return false;
    if (!e.manifolds.empty() && (e.manifolds[0].body_a != a || e.manifolds[0].body_b != b)) return false;
    if (!e.touches.empty() && (e.touches[0].second.body_a != a || e.touches[0].second.body_b != b)) return false;
    const Transform3D& xa = a->transform();
    const Transform3D& xb = b->transform();
    const V3 rel = xa.apply_inverse(xb.position);
    const RQuat rq = fast::mul(xa.orientation.conjugate(), xb.orientation);
    if (vec3::length(rel - e.relative_position) > m_cfg.reuse_distance) return false;
    if (angle_between(rq, e.relative_orientation) > m_cfg.reuse_angle) return false;
    for (std::size_t m = 0; m < e.manifolds.size(); ++m) {
        const V3 n = xa.rotate(e.local_normals[m]);
        for (int p = 0; p < e.manifolds[m].point_count; ++p) {
            const ContactPoint3D& cp = e.manifolds[m].points[p];
            const V3 d = xb.apply(cp.local_anchor_b) - xa.apply(cp.local_anchor_a);
            if (vec3::length(d - n * vec3::dot(d, n)) > m_cfg.reuse_distance) return false;
        }
    }
    out.manifolds = e.manifolds;
    for (std::size_t m = 0; m < out.manifolds.size(); ++m) {
        ContactManifold3D& mf = out.manifolds[m];
        mf.normal = xa.rotate(e.local_normals[m]);
        for (int p = 0; p < mf.point_count; ++p) {
            ContactPoint3D& cp = mf.points[p];
            const V3 pa = xa.apply(cp.local_anchor_a);
            const V3 pb = xb.apply(cp.local_anchor_b);
            cp.separation = vec3::dot(pb - pa, mf.normal);
            cp.position = (pa + pb) * 0.5_r;
            cp.normal_impulse = 0.0_r;
            cp.friction_impulse = V3{};
        }
    }
    out.touches = e.touches;
    out.reused = true;
    e.step = m_step_count;
    return true;
}

void Simulation3D::remember_pair(RigidBody3D* a, RigidBody3D* b, const PairResult& result) {
    if (!m_cfg.reuse_contacts || result.reused) return;
    if (result.manifolds.empty() && result.touches.empty()) {
        m_reuse.erase(body_pair_key(a, b));
        return;
    }
    ReuseEntry& e = m_reuse[body_pair_key(a, b)];
    const Transform3D& xa = a->transform();
    e.relative_position = xa.apply_inverse(b->transform().position);
    e.relative_orientation = fast::mul(xa.orientation.conjugate(), b->transform().orientation);
    e.manifolds = result.manifolds;
    e.local_normals.resize(e.manifolds.size());
    for (std::size_t i = 0; i < e.manifolds.size(); ++i) e.local_normals[i] = xa.rotate_inverse(e.manifolds[i].normal);
    e.touches = result.touches;
    e.step = m_step_count;
}

void Simulation3D::commit_pair(PairResult& result) {
    for (auto& t : result.touches) m_touching[t.first] = t.second;
    for (ContactManifold3D& mf : result.manifolds) {
        if (m_pre_solve && !m_pre_solve(*mf.body_a, *mf.body_b, mf)) continue;
        m_manifolds.push_back(mf);
    }
    if (result.reused) ++m_stats.reused_manifolds;
}

void Simulation3D::step_substep(Real dt) {
    m_substep_dt = dt;
    m_locked = true;
    ++m_step_count;
    m_pool.resize(std::max(1, m_cfg.threads));
    m_stats = Simulation3DStats{};
    m_stats.threads = m_pool.size();
    const bool soft = m_cfg.solver == Solver3D::SoftStep;

    for (std::size_t i = 0; i < m_bodies.size(); ++i) m_bodies[i]->m_solver_index = i;

    if (!soft) {
        for (auto& bp : m_bodies) {
            RigidBody3D& b = *bp;
            if (!is_enabled(b) || !b.is_awake()) continue;
            b.ensure_mass_uptodate();
            b.integrate_forces(dt, m_cfg.gravity);
            if (m_cfg.gyroscopic) b.integrate_gyroscopic(dt);
        }
    }

    if (m_cfg.broadphase == Broadphase3D::Octree) broadphase();
    else tree_broadphase();

    m_manifolds.clear();
    m_manifolds.reserve(m_pairs.size());
    m_touching.clear();
    m_pair_results.resize(m_pairs.size());
    const bool parallel_np = m_pool.size() > 1 && m_pairs.size() >= m_cfg.parallel_threshold;
    if (parallel_np) {
        const std::size_t chunk = 16;
        const std::size_t chunks = (m_pairs.size() + chunk - 1) / chunk;
        m_pool.parallel_for(chunks, [&](std::size_t c) {
            const std::size_t end = std::min(m_pairs.size(), (c + 1) * chunk);
            for (std::size_t i = c * chunk; i < end; ++i) {
                PairResult& r = m_pair_results[i];
                compute_pair(m_pairs[i].first, m_pairs[i].second, r);
            }
        });
    } else {
        for (std::size_t i = 0; i < m_pairs.size(); ++i) compute_pair(m_pairs[i].first, m_pairs[i].second, m_pair_results[i]);
    }
    for (std::size_t i = 0; i < m_pairs.size(); ++i) {
        commit_pair(m_pair_results[i]);
        remember_pair(m_pairs[i].first, m_pairs[i].second, m_pair_results[i]);
    }
    if (m_reuse.size() > 4 * m_pairs.size() + 64) {
        for (auto it = m_reuse.begin(); it != m_reuse.end();) {
            if (it->second.step + 1 < m_step_count) it = m_reuse.erase(it);
            else ++it;
        }
    }

    build_islands_and_wake();
    build_solver_bodies();
    build_velocity_constraints(dt);
    prepare_joints(dt);
    build_islands();

    m_pool.parallel_for(m_island_list.size(), [&](std::size_t i) {
        if (soft) solve_island_soft(m_island_list[i], dt);
        else solve_island_classic(m_island_list[i], dt);
    });

    store_impulses();
    finish_joints(dt);

    for (auto& bp : m_bodies) {
        RigidBody3D& b = *bp;
        if (!is_enabled(b) || !b.is_awake() || is_dynamic(b)) continue;
        b.integrate_velocities(dt);
    }

    for (auto& bp : m_bodies) {
        RigidBody3D& b = *bp;
        if (!is_enabled(b)) continue;
        b.synchronize_colliders();
        b.clear_forces();
    }

    if (m_cfg.allow_sleeping) sleep_islands(dt);
    update_contact_cache();

    if (m_post_solve) {
        for (auto& mf : m_manifolds) {
            if (mf.body_a && mf.body_b) m_post_solve(*mf.body_a, *mf.body_b, mf);
        }
    }

    m_stats.bodies = m_bodies.size();
    for (auto& bp : m_bodies) {
        if (bp->is_awake()) ++m_stats.awake;
    }
    m_stats.proxies = m_dtree.size();
    m_stats.pairs = m_pairs.size();
    m_stats.manifolds = m_manifolds.size();
    m_stats.islands = m_island_list.size();
    m_stats.joints = m_joints.size();
    m_stats.tree_height = m_dtree.height();

    m_locked = false;
    dispatch_events();
    flush_pending_removals();
    cleanup_bodies();
}

void Simulation3D::build_islands_and_wake() {
    const std::size_t n = m_bodies.size();
    m_islands.reset(n);
    m_carried.clear();

    auto link = [this](RigidBody3D* a, RigidBody3D* b) {
        const bool da = is_dynamic(*a), db = is_dynamic(*b);
        if (da && db) {
            m_islands.unite(a->m_solver_index, b->m_solver_index);
        } else if (da && is_active(*b)) {
            a->wake();
        } else if (db && is_active(*a)) {
            b->wake();
        }
    };

    for (auto& [key, c] : m_contact_cache) {
        if (m_touching.count(key)) continue;
        if (!is_enabled(*c.body_a) || !is_enabled(*c.body_b)) continue;
        if (is_active(*c.body_a) || is_active(*c.body_b)) continue;
        m_carried.push_back(key);
        if (!c.sensor) link(c.body_a, c.body_b);
    }
    if (m_cfg.deterministic) {
        std::sort(m_carried.begin(), m_carried.end(), [](const Key& x, const Key& y) {
            if (x.body_a->id() != y.body_a->id()) return x.body_a->id() < y.body_a->id();
            if (x.body_b->id() != y.body_b->id()) return x.body_b->id() < y.body_b->id();
            if (x.collider_a != y.collider_a) return x.collider_a < y.collider_a;
            if (x.collider_b != y.collider_b) return x.collider_b < y.collider_b;
            return x.feature < y.feature;
        });
    }

    for (auto& mf : m_manifolds) link(mf.body_a, mf.body_b);
    for (auto& j : m_joints) {
        if (!j->enabled()) continue;
        RigidBody3D* a = j->body_a();
        RigidBody3D* b = j->body_b();
        if (!a || !b) continue;
        if (!is_enabled(*a) || !is_enabled(*b)) continue;
        if (is_dynamic(*a) && is_dynamic(*b)) m_islands.unite(a->m_solver_index, b->m_solver_index);
        else if (is_dynamic(*a) && is_active(*b)) a->wake();
        else if (is_dynamic(*b) && is_active(*a)) b->wake();
    }
    std::vector<char> island_awake(n, 0);

    for (std::size_t i = 0; i < n; ++i) {
        const RigidBody3D& b = *m_bodies[i];
        if (is_enabled(b) && is_dynamic(b) && b.is_awake()) island_awake[m_islands.find(i)] = 1;
    }

    for (std::size_t i = 0; i < n; ++i) {
        RigidBody3D& b = *m_bodies[i];
        if (is_enabled(b) && is_dynamic(b) && !b.is_awake() && island_awake[m_islands.find(i)]) b.wake();
    }
}

void Simulation3D::build_solver_bodies() {
    const std::size_t n = m_bodies.size();
    m_solver.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        RigidBody3D& b = *m_bodies[i];
        SolverBody3D& s = m_solver[i];
        s.body = &b;
        s.v = b.linear_velocity();
        s.w = b.angular_velocity();
        s.dynamic = is_enabled(b) && is_dynamic(b);
        if (s.dynamic) {
            const Real im = b.inv_mass();
            s.inv_mass = { b.has(PhysicsFlags::LockPositionX) ? 0.0_r : im, b.has(PhysicsFlags::LockPositionY) ? 0.0_r : im, b.has(PhysicsFlags::LockPositionZ) ? 0.0_r : im };
            s.inv_inertia = b.inv_inertia_world();
            s.center = b.world_center();
        } else {
            s.inv_mass = V3{};
            s.inv_inertia = RMat3{};
            s.center = b.world_center();
        }
        s.contact_impulses = s.dynamic && b.has(PhysicsFlags::ContactImpulses);
        s.joint_impulses = s.dynamic && b.has(PhysicsFlags::JointImpulses);
    }
}

void Simulation3D::build_velocity_constraints(Real dt) {
    m_constraints.clear();
    m_constraints.reserve(m_manifolds.size());
    const Real match_sq = m_cfg.warm_start_match_distance * m_cfg.warm_start_match_distance;

    for (std::size_t mi = 0; mi < m_manifolds.size(); ++mi) {
        const auto& mf = m_manifolds[mi];
        RigidBody3D* a = mf.body_a;
        RigidBody3D* b = mf.body_b;
        VelocityConstraint3D vc;
        vc.body_a = a;
        vc.body_b = b;
        vc.index_a = a->m_solver_index;
        vc.index_b = b->m_solver_index;
        const SolverBody3D& sa = m_solver[vc.index_a];
        const SolverBody3D& sb = m_solver[vc.index_b];
        vc.responds_a = sa.contact_impulses;
        vc.responds_b = sb.contact_impulses;
        vc.normal = mf.normal;
        const Collider3D& ca = a->colliders()[mf.collider_a];
        const Collider3D& cb = b->colliders()[mf.collider_b];
        const V3 aniso_a{ ca.material.anisotropic_x, ca.material.anisotropic_y, ca.material.anisotropic_z };
        const V3 aniso_b{ cb.material.anisotropic_x, cb.material.anisotropic_y, cb.material.anisotropic_z };
        const bool anisotropic = aniso_a.x != 1.0_r || aniso_a.y != 1.0_r || aniso_a.z != 1.0_r || aniso_b.x != 1.0_r || aniso_b.y != 1.0_r || aniso_b.z != 1.0_r;
        if (anisotropic) {
            const bool use_a = aniso_a.x != 1.0_r || aniso_a.y != 1.0_r || aniso_a.z != 1.0_r;
            const Transform3D xf = use_a ? a->transform().compose(ca.local_offset) : b->transform().compose(cb.local_offset);
            V3 t1 = xf.rotate({ 1.0_r, 0.0_r, 0.0_r });
            t1 = t1 - vc.normal * vec3::dot(t1, vc.normal);
            if (vec3::length_squared(t1) < 1e-12_r) {
                t1 = xf.rotate({ 0.0_r, 0.0_r, 1.0_r });
                t1 = t1 - vc.normal * vec3::dot(t1, vc.normal);
            }
            if (vec3::length_squared(t1) < 1e-12_r) vec3::orthonormal_basis(mf.normal, vc.tangent1, vc.tangent2);
            else {
                vc.tangent1 = vec3::normalize(t1);
                vc.tangent2 = vec3::cross(vc.normal, vc.tangent1);
            }
            auto scale_along = [](const Transform3D& x, const V3& s, const V3& t) {
                const V3 l = x.rotate_inverse(t);
                return std::sqrt(s.x * s.x * l.x * l.x + s.y * s.y * l.y * l.y + s.z * s.z * l.z * l.z);
            };
            const Transform3D xa = a->transform().compose(ca.local_offset);
            const Transform3D xb = b->transform().compose(cb.local_offset);
            vc.friction = mf.friction * scale_along(xa, aniso_a, vc.tangent1) * scale_along(xb, aniso_b, vc.tangent1);
            vc.friction2 = mf.friction * scale_along(xa, aniso_a, vc.tangent2) * scale_along(xb, aniso_b, vc.tangent2);
        } else {
            vec3::orthonormal_basis(mf.normal, vc.tangent1, vc.tangent2);
            vc.friction = mf.friction;
            vc.friction2 = mf.friction;
        }
        vc.rolling_friction = std::max(ca.material.rolling_friction, cb.material.rolling_friction);
        vc.spinning_friction = std::max(ca.material.spinning_friction, cb.material.spinning_friction);
        vc.restitution = mf.restitution;
        vc.restitution_threshold = mf.restitution_threshold;
        vc.point_count = mf.point_count;
        vc.manifold_index = mi;
        const V3 cA = sa.center;
        const V3 cB = sb.center;
        const CachedContact* old = nullptr;

        if (m_cfg.warm_starting) {
            Key key = key_of(mf);
            key.feature = mf.feature;
            auto it = m_contact_cache.find(key);
            if (it != m_contact_cache.end() && !it->second.sensor) old = &it->second;
        }

        for (int p = 0; p < mf.point_count; ++p) {
            auto& vcp = vc.points[p];
            vcp.rA = mf.points[p].position - cA;
            vcp.rB = mf.points[p].position - cB;
            const Real kn = side_k(sa, vc.responds_a, vc.normal, vcp.rA) + side_k(sb, vc.responds_b, vc.normal, vcp.rB);
            const Real k1 = side_k(sa, vc.responds_a, vc.tangent1, vcp.rA) + side_k(sb, vc.responds_b, vc.tangent1, vcp.rB);
            const Real k2 = side_k(sa, vc.responds_a, vc.tangent2, vcp.rA) + side_k(sb, vc.responds_b, vc.tangent2, vcp.rB);
            vcp.normal_mass = kn > 0.0_r ? 1.0_r / kn : 0.0_r;
            vcp.tangent_mass[0] = k1 > 0.0_r ? 1.0_r / k1 : 0.0_r;
            vcp.tangent_mass[1] = k2 > 0.0_r ? 1.0_r / k2 : 0.0_r;
            const V3 va = sa.v + vec3::cross(sa.w, vcp.rA);
            const V3 vb = sb.v + vec3::cross(sb.w, vcp.rB);
            const Real vn_rel = vec3::dot(vb - va, vc.normal);
            vc.relative_normal_velocity[p] = vn_rel;
            const Real sep = mf.points[p].separation;
            vc.base_separation[p] = sep;
            vcp.velocity_bias = (sep > 0.0_r) ? -sep / dt : 0.0_r;
            if (vn_rel < -vc.restitution_threshold && vn_rel * dt + std::max(sep, 0.0_r) < 0.0_r) {
                vcp.velocity_bias = std::max(vcp.velocity_bias, -vc.restitution * vn_rel);
            }

            for (int q = 0; q <= p; ++q) {
                const auto& o = vc.points[q];
                const V3 ra_p = vec3::cross(vcp.rA, vc.normal), rb_p = vec3::cross(vcp.rB, vc.normal);
                const V3 ra_q = vec3::cross(o.rA, vc.normal), rb_q = vec3::cross(o.rB, vc.normal);
                Real k = 0.0_r;
                if (vc.responds_a) k += vec3::dot(vc.normal, hadamard(sa.inv_mass, vc.normal)) + vec3::dot(ra_p, vec3::mul(sa.inv_inertia, ra_q));
                if (vc.responds_b) k += vec3::dot(vc.normal, hadamard(sb.inv_mass, vc.normal)) + vec3::dot(rb_p, vec3::mul(sb.inv_inertia, rb_q));
                vc.normal_block[p][q] = vc.normal_block[q][p] = k;
            }

            if (old) {
                Real best = match_sq;
                for (int q = 0; q < old->point_count; ++q) {
                    const Real d = vec3::length_squared(old->local_anchor_a[q] - mf.points[p].local_anchor_a);
                    if (d <= best) {
                        best = d;
                        vcp.normal_impulse = old->normal_impulse[q];
                        vcp.tangent_impulse[0] = vec3::dot(old->friction_impulse[q], vc.tangent1);
                        vcp.tangent_impulse[1] = vec3::dot(old->friction_impulse[q], vc.tangent2);
                    }
                }
            }
        }
        if (vc.spinning_friction > 0.0_r) {
            const Real k = angular_k(sa, vc.responds_a, vc.normal) + angular_k(sb, vc.responds_b, vc.normal);
            vc.spin_mass = k > 0.0_r ? 1.0_r / k : 0.0_r;
        }
        if (vc.rolling_friction > 0.0_r) {
            const Real k1 = angular_k(sa, vc.responds_a, vc.tangent1) + angular_k(sb, vc.responds_b, vc.tangent1);
            const Real k2 = angular_k(sa, vc.responds_a, vc.tangent2) + angular_k(sb, vc.responds_b, vc.tangent2);
            vc.roll_mass[0] = k1 > 0.0_r ? 1.0_r / k1 : 0.0_r;
            vc.roll_mass[1] = k2 > 0.0_r ? 1.0_r / k2 : 0.0_r;
        }

        m_constraints.push_back(vc);
    }
}

void Simulation3D::prepare_joints(Real dt) {
    const SoftParams joint_soft = SoftParams::make(m_cfg.joint_hertz, m_cfg.joint_damping_ratio, dt / std::max(1, m_cfg.soft_substeps));
    const bool soft = m_cfg.solver == Solver3D::SoftStep;
    for (auto& jp : m_joints) {
        Joint3D& j = *jp;
        j.has_a = j.body_a() != nullptr;
        j.has_b = j.body_b() != nullptr;
        if (!j.enabled()) continue;
        if (j.has_a) j.solver_a = j.body_a()->m_solver_index;
        if (j.has_b) j.solver_b = j.body_b()->m_solver_index;
        SolverBody3D* sa = j.has_a ? &m_solver[j.solver_a] : nullptr;
        SolverBody3D* sb = j.has_b ? &m_solver[j.solver_b] : nullptr;
        const Real h = soft ? dt / std::max(1, m_cfg.soft_substeps) : dt;
        j.prepare(sa, sb, h, m_cfg.joint_baumgarte, joint_soft, soft);
    }
}

void Simulation3D::finish_joints(Real dt) {
    for (auto& jp : m_joints) {
        if (jp->enabled()) jp->finish(dt);
    }
}

void Simulation3D::build_islands() {
    const std::size_t n = m_bodies.size();
    m_island_list.clear();
    m_body_island.assign(n, std::numeric_limits<std::size_t>::max());
    std::vector<std::size_t> root_island(n, std::numeric_limits<std::size_t>::max());
    for (std::size_t i = 0; i < n; ++i) {
        RigidBody3D& b = *m_bodies[i];
        if (!is_enabled(b) || !is_dynamic(b) || !b.is_awake()) continue;
        const std::size_t r = m_islands.find(i);
        if (root_island[r] == std::numeric_limits<std::size_t>::max()) {
            root_island[r] = m_island_list.size();
            m_island_list.emplace_back();
        }
        Island& isl = m_island_list[root_island[r]];
        isl.bodies.push_back(i);
        m_body_island[i] = root_island[r];
        const int vi = b.velocity_iterations() > 0 ? b.velocity_iterations() : quality_iterations(b.quality(), m_cfg.velocity_iters);
        const int pi = b.position_iterations() > 0 ? b.position_iterations() : quality_iterations(b.quality(), m_cfg.position_iters);
        isl.velocity_iterations = std::max(isl.velocity_iterations, vi);
        isl.position_iterations = std::max(isl.position_iterations, pi);
    }
    for (std::size_t c = 0; c < m_constraints.size(); ++c) {
        VelocityConstraint3D& vc = m_constraints[c];
        std::size_t isl = m_body_island[vc.index_a];
        if (isl == std::numeric_limits<std::size_t>::max()) isl = m_body_island[vc.index_b];
        if (isl == std::numeric_limits<std::size_t>::max()) continue;
        vc.island = isl;
        m_island_list[isl].constraints.push_back(c);
    }
    for (auto& jp : m_joints) {
        Joint3D& j = *jp;
        if (!j.enabled()) continue;
        std::size_t isl = j.has_a ? m_body_island[j.solver_a] : std::numeric_limits<std::size_t>::max();
        if (isl == std::numeric_limits<std::size_t>::max() && j.has_b) isl = m_body_island[j.solver_b];
        if (isl == std::numeric_limits<std::size_t>::max()) continue;
        j.island = isl;
        m_island_list[isl].joints.push_back(&j);
    }
}

void Simulation3D::warm_start_island(Island& island) {
    if (!m_cfg.warm_starting) return;
    for (Joint3D* j : island.joints) j->warm_start(j->has_a ? &m_solver[j->solver_a] : nullptr, j->has_b ? &m_solver[j->solver_b] : nullptr);
    for (std::size_t c : island.constraints) {
        VelocityConstraint3D& vc = m_constraints[c];
        SolverBody3D& A = m_solver[vc.index_a];
        SolverBody3D& B = m_solver[vc.index_b];
        for (int p = 0; p < vc.point_count; ++p) {
            const auto& vcp = vc.points[p];
            const V3 P = vc.normal * vcp.normal_impulse + vc.tangent1 * vcp.tangent_impulse[0] + vc.tangent2 * vcp.tangent_impulse[1];
            if (vc.responds_a) A.apply(P * -1.0_r, vec3::cross(vcp.rA, P) * -1.0_r);
            if (vc.responds_b) B.apply(P, vec3::cross(vcp.rB, P));
        }
    }
}

void Simulation3D::solve_contact(VelocityConstraint3D& vc, bool reverse, bool use_bias, bool soft, Real inv_h) {
    SolverBody3D& A = m_solver[vc.index_a];
    SolverBody3D& B = m_solver[vc.index_b];
    const bool ra = vc.responds_a, rb = vc.responds_b;
    auto apply = [&](const V3& P, const V3& rA, const V3& rB) {
        if (ra) A.apply(P * -1.0_r, vec3::cross(rA, P) * -1.0_r);
        if (rb) B.apply(P, vec3::cross(rB, P));
    };
    Real total_normal = 0.0_r;
    for (int i = 0; i < vc.point_count; ++i) total_normal += vc.points[i].normal_impulse;

    for (int pi = 0; pi < vc.point_count; ++pi) {
        auto& vcp = vc.points[reverse ? vc.point_count - 1 - pi : pi];
        const V3 dv = (B.v + vec3::cross(B.w, vcp.rB)) - (A.v + vec3::cross(A.w, vcp.rA));
        const Real old1 = vcp.tangent_impulse[0], old2 = vcp.tangent_impulse[1];
        Real new1 = old1 - vec3::dot(dv, vc.tangent1) * vcp.tangent_mass[0];
        Real new2 = old2 - vec3::dot(dv, vc.tangent2) * vcp.tangent_mass[1];
        const Real max1 = vc.friction * vcp.normal_impulse;
        const Real max2 = vc.friction2 * vcp.normal_impulse;
        if (max1 <= 0.0_r || max2 <= 0.0_r) {
            new1 = std::clamp(new1, -std::max(max1, 0.0_r), std::max(max1, 0.0_r));
            new2 = std::clamp(new2, -std::max(max2, 0.0_r), std::max(max2, 0.0_r));
        } else {
            const Real e = (new1 * new1) / (max1 * max1) + (new2 * new2) / (max2 * max2);
            if (e > 1.0_r) {
                const Real s = 1.0_r / std::sqrt(e);
                new1 *= s;
                new2 *= s;
            }
        }
        vcp.tangent_impulse[0] = new1;
        vcp.tangent_impulse[1] = new2;
        apply(vc.tangent1 * (new1 - old1) + vc.tangent2 * (new2 - old2), vcp.rA, vcp.rB);
    }

    auto angular = [&](const V3& axis, Real mass, Real& acc, Real limit) {
        if (mass <= 0.0_r) return;
        const Real rel = vec3::dot(B.w - A.w, axis);
        const Real old = acc;
        acc = std::clamp(old - rel * mass, -limit, limit);
        const Real d = acc - old;
        if (d == 0.0_r) return;
        if (ra) A.w -= vec3::mul(A.inv_inertia, axis * d);
        if (rb) B.w += vec3::mul(B.inv_inertia, axis * d);
    };
    if (vc.spinning_friction > 0.0_r) angular(vc.normal, vc.spin_mass, vc.spin_impulse, vc.spinning_friction * total_normal);
    if (vc.rolling_friction > 0.0_r) {
        angular(vc.tangent1, vc.roll_mass[0], vc.roll_impulse[0], vc.rolling_friction * total_normal);
        angular(vc.tangent2, vc.roll_mass[1], vc.roll_impulse[1], vc.rolling_friction * total_normal);
    }

    if (!soft && vc.point_count >= 2 && solve_normal_block_solver(vc)) return;

    const SoftParams contact_soft = soft ? SoftParams::make(m_cfg.contact_hertz, m_cfg.contact_damping_ratio, 1.0_r / inv_h) : SoftParams{};
    const ContactManifold3D& mf = m_manifolds[vc.manifold_index];
    for (int pi = 0; pi < vc.point_count; ++pi) {
        const int p = reverse ? vc.point_count - 1 - pi : pi;
        auto& vcp = vc.points[p];
        const Real vn = vec3::dot((B.v + vec3::cross(B.w, vcp.rB)) - (A.v + vec3::cross(A.w, vcp.rA)), vc.normal);
        Real lambda;
        if (soft) {
            const V3 pa = vc.body_a->transform().apply(mf.points[p].local_anchor_a);
            const V3 pb = vc.body_b->transform().apply(mf.points[p].local_anchor_b);
            const Real s = vec3::dot(pb - pa, vc.normal) + m_cfg.slop;
            Real bias = 0.0_r, mass_scale = 1.0_r, impulse_scale = 0.0_r;
            if (s > 0.0_r) bias = s * inv_h;
            else if (use_bias) {
                bias = std::max(contact_soft.bias_rate * s, -m_cfg.max_push_speed);
                mass_scale = contact_soft.mass_scale;
                impulse_scale = contact_soft.impulse_scale;
            }
            lambda = -vcp.normal_mass * mass_scale * (vn + bias) - impulse_scale * vcp.normal_impulse;
        } else {
            lambda = -(vn - vcp.velocity_bias) * vcp.normal_mass;
        }
        const Real new_impulse = std::max(vcp.normal_impulse + lambda, 0.0_r);
        lambda = new_impulse - vcp.normal_impulse;
        vcp.normal_impulse = new_impulse;
        apply(vc.normal * lambda, vcp.rA, vcp.rB);
    }
}

bool Simulation3D::solve_normal_block_solver(VelocityConstraint3D& vc) {
    constexpr int N = ContactManifold3D::MAX_POINTS;
    const int n = vc.point_count;
    SolverBody3D& a = m_solver[vc.index_a];
    SolverBody3D& b = m_solver[vc.index_b];
    Real bvec[N], acc[N], x[N];
    Real scale = 0.0_r;
    for (int i = 0; i < n; ++i) {
        const auto& c = vc.points[i];
        acc[i] = c.normal_impulse;
        bvec[i] = vec3::dot((b.v + vec3::cross(b.w, c.rB)) - (a.v + vec3::cross(a.w, c.rA)), vc.normal) - c.velocity_bias;
        scale = std::max(scale, vc.normal_block[i][i]);
    }
    if (!(scale > 0.0_r)) return false;
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) bvec[i] -= vc.normal_block[i][j] * acc[j];
    const Real reg = 1e-6_r * scale;
    Real b_max = 0.0_r;
    for (int i = 0; i < n; ++i) b_max = std::max(b_max, std::abs(bvec[i]));
    const Real vel_tol = 1e-9_r * (1.0_r + b_max);
    const Real imp_tol = vel_tol / scale;
    const int subsets = 1 << n;
    for (int size = n; size >= 0; --size) {
        for (int mask = subsets - 1; mask >= 0; --mask) {
            int bits = 0;
            for (int i = 0; i < n; ++i) bits += (mask >> i) & 1;
            if (bits != size) continue;
            int idx[N], m = 0;
            for (int i = 0; i < n; ++i) if (mask & (1 << i)) idx[m++] = i;
            Real A[N][N + 1];
            for (int r = 0; r < m; ++r) {
                for (int c = 0; c < m; ++c) A[r][c] = vc.normal_block[idx[r]][idx[c]] + (r == c ? reg : 0.0_r);
                A[r][m] = -bvec[idx[r]];
            }
            bool ok = true;
            for (int col = 0; col < m && ok; ++col) {
                int piv = col;
                for (int r = col + 1; r < m; ++r) if (std::abs(A[r][col]) > std::abs(A[piv][col])) piv = r;
                if (std::abs(A[piv][col]) <= 1e-300_r) {
                    ok = false;
                    break;
                }
                if (piv != col) for (int c = 0; c <= m; ++c) std::swap(A[piv][c], A[col][c]);
                for (int r = col + 1; r < m; ++r) {
                    const Real f = A[r][col] / A[col][col];
                    for (int c = col; c <= m; ++c) A[r][c] -= f * A[col][c];
                }
            }
            if (!ok) continue;
            for (int i = 0; i < n; ++i) x[i] = 0.0_r;
            for (int r = m - 1; r >= 0; --r) {
                Real v = A[r][m];
                for (int c = r + 1; c < m; ++c) v -= A[r][c] * x[idx[c]];
                x[idx[r]] = v / A[r][r];
            }
            for (int i = 0; i < n && ok; ++i) {
                if (mask & (1 << i)) {
                    if (x[i] < -imp_tol) ok = false;
                } else {
                    Real w = bvec[i];
                    for (int j = 0; j < n; ++j) w += vc.normal_block[i][j] * x[j];
                    if (w < -vel_tol) ok = false;
                }
            }
            if (!ok) continue;
            V3 P_lin{}, ang_a{}, ang_b{};
            for (int i = 0; i < n; ++i) {
                x[i] = std::max(x[i], 0.0_r);
                const V3 P = vc.normal * (x[i] - acc[i]);
                P_lin += P;
                ang_a += vec3::cross(vc.points[i].rA, P);
                ang_b += vec3::cross(vc.points[i].rB, P);
                vc.points[i].normal_impulse = x[i];
            }
            if (vc.responds_a) a.apply(P_lin * -1.0_r, ang_a * -1.0_r);
            if (vc.responds_b) b.apply(P_lin, ang_b);
            return true;
        }
    }
    return false;
}

void Simulation3D::solve_contacts(Island& island, bool use_bias, bool soft, Real inv_h) {
    const bool reverse = (island.flip++ & 1) != 0;
    const std::size_t count = island.constraints.size();
    for (std::size_t k = 0; k < count; ++k) {
        VelocityConstraint3D& vc = m_constraints[island.constraints[reverse ? count - 1 - k : k]];
        solve_contact(vc, reverse, use_bias, soft, inv_h);
    }
}

void Simulation3D::write_back(Island& island) {
    for (std::size_t i : island.bodies) {
        RigidBody3D& b = *m_bodies[i];
        const SolverBody3D& s = m_solver[i];
        b.m_linear_velocity = s.v;
        b.m_angular_velocity = s.w;
    }
}

void Simulation3D::integrate_island(Island& island, Real dt) {
    for (std::size_t i : island.bodies) m_bodies[i]->integrate_velocities(dt);
}

void Simulation3D::position_correct_island(Island& island) {
    for (int it = 0; it < island.position_iterations; ++it) {
        Real min_separation = 0.0_r;
        const bool reverse = (it & 1) != 0;
        const std::size_t count = island.constraints.size();
        for (std::size_t k = 0; k < count; ++k) {
            const VelocityConstraint3D& vc = m_constraints[island.constraints[reverse ? count - 1 - k : k]];
            if (!vc.responds_a && !vc.responds_b) continue;
            ContactManifold3D& mf = m_manifolds[vc.manifold_index];
            RigidBody3D& a = *mf.body_a;
            RigidBody3D& b = *mf.body_b;
            for (int pi = 0; pi < mf.point_count; ++pi) {
                const int p = reverse ? mf.point_count - 1 - pi : pi;
                const V3 pA = a.transform().apply(mf.points[p].local_anchor_a);
                const V3 pB = b.transform().apply(mf.points[p].local_anchor_b);
                const Real separation = vec3::dot(pB - pA, mf.normal);
                min_separation = std::min(min_separation, separation);
                const Real C = std::clamp(m_cfg.baumgarte_factor * (separation + m_cfg.slop), -m_cfg.max_correction, 0.0_r);
                if (C == 0.0_r) continue;
                const V3 point = (pA + pB) * 0.5_r;
                const V3 rA = point - a.world_center();
                const V3 rB = point - b.world_center();
                Real K = 0.0_r;
                if (vc.responds_a) K += a.inv_mass() + vec3::dot(vec3::cross(rA, mf.normal), a.apply_inv_inertia(vec3::cross(rA, mf.normal)));
                if (vc.responds_b) K += b.inv_mass() + vec3::dot(vec3::cross(rB, mf.normal), b.apply_inv_inertia(vec3::cross(rB, mf.normal)));
                const Real impulse = K > 0.0_r ? -C / K : 0.0_r;
                const V3 P = mf.normal * impulse;
                if (vc.responds_a) a.apply_position_correction(P * -a.inv_mass(), a.apply_inv_inertia(vec3::cross(rA, P)) * -1.0_r);
                if (vc.responds_b) b.apply_position_correction(P * b.inv_mass(), b.apply_inv_inertia(vec3::cross(rB, P)));
            }
        }
        if (min_separation >= -3.0_r * m_cfg.slop) break;
    }
}

void Simulation3D::solve_island_classic(Island& island, Real dt) {
    warm_start_island(island);
    const bool wide = m_cfg.simd && island.constraints.size() >= 8;
    for (int it = 0; it < island.velocity_iterations; ++it) {
        for (Joint3D* j : island.joints) j->solve(j->has_a ? &m_solver[j->solver_a] : nullptr, j->has_b ? &m_solver[j->solver_b] : nullptr, true);
        if (wide) solve_contacts_wide(island);
        else solve_contacts(island, true, false, 1.0_r / dt);
    }
    write_back(island);
    integrate_island(island, dt);
    position_correct_island(island);
}

void Simulation3D::apply_restitution(Island& island) {
    for (std::size_t c : island.constraints) {
        VelocityConstraint3D& vc = m_constraints[c];
        if (vc.restitution <= 0.0_r) continue;
        SolverBody3D& A = m_solver[vc.index_a];
        SolverBody3D& B = m_solver[vc.index_b];
        for (int p = 0; p < vc.point_count; ++p) {
            auto& vcp = vc.points[p];
            if (vc.relative_normal_velocity[p] > -vc.restitution_threshold || vcp.normal_impulse <= 0.0_r) continue;
            const Real vn = vec3::dot((B.v + vec3::cross(B.w, vcp.rB)) - (A.v + vec3::cross(A.w, vcp.rA)), vc.normal);
            Real lambda = -vcp.normal_mass * (vn + vc.restitution * vc.relative_normal_velocity[p]);
            const Real nw = std::max(vcp.normal_impulse + lambda, 0.0_r);
            lambda = nw - vcp.normal_impulse;
            vcp.normal_impulse = nw;
            const V3 P = vc.normal * lambda;
            if (vc.responds_a) A.apply(P * -1.0_r, vec3::cross(vcp.rA, P) * -1.0_r);
            if (vc.responds_b) B.apply(P, vec3::cross(vcp.rB, P));
        }
    }
}

void Simulation3D::solve_island_soft(Island& island, Real dt) {
    const int n = std::max(1, m_cfg.soft_substeps);
    const Real h = dt / n;
    const Real inv_h = 1.0_r / h;
    const SoftParams joint_soft = SoftParams::make(m_cfg.joint_hertz, m_cfg.joint_damping_ratio, h);
    auto reload = [&]() {
        for (std::size_t i : island.bodies) {
            RigidBody3D& b = *m_bodies[i];
            SolverBody3D& s = m_solver[i];
            s.v = b.linear_velocity();
            s.w = b.angular_velocity();
            s.inv_inertia = b.inv_inertia_world();
            s.center = b.world_center();
        }
    };
    for (int sub = 0; sub < n; ++sub) {
        for (std::size_t i : island.bodies) {
            RigidBody3D& b = *m_bodies[i];
            b.integrate_forces(h, m_cfg.gravity);
            if (m_cfg.gyroscopic) b.integrate_gyroscopic(h);
        }
        reload();
        if (sub > 0) {
            for (Joint3D* j : island.joints) j->refresh_bias(j->has_a ? &m_solver[j->solver_a] : nullptr, j->has_b ? &m_solver[j->solver_b] : nullptr, h, m_cfg.joint_baumgarte, joint_soft, true);
        }
        warm_start_island(island);
        for (int it = 0; it < std::max(1, island.velocity_iterations / n); ++it) {
            for (Joint3D* j : island.joints) j->solve(j->has_a ? &m_solver[j->solver_a] : nullptr, j->has_b ? &m_solver[j->solver_b] : nullptr, true);
            solve_contacts(island, true, true, inv_h);
        }
        write_back(island);
        integrate_island(island, h);
        reload();
        for (Joint3D* j : island.joints) j->solve(j->has_a ? &m_solver[j->solver_a] : nullptr, j->has_b ? &m_solver[j->solver_b] : nullptr, false);
        solve_contacts(island, false, true, inv_h);
        write_back(island);
    }
    apply_restitution(island);
    write_back(island);
}

void Simulation3D::store_impulses() {
    for (const auto& vc : m_constraints) {
        ContactManifold3D& mf = m_manifolds[vc.manifold_index];
        for (int p = 0; p < vc.point_count; ++p) {
            mf.points[p].normal_impulse = vc.points[p].normal_impulse;
            mf.points[p].friction_impulse = vc.tangent1 * vc.points[p].tangent_impulse[0] + vc.tangent2 * vc.points[p].tangent_impulse[1];
        }
    }
}

void Simulation3D::sleep_islands(Real dt) {
    const std::size_t n = std::min(m_islands.size(), m_bodies.size());
    std::vector<Real> min_timer(n, std::numeric_limits<Real>::max());
    std::vector<Real> need(n, 0.0_r);

    for (std::size_t i = 0; i < n; ++i) {
        RigidBody3D& b = *m_bodies[i];
        if (!is_enabled(b) || !b.is_awake()) continue;
        if (!is_dynamic(b)) {
            b.update_sleep(dt);
            continue;
        }
        const Real t = b.advance_sleep_timer(dt);
        const std::size_t r = m_islands.find(i);
        if (t < min_timer[r]) min_timer[r] = t;
        need[r] = std::max(need[r], b.sleep_time());
    }

    for (auto& j : m_joints) {
        if (!j->enabled()) continue;
        RigidBody3D* a = j->body_a();
        RigidBody3D* b = j->body_b();
        if (a && b && is_dynamic(*a) && !is_dynamic(*b) && is_active(*b) && a->m_solver_index < n) min_timer[m_islands.find(a->m_solver_index)] = 0.0_r;
        if (a && b && is_dynamic(*b) && !is_dynamic(*a) && is_active(*a) && b->m_solver_index < n) min_timer[m_islands.find(b->m_solver_index)] = 0.0_r;
        if (j->def().motor.enabled && j->def().motor.target_speed != 0.0_r) {
            if (a && a->m_solver_index < n) min_timer[m_islands.find(a->m_solver_index)] = 0.0_r;
            if (b && b->m_solver_index < n) min_timer[m_islands.find(b->m_solver_index)] = 0.0_r;
        }
    }

    for (std::size_t i = 0; i < n; ++i) {
        RigidBody3D& b = *m_bodies[i];
        if (!is_enabled(b) || !b.is_awake() || !is_dynamic(b)) continue;
        const std::size_t r = m_islands.find(i);
        if (min_timer[r] >= need[r]) b.put_to_sleep();
    }
}

void Simulation3D::update_contact_cache() {
    for (const auto& mf : m_manifolds) {
        Key key = key_of(mf);
        key.feature = mf.feature;
        auto it = m_touching.find(key);
        if (it == m_touching.end()) continue;
        CachedContact& c = it->second;
        c.point_count = mf.point_count;
        for (int p = 0; p < mf.point_count; ++p) {
            c.local_anchor_a[p] = mf.points[p].local_anchor_a;
            c.normal_impulse[p] = mf.points[p].normal_impulse;
            c.friction_impulse[p] = mf.points[p].friction_impulse;
        }
    }

    for (const Key& key : m_carried) {
        auto old = m_contact_cache.find(key);
        if (old != m_contact_cache.end()) m_touching.emplace(key, old->second);
    }

    auto was_touching = [](const ContactCache& cache, const Key& key) {
        auto it = cache.find(key);
        return it != cache.end() && it->second.touching;
    };

    for (auto& [key, c] : m_touching) {
        if (!c.events || !c.touching || was_touching(m_contact_cache, key)) continue;
        Real impulse = 0.0_r;
        for (int p = 0; p < c.point_count; ++p) impulse += c.normal_impulse[p];
        m_begin_events.push_back({ c.body_a, c.body_b, c.normal, c.point, impulse, c.sensor });
    }

    for (auto& [key, c] : m_contact_cache) {
        if (!c.events || !c.touching || was_touching(m_touching, key)) continue;
        m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0_r, c.sensor });
    }

    if (m_cfg.deterministic) {
        auto order = [](const ContactEvent3D& x, const ContactEvent3D& y) {
            if (x.body_a->id() != y.body_a->id()) return x.body_a->id() < y.body_a->id();
            return x.body_b->id() < y.body_b->id();
        };
        std::stable_sort(m_begin_events.begin(), m_begin_events.end(), order);
        std::stable_sort(m_end_events.begin(), m_end_events.end(), order);
    }

    m_contact_cache.swap(m_touching);
    m_touching.clear();
    m_carried.clear();
}

void Simulation3D::dispatch_events() {
    m_last_begin = m_begin_events;
    m_last_end = m_end_events;
    if (m_begin_events.empty() && m_end_events.empty()) return;
    std::vector<ContactEvent3D> begins, ends;
    begins.swap(m_begin_events);
    ends.swap(m_end_events);
    if (!m_cfg.event_callbacks) return;
    m_locked = true;
    if (m_on_begin) for (const auto& e : begins) m_on_begin(e);
    if (m_on_end) for (const auto& e : ends) m_on_end(e);
    m_locked = false;
}

void Simulation3D::remove_body_now(RigidBody3D* body) {
    auto find_owner = [this, body]() {
        return std::find_if(m_bodies.begin(), m_bodies.end(), [body](const std::unique_ptr<RigidBody3D>& p) { return p.get() == body; });
    };

    if (find_owner() == m_bodies.end()) return;

    for (auto it = m_contact_cache.begin(); it != m_contact_cache.end();) {
        if (it->first.body_a == body || it->first.body_b == body) {
            const CachedContact& c = it->second;
            if (c.events && c.touching) m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0_r, c.sensor });
            it = m_contact_cache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_reuse.begin(); it != m_reuse.end();) {
        const ReuseEntry& e = it->second;
        bool refers = false;
        for (const auto& t : e.touches) refers = refers || t.second.body_a == body || t.second.body_b == body;
        for (const auto& m : e.manifolds) refers = refers || m.body_a == body || m.body_b == body;
        if (refers) it = m_reuse.erase(it);
        else ++it;
    }

    if (body->m_proxy >= 0) {
        const std::int32_t proxy = body->m_proxy;
        for (std::size_t i = 0; i < m_tree_pairs.size();) {
            if (m_tree_pairs[i].first == proxy || m_tree_pairs[i].second == proxy) {
                m_tree_pair_index.erase(pair_key(m_tree_pairs[i].first, m_tree_pairs[i].second));
                if (i + 1 != m_tree_pairs.size()) {
                    m_tree_pairs[i] = m_tree_pairs.back();
                    m_tree_pair_index[pair_key(m_tree_pairs[i].first, m_tree_pairs[i].second)] = i;
                }
                m_tree_pairs.pop_back();
            } else {
                ++i;
            }
        }
        m_dtree.remove(proxy);
        body->m_proxy = -1;
    }

    m_joints.erase(std::remove_if(m_joints.begin(), m_joints.end(), [body](const std::unique_ptr<Joint3D>& j) { return j->body_a() == body || j->body_b() == body; }), m_joints.end());

    auto refers = [body](const ContactManifold3D& mf) { return mf.body_a == body || mf.body_b == body; };
    m_manifolds.erase(std::remove_if(m_manifolds.begin(), m_manifolds.end(), refers), m_manifolds.end());
    m_constraints.clear();
    m_pairs.clear();
    m_body_ptrs.clear();
    m_body_aabbs.clear();
    for (auto& e : m_last_begin) {
        if (e.body_a == body || e.body_b == body) e.body_a = e.body_b = nullptr;
    }
    for (auto& e : m_last_end) {
        if (e.body_a == body || e.body_b == body) e.body_a = e.body_b = nullptr;
    }
    m_last_begin.erase(std::remove_if(m_last_begin.begin(), m_last_begin.end(), [](const ContactEvent3D& e) { return e.body_a == nullptr; }), m_last_begin.end());
    m_last_end.erase(std::remove_if(m_last_end.begin(), m_last_end.end(), [](const ContactEvent3D& e) { return e.body_a == nullptr; }), m_last_end.end());
    const bool callbacks = m_cfg.event_callbacks;
    if (!m_end_events.empty() && callbacks) {
        std::vector<ContactEvent3D> ends;
        ends.swap(m_end_events);
        m_locked = true;
        if (m_on_end) for (const auto& e : ends) m_on_end(e);
        m_locked = false;
    } else {
        m_end_events.clear();
    }
    auto owner = find_owner();
    if (owner != m_bodies.end()) {
        m_handles.remove(body->m_handle);
        m_bodies.erase(owner);
    }
}

void Simulation3D::cleanup_bodies() {
    const AABB3D world_bounds = world_bounds_of(m_cfg);
    for (auto& bp : m_bodies) {
        RigidBody3D& b = *bp;
        bool remove = false;
        if (has_flag(b.flags(), PhysicsFlags::DestroyOnSleep) && !b.is_awake()) remove = true;
        if (has_flag(b.flags(), PhysicsFlags::DestroyOffScreen) && !world_bounds.overlaps(b.compute_body_aabb())) remove = true;
        if (remove) m_pending_removals.push_back(&b);
    }
    flush_pending_removals();
}

bool Simulation3D::accepts(const RigidBody3D& b, const Collider3D& c, const QueryFilter3D& f) const {
    if (!is_enabled(b)) return false;
    if (&b == f.ignore) return false;
    if ((b.collision_layer() & f.layers) == 0) return false;
    if (c.is_sensor && !f.include_sensors) return false;
    const bool dyn = is_dynamic(b);
    const bool kin = !dyn && (has_flag(b.flags(), PhysicsFlags::LinearVelocity) || has_flag(b.flags(), PhysicsFlags::AngularVelocity));
    if (dyn && !f.include_dynamic) return false;
    if (kin && !f.include_kinematic) return false;
    if (!dyn && !kin && !f.include_static) return false;
    if (f.predicate && !f.predicate(b, c)) return false;
    return true;
}

auto Simulation3D::query_aabb(const AABB3D& region) -> std::vector<RigidBody3D*> {
    QueryFilter3D f;
    f.include_sensors = true;
    f.layers = std::numeric_limits<std::uint64_t>::max();
    std::vector<RigidBody3D*> out;
    sync_tree(true);
    m_dtree.query(region, [&](std::int32_t, RigidBody3D* b) {
        if (b->compute_body_aabb().overlaps(region)) out.push_back(b);
        return true;
    });
    for (auto& bp : m_bodies) {
        if (bp->m_proxy < 0 && is_enabled(*bp) && bp->compute_body_aabb().overlaps(region)) out.push_back(bp.get());
    }
    return out;
}

auto Simulation3D::query_aabb(const AABB3D& region, const QueryFilter3D& filter) -> std::vector<RigidBody3D*> {
    std::vector<RigidBody3D*> out;
    sync_tree(true);
    m_dtree.query(region, [&](std::int32_t, RigidBody3D* b) {
        for (const Collider3D& c : b->colliders()) {
            if (accepts(*b, c, filter) && c.world_aabb.overlaps(region)) {
                out.push_back(b);
                break;
            }
        }
        return true;
    });
    return out;
}

auto Simulation3D::point_query(const RVec3& point) -> RigidBody3D* {
    sync_tree(true);
    RigidBody3D* found = nullptr;
    const AABB3D box(point, point);
    const Shape3D probe = Shape3D::make_sphere(V3{}, 1e-6_r);
    const Transform3D xf(point, RQuat(1.0_r, 0.0_r, 0.0_r, 0.0_r));
    m_dtree.query(box, [&](std::int32_t, RigidBody3D* b) {
        for (const Collider3D& c : b->colliders()) {
            if (!c.world_aabb.contains(point)) continue;
            const Transform3D wc = b->transform().compose(c.local_offset);
            if (!c.shape.is_convex() || overlap_shapes(probe, xf, c.shape, wc, 0.0_r)) {
                found = b;
                return false;
            }
        }
        return true;
    });
    if (!found) {
        for (auto& bp : m_bodies) {
            if (bp->m_proxy < 0 && bp->compute_body_aabb().contains(point)) return bp.get();
        }
    }
    return found;
}

std::optional<RayHit3D> Simulation3D::raycast(const V3& origin, const V3& direction, Real max_distance, const QueryFilter3D& filter) {
    const Real len = vec3::length(direction);
    if (len <= 1e-30_r || max_distance <= 0.0_r) return std::nullopt;
    const V3 dir = direction * (1.0_r / len);
    sync_tree(true);
    Real best = max_distance;
    std::optional<RayHit3D> result;
    const V3 inv{ std::abs(dir.x) > 1e-30_r ? 1.0_r / dir.x : 1e30_r, std::abs(dir.y) > 1e-30_r ? 1.0_r / dir.y : 1e30_r, std::abs(dir.z) > 1e-30_r ? 1.0_r / dir.z : 1e30_r };
    auto slab = [&](const TreeBox3F& b) {
        Real t0 = 0.0_r, t1 = best;
        const Real o[3] = { origin.x, origin.y, origin.z }, iv[3] = { inv.x, inv.y, inv.z };
        const Real lo[3] = { b.min.x, b.min.y, b.min.z }, hi[3] = { b.max.x, b.max.y, b.max.z };
        for (int i = 0; i < 3; ++i) {
            Real a = (lo[i] - o[i]) * iv[i], c = (hi[i] - o[i]) * iv[i];
            if (a > c) std::swap(a, c);
            t0 = std::max(t0, a);
            t1 = std::min(t1, c);
            if (t0 > t1) return false;
        }
        return true;
    };
    m_dtree.query_shape(slab, [&](std::int32_t, RigidBody3D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            const Collider3D& c = cols[i];
            if (!accepts(*b, c, filter)) continue;
            ShapeRayHit3D hit;
            const Transform3D wc = b->transform().compose(c.local_offset);
            if (raycast_shape(c.shape, wc, origin, dir, best, hit) && hit.t <= best) {
                best = hit.t;
                RayHit3D r;
                r.body = b;
                r.collider = i;
                r.distance = hit.t;
                r.fraction = hit.t / max_distance;
                r.point = hit.point;
                r.normal = hit.normal;
                r.feature = hit.feature;
                result = r;
            }
        }
        return true;
    });
    return result;
}

std::vector<RayHit3D> Simulation3D::raycast_all(const V3& origin, const V3& direction, Real max_distance, const QueryFilter3D& filter) {
    std::vector<RayHit3D> out;
    const Real len = vec3::length(direction);
    if (len <= 1e-30_r || max_distance <= 0.0_r) return out;
    const V3 dir = direction * (1.0_r / len);
    sync_tree(true);
    const AABB3D sweep = AABB3D::merge(AABB3D(origin, origin), AABB3D(origin + dir * max_distance, origin + dir * max_distance));
    m_dtree.query(sweep, [&](std::int32_t, RigidBody3D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            const Collider3D& c = cols[i];
            if (!accepts(*b, c, filter)) continue;
            ShapeRayHit3D hit;
            const Transform3D wc = b->transform().compose(c.local_offset);
            if (raycast_shape(c.shape, wc, origin, dir, max_distance, hit)) {
                RayHit3D r;
                r.body = b;
                r.collider = i;
                r.distance = hit.t;
                r.fraction = hit.t / max_distance;
                r.point = hit.point;
                r.normal = hit.normal;
                r.feature = hit.feature;
                out.push_back(r);
            }
        }
        return true;
    });
    std::sort(out.begin(), out.end(), [](const RayHit3D& a, const RayHit3D& b) { return a.distance < b.distance; });
    return out;
}

std::optional<ShapeHit3D> Simulation3D::shape_cast(const Shape3D& shape, const Transform3D& start, const V3& translation, const QueryFilter3D& filter) {
    sync_tree(true);
    const AABB3D a0 = shape.compute_aabb(start);
    const AABB3D sweep = AABB3D::merge(a0, AABB3D(a0.min + translation, a0.max + translation));
    std::optional<ShapeHit3D> result;
    Real best = 1.0_r;
    m_dtree.query(sweep, [&](std::int32_t, RigidBody3D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            const Collider3D& c = cols[i];
            if (!accepts(*b, c, filter)) continue;
            const Transform3D wc = b->transform().compose(c.local_offset);
            Real f = 0.0_r;
            V3 n, p;
            if (cast_shape(shape, start, translation, c.shape, wc, f, n, p) && f <= best) {
                best = f;
                ShapeHit3D h;
                h.body = b;
                h.collider = i;
                h.fraction = f;
                h.normal = n;
                h.point = p;
                result = h;
            }
        }
        return true;
    });
    return result;
}

std::vector<OverlapHit3D> Simulation3D::overlap(const Shape3D& shape, const Transform3D& xf, const QueryFilter3D& filter) {
    sync_tree(true);
    std::vector<OverlapHit3D> out;
    const AABB3D box = shape.compute_aabb(xf);
    m_dtree.query(box, [&](std::int32_t, RigidBody3D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            const Collider3D& c = cols[i];
            if (!accepts(*b, c, filter) || !c.world_aabb.overlaps(box)) continue;
            const Transform3D wc = b->transform().compose(c.local_offset);
            if (overlap_shapes(shape, xf, c.shape, wc, 0.0_r)) out.push_back({ b, i });
        }
        return true;
    });
    return out;
}

std::vector<RigidBody3D*> Simulation3D::overlap_sphere(const V3& center, Real radius, const QueryFilter3D& filter) {
    std::vector<RigidBody3D*> out;
    for (const OverlapHit3D& h : overlap(Shape3D::make_sphere(V3{}, radius), Transform3D(center, RQuat(1.0_r, 0.0_r, 0.0_r, 0.0_r)), filter)) {
        if (out.empty() || out.back() != h.body) out.push_back(h.body);
    }
    return out;
}

std::vector<RigidBody3D*> Simulation3D::overlap_box(const V3& center, const V3& half_extents, const RQuat& rotation, const QueryFilter3D& filter) {
    std::vector<RigidBody3D*> out;
    for (const OverlapHit3D& h : overlap(Shape3D::make_box(half_extents.x, half_extents.y, half_extents.z), Transform3D(center, rotation), filter)) {
        if (out.empty() || out.back() != h.body) out.push_back(h.body);
    }
    return out;
}

Joint3D& Simulation3D::add_joint(const JointDef3D& def) {
    m_joints.push_back(std::make_unique<Joint3D>(def));
    m_joints.back()->m_id = m_next_joint_id++;
    if (def.body_a) def.body_a->wake();
    if (def.body_b) def.body_b->wake();
    return *m_joints.back();
}

namespace {

JointDef3D base_def(RigidBody3D& a, RigidBody3D* b, const V3& anchor_a, const V3& anchor_b) {
    JointDef3D d;
    d.body_a = &a;
    d.body_b = b;
    d.local_anchor_a = a.get_local_point(anchor_a);
    d.local_anchor_b = b ? b->get_local_point(anchor_b) : anchor_b;
    d.reference_rotation = fast::mul(a.orientation().conjugate(), b ? b->orientation() : RQuat(1.0_r, 0.0_r, 0.0_r, 0.0_r));
    return d;
}

void joint_axes(JointDef3D& d, RigidBody3D& a, RigidBody3D* b, const V3& world_axis) {
    const V3 axis = vec3::normalize(world_axis);
    V3 ref, t2;
    vec3::orthonormal_basis(axis, ref, t2);
    d.local_axis_a = a.get_local_vector(axis);
    d.local_axis_b = b ? b->get_local_vector(axis) : axis;
    d.local_reference_a = a.get_local_vector(ref);
    d.local_reference_b = b ? b->get_local_vector(ref) : ref;
}

}

Joint3D& Simulation3D::add_ball_joint(RigidBody3D& a, RigidBody3D* b, const V3& world_anchor) {
    JointDef3D d = base_def(a, b, world_anchor, world_anchor);
    d.type = JointType3D::Ball;
    return add_joint(d);
}

Joint3D& Simulation3D::add_hinge_joint(RigidBody3D& a, RigidBody3D* b, const V3& world_anchor, const V3& world_axis) {
    JointDef3D d = base_def(a, b, world_anchor, world_anchor);
    d.type = JointType3D::Hinge;
    joint_axes(d, a, b, world_axis);
    return add_joint(d);
}

Joint3D& Simulation3D::add_slider_joint(RigidBody3D& a, RigidBody3D* b, const V3& world_anchor, const V3& world_axis) {
    JointDef3D d = base_def(a, b, world_anchor, world_anchor);
    d.type = JointType3D::Slider;
    joint_axes(d, a, b, world_axis);
    return add_joint(d);
}

Joint3D& Simulation3D::add_fixed_joint(RigidBody3D& a, RigidBody3D* b) {
    const V3 anchor = b ? (a.world_center() + b->world_center()) * 0.5_r : a.world_center();
    JointDef3D d = base_def(a, b, anchor, anchor);
    d.type = JointType3D::Fixed;
    return add_joint(d);
}

Joint3D& Simulation3D::add_distance_joint(RigidBody3D& a, RigidBody3D* b, const V3& anchor_a, const V3& anchor_b, Real min_length, Real max_length) {
    JointDef3D d = base_def(a, b, anchor_a, anchor_b);
    d.type = JointType3D::Distance;
    d.rest_length = vec3::length(anchor_b - anchor_a);
    if (min_length >= 0.0_r || max_length >= 0.0_r) {
        d.min_length = min_length >= 0.0_r ? min_length : 0.0_r;
        d.max_length = max_length >= 0.0_r ? max_length : d.rest_length;
    }
    return add_joint(d);
}

Joint3D& Simulation3D::add_spring_joint(RigidBody3D& a, RigidBody3D* b, const V3& anchor_a, const V3& anchor_b, Real rest_length, Real frequency, Real damping_ratio) {
    JointDef3D d = base_def(a, b, anchor_a, anchor_b);
    d.type = JointType3D::Spring;
    d.rest_length = rest_length;
    d.spring.frequency = frequency;
    d.spring.damping_ratio = damping_ratio;
    d.collide_connected = true;
    return add_joint(d);
}

Joint3D& Simulation3D::add_cone_twist_joint(RigidBody3D& a, RigidBody3D* b, const V3& world_anchor, const V3& world_axis, Real swing_limit, Real twist_lower, Real twist_upper) {
    JointDef3D d = base_def(a, b, world_anchor, world_anchor);
    d.type = JointType3D::ConeTwist;
    joint_axes(d, a, b, world_axis);
    d.swing_limit = swing_limit;
    if (twist_upper > twist_lower) {
        d.limits.enabled = true;
        d.limits.lower = twist_lower;
        d.limits.upper = twist_upper;
    }
    return add_joint(d);
}

void Simulation3D::remove_joint(Joint3D& joint) {
    if (joint.body_a()) joint.body_a()->wake();
    if (joint.body_b()) joint.body_b()->wake();
    m_joints.erase(std::remove_if(m_joints.begin(), m_joints.end(), [&joint](const std::unique_ptr<Joint3D>& j) { return j.get() == &joint; }), m_joints.end());
}

void Simulation3D::shift_origin(const V3& offset) {
    for (auto& bp : m_bodies) {
        RigidBody3D& b = *bp;
        b.m_transform.position = b.m_transform.position - offset;
        b.m_sweep.position0 = b.m_sweep.position0 - offset;
        b.m_sweep.position1 = b.m_sweep.position1 - offset;
        b.synchronize_colliders();
    }
    m_dtree.shift(offset * -1.0_r);
    m_origin = m_origin + offset;
    for (auto& c : m_cloths) c->shift(offset);
    m_focus = m_focus - offset;
    for (auto& [key, c] : m_contact_cache) c.point = c.point - offset;
    for (auto& [key, e] : m_reuse) {
        for (auto& t : e.touches) t.second.point = t.second.point - offset;
    }
    for (auto& mf : m_manifolds) {
        for (int p = 0; p < mf.point_count; ++p) mf.points[p].position = mf.points[p].position - offset;
    }
    for (auto& e : m_last_begin) e.point = e.point - offset;
    for (auto& e : m_last_end) e.point = e.point - offset;
    for (auto& j : m_joints) j->shift_world(offset);
}

bool Simulation3D::rebase_if_needed() {
    const Real limit = m_cfg.rebase_distance;
    if (!(limit > 0.0_r) || m_locked) return false;
    if (std::abs(m_focus.x) <= limit && std::abs(m_focus.y) <= limit && std::abs(m_focus.z) <= limit) return false;
    const Real cell = limit * 0.5_r;
    const V3 shift{ std::round(m_focus.x / cell) * cell, std::round(m_focus.y / cell) * cell, std::round(m_focus.z / cell) * cell };
    shift_origin(shift);
    return true;
}

void Simulation3D::snapshot(std::vector<BodySnapshot3F>& out) const {
    out.resize(m_bodies.size());
    for (std::size_t i = 0; i < m_bodies.size(); ++i) {
        const RigidBody3D& b = *m_bodies[i];
        BodySnapshot3F& s = out[i];
        const V3 p = b.position();
        const RQuat q = b.orientation();
        s.x = static_cast<float>(p.x);
        s.y = static_cast<float>(p.y);
        s.z = static_cast<float>(p.z);
        s.qx = static_cast<float>(q.x());
        s.qy = static_cast<float>(q.y());
        s.qz = static_cast<float>(q.z());
        s.qw = static_cast<float>(q.w());
        s.handle = b.handle();
        s.awake = b.is_awake();
    }
}

void Simulation3D::solve_contacts_wide(Island& island) {
    constexpr int W = 8;
    struct Batch {
        std::size_t c[W];
        int count = 0;
    };
    std::vector<std::vector<std::size_t>> colors;
    std::vector<std::uint64_t> used(m_bodies.size(), 0);
    std::vector<std::size_t> serial;
    for (std::size_t ci : island.constraints) {
        const VelocityConstraint3D& vc = m_constraints[ci];
        const std::uint64_t mask = (vc.responds_a ? used[vc.index_a] : 0) | (vc.responds_b ? used[vc.index_b] : 0);
        int color = -1;
        for (int k = 0; k < 64; ++k) {
            if (!(mask & (1ULL << k))) {
                color = k;
                break;
            }
        }
        if (color < 0) {
            serial.push_back(ci);
            continue;
        }
        if (static_cast<int>(colors.size()) <= color) colors.resize(static_cast<std::size_t>(color) + 1);
        colors[static_cast<std::size_t>(color)].push_back(ci);
        if (vc.responds_a) used[vc.index_a] |= 1ULL << color;
        if (vc.responds_b) used[vc.index_b] |= 1ULL << color;
    }
    struct Lane {
        float v[W];
    };
    auto run_batch = [&](const std::size_t* ids, int count) {
        Lane vax, vay, vaz, wax, way, waz, vbx, vby, vbz, wbx, wby, wbz;
        int max_points = 0;
        for (int l = 0; l < W; ++l) {
            if (l >= count) {
                vax.v[l] = vay.v[l] = vaz.v[l] = wax.v[l] = way.v[l] = waz.v[l] = 0.0f;
                vbx.v[l] = vby.v[l] = vbz.v[l] = wbx.v[l] = wby.v[l] = wbz.v[l] = 0.0f;
                continue;
            }
            const VelocityConstraint3D& vc = m_constraints[ids[l]];
            const SolverBody3D& A = m_solver[vc.index_a];
            const SolverBody3D& B = m_solver[vc.index_b];
            vax.v[l] = static_cast<float>(A.v.x); vay.v[l] = static_cast<float>(A.v.y); vaz.v[l] = static_cast<float>(A.v.z);
            wax.v[l] = static_cast<float>(A.w.x); way.v[l] = static_cast<float>(A.w.y); waz.v[l] = static_cast<float>(A.w.z);
            vbx.v[l] = static_cast<float>(B.v.x); vby.v[l] = static_cast<float>(B.v.y); vbz.v[l] = static_cast<float>(B.v.z);
            wbx.v[l] = static_cast<float>(B.w.x); wby.v[l] = static_cast<float>(B.w.y); wbz.v[l] = static_cast<float>(B.w.z);
            max_points = std::max(max_points, vc.point_count);
        }
        for (int p = 0; p < max_points; ++p) {
            for (int pass = 0; pass < 2; ++pass) {
                Lane dvn, imp;
                for (int l = 0; l < W; ++l) {
                    dvn.v[l] = 0.0f;
                    imp.v[l] = 0.0f;
                }
                for (int l = 0; l < count; ++l) {
                    VelocityConstraint3D& vc = m_constraints[ids[l]];
                    if (p >= vc.point_count) continue;
                    const auto& c = vc.points[p];
                    const float rax = static_cast<float>(c.rA.x), ray = static_cast<float>(c.rA.y), raz = static_cast<float>(c.rA.z);
                    const float rbx = static_cast<float>(c.rB.x), rby = static_cast<float>(c.rB.y), rbz = static_cast<float>(c.rB.z);
                    const float dx = (vbx.v[l] + wby.v[l] * rbz - wbz.v[l] * rby) - (vax.v[l] + way.v[l] * raz - waz.v[l] * ray);
                    const float dy = (vby.v[l] + wbz.v[l] * rbx - wbx.v[l] * rbz) - (vay.v[l] + waz.v[l] * rax - wax.v[l] * raz);
                    const float dz = (vbz.v[l] + wbx.v[l] * rby - wby.v[l] * rbx) - (vaz.v[l] + wax.v[l] * ray - way.v[l] * rax);
                    if (pass == 0) {
                        const float t1 = dx * static_cast<float>(vc.tangent1.x) + dy * static_cast<float>(vc.tangent1.y) + dz * static_cast<float>(vc.tangent1.z);
                        const float t2 = dx * static_cast<float>(vc.tangent2.x) + dy * static_cast<float>(vc.tangent2.y) + dz * static_cast<float>(vc.tangent2.z);
                        dvn.v[l] = t1;
                        imp.v[l] = t2;
                    } else {
                        dvn.v[l] = dx * static_cast<float>(vc.normal.x) + dy * static_cast<float>(vc.normal.y) + dz * static_cast<float>(vc.normal.z);
                    }
                }
                for (int l = 0; l < count; ++l) {
                    VelocityConstraint3D& vc = m_constraints[ids[l]];
                    if (p >= vc.point_count) continue;
                    auto& c = vc.points[p];
                    V3 P;
                    if (pass == 0) {
                        const Real old1 = c.tangent_impulse[0], old2 = c.tangent_impulse[1];
                        Real n1 = old1 - dvn.v[l] * c.tangent_mass[0];
                        Real n2 = old2 - imp.v[l] * c.tangent_mass[1];
                        const Real m1 = vc.friction * c.normal_impulse, m2 = vc.friction2 * c.normal_impulse;
                        if (m1 > 0.0_r && m2 > 0.0_r) {
                            const Real e = n1 * n1 / (m1 * m1) + n2 * n2 / (m2 * m2);
                            if (e > 1.0_r) {
                                const Real s = 1.0_r / std::sqrt(e);
                                n1 *= s;
                                n2 *= s;
                            }
                        } else {
                            n1 = n2 = 0.0_r;
                        }
                        c.tangent_impulse[0] = n1;
                        c.tangent_impulse[1] = n2;
                        P = vc.tangent1 * (n1 - old1) + vc.tangent2 * (n2 - old2);
                    } else {
                        Real lambda = -(dvn.v[l] - c.velocity_bias) * c.normal_mass;
                        const Real nw = std::max(c.normal_impulse + lambda, 0.0_r);
                        lambda = nw - c.normal_impulse;
                        c.normal_impulse = nw;
                        P = vc.normal * lambda;
                    }
                    const SolverBody3D& A = m_solver[vc.index_a];
                    const SolverBody3D& B = m_solver[vc.index_b];
                    if (vc.responds_a) {
                        const V3 dw = vec3::mul(A.inv_inertia, vec3::cross(c.rA, P));
                        vax.v[l] -= static_cast<float>(A.inv_mass.x * P.x); vay.v[l] -= static_cast<float>(A.inv_mass.y * P.y); vaz.v[l] -= static_cast<float>(A.inv_mass.z * P.z);
                        wax.v[l] -= static_cast<float>(dw.x); way.v[l] -= static_cast<float>(dw.y); waz.v[l] -= static_cast<float>(dw.z);
                    }
                    if (vc.responds_b) {
                        const V3 dw = vec3::mul(B.inv_inertia, vec3::cross(c.rB, P));
                        vbx.v[l] += static_cast<float>(B.inv_mass.x * P.x); vby.v[l] += static_cast<float>(B.inv_mass.y * P.y); vbz.v[l] += static_cast<float>(B.inv_mass.z * P.z);
                        wbx.v[l] += static_cast<float>(dw.x); wby.v[l] += static_cast<float>(dw.y); wbz.v[l] += static_cast<float>(dw.z);
                    }
                }
            }
        }
        for (int l = 0; l < count; ++l) {
            const VelocityConstraint3D& vc = m_constraints[ids[l]];
            SolverBody3D& A = m_solver[vc.index_a];
            SolverBody3D& B = m_solver[vc.index_b];
            if (vc.responds_a) {
                A.v = { vax.v[l], vay.v[l], vaz.v[l] };
                A.w = { wax.v[l], way.v[l], waz.v[l] };
            }
            if (vc.responds_b) {
                B.v = { vbx.v[l], vby.v[l], vbz.v[l] };
                B.w = { wbx.v[l], wby.v[l], wbz.v[l] };
            }
        }
    };
    m_stats.colors = std::max(m_stats.colors, colors.size());
    for (const auto& color : colors) {
        const std::size_t batches = (color.size() + W - 1) / W;
        auto run = [&](std::size_t bi) {
            const std::size_t start = bi * W;
            const int count = static_cast<int>(std::min<std::size_t>(W, color.size() - start));
            run_batch(color.data() + start, count);
        };
        for (std::size_t bi = 0; bi < batches; ++bi) run(bi);
    }
    for (std::size_t ci : serial) solve_contact(m_constraints[ci], false, true, false, 1.0_r / m_substep_dt);
}

} // namespace physics
} // namespace fizmo
