#include "../Includes/physics.hpp"

#include <chrono>
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

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.4g", v);
    return buf;
}

const Real kDt = 1.0_r / 60.0_r;

RigidBody3D& add_box(Simulation3D& sim, const RVec3& pos, Real half, Real density) {
    RigidBody3D& b = sim.add_body(pos, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_box(half, half, half), Material3D(density));
    return b;
}

RigidBody3D& add_ball(Simulation3D& sim, const RVec3& pos, Real radius, Real density) {
    RigidBody3D& b = sim.add_body(pos, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape3D::make_sphere(RVec3{}, radius), Material3D(density));
    return b;
}

RigidBody3D& add_static_box(Simulation3D& sim, const RVec3& pos, const RVec3& half, Real friction = 0.3_r, Real restitution = 0.0_r) {
    RigidBody3D& b = sim.add_body(pos, PhysicsFlags::DefaultStatic);
    Material3D m;
    m.friction = friction;
    m.restitution = restitution;
    m.restitution_threshold = 0.1_r;
    b.add_collider(Shape3D::make_box(half.x, half.y, half.z), m);
    b.synchronize_colliders();
    return b;
}

void run(FluidWorld3D& world, Simulation3D& sim, int frames) {
    for (int i = 0; i < frames; ++i) {
        world.step(sim, kDt);
        sim.step(kDt);
    }
}

Real speed(const RigidBody3D& b) { return vec3::length(b.linear_velocity()); }

bool finite(const RVec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

void test_materials_and_quality() {
    const FluidMaterial w = FluidMaterial::water();
    const FluidMaterial h = FluidMaterial::honey();
    const FluidMaterial l = FluidMaterial::lava();
    check(w.viscosity_factor() == 1.0_r, "material: water viscosity factor is 1");
    check(h.viscosity_factor() > 4.0_r && l.viscosity_factor() > h.viscosity_factor(), "material: honey and lava are more viscous");
    check(FluidMaterial::mercury().density > 13000.0_r && FluidMaterial::sea_water().density > w.density, "material: densities");
    check(FluidMaterial::make(FluidPreset::oil).preset == FluidPreset::oil && std::string(to_string(FluidPreset::mud)) == "mud", "material: presets");
    const FluidQualitySettings lo = FluidQualitySettings::preset(FluidQuality::low);
    const FluidQualitySettings ul = FluidQualitySettings::preset(FluidQuality::ultra);
    check(lo.max_particles < ul.max_particles && lo.solver_iterations < ul.solver_iterations && lo.resolution_scale > ul.resolution_scale, "quality: presets scale work");
}

void test_blocky() {
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::blocky, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 24.0_r, 6.0_r, 24.0_r }));
    d.cell_size = 1.0_r;
    FluidVolume3D& v = world.add_volume(d);
    v.set_source(12, 0, 12);
    v.set_source(3, 5, 3);
    for (int i = 0; i < 400; ++i) world.step(kDt);
    check(v.block_level(13, 0, 12) == 7 && v.block_level(12, 0, 5) == 1 && v.block_level(19, 0, 12) == 1, "blocky: levels drop by one per block (" + std::to_string(v.block_level(13, 0, 12)) + ", " + std::to_string(v.block_level(19, 0, 12)) + ")");
    check(v.block_level(20, 0, 12) == 0 && v.block_level(12, 0, 4) == 0, "blocky: spread stops after 7 cells");
    check(v.block_level(3, 4, 3) == 8 && v.block_level(4, 5, 3) == 0, "blocky: water falls before spreading");
    check(v.block_level(3, 0, 3) >= 7 && v.block_level(4, 0, 3) == 7, "blocky: falling water spreads when it lands");
    for (int i = 0; i < 120; ++i) world.step(kDt);
    check(v.active_regions() == 0, "blocky: settled water sleeps (" + std::to_string(v.active_regions()) + " active chunks)");
    FluidVolumeDef hd = d;
    hd.bounds = AABB3D(RVec3{ 0.0_r, 0.0_r, 30.0_r }, RVec3{ 24.0_r, 6.0_r, 54.0_r });
    FluidVolume3D& hole = world.add_volume(hd);
    for (int k = 0; k < 24; ++k) {
        for (int i = 0; i < 24; ++i) hole.set_solid(i, 0, k, true);
    }
    hole.set_solid(15, 0, 12, false);
    hole.set_source(12, 1, 12);
    for (int i = 0; i < 100; ++i) world.step(kDt);
    check(hole.block_level(14, 1, 12) > 0 && hole.block_level(10, 1, 12) == 0, "blocky: flow heads for the nearest hole");
    check(hole.block_level(15, 0, 12) == 8, "blocky: water pours down the hole");
    FluidVolumeDef inf = d;
    inf.bounds = AABB3D(RVec3{ 30.0_r, 0.0_r, 0.0_r }, RVec3{ 40.0_r, 3.0_r, 10.0_r });
    FluidVolume3D& pool = world.add_volume(inf);
    pool.set_source(4, 0, 4);
    pool.set_source(6, 0, 4);
    for (int i = 0; i < 60; ++i) world.step(kDt);
    check(pool.source(5, 0, 4), "blocky: two sources make an infinite source between them");
    FluidVolumeDef lav = d;
    lav.material = FluidMaterial::lava();
    lav.bounds = AABB3D(RVec3{ 0.0_r, 0.0_r, 60.0_r }, RVec3{ 24.0_r, 6.0_r, 84.0_r });
    FluidVolume3D& lava = world.add_volume(lav);
    lava.set_source(12, 0, 12);
    for (int i = 0; i < 400; ++i) world.step(kDt);
    int reach = 0;
    for (int i = 13; i < 24; ++i) {
        if (lava.block_level(i, 0, 12) > 0) reach = i - 12;
    }
    check(reach > 0 && reach < 7, "blocky: viscous lava spreads fewer blocks (" + std::to_string(reach) + ")");
}

