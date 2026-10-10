#ifndef FIZMO_RENDERER_BASE_HPP
#define FIZMO_RENDERER_BASE_HPP

#include "../../Graphics/color.hpp"
#include "../../Graphics/paint.hpp"
#include "../../Graphics/texture.hpp"
#include "../../Graphics/render_target.hpp"
#include "../../Graphics/draw_types_2d.hpp"
#include "../../Text/text_style.hpp"
#include "../../Text/rich_text.hpp"
#include "../raster_3d.hpp"
#include "../../GPU/types.hpp"
#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <array>

namespace fizmo {
namespace gpu {
class Device;
class CommandList;
class Texture;
} // namespace gpu

namespace windows {

enum class GpuPassStage : std::uint8_t { BeforeTargets = 0, AfterTargets, AfterScene };

struct GpuPassContext {
    gpu::Device*        device      = nullptr;
    gpu::CommandList*   commands    = nullptr;
    const gpu::Texture* back_buffer = nullptr;
    unsigned int        width       = 0;
    unsigned int        height      = 0;
    std::uint32_t       frame_index = 0;
    std::uint64_t       frame       = 0;
};

using GpuPassCallback = std::function<void(GpuPassContext&)>;

struct RenderPoint {
    float x = 0.0f;
    float y = 0.0f;

    constexpr RenderPoint() noexcept = default;

    template <typename X, typename Y, typename = typename std::enable_if<std::is_arithmetic<X>::value && std::is_arithmetic<Y>::value>::type>
    constexpr RenderPoint(X px, Y py) noexcept : x(static_cast<float>(px)), y(static_cast<float>(py)) {}
};

enum class GpuPass : std::uint8_t { Uploads = 0, Shadows, Volume, Reflections, Scene, Upscale, Overlay, Present, Count };

enum class UpscaleFilter : std::uint8_t { Nearest = 0, Bilinear, Sharp };

inline const char* gpu_pass_name(GpuPass pass) noexcept {
    switch (pass) {
        case GpuPass::Uploads:     return "Uploads";
        case GpuPass::Shadows:     return "Shadows";
        case GpuPass::Volume:      return "Volumetrics";
        case GpuPass::Reflections: return "Reflections";
        case GpuPass::Scene:       return "Scene";
        case GpuPass::Upscale:     return "Upscale";
        case GpuPass::Overlay:     return "Overlay";
        case GpuPass::Present:     return "Present";
        default:                   return "";
    }
}

struct GpuTimings {
    static constexpr std::size_t PASSES = static_cast<std::size_t>(GpuPass::Count);

    bool                          valid    = false;
    double                        total_ms = 0.0;
    std::array<double, PASSES>    pass_ms{};

    double ms(GpuPass pass) const noexcept { return pass_ms[static_cast<std::size_t>(pass)]; }
};

struct GpuMemory {
    bool          valid        = false;
    bool          measured     = false;
    std::uint64_t used         = 0;
    std::uint64_t budget       = 0;
    std::uint64_t total        = 0;
    std::uint64_t shared_used  = 0;
    std::uint64_t shared_total = 0;
};

namespace detail {

struct SpriteInstance {
    float                     x[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float                     y[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float                     u0 = 0.0f;
    float                     v0 = 0.0f;
    float                     u1 = 1.0f;
    float                     v1 = 1.0f;
    std::uint8_t              r  = 255;
    std::uint8_t              g  = 255;
    std::uint8_t              b  = 255;
    std::uint8_t              a  = 255;
    const graphics::Texture*  texture = nullptr;
    graphics::TextureRect     source;
    graphics::BlendMode       blend = graphics::BlendMode::Normal;
};

struct QuadBatchDraw {
    graphics::detail::MeshSlot3D* slot = nullptr;
    std::int32_t                  rel_cell[3] = { 0, 0, 0 };
    std::int32_t                  abs_cell[3] = { 0, 0, 0 };
    float                         frac[3]     = { 0.0f, 0.0f, 0.0f };
    graphics::FaceMask            faces       = graphics::ALL_FACE_GROUPS;
};

class RendererImplBase {
public:
    virtual ~RendererImplBase() noexcept = default;
    virtual bool initialize(void* native_handle, unsigned int width, unsigned int height) noexcept = 0;
    virtual void shutdown() noexcept = 0;
    virtual void resize(unsigned int width, unsigned int height) noexcept = 0;
    virtual void begin_frame() noexcept = 0;
    virtual void present() noexcept = 0;
    virtual void clear(const graphics::Color& color) noexcept = 0;
    virtual void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin = RectOrigin::TopLeft) noexcept = 0;
    virtual void reset_clip_rect() noexcept = 0;
    virtual void draw_pixel(int x, int y, const graphics::Color& color) noexcept = 0;
    virtual void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept = 0;
    virtual void paint(void* paint_dc) noexcept = 0;
    virtual void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& paint) noexcept = 0;
    virtual void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept = 0;
    virtual void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept = 0;
    virtual void draw_image(int dx, int dy, unsigned int dw, unsigned int dh, const images::BitmapImage& img, unsigned int sx, unsigned int sy, unsigned int sw, unsigned int sh) noexcept = 0;
    virtual void draw_texture(int dx, int dy, unsigned int dw, unsigned int dh, const graphics::Texture& tex, float opacity, const graphics::TextureRect& src) noexcept = 0;

