#ifndef FIZMO_REGEX_HPP
#define FIZMO_REGEX_HPP

#include "string_view.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fizmo {

enum class RegexFlags : unsigned {
    none = 0,
    ignore_case = 1,
    multiline = 2,
    dot_all = 4,
    extended = 8,
    unicode_classes = 16
};

constexpr RegexFlags operator|(RegexFlags a, RegexFlags b) noexcept { return static_cast<RegexFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }
constexpr RegexFlags operator&(RegexFlags a, RegexFlags b) noexcept { return static_cast<RegexFlags>(static_cast<unsigned>(a) & static_cast<unsigned>(b)); }
constexpr bool has_flag(RegexFlags flags, RegexFlags flag) noexcept { return (static_cast<unsigned>(flags) & static_cast<unsigned>(flag)) != 0; }

enum class CharKind : std::uint8_t {
    digits,
    numbers,
    letters,
    uppercase,
    lowercase,
    spaces,
    newlines,
    punctuation,
    symbols,
    special,
    emoji,
    control,
    ascii,
    hex_digits,
    word,
    non_ascii
};

class CharSet {
public:
    static CharSet everything();
    static CharSet nothing();
    static CharSet of(StringView chars);
    static CharSet range(char32_t first, char32_t last);
    static CharSet kind(CharKind kind);
    static CharSet digits();
    static CharSet numbers();
    static CharSet letters();
    static CharSet uppercase();
    static CharSet lowercase();
    static CharSet spaces();
    static CharSet punctuation();
    static CharSet symbols();
    static CharSet special();
    static CharSet emoji();
    static CharSet hex_digits();
    static CharSet word_characters();
    static CharSet ascii();

    CharSet();

    CharSet& allow(StringView chars);
    CharSet& disable(StringView chars);
    CharSet& allow_range(char32_t first, char32_t last);
    CharSet& disable_range(char32_t first, char32_t last);
    CharSet& allow(CharKind kind);
    CharSet& disable(CharKind kind, StringView keep = StringView());
    CharSet& allow(const CharSet& other);
    CharSet& disable(const CharSet& other);

    CharSet& allow_digits();
    CharSet& disable_digits();
    CharSet& allow_numbers();
    CharSet& disable_numbers();
    CharSet& allow_letters();
    CharSet& disable_letters();
    CharSet& allow_uppercase();
    CharSet& disable_uppercase();
    CharSet& allow_lowercase();
    CharSet& disable_lowercase();
    CharSet& allow_spaces();
    CharSet& disable_spaces();
    CharSet& allow_newlines();
    CharSet& disable_newlines();
    CharSet& allow_punctuation();
    CharSet& disable_punctuation(StringView keep = StringView());
    CharSet& allow_symbols();
    CharSet& disable_symbols(StringView keep = StringView());
    CharSet& allow_special();
    CharSet& disable_special(StringView keep = ".");
    CharSet& allow_emoji();
    CharSet& disable_emoji();
    CharSet& allow_control();
    CharSet& disable_control();
    CharSet& only_ascii();
    CharSet& allow_hex_digits();

    bool contains(char32_t cp) const noexcept;
    bool accepts(StringView text) const noexcept;
    std::size_t first_rejected(StringView text) const noexcept;
    std::string filter(StringView text) const;
    CharSet inverted() const;
    bool is_everything() const noexcept;
    bool is_nothing() const noexcept;

    std::string to_regex() const;
    std::string describe() const;

private:
    struct Item {
        enum class Type : std::uint8_t { chars, range, kind, set };
        Type type = Type::chars;
        std::u32string chars;
        char32_t first = 0;
        char32_t last = 0;
        CharKind kind = CharKind::digits;
        std::shared_ptr<const CharSet> set;
        std::u32string keep;
    };

    struct Rule {
        bool allow = true;
        Item item;
    };

    bool m_everything = false;
    std::vector<Rule> m_rules;

    static bool item_contains(const Item& item, char32_t cp) noexcept;
    static std::string item_regex(const Item& item);
    static std::string item_words(const Item& item);
    CharSet& add(bool allow, Item item);
};

class RegexMatch {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    RegexMatch() = default;

    std::size_t begin(std::size_t group = 0) const noexcept;
    std::size_t end(std::size_t group = 0) const noexcept;
    std::size_t length(std::size_t group = 0) const noexcept;
    bool matched(std::size_t group = 0) const noexcept;
    bool matched(StringView name) const noexcept;
    std::string_view text(std::size_t group = 0) const noexcept;
    std::string_view text(StringView name) const noexcept;
    std::string str(std::size_t group = 0) const;
    std::string str(StringView name) const;
    std::string_view operator[](std::size_t group) const noexcept { return text(group); }
    std::size_t group_count() const noexcept { return m_spans.size(); }
    std::optional<std::size_t> group_index(StringView name) const noexcept;
    std::string_view subject() const noexcept { return m_subject; }
    std::string_view before() const noexcept;
    std::string_view after() const noexcept;

private:
    friend class Regex;
    std::string_view m_subject;
    std::vector<std::pair<std::size_t, std::size_t>> m_spans;
    std::shared_ptr<const std::vector<std::string>> m_names;
};

