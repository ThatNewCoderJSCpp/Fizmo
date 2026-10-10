#ifndef FIZMO_UI_TEXT_HPP
#define FIZMO_UI_TEXT_HPP

#include "ui_core.hpp"
#include <cctype>
#include <cwctype>
#include <limits>

namespace fizmo {
namespace ui {

namespace utf8 {

std::size_t next(const std::string& s, std::size_t i) noexcept;

std::size_t prev(const std::string& s, std::size_t i) noexcept;

char32_t decode(const std::string& s, std::size_t i) noexcept;

void append(std::string& out, char32_t cp);

inline std::size_t length(const std::string& s) noexcept {
    std::size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::u32string to_u32(const std::string& s);

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
    static CharFilter decimal();
    static CharFilter alpha();
    static CharFilter alnum() { return alpha() || digits(); }
    static CharFilter hex();
    static CharFilter identifier();
    static CharFilter ascii() { return CharFilter([](char32_t c) { return c < 0x80; }); }
    static CharFilter printable_ascii() { return CharFilter([](char32_t c) { return c >= 0x20 && c < 0x7F; }); }
    static CharFilter no_whitespace();

    static CharFilter only(const std::string& chars);

    static CharFilter except(const std::string& chars);
};

enum class TextCase : std::uint8_t { Any = 0, Upper, Lower };

std::optional<double> evaluate_expression(const std::string& text);

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

    std::string display_of(const std::string& s) const;

    const Layout& layout_of(const std::string& display) const;

    std::size_t cp_index(std::size_t byte) const noexcept;

    std::size_t byte_of(std::size_t cp) const noexcept;

    float caret_x(std::size_t byte) const;

    std::size_t index_at(float local_x) const;

    void ensure_caret_visible();

    std::size_t word_left(std::size_t i) const noexcept;

    std::size_t word_right(std::size_t i) const noexcept;

    void push_undo(bool typing);

    void changed();

    char32_t apply_case(char32_t c) const noexcept;

    virtual void text_changed() {}
    virtual void committed() { if (m_on_commit) m_on_commit(m_text); }
    virtual float extra_left() const { return 0.0f; }
    virtual float extra_right() const { return 0.0f; }

public:
    explicit TextField(std::string text = "", std::string placeholder = "");

    const std::string& text() const noexcept { return m_text; }

    TextField& set_text(std::string t);

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
    TextField& set_blocked_chars(const std::string& chars);
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

    bool valid() const;

    bool rejected_recently() const noexcept { return ui() && ui()->time() - m_rejected_at < 0.25; }
    std::size_t caret() const noexcept { return m_caret; }
    std::size_t anchor() const noexcept { return m_anchor; }
    bool has_selection() const noexcept { return m_caret != m_anchor; }
    std::string selected_text() const;
    void select_all() noexcept { m_anchor = 0; m_caret = m_text.size(); }
    void select(std::size_t anchor, std::size_t caret) noexcept { m_anchor = std::min(anchor, m_text.size()); m_caret = std::min(caret, m_text.size()); }
    void set_caret(std::size_t byte) noexcept { m_caret = m_anchor = std::min(byte, m_text.size()); }

    bool insert(const std::string& raw, bool typing = false);

    bool erase_selection();

    bool undo();

    bool redo();

    virtual Rect text_rect() const noexcept;

    float counter_width() const;

    bool wants_text_input() const noexcept override { return !m_read_only; }

    windows::SystemCursor cursor(float x, float y) const noexcept override;

    Size measure(Ui&) override;

    void draw_frame(Painter& p);

    void draw_text_content(Painter& p, Ui& ui);

    void draw(Painter& p, Ui& ui) override {
        draw_frame(p);
        draw_text_content(p, ui);
    }

    void update(Ui& ui, double dt) override;

    bool on_mouse_down(MouseEvent& e) override;

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

    void on_focus_changed(bool f) override;

    bool on_key(const KeyEvent& k) override;

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
