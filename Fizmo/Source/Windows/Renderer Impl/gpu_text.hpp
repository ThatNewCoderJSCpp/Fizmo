#ifndef FIZMO_GPU_TEXT_HPP
#define FIZMO_GPU_TEXT_HPP

#include "../../Basic/fizmo_defines.hpp"
#include "../../Text/rich_text.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(OS_WINDOWS)
    #include <dwrite_3.h>
    #if defined(_MSC_VER)
        #pragma comment(lib, "dwrite.lib")       
    #endif
#elif defined(OS_LINUX)
    #include <pango/pangocairo.h>
    #include <pango/pangofc-fontmap.h>
    #include <fontconfig/fontconfig.h>            
#endif

namespace fizmo {
namespace windows {
namespace detail {
namespace gfx {

struct FaceMetrics {
    float ascent              = 0.0f;
    float descent             = 0.0f;
    float underline_position  = -1.0f;
    float underline_thickness = 1.0f;
    float strikeout_position  = 4.0f;
    float strikeout_thickness = 1.0f;
};

struct GlyphImage {
    int                       width  = 0;
    int                       height = 0;
    int                       left   = 0;
    int                       top    = 0;
    bool                      color  = false;
    std::vector<std::uint8_t> pixels;
};

struct PreparedSpan {
    std::string     family;
    float           size           = 16.0f;
    int             weight         = 400;
    text::FontSlant slant          = text::FontSlant::Normal;
    float           letter_spacing = 0.0f;
    float           word_spacing   = 0.0f;
    float           rise           = 0.0f;   
    std::uint32_t   begin          = 0;      
    std::uint32_t   end            = 0;
    std::uint32_t   style          = 0;      
};

struct PreparedText {
    std::string                  utf8;
    std::vector<PreparedSpan>    spans;
    std::vector<text::TextStyle> styles;

    std::uint32_t span_at(std::uint32_t byte) const noexcept {
        std::size_t lo = 0, hi = spans.size();
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (spans[mid].begin <= byte) lo = mid; else hi = mid;
        }
        return static_cast<std::uint32_t>(lo);
    }
};

struct ShapeParams {
    float           max_width    = 0.0f;     
    bool            wrap         = false;    
    bool            ellipsize    = false;    
    bool            justify      = false;
    text::TextAlign align        = text::TextAlign::Left;   
    bool            rtl          = false;
    float           line_spacing = 0.0f;     
    float           indent       = 0.0f;     
};

struct ShapedGlyph {
    std::uint32_t face    = 0;
    std::uint32_t glyph   = 0;
    std::uint32_t span    = 0;
    std::uint32_t line    = 0;
    float         x       = 0.0f;   
    float         y       = 0.0f;
    float         advance = 0.0f;
};

struct ShapedLine {
    float         left          = 0.0f;
    float         width         = 0.0f;
    float         top           = 0.0f;
    float         baseline      = 0.0f;
    float         bottom        = 0.0f;
    std::uint32_t start         = 0;    
    std::uint32_t end           = 0;
    bool          paragraph_end = false;
};

struct ShapedLayout {
    std::vector<ShapedGlyph> glyphs;
    std::vector<ShapedLine>  lines;
    float                    width  = 0.0f;
    float                    height = 0.0f;

    void clear() { glyphs.clear(); lines.clear(); width = height = 0.0f; }
};

inline const char* generic_family(text::FontCategory c) noexcept {
    switch (c) {
        case text::FontCategory::Serif:     return "serif";
        case text::FontCategory::Monospace: return "monospace";
        case text::FontCategory::Cursive:   return "cursive";
        case text::FontCategory::Fantasy:   return "fantasy";
        default:                            return "sans-serif";
    }
}

inline void utf8_append(std::string& s, std::uint32_t cp) {
    if (cp < 0x80) { s += static_cast<char>(cp); }
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

inline std::string wide_to_utf8(const wchar_t* s, int len) {
    std::string out;
    if (!s || len <= 0) return out;
    out.reserve(static_cast<std::size_t>(len));

    for (int i = 0; i < len; ++i) {
        std::uint32_t c = static_cast<std::uint32_t>(s[i]);

    #if WCHAR_MAX <= 0xFFFF
        c &= 0xFFFFu;

        if (c >= 0xD800 && c <= 0xDBFF) {
            const std::uint32_t lo = i + 1 < len ? (static_cast<std::uint32_t>(s[i + 1]) & 0xFFFFu) : 0;
            if (lo >= 0xDC00 && lo <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); ++i; }
            else c = 0xFFFD;
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            c = 0xFFFD;
        }
    #else
        if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) c = 0xFFFD;
    #endif

        utf8_append(out, c);
    }

    return out;
}

inline std::string sanitize_utf8(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    const unsigned char* p   = reinterpret_cast<const unsigned char*>(in.data());
    const unsigned char* end = p + in.size();

    while (p < end) {
        const unsigned char c = *p;
        if (c == '\r') { ++p; continue; }
        if (c < 0x80) { out += static_cast<char>(c); ++p; continue; }
        int extra = (c >> 5) == 0x06 ? 1 : (c >> 4) == 0x0E ? 2 : (c >> 3) == 0x1E ? 3 : -1;
        if (extra < 0) { utf8_append(out, 0xFFFD); ++p; continue; }
        std::uint32_t cp = c & (0x3Fu >> extra);
        const unsigned char* q = p + 1;
        bool ok = true;

        for (int i = 0; i < extra; ++i, ++q) {
            if (q >= end || (*q & 0xC0u) != 0x80u) { ok = false; break; }
            cp = (cp << 6) | (*q & 0x3Fu);
        }

        static const std::uint32_t kMin[4] = { 0, 0x80, 0x800, 0x10000 };
        
        if (!ok || cp < kMin[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            utf8_append(out, 0xFFFD);
            p = ok ? q : p + 1;
            continue;
        }

        out.append(reinterpret_cast<const char*>(p), static_cast<std::size_t>(q - p));
        p = q;
    }

    return out;
}

#if defined(OS_WINDOWS)

template <typename T>
class ComPtr {
private:
    T* m_p = nullptr;

public:
    ComPtr() noexcept = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr& o) noexcept : m_p(o.m_p) { if (m_p) m_p->AddRef(); }
    ComPtr& operator=(const ComPtr& o) noexcept { if (this != &o) { reset(); m_p = o.m_p; if (m_p) m_p->AddRef(); } return *this; }
    ComPtr(ComPtr&& o) noexcept : m_p(o.m_p) { o.m_p = nullptr; }
    ComPtr& operator=(ComPtr&& o) noexcept { if (this != &o) { reset(); m_p = o.m_p; o.m_p = nullptr; } return *this; }
    void reset() noexcept { if (m_p) { m_p->Release(); m_p = nullptr; } }
    T*   get() const noexcept { return m_p; }
    T**  put() noexcept { reset(); return &m_p; }
    T*   operator->() const noexcept { return m_p; }
    explicit operator bool() const noexcept { return m_p != nullptr; }

    template <typename U>
    ComPtr<U> as() const noexcept {
        ComPtr<U> r;
        if (m_p) m_p->QueryInterface(__uuidof(U), reinterpret_cast<void**>(r.put()));
        return r;
    }
};

inline std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

inline std::string narrow(const wchar_t* s, int len) {
    if (!s || len <= 0) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, s, len, &out[0], n, nullptr, nullptr);
    return out;
}

class TextEngine {
private:
    struct Face {
        ComPtr<IDWriteFontFace> face;
        float                   em_size  = 16.0f;
        FaceMetrics             metrics;
    };

    struct FaceKey {
        IDWriteFontFace* face;
        std::uint32_t    size_bits;
        bool operator==(const FaceKey& o) const noexcept { return face == o.face && size_bits == o.size_bits; }
    };

    struct FaceKeyHash {
        std::size_t operator()(const FaceKey& k) const noexcept {
            return std::hash<const void*>()(k.face) ^ (static_cast<std::size_t>(k.size_bits) * 0x9E3779B97F4A7C15ull);
        }
    };

