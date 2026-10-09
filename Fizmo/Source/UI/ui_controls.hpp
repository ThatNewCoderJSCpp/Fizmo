#ifndef FIZMO_UI_CONTROLS_HPP
#define FIZMO_UI_CONTROLS_HPP

#include "ui_core.hpp"
#include "ui_text.hpp"
#include "../Windows/dialogs.hpp"
#include "../System/paths.hpp"
#include <filesystem>

namespace fizmo {
namespace ui {

class Checkbox : public Widget {
protected:
    std::string               m_text;
    bool                      m_checked = false;
    bool                      m_indeterminate = false;
    bool                      m_armed = false;
    std::function<void(bool)> m_on_change;

    Rect box_rect() const noexcept {
        const float cs = theme().check_size;
        return Rect(0.0f, std::round((height() - cs) * 0.5f), cs, cs);
    }

public:
    explicit Checkbox(std::string text = "", bool checked = false) : m_text(std::move(text)), m_checked(checked) { m_focusable = true; }

    bool checked() const noexcept { return m_checked; }
    bool indeterminate() const noexcept { return m_indeterminate; }
    Checkbox& set_checked(bool c, bool notify = false) { const bool ch = c != m_checked || m_indeterminate; m_checked = c; m_indeterminate = false; if (notify && ch && m_on_change) m_on_change(m_checked); return *this; }
    Checkbox& set_indeterminate(bool i = true) noexcept { m_indeterminate = i; return *this; }
    Checkbox& set_text(std::string t) { m_text = std::move(t); return *this; }
    const std::string& text() const noexcept { return m_text; }
    Checkbox& on_change(std::function<void(bool)> fn) { m_on_change = std::move(fn); return *this; }
    void toggle() { if (enabled()) set_checked(m_indeterminate ? true : !m_checked, true); }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override {
        const Theme& t = theme();
        return Size{ t.check_size + (m_text.empty() ? 0.0f : 8.0f + text_width(m_text)), t.row_height };
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const Rect b = box_rect();
        const bool en = enabled(), hot = hovered() && en, on = m_checked || m_indeterminate;
        if (on) p.fill_rounded(b, 3.0f, !en ? t.text_disabled : pressed() ? t.accent_active : hot ? t.accent_hover : t.accent);
        else p.box(b, 3.0f, en ? t.field : t.surface_disabled, hot ? t.border_hover : t.border, t.border_width);
        const float s = b.w / 16.0f;
        if (m_indeterminate) p.fill_rect(Rect(b.x + 4.0f * s, b.center_y() - 1.0f, b.w - 8.0f * s, 2.0f), t.accent_text);
        else if (m_checked) p.polyline({ Point{ b.x + 3.6f * s, b.y + 8.2f * s }, Point{ b.x + 6.6f * s, b.y + 11.2f * s }, Point{ b.x + 12.4f * s, b.y + 4.9f * s } }, t.accent_text, 2.0f * s);
        if (focus_visible()) p.stroke_rounded(b.inset(-2.0f), 5.0f, t.focus, t.focus_width);
        if (!m_text.empty()) {
            const float lh = line_height();
            p.text(b.right() + 8.0f, std::round((height() - lh) * 0.5f), m_text, text_style(en ? t.text : t.text_disabled));
        }
    }

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) toggle(); m_armed = false; }
    bool on_key(const KeyEvent& k) override { if (k.key == input::Key::Space) { if (!k.repeat) toggle(); return true; } return false; }
};

class Toggle : public Widget {
protected:
    std::string               m_text;
    bool                      m_on = false;
    bool                      m_armed = false;
    float                     m_anim = 0.0f;
    std::function<void(bool)> m_on_change;

    Rect track() const noexcept { return Rect(0.0f, std::round((height() - 18.0f) * 0.5f), 34.0f, 18.0f); }

public:
    explicit Toggle(std::string text = "", bool on = false) : m_text(std::move(text)), m_on(on), m_anim(on ? 1.0f : 0.0f) { m_focusable = true; }

    bool checked() const noexcept { return m_on; }
    Toggle& set_checked(bool c, bool notify = false) { const bool ch = c != m_on; m_on = c; if (!ui()) m_anim = c ? 1.0f : 0.0f; if (notify && ch && m_on_change) m_on_change(m_on); return *this; }
    Toggle& set_text(std::string t) { m_text = std::move(t); return *this; }
    Toggle& on_change(std::function<void(bool)> fn) { m_on_change = std::move(fn); return *this; }
    void toggle() { if (enabled()) set_checked(!m_on, true); }
    float animation() const noexcept { return m_anim; }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override { return Size{ 34.0f + (m_text.empty() ? 0.0f : 10.0f + text_width(m_text)), theme().row_height }; }

    void update(Ui&, double dt) override {
        const float target = m_on ? 1.0f : 0.0f;
        const float k = static_cast<float>(std::min(1.0, dt * 14.0));
        m_anim += (target - m_anim) * k;
        if (std::abs(target - m_anim) < 0.01f) m_anim = target;
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = hovered() && en;
        const Rect tr = track();
        const Color off = hot ? t.border_hover : t.surface_active;
        const Color on = hot ? t.accent_hover : t.accent;
        p.fill_rounded(tr, 9.0f, en ? mix(off, on, m_anim) : t.surface_disabled);
        const float kx = tr.x + 9.0f + (tr.w - 18.0f) * m_anim;
        p.fill_circle(kx, tr.center_y(), 7.0f, en ? Color(255, 255, 255) : t.text_disabled);
        if (focus_visible()) p.stroke_rounded(tr.inset(-2.0f), 11.0f, t.focus, t.focus_width);
        if (!m_text.empty()) {
            const float lh = line_height();
            p.text(tr.right() + 10.0f, std::round((height() - lh) * 0.5f), m_text, text_style(en ? t.text : t.text_disabled));
        }
    }

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) toggle(); m_armed = false; }
    bool on_key(const KeyEvent& k) override { if (k.key == input::Key::Space || k.key == input::Key::Enter) { if (!k.repeat) toggle(); return true; } return false; }
};

class RadioButton;

class RadioGroup {
    friend class RadioButton;
    int                       m_selected = -1;
    std::function<void(int)>  m_on_change;
    std::vector<RadioButton*> m_buttons;

public:
    int selected() const noexcept { return m_selected; }
    RadioGroup& set_selected(int v, bool notify = false) { const bool ch = v != m_selected; m_selected = v; if (notify && ch && m_on_change) m_on_change(v); return *this; }
    RadioGroup& on_change(std::function<void(int)> fn) { m_on_change = std::move(fn); return *this; }
    const std::vector<RadioButton*>& buttons() const noexcept { return m_buttons; }
};

class RadioButton : public Widget {
protected:
    std::string                 m_text;
    std::shared_ptr<RadioGroup> m_group;
    int                         m_value = 0;
    bool                        m_armed = false;

public:
    RadioButton(std::string text, std::shared_ptr<RadioGroup> group, int value) : m_text(std::move(text)), m_group(std::move(group)), m_value(value) {
        m_focusable = true;
        if (!m_group) m_group = std::make_shared<RadioGroup>();
        m_group->m_buttons.push_back(this);
    }

    ~RadioButton() override {
        auto& b = m_group->m_buttons;
        b.erase(std::remove(b.begin(), b.end(), this), b.end());
    }

    bool selected() const noexcept { return m_group->selected() == m_value; }
    int value() const noexcept { return m_value; }
    RadioGroup& group() noexcept { return *m_group; }
    std::shared_ptr<RadioGroup> group_ptr() const noexcept { return m_group; }
    void select() { if (enabled()) m_group->set_selected(m_value, true); }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override {
        const Theme& t = theme();
        return Size{ t.check_size + (m_text.empty() ? 0.0f : 8.0f + text_width(m_text)), t.row_height };
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = hovered() && en, on = selected();
        const float r = t.check_size * 0.5f, cx = r, cy = height() * 0.5f;
        if (on) {
            p.fill_circle(cx, cy, r, !en ? t.text_disabled : hot ? t.accent_hover : t.accent);
            p.fill_circle(cx, cy, r * 0.4f, t.accent_text);
        } else {
            p.fill_circle(cx, cy, r, hot ? t.border_hover : t.border);
            p.fill_circle(cx, cy, r - t.border_width, en ? t.field : t.surface_disabled);
        }
        if (focus_visible()) p.stroke_circle(cx, cy, r + 2.5f, t.focus, t.focus_width);
        if (!m_text.empty()) {
            const float lh = line_height();
            p.text(t.check_size + 8.0f, std::round((height() - lh) * 0.5f), m_text, text_style(en ? t.text : t.text_disabled));
        }
    }

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) select(); m_armed = false; }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        if (k.key == K::Space) { select(); return true; }
        int dir = 0;
        if (k.key == K::Down || k.key == K::Right) dir = 1;
        else if (k.key == K::Up || k.key == K::Left) dir = -1;
        if (!dir) return false;
        const auto& b = m_group->m_buttons;
        auto it = std::find(b.begin(), b.end(), this);
        if (it == b.end()) return false;
        std::size_t i = static_cast<std::size_t>(it - b.begin());
        for (std::size_t n = 0; n < b.size(); ++n) {
            i = (i + b.size() + static_cast<std::size_t>(dir == 1 ? 1 : b.size() - 1)) % b.size();
            RadioButton* rb = b[i];
            if (rb->visible() && rb->enabled() && rb->ui() == ui()) { rb->select(); if (ui()) ui()->set_focus(rb, true); break; }
        }
        return true;
    }
};

class RadioList : public Stack {
    std::shared_ptr<RadioGroup> m_group = std::make_shared<RadioGroup>();

public:
    explicit RadioList(const std::vector<std::string>& options = {}, int selected = 0, Orientation o = Orientation::Vertical) : Stack(o) {
        m_spacing = o == Orientation::Vertical ? 0.0f : 14.0f;
        m_cross = Align::Start;
        for (std::size_t i = 0; i < options.size(); ++i) add<RadioButton>(options[i], m_group, static_cast<int>(i));
        m_group->set_selected(selected);
    }

    RadioButton& add_option(const std::string& text) { return add<RadioButton>(text, m_group, static_cast<int>(m_group->buttons().size())); }
    RadioGroup& group() noexcept { return *m_group; }
    int selected() const noexcept { return m_group->selected(); }
    RadioList& set_selected(int i, bool notify = false) { m_group->set_selected(i, notify); return *this; }
    RadioList& on_change(std::function<void(int)> fn) { m_group->on_change(std::move(fn)); return *this; }
    bool hit_self(float, float) const noexcept override { return false; }
};

class Slider : public Widget {
protected:
    double                              m_value = 0.0, m_min = 0.0, m_max = 1.0, m_step = 0.0;
    Orientation                         m_orientation = Orientation::Horizontal;
    bool                                m_show_value = false;
    int                                 m_decimals = 2;
    bool                                m_dragging = false;
    float                               m_grab = 0.0f;
    std::function<std::string(double)>  m_format;
    std::function<void(double)>         m_on_change, m_on_release;

    float value_width() const {
        if (!m_show_value) return 0.0f;
        return std::max(text_width(format(m_min)), text_width(format(m_max))) + 10.0f;
    }

