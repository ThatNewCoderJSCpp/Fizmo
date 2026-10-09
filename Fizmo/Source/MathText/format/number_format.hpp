#ifndef FIZMO_MATHTEXT_NUMBER_FORMAT_HPP
#define FIZMO_MATHTEXT_NUMBER_FORMAT_HPP

#include <string>
#include <string_view>
#include "../parser/document.hpp"

namespace fizmo {
namespace mathtext {

enum class NumberStyle : std::uint8_t { AsWritten, LowerE, UpperE, TimesTen };

struct NumberParts {
    bool             negative = false;
    bool             has_exponent = false;
    char             marker = 0;
    std::string_view mantissa;
    std::string_view integer_digits;
    std::string_view fraction_digits;
    bool             has_point = false;
    bool             exponent_negative = false;
    std::string_view exponent_digits;
};

inline std::string_view strip_leading_zeros(std::string_view digits) noexcept {
    std::size_t i = 0;
    while (i + 1 < digits.size() && digits[i] == '0') ++i;
    return digits.substr(i);
}

inline NumberParts number_parts(const Document& doc, NodeId id) noexcept {
    NumberParts p;
    const Node& n = doc.node(id);
    if (!n.is(NodeKind::Number)) return p;
    std::string_view m = doc.text(n.name);
    if (!m.empty() && (m[0] == '-' || m[0] == '+')) {
        p.negative = m[0] == '-';
        m.remove_prefix(1);
    }
    if (n.has(node_flags::Negative)) p.negative = !p.negative;
    p.mantissa = m;
    const std::size_t dot = m.find('.');
    p.has_point = dot != std::string_view::npos;
    p.integer_digits = p.has_point ? m.substr(0, dot) : m;
    p.fraction_digits = p.has_point ? m.substr(dot + 1) : std::string_view();
    p.marker = n.marker;
    p.has_exponent = n.marker != 0;
    if (p.has_exponent) {
        std::string_view e = doc.text(n.exponent);
        if (!e.empty() && (e[0] == '-' || e[0] == '+')) {
            p.exponent_negative = e[0] == '-';
            e.remove_prefix(1);
        }
        p.exponent_digits = strip_leading_zeros(e);
    }
    return p;
}

inline std::string format_number(const Document& doc, NodeId id, NumberStyle style, std::string_view times_ten = "*10^") {
    const Node& n = doc.node(id);
    if (!n.is(NodeKind::Number)) return std::string(doc.source_text(id));
    const std::string_view mantissa = doc.text(n.name);
    const std::string_view exponent = doc.text(n.exponent);
    std::string out;
    if (n.has(node_flags::Negative)) out.push_back('-');
    out.append(mantissa.data(), mantissa.size());
    if (!n.marker) return out;
    if (style == NumberStyle::AsWritten) {
        out.push_back(n.marker);
        out.append(exponent.data(), exponent.size());
        return out;
    }
    if (style == NumberStyle::TimesTen) {
        const NumberParts p = number_parts(doc, id);
        out.append(times_ten.data(), times_ten.size());
        if (p.exponent_negative && p.exponent_digits != "0") out.push_back('-');
        out.append(p.exponent_digits.data(), p.exponent_digits.size());
        return out;
    }
    out.push_back(style == NumberStyle::LowerE ? 'e' : 'E');
    out.append(exponent.data(), exponent.size());
    return out;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_NUMBER_FORMAT_HPP