    class Collector : public IDWriteTextRenderer {
    public:
        TextEngine*                       engine   = nullptr;
        const PreparedText*               text     = nullptr;
        const std::vector<std::uint32_t>* to_utf8  = nullptr;   
        ShapedLayout*                     out      = nullptr;
        std::uint32_t                     fallback_span = 0;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** obj) override {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWritePixelSnapping) || riid == __uuidof(IDWriteTextRenderer)) {
                *obj = static_cast<IDWriteTextRenderer*>(this);
                return S_OK;
            }

            *obj = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override  { return 1; }   
        ULONG STDMETHODCALLTYPE Release() override { return 1; }

        HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override { *disabled = FALSE; return S_OK; }

        HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* m) override {
            *m = DWRITE_MATRIX{ 1, 0, 0, 1, 0, 0 };
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* ppd) override { *ppd = 1.0f; return S_OK; }

        HRESULT STDMETHODCALLTYPE DrawGlyphRun(
            void*, FLOAT bx, FLOAT by, DWRITE_MEASURING_MODE,
            const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* desc, IUnknown*
        ) override {
            if (!run || !run->fontFace) return S_OK;
            const std::uint32_t face = engine->face_id(run->fontFace, run->fontEmSize);
            const bool rtl = (run->bidiLevel & 1) != 0;
            float pen = bx;

            for (UINT32 i = 0; i < run->glyphCount; ++i) {
                const float adv = run->glyphAdvances ? run->glyphAdvances[i] : 0.0f;
                float ox = 0.0f, oy = 0.0f;
                if (run->glyphOffsets) { ox = run->glyphOffsets[i].advanceOffset; oy = -run->glyphOffsets[i].ascenderOffset; }
                if (rtl) pen -= adv;

                std::uint32_t span = fallback_span;

                if (desc && desc->clusterMap && text && to_utf8) {
                    UINT32 ch = 0;
                    for (UINT32 c = 0; c < desc->stringLength; ++c) { if (desc->clusterMap[c] <= i) ch = c; else break; }
                    const std::size_t u16 = static_cast<std::size_t>(desc->textPosition) + ch;
                    if (u16 < to_utf8->size()) span = text->span_at((*to_utf8)[u16]);
                }

                ShapedGlyph g;
                g.face    = face;
                g.glyph   = run->glyphIndices[i];
                g.span    = span;
                g.x       = rtl ? pen - ox : pen + ox;
                g.y       = by + oy;
                g.advance = adv;
                g.line    = 0;                                
                out->glyphs.push_back(g);
                if (!rtl) pen += adv;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }

        HRESULT STDMETHODCALLTYPE DrawInlineObject(void* ctx, FLOAT x, FLOAT y, IDWriteInlineObject* obj, BOOL sideways, BOOL rtl, IUnknown* effect) override {
            return obj ? obj->Draw(ctx, this, x, y, sideways, rtl, effect) : S_OK;  
        }
    };

    ComPtr<IDWriteFactory2>        m_factory;
    ComPtr<IDWriteFontCollection>  m_collection;     
    ComPtr<IDWriteFontSetBuilder1> m_set_builder;
    bool                           m_collection_dirty = false;
    std::vector<Face>              m_faces;
    std::unordered_map<FaceKey, std::uint32_t, FaceKeyHash> m_face_ids;

    friend class Collector;

    std::uint32_t face_id(IDWriteFontFace* face, float em) {
        std::uint32_t bits;
        std::memcpy(&bits, &em, 4);
        const FaceKey key{ face, bits };
        auto it = m_face_ids.find(key);
        if (it != m_face_ids.end()) return it->second;
        Face f;
        face->AddRef();
        *f.face.put() = face;
        f.em_size = em;
        DWRITE_FONT_METRICS dm{};
        face->GetMetrics(&dm);
        const float s = dm.designUnitsPerEm ? em / dm.designUnitsPerEm : 1.0f;
        f.metrics.ascent              = dm.ascent * s;
        f.metrics.descent             = dm.descent * s;
        f.metrics.underline_position  = dm.underlinePosition * s;
        f.metrics.underline_thickness = std::max(1.0f, dm.underlineThickness * s);
        f.metrics.strikeout_position  = dm.strikethroughPosition * s + dm.strikethroughThickness * s * 0.5f;
        f.metrics.strikeout_thickness = std::max(1.0f, dm.strikethroughThickness * s);
        m_faces.push_back(std::move(f));
        const std::uint32_t id = static_cast<std::uint32_t>(m_faces.size() - 1);
        m_face_ids.emplace(key, id);
        return id;
    }

    bool rebuild_collection() {
        m_collection_dirty = false;
        auto f3 = m_factory.as<IDWriteFactory3>();
        auto f5 = m_factory.as<IDWriteFactory5>();
        if (!f3 || !f5 || !m_set_builder) return false;
        ComPtr<IDWriteFontSet> system;
        if (FAILED(f3->GetSystemFontSet(system.put()))) return false;
        ComPtr<IDWriteFontSetBuilder1> builder;
        if (FAILED(f5->CreateFontSetBuilder(builder.put()))) return false;
        builder->AddFontSet(system.get());
        ComPtr<IDWriteFontSet> custom;
        if (SUCCEEDED(m_set_builder->CreateFontSet(custom.put()))) builder->AddFontSet(custom.get());
        ComPtr<IDWriteFontSet> merged;
        if (FAILED(builder->CreateFontSet(merged.put()))) return false;
        ComPtr<IDWriteFontCollection1> collection;
        if (FAILED(f5->CreateFontCollectionFromFontSet(merged.get(), collection.put()))) return false;
        *m_collection.put() = collection.get();
        collection.get()->AddRef();
        return true;
    }

    bool has_family(const std::wstring& name) {
        if (!m_collection) return true;          
        UINT32 index = 0;
        BOOL exists = FALSE;
        return SUCCEEDED(m_collection->FindFamilyName(name.c_str(), &index, &exists)) && exists;
    }

    std::wstring resolve_family(const std::string& list) {
        const std::size_t comma = list.find(',');
        std::string name = list.substr(0, comma);
        std::string generic = comma == std::string::npos ? std::string("sans-serif") : list.substr(comma + 1);
        while (!generic.empty() && generic.front() == ' ') generic.erase(generic.begin());
        const std::wstring wname = widen(name);
        if (!name.empty() && has_family(wname)) return wname;
        if (generic == "serif")     return L"Times New Roman";
        if (generic == "monospace") return L"Consolas";
        if (generic == "cursive")   return L"Segoe Script";
        if (generic == "fantasy")   return L"Impact";
        return L"Segoe UI";
    }

    static DWRITE_FONT_STYLE dw_style(text::FontSlant s) noexcept {
        return s == text::FontSlant::Italic ? DWRITE_FONT_STYLE_ITALIC
             : s == text::FontSlant::Oblique ? DWRITE_FONT_STYLE_OBLIQUE : DWRITE_FONT_STYLE_NORMAL;
    }