void u_tube_surfaces(const FluidVolume3D& v, Real& left, Real& right) {
    left = v.surface_height(v.cell_center(1, 0, 0));
    right = v.surface_height(v.cell_center(v.cells_x() - 2, 0, 0));
}

void test_flowing() {
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::flowing, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 3.0_r, 3.0_r, 0.25_r }));
    d.cell_size = 0.25_r;
    FluidVolume3D& v = world.add_volume(d);
    for (int j = 2; j < v.cells_y(); ++j) {
        v.set_solid(5, j, 0, true);
        v.set_solid(6, j, 0, true);
    }
    for (int j = 0; j < 10; ++j) {
        for (int i = 0; i < 5; ++i) v.set_amount(i, j, 0, 1.0_r);
    }
    const Real v0 = v.total_volume();
    for (int i = 0; i < 60 * 40; ++i) world.step(kDt);
    const Real v1 = v.total_volume();
    Real left = 0.0_r, right = 0.0_r;
    u_tube_surfaces(v, left, right);
    check(std::abs(v1 - v0) < 1e-3_r, "flowing: volume is conserved (" + num(v0) + " -> " + num(v1) + ")");
    check(std::abs(left - right) < 0.3_r && right > 0.8_r, "flowing: U-tube levels equalise (" + num(left) + " vs " + num(right) + ")");

    FluidVolumeDef hd = FluidVolumeDef::box(FluidLevel::flowing, AABB3D(RVec3{ 10.0_r, 0.0_r, 0.0_r }, RVec3{ 13.0_r, 2.0_r, 0.25_r }));
    FluidVolume3D& drain = world.add_volume(hd);
    for (int i = 0; i < drain.cells_x(); ++i) drain.set_solid(i, 3, 0, true);
    drain.set_solid(6, 3, 0, false);
    for (int i = 0; i < drain.cells_x(); ++i) drain.set_amount(i, 4, 0, 1.0_r);
    for (int i = 0; i < 600; ++i) world.step(kDt);
    Real below = 0.0_r, above = 0.0_r;
    for (int j = 0; j < drain.cells_y(); ++j) {
        for (int i = 0; i < drain.cells_x(); ++i) (j < 3 ? below : above) += drain.amount(i, j, 0);
    }
    check(below > 10.0_r && above < 0.5_r, "flowing: water drains down a hole (" + num(below) + " below, " + num(above) + " above)");
}

Real spread_cells(const FluidVolume3D& v) {
    int n = 0;
    for (int k = 0; k < v.cells_z(); ++k) {
        for (int i = 0; i < v.cells_x(); ++i) {
            if (v.amount(i, 0, k) > 0.01_r) ++n;
        }
    }
    return static_cast<Real>(n);
}

void test_flowing_viscosity() {
    FluidWorld3D world;
    auto make = [&](const FluidMaterial& m, Real x) -> FluidVolume3D& {
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::flowing, AABB3D(RVec3{ x, 0.0_r, 0.0_r }, RVec3{ x + 6.0_r, 1.0_r, 6.0_r }), m);
        FluidVolume3D& v = world.add_volume(d);
        for (int j = 0; j < 4; ++j) {
            for (int k = 11; k < 13; ++k) {
                for (int i = 11; i < 13; ++i) v.set_amount(i, j, k, 1.0_r);
            }
        }
        return v;
    };
    FluidVolume3D& water = make(FluidMaterial::water(), 0.0_r);
    FluidVolume3D& honey = make(FluidMaterial::honey(), 10.0_r);
    for (int i = 0; i < 60; ++i) world.step(kDt);
    const Real ws = spread_cells(water), hs = spread_cells(honey);
    check(hs < ws, "viscosity: honey spreads slower than water in flowing (" + num(hs) + " vs " + num(ws) + " cells)");
}

void test_still_buoyancy() {
    Simulation3D sim;
    FluidWorld3D world;
    world.add_volume(AABB3D(RVec3{ -20.0_r, -20.0_r, -20.0_r }, RVec3{ 20.0_r, 0.0_r, 20.0_r }), FluidLevel::still, FluidMaterial::water(), 1.0_r);
    RigidBody3D& wood = add_box(sim, RVec3{ 0.0_r, 0.5_r, 0.0_r }, 0.5_r, 600.0_r);
    RigidBody3D& steel = add_box(sim, RVec3{ 5.0_r, 0.0_r, 0.0_r }, 0.5_r, 7850.0_r);
    RigidBody3D& ball = add_ball(sim, RVec3{ -5.0_r, 0.5_r, 0.0_r }, 0.5_r, 500.0_r);
    run(world, sim, 600);
    const FluidForces3D* f = world.body_forces(wood);
    const Real frac = f ? f->submerged_fraction : 0.0_r;
    check(f && std::abs(frac - 0.6_r) < 0.05_r, "still: wood (600 kg/m3) floats about 60% submerged (" + num(frac) + ")");
    check(std::abs(wood.position().y + 0.1_r) < 0.06_r && speed(wood) < 0.05_r, "still: wood comes to rest at the waterline (" + num(wood.position().y) + ")");
    check(steel.position().y < -5.0_r, "still: steel sinks (" + num(steel.position().y) + ")");
    const FluidForces3D* fb = world.body_forces(ball);
    check(fb && std::abs(fb->submerged_fraction - 0.5_r) < 0.05_r, "still: a 500 kg/m3 ball floats half submerged (" + num(fb ? fb->submerged_fraction : 0.0_r) + ")");
    check(world.depth_at(RVec3{ 1.0_r, -2.0_r, 1.0_r }) > 1.99_r && !world.inside(RVec3{ 1.0_r, 0.5_r, 1.0_r }), "still: depth and inside queries");

    Simulation3D sim2;
    FluidWorld3D world2;
    world2.add_volume(AABB3D(RVec3{ -20.0_r, -20.0_r, -20.0_r }, RVec3{ 20.0_r, 0.0_r, 20.0_r }), FluidLevel::still, FluidMaterial::water(), 1.0_r);
    RigidBody3D& tilted = add_box(sim2, RVec3{ 0.0_r, 0.0_r, 0.0_r }, 0.5_r, 300.0_r);
    tilted.set_rotation(RVec3{ 0.0_r, 0.0_r, 1.0_r }, 0.6_r);
    RigidBody3D& plank = sim2.add_body(RVec3{ 3.0_r, 0.0_r, 0.0_r }, PhysicsFlags::DefaultDynamic);
    plank.add_collider(Shape3D::make_box(1.0_r, 0.1_r, 0.3_r), Material3D(500.0_r));
    plank.set_rotation(RVec3{ 0.0_r, 0.0_r, 1.0_r }, 1.2_r);
    run(world2, sim2, 900);
    const RVec3 up = plank.get_world_vector(RVec3{ 0.0_r, 1.0_r, 0.0_r });
    check(std::abs(up.y) > 0.95_r, "still: a plank rights itself to lie flat (" + num(up.y) + ")");
    check(speed(tilted) < 0.1_r && vec3::length(tilted.angular_velocity()) < 0.2_r, "still: angular drag settles a tilted box");
}

