#ifndef FIZMO_GL_SHADERS_HPP
#define FIZMO_GL_SHADERS_HPP

#include "../../GPU/types.hpp"
#include "gpu_shaders.hpp"
#include "gpu_shaders_3d.hpp"
#include <cstdint>

namespace fizmo {
namespace windows {
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kDraw2DVert, sizeof(::fizmo::windows::detail::gfx::kDraw2DVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kDraw2DFrag, sizeof(::fizmo::windows::detail::gfx::kDraw2DFrag) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kPresentVert, sizeof(::fizmo::windows::detail::gfx::kPresentVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kPresentFrag, sizeof(::fizmo::windows::detail::gfx::kPresentFrag) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kUpscaleFrag, sizeof(::fizmo::windows::detail::gfx::kUpscaleFrag) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kMesh3DVert, sizeof(::fizmo::windows::detail::gfx::kMesh3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kMesh3DVert;
    d.glsl_pieces   = sizeof(kMesh3DVert) / sizeof(kMesh3DVert[0]);
    d.glsl_version  = kMesh3DVertVersion;
    d.glsl_bindings = { kMesh3DVertBindings, 1 };
    d.name          = "mesh3d.vert";
    return d;
}

inline constexpr int kMesh3DFragVersion = 410;

inline constexpr const char* kMesh3DFrag[] = {
R"fizmo_glsl(const vec3 _1031[8] = vec3[](vec3(1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(-1.0), vec3(1.0, -1.0, 1.0), vec3(-1.0, 1.0, 1.0));

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
} scene;

uniform sampler2DShadow sun_map;
uniform sampler2D sun_depth;
uniform sampler2DShadow sun_map_b;
uniform sampler2D sun_depth_b;
uniform samplerCubeShadow point_maps[8];
uniform sampler3D volume_light;
uniform sampler2D tex;
uniform sampler2D scene_color;
uniform sampler2D scene_depth;
uniform sampler2D planar_maps[2];

layout(location = 5) in vec2 v_uv;
layout(location = 3) flat in uint v_flags;
layout(location = 6) flat in uvec2 v_pbr;
layout(location = 0) in vec3 v_world;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_light;
layout(location = 4) in vec4 v_color;
layout(location = 0) out vec4 out_color;

mat4 spvWorkaroundRowMajor(mat4 wrap) { return wrap; }

float light_curve(float level)
{
    float k = max(scene.block_color.w, 1.0);
    return level / (k - ((k - 1.0) * level));
}

float star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.98980045318603515625, 78.233001708984375, 37.71900177001953125))) * 43758.546875);
}

vec3 sky_radiance(vec3 dir, bool disks)
{
    float h = dir.z;
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
        vec3 param = floor(dir * 400.0);
        float star = step(0.99849998950958251953125, star_hash(param));
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
        vec3 _470 = vec3(uv + (vogel(param_4, param_5, param_6) * radius), ref);
        lit += texture(map, vec3(_470.xy, _470.z));
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
            vec3 _329 = vec3(uv + ((vec2(float(x), float(y)) - vec2(1.5)) * texel), ref);
            lit += texture(map, vec3(_329.xy, _329.z));
        }
    }
    return lit / 16.0;
}

float sun_sample(mat4 m, sampler2DShadow map, sampler2D depth, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    vec2 edge = abs(c.xy);
    float reach = max(edge.x, edge.y);
    bool _502 = reach >= 1.0;
    bool _509;
    if (!_502)
    {
        _509 = c.z <= 0.0;
    }
    else
    {
        _509 = _502;
    }
    bool _516;
    if (!_509)
    {
        _516 = c.z >= 1.0;
    }
    else
    {
        _516 = _509;
    }
    if (_516)
    {
        return 1.0;
    }
    vec2 uv = (c.xy * 0.5) + vec2(0.5);
    float ref = c.z - scene.sun_shadow.x;
    float _534;
    if (reflection_pass())
    {
        vec3 _542 = vec3(uv, ref);
        _534 = texture(map, vec3(_542.xy, _542.z));
    }
    else
    {
        float _551;
        if ((scene.counts.y & 512) != 0)
        {
            vec2 param = uv;
            float param_1 = ref;
            _551 = sun_soft(map, depth, param, param_1);
        }
        else
        {
            vec2 param_2 = uv;
            float param_3 = ref;
            _551 = sun_grid(map, param_2, param_3);
        }
        _534 = _551;
    }
    float lit = _534;
    return mix(lit, 1.0, smoothstep(0.85000002384185791015625, 1.0, reach));
}

float capsule_lit(vec3 p, vec3 to_light, float reach, float spread, float source)
{
    int count = min(int(scene.capsules.x), 4);
    float lit = 1.0;
    vec3 ray = to_light * reach;
    float rr = dot(ray, ray);
    float _629;
    float _685;
    float _703;
    for (int i = 0; i < count; i++)
    {
        vec3 a = scene.capsule_a[i].xyz;
        float r = scene.capsule_a[i].w;
        vec3 axis = scene.capsule_b[i].xyz - a;
        vec3 w = p - a;
        float aa = dot(axis, axis);
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _629 = clamp(dot(w, axis) / aa, 0.0, 1.0);
        }
        else
        {
            _629 = 0.0;
        }
        float own = _629;
        vec3 out_dir = w - (axis * own);
        bool _651 = length(out_dir) < (r + 0.0500000007450580596923828125);
        bool _658;
        if (_651)
        {
            _658 = dot(out_dir, to_light) >= 0.0;
        }
        else
        {
            _658 = _651;
        }
        if (_658)
        {
            continue;
        }
        float b = dot(ray, axis);
        float c = dot(ray, w);
        float f = dot(axis, w);
        float denom = (rr * aa) - (b * b);
        if (denom > 9.9999997473787516355514526367188e-05)
        {
            _685 = clamp(((b * f) - (c * aa)) / denom, 0.0, 1.0);
        }
        else
        {
            _685 = 0.0;
        }
        float s = _685;
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _703 = ((b * s) + f) / aa;
        }
        else
        {
            _703 = 0.0;
        }
        float t = _703;
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

