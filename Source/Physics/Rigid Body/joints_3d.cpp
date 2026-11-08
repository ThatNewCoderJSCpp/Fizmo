#include "fizmo_library.hpp"
#include "joints_3d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

enum RowKind : std::uint8_t {
    row_point = 0,
    row_angular,
    row_lower,
    row_upper,
    row_motor,
    row_spring,
    row_perp
};

enum Slot : std::uint8_t {
    slot_px = 0,
    slot_py,
    slot_pz,
    slot_ax,
    slot_ay,
    slot_az,
    slot_lower,
    slot_upper,
    slot_motor,
    slot_spring,
    slot_perp1,
    slot_perp2,
    slot_swing,
    slot_twist_lower,
    slot_twist_upper,
    slot_count
};

V3 unit(const V3& v, const V3& fallback) noexcept {
    const Real l = vec3::length(v);
    return l > 1e-14_r ? v * (1.0_r / l) : fallback;
}

V3 rotate(const RQuat& q, const V3& v) noexcept {
    return fast::rotate(q, v);
}

RQuat orientation_of(const RigidBody3D* b) noexcept {
    return b ? b->orientation() : RQuat(1.0_r, 0.0_r, 0.0_r, 0.0_r);
}

Real wrap_angle(Real a) noexcept {
    const Real two_pi = 6.283185307179586_r;
    while (a > 3.141592653589793_r) a -= two_pi;
    while (a < -3.141592653589793_r) a += two_pi;
    return a;
}

Real row_k(const Joint3D::Row& r, const SolverBody3D* a, bool use_a, const SolverBody3D* b, bool use_b) noexcept {
    Real k = 0.0_r;
    if (use_a) {
        k += r.lin_a.x * r.lin_a.x * a->inv_mass.x + r.lin_a.y * r.lin_a.y * a->inv_mass.y + r.lin_a.z * r.lin_a.z * a->inv_mass.z;
        k += vec3::dot(r.ang_a, vec3::mul(a->inv_inertia, r.ang_a));
    }
    if (use_b) {
        k += r.lin_b.x * r.lin_b.x * b->inv_mass.x + r.lin_b.y * r.lin_b.y * b->inv_mass.y + r.lin_b.z * r.lin_b.z * b->inv_mass.z;
        k += vec3::dot(r.ang_b, vec3::mul(b->inv_inertia, r.ang_b));
    }
    return k;
}

}

const char* to_string(JointType3D t) noexcept {
    switch (t) {
    case JointType3D::Ball: return "ball";
    case JointType3D::Hinge: return "hinge";
    case JointType3D::Slider: return "slider";
    case JointType3D::Fixed: return "fixed";
    case JointType3D::Distance: return "distance";
    case JointType3D::Spring: return "spring";
    case JointType3D::ConeTwist: return "cone-twist";
    }
    return "ball";
}

std::optional<JointType3D> parse_joint_type(std::string_view text) noexcept {
    if (text == "ball" || text == "point" || text == "socket") return JointType3D::Ball;
    if (text == "hinge" || text == "revolute") return JointType3D::Hinge;
    if (text == "slider" || text == "prismatic") return JointType3D::Slider;
    if (text == "fixed" || text == "weld") return JointType3D::Fixed;
    if (text == "distance" || text == "rope" || text == "rod") return JointType3D::Distance;
    if (text == "spring") return JointType3D::Spring;
    if (text == "cone-twist" || text == "cone" || text == "ragdoll") return JointType3D::ConeTwist;
    return std::nullopt;
}

V3 Joint3D::world_anchor_a() const noexcept {
    return m_def.body_a ? m_def.body_a->get_world_point(m_def.local_anchor_a) : m_def.local_anchor_a;
}

V3 Joint3D::world_anchor_b() const noexcept {
    return m_def.body_b ? m_def.body_b->get_world_point(m_def.local_anchor_b) : m_def.local_anchor_b;
}