public:
    TextEngine() noexcept = default;
    ~TextEngine() noexcept { shutdown(); }
    TextEngine(const TextEngine&) = delete;
    TextEngine& operator=(const TextEngine&) = delete;

    bool initialize(void* /*native_window*/ = nullptr) noexcept {
        if (m_factory) return true;

        try {
            if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(m_factory.put()))))
                return false;
            if (auto f5 = m_factory.as<IDWriteFactory5>()) f5->CreateFontSetBuilder(m_set_builder.put());
            m_factory->GetSystemFontCollection(m_collection.put(), FALSE);
            return true;
        } catch (...) {
            m_factory.reset();
            return false;
        }
    }

    void shutdown() noexcept {
        m_face_ids.clear();
        m_faces.clear();
        m_collection.reset();
        m_set_builder.reset();
        m_factory.reset();
    }

    bool ready() const noexcept { return static_cast<bool>(m_factory); }

    bool load_font_file(const std::string& utf8_path) {
        if (!m_factory || !m_set_builder) return false;
        const std::wstring path = widen(utf8_path);
        ComPtr<IDWriteFontFile> file;
        if (FAILED(m_factory->CreateFontFileReference(path.c_str(), nullptr, file.put()))) return false;
        if (FAILED(m_set_builder->AddFontFile(file.get()))) return false;
        m_collection_dirty = true;
        return rebuild_collection();
    }

    std::string transform_case(const std::string& s, text::TextTransform t) {
        if (t == text::TextTransform::None || s.empty()) return s;
        std::wstring w = widen(s);
        DWORD flags = LCMAP_LINGUISTIC_CASING;

        if (t == text::TextTransform::Uppercase) flags |= LCMAP_UPPERCASE;
        else if (t == text::TextTransform::Lowercase) flags |= LCMAP_LOWERCASE;
        else {
        #ifdef LCMAP_TITLECASE
            flags = LCMAP_TITLECASE;
        #else
            flags |= LCMAP_UPPERCASE;
        #endif
        }

        const int n = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr, 0);
        if (n <= 0) return s;
        std::wstring out(static_cast<std::size_t>(n), L'\0');
        LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, w.c_str(), static_cast<int>(w.size()), &out[0], n, nullptr, nullptr, 0);
        return narrow(out.c_str(), n);
    }

    bool shape(const PreparedText& text, const ShapeParams& p, ShapedLayout& out) {
        out.clear();
        if (!m_factory || text.spans.empty()) return false;
        if (m_collection_dirty) rebuild_collection();

        std::wstring w;
        std::vector<std::uint32_t> to_utf8;
        std::vector<std::uint32_t> to_utf16(text.utf8.size() + 1, 0);
        w.reserve(text.utf8.size());
        to_utf8.reserve(text.utf8.size());

        for (std::size_t i = 0; i < text.utf8.size();) {
            const unsigned char c = static_cast<unsigned char>(text.utf8[i]);
            const int len = c < 0x80 ? 1 : (c >> 5) == 0x06 ? 2 : (c >> 4) == 0x0E ? 3 : 4;
            std::uint32_t cp = len == 1 ? c : c & (0x7Fu >> len);
            for (int k = 1; k < len && i + k < text.utf8.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(text.utf8[i + k]) & 0x3Fu);
            for (int k = 0; k < len && i + k <= text.utf8.size(); ++k) to_utf16[i + k] = static_cast<std::uint32_t>(w.size());

            if (cp >= 0x10000) {
                cp -= 0x10000;
                w += static_cast<wchar_t>(0xD800 + (cp >> 10));
                w += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
                to_utf8.push_back(static_cast<std::uint32_t>(i));
                to_utf8.push_back(static_cast<std::uint32_t>(i));
            } else {
                w += static_cast<wchar_t>(cp);
                to_utf8.push_back(static_cast<std::uint32_t>(i));
            }

            i += static_cast<std::size_t>(len);
        }

        to_utf16[text.utf8.size()] = static_cast<std::uint32_t>(w.size());
        if (w.empty()) return false;
        const PreparedSpan& first = text.spans.front();
        ComPtr<IDWriteTextFormat> format;

        if (
            FAILED(
                m_factory->CreateTextFormat(
                    resolve_family(first.family).c_str(), m_collection.get(),
                    static_cast<DWRITE_FONT_WEIGHT>(first.weight), 
                    dw_style(first.slant), 
                    DWRITE_FONT_STRETCH_NORMAL,
                    first.size, L"", format.put()
                )
            )
        ) return false;

        const bool bounded = p.max_width > 0.0f && (p.wrap || p.ellipsize);
        format->SetWordWrapping(p.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
        format->SetReadingDirection(p.rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

        if (p.wrap) {
            format->SetTextAlignment(
                p.justify ? DWRITE_TEXT_ALIGNMENT_JUSTIFIED :
                p.align == text::TextAlign::Center ? DWRITE_TEXT_ALIGNMENT_CENTER :
                p.align == text::TextAlign::Right  ? (p.rtl ? DWRITE_TEXT_ALIGNMENT_LEADING : DWRITE_TEXT_ALIGNMENT_TRAILING) :
                (p.rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING)
            );
        }

        if (p.line_spacing > 0.0f) {
            format->SetLineSpacing(static_cast<DWRITE_LINE_SPACING_METHOD>(2 /* PROPORTIONAL */), p.line_spacing, p.line_spacing * 0.8f);
        }

        ComPtr<IDWriteInlineObject> ellipsis;

        if (p.ellipsize) {
            DWRITE_TRIMMING trim{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
            m_factory->CreateEllipsisTrimmingSign(format.get(), ellipsis.put());
            format->SetTrimming(&trim, ellipsis.get());
        }

        ComPtr<IDWriteTextLayout> layout;
        const float max_w = bounded ? p.max_width : 1.0e7f;
        if (FAILED(m_factory->CreateTextLayout(w.c_str(), static_cast<UINT32>(w.size()), format.get(), max_w, 1.0e7f, layout.put()))) return false;
        auto layout1 = layout.as<IDWriteTextLayout1>();

        for (const PreparedSpan& sp : text.spans) {
            const DWRITE_TEXT_RANGE r{ to_utf16[sp.begin], to_utf16[sp.end] - to_utf16[sp.begin] };
            if (r.length == 0) continue;
            layout->SetFontFamilyName(resolve_family(sp.family).c_str(), r);
            layout->SetFontSize(sp.size, r);
            layout->SetFontWeight(static_cast<DWRITE_FONT_WEIGHT>(sp.weight), r);
            layout->SetFontStyle(dw_style(sp.slant), r);

            if (layout1 && (sp.letter_spacing != 0.0f || sp.word_spacing != 0.0f)) {
                if (sp.letter_spacing != 0.0f) layout1->SetCharacterSpacing(0.0f, sp.letter_spacing, 0.0f, r);

                if (sp.word_spacing != 0.0f) {
                    for (UINT32 i = r.startPosition; i < r.startPosition + r.length; ++i)
                        if (w[i] == L' ' || w[i] == 0x00A0)
                            layout1->SetCharacterSpacing(0.0f, sp.letter_spacing + sp.word_spacing, 0.0f, DWRITE_TEXT_RANGE{ i, 1 });
                }
            }
        }

        if (p.indent != 0.0f && layout1) {
            for (std::size_t i = 0; i < w.size(); ++i) {
                if (i == 0 || w[i - 1] == L'\n') {
                    const std::uint32_t sp = text.span_at(to_utf8[i]);
                    layout1->SetCharacterSpacing(p.indent, text.spans[sp].letter_spacing, 0.0f, DWRITE_TEXT_RANGE{ static_cast<UINT32>(i), 1 });
                }
            }
        }

        UINT32 line_count = 0;
        layout->GetLineMetrics(nullptr, 0, &line_count);
        std::vector<DWRITE_LINE_METRICS> lm(line_count);
        if (line_count && FAILED(layout->GetLineMetrics(lm.data(), line_count, &line_count))) return false;
        std::vector<DWRITE_CLUSTER_METRICS> cm;
        UINT32 cluster_count = 0;
        layout->GetClusterMetrics(nullptr, 0, &cluster_count);
        float y = 0.0f;
        UINT32 pos = 0;

        for (UINT32 i = 0; i < line_count; ++i) {
            ShapedLine L;
            L.top      = y;
            L.baseline = y + lm[i].baseline;
            L.bottom   = y + lm[i].height;
            L.start    = to_utf8.empty() ? 0 : to_utf8[std::min<std::size_t>(pos, to_utf8.size() - 1)];
            const UINT32 end16 = pos + lm[i].length;
            L.end      = end16 >= w.size() ? static_cast<std::uint32_t>(text.utf8.size()) : to_utf8[end16];
            L.paragraph_end = lm[i].newlineLength > 0;
            out.lines.push_back(L);
            y   += lm[i].height;
            pos  = end16;
        }

        for (UINT32 i = 0, p16 = 0; i < line_count; ++i) {
            DWRITE_HIT_TEST_METRICS hm[8];
            UINT32 n = 0;
            const UINT32 len = lm[i].length - lm[i].trailingWhitespaceLength;

            if (len > 0 && SUCCEEDED(layout->HitTestTextRange(p16, len, 0.0f, 0.0f, hm, 8, &n)) && n > 0) {
                float l = hm[0].left, r = hm[0].left + hm[0].width;
                for (UINT32 k = 1; k < n; ++k) { l = std::min(l, hm[k].left); r = std::max(r, hm[k].left + hm[k].width); }
                out.lines[i].left  = l;
                out.lines[i].width = r - l;
            }

            p16 += lm[i].length;
        }

        Collector collector;
        collector.engine  = this;
        collector.text    = &text;
        collector.to_utf8 = &to_utf8;
        collector.out     = &out;
        collector.fallback_span = static_cast<std::uint32_t>(text.spans.size() - 1);
        layout->Draw(nullptr, &collector, 0.0f, 0.0f);

        for (ShapedGlyph& g : out.glyphs) {
            std::uint32_t best = 0;
            for (std::uint32_t i = 0; i < out.lines.size(); ++i) if (out.lines[i].top <= g.y) best = i;
            g.line = best;
        }

        std::vector<float> lo(out.lines.size(), 1.0e30f), hi(out.lines.size(), -1.0e30f);

        for (const ShapedGlyph& g : out.glyphs) {
            lo[g.line] = std::min(lo[g.line], g.x);
            hi[g.line] = std::max(hi[g.line], g.x + g.advance);
        }

        for (std::size_t i = 0; i < out.lines.size(); ++i) {
            if (out.lines[i].width <= 0.0f && hi[i] > lo[i]) { out.lines[i].left = lo[i]; out.lines[i].width = hi[i] - lo[i]; }
        }

        DWRITE_TEXT_METRICS tm{};
        layout->GetMetrics(&tm);
        out.width  = 0.0f;
        for (const ShapedLine& L : out.lines) out.width = std::max(out.width, L.width);
        out.height = tm.height;
        return true;
    }

    bool rasterize(std::uint32_t face, std::uint32_t glyph, GlyphImage& out) {
        out = GlyphImage{};
        if (face >= m_faces.size()) return false;
        const Face& f = m_faces[face];
        const UINT16 index = static_cast<UINT16>(glyph);
        const FLOAT advance = 0.0f;
        const DWRITE_GLYPH_OFFSET offset{ 0.0f, 0.0f };
        DWRITE_GLYPH_RUN run{};
        run.fontFace      = f.face.get();
        run.fontEmSize    = f.em_size;
        run.glyphCount    = 1;
        run.glyphIndices  = &index;
        run.glyphAdvances = &advance;
        run.glyphOffsets  = &offset;
        ComPtr<IDWriteColorGlyphRunEnumerator> layers;
        const HRESULT hr = m_factory->TranslateColorGlyphRun(0.0f, 0.0f, &run, nullptr, DWRITE_MEASURING_MODE_NATURAL, nullptr, 0, layers.put());

        if (SUCCEEDED(hr) && layers) {
            struct Layer { std::vector<std::uint8_t> a; RECT r; DWRITE_COLOR_F c; };
            std::vector<Layer> list;
            RECT bounds{ INT_MAX, INT_MAX, INT_MIN, INT_MIN };
            BOOL has = FALSE;

            while (SUCCEEDED(layers->MoveNext(&has)) && has) {
                const DWRITE_COLOR_GLYPH_RUN* cr = nullptr;
                if (FAILED(layers->GetCurrentRun(&cr)) || !cr) break;
                Layer L;
                if (!coverage(cr->glyphRun, cr->baselineOriginX, cr->baselineOriginY, L.r, L.a)) continue;
                L.c = cr->paletteIndex == 0xFFFF ? DWRITE_COLOR_F{ 0, 0, 0, 1 } : cr->runColor;
                bounds.left = std::min(bounds.left, L.r.left);   bounds.top    = std::min(bounds.top, L.r.top);
                bounds.right = std::max(bounds.right, L.r.right); bounds.bottom = std::max(bounds.bottom, L.r.bottom);
                list.push_back(std::move(L));
            }

            if (list.empty() || bounds.right <= bounds.left) return true;
            out.width  = bounds.right - bounds.left;
            out.height = bounds.bottom - bounds.top;
            out.left   = bounds.left;
            out.top    = -bounds.top;
            out.color  = true;
            out.pixels.assign(static_cast<std::size_t>(out.width) * out.height * 4, 0);

            for (const Layer& L : list) {
                const int lw = L.r.right - L.r.left;
                const float cr = L.c.r * L.c.a, cg = L.c.g * L.c.a, cb = L.c.b * L.c.a, ca = L.c.a;

                for (int yy = L.r.top; yy < L.r.bottom; ++yy) {
                    for (int xx = L.r.left; xx < L.r.right; ++xx) {
                        const float cov = L.a[static_cast<std::size_t>(yy - L.r.top) * lw + (xx - L.r.left)] / 255.0f;
                        if (cov <= 0.0f) continue;
                        std::uint8_t* d = &out.pixels[(static_cast<std::size_t>(yy - bounds.top) * out.width + (xx - bounds.left)) * 4];
                        const float sa = ca * cov, inv = 1.0f - sa;
                        d[0] = static_cast<std::uint8_t>(std::min(255.0f, cr * cov * 255.0f + d[0] * inv + 0.5f));
                        d[1] = static_cast<std::uint8_t>(std::min(255.0f, cg * cov * 255.0f + d[1] * inv + 0.5f));
                        d[2] = static_cast<std::uint8_t>(std::min(255.0f, cb * cov * 255.0f + d[2] * inv + 0.5f));
                        d[3] = static_cast<std::uint8_t>(std::min(255.0f, sa * 255.0f + d[3] * inv + 0.5f));
                    }
                }
            }

            return true;
        }

        RECT r{};
        if (!coverage(run, 0.0f, 0.0f, r, out.pixels)) return true;  
        out.width  = r.right - r.left;
        out.height = r.bottom - r.top;
        out.left   = r.left;
        out.top    = -r.top;
        return true;
    }

    FaceMetrics face_metrics(std::uint32_t face) const noexcept {
        return face < m_faces.size() ? m_faces[face].metrics : FaceMetrics{};
    }

private:
    bool coverage(const DWRITE_GLYPH_RUN& run, float ox, float oy, RECT& r, std::vector<std::uint8_t>& a) {
        ComPtr<IDWriteGlyphRunAnalysis> analysis;
        const DWRITE_RENDERING_MODE mode = run.fontEmSize > 64.0f ? DWRITE_RENDERING_MODE_OUTLINE : DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC;

        if (
            FAILED(
                m_factory->CreateGlyphRunAnalysis(
                    &run, nullptr, mode, 
                    DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT,
                    DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, ox, oy, analysis.put()
                )
            )
        ) return false;

        if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &r)) || r.right <= r.left || r.bottom <= r.top) return false;
        a.assign(static_cast<std::size_t>(r.right - r.left) * (r.bottom - r.top), 0);
        return SUCCEEDED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &r, a.data(), static_cast<UINT32>(a.size())));
    }
};