    virtual void draw_text(
        int x, int y,
        const char* utf8, int len,
        const text::TextStyle& style
    ) noexcept = 0;

    virtual void draw_text(
        int x, int y,
        const wchar_t* str, int len,
        const text::TextStyle& style
    ) noexcept = 0;

    virtual text::TextMetrics measure_text(
        const char* utf8, int len,
        const text::TextStyle& style
    ) noexcept = 0;

    virtual text::TextMetrics measure_text(
        const wchar_t* str, int len,
        const text::TextStyle& style
    ) noexcept = 0;


    virtual const char* backend_name() const noexcept { return "software"; }
    virtual bool is_gpu() const noexcept { return false; }
    virtual void set_vsync(bool /*enabled*/) noexcept {}
    virtual void set_gpu_timing(bool) noexcept {}
    virtual void set_render_scale(float) noexcept {}
    virtual float render_scale() const noexcept { return 1.0f; }
    virtual void set_upscale(UpscaleFilter, float) noexcept {}
    virtual GpuTimings gpu_timings() const noexcept { return {}; }
    virtual GpuMemory gpu_memory() const noexcept { return {}; }
    virtual gpu::Caps device_caps() const { return {}; }

    virtual void draw_pixel_buffer(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const graphics::Color* pixels, unsigned int pw, unsigned int ph,
        bool /*smooth*/, std::uint64_t /*version*/
    ) noexcept {
        if (!pixels || pw == 0 || ph == 0 || dw == 0 || dh == 0) return;

        try {
            images::BitmapImage img(pw, ph);

            for (unsigned int y = 0; y < ph; ++y)
                for (unsigned int x = 0; x < pw; ++x)
                    img.set_pixel(x, y, pixels[static_cast<std::size_t>(y) * pw + x]);

            draw_image(dx, dy, dw, dh, img, 0, 0, pw, ph);
        } catch (...) {}
    }

    virtual void set_sprite_atlas(bool) noexcept {}
    virtual bool sprite_atlas() const noexcept { return false; }

    virtual void draw_sprite_instances(const SpriteInstance* sprites, std::size_t count) noexcept {
        if (!sprites) return;

        for (std::size_t i = 0; i < count; ++i) {
            const SpriteInstance& s = sprites[i];
            if (!s.texture || s.a == 0) continue;
            const RenderPoint quad[4] = { { s.x[0], s.y[0] }, { s.x[1], s.y[1] }, { s.x[2], s.y[2] }, { s.x[3], s.y[3] } };
            const graphics::BlendMode saved = blend_mode();
            if (s.blend != saved) set_blend_mode(s.blend);
            if (s.r == 255 && s.g == 255 && s.b == 255) draw_texture_quad(quad, *s.texture, s.a / 255.0f, s.source);
            else draw_texture_quad_tinted(quad, *s.texture, graphics::Color(s.r, s.g, s.b, 255), s.a / 255.0f, s.source);
            if (s.blend != saved) set_blend_mode(saved);
        }
    }

    virtual void set_blend_mode(graphics::BlendMode mode) noexcept { m_blend_mode = mode; }
    virtual graphics::BlendMode blend_mode() const noexcept { return m_blend_mode; }

