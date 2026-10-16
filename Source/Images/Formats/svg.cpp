#include "fizmo_library.hpp"
#include "format_util.hpp"
#include "svg_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>

namespace fizmo {
namespace images {
namespace svg_detail {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Rgba32 {
    float r = 0, g = 0, b = 0, a = 0;
};

struct NamedColor {
    const char* name;
    std::uint32_t rgb;
};

const NamedColor kNamedColors[] = {
    { "aliceblue", 0xF0F8FF }, { "antiquewhite", 0xFAEBD7 }, { "aqua", 0x00FFFF }, { "aquamarine", 0x7FFFD4 }, { "azure", 0xF0FFFF },
    { "beige", 0xF5F5DC }, { "bisque", 0xFFE4C4 }, { "black", 0x000000 }, { "blanchedalmond", 0xFFEBCD }, { "blue", 0x0000FF },
    { "blueviolet", 0x8A2BE2 }, { "brown", 0xA52A2A }, { "burlywood", 0xDEB887 }, { "cadetblue", 0x5F9EA0 }, { "chartreuse", 0x7FFF00 },
    { "chocolate", 0xD2691E }, { "coral", 0xFF7F50 }, { "cornflowerblue", 0x6495ED }, { "cornsilk", 0xFFF8DC }, { "crimson", 0xDC143C },
    { "cyan", 0x00FFFF }, { "darkblue", 0x00008B }, { "darkcyan", 0x008B8B }, { "darkgoldenrod", 0xB8860B }, { "darkgray", 0xA9A9A9 },
    { "darkgreen", 0x006400 }, { "darkgrey", 0xA9A9A9 }, { "darkkhaki", 0xBDB76B }, { "darkmagenta", 0x8B008B }, { "darkolivegreen", 0x556B2F },
    { "darkorange", 0xFF8C00 }, { "darkorchid", 0x9932CC }, { "darkred", 0x8B0000 }, { "darksalmon", 0xE9967A }, { "darkseagreen", 0x8FBC8F },
    { "darkslateblue", 0x483D8B }, { "darkslategray", 0x2F4F4F }, { "darkslategrey", 0x2F4F4F }, { "darkturquoise", 0x00CED1 }, { "darkviolet", 0x9400D3 },
    { "deeppink", 0xFF1493 }, { "deepskyblue", 0x00BFFF }, { "dimgray", 0x696969 }, { "dimgrey", 0x696969 }, { "dodgerblue", 0x1E90FF },
    { "firebrick", 0xB22222 }, { "floralwhite", 0xFFFAF0 }, { "forestgreen", 0x228B22 }, { "fuchsia", 0xFF00FF }, { "gainsboro", 0xDCDCDC },
    { "ghostwhite", 0xF8F8FF }, { "gold", 0xFFD700 }, { "goldenrod", 0xDAA520 }, { "gray", 0x808080 }, { "grey", 0x808080 },
    { "green", 0x008000 }, { "greenyellow", 0xADFF2F }, { "honeydew", 0xF0FFF0 }, { "hotpink", 0xFF69B4 }, { "indianred", 0xCD5C5C },
    { "indigo", 0x4B0082 }, { "ivory", 0xFFFFF0 }, { "khaki", 0xF0E68C }, { "lavender", 0xE6E6FA }, { "lavenderblush", 0xFFF0F5 },
    { "lawngreen", 0x7CFC00 }, { "lemonchiffon", 0xFFFACD }, { "lightblue", 0xADD8E6 }, { "lightcoral", 0xF08080 }, { "lightcyan", 0xE0FFFF },
    { "lightgoldenrodyellow", 0xFAFAD2 }, { "lightgray", 0xD3D3D3 }, { "lightgreen", 0x90EE90 }, { "lightgrey", 0xD3D3D3 }, { "lightpink", 0xFFB6C1 },
    { "lightsalmon", 0xFFA07A }, { "lightseagreen", 0x20B2AA }, { "lightskyblue", 0x87CEFA }, { "lightslategray", 0x778899 }, { "lightslategrey", 0x778899 },
    { "lightsteelblue", 0xB0C4DE }, { "lightyellow", 0xFFFFE0 }, { "lime", 0x00FF00 }, { "limegreen", 0x32CD32 }, { "linen", 0xFAF0E6 },
    { "magenta", 0xFF00FF }, { "maroon", 0x800000 }, { "mediumaquamarine", 0x66CDAA }, { "mediumblue", 0x0000CD }, { "mediumorchid", 0xBA55D3 },
    { "mediumpurple", 0x9370DB }, { "mediumseagreen", 0x3CB371 }, { "mediumslateblue", 0x7B68EE }, { "mediumspringgreen", 0x00FA9A }, { "mediumturquoise", 0x48D1CC },
    { "mediumvioletred", 0xC71585 }, { "midnightblue", 0x191970 }, { "mintcream", 0xF5FFFA }, { "mistyrose", 0xFFE4E1 }, { "moccasin", 0xFFE4B5 },
    { "navajowhite", 0xFFDEAD }, { "navy", 0x000080 }, { "oldlace", 0xFDF5E6 }, { "olive", 0x808000 }, { "olivedrab", 0x6B8E23 },
    { "orange", 0xFFA500 }, { "orangered", 0xFF4500 }, { "orchid", 0xDA70D6 }, { "palegoldenrod", 0xEEE8AA }, { "palegreen", 0x98FB98 },
    { "paleturquoise", 0xAFEEEE }, { "palevioletred", 0xDB7093 }, { "papayawhip", 0xFFEFD5 }, { "peachpuff", 0xFFDAB9 }, { "peru", 0xCD853F },
    { "pink", 0xFFC0CB }, { "plum", 0xDDA0DD }, { "powderblue", 0xB0E0E6 }, { "purple", 0x800080 }, { "rebeccapurple", 0x663399 },
    { "red", 0xFF0000 }, { "rosybrown", 0xBC8F8F }, { "royalblue", 0x4169E1 }, { "saddlebrown", 0x8B4513 }, { "salmon", 0xFA8072 },
    { "sandybrown", 0xF4A460 }, { "seagreen", 0x2E8B57 }, { "seashell", 0xFFF5EE }, { "sienna", 0xA0522D }, { "silver", 0xC0C0C0 },
    { "skyblue", 0x87CEEB }, { "slateblue", 0x6A5ACD }, { "slategray", 0x708090 }, { "slategrey", 0x708090 }, { "snow", 0xFFFAFA },
    { "springgreen", 0x00FF7F }, { "steelblue", 0x4682B4 }, { "tan", 0xD2B48C }, { "teal", 0x008080 }, { "thistle", 0xD8BFD8 },
    { "tomato", 0xFF6347 }, { "turquoise", 0x40E0D0 }, { "violet", 0xEE82EE }, { "wheat", 0xF5DEB3 }, { "white", 0xFFFFFF },
    { "whitesmoke", 0xF5F5F5 }, { "yellow", 0xFFFF00 }, { "yellowgreen", 0x9ACD32 },
};

std::string lower(std::string_view s) {
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

std::string trim(std::string_view s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

bool parse_number_prefix(std::string_view s, double& v, std::size_t& used) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    const std::size_t st = i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    bool digits = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; digits = true; }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; digits = true; }
    }
    if (!digits) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        std::size_t k = i + 1;
        if (k < s.size() && (s[k] == '+' || s[k] == '-')) ++k;
        if (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) {
            i = k;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
    }
    v = std::strtod(std::string(s.substr(st, i - st)).c_str(), nullptr);
    used = i;
    return std::isfinite(v);
}

std::vector<double> parse_number_list(std::string_view s) {
    std::vector<double> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',')) ++i;
        if (i >= s.size()) break;
        double v;
        std::size_t used;
        if (!parse_number_prefix(s.substr(i), v, used)) break;
        out.push_back(v);
        i += used;
    }
    return out;
}

bool parse_color(std::string_view text, Rgba32& out) {
    const std::string s = lower(trim(text));
    if (s.empty()) return false;

    if (s[0] == '#') {
        const std::string h = s.substr(1);
        auto hex = [](char c) -> int { return std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1; };
        for (char c : h) if (hex(c) < 0) return false;
        if (h.size() == 3 || h.size() == 4) {
            out.r = static_cast<float>(hex(h[0]) * 17 / 255.0);
            out.g = static_cast<float>(hex(h[1]) * 17 / 255.0);
            out.b = static_cast<float>(hex(h[2]) * 17 / 255.0);
            out.a = h.size() == 4 ? static_cast<float>(hex(h[3]) * 17 / 255.0) : 1.0f;
            return true;
        }
        if (h.size() == 6 || h.size() == 8) {
            out.r = static_cast<float>((hex(h[0]) * 16 + hex(h[1])) / 255.0);
            out.g = static_cast<float>((hex(h[2]) * 16 + hex(h[3])) / 255.0);
            out.b = static_cast<float>((hex(h[4]) * 16 + hex(h[5])) / 255.0);
            out.a = h.size() == 8 ? static_cast<float>((hex(h[6]) * 16 + hex(h[7])) / 255.0) : 1.0f;
            return true;
        }
        return false;
    }

    const std::size_t paren = s.find('(');
    if (paren != std::string::npos && s.back() == ')') {
        const std::string fn = trim(std::string_view(s).substr(0, paren));
        std::string body = s.substr(paren + 1, s.size() - paren - 2);
        for (char& c : body) if (c == ',' || c == '/') c = ' ';
        std::vector<std::string> parts;
        std::size_t i = 0;
        while (i < body.size()) {
            while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) ++i;
            const std::size_t st = i;
            while (i < body.size() && !std::isspace(static_cast<unsigned char>(body[i]))) ++i;
            if (i > st) parts.push_back(body.substr(st, i - st));
        }
        if (parts.size() < 3) return false;

        auto comp = [](const std::string& p, double scale, double& v) {
            std::size_t used;
            if (!parse_number_prefix(p, v, used)) return false;
            const std::string unit = p.substr(used);
            if (unit == "%") v = v / 100.0 * scale;
            else if (unit == "deg" || unit.empty()) {}
            else if (unit == "rad") v = v * 180.0 / kPi;
            else if (unit == "turn") v *= 360.0;
            else return false;
            return true;
        };

        double alpha = 1;
        if (parts.size() >= 4) {
            std::size_t used;
            if (!parse_number_prefix(parts[3], alpha, used)) return false;
            if (parts[3].substr(used) == "%") alpha /= 100.0;
        }
        alpha = std::max(0.0, std::min(1.0, alpha));

        if (fn == "rgb" || fn == "rgba") {
            double c[3];
            for (int k = 0; k < 3; ++k) if (!comp(parts[static_cast<std::size_t>(k)], 255.0, c[k])) return false;
            out.r = static_cast<float>(std::max(0.0, std::min(255.0, c[0])) / 255.0);
            out.g = static_cast<float>(std::max(0.0, std::min(255.0, c[1])) / 255.0);
            out.b = static_cast<float>(std::max(0.0, std::min(255.0, c[2])) / 255.0);
            out.a = static_cast<float>(alpha);
            return true;
        }

        if (fn == "hsl" || fn == "hsla") {
            double h, sat, l;
            if (!comp(parts[0], 360.0, h) || !comp(parts[1], 1.0, sat) || !comp(parts[2], 1.0, l)) return false;
            if (parts[1].back() != '%') sat /= 100.0;
            if (parts[2].back() != '%') l /= 100.0;
            h = std::fmod(h, 360.0);
            if (h < 0) h += 360.0;
            sat = std::max(0.0, std::min(1.0, sat));
            l = std::max(0.0, std::min(1.0, l));
            auto f = [&](double n) {
                const double k = std::fmod(n + h / 30.0, 12.0);
                const double a = sat * std::min(l, 1 - l);
                return l - a * std::max(-1.0, std::min({ k - 3, 9 - k, 1.0 }));
            };
            out.r = static_cast<float>(f(0));
            out.g = static_cast<float>(f(8));
            out.b = static_cast<float>(f(4));
            out.a = static_cast<float>(alpha);
            return true;
        }

        return false;
    }

    if (s == "transparent") {
        out = Rgba32{ 0, 0, 0, 0 };
        return true;
    }

    for (const auto& nc : kNamedColors) {
        if (s == nc.name) {
            out.r = static_cast<float>(((nc.rgb >> 16) & 255) / 255.0);
            out.g = static_cast<float>(((nc.rgb >> 8) & 255) / 255.0);
            out.b = static_cast<float>((nc.rgb & 255) / 255.0);
            out.a = 1;
            return true;
        }
    }

    return false;
}

bool parse_transform(std::string_view s, Matrix& out) {
    out = Matrix();
    std::size_t i = 0;

    while (i < s.size()) {
        while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',')) ++i;
        if (i >= s.size()) break;
        const std::size_t st = i;
        while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) ++i;
        const std::string name(s.substr(st, i - st));
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= s.size() || s[i] != '(') return false;
        const std::size_t close = s.find(')', i);
        if (close == std::string_view::npos) return false;
        const std::vector<double> a = parse_number_list(s.substr(i + 1, close - i - 1));
        i = close + 1;
        Matrix m;

        if (name == "matrix" && a.size() == 6) m = { a[0], a[1], a[2], a[3], a[4], a[5] };
        else if (name == "translate" && !a.empty()) m = Matrix::translate(a[0], a.size() > 1 ? a[1] : 0);
        else if (name == "scale" && !a.empty()) m = Matrix::scale(a[0], a.size() > 1 ? a[1] : a[0]);
        else if (name == "rotate" && !a.empty()) {
            m = Matrix::rotate(a[0]);
            if (a.size() >= 3) m = Matrix::translate(a[1], a[2]) * m * Matrix::translate(-a[1], -a[2]);
        } else if (name == "skewX" && a.size() == 1) m = { 1, 0, std::tan(a[0] * kPi / 180), 1, 0, 0 };
        else if (name == "skewY" && a.size() == 1) m = { 1, std::tan(a[0] * kPi / 180), 0, 1, 0, 0 };
        else return false;

        out = out * m;
    }

    return true;
}

