#ifndef FIZMO_MATHTEXT_LAYOUT_ENGINE_HPP
#define FIZMO_MATHTEXT_LAYOUT_ENGINE_HPP

#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include "font_metrics.hpp"
#include "math_alphabet.hpp"
#include "../format/number_format.hpp"
#include "../math/math_precedence.hpp"

namespace fizmo {
namespace mathtext {

struct LayoutOptions {
    double      font_size = 24.0;
    bool        display = true;
    double      script_ratio = 0.7;
    double      script_script_ratio = 0.5;
    double      fraction_ratio = 0.85;
    double      min_size_ratio = 0.4;
    double      line_gap = 0.6;
    NumberStyle numbers = NumberStyle::AsWritten;
    bool        math_italic_alphabet = true;
    bool        italic_variables = true;
    bool        italic_greek_lowercase = true;
    bool        limits_above_below = true;
    bool        integral_limits_above_below = true;
    bool        center_lines = false;
};

class LayoutEngine {
public:
    LayoutEngine(const Document& doc, FontMetrics& metrics, const LayoutOptions& options = LayoutOptions()) noexcept : m_doc(doc), m_metrics(metrics), m_opt(options) {}

    MathBox layout() { return layout(m_doc.root()); }

    MathBox layout(NodeId id) {
        const Style s{ m_opt.font_size, 0, m_opt.display };
        if (m_doc.kind(id) == NodeKind::Document) return document(s);
        return node(id, s);
    }

private:
    struct Style {
        double size;
        int    level;
        bool   display;
    };

    Style script_style(const Style& s) const noexcept {
        const double ratio = s.level == 0 ? m_opt.script_ratio : m_opt.script_script_ratio / m_opt.script_ratio;
        return Style{ clamp_size(s.size * (s.level >= 2 ? 1.0 : ratio)), s.level + 1, false };
    }

    Style fraction_style(const Style& s) const noexcept {
        if (s.display && s.level == 0) return Style{ s.size, s.level, false };
        return Style{ clamp_size(s.size * m_opt.fraction_ratio), s.level, false };
    }

    double clamp_size(double size) const noexcept {
        const double floor = m_opt.font_size * m_opt.min_size_ratio;
        return size < floor ? floor : size;
    }

    double rule(const Style& s) const noexcept { return s.size * 0.045 < 1.0 ? 1.0 : s.size * 0.045; }
    double space(const Style& s, double eighteenths) const noexcept { return s.size * eighteenths / 18.0 * (s.level > 0 ? 0.6 : 1.0); }

    double axis(const Style& s) {
        const TextExtent e = m_metrics.measure("+", s.size, GlyphRole::Operator);
        const double center = (e.ascent - e.descent) * 0.5;
        if (center > s.size * 0.1 && center < s.size * 0.45) return center;
        return s.size * 0.25;
    }

    MathBox text(std::string_view str, GlyphRole role, const Style& s, NodeId source = kNoNode) {
        MathBox b;
        if (str.empty()) return b;
        std::string shown(str);
        if (m_opt.math_italic_alphabet && role == GlyphRole::Variable) shown = to_math_italic(shown);
        const TextExtent e = m_metrics.measure(shown, s.size, role);
        LayoutText t;
        t.x = 0.0;
        t.baseline = 0.0;
        t.size = s.size;
        t.role = role;
        t.text = std::move(shown);
        t.source = source;
        b.texts.push_back(std::move(t));
        b.width = e.advance;
        b.ascent = e.ascent;
        b.descent = e.descent;
        return b;
    }

    static MathBox gap(double width) {
        MathBox b;
        b.width = width;
        return b;
    }

    static bool is_identifier(std::string_view s) noexcept {
        if (s.empty() || !chars::is_letter(s[0])) return false;
        for (char c : s) if (!chars::is_letter(c) && !chars::is_digit(c)) return false;
        return true;
    }

