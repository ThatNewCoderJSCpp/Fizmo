#ifndef FIZMO_ANDROID_FONTS_HPP
#define FIZMO_ANDROID_FONTS_HPP

#include "../../../Basic/fizmo_defines.hpp"

#if defined(OS_ANDROID)

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

struct AndroidFontFile {
    std::string                               path;
    int                                       index  = 0;
    int                                       weight = 400;
    bool                                      italic = false;
    std::vector<std::pair<std::uint32_t, float>> axes;

    std::string key() const;
};

struct AndroidFontFamily {
    std::string                  name;
    std::string                  lang;
    std::string                  variant;
    std::vector<AndroidFontFile> fonts;
};

struct AndroidFontAlias {
    std::string name;
    std::string to;
    int         weight = 0;
};

struct AndroidFontPick {
    const AndroidFontFile* font     = nullptr;
    bool                   embolden = false;
    float                  slant    = 0.0f;
};

class AndroidFontCatalog {
private:
    std::vector<std::unique_ptr<AndroidFontFamily>> m_named;
    std::vector<std::unique_ptr<AndroidFontFamily>> m_fallbacks;
    std::vector<std::unique_ptr<AndroidFontFamily>> m_user;
    std::vector<AndroidFontAlias>                   m_aliases;
    bool                                            m_loaded = false;
    mutable std::mutex                              m_mutex;

    void load();
    bool parse_file(const std::string& path);
    void scan_directory();
    const AndroidFontFamily* find_family(const std::string& lower_name, int& alias_weight) const;
    static AndroidFontPick best_of(const AndroidFontFamily& family, int weight, bool italic) noexcept;

public:
    static AndroidFontCatalog& instance();

    std::vector<AndroidFontPick> candidates(const std::string& families, int weight, bool italic);
    bool add_user_font(const std::string& path, int index, const std::string& family, int weight, bool italic);
    std::vector<std::string> family_names();
};

std::string android_resolve_font_path(const std::string& file);

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // OS_ANDROID
#endif // FIZMO_ANDROID_FONTS_HPP
