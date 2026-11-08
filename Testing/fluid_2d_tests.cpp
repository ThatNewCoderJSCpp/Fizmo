#include "../Includes/physics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

RigidBody2D& add_box(Simulation2D& sim, const RVec2& pos, Real hx, Real hy, Real density) {
    RigidBody2D& b = sim.add_body(pos, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape2D::make_box(hx, hy), Material(density));
    b.synchronize_colliders();
    return b;
}

RigidBody2D& add_ball(Simulation2D& sim, const RVec2& pos, Real radius, Real density) {
    RigidBody2D& b = sim.add_body(pos, PhysicsFlags::DefaultDynamic);
    b.add_collider(Shape2D::make_circle(RVec2{}, radius), Material(density));
    b.synchronize_colliders();
    return b;
}

RigidBody2D& add_static(Simulation2D& sim, const RVec2& pos, Real hx, Real hy) {
    RigidBody2D& b = sim.add_body(pos, PhysicsFlags::DefaultStatic);
    b.add_collider(Shape2D::make_box(hx, hy), Material(1.0_r));
    b.synchronize_colliders();
    return b;
}

void run(FluidWorld2D& world, Simulation2D& sim, int frames) {
    for (int i = 0; i < frames; ++i) {
        world.step(sim, kDt);
        sim.step(kDt);
    }
}

void run(FluidWorld2D& world, int frames) {
    for (int i = 0; i < frames; ++i) world.step(kDt);
}

bool finite(const RVec2& v) { return std::isfinite(v.x) && std::isfinite(v.y); }

template <class Fn>
double time_ms(int frames, Fn&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) fn();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
}

Real mean_row(const FluidVolume2D& v, SandCell2D cell) {
    double s = 0.0;
    std::size_t n = 0;
    for (int j = 0; j < v.cells_y(); ++j) {
        for (int i = 0; i < v.cells_x(); ++i) {
            if (v.sand_cell(i, j) != cell) continue;
            s += j;
            ++n;
        }
    }
    return n ? static_cast<Real>(s / static_cast<double>(n)) : -1.0_r;
}

void test_basics() {
    check(std::string(to_string(SandCell2D::lava)) == "lava" && std::string(to_string(SandKind2D::powder)) == "powder", "basics: names");
    check(static_cast<int>(SandCell2D::count) == 13, "basics: 13 cell materials");
    check(sand_cell_info(SandCell2D::sand).kind == SandKind2D::powder && sand_cell_info(SandCell2D::water).kind == SandKind2D::liquid && sand_cell_info(SandCell2D::steam).kind == SandKind2D::gas && sand_cell_info(SandCell2D::wall).kind == SandKind2D::solid && sand_cell_info(SandCell2D::fire).kind == SandKind2D::fire, "basics: cell kinds");
    check(sand_cell_info(SandCell2D::oil).density < sand_cell_info(SandCell2D::water).density && sand_cell_info(SandCell2D::water).density < sand_cell_info(SandCell2D::sand).density, "basics: cell densities layer");
    const FluidColor c{ 0.2f, 0.4f, 0.6f, 0.8f };
    const FluidColor r = fluid_unpack_color(fluid_pack_color(c));
    check(std::abs(r.r - c.r) < 0.01f && std::abs(r.g - c.g) < 0.01f && std::abs(r.b - c.b) < 0.01f && std::abs(r.a - c.a) < 0.01f, "basics: colour pack round trip");
    check((fluid_pack_color(FluidColor{ 1.0f, 0.0f, 0.0f, 1.0f }) & 0xffu) == 0xffu, "basics: packed colour is R in the low byte");
    const FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::waves, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.0_r }), FluidMaterial::oil(), 0.5_r);
    check(d.level == FluidLevel::waves && d.material.preset == FluidPreset::oil && d.fill == 0.5_r, "basics: def box");
    check(std::string(to_string(FluidTransfer2D::apic)) == "apic", "basics: transfer names");
}

void test_still() {
    Simulation2D sim;
    FluidWorld2D world;
    FluidVolume2D& v = world.add_volume(AABB(RVec2{ -10.0_r, -5.0_r }, RVec2{ 10.0_r, 0.0_r }), FluidLevel::still, FluidMaterial::water(), 1.0_r);
    RigidBody2D& box = add_box(sim, RVec2{ -4.0_r, 1.0_r }, 0.5_r, 0.5_r, 500.0_r);
    RigidBody2D& ball = add_ball(sim, RVec2{ 0.0_r, 1.0_r }, 0.5_r, 250.0_r);
    RigidBody2D& rock = add_box(sim, RVec2{ 4.0_r, 1.0_r }, 0.3_r, 0.3_r, 3000.0_r);
    run(world, sim, 900);
    check(std::abs(box.position().y) < 0.06_r && std::abs(box.linear_velocity().y) < 0.05_r, "still: half-density box floats half submerged (y=" + num(box.position().y) + ")");
    check(std::abs(ball.position().y - 0.202_r) < 0.06_r, "still: quarter-density ball floats with a quarter submerged (y=" + num(ball.position().y) + ")");
    check(rock.position().y < -3.0_r, "still: dense box sinks (y=" + num(rock.position().y) + ")");
    const FluidForces2D f = world.forces_on(box);
    check(f.touching && f.buoyancy.y > 0.0_r && f.submerged_fraction > 0.4_r && f.submerged_fraction < 0.6_r, "still: forces_on reports buoyancy (" + num(f.submerged_fraction) + ")");
    check(world.body_forces(box) != nullptr, "still: body_forces after step");
    check(std::abs(world.surface_height(RVec2{ 2.0_r, -1.0_r })) < 1e-4_r && std::abs(world.surface_height(3.0_r)) < 1e-4_r, "still: surface height");
    check(std::abs(v.depth_at(RVec2{ 0.0_r, -1.0_r }) - 1.0_r) < 1e-4_r && v.inside(RVec2{ 0.0_r, -1.0_r }) && !v.inside(RVec2{ 0.0_r, 1.0_r }), "still: depth and inside");
    check(std::abs(v.total_volume() - 100.0_r) < 0.01_r && v.sleeping(), "still: volume and sleeping");
    check(world.volume_at(RVec2{ 0.0_r, -2.0_r }) == &v && world.volume_at(RVec2{ 50.0_r, 0.0_r }) == nullptr, "still: volume_at");
    v.set_current(RVec2{ 1.0_r, 0.0_r });
    check(std::abs(world.velocity_at(RVec2{ 0.0_r, -1.0_r }).x - 1.0_r) < 1e-6_r, "still: current");
}