#elif defined(OS_LINUX)

class TextEngine {
private:
    struct Face {
        PangoFont*           font   = nullptr;
        cairo_scaled_font_t* scaled = nullptr;   
        FaceMetrics          metrics;
    };

    PangoFontMap*                             m_map = nullptr;
    PangoContext*                             m_ctx = nullptr;
    std::vector<Face>                         m_faces;
    std::unordered_map<PangoFont*, std::uint32_t> m_face_ids;

    std::uint32_t face_id(PangoFont* font) {
        auto it = m_face_ids.find(font);
        if (it != m_face_ids.end()) return it->second;
        Face f;
        f.font   = PANGO_FONT(g_object_ref(font));
        f.scaled = pango_cairo_font_get_scaled_font(PANGO_CAIRO_FONT(font));

        if (PangoFontMetrics* m = pango_font_get_metrics(font, nullptr)) {
            const float S = static_cast<float>(PANGO_SCALE);
            f.metrics.ascent              = pango_font_metrics_get_ascent(m) / S;
            f.metrics.descent             = pango_font_metrics_get_descent(m) / S;
            f.metrics.underline_position  = pango_font_metrics_get_underline_position(m) / S;
            f.metrics.underline_thickness = std::max(1.0f, pango_font_metrics_get_underline_thickness(m) / S);
            f.metrics.strikeout_position  = pango_font_metrics_get_strikethrough_position(m) / S;
            f.metrics.strikeout_thickness = std::max(1.0f, pango_font_metrics_get_strikethrough_thickness(m) / S);
            pango_font_metrics_unref(m);
        }

        m_faces.push_back(f);
        const std::uint32_t id = static_cast<std::uint32_t>(m_faces.size() - 1);
        m_face_ids.emplace(font, id);
        return id;
    }