    Rect track() const noexcept {
        const float r = 8.0f;
        if (m_orientation == Orientation::Horizontal) return Rect(r, height() * 0.5f, std::max(0.0f, width() - 2.0f * r - value_width()), 0.0f);
        return Rect(width() * 0.5f, r, 0.0f, std::max(0.0f, height() - 2.0f * r - (m_show_value ? line_height() + 4.0f : 0.0f)));
    }

    float ratio(double v) const noexcept { return m_max > m_min ? static_cast<float>((v - m_min) / (m_max - m_min)) : 0.0f; }

    Point thumb_pos(double v) const noexcept {
        const Rect t = track();
        if (m_orientation == Orientation::Horizontal) return Point{ t.x + t.w * ratio(v), t.y };
        return Point{ t.x, t.bottom() - t.h * ratio(v) };
    }

    double value_at(float x, float y) const noexcept {
        const Rect t = track();
        float r = m_orientation == Orientation::Horizontal ? (t.w > 0.0f ? (x - t.x) / t.w : 0.0f) : (t.h > 0.0f ? (t.bottom() - y) / t.h : 0.0f);
        r = std::max(0.0f, std::min(1.0f, r));
        return m_min + (m_max - m_min) * r;
    }

    double snap(double v) const noexcept {
        if (m_step > 0.0) v = m_min + std::round((v - m_min) / m_step) * m_step;
        return std::max(m_min, std::min(m_max, v));
    }

    void set_live(double v) {
        v = snap(v);
        if (v == m_value) return;
        m_value = v;
        if (m_on_change) m_on_change(m_value);
    }

public:
    Slider(double value = 0.0, double min = 0.0, double max = 1.0, double step = 0.0) : m_min(std::min(min, max)), m_max(std::max(min, max)), m_step(step) { m_focusable = true; m_value = snap(value); }

    double value() const noexcept { return m_value; }
    Slider& set_value(double v, bool notify = false) { const double s = snap(v); const bool ch = s != m_value; m_value = s; if (notify && ch && m_on_change) m_on_change(m_value); return *this; }
    Slider& set_range(double min, double max) { m_min = std::min(min, max); m_max = std::max(min, max); m_value = snap(m_value); return *this; }
    Slider& set_step(double s) noexcept { m_step = s; return *this; }
    Slider& set_orientation(Orientation o) noexcept { m_orientation = o; return *this; }
    Slider& set_show_value(bool s, int decimals = 2) noexcept { m_show_value = s; m_decimals = decimals; return *this; }
    Slider& set_format(std::function<std::string(double)> fn) { m_format = std::move(fn); return *this; }
    Slider& on_change(std::function<void(double)> fn) { m_on_change = std::move(fn); return *this; }
    Slider& on_release(std::function<void(double)> fn) { m_on_release = std::move(fn); return *this; }
    double min() const noexcept { return m_min; }
    double max() const noexcept { return m_max; }
    bool dragging() const noexcept { return m_dragging; }

    std::string format(double v) const {
        if (m_format) return m_format(v);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.*f", m_step >= 1.0 && std::floor(m_step) == m_step ? 0 : m_decimals, v);
        return buf;
    }

    Size measure(Ui&) override {
        const Theme& t = theme();
        if (m_orientation == Orientation::Horizontal) return Size{ 160.0f + value_width(), t.row_height };
        return Size{ std::max(t.row_height, m_show_value ? value_width() : 0.0f), 140.0f };
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = (hovered() || m_dragging) && en;
        const Rect tr = track();
        const Point th = thumb_pos(m_value);
        const Color fill = en ? (hot ? t.accent_hover : t.accent) : t.text_disabled;
        if (m_orientation == Orientation::Horizontal) {
            p.fill_rounded(Rect(tr.x, tr.y - 2.0f, tr.w, 4.0f), 2.0f, t.surface_active);
            p.fill_rounded(Rect(tr.x, tr.y - 2.0f, th.x - tr.x, 4.0f), 2.0f, fill);
        } else {
            p.fill_rounded(Rect(tr.x - 2.0f, tr.y, 4.0f, tr.h), 2.0f, t.surface_active);
            p.fill_rounded(Rect(tr.x - 2.0f, th.y, 4.0f, tr.bottom() - th.y), 2.0f, fill);
        }
        if (m_step > 0.0 && m_max > m_min && (m_max - m_min) / m_step <= 20.0) {
            for (double v = m_min; v <= m_max + 1e-9; v += m_step) {
                const Point q = thumb_pos(v);
                if (m_orientation == Orientation::Horizontal) p.fill_rect(Rect(std::round(q.x), tr.y + 5.0f, 1.0f, 3.0f), t.border);
                else p.fill_rect(Rect(tr.x + 5.0f, std::round(q.y), 3.0f, 1.0f), t.border);
            }
        }
        p.fill_circle(th.x, th.y, 8.0f, fill);
        p.fill_circle(th.x, th.y, 5.0f, en ? Color(255, 255, 255) : t.surface_disabled);
        if (focus_visible()) p.stroke_circle(th.x, th.y, 10.5f, t.focus, t.focus_width);
        if (m_show_value) {
            const std::string s = format(m_value);
            const Size ts = text_size(s);
            if (m_orientation == Orientation::Horizontal) p.text(width() - ts.w, std::round((height() - ts.h) * 0.5f), s, text_style(en ? t.text : t.text_disabled));
            else p.text(std::round((width() - ts.w) * 0.5f), height() - ts.h, s, text_style(en ? t.text : t.text_disabled));
        }
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return false;
        const Point th = thumb_pos(m_value);
        const float dx = e.x - th.x, dy = e.y - th.y;
        if (dx * dx + dy * dy <= 100.0f) m_grab = m_orientation == Orientation::Horizontal ? dx : dy;
        else { m_grab = 0.0f; set_live(value_at(e.x, e.y)); }
        m_dragging = true;
        return true;
    }

    void on_mouse_move(MouseEvent& e) override {
        if (!m_dragging || !pressed()) return;
        set_live(m_orientation == Orientation::Horizontal ? value_at(e.x - m_grab, e.y) : value_at(e.x, e.y - m_grab));
    }

    void on_mouse_up(MouseEvent&) override {
        if (!m_dragging) return;
        m_dragging = false;
        if (m_on_release) m_on_release(m_value);
    }

    bool on_wheel(MouseEvent& e) override {
        if (!focused() || e.wheel_y == 0.0f) return false;
        nudge(e.wheel_y > 0.0f ? 1.0 : -1.0);
        return true;
    }

    void nudge(double dir) {
        const double st = m_step > 0.0 ? m_step : (m_max - m_min) / 100.0;
        set_live(m_value + dir * st);
        if (m_on_release) m_on_release(m_value);
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        const double big = m_step > 0.0 ? std::max(1.0, std::round((m_max - m_min) / m_step / 10.0)) : 10.0;
        switch (k.key) {
            case K::Left: case K::Down: nudge(k.shift() ? -big : -1.0); return true;
            case K::Right: case K::Up: nudge(k.shift() ? big : 1.0); return true;
            case K::PageDown: nudge(-big); return true;
            case K::PageUp: nudge(big); return true;
            case K::Home: set_live(m_min); if (m_on_release) m_on_release(m_value); return true;
            case K::End: set_live(m_max); if (m_on_release) m_on_release(m_value); return true;
            default: return false;
        }
    }
};

class RangeSlider : public Widget {
protected:
    double m_low = 0.0, m_high = 1.0, m_min = 0.0, m_max = 1.0, m_step = 0.0, m_min_gap = 0.0;
    int    m_active = 1;
    bool   m_dragging = false;
    bool   m_show_value = false;
    int    m_decimals = 2;
    float  m_grab = 0.0f;
    std::function<void(double, double)> m_on_change, m_on_release;

    Rect track() const noexcept { return Rect(8.0f, height() * 0.5f, std::max(0.0f, width() - 16.0f - value_width()), 0.0f); }
    float x_of(double v) const noexcept { const Rect t = track(); return t.x + t.w * (m_max > m_min ? static_cast<float>((v - m_min) / (m_max - m_min)) : 0.0f); }
    double value_at(float x) const noexcept { const Rect t = track(); const float r = t.w > 0.0f ? std::max(0.0f, std::min(1.0f, (x - t.x) / t.w)) : 0.0f; return m_min + (m_max - m_min) * r; }
    double snap(double v) const noexcept { if (m_step > 0.0) v = m_min + std::round((v - m_min) / m_step) * m_step; return std::max(m_min, std::min(m_max, v)); }

    std::string fmt(double v) const { char b[48]; std::snprintf(b, sizeof(b), "%.*f", m_step >= 1.0 && std::floor(m_step) == m_step ? 0 : m_decimals, v); return b; }
    float value_width() const { return m_show_value ? text_width(fmt(m_max) + " - " + fmt(m_max)) + 10.0f : 0.0f; }

    void move_active(double v) {
        v = snap(v);
        double lo = m_low, hi = m_high;
        if (m_active == 0) lo = std::min(v, hi - m_min_gap);
        else hi = std::max(v, lo + m_min_gap);
        lo = std::max(m_min, lo); hi = std::min(m_max, hi);
        if (lo == m_low && hi == m_high) return;
        m_low = lo; m_high = hi;
        if (m_on_change) m_on_change(m_low, m_high);
    }

public:
    RangeSlider(double low = 0.25, double high = 0.75, double min = 0.0, double max = 1.0, double step = 0.0) : m_min(std::min(min, max)), m_max(std::max(min, max)), m_step(step) {
        m_focusable = true;
        set_values(low, high);
    }

    double low() const noexcept { return m_low; }
    double high() const noexcept { return m_high; }
    RangeSlider& set_values(double lo, double hi, bool notify = false) {
        if (lo > hi) std::swap(lo, hi);
        const double a = snap(lo), b = snap(hi);
        const bool ch = a != m_low || b != m_high;
        m_low = a; m_high = b;
        if (notify && ch && m_on_change) m_on_change(m_low, m_high);
        return *this;
    }
    RangeSlider& set_range(double min, double max) { m_min = std::min(min, max); m_max = std::max(min, max); return set_values(m_low, m_high); }
    RangeSlider& set_step(double s) noexcept { m_step = s; return *this; }
    RangeSlider& set_min_gap(double g) noexcept { m_min_gap = std::max(0.0, g); return *this; }
    RangeSlider& set_show_value(bool s, int decimals = 2) noexcept { m_show_value = s; m_decimals = decimals; return *this; }
    RangeSlider& on_change(std::function<void(double, double)> fn) { m_on_change = std::move(fn); return *this; }
    RangeSlider& on_release(std::function<void(double, double)> fn) { m_on_release = std::move(fn); return *this; }
    int active_thumb() const noexcept { return m_active; }