float spot_factor(vec4 spot, vec3 dir)
{
    if (spot.w < (-2.0))
    {
        return 1.0;
    }
    float sharp = length(spot.xyz);
    return clamp((dot(dir, spot.xyz / vec3(sharp)) - spot.w) * sharp, 0.0, 1.0);
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

)fizmo_glsl",
R"fizmo_glsl(float point_visibility(int slot, vec3 light, vec3 d, float radius)
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
        vec4 param_8 = vec4(d + (_1031[i] * spread), ref);
        lit += point_tap(param_7, param_8);
    }
    return mix(1.0, lit / 8.0, scene.point_shadow.y) * body;
}

vec3 pbr_shade(vec3 world, vec3 normal, vec4 baked, vec3 albedo, float metallic, float roughness)
{
    float len = length(normal);
    vec3 v = normalize(-world);
    vec3 _2143;
    if (len > 9.9999999747524270787835121154785e-07)
    {
        _2143 = normal / vec3(len);
    }
    else
    {
        _2143 = v;
    }
    vec3 n = _2143;
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
    vec3 _2263;
    if ((scene.counts.y & 8) != 0)
    {
        vec3 param_4 = reflect(-v, n);
        bool param_5 = false;
        _2263 = sky_radiance(param_4, param_5);
    }
    else
    {
        _2263 = scene.sky_color.xyz;
    }
    vec3 env = _2263;
    color += ((((env * fa) * sky) * (1.0 - (r * 0.75))) + (((fa * from_block) * r) * 0.25));
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
        vec3 d = world - scene.point_pos[i].xyz;
        float radius = scene.point_pos[i].w;
        float dist2 = dot(d, d);
        if (dist2 >= (radius * radius))
        {
            continue;
        }
        float dist = sqrt(dist2);
        float fall = 1.0 - (dist / radius);
        vec3 dir = d / vec3(max(dist, 9.9999997473787516355514526367188e-05));
        if (dot(n, -dir) <= 0.0)
        {
            continue;
        }
        vec4 param_14 = scene.point_spot[i];
        vec3 param_15 = dir;
        float cone = spot_factor(param_14, param_15);
        if (cone <= 0.0)
        {
            continue;
        }
        float vis = 1.0;
        int slot = int(scene.point_color[i].w);
        bool _2430 = slot >= 0;
        bool _2437;
        if (_2430)
        {
            _2437 = (scene.counts.y & 4) != 0;
        }
        else
        {
            _2437 = _2430;
        }
        if (_2437)
        {
            vec4 map = scene.point_map[slot];
            int param_16 = slot;
            vec3 param_17 = map.xyz;
            vec3 param_18 = (world + (n * scene.point_shadow.z)) - map.xyz;
            float param_19 = map.w;
            vis = point_visibility(param_16, param_17, param_18, param_19);
        }
        vec3 param_20 = n;
        vec3 param_21 = v;
        vec3 param_22 = -dir;
        vec3 param_23 = diffuse;
        vec3 param_24 = f0;
        float param_25 = r;
        color += ((pbr_direct(param_20, param_21, param_22, param_23, param_24, param_25) * scene.point_color[i].xyz) * (((fall * fall) * cone) * vis));
    }
    return min(color, vec3(scene.params.y * 4.0));
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
    bool _1549 = abs(c.x) >= 1.0;
    bool _1557;
    if (!_1549)
    {
        _1557 = abs(c.y) >= 1.0;
    }
    else
    {
        _1557 = _1549;
    }
    bool _1564;
    if (!_1557)
    {
        _1564 = c.z <= 0.0;
    }
    else
    {
        _1564 = _1557;
    }
    bool _1571;
    if (!_1564)
    {
        _1571 = c.z >= 1.0;
    }
    else
    {
        _1571 = _1564;
    }
    if (_1571)
    {
        return 1.0;
    }
    vec3 _1588 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_1588.xy, _1588.z));
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
    float _1799;
    if ((scene.counts.y & 8192) != 0)
    {
        vec3 param = dir;
        float param_1 = reach;
        _1799 = volume_lookup(param, param_1);
    }
    else
    {
        vec3 param_2 = dir;
        float param_3 = reach;
        _1799 = march_light(param_2, param_3);
    }
    float lit = _1799;
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
    vec3 _1987;
    if ((flags & 64u) != 0u)
    {
        vec3 param = rgb;
        vec3 _1992 = tone(param);
        _1987 = _1992;
    }
    else
    {
        vec3 param_1 = rgb;
        vec3 param_2 = world;
        vec3 _1998 = atmosphere(param_1, param_2);
        vec3 param_3 = _1998;
        vec3 _2000 = tone(param_3);
        _1987 = _2000;
    }
    return _1987;
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
    if (dot(scene.sun_color.xyz, vec3(1.0)) > 0.0)
    {
        float _1123;
        if (has_normal)
        {
            _1123 = max(dot(n, -scene.sun_dir.xyz), 0.0);
        }
        else
        {
            _1123 = 1.0;
        }
        float ndl = _1123;
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
    float _1218;
    for (int i = 0; i < count; i++)
    {
        vec3 d = world - scene.point_pos[i].xyz;
        float radius = scene.point_pos[i].w;
        float dist2 = dot(d, d);
        if (dist2 >= (radius * radius))
        {
            continue;
        }
        float dist = sqrt(dist2);
        float fall = 1.0 - (dist / radius);
        vec3 dir = d / vec3(max(dist, 9.9999997473787516355514526367188e-05));
        if (has_normal)
        {
            _1218 = max(dot(n, -dir), 0.0);
        }
        else
        {
            _1218 = 1.0;
)fizmo_glsl",
R"fizmo_glsl(        }
        float ndl_1 = _1218;
        if (ndl_1 <= 0.0)
        {
            continue;
        }
        vec4 param_6 = scene.point_spot[i];
        vec3 param_7 = dir;
        float cone = spot_factor(param_6, param_7);
        if (cone <= 0.0)
        {
            continue;
        }
        float vis = 1.0;
        int slot = int(scene.point_color[i].w);
        bool _1255 = slot >= 0;
        bool _1262;
        if (_1255)
        {
            _1262 = (scene.counts.y & 4) != 0;
        }
        else
        {
            _1262 = _1255;
        }
        if (_1262)
        {
            vec4 map = scene.point_map[slot];
            int param_8 = slot;
            vec3 param_9 = map.xyz;
            vec3 param_10 = (world + (n * scene.point_shadow.z)) - map.xyz;
            float param_11 = map.w;
            vis = point_visibility(param_8, param_9, param_10, param_11);
        }
        total += (scene.point_color[i].xyz * ((((fall * fall) * ndl_1) * cone) * vis));
    }
    return clamp(max(total, vec3(scene.sky_color.w)), vec3(0.0), vec3(scene.params.y));
}

