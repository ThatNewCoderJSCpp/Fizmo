#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include "../Includes/particles.hpp"
#include "../Source/Graphics/particles_2d.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

std::atomic<long long> g_allocs{ 0 };

} // namespace

void* operator new(std::size_t n) {
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t n) {
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace fizmo;
using namespace fizmo::particles;
using fizmo::linalg::vector3d;

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

const float kDt = 1.0f / 60.0f;

EmitterSettings plain() {
    EmitterSettings s;
    s.rate = 0.0f;
    s.speed = { 0.0f, 0.0f };
    s.spread = 0.0f;
    s.lifetime = { 1.0f, 1.0f };
    s.size = { 0.1f, 0.1f };
    s.max_particles = 10000;
    s.seed = 7;
    return s;
}

void run(Emitter& e, float seconds) {
    const int n = static_cast<int>(std::lround(seconds / kDt));
    for (int i = 0; i < n; ++i) e.step(kDt);
}

void test_text_round_trip() {
    for (int p = 0; p < static_cast<int>(ParticlePreset::Count); ++p) {
        const ParticlePreset which = static_cast<ParticlePreset>(p);
        for (int d = 0; d < 2; ++d) {
            const EmitterSettings s = preset(which, d ? ParticleDim::D2 : ParticleDim::D3);
            const std::string text = to_text(s);
            EmitterSettings back;
            std::string err;
            const bool ok = from_text(text, back, &err);
            check(ok && err.empty(), std::string("text: preset parses: ") + to_string(which) + " " + err);
            check(back == s && to_text(back) == text, std::string("text: preset round trips: ") + to_string(which) + (d ? " 2d" : " 3d"));
        }
    }
    ParticlePreset pp{};
    check(parse("magic_sparkle", pp) && pp == ParticlePreset::MagicSparkle, "text: enum parse");
    check(parse("Magic-Sparkle", pp) && pp == ParticlePreset::MagicSparkle, "text: enum parse is lenient on case and dashes");
    EmissionShape sh{};
    check(!parse("triangle", sh), "text: unknown enum rejected");
    check(std::string(to_string(SortMode::Approximate)) == "approximate" && std::string(to_string(ParticleBlend::Premultiplied)) == "premultiplied", "text: enum names");
    EmitterSettings s;
    std::string err;
    check(!from_text("rate: abc\nshape: blob\n", s, &err) && !err.empty(), "text: bad values reported");
    check(from_text("# comment\n\nrate: 33\nunknown_key: 5\nlifetime: 2\n", s, &err) && s.rate == 33.0f && s.lifetime.lo == 2.0f && s.lifetime.hi == 2.0f, "text: comments, unknown keys, single value ranges");
    EmitterSettings nested = preset(ParticlePreset::Explosion);
    nested.sub_emitters[0].settings.sub_emitters.push_back(SubEmitter{});
    nested.sub_emitters[0].settings.sub_emitters[0].settings.rate = 3.5f;
    EmitterSettings nb;
    check(from_text(to_text(nested), nb) && nb == nested && nb.sub_emitters[0].settings.sub_emitters[0].settings.rate == 3.5f, "text: nested sub-emitters round trip");
}

void test_curves() {
    LifeCurve c = LifeCurve::constant(2.0f);
    check(c.evaluate(0.3f) == 2.0f && c.is_constant(), "curve: constant");
    c = LifeCurve::linear(0.0f, 1.0f);
    check(std::fabs(c.evaluate(0.25f) - 0.25f) < 1e-6f && c.evaluate(-1.0f) == 0.0f && c.evaluate(2.0f) == 1.0f, "curve: linear and clamped");
    c = LifeCurve::constant(0.0f);
    c.add(1.0f, 0.0f).add(0.5f, 1.0f);
    check(c.count == 3 && c.keys[1].t == 0.5f && std::fabs(c.evaluate(0.75f) - 0.5f) < 1e-6f && c.peak() == 1.0f, "curve: keys stay sorted");
    for (int i = 0; i < 20; ++i) c.add(0.01f * i + 0.001f, 1.0f);
    check(c.count == LifeCurve::kMaxKeys, "curve: capped key count");
    LifeGradient g = LifeGradient::linear({ 1, 0, 0, 1 }, { 0, 0, 1, 0 });
    const ColorF m = g.evaluate(0.5f);
    check(std::fabs(m.r - 0.5f) < 1e-6f && std::fabs(m.b - 0.5f) < 1e-6f && std::fabs(m.a - 0.5f) < 1e-6f, "gradient: midpoint");

    EmitterSettings s = plain();
    s.size_over_life = LifeCurve::linear(1.0f, 3.0f);
    s.color = { 1, 1, 1, 1 };
    s.color_over_life = LifeGradient::linear({ 1, 1, 1, 1 }, { 1, 0, 0, 1 });
    s.alpha_over_life = LifeCurve::linear(1.0f, 0.0f);
    Emitter e(s);
    e.burst(1);
    run(e, 0.5f);
    check(std::fabs(e.particle_size(0) - 0.2f) < 0.01f, "curve: size over life applied (" + num(e.particle_size(0)) + ")");
    const ColorF c2 = e.particle_color(0.5f, 0.0f);
    check(std::fabs(c2.g - 0.5f) < 0.02f && std::fabs(c2.a - 0.5f) < 0.02f, "curve: colour and alpha over life");
}

void test_emission_counts() {
    EmitterSettings s = plain();
    s.rate = 100.0f;
    s.lifetime = { 10.0f, 10.0f };
    Emitter e(s);
    run(e, 1.0f);
    check(e.count() >= 99 && e.count() <= 101, "emission: rate 100/s for 1 s (" + num(e.count()) + ")");

    s.rate = 0.0f;
    s.bursts = { { 0.0f, 50, 1, 1.0f, 1.0f }, { 0.5f, 10, 3, 0.1f, 1.0f } };
    Emitter b(s);
    run(b, 1.0f);
    check(b.count() == 80, "emission: bursts with cycles (" + num(b.count()) + ")");

    s.bursts.clear();
    s.rate = 100.0f;
    s.loop = false;
    s.duration = 0.5f;
    Emitter d(s);
    run(d, 1.0f);
    check(d.count() >= 49 && d.count() <= 51 && !d.playing(), "emission: duration stops a one-shot (" + num(d.count()) + ")");

    s.rate = 0.0f;
    s.loop = true;
    s.duration = 0.5f;
    s.bursts = { { 0.1f, 5, 1, 1.0f, 1.0f } };
    Emitter l(s);
    run(l, 2.0f);
    check(l.count() == 20 && l.playing(), "emission: looping re-fires bursts (" + num(l.count()) + ")");

    s.bursts.clear();
    s.rate = 60.0f;
    s.duration = 0.0f;
    s.start_delay = 0.5f;
    Emitter dl(s);
    run(dl, 0.45f);
    const std::size_t before = dl.count();
    run(dl, 0.55f);
    check(before == 0 && dl.count() >= 29 && dl.count() <= 31, "emission: start delay (" + num(dl.count()) + ")");

    s = plain();
    s.rate_per_distance = 10.0f;
    s.lifetime = { 10.0f, 10.0f };
    Emitter tr(s);
    for (int i = 0; i <= 60; ++i) { tr.set_position(i * 0.1, 0.0, 0.0); tr.step(kDt); }
    check(tr.count() >= 59 && tr.count() <= 61, "emission: rate per distance (" + num(tr.count()) + ")");
    const ParticleView v = tr.view();
    float lo = 1e9f, hi = -1e9f;
    for (std::size_t i = 0; i < v.count; ++i) { lo = std::fmin(lo, v.px[i]); hi = std::fmax(hi, v.px[i]); }
    check(lo < 0.3f && hi > 5.5f, "emission: distance spawns spread along the path");

    s = plain();
    s.max_particles = 64;
    s.rate = 10000.0f;
    Emitter cap(s);
    run(cap, 0.2f);
    check(cap.count() == 64, "emission: capacity is a hard cap");
}

void test_lifetimes() {
    EmitterSettings s = plain();
    s.lifetime = { 0.5f, 0.5f };
    Emitter e(s);
    e.burst(100);
    run(e, 0.45f);
    check(e.count() == 100, "life: alive before lifetime");
    run(e, 0.1f);
    check(e.count() == 0 && e.died_total() == 100 && e.finished() == false, "life: all died after lifetime");
    e.stop();
    check(e.finished() && e.sleeping(), "life: stopped and empty emitter sleeps");

    s.lifetime = { 0.2f, 1.0f };
    Emitter r(s);
    r.burst(1000);
    run(r, 0.6f);
    check(r.count() > 350 && r.count() < 650, "life: lifetime range is random (" + num(r.count()) + ")");
}

void test_motion() {
    EmitterSettings s = plain();
    s.gravity_scale = 1.0f;
    s.lifetime = { 5.0f, 5.0f };
    Emitter g(s);
    g.burst(1);
    run(g, 1.0f);
    ParticleView v = g.view();
    check(std::fabs(v.vy[0] + 9.81f) < 0.05f && std::fabs(v.py[0] + 4.9f) < 0.2f, "motion: gravity (" + num(v.vy[0]) + ", " + num(v.py[0]) + ")");

    s = plain();
    s.speed = { 10.0f, 10.0f };
    s.drag = 1.0f;
    s.lifetime = { 5.0f, 5.0f };
    Emitter d(s);
    d.burst(1);
    run(d, 1.0f);
    v = d.view();
    check(std::fabs(v.vy[0] - 10.0f * std::exp(-1.0f)) < 0.05f, "motion: drag is exact exponential (" + num(v.vy[0]) + ")");

    s.speed = { 0.0f, 0.0f };
    s.wind = { 4.0f, 0.0f, 0.0f };
    s.drag = 3.0f;
    Emitter w(s);
    w.burst(1);
    run(w, 3.0f);
    v = w.view();
    check(std::fabs(v.vx[0] - 4.0f) < 0.01f, "motion: wind pulls toward wind velocity");

    s = plain();
    s.speed = { 1.0f, 1.0f };
    s.speed_over_life = LifeCurve::constant(0.0f);
    s.lifetime = { 5.0f, 5.0f };
    Emitter sc(s);
    sc.burst(1);
    run(sc, 1.0f);
    v = sc.view();
    check(std::fabs(v.py[0]) < 1e-5f && v.vy[0] == 1.0f, "motion: speed over life scales displacement");

    s = plain();
    s.turbulence = 2.0f;
    s.lifetime = { 5.0f, 5.0f };
    Emitter t(s);
    t.burst(50);
    run(t, 1.0f);
    v = t.view();
    double moved = 0.0;
    bool finite = true;
    for (std::size_t i = 0; i < v.count; ++i) {
        moved += std::sqrt(v.px[i] * v.px[i] + v.py[i] * v.py[i] + v.pz[i] * v.pz[i]);
        finite = finite && std::isfinite(v.px[i]) && std::isfinite(v.vx[i]);
    }
    check(finite && moved / v.count > 0.05, "motion: turbulence moves particles (" + num(moved / v.count) + ")");

    s = plain();
    s.spin = { 90.0f, 90.0f };
    s.lifetime = { 5.0f, 5.0f };
    Emitter sp(s);
    sp.burst(1);
    run(sp, 1.0f);
    check(std::fabs(sp.view().rotation[0] - 1.5708f) < 0.01f, "motion: spin in degrees per second");

    s = plain();
    s.speed = { 2.0f, 2.0f };
    s.inherit_velocity = 1.0f;
    s.lifetime = { 5.0f, 5.0f };
    Emitter iv(s);
    iv.set_velocity({ 3.0, 0.0, 0.0 });
    iv.burst(1);
    v = iv.view();
    check(v.vx[0] == 3.0f && v.vy[0] == 2.0f, "motion: inherit velocity");

    s = plain();
    s.space = SimulationSpace::Local;
    s.speed = { 1.0f, 1.0f };
    s.lifetime = { 5.0f, 5.0f };
    Emitter lc(s);
    lc.burst(1);
    run(lc, 1.0f);
    lc.set_position(10.0, 0.0, 0.0);
    lc.set_orientation({ 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 });
    const WorldBounds b = lc.bounds();
    check(b.valid && b.lo.x > 10.5 && b.hi.x < 11.5, "motion: local space follows the emitter transform");
}

void test_shapes() {
    auto make = [](EmissionShape shape) {
        EmitterSettings s = plain();
        s.shape = shape;
        s.shape_radius = 2.0f;
        s.shape_size = { 4.0f, 2.0f, 6.0f };
        s.lifetime = { 5.0f, 5.0f };
        return s;
    };
    {
        Emitter e(make(EmissionShape::Sphere));
        e.burst(2000);
        const ParticleView v = e.view();
        bool in = true;
        double mean = 0.0;
        for (std::size_t i = 0; i < v.count; ++i) { const float r = std::sqrt(v.px[i] * v.px[i] + v.py[i] * v.py[i] + v.pz[i] * v.pz[i]); in = in && r <= 2.0001f; mean += r; }
        mean /= v.count;
        check(in && std::fabs(mean - 1.5) < 0.08, "shape: sphere volume (mean r " + num(mean) + ")");
    }
    {
        EmitterSettings s = make(EmissionShape::Sphere);
        s.shape_thickness = 0.0f;
        Emitter e(s);
        e.burst(500);
        const ParticleView v = e.view();
        bool on = true;
        for (std::size_t i = 0; i < v.count; ++i) on = on && std::fabs(std::sqrt(v.px[i] * v.px[i] + v.py[i] * v.py[i] + v.pz[i] * v.pz[i]) - 2.0f) < 1e-3f;
        check(on, "shape: sphere surface");
    }
    {
        Emitter e(make(EmissionShape::Hemisphere));
        e.burst(500);
        const ParticleView v = e.view();
        bool up = true;
        for (std::size_t i = 0; i < v.count; ++i) up = up && v.py[i] >= -1e-5f;
        check(up, "shape: hemisphere faces the direction");
    }
    {
        Emitter e(make(EmissionShape::Box));
        e.burst(1000);
        const ParticleView v = e.view();
        bool in = true;
        for (std::size_t i = 0; i < v.count; ++i) in = in && std::fabs(v.px[i]) <= 2.0f && std::fabs(v.py[i]) <= 1.0f && std::fabs(v.pz[i]) <= 3.0f;
        check(in, "shape: box");
    }
    {
        Emitter e(make(EmissionShape::Ring));
        e.burst(500);
        const ParticleView v = e.view();
        bool ok = true;
        for (std::size_t i = 0; i < v.count; ++i) ok = ok && std::fabs(std::sqrt(v.px[i] * v.px[i] + v.pz[i] * v.pz[i]) - 2.0f) < 1e-3f && std::fabs(v.py[i]) < 1e-5f;
        check(ok, "shape: ring lies flat around the direction");
    }
    {
        EmitterSettings s = make(EmissionShape::Circle);
        s.shape_arc = 90.0f;
        Emitter e(s);
        e.burst(500);
        const ParticleView v = e.view();
        bool ok = true;
        for (std::size_t i = 0; i < v.count; ++i) ok = ok && std::sqrt(v.px[i] * v.px[i] + v.pz[i] * v.pz[i]) <= 2.0001f;
        check(ok, "shape: circle with arc");
    }
    {
        Emitter e(make(EmissionShape::Line));
        e.burst(200);
        const ParticleView v = e.view();
        bool ok = true;
        for (std::size_t i = 0; i < v.count; ++i) ok = ok && std::fabs(v.px[i]) <= 2.0f && v.py[i] == 0.0f && v.pz[i] == 0.0f;
        check(ok, "shape: line");
    }
    {
        EmitterSettings s = make(EmissionShape::Point);
        s.speed = { 1.0f, 1.0f };
        s.spread = 30.0f;
        Emitter e(s);
        e.burst(1000);
        const ParticleView v = e.view();
        bool ok = true;
        for (std::size_t i = 0; i < v.count; ++i) ok = ok && v.vy[i] >= std::cos(30.0f * 0.0174533f) - 1e-4f;
        check(ok, "shape: spread cone limits direction");
    }
    {
        EmitterSettings s = make(EmissionShape::Cone);
        s.speed = { 1.0f, 1.0f };
        s.spread = 45.0f;
        Emitter e(s);
        e.burst(500);
        const ParticleView v = e.view();
        bool ok = true;
        for (std::size_t i = 0; i < v.count; ++i) {
            const float r = std::sqrt(v.px[i] * v.px[i] + v.pz[i] * v.pz[i]);
            const float outward = r > 1e-4f ? (v.vx[i] * v.px[i] + v.vz[i] * v.pz[i]) / r : 0.0f;
            ok = ok && outward >= -1e-4f && v.vy[i] >= std::cos(45.0f * 0.0174533f) - 1e-3f;
        }
        check(ok, "shape: cone tilts outward");
    }
    {
        EmitterSettings s = make(EmissionShape::Mesh);
        Emitter e(s);
        const float tri[18] = { 0, 0, 0, 1, 0, 0, 0, 0, 1, 5, 5, 5, 6, 5, 5, 5, 5, 6 };
        e.set_emission_mesh(tri, 2);
        e.burst(400);
        const ParticleView v = e.view();
        bool ok = true;
        int high = 0;
        for (std::size_t i = 0; i < v.count; ++i) {
            const bool a = std::fabs(v.py[i]) < 1e-5f && v.px[i] >= -1e-5f && v.pz[i] >= -1e-5f && v.px[i] + v.pz[i] <= 1.0001f;
            const bool b = std::fabs(v.py[i] - 5.0f) < 1e-4f;
            ok = ok && (a || b);
            if (b) ++high;
        }
        check(ok && high > 120 && high < 280, "shape: mesh surface, area weighted");
    }
    {
        EmitterSettings s = to_2d(make(EmissionShape::Sphere), 10.0f);
        s.speed = { 5.0f, 5.0f };
        s.direction_mode = DirectionMode::Random;
        Emitter e(s);
        e.burst(300);
        run(e, 0.5f);
        const ParticleView v = e.view();
        bool flat = true;
        for (std::size_t i = 0; i < v.count; ++i) flat = flat && v.pz[i] == 0.0f && v.vz[i] == 0.0f;
        check(flat && s.shape == EmissionShape::Circle && s.shape_radius == 20.0f, "shape: 2d keeps z at 0");
    }
    {
        EmitterSettings s = plain();
        s.speed = { 1.0f, 1.0f };
        Emitter e(s);
        e.set_orientation({ 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 });
        e.burst(1);
        check(std::fabs(e.view().vx[0] - 1.0f) < 1e-5f, "shape: orientation rotates the direction");
        EmitterSettings s2 = to_2d(plain(), 1.0f);
        s2.speed = { 1.0f, 1.0f };
        Emitter e2(s2);
        e2.set_angle(90.0);
        e2.burst(1);
        check(std::fabs(e2.view().vx[0] - 1.0f) < 1e-5f && std::fabs(e2.view().vy[0]) < 1e-5f, "shape: 2d angle rotates the direction");
    }
}

void collide_cb(void* user, CollisionBatch& b) {
    ++*static_cast<int*>(user);
    for (std::size_t i = 0; i < b.count; ++i) {
        if (b.px[i] > 1.0f) { b.px[i] = 1.0f; b.vx[i] = 0.0f; b.hit[i] = kHitBounce; }
    }
}

void test_collisions() {
    ParticleSystem sys;
    sys.add_ground(0.0);
    EmitterSettings s = plain();
    s.gravity_scale = 1.0f;
    s.speed = { 0.0f, 0.0f };
    s.lifetime = { 5.0f, 5.0f };
    s.collide = true;
    s.bounce = 0.5f;
    s.friction = 0.0f;
    s.collision_radius = 0.0f;
    Emitter& e = sys.create_emitter(s, { 0.0, 2.0, 0.0 });
    sys.step(kDt);
    e.burst(10);
    float min_y = 10.0f, max_up = 0.0f;
    for (int i = 0; i < 120; ++i) {
        sys.step(kDt);
        const ParticleView v = e.view();
        for (std::size_t k = 0; k < v.count; ++k) { min_y = std::fmin(min_y, v.py[k]); max_up = std::fmax(max_up, v.vy[k]); }
    }
    check(min_y >= -1e-4f && max_up > 2.0f, "collision: ground plane bounces (" + num(min_y) + ", " + num(max_up) + ")");

    s.kill_on_collision = true;
    Emitter& k = sys.create_emitter(s, { 5.0, 1.0, 0.0 });
    sys.step(kDt);
    k.burst(10);
    for (int i = 0; i < 60; ++i) sys.step(kDt);
    check(k.count() == 0 && k.died_total() == 10, "collision: kill on hit");

    ParticleSystem sys2;
    sys2.add_sphere({ 0.0, 0.0, 0.0 }, 1.0);
    sys2.add_box({ 3.0, -1.0, -1.0 }, { 5.0, 1.0, 1.0 });
    EmitterSettings s2 = plain();
    s2.speed = { 2.0f, 2.0f };
    s2.direction = { -1.0f, 0.0f, 0.0f };
    s2.collide = true;
    s2.bounce = 1.0f;
    s2.friction = 0.0f;
    s2.collision_radius = 0.0f;
    s2.lifetime = { 3.0f, 3.0f };
    Emitter& se = sys2.create_emitter(s2, { 2.0, 0.0, 0.0 });
    sys2.step(kDt);
    se.burst(1);
    float closest = 10.0f;
    for (int i = 0; i < 90; ++i) { sys2.step(kDt); closest = std::fmin(closest, se.view().px[0]); }
    check(closest >= 0.99f && se.view().vx[0] > 1.9f, "collision: sphere reflects (" + num(closest) + ")");
    EmitterSettings s3 = s2;
    s3.direction = { 1.0f, 0.0f, 0.0f };
    Emitter& be = sys2.create_emitter(s3, { 2.0, 0.5, 0.0 });
    sys2.step(kDt);
    be.burst(1);
    float far_x = 0.0f;
    for (int i = 0; i < 90; ++i) { sys2.step(kDt); far_x = std::fmax(far_x, be.view().px[0]); }
    check(far_x <= 3.001f && be.view().vx[0] < -1.9f, "collision: box reflects (" + num(far_x) + ")");

    EmitterSettings s4 = plain();
    s4.speed = { 3.0f, 3.0f };
    s4.direction = { 1.0f, 0.0f, 0.0f };
    s4.lifetime = { 5.0f, 5.0f };
    Emitter cb(s4);
    int calls = 0;
    cb.set_collision_callback(&collide_cb, &calls);
    cb.burst(5);
    run(cb, 1.0f);
    check(calls > 0 && cb.view().px[0] == 1.0f && cb.view().vx[0] == 0.0f, "collision: user callback per batch");

    EmitterSettings s5 = s4;
    s5.kill_on_collision = true;
    Emitter cbk(s5);
    cbk.set_collision_callback(&collide_cb, &calls);
    cbk.burst(5);
    run(cbk, 1.0f);
    check(cbk.count() == 0, "collision: callback hits kill when kill_on_collision");
}

void test_sub_emitters() {
    EmitterSettings parent = plain();
    parent.lifetime = { 0.5f, 0.5f };
    SubEmitter sub;
    sub.trigger = SubEmitterTrigger::Death;
    sub.count = 3;
    sub.settings = plain();
    sub.settings.lifetime = { 5.0f, 5.0f };
    sub.settings.rate = 1000.0f;
    parent.sub_emitters.push_back(sub);
    Emitter e(parent);
    check(e.child_count() == 1 && !e.child(0)->playing(), "sub: child created and event driven");
    e.burst(10);
    run(e, 0.3f);
    check(e.child(0)->count() == 0, "sub: nothing before death");
    run(e, 0.3f);
    check(e.count() == 0 && e.child(0)->count() == 30 && e.total_count() == 30, "sub: death spawns count per particle (" + num(e.child(0)->count()) + ")");
    check(!e.finished(), "sub: emitter not finished while children live");

    ParticleSystem sys;
    sys.add_ground(0.0);
    EmitterSettings sparks = preset(ParticlePreset::Sparks);
    sparks.sub_emitters[0].probability = 1.0f;
    sparks.direction = { 0.0f, -1.0f, 0.0f };
    Emitter& sp = sys.create_emitter(sparks, { 0.0, 0.5, 0.0 });
    std::size_t peak_child = 0;
    for (int i = 0; i < 90; ++i) { sys.step(kDt); peak_child = (std::max)(peak_child, sp.child(0)->count()); }
    check(peak_child > 5, "sub: sparks spawn smoke on collision (" + num(peak_child) + ")");

    EmitterSettings ex = preset(ParticlePreset::Explosion);
    Emitter& boom = sys.create_emitter(ex, { 20.0, 5.0, 0.0 });
    sys.step(kDt);
    sys.step(kDt);
    check(boom.child(1)->count() > 0, "sub: birth trigger (explosion sparks)");
    for (int i = 0; i < 180; ++i) sys.step(kDt);
    check(boom.child(0)->spawned_total() > 0, "sub: death trigger (explosion smoke)");
}

void test_budget() {
    SystemSettings cfg;
    cfg.budget = 5000;
    ParticleSystem sys(cfg);
    EmitterSettings s = plain();
    s.rate = 3000.0f;
    s.lifetime = { 2.0f, 2.0f };
    s.max_particles = 8000;
    std::vector<Emitter*> lows;
    for (int i = 0; i < 6; ++i) lows.push_back(&sys.create_emitter(s, { i * 1.0, 0.0, 0.0 }));
    s.priority = 10;
    s.rate = 1000.0f;
    Emitter& vip = sys.create_emitter(s, { 0.0, 0.0, 5.0 });
    std::size_t peak = 0;
    for (int i = 0; i < 300; ++i) { sys.step(kDt); peak = (std::max)(peak, sys.particle_count()); }
    check(peak <= cfg.budget, "budget: total never exceeds the hard budget (" + num(peak) + ")");
    check(peak >= cfg.budget * 8 / 10, "budget: budget is used (" + num(peak) + ")");
    check(vip.count() >= 1800, "budget: high priority emitter keeps its rate (" + num(vip.count()) + ")");
    std::size_t low_total = 0;
    for (Emitter* e : lows) low_total += e->count();
    check(low_total < 6u * 6000u / 4u && sys.stats().budget_scale < 1.0f, "budget: low priority emitters are scaled down (" + num(low_total) + ")");

    SystemSettings fast;
    fast.budget = 200000;
    fast.adaptive = true;
    fast.target_ms = 0.0001f;
    ParticleSystem ad(fast);
    EmitterSettings big = plain();
    big.rate = 20000.0f;
    big.max_particles = 100000;
    big.lifetime = { 3.0f, 3.0f };
    ad.create_emitter(big);
    for (int i = 0; i < 120; ++i) ad.step(kDt);
    check(ad.quality() < 0.5f && ad.particle_count() < 100000, "budget: adaptive quality lowers the budget when over time (" + num(ad.quality()) + ")");

    ParticleSystem fx_sys(cfg);
    const int fx = fx_sys.register_effect(preset(ParticlePreset::Impact), 4);
    check(fx_sys.effect_pool_size(fx) == 4, "pool: preallocated effects");
    for (int i = 0; i < 4; ++i) fx_sys.spawn_effect(fx, { i * 1.0, 0.0, 0.0 });
    check(fx_sys.effect_pool_size(fx) == 0 && fx_sys.emitter_count() == 4, "pool: spawn takes from the pool");
    for (int i = 0; i < 180; ++i) fx_sys.step(kDt);
    check(fx_sys.effect_pool_size(fx) == 4 && fx_sys.emitter_count() == 0, "pool: finished effects return to the pool");
}

void test_lod_and_culling() {
    SystemSettings cfg;
    ParticleSystem sys(cfg);
    EmitterSettings s = plain();
    s.rate = 600.0f;
    s.lifetime = { 1.0f, 1.0f };
    Emitter& near_e = sys.create_emitter(s, { 0.0, 0.0, -5.0 });
    Emitter& mid_e = sys.create_emitter(s, { 0.0, 0.0, -70.0 });
    Emitter& far_e = sys.create_emitter(s, { 0.0, 0.0, -150.0 });
    Emitter& gone = sys.create_emitter(s, { 0.0, 0.0, -400.0 });
    Emitter& behind = sys.create_emitter(s, { 0.0, 0.0, 30.0 });
    graphics::Camera3D cam({ 0.0, 0.0, 0.0 }, 1280, 720, 70.0, 0.05, 1000.0);
    cam.look_in({ 0.0, 0.0, -1.0 });
    const CameraView view = CameraView::from_camera(cam);
    for (int i = 0; i < 120; ++i) sys.step(kDt, view);
    check(near_e.lod_level() == 0 && mid_e.lod_level() == 2 && far_e.lod_level() == 3, "lod: levels from distance (" + num(mid_e.lod_level()) + "," + num(far_e.lod_level()) + ")");
    check(near_e.count() > mid_e.count() && mid_e.count() > far_e.count(), "lod: fewer spawns far away (" + num(near_e.count()) + "," + num(mid_e.count()) + "," + num(far_e.count()) + ")");
    check(far_e.simple_render() && !near_e.simple_render(), "lod: simple render far away");
    check(!gone.visible() && gone.count() == 0, "cull: beyond cull distance paused with nothing spawned");
    check(!behind.visible() && behind.count() == 0 && sys.stats().paused >= 2, "cull: offscreen looping emitter paused");
    check(near_e.visible(), "cull: visible emitter stays visible");

    cam.look_in({ 0.0, 0.0, 1.0 });
    const CameraView turned = CameraView::from_camera(cam);
    sys.step(kDt, turned);
    check(behind.visible() && behind.count() > 300, "cull: catch-up warms an emitter that comes into view (" + num(behind.count()) + ")");
    const std::size_t frozen = near_e.count();
    for (int i = 0; i < 30; ++i) sys.step(kDt, turned);
    check(!near_e.visible() && near_e.count() == frozen, "cull: emitter behind the camera keeps its state while paused");

    ParticleSystem sys2;
    EmitterSettings one = plain();
    one.loop = false;
    one.duration = 0.1f;
    one.bursts = { { 0.0f, 100, 1, 1.0f, 1.0f } };
    one.lifetime = { 0.5f, 0.5f };
    Emitter& off = sys2.create_emitter(one, { 0.0, 0.0, 50.0 });
    for (int i = 0; i < 60; ++i) sys2.step(kDt, view);
    check(off.finished() && off.spawned_total() < 100 && off.spawned_total() > 0, "cull: offscreen one-shot is throttled but still finishes (" + num(off.spawned_total()) + ")");

    ParticleSystem s2d;
    EmitterSettings p2 = to_2d(plain(), 1.0f);
    p2.rate = 100.0f;
    Emitter& in2 = s2d.create_emitter(p2, { 100.0, 100.0, 0.0 });
    Emitter& out2 = s2d.create_emitter(p2, { 5000.0, 100.0, 0.0 });
    const CameraView rect = CameraView::from_rect(0.0, 0.0, 800.0, 600.0);
    for (int i = 0; i < 60; ++i) s2d.step(kDt, rect);
    check(in2.visible() && !out2.visible() && in2.count() > 0 && out2.count() == 0, "cull: 2d view rect");

    ParticleSystem sl;
    EmitterSettings st = plain();
    st.rate = 0.0f;
    st.auto_play = false;
    sl.create_emitter(st);
    for (int i = 0; i < 5; ++i) sl.step(kDt);
    check(sl.stats().sleeping == 1 && sl.stats().updated == 0, "sleep: idle emitters cost nothing");

    SystemSettings fixed;
    fixed.fixed_step = 1.0 / 60.0;
    fixed.max_substeps = 3;
    ParticleSystem fs(fixed);
    fs.create_emitter(plain());
    fs.step(1.0 / 30.0);
    check(fs.stats().substeps == 2, "fixed: two substeps for a 30 Hz frame");
    fs.step(0.2);
    check(fs.stats().substeps == 3, "fixed: substeps are capped");
    fs.step(1.0 / 240.0);
    check(fs.stats().substeps == 0 && fs.render_ahead() > 0.0, "fixed: sub-frame steps accumulate and extrapolate");
}

std::vector<float> snapshot(const ParticleSystem& sys) {
    std::vector<float> out;
    struct Walk {
        static void go(const Emitter& e, std::vector<float>& out) {
            const ParticleView v = e.view();
            out.push_back(static_cast<float>(v.count));
            out.insert(out.end(), v.px, v.px + v.count);
            out.insert(out.end(), v.py, v.py + v.count);
            out.insert(out.end(), v.pz, v.pz + v.count);
            out.insert(out.end(), v.vx, v.vx + v.count);
            for (std::size_t i = 0; i < e.child_count(); ++i) go(*e.child(i), out);
        }
    };
    for (std::size_t i = 0; i < sys.root_count(); ++i) Walk::go(*sys.root(i), out);
    return out;
}

std::vector<float> scenario(int threads, std::uint32_t seed) {
    SystemSettings cfg;
    cfg.threads = threads;
    cfg.seed = seed;
    cfg.budget = 30000;
    ParticleSystem sys(cfg);
    sys.add_ground(0.0);
    EmitterSettings big = preset(ParticlePreset::Smoke);
    big.max_particles = 20000;
    big.rate = 8000.0f;
    big.seed = 0;
    sys.create_emitter(big, { 0.0, 1.0, 0.0 });
    sys.create_emitter(preset(ParticlePreset::Fire), { 2.0, 0.0, 0.0 });
    sys.create_emitter(preset(ParticlePreset::Rain), { 0.0, 10.0, 0.0 });
    const int fx = sys.register_effect(preset(ParticlePreset::Sparks));
    for (int i = 0; i < 90; ++i) {
        if (i % 15 == 0) sys.spawn_effect(fx, { i * 0.1, 1.0, 0.0 });
        sys.step(kDt);
    }
    return snapshot(sys);
}

void test_determinism() {
    const std::vector<float> a = scenario(1, 42);
    const std::vector<float> b = scenario(1, 42);
    const std::vector<float> c = scenario(2, 42);
    const std::vector<float> d = scenario(1, 43);
    check(a.size() > 1000, "determinism: scenario has particles (" + num(a.size()) + ")");
    check(a == b, "determinism: same seed gives the same particles");
    check(a.size() == c.size() && std::memcmp(a.data(), c.data(), a.size() * sizeof(float)) == 0, "determinism: bit identical for 1 and 2 threads");
    check(a != d, "determinism: different seed differs");
}

void test_draw_lists() {
    ParticleSystem sys;
    EmitterSettings bb = plain();
    bb.lifetime = { 5.0f, 5.0f };
    bb.blend = ParticleBlend::Additive;
    Emitter& inst = sys.create_emitter(bb, { 0.0, 0.0, -5.0 });
    EmitterSettings rot = bb;
    rot.blend = ParticleBlend::Alpha;
    rot.rotation = { 0.0f, 360.0f };
    rot.shape = EmissionShape::Box;
    rot.shape_size = { 2.0f, 2.0f, 6.0f };
    rot.frames_x = 4;
    rot.frames_y = 2;
    rot.flipbook = FlipbookMode::Random;
    Emitter& tri = sys.create_emitter(rot, { 0.0, 0.0, -10.0 });
    EmitterSettings st = bb;
    st.render_mode = RenderMode::Stretched;
    st.stretch = 0.1f;
    st.speed = { 5.0f, 5.0f };
    Emitter& str = sys.create_emitter(st, { 3.0, 0.0, -8.0 });
    sys.step(kDt);
    inst.burst(100);
    tri.burst(200);
    str.burst(10);
    graphics::Camera3D cam({ 0.0, 0.0, 0.0 }, 1280, 720, 70.0, 0.05, 1000.0);
    cam.look_in({ 0.0, 0.0, -1.0 });
    const CameraView view = CameraView::from_camera(cam);
    sys.step(kDt, view);
    DrawList3D list;
    build_draw_list(sys, view, list);
    const DrawBatch3D* bi = nullptr;
    const DrawBatch3D* bt = nullptr;
    const DrawBatch3D* bs = nullptr;
    for (const DrawBatch3D& b : list.batches) {
        if (b.emitter == &inst) bi = &b;
        if (b.emitter == &tri) bt = &b;
        if (b.emitter == &str) bs = &b;
    }
    check(list.batches.size() == 3 && bi && bt && bs, "draw: one batch per emitter");
    check(bi && bi->path == DrawPath::Instanced && bi->count == 100 && !bi->sorted && list.billboard.vertices().size() == 4, "draw: plain billboards use one instanced draw");
    check(bt && bt->path == DrawPath::Triangles && bt->count == 1200 && bt->sorted, "draw: rotated flipbook uses triangles and is sorted");
    check(bs && bs->path == DrawPath::Triangles && bs->count == 60, "draw: stretched billboards");
    bool back_to_front = bt != nullptr;
    float prev = 1e9f;
    for (std::size_t k = 0; bt && k < bt->count; k += 6) {
        const graphics::Vertex3D* v = list.vertices.data() + bt->first + k;
        const float cz = (v[0].z + v[1].z + v[2].z + v[5].z) * 0.25f;
        const float depth = -cz;
        back_to_front = back_to_front && depth <= prev + 0.01f;
        prev = depth;
    }
    check(back_to_front, "draw: alpha particles sorted back to front");
    bool uv_ok = true;
    for (std::size_t k = 0; bt && k < bt->count; ++k) {
        const graphics::Vertex3D& v = list.vertices[bt->first + k];
        const float fu = v.u * 4.0f, fv = v.v * 2.0f;
        uv_ok = uv_ok && std::fabs(fu - std::round(fu)) < 1e-4f && std::fabs(fv - std::round(fv)) < 1e-4f;
    }
    check(uv_ok, "draw: flipbook uv on frame grid");
    float len = 0.0f;
    if (bs) {
        const graphics::Vertex3D* v = list.vertices.data() + bs->first;
        len = std::sqrt((v[2].x - v[1].x) * (v[2].x - v[1].x) + (v[2].y - v[1].y) * (v[2].y - v[1].y) + (v[2].z - v[1].z) * (v[2].z - v[1].z));
    }
    check(len > 0.5f, "draw: stretched along velocity (" + num(len) + ")");
    const graphics::Material3D mat = particle_material(*bi);
    check(mat.blend == graphics::Blend3D::Additive && !mat.depth_write && mat.texture && mat.cull == graphics::Cull3D::None, "draw: material");

    DrawOptions so;
    so.sprites = true;
    DrawList3D sl;
    build_draw_list(sys, view, sl, so);
    check(sl.sprites.size() == 310 && sl.vertices.empty() && sl.instances.empty(), "draw: engine sprite output");

    ParticleSystem tiny;
    EmitterSettings t = plain();
    t.size = { 0.001f, 0.001f };
    t.lifetime = { 5.0f, 5.0f };
    Emitter& te = tiny.create_emitter(t, { 0.0, 0.0, -50.0 });
    tiny.step(kDt);
    te.burst(50);
    DrawList3D tl;
    build_draw_list(tiny, view, tl);
    check(tl.particles == 0 && tl.culled == 50, "draw: sub-pixel particles skipped");

    ParticleSystem nf;
    EmitterSettings f = plain();
    f.near_fade = 1.0f;
    f.lifetime = { 5.0f, 5.0f };
    f.alpha_over_life = LifeCurve::constant(1.0f);
    Emitter& fe = nf.create_emitter(f, { 0.0, 0.0, -0.55 });
    nf.step(kDt);
    fe.burst(1);
    DrawList3D fl;
    build_draw_list(nf, view, fl);
    check(fl.instances.size() == 1 && (fl.instances[0].rgba >> 24) < 160 && (fl.instances[0].rgba >> 24) > 90, "draw: near fade (" + num(fl.instances[0].rgba >> 24) + ")");

    ParticleSystem s2;
    EmitterSettings p2 = preset(ParticlePreset::Fire, ParticleDim::D2);
    Emitter& e2 = s2.create_emitter(p2, { 400.0, 300.0, 0.0 });
    for (int i = 0; i < 30; ++i) s2.step(kDt, CameraView::from_rect(0, 0, 800, 600));
    DrawList2D l2;
    build_draw_list_2d(s2, CameraView::from_rect(0, 0, 800, 600), l2);
    check(l2.batches.size() == 1 && l2.vertices.size() == l2.particles * 6 && l2.particles > 5 && l2.batches[0].mode == graphics::BlendMode::Add, "draw: 2d vertex list (" + num(l2.particles) + ")");
    bool near_emitter = true;
    for (const graphics::Vertex2D& v : l2.vertices) near_emitter = near_emitter && std::fabs(v.x - 400.0f) < 300.0f && v.y < 400.0f;
    check(near_emitter && e2.count() >= l2.particles, "draw: 2d vertices around the emitter, fire rises (y down)");
}

void test_zero_allocations() {
    SystemSettings cfg;
    cfg.threads = 2;
    cfg.budget = 20000;
    ParticleSystem sys(cfg);
    sys.add_ground(0.0);
    sys.create_emitter(preset(ParticlePreset::Smoke), { 0.0, 1.0, -5.0 });
    sys.create_emitter(preset(ParticlePreset::Rain), { 0.0, 10.0, -5.0 });
    sys.create_emitter(preset(ParticlePreset::Fire), { 1.0, 0.0, -5.0 });
    const int fx = sys.register_effect(preset(ParticlePreset::Explosion), 8);
    graphics::Camera3D cam({ 0.0, 2.0, 0.0 }, 1280, 720, 70.0, 0.05, 1000.0);
    cam.look_in({ 0.0, 0.0, -1.0 });
    const CameraView view = CameraView::from_camera(cam);
    DrawList3D list;
    list.reserve(30000);
    auto frame = [&](int i) {
        if (i % 20 == 0) sys.spawn_effect(fx, { (i % 7) * 1.0, 2.0, -6.0 });
        sys.step(kDt, view);
        build_draw_list(sys, view, list);
    };
    for (int i = 0; i < 600; ++i) frame(i);
    const long long before = g_allocs.load();
    for (int i = 600; i < 900; ++i) frame(i);
    const long long allocs = g_allocs.load() - before;
    check(allocs == 0, "alloc: zero allocations per frame in steady state (" + num(static_cast<double>(allocs)) + ")");
}

void test_old_emitter() {
    graphics::ParticleConfig cfg;
    cfg.rate = 100.0f;
    graphics::ParticleEmitter e(cfg);
    for (int i = 0; i < 30; ++i) e.update(kDt);
    check(e.count() >= 49 && e.count() <= 51 && e.config().rate == 100.0f, "legacy: graphics::ParticleEmitter still works (" + num(e.count()) + ")");
}

double time_steps(ParticleSystem& sys, int steps, const CameraView& view) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < steps; ++i) sys.step(kDt, view);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / steps;
}