    MathBox document(const Style& s) {
        std::vector<MathBox> lines;
        double widest = 0.0;
        for (NodeId id : m_doc.top_level()) {
            lines.push_back(node(id, s));
            if (lines.back().width > widest) widest = lines.back().width;
        }
        MathBox out;
        double y = 0.0;
        bool first = true;
        for (const MathBox& line : lines) {
            if (!first) y = out.descent + m_opt.line_gap * s.size + line.ascent;
            out.place(line, m_opt.center_lines ? (widest - line.width) * 0.5 : 0.0, y);
            first = false;
        }
        return out;
    }

    MathBox wrapped(NodeId id, int required, const Style& s, bool force = false) {
        MathBox inner = node(id, s);
        if (!force && math_precedence(m_doc, id) >= required) return inner;
        return delimited(inner, "(", ")", s, id);
    }

    MathBox delimiter_glyph(std::string_view glyph, const MathBox& inner, double height_needed, double center, const Style& s, NodeId source, bool left) {
        MathBox g = text(glyph, GlyphRole::Delimiter, s, source);
        if (g.ascent + g.descent >= height_needed * 0.98) return g;
        if (inner.ascent <= g.ascent * 1.04 + 0.5 && inner.descent <= g.descent * 1.1 + 0.5) return g;
        MathBox b;
        const double h = height_needed;
        const double top = -(center + h * 0.5);
        const double bottom = -(center - h * 0.5);
        const double th = rule(s) * 1.3;
        const double w = s.size * 0.32 + h * 0.06;
        LayoutPath p;
        p.thickness = th;
        p.role = GlyphRole::Delimiter;
        p.source = source;
        const std::string_view k = glyph;
        auto add = [&](double x, double y) { p.points.push_back(LayoutPoint{ x, y }); };
        if (k == "(" || k == ")") {
            const double xo = left ? w * 0.85 : w * 0.15;
            const double xi = left ? w * 0.2 : w * 0.8;
            for (int i = 0; i <= 24; ++i) {
                const double t = static_cast<double>(i) / 24.0;
                const double y = top + (bottom - top) * t;
                const double bulge = 1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0);
                add(xo + (xi - xo) * bulge, y);
            }
        } else if (k == "[" || k == "]" || k == "\xE2\x8C\x8A" || k == "\xE2\x8C\x8B" || k == "\xE2\x8C\x88" || k == "\xE2\x8C\x89") {
            const bool floor = k == "\xE2\x8C\x8A" || k == "\xE2\x8C\x8B";
            const bool ceil = k == "\xE2\x8C\x88" || k == "\xE2\x8C\x89";
            const double xs = left ? w * 0.3 : w * 0.7;
            const double xe = left ? w * 0.85 : w * 0.15;
            if (!floor) add(xe, top);
            add(xs, top);
            add(xs, bottom);
            if (!ceil) add(xe, bottom);
        } else if (k == "{" || k == "}") {
            const double xs = left ? w * 0.85 : w * 0.15;
            const double xm = left ? w * 0.45 : w * 0.55;
            const double xt = left ? w * 0.1 : w * 0.9;
            const double mid = (top + bottom) * 0.5;
            add(xs, top);
            add(xm, top + h * 0.06);
            add(xm, mid - h * 0.06);
            add(xt, mid);
            add(xm, mid + h * 0.06);
            add(xm, bottom - h * 0.06);
            add(xs, bottom);
        } else if (k == "\xE2\x9F\xA8" || k == "\xE2\x9F\xA9") {
            const double xp = left ? w * 0.85 : w * 0.15;
            const double xq = left ? w * 0.2 : w * 0.8;
            add(xp, top);
            add(xq, (top + bottom) * 0.5);
            add(xp, bottom);
        } else if (k == "|") {
            add(w * 0.5, top);
            add(w * 0.5, bottom);
        } else {
            add(w * 0.4, top);
            add(w * 0.4, bottom);
            b.paths.push_back(p);
            p.points.clear();
            add(w * 0.65, top);
            add(w * 0.65, bottom);
        }
        b.paths.push_back(std::move(p));
        b.width = w;
        b.ascent = -top + th * 0.5;
        b.descent = bottom + th * 0.5;
        return b;
    }