void test_still_z_up() {
    Simulation3DConfig cfg;
    cfg.gravity = RVec3{ 0.0_r, 0.0_r, -9.81_r };
    Simulation3D sim(cfg);
    FluidWorld3DConfig fc;
    fc.gravity = cfg.gravity;
    FluidWorld3D world(fc);
    FluidVolume3D& v = world.add_volume(AABB3D(RVec3{ -10.0_r, -10.0_r, -10.0_r }, RVec3{ 10.0_r, 10.0_r, 0.0_r }), FluidLevel::still, FluidMaterial::water(), 1.0_r);
    RigidBody3D& wood = add_box(sim, RVec3{ 0.0_r, 0.0_r, 0.5_r }, 0.5_r, 600.0_r);
    run(world, sim, 600);
    const FluidForces3D* f = world.body_forces(wood);
    check(vec3::length(v.up() - RVec3{ 0.0_r, 0.0_r, 1.0_r }) < 1e-5_r, "z-up: volume up axis follows gravity");
    check(f && std::abs(f->submerged_fraction - 0.6_r) < 0.05_r && std::abs(wood.position().z + 0.1_r) < 0.06_r, "z-up: wood floats 60% submerged with z up (" + num(wood.position().z) + ")");
    check(std::abs(v.surface_height(3.0_r, 2.0_r)) < 1e-5_r && v.depth_at(RVec3{ 0.0_r, 0.0_r, -3.0_r }) > 2.99_r, "z-up: surface and depth queries use z");

    Simulation3D sim_y;
    FluidWorld3D world_y;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 8.0_r, 2.0_r, 8.0_r }), FluidMaterial::water(), 0.5_r);
    world_y.add_volume(d);
    FluidWorld3DConfig zc;
    zc.gravity = cfg.gravity;
    FluidWorld3D world_z(zc);
    FluidVolumeDef dz = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 8.0_r, 8.0_r, 2.0_r }), FluidMaterial::water(), 0.5_r);
    FluidVolume3D& wz = world_z.add_volume(dz);
    wz.disturb(RVec3{ 4.0_r, 4.0_r, 1.0_r }, 0.2_r, 1.0_r);
    for (int i = 0; i < 60; ++i) world_z.step(kDt);
    check(std::isfinite(wz.surface_height(4.0_r, 4.0_r)) && std::abs(wz.total_volume() - 64.0_r) < 1e-2_r, "z-up: waves level runs with z up");
}