V3 Joint3D::world_axis_a() const noexcept {
    return unit(rotate(orientation_of(m_def.body_a), m_def.local_axis_a), V3{ 0.0_r, 1.0_r, 0.0_r });
}

Real Joint3D::angle() const noexcept {
    const V3 axis = world_axis_a();
    const V3 ref_a = rotate(orientation_of(m_def.body_a), m_def.local_reference_a);
    V3 ref_b = rotate(orientation_of(m_def.body_b), m_def.local_reference_b);
    ref_b = ref_b - axis * vec3::dot(ref_b, axis);
    if (vec3::length_squared(ref_b) < 1e-20_r) return 0.0_r;
    return std::atan2(vec3::dot(vec3::cross(ref_a, ref_b), axis), vec3::dot(ref_a, ref_b));
}

Real Joint3D::translation() const noexcept {
    return vec3::dot(world_anchor_b() - world_anchor_a(), world_axis_a());
}

Real Joint3D::length() const noexcept {
    return vec3::length(world_anchor_b() - world_anchor_a());
}

void Joint3D::add_linear(const V3& rA, const V3& rB, const V3& axis, Real C, std::uint8_t slot) {
    Row r;
    r.lin_a = axis * -1.0_r;
    r.ang_a = vec3::cross(rA, axis) * -1.0_r;
    r.lin_b = axis;
    r.ang_b = vec3::cross(rB, axis);
    r.position_error = C;
    r.slot = slot;
    r.kind = row_point;
    m_rows.push_back(r);
}

void Joint3D::add_angular(const V3& axis, Real C, std::uint8_t slot) {
    Row r;
    r.ang_a = axis * -1.0_r;
    r.ang_b = axis;
    r.position_error = C;
    r.slot = slot;
    r.kind = row_angular;
    m_rows.push_back(r);
}