void test_still_screen_axes() {
    Simulation2D sim;
    sim.set_gravity(RVec2{ 0.0_r, 9.81_r });
    FluidWorld2DConfig cfg;
    cfg.gravity = RVec2{ 0.0_r, 9.81_r };
    FluidWorld2D world(cfg);
    FluidVolume2D& v = world.add_volume(AABB(RVec2{ -10.0_r, 0.0_r }, RVec2{ 10.0_r, 5.0_r }), FluidLevel::still, FluidMaterial::water(), 1.0_r);
    check(v.up().y < 0.0_r, "still y-down: up points to -y");
    RigidBody2D& box = add_box(sim, RVec2{ 0.0_r, -1.0_r }, 0.5_r, 0.5_r, 500.0_r);
    run(world, sim, 900);
    check(std::abs(box.position().y) < 0.06_r, "still y-down: box floats at the surface (y=" + num(box.position().y) + ")");
}

void test_waves() {
    FluidWorld2D world;
    FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::waves, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 20.0_r, 4.0_r }), FluidMaterial::water(), 0.5_r);
    d.cell_size = 0.1_r;
    FluidVolume2D& v = world.add_volume(d);
    check(v.columns() == 200 && std::abs(v.total_volume() - 40.0_r) < 1e-3_r, "waves: columns and volume");
    const Real v0 = v.total_volume();
    v.disturb(RVec2{ 5.0_r, 2.0_r }, 0.3_r, 1.0_r);
    int probe = 0;
    v.column_of(RVec2{ 10.0_r, 2.0_r }, probe);
    Real dev = 0.0_r;
    for (int i = 0; i < 180; ++i) {
        world.step(kDt);
        dev = std::max(dev, std::abs(v.column_height(probe) - 2.0_r));
    }
    check(dev > 0.01_r, "waves: ripple travels (" + num(dev) + ")");
    check(std::abs(v.total_volume() - v0) < 1e-3_r * v0, "waves: volume conserved (" + num(v.total_volume()) + ")");
    check(std::abs(v.surface_height(RVec2{ 3.0_r, 0.0_r }) - v.column_height(30)) < 0.05_r, "waves: surface_height samples columns");
    for (int i = 0; i < 1800 && !v.sleeping(); ++i) world.step(kDt);
    check(v.sleeping(), "waves: calm water sleeps (" + std::to_string(v.active_regions()) + " tiles)");

    FluidVolumeDef2D dw = d;
    dw.bounds = AABB(RVec2{ 0.0_r, 10.0_r }, RVec2{ 10.0_r, 14.0_r });
    dw.fill = 0.0_r;
    FluidVolume2D& w = world.add_volume(dw);
    for (int i = 0; i < 40; ++i) w.set_depth(i, 2.0_r);
    w.set_wall(50, true);
    for (int i = 0; i < 600; ++i) world.step(kDt);
    check(w.depth(60) < 1e-6_r && w.depth(45) > 0.5_r && w.depth(50) == 0.0_r, "waves: wall column holds water back");

    Simulation2D sim;
    FluidWorld2D bw;
    FluidVolume2D& pool = bw.add_volume(d);
    RigidBody2D& box = add_box(sim, RVec2{ 10.0_r, 3.0_r }, 0.5_r, 0.5_r, 500.0_r);
    Real ripple = 0.0_r;
    int col = 0;
    pool.column_of(RVec2{ 14.0_r, 0.0_r }, col);
    for (int i = 0; i < 600; ++i) {
        bw.step(sim, kDt);
        sim.step(kDt);
        ripple = std::max(ripple, std::abs(pool.column_height(col) - 2.0_r));
    }
    check(std::abs(box.position().y - 2.0_r) < 0.1_r, "waves: box floats on the heightfield (y=" + num(box.position().y) + ")");
    check(ripple > 0.005_r, "waves: falling body makes ripples (" + num(ripple) + ")");
    check(std::abs(pool.total_volume() - 40.0_r) < 0.05_r, "waves: displacement conserves water");
    FluidVolumeDef2D sw = d;
    sw.waves.swell_amplitude = 0.2_r;
    FluidVolume2D& swell = world.add_volume(sw);
    world.step(kDt);
    Real lo = 1e9_r, hi = -1e9_r;
    for (Real hgt : swell.heights()) {
        lo = std::min(lo, hgt);
        hi = std::max(hi, hgt);
    }
    check(hi - lo > 0.1_r, "waves: swell adds analytic waves");
}

