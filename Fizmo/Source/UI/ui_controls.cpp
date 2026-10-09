#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "ui_controls.hpp"

namespace fizmo {
namespace ui {

auto Checkbox::set_checked(bool c, bool notify) -> Checkbox& { const bool ch = c != m_checked || m_indeterminate; m_checked = c; m_indeterminate = false; if (notify && ch && m_on_change) m_on_change(m_checked); return *this; }

auto Checkbox::measure(Ui&) -> Size {
        const Theme& t = theme();
        return Size{ t.check_size + (m_text.empty() ? 0.0f : 8.0f + text_width(m_text)), t.row_height };
    }

auto Checkbox::draw(Painter& p, Ui&) -> void {
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

auto Toggle::set_checked(bool c, bool notify) -> Toggle& { const bool ch = c != m_on; m_on = c; if (!ui()) m_anim = c ? 1.0f : 0.0f; if (notify && ch && m_on_change) m_on_change(m_on); return *this; }

auto Toggle::update(Ui&, double dt) -> void {
        const float target = m_on ? 1.0f : 0.0f;
        const float k = static_cast<float>(std::min(1.0, dt * 14.0));
        m_anim += (target - m_anim) * k;
        if (std::abs(target - m_anim) < 0.01f) m_anim = target;
    }

auto Toggle::draw(Painter& p, Ui&) -> void {
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

auto Toggle::on_key(const KeyEvent& k) -> bool { if (k.key == input::Key::Space || k.key == input::Key::Enter) { if (!k.repeat) toggle(); return true; } return false; }

auto RadioGroup::set_selected(int v, bool notify) -> RadioGroup& { const bool ch = v != m_selected; m_selected = v; if (notify && ch && m_on_change) m_on_change(v); return *this; }

RadioButton::RadioButton(std::string text, std::shared_ptr<RadioGroup> group, int value) : m_text(std::move(text)), m_group(std::move(group)), m_value(value) {
        m_focusable = true;
        if (!m_group) m_group = std::make_shared<RadioGroup>();
        m_group->m_buttons.push_back(this);
    }

auto RadioButton::measure(Ui&) -> Size {
        const Theme& t = theme();
        return Size{ t.check_size + (m_text.empty() ? 0.0f : 8.0f + text_width(m_text)), t.row_height };
    }

auto RadioButton::draw(Painter& p, Ui&) -> void {
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

auto RadioButton::on_key(const KeyEvent& k) -> bool {
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

RadioList::RadioList(const std::vector<std::string>& options, int selected, Orientation o) : Stack(o) {
        m_spacing = o == Orientation::Vertical ? 0.0f : 14.0f;
        m_cross = Align::Start;
        for (std::size_t i = 0; i < options.size(); ++i) add<RadioButton>(options[i], m_group, static_cast<int>(i));
        m_group->set_selected(selected);
    }

auto Slider::value_width() const -> float {
        if (!m_show_value) return 0.0f;
        return std::max(text_width(format(m_min)), text_width(format(m_max))) + 10.0f;
    }

auto Slider::track() const noexcept -> Rect {
        const float r = 8.0f;
        if (m_orientation == Orientation::Horizontal) return Rect(r, height() * 0.5f, std::max(0.0f, width() - 2.0f * r - value_width()), 0.0f);
        return Rect(width() * 0.5f, r, 0.0f, std::max(0.0f, height() - 2.0f * r - (m_show_value ? line_height() + 4.0f : 0.0f)));
    }

auto Slider::thumb_pos(double v) const noexcept -> Point {
        const Rect t = track();
        if (m_orientation == Orientation::Horizontal) return Point{ t.x + t.w * ratio(v), t.y };
        return Point{ t.x, t.bottom() - t.h * ratio(v) };
    }

auto Slider::value_at(float x, float y) const noexcept -> double {
        const Rect t = track();
        float r = m_orientation == Orientation::Horizontal ? (t.w > 0.0f ? (x - t.x) / t.w : 0.0f) : (t.h > 0.0f ? (t.bottom() - y) / t.h : 0.0f);
        r = std::max(0.0f, std::min(1.0f, r));
        return m_min + (m_max - m_min) * r;
    }

auto Slider::snap(double v) const noexcept -> double {
        if (m_step > 0.0) v = m_min + std::round((v - m_min) / m_step) * m_step;
        return std::max(m_min, std::min(m_max, v));
    }

Slider::Slider(double value, double min, double max, double step) : m_min(std::min(min, max)), m_max(std::max(min, max)), m_step(step) { m_focusable = true; m_value = snap(value); }

auto Slider::set_value(double v, bool notify) -> Slider& { const double s = snap(v); const bool ch = s != m_value; m_value = s; if (notify && ch && m_on_change) m_on_change(m_value); return *this; }

auto Slider::format(double v) const -> std::string {
        if (m_format) return m_format(v);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.*f", m_step >= 1.0 && std::floor(m_step) == m_step ? 0 : m_decimals, v);
        return buf;
    }

auto Slider::measure(Ui&) -> Size {
        const Theme& t = theme();
        if (m_orientation == Orientation::Horizontal) return Size{ 160.0f + value_width(), t.row_height };
        return Size{ std::max(t.row_height, m_show_value ? value_width() : 0.0f), 140.0f };
    }

auto Slider::draw(Painter& p, Ui&) -> void {
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

auto Slider::on_mouse_down(MouseEvent& e) -> bool {
        if (e.button != input::MouseButton::Left) return false;
        const Point th = thumb_pos(m_value);
        const float dx = e.x - th.x, dy = e.y - th.y;
        if (dx * dx + dy * dy <= 100.0f) m_grab = m_orientation == Orientation::Horizontal ? dx : dy;
        else { m_grab = 0.0f; set_live(value_at(e.x, e.y)); }
        m_dragging = true;
        return true;
    }

auto Slider::on_mouse_move(MouseEvent& e) -> void {
        if (!m_dragging || !pressed()) return;
        set_live(m_orientation == Orientation::Horizontal ? value_at(e.x - m_grab, e.y) : value_at(e.x, e.y - m_grab));
    }

auto Slider::nudge(double dir) -> void {
        const double st = m_step > 0.0 ? m_step : (m_max - m_min) / 100.0;
        set_live(m_value + dir * st);
        if (m_on_release) m_on_release(m_value);
    }

auto Slider::on_key(const KeyEvent& k) -> bool {
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

auto RangeSlider::x_of(double v) const noexcept -> float { const Rect t = track(); return t.x + t.w * (m_max > m_min ? static_cast<float>((v - m_min) / (m_max - m_min)) : 0.0f); }

auto RangeSlider::value_at(float x) const noexcept -> double { const Rect t = track(); const float r = t.w > 0.0f ? std::max(0.0f, std::min(1.0f, (x - t.x) / t.w)) : 0.0f; return m_min + (m_max - m_min) * r; }

auto RangeSlider::snap(double v) const noexcept -> double { if (m_step > 0.0) v = m_min + std::round((v - m_min) / m_step) * m_step; return std::max(m_min, std::min(m_max, v)); }

auto RangeSlider::fmt(double v) const -> std::string { char b[48]; std::snprintf(b, sizeof(b), "%.*f", m_step >= 1.0 && std::floor(m_step) == m_step ? 0 : m_decimals, v); return b; }

auto RangeSlider::move_active(double v) -> void {
        v = snap(v);
        double lo = m_low, hi = m_high;
        if (m_active == 0) lo = std::min(v, hi - m_min_gap);
        else hi = std::max(v, lo + m_min_gap);
        lo = std::max(m_min, lo); hi = std::min(m_max, hi);
        if (lo == m_low && hi == m_high) return;
        m_low = lo; m_high = hi;
        if (m_on_change) m_on_change(m_low, m_high);
    }

RangeSlider::RangeSlider(double low, double high, double min, double max, double step) : m_min(std::min(min, max)), m_max(std::max(min, max)), m_step(step) {
        m_focusable = true;
        set_values(low, high);
    }

auto RangeSlider::set_values(double lo, double hi, bool notify) -> RangeSlider& {
        if (lo > hi) std::swap(lo, hi);
        const double a = snap(lo), b = snap(hi);
        const bool ch = a != m_low || b != m_high;
        m_low = a; m_high = b;
        if (notify && ch && m_on_change) m_on_change(m_low, m_high);
        return *this;
    }

auto RangeSlider::draw(Painter& p, Ui&) -> void {
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

auto RangeSlider::on_mouse_down(MouseEvent& e) -> bool {
        if (e.button != input::MouseButton::Left) return false;
        const float a = x_of(m_low), b = x_of(m_high);
        if (std::abs(e.x - a) < std::abs(e.x - b) || (a == b && e.x < a)) m_active = 0; else m_active = 1;
        const float tx = m_active == 0 ? a : b;
        if (std::abs(e.x - tx) <= 10.0f) m_grab = e.x - tx; else { m_grab = 0.0f; move_active(value_at(e.x)); }
        m_dragging = true;
        return true;
    }

auto RangeSlider::on_key(const KeyEvent& k) -> bool {
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

auto ProgressBar::label() const -> std::string {
        if (!m_text.empty()) return m_text;
        if (m_indeterminate) return std::string();
        if (m_format) return m_format(m_value);
        return std::to_string(static_cast<int>(std::floor(m_value * 100.0 + 1e-9))) + "%";
    }

auto ProgressBar::draw(Painter& p, Ui& ui) -> void {
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

auto BusySpinner::draw(Painter& p, Ui& ui) -> void {
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

auto DropdownPopup::ensure_visible(int item_pos) -> void {
        const Rect lr = list_rect();
        const float y = item_pos * row_h();
        if (y < m_scroll) m_scroll = y;
        if (y + row_h() > m_scroll + lr.h) m_scroll = y + row_h() - lr.h;
        m_scroll = std::max(0.0f, std::min(m_scroll, std::max(0.0f, content_h() - lr.h)));
    }

auto DropdownPopup::arrange(Ui& ui) -> void {
        if (m_search && m_search->visible()) { m_search->set_rect(Rect(4.0f, 4.0f, width() - 8.0f, theme().row_height)); m_search->arrange(ui); }
        if (m_pending_reveal >= 0 && height() > 0.0f) { ensure_visible(m_pending_reveal); m_pending_reveal = -1; }
    }

auto DropdownPopup::on_wheel(MouseEvent& e) -> bool {
        const Rect lr = list_rect();
        m_scroll = std::max(0.0f, std::min(m_scroll - e.wheel_y * row_h() * 3.0f, std::max(0.0f, content_h() - lr.h)));
        return true;
    }

auto Dropdown::find_prefix(const std::string& prefix, int from) const -> int {
        auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
        const std::string p = lower(prefix);
        const int n = static_cast<int>(m_items.size());
        for (int k = 0; k < n; ++k) {
            const int i = (from + k) % n;
            if (lower(m_items[static_cast<std::size_t>(i)]).compare(0, p.size(), p) == 0) return i;
        }
        return -1;
    }

auto Dropdown::key_char(input::Key k) noexcept -> char {
        if (input::is_letter(k)) return static_cast<char>('a' + (static_cast<int>(k) - static_cast<int>(input::Key::A)));
        if (input::is_digit(k)) return static_cast<char>('0' + (static_cast<int>(k) - static_cast<int>(input::Key::Num0)));
        if (k == input::Key::Space) return ' ';
        return 0;
    }

Dropdown::Dropdown(std::vector<std::string> items, int selected) : m_items(std::move(items)) {
        m_focusable = true;
        m_selected = selected >= 0 && selected < static_cast<int>(m_items.size()) ? selected : -1;
        m_popup = std::make_unique<DropdownPopup>(this);
    }

auto Dropdown::set_items(std::vector<std::string> items) -> Dropdown& { m_items = std::move(items); if (m_selected >= static_cast<int>(m_items.size())) m_selected = -1; if (is_open()) m_popup->refresh(); return *this; }

auto Dropdown::set_selected(int i, bool notify) -> Dropdown& {
        if (i < -1 || i >= static_cast<int>(m_items.size())) i = -1;
        const bool ch = i != m_selected;
        m_selected = i;
        if (notify && ch && m_on_change) m_on_change(i);
        return *this;
    }

auto Dropdown::open() -> void {
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

auto Dropdown::measure(Ui&) -> Size {
        const Theme& t = theme();
        float w = text_width(m_placeholder);
        for (const std::string& s : m_items) w = std::max(w, text_width(s));
        return Size{ std::max(120.0f, w + t.padding * 2.0f + 22.0f), t.row_height };
    }

auto Dropdown::draw(Painter& p, Ui&) -> void {
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

auto Dropdown::on_mouse_down(MouseEvent& e) -> bool {
        if (e.button != input::MouseButton::Left) return false;
        if (is_open()) close(); else open();
        return true;
    }

auto Dropdown::on_key(const KeyEvent& k) -> bool {
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

DropdownPopup::DropdownPopup(Dropdown* owner) : m_owner(owner) {
    m_search = &add<TextField>("", "Search...");
    m_search->set_visible(false);
    m_search->on_change([this](const std::string&) {
        refresh();
        m_owner->m_highlight = m_items.empty() ? -1 : m_items.front();
        m_scroll = 0.0f;
    });
    m_search->on_submit([this](const std::string&) { if (m_owner->m_highlight >= 0) m_owner->choose(m_owner->m_highlight); });
}

void DropdownPopup::refresh() {
    m_items.clear();
    const std::string q = m_search && m_search->visible() ? m_search->text() : std::string();
    auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    const std::string lq = lower(q);
    for (std::size_t i = 0; i < m_owner->m_items.size(); ++i) {
        if (lq.empty() || lower(m_owner->m_items[i]).find(lq) != std::string::npos) m_items.push_back(static_cast<int>(i));
    }
}

Size DropdownPopup::measure(Ui&) {
    const Theme& t = theme();
    float w = m_owner->width();
    for (int i : m_items) w = std::max(w, text_width(m_owner->m_items[static_cast<std::size_t>(i)]) + t.padding * 2.0f + 16.0f);
    const int rows = std::max(1, std::min(m_owner->m_max_visible, static_cast<int>(m_owner->m_items.size())));
    return Size{ w, list_top() + rows * row_h() + 4.0f };
}

void DropdownPopup::draw(Painter& p, Ui& ui) {
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

bool DropdownPopup::on_mouse_down(MouseEvent& e) {
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

void DropdownPopup::on_mouse_move(MouseEvent& e) {
    const Rect lr = list_rect();
    m_hover = -1;
    if (!lr.contains(e.x, e.y)) return;
    const int k = static_cast<int>((e.y - lr.y + m_scroll) / row_h());
    if (k >= 0 && k < static_cast<int>(m_items.size())) m_hover = m_items[static_cast<std::size_t>(k)];
}

bool DropdownPopup::on_key(const KeyEvent& k) {
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

auto TabView::tab_at(float x, float y) const noexcept -> int {
        for (std::size_t i = 0; i < m_tabs.size(); ++i) if (m_tabs[i].header.contains(x, y)) return static_cast<int>(i);
        return -1;
    }

auto TabView::layout_headers() -> void {
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

auto TabView::add_tab(const std::string& title, std::unique_ptr<Widget> page) -> Widget& {
        Widget& ref = adopt(std::move(page));
        m_tabs.push_back(Tab{ title, &ref, Rect() });
        if (m_selected < 0) m_selected = 0;
        ref.set_visible(static_cast<int>(m_tabs.size()) - 1 == m_selected);
        return ref;
    }

auto TabView::remove_tab(int i) -> bool {
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

auto TabView::select(int i, bool notify) -> TabView& {
        if (i < 0 || i >= static_cast<int>(m_tabs.size()) || i == m_selected) return *this;
        m_selected = i;
        for (std::size_t k = 0; k < m_tabs.size(); ++k) m_tabs[k].page->set_visible(static_cast<int>(k) == i);
        if (notify && m_on_change) m_on_change(i);
        return *this;
    }

auto TabView::page(int i) const noexcept -> Widget* { return i >= 0 && i < static_cast<int>(m_tabs.size()) ? m_tabs[static_cast<std::size_t>(i)].page : nullptr; }

auto TabView::page_rect() const noexcept -> Rect {
        const float pad = m_page_padding ? theme().padding : 0.0f;
        return Rect(pad, header_h() + pad, std::max(0.0f, width() - pad * 2.0f), std::max(0.0f, height() - header_h() - pad * 2.0f));
    }

auto TabView::measure(Ui& ui) -> Size {
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

auto TabView::arrange(Ui& ui) -> void {
        layout_headers();
        const Rect pr = page_rect();
        for (const Tab& tab : m_tabs) {
            tab.page->set_rect(pr);
            if (tab.page->visible()) tab.page->arrange(ui);
        }
    }

auto TabView::draw(Painter& p, Ui& ui) -> void {
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

auto TabView::on_mouse_down(MouseEvent& e) -> bool {
        if (e.y >= header_h()) return false;
        const int i = tab_at(e.x, e.y);
        if (i < 0) return true;
        if ((m_closable && close_rect(m_tabs[static_cast<std::size_t>(i)]).contains(e.x, e.y) && e.button == input::MouseButton::Left) || (m_closable && e.button == input::MouseButton::Middle)) { request_close(i); return true; }
        if (e.button == input::MouseButton::Left) select(i);
        return true;
    }

auto TabView::on_key(const KeyEvent& k) -> bool {
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

auto ScrollView::vthumb() const noexcept -> Rect {
        const float th = m_ch > 0.0f ? std::max(20.0f, m_vh * m_vh / m_ch) : m_vh;
        const float ty = max_y() > 0.0f ? (m_vh - th) * (m_sy / max_y()) : 0.0f;
        return Rect(m_vw + 2.0f, ty + 2.0f, sb() - 4.0f, std::max(0.0f, th - 4.0f));
    }

auto ScrollView::hthumb() const noexcept -> Rect {
        const float tw = m_cw > 0.0f ? std::max(20.0f, m_vw * m_vw / m_cw) : m_vw;
        const float tx = max_x() > 0.0f ? (m_vw - tw) * (m_sx / max_x()) : 0.0f;
        return Rect(tx + 2.0f, m_vh + 2.0f, std::max(0.0f, tw - 4.0f), sb() - 4.0f);
    }

auto ScrollView::clamp_targets() noexcept -> void {
        m_tx = std::max(0.0f, std::min(m_tx, max_x()));
        m_ty = std::max(0.0f, std::min(m_ty, max_y()));
        m_sx = std::max(0.0f, std::min(m_sx, max_x()));
        m_sy = std::max(0.0f, std::min(m_sy, max_y()));
    }

auto ScrollView::reveal(const Rect& screen) -> void {
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

auto ScrollView::measure(Ui& ui) -> Size {
        Widget* c = content();
        if (!c) return Size{ 100.0f, 60.0f };
        const Size s = c->preferred(ui);
        return Size{ s.w + (m_vertical ? sb() : 0.0f), std::min(s.h, m_pref_h) };
    }

auto ScrollView::arrange(Ui& ui) -> void {
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

auto ScrollView::update(Ui&, double dt) -> void {
        if (!m_smooth) { m_sx = m_tx; m_sy = m_ty; return; }
        const float k = static_cast<float>(std::min(1.0, dt * 16.0));
        m_sx += (m_tx - m_sx) * k;
        m_sy += (m_ty - m_sy) * k;
        if (std::abs(m_tx - m_sx) < 0.5f) m_sx = m_tx;
        if (std::abs(m_ty - m_sy) < 0.5f) m_sy = m_ty;
    }

auto ScrollView::hit_test(float x, float y) -> Widget* {
        if (!visible() || !bounds().contains(x, y)) return nullptr;
        if ((m_show_v && vbar().contains(x, y)) || (m_show_h && hbar().contains(x, y))) return this;
        return Widget::hit_test(x, y);
    }

auto ScrollView::draw(Painter& p, Ui& ui) -> void {
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

auto ScrollView::on_mouse_down(MouseEvent& e) -> bool {
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

auto ScrollView::on_mouse_move(MouseEvent& e) -> void {
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

auto ScrollView::on_wheel(MouseEvent& e) -> bool {
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

auto ScrollView::on_key(const KeyEvent& k) -> bool {
        using K = input::Key;
        switch (k.key) {
            case K::PageDown: scroll_by(0.0f, m_vh * 0.9f); return max_y() > 0.0f;
            case K::PageUp: scroll_by(0.0f, -m_vh * 0.9f); return max_y() > 0.0f;
            default: return false;
        }
    }

Modal::Modal(std::string title) : m_title(std::move(title)) {
        m_body = &add<VStack>();
        m_buttons = &add<HStack>();
        m_buttons->set_main_align(Align::End);
        m_buttons->set_cross_align(Align::Center);
    }

auto Modal::button(int i) const noexcept -> Button* { return i >= 0 && i < static_cast<int>(m_button_list.size()) ? m_button_list[static_cast<std::size_t>(i)] : nullptr; }

auto Modal::add_button(std::string text, ButtonStyle style) -> Button& {
        const int index = static_cast<int>(m_button_list.size());
        Button& b = m_buttons->add<Button>(std::move(text));
        b.set_style(style);
        b.on_click([this, index]() { close(index); });
        m_button_list.push_back(&b);
        return b;
    }

auto Modal::close(int result) -> void {
        if (m_closing) return;
        m_closing = true;
        Ui* u = ui();
        auto cb = m_on_result;
        if (!u) { if (cb) cb(result); return; }
        u->defer([u, this, cb, result]() { u->close_modal(*this); if (cb) cb(result); });
    }

auto Modal::measure(Ui& ui) -> Size {
        const float p = pad();
        const Size b = m_body->preferred(ui);
        const Size r = m_buttons->preferred(ui);
        const float tw = m_title.empty() ? 0.0f : text_width(m_title, theme().font_size * 1.1, true) + p * 2.0f;
        const float w = std::min(m_max_w, std::max({ m_min_w, b.w + p * 2.0f, r.w + p * 2.0f, tw }));
        return Size{ w, title_h() + b.h + (m_button_list.empty() ? 0.0f : r.h + p) + p * 2.0f };
    }

auto Modal::arrange(Ui& ui) -> void {
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

auto Modal::draw(Painter& p, Ui& ui) -> void {
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

auto Modal::on_key(const KeyEvent& k) -> bool {
        using K = input::Key;
        if (k.key == K::Escape) { close(m_cancel); return true; }
        if ((k.key == K::Enter || k.key == K::NumpadEnter) && m_default >= 0) { close(m_default); return true; }
        return false;
    }

void Ui::alert(const std::string& title, const std::string& message, std::function<void()> on_close) {
    auto m = std::make_unique<Modal>(title);
    m->body().add<Label>(message);
    m->add_button("OK", ButtonStyle::Primary);
    m->set_default_button(0);
    m->set_cancel_result(0);
    m->on_result([cb = std::move(on_close)](int) { if (cb) cb(); });
    show_modal(std::move(m));
}

void Ui::confirm(const std::string& title, const std::string& message, std::function<void(bool)> on_result, const std::string& yes, const std::string& no) {
    auto m = std::make_unique<Modal>(title);
    m->body().add<Label>(message);
    m->add_button(no);
    m->add_button(yes, ButtonStyle::Primary);
    m->set_default_button(1);
    m->set_cancel_result(0);
    m->on_result([cb = std::move(on_result)](int r) { if (cb) cb(r == 1); });
    show_modal(std::move(m));
}

void Ui::prompt(const std::string& title, const std::string& message, const std::string& initial, std::function<void(std::optional<std::string>)> on_result) {
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

auto Keybind::operator==(const Keybind& o) const noexcept -> bool {
        if (kind != o.kind) return false;
        switch (kind) {
            case Kind::Key: return key == o.key && mods == o.mods;
            case Kind::Mouse: return mouse == o.mouse && mods == o.mods;
            case Kind::Gamepad: return pad == o.pad;
            default: return true;
        }
    }

auto Keybind::mouse_name(input::MouseButton b) noexcept -> const char* {
        switch (b) {
            case input::MouseButton::Left: return "Mouse Left";
            case input::MouseButton::Right: return "Mouse Right";
            case input::MouseButton::Middle: return "Mouse Middle";
            case input::MouseButton::X1: return "Mouse 4";
            case input::MouseButton::X2: return "Mouse 5";
            default: return "Mouse";
        }
    }

auto Keybind::mod_prefix() const -> std::string {
        std::string s;
        if (input::has(mods, input::Modifiers::Control)) s += "Ctrl+";
        if (input::has(mods, input::Modifiers::Shift)) s += "Shift+";
        if (input::has(mods, input::Modifiers::Alt)) s += "Alt+";
        if (input::has(mods, input::Modifiers::Meta)) s += "Meta+";
        return s;
    }

auto Keybind::to_string() const -> std::string {
        switch (kind) {
            case Kind::Key: return mod_prefix() + input::key_name(key);
            case Kind::Mouse: return mod_prefix() + mouse_name(mouse);
            case Kind::Gamepad: return std::string("Gamepad ") + input::gamepad_button_name(pad);
            default: return std::string();
        }
    }

auto Keybind::parse(const std::string& text) -> std::optional<Keybind> {
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

auto Keybind::matches(const windows::WindowEvent& e) const noexcept -> bool {
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

auto KeybindField::mod_of(input::Key k) noexcept -> input::Modifiers {
        using K = input::Key;
        switch (k) {
            case K::LeftShift: case K::RightShift: return input::Modifiers::Shift;
            case K::LeftControl: case K::RightControl: return input::Modifiers::Control;
            case K::LeftAlt: case K::RightAlt: return input::Modifiers::Alt;
            case K::LeftMeta: case K::RightMeta: return input::Modifiers::Meta;
            default: return input::Modifiers::None;
        }
    }

auto KeybindField::without(input::Modifiers a, input::Modifiers b) noexcept -> input::Modifiers { return static_cast<input::Modifiers>(static_cast<std::uint16_t>(a) & ~static_cast<std::uint16_t>(b)); }

auto KeybindField::commit(const Keybind& b) -> void {
        if (m_validator && !m_validator(b)) { if (ui()) m_rejected_at = ui()->time(); return; }
        m_listening = false;
        m_pending = input::Modifiers::None;
        const bool ch = b != m_bind;
        m_bind = b;
        if (ch && m_on_change) m_on_change(m_bind);
    }

auto KeybindField::set_value(const Keybind& b, bool notify) -> KeybindField& { const bool ch = b != m_bind; m_bind = b; if (notify && ch && m_on_change) m_on_change(m_bind); return *this; }

auto KeybindField::start_listening() -> void { if (!enabled()) return; m_listening = true; m_pending = input::Modifiers::None; m_last_mod = input::Key::Unknown; focus(); }

auto KeybindField::measure(Ui&) -> Size {
        const Theme& t = theme();
        return Size{ std::max(130.0f, std::max(text_width(m_prompt), text_width(m_bind.to_string())) + t.padding * 2.0f), t.row_height };
    }

auto KeybindField::draw(Painter& p, Ui& ui) -> void {
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

auto KeybindField::on_mouse_down(MouseEvent& e) -> bool {
        if (!m_listening) {
            if (e.button == input::MouseButton::Left) { start_listening(); return true; }
            if (e.button == input::MouseButton::Right && m_right_click_clears) { clear(true); return true; }
            return false;
        }
        if (m_allow_mouse && e.button != input::MouseButton::None) commit(Keybind::from_mouse(e.button, e.mods));
        else cancel();
        return true;
    }

auto KeybindField::on_key(const KeyEvent& k) -> bool {
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

auto KeybindField::on_key_up(const KeyEvent& k) -> bool {
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

auto KeybindField::on_gamepad_button(input::GamepadButton b) -> bool {
        if (!m_listening || !m_allow_gamepad) return false;
        commit(Keybind::from_gamepad(b));
        return true;
    }

auto FilePathField::placeholder() -> void {
        switch (m_mode) {
            case FileMode::Folder: m_field->set_placeholder("Choose a folder..."); break;
            case FileMode::Save: m_field->set_placeholder("Choose where to save..."); break;
            case FileMode::OpenMultiple: m_field->set_placeholder("Choose files..."); break;
            default: m_field->set_placeholder("Choose a file..."); break;
        }
    }

FilePathField::FilePathField(const std::string& path, FileMode mode) : HStack(4.0f), m_mode(mode) {
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

auto FilePathField::split(const std::string& s) -> std::vector<std::string> {
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

auto FilePathField::browse() -> void {
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

auto FilePathField::on_files_dropped(const std::vector<std::string>& files) -> bool {
        if (files.empty()) return false;
        if (m_mode == FileMode::OpenMultiple) { std::string s; for (std::size_t i = 0; i < files.size(); ++i) { if (i) s += "; "; s += files[i]; } set_path(s, true); }
        else if (m_mode == FileMode::Folder) {
            std::error_code ec;
            const auto p = system::paths::from_utf8(files.front());
            set_path(std::filesystem::is_directory(p, ec) ? files.front() : system::paths::to_utf8(p.parent_path()), true);
        } else set_path(files.front(), true);
        return true;
    }

} // namespace ui
} // namespace fizmo
