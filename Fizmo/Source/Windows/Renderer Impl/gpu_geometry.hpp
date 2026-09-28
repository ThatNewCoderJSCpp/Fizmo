#ifndef FIZMO_GPU_GEOMETRY_HPP
#define FIZMO_GPU_GEOMETRY_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {
namespace gpu {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2  operator+(Vec2 a, Vec2 b) noexcept { return { a.x + b.x, a.y + b.y }; }
inline Vec2  operator-(Vec2 a, Vec2 b) noexcept { return { a.x - b.x, a.y - b.y }; }
inline Vec2  operator*(Vec2 a, float s) noexcept { return { a.x * s, a.y * s }; }
inline float dot(Vec2 a, Vec2 b) noexcept { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) noexcept { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 a) noexcept { return std::sqrt(dot(a, a)); }

inline Vec2 normalize(Vec2 a) noexcept {
    const float len = length(a);
    return len > 1e-12f ? a * (1.0f / len) : Vec2{ 0.0f, 0.0f };
}

inline Vec2 left_normal(Vec2 dir) noexcept { return { -dir.y, dir.x }; }

enum class Cap  : std::uint8_t { Flat, Round, Square };
enum class Join : std::uint8_t { Miter, Round, Bevel };

constexpr float kPi       = 3.14159265358979323846f;
constexpr float kTwoPi    = 6.28318530717958647692f;
constexpr float kMiterLimit = 4.0f;   

inline void push_tri(std::vector<Vec2>& out, Vec2 a, Vec2 b, Vec2 c) {
    out.push_back(a);
    out.push_back(b);
    out.push_back(c);
}

inline void push_quad(std::vector<Vec2>& out, Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    push_tri(out, a, b, c);
    push_tri(out, a, c, d);
}

inline int arc_segments(float radius, float sweep) noexcept {
    const float r = std::max(radius, 0.5f);
    const float per_radian = std::sqrt(r) * 1.6f;
    const int n = static_cast<int>(std::ceil(std::abs(sweep) * per_radian));
    return std::max(2, std::min(n, 512));
}

inline void fan_arc(std::vector<Vec2>& out, Vec2 center, float radius, float a0, float a1) {
    const int n = arc_segments(radius, a1 - a0);
    Vec2 prev{ center.x + radius * std::cos(a0), center.y + radius * std::sin(a0) };

    for (int i = 1; i <= n; ++i) {
        const float a = a0 + (a1 - a0) * (static_cast<float>(i) / n);
        const Vec2 cur{ center.x + radius * std::cos(a), center.y + radius * std::sin(a) };
        push_tri(out, center, prev, cur);
        prev = cur;
    }
}

inline void ellipse_points(std::vector<Vec2>& out, Vec2 c, float rx, float ry, float a0, float a1) {
    const int n = arc_segments(std::max(rx, ry), a1 - a0);
    out.reserve(out.size() + static_cast<std::size_t>(n) + 1);

    for (int i = 0; i <= n; ++i) {
        const float a = a0 + (a1 - a0) * (static_cast<float>(i) / n);
        out.push_back({ c.x + rx * std::cos(a), c.y + ry * std::sin(a) });
    }
}

inline void fill_rect(std::vector<Vec2>& out, float x, float y, float w, float h) {
    push_quad(out, { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h });
}

inline void fill_fan(std::vector<Vec2>& out, const Vec2* pts, std::size_t n) {
    for (std::size_t i = 1; i + 1 < n; ++i) push_tri(out, pts[0], pts[i], pts[i + 1]);
}

inline void fill_center_fan(std::vector<Vec2>& out, Vec2 center, const Vec2* ring, std::size_t n, bool closed) {
    for (std::size_t i = 0; i + 1 < n; ++i) push_tri(out, center, ring[i], ring[i + 1]);
    if (closed && n > 2) push_tri(out, center, ring[n - 1], ring[0]);
}

namespace polygon_detail {

inline float signed_area(const Vec2* p, std::size_t n) noexcept {
    float a = 0.0f;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) a += cross(p[j], p[i]);
    return a * 0.5f;
}

inline bool point_in_triangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) noexcept {
    const float d1 = cross(b - a, p - a);
    const float d2 = cross(c - b, p - b);
    const float d3 = cross(a - c, p - c);
    const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

} // namespace polygon_detail

inline void fill_polygon(std::vector<Vec2>& out, const Vec2* pts, std::size_t n) {
    using namespace polygon_detail;
    if (n < 3) return;
    if (n == 3) { push_tri(out, pts[0], pts[1], pts[2]); return; }
    std::vector<std::size_t> idx(n);
    const bool ccw = signed_area(pts, n) > 0.0f;
    for (std::size_t i = 0; i < n; ++i) idx[i] = ccw ? i : (n - 1 - i);
    const std::size_t start = out.size();
    std::size_t guard = n * n + 8;

    while (idx.size() > 3 && guard-- > 0) {
        bool clipped = false;
        const std::size_t m = idx.size();

        for (std::size_t i = 0; i < m; ++i) {
            const Vec2 a = pts[idx[(i + m - 1) % m]];
            const Vec2 b = pts[idx[i]];
            const Vec2 c = pts[idx[(i + 1) % m]];
            if (cross(b - a, c - b) <= 0.0f) continue;          
            bool contains = false;

            for (std::size_t k = 0; k < m && !contains; ++k) {
                if (k == i || k == (i + m - 1) % m || k == (i + 1) % m) continue;
                contains = point_in_triangle(pts[idx[k]], a, b, c);
            }

            if (contains) continue;
            push_tri(out, a, b, c);
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }

        if (!clipped) {                                         
            out.resize(start);
            fill_fan(out, pts, n);
            return;
        }
    }

    if (idx.size() == 3) push_tri(out, pts[idx[0]], pts[idx[1]], pts[idx[2]]);
}