vec3 shade(vec3 world, vec3 normal, vec4 baked, uint flags)
{
    if ((flags & 1u) == 0u)
    {
        return vec3(1.0);
    }
    float len = length(normal);
    bool has_normal = len > 9.9999999747524270787835121154785e-07;
    vec3 _1334;
    if (has_normal)
    {
        _1334 = normal / vec3(len);
    }
    else
    {
        _1334 = vec3(0.0);
    }
    vec3 n = _1334;
    if ((scene.counts.y & 1) == 0)
    {
        bool _1351 = !has_normal;
        bool _1358;
        if (!_1351)
        {
            _1358 = (flags & 4u) == 0u;
        }
        else
        {
            _1358 = _1351;
        }
        if (_1358)
        {
            return vec3(1.0);
        }
        return vec3(scene.legacy.w + max(dot(n, -scene.legacy.xyz), 0.0));
    }
    vec3 param = world;
    vec3 param_1 = n;
    vec4 param_2 = baked;
    bool param_3 = has_normal;
    return scene_light(param, param_1, param_2, param_3);
}

void main()
{
    vec4 t = texture(tex, v_uv);
    bool _2514 = (v_flags & 2048u) != 0u;
    bool _2521;
    if (_2514)
    {
        _2521 = (scene.counts.y & 1) != 0;
    }
    else
    {
        _2521 = _2514;
    }
    if (_2521)
    {
        vec4 params = unpackUnorm4x8(v_pbr.x);
        vec3 emissive = (unpackUnorm4x8(v_pbr.y).xyz * params.z) * 16.0;
        vec3 param = v_world;
        vec3 param_1 = v_normal;
        vec4 param_2 = v_light;
        vec3 param_3 = t.xyz * v_color.xyz;
        float param_4 = params.x;
        float param_5 = params.y;
        vec3 lit = (pbr_shade(param, param_1, param_2, param_3, param_4, param_5) * params.w) + (emissive * t.w);
        vec3 param_6 = lit;
        vec3 param_7 = v_world;
        uint param_8 = v_flags;
        out_color = vec4(finish(param_6, param_7, param_8) * v_color.w, t.w * v_color.w);
        return;
    }
    vec3 param_9 = v_world;
    vec3 param_10 = v_normal;
    vec4 param_11 = v_light;
    uint param_12 = v_flags;
    vec3 light = shade(param_9, param_10, param_11, param_12);
    vec3 param_13 = (t.xyz * v_color.xyz) * light;
    vec3 param_14 = v_world;
    uint param_15 = v_flags;
    out_color = vec4(finish(param_13, param_14, param_15) * v_color.w, t.w * v_color.w);
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
    { "tex", ::fizmo::gpu::GlslResource::Texture, 0, 0, 1 },
    { "scene_color", ::fizmo::gpu::GlslResource::Texture, 1, 3, 1 },
    { "scene_depth", ::fizmo::gpu::GlslResource::Texture, 1, 4, 1 },
    { "planar_maps", ::fizmo::gpu::GlslResource::Texture, 1, 8, 2 },
    { "SceneLight", ::fizmo::gpu::GlslResource::UniformBlock, 1, 0, 1 },
};

