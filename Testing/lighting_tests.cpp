#include "../Includes/lighting.hpp"
#include "../Source/Lighting/integrator.hpp"
#include "../Source/Lighting/photons.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace fizmo::lighting;

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

bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

std::string str(const Rgb& c) {
    return "(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
}

LightingSettings quiet(LightingQuality q) {
    LightingSettings s = LightingSettings::preset(q);
    s.threads = 2;
    s.fog.enabled = false;
    return s;
}

void test_color() {
    const Rgb d65 = Rgb::from_kelvin(6504.0f);
    check(near(d65.r, 1.0, 0.12) && near(d65.g, 1.0, 0.12) && near(d65.b, 1.0, 0.15), "color: 6504 K is close to white " + str(d65));
    const Rgb warm = Rgb::from_kelvin(2000.0f);
    check(warm.r > warm.g && warm.g > warm.b, "color: 2000 K is orange " + str(warm));
    check(near(Rgb(0.5f).luminance(), 0.5, 1e-6), "color: luminance of grey");
    check(near(linear_to_srgb(srgb_to_linear(0.4f)), 0.4, 1e-5), "color: sRGB round trip");
    const Rgb red = Rgb::from_wavelength(650.0f), blue = Rgb::from_wavelength(450.0f);
    check(red.r > red.b && blue.b > blue.r, "color: 650 nm is red and 450 nm is blue");
}

void test_spectral_round_trip() {
    const Rgb colors[] = { Rgb(1.0f), Rgb(0.8f, 0.2f, 0.1f), Rgb(0.1f, 0.6f, 0.3f), Rgb(0.2f, 0.3f, 0.9f) };
    for (const Rgb& c : colors) {
        Rgb sum;
        const int n = 200000;
        for (int i = 0; i < n; ++i) {
            const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
            float pdf = 1.0f;
            const float nm = spectral::sample_wavelength(u, pdf);
            sum += spectral::wavelength_weight(nm) * (spectral::uplift(c, nm) / pdf);
        }
        sum = sum / static_cast<float>(n);
        check(near(sum.r, c.r, 0.02) && near(sum.g, c.g, 0.02) && near(sum.b, c.b, 0.02), "spectral: RGB survives the trip through wavelengths " + str(c) + " -> " + str(sum));
    }
}

