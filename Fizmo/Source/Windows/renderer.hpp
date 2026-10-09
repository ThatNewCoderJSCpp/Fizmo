#ifndef FIZMO_RENDERER_HPP
#define FIZMO_RENDERER_HPP

#include "../Graphics/color.hpp"
#include "window.hpp"
#include "../Graphics/framebuffer.hpp"
#include "../Graphics/sprite.hpp"
#include "../Graphics/sprite_batch.hpp"
#include "../Graphics/camera_2d.hpp"
#include "../Graphics/path_2d.hpp"
#include "../Graphics/nine_slice.hpp"
#include "../Graphics/tilemap.hpp"
#include "../Graphics/particles_2d.hpp"
#include "../Graphics/trail_2d.hpp"
#include "../Graphics/camera_3d.hpp"
#include "../Graphics/mesh.hpp"
#include "Renderer Impl/renderer_base.hpp"
#include "Renderer Impl/renderer_factory.hpp"
#include "../GPU/device.hpp"
#include "../Text/fancy_text_fwd.hpp"

#include <chrono>
#include <cstring>
#include <string>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

namespace fizmo {
namespace windows {

enum class RendererBackend {
    Auto = 0,
    GPU,
    Software,
    Vulkan,
    OpenGL
};

class Renderer;

struct FrameStats {
    std::uint64_t index       = 0;
    double        interval_ms = 0.0;
    double        cpu_ms      = 0.0;
    double        present_ms  = 0.0;
    bool          gpu_valid   = false;
    double        gpu_ms      = 0.0;
};

class FrameObserver {
public:
    virtual ~FrameObserver() = default;
    virtual void on_frame(const Renderer& renderer, const FrameStats& stats) noexcept = 0;
    virtual void on_unbind(const Renderer&) noexcept {}
};

class Renderer {
private:
    using FrameClock = std::chrono::steady_clock;

    Window* m_window;
    std::unique_ptr<detail::RendererImplBase> m_impl;
    RendererBackend m_requested = RendererBackend::Auto;
    bool m_vsync = true;
    bool m_gpu_timing = false;
    bool m_sprite_atlas = true;

    FrameClock::time_point                    m_frame_begin{};
    FrameClock::time_point                    m_last_present{};
    bool                                      m_frame_open  = false;
    bool                                      m_has_present = false;
    FrameStats                                m_frame_stats;
    std::vector<std::weak_ptr<FrameObserver>> m_observers;

    struct PassRecord {
        std::uint64_t   id      = 0;
        std::uint64_t   impl_id = 0;
        GpuPassStage    stage   = GpuPassStage::AfterScene;
        GpuPassCallback callback;
    };

    struct TargetFrame {
        math::Matrix3d              xform;
        std::vector<math::Matrix3d> stack;
        int                         kind = 0;
        unsigned int                width = 0;
        unsigned int                height = 0;
    };

    std::vector<PassRecord>  m_passes;
    std::uint64_t            m_next_pass = 1;
    std::vector<TargetFrame> m_target_frames;
    std::shared_ptr<void>    m_fancy_cache;

public:
    explicit Renderer(Window& window, RendererBackend backend = RendererBackend::Auto) noexcept
        : m_window(&window), m_impl(nullptr), m_requested(backend) {}

    ~Renderer() { unbind(); }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = default;
    Renderer& operator=(Renderer&&) = default;

    bool bind() noexcept;

    template <typename Factory>
    void try_backend(void* handle, Factory make) noexcept {
        try {
            std::unique_ptr<detail::RendererImplBase> impl = make();
            if (!impl) return;
            impl->set_vsync(m_vsync);
            impl->set_gpu_timing(m_gpu_timing);
            impl->set_sprite_atlas(m_sprite_atlas);
            impl->set_blend_mode(m_blend);
            if (impl->initialize(handle, m_window->width(), m_window->height())) m_impl = std::move(impl);
        } catch (...) {}
    }

    void unbind() noexcept {
        const bool was_bound = m_impl != nullptr;
        release();
        if (was_bound) notify_unbind();
    }

    void add_frame_observer(const std::shared_ptr<FrameObserver>& observer);