    static PangoStyle pango_style(text::FontSlant s) noexcept {
        return s == text::FontSlant::Italic ? PANGO_STYLE_ITALIC : s == text::FontSlant::Oblique ? PANGO_STYLE_OBLIQUE : PANGO_STYLE_NORMAL;
    }

    static PangoFontDescription* describe(const PreparedSpan& sp) {
        PangoFontDescription* d = pango_font_description_new();
        pango_font_description_set_family(d, sp.family.empty() ? "sans-serif" : sp.family.c_str());
        pango_font_description_set_absolute_size(d, static_cast<double>(sp.size) * PANGO_SCALE);
        pango_font_description_set_weight(d, static_cast<PangoWeight>(sp.weight));
        pango_font_description_set_style(d, pango_style(sp.slant));
        return d;
    }

public:
    TextEngine() noexcept = default;
    ~TextEngine() noexcept { shutdown(); }
    TextEngine(const TextEngine&) = delete;
    TextEngine& operator=(const TextEngine&) = delete;

    bool initialize(void* /*native_window*/ = nullptr) noexcept {
        if (m_ctx) return true;
        m_map = pango_cairo_font_map_new();
        if (!m_map) return false;
        m_ctx = pango_font_map_create_context(m_map);
        if (!m_ctx) { shutdown(); return false; }
        cairo_font_options_t* fo = cairo_font_options_create();
        cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
        cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT);
        cairo_font_options_set_hint_metrics(fo, CAIRO_HINT_METRICS_OFF);
        pango_cairo_context_set_font_options(m_ctx, fo);
        cairo_font_options_destroy(fo);
        return true;
    }

    void shutdown() noexcept {
        for (Face& f : m_faces) if (f.font) g_object_unref(f.font);
        m_faces.clear();
        m_face_ids.clear();
        if (m_ctx) { g_object_unref(m_ctx); m_ctx = nullptr; }
        if (m_map) { g_object_unref(m_map); m_map = nullptr; }
    }

    bool ready() const noexcept { return m_ctx != nullptr; }

    bool load_font_file(const std::string& utf8_path) {
        if (!m_map) return false;
        if (!FcConfigAppFontAddFile(FcConfigGetCurrent(), reinterpret_cast<const FcChar8*>(utf8_path.c_str()))) return false;
        if (PANGO_IS_FC_FONT_MAP(m_map)) pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(m_map));
        pango_context_changed(m_ctx);
        return true;
    }

    std::string transform_case(const std::string& s, text::TextTransform t) {
        if (t == text::TextTransform::None || s.empty()) return s;
        std::string out;

        if (t == text::TextTransform::Uppercase || t == text::TextTransform::Lowercase) {
            gchar* r = t == text::TextTransform::Uppercase ? g_utf8_strup(s.c_str(), static_cast<gssize>(s.size()))
                                                          : g_utf8_strdown(s.c_str(), static_cast<gssize>(s.size()));
            if (!r) return s;
            out = r;
            g_free(r);
            return out;
        }

        bool word_start = true;

        for (const gchar* p = s.c_str(); *p; p = g_utf8_next_char(p)) {
            const gunichar c = g_utf8_get_char(p);
            const gunichar o = word_start && g_unichar_isalpha(c) ? g_unichar_totitle(c) : c;
            gchar buf[8];
            out.append(buf, static_cast<std::size_t>(g_unichar_to_utf8(o, buf)));
            word_start = !g_unichar_isalnum(c) && c != '\'';
        }

        return out;
    }

    bool shape(const PreparedText& text, const ShapeParams& p, ShapedLayout& out) {
        out.clear();
        if (!m_ctx || text.spans.empty() || text.utf8.empty()) return false;
        pango_context_set_base_dir(m_ctx, p.rtl ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR);
        PangoLayout* layout = pango_layout_new(m_ctx);
        if (!layout) return false;
        pango_layout_set_auto_dir(layout, FALSE);
        pango_layout_set_text(layout, text.utf8.data(), static_cast<int>(text.utf8.size()));
        PangoFontDescription* base = describe(text.spans.front());
        pango_layout_set_font_description(layout, base);
        pango_font_description_free(base);
        PangoAttrList* attrs = pango_attr_list_new();

        for (const PreparedSpan& sp : text.spans) {
            if (sp.end <= sp.begin) continue;
            PangoFontDescription* d = describe(sp);
            PangoAttribute* a = pango_attr_font_desc_new(d);
            pango_font_description_free(d);
            a->start_index = sp.begin;
            a->end_index   = sp.end;
            pango_attr_list_insert(attrs, a);

            if (sp.letter_spacing != 0.0f) {
                a = pango_attr_letter_spacing_new(static_cast<int>(sp.letter_spacing * PANGO_SCALE));
                a->start_index = sp.begin;
                a->end_index   = sp.end;
                pango_attr_list_insert(attrs, a);
            }

            if (sp.word_spacing != 0.0f) {
                for (std::uint32_t i = sp.begin; i < sp.end; ++i) {
                    const bool nbsp = i + 1 < sp.end && static_cast<unsigned char>(text.utf8[i]) == 0xC2 && static_cast<unsigned char>(text.utf8[i + 1]) == 0xA0;
                    if (text.utf8[i] != ' ' && !nbsp) continue;
                    a = pango_attr_letter_spacing_new(static_cast<int>((sp.letter_spacing + sp.word_spacing) * PANGO_SCALE));
                    a->start_index = i;
                    a->end_index   = i + (nbsp ? 2 : 1);
                    pango_attr_list_change(attrs, a);
                }
            }
        }

        pango_layout_set_attributes(layout, attrs);
        pango_attr_list_unref(attrs);

        if (p.max_width > 0.0f && (p.wrap || p.ellipsize)) {
            pango_layout_set_width(layout, static_cast<int>(p.max_width * PANGO_SCALE));
            pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
            if (p.ellipsize) pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        }

        if (p.wrap) {
            pango_layout_set_alignment(
                layout,
                p.align == text::TextAlign::Center ? PANGO_ALIGN_CENTER :
                p.align == text::TextAlign::Right  ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT
            );

            pango_layout_set_justify(layout, p.justify);
        }

        if (p.line_spacing > 0.0f) pango_layout_set_line_spacing(layout, p.line_spacing);
        if (p.indent != 0.0f) pango_layout_set_indent(layout, static_cast<int>(p.indent * PANGO_SCALE));

        const float S = static_cast<float>(PANGO_SCALE);
        PangoLayoutIter* it = pango_layout_get_iter(layout);
        PangoLayoutLine* current = nullptr;
        std::uint32_t line_index = 0;

        do {
            PangoLayoutLine* line = pango_layout_iter_get_line_readonly(it);

            if (line != current) {
                if (current && line && line->is_paragraph_start) out.lines.back().paragraph_end = true;
                current = line;
                PangoRectangle logical;
                pango_layout_iter_get_line_extents(it, nullptr, &logical);
                int y0 = 0, y1 = 0;
                pango_layout_iter_get_line_yrange(it, &y0, &y1);
                ShapedLine L;
                L.left     = logical.x / S;
                L.width    = logical.width / S;
                L.top      = y0 / S;
                L.bottom   = y1 / S;
                L.baseline = pango_layout_iter_get_baseline(it) / S;
                L.start    = static_cast<std::uint32_t>(line->start_index);
                L.end      = static_cast<std::uint32_t>(line->start_index + line->length);
                out.lines.push_back(L);
                line_index = static_cast<std::uint32_t>(out.lines.size() - 1);
            }

            PangoLayoutRun* run = pango_layout_iter_get_run_readonly(it);
            if (!run) continue;
            PangoRectangle rl;
            pango_layout_iter_get_run_extents(it, nullptr, &rl);
            const int baseline = pango_layout_iter_get_baseline(it);
            const std::uint32_t face = face_id(run->item->analysis.font);
            PangoGlyphString* gs = run->glyphs;
            int pen = rl.x;

            for (int i = 0; i < gs->num_glyphs; ++i) {
                const PangoGlyphInfo& gi = gs->glyphs[i];
                const bool drawable = gi.glyph != PANGO_GLYPH_EMPTY && !(gi.glyph & PANGO_GLYPH_UNKNOWN_FLAG);

                if (drawable) {
                    ShapedGlyph g;
                    g.face    = face;
                    g.glyph   = gi.glyph;
                    g.line    = line_index;
                    g.span    = text.span_at(static_cast<std::uint32_t>(run->item->offset + gs->log_clusters[i]));
                    g.x       = (pen + gi.geometry.x_offset) / S;
                    g.y       = (baseline + gi.geometry.y_offset) / S;
                    g.advance = gi.geometry.width / S;
                    out.glyphs.push_back(g);
                }

                pen += gi.geometry.width;
            }
        } while (pango_layout_iter_next_run(it));

        pango_layout_iter_free(it);
        if (!out.lines.empty()) out.lines.back().paragraph_end = true;
        PangoRectangle logical;
        pango_layout_get_extents(layout, nullptr, &logical);
        out.width = 0.0f;
        for (const ShapedLine& L : out.lines) out.width = std::max(out.width, L.width);
        out.height = logical.height / S;
        g_object_unref(layout);
        return true;
    }

    bool rasterize(std::uint32_t face, std::uint32_t glyph, GlyphImage& out) {
        out = GlyphImage{};
        if (face >= m_faces.size() || !m_faces[face].scaled) return false;
        cairo_scaled_font_t* sf = m_faces[face].scaled;
        cairo_glyph_t g{ glyph, 0.0, 0.0 };
        cairo_text_extents_t e;
        cairo_scaled_font_glyph_extents(sf, &g, 1, &e);
        if (e.width <= 0.0 || e.height <= 0.0) return true;              // blank glyph
        const int x0 = static_cast<int>(std::floor(e.x_bearing)) - 1;
        const int y0 = static_cast<int>(std::floor(e.y_bearing)) - 1;
        const int x1 = static_cast<int>(std::ceil(e.x_bearing + e.width)) + 1;
        const int y1 = static_cast<int>(std::ceil(e.y_bearing + e.height)) + 1;
        const int w = x1 - x0, h = y1 - y0;
        if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return true;
        cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(surface); return false; }
        cairo_t* cr = cairo_create(surface);
        cairo_set_scaled_font(cr, sf);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        g.x = -x0;
        g.y = -y0;
        cairo_show_glyphs(cr, &g, 1);
        cairo_destroy(cr);
        cairo_surface_flush(surface);
        const unsigned char* data = cairo_image_surface_get_data(surface);
        const int stride = cairo_image_surface_get_stride(surface);
        bool color = false;

        for (int y = 0; y < h && !color; ++y) {
            const std::uint32_t* row = reinterpret_cast<const std::uint32_t*>(data + static_cast<std::size_t>(y) * stride);
            for (int x = 0; x < w; ++x) {
                const std::uint32_t px = row[x];
                const std::uint32_t a = px >> 24, r = (px >> 16) & 0xFF, gg = (px >> 8) & 0xFF, b = px & 0xFF;
                if (r != a || gg != a || b != a) { color = true; break; }   // white text is (a,a,a,a) premultiplied
            }
        }

        out.width  = w;
        out.height = h;
        out.left   = x0;
        out.top    = -y0;
        out.color  = color;
        out.pixels.resize(static_cast<std::size_t>(w) * h * (color ? 4 : 1));

        for (int y = 0; y < h; ++y) {
            const std::uint32_t* row = reinterpret_cast<const std::uint32_t*>(data + static_cast<std::size_t>(y) * stride);
            for (int x = 0; x < w; ++x) {
                const std::uint32_t px = row[x];
                if (color) {
                    std::uint8_t* d = &out.pixels[(static_cast<std::size_t>(y) * w + x) * 4];
                    d[0] = static_cast<std::uint8_t>((px >> 16) & 0xFF);
                    d[1] = static_cast<std::uint8_t>((px >> 8) & 0xFF);
                    d[2] = static_cast<std::uint8_t>(px & 0xFF);
                    d[3] = static_cast<std::uint8_t>(px >> 24);
                } else {
                    out.pixels[static_cast<std::size_t>(y) * w + x] = static_cast<std::uint8_t>(px >> 24);
                }
            }
        }

        cairo_surface_destroy(surface);
        return true;
    }

    FaceMetrics face_metrics(std::uint32_t face) const noexcept {
        return face < m_faces.size() ? m_faces[face].metrics : FaceMetrics{};
    }
};

