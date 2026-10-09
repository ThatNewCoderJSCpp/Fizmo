#ifndef FIZMO_UI_TEXT_HPP
#define FIZMO_UI_TEXT_HPP

#include "ui_core.hpp"
#include <cctype>
#include <cwctype>
#include <limits>

namespace fizmo {
namespace ui {

namespace utf8 {

inline std::size_t next(const std::string& s, std::size_t i) noexcept {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
    return i;
}

inline std::size_t prev(const std::string& s, std::size_t i) noexcept {
    if (i == 0) return 0;
    if (i > s.size()) i = s.size();
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return i;
}

inline char32_t decode(const std::string& s, std::size_t i) noexcept {
    if (i >= s.size()) return 0;
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) return c;
    int n = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 0;
    char32_t cp = n == 1 ? (c & 0x1F) : n == 2 ? (c & 0x0F) : n == 3 ? (c & 0x07) : 0xFFFD;
    for (int k = 1; k <= n; ++k) {
        if (i + k >= s.size()) return 0xFFFD;
        const unsigned char cc = static_cast<unsigned char>(s[i + k]);
        if ((cc & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (cc & 0x3F);
    }
    return cp;
}

inline void append(std::string& out, char32_t cp) {
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
}

inline std::size_t length(const std::string& s) noexcept {
    std::size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

inline std::u32string to_u32(const std::string& s) {
    std::u32string out;
    for (std::size_t i = 0; i < s.size(); i = next(s, i)) out.push_back(decode(s, i));
    return out;
}

inline std::string from_u32(const std::u32string& s) {
    std::string out;
    for (char32_t c : s) append(out, c);
    return out;
}

} // namespace utf8

class CharFilter {
    std::function<bool(char32_t)> m_pred;

public:
    CharFilter() = default;
    CharFilter(std::function<bool(char32_t)> pred) : m_pred(std::move(pred)) {}

    bool operator()(char32_t c) const { return !m_pred || m_pred(c); }
    explicit operator bool() const noexcept { return static_cast<bool>(m_pred); }

    CharFilter operator&&(const CharFilter& o) const { CharFilter a = *this, b = o; return CharFilter([a, b](char32_t c) { return a(c) && b(c); }); }
    CharFilter operator||(const CharFilter& o) const { CharFilter a = *this, b = o; return CharFilter([a, b](char32_t c) { return a(c) || b(c); }); }
    CharFilter operator!() const { CharFilter a = *this; return CharFilter([a](char32_t c) { return !a(c); }); }

    static CharFilter any() { return CharFilter(); }
    static CharFilter digits() { return CharFilter([](char32_t c) { return c >= '0' && c <= '9'; }); }
    static CharFilter integer() { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || c == '-' || c == '+'; }); }
    static CharFilter decimal() { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == ',' || c == 'e' || c == 'E'; }); }
    static CharFilter alpha() { return CharFilter([](char32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c != 0xD7 && c != 0xF7 && c < 0x2000); }); }
    static CharFilter alnum() { return alpha() || digits(); }
    static CharFilter hex() { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }); }
    static CharFilter identifier() { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }); }
    static CharFilter ascii() { return CharFilter([](char32_t c) { return c < 0x80; }); }
    static CharFilter printable_ascii() { return CharFilter([](char32_t c) { return c >= 0x20 && c < 0x7F; }); }
    static CharFilter no_whitespace() { return CharFilter([](char32_t c) { return !(c == ' ' || c == '\t' || c == 0xA0 || c == 0x3000 || (c >= 0x2000 && c <= 0x200B)); }); }

    static CharFilter only(const std::string& chars) {
        const std::u32string set = utf8::to_u32(chars);
        return CharFilter([set](char32_t c) { return set.find(c) != std::u32string::npos; });
    }

    static CharFilter except(const std::string& chars) {
        const std::u32string set = utf8::to_u32(chars);
        return CharFilter([set](char32_t c) { return set.find(c) == std::u32string::npos; });
    }
};

enum class TextCase : std::uint8_t { Any = 0, Upper, Lower };

inline std::optional<double> evaluate_expression(const std::string& text) {
    struct P {
        const std::string& s;
        std::size_t i = 0;
        bool ok = true;
        int depth = 0;
        void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i; }
        bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
        double primary() {
            ws();
            if (++depth > 64) { ok = false; return 0.0; }
            double r = 0.0;
            if (eat('(')) { r = expr(); if (!eat(')')) ok = false; }
            else if (i + 1 < s.size() && (s[i] == 'p' || s[i] == 'P') && (s[i + 1] == 'i' || s[i + 1] == 'I')) { i += 2; r = 3.14159265358979323846; }
            else {
                const std::size_t st = i;
                while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.')) ++i;
                if (i < s.size() && (s[i] == 'e' || s[i] == 'E') && i > st) {
                    std::size_t j = i + 1;
                    if (j < s.size() && (s[j] == '+' || s[j] == '-')) ++j;
                    if (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) { i = j; while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i; }
                }
                if (i == st) { ok = false; --depth; return 0.0; }
                const std::string num = s.substr(st, i - st);
                char* end = nullptr;
                r = std::strtod(num.c_str(), &end);
                if (!end || *end != '\0') ok = false;
            }
            --depth;
            return r;
        }
        double unary() {
            if (eat('-')) return -unary();
            if (eat('+')) return unary();
            return power();
        }
        double power() {
            const double b = primary();
            if (eat('^')) return std::pow(b, unary());
            return b;
        }
        double term() {
            double v = unary();
            for (;;) {
                if (eat('*')) v *= unary();
                else if (eat('/')) { const double d = unary(); if (d == 0.0) ok = false; else v /= d; }
                else if (eat('%')) { const double d = unary(); if (d == 0.0) ok = false; else v = std::fmod(v, d); }
                else return v;
            }
        }
        double expr() {
            double v = term();
            for (;;) {
                if (eat('+')) v += term();
                else if (eat('-')) v -= term();
                else return v;
            }
        }
    };
    std::string t = text;
    for (char& c : t) if (c == ',') c = '.';
    P p{ t };
    p.ws();
    if (p.i >= t.size()) return std::nullopt;
    const double v = p.expr();
    p.ws();
    if (!p.ok || p.i != t.size() || !std::isfinite(v)) return std::nullopt;
    return v;
}