    Size measure(Ui&) override { return Size{ 180.0f + value_width(), theme().row_height }; }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = (hovered() || m_dragging) && en;
        const Rect tr = track();
        const float a = x_of(m_low), b = x_of(m_high);
        const Color fill = en ? (hot ? t.accent_hover : t.accent) : t.text_disabled;
        p.fill_rounded(Rect(tr.x, tr.y - 2.0f, tr.w, 4.0f), 2.0f, t.surface_active);
        p.fill_rect(Rect(a, tr.y - 2.0f, b - a, 4.0f), fill);
        for (int i = 0; i < 2; ++i) {
            const float x = i == 0 ? a : b;
            p.fill_circle(x, tr.y, 8.0f, fill);
            p.fill_circle(x, tr.y, 5.0f, en ? Color(255, 255, 255) : t.surface_disabled);
            if (focus_visible() && m_active == i) p.stroke_circle(x, tr.y, 10.5f, t.focus, t.focus_width);
        }
        if (m_show_value) {
            const std::string s = fmt(m_low) + " - " + fmt(m_high);
            const Size ts = text_size(s);
            p.text(width() - ts.w, std::round((height() - ts.h) * 0.5f), s, text_style(en ? t.text : t.text_disabled));
        }
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return false;
        const float a = x_of(m_low), b = x_of(m_high);
        if (std::abs(e.x - a) < std::abs(e.x - b) || (a == b && e.x < a)) m_active = 0; else m_active = 1;
        const float tx = m_active == 0 ? a : b;
        if (std::abs(e.x - tx) <= 10.0f) m_grab = e.x - tx; else { m_grab = 0.0f; move_active(value_at(e.x)); }
        m_dragging = true;
        return true;
    }

    void on_mouse_move(MouseEvent& e) override { if (m_dragging && pressed()) move_active(value_at(e.x - m_grab)); }
    void on_mouse_up(MouseEvent&) override { if (m_dragging) { m_dragging = false; if (m_on_release) m_on_release(m_low, m_high); } }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        const double st = m_step > 0.0 ? m_step : (m_max - m_min) / 100.0;
        const double cur = m_active == 0 ? m_low : m_high;
        double mult = k.shift() ? 10.0 : 1.0;
        switch (k.key) {
            case K::Left: case K::Down: move_active(cur - st * mult); break;
            case K::Right: case K::Up: move_active(cur + st * mult); break;
            case K::Home: move_active(m_min); break;
            case K::End: move_active(m_max); break;
            case K::Tab: if (!k.ctrl() && !k.alt() && ((m_active == 0 && !k.shift()) || (m_active == 1 && k.shift()))) { m_active = 1 - m_active; return true; } return false;
            default: return false;
        }
        if (m_on_release) m_on_release(m_low, m_high);
        return true;
    }
};

class ProgressBar : public Widget {
protected:
    double                              m_value = 0.0;
    bool                                m_indeterminate = false;
    bool                                m_show_text = true;
    Color                               m_color{ 0, 0, 0, 0 };
    std::function<std::string(double)>  m_format;
    std::string                         m_text;

public:
    explicit ProgressBar(double value = 0.0) : m_value(std::max(0.0, std::min(1.0, value))) {}

    double value() const noexcept { return m_value; }
    ProgressBar& set_value(double v) noexcept { m_value = std::max(0.0, std::min(1.0, v)); m_indeterminate = false; return *this; }
    ProgressBar& set_indeterminate(bool i = true) noexcept { m_indeterminate = i; return *this; }
    bool indeterminate() const noexcept { return m_indeterminate; }
    ProgressBar& set_show_text(bool s) noexcept { m_show_text = s; return *this; }
    ProgressBar& set_text(std::string t) { m_text = std::move(t); return *this; }
    ProgressBar& set_format(std::function<std::string(double)> fn) { m_format = std::move(fn); return *this; }
    ProgressBar& set_color(const Color& c) noexcept { m_color = c; return *this; }
    bool hit_self(float, float) const noexcept override { return !m_tooltip.empty(); }

    std::string label() const {
        if (!m_text.empty()) return m_text;
        if (m_indeterminate) return std::string();
        if (m_format) return m_format(m_value);
        return std::to_string(static_cast<int>(std::floor(m_value * 100.0 + 1e-9))) + "%";
    }

    Size measure(Ui&) override { return Size{ 180.0f, std::max(18.0f, theme().row_height * 0.75f) }; }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        const Rect b = bounds();
        const float r = std::min(t.radius, b.h * 0.5f);
        const Color fill = m_color.alpha() ? m_color : t.accent;
        p.box(b, r, t.field, t.border, t.border_width);
        const Rect inner = b.inset(2.0f);
        if (m_indeterminate) {
            const double ph = std::fmod(ui.time() * 0.8, 1.0);
            const float w = inner.w * 0.3f;
            const float x = inner.x - w + static_cast<float>(ph) * (inner.w + w);
            p.push_clip(inner);
            p.fill_rounded(Rect(x, inner.y, w, inner.h), std::max(0.0f, r - 2.0f), fill);
            p.pop_clip();
        } else if (m_value > 0.0) {
            p.fill_rounded(Rect(inner.x, inner.y, std::max(inner.h, static_cast<float>(inner.w * m_value)), inner.h), std::max(0.0f, r - 2.0f), fill);
        }
        if (m_show_text) {
            const std::string s = label();
            if (!s.empty()) {
                const double sz = std::min(t.font_size, static_cast<double>(b.h) * 0.72);
                const Size ts = text_size(s, sz);
                const float x = std::round((b.w - ts.w) * 0.5f), y = std::round((b.h - ts.h) * 0.5f);
                const float split = inner.x + static_cast<float>(inner.w * m_value);
                p.push_clip(Rect(0.0f, 0.0f, split, b.h));
                p.text(x, y, s, text_style(t.accent_text, sz));
                p.pop_clip();
                p.push_clip(Rect(split, 0.0f, b.w - split, b.h));
                p.text(x, y, s, text_style(t.text, sz));
                p.pop_clip();
            }
        }
    }
};

class BusySpinner : public Widget {
    float m_size;
    Color m_color{ 0, 0, 0, 0 };

public:
    explicit BusySpinner(float size = 20.0f) : m_size(size) {}
    BusySpinner& set_color(const Color& c) noexcept { m_color = c; return *this; }
    bool hit_self(float, float) const noexcept override { return !m_tooltip.empty(); }
    Size measure(Ui&) override { return Size{ m_size, m_size }; }

    void draw(Painter& p, Ui& ui) override {
        const Color c = m_color.alpha() ? m_color : theme().accent;
        const float r = std::min(std::min(width(), height()), m_size) * 0.5f, cx = r, cy = height() * 0.5f;
        const int n = 10;
        const double head = std::fmod(ui.time() * 1.4, 1.0) * n;
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * 3.14159265358979323846 * i / n - 3.14159265358979323846 * 0.5;
            double age = std::fmod(head - i + n, static_cast<double>(n)) / n;
            const std::uint8_t alpha = static_cast<std::uint8_t>(std::lround(40 + 215 * (1.0 - age)));
            p.fill_circle(cx + static_cast<float>(std::cos(a)) * r * 0.72f, cy + static_cast<float>(std::sin(a)) * r * 0.72f, std::max(1.5f, r * 0.16f), with_alpha(c, alpha));
        }
    }
};

class Dropdown;

class DropdownPopup : public Widget {
    friend class Dropdown;
    Dropdown*        m_owner = nullptr;
    TextField*       m_search = nullptr;
    std::vector<int> m_items;
    float            m_scroll = 0.0f;
    int              m_hover = -1;
    int              m_pending_reveal = -1;

    float row_h() const noexcept { return theme().row_height; }
    float list_top() const noexcept { return 4.0f + (m_search && m_search->visible() ? theme().row_height + 4.0f : 0.0f); }
    Rect list_rect() const noexcept { return Rect(0.0f, list_top(), width(), std::max(0.0f, height() - list_top() - 4.0f)); }
    float content_h() const noexcept { return m_items.size() * row_h(); }

public:
    explicit DropdownPopup(Dropdown* owner);

    void refresh();
    void ensure_visible(int item_pos) {
        const Rect lr = list_rect();
        const float y = item_pos * row_h();
        if (y < m_scroll) m_scroll = y;
        if (y + row_h() > m_scroll + lr.h) m_scroll = y + row_h() - lr.h;
        m_scroll = std::max(0.0f, std::min(m_scroll, std::max(0.0f, content_h() - lr.h)));
    }

    const std::vector<int>& items() const noexcept { return m_items; }
    TextField* search() const noexcept { return m_search; }

    Size measure(Ui& ui) override;
    void arrange(Ui& ui) override {
        if (m_search && m_search->visible()) { m_search->set_rect(Rect(4.0f, 4.0f, width() - 8.0f, theme().row_height)); m_search->arrange(ui); }
        if (m_pending_reveal >= 0 && height() > 0.0f) { ensure_visible(m_pending_reveal); m_pending_reveal = -1; }
    }
    void draw(Painter& p, Ui& ui) override;
    bool on_mouse_down(MouseEvent& e) override;
    void on_mouse_move(MouseEvent& e) override;
    void on_mouse_up(MouseEvent&) override {}
    bool on_wheel(MouseEvent& e) override {
        const Rect lr = list_rect();
        m_scroll = std::max(0.0f, std::min(m_scroll - e.wheel_y * row_h() * 3.0f, std::max(0.0f, content_h() - lr.h)));
        return true;
    }
    bool on_key(const KeyEvent& k) override;
};

class Dropdown : public Widget {
    friend class DropdownPopup;

protected:
    std::vector<std::string>       m_items;
    int                            m_selected = -1;
    int                            m_highlight = -1;
    std::string                    m_placeholder = "Select...";
    int                            m_max_visible = 8;
    bool                           m_searchable = false;
    std::unique_ptr<DropdownPopup> m_popup;
    std::function<void(int)>       m_on_change;
    std::string                    m_typeahead;
    double                         m_typeahead_time = -10.0;

    int find_prefix(const std::string& prefix, int from) const {
        auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
        const std::string p = lower(prefix);
        const int n = static_cast<int>(m_items.size());
        for (int k = 0; k < n; ++k) {
            const int i = (from + k) % n;
            if (lower(m_items[static_cast<std::size_t>(i)]).compare(0, p.size(), p) == 0) return i;
        }
        return -1;
    }

    static char key_char(input::Key k) noexcept {
        if (input::is_letter(k)) return static_cast<char>('a' + (static_cast<int>(k) - static_cast<int>(input::Key::A)));
        if (input::is_digit(k)) return static_cast<char>('0' + (static_cast<int>(k) - static_cast<int>(input::Key::Num0)));
        if (k == input::Key::Space) return ' ';
        return 0;
    }

public:
    explicit Dropdown(std::vector<std::string> items = {}, int selected = -1) : m_items(std::move(items)) {
        m_focusable = true;
        m_selected = selected >= 0 && selected < static_cast<int>(m_items.size()) ? selected : -1;
        m_popup = std::make_unique<DropdownPopup>(this);
    }

    ~Dropdown() override { if (ui() && m_popup) ui()->close_popup(*m_popup); }