    void remove_frame_observer(const FrameObserver* observer) noexcept;

    const FrameStats& frame_stats() const noexcept { return m_frame_stats; }

    gpu::Caps device_caps() const {
        if (!m_impl) return {};
        try { return m_impl->device_caps(); } catch (...) { return {}; }
    }

    bool is_bound() const noexcept { return m_impl != nullptr; }
    bool is_gpu() const noexcept { return m_impl && m_impl->is_gpu(); }
    const char* backend_name() const noexcept { return m_impl ? m_impl->backend_name() : "none"; }

    RendererBackend backend() const noexcept;

    RendererBackend active_backend() const noexcept;

    RendererBackend requested_backend() const noexcept { return m_requested; }

    void set_backend(RendererBackend backend) noexcept {
        m_requested = backend;
        if (m_impl) bind();
    }

    void set_vsync(bool enabled) noexcept {
        m_vsync = enabled;
        if (m_impl) m_impl->set_vsync(enabled);
    }

    void set_gpu_timing(bool enabled) noexcept {
        m_gpu_timing = enabled;
        if (m_impl) m_impl->set_gpu_timing(enabled);
    }

    bool       gpu_timing()  const noexcept { return m_gpu_timing; }
    GpuTimings gpu_timings() const noexcept { return m_impl ? m_impl->gpu_timings() : GpuTimings{}; }
    GpuMemory  gpu_memory()  const noexcept { return m_impl ? m_impl->gpu_memory() : GpuMemory{}; }

    void  set_render_scale(float scale) noexcept { if (m_impl) m_impl->set_render_scale(scale); }
    float render_scale() const noexcept { return m_impl ? m_impl->render_scale() : 1.0f; }
    void  set_upscale(UpscaleFilter filter, float sharpness) noexcept { if (m_impl) m_impl->set_upscale(filter, sharpness); }

    bool vsync() const noexcept { return m_vsync; }
    void begin_frame() noexcept;

    void present() noexcept;

    void clear(const graphics::Color& color = graphics::Color()) noexcept { if (m_impl) m_impl->clear(color); }
    bool capture(images::BitmapImage& out) noexcept { return m_impl && m_impl->capture(out); }

    bool begin_target(const graphics::RenderTarget& target) noexcept { return open_target(target, nullptr); }
    bool begin_target(const graphics::RenderTarget& target, const graphics::Color& clear_color) noexcept { return open_target(target, &clear_color); }

    void end_target() noexcept;

    bool in_target() const noexcept { return !m_target_frames.empty(); }
    std::size_t target_depth() const noexcept { return m_target_frames.size(); }

    void draw_target(const graphics::RenderTarget& target, int dx, int dy, float opacity = 1.0f) noexcept {
        draw_target(target, dx, dy, target.width(), target.height(), opacity, target.full_rect());
    }

    void draw_target(const graphics::RenderTarget& target, int dx, int dy, unsigned int dw, unsigned int dh, float opacity = 1.0f) noexcept {
        draw_target(target, dx, dy, dw, dh, opacity, target.full_rect());
    }

    void draw_target(const graphics::RenderTarget& target, int dx, int dy, unsigned int dw, unsigned int dh, float opacity, const graphics::TextureRect& src) noexcept;

    bool read_target(const graphics::RenderTarget& target, images::BitmapImage& out) noexcept { return m_impl && m_impl->read_target(target, out); }

    gpu::Device* gpu_device() noexcept { return m_impl ? m_impl->gpu_device() : nullptr; }
    const gpu::Texture* gpu_texture(const graphics::Texture& texture) noexcept { return m_impl ? m_impl->gpu_texture(texture) : nullptr; }
    const gpu::Texture* gpu_texture(const graphics::RenderTarget& target) noexcept { return m_impl ? m_impl->gpu_texture(target) : nullptr; }
    const gpu::Texture* gpu_back_buffer() noexcept { return m_impl ? m_impl->gpu_back_buffer() : nullptr; }

    std::uint64_t add_gpu_pass(GpuPassStage stage, GpuPassCallback callback);

    bool remove_gpu_pass(std::uint64_t id) noexcept;

