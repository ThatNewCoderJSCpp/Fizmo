#include "regex.hpp"
#include "unicode.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>

namespace fizmo {

namespace {

constexpr std::size_t no_pos = static_cast<std::size_t>(-1);
constexpr std::size_t unbounded = static_cast<std::size_t>(-1);
constexpr std::size_t max_repeat = 1000;
constexpr std::size_t max_program = 250000;

char32_t decode_at(std::string_view s, std::size_t pos, std::size_t& len) noexcept {
    const unsigned char c = static_cast<unsigned char>(s[pos]);
    if (c < 0x80) {
        len = 1;
        return c;
    }
    std::size_t n = 0;
    char32_t cp = 0;
    if ((c & 0xE0) == 0xC0) {
        n = 2;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4;
        cp = c & 0x07;
    } else {
        len = 1;
        return unicode::replacement_character;
    }
    if (pos + n > s.size()) {
        len = 1;
        return unicode::replacement_character;
    }
    for (std::size_t i = 1; i < n; ++i) {
        const unsigned char d = static_cast<unsigned char>(s[pos + i]);
        if ((d & 0xC0) != 0x80) {
            len = 1;
            return unicode::replacement_character;
        }
        cp = (cp << 6) | (d & 0x3F);
    }
    len = n;
    return cp;
}

std::size_t previous_start(std::string_view s, std::size_t pos) noexcept {
    if (pos == 0) return 0;
    std::size_t p = pos - 1;
    std::size_t steps = 0;
    while (p > 0 && (static_cast<unsigned char>(s[p]) & 0xC0) == 0x80 && steps < 3) {
        --p;
        ++steps;
    }
    return p;
}

std::size_t next_start(std::string_view s, std::size_t pos) noexcept {
    if (pos >= s.size()) return s.size() + 1;
    std::size_t len = 1;
    decode_at(s, pos, len);
    return pos + len;
}

void append_utf8(std::string& out, char32_t cp) {
    char buf[4];
    const std::size_t n = unicode::encode_utf8(cp, buf);
    out.append(buf, n == 0 ? 0 : n);
}

std::u32string decode_all(StringView text) {
    std::u32string out;
    const std::string_view s(text.data(), text.size());
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t len = 1;
        out.push_back(decode_at(s, i, len));
        i += len;
    }
    return out;
}

enum class Cat : std::uint8_t {
    digit,
    word,
    space,
    letter,
    upper,
    lower,
    title,
    modifier_letter,
    other_letter,
    mark,
    number,
    decimal,
    letter_number,
    other_number,
    punct,
    connector,
    dash,
    open_punct,
    close_punct,
    other_punct,
    symbol,
    math_symbol,
    currency,
    modifier_symbol,
    other_symbol,
    separator,
    space_separator,
    other,
    control,
    format,
    private_use,
    unassigned,
    emoji,
    ascii,
    alphabetic,
    alnum,
    any,
    hex,
    newline,
    white_space,
    printable,
    graph,
    blank
};

bool ascii_word(char32_t cp) noexcept { return (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_'; }

bool is_newline(char32_t cp) noexcept { return cp == '\n' || cp == '\r' || cp == 0x0B || cp == 0x0C || cp == 0x85 || cp == 0x2028 || cp == 0x2029; }

bool is_emoji_char(char32_t cp) noexcept {
    if (cp < 0x80) return false;
    return unicode::is_extended_pictographic(cp) || unicode::is_regional_indicator(cp) || unicode::is_emoji_presentation(cp);
}

bool in_cat(Cat cat, char32_t cp, bool uni) noexcept {
    using G = unicode::GeneralCategory;
    const G g = unicode::general_category(cp);
    switch (cat) {
        case Cat::digit: return uni ? g == G::decimal_number : (cp >= '0' && cp <= '9');
        case Cat::word: return uni ? (unicode::is_alphanumeric(cp) || cp == '_' || unicode::is_mark(cp) || g == G::connector_punctuation) : ascii_word(cp);
        case Cat::space: return uni ? unicode::is_whitespace(cp) : (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0x0B || cp == 0x0C);
        case Cat::letter: return unicode::is_letter(cp);
        case Cat::upper: return g == G::uppercase_letter;
        case Cat::lower: return g == G::lowercase_letter;
        case Cat::title: return g == G::titlecase_letter;
        case Cat::modifier_letter: return g == G::modifier_letter;
        case Cat::other_letter: return g == G::other_letter;
        case Cat::mark: return unicode::is_mark(cp);
        case Cat::number: return unicode::is_number(cp);
        case Cat::decimal: return g == G::decimal_number;
        case Cat::letter_number: return g == G::letter_number;
        case Cat::other_number: return g == G::other_number;
        case Cat::punct: return unicode::is_punctuation(cp);
        case Cat::connector: return g == G::connector_punctuation;
        case Cat::dash: return g == G::dash_punctuation;
        case Cat::open_punct: return g == G::open_punctuation;
        case Cat::close_punct: return g == G::close_punctuation;
        case Cat::other_punct: return g == G::other_punctuation;
        case Cat::symbol: return unicode::is_symbol(cp);
        case Cat::math_symbol: return g == G::math_symbol;
        case Cat::currency: return g == G::currency_symbol;
        case Cat::modifier_symbol: return g == G::modifier_symbol;
        case Cat::other_symbol: return g == G::other_symbol;
        case Cat::separator: return unicode::is_separator(cp);
        case Cat::space_separator: return g == G::space_separator;
        case Cat::other: return g == G::control || g == G::format || g == G::surrogate || g == G::private_use || g == G::unassigned;
        case Cat::control: return g == G::control;
        case Cat::format: return g == G::format;
        case Cat::private_use: return g == G::private_use;
        case Cat::unassigned: return g == G::unassigned;
        case Cat::emoji: return is_emoji_char(cp);
        case Cat::ascii: return cp < 0x80;
        case Cat::alphabetic: return unicode::is_alphabetic(cp);
        case Cat::alnum: return uni ? unicode::is_alphanumeric(cp) : ((cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z'));
        case Cat::any: return true;
        case Cat::hex: return (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'f') || (cp >= 'A' && cp <= 'F');
        case Cat::newline: return is_newline(cp);
        case Cat::white_space: return unicode::is_whitespace(cp);
        case Cat::printable: return unicode::is_printable(cp);
        case Cat::graph: return unicode::is_printable(cp) && !unicode::is_whitespace(cp);
        case Cat::blank: return cp == ' ' || cp == '\t' || g == G::space_separator;
    }
    return false;
}

std::string lower_ascii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == '_' || c == ' ' || c == '-'; }), s.end());
    return s;
}

std::optional<Cat> cat_named(const std::string& raw) {
    static const std::vector<std::pair<const char*, Cat>> names = {
        { "l", Cat::letter }, { "letter", Cat::letter }, { "lu", Cat::upper }, { "uppercaseletter", Cat::upper }, { "upper", Cat::upper }, { "uppercase", Cat::upper },
        { "ll", Cat::lower }, { "lowercaseletter", Cat::lower }, { "lower", Cat::lower }, { "lowercase", Cat::lower }, { "lt", Cat::title }, { "titlecaseletter", Cat::title },
        { "lm", Cat::modifier_letter }, { "modifierletter", Cat::modifier_letter }, { "lo", Cat::other_letter }, { "otherletter", Cat::other_letter },
        { "m", Cat::mark }, { "mark", Cat::mark }, { "n", Cat::number }, { "number", Cat::number }, { "nd", Cat::decimal }, { "decimalnumber", Cat::decimal },
        { "digit", Cat::decimal }, { "nl", Cat::letter_number }, { "letternumber", Cat::letter_number }, { "no", Cat::other_number }, { "othernumber", Cat::other_number },
        { "p", Cat::punct }, { "punctuation", Cat::punct }, { "punct", Cat::punct }, { "pc", Cat::connector }, { "connectorpunctuation", Cat::connector },
        { "pd", Cat::dash }, { "dashpunctuation", Cat::dash }, { "ps", Cat::open_punct }, { "openpunctuation", Cat::open_punct }, { "pe", Cat::close_punct },
        { "closepunctuation", Cat::close_punct }, { "po", Cat::other_punct }, { "otherpunctuation", Cat::other_punct }, { "s", Cat::symbol }, { "symbol", Cat::symbol },
        { "sm", Cat::math_symbol }, { "mathsymbol", Cat::math_symbol }, { "sc", Cat::currency }, { "currencysymbol", Cat::currency }, { "sk", Cat::modifier_symbol },
        { "modifiersymbol", Cat::modifier_symbol }, { "so", Cat::other_symbol }, { "othersymbol", Cat::other_symbol }, { "z", Cat::separator }, { "separator", Cat::separator },
        { "zs", Cat::space_separator }, { "spaceseparator", Cat::space_separator }, { "c", Cat::other }, { "other", Cat::other }, { "cc", Cat::control }, { "control", Cat::control },
        { "cntrl", Cat::control }, { "cf", Cat::format }, { "format", Cat::format }, { "co", Cat::private_use }, { "privateuse", Cat::private_use }, { "cn", Cat::unassigned },
        { "unassigned", Cat::unassigned }, { "emoji", Cat::emoji }, { "extendedpictographic", Cat::emoji }, { "ascii", Cat::ascii }, { "alphabetic", Cat::alphabetic },
        { "alpha", Cat::alphabetic }, { "alnum", Cat::alnum }, { "any", Cat::any }, { "hexdigit", Cat::hex }, { "xdigit", Cat::hex }, { "whitespace", Cat::white_space },
        { "space", Cat::white_space }, { "word", Cat::word }, { "newline", Cat::newline }, { "print", Cat::printable }, { "graph", Cat::graph }, { "blank", Cat::blank },
    };
    const std::string name = lower_ascii(raw);
    for (const auto& [n, c] : names) {
        if (name == n) return c;
    }
    return std::nullopt;
}

const char* cat_words(Cat cat, bool uni) {
    switch (cat) {
        case Cat::digit: return uni ? "a digit" : "a digit (0-9)";
        case Cat::word: return uni ? "a letter, digit or underscore" : "a letter, digit or underscore (a-z, A-Z, 0-9, _)";
        case Cat::space: return "a space, tab or line break";
        case Cat::letter: return "a letter";
        case Cat::upper: return "an uppercase letter";
        case Cat::lower: return "a lowercase letter";
        case Cat::title: return "a titlecase letter";
        case Cat::modifier_letter: return "a modifier letter";
        case Cat::other_letter: return "a letter without case";
        case Cat::mark: return "a combining mark";
        case Cat::number: return "a number character";
        case Cat::decimal: return "a decimal digit in any script";
        case Cat::letter_number: return "a letter-like number";
        case Cat::other_number: return "a number like \xC2\xBD or \xC2\xB2";
        case Cat::punct: return "punctuation";
        case Cat::connector: return "connecting punctuation like _";
        case Cat::dash: return "a dash";
        case Cat::open_punct: return "an opening bracket";
        case Cat::close_punct: return "a closing bracket";
        case Cat::other_punct: return "other punctuation";
        case Cat::symbol: return "a symbol";
        case Cat::math_symbol: return "a math symbol";
        case Cat::currency: return "a currency symbol";
        case Cat::modifier_symbol: return "a modifier symbol";
        case Cat::other_symbol: return "an other symbol";
        case Cat::separator: return "a separator";
        case Cat::space_separator: return "a space character";
        case Cat::other: return "a control, format or unassigned character";
        case Cat::control: return "a control character";
        case Cat::format: return "a format character";
        case Cat::private_use: return "a private-use character";
        case Cat::unassigned: return "an unassigned character";
        case Cat::emoji: return "an emoji";
        case Cat::ascii: return "an ASCII character";
        case Cat::alphabetic: return "an alphabetic character";
        case Cat::alnum: return "a letter or digit";
        case Cat::any: return "any character";
        case Cat::hex: return "a hex digit (0-9, a-f, A-F)";
        case Cat::newline: return "a line break";
        case Cat::white_space: return "whitespace";
        case Cat::printable: return "a printable character";
        case Cat::graph: return "a visible character";
        case Cat::blank: return "a space or tab";
    }
    return "a character";
}

std::string quote_char(char32_t cp) {
    switch (cp) {
        case '\n': return "a line feed (\\n)";
        case '\r': return "a carriage return (\\r)";
        case '\t': return "a tab (\\t)";
        case ' ': return "a space";
        case 0: return "a null character";
        default: break;
    }
    const char* q = cp == '\'' ? "\"" : "'";
    std::string out = q;
    if (cp < 0x20) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "\\x%02X", static_cast<unsigned>(cp));
        out += buf;
    } else {
        append_utf8(out, cp);
    }
    return out + q;
}

