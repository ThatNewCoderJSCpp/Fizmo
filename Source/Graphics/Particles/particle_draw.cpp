#include "fizmo_library.hpp"
#include "particle_draw.hpp"
#include "../particles_2d.hpp"
#include "../Renderer/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace particles {

namespace {

struct Axes {
    float rx = 1.0f, ry = 0.0f, rz = 0.0f;
    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    float fx = 0.0f, fy = 0.0f, fz = -1.0f;
};

Axes camera_axes(const CameraView& v) noexcept {
    Axes a;
    if (!v.active || v.is_2d) return a;
    a.rx = static_cast<float>(v.right.x); a.ry = static_cast<float>(v.right.y); a.rz = static_cast<float>(v.right.z);
    a.ux = static_cast<float>(v.up.x); a.uy = static_cast<float>(v.up.y); a.uz = static_cast<float>(v.up.z);
    a.fx = static_cast<float>(v.forward.x); a.fy = static_cast<float>(v.forward.y); a.fz = static_cast<float>(v.forward.z);
    return a;
}

bool wants_sort(const EmitterSettings& s) noexcept {
    if (s.sort == SortMode::None) return false;
    if (s.sort == SortMode::Auto) return s.blend != ParticleBlend::Additive;
    return true;
}

template <class T>
void grow(std::vector<T>& v, std::size_t n) {
    if (v.size() < n) v.resize(n + n / 4 + 64);
}

void sort_entries(std::size_t n, bool exact, const float* depth, std::uint32_t* order, std::uint32_t* tmp, std::uint16_t* key, std::uint16_t* key_tmp) {
    for (std::size_t k = 0; k < n; ++k) order[k] = static_cast<std::uint32_t>(k);
    if (exact) {
        std::sort(order, order + n, [depth](std::uint32_t a, std::uint32_t b) {
            if (depth[a] != depth[b]) return depth[a] > depth[b];
            return a < b;
        });
        return;
    }
    float dmin = depth[0], dmax = depth[0];
    for (std::size_t k = 1; k < n; ++k) { dmin = depth[k] < dmin ? depth[k] : dmin; dmax = depth[k] > dmax ? depth[k] : dmax; }
    const float span = dmax - dmin;
    if (span <= 0.0f) return;
    const float scale = 65535.0f / span;
    for (std::size_t k = 0; k < n; ++k) key[k] = static_cast<std::uint16_t>((dmax - depth[k]) * scale);
    std::uint32_t count[256];
    for (int pass = 0; pass < 2; ++pass) {
        const int shift = pass * 8;
        std::memset(count, 0, sizeof(count));
        for (std::size_t k = 0; k < n; ++k) ++count[(key[k] >> shift) & 0xFF];
        std::uint32_t sum = 0;
        for (int b = 0; b < 256; ++b) { const std::uint32_t c = count[b]; count[b] = sum; sum += c; }
        for (std::size_t k = 0; k < n; ++k) {
            const std::uint32_t at = count[(key[k] >> shift) & 0xFF]++;
            tmp[at] = order[k];
            key_tmp[at] = key[k];
        }
        std::memcpy(order, tmp, n * sizeof(std::uint32_t));
        std::memcpy(key, key_tmp, n * sizeof(std::uint16_t));
    }
}

struct Corners {
    float x[4], y[4], z[4];
};

void update_billboard(DrawList3D& out, const Axes& a) {
    const float b[6] = { a.rx, a.ry, a.rz, a.ux, a.uy, a.uz };
    bool same = !out.billboard.empty();
    for (int i = 0; i < 6 && same; ++i) if (std::fabs(b[i] - out.basis[i]) > 1e-6f) same = false;
    if (same) return;
    std::memcpy(out.basis, b, sizeof(b));
    out.billboard.clear();
    const float cx[4] = { -0.5f, 0.5f, 0.5f, -0.5f };
    const float cy[4] = { -0.5f, -0.5f, 0.5f, 0.5f };
    const float uu[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float vv[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    for (int k = 0; k < 4; ++k) {
        graphics::Vertex3D v;
        v.x = a.rx * cx[k] + a.ux * cy[k];
        v.y = a.ry * cx[k] + a.uy * cy[k];
        v.z = a.rz * cx[k] + a.uz * cy[k];
        v.nx = -a.fx; v.ny = -a.fy; v.nz = -a.fz;
        v.u = uu[k]; v.v = vv[k];
        v.rgba = 0xFFFFFFFFu;
        out.billboard.add_vertex(v);
    }
    out.billboard.add_triangle(0, 1, 2);
    out.billboard.add_triangle(0, 2, 3);
}

struct Context {
    const Emitter&         e;
    const EmitterSettings& s;
    const BakedCurves&     c;
    ParticleView           pv;
    bool                   local;
    float                  b[9];
    Float3                 epos;
    Float3                 delta;
    float                  ahead;
    bool                   d2;

    Context(const Emitter& em, const linalg::vector3d& origin, const DrawOptions& o)
        : e(em), s(em.settings()), c(em.curves()), pv(em.view()), local(em.settings().space == SimulationSpace::Local) {
        em.basis_world(b);
        epos = em.position_local();
        const linalg::vector3d eo = em.origin();
        delta = { static_cast<float>(eo.x - origin.x), static_cast<float>(eo.y - origin.y), static_cast<float>(eo.z - origin.z) };
        ahead = 0.0f;
        if (o.extrapolate) {
            ahead = em.render_lag();
            if (ahead > o.max_extrapolation) ahead = o.max_extrapolation;
            if (ahead < 0.0f) ahead = 0.0f;
        }
        d2 = em.settings().dimension == ParticleDim::D2;
    }

    void position(std::size_t i, float& x, float& y, float& z, float& vx, float& vy, float& vz) const noexcept {
        float px = pv.px[i], py = pv.py[i], pz = pv.pz[i];
        vx = pv.vx[i]; vy = pv.vy[i]; vz = pv.vz[i];
        if (local) {
            const float wx = epos.x + b[0] * px + b[3] * py + b[6] * pz;
            const float wy = epos.y + b[1] * px + b[4] * py + b[7] * pz;
            const float wz = epos.z + b[2] * px + b[5] * py + b[8] * pz;
            const float tx = b[0] * vx + b[3] * vy + b[6] * vz;
            const float ty = b[1] * vx + b[4] * vy + b[7] * vz;
            const float tz = b[2] * vx + b[5] * vy + b[8] * vz;
            px = wx; py = wy; pz = wz; vx = tx; vy = ty; vz = tz;
        }
        x = px + delta.x + vx * ahead;
        y = py + delta.y + vy * ahead;
        z = pz + delta.z + vz * ahead;
    }

    float alpha(std::size_t i, int k) const noexcept {
        const float r = pv.rnd[i];
        return (s.color.a + (s.color_alt.a - s.color.a) * r) * c.color[k].a * c.alpha[k];
    }

    void uv(std::size_t i, bool simple, float& u0, float& v0, float& u1, float& v1) const noexcept {
        const int fx = s.frames_x < 1 ? 1 : s.frames_x;
        const int fy = s.frames_y < 1 ? 1 : s.frames_y;
        const std::uint32_t f = simple ? 0u : e.particle_frame(i);
        const float w = 1.0f / static_cast<float>(fx), h = 1.0f / static_cast<float>(fy);
        const float col = static_cast<float>(f % static_cast<std::uint32_t>(fx));
        const float row = static_cast<float>(f / static_cast<std::uint32_t>(fx));
        u0 = col * w; u1 = u0 + w;
        v0 = row * h; v1 = v0 + h;
    }
};

std::size_t collect(const Context& cx, const CameraView& view, const linalg::vector3d& origin, float min_px, DrawList3D& out, const Axes& cam) {
    const std::size_t n = cx.pv.count;
    grow(out.scratch_stage, n);
    grow(out.scratch_depth, n);
    const bool persp = view.active && !view.is_2d && !view.orthographic;
    const float cpx = static_cast<float>(view.position.x - origin.x);
    const float cpy = static_cast<float>(view.position.y - origin.y);
    const float cpz = static_cast<float>(view.position.z - origin.z);
    const float pxs = static_cast<float>(view.pixel_scale);
    const float near_clip = static_cast<float>(view.near_clip);
    const bool fade = cx.s.near_fade > 0.0f && view.active && !view.is_2d;
    const float inv_fade = fade ? 1.0f / cx.s.near_fade : 0.0f;
    const bool size_cull = persp && pxs > 0.0f && min_px > 0.0f;
    const float cull_k = size_cull ? min_px / pxs : 0.0f;
    const float* __restrict age = cx.pv.age;
    const float* __restrict inv = cx.pv.inv_life;
    const float* __restrict sz = cx.pv.size;
    const float* __restrict rnd = cx.pv.rnd;
    const float* size_lut = cx.c.size;
    const bool plain = !cx.local && cx.ahead == 0.0f;
    const float dx = cx.delta.x, dy = cx.delta.y, dz = cx.delta.z;
    StagedParticle* __restrict st = out.scratch_stage.data();
    float* __restrict dep = out.scratch_depth.data();
    std::size_t m = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const float t = age[i] * inv[i];
        if (t >= 1.0f) continue;
        const int k = BakedCurves::index(t);
        std::uint32_t rgba = cx.c.tint(rnd[i])[k];
        if (rgba < 0x01000000u) continue;
        const float size = sz[i] * size_lut[k];
        if (size <= 0.0f) continue;
        float x, y, z;
        if (plain) {
            x = cx.pv.px[i] + dx; y = cx.pv.py[i] + dy; z = cx.pv.pz[i] + dz;
        } else {
            float vx, vy, vz;
            cx.position(i, x, y, z, vx, vy, vz);
        }
        const float depth = (x - cpx) * cam.fx + (y - cpy) * cam.fy + (z - cpz) * cam.fz;
        if (persp) {
            if (depth <= near_clip) continue;
            if (size < cull_k * depth) continue;
        }
        if (fade) {
            const float f = (depth - near_clip) * inv_fade;
            if (f <= 0.0f) continue;
            if (f < 1.0f) {
                const std::uint32_t a = static_cast<std::uint32_t>(static_cast<float>(rgba >> 24) * f);
                if (a == 0) continue;
                rgba = (rgba & 0x00FFFFFFu) | (a << 24);
            }
        }
        StagedParticle& o = st[m];
        o.x = x; o.y = y; o.z = z;
        o.size = size;
        o.rgba = rgba;
        o.index = static_cast<std::uint32_t>(i);
        dep[m] = depth;
        ++m;
    }
    out.culled += n - m;
    return m;
}

void append_one(const Emitter& e, const CameraView& view, DrawList3D& out, const DrawOptions& o, float min_px, const Axes& cam) {
    for (std::size_t ci = 0; ci < e.child_count(); ++ci) if (const Emitter* ch = e.child(ci)) append_one(*ch, view, out, o, min_px, cam);
    if (e.count() == 0) return;
    if (view.active && !e.visible()) return;

    const Context cx(e, out.origin, o);
    const EmitterSettings& s = cx.s;
    const std::size_t m = collect(cx, view, out.origin, min_px, out, cam);
    if (m == 0) return;

    const bool sort = wants_sort(s) && view.active && !view.is_2d && m > 1;
    const std::uint32_t* order = nullptr;
    if (sort) {
        grow(out.scratch_index_b, m);
        grow(out.scratch_order, m);
        grow(out.scratch_key, m);
        grow(out.scratch_key_b, m);
        sort_entries(m, s.sort == SortMode::Exact, out.scratch_depth.data(), out.scratch_order.data(), out.scratch_index_b.data(), out.scratch_key.data(), out.scratch_key_b.data());
        order = out.scratch_order.data();
    }

    const bool simple = e.simple_render();
    DrawBatch3D batch;
    batch.emitter = &e;
    batch.blend = s.blend;
    batch.mode = s.render_mode;
    batch.lit = s.lit;
    batch.soft = s.soft;
    batch.soft_distance = s.soft_distance;
    batch.texture_id = s.texture_id;
    batch.texture_name = &s.texture;
    batch.texture = e.texture();
    batch.particles = m;
    batch.sorted = sort;

    const bool instanced = !o.sprites && o.allow_instancing && s.render_mode == RenderMode::Billboard && (e.flags().instanced_ok || simple);
    batch.path = o.sprites ? DrawPath::Sprites : (instanced ? DrawPath::Instanced : DrawPath::Triangles);
    const StagedParticle* stage = out.scratch_stage.data();
    const ParticleView& pv = cx.pv;

    if (batch.path == DrawPath::Instanced) {
        batch.first = out.instances.size();
        out.instances.resize(batch.first + m);
        graphics::Instance3D* dst = out.instances.data() + batch.first;
        for (std::size_t k = 0; k < m; ++k) {
            const StagedParticle& sp = stage[order ? order[k] : k];
            graphics::Instance3D& in = dst[k];
            in.x = sp.x; in.y = sp.y; in.z = sp.z;
            in.scale = sp.size;
            in.rgba = sp.rgba;
            in.light = graphics::LIGHT_FULL_SKY;
        }
        batch.count = m;
        out.batches.push_back(batch);
        out.particles += m;
        return;
    }

    const bool rot = e.flags().rotation && !simple;
    const bool frames = e.flags().flipbook && !simple;
    float fu0 = 0.0f, fv0 = 0.0f, fu1 = 1.0f, fv1 = 1.0f;
    if (batch.path == DrawPath::Sprites) batch.first = out.sprites.size(), out.sprites.resize(batch.first + m);
    else batch.first = out.vertices.size(), out.vertices.resize(batch.first + m * 6);

    for (std::size_t k = 0; k < m; ++k) {
        const std::uint32_t ek = order ? order[k] : static_cast<std::uint32_t>(k);
        const StagedParticle& sp = stage[ek];
        const std::size_t i = sp.index;
        const float x = sp.x, y = sp.y, z = sp.z;
        const float h = 0.5f * sp.size;
        const std::uint32_t rgba = sp.rgba;
        float u0 = fu0, v0 = fv0, u1 = fu1, v1 = fv1;
        if (frames) cx.uv(i, false, u0, v0, u1, v1);
        const float angle = rot ? pv.rotation[i] : 0.0f;

        float ax, ay, az, bx, by, bz;
        float ha = h, hb = h;
        float sx = 0.0f, sy = 0.0f, sz = 0.0f;
        switch (s.render_mode) {
            case RenderMode::Stretched: {
                float vx = pv.vx[i], vy = pv.vy[i], vz = pv.vz[i];
                if (cx.local) {
                    const float tx = cx.b[0] * vx + cx.b[3] * vy + cx.b[6] * vz;
                    const float ty = cx.b[1] * vx + cx.b[4] * vy + cx.b[7] * vz;
                    const float tz = cx.b[2] * vx + cx.b[5] * vy + cx.b[8] * vz;
                    vx = tx; vy = ty; vz = tz;
                }
                const float vf = vx * cam.fx + vy * cam.fy + vz * cam.fz;
                float px = vx - cam.fx * vf, py = vy - cam.fy * vf, pz = vz - cam.fz * vf;
                const float l = std::sqrt(px * px + py * py + pz * pz);
                const float speed = std::sqrt(vx * vx + vy * vy + vz * vz);
                if (l > 1e-5f) { px /= l; py /= l; pz /= l; } else { px = cam.ux; py = cam.uy; pz = cam.uz; }
                bx = px; by = py; bz = pz;
                ax = cam.fy * bz - cam.fz * by; ay = cam.fz * bx - cam.fx * bz; az = cam.fx * by - cam.fy * bx;
                const float al = std::sqrt(ax * ax + ay * ay + az * az);
                if (al > 1e-6f) { ax /= al; ay /= al; az /= al; } else { ax = cam.rx; ay = cam.ry; az = cam.rz; }
                hb = h + 0.5f * s.stretch * speed;
                sx = bx * hb; sy = by * hb; sz = bz * hb;
                break;
            }
            case RenderMode::Horizontal: {
                const float ca = std::cos(angle), sa = std::sin(angle);
                ax = ca; ay = 0.0f; az = sa;
                bx = -sa; by = 0.0f; bz = ca;
                break;
            }
            case RenderMode::Vertical: {
                bx = 0.0f; by = 1.0f; bz = 0.0f;
                ax = -cam.fz; ay = 0.0f; az = cam.fx;
                const float al = std::sqrt(ax * ax + az * az);
                if (al > 1e-6f) { ax /= al; az /= al; } else { ax = cam.rx; ay = cam.ry; az = cam.rz; }
                break;
            }
            default: {
                if (angle != 0.0f) {
                    const float ca = std::cos(angle), sa = std::sin(angle);
                    ax = cam.rx * ca + cam.ux * sa; ay = cam.ry * ca + cam.uy * sa; az = cam.rz * ca + cam.uz * sa;
                    bx = cam.ux * ca - cam.rx * sa; by = cam.uy * ca - cam.ry * sa; bz = cam.uz * ca - cam.rz * sa;
                } else {
                    ax = cam.rx; ay = cam.ry; az = cam.rz;
                    bx = cam.ux; by = cam.uy; bz = cam.uz;
                }
                break;
            }
        }

        if (batch.path == DrawPath::Sprites) {
            ParticleSprite& ps = out.sprites[batch.first + k];
            ps.x = x; ps.y = y; ps.z = z;
            ps.size = sp.size;
            ps.axis_x = sx; ps.axis_y = sy; ps.axis_z = sz;
            ps.rotation = angle;
            ps.u0 = u0; ps.v0 = v0; ps.u1 = u1; ps.v1 = v1;
            ps.rgba = rgba;
            ps.depth = out.scratch_depth[ek];
            continue;
        }

        graphics::Vertex3D* v = out.vertices.data() + batch.first + k * 6;
        const float cxs[4] = { -ha, ha, ha, -ha };
        const float cys[4] = { -hb, -hb, hb, hb };
        const float us[4] = { u0, u1, u1, u0 };
        const float vs[4] = { v1, v1, v0, v0 };
        graphics::Vertex3D q[4];
        for (int c = 0; c < 4; ++c) {
            q[c].x = x + ax * cxs[c] + bx * cys[c];
            q[c].y = y + ay * cxs[c] + by * cys[c];
            q[c].z = z + az * cxs[c] + bz * cys[c];
            q[c].nx = -cam.fx; q[c].ny = -cam.fy; q[c].nz = -cam.fz;
            q[c].u = us[c]; q[c].v = vs[c];
            q[c].rgba = rgba;
        }
        v[0] = q[0]; v[1] = q[1]; v[2] = q[2];
        v[3] = q[0]; v[4] = q[2]; v[5] = q[3];
    }
    batch.count = batch.path == DrawPath::Sprites ? m : m * 6;
    out.batches.push_back(batch);
    out.particles += m;
}

void append_one_2d(const Emitter& e, const CameraView& view, DrawList2D& out, const DrawOptions& o) {
    for (std::size_t ci = 0; ci < e.child_count(); ++ci) if (const Emitter* ch = e.child(ci)) append_one_2d(*ch, view, out, o);
    if (e.count() == 0) return;
    if (view.active && !e.visible()) return;

    const Context cx(e, linalg::vector3d{ 0.0, 0.0, 0.0 }, o);
    const EmitterSettings& s = cx.s;
    const ParticleView& pv = cx.pv;
    const bool simple = e.simple_render();
    const bool rot = e.flags().rotation && !simple;
    const std::size_t n = pv.count;

    DrawBatch2D batch;
    batch.emitter = &e;
    batch.blend = s.blend;
    batch.mode = particle_blend_2d(s.blend);
    batch.texture_id = s.texture_id;
    batch.texture_name = &s.texture;
    batch.texture = e.texture();
    batch.first = out.vertices.size();
    out.vertices.resize(batch.first + n * 6);
    graphics::Vertex2D* dst = out.vertices.data() + batch.first;
    std::size_t m = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const float t = pv.age[i] * pv.inv_life[i];
        if (t >= 1.0f) continue;
        const int lut = BakedCurves::index(t);
        const std::uint32_t rgba = cx.c.tint(pv.rnd[i])[lut];
        if (rgba < 0x01000000u) continue;
        const float size = pv.size[i] * cx.c.size[lut];
        if (size <= 0.0f) continue;
        float x, y, z, vx, vy, vz;
        cx.position(i, x, y, z, vx, vy, vz);
        const float h = 0.5f * size;
        float ax = 1.0f, ay = 0.0f, bx = 0.0f, by = 1.0f, ha = h, hb = h;
        if (s.render_mode == RenderMode::Stretched) {
            const float l = std::sqrt(vx * vx + vy * vy);
            if (l > 1e-5f) { bx = vx / l; by = vy / l; ax = by; ay = -bx; }
            hb = h + 0.5f * s.stretch * l;
        } else if (rot && pv.rotation[i] != 0.0f) {
            const float ca = std::cos(pv.rotation[i]), sa = std::sin(pv.rotation[i]);
            ax = ca; ay = sa; bx = -sa; by = ca;
        }
        float u0, v0, u1, v1;
        cx.uv(i, simple, u0, v0, u1, v1);
        const graphics::Color c(static_cast<std::uint8_t>(rgba & 0xFF), static_cast<std::uint8_t>((rgba >> 8) & 0xFF), static_cast<std::uint8_t>((rgba >> 16) & 0xFF), static_cast<std::uint8_t>(rgba >> 24));
        const float cxs[4] = { -ha, ha, ha, -ha };
        const float cys[4] = { -hb, -hb, hb, hb };
        const float us[4] = { u0, u1, u1, u0 };
        const float vs[4] = { v0, v0, v1, v1 };
        graphics::Vertex2D q[4];
        for (int k = 0; k < 4; ++k) q[k] = graphics::Vertex2D(x + ax * cxs[k] + bx * cys[k], y + ay * cxs[k] + by * cys[k], us[k], vs[k], c);
        graphics::Vertex2D* v = dst + m * 6;
        v[0] = q[0]; v[1] = q[1]; v[2] = q[2];
        v[3] = q[0]; v[4] = q[2]; v[5] = q[3];
        ++m;
    }
    out.vertices.resize(batch.first + m * 6);
    if (m == 0) return;
    batch.count = m * 6;
    batch.particles = m;
    out.batches.push_back(batch);
    out.particles += m;
}

float pick_min_px(const DrawOptions& o, const ParticleSystem* sys) noexcept {
    if (o.min_pixel_size >= 0.0f) return o.min_pixel_size;
    return sys ? sys->settings().min_pixel_size : 0.0f;
}

} // namespace

