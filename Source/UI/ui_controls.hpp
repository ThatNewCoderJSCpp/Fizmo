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
    Checkbox& set_checked(bool c, bool notify = false);
    Checkbox& set_indeterminate(bool i = true) noexcept { m_indeterminate = i; return *this; }
    Checkbox& set_text(std::string t) { m_text = std::move(t); return *this; }
    const std::string& text() const noexcept { return m_text; }
    Checkbox& on_change(std::function<void(bool)> fn) { m_on_change = std::move(fn); return *this; }
    void toggle() { if (enabled()) set_checked(m_indeterminate ? true : !m_checked, true); }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;

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
    Toggle& set_checked(bool c, bool notify = false);
    Toggle& set_text(std::string t) { m_text = std::move(t); return *this; }
    Toggle& on_change(std::function<void(bool)> fn) { m_on_change = std::move(fn); return *this; }
    void toggle() { if (enabled()) set_checked(!m_on, true); }
    float animation() const noexcept { return m_anim; }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override { return Size{ 34.0f + (m_text.empty() ? 0.0f : 10.0f + text_width(m_text)), theme().row_height }; }

    void update(Ui&, double dt) override;

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) toggle(); m_armed = false; }
    bool on_key(const KeyEvent& k) override;
};

class RadioButton;

class RadioGroup {
    friend class RadioButton;
    int                       m_selected = -1;
    std::function<void(int)>  m_on_change;
    std::vector<RadioButton*> m_buttons;

public:
    int selected() const noexcept { return m_selected; }
    RadioGroup& set_selected(int v, bool notify = false);
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
    RadioButton(std::string text, std::shared_ptr<RadioGroup> group, int value);

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

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) select(); m_armed = false; }

    bool on_key(const KeyEvent& k) override;
};

class RadioList : public Stack {
    std::shared_ptr<RadioGroup> m_group = std::make_shared<RadioGroup>();

public:
    explicit RadioList(const std::vector<std::string>& options = {}, int selected = 0, Orientation o = Orientation::Vertical);

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

    float value_width() const;

    Rect track() const noexcept;

    float ratio(double v) const noexcept { return m_max > m_min ? static_cast<float>((v - m_min) / (m_max - m_min)) : 0.0f; }

    Point thumb_pos(double v) const noexcept;

    double value_at(float x, float y) const noexcept;

    double snap(double v) const noexcept;

    void set_live(double v) {
        v = snap(v);
        if (v == m_value) return;
        m_value = v;
        if (m_on_change) m_on_change(m_value);
    }

public:
    Slider(double value = 0.0, double min = 0.0, double max = 1.0, double step = 0.0);

    double value() const noexcept { return m_value; }
    Slider& set_value(double v, bool notify = false);
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

    std::string format(double v) const;

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override;

    void on_mouse_move(MouseEvent& e) override;

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

    void nudge(double dir);

    bool on_key(const KeyEvent& k) override;
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
    float x_of(double v) const noexcept;
    double value_at(float x) const noexcept;
    double snap(double v) const noexcept;

    std::string fmt(double v) const;
    float value_width() const { return m_show_value ? text_width(fmt(m_max) + " - " + fmt(m_max)) + 10.0f : 0.0f; }

    void move_active(double v);

public:
    RangeSlider(double low = 0.25, double high = 0.75, double min = 0.0, double max = 1.0, double step = 0.0);

    double low() const noexcept { return m_low; }
    double high() const noexcept { return m_high; }
    RangeSlider& set_values(double lo, double hi, bool notify = false);
    RangeSlider& set_range(double min, double max) { m_min = std::min(min, max); m_max = std::max(min, max); return set_values(m_low, m_high); }
    RangeSlider& set_step(double s) noexcept { m_step = s; return *this; }
    RangeSlider& set_min_gap(double g) noexcept { m_min_gap = std::max(0.0, g); return *this; }
    RangeSlider& set_show_value(bool s, int decimals = 2) noexcept { m_show_value = s; m_decimals = decimals; return *this; }
    RangeSlider& on_change(std::function<void(double, double)> fn) { m_on_change = std::move(fn); return *this; }
    RangeSlider& on_release(std::function<void(double, double)> fn) { m_on_release = std::move(fn); return *this; }
    int active_thumb() const noexcept { return m_active; }