class TextField : public Widget {
protected:
    struct Snapshot { std::string text; std::size_t caret = 0, anchor = 0; };
    struct Layout { std::string display; std::vector<std::size_t> offsets; std::vector<float> xs; double size = 0.0; };

    std::string                              m_text;
    std::string                              m_placeholder;
    std::string                              m_suffix;
    std::size_t                              m_caret = 0, m_anchor = 0;
    float                                    m_scroll = 0.0f;
    std::size_t                              m_max_length = 0;
    std::size_t                              m_min_length = 0;
    CharFilter                               m_filter;
    std::function<bool(const std::string&)>  m_validator;
    std::function<bool(const std::string&)>  m_check;
    TextCase                                 m_case = TextCase::Any;
    char32_t                                 m_mask = 0;
    bool                                     m_read_only = false;
    bool                                     m_select_on_focus = false;
    bool                                     m_show_counter = false;
    bool                                     m_dragging = false;
    bool                                     m_touched = false;
    bool                                     m_typing = false;
    std::string                              m_preedit;
    int                                      m_preedit_cursor = 0;
    std::vector<Snapshot>                    m_undo, m_redo;
    mutable Layout                           m_layout;
    std::string                              m_focus_text;
    std::function<void(const std::string&)>  m_on_change, m_on_submit, m_on_commit;
    double                                   m_rejected_at = -10.0;

    static bool word_char(char32_t c) noexcept { return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80; }

    std::string display_of(const std::string& s) const {
        if (!m_mask) return s;
        std::string out, m;
        utf8::append(m, m_mask);
        const std::size_t n = utf8::length(s);
        out.reserve(n * m.size());
        for (std::size_t i = 0; i < n; ++i) out += m;
        return out;
    }

    const Layout& layout_of(const std::string& display) const {
        const double size = theme().font_size;
        if (m_layout.display == display && m_layout.size == size && !m_layout.xs.empty()) return m_layout;
        m_layout.display = display;
        m_layout.size = size;
        m_layout.offsets.clear();
        m_layout.xs.clear();
        for (std::size_t i = 0;; i = utf8::next(display, i)) {
            m_layout.offsets.push_back(i);
            m_layout.xs.push_back(i == 0 ? 0.0f : text_width(display.substr(0, i)));
            if (i >= display.size()) break;
        }
        return m_layout;
    }

    std::size_t cp_index(std::size_t byte) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < byte && i < m_text.size(); i = utf8::next(m_text, i)) ++n;
        return n;
    }

    std::size_t byte_of(std::size_t cp) const noexcept {
        std::size_t i = 0;
        for (std::size_t k = 0; k < cp && i < m_text.size(); ++k) i = utf8::next(m_text, i);
        return i;
    }

    float caret_x(std::size_t byte) const {
        const Layout& l = layout_of(display_of(m_text));
        const std::size_t k = std::min(cp_index(byte), l.xs.size() - 1);
        return l.xs[k];
    }

    std::size_t index_at(float local_x) const {
        const Rect tr = text_rect();
        const float x = local_x - tr.x + m_scroll;
        const Layout& l = layout_of(display_of(m_text));
        std::size_t best = 0;
        float bd = 1e30f;
        for (std::size_t k = 0; k < l.xs.size(); ++k) {
            const float d = std::abs(l.xs[k] - x);
            if (d < bd) { bd = d; best = k; }
        }
        return byte_of(best);
    }

    void ensure_caret_visible() {
        const Rect tr = text_rect();
        const float cx = caret_x(m_caret);
        const float total = caret_x(m_text.size());
        if (cx - m_scroll > tr.w - 2.0f) m_scroll = cx - tr.w + 2.0f;
        if (cx - m_scroll < 0.0f) m_scroll = cx;
        if (total - m_scroll < tr.w - 2.0f) m_scroll = std::max(0.0f, total - tr.w + 2.0f);
        if (m_scroll < 0.0f) m_scroll = 0.0f;
    }

    std::size_t word_left(std::size_t i) const noexcept {
        while (i > 0) { const std::size_t p = utf8::prev(m_text, i); if (word_char(utf8::decode(m_text, p))) break; i = p; }
        while (i > 0) { const std::size_t p = utf8::prev(m_text, i); if (!word_char(utf8::decode(m_text, p))) break; i = p; }
        return i;
    }

    std::size_t word_right(std::size_t i) const noexcept {
        while (i < m_text.size() && !word_char(utf8::decode(m_text, i))) i = utf8::next(m_text, i);
        while (i < m_text.size() && word_char(utf8::decode(m_text, i))) i = utf8::next(m_text, i);
        return i;
    }

    void push_undo(bool typing) {
        if (typing && m_typing && !m_undo.empty()) return;
        m_undo.push_back(Snapshot{ m_text, m_caret, m_anchor });
        if (m_undo.size() > 200) m_undo.erase(m_undo.begin());
        m_redo.clear();
        m_typing = typing;
    }

    void changed() {
        m_touched = true;
        m_layout.xs.clear();
        ensure_caret_visible();
        if (m_on_change) m_on_change(m_text);
        text_changed();
    }

    char32_t apply_case(char32_t c) const noexcept {
        if (m_case == TextCase::Upper) return static_cast<char32_t>(std::towupper(static_cast<std::wint_t>(c)));
        if (m_case == TextCase::Lower) return static_cast<char32_t>(std::towlower(static_cast<std::wint_t>(c)));
        return c;
    }

    virtual void text_changed() {}
    virtual void committed() { if (m_on_commit) m_on_commit(m_text); }
    virtual float extra_left() const { return 0.0f; }
    virtual float extra_right() const { return 0.0f; }