    virtual void draw_vertices_2d(const graphics::Vertex2D* v, std::size_t count, const graphics::Texture* tex) noexcept {
        (void)tex;
        if (!v) return;

        for (std::size_t i = 0; i + 2 < count; i += 3) {
            unsigned int r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < 3; ++k) { r += v[i + k].color.red(); g += v[i + k].color.green(); b += v[i + k].color.blue(); a += v[i + k].color.alpha(); }
            if (a == 0) continue;
            const RenderPoint tri[3] = { { v[i].x, v[i].y }, { v[i + 1].x, v[i + 1].y }, { v[i + 2].x, v[i + 2].y } };
            const graphics::Color c(static_cast<std::uint8_t>(r / 3), static_cast<std::uint8_t>(g / 3), static_cast<std::uint8_t>(b / 3), static_cast<std::uint8_t>(a / 3));
            draw_polygon(tri, 3, graphics::Paint::fill(c));
        }
    }

    virtual bool draw_rich_text_transformed(const float*, float, float, unsigned int, unsigned int, const text::RichText&) noexcept { return false; }

    static void scale_style(text::TextStyle& st, double k, bool base) noexcept {
        if (st.has_size()) st.set_size(st.size() * k);
        else if (base) st.set_size(16.0 * k);
        if (st.has_letter_spacing()) st.set_letter_spacing(st.letter_spacing() * k);
        if (st.has_word_spacing()) st.set_word_spacing(st.word_spacing() * k);
        if (st.has_outline_width()) st.set_outline_width(st.outline_width() * k);
        if (st.has_indent()) st.set_indent(st.indent() * k);
        if (st.has_shadow()) { auto sh = st.shadow(); sh.offset_x *= k; sh.offset_y *= k; sh.blur *= k; st.set_shadow(sh); }
    }

    static text::RichText scaled_rich_text(const text::RichText& rt, double k) {
        text::RichText out = rt;
        scale_style(out.base(), k, true);
        for (text::TextSpan& sp : out.spans()) scale_style(sp.style, k, false);
        return out;
    }

    virtual void draw_texture_quad(
        const RenderPoint quad[4], const graphics::Texture& tex,
        float opacity, const graphics::TextureRect& src
    ) noexcept {
        draw_texture_quad_tinted(quad, tex, graphics::Color(255, 255, 255, 255), opacity, src);
    }

    virtual void draw_texture_quad_tinted(
        const RenderPoint quad[4], const graphics::Texture& tex, const graphics::Color& tint,
        float opacity, const graphics::TextureRect& src
    ) noexcept {
        if (!quad || !tex.valid() || src.is_empty() || opacity <= 0.0f) return;
        opacity = std::min(opacity, 1.0f) * (tint.alpha() / 255.0f);
        const bool tinted = tint.red() != 255 || tint.green() != 255 || tint.blue() != 255;
        opacity = std::min(opacity, 1.0f);
        float minx = quad[0].x, maxx = quad[0].x, miny = quad[0].y, maxy = quad[0].y;

        for (int i = 1; i < 4; ++i) {
            minx = std::min(minx, quad[i].x); maxx = std::max(maxx, quad[i].x);
            miny = std::min(miny, quad[i].y); maxy = std::max(maxy, quad[i].y);
        }

        const int x0 = static_cast<int>(std::floor(minx)), y0 = static_cast<int>(std::floor(miny));
        const int x1 = static_cast<int>(std::ceil(maxx)),  y1 = static_cast<int>(std::ceil(maxy));
        if (x1 <= x0 || y1 <= y0 || x1 - x0 > 16384 || y1 - y0 > 16384) return;
        const unsigned int bw = static_cast<unsigned int>(x1 - x0), bh = static_cast<unsigned int>(y1 - y0);
        const double e1x = quad[1].x - quad[0].x, e1y = quad[1].y - quad[0].y;
        const double e2x = quad[3].x - quad[0].x, e2y = quad[3].y - quad[0].y;
        const double det = e1x * e2y - e1y * e2x;
        if (std::abs(det) < 1e-9) return;
        const double inv = 1.0 / det;

        try {
            images::BitmapImage img(bw, bh);
            const graphics::Color clear(0, 0, 0, 0);

            for (unsigned int row = 0; row < bh; ++row) {
                const double qy = (y0 + static_cast<int>(row)) + 0.5 - quad[0].y;

                for (unsigned int col = 0; col < bw; ++col) {
                    const double qx = (x0 + static_cast<int>(col)) + 0.5 - quad[0].x;
                    const double s  = (qx * e2y - qy * e2x) * inv;
                    const double t  = (e1x * qy - e1y * qx) * inv;

                    if (s < 0.0 || s >= 1.0 || t < 0.0 || t >= 1.0) { img.set_pixel(col, row, clear); continue; }

                    graphics::Color c = tex.sample(src.x + s * src.w, src.y + t * src.h);
                    if (tinted) c = graphics::Color(static_cast<std::uint8_t>((c.red() * tint.red() + 127) / 255), static_cast<std::uint8_t>((c.green() * tint.green() + 127) / 255), static_cast<std::uint8_t>((c.blue() * tint.blue() + 127) / 255), c.alpha());
                    if (opacity < 1.0f) c.set_alpha(static_cast<std::uint8_t>(c.alpha() * opacity + 0.5f));
                    img.set_pixel(col, row, c);
                }
            }

            const graphics::Texture tmp(std::move(img));
            draw_texture(x0, y0, bw, bh, tmp, 1.0f, graphics::TextureRect(0, 0, bw, bh));
        } catch (...) {}
    }

