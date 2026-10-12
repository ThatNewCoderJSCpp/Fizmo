#include "fizmo_library.hpp"
#include "svg_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace fizmo {
namespace images {
namespace svg_detail {

namespace {

constexpr double kPi = 3.14159265358979323846;

Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
Vec2 operator*(Vec2 a, double s) { return { a.x * s, a.y * s }; }
double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
double length(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }

double signed_area(const std::vector<Vec2>& pts) {
    double a = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const Vec2& p = pts[i];
        const Vec2& q = pts[(i + 1) % pts.size()];
        a += p.x * q.y - q.x * p.y;
    }
    return a * 0.5;
}

void push_oriented(std::vector<Polyline>& out, std::vector<Vec2> pts) {
    if (pts.size() < 3) return;
    if (signed_area(pts) < 0) std::reverse(pts.begin(), pts.end());
    Polyline pl;
    pl.pts = std::move(pts);
    pl.closed = true;
    out.push_back(std::move(pl));
}

int arc_steps(double radius, double angle, double tolerance) {
    if (radius <= tolerance) return std::max(2, static_cast<int>(std::ceil(std::fabs(angle) / (kPi / 2))));
    const double step = 2.0 * std::acos(std::max(-1.0, 1.0 - tolerance / radius));
    if (!(step > 0)) return 64;
    return std::max(2, std::min(1024, static_cast<int>(std::ceil(std::fabs(angle) / step))));
}

void add_arc_points(std::vector<Vec2>& pts, Vec2 c, double r, double a0, double sweep, double tolerance) {
    const int n = arc_steps(r, sweep, tolerance);
    for (int i = 0; i <= n; ++i) {
        const double a = a0 + sweep * i / n;
        pts.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
}

void add_circle(std::vector<Polyline>& out, Vec2 c, double r, double tolerance) {
    std::vector<Vec2> pts;
    const int n = std::max(8, arc_steps(r, 2 * kPi, tolerance));
    for (int i = 0; i < n; ++i) {
        const double a = 2 * kPi * i / n;
        pts.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
    push_oriented(out, std::move(pts));
}

class NumberParser {
public:
    explicit NumberParser(std::string_view s) : m_s(s) {}

    void skip_ws_comma() {
        while (m_i < m_s.size() && (std::isspace(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == ',')) ++m_i;
    }

    void skip_ws() {
        while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
    }

    bool at_end() {
        skip_ws();
        return m_i >= m_s.size();
    }

    char peek() {
        skip_ws();
        return m_i < m_s.size() ? m_s[m_i] : '\0';
    }

    void advance() { ++m_i; }

    bool number(double& out) {
        skip_ws_comma();
        const std::size_t st = m_i;
        if (m_i < m_s.size() && (m_s[m_i] == '+' || m_s[m_i] == '-')) ++m_i;
        bool digits = false;
        while (m_i < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_i]))) { ++m_i; digits = true; }
        if (m_i < m_s.size() && m_s[m_i] == '.') {
            ++m_i;
            while (m_i < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_i]))) { ++m_i; digits = true; }
        }
        if (!digits) {
            m_i = st;
            return false;
        }
        if (m_i < m_s.size() && (m_s[m_i] == 'e' || m_s[m_i] == 'E')) {
            std::size_t k = m_i + 1;
            if (k < m_s.size() && (m_s[k] == '+' || m_s[k] == '-')) ++k;
            if (k < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[k]))) {
                m_i = k;
                while (m_i < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
            }
        }
        out = std::strtod(std::string(m_s.substr(st, m_i - st)).c_str(), nullptr);
        return std::isfinite(out);
    }

    bool flag(bool& out) {
        skip_ws_comma();
        if (m_i < m_s.size() && (m_s[m_i] == '0' || m_s[m_i] == '1')) {
            out = m_s[m_i++] == '1';
            return true;
        }
        return false;
    }

private:
    std::string_view m_s;
    std::size_t      m_i = 0;
};

