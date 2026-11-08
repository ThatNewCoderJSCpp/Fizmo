#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include "../Includes/lighting.hpp"
#include "../Source/Graphics/Renderer/renderer.hpp"
#include "../Source/Graphics/texture_library.hpp"
#include "../Source/Graphics/gpu_path_tracer.hpp"
#include "../Source/Images/Formats/image_formats.hpp"
#include "../Source/Windows/window.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace gfx = fizmo::graphics;
namespace lt = fizmo::lighting;
namespace fs = std::filesystem;
using fizmo::colors::Color;
using fizmo::images::BitmapImage;
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

bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    return buf;
}

BitmapImage checker_image(unsigned size, unsigned cells, Color a, Color b) {
    std::vector<Color> px(static_cast<std::size_t>(size) * size);
    for (unsigned y = 0; y < size; ++y)
        for (unsigned x = 0; x < size; ++x) px[static_cast<std::size_t>(y) * size + x] = (((x * cells / size) + (y * cells / size)) & 1u) ? b : a;
    return BitmapImage(size, size, std::move(px));
}

fs::path scratch_dir() {
    const fs::path dir = fs::temp_directory_path() / ("fizmo_texture_tests_" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count() % 1000000007)));
    fs::create_directories(dir);
    return dir;
}

void test_loading_and_cache(const fs::path& dir) {
    const BitmapImage img = checker_image(8, 2, Color(200, 10, 10), Color(10, 10, 200));
    const char* formats[] = { "png", "bmp", "tga", "qoi" };
    for (const char* ext : formats) {
        const std::string path = (dir / (std::string("checker.") + ext)).string();
        std::string err;
        const bool saved = fizmo::images::save_image(path, img, fizmo::images::ImageFormat::Unknown, fizmo::images::EncodeOptions(), &err);
        check(saved, std::string("load: writes a ") + ext + " file " + err);
        gfx::TextureLibrary lib;
        auto t = lib.load(path);
        check(t && t->width() == 8 && t->height() == 8, std::string("load: reads a ") + ext + " texture");
        if (t) check(t->image()->get_pixel(0, 0).red() == 200 && t->image()->get_pixel(7, 0).blue() == 200, std::string("load: ") + ext + " pixels survive");
    }

    gfx::TextureLibrary lib;
    const std::string png = (dir / "checker.png").string();
    auto a = lib.load(png);
    auto b = lib.load((dir / "." / "checker.png").string());
    check(a && a == b, "cache: the same file and settings give the same texture");
    check(lib.stats().hits == 1 && lib.stats().misses == 1, "cache: one miss, then one hit");
    auto lin = lib.load(png, gfx::TextureRole::Normal);
    check(lin && lin != a && lib.size() == 2, "cache: different settings make a separate entry");
    check(a->color_space() == gfx::ColorSpace::SRGB && lin->color_space() == gfx::ColorSpace::Linear, "cache: colour maps are sRGB, normal maps are linear");
    check(a->mipmaps() && a->wrap() == gfx::WrapMode::Repeat && a->filter() == gfx::SampleFilter::Bilinear, "cache: default settings are trilinear and repeat");
    check(lib.contains(png) && lib.find(png) == a, "cache: find returns the cached texture");
    check(gfx::default_color_space(gfx::TextureRole::Emissive) == gfx::ColorSpace::SRGB && gfx::default_color_space(gfx::TextureRole::Roughness) == gfx::ColorSpace::Linear
        && gfx::default_color_space(gfx::TextureRole::Packed) == gfx::ColorSpace::Linear && gfx::default_color_space(gfx::TextureRole::Opacity) == gfx::ColorSpace::Linear, "cache: role colour spaces");
    auto missing = lib.load((dir / "missing.png").string());
    check(!missing && !lib.last_error().empty() && lib.stats().failures == 1, "load: a missing file fails with an error");

    auto flipped = lib.load(png, gfx::TextureSettings::color().with_flip_y(true));
    check(flipped && flipped->image()->get_pixel(0, 7) == a->image()->get_pixel(0, 0), "load: flip_y flips rows");

    auto lt_tex = lib.load_lighting(png);
    check(lt_tex && lt_tex->width() == 8 && lt_tex->color_space() == lt::ColorSpace::srgb, "cache: the lighting copy keeps size and colour space");
    check(lib.load_lighting(png) == lt_tex, "cache: the lighting copy is cached too");

    check(!lib.changed(png), "reload: an untouched file is not changed");
    const BitmapImage green = checker_image(8, 1, Color(0, 255, 0), Color(0, 255, 0));
    fizmo::images::save_image(png, green);
    std::error_code ec;
    fs::last_write_time(png, fs::last_write_time(png, ec) + std::chrono::seconds(2), ec);
    check(lib.changed(png), "reload: a rewritten file is changed");
    const std::size_t n = lib.reload_changed();
    check(n >= 2, "reload: reload_changed reloads every entry of the file (" + std::to_string(n) + ")");
    check(a->image()->get_pixel(0, 0).green() == 255 && a == lib.load(png), "reload: the texture object is kept and its pixels change");
    check(lt_tex->texel(0, 0).g > 0.99f && lt_tex->texel(0, 0).r < 0.01f, "reload: the lighting copy follows");
    check(!lib.changed(png) && lib.reload_changed() == 0, "reload: nothing left to reload");

    auto added = lib.add("memory:white", gfx::TextureLibrary::solid(Color(255, 255, 255)), gfx::TextureSettings::data());
    check(added && added->width() == 1 && lib.find("memory:white", gfx::TextureSettings::data()) == added, "cache: in-memory textures can be added");
    a.reset();
    b.reset();
    lin.reset();
    flipped.reset();
    lt_tex.reset();
    added.reset();
    const std::size_t pruned = lib.prune();
    check(pruned >= 3 && lib.size() == 0, "cache: prune drops textures nobody holds (" + std::to_string(pruned) + ")");

    const BitmapImage ao = gfx::TextureLibrary::solid(Color(10, 10, 10), 2), rough = gfx::TextureLibrary::solid(Color(20, 20, 20), 4);
    const BitmapImage orm = gfx::TextureLibrary::pack_orm(&ao, &rough, nullptr);
    check(orm.width() == 4 && orm.get_pixel(3, 3) == Color(10, 20, 255, 255), "pack_orm: packs occlusion, roughness and metallic into R, G and B");
}

