#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include "../Source/Graphics/Renderer/renderer.hpp"
#include "../Source/Graphics/lighting_bridge.hpp"
#include "../Source/Graphics/gpu_path_tracer.hpp"
#include "../Source/Windows/window.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>

namespace gfx = fizmo::graphics;
using fizmo::colors::Color;
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

gfx::Mesh3D plane(double half) {
    gfx::Mesh3D m;
    const Color white(255, 255, 255);
    const int n = 48;
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

double luminance(const fizmo::images::BitmapImage& img, int x, int y) {
    const Color c = img.get_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y));
    return 0.2126 * c.red() + 0.7152 * c.green() + 0.0722 * c.blue();
}

struct Spread {
    int along_x = 0;
    int along_y = 0;
    double center = 0.0;
};

Spread spread(const fizmo::images::BitmapImage& img) {
    Spread s;
    const int w = static_cast<int>(img.width()), h = static_cast<int>(img.height());
    const int cx = w / 2, cy = h / 2;
    s.center = luminance(img, cx, cy);
    const double cut = s.center * 0.6;
    for (int x = 0; x < w; ++x) s.along_x += luminance(img, x, cy) >= cut ? 1 : 0;
    for (int y = 0; y < h; ++y) s.along_y += luminance(img, cx, y) >= cut ? 1 : 0;
    return s;
}

void save_ppm(const fizmo::images::BitmapImage& img, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << img.width() << " " << img.height() << "\n255\n";
    for (unsigned y = 0; y < img.height(); ++y) {
        for (unsigned x = 0; x < img.width(); ++x) {
            const Color c = img.get_pixel(x, y);
            const char rgb[3] = { static_cast<char>(c.red()), static_cast<char>(c.green()), static_cast<char>(c.blue()) };
            f.write(rgb, 3);
        }
    }
}

bool shoot(fizmo::windows::Window& win, gfx::Renderer& r, const gfx::Mesh3D& floor, const gfx::PointLight3D& light, fizmo::images::BitmapImage& out) {
    gfx::SceneLighting3D lighting;
    lighting.enabled = true;
    lighting.sky_intensity = 0.0f;
    lighting.ambient = 0.0f;
    lighting.min_light = 0.0f;
    lighting.point_lights.push_back(light);
    r.set_scene_lighting(lighting);
    gfx::Camera3D cam(vector3d{ 0.0, 0.0, 6.0 }, 160, 160, 60.0);
    cam.look_at(vector3d{ 0.0, 0.0, 0.0 }, vector3d{ 0.0, 1.0, 0.0 });
    for (int attempt = 0; attempt < 3; ++attempt) {
        win.poll_events();
        r.begin_frame();
        r.clear(Color(0, 0, 0, 255));
        if (r.begin_3d(cam, 0, 0, 160, 160)) {
            r.draw_mesh(floor, gfx::Material3D::plastic(0.9f));
            r.end_3d();
        }
        r.present();
        if (r.capture(out) && out.width() > 0) return true;
    }
    return false;
}

