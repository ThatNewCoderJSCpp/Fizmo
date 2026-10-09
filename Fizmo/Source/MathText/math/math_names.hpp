#ifndef FIZMO_MATHTEXT_MATH_NAMES_HPP
#define FIZMO_MATHTEXT_MATH_NAMES_HPP

#include <string_view>
#include "../core/names.hpp"

namespace fizmo {
namespace mathtext {
namespace precedence {

inline constexpr int Relation = 10;
inline constexpr int Additive = 20;
inline constexpr int Multiplicative = 30;
inline constexpr int Implicit = 35;
inline constexpr int Unary = 40;
inline constexpr int Power = 50;
inline constexpr int Postfix = 60;
inline constexpr int Atom = 100;

} // namespace precedence

inline bool is_additive_symbol(std::string_view name) noexcept {
    static constexpr std::string_view additive[] = { "pm", "mp", "oplus", "ominus", "cup", "Cup", "sqcup", "uplus", "setminus", "smallsetminus", "vee", "lor", "curlyvee", "veebar", "dotplus", "dotminus", "boxplus", "boxminus", "circleddash" };
    for (std::string_view a : additive) if (a == name) return true;
    return false;
}

inline std::string_view canonical_relation(std::string_view symbol_name) noexcept {
    if (symbol_name == "leq" || symbol_name == "le") return names::LessEqual;
    if (symbol_name == "geq" || symbol_name == "ge") return names::GreaterEqual;
    if (symbol_name == "neq" || symbol_name == "ne") return names::NotEqual;
    if (symbol_name == "lt") return names::Less;
    if (symbol_name == "gt") return names::Greater;
    return std::string_view();
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_NAMES_HPP