struct PaintSpec {
    enum Kind { None, Solid, Url, CurrentColor } kind = None;
    Rgba32      color;
    std::string url;
    bool        has_fallback = false;
    PaintSpec*  fallback_ptr = nullptr;
    Rgba32      fallback;
    bool        fallback_none = false;
    bool        fallback_current = false;
};

bool parse_paint(std::string_view text, PaintSpec& p) {
    const std::string s = trim(text);
    const std::string l = lower(s);
    p = PaintSpec();
    if (l == "none") return true;
    if (l == "currentcolor") {
        p.kind = PaintSpec::CurrentColor;
        return true;
    }
    if (l.rfind("url(", 0) == 0) {
        const std::size_t close = s.find(')');
        if (close == std::string::npos) return false;
        std::string ref = trim(std::string_view(s).substr(4, close - 4));
        if (ref.size() >= 2 && (ref[0] == '"' || ref[0] == '\'')) ref = ref.substr(1, ref.size() - 2);
        if (!ref.empty() && ref[0] == '#') ref = ref.substr(1);
        p.kind = PaintSpec::Url;
        p.url = ref;
        const std::string rest = lower(trim(std::string_view(s).substr(close + 1)));
        if (!rest.empty()) {
            p.has_fallback = true;
            if (rest == "none") p.fallback_none = true;
            else if (rest == "currentcolor") p.fallback_current = true;
            else if (!parse_color(rest, p.fallback)) p.has_fallback = false;
        }
        return true;
    }
    if (parse_color(s, p.color)) {
        p.kind = PaintSpec::Solid;
        return true;
    }
    return false;
}

struct Stop {
    double offset;
    Rgba32 color;
};

struct Gradient {
    bool                linear = true;
    bool                bbox_units = true;
    Matrix              transform;
    int                 spread = 0;
    double              x1 = 0, y1 = 0, x2 = 1, y2 = 0;
    double              cx = 0.5, cy = 0.5, r = 0.5, fx = 0.5, fy = 0.5, fr = 0;
    std::vector<Stop>   stops;
};

struct State {
    PaintSpec   fill;
    PaintSpec   stroke;
    double      fill_opacity = 1;
    double      stroke_opacity = 1;
    bool        fill_evenodd = false;
    bool        clip_evenodd = false;
    StrokeStyle stroke_style;
    std::string stroke_width_raw = "1";
    std::string dash_raw;
    std::string dash_offset_raw;
    Rgba32      color{ 0, 0, 0, 1 };
    bool        visible = true;
    double      font_size = 16;
    bool        non_scaling_stroke = false;
    std::string font_family = "serif";
    int         font_weight = 400;
    bool        italic = false;
    int         text_anchor = 0;
    bool        preserve_space = false;
};

struct Viewport {
    double w = 100, h = 100;
};

struct Layer {
    int                w = 0, h = 0;
    std::vector<float> px;

    Layer() = default;
    Layer(int ww, int hh) : w(ww), h(hh), px(static_cast<std::size_t>(ww) * static_cast<std::size_t>(hh) * 4, 0.0f) {}
};

class Renderer {
public:
    Renderer(const XmlDocument& doc, int w, int h) : m_doc(doc), m_w(w), m_h(h) {
        index_ids();
        compute_styles();
    }

    void render_root(const Matrix& base, Layer& target) {
        State st;
        st.fill.kind = PaintSpec::Solid;
        st.fill.color = Rgba32{ 0, 0, 0, 1 };
        Viewport vp{ m_root_vw, m_root_vh };
        render_children_of_svg(m_doc.root, base, st, vp, target, 0);
    }

    double m_root_vw = 100, m_root_vh = 100;

    const std::string* prop(int node, const char* name) const {
        const auto& m = m_props[static_cast<std::size_t>(node)];
        auto it = m.find(name);
        return it == m.end() ? nullptr : &it->second;
    }

    const XmlDocument& m_doc;

private:
    void index_ids() {
        for (std::size_t i = 0; i < m_doc.nodes.size(); ++i) {
            if (m_doc.nodes[i].is_text) continue;
            const std::string* id = m_doc.nodes[i].attr("id");
            if (id && !m_ids.count(*id)) m_ids[*id] = static_cast<int>(i);
        }
    }

    static bool is_presentation(const std::string& name) {
        static const char* const names[] = { "fill", "fill-opacity", "fill-rule", "stroke", "stroke-width", "stroke-opacity", "stroke-linecap", "stroke-linejoin",
                                             "stroke-miterlimit", "stroke-dasharray", "stroke-dashoffset", "opacity", "color", "display", "visibility", "clip-path",
                                             "clip-rule", "mask", "stop-color", "stop-opacity", "font-size", "transform", "vector-effect", "overflow", "filter",
                                             "mask-type", "marker-start", "marker-mid", "marker-end", "marker", "paint-order", "flood-color", "flood-opacity",
                                             "color-interpolation-filters", "image-rendering", "shape-rendering", "font-family", "font-weight",
                                             "font-style", "text-anchor", "xml:space", "letter-spacing" };
        for (const char* n : names) if (name == n) return true;
        return false;
    }

    void compute_styles() {
        std::vector<CssRule> rules;
        for (std::size_t i = 0; i < m_doc.nodes.size(); ++i) {
            const XmlNode& n = m_doc.nodes[i];
            if (n.is_text || n.tag != "style") continue;
            const std::string* type = n.attr("type");
            if (type && !type->empty() && lower(*type) != "text/css") continue;
            std::string css;
            for (int c : n.children) css += m_doc.nodes[static_cast<std::size_t>(c)].text;
            parse_css(css, rules);
        }

        m_props.assign(m_doc.nodes.size(), {});

        for (std::size_t i = 0; i < m_doc.nodes.size(); ++i) {
            const XmlNode& n = m_doc.nodes[i];
            if (n.is_text) continue;
            auto& props = m_props[i];
            for (const auto& a : n.attrs) if (is_presentation(a.first)) props[a.first] = a.second;

            std::vector<const CssRule*> matched;
            for (const auto& r : rules) if (selector_matches(m_doc, static_cast<int>(i), r.selector)) matched.push_back(&r);
            std::stable_sort(matched.begin(), matched.end(), [](const CssRule* a, const CssRule* b) {
                if (a->selector.specificity != b->selector.specificity) return a->selector.specificity < b->selector.specificity;
                return a->order < b->order;
            });

            std::unordered_map<std::string, bool> important;
            for (const CssRule* r : matched)
                for (const auto& d : r->declarations) {
                    if (d.important) important[d.property] = true;
                    else if (important.count(d.property)) continue;
                    props[d.property] = d.value;
                }

            if (const std::string* style = n.attr("style"))
                for (const auto& d : parse_declarations(*style)) {
                    if (important.count(d.property) && !d.important) continue;
                    props[d.property] = d.value;
                }
        }
    }

    double length(const std::string* s, double def, double reference, double font_size) const {
        if (!s) return def;
        double v;
        std::size_t used;
        if (!parse_number_prefix(*s, v, used)) return def;
        const std::string unit = lower(trim(std::string_view(*s).substr(used)));
        if (unit.empty() || unit == "px") return v;
        if (unit == "%") return v / 100.0 * reference;
        if (unit == "pt") return v * 96.0 / 72.0;
        if (unit == "pc") return v * 16.0;
        if (unit == "mm") return v * 96.0 / 25.4;
        if (unit == "cm") return v * 96.0 / 2.54;
        if (unit == "in") return v * 96.0;
        if (unit == "em") return v * font_size;
        if (unit == "ex") return v * font_size / 2;
        return def;
    }

    double attr_len(int node, const char* name, double def, double ref, const State& st) const {
        return length(m_doc.nodes[static_cast<std::size_t>(node)].attr(name), def, ref, st.font_size);
    }

    static double parse_opacity(const std::string& s, double def) {
        double v;
        std::size_t used;
        if (!parse_number_prefix(s, v, used)) return def;
        if (trim(std::string_view(s).substr(used)) == "%") v /= 100.0;
        return std::max(0.0, std::min(1.0, v));
    }

    void apply_style(int node, State& st, const Viewport& vp) const {
        const auto& p = m_props[static_cast<std::size_t>(node)];
        auto get = [&](const char* n) -> const std::string* {
            auto it = p.find(n);
            if (it == p.end() || lower(trim(it->second)) == "inherit") return nullptr;
            return &it->second;
        };

        if (const auto* v = get("font-size")) st.font_size = std::max(0.0, length(v, st.font_size, st.font_size, st.font_size));
        if (const auto* v = get("color")) {
            Rgba32 c;
            if (parse_color(*v, c)) st.color = c;
        }
        if (const auto* v = get("fill")) {
            PaintSpec ps;
            if (parse_paint(*v, ps)) st.fill = ps;
        }
        if (const auto* v = get("stroke")) {
            PaintSpec ps;
            if (parse_paint(*v, ps)) st.stroke = ps;
        }
        if (const auto* v = get("fill-opacity")) st.fill_opacity = parse_opacity(*v, st.fill_opacity);
        if (const auto* v = get("stroke-opacity")) st.stroke_opacity = parse_opacity(*v, st.stroke_opacity);
        if (const auto* v = get("fill-rule")) st.fill_evenodd = trim(*v) == "evenodd";
        if (const auto* v = get("clip-rule")) st.clip_evenodd = trim(*v) == "evenodd";
        if (const auto* v = get("stroke-width")) {
            const double w = length(v, -1, std::sqrt((vp.w * vp.w + vp.h * vp.h) / 2), st.font_size);
            if (w >= 0) st.stroke_style.width = w;
        }
        if (const auto* v = get("stroke-linecap")) {
            const std::string c = trim(*v);
            st.stroke_style.cap = c == "round" ? LineCap::Round : c == "square" ? LineCap::Square : LineCap::Butt;
        }
        if (const auto* v = get("stroke-linejoin")) {
            const std::string c = trim(*v);
            st.stroke_style.join = c == "round" ? LineJoin::Round : c == "bevel" ? LineJoin::Bevel : c == "miter-clip" ? LineJoin::MiterClip : c == "arcs" ? LineJoin::Arcs : LineJoin::Miter;
        }
        if (const auto* v = get("stroke-miterlimit")) {
            double m;
            std::size_t used;
            if (parse_number_prefix(*v, m, used) && m >= 1) st.stroke_style.miter_limit = m;
        }
        if (const auto* v = get("stroke-dasharray")) {
            st.stroke_style.dashes.clear();
            const std::string t = lower(trim(*v));
            if (t != "none") {
                std::string norm = t;
                for (char& c : norm) if (c == ',') c = ' ';
                std::size_t i = 0;
                bool bad = false;
                while (i < norm.size()) {
                    while (i < norm.size() && std::isspace(static_cast<unsigned char>(norm[i]))) ++i;
                    const std::size_t s0 = i;
                    while (i < norm.size() && !std::isspace(static_cast<unsigned char>(norm[i]))) ++i;
                    if (i > s0) {
                        const std::string tok = norm.substr(s0, i - s0);
                        const double d = length(&tok, -1, std::sqrt((vp.w * vp.w + vp.h * vp.h) / 2), st.font_size);
                        if (d < 0) bad = true;
                        st.stroke_style.dashes.push_back(d);
                    }
                }
                if (bad) st.stroke_style.dashes.clear();
            }
        }
        if (const auto* v = get("stroke-dashoffset")) st.stroke_style.dash_offset = length(v, 0, std::sqrt((vp.w * vp.w + vp.h * vp.h) / 2), st.font_size);
        if (const auto* v = get("visibility")) {
            const std::string t = trim(*v);
            st.visible = !(t == "hidden" || t == "collapse");
        }
        if (const auto* v = get("vector-effect")) st.non_scaling_stroke = trim(*v) == "non-scaling-stroke";
        if (const auto* v = get("font-family")) {
            std::string f = trim(*v);
            const std::size_t comma = f.find(',');
            if (comma != std::string::npos) f = trim(std::string_view(f).substr(0, comma));
            if (f.size() >= 2 && (f[0] == '"' || f[0] == '\'')) f = f.substr(1, f.size() - 2);
            st.font_family = f;
        }
        if (const auto* v = get("font-weight")) {
            const std::string t = lower(trim(*v));
            if (t == "bold" || t == "bolder") st.font_weight = 700;
            else if (t == "normal") st.font_weight = 400;
            else if (t == "lighter") st.font_weight = 300;
            else {
                double w;
                std::size_t used;
                if (parse_number_prefix(t, w, used)) st.font_weight = static_cast<int>(w);
            }
        }
        if (const auto* v = get("font-style")) {
            const std::string t = lower(trim(*v));
            st.italic = t == "italic" || t == "oblique";
        }
        if (const auto* v = get("text-anchor")) {
            const std::string t = trim(*v);
            st.text_anchor = t == "middle" ? 1 : t == "end" ? 2 : 0;
        }
        if (const auto* v = get("xml:space")) st.preserve_space = trim(*v) == "preserve";
    }

    bool displayed(int node) const {
        const std::string* d = prop(node, "display");
        return !(d && trim(*d) == "none");
    }

    Matrix node_transform(int node) const {
        Matrix m;
        if (const std::string* t = prop(node, "transform")) parse_transform(*t, m);
        return m;
    }

    int resolve_href(int node) const {
        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        const std::string* h = n.attr("href");
        if (!h) h = n.attr("xlink:href");
        if (!h || h->empty() || (*h)[0] != '#') return -1;
        auto it = m_ids.find(h->substr(1));
        return it == m_ids.end() ? -1 : it->second;
    }

    int resolve_url(const std::string* v) const {
        if (!v) return -1;
        const std::string s = trim(*v);
        if (s.rfind("url(", 0) != 0) return -1;
        const std::size_t close = s.find(')');
        if (close == std::string::npos) return -1;
        std::string ref = trim(std::string_view(s).substr(4, close - 4));
        if (ref.size() >= 2 && (ref[0] == '"' || ref[0] == '\'')) ref = ref.substr(1, ref.size() - 2);
        if (!ref.empty() && ref[0] == '#') ref = ref.substr(1);
        auto it = m_ids.find(ref);
        return it == m_ids.end() ? -1 : it->second;
    }

