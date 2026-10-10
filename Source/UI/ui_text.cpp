#include "fizmo_library.hpp"
#include "ui_text.hpp"

namespace fizmo {
namespace ui {
namespace utf8 {

std::size_t next(const std::string& s, std::size_t i) noexcept {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
    return i;
}

std::size_t prev(const std::string& s, std::size_t i) noexcept {
    if (i == 0) return 0;
    if (i > s.size()) i = s.size();
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return i;
}

char32_t decode(const std::string& s, std::size_t i) noexcept {
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

void append(std::string& out, char32_t cp) {
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
}

std::u32string to_u32(const std::string& s) {
    std::u32string out;
    for (std::size_t i = 0; i < s.size(); i = next(s, i)) out.push_back(decode(s, i));
    return out;
}

} // namespace utf8
} // namespace ui
} // namespace fizmo

namespace fizmo {
namespace ui {

auto CharFilter::decimal() -> CharFilter { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == ',' || c == 'e' || c == 'E'; }); }

auto CharFilter::alpha() -> CharFilter { return CharFilter([](char32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c != 0xD7 && c != 0xF7 && c < 0x2000); }); }

auto CharFilter::hex() -> CharFilter { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }); }

auto CharFilter::identifier() -> CharFilter { return CharFilter([](char32_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }); }

auto CharFilter::no_whitespace() -> CharFilter { return CharFilter([](char32_t c) { return !(c == ' ' || c == '\t' || c == 0xA0 || c == 0x3000 || (c >= 0x2000 && c <= 0x200B)); }); }

auto CharFilter::only(const std::string& chars) -> CharFilter {
    const std::u32string set = utf8::to_u32(chars);
    return CharFilter([set](char32_t c) { return set.find(c) != std::u32string::npos; });
}

auto CharFilter::except(const std::string& chars) -> CharFilter {
    const std::u32string set = utf8::to_u32(chars);
    return CharFilter([set](char32_t c) { return set.find(c) == std::u32string::npos; });
}

std::optional<double> evaluate_expression(const std::string& text) {
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

std::string TextField::display_of(const std::string& s) const {
    if (!m_mask) return s;
    std::string out, m;
    utf8::append(m, m_mask);
    const std::size_t n = utf8::length(s);
    out.reserve(n * m.size());
    for (std::size_t i = 0; i < n; ++i) out += m;
    return out;
}

auto TextField::layout_of(const std::string& display) const -> const Layout& {
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

std::size_t TextField::cp_index(std::size_t byte) const noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < byte && i < m_text.size(); i = utf8::next(m_text, i)) ++n;
    return n;
}

std::size_t TextField::byte_of(std::size_t cp) const noexcept {
    std::size_t i = 0;
    for (std::size_t k = 0; k < cp && i < m_text.size(); ++k) i = utf8::next(m_text, i);
    return i;
}

float TextField::caret_x(std::size_t byte) const {
    const Layout& l = layout_of(display_of(m_text));
    const std::size_t k = std::min(cp_index(byte), l.xs.size() - 1);
    return l.xs[k];
}