void bench_count(std::size_t target, int threads, const CameraView& view) {
    SystemSettings cfg;
    cfg.threads = threads;
    cfg.budget = target * 13 / 10 + 1000;
    ParticleSystem sys(cfg);
    EmitterSettings s = plain();
    s.max_particles = static_cast<std::uint32_t>(target / 4 + 100);
    s.lifetime = { 2.0f, 2.0f };
    s.rate = static_cast<float>(target / 4) / 2.0f;
    s.speed = { 0.5f, 2.0f };
    s.spread = 40.0f;
    s.gravity_scale = 0.2f;
    s.drag = 0.3f;
    s.turbulence = 0.5f;
    s.size_over_life = LifeCurve::linear(0.5f, 1.5f);
    for (int i = 0; i < 4; ++i) sys.create_emitter(s, { i * 3.0, 0.0, -20.0 });
    for (int i = 0; i < 150; ++i) sys.step(kDt, view);
    const double ms = time_steps(sys, 120, view);
    DrawList3D list;
    list.reserve(target + 1000);
    build_draw_list(sys, view, list);
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) build_draw_list(sys, view, list);
    const double dms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 20.0;
    std::printf("bench: %7zu particles, %d thread(s): step %.3f ms, draw list %.3f ms (%.1f ns/particle step)\n",
                sys.particle_count(), threads, ms, dms, ms * 1e6 / static_cast<double>(sys.particle_count() ? sys.particle_count() : 1));
    check(sys.particle_count() > target * 9 / 10, "bench: reached " + num(static_cast<double>(target)) + " particles");
}

