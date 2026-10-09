#ifndef FIZMO_MATHTEXT_MATH_BOX_HPP
#define FIZMO_MATHTEXT_MATH_BOX_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "../parser/node.hpp"

namespace fizmo {
namespace mathtext {

#define FIZMO_MATHTEXT_GLYPH_ROLES(X) \
    X(Variable) \
    X(Number) \
    X(Text) \
    X(Function) \
    X(Symbol) \
    X(Operator) \
    X(LargeOperator) \
    X(Delimiter) \
    X(Punctuation) \
    X(Error)

enum class GlyphRole : std::uint8_t {
#define FIZMO_MATHTEXT_X(name) name,
    FIZMO_MATHTEXT_GLYPH_ROLES(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
};

inline constexpr const char* glyph_role_name(GlyphRole r) noexcept {
    switch (r) {
#define FIZMO_MATHTEXT_X(name) case GlyphRole::name: return #name;
        FIZMO_MATHTEXT_GLYPH_ROLES(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "Text";
}

struct LayoutPoint {
    double x = 0.0;
    double y = 0.0;
};

struct LayoutText {
    double      x = 0.0;
    double      baseline = 0.0;
    double      size = 0.0;
    GlyphRole   role = GlyphRole::Text;
    std::string text;
    NodeId      source = kNoNode;
};

struct LayoutRule {
    double    x = 0.0;
    double    y = 0.0;
    double    width = 0.0;
    double    height = 0.0;
    GlyphRole role = GlyphRole::Symbol;
    NodeId    source = kNoNode;
};

struct LayoutPath {
    std::vector<LayoutPoint> points;
    double                   thickness = 1.0;
    bool                     closed = false;
    bool                     filled = false;
    GlyphRole                role = GlyphRole::Delimiter;
    NodeId                   source = kNoNode;
};

class MathBox {
public:
    double                  width = 0.0;
    double                  ascent = 0.0;
    double                  descent = 0.0;
    std::vector<LayoutText> texts;
    std::vector<LayoutRule> rules;
    std::vector<LayoutPath> paths;

    double height() const noexcept { return ascent + descent; }
    bool empty() const noexcept { return texts.empty() && rules.empty() && paths.empty(); }

    void translate(double dx, double dy) {
        for (LayoutText& t : texts) { t.x += dx; t.baseline += dy; }
        for (LayoutRule& r : rules) { r.x += dx; r.y += dy; }
        for (LayoutPath& p : paths) for (LayoutPoint& q : p.points) { q.x += dx; q.y += dy; }
    }

    void place(const MathBox& child, double dx, double dy) {
        for (LayoutText t : child.texts) { t.x += dx; t.baseline += dy; texts.push_back(std::move(t)); }
        for (LayoutRule r : child.rules) { r.x += dx; r.y += dy; rules.push_back(r); }
        for (LayoutPath p : child.paths) {
            for (LayoutPoint& q : p.points) { q.x += dx; q.y += dy; }
            paths.push_back(std::move(p));
        }
        if (child.ascent - dy > ascent) ascent = child.ascent - dy;
        if (child.descent + dy > descent) descent = child.descent + dy;
        if (dx + child.width > width) width = dx + child.width;
    }

    void append(const MathBox& child, double gap_before = 0.0) { place(child, width + gap_before, 0.0); }

    void tag(NodeId id) {
        for (LayoutText& t : texts) if (t.source == kNoNode) t.source = id;
        for (LayoutRule& r : rules) if (r.source == kNoNode) r.source = id;
        for (LayoutPath& p : paths) if (p.source == kNoNode) p.source = id;
    }
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_BOX_HPP