    Matrix viewbox_transform(const std::string* viewbox, const std::string* par, double x, double y, double w, double h) const {
        if (!viewbox) return Matrix::translate(x, y);
        const std::vector<double> vb = parse_number_list(*viewbox);
        if (vb.size() != 4 || vb[2] <= 0 || vb[3] <= 0) return Matrix::translate(x, y);
        std::string align = "xmidymid";
        bool slice = false;
        if (par) {
            std::string t = lower(trim(*par));
            if (t.rfind("defer", 0) == 0) t = trim(std::string_view(t).substr(5));
            const std::size_t sp = t.find(' ');
            align = t.substr(0, sp);
            if (sp != std::string::npos) slice = trim(std::string_view(t).substr(sp)) == "slice";
        }
        double sx = w / vb[2], sy = h / vb[3];
        if (align == "none") return Matrix::translate(x, y) * Matrix::scale(sx, sy) * Matrix::translate(-vb[0], -vb[1]);
        const double s = slice ? std::max(sx, sy) : std::min(sx, sy);
        double tx = x - vb[0] * s, ty = y - vb[1] * s;
        const double ew = vb[2] * s, eh = vb[3] * s;
        if (align.find("xmid") != std::string::npos) tx += (w - ew) / 2;
        else if (align.find("xmax") != std::string::npos) tx += w - ew;
        if (align.find("ymid") != std::string::npos) ty += (h - eh) / 2;
        else if (align.find("ymax") != std::string::npos) ty += h - eh;
        return Matrix::translate(tx, ty) * Matrix::scale(s, s);
    }

    void render_children_of_svg(int node, const Matrix& ctm, const State& parent, const Viewport& vp, Layer& target, int depth) {
        State st = parent;
        apply_style(node, st, vp);
        for (int c : m_doc.nodes[static_cast<std::size_t>(node)].children) render_node(c, ctm, st, vp, target, depth + 1);
    }

    bool build_shape(int node, const State& st, const Viewport& vp, Path& path) const {
        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        const std::string& t = n.tag;
        const double diag = std::sqrt((vp.w * vp.w + vp.h * vp.h) / 2);

        if (t == "path") {
            const std::string* d = n.attr("d");
            if (!d) return false;
            return parse_path_data(*d, path) && !path.empty();
        }
        if (t == "rect") {
            const double x = attr_len(node, "x", 0, vp.w, st), y = attr_len(node, "y", 0, vp.h, st);
            const double w = attr_len(node, "width", 0, vp.w, st), h = attr_len(node, "height", 0, vp.h, st);
            if (!(w > 0) || !(h > 0)) return false;
            const std::string* rxs = n.attr("rx");
            const std::string* rys = n.attr("ry");
            double rx = length(rxs, -1, vp.w, st.font_size), ry = length(rys, -1, vp.h, st.font_size);
            if (rx < 0 && ry >= 0) rx = ry;
            if (ry < 0 && rx >= 0) ry = rx;
            rx = std::max(0.0, std::min(rx, w / 2));
            ry = std::max(0.0, std::min(ry, h / 2));
            if (rx <= 0 || ry <= 0) {
                path.move_to({ x, y });
                path.line_to({ x + w, y });
                path.line_to({ x + w, y + h });
                path.line_to({ x, y + h });
                path.close();
                return true;
            }
            path.move_to({ x + rx, y });
            path.line_to({ x + w - rx, y });
            path.arc_to({ x + w - rx, y }, rx, ry, 0, false, true, { x + w, y + ry });
            path.line_to({ x + w, y + h - ry });
            path.arc_to({ x + w, y + h - ry }, rx, ry, 0, false, true, { x + w - rx, y + h });
            path.line_to({ x + rx, y + h });
            path.arc_to({ x + rx, y + h }, rx, ry, 0, false, true, { x, y + h - ry });
            path.line_to({ x, y + ry });
            path.arc_to({ x, y + ry }, rx, ry, 0, false, true, { x + rx, y });
            path.close();
            return true;
        }
        if (t == "circle" || t == "ellipse") {
            const double cx = attr_len(node, "cx", 0, vp.w, st), cy = attr_len(node, "cy", 0, vp.h, st);
            double rx, ry;
            if (t == "circle") rx = ry = attr_len(node, "r", 0, diag, st);
            else {
                rx = attr_len(node, "rx", -1, vp.w, st);
                ry = attr_len(node, "ry", -1, vp.h, st);
                if (rx < 0) rx = ry;
                if (ry < 0) ry = rx;
            }
            if (!(rx > 0) || !(ry > 0)) return false;
            path.move_to({ cx + rx, cy });
            path.arc_to({ cx + rx, cy }, rx, ry, 0, false, true, { cx, cy + ry });
            path.arc_to({ cx, cy + ry }, rx, ry, 0, false, true, { cx - rx, cy });
            path.arc_to({ cx - rx, cy }, rx, ry, 0, false, true, { cx, cy - ry });
            path.arc_to({ cx, cy - ry }, rx, ry, 0, false, true, { cx + rx, cy });
            path.close();
            return true;
        }
        if (t == "line") {
            path.move_to({ attr_len(node, "x1", 0, vp.w, st), attr_len(node, "y1", 0, vp.h, st) });
            path.line_to({ attr_len(node, "x2", 0, vp.w, st), attr_len(node, "y2", 0, vp.h, st) });
            return true;
        }
        if (t == "polyline" || t == "polygon") {
            const std::string* pts = n.attr("points");
            if (!pts) return false;
            const std::vector<double> v = parse_number_list(*pts);
            if (v.size() < 4) return false;
            path.move_to({ v[0], v[1] });
            for (std::size_t i = 2; i + 1 < v.size(); i += 2) path.line_to({ v[i], v[i + 1] });
            if (t == "polygon") path.close();
            return true;
        }
        return false;
    }

    bool resolve_gradient(int node, Gradient& g) const {
        std::vector<int> chain;
        for (int cur = node; cur >= 0 && chain.size() < 16; cur = resolve_href(cur)) {
            if (std::find(chain.begin(), chain.end(), cur) != chain.end()) break;
            const std::string& tag = m_doc.nodes[static_cast<std::size_t>(cur)].tag;
            if (tag != "linearGradient" && tag != "radialGradient") break;
            chain.push_back(cur);
        }
        if (chain.empty()) return false;
        g.linear = m_doc.nodes[static_cast<std::size_t>(node)].tag == "linearGradient";

        auto find_attr = [&](const char* name) -> const std::string* {
            for (int c : chain) {
                const std::string* v = m_doc.nodes[static_cast<std::size_t>(c)].attr(name);
                if (v) return v;
            }
            return nullptr;
        };

        if (const auto* u = find_attr("gradientUnits")) g.bbox_units = trim(*u) != "userSpaceOnUse";
        if (const auto* sm = find_attr("spreadMethod")) g.spread = trim(*sm) == "reflect" ? 1 : trim(*sm) == "repeat" ? 2 : 0;
        for (int c : chain) {
            const std::string* t = m_doc.nodes[static_cast<std::size_t>(c)].attr("gradientTransform");
            if (t) { parse_transform(*t, g.transform); break; }
        }

        const double refw = g.bbox_units ? 1.0 : m_vp_stack_w, refh = g.bbox_units ? 1.0 : m_vp_stack_h;
        const double diag = g.bbox_units ? 1.0 : std::sqrt((refw * refw + refh * refh) / 2);
        auto num = [&](const char* name, double def, double ref) {
            const std::string* v = find_attr(name);
            if (!v) return def;
            double x;
            std::size_t used;
            if (!parse_number_prefix(*v, x, used)) return def;
            const std::string unit = trim(std::string_view(*v).substr(used));
            if (unit == "%") return x / 100.0 * ref;
            return length(v, def, ref, 16);
        };

        if (g.linear) {
            g.x1 = num("x1", 0, refw);
            g.y1 = num("y1", 0, refh);
            g.x2 = num("x2", g.bbox_units ? 1.0 : refw, refw);
            g.y2 = num("y2", 0, refh);
        } else {
            g.cx = num("cx", g.bbox_units ? 0.5 : refw * 0.5, refw);
            g.cy = num("cy", g.bbox_units ? 0.5 : refh * 0.5, refh);
            g.r = num("r", g.bbox_units ? 0.5 : diag * 0.5, diag);
            g.fx = find_attr("fx") ? num("fx", g.cx, refw) : g.cx;
            g.fy = find_attr("fy") ? num("fy", g.cy, refh) : g.cy;
            g.fr = num("fr", 0, diag);
        }

        for (int c : chain) {
            const XmlNode& gn = m_doc.nodes[static_cast<std::size_t>(c)];
            bool any = false;
            for (int k : gn.children) {
                const XmlNode& s = m_doc.nodes[static_cast<std::size_t>(k)];
                if (s.is_text || s.tag != "stop") continue;
                any = true;
                Stop stop;
                stop.offset = 0;
                if (const std::string* o = s.attr("offset")) {
                    double v;
                    std::size_t used;
                    if (parse_number_prefix(*o, v, used)) stop.offset = trim(std::string_view(*o).substr(used)) == "%" ? v / 100.0 : v;
                }
                stop.offset = std::max(0.0, std::min(1.0, stop.offset));
                if (!g.stops.empty()) stop.offset = std::max(stop.offset, g.stops.back().offset);
                Rgba32 col{ 0, 0, 0, 1 };
                double op = 1;
                if (const std::string* sc = prop(k, "stop-color")) {
                    const std::string t = lower(trim(*sc));
                    if (t == "currentcolor") {
                        const std::string* cc = prop(k, "color");
                        if (!cc || !parse_color(*cc, col)) col = Rgba32{ 0, 0, 0, 1 };
                    } else if (!parse_color(*sc, col)) {
                        col = Rgba32{ 0, 0, 0, 1 };
                    }
                }
                if (const std::string* so = prop(k, "stop-opacity")) op = parse_opacity(*so, 1);
                col.a = static_cast<float>(col.a * op);
                stop.color = col;
                g.stops.push_back(stop);
            }
            if (any) break;
        }

        return true;
    }

    static Rgba32 sample_stops(const std::vector<Stop>& stops, double t) {
        if (t <= stops.front().offset) return stops.front().color;
        if (t >= stops.back().offset) return stops.back().color;
        for (std::size_t i = 1; i < stops.size(); ++i) {
            if (t <= stops[i].offset) {
                const Stop& a = stops[i - 1];
                const Stop& b = stops[i];
                const double span = b.offset - a.offset;
                const double f = span > 0 ? (t - a.offset) / span : 1.0;
                Rgba32 c;
                c.r = static_cast<float>(a.color.r + (b.color.r - a.color.r) * f);
                c.g = static_cast<float>(a.color.g + (b.color.g - a.color.g) * f);
                c.b = static_cast<float>(a.color.b + (b.color.b - a.color.b) * f);
                c.a = static_cast<float>(a.color.a + (b.color.a - a.color.a) * f);
                return c;
            }
        }
        return stops.back().color;
    }

    static double spread_t(double t, int spread) {
        if (spread == 2) return t - std::floor(t);
        if (spread == 1) {
            double m = std::fmod(std::fabs(t), 2.0);
            return m > 1 ? 2 - m : m;
        }
        return t;
    }

    struct PaintSource {
        bool                   solid = true;
        Rgba32                 color;
        Gradient               grad;
        Matrix                 inv;
        double                 opacity = 1;

        Rgba32 at(double x, double y) const {
            if (solid) return color;
            const Vec2 p = inv.apply({ x, y });
            double t;
            if (grad.linear) {
                const double dx = grad.x2 - grad.x1, dy = grad.y2 - grad.y1;
                const double len2 = dx * dx + dy * dy;
                t = len2 > 0 ? ((p.x - grad.x1) * dx + (p.y - grad.y1) * dy) / len2 : 0;
            } else {
                const double cdx = grad.cx - grad.fx, cdy = grad.cy - grad.fy, dr = grad.r - grad.fr;
                const double pdx = p.x - grad.fx, pdy = p.y - grad.fy;
                const double a = cdx * cdx + cdy * cdy - dr * dr;
                const double b = pdx * cdx + pdy * cdy + grad.fr * dr;
                const double c = pdx * pdx + pdy * pdy - grad.fr * grad.fr;
                if (std::fabs(a) < 1e-12) {
                    t = b != 0 ? c / (2 * b) : 0;
                } else {
                    const double disc = b * b - a * c;
                    if (disc < 0) return Rgba32{};
                    const double sq = std::sqrt(disc);
                    const double t1 = (b + sq) / a, t2 = (b - sq) / a;
                    t = std::max(t1, t2);
                    if (grad.fr + t * dr < 0) t = std::min(t1, t2);
                    if (grad.fr + t * dr < 0) return Rgba32{};
                }
            }
            const Rgba32 c = sample_stops(grad.stops, spread_t(t, grad.spread));
            return c;
        }
    };