void benchmark() {
    graphics::Camera3D cam({ 0.0, 0.0, 0.0 }, 1280, 720, 70.0, 0.05, 1000.0);
    cam.look_in({ 0.0, 0.0, -1.0 });
    const CameraView view = CameraView::from_camera(cam);
    bench_count(50000, 1, view);
    bench_count(200000, 1, view);
    bench_count(200000, 2, view);

    SystemSettings cfg;
    cfg.budget = 100000;
    ParticleSystem sys(cfg);
    EmitterSettings s = plain();
    s.max_particles = 100000;
    s.lifetime = { 2.0f, 2.0f };
    s.rate = 50000.0f;
    s.speed = { 0.5f, 2.0f };
    s.spread = 40.0f;
    s.turbulence = 0.5f;
    for (int i = 0; i < 20; ++i) sys.create_emitter(s, { i * 2.0, 0.0, -20.0 });
    for (int i = 0; i < 150; ++i) sys.step(kDt, view);
    const double ms = time_steps(sys, 60, view);
    std::printf("bench: 20 emitters asking for 2M particles, budget 100k: %zu live, step %.3f ms, budget scale %.3f\n", sys.particle_count(), ms, sys.stats().budget_scale);
    check(sys.particle_count() <= cfg.budget, "bench: over-subscribed scene stays within budget");
}

} // namespace

int main(int argc, char** argv) {
    const bool bench = !(argc > 1 && std::strcmp(argv[1], "--no-bench") == 0);
    test_text_round_trip();
    test_curves();
    test_emission_counts();
    test_lifetimes();
    test_motion();
    test_shapes();
    test_collisions();
    test_sub_emitters();
    test_budget();
    test_lod_and_culling();
    test_determinism();
    test_draw_lists();
    test_zero_allocations();
    test_old_emitter();
    if (bench) benchmark();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