void test_flowing() {
    FluidWorld2D world;
    FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::flowing, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 4.0_r, 2.0_r }));
    d.cell_size = 0.1_r;
    FluidVolume2D& v = world.add_volume(d);
    check(v.cells_x() == 40 && v.cells_y() == 20, "flowing: grid size");
    v.fill_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 1.0_r, 1.0_r }), 1.0_r);
    const Real v0 = v.total_volume();
    check(std::abs(v0 - 1.0_r) < 1e-3_r, "flowing: fill_box volume (" + num(v0) + ")");
    run(world, 900);
    check(std::abs(v.total_volume() - v0) < 1e-3_r * v0, "flowing: mass conserved (" + num(v.total_volume()) + ")");
    const Real hl = v.surface_height(RVec2{ 0.3_r, 0.05_r }), hr = v.surface_height(RVec2{ 3.7_r, 0.05_r });
    check(std::abs(hl - 0.25_r) < 0.08_r && std::abs(hr - 0.25_r) < 0.08_r, "flowing: water levels out (" + num(hl) + ", " + num(hr) + ")");
    for (int i = 0; i < 1200 && !v.sleeping(); ++i) world.step(kDt);
    check(v.active_regions() == 0, "flowing: settled water sleeps (" + std::to_string(v.active_regions()) + ")");

    FluidVolumeDef2D dh = d;
    dh.bounds = AABB(RVec2{ 0.0_r, 10.0_r }, RVec2{ 4.0_r, 12.0_r });
    FluidVolume2D& h = world.add_volume(dh);
    for (int j = 1; j < 20; ++j) h.set_solid(20, j, true);
    h.fill_box(AABB(RVec2{ 0.0_r, 10.0_r }, RVec2{ 2.0_r, 11.0_r }), 1.0_r);
    run(world, 1200);
    Real right = 0.0_r;
    for (int j = 0; j < 20; ++j) {
        for (int i = 21; i < 40; ++i) right += h.amount(i, j);
    }
    check(right > 20.0_r && h.amount(20, 5) == 0.0_r, "flowing: water drains through the gap under a wall (" + num(right) + ")");

    FluidVolumeDef2D ds = d;
    ds.bounds = AABB(RVec2{ 10.0_r, 0.0_r }, RVec2{ 14.0_r, 2.0_r });
    FluidVolume2D& s = world.add_volume(ds);
    s.set_source(20, 15);
    run(world, 300);
    check(s.total_volume() > 0.5_r && s.source(20, 15), "flowing: source keeps producing water (" + num(s.total_volume()) + ")");

    Simulation2D sim;
    FluidWorld2D bw;
    FluidVolumeDef2D db = d;
    db.fill = 0.5_r;
    FluidVolume2D& pool = bw.add_volume(db);
    const Real before = pool.surface_height(RVec2{ 0.3_r, 0.5_r });
    const Real pv0 = pool.total_volume();
    RigidBody2D& box = add_box(sim, RVec2{ 2.0_r, 1.3_r }, 0.8_r, 0.15_r, 700.0_r);
    run(bw, sim, 600);
    static_cast<void>(before);
    Real inside = 0.0_r;
    std::size_t occupied = 0;
    for (std::size_t c = 0; c < pool.cell_occupancy().size(); ++c) {
        if (!pool.cell_occupancy()[c]) continue;
        ++occupied;
        inside += pool.cell_fill()[c];
    }
    check(occupied > 20 && inside < 0.05_r * static_cast<Real>(occupied), "flowing: body displaces water out of its cells (" + num(inside) + " in " + std::to_string(occupied) + ")");
    check(std::abs(pool.total_volume() - pv0) < 1e-3_r * pv0, "flowing: displacement conserves mass");
    check(box.position().y > 0.8_r && box.position().y < 1.2_r, "flowing: plank floats (y=" + num(box.position().y) + ")");
    check(bw.body_forces(box) && bw.body_forces(box)->heave_damping > 0.0_r, "flowing: heave damping reported");
}

FluidVolume2D& sand_box(FluidWorld2D& world, const RVec2& at, int cells, bool heat = false) {
    FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::blocky, AABB(at, at + RVec2{ static_cast<Real>(cells) * 0.1_r, static_cast<Real>(cells) * 0.1_r }));
    d.cell_size = 0.1_r;
    d.sand.heat = heat;
    return world.add_volume(d);
}

