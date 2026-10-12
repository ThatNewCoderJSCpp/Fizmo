#include "unicode.hpp"
#include "string.hpp"
#include "unicode_tables.hpp"

#if defined(FIZMO_UNICODE_GENERATED_DATA)
    #include "fizmo_unicode_data.hpp"
#else
    #include "unicode_data.hpp"
#endif

#include <algorithm>
#include <vector>

namespace fizmo {
namespace unicode {
namespace {

namespace t = tables;

constexpr char32_t hangul_s_base  = 0xAC00;
constexpr char32_t hangul_l_base  = 0x1100;
constexpr char32_t hangul_v_base  = 0x1161;
constexpr char32_t hangul_t_base  = 0x11A7;
constexpr char32_t hangul_l_count = 19;
constexpr char32_t hangul_v_count = 21;
constexpr char32_t hangul_t_count = 28;
constexpr char32_t hangul_n_count = hangul_v_count * hangul_t_count;
constexpr char32_t hangul_s_count = hangul_l_count * hangul_n_count;

constexpr char32_t greek_capital_sigma = 0x03A3;
constexpr char32_t greek_final_sigma   = 0x03C2;

const t::Record& rec(char32_t cp) noexcept {
    if (cp > max_code_point) cp = max_code_point;
    const unsigned    shift = t::stage_shift;
    const std::size_t block = t::stage1[cp >> shift];
    return t::records[t::stage2[(block << shift) | (cp & ((char32_t(1) << shift) - 1))]];
}

bool has_flag(char32_t cp, std::uint16_t f) noexcept { return (rec(cp).flags & f) != 0; }

GeneralCategory gc_of(char32_t cp) noexcept { return static_cast<GeneralCategory>(rec(cp).general_category); }

GraphemeBreak gcb_of(char32_t cp) noexcept { return static_cast<GraphemeBreak>(rec(cp).grapheme_break); }

WordBreak wb_of(char32_t cp) noexcept { return static_cast<WordBreak>(rec(cp).word_break); }

bool is_hangul_syllable(char32_t cp) noexcept { return cp >= hangul_s_base && cp < hangul_s_base + hangul_s_count; }

const t::SpecialCaseEntry* special_of(char32_t cp) noexcept {
    const t::SpecialCaseEntry* first = t::special_cases;
    const t::SpecialCaseEntry* last  = t::special_cases + t::special_case_count;
    const t::SpecialCaseEntry* it    = std::lower_bound(first, last, cp, [](const t::SpecialCaseEntry& e, char32_t c) { return e.code_point < c; });
    return (it != last && it->code_point == cp) ? it : nullptr;
}

const t::DecompositionEntry* decomposition_of(char32_t cp) noexcept {
    const t::DecompositionEntry* first = t::decompositions;
    const t::DecompositionEntry* last  = t::decompositions + t::decomposition_count;
    const t::DecompositionEntry* it    = std::lower_bound(first, last, cp, [](const t::DecompositionEntry& e, char32_t c) { return e.code_point < c; });
    return (it != last && it->code_point == cp) ? it : nullptr;
}

std::size_t copy_mapping(const char32_t (&src)[3], char32_t out[3]) noexcept {
    std::size_t n = 0;
    while (n < 3 && src[n] != 0) {
        out[n] = src[n];
        ++n;
    }
    return n;
}

void put(String& out, char32_t cp) {
    char buf[4];
    const std::size_t n = encode_utf8(cp, buf);
    out.append(buf, n);
}

void put_all(String& out, const char32_t* cps, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) put(out, cps[i]);
}

bool is_ascii(StringView s) noexcept {
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (static_cast<unsigned char>(s[i]) >= 0x80) return false;
    }
    return true;
}

bool final_sigma_applies(StringView s, std::size_t start, std::size_t end) noexcept {
    bool cased_before = false;
    std::size_t p = start;
    while (p > 0) {
        p = prev_code_point(s, p);
        const char32_t c = code_point_at(s, p);
        if (is_case_ignorable(c)) continue;
        cased_before = is_cased(c);
        break;
    }
    if (!cased_before) return false;
    std::size_t q = end;
    while (q < s.size()) {
        const char32_t c = code_point_at(s, q);
        q = next_code_point(s, q);
        if (is_case_ignorable(c)) continue;
        return !is_cased(c);
    }
    return true;
}

void append_lower_code_point(StringView s, std::size_t start, std::size_t end, char32_t cp, String& out) {
    if (cp == greek_capital_sigma && final_sigma_applies(s, start, end)) {
        put(out, greek_final_sigma);
        return;
    }
    char32_t m[3];
    put_all(out, m, full_to_lower(cp, m));
}

void decompose_into(char32_t cp, bool compat, std::vector<char32_t>& out) {
    if (is_hangul_syllable(cp)) {
        const char32_t s = cp - hangul_s_base;
        out.push_back(hangul_l_base + s / hangul_n_count);
        out.push_back(hangul_v_base + (s % hangul_n_count) / hangul_t_count);
        if (s % hangul_t_count != 0) out.push_back(hangul_t_base + s % hangul_t_count);
        return;
    }
    const std::uint16_t need = compat ? t::flag_compat_decomp : t::flag_canonical_decomp;
    if (has_flag(cp, need)) {
        const t::DecompositionEntry* e = decomposition_of(cp);
        if (e) {
            const std::size_t off = compat ? e->compat_offset : e->canonical_offset;
            const std::size_t len = compat ? e->compat_length : e->canonical_length;
            out.insert(out.end(), t::decomposition_data + off, t::decomposition_data + off + len);
            return;
        }
    }
    out.push_back(cp);
}

void canonical_order(std::vector<char32_t>& v) {
    for (std::size_t i = 1; i < v.size(); ++i) {
        const std::uint8_t cc = combining_class(v[i]);
        if (cc == 0) continue;
        std::size_t j = i;
        while (j > 0) {
            const std::uint8_t pc = combining_class(v[j - 1]);
            if (pc == 0 || pc <= cc) break;
            std::swap(v[j], v[j - 1]);
            --j;
        }
    }
}

char32_t compose_pair(char32_t a, char32_t b) noexcept {
    if (a >= hangul_l_base && a < hangul_l_base + hangul_l_count && b >= hangul_v_base && b < hangul_v_base + hangul_v_count) {
        return hangul_s_base + ((a - hangul_l_base) * hangul_v_count + (b - hangul_v_base)) * hangul_t_count;
    }
    if (is_hangul_syllable(a) && (a - hangul_s_base) % hangul_t_count == 0 && b > hangul_t_base && b < hangul_t_base + hangul_t_count) {
        return a + (b - hangul_t_base);
    }
    if (!has_flag(b, t::flag_composes_second)) return 0;
    const t::CompositionEntry* first = t::compositions;
    const t::CompositionEntry* last  = t::compositions + t::composition_count;
    const t::CompositionEntry* it    = std::lower_bound(first, last, std::make_pair(a, b), [](const t::CompositionEntry& e, const std::pair<char32_t, char32_t>& k) {
        return e.first < k.first || (e.first == k.first && e.second < k.second);
    });
    return (it != last && it->first == a && it->second == b) ? it->composite : 0;
}

void compose(std::vector<char32_t>& v) {
    if (v.empty()) return;
    std::size_t starter_pos = 0;
    char32_t    starter     = v[0];
    int         last_class  = combining_class(starter);
    if (last_class != 0) last_class = 256;
    std::size_t write = 1;
    for (std::size_t i = 1; i < v.size(); ++i) {
        const char32_t ch       = v[i];
        const int      ch_class = combining_class(ch);
        const char32_t comp     = (last_class < ch_class || last_class == 0) ? compose_pair(starter, ch) : 0;
        if (comp != 0) {
            v[starter_pos] = comp;
            starter        = comp;
            continue;
        }
        if (ch_class == 0) {
            starter_pos = write;
            starter     = ch;
        }
        last_class = ch_class;
        v[write++] = ch;
    }
    v.resize(write);
}

bool quick_check(StringView s, NormalizationForm form) noexcept {
    std::uint16_t bad = 0;
    switch (form) {
        case NormalizationForm::nfc:  bad = t::flag_nfc_qc_not_yes; break;
        case NormalizationForm::nfkc: bad = t::flag_nfkc_qc_not_yes; break;
        case NormalizationForm::nfd:  bad = t::flag_canonical_decomp; break;
        case NormalizationForm::nfkd: bad = t::flag_compat_decomp; break;
    }
    std::uint8_t last = 0;
    std::size_t  pos  = 0;
    while (pos < s.size()) {
        if (static_cast<unsigned char>(s[pos]) < 0x80) {
            ++pos;
            last = 0;
            continue;
        }
        const std::size_t start = pos;
        const char32_t    cp    = decode_utf8(s.data(), s.size(), pos);
        if (cp == replacement_character && !(pos - start == 3 && static_cast<unsigned char>(s[start]) == 0xEF)) return false;
        const t::Record& r = rec(cp);
        if (r.flags & bad) return false;
        if (is_hangul_syllable(cp) && (form == NormalizationForm::nfd || form == NormalizationForm::nfkd)) return false;
        if (r.combining_class != 0 && last > r.combining_class) return false;
        last = r.combining_class;
    }
    return true;
}

bool grapheme_break_between(StringView s, std::size_t a_pos, char32_t a, char32_t b) noexcept {
    const GraphemeBreak pa = gcb_of(a);
    const GraphemeBreak pb = gcb_of(b);
    using G = GraphemeBreak;
    if (pa == G::cr && pb == G::lf) return false;
    if (pa == G::control || pa == G::cr || pa == G::lf) return true;
    if (pb == G::control || pb == G::cr || pb == G::lf) return true;
    if (pa == G::l && (pb == G::l || pb == G::v || pb == G::lv || pb == G::lvt)) return false;
    if ((pa == G::lv || pa == G::v) && (pb == G::v || pb == G::t)) return false;
    if ((pa == G::lvt || pa == G::t) && pb == G::t) return false;
    if (pb == G::extend || pb == G::zwj) return false;
    if (pb == G::spacing_mark) return false;
    if (pa == G::prepend) return false;
    if (rec(b).indic_conjunct_break == 1) {
        bool        linker = false;
        std::size_t i      = a_pos;
        for (;;) {
            const std::uint8_t ic = rec(code_point_at(s, i)).indic_conjunct_break;
            if (ic == 3) linker = true;
            else if (ic == 1) {
                if (linker) return false;
                break;
            } else if (ic != 2) break;
            if (i == 0) break;
            i = prev_code_point(s, i);
        }
    }
    if (pa == G::zwj && is_extended_pictographic(b) && a_pos > 0) {
        std::size_t i = prev_code_point(s, a_pos);
        for (;;) {
            const char32_t c = code_point_at(s, i);
            if (gcb_of(c) == G::extend && i > 0) {
                i = prev_code_point(s, i);
                continue;
            }
            if (is_extended_pictographic(c)) return false;
            break;
        }
    }
    if (pa == G::regional_indicator && pb == G::regional_indicator) {
        std::size_t count = 0;
        std::size_t i     = a_pos;
        for (;;) {
            if (gcb_of(code_point_at(s, i)) != G::regional_indicator) break;
            ++count;
            if (i == 0) break;
            i = prev_code_point(s, i);
        }
        return count % 2 == 0;
    }
    return true;
}

bool wb_ignorable(WordBreak w) noexcept { return w == WordBreak::extend || w == WordBreak::format || w == WordBreak::zwj; }
bool wb_newline(WordBreak w) noexcept { return w == WordBreak::newline || w == WordBreak::cr || w == WordBreak::lf; }
bool wb_ahletter(WordBreak w) noexcept { return w == WordBreak::a_letter || w == WordBreak::hebrew_letter; }
bool wb_midnumletq(WordBreak w) noexcept { return w == WordBreak::mid_num_let || w == WordBreak::single_quote; }

constexpr WordBreak wb_none = static_cast<WordBreak>(255);

std::size_t wb_effective_start(StringView s, std::size_t p) noexcept {
    while (wb_ignorable(wb_of(code_point_at(s, p))) && p > 0) {
        const std::size_t k = prev_code_point(s, p);
        if (wb_newline(wb_of(code_point_at(s, k)))) break;
        p = k;
    }
    return p;
}

WordBreak wb_prev_effective(StringView s, std::size_t p) noexcept {
    if (p == 0) return wb_none;
    return wb_of(code_point_at(s, wb_effective_start(s, prev_code_point(s, p))));
}

WordBreak wb_next_effective(StringView s, std::size_t p) noexcept {
    std::size_t q = next_code_point(s, p);
    while (q < s.size() && wb_ignorable(wb_of(code_point_at(s, q)))) q = next_code_point(s, q);
    return q < s.size() ? wb_of(code_point_at(s, q)) : wb_none;
}

} // namespace

