#ifndef FIZMO_MATHTEXT_SYMBOLS_ORDINARY_HPP
#define FIZMO_MATHTEXT_SYMBOLS_ORDINARY_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kOrdinarySymbols[] = {
    { { "plus_sign" }, "+", 0x002B, SymbolClass::Ordinary },
    { { "minus_sign" }, "\xE2\x88\x92", 0x2212, SymbolClass::Ordinary },
    { { "infty", "infinity", "inf" }, "\xE2\x88\x9E", 0x221E, SymbolClass::Ordinary },
    { { "partial", "partial_derivative" }, "\xE2\x88\x82", 0x2202, SymbolClass::Ordinary },
    { { "nabla", "gradient", "grad" }, "\xE2\x88\x87", 0x2207, SymbolClass::Ordinary },
    { { "forall", "for_all" }, "\xE2\x88\x80", 0x2200, SymbolClass::Ordinary },
    { { "exists", "there_exists" }, "\xE2\x88\x83", 0x2203, SymbolClass::Ordinary },
    { { "nexists", "there_does_not_exist", "not_exists" }, "\xE2\x88\x84", 0x2204, SymbolClass::Ordinary },
    { { "emptyset", "varnothing", "empty_set", "empty" }, "\xE2\x88\x85", 0x2205, SymbolClass::Ordinary },
    { { "neg", "lnot", "logical_not" }, "\xC2\xAC", 0x00AC, SymbolClass::Ordinary },
    { { "top", "tautology" }, "\xE2\x8A\xA4", 0x22A4, SymbolClass::Ordinary },
    { { "bot", "contradiction", "falsum" }, "\xE2\x8A\xA5", 0x22A5, SymbolClass::Ordinary },
    { { "therefore", "therefore_sign" }, "\xE2\x88\xB4", 0x2234, SymbolClass::Ordinary },
    { { "because", "because_sign" }, "\xE2\x88\xB5", 0x2235, SymbolClass::Ordinary },
    { { "angle", "angle_sign" }, "\xE2\x88\xA0", 0x2220, SymbolClass::Ordinary },
    { { "measuredangle", "measured_angle" }, "\xE2\x88\xA1", 0x2221, SymbolClass::Ordinary },
    { { "sphericalangle" }, "\xE2\x88\xA2", 0x2222, SymbolClass::Ordinary },
    { { "degree", "degrees" }, "\xC2\xB0", 0x00B0, SymbolClass::Ordinary },
    { { "prime", "prime_mark" }, "\xE2\x80\xB2", 0x2032, SymbolClass::Ordinary },
    { { "dprime", "double_prime" }, "\xE2\x80\xB3", 0x2033, SymbolClass::Ordinary },
    { { "trprime" }, "\xE2\x80\xB4", 0x2034, SymbolClass::Ordinary },
    { { "backprime" }, "\xE2\x80\xB5", 0x2035, SymbolClass::Ordinary },
    { { "cdots", "center_dots" }, "\xE2\x8B\xAF", 0x22EF, SymbolClass::Ordinary },
    { { "ldots", "dots", "ellipsis" }, "\xE2\x80\xA6", 0x2026, SymbolClass::Ordinary },
    { { "vdots", "vertical_dots" }, "\xE2\x8B\xAE", 0x22EE, SymbolClass::Ordinary },
    { { "ddots", "diagonal_dots" }, "\xE2\x8B\xB1", 0x22F1, SymbolClass::Ordinary },
    { { "adots" }, "\xE2\x8B\xB0", 0x22F0, SymbolClass::Ordinary },
    { { "surd", "root_sign" }, "\xE2\x88\x9A", 0x221A, SymbolClass::Ordinary },
    { { "triangle", "triangle_shape" }, "\xE2\x96\xB3", 0x25B3, SymbolClass::Ordinary },
    { { "vartriangle" }, "\xE2\x96\xB5", 0x25B5, SymbolClass::Ordinary },
    { { "triangledown" }, "\xE2\x96\xBF", 0x25BF, SymbolClass::Ordinary },
    { { "blacktriangle" }, "\xE2\x96\xB4", 0x25B4, SymbolClass::Ordinary },
    { { "blacktriangledown" }, "\xE2\x96\xBE", 0x25BE, SymbolClass::Ordinary },
    { { "blacktriangleleft" }, "\xE2\x97\x82", 0x25C2, SymbolClass::Ordinary },
    { { "blacktriangleright" }, "\xE2\x96\xB8", 0x25B8, SymbolClass::Ordinary },
    { { "square", "Box", "square_shape" }, "\xE2\x96\xA1", 0x25A1, SymbolClass::Ordinary },
    { { "blacksquare" }, "\xE2\x96\xA0", 0x25A0, SymbolClass::Ordinary },
    { { "lozenge" }, "\xE2\x97\x8A", 0x25CA, SymbolClass::Ordinary },
    { { "blacklozenge" }, "\xE2\xA7\xAB", 0x29EB, SymbolClass::Ordinary },
    { { "bigstar" }, "\xE2\x98\x85", 0x2605, SymbolClass::Ordinary },
    { { "diamondsuit" }, "\xE2\x99\xA2", 0x2662, SymbolClass::Ordinary },
    { { "heartsuit" }, "\xE2\x99\xA1", 0x2661, SymbolClass::Ordinary },
    { { "clubsuit" }, "\xE2\x99\xA3", 0x2663, SymbolClass::Ordinary },
    { { "spadesuit" }, "\xE2\x99\xA0", 0x2660, SymbolClass::Ordinary },
    { { "flat" }, "\xE2\x99\xAD", 0x266D, SymbolClass::Ordinary },
    { { "natural" }, "\xE2\x99\xAE", 0x266E, SymbolClass::Ordinary },
    { { "sharp" }, "\xE2\x99\xAF", 0x266F, SymbolClass::Ordinary },
    { { "checkmark", "check_mark" }, "\xE2\x9C\x93", 0x2713, SymbolClass::Ordinary },
    { { "maltese" }, "\xE2\x9C\xA0", 0x2720, SymbolClass::Ordinary },
    { { "S" }, "\xC2\xA7", 0x00A7, SymbolClass::Ordinary },
    { { "P" }, "\xC2\xB6", 0x00B6, SymbolClass::Ordinary },
    { { "copyright" }, "\xC2\xA9", 0x00A9, SymbolClass::Ordinary },
    { { "circledR" }, "\xC2\xAE", 0x00AE, SymbolClass::Ordinary },
    { { "circledS" }, "\xE2\x93\x88", 0x24C8, SymbolClass::Ordinary },
    { { "pounds" }, "\xC2\xA3", 0x00A3, SymbolClass::Ordinary },
    { { "yen" }, "\xC2\xA5", 0x00A5, SymbolClass::Ordinary },
    { { "euro" }, "\xE2\x82\xAC", 0x20AC, SymbolClass::Ordinary },
    { { "cent" }, "\xC2\xA2", 0x00A2, SymbolClass::Ordinary },
    { { "permil" }, "\xE2\x80\xB0", 0x2030, SymbolClass::Ordinary },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_ORDINARY_HPP
