#ifndef FIZMO_RENDERER_WINDOWS_IMPL_HPP
#define FIZMO_RENDERER_WINDOWS_IMPL_HPP

#include "renderer_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_WINDOWS
    #if defined(_MSC_VER)
        #pragma comment(lib, "msimg32.lib")   
    #endif
#endif

namespace fizmo {
namespace windows {
namespace detail {

#ifdef OS_WINDOWS
class RendererImpl : public RendererImplBase {
private:
    HWND m_hwnd = NULL;
    HDC m_back_dc = NULL;
    HBITMAP m_back_bitmap = NULL;
    HBITMAP m_old_bitmap = NULL;
    unsigned int m_width = 0;
    unsigned int m_height = 0;
    HDC           m_scratch_dc   = NULL;
    HBITMAP       m_scratch_bmp  = NULL;
    HGDIOBJ       m_scratch_old  = NULL;
    std::uint8_t* m_scratch_bits = nullptr;
    unsigned int  m_scratch_w    = 0;
    unsigned int  m_scratch_h    = 0;

    void create_back_buffer(unsigned int w, unsigned int h) noexcept {
        HDC window_dc = GetDC(m_hwnd);
        m_back_dc = CreateCompatibleDC(window_dc);
        m_back_bitmap = CreateCompatibleBitmap(window_dc, w, h);
        m_old_bitmap = static_cast<HBITMAP>(SelectObject(m_back_dc, m_back_bitmap));
        ReleaseDC(m_hwnd, window_dc);
        m_width = w;
        m_height = h;
    }

    void destroy_back_buffer() noexcept {
        if (m_back_dc) {
            SelectObject(m_back_dc, m_old_bitmap);
            DeleteObject(m_back_bitmap);
            DeleteDC(m_back_dc);
            m_back_dc = NULL;
            m_back_bitmap = NULL;
            m_old_bitmap = NULL;
        }
    }

    void destroy_scratch() noexcept {
        if (m_scratch_dc) {
            SelectObject(m_scratch_dc, m_scratch_old);
            if (m_scratch_bmp) DeleteObject(m_scratch_bmp);
            DeleteDC(m_scratch_dc);
        }

        m_scratch_dc = NULL; m_scratch_bmp = NULL; m_scratch_old = NULL;
        m_scratch_bits = nullptr; m_scratch_w = m_scratch_h = 0;
    }

    bool ensure_scratch(unsigned int w, unsigned int h) noexcept {
        if (m_scratch_bmp && w <= m_scratch_w && h <= m_scratch_h) { GdiFlush(); return true; }
        const unsigned int nw = std::max(w, m_scratch_w), nh = std::max(h, m_scratch_h);
        destroy_scratch();
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = static_cast<LONG>(nw);
        bmi.bmiHeader.biHeight      = -static_cast<LONG>(nh);
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        m_scratch_dc = CreateCompatibleDC(NULL);
        if (!m_scratch_dc) return false;
        void* bits = nullptr;
        m_scratch_bmp = CreateDIBSection(m_scratch_dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!m_scratch_bmp || !bits) { destroy_scratch(); return false; }
        m_scratch_old  = SelectObject(m_scratch_dc, m_scratch_bmp);
        m_scratch_bits = static_cast<std::uint8_t*>(bits);
        m_scratch_w = nw;
        m_scratch_h = nh;
        return true;
    }

    std::uint8_t* scratch_row(unsigned int row) noexcept {
        return m_scratch_bits + static_cast<std::size_t>(row) * m_scratch_w * 4;
    }

    static void put_premul_bgra(std::uint8_t* d, const graphics::Color& c) noexcept {
        const unsigned int a = c.alpha();
        d[0] = static_cast<std::uint8_t>((c.blue()  * a + 127u) / 255u);
        d[1] = static_cast<std::uint8_t>((c.green() * a + 127u) / 255u);
        d[2] = static_cast<std::uint8_t>((c.red()   * a + 127u) / 255u);
        d[3] = static_cast<std::uint8_t>(a);
    }

