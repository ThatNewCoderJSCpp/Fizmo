#include "geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace fizmo {
namespace numerics {

Point2 operator+(Point2 a, Point2 b) { return Point2{ a.x + b.x, a.y + b.y }; }
Point2 operator-(Point2 a, Point2 b) { return Point2{ a.x - b.x, a.y - b.y }; }
Point2 operator*(Point2 a, double s) { return Point2{ a.x * s, a.y * s }; }
Point3 operator+(Point3 a, Point3 b) { return Point3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
Point3 operator-(Point3 a, Point3 b) { return Point3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
Point3 operator*(Point3 a, double s) { return Point3{ a.x * s, a.y * s, a.z * s }; }
double dot(Point3 a, Point3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Point3 cross(Point3 a, Point3 b) { return Point3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
double length(Point3 a) { return std::sqrt(dot(a, a)); }
bool finite(Point2 a) { return std::isfinite(a.x) && std::isfinite(a.y); }
bool finite(Point3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

Point3 normalized(Point3 a) {
    const double l = length(a);
    if (!(l > 0.0) || !std::isfinite(l)) return Point3{ 0.0, 0.0, 1.0 };
    return a * (1.0 / l);
}

namespace {

const double k_nan = std::numeric_limits<double>::quiet_NaN();

using Stop = std::array<double, 3>;

const Stop k_viridis[] = {
    { 0.267, 0.005, 0.329 }, { 0.283, 0.141, 0.458 }, { 0.254, 0.265, 0.530 }, { 0.207, 0.372, 0.553 },
    { 0.164, 0.471, 0.558 }, { 0.128, 0.567, 0.551 }, { 0.135, 0.659, 0.518 }, { 0.267, 0.749, 0.441 },
    { 0.478, 0.821, 0.318 }, { 0.741, 0.873, 0.150 }, { 0.993, 0.906, 0.144 }
};

const Stop k_turbo[] = {
    { 0.190, 0.072, 0.232 }, { 0.255, 0.355, 0.784 }, { 0.163, 0.613, 0.983 }, { 0.106, 0.829, 0.761 },
    { 0.350, 0.968, 0.420 }, { 0.700, 0.973, 0.218 }, { 0.950, 0.792, 0.207 }, { 0.988, 0.522, 0.143 },
    { 0.863, 0.255, 0.051 }, { 0.600, 0.067, 0.008 }
};

const Stop k_magma[] = {
    { 0.001, 0.000, 0.014 }, { 0.130, 0.069, 0.317 }, { 0.372, 0.092, 0.500 }, { 0.592, 0.168, 0.505 },
    { 0.819, 0.255, 0.442 }, { 0.973, 0.442, 0.375 }, { 0.995, 0.700, 0.522 }, { 0.987, 0.991, 0.750 }
};

const Stop k_coolwarm[] = {
    { 0.230, 0.299, 0.754 }, { 0.483, 0.615, 0.968 }, { 0.865, 0.865, 0.865 }, { 0.958, 0.603, 0.482 }, { 0.706, 0.016, 0.150 }
};

const Stop k_ocean[] = {
    { 0.106, 0.227, 0.490 }, { 0.137, 0.494, 0.706 }, { 0.196, 0.722, 0.706 }, { 0.565, 0.855, 0.459 }, { 0.953, 0.890, 0.420 }
};

const Stop k_gray[] = { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 } };

Rgba8 ramp(const Stop* stops, std::size_t n, double t) {
    if (!std::isfinite(t)) return Rgba8{ 128, 128, 128, 255 };
    t = std::min(1.0, std::max(0.0, t)) * static_cast<double>(n - 1);
    const std::size_t i = std::min(static_cast<std::size_t>(t), n - 2);
    const double f = t - static_cast<double>(i);
    const Stop& a = stops[i];
    const Stop& b = stops[i + 1];
    auto c = [&](int k) { return static_cast<std::uint8_t>(std::lround((a[static_cast<std::size_t>(k)] + (b[static_cast<std::size_t>(k)] - a[static_cast<std::size_t>(k)]) * f) * 255.0)); };
    return Rgba8{ c(0), c(1), c(2), 255 };
}

} // namespace

const std::vector<std::string>& colormap_names() {
    static const std::vector<std::string> names = { "viridis", "magma", "turbo", "coolwarm", "ocean", "gray" };
    return names;
}

Rgba8 colormap(const std::string& name, double t) {
    if (name == "turbo") return ramp(k_turbo, sizeof k_turbo / sizeof k_turbo[0], t);
    if (name == "magma") return ramp(k_magma, sizeof k_magma / sizeof k_magma[0], t);
    if (name == "coolwarm") return ramp(k_coolwarm, sizeof k_coolwarm / sizeof k_coolwarm[0], t);
    if (name == "ocean") return ramp(k_ocean, sizeof k_ocean / sizeof k_ocean[0], t);
    if (name == "gray" || name == "grey") return ramp(k_gray, 2, t);
    return ramp(k_viridis, sizeof k_viridis / sizeof k_viridis[0], t);
}

Rgba8 colormap_range(const std::string& name, double value, double lo, double hi) {
    return colormap(name, hi > lo ? (value - lo) / (hi - lo) : 0.5);
}

Point3 ScalarGrid3::position(int i, int j, int k) const {
    const int idx[3] = { i, j, k };
    double p[3];
    for (int a = 0; a < 3; ++a) p[a] = n[a] > 1 ? lo[a] + (hi[a] - lo[a]) * idx[a] / (n[a] - 1) : lo[a];
    return Point3{ p[0], p[1], p[2] };
}

ScalarGrid2 sample_grid(const std::function<double(double, double)>& f, double x0, double x1, double y0, double y1, int nx, int ny) {
    ScalarGrid2 g;
    g.nx = std::max(2, nx);
    g.ny = std::max(2, ny);
    g.x0 = x0;
    g.x1 = x1;
    g.y0 = y0;
    g.y1 = y1;
    g.v.resize(static_cast<std::size_t>(g.nx) * g.ny);
    for (int j = 0; j < g.ny; ++j) {
        for (int i = 0; i < g.nx; ++i) g.v[static_cast<std::size_t>(j) * g.nx + i] = f(g.x(i), g.y(j));
    }
    return g;
}

ScalarGrid3 sample_grid(const std::function<double(double, double, double)>& f, const double lo[3], const double hi[3], int nx, int ny, int nz) {
    ScalarGrid3 g;
    g.n[0] = std::max(2, nx);
    g.n[1] = std::max(2, ny);
    g.n[2] = std::max(2, nz);
    for (int a = 0; a < 3; ++a) {
        g.lo[a] = lo[a];
        g.hi[a] = hi[a];
    }
    g.v.resize(static_cast<std::size_t>(g.n[0]) * g.n[1] * g.n[2]);
    for (int k = 0; k < g.n[2]; ++k) {
        for (int j = 0; j < g.n[1]; ++j) {
            for (int i = 0; i < g.n[0]; ++i) {
                const Point3 p = g.position(i, j, k);
                g.v[(static_cast<std::size_t>(k) * g.n[1] + j) * g.n[0] + i] = f(p.x, p.y, p.z);
            }
        }
    }
    return g;
}

std::vector<Point2> marching_squares(const ScalarGrid2& g, double level, const std::function<double(double, double)>& refine) {
    const int nx = g.nx;
    const int ny = g.ny;
    std::vector<Point2> out;
    if (nx < 2 || ny < 2) return out;
    const std::size_t hcount = static_cast<std::size_t>(nx - 1) * ny;
    const std::size_t total = hcount + static_cast<std::size_t>(nx) * (ny - 1);
    std::vector<Point2> crossings(total, Point2{ k_nan, k_nan });
    std::vector<char> valid(total, 0);
    auto val = [&](int i, int j) { return g.at(i, j) - level; };
    auto edge = [&](std::size_t e, int ia, int ja, int ib, int jb) {
        const double a = val(ia, ja);
        const double b = val(ib, jb);
        if (!std::isfinite(a) || !std::isfinite(b)) return;
        if ((a < 0.0) == (b < 0.0)) return;
        const Point2 pa{ g.x(ia), g.y(ja) };
        const Point2 pb{ g.x(ib), g.y(jb) };
        double t = a / (a - b);
        if (refine) {
            double lo = 0.0, hi = 1.0;
            double flo = a;
            for (int it = 0; it < 30; ++it) {
                const Point2 p = pa + (pb - pa) * t;
                const double fm = refine(p.x, p.y) - level;
                if (!std::isfinite(fm)) break;
                if (std::fabs(fm) < 1e-13 * (std::fabs(a) + std::fabs(b))) break;
                if ((fm < 0.0) == (flo < 0.0)) {
                    lo = t;
                    flo = fm;
                } else {
                    hi = t;
                }
                t = 0.5 * (lo + hi);
                if (hi - lo < 1e-9) break;
            }
        }
        crossings[e] = pa + (pb - pa) * t;
        valid[e] = 1;
    };
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i + 1 < nx; ++i) edge(static_cast<std::size_t>(j) * (nx - 1) + i, i, j, i + 1, j);
    }
    for (int j = 0; j + 1 < ny; ++j) {
        for (int i = 0; i < nx; ++i) edge(hcount + static_cast<std::size_t>(j) * nx + i, i, j, i, j + 1);
    }
    const double tiny = 1e-9 * std::min(std::fabs(g.x1 - g.x0) / (nx - 1), std::fabs(g.y1 - g.y0) / (ny - 1));
    auto seg = [&](std::size_t a, std::size_t b) {
        if (!valid[a] || !valid[b]) return;
        if (std::fabs(crossings[a].x - crossings[b].x) <= tiny && std::fabs(crossings[a].y - crossings[b].y) <= tiny) return;
        out.push_back(crossings[a]);
        out.push_back(crossings[b]);
    };
    for (int j = 0; j + 1 < ny; ++j) {
        for (int i = 0; i + 1 < nx; ++i) {
            const double v0 = val(i, j);
            const double v1 = val(i + 1, j);
            const double v2 = val(i + 1, j + 1);
            const double v3 = val(i, j + 1);
            if (!std::isfinite(v0) || !std::isfinite(v1) || !std::isfinite(v2) || !std::isfinite(v3)) continue;
            const int code = (v0 < 0.0 ? 1 : 0) | (v1 < 0.0 ? 2 : 0) | (v2 < 0.0 ? 4 : 0) | (v3 < 0.0 ? 8 : 0);
            if (code == 0 || code == 15) continue;
            const std::size_t e0 = static_cast<std::size_t>(j) * (nx - 1) + i;
            const std::size_t e2 = static_cast<std::size_t>(j + 1) * (nx - 1) + i;
            const std::size_t e3 = hcount + static_cast<std::size_t>(j) * nx + i;
            const std::size_t e1 = hcount + static_cast<std::size_t>(j) * nx + i + 1;
            switch (code) {
            case 1: case 14: seg(e3, e0); break;
            case 2: case 13: seg(e0, e1); break;
            case 3: case 12: seg(e3, e1); break;
            case 4: case 11: seg(e1, e2); break;
            case 6: case 9: seg(e0, e2); break;
            case 7: case 8: seg(e3, e2); break;
            case 5: case 10: {
                const bool center_neg = 0.25 * (v0 + v1 + v2 + v3) < 0.0;
                if ((code == 5) == center_neg) {
                    seg(e3, e2);
                    seg(e0, e1);
                } else {
                    seg(e3, e0);
                    seg(e1, e2);
                }
                break;
            }
            default: break;
            }
        }
    }
    return out;
}

std::vector<std::vector<Point2>> join_segments(const std::vector<Point2>& segments, double tolerance) {
    std::vector<std::vector<Point2>> out;
    const std::size_t count = segments.size() / 2;
    if (count == 0) return out;
    auto key = [&](Point2 p) {
        const double q = tolerance > 0.0 ? tolerance : 1e-9;
        return std::make_pair(std::llround(p.x / q), std::llround(p.y / q));
    };
    std::multimap<std::pair<long long, long long>, std::size_t> ends;
    for (std::size_t s = 0; s < count; ++s) {
        ends.insert({ key(segments[2 * s]), s });
        ends.insert({ key(segments[2 * s + 1]), s });
    }
    std::vector<char> used(count, 0);
    auto take = [&](Point2 at, std::size_t& seg, bool& forward) {
        auto range = ends.equal_range(key(at));
        for (auto it = range.first; it != range.second; ++it) {
            if (used[it->second]) continue;
            seg = it->second;
            forward = key(segments[2 * seg]) == key(at);
            return true;
        }
        return false;
    };
    for (std::size_t s = 0; s < count; ++s) {
        if (used[s]) continue;
        used[s] = 1;
        std::vector<Point2> line = { segments[2 * s], segments[2 * s + 1] };
        for (int dir = 0; dir < 2; ++dir) {
            while (true) {
                std::size_t next = 0;
                bool forward = true;
                const Point2 tip = dir == 0 ? line.back() : line.front();
                if (!take(tip, next, forward)) break;
                used[next] = 1;
                const Point2 other_end = forward ? segments[2 * next + 1] : segments[2 * next];
                if (dir == 0) line.push_back(other_end);
                else line.insert(line.begin(), other_end);
            }
        }
        out.push_back(std::move(line));
    }
    return out;
}

std::vector<Point3> marching_tetrahedra(const ScalarGrid3& g, double level) {
    static const int corner[8][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } };
    static const int tets[6][4] = { { 0, 5, 1, 6 }, { 0, 1, 2, 6 }, { 0, 2, 3, 6 }, { 0, 3, 7, 6 }, { 0, 7, 4, 6 }, { 0, 4, 5, 6 } };
    std::vector<Point3> tris;
    for (int k = 0; k + 1 < g.n[2]; ++k) {
        for (int j = 0; j + 1 < g.n[1]; ++j) {
            for (int i = 0; i + 1 < g.n[0]; ++i) {
                double v[8];
                Point3 p[8];
                bool ok = true;
                int neg = 0;
                for (int c = 0; c < 8; ++c) {
                    v[c] = g.at(i + corner[c][0], j + corner[c][1], k + corner[c][2]) - level;
                    if (!std::isfinite(v[c])) ok = false;
                    if (v[c] < 0.0) ++neg;
                    p[c] = g.position(i + corner[c][0], j + corner[c][1], k + corner[c][2]);
                }
                if (!ok || neg == 0 || neg == 8) continue;
                for (const auto& t : tets) {
                    int inside[4];
                    int outside[4];
                    int ni = 0;
                    int no = 0;
                    for (int q = 0; q < 4; ++q) {
                        if (v[t[q]] < 0.0) inside[ni++] = t[q];
                        else outside[no++] = t[q];
                    }
                    auto lerp = [&](int x, int y) {
                        const double s = v[x] / (v[x] - v[y]);
                        return p[x] + (p[y] - p[x]) * s;
                    };
                    if (ni == 1 || no == 1) {
                        const int single = ni == 1 ? inside[0] : outside[0];
                        const int* rest = ni == 1 ? outside : inside;
                        tris.push_back(lerp(single, rest[0]));
                        tris.push_back(lerp(single, rest[1]));
                        tris.push_back(lerp(single, rest[2]));
                    } else if (ni == 2) {
                        const Point3 q0 = lerp(inside[0], outside[0]);
                        const Point3 q1 = lerp(inside[0], outside[1]);
                        const Point3 q2 = lerp(inside[1], outside[1]);
                        const Point3 q3 = lerp(inside[1], outside[0]);
                        tris.push_back(q0);
                        tris.push_back(q1);
                        tris.push_back(q2);
                        tris.push_back(q0);
                        tris.push_back(q2);
                        tris.push_back(q3);
                    }
                }
            }
        }
    }
    return tris;
}

void TriangleMesh::compute_normals() {
    normals.assign(positions.size(), Point3{ 0.0, 0.0, 0.0 });
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        const Point3& a = positions[indices[t]];
        const Point3& b = positions[indices[t + 1]];
        const Point3& c = positions[indices[t + 2]];
        const Point3 nrm = cross(b - a, c - a);
        if (!finite(nrm)) continue;
        for (int k = 0; k < 3; ++k) normals[indices[t + static_cast<std::size_t>(k)]] = normals[indices[t + static_cast<std::size_t>(k)]] + nrm;
    }
    for (Point3& nrm : normals) nrm = normalized(nrm);
}

void TriangleMesh::color_by(const std::vector<double>& values, const std::string& map, double lo, double hi) {
    colors.resize(positions.size());
    for (std::size_t i = 0; i < positions.size() && i < values.size(); ++i) colors[i] = colormap_range(map, values[i], lo, hi);
}

void TriangleMesh::color_by_height(const std::string& map) {
    double lo = INFINITY, hi = -INFINITY;
    std::vector<double> z;
    z.reserve(positions.size());
    for (const Point3& p : positions) {
        z.push_back(p.z);
        if (std::isfinite(p.z)) {
            lo = std::min(lo, p.z);
            hi = std::max(hi, p.z);
        }
    }
    color_by(z, map, lo, hi);
}

TriangleMesh mesh_from_soup(const std::vector<Point3>& triangles) {
    TriangleMesh m;
    m.positions = triangles;
    m.indices.resize(triangles.size());
    for (std::size_t i = 0; i < triangles.size(); ++i) m.indices[i] = static_cast<std::uint32_t>(i);
    m.compute_normals();
    return m;
}

TriangleMesh surface_mesh(const std::function<Point3(double u, double v)>& f, double u0, double u1, double v0, double v1, int nu, int nv) {
    TriangleMesh m;
    nu = std::max(2, nu);
    nv = std::max(2, nv);
    m.positions.resize(static_cast<std::size_t>(nu) * nv);
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const double u = u0 + (u1 - u0) * i / (nu - 1);
            const double v = v0 + (v1 - v0) * j / (nv - 1);
            m.positions[static_cast<std::size_t>(j) * nu + i] = f(u, v);
        }
    }
    for (int j = 0; j + 1 < nv; ++j) {
        for (int i = 0; i + 1 < nu; ++i) {
            const std::uint32_t a = static_cast<std::uint32_t>(j * nu + i);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(nu);
            const std::uint32_t d = c + 1;
            if (!finite(m.positions[a]) || !finite(m.positions[b]) || !finite(m.positions[c]) || !finite(m.positions[d])) continue;
            m.indices.insert(m.indices.end(), { a, b, d, a, d, c });
        }
    }
    m.compute_normals();
    return m;
}