void test_color_space() {
    const lt::Texture s = lt::Texture::from_rgba8(1, 1, { 128, 128, 128, 128 }, lt::ColorSpace::srgb);
    const lt::Texture l = lt::Texture::from_rgba8(1, 1, { 128, 128, 128, 128 }, lt::ColorSpace::linear);
    check(near(s.texel(0, 0).r, lt::srgb_to_linear(128.0f / 255.0f), 1e-6) && near(s.texel(0, 0).r, 0.2158, 1e-3), "colour: sRGB texels decode to linear (" + num(s.texel(0, 0).r) + ")");
    check(near(s.texel(0, 0).a, 128.0 / 255.0, 1e-6), "colour: alpha is never gamma decoded");
    check(near(l.texel(0, 0).r, 128.0 / 255.0, 1e-6), "colour: linear texels are left alone");
    check(near(s.stored(0, 0).r, 128.0 / 255.0, 1e-6), "colour: stored() is the raw value");
    check(s.has_alpha() && !lt::Texture::solid(lt::Rgba(1, 1, 1, 1)).has_alpha(), "colour: has_alpha");
    const lt::Texture f = lt::Texture::from_float(2, 1, { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }, lt::ColorSpace::linear);
    check(near(f.average().r, 0.5, 1e-6), "colour: average of a float texture");
    check(near(lt::linear_to_srgb(s.texel(0, 0).r), 128.0 / 255.0, 1e-4), "colour: decode then encode round trips");
}

void test_uv_transform() {
    const lt::UvTransform tile = lt::UvTransform::tiling(2.0f, 3.0f);
    const lt::Vec2f a = tile.apply({ 0.25f, 0.5f });
    check(near(a.x, 0.5, 1e-6) && near(a.y, 1.5, 1e-6), "uv: tiling scales");
    const lt::UvTransform rot = lt::UvTransform::make({ 1.0f, 1.0f }, { 0.0f, 0.0f }, 1.5707963f);
    const lt::Vec2f b = rot.apply({ 1.0f, 0.0f });
    check(near(b.x, 0.0, 1e-6) && near(b.y, 1.0, 1e-6), "uv: rotation turns u into v");
    const lt::UvTransform all = lt::UvTransform::make({ 2.0f, 2.0f }, { 0.1f, 0.2f }, 1.5707963f);
    const lt::Vec2f c = all.apply({ 0.5f, 0.0f });
    check(near(c.x, 0.1, 1e-6) && near(c.y, 1.2, 1e-6), "uv: scale, then rotate, then offset");
    check(lt::UvTransform().is_identity() && !tile.is_identity(), "uv: identity");

    const gfx::UvTransform g1 = gfx::UvTransform::make(2.0f, 3.0f, 0.5f, 0.25f, 0.3f);
    const gfx::UvTransform g2 = gfx::UvTransform::make(0.5f, 1.5f, -0.2f, 0.1f, -1.1f);
    float m[6], u1, v1, u2, v2;
    gfx::UvTransform::compose(g1, g2, m);
    g1.apply(0.3f, 0.7f, u1, v1);
    g2.apply(u1, v1, u2, v2);
    check(near(m[0] * 0.3 + m[1] * 0.7 + m[2], u2, 1e-5) && near(m[3] * 0.3 + m[4] * 0.7 + m[5], v2, 1e-5), "uv: composing two transforms equals applying both");
    const lt::UvTransform lg = gfx::to_lighting(g1);
    const lt::Vec2f lp = lg.apply({ 0.3f, 0.7f });
    check(near(lp.x, u1, 1e-6) && near(lp.y, v1, 1e-6), "uv: the graphics and lighting transforms agree");
}

