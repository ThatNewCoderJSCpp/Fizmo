#ifndef FIZMO_TEXT_FONTS_HPP
#define FIZMO_TEXT_FONTS_HPP

#include "../Graphics/color.hpp"
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <ostream>

#ifdef OS_LINUX
#endif

namespace fizmo {
namespace text {

#define FIZMO_SERIF_FONTS(X) \
    X(TimesNewRoman,       "Times New Roman")       \
    X(Georgia,             "Georgia")               \
    X(Garamond,            "Garamond")              \
    X(Palatino,            "Palatino Linotype")     \
    X(BookAntiqua,         "Book Antiqua")          \
    X(Cambria,             "Cambria")               \
    X(Merriweather,        "Merriweather")          \
    X(PlayfairDisplay,     "Playfair Display")      \
    X(Lora,                "Lora")                  \
    X(PTSerif,             "PT Serif")              \
    X(NotoSerif,           "Noto Serif")            \
    X(LibreBaskerville,    "Libre Baskerville")     \
    X(EBGaramond,          "EB Garamond")           \
    X(SourceSerifPro,      "Source Serif Pro")      \
    X(CrimsonText,         "Crimson Text")          \
    X(CormorantGaramond,   "Cormorant Garamond")   \
    X(Bitter,              "Bitter")                \
    X(Arvo,                "Arvo")                  \
    X(ZillaSlab,           "Zilla Slab")            \
    X(Spectral,            "Spectral")              \
    X(DroidSerif,          "Droid Serif")           \
    X(CrimsonPro,          "Crimson Pro")           \
    X(Vollkorn,            "Vollkorn")              \
    X(NotoSerifJP,         "Noto Serif JP")         \
    X(NotoSerifSC,         "Noto Serif SC")         \
    X(NotoSerifKR,         "Noto Serif KR")         \
    X(InvalidSerif,        "Invalid Serif")

#define FIZMO_SANS_SERIF_FONTS(X) \
    X(Arial,               "Arial")                 \
    X(Helvetica,           "Helvetica")             \
    X(Verdana,             "Verdana")               \
    X(Tahoma,              "Tahoma")                \
    X(TrebuchetMS,         "Trebuchet MS")          \
    X(Calibri,             "Calibri")               \
    X(SegoeUI,             "Segoe UI")              \
    X(Roboto,              "Roboto")                \
    X(OpenSans,            "Open Sans")             \
    X(Lato,                "Lato")                  \
    X(Montserrat,          "Montserrat")            \
    X(Poppins,             "Poppins")               \
    X(Raleway,             "Raleway")               \
    X(Ubuntu,              "Ubuntu")                \
    X(NotoSans,            "Noto Sans")             \
    X(SourceSansPro,       "Source Sans Pro")       \
    X(Inter,               "Inter")                 \
    X(Nunito,              "Nunito")                \
    X(Oswald,              "Oswald")                \
    X(WorkSans,            "Work Sans")             \
    X(PTSans,              "PT Sans")               \
    X(Rubik,               "Rubik")                 \
    X(Karla,               "Karla")                 \
    X(Quicksand,           "Quicksand")             \
    X(Comfortaa,           "Comfortaa")             \
    X(Barlow,              "Barlow")                \
    X(Manrope,             "Manrope")               \
    X(DMSans,              "DM Sans")               \
    X(Figtree,             "Figtree")               \
    X(Outfit,              "Outfit")                \
    X(Mukta,               "Mukta")                 \
    X(Lexend,              "Lexend")                \
    X(Cabin,               "Cabin")                 \
    X(Catamaran,           "Catamaran")             \
    X(Exo2,                "Exo 2")                 \
    X(Overpass,            "Overpass")              \
    X(Asap,                "Asap")                  \
    X(Mulish,              "Mulish")                \
    X(NotoSansJP,          "Noto Sans JP")          \
    X(NotoSansSC,          "Noto Sans SC")          \
    X(NotoSansKR,          "Noto Sans KR")          \
    X(DroidSans,           "Droid Sans")            \
    X(FranklinGothic,      "Franklin Gothic Medium")\
    X(CenturyGothic,       "Century Gothic")        \
    X(GillSans,            "Gill Sans")             \
    X(Futura,              "Futura")                \
    X(NunitoSans,          "Nunito Sans")           \
    X(InvalidSansSerif,    "Invalid Sans-Serif")

#define FIZMO_MONOSPACE_FONTS(X) \
    X(CourierNew,          "Courier New")            \
    X(Consolas,            "Consolas")              \
    X(LucidaConsole,       "Lucida Console")        \
    X(Monaco,              "Monaco")                \
    X(Menlo,               "Menlo")                 \
    X(SourceCodePro,       "Source Code Pro")       \
    X(FiraCode,            "Fira Code")             \
    X(JetBrainsMono,       "JetBrains Mono")        \
    X(RobotoMono,          "Roboto Mono")           \
    X(Inconsolata,         "Inconsolata")           \
    X(UbuntuMono,          "Ubuntu Mono")           \
    X(IBMPlexMono,         "IBM Plex Mono")         \
    X(CascadiaCode,        "Cascadia Code")         \
    X(DroidSansMono,       "Droid Sans Mono")       \
    X(NotoSansMono,        "Noto Sans Mono")        \
    X(SpaceMono,           "Space Mono")            \
    X(FiraMono,            "Fira Mono")             \
    X(AnonymousPro,        "Anonymous Pro")         \
    X(CascadiaMono,        "Cascadia Mono")         \
    X(Hack,                "Hack")                  \
    X(DejaVuSansMono,      "DejaVu Sans Mono")      \
    X(InvalidMonospace,    "Invalid Monospace")

#define FIZMO_DISPLAY_FONTS(X) \
    X(Impact,              "Impact")                \
    X(ComicSansMS,         "Comic Sans MS")         \
    X(BebasNeue,           "Bebas Neue")            \
    X(Anton,               "Anton")                 \
    X(Righteous,           "Righteous")             \
    X(PermanentMarker,     "Permanent Marker")      \
    X(Bangers,             "Bangers")               \
    X(Bungee,              "Bungee")                \
    X(Fredoka,             "Fredoka")               \
    X(LobsterTwo,          "Lobster Two")           \
    X(Pacifico,            "Pacifico")              \
    X(LuckiestGuy,         "Luckiest Guy")          \
    X(BlackOpsOne,         "Black Ops One")         \
    X(PressStart2P,        "Press Start 2P")        \
    X(VT323,               "VT323")                 \
    X(Orbitron,            "Orbitron")              \
    X(Audiowide,           "Audiowide")             \
    X(BowlbyOneSC,         "Bowlby One SC")         \
    X(Monoton,             "Monoton")               \
    X(SpecialElite,        "Special Elite")         \
    X(CourierPrime,        "Courier Prime")         \
    X(InvalidDisplay,      "Invalid Display Font")

#define FIZMO_HANDWRITING_FONTS(X) \
    X(BrushScriptMT,       "Brush Script MT")       \
    X(SegoeScript,         "Segoe Script")          \
    X(Caveat,              "Caveat")                \
    X(DancingScript,       "Dancing Script")        \
    X(Sacramento,          "Sacramento")            \
    X(IndieFlower,         "Indie Flower")          \
    X(PatrickHand,         "Patrick Hand")          \
    X(Satisfy,             "Satisfy")               \
    X(GreatVibes,          "Great Vibes")           \
    X(Kalam,               "Kalam")                 \
    X(HomemadeApple,       "Homemade Apple")        \
    X(Amatic,              "Amatic SC")             \
    X(ShadowsIntoLight,    "Shadows Into Light")    \
    X(CoveredByYourGrace,  "Covered By Your Grace") \
    X(GloriaHallelujah,    "Gloria Hallelujah")     \
    X(Courgette,           "Courgette")             \
    X(CedarvilleCursive,   "Cedarville Cursive")    \
    X(MarckScript,         "Marck Script")          \
    X(InvalidCursive,      "Invalid Cursive Font")

#define FIZMO_ALL_FONTS(X)     \
    FIZMO_SERIF_FONTS(X)       \
    FIZMO_SANS_SERIF_FONTS(X)  \
    FIZMO_MONOSPACE_FONTS(X)   \
    FIZMO_DISPLAY_FONTS(X)     \
    FIZMO_HANDWRITING_FONTS(X)

enum class FontCategory : std::uint8_t {
    Default    = 0,   
    SansSerif, // (terminal: normal)
    Serif,     // (terminal: normal)
    Monospace, // fixed-width       (terminal: normal, Canvas: grid font)
    Cursive,   // handwriting-style (terminal: italic fallback)
    Fantasy,   // decorative        (terminal: bold fallback)
    System     // OS UI font        
};

std::ostream& operator<<(std::ostream& os, FontCategory f);

#define FIZMO_ENUM_ONLY(name, css) name,
#define FIZMO_ENUM_CSS(name, css)     case Font::name:        return css;
#define FIZMO_ENUM_CSS_CAT(name, css) case decltype(f)::name: return css;
#define FIZMO_ENUM_CAT(name, css) case Font::name:
#define FIZMO_TO_FONT(name, css) case decltype(f)::name: return Font::name;
#define FIZMO_FROM_FONT_SERIF(name, css)       case Font::name: return SerifFont::name;
#define FIZMO_FROM_FONT_SANS(name, css)        case Font::name: return SansSerifFont::name;
#define FIZMO_FROM_FONT_MONO(name, css)        case Font::name: return MonospaceFont::name;
#define FIZMO_FROM_FONT_DISPLAY(name, css)     case Font::name: return DisplayFont::name;
#define FIZMO_FROM_FONT_HANDWRITING(name, css) case Font::name: return HandwritingFont::name;

enum class SerifFont       : std::uint8_t { FIZMO_SERIF_FONTS(FIZMO_ENUM_ONLY)       Count_ };
enum class SansSerifFont   : std::uint8_t { FIZMO_SANS_SERIF_FONTS(FIZMO_ENUM_ONLY)  Count_ };
enum class MonospaceFont   : std::uint8_t { FIZMO_MONOSPACE_FONTS(FIZMO_ENUM_ONLY)   Count_ };
enum class DisplayFont     : std::uint8_t { FIZMO_DISPLAY_FONTS(FIZMO_ENUM_ONLY)     Count_ };
enum class HandwritingFont : std::uint8_t { FIZMO_HANDWRITING_FONTS(FIZMO_ENUM_ONLY) Count_ };

enum class Font : std::uint16_t {
    None = 0,
    FIZMO_ALL_FONTS(FIZMO_ENUM_ONLY)
    Count_
};


inline const char* font_css_name(Font f) noexcept {
    switch (f) {
        case Font::None: return "";
        FIZMO_ALL_FONTS(FIZMO_ENUM_CSS)
        default: return "";
    }
}

inline const char* font_css_name(SerifFont f) noexcept {
    switch (f) { FIZMO_SERIF_FONTS(FIZMO_ENUM_CSS_CAT) default: return ""; }
}
inline const char* font_css_name(SansSerifFont f) noexcept {
    switch (f) { FIZMO_SANS_SERIF_FONTS(FIZMO_ENUM_CSS_CAT) default: return ""; }
}
inline const char* font_css_name(MonospaceFont f) noexcept {
    switch (f) { FIZMO_MONOSPACE_FONTS(FIZMO_ENUM_CSS_CAT) default: return ""; }
}
inline const char* font_css_name(DisplayFont f) noexcept {
    switch (f) { FIZMO_DISPLAY_FONTS(FIZMO_ENUM_CSS_CAT) default: return ""; }
}
inline const char* font_css_name(HandwritingFont f) noexcept {
    switch (f) { FIZMO_HANDWRITING_FONTS(FIZMO_ENUM_CSS_CAT) default: return ""; }
}

FontCategory font_category(Font f) noexcept;

inline Font to_font(SerifFont f) noexcept {
    switch (f) { FIZMO_SERIF_FONTS(FIZMO_TO_FONT) default: return Font::None; }
}
inline Font to_font(SansSerifFont f) noexcept {
    switch (f) { FIZMO_SANS_SERIF_FONTS(FIZMO_TO_FONT) default: return Font::None; }
}
inline Font to_font(MonospaceFont f) noexcept {
    switch (f) { FIZMO_MONOSPACE_FONTS(FIZMO_TO_FONT) default: return Font::None; }
}
inline Font to_font(DisplayFont f) noexcept {
    switch (f) { FIZMO_DISPLAY_FONTS(FIZMO_TO_FONT) default: return Font::None; }
}
inline Font to_font(HandwritingFont f) noexcept {
    switch (f) { FIZMO_HANDWRITING_FONTS(FIZMO_TO_FONT) default: return Font::None; }
}

SerifFont to_serif(Font f) noexcept;

SansSerifFont to_sans_serif(Font f) noexcept;

MonospaceFont to_monospace(Font f) noexcept;

DisplayFont to_display(Font f) noexcept;

HandwritingFont to_handwriting(Font f) noexcept;

std::ostream& operator<<(std::ostream& os, Font f);

inline std::ostream& operator<<(std::ostream& os, SerifFont f)       { return os << font_css_name(f); }
inline std::ostream& operator<<(std::ostream& os, SansSerifFont f)   { return os << font_css_name(f); }
inline std::ostream& operator<<(std::ostream& os, MonospaceFont f)   { return os << font_css_name(f); }
inline std::ostream& operator<<(std::ostream& os, DisplayFont f)     { return os << font_css_name(f); }
inline std::ostream& operator<<(std::ostream& os, HandwritingFont f) { return os << font_css_name(f); }

inline bool is_serif(Font f)        noexcept { return font_category(f) == FontCategory::Serif; }
inline bool is_sans_serif(Font f)   noexcept { return font_category(f) == FontCategory::SansSerif; }
inline bool is_monospace(Font f)    noexcept { return font_category(f) == FontCategory::Monospace; }
inline bool is_display(Font f)      noexcept { return font_category(f) == FontCategory::Fantasy; }
inline bool is_handwriting(Font f)  noexcept { return font_category(f) == FontCategory::Cursive; }

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_FONTS_HPP