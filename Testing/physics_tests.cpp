#include "../Includes/physics.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace fizmo;
using namespace fizmo::physics;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

bool near(double a, double b, double tol = 1e-6) {
    return std::fabs(a - b) <= tol;
}

void test_2d_rotation_locks() {
    Simulation2D sim;
    sim.set_gravity({0.0, 0.0});

    RigidBody2D& z = sim.add_body(PhysicsFlags::DefaultDynamic);
    z.add_collider(Shape2D::make_box(0.5, 0.5));
    z.set_flag(PhysicsFlags::LockRotationZ);
    z.set_angular_velocity(1.0);

    RigidBody2D& xy = sim.add_body({10.0, 0.0}, PhysicsFlags::DefaultDynamic);
    xy.add_collider(Shape2D::make_box(0.5, 0.5));
    xy.set_flag(PhysicsFlags::LockRotationX | PhysicsFlags::LockRotationY);
    xy.set_angular_velocity(1.0);

    RigidBody2D& preset = sim.add_body({20.0, 0.0}, PhysicsFlags::DefaultDynamic | PhysicsFlags::LockRotation2D);
    preset.add_collider(Shape2D::make_box(0.5, 0.5));
    preset.set_angular_velocity(1.0);

    RigidBody2D& free_body = sim.add_body({30.0, 0.0}, PhysicsFlags::DefaultDynamic);
    free_body.add_collider(Shape2D::make_box(0.5, 0.5));
    free_body.set_angular_damping(0.0);
    free_body.set_angular_velocity(1.0);

    for (int i = 0; i < 60; ++i) sim.step();

    check(near(z.angle(), 0.0), "2D: LockRotationZ stops rotation");
    check(near(xy.angle(), 0.0), "2D: LockRotationX | LockRotationY still stops rotation");
    check(near(preset.angle(), 0.0), "2D: LockRotation2D stops rotation");
    check(near(free_body.angle(), 1.0, 1e-3), "2D: a free body turns about z");
    check(z.rotation_locked() && !free_body.rotation_locked(), "2D: rotation_locked()");
}

void test_2d_lock_after_mass() {
    Simulation2D sim;
    sim.set_gravity({0.0, 0.0});
    RigidBody2D& b = sim.add_body(PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape2D::make_box(0.5, 0.5));
    b.ensure_mass_uptodate();
    b.set_flag(PhysicsFlags::LockRotationZ);
    check(b.is_mass_dirty(), "2D: locking rotation marks the mass dirty");
}

void test_3d_angles() {
    RigidBody3D b(PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));

    b.set_euler_angles(10.0, 20.0, 30.0, false);
    const auto e = b.euler_angles(false);
    check(near(e[0], 10.0) && near(e[1], 20.0) && near(e[2], 30.0), "3D: euler_angles round trip");

    b.set_euler_xyz(15.0, 25.0, 35.0, false);
    const auto x = b.euler_xyz(false);
    check(near(x[0], 15.0) && near(x[1], 25.0) && near(x[2], 35.0), "3D: euler_xyz round trip");

    b.set_rotation({0.0, 0.0, 1.0}, 90.0, false);
    const linalg::vector3d r = b.transform().rotate({1.0, 0.0, 0.0});
    check(near(r.x, 0.0) && near(r.y, 1.0) && near(r.z, 0.0), "3D: 90 degrees about z turns +x into +y");

    b.set_yaw_pitch_roll(30.0, 20.0, 10.0, false);
    const auto y = b.yaw_pitch_roll(false);
    check(near(y[0], 30.0) && near(y[1], 20.0) && near(y[2], 10.0), "3D: yaw_pitch_roll round trip");

    b.set_yaw_pitch_roll(90.0, 0.0, 0.0, false);
    const linalg::vector3d f = b.transform().rotate({0.0, 0.0, -1.0});
    check(near(f.x, -1.0) && near(f.y, 0.0) && near(f.z, 0.0), "3D: yaw turns about +y like Camera3D");
}