void test_waves() {
    Simulation3D sim;
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 16.0_r, 2.0_r, 16.0_r }), FluidMaterial::water(), 0.5_r);
    d.cell_size = 0.25_r;
    FluidVolume3D& v = world.add_volume(d);
    for (int i = 0; i < 30; ++i) world.step(sim, kDt);
    const Real v0 = v.total_volume();
    check(v.active_regions() == 0, "waves: calm water sleeps (" + std::to_string(v.active_regions()) + " tiles)");
    RigidBody3D& ball = add_ball(sim, RVec3{ 8.0_r, 1.8_r, 8.0_r }, 0.4_r, 500.0_r);
    Real ring_peak = 0.0_r;
    Real late = 0.0_r;
    for (int f = 0; f < 60 * 12; ++f) {
        world.step(sim, kDt);
        sim.step(kDt);
        const std::vector<Real>& h = v.heights();
        double mean = 0.0;
        int count = 0;
        for (int k = 0; k < v.columns_z(); ++k) {
            for (int i = 0; i < v.columns_x(); ++i) {
                const Real du = (static_cast<Real>(i) + 0.5_r) * v.column_size() - 8.0_r, dw = (static_cast<Real>(k) + 0.5_r) * v.column_size() - 8.0_r;
                if (du * du + dw * dw < 2.25_r) continue;
                mean += static_cast<double>(h[v.column_index(i, k)]);
                ++count;
            }
        }
        const Real m = static_cast<Real>(mean / count);
        Real ring = 0.0_r, far = 0.0_r;
        for (int k = 0; k < v.columns_z(); ++k) {
            for (int i = 0; i < v.columns_x(); ++i) {
                const Real du = (static_cast<Real>(i) + 0.5_r) * v.column_size() - 8.0_r, dw = (static_cast<Real>(k) + 0.5_r) * v.column_size() - 8.0_r;
                const Real r2 = du * du + dw * dw;
                if (r2 < 2.25_r) continue;
                const Real dev = std::abs(h[v.column_index(i, k)] - m);
                if (r2 > 9.0_r && r2 < 16.0_r) ring = std::max(ring, dev);
                far = std::max(far, dev);
            }
        }
        if (f < 120) ring_peak = std::max(ring_peak, ring);
        if (f >= 60 * 12 - 30) late = std::max(late, far);
    }
    const Real v1 = v.total_volume();
    check(ring_peak > 0.01_r, "waves: a dropped ball makes a ripple that reaches 3-4 m out (" + num(ring_peak) + " m)");
    check(late < 0.25_r * ring_peak, "waves: the ripple damps out (" + num(late) + " m late vs " + num(ring_peak) + " m)");
    check(std::abs(v1 - v0) < 1e-4_r * v0, "waves: total volume is conserved (" + num(v0) + " -> " + num(v1) + ")");
    const FluidForces3D* f = world.body_forces(ball);
    check(f && std::abs(f->submerged_fraction - 0.5_r) < 0.08_r, "waves: the ball floats half submerged (" + num(f ? f->submerged_fraction : 0.0_r) + ")");
    const std::vector<RVec3>& n = v.normals();
    check(n.size() == v.heights().size() && std::abs(n[v.column_index(2, 2)].y - 1.0_r) < 0.05_r, "waves: render normals");

    FluidVolumeDef sd = d;
    sd.bounds = AABB3D(RVec3{ 40.0_r, 0.0_r, 0.0_r }, RVec3{ 56.0_r, 2.0_r, 16.0_r });
    sd.waves.swell_amplitude = 0.2_r;
    FluidVolume3D& sea = world.add_volume(sd);
    for (int i = 0; i < 30; ++i) world.step(sim, kDt);
    Real lo = 1e9_r, hi = -1e9_r;
    for (Real h : sea.heights()) {
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    check(hi - lo > 0.2_r && hi - lo < 1.0_r, "waves: wind swell adds rendered waves (" + num(hi - lo) + " m)");
    check(vec3::length(sea.velocity_at(RVec3{ 48.0_r, 0.9_r, 8.0_r })) > 0.05_r, "waves: swell has orbital velocity");
}

void test_waves_threads_and_walls() {
    auto make = [](int threads, std::vector<Real>& out) {
        FluidWorld3DConfig cfg;
        cfg.threads = threads;
        FluidWorld3D world(cfg);
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 12.0_r, 2.0_r, 12.0_r }), FluidMaterial::water(), 0.4_r);
        FluidVolume3D& v = world.add_volume(d);
        for (int k = 0; k < v.columns_z(); ++k) v.set_wall(24, k, k < 40);
        v.disturb(RVec3{ 3.0_r, 1.0_r, 3.0_r }, 0.3_r, 1.0_r);
        for (int i = 0; i < 120; ++i) world.step(kDt);
        out = v.depths();
        return v.total_volume();
    };
    std::vector<Real> a, b;
    const Real va = make(1, a);
    const Real vb = make(2, b);
    check(a == b && va == vb, "waves: multi-threaded stepping is deterministic");
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 8.0_r, 2.0_r, 4.0_r }), FluidMaterial::water(), 0.0_r);
    FluidVolume3D& v = world.add_volume(d);
    for (int k = 0; k < v.columns_z(); ++k) {
        for (int i = 0; i < v.columns_x(); ++i) v.set_depth(i, k, i < 8 ? 1.5_r : 0.0_r);
        v.set_wall(12, k, true);
    }
    const Real v0 = v.total_volume();
    for (int i = 0; i < 300; ++i) world.step(kDt);
    Real past = 0.0_r;
    for (int k = 0; k < v.columns_z(); ++k) {
        for (int i = 13; i < v.columns_x(); ++i) past += v.depth(i, k);
    }
    check(past == 0.0_r && std::abs(v.total_volume() - v0) < 1e-4_r * v0, "waves: water does not spill over walls");
    check(v.depth(10, 2) > 0.3_r, "waves: a dam break floods up to the wall (" + num(v.depth(10, 2)) + ")");
}

Real max_extent(const FluidVolume3D& v) {
    Real m = 0.0_r;
    for (const RVec3& p : v.positions()) m = std::max(m, p.x);
    return m;
}

bool inside_tank(const FluidVolume3D& v) {
    const AABB3D b = v.bounds().fatten(1e-3_r);
    for (const RVec3& p : v.positions()) {
        if (!finite(p) || !b.contains(p)) return false;
    }
    return true;
}

void test_realistic() {
    Simulation3D sim;
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 2.0_r, 1.2_r, 0.4_r }));
    d.particles.radius = 0.025_r;
    FluidVolume3D& v = world.add_volume(d);
    const std::size_t n = v.add_particle_box(AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 0.4_r, 0.7_r, 0.4_r }));
    const Real start = max_extent(v);
    run(world, sim, 90);
    const Real spread = max_extent(v);
    check(n > 500 && spread > start + 0.8_r, "realistic: a dam-break column spreads (" + std::to_string(n) + " particles, " + num(start) + " -> " + num(spread) + " m)");
    check(inside_tank(v), "realistic: particles stay inside the tank");
    Real top = 0.0_r;
    for (const RVec3& p : v.positions()) top = std::max(top, p.y);
    check(top < 0.7_r, "realistic: the column collapses (" + num(top) + ")");
    run(world, sim, 240);
    double dsum = 0.0;
    for (Real r : v.densities()) dsum += static_cast<double>(r);
    const double avg = dsum / static_cast<double>(v.particle_count());
    check(avg > 500.0 && avg < 1300.0, "realistic: density stays near rest (" + num(avg) + ")");
    check(v.surface_grid().size() > 0 && v.surface_height(RVec3{ 1.0_r, 0.0_r, 0.2_r }) > 0.0_r, "realistic: coarse surface grid for rendering");
    check(v.awake_particles() < v.particle_count(), "realistic: calm particles fall asleep (" + std::to_string(v.awake_particles()) + "/" + std::to_string(v.particle_count()) + " awake)");
}

