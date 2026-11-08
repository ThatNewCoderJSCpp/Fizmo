#ifndef FIZMO_GRAPHICS_VISIBILITY_3D_HPP
#define FIZMO_GRAPHICS_VISIBILITY_3D_HPP

#include "camera_3d.hpp"
#include "../Physics/Rigid Body/dynamic_tree.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fizmo {
namespace graphics {

struct VisBox3D {
    linalg::vector3d min{};
    linalg::vector3d max{};

    VisBox3D() = default;
    VisBox3D(const linalg::vector3d& lo, const linalg::vector3d& hi) noexcept : min(lo), max(hi) {}

    linalg::vector3d center() const noexcept { return (min + max) * 0.5; }
    linalg::vector3d extents() const noexcept { return (max - min) * 0.5; }
    double radius() const noexcept;
    bool overlaps(const VisBox3D& o) const noexcept;
    bool contains(const VisBox3D& o) const noexcept;
    VisBox3D fatten(double m) const noexcept;
    static VisBox3D merge(const VisBox3D& a, const VisBox3D& b) noexcept;
    double distance_to(const linalg::vector3d& p) const noexcept;
};

} // namespace graphics

namespace physics {

template <>
struct BoxTraits<graphics::VisBox3D> {
    using Vector = linalg::vector3d;
    static bool overlaps(const graphics::VisBox3D& a, const graphics::VisBox3D& b) noexcept { return a.overlaps(b); }
    static bool contains(const graphics::VisBox3D& outer, const graphics::VisBox3D& inner) noexcept { return outer.contains(inner); }
    static graphics::VisBox3D merge(const graphics::VisBox3D& a, const graphics::VisBox3D& b) noexcept { return graphics::VisBox3D::merge(a, b); }
    static graphics::VisBox3D fatten(const graphics::VisBox3D& a, double m) noexcept { return a.fatten(m); }
    static double cost(const graphics::VisBox3D& a) noexcept {
        const double w = std::min(a.max.x - a.min.x, 1e12), h = std::min(a.max.y - a.min.y, 1e12), d = std::min(a.max.z - a.min.z, 1e12);
        return 2.0 * (w * h + h * d + d * w);
    }
    static graphics::VisBox3D translate(const graphics::VisBox3D& a, const Vector& v) noexcept { return { a.min + v, a.max + v }; }
};

} // namespace physics

namespace graphics {

struct LodTile3D {
    int level = 0;
    std::int32_t x = 0;
    std::int32_t z = 0;
    double size = 0.0;
    linalg::vector3d center{};
    std::uint64_t key = 0;
};

struct LodQuadtreeSettings3D {
    linalg::vector3d origin{};
    double world_size = 4096.0;
    double base_distance = 64.0;
    int max_level = 8;
    double hysteresis = 0.1;
    int up_axis = 1;
    double tile_height = 0.0;
};

class LodQuadtree3D {
public:
    LodQuadtree3D() = default;
    explicit LodQuadtree3D(const LodQuadtreeSettings3D& settings) : m_settings(settings) {}

    void set_settings(const LodQuadtreeSettings3D& settings);
    const LodQuadtreeSettings3D& settings() const noexcept { return m_settings; }

    bool update(const linalg::vector3d& eye);
    void clear();

    const std::vector<LodTile3D>& tiles() const noexcept { return m_tiles; }
    const std::vector<LodTile3D>& added() const noexcept { return m_added; }
    const std::vector<std::uint64_t>& removed() const noexcept { return m_removed; }
    bool has(std::uint64_t key) const { return m_current.count(key) != 0; }
    const LodTile3D* tile_at(const linalg::vector3d& p) const;
    int level_at(const linalg::vector3d& p) const;

    static std::uint64_t make_key(int level, std::int32_t x, std::int32_t z) noexcept;

private:
    void visit(int level, std::int32_t x, std::int32_t z, const linalg::vector3d& eye);
    double tile_size(int level) const noexcept;
    linalg::vector3d tile_center(int level, std::int32_t x, std::int32_t z) const noexcept;
    double plane_distance(const linalg::vector3d& eye, int level, std::int32_t x, std::int32_t z) const noexcept;

    LodQuadtreeSettings3D m_settings;
    std::vector<LodTile3D> m_tiles;
    std::vector<LodTile3D> m_added;
    std::vector<std::uint64_t> m_removed;
    std::unordered_map<std::uint64_t, std::size_t> m_current;
    std::unordered_set<std::uint64_t> m_split;
    std::unordered_set<std::uint64_t> m_next_split;
};

struct LodLevels3D {
    static constexpr int MAX_LEVELS = 8;
    std::array<double, MAX_LEVELS> distances{};
    int count = 0;

    static LodLevels3D none() noexcept { return {}; }
    static LodLevels3D of(std::initializer_list<double> switches) noexcept;
    int pick(double distance, int previous, double hysteresis) const noexcept;
};

enum class ShadowMode3D : std::uint8_t {
    Real = 0,
    Blob,
    None
};

struct ShadowLodSettings3D {
    bool enabled = true;
    double real_distance = 48.0;
    double blob_distance = 160.0;
    double hysteresis = 0.08;
    float blob_opacity = 0.45f;
    double blob_scale = 0.9;
    bool fade_blobs = true;
};

struct BlobShadow3D {
    std::uint32_t object = 0;
    linalg::vector3d center{};
    linalg::vector3d normal{ 0.0, 1.0, 0.0 };
    double radius = 0.5;
    float opacity = 0.45f;
};

struct VisibilitySettings3D {
    bool frustum = true;
    bool distance = true;
    double max_distance = 0.0;
    double min_screen_pixels = 0.0;
    bool occlusion = true;
    unsigned int occlusion_width = 128;
    unsigned int occlusion_height = 64;
    std::size_t max_occluders = 48;
    double min_occluder_pixels = 6.0;
    bool erode_occlusion = true;
    double reuse_move = 0.0;
    double reuse_turn = 0.0;
    double lod_hysteresis = 0.1;
    double lod_bias = 1.0;
    int up_axis = 1;
    ShadowLodSettings3D shadows;
    std::function<bool(const linalg::vector3d& from, const linalg::vector3d& down, double reach, linalg::vector3d& hit, linalg::vector3d& normal)> ground;
};

struct VisibilityResult3D {
    bool visible = false;
    bool occluded = false;
    int lod = 0;
    ShadowMode3D shadow = ShadowMode3D::None;
    float screen_pixels = 0.0f;
    float distance = 0.0f;
};

struct VisibilityStats3D {
    std::size_t objects = 0;
    std::size_t visible = 0;
    std::size_t frustum_culled = 0;
    std::size_t distance_culled = 0;
    std::size_t small_culled = 0;
    std::size_t occluded = 0;
    std::size_t occluders = 0;
    std::size_t real_shadows = 0;
    std::size_t blob_shadows = 0;
    std::size_t tree_nodes_visited = 0;
    bool reused = false;
};

enum VisibilityFlags3D : std::uint32_t {
    VIS_NONE = 0,
    VIS_OCCLUDER = 1u << 0,
    VIS_CASTER = 1u << 1,
    VIS_ALWAYS = 1u << 2,
    VIS_NO_OCCLUSION = 1u << 3,
    VIS_NO_BLOB = 1u << 4,
    VIS_DEFAULT = VIS_CASTER
};

class Visibility3D {
public:
    using Handle = std::uint32_t;
    static constexpr Handle INVALID = std::numeric_limits<Handle>::max();

    Visibility3D() = default;
    explicit Visibility3D(const VisibilitySettings3D& settings) : m_settings(settings) {}

    void set_settings(const VisibilitySettings3D& settings);
    const VisibilitySettings3D& settings() const noexcept { return m_settings; }
    VisibilitySettings3D& edit_settings() noexcept { ++m_version; return m_settings; }

    Handle add(const VisBox3D& bounds, std::uint32_t flags = VIS_DEFAULT, const LodLevels3D& lods = LodLevels3D::none(), std::uint64_t user = 0);
    void update(Handle h, const VisBox3D& bounds);
    void set_flags(Handle h, std::uint32_t flags);
    void set_lods(Handle h, const LodLevels3D& lods);
    void set_occluder_box(Handle h, const VisBox3D& solid);
    void remove(Handle h);
    void clear();
    bool valid(Handle h) const noexcept;
    std::size_t size() const noexcept { return m_live; }
    std::uint64_t user(Handle h) const noexcept;
    const VisBox3D& bounds(Handle h) const noexcept;

    void cull(const Camera3D& camera);

    const VisibilityResult3D& result(Handle h) const noexcept;
    bool visible(Handle h) const noexcept { return result(h).visible; }
    int lod(Handle h) const noexcept { return result(h).lod; }
    ShadowMode3D shadow(Handle h) const noexcept { return result(h).shadow; }
    const std::vector<Handle>& visible_list() const noexcept { return m_visible; }
    const std::vector<Handle>& shadow_casters() const noexcept { return m_casters; }
    const std::vector<BlobShadow3D>& blobs() const noexcept { return m_blobs; }
    const VisibilityStats3D& stats() const noexcept { return m_stats; }

    const std::vector<float>& occlusion_depth() const noexcept { return m_depth; }
    bool point_occluded(const Camera3D& camera, const linalg::vector3d& p) const;

private:
    struct Object {
        VisBox3D bounds;
        VisBox3D solid;
        bool has_solid = false;
        std::uint32_t flags = VIS_DEFAULT;
        LodLevels3D lods;
        std::uint64_t user = 0;
        std::int32_t proxy = -1;
        bool live = false;
    };

    bool can_reuse(const Camera3D& camera) const;
    std::size_t build_occlusion(const Camera3D& camera, const std::vector<Handle>& candidates);
    void raster_box(const linalg::Matrix4d& vp, const VisBox3D& box);
    void raster_triangle(const double* a, const double* b, const double* c);
    bool box_occluded(const linalg::Matrix4d& vp, const VisBox3D& box) const;
    double screen_pixels(const Camera3D& camera, const VisBox3D& box, double distance) const;
    void place_blob(Handle h, const Object& o, double distance);

    VisibilitySettings3D m_settings;
    std::vector<Object> m_objects;
    std::vector<VisibilityResult3D> m_results;
    std::vector<Handle> m_free;
    std::size_t m_live = 0;
    physics::DynamicTree<VisBox3D, Handle> m_tree;
    std::vector<Handle> m_visible;
    std::vector<Handle> m_casters;
    std::vector<BlobShadow3D> m_blobs;
    std::vector<float> m_depth;
    std::vector<float> m_scratch;
    VisibilityStats3D m_stats;
    std::uint64_t m_version = 1;
    std::uint64_t m_culled_version = 0;
    linalg::vector3d m_last_eye{};
    linalg::vector3d m_last_forward{};
    bool m_has_last = false;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_GRAPHICS_VISIBILITY_3D_HPP