struct ClassNode {
    enum class Type : std::uint8_t { set, range, category, subtract, intersect };
    Type type = Type::set;
    bool negated = false;
    char32_t first = 0;
    char32_t last = 0;
    Cat cat = Cat::any;
    std::vector<ClassNode> kids;
};

bool class_test(const ClassNode& n, char32_t cp, bool uni) noexcept {
    bool hit = false;
    switch (n.type) {
        case ClassNode::Type::range: hit = cp >= n.first && cp <= n.last; break;
        case ClassNode::Type::category: hit = in_cat(n.cat, cp, uni); break;
        case ClassNode::Type::set:
            for (const ClassNode& k : n.kids) {
                if (class_test(k, cp, uni)) {
                    hit = true;
                    break;
                }
            }
            break;
        case ClassNode::Type::subtract:
            hit = !n.kids.empty() && class_test(n.kids[0], cp, uni);
            for (std::size_t i = 1; hit && i < n.kids.size(); ++i) {
                if (class_test(n.kids[i], cp, uni)) hit = false;
            }
            break;
        case ClassNode::Type::intersect:
            hit = !n.kids.empty();
            for (const ClassNode& k : n.kids) {
                if (!class_test(k, cp, uni)) {
                    hit = false;
                    break;
                }
            }
            break;
    }
    return hit != n.negated;
}

bool class_match(const ClassNode& n, char32_t cp, bool icase, bool uni) noexcept {
    if (class_test(n, cp, uni)) return true;
    if (!icase) return false;
    const char32_t alts[3] = { unicode::to_lower(cp), unicode::to_upper(cp), unicode::fold_case(cp) };
    for (char32_t a : alts) {
        if (a != cp && class_test(n, a, uni)) return true;
    }
    return false;
}

std::string class_words(const ClassNode& n, bool uni);

std::string join_words(const std::vector<std::string>& parts, const char* last_word) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += (i + 1 == parts.size()) ? std::string(" ") + last_word + " " : ", ";
        out += parts[i];
    }
    return out;
}

std::string class_words(const ClassNode& n, bool uni) {
    std::string body;
    switch (n.type) {
        case ClassNode::Type::range:
            body = n.first == n.last ? quote_char(n.first) : quote_char(n.first) + " to " + quote_char(n.last);
            break;
        case ClassNode::Type::category: body = cat_words(n.cat, uni); break;
        case ClassNode::Type::set: {
            std::vector<std::string> parts;
            for (const ClassNode& k : n.kids) parts.push_back(class_words(k, uni));
            body = parts.empty() ? "nothing" : join_words(parts, "or");
            break;
        }
        case ClassNode::Type::subtract: {
            std::vector<std::string> rest;
            for (std::size_t i = 1; i < n.kids.size(); ++i) rest.push_back(class_words(n.kids[i], uni));
            body = (n.kids.empty() ? std::string("nothing") : class_words(n.kids[0], uni)) + ", but not " + join_words(rest, "or");
            break;
        }
        case ClassNode::Type::intersect: {
            std::vector<std::string> parts;
            for (const ClassNode& k : n.kids) parts.push_back(class_words(k, uni));
            body = join_words(parts, "that is also");
            break;
        }
    }
    if (!n.negated) return body;
    if (n.type == ClassNode::Type::set && n.kids.empty()) return "any character";
    return "any character except " + body;
}

enum class AssertKind : std::uint8_t { line_start, line_end, text_start, text_end, text_end_newline, word_boundary, not_word_boundary };

struct Node {
    enum class Type : std::uint8_t { empty, literal, set, any, concat, alternate, repeat, group, assertion, backref, look, atomic };
    Type type = Type::empty;
    char32_t ch = 0;
    std::size_t set = 0;
    bool icase = false;
    bool dot_all = false;
    bool multiline = false;
    bool uni = false;
    std::size_t min = 0;
    std::size_t max = 0;
    bool greedy = true;
    bool possessive = false;
    std::size_t group = no_pos;
    std::string name;
    AssertKind assertion = AssertKind::line_start;
    bool behind = false;
    bool negate = false;
    std::vector<Node> kids;
};

enum class Op : std::uint8_t { literal, set, any, split, jump, save, mark, check, assertion, backref, look, atomic, at_target, match };

struct Inst {
    Op op = Op::match;
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    char32_t ch = 0;
    bool icase = false;
    bool flag = false;
    bool flag2 = false;
    std::size_t width = unbounded;
};

struct Sub {
    std::vector<Inst> code;
};

struct Flags {
    bool icase = false;
    bool multiline = false;
    bool dot_all = false;
    bool extended = false;
    bool uni = false;
};

} // namespace

struct Regex::Program {
    std::vector<Sub> subs;
    std::vector<ClassNode> classes;
    std::size_t groups = 1;
    std::size_t regs = 0;
    std::vector<std::string> names;
    Node ast;
    bool anchored = false;
    bool uni = false;
    std::optional<char> first_byte;
};

namespace {

class Parser {
public:
    Parser(std::string_view pattern, RegexFlags flags, Regex::Program& program) : m_p(pattern), m_prog(program) {
        m_flags.icase = has_flag(flags, RegexFlags::ignore_case);
        m_flags.multiline = has_flag(flags, RegexFlags::multiline);
        m_flags.dot_all = has_flag(flags, RegexFlags::dot_all);
        m_flags.extended = has_flag(flags, RegexFlags::extended);
        m_flags.uni = has_flag(flags, RegexFlags::unicode_classes);
        m_prog.names.assign(1, std::string());
    }

    bool parse(Node& out) {
        out = alternation();
        if (failed()) return false;
        if (m_i < m_p.size()) {
            fail(m_p[m_i] == ')' ? "this ) has no ( to close" : "unexpected character", m_i);
            return false;
        }
        for (const auto& [index, at] : m_backrefs) {
            if (index >= m_prog.groups) {
                fail("there is no group " + std::to_string(index) + " to refer back to", at);
                return false;
            }
        }
        for (const auto& [name, at] : m_named_backrefs) {
            if (std::find(m_prog.names.begin(), m_prog.names.end(), name) == m_prog.names.end()) {
                fail("there is no group named '" + name + "' to refer back to", at);
                return false;
            }
        }
        return true;
    }

    bool failed() const noexcept { return !m_error.empty(); }
    const std::string& error() const noexcept { return m_error; }
    std::size_t error_position() const noexcept { return m_error_at; }

private:
    void fail(const std::string& message, std::size_t at) {
        if (m_error.empty()) {
            m_error = message;
            m_error_at = at;
        }
    }

    bool more() const noexcept { return m_i < m_p.size(); }
    char peek(std::size_t ahead = 0) const noexcept { return m_i + ahead < m_p.size() ? m_p[m_i + ahead] : '\0'; }

    char32_t take_cp() {
        std::size_t len = 1;
        const char32_t cp = decode_at(m_p, m_i, len);
        m_i += len;
        return cp;
    }

    void skip_extended() {
        if (!m_flags.extended) return;
        while (more()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
                ++m_i;
            } else if (c == '#') {
                while (more() && peek() != '\n') ++m_i;
            } else {
                break;
            }
        }
    }

    Node make(Node::Type type) const {
        Node n;
        n.type = type;
        n.icase = m_flags.icase;
        n.dot_all = m_flags.dot_all;
        n.multiline = m_flags.multiline;
        n.uni = m_flags.uni;
        return n;
    }

    Node alternation() {
        std::vector<Node> options;
        options.push_back(sequence());
        while (!failed() && more() && peek() == '|') {
            ++m_i;
            options.push_back(sequence());
        }
        if (options.size() == 1) return std::move(options[0]);
        Node n = make(Node::Type::alternate);
        n.kids = std::move(options);
        return n;
    }

    Node sequence() {
        Node n = make(Node::Type::concat);
        while (!failed()) {
            skip_extended();
            if (!more() || peek() == '|' || peek() == ')') break;
            const std::size_t at = m_i;
            std::optional<Node> atom_node = atom();
            if (failed()) break;
            if (!atom_node) continue;
            Node piece = quantifier(std::move(*atom_node), at);
            if (failed()) break;
            n.kids.push_back(std::move(piece));
        }
        if (n.kids.size() == 1) return std::move(n.kids[0]);
        if (n.kids.empty()) return make(Node::Type::empty);
        return n;
    }

    bool read_number(std::size_t& out) {
        const std::size_t start = m_i;
        std::size_t v = 0;
        while (more() && peek() >= '0' && peek() <= '9') {
            v = v * 10 + static_cast<std::size_t>(peek() - '0');
            if (v > 100000) v = 100000;
            ++m_i;
        }
        out = v;
        return m_i > start;
    }

    Node quantifier(Node atom_node, std::size_t atom_at) {
        skip_extended();
        if (!more()) return atom_node;
        const char c = peek();
        std::size_t min = 0;
        std::size_t max = 0;
        const std::size_t at = m_i;
        if (c == '*') {
            min = 0;
            max = unbounded;
            ++m_i;
        } else if (c == '+') {
            min = 1;
            max = unbounded;
            ++m_i;
        } else if (c == '?') {
            min = 0;
            max = 1;
            ++m_i;
        } else if (c == '{') {
            const std::size_t save = m_i;
            ++m_i;
            std::size_t a = 0;
            std::size_t b = 0;
            const bool has_a = read_number(a);
            bool ok = true;
            if (more() && peek() == ',') {
                ++m_i;
                const bool has_b = read_number(b);
                if (!has_b) b = unbounded;
                if (!has_a) a = 0;
                if (!has_a && !has_b) ok = false;
            } else {
                b = a;
                if (!has_a) ok = false;
            }
            if (!ok || !more() || peek() != '}') {
                m_i = save;
                return atom_node;
            }
            ++m_i;
            if (b != unbounded && a > b) {
                fail("the smallest count is bigger than the largest in {" + std::to_string(a) + "," + std::to_string(b) + "}", at);
                return atom_node;
            }
            if (a > max_repeat || (b != unbounded && b > max_repeat)) {
                fail("counts above " + std::to_string(max_repeat) + " aren't supported", at);
                return atom_node;
            }
            min = a;
            max = b;
        } else {
            return atom_node;
        }
        if (atom_node.type == Node::Type::assertion || atom_node.type == Node::Type::look || atom_node.type == Node::Type::empty) {
            if (atom_node.type == Node::Type::empty) {
                fail("there is nothing before this to repeat", at);
                return atom_node;
            }
        }
        (void)atom_at;
        Node r = make(Node::Type::repeat);
        r.min = min;
        r.max = max;
        if (more() && peek() == '?') {
            r.greedy = false;
            ++m_i;
        } else if (more() && peek() == '+') {
            r.possessive = true;
            ++m_i;
        }
        r.kids.push_back(std::move(atom_node));
        skip_extended();
        if (more() && (peek() == '*' || peek() == '+' || peek() == '?')) {
            fail("this repeats something that already repeats; wrap it in (?: ) first", m_i);
        } else if (more() && peek() == '{') {
            const std::size_t save = m_i;
            ++m_i;
            std::size_t dummy = 0;
            const bool number = read_number(dummy);
            m_i = save;
            if (number) fail("this repeats something that already repeats; wrap it in (?: ) first", m_i);
        }
        return r;
    }

    bool parse_flag_letters(Flags& flags, bool& any) {
        bool on = true;
        any = false;
        while (more()) {
            const char c = peek();
            if (c == '-') {
                on = false;
                ++m_i;
                continue;
            }
            bool* target = nullptr;
            if (c == 'i') target = &flags.icase;
            else if (c == 'm') target = &flags.multiline;
            else if (c == 's') target = &flags.dot_all;
            else if (c == 'x') target = &flags.extended;
            else if (c == 'u') target = &flags.uni;
            else break;
            *target = on;
            any = true;
            ++m_i;
        }
        return true;
    }