    void push_transform() { m_xform_stack.push_back(m_xform); }

    void pop_transform() noexcept;

    void set_transform(const math::Matrix3d& m) noexcept { m_xform = m; classify_transform(); }
    void reset_transform() noexcept { m_xform = math::Matrix3d::identity(); m_xform_kind = XformKind::Identity; }
    void transform(const math::Matrix3d& m) noexcept { m_xform = m_xform * m; classify_transform(); }
    void translate(double dx, double dy) noexcept { transform(math::Matrix3d::translation_2d(dx, dy)); }
    void rotate(double angle, bool degrees = true) noexcept { transform(math::Matrix3d::rotation_2d(angle, degrees)); }
    void scale(double sx, double sy) noexcept { transform(math::Matrix3d{ sx, 0.0, 0.0, 0.0, sy, 0.0, 0.0, 0.0, 1.0 }); }
    void scale(double s) noexcept { scale(s, s); }

    const math::Matrix3d& current_transform() const noexcept { return m_xform; }
    std::size_t transform_depth() const noexcept { return m_xform_stack.size(); }

    vector2d map_point(double x, double y) const noexcept {
        const auto& m = m_xform.data;
        return { m[0] * x + m[1] * y + m[2], m[3] * x + m[4] * y + m[5] };
    }

    vector2d inverse_map_point(double sx, double sy) const noexcept;

    void begin_2d(const graphics::Camera2D& camera) { push_transform(); transform(camera.view_matrix()); }
    void end_2d() noexcept { pop_transform(); }

    void blit_framebuffer(const graphics::Framebuffer& fb) noexcept {
        draw_framebuffer(fb, 0, 0, fb.width(), fb.height());
    }

    void draw_framebuffer(
        const graphics::Framebuffer& fb,
        int x, int y, unsigned int w, unsigned int h,
        bool smooth = false
    ) noexcept;

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin = RectOrigin::TopLeft) noexcept;

    void reset_clip_rect() noexcept { if (m_impl) m_impl->reset_clip_rect(); }

    void draw_pixel(int x, int y, const fizmo::graphics::Color& color) noexcept;

    void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept;

    void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& paint) noexcept;

    void draw_rect_corners(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept;

    void draw_circle(int cx, int cy, unsigned int radius, const graphics::Paint& paint) noexcept { draw_ellipse(cx, cy, radius, radius, paint); }

    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept;

    void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept;

    void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, const graphics::Paint& paint) noexcept {
        const RenderPoint pts[3] = { { x1, y1 }, { x2, y2 }, { x3, y3 } };
        draw_polygon(pts, 3, paint);
    }

    void draw_polygon(const RenderPoint* points, std::size_t count, const graphics::Paint& paint) noexcept {
        if (!m_impl || !points) return;
        m_impl->draw_polygon(map_points(points, count), count, paint);
    }

    void draw_polygon(const std::vector<RenderPoint>& points, const graphics::Paint& paint) noexcept { draw_polygon(points.data(), points.size(), paint); }
    void draw_polygon(std::initializer_list<RenderPoint> points, const graphics::Paint& paint) noexcept { draw_polygon(points.begin(), points.size(), paint); }

    void draw_polyline(const RenderPoint* points, std::size_t count, const graphics::Paint& paint, bool closed = false) noexcept;

    void draw_polyline(const std::vector<RenderPoint>& points, const graphics::Paint& paint, bool closed = false) noexcept { draw_polyline(points.data(), points.size(), paint, closed); }
    void draw_polyline(std::initializer_list<RenderPoint> points, const graphics::Paint& paint, bool closed = false) noexcept { draw_polyline(points.begin(), points.size(), paint, closed); }

    void draw_path(const graphics::Path2D& path, const graphics::Paint& paint, double tolerance = 0.5) noexcept;

    void draw_image(const fizmo::images::BitmapImage& img, int dx, int dy) noexcept {
        draw_image(img, dx, dy, img.width(), img.height(), 0, 0, img.width(), img.height());
    }

    void draw_image(const fizmo::images::BitmapImage& img, int dx, int dy, unsigned int dw, unsigned int dh) noexcept {
        draw_image(img, dx, dy, dw, dh, 0, 0, img.width(), img.height());
    }