    const std::vector<std::string>& items() const noexcept { return m_items; }
    Dropdown& set_items(std::vector<std::string> items) { m_items = std::move(items); if (m_selected >= static_cast<int>(m_items.size())) m_selected = -1; if (is_open()) m_popup->refresh(); return *this; }
    Dropdown& add_item(std::string item) { m_items.push_back(std::move(item)); return *this; }
    int selected() const noexcept { return m_selected; }
    std::string selected_text() const { return m_selected >= 0 ? m_items[static_cast<std::size_t>(m_selected)] : std::string(); }
    Dropdown& set_selected(int i, bool notify = false) {
        if (i < -1 || i >= static_cast<int>(m_items.size())) i = -1;
        const bool ch = i != m_selected;
        m_selected = i;
        if (notify && ch && m_on_change) m_on_change(i);
        return *this;
    }
    Dropdown& set_placeholder(std::string p) { m_placeholder = std::move(p); return *this; }
    Dropdown& set_max_visible(int n) noexcept { m_max_visible = std::max(1, n); return *this; }
    Dropdown& set_searchable(bool s) noexcept { m_searchable = s; return *this; }
    Dropdown& on_change(std::function<void(int)> fn) { m_on_change = std::move(fn); return *this; }
    bool is_open() const noexcept { return ui() && m_popup && ui()->popup_open(*m_popup); }
    DropdownPopup& popup() noexcept { return *m_popup; }
    int highlight() const noexcept { return m_highlight; }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    void open() {
        if (!ui() || is_open() || !enabled() || m_items.empty()) return;
        m_highlight = m_selected >= 0 ? m_selected : 0;
        if (m_popup->m_search) { m_popup->m_search->set_visible(m_searchable); m_popup->m_search->set_text(""); }
        m_popup->refresh();
        m_popup->m_scroll = 0.0f;
        ui()->open_popup(*this, *m_popup, screen_rect(), Placement::Below);
        const auto& it = m_popup->m_items;
        const auto pos = std::find(it.begin(), it.end(), m_highlight);
        m_popup->m_pending_reveal = pos != it.end() ? static_cast<int>(pos - it.begin()) : -1;
        if (m_searchable && m_popup->m_search) ui()->set_focus(m_popup->m_search);
    }

    void close() { if (ui() && m_popup) ui()->close_popup(*m_popup); }

    void choose(int i) {
        close();
        set_selected(i, true);
    }

    Size measure(Ui&) override {
        const Theme& t = theme();
        float w = text_width(m_placeholder);
        for (const std::string& s : m_items) w = std::max(w, text_width(s));
        return Size{ std::max(120.0f, w + t.padding * 2.0f + 22.0f), t.row_height };
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = hovered() && en, open = is_open();
        p.box(bounds(), t.radius, !en ? t.surface_disabled : open ? t.surface_active : hot ? t.surface_hover : t.surface, open || focused() ? t.focus : hot ? t.border_hover : t.border, t.border_width);
        const std::string s = m_selected >= 0 ? m_items[static_cast<std::size_t>(m_selected)] : m_placeholder;
        const float lh = line_height();
        p.push_clip(Rect(t.padding, 0.0f, std::max(0.0f, width() - t.padding - 22.0f), height()));
        p.text(t.padding, std::round((height() - lh) * 0.5f), s, text_style(!en ? t.text_disabled : m_selected >= 0 ? t.text : t.placeholder));
        p.pop_clip();
        p.chevron_down(width() - 13.0f, height() * 0.5f, 4.0f, en ? t.text_muted : t.text_disabled, 1.6f);
        draw_focus_ring(p);
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return false;
        if (is_open()) close(); else open();
        return true;
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        const int n = static_cast<int>(m_items.size());
        if (is_open()) return m_popup->on_key(k);
        if (n == 0) return false;
        switch (k.key) {
            case K::Enter: case K::NumpadEnter: case K::Space: case K::F4: open(); return true;
            case K::Down: if (k.alt()) { open(); return true; } set_selected(std::min(n - 1, m_selected + 1), true); return true;
            case K::Up: set_selected(std::max(0, m_selected - 1), true); return true;
            case K::Home: set_selected(0, true); return true;
            case K::End: set_selected(n - 1, true); return true;
            default: break;
        }
        const char c = key_char(k.key);
        if (c && !k.ctrl() && !k.alt()) {
            const double now = ui() ? ui()->time() : 0.0;
            if (now - m_typeahead_time > 1.0) m_typeahead.clear();
            m_typeahead_time = now;
            m_typeahead.push_back(c);
            const int start = m_typeahead.size() == 1 ? m_selected + 1 : std::max(0, m_selected);
            const int f = find_prefix(m_typeahead, start < 0 ? 0 : start);
            if (f >= 0) set_selected(f, true);
            return true;
        }
        return false;
    }
};

inline DropdownPopup::DropdownPopup(Dropdown* owner) : m_owner(owner) {
    m_search = &add<TextField>("", "Search...");
    m_search->set_visible(false);
    m_search->on_change([this](const std::string&) {
        refresh();
        m_owner->m_highlight = m_items.empty() ? -1 : m_items.front();
        m_scroll = 0.0f;
    });
    m_search->on_submit([this](const std::string&) { if (m_owner->m_highlight >= 0) m_owner->choose(m_owner->m_highlight); });
}

inline void DropdownPopup::refresh() {
    m_items.clear();
    const std::string q = m_search && m_search->visible() ? m_search->text() : std::string();
    auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    const std::string lq = lower(q);
    for (std::size_t i = 0; i < m_owner->m_items.size(); ++i) {
        if (lq.empty() || lower(m_owner->m_items[i]).find(lq) != std::string::npos) m_items.push_back(static_cast<int>(i));
    }
}

inline Size DropdownPopup::measure(Ui&) {
    const Theme& t = theme();
    float w = m_owner->width();
    for (int i : m_items) w = std::max(w, text_width(m_owner->m_items[static_cast<std::size_t>(i)]) + t.padding * 2.0f + 16.0f);
    const int rows = std::max(1, std::min(m_owner->m_max_visible, static_cast<int>(m_owner->m_items.size())));
    return Size{ w, list_top() + rows * row_h() + 4.0f };
}

inline void DropdownPopup::draw(Painter& p, Ui& ui) {
    const Theme& t = theme();
    p.box(bounds(), t.radius, t.popup, t.border, t.border_width);
    const Rect lr = list_rect();
    p.push_clip(lr);
    const float rh = row_h(), lh = line_height();
    const int first = static_cast<int>(m_scroll / rh);
    for (int k = std::max(0, first); k < static_cast<int>(m_items.size()); ++k) {
        const float y = lr.y + k * rh - m_scroll;
        if (y > lr.bottom()) break;
        const int idx = m_items[static_cast<std::size_t>(k)];
        const Rect row(4.0f, y, lr.w - 8.0f - (content_h() > lr.h ? 6.0f : 0.0f), rh);
        if (idx == m_owner->m_highlight) p.fill_rounded(row, t.radius, mix(t.popup, t.accent, 0.35f));
        else if (idx == m_hover) p.fill_rounded(row, t.radius, t.surface_hover);
        const bool sel = idx == m_owner->m_selected;
        if (sel) p.fill_rect(Rect(row.x + 2.0f, row.y + 6.0f, 2.0f, rh - 12.0f), t.accent);
        p.text(row.x + t.padding, std::round(y + (rh - lh) * 0.5f), m_owner->m_items[static_cast<std::size_t>(idx)], text_style(t.text, 0.0, sel));
    }
    if (m_items.empty()) p.text(4.0f + t.padding, std::round(lr.y + (rh - lh) * 0.5f), "No matches", text_style(t.text_muted));
    p.pop_clip();
    if (content_h() > lr.h) {
        const float th = std::max(16.0f, lr.h * lr.h / content_h());
        const float ty = lr.y + (lr.h - th) * (m_scroll / std::max(1.0f, content_h() - lr.h));
        p.fill_rounded(Rect(width() - 7.0f, ty, 4.0f, th), 2.0f, t.border_hover);
    }
    draw_children(p, ui);
}

inline bool DropdownPopup::on_mouse_down(MouseEvent& e) {
    if (e.button != input::MouseButton::Left) return true;
    const Rect lr = list_rect();
    if (!lr.contains(e.x, e.y)) return true;
    const int k = static_cast<int>((e.y - lr.y + m_scroll) / row_h());
    if (k >= 0 && k < static_cast<int>(m_items.size())) {
        Dropdown* o = m_owner;
        const int idx = m_items[static_cast<std::size_t>(k)];
        o->choose(idx);
    }
    return true;
}

inline void DropdownPopup::on_mouse_move(MouseEvent& e) {
    const Rect lr = list_rect();
    m_hover = -1;
    if (!lr.contains(e.x, e.y)) return;
    const int k = static_cast<int>((e.y - lr.y + m_scroll) / row_h());
    if (k >= 0 && k < static_cast<int>(m_items.size())) m_hover = m_items[static_cast<std::size_t>(k)];
}

inline bool DropdownPopup::on_key(const KeyEvent& k) {
    using K = input::Key;
    if (m_items.empty()) {
        if (k.key == K::Escape) { m_owner->close(); return true; }
        return false;
    }
    auto pos = std::find(m_items.begin(), m_items.end(), m_owner->m_highlight);
    int p = pos == m_items.end() ? -1 : static_cast<int>(pos - m_items.begin());
    const int n = static_cast<int>(m_items.size());
    const int page = std::max(1, static_cast<int>(list_rect().h / row_h()) - 1);
    switch (k.key) {
        case K::Down: p = std::min(n - 1, p + 1); break;
        case K::Up: p = std::max(0, p - 1); break;
        case K::PageDown: p = std::min(n - 1, p + page); break;
        case K::PageUp: p = std::max(0, p - page); break;
        case K::Home: if (m_search && m_search->visible()) return false; p = 0; break;
        case K::End: if (m_search && m_search->visible()) return false; p = n - 1; break;
        case K::Enter: case K::NumpadEnter: case K::Space:
            if (k.key == K::Space && m_search && m_search->visible()) return false;
            if (m_owner->m_highlight >= 0) m_owner->choose(m_owner->m_highlight);
            return true;
        case K::Escape: case K::Tab: m_owner->close(); return k.key == K::Escape;
        default: return false;
    }
    m_owner->m_highlight = m_items[static_cast<std::size_t>(std::max(0, p))];
    ensure_visible(std::max(0, p));
    return true;
}

class TabView : public Widget {
protected:
    struct Tab { std::string title; Widget* page = nullptr; Rect header; };
    std::vector<Tab>          m_tabs;
    int                       m_selected = -1;
    int                       m_hover_tab = -1;
    bool                      m_closable = false;
    bool                      m_page_padding = true;
    std::function<void(int)>  m_on_change;
    std::function<bool(int)>  m_on_close;

    float header_h() const noexcept { return theme().row_height + 6.0f; }
    Rect close_rect(const Tab& t) const noexcept { return Rect(t.header.right() - 20.0f, t.header.y + (t.header.h - 14.0f) * 0.5f, 14.0f, 14.0f); }

    int tab_at(float x, float y) const noexcept {
        for (std::size_t i = 0; i < m_tabs.size(); ++i) if (m_tabs[i].header.contains(x, y)) return static_cast<int>(i);
        return -1;
    }

    void layout_headers() {
        const Theme& t = theme();
        const float h = header_h();
        std::vector<float> ws;
        float total = 0.0f;
        for (const Tab& tab : m_tabs) { const float w = text_width(tab.title) + t.padding * 2.5f + (m_closable ? 18.0f : 0.0f); ws.push_back(w); total += w; }
        const float avail = std::max(0.0f, width() - 4.0f);
        const float k = total > avail && total > 0.0f ? avail / total : 1.0f;
        float x = 2.0f;
        for (std::size_t i = 0; i < m_tabs.size(); ++i) {
            const float w = std::max(48.0f, std::floor(ws[i] * k));
            m_tabs[i].header = Rect(x, 2.0f, w, h - 2.0f);
            x += w + 1.0f;
        }
    }

public:
    TabView() { m_focusable = true; }