public:
    explicit TextField(std::string text = "", std::string placeholder = "") : m_text(std::move(text)), m_placeholder(std::move(placeholder)) {
        m_focusable = true;
        m_caret = m_anchor = m_text.size();
    }

    const std::string& text() const noexcept { return m_text; }

    TextField& set_text(std::string t) {
        if (m_max_length && utf8::length(t) > m_max_length) t = t.substr(0, [&] { std::size_t i = 0; for (std::size_t k = 0; k < m_max_length; ++k) i = utf8::next(t, i); return i; }());
        m_text = std::move(t);
        m_caret = m_anchor = m_text.size();
        m_layout.xs.clear();
        m_scroll = 0.0f;
        m_undo.clear(); m_redo.clear();
        if (focused()) m_focus_text = m_text;
        return *this;
    }

    TextField& set_placeholder(std::string p) { m_placeholder = std::move(p); return *this; }
    const std::string& placeholder() const noexcept { return m_placeholder; }
    TextField& set_suffix(std::string s) { m_suffix = std::move(s); return *this; }
    TextField& set_max_length(std::size_t n) { m_max_length = n; if (n && utf8::length(m_text) > n) set_text(m_text); return *this; }
    std::size_t max_length() const noexcept { return m_max_length; }
    TextField& set_min_length(std::size_t n) noexcept { m_min_length = n; return *this; }
    std::size_t min_length() const noexcept { return m_min_length; }
    std::size_t length() const noexcept { return utf8::length(m_text); }
    TextField& set_filter(CharFilter f) { m_filter = std::move(f); return *this; }
    TextField& set_allowed_chars(const std::string& chars) { m_filter = CharFilter::only(chars); return *this; }
    TextField& set_blocked_chars(const std::string& chars) { m_filter = m_filter ? (m_filter && CharFilter::except(chars)) : CharFilter::except(chars); return *this; }
    TextField& set_validator(std::function<bool(const std::string&)> fn) { m_validator = std::move(fn); return *this; }
    TextField& set_check(std::function<bool(const std::string&)> fn) { m_check = std::move(fn); return *this; }
    TextField& set_case(TextCase c) noexcept { m_case = c; return *this; }
    TextField& set_password(bool on, char32_t mask = U'•') noexcept { m_mask = on ? mask : 0; m_layout.xs.clear(); return *this; }
    bool password() const noexcept { return m_mask != 0; }
    TextField& set_read_only(bool r) noexcept { m_read_only = r; return *this; }
    bool read_only() const noexcept { return m_read_only; }
    TextField& set_select_all_on_focus(bool s) noexcept { m_select_on_focus = s; return *this; }
    TextField& set_show_counter(bool s) noexcept { m_show_counter = s; return *this; }
    TextField& on_change(std::function<void(const std::string&)> fn) { m_on_change = std::move(fn); return *this; }
    TextField& on_submit(std::function<void(const std::string&)> fn) { m_on_submit = std::move(fn); return *this; }
    TextField& on_commit(std::function<void(const std::string&)> fn) { m_on_commit = std::move(fn); return *this; }

    bool valid() const {
        if (utf8::length(m_text) < m_min_length) return false;
        if (m_check && !m_check(m_text)) return false;
        return true;
    }

    bool rejected_recently() const noexcept { return ui() && ui()->time() - m_rejected_at < 0.25; }
    std::size_t caret() const noexcept { return m_caret; }
    std::size_t anchor() const noexcept { return m_anchor; }
    bool has_selection() const noexcept { return m_caret != m_anchor; }
    std::string selected_text() const { const auto a = std::min(m_caret, m_anchor), b = std::max(m_caret, m_anchor); return m_text.substr(a, b - a); }
    void select_all() noexcept { m_anchor = 0; m_caret = m_text.size(); }
    void select(std::size_t anchor, std::size_t caret) noexcept { m_anchor = std::min(anchor, m_text.size()); m_caret = std::min(caret, m_text.size()); }
    void set_caret(std::size_t byte) noexcept { m_caret = m_anchor = std::min(byte, m_text.size()); }

    bool insert(const std::string& raw, bool typing = false) {
        if (m_read_only || !enabled()) return false;
        std::string s;
        bool dropped = false;
        for (std::size_t i = 0; i < raw.size(); i = utf8::next(raw, i)) {
            char32_t c = utf8::decode(raw, i);
            if (c == '\r') continue;
            if (c == '\n' || c == '\t') c = ' ';
            if (c < 0x20 || c == 0x7F) continue;
            c = apply_case(c);
            if (!m_filter(c)) { dropped = true; continue; }
            utf8::append(s, c);
        }
        const std::size_t a = std::min(m_caret, m_anchor), b = std::max(m_caret, m_anchor);
        if (m_max_length) {
            const std::size_t keep = utf8::length(m_text) - utf8::length(m_text.substr(a, b - a));
            const std::size_t room = keep >= m_max_length ? 0 : m_max_length - keep;
            if (utf8::length(s) > room) {
                std::size_t i = 0;
                for (std::size_t k = 0; k < room; ++k) i = utf8::next(s, i);
                s.resize(i);
                dropped = true;
            }
        }
        if (s.empty() && a == b) { if (dropped && ui()) m_rejected_at = ui()->time(); return false; }
        std::string candidate = m_text.substr(0, a) + s + m_text.substr(b);
        if (m_validator && !m_validator(candidate)) { if (ui()) m_rejected_at = ui()->time(); return false; }
        push_undo(typing && a == b);
        m_text = std::move(candidate);
        m_caret = m_anchor = a + s.size();
        if (dropped && ui()) m_rejected_at = ui()->time();
        changed();
        return true;
    }

    bool erase_selection() {
        if (m_read_only || m_caret == m_anchor) return false;
        const std::size_t a = std::min(m_caret, m_anchor), b = std::max(m_caret, m_anchor);
        std::string candidate = m_text.substr(0, a) + m_text.substr(b);
        if (m_validator && !m_validator(candidate)) { if (ui()) m_rejected_at = ui()->time(); return false; }
        push_undo(false);
        m_text = std::move(candidate);
        m_caret = m_anchor = a;
        changed();
        return true;
    }

    bool undo() {
        if (m_undo.empty()) return false;
        m_redo.push_back(Snapshot{ m_text, m_caret, m_anchor });
        const Snapshot s = m_undo.back();
        m_undo.pop_back();
        m_text = s.text; m_caret = s.caret; m_anchor = s.anchor;
        m_typing = false;
        changed();
        return true;
    }

    bool redo() {
        if (m_redo.empty()) return false;
        m_undo.push_back(Snapshot{ m_text, m_caret, m_anchor });
        const Snapshot s = m_redo.back();
        m_redo.pop_back();
        m_text = s.text; m_caret = s.caret; m_anchor = s.anchor;
        m_typing = false;
        changed();
        return true;
    }

    virtual Rect text_rect() const noexcept {
        const Theme& t = theme();
        float right = extra_right() + t.padding;
        if (m_show_counter) right += counter_width();
        return Rect(t.padding + extra_left(), 0.0f, std::max(0.0f, width() - t.padding - extra_left() - right), height());
    }

    float counter_width() const {
        if (!m_show_counter) return 0.0f;
        const std::string c = m_max_length ? std::to_string(m_max_length) + "/" + std::to_string(m_max_length) : "0000";
        return text_width(c, theme().font_size * 0.8) + 6.0f;
    }

    bool wants_text_input() const noexcept override { return !m_read_only; }

    windows::SystemCursor cursor(float x, float y) const noexcept override {
        return text_rect().contains(x, y) || bounds().contains(x, y) ? windows::SystemCursor::IBeam : windows::SystemCursor::Arrow;
    }

    Size measure(Ui&) override {
        const Theme& t = theme();
        return Size{ std::max(140.0f, extra_left() + extra_right() + t.padding * 2.0f + counter_width() + 40.0f), t.row_height };
    }

    void draw_frame(Painter& p) {
        const Theme& t = theme();
        const bool en = enabled();
        Color border = t.border;
        if (!en) border = t.surface_disabled;
        else if (rejected_recently()) border = t.warning;
        else if (m_touched && !valid()) border = t.error;
        else if (focus_within()) border = t.focus;
        else if (hovered()) border = t.border_hover;
        p.box(bounds(), t.radius, en ? t.field : t.surface_disabled, border, focus_within() ? std::max(t.border_width, 1.5f) : t.border_width);
    }

    void draw_text_content(Painter& p, Ui& ui) {
        const Theme& t = theme();
        const Rect tr = text_rect();
        const bool foc = focused();
        const float lh = line_height();
        const float ty = std::round(tr.y + (tr.h - lh) * 0.5f);
        p.push_clip(Rect(tr.x - 1.0f, 1.0f, tr.w + 2.0f, height() - 2.0f));
        const std::string disp = display_of(m_text);
        if (m_preedit.empty()) {
            if (foc && m_caret != m_anchor) {
                const float x0 = caret_x(std::min(m_caret, m_anchor)), x1 = caret_x(std::max(m_caret, m_anchor));
                p.fill_rect(Rect(tr.x + x0 - m_scroll, ty, x1 - x0, lh), t.selection);
            }
            if (m_text.empty() && !m_placeholder.empty()) p.text(tr.x, ty, m_placeholder, text_style(t.placeholder));
            else p.text(tr.x - m_scroll, ty, disp, text_style(enabled() ? t.text : t.text_disabled));
            if (!m_suffix.empty() && !m_text.empty()) p.text(tr.x - m_scroll + caret_x(m_text.size()) + 2.0f, ty, m_suffix, text_style(t.text_muted));
            if (foc && !m_read_only && ui.caret_on()) p.fill_rect(Rect(std::round(tr.x + caret_x(m_caret) - m_scroll), ty, 1.0f, lh), t.caret);
        } else {
            const std::size_t at = std::min(m_caret, m_anchor);
            const std::string before = display_of(m_text.substr(0, at));
            const std::string full = before + m_preedit + display_of(m_text.substr(std::max(m_caret, m_anchor)));
            const float bx = text_width(before), pw = text_width(m_preedit);
            p.text(tr.x - m_scroll, ty, full, text_style(t.text));
            p.fill_rect(Rect(tr.x + bx - m_scroll, ty + lh - 2.0f, pw, 1.0f), t.text);
            std::size_t pc = 0;
            for (int k = 0; k < m_preedit_cursor && pc < m_preedit.size(); ++k) pc = utf8::next(m_preedit, pc);
            const float cx = bx + text_width(m_preedit.substr(0, pc));
            if (ui.caret_on()) p.fill_rect(Rect(std::round(tr.x + cx - m_scroll), ty, 1.0f, lh), t.caret);
        }
        p.pop_clip();
        if (m_show_counter) {
            const std::string c = m_max_length ? std::to_string(length()) + "/" + std::to_string(m_max_length) : std::to_string(length());
            const double sz = t.font_size * 0.8;
            const Size cs = text_size(c, sz);
            const bool full = m_max_length && length() >= m_max_length;
            p.text(width() - t.padding - cs.w - extra_right(), std::round((height() - cs.h) * 0.5f), c, text_style(full ? t.warning : t.text_muted, sz));
        }
    }

    void draw(Painter& p, Ui& ui) override {
        draw_frame(p);
        draw_text_content(p, ui);
    }

    void update(Ui& ui, double dt) override {
        if (!m_dragging || !pressed()) return;
        const Rect sr = screen_rect();
        const Rect tr = text_rect();
        const float mx = ui.mouse().x - sr.x;
        const float speed = static_cast<float>(dt) * 600.0f;
        if (mx < tr.x) { m_scroll = std::max(0.0f, m_scroll - speed); m_caret = index_at(mx); }
        else if (mx > tr.right()) { m_scroll += speed; m_caret = index_at(mx); ensure_caret_visible(); }
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return false;
        if (e.focus_gained && m_select_on_focus && e.clicks == 1) { select_all(); ensure_caret_visible(); return true; }
        const std::size_t i = index_at(e.x);
        if (e.clicks >= 2) {
            std::size_t a = i, b = i;
            const bool w = i < m_text.size() && word_char(utf8::decode(m_text, i));
            if (w || (i > 0 && word_char(utf8::decode(m_text, utf8::prev(m_text, i))))) { a = word_left(i < m_text.size() && w ? utf8::next(m_text, i) : i); b = word_right(a); }
            else { a = i > 0 ? utf8::prev(m_text, i) : 0; b = i < m_text.size() ? utf8::next(m_text, i) : i; }
            m_anchor = a; m_caret = b;
            m_dragging = false;
            return true;
        }
        m_caret = i;
        if (!e.shift()) m_anchor = i;
        m_dragging = true;
        m_typing = false;
        return true;
    }

    void on_mouse_move(MouseEvent& e) override {
        if (!m_dragging || !pressed()) return;
        m_caret = index_at(e.x);
        ensure_caret_visible();
    }

    void on_mouse_up(MouseEvent&) override { m_dragging = false; }

    bool on_text(const std::string& s) override {
        m_preedit.clear();
        insert(s, true);
        return true;
    }

    void on_preedit(const std::string& s, int cursor) override { m_preedit = s; m_preedit_cursor = cursor; }

    void on_focus_changed(bool f) override {
        if (f) {
            m_focus_text = m_text;
            if (m_select_on_focus) select_all();
            m_typing = false;
        } else {
            m_preedit.clear();
            m_dragging = false;
            m_anchor = m_caret;
            if (m_text != m_focus_text) m_touched = true;
            committed();
            m_scroll = 0.0f;
            ensure_caret_visible();
        }
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        const bool ctrl = k.ctrl(), shift = k.shift();
        auto move = [&](std::size_t to) {
            m_caret = to;
            if (!shift) m_anchor = to;
            m_typing = false;
            ensure_caret_visible();
            return true;
        };
        switch (k.key) {
            case K::Left:
                if (!shift && has_selection() && !ctrl) return move(std::min(m_caret, m_anchor));
                return move(ctrl ? word_left(m_caret) : utf8::prev(m_text, m_caret));
            case K::Right:
                if (!shift && has_selection() && !ctrl) return move(std::max(m_caret, m_anchor));
                return move(ctrl ? word_right(m_caret) : utf8::next(m_text, m_caret));
            case K::Home: return move(0);
            case K::End: return move(m_text.size());
            case K::Backspace:
                if (m_read_only) return true;
                if (!has_selection()) m_anchor = ctrl ? word_left(m_caret) : utf8::prev(m_text, m_caret);
                erase_selection();
                return true;
            case K::Delete:
                if (m_read_only) return true;
                if (!has_selection()) m_anchor = ctrl ? word_right(m_caret) : utf8::next(m_text, m_caret);
                erase_selection();
                return true;
            case K::Enter:
            case K::NumpadEnter:
                if (m_on_submit) m_on_submit(m_text);
                submitted();
                return true;
            default: break;
        }
        if (ctrl && !k.alt()) {
            switch (k.key) {
                case K::A: select_all(); return true;
                case K::C: if (has_selection() && !m_mask && ui()) ui()->set_clipboard(selected_text()); return true;
                case K::X: if (has_selection() && !m_mask && ui() && !m_read_only) { ui()->set_clipboard(selected_text()); erase_selection(); } return true;
                case K::V: if (ui() && !m_read_only) insert(ui()->clipboard(), false); return true;
                case K::Z: if (shift) redo(); else undo(); return true;
                case K::Y: redo(); return true;
                default: break;
            }
        }
        if (k.key == K::Space || input::is_letter(k.key) || input::is_digit(k.key)) return !ctrl && !k.alt();
        return false;
    }

    virtual void submitted() { committed(); m_focus_text = m_text; }
};