inline void stroke_polyline(
    std::vector<Vec2>& out, const Vec2* input, std::size_t count, bool closed,
    float width, Cap cap, Join join
) {
    if (count == 0 || width <= 0.0f) return;
    const float hw = width * 0.5f;
    thread_local std::vector<Vec2> pts;
    pts.clear();

    for (std::size_t i = 0; i < count; ++i) {
        if (pts.empty() || dot(input[i] - pts.back(), input[i] - pts.back()) > 1e-8f) pts.push_back(input[i]);
    }

    if (closed && pts.size() > 2 && dot(pts.front() - pts.back(), pts.front() - pts.back()) <= 1e-8f) pts.pop_back();
    const std::size_t n = pts.size();

    if (n == 1) {                                              
        if (cap == Cap::Round) fan_arc(out, pts[0], hw, 0.0f, kTwoPi);
        else fill_rect(out, pts[0].x - hw, pts[0].y - hw, width, width);
        return;
    }

    if (n == 2) closed = false;
    const std::size_t segs = closed ? n : n - 1;
    thread_local std::vector<Vec2>  dir, nrm;
    thread_local std::vector<float> len;
    dir.resize(segs); nrm.resize(segs); len.resize(segs);

    for (std::size_t s = 0; s < segs; ++s) {
        const Vec2 d = pts[(s + 1) % n] - pts[s];
        len[s] = length(d);
        dir[s] = d * (1.0f / len[s]);
        nrm[s] = left_normal(dir[s]);
    }

    struct Joint { Vec2 left_in, right_in, left_out, right_out; };
    thread_local std::vector<Joint> joints;
    joints.resize(n);

    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 p = pts[i];
        const bool has_prev = closed || i > 0;
        const bool has_next = closed || i + 1 < n;
        Joint& j = joints[i];

        if (!has_prev) {                                        
            const Vec2 d = dir[0], nn = nrm[0] * hw;
            const Vec2 base = cap == Cap::Square ? p - d * hw : p;
            j.left_out = base + nn; j.right_out = base - nn;

            if (cap == Cap::Round) {
                const float a = std::atan2(nn.y, nn.x);
                fan_arc(out, p, hw, a, a + kPi);                
            }
            continue;
        }
        if (!has_next) {                                        
            const Vec2 d = dir[segs - 1], nn = nrm[segs - 1] * hw;
            const Vec2 base = cap == Cap::Square ? p + d * hw : p;
            j.left_in = base + nn; j.right_in = base - nn;
            
            if (cap == Cap::Round) {
                const float a = std::atan2(-nn.y, -nn.x);
                fan_arc(out, p, hw, a, a + kPi);                
            }

            continue;
        }

        const std::size_t a_seg = (i + segs - 1) % segs;      
        const std::size_t b_seg = i % segs;                     
        const Vec2 na = nrm[a_seg], nb = nrm[b_seg];
        const float turn = cross(dir[a_seg], dir[b_seg]);
        const Vec2 m = na + nb;
        const float m2 = dot(m, m);

        if (std::abs(turn) < 1e-6f && dot(dir[a_seg], dir[b_seg]) > 0.0f) {   
            j.left_in = j.left_out = p + na * hw;
            j.right_in = j.right_out = p - na * hw;
            continue;
        }

        if (m2 < 1e-6f) {                                      
            j.left_in = p + na * hw; j.right_in = p - na * hw;
            j.left_out = p + nb * hw; j.right_out = p - nb * hw;
            if (join == Join::Round) fan_arc(out, p, hw, std::atan2(na.y, na.x), std::atan2(na.y, na.x) + kPi);
            continue;
        }

        const Vec2  miter = m * (2.0f * hw / m2);               
        const float ratio = 2.0f / std::sqrt(m2);               
        const float side  = turn > 0.0f ? -1.0f : 1.0f;         
        const float inner_len = hw * ratio;
        const bool  inner_ok  = inner_len <= std::min(len[a_seg], len[b_seg]) + hw;
        const Vec2  inner     = p - miter * side;
        const Vec2  inner_in  = inner_ok ? inner : p - na * (hw * side);
        const Vec2  inner_out = inner_ok ? inner : p - nb * (hw * side);
        Vec2 outer_in = p + na * (hw * side);
        Vec2 outer_out = p + nb * (hw * side);

        if (join == Join::Miter && ratio <= kMiterLimit) {
            outer_in = outer_out = p + miter * side;           
        } else {
            push_tri(out, inner_in, outer_in, p);
            push_tri(out, inner_out, p, outer_out);

            if (join == Join::Round) {
                const float a0 = std::atan2(outer_in.y - p.y, outer_in.x - p.x);
                const float a1 = std::atan2(outer_out.y - p.y, outer_out.x - p.x);
                float d = a1 - a0;
                while (d >  kPi) d -= kTwoPi;
                while (d < -kPi) d += kTwoPi;
                fan_arc(out, p, hw, a0, a0 + d);
            } else {
                push_tri(out, p, outer_in, outer_out);         
            }
        }

        if (side > 0.0f) {
            j.left_in = outer_in;  j.left_out = outer_out;
            j.right_in = inner_in; j.right_out = inner_out;
        } else {
            j.right_in = outer_in; j.right_out = outer_out;
            j.left_in = inner_in;  j.left_out = inner_out;
        }
    }

    for (std::size_t s = 0; s < segs; ++s) {
        const Joint& a = joints[s];
        const Joint& b = joints[(s + 1) % n];
        push_quad(out, a.left_out, b.left_in, b.right_in, a.right_out);
    }
}

} // namespace gpu
} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_GPU_GEOMETRY_HPP