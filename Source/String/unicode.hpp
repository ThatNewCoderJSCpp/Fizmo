#ifndef FIZMO_UNICODE_HPP
#define FIZMO_UNICODE_HPP

#include "string_view.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace fizmo {

class String;

namespace unicode {

enum class GeneralCategory : std::uint8_t {
    unassigned,
    uppercase_letter,
    lowercase_letter,
    titlecase_letter,
    modifier_letter,
    other_letter,
    nonspacing_mark,
    spacing_mark,
    enclosing_mark,
    decimal_number,
    letter_number,
    other_number,
    connector_punctuation,
    dash_punctuation,
    open_punctuation,
    close_punctuation,
    initial_punctuation,
    final_punctuation,
    other_punctuation,
    math_symbol,
    currency_symbol,
    modifier_symbol,
    other_symbol,
    space_separator,
    line_separator,
    paragraph_separator,
    control,
    format,
    surrogate,
    private_use
};

enum class NormalizationForm : std::uint8_t { nfc, nfd, nfkc, nfkd };

enum class GraphemeBreak : std::uint8_t {
    other, cr, lf, control, extend, zwj, regional_indicator, prepend, spacing_mark, l, v, t, lv, lvt
};

enum class WordBreak : std::uint8_t {
    other, cr, lf, newline, extend, zwj, regional_indicator, format, katakana, hebrew_letter, a_letter,
    single_quote, double_quote, mid_num_let, mid_letter, mid_num, numeric, extend_num_let, w_seg_space
};

constexpr char32_t replacement_character = 0xFFFD;
constexpr char32_t max_code_point        = 0x10FFFF;

const char* version() noexcept;

constexpr bool is_surrogate(char32_t cp) noexcept { return cp >= 0xD800 && cp <= 0xDFFF; }
constexpr bool is_valid_code_point(char32_t cp) noexcept { return cp <= max_code_point && !is_surrogate(cp); }

constexpr std::size_t utf8_length(char32_t cp) noexcept {
    if (!is_valid_code_point(cp)) return 3;
    return cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
}

constexpr bool is_continuation_byte(char c) noexcept { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

constexpr std::size_t encode_utf8(char32_t cp, char* out) noexcept {
    if (!is_valid_code_point(cp)) cp = replacement_character;
    if (cp < 0x80) {
        out[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        out[0] = static_cast<char>(0xC0 | (cp >> 6));
        out[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = static_cast<char>(0xE0 | (cp >> 12));
        out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = static_cast<char>(0xF0 | (cp >> 18));
    out[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
}

constexpr char32_t decode_utf8(const char* s, std::size_t n, std::size_t& pos) noexcept {
    const unsigned char b0 = static_cast<unsigned char>(s[pos]);
    if (b0 < 0x80) {
        ++pos;
        return b0;
    }
    std::size_t need = 0;
    char32_t    cp   = 0;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;
    if (b0 >= 0xC2 && b0 <= 0xDF) { need = 1; cp = b0 & 0x1F; }
    else if (b0 >= 0xE0 && b0 <= 0xEF) {
        need = 2;
        cp   = b0 & 0x0F;
        if (b0 == 0xE0) lo = 0xA0;
        if (b0 == 0xED) hi = 0x9F;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        need = 3;
        cp   = b0 & 0x07;
        if (b0 == 0xF0) lo = 0x90;
        if (b0 == 0xF4) hi = 0x8F;
    } else {
        ++pos;
        return replacement_character;
    }
    std::size_t i = pos + 1;
    for (std::size_t k = 0; k < need; ++k, ++i) {
        if (i >= n) {
            pos = i;
            return replacement_character;
        }
        const unsigned char b = static_cast<unsigned char>(s[i]);
        if (b < lo || b > hi) {
            pos = i;
            return replacement_character;
        }
        lo = 0x80;
        hi = 0xBF;
        cp = (cp << 6) | (b & 0x3F);
    }
    pos = i;
    return cp;
}

constexpr char32_t decode_utf8(StringView s, std::size_t& pos) noexcept { return decode_utf8(s.data(), s.size(), pos); }

constexpr char32_t code_point_at(StringView s, std::size_t pos) noexcept {
    if (pos >= s.size()) return 0;
    return decode_utf8(s.data(), s.size(), pos);
}

constexpr std::size_t next_code_point(StringView s, std::size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    decode_utf8(s.data(), s.size(), pos);
    return pos;
}

constexpr std::size_t prev_code_point(StringView s, std::size_t pos) noexcept {
    if (pos == 0) return 0;
    if (pos > s.size()) pos = s.size();
    std::size_t start = pos - 1;
    std::size_t limit = pos >= 4 ? pos - 4 : 0;
    while (start > limit && is_continuation_byte(s[start])) --start;
    std::size_t probe = start;
    decode_utf8(s.data(), s.size(), probe);
    return probe == pos ? start : pos - 1;
}

constexpr bool is_code_point_boundary(StringView s, std::size_t pos) noexcept {
    if (pos == 0 || pos == s.size()) return true;
    if (pos > s.size()) return false;
    if (!is_continuation_byte(s[pos])) return true;
    std::size_t start = pos;
    const std::size_t limit = pos >= 3 ? pos - 3 : 0;
    while (start > limit && is_continuation_byte(s[start - 1])) --start;
    if (start == 0 || is_continuation_byte(s[start - 1])) return true;
    std::size_t end = start - 1;
    decode_utf8(s.data(), s.size(), end);
    return end <= pos;
}

bool        is_valid_utf8(StringView s) noexcept;
std::size_t count_code_points(StringView s) noexcept;
std::size_t first_invalid_utf8(StringView s) noexcept;

GeneralCategory general_category(char32_t cp) noexcept;
GraphemeBreak   grapheme_break_property(char32_t cp) noexcept;
WordBreak       word_break_property(char32_t cp) noexcept;
std::uint8_t    combining_class(char32_t cp) noexcept;
int             code_point_width(char32_t cp) noexcept;

bool is_letter(char32_t cp) noexcept;
bool is_mark(char32_t cp) noexcept;
bool is_number(char32_t cp) noexcept;
bool is_digit(char32_t cp) noexcept;
bool is_punctuation(char32_t cp) noexcept;
bool is_symbol(char32_t cp) noexcept;
bool is_separator(char32_t cp) noexcept;
bool is_control(char32_t cp) noexcept;
bool is_private_use(char32_t cp) noexcept;
bool is_assigned(char32_t cp) noexcept;
bool is_alphabetic(char32_t cp) noexcept;
bool is_alphanumeric(char32_t cp) noexcept;
bool is_whitespace(char32_t cp) noexcept;
bool is_lowercase(char32_t cp) noexcept;
bool is_uppercase(char32_t cp) noexcept;
bool is_titlecase(char32_t cp) noexcept;
bool is_cased(char32_t cp) noexcept;
bool is_case_ignorable(char32_t cp) noexcept;
bool is_default_ignorable(char32_t cp) noexcept;
bool is_emoji(char32_t cp) noexcept;
bool is_emoji_presentation(char32_t cp) noexcept;
bool is_extended_pictographic(char32_t cp) noexcept;
bool is_regional_indicator(char32_t cp) noexcept;
bool is_printable(char32_t cp) noexcept;

char32_t to_upper(char32_t cp) noexcept;
char32_t to_lower(char32_t cp) noexcept;
char32_t to_title(char32_t cp) noexcept;
char32_t fold_case(char32_t cp) noexcept;

std::size_t full_to_upper(char32_t cp, char32_t out[3]) noexcept;
std::size_t full_to_lower(char32_t cp, char32_t out[3]) noexcept;
std::size_t full_to_title(char32_t cp, char32_t out[3]) noexcept;
std::size_t full_fold_case(char32_t cp, char32_t out[3]) noexcept;

String to_upper(StringView s);
String to_lower(StringView s);
String to_title(StringView s);
String fold_case(StringView s);

void append_upper(StringView s, String& out);
void append_lower(StringView s, String& out);
void append_lower(StringView context, std::size_t start, std::size_t end, String& out);
void append_title(StringView s, String& out);
void append_folded(StringView s, String& out);

int  compare_ignore_case(StringView a, StringView b);
bool equals_ignore_case(StringView a, StringView b);

String normalize(StringView s, NormalizationForm form);
void   append_normalized(StringView s, NormalizationForm form, String& out);
bool   is_normalized(StringView s, NormalizationForm form);

bool        is_grapheme_boundary(StringView s, std::size_t pos) noexcept;
std::size_t next_grapheme_break(StringView s, std::size_t pos) noexcept;
std::size_t prev_grapheme_break(StringView s, std::size_t pos) noexcept;
std::size_t count_graphemes(StringView s) noexcept;

bool        is_word_boundary(StringView s, std::size_t pos) noexcept;
std::size_t next_word_break(StringView s, std::size_t pos) noexcept;
std::size_t prev_word_break(StringView s, std::size_t pos) noexcept;

std::size_t grapheme_width(StringView cluster) noexcept;
std::size_t display_width(StringView s) noexcept;

class CodePointIterator {
private:
    StringView  m_text;
    std::size_t m_pos;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = char32_t;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = char32_t;

    constexpr CodePointIterator() noexcept : m_text(), m_pos(0) {}
    constexpr CodePointIterator(StringView text, std::size_t pos) noexcept : m_text(text), m_pos(pos) {}

    constexpr char32_t operator*() const noexcept { return code_point_at(m_text, m_pos); }
    constexpr std::size_t position() const noexcept { return m_pos; }
    constexpr std::size_t byte_length() const noexcept { return next_code_point(m_text, m_pos) - m_pos; }

    constexpr CodePointIterator& operator++() noexcept { m_pos = next_code_point(m_text, m_pos); return *this; }
    constexpr CodePointIterator  operator++(int) noexcept { CodePointIterator t(*this); ++*this; return t; }
    constexpr CodePointIterator& operator--() noexcept { m_pos = prev_code_point(m_text, m_pos); return *this; }
    constexpr CodePointIterator  operator--(int) noexcept { CodePointIterator t(*this); --*this; return t; }

    friend constexpr bool operator==(const CodePointIterator& a, const CodePointIterator& b) noexcept { return a.m_pos == b.m_pos; }
    friend constexpr bool operator!=(const CodePointIterator& a, const CodePointIterator& b) noexcept { return a.m_pos != b.m_pos; }
};

class CodePointView {
private:
    StringView m_text;

public:
    constexpr CodePointView() noexcept : m_text() {}
    constexpr explicit CodePointView(StringView text) noexcept : m_text(text) {}

    constexpr CodePointIterator begin() const noexcept { return CodePointIterator(m_text, 0); }
    constexpr CodePointIterator end() const noexcept { return CodePointIterator(m_text, m_text.size()); }
    constexpr StringView text() const noexcept { return m_text; }
    std::size_t size() const noexcept { return count_code_points(m_text); }
    constexpr bool empty() const noexcept { return m_text.empty(); }
};

class GraphemeIterator {
private:
    StringView  m_text;
    std::size_t m_pos;
    std::size_t m_next;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = StringView;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = StringView;

    GraphemeIterator() noexcept : m_text(), m_pos(0), m_next(0) {}
    GraphemeIterator(StringView text, std::size_t pos) noexcept : m_text(text), m_pos(pos), m_next(next_grapheme_break(text, pos)) {}

    StringView operator*() const noexcept { return m_text.substring(m_pos, m_next - m_pos); }
    std::size_t position() const noexcept { return m_pos; }

    GraphemeIterator& operator++() noexcept {
        m_pos  = m_next;
        m_next = next_grapheme_break(m_text, m_pos);
        return *this;
    }

    GraphemeIterator operator++(int) noexcept { GraphemeIterator t(*this); ++*this; return t; }

    GraphemeIterator& operator--() noexcept {
        m_next = m_pos;
        m_pos  = prev_grapheme_break(m_text, m_pos);
        return *this;
    }

    GraphemeIterator operator--(int) noexcept { GraphemeIterator t(*this); --*this; return t; }

    friend bool operator==(const GraphemeIterator& a, const GraphemeIterator& b) noexcept { return a.m_pos == b.m_pos; }
    friend bool operator!=(const GraphemeIterator& a, const GraphemeIterator& b) noexcept { return a.m_pos != b.m_pos; }
};

class GraphemeView {
private:
    StringView m_text;

public:
    GraphemeView() noexcept : m_text() {}
    explicit GraphemeView(StringView text) noexcept : m_text(text) {}

    GraphemeIterator begin() const noexcept { return GraphemeIterator(m_text, 0); }
    GraphemeIterator end() const noexcept { return GraphemeIterator(m_text, m_text.size()); }
    StringView text() const noexcept { return m_text; }
    std::size_t size() const noexcept { return count_graphemes(m_text); }
    bool empty() const noexcept { return m_text.empty(); }
};

class WordIterator {
private:
    StringView  m_text;
    std::size_t m_pos;
    std::size_t m_next;
    bool        m_words_only;

    void skip() noexcept {
        while (m_words_only && m_pos < m_text.size() && !is_word_like(m_text.substring(m_pos, m_next - m_pos))) {
            m_pos  = m_next;
            m_next = next_word_break(m_text, m_pos);
        }
    }

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = StringView;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = StringView;

    static bool is_word_like(StringView segment) noexcept;

    WordIterator() noexcept : m_text(), m_pos(0), m_next(0), m_words_only(false) {}
    WordIterator(StringView text, std::size_t pos, bool words_only) noexcept
        : m_text(text), m_pos(pos), m_next(next_word_break(text, pos)), m_words_only(words_only) { skip(); }

    StringView operator*() const noexcept { return m_text.substring(m_pos, m_next - m_pos); }
    std::size_t position() const noexcept { return m_pos; }

    WordIterator& operator++() noexcept {
        m_pos  = m_next;
        m_next = next_word_break(m_text, m_pos);
        skip();
        return *this;
    }

    WordIterator operator++(int) noexcept { WordIterator t(*this); ++*this; return t; }

    friend bool operator==(const WordIterator& a, const WordIterator& b) noexcept { return a.m_pos == b.m_pos; }
    friend bool operator!=(const WordIterator& a, const WordIterator& b) noexcept { return a.m_pos != b.m_pos; }
};

class WordView {
private:
    StringView m_text;
    bool       m_words_only;

public:
    WordView() noexcept : m_text(), m_words_only(true) {}
    explicit WordView(StringView text, bool words_only = true) noexcept : m_text(text), m_words_only(words_only) {}

    WordIterator begin() const noexcept { return WordIterator(m_text, 0, m_words_only); }
    WordIterator end() const noexcept { return WordIterator(m_text, m_text.size(), m_words_only); }
    StringView text() const noexcept { return m_text; }
};

constexpr CodePointView code_points(StringView s) noexcept { return CodePointView(s); }
inline GraphemeView     graphemes(StringView s) noexcept { return GraphemeView(s); }
inline WordView         words(StringView s, bool words_only = true) noexcept { return WordView(s, words_only); }

} // namespace unicode
} // namespace fizmo

#endif // FIZMO_UNICODE_HPP