void Joint3D::build_rows(SolverBody3D* a, SolverBody3D* b, Real dt, Real baumgarte, const SoftParams& soft, bool use_soft, bool keep_impulses) {
    if (keep_impulses) {
        for (const Row& r : m_rows) m_saved[r.slot] = r.impulse;
    }
    m_rows.clear();
    const V3 pA = world_anchor_a();
    const V3 pB = world_anchor_b();
    const V3 cA = has_a ? a->center : pA;
    const V3 cB = has_b ? b->center : pB;
    const V3 rA = pA - cA;
    const V3 rB = pB - cB;
    const V3 d = pB - pA;
    const RQuat qA = orientation_of(m_def.body_a);
    const RQuat qB = orientation_of(m_def.body_b);
    const V3 ex{ 1.0_r, 0.0_r, 0.0_r }, ey{ 0.0_r, 1.0_r, 0.0_r }, ez{ 0.0_r, 0.0_r, 1.0_r };
    auto point_rows = [&]() {
        add_linear(rA, rB, ex, d.x, slot_px);
        add_linear(rA, rB, ey, d.y, slot_py);
        add_linear(rA, rB, ez, d.z, slot_pz);
    };
    auto orientation_rows = [&]() {
        const RQuat rel = fast::mul(qA.conjugate(), qB);
        RQuat err = fast::mul(rel, m_def.reference_rotation.conjugate());
        if (err.w() < 0.0_r) err = RQuat(-err.w(), -err.x(), -err.y(), -err.z());
        const V3 theta = rotate(qA, V3{ 2.0_r * err.x(), 2.0_r * err.y(), 2.0_r * err.z() });
        add_angular(ex, theta.x, slot_ax);
        add_angular(ey, theta.y, slot_ay);
        add_angular(ez, theta.z, slot_az);
    };
    auto axis_rows = [&](const V3& axis, Real value, bool linear, const V3& rA_axis) {
        auto make = [&](std::uint8_t kind, std::uint8_t slot, Real C) {
            if (linear) add_linear(rA_axis, rB, axis, C, slot);
            else add_angular(axis, C, slot);
            m_rows.back().kind = kind;
        };
        if (m_def.limits.enabled) {
            if (m_def.limits.upper - m_def.limits.lower < 1e-9_r) {
                make(row_lower, slot_lower, value - m_def.limits.lower);
                m_rows.back().kind = row_point;
            } else {
                make(row_lower, slot_lower, value - m_def.limits.lower);
                m_rows.back().lower = 0.0_r;
                make(row_upper, slot_upper, value - m_def.limits.upper);
                m_rows.back().upper = 0.0_r;
            }
        }
        if (m_def.motor.enabled) {
            make(row_motor, slot_motor, 0.0_r);
            m_rows.back().target = m_def.motor.target_speed;
            m_rows.back().lower = -std::abs(m_def.motor.max_force) * dt;
            m_rows.back().upper = std::abs(m_def.motor.max_force) * dt;
        }
        if (m_def.spring.frequency > 0.0_r) {
            make(row_spring, slot_spring, value - m_def.spring.target);
        }
    };
    switch (m_def.type) {
    case JointType3D::Ball:
        point_rows();
        break;
    case JointType3D::Fixed:
        point_rows();
        orientation_rows();
        break;
    case JointType3D::Hinge: {
        point_rows();
        const V3 aA = unit(rotate(qA, m_def.local_axis_a), ey);
        const V3 aB = unit(rotate(qB, m_def.local_axis_b), ey);
        V3 t1, t2;
        vec3::orthonormal_basis(aA, t1, t2);
        const V3 err = vec3::cross(aA, aB);
        add_angular(t1, vec3::dot(err, t1), slot_perp1);
        add_angular(t2, vec3::dot(err, t2), slot_perp2);
        axis_rows(aA, angle(), false, rA);
        break;
    }
    case JointType3D::Slider: {
        orientation_rows();
        const V3 aA = unit(rotate(qA, m_def.local_axis_a), ey);
        V3 t1, t2;
        vec3::orthonormal_basis(aA, t1, t2);
        const V3 rAd = pB - cA;
        add_linear(rAd, rB, t1, vec3::dot(d, t1), slot_perp1);
        add_linear(rAd, rB, t2, vec3::dot(d, t2), slot_perp2);
        axis_rows(aA, vec3::dot(d, aA), true, rAd);
        break;
    }
    case JointType3D::Distance:
    case JointType3D::Spring: {
        const Real len = vec3::length(d);
        const V3 n = len > 1e-9_r ? d * (1.0_r / len) : ey;
        const bool spring = m_def.type == JointType3D::Spring || m_def.spring.frequency > 0.0_r;
        const bool range = m_def.max_length > m_def.min_length;
        if (spring) {
            add_linear(rA, rB, n, len - m_def.rest_length, slot_spring);
            m_rows.back().kind = row_spring;
        }
        if (range) {
            add_linear(rA, rB, n, len - m_def.min_length, slot_lower);
            m_rows.back().kind = row_lower;
            m_rows.back().lower = 0.0_r;
            add_linear(rA, rB, n, len - m_def.max_length, slot_upper);
            m_rows.back().kind = row_upper;
            m_rows.back().upper = 0.0_r;
        } else if (!spring) {
            add_linear(rA, rB, n, len - m_def.rest_length, slot_lower);
        }
        if (m_def.motor.enabled) {
            add_linear(rA, rB, n, 0.0_r, slot_motor);
            m_rows.back().kind = row_motor;
            m_rows.back().target = m_def.motor.target_speed;
            m_rows.back().lower = -std::abs(m_def.motor.max_force) * dt;
            m_rows.back().upper = std::abs(m_def.motor.max_force) * dt;
        }
        break;
    }
    case JointType3D::ConeTwist: {
        point_rows();
        const V3 aA = unit(rotate(qA, m_def.local_axis_a), ey);
        const V3 aB = unit(rotate(qB, m_def.local_axis_b), ey);
        const Real cos_phi = std::clamp(vec3::dot(aA, aB), -1.0_r, 1.0_r);
        const Real phi = std::acos(cos_phi);
        if (m_def.swing_limit > 0.0_r && phi > m_def.swing_limit - 0.02_r) {
            const V3 n = unit(vec3::cross(aA, aB), V3{ 1.0_r, 0.0_r, 0.0_r });
            add_angular(n, phi - m_def.swing_limit, slot_swing);
            m_rows.back().kind = row_upper;
            m_rows.back().upper = 0.0_r;
        }
        if (m_def.limits.enabled) {
            const V3 ref_a = rotate(qA, m_def.local_reference_a);
            V3 ref_b = rotate(qB, m_def.local_reference_b);
            const V3 axis = unit(vec3::cross(aB, aA), V3{});
            if (vec3::length_squared(axis) > 0.0_r) {
                const Real ang = std::acos(cos_phi);
                const Real s = std::sin(ang), c = std::cos(ang);
                ref_b = ref_b * c + vec3::cross(axis, ref_b) * s + axis * (vec3::dot(axis, ref_b) * (1.0_r - c));
            }
            ref_b = ref_b - aA * vec3::dot(ref_b, aA);
            const Real twist = vec3::length_squared(ref_b) > 1e-20_r ? std::atan2(vec3::dot(vec3::cross(ref_a, ref_b), aA), vec3::dot(ref_a, ref_b)) : 0.0_r;
            add_angular(aA, twist - m_def.limits.lower, slot_twist_lower);
            m_rows.back().kind = row_lower;
            m_rows.back().lower = 0.0_r;
            add_angular(aA, twist - m_def.limits.upper, slot_twist_upper);
            m_rows.back().kind = row_upper;
            m_rows.back().upper = 0.0_r;
        }
        break;
    }
    }
    const bool use_a = has_a && a->joint_impulses;
    const bool use_b = has_b && b->joint_impulses;
    for (Row& r : m_rows) {
        const Real k = row_k(r, a, use_a, b, use_b);
        r.impulse = m_saved[r.slot];
        if (r.kind == row_lower && r.position_error > 0.05_r) r.impulse = 0.0_r;
        if (r.kind == row_upper && r.position_error < -0.05_r) r.impulse = 0.0_r;
        if (k <= 1e-30_r) {
            r.mass = 0.0_r;
            continue;
        }
        r.mass = 1.0_r / k;
        const Real C = r.position_error;
        if (r.kind == row_spring) {
            const Real freq = m_def.spring.frequency > 0.0_r ? m_def.spring.frequency : 2.0_r;
            const Real omega = 2.0_r * 3.14159265358979323846_r * freq;
            const Real mass = r.mass;
            const Real kk = mass * omega * omega;
            const Real c = 2.0_r * mass * m_def.spring.damping_ratio * omega;
            const Real denom = dt * (c + dt * kk);
            r.gamma = denom > 0.0_r ? 1.0_r / denom : 0.0_r;
            r.bias = C * dt * kk * r.gamma;
            r.mass = 1.0_r / (k + r.gamma);
            r.soft_bias = false;
            continue;
        }
        if (r.kind == row_motor) {
            r.bias = 0.0_r;
            continue;
        }
        if (r.kind == row_lower && C > 0.0_r) {
            r.bias = C / dt;
            r.soft_bias = false;
            continue;
        }
        if (r.kind == row_upper && C < 0.0_r) {
            r.bias = C / dt;
            r.soft_bias = false;
            continue;
        }
        if (use_soft) {
            r.bias = C * soft.bias_rate;
            r.gamma = soft.impulse_scale;
            r.mass *= soft.mass_scale;
        } else {
            r.bias = C * baumgarte / dt;
        }
    }
}