TriangleMesh height_mesh(const ScalarGrid2& g) {
    return surface_mesh([&](double u, double v) {
        const int i = std::min(g.nx - 1, std::max(0, static_cast<int>(std::lround(u))));
        const int j = std::min(g.ny - 1, std::max(0, static_cast<int>(std::lround(v))));
        return Point3{ g.x(i), g.y(j), g.at(i, j) };
    }, 0.0, g.nx - 1.0, 0.0, g.ny - 1.0, g.nx, g.ny);
}

std::vector<std::vector<Point3>> mesh_grid_lines(const std::function<Point3(double u, double v)>& f, double u0, double u1, double v0, double v1, int lines_u, int lines_v, int samples) {
    std::vector<std::vector<Point3>> out;
    samples = std::max(2, samples);
    for (int i = 0; i < lines_u; ++i) {
        const double u = lines_u > 1 ? u0 + (u1 - u0) * i / (lines_u - 1) : u0;
        std::vector<Point3> line;
        for (int s = 0; s < samples; ++s) line.push_back(f(u, v0 + (v1 - v0) * s / (samples - 1)));
        out.push_back(std::move(line));
    }
    for (int j = 0; j < lines_v; ++j) {
        const double v = lines_v > 1 ? v0 + (v1 - v0) * j / (lines_v - 1) : v0;
        std::vector<Point3> line;
        for (int s = 0; s < samples; ++s) line.push_back(f(u0 + (u1 - u0) * s / (samples - 1), v));
        out.push_back(std::move(line));
    }
    return out;
}