const char* version() noexcept { return t::version; }

bool is_valid_utf8(StringView s) noexcept { return first_invalid_utf8(s) == s.size(); }

std::size_t first_invalid_utf8(StringView s) noexcept {
    std::size_t pos = 0;
    while (pos < s.size()) {
        if (static_cast<unsigned char>(s[pos]) < 0x80) {
            ++pos;
            continue;
        }
        const std::size_t start = pos;
        const char32_t    cp    = decode_utf8(s.data(), s.size(), pos);
        if (cp == replacement_character && !(pos - start == 3 && static_cast<unsigned char>(s[start]) == 0xEF)) return start;
    }
    return s.size();
}

std::size_t count_code_points(StringView s) noexcept {
    std::size_t n   = 0;
    std::size_t pos = 0;
    while (pos < s.size()) {
        if (static_cast<unsigned char>(s[pos]) < 0x80) ++pos;
        else decode_utf8(s.data(), s.size(), pos);
        ++n;
    }
    return n;
}

GeneralCategory general_category(char32_t cp) noexcept { return gc_of(cp); }
GraphemeBreak grapheme_break_property(char32_t cp) noexcept { return gcb_of(cp); }
WordBreak word_break_property(char32_t cp) noexcept { return wb_of(cp); }
std::uint8_t combining_class(char32_t cp) noexcept { return rec(cp).combining_class; }
int code_point_width(char32_t cp) noexcept { return rec(cp).width; }