void test_units() {
    check(near(units::lumens_to_candela(4.0f * kPi), 1.0, 1e-5), "units: 4 pi lumens from a point light is 1 candela");
    check(near(units::candela_to_lumens(1.0f, 90.0f), 2.0 * kPi, 1e-4), "units: a hemisphere spot of 1 cd is 2 pi lumens");
    check(near(units::watts_to_lumens(1.0f), 683.0, 1e-3), "units: 1 W is 683 lm");
    Light bulb = Light::point_light({ 0, 0, 0 }, Rgb(1.0f), 800.0f).with_unit(LightUnit::lumens);
    check(near(bulb.emitted_scale(), 800.0 / (4.0 * kPi), 1e-3), "units: an 800 lm bulb is 63.7 cd");
    Light panel = Light::rectangle_light({ 0, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, 1.0f, 1.0f, Rgb(1.0f), 500.0f).with_unit(LightUnit::nits);
    check(near(panel.emitted_scale(), 500.0, 1e-4), "units: nits are radiance as given");
    check(exposure_from_ev100(0.0f) > exposure_from_ev100(10.0f), "units: higher EV100 means darker exposure");
}

void test_parsing() {
    for (LightingQuality q : lighting_qualities()) {
        const auto p = parse_lighting_quality(to_string(q));
        check(p && *p == q, std::string("settings: quality round trip ") + to_string(q));
        check(LightingSettings::preset(q).quality == q, std::string("settings: preset sets quality ") + to_string(q));
    }
    for (LightShape s : { LightShape::point, LightShape::spot, LightShape::directional, LightShape::sphere, LightShape::disk, LightShape::rectangle, LightShape::tube }) {
        const auto p = parse_light_shape(to_string(s));
        check(p && *p == s, std::string("light: shape round trip ") + to_string(s));
    }
    check(parse_light_shape("sun") == LightShape::directional && parse_light_shape("area") == LightShape::rectangle && parse_light_shape("line") == LightShape::tube, "light: shape aliases");
    check(parse_light_mobility("static") == LightMobility::baked && parse_light_mobility("movable") == LightMobility::dynamic, "light: mobility aliases");
    check(parse_lighting_quality("lumen") == LightingQuality::realistic && parse_lighting_quality("voxel") == LightingQuality::simple, "settings: quality aliases");
    check(parse_global_illumination("ddgi") == GlobalIllumination::probes && parse_tone_mapper("agx") == ToneMapper::agx, "settings: gi and tone aliases");
}

void test_ies() {
    const std::string text =
        "IESNA:LM-63-2002\n[TEST] fizmo\nTILT=NONE\n"
        "1 1000 1 3 1 1 2 0 0 0\n"
        "1 1 100\n"
        "0 45 90\n"
        "0\n"
        "1000 500 0\n";
    std::string error;
    const auto p = IesProfile::parse(text, &error);
    check(p.has_value(), "ies: parses a small LM-63 file " + error);
    if (!p) return;
    check(near(p->candela(0.0f, 0.0f), 1000.0, 1e-3) && near(p->candela(22.5f, 0.0f), 750.0, 1e-3) && near(p->candela(90.0f, 123.0f), 0.0, 1e-3), "ies: interpolates candela");
    check(near(p->normalized(45.0f, 0.0f), 0.5, 1e-4), "ies: normalized by the peak");
}

void test_material() {
    const Material d = Material::diamond();
    check(near(d.ior_at(spectral::kSodiumD), 2.417, 1e-3), "material: diamond is 2.417 at the sodium line");
    check(d.ior_at(430.0f) > d.ior_at(680.0f), "material: blue bends more than red");
    const Material f = Material::flint_glass();
    const float spread = f.ior_at(spectral::kHydrogenF) - f.ior_at(spectral::kHydrogenC);
    check(near((f.ior - 1.0f) / spread, 29.0, 0.5), "material: Abbe number is honored");
    check(!Material::glass(1.5f, 0.0f).is_dispersive() && Material::crystal().is_dispersive(), "material: abbe 0 turns dispersion off");
}

Scene floor_scene() {
    Scene sc;
    sc.environment() = Environment::none();
    sc.add_object(Mesh::plane(10.0f, 10.0f), Material::diffuse(Rgb(0.8f)));
    return sc;
}

void test_bvh() {
    Scene sc = floor_scene();
    sc.add_object(Mesh::box({ 0.5f, 0.5f, 0.5f }), Material::diffuse(Rgb(0.5f)), Transform::translate({ 2.0, 0.5, 0.0 }));
    sc.commit();
    const RayHit h = sc.intersect({ 0.0, 5.0, 0.0 }, { 0.0, -1.0, 0.0 });
    check(h.hit && near(h.distance, 5.0, 1e-4) && near(h.normal.y, 1.0, 1e-4), "bvh: ray hits the floor");
    const RayHit b = sc.intersect({ 2.0, 5.0, 0.0 }, { 0.0, -1.0, 0.0 });
    check(b.hit && near(b.distance, 4.0, 1e-4) && b.instance == 1, "bvh: ray hits the box first");
    check(sc.occluded({ 2.0, 3.0, 0.0 }, { 2.0, 0.1, 0.0 }) && !sc.occluded({ 0.0, 3.0, 0.0 }, { 0.0, 0.1, 0.0 }), "bvh: occlusion");
    Scene big;
    const std::size_t mesh = big.add_mesh(Mesh::sphere(0.2f, 16, 8));
    const std::size_t mat = big.add_material(Material::diffuse(Rgb(0.5f)));
    for (int i = 0; i < 400; ++i) big.add_instance(mesh, mat, Transform::translate({ static_cast<double>(i % 20), 0.0, static_cast<double>(i / 20) }));
    big.commit();
    const RayHit far = big.intersect({ 19.0, 5.0, 19.0 }, { 0.0, -1.0, 0.0 });
    check(far.hit && near(far.distance, 4.8, 1e-3) && far.instance == 399, "bvh: finds the right sphere among 400");
}

void test_moving_layer() {
    Scene sc = floor_scene();
    const std::size_t box = sc.add_object(Mesh::box({ 0.5f, 0.5f, 0.5f }), Material::diffuse(Rgb(0.5f)), Transform::translate({ 2.0, 0.5, 0.0 }));
    sc.commit();
    const std::uint64_t before = sc.geometry_version();
    check(sc.move_instance(box, Transform::translate({ -3.0, 0.5, 1.0 })) && sc.moved_instances() == 1, "moving layer: move_instance queues one instance");
    sc.commit();
    check(sc.moved_instances() == 0 && sc.geometry_version() > before, "moving layer: commit refits and bumps the geometry version");
    const RayHit gone = sc.intersect({ 2.0, 5.0, 0.0 }, { 0.0, -1.0, 0.0 });
    check(gone.hit && near(gone.distance, 5.0, 1e-4), "moving layer: the old spot is empty after the refit");
    const RayHit moved = sc.intersect({ -3.0, 5.0, 1.0 }, { 0.0, -1.0, 0.0 });
    check(moved.hit && near(moved.distance, 4.0, 1e-4) && moved.instance == box, "moving layer: the box is found at its new spot");
    Scene fresh = floor_scene();
    fresh.add_object(Mesh::box({ 0.5f, 0.5f, 0.5f }), Material::diffuse(Rgb(0.5f)), Transform::translate({ -3.0, 0.5, 1.0 }));
    fresh.commit();
    bool same = true;
    for (int i = 0; i < 64; ++i) {
        const Vec3 o{ -6.0 + 0.19 * i, 3.0, -2.0 + 0.07 * i };
        const RayHit a = sc.intersect(o, { 0.1, -1.0, 0.05 });
        const RayHit b2 = fresh.intersect(o, { 0.1, -1.0, 0.05 });
        if (a.hit != b2.hit || (a.hit && !near(a.distance, b2.distance, 1e-4))) same = false;
    }
    check(same, "moving layer: a refit scene matches a full rebuild ray for ray");
    for (int k = 0; k < 300; ++k) {
        sc.move_instance(box, Transform::translate({ -3.0 + 0.01 * k, 0.5, 1.0 }));
        sc.commit();
    }
    const RayHit last = sc.intersect({ -0.01, 5.0, 1.0 }, { 0.0, -1.0, 0.0 });
    check(last.hit && near(last.distance, 4.0, 1e-4), "moving layer: many refits (with periodic rebuilds) stay correct");
}

double irradiance(Scene& sc, const Light& l, LightingQuality q, const Vec3& at = { 0.0, 0.0, 0.0 }, int samples = 64) {
    sc.lights().clear();
    sc.add_light(l);
    Renderer r(quiet(q));
    return r.irradiance(sc, at, { 0.0, 1.0, 0.0 }, samples).luminance();
}

void test_light_shapes() {
    Scene sc = floor_scene();
    const double point = irradiance(sc, Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f), LightingQuality::shadowed);
    check(near(point, 1.0, 1e-3), "lights: 16 cd at 4 m gives 1 lux (" + std::to_string(point) + ")");
    const double sphere = irradiance(sc, Light::sphere_light({ 0, 4, 0 }, 0.2f, Rgb(1.0f), 16.0f), LightingQuality::shadowed, { 0, 0, 0 }, 256);
    check(near(sphere, 1.0, 0.05), "lights: a small sphere light matches the point light (" + std::to_string(sphere) + ")");
    const double disk = irradiance(sc, Light::disk_light({ 0, 4, 0 }, { 0, -1, 0 }, 0.2f, Rgb(1.0f), 16.0f), LightingQuality::shadowed, { 0, 0, 0 }, 256);
    check(near(disk, 1.0, 0.05), "lights: a small disk facing down matches (" + std::to_string(disk) + ")");
    const double rect = irradiance(sc, Light::rectangle_light({ 0, 4, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, 0.3f, 0.3f, Rgb(1.0f), 16.0f), LightingQuality::shadowed, { 0, 0, 0 }, 256);
    check(near(rect, 1.0, 0.05), "lights: a small rectangle facing down matches (" + std::to_string(rect) + ")");
    const double back = irradiance(sc, Light::rectangle_light({ 0, 4, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, 0.3f, 0.3f, Rgb(1.0f), 16.0f), LightingQuality::shadowed);
    check(back < 1e-6, "lights: a one-sided rectangle facing away is dark");
    Light both = Light::rectangle_light({ 0, 4, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, 0.3f, 0.3f, Rgb(1.0f), 16.0f);
    both.two_sided = true;
    check(irradiance(sc, both, LightingQuality::shadowed, { 0, 0, 0 }, 256) > 0.9, "lights: two-sided rectangles light both ways");
    const double tube = irradiance(sc, Light::tube_light({ 0, 4, 0 }, { 1, 0, 0 }, 0.3f, 0.02f, Rgb(1.0f), 16.0f), LightingQuality::shadowed, { 0, 0, 0 }, 256);
    check(near(tube, 1.0, 0.1), "lights: a short tube matches (" + std::to_string(tube) + ")");
    const double sun = irradiance(sc, Light::sun({ 0, -1, -1 }, Rgb(1.0f), 2.0f, 0.0f), LightingQuality::shadowed);
    check(near(sun, std::sqrt(2.0), 1e-3), "lights: a 2 lux sun at 45 degrees gives 1.41 lux (" + std::to_string(sun) + ")");
    Light spot = Light::spot_light({ 0, 4, 0 }, { 0, -1, 0 }, Rgb(1.0f), 16.0f, 20.0f, 10.0f);
    check(near(irradiance(sc, spot, LightingQuality::shadowed), 1.0, 1e-3), "lights: inside the spot cone is fully lit");
    check(irradiance(sc, spot, LightingQuality::shadowed, { 3.0, 0.0, 0.0 }) < 1e-6, "lights: outside the cone is dark");
    Light ranged = Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f, 3.0f);
    check(irradiance(sc, ranged, LightingQuality::shadowed) < 1e-6, "lights: range cuts the light off");
    Light lin = Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 1.0f, 8.0f);
    lin.falloff = Falloff::linear;
    check(near(irradiance(sc, lin, LightingQuality::shadowed), 0.5, 1e-3), "lights: linear falloff halves at half range");
    Light warm = Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f).with_temperature(2700.0f);
    sc.lights().clear();
    sc.add_light(warm);
    Renderer rr(quiet(LightingQuality::shadowed));
    const Rgb e = rr.irradiance(sc, { 0, 0, 0 }, { 0, 1, 0 }, 4);
    check(e.r > e.b * 1.5f && near(e.luminance(), 1.0, 0.02), "lights: 2700 K is warm and keeps its brightness " + str(e));
    auto ies = std::make_shared<IesProfile>(IesProfile::from_table({ 0.0f, 90.0f, 180.0f }, { 0.0f }, { 0.0f, 1.0f, 1.0f }));
    Light shaped = Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f);
    shaped.direction = { 0, -1, 0 };
    shaped.ies = ies;
    check(irradiance(sc, shaped, LightingQuality::shadowed) < 1e-4, "lights: an IES profile that is dark straight down blocks the light");
}

void test_shadows_by_quality() {
    Scene sc = floor_scene();
    sc.add_object(Mesh::box({ 1.0f, 0.1f, 1.0f }), Material::diffuse(Rgb(0.5f)), Transform::translate({ 0.0, 2.0, 0.0 }));
    const Light lamp = Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f);
    const double simple = irradiance(sc, lamp, LightingQuality::simple);
    const double shadowed = irradiance(sc, lamp, LightingQuality::shadowed);
    check(simple > 0.9, "shadows: in simple, only the sun casts shadows, so the lamp shines through (" + std::to_string(simple) + ")");
    check(shadowed < 1e-4, "shadows: in shadowed, every object blocks every light (" + std::to_string(shadowed) + ")");
    const Light sun = Light::sun({ 0, -1, 0 }, Rgb(1.0f), 1.0f, 0.0f);
    check(irradiance(sc, sun, LightingQuality::simple) < 1e-4, "shadows: in simple, the sun still casts a ray-cast shadow");
    Light no_shadow = lamp;
    no_shadow.casts_shadows = false;
    check(irradiance(sc, no_shadow, LightingQuality::shadowed) > 0.9, "shadows: a light can opt out of shadows");
    Light half = lamp;
    half.shadow_strength = 0.5f;
    check(near(irradiance(sc, half, LightingQuality::shadowed), 0.5, 1e-3), "shadows: shadow strength");
    Scene glass = floor_scene();
    glass.add_object(Mesh::box({ 1.0f, 0.02f, 1.0f }), Material::glass(), Transform::translate({ 0.0, 2.0, 0.0 }));
    const double through = irradiance(glass, lamp, LightingQuality::shadowed);
    check(through > 0.85 && through < 1.0, "shadows: glass lets most light through as a soft colored shadow (" + std::to_string(through) + ")");
    Scene soft = floor_scene();
    soft.add_object(Mesh::box({ 0.05f, 0.05f, 0.05f }), Material::diffuse(Rgb(0.5f)), Transform::translate({ 0.0, 2.0, 0.0 }));
    const double big_light = irradiance(soft, Light::sphere_light({ 0, 4, 0 }, 0.5f, Rgb(1.0f), 16.0f), LightingQuality::shadowed, { 0, 0, 0 }, 512);
    check(big_light > 0.5 && big_light < 1.0, "shadows: a big light gives a penumbra instead of a hard shadow (" + std::to_string(big_light) + ")");
}