    virtual void draw_polyline(const RenderPoint* pts, std::size_t count, bool closed, const graphics::Paint& p) noexcept {
        if (!pts || count < 2 || !p.has_stroke()) return;
        for (std::size_t i = 0; i + 1 < count; ++i) draw_segment(pts[i], pts[i + 1], p);
        if (closed && count > 2) draw_segment(pts[count - 1], pts[0], p);
    }

    virtual void draw_polygon(const RenderPoint* pts, std::size_t count, const graphics::Paint& p) noexcept {
        if (!pts || count < 3) return;

        if (p.has_fill()) {
            float ymin = pts[0].y, ymax = pts[0].y;
            for (std::size_t i = 1; i < count; ++i) { ymin = std::min(ymin, pts[i].y); ymax = std::max(ymax, pts[i].y); }
            const graphics::Paint span = graphics::Paint::stroke(p.fill_color(), 1);
            std::vector<float> xs;

            for (int y = static_cast<int>(std::floor(ymin)); y <= static_cast<int>(std::ceil(ymax)); ++y) {
                const float yc = static_cast<float>(y) + 0.5f;
                xs.clear();

                for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
                    const RenderPoint a = pts[j], b = pts[i];

                    if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc))
                        xs.push_back(a.x + (yc - a.y) / (b.y - a.y) * (b.x - a.x));
                }

                std::sort(xs.begin(), xs.end());