void test_3d_spin() {
    Simulation3D sim;
    sim.set_gravity({0.0, 0.0, 0.0});
    RigidBody3D& b = sim.add_body(PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
    b.set_angular_damping(0.0);
    b.set_angular_velocity({0.0, 0.0, constants::pi<double>() / 2.0});
    for (int i = 0; i < 60; ++i) sim.step();
    const auto e = b.euler_angles(false);
    check(near(e[2], 90.0, 1e-3) && near(e[0], 0.0, 1e-6) && near(e[1], 0.0, 1e-6), "3D: spinning pi/2 rad/s about z for 1 s gives 90 degrees about z");

    RigidBody3D& locked = sim.add_body({10.0, 0.0, 0.0}, PhysicsFlags::DefaultDynamic);
    locked.add_collider(Shape3D::make_box(1.0, 0.2, 0.5));
    locked.set_flag(PhysicsFlags::LockRotationX);
    locked.set_flag(PhysicsFlags::LockRotationZ);
    locked.set_angular_damping(0.0);
    locked.set_angular_velocity({1.0, 1.0, 1.0});
    sim.step();
    const linalg::vector3d w = locked.angular_velocity();
    check(w.x == 0.0 && w.z == 0.0 && near(w.y, 1.0, 1e-3), "3D: locked axes don't turn, the free axis does");
}

bool near3(const linalg::vector3d& a, const linalg::vector3d& b, double tol = 1e-9) {
    return near(a.x, b.x, tol) && near(a.y, b.y, tol) && near(a.z, b.z, tol);
}

void test_axes_presets() {
    check(near3(Axes3D::y_up().up(), {0.0, 1.0, 0.0}), "axes: y-up up is +y");
    check(near3(Axes3D::z_up().up(), {0.0, 0.0, 1.0}), "axes: z-up up is +z");
    check(near3(Axes3D::z_up_left().up(), {0.0, 0.0, 1.0}), "axes: z-up-left up is +z");
    check(near3(Axes3D::z_up().forward(), {0.0, 1.0, 0.0}), "axes: z-up forward is +y");
    check(near3(Axes3D::z_up_left().forward(), {1.0, 0.0, 0.0}), "axes: z-up-left forward is +x");
    check(near3(Axes3D::y_up_left().forward(), {0.0, 0.0, 1.0}), "axes: y-up-left forward is +z");
    check(!Axes3D::y_up().mirrored() && Axes3D::y_up_left().mirrored(), "axes: left-handed presets are mirrored");
    for (AxesPreset p : {AxesPreset::YUpRight, AxesPreset::YUpLeft, AxesPreset::ZUpRight, AxesPreset::ZUpLeft}) {
        const Axes3D a = Axes3D::preset(p);
        const auto parsed = Axes3D::parse(a.name());
        check(parsed && *parsed == a, "axes: name/parse round trip " + a.name());
        const linalg::vector3d v{0.3, -1.2, 2.5};
        check(near3(a.from_render(a.to_render(v)), v), "axes: render round trip " + a.name());
        const auto back = a.to_yaw_pitch_roll(a.from_yaw_pitch_roll(30.0, 20.0, 10.0, true), true);
        check(near(back[0], 30.0) && near(back[1], 20.0) && near(back[2], 10.0), "axes: yaw/pitch/roll round trip " + a.name());
        const linalg::vector3d g = a.gravity(9.0);
        check(near3(g, a.down() * 9.0) && near(a.height(g), -9.0), "axes: gravity points down " + a.name());
        const linalg::vector3d off{1.0, 2.0, 3.0};
        check(near3(a.orbit_unview(a.orbit_view(off, 40.0, -25.0, true), 40.0, -25.0, true), off), "axes: orbit round trip " + a.name());
    }
    check(Axes3D::parse("unity") == Axes3D::y_up_left() && Axes3D::parse("unreal") == Axes3D::z_up_left() && Axes3D::parse("blender") == Axes3D::z_up(), "axes: engine names parse");
}

void test_axes_turns() {
    const Axes3D z = Axes3D::z_up();
    check(near3(z.direction(90.0, 0.0, true), {-1.0, 0.0, 0.0}), "axes: z-up positive yaw turns left (counter-clockwise from above)");
    check(near3(z.direction(0.0, 90.0, true), {0.0, 0.0, 1.0}), "axes: z-up positive pitch looks up");
    const Axes3D u = Axes3D::z_up_left();
    check(near3(u.direction(90.0, 0.0, true), {0.0, 1.0, 0.0}), "axes: z-up-left positive yaw turns right");
    check(near3(u.direction(0.0, 90.0, true), {0.0, 0.0, 1.0}), "axes: z-up-left positive pitch looks up");
    check(near3(Axes3D::y_up_left().direction(0.0, 90.0, true), {0.0, 1.0, 0.0}), "axes: y-up-left positive pitch looks up");
    const Axes3D y = Axes3D::y_up();
    check(near3(y.direction(90.0, 0.0, true), {-1.0, 0.0, 0.0}), "axes: y-up positive yaw turns left");
}

void test_axes_orbit_matches_arbor() {
    const Axes3D z = Axes3D::z_up();
    const double yaw = 0.7, pitch = 0.4;
    const linalg::vector3d n{1.5, -0.5, 2.0};
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const double x1 = n.x * cy - n.y * sy;
    const double y1 = n.x * sy + n.y * cy;
    const linalg::vector3d arbor{x1, n.z * cp + y1 * sp, y1 * cp - n.z * sp};
    const linalg::vector3d v = z.orbit_view(n, yaw, pitch);
    check(near3({v.x, v.y, -v.z}, arbor), "axes: z-up orbit_view matches Arbor's camera");

    const linalg::vector3d q{0.4, 1.1, -2.2};
    const double x2 = cy * q.x + sy * q.z;
    const double z2 = -sy * q.x + cy * q.z;
    const linalg::vector3d sapling{x2, cp * q.y - sp * z2, sp * q.y + cp * z2};
    check(near3(Axes3D::y_up().orbit_view(q, yaw, pitch), sapling), "axes: y-up orbit_view matches Sapling's camera");
}

void test_axes_bodies_and_sims() {
    RigidBody3D b(PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
    const Axes3D z = Axes3D::z_up();
    b.set_yaw_pitch_roll(z, 90.0, 0.0, 0.0, false);
    check(near3(b.transform().rotate(z.forward()), {-1.0, 0.0, 0.0}), "axes: body yaw about +z with z-up axes");
    const auto ypr = b.yaw_pitch_roll(z, false);
    check(near(ypr[0], 90.0) && near(ypr[1], 0.0) && near(ypr[2], 0.0), "axes: body yaw_pitch_roll with z-up axes");

    Simulation3D sim3;
    check(near3(sim3.config().gravity, {0.0, -constants::gravity(), 0.0}), "axes: 3D default gravity is -y");
    sim3.set_axes(z);
    check(near3(sim3.config().gravity, {0.0, 0.0, -constants::gravity()}), "axes: 3D set_axes(z-up) moves gravity to -z");

    Simulation2D sim2;
    check(near(sim2.config().gravity.y, -constants::gravity()), "axes: 2D default gravity is -y");
    sim2.set_axes(Axes2D::y_down());
    check(near(sim2.config().gravity.y, constants::gravity()), "axes: 2D y-down gravity is +y");
    check(near(Axes2D::y_up().to_screen_angle(0.5), -0.5) && near(Axes2D::y_down().to_screen_angle(0.5), 0.5), "axes: 2D screen angles");

    const Axes3D before = default_axes3d();
    set_default_axes(AxesPreset::ZUpRight);
    Simulation3D sim_z;
    check(near3(sim_z.config().gravity, {0.0, 0.0, -constants::gravity()}), "axes: set_default_axes changes new simulations");
    set_default_axes(before);
}

void test_axes_flips() {
    Axes3D a;
    check(a == Axes3D::standard() && a == Axes3D::y_up() && a.flips() == AxisFlip::None, "axes: everything starts standard");
    a.flip_z();
    check(a == Axes3D::unity() && a.mirrored() && a.name() == "y-up-left", "axes: flipping z on standard is Unity");
    a.flip_z();
    check(a == Axes3D::standard() && !a.mirrored(), "axes: flipping twice undoes it");
    a.flip_x().flip_y();
    check(!a.mirrored() && a.flipped(Axis::X) && a.flipped(Axis::Y) && !a.flipped(Axis::Z), "axes: two flips keep handedness");
    check(a.name() == "y-up flip-x flip-y" && Axes3D::parse(a.name()) == a, "axes: custom names round trip");
    check(near3(a.up(), {0.0, -1.0, 0.0}) && near3(a.right(), {-1.0, 0.0, 0.0}), "axes: flipped y makes up -y, flipped x makes right -x");
    check(Axes3D(AxisFlip::Z) == Axes3D::unity() && Axes3D(AxesPreset::ZUpRight, AxisFlip::XZ).base() == AxesPreset::ZUpRight, "axes: constructing from flags");
    check(Axes3D::source().preset() == AxesPreset::Custom && near3(Axes3D::source().forward(), {1.0, 0.0, 0.0}) && near3(Axes3D::source().right(), {0.0, -1.0, 0.0}) && !Axes3D::source().mirrored(), "axes: Source is x forward, y left, z up, right-handed");
    check(Axes3D::parse("unreal flip-y") == Axes3D::source() && Axes3D::parse("flip-z") == Axes3D::unity() && !Axes3D::parse("sideways"), "axes: parsing presets and flips");
    check(Axes3D::blender() == Axes3D::z_up() && Axes3D::unreal() == Axes3D::z_up_left() && Axes3D::godot() == Axes3D::opengl(), "axes: named styles");
    const Axes3D odd = Axes3D::z_up().with_flips(AxisFlip::X);
    const linalg::vector3d v{0.4, -1.5, 2.0};
    check(near3(odd.from_render(odd.to_render(v)), v), "axes: flipped render round trip");
    const auto back = odd.to_yaw_pitch_roll(odd.from_yaw_pitch_roll(25.0, 15.0, 5.0, true), true);
    check(near(back[0], 25.0) && near(back[1], 15.0) && near(back[2], 5.0), "axes: flipped yaw/pitch/roll round trip");
    check(near3(odd.direction(0.0, 90.0, true), {0.0, 0.0, 1.0}), "axes: pitch still looks up after flips");

    Axes2D b;
    check(b == Axes2D::y_up() && b.screen_y_sign() == -1.0 && b.screen_x_sign() == 1.0, "axes 2d: starts y-up");
    b.flip_y();
    check(b == Axes2D::y_down() && b.y_axis() == YAxis2D::Down && b.mirrored(), "axes 2d: flipping y is screen style");
    b.flip_x();
    check(!b.mirrored() && b.name() == "y-down flip-x" && Axes2D::parse(b.name()) == b && near(b.right().x, -1.0), "axes 2d: flipping x");
    check(near(b.to_screen_angle(0.5), -0.5) && near(Axes2D::y_up().with_flips(AxisFlip::X).to_screen_angle(0.5), 0.5), "axes 2d: screen angles follow both flips");
}

double slide_distance(CombineMode box_mode, double box_friction, double ground_friction, CombineMode sim_mode = CombineMode::GeometricMean, Simulation3D::CombineCallback cb = {}) {
    Simulation3D::Config cfg;
    cfg.friction_combine = sim_mode;
    Simulation3D sim(cfg);
    sim.set_axes(Axes3D::y_up());
    if (cb) sim.set_friction_combiner(cb);
    RigidBody3D& ground = sim.add_body({0.0, -0.5, 0.0}, PhysicsFlags::DefaultStatic);
    Material3D gm;
    gm.friction = ground_friction;
    ground.add_collider(Shape3D::make_box(50.0, 0.5, 50.0), gm);
    RigidBody3D& box = sim.add_body({0.0, 0.5, 0.0}, PhysicsFlags::DefaultDynamic);
    Material3D bm(1000.0);
    bm.friction = box_friction;
    bm.friction_combine = box_mode;
    box.add_collider(Shape3D::make_box(0.5, 0.5, 0.5), bm);
    for (int i = 0; i < 10; ++i) sim.step();
    box.set_linear_velocity({5.0, 0.0, 0.0});
    for (int i = 0; i < 120; ++i) sim.step();
    return box.position().x;
}

double bounce_height(CombineMode ball_mode, CombineMode sim_mode = CombineMode::Maximum) {
    Simulation3D::Config cfg;
    cfg.restitution_combine = sim_mode;
    Simulation3D sim(cfg);
    sim.set_axes(Axes3D::y_up());
    RigidBody3D& ground = sim.add_body({0.0, -0.5, 0.0}, PhysicsFlags::DefaultStatic);
    ground.add_collider(Shape3D::make_box(10.0, 0.5, 10.0));
    RigidBody3D& ball = sim.add_body({0.0, 3.0, 0.0}, PhysicsFlags::DefaultDynamic);
    Material3D bm(1000.0);
    bm.restitution = 0.9;
    bm.restitution_combine = ball_mode;
    ball.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5), bm);
    double best = 0.0;
    bool hit = false;
    for (int i = 0; i < 240; ++i) {
        sim.step();
        if (ball.linear_velocity().y > 0.5) hit = true;
        if (hit) best = std::max(best, ball.position().y);
    }
    return best;
}