void Joint3D::prepare(SolverBody3D* a, SolverBody3D* b, Real dt, Real baumgarte, const SoftParams& soft, bool use_soft) {
    build_rows(a, b, dt, baumgarte, soft, use_soft, false);
}

void Joint3D::refresh_bias(SolverBody3D* a, SolverBody3D* b, Real dt, Real baumgarte, const SoftParams& soft, bool use_soft) {
    build_rows(a, b, dt, baumgarte, soft, use_soft, true);
}

void Joint3D::warm_start(SolverBody3D* a, SolverBody3D* b) noexcept {
    const bool use_a = has_a && a->joint_impulses;
    const bool use_b = has_b && b->joint_impulses;
    for (const Row& r : m_rows) {
        if (r.impulse == 0.0_r || r.mass == 0.0_r) continue;
        if (use_a) a->apply(r.lin_a * r.impulse, r.ang_a * r.impulse);
        if (use_b) b->apply(r.lin_b * r.impulse, r.ang_b * r.impulse);
    }
}

void Joint3D::solve(SolverBody3D* a, SolverBody3D* b, bool use_bias) noexcept {
    const bool use_a = has_a && a->joint_impulses;
    const bool use_b = has_b && b->joint_impulses;
    const V3 zero{};
    for (Row& r : m_rows) {
        if (r.mass == 0.0_r) continue;
        const V3& va = has_a ? a->v : zero;
        const V3& wa = has_a ? a->w : zero;
        const V3& vb = has_b ? b->v : zero;
        const V3& wb = has_b ? b->w : zero;
        const Real jv = vec3::dot(r.lin_a, va) + vec3::dot(r.ang_a, wa) + vec3::dot(r.lin_b, vb) + vec3::dot(r.ang_b, wb);
        Real lambda;
        if (r.kind == row_motor) {
            lambda = -r.mass * (jv - r.target);
        } else if (r.kind == row_spring) {
            lambda = -r.mass * (jv + r.bias + r.gamma * r.impulse);
        } else {
            Real bias = r.bias;
            Real gamma = r.soft_bias ? r.gamma : 0.0_r;
            if (!use_bias && r.soft_bias) {
                bias = 0.0_r;
                gamma = 0.0_r;
            }
            lambda = -r.mass * (jv + bias) - gamma * r.impulse;
        }
        const Real old = r.impulse;
        r.impulse = std::clamp(old + lambda, r.lower, r.upper);
        const Real dl = r.impulse - old;
        if (dl == 0.0_r) continue;
        if (use_a) a->apply(r.lin_a * dl, r.ang_a * dl);
        if (use_b) b->apply(r.lin_b * dl, r.ang_b * dl);
    }
}

void Joint3D::finish(Real dt) {
    m_saved.fill(0.0_r);
    m_reaction_force = V3{};
    m_reaction_torque = V3{};
    const Real inv = dt > 0.0_r ? 1.0_r / dt : 0.0_r;
    for (const Row& r : m_rows) {
        m_saved[r.slot] = r.impulse;
        if (vec3::length_squared(r.lin_b) > 0.0_r) m_reaction_force += r.lin_b * (r.impulse * inv);
        else m_reaction_torque += r.ang_b * (r.impulse * inv);
    }
    if (m_def.break_force > 0.0_r && vec3::length(m_reaction_force) > m_def.break_force) m_broken = true;
    if (m_def.break_torque > 0.0_r && vec3::length(m_reaction_torque) > m_def.break_torque) m_broken = true;
    if (m_broken) {
        m_saved.fill(0.0_r);
        m_rows.clear();
    }
}

void Joint3D::shift_world(const V3& offset) noexcept {
    if (!m_def.body_a) m_def.local_anchor_a = m_def.local_anchor_a - offset;
    if (!m_def.body_b) m_def.local_anchor_b = m_def.local_anchor_b - offset;
}

} // namespace physics
} // namespace fizmo