#endif

inline float style_size(const text::TextStyle& s) noexcept {
    return s.has_size() && s.size() > 0.0 ? static_cast<float>(s.size()) : 16.0f;
}

inline bool prepare_text(TextEngine& engine, const text::RichText& rt, PreparedText& out) {
    out.utf8.clear();
    out.spans.clear();
    out.styles.clear();

    for (std::size_t i = 0; i < rt.size(); ++i) {
        const text::TextSpan& src = rt.spans()[i];
        const text::TextStyle style = rt.resolved(i);
        std::string t = sanitize_utf8(src.text);
        if (style.has_transform()) t = engine.transform_case(t, style.transform());
        if (t.empty()) continue;
        PreparedSpan sp;
        const text::FontCategory cat = style.has_font() ? text::font_category(style.font()) : text::FontCategory::Default;
        
        std::string name = !src.family.empty() ? src.family
                         : (style.has_font() && style.font() != text::Font::None) ? std::string(text::font_css_name(style.font()))
                         : std::string();

        sp.family = name.empty() ? std::string(generic_family(cat)) : name + ", " + generic_family(cat);

        const float size = style_size(style);
        sp.size = size;

        if (style.has_vertical_align() && style.vertical_align() == text::VerticalAlign::Superscript) {
            sp.size = size * 0.7f;
            sp.rise = -size * 0.35f;
        } else if (style.has_vertical_align() && style.vertical_align() == text::VerticalAlign::Subscript) {
            sp.size = size * 0.7f;
            sp.rise = size * 0.15f;
        }

        sp.weight         = style.has_weight() ? static_cast<int>(style.weight()) : 400;
        sp.slant          = style.has_slant() ? style.slant() : text::FontSlant::Normal;
        sp.letter_spacing = style.has_letter_spacing() ? static_cast<float>(style.letter_spacing()) : 0.0f;
        sp.word_spacing   = style.has_word_spacing() ? static_cast<float>(style.word_spacing()) : 0.0f;
        sp.begin          = static_cast<std::uint32_t>(out.utf8.size());
        out.utf8 += t;
        sp.end            = static_cast<std::uint32_t>(out.utf8.size());
        sp.style          = static_cast<std::uint32_t>(out.styles.size());
        out.styles.push_back(style);
        out.spans.push_back(std::move(sp));
    }

    return !out.spans.empty();
}

inline void truncate_with_ellipsis(const PreparedText& src, std::uint32_t cut, PreparedText& dst) {
    while (cut > 0 && (src.utf8[cut - 1] == ' ' || src.utf8[cut - 1] == '\n' || src.utf8[cut - 1] == '\t')) --cut;
    dst.styles = src.styles;
    dst.spans.clear();
    dst.utf8.assign(src.utf8, 0, cut);

    for (const PreparedSpan& sp : src.spans) {
        if (sp.begin >= cut) break;
        PreparedSpan c = sp;
        c.end = std::min(c.end, cut);
        dst.spans.push_back(c);
    }

    if (dst.spans.empty()) {
        PreparedSpan c = src.spans.front();
        c.begin = c.end = 0;
        dst.spans.push_back(c);
    }

    dst.utf8 += "\xE2\x80\xA6";
    dst.spans.back().end = static_cast<std::uint32_t>(dst.utf8.size());
}