inline ::fizmo::gpu::ShaderDesc mesh3d_frag() noexcept {
    ::fizmo::gpu::ShaderDesc d;
    d.stage         = ::fizmo::gpu::ShaderStage::Fragment;
    d.spirv         = { ::fizmo::windows::detail::gfx::kMesh3DFrag, sizeof(::fizmo::windows::detail::gfx::kMesh3DFrag) / sizeof(std::uint32_t) };
    d.glsl          = kMesh3DFrag;
    d.glsl_pieces   = sizeof(kMesh3DFrag) / sizeof(kMesh3DFrag[0]);
    d.glsl_version  = kMesh3DFragVersion;
    d.glsl_bindings = { kMesh3DFragBindings, 11 };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kLine3DVert, sizeof(::fizmo::windows::detail::gfx::kLine3DVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kLine3DFrag, sizeof(::fizmo::windows::detail::gfx::kLine3DFrag) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kQuad3DVert, sizeof(::fizmo::windows::detail::gfx::kQuad3DVert) / sizeof(std::uint32_t) };
    d.glsl          = kQuad3DVert;
    d.glsl_pieces   = sizeof(kQuad3DVert) / sizeof(kQuad3DVert[0]);
    d.glsl_version  = kQuad3DVertVersion;
    d.glsl_bindings = { kQuad3DVertBindings, 1 };
    d.name          = "quad3d.vert";
    return d;
}

inline constexpr int kQuad3DFragVersion = 410;

inline constexpr const char* kQuad3DFrag[] = {
R"fizmo_glsl(const vec3 _1166[8] = vec3[](vec3(1.0), vec3(-1.0, -1.0, 1.0), vec3(-1.0, 1.0, -1.0), vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, -1.0), vec3(-1.0), vec3(1.0, -1.0, 1.0), vec3(-1.0, 1.0, 1.0));
const float _2188[3] = float[](0.0, 0.5, -0.699999988079071044921875);
const float _2226[3] = float[](1.0, 1.37000000476837158203125, 1.83000004291534423828125);
const float _2235[3] = float[](1.0, 0.4000000059604644775390625, 0.180000007152557373046875);
const vec3 _2311[4] = vec3[](vec3(1.0, 0.3499999940395355224609375, 1.0), vec3(-0.60000002384185791015625, 1.0, 1.7000000476837158203125), vec3(0.300000011920928955078125, -1.0, 2.900000095367431640625), vec3(-1.0, -0.20000000298023223876953125, 4.30000019073486328125));
const vec3 _3288[6] = vec3[](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0));

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
        vec3 _608 = vec3(uv + (vogel(param_4, param_5, param_6) * radius), ref);
        lit += texture(map, vec3(_608.xy, _608.z));
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
            vec3 _469 = vec3(uv + ((vec2(float(x), float(y)) - vec2(1.5)) * texel), ref);
            lit += texture(map, vec3(_469.xy, _469.z));
        }
    }
    return lit / 16.0;
}

float sun_sample(mat4 m, sampler2DShadow map, sampler2D depth, vec3 p)
{
    vec4 c = m * vec4(p, 1.0);
    vec2 edge = abs(c.xy);
    float reach = max(edge.x, edge.y);
    bool _640 = reach >= 1.0;
    bool _647;
    if (!_640)
    {
        _647 = c.z <= 0.0;
    }
    else
    {
        _647 = _640;
    }
    bool _654;
    if (!_647)
    {
        _654 = c.z >= 1.0;
    }
    else
    {
        _654 = _647;
    }
    if (_654)
    {
        return 1.0;
    }
    vec2 uv = (c.xy * 0.5) + vec2(0.5);
    float ref = c.z - scene.sun_shadow.x;
    float _672;
    if (reflection_pass())
    {
        vec3 _680 = vec3(uv, ref);
        _672 = texture(map, vec3(_680.xy, _680.z));
    }
    else
    {
        float _689;
        if ((scene.counts.y & 512) != 0)
        {
            vec2 param = uv;
            float param_1 = ref;
            _689 = sun_soft(map, depth, param, param_1);
        }
        else
        {
            vec2 param_2 = uv;
            float param_3 = ref;
            _689 = sun_grid(map, param_2, param_3);
        }
        _672 = _689;
    }
    float lit = _672;
    return mix(lit, 1.0, smoothstep(0.85000002384185791015625, 1.0, reach));
}