void test_sand() {
    FluidWorld2D world;
    FluidVolume2D& v = sand_box(world, RVec2{ 0.0_r, 0.0_r }, 64);
    check(v.cells_x() == 64 && v.cells_y() == 64 && v.sand_cells().size() == 64u * 64u, "sand: grid");
    const std::size_t grains = v.paint_cells_box(AABB(RVec2{ 3.0_r, 4.0_r }, RVec2{ 3.4_r, 6.0_r }), SandCell2D::sand);
    check(grains == 80, "sand: paint box (" + std::to_string(grains) + ")");
    run(world, 600);
    check(v.count_cells(SandCell2D::sand) == grains, "sand: grains conserved");
    int top = 0;
    for (int j = 0; j < 64; ++j) {
        if (v.sand_cell(32, j) == SandCell2D::sand) top = j;
    }
    int side = 0;
    for (int j = 0; j < 64; ++j) {
        if (v.sand_cell(26, j) == SandCell2D::sand) side = j + 1;
    }
    check(v.sand_cell(32, 0) == SandCell2D::sand && top >= 4 && top < 15 && side < top, "sand: grains fall and pile (top " + std::to_string(top) + ", side " + std::to_string(side) + ")");
    check(v.active_regions() == 0, "sand: settled pile sleeps (" + std::to_string(v.active_regions()) + " chunks)");

    FluidVolume2D& w = sand_box(world, RVec2{ 10.0_r, 0.0_r }, 64);
    const std::size_t water = w.paint_cells_box(AABB(RVec2{ 10.0_r, 2.0_r }, RVec2{ 11.0_r, 5.0_r }), SandCell2D::water);
    run(world, 900);
    check(w.count_cells(SandCell2D::water) == water, "sand: water conserved");
    int spread = 0;
    for (int i = 0; i < 64; ++i) spread += w.sand_cell(i, 0) == SandCell2D::water ? 1 : 0;
    check(spread == 64, "sand: water spreads over the floor (" + std::to_string(spread) + ")");
    check(w.surface_height(RVec2{ 13.0_r, 0.05_r }) > 0.3_r, "sand: surface query on liquid cells (" + num(w.surface_height(RVec2{ 13.0_r, 0.05_r })) + ")");

    FluidVolume2D& o = sand_box(world, RVec2{ 20.0_r, 0.0_r }, 32);
    o.paint_cells_box(AABB(RVec2{ 20.0_r, 0.0_r }, RVec2{ 23.2_r, 0.4_r }), SandCell2D::oil);
    o.paint_cells_box(AABB(RVec2{ 20.0_r, 0.4_r }, RVec2{ 23.2_r, 0.8_r }), SandCell2D::water);
    run(world, 1500);
    check(mean_row(o, SandCell2D::oil) > mean_row(o, SandCell2D::water), "sand: oil floats on water (" + num(mean_row(o, SandCell2D::oil)) + " vs " + num(mean_row(o, SandCell2D::water)) + ")");

    FluidVolume2D& l = sand_box(world, RVec2{ 30.0_r, 0.0_r }, 32);
    l.paint_cells_box(AABB(RVec2{ 30.0_r, 0.0_r }, RVec2{ 31.6_r, 0.5_r }), SandCell2D::water);
    l.paint_cells_box(AABB(RVec2{ 31.6_r, 0.0_r }, RVec2{ 33.2_r, 0.5_r }), SandCell2D::lava);
    std::size_t steam = 0;
    for (int i = 0; i < 120; ++i) {
        world.step(kDt);
        steam = std::max(steam, l.count_cells(SandCell2D::steam));
    }
    check(l.count_cells(SandCell2D::stone) > 0 && steam > 0, "sand: lava + water make stone and steam");

    FluidVolume2D& f = sand_box(world, RVec2{ 40.0_r, 0.0_r }, 32);
    const std::size_t wood = f.paint_cells_box(AABB(RVec2{ 41.0_r, 0.0_r }, RVec2{ 42.0_r, 1.0_r }), SandCell2D::wood);
    f.set_sand_cell(10, 10, SandCell2D::fire);
    std::size_t smoke = 0;
    for (int i = 0; i < 600; ++i) {
        world.step(kDt);
        smoke = std::max(smoke, f.count_cells(SandCell2D::smoke));
    }
    check(f.count_cells(SandCell2D::wood) < wood / 2 && smoke > 0, "sand: fire burns wood into smoke (" + std::to_string(f.count_cells(SandCell2D::wood)) + " left)");

    FluidVolume2D& st = sand_box(world, RVec2{ 50.0_r, 0.0_r }, 32);
    st.paint_cells_box(AABB(RVec2{ 50.0_r, 0.0_r }, RVec2{ 53.2_r, 0.3_r }), SandCell2D::steam);
    for (int i = 0; i < 40; ++i) world.step(kDt);
    const Real risen = mean_row(st, SandCell2D::steam);
    for (int i = 0; i < 600; ++i) world.step(kDt);
    check(risen > 3.0_r && st.count_cells(SandCell2D::water) > 0, "sand: steam rises and condenses (" + num(risen) + ", " + std::to_string(st.count_cells(SandCell2D::water)) + " water)");

    FluidVolume2D& a = sand_box(world, RVec2{ 60.0_r, 0.0_r }, 32);
    const std::size_t stone = a.paint_cells_box(AABB(RVec2{ 60.0_r, 0.0_r }, RVec2{ 63.2_r, 0.5_r }), SandCell2D::stone);
    a.paint_cells_box(AABB(RVec2{ 61.0_r, 0.5_r }, RVec2{ 62.0_r, 1.0_r }), SandCell2D::acid);
    run(world, 600);
    check(a.count_cells(SandCell2D::stone) < stone, "sand: acid dissolves stone");

    FluidVolume2D& ice = sand_box(world, RVec2{ 70.0_r, 0.0_r }, 32);
    ice.paint_cells_box(AABB(RVec2{ 70.0_r, 0.0_r }, RVec2{ 71.0_r, 1.0_r }), SandCell2D::ice);
    ice.paint_cells_box(AABB(RVec2{ 71.0_r, 0.0_r }, RVec2{ 71.5_r, 0.5_r }), SandCell2D::lava);
    run(world, 300);
    check(ice.count_cells(SandCell2D::ice) < 100, "sand: lava melts ice (" + std::to_string(ice.count_cells(SandCell2D::ice)) + ")");

    FluidVolume2D& hot = sand_box(world, RVec2{ 80.0_r, 0.0_r }, 32, true);
    check(!hot.cell_temperatures().empty(), "sand heat: temperature field");
    hot.paint_cells_box(AABB(RVec2{ 80.0_r, 0.0_r }, RVec2{ 81.6_r, 0.6_r }), SandCell2D::water);
    hot.paint_cells_box(AABB(RVec2{ 81.6_r, 0.0_r }, RVec2{ 83.2_r, 0.6_r }), SandCell2D::lava);
    hot.add_heat(RVec2{ 80.5_r, 0.3_r }, 0.3_r, 500.0_r);
    std::size_t boiled = 0;
    for (int i = 0; i < 300; ++i) {
        world.step(kDt);
        boiled = std::max(boiled, hot.count_cells(SandCell2D::steam));
    }
    check(boiled > 0 && hot.temperature(20, 1) > 100.0_r, "sand heat: heat boils water (" + num(hot.temperature(20, 1)) + ")");

    FluidVolume2D& e = sand_box(world, RVec2{ 90.0_r, 0.0_r }, 32);
    e.paint_cells_box(AABB(RVec2{ 90.0_r, 0.0_r }, RVec2{ 93.2_r, 1.6_r }), SandCell2D::sand);
    const std::size_t pre = e.count_cells(SandCell2D::sand);
    e.explode(RVec2{ 91.6_r, 0.8_r }, 0.6_r, 5.0_r);
    check(e.count_cells(SandCell2D::sand) < pre, "sand: explode blasts cells");
    const std::size_t erased = e.erase_cells(RVec2{ 91.6_r, 1.4_r }, 0.2_r);
    check(erased > 0, "sand: erase");

    FluidWorld2D w1, w2;
    w2.set_threads(2);
    FluidVolume2D& s1 = sand_box(w1, RVec2{ 0.0_r, 0.0_r }, 96);
    FluidVolume2D& s2 = sand_box(w2, RVec2{ 0.0_r, 0.0_r }, 96);
    for (FluidVolume2D* s : { &s1, &s2 }) {
        s->paint_cells_box(AABB(RVec2{ 1.0_r, 4.0_r }, RVec2{ 8.0_r, 9.0_r }), SandCell2D::water);
        s->paint_cells_box(AABB(RVec2{ 2.0_r, 2.0_r }, RVec2{ 6.0_r, 3.0_r }), SandCell2D::sand);
        s->paint_cells_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.0_r }), SandCell2D::lava);
    }
    run(w1, 200);
    run(w2, 200);
    check(s1.sand_cells() == s2.sand_cells(), "sand: identical results with 1 and 2 threads");
}

