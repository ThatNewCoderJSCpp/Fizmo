#include "line_break.hpp"
#include "../../../String/unicode.hpp"

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

namespace {

bool in(char32_t c, char32_t lo, char32_t hi) noexcept { return c >= lo && c <= hi; }

bool is_ideograph(char32_t c) noexcept {
    return in(c, 0x2E80, 0x2FFF) || in(c, 0x3040, 0x309F) || in(c, 0x30A0, 0x30FF) || in(c, 0x3100, 0x312F)
        || in(c, 0x3130, 0x318F) || in(c, 0x3190, 0x31EF) || in(c, 0x3200, 0x4DBF) || in(c, 0x4E00, 0x9FFF)
        || in(c, 0xA000, 0xA4CF) || in(c, 0xAC00, 0xD7AF) || in(c, 0xF900, 0xFAFF) || in(c, 0xFE30, 0xFE4F)
        || in(c, 0xFF00, 0xFFEF) || in(c, 0x1B000, 0x1B2FF) || in(c, 0x20000, 0x3FFFD) || in(c, 0x1100, 0x11FF);
}

bool is_small_kana(char32_t c) noexcept {
    switch (c) {
        case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049: case 0x3063: case 0x3083: case 0x3085:
        case 0x3087: case 0x308E: case 0x3095: case 0x3096: case 0x30A1: case 0x30A3: case 0x30A5: case 0x30A7:
        case 0x30A9: case 0x30C3: case 0x30E3: case 0x30E5: case 0x30E7: case 0x30EE: case 0x30F5: case 0x30F6:
        case 0x30FC: case 0x30FB: case 0x3005: case 0x303B: case 0x309B: case 0x309C: case 0x309D: case 0x309E:
        case 0x30FD: case 0x30FE: case 0x301C: case 0x30A0: case 0x203C: case 0x2047: case 0x2048: case 0x2049:
        case 0xFF1A: case 0xFF1B: case 0xFF65: case 0xFF70:
            return true;
        default:
            return in(c, 0x31F0, 0x31FF) || in(c, 0xFF67, 0xFF6F);
    }
}

} // namespace