template <typename T>
class NumberField : public TextField {
    static_assert(std::is_arithmetic<T>::value, "NumberField needs an arithmetic type");

protected:
    T                         m_value = T(0);
    T                         m_min = std::is_floating_point<T>::value ? static_cast<T>(-1e300) : std::numeric_limits<T>::lowest();
    T                         m_max = std::is_floating_point<T>::value ? static_cast<T>(1e300) : std::numeric_limits<T>::max();
    T                         m_step = std::is_floating_point<T>::value ? static_cast<T>(0.1) : T(1);
    int                       m_decimals = -1;
    bool                      m_wrap = false;
    bool                      m_spin = false;
    bool                      m_scrub = true;
    double                    m_scrub_speed = 1.0;
    std::string               m_tag;
    Color                     m_tag_color{ 0, 0, 0, 0 };
    int                       m_tag_axis = -1;
    bool                      m_scrub_pending = false;
    bool                      m_scrubbing = false;
    bool                      m_scrub_focus = false;
    float                     m_scrub_x = 0.0f;
    T                         m_scrub_start = T(0);
    int                       m_spin_dir = 0;
    double                    m_spin_timer = 0.0;
    std::function<void(T)>    m_on_value;
    std::function<void(T)>    m_on_value_commit;
    std::function<std::string(T)> m_formatter;