    Widget& add_tab(const std::string& title, std::unique_ptr<Widget> page) {
        Widget& ref = adopt(std::move(page));
        m_tabs.push_back(Tab{ title, &ref, Rect() });
        if (m_selected < 0) m_selected = 0;
        ref.set_visible(static_cast<int>(m_tabs.size()) - 1 == m_selected);
        return ref;
    }

    template <typename T, typename... Args>
    T& add_tab(const std::string& title, Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        add_tab(title, std::move(p));
        return ref;
    }

    bool remove_tab(int i) {
        if (i < 0 || i >= static_cast<int>(m_tabs.size())) return false;
        Widget* page = m_tabs[static_cast<std::size_t>(i)].page;
        m_tabs.erase(m_tabs.begin() + i);
        auto keep = release(page);
        if (ui()) { auto held = std::make_shared<std::unique_ptr<Widget>>(std::move(keep)); ui()->defer([held]() { held->reset(); }); }
        if (m_tabs.empty()) m_selected = -1;
        else if (m_selected >= static_cast<int>(m_tabs.size())) m_selected = static_cast<int>(m_tabs.size()) - 1;
        else if (i < m_selected) --m_selected;
        for (std::size_t k = 0; k < m_tabs.size(); ++k) m_tabs[k].page->set_visible(static_cast<int>(k) == m_selected);
        if (m_on_change) m_on_change(m_selected);
        return true;
    }

    TabView& select(int i, bool notify = true) {
        if (i < 0 || i >= static_cast<int>(m_tabs.size()) || i == m_selected) return *this;
        m_selected = i;
        for (std::size_t k = 0; k < m_tabs.size(); ++k) m_tabs[k].page->set_visible(static_cast<int>(k) == i);
        if (notify && m_on_change) m_on_change(i);
        return *this;
    }

    int selected() const noexcept { return m_selected; }
    int tab_count() const noexcept { return static_cast<int>(m_tabs.size()); }
    Widget* page(int i) const noexcept { return i >= 0 && i < static_cast<int>(m_tabs.size()) ? m_tabs[static_cast<std::size_t>(i)].page : nullptr; }
    const std::string& title(int i) const { return m_tabs.at(static_cast<std::size_t>(i)).title; }
    TabView& set_title(int i, std::string t) { m_tabs.at(static_cast<std::size_t>(i)).title = std::move(t); return *this; }
    TabView& set_closable(bool c) noexcept { m_closable = c; return *this; }
    TabView& set_page_padding(bool p) noexcept { m_page_padding = p; return *this; }
    TabView& on_change(std::function<void(int)> fn) { m_on_change = std::move(fn); return *this; }
    TabView& on_close(std::function<bool(int)> fn) { m_on_close = std::move(fn); return *this; }
    Rect tab_header(int i) const { return m_tabs.at(static_cast<std::size_t>(i)).header; }

    bool request_close(int i) {
        if (m_on_close && !m_on_close(i)) return false;
        return remove_tab(i);
    }

    Rect page_rect() const noexcept {
        const float pad = m_page_padding ? theme().padding : 0.0f;
        return Rect(pad, header_h() + pad, std::max(0.0f, width() - pad * 2.0f), std::max(0.0f, height() - header_h() - pad * 2.0f));
    }

    Size measure(Ui& ui) override {
        const Theme& t = theme();
        float w = 0.0f, h = 0.0f;
        for (const Tab& tab : m_tabs) {
            w += text_width(tab.title) + t.padding * 2.5f + (m_closable ? 18.0f : 0.0f) + 1.0f;
            const Size s = tab.page->preferred(ui);
            h = std::max(h, s.h);
            w = std::max(w, s.w + (m_page_padding ? t.padding * 2.0f : 0.0f));
        }
        return Size{ w + 4.0f, header_h() + h + (m_page_padding ? t.padding * 2.0f : 0.0f) };
    }

    void arrange(Ui& ui) override {
        layout_headers();
        const Rect pr = page_rect();
        for (const Tab& tab : m_tabs) {
            tab.page->set_rect(pr);
            if (tab.page->visible()) tab.page->arrange(ui);
        }
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        const float hh = header_h();
        p.box(Rect(0.0f, hh - 1.0f, width(), height() - hh + 1.0f), t.radius, t.panel, t.border, t.border_width);
        const Point m = ui.mouse();
        const Rect sr = screen_rect();
        const float mx = m.x - sr.x, my = m.y - sr.y;
        m_hover_tab = hovered() ? tab_at(mx, my) : -1;
        for (std::size_t i = 0; i < m_tabs.size(); ++i) {
            const Tab& tab = m_tabs[i];
            const bool sel = static_cast<int>(i) == m_selected, hot = static_cast<int>(i) == m_hover_tab && enabled();
            const Rect h = tab.header;
            if (sel) {
                p.fill_rounded(Rect(h.x, h.y, h.w, h.h + 4.0f), t.radius, t.panel);
                p.stroke_rounded(Rect(h.x, h.y, h.w, h.h + 4.0f), t.radius, t.border, t.border_width);
                p.fill_rect(Rect(h.x + 1.0f, h.bottom() - 1.0f, h.w - 2.0f, 4.0f), t.panel);
                p.fill_rect(Rect(h.x + 2.0f, h.y, h.w - 4.0f, 2.0f), t.accent);
            } else if (hot) {
                p.fill_rounded(Rect(h.x, h.y + 2.0f, h.w, h.h - 4.0f), t.radius, t.surface_hover);
            }
            const float closew = m_closable ? 18.0f : 0.0f;
            const Size ts = text_size(tab.title);
            p.push_clip(Rect(h.x + 4.0f, h.y, std::max(0.0f, h.w - 8.0f - closew), h.h));
            const float tx = std::max(h.x + 8.0f, std::round(h.x + (h.w - closew - ts.w) * 0.5f));
            p.text(tx, std::round(h.y + (h.h - ts.h) * 0.5f), tab.title, text_style(!enabled() ? t.text_disabled : sel ? t.text : t.text_muted));
            p.pop_clip();
            if (m_closable && (sel || hot)) {
                const Rect c = close_rect(tab);
                const bool chot = hot && c.contains(mx, my);
                if (chot) p.fill_rounded(c, 3.0f, t.surface_active);
                const float s = 3.5f;
                p.line(c.center_x() - s, c.center_y() - s, c.center_x() + s, c.center_y() + s, t.text_muted, 1.4f);
                p.line(c.center_x() - s, c.center_y() + s, c.center_x() + s, c.center_y() - s, t.text_muted, 1.4f);
            }
            if (sel && focus_visible()) p.stroke_rounded(h.inset(2.0f), t.radius, t.focus, t.focus_width);
        }
        draw_children(p, ui);
    }

    bool hit_self(float, float y) const noexcept override { return y < header_h(); }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.y >= header_h()) return false;
        const int i = tab_at(e.x, e.y);
        if (i < 0) return true;
        if ((m_closable && close_rect(m_tabs[static_cast<std::size_t>(i)]).contains(e.x, e.y) && e.button == input::MouseButton::Left) || (m_closable && e.button == input::MouseButton::Middle)) { request_close(i); return true; }
        if (e.button == input::MouseButton::Left) select(i);
        return true;
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        const int n = tab_count();
        if (n == 0) return false;
        if ((k.ctrl() && k.key == K::Tab) || (k.ctrl() && (k.key == K::PageDown || k.key == K::PageUp))) {
            const bool back = k.key == K::PageUp || (k.key == K::Tab && k.shift());
            select((m_selected + (back ? n - 1 : 1)) % n);
            return true;
        }
        if (!focused()) return false;
        if (k.key == K::Left) { select(std::max(0, m_selected - 1)); return true; }
        if (k.key == K::Right) { select(std::min(n - 1, m_selected + 1)); return true; }
        if (k.key == K::Home) { select(0); return true; }
        if (k.key == K::End) { select(n - 1); return true; }
        if (k.key == K::Delete && m_closable) { request_close(m_selected); return true; }
        return false;
    }
};

class ScrollView : public Widget {
protected:
    float m_sx = 0.0f, m_sy = 0.0f, m_tx = 0.0f, m_ty = 0.0f;
    bool  m_horizontal = false;
    bool  m_vertical = true;
    bool  m_smooth = true;
    bool  m_show_v = false, m_show_h = false;
    float m_cw = 0.0f, m_ch = 0.0f, m_vw = 0.0f, m_vh = 0.0f;
    int   m_drag = 0;
    float m_drag_offset = 0.0f;
    float m_pref_h = 220.0f;

    float sb() const noexcept { return theme().scrollbar_size; }
    float max_x() const noexcept { return std::max(0.0f, m_cw - m_vw); }
    float max_y() const noexcept { return std::max(0.0f, m_ch - m_vh); }

    Rect vbar() const noexcept { return Rect(m_vw, 0.0f, sb(), m_vh); }
    Rect hbar() const noexcept { return Rect(0.0f, m_vh, m_vw, sb()); }

    Rect vthumb() const noexcept {
        const float th = m_ch > 0.0f ? std::max(20.0f, m_vh * m_vh / m_ch) : m_vh;
        const float ty = max_y() > 0.0f ? (m_vh - th) * (m_sy / max_y()) : 0.0f;
        return Rect(m_vw + 2.0f, ty + 2.0f, sb() - 4.0f, std::max(0.0f, th - 4.0f));
    }

    Rect hthumb() const noexcept {
        const float tw = m_cw > 0.0f ? std::max(20.0f, m_vw * m_vw / m_cw) : m_vw;
        const float tx = max_x() > 0.0f ? (m_vw - tw) * (m_sx / max_x()) : 0.0f;
        return Rect(tx + 2.0f, m_vh + 2.0f, std::max(0.0f, tw - 4.0f), sb() - 4.0f);
    }

    void clamp_targets() noexcept {
        m_tx = std::max(0.0f, std::min(m_tx, max_x()));
        m_ty = std::max(0.0f, std::min(m_ty, max_y()));
        m_sx = std::max(0.0f, std::min(m_sx, max_x()));
        m_sy = std::max(0.0f, std::min(m_sy, max_y()));
    }

public:
    ScrollView() = default;

    Widget* content() const noexcept { return child(0); }

    template <typename T, typename... Args>
    T& set_content(Args&&... args) {
        clear_children();
        return add<T>(std::forward<Args>(args)...);
    }

    ScrollView& set_horizontal(bool h) noexcept { m_horizontal = h; return *this; }
    ScrollView& set_vertical(bool v) noexcept { m_vertical = v; return *this; }
    ScrollView& set_smooth(bool s) noexcept { m_smooth = s; return *this; }
    ScrollView& set_preferred_height(float h) noexcept { m_pref_h = h; return *this; }
    float scroll_x() const noexcept { return m_sx; }
    float scroll_y() const noexcept { return m_sy; }
    float max_scroll_x() const noexcept { return max_x(); }
    float max_scroll_y() const noexcept { return max_y(); }
    Rect viewport() const noexcept { return Rect(0.0f, 0.0f, m_vw, m_vh); }
    Size content_size() const noexcept { return Size{ m_cw, m_ch }; }
    bool vertical_bar_visible() const noexcept { return m_show_v; }
    bool horizontal_bar_visible() const noexcept { return m_show_h; }

