#ifndef FIZMO_GL_SHADERS_HPP
#define FIZMO_GL_SHADERS_HPP

#include "../../../GPU/types.hpp"
#include "gpu_shaders.hpp"
#include "gpu_shaders_3d.hpp"
#include <cstdint>

namespace fizmo {
namespace graphics {
namespace detail {
namespace glsl {

inline constexpr int kDraw2DVertVersion = 330;

inline constexpr const char* kDraw2DVert[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    vec2 inv_half_size;
} pc;

out vec2 v_uv;
layout(location = 1) in vec2 in_uv;
out vec4 v_color;
layout(location = 2) in vec4 in_color;
flat out uint v_mode;
layout(location = 3) in uint in_mode;
flat out vec4 v_grad;
layout(location = 4) in vec4 in_grad;
out vec2 v_pos;
layout(location = 0) in vec2 in_pos;

void main()
{
    v_uv = in_uv;
    v_color = in_color;
    v_mode = in_mode;
    v_grad = in_grad;
    v_pos = in_pos;
    gl_Position = vec4((in_pos * pc.inv_half_size) - vec2(1.0), 0.0, 1.0);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kDraw2DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc draw2d_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kDraw2DVert, sizeof(::fizmo::graphics::detail::gfx::kDraw2DVert) / sizeof(std::uint32_t) };
    d.glsl          = kDraw2DVert;
    d.glsl_pieces   = sizeof(kDraw2DVert) / sizeof(kDraw2DVert[0]);
    d.glsl_version  = kDraw2DVertVersion;
    d.glsl_bindings = { kDraw2DVertBindings, 1 };
    d.name          = "draw2d.vert";
    return d;
}

inline constexpr int kDraw2DFragVersion = 330;

inline constexpr const char* kDraw2DFrag[] = {
R"fizmo_glsl(uniform sampler2D tex;
uniform sampler2D ramp;

flat in uint v_mode;
layout(location = 0) out vec4 out_color;
in vec2 v_uv;
in vec4 v_color;
flat in vec4 v_grad;
in vec2 v_pos;

float apply_spread(float t, uint spread)
{
    if (spread == 1u)
    {
        return fract(t);
    }
    if (spread == 2u)
    {
        float m = mod(t, 2.0);
        float _36;
        if (m > 1.0)
        {
            _36 = 2.0 - m;
        }
        else
        {
            _36 = m;
        }
        return _36;
    }
    return clamp(t, 0.0, 1.0);
}

void main()
{
    uint kind = v_mode & 3u;
    if (kind == 0u)
    {
        out_color = texture(tex, v_uv) * v_color;
        return;
    }
    float t;
    if (kind == 1u)
    {
        vec2 d = v_grad.zw - v_grad.xy;
        t = dot(v_pos - v_grad.xy, d) / max(dot(d, d), 9.9999999747524270787835121154785e-07);
    }
    else
    {
        t = length(v_pos - v_grad.xy) / max(v_grad.z, 9.9999999747524270787835121154785e-07);
    }
    float param = t;
    uint param_1 = (v_mode >> uint(2)) & 3u;
    t = apply_spread(param, param_1);
    float row = float(v_mode >> uint(16));
    out_color = texture(ramp, vec2(((t * 255.0) + 0.5) / 256.0, (row + 0.5) / 256.0)) * v_color;
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kDraw2DFragBindings[] = {
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "ramp", ::fizmo::gpu::GlslResource::Texture, 0, 1, 1 },
};

inline ::fizmo::gpu::ShaderDesc draw2d_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kDraw2DFrag, sizeof(::fizmo::graphics::detail::gfx::kDraw2DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kDraw2DFrag;
    d.glsl_pieces   = sizeof(kDraw2DFrag) / sizeof(kDraw2DFrag[0]);
    d.glsl_version  = kDraw2DFragVersion;
    d.glsl_bindings = { kDraw2DFragBindings, 2 };
    d.name          = "draw2d.frag";
    return d;
}

inline constexpr int kPresentVertVersion = 330;

inline constexpr const char* kPresentVert[] = {
R"fizmo_glsl(out vec2 v_uv;

void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_uv = p;
    gl_Position = vec4((p * 2.0) - vec2(1.0), 0.0, 1.0);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kPresentVertBindings[] = {
    { nullptr, ::fizmo::gpu::GlslResource::Texture, 0, 0, 0 },
};

inline ::fizmo::gpu::ShaderDesc present_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kPresentVert, sizeof(::fizmo::graphics::detail::gfx::kPresentVert) / sizeof(std::uint32_t) };
    d.glsl          = kPresentVert;
    d.glsl_pieces   = sizeof(kPresentVert) / sizeof(kPresentVert[0]);
    d.glsl_version  = kPresentVertVersion;
    d.glsl_bindings = { kPresentVertBindings, 0 };
    d.name          = "present.vert";
    return d;
}

inline constexpr int kPresentFragVersion = 330;

inline constexpr const char* kPresentFrag[] = {
R"fizmo_glsl(uniform sampler2D src;

layout(location = 0) out vec4 out_color;
in vec2 v_uv;

void main()
{
    out_color = vec4(texture(src, v_uv).xyz, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kPresentFragBindings[] = {
    { "src", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc present_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kPresentFrag, sizeof(::fizmo::graphics::detail::gfx::kPresentFrag) / sizeof(std::uint32_t) };
    d.glsl          = kPresentFrag;
    d.glsl_pieces   = sizeof(kPresentFrag) / sizeof(kPresentFrag[0]);
    d.glsl_version  = kPresentFragVersion;
    d.glsl_bindings = { kPresentFragBindings, 1 };
    d.name          = "present.frag";
    return d;
}

inline constexpr int kUpscaleFragVersion = 330;

inline constexpr const char* kUpscaleFrag[] = {
R"fizmo_glsl(layout(std140) uniform Upscale
{
    vec4 rect;
    vec4 info;
} pc;

uniform sampler2D src;

in vec2 v_uv;
layout(location = 0) out vec4 out_color;

vec2 texel()
{
    return pc.info.xy;
}

vec3 fetch(ivec2 p)
{
    ivec2 lo = ivec2(ceil((pc.rect.xy / texel()) - vec2(9.9999997473787516355514526367188e-05)));
    ivec2 hi = ivec2(floor((pc.rect.zw / texel()) + vec2(9.9999997473787516355514526367188e-05))) - ivec2(1);
    return texelFetch(src, clamp(p, lo, hi), 0).xyz;
}

vec2 keep(vec2 uv)
{
    return clamp(uv, pc.rect.xy + (texel() * 0.5), pc.rect.zw - (texel() * 0.5));
}

vec3 bicubic(vec2 uv)
{
    vec2 pos = uv / texel();
    vec2 center = floor(pos - vec2(0.5)) + vec2(0.5);
    vec2 f = pos - center;
    vec2 w0 = f * (vec2(-0.5) + (f * (vec2(1.0) - (f * 0.5))));
    vec2 w1 = vec2(1.0) + ((f * f) * (vec2(-2.5) + (f * 1.5)));
    vec2 w2 = f * (vec2(0.5) + (f * (vec2(2.0) - (f * 1.5))));
    vec2 w3 = (f * f) * (vec2(-0.5) + (f * 0.5));
    vec2 w12 = w1 + w2;
    vec2 t0 = (center - vec2(1.0)) * texel();
    vec2 t3 = (center + vec2(2.0)) * texel();
    vec2 t12 = (center + (w2 / w12)) * texel();
    float a = w12.x * w0.y;
    float b = w0.x * w12.y;
    float c = w12.x * w12.y;
    float d = w3.x * w12.y;
    float e = w12.x * w3.y;
    vec2 param = vec2(t12.x, t0.y);
    vec2 param_1 = vec2(t0.x, t12.y);
    vec2 param_2 = t12;
    vec2 param_3 = vec2(t3.x, t12.y);
    vec2 param_4 = vec2(t12.x, t3.y);
    vec3 sum = ((((texture(src, keep(param)).xyz * a) + (texture(src, keep(param_1)).xyz * b)) + (texture(src, keep(param_2)).xyz * c)) + (texture(src, keep(param_3)).xyz * d)) + (texture(src, keep(param_4)).xyz * e);
    return sum / vec3((((a + b) + c) + d) + e);
}

void main()
{
    vec2 uv = mix(pc.rect.xy, pc.rect.zw, v_uv);
    if (pc.info.w < 0.5)
    {
        ivec2 param = ivec2(floor(uv / texel()));
        out_color = vec4(fetch(param), 1.0);
        return;
    }
    vec2 param_1 = uv;
    vec3 smooth_color = texture(src, keep(param_1)).xyz;
    if (pc.info.w < 1.5)
    {
        out_color = vec4(smooth_color, 1.0);
        return;
    }
    ivec2 base = ivec2(floor((uv / texel()) - vec2(0.5)));
    ivec2 param_2 = base;
    vec3 a = fetch(param_2);
    ivec2 param_3 = base + ivec2(1, 0);
    vec3 b = fetch(param_3);
    ivec2 param_4 = base + ivec2(0, 1);
    vec3 c = fetch(param_4);
    ivec2 param_5 = base + ivec2(1);
    vec3 d = fetch(param_5);
    vec3 mn = min(min(a, b), min(c, d));
    vec3 mx = max(max(a, b), max(c, d));
    vec2 param_6 = uv;
    vec3 cubic = bicubic(param_6);
    vec3 amp = sqrt(clamp(min(mn, vec3(1.0) - mx) / max(mx, vec3(9.9999997473787516355514526367188e-05)), vec3(0.0), vec3(1.0)));
    vec3 sharp = cubic + ((((cubic - smooth_color) * amp) * pc.info.z) * 2.0);
    out_color = vec4(clamp(sharp, mn, mx), 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kUpscaleFragBindings[] = {
    { "src", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "Upscale", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc upscale_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kUpscaleFrag, sizeof(::fizmo::graphics::detail::gfx::kUpscaleFrag) / sizeof(std::uint32_t) };
    d.glsl          = kUpscaleFrag;
    d.glsl_pieces   = sizeof(kUpscaleFrag) / sizeof(kUpscaleFrag[0]);
    d.glsl_version  = kUpscaleFragVersion;
    d.glsl_bindings = { kUpscaleFragBindings, 2 };
    d.name          = "upscale.frag";
    return d;
}

inline constexpr int kMesh3DVertVersion = 410;

inline constexpr const char* kMesh3DVert[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    layout(row_major) mat4 mvp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    uvec4 extra;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 1) in vec3 in_normal;
layout(location = 2) out vec4 v_light;
layout(location = 3) flat out uint v_flags;
layout(location = 4) out vec4 v_color;
layout(location = 3) in vec4 in_color;
layout(location = 5) out vec2 v_uv;
layout(location = 2) in vec2 in_uv;
layout(location = 6) flat out uvec2 v_pbr;
layout(location = 7) out vec3 v_object;
layout(location = 8) out vec3 v_object_normal;

void main()
{
    vec4 p = vec4(in_pos, 1.0);
    gl_Position = pc.mvp * p;
    v_world = vec3(dot(pc.model_x, p), dot(pc.model_y, p), dot(pc.model_z, p));
    v_normal = vec3(dot(pc.model_x.xyz, in_normal), dot(pc.model_y.xyz, in_normal), dot(pc.model_z.xyz, in_normal));
    v_light = unpackUnorm4x8(pc.extra.x);
    v_flags = pc.extra.y;
    v_color = in_color;
    v_uv = in_uv;
    v_pbr = pc.extra.zw;
    v_object = in_pos;
    v_object_normal = in_normal;
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kMesh3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc mesh3d_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kMesh3DVert, sizeof(::fizmo::graphics::detail::gfx::kMesh3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kMesh3DVert;
    d.glsl_pieces   = sizeof(kMesh3DVert) / sizeof(kMesh3DVert[0]);
    d.glsl_version  = kMesh3DVertVersion;
    d.glsl_bindings = { kMesh3DVertBindings, 1 };
    d.name          = "mesh3d.vert";
    return d;
}

inline constexpr int kMesh3DFragVersion = 410;

inline constexpr const char* kMesh3DFrag[] = {
R"fizmo_glsl(const vec3 _1494[8] = vec3[](vec3(1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(-1.0), vec3(1.0, -1.0, 1.0), vec3(-1.0, 1.0, 1.0));

layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
} scene;

layout(std140) uniform MaterialMaps
{
    vec4 albedo_u;
    vec4 albedo_v;
    vec4 normal_u;
    vec4 normal_v;
    vec4 orm_u;
    vec4 orm_v;
    vec4 emissive_u;
    vec4 emissive_v;
    vec4 opacity_u;
    vec4 opacity_v;
    vec4 params;
    uvec4 flags;
    vec4 soft;
    vec4 splat;
    vec4 atlas;
    vec4 tiling[2];
    vec4 tints[8];
    vec4 splat_u;
    vec4 splat_v;
} maps;

uniform sampler2DShadow sun_map;
uniform sampler2D sun_depth;
uniform sampler2DShadow sun_map_b;
uniform sampler2D sun_depth_b;
uniform samplerCubeShadow point_maps[8];
uniform sampler3D volume_light;
uniform sampler2D scene_depth;
uniform sampler2D scene_color;
uniform sampler2D normal_map;
uniform sampler2D splat_weights;
uniform sampler2D splat_weights_b;
uniform sampler2D splat_albedo;
uniform sampler2D splat_normals;
uniform sampler2D tex;
uniform sampler2D opacity_map;
uniform sampler2D orm_map;
uniform sampler2D emissive_map;
uniform sampler2D planar_maps[2];

layout(location = 5) in vec2 v_uv;
layout(location = 8) in vec3 v_object_normal;
layout(location = 7) in vec3 v_object;
layout(location = 0) in vec3 v_world;
layout(location = 4) in vec4 v_color;
layout(location = 1) in vec3 v_normal;
layout(location = 3) flat in uint v_flags;
layout(location = 6) flat in uvec2 v_pbr;
layout(location = 2) in vec4 v_light;
layout(location = 0) out vec4 out_color;
float g_ao_frac;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

vec2 map_uv(vec4 u, vec4 v)
{
    vec3 p = vec3(v_uv, 1.0);
    return vec2(dot(u.xyz, p), dot(v.xyz, p));
}

vec3 triplanar_weights()
{
    vec3 n = abs(normalize(v_object_normal + vec3(9.9999999747524270787835121154785e-07)));
    vec3 w = pow(n, vec3(max(maps.params.w, 1.0)));
    return w / vec3(max((w.x + w.y) + w.z, 9.9999999747524270787835121154785e-07));
}

vec4 map_sample(sampler2D s, vec4 u, vec4 v)
{
    if ((maps.flags.x & 16u) == 0u)
    {
        vec4 param = u;
        vec4 param_1 = v;
        return texture(s, map_uv(param, param_1));
    }
    vec3 w = triplanar_weights();
    vec3 q = v_object * maps.params.z;
    vec4 c = vec4(0.0);
    c += (texture(s, vec2(dot(u.xyz, vec3(q.z, -q.y, 1.0)), dot(v.xyz, vec3(q.z, -q.y, 1.0)))) * w.x);
    c += (texture(s, vec2(dot(u.xyz, vec3(q.x, -q.z, 1.0)), dot(v.xyz, vec3(q.x, -q.z, 1.0)))) * w.y);
    c += (texture(s, vec2(dot(u.xyz, vec3(q.x, -q.y, 1.0)), dot(v.xyz, vec3(q.x, -q.y, 1.0)))) * w.z);
    return c;
}

vec3 mapped_normal(vec3 normal)
{
    float len = length(normal);
    if (len <= 9.9999999747524270787835121154785e-07)
    {
        return normal;
    }
    vec3 n = normal / vec3(len);
    vec4 param = maps.normal_u;
    vec4 param_1 = maps.normal_v;
    vec2 uv = map_uv(param, param_1);
    vec3 ts = (texture(normal_map, uv).xyz * 2.0) - vec3(1.0);
    vec3 _3255 = ts;
    vec2 _3257 = _3255.xy * maps.params.x;
    ts.x = _3257.x;
    ts.y = _3257.y;
    ts.z = max(ts.z, 0.001000000047497451305389404296875);
    vec3 dp1 = dFdx(v_world);
    vec3 dp2 = dFdy(v_world);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    float facing = (dot(n, cross(dp1, dp2)) < 0.0) ? (-1.0) : 1.0;
    vec3 tu = ((dp2perp * duv1.x) + (dp1perp * duv2.x)) * facing;
    vec3 tv = ((dp2perp * duv1.y) + (dp1perp * duv2.y)) * facing;
    float scale = max(dot(tu, tu), dot(tv, tv));
    if (scale <= 9.9999996826552253889678874634872e-21)
    {
        return normal;
    }
    scale = inversesqrt(scale);
    vec3 p = normalize(((tu * (scale * ts.x)) - (tv * (scale * ts.y))) + (n * ts.z));
    return p * len;
}

vec2 splat_coord()
{
    vec3 p = vec3(v_uv, 1.0);
    return vec2(dot(maps.splat_u.xyz, p), dot(maps.splat_v.xyz, p));
}

void splat_read(inout float w[8])
{
    vec2 su = splat_coord();
    vec4 a = texture(splat_weights, su);
    vec4 _3697;
    if ((maps.flags.x & 1024u) != 0u)
    {
        _3697 = texture(splat_weights_b, su);
    }
    else
    {
        _3697 = vec4(0.0);
    }
    vec4 b = _3697;
    w[0] = a.x;
    w[1] = a.y;
    w[2] = a.z;
    w[3] = a.w;
    w[4] = b.x;
    w[5] = b.y;
    w[6] = b.z;
    w[7] = b.w;
}

float splat_tiling(int i)
{
    return maps.tiling[i / 4][i % 4];
}

float splat_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.09999847412109375, 311.70001220703125))) * 43758.546875);
}

float splat_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = (f * f) * (vec2(3.0) - (f * 2.0));
    vec2 param = i;
    float a = splat_hash(param);
    vec2 param_1 = i + vec2(1.0, 0.0);
    float b = splat_hash(param_1);
    vec2 param_2 = i + vec2(0.0, 1.0);
    float c = splat_hash(param_2);
    vec2 param_3 = i + vec2(1.0);
    float d = splat_hash(param_3);
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec2 splat_origin(int i)
{
    int cols = max(int(maps.splat.z + 0.5), 1);
    return (vec2(float(i % cols), float(i / cols)) * maps.atlas.xy) + maps.atlas.zw;
}

vec4 splat_fetch(sampler2D s, int i, vec2 p, vec2 gx, vec2 gy, float vary)
{
    vec2 tile = maps.atlas.xy - (maps.atlas.zw * 2.0);
    vec2 texels = tile * vec2(textureSize(s, 0));
    int param = i;
    float t = splat_tiling(param);
    vec2 a = p * t;
    vec2 dx = (gx * t) * texels;
    vec2 dy = (gy * t) * texels;
    float lod = 0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 9.9999999392252902907785028219223e-09));
    int param_1 = i;
    vec2 base = splat_origin(param_1);
    vec4 c = textureLod(s, base + (fract(a) * tile), clamp(lod, 0.0, maps.splat.w));
    vec2 b = ((mat2(vec2(0.800000011920928955078125, 0.60000002384185791015625), vec2(-0.60000002384185791015625, 0.800000011920928955078125)) * a) * 0.2899999916553497314453125) + vec2(0.37000000476837158203125, 0.709999978542327880859375);
    vec4 d = textureLod(s, base + (fract(b) * tile), clamp(lod + (-1.78587520122528076171875), 0.0, maps.splat.w));
    return mix(c, d, vec4(vary));
}

vec4 splat_layer(sampler2D s, int i, vec2 uv, vec3 q, vec3 tri, vec2 gu[2], vec3 gq[2], float vary)
{
    if ((maps.flags.x & 4096u) == 0u)
    {
        int param = i;
        vec2 param_1 = uv;
        vec2 param_2 = gu[0];
        vec2 param_3 = gu[1];
        float param_4 = vary;
        return splat_fetch(s, param, param_1, param_2, param_3, param_4);
    }
    vec4 c = vec4(0.0);
    if (tri.x > 0.0199999995529651641845703125)
    {
        int param_5 = i;
        vec2 param_6 = q.yz;
        vec2 param_7 = gq[0].yz;
        vec2 param_8 = gq[1].yz;
        float param_9 = vary;
        c += (splat_fetch(s, param_5, param_6, param_7, param_8, param_9) * tri.x);
    }
    if (tri.y > 0.0199999995529651641845703125)
    {
        int param_10 = i;
        vec2 param_11 = q.xz;
        vec2 param_12 = gq[0].xz;
        vec2 param_13 = gq[1].xz;
        float param_14 = vary;
        c += (splat_fetch(s, param_10, param_11, param_12, param_13, param_14) * tri.y);
    }
    if (tri.z > 0.0199999995529651641845703125)
    {
        int param_15 = i;
        vec2 param_16 = q.xy;
        vec2 param_17 = gq[0].xy;
        vec2 param_18 = gq[1].xy;
        float param_19 = vary;
        c += (splat_fetch(s, param_15, param_16, param_17, param_18, param_19) * tri.z);
    }
    return c / vec4(max(dot(tri, step(vec3(0.0199999995529651641845703125), tri)), 9.9999997473787516355514526367188e-05));
}

vec3 perturb_normal(vec3 normal, inout vec3 ts, vec2 uv, vec2 duv1, vec2 duv2)
{
    float len = length(normal);
    if (len <= 9.9999999747524270787835121154785e-07)
    {
        return normal;
    }
    vec3 n = normal / vec3(len);
    ts.z = max(ts.z, 0.001000000047497451305389404296875);
    vec3 dp1 = dFdx(v_world);
    vec3 dp2 = dFdy(v_world);
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    float facing = (dot(n, cross(dp1, dp2)) < 0.0) ? (-1.0) : 1.0;
    vec3 tu = ((dp2perp * duv1.x) + (dp1perp * duv2.x)) * facing;
    vec3 tv = ((dp2perp * duv1.y) + (dp1perp * duv2.y)) * facing;
    float scale = max(dot(tu, tu), dot(tv, tv));
    if (scale <= 9.9999996826552253889678874634872e-21)
    {
        return normal;
    }
    scale = inversesqrt(scale);
    return normalize(((tu * (scale * ts.x)) - (tv * (scale * ts.y))) + (n * ts.z)) * len;
}

vec3 splat_surface(vec3 normal, inout vec3 bent)
{
    int count = min(int(maps.splat.x + 0.5), 8);
    float param[8];
    splat_read(param);
    float w[8] = param;
    vec2 gu[2];
    gu[0] = dFdx(v_uv);
    gu[1] = dFdy(v_uv);
    vec3 q = v_object * maps.params.z;
    vec3 gq[2];
    gq[0] = dFdx(q);
    gq[1] = dFdy(q);
    vec3 tri = vec3(0.0, 0.0, 1.0);
    if ((maps.flags.x & 4096u) != 0u)
    {
        vec3 vn = normalize(v_object_normal + vec3(9.9999999747524270787835121154785e-07));
        vec3 gn = cross(gq[0], gq[1]);
        float gl = length(gn);
        if (gl > 9.9999999600419720025001879548654e-13)
        {
            gn /= vec3(gl);
            if (abs(dot(gn, vn)) < 0.5)
            {
                vn = gn;
            }
        }
        vec3 an = abs(vn);
        tri = pow(an, vec3(max(maps.params.w, 1.0)));
        tri /= vec3(max((tri.x + tri.y) + tri.z, 9.9999999747524270787835121154785e-07));
    }
    vec2 _3823;
    if ((maps.flags.x & 4096u) != 0u)
    {
        _3823 = q.xy + vec2(q.z);
    }
    else
    {
        _3823 = v_uv;
    }
    int param_1 = 0;
    vec2 macro_p = (_3823 * 0.0430000014603137969970703125) * max(splat_tiling(param_1), 1.0);
    vec2 param_2 = macro_p * 3.099999904632568359375;
    float vary = clamp((splat_noise(param_2) * 1.2000000476837158203125) - 0.25, 0.0, 0.75);
    vec2 param_3 = (macro_p * 7.30000019073486328125) + vec2(11.0);
    float shade_k = mix(0.86000001430511474609375, 1.12000000476837158203125, splat_noise(param_3));
    float total = 0.0;
    for (int i = 0; i < 8; i++)
    {
        if (i >= count)
        {
            w[i] = 0.0;
        }
        total += w[i];
    }
    if (total <= 9.9999997473787516355514526367188e-06)
    {
        w[0] = 1.0;
        total = 1.0;
    }
    float best = -1000000000.0;
    vec4 col[8];
    float score[8];
    for (int i_1 = 0; i_1 < 8; i_1++)
    {
        col[i_1] = vec4(0.0);
        score[i_1] = -1000000000.0;
        w[i_1] /= total;
        bool _3917 = i_1 >= count;
        bool _3926;
        if (!_3917)
        {
            _3926 = w[i_1] <= 0.00200000009499490261077880859375;
        }
        else
        {
            _3926 = _3917;
        }
        if (_3926)
        {
            continue;
        }
        int param_4 = i_1;
        vec2 param_5 = v_uv;
        vec3 param_6 = q;
        vec3 param_7 = tri;
        vec2 param_8[2] = gu;
        vec3 param_9[2] = gq;
)fizmo_glsl",
R"fizmo_glsl(        float param_10 = vary;
        col[i_1] = splat_layer(splat_albedo, param_4, param_5, param_6, param_7, param_8, param_9, param_10);
        score[i_1] = w[i_1] + ((col[i_1].w * w[i_1]) * 0.5);
        best = max(best, score[i_1]);
    }
    float depth = max(maps.splat.y, 0.001000000047497451305389404296875);
    float sum = 0.0;
    vec3 rgb = vec3(0.0);
    float bw[8];
    for (int i_2 = 0; i_2 < 8; i_2++)
    {
        bw[i_2] = max(score[i_2] - (best - depth), 0.0);
        sum += bw[i_2];
        rgb += ((col[i_2].xyz * maps.tints[i_2].xyz) * bw[i_2]);
    }
    rgb = (rgb / vec3(max(sum, 9.9999997473787516355514526367188e-06))) * shade_k;
    bent = normal;
    bool _4028 = (maps.flags.x & 2048u) != 0u;
    bool _4034;
    if (_4028)
    {
        _4034 = length(normal) > 9.9999999747524270787835121154785e-07;
    }
    else
    {
        _4034 = _4028;
    }
    if (_4034)
    {
        vec3 ts = vec3(0.0);
        for (int i_3 = 0; i_3 < 8; i_3++)
        {
            if (bw[i_3] <= 0.0)
            {
                continue;
            }
            int param_11 = i_3;
            vec2 param_12 = v_uv;
            vec3 param_13 = q;
            vec3 param_14 = tri;
            vec2 param_15[2] = gu;
            vec3 param_16[2] = gq;
            float param_17 = vary;
            ts += (((splat_layer(splat_normals, param_11, param_12, param_13, param_14, param_15, param_16, param_17).xyz * 2.0) - vec3(1.0)) * (bw[i_3] / max(sum, 9.9999997473787516355514526367188e-06)));
        }
        vec3 param_18 = normal;
        vec3 param_19 = ts;
        vec2 param_20 = v_uv;
        vec2 param_21 = gu[0];
        vec2 param_22 = gu[1];
        vec3 _4096 = perturb_normal(param_18, param_19, param_20, param_21, param_22);
        bent = _4096;
    }
    return rgb;
}

float light_curve(float level)
{
    float k = max(scene.block_color.w, 1.0);
    return level / (k - ((k - 1.0) * level));
}

float sky_height(vec3 d)
{
    float a = abs(scene.glow.z);
    float s = (scene.glow.z < 0.0) ? (-1.0) : 1.0;
    if ((a > 0.5) && (a < 1.5))
    {
        return s * d.x;
    }
    if ((a > 1.5) && (a < 2.5))
    {
        return s * d.y;
    }
    if (a > 2.5)
    {
        return s * d.z;
    }
    return d.z;
}

float star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.98980045318603515625, 78.233001708984375, 37.71900177001953125))) * 43758.546875);
}