LineBreakClass line_break_class(char32_t c) noexcept {
    using L = LineBreakClass;
    switch (c) {
        case '\n': case '\r': case 0x0B: case 0x0C: case 0x85: case 0x2028: case 0x2029: return L::BK;
        case ' ': return L::SP;
        case '\t': return L::BA;
        case 0x200B: return L::ZW;
        case 0x200D: return L::ZWJ;
        case 0x2060: case 0xFEFF: return L::WJ;
        case 0x00A0: case 0x202F: case 0x2007: case 0x034F: case 0x180E: case 0x2011: case 0x0F0C: return L::GL;
        case 0x00AD: case 0x2010: case 0x2012: case 0x2013: case '|': case 0x2027: case 0x1680: case 0x205F: return L::BA;
        case 0x00B4: case 0x02C8: case 0x02CC: case 0x02DF: case 0x0F01: case 0x1FFD: return L::BB;
        case '-': return L::HY;
        case 0x2014: return L::B2;
        case ')': case ']': return L::CP;
        case '}': case 0x3001: case 0x3002: case 0xFF0C: case 0xFF0E: case 0x300D: case 0x300F: case 0x3011:
        case 0x3015: case 0x3017: case 0x3019: case 0x301B: case 0xFF09: case 0xFF3D: case 0xFF5D: case 0x0F3B:
        case 0x0F3D: case 0x169C: case 0x2046: case 0x207E: case 0x208E: case 0x2309: case 0x230B: case 0x232A:
        case 0xFE11: case 0xFE12: case 0xFF61: case 0xFF63: case 0xFF64:
            return L::CL;
        case '(': case '[': case '{': case 0x00A1: case 0x00BF: case 0x300C: case 0x300E: case 0x3010: case 0x3014:
        case 0x3016: case 0x3018: case 0x301A: case 0xFF08: case 0xFF3B: case 0xFF5B: case 0xFF62: case 0x2045:
        case 0x207D: case 0x208D: case 0x2308: case 0x230A: case 0x2329: case 0x0F3A: case 0x0F3C: case 0x169B:
            return L::OP;
        case '!': case '?': case 0xFF01: case 0xFF1F: case 0x05C6: case 0x061B: case 0x061F: case 0x06D4: return L::EX;
        case ',': case '.': case ':': case ';': case 0x037E: case 0x0589: case 0x060C: case 0x060D: case 0x07F8:
        case 0x2044: case 0xFE10: case 0xFE13: case 0xFE14:
            return L::IS;
        case '/': return L::SY;
        case '"': case '\'': case 0x00AB: case 0x00BB: case 0x2018: case 0x2019: case 0x201B: case 0x201C:
        case 0x201D: case 0x201F: case 0x2039: case 0x203A: case 0x275B: case 0x275C: case 0x275D: case 0x275E:
            return L::QU;
        case '$': case '+': case '\\': case 0x00A3: case 0x00A5: case 0x00B1: case 0x2116: case 0x2212: case 0x2213:
            return L::PR;
        case '%': case 0x00A2: case 0x00B0: case 0x2030: case 0x2031: case 0x2032: case 0x2033: case 0x2034:
        case 0x2035: case 0x2036: case 0x2037: case 0x2103: case 0x2109: case 0x060A: case 0x066A:
            return L::PO;
        default:
            break;
    }
    if (in(c, 0x20A0, 0x20CF)) return c == 0x20A7 || c == 0x20B6 || c == 0x20BB || c == 0x20BE ? L::PO : L::PR;
    if (in(c, 0x2000, 0x200A)) return L::BA;
    if (in(c, 0xFE00, 0xFE0F) || in(c, 0xE0100, 0xE01EF) || in(c, 0xE0020, 0xE007F) || c == 0x200C) return L::CM;
    if (in(c, 0x1F1E6, 0x1F1FF)) return L::RI;
    if (in(c, 0x1F3FB, 0x1F3FF)) return L::EM;
    if (c < 0x20 || in(c, 0x7F, 0x9F)) return L::CM;
    if (unicode::is_mark(c)) return L::CM;
    if (is_small_kana(c)) return L::NS;
    if (unicode::is_digit(c)) return L::NU;
    if (in(c, 0x05D0, 0x05EA) || in(c, 0x05EF, 0x05F2) || in(c, 0xFB1D, 0xFB4F)) return L::HL;
    if (is_ideograph(c)) return L::ID;
    if (unicode::is_extended_pictographic(c)) return unicode::is_emoji_presentation(c) ? L::EB : L::ID;
    return L::AL;
}