void test_realistic_over_wall() {
    Simulation3D sim;
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 2.0_r, 1.5_r, 0.3_r }));
    d.particles.radius = 0.025_r;
    FluidVolume3D& v = world.add_volume(d);
    add_static_box(sim, RVec3{ 1.0_r, 0.15_r, 0.15_r }, RVec3{ 0.05_r, 0.15_r, 0.2_r });
    v.add_particle_box(AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 0.5_r, 0.8_r, 0.3_r }));
    FluidEmitter3D e;
    e.position = RVec3{ 0.3_r, 1.3_r, 0.15_r };
    e.velocity = RVec3{ 0.5_r, -1.5_r, 0.0_r };
    e.radius = 0.05_r;
    e.rate = 150.0_r;
    v.add_emitter(e);
    run(world, sim, 150);
    const std::size_t second = v.particles_in(AABB3D(RVec3{ 1.05_r, 0.0_r, -1.0_r }, RVec3{ 2.1_r, 2.0_r, 1.0_r }));
    bool through = false;
    for (const RVec3& p : v.positions()) {
        if (p.x > 0.95_r && p.x < 1.05_r && p.y < 0.28_r) through = true;
    }
    check(second > 30, "realistic: water pours over a low wall into the second tank (" + std::to_string(second) + " particles)");
    check(!through, "realistic: no particles pass through the wall");
    check(inside_tank(v), "realistic: poured particles stay in the volume");
}

void test_realistic_viscosity() {
    Simulation3D sim;
    FluidWorld3D world;
    auto make = [&](const FluidMaterial& m, Real z) -> FluidVolume3D& {
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ 0.0_r, 0.0_r, z }, RVec3{ 2.0_r, 1.0_r, z + 0.3_r }), m);
        d.particles.radius = 0.025_r;
        FluidVolume3D& v = world.add_volume(d);
        v.add_particle_box(AABB3D(RVec3{ 0.0_r, 0.0_r, z }, RVec3{ 0.3_r, 0.6_r, z + 0.3_r }));
        return v;
    };
    FluidVolume3D& water = make(FluidMaterial::water(), 0.0_r);
    FluidVolume3D& honey = make(FluidMaterial::honey(), 1.0_r);
    run(world, sim, 50);
    const Real ws = max_extent(water), hs = max_extent(honey);
    check(hs < ws - 0.1_r, "viscosity: honey spreads slower than water in realistic (" + num(hs) + " vs " + num(ws) + " m)");
}

void test_realistic_bounce_and_friction() {
    Simulation3D sim;
    FluidWorld3D world;
    auto make = [&](Real x) -> FluidVolume3D& {
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ x, 0.0_r, 0.0_r }, RVec3{ x + 3.0_r, 3.0_r, 1.0_r }));
        d.particles.radius = 0.02_r;
        d.allow_sleep = false;
        return world.add_volume(d);
    };
    FluidVolume3D& bouncy = make(0.0_r);
    FluidVolume3D& dead = make(10.0_r);
    add_static_box(sim, RVec3{ 1.5_r, 0.25_r, 0.5_r }, RVec3{ 1.5_r, 0.25_r, 0.5_r }, 0.3_r, 0.9_r);
    add_static_box(sim, RVec3{ 11.5_r, 0.25_r, 0.5_r }, RVec3{ 1.5_r, 0.25_r, 0.5_r }, 0.3_r, 0.0_r);
    bouncy.add_particle(RVec3{ 1.5_r, 1.5_r, 0.5_r });
    dead.add_particle(RVec3{ 11.5_r, 1.5_r, 0.5_r });
    Real hb = 0.0_r, hd = 0.0_r;
    bool hit_b = false, hit_d = false;
    for (int f = 0; f < 90; ++f) {
        world.step(sim, kDt);
        sim.step(kDt);
        const Real yb = bouncy.positions()[0].y, yd = dead.positions()[0].y;
        if (yb < 0.6_r) hit_b = true;
        if (yd < 0.6_r) hit_d = true;
        if (hit_b) hb = std::max(hb, yb);
        if (hit_d) hd = std::max(hd, yd);
    }
    check(hit_b && hit_d && hb > 0.9_r && hd < 0.6_r, "restitution: a particle rebounds off a bouncy body more than a dead one (" + num(hb) + " vs " + num(hd) + ")");

    Simulation3D sim2;
    FluidWorld3D world2;
    auto make2 = [&](Real x) -> FluidVolume3D& {
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ x, 0.0_r, 0.0_r }, RVec3{ x + 6.0_r, 2.0_r, 1.0_r }));
        d.particles.radius = 0.02_r;
        d.allow_sleep = false;
        return world2.add_volume(d);
    };
    FluidVolume3D& rough = make2(0.0_r);
    FluidVolume3D& slick = make2(10.0_r);
    add_static_box(sim2, RVec3{ 3.0_r, 0.25_r, 0.5_r }, RVec3{ 3.0_r, 0.25_r, 0.5_r }, 1.0_r, 0.0_r);
    add_static_box(sim2, RVec3{ 13.0_r, 0.25_r, 0.5_r }, RVec3{ 3.0_r, 0.25_r, 0.5_r }, 0.0_r, 0.0_r);
    rough.add_particle(RVec3{ 0.5_r, 0.52_r, 0.5_r }, RVec3{ 2.0_r, 0.0_r, 0.0_r });
    slick.add_particle(RVec3{ 10.5_r, 0.52_r, 0.5_r }, RVec3{ 2.0_r, 0.0_r, 0.0_r });
    run(world2, sim2, 60);
    const Real dr = rough.positions()[0].x - 0.5_r, ds = slick.positions()[0].x - 10.5_r;
    check(dr < 0.8_r * ds && ds > 1.5_r, "friction: particles slide less on a high-friction body (" + num(dr) + " vs " + num(ds) + " m)");

    Simulation3D sim3;
    FluidWorld3D world3;
    FluidVolumeDef d3 = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ -0.6_r, 0.0_r, -0.6_r }, RVec3{ 0.6_r, 1.5_r, 0.6_r }));
    d3.particles.radius = 0.03_r;
    FluidVolume3D& tank = world3.add_volume(d3);
    add_static_box(sim3, RVec3{ 0.0_r, -0.5_r, 0.0_r }, RVec3{ 2.0_r, 0.5_r, 2.0_r });
    tank.add_particle_box(AABB3D(RVec3{ -0.6_r, 0.0_r, -0.6_r }, RVec3{ 0.6_r, 0.5_r, 0.6_r }));
    run(world3, sim3, 60);
    RigidBody3D& cube = add_box(sim3, RVec3{ 0.0_r, 0.7_r, 0.0_r }, 0.15_r, 400.0_r);
    run(world3, sim3, 180);
    check(tank.particle_contacts() > 0, "two-way: particles touch the body");
    check(cube.position().y > 0.3_r, "two-way: particle pressure holds a light body up (" + num(cube.position().y) + ")");
}