    std::string read_name(char close) {
        std::string name;
        while (more() && peek() != close) {
            const char c = peek();
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
                fail("a group name can only have letters, digits and _", m_i);
                return name;
            }
            name += c;
            ++m_i;
        }
        if (!more()) {
            fail("this group name is never closed with " + std::string(1, close), m_i);
            return name;
        }
        ++m_i;
        if (name.empty()) fail("this group needs a name", m_i);
        else if (std::isdigit(static_cast<unsigned char>(name[0]))) fail("a group name can't start with a digit", m_i);
        return name;
    }

    std::optional<Node> group() {
        const std::size_t open = m_i;
        ++m_i;
        Flags saved = m_flags;
        Node result;
        if (peek() == '?') {
            ++m_i;
            const char c = peek();
            if (c == '#') {
                while (more() && peek() != ')') ++m_i;
                if (!more()) {
                    fail("this comment is never closed with )", open);
                    return std::nullopt;
                }
                ++m_i;
                return std::nullopt;
            }
            if (c == ':' || c == '>' || c == '=' || c == '!') {
                ++m_i;
                Node inner = alternation();
                if (c == ':') {
                    result = std::move(inner);
                } else if (c == '>') {
                    result = make(Node::Type::atomic);
                    result.kids.push_back(std::move(inner));
                } else {
                    result = make(Node::Type::look);
                    result.negate = c == '!';
                    result.kids.push_back(std::move(inner));
                }
            } else if (c == '<' && (peek(1) == '=' || peek(1) == '!')) {
                const bool negate = peek(1) == '!';
                m_i += 2;
                Node inner = alternation();
                result = make(Node::Type::look);
                result.behind = true;
                result.negate = negate;
                result.kids.push_back(std::move(inner));
            } else if (c == '<' || c == '\'' || (c == 'P' && peek(1) == '<')) {
                if (c == 'P') ++m_i;
                const char close = peek() == '\'' ? '\'' : '>';
                ++m_i;
                const std::string name = read_name(close);
                if (failed()) return std::nullopt;
                if (std::find(m_prog.names.begin(), m_prog.names.end(), name) != m_prog.names.end()) {
                    fail("two groups are named '" + name + "'", open);
                    return std::nullopt;
                }
                const std::size_t index = m_prog.groups++;
                m_prog.names.push_back(name);
                Node inner = alternation();
                result = make(Node::Type::group);
                result.group = index;
                result.name = name;
                result.kids.push_back(std::move(inner));
            } else {
                Flags flags = m_flags;
                bool any = false;
                parse_flag_letters(flags, any);
                if (!any && peek() != ')' && peek() != ':') {
                    fail("unknown group type (?" + std::string(1, c) + "; use (?: ), (?<name> ), (?= ), (?! ), (?<= ), (?<! ) or (?> )", open);
                    return std::nullopt;
                }
                if (peek() == ')') {
                    ++m_i;
                    m_flags = flags;
                    return std::nullopt;
                }
                if (peek() != ':') {
                    fail("expected : or ) after the flags", m_i);
                    return std::nullopt;
                }
                ++m_i;
                m_flags = flags;
                result = alternation();
            }
        } else {
            const std::size_t index = m_prog.groups++;
            m_prog.names.push_back(std::string());
            Node inner = alternation();
            result = make(Node::Type::group);
            result.group = index;
            result.kids.push_back(std::move(inner));
        }
        m_flags = saved;
        if (failed()) return std::nullopt;
        if (!more() || peek() != ')') {
            fail("this ( is never closed", open);
            return std::nullopt;
        }
        ++m_i;
        return result;
    }

    std::optional<char32_t> hex_escape(std::size_t digits, std::size_t at) {
        char32_t v = 0;
        if (peek() == '{') {
            ++m_i;
            std::size_t n = 0;
            while (more() && std::isxdigit(static_cast<unsigned char>(peek()))) {
                v = v * 16 + static_cast<char32_t>(std::isdigit(static_cast<unsigned char>(peek())) ? peek() - '0' : (std::tolower(static_cast<unsigned char>(peek())) - 'a' + 10));
                ++m_i;
                if (++n > 8) break;
            }
            if (!more() || peek() != '}' || n == 0) {
                fail("expected hex digits and } here", at);
                return std::nullopt;
            }
            ++m_i;
        } else {
            for (std::size_t k = 0; k < digits; ++k) {
                if (!more() || !std::isxdigit(static_cast<unsigned char>(peek()))) {
                    fail("expected " + std::to_string(digits) + " hex digits here", at);
                    return std::nullopt;
                }
                v = v * 16 + static_cast<char32_t>(std::isdigit(static_cast<unsigned char>(peek())) ? peek() - '0' : (std::tolower(static_cast<unsigned char>(peek())) - 'a' + 10));
                ++m_i;
            }
        }
        if (v > unicode::max_code_point) {
            fail("that code point is past U+10FFFF", at);
            return std::nullopt;
        }
        return v;
    }

    std::optional<ClassNode> category_escape(char c, std::size_t at) {
        ClassNode n;
        n.type = ClassNode::Type::category;
        switch (c) {
            case 'd': n.cat = Cat::digit; return n;
            case 'D': n.cat = Cat::digit; n.negated = true; return n;
            case 'w': n.cat = Cat::word; return n;
            case 'W': n.cat = Cat::word; n.negated = true; return n;
            case 's': n.cat = Cat::space; return n;
            case 'S': n.cat = Cat::space; n.negated = true; return n;
            case 'h': n.cat = Cat::blank; return n;
            case 'H': n.cat = Cat::blank; n.negated = true; return n;
            case 'R': n.cat = Cat::newline; return n;
            case 'p':
            case 'P': {
                n.negated = c == 'P';
                std::string name;
                if (peek() == '{') {
                    ++m_i;
                    while (more() && peek() != '}') name += m_p[m_i++];
                    if (!more()) {
                        fail("\\" + std::string(1, c) + "{ is never closed with }", at);
                        return std::nullopt;
                    }
                    ++m_i;
                } else if (more()) {
                    name = std::string(1, m_p[m_i++]);
                }
                bool negate_inside = false;
                if (!name.empty() && name[0] == '^') {
                    negate_inside = true;
                    name.erase(name.begin());
                }
                const std::size_t eq = name.find('=');
                if (eq != std::string::npos) name = name.substr(eq + 1);
                const std::optional<Cat> cat = cat_named(name);
                if (!cat) {
                    fail("unknown character property '" + name + "' (try L, Lu, Ll, N, Nd, P, S, Z, Emoji, ASCII)", at);
                    return std::nullopt;
                }
                n.cat = *cat;
                if (negate_inside) n.negated = !n.negated;
                return n;
            }
            default: return std::nullopt;
        }
    }

    std::optional<char32_t> simple_escape(char c, std::size_t at) {
        switch (c) {
            case 't': return U'\t';
            case 'n': return U'\n';
            case 'r': return U'\r';
            case 'f': return U'\f';
            case 'v': return U'\v';
            case 'a': return U'\a';
            case 'e': return char32_t(0x1B);
            case '0': return char32_t(0);
            case 'x': return hex_escape(2, at);
            case 'u': return hex_escape(4, at);
            case 'U': return hex_escape(8, at);
            case 'c': {
                if (!more() || !std::isalpha(static_cast<unsigned char>(peek()))) {
                    fail("\\c needs a letter after it", at);
                    return std::nullopt;
                }
                const char letter = m_p[m_i++];
                return static_cast<char32_t>(std::toupper(static_cast<unsigned char>(letter)) - 'A' + 1);
            }
            default: break;
        }
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 0x80 || !std::isalnum(u)) {
            --m_i;
            return take_cp();
        }
        return std::nullopt;
    }

    std::optional<Node> escape() {
        const std::size_t at = m_i;
        ++m_i;
        if (!more()) {
            fail("the pattern can't end with a lone \\", at);
            return std::nullopt;
        }
        const char c = m_p[m_i++];
        Node n = make(Node::Type::assertion);
        switch (c) {
            case 'b': n.assertion = AssertKind::word_boundary; return n;
            case 'B': n.assertion = AssertKind::not_word_boundary; return n;
            case 'A': n.assertion = AssertKind::text_start; return n;
            case 'z': n.assertion = AssertKind::text_end; return n;
            case 'Z': n.assertion = AssertKind::text_end_newline; return n;
            default: break;
        }
        if (c >= '1' && c <= '9') {
            std::size_t index = static_cast<std::size_t>(c - '0');
            if (more() && peek() >= '0' && peek() <= '9' && index * 10 + static_cast<std::size_t>(peek() - '0') < m_prog.groups) {
                index = index * 10 + static_cast<std::size_t>(m_p[m_i++] - '0');
            }
            Node r = make(Node::Type::backref);
            r.group = index;
            m_backrefs.emplace_back(index, at);
            return r;
        }
        if (c == 'k') {
            const char open = peek();
            if (open != '<' && open != '{' && open != '\'') {
                fail("\\k needs a group name like \\k<name>", at);
                return std::nullopt;
            }
            ++m_i;
            const std::string name = read_name(open == '<' ? '>' : open == '{' ? '}' : '\'');
            if (failed()) return std::nullopt;
            Node r = make(Node::Type::backref);
            r.name = name;
            m_named_backrefs.emplace_back(name, at);
            return r;
        }
        if (auto cat = category_escape(c, at)) {
            Node s = make(Node::Type::set);
            ClassNode wrapper;
            wrapper.kids.push_back(std::move(*cat));
            s.set = m_prog.classes.size();
            m_prog.classes.push_back(std::move(wrapper));
            return s;
        }
        if (failed()) return std::nullopt;
        if (auto cp = simple_escape(c, at)) {
            Node l = make(Node::Type::literal);
            l.ch = *cp;
            return l;
        }
        if (!failed()) fail("unknown escape \\" + std::string(1, c), at);
        return std::nullopt;
    }

    bool class_atom(ClassNode& out, char32_t& single, bool& is_single) {
        const std::size_t at = m_i;
        is_single = false;
        if (peek() == '[' && peek(1) == ':') {
            const std::size_t close = m_p.find(":]", m_i + 2);
            if (close != std::string_view::npos) {
                std::string name(m_p.substr(m_i + 2, close - m_i - 2));
                bool neg = false;
                if (!name.empty() && name[0] == '^') {
                    neg = true;
                    name.erase(name.begin());
                }
                const std::optional<Cat> cat = cat_named(name == "digit" ? "nd" : name);
                if (!cat) {
                    fail("unknown class [:" + name + ":]", at);
                    return false;
                }
                out = ClassNode{};
                out.type = ClassNode::Type::category;
                out.cat = name == "digit" ? Cat::digit : *cat;
                out.negated = neg;
                m_i = close + 2;
                return true;
            }
        }
        if (peek() == '[') {
            return bracket(out);
        }
        if (peek() == '\\') {
            ++m_i;
            if (!more()) {
                fail("the pattern can't end with a lone \\", at);
                return false;
            }
            const char c = m_p[m_i++];
            if (c == 'b') {
                single = U'\b';
                is_single = true;
                return true;
            }
            if (auto cat = category_escape(c, at)) {
                out = std::move(*cat);
                return true;
            }
            if (failed()) return false;
            if (auto cp = simple_escape(c, at)) {
                single = *cp;
                is_single = true;
                return true;
            }
            if (!failed()) fail("unknown escape \\" + std::string(1, c) + " in a character class", at);
            return false;
        }
        single = take_cp();
        is_single = true;
        return true;
    }

    bool bracket(ClassNode& out) {
        const std::size_t open = m_i;
        ++m_i;
        out = ClassNode{};
        out.type = ClassNode::Type::set;
        if (peek() == '^') {
            out.negated = true;
            ++m_i;
        }
        ClassNode current;
        current.type = ClassNode::Type::set;
        ClassNode::Type pending_op = ClassNode::Type::set;
        std::vector<std::pair<ClassNode::Type, ClassNode>> operands;
        bool first = true;
        while (true) {
            if (!more()) {
                fail("this [ is never closed with ]", open);
                return false;
            }
            const char c = peek();
            if (c == ']' && !first) {
                ++m_i;
                break;
            }
            first = false;
            if ((c == '-' && peek(1) == '-') || (c == '&' && peek(1) == '&')) {
                operands.emplace_back(pending_op, std::move(current));
                current = ClassNode{};
                current.type = ClassNode::Type::set;
                pending_op = c == '-' ? ClassNode::Type::subtract : ClassNode::Type::intersect;
                m_i += 2;
                continue;
            }
            ClassNode item;
            char32_t single = 0;
            bool is_single = false;
            if (!class_atom(item, single, is_single)) return false;
            if (is_single && peek() == '-' && peek(1) != ']' && peek(1) != '-' && more()) {
                const std::size_t dash = m_i;
                ++m_i;
                ClassNode end_item;
                char32_t end_single = 0;
                bool end_is_single = false;
                if (!class_atom(end_item, end_single, end_is_single)) return false;
                if (!end_is_single) {
                    fail("a range needs a single character at each end", dash);
                    return false;
                }
                if (end_single < single) {
                    fail("this range goes backwards", dash);
                    return false;
                }
                ClassNode r;
                r.type = ClassNode::Type::range;
                r.first = single;
                r.last = end_single;
                current.kids.push_back(std::move(r));
                continue;
            }
            if (is_single) {
                ClassNode r;
                r.type = ClassNode::Type::range;
                r.first = single;
                r.last = single;
                current.kids.push_back(std::move(r));
            } else {
                current.kids.push_back(std::move(item));
            }
        }
        operands.emplace_back(pending_op, std::move(current));
        if (operands.size() == 1) {
            out.kids = std::move(operands[0].second.kids);
            return true;
        }
        ClassNode acc = std::move(operands[0].second);
        for (std::size_t k = 1; k < operands.size(); ++k) {
            ClassNode combined;
            combined.type = operands[k].first;
            combined.kids.push_back(std::move(acc));
            combined.kids.push_back(std::move(operands[k].second));
            acc = std::move(combined);
        }
        out.kids.push_back(std::move(acc));
        return true;
    }

    std::optional<Node> atom() {
        const char c = peek();
        const std::size_t at = m_i;
        switch (c) {
            case '(': return group();
            case '[': {
                ClassNode cls;
                if (!bracket(cls)) return std::nullopt;
                Node n = make(Node::Type::set);
                n.set = m_prog.classes.size();
                m_prog.classes.push_back(std::move(cls));
                return n;
            }
            case '.': {
                ++m_i;
                return make(Node::Type::any);
            }
            case '^': {
                ++m_i;
                Node n = make(Node::Type::assertion);
                n.assertion = m_flags.multiline ? AssertKind::line_start : AssertKind::text_start;
                return n;
            }
            case '$': {
                ++m_i;
                Node n = make(Node::Type::assertion);
                n.assertion = m_flags.multiline ? AssertKind::line_end : AssertKind::text_end;
                return n;
            }
            case '\\': return escape();
            case '*':
            case '+':
            case '?':
                fail("there is nothing before this " + std::string(1, c) + " to repeat", at);
                return std::nullopt;
            case '{': {
                const std::size_t save = m_i;
                ++m_i;
                std::size_t dummy = 0;
                if (read_number(dummy)) {
                    m_i = save;
                    fail("there is nothing before this { to repeat", at);
                    return std::nullopt;
                }
                m_i = save;
                break;
            }
            default: break;
        }
        Node n = make(Node::Type::literal);
        n.ch = take_cp();
        return n;
    }

    std::string_view m_p;
    std::size_t m_i = 0;
    Regex::Program& m_prog;
    Flags m_flags;
    std::string m_error;
    std::size_t m_error_at = 0;
    std::vector<std::pair<std::size_t, std::size_t>> m_backrefs;
    std::vector<std::pair<std::string, std::size_t>> m_named_backrefs;
};

