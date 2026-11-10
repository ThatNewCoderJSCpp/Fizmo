#include "../Includes/physics.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

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
    check(near(e[0], 10.0, 1e-3) && near(e[1], 20.0, 1e-3) && near(e[2], 30.0, 1e-3), "3D: euler_angles round trip");

    b.set_euler_xyz(15.0, 25.0, 35.0, false);
    const auto x = b.euler_xyz(false);
    check(near(x[0], 15.0, 1e-3) && near(x[1], 25.0, 1e-3) && near(x[2], 35.0, 1e-3), "3D: euler_xyz round trip");

    b.set_rotation({0.0, 0.0, 1.0}, 90.0, false);
    const linalg::vector3d r = to_double(b.transform().rotate({1.0f, 0.0f, 0.0f}));
    check(near(r.x, 0.0) && near(r.y, 1.0) && near(r.z, 0.0), "3D: 90 degrees about z turns +x into +y");

    b.set_yaw_pitch_roll(30.0, 20.0, 10.0, false);
    const auto y = b.yaw_pitch_roll(false);
    check(near(y[0], 30.0) && near(y[1], 20.0) && near(y[2], 10.0), "3D: yaw_pitch_roll round trip");

    b.set_yaw_pitch_roll(90.0, 0.0, 0.0, false);
    const linalg::vector3d f = to_double(b.transform().rotate({0.0f, 0.0f, -1.0f}));
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
    const linalg::vector3d w = to_double(locked.angular_velocity());
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
    check(near3(to_double(b.transform().rotate(to_real(z.forward()))), {-1.0, 0.0, 0.0}, 1e-5), "axes: body yaw about +z with z-up axes");
    const auto ypr = b.yaw_pitch_roll(z, false);
    check(near(ypr[0], 90.0) && near(ypr[1], 0.0) && near(ypr[2], 0.0), "axes: body yaw_pitch_roll with z-up axes");

    Simulation3D sim3;
    check(near3(to_double(sim3.config().gravity), {0.0, -constants::gravity(), 0.0}, 1e-5), "axes: 3D default gravity is -y");
    sim3.set_axes(z);
    check(near3(to_double(sim3.config().gravity), {0.0, 0.0, -constants::gravity()}, 1e-5), "axes: 3D set_axes(z-up) moves gravity to -z");

    Simulation2D sim2;
    check(near(sim2.config().gravity.y, -constants::gravity()), "axes: 2D default gravity is -y");
    sim2.set_axes(Axes2D::y_down());
    check(near(sim2.config().gravity.y, constants::gravity()), "axes: 2D y-down gravity is +y");
    check(near(Axes2D::y_up().to_screen_angle(0.5), -0.5) && near(Axes2D::y_down().to_screen_angle(0.5), 0.5), "axes: 2D screen angles");

    const Axes3D before = default_axes3d();
    set_default_axes(AxesPreset::ZUpRight);
    Simulation3D sim_z;
    check(near3(to_double(sim_z.config().gravity), {0.0, 0.0, -constants::gravity()}, 1e-5), "axes: set_default_axes changes new simulations");
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
        if (hit) best = std::max(best, static_cast<double>(ball.position().y));
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

double shoot_through_wall(bool bullet, bool continuous, int substeps) {
    Simulation3D::Config cfg;
    cfg.gravity = {0.0, 0.0, 0.0};
    cfg.continuous = continuous;
    cfg.substeps = substeps;
    Simulation3D sim(cfg);
    RigidBody3D& wall = sim.add_body({5.0, 0.0, 0.0}, PhysicsFlags::DefaultStatic);
    wall.add_collider(Shape3D::make_box(0.025, 2.0, 2.0));
    PhysicsFlags flags = PhysicsFlags::DefaultDynamic;
    if (bullet) flags |= PhysicsFlags::Bullet;
    RigidBody3D& ball = sim.add_body({0.3, 0.0, 0.0}, flags);
    ball.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.1));
    ball.set_linear_damping(0.0);
    ball.set_linear_velocity({300.0, 0.0, 0.0});
    for (int i = 0; i < 30; ++i) sim.step();
    return ball.position().x;
}

void test_continuous_collision() {
    const double tunnel = shoot_through_wall(false, true, 1);
    const double stopped = shoot_through_wall(true, true, 1);
    const double off = shoot_through_wall(true, false, 1);
    check(tunnel > 5.0, "ccd: a fast ball with no Bullet flag tunnels through a thin wall (x = " + std::to_string(tunnel) + ")");
    check(stopped < 5.0, "ccd: a Bullet ball is stopped by a thin wall (x = " + std::to_string(stopped) + ")");
    check(off > 5.0, "ccd: continuous = false turns it off (x = " + std::to_string(off) + ")");
    Simulation3D::Config cfg;
    cfg.gravity = {0.0, 0.0, 0.0};
    cfg.continuous_all = true;
    Simulation3D sim(cfg);
    RigidBody3D& wall = sim.add_body({5.0, 0.0, 0.0}, PhysicsFlags::DefaultStatic);
    wall.add_collider(Shape3D::make_box(0.025, 2.0, 2.0));
    RigidBody3D& ball = sim.add_body({0.3, 0.0, 0.0}, PhysicsFlags::DefaultDynamic);
    ball.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.1));
    ball.set_linear_velocity({300.0, 0.0, 0.0});
    for (int i = 0; i < 30; ++i) sim.step();
    check(ball.position().x < 5.0, "ccd: continuous_all treats every dynamic body as a bullet");
}

