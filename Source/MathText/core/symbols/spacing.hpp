#ifndef FIZMO_MATHTEXT_SYMBOLS_SPACING_HPP
#define FIZMO_MATHTEXT_SYMBOLS_SPACING_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kSpaceSymbols[] = {
    { { "thinspace", ",", "thin_space" }, "\xE2\x80\x89", 0x2009, SymbolClass::Space },
    { { "medspace", ":", "medium_space" }, "\xE2\x81\x9F", 0x205F, SymbolClass::Space },
    { { "thickspace", ";", "thick_space" }, "\xE2\x80\x85", 0x2005, SymbolClass::Space },
    { { "enspace", "en_space" }, "\xE2\x80\x82", 0x2002, SymbolClass::Space },
    { { "quad", "quad_space" }, "\xE2\x80\x83", 0x2003, SymbolClass::Space },
    { { "qquad", "double_quad" }, "\xE2\x80\x81", 0x2001, SymbolClass::Space },
};

inline constexpr SymbolInfo kPunctuationSymbols[] = {
    { { "\\" }, "\x5C", 0x005C, SymbolClass::Punctuation },
    { { "_" }, "\x5F", 0x005F, SymbolClass::Punctuation },
    { { "#" }, "\x23", 0x0023, SymbolClass::Punctuation },
    { { "&" }, "\x26", 0x0026, SymbolClass::Punctuation },
    { { "$" }, "\x24", 0x0024, SymbolClass::Punctuation },
    { { "%" }, "\x25", 0x0025, SymbolClass::Punctuation },
    { { "colon" }, "\x3A", 0x003A, SymbolClass::Punctuation },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_SPACING_HPP