    bool make_paint(const PaintSpec& spec, const State& st, double opacity, const Matrix& ctm, const Path& path, PaintSource& out, int depth) const {
        out.opacity = opacity;
        switch (spec.kind) {
            case PaintSpec::None: return false;
            case PaintSpec::Solid: out.solid = true; out.color = spec.color; return true;
            case PaintSpec::CurrentColor: out.solid = true; out.color = st.color; return true;
            case PaintSpec::Url: {
                auto it = m_ids.find(spec.url);
                int node = it == m_ids.end() ? -1 : it->second;
                Gradient g;
                if (node >= 0 && depth < 8 && resolve_gradient(node, g)) {
                    if (g.stops.empty()) return false;
                    if (g.stops.size() == 1) {
                        out.solid = true;
                        out.color = g.stops[0].color;
                        return true;
                    }
                    Matrix space = ctm;
                    if (g.bbox_units) {
                        double x0, y0, x1, y1;
                        if (!path_bbox(path, x0, y0, x1, y1)) return false;
                        if (x1 - x0 <= 0 || y1 - y0 <= 0) return false;
                        space = space * Matrix::translate(x0, y0) * Matrix::scale(x1 - x0, y1 - y0);
                    }
                    space = space * g.transform;
                    if (!space.invert(out.inv)) return false;
                    out.solid = false;
                    out.grad = std::move(g);
                    return true;
                }
                if (spec.has_fallback) {
                    if (spec.fallback_none) return false;
                    out.solid = true;
                    out.color = spec.fallback_current ? st.color : spec.fallback;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    void composite_coverage(Layer& target, const std::vector<float>& cov, int x0, int y0, int x1, int y1, const PaintSource& paint) const {
        const int bw = x1 - x0;
        for (int y = y0; y < y1; ++y) {
            if (y < 0 || y >= target.h) continue;
            for (int x = x0; x < x1; ++x) {
                const float c = cov[static_cast<std::size_t>(y - y0) * static_cast<std::size_t>(bw) + static_cast<std::size_t>(x - x0)];
                if (c <= 0) continue;
                const Rgba32 s = paint.at(x + 0.5, y + 0.5);
                const float a = static_cast<float>(s.a * paint.opacity) * c;
                if (a <= 0) continue;
                float* d = &target.px[(static_cast<std::size_t>(y) * static_cast<std::size_t>(target.w) + static_cast<std::size_t>(x)) * 4];
                const float inv = 1.0f - a;
                d[0] = s.r * a + d[0] * inv;
                d[1] = s.g * a + d[1] * inv;
                d[2] = s.b * a + d[2] * inv;
                d[3] = a + d[3] * inv;
            }
        }
    }

    void fill_polylines(Layer& target, const std::vector<Polyline>& polys, bool evenodd, const PaintSource& paint) const {
        Coverage cov(target.w, target.h);
        for (const auto& p : polys) cov.add_polyline(p);
        std::vector<float> c;
        int x0, y0, x1, y1;
        cov.resolve(evenodd, c, x0, y0, x1, y1);
        if (c.empty()) return;
        composite_coverage(target, c, x0, y0, x1, y1, paint);
    }

    std::vector<Polyline> stroke_polys(const Path& path, const State& st, const Matrix& ctm) const {
        StrokeStyle ss = st.stroke_style;
        if (st.non_scaling_stroke) {
            std::vector<Polyline> lines = flatten(path, ctm, 0.02);
            if (!ss.dashes.empty()) lines = apply_dashes(lines, ss.dashes, ss.dash_offset);
            return stroke_to_polygons(lines, ss, 0.02);
        }
        const double scale = std::max(1e-9, std::sqrt(std::max(std::fabs(ctm.a * ctm.a + ctm.b * ctm.b), std::fabs(ctm.c * ctm.c + ctm.d * ctm.d))));
        const double tol = 0.02 / scale;
        std::vector<Polyline> lines = flatten(path, Matrix(), tol);
        if (!ss.dashes.empty()) lines = apply_dashes(lines, ss.dashes, ss.dash_offset);
        std::vector<Polyline> polys = stroke_to_polygons(lines, ss, tol);
        for (auto& p : polys)
            for (auto& v : p.pts) v = ctm.apply(v);
        return polys;
    }

    void paint_shape(int node, const Path& path, const State& st, const Matrix& ctm, Layer& target, double opacity, int depth) {
        PaintSource fill_paint, stroke_paint;
        const bool has_fill = make_paint(st.fill, st, st.fill_opacity * opacity, ctm, path, fill_paint, depth);
        const bool has_stroke = st.stroke_style.width > 0 && make_paint(st.stroke, st, st.stroke_opacity * opacity, ctm, path, stroke_paint, depth);
        const std::string* order = prop(node, "paint-order");
        bool stroke_first = false;
        if (order) {
            const std::string o = trim(*order);
            const std::size_t sp = o.find("stroke"), fp = o.find("fill");
            stroke_first = sp != std::string::npos && (fp == std::string::npos || sp < fp);
        }

        auto do_fill = [&]() {
            if (!has_fill) return;
            fill_polylines(target, flatten(path, ctm, 0.02), st.fill_evenodd, fill_paint);
        };
        auto do_stroke = [&]() {
            if (!has_stroke) return;
            fill_polylines(target, stroke_polys(path, st, ctm), false, stroke_paint);
        };

        if (stroke_first) { do_stroke(); do_fill(); }
        else { do_fill(); do_stroke(); }
        render_markers(node, path, st, ctm, target, depth);
    }

    void render_markers(int node, const Path& path, const State& st, const Matrix& ctm, Layer& target, int depth) {
        const std::string& tag = m_doc.nodes[static_cast<std::size_t>(node)].tag;
        if (tag != "path" && tag != "line" && tag != "polyline" && tag != "polygon") return;
        const std::string* all = prop(node, "marker");
        const std::string* ms = prop(node, "marker-start");
        const std::string* mm = prop(node, "marker-mid");
        const std::string* me = prop(node, "marker-end");
        if (!ms) ms = all;
        if (!mm) mm = all;
        if (!me) me = all;
        const int start = resolve_url(ms), mid = resolve_url(mm), end = resolve_url(me);
        if (start < 0 && mid < 0 && end < 0) return;

        struct Vertex {
            Vec2   p;
            double in_angle, out_angle;
            bool   has_in, has_out;
        };
        std::vector<Vertex> verts;
        Vec2 cur{}, sub_start{};
        std::size_t sub_index = 0;

        auto angle_of = [](Vec2 a, Vec2 b) { return std::atan2(b.y - a.y, b.x - a.x); };

        for (const Segment& s : path.segs) {
            switch (s.kind) {
                case SegKind::Move:
                    cur = sub_start = s.p[0];
                    sub_index = verts.size();
                    verts.push_back({ cur, 0, 0, false, false });
                    break;
                case SegKind::Line:
                case SegKind::Cubic:
                case SegKind::Quad: {
                    const Vec2 endp = s.kind == SegKind::Line ? s.p[0] : s.kind == SegKind::Quad ? s.p[1] : s.p[2];
                    const Vec2 c0 = s.kind == SegKind::Line ? endp : s.p[0];
                    const Vec2 c1 = s.kind == SegKind::Line ? cur : s.kind == SegKind::Quad ? s.p[0] : s.p[1];
                    const Vec2 out_dir_from = (c0.x == cur.x && c0.y == cur.y) ? (s.kind == SegKind::Cubic ? s.p[1] : endp) : c0;
                    const Vec2 in_dir_to = (c1.x == endp.x && c1.y == endp.y) ? (s.kind == SegKind::Cubic ? s.p[0] : cur) : c1;
                    if (!verts.empty()) {
                        verts.back().out_angle = angle_of(cur, out_dir_from);
                        verts.back().has_out = true;
                    }
                    verts.push_back({ endp, angle_of(in_dir_to, endp), 0, true, false });
                    cur = endp;
                    break;
                }
                case SegKind::Close:
                    if (!verts.empty() && sub_index < verts.size()) {
                        if (cur.x != sub_start.x || cur.y != sub_start.y) {
                            verts.back().out_angle = angle_of(cur, sub_start);
                            verts.back().has_out = true;
                            verts.push_back({ sub_start, angle_of(cur, sub_start), 0, true, false });
                        }
                        Vertex& first = verts[sub_index];
                        Vertex& last = verts.back();
                        if (first.has_out) {
                            last.out_angle = first.out_angle;
                            last.has_out = true;
                            first.in_angle = last.in_angle;
                            first.has_in = true;
                        }
                    }
                    cur = sub_start;
                    break;
            }
        }

        if (verts.empty()) return;

        auto bisect = [](const Vertex& v) {
            if (v.has_in && v.has_out) {
                double d = v.out_angle - v.in_angle;
                while (d > kPi) d -= 2 * kPi;
                while (d < -kPi) d += 2 * kPi;
                return v.in_angle + d / 2;
            }
            return v.has_in ? v.in_angle : v.out_angle;
        };

        for (std::size_t i = 0; i < verts.size(); ++i) {
            const int m = i == 0 ? start : i + 1 == verts.size() ? end : mid;
            if (m < 0) continue;
            double angle = bisect(verts[i]);
            if (i == 0 && verts[i].has_out && !verts[i].has_in) angle = verts[i].out_angle;
            draw_marker(m, verts[i].p, angle, i == 0, st, ctm, target, depth);
        }
    }

    void draw_marker(int marker, Vec2 at, double angle, bool is_start, const State& st, const Matrix& ctm, Layer& target, int depth) {
        const XmlNode& mk = m_doc.nodes[static_cast<std::size_t>(marker)];
        if (mk.tag != "marker" || depth > 16) return;
        Viewport vp{ m_vp_stack_w, m_vp_stack_h };
        const double mw = attr_len(marker, "markerWidth", 3, vp.w, st), mh = attr_len(marker, "markerHeight", 3, vp.h, st);
        if (mw <= 0 || mh <= 0) return;
        const std::string* orient = mk.attr("orient");
        double rot = 0;
        if (orient) {
            const std::string o = trim(*orient);
            if (o == "auto") rot = angle * 180 / kPi;
            else if (o == "auto-start-reverse") rot = angle * 180 / kPi + (is_start ? 180 : 0);
            else {
                double v;
                std::size_t used;
                if (parse_number_prefix(o, v, used)) rot = v;
            }
        }
        const std::string* units = mk.attr("markerUnits");
        const double sw = (!units || trim(*units) == "strokeWidth") ? st.stroke_style.width : 1.0;
        Matrix m = ctm * Matrix::translate(at.x, at.y) * Matrix::rotate(rot) * Matrix::scale(sw, sw);
        Matrix vbm = viewbox_transform(mk.attr("viewBox"), mk.attr("preserveAspectRatio"), 0, 0, mw, mh);
        const double rx = attr_len(marker, "refX", 0, vp.w, st), ry = attr_len(marker, "refY", 0, vp.h, st);
        const Vec2 ref = vbm.apply({ rx, ry });
        m = m * Matrix::translate(-ref.x, -ref.y) * vbm;
        State ms;
        ms.fill.kind = PaintSpec::Solid;
        ms.fill.color = Rgba32{ 0, 0, 0, 1 };
        ms.font_size = st.font_size;
        apply_style(marker, ms, vp);
        Viewport mvp{ mw, mh };
        if (const std::string* vb = mk.attr("viewBox")) {
            const std::vector<double> v = parse_number_list(*vb);
            if (v.size() == 4) mvp = Viewport{ v[2], v[3] };
        }
        for (int c : mk.children) render_node(c, m, ms, mvp, target, depth + 1);
    }

    std::vector<float> clip_mask(int clip_node, const Matrix& ctm, const Path* bbox_path, double bx0, double by0, double bx1, double by1, int depth) {
        std::vector<float> mask(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), 0.0f);
        const XmlNode& cn = m_doc.nodes[static_cast<std::size_t>(clip_node)];
        Matrix m = ctm * node_transform(clip_node);
        const std::string* units = cn.attr("clipPathUnits");
        if (units && trim(*units) == "objectBoundingBox") {
            if (!bbox_path || bx1 <= bx0 || by1 <= by0) return mask;
            m = m * Matrix::translate(bx0, by0) * Matrix::scale(bx1 - bx0, by1 - by0);
        }
        Viewport vp{ m_vp_stack_w, m_vp_stack_h };
        State base;
        apply_style(clip_node, base, vp);

        for (int c : cn.children) {
            const XmlNode& ch = m_doc.nodes[static_cast<std::size_t>(c)];
            if (ch.is_text || !displayed(c)) continue;
            int shape = c;
            Matrix sm = m * node_transform(c);
            if (ch.tag == "use") {
                const int ref = resolve_href(c);
                if (ref < 0) continue;
                State tmp = base;
                sm = sm * Matrix::translate(attr_len(c, "x", 0, vp.w, tmp), attr_len(c, "y", 0, vp.h, tmp)) * node_transform(ref);
                shape = ref;
            }
            State st = base;
            apply_style(c, st, vp);
            if (shape != c) apply_style(shape, st, vp);
            if (!st.visible) continue;
            Path path;
            if (!build_shape(shape, st, vp, path)) continue;
            Coverage cov(m_w, m_h);
            for (const auto& p : flatten(path, sm, 0.02)) cov.add_polyline(p);
            std::vector<float> cv;
            int x0, y0, x1, y1;
            cov.resolve(st.clip_evenodd, cv, x0, y0, x1, y1);
            std::vector<float> child_clip;
            const int cc = resolve_url(prop(c, "clip-path"));
            if (cc >= 0 && depth < 8) {
                double ex0, ey0, ex1, ey1;
                path_bbox(path, ex0, ey0, ex1, ey1);
                child_clip = clip_mask(cc, m * node_transform(c), &path, ex0, ey0, ex1, ey1, depth + 1);
            }
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    float a = cv[static_cast<std::size_t>(y - y0) * static_cast<std::size_t>(x1 - x0) + static_cast<std::size_t>(x - x0)];
                    const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
                    if (!child_clip.empty()) a *= child_clip[i];
                    mask[i] = a + mask[i] * (1 - a);
                }
        }

        const int nested = resolve_url(prop(clip_node, "clip-path"));
        if (nested >= 0 && nested != clip_node && depth < 8) {
            std::vector<float> outer = clip_mask(nested, ctm, bbox_path, bx0, by0, bx1, by1, depth + 1);
            for (std::size_t i = 0; i < mask.size(); ++i) mask[i] *= outer[i];
        }
        return mask;
    }

    std::vector<float> mask_values(int mask_node, const Matrix& ctm, double bx0, double by0, double bx1, double by1, const State& st, int depth) {
        std::vector<float> out(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), 0.0f);
        const XmlNode& mn = m_doc.nodes[static_cast<std::size_t>(mask_node)];
        const std::string* mu = mn.attr("maskUnits");
        const bool bbox_units = !(mu && trim(*mu) == "userSpaceOnUse");
        const std::string* cu = mn.attr("maskContentUnits");
        const bool content_bbox = cu && trim(*cu) == "objectBoundingBox";
        double rx, ry, rw, rh;
        Viewport vp{ m_vp_stack_w, m_vp_stack_h };

        auto frac = [&](const char* name, double def) {
            const std::string* v = mn.attr(name);
            if (!v) return def;
            double x;
            std::size_t used;
            if (!parse_number_prefix(*v, x, used)) return def;
            if (trim(std::string_view(*v).substr(used)) == "%") x /= 100.0;
            return x;
        };

        if (bbox_units) {
            if (bx1 <= bx0 || by1 <= by0) return out;
            rx = bx0 + frac("x", -0.1) * (bx1 - bx0);
            ry = by0 + frac("y", -0.1) * (by1 - by0);
            rw = frac("width", 1.2) * (bx1 - bx0);
            rh = frac("height", 1.2) * (by1 - by0);
        } else {
            rx = attr_len(mask_node, "x", -0.1 * vp.w, vp.w, st);
            ry = attr_len(mask_node, "y", -0.1 * vp.h, vp.h, st);
            rw = attr_len(mask_node, "width", 1.2 * vp.w, vp.w, st);
            rh = attr_len(mask_node, "height", 1.2 * vp.h, vp.h, st);
        }
        if (rw <= 0 || rh <= 0) return out;

        Layer content(m_w, m_h);
        Matrix m = ctm;
        if (content_bbox) {
            if (bx1 <= bx0 || by1 <= by0) return out;
            m = m * Matrix::translate(bx0, by0) * Matrix::scale(bx1 - bx0, by1 - by0);
        }
        State ms;
        ms.fill.kind = PaintSpec::Solid;
        ms.fill.color = Rgba32{ 0, 0, 0, 1 };
        apply_style(mask_node, ms, vp);
        for (int c : mn.children) render_node(c, m, ms, vp, content, depth + 1);

        Path region;
        region.move_to({ rx, ry });
        region.line_to({ rx + rw, ry });
        region.line_to({ rx + rw, ry + rh });
        region.line_to({ rx, ry + rh });
        region.close();
        Coverage cov(m_w, m_h);
        for (const auto& p : flatten(region, ctm, 0.02)) cov.add_polyline(p);
        std::vector<float> rc;
        int x0, y0, x1, y1;
        cov.resolve(false, rc, x0, y0, x1, y1);
        const std::string* mt = prop(mask_node, "mask-type");
        const bool alpha_mode = mt && trim(*mt) == "alpha";

        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
                const float* p = &content.px[i * 4];
                const float v = alpha_mode ? p[3] : 0.2125f * p[0] + 0.7154f * p[1] + 0.0721f * p[2];
                out[i] = v * rc[static_cast<std::size_t>(y - y0) * static_cast<std::size_t>(x1 - x0) + static_cast<std::size_t>(x - x0)];
            }

        const int nested = resolve_url(prop(mask_node, "mask"));
        if (nested >= 0 && nested != mask_node && depth < 8) {
            const std::vector<float> outer = mask_values(nested, ctm, bx0, by0, bx1, by1, st, depth + 1);
            for (std::size_t i = 0; i < out.size(); ++i) out[i] *= outer[i];
        }
        return out;
    }