void test_sand_bodies() {
    Simulation2D sim;
    FluidWorld2D world;
    FluidVolume2D& v = sand_box(world, RVec2{ 0.0_r, 0.0_r }, 64);
    v.paint_cells_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 6.4_r, 0.2_r }), SandCell2D::wall);
    v.paint_cells_box(AABB(RVec2{ 0.0_r, 0.2_r }, RVec2{ 6.4_r, 2.2_r }), SandCell2D::water);
    RigidBody2D& floor = v.build_static_body(sim);
    check(!floor.colliders().empty(), "sand bodies: static body from wall cells (" + std::to_string(floor.colliders().size()) + " boxes)");
    std::vector<AABB> rects;
    check(v.solid_rectangles(rects) == 1, "sand bodies: wall row merges into one rectangle");
    const std::size_t water = v.count_cells(SandCell2D::water);
    RigidBody2D& box = add_box(sim, RVec2{ 3.2_r, 3.0_r }, 0.4_r, 0.4_r, 500.0_r);
    RigidBody2D& rock = add_box(sim, RVec2{ 1.5_r, 3.0_r }, 0.3_r, 0.3_r, 5000.0_r);
    run(world, sim, 900);
    check(box.position().y > 1.8_r && box.position().y < 2.6_r, "sand bodies: box floats on cell water (y=" + num(box.position().y) + ")");
    check(rock.position().y < 0.8_r, "sand bodies: heavy box sinks to the wall floor (y=" + num(rock.position().y) + ")");
    check(v.count_cells(SandCell2D::water) == water, "sand bodies: displacement keeps every water cell");
}

FluidVolume2D& particle_box(FluidWorld2D& world, const AABB& bounds, Real radius, const FluidMaterial& m = FluidMaterial::water()) {
    FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::realistic, bounds, m);
    d.particles.radius = radius;
    return world.add_volume(d);
}

Real max_x(const FluidVolume2D& v) {
    Real m = -1e9_r;
    for (const RVec2& p : v.positions()) m = std::max(m, p.x);
    return m;
}

void test_realistic() {
    Simulation2D sim;
    FluidWorld2D world;
    FluidVolume2D& v = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.0_r }), 0.01_r);
    const std::size_t n = v.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.6_r, 0.8_r }));
    check(n == 30u * 40u && v.particle_count() == n, "realistic: lattice fill (" + std::to_string(n) + ")");
    check(std::abs(v.particle_radius() - 0.01_r) < 1e-6_r && v.cells_x() == 60, "realistic: radius sets grid");
    run(world, sim, 480);
    bool in = true;
    for (const RVec2& p : v.positions()) in = in && finite(p) && p.x >= 0.0_r && p.x <= 2.0_r && p.y >= 0.0_r && p.y <= 1.0_r;
    check(in && v.particle_count() == n, "realistic: particles stay in bounds");
    const Real level = v.surface_height(RVec2{ 1.0_r, 0.02_r });
    check(std::abs(level - 0.24_r) < 0.05_r, "realistic: settles to the incompressible level (" + num(level) + ")");
    std::size_t cells = 0;
    for (std::uint8_t t : v.cell_types()) cells += t == 1 ? 1u : 0u;
    const Real area = static_cast<Real>(cells) * v.cell_size() * v.cell_size();
    check(area > 0.45_r * 0.85_r && area < 0.48_r * 1.25_r, "realistic: fluid area stays near 0.48 m2 (" + num(area) + ")");
    check(v.depth_at(RVec2{ 1.0_r, 0.05_r }) > 0.1_r && !v.inside(RVec2{ 1.0_r, 0.9_r }), "realistic: depth query");
    for (int i = 0; i < 2400 && !v.sleeping(); ++i) world.step(sim, kDt);
    check(v.sleeping() && v.awake_particles() == 0, "realistic: calm fluid sleeps");
    v.push(RVec2{ 1.0_r, 0.1_r }, 0.2_r, RVec2{ 2.0_r, 0.0_r });
    check(!v.sleeping(), "realistic: brushes wake the fluid");
    Real vx = 0.0_r;
    for (const RVec2& u : v.velocities()) vx = std::max(vx, u.x);
    check(vx > 1.0_r, "realistic: push sets velocity");
}