    float tag_width() const { return m_tag.empty() ? 0.0f : std::max(16.0f, text_width(m_tag, theme().font_size * 0.85) + 8.0f); }
    float spin_width() const noexcept { return m_spin ? 16.0f : 0.0f; }

    float extra_left() const override { return tag_width(); }
    float extra_right() const override { return spin_width(); }

    T normalize(long double v) const {
        if (m_wrap && m_max > m_min) {
            if (std::is_floating_point<T>::value) {
                const long double span = static_cast<long double>(m_max) - m_min;
                long double r = std::fmod(v - m_min, span);
                if (r < 0) r += span;
                v = m_min + r;
            } else {
                const long double span = static_cast<long double>(m_max) - m_min + 1.0L;
                long double r = std::fmod(std::floor(v + 0.5L) - m_min, span);
                if (r < 0) r += span;
                v = m_min + r;
            }
        }
        if (v < static_cast<long double>(m_min)) v = m_min;
        if (v > static_cast<long double>(m_max)) v = m_max;
        if (std::is_integral<T>::value) return static_cast<T>(std::llround(v));
        return static_cast<T>(v);
    }

    void text_changed() override {}

    void committed() override {
        const auto r = evaluate_expression(m_text);
        if (r) {
            const T v = normalize(static_cast<long double>(*r));
            const bool changed_value = v != m_value;
            m_value = v;
            reformat();
            if (changed_value) {
                if (m_on_value) m_on_value(m_value);
                if (m_on_value_commit) m_on_value_commit(m_value);
            }
        } else {
            reformat();
        }
        TextField::committed();
    }