    void scroll_to(float x, float y, bool animate = false) {
        m_tx = x; m_ty = y;
        clamp_targets();
        if (!animate || !m_smooth) { m_sx = m_tx; m_sy = m_ty; }
    }

    void scroll_by(float dx, float dy, bool animate = true) { scroll_to(m_tx + dx, m_ty + dy, animate); }

    Point content_offset() const noexcept override { return Point{ -std::round(m_sx), -std::round(m_sy) }; }
    Rect content_clip() const noexcept override { return Rect(0.0f, 0.0f, m_vw, m_vh); }
    bool clips_children() const noexcept override { return true; }

    void reveal(const Rect& screen) override {
        const Rect me = screen_rect();
        const float top = screen.y - me.y, bottom = screen.bottom() - me.y;
        const float left = screen.x - me.x, right = screen.right() - me.x;
        float ty = m_ty, tx = m_tx;
        if (top < 0.0f) ty += top - 4.0f;
        else if (bottom > m_vh) ty += bottom - m_vh + 4.0f;
        if (left < 0.0f) tx += left - 4.0f;
        else if (right > m_vw) tx += right - m_vw + 4.0f;
        scroll_to(tx, ty, false);
    }

    Size measure(Ui& ui) override {
        Widget* c = content();
        if (!c) return Size{ 100.0f, 60.0f };
        const Size s = c->preferred(ui);
        return Size{ s.w + (m_vertical ? sb() : 0.0f), std::min(s.h, m_pref_h) };
    }

    void arrange(Ui& ui) override {
        Widget* c = content();
        m_vw = width(); m_vh = height();
        if (!c) { m_show_v = m_show_h = false; return; }
        const Size s = c->preferred(ui);
        m_show_v = m_vertical && s.h > m_vh + 0.5f;
        if (m_show_v) m_vw -= sb();
        m_show_h = m_horizontal && s.w > m_vw + 0.5f;
        if (m_show_h) {
            m_vh -= sb();
            if (!m_show_v && m_vertical && s.h > m_vh + 0.5f) { m_show_v = true; m_vw -= sb(); }
        }
        m_vw = std::max(0.0f, m_vw); m_vh = std::max(0.0f, m_vh);
        m_cw = m_horizontal ? std::max(s.w, m_vw) : m_vw;
        m_ch = m_vertical ? std::max(s.h, m_vh) : m_vh;
        c->set_rect(Rect(0.0f, 0.0f, m_cw, m_ch));
        c->arrange(ui);
        clamp_targets();
    }

    void update(Ui&, double dt) override {
        if (!m_smooth) { m_sx = m_tx; m_sy = m_ty; return; }
        const float k = static_cast<float>(std::min(1.0, dt * 16.0));
        m_sx += (m_tx - m_sx) * k;
        m_sy += (m_ty - m_sy) * k;
        if (std::abs(m_tx - m_sx) < 0.5f) m_sx = m_tx;
        if (std::abs(m_ty - m_sy) < 0.5f) m_sy = m_ty;
    }

    Widget* hit_test(float x, float y) override {
        if (!visible() || !bounds().contains(x, y)) return nullptr;
        if ((m_show_v && vbar().contains(x, y)) || (m_show_h && hbar().contains(x, y))) return this;
        return Widget::hit_test(x, y);
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        draw_children(p, ui);
        const Point m = ui.mouse();
        const Rect sr = screen_rect();
        const float mx = m.x - sr.x, my = m.y - sr.y;
        if (m_show_v) {
            p.fill_rect(vbar(), with_alpha(t.surface, 160));
            const Rect th = vthumb();
            const bool hot = m_drag == 1 || th.contains(mx, my);
            p.fill_rounded(th, th.w * 0.5f, hot ? t.text_muted : t.border_hover);
        }
        if (m_show_h) {
            p.fill_rect(hbar(), with_alpha(t.surface, 160));
            const Rect th = hthumb();
            const bool hot = m_drag == 2 || th.contains(mx, my);
            p.fill_rounded(th, th.h * 0.5f, hot ? t.text_muted : t.border_hover);
        }
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return false;
        if (m_show_v && vbar().contains(e.x, e.y)) {
            const Rect th = vthumb();
            if (th.inset(0.0f, -2.0f).contains(e.x, e.y)) { m_drag = 1; m_drag_offset = e.y - th.y; }
            else scroll_by(0.0f, (e.y < th.y ? -1.0f : 1.0f) * m_vh * 0.9f, true);
            return true;
        }
        if (m_show_h && hbar().contains(e.x, e.y)) {
            const Rect th = hthumb();
            if (th.inset(-2.0f, 0.0f).contains(e.x, e.y)) { m_drag = 2; m_drag_offset = e.x - th.x; }
            else scroll_by((e.x < th.x ? -1.0f : 1.0f) * m_vw * 0.9f, 0.0f, true);
            return true;
        }
        return false;
    }

    void on_mouse_move(MouseEvent& e) override {
        if (!pressed()) return;
        if (m_drag == 1) {
            const float th = vthumb().h + 4.0f;
            const float range = std::max(1.0f, m_vh - th);
            scroll_to(m_tx, (e.y - m_drag_offset + 2.0f) / range * max_y());
        } else if (m_drag == 2) {
            const float tw = hthumb().w + 4.0f;
            const float range = std::max(1.0f, m_vw - tw);
            scroll_to((e.x - m_drag_offset + 2.0f) / range * max_x(), m_ty);
        }
    }

    void on_mouse_up(MouseEvent&) override { m_drag = 0; }

    bool on_wheel(MouseEvent& e) override {
        const float step = theme().row_height * 2.0f;
        bool used = false;
        if (e.wheel_y != 0.0f && m_vertical && max_y() > 0.0f) {
            const float before = m_ty;
            scroll_by(0.0f, -e.wheel_y * step, true);
            used = m_ty != before;
        }
        if (e.wheel_x != 0.0f && m_horizontal && max_x() > 0.0f) {
            const float before = m_tx;
            scroll_by(e.wheel_x * step, 0.0f, true);
            used = used || m_tx != before;
        }
        return used;
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        switch (k.key) {
            case K::PageDown: scroll_by(0.0f, m_vh * 0.9f); return max_y() > 0.0f;
            case K::PageUp: scroll_by(0.0f, -m_vh * 0.9f); return max_y() > 0.0f;
            default: return false;
        }
    }
};

class Modal : public Widget {
protected:
    std::string              m_title;
    VStack*                  m_body = nullptr;
    HStack*                  m_buttons = nullptr;
    std::vector<Button*>     m_button_list;
    std::function<void(int)> m_on_result;
    int                      m_default = 0;
    int                      m_cancel = -1;
    bool                     m_closing = false;
    float                    m_min_w = 320.0f, m_max_w = 560.0f;

    float title_h() const noexcept { return m_title.empty() ? 0.0f : theme().row_height + 12.0f; }
    float pad() const noexcept { return theme().padding * 1.5f; }

public:
    explicit Modal(std::string title = "") : m_title(std::move(title)) {
        m_body = &add<VStack>();
        m_buttons = &add<HStack>();
        m_buttons->set_main_align(Align::End);
        m_buttons->set_cross_align(Align::Center);
    }

    VStack& body() noexcept { return *m_body; }
    HStack& button_row() noexcept { return *m_buttons; }
    const std::string& title() const noexcept { return m_title; }
    Modal& set_title(std::string t) { m_title = std::move(t); return *this; }
    Modal& set_width_range(float min_w, float max_w) noexcept { m_min_w = min_w; m_max_w = std::max(min_w, max_w); return *this; }
    Modal& set_default_button(int i) noexcept { m_default = i; return *this; }
    Modal& set_cancel_result(int i) noexcept { m_cancel = i; return *this; }
    Modal& on_result(std::function<void(int)> fn) { m_on_result = std::move(fn); return *this; }
    Button* button(int i) const noexcept { return i >= 0 && i < static_cast<int>(m_button_list.size()) ? m_button_list[static_cast<std::size_t>(i)] : nullptr; }
    bool closing() const noexcept { return m_closing; }

    Button& add_button(std::string text, ButtonStyle style = ButtonStyle::Normal) {
        const int index = static_cast<int>(m_button_list.size());
        Button& b = m_buttons->add<Button>(std::move(text));
        b.set_style(style);
        b.on_click([this, index]() { close(index); });
        m_button_list.push_back(&b);
        return b;
    }

    void close(int result) {
        if (m_closing) return;
        m_closing = true;
        Ui* u = ui();
        auto cb = m_on_result;
        if (!u) { if (cb) cb(result); return; }
        u->defer([u, this, cb, result]() { u->close_modal(*this); if (cb) cb(result); });
    }

    Size measure(Ui& ui) override {
        const float p = pad();
        const Size b = m_body->preferred(ui);
        const Size r = m_buttons->preferred(ui);
        const float tw = m_title.empty() ? 0.0f : text_width(m_title, theme().font_size * 1.1, true) + p * 2.0f;
        const float w = std::min(m_max_w, std::max({ m_min_w, b.w + p * 2.0f, r.w + p * 2.0f, tw }));
        return Size{ w, title_h() + b.h + (m_button_list.empty() ? 0.0f : r.h + p) + p * 2.0f };
    }

    void arrange(Ui& ui) override {
        const float p = pad();
        const Size r = m_buttons->preferred(ui);
        const float bh = m_button_list.empty() ? 0.0f : r.h;
        const float body_h = std::max(0.0f, height() - title_h() - p * 2.0f - (m_button_list.empty() ? 0.0f : bh + p));
        m_body->set_rect(Rect(p, title_h() + p, std::max(0.0f, width() - p * 2.0f), body_h));
        m_body->arrange(ui);
        m_buttons->set_visible(!m_button_list.empty());
        m_buttons->set_rect(Rect(p, height() - p - bh, std::max(0.0f, width() - p * 2.0f), bh));
        m_buttons->arrange(ui);
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        p.box(bounds(), t.radius + 2.0f, t.popup, t.border, t.border_width);
        if (!m_title.empty()) {
            const double sz = t.font_size * 1.1;
            const Size ts = text_size(m_title, sz, true);
            p.text(pad(), std::round((title_h() - ts.h) * 0.5f) + 2.0f, m_title, text_style(t.text, sz, true));
            p.fill_rect(Rect(1.0f, title_h(), width() - 2.0f, 1.0f), t.border);
        }
        draw_children(p, ui);
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        if (k.key == K::Escape) { close(m_cancel); return true; }
        if ((k.key == K::Enter || k.key == K::NumpadEnter) && m_default >= 0) { close(m_default); return true; }
        return false;
    }
};

inline void Ui::alert(const std::string& title, const std::string& message, std::function<void()> on_close) {
    auto m = std::make_unique<Modal>(title);
    m->body().add<Label>(message);
    m->add_button("OK", ButtonStyle::Primary);
    m->set_default_button(0);
    m->set_cancel_result(0);
    m->on_result([cb = std::move(on_close)](int) { if (cb) cb(); });
    show_modal(std::move(m));
}