vec3 sky_radiance(vec3 dir, bool disks)
{
    vec3 param = dir;
    float h = sky_height(param);
    vec3 col = mix(scene.sky_horizon.xyz, scene.sky_zenith.xyz, vec3(pow(clamp(h, 0.0, 1.0), 0.449999988079071044921875)));
    if (h < 0.0)
    {
        col = mix(scene.sky_horizon.xyz, scene.sky_horizon.xyz * 0.3499999940395355224609375, vec3(clamp((-h) * 3.0, 0.0, 1.0)));
    }
    float cs = max(dot(dir, scene.sky_sun.xyz), 0.0);
    col += ((scene.sky_glow.xyz * scene.sky_glow.w) * ((pow(cs, scene.glow.x) * 0.5) + pow(cs, scene.sun_disk.w)));
    if (disks && (h > (-0.0199999995529651641845703125)))
    {
        col += (scene.sun_disk.xyz * smoothstep(scene.sky_sun.w - ((1.0 - scene.sky_sun.w) * 0.300000011920928955078125), scene.sky_sun.w, cs));
        float cm = dot(dir, scene.sky_moon.xyz);
        col += ((vec3(0.85000002384185791015625, 0.89999997615814208984375, 1.0) * scene.gloss.w) * smoothstep(scene.sky_moon.w - ((1.0 - scene.sky_moon.w) * 0.300000011920928955078125), scene.sky_moon.w, cm));
        vec3 param_1 = floor(dir * 400.0);
        float star = step(0.99849998950958251953125, star_hash(param_1));
        col += vec3((star * scene.sky_zenith.w) * smoothstep(0.0, 0.100000001490116119384765625, h));
    }
    return col;
}

vec3 fresnel(float c, vec3 f0)
{
    return f0 + ((vec3(1.0) - f0) * pow(1.0 - clamp(c, 0.0, 1.0), 5.0));
}

float ggx_d(float ndh, float a)
{
    float a2 = a * a;
    float d = ((ndh * ndh) * (a2 - 1.0)) + 1.0;
    return a2 / max((3.1415927410125732421875 * d) * d, 9.9999999747524270787835121154785e-07);
}

float ggx_g(float ndv, float ndl, float r)
{
    float k = ((r + 1.0) * (r + 1.0)) / 8.0;
    return (ndv / ((ndv * (1.0 - k)) + k)) * (ndl / ((ndl * (1.0 - k)) + k));
}

vec3 pbr_direct(vec3 n, vec3 v, vec3 l, vec3 diffuse, vec3 f0, float r)
{
    float ndl = max(dot(n, l), 0.0);
    if (ndl <= 0.0)
    {
        return vec3(0.0);
    }
    float ndv = max(dot(n, v), 9.9999997473787516355514526367188e-05);
    vec3 h = normalize(v + l);
    float param = max(dot(v, h), 0.0);
    vec3 param_1 = f0;
    vec3 f = fresnel(param, param_1);
    float param_2 = max(dot(n, h), 0.0);
    float param_3 = r * r;
    float param_4 = ndv;
    float param_5 = ndl;
    float param_6 = r;
    vec3 spec = (f * (ggx_d(param_2, param_3) * ggx_g(param_4, param_5, param_6))) / vec3(max((4.0 * ndv) * ndl, 9.9999997473787516355514526367188e-05));
    return (((vec3(1.0) - f) * diffuse) + (spec * 3.1415927410125732421875)) * ndl;
}

bool reflection_pass()
{
    return (scene.counts.y & 2048) != 0;
}

float shadow_noise(sampler2DShadow map, vec2 uv)
{
    vec2 cell = floor((uv * vec2(textureSize(map, 0))) * 4.0);
    return fract(52.98291778564453125 * fract(dot(cell, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

vec2 vogel(int i, int count, float spin)
{
    float r = sqrt((float(i) + 0.5) / float(count));
    float a = (float(i) * 2.3999631404876708984375) + spin;
    return vec2(cos(a), sin(a)) * r;
}

float sun_soft(sampler2DShadow map, sampler2D depth, vec2 uv, float ref)
{
    vec2 param = uv;
    float spin = (shadow_noise(map, param) * 2.0) * 3.1415927410125732421875;
    float search = scene.soft.y;
    float blockers = 0.0;
    float depth_sum = 0.0;
    for (int i = 0; i < 16; i++)
    {
        int param_1 = i;
        int param_2 = 16;
        float param_3 = spin;
        float d = texture(depth, uv + (vogel(param_1, param_2, param_3) * search)).x;
        if (d < ref)
        {
            blockers += 1.0;
            depth_sum += d;
        }
    }
    if (blockers == 0.0)
    {
        return 1.0;
    }
    if (blockers == 16.0)
    {
        return 0.0;
    }
    float radius = clamp((ref - (depth_sum / blockers)) * scene.soft.x, scene.soft.w, scene.soft.y);
    int side = max(int(scene.soft.z), 1);
    int taps = side * side;
    float lit = 0.0;
    for (int i_1 = 0; i_1 < taps; i_1++)
    {
        int param_4 = i_1;
        int param_5 = taps;
        float param_6 = spin;
        vec3 _617 = vec3(uv + (vogel(param_4, param_5, param_6) * radius), ref);
        lit += texture(map, vec3(_617.xy, _617.z));
    }
    return lit / float(taps);
}

float sun_grid(sampler2DShadow map, vec2 uv, float ref)
{
    float texel = scene.sun_shadow.z;
    float lit = 0.0;
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            vec3 _476 = vec3(uv + ((vec2(float(x), float(y)) - vec2(1.5)) * texel), ref);
            lit += texture(map, vec3(_476.xy, _476.z));
        }
    }
    return lit / 16.0;
}

float sun_sample(mat4 m, sampler2DShadow map, sampler2D depth, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    vec2 edge = abs(c.xy);
    float reach = max(edge.x, edge.y);
    bool _649 = reach >= 1.0;
    bool _656;
    if (!_649)
    {
        _656 = c.z <= 0.0;
    }
    else
    {
        _656 = _649;
    }
    bool _663;
    if (!_656)
    {
        _663 = c.z >= 1.0;
    }
    else
    {
        _663 = _656;
    }
    if (_663)
    {
        return 1.0;
    }
    vec2 uv = (c.xy * 0.5) + vec2(0.5);
    float ref = c.z - scene.sun_shadow.x;
    float _681;
    if (reflection_pass())
    {
        vec3 _689 = vec3(uv, ref);
        _681 = texture(map, vec3(_689.xy, _689.z));
    }
    else
    {
        float _698;
        if ((scene.counts.y & 512) != 0)
        {
            vec2 param = uv;
            float param_1 = ref;
            _698 = sun_soft(map, depth, param, param_1);
        }
        else
        {
            vec2 param_2 = uv;
            float param_3 = ref;
            _698 = sun_grid(map, param_2, param_3);
        }
        _681 = _698;
    }
    float lit = _681;
    return mix(lit, 1.0, smoothstep(0.85000002384185791015625, 1.0, reach));
}

float capsule_lit(vec3 p, vec3 to_light, float reach, float spread, float source)
{
    int count = min(int(scene.capsules.x), 4);
    float lit = 1.0;
    vec3 ray = to_light * reach;
    float rr = dot(ray, ray);
    float _776;
    float _832;
    float _850;
    for (int i = 0; i < count; i++)
    {
        vec3 a = scene.capsule_a[i].xyz;
        float r = scene.capsule_a[i].w;
        vec3 axis = scene.capsule_b[i].xyz - a;
        vec3 w = p - a;
        float aa = dot(axis, axis);
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _776 = clamp(dot(w, axis) / aa, 0.0, 1.0);
        }
        else
        {
            _776 = 0.0;
        }
        float own = _776;
        vec3 out_dir = w - (axis * own);
        bool _798 = length(out_dir) < (r + 0.0500000007450580596923828125);
        bool _805;
        if (_798)
        {
            _805 = dot(out_dir, to_light) >= 0.0;
        }
        else
        {
            _805 = _798;
        }
        if (_805)
        {
            continue;
        }
        float b = dot(ray, axis);
        float c = dot(ray, w);
        float f = dot(axis, w);
        float denom = (rr * aa) - (b * b);
        if (denom > 9.9999997473787516355514526367188e-05)
        {
            _832 = clamp(((b * f) - (c * aa)) / denom, 0.0, 1.0);
        }
        else
        {
            _832 = 0.0;
        }
        float s = _832;
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _850 = ((b * s) + f) / aa;
        }
        else
        {
            _850 = 0.0;
        }
        float t = _850;
        if (t < 0.0)
        {
            t = 0.0;
            s = clamp((-c) / rr, 0.0, 1.0);
        }
        else
        {
            if (t > 1.0)
            {
                t = 1.0;
                s = clamp((b - c) / rr, 0.0, 1.0);
            }
        }
        float along = s * reach;
        float gap = length((w + (ray * s)) - (axis * t)) - r;
        float blur = max(along * (spread + (source / max(reach - along, 9.9999997473787516355514526367188e-05))), 0.0199999995529651641845703125);
        lit = min(lit, smoothstep(-blur, blur, gap));
    }
    return mix(1.0, lit, scene.capsules.w);
}

float sun_visibility(vec3 world, vec3 n)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    vec3 p = world + (n * scene.sun_shadow.y);
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_sample(param, sun_map, sun_depth, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_sample(param_2, sun_map_b, sun_depth_b, param_3), scene.sun_mix.x);
    }
    vec3 param_4 = world;
    vec3 param_5 = -scene.sun_dir.xyz;
    float param_6 = 128.0;
    float param_7 = scene.capsules.y;
    float param_8 = 0.0;
    return mix(1.0, lit, scene.sun_shadow.w) * capsule_lit(param_4, param_5, param_6, param_7, param_8);
}

vec3 widen_to_sphere(vec3 to_point, vec3 r, float radius)
{
    vec3 to_ray = (r * dot(to_point, r)) - to_point;
    float len = length(to_ray);
    return to_point + (to_ray * clamp(radius / max(len, 9.9999999747524270787835121154785e-07), 0.0, 1.0));
}

vec3 segment_closest(vec3 a, vec3 b, vec3 p)
{
    vec3 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 9.9999999392252902907785028219223e-09), 0.0, 1.0);
    return a + (ab * t);
}

vec3 light_anchor(int i, vec3 world, vec3 r, bool use_ray)
{
    vec3 c = scene.point_pos[i].xyz;
    vec4 shape = scene.point_shape[i];
    int kind = int(shape.w + 0.5);
    if (kind == 0)
    {
        return c;
    }
    vec4 size = scene.point_size[i];
    if (kind == 1)
    {
        if (!use_ray)
        {
            return c;
)fizmo_glsl",
R"fizmo_glsl(        }
        vec3 param = c - world;
        vec3 param_1 = r;
        float param_2 = size.x;
        return world + widen_to_sphere(param, param_1, param_2);
    }
    if (kind == 2)
    {
        vec3 a = c - (shape.xyz * size.y);
        vec3 b = c + (shape.xyz * size.y);
        if (!use_ray)
        {
            vec3 param_3 = a;
            vec3 param_4 = b;
            vec3 param_5 = world;
            return segment_closest(param_3, param_4, param_5);
        }
        vec3 l0 = a - world;
        vec3 ld = b - a;
        float rd = dot(r, ld);
        float t = clamp(((dot(r, l0) * rd) - dot(l0, ld)) / max(dot(ld, ld) - (rd * rd), 9.9999999747524270787835121154785e-07), 0.0, 1.0);
        vec3 param_6 = l0 + (ld * t);
        vec3 param_7 = r;
        float param_8 = size.x;
        return world + widen_to_sphere(param_6, param_7, param_8);
    }
    vec3 nrm = scene.point_normal[i].xyz;
    vec3 p = world;
    if (use_ray)
    {
        float denom = dot(r, nrm);
        if (abs(denom) > 9.9999997473787516355514526367188e-05)
        {
            float t_1 = dot(c - world, nrm) / denom;
            if (t_1 > 0.0)
            {
                p = world + (r * t_1);
            }
        }
    }
    vec3 local = p - c;
    local -= (nrm * dot(local, nrm));
    if (kind == 3)
    {
        vec3 tangent = shape.xyz;
        vec3 bitangent = cross(nrm, tangent);
        return (c + (tangent * clamp(dot(local, tangent), -size.x, size.x))) + (bitangent * clamp(dot(local, bitangent), -size.y, size.y));
    }
    float len = length(local);
    float _1290;
    if (len > size.x)
    {
        _1290 = size.x / len;
    }
    else
    {
        _1290 = 1.0;
    }
    return c + (local * _1290);
}

float light_reach(int i, float dist, float radius)
{
    float ref = scene.point_size[i].w;
    float x = dist / max(radius, 9.9999997473787516355514526367188e-05);
    if (ref <= 0.0)
    {
        float f = 1.0 - x;
        return f * f;
    }
    float w = clamp(1.0 - (((x * x) * x) * x), 0.0, 1.0);
    return (((w * w) * ref) * ref) / ((dist * dist) + (ref * ref));
}

float light_wrap(int i, float ndl)
{
    float s = max(scene.point_normal[i].w, 0.0);
    return clamp((ndl + s) / (1.0 + s), 0.0, 1.0);
}

float spot_factor(vec4 spot, vec3 dir)
{
    if (spot.w < (-2.0))
    {
        return 1.0;
    }
    float sharp = length(spot.xyz);
    return clamp((dot(dir, spot.xyz / vec3(sharp)) - spot.w) * sharp, 0.0, 1.0);
}

float light_face(int i, vec3 dir)
{
    int kind = int(scene.point_shape[i].w + 0.5);
    if (kind < 3)
    {
        return 1.0;
    }
    float f = dot(scene.point_normal[i].xyz, dir);
    float _1325;
    if (scene.point_size[i].z > 0.5)
    {
        _1325 = abs(f);
    }
    else
    {
        _1325 = max(f, 0.0);
    }
    return _1325;
}

float point_tap(int slot, vec4 coord)
{
    if (slot == 0)
    {
        return texture(point_maps[0], vec4(coord.xyz, coord.w));
    }
    if (slot == 1)
    {
        return texture(point_maps[1], vec4(coord.xyz, coord.w));
    }
    if (slot == 2)
    {
        return texture(point_maps[2], vec4(coord.xyz, coord.w));
    }
    if (slot == 3)
    {
        return texture(point_maps[3], vec4(coord.xyz, coord.w));
    }
    if (slot == 4)
    {
        return texture(point_maps[4], vec4(coord.xyz, coord.w));
    }
    if (slot == 5)
    {
        return texture(point_maps[5], vec4(coord.xyz, coord.w));
    }
    if (slot == 6)
    {
        return texture(point_maps[6], vec4(coord.xyz, coord.w));
    }
    return texture(point_maps[7], vec4(coord.xyz, coord.w));
}

