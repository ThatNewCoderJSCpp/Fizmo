#version 450
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D tex_linear;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) flat in uint v_mode;
layout(location = 3) flat in vec4 v_grad;
layout(location = 4) in vec2 v_pos;
layout(location = 5) flat in vec4 v_rect;

layout(location = 0) out vec4 out_color;

void main() {
    vec2 uv = v_uv;
    if ((v_mode & 2u) != 0u) {
        vec2 size = vec2(textureSize(tex, 0));
        uv = clamp(uv * size, v_rect.xy, v_rect.zw) / size;
    }
    if ((v_mode & 1u) != 0u) out_color = texture(tex_linear, uv) * v_color;
    else out_color = texture(tex, uv) * v_color;
}