void test_realistic_walls() {
    FluidWorld2D world;
    FluidVolume2D& v = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.2_r }), 0.01_r);
    v.set_solid_box(AABB(RVec2{ 0.9_r, 0.0_r }, RVec2{ 1.0_r, 0.3_r }), true);
    v.paint_solid(RVec2{ 1.5_r, 0.6_r }, 0.08_r, true);
    v.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.6_r, 1.0_r }));
    const std::size_t n = v.particle_count();
    run(world, 300);
    const std::size_t right = v.particles_in(AABB(RVec2{ 1.0_r, 0.0_r }, RVec2{ 2.0_r, 1.2_r }));
    bool clean = true;
    for (const RVec2& p : v.positions()) {
        int i = 0, j = 0;
        if (v.cell_of(p, i, j) && v.solid(i, j)) clean = false;
    }
    check(right > n / 10, "realistic walls: fluid pours over the wall (" + std::to_string(right) + ")");
    check(clean, "realistic walls: no particle inside painted solids");
    check(v.particle_count() == n, "realistic walls: no particles lost");
    v.paint_solid(RVec2{ 0.3_r, 0.1_r }, 0.25_r, true);
    world.step(kDt);
    bool clean2 = true;
    for (const RVec2& p : v.positions()) {
        int i = 0, j = 0;
        if (v.cell_of(p, i, j) && v.solid(i, j)) clean2 = false;
    }
    check(clean2, "realistic walls: painting a solid over fluid clears it");
}

void test_realistic_viscosity_and_layers() {
    FluidWorld2D world;
    FluidVolume2D& water = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.0_r }), 0.01_r);
    FluidVolume2D& honey = particle_box(world, AABB(RVec2{ 0.0_r, 2.0_r }, RVec2{ 2.0_r, 3.0_r }), 0.01_r, FluidMaterial::honey());
    FluidVolume2D& oil = particle_box(world, AABB(RVec2{ 0.0_r, 4.0_r }, RVec2{ 2.0_r, 5.0_r }), 0.01_r, FluidMaterial::oil());
    water.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.4_r, 0.6_r }));
    honey.add_particle_box(AABB(RVec2{ 0.0_r, 2.0_r }, RVec2{ 0.4_r, 2.6_r }));
    oil.add_particle_box(AABB(RVec2{ 0.0_r, 4.0_r }, RVec2{ 0.4_r, 4.6_r }));
    run(world, 45);
    const Real fw = max_x(water), fh = max_x(honey), fo = max_x(oil);
    check(fh < fw - 0.15_r && fh < fo, "realistic: honey flows slower than oil and water (" + num(fw) + ", " + num(fo) + ", " + num(fh) + ")");

    FluidWorld2D lw;
    FluidVolume2D& mix = particle_box(lw, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 1.0_r, 1.0_r }), 0.01_r);
    FluidMaterial light = FluidMaterial::oil();
    light.density = 500.0_r;
    const std::uint8_t oil_id = mix.add_material(light);
    check(oil_id == 1 && mix.material_count() == 2 && mix.material(1).density == 500.0_r, "realistic: add_material");
    for (int j = 0; j < 30; ++j) {
        for (int i = 0; i < 50; ++i) {
            const RVec2 p{ (static_cast<Real>(i) + 0.5_r) * 0.02_r, (static_cast<Real>(j) + 0.5_r) * 0.02_r };
            mix.add_particle(p, RVec2{}, static_cast<std::uint8_t>(((i + j) % 2) ? oil_id : 0));
        }
    }
    run(lw, 600);
    double yo = 0.0, yw = 0.0;
    std::size_t no = 0, nw = 0;
    for (std::size_t i = 0; i < mix.particle_count(); ++i) {
        if (mix.material_ids()[i] == oil_id) {
            yo += mix.positions()[i].y;
            ++no;
        } else {
            yw += mix.positions()[i].y;
            ++nw;
        }
    }
    yo /= static_cast<double>(std::max<std::size_t>(no, 1));
    yw /= static_cast<double>(std::max<std::size_t>(nw, 1));
    check(no == 750 && nw == 750 && yo > yw + 0.08, "realistic: lighter fluid layers on top (" + num(yo) + " vs " + num(yw) + ")");
    check(mix.colors()[0] != 0u && mix.colors().size() == mix.particle_count(), "realistic: per-particle colours");
}

void test_realistic_bodies() {
    Simulation2D sim;
    FluidWorld2D world;
    FluidVolume2D& v = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.2_r }), 0.01_r);
    v.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 0.5_r }));
    add_static(sim, RVec2{ 1.0_r, -0.5_r }, 1.5_r, 0.5_r);
    add_static(sim, RVec2{ -0.5_r, 0.6_r }, 0.5_r, 1.0_r);
    add_static(sim, RVec2{ 2.5_r, 0.6_r }, 0.5_r, 1.0_r);
    run(world, sim, 60);
    RigidBody2D& wood = add_box(sim, RVec2{ 0.5_r, 0.8_r }, 0.1_r, 0.1_r, 500.0_r);
    RigidBody2D& rock = add_box(sim, RVec2{ 1.5_r, 0.8_r }, 0.06_r, 0.06_r, 3000.0_r);
    Real lowest = 10.0_r;
    for (int i = 0; i < 480; ++i) {
        world.step(sim, kDt);
        sim.step(kDt);
        lowest = std::min(lowest, wood.position().y);
    }
    const Real surface = v.surface_height(RVec2{ 1.0_r, 0.05_r });
    check(std::abs(wood.position().y - surface) < 0.06_r, "realistic bodies: wood floats at the surface (y=" + num(wood.position().y) + ", surface " + num(surface) + ")");
    check(lowest < surface, "realistic bodies: wood dips in when it lands");
    check(rock.position().y < 0.15_r, "realistic bodies: rock sinks (y=" + num(rock.position().y) + ")");
    check(v.particle_contacts() > 0 || world.body_forces(wood) != nullptr, "realistic bodies: coupling active");
    bool clean = true;
    for (const RVec2& p : v.positions()) {
        const RVec2 l = rock.get_local_point(p);
        if (std::abs(l.x) < 0.04_r && std::abs(l.y) < 0.04_r) clean = false;
    }
    check(clean, "realistic bodies: particles kept out of bodies");
    const RVec2 before = wood.linear_velocity();
    world.explode(sim, wood.position(), 0.4_r, 3.0_r);
    check(wood.linear_velocity().y > before.y + 1.0_r, "realistic bodies: explode pushes bodies");
}