float point_visibility(int slot, vec3 light, vec3 d, float radius)
{
    float dist = length(d);
    vec3 param = light + d;
    vec3 param_1 = (-d) / vec3(max(dist, 9.9999997473787516355514526367188e-05));
    float param_2 = dist;
    float param_3 = 0.0;
    float param_4 = scene.capsules.z;
    float body = capsule_lit(param, param_1, param_2, param_3, param_4);
    float ref = (dist - scene.point_shadow.x) / radius;
    float spread = dist * scene.point_shadow.w;
    if (reflection_pass())
    {
        int param_5 = slot;
        vec4 param_6 = vec4(d, ref);
        return mix(1.0, point_tap(param_5, param_6), scene.point_shadow.y) * body;
    }
    float lit = 0.0;
    for (int i = 0; i < 8; i++)
    {
        int param_7 = slot;
        vec4 param_8 = vec4(d + (_1494[i] * spread), ref);
        lit += point_tap(param_7, param_8);
    }
    return mix(1.0, lit / 8.0, scene.point_shadow.y) * body;
}

float ao_luma(vec3 c)
{
    return dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
}

vec3 pbr_shade(vec3 world, vec3 normal, vec4 baked, vec3 albedo, float metallic, float roughness)
{
    float len = length(normal);
    vec3 v = normalize(-world);
    vec3 _4352;
    if (len > 9.9999999747524270787835121154785e-07)
    {
        _4352 = normal / vec3(len);
    }
    else
    {
        _4352 = v;
    }
    vec3 n = _4352;
    if (dot(n, v) < 0.0)
    {
        n = -n;
    }
    float r = clamp(roughness, 0.039999999105930328369140625, 1.0);
    vec3 f0 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metallic));
    vec3 diffuse = albedo * (1.0 - metallic);
    float param = baked.w;
    float sky = light_curve(param);
    float param_1 = baked.x;
    float param_2 = baked.y;
    float param_3 = baked.z;
    vec3 block = vec3(light_curve(param_1), light_curve(param_2), light_curve(param_3));
    vec3 from_sky = clamp(scene.sky_color.xyz * sky, vec3(0.0), vec3(1.0));
    vec3 from_block = clamp(block * scene.block_color.xyz, vec3(0.0), vec3(1.0));
    vec3 ambient = max((vec3(1.0) - ((vec3(1.0) - from_sky) * (vec3(1.0) - from_block))) + vec3(scene.params.x), vec3(scene.sky_color.w));
    float ndv = max(dot(n, v), 0.0);
    vec3 fa = f0 + ((max(vec3(1.0 - r), f0) - f0) * pow(1.0 - ndv, 5.0));
    vec3 color = (diffuse * ambient) * (vec3(1.0) - fa);
    vec3 _4472;
    if ((scene.counts.y & 8) != 0)
    {
        vec3 param_4 = reflect(-v, n);
        bool param_5 = false;
        _4472 = sky_radiance(param_4, param_5);
    }
    else
    {
        _4472 = scene.sky_color.xyz;
    }
    vec3 env = _4472;
    color += ((((env * fa) * sky) * (1.0 - (r * 0.75))) + (((fa * from_block) * r) * 0.25));
    vec3 ambient_rgb = color;
    if (dot(scene.sun_color.xyz, vec3(1.0)) > 0.0)
    {
        vec3 l = -scene.sun_dir.xyz;
        if (dot(n, l) > 0.0)
        {
            float exposure = pow(baked.w, scene.sun_color.w);
            if (exposure > 0.0)
            {
                vec3 param_6 = n;
                vec3 param_7 = v;
                vec3 param_8 = l;
                vec3 param_9 = diffuse;
                vec3 param_10 = f0;
                float param_11 = r;
                vec3 param_12 = world;
                vec3 param_13 = n;
                color += (((pbr_direct(param_6, param_7, param_8, param_9, param_10, param_11) * scene.sun_color.xyz) * exposure) * sun_visibility(param_12, param_13));
            }
        }
    }
    int count = min(scene.counts.x, 64);
    for (int i = 0; i < count; i++)
    {
        int param_14 = i;
        vec3 param_15 = world;
        vec3 param_16 = reflect(-v, n);
        bool param_17 = true;
        vec3 d = world - light_anchor(param_14, param_15, param_16, param_17);
        float radius = scene.point_pos[i].w;
        float dist2 = dot(d, d);
        if (dist2 >= (radius * radius))
        {
            continue;
        }
        float dist = sqrt(dist2);
        int param_18 = i;
        float param_19 = dist;
        float param_20 = radius;
        float fall = light_reach(param_18, param_19, param_20);
        vec3 dir = d / vec3(max(dist, 9.9999997473787516355514526367188e-05));
        int param_21 = i;
        float param_22 = dot(n, -dir);
        float wrapped = light_wrap(param_21, param_22);
        if (wrapped <= 0.0)
        {
            continue;
        }
        vec4 param_23 = scene.point_spot[i];
        vec3 param_24 = normalize((world - scene.point_pos[i].xyz) + vec3(9.9999999747524270787835121154785e-07));
        int param_25 = i;
        vec3 param_26 = dir;
        float cone = spot_factor(param_23, param_24) * light_face(param_25, param_26);
        if (cone <= 0.0)
        {
            continue;
        }
        float vis = 1.0;
        int slot = int(scene.point_color[i].w);
        bool _4670 = slot >= 0;
        bool _4677;
        if (_4670)
        {
            _4677 = (scene.counts.y & 4) != 0;
        }
        else
        {
            _4677 = _4670;
        }
        if (_4677)
        {
            vec4 map = scene.point_map[slot];
            int param_27 = slot;
            vec3 param_28 = map.xyz;
            vec3 param_29 = (world + (n * scene.point_shadow.z)) - map.xyz;
            float param_30 = map.w;
            vis = point_visibility(param_27, param_28, param_29, param_30);
        }
        vec3 param_31 = n;
        vec3 param_32 = v;
        vec3 param_33 = -dir;
        vec3 param_34 = diffuse;
        vec3 param_35 = f0;
        float param_36 = r;
        color += (((pbr_direct(param_31, param_32, param_33, param_34, param_35, param_36) + (diffuse * max(wrapped - max(dot(n, -dir), 0.0), 0.0))) * scene.point_color[i].xyz) * ((fall * cone) * vis));
    }
    vec3 param_37 = ambient_rgb;
    vec3 param_38 = color;
    g_ao_frac = clamp(ao_luma(param_37) / max(ao_luma(param_38), 9.9999997473787516355514526367188e-05), 0.0, 1.0);
    return min(color, vec3(scene.params.y * 4.0));
}

vec2 project_uv(vec3 p, out float w)
{
    vec4 c = spvWorkaroundRowMajor(scene.view_proj) * vec4(p, 1.0);
    w = c.w;
    vec2 ndc = c.xy / vec2(max(c.w, 9.9999997473787516355514526367188e-06));
    return scene.screen.xy + (((ndc * 0.5) + vec2(0.5)) * scene.screen.zw);
}

bool on_screen(vec2 uv)
{
    bool _2697 = all(greaterThanEqual(uv, scene.screen.xy));
    bool _2710;
    if (_2697)
    {
        _2710 = all(lessThanEqual(uv, scene.screen.xy + scene.screen.zw));
    }
    else
    {
        _2710 = _2697;
    }
    return _2710;
}

vec3 scene_point(vec2 uv)
{
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, texture(scene_depth, uv).x, 1.0);
    return q.xyz / vec3(q.w);
}

vec4 screen_reflection(vec3 world, vec3 r)
{
    if ((scene.counts.y & 256) == 0)
    {
        return vec4(0.0);
    }
    float reach = scene.target.w;
    float near_k = clamp(1.0 - (length(world) / max(reach * 3.0, 0.001000000047497451305389404296875)), 0.300000011920928955078125, 1.0);
    int steps = max(int(scene.target.z * near_k), 4);
    float prev = 0.0;
    float param_1;
    float param_5;
    float param_8;
    for (int i = 0; i < steps; i++)
    {
        float a = float(i + 1) / float(steps);
        float t = (reach * a) * a;
        vec3 param = world + (r * t);
        vec2 _2810 = project_uv(param, param_1);
        float w = param_1;
        vec2 uv = _2810;
        bool _2813 = w <= 0.0500000007450580596923828125;
        bool _2821;
        if (!_2813)
        {
            vec2 param_2 = uv;
            _2821 = !on_screen(param_2);
        }
        else
        {
            _2821 = _2813;
        }
        if (_2821)
        {
            break;
        }
        float ray = length(world + (r * t));
        vec2 param_3 = uv;
        float hit = length(scene_point(param_3));
        bool _2839 = ray > hit;
        bool _2851;
        if (_2839)
        {
            _2851 = (ray - hit) < (max(t - prev, 0.5) * 1.5);
        }
        else
        {
            _2851 = _2839;
        }
        if (_2851)
        {
            float lo = prev;
            float hi = t;
            for (int k = 0; k < 5; k++)
            {
                float mid = 0.5 * (lo + hi);
                vec3 param_4 = world + (r * mid);
                vec2 _2879 = project_uv(param_4, param_5);
                w = param_5;
)fizmo_glsl",
R"fizmo_glsl(                vec2 m = _2879;
                vec2 param_6 = m;
                if (length(world + (r * mid)) > length(scene_point(param_6)))
                {
                    hi = mid;
                }
                else
                {
                    lo = mid;
                }
            }
            vec3 param_7 = world + (r * hi);
            vec2 _2906 = project_uv(param_7, param_8);
            w = param_8;
            uv = _2906;
            vec2 inner = min(uv - scene.screen.xy, (scene.screen.xy + scene.screen.zw) - uv) / scene.screen.zw;
            float edge = smoothstep(0.0, 0.07999999821186065673828125, min(inner.x, inner.y));
            float fade = edge * (1.0 - (a * a));
            return vec4(texture(scene_color, uv).xyz, fade);
        }
        prev = t;
    }
    return vec4(0.0);
}

vec3 tone(inout vec3 c)
{
    if ((scene.counts.y & 32) == 0)
    {
        return c;
    }
    c *= scene.sky_params.w;
    c = clamp((c * ((c * 2.5099999904632568359375) + vec3(0.02999999932944774627685546875))) / ((c * ((c * 2.4300000667572021484375) + vec3(0.589999973773956298828125))) + vec3(0.14000000059604644775390625)), vec3(0.0), vec3(1.0));
    float luma = dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return clamp(mix(vec3(luma), c, vec3(scene.medium_color.w)), vec3(0.0), vec3(1.0));
}

float noise_at(vec2 p)
{
    return fract(52.98291778564453125 * fract(dot(p, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

float screen_noise()
{
    vec2 param = gl_FragCoord.xy;
    return noise_at(param);
}

float sun_tap(mat4 m, sampler2DShadow map, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    bool _2169 = abs(c.x) >= 1.0;
    bool _2177;
    if (!_2169)
    {
        _2177 = abs(c.y) >= 1.0;
    }
    else
    {
        _2177 = _2169;
    }
    bool _2184;
    if (!_2177)
    {
        _2184 = c.z <= 0.0;
    }
    else
    {
        _2184 = _2177;
    }
    bool _2191;
    if (!_2184)
    {
        _2191 = c.z >= 1.0;
    }
    else
    {
        _2191 = _2184;
    }
    if (_2191)
    {
        return 1.0;
    }
    vec3 _2208 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_2208.xy, _2208.z));
}

float sun_lit_at(vec3 p)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_tap(param, sun_map, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_tap(param_2, sun_map_b, param_3), scene.sun_mix.x);
    }
    return lit;
}

float march_light(vec3 dir, float reach)
{
    int steps = max(int(scene.volume.w), 1);
    float bias = max(scene.shafts.y, 1.0);
    float jitter = screen_noise();
    float density = scene.volume.x;
    float lit = 0.0;
    for (int i = 0; i < steps; i++)
    {
        float a = (float(i) + mix(0.0199999995529651641845703125, 0.980000019073486328125, jitter)) / float(steps);
        float t = reach * pow(a, bias);
        float dt = ((reach * bias) * pow(a, bias - 1.0)) / float(steps);
        vec3 param = dir * t;
        lit += ((sun_lit_at(param) * exp((-density) * t)) * dt);
    }
    return lit;
}

float volume_lookup(vec3 dir, float reach)
{
    vec4 c = spvWorkaroundRowMajor(scene.volume_proj) * vec4(dir, 1.0);
    if (c.w <= 0.0500000007450580596923828125)
    {
        vec3 param = dir;
        float param_1 = reach;
        return march_light(param, param_1);
    }
    float slices = scene.volume_grid.z;
    float s = pow(clamp(reach / scene.volume.z, 0.0, 1.0), 1.0 / max(scene.shafts.y, 1.0)) * slices;
    vec2 uv = ((c.xy / vec2(c.w)) * 0.5) + vec2(0.5);
    return textureLod(volume_light, vec3(uv, (max(s, 1.0) - 0.5) / slices), 0.0).x * min(s, 1.0);
}

float phase_hg(float g, float cs)
{
    return (1.0 - (g * g)) / (12.56637096405029296875 * pow(max((1.0 + (g * g)) - ((2.0 * g) * cs), 9.9999997473787516355514526367188e-05), 1.5));
}

vec3 inscatter(vec3 dir, float dist)
{
    float reach = min(dist, scene.volume.z);
    float _2419;
    if ((scene.counts.y & 8192) != 0)
    {
        vec3 param = dir;
        float param_1 = reach;
        _2419 = volume_lookup(param, param_1);
    }
    else
    {
        vec3 param_2 = dir;
        float param_3 = reach;
        _2419 = march_light(param_2, param_3);
    }
    float lit = _2419;
    float cs = dot(dir, -scene.sun_dir.xyz);
    float param_4 = 0.0;
    float param_5 = cs;
    float param_6 = scene.volume.y;
    float param_7 = cs;
    float phase = mix(phase_hg(param_4, param_5), phase_hg(param_6, param_7), 0.75);
    return (((scene.sun_color.xyz * lit) * scene.volume.x) * phase) * scene.shafts.x;
}

vec3 atmosphere(inout vec3 rgb, vec3 world)
{
    int f = scene.counts.y;
    float dist = length(world);
    vec3 dir = world / vec3(max(dist, 9.9999997473787516355514526367188e-05));
    if ((f & 64) != 0)
    {
        return mix(rgb, scene.medium_color.xyz, vec3(1.0 - exp((-dist) * scene.sky_params.z)));
    }
    if ((f & 8) != 0)
    {
        vec3 param = dir;
        bool param_1 = false;
        rgb = mix(rgb, sky_radiance(param, param_1), vec3(1.0 - exp((-max(dist - scene.sky_params.x, 0.0)) * scene.sky_horizon.w)));
    }
    if ((f & 16) != 0)
    {
        vec3 param_2 = dir;
        float param_3 = dist;
        rgb += inscatter(param_2, param_3);
    }
    return rgb;
}

vec3 finish(vec3 rgb, vec3 world, uint flags)
{
    if ((scene.counts.y & 1) == 0)
    {
        return rgb;
    }
    vec3 _2602;
    if ((flags & 64u) != 0u)
    {
        vec3 param = rgb;
        vec3 _2607 = tone(param);
        _2602 = _2607;
    }
    else
    {
        vec3 param_1 = rgb;
        vec3 param_2 = world;
        vec3 _2613 = atmosphere(param_1, param_2);
        vec3 param_3 = _2613;
        vec3 _2615 = tone(param_3);
        _2602 = _2615;
    }
    return _2602;
}

vec3 scene_light(vec3 world, vec3 n, vec4 baked, bool has_normal)
{
    float param = baked.w;
    float sky = light_curve(param);
    float param_1 = baked.x;
    float param_2 = baked.y;
    float param_3 = baked.z;
    vec3 block = vec3(light_curve(param_1), light_curve(param_2), light_curve(param_3));
    vec3 from_sky = clamp(scene.sky_color.xyz * sky, vec3(0.0), vec3(1.0));
    vec3 from_block = clamp(block * scene.block_color.xyz, vec3(0.0), vec3(1.0));
    vec3 total = (vec3(1.0) - ((vec3(1.0) - from_sky) * (vec3(1.0) - from_block))) + vec3(scene.params.x);
    vec3 ambient_part = total;
    if (dot(scene.sun_color.xyz, vec3(1.0)) > 0.0)
    {
        float _1657;
        if (has_normal)
        {
            _1657 = max(dot(n, -scene.sun_dir.xyz), 0.0);
        }
        else
        {
            _1657 = 1.0;
        }
        float ndl = _1657;
        if (ndl > 0.0)
        {
            float exposure = pow(baked.w, scene.sun_color.w);
            if (exposure > 0.0)
            {
                vec3 param_4 = world;
                vec3 param_5 = n;
                total += (((scene.sun_color.xyz * ndl) * exposure) * sun_visibility(param_4, param_5));
            }
        }
    }
    int count = min(scene.counts.x, 64);
    float _1759;
    for (int i = 0; i < count; i++)
    {
        int param_6 = i;
        vec3 param_7 = world;
        vec3 param_8 = n;
        bool param_9 = false;
        vec3 d = world - light_anchor(param_6, param_7, param_8, param_9);
        float radius = scene.point_pos[i].w;
        float dist2 = dot(d, d);
        if (dist2 >= (radius * radius))
        {
            continue;
        }
        float dist = sqrt(dist2);
        int param_10 = i;
        float param_11 = dist;
        float param_12 = radius;
        float fall = light_reach(param_10, param_11, param_12);
        vec3 dir = d / vec3(max(dist, 9.9999997473787516355514526367188e-05));
        if (has_normal)
        {
            int param_13 = i;
            float param_14 = dot(n, -dir);
            _1759 = light_wrap(param_13, param_14);
        }
        else
        {
            _1759 = 1.0;
        }
        float ndl_1 = _1759;
        if (ndl_1 <= 0.0)
        {
            continue;
        }
        vec4 param_15 = scene.point_spot[i];
        vec3 param_16 = normalize((world - scene.point_pos[i].xyz) + vec3(9.9999999747524270787835121154785e-07));
        int param_17 = i;
        vec3 param_18 = dir;
        float cone = spot_factor(param_15, param_16) * light_face(param_17, param_18);
        if (cone <= 0.0)
        {
            continue;
        }
        float vis = 1.0;
        int slot = int(scene.point_color[i].w);
        bool _1813 = slot >= 0;
        bool _1820;
        if (_1813)
        {
            _1820 = (scene.counts.y & 4) != 0;
        }
        else
        {
            _1820 = _1813;
        }
        if (_1820)
        {
            vec4 map = scene.point_map[slot];
            int param_19 = slot;
            vec3 param_20 = map.xyz;
            vec3 param_21 = (world + (n * scene.point_shadow.z)) - map.xyz;
            float param_22 = map.w;
            vis = point_visibility(param_19, param_20, param_21, param_22);
        }
        total += (scene.point_color[i].xyz * (((fall * ndl_1) * cone) * vis));
    }
    vec3 lit = clamp(max(total, vec3(scene.sky_color.w)), vec3(0.0), vec3(scene.params.y));
    vec3 param_23 = min(max(ambient_part, vec3(scene.sky_color.w)), lit);
    vec3 param_24 = lit;
    g_ao_frac = clamp(ao_luma(param_23) / max(ao_luma(param_24), 9.9999997473787516355514526367188e-05), 0.0, 1.0);
    return lit;
}

vec3 shade(vec3 world, vec3 normal, vec4 baked, uint flags)
{
    g_ao_frac = 0.0;
    if ((flags & 1u) == 0u)
    {
        return vec3(1.0);
    }
    float len = length(normal);
    bool has_normal = len > 9.9999999747524270787835121154785e-07;
    vec3 _1906;
    if (has_normal)
    {
        _1906 = normal / vec3(len);
    }
    else
    {
        _1906 = vec3(0.0);
    }
    vec3 n = _1906;
    if ((scene.counts.y & 1) == 0)
    {
        bool _1923 = !has_normal;
        bool _1930;
        if (!_1923)
        {
            _1930 = (flags & 4u) == 0u;
        }
        else
        {
            _1930 = _1923;
        }
        if (_1930)
        {
            return vec3(1.0);
        }
        return vec3(scene.legacy.w + max(dot(n, -scene.legacy.xyz), 0.0));
    }
    vec3 param = world;
    vec3 param_1 = n;
    vec4 param_2 = baked;
    bool param_3 = has_normal;
    vec3 _1954 = scene_light(param, param_1, param_2, param_3);
    return _1954;
}