bool nullable(const Node& n) {
    switch (n.type) {
        case Node::Type::empty:
        case Node::Type::assertion:
        case Node::Type::look:
        case Node::Type::backref: return true;
        case Node::Type::literal:
        case Node::Type::set:
        case Node::Type::any: return false;
        case Node::Type::concat:
            for (const Node& k : n.kids) {
                if (!nullable(k)) return false;
            }
            return true;
        case Node::Type::alternate:
            for (const Node& k : n.kids) {
                if (nullable(k)) return true;
            }
            return false;
        case Node::Type::repeat: return n.min == 0 || nullable(n.kids[0]);
        case Node::Type::group:
        case Node::Type::atomic: return nullable(n.kids[0]);
    }
    return true;
}

std::size_t max_width(const Node& n) {
    switch (n.type) {
        case Node::Type::empty:
        case Node::Type::assertion:
        case Node::Type::look: return 0;
        case Node::Type::literal:
        case Node::Type::set:
        case Node::Type::any: return 1;
        case Node::Type::backref: return unbounded;
        case Node::Type::concat: {
            std::size_t total = 0;
            for (const Node& k : n.kids) {
                const std::size_t w = max_width(k);
                if (w == unbounded) return unbounded;
                total += w;
            }
            return total;
        }
        case Node::Type::alternate: {
            std::size_t most = 0;
            for (const Node& k : n.kids) {
                const std::size_t w = max_width(k);
                if (w == unbounded) return unbounded;
                most = std::max(most, w);
            }
            return most;
        }
        case Node::Type::repeat: {
            if (n.max == unbounded) return max_width(n.kids[0]) == 0 ? 0 : unbounded;
            const std::size_t w = max_width(n.kids[0]);
            if (w == unbounded) return unbounded;
            return w * n.max;
        }
        case Node::Type::group:
        case Node::Type::atomic: return max_width(n.kids[0]);
    }
    return unbounded;
}

class Compiler {
public:
    Compiler(Regex::Program& program) : m_prog(program) {}

    bool compile() {
        m_prog.subs.emplace_back();
        Sub& main = m_prog.subs[0];
        (void)main;
        emit(0, Inst{ Op::save, 0 });
        node(0, m_prog.ast);
        emit(0, Inst{ Op::save, 1 });
        emit(0, Inst{ Op::at_target });
        emit(0, Inst{ Op::match });
        return !m_too_big;
    }

private:
    std::size_t emit(std::size_t sub, Inst inst) {
        std::vector<Inst>& code = m_prog.subs[sub].code;
        if (++m_total > max_program) m_too_big = true;
        code.push_back(inst);
        return code.size() - 1;
    }

    std::size_t here(std::size_t sub) const { return m_prog.subs[sub].code.size(); }

    Inst& at(std::size_t sub, std::size_t pc) { return m_prog.subs[sub].code[pc]; }

    void node(std::size_t sub, const Node& n) {
        if (m_too_big) return;
        switch (n.type) {
            case Node::Type::empty: break;
            case Node::Type::literal: {
                Inst i{ Op::literal };
                i.ch = n.icase ? unicode::fold_case(n.ch) : n.ch;
                i.icase = n.icase;
                emit(sub, i);
                break;
            }
            case Node::Type::set: {
                Inst i{ Op::set, static_cast<std::uint32_t>(n.set) };
                i.icase = n.icase;
                i.flag = n.uni;
                emit(sub, i);
                break;
            }
            case Node::Type::any: {
                Inst i{ Op::any };
                i.flag = n.dot_all;
                emit(sub, i);
                break;
            }
            case Node::Type::concat:
                for (const Node& k : n.kids) node(sub, k);
                break;
            case Node::Type::alternate: {
                std::vector<std::size_t> jumps;
                for (std::size_t k = 0; k < n.kids.size(); ++k) {
                    if (k + 1 < n.kids.size()) {
                        const std::size_t split = emit(sub, Inst{ Op::split });
                        at(sub, split).a = static_cast<std::uint32_t>(here(sub));
                        node(sub, n.kids[k]);
                        jumps.push_back(emit(sub, Inst{ Op::jump }));
                        at(sub, split).b = static_cast<std::uint32_t>(here(sub));
                    } else {
                        node(sub, n.kids[k]);
                    }
                }
                for (std::size_t j : jumps) at(sub, j).a = static_cast<std::uint32_t>(here(sub));
                break;
            }
            case Node::Type::group: {
                emit(sub, Inst{ Op::save, static_cast<std::uint32_t>(n.group * 2) });
                node(sub, n.kids[0]);
                emit(sub, Inst{ Op::save, static_cast<std::uint32_t>(n.group * 2 + 1) });
                break;
            }
            case Node::Type::assertion: {
                Inst i{ Op::assertion, static_cast<std::uint32_t>(n.assertion) };
                i.flag = n.uni;
                emit(sub, i);
                break;
            }
            case Node::Type::backref: {
                std::size_t index = n.group;
                if (!n.name.empty()) {
                    const auto it = std::find(m_prog.names.begin(), m_prog.names.end(), n.name);
                    index = static_cast<std::size_t>(it - m_prog.names.begin());
                }
                Inst i{ Op::backref, static_cast<std::uint32_t>(index) };
                i.icase = n.icase;
                emit(sub, i);
                break;
            }
            case Node::Type::look: {
                const std::size_t inner = m_prog.subs.size();
                m_prog.subs.emplace_back();
                node(inner, n.kids[0]);
                if (n.behind) emit(inner, Inst{ Op::at_target });
                emit(inner, Inst{ Op::match });
                Inst i{ Op::look, static_cast<std::uint32_t>(inner) };
                i.flag = n.behind;
                i.flag2 = n.negate;
                i.width = n.behind ? max_width(n.kids[0]) : unbounded;
                emit(sub, i);
                break;
            }
            case Node::Type::atomic: {
                const std::size_t inner = m_prog.subs.size();
                m_prog.subs.emplace_back();
                node(inner, n.kids[0]);
                emit(inner, Inst{ Op::match });
                emit(sub, Inst{ Op::atomic, static_cast<std::uint32_t>(inner) });
                break;
            }
            case Node::Type::repeat: {
                if (n.possessive) {
                    Node plain = n;
                    plain.possessive = false;
                    const std::size_t inner = m_prog.subs.size();
                    m_prog.subs.emplace_back();
                    node(inner, plain);
                    emit(inner, Inst{ Op::match });
                    emit(sub, Inst{ Op::atomic, static_cast<std::uint32_t>(inner) });
                    break;
                }
                const Node& body = n.kids[0];
                for (std::size_t k = 0; k < n.min && !m_too_big; ++k) node(sub, body);
                if (n.max == unbounded) {
                    const bool empty_ok = nullable(body);
                    const std::size_t reg = empty_ok ? m_prog.regs++ : 0;
                    const std::size_t loop = emit(sub, Inst{ Op::split });
                    const std::size_t body_start = here(sub);
                    if (empty_ok) emit(sub, Inst{ Op::mark, static_cast<std::uint32_t>(reg) });
                    node(sub, body);
                    if (empty_ok) emit(sub, Inst{ Op::check, static_cast<std::uint32_t>(reg) });
                    emit(sub, Inst{ Op::jump, static_cast<std::uint32_t>(loop) });
                    const std::size_t exit = here(sub);
                    at(sub, loop).a = static_cast<std::uint32_t>(n.greedy ? body_start : exit);
                    at(sub, loop).b = static_cast<std::uint32_t>(n.greedy ? exit : body_start);
                } else {
                    std::vector<std::size_t> splits;
                    for (std::size_t k = n.min; k < n.max && !m_too_big; ++k) {
                        const std::size_t split = emit(sub, Inst{ Op::split });
                        splits.push_back(split);
                        const std::size_t body_start = here(sub);
                        node(sub, body);
                        at(sub, split).a = static_cast<std::uint32_t>(body_start);
                    }
                    const std::size_t exit = here(sub);
                    for (std::size_t s : splits) {
                        Inst& i = at(sub, s);
                        const std::uint32_t body_start = i.a;
                        if (n.greedy) {
                            i.a = body_start;
                            i.b = static_cast<std::uint32_t>(exit);
                        } else {
                            i.a = static_cast<std::uint32_t>(exit);
                            i.b = body_start;
                        }
                    }
                }
                break;
            }
        }
    }

    Regex::Program& m_prog;
    std::size_t m_total = 0;
    bool m_too_big = false;
};

struct Frame {
    std::uint8_t kind = 0;
    std::uint32_t pc = 0;
    std::size_t pos = 0;
};

class Runner {
public:
    Runner(const Regex::Program& program, std::string_view text, std::size_t budget) : m_prog(program), m_text(text), m_budget(budget) {
        slots.assign(m_prog.groups * 2, no_pos);
        regs.assign(m_prog.regs, no_pos);
    }

