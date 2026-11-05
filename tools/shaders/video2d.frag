#version 450
layout(set = 0, binding = 0) uniform sampler2D plane0;
layout(set = 0, binding = 1) uniform sampler2D plane1;
layout(set = 0, binding = 2) uniform sampler2D plane2;

layout(push_constant) uniform Push {
    vec2 inv_half_size;
    vec2 chroma_scale;
    vec4 coeffs;
    vec4 levels;
    vec4 siting;
    vec4 sizes;
} pc;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 0) out vec4 out_color;

float tap(sampler2D s, ivec2 p, ivec2 hi) {
    return texelFetch(s, clamp(p, ivec2(0), hi), 0).r;
}

float bilinear(sampler2D s, vec2 p, vec2 size) {
    ivec2 hi = max(ivec2(size) - ivec2(1), ivec2(0));
    vec2 b = floor(p);
    vec2 f = p - b;
    ivec2 i = ivec2(b);
    float top = mix(tap(s, i, hi), tap(s, i + ivec2(1, 0), hi), f.x);
    float bottom = mix(tap(s, i + ivec2(0, 1), hi), tap(s, i + ivec2(1, 1), hi), f.x);
    return mix(top, bottom, f.y) * pc.levels.w;
}

void main() {
    vec2 q = v_uv * pc.sizes.xy - 0.5;
    if (pc.siting.w < 0.5) q = floor(q + 0.5);
    float y = (bilinear(plane0, q, pc.sizes.xy) - pc.levels.x) * pc.levels.y;
    vec3 rgb = vec3(y);
    if (pc.siting.z > 0.5) {
        vec2 c = q * pc.chroma_scale + pc.siting.xy;
        float u = floor(bilinear(plane1, c, pc.sizes.zw) + 0.501953125);
        float v = floor(bilinear(plane2, c, pc.sizes.zw) + 0.501953125);
        if (pc.siting.z > 1.5) {
            rgb = vec3((v - pc.levels.x) * pc.levels.y, y, (u - pc.levels.x) * pc.levels.y);
        } else {
            u -= pc.levels.z;
            v -= pc.levels.z;
            rgb = vec3(y + pc.coeffs.x * v, y - pc.coeffs.y * u - pc.coeffs.z * v, y + pc.coeffs.w * u);
        }
    }
    out_color = vec4(clamp(rgb, 0.0, 1.0), 1.0) * v_color;
}