void test_wrap_modes() {
    using W = lt::TextureWrap;
    check(lt::Texture::wrap_coord(-1, 4, W::repeat) == 3 && lt::Texture::wrap_coord(5, 4, W::repeat) == 1, "wrap: repeat");
    check(lt::Texture::wrap_coord(-1, 4, W::clamp) == 0 && lt::Texture::wrap_coord(9, 4, W::clamp) == 3, "wrap: clamp");
    check(lt::Texture::wrap_coord(-1, 4, W::mirror) == 0 && lt::Texture::wrap_coord(4, 4, W::mirror) == 3 && lt::Texture::wrap_coord(5, 4, W::mirror) == 2 && lt::Texture::wrap_coord(8, 4, W::mirror) == 0, "wrap: mirror");
    const lt::Texture ramp = lt::Texture::from_float(4, 1, { 0, 0, 0, 1, 0.25f, 0, 0, 1, 0.5f, 0, 0, 1, 0.75f, 0, 0, 1 }, lt::ColorSpace::linear);
    check(near(ramp.sample({ 1.125f, 0.5f }, lt::TextureFilter::nearest, W::repeat).r, 0.0, 1e-6), "wrap: nearest repeat lands on the first texel");
    check(near(ramp.sample({ 1.125f, 0.5f }, lt::TextureFilter::nearest, W::clamp).r, 0.75, 1e-6), "wrap: nearest clamp lands on the last texel");
    check(near(ramp.sample({ 1.125f, 0.5f }, lt::TextureFilter::nearest, W::mirror).r, 0.75, 1e-6), "wrap: nearest mirror reflects");
    check(near(ramp.sample({ 0.25f, 0.5f }, lt::TextureFilter::bilinear, W::clamp).r, 0.125, 1e-6), "filter: bilinear blends neighbouring texels");
    check(near(ramp.sample({ 0.0f, 0.5f }, lt::TextureFilter::bilinear, W::repeat).r, 0.375, 1e-6), "filter: bilinear repeat blends across the seam");
    const lt::Vec3f w = lt::triplanar_weights({ 0.0f, 1.0f, 0.0f }, 4.0f);
    const lt::Vec3f d = lt::triplanar_weights(lt::normalize({ 1.0f, 1.0f, 0.0f }), 4.0f);
    check(near(w.y, 1.0, 1e-6) && near(d.x, 0.5, 1e-6) && near(d.z, 0.0, 1e-6), "triplanar: weights follow the normal");

    gfx::Texture gt(checker_image(2, 2, Color(255, 0, 0), Color(0, 0, 255)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    check(gt.sample_wrapped(1.25, 0.25).red() == 255 && gt.sample_wrapped(1.75, 0.25).blue() == 255, "wrap: graphics textures repeat in uv space");
    gt.set_wrap(gfx::WrapMode::Clamp);
    check(gt.sample_wrapped(1.25, 0.25).blue() == 255, "wrap: graphics textures clamp in uv space");
}

lt::LightingSettings direct_settings() {
    lt::LightingSettings s = lt::LightingSettings::preset(lt::LightingQuality::shadowed);
    s.threads = 2;
    s.fog.enabled = false;
    s.gi = lt::GlobalIllumination::none;
    s.samples_per_pixel = 1;
    s.tone_mapper = lt::ToneMapper::none;
    s.firefly_clamp = 0.0f;
    return s;
}

lt::Camera top_camera(double height = 1.5) {
    return lt::Camera::look_at({ 0.0, height, 0.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, -1.0 }, 60.0f);
}

double lum(const lt::Rgb& c) {
    return c.luminance();
}

double region(const lt::Image& img, double fx, double fy, int r = 1) {
    const int cx = static_cast<int>(fx * img.width()), cy = static_cast<int>(fy * img.height());
    double sum = 0.0;
    int n = 0;
    for (int y = cy - r; y <= cy + r; ++y)
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x < 0 || y < 0 || x >= static_cast<int>(img.width()) || y >= static_cast<int>(img.height())) continue;
            sum += lum(img.at(static_cast<unsigned>(x), static_cast<unsigned>(y)));
            ++n;
        }
    return n ? sum / n : 0.0;
}

std::shared_ptr<const lt::Texture> solid_map(float r, float g, float b, float a = 1.0f) {
    return std::make_shared<lt::Texture>(lt::Texture::solid(lt::Rgba(r, g, b, a), lt::ColorSpace::linear));
}

lt::Image render_plane(const lt::Material& m, const lt::Light& light, const lt::Camera& cam, unsigned size = 32) {
    lt::Scene sc;
    sc.environment() = lt::Environment::none();
    sc.add_object(lt::Mesh::plane(1.0f, 1.0f), m);
    sc.add_light(light);
    lt::Renderer r(direct_settings());
    return r.render(sc, cam, size, size);
}

bool same_image(const lt::Image& a, const lt::Image& b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (std::size_t i = 0; i < a.pixels().size(); ++i)
        if (a.pixels()[i] != b.pixels()[i]) return false;
    return true;
}