struct TextPoint { float x, y; };

struct TextCmd {
    enum class Kind : std::uint8_t { Rect, Glyph, Wave };

    Kind            kind  = Kind::Rect;
    graphics::Color color;              
    float           x = 0.0f, y = 0.0f; 
    float           w = 0.0f, h = 0.0f; 
    std::uint32_t   face  = 0;
    std::uint32_t   glyph = 0;
    std::uint32_t   wave  = 0;          
};

struct TextDrawList {
    std::vector<TextCmd>                cmds;
    std::vector<std::vector<TextPoint>> waves;
    float    width          = 0.0f;   
    float    height         = 0.0f;
    float    first_baseline = 0.0f;
    float    ascent         = 0.0f;   
    float    descent        = 0.0f;
    unsigned lines          = 0;
    bool     truncated      = false;
    bool     clip           = false;  
    float    clip_w         = 0.0f;
    float    clip_h         = 0.0f;

    void clear() {
        cmds.clear();
        waves.clear();
        width = height = first_baseline = ascent = descent = clip_w = clip_h = 0.0f;
        lines = 0;
        truncated = clip = false;
    }

    text::TextMetrics metrics() const noexcept {
        text::TextMetrics m;
        m.width   = static_cast<unsigned int>(std::ceil(width));
        m.height  = static_cast<unsigned int>(std::ceil(height));
        m.ascent  = static_cast<int>(std::lround(ascent));
        m.descent = static_cast<int>(std::lround(descent));
        return m;
    }
};

namespace text_detail {

inline graphics::Color fade(graphics::Color c, float opacity) noexcept {
    if (opacity < 1.0f) c.set_alpha(static_cast<std::uint8_t>(c.alpha() * std::max(0.0f, opacity) + 0.5f));
    return c;
}

struct Segment {
    std::uint32_t line, span, face;
    float         x0, x1, baseline;  
};

inline void add_rect(TextDrawList& out, float x, float y, float w, float h, const graphics::Color& c) {
    if (w <= 0.0f || h <= 0.0f || c.alpha() == 0) return;
    TextCmd cmd;
    cmd.kind = TextCmd::Kind::Rect;
    cmd.color = c;
    cmd.x = x; cmd.y = y; cmd.w = w; cmd.h = h;
    out.cmds.push_back(cmd);
}

inline void add_line(TextDrawList& out, float x0, float x1, float y, float t, text::DecorationStyle style, const graphics::Color& c) {
    const float len = x1 - x0;
    if (len <= 0.0f || c.alpha() == 0) return;

    switch (style) {
        case text::DecorationStyle::Double:
            add_rect(out, x0, y, len, t, c);
            add_rect(out, x0, y + 2.0f * t, len, t, c);
            break;

        case text::DecorationStyle::Dashed: {
            const float dash = 3.0f * t, gap = 2.0f * t;
            for (float x = x0; x < x1; x += dash + gap) add_rect(out, x, y, std::min(dash, x1 - x), t, c);
            break;
        }

        case text::DecorationStyle::Dotted:
            for (float x = x0; x < x1; x += 2.0f * t) add_rect(out, x, y, std::min(t, x1 - x), t, c);
            break;

        case text::DecorationStyle::Wavy: {
            const float amp = std::max(1.0f, t);
            const float period = std::max(4.0f, 4.0f * t);
            const float step = period / 8.0f;
            const float cy = y + t * 0.5f;
            std::vector<TextPoint> pts;
            for (float x = x0; x < x1; x += step) pts.push_back({ x, cy + amp * std::sin((x - x0) / period * 6.2831853f) });
            pts.push_back({ x1, cy + amp * std::sin((x1 - x0) / period * 6.2831853f) });
            TextCmd cmd;
            cmd.kind  = TextCmd::Kind::Wave;
            cmd.color = c;
            cmd.w     = t;
            cmd.wave  = static_cast<std::uint32_t>(out.waves.size());
            out.waves.push_back(std::move(pts));
            out.cmds.push_back(cmd);
            break;
        }

        default:
            add_rect(out, x0, y, len, t, c);
            break;
    }
}

inline void add_decorations(
    TextDrawList& out, const Segment& s, const text::TextStyle& style, const FaceMetrics& m,
    float rise, float ox, float oy, const graphics::Color& color
) {
    const bool under  = style.is_underlined() || text::has_decoration(style.decoration(), text::TextDecoration::DoubleUnderline);
    const bool dbl    = text::has_decoration(style.decoration(), text::TextDecoration::DoubleUnderline);
    if (!under && !style.is_strikethrough() && !style.is_overlined()) return;
    const text::DecorationStyle ds = style.has_decoration_style() ? style.decoration_style() : text::DecorationStyle::Solid;
    const float base = s.baseline + rise + oy;
    const float x0 = s.x0 + ox, x1 = s.x1 + ox;

    if (under) {
        const float t = std::round(std::max(1.0f, m.underline_thickness));
        const float y = std::round(base - m.underline_position);
        add_line(out, x0, x1, y, t, dbl && ds == text::DecorationStyle::Solid ? text::DecorationStyle::Double : ds, color);
    }

    if (style.is_strikethrough()) {
        const float t = std::round(std::max(1.0f, m.strikeout_thickness));
        add_line(out, x0, x1, std::round(base - m.strikeout_position), t, ds, color);
    }

    if (style.is_overlined()) {
        const float t = std::round(std::max(1.0f, m.underline_thickness));
        add_line(out, x0, x1, std::round(base - m.ascent), t, ds, color);
    }
}

} // namespace text_detail

