#include "fizmo_library.hpp"
#include "text.hpp"
#include "fancy_math.hpp"
#include "fancy_text_render.hpp"
#include "../Images/Formats/image_formats.hpp"
#include "../String/string.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace text {

namespace {

constexpr double kDefaultSize = 16.0;

std::string wide_to_utf8(const std::wstring& w) {
    std::string out;
    for (std::size_t i = 0; i < w.size(); ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(w[i]);
        if (sizeof(wchar_t) == 2 && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < w.size()) {
            const std::uint32_t lo = static_cast<std::uint32_t>(w[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::size_t utf8_next(std::string_view s, std::size_t i) noexcept {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
    return i;
}

bool is_word_char(char c) noexcept { return std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) >= 0x80; }

bool parse_color_token(std::string_view v, graphics::Color& out) {
    std::string s;
    for (char c : v) if (!std::isspace(static_cast<unsigned char>(c))) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (s.empty()) return false;
    if (s[0] == '#') {
        auto hex = [](char c) -> int { return std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1; };
        const std::string h = s.substr(1);
        for (char c : h) if (hex(c) < 0) return false;
        if (h.size() == 3 || h.size() == 4) {
            out = graphics::Color(static_cast<std::uint8_t>(hex(h[0]) * 17), static_cast<std::uint8_t>(hex(h[1]) * 17), static_cast<std::uint8_t>(hex(h[2]) * 17), static_cast<std::uint8_t>(h.size() == 4 ? hex(h[3]) * 17 : 255));
            return true;
        }
        if (h.size() == 6 || h.size() == 8) {
            out = graphics::Color(static_cast<std::uint8_t>(hex(h[0]) * 16 + hex(h[1])), static_cast<std::uint8_t>(hex(h[2]) * 16 + hex(h[3])), static_cast<std::uint8_t>(hex(h[4]) * 16 + hex(h[5])), static_cast<std::uint8_t>(h.size() == 8 ? hex(h[6]) * 16 + hex(h[7]) : 255));
            return true;
        }
        return false;
    }
    static const struct { const char* name; std::uint32_t rgb; } named[] = {
        { "black", 0x000000 }, { "white", 0xFFFFFF }, { "red", 0xFF0000 }, { "green", 0x008000 }, { "lime", 0x00FF00 }, { "blue", 0x0000FF },
        { "yellow", 0xFFFF00 }, { "cyan", 0x00FFFF }, { "aqua", 0x00FFFF }, { "magenta", 0xFF00FF }, { "fuchsia", 0xFF00FF }, { "gray", 0x808080 },
        { "grey", 0x808080 }, { "silver", 0xC0C0C0 }, { "maroon", 0x800000 }, { "olive", 0x808000 }, { "navy", 0x000080 }, { "purple", 0x800080 },
        { "teal", 0x008080 }, { "orange", 0xFFA500 }, { "pink", 0xFFC0CB }, { "brown", 0xA52A2A }, { "gold", 0xFFD700 }, { "indigo", 0x4B0082 },
        { "violet", 0xEE82EE }, { "crimson", 0xDC143C }, { "coral", 0xFF7F50 }, { "salmon", 0xFA8072 }, { "turquoise", 0x40E0D0 }, { "skyblue", 0x87CEEB },
        { "tomato", 0xFF6347 }, { "orchid", 0xDA70D6 }, { "khaki", 0xF0E68C }, { "plum", 0xDDA0DD }, { "tan", 0xD2B48C }, { "beige", 0xF5F5DC },
        { "blurple", 0x5865F2 },
    };
    for (const auto& n : named) {
        if (s == n.name) {
            out = graphics::Color(static_cast<std::uint8_t>(n.rgb >> 16), static_cast<std::uint8_t>(n.rgb >> 8), static_cast<std::uint8_t>(n.rgb));
            return true;
        }
    }
    return false;
}

class MarkupParser {
public:
    MarkupParser(Text& out, const MarkupOptions& opt) : m_out(out), m_opt(opt) {}

    void parse(std::string_view src, const TextStyle& style, const std::string& family) {
        std::size_t line_start = 0;
        bool in_code_block = false;
        (void)in_code_block;

        while (line_start <= src.size()) {
            if (m_opt.code_blocks && src.compare(line_start, 3, "```") == 0) {
                const std::size_t end = src.find("```", line_start + 3);
                if (end != std::string_view::npos) {
                    std::string_view body = src.substr(line_start + 3, end - line_start - 3);
                    const std::size_t nl = body.find('\n');
                    if (nl != std::string_view::npos) {
                        bool lang = true;
                        for (char c : body.substr(0, nl)) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '#' && c != '-') lang = false;
                        if (lang) body = body.substr(nl + 1);
                    }
                    while (!body.empty() && body.back() == '\n') body.remove_suffix(1);
                    TextStyle cs = style;
                    cs.set_bg_color(m_opt.code_background);
                    cs.set_fg_color(m_opt.code_color);
                    std::size_t p = 0;
                    while (p <= body.size()) {
                        const std::size_t e = body.find('\n', p);
                        const std::string_view line = body.substr(p, e == std::string_view::npos ? std::string_view::npos : e - p);
                        if (!line.empty()) m_out.add(std::string(line), cs, m_opt.code_family);
                        if (e == std::string_view::npos) break;
                        m_out.newline();
                        p = e + 1;
                    }
                    std::size_t next = end + 3;
                    if (next < src.size() && src[next] == '\n') {
                        m_out.newline();
                        ++next;
                    }
                    line_start = next;
                    if (line_start >= src.size()) break;
                    continue;
                }
            }

            const std::size_t nl = src.find('\n', line_start);
            std::string_view line = src.substr(line_start, nl == std::string_view::npos ? std::string_view::npos : nl - line_start);
            TextStyle ls = style;

            if (m_opt.headings && !line.empty() && line[0] == '#') {
                int level = 0;
                while (level < static_cast<int>(line.size()) && line[static_cast<std::size_t>(level)] == '#') ++level;
                if (level <= 3 && static_cast<std::size_t>(level) < line.size() && line[static_cast<std::size_t>(level)] == ' ') {
                    const double base = ls.has_size() ? ls.size() : kDefaultSize;
                    ls.set_size(base * (level == 1 ? 1.5 : level == 2 ? 1.25 : 1.1));
                    ls.set_bold(true);
                    line.remove_prefix(static_cast<std::size_t>(level) + 1);
                }
            }

            if (m_opt.quotes && line.size() >= 2 && line[0] == '>' && line[1] == ' ') {
                TextSegment bar;
                bar.kind = TextSegmentKind::QuoteBar;
                bar.style.set_fg_color(m_opt.quote_bar_color);
                m_out.segments().push_back(std::move(bar));
                line.remove_prefix(2);
            }

            if (m_opt.lists && line.size() >= 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
                m_out.add("\xE2\x80\xA2 ", ls, family);
                line.remove_prefix(2);
            }

            inline_parse(line, ls, family, 0);
            if (nl == std::string_view::npos) break;
            m_out.newline();
            line_start = nl + 1;
        }
    }

private:
    void flush(std::string& buf, const TextStyle& style, const std::string& family, std::int32_t link) {
        if (buf.empty()) return;
        if (link >= 0) {
            TextSegment seg;
            seg.kind = TextSegmentKind::Text;
            seg.content = std::move(buf);
            seg.style = style;
            seg.family = family;
            seg.link = link;
            m_out.segments().push_back(std::move(seg));
        } else {
            m_out.add(std::move(buf), style, family);
        }
        buf.clear();
    }

    static std::size_t find_closing(std::string_view s, std::size_t from, std::string_view delim, bool word_boundary) {
        std::size_t i = from;
        while (i < s.size()) {
            if (s[i] == '\\') { i += 2; continue; }
            if (s.compare(i, delim.size(), delim) == 0) {
                if (i == from) { i += 1; continue; }
                if (word_boundary) {
                    const std::size_t after = i + delim.size();
                    if (after < s.size() && is_word_char(s[after])) { i += 1; continue; }
                }
                if (delim.size() == 1 && delim[0] == '*' && i + 1 < s.size() && s[i + 1] == '*') { i += 2; continue; }
                return i;
            }
            ++i;
        }
        return std::string_view::npos;
    }

    void inline_parse(std::string_view s, const TextStyle& style, const std::string& family, int depth, std::int32_t link = -1) {
        std::string buf;
        std::size_t i = 0;

        auto wrap = [&](std::size_t open_len, std::string_view delim, bool word_boundary, auto apply) -> bool {
            const std::size_t close = find_closing(s, i + open_len, delim, word_boundary);
            if (close == std::string_view::npos || depth > 32) return false;
            flush(buf, style, family, link);
            TextStyle inner = style;
            apply(inner);
            inline_parse(s.substr(i + open_len, close - i - open_len), inner, family, depth + 1, link);
            i = close + delim.size();
            return true;
        };

        while (i < s.size()) {
            const char c = s[i];

            if (c == '\\' && i + 1 < s.size() && std::ispunct(static_cast<unsigned char>(s[i + 1]))) {
                buf.push_back(s[i + 1]);
                i += 2;
                continue;
            }

            if (m_opt.code && c == '`') {
                std::size_t ticks = 1;
                while (i + ticks < s.size() && s[i + ticks] == '`' && ticks < 2) ++ticks;
                const std::string delim(ticks, '`');
                const std::size_t close = s.find(delim, i + ticks);
                if (close != std::string_view::npos && close > i + ticks) {
                    flush(buf, style, family, link);
                    TextStyle cs = style;
                    cs.set_bg_color(m_opt.code_background);
                    cs.set_fg_color(m_opt.code_color);
                    std::string body(s.substr(i + ticks, close - i - ticks));
                    if (ticks == 2 && body.size() >= 2 && body.front() == ' ' && body.back() == ' ') body = body.substr(1, body.size() - 2);
                    m_out.add(std::move(body), cs, m_opt.code_family);
                    i = close + ticks;
                    continue;
                }
            }

            if (m_opt.math && c == '$') {
                const bool display = i + 1 < s.size() && s[i + 1] == '$';
                const std::size_t open = display ? 2 : 1;
                std::size_t close = std::string_view::npos;
                if (display) close = s.find("$$", i + 2);
                else if (i + 1 < s.size() && s[i + 1] != ' ') {
                    for (std::size_t k = i + 1; k < s.size(); ++k) {
                        if (s[k] == '\\') { ++k; continue; }
                        if (s[k] == '$') {
                            if (s[k - 1] != ' ' && !(k + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[k + 1])))) close = k;
                            break;
                        }
                    }
                }
                if (close != std::string_view::npos && close > i + open) {
                    flush(buf, style, family, link);
                    m_out.add_math(std::string(s.substr(i + open, close - i - open)), style, display);
                    i = close + open;
                    continue;
                }
            }

            if (m_opt.spoilers && s.compare(i, 2, "||") == 0) {
                const graphics::Color sc = m_opt.spoiler_color;
                if (wrap(2, "||", false, [&](TextStyle& t) { t.set_bg_color(sc); t.set_fg_color(sc); })) continue;
            }

            if (m_opt.bold && m_opt.italic && s.compare(i, 3, "***") == 0) {
                if (wrap(3, "***", false, [](TextStyle& t) { t.set_bold(true); t.set_italic(true); })) continue;
            }

            if (m_opt.bold && s.compare(i, 2, "**") == 0) {
                if (wrap(2, "**", false, [](TextStyle& t) { t.set_bold(true); })) continue;
            }

            if (m_opt.underline && s.compare(i, 2, "__") == 0) {
                if (wrap(2, "__", false, [](TextStyle& t) { t.set_underline(true); })) continue;
            }

            if (m_opt.strikethrough && s.compare(i, 2, "~~") == 0) {
                if (wrap(2, "~~", false, [](TextStyle& t) { t.set_strikethrough(true); })) continue;
            }

            if (m_opt.italic && c == '*' && i + 1 < s.size() && s[i + 1] != ' ') {
                if (wrap(1, "*", false, [](TextStyle& t) { t.set_italic(true); })) continue;
            }

            if (m_opt.italic && c == '_' && (i == 0 || !is_word_char(s[i - 1])) && i + 1 < s.size() && s[i + 1] != ' ') {
                if (wrap(1, "_", true, [](TextStyle& t) { t.set_italic(true); })) continue;
            }

            if (m_opt.emoji && c == ':') {
                std::size_t k = i + 1;
                while (k < s.size() && (std::isalnum(static_cast<unsigned char>(s[k])) || s[k] == '_' || s[k] == '+' || s[k] == '-')) ++k;
                if (k < s.size() && s[k] == ':' && k > i + 1) {
                    const std::string_view name = s.substr(i + 1, k - i - 1);
                    if (EmojiImage img = emoji_registry().find(name)) {
                        flush(buf, style, family, link);
                        TextSegment seg;
                        seg.kind = TextSegmentKind::Image;
                        seg.content = ":" + std::string(name) + ":";
                        seg.style = style;
                        seg.image = std::move(img);
                        seg.link = link;
                        m_out.segments().push_back(std::move(seg));
                        i = k + 1;
                        continue;
                    }
                    const std::string_view uni = unicode_emoji(name);
                    if (!uni.empty()) {
                        buf.append(uni.data(), uni.size());
                        i = k + 1;
                        continue;
                    }
                }
            }

            if (m_opt.color_tags && c == '[') {
                bool handled = false;
                static const char* const tags[] = { "color", "bg", "size", "font", "b", "i", "u", "s" };
                for (const char* tag : tags) {
                    const std::size_t tl = std::strlen(tag);
                    if (s.compare(i + 1, tl, tag) != 0) continue;
                    const std::size_t after = i + 1 + tl;
                    if (after >= s.size() || (s[after] != '=' && s[after] != ']')) continue;
                    const std::size_t rb = s.find(']', after);
                    if (rb == std::string_view::npos) continue;
                    const std::string_view value = s[after] == '=' ? s.substr(after + 1, rb - after - 1) : std::string_view();
                    const std::string closer = "[/" + std::string(tag) + "]";
                    const std::size_t close = s.find(closer, rb + 1);
                    if (close == std::string_view::npos) continue;
                    TextStyle inner = style;
                    std::string fam = family;
                    graphics::Color col;
                    const std::string t = tag;
                    if (t == "color") { if (!parse_color_token(value, col)) continue; inner.set_fg_color(col); }
                    else if (t == "bg") { if (!parse_color_token(value, col)) continue; inner.set_bg_color(col); }
                    else if (t == "size") {
                        const double v = std::strtod(std::string(value).c_str(), nullptr);
                        if (!(v > 0 && v < 1000)) continue;
                        inner.set_size(v);
                    } else if (t == "font") fam = std::string(value);
                    else if (t == "b") inner.set_bold(true);
                    else if (t == "i") inner.set_italic(true);
                    else if (t == "u") inner.set_underline(true);
                    else if (t == "s") inner.set_strikethrough(true);
                    flush(buf, style, family, link);
                    inline_parse(s.substr(rb + 1, close - rb - 1), inner, fam, depth + 1, link);
                    i = close + closer.size();
                    handled = true;
                    break;
                }
                if (handled) continue;
            }

            if (m_opt.links && c == '[' && link < 0) {
                const std::size_t rb = find_closing(s, i + 1, "]", false);
                if (rb != std::string_view::npos && rb + 1 < s.size() && s[rb + 1] == '(') {
                    const std::size_t rp = s.find(')', rb + 2);
                    if (rp != std::string_view::npos) {
                        flush(buf, style, family, link);
                        const std::string label(s.substr(i + 1, rb - i - 1));
                        const std::string target(s.substr(rb + 2, rp - rb - 2));
                        TextStyle ls = style;
                        ls.set_fg_color(m_opt.link_color);
                        ls.set_underline(true);
                        const std::int32_t id = register_link(target, label);
                        inline_parse(label, ls, family, depth + 1, id);
                        i = rp + 1;
                        continue;
                    }
                }
            }

            if (m_opt.links && link < 0 && (s.compare(i, 8, "https://") == 0 || s.compare(i, 7, "http://") == 0) && (i == 0 || !is_word_char(s[i - 1]))) {
                std::size_t k = i;
                while (k < s.size() && !std::isspace(static_cast<unsigned char>(s[k])) && s[k] != '<' && s[k] != '>') ++k;
                while (k > i && (s[k - 1] == '.' || s[k - 1] == ',' || s[k - 1] == ')' || s[k - 1] == '!' || s[k - 1] == '?')) --k;
                flush(buf, style, family, link);
                const std::string url(s.substr(i, k - i));
                TextStyle ls = style;
                ls.set_fg_color(m_opt.link_color);
                ls.set_underline(true);
                std::string copy = url;
                flush(copy, ls, family, register_link(url, url));
                i = k;
                continue;
            }

            const std::size_t n = utf8_next(s, i);
            buf.append(s.data() + i, n - i);
            i = n;
        }

        flush(buf, style, family, link);
    }

    std::int32_t register_link(const std::string& target, const std::string& label) { return m_out.register_link(target, label); }

    Text&                m_out;
    const MarkupOptions& m_opt;
};


bool is_alpha(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

std::size_t skip_spaces(std::string_view s, std::size_t i) noexcept {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n')) ++i;
    return i;
}

std::string latex_to_mathtext(std::string_view s, int depth = 0);

bool read_group(std::string_view s, std::size_t& i, std::string& out) {
    i = skip_spaces(s, i);
    if (i >= s.size()) return false;
    if (s[i] == '{') {
        int level = 0;
        const std::size_t start = i + 1;
        for (std::size_t k = i; k < s.size(); ++k) {
            if (s[k] == '\\' && k + 1 < s.size()) { ++k; continue; }
            if (s[k] == '{') ++level;
            else if (s[k] == '}' && --level == 0) {
                out = std::string(s.substr(start, k - start));
                i = k + 1;
                return true;
            }
        }
        return false;
    }
    if (s[i] == '\\') {
        std::size_t k = i + 1;
        while (k < s.size() && is_alpha(s[k])) ++k;
        if (k == i + 1 && k < s.size()) ++k;
        out = std::string(s.substr(i, k - i));
        i = k;
        return true;
    }
    const std::size_t k = utf8_next(s, i);
    out = std::string(s.substr(i, k - i));
    i = k;
    return true;
}

std::string latex_to_mathtext(std::string_view s, int depth) {
    if (depth > 32) return std::string(s);
    std::string out;
    std::size_t i = 0;
    std::vector<int> set_depth;
    int brace = 0;

    while (i < s.size()) {
        const char c = s[i];
        if (c != '\\') {
            if (c == '{') ++brace;
            else if (c == '}') --brace;
            if (c == ',' && !set_depth.empty() && set_depth.back() == brace) out += "\\comma\\ ";
            else out.push_back(c);
            ++i;
            continue;
        }

        if (i + 1 < s.size() && !is_alpha(s[i + 1])) {
            const char n = s[i + 1];
            if (n == ',' || n == ';' || n == ':' || n == '!' || n == ' ') { out.push_back(' '); i += 2; continue; }
            if (n == '\\') { out.push_back(' '); i += 2; continue; }
            if (n == '{') set_depth.push_back(brace);
            if (n == '}' && !set_depth.empty()) set_depth.pop_back();
            if (n == '{' || n == '}' || n == '|' || n == '%' || n == '$' || n == '#' || n == '&' || n == '_') {
                out += "\\";
                out.push_back(n);
                out += "\\";
                i += 2;
                if (i < s.size() && s[i] == '\\' && (i + 1 >= s.size() || !is_alpha(s[i + 1]))) ++i;
                continue;
            }
            out.push_back(c);
            ++i;
            continue;
        }

        std::size_t k = i + 1;
        while (k < s.size() && is_alpha(s[k])) ++k;
        const std::string name(s.substr(i + 1, k - i - 1));
        const bool closed = k < s.size() && s[k] == '\\';
        if (closed) {
            out.append(s.data() + i, k + 1 - i);
            i = k + 1;
            continue;
        }

        if (name == "left" || name == "right" || name == "big" || name == "Big" || name == "bigg" || name == "Bigg" || name == "bigl" || name == "bigr" ||
            name == "Bigl" || name == "Bigr" || name == "displaystyle" || name == "textstyle" || name == "scriptstyle" || name == "limits" || name == "nolimits") {
            i = k;
            if ((name == "left" || name == "right") && i < s.size() && s[i] == '.') ++i;
            continue;
        }

        static const char* const operator_names[] = { "sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan", "sinh", "cosh", "tanh", "coth",
                                                      "log", "ln", "lg", "exp", "lim", "limsup", "liminf", "max", "min", "sup", "inf", "det", "dim", "gcd",
                                                      "lcm", "deg", "arg", "ker", "hom", "Pr", "mod", "bmod", "sgn", "tr", "rank", "argmax", "argmin" };
        bool op_name = false;
        for (const char* on : operator_names) if (name == on) op_name = true;
        if (op_name) {
            const std::size_t nx = skip_spaces(s, k);
            const bool script = nx < s.size() && (s[nx] == '_' || s[nx] == '^');
            out += " \"" + name + (script ? "\xE2\x80\x8B\"" : "\xE2\x80\x89\" ");
            i = k;
            continue;
        }

        if (name == "quad" || name == "qquad") {
            out += "  ";
            i = k;
            continue;
        }

        if (name == "frac" || name == "dfrac" || name == "tfrac" || name == "cfrac" || name == "binom" || name == "dbinom" || name == "tbinom" || name == "overset" || name == "underset" || name == "stackrel") {
            std::size_t j = k;
            std::string a, b;
            if (read_group(s, j, a) && read_group(s, j, b)) {
                const std::string fn = (name == "dfrac" || name == "tfrac" || name == "cfrac") ? "frac" : (name == "dbinom" || name == "tbinom") ? "binom" : name;
                out += "\\" + fn + "\\{" + latex_to_mathtext(a, depth + 1) + ", " + latex_to_mathtext(b, depth + 1) + "}";
                i = j;
                continue;
            }
        }

        if (name == "sqrt") {
            std::size_t j = skip_spaces(s, k);
            std::string index, body;
            if (j < s.size() && s[j] == '[') {
                const std::size_t close = s.find(']', j);
                if (close != std::string_view::npos) {
                    index = std::string(s.substr(j + 1, close - j - 1));
                    j = close + 1;
                }
            }
            if (read_group(s, j, body)) {
                if (index.empty()) out += "\\sqrt\\{" + latex_to_mathtext(body, depth + 1) + "}";
                else out += "\\root\\{" + latex_to_mathtext(index, depth + 1) + ", " + latex_to_mathtext(body, depth + 1) + "}";
                i = j;
                continue;
            }
        }

        if (name == "text" || name == "mathrm" || name == "textrm" || name == "operatorname" || name == "textit" || name == "textbf" || name == "mbox") {
            std::size_t j = k;
            std::string body;
            if (read_group(s, j, body)) {
                std::string quoted = "\"";
                for (char ch : body) {
                    if (ch == '"' || ch == '\\') quoted.push_back('\\');
                    quoted.push_back(ch);
                }
                quoted += "\xE2\x80\x8B\"";
                out += quoted;
                i = j;
                continue;
            }
        }

        static const struct { const char* name; const char* mark; bool quote; } accents[] = {
            { "hat", "\xCC\x82", false }, { "widehat", "\xCC\x82", false }, { "tilde", "\xCC\x83", false }, { "widetilde", "\xCC\x83", false },
            { "bar", "\xCC\x84", false }, { "overline", "\xCC\x85", false }, { "dot", "\xCC\x87", false }, { "ddot", "\xCC\x88", false },
            { "acute", "\xCC\x81", false }, { "grave", "\xCC\x80", false }, { "check", "\xCC\x8C", false }, { "breve", "\xCC\x86", false },
            { "vec", "\xE2\x83\x97", true }, { "overrightarrow", "\xE2\x83\x97", true }, { "underline", "\xCC\xB2", false },
        };
        bool accented = false;
        for (const auto& ac : accents) {
            if (name != ac.name) continue;
            std::size_t j = k;
            std::string body;
            if (!read_group(s, j, body)) break;
            const std::string inner = latex_to_mathtext(body, depth + 1);
            const bool single = utf8_next(inner, 0) == inner.size() && !inner.empty();
            if (single && !ac.quote) out += inner + ac.mark;
            else out += "\"" + inner + ac.mark + "\"";
            i = j;
            accented = true;
            break;
        }
        if (accented) continue;

        if (name == "mathbf" || name == "mathit" || name == "mathsf" || name == "mathtt" || name == "boldsymbol" || name == "bm") {
            std::size_t j = k;
            std::string body;
            if (read_group(s, j, body)) {
                out += "{" + latex_to_mathtext(body, depth + 1) + "}";
                i = j;
                continue;
            }
        }

        out += "\\" + name + "\\";
        i = k;
        std::size_t j = skip_spaces(s, i);
        if (j < s.size() && s[j] == '{' && mathtext::find_symbol(name) == mathtext::kNoSymbol) {
            std::string body;
            if (read_group(s, j, body)) {
                out += "{" + latex_to_mathtext(body, depth + 1) + "}";
                i = j;
            }
        }
    }

    return out;
}

struct Item {
    enum Kind : std::uint8_t { Run, Math, Image, Break, Bar } kind = Run;
    std::string  text;
    TextStyle    style;
    std::string  family;
    std::int32_t link = -1;
    FancyText    math;
    EmojiImage   image;
    float        width = 0, ascent = 0, descent = 0;
    bool         space_only = false;
    graphics::Color bar_color;
};

TextStyle resolved_style(const TextStyle& base, const TextStyle& over) {
    TextStyle s = base.merge_over(over);
    if (!s.has_size()) s.set_size(kDefaultSize);
    return s;
}

RichText make_run(const std::string& text, const TextStyle& style, const std::string& family) {
    RichText rt(style);
    rt.add(text, TextStyle(), family);
    return rt;
}

void measure_run(TextRasterizer& raster, Item& it) {
    const TextInk ink = raster.measure(make_run(it.text, it.style, it.family));
    it.width = ink.advance;
    it.ascent = ink.line_ascent > 0 ? ink.line_ascent : static_cast<float>(it.style.size() * 0.8);
    it.descent = ink.line_descent > 0 ? ink.line_descent : static_cast<float>(it.style.size() * 0.2);
}

} // namespace

bool TextSegment::operator==(const TextSegment& o) const noexcept {
    return kind == o.kind && content == o.content && style == o.style && family == o.family && image == o.image && image_width == o.image_width && image_height == o.image_height && link == o.link && display_math == o.display_math;
}

Text::Text(const char* utf8) { if (utf8 && *utf8) push_text(utf8, TextStyle(), std::string(), -1); }

Text::Text(const char* utf8, const TextStyle& style) : m_base(style) { if (utf8 && *utf8) push_text(utf8, TextStyle(), std::string(), -1); }

Text::Text(std::string utf8) { if (!utf8.empty()) push_text(std::move(utf8), TextStyle(), std::string(), -1); }

Text::Text(std::string utf8, const TextStyle& style) : m_base(style) { if (!utf8.empty()) push_text(std::move(utf8), TextStyle(), std::string(), -1); }

Text::Text(std::string_view utf8) { if (!utf8.empty()) push_text(std::string(utf8), TextStyle(), std::string(), -1); }

Text::Text(const std::wstring& wide) { if (!wide.empty()) push_text(wide_to_utf8(wide), TextStyle(), std::string(), -1); }

Text::Text(const String& str) { if (str.size()) push_text(str.to_std_string(), TextStyle(), std::string(), -1); }

Text::Text(const String& str, const TextStyle& style) : m_base(style) { if (str.size()) push_text(str.to_std_string(), TextStyle(), std::string(), -1); }

Text::Text(const RichText& rich) : m_base(rich.base()) {
    for (const TextSpan& sp : rich.spans()) push_text(sp.text, sp.style, sp.family, -1);
}

Text Text::plain(std::string utf8, const TextStyle& style) { return Text(std::move(utf8), style); }

Text Text::markup(std::string_view source, const TextStyle& base, const MarkupOptions& options) {
    Text t;
    t.m_base = base;
    t.add_markup(source, TextStyle(), options);
    if (options.jumbo_emoji) t.m_jumbo = true;
    return t;
}

Text Text::math(std::string_view source, const TextStyle& style, bool display) {
    Text t;
    t.add_math(std::string(source), style, display);
    return t;
}

Text Text::emoji(std::string_view name, const TextStyle& style) {
    Text t;
    t.add_emoji(name, style);
    return t;
}

Text Text::image(EmojiImage img, float width, float height) {
    Text t;
    t.add_image(std::move(img), width, height);
    return t;
}

void Text::push_text(std::string utf8, const TextStyle& style, std::string family, std::int32_t link) {
    std::size_t start = 0;
    while (start <= utf8.size()) {
        const std::size_t nl = utf8.find('\n', start);
        std::string piece = utf8.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!piece.empty() && piece.back() == '\r') piece.pop_back();
        if (!piece.empty()) {
            if (!m_segments.empty()) {
                TextSegment& last = m_segments.back();
                if (last.kind == TextSegmentKind::Text && last.style == style && last.family == family && last.link == link) {
                    last.content += piece;
                    piece.clear();
                }
            }
            if (!piece.empty()) {
                TextSegment seg;
                seg.kind = TextSegmentKind::Text;
                seg.content = std::move(piece);
                seg.style = style;
                seg.family = family;
                seg.link = link;
                m_segments.push_back(std::move(seg));
            }
        }
        if (nl == std::string::npos) break;
        TextSegment br;
        br.kind = TextSegmentKind::LineBreak;
        m_segments.push_back(std::move(br));
        start = nl + 1;
    }
}

Text& Text::add(std::string utf8, const TextStyle& style, std::string family) {
    if (!utf8.empty()) push_text(std::move(utf8), style, std::move(family), -1);
    return *this;
}

Text& Text::add(const Text& other) {
    const std::int32_t link_offset = static_cast<std::int32_t>(m_links.size());
    for (const TextLink& l : other.m_links) m_links.push_back(l);
    for (TextSegment seg : other.m_segments) {
        if (seg.kind != TextSegmentKind::LineBreak && seg.kind != TextSegmentKind::QuoteBar) seg.style = other.m_base.merge_over(seg.style);
        if (seg.family.empty()) seg.family = other.m_family;
        if (seg.link >= 0) seg.link += link_offset;
        if (seg.kind == TextSegmentKind::Text) {
            const std::int32_t lk = seg.link;
            push_text(std::move(seg.content), seg.style, std::move(seg.family), lk);
        } else {
            m_segments.push_back(std::move(seg));
        }
    }
    return *this;
}

Text& Text::add_markup(std::string_view source, const TextStyle& style, const MarkupOptions& options) {
    MarkupParser p(*this, options);
    p.parse(source, style, std::string());
    return *this;
}

Text& Text::add_math(std::string source, const TextStyle& style, bool display) {
    if (source.empty()) return *this;
    TextSegment seg;
    seg.kind = TextSegmentKind::Math;
    seg.content = std::move(source);
    seg.style = style;
    seg.display_math = display;
    m_segments.push_back(std::move(seg));
    return *this;
}

Text& Text::add_emoji(std::string_view name, const TextStyle& style) {
    std::string_view n = name;
    if (n.size() >= 2 && n.front() == ':' && n.back() == ':') n = n.substr(1, n.size() - 2);
    if (EmojiImage img = emoji_registry().find(n)) {
        TextSegment seg;
        seg.kind = TextSegmentKind::Image;
        seg.content = ":" + std::string(n) + ":";
        seg.style = style;
        seg.image = std::move(img);
        m_segments.push_back(std::move(seg));
        return *this;
    }
    const std::string_view uni = unicode_emoji(n);
    if (!uni.empty()) return add(std::string(uni), style);
    return add(":" + std::string(n) + ":", style);
}

Text& Text::add_image(EmojiImage img, float width, float height, std::string alt) {
    if (!img || img->empty()) return *this;
    TextSegment seg;
    seg.kind = TextSegmentKind::Image;
    seg.content = std::move(alt);
    seg.image = std::move(img);
    seg.image_width = width;
    seg.image_height = height;
    m_segments.push_back(std::move(seg));
    return *this;
}

Text& Text::add_link(std::string label, std::string target, const TextStyle& style) {
    const std::int32_t id = register_link(std::move(target), label);
    if (!label.empty()) push_text(std::move(label), style, std::string(), id);
    return *this;
}

std::int32_t Text::register_link(std::string target, std::string label) {
    m_links.push_back(TextLink{ std::move(target), std::move(label) });
    return static_cast<std::int32_t>(m_links.size() - 1);
}

Text& Text::newline() {
    TextSegment br;
    br.kind = TextSegmentKind::LineBreak;
    m_segments.push_back(std::move(br));
    return *this;
}

bool Text::empty() const noexcept {
    for (const TextSegment& s : m_segments)
        if (s.kind != TextSegmentKind::LineBreak && (s.kind != TextSegmentKind::Text || !s.content.empty())) return false;
    return true;
}

bool Text::has_math() const noexcept {
    for (const TextSegment& s : m_segments) if (s.kind == TextSegmentKind::Math) return true;
    return false;
}

bool Text::has_images() const noexcept {
    for (const TextSegment& s : m_segments) if (s.kind == TextSegmentKind::Image) return true;
    return false;
}

bool Text::animated() const noexcept {
    for (const TextSegment& s : m_segments) if (s.kind == TextSegmentKind::Image && s.image && s.image->animated()) return true;
    return false;
}

bool Text::is_simple() const noexcept {
    if (m_max_width > 0.0f || m_jumbo || !m_links.empty() || m_line_spacing != 1.0f || m_align != TextAlign::Left) return false;
    std::size_t texts = 0;
    for (const TextSegment& s : m_segments) {
        if (s.kind != TextSegmentKind::Text) return false;
        ++texts;
    }
    return texts <= 1;
}

std::string Text::plain_text() const {
    std::string out;
    for (const TextSegment& s : m_segments) {
        switch (s.kind) {
            case TextSegmentKind::LineBreak: out.push_back('\n'); break;
            case TextSegmentKind::QuoteBar: out += "> "; break;
            case TextSegmentKind::Math: out += (s.display_math ? "$$" : "$") + s.content + (s.display_math ? "$$" : "$"); break;
            default: out += s.content; break;
        }
    }
    return out;
}

RichText Text::to_rich_text() const {
    RichText rt(m_base);
    for (const TextSegment& s : m_segments) {
        switch (s.kind) {
            case TextSegmentKind::LineBreak: rt.newline(); break;
            case TextSegmentKind::Text: rt.add(s.content, s.style, s.family.empty() ? m_family : s.family); break;
            case TextSegmentKind::Math: rt.add(s.content, s.style, m_family); break;
            case TextSegmentKind::Image: if (!s.content.empty()) rt.add(s.content, s.style, m_family); break;
            default: break;
        }
    }
    return rt;
}

std::string Text::fingerprint() const {
    std::string k;
    auto put = [&](const void* p, std::size_t n) { k.append(static_cast<const char*>(p), n); };
    auto put_str = [&](const std::string& s) {
        const std::size_t n = s.size();
        put(&n, sizeof(n));
        k += s;
    };
    auto put_style = [&](const TextStyle& s) {
        const double sz = s.has_size() ? s.size() : -1.0;
        put(&sz, sizeof(sz));
        const auto fg = s.fg_color();
        const auto bg = s.bg_color();
        const std::uint32_t fgv = fg.is_set() ? (static_cast<std::uint32_t>(fg.value().red()) << 24 | static_cast<std::uint32_t>(fg.value().green()) << 16 | static_cast<std::uint32_t>(fg.value().blue()) << 8 | fg.value().alpha()) : 0x01;
        const std::uint32_t bgv = bg.is_set() ? (static_cast<std::uint32_t>(bg.value().red()) << 24 | static_cast<std::uint32_t>(bg.value().green()) << 16 | static_cast<std::uint32_t>(bg.value().blue()) << 8 | bg.value().alpha()) : 0x01;
        put(&fgv, sizeof(fgv));
        put(&bgv, sizeof(bgv));
        const std::uint32_t flags = (s.has_weight() ? static_cast<std::uint32_t>(s.weight()) : 0u) | (s.has_slant() ? (static_cast<std::uint32_t>(s.slant()) + 1) << 12 : 0u) |
                                    (s.is_underlined() ? 1u << 16 : 0u) | (s.is_strikethrough() ? 1u << 17 : 0u) | (s.is_overlined() ? 1u << 18 : 0u) | (s.has_font() ? 1u << 19 : 0u);
        put(&flags, sizeof(flags));
        const std::uint32_t font = s.has_font() ? static_cast<std::uint32_t>(s.font()) : 0u;
        put(&font, sizeof(font));
        const float op = s.has_opacity() ? s.opacity() : -1.0f;
        put(&op, sizeof(op));
    };

    put_style(m_base);
    put_str(m_family);
    const float nums[5] = { m_max_width, m_line_spacing, m_emoji_em, static_cast<float>(m_align), m_jumbo ? 1.0f : 0.0f };
    put(nums, sizeof(nums));
    for (const TextSegment& s : m_segments) {
        k.push_back(static_cast<char>(s.kind));
        put_str(s.content);
        put_style(s.style);
        put_str(s.family);
        const void* img = s.image.get();
        put(&img, sizeof(img));
        const float dims[2] = { s.image_width, s.image_height };
        put(dims, sizeof(dims));
        put(&s.link, sizeof(s.link));
        k.push_back(s.display_math ? '\1' : '\0');
    }
    return k;
}

bool Text::operator==(const Text& o) const noexcept {
    return m_base == o.m_base && m_family == o.m_family && m_segments == o.m_segments && m_max_width == o.m_max_width && m_align == o.m_align && m_line_spacing == o.m_line_spacing &&
           m_x == o.m_x && m_y == o.m_y && m_positioned == o.m_positioned && m_anchor == o.m_anchor && m_emoji_em == o.m_emoji_em && m_jumbo == o.m_jumbo && m_links.size() == o.m_links.size();
}

FancyText Text::layout() const { return layout(shared_text_rasterizer()); }

FancyText Text::layout(TextRasterizer& raster) const {
    FancyText out;
    if (m_segments.empty()) return out;

    bool jumbo = false;
    if (m_jumbo) {
        jumbo = true;
        std::size_t count = 0;
        for (const TextSegment& s : m_segments) {
            if (s.kind == TextSegmentKind::Image) ++count;
            else if (s.kind == TextSegmentKind::Text) {
                if (!is_emoji_only(s.content)) {
                    bool blank = true;
                    for (char c : s.content) if (!std::isspace(static_cast<unsigned char>(c))) blank = false;
                    if (!blank) { jumbo = false; break; }
                } else {
                    ++count;
                }
            } else if (s.kind != TextSegmentKind::LineBreak) {
                jumbo = false;
                break;
            }
        }
        if (count == 0 || count > 27) jumbo = false;
    }

    std::vector<Item> items;
    const std::string& base_family = m_family;

    for (const TextSegment& seg : m_segments) {
        switch (seg.kind) {
            case TextSegmentKind::LineBreak: {
                Item it;
                it.kind = Item::Break;
                items.push_back(std::move(it));
                break;
            }
            case TextSegmentKind::QuoteBar: {
                Item it;
                it.kind = Item::Bar;
                const TextStyle st = resolved_style(m_base, seg.style);
                it.bar_color = st.fg_color().is_set() ? st.fg_color().value() : graphics::Color(79, 84, 92);
                it.width = static_cast<float>(std::max(4.0, st.size() * 0.25) + st.size() * 0.5);
                it.style = st;
                items.push_back(std::move(it));
                break;
            }
            case TextSegmentKind::Text: {
                TextStyle st = resolved_style(m_base, seg.style);
                if (jumbo && is_emoji_only(seg.content)) st.set_size(st.size() * 3.0);
                const std::string fam = seg.family.empty() ? base_family : seg.family;
                if (m_max_width > 0.0f) {
                    std::size_t i = 0;
                    const std::string& t = seg.content;
                    while (i < t.size()) {
                        std::size_t j = i;
                        while (j < t.size() && t[j] != ' ') ++j;
                        while (j < t.size() && t[j] == ' ') ++j;
                        Item it;
                        it.kind = Item::Run;
                        it.text = t.substr(i, j - i);
                        it.style = st;
                        it.family = fam;
                        it.link = seg.link;
                        it.space_only = it.text.find_first_not_of(' ') == std::string::npos;
                        items.push_back(std::move(it));
                        i = j;
                    }
                } else {
                    Item it;
                    it.kind = Item::Run;
                    it.text = seg.content;
                    it.style = st;
                    it.family = fam;
                    it.link = seg.link;
                    items.push_back(std::move(it));
                }
                break;
            }
            case TextSegmentKind::Math: {
                const TextStyle st = resolved_style(m_base, seg.style);
                MathRenderOptions o;
                o.layout.font_size = static_cast<float>(st.size() * (seg.display_math ? 1.15 : 1.0));
                if (st.fg_color().is_set()) o.theme.color = st.fg_color().value();
                o.layout.display = seg.display_math;
                Item it;
                it.kind = Item::Math;
                const std::string source = seg.content.find('\\') != std::string::npos ? latex_to_mathtext(seg.content) : seg.content;
                it.math = make_fancy_math(source, raster, o);
                it.width = it.math.width();
                it.ascent = it.math.ascent();
                it.descent = it.math.descent();
                it.link = seg.link;
                it.style = st;
                if (seg.display_math) {
                    Item br;
                    br.kind = Item::Break;
                    if (!items.empty() && items.back().kind != Item::Break) items.push_back(br);
                    items.push_back(std::move(it));
                    items.push_back(br);
                } else {
                    items.push_back(std::move(it));
                }
                break;
            }
            case TextSegmentKind::Image: {
                const TextStyle st = resolved_style(m_base, seg.style);
                Item it;
                it.kind = Item::Image;
                it.image = seg.image;
                it.style = st;
                it.link = seg.link;
                const double em = st.size() * (jumbo ? 3.0 : 1.0);
                const double iw = seg.image->width() ? seg.image->width() : seg.image->frame(0).image.width();
                const double ih = seg.image->height() ? seg.image->height() : seg.image->frame(0).image.height();
                double w = seg.image_width, h = seg.image_height;
                if (w <= 0 && h <= 0) {
                    h = em * m_emoji_em;
                    w = ih > 0 ? h * iw / ih : h;
                } else if (w <= 0) {
                    w = ih > 0 ? h * iw / ih : h;
                } else if (h <= 0) {
                    h = iw > 0 ? w * ih / iw : w;
                }
                const double drop = std::min(h * 0.5, em * 0.2);
                it.width = static_cast<float>(w);
                it.ascent = static_cast<float>(h - drop);
                it.descent = static_cast<float>(drop);
                items.push_back(std::move(it));
                break;
            }
        }
    }

    for (Item& it : items) if (it.kind == Item::Run) measure_run(raster, it);

    struct Line {
        std::size_t begin, end;
        float       width = 0, ascent = 0, descent = 0;
    };
    std::vector<Line> lines;
    Line cur{ 0, 0 };
    float x = 0;

    auto close_line = [&](std::size_t end) {
        cur.end = end;
        float trailing = 0;
        for (std::size_t k = cur.end; k > cur.begin; --k) {
            const Item& it = items[k - 1];
            if (it.kind == Item::Break) continue;
            if (it.kind == Item::Run && it.space_only) {
                trailing += it.width;
                continue;
            }
            if (it.kind == Item::Run && m_max_width > 0.0f && !it.text.empty() && it.text.back() == ' ') {
                Item probe = it;
                probe.text = it.text.substr(0, it.text.find_last_not_of(' ') + 1);
                measure_run(raster, probe);
                trailing += it.width - probe.width;
            }
            break;
        }
        cur.width = x - trailing;
        lines.push_back(cur);
        cur = Line{ end, end };
        x = 0;
    };

    for (std::size_t i = 0; i < items.size(); ++i) {
        Item& it = items[i];
        if (it.kind == Item::Break) {
            close_line(i);
            cur.begin = cur.end = i + 1;
            continue;
        }

        float w = it.width;
        if (m_max_width > 0.0f && it.kind == Item::Run && !it.space_only) {
            const std::size_t t = it.text.find_last_not_of(' ');
            if (t != std::string::npos && t + 1 < it.text.size()) {
                Item probe = it;
                probe.text = it.text.substr(0, t + 1);
                measure_run(raster, probe);
                w = probe.width;
            }
        }

        if (m_max_width > 0.0f && x > 0 && x + w > m_max_width && !(it.kind == Item::Run && it.space_only)) close_line(i);

        if (m_max_width > 0.0f && x == 0 && it.kind == Item::Run && w > m_max_width && it.text.size() > 1) {
            std::size_t cut = 0, k = 0;
            while (k < it.text.size()) {
                const std::size_t nk = utf8_next(it.text, k);
                Item probe = it;
                probe.text = it.text.substr(0, nk);
                measure_run(raster, probe);
                if (probe.width > m_max_width && cut > 0) break;
                cut = nk;
                k = nk;
            }
            if (cut > 0 && cut < it.text.size()) {
                Item rest = it;
                rest.text = it.text.substr(cut);
                it.text = it.text.substr(0, cut);
                measure_run(raster, it);
                measure_run(raster, rest);
                items.insert(items.begin() + static_cast<std::ptrdiff_t>(i) + 1, rest);
                Item br;
                br.kind = Item::Break;
                items.insert(items.begin() + static_cast<std::ptrdiff_t>(i) + 1, br);
            }
        }

        Item& cur_item = items[i];
        if (x == 0 && cur_item.kind == Item::Run && cur_item.space_only && m_max_width > 0.0f && !lines.empty()) continue;
        x += cur_item.width;
    }
    close_line(items.size());

    float baseline = 0;
    float total_width = 0;
    bool first = true;
    float first_ascent = 0;

    for (Line& ln : lines) {
        float asc = 0, desc = 0;
        for (std::size_t k = ln.begin; k < ln.end; ++k) {
            const Item& it = items[k];
            if (it.kind == Item::Break || it.kind == Item::Bar) continue;
            asc = std::max(asc, it.ascent);
            desc = std::max(desc, it.descent);
        }
        if (asc == 0 && desc == 0) {
            const double sz = m_base.has_size() ? m_base.size() : kDefaultSize;
            Item probe;
            probe.kind = Item::Run;
            probe.text = "Ag";
            probe.style = resolved_style(m_base, TextStyle());
            probe.family = base_family;
            measure_run(raster, probe);
            asc = probe.ascent > 0 ? probe.ascent : static_cast<float>(sz * 0.8);
            desc = probe.descent > 0 ? probe.descent : static_cast<float>(sz * 0.2);
        }
        ln.ascent = asc;
        ln.descent = desc;
        total_width = std::max(total_width, ln.width);
    }

    const float box_width = m_max_width > 0.0f && m_align != TextAlign::Left ? m_max_width : total_width;

    for (std::size_t li = 0; li < lines.size(); ++li) {
        Line& ln = lines[li];
        if (first) {
            baseline = ln.ascent;
            first_ascent = ln.ascent;
            first = false;
        } else {
            const Line& prev = lines[li - 1];
            baseline += (prev.descent + ln.ascent) * m_line_spacing;
        }

        float lx = 0;
        if (m_align == TextAlign::Center) lx = (box_width - ln.width) / 2;
        else if (m_align == TextAlign::Right) lx = box_width - ln.width;

        float justify_extra = 0;
        if (m_align == TextAlign::Justify && m_max_width > 0.0f && li + 1 < lines.size()) {
            std::size_t gaps = 0;
            for (std::size_t k = ln.begin; k + 1 < ln.end; ++k) if (items[k].kind == Item::Run && items[k].text.back() == ' ') ++gaps;
            if (gaps) justify_extra = (m_max_width - ln.width) / gaps;
        }

        const float y = baseline - first_ascent;
        std::size_t k = ln.begin;
        bool line_start = true;

        while (k < ln.end) {
            Item& it = items[k];
            if (it.kind == Item::Break) { ++k; continue; }
            if (line_start && it.kind == Item::Run && it.space_only && m_max_width > 0.0f && li > 0) { ++k; continue; }
            line_start = false;

            if (it.kind == Item::Bar) {
                const float bw = static_cast<float>(std::max(3.0, it.style.size() * 0.2));
                out.add_rect(lx, y - ln.ascent, bw, ln.ascent + ln.descent, it.bar_color);
                lx += it.width;
                ++k;
                continue;
            }

            if (it.kind == Item::Math) {
                out.append(it.math, lx, y);
                if (it.link >= 0) out.add_rect(lx, y - it.ascent, it.width, it.ascent + it.descent, graphics::Color(0, 0, 0, 0), static_cast<std::uint32_t>(it.link));
                lx += it.width;
                ++k;
                continue;
            }

            if (it.kind == Item::Image) {
                const float h = it.ascent + it.descent;
                const float op = it.style.has_opacity() ? it.style.opacity() : 1.0f;
                out.add_image(lx, y - it.ascent, it.width, h, it.image, op, it.link >= 0 ? static_cast<std::uint32_t>(it.link) : kNoFancyTag);
                lx += it.width;
                ++k;
                continue;
            }

            std::string merged = it.text;
            std::size_t m = k + 1;
            std::size_t gaps = it.text.back() == ' ' ? 1 : 0;
            if (justify_extra == 0) {
                while (m < ln.end && items[m].kind == Item::Run && items[m].style == it.style && items[m].family == it.family && items[m].link == it.link) {
                    merged += items[m].text;
                    if (items[m].text.back() == ' ') ++gaps;
                    ++m;
                }
            }
            (void)gaps;
            const bool trailing_line_end = m >= ln.end;
            if (trailing_line_end && m_max_width > 0.0f) {
                const std::size_t t = merged.find_last_not_of(' ');
                if (t == std::string::npos) { k = m; continue; }
                merged.resize(t + 1);
            }
            Item run = it;
            run.text = merged;
            if (m > k + 1 || trailing_line_end) measure_run(raster, run);
            const std::uint32_t tag = it.link >= 0 ? static_cast<std::uint32_t>(it.link) : kNoFancyTag;
            out.add_run(lx, y, make_run(run.text, run.style, run.family), tag);
            if (it.link >= 0) out.add_rect(lx, y - run.ascent, run.width, run.ascent + run.descent, graphics::Color(0, 0, 0, 0), tag);
            lx += run.width + (it.text.back() == ' ' ? justify_extra : 0.0f);
            k = m;
        }
    }

    float height = 0;
    if (!lines.empty()) height = baseline + lines.back().descent;
    out.set_box(box_width, first_ascent, std::max(0.0f, height - first_ascent));
    return out;
}

TextBox Text::measure() const { return measure(shared_text_rasterizer()); }

TextBox Text::measure(TextRasterizer& raster) const {
    const FancyText ft = layout(raster);
    TextBox b;
    b.width = ft.width();
    b.ascent = ft.ascent();
    b.descent = ft.descent();
    b.height = ft.height();
    std::size_t lines = 1;
    for (const TextSegment& s : m_segments) if (s.kind == TextSegmentKind::LineBreak) ++lines;
    b.lines = m_segments.empty() ? 0 : lines;
    return b;
}

images::BitmapImage Text::render(float padding, const graphics::Color& background) const { return render(shared_text_rasterizer(), padding, background); }

images::BitmapImage Text::render(TextRasterizer& raster, float padding, const graphics::Color& background) const {
    const FancyText ft = layout(raster);
    return fancy_text_image(ft, raster, padding, background);
}

std::uint32_t Text::link_at(const FancyText& layout, float x, float y) const noexcept {
    const std::uint32_t tag = layout.hit_test(x, y);
    if (tag == kNoFancyTag || tag >= m_links.size()) return kNoFancyTag;
    return tag;
}

void draw_animated_frame(images::BitmapImage& img, const FancyImage& item, float x, float y, double time_ms) {
    if (!item.image || item.image->empty()) return;
    const images::BitmapImage& frame = item.image->frame_at(time_ms < 0 ? 0 : static_cast<std::uint64_t>(time_ms));
    const unsigned fw = frame.width(), fh = frame.height();
    if (!fw || !fh) return;
    const float dx = x + item.x, dy = y + item.y;
    const int x0 = std::max(0, static_cast<int>(std::floor(dx))), y0 = std::max(0, static_cast<int>(std::floor(dy)));
    const int x1 = std::min(static_cast<int>(img.width()), static_cast<int>(std::ceil(dx + item.w)));
    const int y1 = std::min(static_cast<int>(img.height()), static_cast<int>(std::ceil(dy + item.h)));
    const double sx = fw / static_cast<double>(item.w), sy = fh / static_cast<double>(item.h);
    const auto& src = frame.pixels();

    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            const double cx0 = std::max<double>(px, dx), cx1 = std::min<double>(px + 1.0, dx + item.w);
            const double cy0 = std::max<double>(py, dy), cy1 = std::min<double>(py + 1.0, dy + item.h);
            if (cx1 <= cx0 || cy1 <= cy0) continue;
            const double u0 = (cx0 - dx) * sx, u1 = (cx1 - dx) * sx, v0 = (cy0 - dy) * sy, v1 = (cy1 - dy) * sy;
            double acc[4] = { 0, 0, 0, 0 }, area = 0;
            for (int sy0 = static_cast<int>(std::floor(v0)); sy0 < static_cast<int>(std::ceil(v1)) && sy0 < static_cast<int>(fh); ++sy0) {
                if (sy0 < 0) continue;
                const double wy = std::min(v1, sy0 + 1.0) - std::max(v0, static_cast<double>(sy0));
                if (wy <= 0) continue;
                for (int sx0 = static_cast<int>(std::floor(u0)); sx0 < static_cast<int>(std::ceil(u1)) && sx0 < static_cast<int>(fw); ++sx0) {
                    if (sx0 < 0) continue;
                    const double wx = std::min(u1, sx0 + 1.0) - std::max(u0, static_cast<double>(sx0));
                    if (wx <= 0) continue;
                    const graphics::Color& c = src[static_cast<std::size_t>(sy0) * fw + static_cast<std::size_t>(sx0)];
                    const double w = wx * wy, a = c.alpha() / 255.0;
                    acc[0] += w * c.red() * a;
                    acc[1] += w * c.green() * a;
                    acc[2] += w * c.blue() * a;
                    acc[3] += w * a;
                    area += w;
                }
            }
            if (area <= 0 || acc[3] <= 0) continue;
            const double alpha = acc[3] / area;
            const graphics::Color col(static_cast<std::uint8_t>(std::lround(std::min(255.0, acc[0] / acc[3]))), static_cast<std::uint8_t>(std::lround(std::min(255.0, acc[1] / acc[3]))), static_cast<std::uint8_t>(std::lround(std::min(255.0, acc[2] / acc[3]))), 255);
            const double coverage = (cx1 - cx0) * (cy1 - cy0);
            fancy_detail::blend(img, px, py, col, static_cast<float>(alpha * coverage * item.opacity));
        }
}

} // namespace text
} // namespace fizmo