    void draw_image(
        const fizmo::images::BitmapImage& img,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        unsigned int sx, unsigned int sy,
        unsigned int sw, unsigned int sh
    ) noexcept;

    void draw_texture(const graphics::Texture& tex, int dx, int dy, float opacity = 1.0f) noexcept;

    void draw_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, float opacity = 1.0f) noexcept {
        if (tex.valid()) emit_texture(tex, dx, dy, dw, dh, opacity, tex.full_rect());
    }

    void draw_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, float opacity, const graphics::TextureRect& src) noexcept {
        if (tex.valid()) emit_texture(tex, dx, dy, dw, dh, opacity, src);
    }

    void draw_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, const graphics::TextureRect& src) noexcept {
        if (tex.valid()) emit_texture(tex, dx, dy, dw, dh, 1.0f, src);
    }

    void draw_texture_quad(const graphics::Texture& tex, const RenderPoint quad[4], float opacity = 1.0f) noexcept {
        if (tex.valid()) draw_texture_quad(tex, quad, opacity, tex.full_rect());
    }

    void draw_texture_quad(const graphics::Texture& tex, const RenderPoint quad[4], float opacity, const graphics::TextureRect& src) noexcept;

    void draw_sprite(const graphics::Sprite& sprite) noexcept { draw_sprite(sprite, 0.0, 0.0); }

    void draw_sprite(const graphics::Sprite& sprite, double offset_x, double offset_y) noexcept;

    void set_blend_mode(graphics::BlendMode mode) noexcept { m_blend = mode; if (m_impl) m_impl->set_blend_mode(mode); }
    graphics::BlendMode blend_mode() const noexcept { return m_blend; }

    class BlendScope {
    private:
        Renderer*           m_renderer;
        graphics::BlendMode m_saved;

    public:
        BlendScope(Renderer& r, graphics::BlendMode mode) noexcept : m_renderer(&r), m_saved(r.blend_mode()) { r.set_blend_mode(mode); }
        ~BlendScope() { m_renderer->set_blend_mode(m_saved); }
        BlendScope(const BlendScope&) = delete;
        BlendScope& operator=(const BlendScope&) = delete;
    };

    void draw_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, const graphics::TextureRect& src, const graphics::Color& tint, float opacity = 1.0f) noexcept;

    void draw_texture(const graphics::Texture& tex, int dx, int dy, const graphics::Color& tint, float opacity = 1.0f) noexcept {
        draw_texture(tex, dx, dy, tex.width(), tex.height(), tex.full_rect(), tint, opacity);
    }

    void draw_texture_quad(const graphics::Texture& tex, const RenderPoint quad[4], const graphics::TextureRect& src, const graphics::Color& tint, float opacity = 1.0f) noexcept;

    void draw_vertices(const graphics::Vertex2D* vertices, std::size_t count, const graphics::Texture* texture = nullptr) noexcept;

    void draw_vertices(const std::vector<graphics::Vertex2D>& vertices, const graphics::Texture* texture = nullptr) noexcept {
        draw_vertices(vertices.data(), vertices.size(), texture);
    }

    unsigned int viewport_width() const noexcept;
    unsigned int viewport_height() const noexcept;

    void visible_world_rect(double& x0, double& y0, double& x1, double& y1) const noexcept;

    void draw_nine_slice(const graphics::NineSlice& slice, double x, double y, double w, double h, const graphics::Color& tint = graphics::Color(255, 255, 255, 255), float opacity = 1.0f) noexcept;

    std::size_t draw_tilemap(const graphics::TileMap& map, double x = 0.0, double y = 0.0) noexcept;

    std::size_t draw_particles(const graphics::ParticleEmitter& emitter) noexcept;

    void draw_trail(graphics::Trail& trail) noexcept;

    void draw_rounded_rect(double x, double y, double w, double h, double radius, const graphics::Paint& paint) noexcept;

    void draw_rounded_rect(double x, double y, double w, double h, double r_tl, double r_tr, double r_br, double r_bl, const graphics::Paint& paint) noexcept;

    void draw_sprites(const graphics::Sprite* const* sprites, std::size_t count) noexcept;

    void draw_sprites(const std::vector<graphics::Sprite>& sprites) noexcept;

    void set_sprite_atlas(bool enabled) noexcept {
        m_sprite_atlas = enabled;
        if (m_impl) m_impl->set_sprite_atlas(enabled);
    }

    bool sprite_atlas() const noexcept { return m_sprite_atlas; }

    void draw_sprite_batch(const graphics::SpriteBatch& batch, double offset_x = 0.0, double offset_y = 0.0) noexcept;

    void set_text_follows_transform(bool on) noexcept { m_text_follow = on; }
    bool text_follows_transform() const noexcept { return m_text_follow; }

    bool transformed_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept;

    static std::string wide_to_utf8(const wchar_t* s, int len);

    void draw_text(int x, int y, const char* utf8, int len, const text::TextStyle& style) noexcept;

    void draw_text(int x, int y, const wchar_t* str, int len, const text::TextStyle& style) noexcept;

    void draw_text(int x, int y, const std::string& text, const text::TextStyle& style) noexcept { draw_text(x, y, text.c_str(), static_cast<int>(text.size()), style); }
    void draw_text(int x, int y, const std::wstring& text, const text::TextStyle& style) noexcept { draw_text(x, y, text.c_str(), static_cast<int>(text.size()), style); }

    void draw_text(int x, int y, unsigned int w, unsigned int h, const std::string& text, const text::TextStyle& style) noexcept;

    void draw_fancy_text(const text::FancyText& ft, float x, float y, text::FancyAnchor anchor = text::FancyAnchor::TopLeft, text::FancyDrawMode mode = text::FancyDrawMode::Auto);
    void draw_fancy_texture(const text::FancyTexture& texture, float x, float y, text::FancyAnchor anchor = text::FancyAnchor::TopLeft);
    text::FancyTexture make_fancy_texture(const text::FancyText& ft, float padding = 2.0f) const;
    void draw_math(std::string_view source, float x, float y, const text::MathRenderOptions& options, text::FancyAnchor anchor = text::FancyAnchor::TopLeft);
    void draw_math(std::string_view source, float x, float y, float font_size = 24.0f, const graphics::Color& color = graphics::Color(20, 20, 20, 255), text::FancyAnchor anchor = text::FancyAnchor::TopLeft);
    text::FancyText math_text(std::string_view source, const text::MathRenderOptions& options);
    text::FancyText math_text(std::string_view source, float font_size = 24.0f, const graphics::Color& color = graphics::Color(20, 20, 20, 255));
    void set_math_cache_capacity(std::size_t entries);
    void clear_math_cache() noexcept { m_fancy_cache.reset(); }

    void draw_rich_text(int x, int y, const text::RichText& rt) noexcept { draw_rich_text(x, y, 0, 0, rt); }

    void draw_rich_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept;

    bool begin_3d(const graphics::Camera3D& camera) noexcept {
        return m_window ? begin_3d(camera, 0, 0, m_window->width(), m_window->height()) : false;
    }

    bool begin_3d(const graphics::Camera3D& camera, int x, int y, unsigned int w, unsigned int h) noexcept;

    void end_3d() noexcept { if (m_impl) m_impl->end_3d(); }
    bool in_3d() const noexcept { return m_impl && m_impl->in_3d(); }

    void set_light_3d(const graphics::Light3D& light) noexcept { if (m_impl) m_impl->set_light_3d(light); }

    void set_scene_lighting(const graphics::SceneLighting3D& lighting) noexcept { if (m_impl) m_impl->set_scene_lighting(lighting); }

    const graphics::SceneLighting3D& scene_lighting() const noexcept {
        static const graphics::SceneLighting3D none{};
        return m_impl ? m_impl->scene_lighting() : none;
    }

    const vector3d& render_origin() const noexcept { return m_origin_3d; }

    void draw_mesh(const graphics::Mesh3D& mesh, const graphics::Material3D& material = {}) noexcept {
        draw_mesh(mesh, math::Matrix4d::identity(), material);
    }

    void draw_mesh(const graphics::Mesh3D& mesh, const math::Matrix4d& model, const graphics::Material3D& material = {}) noexcept;

    void draw_mesh_at(const graphics::Mesh3D& mesh, const vector3d& offset, const graphics::Material3D& material = {}) noexcept {
        draw_mesh(mesh, translation(offset), material);
    }

    void draw_quads(const graphics::QuadMesh3D& quads, const math::Matrix4d& model, const graphics::Material3D& material = {}) noexcept;

    void draw_quads_at(const graphics::QuadMesh3D& quads, const vector3d& offset, const graphics::Material3D& material = {}) noexcept;

    graphics::MeshHandle3D upload_mesh(graphics::Mesh3D mesh, bool keep_cpu_copy = false);

    graphics::MeshHandle3D upload_quads(graphics::QuadMesh3D quads, bool keep_cpu_copy = false);

    void draw_mesh(const graphics::MeshHandle3D& mesh, const math::Matrix4d& model, const graphics::Material3D& material = {}) noexcept;

    void draw_mesh_at(const graphics::MeshHandle3D& mesh, const vector3d& offset, const graphics::Material3D& material = {}) noexcept;

    void draw_quad_batch(const graphics::QuadBatch3D& batch, const graphics::Material3D& material = {}) noexcept;

    void draw_instances(const graphics::Mesh3D& mesh, const vector3d& origin, const graphics::Instance3D* instances,
                        std::size_t count, const graphics::Material3D& material = {}) noexcept;

    void draw_instances(const graphics::Mesh3D& mesh, const vector3d& origin, const std::vector<graphics::Instance3D>& instances,
                        const graphics::Material3D& material = {}) noexcept {
        draw_instances(mesh, origin, instances.data(), instances.size(), material);
    }

    std::uint64_t gpu_mesh_bytes() const noexcept { return m_impl ? m_impl->gpu_mesh_bytes() : 0; }

    void draw_triangles_3d(const graphics::Vertex3D* vertices, std::size_t count, const graphics::Material3D& material = {}) noexcept;

    void draw_triangles_3d(const std::vector<graphics::Vertex3D>& vertices, const graphics::Material3D& material = {}) noexcept {
        draw_triangles_3d(vertices.data(), vertices.size(), material);
    }

    void draw_triangles_3d(const graphics::Vertex3D* vertices, std::size_t count, const math::Matrix4d& model, const graphics::Material3D& material = {}) noexcept;

    void draw_triangles_at(const std::vector<graphics::Vertex3D>& vertices, const vector3d& offset, const graphics::Material3D& material = {}) noexcept {
        draw_triangles_3d(vertices.data(), vertices.size(), translation(offset), material);
    }

    void draw_line_3d(const vector3d& a, const vector3d& b, const graphics::Color& color, float width = 1.0f, bool depth_test = true) noexcept;

    void draw_lines_3d(const std::vector<vector3d>& points, const graphics::Color& color, float width = 1.0f, bool depth_test = true) noexcept;

    void draw_box_3d(const vector3d& lo, const vector3d& hi, const graphics::Color& color, float width = 1.0f, bool depth_test = true) noexcept;

    text::TextMetrics measure_rich_text(const text::RichText& rt, unsigned int max_width = 0) noexcept {
        if (m_impl) return m_impl->measure_rich_text(rt, max_width);
        return {};
    }

    text::TextMetrics measure_text(const std::string& text, const text::TextStyle& style, unsigned int max_width) noexcept;

    bool load_font_file(const std::string& path) noexcept {
        return m_impl && m_impl->load_font_file(path.c_str());
    }

    text::TextMetrics measure_text(const char* utf8, int len, const text::TextStyle& style) noexcept {
        if (m_impl) return m_impl->measure_text(utf8, len, style);
        return {};
    }

    text::TextMetrics measure_text(const wchar_t* str, int len, const text::TextStyle& style) noexcept {
        if (m_impl) return m_impl->measure_text(str, len, style);
        return {};
    }

    text::TextMetrics measure_text(const std::string& text, const text::TextStyle& style) noexcept;

    text::TextMetrics measure_text(const std::wstring& text, const text::TextStyle& style) noexcept;