std::size_t TextField::index_at(float local_x) const {
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

void TextField::ensure_caret_visible() {
    const Rect tr = text_rect();
    const float cx = caret_x(m_caret);
    const float total = caret_x(m_text.size());
    if (cx - m_scroll > tr.w - 2.0f) m_scroll = cx - tr.w + 2.0f;
    if (cx - m_scroll < 0.0f) m_scroll = cx;
    if (total - m_scroll < tr.w - 2.0f) m_scroll = std::max(0.0f, total - tr.w + 2.0f);
    if (m_scroll < 0.0f) m_scroll = 0.0f;
}

std::size_t TextField::word_left(std::size_t i) const noexcept {
    while (i > 0) { const std::size_t p = utf8::prev(m_text, i); if (word_char(utf8::decode(m_text, p))) break; i = p; }
    while (i > 0) { const std::size_t p = utf8::prev(m_text, i); if (!word_char(utf8::decode(m_text, p))) break; i = p; }
    return i;
}

std::size_t TextField::word_right(std::size_t i) const noexcept {
    while (i < m_text.size() && !word_char(utf8::decode(m_text, i))) i = utf8::next(m_text, i);
    while (i < m_text.size() && word_char(utf8::decode(m_text, i))) i = utf8::next(m_text, i);
    return i;
}

void TextField::push_undo(bool typing) {
    if (typing && m_typing && !m_undo.empty()) return;
    m_undo.push_back(Snapshot{ m_text, m_caret, m_anchor });
    if (m_undo.size() > 200) m_undo.erase(m_undo.begin());
    m_redo.clear();
    m_typing = typing;
}

void TextField::changed() {
    m_touched = true;
    m_layout.xs.clear();
    ensure_caret_visible();
    if (m_on_change) m_on_change(m_text);
    text_changed();
}

char32_t TextField::apply_case(char32_t c) const noexcept {
    if (m_case == TextCase::Upper) return static_cast<char32_t>(std::towupper(static_cast<std::wint_t>(c)));
    if (m_case == TextCase::Lower) return static_cast<char32_t>(std::towlower(static_cast<std::wint_t>(c)));
    return c;
}

TextField::TextField(std::string text, std::string placeholder) : m_text(std::move(text)), m_placeholder(std::move(placeholder)) {
    m_focusable = true;
    m_caret = m_anchor = m_text.size();
}

auto TextField::set_text(std::string t) -> TextField& {
    if (m_max_length && utf8::length(t) > m_max_length) t = t.substr(0, [&] { std::size_t i = 0; for (std::size_t k = 0; k < m_max_length; ++k) i = utf8::next(t, i); return i; }());
    m_text = std::move(t);
    m_caret = m_anchor = m_text.size();
    m_layout.xs.clear();
    m_scroll = 0.0f;
    m_undo.clear(); m_redo.clear();
    if (focused()) m_focus_text = m_text;
    return *this;
}

auto TextField::set_blocked_chars(const std::string& chars) -> TextField& { m_filter = m_filter ? (m_filter && CharFilter::except(chars)) : CharFilter::except(chars); return *this; }

bool TextField::valid() const {
    if (utf8::length(m_text) < m_min_length) return false;
    if (m_check && !m_check(m_text)) return false;
    return true;
}

std::string TextField::selected_text() const { const auto a = std::min(m_caret, m_anchor), b = std::max(m_caret, m_anchor); return m_text.substr(a, b - a); }

bool TextField::insert(const std::string& raw, bool typing) {
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

bool TextField::erase_selection() {
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

bool TextField::undo() {
    if (m_undo.empty()) return false;
    m_redo.push_back(Snapshot{ m_text, m_caret, m_anchor });
    const Snapshot s = m_undo.back();
    m_undo.pop_back();
    m_text = s.text; m_caret = s.caret; m_anchor = s.anchor;
    m_typing = false;
    changed();
    return true;
}

bool TextField::redo() {
    if (m_redo.empty()) return false;
    m_undo.push_back(Snapshot{ m_text, m_caret, m_anchor });
    const Snapshot s = m_redo.back();
    m_redo.pop_back();
    m_text = s.text; m_caret = s.caret; m_anchor = s.anchor;
    m_typing = false;
    changed();
    return true;
}

auto TextField::text_rect() const noexcept -> Rect {
    const Theme& t = theme();
    float right = extra_right() + t.padding;
    if (m_show_counter) right += counter_width();
    return Rect(t.padding + extra_left(), 0.0f, std::max(0.0f, width() - t.padding - extra_left() - right), height());
}

float TextField::counter_width() const {
    if (!m_show_counter) return 0.0f;
    const std::string c = m_max_length ? std::to_string(m_max_length) + "/" + std::to_string(m_max_length) : "0000";
    return text_width(c, theme().font_size * 0.8) + 6.0f;
}

auto TextField::cursor(float x, float y) const noexcept -> windows::SystemCursor {
    return text_rect().contains(x, y) || bounds().contains(x, y) ? windows::SystemCursor::IBeam : windows::SystemCursor::Arrow;
}

auto TextField::measure(Ui&) -> Size {
    const Theme& t = theme();
    return Size{ std::max(140.0f, extra_left() + extra_right() + t.padding * 2.0f + counter_width() + 40.0f), t.row_height };
}

void TextField::draw_frame(Painter& p) {
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

void TextField::draw_text_content(Painter& p, Ui& ui) {
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

void TextField::update(Ui& ui, double dt) {
    if (!m_dragging || !pressed()) return;
    const Rect sr = screen_rect();
    const Rect tr = text_rect();
    const float mx = ui.mouse().x - sr.x;
    const float speed = static_cast<float>(dt) * 600.0f;
    if (mx < tr.x) { m_scroll = std::max(0.0f, m_scroll - speed); m_caret = index_at(mx); }
    else if (mx > tr.right()) { m_scroll += speed; m_caret = index_at(mx); ensure_caret_visible(); }
}

bool TextField::on_mouse_down(MouseEvent& e) {
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

void TextField::on_focus_changed(bool f) {
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

bool TextField::on_key(const KeyEvent& k) {
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

} // namespace ui
} // namespace fizmo
