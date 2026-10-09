#ifndef FIZMO_MATHTEXT_NAMES_HPP
#define FIZMO_MATHTEXT_NAMES_HPP

#include <cstdint>
#include <string_view>
#include "spelling.hpp"
#include "symbols/symbol_types.hpp"

namespace fizmo {
namespace mathtext {
namespace names {

inline constexpr std::string_view Add = "add";
inline constexpr std::string_view Subtract = "subtract";
inline constexpr std::string_view Multiply = "multiply";
inline constexpr std::string_view ImplicitMultiply = "implicit_multiply";
inline constexpr std::string_view Fraction = "fraction";
inline constexpr std::string_view Power = "power";
inline constexpr std::string_view Subscript = "subscript";
inline constexpr std::string_view Factorial = "factorial";
inline constexpr std::string_view Negate = "negate";
inline constexpr std::string_view Parens = "parens";
inline constexpr std::string_view Equals = "equals";
inline constexpr std::string_view Less = "less";
inline constexpr std::string_view Greater = "greater";
inline constexpr std::string_view LessEqual = "less_equal";
inline constexpr std::string_view GreaterEqual = "greater_equal";
inline constexpr std::string_view NotEqual = "not_equal";
inline constexpr std::string_view Relation = "relation";
inline constexpr std::string_view Operator = "operator";
inline constexpr std::string_view Sqrt = "sqrt";
inline constexpr std::string_view Root = "root";
inline constexpr std::string_view Abs = "abs";
inline constexpr std::string_view Norm = "norm";
inline constexpr std::string_view Floor = "floor";
inline constexpr std::string_view Ceil = "ceil";
inline constexpr std::string_view DefiniteIntegral = "definite_integral";
inline constexpr std::string_view IndefiniteIntegral = "indefinite_integral";
inline constexpr std::string_view DoubleIntegral = "double_integral";
inline constexpr std::string_view TripleIntegral = "triple_integral";
inline constexpr std::string_view QuadrupleIntegral = "quadruple_integral";
inline constexpr std::string_view ContourIntegral = "contour_integral";
inline constexpr std::string_view SurfaceIntegral = "surface_integral";
inline constexpr std::string_view VolumeIntegral = "volume_integral";
inline constexpr std::string_view Sum = "Sum";
inline constexpr std::string_view Product = "Product";
inline constexpr std::string_view Coproduct = "Coproduct";
inline constexpr std::string_view Union = "Union";
inline constexpr std::string_view Intersection = "Intersection";
inline constexpr std::string_view Limit = "Limit";
inline constexpr std::string_view LimitSuperior = "Limit_superior";
inline constexpr std::string_view LimitInferior = "Limit_inferior";
inline constexpr std::string_view Supremum = "Supremum";
inline constexpr std::string_view Infimum = "Infimum";
inline constexpr std::string_view Maximum = "Maximum";
inline constexpr std::string_view Minimum = "Minimum";

} // namespace names

struct CallSpellings {
    std::string_view canonical;
    SymbolNames      spellings;
};

inline constexpr CallSpellings kCallSpellings[] = {
    { names::Add, { "add", "plus" } },
    { names::Subtract, { "subtract", "minus", "difference" } },
    { names::Multiply, { "multiply", "times", "mul", "product_of" } },
    { names::ImplicitMultiply, { "implicit_multiply", "juxtapose" } },
    { names::Fraction, { "fraction", "frac", "dfrac", "tfrac", "over", "divide" } },
    { names::Power, { "power", "pow", "superscript", "sup" } },
    { names::Subscript, { "subscript", "sub" } },
    { names::Factorial, { "factorial", "fact" } },
    { names::Negate, { "negate", "negative" } },
    { names::Parens, { "parens", "paren", "group" } },
    { names::Equals, { "equals", "equal", "eq" } },
    { names::Less, { "less", "lt" } },
    { names::Greater, { "greater", "gt" } },
    { names::LessEqual, { "less_equal", "less_or_equal", "leq", "le" } },
    { names::GreaterEqual, { "greater_equal", "greater_or_equal", "geq", "ge" } },
    { names::NotEqual, { "not_equal", "neq", "ne" } },
    { names::Relation, { "relation", "rel" } },
    { names::Operator, { "operator", "op", "binop" } },
    { names::Sqrt, { "sqrt", "surd", "square_root" } },
    { names::Root, { "root", "nth_root", "nthroot" } },
    { names::Abs, { "abs", "absolute", "absolute_value" } },
    { names::Norm, { "norm" } },
    { names::Floor, { "floor" } },
    { names::Ceil, { "ceil", "ceiling" } },
    { names::DefiniteIntegral, { "definite_integral", "defint" } },
    { names::IndefiniteIntegral, { "indefinite_integral", "antiderivative" } },
    { names::DoubleIntegral, { "double_integral" } },
    { names::TripleIntegral, { "triple_integral" } },
    { names::QuadrupleIntegral, { "quadruple_integral" } },
    { names::ContourIntegral, { "contour_integral", "closed_integral", "line_integral" } },
    { names::SurfaceIntegral, { "surface_integral" } },
    { names::VolumeIntegral, { "volume_integral" } },
    { names::Sum, { "Sum", "Summation" } },
    { names::Product, { "Product", "Prod" } },
    { names::Coproduct, { "Coproduct" } },
    { names::Union, { "Union", "Big_union" } },
    { names::Intersection, { "Intersection", "Big_intersection" } },
    { names::Limit, { "Limit", "Lim" } },
    { names::LimitSuperior, { "Limit_superior", "Limsup", "Lim_sup" } },
    { names::LimitInferior, { "Limit_inferior", "Liminf", "Lim_inf" } },
    { names::Supremum, { "Supremum", "Sup" } },
    { names::Infimum, { "Infimum", "Inf" } },
    { names::Maximum, { "Maximum", "Max" } },
    { names::Minimum, { "Minimum", "Min" } },
};

enum class OperatorForm : std::uint8_t { Integral, ClosedIntegral, IndefiniteIntegral, Series, Limit, Extremum };

struct OperatorCall {
    std::string_view canonical;
    OperatorForm     form;
    const char*      glyph;
    std::uint8_t     signs;
};

inline constexpr OperatorCall kOperatorCalls[] = {
    { names::DefiniteIntegral, OperatorForm::Integral, "\xE2\x88\xAB", 1 },
    { names::DoubleIntegral, OperatorForm::Integral, "\xE2\x88\xAB", 2 },
    { names::TripleIntegral, OperatorForm::Integral, "\xE2\x88\xAB", 3 },
    { names::QuadrupleIntegral, OperatorForm::Integral, "\xE2\x88\xAB", 4 },
    { names::IndefiniteIntegral, OperatorForm::IndefiniteIntegral, "\xE2\x88\xAB", 1 },
    { names::ContourIntegral, OperatorForm::ClosedIntegral, "\xE2\x88\xAE", 1 },
    { names::SurfaceIntegral, OperatorForm::ClosedIntegral, "\xE2\x88\xAF", 1 },
    { names::VolumeIntegral, OperatorForm::ClosedIntegral, "\xE2\x88\xB0", 1 },
    { names::Sum, OperatorForm::Series, "\xE2\x88\x91", 1 },
    { names::Product, OperatorForm::Series, "\xE2\x88\x8F", 1 },
    { names::Coproduct, OperatorForm::Series, "\xE2\x88\x90", 1 },
    { names::Union, OperatorForm::Series, "\xE2\x8B\x83", 1 },
    { names::Intersection, OperatorForm::Series, "\xE2\x8B\x82", 1 },
    { names::Limit, OperatorForm::Limit, "lim", 1 },
    { names::LimitSuperior, OperatorForm::Limit, "lim sup", 1 },
    { names::LimitInferior, OperatorForm::Limit, "lim inf", 1 },
    { names::Supremum, OperatorForm::Extremum, "sup", 1 },
    { names::Infimum, OperatorForm::Extremum, "inf", 1 },
    { names::Maximum, OperatorForm::Extremum, "max", 1 },
    { names::Minimum, OperatorForm::Extremum, "min", 1 },
};

inline const CallSpellings* find_call_spellings(std::string_view spelling) noexcept {
    for (const CallSpellings& c : kCallSpellings)
        for (const char* s : c.spellings) if (spelling_equal(spelling, s)) return &c;
    return nullptr;
}

inline std::string_view canonical_call(std::string_view spelling) noexcept {
    const CallSpellings* c = find_call_spellings(spelling);
    return c ? c->canonical : spelling;
}

inline bool is_call_spelling(std::string_view spelling) noexcept { return find_call_spellings(spelling) != nullptr; }

inline const OperatorCall* find_operator_call(std::string_view spelling) noexcept {
    const std::string_view canonical = canonical_call(spelling);
    for (const OperatorCall& op : kOperatorCalls) if (op.canonical == canonical) return &op;
    return nullptr;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_NAMES_HPP