void test_realistic_brushes() {
    FluidWorld2D world;
    FluidVolume2D& v = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.0_r }), 0.01_r);
    const std::size_t a = v.add_fluid_circle(RVec2{ 1.0_r, 0.5_r }, 0.1_r);
    const std::size_t b = v.add_fluid_circle(RVec2{ 1.0_r, 0.5_r }, 0.1_r);
    check(a > 50 && b < a / 5, "realistic brushes: pouring twice in one spot does not stack (" + std::to_string(a) + ", " + std::to_string(b) + ")");
    world.step(kDt);
    const std::size_t removed = v.remove_fluid_circle(RVec2{ 1.0_r, 0.5_r }, 0.05_r);
    check(removed > 0 && v.particle_count() == a + b - removed, "realistic brushes: erase");
    v.explode(RVec2{ 1.0_r, 0.4_r }, 0.3_r, 4.0_r);
    Real sp = 0.0_r;
    for (const RVec2& u : v.velocities()) sp = std::max(sp, std::sqrt(u.x * u.x + u.y * u.y));
    check(sp > 2.0_r, "realistic brushes: explode");
    v.swirl(RVec2{ 1.0_r, 0.4_r }, 0.3_r, 10.0_r);
    FluidEmitter2D e;
    e.position = RVec2{ 0.2_r, 0.8_r };
    e.velocity = RVec2{ 1.0_r, 0.0_r };
    e.radius = 0.03_r;
    e.rate = 600.0_r;
    v.add_emitter(e);
    const std::size_t before = v.particle_count();
    run(world, 60);
    check(v.particle_count() > before + 400 && v.emitter_count() == 1, "realistic brushes: emitter pours (" + std::to_string(v.particle_count() - before) + ")");
    v.clear_emitters();
    const std::size_t m = v.remove_material(0);
    check(m > 0 && v.particle_count() == 0, "realistic brushes: remove_material");
    FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::realistic, AABB(RVec2{ 0.0_r, 10.0_r }, RVec2{ 1.0_r, 11.0_r }));
    d.particles.radius = 0.01_r;
    d.particles.max_particles = 100;
    FluidVolume2D& capped = world.add_volume(d);
    capped.add_particle_box(capped.bounds());
    check(capped.particle_count() == 100, "realistic brushes: max_particles cap");
    FluidVolumeDef2D od = d;
    od.bounds = AABB(RVec2{ 0.0_r, 20.0_r }, RVec2{ 1.0_r, 21.0_r });
    od.particles.contain = false;
    od.particles.max_particles = 0;
    FluidVolume2D& open = world.add_volume(od);
    open.add_particle_box(AABB(RVec2{ 0.3_r, 20.5_r }, RVec2{ 0.7_r, 20.9_r }));
    run(world, 120);
    check(open.particle_count() == 0, "realistic brushes: open bounds lose fluid out of the bottom (" + std::to_string(open.particle_count()) + ")");
}

void test_realistic_threads_and_apic() {
    FluidWorld2DConfig c1, c2;
    c2.threads = 2;
    FluidWorld2D w1(c1), w2(c2);
    FluidVolume2D& a = particle_box(w1, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 1.5_r, 1.0_r }), 0.01_r);
    FluidVolume2D& b = particle_box(w2, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 1.5_r, 1.0_r }), 0.01_r);
    a.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.5_r, 0.8_r }));
    b.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.5_r, 0.8_r }));
    run(w1, 90);
    run(w2, 90);
    check(a.positions() == b.positions(), "realistic: identical results with 1 and 2 threads");
    FluidWorld2D w3;
    FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::realistic, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 1.5_r, 1.0_r }));
    d.particles.radius = 0.01_r;
    d.particles.transfer = FluidTransfer2D::apic;
    FluidVolume2D& ap = w3.add_volume(d);
    ap.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.5_r, 0.8_r }));
    run(w3, 300);
    bool ok = true;
    for (const RVec2& p : ap.positions()) ok = ok && finite(p) && p.y >= 0.0_r && p.y <= 1.0_r;
    check(ok && std::abs(ap.surface_height(RVec2{ 0.75_r, 0.02_r }) - 0.267_r) < 0.06_r, "realistic: APIC transfer settles (" + num(ap.surface_height(RVec2{ 0.75_r, 0.02_r })) + ")");
}

void test_render() {
    FluidWorld2D world;
    FluidVolume2D& v = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 1.0_r }), 0.01_r);
    v.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 2.0_r, 0.4_r }));
    run(world, 30);
    FluidRenderer2D r;
    FluidImageSettings2D s;
    s.width = 200;
    const std::vector<std::uint8_t>& img = r.render(v, s);
    check(r.width() == 200 && r.height() == 100 && img.size() == 200u * 100u * 4u, "render: image size follows the aspect ratio");
    auto alpha = [&](int x, int y) { return img[(static_cast<std::size_t>(y) * 200u + static_cast<std::size_t>(x)) * 4u + 3u]; };
    check(alpha(100, 90) > 120 && alpha(100, 10) == 0, "render: fluid is opaque below the surface and clear above (row 0 is the top)");
    std::vector<RVec2> segs;
    const std::size_t n = r.contour(0.35_r, segs);
    Real y = 0.0_r;
    for (const RVec2& p : segs) y += p.y;
    y /= static_cast<Real>(std::max<std::size_t>(segs.size(), 1));
    check(n > 50 && segs.size() == n * 2, "render: marching squares contour (" + std::to_string(n) + " segments)");
    check(y > 0.25_r && y < 0.55_r, "render: contour follows the surface (" + num(y) + ")");
    FluidVolume2D& sand = sand_box(world, RVec2{ 10.0_r, 0.0_r }, 32);
    sand.paint_cells_box(AABB(RVec2{ 10.0_r, 0.0_r }, RVec2{ 13.2_r, 0.5_r }), SandCell2D::sand);
    FluidImageSettings2D cs;
    cs.width = 0;
    const std::vector<std::uint8_t>& cimg = r.render(sand, cs);
    check(r.width() == 32 && r.height() == 32 && cimg[(31u * 32u + 5u) * 4u + 3u] == 255 && cimg[(0u * 32u + 5u) * 4u + 3u] == 0, "render: one pixel per sand cell");
    const FluidColor sc = fluid_unpack_color(sand_cell_color(SandCell2D::sand, 128));
    check(sc.r > sc.b && sc.a > 0.99f, "render: sand colour");
}