void run(fizmo::windows::Window& win, bool created, gfx::RendererBackend backend, const char* name, const char* folder) {
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
    const gfx::Mesh3D floor = plane(4.0);
    const Color white(255, 255, 255);
    const vector3d above{ 0.0, 0.0, 1.0 };
    struct Shot {
        const char* label;
        gfx::PointLight3D light;
    };
    const Shot shots[] = {
        { "point", gfx::PointLight3D(above, white, 0.8f, 4.0f) },
        { "sphere", gfx::PointLight3D::sphere(above, 0.5f, white, 0.8f, 4.0f) },
        { "tube", gfx::PointLight3D::tube(above, vector3d{ 1.0, 0.0, 0.0 }, 3.0f, 0.05f, white, 0.8f, 4.0f) },
        { "rectangle", gfx::PointLight3D::rectangle(above, vector3d{ 0.0, 0.0, -1.0 }, vector3d{ 1.0, 0.0, 0.0 }, 0.6f, 2.6f, white, 0.8f, 4.0f) },
        { "rectangle_up", gfx::PointLight3D::rectangle(above, vector3d{ 0.0, 0.0, 1.0 }, vector3d{ 1.0, 0.0, 0.0 }, 0.6f, 2.6f, white, 0.8f, 4.0f) },
        { "disk", gfx::PointLight3D::disk(above, vector3d{ 0.0, 0.0, -1.0 }, 0.6f, white, 0.8f, 4.0f) },
    };
    Spread got[6];
    for (int i = 0; i < 6; ++i) {
        fizmo::images::BitmapImage img;
        const bool ok = shoot(win, r, floor, shots[i].light, img);
        check(ok, std::string(name) + ": " + shots[i].label + " renders");
        if (!ok) return;
        got[i] = spread(img);
        if (folder) save_ppm(img, std::string(folder) + "/" + name + "_" + shots[i].label + ".ppm");
        std::printf("  %-13s center %.0f, lit width %d x %d\n", shots[i].label, got[i].center, got[i].along_x, got[i].along_y);
    }
    check(got[0].center > 20.0, std::string(name) + ": the point light lights the floor");
    check(got[1].center >= got[0].center * 0.9, std::string(name) + ": a sphere light is at least as bright under its middle");
    check(got[2].along_x > got[0].along_x + 8 && got[2].along_x > got[2].along_y + 8, std::string(name) + ": a tube along x stretches the lit area along x");
    check(got[3].along_y > got[0].along_y + 8 && got[3].along_y > got[3].along_x + 8, std::string(name) + ": a tall rectangle stretches the lit area along y");
    check(got[4].center < got[3].center * 0.2, std::string(name) + ": a one-sided rectangle facing away barely lights the floor");
    check(got[5].center > 20.0 && got[5].along_x > got[0].along_x, std::string(name) + ": a disk light spreads wider than a point");
    gfx::PointLight3D two = shots[4].light;
    two.two_sided = true;
    fizmo::images::BitmapImage img;
    check(shoot(win, r, floor, two, img) && spread(img).center > got[3].center * 0.8, std::string(name) + ": a two-sided rectangle lights both ways");
    fizmo::lighting::Light tube = fizmo::lighting::Light::tube_light({ 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 }, 3.0f, 0.05f, fizmo::lighting::Rgb(1.0f), 2.0f);
    const gfx::PointLight3D bridged = gfx::to_point_light(tube);
    check(bridged.shape == gfx::LightShape3D::Tube && std::fabs(bridged.size_y - 1.5f) < 1e-5f, std::string(name) + ": the bridge keeps tube shapes");
    win.close();
}


namespace lt = fizmo::lighting;

lt::LightingSettings quiet(lt::LightingQuality q) {
    lt::LightingSettings s = lt::LightingSettings::preset(q);
    s.threads = 2;
    s.fog.enabled = false;
    s.firefly_clamp = 0.0f;
    s.tone_mapper = lt::ToneMapper::none;
    return s;
}

bool trace_frames(fizmo::windows::Window& win, gfx::Renderer& r, gfx::GpuPathTracer& pt, int frames) {
    for (int i = 0; i < frames; ++i) {
        win.poll_events();
        r.begin_frame();
        r.clear(Color(0, 0, 0, 255));
        pt.draw(r, 0, 0);
        r.present();
        if (!pt.supported()) return false;
    }
    return true;
}

bool gpu_render(fizmo::windows::Window& win, gfx::Renderer& r, lt::Scene& sc, const lt::LightingSettings& s, const lt::Camera& cam, unsigned w, unsigned h, int frames, int spp, lt::Image& out, std::uint32_t photons = 0) {
    gfx::GpuPathTracer pt;
    if (!pt.attach(r, w, h)) return false;
    pt.set_settings(s);
    pt.set_samples_per_frame(spp);
    if (photons > 0) pt.set_photons_per_frame(photons);
    pt.set_scene(sc);
    pt.set_camera(cam);
    if (!trace_frames(win, r, pt, frames)) {
        std::printf("  tracer error: %s\n", pt.error().c_str());
        return false;
    }
    const bool ok = pt.read_hdr(out);
    pt.detach();
    return ok;
}

double avg_lum(const lt::Image& img) {
    double sum = 0.0;
    for (const auto& p : img.pixels()) sum += p.luminance();
    return img.pixels().empty() ? 0.0 : sum / static_cast<double>(img.pixels().size());
}

void save_hdr(const lt::Image& img, const std::string& path) {
    lt::LightingSettings s;
    s.exposure = 0.8f;
    img.write_ppm(path, s);
}