    void blend_scratch(int dx, int dy, unsigned int dw, unsigned int dh, unsigned int sw, unsigned int sh, float opacity) noexcept {
        const float o = std::max(0.0f, std::min(1.0f, opacity));
        BLENDFUNCTION bf = {};
        bf.BlendOp             = AC_SRC_OVER;
        bf.SourceConstantAlpha = static_cast<BYTE>(o * 255.0f + 0.5f);
        bf.AlphaFormat         = AC_SRC_ALPHA;
        if (bf.SourceConstantAlpha == 0) return;

        AlphaBlend(
            m_back_dc, dx, dy, static_cast<int>(dw), static_cast<int>(dh),
            m_scratch_dc, 0, 0, static_cast<int>(sw), static_cast<int>(sh), bf
        );
    }

    static COLORREF to_cr(const graphics::Color& c) noexcept {
        return RGB(c.red(), c.green(), c.blue());
    }

    static int gdi_pen_endcap(graphics::LineCap cap) noexcept {
        switch (cap) {
            case graphics::LineCap::Round:  return PS_ENDCAP_ROUND;
            case graphics::LineCap::Square: return PS_ENDCAP_SQUARE;
            default:                        return PS_ENDCAP_FLAT;
        }
    }

    static int gdi_pen_join(graphics::LineJoin join) noexcept {
        switch (join) {
            case graphics::LineJoin::Round: return PS_JOIN_ROUND;
            case graphics::LineJoin::Bevel: return PS_JOIN_BEVEL;
            default:                        return PS_JOIN_MITER;
        }
    }

    HPEN create_stroke_pen(const graphics::Paint& p) noexcept {
        if (!p.has_stroke()) return static_cast<HPEN>(GetStockObject(NULL_PEN));
        LOGBRUSH lb;
        lb.lbStyle = BS_SOLID;
        lb.lbColor = to_cr(p.stroke_color());
        lb.lbHatch = 0;

        return ExtCreatePen(
            PS_GEOMETRIC | PS_SOLID | gdi_pen_endcap(p.line_cap()) | gdi_pen_join(p.line_join()),
            static_cast<int>(p.stroke_width()),
            &lb, 0, NULL
        );
    }

    HBRUSH create_fill_brush(const graphics::Paint& p) noexcept {
        if (!p.has_fill()) return static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
        return CreateSolidBrush(to_cr(p.fill_color()));
    }

public:
    RendererImpl() noexcept = default;
    ~RendererImpl() noexcept override { shutdown(); }

    bool initialize(void* native_handle, unsigned int w, unsigned int h) noexcept override {
        m_hwnd = static_cast<HWND>(native_handle);
        if (!m_hwnd) return false;
        create_back_buffer(w, h);
        return m_back_dc != NULL;
    }

    void shutdown() noexcept override {
        destroy_scratch();
        destroy_back_buffer();
        m_hwnd = NULL;
    }

    void resize(unsigned int w, unsigned int h) noexcept override {
        if (w == m_width && h == m_height) return;
        destroy_back_buffer();
        create_back_buffer(w, h);
    }

    void begin_frame() noexcept override {}

    void present() noexcept override {
        if (!m_hwnd || !m_back_dc) return;
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateWindow(m_hwnd);
    }

    void paint(void* paint_dc) noexcept override {
        if (!m_back_dc || !paint_dc) return;
        BitBlt(static_cast<HDC>(paint_dc), 0, 0, m_width, m_height, m_back_dc, 0, 0, SRCCOPY);
    }

    void clear(const graphics::Color& color) noexcept override {
        if (!m_back_dc) return;
        HBRUSH brush = CreateSolidBrush(to_cr(color));
        RECT rect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        FillRect(m_back_dc, &rect, brush);
        DeleteObject(brush);
    }

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin = RectOrigin::TopLeft) noexcept override {
        if (!m_back_dc) return;
        int rx, ry;
        resolve_rect_origin(rx, ry, x, y, w, h, origin);
        HRGN rgn = CreateRectRgn(rx, ry, rx + static_cast<int>(w), ry + static_cast<int>(h));
        SelectClipRgn(m_back_dc, rgn);
        DeleteObject(rgn);
    }

    void reset_clip_rect() noexcept override {
        if (!m_back_dc) return;
        SelectClipRgn(m_back_dc, NULL);
    }