inline bool build_text_draw_list(TextEngine& engine, const text::RichText& rt, float box_w, float box_h, TextDrawList& out) {
    using namespace text_detail;
    out.clear();
    if (!engine.ready() || rt.empty()) return false;
    PreparedText prepared;
    if (!prepare_text(engine, rt, prepared)) return false;
    const text::TextStyle& para = rt.base();
    const text::TextOverflow overflow = para.has_text_overflow() ? para.text_overflow() : text::TextOverflow::Visible;
    const bool rtl = para.has_direction() && para.direction() == text::WritingDirection::RightToLeft;
    text::TextAlign align = para.has_text_align() ? para.text_align() : (rtl ? text::TextAlign::Right : text::TextAlign::Left);
    ShapeParams sp;
    sp.wrap         = overflow == text::TextOverflow::WordWrap && box_w > 0.0f;
    sp.ellipsize    = overflow == text::TextOverflow::Ellipsis && box_w > 0.0f;
    sp.max_width    = (sp.wrap || sp.ellipsize) ? box_w : 0.0f;
    sp.align        = align;
    sp.justify      = align == text::TextAlign::Justify;
    sp.rtl          = rtl;
    sp.line_spacing = para.has_line_height() && para.line_height() > 0.0 ? static_cast<float>(para.line_height()) : 0.0f;
    sp.indent       = para.has_indent() ? static_cast<float>(para.indent()) : 0.0f;
    const float para_gap = para.has_paragraph_spacing() ? static_cast<float>(para.paragraph_spacing()) : 0.0f;
    ShapedLayout layout;
    if (!engine.shape(prepared, sp, layout) || layout.lines.empty()) return false;
    std::size_t limit = layout.lines.size();
    if (para.has_max_lines()) limit = std::min<std::size_t>(limit, para.max_lines());

    if (box_h > 0.0f && overflow != text::TextOverflow::Visible) {
        std::size_t fit = 0;
        float gap = 0.0f;

        for (std::size_t i = 0; i < layout.lines.size(); ++i) {
            if (layout.lines[i].bottom + gap > box_h + 0.5f) break;
            fit = i + 1;
            if (layout.lines[i].paragraph_end) gap += para_gap;
        }

        limit = std::min(limit, std::max<std::size_t>(fit, 1));
    }

    if (limit < layout.lines.size()) {
        out.truncated = true;

        if (overflow == text::TextOverflow::WordWrap || overflow == text::TextOverflow::Ellipsis) {
            const ShapedLine keep = layout.lines[limit - 1];
            std::vector<std::uint32_t> cuts;

            for (std::uint32_t b = keep.start; b <= keep.end && b <= prepared.utf8.size(); ++b) {
                if (b == prepared.utf8.size() || (static_cast<unsigned char>(prepared.utf8[b]) & 0xC0u) != 0x80u) cuts.push_back(b);
            }

            PreparedText best, trial;
            ShapedLayout best_layout, trial_layout;
            bool found = false;
            std::size_t lo = 0, hi = cuts.size();

            while (lo < hi) {
                const std::size_t mid = (lo + hi) / 2;
                truncate_with_ellipsis(prepared, cuts[mid], trial);

                if (engine.shape(trial, sp, trial_layout) && trial_layout.lines.size() <= limit) {
                    best = trial; best_layout = trial_layout; found = true;
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }

            if (!found) {
                truncate_with_ellipsis(prepared, keep.start, best);
                engine.shape(best, sp, best_layout);
            }

            prepared = std::move(best);
            layout   = std::move(best_layout);
        }

        if (layout.lines.size() > limit) {
            layout.lines.resize(limit);
            layout.glyphs.erase(
                std::remove_if(layout.glyphs.begin(), layout.glyphs.end(), [limit](const ShapedGlyph& g) { return g.line >= limit; }),
                layout.glyphs.end()
            );
        }
    }

    const std::size_t n = layout.lines.size();
    std::vector<float> dx(n, 0.0f), dy(n, 0.0f);
    float widest = 0.0f;
    for (const ShapedLine& L : layout.lines) widest = std::max(widest, L.width);
    const float region = box_w > 0.0f ? box_w : widest;
    float gap = 0.0f;

    for (std::size_t i = 0; i < n; ++i) {
        const ShapedLine& L = layout.lines[i];
        dy[i] = gap;
        if (L.paragraph_end) gap += para_gap;

        if (!sp.wrap) {
            switch (align) {
                case text::TextAlign::Center: dx[i] = (region - L.width) * 0.5f - L.left; break;
                case text::TextAlign::Right:  dx[i] = region - L.width - L.left;          break;
                default:                      dx[i] = -L.left;                            break;
            }
        }

        if (box_w <= 0.0f) {
            if (align == text::TextAlign::Center) dx[i] -= region * 0.5f;
            else if (align == text::TextAlign::Right) dx[i] -= region;
        }
    }

    const float total_h = layout.lines.back().bottom + dy.back();
    float vy = 0.0f;

    if (box_h > 0.0f && para.has_vertical_align()) {
        if (para.vertical_align() == text::VerticalAlign::Middle) vy = (box_h - total_h) * 0.5f;
        else if (para.vertical_align() == text::VerticalAlign::Bottom) vy = box_h - total_h;
    }

    for (float& v : dy) v += vy;
    std::vector<Segment> segs;

    for (const ShapedGlyph& g : layout.glyphs) {
        const float x = g.x + dx[g.line];

        if (segs.empty() || segs.back().line != g.line || segs.back().span != g.span) {
            segs.push_back({ g.line, g.span, g.face, x, x + g.advance, layout.lines[g.line].baseline + dy[g.line] });
        } else {
            segs.back().x0 = std::min(segs.back().x0, x);
            segs.back().x1 = std::max(segs.back().x1, x + g.advance);
        }
    }

    auto style_of = [&](std::uint32_t span) -> const text::TextStyle& { return prepared.styles[prepared.spans[span].style]; };
    auto opacity_of = [&](std::uint32_t span) { const auto& s = style_of(span); return s.has_opacity() ? s.opacity() : 1.0f; };

    auto emit_glyphs = [&](std::uint32_t line, std::uint32_t span, float ox, float oy, const graphics::Color& c) {
        if (c.alpha() == 0) return;
        const float rise = prepared.spans[span].rise;

        for (const ShapedGlyph& g : layout.glyphs) {
            if (g.line != line || g.span != span) continue;
            TextCmd cmd;
            cmd.kind  = TextCmd::Kind::Glyph;
            cmd.color = c;
            cmd.x     = g.x + dx[g.line] + ox;
            cmd.y     = g.y + dy[g.line] + rise + oy;
            cmd.face  = g.face;
            cmd.glyph = g.glyph;
            out.cmds.push_back(cmd);
        }
    };

    for (const Segment& s : segs) {
        const text::TextStyle& st = style_of(s.span);
        if (!st.has_bg_color()) continue;
        const ShapedLine& L = layout.lines[s.line];
        add_rect(out, s.x0, L.top + dy[s.line], s.x1 - s.x0, L.bottom - L.top, fade(st.bg_color().value(), opacity_of(s.span)));
    }

    for (const Segment& s : segs) {
        const text::TextStyle& st = style_of(s.span);
        if (!st.has_shadow()) continue;
        const auto& sh = st.shadow();
        const float ox = static_cast<float>(sh.offset_x), oy = static_cast<float>(sh.offset_y);
        const float blur = static_cast<float>(std::max(0.0, sh.blur));
        const graphics::Color base_c = fade(sh.color, opacity_of(s.span));
        const graphics::Color deco = base_c;

        if (blur < 0.5f) {
            emit_glyphs(s.line, s.span, ox, oy, base_c);
            add_decorations(out, s, st, engine.face_metrics(s.face), prepared.spans[s.span].rise, ox, oy, deco);
        } else {
            const graphics::Color ring = fade(base_c, 0.22f);
            emit_glyphs(s.line, s.span, ox, oy, fade(base_c, 0.45f));
            for (int k = 0; k < 8; ++k) {
                const float a = k * 0.78539816f;
                emit_glyphs(s.line, s.span, ox + std::cos(a) * blur * 0.5f, oy + std::sin(a) * blur * 0.5f, ring);
            }
        }
    }

    for (const Segment& s : segs) {
        const text::TextStyle& st = style_of(s.span);
        if (!st.has_outline_color() || !st.has_outline_width() || st.outline_width() <= 0.0) continue;
        const float r = static_cast<float>(st.outline_width());
        const int steps = r <= 1.5f ? 8 : 16;
        const graphics::Color c = fade(st.outline_color().value(), opacity_of(s.span));

        for (float rr = r; rr > 0.0f; rr -= 1.5f) {
            for (int k = 0; k < steps; ++k) {
                const float a = k * 6.2831853f / steps;
                emit_glyphs(s.line, s.span, std::cos(a) * rr, std::sin(a) * rr, c);
            }
        }
    }

    for (const Segment& s : segs) {
        const text::TextStyle& st = style_of(s.span);
        const graphics::Color fg = st.has_fg_color() ? st.fg_color().value() : graphics::Color(0, 0, 0, 255);
        emit_glyphs(s.line, s.span, 0.0f, 0.0f, fade(fg, opacity_of(s.span)));
    }

    for (const Segment& s : segs) {
        const text::TextStyle& st = style_of(s.span);
        const graphics::Color fg = st.has_fg_color() ? st.fg_color().value() : graphics::Color(0, 0, 0, 255);
        const graphics::Color c = fade(st.has_decoration_color() ? st.decoration_color().value() : fg, opacity_of(s.span));
        add_decorations(out, s, st, engine.face_metrics(s.face), prepared.spans[s.span].rise, 0.0f, 0.0f, c);
    }

    out.width          = widest;
    out.height         = total_h;
    out.lines          = static_cast<unsigned>(n);
    out.first_baseline = layout.lines.front().baseline + dy.front();
    out.ascent         = layout.lines.front().baseline - layout.lines.front().top;
    out.descent        = layout.lines.front().bottom - layout.lines.front().baseline;
    out.clip           = overflow == text::TextOverflow::Clip && box_w > 0.0f && box_h > 0.0f;
    out.clip_w         = box_w;
    out.clip_h         = box_h;
    return true;
}

} // namespace gfx
} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_GPU_TEXT_HPP