void shade_main()
{
    uint mf = maps.flags.x;
    vec4 _4767;
    if ((mf & 64u) != 0u)
    {
        vec4 param = maps.albedo_u;
        vec4 param_1 = maps.albedo_v;
        _4767 = map_sample(tex, param, param_1);
    }
    else
    {
        _4767 = texture(tex, v_uv);
    }
    vec4 t = _4767;
    if ((mf & 8u) != 0u)
    {
        vec4 param_2 = maps.opacity_u;
        vec4 param_3 = maps.opacity_v;
        t *= map_sample(opacity_map, param_2, param_3).x;
    }
    if ((mf & 128u) != 0u)
    {
        if ((t.w * v_color.w) < maps.params.y)
        {
            discard;
        }
        t = vec4(t.xyz / vec3(max(t.w, 9.9999997473787516355514526367188e-05)), 1.0);
    }
    vec3 _4832;
    if ((mf & 1u) != 0u)
    {
        vec3 param_4 = v_normal;
        _4832 = mapped_normal(param_4);
    }
    else
    {
        _4832 = v_normal;
    }
    vec3 normal = _4832;
    if ((mf & 512u) != 0u)
    {
        vec3 param_5 = normal;
        vec3 param_6;
        vec3 _4852 = splat_surface(param_5, param_6);
        vec3 bent = param_6;
        t = vec4(_4852, 1.0);
        normal = bent;
    }
    bool _4863 = (v_flags & 2048u) != 0u;
    bool _4870;
    if (_4863)
    {
        _4870 = (scene.counts.y & 1) != 0;
    }
    else
    {
        _4870 = _4863;
    }
    if (_4870)
    {
)fizmo_glsl",
R"fizmo_glsl(        vec4 params = unpackUnorm4x8(v_pbr.x);
        vec3 emissive = (unpackUnorm4x8(v_pbr.y).xyz * params.z) * 16.0;
        if ((mf & 2u) != 0u)
        {
            vec4 param_7 = maps.orm_u;
            vec4 param_8 = maps.orm_v;
            vec3 orm = map_sample(orm_map, param_7, param_8).xyz;
            if ((mf & 32u) != 0u)
            {
                params.w *= orm.x;
            }
            params.y *= orm.y;
            params.x *= orm.z;
        }
        if ((mf & 4u) != 0u)
        {
            vec4 param_9 = maps.emissive_u;
            vec4 param_10 = maps.emissive_v;
            emissive *= map_sample(emissive_map, param_9, param_10).xyz;
        }
        vec3 param_11 = v_world;
        vec3 param_12 = normal;
        vec4 param_13 = v_light;
        vec3 param_14 = t.xyz * v_color.xyz;
        float param_15 = params.x;
        float param_16 = params.y;
        vec3 _4964 = pbr_shade(param_11, param_12, param_13, param_14, param_15, param_16);
        vec3 lit = (_4964 * params.w) + (emissive * t.w);
        vec3 param_17 = lit;
        float _4977;
        if (ao_luma(param_17) > 9.9999997473787516355514526367188e-05)
        {
            vec3 param_18 = lit - (emissive * t.w);
            vec3 param_19 = lit;
            _4977 = clamp(ao_luma(param_18) / ao_luma(param_19), 0.0, 1.0);
        }
        else
        {
            _4977 = 0.0;
        }
        g_ao_frac *= _4977;
        float alpha = t.w * v_color.w;
        float mirror_k = unpackUnorm4x8(v_pbr.y).w;
        if (mirror_k > 0.0)
        {
            vec3 v = normalize(-v_world);
            float nl = length(normal);
            vec3 _5022;
            if (nl > 9.9999999747524270787835121154785e-07)
            {
                _5022 = normal / vec3(nl);
            }
            else
            {
                _5022 = v;
            }
            vec3 n = _5022;
            if (dot(n, v) < 0.0)
            {
                n = -n;
            }
            vec3 r = reflect(-v, n);
            float edge = pow(1.0 - max(dot(n, v), 0.0), 5.0);
            float k = clamp(mirror_k + (((1.0 - mirror_k) * edge) * mirror_k), 0.0, 1.0) * (1.0 - (params.y * 0.85000002384185791015625));
            vec4 _5072;
            if ((scene.counts.y & 128) != 0)
            {
                vec3 param_20 = v_world;
                vec3 param_21 = r;
                _5072 = screen_reflection(param_20, param_21);
            }
            else
            {
                _5072 = vec4(0.0);
            }
            vec4 hit = _5072;
            vec3 _5087;
            if ((scene.counts.y & 8) != 0)
            {
                vec3 param_22 = r;
                bool param_23 = false;
                _5087 = sky_radiance(param_22, param_23);
            }
            else
            {
                _5087 = scene.sky_color.xyz;
            }
            vec3 env = _5087;
            vec3 seen = mix(env, hit.xyz, vec3(hit.w)) * mix(vec3(1.0), t.xyz * v_color.xyz, vec3(params.x));
            vec3 param_24 = lit;
            vec3 param_25 = v_world;
            uint param_26 = v_flags;
            out_color = vec4(((finish(param_24, param_25, param_26) * alpha) * (1.0 - k)) + (seen * k), (alpha * (1.0 - k)) + k);
            return;
        }
        vec3 param_27 = lit;
        vec3 param_28 = v_world;
        uint param_29 = v_flags;
        out_color = vec4(finish(param_27, param_28, param_29) * alpha, alpha);
        return;
    }
    vec3 param_30 = v_world;
    vec3 param_31 = normal;
    vec4 param_32 = v_light;
    uint param_33 = v_flags;
    vec3 _5170 = shade(param_30, param_31, param_32, param_33);
    vec3 light = _5170;
    vec3 param_34 = (t.xyz * v_color.xyz) * light;
    vec3 param_35 = v_world;
    uint param_36 = v_flags;
    out_color = vec4(finish(param_34, param_35, param_36) * v_color.w, t.w * v_color.w);
}

float soft_fade()
{
    float d = length(v_world);
    float _4107;
    if (maps.soft.z > 0.0)
    {
        _4107 = clamp((d - maps.soft.y) * maps.soft.z, 0.0, 1.0);
    }
    else
    {
        _4107 = 1.0;
    }
    float k = _4107;
    bool _4122 = maps.soft.x > 0.0;
    bool _4130;
    if (_4122)
    {
        _4130 = (scene.counts.y & 128) != 0;
    }
    else
    {
        _4130 = _4122;
    }
    bool _4135;
    if (_4130)
    {
        _4135 = !reflection_pass();
    }
    else
    {
        _4135 = _4130;
    }
    if (_4135)
    {
        vec2 uv = gl_FragCoord.xy * scene.target.xy;
        float raw = texture(scene_depth, uv).x;
        if (raw < 1.0)
        {
            vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
            vec4 h = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, raw, 1.0);
            vec3 behind = h.xyz / vec3(h.w);
            float gap = length(behind - v_world) * ((raw >= gl_FragCoord.z) ? 1.0 : (-1.0));
            float f = clamp(gap * maps.soft.x, 0.0, 1.0);
            k *= ((f * f) * (3.0 - (2.0 * f)));
        }
    }
    return k;
}

float veil(vec3 world)
{
    int f = scene.counts.y;
    float dist = length(world);
    if ((f & 64) != 0)
    {
        return exp((-dist) * scene.sky_params.z);
    }
    if ((f & 8) != 0)
    {
        return exp((-max(dist - scene.sky_params.x, 0.0)) * scene.sky_horizon.w);
    }
    return 1.0;
}

void main()
{
    g_ao_frac = 0.0;
    g_ao_frac = 0.0;
    shade_main();
    if ((maps.flags.x & 256u) != 0u)
    {
        out_color *= soft_fade();
    }
    if ((scene.counts.y & 16384) != 0)
    {
        float _5217;
        if ((v_flags & 64u) != 0u)
        {
            _5217 = g_ao_frac;
        }
        else
        {
            vec3 param = v_world;
            _5217 = g_ao_frac * veil(param);
        }
        out_color.w = _5217;
    }
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kMesh3DFragBindings[] = {
    { "sun_map", ::fizmo::gpu::GlslResource::Texture, 1, 1, 1 },
    { "sun_depth", ::fizmo::gpu::GlslResource::Texture, 1, 5, 1 },
    { "sun_map_b", ::fizmo::gpu::GlslResource::Texture, 1, 6, 1 },
    { "sun_depth_b", ::fizmo::gpu::GlslResource::Texture, 1, 7, 1 },
    { "point_maps", ::fizmo::gpu::GlslResource::Texture, 1, 2, 8 },
    { "volume_light", ::fizmo::gpu::GlslResource::Texture, 1, 9, 1 },
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "scene_color", ::fizmo::gpu::GlslResource::Texture, 1, 3, 1 },
    { "normal_map", ::fizmo::gpu::GlslResource::Texture, 0, 2, 1 },
    { "splat_weights", ::fizmo::gpu::GlslResource::Texture, 0, 8, 1 },
    { "splat_weights_b", ::fizmo::gpu::GlslResource::Texture, 0, 9, 1 },
    { "splat_albedo", ::fizmo::gpu::GlslResource::Texture, 0, 6, 1 },
    { "splat_normals", ::fizmo::gpu::GlslResource::Texture, 0, 7, 1 },
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "opacity_map", ::fizmo::gpu::GlslResource::Texture, 0, 5, 1 },
    { "orm_map", ::fizmo::gpu::GlslResource::Texture, 0, 3, 1 },
    { "emissive_map", ::fizmo::gpu::GlslResource::Texture, 0, 4, 1 },
    { "planar_maps", ::fizmo::gpu::GlslResource::Texture, 1, 8, 2 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
    { "MaterialMaps", ::fizmo::gpu::GlslResource::UniformBlock, 0, 1, 1 },
};

inline ::fizmo::gpu::ShaderDesc mesh3d_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kMesh3DFrag, sizeof(::fizmo::graphics::detail::gfx::kMesh3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kMesh3DFrag;
    d.glsl_pieces   = sizeof(kMesh3DFrag) / sizeof(kMesh3DFrag[0]);
    d.glsl_version  = kMesh3DFragVersion;
    d.glsl_bindings = { kMesh3DFragBindings, 20 };
    d.name          = "mesh3d.frag";
    return d;
}

inline constexpr int kLine3DVertVersion = 410;

inline constexpr const char* kLine3DVert[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    layout(row_major) mat4 vp;
    vec4 viewport;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_other;
layout(location = 3) in float in_side;
layout(location = 4) in float in_width;
layout(location = 0) out vec4 v_color;
layout(location = 2) in vec4 in_color;

void main()
{
    vec4 a = pc.vp * vec4(in_pos, 1.0);
    vec4 b = pc.vp * vec4(in_other, 1.0);
    vec2 sa = (a.xy / vec2(a.w)) * pc.viewport.xy;
    vec2 sb = (b.xy / vec2(b.w)) * pc.viewport.xy;
    vec2 d = sb - sa;
    float len = length(d);
    vec2 _80;
    if (len > 9.9999999747524270787835121154785e-07)
    {
        _80 = d / vec2(len);
    }
    else
    {
        _80 = vec2(1.0, 0.0);
    }
    vec2 dir = _80;
    vec2 nrm = vec2(-dir.y, dir.x);
    vec2 off = (((nrm * in_side) - dir) * in_width) / pc.viewport.xy;
    gl_Position = a + vec4(off * a.w, 0.0, 0.0);
    gl_Position.z -= (1.9999999494757503271102905273438e-05 * gl_Position.w);
    v_color = in_color;
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kLine3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc line3d_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kLine3DVert, sizeof(::fizmo::graphics::detail::gfx::kLine3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kLine3DVert;
    d.glsl_pieces   = sizeof(kLine3DVert) / sizeof(kLine3DVert[0]);
    d.glsl_version  = kLine3DVertVersion;
    d.glsl_bindings = { kLine3DVertBindings, 1 };
    d.name          = "line3d.vert";
    return d;
}

inline constexpr int kLine3DFragVersion = 410;

inline constexpr const char* kLine3DFrag[] = {
R"fizmo_glsl(layout(location = 0) out vec4 out_color;
layout(location = 0) in vec4 v_color;

void main()
{
    out_color = vec4(v_color.xyz * v_color.w, v_color.w);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kLine3DFragBindings[] = {
    { nullptr, ::fizmo::gpu::GlslResource::Texture, 0, 0, 0 },
};

inline ::fizmo::gpu::ShaderDesc line3d_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kLine3DFrag, sizeof(::fizmo::graphics::detail::gfx::kLine3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kLine3DFrag;
    d.glsl_pieces   = sizeof(kLine3DFrag) / sizeof(kLine3DFrag[0]);
    d.glsl_version  = kLine3DFragVersion;
    d.glsl_bindings = { kLine3DFragBindings, 0 };
    d.name          = "line3d.frag";
    return d;
}

inline constexpr int kQuad3DVertVersion = 410;

inline constexpr const char* kQuad3DVert[] = {
R"fizmo_glsl(const vec3 _72[7] = vec3[](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0), vec3(0.0));

layout(std140) uniform Push
{
    layout(row_major) mat4 mvp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    ivec4 extra;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 2) in uint in_params;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec4 v_light;
layout(location = 3) in vec4 in_light;
layout(location = 3) flat out uint v_flags;
layout(location = 4) out vec4 v_color;
layout(location = 1) in vec4 in_color;
layout(location = 5) flat out uvec4 v_params;
layout(location = 6) flat out ivec3 v_origin;
layout(location = 7) out vec3 v_local;
layout(location = 8) out float v_shade;

void main()
{
    vec4 p = vec4(in_pos, 1.0);
    gl_Position = pc.mvp * p;
    uvec4 params = uvec4(in_params & 255u, (in_params >> uint(8)) & 255u, (in_params >> uint(16)) & 255u, in_params >> uint(24));
    vec3 f = _72[min(params.y, 6u)];
    v_world = vec3(dot(pc.model_x, p), dot(pc.model_y, p), dot(pc.model_z, p));
    v_normal = vec3(dot(pc.model_x.xyz, f), dot(pc.model_y.xyz, f), dot(pc.model_z.xyz, f));
    v_light = in_light;
    v_flags = ((((uint(pc.extra.w) | (((params.z & 1u) != 0u) ? 4u : 0u)) | (((params.z & 2u) != 0u) ? 8u : 0u)) | (((params.z & 4u) != 0u) ? 16u : 0u)) | (((params.z & 8u) != 0u) ? 32u : 0u)) | (((params.z & 16u) != 0u) ? 128u : 0u);
    v_color = in_color;
    v_params = params;
    v_origin = pc.extra.xyz;
    v_local = in_pos;
    v_shade = float(params.w);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kQuad3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc quad3d_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kQuad3DVert, sizeof(::fizmo::graphics::detail::gfx::kQuad3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kQuad3DVert;
    d.glsl_pieces   = sizeof(kQuad3DVert) / sizeof(kQuad3DVert[0]);
    d.glsl_version  = kQuad3DVertVersion;
    d.glsl_bindings = { kQuad3DVertBindings, 1 };
    d.name          = "quad3d.vert";
    return d;
}

inline constexpr int kQuad3DFragVersion = 410;

inline constexpr const char* kQuad3DFrag[] = {
R"fizmo_glsl(const vec3 _1530[8] = vec3[](vec3(1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(-1.0), vec3(1.0, -1.0, 1.0), vec3(-1.0, 1.0, 1.0));
const float _2704[3] = float[](0.0, 0.5, -0.699999988079071044921875);
const float _2742[3] = float[](1.0, 1.37000000476837158203125, 1.83000004291534423828125);
const float _2751[3] = float[](1.0, 0.4000000059604644775390625, 0.180000007152557373046875);
const vec3 _2827[4] = vec3[](vec3(1.0, 0.3499999940395355224609375, 1.0), vec3(-0.60000002384185791015625, 1.0, 1.7000000476837158203125), vec3(0.300000011920928955078125, -1.0, 2.900000095367431640625), vec3(-1.0, -0.20000000298023223876953125, 4.30000019073486328125));
const vec3 _3825[6] = vec3[](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0));

layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
} scene;

uniform sampler2D planar_maps[2];
uniform sampler2DShadow sun_map;
uniform sampler2D sun_depth;
uniform sampler2DShadow sun_map_b;
uniform sampler2D sun_depth_b;
uniform samplerCubeShadow point_maps[8];
uniform sampler3D volume_light;
uniform sampler2D scene_depth;
uniform sampler2D scene_color;

layout(location = 4) in vec4 v_color;
layout(location = 3) flat in uint v_flags;
layout(location = 5) flat in uvec4 v_params;
layout(location = 7) in vec3 v_local;
layout(location = 6) flat in ivec3 v_origin;
layout(location = 8) in float v_shade;
layout(location = 0) in vec3 v_world;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_light;
layout(location = 0) out vec4 out_color;
float g_ao_frac;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

float light_curve(float level)
{
    float k = max(scene.block_color.w, 1.0);
    return level / (k - ((k - 1.0) * level));
}

bool reflection_pass()
{
    return (scene.counts.y & 2048) != 0;
}

float shadow_noise(sampler2DShadow map, vec2 uv)
{
    vec2 cell = floor((uv * vec2(textureSize(map, 0))) * 4.0);
    return fract(52.98291778564453125 * fract(dot(cell, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

vec2 vogel(int i, int count, float spin)
{
    float r = sqrt((float(i) + 0.5) / float(count));
    float a = (float(i) * 2.3999631404876708984375) + spin;
    return vec2(cos(a), sin(a)) * r;
}

float sun_soft(sampler2DShadow map, sampler2D depth, vec2 uv, float ref)
{
    vec2 param = uv;
    float spin = (shadow_noise(map, param) * 2.0) * 3.1415927410125732421875;
    float search = scene.soft.y;
    float blockers = 0.0;
    float depth_sum = 0.0;
    for (int i = 0; i < 16; i++)
    {
        int param_1 = i;
        int param_2 = 16;
        float param_3 = spin;
        float d = texture(depth, uv + (vogel(param_1, param_2, param_3) * search)).x;
        if (d < ref)
        {
            blockers += 1.0;
            depth_sum += d;
        }
    }
    if (blockers == 0.0)
    {
        return 1.0;
    }
    if (blockers == 16.0)
    {
        return 0.0;
    }
    float radius = clamp((ref - (depth_sum / blockers)) * scene.soft.x, scene.soft.w, scene.soft.y);
    int side = max(int(scene.soft.z), 1);
    int taps = side * side;
    float lit = 0.0;
    for (int i_1 = 0; i_1 < taps; i_1++)
    {
        int param_4 = i_1;
        int param_5 = taps;
        float param_6 = spin;
        vec3 _656 = vec3(uv + (vogel(param_4, param_5, param_6) * radius), ref);
        lit += texture(map, vec3(_656.xy, _656.z));
    }
    return lit / float(taps);
}

float sun_grid(sampler2DShadow map, vec2 uv, float ref)
{
    float texel = scene.sun_shadow.z;
    float lit = 0.0;
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            vec3 _517 = vec3(uv + ((vec2(float(x), float(y)) - vec2(1.5)) * texel), ref);
            lit += texture(map, vec3(_517.xy, _517.z));
        }
    }
    return lit / 16.0;
}

float sun_sample(mat4 m, sampler2DShadow map, sampler2D depth, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    vec2 edge = abs(c.xy);
    float reach = max(edge.x, edge.y);
    bool _688 = reach >= 1.0;
    bool _695;
    if (!_688)
    {
        _695 = c.z <= 0.0;
    }
    else
    {
        _695 = _688;
    }
    bool _702;
    if (!_695)
    {
        _702 = c.z >= 1.0;
    }
    else
    {
        _702 = _695;
    }
    if (_702)
    {
        return 1.0;
    }
    vec2 uv = (c.xy * 0.5) + vec2(0.5);
    float ref = c.z - scene.sun_shadow.x;
    float _720;
    if (reflection_pass())
    {
        vec3 _728 = vec3(uv, ref);
        _720 = texture(map, vec3(_728.xy, _728.z));
    }
    else
    {
        float _737;
        if ((scene.counts.y & 512) != 0)
        {
            vec2 param = uv;
            float param_1 = ref;
            _737 = sun_soft(map, depth, param, param_1);
        }
        else
        {
            vec2 param_2 = uv;
            float param_3 = ref;
            _737 = sun_grid(map, param_2, param_3);
        }
        _720 = _737;
    }
    float lit = _720;
    return mix(lit, 1.0, smoothstep(0.85000002384185791015625, 1.0, reach));
}

float capsule_lit(vec3 p, vec3 to_light, float reach, float spread, float source)
{
    int count = min(int(scene.capsules.x), 4);
    float lit = 1.0;
    vec3 ray = to_light * reach;
    float rr = dot(ray, ray);
    float _814;
    float _869;
    float _887;
    for (int i = 0; i < count; i++)
    {
        vec3 a = scene.capsule_a[i].xyz;
        float r = scene.capsule_a[i].w;
        vec3 axis = scene.capsule_b[i].xyz - a;
        vec3 w = p - a;
        float aa = dot(axis, axis);
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _814 = clamp(dot(w, axis) / aa, 0.0, 1.0);
        }
        else
        {
            _814 = 0.0;
        }
        float own = _814;
        vec3 out_dir = w - (axis * own);
        bool _835 = length(out_dir) < (r + 0.0500000007450580596923828125);
        bool _842;
        if (_835)
        {
            _842 = dot(out_dir, to_light) >= 0.0;
        }
        else
        {
            _842 = _835;
        }
        if (_842)
        {
            continue;
        }
        float b = dot(ray, axis);
        float c = dot(ray, w);
        float f = dot(axis, w);
        float denom = (rr * aa) - (b * b);
        if (denom > 9.9999997473787516355514526367188e-05)
        {
            _869 = clamp(((b * f) - (c * aa)) / denom, 0.0, 1.0);
        }
        else
        {
            _869 = 0.0;
        }
        float s = _869;
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _887 = ((b * s) + f) / aa;
        }
        else
        {
            _887 = 0.0;
        }
        float t = _887;
        if (t < 0.0)
        {
            t = 0.0;
            s = clamp((-c) / rr, 0.0, 1.0);
        }
        else
        {
            if (t > 1.0)
            {
                t = 1.0;
                s = clamp((b - c) / rr, 0.0, 1.0);
            }
        }
        float along = s * reach;
        float gap = length((w + (ray * s)) - (axis * t)) - r;
        float blur = max(along * (spread + (source / max(reach - along, 9.9999997473787516355514526367188e-05))), 0.0199999995529651641845703125);
        lit = min(lit, smoothstep(-blur, blur, gap));
    }
    return mix(1.0, lit, scene.capsules.w);
}

float sun_visibility(vec3 world, vec3 n)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    vec3 p = world + (n * scene.sun_shadow.y);
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_sample(param, sun_map, sun_depth, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_sample(param_2, sun_map_b, sun_depth_b, param_3), scene.sun_mix.x);
    }
    vec3 param_4 = world;
    vec3 param_5 = -scene.sun_dir.xyz;
    float param_6 = 128.0;
    float param_7 = scene.capsules.y;
    float param_8 = 0.0;
    return mix(1.0, lit, scene.sun_shadow.w) * capsule_lit(param_4, param_5, param_6, param_7, param_8);
}

vec3 widen_to_sphere(vec3 to_point, vec3 r, float radius)
{
    vec3 to_ray = (r * dot(to_point, r)) - to_point;
    float len = length(to_ray);
    return to_point + (to_ray * clamp(radius / max(len, 9.9999999747524270787835121154785e-07), 0.0, 1.0));
}

vec3 segment_closest(vec3 a, vec3 b, vec3 p)
{
    vec3 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 9.9999999392252902907785028219223e-09), 0.0, 1.0);
    return a + (ab * t);
}

vec3 light_anchor(int i, vec3 world, vec3 r, bool use_ray)
{
    vec3 c = scene.point_pos[i].xyz;
    vec4 shape = scene.point_shape[i];
    int kind = int(shape.w + 0.5);
    if (kind == 0)
    {
        return c;
    }
    vec4 size = scene.point_size[i];
    if (kind == 1)
    {
        if (!use_ray)
        {
            return c;
        }
        vec3 param = c - world;
        vec3 param_1 = r;
        float param_2 = size.x;
        return world + widen_to_sphere(param, param_1, param_2);
    }
    if (kind == 2)
    {
        vec3 a = c - (shape.xyz * size.y);
        vec3 b = c + (shape.xyz * size.y);
        if (!use_ray)
        {
            vec3 param_3 = a;
            vec3 param_4 = b;
            vec3 param_5 = world;
            return segment_closest(param_3, param_4, param_5);
        }
        vec3 l0 = a - world;
        vec3 ld = b - a;
        float rd = dot(r, ld);
        float t = clamp(((dot(r, l0) * rd) - dot(l0, ld)) / max(dot(ld, ld) - (rd * rd), 9.9999999747524270787835121154785e-07), 0.0, 1.0);
        vec3 param_6 = l0 + (ld * t);
        vec3 param_7 = r;
        float param_8 = size.x;
        return world + widen_to_sphere(param_6, param_7, param_8);
    }
    vec3 nrm = scene.point_normal[i].xyz;
    vec3 p = world;
    if (use_ray)
    {
        float denom = dot(r, nrm);
        if (abs(denom) > 9.9999997473787516355514526367188e-05)
        {
            float t_1 = dot(c - world, nrm) / denom;
            if (t_1 > 0.0)
            {
                p = world + (r * t_1);
            }
        }
    }
    vec3 local = p - c;
    local -= (nrm * dot(local, nrm));
    if (kind == 3)
    {
        vec3 tangent = shape.xyz;
        vec3 bitangent = cross(nrm, tangent);
        return (c + (tangent * clamp(dot(local, tangent), -size.x, size.x))) + (bitangent * clamp(dot(local, bitangent), -size.y, size.y));
    }
    float len = length(local);
    float _1326;
    if (len > size.x)
    {
        _1326 = size.x / len;
    }
    else
    {
        _1326 = 1.0;
    }
    return c + (local * _1326);
}

float light_reach(int i, float dist, float radius)
{
    float ref = scene.point_size[i].w;
    float x = dist / max(radius, 9.9999997473787516355514526367188e-05);
    if (ref <= 0.0)
    {
        float f = 1.0 - x;
        return f * f;
    }
)fizmo_glsl",
R"fizmo_glsl(    float w = clamp(1.0 - (((x * x) * x) * x), 0.0, 1.0);
    return (((w * w) * ref) * ref) / ((dist * dist) + (ref * ref));
}