    void reformat() {
        m_text = format(m_value);
        m_caret = m_anchor = m_text.size();
        m_layout.xs.clear();
        m_scroll = 0.0f;
    }

    void step_by(long double steps) {
        const T v = normalize(static_cast<long double>(m_value) + steps * static_cast<long double>(m_step));
        if (v == m_value) return;
        m_value = v;
        reformat();
        if (focused()) select_all();
        if (m_on_value) m_on_value(m_value);
        if (m_on_value_commit) m_on_value_commit(m_value);
    }

    Rect spin_rect() const noexcept { return Rect(width() - spin_width() - 1.0f, 1.0f, spin_width(), height() - 2.0f); }
    Rect tag_rect() const noexcept { return Rect(1.0f, 1.0f, tag_width(), height() - 2.0f); }

public:
    explicit NumberField(T value = T(0)) : TextField("", "") {
        m_select_on_focus = true;
        m_filter = CharFilter::only("0123456789.,-+eE*/()%^ pPiI");
        m_value = value;
        reformat();
    }

    NumberField(T value, T min, T max, T step = std::is_floating_point<T>::value ? static_cast<T>(0.1) : T(1)) : NumberField(value) {
        m_min = min; m_max = max; m_step = step;
        m_value = normalize(value);
        reformat();
    }

    T value() const noexcept { return m_value; }

    NumberField& set_value(T v, bool notify = false) {
        const T n = normalize(static_cast<long double>(v));
        const bool ch = n != m_value;
        m_value = n;
        if (!focused() || !has_selection()) reformat();
        else { reformat(); select_all(); }
        if (notify && ch && m_on_value) m_on_value(m_value);
        return *this;
    }

