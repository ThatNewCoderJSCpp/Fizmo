#ifndef FIZMO_GLYPH_RASTERIZER_HPP
#define FIZMO_GLYPH_RASTERIZER_HPP

#include "../Basic/fizmo_defines.hpp"
#include <cstdint>
#include <string>

namespace fizmo {
namespace text {

class GlyphRasterizer {
public:
    static constexpr unsigned int kCellSize = 8;

#ifdef OS_WINDOWS

    explicit GlyphRasterizer(const char* font_face = "Segoe UI") noexcept
        : m_screen_dc(NULL), m_mem_dc(NULL), m_bmp(NULL),
          m_font(NULL), m_old_bmp(NULL), m_old_font(NULL), m_ok(false)
    {
        m_screen_dc = GetDC(NULL);
        if (!m_screen_dc) return;
        m_mem_dc = CreateCompatibleDC(m_screen_dc);
        if (!m_mem_dc) return;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = kCellSize;
        bmi.bmiHeader.biHeight      = -static_cast<LONG>(kCellSize); 
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        m_bmp = CreateDIBSection(m_mem_dc, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_bits), NULL, 0);
        if (!m_bmp) return;
        m_old_bmp = static_cast<HBITMAP>(SelectObject(m_mem_dc, m_bmp));
        LOGFONTW lf = {};
        lf.lfHeight         = -static_cast<LONG>(kCellSize);
        lf.lfWeight         = FW_NORMAL;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_TT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = NONANTIALIASED_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        MultiByteToWideChar(CP_UTF8, 0, font_face, -1, lf.lfFaceName, LF_FACESIZE);
        m_font = CreateFontIndirectW(&lf);
        if (!m_font) return;
        m_old_font = static_cast<HFONT>(SelectObject(m_mem_dc, m_font));
        SetTextColor(m_mem_dc, RGB(255, 255, 255));
        SetBkColor(m_mem_dc, RGB(0, 0, 0));
        SetBkMode(m_mem_dc, OPAQUE);
        SetTextAlign(m_mem_dc, TA_TOP | TA_LEFT);
        m_ok = true;
    }

    ~GlyphRasterizer() noexcept {
        if (m_old_font)  SelectObject(m_mem_dc, m_old_font);
        if (m_old_bmp)   SelectObject(m_mem_dc, m_old_bmp);
        if (m_font)      DeleteObject(m_font);
        if (m_bmp)       DeleteObject(m_bmp);
        if (m_mem_dc)    DeleteDC(m_mem_dc);
        if (m_screen_dc) ReleaseDC(NULL, m_screen_dc);
    }

    GlyphRasterizer(const GlyphRasterizer&) = delete;
    GlyphRasterizer& operator=(const GlyphRasterizer&) = delete;

    bool valid() const noexcept { return m_ok; }

    bool rasterize(std::uint32_t codepoint, std::uint8_t* out) noexcept {
        if (!m_ok || !out) return false;
        RECT r = { 0, 0, static_cast<LONG>(kCellSize), static_cast<LONG>(kCellSize) };
        HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        FillRect(m_mem_dc, &r, black);
        wchar_t wc[2];
        int wlen = 0;

        if (codepoint <= 0xFFFF) {
            wc[0] = static_cast<wchar_t>(codepoint);
            wlen = 1;
        } else if (codepoint <= 0x10FFFF) {
            std::uint32_t cp = codepoint - 0x10000;
            wc[0] = static_cast<wchar_t>(0xD800 + (cp >> 10));
            wc[1] = static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
            wlen = 2;
        } else {
            return false;
        }

        SIZE sz;
        GetTextExtentPoint32W(m_mem_dc, wc, wlen, &sz);
        int ox = (static_cast<int>(kCellSize) - sz.cx) / 2;
        int oy = (static_cast<int>(kCellSize) - sz.cy) / 2;
        if (ox < 0) ox = 0;
        if (oy < 0) oy = 0;
        TextOutW(m_mem_dc, ox, oy, wc, wlen);
        GdiFlush();
        bool any_lit = false;

        for (unsigned int row = 0; row < kCellSize; ++row) {
            std::uint8_t byte = 0;
            for (unsigned int col = 0; col < kCellSize; ++col) {
                std::uint32_t pixel = m_bits[row * kCellSize + col];

                if (pixel & 0x00FFFFFF) {
                    byte |= (0x80 >> col);
                    any_lit = true;
                }
            }

            out[row] = byte;
        }

        return any_lit;
    }

private:
    HDC     m_screen_dc;
    HDC     m_mem_dc;
    HBITMAP m_bmp;
    HFONT   m_font;
    HBITMAP m_old_bmp;
    HFONT   m_old_font;

    std::uint32_t* m_bits = nullptr;   
    bool           m_ok;

#else
    explicit GlyphRasterizer(const char* /*font_face*/ = nullptr) noexcept {}
    bool valid() const noexcept { return false; }
    bool rasterize(std::uint32_t, std::uint8_t*) noexcept { return false; }
#endif // OS_WINDOWS
};

} // namespace text
} // namespace fizmo

#endif // FIZMO_GLYPH_RASTERIZER_HPP