#ifndef FIZMO_MATHTEXT_FONT_METRICS_HPP
#define FIZMO_MATHTEXT_FONT_METRICS_HPP

#include <string_view>
#include "math_box.hpp"
#include "../core/utf8.hpp"

namespace fizmo {
namespace mathtext {

struct TextExtent {
    double advance = 0.0;
    double ascent = 0.0;
    double descent = 0.0;
};

class FontMetrics {
public:
    virtual ~FontMetrics() = default;
    virtual TextExtent measure(std::string_view utf8, double size, GlyphRole role) = 0;
};

class ApproximateMetrics : public FontMetrics {
public:
    TextExtent measure(std::string_view utf8, double size, GlyphRole) override {
        TextExtent e;
        double asc = 0.0, desc = 0.0;
        for (std::size_t i = 0; i < utf8.size(); i = utf8::advance(utf8, i)) {
            const unsigned char c = static_cast<unsigned char>(utf8[i]);
            double w = 0.55, a = 0.68, d = 0.0;
            if (c >= '0' && c <= '9') { w = 0.5; a = 0.66; }
            else if (c >= 'a' && c <= 'z') {
                w = (c == 'i' || c == 'l' || c == 'j' || c == 't' || c == 'f') ? 0.3 : (c == 'm' || c == 'w') ? 0.8 : 0.5;
                a = (c == 'b' || c == 'd' || c == 'f' || c == 'h' || c == 'k' || c == 'l' || c == 't') ? 0.7 : (c == 'i' || c == 'j') ? 0.66 : 0.45;
                d = (c == 'g' || c == 'j' || c == 'p' || c == 'q' || c == 'y') ? 0.2 : 0.0;
            } else if (c >= 'A' && c <= 'Z') { w = 0.7; a = 0.68; }
            else if (c == ' ') { w = 0.25; a = 0.0; }
            else if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|') { w = 0.39; a = 0.75; d = 0.25; }
            else if (c == '+' || c == '=' || c == '<' || c == '>') { w = 0.78; a = 0.58; d = 0.08; }
            else if (c == ',' || c == '.') { w = 0.28; a = 0.1; d = c == ',' ? 0.19 : 0.0; }
            else if (c == '!') { w = 0.28; a = 0.7; }
            else if (c >= 0x80) { w = 0.7; a = 0.7; d = 0.15; }
            e.advance += w * size;
            if (a * size > asc) asc = a * size;
            if (d * size > desc) desc = d * size;
        }
        e.ascent = asc;
        e.descent = desc;
        return e;
    }
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_FONT_METRICS_HPP