namespace {

template <class P, class D>
std::vector<double> adaptive_parameters(const std::function<P(double)>& f, double t0, double t1, double tolerance, int initial, int max_depth, const D& deviation) {
    initial = std::max(2, initial);
    std::vector<double> ts;
    std::vector<P> ps;
    for (int i = 0; i <= initial; ++i) {
        const double t = t0 + (t1 - t0) * i / initial;
        ts.push_back(t);
        ps.push_back(f(t));
    }
    for (int depth = 0; depth < max_depth; ++depth) {
        std::vector<double> nt;
        std::vector<P> np;
        bool changed = false;
        nt.push_back(ts[0]);
        np.push_back(ps[0]);
        for (std::size_t i = 0; i + 1 < ts.size(); ++i) {
            const double tm = 0.5 * (ts[i] + ts[i + 1]);
            const P pm = f(tm);
            if (deviation(ps[i], pm, ps[i + 1]) > tolerance) {
                nt.push_back(tm);
                np.push_back(pm);
                changed = true;
            }
            nt.push_back(ts[i + 1]);
            np.push_back(ps[i + 1]);
        }
        ts.swap(nt);
        ps.swap(np);
        if (!changed) break;
    }
    return ts;
}

double deviation2(Point2 a, Point2 m, Point2 b) {
    if (!finite(a) || !finite(b)) return finite(m) ? 1e300 : 0.0;
    if (!finite(m)) return 1e300;
    const Point2 mid = (a + b) * 0.5;
    return std::hypot(m.x - mid.x, m.y - mid.y);
}

double deviation3(Point3 a, Point3 m, Point3 b) {
    if (!finite(a) || !finite(b)) return finite(m) ? 1e300 : 0.0;
    if (!finite(m)) return 1e300;
    return length(m - (a + b) * 0.5);
}

} // namespace

std::vector<double> sample_parameters(const std::function<Point2(double)>& f, double t0, double t1, double tolerance, int initial, int max_depth) {
    return adaptive_parameters<Point2>(f, t0, t1, tolerance, initial, max_depth, deviation2);
}

std::vector<Point2> sample_curve(const std::function<Point2(double)>& f, double t0, double t1, double tolerance, int initial, int max_depth) {
    std::vector<Point2> out;
    for (double t : adaptive_parameters<Point2>(f, t0, t1, tolerance, initial, max_depth, deviation2)) out.push_back(f(t));
    return out;
}

std::vector<Point3> sample_curve(const std::function<Point3(double)>& f, double t0, double t1, double tolerance, int initial, int max_depth) {
    std::vector<Point3> out;
    for (double t : adaptive_parameters<Point3>(f, t0, t1, tolerance, initial, max_depth, deviation3)) out.push_back(f(t));
    return out;
}

} // namespace numerics
} // namespace fizmo
