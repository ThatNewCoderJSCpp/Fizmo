#ifndef FIZMO_MATHTEXT_SYMBOLS_LETTERLIKE_HPP
#define FIZMO_MATHTEXT_SYMBOLS_LETTERLIKE_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kLetterlikeSymbols[] = {
    { { "hbar", "hslash", "h_bar", "reduced_planck", "planck" }, "\xE2\x84\x8F", 0x210F, SymbolClass::Letterlike },
    { { "ell", "script_l" }, "\xE2\x84\x93", 0x2113, SymbolClass::Letterlike },
    { { "Re", "real_part" }, "\xE2\x84\x9C", 0x211C, SymbolClass::Letterlike },
    { { "Im", "imaginary_part" }, "\xE2\x84\x91", 0x2111, SymbolClass::Letterlike },
    { { "wp", "weierstrass_p" }, "\xE2\x84\x98", 0x2118, SymbolClass::Letterlike },
    { { "aleph", "alef" }, "\xE2\x84\xB5", 0x2135, SymbolClass::Letterlike },
    { { "beth" }, "\xE2\x84\xB6", 0x2136, SymbolClass::Letterlike },
    { { "gimel" }, "\xE2\x84\xB7", 0x2137, SymbolClass::Letterlike },
    { { "daleth" }, "\xE2\x84\xB8", 0x2138, SymbolClass::Letterlike },
    { { "mho" }, "\xE2\x84\xA7", 0x2127, SymbolClass::Letterlike },
    { { "Finv" }, "\xE2\x84\xB2", 0x2132, SymbolClass::Letterlike },
    { { "Game" }, "\xE2\x85\x81", 0x2141, SymbolClass::Letterlike },
    { { "complement" }, "\xE2\x88\x81", 0x2201, SymbolClass::Letterlike },
    { { "eth" }, "\xC3\xB0", 0x00F0, SymbolClass::Letterlike },
    { { "imath" }, "\xC4\xB1", 0x0131, SymbolClass::Letterlike },
    { { "jmath" }, "\xC8\xB7", 0x0237, SymbolClass::Letterlike },
    { { "Bbbk" }, "\xF0\x9D\x95\x9C", 0x1D55C, SymbolClass::Letterlike },
    { { "N", "naturals" }, "\xE2\x84\x95", 0x2115, SymbolClass::Letterlike },
    { { "Z", "integers" }, "\xE2\x84\xA4", 0x2124, SymbolClass::Letterlike },
    { { "Q", "rationals" }, "\xE2\x84\x9A", 0x211A, SymbolClass::Letterlike },
    { { "R", "reals" }, "\xE2\x84\x9D", 0x211D, SymbolClass::Letterlike },
    { { "C", "complexes" }, "\xE2\x84\x82", 0x2102, SymbolClass::Letterlike },
    { { "H", "quaternions" }, "\xE2\x84\x8D", 0x210D, SymbolClass::Letterlike },
    { { "primes" }, "\xE2\x84\x99", 0x2119, SymbolClass::Letterlike },
    { { "Angstrom" }, "\xE2\x84\xAB", 0x212B, SymbolClass::Letterlike },
    { { "ohm" }, "\xE2\x84\xA6", 0x2126, SymbolClass::Letterlike },
    { { "kelvin" }, "\xE2\x84\xAA", 0x212A, SymbolClass::Letterlike },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_LETTERLIKE_HPP
