#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include "../Source/Graphics/visibility_3d.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace fizmo;
using namespace fizmo::graphics;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

VisBox3D cube(double x, double y, double z, double h) {
    return { linalg::vector3d{ x - h, y - h, z - h }, linalg::vector3d{ x + h, y + h, z + h } };
}

Camera3D looking_down_minus_z() {
    Camera3D cam(linalg::vector3d{ 0.0, 0.0, 0.0 }, 320, 200, 60.0, 0.1, 500.0);
    cam.look_at(linalg::vector3d{ 0.0, 0.0, -1.0 });
    return cam;
}

void test_frustum_and_distance() {
    Visibility3D vis;
    VisibilitySettings3D s;
    s.occlusion = false;
    s.max_distance = 100.0;
    vis.set_settings(s);
    const auto ahead = vis.add(cube(0, 0, -10, 1));
    const auto behind = vis.add(cube(0, 0, 10, 1));
    const auto far = vis.add(cube(0, 0, -200, 1));
    const auto always = vis.add(cube(0, 0, 10, 1), VIS_DEFAULT | VIS_ALWAYS);
    vis.cull(looking_down_minus_z());
    check(vis.visible(ahead), "frustum: box ahead is visible");
    check(!vis.visible(behind), "frustum: box behind is culled");
    check(!vis.visible(far), "distance: box past max_distance is culled");
    check(vis.visible(always), "flags: VIS_ALWAYS ignores the frustum");
    check(vis.stats().frustum_culled == 1 && vis.stats().distance_culled == 1, "stats count frustum and distance culls");
}

void test_occlusion() {
    Visibility3D vis;
    const auto wall = vis.add({ linalg::vector3d{ -20, -20, -11 }, linalg::vector3d{ 20, 20, -10 } }, VIS_DEFAULT | VIS_OCCLUDER);
    const auto hidden = vis.add(cube(0, 0, -30, 1));
    const auto beside = vis.add(cube(60, 0, -30, 1));
    const auto front = vis.add(cube(0, 0, -5, 0.5));
    vis.cull(looking_down_minus_z());
    check(vis.visible(wall), "occlusion: the wall itself is visible");
    check(!vis.visible(hidden) && vis.result(hidden).occluded, "occlusion: a box behind the wall is hidden");
    check(vis.visible(front), "occlusion: a box in front of the wall stays visible");
    check(!vis.result(beside).occluded, "occlusion: a box off to the side is not occluded");
    Visibility3D narrow;
    narrow.add({ linalg::vector3d{ -5, -5, -11 }, linalg::vector3d{ 5, 5, -10 } }, VIS_DEFAULT | VIS_OCCLUDER);
    const auto inside = narrow.add(cube(0, 0, -30, 2));
    const auto peek = narrow.add(cube(15.5, 0, -30, 2));
    narrow.cull(looking_down_minus_z());
    check(!narrow.visible(inside), "occlusion: narrow wall hides a box straight behind it");
    check(narrow.visible(peek), "occlusion: a box poking out past the wall edge stays visible");
}

void test_lod() {
    LodLevels3D l = LodLevels3D::of({ 10.0, 30.0, 80.0 });
    check(l.pick(5.0, -1, 0.1) == 0 && l.pick(20.0, -1, 0.1) == 1 && l.pick(50.0, -1, 0.1) == 2 && l.pick(100.0, -1, 0.1) == 3, "lod: picks by distance");
    check(l.pick(10.5, 0, 0.1) == 0, "lod: hysteresis keeps the finer level just past the edge");
    check(l.pick(11.5, 0, 0.1) == 1, "lod: switches once well past the edge");
    check(l.pick(9.5, 1, 0.1) == 1, "lod: hysteresis keeps the coarser level just inside the edge");
    Visibility3D vis;
    VisibilitySettings3D s;
    s.occlusion = false;
    vis.set_settings(s);
    const auto obj = vis.add(cube(0, 0, -50, 1), VIS_DEFAULT, l);
    vis.cull(looking_down_minus_z());
    check(vis.lod(obj) == 2, "lod: visibility picks the object's level");
}

void test_shadow_lod() {
    Visibility3D vis;
    VisibilitySettings3D s;
    s.occlusion = false;
    s.shadows.real_distance = 20.0;
    s.shadows.blob_distance = 60.0;
    vis.set_settings(s);
    const auto near = vis.add(cube(0, 0, -10, 1));
    const auto mid = vis.add(cube(0, 0, -40, 1));
    const auto distant = vis.add(cube(0, 0, -120, 1));
    const auto offscreen = vis.add(cube(0, 0, 15, 1));
    vis.cull(looking_down_minus_z());
    check(vis.shadow(near) == ShadowMode3D::Real, "shadow lod: near caster gets a real shadow");
    check(vis.shadow(mid) == ShadowMode3D::Blob, "shadow lod: mid-range caster gets a blob");
    check(vis.shadow(distant) == ShadowMode3D::None, "shadow lod: far caster gets nothing");
    check(vis.shadow(offscreen) == ShadowMode3D::Real, "shadow lod: off-screen near caster still casts into view");
    check(vis.blobs().size() == 1 && std::abs(vis.blobs()[0].center.y + 1.0) < 1e-9, "shadow lod: blob sits under the object");
}

void test_reuse() {
    Visibility3D vis;
    VisibilitySettings3D s;
    s.reuse_move = 0.01;
    s.reuse_turn = 0.001;
    vis.set_settings(s);
    vis.add(cube(0, 0, -10, 1));
    vis.cull(looking_down_minus_z());
    vis.cull(looking_down_minus_z());
    check(vis.stats().reused, "reuse: a still camera reuses last frame");
    vis.add(cube(0, 0, -20, 1));
    vis.cull(looking_down_minus_z());
    check(!vis.stats().reused, "reuse: a scene change forces a new cull");
}

void test_quadtree() {
    LodQuadtreeSettings3D s;
    s.world_size = 1024.0;
    s.base_distance = 32.0;
    s.max_level = 5;
    LodQuadtree3D q(s);
    check(q.update(linalg::vector3d{ 0.0, 0.0, 0.0 }), "quadtree: first update adds tiles");
    check(q.level_at(linalg::vector3d{ 0.0, 0.0, 0.0 }) == 0, "quadtree: finest tiles under the eye");
    check(q.level_at(linalg::vector3d{ 500.0, 0.0, 500.0 }) > 1, "quadtree: coarse tiles far away");
    double area = 0.0;
    for (const auto& t : q.tiles()) area += t.size * t.size;
    check(std::abs(area - 1024.0 * 1024.0) < 1e-6, "quadtree: tiles cover the world exactly once");
    check(!q.update(linalg::vector3d{ 0.5, 0.0, 0.5 }), "quadtree: a tiny move changes nothing");
    check(q.update(linalg::vector3d{ 300.0, 0.0, 0.0 }) && !q.added().empty() && !q.removed().empty(), "quadtree: a big move reports added and removed tiles");
}

} // namespace

int main() {
    test_frustum_and_distance();
    test_occlusion();
    test_lod();
    test_shadow_lod();
    test_reuse();
    test_quadtree();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