class Regex {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    static constexpr std::size_t default_step_limit = 20000000;

    Regex();
    explicit Regex(StringView pattern, RegexFlags flags = RegexFlags::none);

    bool ok() const noexcept { return m_error.empty(); }
    explicit operator bool() const noexcept { return ok(); }
    const std::string& error() const noexcept { return m_error; }
    std::size_t error_position() const noexcept { return m_error_position; }
    const std::string& pattern() const noexcept { return m_pattern; }
    RegexFlags flags() const noexcept { return m_flags; }

    std::size_t group_count() const noexcept;
    const std::vector<std::string>& group_names() const noexcept;
    std::optional<std::size_t> group_index(StringView name) const noexcept;

    bool matches(StringView text) const;
    bool contains(StringView text) const;
    std::optional<RegexMatch> match(StringView text) const;
    std::optional<RegexMatch> match_at(StringView text, std::size_t position) const;
    std::optional<RegexMatch> search(StringView text, std::size_t start = 0) const;
    std::vector<RegexMatch> find_all(StringView text) const;
    std::size_t count(StringView text) const;

    std::string replace(StringView text, StringView replacement, std::size_t limit = npos) const;
    std::string replace(StringView text, const std::function<std::string(const RegexMatch&)>& replacement, std::size_t limit = npos) const;
    std::string replace_first(StringView text, StringView replacement) const;
    std::vector<std::string> split(StringView text, std::size_t limit = npos) const;

    std::string explain() const;

    void set_step_limit(std::size_t steps) noexcept { m_step_limit = steps; }
    std::size_t step_limit() const noexcept { return m_step_limit; }
    bool gave_up() const noexcept { return m_gave_up; }

    static std::string escape(StringView literal);
    static std::string expand(const RegexMatch& match, StringView replacement);

    struct Program;

private:
    std::optional<RegexMatch> run(StringView text, std::size_t start, bool anchored, bool whole) const;

    std::shared_ptr<const Program> m_program;
    std::string m_pattern;
    RegexFlags m_flags = RegexFlags::none;
    std::string m_error;
    std::size_t m_error_position = 0;
    std::size_t m_step_limit = default_step_limit;
    mutable bool m_gave_up = false;
};

class Pattern {
public:
    Pattern() = default;

    static Pattern text(StringView literal);
    static Pattern one_of(const CharSet& set);
    static Pattern one_of(StringView chars);
    static Pattern not_one_of(StringView chars);
    static Pattern any_character();
    static Pattern any_except_newline();
    static Pattern digit();
    static Pattern letter();
    static Pattern whitespace();
    static Pattern word_character();
    static Pattern start_of_text();
    static Pattern end_of_text();
    static Pattern start_of_line();
    static Pattern end_of_line();
    static Pattern word_boundary();
    static Pattern either(std::initializer_list<Pattern> options);
    static Pattern either(const std::vector<Pattern>& options);
    static Pattern sequence(std::initializer_list<Pattern> parts);
    static Pattern same_as(StringView group_name);
    static Pattern same_as(std::size_t group_number);
    static Pattern raw(StringView regex);

    static Pattern digits(std::size_t at_least = 1);
    static Pattern integer(bool allow_sign = true);
    static Pattern decimal(bool allow_sign = true);
    static Pattern identifier();
    static Pattern hex_number();
    static Pattern hex_color();
    static Pattern email();
    static Pattern word();

    Pattern& then(const Pattern& next);
    Pattern& then(StringView literal);
    Pattern& then_one_of(const CharSet& set);
    Pattern& or_else(const Pattern& other);

    Pattern optional() const;
    Pattern one_or_more() const;
    Pattern zero_or_more() const;
    Pattern repeat(std::size_t times) const;
    Pattern repeat(std::size_t min, std::size_t max) const;
    Pattern at_least(std::size_t times) const;
    Pattern at_most(std::size_t times) const;
    Pattern lazy() const;
    Pattern possessive() const;
    Pattern capture() const;
    Pattern capture(StringView name) const;
    Pattern group() const;
    Pattern followed_by(const Pattern& next) const;
    Pattern not_followed_by(const Pattern& next) const;
    Pattern preceded_by(const Pattern& before) const;
    Pattern not_preceded_by(const Pattern& before) const;
    Pattern whole() const;

    const std::string& to_regex() const noexcept { return m_regex; }
    Regex compile(RegexFlags flags = RegexFlags::none) const;
    bool matches(StringView text, RegexFlags flags = RegexFlags::none) const;
    std::string explain() const;
    bool empty() const noexcept { return m_regex.empty(); }

private:
    enum class Shape : std::uint8_t { empty, atom, sequence, alternation, quantified };

    Pattern(std::string regex, Shape shape) : m_regex(std::move(regex)), m_shape(shape) {}
    std::string as_atom() const;
    std::string as_part() const;
    Pattern quantify(const std::string& suffix) const;

    std::string m_regex;
    Shape m_shape = Shape::empty;
};

} // namespace fizmo

#endif // FIZMO_REGEX_HPP
