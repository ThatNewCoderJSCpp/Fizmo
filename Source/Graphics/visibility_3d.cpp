#include "fizmo_library.hpp"
#include "visibility_3d.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace graphics {

namespace {

using V3 = linalg::vector3d;

double axis_of(const V3& v, int a) noexcept { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }

void set_axis(V3& v, int a, double value) noexcept {
    if (a == 0) v.x = value;
    else if (a == 1) v.y = value;
    else v.z = value;
}

void plane_axes(int up, int& a, int& b) noexcept {
    if (up == 0) {
        a = 1;
        b = 2;
    } else if (up == 2) {
        a = 0;
        b = 1;
    } else {
        a = 0;
        b = 2;
    }
}

double length3(const V3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

}

double VisBox3D::radius() const noexcept { return length3(extents()); }

bool VisBox3D::overlaps(const VisBox3D& o) const noexcept {
    return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y && min.z <= o.max.z && max.z >= o.min.z;
}

bool VisBox3D::contains(const VisBox3D& o) const noexcept {
    return min.x <= o.min.x && min.y <= o.min.y && min.z <= o.min.z && max.x >= o.max.x && max.y >= o.max.y && max.z >= o.max.z;
}

VisBox3D VisBox3D::fatten(double m) const noexcept {
    return { V3{ min.x - m, min.y - m, min.z - m }, V3{ max.x + m, max.y + m, max.z + m } };
}

VisBox3D VisBox3D::merge(const VisBox3D& a, const VisBox3D& b) noexcept {
    return { V3{ std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z) },
             V3{ std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z) } };
}

double VisBox3D::distance_to(const V3& p) const noexcept {
    const double dx = std::max({ min.x - p.x, 0.0, p.x - max.x });
    const double dy = std::max({ min.y - p.y, 0.0, p.y - max.y });
    const double dz = std::max({ min.z - p.z, 0.0, p.z - max.z });
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::uint64_t LodQuadtree3D::make_key(int level, std::int32_t x, std::int32_t z) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint8_t>(level)) << 56) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x) & 0x0fffffffu) << 28) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z) & 0x0fffffffu));
}

void LodQuadtree3D::set_settings(const LodQuadtreeSettings3D& settings) {
    m_settings = settings;
    m_settings.max_level = std::clamp(m_settings.max_level, 0, 24);
    m_settings.world_size = std::max(m_settings.world_size, 1e-6);
    m_settings.base_distance = std::max(m_settings.base_distance, 0.0);
    clear();
}

void LodQuadtree3D::clear() {
    m_removed.clear();
    for (const auto& kv : m_current) m_removed.push_back(kv.first);
    m_tiles.clear();
    m_added.clear();
    m_current.clear();
    m_split.clear();
}

double LodQuadtree3D::tile_size(int level) const noexcept {
    return m_settings.world_size / static_cast<double>(1u << static_cast<unsigned>(m_settings.max_level - level));
}

V3 LodQuadtree3D::tile_center(int level, std::int32_t x, std::int32_t z) const noexcept {
    int a = 0, b = 2;
    plane_axes(m_settings.up_axis, a, b);
    const double size = tile_size(level);
    V3 c = m_settings.origin;
    set_axis(c, a, axis_of(m_settings.origin, a) - m_settings.world_size * 0.5 + (x + 0.5) * size);
    set_axis(c, b, axis_of(m_settings.origin, b) - m_settings.world_size * 0.5 + (z + 0.5) * size);
    return c;
}

double LodQuadtree3D::plane_distance(const V3& eye, int level, std::int32_t x, std::int32_t z) const noexcept {
    int a = 0, b = 2;
    plane_axes(m_settings.up_axis, a, b);
    const double half = tile_size(level) * 0.5;
    const V3 c = tile_center(level, x, z);
    const double da = std::max(std::abs(axis_of(eye, a) - axis_of(c, a)) - half, 0.0);
    const double db = std::max(std::abs(axis_of(eye, b) - axis_of(c, b)) - half, 0.0);
    const double du = std::max(std::abs(axis_of(eye, m_settings.up_axis) - axis_of(m_settings.origin, m_settings.up_axis)) - m_settings.tile_height * 0.5, 0.0);
    return std::sqrt(da * da + db * db + du * du);
}

void LodQuadtree3D::visit(int level, std::int32_t x, std::int32_t z, const V3& eye) {
    const std::uint64_t key = make_key(level, x, z);
    bool split = false;
    if (level > 0) {
        const double reach = m_settings.base_distance * std::ldexp(1.0, level - 1);
        const double factor = m_split.count(key) ? 1.0 + m_settings.hysteresis : 1.0 - m_settings.hysteresis;
        split = plane_distance(eye, level, x, z) < reach * factor;
    }
    if (split) {
        m_next_split.insert(key);
        for (int k = 0; k < 4; ++k) visit(level - 1, x * 2 + (k & 1), z * 2 + (k >> 1), eye);
        return;
    }
    LodTile3D t;
    t.level = level;
    t.x = x;
    t.z = z;
    t.size = tile_size(level);
    t.center = tile_center(level, x, z);
    t.key = key;
    m_tiles.push_back(t);
}

bool LodQuadtree3D::update(const V3& eye) {
    std::unordered_map<std::uint64_t, std::size_t> previous;
    previous.swap(m_current);
    m_tiles.clear();
    m_added.clear();
    m_removed.clear();
    m_next_split.clear();
    visit(m_settings.max_level, 0, 0, eye);
    m_split.swap(m_next_split);
    for (std::size_t i = 0; i < m_tiles.size(); ++i) {
        m_current.emplace(m_tiles[i].key, i);
        if (!previous.count(m_tiles[i].key)) m_added.push_back(m_tiles[i]);
    }
    for (const auto& kv : previous) {
        if (!m_current.count(kv.first)) m_removed.push_back(kv.first);
    }
    std::sort(m_removed.begin(), m_removed.end());
    return !m_added.empty() || !m_removed.empty();
}

const LodTile3D* LodQuadtree3D::tile_at(const V3& p) const {
    int a = 0, b = 2;
    plane_axes(m_settings.up_axis, a, b);
    for (const LodTile3D& t : m_tiles) {
        const double half = t.size * 0.5;
        if (std::abs(axis_of(p, a) - axis_of(t.center, a)) <= half && std::abs(axis_of(p, b) - axis_of(t.center, b)) <= half) return &t;
    }
    return nullptr;
}

int LodQuadtree3D::level_at(const V3& p) const {
    const LodTile3D* t = tile_at(p);
    return t ? t->level : m_settings.max_level;
}

LodLevels3D LodLevels3D::of(std::initializer_list<double> switches) noexcept {
    LodLevels3D l;
    for (double d : switches) {
        if (l.count >= MAX_LEVELS) break;
        l.distances[static_cast<std::size_t>(l.count++)] = d;
    }
    std::sort(l.distances.begin(), l.distances.begin() + l.count);
    return l;
}

int LodLevels3D::pick(double distance, int previous, double hysteresis) const noexcept {
    int level = 0;
    while (level < count && distance >= distances[static_cast<std::size_t>(level)]) ++level;
    if (previous < 0 || previous > count || previous == level) return level;
    if (level > previous) {
        const double edge = distances[static_cast<std::size_t>(previous)];
        if (distance < edge * (1.0 + hysteresis)) return previous;
    } else {
        const double edge = distances[static_cast<std::size_t>(level)];
        if (distance > edge * (1.0 - hysteresis)) return previous;
    }
    return level;
}

void Visibility3D::set_settings(const VisibilitySettings3D& settings) {
    m_settings = settings;
    ++m_version;
}

Visibility3D::Handle Visibility3D::add(const VisBox3D& bounds, std::uint32_t flags, const LodLevels3D& lods, std::uint64_t user) {
    Handle h;
    if (!m_free.empty()) {
        h = m_free.back();
        m_free.pop_back();
    } else {
        h = static_cast<Handle>(m_objects.size());
        m_objects.emplace_back();
        m_results.emplace_back();
    }
    Object& o = m_objects[h];
    o = Object{};
    o.bounds = bounds;
    o.flags = flags;
    o.lods = lods;
    o.user = user;
    o.live = true;
    o.proxy = m_tree.insert(bounds.fatten(0.05), h);
    m_results[h] = VisibilityResult3D{};
    m_results[h].lod = -1;
    ++m_live;
    ++m_version;
    return h;
}

bool Visibility3D::valid(Handle h) const noexcept {
    return h < m_objects.size() && m_objects[h].live;
}

void Visibility3D::update(Handle h, const VisBox3D& bounds) {
    if (!valid(h)) return;
    Object& o = m_objects[h];
    if (o.bounds.min.x == bounds.min.x && o.bounds.min.y == bounds.min.y && o.bounds.min.z == bounds.min.z &&
        o.bounds.max.x == bounds.max.x && o.bounds.max.y == bounds.max.y && o.bounds.max.z == bounds.max.z) return;
    if (o.has_solid) {
        const V3 d = bounds.center() - o.bounds.center();
        o.solid = { o.solid.min + d, o.solid.max + d };
    }
    o.bounds = bounds;
    m_tree.move(o.proxy, bounds, 0.05, bounds);
    ++m_version;
}

void Visibility3D::set_flags(Handle h, std::uint32_t flags) {
    if (!valid(h) || m_objects[h].flags == flags) return;
    m_objects[h].flags = flags;
    ++m_version;
}

void Visibility3D::set_lods(Handle h, const LodLevels3D& lods) {
    if (!valid(h)) return;
    m_objects[h].lods = lods;
    ++m_version;
}

void Visibility3D::set_occluder_box(Handle h, const VisBox3D& solid) {
    if (!valid(h)) return;
    m_objects[h].solid = solid;
    m_objects[h].has_solid = true;
    m_objects[h].flags |= VIS_OCCLUDER;
    ++m_version;
}

void Visibility3D::remove(Handle h) {
    if (!valid(h)) return;
    m_tree.remove(m_objects[h].proxy);
    m_objects[h].live = false;
    m_objects[h].proxy = -1;
    m_results[h] = VisibilityResult3D{};
    m_free.push_back(h);
    --m_live;
    ++m_version;
}

void Visibility3D::clear() {
    m_objects.clear();
    m_results.clear();
    m_free.clear();
    m_tree.clear();
    m_visible.clear();
    m_casters.clear();
    m_blobs.clear();
    m_live = 0;
    ++m_version;
}

std::uint64_t Visibility3D::user(Handle h) const noexcept {
    return valid(h) ? m_objects[h].user : 0;
}

const VisBox3D& Visibility3D::bounds(Handle h) const noexcept {
    static const VisBox3D none{};
    return valid(h) ? m_objects[h].bounds : none;
}

const VisibilityResult3D& Visibility3D::result(Handle h) const noexcept {
    static const VisibilityResult3D none{};
    return h < m_results.size() ? m_results[h] : none;
}

bool Visibility3D::can_reuse(const Camera3D& camera) const {
    if (!m_has_last || m_culled_version != m_version) return false;
    if (m_settings.reuse_move <= 0.0 && m_settings.reuse_turn <= 0.0) return false;
    const V3 d = camera.position() - m_last_eye;
    if (length3(d) > m_settings.reuse_move) return false;
    const V3 f = camera.forward();
    const double c = std::clamp(f.x * m_last_forward.x + f.y * m_last_forward.y + f.z * m_last_forward.z, -1.0, 1.0);
    return std::acos(c) <= m_settings.reuse_turn;
}

double Visibility3D::screen_pixels(const Camera3D& camera, const VisBox3D& box, double distance) const {
    const double h = static_cast<double>(std::max(1u, camera.viewport_height()));
    const double r = box.radius();
    if (camera.projection_type() == Projection3D::Orthographic) return 2.0 * r / std::max(camera.ortho_height(), 1e-9) * h;
    if (distance <= r) return h * 4.0;
    const double t = std::tan(camera.fov_y() * 3.14159265358979323846 / 360.0);
    return 2.0 * r / (std::max(distance, 1e-9) * 2.0 * std::max(t, 1e-9)) * h;
}

void Visibility3D::raster_triangle(const double* a, const double* b, const double* c) {
    const int w = static_cast<int>(m_settings.occlusion_width), h = static_cast<int>(m_settings.occlusion_height);
    const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (std::abs(area) < 1e-12) return;
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min({ a[0], b[0], c[0] }))));
    const int x1 = std::min(w - 1, static_cast<int>(std::ceil(std::max({ a[0], b[0], c[0] }))));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min({ a[1], b[1], c[1] }))));
    const int y1 = std::min(h - 1, static_cast<int>(std::ceil(std::max({ a[1], b[1], c[1] }))));
    if (x0 > x1 || y0 > y1) return;
    const double inv_area = 1.0 / area;
    const double ia = 1.0 / a[2], ib = 1.0 / b[2], ic = 1.0 / c[2];
    for (int y = y0; y <= y1; ++y) {
        const double py = y + 0.5;
        for (int x = x0; x <= x1; ++x) {
            const double px = x + 0.5;
            const double w0 = ((b[0] - px) * (c[1] - py) - (b[1] - py) * (c[0] - px)) * inv_area;
            const double w1 = ((c[0] - px) * (a[1] - py) - (c[1] - py) * (a[0] - px)) * inv_area;
            const double w2 = 1.0 - w0 - w1;
            if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0) continue;
            const double inv = w0 * ia + w1 * ib + w2 * ic;
            if (inv <= 0.0) continue;
            const float depth = static_cast<float>(1.0 / inv);
            float& dst = m_depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
            if (depth < dst) dst = depth;
        }
    }
}

void Visibility3D::raster_box(const linalg::Matrix4d& vp, const VisBox3D& box) {
    double s[8][3];
    const double W = static_cast<double>(m_settings.occlusion_width), H = static_cast<double>(m_settings.occlusion_height);
    for (int i = 0; i < 8; ++i) {
        const V3 p{ (i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z };
        const linalg::vector4d clip = vp * linalg::vector4d{ p.x, p.y, p.z, 1.0 };
        if (clip.w <= 1e-4) return;
        s[i][0] = (clip.x / clip.w * 0.5 + 0.5) * W;
        s[i][1] = (0.5 - clip.y / clip.w * 0.5) * H;
        s[i][2] = clip.w;
    }
    static const int faces[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
    for (const auto& f : faces) {
        raster_triangle(s[f[0]], s[f[1]], s[f[2]]);
        raster_triangle(s[f[0]], s[f[2]], s[f[3]]);
    }
}

bool Visibility3D::box_occluded(const linalg::Matrix4d& vp, const VisBox3D& box) const {
    const double W = static_cast<double>(m_settings.occlusion_width), H = static_cast<double>(m_settings.occlusion_height);
    double lo_x = 1e30, lo_y = 1e30, hi_x = -1e30, hi_y = -1e30, near_w = 1e30;
    for (int i = 0; i < 8; ++i) {
        const V3 p{ (i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z };
        const linalg::vector4d clip = vp * linalg::vector4d{ p.x, p.y, p.z, 1.0 };
        if (clip.w <= 1e-4) return false;
        const double sx = (clip.x / clip.w * 0.5 + 0.5) * W;
        const double sy = (0.5 - clip.y / clip.w * 0.5) * H;
        lo_x = std::min(lo_x, sx);
        hi_x = std::max(hi_x, sx);
        lo_y = std::min(lo_y, sy);
        hi_y = std::max(hi_y, sy);
        near_w = std::min(near_w, clip.w);
    }
    const int x0 = std::max(0, static_cast<int>(std::floor(lo_x)));
    const int x1 = std::min(static_cast<int>(m_settings.occlusion_width) - 1, static_cast<int>(std::ceil(hi_x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(lo_y)));
    const int y1 = std::min(static_cast<int>(m_settings.occlusion_height) - 1, static_cast<int>(std::ceil(hi_y)));
    if (x0 > x1 || y0 > y1) return false;
    const float limit = static_cast<float>(near_w);
    for (int y = y0; y <= y1; ++y) {
        const float* row = m_depth.data() + static_cast<std::size_t>(y) * m_settings.occlusion_width;
        for (int x = x0; x <= x1; ++x) {
            if (row[x] >= limit) return false;
        }
    }
    return true;
}

std::size_t Visibility3D::build_occlusion(const Camera3D& camera, const std::vector<Handle>& candidates) {
    const std::size_t w = std::max(8u, m_settings.occlusion_width), h = std::max(8u, m_settings.occlusion_height);
    m_settings.occlusion_width = static_cast<unsigned int>(w);
    m_settings.occlusion_height = static_cast<unsigned int>(h);
    m_depth.assign(w * h, std::numeric_limits<float>::infinity());
    std::vector<std::pair<double, Handle>> occluders;
    const V3 eye = camera.position();
    for (Handle c : candidates) {
        const Object& o = m_objects[c];
        if (!(o.flags & VIS_OCCLUDER)) continue;
        const VisBox3D& solid = o.has_solid ? o.solid : o.bounds;
        const double dist = solid.distance_to(eye);
        if (dist <= 0.0) continue;
        if (screen_pixels(camera, solid, dist) * static_cast<double>(h) / std::max(1.0, static_cast<double>(camera.viewport_height())) < m_settings.min_occluder_pixels) continue;
        occluders.emplace_back(dist, c);
    }
    std::sort(occluders.begin(), occluders.end());
    if (occluders.size() > m_settings.max_occluders) occluders.resize(m_settings.max_occluders);
    const linalg::Matrix4d& vp = camera.view_projection_matrix();
    for (const auto& oc : occluders) {
        const Object& o = m_objects[oc.second];
        raster_box(vp, o.has_solid ? o.solid : o.bounds);
    }
    if (m_settings.erode_occlusion && !occluders.empty()) {
        m_scratch = m_depth;
        for (std::size_t y = 0; y < h; ++y) {
            for (std::size_t x = 0; x < w; ++x) {
                float far = m_scratch[y * w + x];
                for (int dy = -1; dy <= 1; ++dy) {
                    const std::size_t yy = static_cast<std::size_t>(std::clamp(static_cast<long>(y) + dy, 0L, static_cast<long>(h) - 1));
                    for (int dx = -1; dx <= 1; ++dx) {
                        const std::size_t xx = static_cast<std::size_t>(std::clamp(static_cast<long>(x) + dx, 0L, static_cast<long>(w) - 1));
                        far = std::max(far, m_scratch[yy * w + xx]);
                    }
                }
                m_depth[y * w + x] = far;
            }
        }
    }
    return occluders.size();
}

bool Visibility3D::point_occluded(const Camera3D& camera, const V3& p) const {
    if (m_depth.empty()) return false;
    const linalg::vector4d clip = camera.view_projection_matrix() * linalg::vector4d{ p.x, p.y, p.z, 1.0 };
    if (clip.w <= 1e-4) return false;
    const int x = static_cast<int>((clip.x / clip.w * 0.5 + 0.5) * m_settings.occlusion_width);
    const int y = static_cast<int>((0.5 - clip.y / clip.w * 0.5) * m_settings.occlusion_height);
    if (x < 0 || y < 0 || x >= static_cast<int>(m_settings.occlusion_width) || y >= static_cast<int>(m_settings.occlusion_height)) return false;
    return m_depth[static_cast<std::size_t>(y) * m_settings.occlusion_width + static_cast<std::size_t>(x)] < static_cast<float>(clip.w);
}

void Visibility3D::place_blob(Handle h, const Object& o, double distance) {
    const ShadowLodSettings3D& s = m_settings.shadows;
    const int up = std::clamp(m_settings.up_axis, 0, 2);
    int a = 0, b = 2;
    plane_axes(up, a, b);
    const V3 c = o.bounds.center();
    const double ea = axis_of(o.bounds.extents(), a), eb = axis_of(o.bounds.extents(), b);
    BlobShadow3D blob;
    blob.object = h;
    blob.radius = std::max(1e-3, std::sqrt(ea * ea + eb * eb) * s.blob_scale);
    V3 bottom = c;
    set_axis(bottom, up, axis_of(o.bounds.min, up));
    V3 normal{};
    set_axis(normal, up, 1.0);
    blob.center = bottom;
    blob.normal = normal;
    double height = 0.0;
    if (m_settings.ground) {
        V3 down{};
        set_axis(down, up, -1.0);
        V3 hit, n;
        if (m_settings.ground(bottom, down, std::max(4.0 * blob.radius, 8.0), hit, n)) {
            height = length3(bottom - hit);
            blob.center = hit;
            blob.normal = n;
        }
    }
    float opacity = s.blob_opacity / static_cast<float>(1.0 + height / std::max(blob.radius, 1e-3));
    if (s.fade_blobs && s.blob_distance > s.real_distance) {
        const double t = std::clamp((distance - s.real_distance) / (s.blob_distance - s.real_distance), 0.0, 1.0);
        opacity *= static_cast<float>(1.0 - t * t);
    }
    blob.opacity = opacity;
    if (opacity > 0.004f) m_blobs.push_back(blob);
}

void Visibility3D::cull(const Camera3D& camera) {
    if (can_reuse(camera)) {
        m_stats.reused = true;
        return;
    }
    VisibilityStats3D stats;
    stats.objects = m_live;
    m_visible.clear();
    m_casters.clear();
    m_blobs.clear();
    std::vector<VisibilityResult3D> previous = m_results;
    for (VisibilityResult3D& r : m_results) {
        const int lod = r.lod;
        r = VisibilityResult3D{};
        r.lod = lod;
    }
    const V3 eye = camera.position();
    const double far = m_settings.max_distance > 0.0 ? m_settings.max_distance : camera.far_plane();
    const Frustum3D& fr = camera.frustum();
    std::vector<Handle> candidates;
    candidates.reserve(m_live);
    std::vector<char> seen(m_objects.size(), 0);
    std::size_t visited = 0;
    if (m_settings.frustum) {
        m_tree.query_shape([&](const VisBox3D& box) {
            ++visited;
            return fr.intersects_aabb(box.min, box.max);
        }, [&](std::int32_t, Handle h) {
            if (fr.intersects_aabb(m_objects[h].bounds.min, m_objects[h].bounds.max)) {
                candidates.push_back(h);
                seen[h] = 1;
            }
            return true;
        });
        for (Handle h = 0; h < m_objects.size(); ++h) {
            if (m_objects[h].live && !seen[h] && (m_objects[h].flags & VIS_ALWAYS)) {
                candidates.push_back(h);
                seen[h] = 1;
            }
        }
    } else {
        for (Handle h = 0; h < m_objects.size(); ++h) {
            if (m_objects[h].live) candidates.push_back(h);
        }
    }
    stats.tree_nodes_visited = visited;
    stats.frustum_culled = m_live - std::min(m_live, candidates.size());

    std::vector<Handle> kept;
    kept.reserve(candidates.size());
    std::vector<double> distance(m_objects.size(), 0.0);
    for (Handle h : candidates) {
        const Object& o = m_objects[h];
        const double d = o.bounds.distance_to(eye);
        distance[h] = d;
        const bool always = (o.flags & VIS_ALWAYS) != 0;
        if (!always && m_settings.distance && d > far) {
            ++stats.distance_culled;
            continue;
        }
        const double px = screen_pixels(camera, o.bounds, d);
        m_results[h].screen_pixels = static_cast<float>(px);
        m_results[h].distance = static_cast<float>(d);
        if (!always && m_settings.min_screen_pixels > 0.0 && px < m_settings.min_screen_pixels) {
            ++stats.small_culled;
            continue;
        }
        kept.push_back(h);
    }

    const bool occlusion = m_settings.occlusion && camera.projection_type() == Projection3D::Perspective;
    if (occlusion) stats.occluders = build_occlusion(camera, kept);
    else m_depth.clear();
    const linalg::Matrix4d& vp = camera.view_projection_matrix();
    for (Handle h : kept) {
        const Object& o = m_objects[h];
        VisibilityResult3D& r = m_results[h];
        if (occlusion && stats.occluders > 0 && !(o.flags & (VIS_NO_OCCLUSION | VIS_ALWAYS)) && box_occluded(vp, o.bounds)) {
            r.occluded = true;
            ++stats.occluded;
            continue;
        }
        r.visible = true;
        const int prev = previous[h].visible ? previous[h].lod : -1;
        r.lod = o.lods.count > 0 ? o.lods.pick(distance[h] * std::max(m_settings.lod_bias, 1e-6), prev, m_settings.lod_hysteresis) : 0;
        m_visible.push_back(h);
    }
    stats.visible = m_visible.size();

    const ShadowLodSettings3D& s = m_settings.shadows;
    if (s.enabled) {
        const double reach = std::max(s.real_distance, s.blob_distance) * (1.0 + s.hysteresis);
        const VisBox3D region(V3{ eye.x - reach, eye.y - reach, eye.z - reach }, V3{ eye.x + reach, eye.y + reach, eye.z + reach });
        m_tree.query(region, [&](std::int32_t, Handle h) {
            const Object& o = m_objects[h];
            if (!(o.flags & VIS_CASTER)) return true;
            VisibilityResult3D& r = m_results[h];
            const double d = o.bounds.distance_to(eye);
            const ShadowMode3D was = previous[h].shadow;
            const double real_edge = s.real_distance * (was == ShadowMode3D::Real ? 1.0 + s.hysteresis : 1.0);
            const double blob_edge = s.blob_distance * (was == ShadowMode3D::None ? 1.0 : 1.0 + s.hysteresis);
            if (d < real_edge) {
                r.shadow = ShadowMode3D::Real;
                m_casters.push_back(h);
                ++stats.real_shadows;
            } else if (d < blob_edge && !(o.flags & VIS_NO_BLOB) && (r.visible || fr.intersects_sphere(o.bounds.center(), o.bounds.radius() * 2.0))) {
                r.shadow = ShadowMode3D::Blob;
                place_blob(h, o, d);
                ++stats.blob_shadows;
            } else {
                r.shadow = ShadowMode3D::None;
            }
            return true;
        });
    } else {
        for (Handle h = 0; h < m_objects.size(); ++h) {
            if (!m_objects[h].live || !(m_objects[h].flags & VIS_CASTER)) continue;
            m_results[h].shadow = ShadowMode3D::Real;
            m_casters.push_back(h);
        }
        stats.real_shadows = m_casters.size();
    }

    m_stats = stats;
    m_last_eye = eye;
    m_last_forward = camera.forward();
    m_has_last = true;
    m_culled_version = m_version;
}

} // namespace graphics
} // namespace fizmo