    bool element_bbox(int node, const State& st, const Viewport& vp, const Matrix& local, double& x0, double& y0, double& x1, double& y1, int depth) const {
        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        Path path;
        if (build_shape(node, st, vp, path)) {
            std::vector<Polyline> pl = flatten(path, local, 1e-3);
            bool any = false;
            for (const auto& p : pl)
                for (const auto& v : p.pts) {
                    x0 = std::min(x0, v.x); y0 = std::min(y0, v.y); x1 = std::max(x1, v.x); y1 = std::max(y1, v.y);
                    any = true;
                }
            return any;
        }
        if (depth > 16) return false;
        bool any = false;
        if (n.tag == "g" || n.tag == "a" || n.tag == "switch" || n.tag == "svg") {
            for (int c : n.children) {
                if (m_doc.nodes[static_cast<std::size_t>(c)].is_text || !displayed(c)) continue;
                State cs = st;
                apply_style(c, cs, vp);
                any |= element_bbox(c, cs, vp, local * node_transform(c), x0, y0, x1, y1, depth + 1);
            }
        } else if (n.tag == "use") {
            const int ref = resolve_href(node);
            if (ref >= 0) {
                State cs = st;
                apply_style(ref, cs, vp);
                any = element_bbox(ref, cs, vp, local * Matrix::translate(attr_len(node, "x", 0, vp.w, st), attr_len(node, "y", 0, vp.h, st)) * node_transform(ref), x0, y0, x1, y1, depth + 1);
            }
        } else if (n.tag == "image") {
            const double ix = attr_len(node, "x", 0, vp.w, st), iy = attr_len(node, "y", 0, vp.h, st);
            const double iw = attr_len(node, "width", 0, vp.w, st), ih = attr_len(node, "height", 0, vp.h, st);
            for (Vec2 c : { Vec2{ ix, iy }, Vec2{ ix + iw, iy }, Vec2{ ix, iy + ih }, Vec2{ ix + iw, iy + ih } }) {
                const Vec2 v = local.apply(c);
                x0 = std::min(x0, v.x); y0 = std::min(y0, v.y); x1 = std::max(x1, v.x); y1 = std::max(y1, v.y);
                any = true;
            }
        }
        return any;
    }

    void blur_channel(std::vector<float>& buf, int w, int h, double sx, double sy) const {
        auto box_passes = [](double sigma, int sizes[3]) {
            const double wi = std::sqrt(12 * sigma * sigma / 3 + 1);
            int wl = static_cast<int>(std::floor(wi));
            if (wl % 2 == 0) --wl;
            const int wu = wl + 2;
            const double mi = (12 * sigma * sigma - 3 * wl * wl - 4 * 3 * wl - 3 * 3) / (-4 * wl - 4);
            const int m = static_cast<int>(std::round(mi));
            for (int i = 0; i < 3; ++i) sizes[i] = i < m ? wl : wu;
        };

        std::vector<float> tmp(buf.size());
        auto box = [&](std::vector<float>& src, std::vector<float>& dst, int r, bool horizontal) {
            const int len = horizontal ? w : h, lines = horizontal ? h : w;
            const double inv = 1.0 / (2 * r + 1);
            for (int l = 0; l < lines; ++l) {
                auto at = [&](int i) -> float& { return horizontal ? src[static_cast<std::size_t>(l) * static_cast<std::size_t>(w) + static_cast<std::size_t>(i)] : src[static_cast<std::size_t>(i) * static_cast<std::size_t>(w) + static_cast<std::size_t>(l)]; };
                auto out = [&](int i) -> float& { return horizontal ? dst[static_cast<std::size_t>(l) * static_cast<std::size_t>(w) + static_cast<std::size_t>(i)] : dst[static_cast<std::size_t>(i) * static_cast<std::size_t>(w) + static_cast<std::size_t>(l)]; };
                double acc = 0;
                for (int i = -r; i <= r; ++i) if (i >= 0 && i < len) acc += at(i);
                for (int i = 0; i < len; ++i) {
                    out(i) = static_cast<float>(acc * inv);
                    const int add = i + r + 1, sub = i - r;
                    if (add < len) acc += at(add);
                    if (sub >= 0) acc -= at(sub);
                }
            }
        };

        if (sx > 0) {
            int sizes[3];
            box_passes(sx, sizes);
            for (int k = 0; k < 3; ++k) {
                box(buf, tmp, (sizes[k] - 1) / 2, true);
                buf.swap(tmp);
            }
        }
        if (sy > 0) {
            int sizes[3];
            box_passes(sy, sizes);
            for (int k = 0; k < 3; ++k) {
                box(buf, tmp, (sizes[k] - 1) / 2, false);
                buf.swap(tmp);
            }
        }
    }

    bool apply_filter(int filter_node, Layer& layer, const Matrix& ctm, double bx0, double by0, double bx1, double by1, const State& st) {
        const XmlNode& fn = m_doc.nodes[static_cast<std::size_t>(filter_node)];
        if (fn.tag != "filter") return false;
        const std::string* pu = fn.attr("primitiveUnits");
        const bool prim_bbox = pu && trim(*pu) == "objectBoundingBox";
        const double sx = std::sqrt(ctm.a * ctm.a + ctm.b * ctm.b), sy = std::sqrt(ctm.c * ctm.c + ctm.d * ctm.d);
        const std::size_t npx = static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h);
        std::unordered_map<std::string, Layer> results;
        Layer last = layer;

        auto get_input = [&](const std::string* in) -> Layer {
            if (!in || in->empty()) return last;
            const std::string v = trim(*in);
            if (v == "SourceGraphic") return layer;
            if (v == "SourceAlpha") {
                Layer a(m_w, m_h);
                for (std::size_t i = 0; i < npx; ++i) a.px[i * 4 + 3] = layer.px[i * 4 + 3];
                return a;
            }
            auto it = results.find(v);
            return it == results.end() ? last : it->second;
        };

        auto num_attr = [&](int node, const char* name, double def) {
            const std::string* v = m_doc.nodes[static_cast<std::size_t>(node)].attr(name);
            if (!v) return def;
            const std::vector<double> n = parse_number_list(*v);
            return n.empty() ? def : n[0];
        };

        for (int c : fn.children) {
            const XmlNode& p = m_doc.nodes[static_cast<std::size_t>(c)];
            if (p.is_text) continue;
            Layer result;

            if (p.tag == "feGaussianBlur") {
                result = get_input(p.attr("in"));
                std::vector<double> dev = p.attr("stdDeviation") ? parse_number_list(*p.attr("stdDeviation")) : std::vector<double>{};
                double dx = dev.empty() ? 0 : dev[0], dy = dev.size() > 1 ? dev[1] : dx;
                if (prim_bbox) { dx *= bx1 - bx0; dy *= by1 - by0; }
                dx *= sx;
                dy *= sy;
                if (dx > 0 || dy > 0) {
                    std::vector<float> ch(npx);
                    for (int k = 0; k < 4; ++k) {
                        for (std::size_t i = 0; i < npx; ++i) ch[i] = result.px[i * 4 + static_cast<std::size_t>(k)];
                        blur_channel(ch, m_w, m_h, dx, dy);
                        for (std::size_t i = 0; i < npx; ++i) result.px[i * 4 + static_cast<std::size_t>(k)] = ch[i];
                    }
                }
            } else if (p.tag == "feOffset") {
                const Layer in = get_input(p.attr("in"));
                double dx = num_attr(c, "dx", 0), dy = num_attr(c, "dy", 0);
                if (prim_bbox) { dx *= bx1 - bx0; dy *= by1 - by0; }
                const Vec2 d = ctm.apply_vector({ dx, dy });
                const int ox = static_cast<int>(std::lround(d.x)), oy = static_cast<int>(std::lround(d.y));
                result = Layer(m_w, m_h);
                for (int y = 0; y < m_h; ++y)
                    for (int x = 0; x < m_w; ++x) {
                        const int sxp = x - ox, syp = y - oy;
                        if (sxp < 0 || syp < 0 || sxp >= m_w || syp >= m_h) continue;
                        std::memcpy(&result.px[(static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x)) * 4], &in.px[(static_cast<std::size_t>(syp) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(sxp)) * 4], 16);
                    }
            } else if (p.tag == "feFlood") {
                result = Layer(m_w, m_h);
                Rgba32 col{ 0, 0, 0, 1 };
                if (const std::string* fc = prop(c, "flood-color")) {
                    if (lower(trim(*fc)) == "currentcolor") col = st.color;
                    else parse_color(*fc, col);
                }
                double op = 1;
                if (const std::string* fo = prop(c, "flood-opacity")) op = parse_opacity(*fo, 1);
                const float a = static_cast<float>(col.a * op);
                for (std::size_t i = 0; i < npx; ++i) {
                    result.px[i * 4] = col.r * a;
                    result.px[i * 4 + 1] = col.g * a;
                    result.px[i * 4 + 2] = col.b * a;
                    result.px[i * 4 + 3] = a;
                }
            } else if (p.tag == "feComposite" || p.tag == "feBlend") {
                const Layer a = get_input(p.attr("in")), b = get_input(p.attr("in2"));
                result = Layer(m_w, m_h);
                const std::string op = p.attr("operator") ? trim(*p.attr("operator")) : "over";
                const std::string mode = p.attr("mode") ? trim(*p.attr("mode")) : "normal";
                const double k1 = num_attr(c, "k1", 0), k2 = num_attr(c, "k2", 0), k3 = num_attr(c, "k3", 0), k4 = num_attr(c, "k4", 0);
                for (std::size_t i = 0; i < npx; ++i) {
                    const float* s = &a.px[i * 4];
                    const float* d = &b.px[i * 4];
                    float* o = &result.px[i * 4];
                    if (p.tag == "feBlend") {
                        for (int k = 0; k < 3; ++k) {
                            const float cs = s[k], cd = d[k];
                            float v;
                            if (mode == "multiply") v = cs * (1 - d[3]) + cd * (1 - s[3]) + cs * cd;
                            else if (mode == "screen") v = cs + cd - cs * cd;
                            else if (mode == "darken") v = std::min(cs * d[3], cd * s[3]) + cs * (1 - d[3]) + cd * (1 - s[3]);
                            else if (mode == "lighten") v = std::max(cs * d[3], cd * s[3]) + cs * (1 - d[3]) + cd * (1 - s[3]);
                            else v = cs + cd * (1 - s[3]);
                            o[k] = v;
                        }
                        o[3] = s[3] + d[3] * (1 - s[3]);
                        continue;
                    }
                    float fa, fb;
                    if (op == "in") { fa = d[3]; fb = 0; }
                    else if (op == "out") { fa = 1 - d[3]; fb = 0; }
                    else if (op == "atop") { fa = d[3]; fb = 1 - s[3]; }
                    else if (op == "xor") { fa = 1 - d[3]; fb = 1 - s[3]; }
                    else if (op == "arithmetic") {
                        for (int k = 0; k < 4; ++k) o[k] = static_cast<float>(std::max(0.0, std::min(1.0, k1 * s[k] * d[k] + k2 * s[k] + k3 * d[k] + k4)));
                        for (int k = 0; k < 3; ++k) o[k] = std::min(o[k], o[3]);
                        continue;
                    } else { fa = 1; fb = 1 - s[3]; }
                    for (int k = 0; k < 4; ++k) o[k] = s[k] * fa + d[k] * fb;
                }
            } else if (p.tag == "feMerge") {
                result = Layer(m_w, m_h);
                for (int mn : p.children) {
                    const XmlNode& m = m_doc.nodes[static_cast<std::size_t>(mn)];
                    if (m.is_text || m.tag != "feMergeNode") continue;
                    const Layer in = get_input(m.attr("in"));
                    for (std::size_t i = 0; i < npx; ++i) {
                        const float a = in.px[i * 4 + 3];
                        for (int k = 0; k < 4; ++k) result.px[i * 4 + static_cast<std::size_t>(k)] = in.px[i * 4 + static_cast<std::size_t>(k)] + result.px[i * 4 + static_cast<std::size_t>(k)] * (1 - a);
                    }
                }
            } else if (p.tag == "feColorMatrix") {
                result = get_input(p.attr("in"));
                const std::string type = p.attr("type") ? trim(*p.attr("type")) : "matrix";
                std::vector<double> v = p.attr("values") ? parse_number_list(*p.attr("values")) : std::vector<double>{};
                double mtx[20] = { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0 };
                if (type == "matrix" && v.size() == 20) for (int k = 0; k < 20; ++k) mtx[k] = v[static_cast<std::size_t>(k)];
                else if (type == "saturate") {
                    const double s = v.empty() ? 1 : v[0];
                    const double m2[20] = { 0.213 + 0.787 * s, 0.715 - 0.715 * s, 0.072 - 0.072 * s, 0, 0, 0.213 - 0.213 * s, 0.715 + 0.285 * s, 0.072 - 0.072 * s, 0, 0, 0.213 - 0.213 * s, 0.715 - 0.715 * s, 0.072 + 0.928 * s, 0, 0, 0, 0, 0, 1, 0 };
                    std::copy(m2, m2 + 20, mtx);
                } else if (type == "hueRotate") {
                    const double a = (v.empty() ? 0 : v[0]) * kPi / 180, cs = std::cos(a), sn = std::sin(a);
                    const double m2[20] = { 0.213 + cs * 0.787 - sn * 0.213, 0.715 - cs * 0.715 - sn * 0.715, 0.072 - cs * 0.072 + sn * 0.928, 0, 0,
                                            0.213 - cs * 0.213 + sn * 0.143, 0.715 + cs * 0.285 + sn * 0.140, 0.072 - cs * 0.072 - sn * 0.283, 0, 0,
                                            0.213 - cs * 0.213 - sn * 0.787, 0.715 - cs * 0.715 + sn * 0.715, 0.072 + cs * 0.928 + sn * 0.072, 0, 0, 0, 0, 0, 1, 0 };
                    std::copy(m2, m2 + 20, mtx);
                } else if (type == "luminanceToAlpha") {
                    const double m2[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125, 0.7154, 0.0721, 0, 0 };
                    std::copy(m2, m2 + 20, mtx);
                }
                for (std::size_t i = 0; i < npx; ++i) {
                    float* q = &result.px[i * 4];
                    const double a = q[3];
                    const double r = a > 0 ? q[0] / a : 0, g = a > 0 ? q[1] / a : 0, b = a > 0 ? q[2] / a : 0;
                    double o[4];
                    for (int k = 0; k < 4; ++k) o[k] = std::max(0.0, std::min(1.0, mtx[k * 5] * r + mtx[k * 5 + 1] * g + mtx[k * 5 + 2] * b + mtx[k * 5 + 3] * a + mtx[k * 5 + 4]));
                    q[0] = static_cast<float>(o[0] * o[3]);
                    q[1] = static_cast<float>(o[1] * o[3]);
                    q[2] = static_cast<float>(o[2] * o[3]);
                    q[3] = static_cast<float>(o[3]);
                }
            } else {
                return false;
            }

            last = result;
            if (const std::string* r = p.attr("result")) results[trim(*r)] = result;
        }

        double fx, fy, fw, fh;
        const std::string* fu = fn.attr("filterUnits");
        auto frac = [&](const char* name, double def) {
            const std::string* v = fn.attr(name);
            if (!v) return def;
            double x;
            std::size_t used;
            if (!parse_number_prefix(*v, x, used)) return def;
            if (trim(std::string_view(*v).substr(used)) == "%") x /= 100.0;
            return x;
        };
        if (!(fu && trim(*fu) == "userSpaceOnUse")) {
            fx = bx0 + frac("x", -0.1) * (bx1 - bx0);
            fy = by0 + frac("y", -0.1) * (by1 - by0);
            fw = frac("width", 1.2) * (bx1 - bx0);
            fh = frac("height", 1.2) * (by1 - by0);
        } else {
            fx = attr_len(filter_node, "x", -0.1 * m_vp_stack_w, m_vp_stack_w, st);
            fy = attr_len(filter_node, "y", -0.1 * m_vp_stack_h, m_vp_stack_h, st);
            fw = attr_len(filter_node, "width", 1.2 * m_vp_stack_w, m_vp_stack_w, st);
            fh = attr_len(filter_node, "height", 1.2 * m_vp_stack_h, m_vp_stack_h, st);
        }

        Path region;
        region.move_to({ fx, fy });
        region.line_to({ fx + fw, fy });
        region.line_to({ fx + fw, fy + fh });
        region.line_to({ fx, fy + fh });
        region.close();
        Coverage cov(m_w, m_h);
        for (const auto& pl : flatten(region, ctm, 0.02)) cov.add_polyline(pl);
        std::vector<float> rc;
        int x0, y0, x1, y1;
        cov.resolve(false, rc, x0, y0, x1, y1);
        Layer clipped(m_w, m_h);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
                const float k = rc[static_cast<std::size_t>(y - y0) * static_cast<std::size_t>(x1 - x0) + static_cast<std::size_t>(x - x0)];
                for (int q = 0; q < 4; ++q) clipped.px[i * 4 + static_cast<std::size_t>(q)] = last.px[i * 4 + static_cast<std::size_t>(q)] * k;
            }
        layer = std::move(clipped);
        return true;
    }

    void draw_image(int node, const State& st, const Viewport& vp, const Matrix& ctm, Layer& target, double opacity) {
        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        const std::string* href = n.attr("href");
        if (!href) href = n.attr("xlink:href");
        if (!href) return;
        const std::string h = trim(*href);
        if (h.rfind("data:", 0) != 0) return;
        const std::size_t comma = h.find(',');
        if (comma == std::string::npos) return;
        const std::string meta = h.substr(5, comma - 5);
        std::vector<std::uint8_t> bytes;

        if (meta.find(";base64") != std::string::npos) {
            static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::uint32_t acc = 0;
            int bits = 0;
            for (std::size_t i = comma + 1; i < h.size(); ++i) {
                const char c = h[i];
                if (c == '=') break;
                const std::size_t v = chars.find(c);
                if (v == std::string::npos) continue;
                acc = (acc << 6) | static_cast<std::uint32_t>(v);
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    bytes.push_back(static_cast<std::uint8_t>(acc >> bits));
                }
            }
        } else {
            for (std::size_t i = comma + 1; i < h.size(); ++i) {
                if (h[i] == '%' && i + 2 < h.size()) {
                    bytes.push_back(static_cast<std::uint8_t>(std::strtol(h.substr(i + 1, 2).c_str(), nullptr, 16)));
                    i += 2;
                } else {
                    bytes.push_back(static_cast<std::uint8_t>(h[i]));
                }
            }
        }

        BitmapImage img;
        if (meta.find("svg") != std::string::npos || is_svg(bytes.data(), bytes.size())) {
            if (m_nested_depth > 4) return;
            SvgOptions o;
            ++m_nested_depth;
            const bool ok = decode_svg(bytes.data(), bytes.size(), img, o, nullptr);
            --m_nested_depth;
            if (!ok) return;
        } else if (!decode_image(bytes.data(), bytes.size(), img, nullptr)) {
            return;
        }

        const double ix = attr_len(node, "x", 0, vp.w, st), iy = attr_len(node, "y", 0, vp.h, st);
        double iw = attr_len(node, "width", -1, vp.w, st), ih = attr_len(node, "height", -1, vp.h, st);
        if (iw < 0 && ih < 0) { iw = img.width(); ih = img.height(); }
        else if (iw < 0) iw = ih * img.width() / img.height();
        else if (ih < 0) ih = iw * img.height() / img.width();
        if (iw <= 0 || ih <= 0) return;

        const std::string vb = "0 0 " + std::to_string(img.width()) + " " + std::to_string(img.height());
        const Matrix m = ctm * viewbox_transform(&vb, n.attr("preserveAspectRatio"), ix, iy, iw, ih);
        Matrix inv;
        if (!m.invert(inv)) return;
        const std::vector<std::uint8_t> rgba = img.to_rgba();
        const int IW = static_cast<int>(img.width()), IH = static_cast<int>(img.height());

        Path clip;
        clip.move_to({ ix, iy });
        clip.line_to({ ix + iw, iy });
        clip.line_to({ ix + iw, iy + ih });
        clip.line_to({ ix, iy + ih });
        clip.close();
        Path full;
        full.move_to({ 0, 0 });
        full.line_to({ static_cast<double>(IW), 0 });
        full.line_to({ static_cast<double>(IW), static_cast<double>(IH) });
        full.line_to({ 0, static_cast<double>(IH) });
        full.close();
        Coverage cov(m_w, m_h), cov2(m_w, m_h);
        for (const auto& p : flatten(clip, ctm, 0.02)) cov.add_polyline(p);
        for (const auto& p : flatten(full, m, 0.02)) cov2.add_polyline(p);
        std::vector<float> c1, c2;
        int ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
        cov.resolve(false, c1, ax0, ay0, ax1, ay1);
        cov2.resolve(false, c2, bx0, by0, bx1, by1);
        const bool pixelated = [&] {
            const std::string* r = prop(node, "image-rendering");
            return r && (trim(*r) == "pixelated" || trim(*r) == "optimizeSpeed" || trim(*r) == "crisp-edges");
        }();

        for (int y = std::max(ay0, by0); y < std::min(ay1, by1); ++y)
            for (int x = std::max(ax0, bx0); x < std::min(ax1, bx1); ++x) {
                const float k = c1[static_cast<std::size_t>(y - ay0) * static_cast<std::size_t>(ax1 - ax0) + static_cast<std::size_t>(x - ax0)] * c2[static_cast<std::size_t>(y - by0) * static_cast<std::size_t>(bx1 - bx0) + static_cast<std::size_t>(x - bx0)];
                if (k <= 0) continue;
                const Vec2 sp = inv.apply({ x + 0.5, y + 0.5 });
                float px[4] = { 0, 0, 0, 0 };
                if (pixelated) {
                    const int sx0 = std::min(IW - 1, std::max(0, static_cast<int>(std::floor(sp.x))));
                    const int sy0 = std::min(IH - 1, std::max(0, static_cast<int>(std::floor(sp.y))));
                    const std::uint8_t* q = &rgba[(static_cast<std::size_t>(sy0) * static_cast<std::size_t>(IW) + static_cast<std::size_t>(sx0)) * 4];
                    const float a = q[3] / 255.0f;
                    px[0] = q[0] / 255.0f * a; px[1] = q[1] / 255.0f * a; px[2] = q[2] / 255.0f * a; px[3] = a;
                } else {
                    const double fx = sp.x - 0.5, fy = sp.y - 0.5;
                    const int sx0 = static_cast<int>(std::floor(fx)), sy0 = static_cast<int>(std::floor(fy));
                    const double tx = fx - sx0, ty = fy - sy0;
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx) {
                            const int qx = std::min(IW - 1, std::max(0, sx0 + dx)), qy = std::min(IH - 1, std::max(0, sy0 + dy));
                            const double wgt = (dx ? tx : 1 - tx) * (dy ? ty : 1 - ty);
                            const std::uint8_t* q = &rgba[(static_cast<std::size_t>(qy) * static_cast<std::size_t>(IW) + static_cast<std::size_t>(qx)) * 4];
                            const double a = q[3] / 255.0;
                            px[0] += static_cast<float>(wgt * q[0] / 255.0 * a);
                            px[1] += static_cast<float>(wgt * q[1] / 255.0 * a);
                            px[2] += static_cast<float>(wgt * q[2] / 255.0 * a);
                            px[3] += static_cast<float>(wgt * a);
                        }
                }
                const float f = static_cast<float>(k * opacity);
                float* d = &target.px[(static_cast<std::size_t>(y) * static_cast<std::size_t>(target.w) + static_cast<std::size_t>(x)) * 4];
                const float inva = 1 - px[3] * f;
                for (int q = 0; q < 4; ++q) d[q] = px[q] * f + d[q] * inva;
            }
    }

    struct TextRun {
        std::string text;
        State       st;
        bool        has_x = false, has_y = false;
        double      x = 0, y = 0, dx = 0, dy = 0;
        double      px = 0, py = 0, width = 0;
        int         chunk = 0;
    };

    static double first_number(const std::string* s, double def, bool& has) {
        has = false;
        if (!s) return def;
        const std::vector<double> v = parse_number_list(*s);
        if (v.empty()) return def;
        has = true;
        return v[0];
    }

    void collect_runs(int node, const State& st, const Viewport& vp, std::vector<TextRun>& runs, bool& last_space, bool first_level) const {
        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        bool pending_pos = false;
        TextRun pos;

        if (!first_level) {
            const std::string* xs = n.attr("x");
            const std::string* ys = n.attr("y");
            pos.x = xs ? length(xs, 0, vp.w, st.font_size) : 0;
            pos.y = ys ? length(ys, 0, vp.h, st.font_size) : 0;
            pos.has_x = xs != nullptr;
            pos.has_y = ys != nullptr;
            bool h;
            pos.dx = first_number(n.attr("dx"), 0, h);
            pos.dy = first_number(n.attr("dy"), 0, h);
            pending_pos = pos.has_x || pos.has_y || pos.dx != 0 || pos.dy != 0;
        }

        for (int c : n.children) {
            const XmlNode& ch = m_doc.nodes[static_cast<std::size_t>(c)];
            if (ch.is_text) {
                std::string t;
                for (char k : ch.text) {
                    if (k == '\n' || k == '\r') {
                        if (st.preserve_space) t.push_back(' ');
                        continue;
                    }
                    if (k == '\t') k = ' ';
                    if (!st.preserve_space && k == ' ') {
                        if (last_space) continue;
                        last_space = true;
                    } else {
                        last_space = false;
                    }
                    t.push_back(k);
                }
                if (t.empty()) continue;
                TextRun r = pending_pos ? pos : TextRun();
                pending_pos = false;
                r.text = std::move(t);
                r.st = st;
                runs.push_back(std::move(r));
            } else if (ch.tag == "tspan" || ch.tag == "a" || ch.tag == "textPath") {
                if (!displayed(c)) continue;
                State cs = st;
                apply_style(c, cs, vp);
                const std::size_t before = runs.size();
                collect_runs(c, cs, vp, runs, last_space, false);
                if (pending_pos && runs.size() > before) {
                    TextRun& r = runs[before];
                    if (!r.has_x && pos.has_x) { r.has_x = true; r.x = pos.x; }
                    if (!r.has_y && pos.has_y) { r.has_y = true; r.y = pos.y; }
                    r.dx += pos.dx;
                    r.dy += pos.dy;
                    pending_pos = false;
                }
            }
        }
    }

    void render_text(int node, const State& st, const Viewport& vp, const Matrix& ctm, Layer& target, double opacity) {
        std::vector<TextRun> runs;
        bool last_space = true;
        collect_runs(node, st, vp, runs, last_space, true);
        if (!st.preserve_space) {
            while (!runs.empty() && !runs.back().text.empty() && runs.back().text.back() == ' ') {
                runs.back().text.pop_back();
                if (runs.back().text.empty()) runs.pop_back();
            }
        }
        if (runs.empty()) return;

        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        bool h;
        double cx = 0, cy = 0;
        if (const std::string* xs = n.attr("x")) cx = length(xs, 0, vp.w, st.font_size);
        if (const std::string* ys = n.attr("y")) cy = length(ys, 0, vp.h, st.font_size);
        cx += first_number(n.attr("dx"), 0, h);
        cy += first_number(n.attr("dy"), 0, h);
        const double scale = std::max(1e-6, ctm.scale_factor());

        static std::mutex text_mutex;
        std::lock_guard<std::mutex> lock(text_mutex);
        text::TextRasterizer& raster = text::shared_text_rasterizer();
        if (!raster.ready()) return;

        auto make_rich = [&](const TextRun& r, const colors::Color& color) {
            text::TextStyle ts(r.st.font_size * scale, color);
            ts.set_weight(static_cast<text::FontWeight>(std::max(100, std::min(900, (r.st.font_weight + 50) / 100 * 100))));
            if (r.st.italic) ts.set_italic(true);
            text::RichText rt(ts);
            rt.add(r.text, ts, r.st.font_family);
            return rt;
        };

        int chunk = 0;
        for (std::size_t i = 0; i < runs.size(); ++i) {
            TextRun& r = runs[i];
            if (r.has_x) {
                cx = r.x;
                if (i > 0) ++chunk;
            }
            if (r.has_y) cy = r.y;
            cx += r.dx;
            cy += r.dy;
            r.px = cx;
            r.py = cy;
            r.chunk = chunk;
            const text::TextInk ink = raster.measure(make_rich(r, colors::Color(255, 255, 255)));
            r.width = ink.advance / scale;
            cx += r.width;
        }

        for (int c = 0; c <= chunk; ++c) {
            double x0 = 1e300, x1 = -1e300;
            int anchor = 0;
            bool first = true;
            for (const TextRun& r : runs)
                if (r.chunk == c) {
                    if (first) { anchor = r.st.text_anchor; first = false; }
                    x0 = std::min(x0, r.px);
                    x1 = std::max(x1, r.px + r.width);
                }
            if (first || anchor == 0) continue;
            const double shift = anchor == 1 ? -(x1 - x0) / 2 : -(x1 - x0);
            for (TextRun& r : runs) if (r.chunk == c) r.px += shift;
        }

        Matrix inv_ctm;
        if (!ctm.invert(inv_ctm)) return;

        for (const TextRun& r : runs) {
            if (!r.st.visible) continue;
            Path dummy;
            dummy.move_to({ r.px, r.py - r.st.font_size });
            dummy.line_to({ r.px + std::max(r.width, 1e-6), r.py });
            PaintSource paint;
            if (!make_paint(r.st.fill, r.st, r.st.fill_opacity * opacity, ctm, dummy, paint, 0)) continue;
            const bool solid = paint.solid;
            const colors::Color draw_color = solid ? colors::Color(static_cast<std::uint8_t>(std::lround(paint.color.r * 255)), static_cast<std::uint8_t>(std::lround(paint.color.g * 255)), static_cast<std::uint8_t>(std::lround(paint.color.b * 255)), 255) : colors::Color(255, 255, 255, 255);
            const text::RichText rt = make_rich(r, draw_color);
            const text::TextInk ink = raster.measure(rt);
            const double fs = r.st.font_size * scale;
            const double pad = 2 + fs * 0.35;
            const int tw = static_cast<int>(std::ceil(ink.advance + 2 * pad));
            const int th = static_cast<int>(std::ceil(ink.line_ascent + ink.line_descent + 2 * pad));
            if (tw <= 0 || th <= 0 || static_cast<long long>(tw) * th > (1ll << 26)) continue;
            BitmapImage tmp(static_cast<unsigned>(tw), static_cast<unsigned>(th), std::vector<colors::Color>(static_cast<std::size_t>(tw) * static_cast<std::size_t>(th), colors::Color(0, 0, 0, 0)));
            const double base_v = pad + ink.line_ascent;
            raster.draw(tmp, rt, static_cast<float>(pad), static_cast<float>(base_v));
            const std::vector<std::uint8_t> px = tmp.to_rgba();

            auto to_user = [&](double u, double v) -> Vec2 { return { r.px + (u - pad) / scale, r.py + (v - base_v) / scale }; };
            double dx0 = 1e300, dy0 = 1e300, dx1 = -1e300, dy1 = -1e300;
            for (Vec2 corner : { to_user(0, 0), to_user(tw, 0), to_user(0, th), to_user(tw, th) }) {
                const Vec2 d = ctm.apply(corner);
                dx0 = std::min(dx0, d.x); dy0 = std::min(dy0, d.y); dx1 = std::max(dx1, d.x); dy1 = std::max(dy1, d.y);
            }
            const int ix0 = std::max(0, static_cast<int>(std::floor(dx0))), iy0 = std::max(0, static_cast<int>(std::floor(dy0)));
            const int ix1 = std::min(target.w, static_cast<int>(std::ceil(dx1))), iy1 = std::min(target.h, static_cast<int>(std::ceil(dy1)));

            for (int y = iy0; y < iy1; ++y)
                for (int x = ix0; x < ix1; ++x) {
                    const Vec2 u = inv_ctm.apply({ x + 0.5, y + 0.5 });
                    const double fu = (u.x - r.px) * scale + pad - 0.5, fv = (u.y - r.py) * scale + base_v - 0.5;
                    const int su = static_cast<int>(std::floor(fu)), sv = static_cast<int>(std::floor(fv));
                    const double tu = fu - su, tv = fv - sv;
                    double acc[4] = { 0, 0, 0, 0 };
                    for (int k = 0; k < 4; ++k) {
                        const int qx = su + (k & 1), qy = sv + (k >> 1);
                        if (qx < 0 || qy < 0 || qx >= tw || qy >= th) continue;
                        const double wgt = ((k & 1) ? tu : 1 - tu) * ((k >> 1) ? tv : 1 - tv);
                        const std::uint8_t* q = &px[(static_cast<std::size_t>(qy) * static_cast<std::size_t>(tw) + static_cast<std::size_t>(qx)) * 4];
                        const double a = q[3] / 255.0;
                        acc[0] += wgt * q[0] / 255.0 * a;
                        acc[1] += wgt * q[1] / 255.0 * a;
                        acc[2] += wgt * q[2] / 255.0 * a;
                        acc[3] += wgt * a;
                    }
                    if (acc[3] <= 0) continue;
                    float s4[4];
                    if (solid) {
                        const double k = paint.color.a * paint.opacity;
                        for (int q = 0; q < 4; ++q) s4[q] = static_cast<float>(acc[q] * k);
                    } else {
                        const Rgba32 c = paint.at(x + 0.5, y + 0.5);
                        const double a = c.a * paint.opacity * acc[3];
                        s4[0] = static_cast<float>(c.r * a);
                        s4[1] = static_cast<float>(c.g * a);
                        s4[2] = static_cast<float>(c.b * a);
                        s4[3] = static_cast<float>(a);
                    }
                    float* d = &target.px[(static_cast<std::size_t>(y) * static_cast<std::size_t>(target.w) + static_cast<std::size_t>(x)) * 4];
                    const float inva = 1 - s4[3];
                    for (int q = 0; q < 4; ++q) d[q] = s4[q] + d[q] * inva;
                }
        }
    }