float capsule_lit(vec3 p, vec3 to_light, float reach, float spread, float source)
{
    int count = min(int(scene.capsules.x), 4);
    float lit = 1.0;
    vec3 ray = to_light * reach;
    float rr = dot(ray, ray);
    float _766;
    float _821;
    float _839;
    for (int i = 0; i < count; i++)
    {
        vec3 a = scene.capsule_a[i].xyz;
        float r = scene.capsule_a[i].w;
        vec3 axis = scene.capsule_b[i].xyz - a;
        vec3 w = p - a;
        float aa = dot(axis, axis);
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _766 = clamp(dot(w, axis) / aa, 0.0, 1.0);
        }
        else
        {
            _766 = 0.0;
        }
        float own = _766;
        vec3 out_dir = w - (axis * own);
        bool _787 = length(out_dir) < (r + 0.0500000007450580596923828125);
        bool _794;
        if (_787)
        {
            _794 = dot(out_dir, to_light) >= 0.0;
        }
        else
        {
            _794 = _787;
        }
        if (_794)
        {
            continue;
        }
        float b = dot(ray, axis);
        float c = dot(ray, w);
        float f = dot(axis, w);
        float denom = (rr * aa) - (b * b);
        if (denom > 9.9999997473787516355514526367188e-05)
        {
            _821 = clamp(((b * f) - (c * aa)) / denom, 0.0, 1.0);
        }
        else
        {
            _821 = 0.0;
        }
        float s = _821;
        if (aa > 9.9999997473787516355514526367188e-05)
        {
            _839 = ((b * s) + f) / aa;
        }
        else
        {
            _839 = 0.0;
        }
        float t = _839;
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

float spot_factor(vec4 spot, vec3 dir)
{
    if (spot.w < (-2.0))
    {
        return 1.0;
    }
    float sharp = length(spot.xyz);
    return clamp((dot(dir, spot.xyz / vec3(sharp)) - spot.w) * sharp, 0.0, 1.0);
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
        vec4 param_8 = vec4(d + (_1166[i] * spread), ref);
        lit += point_tap(param_7, param_8);
    }
    return mix(1.0, lit / 8.0, scene.point_shadow.y) * body;
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
    if (dot(scene.sun_color.xyz, vec3(1.0)) > 0.0)
    {
        float _1258;
        if (has_normal)
        {
            _1258 = max(dot(n, -scene.sun_dir.xyz), 0.0);
        }
        else
        {
            _1258 = 1.0;
        }
        float ndl = _1258;
        if (ndl > 0.0)
        {
            float exposure = pow(baked.w, scene.sun_color.w);
            if (exposure > 0.0)
)fizmo_glsl",
R"fizmo_glsl(            {
                vec3 param_4 = world;
                vec3 param_5 = n;
                total += (((scene.sun_color.xyz * ndl) * exposure) * sun_visibility(param_4, param_5));
            }
        }
    }
    int count = min(scene.counts.x, 64);
    float _1353;
    for (int i = 0; i < count; i++)
    {
        vec3 d = world - scene.point_pos[i].xyz;
        float radius = scene.point_pos[i].w;
        float dist2 = dot(d, d);
        if (dist2 >= (radius * radius))
        {
            continue;
        }
        float dist = sqrt(dist2);
        float fall = 1.0 - (dist / radius);
        vec3 dir = d / vec3(max(dist, 9.9999997473787516355514526367188e-05));
        if (has_normal)
        {
            _1353 = max(dot(n, -dir), 0.0);
        }
        else
        {
            _1353 = 1.0;
        }
        float ndl_1 = _1353;
        if (ndl_1 <= 0.0)
        {
            continue;
        }
        vec4 param_6 = scene.point_spot[i];
        vec3 param_7 = dir;
        float cone = spot_factor(param_6, param_7);
        if (cone <= 0.0)
        {
            continue;
        }
        float vis = 1.0;
        int slot = int(scene.point_color[i].w);
        bool _1390 = slot >= 0;
        bool _1397;
        if (_1390)
        {
            _1397 = (scene.counts.y & 4) != 0;
        }
        else
        {
            _1397 = _1390;
        }
        if (_1397)
        {
            vec4 map = scene.point_map[slot];
            int param_8 = slot;
            vec3 param_9 = map.xyz;
            vec3 param_10 = (world + (n * scene.point_shadow.z)) - map.xyz;
            float param_11 = map.w;
            vis = point_visibility(param_8, param_9, param_10, param_11);
        }
        total += (scene.point_color[i].xyz * ((((fall * fall) * ndl_1) * cone) * vis));
    }
    return clamp(max(total, vec3(scene.sky_color.w)), vec3(0.0), vec3(scene.params.y));
}

vec3 shade(vec3 world, vec3 normal, vec4 baked, uint flags)
{
    if ((flags & 1u) == 0u)
    {
        return vec3(1.0);
    }
    float len = length(normal);
    bool has_normal = len > 9.9999999747524270787835121154785e-07;
    vec3 _1469;
    if (has_normal)
    {
        _1469 = normal / vec3(len);
    }
    else
    {
        _1469 = vec3(0.0);
    }
    vec3 n = _1469;
    if ((scene.counts.y & 1) == 0)
    {
        bool _1486 = !has_normal;
        bool _1493;
        if (!_1486)
        {
            _1493 = (flags & 4u) == 0u;
        }
        else
        {
            _1493 = _1486;
        }
        if (_1493)
        {
            return vec3(1.0);
        }
        return vec3(scene.legacy.w + max(dot(n, -scene.legacy.xyz), 0.0));
    }
    vec3 param = world;
    vec3 param_1 = n;
    vec4 param_2 = baked;
    bool param_3 = has_normal;
    return scene_light(param, param_1, param_2, param_3);
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

float star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.98980045318603515625, 78.233001708984375, 37.71900177001953125))) * 43758.546875);
}

