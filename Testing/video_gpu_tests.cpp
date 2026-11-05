#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include "../Source/Graphics/Renderer/renderer.hpp"
#include "../Source/Graphics/video_frame_texture.hpp"
#include "../Source/Video/frame.hpp"
#include "../Source/Windows/window.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace fizmo::video;
namespace gfx = fizmo::graphics;

namespace {

int g_passed = 0;
int g_failed = 0;
int g_worst = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

struct Rng {
    std::uint32_t s;
    explicit Rng(std::uint32_t seed) : s(seed * 2654435761u + 1u) {}
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

void put(VideoFrame& f, int plane, std::uint32_t x, std::uint32_t y, int v) {
    std::uint8_t* row = f.plane(plane) + f.stride[plane] * y;
    if (bytes_per_sample(f.format) == 2) {
        row[2 * x] = static_cast<std::uint8_t>(v & 255);
        row[2 * x + 1] = static_cast<std::uint8_t>(v >> 8);
    } else {
        row[x] = static_cast<std::uint8_t>(v);
    }
}

VideoFrame make_frame(std::uint32_t w, std::uint32_t h, PixelFormat fmt, ColorMatrix m, ColorRange r, std::uint8_t loc, std::uint32_t seed) {
    VideoFrame f;
    f.allocate(w, h, fmt);
    f.color.matrix = m;
    f.color.range = r;
    f.color.chroma_location = loc;
    const int depth = bit_depth(fmt);
    const int shift = depth - 8;
    const int top = (1 << depth) - 1;
    Rng rng(seed);
    for (int p = 0; p < plane_count(fmt); ++p) {
        const int lo = r == ColorRange::full ? 0 : (8 << shift);
        const int hi = r == ColorRange::full ? top : (p == 0 ? (245 << shift) : (248 << shift));
        for (std::uint32_t y = 0; y < f.plane_height(p); ++y) {
            for (std::uint32_t x = 0; x < f.plane_width(p); ++x) {
                const int base = lo + static_cast<int>((static_cast<long long>(x * 37 + y * 23 + p * 101) * (hi - lo)) / (37 * 64 + 23 * 64 + 400)) % (hi - lo + 1);
                const int noise = static_cast<int>(rng.next() % static_cast<std::uint32_t>(((hi - lo) / 3) + 1));
                int v = (rng.next() & 3) == 0 ? lo + static_cast<int>(rng.next() % static_cast<std::uint32_t>(hi - lo + 1)) : base + noise - (hi - lo) / 6;
                v = std::max(lo, std::min(hi, v));
                put(f, p, x, y, v);
            }
        }
    }
    return f;
}

struct Diff {
    int worst = 0;
    int x = -1;
    int y = -1;
    int channel = -1;
    int got = 0;
    int want = 0;
};

Diff compare(const fizmo::images::BitmapImage& img, int ox, int oy, const std::vector<std::uint8_t>& want, std::uint32_t w, std::uint32_t h, int scale = 1, float opacity = 1.0f) {
    Diff d;
    for (std::uint32_t y = 0; y < h * static_cast<std::uint32_t>(scale); ++y) {
        for (std::uint32_t x = 0; x < w * static_cast<std::uint32_t>(scale); ++x) {
            const fizmo::colors::Color c = img.get_pixel(static_cast<unsigned>(ox) + x, static_cast<unsigned>(oy) + y);
            const std::uint8_t* p = &want[(static_cast<std::size_t>(y / static_cast<std::uint32_t>(scale)) * w + x / static_cast<std::uint32_t>(scale)) * 4];
            const int got[3] = { c.red(), c.green(), c.blue() };
            for (int k = 0; k < 3; ++k) {
                const int expect = static_cast<int>(p[k] * opacity + 0.5f);
                const int diff = std::abs(got[k] - expect);
                if (diff > d.worst) {
                    d.worst = diff;
                    d.x = static_cast<int>(x);
                    d.y = static_cast<int>(y);
                    d.channel = k;
                    d.got = got[k];
                    d.want = expect;
                }
            }
        }
    }
    return d;
}

std::string describe(const Diff& d) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "max diff %d at (%d,%d) ch %d got %d want %d", d.worst, d.x, d.y, d.channel, d.got, d.want);
    return buf;
}