double fall_after_one_second(int substeps) {
    Simulation3D::Config cfg;
    cfg.gravity = {0.0, -10.0, 0.0};
    cfg.substeps = substeps;
    Simulation3D sim(cfg);
    RigidBody3D& b = sim.add_body({0.0, 0.0, 0.0}, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
    b.set_linear_damping(0.0);
    for (int i = 0; i < 60; ++i) sim.step();
    return b.position().y;
}

void test_substeps() {
    const double one = fall_after_one_second(1), four = fall_after_one_second(4);
    check(std::fabs(four + 5.0) <= std::fabs(one + 5.0) + 1e-9, "substeps: 4 substeps track free fall at least as well as 1 (" + std::to_string(one) + ", " + std::to_string(four) + ")");
    check(near(four, -5.0, 0.05), "substeps: free fall for 1 s lands near -5 m");
    Simulation2D::Config c2;
    c2.gravity = {0.0, -10.0};
    c2.substeps = 3;
    Simulation2D sim2(c2);
    RigidBody2D& b2 = sim2.add_body({0.0, 0.0}, PhysicsFlags::DefaultDynamic);
    b2.add_collider(Shape2D::make_circle({0.0, 0.0}, 0.5));
    b2.set_linear_damping(0.0);
    for (int i = 0; i < 60; ++i) sim2.step();
    check(near(b2.position().y, -5.0, 0.1), "substeps: 2D substeps fall the same way");
}

Simulation3D::Config ground_cfg() {
    Simulation3D::Config cfg;
    cfg.gravity = {0.0, -10.0, 0.0};
    return cfg;
}

RigidBody3D& add_ground(Simulation3D& sim) {
    RigidBody3D& g = sim.add_body({0.0, -0.5, 0.0}, PhysicsFlags::DefaultStatic);
    g.add_collider(Shape3D::make_box(20.0, 0.5, 20.0));
    return g;
}

double rest_height(const Shape3D& shape, int steps = 240) {
    Simulation3D sim(ground_cfg());
    add_ground(sim);
    RigidBody3D& b = sim.add_body({0.0, 2.0, 0.0}, PhysicsFlags::DefaultDynamic);
    b.add_collider(shape);
    for (int i = 0; i < steps; ++i) sim.step();
    return b.position().y;
}

double stack_top(Simulation3D::Config cfg, int count, int steps = 300) {
    Simulation3D sim(cfg);
    add_ground(sim);
    RigidBody3D* top = nullptr;
    for (int i = 0; i < count; ++i) {
        RigidBody3D& b = sim.add_body({0.0, 0.5 + i * 1.02, 0.0}, PhysicsFlags::DefaultDynamic);
        b.add_collider(Shape3D::make_box(0.5, 0.5, 0.5));
        top = &b;
    }
    for (int i = 0; i < steps; ++i) sim.step();
    return top->position().y;
}

void test_new_shapes() {
    check(near(rest_height(Shape3D::make_cylinder(0.5, 0.4)), 0.5, 0.06), "shapes: cylinder rests on its flat end");
    check(near(rest_height(Shape3D::make_rounded_box(0.3, 0.3, 0.3, 0.1)), 0.3, 0.03), "shapes: rounded box rests on its outer extent");
    const double cone = rest_height(Shape3D::make_cone(0.5, 0.4));
    check(cone > 0.2 && cone < 0.9, "shapes: cone comes to rest on the ground (y = " + std::to_string(cone) + ")");

    Simulation3D sim(ground_cfg());
    std::vector<Real> h(9 * 9, 0.0f);
    RigidBody3D& hf = sim.add_body({0.0, 0.0, 0.0}, PhysicsFlags::DefaultStatic);
    hf.add_collider(Shape3D::make_heightfield(h, 9, 9, 1.0, 1.0));
    RigidBody3D& ball = sim.add_body({0.3, 2.0, 0.2}, PhysicsFlags::DefaultDynamic);
    ball.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
    for (int i = 0; i < 240; ++i) sim.step();
    check(near(ball.position().y, 0.5, 0.06), "shapes: sphere rests on a flat heightfield (y = " + std::to_string(ball.position().y) + ")");

    Simulation3D sim2(ground_cfg());
    RigidBody3D& mesh = sim2.add_body({0.0, 0.0, 0.0}, PhysicsFlags::DefaultStatic);
    mesh.add_collider(Shape3D::make_mesh({{-5.0, 0.0, -5.0}, {5.0, 0.0, -5.0}, {5.0, 0.0, 5.0}, {-5.0, 0.0, 5.0}}, {{{0u, 2u, 1u}}, {{0u, 3u, 2u}}}));
    RigidBody3D& box = sim2.add_body({0.2, 2.0, 0.1}, PhysicsFlags::DefaultDynamic);
    box.add_collider(Shape3D::make_box(0.5, 0.5, 0.5));
    for (int i = 0; i < 240; ++i) sim2.step();
    check(near(box.position().y, 0.5, 0.06), "shapes: box rests on a triangle mesh (y = " + std::to_string(box.position().y) + ")");
}

void test_queries() {
    Simulation3D sim(ground_cfg());
    add_ground(sim);
    RigidBody3D& a = sim.add_body({3.0, 1.0, 0.0}, PhysicsFlags::DefaultStatic);
    a.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
    RigidBody3D& c = sim.add_body({-3.0, 1.0, 0.0}, PhysicsFlags::DefaultStatic);
    c.add_collider(Shape3D::make_box(0.5, 0.5, 0.5));
    c.set_collision_layer(4);
    auto hit = sim.raycast({0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, 10.0);
    check(hit && hit->body == &a && near(hit->distance, 2.5, 1e-3), "queries: raycast hits the sphere at 2.5 m");
    auto down = sim.raycast({0.0, 5.0, 0.0}, {0.0, -1.0, 0.0}, 10.0);
    check(down && near(down->point.y, 0.0, 1e-3) && near(down->normal.y, 1.0, 1e-3), "queries: raycast down hits the ground top");
    QueryFilter3D only4;
    only4.layers = 4;
    auto left = sim.raycast({0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, 10.0, only4);
    check(left && left->body == &c && near(left->distance, 2.5, 1e-3), "queries: layer filter keeps the box");
    check(!sim.raycast({0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, 10.0, only4), "queries: layer filter drops the sphere");
    check(sim.raycast_all({-10.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, 20.0).size() == 2, "queries: raycast_all finds both");
    check(sim.overlap_sphere({3.0, 1.0, 0.0}, 0.2).size() == 1, "queries: overlap_sphere finds one body");
    check(sim.overlap_sphere({0.0, 3.0, 0.0}, 0.2).empty(), "queries: overlap_sphere in empty space is empty");
    auto cast = sim.shape_cast(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.25), Transform3D({0.0f, 1.0f, 0.0f}, RQuat(1.0f, 0.0f, 0.0f, 0.0f)), {5.0, 0.0, 0.0});
    check(cast && cast->body == &a && near(cast->fraction * 5.0, 2.25, 0.02), "queries: sphere cast stops 2.25 m in");
}

void test_layers() {
    Simulation3D sim(ground_cfg());
    RigidBody3D& g = add_ground(sim);
    g.set_collision_layer(2);
    RigidBody3D& b = sim.add_body({0.0, 1.0, 0.0}, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_box(0.5, 0.5, 0.5));
    b.set_collision_mask(~2ULL);
    for (int i = 0; i < 60; ++i) sim.step();
    check(b.position().y < -1.0, "layers: a masked-out ground is ignored");
    check(sim.stats().layer_rejects > 0 || sim.stats().pairs == 0, "layers: the reject is counted in the broadphase");
    Simulation3D sim2(ground_cfg());
    add_ground(sim2);
    RigidBody3D& ghost = sim2.add_body({0.0, 1.0, 0.0}, PhysicsFlags::DefaultDynamic);
    ghost.add_collider(Shape3D::make_box(0.5, 0.5, 0.5));
    ghost.set_collision_layer(0);
    for (int i = 0; i < 5; ++i) sim2.step();
    check(sim2.stats().proxies == 1, "layers: a layer-0 body never enters the tree");
}

void test_joints() {
    Simulation3D sim(ground_cfg());
    RigidBody3D& bob = sim.add_body({2.0, 5.0, 0.0}, PhysicsFlags::DefaultDynamic);
    bob.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.2));
    sim.add_ball_joint(bob, nullptr, {0.0, 5.0, 0.0});
    double worst = 0.0;
    for (int i = 0; i < 240; ++i) {
        sim.step();
        worst = std::max(worst, std::fabs(static_cast<double>(vec3::length(bob.position() - RVec3{0.0f, 5.0f, 0.0f})) - 2.0));
    }
    check(worst < 0.05, "joints: ball pendulum keeps its length (drift " + std::to_string(worst) + ")");
    check(bob.position().y < 4.0, "joints: pendulum swings down");

    Simulation3D sim2(ground_cfg());
    RigidBody3D& door = sim2.add_body({1.0, 5.0, 0.0}, PhysicsFlags::DefaultDynamic);
    door.add_collider(Shape3D::make_box(1.0, 1.0, 0.05));
    Joint3D& hinge = sim2.add_hinge_joint(door, nullptr, {0.0, 5.0, 0.0}, {0.0, 1.0, 0.0});
    hinge.set_motor(true, 2.0, 1.0e6);
    for (int i = 0; i < 60; ++i) sim2.step();
    check(near(door.angular_velocity().y, -2.0, 0.05) && near(door.position().y, 5.0, 0.05), "joints: hinge motor drives B relative to A at the target speed (w = " + std::to_string(door.angular_velocity().y) + ")");

    Simulation3D sim3(ground_cfg());
    RigidBody3D& slide = sim3.add_body({0.0, 5.0, 0.0}, PhysicsFlags::DefaultDynamic);
    slide.add_collider(Shape3D::make_box(0.2, 0.2, 0.2));
    Joint3D& s = sim3.add_slider_joint(slide, nullptr, {0.0, 5.0, 0.0}, {0.0, 1.0, 0.0});
    s.set_limits(true, -1.0, 1.0);
    for (int i = 0; i < 180; ++i) sim3.step();
    check(near(slide.position().y, 4.0, 0.05) && near(slide.position().x, 0.0, 0.01), "joints: slider falls to its lower limit (y = " + std::to_string(slide.position().y) + ")");

    Simulation3D sim4(ground_cfg());
    RigidBody3D& w = sim4.add_body({0.0, 5.0, 0.0}, PhysicsFlags::DefaultDynamic);
    w.add_collider(Shape3D::make_box(0.2, 0.2, 0.2));
    sim4.add_spring_joint(w, nullptr, {0.0, 5.0, 0.0}, {0.0, 6.0, 0.0}, 1.0, 2.0, 1.0);
    for (int i = 0; i < 300; ++i) sim4.step();
    const double k = w.mass() * std::pow(2.0 * 3.14159265358979 * 2.0, 2.0);
    check(near(w.position().y, 5.0 - w.mass() * 10.0 / k, 0.05), "joints: spring settles at rest length plus sag (y = " + std::to_string(w.position().y) + ")");
}

void test_solvers() {
    Simulation3D::Config classic = ground_cfg();
    const double c = stack_top(classic, 5);
    check(near(c, 4.5, 0.15), "solver: classic 5-box stack stands (top " + std::to_string(c) + ")");
    Simulation3D::Config soft = ground_cfg();
    soft.solver = Solver3D::SoftStep;
    const double s = stack_top(soft, 5);
    check(near(s, 4.5, 0.15), "solver: soft-step 5-box stack stands (top " + std::to_string(s) + ")");
    Simulation3D::Config wide = ground_cfg();
    wide.simd = true;
    const double w = stack_top(wide, 10);
    check(near(w, 9.5, 0.3), "solver: wide solver 10-box stack stands (top " + std::to_string(w) + ")");
    Simulation3D::Config oct = ground_cfg();
    oct.broadphase = Broadphase3D::Octree;
    check(near(stack_top(oct, 3), 2.5, 0.1), "solver: octree broadphase still works");
    Simulation3D::Config gjk = ground_cfg();
    gjk.hull_collisions = ConvexMethod3D::Gjk;
    const double g = stack_top(gjk, 3);
    check(near(g, 2.5, 0.1), "solver: GJK hull path stacks boxes (top " + std::to_string(g) + ")");
    Simulation3D::Config threads = ground_cfg();
    threads.threads = 2;
    threads.parallel_threshold = 1;
    check(near(stack_top(threads, 5), 4.5, 0.15), "solver: two threads stack the same");
}

std::vector<double> scatter_run(bool deterministic, int threads) {
    Simulation3D::Config cfg = ground_cfg();
    cfg.deterministic = deterministic;
    cfg.threads = threads;
    cfg.parallel_threshold = 1;
    Simulation3D sim(cfg);
    add_ground(sim);
    std::vector<RigidBody3D*> bodies;
    for (int i = 0; i < 20; ++i) {
        RigidBody3D& b = sim.add_body({(i % 5) * 0.9 - 2.0, 1.0 + i * 0.6, (i / 5) * 0.4}, PhysicsFlags::DefaultDynamic);
        b.add_collider(i % 2 ? Shape3D::make_box(0.3, 0.3, 0.3) : Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.3));
        bodies.push_back(&b);
    }
    for (int i = 0; i < 120; ++i) sim.step();
    std::vector<double> out;
    for (auto* b : bodies) {
        out.push_back(b->position().x);
        out.push_back(b->position().y);
        out.push_back(b->position().z);
    }
    return out;
}

void test_determinism_and_misc() {
    check(scatter_run(true, 1) == scatter_run(true, 1), "determinism: two runs match bit for bit");
    check(scatter_run(true, 2) == scatter_run(true, 1), "determinism: threads do not change the result");

    auto roll = [](double rolling) {
        Simulation3D sim(ground_cfg());
        RigidBody3D& g = sim.add_body({0.0, -0.5, 0.0}, PhysicsFlags::DefaultStatic);
        Collider3D& gc = g.add_collider(Shape3D::make_box(20.0, 0.5, 20.0));
        RigidBody3D& b = sim.add_body({0.0, 0.5, 0.0}, PhysicsFlags::DefaultDynamic);
        Collider3D& bc = b.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
        b.set_linear_damping(0.0);
        b.set_angular_damping(0.0);
        bc.material.rolling_friction = rolling;
        gc.material.rolling_friction = rolling;
        b.set_linear_velocity({3.0, 0.0, 0.0});
        b.set_angular_velocity({0.0, 0.0, -6.0});
        for (int i = 0; i < 120; ++i) sim.step();
        return b.position().x;
    };
    const double free_roll = roll(0.0), braked = roll(0.1);
    check(braked < free_roll - 0.5, "friction: rolling friction slows a ball (" + std::to_string(free_roll) + " vs " + std::to_string(braked) + ")");

    Simulation3D sim(ground_cfg());
    add_ground(sim);
    RigidBody3D& b = sim.add_body({0.0, 1.0, 0.0}, PhysicsFlags::DefaultDynamic | PhysicsFlags::ContactEvents);
    b.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
    std::size_t begins = 0;
    for (int i = 0; i < 60; ++i) {
        sim.step();
        begins += sim.begin_events().size();
    }
    check(begins == 1, "events: begin buffer reports one touch (" + std::to_string(begins) + ")");

    const RVec3 before = b.position();
    sim.shift_origin({100.0, 0.0, 0.0});
    check(near(b.position().x, before.x - 100.0, 1e-9), "rebase: shift_origin moves bodies");
    for (int i = 0; i < 30; ++i) sim.step();
    check(near(b.position().y, 0.5, 0.05), "rebase: ball still rests after the shift");

    check(quality_iterations(BodyQuality::Ultra, 10) == 40 && quality_iterations(BodyQuality::Arcade, 10) == 5, "quality: iteration scaling");
}

double shoot_2d(bool bullet, bool tree) {
    Simulation2D::Config cfg;
    cfg.gravity = {0.0, 0.0};
    cfg.dynamic_tree = tree;
    Simulation2D sim(cfg);
    RigidBody2D& wall = sim.add_body({5.0, 0.0}, PhysicsFlags::DefaultStatic);
    wall.add_collider(Shape2D::make_box(0.025, 2.0));
    PhysicsFlags flags = PhysicsFlags::DefaultDynamic;
    if (bullet) flags |= PhysicsFlags::Bullet;
    RigidBody2D& ball = sim.add_body({0.3, 0.0}, flags);
    ball.add_collider(Shape2D::make_circle({0.0, 0.0}, 0.1));
    ball.set_linear_damping(0.0);
    ball.set_linear_velocity({300.0, 0.0});
    for (int i = 0; i < 30; ++i) sim.step();
    return ball.position().x;
}

void test_2d_roadmap() {
    check(shoot_2d(false, true) > 5.0, "2d ccd: a plain fast ball tunnels");
    const double stopped = shoot_2d(true, true);
    check(stopped < 5.0, "2d ccd: a Bullet ball stops at a thin wall (x = " + std::to_string(stopped) + ")");
    check(shoot_2d(true, false) < 5.0, "2d ccd: works with the quadtree broadphase too");

    auto stack = [](bool tree) {
        Simulation2D::Config cfg;
        cfg.gravity = {0.0, -10.0};
        cfg.dynamic_tree = tree;
        Simulation2D sim(cfg);
        RigidBody2D& g = sim.add_body({0.0, -0.5}, PhysicsFlags::DefaultStatic);
        g.add_collider(Shape2D::make_box(20.0, 0.5));
        RigidBody2D* top = nullptr;
        for (int i = 0; i < 5; ++i) {
            RigidBody2D& b = sim.add_body({0.0, 0.5 + i * 1.02}, PhysicsFlags::DefaultDynamic);
            b.add_collider(Shape2D::make_box(0.5, 0.5));
            top = &b;
        }
        for (int i = 0; i < 300; ++i) sim.step();
        return top->position().y;
    };
    check(near(stack(true), 4.5, 0.1), "2d tree: 5-box stack stands on the dynamic tree");
    check(near(stack(false), 4.5, 0.1), "2d tree: quadtree path unchanged");

    Simulation2D::Config cfg;
    cfg.gravity = {0.0, -10.0};
    Simulation2D sim(cfg);
    RigidBody2D& g = sim.add_body({0.0, -0.5}, PhysicsFlags::DefaultStatic);
    g.add_collider(Shape2D::make_box(20.0, 0.5));
    RigidBody2D& circle = sim.add_body({3.0, 1.0}, PhysicsFlags::DefaultStatic);
    circle.add_collider(Shape2D::make_circle({0.0, 0.0}, 0.5));
    RigidBody2D& pill = sim.add_body({-3.0, 1.0}, PhysicsFlags::DefaultStatic);
    pill.add_collider(Shape2D::make_capsule({0.0, -0.5}, {0.0, 0.5}, 0.25));
    pill.set_collision_layer(8);
    auto r = sim.raycast({0.0, 1.0}, {1.0, 0.0}, 10.0);
    check(r && r->body == &circle && near(r->distance, 2.5, 1e-4), "2d queries: ray hits the circle");
    auto d = sim.raycast({0.0, 5.0}, {0.0, -1.0}, 10.0);
    check(d && d->body == &g && near(d->point.y, 0.0, 1e-4) && near(d->normal.y, 1.0, 1e-4), "2d queries: ray hits the ground box");
    auto l = sim.raycast({0.0, 1.0}, {-1.0, 0.0}, 10.0);
    check(l && l->body == &pill && near(l->distance, 2.75, 1e-3), "2d queries: ray hits the capsule (d = " + std::to_string(l ? l->distance : -1.0) + ")");
    QueryFilter2D only8;
    only8.layers = 8;
    check(!sim.raycast({0.0, 1.0}, {1.0, 0.0}, 10.0, only8), "2d queries: layer filter drops the circle");
    check(sim.raycast_all({-10.0, 1.0}, {1.0, 0.0}, 20.0).size() == 2, "2d queries: raycast_all finds both");
    check(sim.overlap_circle({3.0, 1.4}, 0.2).size() == 1, "2d queries: overlap_circle finds the circle");
    auto cast = sim.shape_cast(Shape2D::make_circle({0.0, 0.0}, 0.25), Transform2D({0.0, 1.0}, 0.0), {5.0, 0.0});
    check(cast && cast->body == &circle && near(cast->fraction * 5.0, 2.25, 0.01), "2d queries: circle cast stops 2.25 m in");

    RigidBody2D& ball = sim.add_body({0.0, 1.0}, PhysicsFlags::DefaultDynamic | PhysicsFlags::ContactEvents);
    ball.add_collider(Shape2D::make_circle({0.0, 0.0}, 0.5));
    std::size_t begins = 0;
    for (int i = 0; i < 60; ++i) {
        sim.step();
        begins += sim.begin_events().size();
    }
    check(begins == 1, "2d events: begin buffer reports one touch (" + std::to_string(begins) + ")");
    sim.shift_origin({50.0, 0.0});
    for (int i = 0; i < 30; ++i) sim.step();
    check(near(ball.position().x, -50.0, 1e-6) && near(ball.position().y, 0.5, 0.05), "2d rebase: shift_origin keeps the ball resting");

    Simulation2D sim2(cfg);
    RigidBody2D& g2 = sim2.add_body({0.0, -0.5}, PhysicsFlags::DefaultStatic);
    g2.add_collider(Shape2D::make_box(20.0, 0.5));
    g2.set_collision_layer(2);
    RigidBody2D& b2 = sim2.add_body({0.0, 1.0}, PhysicsFlags::DefaultDynamic);
    b2.add_collider(Shape2D::make_box(0.5, 0.5));
    b2.set_collision_mask(~2ULL);
    for (int i = 0; i < 60; ++i) sim2.step();
    check(b2.position().y < -1.0, "2d layers: a masked-out ground is ignored");
}

void test_storage_and_handles() {
    const std::size_t pooled = RigidBody3D::pooled_bodies();
    Simulation3D sim(ground_cfg());
    RigidBody3D& a = sim.add_body({ 0.0, 1.0, 0.0 }, PhysicsFlags::DefaultDynamic);
    RigidBody3D& b = sim.add_body({ 2.0, 1.0, 0.0 }, PhysicsFlags::DefaultDynamic);
    const BodyHandle ha = a.handle(), hb = b.handle();
    check(RigidBody3D::pooled_bodies() == pooled + 2, "storage: bodies come from the pool");
    check(sim.get(ha) == &a && sim.get(hb) == &b && ha != hb, "handles: each body gets its own handle");
    sim.remove_body(a);
    check(!sim.alive(ha) && sim.get(ha) == nullptr && sim.get(hb) == &b, "handles: a removed body's handle goes stale");
    check(RigidBody3D::pooled_bodies() == pooled + 1, "storage: removing a body returns it to the pool");
    RigidBody3D& c = sim.add_body(PhysicsFlags::DefaultDynamic);
    check(c.handle().index == ha.index && c.handle() != ha && !sim.alive(ha), "handles: a reused slot gets a new generation");
    check(BodyHandle::unpack(hb.packed()) == hb, "handles: pack and unpack round trip");

    Simulation2D sim2;
    RigidBody2D& p = sim2.add_body(PhysicsFlags::DefaultDynamic);
    const BodyHandle hp = p.handle();
    check(sim2.get(hp) == &p, "handles: 2D bodies get handles too");
    sim2.remove_body(p);
    check(!sim2.alive(hp), "handles: 2D handles go stale");
}

void test_rebasing_and_snapshots() {
    Simulation3D::Config cfg = ground_cfg();
    cfg.rebase_distance = 1000.0;
    Simulation3D sim(cfg);
    RigidBody3D& g = sim.add_body({ 5000.0, -0.5, 0.0 }, PhysicsFlags::DefaultStatic);
    g.add_collider(Shape3D::make_box(20.0, 0.5, 20.0));
    RigidBody3D& b = sim.add_body({ 5000.0, 1.0, 0.0 }, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_box(0.5, 0.5, 0.5));
    sim.set_focus({ 5000.0, 0.0, 0.0 });
    for (int i = 0; i < 120; ++i) sim.step();
    check(near(sim.origin().x, 5000.0, 1e-9) && std::abs(b.position().x) < 1.0, "rebase: the origin follows the focus (origin " + std::to_string(sim.origin().x) + ")");
    check(near(sim.absolute_position(b).x, 5000.0, 0.01) && near(b.position().y, 0.5, 0.05), "rebase: the box still rests at its absolute spot");
    std::vector<BodySnapshot3F> snap;
    sim.snapshot(snap);
    check(snap.size() == 2 && snap[1].handle == b.handle() && std::abs(snap[1].y - 0.5f) < 0.05f && std::abs(snap[1].qw) > 0.99f, "snapshot: float copies of every body");
    const TreeBox3F tb(AABB3D({ 0.1, 0.2, 0.3 }, { 0.7, 0.8, 0.9 }));
    check(tb.min.x <= 0.1_r && tb.min.y <= 0.2_r && tb.max.x >= 0.7_r && tb.max.z >= 0.9_r, "float tree boxes round outward");
    const Shape3D core = Shape3D::make_rounded_box_core(0.2, 0.2, 0.2, 0.1);
    check(near(core.rounded_box.half_extents.x, 0.2, 1e-6) && near(core.rounded_box.radius, 0.1, 1e-6), "rounded box: the core version takes the inner size");
}

void test_cloth() {
    Simulation3D sim(ground_cfg());
    ClothDef3D d;
    d.columns = 12;
    d.rows = 12;
    d.spacing = 0.1;
    d.origin = { -0.55, 3.0, 0.0 };
    d.along = { 0.0, -1.0, 0.0 };
    Cloth3D& banner = sim.add_cloth(d);
    banner.pin(banner.index(0, 0));
    banner.pin(banner.index(11, 0));
    for (int i = 0; i < 240; ++i) sim.step();
    check(banner.max_stretch() < 0.02, "cloth: a hanging cloth barely stretches (" + std::to_string(banner.max_stretch()) + ")");
    check(near(banner.position(banner.index(0, 0)).y, 3.0, 1e-9), "cloth: pinned corners stay put");
    check(banner.position(banner.index(6, 11)).y < 2.0, "cloth: the free edge hangs down");

    Simulation3D sim2(ground_cfg());
    RigidBody3D& ball = sim2.add_body({ 0.0, 1.0, 0.0 }, PhysicsFlags::DefaultStatic);
    ball.add_collider(Shape3D::make_sphere({ 0.0, 0.0, 0.0 }, 0.5));
    ClothDef3D sheet;
    sheet.columns = 16;
    sheet.rows = 16;
    sheet.spacing = 0.1;
    sheet.origin = { -0.75, 2.0, -0.75 };
    Cloth3D& drape = sim2.add_cloth(sheet);
    for (int i = 0; i < 180; ++i) sim2.step();
    double closest = 1e9;
    for (const auto& p : drape.positions()) closest = std::min(closest, static_cast<double>(vec3::length(p - RVec3{ 0.0f, 1.0f, 0.0f })));
    check(closest > 0.5 && closest < 0.6, "cloth: a sheet drapes over a ball without going inside (" + std::to_string(closest) + ")");
    check(drape.position(drape.index(8, 8)).y > 1.45, "cloth: its middle rests on top of the ball");

    Simulation3D sim3(ground_cfg());
    RigidBody3D& floor = sim3.add_body({ 0.0, 0.0, 0.0 }, PhysicsFlags::DefaultStatic);
    floor.add_collider(Shape3D::make_plane({ 0.0, 1.0, 0.0 }, 0.0));
    RigidBody3D& anchor = sim3.add_body({ 0.0, 4.0, 0.0 }, PhysicsFlags::DefaultStatic);
    anchor.add_collider(Shape3D::make_box(0.1, 0.1, 0.1));
    Cloth3D& rope = sim3.add_rope({ 0.0, 4.0, 0.0 }, { 0.3, -1.0, 0.0 }, 20, 2.0);
    rope.attach(0, anchor);
    for (int i = 0; i < 300; ++i) sim3.step();
    const auto& pts = rope.positions();
    double length = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) length += vec3::length(pts[i] - pts[i - 1]);
    check(near(length, 2.0, 0.04) && near(pts.back().x, 0.0, 0.15) && near(pts.back().y, 2.0, 0.1), "rope: hangs straight down from its anchor at full length (" + std::to_string(length) + ")");
    sim3.remove_body(anchor);
    for (int i = 0; i < 120; ++i) sim3.step();
    bool above = true;
    for (const auto& p : rope.positions()) above = above && p.y >= -0.001;
    check(!rope.attached(0) && above, "rope: falls when its anchor is removed and lands on the floor");
}