    void draw_pixel(int x, int y, const graphics::Color& color) noexcept override {
        if (!m_back_dc) return;
        SetPixel(m_back_dc, x, y, to_cr(color));
    }

    void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& p) noexcept override {
        if (!m_back_dc || !p.has_stroke()) return;
        HPEN pen = create_stroke_pen(p);
        HPEN old = static_cast<HPEN>(SelectObject(m_back_dc, pen));
        MoveToEx(m_back_dc, x1, y1, NULL);
        LineTo(m_back_dc, x2, y2);
        SelectObject(m_back_dc, old);
        DeleteObject(pen);
    }

    void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& p) noexcept override {
        if (!m_back_dc) return;
        if (!p.has_fill() && !p.has_stroke()) return;
        int rx, ry;
        resolve_rect_origin(rx, ry, x, y, w, h, p.origin());

        if (p.has_fill() && !p.has_stroke()) {
            HBRUSH brush = CreateSolidBrush(to_cr(p.fill_color()));
            RECT rect = { rx, ry, rx + static_cast<int>(w), ry + static_cast<int>(h) };
            FillRect(m_back_dc, &rect, brush);
            DeleteObject(brush);
            return;
        }

        HPEN pen = create_stroke_pen(p);
        HBRUSH brush = create_fill_brush(p);
        bool owns_brush = p.has_fill();
        HPEN old_pen = static_cast<HPEN>(SelectObject(m_back_dc, pen));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(m_back_dc, brush));
        Rectangle(m_back_dc, rx, ry, rx + static_cast<int>(w), ry + static_cast<int>(h));
        SelectObject(m_back_dc, old_pen);
        SelectObject(m_back_dc, old_brush);
        DeleteObject(pen);
        if (owns_brush) DeleteObject(brush);
    }

    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!m_back_dc) return;
        if (!p.has_fill() && !p.has_stroke()) return;
        int irx = static_cast<int>(rx), iry = static_cast<int>(ry);
        bool full = p.is_full_sweep() || std::abs(p.end_angle() - p.start_angle()) < 1e-9;
        HPEN pen = create_stroke_pen(p);
        HBRUSH brush = create_fill_brush(p);
        bool owns_pen = p.has_stroke();
        bool owns_brush = p.has_fill();
        HPEN old_pen = static_cast<HPEN>(SelectObject(m_back_dc, pen));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(m_back_dc, brush));

        if (full) {
            Ellipse(m_back_dc, cx - irx, cy - iry, cx + irx + 1, cy + iry + 1);
        } else {
            constexpr double kPi_180 = constants::pi_180();
            double sa_rad = p.start_angle() * kPi_180;
            double ea_rad = p.end_angle()   * kPi_180;
            int x1 = cx + static_cast<int>(std::round(1000.0 * std::cos(sa_rad)));
            int y1 = cy + static_cast<int>(std::round(1000.0 * std::sin(sa_rad)));
            int x2 = cx + static_cast<int>(std::round(1000.0 * std::cos(ea_rad)));
            int y2 = cy + static_cast<int>(std::round(1000.0 * std::sin(ea_rad)));
            int dir = (p.end_angle() >= p.start_angle()) ? AD_CLOCKWISE : AD_COUNTERCLOCKWISE;
            SetArcDirection(m_back_dc, dir);
            Pie(m_back_dc, cx - irx, cy - iry, cx + irx + 1, cy + iry + 1, x1, y1, x2, y2);
        }

        SelectObject(m_back_dc, old_pen);
        SelectObject(m_back_dc, old_brush);
        if (owns_pen)   DeleteObject(pen);
        if (owns_brush)  DeleteObject(brush);
    }

    void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!m_back_dc) return;
        if (!p.has_fill() && !p.has_stroke()) return;
        int irx = static_cast<int>(rx), iry = static_cast<int>(ry);
        constexpr double kPi_180 = constants::pi_180();
        double sa_rad = p.start_angle() * kPi_180;
        double ea_rad = p.end_angle()   * kPi_180;
        int x1 = cx + static_cast<int>(std::round(1000.0 * std::cos(sa_rad)));
        int y1 = cy + static_cast<int>(std::round(1000.0 * std::sin(sa_rad)));
        int x2 = cx + static_cast<int>(std::round(1000.0 * std::cos(ea_rad)));
        int y2 = cy + static_cast<int>(std::round(1000.0 * std::sin(ea_rad)));
        int dir = (p.end_angle() >= p.start_angle()) ? AD_CLOCKWISE : AD_COUNTERCLOCKWISE;
        SetArcDirection(m_back_dc, dir);
        HPEN pen = create_stroke_pen(p);
        HBRUSH brush = create_fill_brush(p);
        bool owns_pen = p.has_stroke();
        bool owns_brush = p.has_fill();
        HPEN old_pen = static_cast<HPEN>(SelectObject(m_back_dc, pen));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(m_back_dc, brush));

        if (p.has_fill()) {
            Chord(m_back_dc, cx - irx, cy - iry, cx + irx + 1, cy + iry + 1, x1, y1, x2, y2);
        } else {
            Arc(m_back_dc, cx - irx, cy - iry, cx + irx + 1, cy + iry + 1, x1, y1, x2, y2);
        }

        SelectObject(m_back_dc, old_pen);
        SelectObject(m_back_dc, old_brush);
        if (owns_pen)   DeleteObject(pen);
        if (owns_brush)  DeleteObject(brush);
    }

    void draw_image(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const fizmo::images::BitmapImage& img,
        unsigned int sx, unsigned int sy,
        unsigned int sw, unsigned int sh
    ) noexcept override {
        if (!m_back_dc) return;
        const unsigned int img_w = img.width(), img_h = img.height();
        if (img_w == 0 || img_h == 0 || sw == 0 || sh == 0 || dw == 0 || dh == 0) return;
        if (sx >= img_w || sy >= img_h) return;
        sw = std::min(sw, img_w - sx);
        sh = std::min(sh, img_h - sy);
        if (!ensure_scratch(sw, sh)) return;

        for (unsigned int r = 0; r < sh; ++r) {
            std::uint8_t* row = scratch_row(r);
            for (unsigned int c = 0; c < sw; ++c) put_premul_bgra(row + c * 4, img.get_pixel(sx + c, sy + r));
        }

        blend_scratch(dx, dy, dw, dh, sw, sh, 1.0f);
    }

    void draw_pixel_buffer(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const graphics::Color* pixels, unsigned int pw, unsigned int ph,
        bool smooth, std::uint64_t /*version*/
    ) noexcept override {
        if (!m_back_dc || !pixels || pw == 0 || ph == 0 || dw == 0 || dh == 0) return;
        if (!ensure_scratch(pw, ph)) return;

        for (unsigned int r = 0; r < ph; ++r) {
            std::uint8_t* row = scratch_row(r);
            const graphics::Color* src = pixels + static_cast<std::size_t>(r) * pw;
            for (unsigned int c = 0; c < pw; ++c) put_premul_bgra(row + c * 4, src[c]);
        }

        (void)smooth;  
        blend_scratch(dx, dy, dw, dh, pw, ph, 1.0f);
    }

    void draw_texture(
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        const graphics::Texture& tex,
        float opacity,
        const graphics::TextureRect& src
    ) noexcept override {
        if (!m_back_dc || !tex.valid()) return;
        if (dw == 0 || dh == 0 || src.is_empty() || opacity <= 0.0f) return;
        const int x0 = std::max(dx, 0);
        const int y0 = std::max(dy, 0);
        const int x1 = std::min(dx + static_cast<int>(dw), static_cast<int>(m_width));
        const int y1 = std::min(dy + static_cast<int>(dh), static_cast<int>(m_height));
        if (x1 <= x0 || y1 <= y0) return;
        const unsigned int rw = static_cast<unsigned int>(x1 - x0);
        const unsigned int rh = static_cast<unsigned int>(y1 - y0);
        if (!ensure_scratch(rw, rh)) return;
        const double sx = static_cast<double>(src.w) / dw;
        const double sy = static_cast<double>(src.h) / dh;

        for (unsigned int r = 0; r < rh; ++r) {
            std::uint8_t* row = scratch_row(r);
            const double v = src.y + (static_cast<double>(y0 - dy + static_cast<int>(r)) + 0.5) * sy;

            for (unsigned int c = 0; c < rw; ++c) {
                const double u = src.x + (static_cast<double>(x0 - dx + static_cast<int>(c)) + 0.5) * sx;
                put_premul_bgra(row + c * 4, tex.sample(u, v));
            }
        }

        blend_scratch(x0, y0, rw, rh, rw, rh, opacity);
    }

    bool load_font_file(const char* utf8_path) noexcept override {
        if (!utf8_path) return false;
        const int n = MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, nullptr, 0);
        if (n <= 0) return false;
        std::vector<wchar_t> path(static_cast<std::size_t>(n));
        MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, path.data(), n);
        return AddFontResourceExW(path.data(), FR_PRIVATE, nullptr) > 0;
    }

    void draw_text(
        int x, int y,
        const wchar_t* str, int len,
        const text::TextStyle& style
    ) noexcept override {
        if (!m_back_dc || !str || len == 0) return;
        HFONT font = create_gdi_font(style);
        HFONT old_font = static_cast<HFONT>(SelectObject(m_back_dc, font));
        COLORREF old_fg = GetTextColor(m_back_dc);
        if (style.has_fg_color()) { SetTextColor(m_back_dc, to_cr(style.fg_color().value())); }
        int old_bk_mode = GetBkMode(m_back_dc);
        COLORREF old_bk = GetBkColor(m_back_dc);

        if (style.has_bg_color()) {
            SetBkMode(m_back_dc, OPAQUE);
            SetBkColor(m_back_dc, to_cr(style.bg_color().value()));
        } else {
            SetBkMode(m_back_dc, TRANSPARENT);
        }

        int old_extra = 0;

        if (style.has_letter_spacing()) {
            old_extra = GetTextCharacterExtra(m_back_dc);
            SetTextCharacterExtra(m_back_dc, static_cast<int>(style.letter_spacing()));
        }

        if (style.has_text_align()) {
            UINT align_flags = TA_TOP;

            switch (style.text_align()) {
                case text::TextAlign::Left:    align_flags |= TA_LEFT;   break;
                case text::TextAlign::Center:  align_flags |= TA_CENTER; break;
                case text::TextAlign::Right:   align_flags |= TA_RIGHT;  break;
                default:                       align_flags |= TA_LEFT;   break;
            }

            UINT old_align = SetTextAlign(m_back_dc, align_flags);
            TextOutW(m_back_dc, x, y, str, len);
            SetTextAlign(m_back_dc, old_align);
        } else {
            TextOutW(m_back_dc, x, y, str, len);
        }

        if (style.has_letter_spacing()) { SetTextCharacterExtra(m_back_dc, old_extra); }
        SetBkMode(m_back_dc, old_bk_mode);
        SetBkColor(m_back_dc, old_bk);
        SetTextColor(m_back_dc, old_fg);
        SelectObject(m_back_dc, old_font);
        DeleteObject(font);
    }

    void draw_text(
        int x, int y,
        const char* utf8, int len,
        const text::TextStyle& style
    ) noexcept override {
        if (!utf8 || len == 0) return;
        constexpr int kStackBuf = 512;
        wchar_t stack[kStackBuf];
        int wlen = utf8_to_wide(utf8, len, nullptr, 0);
        if (wlen <= 0) return;

        if (wlen <= kStackBuf) {
            utf8_to_wide(utf8, len, stack, kStackBuf);
            draw_text(x, y, stack, wlen, style);
        } else {
            std::vector<wchar_t> heap(wlen);
            utf8_to_wide(utf8, len, heap.data(), wlen);
            draw_text(x, y, heap.data(), wlen, style);
        }
    }

    text::TextMetrics measure_text(
        const wchar_t* str, int len,
        const text::TextStyle& style
    ) noexcept override {
        text::TextMetrics m;
        if (!m_back_dc || !str || len == 0) return m;
        HFONT font = create_gdi_font(style);
        HFONT old_font = static_cast<HFONT>(SelectObject(m_back_dc, font));
        int old_extra = 0;

        if (style.has_letter_spacing()) {
            old_extra = GetTextCharacterExtra(m_back_dc);
            SetTextCharacterExtra(m_back_dc, static_cast<int>(style.letter_spacing()));
        }

        SIZE sz;
        GetTextExtentPoint32W(m_back_dc, str, len, &sz);
        m.width  = static_cast<unsigned int>(sz.cx);
        m.height = static_cast<unsigned int>(sz.cy);
        TEXTMETRICW tm;

        if (GetTextMetricsW(m_back_dc, &tm)) {
            m.ascent  = tm.tmAscent;
            m.descent = tm.tmDescent;
        }

        if (style.has_letter_spacing()) { SetTextCharacterExtra(m_back_dc, old_extra); }
        SelectObject(m_back_dc, old_font);
        DeleteObject(font);
        return m;
    }

    text::TextMetrics measure_text(
        const char* utf8, int len,
        const text::TextStyle& style
    ) noexcept override {
        if (!utf8 || len == 0) return {};
        constexpr int kStackBuf = 512;
        wchar_t stack[kStackBuf];
        int wlen = utf8_to_wide(utf8, len, nullptr, 0);
        if (wlen <= 0) return {};

        if (wlen <= kStackBuf) {
            utf8_to_wide(utf8, len, stack, kStackBuf);
            return measure_text(stack, wlen, style);
        } else {
            std::vector<wchar_t> heap(wlen);
            utf8_to_wide(utf8, len, heap.data(), wlen);
            return measure_text(heap.data(), wlen, style);
        }
    }