void test_combine_modes() {
    check(near(combine_values(CombineMode::Average, 0.2, 0.8), 0.5), "combine: average");
    check(near(combine_values(CombineMode::GeometricMean, 0.25, 1.0), 0.5), "combine: geometric mean");
    check(near(combine_values(CombineMode::HarmonicMean, 0.25, 1.0), 0.4), "combine: harmonic mean");
    check(near(combine_values(CombineMode::QuadraticMean, 0.0, 1.0), std::sqrt(0.5)), "combine: quadratic mean");
    check(near(combine_values(CombineMode::Minimum, 0.2, 0.8), 0.2) && near(combine_values(CombineMode::Maximum, 0.2, 0.8), 0.8), "combine: min and max");
    check(near(combine_values(CombineMode::Multiply, 0.5, 0.4), 0.2) && near(combine_values(CombineMode::Sum, 0.5, 0.4), 0.9), "combine: multiply and sum");
    check(resolve_combine(CombineMode::Average, CombineMode::Maximum, CombineMode::Average) == CombineMode::Maximum, "combine: the higher priority mode wins, like Unity and Unreal");
    check(resolve_combine(CombineMode::Minimum, CombineMode::Multiply, CombineMode::Average) == CombineMode::Multiply, "combine: multiply beats minimum");
    check(resolve_combine(CombineMode::Default, CombineMode::Default, CombineMode::HarmonicMean) == CombineMode::HarmonicMean, "combine: default uses the simulation's mode");
    check(parse_combine_mode(to_string(CombineMode::QuadraticMean)) == CombineMode::QuadraticMean && parse_combine_mode("rms") == CombineMode::QuadraticMean && !parse_combine_mode("nope"), "combine: names");

    const double ice = slide_distance(CombineMode::Default, 0.0, 1.0);
    const double grippy = slide_distance(CombineMode::Maximum, 0.0, 1.0);
    const double avg = slide_distance(CombineMode::Average, 0.0, 1.0, CombineMode::Average);
    check(ice > 4.0, "combine: geometric mean of 0 and 1 is ice (slid " + std::to_string(ice) + ")");
    check(grippy < 1.5 && grippy < avg && avg < ice, "combine: maximum grips, average is in between (" + std::to_string(grippy) + ", " + std::to_string(avg) + ")");
    const double custom = slide_distance(CombineMode::Custom, 0.0, 1.0, CombineMode::GeometricMean, [](const Collider3D&, const Collider3D&) { return 1.0; });
    check(custom < 1.5, "combine: a custom combiner decides the friction (" + std::to_string(custom) + ")");
    const double sim_wide = slide_distance(CombineMode::Default, 0.0, 1.0, CombineMode::Maximum);
    check(sim_wide < 1.5, "combine: the simulation's default mode applies when colliders don't pick one");
    const double bouncy = bounce_height(CombineMode::Default), dead = bounce_height(CombineMode::Default, CombineMode::Multiply), half = bounce_height(CombineMode::Sum);
    check(bouncy > 1.5 && dead < 0.6 && half > 1.5, "combine: restitution max bounces, multiply with 0 doesn't (" + std::to_string(bouncy) + ", " + std::to_string(dead) + ")");
}

} // namespace

int main() {
    test_2d_rotation_locks();
    test_2d_lock_after_mass();
    test_3d_angles();
    test_3d_spin();
    test_axes_presets();
    test_axes_turns();
    test_axes_orbit_matches_arbor();
    test_axes_bodies_and_sims();
    test_axes_flips();
    test_combine_modes();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