void test_cpu_tracer() {
    const lt::Light sun = lt::Light::sun({ -1.0, -1.0, 0.0 }, lt::Rgb(1.0f), 2.0f);
    lt::Material base = lt::Material::diffuse(lt::Rgb(0.8f));

    lt::Material checker = base;
    checker.base_color = lt::Rgb(1.0f);
    checker.base_color_map = lt::TextureMap(std::make_shared<lt::Texture>(lt::Texture::checker(4, 2, lt::Rgba(1, 1, 1, 1), lt::Rgba(0.05f, 0.05f, 0.05f, 1), lt::ColorSpace::linear)));
    checker.base_color_map.filter = lt::TextureFilter::nearest;
    checker.base_color_map.wrap = lt::TextureWrap::clamp;
    const lt::Image ci = render_plane(checker, sun, top_camera());
    const double q[4] = { region(ci, 0.25, 0.25), region(ci, 0.75, 0.25), region(ci, 0.25, 0.75), region(ci, 0.75, 0.75) };
    check((q[0] > q[1] * 5.0 && q[3] > q[2] * 5.0 && near(q[0], q[3], q[0] * 0.05) && near(q[1], q[2], q[0] * 0.05)) || (q[1] > q[0] * 5.0 && q[2] > q[3] * 5.0 && near(q[1], q[2], q[1] * 0.05)),
          "cpu: a checker base-colour map renders a checker (" + num(q[0]) + " " + num(q[1]) + " " + num(q[2]) + " " + num(q[3]) + ")");

    lt::Material tiled = checker;
    tiled.uv = lt::UvTransform::tiling(2.0f, 2.0f);
    tiled.base_color_map.wrap = lt::TextureWrap::repeat;
    const lt::Image ti = render_plane(tiled, sun, top_camera());
    const double t0 = region(ti, 0.125, 0.125, 0), t1 = region(ti, 0.375, 0.125, 0);
    check((t0 > t1 * 5.0 || t1 > t0 * 5.0) && near(region(ti, 0.125, 0.125, 0), region(ti, 0.625, 0.125, 0), 0.05), "cpu: material tiling repeats the checker");

    const lt::Image plain = render_plane(base, sun, top_camera());
    lt::Material identity = base;
    identity.base_color_map = lt::TextureMap(solid_map(1, 1, 1));
    identity.roughness_map = lt::TextureMap(solid_map(1, 1, 1));
    identity.metallic_map = lt::TextureMap(solid_map(1, 1, 1));
    check(same_image(plain, render_plane(identity, sun, top_camera())), "cpu: identity maps render exactly like no maps");
    lt::Material copy = base;
    check(same_image(plain, render_plane(copy, sun, top_camera())), "cpu: materials without maps render the same every time");

    lt::Material flat = base;
    flat.normal_map = lt::TextureMap(solid_map(0.5f, 0.5f, 1.0f));
    const double plain_c = region(plain, 0.5, 0.5, 2);
    const double flat_c = region(render_plane(flat, sun, top_camera()), 0.5, 0.5, 2);
    check(near(plain_c, flat_c, plain_c * 1e-3), "cpu: a flat normal map changes nothing (" + num(plain_c) + " vs " + num(flat_c) + ")");
    lt::Material toward = base;
    toward.normal_map = lt::TextureMap(solid_map(0.75f, 0.5f, 0.933f));
    lt::Material away = base;
    away.normal_map = lt::TextureMap(solid_map(0.25f, 0.5f, 0.933f));
    const double toward_c = region(render_plane(toward, sun, top_camera()), 0.5, 0.5, 2);
    const double away_c = region(render_plane(away, sun, top_camera()), 0.5, 0.5, 2);
    check(toward_c > plain_c * 1.2 && away_c < plain_c * 0.6, "cpu: a tilted normal map changes shading (" + num(away_c) + " < " + num(plain_c) + " < " + num(toward_c) + ")");
    lt::Material weak = toward;
    weak.normal_map.strength = 0.0f;
    check(near(region(render_plane(weak, sun, top_camera()), 0.5, 0.5, 2), plain_c, plain_c * 1e-3), "cpu: normal strength 0 turns the map off");

    lt::Mesh tangent_plane = lt::Mesh::plane(1.0f, 1.0f);
    tangent_plane.compute_tangents();
    check(tangent_plane.has_tangents() && near(tangent_plane.tangents[0].x, 1.0, 1e-5), "cpu: compute_tangents follows +u");
    {
        lt::Scene sc;
        sc.environment() = lt::Environment::none();
        sc.add_object(tangent_plane, toward);
        sc.add_light(sun);
        lt::Renderer r(direct_settings());
        const double with_t = region(r.render(sc, top_camera(), 32, 32), 0.5, 0.5, 2);
        check(near(with_t, toward_c, toward_c * 0.01), "cpu: mesh tangents and derived tangents agree (" + num(with_t) + " vs " + num(toward_c) + ")");
    }

    const lt::Light spot = lt::Light::point_light({ 0.3, 0.6, 0.0 }, lt::Rgb(1.0f), 2.0f);
    auto highlight = [&](const lt::Material& m, double& peak) {
        lt::Scene sc;
        sc.environment() = lt::Environment::none();
        sc.add_object(lt::Mesh::plane(1.0f, 1.0f), m);
        sc.add_light(spot);
        lt::Renderer r(direct_settings());
        const lt::Image img = r.render(sc, top_camera(1.2), 48, 48);
        peak = 0.0;
        for (const lt::Rgb& p : img.pixels()) peak = std::max(peak, lum(p));
        int lit = 0;
        for (const lt::Rgb& p : img.pixels()) lit += lum(p) >= peak * 0.5 ? 1 : 0;
        return lit;
    };
    lt::Material metal = lt::Material::metal(lt::Rgb(0.9f), 1.0f);
    lt::Material shiny = metal;
    shiny.roughness_map = lt::TextureMap(solid_map(0.2f, 0.2f, 0.2f));
    lt::Material rough = metal;
    rough.roughness_map = lt::TextureMap(solid_map(1.0f, 1.0f, 1.0f));
    double shiny_peak = 0.0, rough_peak = 0.0, scalar_peak = 0.0;
    const int shiny_n = highlight(shiny, shiny_peak), rough_n = highlight(rough, rough_peak);
    check(shiny_n * 2 < rough_n && shiny_peak > rough_peak * 2.0, "cpu: a roughness map narrows the highlight (" + std::to_string(shiny_n) + " vs " + std::to_string(rough_n) + " pixels)");
    lt::Material scalar = lt::Material::metal(lt::Rgb(0.9f), 0.2f);
    const int scalar_n = highlight(scalar, scalar_peak);
    check(scalar_n == shiny_n && near(scalar_peak, shiny_peak, 1e-6), "cpu: roughness maps multiply the scalar roughness");

    lt::Material cut = lt::Material::diffuse(lt::Rgb(1.0f));
    cut.base_color_map = lt::TextureMap(std::make_shared<lt::Texture>(lt::Texture::checker(4, 2, lt::Rgba(1, 1, 1, 1), lt::Rgba(1, 1, 1, 0), lt::ColorSpace::linear)));
    cut.base_color_map.filter = lt::TextureFilter::nearest;
    cut.alpha_cutoff = 0.5f;
    {
        lt::Scene sc;
        sc.environment() = lt::Environment::none();
        sc.add_object(lt::Mesh::plane(1.0f, 1.0f), cut, lt::Transform::translate({ 0.0, 0.3, 0.0 }));
        sc.add_object(lt::Mesh::plane(3.0f, 3.0f), lt::Material::diffuse(lt::Rgb(0.8f, 0.05f, 0.05f)));
        sc.add_light(lt::Light::sun({ 0.0, -1.0, 0.0 }, lt::Rgb(1.0f), 2.0f));
        lt::Renderer r(direct_settings());
        const lt::Image img = r.render(sc, top_camera(1.8), 32, 32);
        const lt::Rgb a = img.at(10, 10), b = img.at(21, 10);
        const bool a_white = a.g > 0.3f, b_white = b.g > 0.3f;
        check(a_white != b_white && (a_white ? b.r > b.g * 4.0f : a.r > a.g * 4.0f), "cpu: alpha cut-out shows the floor through the holes");
        check((img.at(21, 21).g > 0.3f) == a_white, "cpu: alpha cut-out follows the checker");
    }
    {
        lt::Scene sc;
        sc.environment() = lt::Environment::none();
        sc.add_object(lt::Mesh::plane(1.0f, 1.0f), cut, lt::Transform::translate({ 0.0, 0.3, 0.0 }));
        sc.add_object(lt::Mesh::plane(3.0f, 3.0f), lt::Material::diffuse(lt::Rgb(0.8f, 0.05f, 0.05f)));
        sc.add_light(lt::Light::sun({ 0.0, -1.0, 0.0 }, lt::Rgb(1.0f), 2.0f, 0.0f));
        lt::LightingSettings s = lt::LightingSettings::preset(lt::LightingQuality::path_traced);
        s.threads = 2;
        s.fog.enabled = false;
        s.tone_mapper = lt::ToneMapper::none;
        s.firefly_clamp = 0.0f;
        s.samples_per_pixel = 4;
        s.max_bounces = 1;
        s.denoise = false;
        lt::Renderer r(s);
        const lt::Image img = r.render(sc, top_camera(1.8), 32, 32);
        const lt::Rgb a = img.at(10, 10), b = img.at(21, 10);
        const lt::Rgb floor = a.g > b.g ? b : a;
        check(floor.r > 0.2f && floor.r > floor.g * 4.0f, "cpu: path traced shadows pass through alpha cut-outs (" + num(floor.r) + ")");
    }

    lt::Material glow = lt::Material::emissive(lt::Rgb(1.0f), 1.0f);
    glow.emission_map = lt::TextureMap(std::make_shared<lt::Texture>(lt::Texture::checker(4, 2, lt::Rgba(1, 1, 1, 1), lt::Rgba(0, 0, 0, 1), lt::ColorSpace::linear)));
    glow.emission_map.filter = lt::TextureFilter::nearest;
    check(near(glow.average_emitted().r, 0.5, 1e-6), "cpu: average emission follows the emission map");
    {
        lt::Scene sc;
        sc.environment() = lt::Environment::none();
        sc.add_object(lt::Mesh::plane(1.0f, 1.0f), glow);
        lt::LightingSettings s = direct_settings();
        lt::Renderer r(s);
        const lt::Image img = r.render(sc, top_camera(), 32, 32);
        const double e0 = region(img, 0.25, 0.25), e1 = region(img, 0.75, 0.25);
        check((e0 > 0.5 && e1 < 0.05) || (e1 > 0.5 && e0 < 0.05), "cpu: an emission map masks emission (" + num(e0) + ", " + num(e1) + ")");
    }

    lt::Material tri = base;
    tri.base_color = lt::Rgb(1.0f);
    tri.base_color_map = lt::TextureMap(std::make_shared<lt::Texture>(lt::Texture::checker(2, 2, lt::Rgba(1, 1, 1, 1), lt::Rgba(0.05f, 0.05f, 0.05f, 1), lt::ColorSpace::linear)));
    tri.base_color_map.filter = lt::TextureFilter::nearest;
    tri.base_color_map.triplanar = true;
    tri.base_color_map.triplanar_scale = 1.0f;
    const lt::Image tpi = render_plane(tri, sun, top_camera());
    const double p0 = region(tpi, 0.25, 0.25), p1 = region(tpi, 0.75, 0.25);
    check(p0 > p1 * 5.0 || p1 > p0 * 5.0, "cpu: a triplanar map projects onto a plane without uvs (" + num(p0) + ", " + num(p1) + ")");
}

