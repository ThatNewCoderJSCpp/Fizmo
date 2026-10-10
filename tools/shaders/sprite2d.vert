#version 450
layout(location = 0) in vec4 in_x;
layout(location = 1) in vec4 in_y;
layout(location = 2) in vec4 in_uv;
layout(location = 3) in vec4 in_color;
layout(location = 4) in uint in_flags;
layout(location = 5) in uint in_rect_xy;
layout(location = 6) in uint in_rect_wh;

layout(push_constant) uniform Push { vec2 inv_half_size; } pc;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) flat out uint v_mode;
layout(location = 3) flat out vec4 v_grad;
layout(location = 4) out vec2 v_pos;
layout(location = 5) flat out vec4 v_rect;

void main() {
    int v = gl_VertexIndex % 6;
    int corner = v < 3 ? v : (v == 3 ? 0 : v - 2);
    vec2 pos = vec2(in_x[corner], in_y[corner]);
    v_uv = vec2(corner == 1 || corner == 2 ? in_uv.z : in_uv.x, corner >= 2 ? in_uv.w : in_uv.y);
    v_color = in_color;
    v_mode = in_flags;
    v_grad = vec4(0.0);
    v_pos = pos;
    vec2 lo = vec2(float(in_rect_xy & 65535u), float(in_rect_xy >> 16));
    vec2 hi = lo + vec2(float(in_rect_wh & 65535u), float(in_rect_wh >> 16));
    v_rect = vec4(lo + 0.5, hi - 0.5);
    gl_Position = vec4(pos * pc.inv_half_size - 1.0, 0.0, 1.0);
}