void test_drag() {
    auto fall = [](const FluidMaterial& m, Real seconds, Real& v_mid) {
        Simulation3D sim;
        FluidWorld3D world;
        world.add_volume(AABB3D(RVec3{ -5.0_r, -800.0_r, -5.0_r }, RVec3{ 5.0_r, 0.0_r, 5.0_r }), FluidLevel::still, m, 1.0_r);
        RigidBody3D& b = add_ball(sim, RVec3{ 0.0_r, -1.0_r, 0.0_r }, 0.25_r, 3000.0_r);
        const int frames = static_cast<int>(seconds * 60.0_r);
        for (int f = 0; f < frames; ++f) {
            world.step(sim, kDt);
            sim.step(kDt);
            if (f == frames * 2 / 3) v_mid = speed(b);
        }
        return speed(b);
    };
    Real wm = 0.0_r, am = 0.0_r;
    const Real wv = fall(FluidMaterial::water(), 6.0_r, wm);
    const Real av = fall(FluidMaterial::custom(1.2_r, 1.8e-5_r), 6.0_r, am);
    check(std::abs(wv - wm) < 0.05_r * wv && wv > 2.0_r && wv < 8.0_r, "drag: a ball reaches terminal speed in water (" + num(wm) + " -> " + num(wv) + " m/s)");
    check(av > 5.0_r * wv, "drag: terminal speed in water is far lower than in air (" + num(wv) + " vs " + num(av) + " m/s)");

    Simulation3D sim;
    FluidWorld3D world;
    FluidMaterial q = FluidMaterial::water();
    q.linear_drag = 0.0_r;
    FluidVolumeDef qd = FluidVolumeDef::box(FluidLevel::still, AABB3D(RVec3{ -5.0_r, -10.0_r, -5.0_r }, RVec3{ 5.0_r, 0.0_r, 5.0_r }), q, 1.0_r);
    qd.coupling.linear_damping = 0.0_r;
    world.add_volume(qd);
    RigidBody3D& b = add_box(sim, RVec3{ 0.0_r, -3.0_r, 0.0_r }, 0.5_r, 1000.0_r);
    b.synchronize_colliders();
    b.set_linear_velocity(RVec3{ 1.0_r, 0.0_r, 0.0_r });
    const Real d1 = vec3::length(world.forces_on(b).drag);
    b.set_linear_velocity(RVec3{ 2.0_r, 0.0_r, 0.0_r });
    const Real d2 = vec3::length(world.forces_on(b).drag);
    b.set_linear_velocity(RVec3{ 4.0_r, 0.0_r, 0.0_r });
    const Real d4 = vec3::length(world.forces_on(b).drag);
    check(std::abs(d2 / d1 - 4.0_r) < 0.01_r && std::abs(d4 / d1 - 16.0_r) < 0.05_r, "drag: quadratic drag scales with speed squared (" + num(d2 / d1) + ", " + num(d4 / d1) + ")");
    FluidMaterial lin = FluidMaterial::honey();
    lin.quadratic_drag = 0.0_r;
    FluidWorld3D world_l;
    qd.material = lin;
    world_l.add_volume(qd);
    b.set_linear_velocity(RVec3{ 1.0_r, 0.0_r, 0.0_r });
    const Real l1 = vec3::length(world_l.forces_on(b).drag);
    b.set_linear_velocity(RVec3{ 3.0_r, 0.0_r, 0.0_r });
    const Real l3 = vec3::length(world_l.forces_on(b).drag);
    check(std::abs(l3 / l1 - 3.0_r) < 0.01_r, "drag: linear (viscous) drag scales with speed");
    const Real honey_d = l1;
    FluidMaterial wl = FluidMaterial::water();
    wl.quadratic_drag = 0.0_r;
    FluidWorld3D world_w;
    qd.material = wl;
    world_w.add_volume(qd);
    check(honey_d > 1000.0_r * vec3::length(world_w.forces_on(b).drag), "drag: viscous drag follows viscosity");
}

