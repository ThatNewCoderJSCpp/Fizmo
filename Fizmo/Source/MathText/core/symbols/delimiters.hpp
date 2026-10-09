#ifndef FIZMO_MATHTEXT_SYMBOLS_DELIMITERS_HPP
#define FIZMO_MATHTEXT_SYMBOLS_DELIMITERS_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kDelimiterSymbols[] = {
    { { "langle", "left_angle", "lang" }, "\xE2\x9F\xA8", 0x27E8, SymbolClass::Delimiter },
    { { "rangle", "right_angle", "rang" }, "\xE2\x9F\xA9", 0x27E9, SymbolClass::Delimiter },
    { { "lceil", "left_ceil" }, "\xE2\x8C\x88", 0x2308, SymbolClass::Delimiter },
    { { "rceil", "right_ceil" }, "\xE2\x8C\x89", 0x2309, SymbolClass::Delimiter },
    { { "lfloor", "left_floor" }, "\xE2\x8C\x8A", 0x230A, SymbolClass::Delimiter },
    { { "rfloor", "right_floor" }, "\xE2\x8C\x8B", 0x230B, SymbolClass::Delimiter },
    { { "llbracket", "left_double_bracket" }, "\xE2\x9F\xA6", 0x27E6, SymbolClass::Delimiter },
    { { "rrbracket", "right_double_bracket" }, "\xE2\x9F\xA7", 0x27E7, SymbolClass::Delimiter },
    { { "lgroup" }, "\xE2\x9F\xAE", 0x27EE, SymbolClass::Delimiter },
    { { "rgroup" }, "\xE2\x9F\xAF", 0x27EF, SymbolClass::Delimiter },
    { { "lmoustache" }, "\xE2\x8E\xB0", 0x23B0, SymbolClass::Delimiter },
    { { "rmoustache" }, "\xE2\x8E\xB1", 0x23B1, SymbolClass::Delimiter },
    { { "ulcorner" }, "\xE2\x8C\x9C", 0x231C, SymbolClass::Delimiter },
    { { "urcorner" }, "\xE2\x8C\x9D", 0x231D, SymbolClass::Delimiter },
    { { "llcorner" }, "\xE2\x8C\x9E", 0x231E, SymbolClass::Delimiter },
    { { "lrcorner" }, "\xE2\x8C\x9F", 0x231F, SymbolClass::Delimiter },
    { { "vert", "lvert", "rvert" }, "\x7C", 0x007C, SymbolClass::Delimiter },
    { { "Vert", "lVert", "rVert", "|" }, "\xE2\x80\x96", 0x2016, SymbolClass::Delimiter },
    { { "lbrace", "{" }, "\x7B", 0x007B, SymbolClass::Delimiter },
    { { "rbrace", "}" }, "\x7D", 0x007D, SymbolClass::Delimiter },
    { { "lbrack" }, "\x5B", 0x005B, SymbolClass::Delimiter },
    { { "rbrack" }, "\x5D", 0x005D, SymbolClass::Delimiter },
    { { "lparen" }, "\x28", 0x0028, SymbolClass::Delimiter },
    { { "rparen" }, "\x29", 0x0029, SymbolClass::Delimiter },
    { { "backslash" }, "\x5C", 0x005C, SymbolClass::Delimiter },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_DELIMITERS_HPP