void test_cloth_mesh_and_resolution() {
    Simulation3D sim(ground_cfg());
    RigidBody3D& ramp = sim.add_body({ 0.0, 0.0, 0.0 }, PhysicsFlags::DefaultStatic);
    std::vector<RVec3> verts = { { -2.0f, 1.0f, -2.0f }, { 2.0f, 1.0f, -2.0f }, { 2.0f, 1.0f, 2.0f }, { -2.0f, 1.0f, 2.0f } };
    std::vector<std::array<std::uint32_t, 3>> tris = { { 0, 2, 1 }, { 0, 3, 2 } };
    ramp.add_collider(Shape3D::make_mesh(verts, tris, false));
    Cloth3D& rug = sim.add_cloth(ClothDef3D::sheet({ -0.5f, 2.0f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 1.0f, 1.0f, 12, 12));
    for (int i = 0; i < 180; ++i) sim.step();
    Real lowest = 1e9_r;
    for (const auto& p : rug.positions()) lowest = std::min(lowest, p.y);
    check(lowest > 0.99_r && lowest < 1.05_r && rug.contacts() > 0, "cloth: lands on a triangle mesh without falling through (" + std::to_string(lowest) + ")");

    Simulation3D sim2(ground_cfg());
    RigidBody3D& wall = sim2.add_body({ 0.0, 0.0, 0.0 }, PhysicsFlags::DefaultStatic);
    std::vector<RVec3> wv = { { 0.0f, 0.0f, -2.0f }, { 0.0f, 0.0f, 2.0f }, { 0.0f, 4.0f, 2.0f }, { 0.0f, 4.0f, -2.0f } };
    std::vector<std::array<std::uint32_t, 3>> wt = { { 0, 1, 2 }, { 0, 2, 3 } };
    wall.add_collider(Shape3D::make_mesh(wv, wt, true));
    ClothDef3D flag = ClothDef3D::sheet({ -1.2f, 2.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 1.0f, 1.0f, 10, 10);
    flag.gravity_scale = 0.0f;
    Cloth3D& thrown = sim2.add_cloth(flag);
    for (std::size_t i = 0; i < thrown.particle_count(); ++i) thrown.set_velocity(i, { 8.0f, 0.0f, 0.0f });
    std::size_t touches = 0;
    for (int i = 0; i < 60; ++i) {
        sim2.step();
        touches += thrown.contacts();
    }
    Real furthest = -1e9_r;
    for (const auto& p : thrown.positions()) furthest = std::max(furthest, p.x);
    check(furthest < 0.0_r && touches > 0, "cloth: a fast cloth does not pass through a two-sided mesh wall (" + std::to_string(furthest) + ")");

    std::vector<double> sags;
    for (int res : { 6, 12, 24 }) {
        Simulation3D s(ground_cfg());
        ClothDef3D def = ClothDef3D::sheet({ -0.5f, 3.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 1.0f, 1.0f, res, res);
        def.collide = false;
        check(def.triangle_count() == static_cast<std::size_t>(2 * (res - 1) * (res - 1)), "cloth: triangle count follows the resolution");
        Cloth3D& c = s.add_cloth(def);
        check(c.triangle_count() == def.triangle_count() && c.particle_count() == def.particle_count(), "cloth: the built cloth has the asked-for triangles and points");
        check(near(static_cast<double>(c.position(c.index(res - 1, res - 1)).x), 0.5, 1e-5), "cloth: the size stays the same at every resolution");
        for (int col = 0; col < res; ++col) c.pin(c.index(col, 0));
        for (int i = 0; i < 240; ++i) s.step();
        check(near(static_cast<double>(c.particle_mass()) * static_cast<double>(c.particle_count()), 1.0, 1e-4), "cloth: total mass stays the same at every resolution");
        sags.push_back(3.0 - static_cast<double>(c.position(c.index(res / 2, res - 1)).y));
    }
    check(near(sags[0], sags[2], 0.1) && near(sags[1], sags[2], 0.1), "cloth: hangs the same length at low, medium and high resolution (" + std::to_string(sags[0]) + ", " + std::to_string(sags[1]) + ", " + std::to_string(sags[2]) + ")");

    ClothDef3D picked = ClothDef3D::sheet_with_triangles({}, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 2.0f, 1.0f, 800);
    check(picked.triangle_count() >= 700 && picked.triangle_count() <= 900 && near(static_cast<double>(picked.width()), 2.0, 1e-5), "cloth: can be sized by triangle count (" + std::to_string(picked.triangle_count()) + ")");
    picked.set_resolution(41, 21);
    check(picked.triangle_count() == 1600 && near(static_cast<double>(picked.height()), 1.0, 1e-5), "cloth: set_resolution keeps the cloth's size");
}


double section_parts_volume(const std::vector<Shape3D>& parts) {
    double v = 0.0;
    for (const auto& s : parts) v += static_cast<double>(compute_shape_volume(s));
    return v;
}

double section_triangle_volume(const std::vector<std::array<RVec3, 3>>& tris) {
    double v = 0.0;
    for (const auto& t : tris) v += static_cast<double>(vec3::dot(t[0], vec3::cross(t[1], t[2]))) / 6.0;
    return v;
}

void test_sphere_sections() {
    const double pi = 3.14159265358979323846;
    const Shape3D hemi = Shape3D::make_hemisphere(1.0);
    const double hv = static_cast<double>(compute_shape_volume(hemi));
    check(hemi.type == ShapeType3D::Hull && near(hv, 2.0 / 3.0 * pi, 0.02), "sections: a solid hemisphere has 2/3 pi r^3 volume (" + std::to_string(hv) + ")");
    const MassData3D hm = compute_mass(hemi, 2.0);
    check(near(static_cast<double>(hm.mass), 4.0 / 3.0 * pi, 0.04), "sections: hemisphere mass is volume times density (" + std::to_string(hm.mass) + ")");
    check(near(static_cast<double>(hm.center.y), 0.375, 0.01) && near(static_cast<double>(hm.center.x), 0.0, 1e-3) && near(static_cast<double>(hm.center.z), 0.0, 1e-3), "sections: hemisphere centre of mass is 3r/8 above the flat face (" + std::to_string(hm.center.y) + ")");
    const double m = static_cast<double>(hm.mass);
    check(near(static_cast<double>(hm.inertia.entry(1, 1)), 0.4 * m, 0.4 * m * 0.03) && near(static_cast<double>(hm.inertia.entry(0, 0)), 83.0 / 320.0 * m, 83.0 / 320.0 * m * 0.03), "sections: hemisphere inertia matches 2/5 m r^2 and 83/320 m r^2");
    check(near(static_cast<double>(hemi.local_aabb.min.y), 0.0, 1e-4) && hemi.local_aabb.max.y > 0.99f && hemi.bounding_radius >= 0.99f, "sections: hemisphere bounds start at the flat face");

    const Shape3D quarter = Shape3D::make_sphere_section(SphereSection3D::wedge(1.0, 90.0));
    check(SphereSection3D::wedge(1.0, 90.0).is_convex() && quarter.type == ShapeType3D::Hull && near(static_cast<double>(compute_shape_volume(quarter)), pi / 3.0, 0.015), "sections: a 90 degree wedge is a quarter of the sphere (" + std::to_string(compute_shape_volume(quarter)) + ")");

    SphereSection3D full;
    full.radius = 0.7;
    full.polar_min = 0.0;
    full.polar_max = 180.0;
    full.azimuth_min = 30.0;
    full.azimuth_max = 390.0;
    const Shape3D whole = Shape3D::make_sphere_section(full);
    check(full.is_full_sphere() && whole.type == ShapeType3D::Sphere && near(static_cast<double>(compute_shape_volume(whole)), static_cast<double>(compute_shape_volume(Shape3D::make_sphere({}, 0.7))), 1e-6), "sections: full ranges give a plain sphere");

    const Shape3D cap = Shape3D::make_sphere_section(SphereSection3D::cap(1.0, 0.5));
    check(cap.type == ShapeType3D::Hull && near(static_cast<double>(compute_shape_volume(cap)), pi * 0.25 * 2.5 / 3.0, 0.012) && near(static_cast<double>(cap.local_aabb.min.y), 0.5, 1e-3), "sections: a cap of height h has pi h^2 (3r - h) / 3 volume (" + std::to_string(compute_shape_volume(cap)) + ")");

    const SphereSection3D bowl = SphereSection3D::bowl(1.0, 0.1);
    const std::vector<Shape3D> bowl_parts = Shape3D::make_sphere_section_parts(bowl);
    const double bowl_exact = 2.0 / 3.0 * pi * (1.0 - 0.9 * 0.9 * 0.9);
    const double bowl_v = section_parts_volume(bowl_parts);
    check(!bowl.is_convex() && near(static_cast<double>(bowl.exact_volume()), bowl_exact, bowl_exact * 0.002), "sections: bowl exact volume");
    check(bowl_parts.size() > 4 && near(bowl_v, bowl_exact, bowl_exact * 0.04), "sections: bowl parts add up to the shell volume (" + std::to_string(bowl_v) + " vs " + std::to_string(bowl_exact) + ")");
    const double bowl_tri = section_triangle_volume(sphere_section_triangles(bowl));
    check(near(bowl_tri, bowl_v, bowl_v * 0.01), "sections: bowl triangles close up around the same volume as the parts (" + std::to_string(bowl_tri) + ")");
    check(near(section_triangle_volume(sphere_section_triangles(SphereSection3D::hemisphere(1.0))), hv, hv * 0.002), "sections: hemisphere triangles match the collision hull");

    const SphereSection3D d135 = SphereSection3D::dome(1.0, 135.0);
    const double d135_v = section_parts_volume(Shape3D::make_sphere_section_parts(d135));
    check(near(d135_v, static_cast<double>(d135.exact_volume()), static_cast<double>(d135.exact_volume()) * 0.03), "sections: a 135 degree dome splits into convex parts of the right volume (" + std::to_string(d135_v) + ")");
    SphereSection3D three_quarter = SphereSection3D::wedge(1.0, 270.0);
    const std::vector<Shape3D> tq = Shape3D::make_sphere_section_parts(three_quarter);
    check(tq.size() == 2 && near(section_parts_volume(tq), pi, 0.04), "sections: a 270 degree wedge is two convex parts of 3/4 the sphere");

    const double flat = rest_height(Shape3D::make_hemisphere(0.5), 300);
    check(near(flat, 0.0, 0.03), "sections: hemisphere rests on its flat face (y = " + std::to_string(flat) + ")");

    {
        Simulation3D sim(ground_cfg());
        add_ground(sim);
        RigidBody3D& b = sim.add_body({0.0, 1.0, 0.0}, PhysicsFlags::DefaultDynamic);
        b.add_collider(Shape3D::make_hemisphere(0.5));
        b.set_rotation({1.0, 0.0, 0.0}, 3.14159265f + 0.45f);
        b.set_angular_damping(0.3);
        for (int i = 0; i < 900; ++i) sim.step();
        const RVec3 up = b.orientation().rotate(RVec3{0.0, 1.0, 0.0});
        check(up.y < -0.97f && near(b.position().y, 0.5, 0.04), "sections: a hemisphere dropped on its dome rocks back flat side up (up.y = " + std::to_string(up.y) + ", y = " + std::to_string(b.position().y) + ")");
    }

    {
        Simulation3D sim(ground_cfg());
        RigidBody3D& h = sim.add_body({0.0, 0.0, 0.0}, PhysicsFlags::DefaultStatic);
        h.add_collider(Shape3D::make_hemisphere(1.0));
        auto hit = sim.raycast({0.3, 5.0, 0.2}, {0.0, -1.0, 0.0}, 10.0);
        const double want = std::sqrt(1.0 - 0.13);
        check(hit && hit->body == &h && near(static_cast<double>(hit->point.y), want, 0.015) && hit->normal.y > 0.85f, "sections: raycast down hits the dome at the right height (" + std::to_string(hit ? hit->point.y : -1.0f) + ")");
        auto side = sim.raycast({0.3, -5.0, 0.2}, {0.0, 1.0, 0.0}, 10.0);
        check(side && near(static_cast<double>(side->point.y), 0.0, 1e-3) && side->normal.y < -0.99f, "sections: raycast up hits the flat face");
    }

    for (int mode = 0; mode < 2; ++mode) {
        Simulation3D sim(ground_cfg());
        add_ground(sim);
        RigidBody3D& b = sim.add_body({0.0, 1.0, 0.0}, PhysicsFlags::DefaultStatic);
        if (mode == 0) { for (const auto& p : bowl_parts) b.add_collider(p); }
        else b.add_collider(Shape3D::make_sphere_section(bowl));
        RigidBody3D& ball = sim.add_body({0.35, 1.8, 0.1}, PhysicsFlags::DefaultDynamic);
        ball.add_collider(Shape3D::make_sphere({}, 0.15));
        for (int i = 0; i < 600; ++i) sim.step();
        const RVec3 rel = ball.position() - b.position();
        const double dist = static_cast<double>(vec3::length(rel));
        check(dist < 0.9 - 0.15 + 0.03 && rel.y < 0.0f && ball.position().y > 0.1f, std::string("sections: a bowl ") + (mode == 0 ? "made of parts" : "mesh") + " holds a ball (dist = " + std::to_string(dist) + ", y = " + std::to_string(rel.y) + ")");
    }

    {
        Simulation3D sim(ground_cfg());
        add_ground(sim);
        RigidBody3D& b = sim.add_body({0.0, 1.02, 0.0}, PhysicsFlags::DefaultDynamic);
        for (const auto& p : Shape3D::make_sphere_section_parts(SphereSection3D::bowl(1.0, 0.1), 16)) b.add_collider(p);
        RigidBody3D& ball = sim.add_body({0.2, 1.6, 0.0}, PhysicsFlags::DefaultDynamic);
        ball.add_collider(Shape3D::make_sphere({}, 0.15));
        for (int i = 0; i < 600; ++i) sim.step();
        const RVec3 rel = b.orientation().rotate_inverse(ball.position() - b.position());
        check(vec3::length(rel) < 0.9f && rel.y < 0.0f && near(b.position().y, 1.0, 0.06), "sections: a dynamic bowl on the ground keeps a ball inside (y = " + std::to_string(b.position().y) + ")");
    }
}

} // namespace

void test_contact_regressions() {
    for (int big = 0; big < 2; ++big) {
        for (int reuse = 0; reuse < 2; ++reuse) {
            Simulation3D::Config cfg = ground_cfg();
            cfg.reuse_contacts = reuse != 0;
            Simulation3D sim(cfg);
            RigidBody3D& g = sim.add_body({0.0, -0.5, 0.0}, PhysicsFlags::DefaultStatic);
            const Real half = big ? 200.0 : 20.0;
            g.add_collider(Shape3D::make_box(half, 0.5, half));
            RigidBody3D& b = sim.add_body({0.0, 2.0, 0.0}, PhysicsFlags::DefaultDynamic);
            b.add_collider(Shape3D::make_rounded_box(0.3, 0.3, 0.3, 0.1));
            double low = 10.0;
            for (int i = 0; i < 300; ++i) {
                sim.step();
                if (i > 60) low = std::min(low, static_cast<double>(b.position().y));
            }
            check(near(low, 0.3, 0.01) && near(b.position().y, 0.3, 0.01), std::string("contacts: rounded box stays on a ") + (big ? "400 m" : "40 m") + " floor" + (reuse ? "" : " without contact reuse"));
        }
    }
    {
        Shape3D ground = Shape3D::make_box(200.0, 0.5, 200.0);
        Shape3D cube = Shape3D::make_rounded_box(0.3, 0.3, 0.3, 0.1);
        std::vector<ContactManifold3D> out;
        const Transform3D xg({0.0, -0.5, 0.0}, RQuat{1.0, 0.0, 0.0, 0.0});
        const Transform3D xc({0.013, 0.3, -0.021}, RQuat{1.0, 0.0, 0.0, 0.0});
        const bool hit = collide_shapes_multi(ground, xg, cube, xc, 0.02, out);
        bool flat = hit && !out.empty() && out[0].point_count > 0;
        for (int i = 0; flat && i < out[0].point_count; ++i) flat = std::abs(out[0].points[i].separation) < 0.001;
        check(flat, "contacts: face contact against a big box measures the gap from the face");
    }
    {
        Simulation3D::Config cfg = ground_cfg();
        Simulation3D sim(cfg);
        RigidBody3D& g = sim.add_body({0.0, -0.5, 0.0}, PhysicsFlags::DefaultStatic);
        g.add_collider(Shape3D::make_box(200.0, 0.5, 200.0));
        RigidBody3D& ball = sim.add_body({0.0, 0.6, 0.0}, PhysicsFlags::DefaultDynamic);
        ball.add_collider(Shape3D::make_sphere({0.0, 0.0, 0.0}, 0.5));
        ball.set_linear_velocity({0.02, 0.0, 0.01});
        ball.set_angular_velocity({0.02, 0.0, -0.04});
        double fastest_late = 0.0;
        for (int i = 0; i < 60 * 120; ++i) {
            sim.step();
            if (i > 60 * 90) {
                const RVec3 v = ball.linear_velocity();
                fastest_late = std::max(fastest_late, static_cast<double>(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z)));
            }
        }
        check(fastest_late < 0.0235, "contacts: reused contacts don't speed up a slow rolling ball (" + std::to_string(fastest_late) + " m/s after 90 s, started at 0.022)");
    }
}

int main() {
    test_contact_regressions();
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
    test_continuous_collision();
    test_substeps();
    test_new_shapes();
    test_queries();
    test_layers();
    test_joints();
    test_solvers();
    test_determinism_and_misc();
    test_2d_roadmap();
    test_storage_and_handles();
    test_rebasing_and_snapshots();
    test_cloth();
    test_cloth_mesh_and_resolution();
    test_sphere_sections();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
