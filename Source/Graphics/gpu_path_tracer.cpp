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
};

static_assert(sizeof(Params) == 272, "Params must match the std140 block in path_tracer.comp");

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
        gpu::BindGroupLayoutEntry entries[10];
        for (std::uint32_t i = 0; i < 10; ++i) {
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
        gpu::BindGroupLayoutDesc ld;
        ld.entries = { entries, 10 };
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
        if (!accum.valid() || accum.size() < bytes) {
            accum.destroy();
            if (!storage(accum, bytes, "fizmo path tracer accumulation")) return false;
            samples = 0;
        }
        return true;
    }

    void build_scene() {
        lighting::Scene& sc = *scene;
        sc.commit();
        const lighting::detail::Compiled& c = sc.compiled();
        geometry_version = sc.geometry_version();
        lighting_version = sc.lighting_version();

        std::vector<int> tri_light(c.tris.size(), -1);
        for (std::size_t i = c.analytic_count; i < c.lights.size(); ++i) {
            const auto& l = c.lights[i];
            if (l.kind == lighting::detail::LightKind::triangle && l.tri < tri_light.size()) tri_light[l.tri] = static_cast<int>(i);
        }
        std::vector<float> t;
        t.reserve(c.tris.size() * 24);
        for (std::size_t i = 0; i < c.tris.size(); ++i) {
            const auto& tri = c.tris[i];
            const std::uint32_t channels = tri.instance < c.instance_channels.size() ? c.instance_channels[tri.instance] : 1u;
            put(t, tri.v0, as_float(tri.material));
            put(t, tri.e1, as_float(tri.flags));
            put(t, tri.e2, as_float(channels));
            std::int32_t li = tri_light[i];
            std::uint32_t lu;
            std::memcpy(&lu, &li, sizeof(lu));
            put(t, tri.n0, as_float(lu));
            put(t, tri.n1, 0.0f);
            put(t, tri.n2, 0.0f);
        }
        std::vector<float> n;
        const auto& bvh_nodes = c.bvh.nodes();
        n.reserve(bvh_nodes.size() * 8);
        for (const auto& node : bvh_nodes) {
            put(n, node.lo, as_float(node.first));
            put(n, node.hi, as_float(node.count));
        }
        std::vector<float> m;
        const auto& materials = static_cast<const lighting::Scene&>(sc).materials();
        m.reserve(materials.size() * 20);
        for (const auto& mat : materials) {
            put(m, mat.base_color, mat.metallic);
            put(m, mat.emitted(), mat.roughness);
            put(m, mat.specular, mat.transmission, mat.ior, mat.abbe);
            put(m, mat.absorption_color, mat.absorption_distance);
            put(m, mat.opacity, as_float((mat.thin_walled ? 1u : 0u) | (mat.double_sided ? 2u : 0u)), 0.0f, 0.0f);
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

        center = c.center;
        radius = c.radius;
        tri_count = c.tris.size();
        light_count = c.lights.size();
        if (!upload(tris, t, "fizmo path tracer triangles")) return;
        if (!upload(nodes, n, "fizmo path tracer bvh")) return;
        if (!upload(mats, m, "fizmo path tracer materials")) return;
        if (!upload(lights, l, "fizmo path tracer lights")) return;
        if (!upload(tables, tab, "fizmo path tracer tables")) return;
        p.counts[0] = static_cast<std::uint32_t>(c.tris.size());
        p.counts[1] = static_cast<std::uint32_t>(bvh_nodes.size());
        p.counts[2] = static_cast<std::uint32_t>(materials.size());
        p.counts[3] = static_cast<std::uint32_t>(c.lights.size() - na);
        p.size[2] = static_cast<std::uint32_t>(c.lights.size());
        p.size[3] = static_cast<std::uint32_t>(na);
        scene_dirty = false;
        params_dirty = true;
        samples = 0;
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
    }

    bool ensure_group() {
        if (!group_dirty && group.valid()) return true;
        group.destroy();
        gpu::BindGroupEntry e[10] = {
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
        };
        gpu::BindGroupDesc d;
        d.layout = &layout;
        d.entries = { e, 10 };
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
        push.seed = mix_seed(settings.seed, static_cast<std::uint64_t>(samples) * 7919u + frames);
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
    s.samples = 0;
}

bool GpuPathTracer::attached() const noexcept { return m_state->has_pass; }

bool GpuPathTracer::supported() const noexcept { return m_state->supported && !m_state->failed; }

const std::string& GpuPathTracer::error() const noexcept { return m_state->error; }

void GpuPathTracer::set_settings(const lighting::LightingSettings& settings) {
    m_state->settings = settings;
    m_state->params_dirty = true;
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
    reset();
}

void GpuPathTracer::reset() {
    m_state->samples = 0;
    m_state->frames = 0;
    m_state->output_dirty = true;
}

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
    if (gpu::failed(s.accum.read(data.data(), data.size() * sizeof(float), 0))) return false;
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
