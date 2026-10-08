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

#include "../../Basic/constants.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
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
        unsigned int                   width     = 0;
        unsigned int                   height    = 0;
        std::uint64_t                  stamp     = 0;      
        std::uint64_t                  last_used = 0;
        graphics::Texture              keep_alive;
    };

    struct AtlasPage {
        std::unique_ptr<vulkan::Image> image;
        std::vector<std::uint8_t>      pixels;          
        int shelf_x  = 0;
        int shelf_y  = 0;
        int shelf_h  = 0;
        int dirty_y0 = INT_MAX;
        int dirty_y1 = 0;
    };

    struct Glyph {
        bool          placed = false;
        bool          color  = false;          
        std::uint16_t page   = 0; 
        std::uint16_t x      = 0;
        std::uint16_t y      = 0;
        std::uint16_t w      = 0;
        std::uint16_t h      = 0;
        int           left   = 0;
        int           top    = 0;
    };

    struct CachedLayout {
        text::RichText    text;
        float             box_w = 0.0f;
        float             box_h = 0.0f;
        gpu::TextDrawList list;
        std::uint64_t     last_used = 0;
    };

    struct PerFrame {
        vulkan::Buffer         vertices;
        vulkan::Buffer         staging;
        vulkan::Image          ramp;
        vulkan::DescriptorPool pool;
        vulkan::Buffer         vertices3d;
        vulkan::Buffer         lines3d;
        vulkan::Buffer         batch_instances;
        vulkan::Buffer         batch_commands;
        vulkan::Buffer         instances3d;
        vulkan::Buffer         scene_uniforms;
        vulkan::TimestampPool  timestamps;
        std::uint32_t          stamps_used = 0;
        std::vector<std::pair<GpuPass, std::uint32_t>>               spans;
        std::vector<std::pair<std::uint64_t, vulkan::DescriptorSet>> scene_sets;
    };

    struct GpuMesh {
        std::unique_ptr<vulkan::Buffer> vertices;
        std::unique_ptr<vulkan::Buffer> indices;
        std::uint64_t                   stamp        = 0;
        std::uint64_t                   last_used    = 0;
        std::uint64_t                   bytes        = 0;
        std::uint64_t                   generation   = 0;
        std::uint32_t                   vertex_count = 0;
        std::uint32_t                   index_count  = 0;
        bool                            quads        = false;
        graphics::Bounds3D              bounds;
        graphics::FaceGroups            groups{};
    };

    struct ArenaRange {
        std::uint32_t page  = 0;
        std::uint32_t first = 0;
        std::uint32_t count = 0;
    };

    struct ArenaPage {
        std::unique_ptr<vulkan::Buffer>         buffer;
        std::uint32_t                           capacity = 0;
        std::uint32_t                           used     = 0;
        std::map<std::uint32_t, std::uint32_t>  free;
    };

    struct HandleMesh {
        std::weak_ptr<graphics::detail::MeshSlot3D> slot;
        bool                                        quads = false;
        ArenaRange                                  range;
        GpuMesh                                     mesh;
        std::uint64_t                               bytes = 0;
    };

    struct PendingHandle {
        std::uint64_t                                 key = 0;
        std::shared_ptr<graphics::detail::MeshSlot3D> slot;
    };

    struct RetiredRange {
        std::uint64_t frame;
        ArenaRange    range;
    };

    struct BatchInstance {
        std::int32_t rel_cell[3];
        std::int32_t abs_cell[3];
        float        frac[3];
    };

    struct IndirectCommand {
        std::uint32_t index_count;
        std::uint32_t instance_count;
        std::uint32_t first_index;
        std::int32_t  vertex_offset;
        std::uint32_t first_instance;
    };

    static_assert(sizeof(BatchInstance) == 36, "batch instance layout must match the pipeline");
    static_assert(sizeof(IndirectCommand) == 20, "indirect command layout must match VkDrawIndexedIndirectCommand");

    struct BufferUpload {
        vulkan::Buffer* dst    = nullptr;
        std::size_t     offset = 0;
        std::uint64_t   bytes  = 0;
        GpuMesh*        owner  = nullptr;
        std::uint64_t   dst_offset = 0;
    };

    struct RetiredBuffer {
        std::uint64_t                   frame;
        std::unique_ptr<vulkan::Buffer> buffer;
    };

    enum class DrawKind3D : std::uint8_t { Mesh, Triangles, Lines, ArenaQuads, Batch, Instanced };

    struct Draw3D {
        DrawKind3D               kind           = DrawKind3D::Triangles;
        std::uint32_t            state          = 0;          
        GpuMesh*                 mesh           = nullptr;
        std::uint32_t            first          = 0, count = 0;
        const vulkan::ImageView* view           = nullptr;
        std::uint8_t             sampler        = NearestClamp;
        float                    push[32]       = {};
        bool                     quads          = false;
        std::uint32_t            page           = 0;
        std::int32_t             vertex_offset  = 0;
        std::uint32_t            instance_first = 0;
        std::uint32_t            instance_count = 0;
        bool                     on_camera      = true;
        bool                     in_reflections = true;
        bool                     sky            = false;
    };

    struct SceneRec {
        std::size_t   batch_index  = 0;   
        std::uint32_t first_draw   = 0;
        std::uint32_t first_caster = 0;
        Scene3D       scene;
        Scene3D       full;
        bool          scaled       = false;
        Mat4f         vp;                
    };

    static constexpr int                 kMaxPointLights          = static_cast<int>(graphics::SceneLighting3D::MAX_POINT_LIGHTS);
    static constexpr int                 kPointShadowMaps         = static_cast<int>(graphics::PointShadows3D::MAX_LIGHTS);
    static constexpr int                 kCubeFaces               = 6;
    static constexpr std::uint8_t        kAllCubeFaces            = 0x3F;
    static constexpr float               kMinRenderScale          = 0.25f;
    static constexpr float               kFullRenderScale         = 1.0f;
    static constexpr std::uint32_t       kUpscalePushBytes        = 32;
    static constexpr std::uint32_t       kFullscreenVertices      = 3;
    static constexpr float               kNoCone                  = -3.0f;
    static constexpr double              kCubeFaceSpread          = 0.9553166181245093;
    static constexpr double              kConeMargin              = 0.05;
    static constexpr double              kMinConeGap              = 1e-3;
    static constexpr std::int32_t        kSceneEnabled            = 1;
    static constexpr std::int32_t        kSunShadows              = 2;
    static constexpr std::int32_t        kPointShadows            = 4;
    static constexpr std::int32_t        kAtmosphere              = 8;
    static constexpr std::int32_t        kVolumetric              = 16;
    static constexpr std::int32_t        kToneMap                 = 32;
    static constexpr std::int32_t        kMedium                  = 64;
    static constexpr std::int32_t        kSceneCopy               = 128;
    static constexpr std::int32_t        kScreenReflections       = 256;
    static constexpr std::int32_t        kSoftShadows             = 512;
    static constexpr std::int32_t        kLightShafts             = 1024;
    static constexpr std::int32_t        kReflectionPass          = 2048;
    static constexpr std::int32_t        kPlanarReflections       = 4096;
    static constexpr std::int32_t        kVolumeGrid              = 8192;
    static constexpr std::uint32_t       kVolumeGroup             = 8;
    static constexpr std::uint32_t       kReflectionVolumeDivisor = 4;
    static constexpr std::uint32_t       kMaxVolumeSlices         = 256;
    static constexpr std::uint32_t       kMaxStorageSets          = 64;
    static constexpr std::uint32_t       kMaxTimestamps           = 128;
    static constexpr int                 kSceneSetShift           = 8;
    static constexpr int                 kBoxCorners              = 8;
    static constexpr double              kReflectionClipBias      = 0.01;
    static constexpr double              kPlaneNearW              = 1e-3;
    static constexpr double              kPlaneRectMargin         = 0.02;
    static constexpr std::uint64_t       kHashSeed                = 1469598103934665603ull;
    static constexpr std::uint64_t       kHashPrime               = 1099511628211ull;
    static constexpr int                 kHashShift               = 29;
    static constexpr double              kSignatureGrid           = 256.0;
    static constexpr vulkan::ShaderStage kSceneStages             = vulkan::ShaderStage::Vertex | vulkan::ShaderStage::Fragment | vulkan::ShaderStage::Compute;
    static constexpr int                 kMaxPlanes               = static_cast<int>(graphics::PlanarReflections3D::MAX_PLANES);
    static constexpr int                 kMaxCapsules             = static_cast<int>(graphics::CapsuleShadows3D::MAX_CAPSULES);
    static constexpr double              kPlaneFacing             = 1e-3;
    static constexpr std::uint32_t       kBlendShift              = 2;
    static constexpr std::uint32_t       kBlendMask               = 3u;
    static constexpr double              kSoftDegreesToSlope      = constants::pi_180();
    static constexpr double              kAnchorStart             = 0.02;
    static constexpr double              kAnchorBackTrack         = 0.25;
    static constexpr double              kAnchorDrift             = 0.5;
    static constexpr int                 kAnchorAdvance           = 4;
    static constexpr std::uint32_t       kSkyVertices             = 3;
    static constexpr std::uint32_t       kFlagLit                 = 1u;
    static constexpr std::uint32_t       kFlagLegacy              = 4u;
    static constexpr std::uint32_t       kFlagNoFog               = 64u;
    static constexpr double              kPointNear               = 0.05;
    static constexpr double              kSunUpLimit              = 0.99;
    static constexpr double              kRadiansPerDegree        = constants::pi_180();
    static constexpr float               kCubeTexelSpan           = 2.0f;
    static constexpr std::uint32_t       kShadowPointKey          = 1u << 2;
    static constexpr float               kShadowBiasConstant      = 1.25f;
    static constexpr float               kShadowBiasSlope         = 1.75f;

    struct SceneUniforms {
        float        legacy[4]                       = { 0, 0, 0, 1 };
        float        sun_dir[4]                      = { 0, 0, -1, 0 };
        float        sun_color[4]                    = { 0, 0, 0, 1 };
        float        sky_color[4]                    = { 1, 1, 1, 0 };
        float        block_color[4]                  = { 1, 1, 1, 1 };
        float        params[4]                       = { 0, 1, 0, 0 };
        std::int32_t counts[4]                       = { 0, 0, 0, 0 };
        float        sun_matrix[16]                  = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        float        sun_shadow[4]                   = { 0, 0, 0, 0 };
        float        point_shadow[4]                 = { 0, 0, 0, 0 };
        float        point_pos[kMaxPointLights][4]   = {};
        float        point_color[kMaxPointLights][4] = {};
        float        sky_zenith[4]                   = {};
        float        sky_horizon[4]                  = {};
        float        sky_glow[4]                     = {};
        float        sky_sun[4]                      = {};
        float        sky_moon[4]                     = {};
        float        sky_params[4]                   = { 0, 0, 0, 1 };
        float        medium_color[4]                 = { 0, 0, 0, 1 };
        float        volume[4]                       = {};
        float        waves[4]                        = {};
        float        gloss[4]                        = {};
        float        inv_view_proj[16]               = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        float        view_proj[16]                   = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        float        screen[4]                       = { 0, 0, 1, 1 };
        float        target[4]                       = { 1, 1, 0, 0 };
        float        water[4]                        = {};
        float        soft[4]                         = {};
        float        shafts[4]                       = { 1, 1, 0, 0 };
        float        rays[4]                         = {};
        float        sun_matrix_b[16]                = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        float        sun_mix[4]                      = {};
        float        planes[kMaxPlanes][4]           = {};
        float        plane_info[4]                   = {};
        float        clip[4]                         = {};
        float        sun_disk[4]                     = { 1, 1, 1, 64 };
        float        glow[4]                         = { 6, 0, 0, 0 };
        float        capsule_a[kMaxCapsules][4]      = {};
        float        capsule_b[kMaxCapsules][4]      = {};
        float        capsules[4]                     = {};
        float        volume_grid[4]                  = {};
        float        volume_proj[16]                 = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        float        plane_rects[kMaxPlanes][4]      = {};
        float        swell[4]                        = {};
        float        swell_phase[4]                  = {};
        float        point_spot[kMaxPointLights][4]  = {};
        float        point_map[kPointShadowMaps][4]  = {};
    };

    static_assert(sizeof(SceneUniforms) == 4256, "scene uniforms must match the std140 block in the shaders");
    
    struct Caster {
        enum class Kind : std::uint8_t { Mesh = 0, Instanced, Quads };
        Kind                  kind           = Kind::Mesh;
        const vulkan::Buffer* vertices       = nullptr;
        const vulkan::Buffer* indices        = nullptr;
        std::uint32_t         count          = 0;
        std::int32_t          vertex_base    = 0;
        std::uint32_t         instance_first = 0;
        std::uint32_t         instance_count = 0;
        graphics::FaceGroups  groups{};
        graphics::FaceMask    faces          = graphics::ALL_FACE_GROUPS;
        std::uint64_t         generation     = 0;
        bool                  axis_aligned   = true;
        float                 model[12]      = { 1,0,0,0, 0,1,0,0, 0,0,1,0 };
        float                 lo[3]          = { 0, 0, 0 };
        float                 hi[3]          = { 0, 0, 0 };
    };

    struct PointShadowRec {
        float         light[4] = { 0, 0, 0, 1 };
        Mat4f         faces[kCubeFaces];
        std::uint64_t key      = 0;
        std::uint8_t  mask     = kAllCubeFaces;
        bool          moving   = false;
        bool          render   = false;
        std::uint64_t id       = 0;
        double        world[3] = { 0, 0, 0 };
    };

    struct SceneLightRec {
        SceneUniforms                                uniforms;
        bool                                         sun_shadow                 = false;
        bool                                         sun_blend                  = false;
        float                                        sun_dir_a[3]               = { 0, 0, -1 };
        float                                        sun_dir_b[3]               = { 0, 0, -1 };
        std::uint64_t                                sun_key_a                  = 0;
        std::uint64_t                                sun_key_b                  = 0;
        unsigned int                                 sun_size                   = 0;
        bool                                         wants_casters              = false;
        bool                                         sky                        = false;
        std::array<PointShadowRec, kPointShadowMaps> points{};
        std::uint32_t                                point_count                = 0;
        unsigned int                                 point_size                 = 0;
        unsigned int                                 moving_faces               = 0;
        bool                                         wants_copy                 = false;
        bool                                         wants_rays                 = false;
        std::uint32_t                                plane_count                = 0;
        float                                        planes[kMaxPlanes][4]      = {};
        int                                          plane_rects[kMaxPlanes][4] = {};
        float                                        plane_scales[kMaxPlanes]   = {};
        bool                                         copy                       = false;
        std::uint32_t                                copy_at                    = 0;
        bool                                         volume                     = false;
        std::uint32_t                                volume_size[3]             = {};
    };

    struct ShadowMap {
        std::unique_ptr<vulkan::Image>            image;
        vulkan::ImageView                         sample_view;
        std::array<vulkan::ImageView, kCubeFaces> face_views;
        unsigned int                              size      = 0;
        bool                                      cube      = false;
        bool                                      cached    = false;
        std::uint64_t                             signature = 0;
        std::uint64_t                             light_id  = 0;
        std::uint8_t                              mask      = 0;
        double                                    world[3]  = { 0, 0, 0 };
        float                                     radius    = 1.0f;

        const vulkan::ImageView& attachment(int face) const noexcept { return cube ? face_views[static_cast<std::size_t>(face)] : image->view(); }
    };

    struct ReflectionTarget {
        std::unique_ptr<vulkan::Image> color, msaa, depth;
        unsigned int                   width = 0, height = 0;
    };

    struct RetiredMap {
        std::uint64_t              frame;
        std::unique_ptr<ShadowMap> map;
    };

    struct Retired {
        std::uint64_t                  frame;
        std::unique_ptr<vulkan::Image> image;
    };

    vulkan::Instance                                             m_instance;
    vulkan::Surface                                              m_surface;
    vulkan::Device                                               m_device;
    vulkan::Swapchain                                            m_swapchain;
    vulkan::FrameRing                                            m_frames;
    vulkan::DescriptorSetLayout                                  m_draw_set_layout;
    vulkan::DescriptorSetLayout                                  m_present_set_layout;
    vulkan::PipelineLayout                                       m_draw_layout;
    vulkan::PipelineLayout                                       m_present_layout;
    vulkan::ShaderModule                                         m_draw_vs;
    vulkan::ShaderModule                                         m_draw_fs;
    vulkan::ShaderModule                                         m_present_vs;
    vulkan::ShaderModule                                         m_present_fs;
    vulkan::Pipeline                                             m_draw_pipeline;
    vulkan::Pipeline                                             m_present_pipeline;
    vulkan::PipelineLayout                                       m_upscale_layout;
    vulkan::ShaderModule                                         m_upscale_fs;
    vulkan::Pipeline                                             m_upscale_pipeline;
    std::array<vulkan::Sampler, kSamplerCount>                   m_samplers;
    vulkan::Image                                                m_white;
    vulkan::Image                                                m_target;
    vulkan::Image                                                m_msaa_target;
    std::array<PerFrame, kFramesInFlight>                        m_per_frame;
    std::unordered_map<const void*, CachedTexture>               m_textures;
    std::vector<AtlasPage>                                       m_atlas;
    std::unordered_map<std::uint64_t, Glyph>                     m_glyphs;             
    std::unordered_map<std::uint64_t, std::vector<CachedLayout>> m_layouts;
    std::vector<Retired>                                         m_retired;
    gpu::TextEngine                                              m_text;

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
    unsigned int         m_width          = 0;
    unsigned int         m_height         = 0;
    std::uint64_t        m_frame_serial   = 1;
    std::uint64_t        m_texture_bytes  = 0;

    std::vector<gpu::Vertex>                         m_vertices;
    std::vector<Batch>                               m_batches;
    std::vector<Upload>                              m_uploads;
    std::vector<std::uint8_t>                        m_staging;
    std::vector<std::uint8_t>                        m_ramp_pixels;
    std::uint32_t                                    m_ramp_used = 0;
    std::unordered_map<std::uint64_t, std::uint32_t> m_ramp_rows;
    std::vector<gpu::Vec2>                           m_tris;
    std::vector<gpu::Vec2>                           m_points;
    std::vector<gpu::Vec2>                           m_wave;

    vulkan::Image                                            m_depth;
    vulkan::Image                                            m_scene_color;
    vulkan::Image                                            m_upscale;
    float                                                    m_render_scale = 1.0f;
    UpscaleFilter                                            m_upscale_filter = UpscaleFilter::Bilinear;
    float                                                    m_sharpness      = 0.0f;
    vulkan::Image                                            m_scene_depth;
    vulkan::ImageView                                        m_scene_depth_view;
    vulkan::Format                                           m_depth_format = vulkan::Format::D32Float;
    vulkan::PipelineLayout                                   m_mesh_layout;
    vulkan::PipelineLayout                                   m_line_layout;
    vulkan::ShaderModule                                     m_mesh_vs;
    vulkan::ShaderModule                                     m_mesh_fs;
    vulkan::ShaderModule                                     m_line_vs;
    vulkan::ShaderModule                                     m_line_fs;
    std::unordered_map<std::uint32_t, vulkan::Pipeline>      m_pipelines3d;
    std::unordered_map<const void*, GpuMesh>                 m_meshes;
    std::uint64_t                                            m_mesh_bytes = 0;
    vulkan::ShaderModule                                     m_quad_vs;
    vulkan::ShaderModule                                     m_quad_fs;
    std::unique_ptr<vulkan::Buffer>                          m_quad_indices;
    std::uint64_t                                            m_quad_index_capacity = 0;
    bool                                                     m_quad_index_pending  = false;
    vulkan::ShaderModule                                     m_batch_vs;
    vulkan::ShaderModule                                     m_instanced_vs;
    std::unordered_map<std::uint64_t, HandleMesh>            m_handle_meshes;
    std::vector<PendingHandle>                               m_handle_uploads;
    std::uint64_t                                            m_handle_bytes = 0;
    std::uint64_t                                            m_next_handle_key = 0;
    std::shared_ptr<graphics::detail::MeshReleaseQueue>      m_release_queue = std::make_shared<graphics::detail::MeshReleaseQueue>();
    std::vector<ArenaPage>                                   m_arena;
    std::vector<RetiredRange>                                m_retired_ranges;
    std::uint64_t                                            m_arena_quads_used = 0;
    std::vector<BatchInstance>                               m_batch_instances;
    std::vector<IndirectCommand>                             m_batch_commands;
    std::vector<std::vector<IndirectCommand>>                m_page_commands;
    std::vector<graphics::Instance3D>                        m_instances3d;
    bool                                                     m_multi_draw = false;
    std::vector<RetiredBuffer>                               m_retired_buffers;
    std::vector<BufferUpload>                                m_buffer_uploads;
    std::vector<graphics::Vertex3D>                          m_vertices3d;
    std::vector<gpu::LineVertex3D>                           m_lines3d;
    std::vector<Draw3D>                                      m_draws3d;
    std::vector<SceneRec>                                    m_scenes;
    std::vector<SceneLightRec>                               m_scene_lights;
    std::vector<Caster>                                      m_casters;
    std::vector<std::uint8_t>                                m_uniform_bytes;
    std::uint64_t                                            m_uniform_stride = sizeof(SceneUniforms);
    vulkan::DescriptorSetLayout                              m_scene_set_layout;
    vulkan::PipelineLayout                                   m_shadow_layout;
    vulkan::ShaderModule                                     m_shadow_mesh_vs;
    vulkan::ShaderModule                                     m_shadow_instanced_vs;
    vulkan::ShaderModule                                     m_shadow_quad_vs;
    vulkan::ShaderModule                                     m_shadow_point_fs;
    vulkan::ShaderModule                                     m_sky_vs;
    vulkan::ShaderModule                                     m_sky_fs;
    vulkan::Pipeline                                         m_sky_pipeline;
    vulkan::ShaderModule                                     m_rays_fs;
    vulkan::Pipeline                                         m_rays_pipeline;
    vulkan::ShaderModule                                     m_volume_cs;
    vulkan::DescriptorSetLayout                              m_volume_set_layout;
    vulkan::PipelineLayout                                   m_volume_layout;
    vulkan::Pipeline                                         m_volume_pipeline;
    std::unique_ptr<vulkan::Image>                           m_volume;
    vulkan::Image                                            m_dummy_volume;
    std::uint8_t                                             m_volume_sampler = LinearClamp;
    std::unordered_map<std::uint32_t, vulkan::Pipeline>      m_shadow_pipelines;
    vulkan::Sampler                                          m_shadow_sampler;
    vulkan::Format                                           m_shadow_format = vulkan::Format::D16Unorm;
    std::unique_ptr<ShadowMap>                               m_sun_map;
    std::unique_ptr<ShadowMap>                               m_sun_map_b;
    std::array<ReflectionTarget, kMaxPlanes>                 m_reflections;
    mutable vector3d                                         m_sun_a{ 0.0, 0.0, -1.0 };
    mutable vector3d                                         m_sun_b{ 0.0, 0.0, -1.0 };
    mutable vector3d                                         m_sun_prev{ 0.0, 0.0, -1.0 };
    mutable bool                                             m_sun_anchored = false;
    std::array<std::unique_ptr<ShadowMap>, kPointShadowMaps> m_point_maps;
    std::unique_ptr<ShadowMap>                               m_dummy_sun;
    std::unique_ptr<ShadowMap>                               m_dummy_cube;
    std::vector<RetiredMap>                                  m_retired_maps;
    mutable std::vector<std::pair<double, std::size_t>>      m_light_order;
    mutable std::vector<std::pair<double, std::size_t>>      m_capsule_order;
    bool                                                     m_force_new_batch = false;
    bool                                                     m_gpu_timing = false;
    GpuTimings                                               m_timings;
    std::vector<std::uint64_t>                               m_stamp_values;
    std::uint64_t                                            m_mesh_generation = 0;
    std::array<std::uint64_t, kPointShadowMaps>              m_point_signatures{};
    std::uint32_t                                            m_point_kept   = 0;
    std::uint32_t                                            m_moving_turn  = 0;
    bool                                                     m_uploads_committed = false;