private:
    static int gdi_font_weight(text::FontWeight w) noexcept {
        switch (w) {
            case text::FontWeight::Thin:       return FW_THIN;
            case text::FontWeight::ExtraLight: return FW_EXTRALIGHT;
            case text::FontWeight::Light:      return FW_LIGHT;
            case text::FontWeight::Normal:     return FW_NORMAL;
            case text::FontWeight::Medium:     return FW_MEDIUM;
            case text::FontWeight::SemiBold:   return FW_SEMIBOLD;
            case text::FontWeight::Bold:       return FW_BOLD;
            case text::FontWeight::ExtraBold:  return FW_EXTRABOLD;
            case text::FontWeight::Black:      return FW_BLACK;
            default:                           return FW_NORMAL;
        }
    }

    HFONT create_gdi_font(const text::TextStyle& style) noexcept {
        int height = 0;

        if (style.has_size()) {
            // Negative = character height
            height = -static_cast<int>(style.size());
        } else {
            height = -16;
        }

        int weight  = style.has_weight() ? gdi_font_weight(style.weight()) : FW_NORMAL;
        BOOL italic = (style.has_slant() && style.slant() != text::FontSlant::Normal) ? TRUE : FALSE;
        BOOL underline    = style.is_underlined()    ? TRUE : FALSE;
        BOOL strikeout    = style.is_strikethrough() ? TRUE : FALSE;
        const char* face = "";
        if (style.has_font() && style.font() != text::Font::None) { face = text::font_css_name(style.font()); }

        return CreateFontA(
            height,             // nHeight
            0,                  // nWidth (0 = default aspect ratio)
            0,                  // nEscapement
            0,                  // nOrientation
            weight,             // fnWeight
            italic,             // fdwItalic
            underline,          // fdwUnderline
            strikeout,          // fdwStrikeOut
            DEFAULT_CHARSET,    // fdwCharSet
            OUT_TT_PRECIS,      // fdwOutputPrecision
            CLIP_DEFAULT_PRECIS,// fdwClipPrecision
            CLEARTYPE_QUALITY,  // fdwQuality
            DEFAULT_PITCH | FF_DONTCARE, // fdwPitchAndFamily
            face                // lpszFace
        );
    }

    static int utf8_to_wide(const char* utf8, int len, wchar_t* out, int out_cap) noexcept {
        if (!utf8 || len == 0) return 0;
        return MultiByteToWideChar(CP_UTF8, 0, utf8, len, out, out_cap);
    }
};
#endif // OS_WINDOWS

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_RENDERER_WINDOWS_IMPL_HPP