    bool run(std::size_t sub, std::size_t start, std::size_t target, std::size_t& end) {
        const std::vector<Inst>& code = m_prog.subs[sub].code;
        std::vector<Frame> stack;
        stack.push_back(Frame{ 0, 0, start });
        while (!stack.empty()) {
            const Frame f = stack.back();
            stack.pop_back();
            if (f.kind == 1) {
                slots[f.pc] = f.pos;
                continue;
            }
            if (f.kind == 2) {
                regs[f.pc] = f.pos;
                continue;
            }
            std::size_t pc = f.pc;
            std::size_t pos = f.pos;
            bool alive = true;
            while (alive) {
                if (m_budget == 0) {
                    gave_up = true;
                    return false;
                }
                --m_budget;
                const Inst& in = code[pc];
                switch (in.op) {
                    case Op::literal: {
                        if (pos >= m_text.size()) {
                            alive = false;
                            break;
                        }
                        std::size_t len = 1;
                        char32_t cp = decode_at(m_text, pos, len);
                        if (in.icase) cp = unicode::fold_case(cp);
                        if (cp != in.ch) {
                            alive = false;
                            break;
                        }
                        pos += len;
                        ++pc;
                        break;
                    }
                    case Op::set: {
                        if (pos >= m_text.size()) {
                            alive = false;
                            break;
                        }
                        std::size_t len = 1;
                        const char32_t cp = decode_at(m_text, pos, len);
                        if (!class_match(m_prog.classes[in.a], cp, in.icase, in.flag)) {
                            alive = false;
                            break;
                        }
                        pos += len;
                        ++pc;
                        break;
                    }
                    case Op::any: {
                        if (pos >= m_text.size()) {
                            alive = false;
                            break;
                        }
                        std::size_t len = 1;
                        const char32_t cp = decode_at(m_text, pos, len);
                        if (!in.flag && (cp == '\n' || cp == '\r')) {
                            alive = false;
                            break;
                        }
                        pos += len;
                        ++pc;
                        break;
                    }
                    case Op::split:
                        stack.push_back(Frame{ 0, in.b, pos });
                        pc = in.a;
                        break;
                    case Op::jump: pc = in.a; break;
                    case Op::save:
                        stack.push_back(Frame{ 1, in.a, slots[in.a] });
                        slots[in.a] = pos;
                        ++pc;
                        break;
                    case Op::mark:
                        stack.push_back(Frame{ 2, in.a, regs[in.a] });
                        regs[in.a] = pos;
                        ++pc;
                        break;
                    case Op::check:
                        if (regs[in.a] == pos) {
                            alive = false;
                            break;
                        }
                        ++pc;
                        break;
                    case Op::assertion:
                        if (!assertion(static_cast<AssertKind>(in.a), pos, in.flag)) {
                            alive = false;
                            break;
                        }
                        ++pc;
                        break;
                    case Op::backref: {
                        const std::size_t b = slots[in.a * 2];
                        const std::size_t e = slots[in.a * 2 + 1];
                        if (b == no_pos || e == no_pos) {
                            ++pc;
                            break;
                        }
                        std::size_t p = pos;
                        std::size_t q = b;
                        bool ok = true;
                        while (q < e) {
                            if (p >= m_text.size()) {
                                ok = false;
                                break;
                            }
                            std::size_t la = 1;
                            std::size_t lb = 1;
                            char32_t ca = decode_at(m_text, q, la);
                            char32_t cb = decode_at(m_text, p, lb);
                            if (in.icase) {
                                ca = unicode::fold_case(ca);
                                cb = unicode::fold_case(cb);
                            }
                            if (ca != cb) {
                                ok = false;
                                break;
                            }
                            q += la;
                            p += lb;
                        }
                        if (!ok) {
                            alive = false;
                            break;
                        }
                        pos = p;
                        ++pc;
                        break;
                    }
                    case Op::look: {
                        const std::vector<std::size_t> saved_slots = slots;
                        const std::vector<std::size_t> saved_regs = regs;
                        bool ok = false;
                        std::size_t e = 0;
                        if (!in.flag) {
                            ok = run(in.a, pos, no_pos, e);
                        } else {
                            const std::size_t lowest = in.width == unbounded ? 0 : (pos > in.width * 4 ? pos - in.width * 4 : 0);
                            std::size_t s = pos;
                            while (true) {
                                if (run(in.a, s, pos, e)) {
                                    ok = true;
                                    break;
                                }
                                if (gave_up) return false;
                                if (s == 0 || s <= lowest) break;
                                s = previous_start(m_text, s);
                            }
                        }
                        if (gave_up) return false;
                        if (in.flag2) {
                            slots = saved_slots;
                            regs = saved_regs;
                            if (ok) {
                                alive = false;
                                break;
                            }
                        } else {
                            if (!ok) {
                                slots = saved_slots;
                                regs = saved_regs;
                                alive = false;
                                break;
                            }
                            keep_changes(stack, saved_slots, saved_regs);
                        }
                        ++pc;
                        break;
                    }
                    case Op::atomic: {
                        const std::vector<std::size_t> saved_slots = slots;
                        const std::vector<std::size_t> saved_regs = regs;
                        std::size_t e = 0;
                        const bool ok = run(in.a, pos, no_pos, e);
                        if (gave_up) return false;
                        if (!ok) {
                            slots = saved_slots;
                            regs = saved_regs;
                            alive = false;
                            break;
                        }
                        keep_changes(stack, saved_slots, saved_regs);
                        pos = e;
                        ++pc;
                        break;
                    }
                    case Op::at_target:
                        if (target != no_pos && pos != target) {
                            alive = false;
                            break;
                        }
                        ++pc;
                        break;
                    case Op::match:
                        end = pos;
                        return true;
                }
            }
        }
        return false;
    }

    std::vector<std::size_t> slots;
    std::vector<std::size_t> regs;
    bool gave_up = false;

private:
    void keep_changes(std::vector<Frame>& stack, const std::vector<std::size_t>& saved_slots, const std::vector<std::size_t>& saved_regs) {
        for (std::size_t k = 0; k < slots.size(); ++k) {
            if (slots[k] != saved_slots[k]) stack.push_back(Frame{ 1, static_cast<std::uint32_t>(k), saved_slots[k] });
        }
        for (std::size_t k = 0; k < regs.size(); ++k) {
            if (regs[k] != saved_regs[k]) stack.push_back(Frame{ 2, static_cast<std::uint32_t>(k), saved_regs[k] });
        }
    }

    bool word_at(std::size_t pos, bool uni) const {
        if (pos >= m_text.size()) return false;
        std::size_t len = 1;
        return in_cat(Cat::word, decode_at(m_text, pos, len), uni);
    }

    bool assertion(AssertKind kind, std::size_t pos, bool uni) const {
        const std::size_t n = m_text.size();
        switch (kind) {
            case AssertKind::text_start: return pos == 0;
            case AssertKind::text_end: return pos == n;
            case AssertKind::text_end_newline: return pos == n || (pos + 1 == n && m_text[pos] == '\n');
            case AssertKind::line_start: return pos == 0 || m_text[pos - 1] == '\n';
            case AssertKind::line_end: return pos == n || m_text[pos] == '\n' || (m_text[pos] == '\r' && pos + 1 < n && m_text[pos + 1] == '\n');
            case AssertKind::word_boundary:
            case AssertKind::not_word_boundary: {
                const bool before = pos > 0 && word_at(previous_start(m_text, pos), uni);
                const bool after = word_at(pos, uni);
                return (before != after) == (kind == AssertKind::word_boundary);
            }
        }
        return false;
    }

    const Regex::Program& m_prog;
    std::string_view m_text;
    std::size_t m_budget;
};

std::string repeat_words(const Node& n) {
    std::string out;
    if (n.min == 0 && n.max == 1) out = "optionally";
    else if (n.min == 0 && n.max == unbounded) out = "zero or more of";
    else if (n.min == 1 && n.max == unbounded) out = "one or more of";
    else if (n.max == unbounded) out = "at least " + std::to_string(n.min) + " of";
    else if (n.min == n.max) out = "exactly " + std::to_string(n.min) + " of";
    else if (n.min == 0) out = "at most " + std::to_string(n.max) + " of";
    else out = "between " + std::to_string(n.min) + " and " + std::to_string(n.max) + " of";
    if (!n.greedy) out += " (as few as possible)";
    if (n.possessive) out += " (as many as possible, never giving any back)";
    return out;
}

const char* assert_words(AssertKind kind) {
    switch (kind) {
        case AssertKind::line_start: return "the start of a line";
        case AssertKind::line_end: return "the end of a line";
        case AssertKind::text_start: return "the start of the text";
        case AssertKind::text_end: return "the end of the text";
        case AssertKind::text_end_newline: return "the end of the text (or just before a final line break)";
        case AssertKind::word_boundary: return "a word boundary (where a word starts or ends)";
        case AssertKind::not_word_boundary: return "a place that isn't a word boundary";
    }
    return "";
}

class Explainer {
public:
    Explainer(const Regex::Program& program) : m_prog(program) {}

    std::string run() {
        std::vector<std::string> lines;
        const Node& root = m_prog.ast;
        if (root.type == Node::Type::concat) {
            std::size_t number = 1;
            for (std::size_t k = 0; k < root.kids.size();) {
                std::size_t next = k;
                const std::string first = item(root.kids, next);
                lines.push_back(std::to_string(number++) + ". " + first);
                block(root.kids[k], next - k, lines, "   ");
                k = next;
            }
        } else if (root.type == Node::Type::empty) {
            lines.push_back("matches the empty text (everything matches, at every position)");
        } else {
            std::size_t next = 0;
            std::vector<Node> wrap;
            wrap.push_back(root);
            lines.push_back(item(wrap, next));
            block(root, 1, lines, "  ");
        }
        std::string out;
        for (const std::string& l : lines) out += l + "\n";
        return out;
    }

private:
    bool simple(const Node& n) const {
        switch (n.type) {
            case Node::Type::literal:
            case Node::Type::set:
            case Node::Type::any:
            case Node::Type::assertion:
            case Node::Type::backref:
            case Node::Type::empty: return true;
            case Node::Type::repeat: return simple(n.kids[0]) || literal_run(n.kids[0]);
            case Node::Type::group: return false;
            default: return false;
        }
    }

    bool literal_run(const Node& n) const {
        if (n.type != Node::Type::concat) return false;
        for (const Node& k : n.kids) {
            if (k.type != Node::Type::literal) return false;
        }
        return true;
    }

    std::string literal_text(const std::vector<Node>& nodes, std::size_t from, std::size_t to) const {
        std::string text;
        for (std::size_t k = from; k < to; ++k) append_utf8(text, nodes[k].ch);
        bool icase = false;
        for (std::size_t k = from; k < to; ++k) icase = icase || nodes[k].icase;
        if (to - from == 1) return quote_char(nodes[from].ch) + (icase ? " (any case)" : "");
        return "the text \"" + text + "\"" + (icase ? " (any case)" : "");
    }

    std::string short_words(const Node& n) const {
        switch (n.type) {
            case Node::Type::literal: return quote_char(n.ch) + (n.icase ? " (any case)" : "");
            case Node::Type::set: {
                const ClassNode& c = m_prog.classes[n.set];
                std::string w = class_words(c, n.uni);
                if (!c.negated && c.type == ClassNode::Type::set && c.kids.size() > 1) w = "one of " + w;
                return w + (n.icase ? " (any case)" : "");
            }
            case Node::Type::any: return n.dot_all ? "any character" : "any character except a line break";
            case Node::Type::assertion: return assert_words(n.assertion);
            case Node::Type::backref: {
                std::string which = n.name.empty() ? "group " + std::to_string(n.group) : "the group named \"" + n.name + "\"";
                return "the same text " + which + " matched";
            }
            case Node::Type::empty: return "nothing";
            case Node::Type::repeat: {
                const Node& body = n.kids[0];
                std::string inner;
                if (literal_run(body)) inner = literal_text(body.kids, 0, body.kids.size());
                else if (body.type == Node::Type::set) inner = class_words(m_prog.classes[body.set], body.uni) + (body.icase ? " (any case)" : "");
                else inner = short_words(body);
                return repeat_words(n) + ": " + inner;
            }
            default: return "";
        }
    }