public:
    void render_node(int node, const Matrix& parent_ctm, const State& parent, const Viewport& vp, Layer& target, int depth) {
        if (depth > 64) return;
        const XmlNode& n = m_doc.nodes[static_cast<std::size_t>(node)];
        if (n.is_text || !displayed(node)) return;
        const std::string& tag = n.tag;
        static const char* const skip[] = { "defs", "clipPath", "mask", "linearGradient", "radialGradient", "pattern", "marker", "style", "title", "desc", "metadata", "filter", "symbol", "script", "font", "font-face", "foreignObject" };
        for (const char* s : skip) if (tag == s) return;

        State st = parent;
        apply_style(node, st, vp);
        Matrix ctm = parent_ctm * node_transform(node);
        Viewport child_vp = vp;
        double opacity = 1;
        if (const std::string* op = prop(node, "opacity")) opacity = parse_opacity(*op, 1);
        const int clip = resolve_url(prop(node, "clip-path"));
        const int mask = resolve_url(prop(node, "mask"));
        const int filter = resolve_url(prop(node, "filter"));
        const std::string* filter_raw = prop(node, "filter");
        if (filter_raw && trim(*filter_raw) != "none" && filter < 0) return;
        if (opacity <= 0) return;

        Path shape;
        const bool is_shape = build_shape(node, st, vp, shape);
        const bool is_container = tag == "g" || tag == "a" || tag == "switch" || tag == "svg" || tag == "use";
        if (!is_shape && !is_container && tag != "image" && tag != "text") return;

        const bool needs_layer = clip >= 0 || mask >= 0 || filter >= 0 || (opacity < 1 && (is_container || (st.fill.kind != PaintSpec::None && st.stroke.kind != PaintSpec::None) || tag == "image" || tag == "text"));
        Layer local;
        Layer* dst = &target;
        if (needs_layer) {
            local = Layer(m_w, m_h);
            dst = &local;
        }
        const double draw_opacity = needs_layer ? 1.0 : opacity;

        const double saved_w = m_vp_stack_w, saved_h = m_vp_stack_h;

        if (is_shape) {
            if (st.visible) paint_shape(node, shape, st, ctm, *dst, draw_opacity, depth);
        } else if (tag == "image") {
            if (st.visible) draw_image(node, st, vp, ctm, *dst, draw_opacity);
        } else if (tag == "text") {
            render_text(node, st, vp, ctm, *dst, draw_opacity);
        } else if (tag == "svg") {
            const double x = attr_len(node, "x", 0, vp.w, st), y = attr_len(node, "y", 0, vp.h, st);
            const double w = attr_len(node, "width", vp.w, vp.w, st), h = attr_len(node, "height", vp.h, vp.h, st);
            if (w <= 0 || h <= 0) return;
            Matrix inner = ctm * viewbox_transform(n.attr("viewBox"), n.attr("preserveAspectRatio"), x, y, w, h);
            child_vp = Viewport{ w, h };
            if (const std::string* vb = n.attr("viewBox")) {
                const std::vector<double> v = parse_number_list(*vb);
                if (v.size() == 4 && v[2] > 0 && v[3] > 0) child_vp = Viewport{ v[2], v[3] };
            }
            m_vp_stack_w = child_vp.w;
            m_vp_stack_h = child_vp.h;
            const std::string* ov = prop(node, "overflow");
            const bool clip_vp = !(ov && (trim(*ov) == "visible" || trim(*ov) == "auto"));
            if (clip_vp) {
                Layer inner_layer(m_w, m_h);
                for (int c : n.children) render_node(c, inner, st, child_vp, inner_layer, depth + 1);
                apply_rect_clip(inner_layer, ctm, x, y, w, h);
                composite_layer(*dst, inner_layer, 1.0, nullptr);
            } else {
                for (int c : n.children) render_node(c, inner, st, child_vp, *dst, depth + 1);
            }
        } else if (tag == "use") {
            const int ref = resolve_href(node);
            if (ref < 0 || ref == node) return;
            for (int p = n.parent; p >= 0; p = m_doc.nodes[static_cast<std::size_t>(p)].parent) if (p == ref) return;
            const double x = attr_len(node, "x", 0, vp.w, st), y = attr_len(node, "y", 0, vp.h, st);
            const Matrix um = ctm * Matrix::translate(x, y);
            const XmlNode& rn = m_doc.nodes[static_cast<std::size_t>(ref)];
            if (rn.tag == "symbol" || rn.tag == "svg") {
                State rs = st;
                apply_style(ref, rs, vp);
                double w = attr_len(node, "width", -1, vp.w, st), h = attr_len(node, "height", -1, vp.h, st);
                if (w < 0) w = attr_len(ref, "width", vp.w, vp.w, st);
                if (h < 0) h = attr_len(ref, "height", vp.h, vp.h, st);
                if (w <= 0 || h <= 0) return;
                const double sx = rn.tag == "svg" ? attr_len(ref, "x", 0, vp.w, st) : 0, sy = rn.tag == "svg" ? attr_len(ref, "y", 0, vp.h, st) : 0;
                const Matrix inner = um * node_transform(ref) * viewbox_transform(rn.attr("viewBox"), rn.attr("preserveAspectRatio"), sx, sy, w, h);
                child_vp = Viewport{ w, h };
                if (const std::string* vb = rn.attr("viewBox")) {
                    const std::vector<double> v = parse_number_list(*vb);
                    if (v.size() == 4 && v[2] > 0 && v[3] > 0) child_vp = Viewport{ v[2], v[3] };
                }
                m_vp_stack_w = child_vp.w;
                m_vp_stack_h = child_vp.h;
                const std::string* ov = prop(ref, "overflow");
                const bool clip_vp = !(ov && (trim(*ov) == "visible" || trim(*ov) == "auto"));
                Layer inner_layer(m_w, m_h);
                for (int c : rn.children) render_node(c, inner, rs, child_vp, clip_vp ? inner_layer : *dst, depth + 1);
                if (clip_vp) {
                    apply_rect_clip(inner_layer, um * node_transform(ref), sx, sy, w, h);
                    composite_layer(*dst, inner_layer, 1.0, nullptr);
                }
            } else {
                render_node(ref, um, st, vp, *dst, depth + 1);
            }
        } else if (tag == "switch") {
            for (int c : n.children) {
                if (m_doc.nodes[static_cast<std::size_t>(c)].is_text) continue;
                const XmlNode& cn = m_doc.nodes[static_cast<std::size_t>(c)];
                if (cn.attr("requiredExtensions") || cn.attr("systemLanguage")) continue;
                render_node(c, ctm, st, vp, *dst, depth + 1);
                break;
            }
        } else {
            for (int c : n.children) render_node(c, ctm, st, child_vp, *dst, depth + 1);
        }

        m_vp_stack_w = saved_w;
        m_vp_stack_h = saved_h;
        if (!needs_layer) return;

        double bx0 = 1e300, by0 = 1e300, bx1 = -1e300, by1 = -1e300;
        if (clip >= 0 || mask >= 0 || filter >= 0) element_bbox(node, st, vp, Matrix(), bx0, by0, bx1, by1, 0);
        if (bx0 > bx1) { bx0 = by0 = 0; bx1 = by1 = 0; }

        if (filter >= 0 && !apply_filter(filter, local, ctm, bx0, by0, bx1, by1, st)) return;

        std::vector<float> factor;
        if (clip >= 0) factor = clip_mask(clip, parent_ctm * node_transform(node), &shape, bx0, by0, bx1, by1, 0);
        if (mask >= 0) {
            const std::vector<float> mv = mask_values(mask, ctm, bx0, by0, bx1, by1, st, depth);
            if (factor.empty()) factor = mv;
            else for (std::size_t i = 0; i < factor.size(); ++i) factor[i] *= mv[i];
        }
        composite_layer(target, local, opacity, factor.empty() ? nullptr : &factor);
    }