    NumberField& set_range(T min, T max) { m_min = std::min(min, max); m_max = std::max(min, max); set_value(m_value); return *this; }
    NumberField& set_step(T s) noexcept { m_step = s; return *this; }
    NumberField& set_decimals(int d) { m_decimals = d; reformat(); return *this; }
    NumberField& set_wrap(bool w) noexcept { m_wrap = w; return *this; }
    NumberField& set_spin_buttons(bool s) noexcept { m_spin = s; return *this; }
    NumberField& set_scrub(bool s) noexcept { m_scrub = s; return *this; }
    NumberField& set_scrub_speed(double s) noexcept { m_scrub_speed = s; return *this; }
    NumberField& set_tag(std::string tag, const Color& color) { m_tag = std::move(tag); m_tag_color = color; m_tag_axis = -1; return *this; }
    NumberField& set_tag_axis(std::string tag, int axis) { m_tag = std::move(tag); m_tag_axis = axis; return *this; }
    NumberField& set_formatter(std::function<std::string(T)> fn) { m_formatter = std::move(fn); reformat(); return *this; }
    NumberField& on_value_change(std::function<void(T)> fn) { m_on_value = std::move(fn); return *this; }
    NumberField& on_value_commit(std::function<void(T)> fn) { m_on_value_commit = std::move(fn); return *this; }
    T min() const noexcept { return m_min; }
    T max() const noexcept { return m_max; }
    T step() const noexcept { return m_step; }

    std::string format(T v) const {
        if (m_formatter) return m_formatter(v);
        if (std::is_integral<T>::value) return std::to_string(static_cast<long long>(v));
        char buf[64];
        const double d = static_cast<double>(v);
        if (m_decimals >= 0) std::snprintf(buf, sizeof(buf), "%.*f", m_decimals, d);
        else {
            std::snprintf(buf, sizeof(buf), "%.6f", d);
            std::string s = buf;
            if (s.find('.') != std::string::npos) { while (!s.empty() && s.back() == '0') s.pop_back(); if (!s.empty() && s.back() == '.') s.pop_back(); }
            if (std::abs(d) >= 1e12 || (d != 0.0 && std::abs(d) < 1e-6)) { std::snprintf(buf, sizeof(buf), "%.6g", d); s = buf; }
            if (s == "-0") s = "0";
            return s;
        }
        std::string s = buf;
        if (s.size() > 1 && s[0] == '-' && s.find_first_not_of("-0.") == std::string::npos) s.erase(0, 1);
        return s;
    }

    Size measure(Ui& ui) override {
        Size s = TextField::measure(ui);
        s.w = std::max(70.0f, std::min(s.w, 90.0f + extra_left() + extra_right()));
        return s;
    }

    windows::SystemCursor cursor(float x, float y) const noexcept override {
        if (m_spin && spin_rect().contains(x, y)) return windows::SystemCursor::Arrow;
        if (!m_tag.empty() && tag_rect().contains(x, y)) return windows::SystemCursor::ResizeHorizontal;
        if (m_scrubbing) return windows::SystemCursor::ResizeHorizontal;
        if (!focused() && m_scrub) return windows::SystemCursor::ResizeHorizontal;
        return windows::SystemCursor::IBeam;
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        draw_frame(p);
        if (!m_tag.empty()) {
            const Rect tr = tag_rect();
            const Color axis = m_tag_axis == 0 ? t.axis_x : m_tag_axis == 1 ? t.axis_y : m_tag_axis == 2 ? t.axis_z : m_tag_axis == 3 ? t.axis_w : m_tag_color;
            const Color c = enabled() ? axis : t.text_disabled;
            p.fill_rounded(Rect(tr.x + 2.0f, tr.y + 2.0f, tr.w - 4.0f, tr.h - 4.0f), std::max(0.0f, t.radius - 1.0f), c);
            const double sz = t.font_size * 0.85;
            const Size ts = text_size(m_tag, sz, true);
            p.text(std::round(tr.center_x() - ts.w * 0.5f), std::round(tr.center_y() - ts.h * 0.5f), m_tag, text_style(Color(255, 255, 255), sz, true));
        }
        if (m_scrubbing || (!focused() && !m_text.empty())) {
            const Rect tr = text_rect();
            const float lh = line_height();
            p.push_clip(tr);
            const float w = text_width(m_text);
            const float x = m_tag.empty() ? tr.x : std::max(tr.x, std::round(tr.x + (tr.w - w) * 0.5f));
            p.text(x, std::round((height() - lh) * 0.5f), m_text, text_style(enabled() ? t.text : t.text_disabled));
            if (!m_suffix.empty()) p.text(x + w + 2.0f, std::round((height() - lh) * 0.5f), m_suffix, text_style(t.text_muted));
            p.pop_clip();
        } else {
            draw_text_content(p, ui);
        }
        if (m_spin) {
            const Rect sr = spin_rect();
            const bool hot = hovered() && enabled();
            const Point m = ui.mouse();
            const Rect sc = screen_rect();
            const float my = m.y - sc.y, mx = m.x - sc.x;
            const bool in = sr.contains(mx, my);
            const Rect up(sr.x, sr.y, sr.w, sr.h * 0.5f), dn(sr.x, sr.y + sr.h * 0.5f, sr.w, sr.h * 0.5f);
            if (hot && in) p.fill_rect(my < sr.center_y() ? up : dn, (pressed() && m_spin_dir != 0) ? t.surface_active : t.surface_hover);
            p.fill_rect(Rect(sr.x, sr.y + 2.0f, 1.0f, sr.h - 4.0f), t.border);
            const Color ac = enabled() ? t.text_muted : t.text_disabled;
            const float cx = sr.center_x(), s = 3.5f;
            p.fill_triangle(Point{ cx - s, up.center_y() + s * 0.5f }, Point{ cx + s, up.center_y() + s * 0.5f }, Point{ cx, up.center_y() - s * 0.6f }, ac);
            p.fill_triangle(Point{ cx - s, dn.center_y() - s * 0.5f }, Point{ cx + s, dn.center_y() - s * 0.5f }, Point{ cx, dn.center_y() + s * 0.6f }, ac);
        }
    }