void find_line_breaks(const char32_t* text, std::size_t count, std::vector<std::uint8_t>& allowed) {
    using L = LineBreakClass;
    allowed.assign(count + 1, 0);
    if (count < 2) return;
    std::vector<L> cls(count);
    for (std::size_t i = 0; i < count; ++i) cls[i] = line_break_class(text[i]);
    std::vector<L> eff(cls);
    for (std::size_t i = 0; i < count; ++i) {
        if (cls[i] != L::CM && cls[i] != L::ZWJ) continue;
        if (i > 0 && eff[i - 1] != L::SP && eff[i - 1] != L::BK && eff[i - 1] != L::ZW) eff[i] = eff[i - 1];
        else eff[i] = L::AL;
    }
    std::size_t ri_run = 0;
    for (std::size_t i = 1; i < count; ++i) {
        const L b = cls[i];
        const L a = eff[i - 1];
        if (cls[i - 1] == L::RI) ++ri_run;
        else if (cls[i - 1] != L::CM && cls[i - 1] != L::ZWJ) ri_run = 0;
        bool brk = true;
        if (a == L::BK || b == L::BK) brk = false;
        else if (b == L::SP || b == L::ZW) brk = false;
        else if (b == L::CM || b == L::ZWJ) brk = false;
        else if (cls[i - 1] == L::ZWJ) brk = false;
        else if (a == L::SP) {
            std::size_t j = i - 1;
            while (j > 0 && eff[j] == L::SP) --j;
            const L before = eff[j] == L::SP ? L::SP : eff[j];
            if (before == L::ZW) brk = true;
            else if (b == L::WJ || b == L::GL || b == L::CL || b == L::CP || b == L::EX || b == L::IS || b == L::SY) brk = false;
            else if (before == L::OP) brk = false;
            else if (before == L::QU && b == L::OP) brk = false;
            else if ((before == L::CL || before == L::CP) && b == L::NS) brk = false;
            else if (before == L::B2 && b == L::B2) brk = false;
            else brk = true;
        } else if (a == L::ZW) brk = true;
        else if (a == L::WJ || b == L::WJ) brk = false;
        else if (a == L::GL) brk = false;
        else if (b == L::GL) brk = !(a != L::SP && a != L::BA && a != L::HY);
        else if (b == L::CL || b == L::CP || b == L::EX || b == L::IS || b == L::SY) brk = false;
        else if (a == L::OP) brk = false;
        else if (a == L::QU && b == L::OP) brk = false;
        else if ((a == L::CL || a == L::CP) && b == L::NS) brk = false;
        else if (a == L::B2 && b == L::B2) brk = false;
        else if (b == L::QU || a == L::QU) brk = false;
        else if (b == L::BA || b == L::HY || b == L::NS || a == L::BB) brk = false;
        else if (i >= 2 && eff[i - 2] == L::HL && (a == L::HY || a == L::BA)) brk = false;
        else if (a == L::SY && b == L::HL) brk = false;
        else if ((a == L::AL || a == L::HL) && b == L::NU) brk = false;
        else if (a == L::NU && (b == L::AL || b == L::HL)) brk = false;
        else if (a == L::PR && (b == L::ID || b == L::EB || b == L::EM)) brk = false;
        else if ((a == L::ID || a == L::EB || a == L::EM) && b == L::PO) brk = false;
        else if ((a == L::PR || a == L::PO) && (b == L::AL || b == L::HL)) brk = false;
        else if ((a == L::AL || a == L::HL) && (b == L::PR || b == L::PO)) brk = false;
        else if ((a == L::CL || a == L::CP || a == L::NU) && (b == L::PO || b == L::PR)) brk = false;
        else if ((a == L::PO || a == L::PR) && (b == L::OP || b == L::NU)) brk = false;
        else if ((a == L::HY || a == L::NU) && b == L::NU) brk = false;
        else if ((a == L::IS || a == L::SY) && b == L::NU) {
            std::size_t j = i - 1;
            bool number = false;
            while (true) {
                if (eff[j] == L::NU) { number = true; break; }
                if (eff[j] != L::IS && eff[j] != L::SY) break;
                if (j == 0) break;
                --j;
            }
            brk = !number;
        }
        else if ((a == L::AL || a == L::HL) && (b == L::AL || b == L::HL)) brk = false;
        else if (a == L::IS && (b == L::AL || b == L::HL)) brk = false;
        else if ((a == L::AL || a == L::HL || a == L::NU) && b == L::OP) brk = false;
        else if (a == L::CP && (b == L::AL || b == L::HL || b == L::NU)) brk = false;
        else if (a == L::RI && b == L::RI) brk = (ri_run % 2) == 0;
        else if (a == L::EB && b == L::EM) brk = false;
        allowed[i] = brk ? 1 : 0;
    }
}

bool needs_hyphen(char32_t before, char32_t after) noexcept {
    if (before == 0x00AD) return true;
    return unicode::is_alphabetic(before) && unicode::is_alphabetic(after) && !(before >= 0x2E80 && unicode::code_point_width(before) == 2);
}

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo
