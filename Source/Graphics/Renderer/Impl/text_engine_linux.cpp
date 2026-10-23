#include "gpu_text.hpp"

#if defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_ANDROID)

#include "glyph_raster.hpp"
#include "line_break.hpp"
#include "../../../String/unicode.hpp"

#if defined(OS_ANDROID)
    #include "android_fonts.hpp"
#else
    #include <fontconfig/fontconfig.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H

#include <hb.h>
#include <hb-ot.h>

#include <memory>

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

namespace {

constexpr std::int32_t kUnit = 1024;
constexpr float kUnitF = 1024.0f;
constexpr std::uint32_t kLongRun = 4096;

inline std::int32_t round_units(std::int32_t v) noexcept {
    return (v + kUnit / 2) & ~(kUnit - 1);
}

inline std::int32_t to_units(float px) noexcept {
    return static_cast<std::int32_t>(std::lround(static_cast<double>(px) * kUnit));
}

inline float to_px(std::int32_t units) noexcept {
    return static_cast<float>(units) / kUnitF;
}

inline char32_t decode(const std::string& s, std::size_t i, std::size_t& len) noexcept {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { len = 1; return c; }
    len = (c >> 5) == 0x06 ? 2 : (c >> 4) == 0x0E ? 3 : 4;
    if (i + len > s.size()) { len = 1; return 0xFFFD; }
    char32_t cp = c & (0x7Fu >> len);
    for (std::size_t k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
    return cp;
}

enum class Bidi : std::uint8_t { L, R, EN, N, NSM };

inline bool is_rtl_char(char32_t c) noexcept {
    return (c >= 0x0590 && c <= 0x08FF && !(c >= 0x0660 && c <= 0x0669) && !(c >= 0x06F0 && c <= 0x06F9))
        || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF)
        || (c >= 0x10800 && c <= 0x10FFF) || (c >= 0x1E800 && c <= 0x1EFFF) || c == 0x200F || c == 0x061C;
}

inline Bidi bidi_class(char32_t c) noexcept {
    if (is_rtl_char(c)) return Bidi::R;
    if ((c >= '0' && c <= '9') || (c >= 0x0660 && c <= 0x0669) || (c >= 0x06F0 && c <= 0x06F9)) return Bidi::EN;
    if (c == 0x200E) return Bidi::L;
    if (unicode::is_mark(c) || c == 0x200D || c == 0x200C || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF)) return Bidi::NSM;
    if (unicode::is_letter(c) || unicode::is_number(c)) return Bidi::L;
    if (c >= 0x1F000 && c <= 0x1FAFF) return Bidi::L;
    return Bidi::N;
}

inline bool is_space_char(char32_t c) noexcept {
    return c == ' ' || c == 0x3000 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A && c != 0x2007) || c == 0x205F;
}

inline bool is_white(char32_t c) noexcept {
    return is_space_char(c) || c == '\t' || c == 0x00A0 || c == 0x2007 || c == 0x202F;
}

struct FtLibrary {
    FT_Library lib = nullptr;
    FtLibrary() { if (FT_Init_FreeType(&lib) != 0) lib = nullptr; }
    ~FtLibrary() { if (lib) FT_Done_FreeType(lib); }
};

} // namespace

struct TextEngine::Impl {
    struct Source {
        std::string file;
        int         index   = 0;
        FT_Face     ft      = nullptr;
        hb_blob_t*  blob    = nullptr;
        hb_face_t*  hb      = nullptr;
        bool        color   = false;
        bool        bitmap  = false;
        std::vector<hb_variation_t> variations;
    };

    struct Face {
        std::uint32_t source   = 0;
        float         size     = 16.0f;
        bool          embolden = false;
        float         slant    = 0.0f;
        hb_font_t*    hb       = nullptr;
        FT_Size       ft_size  = nullptr;
        float         bitmap_scale = 1.0f;
        std::int32_t  ascent   = 0;
        std::int32_t  descent  = 0;
        std::int32_t  space    = -1;
        FaceMetrics   metrics;
    };

#if defined(OS_ANDROID)
    struct Candidate {
        AndroidFontFile font;
        bool            prepared = false;
        int             source   = -1;
        bool            embolden = false;
        float           slant    = 0.0f;
    };

    struct FontList {
        std::vector<Candidate>              fonts;
        std::unordered_map<char32_t, int>   picks;
    };
#else
    struct Candidate {
        FcPattern*  font     = nullptr;
        FcCharSet*  charset  = nullptr;
        bool        prepared = false;
        int         source   = -1;
        bool        embolden = false;
        float       slant    = 0.0f;
    };

    struct FontList {
        std::vector<Candidate>              fonts;
        FcFontSet*                          set     = nullptr;
        FcPattern*                          pattern = nullptr;
        std::unordered_map<char32_t, int>   picks;
    };
#endif

    struct HbGlyph {
        std::uint32_t glyph;
        std::uint32_t cluster;
        std::int32_t  advance;
        std::int32_t  x_offset;
        std::int32_t  y_offset;
    };

    struct GlyphRec {
        std::uint32_t glyph;
        std::uint32_t face;
        std::int32_t  width;
        std::int32_t  x_offset;
        std::int32_t  y_offset;
    };

    struct Cluster {
        std::uint32_t begin        = 0;
        std::uint32_t end          = 0;
        std::uint32_t glyph_begin  = 0;
        std::uint32_t glyph_end    = 0;
        std::uint32_t face         = 0;
        std::int32_t  width        = 0;
        std::int32_t  space_left   = 0;
        std::int32_t  space_right  = 0;
        std::uint8_t  level        = 0;
        bool          white        = false;
        bool          expandable   = false;
        bool          tab          = false;
        bool          break_before = false;
        std::uint32_t run          = 0;
    };

    struct RunInfo {
        std::uint32_t begin  = 0;
        std::uint32_t end    = 0;
        std::uint32_t face   = 0;
        hb_script_t   script = HB_SCRIPT_COMMON;
        std::uint8_t  level  = 0;
    };

    struct LineRange {
        std::uint32_t first = 0;
        std::uint32_t last  = 0;
        bool          wrapped = false;
        bool          first_of_paragraph = false;
    };

    FtLibrary                                         ft;
    std::vector<std::unique_ptr<Source>>              sources;
    std::unordered_map<std::string, std::uint32_t>    source_ids;
    std::vector<Face>                                 faces;
    struct FaceKey {
        std::uint32_t source;
        std::uint32_t size;
        std::uint32_t slant;
        bool          embolden;
        bool operator==(const FaceKey& o) const noexcept { return source == o.source && size == o.size && slant == o.slant && embolden == o.embolden; }
    };

    struct FaceKeyHash {
        std::size_t operator()(const FaceKey& k) const noexcept {
            std::uint64_t h = k.source;
            h = h * 0x9E3779B97F4A7C15ull ^ k.size;
            h = h * 0x9E3779B97F4A7C15ull ^ k.slant;
            return static_cast<std::size_t>(h * 0x9E3779B97F4A7C15ull ^ (k.embolden ? 1u : 0u));
        }
    };

    std::unordered_map<FaceKey, std::uint32_t, FaceKeyHash> face_ids;
    std::unordered_map<std::string, std::unique_ptr<FontList>> lists;
    std::unordered_map<std::string, std::vector<HbGlyph>>      shapes;
    hb_buffer_t*                                      buffer = nullptr;
    OutlineRasterizer                                 raster;
    bool                                              ok = false;

    std::vector<char32_t>                             cps;
    std::vector<std::uint32_t>                        cp_bytes;
    std::vector<std::uint32_t>                        cp_span;
    std::vector<std::int32_t>                         cp_face;
    std::vector<std::uint8_t>                         cp_level;
    std::vector<hb_script_t>                          cp_script;
    std::vector<std::uint8_t>                         byte_break;
    std::vector<std::uint8_t>                         cp_break;
    std::vector<std::uint8_t>                         cp_emoji;
    std::string                                       context_scratch;
    std::vector<Cluster>                              clusters;
    std::vector<GlyphRec>                             glyphs;
    std::vector<RunInfo>                              runs;
    std::vector<Cluster>                              line_clusters;
    std::vector<GlyphRec>                             line_glyphs;
    std::string                                       key_scratch;

    ~Impl() {
        for (auto& [k, l] : lists) release_list(*l);
        lists.clear();
        for (Face& f : faces) {
            if (f.hb) hb_font_destroy(f.hb);
            if (f.ft_size) FT_Done_Size(f.ft_size);
        }
        faces.clear();
        for (auto& s : sources) {
            if (s->hb) hb_face_destroy(s->hb);
            if (s->ft) FT_Done_Face(s->ft);
            if (s->blob) hb_blob_destroy(s->blob);
        }
        sources.clear();
        if (buffer) hb_buffer_destroy(buffer);
    }

#if defined(OS_ANDROID)
    static void release_list(FontList& l) {
        l.fonts.clear();
        l.picks.clear();
    }

    bool init() {
        if (ok) return true;
        if (!ft.lib) return false;
        buffer = hb_buffer_create();
        ok = hb_buffer_allocation_successful(buffer);
        return ok;
    }

    FontList& list_for(const std::string& family, int weight, text::FontSlant slant) {
        std::string key = family;
        key += '\x1f';
        key += std::to_string(weight);
        key += '\x1f';
        key += static_cast<char>('0' + static_cast<int>(slant));
        auto it = lists.find(key);
        if (it != lists.end()) return *it->second;
        auto list = std::make_unique<FontList>();
        const bool italic = slant != text::FontSlant::Normal;
        for (const AndroidFontPick& p : AndroidFontCatalog::instance().candidates(family, weight, italic)) {
            if (!p.font) continue;
            Candidate c;
            c.font = *p.font;
            c.embolden = p.embolden;
            c.slant = p.slant;
            list->fonts.push_back(std::move(c));
        }
        FontList& ref = *list;
        lists.emplace(std::move(key), std::move(list));
        return ref;
    }

    void prepare(FontList&, Candidate& c) {
        if (c.prepared) return;
        c.prepared = true;
        c.source = source_for(c.font.path, c.font.index, c.font.axes);
    }

    bool covers(FontList& list, Candidate& c, char32_t cp) {
        prepare(list, c);
        if (c.source < 0) return false;
        return FT_Get_Char_Index(sources[static_cast<std::size_t>(c.source)]->ft, cp) != 0;
    }
#else
    static void release_list(FontList& l) {
        for (Candidate& c : l.fonts) if (c.prepared && c.font) FcPatternDestroy(c.font);
        if (l.set) FcFontSetDestroy(l.set);
        if (l.pattern) FcPatternDestroy(l.pattern);
        l.fonts.clear();
        l.set = nullptr;
        l.pattern = nullptr;
    }

    bool init() {
        if (ok) return true;
        if (!ft.lib || !FcInit()) return false;
        buffer = hb_buffer_create();
        ok = hb_buffer_allocation_successful(buffer);
        return ok;
    }

    static int fc_slant(text::FontSlant s) noexcept {
        return s == text::FontSlant::Italic ? FC_SLANT_ITALIC : s == text::FontSlant::Oblique ? FC_SLANT_OBLIQUE : FC_SLANT_ROMAN;
    }

    FontList& list_for(const std::string& family, int weight, text::FontSlant slant) {
        std::string key = family;
        key += '\x1f';
        key += std::to_string(weight);
        key += '\x1f';
        key += static_cast<char>('0' + static_cast<int>(slant));
        auto it = lists.find(key);
        if (it != lists.end()) return *it->second;
        auto list = std::make_unique<FontList>();
        FcPattern* pat = FcPatternCreate();
        std::size_t at = 0;
        while (at <= family.size()) {
            std::size_t comma = family.find(',', at);
            if (comma == std::string::npos) comma = family.size();
            std::string name = family.substr(at, comma - at);
            while (!name.empty() && (name.front() == ' ' || name.front() == '"' || name.front() == '\'')) name.erase(name.begin());
            while (!name.empty() && (name.back() == ' ' || name.back() == '"' || name.back() == '\'')) name.pop_back();
            if (!name.empty()) FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8*>(name.c_str()));
            at = comma + 1;
        }
        FcPatternAddDouble(pat, FC_WEIGHT, FcWeightFromOpenTypeDouble(static_cast<double>(weight)));
        FcPatternAddInteger(pat, FC_SLANT, fc_slant(slant));
        FcConfigSubstitute(nullptr, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);
        FcResult result = FcResultNoMatch;
        FcFontSet* set = FcFontSort(nullptr, pat, FcTrue, nullptr, &result);
        list->pattern = pat;
        list->set = set;
        if (set) {
            for (int i = 0; i < set->nfont; ++i) {
                Candidate c;
                c.font = set->fonts[i];
                FcCharSet* cs = nullptr;
                if (FcPatternGetCharSet(set->fonts[i], FC_CHARSET, 0, &cs) == FcResultMatch) c.charset = cs;
                list->fonts.push_back(c);
            }
        }
        FontList& ref = *list;
        lists.emplace(std::move(key), std::move(list));
        return ref;
    }

#endif

    int source_for(const std::string& file, int index) {
        return source_for(file, index, {});
    }

    int source_for(const std::string& file, int index, const std::vector<std::pair<std::uint32_t, float>>& axes) {
        std::string key = file;
        key += '#';
        key += std::to_string(index);
        for (const auto& [tag, value] : axes) {
            key += '|';
            key += std::to_string(tag);
            key += '=';
            key += std::to_string(static_cast<int>(std::lround(value * 100.0f)));
        }
        auto it = source_ids.find(key);
        if (it != source_ids.end()) return static_cast<int>(it->second);
        auto src = std::make_unique<Source>();
        src->file = file;
        src->index = index;
        src->blob = hb_blob_create_from_file(file.c_str());
        if (!src->blob || hb_blob_get_length(src->blob) == 0) {
            if (src->blob) hb_blob_destroy(src->blob);
            source_ids.emplace(key, 0xFFFFFFFFu);
            return -1;
        }
        unsigned int length = 0;
        const char* data = hb_blob_get_data(src->blob, &length);
        if (!data || FT_New_Memory_Face(ft.lib, reinterpret_cast<const FT_Byte*>(data), static_cast<FT_Long>(length), index, &src->ft) != 0) {
            hb_blob_destroy(src->blob);
            source_ids.emplace(key, 0xFFFFFFFFu);
            return -1;
        }
        src->hb = hb_face_create(src->blob, static_cast<unsigned int>(index & 0xFFFF));
        if (!axes.empty()) apply_axes(*src, axes);
        src->color = FT_HAS_COLOR(src->ft);
        bool vector_color = hb_ot_color_has_layers(src->hb) || hb_ot_color_has_svg(src->hb);
#if HB_VERSION_ATLEAST(7, 0, 0)
        vector_color = vector_color || hb_ot_color_has_paint(src->hb);
#endif
        src->bitmap = !FT_IS_SCALABLE(src->ft) || (src->color && src->ft->num_fixed_sizes > 0 && !vector_color);
        sources.push_back(std::move(src));
        const std::uint32_t id = static_cast<std::uint32_t>(sources.size() - 1);
        source_ids.emplace(std::move(key), id);
        return static_cast<int>(id);
    }

    void apply_axes(Source& src, const std::vector<std::pair<std::uint32_t, float>>& axes) {
        for (const auto& [tag, value] : axes) {
            hb_variation_t v;
            v.tag = tag;
            v.value = value;
            src.variations.push_back(v);
        }
        if (!FT_HAS_MULTIPLE_MASTERS(src.ft)) return;
        FT_MM_Var* mm = nullptr;
        if (FT_Get_MM_Var(src.ft, &mm) != 0 || !mm) return;
        std::vector<FT_Fixed> coords(mm->num_axis);
        for (FT_UInt i = 0; i < mm->num_axis; ++i) {
            coords[i] = mm->axis[i].def;
            for (const auto& [tag, value] : axes) {
                if (static_cast<std::uint32_t>(mm->axis[i].tag) != tag) continue;
                const FT_Fixed v = static_cast<FT_Fixed>(std::lround(value * 65536.0f));
                coords[i] = std::max(mm->axis[i].minimum, std::min(mm->axis[i].maximum, v));
            }
        }
        FT_Set_Var_Design_Coordinates(src.ft, mm->num_axis, coords.data());
        FT_Done_MM_Var(ft.lib, mm);
    }

#if !defined(OS_ANDROID)
    void prepare(FontList& list, Candidate& c) {
        if (c.prepared) return;
        c.prepared = true;
        FcPattern* ready = FcFontRenderPrepare(nullptr, list.pattern, c.font);
        c.font = ready;
        if (!ready) return;
        FcChar8* file = nullptr;
        int index = 0;
        if (FcPatternGetString(ready, FC_FILE, 0, &file) != FcResultMatch || !file) return;
        FcPatternGetInteger(ready, FC_INDEX, 0, &index);
        FcBool embolden = FcFalse;
        if (FcPatternGetBool(ready, FC_EMBOLDEN, 0, &embolden) == FcResultMatch) c.embolden = embolden == FcTrue;
        FcMatrix* m = nullptr;
        if (FcPatternGetMatrix(ready, FC_MATRIX, 0, &m) == FcResultMatch && m && std::fabs(m->xy) > 0.001) c.slant = static_cast<float>(m->xy);
        c.source = source_for(reinterpret_cast<const char*>(file), index);
    }

    bool covers(FontList& list, Candidate& c, char32_t cp) {
        if (c.charset) return FcCharSetHasChar(c.charset, cp) == FcTrue;
        prepare(list, c);
        if (c.source < 0) return false;
        return FT_Get_Char_Index(sources[static_cast<std::size_t>(c.source)]->ft, cp) != 0;
    }
#endif

    int pick(FontList& list, char32_t cp) {
        auto it = list.picks.find(cp);
        if (it != list.picks.end()) return it->second;
        int chosen = -1;
        for (std::size_t i = 0; i < list.fonts.size(); ++i) {
            if (!covers(list, list.fonts[i], cp)) continue;
            prepare(list, list.fonts[i]);
            if (list.fonts[i].source < 0) continue;
            chosen = static_cast<int>(i);
            break;
        }
        if (chosen < 0) {
            for (std::size_t i = 0; i < list.fonts.size(); ++i) {
                prepare(list, list.fonts[i]);
                if (list.fonts[i].source >= 0) { chosen = static_cast<int>(i); break; }
            }
        }
        list.picks.emplace(cp, chosen);
        return chosen;
    }

    std::int32_t face_for(FontList& list, int candidate, float size) {
        if (candidate < 0) return -1;
        Candidate& c = list.fonts[static_cast<std::size_t>(candidate)];
        prepare(list, c);
        if (c.source < 0) return -1;
        return static_cast<std::int32_t>(face_id(static_cast<std::uint32_t>(c.source), size, c.embolden, c.slant));
    }

    std::uint32_t face_id(std::uint32_t source, float size, bool embolden, float slant) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &size, sizeof(bits));
        std::uint32_t slant_bits = 0;
        std::memcpy(&slant_bits, &slant, sizeof(slant_bits));
        const FaceKey key{ source, bits, slant_bits, embolden };
        auto it = face_ids.find(key);
        if (it != face_ids.end()) return it->second;
        Source& src = *sources[source];
        Face f;
        f.source = source;
        f.size = size;
        f.embolden = embolden;
        f.slant = slant;
        f.hb = hb_font_create(src.hb);
        if (!src.variations.empty()) hb_font_set_variations(f.hb, src.variations.data(), static_cast<unsigned int>(src.variations.size()));
#if HB_VERSION_ATLEAST(3, 3, 0)
        if (slant != 0.0f) hb_font_set_synthetic_slant(f.hb, slant);
#endif
        if ((src.index >> 16) != 0) hb_font_set_var_named_instance(f.hb, static_cast<unsigned int>((src.index >> 16) - 1));
        const int scale = std::max(1, to_units(size));
        hb_font_set_scale(f.hb, scale, scale);
        if (FT_New_Size(src.ft, &f.ft_size) == 0) {
            FT_Activate_Size(f.ft_size);
            if (src.bitmap && src.ft->num_fixed_sizes > 0) {
                int best = 0;
                for (int i = 0; i < src.ft->num_fixed_sizes; ++i) {
                    const float ppem = src.ft->available_sizes[i].y_ppem / 64.0f;
                    const float best_ppem = src.ft->available_sizes[best].y_ppem / 64.0f;
                    if ((ppem >= size && (best_ppem < size || ppem < best_ppem)) || (best_ppem < size && ppem > best_ppem)) best = i;
                }
                FT_Select_Size(src.ft, best);
                const float ppem = src.ft->available_sizes[best].y_ppem / 64.0f;
                f.bitmap_scale = ppem > 0.0f ? size / ppem : 1.0f;
            } else {
                FT_Set_Char_Size(src.ft, 0, static_cast<FT_F26Dot6>(std::lround(size * 64.0f)), 72, 72);
            }
        }
        hb_font_extents_t ext{};
        hb_font_get_h_extents(f.hb, &ext);
        f.ascent = ext.ascender;
        f.descent = -ext.descender;
        f.metrics.ascent = to_px(f.ascent);
        f.metrics.descent = to_px(f.descent);
        auto metric = [&](hb_ot_metrics_tag_t tag, std::int32_t fallback) {
            hb_position_t pos = 0;
            if (hb_ot_metrics_get_position(f.hb, tag, &pos) && pos != 0) return static_cast<std::int32_t>(pos);
            return fallback;
        };
        const std::int32_t em = to_units(size);
        const std::int32_t thickness = metric(HB_OT_METRICS_TAG_UNDERLINE_SIZE, em / 14);
        f.metrics.underline_position = to_px(metric(HB_OT_METRICS_TAG_UNDERLINE_OFFSET, -em / 10));
        f.metrics.underline_thickness = std::max(1.0f, to_px(thickness));
        f.metrics.strikeout_position = to_px(metric(HB_OT_METRICS_TAG_STRIKEOUT_OFFSET, f.ascent / 2));
        f.metrics.strikeout_thickness = std::max(1.0f, to_px(metric(HB_OT_METRICS_TAG_STRIKEOUT_SIZE, thickness)));
        faces.push_back(f);
        const std::uint32_t id = static_cast<std::uint32_t>(faces.size() - 1);
        face_ids.emplace(key, id);
        return id;
    }

    std::int32_t space_width(std::uint32_t face) {
        Face& f = faces[face];
        if (f.space >= 0) return f.space;
        hb_codepoint_t g = 0;
        std::int32_t w = 0;
        if (hb_font_get_nominal_glyph(f.hb, ' ', &g)) w = hb_font_get_glyph_h_advance(f.hb, g);
        else w = to_units(f.size * 0.25f);
        f.space = round_units(w);
        return f.space;
    }

    const std::vector<HbGlyph>& shape_piece(std::uint32_t face, hb_script_t script, bool rtl, const char* text, std::size_t len,
                                            const char* pre = nullptr, std::size_t pre_len = 0, const char* post = nullptr, std::size_t post_len = 0) {
        key_scratch.clear();
        key_scratch.append(reinterpret_cast<const char*>(&face), sizeof(face));
        key_scratch.append(reinterpret_cast<const char*>(&script), sizeof(script));
        key_scratch += rtl ? 'r' : 'l';
        key_scratch += static_cast<char>(pre_len);
        key_scratch += static_cast<char>(post_len);
        if (pre_len) key_scratch.append(pre, pre_len);
        if (post_len) key_scratch.append(post, post_len);
        key_scratch.append(text, len);
        auto it = shapes.find(key_scratch);
        if (it != shapes.end()) return it->second;
        if (shapes.size() > 65536) shapes.clear();
        context_scratch.clear();
        if (pre_len) context_scratch.append(pre, pre_len);
        context_scratch.append(text, len);
        if (post_len) context_scratch.append(post, post_len);
        hb_buffer_clear_contents(buffer);
        hb_buffer_add_utf8(buffer, context_scratch.data(), static_cast<int>(context_scratch.size()), static_cast<unsigned int>(pre_len), static_cast<int>(len));
        hb_buffer_set_direction(buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, script);
        hb_buffer_set_language(buffer, hb_language_get_default());
        hb_shape(faces[face].hb, buffer, nullptr, 0);
        unsigned int n = 0;
        const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &n);
        const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, nullptr);
        std::vector<HbGlyph> out(n);
        for (unsigned int i = 0; i < n; ++i) {
            out[i].glyph = info[i].codepoint;
            out[i].cluster = info[i].cluster - static_cast<std::uint32_t>(pre_len);
            out[i].advance = pos[i].x_advance;
            out[i].x_offset = pos[i].x_offset;
            out[i].y_offset = -pos[i].y_offset;
        }
        return shapes.emplace(key_scratch, std::move(out)).first->second;
    }

    const std::vector<HbGlyph>& shape_hyphenated(std::uint32_t face, hb_script_t script, bool rtl, const char* text, std::size_t len, const char* pre, std::size_t pre_len) {
        key_scratch.clear();
        key_scratch.append(reinterpret_cast<const char*>(&face), sizeof(face));
        key_scratch.append(reinterpret_cast<const char*>(&script), sizeof(script));
        key_scratch += rtl ? 'R' : 'L';
        key_scratch += static_cast<char>(pre_len);
        if (pre_len) key_scratch.append(pre, pre_len);
        key_scratch.append(text, len);
        auto it = shapes.find(key_scratch);
        if (it != shapes.end()) return it->second;
        if (shapes.size() > 65536) shapes.clear();
        context_scratch.assign(pre, pre_len);
        context_scratch.append(text, len);
        hb_buffer_clear_contents(buffer);
        hb_buffer_add_utf8(buffer, context_scratch.data(), static_cast<int>(context_scratch.size()), static_cast<unsigned int>(pre_len), static_cast<int>(len));
        std::size_t last = len;
        while (last > 0 && (static_cast<unsigned char>(text[last - 1]) & 0xC0u) == 0x80u) --last;
        if (last > 0) --last;
        while (last > 0 && (static_cast<unsigned char>(text[last]) & 0xC0u) == 0x80u) --last;
        hb_codepoint_t probe = 0;
        const hb_codepoint_t mark = hb_font_get_nominal_glyph(faces[face].hb, 0x2010, &probe) ? 0x2010 : '-';
        hb_buffer_add(buffer, mark, static_cast<unsigned int>(pre_len + last));
        hb_buffer_set_direction(buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, script);
        hb_buffer_set_language(buffer, hb_language_get_default());
        hb_shape(faces[face].hb, buffer, nullptr, 0);
        unsigned int n = 0;
        const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &n);
        const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, nullptr);
        std::vector<HbGlyph> out(n);
        for (unsigned int i = 0; i < n; ++i) {
            out[i].glyph = info[i].codepoint;
            out[i].cluster = info[i].cluster - static_cast<std::uint32_t>(pre_len);
            out[i].advance = pos[i].x_advance;
            out[i].x_offset = pos[i].x_offset;
            out[i].y_offset = -pos[i].y_offset;
        }
        return shapes.emplace(key_scratch, std::move(out)).first->second;
    }

    static std::size_t prev_cp_start(const std::string& s, std::size_t at) noexcept {
        if (at == 0) return 0;
        std::size_t b = at - 1;
        while (b > 0 && (static_cast<unsigned char>(s[b]) & 0xC0u) == 0x80u) --b;
        return b;
    }

    static std::size_t next_cp_end(const std::string& s, std::size_t at) noexcept {
        if (at >= s.size()) return at;
        std::size_t len = 1;
        decode(s, at, len);
        return std::min(s.size(), at + len);
    }

    static bool same_font(const PreparedSpan& a, const PreparedSpan& b) noexcept {
        return a.family == b.family && a.size == b.size && a.weight == b.weight && a.slant == b.slant
            && a.letter_spacing == b.letter_spacing && a.word_spacing == b.word_spacing;
    }

    bool emoji_cluster(std::size_t first, std::size_t last) const noexcept {
        const char32_t base = cps[first];
        bool text_vs = false;
        bool emoji_vs = false;
        for (std::size_t i = first; i < last; ++i) {
            if (cps[i] == 0xFE0F) emoji_vs = true;
            if (cps[i] == 0xFE0E) text_vs = true;
            if (i > first && (cps[i] == 0x200D || (cps[i] >= 0x1F3FB && cps[i] <= 0x1F3FF) || cps[i] == 0x20E3)) emoji_vs = true;
        }
        if (text_vs) return false;
        if (emoji_vs) return true;
        if (unicode::is_regional_indicator(base)) return true;
        return unicode::is_emoji_presentation(base);
    }

    void itemize(const PreparedText& text, const ShapeParams& p) {
        const std::string& s = text.utf8;
        cps.clear();
        cp_bytes.clear();
        for (std::size_t i = 0; i < s.size();) {
            std::size_t len = 1;
            cps.push_back(decode(s, i, len));
            cp_bytes.push_back(static_cast<std::uint32_t>(i));
            i += len;
        }
        const std::size_t n = cps.size();
        cp_span.assign(n, 0);
        cp_face.assign(n, -1);
        cp_level.assign(n, p.rtl ? 1 : 0);
        cp_script.assign(n, HB_SCRIPT_COMMON);
        std::uint32_t span = 0;
        for (std::size_t i = 0; i < n; ++i) {
            while (span + 1 < text.spans.size() && text.spans[span + 1].begin <= cp_bytes[i]) ++span;
            while (span + 1 < text.spans.size() && text.spans[span].end <= cp_bytes[i]) ++span;
            cp_span[i] = span;
        }

        const StringView view(s.data(), s.size());
        cp_emoji.assign(n, 0);
        std::size_t ci = 0;
        while (ci < n) {
            const std::size_t gb = cp_bytes[ci];
            std::size_t ge = unicode::next_grapheme_break(view, gb);
            if (ge <= gb) ge = gb + 1;
            std::size_t cj = ci + 1;
            while (cj < n && cp_bytes[cj] < ge) ++cj;
            if (emoji_cluster(ci, cj)) {
                for (std::size_t k = ci; k < cj; ++k) cp_emoji[k] = 1;
            }
            ci = cj;
        }

        hb_unicode_funcs_t* uf = hb_unicode_funcs_get_default();
        hb_script_t last = HB_SCRIPT_INVALID;
        for (std::size_t i = 0; i < n; ++i) {
            hb_script_t sc = hb_unicode_script(uf, cps[i]);
            if (sc == HB_SCRIPT_COMMON || sc == HB_SCRIPT_INHERITED || sc == HB_SCRIPT_UNKNOWN) sc = last;
            cp_script[i] = sc;
            if (sc != HB_SCRIPT_INVALID) last = sc;
        }
        hb_script_t next = HB_SCRIPT_COMMON;
        for (std::size_t i = n; i-- > 0;) {
            if (cp_script[i] == HB_SCRIPT_INVALID) cp_script[i] = next;
            else next = cp_script[i];
        }

        const std::uint8_t base_level = p.rtl ? 1 : 0;
        std::vector<Bidi> cls(n);
        bool any_rtl = p.rtl;
        for (std::size_t i = 0; i < n; ++i) {
            cls[i] = bidi_class(cps[i]);
            if (cls[i] == Bidi::NSM) cls[i] = i > 0 && cps[i - 1] != '\n' ? cls[i - 1] : Bidi::N;
            if (cls[i] == Bidi::R) any_rtl = true;
        }
        if (!any_rtl) return;
        std::size_t para = 0;
        while (para < n) {
            std::size_t end = para;
            while (end < n && cps[end] != '\n') ++end;
            Bidi strong = p.rtl ? Bidi::R : Bidi::L;
            for (std::size_t i = para; i < end; ++i) {
                if (cls[i] == Bidi::L || cls[i] == Bidi::R) strong = cls[i];
                else if (cls[i] == Bidi::EN) cls[i] = strong == Bidi::L ? Bidi::L : Bidi::EN;
            }
            for (std::size_t i = para; i < end; ++i) {
                if (cls[i] != Bidi::N) continue;
                std::size_t j = i;
                while (j < end && cls[j] == Bidi::N) ++j;
                const Bidi before = i == para ? (p.rtl ? Bidi::R : Bidi::L) : (cls[i - 1] == Bidi::EN ? Bidi::R : cls[i - 1]);
                const Bidi after = j == end ? (p.rtl ? Bidi::R : Bidi::L) : (cls[j] == Bidi::EN ? Bidi::R : cls[j]);
                const Bidi resolved = before == after ? before : (p.rtl ? Bidi::R : Bidi::L);
                for (std::size_t k = i; k < j; ++k) cls[k] = resolved == Bidi::R ? Bidi::R : Bidi::L;
                i = j - 1;
            }
            for (std::size_t i = para; i < end; ++i) {
                switch (cls[i]) {
                    case Bidi::R:  cp_level[i] = 1; break;
                    case Bidi::EN: cp_level[i] = 2; break;
                    default:       cp_level[i] = base_level == 1 ? 2 : 0; break;
                }
            }
            if (end < n) cp_level[end] = base_level;
            para = end + 1;
        }
    }

    static bool consider_as_space(char32_t c) noexcept {
        if (c < 0x20 || (c >= 0x7F && c <= 0x9F)) return true;
        if (c == 0x2028 || c == 0x2029) return true;
        if (c == ' ' || c == 0x00A0 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000) return true;
        if ((c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF)) return true;
        if (c == 0x00AD || (c >= 0x200B && c <= 0x200F) || (c >= 0x202A && c <= 0x202E) || (c >= 0x2060 && c <= 0x2064)
            || (c >= 0x2066 && c <= 0x206F) || c == 0xFEFF || (c >= 0xFFF9 && c <= 0xFFFB) || (c >= 0xE0001 && c <= 0xE007F)) return true;
        return false;
    }

    void assign_fonts(const PreparedText& text) {
        const std::size_t n = cps.size();
        std::size_t run_start = 0;
        while (run_start < n) {
            if (cps[run_start] == '\n') {
                ++run_start;
                continue;
            }
            const PreparedSpan& sp0 = text.spans[cp_span[run_start]];
            std::size_t run_end = run_start + 1;
            while (run_end < n && cps[run_end] != '\n' && cp_emoji[run_end] == cp_emoji[run_start] && cp_level[run_end] == cp_level[run_start]
                   && cp_script[run_end] == cp_script[run_start]
                   && (cp_span[run_end] == cp_span[run_start] || same_font(text.spans[cp_span[run_end]], sp0))) ++run_end;
            FontList& fl = cp_emoji[run_start] ? list_for("emoji", sp0.weight, sp0.slant) : list_for(sp0.family, sp0.weight, sp0.slant);
            std::size_t item_start = run_start;
            int item_font = -1;
            int item_pos = 0xFFFF;
            std::size_t first_space = n;
            bool last_forced = false;
            for (std::size_t k = run_start; k < run_end; ++k) {
                const char32_t c = cps[k];
                const bool forced = c == '\t' || c == 0x2028;
                const bool space = consider_as_space(c);
                int font = -1;
                int pos = 0xFFFF;
                if (!space) {
                    pos = pick(fl, c);
                    font = pos;
                }
                const std::size_t trailing = first_space;
                if (space) {
                    if (first_space == n) first_space = k;
                } else {
                    first_space = n;
                }
                if (item_font < 0 && font >= 0) {
                    item_font = font;
                    item_pos = pos;
                    const std::int32_t f = face_for(fl, font, sp0.size);
                    for (std::size_t q = item_start; q < k; ++q) cp_face[q] = f;
                } else if (item_font >= 0 && font < 0) {
                    font = item_font;
                    pos = item_pos;
                }
                if (!(forced || last_forced) && font == item_font) {
                    cp_face[k] = font >= 0 ? face_for(fl, font, sp0.size) : -1;
                    last_forced = forced;
                    continue;
                }
                std::size_t new_start = k;
                if (k > item_start && trailing < k && trailing > item_start && pos < item_pos && font >= 0) new_start = trailing;
                if (item_font < 0) {
                    const int base = pick(fl, ' ');
                    const std::int32_t f = face_for(fl, base, sp0.size);
                    for (std::size_t q = item_start; q < new_start; ++q) cp_face[q] = f;
                }
                item_start = new_start;
                item_font = font;
                item_pos = pos;
                const std::int32_t f = font >= 0 ? face_for(fl, font, sp0.size) : -1;
                for (std::size_t q = new_start; q <= k; ++q) cp_face[q] = f;
                last_forced = forced;
            }
            if (item_font < 0) {
                const std::int32_t f = face_for(fl, pick(fl, ' '), sp0.size);
                for (std::size_t q = item_start; q < run_end; ++q) cp_face[q] = f;
            }
            run_start = run_end;
        }
        for (std::size_t k = 0; k < n; ++k) {
            if (cp_face[k] >= 0) continue;
            const PreparedSpan& sp = text.spans[cp_span[k]];
            FontList& fl = list_for(sp.family, sp.weight, sp.slant);
            cp_face[k] = face_for(fl, pick(fl, 'a'), sp.size);
            if (cp_face[k] < 0) {
                FontList& any = list_for("sans-serif", 400, text::FontSlant::Normal);
                cp_face[k] = face_for(any, pick(any, 'a'), sp.size);
            }
        }
    }

    void compute_breaks(const std::string& s) {
        byte_break.assign(s.size() + 1, 0);
        find_line_breaks(cps.data(), cps.size(), cp_break);
        for (std::size_t i = 1; i < cps.size(); ++i) {
            if (cp_break[i]) byte_break[cp_bytes[i]] = 1;
        }
    }

    static void letter_space(Cluster& c, float ls, std::vector<GlyphRec>& pool) {
        if (ls == 0.0f || c.glyph_end == c.glyph_begin) return;
        const std::int32_t units = to_units(ls);
        std::int32_t left = units / 2;
        if ((units & (kUnit - 1)) == 0) left = round_units(left);
        c.space_left = left;
        c.space_right = units - left;
        pool[c.glyph_begin].x_offset += left;
        pool[c.glyph_begin].width += left;
        pool[c.glyph_end - 1].width += c.space_right;
        c.width += units;
    }

    void shape_segment(const PreparedText& text, std::uint32_t run, std::uint32_t seg, std::uint32_t seg_end, bool hyphen,
                       std::vector<Cluster>& out, std::vector<GlyphRec>& pool) {
        const std::string& s = text.utf8;
        const RunInfo& ri = runs[run];
        const bool rtl = (ri.level & 1) != 0;
        std::size_t pre_at = seg;
        for (int k = 0; k < 5 && pre_at > 0 && s[pre_at - 1] != '\n'; ++k) pre_at = prev_cp_start(s, pre_at);
        std::size_t post_end = seg_end;
        if (!hyphen) {
            for (int k = 0; k < 5 && post_end < s.size() && s[post_end] != '\n'; ++k) post_end = next_cp_end(s, post_end);
        }
        const std::vector<HbGlyph>& hg = hyphen ? shape_hyphenated(ri.face, ri.script, rtl, s.data() + seg, seg_end - seg, s.data() + pre_at, seg - pre_at)
                                                : shape_piece(ri.face, ri.script, rtl, s.data() + seg, seg_end - seg, s.data() + pre_at, seg - pre_at, s.data() + seg_end, post_end - seg_end);
        const std::size_t count = hg.size();
        std::size_t k = 0;
        const std::size_t piece_first = out.size();
        while (k < count) {
            const std::size_t at = rtl ? count - 1 - k : k;
            const std::uint32_t cl = hg[at].cluster;
            std::size_t m = k;
            while (m < count && hg[rtl ? count - 1 - m : m].cluster == cl) ++m;
            Cluster c;
            c.begin = seg + cl;
            c.face = ri.face;
            c.level = ri.level;
            c.run = run;
            c.glyph_begin = static_cast<std::uint32_t>(pool.size());
            const std::size_t lo = rtl ? count - m : k;
            const std::size_t hi = rtl ? count - k : m;
            for (std::size_t g = lo; g < hi; ++g) {
                GlyphRec r;
                r.glyph = hg[g].glyph;
                r.face = ri.face;
                r.width = round_units(hg[g].advance);
                r.x_offset = round_units(hg[g].x_offset);
                r.y_offset = round_units(hg[g].y_offset);
                c.width += r.width;
                pool.push_back(r);
            }
            c.glyph_end = static_cast<std::uint32_t>(pool.size());
            out.push_back(c);
            k = m;
        }
        for (std::size_t q = piece_first; q < out.size(); ++q) {
            Cluster& c = out[q];
            c.end = q + 1 < out.size() ? out[q + 1].begin : seg_end;
            std::size_t len = 1;
            const char32_t ch = decode(s, c.begin, len);
            c.white = is_white(ch);
            c.expandable = ch == ' ' || ch == 0x00A0 || ch == 0x3000;
            c.break_before = byte_break[c.begin] != 0;
            const PreparedSpan& sp = text.spans[text.span_at(c.begin)];
            float ls = sp.letter_spacing;
            if (sp.word_spacing != 0.0f && (ch == ' ' || ch == 0x00A0)) ls += sp.word_spacing;
            letter_space(c, ls, pool);
        }
    }

    void build_clusters(const PreparedText& text) {
        clusters.clear();
        glyphs.clear();
        runs.clear();
        const std::string& s = text.utf8;
        const std::size_t n = cps.size();
        std::size_t i = 0;
        while (i < n) {
            const char32_t c0 = cps[i];
            if (c0 == '\n') { ++i; continue; }
            RunInfo ri;
            ri.face = static_cast<std::uint32_t>(std::max<std::int32_t>(0, cp_face[i]));
            ri.level = cp_level[i];
            ri.script = cp_script[i];
            ri.begin = cp_bytes[i];
            if (c0 == '\t') {
                ri.end = ri.begin + 1;
                runs.push_back(ri);
                Cluster c;
                c.begin = ri.begin;
                c.end = ri.end;
                c.face = ri.face;
                c.level = ri.level;
                c.tab = true;
                c.white = true;
                c.run = static_cast<std::uint32_t>(runs.size() - 1);
                c.break_before = byte_break[c.begin] != 0;
                c.glyph_begin = c.glyph_end = static_cast<std::uint32_t>(glyphs.size());
                clusters.push_back(c);
                ++i;
                continue;
            }
            std::size_t j = i + 1;
            const PreparedSpan& sp0 = text.spans[cp_span[i]];
            while (j < n && cps[j] != '\n' && cps[j] != '\t' && cp_face[j] == cp_face[i] && cp_level[j] == cp_level[i]
                   && cp_script[j] == cp_script[i] && (cp_span[j] == cp_span[i] || same_font(text.spans[cp_span[j]], sp0))) ++j;
            ri.end = j < n ? cp_bytes[j] : static_cast<std::uint32_t>(s.size());
            runs.push_back(ri);
            const std::uint32_t run = static_cast<std::uint32_t>(runs.size() - 1);
            std::uint32_t seg = ri.begin;
            while (seg < ri.end) {
                std::uint32_t seg_end = ri.end;
                if (ri.end - seg > kLongRun) {
                    seg_end = seg;
                    while (seg_end < ri.end) {
                        std::size_t len = 1;
                        const char32_t c = decode(s, seg_end, len);
                        seg_end += static_cast<std::uint32_t>(len);
                        if (c == ' ' && seg_end - seg >= kLongRun / 4) {
                            while (seg_end < ri.end && s[seg_end] == ' ') ++seg_end;
                            break;
                        }
                    }
                }
                shape_segment(text, run, seg, seg_end, false, clusters, glyphs);
                seg = seg_end;
            }
            i = j;
        }
    }

    static char32_t char_at(const std::string& s, std::size_t at) noexcept {
        if (at >= s.size()) return 0;
        std::size_t len = 1;
        return decode(s, at, len);
    }

    static char32_t char_before(const std::string& s, std::size_t at) noexcept {
        if (at == 0) return 0;
        return char_at(s, prev_cp_start(s, at));
    }

    const std::vector<HbGlyph>& hyphen_shape(std::uint32_t face) {
        hb_codepoint_t g = 0;
        if (hb_font_get_nominal_glyph(faces[face].hb, 0x2010, &g)) return shape_piece(face, HB_SCRIPT_COMMON, false, "\xE2\x80\x90", 3);
        return shape_piece(face, HB_SCRIPT_COMMON, false, "-", 1);
    }

    std::int32_t hyphen_width(std::uint32_t face) {
        std::int32_t w = 0;
        for (const HbGlyph& g : hyphen_shape(face)) w += round_units(g.advance);
        return w;
    }

    static bool grapheme_start(const std::string& s, std::size_t at) noexcept {
        return unicode::is_grapheme_boundary(StringView(s.data(), s.size()), at);
    }

    std::uint32_t ellipsis_face(const PreparedText& text, std::uint32_t byte) {
        const PreparedSpan& sp = text.spans[text.span_at(byte)];
        FontList& fl = list_for(sp.family, sp.weight, sp.slant);
        const std::int32_t f = face_for(fl, pick(fl, 0x2026), sp.size);
        return f < 0 ? 0u : static_cast<std::uint32_t>(f);
    }

    std::int32_t tab_width(const Cluster& c, std::int32_t x) {
        const std::int32_t stop = std::max<std::int32_t>(kUnit, space_width(c.face) * 8);
        const std::int32_t next = (x / stop + 1) * stop;
        return next - x;
    }

    std::int32_t run_baseline(std::uint32_t face, hb_script_t script, std::uint32_t tag) {
#if HB_VERSION_ATLEAST(4, 0, 0)
        hb_tag_t script_tags[HB_OT_MAX_TAGS_PER_SCRIPT];
        hb_tag_t lang_tags[HB_OT_MAX_TAGS_PER_LANGUAGE];
        unsigned int script_count = HB_OT_MAX_TAGS_PER_SCRIPT;
        unsigned int lang_count = HB_OT_MAX_TAGS_PER_LANGUAGE;
        hb_ot_tags_from_script_and_language(script, hb_language_from_string("c", -1), &script_count, script_tags, &lang_count, lang_tags);
        hb_position_t pos = 0;
        hb_ot_layout_get_baseline_with_fallback(faces[face].hb, static_cast<hb_ot_layout_baseline_tag_t>(tag), HB_DIRECTION_LTR,
                                                script_count ? script_tags[script_count - 1] : HB_OT_TAG_DEFAULT_SCRIPT,
                                                lang_count ? lang_tags[lang_count - 1] : HB_TAG_NONE, &pos);
        return pos;
#else
        (void)face;
        (void)script;
        (void)tag;
        return 0;
#endif
    }

    void line_metrics(std::size_t first, std::size_t last, std::uint32_t fallback_face, std::int32_t& ascent, std::int32_t& descent) {
        ascent = 0;
        descent = 0;
        bool any = false;
        std::uint32_t tag = 0;
        std::int32_t baseline = 0;
        std::size_t i = first;
        while (i < last) {
            const std::uint32_t run = clusters[i].run;
            std::size_t j = i;
            while (j < last && clusters[j].run == run) ++j;
            const RunInfo& ri = runs[run];
            const Face& f = faces[ri.face];
            std::int32_t shift = 0;
#if HB_VERSION_ATLEAST(4, 0, 0)
            if (!any) {
                tag = hb_ot_layout_get_horizontal_baseline_tag_for_script(ri.script);
                baseline = run_baseline(ri.face, ri.script, tag);
            } else if (tag != HB_OT_LAYOUT_BASELINE_TAG_ROMAN) {
                shift = baseline - run_baseline(ri.face, ri.script, tag);
            }
#endif
            if (!any) {
                ascent = f.ascent;
                descent = f.descent;
            } else {
                ascent = std::max(ascent, f.ascent + shift);
                descent = std::max(descent, f.descent - shift);
            }
            any = true;
            i = j;
        }
        if (!any && fallback_face < faces.size()) {
            ascent = faces[fallback_face].ascent;
            descent = faces[fallback_face].descent;
        }
    }

    std::uint32_t face_at_byte(const PreparedText& text, std::uint32_t byte) {
        for (std::size_t i = 0; i < cps.size(); ++i) {
            if (cp_bytes[i] >= byte && cp_face[i] >= 0) return static_cast<std::uint32_t>(cp_face[i]);
        }
        for (std::size_t i = cps.size(); i-- > 0;) {
            if (cp_face[i] >= 0) return static_cast<std::uint32_t>(cp_face[i]);
        }
        const PreparedSpan& sp = text.spans[text.span_at(byte)];
        FontList& fl = list_for(sp.family, sp.weight, sp.slant);
        const std::int32_t f = face_for(fl, pick(fl, 'a'), sp.size);
        return f < 0 ? 0u : static_cast<std::uint32_t>(f);
    }

    bool shape(const PreparedText& text, const ShapeParams& p, ShapedLayout& out) {
        out.clear();
        if (!ok || text.spans.empty() || text.utf8.empty()) return false;
        itemize(text, p);
        assign_fonts(text);
        compute_breaks(text.utf8);
        build_clusters(text);
        if (faces.empty()) return false;

        const bool bounded = p.max_width > 0.0f && (p.wrap || p.ellipsize);
        const std::int32_t max_w = bounded ? static_cast<std::int32_t>(p.max_width * kUnitF) : 0;
        const std::int32_t indent = to_units(p.indent);
        const std::string& s = text.utf8;

        std::vector<std::pair<std::uint32_t, std::uint32_t>> paragraphs;
        std::uint32_t pb = 0;
        for (std::uint32_t b = 0; b <= s.size(); ++b) {
            if (b == s.size() || s[b] == '\n') {
                paragraphs.emplace_back(pb, b);
                pb = b + 1;
            }
        }

        struct Pending {
            std::uint32_t first, last;
            std::uint32_t start, end;
            bool wrapped, para_first, para_end;
            std::uint32_t extra_face;
            bool ellipsis;
            bool hyphen = false;
        };
        std::vector<Pending> pending;
        std::size_t ci = 0;
        for (const auto& [b0, b1] : paragraphs) {
            const std::size_t cfirst = ci;
            while (ci < clusters.size() && clusters[ci].begin < b1) ++ci;
            const std::size_t clast = ci;
            if (cfirst == clast) {
                pending.push_back({ static_cast<std::uint32_t>(cfirst), static_cast<std::uint32_t>(clast), b0, b1, false, true, true, 0, false });
                continue;
            }
            if (p.ellipsize && bounded) {
                std::int32_t total = 0;
                for (std::size_t k = cfirst; k < clast; ++k) total += clusters[k].tab ? tab_width(clusters[k], total) : clusters[k].width;
                const std::int32_t avail = max_w - std::max<std::int32_t>(0, indent);
                if (total <= avail) {
                    pending.push_back({ static_cast<std::uint32_t>(cfirst), static_cast<std::uint32_t>(clast), b0, b1, false, true, true, 0, false });
                } else {
                    pending.push_back({ static_cast<std::uint32_t>(cfirst), static_cast<std::uint32_t>(clast), b0, b1, false, true, true, clusters[cfirst].face, true });
                }
                continue;
            }
            std::size_t start = cfirst;
            bool para_first = true;
            while (start < clast) {
                const std::int32_t avail = max_w - (para_first ? std::max<std::int32_t>(0, indent) : std::max<std::int32_t>(0, -indent));
                std::int32_t x = 0;
                std::size_t last_break = start;
                std::size_t j = start;
                std::size_t cut = clast;
                bool hyphen = false;
                while (j < clast) {
                    const Cluster& c = clusters[j];
                    if (j > start && c.break_before) last_break = j;
                    const std::int32_t w = c.tab ? tab_width(c, x) : c.width;
                    if (p.wrap && bounded && j > start && !c.white) {
                        if (x + w - c.space_right > avail) {
                            if (last_break > start) {
                                cut = last_break;
                                hyphen = char_before(s, clusters[cut].begin) == 0x00AD;
                            } else {
                                cut = j;
                                std::int32_t used = x;
                                while (true) {
                                    hyphen = needs_hyphen(char_before(s, clusters[cut].begin), char_at(s, clusters[cut].begin));
                                    if (!hyphen || cut <= start + 1) break;
                                    const std::int32_t hw = hyphen_width(clusters[cut - 1].face);
                                    if (used + hw <= avail) break;
                                    --cut;
                                    used -= clusters[cut].width;
                                }
                            }
                            break;
                        }
                    }
                    x += w;
                    ++j;
                }
                if (cut == clast) {
                    pending.push_back({ static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(clast), clusters[start].begin, b1, false, para_first, true, 0, false });
                    start = clast;
                } else {
                    pending.push_back({ static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(cut), clusters[start].begin, clusters[cut].begin, true, para_first, false, clusters[cut - 1].face, false, hyphen });
                    start = cut;
                }
                if (para_first && pending.back().start == clusters[cfirst].begin) pending.back().start = b0;
                para_first = false;
            }
        }

        std::int32_t prev_baseline = 0;
        std::int32_t prev_bottom = 0;
        std::vector<std::uint32_t> order;
        std::vector<std::int32_t> widths;
        for (std::size_t li = 0; li < pending.size(); ++li) {
            const Pending& L = pending[li];
            const std::uint32_t fallback = L.first == L.last ? face_at_byte(text, L.start) : 0u;
            std::int32_t ascent = 0, descent = 0;
            line_metrics(L.first, L.last, fallback, ascent, descent);
            if (L.ellipsis) {
                const Face& ef = faces[L.extra_face];
                ascent = std::max(ascent, ef.ascent);
                descent = std::max(descent, ef.descent);
            }
            const std::int32_t height = ascent + descent;
            std::int32_t top = 0;
            std::int32_t baseline = 0;
            if (li == 0) {
                top = 0;
                baseline = ascent;
            } else if (p.line_spacing > 0.0f) {
                baseline = prev_baseline + static_cast<std::int32_t>(std::lround(p.line_spacing * static_cast<float>(height)));
                top = baseline - ascent;
            } else {
                top = prev_bottom;
                baseline = top + ascent;
            }
            prev_baseline = baseline;
            prev_bottom = top + height;

            line_clusters.clear();
            line_glyphs.clear();
            for (std::size_t k = L.first; k < L.last;) {
                const std::uint32_t run = clusters[k].run;
                std::size_t m = k;
                while (m < L.last && clusters[m].run == run) ++m;
                const RunInfo& ri = runs[run];
                const bool cut_before = k == L.first && clusters[k].begin > ri.begin;
                const bool cut_after = m == L.last && clusters[m - 1].end < ri.end;
                const bool hyphen = L.hyphen && m == L.last;
                if (!clusters[k].tab && (cut_before || cut_after || hyphen)) {
                    shape_segment(text, run, clusters[k].begin, clusters[m - 1].end, hyphen, line_clusters, line_glyphs);
                } else {
                    for (std::size_t q = k; q < m; ++q) {
                        Cluster c = clusters[q];
                        const std::uint32_t g0 = static_cast<std::uint32_t>(line_glyphs.size());
                        line_glyphs.insert(line_glyphs.end(), glyphs.begin() + c.glyph_begin, glyphs.begin() + c.glyph_end);
                        c.glyph_end = g0 + (c.glyph_end - c.glyph_begin);
                        c.glyph_begin = g0;
                        line_clusters.push_back(c);
                    }
                }
                k = m;
            }
            std::size_t first = 0;
            std::size_t last = line_clusters.size();
            widths.assign(last - first, 0);
            std::int32_t x = 0;
            for (std::size_t k = first; k < last; ++k) {
                const Cluster& c = line_clusters[k];
                widths[k - first] = c.tab ? tab_width(c, x) : c.width;
                x += widths[k - first];
            }
            if (last > first) {
                widths[0] -= line_clusters[first].space_left;
                widths[last - first - 1] -= line_clusters[last - 1].space_right;
            }
            if (L.wrapped && last > first && line_clusters[last - 1].white && !line_clusters[last - 1].tab) widths[last - first - 1] = 0;

            std::vector<GlyphRec> ell;
            std::int32_t ell_w = 0;
            if (L.ellipsis) {
                const std::int32_t avail = max_w - std::max<std::int32_t>(0, indent);
                std::int32_t total = 0;
                for (std::int32_t w : widths) total += w;
                std::size_t k = last;
                std::int32_t keep = total;
                std::uint32_t ef = 0xFFFFFFFFu;
                while (k > first) {
                    --k;
                    keep -= widths[k - first];
                    while (k > first && (widths[k - first] == 0 || !grapheme_start(text.utf8, line_clusters[k].begin))) {
                        --k;
                        keep -= widths[k - first];
                    }
                    const std::uint32_t face = ellipsis_face(text, line_clusters[k].begin);
                    if (face != ef) {
                        ef = face;
                        ell.clear();
                        ell_w = 0;
                        for (const HbGlyph& g : shape_piece(ef, HB_SCRIPT_COMMON, false, "\xE2\x80\xA6", 3)) {
                            GlyphRec r{ g.glyph, ef, round_units(g.advance), round_units(g.x_offset), round_units(g.y_offset) };
                            ell_w += r.width;
                            ell.push_back(r);
                        }
                    }
                    if (keep + ell_w <= avail) break;
                }
                if (!ell.empty() && keep + ell_w < avail) {
                    ell.back().width += avail - keep - ell_w;
                    ell_w = avail - keep;
                }
                last = k;
                widths.resize(last - first);
            }

            std::int32_t line_w = ell_w;
            for (std::int32_t w : widths) line_w += w;

            const bool justify = p.justify && p.wrap && bounded && L.wrapped;
            const std::int32_t first_indent = L.para_first ? indent : -indent;
            const std::int32_t avail = max_w - std::max<std::int32_t>(0, first_indent);
            if (justify && line_w < avail) {
                std::size_t spaces = 0;
                for (std::size_t k = first; k < last; ++k) if (line_clusters[k].expandable && widths[k - first] > 0) ++spaces;
                if (spaces > 0) {
                    const std::int64_t extra = avail - line_w;
                    std::int64_t added = 0;
                    std::size_t seen = 0;
                    for (std::size_t k = first; k < last; ++k) {
                        if (!line_clusters[k].expandable || widths[k - first] <= 0) continue;
                        ++seen;
                        std::int32_t adj = static_cast<std::int32_t>(static_cast<std::int64_t>(seen) * extra / static_cast<std::int64_t>(spaces) - added);
                        adj = round_units(adj);
                        widths[k - first] += adj;
                        added += adj;
                        line_w += adj;
                    }
                }
            }

            std::int32_t offset = std::max<std::int32_t>(0, first_indent);
            if (p.wrap && bounded && !justify) {
                const std::int32_t room = avail - line_w;
                if (p.align == text::TextAlign::Center) offset += round_units(room / 2);
                else if (p.align == text::TextAlign::Right) offset += room;
            }

            order.clear();
            for (std::size_t k = first; k < last; ++k) order.push_back(static_cast<std::uint32_t>(k));
            std::uint8_t max_level = 0, min_odd = 255;
            for (std::uint32_t k : order) {
                max_level = std::max(max_level, line_clusters[k].level);
                if (line_clusters[k].level & 1) min_odd = std::min(min_odd, line_clusters[k].level);
            }
            if (max_level > 0) {
                const std::uint8_t lowest = std::min<std::uint8_t>(min_odd, max_level);
                for (std::uint8_t lv = max_level; lv >= std::max<std::uint8_t>(1, lowest); --lv) {
                    std::size_t a = 0;
                    while (a < order.size()) {
                        if (line_clusters[order[a]].level < lv) { ++a; continue; }
                        std::size_t b = a;
                        while (b < order.size() && line_clusters[order[b]].level >= lv) ++b;
                        std::reverse(order.begin() + static_cast<std::ptrdiff_t>(a), order.begin() + static_cast<std::ptrdiff_t>(b));
                        a = b;
                    }
                    if (lv == 1) break;
                }
            }

            const std::uint32_t line_index = static_cast<std::uint32_t>(out.lines.size());
            std::int32_t pen = offset;
            for (std::uint32_t k : order) {
                const Cluster& c = line_clusters[k];
                const std::int32_t w = widths[k - first];
                std::int32_t gx = pen;
                const std::uint32_t span = text.span_at(c.begin);
                const bool hidden = L.wrapped && k + 1 == last && c.white && w == 0;
                for (std::uint32_t g = hidden ? c.glyph_end : c.glyph_begin; g < c.glyph_end; ++g) {
                    const GlyphRec& r = line_glyphs[g];
                    std::int32_t xo = r.x_offset;
                    if (k == first && g == c.glyph_begin) xo -= c.space_left;
                    if (r.glyph != 0) {
                        ShapedGlyph sg;
                        sg.face = r.face;
                        sg.glyph = r.glyph;
                        sg.span = span;
                        sg.line = line_index;
                        sg.x = to_px(gx + xo);
                        sg.y = to_px(baseline + r.y_offset);
                        sg.advance = to_px(r.width);
                        out.glyphs.push_back(sg);
                    }
                    gx += r.width;
                    if (k == first && g == c.glyph_begin) gx -= c.space_left;
                }
                pen += w;
            }
            if (!ell.empty()) {
                const std::uint32_t span = last > first ? text.span_at(line_clusters[last - 1].begin) : text.span_at(L.start);
                for (const GlyphRec& r : ell) {
                    ShapedGlyph sg;
                    sg.face = r.face;
                    sg.glyph = r.glyph;
                    sg.span = span;
                    sg.line = line_index;
                    sg.x = to_px(pen + r.x_offset);
                    sg.y = to_px(baseline + r.y_offset);
                    sg.advance = to_px(r.width);
                    out.glyphs.push_back(sg);
                    pen += r.width;
                }
            }

            ShapedLine sl;
            sl.left = to_px(offset);
            sl.width = to_px(line_w);
            sl.top = to_px(top);
            sl.baseline = to_px(baseline);
            sl.bottom = to_px(top + height);
            sl.start = L.start;
            sl.end = L.end;
            sl.paragraph_end = L.para_end;
            out.lines.push_back(sl);
        }

        out.width = 0.0f;
        for (const ShapedLine& L : out.lines) out.width = std::max(out.width, L.width);
        out.height = out.lines.empty() ? 0.0f : out.lines.back().bottom;
        return true;
    }

    bool rasterize(std::uint32_t face, std::uint32_t glyph, GlyphImage& out) {
        out = GlyphImage{};
        if (face >= faces.size()) return false;
        const Face& f = faces[face];
        Source& src = *sources[f.source];
        if (!f.ft_size) return false;
        FT_Activate_Size(f.ft_size);
        FT_Face ftf = src.ft;
        if (src.bitmap) {
            if (FT_Load_Glyph(ftf, glyph, FT_LOAD_COLOR) != 0) return false;
            if (ftf->glyph->format != FT_GLYPH_FORMAT_BITMAP && FT_Render_Glyph(ftf->glyph, FT_RENDER_MODE_NORMAL) != 0) return false;
            return copy_bitmap(ftf->glyph, f.bitmap_scale, out);
        }
        if (src.color) {
            if (FT_Load_Glyph(ftf, glyph, FT_LOAD_COLOR | FT_LOAD_NO_BITMAP) == 0 && FT_Render_Glyph(ftf->glyph, FT_RENDER_MODE_NORMAL) == 0
                && ftf->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                return copy_bitmap(ftf->glyph, 1.0f, out);
            }
        }
        if (FT_Load_Glyph(ftf, glyph, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0) return false;
        FT_GlyphSlot slot = ftf->glyph;
        if (slot->format != FT_GLYPH_FORMAT_OUTLINE) {
            if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) return false;
            return copy_bitmap(slot, 1.0f, out);
        }
        FT_Outline& outline = slot->outline;
        if (f.embolden) {
            const FT_Pos strength = FT_MulFix(ftf->units_per_EM, ftf->size->metrics.y_scale) / 24;
            FT_Outline_Embolden(&outline, strength);
        }
        if (f.slant != 0.0f) {
            FT_Matrix m{ 0x10000, static_cast<FT_Fixed>(std::lround(f.slant * 65536.0f)), 0, 0x10000 };
            FT_Outline_Transform(&outline, &m);
        }
        FT_BBox box;
        FT_Outline_Get_CBox(&outline, &box);
        const int x0 = static_cast<int>(std::floor(box.xMin / 64.0));
        const int x1 = static_cast<int>(std::ceil(box.xMax / 64.0));
        const int y_top = static_cast<int>(std::ceil(box.yMax / 64.0));
        const int y_bottom = static_cast<int>(std::floor(box.yMin / 64.0));
        const int w = x1 - x0;
        const int h = y_top - y_bottom;
        if (w <= 0 || h <= 0) return true;
        if (w > 4096 || h > 4096) return true;
        raster.reset(w, h, static_cast<float>(x0), static_cast<float>(-y_top));
        FT_Outline_Funcs funcs{};
        funcs.move_to = [](const FT_Vector* to, void* user) -> int {
            static_cast<OutlineRasterizer*>(user)->move_to(to->x / 64.0f, -to->y / 64.0f);
            return 0;
        };
        funcs.line_to = [](const FT_Vector* to, void* user) -> int {
            static_cast<OutlineRasterizer*>(user)->line_to(to->x / 64.0f, -to->y / 64.0f);
            return 0;
        };
        funcs.conic_to = [](const FT_Vector* c, const FT_Vector* to, void* user) -> int {
            static_cast<OutlineRasterizer*>(user)->quad_to(c->x / 64.0f, -c->y / 64.0f, to->x / 64.0f, -to->y / 64.0f);
            return 0;
        };
        funcs.cubic_to = [](const FT_Vector* c1, const FT_Vector* c2, const FT_Vector* to, void* user) -> int {
            static_cast<OutlineRasterizer*>(user)->cubic_to(c1->x / 64.0f, -c1->y / 64.0f, c2->x / 64.0f, -c2->y / 64.0f, to->x / 64.0f, -to->y / 64.0f);
            return 0;
        };
        if (FT_Outline_Decompose(&outline, &funcs, &raster) != 0) return false;
        raster.close();
        out.width = w;
        out.height = h;
        out.left = x0;
        out.top = y_top;
        out.color = false;
        raster.coverage(out.pixels);
        return true;
    }

    static bool copy_bitmap(FT_GlyphSlot slot, float scale, GlyphImage& out) {
        const FT_Bitmap& bm = slot->bitmap;
        if (bm.width == 0 || bm.rows == 0) return true;
        const int w = static_cast<int>(bm.width);
        const int h = static_cast<int>(bm.rows);
        const int pitch = bm.pitch < 0 ? -bm.pitch : bm.pitch;
        if (bm.pixel_mode == FT_PIXEL_MODE_BGRA) {
            if (scale < 0.999f || scale > 1.001f) {
                int dw = 0, dh = 0;
                downscale_rgba(bm.buffer, w, h, pitch, true, scale, out.pixels, dw, dh);
                out.width = dw;
                out.height = dh;
                out.left = static_cast<int>(std::lround(slot->bitmap_left * scale));
                out.top = static_cast<int>(std::lround(slot->bitmap_top * scale));
            } else {
                out.width = w;
                out.height = h;
                out.left = slot->bitmap_left;
                out.top = slot->bitmap_top;
                out.pixels.resize(static_cast<std::size_t>(w) * h * 4);
                for (int y = 0; y < h; ++y) {
                    const std::uint8_t* row = bm.buffer + static_cast<std::size_t>(y) * pitch;
                    std::uint8_t* d = &out.pixels[static_cast<std::size_t>(y) * w * 4];
                    for (int x = 0; x < w; ++x) {
                        d[x * 4 + 0] = row[x * 4 + 2];
                        d[x * 4 + 1] = row[x * 4 + 1];
                        d[x * 4 + 2] = row[x * 4 + 0];
                        d[x * 4 + 3] = row[x * 4 + 3];
                    }
                }
            }
            out.color = true;
            return true;
        }
        out.width = w;
        out.height = h;
        out.left = slot->bitmap_left;
        out.top = slot->bitmap_top;
        out.color = false;
        out.pixels.assign(static_cast<std::size_t>(w) * h, 0);
        for (int y = 0; y < h; ++y) {
            const std::uint8_t* row = bm.buffer + static_cast<std::size_t>(y) * pitch;
            std::uint8_t* d = &out.pixels[static_cast<std::size_t>(y) * w];
            if (bm.pixel_mode == FT_PIXEL_MODE_GRAY) {
                std::memcpy(d, row, static_cast<std::size_t>(w));
            } else if (bm.pixel_mode == FT_PIXEL_MODE_MONO) {
                for (int x = 0; x < w; ++x) d[x] = (row[x >> 3] & (0x80 >> (x & 7))) ? 255 : 0;
            }
        }
        return true;
    }
};

TextEngine::TextEngine() noexcept = default;

TextEngine::~TextEngine() noexcept {
    shutdown();
}

bool TextEngine::initialize(void*) noexcept {
    if (m_impl && m_impl->ok) return true;
    try {
        if (!m_impl) m_impl = std::make_unique<Impl>();
        if (!m_impl->init()) {
            m_impl.reset();
            return false;
        }
        return true;
    } catch (...) {
        m_impl.reset();
        return false;
    }
}

void TextEngine::shutdown() noexcept {
    m_impl.reset();
}

bool TextEngine::ready() const noexcept {
    return m_impl && m_impl->ok;
}

bool TextEngine::load_font_file(const std::string& utf8_path) {
    if (!ready()) return false;
#if defined(OS_ANDROID)
    FT_Face face = nullptr;
    if (FT_New_Face(m_impl->ft.lib, utf8_path.c_str(), 0, &face) != 0 || !face) return false;
    const FT_Long count = std::max<FT_Long>(1, face->num_faces);
    FT_Done_Face(face);
    bool any = false;
    for (FT_Long i = 0; i < count; ++i) {
        if (FT_New_Face(m_impl->ft.lib, utf8_path.c_str(), i, &face) != 0 || !face) continue;
        const std::string family = face->family_name ? face->family_name : "";
        const bool italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
        int weight = (face->style_flags & FT_STYLE_FLAG_BOLD) ? 700 : 400;
        if (const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2))) {
            if (os2->usWeightClass >= 1 && os2->usWeightClass <= 1000) weight = os2->usWeightClass;
        }
        FT_Done_Face(face);
        if (!family.empty()) any = AndroidFontCatalog::instance().add_user_font(utf8_path, static_cast<int>(i), family, weight, italic) || any;
    }
    if (!any) return false;