bool is_letter(char32_t cp) noexcept {
    const auto g = rec(cp).general_category;
    return g >= 1 && g <= 5;
}

bool is_mark(char32_t cp) noexcept {
    const auto g = rec(cp).general_category;
    return g >= 6 && g <= 8;
}

bool is_number(char32_t cp) noexcept {
    const auto g = rec(cp).general_category;
    return g >= 9 && g <= 11;
}

bool is_digit(char32_t cp) noexcept { return gc_of(cp) == GeneralCategory::decimal_number; }

bool is_punctuation(char32_t cp) noexcept {
    const auto g = rec(cp).general_category;
    return g >= 12 && g <= 18;
}

bool is_symbol(char32_t cp) noexcept {
    const auto g = rec(cp).general_category;
    return g >= 19 && g <= 22;
}

bool is_separator(char32_t cp) noexcept {
    const auto g = rec(cp).general_category;
    return g >= 23 && g <= 25;
}

bool is_control(char32_t cp) noexcept { return gc_of(cp) == GeneralCategory::control; }
bool is_private_use(char32_t cp) noexcept { return gc_of(cp) == GeneralCategory::private_use; }
bool is_assigned(char32_t cp) noexcept { return cp <= max_code_point && gc_of(cp) != GeneralCategory::unassigned; }
bool is_alphabetic(char32_t cp) noexcept { return has_flag(cp, t::flag_alphabetic); }
bool is_alphanumeric(char32_t cp) noexcept { return is_alphabetic(cp) || is_number(cp); }
bool is_whitespace(char32_t cp) noexcept { return has_flag(cp, t::flag_white_space); }
bool is_lowercase(char32_t cp) noexcept { return has_flag(cp, t::flag_lowercase); }
bool is_uppercase(char32_t cp) noexcept { return has_flag(cp, t::flag_uppercase); }
bool is_titlecase(char32_t cp) noexcept { return gc_of(cp) == GeneralCategory::titlecase_letter; }
bool is_cased(char32_t cp) noexcept { return has_flag(cp, t::flag_cased); }
bool is_case_ignorable(char32_t cp) noexcept { return has_flag(cp, t::flag_case_ignorable); }
bool is_default_ignorable(char32_t cp) noexcept { return has_flag(cp, t::flag_default_ignorable); }
bool is_emoji(char32_t cp) noexcept { return has_flag(cp, t::flag_emoji); }
bool is_emoji_presentation(char32_t cp) noexcept { return has_flag(cp, t::flag_emoji_presentation); }
bool is_extended_pictographic(char32_t cp) noexcept { return has_flag(cp, t::flag_extended_pictographic); }
bool is_regional_indicator(char32_t cp) noexcept { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }

bool is_printable(char32_t cp) noexcept {
    if (cp == 0x20) return true;
    const auto g = rec(cp).general_category;
    return g != 0 && g < 23;
}

char32_t to_upper(char32_t cp) noexcept { return cp > max_code_point ? cp : static_cast<char32_t>(static_cast<std::int32_t>(cp) + rec(cp).upper_delta); }
char32_t to_lower(char32_t cp) noexcept { return cp > max_code_point ? cp : static_cast<char32_t>(static_cast<std::int32_t>(cp) + rec(cp).lower_delta); }
char32_t to_title(char32_t cp) noexcept { return cp > max_code_point ? cp : static_cast<char32_t>(static_cast<std::int32_t>(cp) + rec(cp).title_delta); }
char32_t fold_case(char32_t cp) noexcept { return cp > max_code_point ? cp : static_cast<char32_t>(static_cast<std::int32_t>(cp) + rec(cp).fold_delta); }

std::size_t full_to_upper(char32_t cp, char32_t out[3]) noexcept {
    if (has_flag(cp, t::flag_special_case)) {
        if (const t::SpecialCaseEntry* e = special_of(cp)) return copy_mapping(e->upper, out);
    }
    out[0] = to_upper(cp);
    return 1;
}

std::size_t full_to_lower(char32_t cp, char32_t out[3]) noexcept {
    if (has_flag(cp, t::flag_special_case)) {
        if (const t::SpecialCaseEntry* e = special_of(cp)) return copy_mapping(e->lower, out);
    }
    out[0] = to_lower(cp);
    return 1;
}

std::size_t full_to_title(char32_t cp, char32_t out[3]) noexcept {
    if (has_flag(cp, t::flag_special_case)) {
        if (const t::SpecialCaseEntry* e = special_of(cp)) return copy_mapping(e->title, out);
    }
    out[0] = to_title(cp);
    return 1;
}

std::size_t full_fold_case(char32_t cp, char32_t out[3]) noexcept {
    if (has_flag(cp, t::flag_special_case)) {
        if (const t::SpecialCaseEntry* e = special_of(cp)) return copy_mapping(e->fold, out);
    }
    out[0] = fold_case(cp);
    return 1;
}

void append_upper(StringView s, String& out) {
    out.reserve(out.size() + s.size());
    std::size_t pos = 0;
    while (pos < s.size()) {
        const unsigned char b = static_cast<unsigned char>(s[pos]);
        if (b < 0x80) {
            out.push_back((b >= 'a' && b <= 'z') ? static_cast<char>(b - 32) : static_cast<char>(b));
            ++pos;
            continue;
        }
        char32_t m[3];
        put_all(out, m, full_to_upper(decode_utf8(s.data(), s.size(), pos), m));
    }
}

void append_lower(StringView s, String& out) { append_lower(s, 0, s.size(), out); }

void append_lower(StringView s, std::size_t start, std::size_t end, String& out) {
    if (end > s.size()) end = s.size();
    if (start >= end) return;
    out.reserve(out.size() + (end - start));
    std::size_t pos = start;
    while (pos < end) {
        const unsigned char b = static_cast<unsigned char>(s[pos]);
        if (b < 0x80) {
            out.push_back((b >= 'A' && b <= 'Z') ? static_cast<char>(b + 32) : static_cast<char>(b));
            ++pos;
            continue;
        }
        const std::size_t cp_start = pos;
        const char32_t    cp       = decode_utf8(s.data(), s.size(), pos);
        append_lower_code_point(s, cp_start, pos, cp, out);
    }
}

void append_title(StringView s, String& out) {
    out.reserve(out.size() + s.size());
    std::size_t word = 0;
    while (word < s.size()) {
        const std::size_t end   = next_word_break(s, word);
        bool              found = false;
        std::size_t       pos   = word;
        while (pos < end) {
            const std::size_t start = pos;
            const char32_t    cp    = decode_utf8(s.data(), s.size(), pos);
            if (!found && is_cased(cp)) {
                found = true;
                char32_t m[3];
                put_all(out, m, full_to_title(cp, m));
            } else {
                append_lower_code_point(s, start, pos, cp, out);
            }
        }
        word = end;
    }
}

void append_folded(StringView s, String& out) {
    out.reserve(out.size() + s.size());
    std::size_t pos = 0;
    while (pos < s.size()) {
        const unsigned char b = static_cast<unsigned char>(s[pos]);
        if (b < 0x80) {
            out.push_back((b >= 'A' && b <= 'Z') ? static_cast<char>(b + 32) : static_cast<char>(b));
            ++pos;
            continue;
        }
        char32_t m[3];
        put_all(out, m, full_fold_case(decode_utf8(s.data(), s.size(), pos), m));
    }
}