gfx::Mesh3D quad_mesh(double half) {
    gfx::Mesh3D m;
    const Color white(255, 255, 255);
    const int n = 8;
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            const double x = -half + 2.0 * half * i / n, y = -half + 2.0 * half * j / n;
            m.add_vertex(gfx::Vertex3D(vector3d{ x, y, 0.0 }, vector3d{ 0.0, 0.0, 1.0 }, white, static_cast<float>(i) / n, static_cast<float>(j) / n));
        }
    }
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const auto a = static_cast<std::uint32_t>(j * (n + 1) + i);
            const auto b = static_cast<std::uint32_t>(a + n + 1);
            m.add_triangle(a, a + 1, b + 1);
            m.add_triangle(a, b + 1, b);
        }
    }
    return m;
}

bool shoot(fizmo::windows::Window& win, gfx::Renderer& r, const gfx::Mesh3D& quad, const gfx::Material3D& mat, bool lit, BitmapImage& out) {
    gfx::SceneLighting3D lighting;
    lighting.enabled = lit;
    lighting.sky_intensity = 0.0f;
    lighting.ambient = 0.0f;
    lighting.min_light = 0.0f;
    lighting.sun_direction = vector3d{ -1.0, 0.0, -1.0 };
    lighting.sun_color = Color(255, 255, 255);
    lighting.sun_intensity = 1.0f;
    r.set_scene_lighting(lighting);
    gfx::Camera3D cam(vector3d{ 0.0, 0.0, 6.0 }, 160, 160, 60.0);
    cam.look_at(vector3d{ 0.0, 0.0, 0.0 }, vector3d{ 0.0, 1.0, 0.0 });
    for (int attempt = 0; attempt < 3; ++attempt) {
        win.poll_events();
        r.begin_frame();
        r.clear(Color(0, 255, 0, 255));
        if (r.begin_3d(cam, 0, 0, 160, 160)) {
            r.draw_mesh(quad, mat);
            r.end_3d();
        }
        r.present();
        if (r.capture(out) && out.width() > 0) return true;
    }
    return false;
}

