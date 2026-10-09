#ifndef FIZMO_MATHTEXT_SYMBOLS_OPERATORS_HPP
#define FIZMO_MATHTEXT_SYMBOLS_OPERATORS_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kBinaryOperatorSymbols[] = {
    { { "pm", "plus_minus" }, "\xC2\xB1", 0x00B1, SymbolClass::BinaryOperator },
    { { "mp", "minus_plus" }, "\xE2\x88\x93", 0x2213, SymbolClass::BinaryOperator },
    { { "times", "cross" }, "\xC3\x97", 0x00D7, SymbolClass::BinaryOperator },
    { { "div", "division" }, "\xC3\xB7", 0x00F7, SymbolClass::BinaryOperator },
    { { "cdot", "center_dot" }, "\xE2\x8B\x85", 0x22C5, SymbolClass::BinaryOperator },
    { { "ast", "asterisk" }, "\xE2\x88\x97", 0x2217, SymbolClass::BinaryOperator },
    { { "star" }, "\xE2\x8B\x86", 0x22C6, SymbolClass::BinaryOperator },
    { { "circ", "composition", "compose" }, "\xE2\x88\x98", 0x2218, SymbolClass::BinaryOperator },
    { { "bullet", "bullet_point" }, "\xE2\x88\x99", 0x2219, SymbolClass::BinaryOperator },
    { { "oplus", "direct_sum" }, "\xE2\x8A\x95", 0x2295, SymbolClass::BinaryOperator },
    { { "ominus" }, "\xE2\x8A\x96", 0x2296, SymbolClass::BinaryOperator },
    { { "otimes", "tensor_product" }, "\xE2\x8A\x97", 0x2297, SymbolClass::BinaryOperator },
    { { "oslash" }, "\xE2\x8A\x98", 0x2298, SymbolClass::BinaryOperator },
    { { "odot", "hadamard" }, "\xE2\x8A\x99", 0x2299, SymbolClass::BinaryOperator },
    { { "circledast" }, "\xE2\x8A\x9B", 0x229B, SymbolClass::BinaryOperator },
    { { "circledcirc" }, "\xE2\x8A\x9A", 0x229A, SymbolClass::BinaryOperator },
    { { "circleddash" }, "\xE2\x8A\x9D", 0x229D, SymbolClass::BinaryOperator },
    { { "bigcirc" }, "\xE2\x97\xAF", 0x25EF, SymbolClass::BinaryOperator },
    { { "cap", "intersection" }, "\xE2\x88\xA9", 0x2229, SymbolClass::BinaryOperator },
    { { "cup", "union" }, "\xE2\x88\xAA", 0x222A, SymbolClass::BinaryOperator },
    { { "Cap" }, "\xE2\x8B\x92", 0x22D2, SymbolClass::BinaryOperator },
    { { "Cup" }, "\xE2\x8B\x93", 0x22D3, SymbolClass::BinaryOperator },
    { { "uplus" }, "\xE2\x8A\x8E", 0x228E, SymbolClass::BinaryOperator },
    { { "sqcap" }, "\xE2\x8A\x93", 0x2293, SymbolClass::BinaryOperator },
    { { "sqcup" }, "\xE2\x8A\x94", 0x2294, SymbolClass::BinaryOperator },
    { { "setminus", "smallsetminus", "set_minus" }, "\xE2\x88\x96", 0x2216, SymbolClass::BinaryOperator },
    { { "wedge", "land", "logical_and" }, "\xE2\x88\xA7", 0x2227, SymbolClass::BinaryOperator },
    { { "vee", "lor", "logical_or" }, "\xE2\x88\xA8", 0x2228, SymbolClass::BinaryOperator },
    { { "curlywedge" }, "\xE2\x8B\x8F", 0x22CF, SymbolClass::BinaryOperator },
    { { "curlyvee" }, "\xE2\x8B\x8E", 0x22CE, SymbolClass::BinaryOperator },
    { { "barwedge" }, "\xE2\x8A\xBC", 0x22BC, SymbolClass::BinaryOperator },
    { { "veebar" }, "\xE2\x8A\xBB", 0x22BB, SymbolClass::BinaryOperator },
    { { "doublebarwedge" }, "\xE2\xA9\x9E", 0x2A5E, SymbolClass::BinaryOperator },
    { { "wr" }, "\xE2\x89\x80", 0x2240, SymbolClass::BinaryOperator },
    { { "diamond" }, "\xE2\x8B\x84", 0x22C4, SymbolClass::BinaryOperator },
    { { "bigtriangleup" }, "\xE2\x96\xB3", 0x25B3, SymbolClass::BinaryOperator },
    { { "bigtriangledown" }, "\xE2\x96\xBD", 0x25BD, SymbolClass::BinaryOperator },
    { { "triangleleft" }, "\xE2\x97\x81", 0x25C1, SymbolClass::BinaryOperator },
    { { "triangleright" }, "\xE2\x96\xB7", 0x25B7, SymbolClass::BinaryOperator },
    { { "lhd" }, "\xE2\x8A\xB2", 0x22B2, SymbolClass::BinaryOperator },
    { { "rhd" }, "\xE2\x8A\xB3", 0x22B3, SymbolClass::BinaryOperator },
    { { "unlhd" }, "\xE2\x8A\xB4", 0x22B4, SymbolClass::BinaryOperator },
    { { "unrhd" }, "\xE2\x8A\xB5", 0x22B5, SymbolClass::BinaryOperator },
    { { "dagger" }, "\xE2\x80\xA0", 0x2020, SymbolClass::BinaryOperator },
    { { "ddagger" }, "\xE2\x80\xA1", 0x2021, SymbolClass::BinaryOperator },
    { { "amalg" }, "\xE2\xA8\xBF", 0x2A3F, SymbolClass::BinaryOperator },
    { { "dotplus" }, "\xE2\x88\x94", 0x2214, SymbolClass::BinaryOperator },
    { { "dotminus" }, "\xE2\x88\xB8", 0x2238, SymbolClass::BinaryOperator },
    { { "ltimes" }, "\xE2\x8B\x89", 0x22C9, SymbolClass::BinaryOperator },
    { { "rtimes" }, "\xE2\x8B\x8A", 0x22CA, SymbolClass::BinaryOperator },
    { { "leftthreetimes" }, "\xE2\x8B\x8B", 0x22CB, SymbolClass::BinaryOperator },
    { { "rightthreetimes" }, "\xE2\x8B\x8C", 0x22CC, SymbolClass::BinaryOperator },
    { { "boxplus" }, "\xE2\x8A\x9E", 0x229E, SymbolClass::BinaryOperator },
    { { "boxminus" }, "\xE2\x8A\x9F", 0x229F, SymbolClass::BinaryOperator },
    { { "boxtimes" }, "\xE2\x8A\xA0", 0x22A0, SymbolClass::BinaryOperator },
    { { "boxdot" }, "\xE2\x8A\xA1", 0x22A1, SymbolClass::BinaryOperator },
    { { "intercal" }, "\xE2\x8A\xBA", 0x22BA, SymbolClass::BinaryOperator },
    { { "divideontimes" }, "\xE2\x8B\x87", 0x22C7, SymbolClass::BinaryOperator },
    { { "centerdot" }, "\xC2\xB7", 0x00B7, SymbolClass::BinaryOperator },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_OPERATORS_HPP
