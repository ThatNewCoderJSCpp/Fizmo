#ifndef FIZMO_UI_COLOR_HPP
#define FIZMO_UI_COLOR_HPP

#include "ui_controls.hpp"
#include "../Graphics/texture.hpp"
#include "../Images/Bitmap/image.hpp"

namespace fizmo {
namespace ui {

struct Hsv {
    double h = 0.0, s = 0.0, v = 0.0, a = 1.0;
};

Color hsv_to_color(const Hsv& c) noexcept;

Hsv color_to_hsv(const Color& c, double keep_hue = 0.0) noexcept;

std::string color_to_hex(const Color& c, bool alpha);

std::optional<Color> color_from_hex(std::string s);

const graphics::Texture& color_wheel_texture();

class ColorPicker : public Widget {
protected:
    Hsv                               m_hsv{ 0.0, 0.0, 1.0, 1.0 };
    Color                             m_color{ 255, 255, 255 };
    Color                             m_original{ 255, 255, 255 };
    bool                              m_alpha = true;
    bool                              m_show_hsv = false;
    bool                              m_popup_style = false;
    int                               m_drag = 0;
    float                             m_wheel = 168.0f;
    VStack*                           m_rows = nullptr;
    TextField*                        m_hex = nullptr;
    std::array<IntField*, 4>          m_rgba{};
    std::array<IntField*, 3>          m_hsv_fields{};
    HStack*                           m_hsv_row = nullptr;
    bool                              m_syncing = false;
    std::function<void(const Color&)> m_on_change;
    std::function<void()>             m_on_cancel;

    float pad() const noexcept { return 10.0f; }
    float bar_w() const noexcept { return 18.0f; }
    Rect wheel_rect() const noexcept { return Rect(pad(), pad(), m_wheel, m_wheel); }
    Rect value_rect() const noexcept { return Rect(pad() + m_wheel + 12.0f, pad(), bar_w(), m_wheel); }
    Rect alpha_rect() const noexcept { return Rect(value_rect().right() + 8.0f, pad(), bar_w(), m_wheel); }
    float top_width() const noexcept { return (m_alpha ? alpha_rect().right() : value_rect().right()) + pad(); }

    void sync_fields();

    void from_hsv(bool notify);

    void from_color(const Color& c, bool notify);

    void pick_wheel(float x, float y);

    void pick_value(float y);
    void pick_alpha(float y);

public:
    explicit ColorPicker(const Color& c = Color(255, 255, 255), bool alpha = true);

    const Color& color() const noexcept { return m_color; }
    const Hsv& hsv() const noexcept { return m_hsv; }
    const Color& original() const noexcept { return m_original; }
    ColorPicker& set_color(const Color& c, bool notify = false) { from_color(c, notify); return *this; }
    ColorPicker& set_hsv(const Hsv& h, bool notify = false) { m_hsv = h; from_hsv(notify); return *this; }
    ColorPicker& set_original(const Color& c) noexcept { m_original = c; return *this; }
    ColorPicker& set_alpha_enabled(bool a) {
        m_alpha = a;
        m_rgba[3]->set_visible(a);
        if (!a) { m_hsv.a = 1.0; from_hsv(false); }
        return *this;
    }
    bool alpha_enabled() const noexcept { return m_alpha; }
    ColorPicker& set_show_hsv(bool s) { m_show_hsv = s; m_hsv_row->set_visible(s); return *this; }
    ColorPicker& set_wheel_size(float s) noexcept { m_wheel = std::max(60.0f, s); return *this; }
    ColorPicker& set_popup_style(bool p) noexcept { m_popup_style = p; return *this; }
    ColorPicker& on_change(std::function<void(const Color&)> fn) { m_on_change = std::move(fn); return *this; }
    ColorPicker& on_cancel(std::function<void()> fn) { m_on_cancel = std::move(fn); return *this; }
    TextField& hex_field() noexcept { return *m_hex; }
    IntField& channel_field(int i) noexcept { return *m_rgba.at(static_cast<std::size_t>(i)); }
    IntField& hsv_field(int i) noexcept { return *m_hsv_fields.at(static_cast<std::size_t>(i)); }
    Rect wheel_area() const noexcept { return wheel_rect(); }
    Rect value_area() const noexcept { return value_rect(); }
    Rect alpha_area() const noexcept { return alpha_rect(); }

    Point marker() const noexcept;

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;

    void draw(Painter& p, Ui& ui) override;

    bool on_mouse_down(MouseEvent& e) override;

    void on_mouse_move(MouseEvent& e) override;

    void on_mouse_up(MouseEvent&) override { m_drag = 0; }

    bool on_key(const KeyEvent& k) override {
        if (k.key == input::Key::Escape && m_on_cancel) { m_on_cancel(); return true; }
        return false;
    }

    windows::SystemCursor cursor(float x, float y) const noexcept override;
};

class ColorField : public Widget {
protected:
    Color                              m_color;
    bool                               m_alpha = true;
    bool                               m_show_hex = true;
    std::unique_ptr<ColorPicker>       m_picker;
    std::function<void(const Color&)>  m_on_change;
    bool                               m_armed = false;

    Rect swatch_rect() const noexcept;

public:
    explicit ColorField(const Color& c = Color(255, 255, 255), bool alpha = true);

    ~ColorField() override { if (ui() && m_picker) ui()->close_popup(*m_picker); }

    const Color& color() const noexcept { return m_color; }
    ColorField& set_color(const Color& c, bool notify = false);
    ColorField& set_alpha_enabled(bool a) { m_alpha = a; m_picker->set_alpha_enabled(a); return *this; }
    ColorField& set_show_hex(bool s) noexcept { m_show_hex = s; return *this; }
    ColorField& on_change(std::function<void(const Color&)> fn) { m_on_change = std::move(fn); return *this; }
    ColorPicker& picker() noexcept { return *m_picker; }
    bool is_open() const noexcept { return ui() && m_picker && ui()->popup_open(*m_picker); }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    void open();

    void close() { if (ui() && m_picker) ui()->close_popup(*m_picker); }

    void revert() {
        set_color(m_picker->original(), true);
        m_picker->set_color(m_picker->original());
    }

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override;
    void on_mouse_up(MouseEvent&) override { m_armed = false; }

    bool on_key(const KeyEvent& k) override;
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_COLOR_HPP
