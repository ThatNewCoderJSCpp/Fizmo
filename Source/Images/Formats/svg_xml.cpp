#include "fizmo_library.hpp"
#include "svg_internal.hpp"

#include <algorithm>
#include <cctype>

namespace fizmo {
namespace images {
namespace svg_detail {

namespace {

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

bool is_name_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.' || (static_cast<unsigned char>(c) >= 0x80); }

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x110000) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string decode_entities(std::string_view s, const std::unordered_map<std::string, std::string>& custom) {
    std::string out;
    out.reserve(s.size());

    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out.push_back(s[i]);
            continue;
        }

        const std::size_t semi = s.find(';', i);
        if (semi == std::string_view::npos || semi - i > 32) {
            out.push_back('&');
            continue;
        }

        const std::string_view ent = s.substr(i + 1, semi - i - 1);
        if (ent == "lt") out.push_back('<');
        else if (ent == "gt") out.push_back('>');
        else if (ent == "amp") out.push_back('&');
        else if (ent == "quot") out.push_back('"');
        else if (ent == "apos") out.push_back('\'');
        else if (ent == "nbsp") append_utf8(out, 0xA0);
        else if (!ent.empty() && ent[0] == '#') {
            std::uint32_t cp = 0;
            bool ok = ent.size() > 1;
            if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) {
                ok = ent.size() > 2;
                for (std::size_t k = 2; k < ent.size() && ok; ++k) {
                    const char c = ent[k];
                    if (!std::isxdigit(static_cast<unsigned char>(c))) ok = false;
                    else cp = cp * 16 + static_cast<std::uint32_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : (std::tolower(c) - 'a' + 10));
                }
            } else {
                for (std::size_t k = 1; k < ent.size() && ok; ++k) {
                    if (!std::isdigit(static_cast<unsigned char>(ent[k]))) ok = false;
                    else cp = cp * 10 + static_cast<std::uint32_t>(ent[k] - '0');
                }
            }
            if (!ok) {
                out.push_back('&');
                continue;
            }
            append_utf8(out, cp);
        } else {
            auto it = custom.find(std::string(ent));
            if (it == custom.end()) {
                out.push_back('&');
                continue;
            }
            out += it->second;
        }

        i = semi;
    }

    return out;
}

std::string strip_prefix(const std::string& name) {
    const std::size_t colon = name.find(':');
    if (colon == std::string::npos) return name;
    const std::string prefix = name.substr(0, colon);
    if (prefix == "svg") return name.substr(colon + 1);
    return name;
}

} // namespace