void test_channels_and_mobility() {
    Scene sc = floor_scene();
    sc.instances()[0].channels = 1u;
    Light l = Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f);
    l.channels = 2u;
    sc.add_light(l);
    Renderer r(quiet(LightingQuality::shadowed));
    LightingSettings s = quiet(LightingQuality::shadowed);
    s.samples_per_pixel = 1;
    r.set_settings(s);
    const Image& img = r.render(sc, Camera::look_at({ 0, 3, 3 }, { 0, 0, 0 }), 16, 16);
    check(img.average().max_component() < 1e-5, "channels: a light on channel 2 does not light channel 1 objects");
    sc.lights()[0].channels = 3u;
    const Image& lit = r.render(sc, Camera::look_at({ 0, 3, 3 }, { 0, 0, 0 }), 16, 16);
    check(lit.average().max_component() > 0.01, "channels: sharing a channel lights it");
}

void test_furnace() {
    Scene sc;
    sc.environment() = Environment::uniform(Rgb(1.0f));
    sc.add_object(Mesh::sphere(1.0f, 48, 24), Material::diffuse(Rgb(0.8f)));
    LightingSettings s = quiet(LightingQuality::path_traced);
    s.samples_per_pixel = 64;
    s.tone_mapper = ToneMapper::none;
    Renderer r(s);
    const Image& img = r.render(sc, Camera::look_at({ 0, 0, 4 }, { 0, 0, 0 }, { 0, 1, 0 }, 30.0f), 9, 9);
    const Rgb c = img.at(4, 4);
    check(near(c.r, 0.8, 0.05) && near(c.g, 0.8, 0.05) && near(c.b, 0.8, 0.05), "path: a diffuse sphere in a white sky reflects its albedo " + str(c));
    LightingSettings sp = s;
    sp.transport = Transport::spectral;
    sp.samples_per_pixel = 256;
    Renderer rs(sp);
    const Rgb cs = rs.render(sc, Camera::look_at({ 0, 0, 4 }, { 0, 0, 0 }, { 0, 1, 0 }, 30.0f), 9, 9).at(4, 4);
    check(near(cs.r, 0.8, 0.08) && near(cs.g, 0.8, 0.08) && near(cs.b, 0.8, 0.08), "spectral: the same sphere comes out the same color " + str(cs));
    Scene glass;
    glass.environment() = Environment::uniform(Rgb(1.0f));
    glass.add_object(Mesh::sphere(1.0f, 48, 24), Material::glass(1.5f, 0.0f));
    Renderer rg(s);
    const Rgb g = rg.render(glass, Camera::look_at({ 0, 0, 4 }, { 0, 0, 0 }, { 0, 1, 0 }, 30.0f), 9, 9).at(4, 4);
    check(near(g.r, 1.0, 0.08) && near(g.g, 1.0, 0.08), "path: clear glass in a white sky neither adds nor loses light " + str(g));
}