void DrawList3D::reserve(std::size_t n) {
    instances.reserve(n);
    vertices.reserve(n * 6);
    scratch_stage.reserve(n);
    scratch_index_b.reserve(n);
    scratch_order.reserve(n);
    scratch_depth.reserve(n);
    scratch_key.reserve(n);
    scratch_key_b.reserve(n);
}

void build_draw_list(const ParticleSystem& system, const CameraView& view, DrawList3D& out, const DrawOptions& options) {
    out.clear();
    out.origin = system.origin();
    const Axes cam = camera_axes(view);
    const float min_px = pick_min_px(options, &system);
    for (std::size_t i = 0; i < system.root_count(); ++i) append_one(*system.root(i), view, out, options, min_px, cam);
    bool any_instanced = false;
    for (const DrawBatch3D& b : out.batches) if (b.path == DrawPath::Instanced) { any_instanced = true; break; }
    if (any_instanced) update_billboard(out, cam);
}

void append_draw_list(const Emitter& emitter, const CameraView& view, DrawList3D& out, const DrawOptions& options) {
    if (out.batches.empty()) out.origin = emitter.origin();
    const Axes cam = camera_axes(view);
    append_one(emitter, view, out, options, pick_min_px(options, nullptr), cam);
    for (const DrawBatch3D& b : out.batches) if (b.path == DrawPath::Instanced) { update_billboard(out, cam); break; }
}

void build_draw_list_2d(const ParticleSystem& system, const CameraView& view, DrawList2D& out, const DrawOptions& options) {
    out.clear();
    for (std::size_t i = 0; i < system.root_count(); ++i) append_one_2d(*system.root(i), view, out, options);
}

void append_draw_list_2d(const Emitter& emitter, const CameraView& view, DrawList2D& out, const DrawOptions& options) {
    append_one_2d(emitter, view, out, options);
}

graphics::BlendMode particle_blend_2d(ParticleBlend blend) noexcept {
    return blend == ParticleBlend::Additive ? graphics::BlendMode::Add : graphics::BlendMode::Normal;
}

const graphics::Texture& default_particle_texture() {
    return graphics::ParticleEmitter::default_texture();
}

graphics::Material3D particle_material(const DrawBatch3D& batch) {
    graphics::Material3D m;
    m.texture = batch.texture ? batch.texture : &default_particle_texture();
    m.cull = graphics::Cull3D::None;
    m.blend = batch.blend == ParticleBlend::Additive ? graphics::Blend3D::Additive : graphics::Blend3D::Alpha;
    m.depth_test = true;
    m.depth_write = false;
    m.lit = batch.lit;
    m.shadow = graphics::Shadow3D::None;
    m.fog = true;
    return m;
}

std::size_t draw(graphics::Renderer& renderer, const DrawList3D& list) {
    linalg::Matrix4d model = linalg::Matrix4d::identity();
    model.data[3] = list.origin.x;
    model.data[7] = list.origin.y;
    model.data[11] = list.origin.z;
    std::size_t drawn = 0;
    for (const DrawBatch3D& b : list.batches) {
        if (b.count == 0) continue;
        const graphics::Material3D mat = particle_material(b);
        if (b.path == DrawPath::Instanced) {
            renderer.draw_instances(list.billboard, list.origin, list.instances.data() + b.first, b.count, mat);
        } else if (b.path == DrawPath::Triangles) {
            renderer.draw_triangles_3d(list.vertices.data() + b.first, b.count, model, mat);
        } else {
            continue;
        }
        drawn += b.particles;
    }
    return drawn;
}

std::size_t draw(graphics::Renderer& renderer, const DrawList2D& list) {
    std::size_t drawn = 0;
    for (const DrawBatch2D& b : list.batches) {
        if (b.count == 0) continue;
        graphics::Renderer::BlendScope scope(renderer, b.mode);
        renderer.draw_vertices(list.vertices.data() + b.first, b.count, b.texture ? b.texture : &default_particle_texture());
        drawn += b.particles;
    }
    return drawn;
}

std::size_t draw_particles(graphics::Renderer& renderer, const ParticleSystem& system, const graphics::Camera3D& camera, DrawList3D& scratch) {
    build_draw_list(system, CameraView::from_camera(camera), scratch);
    return draw(renderer, scratch);
}

std::size_t draw_particles_2d(graphics::Renderer& renderer, const ParticleSystem& system, DrawList2D& scratch) {
    build_draw_list_2d(system, view_of(renderer), scratch);
    return draw(renderer, scratch);
}

CameraView view_of(const graphics::Renderer& renderer) noexcept {
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    renderer.visible_world_rect(x0, y0, x1, y1);
    return CameraView::from_rect(x0, y0, x1, y1);
}

} // namespace particles
} // namespace fizmo