void benchmark() {
    std::printf("-- benchmarks (ms per 1/60 s frame, %s) --\n", sizeof(Real) == 4 ? "float" : "double");
    for (std::size_t target : { std::size_t{ 5000 }, std::size_t{ 20000 }, std::size_t{ 50000 } }) {
        for (int threads : { 1, 2 }) {
            if (threads == 2 && target == 5000) continue;
            FluidWorld2DConfig cfg;
            cfg.threads = threads;
            FluidWorld2D world(cfg);
            const Real W = 4.0_r, H = 2.0_r;
            const Real r = std::sqrt(0.4_r * W * H / (4.0_r * static_cast<Real>(target)));
            FluidVolume2D& v = particle_box(world, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ W, H }), r);
            v.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.4_r * W, H }));
            const double splash = time_ms(120, [&] { world.step(kDt); });
            const int sub = world.stats().substeps;
            for (int i = 0; i < 360; ++i) world.step(kDt);
            const double calm = time_ms(60, [&] { world.step(kDt); });
            std::printf("bench realistic %6zu particles %d thread%s: dam break %7.2f ms/step (%d substeps, %d cg iters), settling %6.2f ms/step%s\n", v.particle_count(), threads, threads > 1 ? "s" : " ", splash, sub, world.stats().iterations, calm, v.sleeping() ? " (asleep)" : "");
        }
    }
    {
        FluidWorld2D world;
        FluidVolume2D& v = sand_box(world, RVec2{ 0.0_r, 0.0_r }, 512);
        v.paint_cells_box(AABB(RVec2{ 2.0_r, 25.0_r }, RVec2{ 50.0_r, 50.0_r }), SandCell2D::sand, 0.5_r);
        v.paint_cells_box(AABB(RVec2{ 2.0_r, 25.0_r }, RVec2{ 50.0_r, 50.0_r }), SandCell2D::water, 0.6_r, false);
        v.paint_cells_box(AABB(RVec2{ 5.0_r, 10.0_r }, RVec2{ 20.0_r, 12.0_r }), SandCell2D::wood);
        v.paint_cells_box(AABB(RVec2{ 30.0_r, 5.0_r }, RVec2{ 45.0_r, 8.0_r }), SandCell2D::lava);
        const double busy = time_ms(120, [&] { world.step(kDt); });
        const std::size_t active = v.active_regions();
        for (int i = 0; i < 1200; ++i) world.step(kDt);
        const double later = time_ms(120, [&] { world.step(kDt); });
        std::printf("bench sand      512x512 cells, falling     %7.2f ms/step (%zu/%zu chunks active)\n", busy, active, v.total_regions());
        std::printf("bench sand      512x512 cells, settled     %7.2f ms/step (%zu chunks active)\n", later, v.active_regions());
        FluidWorld2DConfig tc;
        tc.threads = 2;
        FluidWorld2D tw(tc);
        FluidVolume2D& t = sand_box(tw, RVec2{ 0.0_r, 0.0_r }, 512);
        t.paint_cells_box(AABB(RVec2{ 2.0_r, 25.0_r }, RVec2{ 50.0_r, 50.0_r }), SandCell2D::sand, 0.5_r);
        t.paint_cells_box(AABB(RVec2{ 2.0_r, 25.0_r }, RVec2{ 50.0_r, 50.0_r }), SandCell2D::water, 0.6_r, false);
        const double t2 = time_ms(120, [&] { tw.step(kDt); });
        std::printf("bench sand      512x512 cells, 2 threads   %7.2f ms/step\n", t2);
    }
    {
        FluidWorld2D world;
        FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::flowing, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 25.6_r, 12.8_r }));
        d.cell_size = 0.1_r;
        FluidVolume2D& v = world.add_volume(d);
        v.fill_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 8.0_r, 8.0_r }), 1.0_r);
        const double ms = time_ms(240, [&] { world.step(kDt); });
        std::printf("bench flowing   256x128 cells              %7.2f ms/step (%zu/%zu chunks active)\n", ms, v.active_regions(), v.total_regions());
    }
    {
        FluidWorld2D world;
        FluidVolumeDef2D d = FluidVolumeDef2D::box(FluidLevel::waves, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 409.6_r, 4.0_r }), FluidMaterial::water(), 0.5_r);
        d.cell_size = 0.1_r;
        FluidVolume2D& v = world.add_volume(d);
        v.disturb(RVec2{ 200.0_r, 2.0_r }, 0.5_r, 2.0_r);
        const double ms = time_ms(240, [&] { world.step(kDt); });
        std::printf("bench waves     4096 columns               %7.3f ms/step (%d substeps)\n", ms, world.stats().substeps);
    }
}

} // namespace

int main(int argc, char** argv) {
    const bool bench_only = argc > 1 && std::string(argv[1]) == "bench";
    if (!bench_only) {
        test_basics();
        test_still();
        test_still_screen_axes();
        test_waves();
        test_flowing();
        test_sand();
        test_sand_bodies();
        test_realistic();
        test_realistic_walls();
        test_realistic_viscosity_and_layers();
        test_realistic_bodies();
        test_realistic_brushes();
        test_realistic_threads_and_apic();
        test_render();
    }
    if (argc <= 1 || bench_only) benchmark();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