    std::string item(const std::vector<Node>& nodes, std::size_t& k) const {
        const Node& n = nodes[k];
        if (n.type == Node::Type::literal) {
            std::size_t e = k;
            while (e < nodes.size() && nodes[e].type == Node::Type::literal && nodes[e].icase == n.icase) ++e;
            const std::string t = literal_text(nodes, k, e);
            k = e;
            return t;
        }
        ++k;
        if (simple(n)) return short_words(n);
        switch (n.type) {
            case Node::Type::concat: return "all of these, in order:";
            case Node::Type::alternate: return "one of these:";
            case Node::Type::repeat: return repeat_words(n) + ":";
            case Node::Type::group: {
                std::string label = "group " + std::to_string(n.group);
                if (!n.name.empty()) label += " (\"" + n.name + "\")";
                if (n.kids[0].type == Node::Type::literal || n.kids[0].type == Node::Type::set || n.kids[0].type == Node::Type::any || simple(n.kids[0])) {
                    return label + ", which is " + short_words(n.kids[0]);
                }
                return label + ", which is:";
            }
            case Node::Type::look:
                if (n.behind) return n.negate ? "a place not right after:" : "a place right after:";
                return n.negate ? "a place not followed by:" : "a place followed by:";
            case Node::Type::atomic: return "this, without ever trying it a different way:";
            default: return short_words(n);
        }
    }

    void block(const Node& n, std::size_t consumed, std::vector<std::string>& lines, const std::string& indent) const {
        if (consumed != 1 || simple(n) || n.type == Node::Type::literal) return;
        const Node* body = nullptr;
        switch (n.type) {
            case Node::Type::concat:
            case Node::Type::alternate: body = &n; break;
            case Node::Type::repeat:
            case Node::Type::look:
            case Node::Type::atomic: body = &n.kids[0]; break;
            case Node::Type::group:
                if (simple(n.kids[0])) return;
                body = &n.kids[0];
                break;
            default: return;
        }
        if (body->type == Node::Type::alternate) {
            for (const Node& option : body->kids) {
                std::vector<Node> wrap;
                if (option.type == Node::Type::concat) {
                    std::size_t k = 0;
                    std::string first = item(option.kids, k);
                    if (k == option.kids.size()) {
                        lines.push_back(indent + "- " + first);
                        block(option.kids[0], k, lines, indent + "  ");
                    } else {
                        lines.push_back(indent + "- all of these, in order:");
                        list(option.kids, lines, indent + "    ");
                    }
                } else {
                    wrap.push_back(option);
                    std::size_t k = 0;
                    lines.push_back(indent + "- " + item(wrap, k));
                    block(option, 1, lines, indent + "  ");
                }
            }
            return;
        }
        if (body->type == Node::Type::concat) {
            list(body->kids, lines, indent);
            return;
        }
        std::vector<Node> wrap;
        wrap.push_back(*body);
        std::size_t k = 0;
        lines.push_back(indent + "- " + item(wrap, k));
        block(*body, 1, lines, indent + "  ");
    }

    void list(const std::vector<Node>& kids, std::vector<std::string>& lines, const std::string& indent) const {
        for (std::size_t k = 0; k < kids.size();) {
            std::size_t next = k;
            const std::string text = item(kids, next);
            lines.push_back(indent + "- " + text);
            block(kids[k], next - k, lines, indent + "  ");
            k = next;
        }
    }

    const Regex::Program& m_prog;
};

std::string class_escape(char32_t cp) {
    switch (cp) {
        case '\\':
        case ']':
        case '[':
        case '^':
        case '-':
        case '&': return std::string("\\") + static_cast<char>(cp);
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        default: break;
    }
    std::string out;
    if (cp < 0x20 || cp == 0x7F) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "\\x%02X", static_cast<unsigned>(cp));
        return buf;
    }
    append_utf8(out, cp);
    return out;
}

const char* kind_regex(CharKind kind) {
    switch (kind) {
        case CharKind::digits: return "\\p{Nd}";
        case CharKind::numbers: return "\\p{N}";
        case CharKind::letters: return "\\p{L}";
        case CharKind::uppercase: return "\\p{Lu}";
        case CharKind::lowercase: return "\\p{Ll}";
        case CharKind::spaces: return "\\p{White_Space}";
        case CharKind::newlines: return "\\R";
        case CharKind::punctuation: return "\\p{P}";
        case CharKind::symbols: return "\\p{S}";
        case CharKind::special: return "\\p{P}\\p{S}";
        case CharKind::emoji: return "\\p{Emoji}";
        case CharKind::control: return "\\p{Cc}";
        case CharKind::ascii: return "\\p{ASCII}";
        case CharKind::hex_digits: return "0-9A-Fa-f";
        case CharKind::word: return "\\p{L}\\p{N}_";
        case CharKind::non_ascii: return "\\P{ASCII}";
    }
    return "";
}

const char* kind_words(CharKind kind) {
    switch (kind) {
        case CharKind::digits: return "digits";
        case CharKind::numbers: return "numbers (digits, \xC2\xBD, \xC2\xB2, roman numerals, ...)";
        case CharKind::letters: return "letters";
        case CharKind::uppercase: return "uppercase letters";
        case CharKind::lowercase: return "lowercase letters";
        case CharKind::spaces: return "spaces, tabs and line breaks";
        case CharKind::newlines: return "line breaks";
        case CharKind::punctuation: return "punctuation";
        case CharKind::symbols: return "symbols";
        case CharKind::special: return "punctuation and symbols";
        case CharKind::emoji: return "emoji";
        case CharKind::control: return "control characters";
        case CharKind::ascii: return "ASCII characters";
        case CharKind::hex_digits: return "hex digits";
        case CharKind::word: return "letters, numbers and _";
        case CharKind::non_ascii: return "non-ASCII characters";
    }
    return "";
}

bool kind_contains(CharKind kind, char32_t cp) noexcept {
    switch (kind) {
        case CharKind::digits: return in_cat(Cat::decimal, cp, true);
        case CharKind::numbers: return in_cat(Cat::number, cp, true);
        case CharKind::letters: return in_cat(Cat::letter, cp, true);
        case CharKind::uppercase: return in_cat(Cat::upper, cp, true);
        case CharKind::lowercase: return in_cat(Cat::lower, cp, true);
        case CharKind::spaces: return in_cat(Cat::white_space, cp, true);
        case CharKind::newlines: return is_newline(cp);
        case CharKind::punctuation: return in_cat(Cat::punct, cp, true);
        case CharKind::symbols: return in_cat(Cat::symbol, cp, true);
        case CharKind::special: return in_cat(Cat::punct, cp, true) || in_cat(Cat::symbol, cp, true);
        case CharKind::emoji: return is_emoji_char(cp);
        case CharKind::control: return in_cat(Cat::control, cp, true);
        case CharKind::ascii: return cp < 0x80;
        case CharKind::hex_digits: return in_cat(Cat::hex, cp, false);
        case CharKind::word: return in_cat(Cat::letter, cp, true) || in_cat(Cat::number, cp, true) || cp == '_';
        case CharKind::non_ascii: return cp >= 0x80;
    }
    return false;
}

std::string quote_list(const std::u32string& chars) {
    std::vector<std::string> parts;
    for (char32_t c : chars) parts.push_back(quote_char(c));
    return join_words(parts, "or");
}

} // namespace

CharSet::CharSet() = default;

CharSet CharSet::everything() {
    CharSet s;
    s.m_everything = true;
    return s;
}

CharSet CharSet::nothing() { return CharSet(); }

CharSet CharSet::of(StringView chars) { return CharSet().allow(chars); }

CharSet CharSet::range(char32_t first, char32_t last) { return CharSet().allow_range(first, last); }

CharSet CharSet::kind(CharKind kind) { return CharSet().allow(kind); }

CharSet CharSet::digits() { return kind(CharKind::digits); }

CharSet CharSet::numbers() { return kind(CharKind::numbers); }

CharSet CharSet::letters() { return kind(CharKind::letters); }

CharSet CharSet::uppercase() { return kind(CharKind::uppercase); }

CharSet CharSet::lowercase() { return kind(CharKind::lowercase); }

CharSet CharSet::spaces() { return kind(CharKind::spaces); }

CharSet CharSet::punctuation() { return kind(CharKind::punctuation); }

CharSet CharSet::symbols() { return kind(CharKind::symbols); }

CharSet CharSet::special() { return kind(CharKind::special); }

CharSet CharSet::emoji() { return kind(CharKind::emoji); }

CharSet CharSet::hex_digits() { return kind(CharKind::hex_digits); }

CharSet CharSet::word_characters() { return kind(CharKind::word); }

CharSet CharSet::ascii() { return kind(CharKind::ascii); }

CharSet& CharSet::add(bool allow, Item item) {
    m_rules.push_back(Rule{ allow, std::move(item) });
    return *this;
}

CharSet& CharSet::allow(StringView chars) {
    Item item;
    item.type = Item::Type::chars;
    item.chars = decode_all(chars);
    return add(true, std::move(item));
}

CharSet& CharSet::disable(StringView chars) {
    Item item;
    item.type = Item::Type::chars;
    item.chars = decode_all(chars);
    return add(false, std::move(item));
}

CharSet& CharSet::allow_range(char32_t first, char32_t last) {
    Item item;
    item.type = Item::Type::range;
    item.first = std::min(first, last);
    item.last = std::max(first, last);
    return add(true, std::move(item));
}

CharSet& CharSet::disable_range(char32_t first, char32_t last) {
    Item item;
    item.type = Item::Type::range;
    item.first = std::min(first, last);
    item.last = std::max(first, last);
    return add(false, std::move(item));
}

CharSet& CharSet::allow(CharKind kind) {
    Item item;
    item.type = Item::Type::kind;
    item.kind = kind;
    return add(true, std::move(item));
}

CharSet& CharSet::disable(CharKind kind, StringView keep) {
    Item item;
    item.type = Item::Type::kind;
    item.kind = kind;
    item.keep = decode_all(keep);
    return add(false, std::move(item));
}

CharSet& CharSet::allow(const CharSet& other) {
    Item item;
    item.type = Item::Type::set;
    item.set = std::make_shared<const CharSet>(other);
    return add(true, std::move(item));
}

CharSet& CharSet::disable(const CharSet& other) {
    Item item;
    item.type = Item::Type::set;
    item.set = std::make_shared<const CharSet>(other);
    return add(false, std::move(item));
}

CharSet& CharSet::allow_digits() { return allow(CharKind::digits); }
CharSet& CharSet::disable_digits() { return disable(CharKind::digits); }
CharSet& CharSet::allow_numbers() { return allow(CharKind::numbers); }
CharSet& CharSet::disable_numbers() { return disable(CharKind::numbers); }
CharSet& CharSet::allow_letters() { return allow(CharKind::letters); }
CharSet& CharSet::disable_letters() { return disable(CharKind::letters); }
CharSet& CharSet::allow_uppercase() { return allow(CharKind::uppercase); }
CharSet& CharSet::disable_uppercase() { return disable(CharKind::uppercase); }
CharSet& CharSet::allow_lowercase() { return allow(CharKind::lowercase); }
CharSet& CharSet::disable_lowercase() { return disable(CharKind::lowercase); }
CharSet& CharSet::allow_spaces() { return allow(CharKind::spaces); }
CharSet& CharSet::disable_spaces() { return disable(CharKind::spaces); }
CharSet& CharSet::allow_newlines() { return allow(CharKind::newlines); }
CharSet& CharSet::disable_newlines() { return disable(CharKind::newlines); }
CharSet& CharSet::allow_punctuation() { return allow(CharKind::punctuation); }
CharSet& CharSet::disable_punctuation(StringView keep) { return disable(CharKind::punctuation, keep); }
CharSet& CharSet::allow_symbols() { return allow(CharKind::symbols); }
CharSet& CharSet::disable_symbols(StringView keep) { return disable(CharKind::symbols, keep); }
CharSet& CharSet::allow_special() { return allow(CharKind::special); }
CharSet& CharSet::disable_special(StringView keep) { return disable(CharKind::special, keep); }
CharSet& CharSet::allow_emoji() { return allow(CharKind::emoji); }
CharSet& CharSet::disable_emoji() { return disable(CharKind::emoji); }
CharSet& CharSet::allow_control() { return allow(CharKind::control); }
CharSet& CharSet::disable_control() { return disable(CharKind::control); }
CharSet& CharSet::allow_hex_digits() { return allow(CharKind::hex_digits); }

CharSet& CharSet::only_ascii() { return disable(CharKind::non_ascii); }