void test_currents() {
    Simulation3D sim;
    FluidWorld3D world;
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::still, AABB3D(RVec3{ -50.0_r, -10.0_r, -5.0_r }, RVec3{ 50.0_r, 0.0_r, 5.0_r }), FluidMaterial::water(), 1.0_r);
    d.current = RVec3{ 1.0_r, 0.0_r, 0.0_r };
    world.add_volume(d);
    RigidBody3D& box = add_box(sim, RVec3{ 0.0_r, 0.1_r, 0.0_r }, 0.5_r, 600.0_r);
    run(world, sim, 300);
    check(box.position().x > 2.0_r, "currents: a floating box drifts downstream in a still current (" + num(box.position().x) + ")");

    Simulation3D sim2;
    FluidWorld3D world2;
    FluidVolumeDef w = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 30.0_r, 2.0_r, 6.0_r }), FluidMaterial::water(), 0.5_r);
    w.current = RVec3{ 0.0_r, 0.0_r, 0.8_r };
    FluidVolume3D& river = world2.add_volume(w);
    RigidBody3D& raft = add_box(sim2, RVec3{ 6.0_r, 1.1_r, 1.5_r }, 0.4_r, 600.0_r);
    run(world2, sim2, 300);
    check(raft.position().z > 2.5_r, "currents: a floating box drifts downstream on the waves level (" + num(raft.position().z) + ")");
    check(vec3::length(river.velocity_at(RVec3{ 20.0_r, 0.5_r, 3.0_r }) - w.current) < 0.05_r, "currents: velocity_at reports the current");

    Simulation3D sim3;
    FluidWorld3D world3;
    FluidVolumeDef s = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 24.0_r, 3.0_r, 3.0_r }), FluidMaterial::water(), 0.0_r);
    FluidVolume3D& dam = world3.add_volume(s);
    for (int k = 0; k < dam.columns_z(); ++k) {
        for (int i = 0; i < dam.columns_x(); ++i) dam.set_depth(i, k, i < 24 ? 2.0_r : 0.6_r);
    }
    RigidBody3D& log = add_box(sim3, RVec3{ 7.0_r, 0.7_r, 1.5_r }, 0.3_r, 500.0_r);
    run(world3, sim3, 120);
    check(log.position().x > 8.0_r, "currents: a dam-break surge in the waves level carries a box downstream (" + num(log.position().x) + ")");

    Simulation3D sim4;
    FluidWorld3D world4;
    FluidVolumeDef fl = FluidVolumeDef::box(FluidLevel::flowing, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 12.0_r, 1.5_r, 1.0_r }), FluidMaterial::water(), 0.0_r);
    fl.cell_size = 0.25_r;
    FluidVolume3D& chan = world4.add_volume(fl);
    for (int k = 0; k < chan.cells_z(); ++k) {
        for (int j = 0; j < 4; ++j) {
            for (int i = 0; i < 12; ++i) chan.set_amount(i, j, k, 1.0_r);
        }
        for (int i = 12; i < chan.cells_x(); ++i) chan.set_amount(i, 0, k, 1.0_r);
    }
    RigidBody3D& cork = add_box(sim4, RVec3{ 3.6_r, 0.35_r, 0.5_r }, 0.2_r, 400.0_r);
    run(world4, sim4, 180);
    check(cork.position().x > 4.2_r, "currents: a floating box drifts with the flowing level (" + num(cork.position().x) + ")");
}

void test_lod_and_queries() {
    FluidWorld3DConfig cfg;
    cfg.lod_distance = 50.0_r;
    cfg.lod_interval = 4;
    cfg.freeze_distance = 500.0_r;
    FluidWorld3D world(cfg);
    FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 8.0_r, 2.0_r, 8.0_r }), FluidMaterial::water(), 0.5_r);
    FluidVolume3D& near_v = world.add_volume(d);
    d.bounds = AABB3D(RVec3{ 200.0_r, 0.0_r, 0.0_r }, RVec3{ 208.0_r, 2.0_r, 8.0_r });
    FluidVolume3D& far_v = world.add_volume(d);
    d.bounds = AABB3D(RVec3{ 900.0_r, 0.0_r, 0.0_r }, RVec3{ 908.0_r, 2.0_r, 8.0_r });
    FluidVolume3D& frozen_v = world.add_volume(d);
    world.set_focus(RVec3{ 4.0_r, 0.0_r, 4.0_r });
    int stepped = 0;
    for (int i = 0; i < 8; ++i) {
        world.step(kDt);
        stepped += static_cast<int>(world.stats().stepped);
    }
    check(near_v.time() > 0.13_r && std::abs(far_v.time() - near_v.time()) < 1e-4_r && frozen_v.time() == 0.0_r, "lod: far volumes step less often, very far ones freeze");
    check(stepped == 8 + 2, "lod: far volume stepped every 4th frame (" + std::to_string(stepped) + ")");
    check(std::abs(world.surface_height(RVec3{ 4.0_r, 5.0_r, 4.0_r }) - 1.0_r) < 1e-4_r && world.inside(RVec3{ 4.0_r, 0.5_r, 4.0_r }), "queries: world surface height and inside");
    check(world.volume_at(RVec3{ 204.0_r, 0.5_r, 4.0_r }) == &far_v, "queries: volume_at");
    near_v.set_visible(false);
    check(!near_v.visible(), "lod: visibility flag");
}

template <class Fn>
double time_ms(int frames, Fn&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) fn();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
}