    Size measure(Ui&) override { return Size{ 180.0f + value_width(), theme().row_height }; }

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override;

    void on_mouse_move(MouseEvent& e) override { if (m_dragging && pressed()) move_active(value_at(e.x - m_grab)); }
    void on_mouse_up(MouseEvent&) override { if (m_dragging) { m_dragging = false; if (m_on_release) m_on_release(m_low, m_high); } }

    bool on_key(const KeyEvent& k) override;
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

    std::string label() const;

    Size measure(Ui&) override { return Size{ 180.0f, std::max(18.0f, theme().row_height * 0.75f) }; }

    void draw(Painter& p, Ui& ui) override;
};

class BusySpinner : public Widget {
    float m_size;
    Color m_color{ 0, 0, 0, 0 };

public:
    explicit BusySpinner(float size = 20.0f) : m_size(size) {}
    BusySpinner& set_color(const Color& c) noexcept { m_color = c; return *this; }
    bool hit_self(float, float) const noexcept override { return !m_tooltip.empty(); }
    Size measure(Ui&) override { return Size{ m_size, m_size }; }

    void draw(Painter& p, Ui& ui) override;
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
    void ensure_visible(int item_pos);

    const std::vector<int>& items() const noexcept { return m_items; }
    TextField* search() const noexcept { return m_search; }

    Size measure(Ui& ui) override;
    void arrange(Ui& ui) override;
    void draw(Painter& p, Ui& ui) override;
    bool on_mouse_down(MouseEvent& e) override;
    void on_mouse_move(MouseEvent& e) override;
    void on_mouse_up(MouseEvent&) override {}
    bool on_wheel(MouseEvent& e) override;
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

    int find_prefix(const std::string& prefix, int from) const;

    static char key_char(input::Key k) noexcept;

public:
    explicit Dropdown(std::vector<std::string> items = {}, int selected = -1);

    ~Dropdown() override { if (ui() && m_popup) ui()->close_popup(*m_popup); }

    const std::vector<std::string>& items() const noexcept { return m_items; }
    Dropdown& set_items(std::vector<std::string> items);
    Dropdown& add_item(std::string item) { m_items.push_back(std::move(item)); return *this; }
    int selected() const noexcept { return m_selected; }
    std::string selected_text() const { return m_selected >= 0 ? m_items[static_cast<std::size_t>(m_selected)] : std::string(); }
    Dropdown& set_selected(int i, bool notify = false);
    Dropdown& set_placeholder(std::string p) { m_placeholder = std::move(p); return *this; }
    Dropdown& set_max_visible(int n) noexcept { m_max_visible = std::max(1, n); return *this; }
    Dropdown& set_searchable(bool s) noexcept { m_searchable = s; return *this; }
    Dropdown& on_change(std::function<void(int)> fn) { m_on_change = std::move(fn); return *this; }
    bool is_open() const noexcept { return ui() && m_popup && ui()->popup_open(*m_popup); }
    DropdownPopup& popup() noexcept { return *m_popup; }
    int highlight() const noexcept { return m_highlight; }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    void open();

    void close() { if (ui() && m_popup) ui()->close_popup(*m_popup); }

    void choose(int i) {
        close();
        set_selected(i, true);
    }

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override;

    bool on_key(const KeyEvent& k) override;
};















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

    int tab_at(float x, float y) const noexcept;

    void layout_headers();