float light_wrap(int i, float ndl)
{
    float s = max(scene.point_normal[i].w, 0.0);
    return clamp((ndl + s) / (1.0 + s), 0.0, 1.0);
}

float spot_factor(vec4 spot, vec3 dir)
{
    if (spot.w < (-2.0))
    {
        return 1.0;
    }
    float sharp = length(spot.xyz);
    return clamp((dot(dir, spot.xyz / vec3(sharp)) - spot.w) * sharp, 0.0, 1.0);
}

float light_face(int i, vec3 dir)
{
    int kind = int(scene.point_shape[i].w + 0.5);
    if (kind < 3)
    {
        return 1.0;
    }
    float f = dot(scene.point_normal[i].xyz, dir);
    float _1361;
    if (scene.point_size[i].z > 0.5)
    {
        _1361 = abs(f);
    }
    else
    {
        _1361 = max(f, 0.0);
    }
    return _1361;
}

float point_tap(int slot, vec4 coord)
{
    if (slot == 0)
    {
        return texture(point_maps[0], vec4(coord.xyz, coord.w));
    }
    if (slot == 1)
    {
        return texture(point_maps[1], vec4(coord.xyz, coord.w));
    }
    if (slot == 2)
    {
        return texture(point_maps[2], vec4(coord.xyz, coord.w));
    }
    if (slot == 3)
    {
        return texture(point_maps[3], vec4(coord.xyz, coord.w));
    }
    if (slot == 4)
    {
        return texture(point_maps[4], vec4(coord.xyz, coord.w));
    }
    if (slot == 5)
    {
        return texture(point_maps[5], vec4(coord.xyz, coord.w));
    }
    if (slot == 6)
    {
        return texture(point_maps[6], vec4(coord.xyz, coord.w));
    }
    return texture(point_maps[7], vec4(coord.xyz, coord.w));
}

float point_visibility(int slot, vec3 light, vec3 d, float radius)
{
    float dist = length(d);
    vec3 param = light + d;
    vec3 param_1 = (-d) / vec3(max(dist, 9.9999997473787516355514526367188e-05));
    float param_2 = dist;
    float param_3 = 0.0;
    float param_4 = scene.capsules.z;
    float body = capsule_lit(param, param_1, param_2, param_3, param_4);
    float ref = (dist - scene.point_shadow.x) / radius;
    float spread = dist * scene.point_shadow.w;
    if (reflection_pass())
    {
        int param_5 = slot;
        vec4 param_6 = vec4(d, ref);
        return mix(1.0, point_tap(param_5, param_6), scene.point_shadow.y) * body;
    }
    float lit = 0.0;
    for (int i = 0; i < 8; i++)
    {
        int param_7 = slot;
        vec4 param_8 = vec4(d + (_1530[i] * spread), ref);
        lit += point_tap(param_7, param_8);
    }
    return mix(1.0, lit / 8.0, scene.point_shadow.y) * body;
}

float ao_luma(vec3 c)
{
    return dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
}

vec3 scene_light(vec3 world, vec3 n, vec4 baked, bool has_normal)
{
    float param = baked.w;
    float sky = light_curve(param);
    float param_1 = baked.x;
    float param_2 = baked.y;
    float param_3 = baked.z;
    vec3 block = vec3(light_curve(param_1), light_curve(param_2), light_curve(param_3));
    vec3 from_sky = clamp(scene.sky_color.xyz * sky, vec3(0.0), vec3(1.0));
    vec3 from_block = clamp(block * scene.block_color.xyz, vec3(0.0), vec3(1.0));
    vec3 total = (vec3(1.0) - ((vec3(1.0) - from_sky) * (vec3(1.0) - from_block))) + vec3(scene.params.x);
    vec3 ambient_part = total;
    if (dot(scene.sun_color.xyz, vec3(1.0)) > 0.0)
    {
        float _1693;
        if (has_normal)
        {
            _1693 = max(dot(n, -scene.sun_dir.xyz), 0.0);
        }
        else
        {
            _1693 = 1.0;
        }
        float ndl = _1693;
        if (ndl > 0.0)
        {
            float exposure = pow(baked.w, scene.sun_color.w);
            if (exposure > 0.0)
            {
                vec3 param_4 = world;
                vec3 param_5 = n;
                total += (((scene.sun_color.xyz * ndl) * exposure) * sun_visibility(param_4, param_5));
            }
        }
    }
    int count = min(scene.counts.x, 64);
    float _1795;
    for (int i = 0; i < count; i++)
    {
        int param_6 = i;
        vec3 param_7 = world;
        vec3 param_8 = n;
        bool param_9 = false;
        vec3 d = world - light_anchor(param_6, param_7, param_8, param_9);
        float radius = scene.point_pos[i].w;
        float dist2 = dot(d, d);
        if (dist2 >= (radius * radius))
        {
            continue;
        }
        float dist = sqrt(dist2);
        int param_10 = i;
        float param_11 = dist;
        float param_12 = radius;
        float fall = light_reach(param_10, param_11, param_12);
        vec3 dir = d / vec3(max(dist, 9.9999997473787516355514526367188e-05));
        if (has_normal)
        {
            int param_13 = i;
            float param_14 = dot(n, -dir);
            _1795 = light_wrap(param_13, param_14);
        }
        else
        {
            _1795 = 1.0;
        }
        float ndl_1 = _1795;
        if (ndl_1 <= 0.0)
        {
            continue;
        }
        vec4 param_15 = scene.point_spot[i];
        vec3 param_16 = normalize((world - scene.point_pos[i].xyz) + vec3(9.9999999747524270787835121154785e-07));
        int param_17 = i;
        vec3 param_18 = dir;
        float cone = spot_factor(param_15, param_16) * light_face(param_17, param_18);
        if (cone <= 0.0)
        {
            continue;
        }
        float vis = 1.0;
        int slot = int(scene.point_color[i].w);
        bool _1849 = slot >= 0;
        bool _1856;
        if (_1849)
        {
            _1856 = (scene.counts.y & 4) != 0;
        }
        else
        {
            _1856 = _1849;
        }
        if (_1856)
        {
            vec4 map = scene.point_map[slot];
            int param_19 = slot;
            vec3 param_20 = map.xyz;
            vec3 param_21 = (world + (n * scene.point_shadow.z)) - map.xyz;
            float param_22 = map.w;
            vis = point_visibility(param_19, param_20, param_21, param_22);
        }
        total += (scene.point_color[i].xyz * (((fall * ndl_1) * cone) * vis));
    }
    vec3 lit = clamp(max(total, vec3(scene.sky_color.w)), vec3(0.0), vec3(scene.params.y));
    vec3 param_23 = min(max(ambient_part, vec3(scene.sky_color.w)), lit);
    vec3 param_24 = lit;
    g_ao_frac = clamp(ao_luma(param_23) / max(ao_luma(param_24), 9.9999997473787516355514526367188e-05), 0.0, 1.0);
    return lit;
}

vec3 shade(vec3 world, vec3 normal, vec4 baked, uint flags)
{
    g_ao_frac = 0.0;
    if ((flags & 1u) == 0u)
    {
        return vec3(1.0);
    }
    float len = length(normal);
    bool has_normal = len > 9.9999999747524270787835121154785e-07;
    vec3 _1942;
    if (has_normal)
    {
        _1942 = normal / vec3(len);
    }
    else
    {
        _1942 = vec3(0.0);
    }
    vec3 n = _1942;
    if ((scene.counts.y & 1) == 0)
    {
        bool _1959 = !has_normal;
        bool _1966;
        if (!_1959)
        {
            _1966 = (flags & 4u) == 0u;
        }
        else
        {
            _1966 = _1959;
        }
        if (_1966)
        {
            return vec3(1.0);
        }
        return vec3(scene.legacy.w + max(dot(n, -scene.legacy.xyz), 0.0));
    }
    vec3 param = world;
    vec3 param_1 = n;
    vec4 param_2 = baked;
    bool param_3 = has_normal;
    vec3 _1990 = scene_light(param, param_1, param_2, param_3);
    return _1990;
}

vec2 flow_vector(uint flags, uint packed_flow)
{
    if ((flags & 128u) == 0u)
    {
        return vec2(0.0);
    }
    float angle = ((float(packed_flow >> uint(4)) / 16.0) * 2.0) * 3.1415927410125732421875;
    return vec2(cos(angle), sin(angle)) * (float(packed_flow & 15u) / 15.0);
}

vec3 tone(inout vec3 c)
{
    if ((scene.counts.y & 32) == 0)
    {
        return c;
    }
    c *= scene.sky_params.w;
    c = clamp((c * ((c * 2.5099999904632568359375) + vec3(0.02999999932944774627685546875))) / ((c * ((c * 2.4300000667572021484375) + vec3(0.589999973773956298828125))) + vec3(0.14000000059604644775390625)), vec3(0.0), vec3(1.0));
    float luma = dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return clamp(mix(vec3(luma), c, vec3(scene.medium_color.w)), vec3(0.0), vec3(1.0));
}

float sky_height(vec3 d)
{
    float a = abs(scene.glow.z);
    float s = (scene.glow.z < 0.0) ? (-1.0) : 1.0;
    if ((a > 0.5) && (a < 1.5))
    {
        return s * d.x;
    }
    if ((a > 1.5) && (a < 2.5))
    {
        return s * d.y;
    }
    if (a > 2.5)
    {
        return s * d.z;
    }
    return d.z;
}

float star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.98980045318603515625, 78.233001708984375, 37.71900177001953125))) * 43758.546875);
}

vec3 sky_radiance(vec3 dir, bool disks)
{
    vec3 param = dir;
    float h = sky_height(param);
    vec3 col = mix(scene.sky_horizon.xyz, scene.sky_zenith.xyz, vec3(pow(clamp(h, 0.0, 1.0), 0.449999988079071044921875)));
    if (h < 0.0)
    {
        col = mix(scene.sky_horizon.xyz, scene.sky_horizon.xyz * 0.3499999940395355224609375, vec3(clamp((-h) * 3.0, 0.0, 1.0)));
    }
    float cs = max(dot(dir, scene.sky_sun.xyz), 0.0);
    col += ((scene.sky_glow.xyz * scene.sky_glow.w) * ((pow(cs, scene.glow.x) * 0.5) + pow(cs, scene.sun_disk.w)));
    if (disks && (h > (-0.0199999995529651641845703125)))
    {
        col += (scene.sun_disk.xyz * smoothstep(scene.sky_sun.w - ((1.0 - scene.sky_sun.w) * 0.300000011920928955078125), scene.sky_sun.w, cs));
        float cm = dot(dir, scene.sky_moon.xyz);
        col += ((vec3(0.85000002384185791015625, 0.89999997615814208984375, 1.0) * scene.gloss.w) * smoothstep(scene.sky_moon.w - ((1.0 - scene.sky_moon.w) * 0.300000011920928955078125), scene.sky_moon.w, cm));
        vec3 param_1 = floor(dir * 400.0);
        float star = step(0.99849998950958251953125, star_hash(param_1));
        col += vec3((star * scene.sky_zenith.w) * smoothstep(0.0, 0.100000001490116119384765625, h));
    }
    return col;
}

float noise_at(vec2 p)
{
    return fract(52.98291778564453125 * fract(dot(p, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

float screen_noise()
{
    vec2 param = gl_FragCoord.xy;
    return noise_at(param);
}

float sun_tap(mat4 m, sampler2DShadow map, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    bool _2204 = abs(c.x) >= 1.0;
    bool _2212;
    if (!_2204)
    {
        _2212 = abs(c.y) >= 1.0;
    }
    else
    {
        _2212 = _2204;
    }
    bool _2219;
    if (!_2212)
    {
        _2219 = c.z <= 0.0;
    }
    else
    {
        _2219 = _2212;
    }
    bool _2226;
    if (!_2219)
    {
        _2226 = c.z >= 1.0;
    }
    else
    {
        _2226 = _2219;
    }
    if (_2226)
    {
        return 1.0;
    }
    vec3 _2243 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_2243.xy, _2243.z));
}

float sun_lit_at(vec3 p)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_tap(param, sun_map, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_tap(param_2, sun_map_b, param_3), scene.sun_mix.x);
    }
    return lit;
}

float march_light(vec3 dir, float reach)
{
    int steps = max(int(scene.volume.w), 1);
    float bias = max(scene.shafts.y, 1.0);
    float jitter = screen_noise();
    float density = scene.volume.x;
    float lit = 0.0;
    for (int i = 0; i < steps; i++)
    {
        float a = (float(i) + mix(0.0199999995529651641845703125, 0.980000019073486328125, jitter)) / float(steps);
        float t = reach * pow(a, bias);
        float dt = ((reach * bias) * pow(a, bias - 1.0)) / float(steps);
        vec3 param = dir * t;
        lit += ((sun_lit_at(param) * exp((-density) * t)) * dt);
    }
    return lit;
}

float volume_lookup(vec3 dir, float reach)
{
    vec4 c = spvWorkaroundRowMajor(scene.volume_proj) * vec4(dir, 1.0);
    if (c.w <= 0.0500000007450580596923828125)
    {
)fizmo_glsl",
R"fizmo_glsl(        vec3 param = dir;
        float param_1 = reach;
        return march_light(param, param_1);
    }
    float slices = scene.volume_grid.z;
    float s = pow(clamp(reach / scene.volume.z, 0.0, 1.0), 1.0 / max(scene.shafts.y, 1.0)) * slices;
    vec2 uv = ((c.xy / vec2(c.w)) * 0.5) + vec2(0.5);
    return textureLod(volume_light, vec3(uv, (max(s, 1.0) - 0.5) / slices), 0.0).x * min(s, 1.0);
}

float phase_hg(float g, float cs)
{
    return (1.0 - (g * g)) / (12.56637096405029296875 * pow(max((1.0 + (g * g)) - ((2.0 * g) * cs), 9.9999997473787516355514526367188e-05), 1.5));
}

vec3 inscatter(vec3 dir, float dist)
{
    float reach = min(dist, scene.volume.z);
    float _2454;
    if ((scene.counts.y & 8192) != 0)
    {
        vec3 param = dir;
        float param_1 = reach;
        _2454 = volume_lookup(param, param_1);
    }
    else
    {
        vec3 param_2 = dir;
        float param_3 = reach;
        _2454 = march_light(param_2, param_3);
    }
    float lit = _2454;
    float cs = dot(dir, -scene.sun_dir.xyz);
    float param_4 = 0.0;
    float param_5 = cs;
    float param_6 = scene.volume.y;
    float param_7 = cs;
    float phase = mix(phase_hg(param_4, param_5), phase_hg(param_6, param_7), 0.75);
    return (((scene.sun_color.xyz * lit) * scene.volume.x) * phase) * scene.shafts.x;
}

vec3 atmosphere(inout vec3 rgb, vec3 world)
{
    int f = scene.counts.y;
    float dist = length(world);
    vec3 dir = world / vec3(max(dist, 9.9999997473787516355514526367188e-05));
    if ((f & 64) != 0)
    {
        return mix(rgb, scene.medium_color.xyz, vec3(1.0 - exp((-dist) * scene.sky_params.z)));
    }
    if ((f & 8) != 0)
    {
        vec3 param = dir;
        bool param_1 = false;
        rgb = mix(rgb, sky_radiance(param, param_1), vec3(1.0 - exp((-max(dist - scene.sky_params.x, 0.0)) * scene.sky_horizon.w)));
    }
    if ((f & 16) != 0)
    {
        vec3 param_2 = dir;
        float param_3 = dist;
        rgb += inscatter(param_2, param_3);
    }
    return rgb;
}

vec3 finish(vec3 rgb, vec3 world, uint flags)
{
    if ((scene.counts.y & 1) == 0)
    {
        return rgb;
    }
    vec3 _2653;
    if ((flags & 64u) != 0u)
    {
        vec3 param = rgb;
        vec3 _2658 = tone(param);
        _2653 = _2658;
    }
    else
    {
        vec3 param_1 = rgb;
        vec3 param_2 = world;
        vec3 _2664 = atmosphere(param_1, param_2);
        vec3 param_3 = _2664;
        vec3 _2666 = tone(param_3);
        _2653 = _2666;
    }
    return _2653;
}

vec3 swell_normal(vec3 n, vec2 p, uint flags)
{
    float level = float((flags >> 8u) & 7u);
    if ((scene.swell.x <= 0.0) || (level <= 0.0))
    {
        return n;
    }
    vec2 base = scene.swell.zw;
    vec2 grad = vec2(0.0);
    for (int i = 0; i < 3; i++)
    {
        float c = cos(_2704[i]);
        float s = sin(_2704[i]);
        vec2 d = vec2((base.x * c) - (base.y * s), (base.x * s) + (base.y * c));
        float k = scene.swell.y * _2742[i];
        grad += (d * ((_2751[i] * k) * cos((k * dot(d, p)) + scene.swell_phase[i])));
    }
    grad *= ((scene.swell.x * level) / 7.0);
    return normalize(n + (vec3(-grad, 0.0) * sign(n.z)));
}

