#include "fizmo_library.hpp"
#include "gpu_path_tracer.hpp"
#include "gpu_path_tracer_shader.hpp"
#include "Renderer/renderer.hpp"
#include "../Lighting/scene_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace fizmo {
namespace graphics {

namespace {

struct alignas(16) Params {
    float cam_pos[4];
    float cam_forward[4];
    float cam_right[4];
    float cam_up[4];
    std::uint32_t size[4];
    std::uint32_t counts[4];
    std::uint32_t opts[4];
    std::uint32_t opts2[4];
    std::uint32_t offsets[4];
    std::uint32_t offsets2[4];
    float scene[4];
    float photon[4];
    float fog0[4];
    float fog1[4];
    float tone[4];
    float balance[4];
    float misc[4];
    std::uint32_t inst[4];
    float prev_pos[4];
    float prev_forward[4];
    float prev_right[4];
    float prev_up[4];
    std::uint32_t rt[4];
    float filter_strength[4];
};

static_assert(sizeof(Params) == 384, "Params must match the std140 block in path_tracer.comp");

struct Push {
    std::uint32_t mode = 0;
    std::uint32_t frame = 0;
    std::uint32_t first_sample = 0;
    std::uint32_t seed = 0;
};

enum Flag : std::uint32_t {
    f_area = 1u,
    f_emissive = 2u,
    f_shadows = 4u,
    f_spectral = 8u,
    f_dispersion = 16u,
    f_caustics = 32u,
    f_fog = 64u,
    f_env_visible = 128u,
    f_env_lights = 256u,
    f_env_sample = 512u,
    f_srgb = 1024u,
    f_flip = 2048u,
    f_casters_all = 4096u,
    f_casters_sun = 8192u
};

constexpr std::uint32_t kCellCount = 65536u;
constexpr std::uint32_t kCellSlots = 16u;
constexpr unsigned kEnvWidth = 256u;
constexpr unsigned kEnvHeight = 128u;
constexpr int kSpectralSteps = 128;

std::uint32_t bits(float f) noexcept {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

float as_float(std::uint32_t u) noexcept {
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

constexpr std::uint32_t kNoUv = 0xFFFFFFFFu;
constexpr unsigned kMaxMapSize = 2048u;
constexpr std::uint32_t kMatBaseMap = 4u;
constexpr std::uint32_t kMatWrapShift = 3u;
constexpr std::uint32_t kMatNearest = 32u;
constexpr std::uint32_t kMatSrgb = 64u;
constexpr std::uint32_t kMatAlpha = 128u;

struct PackedMap {
    std::uint32_t offset = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool alpha = false;
};

PackedMap pack_map(const lighting::Texture& tex, std::vector<float>& out) {
    PackedMap p;
    unsigned step = 1;
    while (tex.width() / step > kMaxMapSize || tex.height() / step > kMaxMapSize) step *= 2;
    p.width = std::max(1u, tex.width() / step);
    p.height = std::max(1u, tex.height() / step);
    p.alpha = tex.has_alpha();
    p.offset = static_cast<std::uint32_t>(out.size());
    const std::size_t count = static_cast<std::size_t>(p.width) * p.height;
    out.resize(out.size() + count * (p.alpha ? 2u : 1u));
    auto byte = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    for (std::uint32_t y = 0; y < p.height; ++y) {
        for (std::uint32_t x = 0; x < p.width; ++x) {
            lighting::Rgba sum(0.0f, 0.0f, 0.0f, 0.0f);
            for (unsigned sy = 0; sy < step; ++sy)
                for (unsigned sx = 0; sx < step; ++sx) sum = sum + tex.stored(static_cast<int>(x * step + sx), static_cast<int>(y * step + sy));
            sum = sum * (1.0f / static_cast<float>(step * step));
            const std::size_t i = static_cast<std::size_t>(y) * p.width + x;
            out[p.offset + i] = static_cast<float>(byte(sum.r) | (byte(sum.g) << 8) | (byte(sum.b) << 16));
            if (p.alpha) out[p.offset + count + i] = static_cast<float>(byte(sum.a));
        }
    }
    return p;
}

void map_matrix(const lighting::Material& mat, float out[6]) {
    float a[6], b[6];
    mat.uv.matrix(a);
    mat.base_color_map.transform.matrix(b);
    out[0] = b[0] * a[0] + b[1] * a[3];
    out[1] = b[0] * a[1] + b[1] * a[4];
    out[2] = b[0] * a[2] + b[1] * a[5] + b[2];
    out[3] = b[3] * a[0] + b[4] * a[3];
    out[4] = b[3] * a[1] + b[4] * a[4];
    out[5] = b[3] * a[2] + b[4] * a[5] + b[5];
}

float average_channel(const lighting::TextureMap& map) noexcept {
    return map.valid() ? map.texture->average()[std::clamp(map.channel, 0, 3)] : 1.0f;
}

bool gpu_base_map(const lighting::Material& mat) noexcept {
    return mat.base_color_map.valid() && !mat.base_color_map.triplanar;
}

void put(std::vector<float>& v, const lighting::Vec3f& a, float w) {
    v.push_back(a.x);
    v.push_back(a.y);
    v.push_back(a.z);
    v.push_back(w);
}

void put(std::vector<float>& v, const lighting::Rgb& a, float w) {
    v.push_back(a.r);
    v.push_back(a.g);
    v.push_back(a.b);
    v.push_back(w);
}

void put(std::vector<float>& v, float a, float b, float c, float d) {
    v.push_back(a);
    v.push_back(b);
    v.push_back(c);
    v.push_back(d);
}

void set4(float* dst, float a, float b, float c, float d) {
    dst[0] = a;
    dst[1] = b;
    dst[2] = c;
    dst[3] = d;
}

void set4(std::uint32_t* dst, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    dst[0] = a;
    dst[1] = b;
    dst[2] = c;
    dst[3] = d;
}

std::uint32_t mix_seed(std::uint64_t a, std::uint64_t b) noexcept {
    std::uint64_t x = a * 0x9E3779B97F4A7C15ULL + b + 0x632BE59BD9B4E019ULL;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return static_cast<std::uint32_t>(x ^ (x >> 32));
}

} // namespace

struct GpuPathTracer::State {
    Renderer* renderer = nullptr;
    std::uint64_t pass_id = 0;
    bool has_pass = false;
    std::string error;
    bool failed = false;
    bool supported = true;

    lighting::LightingSettings settings = lighting::LightingSettings::path_traced();
    lighting::Camera camera;
    lighting::Scene* scene = nullptr;
    std::uint64_t geometry_version = ~0ULL;
    std::uint64_t lighting_version = ~0ULL;
    bool scene_dirty = true;
    bool params_dirty = true;
    bool output_dirty = true;

    unsigned width = 0;
    unsigned height = 0;
    RenderTarget target;
    const gpu::Texture* target_texture = nullptr;

    std::uint32_t samples = 0;
    std::uint32_t frames = 0;
    std::uint32_t photon_frames = 0;
    std::uint32_t photons_per_frame = 0;
    std::uint32_t max_samples = 0;
    int samples_per_frame = 1;
    bool paused = false;
    bool flip = false;
    bool temporal = true;
    bool have_history = false;
    bool moved = false;
    std::uint32_t parity = 0;
    std::uint64_t total_frames = 0;
    std::uint32_t history_cap = 24;
    float prev_cam[16] = {};
    bool have_prev_cam = false;

    gpu::Device* device = nullptr;
    gpu::ShaderModule shader;
    gpu::BindGroupLayout layout;
    gpu::PipelineLayout pipeline_layout;
    gpu::ComputePipeline pipeline;
    gpu::Buffer params;
    gpu::Buffer tris;
    gpu::Buffer nodes;
    gpu::Buffer mats;
    gpu::Buffer lights;
    gpu::Buffer tables;
    gpu::Buffer accum;
    gpu::Buffer cells;
    gpu::Buffer photons;
    gpu::Buffer guide;
    gpu::Buffer filter;
    gpu::Texture output;
    gpu::BindGroup group;
    bool group_dirty = true;

    Params p{};
    std::size_t tri_count = 0;
    std::size_t light_count = 0;
    std::uint32_t emissive_offset = 0;
    std::uint32_t light_cdf_offset = 0;
    std::uint32_t env_rad_offset = 0;
    std::uint32_t env_marg_offset = 0;
    std::uint32_t env_cond_offset = 0;
    std::uint32_t spectral_offset = 0;
    float env_total = 0.0f;
    float total_power = 0.0f;
    float emissive_power = 0.0f;
    lighting::Vec3f center;
    float radius = 1.0f;
    bool env_visible = true;
    bool env_lights = true;
    bool env_any = false;

    struct BlasSlot {
        std::shared_ptr<const lighting::detail::Blas> blas;
        std::uint32_t tri_base = 0;
        std::uint32_t node_base = 0;
    };
    bool uploaded = false;
    std::uint64_t up_flat = 0;
    std::uint64_t up_blas = 0;
    std::uint64_t up_tlas = 0;
    std::uint64_t up_light = 0;
    std::vector<float> tri_data;
    std::vector<float> node_data;
    std::size_t geo_node_floats = 0;
    std::vector<BlasSlot> blas_slots;
    std::uint64_t partial_updates = 0;
    std::uint64_t full_uploads = 0;

    void fail(const std::string& message) {
        failed = true;
        error = message;
    }

    bool caustics() const noexcept {
        return settings.caustics && settings.quality >= lighting::LightingQuality::path_traced;
    }

    std::uint32_t photon_budget() const noexcept {
        if (!caustics()) return 0;
        if (photons_per_frame > 0) return std::min<std::uint32_t>(photons_per_frame, 1u << 22);
        return static_cast<std::uint32_t>(std::min<std::size_t>(settings.photons, 1u << 18));
    }

    bool storage(gpu::Buffer& b, std::uint64_t bytes, const char* name, gpu::BufferUsage extra = gpu::BufferUsage::None) {
        bytes = std::max<std::uint64_t>(bytes, 16);
        if (b.valid() && b.size() >= bytes) return true;
        b.destroy();
        gpu::BufferDesc d;
        d.size = bytes;
        d.usage = gpu::BufferUsage::Storage | gpu::BufferUsage::CopyDst | gpu::BufferUsage::CopySrc | extra;
        d.memory = gpu::MemoryAccess::GpuOnly;
        d.name = name;
        if (gpu::failed(b.create(*device, d))) {
            fail(std::string("could not create ") + name);
            return false;
        }
        group_dirty = true;
        return true;
    }

    template <typename T>
    bool upload(gpu::Buffer& b, const std::vector<T>& data, const char* name) {
        const std::uint64_t bytes = static_cast<std::uint64_t>(data.size() * sizeof(T));
        if (!storage(b, bytes, name)) return false;
        if (bytes == 0) return true;
        if (gpu::failed(b.write(data.data(), bytes, 0))) {
            fail(std::string("could not upload ") + name);
            return false;
        }
        return true;
    }

    bool create_pipeline() {
        const gpu::Caps& caps = device->caps();
        if (!caps.compute || !caps.storage_buffers) {
            supported = false;
            fail("this GPU backend has no compute shaders or storage buffers");
            return false;
        }
        if (gpu::failed(shader.create(*device, path_tracer_shader::path_tracer_comp()))) {
            fail("could not compile the path tracer shader: " + device->last_error());
            return false;
        }
        gpu::BindGroupLayoutEntry entries[12];
        for (std::uint32_t i = 0; i < 12; ++i) {
            entries[i].binding = i;
            entries[i].stages = gpu::ShaderStage::Compute;
            entries[i].type = gpu::BindingType::StorageBuffer;
            entries[i].access = gpu::StorageAccess::ReadOnly;
        }
        entries[0].type = gpu::BindingType::UniformBuffer;
        entries[6].access = gpu::StorageAccess::ReadWrite;
        entries[7].access = gpu::StorageAccess::ReadWrite;
        entries[8].access = gpu::StorageAccess::ReadWrite;
        entries[9].type = gpu::BindingType::StorageTexture;
        entries[9].storage_format = gpu::Format::RGBA8Unorm;
        entries[9].access = gpu::StorageAccess::WriteOnly;
        entries[10].access = gpu::StorageAccess::ReadWrite;
        entries[11].access = gpu::StorageAccess::ReadWrite;
        gpu::BindGroupLayoutDesc ld;
        ld.entries = { entries, 12 };
        ld.name = "fizmo path tracer";
        if (gpu::failed(layout.create(*device, ld))) {
            fail("could not create the path tracer bind layout");
            return false;
        }
        gpu::PipelineLayoutDesc pd;
        pd.groups = { &layout };
        pd.push_constant_size = sizeof(Push);
        pd.push_constant_stages = gpu::ShaderStage::Compute;
        pd.name = "fizmo path tracer";
        if (gpu::failed(pipeline_layout.create(*device, pd))) {
            fail("could not create the path tracer pipeline layout");
            return false;
        }
        gpu::ComputePipelineDesc cd;
        cd.shader = &shader;
        cd.layout = &pipeline_layout;
        cd.name = "fizmo path tracer";
        if (gpu::failed(pipeline.create(*device, cd))) {
            fail("could not create the path tracer pipeline: " + device->last_error());
            return false;
        }
        gpu::BufferDesc ud;
        ud.size = sizeof(Params);
        ud.usage = gpu::BufferUsage::Uniform | gpu::BufferUsage::CopyDst;
        ud.memory = gpu::MemoryAccess::GpuOnly;
        ud.name = "fizmo path tracer params";
        if (gpu::failed(params.create(*device, ud))) {
            fail("could not create the path tracer parameters");
            return false;
        }
        if (!storage(cells, static_cast<std::uint64_t>(kCellCount) * 4u, "fizmo photon cells")) return false;
        if (!storage(photons, static_cast<std::uint64_t>(kCellCount) * kCellSlots * 3u * 16u, "fizmo photons")) return false;
        return true;
    }

    bool ensure_output() {
        if (width == 0 || height == 0) return false;
        if (!output.valid() || output.width() != width || output.height() != height) {
            output.destroy();
            gpu::TextureDesc td;
            td.extent = { width, height, 1 };
            td.format = gpu::Format::RGBA8Unorm;
            td.usage = gpu::TextureUsage::Storage | gpu::TextureUsage::CopySrc | gpu::TextureUsage::Sampled;
            td.name = "fizmo path tracer output";
            if (gpu::failed(output.create(*device, td))) {
                fail("could not create the path tracer output image");
                return false;
            }
            group_dirty = true;
        }
        const std::uint64_t bytes = static_cast<std::uint64_t>(width) * height * 16u;
        if (!accum.valid() || accum.size() < bytes * 2u) {
            accum.destroy();
            if (!storage(accum, bytes * 2u, "fizmo path tracer accumulation")) return false;
            samples = 0;
            have_history = false;
        }
        if (!guide.valid() || guide.size() < bytes * 6u) {
            guide.destroy();
            if (!storage(guide, bytes * 6u, "fizmo path tracer guide")) return false;
            have_history = false;
        }
        if (!filter.valid() || filter.size() < bytes * 2u) {
            filter.destroy();
            if (!storage(filter, bytes * 2u, "fizmo path tracer denoiser")) return false;
        }
        return true;
    }

    static void pack_tri(std::vector<float>& t, std::size_t at, const lighting::detail::Tri& tri, std::uint32_t material, std::uint32_t flags, std::uint32_t channels, std::int32_t light, float uv_slot, float color_slot) {
        if (t.size() < at + 24) t.resize(at + 24);
        float* d = t.data() + at;
        std::uint32_t lu;
        std::memcpy(&lu, &light, sizeof(lu));
        const lighting::Vec3f* v[6] = { &tri.v0, &tri.e1, &tri.e2, &tri.n0, &tri.n1, &tri.n2 };
        const float w[6] = { as_float(material), as_float(flags), as_float(channels), as_float(lu), uv_slot, color_slot };
        for (int k = 0; k < 6; ++k) {
            d[k * 4 + 0] = v[k]->x;
            d[k * 4 + 1] = v[k]->y;
            d[k * 4 + 2] = v[k]->z;
            d[k * 4 + 3] = w[k];
        }
    }

    static void pack_node(std::vector<float>& n, std::size_t at, const lighting::detail::BvhNode& node, std::uint32_t first) {
        if (n.size() < at + 8) n.resize(at + 8);
        float* d = n.data() + at;
        d[0] = node.lo.x;
        d[1] = node.lo.y;
        d[2] = node.lo.z;
        d[3] = as_float(first);
        d[4] = node.hi.x;
        d[5] = node.hi.y;
        d[6] = node.hi.z;
        d[7] = as_float(node.count);
    }

    bool write_region(gpu::Buffer& b, const std::vector<float>& data, std::size_t first_float, std::size_t count, const char* name) {
        if (count == 0) return true;
        const std::uint64_t offset = static_cast<std::uint64_t>(first_float) * sizeof(float);
        const std::uint64_t bytes = static_cast<std::uint64_t>(count) * sizeof(float);
        if (!b.valid() || b.size() < offset + bytes) return false;
        if (gpu::failed(b.write(data.data() + first_float, bytes, offset))) {
            fail(std::string("could not update ") + name);
            return false;
        }
        return true;
    }

    bool upload_slack(gpu::Buffer& b, const std::vector<float>& data, std::size_t slack_floats, const char* name) {
        const std::uint64_t bytes = static_cast<std::uint64_t>(data.size()) * sizeof(float);
        if (!b.valid() || b.size() < bytes) {
            b.destroy();
            if (!storage(b, bytes + static_cast<std::uint64_t>(slack_floats) * sizeof(float), name)) return false;
        }
        if (bytes == 0) return true;
        if (gpu::failed(b.write(data.data(), bytes, 0))) {
            fail(std::string("could not upload ") + name);
            return false;
        }
        return true;
    }

    void pack_tlas(const lighting::detail::Compiled& c) {
        node_data.resize(geo_node_floats);
        const auto& tn = c.tlas_bvh.nodes();
        if (c.tlas.empty() || tn.empty()) {
            set4(p.inst, 0u, 0u, 0u, 0u);
            return;
        }
        const std::uint32_t root = static_cast<std::uint32_t>(geo_node_floats / 8);
        for (const auto& node : tn) pack_node(node_data, node_data.size(), node, node.count > 0 ? node.first : node.first + root);
        const std::uint32_t inst_base = static_cast<std::uint32_t>(node_data.size() / 4);
        for (const auto& ti : c.tlas) {
            const std::uint32_t blas_root = ti.mesh < blas_slots.size() ? blas_slots[ti.mesh].node_base : 0u;
            for (int r = 0; r < 3; ++r) put(node_data, ti.to_object.m[r * 4 + 0], ti.to_object.m[r * 4 + 1], ti.to_object.m[r * 4 + 2], ti.to_object.m[r * 4 + 3]);
            const bool live = ti.mesh < blas_slots.size() && blas_slots[ti.mesh].blas;
            put(node_data, as_float(blas_root), as_float(ti.material), as_float(live ? ti.flags : 0u), as_float(ti.channels));
        }
        set4(p.inst, root, inst_base, static_cast<std::uint32_t>(c.tlas.size()), 0u);
    }

    bool update_tlas(const lighting::detail::Compiled& c) {
        pack_tlas(c);
        const std::size_t tail = node_data.size() - geo_node_floats;
        if (!write_region(nodes, node_data, geo_node_floats, tail, "fizmo path tracer instances")) {
            if (failed) return false;
            if (!upload_slack(nodes, node_data, tail / 2 + 1024, "fizmo path tracer bvh")) return false;
        }
        return true;
    }

    bool refit_blases(const lighting::detail::Compiled& c) {
        if (c.mesh_blas.size() != blas_slots.size()) return false;
        for (std::size_t mi = 0; mi < c.mesh_blas.size(); ++mi) {
            BlasSlot& slot = blas_slots[mi];
            const auto& now = c.mesh_blas[mi];
            if (now == slot.blas) continue;
            if (!slot.blas) {
                bool referenced = false;
                for (const auto& ti : c.tlas) referenced = referenced || ti.mesh == mi;
                if (referenced) return false;
                continue;
            }
            if (!now || now->refits == 0 || now->tris.size() != slot.blas->tris.size() || now->bvh.nodes().size() != slot.blas->bvh.nodes().size()) return false;
        }
        for (std::size_t mi = 0; mi < c.mesh_blas.size(); ++mi) {
            BlasSlot& slot = blas_slots[mi];
            const auto& now = c.mesh_blas[mi];
            if (now == slot.blas || !slot.blas) continue;
            const std::size_t tri_at = static_cast<std::size_t>(slot.tri_base) * 24;
            for (std::size_t k = 0; k < now->tris.size(); ++k) {
                const std::size_t at = tri_at + k * 24;
                const float uv = tri_data[at + 19];
                const float col = tri_data[at + 23];
                const auto& tri = now->tris[k];
                pack_tri(tri_data, at, tri, 0u, tri.flags, 0u, -1, uv, col);
            }
            const auto& bn = now->bvh.nodes();
            const std::size_t node_at = static_cast<std::size_t>(slot.node_base) * 8;
            for (std::size_t k = 0; k < bn.size(); ++k) pack_node(node_data, node_at + k * 8, bn[k], bn[k].count > 0 ? bn[k].first + slot.tri_base : bn[k].first + slot.node_base);
            if (!write_region(tris, tri_data, tri_at, now->tris.size() * 24, "fizmo path tracer triangles")) return false;
            if (!write_region(nodes, node_data, node_at, bn.size() * 8, "fizmo path tracer bvh")) return false;
            slot.blas = now;
        }
        return true;
    }

    bool partial_update(lighting::Scene& sc, const lighting::detail::Compiled& c) {
        if (!uploaded || c.light_epoch != up_light || c.flat_epoch != up_flat) return false;
        const bool blas_changed = c.blas_epoch != up_blas;
        if (blas_changed && !refit_blases(c)) return false;
        if (failed) return false;
        if (blas_changed || c.tlas_epoch != up_tlas) {
            if (!update_tlas(c)) return false;
        }
        up_blas = c.blas_epoch;
        up_tlas = c.tlas_epoch;
        geometry_version = sc.geometry_version();
        lighting_version = sc.lighting_version();
        center = c.center;
        radius = c.radius;
        ++partial_updates;
        scene_dirty = false;
        params_dirty = true;
        samples = 0;
        moved = true;
        return true;
    }

    void build_scene() {
        lighting::Scene& sc = *scene;
        sc.commit();
        const lighting::detail::Compiled& c = sc.compiled();
        if (partial_update(sc, c)) return;
        if (failed) return;
        geometry_version = sc.geometry_version();
        lighting_version = sc.lighting_version();

        std::vector<int> tri_light(c.tris.size(), -1);
        for (std::size_t i = c.analytic_count; i < c.lights.size(); ++i) {
            const auto& l = c.lights[i];
            if (l.kind == lighting::detail::LightKind::triangle && l.tri < tri_light.size()) tri_light[l.tri] = static_cast<int>(i);
        }
        std::vector<float> t;
        t.reserve(c.tris.size() * 24);
        const auto& scene_materials = static_cast<const lighting::Scene&>(sc).materials();
        std::vector<float> extra;
        std::vector<std::pair<std::size_t, std::uint32_t>> patches;
        auto color_patch = [&](const lighting::detail::TriColors* tc) {
            if (!tc) return;
            patches.emplace_back(t.size() - 1, static_cast<std::uint32_t>(extra.size()));
            for (int k = 0; k < 3; ++k) extra.insert(extra.end(), { tc->c[k].x, tc->c[k].y, tc->c[k].z, tc->c[k].w });
        };
        for (std::size_t i = 0; i < c.tris.size(); ++i) {
            const auto& tri = c.tris[i];
            const std::uint32_t channels = tri.instance < c.instance_channels.size() ? c.instance_channels[tri.instance] : 1u;
            const bool uv = (tri.flags & lighting::detail::tri_texcoords) && tri.material < scene_materials.size() && gpu_base_map(scene_materials[tri.material]);
            pack_tri(t, t.size(), tri, tri.material, tri.flags, channels, tri_light[i], as_float(kNoUv), as_float(kNoUv));
            if (uv) {
                patches.emplace_back(t.size() - 5, static_cast<std::uint32_t>(extra.size()));
                extra.insert(extra.end(), { tri.tex0.x, tri.tex0.y, tri.tex1.x, tri.tex1.y, tri.tex2.x, tri.tex2.y });
            }
            color_patch(tri.colors < c.colors.size() ? &c.colors[tri.colors] : nullptr);
        }
        blas_slots.assign(c.mesh_blas.size(), BlasSlot{});
        std::vector<std::uint8_t> referenced(c.mesh_blas.size(), 0);
        for (const auto& ti : c.tlas)
            if (ti.mesh < referenced.size()) referenced[ti.mesh] = 1;
        for (std::size_t mi = 0; mi < c.mesh_blas.size(); ++mi) {
            const auto& b = c.mesh_blas[mi];
            if (!referenced[mi] || !b || b->tris.empty()) continue;
            blas_slots[mi].blas = b;
            blas_slots[mi].tri_base = static_cast<std::uint32_t>(t.size() / 24);
            for (const auto& tri : b->tris) {
                pack_tri(t, t.size(), tri, 0u, tri.flags, 0u, -1, as_float(kNoUv), as_float(kNoUv));
                if (tri.flags & lighting::detail::tri_texcoords) {
                    patches.emplace_back(t.size() - 5, static_cast<std::uint32_t>(extra.size()));
                    extra.insert(extra.end(), { tri.tex0.x, tri.tex0.y, tri.tex1.x, tri.tex1.y, tri.tex2.x, tri.tex2.y });
                }
                color_patch(tri.colors < b->colors.size() ? &b->colors[tri.colors] : nullptr);
            }
        }
        std::vector<float> n;
        const auto& bvh_nodes = c.bvh.nodes();
        n.reserve(bvh_nodes.size() * 8);
        for (const auto& node : bvh_nodes) pack_node(n, n.size(), node, node.first);
        for (std::size_t mi = 0; mi < blas_slots.size(); ++mi) {
            BlasSlot& slot = blas_slots[mi];
            if (!slot.blas) continue;
            slot.node_base = static_cast<std::uint32_t>(n.size() / 8);
            for (const auto& node : slot.blas->bvh.nodes()) pack_node(n, n.size(), node, node.count > 0 ? node.first + slot.tri_base : node.first + slot.node_base);
        }
        std::vector<float> m;
        const auto& materials = static_cast<const lighting::Scene&>(sc).materials();
        m.reserve(materials.size() * 28);
        std::vector<std::pair<const lighting::Texture*, PackedMap>> packed;
        for (const auto& mat : materials) {
            const bool base_map = gpu_base_map(mat);
            const lighting::Rgb base = base_map || !mat.base_color_map.valid() ? mat.base_color : mat.average_base_color();
            float opacity = mat.opacity * (mat.opacity_map.valid() ? average_channel(mat.opacity_map) : 1.0f);
            if (mat.base_color_map.valid() && !base_map) opacity *= mat.base_color_map.texture->average().a;
            put(m, base, mat.metallic * average_channel(mat.metallic_map));
            put(m, mat.average_emitted(), mat.roughness * average_channel(mat.roughness_map));
            put(m, mat.specular, mat.transmission * average_channel(mat.transmission_map), mat.ior, mat.abbe);
            put(m, mat.absorption_color, mat.absorption_distance);
            std::uint32_t flags = (mat.thin_walled ? 1u : 0u) | (mat.double_sided ? 2u : 0u);
            float xf[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
            PackedMap pm;
            if (base_map) {
                const lighting::Texture* key = mat.base_color_map.texture.get();
                auto found = std::find_if(packed.begin(), packed.end(), [key](const auto& e) { return e.first == key; });
                if (found == packed.end()) {
                    packed.emplace_back(key, pack_map(*key, extra));
                    found = packed.end() - 1;
                }
                pm = found->second;
                flags |= kMatBaseMap | (static_cast<std::uint32_t>(mat.base_color_map.wrap) << kMatWrapShift);
                if (mat.base_color_map.filter == lighting::TextureFilter::nearest) flags |= kMatNearest;
                if (key->color_space() == lighting::ColorSpace::srgb) flags |= kMatSrgb;
                if (pm.alpha) flags |= kMatAlpha;
                map_matrix(mat, xf);
            }
            put(m, opacity, as_float(flags), mat.alpha_cutoff, 0.0f);
            if (base_map) patches.emplace_back(m.size() + 3, pm.offset | 0x80000000u);
            put(m, xf[0], xf[1], xf[2], 0.0f);
            put(m, xf[3], xf[4], xf[5], as_float(pm.width | (pm.height << 16)));
        }
        std::vector<float> l;
        l.reserve(c.lights.size() * 44);
        for (const auto& pl : c.lights) {
            put(l, pl.p, as_float(static_cast<std::uint32_t>(pl.kind)));
            put(l, pl.n, pl.radius);
            put(l, pl.t, pl.half_length);
            put(l, pl.b, pl.half_width);
            put(l, pl.e1, pl.half_height);
            put(l, pl.e2, pl.area);
            put(l, pl.emit, pl.cyl_area);
            put(l, pl.cos_inner, pl.cos_outer, pl.cos_sun, pl.sun_solid_angle);
            put(l, pl.range, as_float(static_cast<std::uint32_t>(pl.falloff)), pl.exponent, pl.power);
            const std::uint32_t flags = (pl.two_sided ? 1u : 0u) | (pl.cone ? 2u : 0u) | (pl.shadows ? 4u : 0u) | (pl.visible ? 8u : 0u);
            put(l, pl.diffuse, pl.specular, pl.volumetric, as_float(flags));
            put(l, as_float(pl.channels), pl.shadow_strength, pl.bias, as_float(pl.tri));
        }

        std::vector<float> tab;
        emissive_offset = 0;
        const std::size_t na = std::min(c.analytic_count, c.lights.size());
        tab.push_back(0.0f);
        for (std::size_t i = na; i < c.lights.size(); ++i) tab.push_back(tab.back() + c.lights[i].power);
        emissive_power = tab.back();
        light_cdf_offset = static_cast<std::uint32_t>(tab.size());
        tab.push_back(0.0f);
        for (std::size_t i = 0; i < c.lights.size(); ++i) tab.push_back(tab.back() + c.lights[i].power);
        total_power = tab.back();

        env_visible = c.env.visible;
        env_lights = c.env.lights_scene;
        env_any = c.env.kind != lighting::SkyKind::none && c.env.intensity > 0.0f;
        env_rad_offset = static_cast<std::uint32_t>(tab.size());
        std::vector<float> lum_rows(static_cast<std::size_t>(kEnvWidth) * kEnvHeight, 0.0f);
        for (unsigned y = 0; y < kEnvHeight; ++y) {
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kEnvHeight);
            const float theta = v * lighting::kPi;
            const float st = std::sin(theta);
            for (unsigned x = 0; x < kEnvWidth; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kEnvWidth);
                const float phi = (u - 0.5f) * lighting::kTwoPi;
                const lighting::Vec3f dir = lighting::normalize(lighting::Vec3f{ st * std::sin(phi), std::cos(theta), -st * std::cos(phi) });
                lighting::Rgb r = env_any ? lighting::detail::sky_radiance(c, dir) : lighting::Rgb();
                if (!r.is_finite()) r = lighting::Rgb();
                tab.push_back(r.r);
                tab.push_back(r.g);
                tab.push_back(r.b);
                lum_rows[static_cast<std::size_t>(y) * kEnvWidth + x] = std::max(r.luminance(), 0.0f) * st;
            }
        }
        env_marg_offset = static_cast<std::uint32_t>(tab.size());
        std::vector<float> row_totals(kEnvHeight, 0.0f);
        std::vector<float> cond;
        cond.reserve(static_cast<std::size_t>(kEnvHeight) * (kEnvWidth + 1));
        for (unsigned y = 0; y < kEnvHeight; ++y) {
            float run = 0.0f;
            cond.push_back(0.0f);
            for (unsigned x = 0; x < kEnvWidth; ++x) {
                run += lum_rows[static_cast<std::size_t>(y) * kEnvWidth + x];
                cond.push_back(run);
            }
            row_totals[y] = run;
        }
        float run = 0.0f;
        tab.push_back(0.0f);
        for (unsigned y = 0; y < kEnvHeight; ++y) {
            run += row_totals[y];
            tab.push_back(run);
        }
        env_total = run;
        env_cond_offset = static_cast<std::uint32_t>(tab.size());
        tab.insert(tab.end(), cond.begin(), cond.end());
        spectral_offset = static_cast<std::uint32_t>(tab.size());
        for (int i = 0; i < kSpectralSteps; ++i) {
            const float nm = 360.0f + (830.0f - 360.0f) * static_cast<float>(i) / static_cast<float>(kSpectralSteps - 1);
            const lighting::Rgb w = lighting::spectral::wavelength_weight(nm);
            tab.push_back(w.r);
            tab.push_back(w.g);
            tab.push_back(w.b);
        }

        const std::uint32_t extra_base = static_cast<std::uint32_t>(tab.size());
        for (const auto& pt : patches) {
            const bool material_slot = (pt.second & 0x80000000u) != 0u;
            const std::uint32_t at = extra_base + (pt.second & 0x7FFFFFFFu);
            (material_slot ? m : t)[pt.first] = as_float(at);
        }
        tab.insert(tab.end(), extra.begin(), extra.end());

        center = c.center;
        radius = c.radius;
        tri_count = t.size() / 24;
        light_count = c.lights.size();
        geo_node_floats = n.size();
        node_data = std::move(n);
        pack_tlas(c);
        tri_data = std::move(t);
        if (!upload(tris, tri_data, "fizmo path tracer triangles")) return;
        if (!upload_slack(nodes, node_data, (node_data.size() - geo_node_floats) / 2 + 1024, "fizmo path tracer bvh")) return;
        if (!upload(mats, m, "fizmo path tracer materials")) return;
        if (!upload(lights, l, "fizmo path tracer lights")) return;
        if (!upload(tables, tab, "fizmo path tracer tables")) return;
        p.counts[0] = static_cast<std::uint32_t>(tri_count);
        p.counts[1] = static_cast<std::uint32_t>(bvh_nodes.size());
        p.counts[2] = static_cast<std::uint32_t>(materials.size());
        p.counts[3] = static_cast<std::uint32_t>(c.lights.size() - na);
        p.size[2] = static_cast<std::uint32_t>(c.lights.size());
        p.size[3] = static_cast<std::uint32_t>(na);
        uploaded = true;
        up_flat = c.flat_epoch;
        up_blas = c.blas_epoch;
        up_tlas = c.tlas_epoch;
        up_light = c.light_epoch;
        ++full_uploads;
        scene_dirty = false;
        params_dirty = true;
        samples = 0;
        moved = true;
    }

    void build_params(float photon_radius) {
        const lighting::LightingSettings& s = settings;
        const lighting::Vec3f f = lighting::normalize(lighting::Vec3f(camera.forward));
        lighting::Vec3f right = lighting::normalize(lighting::cross(f, lighting::Vec3f(camera.up)));
        if (lighting::length_squared(right) == 0.0f) right = lighting::Frame(f).t;
        const lighting::Vec3f up = lighting::cross(right, f);
        const float tan_half = std::tan(std::clamp(camera.fov_degrees, 1.0f, 179.0f) * 0.5f * lighting::kDegToRad);
        const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        const lighting::Vec3f eye(camera.position);
        set4(p.cam_pos, eye.x, eye.y, eye.z, tan_half);
        set4(p.cam_forward, f.x, f.y, f.z, aspect);
        set4(p.cam_right, right.x, right.y, right.z, camera.aperture);
        set4(p.cam_up, up.x, up.y, up.z, camera.focus_distance);
        p.size[0] = width;
        p.size[1] = height;
        std::uint32_t flags = 0;
        if (s.area_lights) flags |= f_area;
        if (s.emissive_lights) flags |= f_emissive;
        if (s.shadows != lighting::ShadowMethod::none) flags |= f_shadows;
        if (s.transport == lighting::Transport::spectral) flags |= f_spectral;
        if (s.dispersion) flags |= f_dispersion;
        if (caustics() && photon_budget() > 0) flags |= f_caustics;
        if (s.fog.enabled && s.fog.density > 0.0f) flags |= f_fog;
        if (env_visible) flags |= f_env_visible;
        if (env_lights) flags |= f_env_lights;
        if (env_any && env_lights && env_total > 0.0f) flags |= f_env_sample;
        if (s.srgb_output) flags |= f_srgb;
        if (flip) flags |= f_flip;
        if (s.shadow_casters == lighting::ShadowCasters::everything) flags |= f_casters_all;
        if (s.shadow_casters == lighting::ShadowCasters::sun_only) flags |= f_casters_sun;
        const std::uint32_t spp = static_cast<std::uint32_t>(std::clamp(samples_per_frame, 1, 64));
        const std::uint32_t rr = static_cast<std::uint32_t>(std::clamp(s.russian_roulette_depth, 0, 0xFFFF));
        set4(p.opts, static_cast<std::uint32_t>(std::max(s.max_bounces, 1)), rr | (spp << 16), flags, static_cast<std::uint32_t>(s.tone_mapper));
        set4(p.opts2, static_cast<std::uint32_t>(std::max(s.all_lights_threshold, 0)), static_cast<std::uint32_t>(std::max(s.light_samples, 1)), photon_budget(), static_cast<std::uint32_t>(std::max(s.photon_bounces, 1)));
        set4(p.offsets, emissive_offset, light_cdf_offset, env_rad_offset, env_marg_offset);
        set4(p.offsets2, env_cond_offset, spectral_offset, env_any ? kEnvWidth : 0u, env_any ? kEnvHeight : 0u);
        set4(p.scene, center.x, center.y, center.z, radius);
        const float r = std::max(photon_radius, 1e-4f);
        set4(p.photon, r, 1.0f / (r * 2.0f), total_power, emissive_power);
        set4(p.fog0, s.fog.density, s.fog.anisotropy, s.fog.height, s.fog.height_falloff);
        set4(p.fog1, s.fog.color.r, s.fog.color.g, s.fog.color.b, s.fog.max_distance);
        set4(p.tone, s.exposure, s.saturation, s.contrast, s.firefly_clamp);
        lighting::Rgb wb(1.0f);
        if (s.white_balance > 0.0f) wb = lighting::Rgb::from_kelvin(s.white_balance);
        set4(p.balance, wb.r, wb.g, wb.b, s.shadow_bias);
        const std::uint32_t budget = photon_budget();
        set4(p.misc, env_total, budget > 0 ? 1.0f / static_cast<float>(budget) : 0.0f, s.gamma, as_float(mix_seed(s.seed, 0x5EED)));
        if (have_prev_cam) {
            std::memcpy(p.prev_pos, prev_cam, sizeof(float) * 4);
            std::memcpy(p.prev_forward, prev_cam + 4, sizeof(float) * 4);
            std::memcpy(p.prev_right, prev_cam + 8, sizeof(float) * 4);
            std::memcpy(p.prev_up, prev_cam + 12, sizeof(float) * 4);
        } else {
            std::memcpy(p.prev_pos, p.cam_pos, sizeof(float) * 4);
            std::memcpy(p.prev_forward, p.cam_forward, sizeof(float) * 4);
            std::memcpy(p.prev_right, p.cam_right, sizeof(float) * 4);
            std::memcpy(p.prev_up, p.cam_up, sizeof(float) * 4);
        }
        const std::uint32_t passes = s.denoise ? static_cast<std::uint32_t>(std::clamp(s.denoise_passes, 1, 6)) : 0u;
        std::uint32_t history = 0;
        if (have_history) history = moved ? (temporal ? 1u : 0u) : 2u;
        if (samples == 0 && !temporal) history = 0;
        set4(p.rt, history, passes, history_cap, parity);
        set4(p.filter_strength, 4.0f, 512.0f, 0.0f, 0.0f);
    }

    void remember_camera() {
        std::memcpy(prev_cam, p.cam_pos, sizeof(float) * 4);
        std::memcpy(prev_cam + 4, p.cam_forward, sizeof(float) * 4);
        std::memcpy(prev_cam + 8, p.cam_right, sizeof(float) * 4);
        std::memcpy(prev_cam + 12, p.cam_up, sizeof(float) * 4);
        have_prev_cam = true;
    }

    bool ensure_group() {
        if (!group_dirty && group.valid()) return true;
        group.destroy();
        gpu::BindGroupEntry e[12] = {
            gpu::BindGroupEntry::uniform(0, params),
            gpu::BindGroupEntry::storage(1, tris),
            gpu::BindGroupEntry::storage(2, nodes),
            gpu::BindGroupEntry::storage(3, mats),
            gpu::BindGroupEntry::storage(4, lights),
            gpu::BindGroupEntry::storage(5, tables),
            gpu::BindGroupEntry::storage(6, accum),
            gpu::BindGroupEntry::storage(7, cells),
            gpu::BindGroupEntry::storage(8, photons),
            gpu::BindGroupEntry::storage_texture(9, output),
            gpu::BindGroupEntry::storage(10, guide),
            gpu::BindGroupEntry::storage(11, filter),
        };
        gpu::BindGroupDesc d;
        d.layout = &layout;
        d.entries = { e, 12 };
        d.name = "fizmo path tracer";
        if (gpu::failed(group.create(*device, d))) {
            fail("could not create the path tracer bind group");
            return false;
        }
        group_dirty = false;
        return true;
    }

    void run(GpuPassContext& ctx) {
        if (failed || !ctx.device || !ctx.commands) return;
        if (!device) {
            device = ctx.device;
            if (!create_pipeline()) return;
        }
        if (!scene) return;
        if (scene->geometry_version() != geometry_version || scene->lighting_version() != lighting_version) scene_dirty = true;
        if (!scene->committed()) scene_dirty = true;
        if (scene_dirty) {
            build_scene();
            if (failed) return;
        }
        if (!ensure_output()) return;
        if (!tris.valid() || !nodes.valid() || !mats.valid() || !lights.valid() || !tables.valid()) return;
        if (!ensure_group()) return;
        gpu::CommandList& cmd = *ctx.commands;
        const bool tracing = !paused && (max_samples == 0 || samples < max_samples);
        if (!tracing && !output_dirty && !params_dirty) {
            copy_out(cmd);
            return;
        }
        if (samples == 0) photon_frames = 0;
        const float shrink = std::pow(static_cast<float>(photon_frames + 1), -1.0f / 6.0f);
        if (tracing) parity ^= 1u;
        build_params(settings.photon_radius * shrink);
        if (gpu::failed(params.write(&p, sizeof(Params), 0))) {
            fail("could not upload the path tracer parameters");
            return;
        }
        params_dirty = false;
        cmd.push_label("fizmo path tracer");
        cmd.set_pipeline(pipeline);
        cmd.set_bind_group(0, group);
        Push push;
        push.frame = frames;
        push.first_sample = samples;
        push.seed = mix_seed(settings.seed, static_cast<std::uint64_t>(samples) * 7919u + frames + total_frames * 104729u);
        const std::uint32_t gx = (width + 7u) / 8u, gy = (height + 7u) / 8u;
        if (tracing) {
            const std::uint32_t budget = (p.opts[2] & f_caustics) ? photon_budget() : 0u;
            if (budget > 0) {
                push.mode = 0;
                cmd.push_constants(push);
                cmd.dispatch(32, kCellCount / 256u / 8u);
                push.mode = 1;
                cmd.push_constants(push);
                cmd.dispatch(32, (budget + 2047u) / 2048u);
                ++photon_frames;
            }
            push.mode = 2;
            cmd.push_constants(push);
            cmd.dispatch(gx, gy);
            samples += static_cast<std::uint32_t>(std::clamp(samples_per_frame, 1, 64));
            ++frames;
            ++total_frames;
            have_history = true;
            moved = false;
            remember_camera();
        }
        if (p.rt[1] > 0u) {
            push.mode = 5;
            push.first_sample = 0;
            cmd.push_constants(push);
            cmd.dispatch(gx, gy);
        }
        for (std::uint32_t k = 0; k < p.rt[1]; ++k) {
            push.mode = 4;
            push.first_sample = k;
            cmd.push_constants(push);
            cmd.dispatch(gx, gy);
        }
        push.mode = 3;
        cmd.push_constants(push);
        cmd.dispatch(gx, gy);
        cmd.pop_label();
        output_dirty = false;
        copy_out(cmd);
    }

    void copy_out(gpu::CommandList& cmd) {
        if (!target_texture || !output.valid()) return;
        if (target_texture->width() != width || target_texture->height() != height) return;
        cmd.copy_texture(output, *target_texture, gpu::Rect2D{ { 0, 0 }, { width, height } });
    }
};

GpuPathTracer::GpuPathTracer() : m_state(std::make_unique<State>()) {}

GpuPathTracer::~GpuPathTracer() {
    detach();
}

bool GpuPathTracer::attach(Renderer& renderer, unsigned width, unsigned height) {
    detach();
    State& s = *m_state;
    s.renderer = &renderer;
    s.failed = false;
    s.error.clear();
    if (!renderer.gpu_device()) {
        s.supported = false;
        s.fail("the renderer has no GPU device");
        s.renderer = nullptr;
        return false;
    }
    resize(width, height);
    State* raw = m_state.get();
    s.pass_id = renderer.add_gpu_pass(GpuPassStage::AfterTargets, [raw](GpuPassContext& ctx) { raw->run(ctx); });
    s.has_pass = true;
    return true;
}

void GpuPathTracer::detach() {
    State& s = *m_state;
    if (s.renderer && s.has_pass) s.renderer->remove_gpu_pass(s.pass_id);
    if (s.device) s.device->wait_idle();
    s.has_pass = false;
    s.renderer = nullptr;
    s.target_texture = nullptr;
    s.group.destroy();
    s.output.destroy();
    s.accum.destroy();
    s.cells.destroy();
    s.photons.destroy();
    s.guide.destroy();
    s.filter.destroy();
    s.have_history = false;
    s.have_prev_cam = false;
    s.tris.destroy();
    s.nodes.destroy();
    s.mats.destroy();
    s.lights.destroy();
    s.tables.destroy();
    s.params.destroy();
    s.pipeline.destroy();
    s.pipeline_layout.destroy();
    s.layout.destroy();
    s.shader.destroy();
    s.device = nullptr;
    s.scene_dirty = true;
    s.group_dirty = true;
    s.uploaded = false;
    s.samples = 0;
}

bool GpuPathTracer::attached() const noexcept { return m_state->has_pass; }

bool GpuPathTracer::supported() const noexcept { return m_state->supported && !m_state->failed; }

const std::string& GpuPathTracer::error() const noexcept { return m_state->error; }

void GpuPathTracer::set_settings(const lighting::LightingSettings& settings) {
    m_state->settings = settings;
    m_state->params_dirty = true;
    m_state->have_history = false;
    reset();
}

const lighting::LightingSettings& GpuPathTracer::settings() const noexcept { return m_state->settings; }

void GpuPathTracer::set_scene(lighting::Scene& scene) {
    m_state->scene = &scene;
    m_state->scene_dirty = true;
    reset();
}

void GpuPathTracer::scene_changed() {
    m_state->scene_dirty = true;
    reset();
}

void GpuPathTracer::set_camera(const lighting::Camera& camera) {
    State& s = *m_state;
    const lighting::Camera& a = s.camera;
    const bool same = a.position.x == camera.position.x && a.position.y == camera.position.y && a.position.z == camera.position.z
        && a.forward.x == camera.forward.x && a.forward.y == camera.forward.y && a.forward.z == camera.forward.z
        && a.up.x == camera.up.x && a.up.y == camera.up.y && a.up.z == camera.up.z
        && a.fov_degrees == camera.fov_degrees && a.aperture == camera.aperture && a.focus_distance == camera.focus_distance;
    if (same) return;
    s.camera = camera;
    s.params_dirty = true;
    reset();
}

const lighting::Camera& GpuPathTracer::camera() const noexcept { return m_state->camera; }

void GpuPathTracer::resize(unsigned width, unsigned height) {
    State& s = *m_state;
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (s.width == width && s.height == height && s.target.valid()) return;
    s.width = width;
    s.height = height;
    if (s.target.valid()) s.target.resize(width, height);
    else s.target = RenderTarget(width, height);
    s.target_texture = nullptr;
    s.params_dirty = true;
    s.have_history = false;
    s.have_prev_cam = false;
    reset();
}

void GpuPathTracer::reset() {
    m_state->samples = 0;
    m_state->frames = 0;
    m_state->moved = true;
    m_state->output_dirty = true;
}

void GpuPathTracer::clear_history() {
    m_state->have_history = false;
    reset();
}

void GpuPathTracer::set_temporal(bool on, std::uint32_t history_cap) {
    m_state->temporal = on;
    m_state->history_cap = std::max(1u, history_cap);
    m_state->params_dirty = true;
}

bool GpuPathTracer::temporal() const noexcept { return m_state->temporal; }

void GpuPathTracer::set_photons_per_frame(std::uint32_t photons) {
    m_state->photons_per_frame = photons;
    m_state->params_dirty = true;
    reset();
}

void GpuPathTracer::set_samples_per_frame(int samples) {
    m_state->samples_per_frame = std::clamp(samples, 1, 64);
    m_state->params_dirty = true;
}

void GpuPathTracer::set_max_samples(std::uint32_t samples) { m_state->max_samples = samples; }

void GpuPathTracer::set_paused(bool paused) { m_state->paused = paused; }

bool GpuPathTracer::paused() const noexcept { return m_state->paused; }

unsigned GpuPathTracer::width() const noexcept { return m_state->width; }

unsigned GpuPathTracer::height() const noexcept { return m_state->height; }

std::uint32_t GpuPathTracer::samples() const noexcept { return m_state->samples; }

bool GpuPathTracer::converged() const noexcept { return m_state->max_samples > 0 && m_state->samples >= m_state->max_samples; }

GpuTraceStats GpuPathTracer::stats() const noexcept {
    const State& s = *m_state;
    GpuTraceStats out;
    out.samples = s.samples;
    out.frames = s.frames;
    out.photons_per_frame = s.photon_budget();
    out.triangles = s.tri_count;
    out.lights = s.light_count;
    out.caustics = s.caustics();
    out.spectral = s.settings.transport == lighting::Transport::spectral;
    return out;
}

const RenderTarget& GpuPathTracer::target() const noexcept { return m_state->target; }

void GpuPathTracer::draw(Renderer& renderer, int x, int y, unsigned width, unsigned height, float opacity) {
    State& s = *m_state;
    if (!s.target.valid()) return;
    s.target_texture = renderer.gpu_texture(s.target);
    renderer.draw_target(s.target, x, y, width, height, opacity);
}

void GpuPathTracer::draw(Renderer& renderer, int x, int y) {
    draw(renderer, x, y, m_state->width, m_state->height, 1.0f);
}

bool GpuPathTracer::read_hdr(lighting::Image& out) {
    State& s = *m_state;
    if (!s.device || !s.accum.valid() || s.width == 0 || s.height == 0) return false;
    std::vector<float> data(static_cast<std::size_t>(s.width) * s.height * 4u);
    if (gpu::failed(s.accum.read(data.data(), data.size() * sizeof(float), static_cast<std::uint64_t>(s.parity & 1u) * data.size() * sizeof(float)))) return false;
    out = lighting::Image(s.width, s.height);
    for (std::size_t i = 0; i < out.pixels().size(); ++i) {
        const float w = data[i * 4 + 3];
        out.pixels()[i] = w > 0.0f ? lighting::Rgb(data[i * 4] / w, data[i * 4 + 1] / w, data[i * 4 + 2] / w) : lighting::Rgb();
    }
    return true;
}

bool GpuPathTracer::wants_gpu(const lighting::LightingSettings& settings) noexcept {
    return settings.quality >= lighting::LightingQuality::path_traced;
}

} // namespace graphics
} // namespace fizmo