private:
    static math::Matrix4d translation(const vector3d& t) noexcept;

    static double axis(const vector3d& v, int a) noexcept { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }

    static std::int32_t clamp_i32(std::int64_t v) noexcept;

    static void cell_origin(const vector3d& offset, std::int32_t* out) noexcept;

    void to_origin_relative(const math::Matrix4d& model, float* out) const noexcept;

    bool open_target(const graphics::RenderTarget& target, const graphics::Color* clear_color) noexcept;

    void release() noexcept;

    bool fill_instance(
        detail::SpriteInstance& s, const double* xs, const double* ys, double offset_x, double offset_y,
        std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, float opacity
    ) const noexcept;

    static double span_ms(FrameClock::duration d) noexcept { return std::chrono::duration<double, std::milli>(d).count(); }

    void finish_frame(FrameClock::time_point start) noexcept;

    void notify_unbind() noexcept;

    enum class XformKind : std::uint8_t { Identity = 0, Translate, AxisAligned, General };
    struct ScreenRect { int x = 0; int y = 0; unsigned int w = 0; unsigned int h = 0; };

    void classify_transform() noexcept;

    bool axis_aligned_positive() const noexcept;

    static int round_i(double v) noexcept;

    static unsigned int scale_len(unsigned int len, double s) noexcept;

    RenderPoint mapped(double x, double y) const noexcept {
        const vector2d p = map_point(x, y);
        return { p.x, p.y };
    }

    ScreenRect map_rect_bounds(int x, int y, unsigned int w, unsigned int h) const noexcept;

    const RenderPoint* map_points(const RenderPoint* pts, std::size_t count) noexcept;

    void build_ellipse(int cx, int cy, unsigned int rx, unsigned int ry);

    static graphics::Paint fill_only(const graphics::Paint& p) noexcept {
        graphics::Paint f = p;
        f.set_stroke_width(0);
        return f;
    }

    void emit_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, float opacity, const graphics::TextureRect& src) noexcept;

    math::Matrix3d              m_xform = math::Matrix3d::identity();
    std::vector<math::Matrix3d> m_xform_stack;
    XformKind                   m_xform_kind = XformKind::Identity;
    std::vector<RenderPoint>    m_points;
    std::vector<detail::SpriteInstance>   m_sprite_scratch;
    std::vector<std::uint32_t>            m_sprite_order;
    std::vector<const graphics::Sprite*>  m_sprite_pointers;
    std::vector<graphics::Vertex2D>       m_vertex_scratch;
    std::vector<graphics::Vertex2D>       m_trail_scratch;
    bool                                  m_text_follow = true;
    std::vector<graphics::SliceQuad>      m_slice_scratch;
    graphics::BlendMode                   m_blend = graphics::BlendMode::Normal;

    vector3d              m_origin_3d{};
    std::vector<vector3d> m_line_scratch;
    std::vector<detail::QuadBatchDraw> m_batch_scratch;

    struct ResolvedRect { int x; int y; unsigned int w; unsigned int h; };

    static ResolvedRect resolve_corners(int x1, int y1, int x2, int y2) noexcept;
};

#ifdef OS_WINDOWS
inline void blit_framebuffer(void* hdc_raw, const graphics::Framebuffer& fb) {
    HDC hdc = static_cast<HDC>(hdc_raw);
    unsigned int w = fb.width(), h = fb.height();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight      = -static_cast<LONG>(h);
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    std::vector<std::uint8_t> bits(w * h * 4);
    const graphics::Color* px = fb.data();

    for (unsigned int i = 0; i < w * h; ++i) {
        bits[i * 4 + 0] = px[i].blue();
        bits[i * 4 + 1] = px[i].green();
        bits[i * 4 + 2] = px[i].red();
        bits[i * 4 + 3] = px[i].alpha();
    }

    SetDIBitsToDevice(
        hdc, 0, 0, w, h,
        0, 0, 0, h,
        bits.data(), &bmi, DIB_RGB_COLORS
    );
}
#endif // OS_WINDOWS

} // namespace windows
} // namespace fizmo

#endif // FIZMO_RENDERER_HPP