vec3 wave_normal(vec3 n, vec3 p)
{
    float t = scene.sky_params.y * scene.waves.z;
    vec2 grad = vec2(0.0);
    float amp = scene.waves.x;
    for (int i = 0; i < 4; i++)
    {
        vec2 d = normalize(_2827[i].xy);
        float k = _2827[i].z * scene.waves.y;
        grad += (d * ((amp * k) * cos((dot(d, p.xy) * k) + (t * _2827[i].z))));
        amp *= 0.550000011920928955078125;
    }
    return normalize(n + (vec3(-grad, 0.0) * sign(n.z)));
}

int planar_index(vec3 world, vec3 n)
{
    if ((scene.counts.y & 4096) == 0)
    {
        return -1;
    }
    int count = min(int(scene.plane_info.x), 2);
    vec2 uv = gl_FragCoord.xy * scene.target.xy;
    for (int i = 0; i < count; i++)
    {
        vec4 r = scene.plane_rects[i];
        bool _347 = any(lessThan(uv, r.xy));
        bool _356;
        if (!_347)
        {
            _356 = any(greaterThan(uv, r.zw));
        }
        else
        {
            _356 = _347;
        }
        if (_356)
        {
            continue;
        }
        bool _374 = abs(dot(scene.planes[i].xyz, world) - scene.planes[i].w) < 0.0500000007450580596923828125;
        bool _385;
        if (_374)
        {
            _385 = dot(n, scene.planes[i].xyz) > 0.89999997615814208984375;
        }
        else
        {
            _385 = _374;
        }
        if (_385)
        {
            return i;
        }
    }
    return -1;
}

vec3 planar_sample(int i, vec2 uv)
{
    if (i == 0)
    {
        return texture(planar_maps[0], uv).xyz;
    }
    return texture(planar_maps[1], uv).xyz;
}

vec2 project_uv(vec3 p, out float w)
{
    vec4 c = spvWorkaroundRowMajor(scene.view_proj) * vec4(p, 1.0);
    w = c.w;
    vec2 ndc = c.xy / vec2(max(c.w, 9.9999997473787516355514526367188e-06));
    return scene.screen.xy + (((ndc * 0.5) + vec2(0.5)) * scene.screen.zw);
}

bool on_screen(vec2 uv)
{
    bool _2961 = all(greaterThanEqual(uv, scene.screen.xy));
    bool _2974;
    if (_2961)
    {
        _2974 = all(lessThanEqual(uv, scene.screen.xy + scene.screen.zw));
    }
    else
    {
        _2974 = _2961;
    }
    return _2974;
}

vec3 scene_point(vec2 uv)
{
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, texture(scene_depth, uv).x, 1.0);
    return q.xyz / vec3(q.w);
}

vec4 screen_reflection(vec3 world, vec3 r)
{
    if ((scene.counts.y & 256) == 0)
    {
        return vec4(0.0);
    }
    float reach = scene.target.w;
    float near_k = clamp(1.0 - (length(world) / max(reach * 3.0, 0.001000000047497451305389404296875)), 0.300000011920928955078125, 1.0);
    int steps = max(int(scene.target.z * near_k), 4);
    float prev = 0.0;
    float param_1;
    float param_5;
    float param_8;
    for (int i = 0; i < steps; i++)
    {
        float a = float(i + 1) / float(steps);
        float t = (reach * a) * a;
        vec3 param = world + (r * t);
        vec2 _3073 = project_uv(param, param_1);
        float w = param_1;
        vec2 uv = _3073;
        bool _3076 = w <= 0.0500000007450580596923828125;
        bool _3084;
        if (!_3076)
        {
            vec2 param_2 = uv;
            _3084 = !on_screen(param_2);
        }
        else
        {
            _3084 = _3076;
        }
        if (_3084)
        {
            break;
        }
        float ray = length(world + (r * t));
        vec2 param_3 = uv;
        float hit = length(scene_point(param_3));
        bool _3102 = ray > hit;
        bool _3114;
        if (_3102)
        {
            _3114 = (ray - hit) < (max(t - prev, 0.5) * 1.5);
        }
        else
        {
            _3114 = _3102;
        }
        if (_3114)
        {
            float lo = prev;
            float hi = t;
            for (int k = 0; k < 5; k++)
            {
                float mid = 0.5 * (lo + hi);
                vec3 param_4 = world + (r * mid);
                vec2 _3142 = project_uv(param_4, param_5);
                w = param_5;
                vec2 m = _3142;
                vec2 param_6 = m;
                if (length(world + (r * mid)) > length(scene_point(param_6)))
                {
                    hi = mid;
                }
                else
                {
                    lo = mid;
                }
            }
            vec3 param_7 = world + (r * hi);
            vec2 _3169 = project_uv(param_7, param_8);
            w = param_8;
            uv = _3169;
            vec2 inner = min(uv - scene.screen.xy, (scene.screen.xy + scene.screen.zw) - uv) / scene.screen.zw;
            float edge = smoothstep(0.0, 0.07999999821186065673828125, min(inner.x, inner.y));
            float fade = edge * (1.0 - (a * a));
            return vec4(texture(scene_color, uv).xyz, fade);
        }
        prev = t;
    }
    return vec4(0.0);
}

float veil(vec3 world)
{
    int f = scene.counts.y;
    float dist = length(world);
    if ((f & 64) != 0)
    {
        return exp((-dist) * scene.sky_params.z);
    }
    if ((f & 8) != 0)
    {
        return exp((-max(dist - scene.sky_params.x, 0.0)) * scene.sky_horizon.w);
    }
    return 1.0;
}

vec3 finish(vec3 rgb, vec3 world)
{
    if ((scene.counts.y & 1) == 0)
    {
        return rgb;
    }
    vec3 param = rgb;
    vec3 param_1 = world;
    vec3 _2637 = atmosphere(param, param_1);
    vec3 param_2 = _2637;
    vec3 _2639 = tone(param_2);
    return _2639;
}

vec4 surface_output(vec3 albedo, vec3 light, float alpha, vec3 world, vec3 n, uint flags, vec3 wave_pos, vec2 flow)
{
    vec3 rgb = albedo * light;
    bool liquid = (flags & 16u) != 0u;
    bool glossy = (flags & 8u) != 0u;
    bool mirror = (flags & 32u) != 0u;
    bool _3275 = ((!liquid) && (!glossy)) && (!mirror);
    bool _3283;
    if (!_3275)
    {
        _3283 = (scene.counts.y & 1) == 0;
    }
    else
    {
        _3283 = _3275;
    }
    if (_3283)
    {
        vec3 param = rgb;
        vec3 param_1 = world;
        uint param_2 = flags;
        return vec4(finish(param, param_1, param_2) * alpha, alpha);
    }
    vec3 v = normalize(-world);
    vec3 _3310;
    if (dot(n, v) < 0.0)
    {
        _3310 = -n;
    }
    else
    {
        _3310 = n;
    }
    vec3 facing = _3310;
    vec3 drifted = wave_pos - vec3(((flow * scene.sky_params.y) * scene.waves.z) * 1.0, 0.0);
    bool _3340;
    if (liquid)
    {
        _3340 = abs(n.z) > 0.5;
    }
    else
    {
        _3340 = liquid;
    }
    vec3 _3341;
    if (_3340)
    {
        vec3 param_3 = facing;
        vec2 param_4 = world.xy;
        uint param_5 = flags;
        vec3 param_6 = swell_normal(param_3, param_4, param_5);
        vec3 param_7 = drifted;
        _3341 = wave_normal(param_6, param_7);
    }
    else
    {
        _3341 = facing;
    }
    vec3 sn = _3341;
    float f0 = liquid ? 0.0199999995529651641845703125 : 0.039999999105930328369140625;
    float fresnel = (f0 + ((1.0 - f0) * pow(1.0 - max(dot(sn, v), 0.0), 5.0))) * scene.waves.w;
    vec3 r = reflect(-v, sn);
    bool underwater = (scene.counts.y & 64) != 0;
    bool from_below = liquid && underwater;
    int _3395;
    if (from_below)
    {
        _3395 = -1;
    }
    else
    {
        vec3 param_8 = world;
        vec3 param_9 = facing;
        _3395 = planar_index(param_8, param_9);
    }
    int plane = _3395;
    vec2 screen_uv = gl_FragCoord.xy * scene.target.xy;
    vec4 _3414;
    if (from_below)
    {
        _3414 = vec4(0.0);
    }
    else
    {
        vec4 _3420;
        if (plane >= 0)
        {
            int param_10 = plane;
            vec2 param_11 = screen_uv + ((sn.xy - facing.xy) * scene.plane_info.y);
            _3420 = vec4(planar_sample(param_10, param_11), 1.0);
        }
        else
        {
            vec4 _3446;
            if (mirror || (fresnel > 0.02999999932944774627685546875))
            {
                vec3 param_12 = world;
                vec3 param_13 = r;
                _3446 = screen_reflection(param_12, param_13);
            }
            else
            {
                _3446 = vec4(0.0);
            }
            _3420 = _3446;
        }
        _3414 = _3420;
    }
    vec4 hit = _3414;
    vec3 _3460;
    if (from_below)
    {
        _3460 = scene.medium_color.xyz;
    }
    else
    {
        vec3 param_14 = r;
        bool param_15 = false;
        _3460 = sky_radiance(param_14, param_15) * (1.0 - hit.w);
    }
    vec3 sky = _3460;
    vec3 half_vec = normalize(v - scene.sun_dir.xyz);
    float _3485;
    if (from_below)
    {
        _3485 = 0.0;
    }
    else
    {
        vec3 param_16 = world + (facing * scene.sun_shadow.y);
        _3485 = (pow(max(dot(sn, half_vec), 0.0), scene.gloss.x) * scene.gloss.y) * sun_lit_at(param_16);
    }
    float spec = _3485;
    vec3 glare = scene.sun_color.xyz * spec;
    vec3 param_17 = world;
)fizmo_glsl",
R"fizmo_glsl(    float keep = veil(param_17);
    if (mirror)
    {
        vec3 param_18 = rgb * mix(1.0, 0.0599999986588954925537109375, hit.w);
        vec3 param_19 = world;
        vec3 own = finish(param_18, param_19);
        return vec4(own + ((((hit.xyz * hit.w) * albedo) + (glare * (1.0 - hit.w))) * keep), 1.0);
    }
    bool _3563;
    if (liquid)
    {
        _3563 = (scene.counts.y & 128) != 0;
    }
    else
    {
        _3563 = liquid;
    }
    bool _3570;
    if (_3563)
    {
        _3570 = scene.water.w > 0.0;
    }
    else
    {
        _3570 = _3563;
    }
    if (_3570)
    {
        vec2 uv0 = gl_FragCoord.xy * scene.target.xy;
        float surface = length(world);
        vec2 param_20 = uv0;
        float depth = max(length(scene_point(param_20)) - surface, 0.0);
        vec3 bend = refract(-v, sn, underwater ? 1.33000004291534423828125 : 0.75187969207763671875);
        if (dot(bend, bend) == 0.0)
        {
            fresnel = 1.0;
        }
        vec2 uv = uv0 + ((((sn.xy - facing.xy) * scene.water.x) * 0.02999999932944774627685546875) * min(depth, 2.0));
        vec2 param_21 = uv;
        bool _3624 = !on_screen(param_21);
        bool _3637;
        if (!_3624)
        {
            vec2 param_22 = uv;
            _3637 = dot(scene_point(param_22) - world, facing) > (-0.0500000007450580596923828125);
        }
        else
        {
            _3637 = _3624;
        }
        if (_3637)
        {
            uv = uv0;
        }
        float _3643;
        if (underwater)
        {
            _3643 = 0.0;
        }
        else
        {
            vec2 param_23 = uv;
            _3643 = max(length(scene_point(param_23)) - surface, 0.0);
        }
        float thick = _3643;
        vec3 through = exp((vec3(1.0) - albedo) * ((-thick) * scene.water.y));
        float murk = 1.0 - exp((-thick) * scene.water.z);
        vec3 behind = (texture(scene_color, uv).xyz * through) * (1.0 - murk);
        vec3 lit = (sky * fresnel) + ((rgb * murk) * (1.0 - fresnel));
        vec3 param_24 = lit;
        vec3 param_25 = world;
        vec3 color = finish(param_24, param_25) + ((((behind * (1.0 - fresnel)) + ((hit.xyz * hit.w) * fresnel)) + glare) * keep);
        return vec4(color, 1.0);
    }
    float cover = (alpha * (1.0 - fresnel)) + fresnel;
    vec3 straight = (((rgb * alpha) * (1.0 - fresnel)) + (sky * fresnel)) / vec3(max(cover, 9.9999997473787516355514526367188e-05));
    vec3 param_26 = straight;
    vec3 param_27 = world;
    vec3 color_1 = (finish(param_26, param_27) * cover) + ((((hit.xyz * hit.w) * fresnel) + glare) * keep);
    return vec4(color_1, cover);
}

void shade_main()
{
    vec3 rgb = floor((v_color.xyz * 255.0) + vec3(0.5));
    bool flowing = (v_flags & 128u) != 0u;
    uint _3791;
    if (flowing)
    {
        _3791 = 0u;
    }
    else
    {
        _3791 = v_params.x;
    }
    uint amount = _3791;
    bool _3802 = amount > 0u;
    bool _3809;
    if (_3802)
    {
        _3809 = v_params.y < 6u;
    }
    else
    {
        _3809 = _3802;
    }
    if (_3809)
    {
        ivec3 c = ivec3(floor(v_local - (_3825[v_params.y] * 0.5))) + v_origin;
        uint h = ((uint(c.x) * 374761393u) + (uint(c.y) * 668265263u)) + (uint(c.z) * 2147483647u);
        h = (h ^ (h >> 13u)) * 1274126177u;
        h ^= (h >> 16u);
        float delta = float(int(h % ((2u * amount) + 1u)) - int(amount));
        rgb = clamp(rgb + vec3(delta), vec3(0.0), vec3(255.0));
    }
    rgb = floor(((rgb * floor(v_shade + 0.5)) + vec3(127.0)) / vec3(255.0));
    vec3 param = v_world;
    vec3 param_1 = v_normal;
    vec4 param_2 = v_light;
    uint param_3 = v_flags;
    vec3 _3912 = shade(param, param_1, param_2, param_3);
    vec3 light = _3912;
    vec3 _3917;
    if (length(v_normal) > 9.9999999747524270787835121154785e-07)
    {
        _3917 = normalize(v_normal);
    }
    else
    {
        _3917 = vec3(0.0, 0.0, 1.0);
    }
    vec3 n = _3917;
    uint param_4 = v_flags;
    uint param_5 = v_params.x;
    vec3 param_6 = rgb / vec3(255.0);
    vec3 param_7 = light;
    float param_8 = v_color.w;
    vec3 param_9 = v_world;
    vec3 param_10 = n;
    uint param_11 = v_flags;
    vec3 param_12 = vec3(v_origin & ivec3(1023)) + v_local;
    vec2 param_13 = flow_vector(param_4, param_5);
    out_color = surface_output(param_6, param_7, param_8, param_9, param_10, param_11, param_12, param_13);
}