void test_direct_modes_render() {
    Scene sc = floor_scene();
    sc.environment() = Environment::gradient(Rgb(0.3f, 0.5f, 0.9f), Rgb(0.8f), Rgb(0.2f));
    sc.add_object(Mesh::sphere(0.5f), Material::glass(), Transform::translate({ 0.0, 0.5, 0.0 }));
    sc.add_object(Mesh::sphere(0.5f), Material::metal(Rgb(0.9f, 0.7f, 0.3f), 0.1f), Transform::translate({ 1.2, 0.5, 0.0 }));
    sc.add_light(Light::sun({ -0.3, -1, -0.4 }, Rgb(1.0f), 3.0f));
    sc.add_light(Light::rectangle_light({ 0, 3, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, 1.0f, 1.0f, Rgb(1.0f, 0.9f, 0.8f), 10.0f));
    const Camera cam = Camera::look_at({ 0, 1.5, 4 }, { 0.5, 0.4, 0 });
    for (LightingQuality q : lighting_qualities()) {
        LightingSettings s = quiet(q);
        s.samples_per_pixel = 1;
        s.photons = 20000;
        s.probe_rays = 32;
        Renderer r(s);
        const Image& img = r.render(sc, cam, 24, 16);
        bool finite = true;
        for (const Rgb& p : img.pixels()) finite = finite && p.is_finite();
        check(finite && img.average().max_component() > 0.0f, std::string("render: ") + to_string(q) + " gives a finite, non-black image");
        check(r.rgba8().size() == 24u * 16u * 4u, std::string("render: ") + to_string(q) + " gives RGBA8 output");
    }
}

void test_probes_bleed_color() {
    Scene sc;
    sc.environment() = Environment::none();
    sc.add_object(Mesh::plane(3.0f, 3.0f), Material::diffuse(Rgb(0.8f)));
    sc.add_object(Mesh::box({ 0.05f, 1.5f, 3.0f }), Material::diffuse(Rgb(0.9f, 0.05f, 0.05f)), Transform::translate({ -1.5, 1.5, 0.0 }));
    sc.add_light(Light::point_light({ -0.5, 2.5, 0.0 }, Rgb(1.0f), 10.0f));
    LightingSettings s = quiet(LightingQuality::realistic);
    s.probe_spacing = 0.5f;
    s.probe_rays = 64;
    Renderer r(s);
    r.render(sc, Camera::look_at({ 2, 2, 3 }, { 0, 0.5, 0 }), 8, 8);
    check(!r.probes().empty(), "probes: realistic quality builds a probe volume (" + std::to_string(r.probes().count()) + " probes)");
    const Rgb near_wall = r.probes().irradiance({ -1.2, 0.3, 0.0 }, { 0.0, 1.0, 0.0 });
    check(near_wall.r > near_wall.g * 1.2f, "probes: the floor next to a red wall picks up red bounced light " + str(near_wall));
}

void test_photon_caustics_and_dispersion() {
    Scene sc = floor_scene();
    sc.add_object(Mesh::sphere(0.5f, 48, 24), Material::glass(1.5f, 0.0f), Transform::translate({ 0.0, 1.5, 0.0 }));
    sc.add_light(Light::spot_light({ 0, 6, 0 }, { 0, -1, 0 }, Rgb(1.0f), 30.0f, 7.0f, 6.0f));
    sc.commit();
    fizmo::lighting::detail::Context ctx;
    ctx.c = &sc.compiled();
    ctx.scene = &sc;
    LightingSettings s = quiet(LightingQuality::photon);
    ctx.s = &s;
    fizmo::lighting::detail::build_tri_light_map(ctx);
    fizmo::lighting::detail::PhotonMap map;
    fizmo::lighting::detail::PhotonOptions po;
    po.count = 60000;
    po.radius = 0.08f;
    po.threads = 2;
    map.build(ctx, po);
    check(map.size() > 1000, "photons: light through a glass ball lands on the floor (" + std::to_string(map.size()) + " photons)");
    double center = 0.0, ring = 0.0;
    for (const auto& p : map.photons()) {
        const float d = std::sqrt(p.p.x * p.p.x + p.p.z * p.p.z);
        if (d < 0.25f) center += p.power.luminance();
        else if (d > 0.5f && d < 1.0f) ring += p.power.luminance();
    }
    const double center_density = center / (kPi * 0.25 * 0.25), ring_density = ring / (kPi * (1.0 - 0.25));
    check(center_density > ring_density * 3.0, "photons: the ball focuses light into a bright caustic under it");
    Scene prism;
    prism.environment() = Environment::none();
    prism.add_object(Mesh::plane(2.0f, 2.0f), Material::diffuse(Rgb(0.8f)));
    prism.add_object(Mesh::prism(0.5f, 0.5f), Material::flint_glass(), Transform::translate({ 0.0, 1.5, 0.0 }) * Transform::from_quaternion(0.0, 0.0, std::sin(0.5), std::cos(0.5)));
    prism.add_light(Light::sun({ 0.3, -1, 0 }, Rgb(1.0f), 5.0f, 0.0f));
    prism.commit();
    fizmo::lighting::detail::Context pc;
    pc.c = &prism.compiled();
    pc.scene = &prism;
    pc.s = &s;
    fizmo::lighting::detail::build_tri_light_map(pc);
    fizmo::lighting::detail::PhotonMap rainbow;
    po.spectral = true;
    po.bundle = 4;
    po.count = 40000;
    rainbow.build(pc, po);
    int reddish = 0, bluish = 0;
    for (const auto& p : rainbow.photons()) {
        if (p.power.r > p.power.b * 2.0f && p.power.r > 0.0f) ++reddish;
        if (p.power.b > p.power.r * 2.0f && p.power.b > 0.0f) ++bluish;
    }
    check(reddish > 50 && bluish > 50, "photons: a flint prism splits sunlight into red and blue photons (" + std::to_string(reddish) + " red, " + std::to_string(bluish) + " blue)");
}

void test_baking() {
    Scene sc;
    sc.environment() = Environment::none();
    Instance floor;
    floor.mesh = sc.add_mesh(Mesh::plane(3.0f, 3.0f));
    floor.material = sc.add_material(Material::diffuse(Rgb(0.8f)));
    floor.is_static = true;
    sc.add_instance(floor);
    Light baked = Light::point_light({ 0, 2, 0 }, Rgb(1.0f), 4.0f).with_mobility(LightMobility::baked);
    sc.add_light(baked);
    BakeOptions o;
    o.samples = 32;
    o.probe_spacing = 1.0f;
    o.probe_rays = 64;
    o.threads = 2;
    o.max_lightmap_size = 32;
    bool reported = false;
    const BakedLighting b = bake(sc, LightingSettings::shadowed(), o, [&](float) { reported = true; });
    check(b.has(0) && b.vertex[0].size() == 4 && !b.lightmaps[0].empty() && !b.volume.empty() && reported, "bake: vertices, a lightmap and probes are baked");
    const Rgb center = b.lightmaps[0].sample(0.5f, 0.5f);
    check(near(center.luminance(), 1.0, 0.1), "bake: the lightmap under a 4 cd light at 2 m holds ~1 lux (" + str(center) + ")");
    const std::string path = "fizmo_lighting_bake_test.bin";
    check(b.save(path), "bake: saves");
    const auto loaded = BakedLighting::load(path);
    std::remove(path.c_str());
    check(loaded && loaded->lightmaps.size() == b.lightmaps.size() && loaded->volume.count() == b.volume.count() && near(loaded->lightmaps[0].sample(0.5f, 0.5f).r, center.r, 1e-6), "bake: loads back the same data");
    LightingSettings s = quiet(LightingQuality::shadowed);
    s.gi = GlobalIllumination::baked;
    s.tone_mapper = ToneMapper::none;
    Renderer r(s);
    r.set_baked(std::make_shared<BakedLighting>(b));
    const Rgb seen = r.render(sc, Camera::look_at({ 0, 3, 0.01 }, { 0, 0, 0 }, { 0, 1, 0 }, 20.0f), 5, 5).at(2, 2);
    check(near(seen.luminance(), 0.8 / kPi, 0.05), "bake: a baked light lights the floor through the bake only " + str(seen));
    sc.lights()[0].mobility = LightMobility::dynamic;
    r.set_baked(nullptr);
    s.gi = GlobalIllumination::none;
    r.set_settings(s);
    const Rgb live = r.render(sc, Camera::look_at({ 0, 3, 0.01 }, { 0, 0, 0 }, { 0, 1, 0 }, 20.0f), 5, 5).at(2, 2);
    check(near(live.luminance(), seen.luminance(), 0.05), "bake: baked and dynamic light agree " + str(live));
}

void test_queries() {
    Scene sc = floor_scene();
    sc.add_object(Mesh::box({ 1.0f, 0.1f, 1.0f }), Material::diffuse(Rgb(0.5f)), Transform::translate({ 0.0, 2.0, 0.0 }));
    sc.add_light(Light::point_light({ 0, 4, 0 }, Rgb(1.0f), 16.0f));
    Renderer r(quiet(LightingQuality::shadowed));
    check(!r.lit_by(sc, { 0, 0, 0 }, 0) && r.lit_by(sc, { 3, 0, 0 }, 0), "queries: lit_by tells if a spot can see a light");
    check(r.light_level(sc, { 3, 0, 0 }, { 0, 1, 0 }) > r.light_level(sc, { 0, 0, 0 }, { 0, 1, 0 }), "queries: light level is lower in shadow");
}

void test_tone_mapping() {
    LightingSettings s;
    for (ToneMapper t : { ToneMapper::none, ToneMapper::reinhard, ToneMapper::aces, ToneMapper::filmic, ToneMapper::agx }) {
        s.tone_mapper = t;
        const Rgb lo = tone_map(Rgb(0.1f), s), hi = tone_map(Rgb(10.0f), s);
        check(lo.r < hi.r && hi.r <= 1.0f && lo.r >= 0.0f, std::string("tone: ") + to_string(t) + " is monotonic and in range");
    }
    s.tone_mapper = ToneMapper::none;
    s.exposure = 2.0f;
    check(near(tone_map(Rgb(0.25f), s).r, 0.5, 1e-5), "tone: exposure scales");
}

void write_showcase(const char* folder) {
    Scene sc;
    sc.environment() = Environment::gradient(Rgb(0.35f, 0.5f, 0.85f), Rgb(0.85f, 0.85f, 0.9f), Rgb(0.25f, 0.22f, 0.2f), 0.6f);
    sc.add_object(Mesh::plane(4.0f, 4.0f), Material::diffuse(Rgb(0.75f)));
    sc.add_object(Mesh::box({ 0.05f, 2.0f, 4.0f }), Material::diffuse(Rgb(0.75f, 0.12f, 0.1f)), Transform::translate({ -3.0, 2.0, 0.0 }));
    sc.add_object(Mesh::box({ 4.0f, 2.0f, 0.05f }), Material::diffuse(Rgb(0.7f)), Transform::translate({ 0.0, 2.0, -3.0 }));
    sc.add_object(Mesh::box({ 0.05f, 2.0f, 4.0f }), Material::diffuse(Rgb(0.12f, 0.55f, 0.15f)), Transform::translate({ 3.0, 2.0, 0.0 }));
    sc.add_object(Mesh::sphere(0.6f, 64, 32), Material::glass(1.5f, 40.0f), Transform::translate({ -1.0, 0.6, 0.6 }));
    sc.add_object(Mesh::sphere(0.55f, 64, 32), Material::metal(Rgb(0.95f, 0.75f, 0.35f), 0.12f), Transform::translate({ 1.1, 0.55, -0.6 }));
    sc.add_object(Mesh::box({ 0.4f, 0.7f, 0.4f }), Material::plastic(Rgb(0.2f, 0.35f, 0.8f), 0.4f), Transform::translate({ 0.3, 0.7, -1.8 }) * Transform::from_quaternion(0.0, std::sin(0.35), 0.0, std::cos(0.35)));
    sc.add_object(Mesh::prism(0.35f, 0.35f), Material::flint_glass(), Transform::translate({ 1.4, 0.35, 1.2 }));
    sc.add_object(Mesh::sphere(0.15f, 24, 12), Material::emissive(Rgb(1.0f, 0.35f, 0.1f), 12.0f), Transform::translate({ -2.2, 0.15, -1.8 }));
    sc.add_light(Light::rectangle_light({ 0.0, 3.95, 0.0 }, { 0, -1, 0 }, { 0, 0, 1 }, 1.4f, 1.4f, Rgb(1.0f), 18.0f).with_temperature(5200.0f));
    sc.add_light(Light::sun({ 0.5, -1.0, -0.6 }, Rgb(1.0f, 0.95f, 0.85f), 2.5f, 1.0f));
    sc.add_light(Light::spot_light({ 2.4, 3.2, 2.4 }, { -0.6, -1.0, -0.6 }, Rgb(0.6f, 0.8f, 1.0f), 25.0f, 28.0f, 18.0f));
    const Camera cam = Camera::look_at({ 0.0, 2.2, 6.2 }, { 0.0, 0.8, 0.0 }, { 0, 1, 0 }, 55.0f);
    const unsigned w = 320, h = 220;
    for (LightingQuality q : lighting_qualities()) {
        LightingSettings s = LightingSettings::preset(q);
        s.exposure = 0.75f;
        int passes = 1;
        if (q == LightingQuality::realistic) passes = 8;
        if (q == LightingQuality::path_traced) passes = 32;
        if (q == LightingQuality::photon) {
            passes = 16;
            s.photons = 400000;
        }
        Renderer r(s);
        r.render_passes(sc, cam, w, h, passes);
        const std::string path = std::string(folder) + "/" + to_string(q) + ".ppm";
        r.image().write_ppm(path, s);
        std::printf("wrote %s (%zu spp, %.0f ms last pass)\n", path.c_str(), r.stats().samples_per_pixel, r.stats().last_pass_ms);
    }
}

} // namespace

int main() {
    test_color();
    test_spectral_round_trip();
    test_units();
    test_parsing();
    test_ies();
    test_material();
    test_bvh();
    test_moving_layer();
    test_light_shapes();
    test_shadows_by_quality();
    test_channels_and_mobility();
    test_furnace();
    test_direct_modes_render();
    test_probes_bleed_color();
    test_photon_caustics_and_dispersion();
    test_baking();
    test_queries();
    test_tone_mapping();
    if (const char* folder = std::getenv("FIZMO_LIGHTING_IMAGES")) write_showcase(folder);
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