double glum(const BitmapImage& img, int x, int y) {
    const Color c = img.get_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y));
    return 0.2126 * c.red() + 0.7152 * c.green() + 0.0722 * c.blue();
}

bool same_pixels(const BitmapImage& a, const BitmapImage& b, int tolerance) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (std::size_t i = 0; i < a.pixels().size(); ++i) {
        const Color p = a.pixels()[i], q = b.pixels()[i];
        if (std::abs(p.red() - q.red()) > tolerance || std::abs(p.green() - q.green()) > tolerance || std::abs(p.blue() - q.blue()) > tolerance) return false;
    }
    return true;
}

void run_raster(fizmo::windows::Window& win, bool created, gfx::RendererBackend backend, const char* name) {
    if (!created) {
        std::printf("SKIP %s: no window (is DISPLAY set?)\n", name);
        return;
    }
    gfx::Renderer r(win, backend);
    if (!r.bind() || !r.is_gpu()) {
        std::printf("SKIP %s: no GPU backend\n", name);
        win.close();
        return;
    }
    std::printf("%s: %s (%s)\n", name, r.backend_name(), r.device_caps().device_name.c_str());
    const std::string tag = std::string(name) + ": ";
    const gfx::Mesh3D quad = quad_mesh(2.0);
    const gfx::Texture checker(checker_image(4, 2, Color(255, 0, 0), Color(0, 0, 255)), gfx::SampleFilter::Nearest, gfx::WrapMode::Clamp);

    BitmapImage img;
    gfx::Material3D flat = gfx::Material3D::unlit();
    flat.texture = &checker;
    check(shoot(win, r, quad, flat, false, img), tag + "an unlit textured quad renders");
    const Color c00 = img.get_pixel(60, 60), c10 = img.get_pixel(100, 60), c01 = img.get_pixel(60, 100), c11 = img.get_pixel(100, 100);
    check(c00 == c11 && c10 == c01 && c00 != c10 && (c00.red() > 200 || c00.blue() > 200), tag + "the checker texture shows a checker");
    const BitmapImage unlit_plain = img;

    gfx::Material3D tiled = flat;
    gfx::Texture tiled_tex(checker_image(4, 2, Color(255, 0, 0), Color(0, 0, 255)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    tiled.texture = &tiled_tex;
    tiled.with_tiling(2.0f, 2.0f);
    check(shoot(win, r, quad, tiled, false, img), tag + "a tiled quad renders");
    check(img.get_pixel(50, 50) != img.get_pixel(70, 50) && img.get_pixel(50, 50) == img.get_pixel(90, 50), tag + "uv tiling repeats the texture");

    gfx::Texture cutout_tex(checker_image(4, 2, Color(255, 255, 255, 255), Color(255, 255, 255, 0)), gfx::SampleFilter::Nearest, gfx::WrapMode::Clamp);
    gfx::Material3D cut = gfx::Material3D::unlit();
    cut.texture = &cutout_tex;
    cut.alpha_cutoff = 0.5f;
    check(shoot(win, r, quad, cut, false, img), tag + "an alpha-cut quad renders");
    const Color k0 = img.get_pixel(60, 60), k1 = img.get_pixel(100, 60);
    check((k0 == Color(255, 255, 255) && k1 == Color(0, 255, 0)) || (k1 == Color(255, 255, 255) && k0 == Color(0, 255, 0)), tag + "alpha cut-out discards the clear texels");

    const gfx::Texture opacity(checker_image(2, 2, Color(255, 255, 255), Color(0, 0, 0)), gfx::SampleFilter::Nearest, gfx::WrapMode::Clamp);
    gfx::Material3D masked = gfx::Material3D::unlit();
    masked.opacity_map = &opacity;
    masked.alpha_cutoff = 0.5f;
    check(shoot(win, r, quad, masked, false, img), tag + "an opacity-mapped quad renders");
    const Color m0 = img.get_pixel(60, 60), m1 = img.get_pixel(100, 60);
    check(m0 != m1 && (m0 == Color(0, 255, 0) || m1 == Color(0, 255, 0)), tag + "an opacity map cuts holes");

    gfx::Material3D pbr = gfx::Material3D::plastic(0.9f);
    BitmapImage lit_plain;
    check(shoot(win, r, quad, pbr, true, lit_plain), tag + "a lit pbr quad renders");
    const double plain_c = glum(lit_plain, 80, 80);
    check(plain_c > 20.0, tag + "the sun lights the quad (" + num(plain_c) + ")");
    BitmapImage again;
    check(shoot(win, r, quad, pbr, true, again) && same_pixels(lit_plain, again, 0), tag + "a material without maps renders the same every time");

    const gfx::Texture white(gfx::TextureLibrary::solid(Color(255, 255, 255)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    gfx::Material3D whites = pbr;
    whites.orm_map = &white;
    whites.emissive_map = &white;
    whites.opacity_map = &white;
    check(shoot(win, r, quad, whites, true, img) && same_pixels(lit_plain, img, 0), tag + "white orm/emissive/opacity maps render exactly like no maps");

    const gfx::Texture flat_n(gfx::TextureLibrary::solid(Color(128, 128, 255)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    const gfx::Texture toward_n(gfx::TextureLibrary::solid(Color(191, 128, 238)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    const gfx::Texture away_n(gfx::TextureLibrary::solid(Color(64, 128, 238)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    gfx::Material3D nm = pbr;
    nm.normal_map = &flat_n;
    check(shoot(win, r, quad, nm, true, img), tag + "a normal-mapped quad renders");
    const double flat_c = glum(img, 80, 80);
    check(near(flat_c, plain_c, 3.0), tag + "a flat normal map looks like no normal map (" + num(flat_c) + " vs " + num(plain_c) + ")");
    nm.normal_map = &toward_n;
    shoot(win, r, quad, nm, true, img);
    const double toward_c = glum(img, 80, 80);
    nm.normal_map = &away_n;
    shoot(win, r, quad, nm, true, img);
    const double away_c = glum(img, 80, 80);
    check(toward_c > plain_c * 1.1 && away_c < plain_c * 0.8, tag + "a tilted normal map changes shading (" + num(away_c) + " < " + num(plain_c) + " < " + num(toward_c) + ")");

    const gfx::Texture ao_dark(gfx::TextureLibrary::solid(Color(0, 255, 255)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    gfx::Material3D occluded = pbr;
    occluded.orm_map = &ao_dark;
    check(shoot(win, r, quad, occluded, true, img) && glum(img, 80, 80) < plain_c * 0.2, tag + "orm occlusion darkens the surface");
    occluded.orm_occlusion = false;
    check(shoot(win, r, quad, occluded, true, img) && near(glum(img, 80, 80), plain_c, 3.0), tag + "orm_occlusion = false ignores the red channel");

    const gfx::Texture glow_map(checker_image(2, 2, Color(255, 255, 255), Color(0, 0, 0)), gfx::SampleFilter::Nearest, gfx::WrapMode::Clamp);
    const gfx::Texture black(gfx::TextureLibrary::solid(Color(0, 0, 0)), gfx::SampleFilter::Nearest, gfx::WrapMode::Repeat);
    gfx::Material3D glow = gfx::Material3D::glowing(Color(255, 255, 255), 1.0f);
    glow.roughness = 1.0f;
    glow.texture = &black;
    glow.emissive_map = &glow_map;
    check(shoot(win, r, quad, glow, true, img), tag + "an emissive-mapped quad renders");
    const double g0 = glum(img, 60, 60), g1 = glum(img, 100, 60);
    check(std::max(g0, g1) > 100.0 && std::min(g0, g1) < 30.0, tag + "an emissive map masks the glow (" + num(g0) + ", " + num(g1) + ")");

    gfx::Texture fine(checker_image(64, 64, Color(255, 0, 0), Color(0, 0, 255)), gfx::SampleFilter::Bilinear, gfx::WrapMode::Repeat);
    fine.set_mipmaps(true);
    gfx::Material3D mipped = gfx::Material3D::unlit();
    mipped.texture = &fine;
    mipped.with_tiling(4.0f, 4.0f);
    check(shoot(win, r, quad, mipped, false, img), tag + "a mipmapped texture renders");
    double rs = 0.0, bs = 0.0, spread = 0.0;
    for (int y = 70; y < 90; ++y)
        for (int x = 70; x < 90; ++x) {
            const Color c = img.get_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y));
            rs += c.red();
            bs += c.blue();
            spread += std::abs(static_cast<int>(c.red()) - static_cast<int>(c.blue()));
        }
    check(near(rs / 400.0, bs / 400.0, 30.0) && spread / 400.0 < 60.0 && rs > 400.0 * 60.0, tag + "trilinear mipmaps average a tiny checker to purple (r " + num(rs / 400.0) + ", b " + num(bs / 400.0) + ", contrast " + num(spread / 400.0) + ")");

    gfx::Material3D tp = gfx::Material3D::unlit();
    tp.texture = &tiled_tex;
    tp.with_triplanar(1.0f, 4.0f);
    check(shoot(win, r, quad, tp, false, img), tag + "a triplanar quad renders");
    check(img.get_pixel(60, 60) != img.get_pixel(100, 60), tag + "triplanar mapping projects the checker");

    gfx::TextureLibrary lib;
    auto shared = lib.add("memory:checker", checker_image(4, 2, Color(255, 0, 0), Color(0, 0, 255)), gfx::TextureSettings::pixel_art());
    gfx::Material3D from_lib = gfx::Material3D::unlit();
    from_lib.texture = shared.get();
    check(shoot(win, r, quad, from_lib, false, img) && img.get_pixel(60, 60) == unlit_plain.get_pixel(60, 60), tag + "library textures draw like hand-made ones");
    lib.add("memory:checker", checker_image(4, 2, Color(0, 255, 255), Color(255, 255, 0)), gfx::TextureSettings::pixel_art());
    check(shoot(win, r, quad, from_lib, false, img) && img.get_pixel(60, 60) != unlit_plain.get_pixel(60, 60), tag + "replacing a library texture re-uploads it");
    win.close();
}

void run_gpu_tracer(fizmo::windows::Window& win, bool created, gfx::RendererBackend backend, const char* name) {
    if (!created) {
        std::printf("SKIP %s tracer: no window\n", name);
        return;
    }
    gfx::Renderer r(win, backend);
    if (!r.bind() || !r.is_gpu()) {
        std::printf("SKIP %s tracer: no GPU backend\n", name);
        win.close();
        return;
    }
    const std::string tag = std::string(name) + " tracer: ";
    gfx::GpuPathTracer pt;
    if (!pt.attach(r, 32, 32) || !pt.supported()) {
        std::printf("SKIP %s tracer: %s\n", name, pt.error().c_str());
        win.close();
        return;
    }
    lt::Scene sc;
    sc.environment() = lt::Environment::none();
    lt::Material checker = lt::Material::diffuse(lt::Rgb(1.0f));
    checker.base_color_map = lt::TextureMap(std::make_shared<lt::Texture>(lt::Texture::checker(4, 2, lt::Rgba(1, 1, 1, 1), lt::Rgba(0.05f, 0.05f, 0.05f, 1), lt::ColorSpace::linear)));
    checker.base_color_map.filter = lt::TextureFilter::nearest;
    checker.base_color_map.wrap = lt::TextureWrap::clamp;
    sc.add_object(lt::Mesh::plane(1.0f, 1.0f), checker);
    sc.add_light(lt::Light::sun({ -1.0, -1.0, 0.0 }, lt::Rgb(1.0f), 2.0f));
    lt::LightingSettings s = lt::LightingSettings::preset(lt::LightingQuality::path_traced);
    s.threads = 2;
    s.fog.enabled = false;
    s.tone_mapper = lt::ToneMapper::none;
    s.firefly_clamp = 0.0f;
    s.max_bounces = 1;
    pt.set_settings(s);
    pt.set_samples_per_frame(4);
    pt.set_scene(sc);
    pt.set_camera(top_camera());
    for (int i = 0; i < 16; ++i) {
        win.poll_events();
        r.begin_frame();
        r.clear(Color(0, 0, 0, 255));
        pt.draw(r, 0, 0);
        r.present();
    }
    lt::Image hdr;
    check(pt.read_hdr(hdr) && hdr.width() == 32, tag + "renders");
    if (hdr.width() == 32) {
        const double q[4] = { region(hdr, 0.25, 0.25), region(hdr, 0.75, 0.25), region(hdr, 0.25, 0.75), region(hdr, 0.75, 0.75) };
        const bool a = q[0] > q[1] * 4.0 && q[3] > q[2] * 4.0, b = q[1] > q[0] * 4.0 && q[2] > q[3] * 4.0;
        check(a || b, tag + "a checker base-colour map renders a checker (" + num(q[0]) + " " + num(q[1]) + " " + num(q[2]) + " " + num(q[3]) + ")");
    }
    pt.detach();
    win.close();
}

} // namespace

int main(int argc, char** argv) {
    const std::string only = argc > 1 ? argv[1] : "";
    if (only.empty() || only == "cpu") {
        const fs::path dir = scratch_dir();
        test_loading_and_cache(dir);
        test_color_space();
        test_uv_transform();
        test_wrap_modes();
        test_cpu_tracer();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    if (only.empty() || only == "vulkan") {
        fizmo::windows::Window w(160, 160, "fizmo texture tests vulkan");
        run_raster(w, w.create(), gfx::RendererBackend::Vulkan, "vulkan");
    }
    if (only.empty() || only == "opengl") {
        fizmo::windows::Window w(160, 160, "fizmo texture tests opengl");
        run_raster(w, w.create(), gfx::RendererBackend::OpenGL, "opengl");
    }
    if (only.empty() || only == "vulkan" || only == "tracer") {
        fizmo::windows::Window w(160, 160, "fizmo texture tracer vulkan");
        run_gpu_tracer(w, w.create(), gfx::RendererBackend::Vulkan, "vulkan");
    }
    std::printf("texture tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