bool parse_xml(std::string_view src, XmlDocument& doc, std::string* error) {
    doc.nodes.clear();
    doc.root = -1;
    std::unordered_map<std::string, std::string> entities;
    std::size_t i = 0;
    int current = -1;
    const std::size_t n = src.size();

    if (n >= 3 && static_cast<unsigned char>(src[0]) == 0xEF && static_cast<unsigned char>(src[1]) == 0xBB && static_cast<unsigned char>(src[2]) == 0xBF) i = 3;

    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };

    while (i < n) {
        if (src[i] != '<') {
            const std::size_t lt = src.find('<', i);
            const std::size_t end = lt == std::string_view::npos ? n : lt;
            if (current >= 0) {
                XmlNode t;
                t.is_text = true;
                t.text = decode_entities(src.substr(i, end - i), entities);
                t.parent = current;
                doc.nodes.push_back(std::move(t));
                doc.nodes[static_cast<std::size_t>(current)].children.push_back(static_cast<int>(doc.nodes.size() - 1));
            }
            i = end;
            continue;
        }

        if (src.compare(i, 4, "<!--") == 0) {
            const std::size_t e = src.find("-->", i + 4);
            if (e == std::string_view::npos) return fail("unterminated XML comment");
            i = e + 3;
            continue;
        }

        if (src.compare(i, 9, "<![CDATA[") == 0) {
            const std::size_t e = src.find("]]>", i + 9);
            if (e == std::string_view::npos) return fail("unterminated CDATA");
            if (current >= 0) {
                XmlNode t;
                t.is_text = true;
                t.text = std::string(src.substr(i + 9, e - i - 9));
                t.parent = current;
                doc.nodes.push_back(std::move(t));
                doc.nodes[static_cast<std::size_t>(current)].children.push_back(static_cast<int>(doc.nodes.size() - 1));
            }
            i = e + 3;
            continue;
        }

        if (src.compare(i, 2, "<?") == 0) {
            const std::size_t e = src.find("?>", i + 2);
            if (e == std::string_view::npos) return fail("unterminated processing instruction");
            i = e + 2;
            continue;
        }

        if (src.compare(i, 2, "<!") == 0) {
            std::size_t j = i + 2;
            int depth = 0;
            while (j < n) {
                if (src[j] == '[') ++depth;
                else if (src[j] == ']') --depth;
                else if (src[j] == '>' && depth <= 0) break;
                else if (src.compare(j, 8, "<!ENTITY") == 0) {
                    std::size_t k = j + 8;
                    while (k < n && is_space(src[k])) ++k;
                    std::size_t ns = k;
                    while (k < n && is_name_char(src[k])) ++k;
                    const std::string name(src.substr(ns, k - ns));
                    while (k < n && is_space(src[k])) ++k;
                    if (k < n && (src[k] == '"' || src[k] == '\'')) {
                        const char q = src[k];
                        const std::size_t ve = src.find(q, k + 1);
                        if (ve != std::string_view::npos) {
                            entities[name] = std::string(src.substr(k + 1, ve - k - 1));
                            k = ve + 1;
                        }
                    }
                    const std::size_t gt = src.find('>', k);
                    j = gt == std::string_view::npos ? n : gt;
                }
                ++j;
            }
            i = j + 1;
            continue;
        }

        if (src.compare(i, 2, "</") == 0) {
            const std::size_t e = src.find('>', i + 2);
            if (e == std::string_view::npos) return fail("unterminated end tag");
            if (current >= 0) current = doc.nodes[static_cast<std::size_t>(current)].parent;
            i = e + 1;
            continue;
        }

        std::size_t j = i + 1;
        while (j < n && is_name_char(src[j])) ++j;
        XmlNode node;
        node.tag = strip_prefix(std::string(src.substr(i + 1, j - i - 1)));
        bool self_close = false;

        while (j < n) {
            while (j < n && is_space(src[j])) ++j;
            if (j >= n) return fail("unterminated start tag");
            if (src[j] == '>') { ++j; break; }
            if (src[j] == '/' && j + 1 < n && src[j + 1] == '>') { self_close = true; j += 2; break; }
            const std::size_t ns = j;
            while (j < n && is_name_char(src[j])) ++j;
            if (j == ns) { ++j; continue; }
            std::string name(src.substr(ns, j - ns));
            while (j < n && is_space(src[j])) ++j;
            std::string value;
            if (j < n && src[j] == '=') {
                ++j;
                while (j < n && is_space(src[j])) ++j;
                if (j < n && (src[j] == '"' || src[j] == '\'')) {
                    const char q = src[j];
                    const std::size_t ve = src.find(q, j + 1);
                    if (ve == std::string_view::npos) return fail("unterminated attribute value");
                    value = decode_entities(src.substr(j + 1, ve - j - 1), entities);
                    j = ve + 1;
                } else {
                    const std::size_t vs = j;
                    while (j < n && !is_space(src[j]) && src[j] != '>') ++j;
                    value = decode_entities(src.substr(vs, j - vs), entities);
                }
            }
            if (name.rfind("svg:", 0) == 0) name = name.substr(4);
            node.attrs.emplace_back(std::move(name), std::move(value));
        }

        node.parent = current;
        doc.nodes.push_back(std::move(node));
        const int idx = static_cast<int>(doc.nodes.size() - 1);
        if (current >= 0) doc.nodes[static_cast<std::size_t>(current)].children.push_back(idx);
        else if (doc.root < 0) doc.root = idx;
        if (!self_close) current = idx;
        i = j;
    }

    if (doc.root < 0) return fail("no XML root element");
    return true;
}

namespace {

std::string trim(std::string_view s) {
    std::size_t a = 0, b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return std::string(s.substr(a, b - a));
}

bool parse_selector(std::string_view text, CssSelector& sel) {
    sel = CssSelector();
    std::size_t i = 0;
    const std::size_t n = text.size();
    char pending = 0;

    while (i < n) {
        while (i < n && is_space(text[i])) {
            ++i;
            if (!sel.parts.empty() && pending == 0) pending = ' ';
        }
        if (i >= n) break;
        if (text[i] == '>' || text[i] == '+' || text[i] == '~') {
            pending = text[i];
            ++i;
            continue;
        }

        CssSimpleSelector s;
        bool any = false;

        while (i < n && !is_space(text[i]) && text[i] != '>' && text[i] != '+' && text[i] != '~') {
            if (text[i] == '*') {
                s.universal = true;
                ++i;
                any = true;
            } else if (text[i] == '#' || text[i] == '.') {
                const char kind = text[i++];
                const std::size_t st = i;
                while (i < n && is_name_char(text[i]) && text[i] != '.' && text[i] != ':') ++i;
                const std::string name(text.substr(st, i - st));
                if (kind == '#') s.id = name;
                else s.classes.push_back(name);
                any = true;
            } else if (text[i] == '[') {
                const std::size_t e = text.find(']', i);
                if (e == std::string_view::npos) return false;
                const std::string body = trim(text.substr(i + 1, e - i - 1));
                const std::size_t eq = body.find('=');
                if (eq == std::string::npos) s.attrs.emplace_back(trim(body), std::string());
                else {
                    std::string v = trim(std::string_view(body).substr(eq + 1));
                    if (v.size() >= 2 && (v[0] == '"' || v[0] == '\'')) v = v.substr(1, v.size() - 2);
                    s.attrs.emplace_back(trim(std::string_view(body).substr(0, eq)), v);
                }
                i = e + 1;
                any = true;
            } else if (text[i] == ':') {
                ++i;
                const std::size_t st = i;
                while (i < n && is_name_char(text[i])) ++i;
                const std::string pseudo(text.substr(st, i - st));
                if (pseudo != "first-child" && pseudo != "root") return false;
                s.attrs.emplace_back(":" + pseudo, std::string());
                any = true;
            } else if (is_name_char(text[i])) {
                const std::size_t st = i;
                while (i < n && is_name_char(text[i]) && text[i] != '.' && text[i] != ':') ++i;
                s.tag = std::string(text.substr(st, i - st));
                any = true;
            } else {
                return false;
            }
        }

        if (!any) return false;
        if (!sel.parts.empty()) sel.combinators.push_back(pending ? pending : ' ');
        pending = 0;
        sel.parts.push_back(std::move(s));
    }

    if (sel.parts.empty()) return false;
    for (const auto& p : sel.parts) {
        sel.specificity += p.id.empty() ? 0 : 10000;
        sel.specificity += static_cast<int>(p.classes.size() + p.attrs.size()) * 100;
        sel.specificity += p.tag.empty() ? 0 : 1;
    }
    return true;
}

bool simple_matches(const XmlDocument& doc, int node, const CssSimpleSelector& s) {
    const XmlNode& n = doc.nodes[static_cast<std::size_t>(node)];
    if (!s.tag.empty() && s.tag != n.tag) return false;

    if (!s.id.empty()) {
        const std::string* id = n.attr("id");
        if (!id || *id != s.id) return false;
    }

    if (!s.classes.empty()) {
        const std::string* cls = n.attr("class");
        if (!cls) return false;
        for (const auto& c : s.classes) {
            bool found = false;
            std::size_t i = 0;
            while (i < cls->size() && !found) {
                while (i < cls->size() && is_space((*cls)[i])) ++i;
                const std::size_t st = i;
                while (i < cls->size() && !is_space((*cls)[i])) ++i;
                found = cls->compare(st, i - st, c) == 0 && i - st == c.size();
            }
            if (!found) return false;
        }
    }

    for (const auto& a : s.attrs) {
        if (a.first == ":first-child") {
            if (n.parent < 0) return false;
            const XmlNode& p = doc.nodes[static_cast<std::size_t>(n.parent)];
            int first = -1;
            for (int c : p.children) if (!doc.nodes[static_cast<std::size_t>(c)].is_text) { first = c; break; }
            if (first != node) return false;
            continue;
        }
        if (a.first == ":root") {
            if (n.parent >= 0) return false;
            continue;
        }
        const std::string* v = n.attr(a.first);
        if (!v) return false;
        if (!a.second.empty() && *v != a.second) return false;
    }

    return true;
}

bool match_from(const XmlDocument& doc, int node, const CssSelector& sel, int part) {
    if (!simple_matches(doc, node, sel.parts[static_cast<std::size_t>(part)])) return false;
    if (part == 0) return true;
    const char comb = sel.combinators[static_cast<std::size_t>(part - 1)];
    const XmlNode& n = doc.nodes[static_cast<std::size_t>(node)];

    if (comb == '>') return n.parent >= 0 && match_from(doc, n.parent, sel, part - 1);

    if (comb == ' ') {
        for (int p = n.parent; p >= 0; p = doc.nodes[static_cast<std::size_t>(p)].parent)
            if (match_from(doc, p, sel, part - 1)) return true;
        return false;
    }

    if (n.parent < 0) return false;
    const auto& sib = doc.nodes[static_cast<std::size_t>(n.parent)].children;
    int prev = -1;
    for (int c : sib) {
        if (c == node) break;
        if (doc.nodes[static_cast<std::size_t>(c)].is_text) continue;
        if (comb == '~' && match_from(doc, c, sel, part - 1)) return true;
        prev = c;
    }
    return comb == '+' && prev >= 0 && match_from(doc, prev, sel, part - 1);
}

} // namespace

std::vector<CssDeclaration> parse_declarations(std::string_view text) {
    std::vector<CssDeclaration> out;
    std::size_t i = 0;

    while (i < text.size()) {
        std::size_t semi = i;
        int paren = 0;
        char quote = 0;
        while (semi < text.size()) {
            const char c = text[semi];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '"' || c == '\'') quote = c;
            else if (c == '(') ++paren;
            else if (c == ')') --paren;
            else if (c == ';' && paren <= 0) break;
            ++semi;
        }
        const std::string_view decl = text.substr(i, semi - i);
        const std::size_t colon = decl.find(':');
        if (colon != std::string_view::npos) {
            CssDeclaration d;
            d.property = trim(decl.substr(0, colon));
            std::transform(d.property.begin(), d.property.end(), d.property.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            d.value = trim(decl.substr(colon + 1));
            const std::size_t imp = d.value.find("!important");
            if (imp != std::string::npos) {
                d.important = true;
                d.value = trim(std::string_view(d.value).substr(0, imp));
            }
            if (!d.property.empty()) out.push_back(std::move(d));
        }
        i = semi + 1;
    }

    return out;
}

void parse_css(std::string_view css_in, std::vector<CssRule>& rules) {
    std::string css;
    css.reserve(css_in.size());
    for (std::size_t i = 0; i < css_in.size(); ++i) {
        if (css_in.compare(i, 2, "/*") == 0) {
            const std::size_t e = css_in.find("*/", i + 2);
            if (e == std::string_view::npos) break;
            i = e + 1;
            continue;
        }
        css.push_back(css_in[i]);
    }

    std::size_t i = 0;
    while (i < css.size()) {
        while (i < css.size() && is_space(css[i])) ++i;
        if (i >= css.size()) break;

        if (css[i] == '@') {
            std::size_t j = i;
            while (j < css.size() && css[j] != '{' && css[j] != ';') ++j;
            if (j < css.size() && css[j] == ';') { i = j + 1; continue; }
            int depth = 0;
            while (j < css.size()) {
                if (css[j] == '{') ++depth;
                else if (css[j] == '}' && --depth == 0) break;
                ++j;
            }
            i = j + 1;
            continue;
        }

        const std::size_t open = css.find('{', i);
        if (open == std::string::npos) break;
        const std::size_t close = css.find('}', open);
        if (close == std::string::npos) break;
        const std::string sel_text = css.substr(i, open - i);
        const std::vector<CssDeclaration> decls = parse_declarations(std::string_view(css).substr(open + 1, close - open - 1));
        std::size_t s = 0;

        while (s <= sel_text.size()) {
            std::size_t comma = sel_text.find(',', s);
            if (comma == std::string::npos) comma = sel_text.size();
            CssRule rule;
            if (parse_selector(trim(std::string_view(sel_text).substr(s, comma - s)), rule.selector)) {
                rule.declarations = decls;
                rule.order = rules.size();
                rules.push_back(std::move(rule));
            }
            s = comma + 1;
        }

        i = close + 1;
    }
}

bool selector_matches(const XmlDocument& doc, int node, const CssSelector& sel) { return match_from(doc, node, sel, static_cast<int>(sel.parts.size()) - 1); }

} // namespace svg_detail
} // namespace images
} // namespace fizmo