    void update(Ui& ui, double dt) override {
        TextField::update(ui, dt);
        if (m_spin_dir == 0 || !pressed()) { m_spin_dir = 0; return; }
        m_spin_timer -= dt;
        while (m_spin_timer <= 0.0) { step_by(m_spin_dir); m_spin_timer += theme().repeat_rate; }
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return false;
        if (m_spin && spin_rect().contains(e.x, e.y)) {
            m_spin_dir = e.y < spin_rect().center_y() ? 1 : -1;
            const long double mult = e.shift() ? 10.0L : 1.0L;
            step_by(m_spin_dir * mult);
            m_spin_timer = theme().repeat_delay;
            return true;
        }
        const bool on_tag = !m_tag.empty() && tag_rect().contains(e.x, e.y);
        if (m_scrub && (on_tag || (e.focus_gained && e.clicks == 1))) {
            m_scrub_pending = true;
            m_scrubbing = false;
            m_scrub_focus = e.focus_gained;
            m_scrub_x = e.x;
            m_scrub_start = m_value;
            return true;
        }
        return TextField::on_mouse_down(e);
    }

    void on_mouse_move(MouseEvent& e) override {
        if (m_scrub_pending && pressed()) {
            const float dx = e.x - m_scrub_x;
            if (!m_scrubbing && std::abs(dx) > 3.0f) m_scrubbing = true;
            if (m_scrubbing) {
                long double mult = static_cast<long double>(m_scrub_speed);
                if (e.shift()) mult *= 10.0L;
                if (e.ctrl() || e.alt()) mult *= 0.1L;
                const long double per_px = std::is_integral<T>::value ? static_cast<long double>(m_step) * 0.25L : static_cast<long double>(m_step) * 0.5L;
                const T v = normalize(static_cast<long double>(m_scrub_start) + (dx > 0 ? dx - 3.0f : dx + 3.0f) * per_px * mult);
                if (v != m_value) {
                    m_value = v;
                    reformat();
                    if (m_on_value) m_on_value(m_value);
                }
            }
            return;
        }
        TextField::on_mouse_move(e);
    }

    void on_mouse_up(MouseEvent& e) override {
        m_spin_dir = 0;
        if (m_scrub_pending) {
            m_scrub_pending = false;
            if (m_scrubbing) {
                m_scrubbing = false;
                if (m_on_value_commit) m_on_value_commit(m_value);
                if (m_scrub_focus) blur();
                else select_all();
            } else {
                select_all();
                ensure_caret_visible();
            }
            return;
        }
        TextField::on_mouse_up(e);
    }

    bool on_wheel(MouseEvent& e) override {
        if (!focused() || e.wheel_y == 0.0f) return false;
        step_by((e.wheel_y > 0.0f ? 1.0L : -1.0L) * (e.shift() ? 10.0L : 1.0L));
        return true;
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        long double mult = k.shift() ? 10.0L : (k.ctrl() || k.alt()) ? 0.1L : 1.0L;
        if (std::is_integral<T>::value && mult < 1.0L) mult = 1.0L;
        switch (k.key) {
            case K::Up: committed_silent(); step_by(mult); return true;
            case K::Down: committed_silent(); step_by(-mult); return true;
            case K::PageUp: committed_silent(); step_by(10.0L * mult); return true;
            case K::PageDown: committed_silent(); step_by(-10.0L * mult); return true;
            case K::Escape:
                if (m_text != format(m_value)) { reformat(); select_all(); return true; }
                return false;
            default: break;
        }
        return TextField::on_key(k);
    }

    void submitted() override { committed(); select_all(); m_focus_text = m_text; }

    void committed_silent() {
        const auto r = evaluate_expression(m_text);
        if (r) m_value = normalize(static_cast<long double>(*r));
    }
};

class IntField : public NumberField<long long> {
public:
    using NumberField<long long>::NumberField;
};

class FloatField : public NumberField<double> {
public:
    using NumberField<double>::NumberField;
};

class IntSpinner : public NumberField<long long> {
public:
    explicit IntSpinner(long long value = 0) : NumberField<long long>(value) { m_spin = true; }
    IntSpinner(long long value, long long min, long long max, long long step = 1) : NumberField<long long>(value, min, max, step) { m_spin = true; }
};

class FloatSpinner : public NumberField<double> {
public:
    explicit FloatSpinner(double value = 0.0) : NumberField<double>(value) { m_spin = true; }
    FloatSpinner(double value, double min, double max, double step = 0.1) : NumberField<double>(value, min, max, step) { m_spin = true; }
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_TEXT_HPP