#else
    if (!FcConfigAppFontAddFile(FcConfigGetCurrent(), reinterpret_cast<const FcChar8*>(utf8_path.c_str()))) return false;
#endif
    for (auto& [k, l] : m_impl->lists) Impl::release_list(*l);
    m_impl->lists.clear();
    m_impl->shapes.clear();
    return true;
}

std::string TextEngine::transform_case(const std::string& s, text::TextTransform t) {
    if (t == text::TextTransform::None || s.empty()) return s;
    std::string out;
    out.reserve(s.size());
    char32_t buf[3];
    char enc[4];
    bool word_start = true;
    for (std::size_t i = 0; i < s.size();) {
        std::size_t len = 1;
        const char32_t c = decode(s, i, len);
        i += len;
        std::size_t n = 0;
        if (t == text::TextTransform::Uppercase) n = unicode::full_to_upper(c, buf);
        else if (t == text::TextTransform::Lowercase) n = unicode::full_to_lower(c, buf);
        else if (word_start && unicode::is_alphabetic(c)) n = unicode::full_to_title(c, buf);
        else { buf[0] = c; n = 1; }
        if (n == 0) { buf[0] = c; n = 1; }
        for (std::size_t k = 0; k < n; ++k) out.append(enc, unicode::encode_utf8(buf[k], enc));
        word_start = !unicode::is_alphanumeric(c) && c != '\'';
    }
    return out;
}

bool TextEngine::shape(const PreparedText& text, const ShapeParams& p, ShapedLayout& out) {
    if (!ready()) {
        out.clear();
        return false;
    }
    return m_impl->shape(text, p, out);
}

bool TextEngine::rasterize(std::uint32_t face, std::uint32_t glyph, GlyphImage& out) {
    if (!ready()) {
        out = GlyphImage{};
        return false;
    }
    return m_impl->rasterize(face, glyph, out);
}

FaceMetrics TextEngine::face_metrics(std::uint32_t face) const noexcept {
    if (!m_impl || face >= m_impl->faces.size()) return FaceMetrics{};
    return m_impl->faces[face].metrics;
}

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif
