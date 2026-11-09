#include "scene_internal.hpp"

#include <algorithm>
#include <numeric>

namespace fizmo {
namespace lighting {
namespace detail {

namespace {

struct Box {
    Vec3f lo{ kInfinity, kInfinity, kInfinity };
    Vec3f hi{ -kInfinity, -kInfinity, -kInfinity };

    void grow(const Vec3f& p) noexcept {
        lo = vmin(lo, p);
        hi = vmax(hi, p);
    }

    void grow(const Box& b) noexcept {
        lo = vmin(lo, b.lo);
        hi = vmax(hi, b.hi);
    }

    float area() const noexcept {
        const Vec3f e = hi - lo;
        if (e.x < 0.0f) return 0.0f;
        return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
    }
};

struct Item {
    Box box;
    Vec3f centroid;
};

constexpr int kBins = 16;
constexpr std::uint32_t kLeafSize = 4;

struct Builder {
    std::vector<Item>& items;
    std::vector<std::uint32_t>& order;
    std::vector<BvhNode>& nodes;

    void build(std::uint32_t node, std::uint32_t first, std::uint32_t count, int depth) {
        Box bounds, cbounds;
        for (std::uint32_t i = first; i < first + count; ++i) {
            bounds.grow(items[order[i]].box);
            cbounds.grow(items[order[i]].centroid);
        }
        nodes[node].lo = bounds.lo;
        nodes[node].hi = bounds.hi;
        if (count <= kLeafSize || depth > 60) {
            nodes[node].first = first;
            nodes[node].count = count;
            return;
        }
        int best_axis = -1;
        int best_split = -1;
        float best_cost = static_cast<float>(count) * bounds.area();
        for (int axis = 0; axis < 3; ++axis) {
            const float lo = cbounds.lo[axis], hi = cbounds.hi[axis];
            if (hi - lo <= 1e-12f) continue;
            Box bins[kBins];
            std::uint32_t counts[kBins] = {};
            const float scale = static_cast<float>(kBins) / (hi - lo);
            for (std::uint32_t i = first; i < first + count; ++i) {
                const Item& it = items[order[i]];
                const int b = std::min(kBins - 1, static_cast<int>((it.centroid[axis] - lo) * scale));
                bins[b].grow(it.box);
                ++counts[b];
            }
            float left_area[kBins - 1], right_area[kBins - 1];
            std::uint32_t left_count[kBins - 1], right_count[kBins - 1];
            Box acc;
            std::uint32_t n = 0;
            for (int i = 0; i < kBins - 1; ++i) {
                acc.grow(bins[i]);
                n += counts[i];
                left_area[i] = acc.area();
                left_count[i] = n;
            }
            acc = Box();
            n = 0;
            for (int i = kBins - 1; i > 0; --i) {
                acc.grow(bins[i]);
                n += counts[i];
                right_area[i - 1] = acc.area();
                right_count[i - 1] = n;
            }
            for (int i = 0; i < kBins - 1; ++i) {
                if (left_count[i] == 0 || right_count[i] == 0) continue;
                const float cost = 0.125f * bounds.area() + static_cast<float>(left_count[i]) * left_area[i] + static_cast<float>(right_count[i]) * right_area[i];
                if (cost < best_cost) {
                    best_cost = cost;
                    best_axis = axis;
                    best_split = i;
                }
            }
        }
        std::uint32_t mid;
        if (best_axis < 0) {
            int axis = 0;
            const Vec3f e = cbounds.hi - cbounds.lo;
            if (e.y > e.x && e.y >= e.z) axis = 1;
            else if (e.z > e.x && e.z > e.y) axis = 2;
            mid = first + count / 2;
            std::nth_element(order.begin() + first, order.begin() + mid, order.begin() + first + count, [&](std::uint32_t a, std::uint32_t b) { return items[a].centroid[axis] < items[b].centroid[axis]; });
            if (count <= kLeafSize * 2 && e[axis] <= 1e-12f) {
                nodes[node].first = first;
                nodes[node].count = count;
                return;
            }
        } else {
            const float lo = cbounds.lo[best_axis], hi = cbounds.hi[best_axis];
            const float scale = static_cast<float>(kBins) / (hi - lo);
            auto split = std::partition(order.begin() + first, order.begin() + first + count, [&](std::uint32_t i) {
                const int b = std::min(kBins - 1, static_cast<int>((items[i].centroid[best_axis] - lo) * scale));
                return b <= best_split;
            });
            mid = static_cast<std::uint32_t>(split - order.begin());
            if (mid == first || mid == first + count) mid = first + count / 2;
        }
        const std::uint32_t left = static_cast<std::uint32_t>(nodes.size());
        nodes.emplace_back();
        nodes.emplace_back();
        nodes[node].first = left;
        nodes[node].count = 0;
        build(left, first, mid - first, depth + 1);
        build(left + 1, mid, first + count - mid, depth + 1);
    }
};

inline bool slab(const BvhNode& n, const Vec3f& o, const Vec3f& inv, float tmax, float& tnear) noexcept {
    float t0 = (n.lo.x - o.x) * inv.x, t1 = (n.hi.x - o.x) * inv.x;
    float tmin = std::min(t0, t1), tfar = std::max(t0, t1);
    t0 = (n.lo.y - o.y) * inv.y;
    t1 = (n.hi.y - o.y) * inv.y;
    tmin = std::max(tmin, std::min(t0, t1));
    tfar = std::min(tfar, std::max(t0, t1));
    t0 = (n.lo.z - o.z) * inv.z;
    t1 = (n.hi.z - o.z) * inv.z;
    tmin = std::max(tmin, std::min(t0, t1));
    tfar = std::min(tfar, std::max(t0, t1));
    tnear = tmin;
    return tfar >= std::max(tmin, 0.0f) && tmin <= tmax;
}

inline bool hit_tri(const Tri& tri, const Ray& r, float tmax, float& t, float& u, float& v) noexcept {
    const Vec3f p = cross(r.d, tri.e2);
    const float det = dot(tri.e1, p);
    if (std::fabs(det) < 1e-12f) return false;
    const float inv = 1.0f / det;
    const Vec3f s = r.o - tri.v0;
    u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3f q = cross(s, tri.e1);
    v = dot(r.d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = dot(tri.e2, q) * inv;
    return t > 0.0f && t < tmax;
}

Vec3f inverse_dir(const Vec3f& d) noexcept {
    auto inv = [](float x) { return std::fabs(x) > 1e-20f ? 1.0f / x : std::copysign(1e20f, x); };
    return { inv(d.x), inv(d.y), inv(d.z) };
}

} // namespace

void Bvh::build(std::vector<Tri>& tris) {
    m_nodes.clear();
    if (tris.empty()) return;
    std::vector<Item> items(tris.size());
    for (std::size_t i = 0; i < tris.size(); ++i) {
        Box b;
        b.grow(tris[i].v0);
        b.grow(tris[i].v0 + tris[i].e1);
        b.grow(tris[i].v0 + tris[i].e2);
        items[i].box = b;
        items[i].centroid = (b.lo + b.hi) * 0.5f;
    }
    std::vector<std::uint32_t> order(tris.size());
    std::iota(order.begin(), order.end(), 0u);
    m_nodes.reserve(tris.size() * 2);
    m_nodes.emplace_back();
    Builder builder{ items, order, m_nodes };
    builder.build(0, 0, static_cast<std::uint32_t>(tris.size()), 0);
    std::vector<Tri> sorted(tris.size());
    for (std::size_t i = 0; i < order.size(); ++i) sorted[i] = tris[order[i]];
    tris.swap(sorted);
}

void Bvh::refit(const std::vector<Tri>& tris) {
    for (std::size_t k = m_nodes.size(); k-- > 0;) {
        BvhNode& n = m_nodes[k];
        Box b;
        if (n.count > 0) {
            for (std::uint32_t i = n.first; i < n.first + n.count && i < tris.size(); ++i) {
                b.grow(tris[i].v0);
                b.grow(tris[i].v0 + tris[i].e1);
                b.grow(tris[i].v0 + tris[i].e2);
            }
        } else if (n.first + 1 < m_nodes.size()) {
            const BvhNode& l = m_nodes[n.first];
            const BvhNode& r = m_nodes[n.first + 1];
            b.lo = vmin(l.lo, r.lo);
            b.hi = vmax(l.hi, r.hi);
        }
        n.lo = b.lo;
        n.hi = b.hi;
    }
}

bool Bvh::intersect(const std::vector<Tri>& tris, const Ray& ray, Hit& hit, std::uint32_t require) const {
    if (m_nodes.empty()) return false;
    const Vec3f inv = inverse_dir(ray.d);
    std::uint32_t stack[128];
    int sp = 0;
    stack[sp++] = 0;
    bool found = false;
    float best = std::min(ray.tmax, hit.t);
    while (sp > 0) {
        const BvhNode& n = m_nodes[stack[--sp]];
        float tn;
        if (!slab(n, ray.o, inv, best, tn)) continue;
        if (n.count > 0) {
            for (std::uint32_t i = n.first; i < n.first + n.count; ++i) {
                const Tri& tri = tris[i];
                if ((tri.flags & require) != require) continue;
                float t, u, v;
                if (hit_tri(tri, ray, best, t, u, v)) {
                    best = t;
                    hit.t = t;
                    hit.tri = i;
                    hit.u = u;
                    hit.v = v;
                    found = true;
                }
            }
        } else {
            const BvhNode& a = m_nodes[n.first];
            const BvhNode& b = m_nodes[n.first + 1];
            float ta, tb;
            const bool ha = slab(a, ray.o, inv, best, ta);
            const bool hb = slab(b, ray.o, inv, best, tb);
            if (ha && hb && sp < 126) {
                if (ta < tb) {
                    stack[sp++] = n.first + 1;
                    stack[sp++] = n.first;
                } else {
                    stack[sp++] = n.first;
                    stack[sp++] = n.first + 1;
                }
            } else if (ha && sp < 127) {
                stack[sp++] = n.first;
            } else if (hb && sp < 127) {
                stack[sp++] = n.first + 1;
            }
        }
    }
    return found;
}

bool Bvh::any(const std::vector<Tri>& tris, const Ray& ray, std::uint32_t require) const {
    if (m_nodes.empty()) return false;
    const Vec3f inv = inverse_dir(ray.d);
    std::uint32_t stack[128];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const BvhNode& n = m_nodes[stack[--sp]];
        float tn;
        if (!slab(n, ray.o, inv, ray.tmax, tn)) continue;
        if (n.count > 0) {
            for (std::uint32_t i = n.first; i < n.first + n.count; ++i) {
                const Tri& tri = tris[i];
                if ((tri.flags & require) != require) continue;
                float t, u, v;
                if (hit_tri(tri, ray, ray.tmax, t, u, v)) return true;
            }
        } else if (sp < 126) {
            stack[sp++] = n.first;
            stack[sp++] = n.first + 1;
        }
    }
    return false;
}

void Bvh::build_boxes(const std::vector<Vec3f>& lo, const std::vector<Vec3f>& hi, std::vector<std::uint32_t>& order) {
    m_nodes.clear();
    order.clear();
    const std::size_t n = std::min(lo.size(), hi.size());
    if (n == 0) return;
    std::vector<Item> items(n);
    for (std::size_t i = 0; i < n; ++i) {
        items[i].box.lo = lo[i];
        items[i].box.hi = hi[i];
        items[i].centroid = (lo[i] + hi[i]) * 0.5f;
    }
    order.resize(n);
    std::iota(order.begin(), order.end(), 0u);
    m_nodes.reserve(n * 2);
    m_nodes.emplace_back();
    Builder builder{ items, order, m_nodes };
    builder.build(0, 0, static_cast<std::uint32_t>(n), 0);
}

void Bvh::refit_boxes(const std::vector<Vec3f>& lo, const std::vector<Vec3f>& hi) {
    for (std::size_t k = m_nodes.size(); k-- > 0;) {
        BvhNode& n = m_nodes[k];
        Box b;
        if (n.count > 0) {
            for (std::uint32_t i = n.first; i < n.first + n.count && i < lo.size() && i < hi.size(); ++i) {
                b.grow(lo[i]);
                b.grow(hi[i]);
            }
        } else if (n.first + 1 < m_nodes.size()) {
            const BvhNode& l = m_nodes[n.first];
            const BvhNode& r = m_nodes[n.first + 1];
            b.lo = vmin(l.lo, r.lo);
            b.hi = vmax(l.hi, r.hi);
        }
        n.lo = b.lo;
        n.hi = b.hi;
    }
}

bool intersect_instances(const Compiled& c, const Ray& ray, Hit& hit, std::uint32_t require) {
    const std::vector<BvhNode>& nodes = c.tlas_bvh.nodes();
    if (nodes.empty() || c.tlas.empty()) return false;
    const Vec3f inv = inverse_dir(ray.d);
    std::uint32_t stack[128];
    int sp = 0;
    stack[sp++] = 0;
    bool found = false;
    float best = std::min(ray.tmax, hit.t);
    while (sp > 0) {
        const BvhNode& n = nodes[stack[--sp]];
        float tn;
        if (!slab(n, ray.o, inv, best, tn)) continue;
        if (n.count > 0) {
            for (std::uint32_t i = n.first; i < n.first + n.count && i < c.tlas.size(); ++i) {
                const TlasInstance& ti = c.tlas[i];
                if ((ti.flags & require) != require) continue;
                if (ti.mesh >= c.mesh_blas.size() || !c.mesh_blas[ti.mesh]) continue;
                const Blas& b = *c.mesh_blas[ti.mesh];
                Ray local{ ti.to_object.point(ray.o), ti.to_object.vector(ray.d), best };
                Hit lh;
                lh.t = best;
                if (b.bvh.intersect(b.tris, local, lh, 0) && lh.t < best) {
                    best = lh.t;
                    hit.t = lh.t;
                    hit.u = lh.u;
                    hit.v = lh.v;
                    hit.tri = kInstancedTri;
                    hit.inst = i;
                    hit.blas_tri = lh.tri;
                    found = true;
                }
            }
        } else {
            const BvhNode& a = nodes[n.first];
            const BvhNode& b = nodes[n.first + 1];
            float ta, tb;
            const bool ha = slab(a, ray.o, inv, best, ta);
            const bool hb = slab(b, ray.o, inv, best, tb);
            if (ha && hb && sp < 126) {
                if (ta < tb) {
                    stack[sp++] = n.first + 1;
                    stack[sp++] = n.first;
                } else {
                    stack[sp++] = n.first;
                    stack[sp++] = n.first + 1;
                }
            } else if (ha && sp < 127) {
                stack[sp++] = n.first;
            } else if (hb && sp < 127) {
                stack[sp++] = n.first + 1;
            }
        }
    }
    return found;
}

bool any_instance(const Compiled& c, const Ray& ray, std::uint32_t require) {
    const std::vector<BvhNode>& nodes = c.tlas_bvh.nodes();
    if (nodes.empty() || c.tlas.empty()) return false;
    const Vec3f inv = inverse_dir(ray.d);
    std::uint32_t stack[128];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const BvhNode& n = nodes[stack[--sp]];
        float tn;
        if (!slab(n, ray.o, inv, ray.tmax, tn)) continue;
        if (n.count > 0) {
            for (std::uint32_t i = n.first; i < n.first + n.count && i < c.tlas.size(); ++i) {
                const TlasInstance& ti = c.tlas[i];
                if ((ti.flags & require) != require) continue;
                if (ti.mesh >= c.mesh_blas.size() || !c.mesh_blas[ti.mesh]) continue;
                const Blas& b = *c.mesh_blas[ti.mesh];
                Ray local{ ti.to_object.point(ray.o), ti.to_object.vector(ray.d), ray.tmax };
                if (b.bvh.any(b.tris, local, 0)) return true;
            }
        } else if (sp < 126) {
            stack[sp++] = n.first;
            stack[sp++] = n.first + 1;
        }
    }
    return false;
}

} // namespace detail
} // namespace lighting
} // namespace fizmo