bool CharSet::item_contains(const Item& item, char32_t cp) noexcept {
    bool hit = false;
    switch (item.type) {
        case Item::Type::chars: hit = item.chars.find(cp) != std::u32string::npos; break;
        case Item::Type::range: hit = cp >= item.first && cp <= item.last; break;
        case Item::Type::kind: hit = kind_contains(item.kind, cp); break;
        case Item::Type::set: hit = item.set && item.set->contains(cp); break;
    }
    if (hit && !item.keep.empty() && item.keep.find(cp) != std::u32string::npos) hit = false;
    return hit;
}

bool CharSet::contains(char32_t cp) const noexcept {
    bool in = m_everything;
    for (const Rule& r : m_rules) {
        if (item_contains(r.item, cp)) in = r.allow;
    }
    return in;
}

bool CharSet::accepts(StringView text) const noexcept { return first_rejected(text) == StringView::npos; }

std::size_t CharSet::first_rejected(StringView text) const noexcept {
    const std::string_view s(text.data(), text.size());
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t len = 1;
        const char32_t cp = decode_at(s, i, len);
        if (!contains(cp)) return i;
        i += len;
    }
    return StringView::npos;
}

std::string CharSet::filter(StringView text) const {
    const std::string_view s(text.data(), text.size());
    std::string out;
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t len = 1;
        const char32_t cp = decode_at(s, i, len);
        if (contains(cp)) out.append(s.substr(i, len));
        i += len;
    }
    return out;
}

CharSet CharSet::inverted() const { return CharSet::everything().disable(*this); }

bool CharSet::is_everything() const noexcept { return m_everything && m_rules.empty(); }

bool CharSet::is_nothing() const noexcept { return !m_everything && m_rules.empty(); }

std::string CharSet::item_regex(const Item& item) {
    std::string body;
    switch (item.type) {
        case Item::Type::chars:
            for (char32_t c : item.chars) body += class_escape(c);
            break;
        case Item::Type::range: body = class_escape(item.first) + "-" + class_escape(item.last); break;
        case Item::Type::kind: body = kind_regex(item.kind); break;
        case Item::Type::set: body = item.set ? item.set->to_regex() : std::string("[^\\s\\S]"); break;
    }
    if (item.keep.empty()) return body;
    std::string keep;
    for (char32_t c : item.keep) keep += class_escape(c);
    return "[" + body + "--[" + keep + "]]";
}

std::string CharSet::to_regex() const {
    bool all = m_everything;
    bool none = !m_everything;
    bool has_op = false;
    std::string body;
    std::size_t k = 0;
    while (k < m_rules.size()) {
        const bool allow_group = m_rules[k].allow;
        std::string x;
        std::size_t e = k;
        while (e < m_rules.size() && m_rules[e].allow == allow_group) {
            x += item_regex(m_rules[e].item);
            ++e;
        }
        k = e;
        if (x.empty()) continue;
        if (allow_group) {
            if (all) continue;
            if (none) {
                body = x;
                none = false;
                has_op = false;
            } else {
                body = (has_op ? "[" + body + "]" : body) + x;
                has_op = false;
            }
        } else {
            if (none) continue;
            if (all) {
                body = "[^" + x + "]";
                all = false;
                has_op = false;
            } else {
                body = body + "--[" + x + "]";
                has_op = true;
            }
        }
    }
    if (all) return "[\\s\\S]";
    if (none) return "[^\\s\\S]";
    if (!has_op && body.size() >= 2 && body[0] == '\\') {
        const bool letter_escape = body.size() == 2 && std::isalpha(static_cast<unsigned char>(body[1]));
        const bool property = (body[1] == 'p' || body[1] == 'P') && body.size() > 3 && body[2] == '{' && body.find('}') == body.size() - 1;
        if (letter_escape || property) return body;
    }
    if (!has_op && body.size() > 2 && body.front() == '[') {
        int depth = 0;
        bool single = true;
        for (std::size_t i = 0; i < body.size(); ++i) {
            if (body[i] == '\\') {
                ++i;
                continue;
            }
            if (body[i] == '[') ++depth;
            else if (body[i] == ']') {
                --depth;
                if (depth == 0 && i + 1 != body.size()) single = false;
            }
        }
        if (single) return body;
    }
    return "[" + body + "]";
}

std::string CharSet::item_words(const Item& item) {
    std::string out;
    switch (item.type) {
        case Item::Type::chars: out = quote_list(item.chars); break;
        case Item::Type::range: out = quote_char(item.first) + " to " + quote_char(item.last); break;
        case Item::Type::kind: out = kind_words(item.kind); break;
        case Item::Type::set: out = item.set ? "(" + item.set->describe() + ")" : std::string("nothing"); break;
    }
    if (!item.keep.empty()) out += " other than " + quote_list(item.keep);
    return out;
}

std::string CharSet::describe() const {
    if (m_rules.empty()) return m_everything ? "any character" : "no characters";
    std::string out = m_everything ? "any character" : "";
    bool started = m_everything;
    std::size_t k = 0;
    while (k < m_rules.size()) {
        const bool allow_group = m_rules[k].allow;
        std::vector<std::string> parts;
        std::size_t e = k;
        while (e < m_rules.size() && m_rules[e].allow == allow_group) parts.push_back(item_words(m_rules[e++].item));
        k = e;
        if (allow_group) {
            const std::string list = join_words(parts, "or");
            if (!started) out = list;
            else out += "; but still " + list;
        } else {
            if (!started) continue;
            std::string list;
            for (std::size_t i = 0; i < parts.size(); ++i) list += (i ? "; " : "") + parts[i];
            out += (out.find(" except") == std::string::npos ? " except: " : "; and not ") + list;
        }
        started = true;
    }
    return out.empty() ? "no characters" : out;
}

std::size_t RegexMatch::begin(std::size_t group) const noexcept { return group < m_spans.size() ? m_spans[group].first : npos; }

std::size_t RegexMatch::end(std::size_t group) const noexcept { return group < m_spans.size() ? m_spans[group].second : npos; }

std::size_t RegexMatch::length(std::size_t group) const noexcept { return matched(group) ? m_spans[group].second - m_spans[group].first : 0; }

bool RegexMatch::matched(std::size_t group) const noexcept { return group < m_spans.size() && m_spans[group].first != npos && m_spans[group].second != npos; }

std::optional<std::size_t> RegexMatch::group_index(StringView name) const noexcept {
    if (!m_names) return std::nullopt;
    const std::string_view n(name.data(), name.size());
    for (std::size_t i = 1; i < m_names->size(); ++i) {
        if ((*m_names)[i] == n) return i;
    }
    return std::nullopt;
}

bool RegexMatch::matched(StringView name) const noexcept {
    const auto i = group_index(name);
    return i && matched(*i);
}

std::string_view RegexMatch::text(std::size_t group) const noexcept {
    if (!matched(group)) return std::string_view();
    return m_subject.substr(m_spans[group].first, m_spans[group].second - m_spans[group].first);
}

std::string_view RegexMatch::text(StringView name) const noexcept {
    const auto i = group_index(name);
    return i ? text(*i) : std::string_view();
}

std::string RegexMatch::str(std::size_t group) const { return std::string(text(group)); }

std::string RegexMatch::str(StringView name) const { return std::string(text(name)); }

std::string_view RegexMatch::before() const noexcept { return matched(0) ? m_subject.substr(0, m_spans[0].first) : std::string_view(); }

std::string_view RegexMatch::after() const noexcept { return matched(0) ? m_subject.substr(m_spans[0].second) : std::string_view(); }

Regex::Regex() : Regex(StringView("")) {}

Regex::Regex(StringView pattern, RegexFlags flags) : m_pattern(pattern.data(), pattern.size()), m_flags(flags) {
    auto program = std::make_shared<Program>();
    program->uni = has_flag(flags, RegexFlags::unicode_classes);
    Parser parser(m_pattern, flags, *program);
    if (!parser.parse(program->ast)) {
        m_error = parser.error();
        m_error_position = parser.error_position();
        return;
    }
    Compiler compiler(*program);
    if (!compiler.compile()) {
        m_error = "the pattern is too big once its counted repeats are written out; use smaller counts";
        m_error_position = 0;
        return;
    }
    const std::vector<Inst>& code = program->subs[0].code;
    for (std::size_t pc = 1; pc < code.size(); ++pc) {
        const Inst& i = code[pc];
        if (i.op == Op::save) continue;
        if (i.op == Op::assertion && static_cast<AssertKind>(i.a) == AssertKind::text_start) program->anchored = true;
        if (i.op == Op::literal && !i.icase && i.ch < 0x80) program->first_byte = static_cast<char>(i.ch);
        break;
    }
    m_program = std::move(program);
}

std::size_t Regex::group_count() const noexcept { return m_program ? m_program->groups : 0; }

const std::vector<std::string>& Regex::group_names() const noexcept {
    static const std::vector<std::string> none;
    return m_program ? m_program->names : none;
}

std::optional<std::size_t> Regex::group_index(StringView name) const noexcept {
    if (!m_program) return std::nullopt;
    const std::string_view n(name.data(), name.size());
    for (std::size_t i = 1; i < m_program->names.size(); ++i) {
        if (m_program->names[i] == n) return i;
    }
    return std::nullopt;
}

std::optional<RegexMatch> Regex::run(StringView text, std::size_t start, bool anchored, bool whole) const {
    m_gave_up = false;
    if (!m_program) return std::nullopt;
    const std::string_view s(text.data(), text.size());
    if (start > s.size()) return std::nullopt;
    Runner runner(*m_program, s, m_step_limit);
    auto names = std::make_shared<const std::vector<std::string>>(m_program->names);
    std::size_t pos = start;
    while (pos <= s.size()) {
        if (!anchored && m_program->first_byte) {
            const std::size_t found = s.find(*m_program->first_byte, pos);
            if (found == std::string_view::npos) return std::nullopt;
            pos = found;
        }
        std::fill(runner.slots.begin(), runner.slots.end(), no_pos);
        std::fill(runner.regs.begin(), runner.regs.end(), no_pos);
        std::size_t end = 0;
        if (runner.run(0, pos, whole ? s.size() : no_pos, end)) {
            RegexMatch m;
            m.m_subject = s;
            m.m_names = names;
            m.m_spans.resize(m_program->groups);
            for (std::size_t g = 0; g < m_program->groups; ++g) m.m_spans[g] = { runner.slots[g * 2], runner.slots[g * 2 + 1] };
            m.m_spans[0] = { pos, end };
            return m;
        }
        if (runner.gave_up) {
            m_gave_up = true;
            return std::nullopt;
        }
        if (anchored || m_program->anchored) break;
        pos = next_start(s, pos);
    }
    return std::nullopt;
}

bool Regex::matches(StringView text) const { return run(text, 0, true, true).has_value(); }

bool Regex::contains(StringView text) const { return run(text, 0, false, false).has_value(); }

std::optional<RegexMatch> Regex::match(StringView text) const { return run(text, 0, true, true); }

std::optional<RegexMatch> Regex::match_at(StringView text, std::size_t position) const { return run(text, position, true, false); }

std::optional<RegexMatch> Regex::search(StringView text, std::size_t start) const { return run(text, start, false, false); }

std::vector<RegexMatch> Regex::find_all(StringView text) const {
    std::vector<RegexMatch> out;
    const std::string_view s(text.data(), text.size());
    std::size_t pos = 0;
    while (pos <= s.size()) {
        std::optional<RegexMatch> m = run(text, pos, false, false);
        if (!m) break;
        const std::size_t b = m->begin();
        const std::size_t e = m->end();
        out.push_back(std::move(*m));
        pos = e > b ? e : next_start(s, e);
    }
    return out;
}

std::size_t Regex::count(StringView text) const { return find_all(text).size(); }