void flatten_cubic(std::vector<Vec2>& pts, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double tol) {
    const Vec2 d1 = p0 - p1 * 2 + p2, d2 = p1 - p2 * 2 + p3;
    const double m = std::max(length(d1), length(d2));
    const int n = std::max(1, std::min(4096, static_cast<int>(std::ceil(std::sqrt(0.75 * m / tol)))));
    for (int i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / n, u = 1 - t;
        const double a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        pts.push_back({ a * p0.x + b * p1.x + c * p2.x + d * p3.x, a * p0.y + b * p1.y + c * p2.y + d * p3.y });
    }
}

void flatten_quad(std::vector<Vec2>& pts, Vec2 p0, Vec2 p1, Vec2 p2, double tol) {
    const double m = length(p0 - p1 * 2 + p2);
    const int n = std::max(1, std::min(4096, static_cast<int>(std::ceil(std::sqrt(0.25 * m / tol)))));
    for (int i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / n, u = 1 - t;
        pts.push_back({ u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x, u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y });
    }
}

void extend_cubic(double p0, double p1, double p2, double p3, double& lo, double& hi) {
    const double a = -p0 + 3 * p1 - 3 * p2 + p3, b = 2 * (p0 - 2 * p1 + p2), c = p1 - p0;
    auto eval = [&](double t) {
        const double u = 1 - t;
        return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
    };
    auto consider = [&](double t) {
        if (t > 0 && t < 1) {
            const double v = eval(t);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    };
    if (std::fabs(a) < 1e-12) {
        if (std::fabs(b) > 1e-12) consider(-c / b);
        return;
    }
    const double disc = b * b - 4 * a * c;
    if (disc < 0) return;
    const double sq = std::sqrt(disc);
    consider((-b + sq) / (2 * a));
    consider((-b - sq) / (2 * a));
}

void extend_quad(double p0, double p1, double p2, double& lo, double& hi) {
    const double den = p0 - 2 * p1 + p2;
    if (std::fabs(den) < 1e-12) return;
    const double t = (p0 - p1) / den;
    if (t > 0 && t < 1) {
        const double u = 1 - t, v = u * u * p0 + 2 * u * t * p1 + t * t * p2;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
}

} // namespace

void Path::arc_to(Vec2 from, double rx, double ry, double angle, bool large, bool sweep, Vec2 to) {
    if (from.x == to.x && from.y == to.y) return;
    rx = std::fabs(rx);
    ry = std::fabs(ry);
    if (rx == 0 || ry == 0) {
        line_to(to);
        return;
    }

    const double phi = angle * kPi / 180.0, cp = std::cos(phi), sp = std::sin(phi);
    const double dx = (from.x - to.x) / 2, dy = (from.y - to.y) / 2;
    const double x1 = cp * dx + sp * dy, y1 = -sp * dx + cp * dy;
    const double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    if (lambda > 1) {
        const double s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
    }

    const double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    const double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    double coef = den > 0 ? std::sqrt(std::max(0.0, num / den)) : 0;
    if (large == sweep) coef = -coef;
    const double cx1 = coef * rx * y1 / ry, cy1 = -coef * ry * x1 / rx;
    const double cx = cp * cx1 - sp * cy1 + (from.x + to.x) / 2, cy = sp * cx1 + cp * cy1 + (from.y + to.y) / 2;

    auto ang = [](double ux, double uy, double vx, double vy) {
        const double a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        return a;
    };

    const double theta1 = ang(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
    double dtheta = ang((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
    if (!sweep && dtheta > 0) dtheta -= 2 * kPi;
    else if (sweep && dtheta < 0) dtheta += 2 * kPi;

    const int segs = std::max(1, static_cast<int>(std::ceil(std::fabs(dtheta) / (kPi / 2) - 1e-9)));
    const double delta = dtheta / segs;
    const double k = 4.0 / 3.0 * std::tan(delta / 4);
    double t = theta1;

    for (int i = 0; i < segs; ++i) {
        const double c0 = std::cos(t), s0 = std::sin(t), c1 = std::cos(t + delta), s1 = std::sin(t + delta);
        auto map = [&](double ex, double ey) -> Vec2 { return { cp * rx * ex - sp * ry * ey + cx, sp * rx * ex + cp * ry * ey + cy }; };
        const Vec2 q1 = map(c0 - k * s0, s0 + k * c0);
        const Vec2 q2 = map(c1 + k * s1, s1 - k * c1);
        const Vec2 q3 = i + 1 == segs ? to : map(c1, s1);
        cubic_to(q1, q2, q3);
        t += delta;
    }
}

std::vector<Polyline> flatten(const Path& path, const Matrix& m, double tolerance) {
    std::vector<Polyline> out;
    Polyline cur;
    Vec2 last{}, start{};
    bool have = false;

    auto flush = [&]() {
        if (cur.pts.size() >= 1) out.push_back(std::move(cur));
        cur = Polyline();
    };

    for (const Segment& s : path.segs) {
        switch (s.kind) {
            case SegKind::Move:
                flush();
                last = start = s.p[0];
                cur.pts.push_back(m.apply(last));
                have = true;
                break;
            case SegKind::Line:
                if (!have) { cur.pts.push_back(m.apply(last)); have = true; }
                last = s.p[0];
                cur.pts.push_back(m.apply(last));
                break;
            case SegKind::Cubic:
                if (!have) { cur.pts.push_back(m.apply(last)); have = true; }
                flatten_cubic(cur.pts, m.apply(last), m.apply(s.p[0]), m.apply(s.p[1]), m.apply(s.p[2]), tolerance);
                last = s.p[2];
                break;
            case SegKind::Quad:
                if (!have) { cur.pts.push_back(m.apply(last)); have = true; }
                flatten_quad(cur.pts, m.apply(last), m.apply(s.p[0]), m.apply(s.p[1]), tolerance);
                last = s.p[1];
                break;
            case SegKind::Close:
                if (have) {
                    cur.closed = true;
                    flush();
                }
                last = start;
                have = false;
                cur.pts.clear();
                break;
        }
    }

    flush();
    return out;
}

bool path_bbox(const Path& path, double& x0, double& y0, double& x1, double& y1) {
    x0 = y0 = 1e300;
    x1 = y1 = -1e300;
    Vec2 last{}, start{};
    bool any = false;

    auto add = [&](Vec2 p) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
        any = true;
    };

    for (const Segment& s : path.segs) {
        switch (s.kind) {
            case SegKind::Move: last = start = s.p[0]; add(last); break;
            case SegKind::Line: add(s.p[0]); last = s.p[0]; break;
            case SegKind::Cubic:
                add(s.p[2]);
                extend_cubic(last.x, s.p[0].x, s.p[1].x, s.p[2].x, x0, x1);
                extend_cubic(last.y, s.p[0].y, s.p[1].y, s.p[2].y, y0, y1);
                last = s.p[2];
                break;
            case SegKind::Quad:
                add(s.p[1]);
                extend_quad(last.x, s.p[0].x, s.p[1].x, x0, x1);
                extend_quad(last.y, s.p[0].y, s.p[1].y, y0, y1);
                last = s.p[1];
                break;
            case SegKind::Close: last = start; break;
        }
    }

    return any;
}

bool parse_path_data(std::string_view d, Path& out) {
    NumberParser p(d);
    Vec2 cur{}, start{}, last_ctrl{};
    char cmd = 0, prev_cmd = 0;
    bool first = true;

    while (!p.at_end()) {
        const char c = p.peek();
        if (std::isalpha(static_cast<unsigned char>(c))) {
            cmd = c;
            p.advance();
        } else if (cmd == 0) {
            return !out.empty();
        } else if (cmd == 'M') {
            cmd = 'L';
        } else if (cmd == 'm') {
            cmd = 'l';
        } else if (cmd == 'Z' || cmd == 'z') {
            return !out.empty();
        }

        if (first && cmd != 'M' && cmd != 'm') return false;
        first = false;
        const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
        const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
        double v[7];

        auto read = [&](int n) {
            for (int i = 0; i < n; ++i)
                if (!p.number(v[i])) return false;
            return true;
        };

        switch (up) {
            case 'M':
                if (!read(2)) return !out.empty();
                cur = rel ? Vec2{ cur.x + v[0], cur.y + v[1] } : Vec2{ v[0], v[1] };
                start = cur;
                out.move_to(cur);
                break;
            case 'L':
                if (!read(2)) return !out.empty();
                cur = rel ? Vec2{ cur.x + v[0], cur.y + v[1] } : Vec2{ v[0], v[1] };
                out.line_to(cur);
                break;
            case 'H':
                if (!read(1)) return !out.empty();
                cur.x = rel ? cur.x + v[0] : v[0];
                out.line_to(cur);
                break;
            case 'V':
                if (!read(1)) return !out.empty();
                cur.y = rel ? cur.y + v[0] : v[0];
                out.line_to(cur);
                break;
            case 'C': {
                if (!read(6)) return !out.empty();
                const Vec2 o = rel ? cur : Vec2{};
                const Vec2 a{ o.x + v[0], o.y + v[1] }, b{ o.x + v[2], o.y + v[3] }, e{ o.x + v[4], o.y + v[5] };
                out.cubic_to(a, b, e);
                last_ctrl = b;
                cur = e;
                break;
            }
            case 'S': {
                if (!read(4)) return !out.empty();
                const Vec2 o = rel ? cur : Vec2{};
                const char pu = static_cast<char>(std::toupper(static_cast<unsigned char>(prev_cmd)));
                const Vec2 a = (pu == 'C' || pu == 'S') ? Vec2{ 2 * cur.x - last_ctrl.x, 2 * cur.y - last_ctrl.y } : cur;
                const Vec2 b{ o.x + v[0], o.y + v[1] }, e{ o.x + v[2], o.y + v[3] };
                out.cubic_to(a, b, e);
                last_ctrl = b;
                cur = e;
                break;
            }
            case 'Q': {
                if (!read(4)) return !out.empty();
                const Vec2 o = rel ? cur : Vec2{};
                const Vec2 a{ o.x + v[0], o.y + v[1] }, e{ o.x + v[2], o.y + v[3] };
                out.quad_to(a, e);
                last_ctrl = a;
                cur = e;
                break;
            }
            case 'T': {
                if (!read(2)) return !out.empty();
                const Vec2 o = rel ? cur : Vec2{};
                const char pu = static_cast<char>(std::toupper(static_cast<unsigned char>(prev_cmd)));
                const Vec2 a = (pu == 'Q' || pu == 'T') ? Vec2{ 2 * cur.x - last_ctrl.x, 2 * cur.y - last_ctrl.y } : cur;
                const Vec2 e{ o.x + v[0], o.y + v[1] };
                out.quad_to(a, e);
                last_ctrl = a;
                cur = e;
                break;
            }
            case 'A': {
                bool large = false, sweep = false;
                if (!p.number(v[0]) || !p.number(v[1]) || !p.number(v[2]) || !p.flag(large) || !p.flag(sweep) || !p.number(v[5]) || !p.number(v[6])) return !out.empty();
                const Vec2 e = rel ? Vec2{ cur.x + v[5], cur.y + v[6] } : Vec2{ v[5], v[6] };
                out.arc_to(cur, v[0], v[1], v[2], large, sweep, e);
                cur = e;
                break;
            }
            case 'Z':
                out.close();
                cur = start;
                break;
            default:
                return !out.empty();
        }

        prev_cmd = cmd;
    }

    return true;
}

std::vector<Polyline> apply_dashes(const std::vector<Polyline>& lines, const std::vector<double>& dashes_in, double offset) {
    std::vector<double> dashes = dashes_in;
    double total = 0;
    for (double d : dashes) {
        if (d < 0 || !std::isfinite(d)) return lines;
        total += d;
    }
    if (dashes.empty() || total <= 0) return lines;
    if (dashes.size() % 2) {
        const std::size_t n = dashes.size();
        for (std::size_t i = 0; i < n; ++i) dashes.push_back(dashes[i]);
        total *= 2;
    }

    std::vector<Polyline> out;

    for (const Polyline& pl : lines) {
        std::vector<Vec2> pts = pl.pts;
        if (pl.closed && !pts.empty()) pts.push_back(pts.front());
        if (pts.size() < 2) continue;

        double phase = std::fmod(offset, total);
        if (phase < 0) phase += total;
        std::size_t di = 0;
        while (phase >= dashes[di]) {
            phase -= dashes[di];
            di = (di + 1) % dashes.size();
        }
        double remaining = dashes[di] - phase;
        bool on = di % 2 == 0;
        Polyline cur;
        std::vector<Polyline> pieces;
        if (on) cur.pts.push_back(pts[0]);
        const bool started_on = on;

        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            Vec2 a = pts[i];
            const Vec2 b = pts[i + 1];
            double seg = length(b - a);

            while (seg > 0) {
                if (remaining >= seg) {
                    remaining -= seg;
                    if (on) cur.pts.push_back(b);
                    seg = 0;
                } else {
                    const Vec2 m = a + (b - a) * (remaining / seg);
                    if (on) {
                        cur.pts.push_back(m);
                        pieces.push_back(std::move(cur));
                        cur = Polyline();
                    } else {
                        cur.pts.push_back(m);
                    }
                    seg -= remaining;
                    a = m;
                    on = !on;
                    di = (di + 1) % dashes.size();
                    remaining = dashes[di];
                }
            }
        }

        if (on && cur.pts.size() >= 1) pieces.push_back(std::move(cur));

        if (pl.closed && started_on && on && pieces.size() >= 2) {
            Polyline merged = std::move(pieces.back());
            pieces.pop_back();
            merged.pts.insert(merged.pts.end(), pieces.front().pts.begin() + 1, pieces.front().pts.end());
            pieces.front() = std::move(merged);
        }

        for (auto& p : pieces) out.push_back(std::move(p));
    }

    return out;
}

std::vector<Polyline> stroke_to_polygons(const std::vector<Polyline>& lines, const StrokeStyle& style, double tolerance) {
    std::vector<Polyline> out;
    const double hw = style.width / 2;
    if (!(hw > 0)) return out;

    for (const Polyline& pl : lines) {
        std::vector<Vec2> pts;
        for (const Vec2& p : pl.pts)
            if (pts.empty() || length(p - pts.back()) > 1e-9 * std::max(1.0, hw)) pts.push_back(p);
        bool closed = pl.closed;
        if (closed && pts.size() > 1 && length(pts.front() - pts.back()) <= 1e-9 * std::max(1.0, hw)) pts.pop_back();

        if (pts.size() == 1) {
            if (style.cap == LineCap::Round) add_circle(out, pts[0], hw, tolerance);
            else if (style.cap == LineCap::Square) push_oriented(out, { { pts[0].x - hw, pts[0].y - hw }, { pts[0].x + hw, pts[0].y - hw }, { pts[0].x + hw, pts[0].y + hw }, { pts[0].x - hw, pts[0].y + hw } });
            continue;
        }
        if (pts.size() < 2) continue;
        if (closed && pts.size() < 3) closed = false;

        const std::size_t n = pts.size();
        const std::size_t segs = closed ? n : n - 1;
        std::vector<Vec2> dirs(segs), norms(segs);

        for (std::size_t i = 0; i < segs; ++i) {
            const Vec2 d = pts[(i + 1) % n] - pts[i];
            const double l = length(d);
            dirs[i] = d * (1.0 / l);
            norms[i] = { -dirs[i].y * hw, dirs[i].x * hw };
        }

        for (std::size_t i = 0; i < segs; ++i) {
            const Vec2 a = pts[i], b = pts[(i + 1) % n];
            push_oriented(out, { a + norms[i], b + norms[i], b - norms[i], a - norms[i] });
        }

        auto join = [&](std::size_t vi, std::size_t s0, std::size_t s1) {
            const Vec2 p = pts[vi];
            const double turn = cross(dirs[s0], dirs[s1]);
            const double cosang = dot(dirs[s0], dirs[s1]);
            if (std::fabs(turn) < 1e-12 && cosang > 0) return;
            const double side = turn > 0 ? -1.0 : 1.0;
            const Vec2 o0 = p + norms[s0] * side, o1 = p + norms[s1] * side;

            switch (style.join) {
                case LineJoin::Round:
                case LineJoin::Arcs: {
                    std::vector<Vec2> w = { p };
                    double a0 = std::atan2(o0.y - p.y, o0.x - p.x);
                    double a1 = std::atan2(o1.y - p.y, o1.x - p.x);
                    double sweep = a1 - a0;
                    while (sweep > kPi) sweep -= 2 * kPi;
                    while (sweep < -kPi) sweep += 2 * kPi;
                    add_arc_points(w, p, hw, a0, sweep, tolerance);
                    push_oriented(out, std::move(w));
                    break;
                }
                case LineJoin::Bevel:
                    push_oriented(out, { p, o0, o1 });
                    break;
                case LineJoin::Miter:
                case LineJoin::MiterClip: {
                    const double theta = std::acos(std::max(-1.0, std::min(1.0, -cosang)));
                    const double ratio = 1.0 / std::sin(theta / 2);
                    if (std::isfinite(ratio) && ratio <= style.miter_limit) {
                        const Vec2 bis = (norms[s0] + norms[s1]) * side;
                        const double bl = length(bis);
                        if (bl > 1e-12) {
                            const Vec2 tip = p + bis * (hw * ratio / bl);
                            push_oriented(out, { p, o0, tip, o1 });
                            break;
                        }
                    }
                    if (style.join == LineJoin::MiterClip && std::isfinite(ratio)) {
                        const Vec2 bis = (norms[s0] + norms[s1]) * side;
                        const double bl = length(bis);
                        if (bl > 1e-12) {
                            const Vec2 bd = bis * (1.0 / bl);
                            const double clip = hw * style.miter_limit / 2;
                            const Vec2 c = p + bd * clip;
                            const Vec2 perp{ -bd.y, bd.x };
                            auto inter = [&](Vec2 o, Vec2 d) {
                                const double den = cross(d, perp);
                                if (std::fabs(den) < 1e-12) return o;
                                const double t = cross(c - o, perp) / den;
                                return o + d * t;
                            };
                            const Vec2 e0 = inter(o0, dirs[s0]), e1 = inter(o1, dirs[s1] * -1.0);
                            push_oriented(out, { p, o0, e0, e1, o1 });
                            break;
                        }
                    }
                    push_oriented(out, { p, o0, o1 });
                    break;
                }
            }
        };

        if (closed) {
            for (std::size_t i = 0; i < n; ++i) join(i, (i + segs - 1) % segs, i);
        } else {
            for (std::size_t i = 1; i + 1 < n; ++i) join(i, i - 1, i);
        }

        if (!closed) {
            auto cap = [&](Vec2 p, Vec2 dir, Vec2 nrm) {
                if (style.cap == LineCap::Square) {
                    const Vec2 e = dir * hw;
                    push_oriented(out, { p + nrm, p + nrm + e, p - nrm + e, p - nrm });
                } else if (style.cap == LineCap::Round) {
                    std::vector<Vec2> w;
                    const double a0 = std::atan2(nrm.y, nrm.x);
                    const double turn = cross(nrm, dir) > 0 ? kPi : -kPi;
                    add_arc_points(w, p, hw, a0, turn, tolerance);
                    push_oriented(out, std::move(w));
                }
            };
            cap(pts[n - 1], dirs[segs - 1], norms[segs - 1]);
            cap(pts[0], dirs[0] * -1.0, norms[0] * -1.0);
        }
    }

    return out;
}

void Coverage::add_polyline(const Polyline& pl) {
    if (pl.pts.size() < 2) return;
    for (std::size_t i = 0; i + 1 < pl.pts.size(); ++i) add_line(pl.pts[i], pl.pts[i + 1]);
    add_line(pl.pts.back(), pl.pts.front());
}

void Coverage::add_line(Vec2 a, Vec2 b) {
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) || !std::isfinite(b.y)) return;
    if (a.y == b.y) return;
    const double W = m_w;
    double ts[4];
    int nt = 0;
    ts[nt++] = 0.0;

    if (a.x != b.x) {
        const double t0 = (0.0 - a.x) / (b.x - a.x);
        const double t1 = (W - a.x) / (b.x - a.x);
        if (t0 > 0 && t0 < 1) ts[nt++] = t0;
        if (t1 > 0 && t1 < 1) ts[nt++] = t1;
        if (nt == 3 && ts[1] > ts[2]) std::swap(ts[1], ts[2]);
    }

    ts[nt++] = 1.0;
    Vec2 prev = a;

    for (int k = 1; k < nt; ++k) {
        const Vec2 next = k + 1 == nt ? b : Vec2{ a.x + (b.x - a.x) * ts[k], a.y + (b.y - a.y) * ts[k] };
        const double mid = a.x + (b.x - a.x) * (ts[k - 1] + ts[k]) / 2;
        Vec2 p0 = prev, p1 = next;
        if (mid <= 0) p0.x = p1.x = 0;
        else if (mid >= W) p0.x = p1.x = W;
        else {
            p0.x = std::min(W, std::max(0.0, p0.x));
            p1.x = std::min(W, std::max(0.0, p1.x));
        }
        if (p0.y != p1.y) push_edge(p0, p1);
        prev = next;
    }
}

