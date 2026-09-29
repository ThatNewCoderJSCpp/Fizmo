#ifndef FIZMO_RENDERER_GPU_IMPL_HPP
#define FIZMO_RENDERER_GPU_IMPL_HPP

#include "renderer_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#if defined(OS_WINDOWS) || defined(OS_LINUX)

#include "../../Vulkan/include.hpp"
#include "gpu_geometry.hpp"
#include "gpu_text.hpp"
#include "gpu_shaders.hpp"
#include "gpu_shaders_3d.hpp"
#include "../raster_3d.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef FIZMO_GPU_MSAA
    #define FIZMO_GPU_MSAA 4
#endif

namespace fizmo {
namespace windows {
namespace detail {

namespace gpu {

struct Vertex {
    float         x, y;      
    float         u, v;      
    std::uint32_t color;     
    std::uint32_t mode;      
    float         grad[4];   
};
static_assert(sizeof(Vertex) == 40, "vertex layout must match the pipeline");

inline std::uint32_t pack_rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept {
    return static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8)
         | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24);
}

inline std::uint8_t mul8(unsigned int c, unsigned int a) noexcept {
    return static_cast<std::uint8_t>((c * a + 127u) / 255u);
}

inline std::uint32_t pack_premul(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept {
    return pack_rgba(mul8(r, a), mul8(g, a), mul8(b, a), a);
}

inline std::uint32_t pack_opacity(std::uint8_t a) noexcept {
    return pack_rgba(a, a, a, a);
}

inline std::uint8_t scale_alpha(std::uint8_t a, float opacity) noexcept {
    if (opacity >= 1.0f) return a;
    if (opacity <= 0.0f) return 0;
    return static_cast<std::uint8_t>(a * opacity + 0.5f);
}

inline std::uint64_t hash_bytes(const void* data, std::size_t n, std::uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint64_t h = seed ^ (n * 0xff51afd7ed558ccdull);

    while (n >= 8) {
        std::uint64_t k;
        std::memcpy(&k, p, 8);
        k *= 0x87c37b91114253d5ull;
        k ^= k >> 31;
        h ^= k;
        h = ((h << 27) | (h >> 37)) * 5 + 0x52dce729;
        p += 8; n -= 8;
    }

    std::uint64_t tail = 0;
    for (std::size_t i = 0; i < n; ++i) tail |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    h ^= tail * 0x4cf5ad432745937full;
    h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33;
    return h;
}

struct LineVertex3D {
    float         x, y, z;        
    float         ox, oy, oz;     
    std::uint32_t color;          
    float         side;           
    float         width;          
};
static_assert(sizeof(LineVertex3D) == 36, "line vertex layout must match the pipeline");

inline Cap  to_cap(graphics::LineCap c) noexcept {
    return c == graphics::LineCap::Round ? Cap::Round : c == graphics::LineCap::Square ? Cap::Square : Cap::Flat;
}

inline Join to_join(graphics::LineJoin j) noexcept {
    return j == graphics::LineJoin::Round ? Join::Round : j == graphics::LineJoin::Bevel ? Join::Bevel : Join::Miter;
}

} // namespace gpu

class RendererImplGPU : public RendererImplBase {
    static_assert(sizeof(graphics::Color) == 4, "graphics::Color must be 4 packed bytes (r, g, b, a)");

private:
    static constexpr std::uint32_t kFramesInFlight   = 2;
    static constexpr int           kAtlasSize        = 1024;
    static constexpr std::size_t   kMaxAtlasPages    = 8;
    static constexpr std::uint32_t kRampRows         = 256;
    static constexpr std::uint64_t kEvictAfterFrames = 180;
    static constexpr std::uint64_t kTextureBudget    = 512ull * 1024 * 1024;  
    static constexpr std::uint64_t kMeshEvictFrames  = 1800;                   
    static constexpr std::uint64_t kMeshBudget       = 512ull * 1024 * 1024;

    enum SamplerKind : std::uint8_t {
        NearestClamp = 0, LinearClamp, NearestRepeat, LinearRepeat, NearestMirror, LinearMirror, kSamplerCount
    };

    struct Brush {
        std::uint32_t color   = 0;     
        std::uint32_t mode    = 0;
        float         grad[4] = { 0, 0, 0, 0 };
    };

    struct Batch {
        std::uint32_t             first   = 0;
        std::uint32_t             count   = 0;
        const vulkan::ImageView*  view    = nullptr;
        std::uint8_t              sampler = NearestClamp;
        vulkan::Rect2D            scissor;
    };

    struct Upload {
        vulkan::Image*   image  = nullptr;
        std::size_t      offset = 0;
        vulkan::Offset2D at;
        vulkan::Extent2D size;
    };

    struct CachedTexture {
        std::unique_ptr<vulkan::Image> image;
        unsigned int                   width = 0, height = 0;
        std::uint64_t                  stamp = 0;      
        std::uint64_t                  last_used = 0;
        graphics::Texture              keep_alive;
    };

    struct AtlasPage {
        std::unique_ptr<vulkan::Image> image;
        std::vector<std::uint8_t>      pixels;          
        int shelf_x = 0, shelf_y = 0, shelf_h = 0;
        int dirty_y0 = INT_MAX, dirty_y1 = 0;
    };

    struct Glyph {
        bool          placed = false;
        bool          color  = false;          
        std::uint16_t page = 0, x = 0, y = 0, w = 0, h = 0;
        int           left = 0, top = 0;
    };

    struct CachedLayout {
        text::RichText     text;
        float              box_w = 0.0f, box_h = 0.0f;
        gpu::TextDrawList  list;
        std::uint64_t      last_used = 0;
    };

    struct PerFrame {
        vulkan::Buffer         vertices;
        vulkan::Buffer         staging;
        vulkan::Image          ramp;
        vulkan::DescriptorPool pool;
        vulkan::Buffer         vertices3d;
        vulkan::Buffer         lines3d;
    };

    struct GpuMesh {
        std::unique_ptr<vulkan::Buffer> vertices, indices;
        std::uint64_t stamp = 0, last_used = 0, bytes = 0;
        std::uint32_t vertex_count = 0, index_count = 0;
        bool          quads = false;
    };

    struct HandleMesh {
        std::weak_ptr<graphics::detail::MeshSlot3D> slot;
        GpuMesh                                    mesh;
    };

    struct BufferUpload {
        vulkan::Buffer* dst    = nullptr;
        std::size_t     offset = 0;
        std::uint64_t   bytes  = 0;
        GpuMesh*        owner  = nullptr;
    };

    struct RetiredBuffer {
        std::uint64_t                   frame;
        std::unique_ptr<vulkan::Buffer> buffer;
    };

    enum class DrawKind3D : std::uint8_t { Mesh, Triangles, Lines };

    struct Draw3D {
        DrawKind3D               kind    = DrawKind3D::Triangles;
        std::uint32_t            state   = 0;          
        GpuMesh*                 mesh    = nullptr;
        std::uint32_t            first   = 0, count = 0;
        const vulkan::ImageView* view    = nullptr;
        std::uint8_t             sampler = NearestClamp;
        float                    push[32] = {};
        bool                     quads   = false;
    };

    struct SceneRec {
        std::size_t   batch_index = 0;   
        std::uint32_t first_draw  = 0;
        Scene3D       scene;
        Mat4f         vp;                
    };

    struct Retired {
        std::uint64_t                  frame;
        std::unique_ptr<vulkan::Image> image;
    };

    vulkan::Instance              m_instance;
    vulkan::Surface               m_surface;
    vulkan::Device                m_device;
    vulkan::Swapchain             m_swapchain;
    vulkan::FrameRing             m_frames;
    vulkan::DescriptorSetLayout   m_draw_set_layout, m_present_set_layout;
    vulkan::PipelineLayout        m_draw_layout, m_present_layout;
    vulkan::ShaderModule          m_draw_vs, m_draw_fs, m_present_vs, m_present_fs;
    vulkan::Pipeline              m_draw_pipeline, m_present_pipeline;
    std::array<vulkan::Sampler, kSamplerCount> m_samplers;
    vulkan::Image                 m_white;
    vulkan::Image                 m_target;
    vulkan::Image                 m_msaa_target;
    std::array<PerFrame, kFramesInFlight> m_per_frame;
    std::unordered_map<const void*, CachedTexture> m_textures;
    std::vector<AtlasPage>        m_atlas;
    std::unordered_map<std::uint64_t, Glyph> m_glyphs;             
    std::unordered_map<std::uint64_t, std::vector<CachedLayout>> m_layouts;
    std::vector<Retired>          m_retired;
    gpu::TextEngine               m_text;

    bool                 m_ready          = false;
    bool                 m_text_ok        = false;
    bool                 m_vsync          = true;
    bool                 m_target_fresh   = true;
    bool                 m_clear_pending  = false;
    bool                 m_clip_active    = false;
    bool                 m_clip_empty     = false;
    bool                 m_atlas_reset    = false;
    vulkan::SampleCount  m_samples        = vulkan::SampleCount::X1;
    vulkan::ClearColor   m_clear_color;
    vulkan::Rect2D       m_scissor;
    unsigned int         m_width  = 0;
    unsigned int         m_height = 0;
    std::uint64_t        m_frame_serial = 1;
    std::uint64_t        m_texture_bytes = 0;

    std::vector<gpu::Vertex>   m_vertices;
    std::vector<Batch>         m_batches;
    std::vector<Upload>        m_uploads;
    std::vector<std::uint8_t>  m_staging;
    std::vector<std::uint8_t>  m_ramp_pixels;
    std::uint32_t              m_ramp_used = 0;
    std::unordered_map<std::uint64_t, std::uint32_t> m_ramp_rows;
    std::vector<gpu::Vec2>     m_tris;
    std::vector<gpu::Vec2>     m_points;
    std::vector<gpu::Vec2>     m_wave;

    vulkan::Image                 m_depth;
    vulkan::Format                m_depth_format = vulkan::Format::D32Float;
    vulkan::PipelineLayout        m_mesh_layout, m_line_layout;
    vulkan::ShaderModule          m_mesh_vs, m_mesh_fs, m_line_vs, m_line_fs;
    std::unordered_map<std::uint32_t, vulkan::Pipeline> m_pipelines3d;
    std::unordered_map<const void*, GpuMesh> m_meshes;
    std::uint64_t                 m_mesh_bytes = 0;
    vulkan::ShaderModule          m_quad_vs, m_quad_fs;
    std::unique_ptr<vulkan::Buffer> m_quad_indices;
    std::uint64_t                 m_quad_index_capacity = 0;
    bool                          m_quad_index_pending  = false;
    std::unordered_map<const graphics::detail::MeshSlot3D*, HandleMesh> m_handle_meshes;
    std::vector<std::shared_ptr<graphics::detail::MeshSlot3D>> m_handle_uploads;
    std::uint64_t                 m_handle_bytes = 0;
    std::vector<RetiredBuffer>    m_retired_buffers;
    std::vector<BufferUpload>     m_buffer_uploads;
    std::vector<graphics::Vertex3D>  m_vertices3d;
    std::vector<gpu::LineVertex3D>   m_lines3d;
    std::vector<Draw3D>           m_draws3d;
    std::vector<SceneRec>         m_scenes;
    bool                          m_force_new_batch = false;
    bool                          m_uploads_committed = false;

public:
    RendererImplGPU() noexcept = default;
    ~RendererImplGPU() noexcept override { shutdown(); }

    const char* backend_name() const noexcept override { return "vulkan"; }
    bool        is_gpu()       const noexcept override { return true; }

    bool initialize(void* native_handle, unsigned int w, unsigned int h) noexcept override {
        shutdown();

        try {
            if (!native_handle || !create_device(native_handle, w, h)) { shutdown(); return false; }
        } catch (...) {
            shutdown();
            return false;
        }

        m_text_ok = m_text.initialize(native_handle);
        m_ready = true;
        return true;
    }

    void shutdown() noexcept override {
        if (m_device.valid()) m_device.wait_idle();
        m_ready = false;
        m_layouts.clear();
        m_text.shutdown();
        m_retired.clear();
        m_meshes.clear();
        m_mesh_bytes = 0;
        lose_handles();
        m_quad_indices.reset();
        m_quad_index_capacity = 0;
        m_quad_index_pending = false;
        m_retired_buffers.clear();
        m_pipelines3d.clear();
        m_quad_fs.destroy(); m_quad_vs.destroy();
        m_line_fs.destroy(); m_line_vs.destroy(); m_mesh_fs.destroy(); m_mesh_vs.destroy();
        m_line_layout.destroy(); m_mesh_layout.destroy();
        m_depth.destroy();
        m_glyphs.clear();
        m_atlas.clear();
        m_textures.clear();
        m_texture_bytes = 0;
        for (auto& pf : m_per_frame) { pf.lines3d.destroy(); pf.vertices3d.destroy(); pf.pool.destroy(); pf.ramp.destroy(); pf.staging.destroy(); pf.vertices.destroy(); }
        m_msaa_target.destroy();
        m_target.destroy();
        m_white.destroy();
        for (auto& s : m_samplers) s.destroy();
        m_present_pipeline.destroy(); m_draw_pipeline.destroy();
        m_present_fs.destroy(); m_present_vs.destroy(); m_draw_fs.destroy(); m_draw_vs.destroy();
        m_present_layout.destroy(); m_draw_layout.destroy();
        m_present_set_layout.destroy(); m_draw_set_layout.destroy();
        m_frames.destroy();
        m_swapchain.destroy();
        m_device.destroy();
        m_surface.destroy();
        m_instance.destroy();
        reset_recording();
    }

    void resize(unsigned int w, unsigned int h) noexcept override {
        if (!m_ready) return;
        m_frames.resize(w, h);
        if (w == 0 || h == 0 || (w == m_width && h == m_height)) return;
        m_device.wait_idle();
        create_targets(w, h);
    }

    void set_vsync(bool enabled) noexcept override {
        if (enabled == m_vsync) return;
        m_vsync = enabled;
        if (!m_ready) return;
        m_device.wait_idle();
        m_frames.destroy();
        m_swapchain.destroy();

        if (failed(m_swapchain.create(m_device, m_surface, swapchain_desc(m_width, m_height)))
            || failed(m_frames.create(m_device, m_swapchain, kFramesInFlight))) {
            m_ready = false;
        }
    }

    void begin_frame() noexcept override {}
    void present() noexcept override { flush(); }
    void paint(void* /*paint_dc*/) noexcept override { flush(); }

    void clear(const graphics::Color& color) noexcept override {
        if (!m_ready) return;
        if (m_clip_active) {
            Brush b;
            b.color = gpu::pack_rgba(color.red(), color.green(), color.blue(), 255);
            m_tris.clear();
            gpu::fill_rect(m_tris, 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
            add_triangles(b);
            return;
        }

        m_vertices.clear();
        m_batches.clear();
        drop_3d_recording();
        m_clear_pending = true;
        m_clear_color = { color.red() / 255.0f, color.green() / 255.0f, color.blue() / 255.0f, 1.0f };
    }

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin) noexcept override {
        int rx = 0, ry = 0;
        resolve_rect_origin(rx, ry, x, y, w, h, origin);
        const int x0 = std::max(rx, 0), y0 = std::max(ry, 0);
        const int x1 = std::min(rx + static_cast<int>(w), static_cast<int>(m_width));
        const int y1 = std::min(ry + static_cast<int>(h), static_cast<int>(m_height));
        m_clip_active = true;
        m_clip_empty  = x1 <= x0 || y1 <= y0;
        m_scissor = m_clip_empty ? vulkan::Rect2D{} : vulkan::Rect2D{ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } };
    }

    void reset_clip_rect() noexcept override {
        m_clip_active = false;
        m_clip_empty  = false;
        m_scissor = { { 0, 0 }, { m_width, m_height } };
    }

    void draw_pixel(int x, int y, const graphics::Color& color) noexcept override {
        if (!m_ready) return;
        Brush b;
        if (!solid_brush(color, 1.0f, b)) return;
        m_tris.clear();
        gpu::fill_rect(m_tris, static_cast<float>(x), static_cast<float>(y), 1.0f, 1.0f);
        add_triangles(b);
    }

    void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& p) noexcept override {
        if (!m_ready || !p.has_stroke()) return;
        Brush b;
        if (!paint_brush(p, true, b)) return;
        const gpu::Vec2 pts[2] = { { x1 + 0.5f, y1 + 0.5f }, { x2 + 0.5f, y2 + 0.5f } };
        m_tris.clear();
        gpu::stroke_polyline(m_tris, pts, 2, false, stroke_width(p), line_cap(p), gpu::to_join(p.line_join()));
        add_triangles(b);
    }

    void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& p) noexcept override {
        if (!m_ready || w == 0 || h == 0) return;
        int rx = 0, ry = 0;
        resolve_rect_origin(rx, ry, x, y, w, h, p.origin());
        const float fx = static_cast<float>(rx), fy = static_cast<float>(ry);
        const float fw = static_cast<float>(w),  fh = static_cast<float>(h);
        Brush b;

        if (p.has_fill() && paint_brush(p, false, b)) {
            m_tris.clear();
            gpu::fill_rect(m_tris, fx, fy, fw, fh);
            add_triangles(b);
        }

        if (p.has_stroke() && paint_brush(p, true, b)) {
            const gpu::Vec2 pts[4] = {
                { fx + 0.5f, fy + 0.5f }, { fx + fw - 0.5f, fy + 0.5f },
                { fx + fw - 0.5f, fy + fh - 0.5f }, { fx + 0.5f, fy + fh - 0.5f }
            };

            m_tris.clear();
            gpu::stroke_polyline(m_tris, pts, 4, true, stroke_width(p), gpu::Cap::Flat, gpu::to_join(p.line_join()));
            add_triangles(b);
        }
    }

    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!m_ready || (rx == 0 && ry == 0)) return;
        const gpu::Vec2 c{ cx + 0.5f, cy + 0.5f };
        float a0, a1;
        const bool full = sweep_of(p, a0, a1);
        const bool stroked = p.has_stroke();
        Brush b;

        if (p.has_fill() && paint_brush(p, false, b)) {
            const float grow = stroked ? 0.0f : 0.5f;
            m_points.clear();
            gpu::ellipse_points(m_points, c, rx + grow, ry + grow, a0, a1);
            if (full) m_points.pop_back();
            m_tris.clear();
            gpu::fill_center_fan(m_tris, c, m_points.data(), m_points.size(), full);
            add_triangles(b);
        }

        if (stroked && paint_brush(p, true, b)) {
            m_points.clear();
            if (!full) m_points.push_back(c);
            gpu::ellipse_points(m_points, c, static_cast<float>(rx), static_cast<float>(ry), a0, a1);
            if (full) m_points.pop_back();
            m_tris.clear();
            gpu::stroke_polyline(m_tris, m_points.data(), m_points.size(), true, stroke_width(p), gpu::Cap::Flat, gpu::to_join(p.line_join()));
            add_triangles(b);
        }
    }

    void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!m_ready || (rx == 0 && ry == 0)) return;
        const gpu::Vec2 c{ cx + 0.5f, cy + 0.5f };
        float a0, a1;
        const bool full = sweep_of(p, a0, a1);
        m_points.clear();
        gpu::ellipse_points(m_points, c, static_cast<float>(rx), static_cast<float>(ry), a0, a1);
        if (full) m_points.pop_back();
        Brush b;

        if (p.has_fill() && paint_brush(p, false, b)) {
            m_tris.clear();
            gpu::fill_fan(m_tris, m_points.data(), m_points.size());
            add_triangles(b);
        }

        if (p.has_stroke() && paint_brush(p, true, b)) {
            m_tris.clear();
            gpu::stroke_polyline(m_tris, m_points.data(), m_points.size(), full, stroke_width(p), line_cap(p), gpu::to_join(p.line_join()));
            add_triangles(b);
        }
    }

    void draw_polyline(const RenderPoint* pts, std::size_t count, bool closed, const graphics::Paint& p) noexcept override {
        if (!m_ready || !pts || count < 2 || !p.has_stroke()) return;
        Brush b;
        if (!paint_brush(p, true, b)) return;
        m_points.clear();
        for (std::size_t i = 0; i < count; ++i) m_points.push_back({ pts[i].x + 0.5f, pts[i].y + 0.5f });
        m_tris.clear();
        gpu::stroke_polyline(m_tris, m_points.data(), m_points.size(), closed, stroke_width(p), line_cap(p), gpu::to_join(p.line_join()));
        add_triangles(b);
    }

    void draw_polygon(const RenderPoint* pts, std::size_t count, const graphics::Paint& p) noexcept override {
        if (!m_ready || !pts || count < 3) return;
        Brush b;

        if (p.has_fill() && paint_brush(p, false, b)) {
            m_points.clear();
            for (std::size_t i = 0; i < count; ++i) m_points.push_back({ pts[i].x, pts[i].y });
            m_tris.clear();
            gpu::fill_polygon(m_tris, m_points.data(), m_points.size());
            add_triangles(b);
        }

        if (p.has_stroke()) draw_polyline(pts, count, true, p);
    }

    void draw_image(
        int dx, int dy, unsigned int dw, unsigned int dh, const images::BitmapImage& img,
        unsigned int sx, unsigned int sy, unsigned int sw, unsigned int sh
    ) noexcept override {
        if (!m_ready || !img.is_valid_image() || dw == 0 || dh == 0 || sw == 0 || sh == 0) return;
        const vulkan::ImageView* view = texture_for(&img, img.pixels().data(), img.width(), img.height(), img.version(), nullptr);
        if (!view) return;
        const bool scaled = dw != sw || dh != sh;

        add_textured_quad(
            static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dw), static_cast<float>(dh),
            static_cast<float>(sx) / img.width(), static_cast<float>(sy) / img.height(),
            static_cast<float>(sx + sw) / img.width(), static_cast<float>(sy + sh) / img.height(),
            gpu::pack_opacity(255), view, scaled ? LinearClamp : NearestClamp
        );
    }

    void draw_texture(
        int dx, int dy, unsigned int dw, unsigned int dh, const graphics::Texture& tex,
        float opacity, const graphics::TextureRect& src
    ) noexcept override {
        if (!m_ready || !tex.valid() || dw == 0 || dh == 0 || src.is_empty()) return;
        const std::uint8_t a = gpu::scale_alpha(255, opacity);
        if (a == 0) return;
        const vulkan::ImageView* view = texture_view(tex);
        if (!view) return;
        const float tw = static_cast<float>(tex.width()), th = static_cast<float>(tex.height());

        add_textured_quad(
            static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dw), static_cast<float>(dh),
            src.x / tw, src.y / th, (src.x + static_cast<float>(src.w)) / tw, (src.y + static_cast<float>(src.h)) / th,
            gpu::pack_opacity(a), view, sampler_for(tex)
        );
    }

    void draw_texture_quad(
        const RenderPoint quad[4], const graphics::Texture& tex,
        float opacity, const graphics::TextureRect& src
    ) noexcept override {
        if (!m_ready || !quad || !tex.valid() || src.is_empty()) return;
        const std::uint8_t a = gpu::scale_alpha(255, opacity);
        if (a == 0) return;
        const vulkan::ImageView* view = texture_view(tex);
        if (!view) return;
        const float tw = static_cast<float>(tex.width()), th = static_cast<float>(tex.height());
        const gpu::Vec2 p[4] = { { quad[0].x, quad[0].y }, { quad[1].x, quad[1].y }, { quad[2].x, quad[2].y }, { quad[3].x, quad[3].y } };

        add_textured_quad4(
            p, src.x / tw, src.y / th, (src.x + static_cast<float>(src.w)) / tw, (src.y + static_cast<float>(src.h)) / th,
            gpu::pack_opacity(a), view, sampler_for(tex)
        );
    }

    void draw_pixel_buffer(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const graphics::Color* pixels,
        unsigned int pw, unsigned int ph, bool smooth, std::uint64_t version
    ) noexcept override {
        if (!m_ready || !pixels || pw == 0 || ph == 0 || dw == 0 || dh == 0) return;
        const vulkan::ImageView* view = texture_for(pixels, pixels, pw, ph, version, nullptr);
        if (!view) return;

        add_textured_quad(
            static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dw), static_cast<float>(dh),
            0.0f, 0.0f, 1.0f, 1.0f,
            gpu::pack_opacity(255), view, smooth ? LinearClamp : NearestClamp
        );
    }

    void draw_text(int x, int y, const char* utf8, int len, const text::TextStyle& style) noexcept override {
        if (!m_ready || !utf8 || len == 0) return;
        if (len < 0) len = static_cast<int>(std::strlen(utf8));

        try {
            draw_rich_text(x, y, 0, 0, text::RichText(std::string(utf8, static_cast<std::size_t>(len)), style));
        } catch (...) {}
    }

    void draw_text(int x, int y, const wchar_t* str, int len, const text::TextStyle& style) noexcept override {
        if (!m_ready || !str || len == 0) return;
        if (len < 0) len = static_cast<int>(std::wcslen(str));

        try {
            draw_rich_text(x, y, 0, 0, text::RichText(gpu::wide_to_utf8(str, len), style));
        } catch (...) {}
    }

    text::TextMetrics measure_text(const char* utf8, int len, const text::TextStyle& style) noexcept override {
        if (!m_ready || !utf8 || len == 0) return {};
        if (len < 0) len = static_cast<int>(std::strlen(utf8));

        try {
            return measure_rich_text(text::RichText(std::string(utf8, static_cast<std::size_t>(len)), style), 0);
        } catch (...) {
            return {};
        }
    }

    text::TextMetrics measure_text(const wchar_t* str, int len, const text::TextStyle& style) noexcept override {
        if (!m_ready || !str || len == 0) return {};
        if (len < 0) len = static_cast<int>(std::wcslen(str));

        try {
            return measure_rich_text(text::RichText(gpu::wide_to_utf8(str, len), style), 0);
        } catch (...) {
            return {};
        }
    }

    void draw_rich_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept override {
        if (!m_ready || !m_text_ok || rt.empty() || m_clip_empty) return;

        try {
            const gpu::TextDrawList* list = layout_for(rt, static_cast<float>(w), static_cast<float>(h));
            if (list) emit_text(*list, static_cast<float>(x), static_cast<float>(y));
        } catch (...) {}
    }

    text::TextMetrics measure_rich_text(const text::RichText& rt, unsigned int max_width) noexcept override {
        if (!m_ready || !m_text_ok || rt.empty()) return {};

        try {
            const gpu::TextDrawList* list = layout_for(rt, static_cast<float>(max_width), 0.0f);
            return list ? list->metrics() : text::TextMetrics{};
        } catch (...) {
            return {};
        }
    }

    bool load_font_file(const char* utf8_path) noexcept override {
        if (!m_text_ok || !utf8_path) return false;

        try {
            const bool ok = m_text.load_font_file(utf8_path);
            if (ok) m_layouts.clear();             
            return ok;
        } catch (...) {
            return false;
        }
    }

    bool capture(images::BitmapImage& out) noexcept override {
        if (!m_ready || m_target_fresh) return false;

        try {
            m_device.wait_idle();
            const std::uint64_t bytes = static_cast<std::uint64_t>(m_width) * m_height * 4;
            vulkan::Buffer readback;
            if (failed(readback.create(m_device, { bytes, vulkan::BufferUsage::TransferDst, vulkan::MemoryUsage::GpuToCpu, "fizmo capture" }))) return false;

            const vulkan::Result r = vulkan::submit_immediate(m_device, [&](vulkan::CommandBuffer& cmd) {
                cmd.transition(m_target, vulkan::ImageLayout::TransferSrc);
                cmd.copy_image_to_buffer(m_target, readback);
                cmd.transition(m_target, vulkan::ImageLayout::ShaderReadOnly);
            });

            if (failed(r)) return false;
            std::vector<std::uint8_t> rgba(static_cast<std::size_t>(bytes));
            if (failed(readback.read(rgba.data(), bytes))) return false;
            images::BitmapImage img(m_width, m_height);

            for (unsigned int y = 0; y < m_height; ++y) {
                for (unsigned int x = 0; x < m_width; ++x) {
                    const std::uint8_t* p = &rgba[(static_cast<std::size_t>(y) * m_width + x) * 4];
                    img.set_pixel(x, y, graphics::Color(p[0], p[1], p[2], 255));
                }
            }

            out = std::move(img);
            return true;
        } catch (...) {
            return false;
        }
    }

    void begin_3d(const Scene3D& scene) noexcept override {
        if (!m_ready) return;
        if (m_in_3d) end_3d();

        try {
            SceneRec rec;
            rec.batch_index = m_batches.size();
            rec.first_draw  = static_cast<std::uint32_t>(m_draws3d.size());
            rec.scene       = scene;
            rec.vp          = scene.view_proj;
            for (int c = 0; c < 4; ++c) rec.vp[4 + c] = -rec.vp[4 + c];   
            m_scenes.push_back(rec);
            m_in_3d = true;
            m_force_new_batch = true;
        } catch (...) {}
    }

    void end_3d() noexcept override {
        if (!m_in_3d) return;
        m_in_3d = false;
        m_force_new_batch = true;   
    }

    void draw_mesh_3d(const graphics::Mesh3D& mesh, const float* model, const graphics::Material3D& mat) noexcept override {
        if (!m_ready || !m_in_3d || mesh.empty()) return;

        try {
            GpuMesh* g = mesh_for(mesh);
            if (!g) return;
            Draw3D d;
            d.kind  = DrawKind3D::Mesh;
            d.mesh  = g;
            d.count = g->index_count ? g->index_count : g->vertex_count;
            if (!material(d, mat)) return;
            mesh_push(d, model);
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    void draw_triangles_3d(const graphics::Vertex3D* v, std::size_t count, const float* model, const graphics::Material3D& mat) noexcept override {
        if (!m_ready || !m_in_3d || !v || count < 3) return;

        try {
            count -= count % 3;
            Draw3D d;
            d.kind  = DrawKind3D::Triangles;
            d.first = static_cast<std::uint32_t>(m_vertices3d.size());
            d.count = static_cast<std::uint32_t>(count);
            if (!material(d, mat)) return;
            mesh_push(d, model);
            m_vertices3d.insert(m_vertices3d.end(), v, v + count);
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    void draw_lines_3d(const vector3d* pts, std::size_t count, const graphics::Color& color, float width, bool depth_test) noexcept override {
        if (!m_ready || !m_in_3d || !pts || count < 2 || color.alpha() == 0) return;

        try {
            const SceneRec& sc = m_scenes.back();
            const std::uint32_t rgba = graphics::Vertex3D::pack(color);
            const float w = std::max(width, 1.0f);
            Draw3D d;
            d.kind  = DrawKind3D::Lines;
            d.first = static_cast<std::uint32_t>(m_lines3d.size());
            d.state = kLines | (1u << 2) /* alpha blend */ | (depth_test ? kDepthTest : 0u);

            for (std::size_t i = 0; i + 1 < count; i += 2) {
                vector3d a = pts[i], b = pts[i + 1];
                if (!clip_to_near(sc.scene.view_proj, a, b)) continue;
                auto corner = [&](const vector3d& p, const vector3d& o, float side) {
                    m_lines3d.push_back({ float(p.x), float(p.y), float(p.z), float(o.x), float(o.y), float(o.z), rgba, side, w });
                };
                corner(a, b, 1.0f); corner(a, b, -1.0f); corner(b, a, 1.0f);
                corner(a, b, 1.0f); corner(b, a, 1.0f); corner(b, a, -1.0f);
            }

            d.count = static_cast<std::uint32_t>(m_lines3d.size()) - d.first;
            if (d.count == 0) return;
            std::copy(sc.vp.begin(), sc.vp.end(), d.push);
            d.push[16] = static_cast<float>(std::max(1u, sc.scene.width));
            d.push[17] = static_cast<float>(std::max(1u, sc.scene.height));
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    void draw_quads_3d(const graphics::QuadMesh3D& quads, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) noexcept override {
        if (!m_ready || !m_in_3d || quads.empty()) return;

        try {
            GpuMesh* g = quads_for(quads);
            if (!g) return;
            push_quad_draw(*g, model, cell_origin, mat);
        } catch (...) {}
    }

    void upload_handle_3d(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot) noexcept override {
        if (!m_ready) { RendererImplBase::upload_handle_3d(slot); return; }
        if (!slot || slot->element_count == 0) return;
        if (slot->owner_epoch == m_epoch && m_handle_meshes.count(slot.get())) return;
        if (!slot->has_cpu_data()) { slot->lost = true; return; }

        try { upload_slot(slot); } catch (...) {}
    }

    void draw_handle_3d(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) noexcept override {
        if (!m_ready) { RendererImplBase::draw_handle_3d(slot, model, cell_origin, mat); return; }
        if (!m_in_3d || !slot || slot->element_count == 0 || slot->lost) return;

        try {
            auto it = m_handle_meshes.find(slot.get());

            if (it == m_handle_meshes.end() || slot->owner_epoch != m_epoch) {
                if (!slot->has_cpu_data()) { slot->lost = true; return; }
                if (!upload_slot(slot)) return;
                it = m_handle_meshes.find(slot.get());
                if (it == m_handle_meshes.end()) return;
            }

            GpuMesh& g = it->second.mesh;
            g.last_used = m_frame_serial;

            if (g.quads) {
                push_quad_draw(g, model, cell_origin, mat);
                return;
            }

            Draw3D d;
            d.kind  = DrawKind3D::Mesh;
            d.mesh  = &g;
            d.count = g.index_count ? g.index_count : g.vertex_count;
            if (!material(d, mat)) return;
            mesh_push(d, model);
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    std::uint64_t gpu_mesh_bytes() const noexcept override { return m_mesh_bytes + m_handle_bytes; }

    void release_mesh(const graphics::Mesh3D& mesh) noexcept {
        auto it = m_meshes.find(&mesh);
        if (it == m_meshes.end()) return;
        retire(it->second);
        m_meshes.erase(it);
    }

private:
    static constexpr std::uint32_t kDepthTest  = 1u << 4;
    static constexpr std::uint32_t kDepthWrite = 1u << 5;
    static constexpr std::uint32_t kLines      = 1u << 6;
    static constexpr std::uint32_t kQuads      = 1u << 7;
    static constexpr std::uint64_t kMinQuadIndexCapacity = 16384;

    void push_quad_draw(GpuMesh& g, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) {
        if (!ensure_quad_indices(g.vertex_count / graphics::QuadMesh3D::VERTICES_PER_QUAD)) return;
        Draw3D d;
        d.kind  = DrawKind3D::Mesh;
        d.mesh  = &g;
        d.quads = true;
        d.count = g.index_count;
        if (!material(d, mat)) return;
        d.state |= kQuads;
        mesh_push(d, model);

        for (int r = 0; r < 3; ++r) {
            const std::int32_t c = cell_origin ? cell_origin[r] : 0;
            std::memcpy(&d.push[16 + r * 4 + 3], &c, sizeof(c));
        }

        m_draws3d.push_back(d);
    }

    bool ensure_quad_indices(std::uint64_t quads) {
        if (quads <= m_quad_index_capacity && m_quad_indices) return true;
        const std::uint64_t capacity = std::max<std::uint64_t>({ quads, m_quad_index_capacity * 2, kMinQuadIndexCapacity });
        const std::uint64_t bytes = capacity * graphics::QuadMesh3D::INDICES_PER_QUAD * sizeof(std::uint32_t);
        auto buffer = std::make_unique<vulkan::Buffer>();
        if (failed(buffer->create(m_device, { bytes, vulkan::BufferUsage::Index, vulkan::MemoryUsage::GpuOnly, "fizmo quad indices" }))) return false;
        const std::size_t offset = staging_reserve(static_cast<std::size_t>(bytes));
        std::uint32_t* out = reinterpret_cast<std::uint32_t*>(m_staging.data() + offset);
        graphics::QuadMesh3D::for_each_quad_index(static_cast<std::size_t>(capacity), [&out](std::uint32_t i) { *out++ = i; });
        if (m_quad_indices) m_retired_buffers.push_back({ m_frame_serial, std::move(m_quad_indices) });
        m_quad_indices = std::move(buffer);
        m_quad_index_capacity = capacity;
        m_quad_index_pending = true;
        m_buffer_uploads.push_back({ m_quad_indices.get(), offset, bytes, nullptr });
        return true;
    }

    bool create_gpu_buffers(GpuMesh& g, const void* vdata, std::uint64_t vbytes, const void* idata, std::uint64_t ibytes, bool track_owner) {
        g.vertices = std::make_unique<vulkan::Buffer>();
        if (failed(g.vertices->create(m_device, { vbytes, vulkan::BufferUsage::Vertex, vulkan::MemoryUsage::GpuOnly, "fizmo mesh vertices" }))) return false;

        if (ibytes) {
            g.indices = std::make_unique<vulkan::Buffer>();
            if (failed(g.indices->create(m_device, { ibytes, vulkan::BufferUsage::Index, vulkan::MemoryUsage::GpuOnly, "fizmo mesh indices" }))) return false;
        }

        g.bytes = vbytes + ibytes;
        m_buffer_uploads.push_back({ g.vertices.get(), push_staging(vdata, static_cast<std::size_t>(vbytes)), vbytes, track_owner ? &g : nullptr });
        if (ibytes) m_buffer_uploads.push_back({ g.indices.get(), push_staging(idata, static_cast<std::size_t>(ibytes)), ibytes, track_owner ? &g : nullptr });
        return true;
    }

    GpuMesh* quads_for(const graphics::QuadMesh3D& quads) {
        const std::uint64_t stamp = quads.version();
        auto it = m_meshes.find(&quads);
        if (it != m_meshes.end() && it->second.stamp == stamp) { it->second.last_used = m_frame_serial; return &it->second; }
        const auto& verts = quads.vertices();
        const std::uint64_t vbytes = verts.size() * sizeof(graphics::CompactVertex3D);
        GpuMesh fresh;
        fresh.quads        = true;
        fresh.stamp        = stamp;
        fresh.last_used    = m_frame_serial;
        fresh.vertex_count = static_cast<std::uint32_t>(verts.size());
        fresh.index_count  = static_cast<std::uint32_t>(quads.quad_count() * graphics::QuadMesh3D::INDICES_PER_QUAD);
        GpuMesh* g = nullptr;
        if (it != m_meshes.end()) { retire(it->second); it->second = std::move(fresh); g = &it->second; }
        else g = &m_meshes.emplace(&quads, std::move(fresh)).first->second;
        if (!create_gpu_buffers(*g, verts.data(), vbytes, nullptr, 0, true)) { m_meshes.erase(&quads); return nullptr; }
        m_mesh_bytes += g->bytes;
        return g;
    }

    bool upload_slot(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot) {
        auto it = m_handle_meshes.find(slot.get());
        if (it != m_handle_meshes.end()) { retire_handle(it->second.mesh); m_handle_meshes.erase(it); }
        HandleMesh h;
        h.slot = slot;
        h.mesh.quads     = slot->quads;
        h.mesh.last_used = m_frame_serial;
        bool ok = false;

        if (slot->quads) {
            const auto& verts = slot->quad_mesh.vertices();
            h.mesh.vertex_count = static_cast<std::uint32_t>(verts.size());
            h.mesh.index_count  = static_cast<std::uint32_t>(slot->quad_mesh.quad_count() * graphics::QuadMesh3D::INDICES_PER_QUAD);
            ok = create_gpu_buffers(h.mesh, verts.data(), verts.size() * sizeof(graphics::CompactVertex3D), nullptr, 0, false);
        } else {
            const auto& verts = slot->mesh.vertices();
            const auto& idx   = slot->mesh.indices();
            h.mesh.vertex_count = static_cast<std::uint32_t>(verts.size());
            h.mesh.index_count  = static_cast<std::uint32_t>(idx.size());
            ok = create_gpu_buffers(h.mesh, verts.data(), verts.size() * sizeof(graphics::Vertex3D), idx.data(), idx.size() * sizeof(std::uint32_t), false);
        }

        if (!ok) return false;
        slot->owner_epoch = m_epoch;
        slot->gpu_bytes   = h.mesh.bytes;
        slot->resident    = false;
        slot->lost        = false;
        m_handle_bytes += h.mesh.bytes;
        m_handle_meshes.emplace(slot.get(), std::move(h));
        m_handle_uploads.push_back(slot);
        return true;
    }

    void retire_handle(GpuMesh& g) {
        m_handle_bytes -= std::min(m_handle_bytes, g.bytes);
        if (g.vertices) m_retired_buffers.push_back({ m_frame_serial, std::move(g.vertices) });
        if (g.indices)  m_retired_buffers.push_back({ m_frame_serial, std::move(g.indices) });
        g.bytes = 0;
    }

    void commit_handle_uploads() noexcept {
        for (const auto& slot : m_handle_uploads) {
            slot->resident = true;
            slot->drop_cpu_data();
        }

        m_handle_uploads.clear();
        m_quad_index_pending = false;
    }

    void abandon_handle_uploads() noexcept {
        for (const auto& slot : m_handle_uploads) {
            auto it = m_handle_meshes.find(slot.get());
            if (it != m_handle_meshes.end()) { retire_handle(it->second.mesh); m_handle_meshes.erase(it); }
            slot->owner_epoch = 0;
            slot->resident = false;
        }

        m_handle_uploads.clear();

        if (m_quad_index_pending) {
            if (m_quad_indices) m_retired_buffers.push_back({ m_frame_serial, std::move(m_quad_indices) });
            m_quad_index_capacity = 0;
            m_quad_index_pending = false;
        }
    }

    void lose_handles() noexcept {
        for (auto& kv : m_handle_meshes) {
            if (auto slot = kv.second.slot.lock()) {
                slot->resident = false;
                slot->owner_epoch = 0;
                slot->gpu_bytes = 0;
                if (!slot->has_cpu_data()) slot->lost = true;
            }
        }

        m_handle_meshes.clear();
        m_handle_uploads.clear();
        m_handle_bytes = 0;
    }

    static std::uint32_t state_for(const graphics::Material3D& m) noexcept {
        return static_cast<std::uint32_t>(m.cull) | (static_cast<std::uint32_t>(m.blend) << 2)
             | (m.depth_test ? kDepthTest : 0u) | (m.depth_write ? kDepthWrite : 0u);
    }

    bool material(Draw3D& d, const graphics::Material3D& m) {
        d.state = state_for(m);

        if (m.texture && m.texture->valid()) {
            d.view    = texture_view(*m.texture);
            d.sampler = sampler_for(*m.texture);
        } else {
            d.view    = &m_white.view();
            d.sampler = NearestClamp;
        }

        return d.view != nullptr;
    }

    void mesh_push(Draw3D& d, const float* model) const noexcept {
        const Mat4f& vp = m_scenes.back().vp;
        Mat4f m = mat4_identity();
        if (model) std::copy(model, model + 16, m.begin());
        const Mat4f mvp = model ? mat4_mul(vp, m) : vp;
        std::copy(mvp.begin(), mvp.end(), d.push);

        for (int r = 0; r < 3; ++r) {
            d.push[16 + r * 4 + 0] = m[r * 4 + 0];
            d.push[16 + r * 4 + 1] = m[r * 4 + 1];
            d.push[16 + r * 4 + 2] = m[r * 4 + 2];
            d.push[16 + r * 4 + 3] = 0.0f;
        }

        const double len = m_light3d.direction.magnitude();
        const double k = len > 0.0 ? m_light3d.diffuse / len : 0.0;
        d.push[28] = static_cast<float>(m_light3d.direction.x * k);
        d.push[29] = static_cast<float>(m_light3d.direction.y * k);
        d.push[30] = static_cast<float>(m_light3d.direction.z * k);
        d.push[31] = m_light3d.ambient;
    }

    static bool clip_to_near(const Mat4f& vp, vector3d& a, vector3d& b) noexcept {
        auto z_of = [&vp](const vector3d& p) { return vp[8] * p.x + vp[9] * p.y + vp[10] * p.z + vp[11]; };
        const double za = z_of(a), zb = z_of(b);
        if (za < 0.0 && zb < 0.0) return false;
        if (za < 0.0) a = a + (b - a) * (za / (za - zb));
        else if (zb < 0.0) b = b + (a - b) * (zb / (zb - za));
        return true;
    }

    GpuMesh* mesh_for(const graphics::Mesh3D& mesh) {
        const std::uint64_t stamp = mesh.version();
        auto it = m_meshes.find(&mesh);
        if (it != m_meshes.end() && it->second.stamp == stamp) { it->second.last_used = m_frame_serial; return &it->second; }

        const auto& verts = mesh.vertices();
        const auto& idx   = mesh.indices();
        const std::uint64_t vbytes = verts.size() * sizeof(graphics::Vertex3D);
        const std::uint64_t ibytes = idx.size() * sizeof(std::uint32_t);
        GpuMesh fresh;
        fresh.vertices = std::make_unique<vulkan::Buffer>();
        if (failed(fresh.vertices->create(m_device, { vbytes, vulkan::BufferUsage::Vertex, vulkan::MemoryUsage::GpuOnly, "fizmo mesh vertices" }))) return nullptr;

        if (ibytes) {
            fresh.indices = std::make_unique<vulkan::Buffer>();
            if (failed(fresh.indices->create(m_device, { ibytes, vulkan::BufferUsage::Index, vulkan::MemoryUsage::GpuOnly, "fizmo mesh indices" }))) return nullptr;
        }

        fresh.stamp        = stamp;
        fresh.last_used    = m_frame_serial;
        fresh.bytes        = vbytes + ibytes;
        fresh.vertex_count = static_cast<std::uint32_t>(verts.size());
        fresh.index_count  = static_cast<std::uint32_t>(idx.size());

        GpuMesh* g = nullptr;
        if (it != m_meshes.end()) { retire(it->second); it->second = std::move(fresh); g = &it->second; }
        else g = &m_meshes.emplace(&mesh, std::move(fresh)).first->second;

        m_mesh_bytes += g->bytes;
        m_buffer_uploads.push_back({ g->vertices.get(), push_staging(verts.data(), vbytes), vbytes, g });
        if (ibytes) m_buffer_uploads.push_back({ g->indices.get(), push_staging(idx.data(), ibytes), ibytes, g });
        return g;
    }

    void retire(GpuMesh& g) {
        m_mesh_bytes -= std::min(m_mesh_bytes, g.bytes);
        if (g.vertices) m_retired_buffers.push_back({ m_frame_serial, std::move(g.vertices) });
        if (g.indices)  m_retired_buffers.push_back({ m_frame_serial, std::move(g.indices) });
        g.bytes = 0;
    }

    vulkan::Pipeline* pipeline_3d(std::uint32_t key) {
        auto it = m_pipelines3d.find(key);
        if (it != m_pipelines3d.end()) return &it->second;
        const bool lines = (key & kLines) != 0;
        const bool quads = (key & kQuads) != 0;
        vulkan::GraphicsPipelineDesc d;
        d.vertex_shader   = lines ? &m_line_vs : quads ? &m_quad_vs : &m_mesh_vs;
        d.fragment_shader = lines ? &m_line_fs : quads ? &m_quad_fs : &m_mesh_fs;
        d.layout          = lines ? &m_line_layout : &m_mesh_layout;

        if (lines) {
            d.vertex_bindings   = { { 0, sizeof(gpu::LineVertex3D), vulkan::VertexRate::PerVertex } };
            d.vertex_attributes = {
                { 0, 0, vulkan::Format::RGB32Float, 0 },  { 1, 0, vulkan::Format::RGB32Float, 12 },
                { 2, 0, vulkan::Format::RGBA8Unorm, 24 }, { 3, 0, vulkan::Format::R32Float, 28 },
                { 4, 0, vulkan::Format::R32Float, 32 },
            };
            d.cull_mode = vulkan::CullMode::None;
        } else if (quads) {
            d.vertex_bindings   = { { 0, sizeof(graphics::CompactVertex3D), vulkan::VertexRate::PerVertex } };
            d.vertex_attributes = {
                { 0, 0, vulkan::Format::RGB32Float, 0 }, { 1, 0, vulkan::Format::RGBA8Unorm, 12 },
                { 2, 0, vulkan::Format::R32Uint, 16 },
            };
            const std::uint32_t cull = key & 3u;
            d.cull_mode = cull == 1u ? vulkan::CullMode::None : cull == 2u ? vulkan::CullMode::Front : vulkan::CullMode::Back;
        } else {
            d.vertex_bindings   = { { 0, sizeof(graphics::Vertex3D), vulkan::VertexRate::PerVertex } };
            d.vertex_attributes = {
                { 0, 0, vulkan::Format::RGB32Float, 0 },  { 1, 0, vulkan::Format::RGB32Float, 12 },
                { 2, 0, vulkan::Format::RG32Float, 24 },  { 3, 0, vulkan::Format::RGBA8Unorm, 32 },
            };
            const std::uint32_t cull = key & 3u;
            d.cull_mode = cull == 1u ? vulkan::CullMode::None : cull == 2u ? vulkan::CullMode::Front : vulkan::CullMode::Back;
        }

        d.front_face    = vulkan::FrontFace::CounterClockwise;
        d.samples       = m_samples;
        d.color_formats = { vulkan::Format::RGBA8Unorm };
        d.depth_format  = m_depth_format;
        d.depth.test    = (key & kDepthTest) != 0;
        d.depth.write   = (key & kDepthWrite) != 0;
        d.depth.compare = vulkan::CompareOp::LessEqual;
        const std::uint32_t blend = (key >> 2) & 3u;

        if (blend == 1u) {
            d.blend = { vulkan::BlendState::premultiplied() };
        } else if (blend == 2u) {
            vulkan::BlendState b;
            b.enable = true;
            b.src_color = vulkan::BlendFactor::One; b.dst_color = vulkan::BlendFactor::One;
            b.src_alpha = vulkan::BlendFactor::One; b.dst_alpha = vulkan::BlendFactor::One;
            d.blend = { b };
        } else {
            d.blend = { vulkan::BlendState::opaque() };
        }

        d.name = lines ? "fizmo 3d lines" : quads ? "fizmo 3d quads" : "fizmo 3d mesh";
        vulkan::Pipeline p;
        if (failed(p.create(m_device, d))) return nullptr;
        return &m_pipelines3d.emplace(key, std::move(p)).first->second;
    }

    void drop_3d_recording() noexcept {
        const bool reopen = m_in_3d && !m_scenes.empty();
        SceneRec open;
        if (reopen) open = m_scenes.back();
        m_scenes.clear();
        m_draws3d.clear();
        m_vertices3d.clear();
        m_lines3d.clear();

        if (reopen) {
            open.batch_index = 0;
            open.first_draw  = 0;
            m_scenes.push_back(open);
        }
    }

    vulkan::SwapchainDesc swapchain_desc(unsigned int w, unsigned int h) const noexcept {
        vulkan::SwapchainDesc d;
        d.size  = { std::max(w, 1u), std::max(h, 1u) };
        d.vsync = m_vsync;
        d.srgb  = false;
        d.usage = vulkan::ImageUsage::ColorAttachment;
        return d;
    }

    bool create_device(void* native_handle, unsigned int w, unsigned int h) {
        vulkan::InstanceDesc id;
        id.app_name = "fizmo";
    #ifdef FIZMO_VULKAN_VALIDATION
        id.enable_validation = true;
    #endif
        if (failed(m_instance.create(id))) return false;
        if (failed(m_surface.create(m_instance, native_handle))) return false;
        vulkan::DeviceDesc dd;
        dd.surface = &m_surface;
        if (failed(m_device.create(m_instance, dd))) return false;
        m_samples = pick_samples();
        if (failed(m_swapchain.create(m_device, m_surface, swapchain_desc(w, h)))) return false;
        if (failed(m_frames.create(m_device, m_swapchain, kFramesInFlight))) return false;
        if (failed(m_draw_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kDraw2DVert, sizeof(gpu::kDraw2DVert) / 4), "fizmo draw2d.vert"))) return false;
        if (failed(m_draw_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kDraw2DFrag, sizeof(gpu::kDraw2DFrag) / 4), "fizmo draw2d.frag"))) return false;
        if (failed(m_present_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kPresentVert, sizeof(gpu::kPresentVert) / 4), "fizmo present.vert"))) return false;
        if (failed(m_present_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kPresentFrag, sizeof(gpu::kPresentFrag) / 4), "fizmo present.frag"))) return false;

        if (
            failed(
                m_draw_set_layout.create(
                    m_device, {
                        vulkan::DescriptorBinding{ 0, vulkan::DescriptorType::CombinedImageSampler, vulkan::ShaderStage::Fragment },
                        vulkan::DescriptorBinding{ 1, vulkan::DescriptorType::CombinedImageSampler, vulkan::ShaderStage::Fragment }
                    }
                )
            )
        ) return false;

        if (
            failed(
                m_present_set_layout.create(
                    m_device, {
                        vulkan::DescriptorBinding{ 0, vulkan::DescriptorType::CombinedImageSampler, vulkan::ShaderStage::Fragment }
                    }
                )
            )
        ) return false;

        vulkan::PipelineLayoutDesc dl;
        dl.set_layouts    = { &m_draw_set_layout };
        dl.push_constants = { { vulkan::ShaderStage::Vertex, 0, 8 } };
        if (failed(m_draw_layout.create(m_device, dl))) return false;
        vulkan::PipelineLayoutDesc pl;
        pl.set_layouts = { &m_present_set_layout };
        if (failed(m_present_layout.create(m_device, pl))) return false;
        vulkan::GraphicsPipelineDesc draw;
        draw.vertex_shader   = &m_draw_vs;
        draw.fragment_shader = &m_draw_fs;
        draw.layout          = &m_draw_layout;
        draw.vertex_bindings = { { 0, sizeof(gpu::Vertex), vulkan::VertexRate::PerVertex } };

        draw.vertex_attributes = {
            { 0, 0, vulkan::Format::RG32Float,   0  },
            { 1, 0, vulkan::Format::RG32Float,   8  },
            { 2, 0, vulkan::Format::RGBA8Unorm,  16 },
            { 3, 0, vulkan::Format::R32Uint,     20 },
            { 4, 0, vulkan::Format::RGBA32Float, 24 },
        };

        draw.samples       = m_samples;
        draw.color_formats = { vulkan::Format::RGBA8Unorm };
        draw.blend         = { vulkan::BlendState::premultiplied() };
        draw.name          = "fizmo 2d";
        if (failed(m_draw_pipeline.create(m_device, draw))) return false;
        vulkan::GraphicsPipelineDesc present;
        present.vertex_shader   = &m_present_vs;
        present.fragment_shader = &m_present_fs;
        present.layout          = &m_present_layout;
        present.color_formats   = { m_swapchain.format() };
        present.name            = "fizmo present";
        if (failed(m_present_pipeline.create(m_device, present))) return false;
        if (failed(m_mesh_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kMesh3DVert, sizeof(gpu::kMesh3DVert) / 4), "fizmo mesh3d.vert"))) return false;
        if (failed(m_mesh_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kMesh3DFrag, sizeof(gpu::kMesh3DFrag) / 4), "fizmo mesh3d.frag"))) return false;
        if (failed(m_line_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kLine3DVert, sizeof(gpu::kLine3DVert) / 4), "fizmo line3d.vert"))) return false;
        if (failed(m_line_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kLine3DFrag, sizeof(gpu::kLine3DFrag) / 4), "fizmo line3d.frag"))) return false;
        if (failed(m_quad_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kQuad3DVert, sizeof(gpu::kQuad3DVert) / 4), "fizmo quad3d.vert"))) return false;
        if (failed(m_quad_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kQuad3DFrag, sizeof(gpu::kQuad3DFrag) / 4), "fizmo quad3d.frag"))) return false;
        vulkan::PipelineLayoutDesc ml;
        ml.set_layouts    = { &m_draw_set_layout };
        ml.push_constants = { { vulkan::ShaderStage::Vertex, 0, 128 } };
        if (failed(m_mesh_layout.create(m_device, ml))) return false;
        vulkan::PipelineLayoutDesc ll;
        ll.push_constants = { { vulkan::ShaderStage::Vertex, 0, 80 } };
        if (failed(m_line_layout.create(m_device, ll))) return false;
        m_depth_format = m_device.physical_device().supports_format(vulkan::Format::D32Float, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                       ? vulkan::Format::D32Float : vulkan::Format::D24UnormS8Uint;
        const vulkan::AddressMode modes[3] = { vulkan::AddressMode::ClampToEdge, vulkan::AddressMode::Repeat, vulkan::AddressMode::MirroredRepeat };

        for (int i = 0; i < kSamplerCount; ++i) {
            vulkan::SamplerDesc s;
            s.mag_filter = s.min_filter = (i % 2) ? vulkan::Filter::Linear : vulkan::Filter::Nearest;
            s.mipmap = vulkan::MipmapMode::Nearest;
            s.address_u = s.address_v = s.address_w = modes[i / 2];
            s.max_lod = 0.0f;
            if (failed(m_samplers[static_cast<std::size_t>(i)].create(m_device, s))) return false;
        }

        vulkan::ImageDesc wd;
        wd.extent = { 1, 1, 1 };
        wd.format = vulkan::Format::RGBA8Unorm;
        wd.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferDst;
        wd.name   = "fizmo white";
        if (failed(m_white.create(m_device, wd))) return false;
        const std::uint8_t white[4] = { 255, 255, 255, 255 };
        if (failed(upload(m_device, m_white, white, 4, vulkan::ImageLayout::ShaderReadOnly, false))) return false;

        for (auto& pf : m_per_frame) {
            vulkan::ImageDesc rd;
            rd.extent = { 256, kRampRows, 1 };
            rd.format = vulkan::Format::RGBA8Unorm;
            rd.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferDst;
            rd.name   = "fizmo gradient ramps";
            if (failed(pf.ramp.create(m_device, rd))) return false;
            if (failed(submit_immediate(m_device, [&](vulkan::CommandBuffer& cmd) { cmd.transition(pf.ramp, vulkan::ImageLayout::ShaderReadOnly); })))
                return false;
            vulkan::DescriptorPoolDesc pd;
            pd.max_sets = 1024;
            pd.sizes = { { vulkan::DescriptorType::CombinedImageSampler, 2048 } };
            if (failed(pf.pool.create(m_device, pd))) return false;
        }

        m_ramp_pixels.assign(static_cast<std::size_t>(256) * kRampRows * 4, 0);
        create_targets(w, h);
        return m_target.valid();
    }

    vulkan::SampleCount pick_samples() const noexcept {
        const VkSampleCountFlags supported = m_device.limits().framebufferColorSampleCounts;
        for (std::uint32_t s = FIZMO_GPU_MSAA; s > 1; s >>= 1)
            if (supported & s) return static_cast<vulkan::SampleCount>(s);
        return vulkan::SampleCount::X1;
    }

    void create_targets(unsigned int w, unsigned int h) noexcept {
        m_width  = std::max(w, 1u);
        m_height = std::max(h, 1u);
        vulkan::ImageDesc td;
        td.extent = { m_width, m_height, 1 };
        td.format = vulkan::Format::RGBA8Unorm;
        td.usage  = vulkan::ImageUsage::ColorAttachment | vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferSrc | vulkan::ImageUsage::TransferDst;
        td.name   = "fizmo back buffer";
        m_target.create(m_device, td);
        m_msaa_target.destroy();

        if (m_samples != vulkan::SampleCount::X1) {
            vulkan::ImageDesc md = td;
            md.usage   = vulkan::ImageUsage::ColorAttachment;
            md.samples = m_samples;
            md.name    = "fizmo back buffer (msaa)";
            m_msaa_target.create(m_device, md);
        }

        vulkan::ImageDesc dd;
        dd.extent  = { m_width, m_height, 1 };
        dd.format  = m_depth_format;
        dd.usage   = vulkan::ImageUsage::DepthAttachment;
        dd.samples = m_samples;
        dd.name    = "fizmo depth buffer";
        m_depth.create(m_device, dd);
        m_target_fresh = true;

        if (m_clip_active) {
            if (
                    m_scissor.offset.x + static_cast<int>(m_scissor.extent.width)  > static_cast<int>(m_width) ||
                    m_scissor.offset.y + static_cast<int>(m_scissor.extent.height) > static_cast<int>(m_height)
                ) reset_clip_rect();
        } else {
            m_scissor = { { 0, 0 }, { m_width, m_height } };
        }
    }

    void reset_recording() noexcept {
        if (!m_uploads_committed) {
            for (const BufferUpload& u : m_buffer_uploads) if (u.owner) u.owner->stamp = 0;
            abandon_handle_uploads();
        }

        m_uploads_committed = false;
        m_buffer_uploads.clear();
        m_scenes.clear();
        m_draws3d.clear();
        m_vertices3d.clear();
        m_lines3d.clear();
        m_in_3d = false;
        m_force_new_batch = false;
        m_vertices.clear();
        m_batches.clear();
        m_uploads.clear();
        m_staging.clear();
        m_ramp_rows.clear();
        m_ramp_used = 0;
        m_clear_pending = false;
    }

    Batch& batch_for(const vulkan::ImageView* view, std::uint8_t sampler) {
        if (!m_batches.empty() && !m_force_new_batch) {
            Batch& b = m_batches.back();

            if (
                b.view == view &&
                b.sampler == sampler &&
                b.scissor.offset.x == m_scissor.offset.x &&
                b.scissor.offset.y == m_scissor.offset.y &&
                b.scissor.extent.width == m_scissor.extent.width &&
                b.scissor.extent.height == m_scissor.extent.height
            ) return b;
        }

        m_force_new_batch = false;
        Batch b;
        b.first   = static_cast<std::uint32_t>(m_vertices.size());
        b.view    = view;
        b.sampler = sampler;
        b.scissor = m_scissor;
        m_batches.push_back(b);
        return m_batches.back();
    }

    void add_triangles(const Brush& brush) {
        if (m_tris.empty() || m_clip_empty) return;
        Batch& batch = batch_for(&m_white.view(), NearestClamp);
        for (const gpu::Vec2& p : m_tris) {
            m_vertices.push_back({ p.x, p.y, 0.5f, 0.5f, brush.color, brush.mode,
                                   { brush.grad[0], brush.grad[1], brush.grad[2], brush.grad[3] } });
        }
        batch.count += static_cast<std::uint32_t>(m_tris.size());
    }

    void add_textured_quad4(
        const gpu::Vec2 p[4], float u0, float v0, float u1, float v1,
        std::uint32_t color, const vulkan::ImageView* view, std::uint8_t sampler
    ) {
        if (m_clip_empty || (color >> 24) == 0) return;
        Batch& batch = batch_for(view, sampler);
        const gpu::Vertex a{ p[0].x, p[0].y, u0, v0, color, 0, { 0, 0, 0, 0 } };
        const gpu::Vertex b{ p[1].x, p[1].y, u1, v0, color, 0, { 0, 0, 0, 0 } };
        const gpu::Vertex c{ p[2].x, p[2].y, u1, v1, color, 0, { 0, 0, 0, 0 } };
        const gpu::Vertex d{ p[3].x, p[3].y, u0, v1, color, 0, { 0, 0, 0, 0 } };
        m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c);
        m_vertices.push_back(a); m_vertices.push_back(c); m_vertices.push_back(d);
        batch.count += 6;
    }

    void add_textured_quad(
        float x, float y, float w, float h, float u0, float v0, float u1, float v1,
        std::uint32_t color, const vulkan::ImageView* view, std::uint8_t sampler
    ) {
        const gpu::Vec2 p[4] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } };
        add_textured_quad4(p, u0, v0, u1, v1, color, view, sampler);
    }

    static std::uint8_t sampler_for(const graphics::Texture& tex) noexcept {
        std::uint8_t sampler = tex.filter() == graphics::SampleFilter::Bilinear ? LinearClamp : NearestClamp;
        if (tex.wrap() == graphics::WrapMode::Repeat)       sampler += 2;
        if (tex.wrap() == graphics::WrapMode::MirrorRepeat) sampler += 4;
        return sampler;
    }

    const vulkan::ImageView* texture_view(const graphics::Texture& tex) {
        const images::BitmapImage* img = tex.image();
        return texture_for(img, img->pixels().data(), img->width(), img->height(), img->version(), &tex);
    }

    static float stroke_width(const graphics::Paint& p) noexcept {
        return static_cast<float>(std::max(1u, p.stroke_width()));
    }

    static gpu::Cap line_cap(const graphics::Paint& p) noexcept {
        if (p.line_cap() == graphics::LineCap::Flat && p.stroke_width() <= 1) return gpu::Cap::Square;
        return gpu::to_cap(p.line_cap());
    }

    static bool sweep_of(const graphics::Paint& p, float& a0, float& a1) noexcept {
        const double sweep = p.end_angle() - p.start_angle();
        if (p.is_full_sweep() || std::abs(sweep) < 1e-9) { a0 = 0.0f; a1 = gpu::kTwoPi; return true; }
        constexpr double deg = constants::pi_180();
        a0 = static_cast<float>(p.start_angle() * deg);
        a1 = static_cast<float>(p.end_angle() * deg);
        return false;
    }

    static bool solid_brush(const graphics::Color& c, float opacity, Brush& out) noexcept {
        const std::uint8_t a = gpu::scale_alpha(c.alpha(), opacity);
        if (a == 0) return false;
        out = Brush{};
        out.color = gpu::pack_premul(c.red(), c.green(), c.blue(), a);
        return true;
    }

    bool paint_brush(const graphics::Paint& p, bool stroke, Brush& out) {
        const graphics::Gradient* g = stroke ? p.stroke_gradient() : p.fill_gradient();
        const float opacity = std::max(0.0f, std::min(1.0f, p.opacity()));
        if (!g || g->stops().empty()) return solid_brush(stroke ? p.stroke_color() : p.fill_color(), opacity, out);
        out = Brush{};
        const std::uint8_t a = gpu::scale_alpha(255, opacity);
        if (a == 0) return false;
        out.color = gpu::pack_opacity(a);                     
        const std::uint32_t kind = g->type() == graphics::GradientType::Radial ? 2u : 1u;
        out.mode = kind | (static_cast<std::uint32_t>(g->spread()) << 2) | (ramp_row(*g) << 16);

        if (kind == 1u) {
            out.grad[0] = static_cast<float>(g->start_x()); out.grad[1] = static_cast<float>(g->start_y());
            out.grad[2] = static_cast<float>(g->end_x());   out.grad[3] = static_cast<float>(g->end_y());
        } else {
            out.grad[0] = static_cast<float>(g->focal_x()); out.grad[1] = static_cast<float>(g->focal_y());
            out.grad[2] = static_cast<float>(g->radius());
        }

        return true;
    }

    std::uint32_t ramp_row(const graphics::Gradient& g) {
        std::uint64_t key = 0x51ED270B27FCD1ull;

        for (const auto& s : g.stops()) {
            const std::uint32_t c = gpu::pack_rgba(s.color.red(), s.color.green(), s.color.blue(), s.color.alpha());
            key = gpu::hash_bytes(&s.position, sizeof(double), key);
            key = gpu::hash_bytes(&c, sizeof(c), key);
        }

        auto it = m_ramp_rows.find(key);
        if (it != m_ramp_rows.end()) return it->second;
        if (m_ramp_used >= kRampRows) return kRampRows - 1;
        const std::uint32_t row = m_ramp_used++;
        std::uint8_t* dst = &m_ramp_pixels[static_cast<std::size_t>(row) * 256 * 4];

        for (int i = 0; i < 256; ++i) {
            const graphics::Color c = g.color_at(i / 255.0);
            const unsigned int a = c.alpha();
            dst[i * 4 + 0] = gpu::mul8(c.red(), a);
            dst[i * 4 + 1] = gpu::mul8(c.green(), a);
            dst[i * 4 + 2] = gpu::mul8(c.blue(), a);
            dst[i * 4 + 3] = static_cast<std::uint8_t>(a);
        }

        m_ramp_rows.emplace(key, row);
        return row;
    }

    std::size_t staging_reserve(std::size_t bytes) {
        const std::size_t offset = (m_staging.size() + 15) & ~static_cast<std::size_t>(15);
        m_staging.resize(offset + bytes);
        return offset;
    }

    std::size_t push_staging(const void* data, std::size_t bytes) {
        const std::size_t offset = staging_reserve(bytes);
        std::memcpy(m_staging.data() + offset, data, bytes);
        return offset;
    }

    std::size_t push_staging_premul(const graphics::Color* px, std::size_t count) {
        const std::size_t offset = staging_reserve(count * 4);
        std::uint8_t* dst = m_staging.data() + offset;

        for (std::size_t i = 0; i < count; ++i, dst += 4) {
            const graphics::Color& c = px[i];
            const unsigned int a = c.alpha();

            if (a == 255) {
                dst[0] = c.red(); dst[1] = c.green(); dst[2] = c.blue(); dst[3] = 255;
            } else if (a == 0) {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            } else {
                dst[0] = gpu::mul8(c.red(), a); dst[1] = gpu::mul8(c.green(), a); dst[2] = gpu::mul8(c.blue(), a);
                dst[3] = static_cast<std::uint8_t>(a);
            }
        }

        return offset;
    }

    std::unique_ptr<vulkan::Image> make_texture_image(unsigned int w, unsigned int h, const char* name) {
        auto img = std::make_unique<vulkan::Image>();
        vulkan::ImageDesc d;
        d.extent = { w, h, 1 };
        d.format = vulkan::Format::RGBA8Unorm;
        d.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferDst;
        d.name   = name;
        if (failed(img->create(m_device, d))) return nullptr;
        return img;
    }

    const vulkan::ImageView* texture_for(
        const void* key, const graphics::Color* pixels, unsigned int w, unsigned int h,
        std::uint64_t version, const graphics::Texture* owner
    ) {
        const std::size_t count = static_cast<std::size_t>(w) * h;
        const std::size_t bytes = count * 4;
        const std::uint64_t stamp = version != 0 ? version : gpu::hash_bytes(pixels, bytes);
        auto it = m_textures.find(key);

        if (it != m_textures.end() && it->second.width == w && it->second.height == h) {
            CachedTexture& e = it->second;
            if (e.stamp == stamp) { e.last_used = m_frame_serial; return &e.image->view(); }

            if (e.last_used == m_frame_serial) {
                auto tmp = make_texture_image(w, h, "fizmo transient texture");
                if (!tmp) return nullptr;
                m_uploads.push_back({ tmp.get(), push_staging_premul(pixels, count), { 0, 0 }, { w, h } });
                const vulkan::ImageView* view = &tmp->view();
                m_retired.push_back({ m_frame_serial, std::move(tmp) });
                return view;
            }

            e.stamp = stamp;
            e.last_used = m_frame_serial;
            if (owner) e.keep_alive = *owner;
            m_uploads.push_back({ e.image.get(), push_staging_premul(pixels, count), { 0, 0 }, { w, h } });
            return &e.image->view();
        }

        if (it != m_textures.end()) {
            m_texture_bytes -= static_cast<std::uint64_t>(it->second.width) * it->second.height * 4;
            m_retired.push_back({ it->second.last_used, std::move(it->second.image) });
            m_textures.erase(it);
        }

        CachedTexture e;
        e.image = make_texture_image(w, h, "fizmo texture");
        if (!e.image) return nullptr;
        e.width = w; e.height = h; e.stamp = stamp; e.last_used = m_frame_serial;
        if (owner) e.keep_alive = *owner;
        m_uploads.push_back({ e.image.get(), push_staging_premul(pixels, count), { 0, 0 }, { w, h } });
        m_texture_bytes += bytes;
        const vulkan::ImageView* view = &e.image->view();
        m_textures.emplace(key, std::move(e));
        return view;
    }

    bool atlas_alloc(int w, int h, std::uint16_t& page, std::uint16_t& x, std::uint16_t& y) {
        if (w + 1 > kAtlasSize || h + 1 > kAtlasSize) return false;

        for (int attempt = 0; attempt < 2; ++attempt) {
            if (m_atlas.empty()) { if (!add_atlas_page()) return false; }
            AtlasPage& pg = m_atlas.back();
            if (pg.shelf_x + w + 1 > kAtlasSize) { pg.shelf_y += pg.shelf_h; pg.shelf_x = 0; pg.shelf_h = 0; }

            if (pg.shelf_y + h + 1 <= kAtlasSize) {
                page = static_cast<std::uint16_t>(m_atlas.size() - 1);
                x = static_cast<std::uint16_t>(pg.shelf_x);
                y = static_cast<std::uint16_t>(pg.shelf_y);
                pg.shelf_x += w + 1;
                pg.shelf_h = std::max(pg.shelf_h, h + 1);
                return true;
            }

            if (m_atlas.size() >= kMaxAtlasPages || !add_atlas_page()) return false;
        }

        return false;
    }

    bool add_atlas_page() {
        AtlasPage pg;
        pg.image = make_texture_image(kAtlasSize, kAtlasSize, "fizmo glyph atlas");
        if (!pg.image) return false;
        pg.pixels.assign(static_cast<std::size_t>(kAtlasSize) * kAtlasSize * 4, 0);  
        pg.dirty_y0 = 0;
        pg.dirty_y1 = kAtlasSize;
        m_atlas.push_back(std::move(pg));
        return true;
    }

    const Glyph& glyph_for(std::uint32_t face, std::uint32_t index) {
        const std::uint64_t key = (static_cast<std::uint64_t>(face) << 32) | index;
        auto it = m_glyphs.find(key);
        if (it != m_glyphs.end()) return it->second;
        Glyph g;
        gpu::GlyphImage img;

        if (m_text.rasterize(face, index, img) && img.width > 0 && img.height > 0) {
            if (atlas_alloc(img.width, img.height, g.page, g.x, g.y)) {
                AtlasPage& pg = m_atlas[g.page];

                for (int row = 0; row < img.height; ++row) {
                    std::uint8_t* dst = &pg.pixels[((static_cast<std::size_t>(g.y) + row) * kAtlasSize + g.x) * 4];

                    if (img.color) {
                        std::memcpy(dst, &img.pixels[static_cast<std::size_t>(row) * img.width * 4], static_cast<std::size_t>(img.width) * 4);
                    } else {
                        const std::uint8_t* src = &img.pixels[static_cast<std::size_t>(row) * img.width];
                        for (int col = 0; col < img.width; ++col) dst[col * 4 + 0] = dst[col * 4 + 1] = dst[col * 4 + 2] = dst[col * 4 + 3] = src[col];
                    }
                }

                pg.dirty_y0 = std::min(pg.dirty_y0, static_cast<int>(g.y));
                pg.dirty_y1 = std::max(pg.dirty_y1, static_cast<int>(g.y) + img.height);
                g.w      = static_cast<std::uint16_t>(img.width);
                g.h      = static_cast<std::uint16_t>(img.height);
                g.left   = img.left;
                g.top    = img.top;
                g.color  = img.color;
                g.placed = true;
            } else {
                m_atlas_reset = true;                  
                return m_glyphs.emplace(key, g).first->second;
            }
        }

        return m_glyphs.emplace(key, g).first->second;
    }

    static std::uint64_t layout_hash(const text::RichText& rt, float w, float h) noexcept {
        std::uint64_t k = gpu::hash_bytes(&w, sizeof(w));
        k = gpu::hash_bytes(&h, sizeof(h), k);

        for (const auto& sp : rt.spans()) {
            k = gpu::hash_bytes(sp.text.data(), sp.text.size(), k);
            k = gpu::hash_bytes(sp.family.data(), sp.family.size(), k ^ 0x5bd1e995u);
        }

        const double size = rt.base().size();
        return gpu::hash_bytes(&size, sizeof(size), k);
    }

    const gpu::TextDrawList* layout_for(const text::RichText& rt, float w, float h) {
        const std::uint64_t key = layout_hash(rt, w, h);
        auto& bucket = m_layouts[key];

        for (CachedLayout& c : bucket) {
            if (c.box_w == w && c.box_h == h && c.text == rt) { c.last_used = m_frame_serial; return &c.list; }
        }

        CachedLayout c;
        c.text  = rt;
        c.box_w = w;
        c.box_h = h;
        c.last_used = m_frame_serial;
        if (!gpu::build_text_draw_list(m_text, rt, w, h, c.list)) return nullptr;
        bucket.push_back(std::move(c));
        return &bucket.back().list;
    }

    void emit_text(const gpu::TextDrawList& list, float ox, float oy) {
        const vulkan::Rect2D saved_scissor = m_scissor;
        const bool saved_active = m_clip_active, saved_empty = m_clip_empty;

        if (list.clip) {                                        
            const int x0 = std::max(m_scissor.offset.x, static_cast<int>(std::floor(ox)));
            const int y0 = std::max(m_scissor.offset.y, static_cast<int>(std::floor(oy)));
            const int x1 = std::min(m_scissor.offset.x + static_cast<int>(m_scissor.extent.width),  static_cast<int>(std::ceil(ox + list.clip_w)));
            const int y1 = std::min(m_scissor.offset.y + static_cast<int>(m_scissor.extent.height), static_cast<int>(std::ceil(oy + list.clip_h)));
            m_clip_active = true;
            m_clip_empty  = x1 <= x0 || y1 <= y0;
            m_scissor = m_clip_empty ? vulkan::Rect2D{} : vulkan::Rect2D{ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } };
        }

        constexpr float inv = 1.0f / kAtlasSize;
        Brush b;

        for (const gpu::TextCmd& c : list.cmds) {
            switch (c.kind) {
                case gpu::TextCmd::Kind::Rect:
                    if (!solid_brush(c.color, 1.0f, b)) break;
                    m_tris.clear();
                    gpu::fill_rect(m_tris, ox + c.x, oy + c.y, c.w, c.h);
                    add_triangles(b);
                    break;

                case gpu::TextCmd::Kind::Wave: {
                    if (!solid_brush(c.color, 1.0f, b)) break;
                    const auto& pts = list.waves[c.wave];
                    m_wave.clear();
                    for (const gpu::TextPoint& p : pts) m_wave.push_back({ ox + p.x, oy + p.y });
                    m_tris.clear();
                    gpu::stroke_polyline(m_tris, m_wave.data(), m_wave.size(), false, c.w, gpu::Cap::Round, gpu::Join::Round);
                    add_triangles(b);
                    break;
                }

                case gpu::TextCmd::Kind::Glyph: {
                    if (c.color.alpha() == 0) break;
                    const Glyph& g = glyph_for(c.face, c.glyph);
                    if (!g.placed) break;
                    const float gx = std::round(ox + c.x) + static_cast<float>(g.left);
                    const float gy = std::round(oy + c.y) - static_cast<float>(g.top);
                    const std::uint32_t color = g.color ? gpu::pack_opacity(c.color.alpha())
                                                        : gpu::pack_premul(c.color.red(), c.color.green(), c.color.blue(), c.color.alpha());

                    add_textured_quad(
                        gx, gy, g.w, g.h,
                        g.x * inv, g.y * inv, (g.x + g.w) * inv, (g.y + g.h) * inv,
                        color, &m_atlas[g.page].image->view(), NearestClamp
                    );
                    break;
                }
            }
        }

        m_scissor     = saved_scissor;
        m_clip_active = saved_active;
        m_clip_empty  = saved_empty;
    }

    void reset_atlas() {
        m_glyphs.clear();
        for (std::size_t i = 1; i < m_atlas.size(); ++i) m_retired.push_back({ m_frame_serial, std::move(m_atlas[i].image) });
        if (m_atlas.size() > 1) m_atlas.resize(1);

        if (!m_atlas.empty()) {
            AtlasPage& pg = m_atlas[0];
            std::fill(pg.pixels.begin(), pg.pixels.end(), static_cast<std::uint8_t>(0));
            pg.shelf_x = pg.shelf_y = pg.shelf_h = 0;
            pg.dirty_y0 = 0;
            pg.dirty_y1 = kAtlasSize;
        }

        m_atlas_reset = false;
    }

    void collect_garbage() {
        const std::uint64_t now = m_frame_serial;
        auto safe = [now](std::uint64_t last_used) { return last_used + kFramesInFlight <= now; };

        m_retired.erase(
            std::remove_if(
                m_retired.begin(), m_retired.end(),
                [&](const Retired& r) { return safe(r.frame); }
            ), m_retired.end()
        );

        m_retired_buffers.erase(
            std::remove_if(m_retired_buffers.begin(), m_retired_buffers.end(), [&](const RetiredBuffer& r) { return safe(r.frame); }),
            m_retired_buffers.end()
        );

        for (auto it = m_meshes.begin(); it != m_meshes.end();) {
            if (it->second.last_used + kMeshEvictFrames <= now) { retire(it->second); it = m_meshes.erase(it); }
            else ++it;
        }

        for (auto it = m_handle_meshes.begin(); it != m_handle_meshes.end();) {
            if (it->second.slot.expired()) { retire_handle(it->second.mesh); it = m_handle_meshes.erase(it); }
            else ++it;
        }

        while (m_mesh_bytes > kMeshBudget) {
            auto oldest = m_meshes.end();
            for (auto it = m_meshes.begin(); it != m_meshes.end(); ++it)
                if (safe(it->second.last_used) && (oldest == m_meshes.end() || it->second.last_used < oldest->second.last_used)) oldest = it;
            if (oldest == m_meshes.end()) break;
            retire(oldest->second);
            m_meshes.erase(oldest);
        }

        for (auto it = m_textures.begin(); it != m_textures.end();) {
            if (it->second.last_used + kEvictAfterFrames <= now) {
                m_texture_bytes -= static_cast<std::uint64_t>(it->second.width) * it->second.height * 4;
                it = m_textures.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = m_layouts.begin(); it != m_layouts.end();) {
            auto& bucket = it->second;

            bucket.erase(
                std::remove_if(bucket.begin(), bucket.end(), [now](const CachedLayout& c) { return c.last_used + kEvictAfterFrames <= now; }),
                bucket.end()
            );

            it = bucket.empty() ? m_layouts.erase(it) : std::next(it);
        }

        while (m_texture_bytes > kTextureBudget) {
            auto oldest = m_textures.end();

            for (auto it = m_textures.begin(); it != m_textures.end(); ++it)
                if (safe(it->second.last_used) && (oldest == m_textures.end() || it->second.last_used < oldest->second.last_used))
                    oldest = it;

            if (oldest == m_textures.end()) break;
            m_texture_bytes -= static_cast<std::uint64_t>(oldest->second.width) * oldest->second.height * 4;
            m_textures.erase(oldest);
        }
    }

    static bool ensure_buffer(
        vulkan::Device& device, vulkan::Buffer& buf, std::uint64_t bytes,
        vulkan::BufferUsage usage, const char* name
    ) {
        if (buf.valid() && buf.size() >= bytes) return true;
        const std::uint64_t size = std::max<std::uint64_t>({ bytes, buf.size() * 2, 64 * 1024 });
        return succeeded(buf.create(device, { size, usage, vulkan::MemoryUsage::CpuToGpu, name }));
    }

    void flush() noexcept {
        if (!m_ready) return;

        try {
            vulkan::Frame f;
            vulkan::Result r = m_frames.begin_frame(f);
            if (r == vulkan::Result::OutOfDate) r = m_frames.begin_frame(f);

            if (r != vulkan::Result::Success) {
                reset_recording();
                return;
            }

            record_frame(f);
            m_frames.end_frame();
        } catch (...) {
            // drop the frame
        }

        reset_recording();
        if (m_atlas_reset) reset_atlas();
        ++m_frame_serial;
    }

    using DescriptorCache = std::vector<std::pair<std::uint64_t, vulkan::DescriptorSet>>;

    vulkan::DescriptorSet descriptor_for(PerFrame& pf, const vulkan::ImageView* view, std::uint8_t sampler, DescriptorCache& cache) {
        const std::uint64_t key = reinterpret_cast<std::uintptr_t>(view) * 8 + sampler;
        for (const auto& s : cache) if (s.first == key) return s.second;
        vulkan::DescriptorSet set;
        if (failed(pf.pool.allocate(m_draw_set_layout, set))) return set;
        vulkan::DescriptorWriter().image(0, *view, m_samplers[sampler]).image(1, pf.ramp.view(), m_samplers[LinearClamp]).update(set);
        cache.emplace_back(key, set);
        return set;
    }

    vulkan::ColorAttachment color_attachment(bool clear) const noexcept {
        const bool msaa = m_msaa_target.valid();
        vulkan::ColorAttachment color;
        color.view    = msaa ? &m_msaa_target.view() : &m_target.view();
        color.resolve = msaa ? &m_target.view() : nullptr;
        color.load    = clear ? vulkan::LoadOp::Clear : vulkan::LoadOp::Load;
        color.store   = vulkan::StoreOp::Store;
        color.clear   = m_clear_pending ? m_clear_color : vulkan::ClearColor{ 0, 0, 0, 1 };
        return color;
    }

    void record_2d_pass(const vulkan::CommandBuffer& cmd, PerFrame& pf, std::size_t from, std::size_t to, bool clear, DescriptorCache& sets) {
        const vulkan::ColorAttachment color = color_attachment(clear);
        const vulkan::Extent2D size{ m_width, m_height };
        cmd.begin_rendering({ { { 0, 0 }, size }, color });

        if (from < to) {
            cmd.bind(m_draw_pipeline);
            cmd.set_viewport({ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f });
            const float push[2] = { 2.0f / m_width, 2.0f / m_height };
            cmd.push_constants(m_draw_pipeline, vulkan::ShaderStage::Vertex, push, sizeof(push));
            cmd.bind_vertex_buffer(0, pf.vertices);

            for (std::size_t i = from; i < to; ++i) {
                const Batch& b = m_batches[i];
                if (b.count == 0 || b.scissor.extent.width == 0 || b.scissor.extent.height == 0) continue;
                const vulkan::DescriptorSet set = descriptor_for(pf, b.view, b.sampler, sets);
                if (!set.valid()) continue;
                cmd.set_scissor(b.scissor);
                cmd.bind_descriptor_set(m_draw_pipeline, 0, set);
                cmd.draw(b.count, 1, b.first);
            }
        }

        cmd.end_rendering();
    }

    void record_3d_pass(const vulkan::CommandBuffer& cmd, PerFrame& pf, const SceneRec& sc, std::size_t from, std::size_t to, bool clear, DescriptorCache& sets) {
        const int x0 = std::max(sc.scene.x, 0), y0 = std::max(sc.scene.y, 0);
        const int x1 = std::min(sc.scene.x + static_cast<int>(sc.scene.width),  static_cast<int>(m_width));
        const int y1 = std::min(sc.scene.y + static_cast<int>(sc.scene.height), static_cast<int>(m_height));
        if ((x1 <= x0 || y1 <= y0 || from >= to) && !clear) return;
        cmd.transition(m_depth, vulkan::ImageLayout::DepthAttachment);
        const vulkan::ColorAttachment color = color_attachment(clear);
        vulkan::DepthAttachment depth;
        depth.view        = &m_depth.view();
        depth.load        = vulkan::LoadOp::Clear;
        depth.store       = vulkan::StoreOp::DontCare;
        depth.clear       = vulkan::ClearDepth{ 1.0f, 0 };
        depth.has_stencil = vulkan::has_stencil(m_depth_format);
        vulkan::RenderingDesc rd;
        rd.area   = { { 0, 0 }, { m_width, m_height } };
        rd.colors = color;
        rd.depth  = &depth;
        cmd.begin_rendering(rd);

        if (x1 > x0 && y1 > y0) {
            cmd.set_viewport({ static_cast<float>(sc.scene.x), static_cast<float>(sc.scene.y),
                               static_cast<float>(sc.scene.width), static_cast<float>(sc.scene.height), 0.0f, 1.0f });
            cmd.set_scissor({ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } });
            const vulkan::Pipeline* bound = nullptr;

            for (std::size_t i = from; i < to; ++i) {
                const Draw3D& d = m_draws3d[i];
                if (d.count == 0) continue;
                const vulkan::Pipeline* p = pipeline_3d(d.state);
                if (!p) continue;
                if (p != bound) { cmd.bind(*p); bound = p; }

                if (d.kind == DrawKind3D::Lines) {
                    cmd.push_constants(*p, vulkan::ShaderStage::Vertex, d.push, 80);
                    cmd.bind_vertex_buffer(0, pf.lines3d);
                    cmd.draw(d.count, 1, d.first);
                    continue;
                }

                const vulkan::DescriptorSet set = descriptor_for(pf, d.view, d.sampler, sets);
                if (!set.valid()) continue;
                cmd.bind_descriptor_set(*p, 0, set);
                cmd.push_constants(*p, vulkan::ShaderStage::Vertex, d.push, 128);

                if (d.kind == DrawKind3D::Mesh) {
                    if (!d.mesh || !d.mesh->vertices) continue;
                    cmd.bind_vertex_buffer(0, *d.mesh->vertices);
                    if (d.quads) {
                        if (!m_quad_indices) continue;
                        cmd.bind_index_buffer(*m_quad_indices, vulkan::IndexType::UInt32);
                        cmd.draw_indexed(d.count);
                    }
                    else if (d.mesh->indices) { cmd.bind_index_buffer(*d.mesh->indices, vulkan::IndexType::UInt32); cmd.draw_indexed(d.count); }
                    else cmd.draw(d.count);
                } else {
                    cmd.bind_vertex_buffer(0, pf.vertices3d);
                    cmd.draw(d.count, 1, d.first);
                }
            }
        }

        cmd.end_rendering();
    }

    void record_frame(vulkan::Frame& f) {
        collect_garbage();
        PerFrame& pf = m_per_frame[f.frame_index];
        pf.pool.reset();
        const vulkan::CommandBuffer& cmd = f.cmd;

        for (AtlasPage& pg : m_atlas) {
            if (pg.dirty_y0 >= pg.dirty_y1) continue;
            const std::size_t row_bytes = static_cast<std::size_t>(kAtlasSize) * 4;

            const std::size_t off = push_staging(
                &pg.pixels[static_cast<std::size_t>(pg.dirty_y0) * row_bytes],
                row_bytes * static_cast<std::size_t>(pg.dirty_y1 - pg.dirty_y0)
            );

            m_uploads.push_back(
                {
                    pg.image.get(), off, { 0, pg.dirty_y0 },
                    {
                        static_cast<std::uint32_t>(kAtlasSize),
                        static_cast<std::uint32_t>(pg.dirty_y1 - pg.dirty_y0)
                    }
                }
            );

            pg.dirty_y0 = INT_MAX;
            pg.dirty_y1 = 0;
        }

        if (m_ramp_used > 0) {
            const std::size_t off = push_staging(m_ramp_pixels.data(), static_cast<std::size_t>(m_ramp_used) * 256 * 4);
            m_uploads.push_back({ &pf.ramp, off, { 0, 0 }, { 256, m_ramp_used } });
        }

        if (!m_staging.empty()) {
            if (!ensure_buffer(m_device, pf.staging, m_staging.size(), vulkan::BufferUsage::TransferSrc, "fizmo staging")) return;
            pf.staging.write(m_staging.data(), m_staging.size());
        }

        if (!m_vertices.empty()) {
            const std::uint64_t bytes = m_vertices.size() * sizeof(gpu::Vertex);
            if (!ensure_buffer(m_device, pf.vertices, bytes, vulkan::BufferUsage::Vertex, "fizmo vertices")) return;
            pf.vertices.write(m_vertices.data(), bytes);
        }

        if (!m_vertices3d.empty()) {
            const std::uint64_t bytes = m_vertices3d.size() * sizeof(graphics::Vertex3D);
            if (!ensure_buffer(m_device, pf.vertices3d, bytes, vulkan::BufferUsage::Vertex, "fizmo 3d vertices")) return;
            pf.vertices3d.write(m_vertices3d.data(), bytes);
        }

        if (!m_lines3d.empty()) {
            const std::uint64_t bytes = m_lines3d.size() * sizeof(gpu::LineVertex3D);
            if (!ensure_buffer(m_device, pf.lines3d, bytes, vulkan::BufferUsage::Vertex, "fizmo 3d lines")) return;
            pf.lines3d.write(m_lines3d.data(), bytes);
        }

        cmd.begin_label("fizmo uploads");

        for (const Upload& u : m_uploads) {
            cmd.transition(*u.image, vulkan::ImageLayout::TransferDst);
            cmd.copy_buffer_to_image_region(pf.staging, *u.image, u.offset, u.at, u.size);
        }

        for (const Upload& u : m_uploads) cmd.transition(*u.image, vulkan::ImageLayout::ShaderReadOnly);
        for (const BufferUpload& u : m_buffer_uploads) cmd.copy_buffer(pf.staging, *u.dst, u.bytes, u.offset, 0);

        if (!m_buffer_uploads.empty()) {
            cmd.memory_barrier(vulkan::PipelineStage::Transfer, vulkan::Access::TransferWrite,
                               vulkan::PipelineStage::VertexInput, vulkan::Access::VertexRead | vulkan::Access::IndexRead);
        }

        m_uploads_committed = true;
        commit_handle_uploads();
        cmd.end_label();
        const bool must_clear = m_clear_pending || m_target_fresh;

        if (must_clear || !m_batches.empty() || !m_scenes.empty()) {
            cmd.begin_label("fizmo draw");
            cmd.transition(m_target, vulkan::ImageLayout::ColorAttachment);
            if (m_msaa_target.valid()) cmd.transition(m_msaa_target, vulkan::ImageLayout::ColorAttachment);
            DescriptorCache sets;
            bool clear_next = must_clear;
            bool had_pass = false;
            std::size_t next_batch = 0;

            auto between_passes = [&]() {
                if (had_pass) {
                    cmd.memory_barrier(
                        vulkan::PipelineStage::ColorAttachmentOutput | vulkan::PipelineStage::LateFragmentTests,
                        vulkan::Access::ColorAttachmentWrite | vulkan::Access::DepthAttachmentWrite,
                        vulkan::PipelineStage::ColorAttachmentOutput | vulkan::PipelineStage::EarlyFragmentTests,
                        vulkan::Access::ColorAttachmentRead | vulkan::Access::ColorAttachmentWrite |
                        vulkan::Access::DepthAttachmentRead | vulkan::Access::DepthAttachmentWrite);
                }
                had_pass = true;
            };

            for (std::size_t si = 0; si <= m_scenes.size(); ++si) {
                const bool last = si == m_scenes.size();
                const std::size_t upto = last ? m_batches.size() : std::min(m_scenes[si].batch_index, m_batches.size());

                if (upto > next_batch || (last && clear_next)) {
                    between_passes();
                    record_2d_pass(cmd, pf, next_batch, upto, clear_next, sets);
                    clear_next = false;
                }

                next_batch = std::max(next_batch, upto);
                if (last) break;
                const std::size_t draw_end = si + 1 < m_scenes.size() ? m_scenes[si + 1].first_draw : m_draws3d.size();
                between_passes();
                record_3d_pass(cmd, pf, m_scenes[si], m_scenes[si].first_draw, draw_end, clear_next, sets);
                clear_next = false;
            }

            cmd.end_label();
            m_target_fresh = false;
        }

        cmd.transition(m_target, vulkan::ImageLayout::ShaderReadOnly);
        cmd.begin_label("fizmo present");
        vulkan::DescriptorSet present_set;

        if (succeeded(pf.pool.allocate(m_present_set_layout, present_set))) {
            const bool same_size = f.extent.width == m_width && f.extent.height == m_height;
            vulkan::DescriptorWriter().image(0, m_target.view(), m_samplers[same_size ? NearestClamp : LinearClamp]).update(present_set);
            vulkan::ColorAttachment out;
            out.view = f.target_view;
            out.load = vulkan::LoadOp::DontCare;
            cmd.begin_rendering({ { { 0, 0 }, f.extent }, out });
            cmd.bind(m_present_pipeline);
            cmd.set_viewport_and_scissor(f.extent);
            cmd.bind_descriptor_set(m_present_pipeline, 0, present_set);
            cmd.draw(3);
            cmd.end_rendering();
        }

        cmd.end_label();
    }
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_WINDOWS || OS_LINUX
#endif // FIZMO_RENDERER_GPU_IMPL_HPP