std::string Regex::expand(const RegexMatch& m, StringView replacement) {
    const std::string_view r(replacement.data(), replacement.size());
    std::string out;
    for (std::size_t i = 0; i < r.size(); ++i) {
        const char c = r[i];
        if (c != '$' || i + 1 >= r.size()) {
            out += c;
            continue;
        }
        const char n = r[i + 1];
        if (n == '$') {
            out += '$';
            ++i;
        } else if (n == '&') {
            out += m.text(0);
            ++i;
        } else if (n == '`') {
            out += m.before();
            ++i;
        } else if (n == '\'') {
            out += m.after();
            ++i;
        } else if (n >= '0' && n <= '9') {
            std::size_t g = static_cast<std::size_t>(n - '0');
            std::size_t used = 1;
            if (i + 2 < r.size() && r[i + 2] >= '0' && r[i + 2] <= '9' && g * 10 + static_cast<std::size_t>(r[i + 2] - '0') < m.group_count()) {
                g = g * 10 + static_cast<std::size_t>(r[i + 2] - '0');
                used = 2;
            }
            if (g < m.group_count()) out += m.text(g);
            else out.append(r.substr(i, used + 1));
            i += used;
        } else if (n == '<' || n == '{') {
            const char close = n == '<' ? '>' : '}';
            const std::size_t e = r.find(close, i + 2);
            if (e == std::string_view::npos) {
                out += c;
                continue;
            }
            const std::string_view name = r.substr(i + 2, e - i - 2);
            bool digits = !name.empty() && std::all_of(name.begin(), name.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
            if (digits) out += m.text(static_cast<std::size_t>(std::stoul(std::string(name))));
            else out += m.text(StringView(name));
            i = e;
        } else {
            out += c;
        }
    }
    return out;
}

std::string Regex::replace(StringView text, StringView replacement, std::size_t limit) const {
    const std::string pattern(replacement.data(), replacement.size());
    return replace(text, [&](const RegexMatch& m) { return expand(m, pattern); }, limit);
}

std::string Regex::replace(StringView text, const std::function<std::string(const RegexMatch&)>& replacement, std::size_t limit) const {
    const std::string_view s(text.data(), text.size());
    std::string out;
    std::size_t copied = 0;
    std::size_t pos = 0;
    std::size_t done = 0;
    while (pos <= s.size() && done < limit) {
        std::optional<RegexMatch> m = run(text, pos, false, false);
        if (!m) break;
        out.append(s.substr(copied, m->begin() - copied));
        out += replacement(*m);
        copied = m->end();
        ++done;
        if (m->end() > m->begin()) {
            pos = m->end();
        } else {
            if (m->end() < s.size()) {
                const std::size_t next = next_start(s, m->end());
                out.append(s.substr(m->end(), next - m->end()));
                copied = next;
            }
            pos = next_start(s, m->end());
        }
    }
    if (copied < s.size()) out.append(s.substr(copied));
    return out;
}

std::string Regex::replace_first(StringView text, StringView replacement) const { return replace(text, replacement, 1); }

std::vector<std::string> Regex::split(StringView text, std::size_t limit) const {
    const std::string_view s(text.data(), text.size());
    std::vector<std::string> out;
    std::size_t last = 0;
    std::size_t pos = 0;
    while (pos <= s.size() && (limit == npos || out.size() + 1 < limit)) {
        std::optional<RegexMatch> m = run(text, pos, false, false);
        if (!m) break;
        if (m->end() == m->begin()) {
            if (m->begin() >= s.size()) break;
            if (m->begin() == last) {
                pos = next_start(s, m->begin());
                continue;
            }
        }
        out.emplace_back(s.substr(last, m->begin() - last));
        for (std::size_t g = 1; g < m->group_count(); ++g) {
            if (m->matched(g)) out.push_back(m->str(g));
        }
        last = m->end();
        pos = m->end() > m->begin() ? m->end() : next_start(s, m->end());
    }
    out.emplace_back(s.substr(std::min(last, s.size())));
    return out;
}

std::string Regex::explain() const {
    if (!ok()) return "This pattern doesn't work: " + m_error + " (at character " + std::to_string(m_error_position + 1) + ").\n";
    std::string out = Explainer(*m_program).run();
    std::vector<std::string> notes;
    if (has_flag(m_flags, RegexFlags::ignore_case)) notes.push_back("upper and lower case count as the same");
    if (has_flag(m_flags, RegexFlags::multiline)) notes.push_back("^ and $ match at every line");
    if (has_flag(m_flags, RegexFlags::dot_all)) notes.push_back(". matches line breaks too");
    if (has_flag(m_flags, RegexFlags::unicode_classes)) notes.push_back("\\d, \\w, \\s and \\b understand every script, not just ASCII");
    if (!notes.empty()) out += "(" + join_words(notes, "and") + ")\n";
    return out;
}

std::string Regex::escape(StringView literal) {
    std::string out;
    for (std::size_t i = 0; i < literal.size(); ++i) {
        const char c = literal[i];
        switch (c) {
            case '\\':
            case '^':
            case '$':
            case '.':
            case '|':
            case '?':
            case '*':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '#':
                out += '\\';
                out += c;
                break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case ' ': out += ' '; break;
            default: out += c; break;
        }
    }
    return out;
}

Pattern Pattern::text(StringView literal) {
    const std::size_t points = decode_all(literal).size();
    return Pattern(Regex::escape(literal), points == 1 ? Shape::atom : points == 0 ? Shape::empty : Shape::sequence);
}

Pattern Pattern::one_of(const CharSet& set) { return Pattern(set.to_regex(), Shape::atom); }

Pattern Pattern::one_of(StringView chars) { return one_of(CharSet::of(chars)); }

Pattern Pattern::not_one_of(StringView chars) { return one_of(CharSet::everything().disable(chars)); }

Pattern Pattern::any_character() { return Pattern("[\\s\\S]", Shape::atom); }

Pattern Pattern::any_except_newline() { return Pattern(".", Shape::atom); }

Pattern Pattern::digit() { return Pattern("\\d", Shape::atom); }

Pattern Pattern::letter() { return Pattern("\\p{L}", Shape::atom); }

Pattern Pattern::whitespace() { return Pattern("\\s", Shape::atom); }

Pattern Pattern::word_character() { return Pattern("\\w", Shape::atom); }

Pattern Pattern::start_of_text() { return Pattern("\\A", Shape::atom); }

Pattern Pattern::end_of_text() { return Pattern("\\z", Shape::atom); }

Pattern Pattern::start_of_line() { return Pattern("(?m:^)", Shape::atom); }

Pattern Pattern::end_of_line() { return Pattern("(?m:$)", Shape::atom); }

Pattern Pattern::word_boundary() { return Pattern("\\b", Shape::atom); }

Pattern Pattern::either(std::initializer_list<Pattern> options) { return either(std::vector<Pattern>(options)); }

Pattern Pattern::either(const std::vector<Pattern>& options) {
    if (options.empty()) return Pattern();
    if (options.size() == 1) return options.front();
    std::string out;
    for (std::size_t i = 0; i < options.size(); ++i) {
        if (i) out += '|';
        out += options[i].m_regex;
    }
    return Pattern(out, Shape::alternation);
}

Pattern Pattern::sequence(std::initializer_list<Pattern> parts) {
    Pattern out;
    for (const Pattern& p : parts) out.then(p);
    return out;
}

Pattern Pattern::same_as(StringView group_name) { return Pattern("\\k<" + std::string(group_name.data(), group_name.size()) + ">", Shape::atom); }

Pattern Pattern::same_as(std::size_t group_number) { return Pattern("(?:\\" + std::to_string(group_number) + ")", Shape::atom); }

Pattern Pattern::raw(StringView regex) {
    const std::string r(regex.data(), regex.size());
    if (r.empty()) return Pattern();
    return Pattern("(?:" + r + ")", Shape::atom);
}

Pattern Pattern::digits(std::size_t at_least) { return digit().at_least(at_least); }

Pattern Pattern::integer(bool allow_sign) { return Pattern(allow_sign ? "[-+]?\\d+" : "\\d+", Shape::sequence); }

Pattern Pattern::decimal(bool allow_sign) { return Pattern(std::string(allow_sign ? "[-+]?" : "") + "(?:\\d+(?:\\.\\d*)?|\\.\\d+)", Shape::sequence); }

Pattern Pattern::identifier() { return Pattern("[\\p{L}_][\\p{L}\\p{N}_]*", Shape::sequence); }

Pattern Pattern::hex_number() { return Pattern("0[xX][0-9A-Fa-f]+", Shape::sequence); }

Pattern Pattern::hex_color() { return Pattern("#(?:[0-9A-Fa-f]{8}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{3,4})\\b", Shape::sequence); }

Pattern Pattern::email() { return Pattern("[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)*\\.[A-Za-z]{2,}", Shape::sequence); }

Pattern Pattern::word() { return Pattern("\\w+", Shape::quantified); }

std::string Pattern::as_atom() const {
    if (m_shape == Shape::atom) return m_regex;
    if (m_shape == Shape::empty) return "(?:)";
    return "(?:" + m_regex + ")";
}

std::string Pattern::as_part() const {
    if (m_shape == Shape::alternation) return "(?:" + m_regex + ")";
    return m_regex;
}

Pattern& Pattern::then(const Pattern& next) {
    if (next.m_shape == Shape::empty) return *this;
    if (m_shape == Shape::empty) {
        *this = next;
        if (m_shape == Shape::alternation) {
            m_regex = "(?:" + m_regex + ")";
            m_shape = Shape::atom;
        }
        return *this;
    }
    m_regex = as_part() + next.as_part();
    m_shape = Shape::sequence;
    return *this;
}

Pattern& Pattern::then(StringView literal) { return then(text(literal)); }

Pattern& Pattern::then_one_of(const CharSet& set) { return then(one_of(set)); }

Pattern& Pattern::or_else(const Pattern& other) {
    if (m_shape == Shape::empty) {
        *this = other;
        return *this;
    }
    m_regex = m_regex + "|" + other.m_regex;
    m_shape = Shape::alternation;
    return *this;
}

Pattern Pattern::quantify(const std::string& suffix) const { return Pattern(as_atom() + suffix, Shape::quantified); }

Pattern Pattern::optional() const { return quantify("?"); }

Pattern Pattern::one_or_more() const { return quantify("+"); }

Pattern Pattern::zero_or_more() const { return quantify("*"); }

Pattern Pattern::repeat(std::size_t times) const { return quantify("{" + std::to_string(times) + "}"); }

Pattern Pattern::repeat(std::size_t min, std::size_t max) const { return quantify("{" + std::to_string(min) + "," + std::to_string(max) + "}"); }

Pattern Pattern::at_least(std::size_t times) const {
    if (times == 0) return zero_or_more();
    if (times == 1) return one_or_more();
    return quantify("{" + std::to_string(times) + ",}");
}

Pattern Pattern::at_most(std::size_t times) const { return quantify("{0," + std::to_string(times) + "}"); }

Pattern Pattern::lazy() const {
    if (m_shape != Shape::quantified || m_regex.empty() || m_regex.back() == '?' || m_regex.back() == '+') {
        if (m_shape == Shape::quantified && !m_regex.empty() && (m_regex.back() == '?' || m_regex.back() == '+')) {
            const char last = m_regex.back();
            const char before = m_regex.size() > 1 ? m_regex[m_regex.size() - 2] : '\0';
            if (last == '+' && (before == '*' || before == '+' || before == '?' || before == '}')) return *this;
            if (last == '?' && (before == '*' || before == '+' || before == '?' || before == '}')) return *this;
            return Pattern(m_regex + "?", Shape::quantified);
        }
        return *this;
    }
    return Pattern(m_regex + "?", Shape::quantified);
}

Pattern Pattern::possessive() const {
    if (m_shape != Shape::quantified) return *this;
    return Pattern(m_regex + "+", Shape::quantified);
}

Pattern Pattern::capture() const { return Pattern("(" + m_regex + ")", Shape::atom); }

Pattern Pattern::capture(StringView name) const { return Pattern("(?<" + std::string(name.data(), name.size()) + ">" + m_regex + ")", Shape::atom); }

Pattern Pattern::group() const { return Pattern("(?:" + m_regex + ")", Shape::atom); }

Pattern Pattern::followed_by(const Pattern& next) const { return Pattern(as_part() + "(?=" + next.m_regex + ")", Shape::sequence); }

Pattern Pattern::not_followed_by(const Pattern& next) const { return Pattern(as_part() + "(?!" + next.m_regex + ")", Shape::sequence); }

Pattern Pattern::preceded_by(const Pattern& before) const { return Pattern("(?<=" + before.m_regex + ")" + as_part(), Shape::sequence); }

Pattern Pattern::not_preceded_by(const Pattern& before) const { return Pattern("(?<!" + before.m_regex + ")" + as_part(), Shape::sequence); }

Pattern Pattern::whole() const { return Pattern("\\A" + as_part() + "\\z", Shape::sequence); }

Regex Pattern::compile(RegexFlags flags) const { return Regex(m_regex, flags); }

bool Pattern::matches(StringView text, RegexFlags flags) const { return compile(flags).matches(text); }

std::string Pattern::explain() const { return compile().explain(); }

} // namespace fizmo
