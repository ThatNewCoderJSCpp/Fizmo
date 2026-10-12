#ifndef FIZMO_IMAGES_SVG_INTERNAL_HPP
#define FIZMO_IMAGES_SVG_INTERNAL_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace images {
namespace svg_detail {

struct XmlNode {
    std::string                                      tag;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<int>                                 children;
    int                                              parent = -1;
    std::string                                      text;
    bool                                             is_text = false;

    const std::string* attr(std::string_view name) const {
        for (const auto& a : attrs)
            if (a.first == name) return &a.second;
        return nullptr;
    }
};

struct XmlDocument {
    std::vector<XmlNode> nodes;
    int                  root = -1;
};

bool parse_xml(std::string_view src, XmlDocument& doc, std::string* error);

struct CssDeclaration {
    std::string property;
    std::string value;
    bool        important = false;
};

struct CssSimpleSelector {
    std::string              tag;
    std::string              id;
    std::vector<std::string> classes;
    std::vector<std::pair<std::string, std::string>> attrs;
    bool                     universal = false;
};

struct CssSelector {
    std::vector<CssSimpleSelector> parts;
    std::vector<char>              combinators;
    int                            specificity = 0;
};

struct CssRule {
    CssSelector                 selector;
    std::vector<CssDeclaration> declarations;
    std::size_t                 order = 0;
};

void parse_css(std::string_view css, std::vector<CssRule>& rules);
std::vector<CssDeclaration> parse_declarations(std::string_view text);
bool selector_matches(const XmlDocument& doc, int node, const CssSelector& sel);

struct Vec2 {
    double x = 0, y = 0;
};

struct Matrix {
    double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;

    Vec2 apply(Vec2 p) const { return { a * p.x + c * p.y + e, b * p.x + d * p.y + f }; }
    Vec2 apply_vector(Vec2 p) const { return { a * p.x + c * p.y, b * p.x + d * p.y }; }

    Matrix operator*(const Matrix& m) const {
        return { a * m.a + c * m.b, b * m.a + d * m.b, a * m.c + c * m.d, b * m.c + d * m.d, a * m.e + c * m.f + e, b * m.e + d * m.f + f };
    }

    double determinant() const { return a * d - b * c; }

    bool invert(Matrix& out) const {
        const double det = determinant();
        if (det == 0 || !std::isfinite(det)) return false;
        out = { d / det, -b / det, -c / det, a / det, (c * f - d * e) / det, (b * e - a * f) / det };
        return true;
    }

    double scale_factor() const { return std::sqrt(std::fabs(determinant())); }

    static Matrix translate(double x, double y) { return { 1, 0, 0, 1, x, y }; }
    static Matrix scale(double x, double y) { return { x, 0, 0, y, 0, 0 }; }
    static Matrix rotate(double deg) {
        const double r = deg * 3.14159265358979323846 / 180.0;
        return { std::cos(r), std::sin(r), -std::sin(r), std::cos(r), 0, 0 };
    }
};

enum class SegKind : std::uint8_t { Move, Line, Cubic, Quad, Close };

struct Segment {
    SegKind kind;
    Vec2    p[3];
};

struct Path {
    std::vector<Segment> segs;

    void move_to(Vec2 p) { segs.push_back({ SegKind::Move, { p, {}, {} } }); }
    void line_to(Vec2 p) { segs.push_back({ SegKind::Line, { p, {}, {} } }); }
    void cubic_to(Vec2 a, Vec2 b, Vec2 c) { segs.push_back({ SegKind::Cubic, { a, b, c } }); }
    void quad_to(Vec2 a, Vec2 b) { segs.push_back({ SegKind::Quad, { a, b, {} } }); }
    void close() { segs.push_back({ SegKind::Close, { {}, {}, {} } }); }
    void arc_to(Vec2 from, double rx, double ry, double angle, bool large, bool sweep, Vec2 to);
    bool empty() const { return segs.empty(); }
};

struct Polyline {
    std::vector<Vec2> pts;
    bool              closed = false;
};

std::vector<Polyline> flatten(const Path& path, const Matrix& m, double tolerance);
bool path_bbox(const Path& path, double& x0, double& y0, double& x1, double& y1);
bool parse_path_data(std::string_view d, Path& out);

enum class LineCap : std::uint8_t { Butt, Round, Square };
enum class LineJoin : std::uint8_t { Miter, MiterClip, Round, Bevel, Arcs };

struct StrokeStyle {
    double              width = 1;
    LineCap             cap = LineCap::Butt;
    LineJoin            join = LineJoin::Miter;
    double              miter_limit = 4;
    std::vector<double> dashes;
    double              dash_offset = 0;
};

std::vector<Polyline> stroke_to_polygons(const std::vector<Polyline>& lines, const StrokeStyle& style, double tolerance);
std::vector<Polyline> apply_dashes(const std::vector<Polyline>& lines, const std::vector<double>& dashes, double offset);

class Coverage {
public:
    Coverage(int w, int h) : m_w(w), m_h(h) {}

    void add_polyline(const Polyline& pl);
    void add_line(Vec2 a, Vec2 b);
    void resolve(bool even_odd, std::vector<float>& out, int& x0, int& y0, int& x1, int& y1);
    bool empty() const { return m_edges.empty(); }

private:
    struct Edge {
        double x0, y0, x1, y1;
        int    dir;

        double x_at(double y) const { return y1 == y0 ? x0 : x0 + (x1 - x0) * (y - y0) / (y1 - y0); }
    };

    void push_edge(Vec2 a, Vec2 b);
    void band(std::vector<double>& row, int ry, double ys, double ye, const std::vector<const Edge*>& active, bool even_odd, int depth);
    void accumulate(std::vector<double>& row, double xa, double xb, double d);

    int               m_w, m_h;
    std::vector<Edge> m_edges;
    double            m_minx = 1e300, m_maxx = -1e300, m_miny = 1e300, m_maxy = -1e300;
};

} // namespace svg_detail
} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGES_SVG_INTERNAL_HPP