    MathBox delimited(const MathBox& inner, std::string_view open, std::string_view close, const Style& s, NodeId source) {
        const double a = axis(s);
        const double half = std::max(inner.ascent - a, inner.descent + a) + s.size * 0.08;
        const double needed = std::max(half * 2.0, s.size * 0.9);
        MathBox out;
        if (!open.empty()) out.append(delimiter_glyph(open, inner, needed, a, s, source, true));
        out.append(inner, open.empty() ? 0.0 : s.size * 0.03);
        if (!close.empty()) out.append(delimiter_glyph(close, inner, needed, a, s, source, false), s.size * 0.03);
        return out;
    }

    MathBox joined(NodeRange items, const Style& s, std::string_view sep) {
        MathBox row;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i) {
                row.append(text(sep, GlyphRole::Punctuation, s));
                row.append(gap(space(s, 3)));
            }
            row.append(node(items[i], s));
        }
        if (items.empty()) {
            row.ascent = s.size * 0.5;
            row.width = s.size * 0.1;
        }
        return row;
    }

    std::string number_text(NodeId id, NumberStyle style) const {
        std::string t = format_number(m_doc, id, style);
        std::string out;
        for (char c : t) {
            if (c == '-') out += "\xE2\x88\x92";
            else out.push_back(c);
        }
        return out;
    }

    MathBox number(NodeId id, const Style& s) {
        const Node& n = m_doc.node(id);
        if (m_opt.numbers != NumberStyle::TimesTen || !n.marker) return text(number_text(id, m_opt.numbers == NumberStyle::TimesTen ? NumberStyle::AsWritten : m_opt.numbers), GlyphRole::Number, s, id);
        const NumberParts p = number_parts(m_doc, id);
        std::string mantissa = p.negative ? "\xE2\x88\x92" : "";
        mantissa.append(p.mantissa.data(), p.mantissa.size());
        MathBox row = text(mantissa, GlyphRole::Number, s, id);
        row.append(text("\xC3\x97", GlyphRole::Operator, s, id), space(s, 2));
        row.append(text("10", GlyphRole::Number, s, id), space(s, 2));
        std::string exponent = p.exponent_negative && p.exponent_digits != "0" ? "\xE2\x88\x92" : "";
        exponent.append(p.exponent_digits.data(), p.exponent_digits.size());
        MathBox sup = text(exponent, GlyphRole::Number, script_style(s), id);
        return scripts(row, &sup, nullptr, s);
    }

    MathBox string(NodeId id, const Style& s) {
        const NodeRange runs = m_doc.children(id);
        if (runs.size() == 1 && m_doc.kind(runs[0]) == NodeKind::TextRun && (m_doc.node(id).has(node_flags::Variable) || is_identifier(m_doc.decoded_text(runs[0])))) {
            return text(m_doc.decoded_text(runs[0]), m_opt.italic_variables ? GlyphRole::Variable : GlyphRole::Text, s, id);
        }
        MathBox row;
        for (NodeId r : runs) {
            if (m_doc.kind(r) == NodeKind::Symbol) {
                const SymbolInfo* info = m_doc.symbol(r);
                if (info) row.append(text(info->utf8, GlyphRole::Text, s, r));
                else row.append(text(m_doc.command_text(r), GlyphRole::Error, s, r));
            } else {
                row.append(text(m_doc.decoded_text(r), GlyphRole::Text, s, r));
            }
        }
        return row;
    }

    static GlyphRole role_for(SymbolClass c, bool italic_greek) noexcept {
        switch (c) {
            case SymbolClass::GreekLower: return italic_greek ? GlyphRole::Variable : GlyphRole::Symbol;
            case SymbolClass::BinaryOperator:
            case SymbolClass::Relation:
            case SymbolClass::NegatedRelation:
            case SymbolClass::Arrow: return GlyphRole::Operator;
            case SymbolClass::LargeOperator: return GlyphRole::LargeOperator;
            case SymbolClass::Delimiter: return GlyphRole::Delimiter;
            case SymbolClass::Punctuation: return GlyphRole::Punctuation;
            default: return GlyphRole::Symbol;
        }
    }

    double space_width(std::string_view name, const Style& s) const noexcept {
        if (name == "thinspace") return s.size * 3.0 / 18.0;
        if (name == "medspace") return s.size * 4.0 / 18.0;
        if (name == "thickspace") return s.size * 5.0 / 18.0;
        if (name == "enspace") return s.size * 0.5;
        if (name == "quad") return s.size;
        if (name == "qquad") return s.size * 2.0;
        return s.size * 0.25;
    }

    MathBox large_glyph(std::string_view utf8, const Style& s, NodeId source) {
        const double scale = s.display && s.level == 0 ? 1.55 : 1.15;
        Style big{ s.size * scale, s.level, s.display };
        MathBox g = text(utf8, GlyphRole::LargeOperator, big, source);
        const double center = (g.ascent - g.descent) * 0.5;
        const double shift = center - axis(s);
        MathBox out;
        out.place(g, 0.0, shift);
        return out;
    }

    MathBox large_operator_glyph(const SymbolInfo& info, const Style& s, NodeId source) { return large_glyph(info.utf8, s, source); }

    MathBox symbol(NodeId id, const Style& s) {
        const SymbolInfo* info = m_doc.symbol(id);
        if (!info) return text(m_doc.command_text(id), GlyphRole::Error, s, id);
        if (info->cls == SymbolClass::Space) return gap(space_width(info->name(), s));
        if (info->cls == SymbolClass::LargeOperator) return large_operator_glyph(*info, s, id);
        return text(info->utf8, role_for(info->cls, m_opt.italic_greek_lowercase), s, id);
    }

    MathBox scripts(const MathBox& base, const MathBox* sup, const MathBox* sub, const Style& s) {
        MathBox out = base;
        const double kern = s.size * 0.05;
        double sup_up = 0.0, sub_down = 0.0;
        if (sup) sup_up = std::max(s.size * 0.38, base.ascent - sup->ascent * 0.45);
        if (sub) sub_down = std::max(s.size * 0.2, base.descent + sub->ascent * 0.2);
        if (sup && sub) {
            const double clearance = rule(s) * 4.0;
            const double gap_now = (sup_up - sup->descent) - (sub->ascent - sub_down);
            if (gap_now < clearance) {
                const double fix = clearance - gap_now;
                sub_down += fix * 0.5;
                sup_up += fix * 0.5;
            }
        }
        const double x = base.width + kern;
        if (sup) out.place(*sup, x, -sup_up);
        if (sub) out.place(*sub, x, sub_down);
        out.width = x + std::max(sup ? sup->width : 0.0, sub ? sub->width : 0.0) + kern;
        return out;
    }

    MathBox fraction(NodeId num_id, NodeId den_id, const Style& s, NodeId source) {
        const Style fs = fraction_style(s);
        const MathBox num = node(num_id, fs);
        const MathBox den = node(den_id, fs);
        const double r = rule(s);
        const double a = axis(s);
        const double clear = s.display && s.level == 0 ? r * 3.0 : r * 1.6;
        const double pad = s.size * 0.12;
        const double w = std::max(num.width, den.width) + 2.0 * pad;
        MathBox out;
        const double num_shift = a + r * 0.5 + clear + num.descent;
        const double den_shift = den.ascent + clear + r * 0.5 - a;
        out.place(num, (w - num.width) * 0.5, -num_shift);
        out.place(den, (w - den.width) * 0.5, den_shift);
        LayoutRule bar;
        bar.x = 0.0;
        bar.y = -(a + r * 0.5);
        bar.width = w;
        bar.height = r;
        bar.role = GlyphRole::Operator;
        bar.source = source;
        out.rules.push_back(bar);
        out.width = w + s.size * 0.06;
        return out;
    }

    MathBox radical(NodeId radicand, NodeId index, const Style& s, NodeId source) {
        const MathBox body = node(radicand, s);
        const double r = rule(s);
        const double clear = r + s.size * 0.1;
        const double top = body.ascent + clear + r;
        const double bottom = body.descent + s.size * 0.04;
        const double h = top + bottom;
        const double w = s.size * 0.55 + h * 0.08;
        MathBox idx;
        double shift = 0.0;
        if (index != kNoNode) {
            Style is = script_style(script_style(s));
            idx = node(index, is);
            shift = std::max(0.0, idx.width - w * 0.55);
        }
        MathBox out;
        LayoutPath sign;
        sign.thickness = r * 1.15;
        sign.role = GlyphRole::Operator;
        sign.source = source;
        sign.points.push_back(LayoutPoint{ shift + 0.0, -(-bottom + h * 0.42) });
        sign.points.push_back(LayoutPoint{ shift + w * 0.22, -(-bottom + h * 0.5) });
        sign.points.push_back(LayoutPoint{ shift + w * 0.5, bottom });
        sign.points.push_back(LayoutPoint{ shift + w, -top + r * 0.5 });
        sign.points.push_back(LayoutPoint{ shift + w + body.width + s.size * 0.1, -top + r * 0.5 });
        out.paths.push_back(sign);
        out.place(body, shift + w + s.size * 0.05, 0.0);
        if (index != kNoNode) out.place(idx, shift + w * 0.55 - idx.width, -(-bottom + h * 0.6) - idx.descent);
        out.width = shift + w + body.width + s.size * 0.15;
        if (top > out.ascent) out.ascent = top;
        if (bottom > out.descent) out.descent = bottom;
        return out;
    }

    MathBox operator_limits(const MathBox& op, const MathBox* up, const MathBox* lo, bool stacked, const Style& s) {
        if (!stacked) return scripts(op, up, lo, s);
        MathBox out;
        const double w = std::max(op.width, std::max(lo ? lo->width : 0.0, up ? up->width : 0.0));
        const double gap_v = s.size * 0.12;
        out.place(op, (w - op.width) * 0.5, 0.0);
        if (up) out.place(*up, (w - up->width) * 0.5, -(op.ascent + gap_v + up->descent));
        if (lo) out.place(*lo, (w - lo->width) * 0.5, op.descent + gap_v + lo->ascent);
        out.width = w;
        return out;
    }

    MathBox limit_lines(NodeId id, const Style& s) {
        if (!is_named(m_doc, id, names::Parens) || m_doc.node(id).child_count < 2) return node(id, s);
        std::vector<MathBox> lines;
        double widest = 0.0;
        for (NodeId c : m_doc.children(id)) {
            lines.push_back(node(c, s));
            widest = std::max(widest, lines.back().width);
        }
        MathBox out;
        double y = 0.0;
        bool first = true;
        for (const MathBox& line : lines) {
            if (!first) y = out.descent + s.size * 0.15 + line.ascent;
            out.place(line, (widest - line.width) * 0.5, y);
            first = false;
        }
        return out;
    }

    MathBox limit_relation(NodeId left, std::string_view glyph, NodeId right, const Style& s, NodeId source) {
        MathBox row = node(left, s);
        row.append(text(glyph, GlyphRole::Operator, s, source), space(s, 4));
        row.append(node(right, s), space(s, 4));
        return row;
    }

    MathBox differentials(NodeRange a, std::size_t from, const Style& s, NodeId source) {
        MathBox row;
        for (std::size_t i = from; i < a.size(); ++i) {
            row.append(text("d", GlyphRole::Function, s, source), i == from ? 0.0 : space(s, 3));
            row.append(node(a[i], s), space(s, 1));
        }
        return row;
    }

    static std::string_view integral_glyph(std::size_t signs) noexcept {
        switch (signs) {
            case 2: return "\xE2\x88\xAC";
            case 3: return "\xE2\x88\xAD";
            case 4: return "\xE2\xA8\x8C";
            default: return "\xE2\x88\xAB";
        }
    }

    MathBox operator_sign(const OperatorCall& op, std::string_view glyph, const MathBox* up, const MathBox* lo, const Style& s, NodeId source) {
        const bool textual = op.form == OperatorForm::Limit || op.form == OperatorForm::Extremum;
        const bool integral = op.form == OperatorForm::Integral || op.form == OperatorForm::ClosedIntegral || op.form == OperatorForm::IndefiniteIntegral;
        const MathBox g = textual ? text(glyph, GlyphRole::Function, s, source) : large_glyph(glyph, s, source);
        const bool stacked = s.display && s.level == 0 && (integral ? m_opt.integral_limits_above_below : m_opt.limits_above_below);
        return operator_limits(g, up, lo, stacked, s);
    }

    MathBox operator_call(const OperatorCall& op, NodeRange a, const Style& s, NodeId source) {
        const Style ls = script_style(s);
        const std::size_t n = a.size();
        std::size_t body = n;
        std::size_t diffs = n;
        MathBox out;
        const auto sign = [&](std::string_view glyph, const MathBox* up, const MathBox* lo) {
            out.append(operator_sign(op, glyph, up, lo, s, source), out.width > 0.0 ? space(s, 1) : 0.0);
        };
        switch (op.form) {
            case OperatorForm::Integral: {
                const std::size_t k = op.signs;
                if (k > 1 && (n == 2 * k + 1 || n == 3 * k + 1)) {
                    for (std::size_t i = 0; i < k; ++i) {
                        const MathBox lo = node(a[2 * i], ls);
                        const MathBox up = node(a[2 * i + 1], ls);
                        sign(integral_glyph(1), &up, &lo);
                    }
                    body = 2 * k;
                    diffs = 2 * k + 1;
                } else if (k == 1 && n >= 3) {
                    const MathBox lo = node(a[0], ls);
                    const MathBox up = node(a[1], ls);
                    sign(integral_glyph(1), &up, &lo);
                    body = 2;
                    diffs = 3;
                } else if (n >= 2) {
                    const MathBox lo = limit_lines(a[0], ls);
                    sign(integral_glyph(k), nullptr, &lo);
                    body = 1;
                    diffs = 2;
                } else {
                    sign(integral_glyph(k), nullptr, nullptr);
                    body = 0;
                }
                break;
            }
            case OperatorForm::ClosedIntegral: {
                if (n >= 2) {
                    const MathBox lo = limit_lines(a[0], ls);
                    sign(op.glyph, nullptr, &lo);
                    body = 1;
                    diffs = 2;
                } else {
                    sign(op.glyph, nullptr, nullptr);
                    body = 0;
                }
                break;
            }
            case OperatorForm::IndefiniteIntegral:
                sign(op.glyph, nullptr, nullptr);
                body = 0;
                diffs = 1;
                break;
            case OperatorForm::Series: {
                if (n >= 4) {
                    const MathBox lo = limit_relation(a[0], "=", a[1], ls, source);
                    const MathBox up = node(a[2], ls);
                    sign(op.glyph, &up, &lo);
                    body = 3;
                } else if (n == 3) {
                    const MathBox lo = limit_lines(a[0], ls);
                    const MathBox up = node(a[1], ls);
                    sign(op.glyph, &up, &lo);
                    body = 2;
                } else if (n == 2) {
                    const MathBox lo = limit_lines(a[0], ls);
                    sign(op.glyph, nullptr, &lo);
                    body = 1;
                } else {
                    sign(op.glyph, nullptr, nullptr);
                    body = 0;
                }
                break;
            }
            case OperatorForm::Limit:
            case OperatorForm::Extremum: {
                if (n >= 3) {
                    const MathBox lo = limit_relation(a[0], op.form == OperatorForm::Limit ? "\xE2\x86\x92" : "\xE2\x88\x88", a[1], ls, source);
                    sign(op.glyph, nullptr, &lo);
                    body = 2;
                } else if (n == 2) {
                    const MathBox lo = limit_lines(a[0], ls);
                    sign(op.glyph, nullptr, &lo);
                    body = 1;
                } else {
                    sign(op.glyph, nullptr, nullptr);
                    body = 0;
                }
                break;
            }
        }
        if (body < n) out.append(wrapped(a[body], precedence::Multiplicative, s), space(s, 2));
        if (diffs < n) out.append(differentials(a, diffs, s, source), space(s, 3));
        const std::size_t used = std::max(body + 1, diffs < n ? n : body + 1);
        for (std::size_t i = used; i < n; ++i) {
            out.append(text(",", GlyphRole::Punctuation, s));
            out.append(node(a[i], s), space(s, 3));
        }
        return out;
    }

    std::string_view relation_glyph(std::string_view nm) const noexcept {
        if (nm == names::Equals) return "=";
        if (nm == names::Less) return "<";
        if (nm == names::Greater) return ">";
        if (nm == names::LessEqual) return "\xE2\x89\xA4";
        if (nm == names::GreaterEqual) return "\xE2\x89\xA5";
        return "\xE2\x89\xA0";
    }

    bool starts_with_number(NodeId id) const noexcept {
        const Node& n = m_doc.node(id);
        if (n.is(NodeKind::Number)) return !is_negative_like(m_doc, id);
        if (!n.is(NodeKind::Call) || n.has(node_flags::Command) || n.child_count == 0) return false;
        const int p = math_precedence(m_doc, id);
        if (p == precedence::Atom || p == precedence::Unary) return false;
        const std::string_view nm = m_doc.name(id);
        if (nm == names::Fraction) return false;
        if (nm == names::Relation || nm == names::Operator) return starts_with_number(m_doc.children(id)[1]);
        return starts_with_number(m_doc.children(id)[0]);
    }

    MathBox binary(NodeRange a, std::size_t from, std::string_view glyph, double gap_each, int p, const Style& s, NodeId source, bool symbolic_from_node = false) {
        MathBox row = wrapped(a[from], p, s);
        const MathBox op = symbolic_from_node ? node(a[0], s) : text(glyph, GlyphRole::Operator, s, source);
        for (std::size_t i = from + 1; i < a.size(); ++i) {
            row.append(op, gap_each);
            row.append(wrapped(a[i], p + 1, s), gap_each);
        }
        return row;
    }

    MathBox call(NodeId id, const Style& s) {
        const Node& n = m_doc.node(id);
        const NodeRange a = m_doc.children(id);
        const bool command = n.has(node_flags::Command);
        const std::string_view nm = command ? m_doc.name(id) : canonical_call(m_doc.name(id));
        const int p = math_precedence(m_doc, id);
        if (!command && p != precedence::Atom) {
            const double med = space(s, 4);
            const double thick = space(s, 5);
            if (nm == names::Add) return binary(a, 0, "+", med, p, s, id);
            if (nm == names::Subtract) return binary(a, 0, "\xE2\x88\x92", med, p, s, id);
            if (nm == names::Multiply) return binary(a, 0, "\xE2\x8B\x85", med, p, s, id);
            if (nm == names::ImplicitMultiply) {
                MathBox row = wrapped(a[0], p, s);
                for (std::size_t i = 1; i < a.size(); ++i) {
                    const bool force = is_negative_like(m_doc, a[i]);
                    const bool parenthesized = force || math_precedence(m_doc, a[i]) < p + 1;
                    const bool dot = !parenthesized && starts_with_number(a[i]);
                    if (dot) row.append(text("\xE2\x8B\x85", GlyphRole::Operator, s, id), space(s, 3));
                    row.append(wrapped(a[i], p + 1, s, force), dot ? space(s, 3) : space(s, 1));
                }
                return row;
            }
            if (nm == names::Fraction) return fraction(a[0], a[1], s, id);
            if (nm == names::Power || nm == names::Subscript) {
                const bool is_power = nm == names::Power;
                NodeId base_id = a[0];
                NodeId sub_id = is_power ? kNoNode : a[1];
                NodeId sup_id = is_power ? a[1] : kNoNode;
                if (is_power && is_named(m_doc, base_id, names::Subscript) && m_doc.node(base_id).child_count == 2) {
                    sub_id = m_doc.children(base_id)[1];
                    base_id = m_doc.children(base_id)[0];
                }
                const MathBox base = wrapped(base_id, precedence::Power + 1, s);
                const Style ss = script_style(s);
                MathBox sup, sub;
                if (sup_id != kNoNode) sup = node(sup_id, ss);
                if (sub_id != kNoNode) sub = node(sub_id, ss);
                return scripts(base, sup_id != kNoNode ? &sup : nullptr, sub_id != kNoNode ? &sub : nullptr, s);
            }
            if (nm == names::Factorial) {
                MathBox row = wrapped(a[0], p, s);
                row.append(text("!", GlyphRole::Operator, s, id));
                return row;
            }
            if (nm == names::Negate) {
                MathBox row = text("\xE2\x88\x92", GlyphRole::Operator, s, id);
                row.append(wrapped(a[0], p, s));
                return row;
            }
            if (nm == names::Relation || nm == names::Operator) return binary(a, 1, std::string_view(), nm == names::Relation ? thick : med, p, s, id, true);
            return binary(a, 0, relation_glyph(nm), thick, p, s, id);
        }
        if (!command && nm == names::Parens) return delimited(joined(a, s, ","), "(", ")", s, id);
        const std::string_view fn = command ? canonical_call(nm) : nm;
        if (a.size() == 1 && fn == names::Sqrt) return radical(a[0], kNoNode, s, id);
        if (a.size() == 2 && fn == names::Root) return radical(a[1], a[0], s, id);
        if (a.size() == 1 && fn == names::Abs) return delimited(node(a[0], s), "|", "|", s, id);
        if (a.size() == 1 && fn == names::Norm) return delimited(node(a[0], s), "\xE2\x80\x96", "\xE2\x80\x96", s, id);
        if (a.size() == 1 && fn == names::Floor) return delimited(node(a[0], s), "\xE2\x8C\x8A", "\xE2\x8C\x8B", s, id);
        if (a.size() == 1 && fn == names::Ceil) return delimited(node(a[0], s), "\xE2\x8C\x88", "\xE2\x8C\x89", s, id);
        if (a.size() == 2 && fn == names::Fraction) return fraction(a[0], a[1], s, id);
        if (const OperatorCall* op = find_operator_call(nm)) return operator_call(*op, a, s, id);
        MathBox head;
        if (command) {
            const std::uint32_t index = find_symbol(nm);
            if (index != kNoSymbol && symbol_info(index).cls == SymbolClass::LargeOperator) {
                MathBox row = large_operator_glyph(symbol_info(index), s, id);
                if (!a.empty()) row.append(joined(a, s, ","), space(s, 2));
                return row;
            }
            if (index != kNoSymbol) head = text(symbol_info(index).utf8, role_for(symbol_info(index).cls, m_opt.italic_greek_lowercase), s, id);
            else head = text(m_doc.command_text(id), GlyphRole::Error, s, id);
        } else {
            head = text(nm, nm.size() == 1 ? GlyphRole::Variable : GlyphRole::Function, s, id);
        }
        head.append(delimited(joined(a, s, ","), "(", ")", s, id), space(s, 1));
        return head;
    }

    MathBox node(NodeId id, const Style& s) {
        MathBox b;
        switch (m_doc.kind(id)) {
            case NodeKind::Number: b = number(id, s); break;
            case NodeKind::String: b = string(id, s); break;
            case NodeKind::Symbol: b = symbol(id, s); break;
            case NodeKind::Negate: {
                b = text("\xE2\x88\x92", GlyphRole::Operator, s, id);
                b.append(wrapped(m_doc.children(id)[0], precedence::Unary, s));
                break;
            }
            case NodeKind::Error: {
                std::string raw(m_doc.source_text(id));
                for (char& c : raw) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                b = text(raw, GlyphRole::Error, s, id);
                break;
            }
            case NodeKind::Call: b = call(id, s); break;
            case NodeKind::TextRun: b = text(m_doc.decoded_text(id), GlyphRole::Text, s, id); break;
            case NodeKind::Document: b = document(s); break;
        }
        b.tag(id);
        return b;
    }

    const Document& m_doc;
    FontMetrics&    m_metrics;
    LayoutOptions   m_opt;
};

inline MathBox layout_math(const Document& doc, FontMetrics& metrics, const LayoutOptions& options = LayoutOptions()) { return LayoutEngine(doc, metrics, options).layout(); }

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_LAYOUT_ENGINE_HPP