public:
    TabView() { m_focusable = true; }

    Widget& add_tab(const std::string& title, std::unique_ptr<Widget> page);

    template <typename T, typename... Args>
    T& add_tab(const std::string& title, Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        add_tab(title, std::move(p));
        return ref;
    }

    bool remove_tab(int i);

    TabView& select(int i, bool notify = true);

    int selected() const noexcept { return m_selected; }
    int tab_count() const noexcept { return static_cast<int>(m_tabs.size()); }
    Widget* page(int i) const noexcept;
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

    Rect page_rect() const noexcept;

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;

    void draw(Painter& p, Ui& ui) override;

    bool hit_self(float, float y) const noexcept override { return y < header_h(); }

    bool on_mouse_down(MouseEvent& e) override;

    bool on_key(const KeyEvent& k) override;
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

    Rect vthumb() const noexcept;

    Rect hthumb() const noexcept;

    void clamp_targets() noexcept;

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

    void reveal(const Rect& screen) override;

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;

    void update(Ui&, double dt) override;

    Widget* hit_test(float x, float y) override;

    void draw(Painter& p, Ui& ui) override;

    bool on_mouse_down(MouseEvent& e) override;

    void on_mouse_move(MouseEvent& e) override;

    void on_mouse_up(MouseEvent&) override { m_drag = 0; }

    bool on_wheel(MouseEvent& e) override;

    bool on_key(const KeyEvent& k) override;
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
    explicit Modal(std::string title = "");

    VStack& body() noexcept { return *m_body; }
    HStack& button_row() noexcept { return *m_buttons; }
    const std::string& title() const noexcept { return m_title; }
    Modal& set_title(std::string t) { m_title = std::move(t); return *this; }
    Modal& set_width_range(float min_w, float max_w) noexcept { m_min_w = min_w; m_max_w = std::max(min_w, max_w); return *this; }
    Modal& set_default_button(int i) noexcept { m_default = i; return *this; }
    Modal& set_cancel_result(int i) noexcept { m_cancel = i; return *this; }
    Modal& on_result(std::function<void(int)> fn) { m_on_result = std::move(fn); return *this; }
    Button* button(int i) const noexcept;
    bool closing() const noexcept { return m_closing; }

    Button& add_button(std::string text, ButtonStyle style = ButtonStyle::Normal);

    void close(int result);

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;

    void draw(Painter& p, Ui& ui) override;

    bool on_key(const KeyEvent& k) override;
};







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

    bool operator==(const Keybind& o) const noexcept;
    bool operator!=(const Keybind& o) const noexcept { return !(*this == o); }

    static const char* mouse_name(input::MouseButton b) noexcept;

    std::string mod_prefix() const;

    std::string to_string() const;

    static std::optional<Keybind> parse(const std::string& text);

    bool matches(const windows::WindowEvent& e) const noexcept;
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

    static input::Modifiers mod_of(input::Key k) noexcept;

    static input::Modifiers without(input::Modifiers a, input::Modifiers b) noexcept;

    void commit(const Keybind& b);

public:
    explicit KeybindField(const Keybind& bind = Keybind()) : m_bind(bind) { m_focusable = true; }

    const Keybind& value() const noexcept { return m_bind; }
    KeybindField& set_value(const Keybind& b, bool notify = false);
    KeybindField& clear(bool notify = true) { return set_value(Keybind(), notify); }
    bool listening() const noexcept { return m_listening; }
    void start_listening();
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

    Size measure(Ui&) override;

    void draw(Painter& p, Ui& ui) override;

    bool on_mouse_down(MouseEvent& e) override;

    void on_focus_changed(bool f) override { if (!f) cancel(); }

    bool on_key(const KeyEvent& k) override;

    bool on_key_up(const KeyEvent& k) override;

    bool on_gamepad_button(input::GamepadButton b) override;
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

    void placeholder();

public:
    explicit FilePathField(const std::string& path = "", FileMode mode = FileMode::Open);

    static std::vector<std::string> split(const std::string& s);

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

    void browse();

    bool on_files_dropped(const std::vector<std::string>& files) override;
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_CONTROLS_HPP