vec3 sky_radiance(vec3 dir, bool disks)
{
    float h = dir.z;
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
        vec3 param = floor(dir * 400.0);
        float star = step(0.99849998950958251953125, star_hash(param));
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
    bool _1683 = abs(c.x) >= 1.0;
    bool _1691;
    if (!_1683)
    {
        _1691 = abs(c.y) >= 1.0;
    }
    else
    {
        _1691 = _1683;
    }
    bool _1698;
    if (!_1691)
    {
        _1698 = c.z <= 0.0;
    }
    else
    {
        _1698 = _1691;
    }
    bool _1705;
    if (!_1698)
    {
        _1705 = c.z >= 1.0;
    }
    else
    {
        _1705 = _1698;
    }
    if (_1705)
    {
        return 1.0;
    }
    vec3 _1722 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_1722.xy, _1722.z));
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
    float _1933;
    if ((scene.counts.y & 8192) != 0)
    {
        vec3 param = dir;
        float param_1 = reach;
        _1933 = volume_lookup(param, param_1);
    }
    else
    {
        vec3 param_2 = dir;
        float param_3 = reach;
        _1933 = march_light(param_2, param_3);
    }
    float lit = _1933;
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
    vec3 _2137;
    if ((flags & 64u) != 0u)
    {
        vec3 param = rgb;
        vec3 _2142 = tone(param);
        _2137 = _2142;
    }
    else
    {
        vec3 param_1 = rgb;
        vec3 param_2 = world;
        vec3 _2148 = atmosphere(param_1, param_2);
        vec3 param_3 = _2148;
        vec3 _2150 = tone(param_3);
        _2137 = _2150;
    }
    return _2137;
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
        float c = cos(_2188[i]);
        float s = sin(_2188[i]);
        vec2 d = vec2((base.x * c) - (base.y * s), (base.x * s) + (base.y * c));
        float k = scene.swell.y * _2226[i];
        grad += (d * ((_2235[i] * k) * cos((k * dot(d, p)) + scene.swell_phase[i])));
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
        vec2 d = normalize(_2311[i].xy);
        float k = _2311[i].z * scene.waves.y;
        grad += (d * ((amp * k) * cos((dot(d, p.xy) * k) + (t * _2311[i].z))));
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
        bool _298 = any(lessThan(uv, r.xy));
        bool _307;
        if (!_298)
        {
            _307 = any(greaterThan(uv, r.zw));
        }
        else
        {
            _307 = _298;
        }
        if (_307)
        {
            continue;
        }
        bool _325 = abs(dot(scene.planes[i].xyz, world) - scene.planes[i].w) < 0.0500000007450580596923828125;
        bool _336;
        if (_325)
        {
            _336 = dot(n, scene.planes[i].xyz) > 0.89999997615814208984375;
        }
        else
        {
            _336 = _325;
        }
        if (_336)
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
)fizmo_glsl",
R"fizmo_glsl(    vec4 c = spvWorkaroundRowMajor(scene.view_proj) * vec4(p, 1.0);
    w = c.w;
    vec2 ndc = c.xy / vec2(max(c.w, 9.9999997473787516355514526367188e-06));
    return scene.screen.xy + (((ndc * 0.5) + vec2(0.5)) * scene.screen.zw);
}