private:
    void apply_rect_clip(Layer& layer, const Matrix& m, double x, double y, double w, double h) const {
        Path r;
        r.move_to({ x, y });
        r.line_to({ x + w, y });
        r.line_to({ x + w, y + h });
        r.line_to({ x, y + h });
        r.close();
        Coverage cov(m_w, m_h);
        for (const auto& p : flatten(r, m, 0.02)) cov.add_polyline(p);
        std::vector<float> c;
        int x0, y0, x1, y1;
        cov.resolve(false, c, x0, y0, x1, y1);
        for (int yy = 0; yy < m_h; ++yy)
            for (int xx = 0; xx < m_w; ++xx) {
                float k = 0;
                if (yy >= y0 && yy < y1 && xx >= x0 && xx < x1) k = c[static_cast<std::size_t>(yy - y0) * static_cast<std::size_t>(x1 - x0) + static_cast<std::size_t>(xx - x0)];
                float* p = &layer.px[(static_cast<std::size_t>(yy) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(xx)) * 4];
                for (int q = 0; q < 4; ++q) p[q] *= k;
            }
    }

    void composite_layer(Layer& dst, const Layer& src, double opacity, const std::vector<float>* factor) const {
        const std::size_t n = static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h);
        for (std::size_t i = 0; i < n; ++i) {
            float k = static_cast<float>(opacity);
            if (factor) k *= (*factor)[i];
            if (k <= 0) continue;
            const float* s = &src.px[i * 4];
            if (s[3] <= 0) continue;
            float* d = &dst.px[i * 4];
            const float inv = 1 - s[3] * k;
            for (int q = 0; q < 4; ++q) d[q] = s[q] * k + d[q] * inv;
        }
    }

    std::unordered_map<std::string, int>                           m_ids;
    std::vector<std::unordered_map<std::string, std::string>>      m_props;
    int                                                            m_w, m_h;

public:
    double m_vp_stack_w = 100, m_vp_stack_h = 100;
    int    m_nested_depth = 0;
};

} // namespace

} // namespace svg_detail

bool is_svg(const std::uint8_t* data, std::size_t size) noexcept {
    std::size_t i = 0;
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) i = 3;
    const std::size_t limit = std::min<std::size_t>(size, 4096);
    while (i < limit) {
        while (i < limit && std::isspace(data[i])) ++i;
        if (i >= limit || data[i] != '<') return false;
        if (i + 4 <= limit && std::memcmp(data + i, "<svg", 4) == 0) return true;
        if (i + 8 <= limit && std::memcmp(data + i, "<svg:svg", 8) == 0) return true;
        if (i + 4 <= limit && (std::memcmp(data + i, "<!--", 4) == 0)) {
            const std::uint8_t* e = static_cast<const std::uint8_t*>(std::memchr(data + i + 4, '>', limit - i - 4));
            while (e && !(e - data >= 2 && e[-1] == '-' && e[-2] == '-')) e = static_cast<const std::uint8_t*>(std::memchr(e + 1, '>', limit - static_cast<std::size_t>(e + 1 - data)));
            if (!e) return false;
            i = static_cast<std::size_t>(e - data) + 1;
            continue;
        }
        if (i + 2 <= limit && (data[i + 1] == '?' || data[i + 1] == '!')) {
            int depth = 0;
            std::size_t j = i + 2;
            for (; j < limit; ++j) {
                if (data[j] == '[') ++depth;
                else if (data[j] == ']') --depth;
                else if (data[j] == '>' && depth <= 0) break;
            }
            if (j >= limit) return false;
            i = j + 1;
            continue;
        }
        return false;
    }
    return false;
}

bool decode_svg(std::string_view source, BitmapImage& out, const SvgOptions& options, std::string* error) {
    using namespace svg_detail;
    XmlDocument doc;
    if (!parse_xml(source, doc, error)) return false;
    const XmlNode& root = doc.nodes[static_cast<std::size_t>(doc.root)];
    if (root.tag != "svg") return fmt::fail(error, "root element is not <svg>");

    auto len = [&](const std::string* s, double def, double ref) {
        if (!s) return def;
        double v;
        std::size_t used;
        std::string t = *s;
        std::size_t i = 0;
        while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) ++i;
        const char* p = t.c_str() + i;
        char* end = nullptr;
        v = std::strtod(p, &end);
        if (end == p || !std::isfinite(v)) return def;
        used = static_cast<std::size_t>(end - t.c_str());
        std::string unit = t.substr(used);
        while (!unit.empty() && std::isspace(static_cast<unsigned char>(unit.back()))) unit.pop_back();
        const double px_per_in = options.dpi > 0 ? options.dpi : 96.0;
        if (unit.empty() || unit == "px") return v;
        if (unit == "%") return ref > 0 ? v / 100.0 * ref : def;
        if (unit == "pt") return v * px_per_in / 72.0;
        if (unit == "pc") return v * px_per_in / 6.0;
        if (unit == "mm") return v * px_per_in / 25.4;
        if (unit == "cm") return v * px_per_in / 2.54;
        if (unit == "in") return v * px_per_in;
        if (unit == "em") return v * 16.0;
        if (unit == "ex") return v * 8.0;
        return def;
    };

    double vbw = 0, vbh = 0;
    std::vector<double> vb;
    if (const std::string* v = root.attr("viewBox")) {
        std::string s = *v;
        for (char& c : s) if (c == ',') c = ' ';
        char* p = s.data();
        while (*p) {
            char* e = nullptr;
            const double x = std::strtod(p, &e);
            if (e == p) break;
            vb.push_back(x);
            p = e;
        }
        if (vb.size() == 4 && vb[2] > 0 && vb[3] > 0) { vbw = vb[2]; vbh = vb[3]; }
        else vb.clear();
    }

    double w = len(root.attr("width"), -1, vbw), h = len(root.attr("height"), -1, vbh);
    if (w <= 0 && h <= 0) { w = vbw > 0 ? vbw : 100; h = vbh > 0 ? vbh : 100; }
    else if (w <= 0) w = vbw > 0 && vbh > 0 ? h * vbw / vbh : 100;
    else if (h <= 0) h = vbw > 0 && vbh > 0 ? w * vbh / vbw : 100;

    double ow = w * options.scale, oh = h * options.scale;
    if (options.width && options.height) { ow = options.width; oh = options.height; }
    else if (options.width) { ow = options.width; oh = h * options.width / w; }
    else if (options.height) { oh = options.height; ow = w * options.height / h; }
    const int W = std::max(1, static_cast<int>(std::ceil(ow - 1e-9))), H = std::max(1, static_cast<int>(std::ceil(oh - 1e-9)));
    if (!fmt::sane_dimensions(static_cast<std::uint64_t>(W), static_cast<std::uint64_t>(H)) || static_cast<std::uint64_t>(W) * static_cast<std::uint64_t>(H) > (1ull << 26)) return fmt::fail(error, "SVG output is too large");

    Renderer r(doc, W, H);
    Matrix base = Matrix::scale(ow / w, oh / h);
    if (!vb.empty()) {
        std::string vbs = std::to_string(vb[0]) + " " + std::to_string(vb[1]) + " " + std::to_string(vb[2]) + " " + std::to_string(vb[3]);
        const std::string* par = root.attr("preserveAspectRatio");
        double sx = w / vb[2], sy = h / vb[3];
        std::string align = "xmidymid";
        bool slice = false;
        if (par) {
            std::string t;
            for (char c : *par) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            std::size_t a = t.find_first_not_of(' ');
            t = a == std::string::npos ? "" : t.substr(a);
            const std::size_t sp = t.find(' ');
            align = t.substr(0, sp);
            if (sp != std::string::npos) slice = t.find("slice", sp) != std::string::npos;
        }
        Matrix vbm;
        if (align == "none") vbm = Matrix::scale(sx, sy) * Matrix::translate(-vb[0], -vb[1]);
        else {
            const double s = slice ? std::max(sx, sy) : std::min(sx, sy);
            double tx = -vb[0] * s, ty = -vb[1] * s;
            if (align.find("xmid") != std::string::npos) tx += (w - vb[2] * s) / 2;
            else if (align.find("xmax") != std::string::npos) tx += w - vb[2] * s;
            if (align.find("ymid") != std::string::npos) ty += (h - vb[3] * s) / 2;
            else if (align.find("ymax") != std::string::npos) ty += h - vb[3] * s;
            vbm = Matrix::translate(tx, ty) * Matrix::scale(s, s);
        }
        base = base * vbm;
        r.m_root_vw = vb[2];
        r.m_root_vh = vb[3];
    } else {
        r.m_root_vw = w;
        r.m_root_vh = h;
    }
    r.m_vp_stack_w = r.m_root_vw;
    r.m_vp_stack_h = r.m_root_vh;

    Layer canvas(W, H);
    const float bg[4] = { options.background.red() / 255.0f, options.background.green() / 255.0f, options.background.blue() / 255.0f, options.background.alpha() / 255.0f };
    if (bg[3] > 0)
        for (std::size_t i = 0; i < canvas.px.size(); i += 4) {
            canvas.px[i] = bg[0] * bg[3];
            canvas.px[i + 1] = bg[1] * bg[3];
            canvas.px[i + 2] = bg[2] * bg[3];
            canvas.px[i + 3] = bg[3];
        }

    r.render_root(base, canvas);

    fmt::Rgba img(static_cast<unsigned>(W), static_cast<unsigned>(H));
    for (std::size_t i = 0; i < static_cast<std::size_t>(W) * static_cast<std::size_t>(H); ++i) {
        const float a = std::max(0.0f, std::min(1.0f, canvas.px[i * 4 + 3]));
        const long ai = std::lround(a * 255.0);
        img.px[i * 4 + 3] = static_cast<std::uint8_t>(ai);
        if (ai == 0) continue;
        for (int c = 0; c < 3; ++c) {
            const double v = canvas.px[i * 4 + static_cast<std::size_t>(c)] / a;
            img.px[i * 4 + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(std::lround(std::max(0.0, std::min(1.0, v)) * 255.0));
        }
    }

    out = img.to_bitmap();
    return true;
}

bool decode_svg(const std::uint8_t* data, std::size_t size, BitmapImage& out, const SvgOptions& options, std::string* error) {
    if (size >= 2 && data[0] == 0x1F && data[1] == 0x8B) {
        std::vector<std::uint8_t> raw;
        if (!fmt::zlib_inflate(data, size, raw, 0) || raw.empty()) {
            return fmt::fail(error, "corrupt SVGZ");
        }
        return decode_svg(std::string_view(reinterpret_cast<const char*>(raw.data()), raw.size()), out, options, error);
    }
    return decode_svg(std::string_view(reinterpret_cast<const char*>(data), size), out, options, error);
}

} // namespace images
} // namespace fizmo