inline void Ui::confirm(const std::string& title, const std::string& message, std::function<void(bool)> on_result, const std::string& yes, const std::string& no) {
    auto m = std::make_unique<Modal>(title);
    m->body().add<Label>(message);
    m->add_button(no);
    m->add_button(yes, ButtonStyle::Primary);
    m->set_default_button(1);
    m->set_cancel_result(0);
    m->on_result([cb = std::move(on_result)](int r) { if (cb) cb(r == 1); });
    show_modal(std::move(m));
}

inline void Ui::prompt(const std::string& title, const std::string& message, const std::string& initial, std::function<void(std::optional<std::string>)> on_result) {
    auto m = std::make_unique<Modal>(title);
    Modal* mp = m.get();
    if (!message.empty()) m->body().add<Label>(message);
    TextField& f = m->body().add<TextField>(initial);
    f.set_select_all_on_focus(true);
    f.select_all();
    f.on_submit([mp](const std::string&) { mp->close(1); });
    m->add_button("Cancel");
    m->add_button("OK", ButtonStyle::Primary);
    m->set_default_button(1);
    m->set_cancel_result(0);
    TextField* fp = &f;
    m->on_result([cb = std::move(on_result), fp](int r) { if (!cb) return; if (r == 1) cb(fp->text()); else cb(std::nullopt); });
    show_modal(std::move(m));
}

struct Keybind {
    enum class Kind : std::uint8_t { None = 0, Key, Mouse, Gamepad };

    Kind                 kind = Kind::None;
    input::Key           key = input::Key::Unknown;
    input::Modifiers     mods = input::Modifiers::None;
    input::MouseButton   mouse = input::MouseButton::None;
    input::GamepadButton pad = input::GamepadButton::South;

    static constexpr input::Modifiers kModMask = static_cast<input::Modifiers>(0x0F);

    static Keybind from_key(input::Key k, input::Modifiers m = input::Modifiers::None) noexcept { Keybind b; b.kind = Kind::Key; b.key = k; b.mods = m & kModMask; return b; }
    static Keybind from_mouse(input::MouseButton bt, input::Modifiers m = input::Modifiers::None) noexcept { Keybind b; b.kind = Kind::Mouse; b.mouse = bt; b.mods = m & kModMask; return b; }
    static Keybind from_gamepad(input::GamepadButton bt) noexcept { Keybind b; b.kind = Kind::Gamepad; b.pad = bt; return b; }

    bool empty() const noexcept { return kind == Kind::None; }

    bool operator==(const Keybind& o) const noexcept {
        if (kind != o.kind) return false;
        switch (kind) {
            case Kind::Key: return key == o.key && mods == o.mods;
            case Kind::Mouse: return mouse == o.mouse && mods == o.mods;
            case Kind::Gamepad: return pad == o.pad;
            default: return true;
        }
    }
    bool operator!=(const Keybind& o) const noexcept { return !(*this == o); }

    static const char* mouse_name(input::MouseButton b) noexcept {
        switch (b) {
            case input::MouseButton::Left: return "Mouse Left";
            case input::MouseButton::Right: return "Mouse Right";
            case input::MouseButton::Middle: return "Mouse Middle";
            case input::MouseButton::X1: return "Mouse 4";
            case input::MouseButton::X2: return "Mouse 5";
            default: return "Mouse";
        }
    }

    std::string mod_prefix() const {
        std::string s;
        if (input::has(mods, input::Modifiers::Control)) s += "Ctrl+";
        if (input::has(mods, input::Modifiers::Shift)) s += "Shift+";
        if (input::has(mods, input::Modifiers::Alt)) s += "Alt+";
        if (input::has(mods, input::Modifiers::Meta)) s += "Meta+";
        return s;
    }

    std::string to_string() const {
        switch (kind) {
            case Kind::Key: return mod_prefix() + input::key_name(key);
            case Kind::Mouse: return mod_prefix() + mouse_name(mouse);
            case Kind::Gamepad: return std::string("Gamepad ") + input::gamepad_button_name(pad);
            default: return std::string();
        }
    }

    static std::optional<Keybind> parse(const std::string& text) {
        if (text.empty() || text == "None") return Keybind();
        auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
        if (lower(text).rfind("gamepad ", 0) == 0) {
            const std::string n = lower(text.substr(8));
            for (std::size_t i = 0; i < input::kGamepadButtonCount; ++i) {
                const auto b = static_cast<input::GamepadButton>(i);
                if (lower(input::gamepad_button_name(b)) == n) return from_gamepad(b);
            }
            return std::nullopt;
        }
        std::vector<std::string> parts;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= text.size(); ++i) {
            if (i == text.size() || (text[i] == '+' && i > start)) { parts.push_back(text.substr(start, i - start)); start = i + 1; }
        }
        input::Modifiers m = input::Modifiers::None;
        for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
            const std::string p = lower(parts[i]);
            if (p == "ctrl" || p == "control") m |= input::Modifiers::Control;
            else if (p == "shift") m |= input::Modifiers::Shift;
            else if (p == "alt" || p == "option") m |= input::Modifiers::Alt;
            else if (p == "meta" || p == "super" || p == "win" || p == "cmd") m |= input::Modifiers::Meta;
            else return std::nullopt;
        }
        if (parts.empty()) return std::nullopt;
        const std::string last = parts.back();
        for (int b = 1; b <= 5; ++b) if (lower(last) == lower(mouse_name(static_cast<input::MouseButton>(b)))) return from_mouse(static_cast<input::MouseButton>(b), m);
        const input::Key k = input::key_from_name(last);
        if (k == input::Key::Unknown) return std::nullopt;
        return from_key(k, m);
    }

    bool matches(const windows::WindowEvent& e) const noexcept {
        using T = windows::WindowEventType;
        switch (kind) {
            case Kind::Key:
                if (e.type != T::KeyPress) return false;
                return (e.logical_key == key || e.physical_key == key) && (e.mods & kModMask) == mods;
            case Kind::Mouse:
                return e.type == T::MouseClick && static_cast<input::MouseButton>(e.button) == mouse && (e.mods & kModMask) == mods;
            case Kind::Gamepad:
                return e.type == T::GamepadButtonDown && e.gamepad_button == pad;
            default:
                return false;
        }
    }
};

class KeybindField : public Widget {
protected:
    Keybind                                 m_bind;
    bool                                    m_listening = false;
    input::Modifiers                        m_pending = input::Modifiers::None;
    input::Key                              m_last_mod = input::Key::Unknown;
    bool                                    m_allow_mouse = true;
    bool                                    m_allow_gamepad = true;
    bool                                    m_allow_modifier_only = true;
    bool                                    m_escape_cancels = true;
    bool                                    m_backspace_clears = true;
    bool                                    m_right_click_clears = true;
    std::string                             m_prompt = "Press a key...";
    std::function<void(const Keybind&)>     m_on_change;
    std::function<bool(const Keybind&)>     m_validator;
    double                                  m_rejected_at = -10.0;

    static input::Modifiers mod_of(input::Key k) noexcept {
        using K = input::Key;
        switch (k) {
            case K::LeftShift: case K::RightShift: return input::Modifiers::Shift;
            case K::LeftControl: case K::RightControl: return input::Modifiers::Control;
            case K::LeftAlt: case K::RightAlt: return input::Modifiers::Alt;
            case K::LeftMeta: case K::RightMeta: return input::Modifiers::Meta;
            default: return input::Modifiers::None;
        }
    }

    static input::Modifiers without(input::Modifiers a, input::Modifiers b) noexcept { return static_cast<input::Modifiers>(static_cast<std::uint16_t>(a) & ~static_cast<std::uint16_t>(b)); }

    void commit(const Keybind& b) {
        if (m_validator && !m_validator(b)) { if (ui()) m_rejected_at = ui()->time(); return; }
        m_listening = false;
        m_pending = input::Modifiers::None;
        const bool ch = b != m_bind;
        m_bind = b;
        if (ch && m_on_change) m_on_change(m_bind);
    }

public:
    explicit KeybindField(const Keybind& bind = Keybind()) : m_bind(bind) { m_focusable = true; }

    const Keybind& value() const noexcept { return m_bind; }
    KeybindField& set_value(const Keybind& b, bool notify = false) { const bool ch = b != m_bind; m_bind = b; if (notify && ch && m_on_change) m_on_change(m_bind); return *this; }
    KeybindField& clear(bool notify = true) { return set_value(Keybind(), notify); }
    bool listening() const noexcept { return m_listening; }
    void start_listening() { if (!enabled()) return; m_listening = true; m_pending = input::Modifiers::None; m_last_mod = input::Key::Unknown; focus(); }
    void cancel() noexcept { m_listening = false; m_pending = input::Modifiers::None; }
    KeybindField& set_allow_mouse(bool a) noexcept { m_allow_mouse = a; return *this; }
    KeybindField& set_allow_gamepad(bool a) noexcept { m_allow_gamepad = a; return *this; }
    KeybindField& set_allow_modifier_only(bool a) noexcept { m_allow_modifier_only = a; return *this; }
    KeybindField& set_escape_cancels(bool e) noexcept { m_escape_cancels = e; return *this; }
    KeybindField& set_backspace_clears(bool c) noexcept { m_backspace_clears = c; return *this; }
    KeybindField& set_right_click_clears(bool c) noexcept { m_right_click_clears = c; return *this; }
    KeybindField& set_prompt(std::string p) { m_prompt = std::move(p); return *this; }
    KeybindField& on_change(std::function<void(const Keybind&)> fn) { m_on_change = std::move(fn); return *this; }
    KeybindField& set_validator(std::function<bool(const Keybind&)> fn) { m_validator = std::move(fn); return *this; }
    bool captures_all_keys() const noexcept override { return m_listening; }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override {
        const Theme& t = theme();
        return Size{ std::max(130.0f, std::max(text_width(m_prompt), text_width(m_bind.to_string())) + t.padding * 2.0f), t.row_height };
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = hovered() && en;
        const bool rejected = ui.time() - m_rejected_at < 0.3;
        const Color border = rejected ? t.error : m_listening ? t.accent : focused() ? t.focus : hot ? t.border_hover : t.border;
        p.box(bounds(), t.radius, !en ? t.surface_disabled : m_listening ? mix(t.field, t.accent, 0.12f) : hot ? t.surface_hover : t.surface, border, m_listening ? 1.5f : t.border_width);
        std::string s;
        Color c = en ? t.text : t.text_disabled;
        if (m_listening) {
            s = m_pending != input::Modifiers::None ? Keybind::from_key(input::Key::Unknown, m_pending).mod_prefix() + "..." : m_prompt;
            c = mix(t.text_muted, t.text, static_cast<float>(0.5 + 0.5 * std::sin(ui.time() * 6.0)));
        } else if (m_bind.empty()) { s = "None"; c = en ? t.placeholder : t.text_disabled; }
        else s = m_bind.to_string();
        const Size ts = text_size(s, 0.0, !m_listening && !m_bind.empty());
        p.push_clip(bounds().inset(4.0f, 0.0f));
        p.text(std::round((width() - ts.w) * 0.5f), std::round((height() - ts.h) * 0.5f), s, text_style(c, 0.0, !m_listening && !m_bind.empty()));
        p.pop_clip();
        draw_focus_ring(p);
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (!m_listening) {
            if (e.button == input::MouseButton::Left) { start_listening(); return true; }
            if (e.button == input::MouseButton::Right && m_right_click_clears) { clear(true); return true; }
            return false;
        }
        if (m_allow_mouse && e.button != input::MouseButton::None) commit(Keybind::from_mouse(e.button, e.mods));
        else cancel();
        return true;
    }

