#ifndef FIZMO_MATHTEXT_SYMBOLS_GREEK_HPP
#define FIZMO_MATHTEXT_SYMBOLS_GREEK_HPP

#include "symbol_types.hpp"

namespace fizmo {
namespace mathtext {

inline constexpr SymbolInfo kGreekLowerSymbols[] = {
    { { "alpha" }, "\xCE\xB1", 0x03B1, SymbolClass::GreekLower },
    { { "beta" }, "\xCE\xB2", 0x03B2, SymbolClass::GreekLower },
    { { "varbeta" }, "\xCF\x90", 0x03D0, SymbolClass::GreekLower },
    { { "gamma" }, "\xCE\xB3", 0x03B3, SymbolClass::GreekLower },
    { { "delta" }, "\xCE\xB4", 0x03B4, SymbolClass::GreekLower },
    { { "epsilon" }, "\xCF\xB5", 0x03F5, SymbolClass::GreekLower },
    { { "varepsilon" }, "\xCE\xB5", 0x03B5, SymbolClass::GreekLower },
    { { "zeta" }, "\xCE\xB6", 0x03B6, SymbolClass::GreekLower },
    { { "eta" }, "\xCE\xB7", 0x03B7, SymbolClass::GreekLower },
    { { "theta" }, "\xCE\xB8", 0x03B8, SymbolClass::GreekLower },
    { { "vartheta" }, "\xCF\x91", 0x03D1, SymbolClass::GreekLower },
    { { "iota" }, "\xCE\xB9", 0x03B9, SymbolClass::GreekLower },
    { { "kappa" }, "\xCE\xBA", 0x03BA, SymbolClass::GreekLower },
    { { "varkappa" }, "\xCF\xB0", 0x03F0, SymbolClass::GreekLower },
    { { "lambda" }, "\xCE\xBB", 0x03BB, SymbolClass::GreekLower },
    { { "mu" }, "\xCE\xBC", 0x03BC, SymbolClass::GreekLower },
    { { "nu" }, "\xCE\xBD", 0x03BD, SymbolClass::GreekLower },
    { { "xi" }, "\xCE\xBE", 0x03BE, SymbolClass::GreekLower },
    { { "omicron" }, "\xCE\xBF", 0x03BF, SymbolClass::GreekLower },
    { { "pi" }, "\xCF\x80", 0x03C0, SymbolClass::GreekLower },
    { { "varpi" }, "\xCF\x96", 0x03D6, SymbolClass::GreekLower },
    { { "rho" }, "\xCF\x81", 0x03C1, SymbolClass::GreekLower },
    { { "varrho" }, "\xCF\xB1", 0x03F1, SymbolClass::GreekLower },
    { { "sigma" }, "\xCF\x83", 0x03C3, SymbolClass::GreekLower },
    { { "varsigma" }, "\xCF\x82", 0x03C2, SymbolClass::GreekLower },
    { { "tau" }, "\xCF\x84", 0x03C4, SymbolClass::GreekLower },
    { { "upsilon" }, "\xCF\x85", 0x03C5, SymbolClass::GreekLower },
    { { "phi" }, "\xCF\x95", 0x03D5, SymbolClass::GreekLower },
    { { "varphi" }, "\xCF\x86", 0x03C6, SymbolClass::GreekLower },
    { { "chi" }, "\xCF\x87", 0x03C7, SymbolClass::GreekLower },
    { { "psi" }, "\xCF\x88", 0x03C8, SymbolClass::GreekLower },
    { { "omega" }, "\xCF\x89", 0x03C9, SymbolClass::GreekLower },
    { { "digamma" }, "\xCF\x9D", 0x03DD, SymbolClass::GreekLower },
    { { "koppa" }, "\xCF\x9F", 0x03DF, SymbolClass::GreekLower },
    { { "varkoppa" }, "\xCF\x99", 0x03D9, SymbolClass::GreekLower },
    { { "stigma" }, "\xCF\x9B", 0x03DB, SymbolClass::GreekLower },
    { { "sampi" }, "\xCF\xA1", 0x03E1, SymbolClass::GreekLower },
    { { "heta" }, "\xCD\xB1", 0x0371, SymbolClass::GreekLower },
    { { "san" }, "\xCF\xBB", 0x03FB, SymbolClass::GreekLower },
    { { "sho" }, "\xCF\xB8", 0x03F8, SymbolClass::GreekLower },
};

inline constexpr SymbolInfo kGreekUpperSymbols[] = {
    { { "Alpha" }, "\xCE\x91", 0x0391, SymbolClass::GreekUpper },
    { { "Beta" }, "\xCE\x92", 0x0392, SymbolClass::GreekUpper },
    { { "Gamma" }, "\xCE\x93", 0x0393, SymbolClass::GreekUpper },
    { { "Delta" }, "\xCE\x94", 0x0394, SymbolClass::GreekUpper },
    { { "Epsilon" }, "\xCE\x95", 0x0395, SymbolClass::GreekUpper },
    { { "Zeta" }, "\xCE\x96", 0x0396, SymbolClass::GreekUpper },
    { { "Eta" }, "\xCE\x97", 0x0397, SymbolClass::GreekUpper },
    { { "Theta" }, "\xCE\x98", 0x0398, SymbolClass::GreekUpper },
    { { "varTheta" }, "\xCF\xB4", 0x03F4, SymbolClass::GreekUpper },
    { { "Iota" }, "\xCE\x99", 0x0399, SymbolClass::GreekUpper },
    { { "Kappa" }, "\xCE\x9A", 0x039A, SymbolClass::GreekUpper },
    { { "Lambda" }, "\xCE\x9B", 0x039B, SymbolClass::GreekUpper },
    { { "Mu" }, "\xCE\x9C", 0x039C, SymbolClass::GreekUpper },
    { { "Nu" }, "\xCE\x9D", 0x039D, SymbolClass::GreekUpper },
    { { "Xi" }, "\xCE\x9E", 0x039E, SymbolClass::GreekUpper },
    { { "Omicron" }, "\xCE\x9F", 0x039F, SymbolClass::GreekUpper },
    { { "Pi" }, "\xCE\xA0", 0x03A0, SymbolClass::GreekUpper },
    { { "Rho" }, "\xCE\xA1", 0x03A1, SymbolClass::GreekUpper },
    { { "Sigma" }, "\xCE\xA3", 0x03A3, SymbolClass::GreekUpper },
    { { "Tau" }, "\xCE\xA4", 0x03A4, SymbolClass::GreekUpper },
    { { "Upsilon" }, "\xCE\xA5", 0x03A5, SymbolClass::GreekUpper },
    { { "varUpsilon" }, "\xCF\x92", 0x03D2, SymbolClass::GreekUpper },
    { { "Phi" }, "\xCE\xA6", 0x03A6, SymbolClass::GreekUpper },
    { { "Chi" }, "\xCE\xA7", 0x03A7, SymbolClass::GreekUpper },
    { { "Psi" }, "\xCE\xA8", 0x03A8, SymbolClass::GreekUpper },
    { { "Omega" }, "\xCE\xA9", 0x03A9, SymbolClass::GreekUpper },
    { { "Digamma" }, "\xCF\x9C", 0x03DC, SymbolClass::GreekUpper },
    { { "Koppa" }, "\xCF\x9E", 0x03DE, SymbolClass::GreekUpper },
    { { "varKoppa" }, "\xCF\x98", 0x03D8, SymbolClass::GreekUpper },
    { { "Stigma" }, "\xCF\x9A", 0x03DA, SymbolClass::GreekUpper },
    { { "Sampi" }, "\xCF\xA0", 0x03E0, SymbolClass::GreekUpper },
    { { "Heta" }, "\xCD\xB0", 0x0370, SymbolClass::GreekUpper },
    { { "San" }, "\xCF\xBA", 0x03FA, SymbolClass::GreekUpper },
    { { "Sho" }, "\xCF\xB7", 0x03F7, SymbolClass::GreekUpper },
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_GREEK_HPP