const char* matrix_name(ColorMatrix m) {
    switch (m) {
        case ColorMatrix::bt601: return "bt601";
        case ColorMatrix::bt709: return "bt709";
        case ColorMatrix::bt2020: return "bt2020";
        case ColorMatrix::rgb: return "rgb";
        default: return "unspecified";
    }
}

class Bench {
public:
    Bench(fizmo::windows::Window& win, gfx::Renderer& r) : m_win(win), m_r(r) {}

    template <typename Draw>
    bool render(Draw&& draw, fizmo::images::BitmapImage& out) {
        for (int attempt = 0; attempt < 3; ++attempt) {
            m_win.poll_events();
            m_r.begin_frame();
            m_r.clear(fizmo::colors::Color(0, 0, 0, 255));
            draw();
            m_r.present();
            if (m_r.capture(out) && out.width() > 0) return true;
        }
        return false;
    }

private:
    fizmo::windows::Window& m_win;
    gfx::Renderer& m_r;
};

void run_backend(fizmo::windows::Window& win, bool created, gfx::RendererBackend backend, const char* name, bool require) {
    if (!created) {
        std::printf("SKIP %s: no window (is DISPLAY set?)\n", name);
        check(!require, std::string(name) + ": window");
        return;
    }
    gfx::Renderer r(win, backend);
    if (!r.bind() || !r.is_gpu()) {
        std::printf("SKIP %s: renderer did not bind a GPU backend\n", name);
        check(!require, std::string(name) + ": bind");
        win.close();
        return;
    }
    std::printf("%s: %s (%s)\n", name, r.backend_name(), r.device_caps().device_name.c_str());
    Bench bench(win, r);

    gfx::VideoFrameView two_planes;
    two_planes.width = 16;
    two_planes.height = 16;
    two_planes.plane_count = 2;
    check(!r.supports_video(two_planes), std::string(name) + ": two-plane layouts are refused");

    std::vector<PixelFormat> formats;
    for (int i = 1; i < 256; ++i) {
        const PixelFormat f = static_cast<PixelFormat>(i);
        if (std::string(format_name(f)) == "none" || plane_count(f) == 0) break;
        formats.push_back(f);
    }
    const ColorMatrix matrices[] = { ColorMatrix::bt601, ColorMatrix::bt709, ColorMatrix::bt2020, ColorMatrix::rgb, ColorMatrix::unspecified };
    const ColorRange ranges[] = { ColorRange::limited, ColorRange::full };
    const std::uint8_t sitings[] = { 0, 1, 2 };
    std::uint32_t seed = 1;
    int cases = 0;
    gfx::VideoTexture tex;
    const std::uint64_t id = tex.id();

    for (PixelFormat fmt : formats) {
        const bool gray = plane_count(fmt) == 1;
        for (ColorMatrix m : matrices) {
            if (gray && m != ColorMatrix::bt601) continue;
            for (ColorRange rg : ranges) {
                for (std::uint8_t loc : sitings) {
                    if ((gray || (chroma_shift_x(fmt) == 0 && chroma_shift_y(fmt) == 0)) && loc != 0) continue;
                    const std::uint32_t w = 37 + (seed % 3) * 8;
                    const std::uint32_t h = 23 + (seed % 2) * 6;
                    VideoFrame f = make_frame(w, h, fmt, m, rg, loc, seed++);
                    const std::vector<std::uint8_t> want = to_rgba(f);
                    tex.set_filter((seed & 1) ? gfx::SampleFilter::Nearest : gfx::SampleFilter::Bilinear);
                    const std::string label = std::string(name) + " " + format_name(fmt) + " " + matrix_name(m) + (rg == ColorRange::full ? " full" : " limited") + " loc" + std::to_string(loc);
                    if (!r.supports_video(gfx::video_frame_view(f))) {
                        check(false, label + ": not supported by the renderer");
                        continue;
                    }
                    check(gfx::update_video_texture(tex, f), label + ": update");
                    bool drawn = false;
                    fizmo::images::BitmapImage img;
                    const bool ok = bench.render([&] { drawn = r.draw_video(tex, 5, 7); }, img);
                    check(ok && drawn, label + ": rendered");
                    if (!ok || !drawn) continue;
                    const Diff d = compare(img, 5, 7, want, w, h);
                    g_worst = std::max(g_worst, d.worst);
                    if (std::getenv("FIZMO_VIDEO_GPU_VERBOSE")) std::printf("%s: %s\n", label.c_str(), describe(d).c_str());
                    check(d.worst <= 2, label + ": " + describe(d));
                    ++cases;
                }
            }
        }
    }
    check(tex.id() == id, std::string(name) + ": the texture keeps its id across updates");
    std::printf("%s: %d conversion cases compared\n", name, cases);

    {
        VideoFrame a = make_frame(40, 30, PixelFormat::yuv420p, ColorMatrix::bt709, ColorRange::limited, 0, 901);
        VideoFrame b = make_frame(40, 30, PixelFormat::yuv420p, ColorMatrix::bt709, ColorRange::limited, 0, 902);
        const std::vector<std::uint8_t> wa = to_rgba(a);
        const std::vector<std::uint8_t> wb = to_rgba(b);
        gfx::VideoTexture t;
        t.set_filter(gfx::SampleFilter::Nearest);
        fizmo::images::BitmapImage img;
        bool d1 = false;
        bool d2 = false;
        const bool ok = bench.render([&] {
            gfx::update_video_texture(t, a);
            d1 = r.draw_video(t, 0, 0);
            gfx::update_video_texture(t, b);
            d2 = r.draw_video(t, 100, 0);
        }, img);
        check(ok && d1 && d2, std::string(name) + ": two versions in one frame rendered");
        if (ok) {
            check(compare(img, 0, 0, wa, 40, 30).worst <= 2, std::string(name) + ": first version kept for the earlier draw " + describe(compare(img, 0, 0, wa, 40, 30)));
            check(compare(img, 100, 0, wb, 40, 30).worst <= 2, std::string(name) + ": second version for the later draw " + describe(compare(img, 100, 0, wb, 40, 30)));
        }
        const bool ok2 = bench.render([&] { r.draw_video(t, 0, 0); }, img);
        check(ok2 && compare(img, 0, 0, wb, 40, 30).worst <= 2, std::string(name) + ": next frame shows the latest version");
        gfx::update_video_texture(t, a);
        const bool ok3 = bench.render([&] { r.draw_video(t, 0, 0); }, img);
        check(ok3 && compare(img, 0, 0, wa, 40, 30).worst <= 2, std::string(name) + ": updating in place shows the new frame");

        const bool ok4 = bench.render([&] { r.draw_video(t, 10, 10, 80, 60); }, img);
        const Diff ds = compare(img, 10, 10, wa, 40, 30, 2);
        check(ok4 && ds.worst <= 2, std::string(name) + ": nearest 2x scale " + describe(ds));

        const bool ok5 = bench.render([&] { r.draw_video(t, 3, 4, 40, 30, 0.5f); }, img);
        const Diff dh = compare(img, 3, 4, wa, 40, 30, 1, 0.5f);
        check(ok5 && dh.worst <= 2, std::string(name) + ": half opacity " + describe(dh));

        t.set_filter(gfx::SampleFilter::Bilinear);
        const bool ok6 = bench.render([&] { r.draw_video(t, 0, 0, 120, 90); }, img);
        check(ok6, std::string(name) + ": bilinear upscale renders");
        if (ok6) {
            const fizmo::colors::Color c = img.get_pixel(61, 46);
            check(c.red() + c.green() + c.blue() > 0, std::string(name) + ": bilinear upscale draws something");
        }
    }

    {
        VideoFrame f = make_frame(33, 17, PixelFormat::yuv422p10, ColorMatrix::bt2020, ColorRange::full, 1, 77);
        const std::vector<std::uint8_t> want = to_rgba(f);
        std::vector<std::vector<std::uint8_t>> copies(3);
        gfx::VideoFrameView v = gfx::video_frame_view(f);
        for (int p = 0; p < 3; ++p) {
            const std::size_t row = static_cast<std::size_t>(f.plane_width(p)) * 2;
            const std::size_t stride = row + 7;
            copies[static_cast<std::size_t>(p)].assign(stride * f.plane_height(p), 0);
            for (std::uint32_t y = 0; y < f.plane_height(p); ++y) std::memcpy(copies[static_cast<std::size_t>(p)].data() + y * stride, f.plane(p) + f.stride[p] * y, row);
            v.planes[p].data = copies[static_cast<std::size_t>(p)].data();
            v.planes[p].stride = stride;
        }
        gfx::VideoTexture t(v, gfx::SampleFilter::Nearest);
        check(t.valid() && t.plane_stride(0) % 2 == 0, std::string(name) + ": odd source strides are repacked");
        fizmo::images::BitmapImage img;
        const bool ok = bench.render([&] { r.draw_video(t, 2, 2); }, img);
        const Diff d = compare(img, 2, 2, want, 33, 17);
        check(ok && d.worst <= 2, std::string(name) + ": odd stride 10-bit " + describe(d));
    }

    {
        VideoFrame f = make_frame(24, 16, PixelFormat::yuv444p, ColorMatrix::bt601, ColorRange::full, 0, 55);
        const std::vector<std::uint8_t> want = to_rgba(f);
        gfx::VideoTexture t(gfx::video_frame_view(f), gfx::SampleFilter::Nearest);
        fizmo::images::BitmapImage img;
        const gfx::RenderPoint quad[4] = { { 50, 10 }, { 50, 34 }, { 34, 34 }, { 34, 10 } };
        const bool ok = bench.render([&] { r.draw_video_quad(t, quad); }, img);
        int worst = 0;
        if (ok) {
            for (std::uint32_t y = 0; y < 16; ++y) {
                for (std::uint32_t x = 0; x < 24; ++x) {
                    const fizmo::colors::Color c = img.get_pixel(34 + 15 - y, 10 + x);
                    const std::uint8_t* p = &want[(y * 24 + x) * 4];
                    worst = std::max({ worst, std::abs(c.red() - p[0]), std::abs(c.green() - p[1]), std::abs(c.blue() - p[2]) });
                }
            }
        }
        check(ok && worst <= 2, std::string(name) + ": rotated quad max diff " + std::to_string(worst));
    }

    r.unbind();
    win.close();
}

} // namespace

int main(int argc, char** argv) {
    const bool require = std::getenv("FIZMO_VIDEO_GPU_REQUIRE") != nullptr;
    const std::string only = argc > 1 ? argv[1] : "";
    const bool vulkan = only.empty() || only == "vulkan";
    const bool opengl = only.empty() || only == "opengl";
    fizmo::windows::Window vulkan_window(320, 240, "fizmo video gpu vulkan");
    fizmo::windows::Window opengl_window(320, 240, "fizmo video gpu opengl");
    const bool vulkan_created = vulkan && vulkan_window.create();
    const bool opengl_created = opengl && opengl_window.create();
    if (vulkan) run_backend(vulkan_window, vulkan_created, gfx::RendererBackend::Vulkan, "vulkan", require);
    if (opengl) run_backend(opengl_window, opengl_created, gfx::RendererBackend::OpenGL, "opengl", require);
    std::printf("video gpu tests: %d passed, %d failed, worst channel difference %d\n", g_passed, g_failed, g_worst);
    return g_failed == 0 ? 0 : 1;
}