                for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                    const int x0 = static_cast<int>(std::ceil(xs[k] - 0.5f));
                    const int x1 = static_cast<int>(std::floor(xs[k + 1] - 0.5f));
                    if (x1 >= x0) draw_line(x0, y, x1, y, span);
                }
            }
        }
        if (p.has_stroke()) draw_polyline(pts, count, true, p);
    }

    virtual bool capture(images::BitmapImage& /*out*/) noexcept { return false; }

    virtual bool begin_target(const graphics::RenderTarget&, const graphics::Color*) noexcept { return false; }
    virtual void end_target() noexcept {}
    virtual std::uint64_t active_target() const noexcept { return 0; }
    virtual void draw_target(int, int, unsigned int, unsigned int, const graphics::RenderTarget&, float, const graphics::TextureRect&) noexcept {}
    virtual void draw_target_quad(const RenderPoint*, const graphics::RenderTarget&, float, const graphics::TextureRect&) noexcept {}
    virtual bool read_target(const graphics::RenderTarget&, images::BitmapImage&) noexcept { return false; }
    virtual gpu::Device* gpu_device() noexcept { return nullptr; }
    virtual const gpu::Texture* gpu_texture(const graphics::Texture&) noexcept { return nullptr; }
    virtual const gpu::Texture* gpu_texture(const graphics::RenderTarget&) noexcept { return nullptr; }
    virtual const gpu::Texture* gpu_back_buffer() noexcept { return nullptr; }
    virtual std::uint64_t add_gpu_pass(GpuPassStage, GpuPassCallback) { return 0; }
    virtual bool remove_gpu_pass(std::uint64_t) noexcept { return false; }

    virtual void draw_rich_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept {
        try { fallback_rich_text(x, y, w, h, rt, true); } catch (...) {}
    }

    virtual text::TextMetrics measure_rich_text(const text::RichText& rt, unsigned int max_width) noexcept {
        try { return fallback_rich_text(0, 0, max_width, 0, rt, false); } catch (...) { return {}; }
    }

    virtual bool load_font_file(const char* /*utf8_path*/) noexcept { return false; }

    virtual void begin_3d(const Scene3D& scene) noexcept {
        try {
            if (!m_soft3d) m_soft3d.reset(new SoftwareRasterizer3D());
            m_soft3d->set_light(m_light3d);
            m_soft3d->set_scene_lighting(&m_scene_lighting);
            m_soft3d->begin(scene);
            m_in_3d = true;
        } catch (...) {
            m_in_3d = false;
        }
    }

    virtual void end_3d() noexcept {
        if (!m_in_3d || !m_soft3d) return;
        m_in_3d = false;
        if (!m_soft3d->touched()) return;

        try {
            m_soft3d->resolve(m_soft3d_pixels);
            const Scene3D& s = m_soft3d->scene();
            draw_pixel_buffer(s.x, s.y, s.width, s.height, m_soft3d_pixels.data(), s.width, s.height, false, ++m_soft3d_version);
        } catch (...) {}
    }

    virtual void set_light_3d(const graphics::Light3D& light) noexcept {
        m_light3d = light;
        if (m_soft3d) m_soft3d->set_light(light);
    }

    virtual void set_scene_lighting(const graphics::SceneLighting3D& lighting) noexcept {
        try {
            m_scene_lighting = lighting;
            if (m_soft3d) m_soft3d->set_scene_lighting(&m_scene_lighting);
        } catch (...) {}
    }

    const graphics::SceneLighting3D& scene_lighting() const noexcept { return m_scene_lighting; }

    virtual void draw_mesh_3d(const graphics::Mesh3D& mesh, const float* model, const graphics::Material3D& mat) noexcept {
        if (!m_in_3d || mesh.empty() || !mat.camera_visible()) return;
        const auto& v = mesh.vertices();
        const auto& i = mesh.indices();
        try { m_soft3d->draw(v.data(), v.size(), mesh.indexed() ? i.data() : nullptr, i.size(), model, mat); } catch (...) {}
    }

    virtual void draw_triangles_3d(const graphics::Vertex3D* v, std::size_t count, const float* model, const graphics::Material3D& mat) noexcept {
        if (!m_in_3d || !v || count < 3 || !mat.camera_visible()) return;
        try { m_soft3d->draw(v, count, nullptr, 0, model, mat); } catch (...) {}
    }

    virtual void draw_quads_3d(const graphics::QuadMesh3D& quads, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) noexcept {
        if (!m_in_3d || quads.empty() || !mat.camera_visible()) return;
        try { m_soft3d->draw_quads(quads.vertices().data(), quads.quad_count(), model, cell_origin, mat); } catch (...) {}
    }

    virtual void upload_handle_3d(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot) noexcept {
        if (!slot) return;
        slot->owner_epoch = m_epoch;
        slot->resident = false;
        if (!slot->has_cpu_data()) slot->lost = slot->element_count > 0;
    }

    virtual void draw_handle_3d(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) noexcept {
        if (!m_in_3d || !slot || slot->element_count == 0 || !mat.camera_visible()) return;
        if (!slot->has_cpu_data()) { slot->lost = true; return; }

        try {
            if (slot->quads) {
                m_soft3d->draw_quads(slot->quad_mesh.vertices().data(), slot->quad_mesh.quad_count(), model, cell_origin, mat);
            } else {
                const auto& v = slot->mesh.vertices();
                const auto& i = slot->mesh.indices();
                m_soft3d->draw(v.data(), v.size(), slot->mesh.indexed() ? i.data() : nullptr, i.size(), model, mat);
            }
        } catch (...) {}
    }

    virtual void draw_lines_3d(const vector3d* pts, std::size_t count, const graphics::Color& color, float width, bool depth_test) noexcept {
        if (!m_in_3d || !pts || count < 2) return;
        try { m_soft3d->lines(pts, count, color, width, depth_test); } catch (...) {}
    }

    bool in_3d() const noexcept { return m_in_3d; }

    virtual std::uint64_t gpu_mesh_bytes() const noexcept { return 0; }

    virtual void draw_quad_batch_3d(const QuadBatchDraw* items, std::size_t count, const float* camera_frac, const graphics::Material3D& mat) noexcept {
        if (!m_in_3d || !items || !mat.camera_visible()) return;

        try {
            for (std::size_t i = 0; i < count; ++i) {
                const QuadBatchDraw& item = items[i];
                graphics::detail::MeshSlot3D* slot = item.slot;
                if (!slot || !slot->quads || slot->element_count == 0) continue;
                if (!slot->has_cpu_data()) { slot->lost = true; continue; }
                float model[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                for (int a = 0; a < 3; ++a) model[a * 4 + 3] = static_cast<float>(item.rel_cell[a]) + (item.frac[a] - camera_frac[a]);
                const graphics::CompactVertex3D* v = slot->quad_mesh.vertices().data();

                for (std::size_t g = 0; g < graphics::CELL_FACE_GROUPS; ++g) {
                    const graphics::QuadRange r = slot->groups[g];
                    if (r.count == 0 || !(item.faces & (1u << g))) continue;
                    m_soft3d->draw_quads(v + r.first * graphics::QuadMesh3D::VERTICES_PER_QUAD, r.count, model, item.abs_cell, mat);
                }
            }
        } catch (...) {}
    }

    virtual void draw_instances_3d(const graphics::Mesh3D& mesh, const float* model, const graphics::Instance3D* instances,
                                   std::size_t count, const graphics::Material3D& mat) noexcept {
        if (!m_in_3d || mesh.empty() || !instances || count == 0 || !mat.camera_visible()) return;

        try {
            const auto& verts = mesh.vertices();
            const auto& idx = mesh.indices();
            const std::size_t per = mesh.indexed() ? idx.size() : verts.size();
            m_instance_scratch.clear();
            m_instance_scratch.reserve(per * count);
            m_instance_lights.clear();
            m_instance_lights.reserve(per * count);

            for (std::size_t i = 0; i < count; ++i) {
                const graphics::Instance3D& in = instances[i];
                const float tint[4] = { (in.rgba & 0xFF) / 255.0f, ((in.rgba >> 8) & 0xFF) / 255.0f, ((in.rgba >> 16) & 0xFF) / 255.0f, (in.rgba >> 24) / 255.0f };

                for (std::size_t k = 0; k < per; ++k) {
                    graphics::Vertex3D v = verts[mesh.indexed() ? idx[k] : k];
                    v.x = v.x * in.scale + in.x; v.y = v.y * in.scale + in.y; v.z = v.z * in.scale + in.z;
                    const graphics::Color c = v.color();
                    v.set_color(graphics::Color(static_cast<std::uint8_t>(c.red() * tint[0] + 0.5f), static_cast<std::uint8_t>(c.green() * tint[1] + 0.5f),
                                                static_cast<std::uint8_t>(c.blue() * tint[2] + 0.5f), static_cast<std::uint8_t>(c.alpha() * tint[3] + 0.5f)));
                    m_instance_scratch.push_back(v);
                    m_instance_lights.push_back(in.light);
                }
            }

            m_soft3d->draw(m_instance_scratch.data(), m_instance_scratch.size(), nullptr, 0, model, mat, m_instance_lights.data());
        } catch (...) {}
    }

protected:
    graphics::Light3D                     m_light3d;
    graphics::SceneLighting3D             m_scene_lighting;
    bool                                  m_in_3d = false;
    std::unique_ptr<SoftwareRasterizer3D> m_soft3d;
    std::vector<graphics::Color>          m_soft3d_pixels;
    std::uint64_t                         m_soft3d_version = 0;
    const std::uint64_t                   m_epoch = next_content_version();
    std::vector<graphics::Vertex3D>       m_instance_scratch;
    std::vector<std::uint32_t>            m_instance_lights;
    graphics::BlendMode                   m_blend_mode = graphics::BlendMode::Normal;

public:

private:
    struct FallbackPiece {
        std::string     text;
        text::TextStyle style;
        int             width   = 0;
        int             ascent  = 0;
        int             descent = 0;
        int             rise    = 0;
    };

    struct FallbackLine {
        std::vector<FallbackPiece> pieces;
        int  width    = 0;
        int  ascent   = 0;
        int  descent  = 0;
        bool para_end = false;
    };

    static std::string fallback_transform(std::string s, text::TextTransform t) {
        bool start = true;

        for (char& ch : s) {
            const unsigned char u = static_cast<unsigned char>(ch);
            if (u >= 0x80) { start = false; continue; }
            if (t == text::TextTransform::Uppercase) ch = static_cast<char>(std::toupper(u));
            else if (t == text::TextTransform::Lowercase) ch = static_cast<char>(std::tolower(u));
            else if (t == text::TextTransform::Capitalize && start && std::isalpha(u)) ch = static_cast<char>(std::toupper(u));
            start = !std::isalnum(u);
        }

        return s;
    }

    FallbackPiece fallback_piece(std::string s, const text::TextStyle& st, int rise) {
        const text::TextMetrics m = measure_text(s.c_str(), static_cast<int>(s.size()), st);
        FallbackPiece p;
        p.text    = std::move(s);
        p.style   = st;
        p.width   = static_cast<int>(m.width);
        p.ascent  = m.ascent;
        p.descent = m.descent;
        p.rise    = rise;
        return p;
    }

    static void fallback_add(FallbackLine& L, FallbackPiece p) {
        L.width  += p.width;
        L.ascent  = std::max(L.ascent, p.ascent - p.rise);
        L.descent = std::max(L.descent, p.descent + p.rise);
        L.pieces.push_back(std::move(p));
    }

    text::TextMetrics fallback_rich_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt, bool draw) {
        text::TextMetrics result;
        if (rt.empty()) return result;
        const text::TextStyle& para = rt.base();
        const text::TextOverflow overflow = para.has_text_overflow() ? para.text_overflow() : text::TextOverflow::Visible;
        const bool wrap = overflow == text::TextOverflow::WordWrap && w > 0;
        const double mult = para.has_line_height() && para.line_height() > 0.0 ? para.line_height() : 1.0;
        const int para_gap = para.has_paragraph_spacing() ? static_cast<int>(std::lround(para.paragraph_spacing())) : 0;
        std::vector<FallbackLine> lines(1);
        text::TextStyle last_style = para;

        for (std::size_t i = 0; i < rt.size(); ++i) {
            text::TextStyle st = rt.resolved(i);
            st.clear_text_align();
            int rise = 0;

            if (st.has_vertical_align() && (st.vertical_align() == text::VerticalAlign::Superscript || st.vertical_align() == text::VerticalAlign::Subscript)) {
                const double size = st.has_size() && st.size() > 0.0 ? st.size() : 16.0;
                rise = static_cast<int>(std::lround(st.vertical_align() == text::VerticalAlign::Superscript ? -size * 0.35 : size * 0.15));
                st.set_size(size * 0.7);
                st.clear_vertical_align();              
            }

            std::string t;
            for (char ch : rt.spans()[i].text) if (ch != '\r') t += ch;
            if (st.has_transform()) t = fallback_transform(std::move(t), st.transform());
            last_style = st;
            std::size_t p = 0;

            while (p < t.size()) {
                if (t[p] == '\n') { lines.back().para_end = true; lines.emplace_back(); ++p; continue; }
                const bool space = t[p] == ' ';
                std::size_t q = p;
                while (q < t.size() && t[q] != '\n' && (t[q] == ' ') == space) ++q;
                FallbackPiece piece = fallback_piece(t.substr(p, q - p), st, rise);
                p = q;

                if (wrap && !space && !lines.back().pieces.empty() && lines.back().width + piece.width > static_cast<int>(w)) {
                    while (!lines.back().pieces.empty() && lines.back().pieces.back().text[0] == ' ') {
                        lines.back().width -= lines.back().pieces.back().width;
                        lines.back().pieces.pop_back();
                    }

                    lines.emplace_back();
                }

                const bool wrapped_start = lines.size() > 1 && !lines[lines.size() - 2].para_end;
                if (space && lines.back().pieces.empty() && wrapped_start) continue;   
                fallback_add(lines.back(), std::move(piece));
            }
        }

        const text::TextMetrics blank = measure_text(" ", 1, para);

        for (FallbackLine& L : lines) {
            if (L.pieces.empty()) { L.ascent = blank.ascent; L.descent = blank.descent; }
        }

        auto line_height = [&](const FallbackLine& L) { return static_cast<int>(std::lround((L.ascent + L.descent) * mult)); };
        std::size_t limit = lines.size();
        if (para.has_max_lines()) limit = std::min<std::size_t>(limit, para.max_lines());

        if (h > 0 && overflow != text::TextOverflow::Visible) {
            int used = 0;
            std::size_t fit = 0;

            for (std::size_t i = 0; i < lines.size(); ++i) {
                used += line_height(lines[i]);
                if (used > static_cast<int>(h)) break;
                fit = i + 1;
                if (lines[i].para_end) used += para_gap;
            }

            limit = std::min(limit, std::max<std::size_t>(fit, 1));
        }

        const bool ellipsize = overflow == text::TextOverflow::WordWrap || overflow == text::TextOverflow::Ellipsis;
        const std::string ell = "\xE2\x80\xA6";

        auto add_ellipsis = [&](FallbackLine& L) {
            const text::TextStyle st = L.pieces.empty() ? last_style : L.pieces.back().style;
            FallbackPiece e = fallback_piece(ell, st, L.pieces.empty() ? 0 : L.pieces.back().rise);

            if (w > 0) {
                while (!L.pieces.empty() && (L.width + e.width > static_cast<int>(w) || L.pieces.back().text[0] == ' ')) {
                    L.width -= L.pieces.back().width;
                    L.pieces.pop_back();
                }
            }

            fallback_add(L, std::move(e));
        };

        if (limit < lines.size()) {
            lines.resize(limit);
            if (ellipsize) add_ellipsis(lines.back());
        }

        if (overflow == text::TextOverflow::Ellipsis && w > 0) {
            for (FallbackLine& L : lines) if (L.width > static_cast<int>(w)) add_ellipsis(L);
        }

        int widest = 0, total = 0;

        for (std::size_t i = 0; i < lines.size(); ++i) {
            widest = std::max(widest, lines[i].width);
            total += line_height(lines[i]);
            if (lines[i].para_end && i + 1 < lines.size()) total += para_gap;
        }

        result.width   = static_cast<unsigned int>(widest);
        result.height  = static_cast<unsigned int>(total);
        result.ascent  = lines.front().ascent;
        result.descent = lines.front().descent;
        if (!draw) return result;

        const text::TextAlign align = para.has_text_align() ? para.text_align() : text::TextAlign::Left;
        const int region = w > 0 ? static_cast<int>(w) : widest;
        int top = y;

        if (h > 0 && para.has_vertical_align()) {
            if (para.vertical_align() == text::VerticalAlign::Middle) top += (static_cast<int>(h) - total) / 2;
            else if (para.vertical_align() == text::VerticalAlign::Bottom) top += static_cast<int>(h) - total;
        }

        const bool clip = overflow == text::TextOverflow::Clip && w > 0 && h > 0;
        if (clip) set_clip_rect(x, y, w, h, RectOrigin::TopLeft);

        for (const FallbackLine& L : lines) {
            const int lh = line_height(L);
            const int baseline = top + (lh - (L.ascent + L.descent)) / 2 + L.ascent;
            int pen = x;
            if (align == text::TextAlign::Center) pen += (region - L.width) / 2;
            else if (align == text::TextAlign::Right) pen += region - L.width;
            if (w == 0 && align == text::TextAlign::Center) pen -= region / 2;
            else if (w == 0 && align == text::TextAlign::Right) pen -= region;

            for (const FallbackPiece& p : L.pieces) {
                draw_text(pen, baseline + p.rise - p.ascent, p.text.c_str(), static_cast<int>(p.text.size()), p.style);
                pen += p.width;
            }

            top += lh + (L.para_end ? para_gap : 0);
        }

        if (clip) reset_clip_rect();
        return result;
    }

private:
    void draw_segment(RenderPoint a, RenderPoint b, const graphics::Paint& p) noexcept {
        draw_line(
            static_cast<int>(std::lround(a.x)), static_cast<int>(std::lround(a.y)),
            static_cast<int>(std::lround(b.x)), static_cast<int>(std::lround(b.y)), p
        );
    }
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_RENDERER_BASE_HPP