bool on_screen(vec2 uv)
{
    bool _2445 = all(greaterThanEqual(uv, scene.screen.xy));
    bool _2458;
    if (_2445)
    {
        _2458 = all(lessThanEqual(uv, scene.screen.xy + scene.screen.zw));
    }
    else
    {
        _2458 = _2445;
    }
    return _2458;
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
    int steps = max(int(scene.target.z), 1);
    float reach = scene.target.w;
    float prev = 0.0;
    float param_1;
    float param_5;
    float param_8;
    for (int i = 0; i < steps; i++)
    {
        float a = float(i + 1) / float(steps);
        float t = (reach * a) * a;
        vec3 param = world + (r * t);
        vec2 _2545 = project_uv(param, param_1);
        float w = param_1;
        vec2 uv = _2545;
        bool _2548 = w <= 0.0500000007450580596923828125;
        bool _2556;
        if (!_2548)
        {
            vec2 param_2 = uv;
            _2556 = !on_screen(param_2);
        }
        else
        {
            _2556 = _2548;
        }
        if (_2556)
        {
            break;
        }
        float ray = length(world + (r * t));
        vec2 param_3 = uv;
        float hit = length(scene_point(param_3));
        bool _2574 = ray > hit;
        bool _2586;
        if (_2574)
        {
            _2586 = (ray - hit) < (max(t - prev, 0.5) * 1.5);
        }
        else
        {
            _2586 = _2574;
        }
        if (_2586)
        {
            float lo = prev;
            float hi = t;
            for (int k = 0; k < 5; k++)
            {
                float mid = 0.5 * (lo + hi);
                vec3 param_4 = world + (r * mid);
                vec2 _2614 = project_uv(param_4, param_5);
                w = param_5;
                vec2 m = _2614;
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
            vec2 _2641 = project_uv(param_7, param_8);
            w = param_8;
            uv = _2641;
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
    vec3 _2121 = atmosphere(param, param_1);
    vec3 param_2 = _2121;
    vec3 _2123 = tone(param_2);
    return _2123;
}

vec4 surface_output(vec3 albedo, vec3 light, float alpha, vec3 world, vec3 n, uint flags, vec3 wave_pos, vec2 flow)
{
    vec3 rgb = albedo * light;
    bool liquid = (flags & 16u) != 0u;
    bool glossy = (flags & 8u) != 0u;
    bool mirror = (flags & 32u) != 0u;
    bool _2747 = ((!liquid) && (!glossy)) && (!mirror);
    bool _2755;
    if (!_2747)
    {
        _2755 = (scene.counts.y & 1) == 0;
    }
    else
    {
        _2755 = _2747;
    }
    if (_2755)
    {
        vec3 param = rgb;
        vec3 param_1 = world;
        uint param_2 = flags;
        return vec4(finish(param, param_1, param_2) * alpha, alpha);
    }
    vec3 v = normalize(-world);
    vec3 _2782;
    if (dot(n, v) < 0.0)
    {
        _2782 = -n;
    }
    else
    {
        _2782 = n;
    }
    vec3 facing = _2782;
    vec3 drifted = wave_pos - vec3(((flow * scene.sky_params.y) * scene.waves.z) * 1.0, 0.0);
    bool _2812;
    if (liquid)
    {
        _2812 = abs(n.z) > 0.5;
    }
    else
    {
        _2812 = liquid;
    }
    vec3 _2813;
    if (_2812)
    {
        vec3 param_3 = facing;
        vec2 param_4 = world.xy;
        uint param_5 = flags;
        vec3 param_6 = swell_normal(param_3, param_4, param_5);
        vec3 param_7 = drifted;
        _2813 = wave_normal(param_6, param_7);
    }
    else
    {
        _2813 = facing;
    }
    vec3 sn = _2813;
    float f0 = liquid ? 0.0199999995529651641845703125 : 0.039999999105930328369140625;
    float fresnel = (f0 + ((1.0 - f0) * pow(1.0 - max(dot(sn, v), 0.0), 5.0))) * scene.waves.w;
    vec3 r = reflect(-v, sn);
    bool underwater = (scene.counts.y & 64) != 0;
    bool from_below = liquid && underwater;
    int _2867;
    if (from_below)
    {
        _2867 = -1;
    }
    else
    {
        vec3 param_8 = world;
        vec3 param_9 = facing;
        _2867 = planar_index(param_8, param_9);
    }
    int plane = _2867;
    vec2 screen_uv = gl_FragCoord.xy * scene.target.xy;
    vec4 _2886;
    if (from_below)
    {
        _2886 = vec4(0.0);
    }
    else
    {
        vec4 _2892;
        if (plane >= 0)
        {
            int param_10 = plane;
            vec2 param_11 = screen_uv + ((sn.xy - facing.xy) * scene.plane_info.y);
            _2892 = vec4(planar_sample(param_10, param_11), 1.0);
        }
        else
        {
            vec3 param_12 = world;
            vec3 param_13 = r;
            _2892 = screen_reflection(param_12, param_13);
        }
        _2886 = _2892;
    }
    vec4 hit = _2886;
    vec3 _2923;
    if (from_below)
    {
        _2923 = scene.medium_color.xyz;
    }
    else
    {
        vec3 param_14 = r;
        bool param_15 = false;
        _2923 = sky_radiance(param_14, param_15) * (1.0 - hit.w);
    }
    vec3 sky = _2923;
    vec3 half_vec = normalize(v - scene.sun_dir.xyz);
    float _2948;
    if (from_below)
    {
        _2948 = 0.0;
    }
    else
    {
        vec3 param_16 = world + (facing * scene.sun_shadow.y);
        _2948 = (pow(max(dot(sn, half_vec), 0.0), scene.gloss.x) * scene.gloss.y) * sun_lit_at(param_16);
    }
    float spec = _2948;
    vec3 glare = scene.sun_color.xyz * spec;
    vec3 param_17 = world;
    float keep = veil(param_17);
    if (mirror)
    {
        vec3 param_18 = rgb * mix(1.0, 0.0599999986588954925537109375, hit.w);
        vec3 param_19 = world;
        vec3 own = finish(param_18, param_19);
        return vec4(own + ((((hit.xyz * hit.w) * albedo) + (glare * (1.0 - hit.w))) * keep), 1.0);
    }
    bool _3026;
    if (liquid)
    {
        _3026 = (scene.counts.y & 128) != 0;
    }
    else
    {
        _3026 = liquid;
    }
    bool _3033;
    if (_3026)
    {
        _3033 = scene.water.w > 0.0;
    }
    else
    {
        _3033 = _3026;
    }
    if (_3033)
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
        bool _3087 = !on_screen(param_21);
        bool _3100;
        if (!_3087)
        {
            vec2 param_22 = uv;
            _3100 = dot(scene_point(param_22) - world, facing) > (-0.0500000007450580596923828125);
        }
        else
        {
            _3100 = _3087;
        }
        if (_3100)
        {
            uv = uv0;
        }
        float _3106;
        if (underwater)
        {
            _3106 = 0.0;
        }
        else
        {
            vec2 param_23 = uv;
            _3106 = max(length(scene_point(param_23)) - surface, 0.0);
        }
        float thick = _3106;
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

void main()
{
    vec3 rgb = floor((v_color.xyz * 255.0) + vec3(0.5));
    bool flowing = (v_flags & 128u) != 0u;
    uint _3254;
    if (flowing)
    {
        _3254 = 0u;
    }
    else
    {
        _3254 = v_params.x;
    }
    uint amount = _3254;
    bool _3265 = amount > 0u;
    bool _3272;
    if (_3265)
    {
        _3272 = v_params.y < 6u;
    }
    else
    {
        _3272 = _3265;
    }
    if (_3272)
    {
        ivec3 c = ivec3(floor(v_local - (_3288[v_params.y] * 0.5))) + v_origin;
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
    vec3 light = shade(param, param_1, param_2, param_3);
    vec3 _3380;
    if (length(v_normal) > 9.9999999747524270787835121154785e-07)
    {
        _3380 = normalize(v_normal);
    }
    else
    {
        _3380 = vec3(0.0, 0.0, 1.0);
    }
    vec3 n = _3380;
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kQuad3DFrag, sizeof(::fizmo::windows::detail::gfx::kQuad3DFrag) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kBatch3DVert, sizeof(::fizmo::windows::detail::gfx::kBatch3DVert) / sizeof(std::uint32_t) };
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
    v_pbr = uvec2(0u);
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kInstanced3DVert, sizeof(::fizmo::windows::detail::gfx::kInstanced3DVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kShadowMesh3DVert, sizeof(::fizmo::windows::detail::gfx::kShadowMesh3DVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kShadowInstanced3DVert, sizeof(::fizmo::windows::detail::gfx::kShadowInstanced3DVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kShadowQuad3DVert, sizeof(::fizmo::windows::detail::gfx::kShadowQuad3DVert) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kShadowPoint3DFrag, sizeof(::fizmo::windows::detail::gfx::kShadowPoint3DFrag) / sizeof(std::uint32_t) };
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kSky3DVert, sizeof(::fizmo::windows::detail::gfx::kSky3DVert) / sizeof(std::uint32_t) };
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

float star_hash(vec3 cell)
{
    return fract(sin(dot(cell, vec3(12.98980045318603515625, 78.233001708984375, 37.71900177001953125))) * 43758.546875);
}

vec3 sky_radiance(vec3 dir, bool disks)
{
    float h = dir.z;
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
        vec3 param = floor(dir * 400.0);
        float star = step(0.99849998950958251953125, star_hash(param));
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
    bool _279 = abs(c.x) >= 1.0;
    bool _288;
    if (!_279)
    {
        _288 = abs(c.y) >= 1.0;
    }
    else
    {
        _288 = _279;
    }
    bool _295;
    if (!_288)
    {
        _295 = c.z <= 0.0;
    }
    else
    {
        _295 = _288;
    }
    bool _302;
    if (!_295)
    {
        _302 = c.z >= 1.0;
    }
    else
    {
        _302 = _295;
    }
    if (_302)
    {
        return 1.0;
    }
    vec3 _320 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_320.xy, _320.z));
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
    float _548;
    if ((scene.counts.y & 8192) != 0)
    {
        vec3 param = dir;
        float param_1 = reach;
        _548 = volume_lookup(param, param_1);
    }
    else
    {
        vec3 param_2 = dir;
        float param_3 = reach;
        _548 = march_light(param_2, param_3);
    }
    float lit = _548;
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
    vec3 _708 = tone(param_4);
    out_color = vec4(_708, 1.0);
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kSky3DFrag, sizeof(::fizmo::windows::detail::gfx::kSky3DFrag) / sizeof(std::uint32_t) };
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
    vec3 _149 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return texture(map, vec3(_149.xy, _149.z));
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
    bool _372 = (scene.counts.y & 1024) == 0;
    bool _381;
    if (!_372)
    {
        _381 = (scene.counts.y & 128) == 0;
    }
    else
    {
        _381 = _372;
    }
    if (_381)
    {
        discard;
    }
    vec3 toward = -scene.sun_dir.xyz;
    vec3 param = toward * 1000.0;
    float param_1;
    vec2 _397 = project_uv(param, param_1);
    float w = param_1;
    vec2 sun_uv = _397;
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
    float _534;
    if (depth >= 1.0)
    {
        _534 = 96.0;
    }
    else
    {
        vec2 param_2 = uv;
        _534 = min(length(scene_point(param_2)), 96.0);
    }
    float reach = _534;
    vec3 param_3 = dir;
    float param_4 = reach;
    float air = (reach / 96.0) * lit_air(param_3, param_4);
    float shaft = (pow(facing, scene.glow.y) * min((open * (1.0 - open)) * 4.0, 1.0)) * air;
    vec3 param_5 = (scene.sun_color.xyz * shaft) * scene.rays.x;
    vec3 _580 = tone(param_5);
    out_color = vec4(_580, 0.0);
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kRays3DFrag, sizeof(::fizmo::windows::detail::gfx::kRays3DFrag) / sizeof(std::uint32_t) };
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
    vec3 _120 = vec3((c.xy * 0.5) + vec2(0.5), c.z - scene.sun_shadow.x);
    return textureLod(map, vec3(_120.xy, _120.z), 0.0);
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
    d.spirv         = { ::fizmo::windows::detail::gfx::kVolume3DComp, sizeof(::fizmo::windows::detail::gfx::kVolume3DComp) / sizeof(std::uint32_t) };
    d.glsl          = kVolume3DComp;
    d.glsl_pieces   = sizeof(kVolume3DComp) / sizeof(kVolume3DComp[0]);
    d.glsl_version  = kVolume3DCompVersion;
    d.glsl_bindings = { kVolume3DCompBindings, 11 };
    d.name          = "volume.comp";
    return d;
}

} // namespace glsl
} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_GL_SHADERS_HPP