void Coverage::push_edge(Vec2 a, Vec2 b) {
    Edge e;
    if (a.y < b.y) e = Edge{ a.x, a.y, b.x, b.y, 1 };
    else e = Edge{ b.x, b.y, a.x, a.y, -1 };
    if (e.y1 <= 0 || e.y0 >= m_h) return;
    m_edges.push_back(e);
    m_minx = std::min(m_minx, std::min(e.x0, e.x1));
    m_maxx = std::max(m_maxx, std::max(e.x0, e.x1));
    m_miny = std::min(m_miny, e.y0);
    m_maxy = std::max(m_maxy, e.y1);
}

void Coverage::accumulate(std::vector<double>& row, double xa, double xb, double d) {
    xa = std::min(static_cast<double>(m_w), std::max(0.0, xa));
    xb = std::min(static_cast<double>(m_w), std::max(0.0, xb));
    const double x0 = std::min(xa, xb), x1 = std::max(xa, xb);
    const double x0floor = std::floor(x0);
    const int x0i = static_cast<int>(x0floor);
    const double x1ceil = std::ceil(x1);
    const int x1i = static_cast<int>(x1ceil);

    if (x1i <= x0i + 1) {
        const double xmf = 0.5 * (xa + xb) - x0floor;
        row[static_cast<std::size_t>(x0i)] += d - d * xmf;
        row[static_cast<std::size_t>(x0i) + 1] += d * xmf;
        return;
    }

    const double s = 1.0 / (x1 - x0);
    const double x0f = x0 - x0floor;
    const double a0 = 0.5 * s * (1.0 - x0f) * (1.0 - x0f);
    const double x1f = x1 - x1ceil + 1.0;
    const double am = 0.5 * s * x1f * x1f;
    row[static_cast<std::size_t>(x0i)] += d * a0;
    if (x1i == x0i + 2) {
        row[static_cast<std::size_t>(x0i) + 1] += d * (1.0 - a0 - am);
    } else {
        const double a1 = s * (1.5 - x0f);
        row[static_cast<std::size_t>(x0i) + 1] += d * (a1 - a0);
        for (int xi = x0i + 2; xi < x1i - 1; ++xi) row[static_cast<std::size_t>(xi)] += d * s;
        const double a2 = a1 + (x1i - x0i - 3) * s;
        row[static_cast<std::size_t>(x1i) - 1] += d * (1.0 - a2 - am);
    }
    row[static_cast<std::size_t>(x1i)] += d * am;
}