void run_tracer(fizmo::windows::Window& win, bool created, gfx::RendererBackend backend, const char* name, const char* folder) {
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
    if (!r.device_caps().compute) {
        std::printf("SKIP %s tracer: no compute shaders\n", name);
        win.close();
        return;
    }
    const std::string tag = std::string(name) + " tracer: ";
    const lt::Camera close = lt::Camera::look_at({ 0, 0, 4 }, { 0, 0, 0 }, { 0, 1, 0 }, 30.0f);

    lt::Scene furnace;
    furnace.environment() = lt::Environment::uniform(lt::Rgb(1.0f));
    furnace.add_object(lt::Mesh::sphere(1.0f, 48, 24), lt::Material::diffuse(lt::Rgb(0.8f)));
    lt::Image img;
    bool ok = gpu_render(win, r, furnace, quiet(lt::LightingQuality::path_traced), close, 9, 9, 16, 16, img);
    check(ok, tag + "path tracing runs");
    if (!ok) {
        win.close();
        return;
    }
    lt::Rgb c = img.at(4, 4);
    std::printf("  furnace %.3f %.3f %.3f\n", c.r, c.g, c.b);
    check(std::fabs(c.r - 0.8f) < 0.05f && std::fabs(c.g - 0.8f) < 0.05f && std::fabs(c.b - 0.8f) < 0.05f, tag + "a diffuse sphere in a white sky reflects its albedo");

    lt::LightingSettings sp = quiet(lt::LightingQuality::path_traced);
    sp.transport = lt::Transport::spectral;
    lt::Scene sky;
    sky.environment() = lt::Environment::uniform(lt::Rgb(1.0f));
    ok = gpu_render(win, r, sky, sp, close, 9, 9, 32, 16, img);
    c = img.average();
    std::printf("  spectral sky %.3f %.3f %.3f\n", c.r, c.g, c.b);
    check(ok && std::fabs(c.r - 1.0f) < 0.05f && std::fabs(c.g - 1.0f) < 0.05f && std::fabs(c.b - 1.0f) < 0.05f, tag + "spectral sampling of a white sky averages to white");
    ok = gpu_render(win, r, furnace, sp, close, 9, 9, 32, 16, img);
    c = img.at(4, 4);
    std::printf("  spectral furnace %.3f %.3f %.3f\n", c.r, c.g, c.b);
    check(ok && std::fabs(c.r - 0.8f) < 0.08f && std::fabs(c.g - 0.8f) < 0.08f && std::fabs(c.b - 0.8f) < 0.08f, tag + "spectral transport keeps the sphere grey");

    lt::Scene glass;
    glass.environment() = lt::Environment::uniform(lt::Rgb(1.0f));
    glass.add_object(lt::Mesh::sphere(1.0f, 48, 24), lt::Material::glass(1.5f, 0.0f));
    ok = gpu_render(win, r, glass, quiet(lt::LightingQuality::path_traced), close, 9, 9, 16, 16, img);
    c = img.at(4, 4);
    std::printf("  glass %.3f %.3f %.3f\n", c.r, c.g, c.b);
    check(ok && std::fabs(c.r - 1.0f) < 0.08f && std::fabs(c.g - 1.0f) < 0.08f, tag + "clear glass in a white sky neither adds nor loses light");

    lt::Scene room;
    room.environment() = lt::Environment::gradient(lt::Rgb(0.3f, 0.5f, 0.9f), lt::Rgb(0.8f), lt::Rgb(0.2f), 0.5f);
    room.add_object(lt::Mesh::plane(4.0f, 4.0f), lt::Material::diffuse(lt::Rgb(0.75f)));
    room.add_object(lt::Mesh::box({ 0.05f, 2.0f, 4.0f }), lt::Material::diffuse(lt::Rgb(0.75f, 0.12f, 0.1f)), lt::Transform::translate({ -2.0, 2.0, 0.0 }));
    room.add_object(lt::Mesh::sphere(0.5f, 32, 16), lt::Material::metal(lt::Rgb(0.9f, 0.7f, 0.3f), 0.2f), lt::Transform::translate({ 0.6, 0.5, 0.0 }));
    room.add_object(lt::Mesh::sphere(0.12f, 16, 8), lt::Material::emissive(lt::Rgb(1.0f, 0.4f, 0.1f), 10.0f), lt::Transform::translate({ -1.0, 0.12, 0.8 }));
    room.add_light(lt::Light::rectangle_light({ 0.0, 3.0, 0.0 }, { 0, -1, 0 }, { 0, 0, 1 }, 1.0f, 1.0f, lt::Rgb(1.0f), 12.0f));
    room.add_light(lt::Light::sun({ 0.4, -1.0, -0.3 }, lt::Rgb(1.0f), 2.0f, 1.0f));
    room.add_light(lt::Light::tube_light({ 1.5, 1.5, -1.0 }, { 0, 1, 0 }, 1.0f, 0.05f, lt::Rgb(0.4f, 0.6f, 1.0f), 6.0f));
    room.add_light(lt::Light::spot_light({ -1.5, 2.5, 1.5 }, { 0.5, -1.0, -0.5 }, lt::Rgb(1.0f), 20.0f, 25.0f, 15.0f));
    const lt::Camera cam = lt::Camera::look_at({ 0.0, 1.6, 4.5 }, { 0.0, 0.6, 0.0 }, { 0, 1, 0 }, 55.0f);
    lt::LightingSettings rs = quiet(lt::LightingQuality::path_traced);
    lt::Renderer cpu(rs);
    const lt::Image& ref = cpu.render_passes(room, cam, 40, 30, 16);
    const double cpu_avg = avg_lum(ref);
    ok = gpu_render(win, r, room, rs, cam, 40, 30, 16, 4, img);
    const double gpu_avg = avg_lum(img);
    std::printf("  room cpu %.4f gpu %.4f\n", cpu_avg, gpu_avg);
    check(ok && cpu_avg > 0.0 && std::fabs(gpu_avg - cpu_avg) < cpu_avg * 0.12, tag + "a lit room matches the CPU path tracer's brightness");
    double diff = 0.0;
    for (std::size_t i = 0; i < img.pixels().size() && i < ref.pixels().size(); ++i) diff += std::fabs(img.pixels()[i].luminance() - ref.pixels()[i].luminance());
    diff /= std::max<std::size_t>(img.pixels().size(), 1);
    std::printf("  room mean abs diff %.4f\n", diff);
    check(ok && diff < cpu_avg * 0.35, tag + "the GPU and CPU images agree pixel by pixel");
    if (folder) {
        save_hdr(img, std::string(folder) + "/" + name + "_gpu_path.ppm");
        save_hdr(ref, std::string(folder) + "/" + name + "_cpu_path.ppm");
    }

    lt::Scene caustic;
    caustic.environment() = lt::Environment::none();
    caustic.add_object(lt::Mesh::plane(3.0f, 3.0f), lt::Material::diffuse(lt::Rgb(0.8f)));
    caustic.add_object(lt::Mesh::sphere(0.5f, 48, 24), lt::Material::glass(1.5f, 0.0f), lt::Transform::translate({ 0.0, 1.5, 0.0 }));
    caustic.add_light(lt::Light::spot_light({ 0, 6, 0 }, { 0, -1, 0 }, lt::Rgb(1.0f), 30.0f, 7.0f, 6.0f));
    const lt::Camera top = lt::Camera::look_at({ 0.0, 4.0, 0.01 }, { 0.0, 0.0, 0.0 }, { 0, 0, -1 }, 40.0f);
    lt::LightingSettings ps = quiet(lt::LightingQuality::photon);
    ps.photon_radius = 0.06f;
    lt::Image with, without;
    const bool a = gpu_render(win, r, caustic, ps, top, 32, 32, 16, 1, with, 131072);
    lt::LightingSettings np = ps;
    np.caustics = false;
    const bool b = gpu_render(win, r, caustic, np, top, 32, 32, 16, 1, without);
    auto disk = [](const lt::Image& im, float lo, float hi) {
        double sum = 0.0;
        int n = 0;
        for (unsigned y = 0; y < im.height(); ++y) {
            for (unsigned x = 0; x < im.width(); ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) / im.width() - 0.5f, dy = (static_cast<float>(y) + 0.5f) / im.height() - 0.5f;
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d >= lo && d < hi) {
                    sum += im.at(x, y).luminance();
                    ++n;
                }
            }
        }
        return n > 0 ? sum / n : 0.0;
    };
    const double focus_with = disk(with, 0.0f, 0.03f), focus_without = disk(without, 0.0f, 0.03f);
    std::printf("  caustic focus %.4f (photons) vs %.4f (paths only)\n", focus_with, focus_without);
    check(a && b && focus_with > focus_without * 3.0 + 1e-3, tag + "photons light the caustic a glass ball focuses under a spot light");
    if (folder) save_hdr(with, std::string(folder) + "/" + name + "_gpu_caustic.ppm");

    lt::Scene prism;
    prism.environment() = lt::Environment::none();
    prism.add_object(lt::Mesh::plane(2.0f, 2.0f), lt::Material::diffuse(lt::Rgb(0.8f)));
    prism.add_object(lt::Mesh::prism(0.5f, 0.5f), lt::Material::flint_glass(), lt::Transform::translate({ 0.0, 1.5, 0.0 }) * lt::Transform::from_quaternion(0.0, 0.0, std::sin(0.5), std::cos(0.5)));
    prism.add_light(lt::Light::sun({ 0.3, -1, 0 }, lt::Rgb(1.0f), 5.0f, 0.0f));
    lt::Image split;
    const bool pr = gpu_render(win, r, prism, ps, lt::Camera::look_at({ 0.0, 4.0, 0.01 }, { 0.0, 0.0, 0.0 }, { 0, 0, -1 }, 50.0f), 48, 48, 24, 1, split, 131072);
    int reddish = 0, bluish = 0;
    for (const auto& p : split.pixels()) {
        if (p.r > 0.05f && p.r > p.b * 1.6f) ++reddish;
        if (p.b > 0.05f && p.b > p.r * 1.6f) ++bluish;
    }
    std::printf("  prism red %d blue %d\n", reddish, bluish);
    check(pr && reddish > 0 && bluish > 0, tag + "a flint prism splits sunlight into a spectrum on the GPU");
    if (folder) save_hdr(split, std::string(folder) + "/" + name + "_gpu_prism.ppm");

    lt::Scene up;
    up.environment() = lt::Environment::none();
    up.add_object(lt::Mesh::quad({ -3, 0, -1 }, { 3, 0, -1 }, { 3, 3, -1 }, { -3, 3, -1 }), lt::Material::emissive(lt::Rgb(1.0f), 2.0f));
    gfx::GpuPathTracer pt;
    pt.attach(r, 16, 16);
    lt::LightingSettings us = quiet(lt::LightingQuality::path_traced);
    pt.set_settings(us);
    pt.set_scene(up);
    pt.set_camera(lt::Camera::look_at({ 0, 0, 2 }, { 0, 0, -1 }, { 0, 1, 0 }, 60.0f));
    pt.set_max_samples(4);
    trace_frames(win, r, pt, 6);
    std::printf("  samples after 6 frames with a limit of 4: %u\n", pt.samples());
    check(pt.samples() == 4 && pt.converged(), tag + "accumulation stops at the sample limit");
    fizmo::images::BitmapImage shown;
    const bool read = r.read_target(pt.target(), shown) && shown.width() == 16;
    double top_l = 0.0, bottom_l = 0.0;
    if (read) {
        for (int x = 0; x < 16; ++x) {
            top_l += luminance(shown, x, 2);
            bottom_l += luminance(shown, x, 13);
        }
    }
    std::printf("  orientation top %.0f bottom %.0f\n", top_l, bottom_l);
    check(read && top_l > bottom_l + 100.0, tag + "the output image is the right way up");
    pt.set_camera(lt::Camera::look_at({ 0, 0, 2.5 }, { 0, 0, -1 }, { 0, 1, 0 }, 60.0f));
    check(pt.samples() == 0, tag + "moving the camera restarts accumulation");
    pt.detach();

    if (folder && std::getenv("FIZMO_LIGHTING_GPU_SHOWCASE")) {
        lt::Scene sc;
        sc.environment() = lt::Environment::gradient(lt::Rgb(0.35f, 0.5f, 0.85f), lt::Rgb(0.85f, 0.85f, 0.9f), lt::Rgb(0.25f, 0.22f, 0.2f), 0.6f);
        sc.add_object(lt::Mesh::plane(4.0f, 4.0f), lt::Material::diffuse(lt::Rgb(0.75f)));
        sc.add_object(lt::Mesh::box({ 0.05f, 2.0f, 4.0f }), lt::Material::diffuse(lt::Rgb(0.75f, 0.12f, 0.1f)), lt::Transform::translate({ -3.0, 2.0, 0.0 }));
        sc.add_object(lt::Mesh::box({ 4.0f, 2.0f, 0.05f }), lt::Material::diffuse(lt::Rgb(0.7f)), lt::Transform::translate({ 0.0, 2.0, -3.0 }));
        sc.add_object(lt::Mesh::box({ 0.05f, 2.0f, 4.0f }), lt::Material::diffuse(lt::Rgb(0.12f, 0.55f, 0.15f)), lt::Transform::translate({ 3.0, 2.0, 0.0 }));
        sc.add_object(lt::Mesh::sphere(0.6f, 64, 32), lt::Material::glass(1.5f, 40.0f), lt::Transform::translate({ -1.0, 0.6, 0.6 }));
        sc.add_object(lt::Mesh::sphere(0.55f, 64, 32), lt::Material::metal(lt::Rgb(0.95f, 0.75f, 0.35f), 0.12f), lt::Transform::translate({ 1.1, 0.55, -0.6 }));
        sc.add_object(lt::Mesh::box({ 0.4f, 0.7f, 0.4f }), lt::Material::plastic(lt::Rgb(0.2f, 0.35f, 0.8f), 0.4f), lt::Transform::translate({ 0.3, 0.7, -1.8 }) * lt::Transform::from_quaternion(0.0, std::sin(0.35), 0.0, std::cos(0.35)));
        sc.add_object(lt::Mesh::prism(0.35f, 0.35f), lt::Material::flint_glass(), lt::Transform::translate({ 1.4, 0.35, 1.2 }));
        sc.add_object(lt::Mesh::sphere(0.15f, 24, 12), lt::Material::emissive(lt::Rgb(1.0f, 0.35f, 0.1f), 12.0f), lt::Transform::translate({ -2.2, 0.15, -1.8 }));
        sc.add_light(lt::Light::rectangle_light({ 0.0, 3.95, 0.0 }, { 0, -1, 0 }, { 0, 0, 1 }, 1.4f, 1.4f, lt::Rgb(1.0f), 18.0f).with_temperature(5200.0f));
        sc.add_light(lt::Light::sun({ 0.5, -1.0, -0.6 }, lt::Rgb(1.0f, 0.95f, 0.85f), 2.5f, 1.0f));
        sc.add_light(lt::Light::spot_light({ 2.4, 3.2, 2.4 }, { -0.6, -1.0, -0.6 }, lt::Rgb(0.6f, 0.8f, 1.0f), 25.0f, 28.0f, 18.0f));
        const lt::Camera view = lt::Camera::look_at({ 0.0, 2.2, 6.2 }, { 0.0, 0.8, 0.0 }, { 0, 1, 0 }, 55.0f);
        for (lt::LightingQuality q : { lt::LightingQuality::path_traced, lt::LightingQuality::photon }) {
            lt::LightingSettings s = lt::LightingSettings::preset(q);
            s.exposure = 0.75f;
            lt::Image shot;
            if (gpu_render(win, r, sc, s, view, 320, 220, 48, 4, shot, 262144)) {
                lt::LightingSettings tone = s;
                shot.write_ppm(std::string(folder) + "/" + name + "_showcase_" + lt::to_string(q) + ".ppm", tone);
                std::printf("  wrote the %s showcase\n", lt::to_string(q));
            } else {
                std::printf("  the %s showcase didn't render\n", lt::to_string(q));
            }
        }
    }
    win.close();
}

} // namespace

int main(int argc, char** argv) {
    const std::string only = argc > 1 ? argv[1] : "";
    const char* folder = std::getenv("FIZMO_LIGHTING_GPU_IMAGES");
    if (only.empty() || only == "vulkan") {
        fizmo::windows::Window w(160, 160, "fizmo lighting gpu vulkan");
        run(w, w.create(), gfx::RendererBackend::Vulkan, "vulkan", folder);
    }
    if (only.empty() || only == "opengl") {
        fizmo::windows::Window w(160, 160, "fizmo lighting gpu opengl");
        run(w, w.create(), gfx::RendererBackend::OpenGL, "opengl", folder);
    }
    if (only.empty() || only == "vulkan" || only == "tracer") {
        fizmo::windows::Window w(160, 160, "fizmo path tracer vulkan");
        run_tracer(w, w.create(), gfx::RendererBackend::Vulkan, "vulkan", folder);
    }
    if (only.empty() || only == "opengl" || only == "tracer") {
        fizmo::windows::Window w(160, 160, "fizmo path tracer opengl");
        run_tracer(w, w.create(), gfx::RendererBackend::OpenGL, "opengl", folder);
    }
    std::printf("lighting gpu tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