void main()
{
    g_ao_frac = 0.0;
    g_ao_frac = 0.0;
    shade_main();
    if ((scene.counts.y & 16384) != 0)
    {
        float _3968;
        if ((v_flags & 64u) != 0u)
        {
            _3968 = g_ao_frac;
        }
        else
        {
            vec3 param = v_world;
            _3968 = g_ao_frac * veil(param);
        }
        out_color.w = _3968;
    }
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kQuad3DFragBindings[] = {
    { "planar_maps", ::fizmo::gpu::GlslResource::Texture, 1, 8, 2 },
    { "sun_map", ::fizmo::gpu::GlslResource::Texture, 1, 1, 1 },
    { "sun_depth", ::fizmo::gpu::GlslResource::Texture, 1, 5, 1 },
    { "sun_map_b", ::fizmo::gpu::GlslResource::Texture, 1, 6, 1 },
    { "sun_depth_b", ::fizmo::gpu::GlslResource::Texture, 1, 7, 1 },
    { "point_maps", ::fizmo::gpu::GlslResource::Texture, 1, 2, 8 },
    { "volume_light", ::fizmo::gpu::GlslResource::Texture, 1, 9, 1 },
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "scene_color", ::fizmo::gpu::GlslResource::Texture, 1, 3, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc quad3d_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kQuad3DFrag, sizeof(::fizmo::graphics::detail::gfx::kQuad3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kQuad3DFrag;
    d.glsl_pieces   = sizeof(kQuad3DFrag) / sizeof(kQuad3DFrag[0]);
    d.glsl_version  = kQuad3DFragVersion;
    d.glsl_bindings = { kQuad3DFragBindings, 10 };
    d.name          = "quad3d.frag";
    return d;
}

inline constexpr int kBatch3DVertVersion = 410;

inline constexpr const char* kBatch3DVert[] = {
R"fizmo_glsl(const float _50[3] = float[](0.0, 0.5, -0.699999988079071044921875);
const float _92[3] = float[](1.0, 1.37000000476837158203125, 1.83000004291534423828125);
const float _111[3] = float[](1.0, 0.4000000059604644775390625, 0.180000007152557373046875);
const vec3 _204[7] = vec3[](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0), vec3(0.0));

layout(std140) uniform Push
{
    layout(row_major) mat4 vp;
    vec4 camera_frac;
    ivec4 extra;
    vec4 swell;
    vec4 swell_phase;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 6) in vec3 in_frac;
layout(location = 4) in ivec3 in_rel_cell;
layout(location = 2) in uint in_params;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec4 v_light;
layout(location = 3) in vec4 in_light;
layout(location = 3) flat out uint v_flags;
layout(location = 4) out vec4 v_color;
layout(location = 1) in vec4 in_color;
layout(location = 5) flat out uvec4 v_params;
layout(location = 6) flat out ivec3 v_origin;
layout(location = 5) in ivec3 in_abs_cell;
layout(location = 7) out vec3 v_local;
layout(location = 8) out float v_shade;

vec3 swell(vec2 p, float scale)
{
    vec2 base = pc.swell.zw;
    float h = 0.0;
    vec2 grad = vec2(0.0);
    for (int i = 0; i < 3; i++)
    {
        float c = cos(_50[i]);
        float s = sin(_50[i]);
        vec2 d = vec2((base.x * c) - (base.y * s), (base.x * s) + (base.y * c));
        float k = pc.swell.y * _92[i];
        float phase = (k * dot(d, p)) + pc.swell_phase[i];
        h += (_111[i] * sin(phase));
        grad += (d * ((_111[i] * k) * cos(phase)));
    }
    float a = pc.swell.x * scale;
    return vec3(grad * a, h * a);
}

void main()
{
    vec3 local = in_pos + in_frac;
    vec3 rel = vec3(in_rel_cell) + (local - pc.camera_frac.xyz);
    uvec4 params = uvec4(in_params & 255u, (in_params >> uint(8)) & 255u, (in_params >> uint(16)) & 255u, in_params >> uint(24));
    vec3 normal = _204[min(params.y, 6u)];
    uint level = (params.z >> 5u) & 7u;
    bool _227 = (params.z & 4u) != 0u;
    bool _233;
    if (_227)
    {
        _233 = params.y == 5u;
    }
    else
    {
        _233 = _227;
    }
    bool surface = _233;
    bool _236 = pc.swell.x > 0.0;
    bool _242;
    if (_236)
    {
        _242 = pc.swell_phase.w > 0.0;
    }
    else
    {
        _242 = _236;
    }
    if ((_242 && surface) && (level != 0u))
    {
        float fade = 1.0 - smoothstep(pc.swell_phase.w * 0.60000002384185791015625, pc.swell_phase.w, length(rel.xy));
        if (fade > 0.0)
        {
            vec2 param = rel.xy;
            float param_1 = float(level) / 7.0;
            rel.z += (swell(param, param_1).z * fade);
        }
    }
    gl_Position = pc.vp * vec4(rel, 1.0);
    v_world = rel;
    v_normal = normal;
    v_light = in_light;
    uint _348;
    if (surface)
    {
        _348 = level << 8u;
    }
    else
    {
        _348 = 0u;
    }
    v_flags = (((((uint(pc.extra.w) | (((params.z & 1u) != 0u) ? 4u : 0u)) | (((params.z & 2u) != 0u) ? 8u : 0u)) | (((params.z & 4u) != 0u) ? 16u : 0u)) | (((params.z & 8u) != 0u) ? 32u : 0u)) | (((params.z & 16u) != 0u) ? 128u : 0u)) | _348;
    v_color = in_color;
    v_params = params;
    v_origin = in_abs_cell;
    v_local = local;
    v_shade = float(params.w);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kBatch3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc batch3d_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kBatch3DVert, sizeof(::fizmo::graphics::detail::gfx::kBatch3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kBatch3DVert;
    d.glsl_pieces   = sizeof(kBatch3DVert) / sizeof(kBatch3DVert[0]);
    d.glsl_version  = kBatch3DVertVersion;
    d.glsl_bindings = { kBatch3DVertBindings, 1 };
    d.name          = "batch3d.vert";
    return d;
}

inline constexpr int kInstanced3DVertVersion = 410;

inline constexpr const char* kInstanced3DVert[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    layout(row_major) mat4 mvp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    uvec4 extra;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 4) in vec4 in_offset_scale;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 1) in vec3 in_normal;
layout(location = 2) out vec4 v_light;
layout(location = 6) in vec4 in_light;
layout(location = 3) flat out uint v_flags;
layout(location = 4) out vec4 v_color;
layout(location = 3) in vec4 in_color;
layout(location = 5) in vec4 in_tint;
layout(location = 5) out vec2 v_uv;
layout(location = 2) in vec2 in_uv;
layout(location = 6) flat out uvec2 v_pbr;
layout(location = 7) out vec3 v_object;
layout(location = 8) out vec3 v_object_normal;

void main()
{
    vec4 p = vec4((in_pos * in_offset_scale.w) + in_offset_scale.xyz, 1.0);
    gl_Position = pc.mvp * p;
    v_world = vec3(dot(pc.model_x, p), dot(pc.model_y, p), dot(pc.model_z, p));
    v_normal = vec3(dot(pc.model_x.xyz, in_normal), dot(pc.model_y.xyz, in_normal), dot(pc.model_z.xyz, in_normal));
    v_light = in_light;
    v_flags = pc.extra.y;
    v_color = in_color * in_tint;
    v_uv = in_uv;
    v_pbr = pc.extra.zw;
    v_object = p.xyz;
    v_object_normal = in_normal;
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kInstanced3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc instanced3d_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kInstanced3DVert, sizeof(::fizmo::graphics::detail::gfx::kInstanced3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kInstanced3DVert;
    d.glsl_pieces   = sizeof(kInstanced3DVert) / sizeof(kInstanced3DVert[0]);
    d.glsl_version  = kInstanced3DVertVersion;
    d.glsl_bindings = { kInstanced3DVertBindings, 1 };
    d.name          = "instanced3d.vert";
    return d;
}

inline constexpr int kShadowMesh3DVertVersion = 410;

inline constexpr const char* kShadowMesh3DVert[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    layout(row_major) mat4 light_vp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    vec4 light;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 1) in vec3 in_normal;

void main()
{
    vec4 p = vec4(in_pos, 1.0);
    v_world = vec3(dot(pc.model_x, p), dot(pc.model_y, p), dot(pc.model_z, p));
    v_normal = vec3(dot(pc.model_x.xyz, in_normal), dot(pc.model_y.xyz, in_normal), dot(pc.model_z.xyz, in_normal));
    gl_Position = pc.light_vp * vec4(v_world, 1.0);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kShadowMesh3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc shadow_mesh_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kShadowMesh3DVert, sizeof(::fizmo::graphics::detail::gfx::kShadowMesh3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kShadowMesh3DVert;
    d.glsl_pieces   = sizeof(kShadowMesh3DVert) / sizeof(kShadowMesh3DVert[0]);
    d.glsl_version  = kShadowMesh3DVertVersion;
    d.glsl_bindings = { kShadowMesh3DVertBindings, 1 };
    d.name          = "shadow_mesh.vert";
    return d;
}

inline constexpr int kShadowInstanced3DVertVersion = 410;

inline constexpr const char* kShadowInstanced3DVert[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    layout(row_major) mat4 light_vp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    vec4 light;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 4) in vec4 in_offset_scale;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 1) in vec3 in_normal;

void main()
{
    vec4 p = vec4((in_pos * in_offset_scale.w) + in_offset_scale.xyz, 1.0);
    v_world = vec3(dot(pc.model_x, p), dot(pc.model_y, p), dot(pc.model_z, p));
    v_normal = vec3(dot(pc.model_x.xyz, in_normal), dot(pc.model_y.xyz, in_normal), dot(pc.model_z.xyz, in_normal));
    gl_Position = pc.light_vp * vec4(v_world, 1.0);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kShadowInstanced3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc shadow_instanced_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kShadowInstanced3DVert, sizeof(::fizmo::graphics::detail::gfx::kShadowInstanced3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kShadowInstanced3DVert;
    d.glsl_pieces   = sizeof(kShadowInstanced3DVert) / sizeof(kShadowInstanced3DVert[0]);
    d.glsl_version  = kShadowInstanced3DVertVersion;
    d.glsl_bindings = { kShadowInstanced3DVertBindings, 1 };
    d.name          = "shadow_instanced.vert";
    return d;
}

inline constexpr int kShadowQuad3DVertVersion = 410;

inline constexpr const char* kShadowQuad3DVert[] = {
R"fizmo_glsl(const vec3 _33[7] = vec3[](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0), vec3(0.0));

layout(std140) uniform Push
{
    layout(row_major) mat4 light_vp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    vec4 light;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 2) in uint in_params;
layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;

void main()
{
    vec4 p = vec4(in_pos, 1.0);
    vec3 f = _33[min(((in_params >> uint(8)) & 255u), 6u)];
    v_world = vec3(dot(pc.model_x, p), dot(pc.model_y, p), dot(pc.model_z, p));
    v_normal = vec3(dot(pc.model_x.xyz, f), dot(pc.model_y.xyz, f), dot(pc.model_z.xyz, f));
    gl_Position = pc.light_vp * vec4(v_world, 1.0);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kShadowQuad3DVertBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc shadow_quad_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kShadowQuad3DVert, sizeof(::fizmo::graphics::detail::gfx::kShadowQuad3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kShadowQuad3DVert;
    d.glsl_pieces   = sizeof(kShadowQuad3DVert) / sizeof(kShadowQuad3DVert[0]);
    d.glsl_version  = kShadowQuad3DVertVersion;
    d.glsl_bindings = { kShadowQuad3DVertBindings, 1 };
    d.name          = "shadow_quad.vert";
    return d;
}

inline constexpr int kShadowPoint3DFragVersion = 410;

inline constexpr const char* kShadowPoint3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform Push
{
    layout(row_major) mat4 light_vp;
    vec4 model_x;
    vec4 model_y;
    vec4 model_z;
    vec4 light;
} pc;

layout(location = 0) in vec3 v_world;
layout(location = 1) in vec3 v_normal;

void main()
{
    vec3 d = v_world - pc.light.xyz;
    if (dot(v_normal, d) > 0.0)
    {
        discard;
    }
    gl_FragDepth = clamp(length(d) / pc.light.w, 0.0, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kShadowPoint3DFragBindings[] = {
    { "Push", ::fizmo::gpu::GlslResource::PushBlock, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc shadow_point_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kShadowPoint3DFrag, sizeof(::fizmo::graphics::detail::gfx::kShadowPoint3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kShadowPoint3DFrag;
    d.glsl_pieces   = sizeof(kShadowPoint3DFrag) / sizeof(kShadowPoint3DFrag[0]);
    d.glsl_version  = kShadowPoint3DFragVersion;
    d.glsl_bindings = { kShadowPoint3DFragBindings, 1 };
    d.name          = "shadow_point.frag";
    return d;
}

inline constexpr int kSky3DVertVersion = 410;

inline constexpr const char* kSky3DVert[] = {
R"fizmo_glsl(layout(location = 0) out vec2 v_ndc;

void main()
{
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_ndc = (corner * 2.0) - vec2(1.0);
    gl_Position = vec4(v_ndc, 1.0, 1.0);
#ifdef FIZMO_GL_DEPTH_FIXUP
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
#endif
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSky3DVertBindings[] = {
    { nullptr, ::fizmo::gpu::GlslResource::Texture, 0, 0, 0 },
};

inline ::fizmo::gpu::ShaderDesc sky_vert() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Vertex;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSky3DVert, sizeof(::fizmo::graphics::detail::gfx::kSky3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kSky3DVert;
    d.glsl_pieces   = sizeof(kSky3DVert) / sizeof(kSky3DVert[0]);
    d.glsl_version  = kSky3DVertVersion;
    d.glsl_bindings = { kSky3DVertBindings, 0 };
    d.name          = "sky.vert";
    return d;
}

inline constexpr int kSky3DFragVersion = 410;

inline constexpr const char* kSky3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
} scene;

uniform sampler2DShadow sun_map;
uniform sampler2DShadow sun_map_b;
uniform sampler3D volume_light;
uniform samplerCubeShadow point_maps[8];
uniform sampler2D scene_color;
uniform sampler2D scene_depth;
uniform sampler2D sun_depth;
uniform sampler2D sun_depth_b;
uniform sampler2D planar_maps[2];

layout(location = 0) in vec2 v_ndc;
layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

float sky_height(vec3 d)
{
    float a = abs(scene.glow.z);
    float s = (scene.glow.z < 0.0) ? (-1.0) : 1.0;
    if ((a > 0.5) && (a < 1.5))
    {
        return s * d.x;
    }
    if ((a > 1.5) && (a < 2.5))
    {
        return s * d.y;
    }
    if (a > 2.5)
    {
        return s * d.z;
    }
    return d.z;
}

float star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.98980045318603515625, 78.233001708984375, 37.71900177001953125))) * 43758.546875);
}

vec3 sky_radiance(vec3 dir, bool disks)
{
    vec3 param = dir;
    float h = sky_height(param);
    vec3 col = mix(scene.sky_horizon.xyz, scene.sky_zenith.xyz, vec3(pow(clamp(h, 0.0, 1.0), 0.449999988079071044921875)));
    if (h < 0.0)
    {
        col = mix(scene.sky_horizon.xyz, scene.sky_horizon.xyz * 0.3499999940395355224609375, vec3(clamp((-h) * 3.0, 0.0, 1.0)));
    }
    float cs = max(dot(dir, scene.sky_sun.xyz), 0.0);
    col += ((scene.sky_glow.xyz * scene.sky_glow.w) * ((pow(cs, scene.glow.x) * 0.5) + pow(cs, scene.sun_disk.w)));
    if (disks && (h > (-0.0199999995529651641845703125)))
    {
        col += (scene.sun_disk.xyz * smoothstep(scene.sky_sun.w - ((1.0 - scene.sky_sun.w) * 0.300000011920928955078125), scene.sky_sun.w, cs));
        float cm = dot(dir, scene.sky_moon.xyz);
        col += ((vec3(0.85000002384185791015625, 0.89999997615814208984375, 1.0) * scene.gloss.w) * smoothstep(scene.sky_moon.w - ((1.0 - scene.sky_moon.w) * 0.300000011920928955078125), scene.sky_moon.w, cm));
        vec3 param_1 = floor(dir * 400.0);
        float star = step(0.99849998950958251953125, star_hash(param_1));
        col += vec3((star * scene.sky_zenith.w) * smoothstep(0.0, 0.100000001490116119384765625, h));
    }
    return col;
}

float noise_at(vec2 p)
{
    return fract(52.98291778564453125 * fract(dot(p, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

float screen_noise()
{
    vec2 param = gl_FragCoord.xy;
    return noise_at(param);
}

float sun_tap(mat4 m, sampler2DShadow map, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    bool _336 = abs(c.x) >= 1.0;
    bool _344;
    if (!_336)
    {
        _344 = abs(c.y) >= 1.0;
    }
    else
    {
        _344 = _336;
    }
    bool _351;
    if (!_344)
    {
        _351 = c.z <= 0.0;
    }
    else
    {
        _351 = _344;
    }
    bool _358;
    if (!_351)
    {
        _358 = c.z >= 1.0;
    }
    else
    {
        _358 = _351;
    }
    if (_358)
    {
        return 1.0;
    }
    vec3 _376 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_376.xy, _376.z));
}

float sun_lit_at(vec3 p)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_tap(param, sun_map, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_tap(param_2, sun_map_b, param_3), scene.sun_mix.x);
    }
    return lit;
}

float march_light(vec3 dir, float reach)
{
    int steps = max(int(scene.volume.w), 1);
    float bias = max(scene.shafts.y, 1.0);
    float jitter = screen_noise();
    float density = scene.volume.x;
    float lit = 0.0;
    for (int i = 0; i < steps; i++)
    {
        float a = (float(i) + mix(0.0199999995529651641845703125, 0.980000019073486328125, jitter)) / float(steps);
        float t = reach * pow(a, bias);
        float dt = ((reach * bias) * pow(a, bias - 1.0)) / float(steps);
        vec3 param = dir * t;
        lit += ((sun_lit_at(param) * exp((-density) * t)) * dt);
    }
    return lit;
}

float volume_lookup(vec3 dir, float reach)
{
    vec4 c = spvWorkaroundRowMajor(scene.volume_proj) * vec4(dir, 1.0);
    if (c.w <= 0.0500000007450580596923828125)
    {
        vec3 param = dir;
        float param_1 = reach;
        return march_light(param, param_1);
    }
    float slices = scene.volume_grid.z;
    float s = pow(clamp(reach / scene.volume.z, 0.0, 1.0), 1.0 / max(scene.shafts.y, 1.0)) * slices;
    vec2 uv = ((c.xy / vec2(c.w)) * 0.5) + vec2(0.5);
    return textureLod(volume_light, vec3(uv, (max(s, 1.0) - 0.5) / slices), 0.0).x * min(s, 1.0);
}

float phase_hg(float g, float cs)
{
    return (1.0 - (g * g)) / (12.56637096405029296875 * pow(max((1.0 + (g * g)) - ((2.0 * g) * cs), 9.9999997473787516355514526367188e-05), 1.5));
}

vec3 inscatter(vec3 dir, float dist)
{
    float reach = min(dist, scene.volume.z);
    float _603;
    if ((scene.counts.y & 8192) != 0)
    {
        vec3 param = dir;
        float param_1 = reach;
        _603 = volume_lookup(param, param_1);
    }
    else
    {
        vec3 param_2 = dir;
        float param_3 = reach;
        _603 = march_light(param_2, param_3);
    }
    float lit = _603;
    float cs = dot(dir, -scene.sun_dir.xyz);
    float param_4 = 0.0;
    float param_5 = cs;
    float param_6 = scene.volume.y;
    float param_7 = cs;
    float phase = mix(phase_hg(param_4, param_5), phase_hg(param_6, param_7), 0.75);
    return (((scene.sun_color.xyz * lit) * scene.volume.x) * phase) * scene.shafts.x;
}

vec3 tone(inout vec3 c)
{
    if ((scene.counts.y & 32) == 0)
    {
        return c;
    }
    c *= scene.sky_params.w;
    c = clamp((c * ((c * 2.5099999904632568359375) + vec3(0.02999999932944774627685546875))) / ((c * ((c * 2.4300000667572021484375) + vec3(0.589999973773956298828125))) + vec3(0.14000000059604644775390625)), vec3(0.0), vec3(1.0));
    float luma = dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return clamp(mix(vec3(luma), c, vec3(scene.medium_color.w)), vec3(0.0), vec3(1.0));
}

void main()
{
    vec4 p = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(v_ndc, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / vec3(p.w));
    vec3 param = dir;
    bool param_1 = true;
    vec3 c = sky_radiance(param, param_1);
    if ((scene.counts.y & 16) != 0)
    {
        vec3 param_2 = dir;
        float param_3 = scene.volume.z;
        c += inscatter(param_2, param_3);
    }
    if ((scene.counts.y & 64) != 0)
    {
        c = scene.medium_color.xyz;
    }
    vec3 param_4 = c;
    vec3 _763 = tone(param_4);
    out_color = vec4(_763, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSky3DFragBindings[] = {
    { "sun_map", ::fizmo::gpu::GlslResource::Texture, 1, 1, 1 },
    { "sun_map_b", ::fizmo::gpu::GlslResource::Texture, 1, 6, 1 },
    { "volume_light", ::fizmo::gpu::GlslResource::Texture, 1, 9, 1 },
    { "point_maps", ::fizmo::gpu::GlslResource::Texture, 1, 2, 8 },
    { "scene_color", ::fizmo::gpu::GlslResource::Texture, 1, 3, 1 },
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "sun_depth", ::fizmo::gpu::GlslResource::Texture, 1, 5, 1 },
    { "sun_depth_b", ::fizmo::gpu::GlslResource::Texture, 1, 7, 1 },
    { "planar_maps", ::fizmo::gpu::GlslResource::Texture, 1, 8, 2 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc sky_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSky3DFrag, sizeof(::fizmo::graphics::detail::gfx::kSky3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kSky3DFrag;
    d.glsl_pieces   = sizeof(kSky3DFrag) / sizeof(kSky3DFrag[0]);
    d.glsl_version  = kSky3DFragVersion;
    d.glsl_bindings = { kSky3DFragBindings, 10 };
    d.name          = "sky.frag";
    return d;
}

inline constexpr int kRays3DFragVersion = 410;

inline constexpr const char* kRays3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
} scene;

uniform sampler2DShadow sun_map;
uniform sampler2DShadow sun_map_b;
uniform sampler2D scene_depth;
uniform samplerCubeShadow point_maps[8];
uniform sampler2D scene_color;
uniform sampler2D sun_depth;
uniform sampler2D sun_depth_b;
uniform sampler2D planar_maps[2];
uniform sampler3D volume_light;

layout(location = 0) in vec2 v_ndc;
layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

vec2 project_uv(vec3 p, out float w)
{
    vec4 c = spvWorkaroundRowMajor(scene.view_proj) * vec4(p, 1.0);
    w = c.w;
    vec2 ndc = c.xy / vec2(max(c.w, 9.9999997473787516355514526367188e-06));
    return scene.screen.xy + (((ndc * 0.5) + vec2(0.5)) * scene.screen.zw);
}

float noise_at(vec2 p)
{
    return fract(52.98291778564453125 * fract(dot(p, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

float screen_noise()
{
    vec2 param = gl_FragCoord.xy;
    return noise_at(param);
}

vec3 scene_point(vec2 uv)
{
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, texture(scene_depth, uv).x, 1.0);
    return q.xyz / vec3(q.w);
}

float sun_tap(mat4 m, sampler2DShadow map, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    bool _88 = abs(c.x) >= 1.0;
    bool _97;
    if (!_88)
    {
        _97 = abs(c.y) >= 1.0;
    }
    else
    {
        _97 = _88;
    }
    bool _106;
    if (!_97)
    {
        _106 = c.z <= 0.0;
    }
    else
    {
        _106 = _97;
    }
    bool _113;
    if (!_106)
    {
        _113 = c.z >= 1.0;
    }
    else
    {
        _113 = _106;
    }
    if (_113)
    {
        return 1.0;
    }
    vec3 _152 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_152.xy, _152.z));
}

float sun_lit_at(vec3 p)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_tap(param, sun_map, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_tap(param_2, sun_map_b, param_3), scene.sun_mix.x);
    }
    return lit;
}

float lit_air(vec3 dir, float reach)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    float lit = 0.0;
    for (int i = 0; i < 4; i++)
    {
        vec3 param = dir * ((reach * (float(i) + 0.5)) / 4.0);
        lit += sun_lit_at(param);
    }
    return lit / 4.0;
}

vec3 tone(inout vec3 c)
{
    if ((scene.counts.y & 32) == 0)
    {
        return c;
    }
    c *= scene.sky_params.w;
    c = clamp((c * ((c * 2.5099999904632568359375) + vec3(0.02999999932944774627685546875))) / ((c * ((c * 2.4300000667572021484375) + vec3(0.589999973773956298828125))) + vec3(0.14000000059604644775390625)), vec3(0.0), vec3(1.0));
    float luma = dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
    return clamp(mix(vec3(luma), c, vec3(scene.medium_color.w)), vec3(0.0), vec3(1.0));
}

void main()
{
    bool _375 = (scene.counts.y & 1024) == 0;
    bool _384;
    if (!_375)
    {
        _384 = (scene.counts.y & 128) == 0;
    }
    else
    {
        _384 = _375;
    }
    if (_384)
    {
        discard;
    }
    vec3 toward = -scene.sun_dir.xyz;
    vec3 param = toward * 1000.0;
    float param_1;
    vec2 _400 = project_uv(param, param_1);
    float w = param_1;
    vec2 sun_uv = _400;
    if (w <= 0.0)
    {
        discard;
    }
    vec2 uv = gl_FragCoord.xy * scene.target.xy;
    vec4 p = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(v_ndc, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / vec3(p.w));
    float facing = max(dot(dir, toward), 0.0);
    if (facing <= 0.0)
    {
        discard;
    }
    int samples = max(int(scene.rays.y), 1);
    vec2 stride = ((sun_uv - uv) * scene.rays.w) / vec2(float(samples));
    vec2 at = uv + (stride * screen_noise());
    float weight = 1.0;
    float light = 0.0;
    float total = 0.0;
    for (int i = 0; i < samples; i++)
    {
        vec2 c = clamp(at, scene.screen.xy, scene.screen.xy + scene.screen.zw);
        light += (step(1.0, texture(scene_depth, c).x) * weight);
        total += weight;
        weight *= scene.rays.z;
        at += stride;
    }
    float open = light / max(total, 9.9999997473787516355514526367188e-05);
    float depth = texture(scene_depth, clamp(uv, scene.screen.xy, scene.screen.xy + scene.screen.zw)).x;
    float _537;
    if (depth >= 1.0)
    {
        _537 = 96.0;
    }
    else
    {
        vec2 param_2 = uv;
        _537 = min(length(scene_point(param_2)), 96.0);
    }
    float reach = _537;
    vec3 param_3 = dir;
    float param_4 = reach;
    float air = (reach / 96.0) * lit_air(param_3, param_4);
    float shaft = (pow(facing, scene.glow.y) * min((open * (1.0 - open)) * 4.0, 1.0)) * air;
    vec3 param_5 = (scene.sun_color.xyz * shaft) * scene.rays.x;
    vec3 _583 = tone(param_5);
    out_color = vec4(_583, 0.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kRays3DFragBindings[] = {
    { "sun_map", ::fizmo::gpu::GlslResource::Texture, 1, 1, 1 },
    { "sun_map_b", ::fizmo::gpu::GlslResource::Texture, 1, 6, 1 },
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "point_maps", ::fizmo::gpu::GlslResource::Texture, 1, 2, 8 },
    { "scene_color", ::fizmo::gpu::GlslResource::Texture, 1, 3, 1 },
    { "sun_depth", ::fizmo::gpu::GlslResource::Texture, 1, 5, 1 },
    { "sun_depth_b", ::fizmo::gpu::GlslResource::Texture, 1, 7, 1 },
    { "planar_maps", ::fizmo::gpu::GlslResource::Texture, 1, 8, 2 },
    { "volume_light", ::fizmo::gpu::GlslResource::Texture, 1, 9, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc rays_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kRays3DFrag, sizeof(::fizmo::graphics::detail::gfx::kRays3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kRays3DFrag;
    d.glsl_pieces   = sizeof(kRays3DFrag) / sizeof(kRays3DFrag[0]);
    d.glsl_version  = kRays3DFragVersion;
    d.glsl_bindings = { kRays3DFragBindings, 10 };
    d.name          = "rays.frag";
    return d;
}

inline constexpr int kVolume3DCompVersion = 430;

inline constexpr const char* kVolume3DComp[] = {
R"fizmo_glsl(layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
} scene;

uniform sampler2DShadow sun_map;
uniform sampler2DShadow sun_map_b;
layout(r32f) uniform writeonly image3D volume_out;
uniform samplerCubeShadow point_maps[8];
uniform sampler2D scene_color;
uniform sampler2D scene_depth;
uniform sampler2D sun_depth;
uniform sampler2D sun_depth_b;
uniform sampler2D planar_maps[2];
uniform sampler3D volume_light;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

float noise_at(vec2 p)
{
    return fract(52.98291778564453125 * fract(dot(p, vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

float sun_tap(mat4 m, sampler2DShadow map, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    bool _59 = abs(c.x) >= 1.0;
    bool _68;
    if (!_59)
    {
        _68 = abs(c.y) >= 1.0;
    }
    else
    {
        _68 = _59;
    }
    bool _77;
    if (!_68)
    {
        _77 = c.z <= 0.0;
    }
    else
    {
        _77 = _68;
    }
    bool _84;
    if (!_77)
    {
        _84 = c.z >= 1.0;
    }
    else
    {
        _84 = _77;
    }
    if (_84)
    {
        return 1.0;
    }
    vec3 _123 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return textureLod(map, vec3(_123.xy, _123.z), 0.0);
}

float sun_lit_at(vec3 p)
{
    if ((scene.counts.y & 2) == 0)
    {
        return 1.0;
    }
    mat4 param = spvWorkaroundRowMajor(scene.sun_matrix);
    vec3 param_1 = p;
    float lit = sun_tap(param, sun_map, param_1);
    if (scene.sun_mix.y > 0.0)
    {
        mat4 param_2 = spvWorkaroundRowMajor(scene.sun_matrix_b);
        vec3 param_3 = p;
        lit = mix(lit, sun_tap(param_2, sun_map_b, param_3), scene.sun_mix.x);
    }
    return lit;
}

void main()
{
    ivec3 size = imageSize(volume_out);
    ivec2 cell = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(cell, size.xy)))
    {
        return;
    }
    vec2 local = (vec2(cell) + vec2(0.5)) / vec2(size.xy);
    vec4 p = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4((local * 2.0) - vec2(1.0), 1.0, 1.0);
    vec3 dir = normalize(p.xyz / vec3(p.w));
    float reach = scene.volume.z;
    float bias = max(scene.shafts.y, 1.0);
    float density = scene.volume.x;
    vec2 param = vec2(cell);
    float jitter = noise_at(param);
    float total = 0.0;
    float from = 0.0;
    for (int k = 0; k < size.z; k++)
    {
        float to = reach * pow(float(k + 1) / float(size.z), bias);
        float dt = (to - from) / 2.0;
        for (int m = 0; m < 2; m++)
        {
            float t = from + ((float(m) + mix(0.0199999995529651641845703125, 0.980000019073486328125, jitter)) * dt);
            vec3 param_1 = dir * t;
            total += ((sun_lit_at(param_1) * exp((-density) * t)) * dt);
        }
        imageStore(volume_out, ivec3(cell, k), vec4(total));
        from = to;
    }
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kVolume3DCompBindings[] = {
    { "sun_map", ::fizmo::gpu::GlslResource::Texture, 1, 1, 1 },
    { "sun_map_b", ::fizmo::gpu::GlslResource::Texture, 1, 6, 1 },
    { "point_maps", ::fizmo::gpu::GlslResource::Texture, 1, 2, 8 },
    { "scene_color", ::fizmo::gpu::GlslResource::Texture, 1, 3, 1 },
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "sun_depth", ::fizmo::gpu::GlslResource::Texture, 1, 5, 1 },
    { "sun_depth_b", ::fizmo::gpu::GlslResource::Texture, 1, 7, 1 },
    { "planar_maps", ::fizmo::gpu::GlslResource::Texture, 1, 8, 2 },
    { "volume_light", ::fizmo::gpu::GlslResource::Texture, 1, 9, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
    { "volume_out", ::fizmo::gpu::GlslResource::StorageImage, 0, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc volume_comp() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Compute;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kVolume3DComp, sizeof(::fizmo::graphics::detail::gfx::kVolume3DComp) / sizeof(std::uint32_t) };
    d.glsl          = kVolume3DComp;
    d.glsl_pieces   = sizeof(kVolume3DComp) / sizeof(kVolume3DComp[0]);
    d.glsl_version  = kVolume3DCompVersion;
    d.glsl_bindings = { kVolume3DCompBindings, 11 };
    d.name          = "volume.comp";
    return d;
}

inline constexpr int kSsao3DFragVersion = 410;

inline constexpr const char* kSsao3DFrag[] = {
R"fizmo_glsl(const float _134[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
    vec4 ao;
    vec4 ao_target;
} scene;

uniform sampler2D scene_depth;
uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

vec2 full_uv(vec2 texel)
{
    return ((texel * scene.ao_target.z) + vec2(0.5)) * scene.target.xy;
}

vec3 view_at(vec2 uv)
{
    float raw = textureLod(scene_depth, uv, 0.0).x;
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, raw, 1.0);
    return q.xyz / vec3(q.w);
}

vec3 pick(vec3 a, vec3 b)
{
    bvec3 _166 = bvec3(dot(a, a) < dot(b, b));
    return vec3(_166.x ? a.x : b.x, _166.y ? a.y : b.y, _166.z ? a.z : b.z);
}

float pattern(ivec2 p)
{
    return _134[((p.y & 3) * 4) + (p.x & 3)] / 16.0;
}

void main()
{
    vec2 texel = floor(gl_FragCoord.xy);
    vec2 param = texel;
    vec2 uv = full_uv(param);
    float raw = textureLod(scene_depth, uv, 0.0).x;
    if (raw >= 1.0)
    {
        out_color = vec4(1.0);
        return;
    }
    vec2 param_1 = uv;
    vec3 p = view_at(param_1);
    vec2 px = scene.target.xy;
    vec2 param_2 = uv + vec2(px.x, 0.0);
    vec2 param_3 = uv - vec2(px.x, 0.0);
    vec3 param_4 = view_at(param_2) - p;
    vec3 param_5 = p - view_at(param_3);
    vec3 dx = pick(param_4, param_5);
    vec2 param_6 = uv + vec2(0.0, px.y);
    vec2 param_7 = uv - vec2(0.0, px.y);
    vec3 param_8 = view_at(param_6) - p;
    vec3 param_9 = p - view_at(param_7);
    vec3 dy = pick(param_8, param_9);
    vec3 n = normalize(cross(dx, dy));
    if (dot(n, -p) < 0.0)
    {
        n = -n;
    }
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 qx = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc + vec2(0.100000001490116119384765625, 0.0), raw, 1.0);
    vec4 qy = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc + vec2(0.0, 0.100000001490116119384765625), raw, 1.0);
    float lx = length((qx.xyz / vec3(qx.w)) - p);
    float ly = length((qy.xyz / vec3(qy.w)) - p);
    float radius = scene.ao.x;
    vec2 reach = (vec2((0.100000001490116119384765625 * radius) / max(lx, 9.9999999747524270787835121154785e-07), (0.100000001490116119384765625 * radius) / max(ly, 9.9999999747524270787835121154785e-07)) * 0.5) * scene.screen.zw;
    float shrink = min(1.0, 0.119999997317790985107421875 / max(reach.y, 9.9999999747524270787835121154785e-07));
    reach *= shrink;
    float world_r = radius * shrink;
    if ((reach.y / scene.target.y) < 1.0)
    {
        out_color = vec4(1.0);
        return;
    }
    int count = clamp(int(scene.ao.z + 0.5), 4, 32);
    ivec2 param_10 = ivec2(texel);
    float spin = pattern(param_10) * 6.283185482025146484375;
    ivec2 param_11 = ivec2(texel) + ivec2(2, 1);
    float jitter = fract(pattern(param_11) + 0.03125);
    float r2 = world_r * world_r;
    float sum = 0.0;
    float used = 0.0;
    vec2 lo = scene.screen.xy;
    vec2 hi = scene.screen.xy + scene.screen.zw;
    for (int i = 0; i < count; i++)
    {
        float t = (float(i) + jitter) / float(count);
        float a = (float(i) * 2.3999631404876708984375) + spin;
        vec2 suv = uv + ((vec2(cos(a), sin(a)) * sqrt(t)) * reach);
        bool _446 = any(lessThan(suv, lo));
        bool _454;
        if (!_446)
        {
            _454 = any(greaterThan(suv, hi));
        }
        else
        {
            _454 = _446;
        }
        if (_454)
        {
            continue;
        }
        used += 1.0;
        vec2 param_12 = suv;
        vec3 v = view_at(param_12) - p;
        float vv = dot(v, v);
        float vn = dot(v, n);
        float fall = max(1.0 - (vv / r2), 0.0);
        sum += (fall * max((vn * inversesqrt(vv + 9.9999997473787516355514526367188e-05)) - 0.07999999821186065673828125, 0.0));
    }
    float _498;
    if (used > 0.0)
    {
        _498 = sum / used;
    }
    else
    {
        _498 = 0.0;
    }
    float occl = _498;
    float ao = clamp(1.0 - ((occl * 2.2000000476837158203125) * scene.ao.y), 0.0, 1.0);
    out_color = vec4(ao, ao, ao, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSsao3DFragBindings[] = {
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc ssao_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSsao3DFrag, sizeof(::fizmo::graphics::detail::gfx::kSsao3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kSsao3DFrag;
    d.glsl_pieces   = sizeof(kSsao3DFrag) / sizeof(kSsao3DFrag[0]);
    d.glsl_version  = kSsao3DFragVersion;
    d.glsl_bindings = { kSsao3DFragBindings, 3 };
    d.name          = "ssao.frag";
    return d;
}

inline constexpr int kSsaoBlurH3DFragVersion = 410;

inline constexpr const char* kSsaoBlurH3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
    vec4 ao;
    vec4 ao_target;
} scene;

uniform sampler2D scene_depth;
uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

vec2 full_uv(vec2 texel)
{
    return ((texel * scene.ao_target.z) + vec2(0.5)) * scene.target.xy;
}

vec3 view_at(vec2 uv)
{
    float raw = textureLod(scene_depth, uv, 0.0).x;
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, raw, 1.0);
    return q.xyz / vec3(q.w);
}

float key_at(vec2 texel)
{
    vec2 param = texel;
    vec2 param_1 = full_uv(param);
    return length(view_at(param_1));
}

void main()
{
    vec2 texel = floor(gl_FragCoord.xy);
    vec2 dir = vec2(1.0, 0.0);
    vec2 size = vec2(textureSize(tex, 0));
    vec2 param = texel;
    float d0 = key_at(param);
    float tol = (d0 * 0.039999999105930328369140625) + 0.0199999995529651641845703125;
    float sum = 0.0;
    float wsum = 0.0;
    for (int k = -4; k <= 4; k++)
    {
        vec2 t = clamp(texel + (dir * float(k)), vec2(0.0), size - vec2(1.0));
        float a = texelFetch(tex, ivec2(t), 0).x;
        vec2 param_1 = t;
        float dk = key_at(param_1);
        float w = exp((-float(k * k)) / 9.68000030517578125) * max(1.0 - (abs(dk - d0) / tol), 0.0);
        sum += (a * w);
        wsum += w;
    }
    float _215;
    if (wsum > 9.9999997473787516355514526367188e-06)
    {
        _215 = sum / wsum;
    }
    else
    {
        _215 = texelFetch(tex, ivec2(texel), 0).x;
    }
    float ao = _215;
    out_color = vec4(ao, ao, ao, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSsaoBlurH3DFragBindings[] = {
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc ssao_blur_h_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSsaoBlurH3DFrag, sizeof(::fizmo::graphics::detail::gfx::kSsaoBlurH3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kSsaoBlurH3DFrag;
    d.glsl_pieces   = sizeof(kSsaoBlurH3DFrag) / sizeof(kSsaoBlurH3DFrag[0]);
    d.glsl_version  = kSsaoBlurH3DFragVersion;
    d.glsl_bindings = { kSsaoBlurH3DFragBindings, 3 };
    d.name          = "ssao_blur_h.frag";
    return d;
}

inline constexpr int kSsaoBlurV3DFragVersion = 410;

inline constexpr const char* kSsaoBlurV3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
    vec4 ao;
    vec4 ao_target;
} scene;

uniform sampler2D scene_depth;
uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

vec2 full_uv(vec2 texel)
{
    return ((texel * scene.ao_target.z) + vec2(0.5)) * scene.target.xy;
}

vec3 view_at(vec2 uv)
{
    float raw = textureLod(scene_depth, uv, 0.0).x;
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, raw, 1.0);
    return q.xyz / vec3(q.w);
}

float key_at(vec2 texel)
{
    vec2 param = texel;
    vec2 param_1 = full_uv(param);
    return length(view_at(param_1));
}

void main()
{
    vec2 texel = floor(gl_FragCoord.xy);
    vec2 dir = vec2(0.0, 1.0);
    vec2 size = vec2(textureSize(tex, 0));
    vec2 param = texel;
    float d0 = key_at(param);
    float tol = (d0 * 0.039999999105930328369140625) + 0.0199999995529651641845703125;
    float sum = 0.0;
    float wsum = 0.0;
    for (int k = -4; k <= 4; k++)
    {
        vec2 t = clamp(texel + (dir * float(k)), vec2(0.0), size - vec2(1.0));
        float a = texelFetch(tex, ivec2(t), 0).x;
        vec2 param_1 = t;
        float dk = key_at(param_1);
        float w = exp((-float(k * k)) / 9.68000030517578125) * max(1.0 - (abs(dk - d0) / tol), 0.0);
        sum += (a * w);
        wsum += w;
    }
    float _215;
    if (wsum > 9.9999997473787516355514526367188e-06)
    {
        _215 = sum / wsum;
    }
    else
    {
        _215 = texelFetch(tex, ivec2(texel), 0).x;
    }
    float ao = _215;
    out_color = vec4(ao, ao, ao, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSsaoBlurV3DFragBindings[] = {
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc ssao_blur_v_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSsaoBlurV3DFrag, sizeof(::fizmo::graphics::detail::gfx::kSsaoBlurV3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kSsaoBlurV3DFrag;
    d.glsl_pieces   = sizeof(kSsaoBlurV3DFrag) / sizeof(kSsaoBlurV3DFrag[0]);
    d.glsl_version  = kSsaoBlurV3DFragVersion;
    d.glsl_bindings = { kSsaoBlurV3DFragBindings, 3 };
    d.name          = "ssao_blur_v.frag";
    return d;
}

inline constexpr int kSsaoApply3DFragVersion = 410;

inline constexpr const char* kSsaoApply3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
    vec4 ao;
    vec4 ao_target;
} scene;

uniform sampler2D scene_depth;
uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

vec3 view_at(vec2 uv)
{
    float raw = textureLod(scene_depth, uv, 0.0).x;
    vec2 ndc = (((uv - scene.screen.xy) / scene.screen.zw) * 2.0) - vec2(1.0);
    vec4 q = spvWorkaroundRowMajor(scene.inv_view_proj) * vec4(ndc, raw, 1.0);
    return q.xyz / vec3(q.w);
}

vec2 full_uv(vec2 texel)
{
    return ((texel * scene.ao_target.z) + vec2(0.5)) * scene.target.xy;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * scene.target.xy;
    float raw = textureLod(scene_depth, uv, 0.0).x;
    if (raw >= 1.0)
    {
        discard;
    }
    vec2 param = uv;
    float d0 = length(view_at(param));
    vec2 size = vec2(textureSize(tex, 0));
    vec2 h = floor(gl_FragCoord.xy) / vec2(scene.ao_target.z);
    vec2 base = floor(h);
    vec2 f = h - base;
    float sum = 0.0;
    float wsum = 0.0;
    float _191;
    float _203;
    for (int j = 0; j < 2; j++)
    {
        for (int i = 0; i < 2; i++)
        {
            vec2 t = clamp(base + vec2(float(i), float(j)), vec2(0.0), size - vec2(1.0));
            if (i == 0)
            {
                _191 = 1.0 - f.x;
            }
            else
            {
                _191 = f.x;
            }
            if (j == 0)
            {
                _203 = 1.0 - f.y;
            }
            else
            {
                _203 = f.y;
            }
            float bil = _191 * _203;
            vec2 param_1 = t;
            vec2 param_2 = full_uv(param_1);
            float dk = length(view_at(param_2));
            float w = (bil + 0.001000000047497451305389404296875) / ((abs(dk - d0) / ((d0 * 0.02999999932944774627685546875) + 0.00999999977648258209228515625)) + 0.00999999977648258209228515625);
            sum += (texelFetch(tex, ivec2(t), 0).x * w);
            wsum += w;
        }
    }
    float _259;
    if (wsum > 0.0)
    {
        _259 = sum / wsum;
    }
    else
    {
        _259 = 1.0;
    }
    float ao = _259;
    out_color = vec4(0.0, 0.0, 0.0, clamp(1.0 - ao, 0.0, 1.0));
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSsaoApply3DFragBindings[] = {
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc ssao_apply_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSsaoApply3DFrag, sizeof(::fizmo::graphics::detail::gfx::kSsaoApply3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kSsaoApply3DFrag;
    d.glsl_pieces   = sizeof(kSsaoApply3DFrag) / sizeof(kSsaoApply3DFrag[0]);
    d.glsl_version  = kSsaoApply3DFragVersion;
    d.glsl_bindings = { kSsaoApply3DFragBindings, 3 };
    d.name          = "ssao_apply.frag";
    return d;
}

inline constexpr int kSsaoFinish3DFragVersion = 410;

inline constexpr const char* kSsaoFinish3DFrag[] = {
R"fizmo_glsl(layout(std140) uniform SceneLight
{
    vec4 legacy;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 block_color;
    vec4 params;
    ivec4 counts;
    layout(row_major) mat4 sun_matrix;
    vec4 sun_shadow;
    vec4 point_shadow;
    vec4 point_pos[64];
    vec4 point_color[64];
    vec4 sky_zenith;
    vec4 sky_horizon;
    vec4 sky_glow;
    vec4 sky_sun;
    vec4 sky_moon;
    vec4 sky_params;
    vec4 medium_color;
    vec4 volume;
    vec4 waves;
    vec4 gloss;
    layout(row_major) mat4 inv_view_proj;
    layout(row_major) mat4 view_proj;
    vec4 screen;
    vec4 target;
    vec4 water;
    vec4 soft;
    vec4 shafts;
    vec4 rays;
    layout(row_major) mat4 sun_matrix_b;
    vec4 sun_mix;
    vec4 planes[2];
    vec4 plane_info;
    vec4 clip;
    vec4 sun_disk;
    vec4 glow;
    vec4 capsule_a[4];
    vec4 capsule_b[4];
    vec4 capsules;
    vec4 volume_grid;
    layout(row_major) mat4 volume_proj;
    vec4 plane_rects[2];
    vec4 swell;
    vec4 swell_phase;
    vec4 point_spot[64];
    vec4 point_map[8];
    vec4 point_shape[64];
    vec4 point_size[64];
    vec4 point_normal[64];
    vec4 ao;
    vec4 ao_target;
} scene;

uniform sampler2D scene_depth;
uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

void main()
{
    float raw = textureLod(scene_depth, gl_FragCoord.xy * scene.target.xy, 0.0).x;
    if (raw >= 1.0)
    {
        discard;
    }
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
}
)fizmo_glsl",
};

inline constexpr ::fizmo::gpu::GlslBinding kSsaoFinish3DFragBindings[] = {
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc ssao_finish_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::graphics::detail::gfx::kSsaoFinish3DFrag, sizeof(::fizmo::graphics::detail::gfx::kSsaoFinish3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kSsaoFinish3DFrag;
    d.glsl_pieces   = sizeof(kSsaoFinish3DFrag) / sizeof(kSsaoFinish3DFrag[0]);
    d.glsl_version  = kSsaoFinish3DFragVersion;
    d.glsl_bindings = { kSsaoFinish3DFragBindings, 3 };
    d.name          = "ssao_finish.frag";
    return d;
}

} // namespace glsl
} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // FIZMO_GL_SHADERS_HPP