    void on_focus_changed(bool f) override { if (!f) cancel(); }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        if (!m_listening) {
            if (k.key == K::Enter || k.key == K::NumpadEnter || k.key == K::Space) { start_listening(); return true; }
            if ((k.key == K::Backspace || k.key == K::Delete) && m_backspace_clears) { clear(true); return true; }
            return false;
        }
        if (k.repeat) return true;
        const input::Modifiers held = k.mods & Keybind::kModMask;
        const input::Modifiers self = mod_of(k.key);
        if (self != input::Modifiers::None) {
            m_pending = held | self;
            m_last_mod = k.key;
            return true;
        }
        if (k.key == K::Escape && m_escape_cancels && held == input::Modifiers::None) { cancel(); return true; }
        if ((k.key == K::Backspace || k.key == K::Delete) && m_backspace_clears && held == input::Modifiers::None) { m_listening = false; clear(true); return true; }
        if (k.key == K::Unknown) return true;
        commit(Keybind::from_key(k.key, held));
        return true;
    }

    bool on_key_up(const KeyEvent& k) override {
        if (!m_listening) return false;
        const input::Modifiers self = mod_of(k.key);
        if (self == input::Modifiers::None) return true;
        if (m_allow_modifier_only && m_last_mod == k.key) {
            commit(Keybind::from_key(k.key, without(m_pending, self)));
            return true;
        }
        m_pending = without(m_pending, self);
        return true;
    }

    bool on_gamepad_button(input::GamepadButton b) override {
        if (!m_listening || !m_allow_gamepad) return false;
        commit(Keybind::from_gamepad(b));
        return true;
    }
};

template <std::size_t N>
class VectorField : public HStack {
    static_assert(N >= 2 && N <= 4, "VectorField supports 2 to 4 components");

protected:
    std::array<FloatField*, N>                        m_fields{};
    std::function<void(const std::array<double, N>&)> m_on_change;

public:
    explicit VectorField(const std::array<double, N>& v = {}) : HStack(4.0f) {
        static const char* names[4] = { "X", "Y", "Z", "W" };
        for (std::size_t i = 0; i < N; ++i) {
            FloatField& f = add<FloatField>(v[i]);
            f.set_flex(1.0f);
            f.set_min_size(56.0f, 0.0f);
            f.set_tag_axis(names[i], static_cast<int>(i));
            f.on_value_change([this](double) { if (m_on_change) m_on_change(value()); });
            m_fields[i] = &f;
        }
    }

    std::array<double, N> value() const {
        std::array<double, N> out{};
        for (std::size_t i = 0; i < N; ++i) out[i] = m_fields[i]->value();
        return out;
    }

    VectorField& set_value(const std::array<double, N>& v, bool notify = false) {
        bool ch = false;
        for (std::size_t i = 0; i < N; ++i) { if (m_fields[i]->value() != v[i]) ch = true; m_fields[i]->set_value(v[i]); }
        if (notify && ch && m_on_change) m_on_change(value());
        return *this;
    }

    FloatField& component(std::size_t i) { return *m_fields.at(i); }
    VectorField& set_range(double min, double max) { for (auto* f : m_fields) f->set_range(min, max); return *this; }
    VectorField& set_step(double s) { for (auto* f : m_fields) f->set_step(s); return *this; }
    VectorField& set_decimals(int d) { for (auto* f : m_fields) f->set_decimals(d); return *this; }
    VectorField& on_change(std::function<void(const std::array<double, N>&)> fn) { m_on_change = std::move(fn); return *this; }
    bool hit_self(float, float) const noexcept override { return false; }

    template <std::size_t M = N, typename std::enable_if<M == 2, int>::type = 0>
    vector2d vec() const { return vector2d(m_fields[0]->value(), m_fields[1]->value()); }
    template <std::size_t M = N, typename std::enable_if<M == 3, int>::type = 0>
    vector3d vec() const { return vector3d(m_fields[0]->value(), m_fields[1]->value(), m_fields[2]->value()); }
    template <std::size_t M = N, typename std::enable_if<M == 4, int>::type = 0>
    vector4d vec() const { return vector4d(m_fields[0]->value(), m_fields[1]->value(), m_fields[2]->value(), m_fields[3]->value()); }

    template <std::size_t M = N, typename std::enable_if<M == 2, int>::type = 0>
    VectorField& set_vec(const vector2d& v, bool notify = false) { return set_value({ v.x, v.y }, notify); }
    template <std::size_t M = N, typename std::enable_if<M == 3, int>::type = 0>
    VectorField& set_vec(const vector3d& v, bool notify = false) { return set_value({ v.x, v.y, v.z }, notify); }
    template <std::size_t M = N, typename std::enable_if<M == 4, int>::type = 0>
    VectorField& set_vec(const vector4d& v, bool notify = false) { return set_value({ v.x, v.y, v.z, v.w }, notify); }
};

using Vector2Field = VectorField<2>;
using Vector3Field = VectorField<3>;
using Vector4Field = VectorField<4>;

enum class FileMode : std::uint8_t { Open = 0, OpenMultiple, Save, Folder };

class FilePathField : public HStack {
protected:
    TextField*                                      m_field = nullptr;
    Button*                                         m_browse = nullptr;
    FileMode                                        m_mode = FileMode::Open;
    windows::dialogs::FileDialogOptions             m_options;
    bool                                            m_must_exist = false;
    std::function<void(const std::string&)>         m_on_change;
    std::function<std::optional<std::string>(FileMode)> m_browser;

    static bool exists(const std::string& p) {
        std::error_code ec;
        return std::filesystem::exists(system::paths::from_utf8(p), ec);
    }

    void placeholder() {
        switch (m_mode) {
            case FileMode::Folder: m_field->set_placeholder("Choose a folder..."); break;
            case FileMode::Save: m_field->set_placeholder("Choose where to save..."); break;
            case FileMode::OpenMultiple: m_field->set_placeholder("Choose files..."); break;
            default: m_field->set_placeholder("Choose a file..."); break;
        }
    }

public:
    explicit FilePathField(const std::string& path = "", FileMode mode = FileMode::Open) : HStack(4.0f), m_mode(mode) {
        m_field = &add<TextField>(path);
        m_field->set_flex(1.0f);
        m_field->on_commit([this](const std::string& s) { if (m_on_change) m_on_change(s); });
        m_field->on_submit([this](const std::string&) {});
        m_field->set_check([this](const std::string& s) {
            if (!m_must_exist || s.empty()) return true;
            if (m_mode == FileMode::OpenMultiple) { for (const std::string& p : split(s)) if (!exists(p)) return false; return true; }
            if (m_mode != FileMode::Save) return exists(s);
            const std::string parent = system::paths::to_utf8(system::paths::from_utf8(s).parent_path());
            return exists(parent.empty() ? std::string(".") : parent);
        });
        m_browse = &add<Button>("...", [this]() { browse(); });
        m_browse->set_width(34.0f);
        m_browse->set_tooltip("Browse");
        placeholder();
    }

    static std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> out;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= s.size(); ++i) {
            if (i == s.size() || s[i] == ';') {
                std::string part = s.substr(start, i - start);
                while (!part.empty() && part.front() == ' ') part.erase(part.begin());
                while (!part.empty() && part.back() == ' ') part.pop_back();
                if (!part.empty()) out.push_back(part);
                start = i + 1;
            }
        }
        return out;
    }

    const std::string& path() const noexcept { return m_field->text(); }
    std::vector<std::string> paths() const { return split(m_field->text()); }
    FilePathField& set_path(const std::string& p, bool notify = false) { m_field->set_text(p); if (notify && m_on_change) m_on_change(p); return *this; }
    FilePathField& set_mode(FileMode m) { m_mode = m; placeholder(); return *this; }
    FileMode mode() const noexcept { return m_mode; }
    FilePathField& set_filters(std::vector<windows::dialogs::FileFilter> f) { m_options.filters = std::move(f); return *this; }
    FilePathField& add_filter(const std::string& name, std::vector<std::string> patterns) { m_options.filters.push_back({ name, std::move(patterns) }); return *this; }
    FilePathField& set_dialog_title(std::string t) { m_options.title = std::move(t); return *this; }
    FilePathField& set_default_name(std::string n) { m_options.default_name = std::move(n); return *this; }
    FilePathField& set_must_exist(bool m) noexcept { m_must_exist = m; return *this; }
    FilePathField& on_change(std::function<void(const std::string&)> fn) { m_on_change = std::move(fn); return *this; }
    FilePathField& set_browse_handler(std::function<std::optional<std::string>(FileMode)> fn) { m_browser = std::move(fn); return *this; }
    TextField& field() noexcept { return *m_field; }
    Button& browse_button() noexcept { return *m_browse; }
    bool valid() const { return m_field->valid(); }
    bool hit_self(float, float) const noexcept override { return false; }

    void browse() {
        std::optional<std::string> result;
        if (m_browser) result = m_browser(m_mode);
        else {
            windows::dialogs::FileDialogOptions o = m_options;
            const std::string cur = paths().empty() ? std::string() : paths().front();
            if (!cur.empty()) {
                std::error_code ec;
                const auto p = system::paths::from_utf8(cur);
                o.default_path = std::filesystem::is_directory(p, ec) ? cur : system::paths::to_utf8(p.parent_path());
                if (m_mode == FileMode::Save && o.default_name.empty()) o.default_name = system::paths::to_utf8(p.filename());
            }
            if (ui() && ui()->window()) o.parent = ui()->window()->native_handle();
            switch (m_mode) {
                case FileMode::Open: result = windows::dialogs::open_file(o); break;
                case FileMode::Save: result = windows::dialogs::save_file(o); break;
                case FileMode::Folder: result = windows::dialogs::select_folder(o); break;
                case FileMode::OpenMultiple: {
                    const auto v = windows::dialogs::open_files(o);
                    if (!v.empty()) { std::string s; for (std::size_t i = 0; i < v.size(); ++i) { if (i) s += "; "; s += v[i]; } result = s; }
                    break;
                }
            }
        }
        if (result) set_path(*result, true);
    }

    bool on_files_dropped(const std::vector<std::string>& files) override {
        if (files.empty()) return false;
        if (m_mode == FileMode::OpenMultiple) { std::string s; for (std::size_t i = 0; i < files.size(); ++i) { if (i) s += "; "; s += files[i]; } set_path(s, true); }
        else if (m_mode == FileMode::Folder) {
            std::error_code ec;
            const auto p = system::paths::from_utf8(files.front());
            set_path(std::filesystem::is_directory(p, ec) ? files.front() : system::paths::to_utf8(p.parent_path()), true);
        } else set_path(files.front(), true);
        return true;
    }
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_CONTROLS_HPP