String to_upper(StringView s) { String r; append_upper(s, r); return r; }
String to_lower(StringView s) { String r; append_lower(s, r); return r; }
String to_title(StringView s) { String r; append_title(s, r); return r; }
String fold_case(StringView s) { String r; append_folded(s, r); return r; }

namespace {

String caseless_key(StringView s) {
    if (is_ascii(s)) return fold_case(s);
    const String folded = fold_case(normalize(s, NormalizationForm::nfd));
    return normalize(folded, NormalizationForm::nfd);
}

} // namespace

int compare_ignore_case(StringView a, StringView b) {
    if (is_ascii(a) && is_ascii(b)) {
        const std::size_t n = a.size() < b.size() ? a.size() : b.size();
        for (std::size_t i = 0; i < n; ++i) {
            unsigned char x = static_cast<unsigned char>(a[i]);
            unsigned char y = static_cast<unsigned char>(b[i]);
            if (x >= 'A' && x <= 'Z') x = static_cast<unsigned char>(x + 32);
            if (y >= 'A' && y <= 'Z') y = static_cast<unsigned char>(y + 32);
            if (x != y) return x < y ? -1 : 1;
        }
        return a.size() < b.size() ? -1 : (a.size() > b.size() ? 1 : 0);
    }
    const int c = caseless_key(a).view().compare(caseless_key(b).view());
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

bool equals_ignore_case(StringView a, StringView b) { return compare_ignore_case(a, b) == 0; }

void append_normalized(StringView s, NormalizationForm form, String& out) {
    if (quick_check(s, form)) {
        out.append(s.data(), s.size());
        return;
    }
    const bool compat  = form == NormalizationForm::nfkc || form == NormalizationForm::nfkd;
    const bool compose_ = form == NormalizationForm::nfc || form == NormalizationForm::nfkc;
    std::vector<char32_t> v;
    v.reserve(s.size() + 8);
    std::size_t pos = 0;
    while (pos < s.size()) decompose_into(decode_utf8(s.data(), s.size(), pos), compat, v);
    canonical_order(v);
    if (compose_) compose(v);
    out.reserve(out.size() + v.size() + s.size() / 2);
    put_all(out, v.data(), v.size());
}

String normalize(StringView s, NormalizationForm form) {
    String r;
    append_normalized(s, form, r);
    return r;
}

bool is_normalized(StringView s, NormalizationForm form) {
    if (quick_check(s, form)) return true;
    if (!is_valid_utf8(s)) return false;
    return normalize(s, form).view() == s;
}

bool is_grapheme_boundary(StringView s, std::size_t pos) noexcept {
    if (pos == 0 || pos == s.size()) return true;
    if (pos > s.size() || !is_code_point_boundary(s, pos)) return false;
    const std::size_t a_pos = prev_code_point(s, pos);
    return grapheme_break_between(s, a_pos, code_point_at(s, a_pos), code_point_at(s, pos));
}

std::size_t next_grapheme_break(StringView s, std::size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    std::size_t a_pos = pos;
    char32_t    a     = code_point_at(s, pos);
    std::size_t i     = next_code_point(s, pos);
    while (i < s.size()) {
        if (static_cast<unsigned char>(s[i]) < 0x80 && static_cast<unsigned char>(s[a_pos]) < 0x80 && s[a_pos] != '\r') return i;
        const char32_t b = code_point_at(s, i);
        if (grapheme_break_between(s, a_pos, a, b)) return i;
        a_pos = i;
        a     = b;
        i     = next_code_point(s, i);
    }
    return s.size();
}

std::size_t prev_grapheme_break(StringView s, std::size_t pos) noexcept {
    if (pos == 0) return 0;
    if (pos > s.size()) pos = s.size();
    std::size_t i = prev_code_point(s, pos);
    while (i > 0 && !is_grapheme_boundary(s, i)) i = prev_code_point(s, i);
    return i;
}

std::size_t count_graphemes(StringView s) noexcept {
    std::size_t n   = 0;
    std::size_t pos = 0;
    while (pos < s.size()) {
        pos = next_grapheme_break(s, pos);
        ++n;
    }
    return n;
}

bool is_word_boundary(StringView s, std::size_t pos) noexcept {
    using W = WordBreak;
    if (pos == 0 || pos == s.size()) return true;
    if (pos > s.size() || !is_code_point_boundary(s, pos)) return false;
    const std::size_t ap = prev_code_point(s, pos);
    const char32_t    a  = code_point_at(s, ap);
    const char32_t    b  = code_point_at(s, pos);
    const W           ra = wb_of(a);
    const W           B  = wb_of(b);
    if (ra == W::cr && B == W::lf) return false;
    if (wb_newline(ra)) return true;
    if (wb_newline(B)) return true;
    if (ra == W::zwj && is_extended_pictographic(b)) return false;
    if (ra == W::w_seg_space && B == W::w_seg_space) return false;
    if (wb_ignorable(B)) return false;
    const std::size_t ea = wb_effective_start(s, ap);
    const W           A  = wb_of(code_point_at(s, ea));
    if (wb_ahletter(A) && wb_ahletter(B)) return false;
    if (wb_ahletter(A) && (B == W::mid_letter || wb_midnumletq(B)) && wb_ahletter(wb_next_effective(s, pos))) return false;
    if ((A == W::mid_letter || wb_midnumletq(A)) && wb_ahletter(B) && wb_ahletter(wb_prev_effective(s, ea))) return false;
    if (A == W::hebrew_letter && B == W::single_quote) return false;
    if (A == W::hebrew_letter && B == W::double_quote && wb_next_effective(s, pos) == W::hebrew_letter) return false;
    if (A == W::double_quote && B == W::hebrew_letter && wb_prev_effective(s, ea) == W::hebrew_letter) return false;
    if (A == W::numeric && B == W::numeric) return false;
    if (wb_ahletter(A) && B == W::numeric) return false;
    if (A == W::numeric && wb_ahletter(B)) return false;
    if ((A == W::mid_num || wb_midnumletq(A)) && B == W::numeric && wb_prev_effective(s, ea) == W::numeric) return false;
    if (A == W::numeric && (B == W::mid_num || wb_midnumletq(B)) && wb_next_effective(s, pos) == W::numeric) return false;
    if (A == W::katakana && B == W::katakana) return false;
    if ((wb_ahletter(A) || A == W::numeric || A == W::katakana || A == W::extend_num_let) && B == W::extend_num_let) return false;
    if (A == W::extend_num_let && (wb_ahletter(B) || B == W::numeric || B == W::katakana)) return false;
    if (A == W::regional_indicator && B == W::regional_indicator) {
        std::size_t count = 0;
        std::size_t q     = ea;
        for (;;) {
            const W w = wb_of(code_point_at(s, q));
            if (w == W::regional_indicator) ++count;
            else if (!wb_ignorable(w)) break;
            if (q == 0) break;
            q = prev_code_point(s, q);
        }
        return count % 2 == 0;
    }
    return true;
}

std::size_t next_word_break(StringView s, std::size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    std::size_t i = next_code_point(s, pos);
    while (i < s.size() && !is_word_boundary(s, i)) i = next_code_point(s, i);
    return i;
}

std::size_t prev_word_break(StringView s, std::size_t pos) noexcept {
    if (pos == 0) return 0;
    if (pos > s.size()) pos = s.size();
    std::size_t i = prev_code_point(s, pos);
    while (i > 0 && !is_word_boundary(s, i)) i = prev_code_point(s, i);
    return i;
}

bool WordIterator::is_word_like(StringView segment) noexcept {
    std::size_t pos = 0;
    while (pos < segment.size()) {
        const char32_t cp = decode_utf8(segment.data(), segment.size(), pos);
        if (is_alphanumeric(cp)) return true;
    }
    return false;
}

std::size_t grapheme_width(StringView cluster) noexcept {
    std::size_t pos   = 0;
    int         w     = 0;
    bool        vs16  = false;
    char32_t    first = 0;
    bool        have  = false;
    while (pos < cluster.size()) {
        const char32_t cp = decode_utf8(cluster.data(), cluster.size(), pos);
        if (!have) {
            first = cp;
            have  = true;
        }
        if (cp == 0xFE0F) vs16 = true;
        const int cw = rec(cp).width;
        if (cw > w) w = cw;
    }
    if (vs16 && w == 1 && is_emoji(first)) w = 2;
    return static_cast<std::size_t>(w);
}

std::size_t display_width(StringView s) noexcept {
    std::size_t total = 0;
    std::size_t pos   = 0;
    while (pos < s.size()) {
        const std::size_t end = next_grapheme_break(s, pos);
        total += grapheme_width(s.substring(pos, end - pos));
        pos = end;
    }
    return total;
}

} // namespace unicode
} // namespace fizmo