public:
    RendererImplGPU() noexcept = default;
    ~RendererImplGPU() noexcept override { shutdown(); }

    const char* backend_name() const noexcept override { return "vulkan"; }
    bool        is_gpu()       const noexcept override { return true; }

    void set_gpu_timing(bool enabled) noexcept override {
        m_gpu_timing = enabled;
        if (!enabled) m_timings = {};
    }

    GpuTimings gpu_timings() const noexcept override { return m_timings; }

    GpuMemory gpu_memory() const noexcept override {
        const vulkan::MemoryReport r = m_device.memory_report();
        GpuMemory m;
        m.valid        = m_device.valid();
        m.measured     = r.measured;
        m.used         = r.device_used;
        m.budget       = r.device_budget;
        m.total        = r.device_total;
        m.shared_used  = r.shared_used;
        m.shared_total = r.shared_total;
        return m;
    }

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
        m_shadow_pipelines.clear();
        m_retired_maps.clear();
        m_sun_map.reset();
        m_sun_map_b.reset();

        for (auto& r : m_reflections) { 
            r.color.reset(); 
            r.msaa.reset(); 
            r.depth.reset(); 
            r.width = r.height = 0; 
        }
        
        for (auto& m : m_point_maps) m.reset();
        m_dummy_sun.reset();
        m_dummy_cube.reset();
        m_shadow_sampler.destroy();
        m_rays_pipeline.destroy(); 
        m_volume_pipeline.destroy(); 
        m_volume_layout.destroy(); 
        m_volume_set_layout.destroy(); 
        m_volume_cs.destroy();
        m_volume.reset();
        m_dummy_volume.destroy();
        m_rays_fs.destroy();
        m_sky_pipeline.destroy(); 
        m_sky_fs.destroy(); 
        m_sky_vs.destroy();
        m_shadow_point_fs.destroy(); 
        m_shadow_quad_vs.destroy(); 
        m_shadow_instanced_vs.destroy(); 
        m_shadow_mesh_vs.destroy();
        m_shadow_layout.destroy();
        m_arena.clear();
        m_retired_ranges.clear();
        m_arena_quads_used = 0;
        m_instanced_vs.destroy(); 
        m_batch_vs.destroy();
        m_quad_fs.destroy(); 
        m_quad_vs.destroy();
        m_line_fs.destroy(); 
        m_line_vs.destroy(); 
        m_mesh_fs.destroy(); 
        m_mesh_vs.destroy();
        m_line_layout.destroy(); 
        m_mesh_layout.destroy();
        m_scene_set_layout.destroy();
        m_scene_depth_view.destroy();
        m_scene_depth.destroy();
        m_scene_color.destroy();
        m_upscale.destroy();
        m_depth.destroy();
        m_glyphs.clear();
        m_atlas.clear();
        m_textures.clear();
        m_texture_bytes = 0;

        for (auto& pf : m_per_frame) { 
            pf.timestamps.destroy(); 
            pf.spans.clear(); 
            pf.stamps_used = 0;
            pf.scene_uniforms.destroy(); 
            pf.instances3d.destroy(); 
            pf.batch_commands.destroy(); 
            pf.batch_instances.destroy(); 
            pf.lines3d.destroy(); 
            pf.vertices3d.destroy(); 
            pf.pool.destroy(); 
            pf.ramp.destroy(); 
            pf.staging.destroy(); 
            pf.vertices.destroy(); 
        }
        
        m_msaa_target.destroy();
        m_target.destroy();
        m_white.destroy();
        for (auto& s : m_samplers) s.destroy();
        m_present_pipeline.destroy(); 
        m_draw_pipeline.destroy();
        m_upscale_pipeline.destroy();
        m_upscale_layout.destroy();
        m_upscale_fs.destroy();
        m_present_fs.destroy(); 
        m_present_vs.destroy(); 
        m_draw_fs.destroy(); 
        m_draw_vs.destroy();
        m_present_layout.destroy(); 
        m_draw_layout.destroy();
        m_present_set_layout.destroy(); 
        m_draw_set_layout.destroy();
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
            rec.batch_index  = m_batches.size();
            rec.first_draw   = static_cast<std::uint32_t>(m_draws3d.size());
            rec.first_caster = static_cast<std::uint32_t>(m_casters.size());
            rec.scene        = scene;
            rec.full         = scene;
            rec.vp           = scene.view_proj;
            for (int c = 0; c < 4; ++c) rec.vp[4 + c] = -rec.vp[4 + c];

            if (m_render_scale < kFullRenderScale && m_upscale.valid() && scene.width > 0 && scene.height > 0) {
                rec.scene.width  = std::max(1u, static_cast<unsigned int>(std::lround(scene.width * m_render_scale)));
                rec.scene.height = std::max(1u, static_cast<unsigned int>(std::lround(scene.height * m_render_scale)));
                rec.scaled       = rec.scene.width < scene.width || rec.scene.height < scene.height;
            }

            m_scene_lights.push_back(snapshot_lighting(rec.scene));
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

    void set_render_scale(float scale) noexcept override {
        const float s = std::min(std::max(scale, kMinRenderScale), kFullRenderScale);
        m_render_scale = s;
        if (s < kFullRenderScale && !m_upscale.valid() && m_ready) create_upscale();
    }

    float render_scale() const noexcept override { return m_render_scale; }

    void set_upscale(UpscaleFilter filter, float sharpness) noexcept override {
        m_upscale_filter = filter;
        m_sharpness      = std::min(std::max(sharpness, 0.0f), 1.0f);
    }

    void create_upscale() noexcept {
        vulkan::ImageDesc ud;
        ud.extent = { m_width, m_height, 1 };
        ud.format = vulkan::Format::RGBA8Unorm;
        ud.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferDst;
        ud.name   = "fizmo scaled scene";
        if (failed(m_upscale.create(m_device, ud))) m_upscale.destroy();
    }

    void record_upscale(const vulkan::CommandBuffer& cmd, PerFrame& pf, const SceneRec& sc) {
        if (!sc.scaled || !m_upscale.valid() || !m_upscale_pipeline.valid()) return;
        const int x0 = std::max(sc.scene.x, 0), y0 = std::max(sc.scene.y, 0);
        const int x1 = std::min(sc.scene.x + static_cast<int>(sc.scene.width), static_cast<int>(m_width));
        const int y1 = std::min(sc.scene.y + static_cast<int>(sc.scene.height), static_cast<int>(m_height));
        if (x1 <= x0 || y1 <= y0) return;
        const vulkan::Rect2D r{ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } };
        cmd.begin_label("fizmo render scale");
        cmd.transition(m_target, vulkan::ImageLayout::TransferSrc);
        cmd.transition(m_upscale, vulkan::ImageLayout::TransferDst);
        cmd.blit(m_target.handle(), r, m_upscale.handle(), r, vulkan::Filter::Nearest);
        cmd.transition(m_upscale, vulkan::ImageLayout::ShaderReadOnly);
        cmd.transition(m_target, vulkan::ImageLayout::ColorAttachment);
        vulkan::DescriptorSet set;

        if (succeeded(pf.pool.allocate(m_present_set_layout, set))) {
            const int fx0 = std::max(sc.full.x, 0), fy0 = std::max(sc.full.y, 0);
            const int fx1 = std::min(sc.full.x + static_cast<int>(sc.full.width), static_cast<int>(m_width));
            const int fy1 = std::min(sc.full.y + static_cast<int>(sc.full.height), static_cast<int>(m_height));

            if (fx1 > fx0 && fy1 > fy0) {
                vulkan::DescriptorWriter().image(0, m_upscale.view(), m_samplers[LinearClamp]).update(set);
                const vulkan::Rect2D full{ { fx0, fy0 }, { static_cast<std::uint32_t>(fx1 - fx0), static_cast<std::uint32_t>(fy1 - fy0) } };
                const float tw = static_cast<float>(m_width), th = static_cast<float>(m_height);

                const float push[8] = {
                    static_cast<float>(x0) / tw, static_cast<float>(y0) / th, static_cast<float>(x1) / tw, static_cast<float>(y1) / th,
                    1.0f / tw, 1.0f / th, m_sharpness, static_cast<float>(m_upscale_filter)
                };

                cmd.memory_barrier(
                    vulkan::PipelineStage::ColorAttachmentOutput, vulkan::Access::ColorAttachmentWrite,
                    vulkan::PipelineStage::ColorAttachmentOutput, vulkan::Access::ColorAttachmentRead | vulkan::Access::ColorAttachmentWrite
                );

                cmd.begin_rendering({ full, color_attachment(false) });
                cmd.bind(m_upscale_pipeline);
                cmd.set_viewport({ static_cast<float>(sc.full.x), static_cast<float>(sc.full.y), static_cast<float>(sc.full.width), static_cast<float>(sc.full.height), 0.0f, 1.0f });
                cmd.set_scissor(full);
                cmd.bind_descriptor_set(m_upscale_pipeline, 0, set);
                cmd.push_constants(m_upscale_pipeline, vulkan::ShaderStage::Fragment, push, kUpscalePushBytes);
                cmd.draw(kFullscreenVertices);
                cmd.end_rendering();
            }
        }

        cmd.end_label();
    }

    void set_light_3d(const graphics::Light3D& light) noexcept override {
        RendererImplBase::set_light_3d(light);
        if (m_in_3d && !m_scene_lights.empty()) legacy_uniforms(m_scene_lights.back().uniforms);
    }

    void set_scene_lighting(const graphics::SceneLighting3D& lighting) noexcept override {
        RendererImplBase::set_scene_lighting(lighting);
        if (!m_in_3d || m_scenes.empty() || m_scene_lights.empty()) return;

        try {
            m_scene_lights.back() = snapshot_lighting(m_scenes.back().scene);
        } catch (...) {}
    }

    void draw_mesh_3d(const graphics::Mesh3D& mesh, const float* model, const graphics::Material3D& mat) noexcept override {
        if (!m_ready || !m_in_3d || mesh.empty()) return;

        try {
            GpuMesh* g = mesh_for(mesh);
            if (!g) return;
            if (mat.casts_shadow() && casting()) add_caster(mesh_caster(*g), model, g->bounds);
            if (!mat.visible()) return;
            Draw3D d;
            d.kind  = DrawKind3D::Mesh;
            d.mesh  = g;
            d.count = g->index_count ? g->index_count : g->vertex_count;
            if (!material(d, mat)) return;
            mesh_push(d, model, mat);
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    void draw_triangles_3d(const graphics::Vertex3D* v, std::size_t count, const float* model, const graphics::Material3D& mat) noexcept override {
        if (!m_ready || !m_in_3d || !v || count < 3 || !mat.visible()) return;

        try {
            count -= count % 3;
            Draw3D d;
            d.kind  = DrawKind3D::Triangles;
            d.first = static_cast<std::uint32_t>(m_vertices3d.size());
            d.count = static_cast<std::uint32_t>(count);
            if (!material(d, mat)) return;
            mesh_push(d, model, mat);
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
            d.state = kLines | (1u << 2) | (depth_test ? kDepthTest : 0u);

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

            if (mat.casts_shadow() && casting() && ensure_quad_indices(g->vertex_count / graphics::QuadMesh3D::VERTICES_PER_QUAD)) {
                Caster c;
                c.kind     = Caster::Kind::Quads;
                c.vertices = g->vertices.get();
                c.groups   = g->groups;
                add_caster(c, model, g->bounds);
            }

            if (mat.visible()) push_quad_draw(*g, model, cell_origin, mat);
        } catch (...) {}
    }

    void upload_handle_3d(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot) noexcept override {
        if (!m_ready) { RendererImplBase::upload_handle_3d(slot); return; }
        if (!slot || slot->element_count == 0) return;
        if (resident_handle(*slot)) return;
        if (!slot->has_cpu_data()) { slot->lost = true; return; }

        try { upload_slot(slot); } catch (...) {}
    }

    void draw_handle_3d(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) noexcept override {
        if (!m_ready) { RendererImplBase::draw_handle_3d(slot, model, cell_origin, mat); return; }
        if (!m_in_3d || !slot || slot->element_count == 0 || slot->lost) return;

        try {
            HandleMesh* h = handle_for(slot);
            if (!h) return;
            const bool cast = mat.casts_shadow() && casting();

            if (h->quads) {
                if (!ensure_quad_indices(h->range.count)) return;
                if (cast) add_caster(arena_caster(*h, slot->groups, graphics::ALL_FACE_GROUPS), model, slot->bounds);
                if (!mat.visible()) return;
                Draw3D d;
                d.kind  = DrawKind3D::ArenaQuads;
                d.page  = h->range.page;
                d.vertex_offset = static_cast<std::int32_t>(h->range.first * graphics::QuadMesh3D::VERTICES_PER_QUAD);
                d.count = h->range.count * static_cast<std::uint32_t>(graphics::QuadMesh3D::INDICES_PER_QUAD);
                if (!material(d, mat)) return;
                d.state |= kQuads;
                quad_push(d, model, cell_origin, mat);
                m_draws3d.push_back(d);
                return;
            }

            h->mesh.last_used = m_frame_serial;
            if (cast) add_caster(mesh_caster(h->mesh), model, slot->bounds);
            if (!mat.visible()) return;
            Draw3D d;
            d.kind  = DrawKind3D::Mesh;
            d.mesh  = &h->mesh;
            d.count = h->mesh.index_count ? h->mesh.index_count : h->mesh.vertex_count;
            if (!material(d, mat)) return;
            mesh_push(d, model, mat);
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    static constexpr std::size_t kSwellPush = 24;

    void swell_into(float* out) const noexcept {
        const graphics::Swell3D& w = m_scene_lighting.surfaces.swell;
        const bool on = m_scene_lighting.enabled && w.active();
        out[0] = on ? w.height : 0.0f;
        out[1] = w.wavenumber;
        out[2] = w.dir_x;
        out[3] = w.dir_y;
        for (std::size_t i = 0; i < graphics::Swell3D::COUNT; ++i) out[4 + i] = w.phase[i];
        out[4 + graphics::Swell3D::COUNT] = w.detail;
    }

    void draw_quad_batch_3d(const QuadBatchDraw* items, std::size_t count, const float* camera_frac, const graphics::Material3D& mat) noexcept override {
        if (!m_ready) { RendererImplBase::draw_quad_batch_3d(items, count, camera_frac, mat); return; }
        if (!m_in_3d || !items || count == 0) return;

        try {
            const bool cast    = mat.casts_shadow() && casting();
            const bool visible = mat.visible();
            if (!cast && !visible) return;
            Draw3D base;
            if (!material(base, mat)) return;
            base.kind = DrawKind3D::Batch;
            base.state |= kBatch;
            std::copy(m_scenes.back().vp.begin(), m_scenes.back().vp.end(), base.push);
            for (int a = 0; a < 3; ++a) base.push[16 + a] = camera_frac[a];
            put_bits(base.push[23], material_flags(mat));
            swell_into(base.push + kSwellPush);
            if (m_page_commands.size() < m_arena.size()) m_page_commands.resize(m_arena.size());
            for (auto& cmds : m_page_commands) cmds.clear();
            std::uint32_t largest = 0;

            for (std::size_t i = 0; i < count; ++i) {
                const QuadBatchDraw& item = items[i];
                if (!item.slot || !item.slot->quads || item.slot->lost) continue;
                HandleMesh* h = resident(*item.slot);
                if (!h) h = handle_for(item.slot->shared_from_this());
                if (!h || !h->quads) continue;

                if (cast) {
                    float model[12] = { 1,0,0,0, 0,1,0,0, 0,0,1,0 };
                    for (int a = 0; a < 3; ++a) model[a * 4 + 3] = static_cast<float>(item.rel_cell[a]) + (item.frac[a] - camera_frac[a]);
                    add_caster(arena_caster(*h, item.slot->groups, item.faces), model, item.slot->bounds);
                    largest = std::max(largest, h->range.count);
                }

                if (!visible) continue;
                const std::uint32_t instance = static_cast<std::uint32_t>(m_batch_instances.size());
                BatchInstance inst;

                for (int a = 0; a < 3; ++a) {
                    inst.rel_cell[a] = item.rel_cell[a];
                    inst.abs_cell[a] = item.abs_cell[a];
                    inst.frac[a]     = item.frac[a];
                }

                bool any = false;
                std::uint32_t run_first = 0, run_count = 0;

                auto flush = [&]() {
                    if (run_count == 0) return;
                    if (m_page_commands.size() <= h->range.page) m_page_commands.resize(h->range.page + 1);
                    m_page_commands[h->range.page].push_back({
                        run_count * static_cast<std::uint32_t>(graphics::QuadMesh3D::INDICES_PER_QUAD), 1, 0,
                        static_cast<std::int32_t>((h->range.first + run_first) * graphics::QuadMesh3D::VERTICES_PER_QUAD), instance });
                    largest = std::max(largest, run_count);
                    run_count = 0;
                    any = true;
                };

                for (std::size_t g = 0; g < graphics::CELL_FACE_GROUPS; ++g) {
                    const graphics::QuadRange r = item.slot->groups[g];
                    if (r.count == 0) continue;
                    if (!(item.faces & (1u << g))) { flush(); continue; }
                    if (run_count == 0) run_first = r.first;
                    if (run_first + run_count != r.first) { flush(); run_first = r.first; }
                    run_count += r.count;
                }

                flush();
                if (any) m_batch_instances.push_back(inst);
            }

            if (largest == 0 || !ensure_quad_indices(largest) || !visible) return;

            for (std::size_t page = 0; page < m_page_commands.size(); ++page) {
                auto& cmds = m_page_commands[page];
                if (cmds.empty()) continue;
                Draw3D d = base;
                d.page  = static_cast<std::uint32_t>(page);
                d.first = static_cast<std::uint32_t>(m_batch_commands.size());
                d.count = static_cast<std::uint32_t>(cmds.size());
                m_batch_commands.insert(m_batch_commands.end(), cmds.begin(), cmds.end());
                m_draws3d.push_back(d);
            }
        } catch (...) {}
    }

    void draw_instances_3d(const graphics::Mesh3D& mesh, const float* model, const graphics::Instance3D* instances,
                           std::size_t count, const graphics::Material3D& mat) noexcept override {
        if (!m_ready) { RendererImplBase::draw_instances_3d(mesh, model, instances, count, mat); return; }
        if (!m_in_3d || mesh.empty() || !instances || count == 0) return;

        try {
            GpuMesh* g = mesh_for(mesh);
            if (!g) return;
            const std::uint32_t first = static_cast<std::uint32_t>(m_instances3d.size());
            const bool cast = mat.casts_shadow() && casting();
            if (!cast && !mat.visible()) return;
            m_instances3d.insert(m_instances3d.end(), instances, instances + count);

            if (cast) {
                Caster c = mesh_caster(*g);
                c.kind           = Caster::Kind::Instanced;
                c.instance_first = first;
                c.instance_count = static_cast<std::uint32_t>(count);
                add_caster(c, model, instance_bounds(g->bounds, instances, count));
            }

            if (!mat.visible()) return;
            Draw3D d;
            d.kind  = DrawKind3D::Instanced;
            d.mesh  = g;
            d.count = g->index_count ? g->index_count : g->vertex_count;
            if (!material(d, mat)) return;
            d.state |= kInstanced;
            mesh_push(d, model, mat);
            d.instance_first = first;
            d.instance_count = static_cast<std::uint32_t>(count);
            m_draws3d.push_back(d);
        } catch (...) {}
    }

    std::uint64_t gpu_mesh_bytes() const noexcept override { return m_mesh_bytes + m_handle_bytes; }

    std::uint64_t gpu_arena_bytes() const noexcept {
        std::uint64_t total = 0;
        for (const ArenaPage& p : m_arena) total += static_cast<std::uint64_t>(p.capacity) * kQuadBytes;
        return total;
    }

    void release_mesh(const graphics::Mesh3D& mesh) noexcept {
        auto it = m_meshes.find(&mesh);
        if (it == m_meshes.end()) return;
        retire(it->second);
        m_meshes.erase(it);
    }

private:
    static constexpr std::uint32_t kCullMask   = 3u;
    static constexpr std::uint32_t kCullBack   = 0u;
    static constexpr std::uint32_t kCullFront  = 2u;
    static constexpr std::uint32_t kDepthTest  = 1u << 4;
    static constexpr std::uint32_t kDepthWrite = 1u << 5;
    static constexpr std::uint32_t kLines      = 1u << 6;
    static constexpr std::uint32_t kQuads      = 1u << 7;
    static constexpr std::uint64_t kMinQuadIndexCapacity = 16384;

    static constexpr std::uint32_t kBatch      = 1u << 8;
    static constexpr std::uint32_t kInstanced  = 1u << 9;
    static constexpr std::uint32_t kArenaPageQuads = 1u << 18;
    static constexpr std::uint64_t kQuadBytes  = graphics::QuadMesh3D::VERTICES_PER_QUAD * sizeof(graphics::CompactVertex3D);

    static void put_bits(float& slot, std::uint32_t value) noexcept { std::memcpy(&slot, &value, sizeof(value)); }
    static void put_bits(float& slot, std::int32_t value) noexcept { std::memcpy(&slot, &value, sizeof(value)); }

    static std::uint32_t material_flags(const graphics::Material3D& m) noexcept { return (m.lit ? kFlagLit : 0u) | (m.fog ? 0u : kFlagNoFog); }

    void quad_push(Draw3D& d, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) const noexcept {
        mesh_push(d, model, mat);
        for (int r = 0; r < 3; ++r) put_bits(d.push[28 + r], cell_origin ? cell_origin[r] : 0);
        put_bits(d.push[31], material_flags(mat));
    }

    static void rgb_into(float* out, const graphics::Color& c, float scale) noexcept {
        for (int k = 0; k < 3; ++k) out[k] = graphics::detail::light_channel(c, k) * scale;
    }

    void legacy_uniforms(SceneUniforms& u) const noexcept {
        const double len = m_light3d.direction.magnitude();
        const double k = len > 0.0 ? m_light3d.diffuse / len : 0.0;
        u.legacy[0] = static_cast<float>(m_light3d.direction.x * k);
        u.legacy[1] = static_cast<float>(m_light3d.direction.y * k);
        u.legacy[2] = static_cast<float>(m_light3d.direction.z * k);
        u.legacy[3] = m_light3d.ambient;
    }

    static void set_row(Mat4f& m, int row, const vector3d& axis, double constant) noexcept {
        m[static_cast<std::size_t>(row * 4 + 0)] = static_cast<float>(axis.x);
        m[static_cast<std::size_t>(row * 4 + 1)] = static_cast<float>(axis.y);
        m[static_cast<std::size_t>(row * 4 + 2)] = static_cast<float>(axis.z);
        m[static_cast<std::size_t>(row * 4 + 3)] = static_cast<float>(constant);
    }

    static vector3d quantized_direction(const vector3d& direction, double step_degrees) noexcept {
        const vector3d d = direction / direction.magnitude();
        if (step_degrees <= 0.0) return d;
        const double step = step_degrees * kRadiansPerDegree;
        const double azimuth = std::round(std::atan2(d.y, d.x) / step) * step;
        const double elevation = std::round(std::asin(std::max(-1.0, std::min(1.0, d.z))) / step) * step;
        const double flat = std::cos(elevation);
        return { flat * std::cos(azimuth), flat * std::sin(azimuth), std::sin(elevation) };
    }

    struct SunAnchors {
        vector3d a, b;
        double   t     = 0.0;
        bool     blend = false;
    };

    SunAnchors sun_anchors(const vector3d& d, double step_degrees) const noexcept {
        const double step = step_degrees * kRadiansPerDegree;
        auto reset = [&]() { m_sun_a = m_sun_b = m_sun_prev = d; m_sun_anchored = true; };
        if (!m_sun_anchored || step <= 0.0) reset();
        const vector3d ab = m_sun_b - m_sun_a;
        const double span = ab.dot(ab);

        if (span <= 0.0) {
            const vector3d ad = d - m_sun_a;
            const double moved = ad.magnitude();

            if (moved > step * kAnchorStart) {
                const vector3d b = m_sun_a + ad * (step / moved);
                m_sun_b = b / b.magnitude();
            }
        }

        for (int guard = 0; guard < kAnchorAdvance; ++guard) {
            const vector3d seg = m_sun_b - m_sun_a;
            const double len2 = seg.dot(seg);
            if (len2 <= 0.0) break;
            const double t = (d - m_sun_a).dot(seg) / len2;
            const vector3d off = d - (m_sun_a + seg * t);

            if (t < -kAnchorBackTrack || off.magnitude() > step * kAnchorDrift) {
                reset();
                break;
            }

            if (t < 1.0) break;
            m_sun_prev = m_sun_a;
            m_sun_a = m_sun_b;
            const vector3d next = m_sun_a * 2.0 - m_sun_prev;
            m_sun_b = next / next.magnitude();
        }

        SunAnchors out;
        out.a = m_sun_a;
        out.b = m_sun_b;
        const vector3d seg = m_sun_b - m_sun_a;
        const double len2 = seg.dot(seg);
        out.blend = len2 > 0.0;
        out.t = out.blend ? std::max(0.0, std::min(1.0, (d - m_sun_a).dot(seg) / len2)) : 0.0;
        return out;
    }

    static double sun_cell(const graphics::SunShadow3D& s) noexcept {
        const double texel = 2.0 * s.distance / static_cast<double>(s.resolution);
        return std::max(texel, std::ceil(std::max(s.recenter, 0.0) * s.distance / texel) * texel);
    }

    static double sun_extent(const graphics::SunShadow3D& s) noexcept {
        return s.distance + 0.5 * sun_cell(s);
    }

    static void mix_hash(std::uint64_t& h, std::uint64_t v) noexcept {
        h = (h ^ v) * kHashPrime;
        h ^= h >> kHashShift;
    }

    static std::uint64_t grid_key(double v) noexcept {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(v * kSignatureGrid)));
    }

    static std::uint64_t float_key(float v) noexcept {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        return bits;
    }

    static Mat4f sun_matrix(
        const vector3d& direction, 
        const vector3d& origin, 
        const graphics::SunShadow3D& s, 
        double step_degrees, 
        std::uint64_t* key = nullptr
    ) noexcept {
        const vector3d f = quantized_direction(direction, step_degrees);
        const vector3d up = std::fabs(f.z) > kSunUpLimit ? vector3d{ 0.0, 1.0, 0.0 } : vector3d{ 0.0, 0.0, 1.0 };
        vector3d r = f.cross(up);
        r = r / r.magnitude();
        const vector3d u = r.cross(f);
        const double cell = sun_cell(s), extent = sun_extent(s);
        const double ox = origin.dot(r), oy = origin.dot(u), oz = origin.dot(f);
        const double cx = (std::floor(ox / cell) + 0.5) * cell, cy = (std::floor(oy / cell) + 0.5) * cell;
        const double near_z = std::floor(oz / cell) * cell - s.depth_range, range = s.depth_range + extent + cell;
        Mat4f m = mat4_identity();
        set_row(m, 0, r / extent, (ox - cx) / extent);
        set_row(m, 1, u / extent, (oy - cy) / extent);
        set_row(m, 2, f / range, (oz - near_z) / range);

        if (key) {
            std::uint64_t h = kHashSeed;
            for (double v : { f.x, f.y, f.z }) mix_hash(h, float_key(static_cast<float>(v)));
            for (double v : { cx, cy, near_z, extent, range }) mix_hash(h, grid_key(v));
            mix_hash(h, s.resolution);
            *key = h;
        }

        return m;
    }

    static void spot_uniform(float* out, const graphics::PointLight3D& p) noexcept {
        out[0] = out[1] = out[2] = 0.0f;
        out[3] = kNoCone;
        const double len = p.direction.magnitude();
        if (!p.is_spot() || len <= 0.0) return;
        const double outer = std::cos(p.cone * kRadiansPerDegree);
        const double inner = std::cos(p.cone * (1.0 - std::clamp(static_cast<double>(p.cone_softness), 0.0, 1.0)) * kRadiansPerDegree);
        const double sharp = 1.0 / std::max(inner - outer, kMinConeGap);
        const vector3d dir = p.direction / len * sharp;
        out[0] = static_cast<float>(dir.x); out[1] = static_cast<float>(dir.y); out[2] = static_cast<float>(dir.z);
        out[3] = static_cast<float>(outer);
    }

    static std::uint8_t cone_faces(const graphics::PointLight3D& p) noexcept {
        const double len = p.direction.magnitude();
        if (!p.is_spot() || len <= 0.0) return kAllCubeFaces;
        const vector3d dir = p.direction / len;
        const double reach = std::min(p.cone * kRadiansPerDegree + kCubeFaceSpread + kConeMargin, constants::pi());
        const double limit = std::cos(reach);
        const double along[kCubeFaces] = { dir.x, -dir.x, dir.y, -dir.y, dir.z, -dir.z };
        std::uint8_t mask = 0;
        for (int f = 0; f < kCubeFaces; ++f) if (along[f] >= limit) mask = static_cast<std::uint8_t>(mask | (1u << f));
        return mask == 0 ? kAllCubeFaces : mask;
    }

    static Mat4f cube_face_matrix(int face, const vector3d& light) noexcept {
        static const double axes[kCubeFaces][3][3] = {
            { {  0,  0, -1 }, {  0, -1,  0 }, {  1,  0,  0 } },
            { {  0,  0,  1 }, {  0, -1,  0 }, { -1,  0,  0 } },
            { {  1,  0,  0 }, {  0,  0,  1 }, {  0,  1,  0 } },
            { {  1,  0,  0 }, {  0,  0, -1 }, {  0, -1,  0 } },
            { {  1,  0,  0 }, {  0, -1,  0 }, {  0,  0,  1 } },
            { { -1,  0,  0 }, {  0, -1,  0 }, {  0,  0, -1 } },
        };

        const double (*a)[3] = axes[face];
        const vector3d sc{ a[0][0], a[0][1], a[0][2] }, tc{ a[1][0], a[1][1], a[1][2] }, ma{ a[2][0], a[2][1], a[2][2] };
        Mat4f m{};
        set_row(m, 0, sc, -sc.dot(light));
        set_row(m, 1, tc, -tc.dot(light));
        set_row(m, 2, ma, -ma.dot(light) - kPointNear);
        set_row(m, 3, ma, -ma.dot(light));
        return m;
    }

    void capsule_uniforms(SceneUniforms& u, const graphics::CapsuleShadows3D& c, const vector3d& origin) const {
        if (!c.enabled || c.strength <= 0.0f) return;
        std::vector<std::pair<double, std::size_t>>& order = m_capsule_order;
        order.clear();

        for (std::size_t i = 0; i < c.capsules.size(); ++i) {
            const graphics::CapsuleOccluder3D& o = c.capsules[i];
            if (o.radius <= 0.0f) continue;
            const vector3d mid = (o.start + o.end) * 0.5 - origin;
            order.emplace_back(mid.dot(mid), i);
        }

        const std::size_t n = std::min(order.size(), graphics::CapsuleShadows3D::MAX_CAPSULES);
        std::partial_sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(n), order.end());

        for (std::size_t k = 0; k < n; ++k) {
            const graphics::CapsuleOccluder3D& o = c.capsules[order[k].second];
            const vector3d a = o.start - origin, b = o.end - origin;
            u.capsule_a[k][0] = static_cast<float>(a.x); u.capsule_a[k][1] = static_cast<float>(a.y); u.capsule_a[k][2] = static_cast<float>(a.z); u.capsule_a[k][3] = o.radius;
            u.capsule_b[k][0] = static_cast<float>(b.x); u.capsule_b[k][1] = static_cast<float>(b.y); u.capsule_b[k][2] = static_cast<float>(b.z);
        }

        u.capsules[0] = static_cast<float>(n);
        u.capsules[1] = static_cast<float>(std::tan(c.sun_size * kSoftDegreesToSlope));
        u.capsules[2] = c.point_size;
        u.capsules[3] = c.strength;
    }

    SceneLightRec snapshot_lighting(const Scene3D& scene) const {
        SceneLightRec rec;
        SceneUniforms& u = rec.uniforms;
        legacy_uniforms(u);
        const graphics::SceneLighting3D& l = m_scene_lighting;
        if (!l.enabled) return rec;
        const vector3d origin{ scene.origin[0], scene.origin[1], scene.origin[2] };
        std::int32_t flags = kSceneEnabled;
        const double sun_len = l.sun_direction.magnitude();
        const vector3d sun = sun_len > 0.0 ? l.sun_direction / sun_len : vector3d{ 0.0, 0.0, -1.0 };
        u.sun_dir[0] = static_cast<float>(sun.x); u.sun_dir[1] = static_cast<float>(sun.y); u.sun_dir[2] = static_cast<float>(sun.z);
        rgb_into(u.sun_color, l.sun_color, std::max(l.sun_intensity, 0.0f));
        u.sun_color[3] = l.sun_exposure;
        rgb_into(u.sky_color, l.sky_color, l.sky_intensity);
        u.sky_color[3] = l.min_light;
        rgb_into(u.block_color, l.block_color, l.block_intensity);
        u.block_color[3] = l.falloff;
        u.params[0] = l.ambient;
        u.params[1] = l.max_light;

        std::vector<std::pair<double, std::size_t>>& all = m_light_order;
        all.clear();

        for (std::size_t i = 0; i < l.point_lights.size(); ++i) {
            const graphics::PointLight3D& p = l.point_lights[i];
            if (p.radius <= 0.0f || p.intensity <= 0.0f) continue;
            const vector3d d = p.position - origin;
            all.emplace_back(d.dot(d), i);
        }

        const std::size_t count = std::min(all.size(), graphics::SceneLighting3D::MAX_POINT_LIGHTS);
        std::sort(all.begin(), all.end());
        const unsigned int shadow_limit = l.point_shadows.enabled && l.point_shadows.resolution > 0 ? std::min<unsigned int>(l.point_shadows.max_lights, kPointShadowMaps) : 0u;
        const double band = std::max(l.point_shadows.fade_distance, 0.0);
        double shadow_cut = std::numeric_limits<double>::infinity();
        std::uint32_t casters = 0;

        for (const auto& entry : all) {
            if (!l.point_lights[entry.second].casts_shadows) continue;
            if (casters++ < shadow_limit) continue;
            shadow_cut = std::sqrt(entry.first);
            break;
        }

        const double count_cut = count < all.size() ? std::sqrt(all[count].first) : std::numeric_limits<double>::infinity();
        auto fade = [band](double cut, double dist) { return band > 0.0 ? std::max(0.0, std::min(1.0, (cut - dist) / band)) : (dist < cut ? 1.0 : 0.0); };

        for (std::size_t k = 0; k < count; ++k) {
            const graphics::PointLight3D& p = l.point_lights[all[k].second];
            const vector3d rel = p.position - origin;
            float* pos = u.point_pos[k];
            float* col = u.point_color[k];
            pos[0] = static_cast<float>(rel.x); pos[1] = static_cast<float>(rel.y); pos[2] = static_cast<float>(rel.z); pos[3] = p.radius;
            spot_uniform(u.point_spot[k], p);
            const double dist = std::sqrt(all[k].first);
            double weight = fade(count_cut, dist);
            col[3] = -1.0f;

            if (p.casts_shadows && shadow_limit > 0) {
                if (rec.point_count < shadow_limit) weight = std::min(weight, fade(shadow_cut, dist));
                else if (l.point_shadows.hide_unshadowed) weight = 0.0;
            }

            rgb_into(col, p.color, static_cast<float>(p.intensity * weight));

            if (p.casts_shadows && rec.point_count < shadow_limit) {
                PointShadowRec& ps = rec.points[rec.point_count];
                ps.light[0] = pos[0]; ps.light[1] = pos[1]; ps.light[2] = pos[2]; ps.light[3] = p.radius;
                const vector3d at{ pos[0], pos[1], pos[2] };
                for (int f = 0; f < kCubeFaces; ++f) ps.faces[static_cast<std::size_t>(f)] = cube_face_matrix(f, at);
                ps.key = kHashSeed;
                for (double v : { p.position.x, p.position.y, p.position.z }) mix_hash(ps.key, grid_key(v));
                mix_hash(ps.key, float_key(p.radius));
                mix_hash(ps.key, l.point_shadows.resolution);
                ps.mask   = cone_faces(p);
                ps.moving = p.moving;
                ps.id     = p.id;
                ps.world[0] = p.position.x; ps.world[1] = p.position.y; ps.world[2] = p.position.z;
                mix_hash(ps.key, ps.mask);

                if (p.is_spot()) {
                    for (double v : { p.direction.x, p.direction.y, p.direction.z }) mix_hash(ps.key, grid_key(v));
                    mix_hash(ps.key, float_key(p.cone));
                }
                col[3] = static_cast<float>(rec.point_count);
                ++rec.point_count;
            }
        }

        u.counts[0] = static_cast<std::int32_t>(count);
        capsule_uniforms(u, l.capsule_shadows, origin);

        if (rec.point_count > 0) {
            flags |= kPointShadows;
            rec.point_size = l.point_shadows.resolution;
            rec.moving_faces = l.point_shadows.moving_faces;
            u.point_shadow[0] = l.point_shadows.bias;
            u.point_shadow[1] = l.point_shadows.strength;
            u.point_shadow[2] = l.point_shadows.normal_offset;
            u.point_shadow[3] = kCubeTexelSpan * l.point_shadows.softness / static_cast<float>(l.point_shadows.resolution);
        }

        const graphics::SunShadow3D& s = l.sun_shadow;

        if (s.enabled && l.sun_intensity > 0.0f && s.resolution > 0 && s.distance > 0.0 && sun_len > 0.0) {
            const SunAnchors anchors = s.crossfade ? sun_anchors(sun, s.angle_step) : SunAnchors{};
            const vector3d dir_a = s.crossfade ? anchors.a : quantized_direction(sun, s.angle_step);
            const Mat4f m = sun_matrix(dir_a, origin, s, 0.0, &rec.sun_key_a);
            std::copy(m.begin(), m.end(), u.sun_matrix);
            rec.sun_dir_a[0] = static_cast<float>(dir_a.x); rec.sun_dir_a[1] = static_cast<float>(dir_a.y); rec.sun_dir_a[2] = static_cast<float>(dir_a.z);

            if (anchors.blend) {
                const Mat4f mb = sun_matrix(anchors.b, origin, s, 0.0, &rec.sun_key_b);
                std::copy(mb.begin(), mb.end(), u.sun_matrix_b);
                u.sun_mix[0] = static_cast<float>(anchors.t);
                u.sun_mix[1] = 1.0f;
                rec.sun_blend = true;
                rec.sun_dir_b[0] = static_cast<float>(anchors.b.x); rec.sun_dir_b[1] = static_cast<float>(anchors.b.y); rec.sun_dir_b[2] = static_cast<float>(anchors.b.z);
            }

            const double texel = 2.0 * sun_extent(s) / static_cast<double>(s.resolution);
            u.sun_shadow[0] = s.bias;
            u.sun_shadow[1] = static_cast<float>(s.normal_offset * texel);
            u.sun_shadow[2] = s.softness / static_cast<float>(s.resolution);
            u.sun_shadow[3] = s.strength;
            const double range = s.depth_range + sun_extent(s) + sun_cell(s);
            u.soft[0] = static_cast<float>(range * std::tan(s.light_size * kSoftDegreesToSlope) / (2.0 * sun_extent(s)));
            u.soft[1] = s.max_softness / static_cast<float>(s.resolution);
            u.soft[2] = static_cast<float>(std::max(1u, s.filter_taps));
            u.soft[3] = s.softness / static_cast<float>(s.resolution);
            if (s.soft) flags |= kSoftShadows;
            rec.sun_shadow = true;
            rec.sun_size = s.resolution;
            flags |= kSunShadows;
        }

        flags |= environment_uniforms(u, l, scene);
        u.counts[1] = flags;
        rec.wants_casters = rec.sun_shadow || rec.point_count > 0;
        const graphics::PlanarReflections3D& pr = l.planar;

        if (pr.enabled && pr.resolution > 0.0f) {
            const std::uint32_t limit = std::min<std::uint32_t>(pr.max_planes, static_cast<std::uint32_t>(kMaxPlanes));

            for (const graphics::ReflectionPlane3D& plane : pr.planes) {
                if (rec.plane_count >= limit) break;
                const double len = plane.normal.magnitude();
                if (len <= 0.0) continue;
                const vector3d n = plane.normal / len;
                const double d = n.dot(plane.point - origin);
                if (d >= -kPlaneFacing) continue;
                int* rect = rec.plane_rects[rec.plane_count];
                if (!plane_rect(plane, origin, scene, pr.distortion, rect)) continue;
                float* out = rec.planes[rec.plane_count];
                out[0] = static_cast<float>(n.x); out[1] = static_cast<float>(n.y); out[2] = static_cast<float>(n.z); out[3] = static_cast<float>(d);
                std::copy(out, out + 4, u.planes[rec.plane_count]);
                rec.plane_scales[rec.plane_count] = std::min(plane.resolution > 0.0f ? plane.resolution : pr.resolution, 1.0f);
                ++rec.plane_count;
            }

            if (rec.plane_count > 0) {
                u.plane_info[0] = static_cast<float>(rec.plane_count);
                u.plane_info[1] = pr.distortion;
                u.counts[1] |= kPlanarReflections;
            }
        }

        rec.wants_rays = l.shafts.enabled && l.shafts.samples > 0 && l.shafts.strength > 0.0f && l.sun_intensity > 0.0f && !l.medium.active;
        rec.wants_copy = rec.wants_rays || l.surfaces.refraction || (l.surfaces.screen_reflections && l.surfaces.reflection_steps > 0);
        if (rec.wants_rays) u.counts[1] |= kLightShafts;
        rec.sky = (flags & kAtmosphere) != 0;
        plan_volume(rec, l, scene);
        return rec;
    }

    static bool plane_rect(
        const graphics::ReflectionPlane3D& plane, 
        const vector3d& origin, 
        const Scene3D& scene, 
        float distortion, 
        int* rect
    ) noexcept {
        const int x0 = scene.x, y0 = scene.y;
        const int x1 = scene.x + static_cast<int>(scene.width), y1 = scene.y + static_cast<int>(scene.height);
        rect[0] = x0; rect[1] = y0; rect[2] = x1; rect[3] = y1;
        if (!plane.bounded) return true;
        double lo[2] = { std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
        double hi[2] = { -std::numeric_limits<double>::max(), -std::numeric_limits<double>::max() };
        const Mat4f& m = scene.view_proj;

        for (int k = 0; k < kBoxCorners; ++k) {
            const vector3d c{ (k & 1) ? plane.hi.x : plane.lo.x, (k & 2) ? plane.hi.y : plane.lo.y, (k & 4) ? plane.hi.z : plane.lo.z };
            const vector3d r = c - origin;
            double clip[4];

            for (int row = 0; row < 4; ++row)
                clip[row] = m[static_cast<std::size_t>(row * 4)] * r.x + m[static_cast<std::size_t>(row * 4 + 1)] * r.y + m[static_cast<std::size_t>(row * 4 + 2)] * r.z + m[static_cast<std::size_t>(row * 4 + 3)];
            
            if (clip[3] <= kPlaneNearW) return true;
            clip[1] = -clip[1];
            for (int a = 0; a < 2; ++a) { const double ndc = clip[a] / clip[3]; lo[a] = std::min(lo[a], ndc); hi[a] = std::max(hi[a], ndc); }
        }

        const double margin = static_cast<double>(distortion) + kPlaneRectMargin;
        const double w = scene.width, h = scene.height;
        rect[0] = std::max(x0, static_cast<int>(std::floor(x0 + ((lo[0] - margin) * 0.5 + 0.5) * w)));
        rect[1] = std::max(y0, static_cast<int>(std::floor(y0 + ((lo[1] - margin) * 0.5 + 0.5) * h)));
        rect[2] = std::min(x1, static_cast<int>(std::ceil(x0 + ((hi[0] + margin) * 0.5 + 0.5) * w)));
        rect[3] = std::min(y1, static_cast<int>(std::ceil(y0 + ((hi[1] + margin) * 0.5 + 0.5) * h)));
        return rect[2] > rect[0] && rect[3] > rect[1];
    }

    static void plan_volume(SceneLightRec& rec, const graphics::SceneLighting3D& l, const Scene3D& scene) noexcept {
        const std::int32_t flags = rec.uniforms.counts[1];
        if ((flags & kVolumetric) == 0 || (flags & kMedium) != 0 || scene.width == 0 || scene.height == 0) return;
        const unsigned int cell = std::max(1u, l.volumetrics.cell_size);
        rec.volume_size[0] = (scene.width + cell - 1) / cell;
        rec.volume_size[1] = (scene.height + cell - 1) / cell;
        rec.volume_size[2] = std::min(std::max(1u, l.volumetrics.steps), kMaxVolumeSlices);
        for (int a = 0; a < 3; ++a) rec.uniforms.volume_grid[a] = static_cast<float>(rec.volume_size[a]);
        rec.uniforms.counts[1] |= kVolumeGrid;
        rec.volume = true;
    }

    static void unit_into(float* out, const vector3d& v) noexcept {
        const double len = v.magnitude();
        const vector3d n = len > 0.0 ? v / len : vector3d{ 0.0, 0.0, 1.0 };
        out[0] = static_cast<float>(n.x); out[1] = static_cast<float>(n.y); out[2] = static_cast<float>(n.z);
    }

    static std::int32_t environment_uniforms(
        SceneUniforms& u, 
        const graphics::SceneLighting3D& l, 
        const Scene3D& scene
    ) noexcept {
        std::int32_t flags = 0;
        const graphics::Atmosphere3D& a = l.atmosphere;
        rgb_into(u.sky_zenith, a.zenith, 1.0f);
        rgb_into(u.sky_horizon, a.horizon, 1.0f);
        rgb_into(u.sky_glow, a.glow, 1.0f);
        u.sky_zenith[3]  = a.stars;
        u.sky_horizon[3] = a.fog_density;
        u.sky_glow[3]    = a.glow_strength;
        unit_into(u.sky_sun, a.sun_position);
        unit_into(u.sky_moon, a.moon_position);
        u.sky_sun[3]  = static_cast<float>(std::cos(a.sun_radius * kRadiansPerDegree));
        u.sky_moon[3] = static_cast<float>(std::cos(a.moon_radius * kRadiansPerDegree));
        u.sky_params[0] = a.fog_start;
        u.sky_params[1] = static_cast<float>(l.time);
        u.sky_params[2] = l.medium.density;
        u.sky_params[3] = l.tone_map.exposure;
        rgb_into(u.medium_color, l.medium.color, 1.0f);
        u.medium_color[3] = l.tone_map.saturation;
        u.volume[0] = l.volumetrics.density;
        u.volume[1] = l.volumetrics.anisotropy;
        u.volume[2] = l.volumetrics.distance;
        u.volume[3] = static_cast<float>(l.volumetrics.steps);
        const graphics::Swell3D& sw = l.surfaces.swell;
        u.swell[0] = sw.active() ? sw.height : 0.0f;
        u.swell[1] = sw.wavenumber;
        u.swell[2] = sw.dir_x;
        u.swell[3] = sw.dir_y;
        for (std::size_t i = 0; i < graphics::Swell3D::COUNT; ++i) u.swell_phase[i] = sw.phase[i];
        u.swell_phase[3] = sw.detail;
        u.waves[0] = l.surfaces.wave_strength;
        u.waves[1] = l.surfaces.wave_scale;
        u.waves[2] = l.surfaces.wave_speed;
        u.waves[3] = l.surfaces.reflectivity;
        u.gloss[0] = l.surfaces.specular_power;
        u.gloss[1] = l.surfaces.specular;
        u.gloss[2] = a.sun_disk;
        u.gloss[3] = a.moon_disk;
        rgb_into(u.sun_disk, a.sun_disk_color, a.sun_disk);
        u.sun_disk[3] = a.glow_focus;
        u.glow[0] = a.glow_spread;
        u.glow[1] = l.shafts.focus;
        u.water[0] = l.surfaces.refraction ? l.surfaces.refraction_strength : 0.0f;
        u.water[1] = l.surfaces.refraction ? l.surfaces.absorption : 0.0f;
        u.water[2] = l.surfaces.scattering;
        u.water[3] = l.surfaces.refraction ? 1.0f : 0.0f;
        u.target[2] = static_cast<float>(l.surfaces.reflection_steps);
        u.target[3] = l.surfaces.screen_reflections && l.surfaces.reflection_steps > 0 ? l.surfaces.reflection_distance : 0.0f;
        u.rays[0] = l.shafts.strength;
        u.rays[1] = static_cast<float>(l.shafts.samples);
        u.rays[2] = l.shafts.decay;
        u.rays[3] = l.shafts.length;
        u.shafts[0] = l.volumetrics.intensity;
        u.shafts[1] = l.volumetrics.near_bias;
        Mat4f vp = scene.view_proj;
        for (int c = 0; c < 4; ++c) vp[static_cast<std::size_t>(4 + c)] = -vp[static_cast<std::size_t>(4 + c)];
        const Mat4f inv = mat4_inverse(vp);
        std::copy(inv.begin(), inv.end(), u.inv_view_proj);
        std::copy(vp.begin(), vp.end(), u.volume_proj);
        if (a.enabled) flags |= kAtmosphere;
        if (l.volumetrics.enabled && l.volumetrics.steps > 0) flags |= kVolumetric;
        if (l.tone_map.enabled) flags |= kToneMap;
        if (l.medium.active) flags |= kMedium;
        return flags;
    }

    static Mat4f mat4_inverse(const Mat4f& m) noexcept {
        double a[4][8];

        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 8; ++c)
                a[r][c] = c < 4 ? m[static_cast<std::size_t>(r * 4 + c)] : (c - 4 == r ? 1.0 : 0.0);

        for (int col = 0; col < 4; ++col) {
            int pivot = col;
            for (int r = col + 1; r < 4; ++r) if (std::fabs(a[r][col]) > std::fabs(a[pivot][col])) pivot = r;
            if (std::fabs(a[pivot][col]) < 1e-12) return mat4_identity();
            if (pivot != col) for (int c = 0; c < 8; ++c) std::swap(a[pivot][c], a[col][c]);
            const double inv = 1.0 / a[col][col];
            for (int c = 0; c < 8; ++c) a[col][c] *= inv;

            for (int r = 0; r < 4; ++r) {
                if (r == col) continue;
                const double k = a[r][col];
                for (int c = 0; c < 8; ++c) a[r][c] -= k * a[col][c];
            }
        }

        Mat4f out{};
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) out[static_cast<std::size_t>(r * 4 + c)] = static_cast<float>(a[r][c + 4]);
        return out;
    }

    bool casting() const noexcept { return !m_scene_lights.empty() && m_scene_lights.back().wants_casters; }

    static void caster_bounds(Caster& c, const graphics::Bounds3D& b) noexcept {
        if (!b.valid) {
            for (int a = 0; a < 3; ++a) { c.lo[a] = -std::numeric_limits<float>::max(); c.hi[a] = std::numeric_limits<float>::max(); }
            return;
        }

        for (int a = 0; a < 3; ++a) {
            const float* row = &c.model[a * 4];
            float lo = row[3], hi = row[3];

            for (int k = 0; k < 3; ++k) {
                const float x = row[k] * b.lo[k], y = row[k] * b.hi[k];
                lo += std::min(x, y);
                hi += std::max(x, y);
            }

            c.lo[a] = lo;
            c.hi[a] = hi;
        }
    }

    static void caster_model(Caster& c, const float* model) noexcept {
        if (model) std::copy(model, model + 12, c.model);
        const float eps = 1e-6f;
        c.axis_aligned = true;

        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 3; ++k)
                if (std::fabs(c.model[r * 4 + k] - (r == k ? 1.0f : 0.0f)) > eps) c.axis_aligned = false;
    }

    void add_caster(Caster c, const float* model, const graphics::Bounds3D& local) {
        caster_model(c, model);
        caster_bounds(c, local);
        m_casters.push_back(c);
    }

    static graphics::FaceGroups whole_groups(std::uint32_t quads) noexcept {
        graphics::FaceGroups g{};
        g[graphics::CELL_FACE_GROUPS - 1] = { 0, quads };
        return g;
    }

    void push_quad_draw(GpuMesh& g, const float* model, const std::int32_t* cell_origin, const graphics::Material3D& mat) {
        if (!ensure_quad_indices(g.vertex_count / graphics::QuadMesh3D::VERTICES_PER_QUAD)) return;
        Draw3D d;
        d.kind  = DrawKind3D::Mesh;
        d.mesh  = &g;
        d.quads = true;
        d.count = g.index_count;
        if (!material(d, mat)) return;
        d.state |= kQuads;
        quad_push(d, model, cell_origin, mat);
        m_draws3d.push_back(d);
    }

    static Caster mesh_caster(const GpuMesh& g) noexcept {
        Caster c;
        c.kind       = Caster::Kind::Mesh;
        c.vertices   = g.vertices.get();
        c.indices    = g.indices.get();
        c.count      = g.index_count ? g.index_count : g.vertex_count;
        c.generation = g.generation;
        return c;
    }

    Caster arena_caster(const HandleMesh& h, const graphics::FaceGroups& groups, graphics::FaceMask faces) const noexcept {
        Caster c;
        c.kind        = Caster::Kind::Quads;
        c.vertices    = h.range.page < m_arena.size() ? m_arena[h.range.page].buffer.get() : nullptr;
        c.vertex_base = static_cast<std::int32_t>(h.range.first * graphics::QuadMesh3D::VERTICES_PER_QUAD);
        c.groups      = groups;
        c.faces       = faces;
        c.generation  = h.mesh.generation;
        return c;
    }

    static graphics::Bounds3D instance_bounds(const graphics::Bounds3D& mesh, const graphics::Instance3D* instances, std::size_t count) noexcept {
        graphics::Bounds3D out;
        if (!mesh.valid) return out;

        for (std::size_t i = 0; i < count; ++i) {
            const graphics::Instance3D& in = instances[i];
            const float s = in.scale;
            out.include(in.x + std::min(mesh.lo[0] * s, mesh.hi[0] * s), in.y + std::min(mesh.lo[1] * s, mesh.hi[1] * s), in.z + std::min(mesh.lo[2] * s, mesh.hi[2] * s));
            out.include(in.x + std::max(mesh.lo[0] * s, mesh.hi[0] * s), in.y + std::max(mesh.lo[1] * s, mesh.hi[1] * s), in.z + std::max(mesh.lo[2] * s, mesh.hi[2] * s));
        }

        return out;
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
        m_buffer_uploads.push_back({ m_quad_indices.get(), offset, bytes, nullptr, 0 });
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
        m_buffer_uploads.push_back({ g.vertices.get(), push_staging(vdata, static_cast<std::size_t>(vbytes)), vbytes, track_owner ? &g : nullptr, 0 });
        if (ibytes) m_buffer_uploads.push_back({ g.indices.get(), push_staging(idata, static_cast<std::size_t>(ibytes)), ibytes, track_owner ? &g : nullptr, 0 });
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
        fresh.bounds       = graphics::Bounds3D::of(verts);
        fresh.groups       = quads.grouped() ? quads.groups() : whole_groups(static_cast<std::uint32_t>(quads.quad_count()));
        GpuMesh* g = nullptr;
        if (it != m_meshes.end()) { retire(it->second); it->second = std::move(fresh); g = &it->second; }
        else g = &m_meshes.emplace(&quads, std::move(fresh)).first->second;
        if (!create_gpu_buffers(*g, verts.data(), vbytes, nullptr, 0, true)) { m_meshes.erase(&quads); return nullptr; }
        m_mesh_bytes += g->bytes;
        return g;
    }

    bool arena_alloc(std::uint32_t quads, ArenaRange& out) {
        for (std::uint32_t p = 0; p < m_arena.size(); ++p) {
            ArenaPage& page = m_arena[p];

            for (auto it = page.free.begin(); it != page.free.end(); ++it) {
                if (it->second < quads) continue;
                out = { p, it->first, quads };
                const std::uint32_t rest_first = it->first + quads, rest = it->second - quads;
                page.free.erase(it);
                if (rest) page.free.emplace(rest_first, rest);
                page.used += quads;
                m_arena_quads_used += quads;
                return true;
            }
        }

        ArenaPage page;
        page.capacity = std::max(quads, kArenaPageQuads);
        page.buffer = std::make_unique<vulkan::Buffer>();
        if (failed(page.buffer->create(m_device, { page.capacity * kQuadBytes, vulkan::BufferUsage::Vertex, vulkan::MemoryUsage::GpuOnly, "fizmo quad arena" }))) return false;
        if (page.capacity > quads) page.free.emplace(quads, page.capacity - quads);
        page.used = quads;
        out = { static_cast<std::uint32_t>(m_arena.size()), 0, quads };
        m_arena.push_back(std::move(page));
        m_arena_quads_used += quads;
        return true;
    }

    void arena_free(const ArenaRange& r) {
        if (r.count == 0 || r.page >= m_arena.size()) return;
        ArenaPage& page = m_arena[r.page];
        std::uint32_t first = r.first, count = r.count;
        auto next = page.free.lower_bound(first);

        if (next != page.free.begin()) {
            auto prev = std::prev(next);
            if (prev->first + prev->second == first) { first = prev->first; count += prev->second; page.free.erase(prev); }
        }

        if (next != page.free.end() && first + count == next->first) { count += next->second; page.free.erase(next); }
        page.free.emplace(first, count);
        page.used -= std::min(page.used, r.count);
        m_arena_quads_used -= std::min<std::uint64_t>(m_arena_quads_used, r.count);
    }

    bool resident_handle(const graphics::detail::MeshSlot3D& slot) const noexcept {
        return slot.owner_epoch == m_epoch && slot.gpu_key && m_handle_meshes.count(slot.gpu_key);
    }

    HandleMesh* resident(const graphics::detail::MeshSlot3D& slot) noexcept {
        if (slot.owner_epoch != m_epoch || !slot.gpu_key) return nullptr;
        auto it = m_handle_meshes.find(slot.gpu_key);
        return it == m_handle_meshes.end() ? nullptr : &it->second;
    }

    HandleMesh* handle_for(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot) {
        if (HandleMesh* h = resident(*slot)) return h;
        if (!slot->has_cpu_data()) { slot->lost = true; return nullptr; }
        if (!upload_slot(slot)) return nullptr;
        auto it = m_handle_meshes.find(slot->gpu_key);
        return it == m_handle_meshes.end() ? nullptr : &it->second;
    }

    bool upload_slot(const std::shared_ptr<graphics::detail::MeshSlot3D>& slot) {
        if (!slot->gpu_key) slot->gpu_key = ++m_next_handle_key;
        slot->release = m_release_queue;
        release_handle(slot->gpu_key);
        HandleMesh h;
        h.slot  = slot;
        h.quads = slot->quads;
        h.mesh.generation = ++m_mesh_generation;

        if (slot->quads) {
            const auto& verts = slot->quad_mesh.vertices();
            const std::uint32_t quads = static_cast<std::uint32_t>(slot->quad_mesh.quad_count());
            if (!arena_alloc(quads, h.range)) return false;
            const std::uint64_t bytes = quads * kQuadBytes;
            m_buffer_uploads.push_back({ m_arena[h.range.page].buffer.get(), push_staging(verts.data(), static_cast<std::size_t>(bytes)), bytes, nullptr, h.range.first * kQuadBytes });
            h.bytes = bytes;
        } else {
            const auto& verts = slot->mesh.vertices();
            const auto& idx   = slot->mesh.indices();
            h.mesh.vertex_count = static_cast<std::uint32_t>(verts.size());
            h.mesh.index_count  = static_cast<std::uint32_t>(idx.size());
            h.mesh.last_used    = m_frame_serial;
            if (!create_gpu_buffers(h.mesh, verts.data(), verts.size() * sizeof(graphics::Vertex3D), idx.data(), idx.size() * sizeof(std::uint32_t), false)) return false;
            h.bytes = h.mesh.bytes;
        }

        slot->owner_epoch = m_epoch;
        slot->gpu_bytes   = h.bytes;
        slot->resident    = false;
        slot->lost        = false;
        m_handle_bytes += h.bytes;
        m_handle_meshes.emplace(slot->gpu_key, std::move(h));
        m_handle_uploads.push_back({ slot->gpu_key, slot });
        return true;
    }

    void release_handle(std::uint64_t key) {
        auto it = m_handle_meshes.find(key);
        if (it == m_handle_meshes.end()) return;
        HandleMesh& h = it->second;
        m_handle_bytes -= std::min(m_handle_bytes, h.bytes);

        if (h.quads) {
            m_retired_ranges.push_back({ m_frame_serial, h.range });
        } else {
            if (h.mesh.vertices) m_retired_buffers.push_back({ m_frame_serial, std::move(h.mesh.vertices) });
            if (h.mesh.indices)  m_retired_buffers.push_back({ m_frame_serial, std::move(h.mesh.indices) });
        }

        m_handle_meshes.erase(it);
    }

    void commit_handle_uploads() noexcept {
        for (const PendingHandle& p : m_handle_uploads) {
            if (p.slot->gpu_key != p.key || !m_handle_meshes.count(p.key)) continue;
            p.slot->resident = true;
            p.slot->drop_cpu_data();
        }

        m_handle_uploads.clear();
        m_quad_index_pending = false;
    }

    void abandon_handle_uploads() noexcept {
        for (const PendingHandle& p : m_handle_uploads) {
            release_handle(p.key);
            p.slot->owner_epoch = 0;
            p.slot->resident = false;
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
        d.on_camera      = m.camera_visible();
        d.in_reflections = m.reflected();
        d.sky            = !m.fog;

        if (m.texture && m.texture->valid()) {
            d.view    = texture_view(*m.texture);
            d.sampler = sampler_for(*m.texture);
        } else {
            d.view    = &m_white.view();
            d.sampler = NearestClamp;
        }

        return d.view != nullptr;
    }

    void mesh_push(Draw3D& d, const float* model, const graphics::Material3D& mat) const noexcept {
        const Mat4f& vp = m_scenes.back().vp;
        Mat4f m = mat4_identity();
        if (model) std::copy(model, model + 16, m.begin());
        const Mat4f mvp = model ? mat4_mul(vp, m) : vp;
        std::copy(mvp.begin(), mvp.end(), d.push);
        std::copy(m.begin(), m.begin() + 12, d.push + 16);
        put_bits(d.push[28], mat.light);
        put_bits(d.push[29], material_flags(mat) | kFlagLegacy);
        d.push[30] = 0.0f;
        d.push[31] = 0.0f;
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
        fresh.generation   = ++m_mesh_generation;
        fresh.last_used    = m_frame_serial;
        fresh.bytes        = vbytes + ibytes;
        fresh.vertex_count = static_cast<std::uint32_t>(verts.size());
        fresh.index_count  = static_cast<std::uint32_t>(idx.size());
        fresh.bounds       = graphics::Bounds3D::of(verts);

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
        const bool lines     = (key & kLines) != 0;
        const bool batch     = (key & kBatch) != 0;
        const bool quads     = (key & kQuads) != 0 || batch;
        const bool instanced = (key & kInstanced) != 0;
        vulkan::GraphicsPipelineDesc d;
        d.vertex_shader   = lines ? &m_line_vs : batch ? &m_batch_vs : quads ? &m_quad_vs : instanced ? &m_instanced_vs : &m_mesh_vs;
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
        } else if (batch) {
            d.vertex_bindings   = {
                { 0, sizeof(graphics::CompactVertex3D), vulkan::VertexRate::PerVertex },
                { 1, sizeof(BatchInstance), vulkan::VertexRate::PerInstance },
            };

            d.vertex_attributes = {
                { 0, 0, vulkan::Format::RGB32Float, 0 }, { 1, 0, vulkan::Format::RGBA8Unorm, 12 },
                { 2, 0, vulkan::Format::R32Uint, 16 },   { 3, 0, vulkan::Format::RGBA8Unorm, 20 },
                { 4, 1, vulkan::Format::RGB32Sint, 0 },  { 5, 1, vulkan::Format::RGB32Sint, 12 },
                { 6, 1, vulkan::Format::RGB32Float, 24 },
            };

            const std::uint32_t cull = key & 3u;
            d.cull_mode = cull == 1u ? vulkan::CullMode::None : cull == 2u ? vulkan::CullMode::Front : vulkan::CullMode::Back;
        } else if (quads) {
            d.vertex_bindings   = { { 0, sizeof(graphics::CompactVertex3D), vulkan::VertexRate::PerVertex } };
            
            d.vertex_attributes = {
                { 0, 0, vulkan::Format::RGB32Float, 0 }, { 1, 0, vulkan::Format::RGBA8Unorm, 12 },
                { 2, 0, vulkan::Format::R32Uint, 16 },   { 3, 0, vulkan::Format::RGBA8Unorm, 20 },
            };

            const std::uint32_t cull = key & 3u;
            d.cull_mode = cull == 1u ? vulkan::CullMode::None : cull == 2u ? vulkan::CullMode::Front : vulkan::CullMode::Back;
        } else {
            d.vertex_bindings   = { { 0, sizeof(graphics::Vertex3D), vulkan::VertexRate::PerVertex } };
            
            d.vertex_attributes = {
                { 0, 0, vulkan::Format::RGB32Float, 0 },  { 1, 0, vulkan::Format::RGB32Float, 12 },
                { 2, 0, vulkan::Format::RG32Float, 24 },  { 3, 0, vulkan::Format::RGBA8Unorm, 32 },
            };

            if (instanced) {
                d.vertex_bindings.push_back({ 1, sizeof(graphics::Instance3D), vulkan::VertexRate::PerInstance });
                d.vertex_attributes.push_back({ 4, 1, vulkan::Format::RGBA32Float, 0 });
                d.vertex_attributes.push_back({ 5, 1, vulkan::Format::RGBA8Unorm, 16 });
                d.vertex_attributes.push_back({ 6, 1, vulkan::Format::RGBA8Unorm, 20 });
            }

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

        d.name = lines ? "fizmo 3d lines" : batch ? "fizmo 3d quad batch" : quads ? "fizmo 3d quads" : instanced ? "fizmo 3d instanced" : "fizmo 3d mesh";
        vulkan::Pipeline p;
        if (failed(p.create(m_device, d))) return nullptr;
        return &m_pipelines3d.emplace(key, std::move(p)).first->second;
    }

    const vulkan::Pipeline* sky_pipeline() {
        if (m_sky_pipeline.valid()) return &m_sky_pipeline;
        vulkan::GraphicsPipelineDesc d;
        d.vertex_shader   = &m_sky_vs;
        d.fragment_shader = &m_sky_fs;
        d.layout          = &m_mesh_layout;
        d.cull_mode       = vulkan::CullMode::None;
        d.samples         = m_samples;
        d.color_formats   = { vulkan::Format::RGBA8Unorm };
        d.depth_format    = m_depth_format;
        d.depth.test      = true;
        d.depth.write     = false;
        d.depth.compare   = vulkan::CompareOp::LessEqual;
        d.blend           = { vulkan::BlendState::opaque() };
        d.name            = "fizmo sky";
        if (failed(m_sky_pipeline.create(m_device, d))) return nullptr;
        return &m_sky_pipeline;
    }

    const vulkan::Pipeline* rays_pipeline() {
        if (m_rays_pipeline.valid()) return &m_rays_pipeline;
        vulkan::GraphicsPipelineDesc d;
        d.vertex_shader   = &m_sky_vs;
        d.fragment_shader = &m_rays_fs;
        d.layout          = &m_mesh_layout;
        d.cull_mode       = vulkan::CullMode::None;
        d.samples         = m_samples;
        d.color_formats   = { vulkan::Format::RGBA8Unorm };
        d.depth_format    = m_depth_format;
        d.depth.test      = false;
        d.depth.write     = false;
        vulkan::BlendState add;
        add.enable    = true;
        add.src_color = vulkan::BlendFactor::One; add.dst_color = vulkan::BlendFactor::One;
        add.src_alpha = vulkan::BlendFactor::Zero; add.dst_alpha = vulkan::BlendFactor::One;
        d.blend           = { add };
        d.name            = "fizmo light shafts";
        if (failed(m_rays_pipeline.create(m_device, d))) return nullptr;
        return &m_rays_pipeline;
    }

    vulkan::Pipeline* shadow_pipeline(Caster::Kind kind, bool point) {
        const std::uint32_t key = static_cast<std::uint32_t>(kind) | (point ? kShadowPointKey : 0u);
        auto it = m_shadow_pipelines.find(key);
        if (it != m_shadow_pipelines.end()) return &it->second;
        vulkan::GraphicsPipelineDesc d;
        d.layout          = &m_shadow_layout;
        d.fragment_shader = point ? &m_shadow_point_fs : nullptr;

        if (kind == Caster::Kind::Quads) {
            d.vertex_shader     = &m_shadow_quad_vs;
            d.vertex_bindings   = { { 0, sizeof(graphics::CompactVertex3D), vulkan::VertexRate::PerVertex } };
            d.vertex_attributes = { { 0, 0, vulkan::Format::RGB32Float, 0 }, { 2, 0, vulkan::Format::R32Uint, 16 } };
        } else {
            d.vertex_shader     = kind == Caster::Kind::Instanced ? &m_shadow_instanced_vs : &m_shadow_mesh_vs;
            d.vertex_bindings   = { { 0, sizeof(graphics::Vertex3D), vulkan::VertexRate::PerVertex } };
            d.vertex_attributes = { { 0, 0, vulkan::Format::RGB32Float, 0 }, { 1, 0, vulkan::Format::RGB32Float, 12 } };

            if (kind == Caster::Kind::Instanced) {
                d.vertex_bindings.push_back({ 1, sizeof(graphics::Instance3D), vulkan::VertexRate::PerInstance });
                d.vertex_attributes.push_back({ 4, 1, vulkan::Format::RGBA32Float, 0 });
            }
        }

        d.cull_mode          = vulkan::CullMode::None;
        d.depth_format       = m_shadow_format;
        d.depth.test         = true;
        d.depth.write        = true;
        d.depth.compare      = vulkan::CompareOp::LessEqual;
        d.depth.clamp        = !point;
        d.depth.bias         = !point;
        d.depth.bias_constant = kShadowBiasConstant;
        d.depth.bias_slope    = kShadowBiasSlope;
        d.name = point ? "fizmo point shadow" : "fizmo sun shadow";
        vulkan::Pipeline p;
        if (failed(p.create(m_device, d))) return nullptr;
        return &m_shadow_pipelines.emplace(key, std::move(p)).first->second;
    }

    void drop_3d_recording() noexcept {
        const bool reopen = m_in_3d && !m_scenes.empty() && !m_scene_lights.empty();
        SceneRec open;
        SceneLightRec open_light;

        if (reopen) {
            open = m_scenes.back();
            open_light = m_scene_lights.back();
        }

        m_scenes.clear();
        m_scene_lights.clear();
        m_casters.clear();
        m_draws3d.clear();
        m_vertices3d.clear();
        m_lines3d.clear();

        if (reopen) {
            open.batch_index  = 0;
            open.first_draw   = 0;
            open.first_caster = 0;
            m_scenes.push_back(open);
            m_scene_lights.push_back(open_light);
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
        if (failed(m_upscale_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kUpscaleFrag, sizeof(gpu::kUpscaleFrag) / 4), "fizmo upscale.frag"))) return false;
        vulkan::PipelineLayoutDesc ul;
        ul.set_layouts    = { &m_present_set_layout };
        ul.push_constants = { { vulkan::ShaderStage::Fragment, 0, kUpscalePushBytes } };
        if (failed(m_upscale_layout.create(m_device, ul))) return false;
        vulkan::GraphicsPipelineDesc upscale;
        upscale.vertex_shader   = &m_present_vs;
        upscale.fragment_shader = &m_upscale_fs;
        upscale.layout          = &m_upscale_layout;
        upscale.samples         = m_samples;
        upscale.color_formats   = { vulkan::Format::RGBA8Unorm };
        upscale.name            = "fizmo upscale";
        if (failed(m_upscale_pipeline.create(m_device, upscale))) return false;
        if (failed(m_mesh_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kMesh3DVert, sizeof(gpu::kMesh3DVert) / 4), "fizmo mesh3d.vert"))) return false;
        if (failed(m_mesh_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kMesh3DFrag, sizeof(gpu::kMesh3DFrag) / 4), "fizmo mesh3d.frag"))) return false;
        if (failed(m_line_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kLine3DVert, sizeof(gpu::kLine3DVert) / 4), "fizmo line3d.vert"))) return false;
        if (failed(m_line_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kLine3DFrag, sizeof(gpu::kLine3DFrag) / 4), "fizmo line3d.frag"))) return false;
        if (failed(m_quad_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kQuad3DVert, sizeof(gpu::kQuad3DVert) / 4), "fizmo quad3d.vert"))) return false;
        if (failed(m_quad_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kQuad3DFrag, sizeof(gpu::kQuad3DFrag) / 4), "fizmo quad3d.frag"))) return false;
        if (failed(m_batch_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kBatch3DVert, sizeof(gpu::kBatch3DVert) / 4), "fizmo batch3d.vert"))) return false;
        if (failed(m_instanced_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kInstanced3DVert, sizeof(gpu::kInstanced3DVert) / 4), "fizmo instanced3d.vert"))) return false;
        if (failed(m_shadow_mesh_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kShadowMesh3DVert, sizeof(gpu::kShadowMesh3DVert) / 4), "fizmo shadow_mesh.vert"))) return false;
        if (failed(m_shadow_instanced_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kShadowInstanced3DVert, sizeof(gpu::kShadowInstanced3DVert) / 4), "fizmo shadow_instanced.vert"))) return false;
        if (failed(m_shadow_quad_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kShadowQuad3DVert, sizeof(gpu::kShadowQuad3DVert) / 4), "fizmo shadow_quad.vert"))) return false;
        if (failed(m_shadow_point_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kShadowPoint3DFrag, sizeof(gpu::kShadowPoint3DFrag) / 4), "fizmo shadow_point.frag"))) return false;
        if (failed(m_sky_vs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kSky3DVert, sizeof(gpu::kSky3DVert) / 4), "fizmo sky.vert"))) return false;
        if (failed(m_sky_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kSky3DFrag, sizeof(gpu::kSky3DFrag) / 4), "fizmo sky.frag"))) return false;
        if (failed(m_rays_fs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kRays3DFrag, sizeof(gpu::kRays3DFrag) / 4), "fizmo rays.frag"))) return false;
        if (failed(m_volume_cs.create(m_device, vulkan::Span<std::uint32_t>(gpu::kVolume3DComp, sizeof(gpu::kVolume3DComp) / 4), "fizmo volume.comp"))) return false;
        m_multi_draw = m_device.features().multi_draw_indirect && m_device.features().draw_indirect_first_instance;

        if (
            failed(
                m_scene_set_layout.create(
                    m_device, {
                        vulkan::DescriptorBinding{ 0, vulkan::DescriptorType::UniformBuffer, kSceneStages },
                        vulkan::DescriptorBinding{ 1, vulkan::DescriptorType::CombinedImageSampler, kSceneStages },
                        vulkan::DescriptorBinding{ 2, vulkan::DescriptorType::CombinedImageSampler, kSceneStages, static_cast<std::uint32_t>(kPointShadowMaps) },
                        vulkan::DescriptorBinding{ 3, vulkan::DescriptorType::CombinedImageSampler, kSceneStages },
                        vulkan::DescriptorBinding{ 4, vulkan::DescriptorType::CombinedImageSampler, kSceneStages },
                        vulkan::DescriptorBinding{ 5, vulkan::DescriptorType::CombinedImageSampler, kSceneStages },
                        vulkan::DescriptorBinding{ 6, vulkan::DescriptorType::CombinedImageSampler, kSceneStages },
                        vulkan::DescriptorBinding{ 7, vulkan::DescriptorType::CombinedImageSampler, kSceneStages },
                        vulkan::DescriptorBinding{ 8, vulkan::DescriptorType::CombinedImageSampler, kSceneStages, static_cast<std::uint32_t>(kMaxPlanes) },
                        vulkan::DescriptorBinding{ 9, vulkan::DescriptorType::CombinedImageSampler, kSceneStages }
                    }
                )
            )
        ) return false;

        if (failed(m_volume_set_layout.create(m_device, { vulkan::DescriptorBinding{ 0, vulkan::DescriptorType::StorageImage, vulkan::ShaderStage::Compute } }))) return false;
        vulkan::PipelineLayoutDesc vl;
        vl.set_layouts = { &m_volume_set_layout, &m_scene_set_layout };
        if (failed(m_volume_layout.create(m_device, vl))) return false;
        vulkan::ComputePipelineDesc vd;
        vd.shader = &m_volume_cs;
        vd.layout = &m_volume_layout;
        vd.name   = "fizmo volume light";
        if (failed(m_volume_pipeline.create(m_device, vd))) return false;
        if (!create_dummy_volume()) return false;
        vulkan::PipelineLayoutDesc ml;
        ml.set_layouts    = { &m_draw_set_layout, &m_scene_set_layout };
        ml.push_constants = { { vulkan::ShaderStage::Vertex, 0, 128 } };
        if (failed(m_mesh_layout.create(m_device, ml))) return false;
        vulkan::PipelineLayoutDesc sl;
        sl.push_constants = { { vulkan::ShaderStage::Vertex | vulkan::ShaderStage::Fragment, 0, 128 } };
        if (failed(m_shadow_layout.create(m_device, sl))) return false;
        const std::uint64_t align = std::max<std::uint64_t>(1, m_device.limits().minUniformBufferOffsetAlignment);
        m_uniform_stride = (sizeof(SceneUniforms) + align - 1) / align * align;
        if (!create_shadow_resources()) return false;
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
            pd.sizes = { { vulkan::DescriptorType::CombinedImageSampler, 4096 }, { vulkan::DescriptorType::UniformBuffer, 256 }, { vulkan::DescriptorType::StorageImage, kMaxStorageSets } };
            if (failed(pf.pool.create(m_device, pd))) return false;
        }

        m_ramp_pixels.assign(static_cast<std::size_t>(256) * kRampRows * 4, 0);
        create_targets(w, h);
        return m_target.valid();
    }

    bool create_dummy_volume() {
        const bool linear = m_device.physical_device().supports_format(vulkan::Format::R32Float, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        m_volume_sampler = linear ? LinearClamp : NearestClamp;
        vulkan::ImageDesc d;
        d.extent = { 1, 1, 1 };
        d.type   = vulkan::ImageType::Image3D;
        d.format = vulkan::Format::R32Float;
        d.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::Storage;
        d.name   = "fizmo empty volume";
        if (failed(m_dummy_volume.create(m_device, d))) return false;
        return succeeded(submit_immediate(m_device, [&](vulkan::CommandBuffer& cmd) { cmd.transition(m_dummy_volume, vulkan::ImageLayout::ShaderReadOnly); }));
    }

    bool ensure_volume(const std::uint32_t* size) {
        if (m_volume && m_volume->extent().width == size[0] && m_volume->extent().height == size[1] && m_volume->extent().depth == size[2]) return true;
        if (m_volume) m_retired.push_back({ m_frame_serial, std::move(m_volume) });
        vulkan::ImageDesc d;
        d.extent = { size[0], size[1], size[2] };
        d.type   = vulkan::ImageType::Image3D;
        d.format = vulkan::Format::R32Float;
        d.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::Storage;
        d.name   = "fizmo volume light";
        m_volume = std::make_unique<vulkan::Image>();
        if (succeeded(m_volume->create(m_device, d))) return true;
        m_volume.reset();
        return false;
    }

    void prepare_volumes() {
        for (SceneLightRec& sl : m_scene_lights) {
            if (!sl.volume || ensure_volume(sl.volume_size)) continue;
            sl.volume = false;
            sl.uniforms.counts[1] &= ~kVolumeGrid;
        }
    }

    void record_volume_pass(const vulkan::CommandBuffer& cmd, PerFrame& pf, std::size_t scene) {
        const SceneLightRec& sl = m_scene_lights[scene];
        if (!sl.volume || !m_volume) return;
        vulkan::DescriptorSet target;
        if (failed(pf.pool.allocate(m_volume_set_layout, target))) return;
        const vulkan::DescriptorSet lighting = scene_descriptor(pf, scene, false, -1, false);
        if (!lighting.valid()) return;
        vulkan::DescriptorWriter().storage_image(0, m_volume->view()).update(target);
        cmd.begin_label("fizmo volume light");
        cmd.transition(*m_volume, vulkan::ImageLayout::General);
        cmd.bind(m_volume_pipeline);
        cmd.bind_descriptor_set(m_volume_pipeline, 0, target);
        cmd.bind_descriptor_set(m_volume_pipeline, 1, lighting);
        cmd.dispatch((sl.volume_size[0] + kVolumeGroup - 1) / kVolumeGroup, (sl.volume_size[1] + kVolumeGroup - 1) / kVolumeGroup);
        cmd.transition(*m_volume, vulkan::ImageLayout::ShaderReadOnly);
        cmd.end_label();
    }

    bool create_shadow_resources() {
        constexpr VkFormatFeatureFlags need = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        const auto& phys = m_device.physical_device();
        m_shadow_format = phys.supports_format(vulkan::Format::D32Float, need) ? vulkan::Format::D32Float : vulkan::Format::D16Unorm;
        const bool linear = phys.supports_format(m_shadow_format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        vulkan::SamplerDesc s;
        s.mag_filter = s.min_filter = linear ? vulkan::Filter::Linear : vulkan::Filter::Nearest;
        s.mipmap = vulkan::MipmapMode::Nearest;
        s.address_u = s.address_v = s.address_w = vulkan::AddressMode::ClampToEdge;
        s.max_lod = 0.0f;
        s.compare_enable = true;
        s.compare_op = vulkan::CompareOp::LessEqual;
        if (failed(m_shadow_sampler.create(m_device, s))) return false;
        m_dummy_sun  = make_shadow_map(1, false);
        m_dummy_cube = make_shadow_map(1, true);
        if (!m_dummy_sun || !m_dummy_cube) return false;

        return succeeded(submit_immediate(m_device, [&](vulkan::CommandBuffer& cmd) {
            clear_shadow_map(cmd, *m_dummy_sun);
            clear_shadow_map(cmd, *m_dummy_cube);
        }));
    }

    std::unique_ptr<ShadowMap> make_shadow_map(unsigned int size, bool cube) {
        auto map = std::make_unique<ShadowMap>();
        map->size = size;
        map->cube = cube;
        map->image = std::make_unique<vulkan::Image>();
        vulkan::ImageDesc d;
        d.extent = { size, size, 1 };
        d.format = m_shadow_format;
        d.usage  = vulkan::ImageUsage::DepthAttachment | vulkan::ImageUsage::Sampled;
        d.type   = cube ? vulkan::ImageType::Cube : vulkan::ImageType::Image2D;
        d.name   = cube ? "fizmo point shadow map" : "fizmo sun shadow map";
        if (failed(map->image->create(m_device, d))) return nullptr;

        if (cube) {
            vulkan::ImageViewDesc all;
            all.as_cube = true;
            if (failed(map->sample_view.create(m_device, *map->image, all))) return nullptr;

            for (int f = 0; f < kCubeFaces; ++f) {
                vulkan::ImageViewDesc face;
                face.base_layer  = static_cast<std::uint32_t>(f);
                face.layer_count = 1;
                if (failed(map->face_views[static_cast<std::size_t>(f)].create(m_device, *map->image, face))) return nullptr;
            }
        }

        return map;
    }

    const vulkan::ImageView& sample_view(const ShadowMap& map) const noexcept { return map.cube ? map.sample_view : map.image->view(); }

    void clear_shadow_map(const vulkan::CommandBuffer& cmd, ShadowMap& map) const noexcept {
        cmd.transition(*map.image, vulkan::ImageLayout::DepthAttachment);

        for (int f = 0; f < (map.cube ? kCubeFaces : 1); ++f) {
            vulkan::DepthAttachment depth;
            depth.view  = &map.attachment(f);
            depth.load  = vulkan::LoadOp::Clear;
            depth.store = vulkan::StoreOp::Store;
            depth.clear = vulkan::ClearDepth{ 1.0f, 0 };
            vulkan::RenderingDesc rd;
            rd.area  = { { 0, 0 }, { map.size, map.size } };
            rd.depth = &depth;
            cmd.begin_rendering(rd);
            cmd.end_rendering();
        }

        cmd.transition(*map.image, vulkan::ImageLayout::ShaderReadOnly);
    }

    ShadowMap* ensure_shadow_map(std::unique_ptr<ShadowMap>& slot, unsigned int size, bool cube) {
        if (slot && slot->size == size) return slot.get();
        if (slot) m_retired_maps.push_back({ m_frame_serial, std::move(slot) });
        slot = make_shadow_map(size, cube);
        return slot.get();
    }

    vulkan::SampleCount pick_samples() const noexcept {
        const VkSampleCountFlags supported = m_device.limits().framebufferColorSampleCounts;
        for (std::uint32_t s = FIZMO_GPU_MSAA; s > 1; s >>= 1)
            if (supported & s) return static_cast<vulkan::SampleCount>(s);
        return vulkan::SampleCount::X1;
    }

    void create_scene_copies() noexcept {
        m_scene_depth_view.destroy();
        vulkan::ImageDesc cd;
        cd.extent = { m_width, m_height, 1 };
        cd.format = vulkan::Format::RGBA8Unorm;
        cd.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferDst;
        cd.name   = "fizmo scene color copy";
        if (failed(m_scene_color.create(m_device, cd))) { m_scene_color.destroy(); return; }
        vulkan::ImageDesc dd;
        dd.extent = { m_width, m_height, 1 };
        dd.format = m_depth_format;
        dd.usage  = vulkan::ImageUsage::Sampled | vulkan::ImageUsage::TransferDst | vulkan::ImageUsage::DepthAttachment;
        dd.name   = "fizmo scene depth copy";
        if (failed(m_scene_depth.create(m_device, dd))) { m_scene_depth.destroy(); return; }
        vulkan::ImageViewDesc vd;
        vd.aspect = vulkan::ImageAspect::Depth;
        if (failed(m_scene_depth_view.create(m_device, m_scene_depth, vd))) m_scene_depth_view.destroy();
    }

    void copy_scene(const vulkan::CommandBuffer& cmd, vulkan::Rect2D area) {
        cmd.memory_barrier(
            vulkan::PipelineStage::ColorAttachmentOutput | vulkan::PipelineStage::LateFragmentTests,
            vulkan::Access::ColorAttachmentWrite | vulkan::Access::DepthAttachmentWrite,
            vulkan::PipelineStage::Transfer | vulkan::PipelineStage::FragmentShader | vulkan::PipelineStage::EarlyFragmentTests | vulkan::PipelineStage::ColorAttachmentOutput,
            vulkan::Access::TransferRead | vulkan::Access::ShaderRead | vulkan::Access::DepthAttachmentRead | vulkan::Access::DepthAttachmentWrite |
            vulkan::Access::ColorAttachmentRead | vulkan::Access::ColorAttachmentWrite
        );
        
        cmd.transition(m_target, vulkan::ImageLayout::TransferSrc);
        cmd.transition(m_scene_color, vulkan::ImageLayout::TransferDst);
        cmd.copy_image(m_target, m_scene_color, area);

        if (!m_msaa_target.valid()) {
            cmd.transition(m_depth, vulkan::ImageLayout::TransferSrc);
            cmd.transition(m_scene_depth, vulkan::ImageLayout::TransferDst);
            cmd.copy_image(m_depth, m_scene_depth, area);
            cmd.transition(m_depth, vulkan::ImageLayout::DepthAttachment);
        }

        cmd.transition(m_target, vulkan::ImageLayout::ColorAttachment);
        cmd.transition(m_scene_color, vulkan::ImageLayout::ShaderReadOnly);
        cmd.transition(m_scene_depth, vulkan::ImageLayout::ShaderReadOnly);
    }

    void prepare_depth_resolve(const vulkan::CommandBuffer& cmd) {
        vulkan::ImageBarrier b;
        b.image      = m_scene_depth.handle();
        b.old_layout = m_scene_depth.layout();
        b.new_layout = vulkan::ImageLayout::DepthAttachment;
        b.aspect     = m_scene_depth.aspect();
        b.src_stage  = vulkan::PipelineStage::FragmentShader | vulkan::PipelineStage::Transfer;
        b.src_access = vulkan::Access::None;
        b.dst_stage  = vulkan::PipelineStage::ColorAttachmentOutput;
        b.dst_access = vulkan::Access::ColorAttachmentWrite | vulkan::Access::ColorAttachmentRead;
        cmd.barrier(b);
        m_scene_depth.set_layout(vulkan::ImageLayout::DepthAttachment);
    }

    static bool blended(const Draw3D& d) noexcept { return d.kind != DrawKind3D::Lines && ((d.state >> kBlendShift) & kBlendMask) != 0u; }

    void prepare_reflections() {
        for (SceneLightRec& sl : m_scene_lights) {
            if (sl.plane_count == 0) continue;

            for (std::uint32_t k = 0; k < sl.plane_count; ++k) {
                const unsigned int w = std::max(1u, static_cast<unsigned int>(std::lround(m_width * sl.plane_scales[k])));
                const unsigned int h = std::max(1u, static_cast<unsigned int>(std::lround(m_height * sl.plane_scales[k])));
                if (ensure_reflection_target(k, w, h)) continue;
                sl.plane_count = k;
                break;
            }

            sl.uniforms.plane_info[0] = static_cast<float>(sl.plane_count);
            if (sl.plane_count == 0) sl.uniforms.counts[1] &= ~kPlanarReflections;
        }
    }

    SceneUniforms reflection_uniforms(std::size_t scene, std::uint32_t plane) const {
        const SceneLightRec& sl = m_scene_lights[scene];
        SceneUniforms u = sl.uniforms;
        u.counts[1] &= ~(kSceneCopy | kScreenReflections | kLightShafts | kPlanarReflections | kSoftShadows);
        u.counts[1] |= kReflectionPass;
        u.volume[3] = std::max(1.0f, std::floor(u.volume[3] / static_cast<float>(kReflectionVolumeDivisor)));
        std::copy(sl.planes[plane], sl.planes[plane] + 4, u.clip);
        const Mat4f vpr = reflected_view_proj(scene, plane);
        std::copy(vpr.begin(), vpr.end(), u.view_proj);
        const Mat4f inv = mat4_inverse(vpr);
        std::copy(inv.begin(), inv.end(), u.inv_view_proj);
        return u;
    }

    void plan_scene_copies() {
        const bool possible = m_scene_color.valid() && m_scene_depth_view.valid();

        for (std::size_t si = 0; si < m_scenes.size() && si < m_scene_lights.size(); ++si) {
            SceneLightRec& sl = m_scene_lights[si];
            const SceneRec& sc = m_scenes[si];
            SceneUniforms& u = sl.uniforms;
            std::copy(sc.vp.begin(), sc.vp.end(), u.view_proj);
            const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
            u.screen[0] = static_cast<float>(sc.scene.x) / w;
            u.screen[1] = static_cast<float>(sc.scene.y) / h;
            u.screen[2] = static_cast<float>(sc.scene.width) / w;
            u.screen[3] = static_cast<float>(sc.scene.height) / h;
            u.target[0] = 1.0f / w;
            u.target[1] = 1.0f / h;


            for (std::uint32_t k = 0; k < sl.plane_count; ++k) {
                const int* r = sl.plane_rects[k];
                u.plane_rects[k][0] = static_cast<float>(r[0]) / w; u.plane_rects[k][1] = static_cast<float>(r[1]) / h;
                u.plane_rects[k][2] = static_cast<float>(r[2]) / w; u.plane_rects[k][3] = static_cast<float>(r[3]) / h;
            }

            sl.copy = false;
            u.counts[1] &= ~(kSceneCopy | kScreenReflections);
            if (!sl.wants_copy || !possible) continue;
            const std::size_t end = si + 1 < m_scenes.size() ? m_scenes[si + 1].first_draw : m_draws3d.size();

            for (std::size_t i = sc.first_draw; i < end; ++i) {
                const Draw3D& d = m_draws3d[i];
                if (!blended(d) || d.sky || d.count == 0 || !d.on_camera) continue;
                sl.copy = true;
                sl.copy_at = static_cast<std::uint32_t>(i);
                break;
            }

            if (!sl.copy && sl.wants_rays) {
                sl.copy = true;
                sl.copy_at = static_cast<std::uint32_t>(end);
            }

            if (sl.copy) u.counts[1] |= kSceneCopy | (sl.uniforms.target[3] > 0.0f ? kScreenReflections : 0);
        }
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
        dd.usage   = m_samples == vulkan::SampleCount::X1 ? vulkan::ImageUsage::DepthAttachment | vulkan::ImageUsage::TransferSrc : vulkan::ImageUsage::DepthAttachment;
        dd.samples = m_samples;
        dd.name    = "fizmo depth buffer";
        m_depth.create(m_device, dd);
        create_scene_copies();
        m_upscale.destroy();
        if (m_render_scale < kFullRenderScale) create_upscale();
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
        m_scene_lights.clear();
        m_casters.clear();
        m_draws3d.clear();
        m_vertices3d.clear();
        m_lines3d.clear();
        m_batch_instances.clear();
        m_batch_commands.clear();
        m_instances3d.clear();
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

        m_retired_maps.erase(
            std::remove_if(m_retired_maps.begin(), m_retired_maps.end(), [&](const RetiredMap& r) { return safe(r.frame); }),
            m_retired_maps.end()
        );

        for (auto it = m_meshes.begin(); it != m_meshes.end();) {
            if (it->second.last_used + kMeshEvictFrames <= now) { retire(it->second); it = m_meshes.erase(it); }
            else ++it;
        }

        m_release_queue->drain([this](std::uint64_t key) { release_handle(key); });

        m_retired_ranges.erase(
            std::remove_if(m_retired_ranges.begin(), m_retired_ranges.end(), [&](const RetiredRange& r) {
                if (!safe(r.frame)) return false;
                arena_free(r.range);
                return true;
            }),
            m_retired_ranges.end()
        );

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

    void plan_point_shadows(SceneLightRec& sl, std::size_t scene) {
        if (scene >= m_scenes.size()) return;
        const std::size_t first = m_scenes[scene].first_caster;
        const std::size_t last = scene + 1 < m_scenes.size() ? m_scenes[scene + 1].first_caster : m_casters.size();
        const double* origin = m_scenes[scene].scene.origin;

        for (std::uint32_t k = 0; k < sl.point_count; ++k)
            m_point_signatures[k] = shadow_signature(sl.points[k].key, first, last, nullptr, sl.points[k].light, origin);

        for (std::uint32_t k = 0; k < sl.point_count; ++k)
            if (!holds(m_point_maps[k], m_point_signatures[k])) claim(sl, k, [&](const ShadowMap& m) { return m.cached && m.signature == m_point_signatures[k]; });

        unsigned int moving_faces = 0;
        m_point_kept = 0;
        const std::uint32_t turn = sl.point_count > 0 ? m_moving_turn++ % sl.point_count : 0;

        for (std::uint32_t step = 0; step < sl.point_count; ++step) {
            const std::uint32_t k = (step + turn) % sl.point_count;
            PointShadowRec& ps = sl.points[k];
            ps.render = !holds(m_point_maps[k], m_point_signatures[k]);
            const unsigned int faces = face_count(ps.mask);

            if (ps.render && ps.moving && sl.moving_faces > 0 && moving_faces + faces > sl.moving_faces && ps.id != 0) {
                auto stale = [&](const ShadowMap& m) { return m.cached && m.light_id == ps.id && (m.mask & ps.mask) == ps.mask; };
                if (stale(*m_point_maps[k]) || claim(sl, k, stale)) { ps.render = false; m_point_kept |= 1u << k; }
            }

            if (ps.render && ps.moving) moving_faces += faces;
        }

        for (std::uint32_t k = 0; k < sl.point_count; ++k) {
            const PointShadowRec& ps = sl.points[k];
            const ShadowMap& map = *m_point_maps[k];
            const double* world = ps.render ? ps.world : map.world;
            float* out = sl.uniforms.point_map[k];
            for (int a = 0; a < 3; ++a) out[a] = static_cast<float>(world[a] - origin[a]);
            out[3] = ps.render ? ps.light[3] : map.radius;
        }
    }

    static unsigned int face_count(std::uint8_t mask) noexcept {
        unsigned int n = 0;
        for (int f = 0; f < kCubeFaces; ++f) if (mask & (1u << f)) ++n;
        return n;
    }

    template <typename Match>
    bool claim(const SceneLightRec& sl, std::uint32_t k, Match&& match) {
        for (std::size_t j = 0; j < m_point_maps.size(); ++j) {
            if (j == k || !m_point_maps[j] || (m_point_kept & (1u << j)) || !match(*m_point_maps[j])) continue;
            if (j < sl.point_count && holds(m_point_maps[j], m_point_signatures[j])) continue;
            if (m_point_maps[j]->size != m_point_maps[k]->size) continue;
            std::swap(m_point_maps[j], m_point_maps[k]);
            return true;
        }

        return false;
    }

    void prepare_shadow_maps() {
        for (SceneLightRec& sl : m_scene_lights) {
            if (sl.sun_shadow && !ensure_shadow_map(m_sun_map, sl.sun_size, false)) {
                sl.sun_shadow = false;
                sl.uniforms.counts[1] &= ~kSunShadows;
            }

            if (!sl.sun_shadow || (sl.sun_blend && !ensure_shadow_map(m_sun_map_b, sl.sun_size, false))) {
                sl.sun_blend = false;
                sl.uniforms.sun_mix[0] = 0.0f;
                sl.uniforms.sun_mix[1] = 0.0f;
            }

            for (std::uint32_t k = 0; k < sl.point_count; ++k) {
                if (ensure_shadow_map(m_point_maps[k], sl.point_size, true)) continue;
                sl.point_count = k;
                break;
            }

            if (sl.point_count == 0) sl.uniforms.counts[1] &= ~kPointShadows;
            else plan_point_shadows(sl, static_cast<std::size_t>(&sl - m_scene_lights.data()));

            for (int i = 0; i < sl.uniforms.counts[0]; ++i)
                if (sl.uniforms.point_color[i][3] >= static_cast<float>(sl.point_count)) sl.uniforms.point_color[i][3] = -1.0f;
        }
    }

    std::size_t reflection_slot(std::size_t scene, int plane) const noexcept {
        return m_scene_lights.size() + scene * static_cast<std::size_t>(kMaxPlanes) + static_cast<std::size_t>(plane);
    }

    vulkan::DescriptorSet scene_descriptor(PerFrame& pf, std::size_t scene, bool copies = false, int reflection_plane = -1, bool volume = true) {
        const std::uint64_t key = (static_cast<std::uint64_t>(scene) << kSceneSetShift) | (static_cast<std::uint64_t>(reflection_plane + 1) << 2) | (copies ? 2u : 0u) | (volume ? 1u : 0u);
        for (const auto& cached : pf.scene_sets) if (cached.first == key) return cached.second;
        vulkan::DescriptorSet set;
        if (failed(pf.pool.allocate(m_scene_set_layout, set))) return set;
        pf.scene_sets.emplace_back(key, set);
        const SceneLightRec& sl = m_scene_lights[scene];
        const ShadowMap& sun = sl.sun_shadow && m_sun_map ? *m_sun_map : *m_dummy_sun;
        vulkan::DescriptorWriter w;
        const std::size_t slot = reflection_plane >= 0 ? reflection_slot(scene, reflection_plane) : scene;
        w.buffer(0, pf.scene_uniforms, vulkan::DescriptorType::UniformBuffer, slot * m_uniform_stride, sizeof(SceneUniforms));

        for (int k = 0; k < kMaxPlanes; ++k) {
            const ReflectionTarget& t = m_reflections[static_cast<std::size_t>(k)];
            const bool used = reflection_plane < 0 && static_cast<std::uint32_t>(k) < sl.plane_count && t.color;
            w.image(8, used ? t.color->view() : m_white.view(), m_samplers[LinearClamp], vulkan::ImageLayout::ShaderReadOnly,
                    vulkan::DescriptorType::CombinedImageSampler, static_cast<std::uint32_t>(k));
        }

        w.image(1, sample_view(sun), m_shadow_sampler);
        const bool have = copies && m_scene_color.valid() && m_scene_depth_view.valid();
        w.image(3, have ? m_scene_color.view() : m_white.view(), m_samplers[LinearClamp]);
        w.image(4, have ? m_scene_depth_view : m_white.view(), m_samplers[NearestClamp]);
        w.image(5, sample_view(sun), m_samplers[NearestClamp]);
        const ShadowMap& sun_b = sl.sun_blend && m_sun_map_b ? *m_sun_map_b : sun;
        w.image(6, sample_view(sun_b), m_shadow_sampler);
        w.image(7, sample_view(sun_b), m_samplers[NearestClamp]);
        const bool grid = volume && sl.volume && m_volume;
        w.image(9, grid ? m_volume->view() : m_dummy_volume.view(), m_samplers[m_volume_sampler]);

        for (int k = 0; k < kPointShadowMaps; ++k) {
            const bool used = static_cast<std::uint32_t>(k) < sl.point_count && m_point_maps[static_cast<std::size_t>(k)];
            const ShadowMap& map = used ? *m_point_maps[static_cast<std::size_t>(k)] : *m_dummy_cube;
            w.image(2, sample_view(map), m_shadow_sampler, vulkan::ImageLayout::ShaderReadOnly, vulkan::DescriptorType::CombinedImageSampler, static_cast<std::uint32_t>(k));
        }

        w.update(set);
        return set;
    }

    static bool holds(const std::unique_ptr<ShadowMap>& map, std::uint64_t signature) noexcept {
        return map && map->cached && map->signature == signature;
    }

    void render_shadow_map(const vulkan::CommandBuffer& cmd, PerFrame& pf, ShadowMap& map, std::uint64_t signature, const Mat4f* faces, const float* light, bool point, std::size_t first, std::size_t last, std::uint8_t mask = kAllCubeFaces) {
        cmd.transition(*map.image, vulkan::ImageLayout::DepthAttachment);
        const int views = point ? kCubeFaces : 1;

        for (int f = 0; f < views; ++f) {
            if (point && (mask & (1u << f)) == 0) continue;
            record_shadow_view(cmd, pf, map, f, faces[static_cast<std::size_t>(f)].data(), light, point, first, last);
        }

        cmd.transition(*map.image, vulkan::ImageLayout::ShaderReadOnly);
        map.signature = signature;
        map.cached    = true;
    }

    void record_shadow_passes(const vulkan::CommandBuffer& cmd, PerFrame& pf, std::size_t scene, std::size_t first, std::size_t last) {
        const SceneLightRec& sl = m_scene_lights[scene];
        if (!sl.sun_shadow && sl.point_count == 0) return;
        const double* origin = m_scenes[scene].scene.origin;
        cmd.begin_label("fizmo shadows");

        if (sl.sun_shadow && m_sun_map) {
            Mat4f ma, mb;
            std::copy(sl.uniforms.sun_matrix, sl.uniforms.sun_matrix + 16, ma.begin());
            std::copy(sl.uniforms.sun_matrix_b, sl.uniforms.sun_matrix_b + 16, mb.begin());
            const std::uint64_t sig_a = shadow_signature(sl.sun_key_a, first, last, ma.data(), nullptr, origin);
            const bool blend = sl.sun_blend && m_sun_map_b;
            const std::uint64_t sig_b = blend ? shadow_signature(sl.sun_key_b, first, last, mb.data(), nullptr, origin) : 0;
            if (!holds(m_sun_map, sig_a) && holds(m_sun_map_b, sig_a)) std::swap(m_sun_map, m_sun_map_b);
            if (blend && !holds(m_sun_map_b, sig_b) && holds(m_sun_map, sig_b)) std::swap(m_sun_map, m_sun_map_b);
            const float light_a[4] = { sl.sun_dir_a[0], sl.sun_dir_a[1], sl.sun_dir_a[2], 0.0f };
            const float light_b[4] = { sl.sun_dir_b[0], sl.sun_dir_b[1], sl.sun_dir_b[2], 0.0f };
            if (!holds(m_sun_map, sig_a)) render_shadow_map(cmd, pf, *m_sun_map, sig_a, &ma, light_a, false, first, last);
            if (blend && !holds(m_sun_map_b, sig_b)) render_shadow_map(cmd, pf, *m_sun_map_b, sig_b, &mb, light_b, false, first, last);
        }

        for (std::uint32_t k = 0; k < sl.point_count; ++k) {
            const PointShadowRec& ps = sl.points[k];
            if (!ps.render || !m_point_maps[k]) continue;
            ShadowMap& map = *m_point_maps[k];
            render_shadow_map(cmd, pf, map, m_point_signatures[k], ps.faces, ps.light, true, first, last, ps.mask);
            map.light_id = ps.id;
            map.mask     = ps.mask;
            map.radius   = ps.light[3];
            std::copy(ps.world, ps.world + 3, map.world);
        }

        cmd.end_label();
    }

    static bool outside_sphere(const Caster& c, const float* light) noexcept {
        float dist2 = 0.0f;

        for (int a = 0; a < 3; ++a) {
            const float v = light[a] < c.lo[a] ? c.lo[a] - light[a] : (light[a] > c.hi[a] ? light[a] - c.hi[a] : 0.0f);
            dist2 += v * v;
        }

        return dist2 >= light[3] * light[3];
    }

    std::uint64_t caster_signature(const Caster& c, const double* origin) const noexcept {
        std::uint64_t h = kHashSeed;
        mix_hash(h, static_cast<std::uint64_t>(c.kind));
        mix_hash(h, reinterpret_cast<std::uintptr_t>(c.vertices));
        mix_hash(h, reinterpret_cast<std::uintptr_t>(c.indices));
        mix_hash(h, c.generation);
        mix_hash(h, c.count);
        mix_hash(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(c.vertex_base)));
        mix_hash(h, c.faces);
        for (const graphics::QuadRange& r : c.groups) { mix_hash(h, r.first); mix_hash(h, r.count); }

        for (int r = 0; r < 3; ++r) {
            for (int k = 0; k < 3; ++k) mix_hash(h, float_key(c.model[r * 4 + k]));
            mix_hash(h, grid_key(static_cast<double>(c.model[r * 4 + 3]) + origin[r]));
        }

        if (c.kind == Caster::Kind::Instanced) {
            const std::size_t end = std::min<std::size_t>(m_instances3d.size(), static_cast<std::size_t>(c.instance_first) + c.instance_count);

            for (std::size_t i = c.instance_first; i < end; ++i) {
                std::uint32_t words[sizeof(graphics::Instance3D) / sizeof(std::uint32_t)];
                std::memcpy(words, &m_instances3d[i], sizeof(words));
                for (std::uint32_t w : words) mix_hash(h, w);
            }
        }

        return h;
    }

    std::uint64_t shadow_signature(std::uint64_t key, std::size_t first, std::size_t last, const float* m, const float* light, const double* origin) const noexcept {
        std::uint64_t sum = 0, count = 0;

        for (std::size_t i = first; i < last; ++i) {
            const Caster& c = m_casters[i];
            if (!c.vertices) continue;
            if (light ? outside_sphere(c, light) : outside_light(c, m, false, 0, nullptr)) continue;
            sum += caster_signature(c, origin);
            ++count;
        }

        std::uint64_t h = key;
        mix_hash(h, sum);
        mix_hash(h, count);
        return h;
    }

    static bool outside_light(const Caster& c, const float* m, bool point, int face, const float* light) noexcept {
        if (point) {
            if (outside_sphere(c, light)) return true;
            const int axis = face / 2;
            return (face % 2 == 0) ? c.hi[axis] <= light[axis] : c.lo[axis] >= light[axis];
        }

        float center[3], half[3];
        for (int a = 0; a < 3; ++a) { center[a] = 0.5f * (c.lo[a] + c.hi[a]); half[a] = 0.5f * (c.hi[a] - c.lo[a]); }

        for (int r = 0; r < 3; ++r) {
            const float* row = m + r * 4;
            const float mid = row[0] * center[0] + row[1] * center[1] + row[2] * center[2] + row[3];
            const float ext = std::fabs(row[0]) * half[0] + std::fabs(row[1]) * half[1] + std::fabs(row[2]) * half[2];
            if (mid - ext > 1.0f) return true;
            if (r < 2 && mid + ext < -1.0f) return true;
        }

        return false;
    }

    static graphics::FaceMask sun_faces(const float* dir) noexcept {
        graphics::FaceMask m = graphics::face_bit(graphics::CellFace::None);
        if (dir[0] > 0.0f) m |= graphics::face_bit(graphics::CellFace::NegX);
        if (dir[0] < 0.0f) m |= graphics::face_bit(graphics::CellFace::PosX);
        if (dir[1] > 0.0f) m |= graphics::face_bit(graphics::CellFace::NegY);
        if (dir[1] < 0.0f) m |= graphics::face_bit(graphics::CellFace::PosY);
        if (dir[2] > 0.0f) m |= graphics::face_bit(graphics::CellFace::NegZ);
        if (dir[2] < 0.0f) m |= graphics::face_bit(graphics::CellFace::PosZ);
        return m;
    }

    void record_shadow_view(
        const vulkan::CommandBuffer& cmd, 
        PerFrame& pf, 
        ShadowMap& map, 
        int face, 
        const float* light_vp,
        const float* light, 
        bool point, 
        std::size_t first, 
        std::size_t last
    ) {
        vulkan::DepthAttachment depth;
        depth.view  = &map.attachment(face);
        depth.load  = vulkan::LoadOp::Clear;
        depth.store = vulkan::StoreOp::Store;
        depth.clear = vulkan::ClearDepth{ 1.0f, 0 };
        vulkan::RenderingDesc rd;
        rd.area  = { { 0, 0 }, { map.size, map.size } };
        rd.depth = &depth;
        cmd.begin_rendering(rd);
        cmd.set_viewport({ 0.0f, 0.0f, static_cast<float>(map.size), static_cast<float>(map.size), 0.0f, 1.0f });
        cmd.set_scissor({ { 0, 0 }, { map.size, map.size } });
        const graphics::FaceMask toward_sun = point ? graphics::ALL_FACE_GROUPS : sun_faces(light);
        const vulkan::Pipeline* bound = nullptr;
        float push[32] = {};
        std::copy(light_vp, light_vp + 16, push);
        std::copy(light, light + 4, push + 28);

        for (std::size_t i = first; i < last; ++i) {
            const Caster& c = m_casters[i];
            if (!c.vertices || outside_light(c, light_vp, point, face, light)) continue;
            graphics::FaceMask faces = c.faces;

            if (c.kind == Caster::Kind::Quads && c.axis_aligned) {
                if (point) {
                    const vector3d eye{ light[0], light[1], light[2] };
                    faces &= graphics::facing_faces(eye, { c.lo[0], c.lo[1], c.lo[2] }, { c.hi[0], c.hi[1], c.hi[2] });
                } else {
                    faces &= toward_sun;
                }

                if (faces == 0) continue;
            }

            const vulkan::Pipeline* p = shadow_pipeline(c.kind, point);
            if (!p) continue;
            if (p != bound) { cmd.bind(*p); bound = p; }
            std::copy(c.model, c.model + 12, push + 16);
            cmd.push_constants(*p, vulkan::ShaderStage::Vertex | vulkan::ShaderStage::Fragment, push, sizeof(push));
            cmd.bind_vertex_buffer(0, *c.vertices);

            if (c.kind == Caster::Kind::Quads) {
                if (!m_quad_indices) continue;
                cmd.bind_index_buffer(*m_quad_indices, vulkan::IndexType::UInt32);
                std::uint32_t run_first = 0, run_count = 0;

                auto flush = [&]() {
                    if (run_count == 0) return;
                    cmd.draw_indexed(run_count * static_cast<std::uint32_t>(graphics::QuadMesh3D::INDICES_PER_QUAD), 1, 0,
                                     c.vertex_base + static_cast<std::int32_t>(run_first * graphics::QuadMesh3D::VERTICES_PER_QUAD));
                    run_count = 0;
                };

                for (std::size_t g = 0; g < graphics::CELL_FACE_GROUPS; ++g) {
                    const graphics::QuadRange r = c.groups[g];
                    if (r.count == 0) continue;
                    if (!(faces & (1u << g))) { flush(); continue; }
                    if (run_count == 0) run_first = r.first;
                    if (run_first + run_count != r.first) { flush(); run_first = r.first; }
                    run_count += r.count;
                }

                flush();
                continue;
            }

            const std::uint32_t instances = c.kind == Caster::Kind::Instanced ? c.instance_count : 1u;
            if (c.kind == Caster::Kind::Instanced) cmd.bind_vertex_buffer(1, pf.instances3d, static_cast<std::uint64_t>(c.instance_first) * sizeof(graphics::Instance3D));

            if (c.indices) {
                cmd.bind_index_buffer(*c.indices, vulkan::IndexType::UInt32);
                cmd.draw_indexed(c.count, instances);
            } else {
                cmd.draw(c.count, instances);
            }
        }

        cmd.end_rendering();
    }

    void issue_draw(const vulkan::CommandBuffer& cmd, PerFrame& pf, const Draw3D& d) {
        if (d.kind == DrawKind3D::Batch || d.kind == DrawKind3D::ArenaQuads) {
            if (d.page >= m_arena.size() || !m_quad_indices) return;
            cmd.bind_vertex_buffer(0, *m_arena[d.page].buffer);
            cmd.bind_index_buffer(*m_quad_indices, vulkan::IndexType::UInt32);

            if (d.kind == DrawKind3D::ArenaQuads) {
                cmd.draw_indexed(d.count, 1, 0, d.vertex_offset, 0);
                return;
            }

            cmd.bind_vertex_buffer(1, pf.batch_instances);

            if (m_multi_draw) {
                cmd.draw_indexed_indirect(pf.batch_commands, static_cast<std::uint64_t>(d.first) * sizeof(IndirectCommand), d.count, sizeof(IndirectCommand));
            } else {
                for (std::uint32_t c = d.first; c < d.first + d.count; ++c) {
                    const IndirectCommand& ic = m_batch_commands[c];
                    cmd.draw_indexed(ic.index_count, ic.instance_count, ic.first_index, ic.vertex_offset, ic.first_instance);
                }
            }

            return;
        }

        if (d.kind == DrawKind3D::Instanced) {
            if (!d.mesh || !d.mesh->vertices) return;
            cmd.bind_vertex_buffer(0, *d.mesh->vertices);
            cmd.bind_vertex_buffer(1, pf.instances3d, static_cast<std::uint64_t>(d.instance_first) * sizeof(graphics::Instance3D));
            if (d.mesh->indices) { cmd.bind_index_buffer(*d.mesh->indices, vulkan::IndexType::UInt32); cmd.draw_indexed(d.count, d.instance_count); }
            else cmd.draw(d.count, d.instance_count);
            return;
        }

        if (d.kind == DrawKind3D::Mesh) {
            if (!d.mesh || !d.mesh->vertices) return;
            cmd.bind_vertex_buffer(0, *d.mesh->vertices);
            if (d.quads) {
                if (!m_quad_indices) return;
                cmd.bind_index_buffer(*m_quad_indices, vulkan::IndexType::UInt32);
                cmd.draw_indexed(d.count);
            }
            else if (d.mesh->indices) { cmd.bind_index_buffer(*d.mesh->indices, vulkan::IndexType::UInt32); cmd.draw_indexed(d.count); }
            else cmd.draw(d.count);
            return;
        }

        cmd.bind_vertex_buffer(0, pf.vertices3d);
        cmd.draw(d.count, 1, d.first);
    }

    static std::uint32_t mirrored_state(std::uint32_t state) noexcept {
        const std::uint32_t cull = state & kCullMask;
        const std::uint32_t flipped = cull == kCullBack ? kCullFront : cull == kCullFront ? kCullBack : cull;
        return (state & ~kCullMask) | flipped;
    }

    static Mat4f oblique_clip(const Mat4f& m, const float* plane) noexcept {
        const Mat4f inv = mat4_inverse(m);
        const double c[4] = { plane[0], plane[1], plane[2], -(plane[3] + kReflectionClipBias) };
        double cc[4] = {};

        for (int j = 0; j < 4; ++j)
            for (int i = 0; i < 4; ++i) cc[j] += static_cast<double>(inv[static_cast<std::size_t>(i * 4 + j)]) * c[i];

        auto sign = [](double v) { return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0); };
        const double reach = cc[0] * sign(cc[0]) + cc[1] * sign(cc[1]) + cc[2] + cc[3];
        if (!(reach > 0.0) || !std::isfinite(reach)) return m;
        Mat4f out = m;
        for (int k = 0; k < 4; ++k) out[static_cast<std::size_t>(8 + k)] = static_cast<float>(c[k] / reach);
        return out;
    }

    Mat4f reflected_view_proj(std::size_t scene, std::uint32_t plane) const noexcept {
        return mat4_mul(m_scenes[scene].vp, reflection_matrix(m_scene_lights[scene].planes[plane]));
    }

    static Mat4f reflection_matrix(const float* plane) noexcept {
        Mat4f r = mat4_identity();
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) r[static_cast<std::size_t>(i * 4 + j)] -= 2.0f * plane[i] * plane[j];
            r[static_cast<std::size_t>(i * 4 + 3)] = 2.0f * plane[3] * plane[i];
        }
        return r;
    }

    ReflectionTarget* ensure_reflection_target(std::size_t k, unsigned int w, unsigned int h) {
        ReflectionTarget& t = m_reflections[k];
        if (t.color && t.width == w && t.height == h) return &t;
        if (t.color) m_retired.push_back({ m_frame_serial, std::move(t.color) });
        if (t.msaa) m_retired.push_back({ m_frame_serial, std::move(t.msaa) });
        if (t.depth) m_retired.push_back({ m_frame_serial, std::move(t.depth) });
        t.width = w;
        t.height = h;
        vulkan::ImageDesc cd;
        cd.extent = { w, h, 1 };
        cd.format = vulkan::Format::RGBA8Unorm;
        cd.usage  = vulkan::ImageUsage::ColorAttachment | vulkan::ImageUsage::Sampled;
        cd.name   = "fizmo reflection";
        t.color = std::make_unique<vulkan::Image>();
        if (failed(t.color->create(m_device, cd))) { t.color.reset(); return nullptr; }

        if (m_samples != vulkan::SampleCount::X1) {
            vulkan::ImageDesc md = cd;
            md.usage   = vulkan::ImageUsage::ColorAttachment;
            md.samples = m_samples;
            md.name    = "fizmo reflection (msaa)";
            t.msaa = std::make_unique<vulkan::Image>();
            if (failed(t.msaa->create(m_device, md))) { t.color.reset(); t.msaa.reset(); return nullptr; }
        }

        vulkan::ImageDesc dd;
        dd.extent  = { w, h, 1 };
        dd.format  = m_depth_format;
        dd.usage   = vulkan::ImageUsage::DepthAttachment;
        dd.samples = m_samples;
        dd.name    = "fizmo reflection depth";
        t.depth = std::make_unique<vulkan::Image>();
        if (failed(t.depth->create(m_device, dd))) { t.color.reset(); t.msaa.reset(); t.depth.reset(); return nullptr; }
        return &t;
    }

    void record_reflection_pass(
        const vulkan::CommandBuffer& cmd, 
        PerFrame& pf, 
        const SceneRec& sc, 
        std::size_t scene, 
        std::uint32_t plane,
        std::size_t from, 
        std::size_t to, 
        DescriptorCache& sets
    ) {
        SceneLightRec& sl = m_scene_lights[scene];
        ReflectionTarget& t = m_reflections[plane];
        if (!t.color || !t.depth) return;
        const float scale = sl.plane_scales[plane];
        const float sx = static_cast<float>(sc.scene.x) * scale, sy = static_cast<float>(sc.scene.y) * scale;
        const float sw = static_cast<float>(sc.scene.width) * scale, sh = static_cast<float>(sc.scene.height) * scale;
        const int* area = sl.plane_rects[plane];
        const int ax0 = std::max(static_cast<int>(std::floor(sx)), 0);
        const int ay0 = std::max(static_cast<int>(std::floor(sy)), 0);
        const int ax1 = std::min(static_cast<int>(std::ceil(sx + sw)), static_cast<int>(t.width));
        const int ay1 = std::min(static_cast<int>(std::ceil(sy + sh)), static_cast<int>(t.height));
        const int x0 = std::max(static_cast<int>(std::floor(area[0] * scale)), ax0);
        const int y0 = std::max(static_cast<int>(std::floor(area[1] * scale)), ay0);
        const int x1 = std::min(static_cast<int>(std::ceil(area[2] * scale)), ax1);
        const int y1 = std::min(static_cast<int>(std::ceil(area[3] * scale)), ay1);
        if (x1 <= x0 || y1 <= y0) return;
        cmd.begin_label("fizmo planar reflection");
        if (t.msaa) cmd.transition(*t.msaa, vulkan::ImageLayout::ColorAttachment);
        cmd.transition(*t.color, vulkan::ImageLayout::ColorAttachment);
        cmd.transition(*t.depth, vulkan::ImageLayout::DepthAttachment);
        vulkan::ColorAttachment color;
        color.view    = t.msaa ? &t.msaa->view() : &t.color->view();
        color.resolve = t.msaa ? &t.color->view() : nullptr;
        color.load    = vulkan::LoadOp::Clear;
        color.store   = vulkan::StoreOp::Store;
        color.clear   = m_clear_color;
        vulkan::DepthAttachment depth;
        depth.view        = &t.depth->view();
        depth.load        = vulkan::LoadOp::Clear;
        depth.store       = vulkan::StoreOp::DontCare;
        depth.clear       = vulkan::ClearDepth{ 1.0f, 0 };
        depth.has_stencil = vulkan::has_stencil(m_depth_format);
        vulkan::RenderingDesc rd;
        rd.area   = { { ax0, ay0 }, { static_cast<std::uint32_t>(ax1 - ax0), static_cast<std::uint32_t>(ay1 - ay0) } };
        rd.colors = color;
        rd.depth  = &depth;
        cmd.begin_rendering(rd);
        cmd.set_viewport({ sx, sy, sw, sh, 0.0f, 1.0f });
        cmd.set_scissor({ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } });
        const vulkan::DescriptorSet lighting = scene_descriptor(pf, scene, false, static_cast<int>(plane));

        if (lighting.valid()) {
            const Mat4f vpr = oblique_clip(reflected_view_proj(scene, plane), sl.planes[plane]);
            const vulkan::Pipeline* bound = nullptr;

            if (sl.sky) {
                if (const vulkan::Pipeline* sky = sky_pipeline()) {
                    cmd.bind(*sky);
                    bound = sky;
                    cmd.bind_descriptor_set(*sky, 1, lighting);
                    cmd.draw(kSkyVertices);
                }
            }

            bool lighting_bound = bound != nullptr;
            float push[32];

            for (std::size_t i = from; i < to; ++i) {
                const Draw3D& d = m_draws3d[i];
                if (d.count == 0 || !d.in_reflections || d.kind == DrawKind3D::Lines) continue;
                const vulkan::Pipeline* p = pipeline_3d(mirrored_state(d.state));
                if (!p) continue;
                const vulkan::DescriptorSet set = descriptor_for(pf, d.view, d.sampler, sets);
                if (!set.valid()) continue;
                if (p != bound) { cmd.bind(*p); bound = p; }
                if (!lighting_bound) { cmd.bind_descriptor_set(*p, 1, lighting); lighting_bound = true; }
                cmd.bind_descriptor_set(*p, 0, set);
                std::copy(d.push, d.push + 32, push);

                if (d.kind == DrawKind3D::Batch) {
                    std::copy(vpr.begin(), vpr.end(), push);
                } else {
                    Mat4f model = mat4_identity();
                    std::copy(d.push + 16, d.push + 28, model.begin());
                    const Mat4f mvp = mat4_mul(vpr, model);
                    std::copy(mvp.begin(), mvp.end(), push);
                }

                cmd.push_constants(*p, vulkan::ShaderStage::Vertex, push, 128);
                issue_draw(cmd, pf, d);
            }
        }

        cmd.end_rendering();
        cmd.transition(*t.color, vulkan::ImageLayout::ShaderReadOnly);
        cmd.end_label();
    }

    void record_3d_pass(const vulkan::CommandBuffer& cmd, PerFrame& pf, const SceneRec& sc, std::size_t scene, std::size_t from, std::size_t to, bool clear, DescriptorCache& sets) {
        const int x0 = std::max(sc.scene.x, 0), y0 = std::max(sc.scene.y, 0);
        const int x1 = std::min(sc.scene.x + static_cast<int>(sc.scene.width),  static_cast<int>(m_width));
        const int y1 = std::min(sc.scene.y + static_cast<int>(sc.scene.height), static_cast<int>(m_height));
        if ((x1 <= x0 || y1 <= y0 || from >= to) && !clear && !m_scene_lights[scene].sky) return;
        const SceneLightRec& sl = m_scene_lights[scene];
        const bool splits = sl.copy && sl.copy_at >= from && sl.copy_at <= to && x1 > x0 && y1 > y0;
        const vulkan::Rect2D whole{ { 0, 0 }, { m_width, m_height } };
        const vulkan::Rect2D drawn = x1 > x0 && y1 > y0 ? vulkan::Rect2D{ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } } : whole;
        cmd.transition(m_depth, vulkan::ImageLayout::DepthAttachment);
        if (splits && m_msaa_target.valid()) prepare_depth_resolve(cmd);

        auto open = [&](bool first) {
            const vulkan::ColorAttachment color = color_attachment(first && clear);
            vulkan::DepthAttachment depth;
            depth.view        = &m_depth.view();
            depth.load        = first ? vulkan::LoadOp::Clear : vulkan::LoadOp::Load;
            depth.store       = first && splits ? vulkan::StoreOp::Store : vulkan::StoreOp::DontCare;
            depth.clear       = vulkan::ClearDepth{ 1.0f, 0 };
            depth.has_stencil = vulkan::has_stencil(m_depth_format);
            depth.resolve     = first && splits && m_msaa_target.valid() ? &m_scene_depth.view() : nullptr;
            vulkan::RenderingDesc rd;
            rd.area   = first && clear ? whole : drawn;
            rd.colors = color;
            rd.depth  = &depth;
            cmd.begin_rendering(rd);
            if (x1 <= x0 || y1 <= y0) return;
            cmd.set_viewport({ static_cast<float>(sc.scene.x), static_cast<float>(sc.scene.y),
                               static_cast<float>(sc.scene.width), static_cast<float>(sc.scene.height), 0.0f, 1.0f });
            cmd.set_scissor({ { x0, y0 }, { static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0) } });
        };

        open(true);

        if (x1 > x0 && y1 > y0) {
            const vulkan::Pipeline* bound = nullptr;
            vulkan::DescriptorSet lighting;
            bool lighting_bound = false;

            if (m_scene_lights[scene].sky) {
                const vulkan::Pipeline* sky = sky_pipeline();
                if (sky) lighting = scene_descriptor(pf, scene);

                if (sky && lighting.valid()) {
                    cmd.bind(*sky);
                    bound = sky;
                    cmd.bind_descriptor_set(*sky, 1, lighting);
                    lighting_bound = true;
                    cmd.draw(kSkyVertices);
                }
            }

            for (std::size_t i = from; i < to; ++i) {
                const Draw3D& d = m_draws3d[i];

                if (splits && i == sl.copy_at) {
                    cmd.end_rendering();
                    copy_scene(cmd, drawn);
                    open(false);
                    bound = nullptr;
                    lighting = scene_descriptor(pf, scene, true);
                    if (lighting.valid()) lighting_bound = false;
                }

                if (d.count == 0 || !d.on_camera) continue;
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

                if (!lighting.valid()) {
                    lighting = scene_descriptor(pf, scene);
                    if (!lighting.valid()) continue;
                    lighting_bound = false;
                }

                if (!lighting_bound) {
                    cmd.bind_descriptor_set(*p, 1, lighting);
                    lighting_bound = true;
                }

                cmd.bind_descriptor_set(*p, 0, set);
                cmd.push_constants(*p, vulkan::ShaderStage::Vertex, d.push, 128);
                issue_draw(cmd, pf, d);
            }

            if (splits && sl.copy_at == to) {
                cmd.end_rendering();
                copy_scene(cmd, drawn);
                open(false);
                lighting = scene_descriptor(pf, scene, true);
            }

            const vulkan::Pipeline* rays = splits && sl.wants_rays ? rays_pipeline() : nullptr;

            if (rays && lighting.valid()) {
                cmd.bind(*rays);
                cmd.bind_descriptor_set(*rays, 1, lighting);
                cmd.draw(kSkyVertices);
            }
        }

        cmd.end_rendering();
    }

    void record_present(
        const vulkan::CommandBuffer& cmd, 
        const vulkan::Frame& f, 
        const vulkan::DescriptorSet& present_set
    ) {
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

    void record_uploads(const vulkan::CommandBuffer& cmd, PerFrame& pf) {
        for (const Upload& u : m_uploads) {
            cmd.transition(*u.image, vulkan::ImageLayout::TransferDst);
            cmd.copy_buffer_to_image_region(pf.staging, *u.image, u.offset, u.at, u.size);
        }

        for (const Upload& u : m_uploads) cmd.transition(*u.image, vulkan::ImageLayout::ShaderReadOnly);
        
        if (!m_buffer_uploads.empty()) {
            cmd.memory_barrier(
                vulkan::PipelineStage::VertexInput, vulkan::Access::VertexRead | vulkan::Access::IndexRead,
                vulkan::PipelineStage::Transfer, vulkan::Access::TransferWrite
            );
        }

        for (const BufferUpload& u : m_buffer_uploads) cmd.copy_buffer(pf.staging, *u.dst, u.bytes, u.offset, u.dst_offset);

        if (!m_buffer_uploads.empty()) {
            cmd.memory_barrier(vulkan::PipelineStage::Transfer, vulkan::Access::TransferWrite,
                               vulkan::PipelineStage::VertexInput, vulkan::Access::VertexRead | vulkan::Access::IndexRead);
        }
    }

    void collect_timings(PerFrame& pf) {
        const std::uint32_t used = pf.stamps_used;
        pf.stamps_used = 0;
        if (used == 0 || !pf.timestamps.valid()) return;
        m_stamp_values.resize(used);
        if (!pf.timestamps.read(0, used, m_stamp_values.data())) return;
        GpuTimings t;
        t.valid = true;

        for (const auto& span : pf.spans) {
            const std::uint64_t a = m_stamp_values[span.second], b = m_stamp_values[span.second + 1];
            if (b > a) t.pass_ms[static_cast<std::size_t>(span.first)] += pf.timestamps.ticks_to_ms(b - a);
        }

        if (m_stamp_values[used - 1] > m_stamp_values[0]) t.total_ms = pf.timestamps.ticks_to_ms(m_stamp_values[used - 1] - m_stamp_values[0]);
        m_timings = t;
    }

    void start_timings(const vulkan::CommandBuffer& cmd, PerFrame& pf) {
        pf.spans.clear();
        pf.stamps_used = 0;
        if (!m_gpu_timing) return;
        if (!pf.timestamps.valid() && failed(pf.timestamps.create(m_device, kMaxTimestamps, "fizmo gpu timings"))) return;
        cmd.reset_timestamps(pf.timestamps, 0, kMaxTimestamps);
    }

    template <typename Body>
    void timed(const vulkan::CommandBuffer& cmd, PerFrame& pf, GpuPass pass, Body&& body) {
        if (!m_gpu_timing || !pf.timestamps.valid() || pf.stamps_used + 2 > kMaxTimestamps) { body(); return; }
        const std::uint32_t first = pf.stamps_used;
        pf.stamps_used += 2;
        cmd.write_timestamp(pf.timestamps, first);
        body();
        cmd.write_timestamp(pf.timestamps, first + 1);
        pf.spans.emplace_back(pass, first);
    }

    void record_frame(vulkan::Frame& f) {
        collect_garbage();
        PerFrame& pf = m_per_frame[f.frame_index];
        pf.pool.reset();
        pf.scene_sets.clear();
        collect_timings(pf);
        const vulkan::CommandBuffer& cmd = f.cmd;
        start_timings(cmd, pf);

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

        if (!m_batch_instances.empty()) {
            const std::uint64_t bytes = m_batch_instances.size() * sizeof(BatchInstance);
            if (!ensure_buffer(m_device, pf.batch_instances, bytes, vulkan::BufferUsage::Vertex, "fizmo batch instances")) return;
            pf.batch_instances.write(m_batch_instances.data(), bytes);
        }

        if (!m_batch_commands.empty() && m_multi_draw) {
            const std::uint64_t bytes = m_batch_commands.size() * sizeof(IndirectCommand);
            if (!ensure_buffer(m_device, pf.batch_commands, bytes, vulkan::BufferUsage::Indirect, "fizmo batch commands")) return;
            pf.batch_commands.write(m_batch_commands.data(), bytes);
        }

        if (!m_instances3d.empty()) {
            const std::uint64_t bytes = m_instances3d.size() * sizeof(graphics::Instance3D);
            if (!ensure_buffer(m_device, pf.instances3d, bytes, vulkan::BufferUsage::Vertex, "fizmo 3d instances")) return;
            pf.instances3d.write(m_instances3d.data(), bytes);
        }

        if (!m_lines3d.empty()) {
            const std::uint64_t bytes = m_lines3d.size() * sizeof(gpu::LineVertex3D);
            if (!ensure_buffer(m_device, pf.lines3d, bytes, vulkan::BufferUsage::Vertex, "fizmo 3d lines")) return;
            pf.lines3d.write(m_lines3d.data(), bytes);
        }

        if (!m_scene_lights.empty()) {
            prepare_shadow_maps();
            plan_scene_copies();
            prepare_reflections();
            prepare_volumes();
            const std::size_t scenes = m_scene_lights.size();
            m_uniform_bytes.assign(static_cast<std::size_t>(m_uniform_stride * scenes * (1 + kMaxPlanes)), 0);

            for (std::size_t i = 0; i < scenes; ++i) {
                std::memcpy(m_uniform_bytes.data() + i * m_uniform_stride, &m_scene_lights[i].uniforms, sizeof(SceneUniforms));

                for (std::uint32_t k = 0; k < m_scene_lights[i].plane_count; ++k) {
                    const SceneUniforms u = reflection_uniforms(i, k);
                    std::memcpy(m_uniform_bytes.data() + reflection_slot(i, static_cast<int>(k)) * m_uniform_stride, &u, sizeof(SceneUniforms));
                }
            }

            if (!ensure_buffer(m_device, pf.scene_uniforms, m_uniform_bytes.size(), vulkan::BufferUsage::Uniform, "fizmo scene lighting")) return;
            pf.scene_uniforms.write(m_uniform_bytes.data(), m_uniform_bytes.size());
        }

        cmd.begin_label("fizmo uploads");
        timed(cmd, pf, GpuPass::Uploads, [&]() { record_uploads(cmd, pf); });
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
                        vulkan::Access::DepthAttachmentRead | vulkan::Access::DepthAttachmentWrite
                    );
                }

                had_pass = true;
            };

            for (std::size_t si = 0; si <= m_scenes.size(); ++si) {
                const bool last = si == m_scenes.size();
                const std::size_t upto = last ? m_batches.size() : std::min(m_scenes[si].batch_index, m_batches.size());

                if (upto > next_batch || (last && clear_next)) {
                    between_passes();
                    timed(cmd, pf, GpuPass::Overlay, [&]() { record_2d_pass(cmd, pf, next_batch, upto, clear_next, sets); });
                    clear_next = false;
                }

                next_batch = std::max(next_batch, upto);
                if (last) break;
                const std::size_t draw_end = si + 1 < m_scenes.size() ? m_scenes[si + 1].first_draw : m_draws3d.size();
                const std::size_t caster_end = si + 1 < m_scenes.size() ? m_scenes[si + 1].first_caster : m_casters.size();
                timed(cmd, pf, GpuPass::Shadows, [&]() { record_shadow_passes(cmd, pf, si, m_scenes[si].first_caster, caster_end); });
                timed(cmd, pf, GpuPass::Volume, [&]() { record_volume_pass(cmd, pf, si); });

                timed(cmd, pf, GpuPass::Reflections, [&]() {
                    for (std::uint32_t k = 0; k < m_scene_lights[si].plane_count; ++k) record_reflection_pass(cmd, pf, m_scenes[si], si, k, m_scenes[si].first_draw, draw_end, sets);
                });

                between_passes();
                timed(cmd, pf, GpuPass::Scene, [&]() { record_3d_pass(cmd, pf, m_scenes[si], si, m_scenes[si].first_draw, draw_end, clear_next, sets); });
                timed(cmd, pf, GpuPass::Upscale, [&]() { record_upscale(cmd, pf, m_scenes[si]); });
                clear_next = false;
            }

            cmd.end_label();
            m_target_fresh = false;
        }

        cmd.transition(m_target, vulkan::ImageLayout::ShaderReadOnly);
        cmd.begin_label("fizmo present");
        vulkan::DescriptorSet present_set;

        if (
            succeeded(pf.pool.allocate(m_present_set_layout, present_set))
        ) timed(
            cmd, pf, GpuPass::Present, 
            [&]() { record_present(cmd, f, present_set); }
        );
        
        cmd.end_label();
    }
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_WINDOWS || OS_LINUX
#endif // FIZMO_RENDERER_GPU_IMPL_HPP