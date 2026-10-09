#ifndef FIZMO_MATHTEXT_SYMBOLS_LARGE_OPERATORS_HPP
#define FIZMO_MATHTEXT_SYMBOLS_LARGE_OPERATORS_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kLargeOperatorSymbols[] = {
    { { "sum", "summation", "Sum", "Summation" }, "\xE2\x88\x91", 0x2211, SymbolClass::LargeOperator },
    { { "prod", "product", "Product", "Prod" }, "\xE2\x88\x8F", 0x220F, SymbolClass::LargeOperator },
    { { "coprod", "coproduct", "Coproduct" }, "\xE2\x88\x90", 0x2210, SymbolClass::LargeOperator },
    { { "int", "smallint", "integral", "definite_integral", "defint", "indefinite_integral", "antiderivative" }, "\xE2\x88\xAB", 0x222B, SymbolClass::LargeOperator },
    { { "iint", "double_integral" }, "\xE2\x88\xAC", 0x222C, SymbolClass::LargeOperator },
    { { "iiint", "triple_integral" }, "\xE2\x88\xAD", 0x222D, SymbolClass::LargeOperator },
    { { "iiiint", "quadruple_integral" }, "\xE2\xA8\x8C", 0x2A0C, SymbolClass::LargeOperator },
    { { "oint", "contour_integral", "closed_integral", "line_integral" }, "\xE2\x88\xAE", 0x222E, SymbolClass::LargeOperator },
    { { "oiint", "surface_integral" }, "\xE2\x88\xAF", 0x222F, SymbolClass::LargeOperator },
    { { "oiiint", "volume_integral" }, "\xE2\x88\xB0", 0x2230, SymbolClass::LargeOperator },
    { { "bigcup", "big_union", "Union", "Big_union" }, "\xE2\x8B\x83", 0x22C3, SymbolClass::LargeOperator },
    { { "bigcap", "big_intersection", "Intersection", "Big_intersection" }, "\xE2\x8B\x82", 0x22C2, SymbolClass::LargeOperator },
    { { "bigsqcup" }, "\xE2\xA8\x86", 0x2A06, SymbolClass::LargeOperator },
    { { "bigvee" }, "\xE2\x8B\x81", 0x22C1, SymbolClass::LargeOperator },
    { { "bigwedge" }, "\xE2\x8B\x80", 0x22C0, SymbolClass::LargeOperator },
    { { "bigodot" }, "\xE2\xA8\x80", 0x2A00, SymbolClass::LargeOperator },
    { { "bigoplus", "big_direct_sum" }, "\xE2\xA8\x81", 0x2A01, SymbolClass::LargeOperator },
    { { "bigotimes", "big_tensor_product" }, "\xE2\xA8\x82", 0x2A02, SymbolClass::LargeOperator },
    { { "biguplus" }, "\xE2\xA8\x84", 0x2A04, SymbolClass::LargeOperator },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_LARGE_OPERATORS_HPP