void benchmark() {
    {
        FluidWorld3D world;
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::blocky, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 64.0_r, 16.0_r, 64.0_r }));
        d.cell_size = 1.0_r;
        d.blocky.tick_interval = 1;
        FluidVolume3D& v = world.add_volume(d);
        for (int i = 0; i < 8; ++i) v.set_source(4 + i * 7, 15, 4 + i * 7);
        const double ms = time_ms(240, [&] { world.step(kDt); });
        std::printf("bench blocky    64x16x64 cells      %8.3f ms/step (%zu/%zu chunks active)\n", ms, v.active_regions(), v.total_regions());
    }
    {
        Simulation3D sim;
        FluidWorld3D world;
        world.add_volume(AABB3D(RVec3{ -40.0_r, -10.0_r, -40.0_r }, RVec3{ 40.0_r, 0.0_r, 40.0_r }), FluidLevel::still, FluidMaterial::water(), 1.0_r);
        for (int i = 0; i < 64; ++i) add_box(sim, RVec3{ static_cast<Real>(i % 8) * 4.0_r - 16.0_r, 0.2_r, static_cast<Real>(i / 8) * 4.0_r - 16.0_r }, 0.5_r, 600.0_r);
        const double ms = time_ms(240, [&] { world.step(sim, kDt); sim.step(kDt); });
        std::printf("bench still     64 floating boxes   %8.3f ms/step (fluid %.3f ms incl. coupling)\n", ms, world.stats().milliseconds);
    }
    {
        FluidWorld3D world;
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::flowing, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 12.0_r, 4.0_r, 12.0_r }));
        d.cell_size = 0.25_r;
        FluidVolume3D& v = world.add_volume(d);
        v.fill_box(AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 4.0_r, 3.0_r, 12.0_r }));
        const double ms = time_ms(240, [&] { world.step(kDt); });
        std::printf("bench flowing   48x16x48 cells      %8.3f ms/step (%zu/%zu chunks active)\n", ms, v.active_regions(), v.total_regions());
    }
    {
        Simulation3D sim;
        FluidWorld3D world;
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 64.0_r, 4.0_r, 64.0_r }), FluidMaterial::water(), 0.5_r);
        d.cell_size = 0.25_r;
        FluidVolume3D& v = world.add_volume(d);
        add_ball(sim, RVec3{ 32.0_r, 3.0_r, 32.0_r }, 0.5_r, 500.0_r);
        const double busy = time_ms(240, [&] { world.step(sim, kDt); sim.step(kDt); });
        const std::size_t active = v.active_regions();
        std::printf("bench waves     256x256 columns     %8.3f ms/step (%zu/%zu tiles active, %d substeps)\n", busy, active, v.total_regions(), world.stats().substeps);
        FluidWorld3D calm_world;
        FluidVolume3D& calm = calm_world.add_volume(d);
        calm_world.step(kDt);
        calm_world.step(kDt);
        const double idle = time_ms(240, [&] { calm_world.step(kDt); });
        std::printf("bench waves     256x256 calm        %8.3f ms/step (%zu tiles active)\n", idle, calm.active_regions());
        FluidWorld3DConfig lc;
        lc.quality = FluidQuality::low;
        FluidWorld3D low_world(lc);
        FluidVolume3D& lowv = low_world.add_volume(d);
        lowv.disturb(RVec3{ 32.0_r, 2.0_r, 32.0_r }, 0.5_r, 2.0_r);
        const double low = time_ms(240, [&] { low_world.step(kDt); });
        std::printf("bench waves     low quality %dx%d %8.3f ms/step\n", lowv.columns_x(), lowv.columns_z(), low);
    }
    {
        Simulation3D sim;
        FluidWorld3D world;
        FluidVolumeDef d = FluidVolumeDef::box(FluidLevel::realistic, AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 3.0_r, 1.5_r, 1.0_r }));
        d.particles.radius = 0.03_r;
        FluidVolume3D& v = world.add_volume(d);
        v.add_particle_box(AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 1.0_r, 0.8_r, 1.0_r }));
        const double ms = time_ms(120, [&] { world.step(sim, kDt); sim.step(kDt); });
        std::printf("bench realistic %zu particles     %8.3f ms/step (%d substeps x %d iterations, %zu awake)\n", v.particle_count(), ms, world.stats().substeps, world.stats().iterations, v.awake_particles());
        const double later = time_ms(120, [&] { world.step(sim, kDt); sim.step(kDt); });
        std::printf("bench realistic settling          %8.3f ms/step (%zu awake)\n", later, v.awake_particles());
        FluidWorld3DConfig tc;
        tc.threads = 2;
        FluidWorld3D threaded(tc);
        FluidVolume3D& tv = threaded.add_volume(d);
        tv.add_particle_box(AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 1.0_r, 0.8_r, 1.0_r }));
        const double t2 = time_ms(120, [&] { threaded.step(sim, kDt); });
        std::printf("bench realistic 2 threads         %8.3f ms/step\n", t2);
        FluidWorld3DConfig lq;
        lq.quality = FluidQuality::low;
        FluidWorld3D low(lq);
        FluidVolumeDef ld = d;
        ld.particles.radius = 0.03_r;
        FluidVolume3D& lv = low.add_volume(ld);
        lv.add_particle_box(AABB3D(RVec3{ 0.0_r, 0.0_r, 0.0_r }, RVec3{ 1.0_r, 0.8_r, 1.0_r }));
        const double lt = time_ms(120, [&] { low.step(sim, kDt); });
        std::printf("bench realistic low quality %zu   %8.3f ms/step (%d substeps x %d iterations)\n", lv.particle_count(), lt, low.stats().substeps, low.stats().iterations);
    }
}

} // namespace

int main() {
    test_materials_and_quality();
    test_blocky();
    test_flowing();
    test_flowing_viscosity();
    test_still_buoyancy();
    test_still_z_up();
    test_waves();
    test_waves_threads_and_walls();
    test_realistic();
    test_realistic_over_wall();
    test_realistic_viscosity();
    test_realistic_bounce_and_friction();
    test_drag();
    test_currents();
    test_lod_and_queries();
    benchmark();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