void Coverage::band(std::vector<double>& row, int ry, double ys, double ye, const std::vector<const Edge*>& active, bool even_odd, int depth) {
    (void)ry;
    if (ye - ys <= 0) return;
    struct Item {
        const Edge* e;
        double      xs, xe, xm;
    };
    std::vector<Item> items;
    items.reserve(active.size());
    const double ym = (ys + ye) / 2;
    for (const Edge* e : active)
        if (e->y0 <= ys && e->y1 >= ye) items.push_back({ e, e->x_at(ys), e->x_at(ye), e->x_at(ym) });
    if (items.empty()) return;
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.xm < b.xm; });

    if (depth < 64) {
        double split = ye;
        const double eps = 1e-12 * std::max(1.0, std::fabs(ye));
        for (std::size_t i = 0; i + 1 < items.size(); ++i) {
            const double ds = items[i].xs - items[i + 1].xs, de = items[i].xe - items[i + 1].xe;
            if ((ds > 0 && de < 0) || (ds < 0 && de > 0)) {
                const double t = ds / (ds - de);
                const double y = ys + (ye - ys) * t;
                if (y > ys + eps && y < ye - eps) split = std::min(split, y);
            }
        }
        if (split < ye) {
            band(row, ry, ys, split, active, even_odd, depth + 1);
            band(row, ry, split, ye, active, even_odd, depth + 1);
            return;
        }
    }

    const double h = ye - ys;
    int w = 0;
    for (const Item& it : items) {
        const bool before = even_odd ? (w & 1) != 0 : w != 0;
        w += it.e->dir;
        const bool after = even_odd ? (w & 1) != 0 : w != 0;
        if (!before && after) accumulate(row, it.xs, it.xe, h);
        else if (before && !after) accumulate(row, it.xs, it.xe, -h);
    }
}

void Coverage::resolve(bool even_odd, std::vector<float>& out, int& x0, int& y0, int& x1, int& y1) {
    out.clear();
    if (m_edges.empty()) {
        x0 = y0 = x1 = y1 = 0;
        return;
    }

    x0 = std::max(0, static_cast<int>(std::floor(m_minx)));
    x1 = std::min(m_w, static_cast<int>(std::ceil(m_maxx)) + 1);
    y0 = std::max(0, static_cast<int>(std::floor(m_miny)));
    y1 = std::min(m_h, static_cast<int>(std::ceil(m_maxy)));
    if (x1 <= x0 || y1 <= y0) {
        x0 = y0 = x1 = y1 = 0;
        m_edges.clear();
        return;
    }

    const int bw = x1 - x0;
    out.assign(static_cast<std::size_t>(bw) * static_cast<std::size_t>(y1 - y0), 0.0f);
    std::sort(m_edges.begin(), m_edges.end(), [](const Edge& a, const Edge& b) { return a.y0 < b.y0; });
    std::vector<const Edge*> active;
    std::vector<double> events;
    std::vector<double> row(static_cast<std::size_t>(m_w) + 2, 0.0);
    std::size_t next = 0;

    for (int y = y0; y < y1; ++y) {
        const double top = y, bottom = y + 1.0;
        active.erase(std::remove_if(active.begin(), active.end(), [&](const Edge* e) { return e->y1 <= top; }), active.end());
        while (next < m_edges.size() && m_edges[next].y0 < bottom) {
            if (m_edges[next].y1 > top) active.push_back(&m_edges[next]);
            ++next;
        }
        if (active.empty()) continue;

        events.clear();
        events.push_back(top);
        events.push_back(bottom);
        for (const Edge* e : active) {
            if (e->y0 > top && e->y0 < bottom) events.push_back(e->y0);
            if (e->y1 > top && e->y1 < bottom) events.push_back(e->y1);
        }
        std::sort(events.begin(), events.end());
        events.erase(std::unique(events.begin(), events.end()), events.end());

        for (std::size_t k = 0; k + 1 < events.size(); ++k) band(row, y, events[k], events[k + 1], active, even_odd, 0);

        double acc = 0;
        for (int x = 0; x < x0; ++x) {
            acc += row[static_cast<std::size_t>(x)];
            row[static_cast<std::size_t>(x)] = 0;
        }
        float* dst = out.data() + static_cast<std::size_t>(y - y0) * static_cast<std::size_t>(bw);
        for (int x = x0; x < x1; ++x) {
            acc += row[static_cast<std::size_t>(x)];
            row[static_cast<std::size_t>(x)] = 0;
            double c = acc;
            if (c < 1e-9) c = 0;
            if (c > 1) c = 1;
            dst[x - x0] = static_cast<float>(c);
        }
        for (std::size_t x = static_cast<std::size_t>(x1); x < row.size(); ++x) row[x] = 0;
    }

    m_edges.clear();
    m_minx = m_miny = 1e300;
    m_maxx = m_maxy = -1e300;
}

} // namespace svg_detail
} // namespace images
} // namespace fizmo
