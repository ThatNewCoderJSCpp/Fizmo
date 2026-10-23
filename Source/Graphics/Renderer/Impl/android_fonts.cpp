#include "android_fonts.hpp"

#if defined(OS_ANDROID)

#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && (std::isspace(static_cast<unsigned char>(s[a])) || s[a] == '"' || s[a] == '\'')) ++a;
    while (b > a && (std::isspace(static_cast<unsigned char>(s[b - 1])) || s[b - 1] == '"' || s[b - 1] == '\'')) --b;
    return s.substr(a, b - a);
}

bool file_exists(const std::string& p) {
    return !p.empty() && ::access(p.c_str(), R_OK) == 0;
}

std::uint32_t tag_of(const std::string& s) {
    std::uint32_t t = 0;
    for (std::size_t i = 0; i < 4; ++i) t = (t << 8) | static_cast<unsigned char>(i < s.size() ? s[i] : ' ');
    return t;
}

struct Tag {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    bool closing = false;
    bool self_closing = false;

    std::string attr(const char* key) const {
        for (const auto& [k, v] : attrs) if (k == key) return v;
        return std::string();
    }
};

class XmlScanner {
private:
    const std::string& m_s;
    std::size_t        m_i = 0;

public:
    explicit XmlScanner(const std::string& s) : m_s(s) {}

    bool next(Tag& tag, std::string& text) {
        text.clear();
        while (m_i < m_s.size()) {
            const std::size_t lt = m_s.find('<', m_i);
            if (lt == std::string::npos) {
                text += m_s.substr(m_i);
                m_i = m_s.size();
                return false;
            }
            text += m_s.substr(m_i, lt - m_i);
            m_i = lt;
            if (m_s.compare(m_i, 4, "<!--") == 0) {
                const std::size_t end = m_s.find("-->", m_i + 4);
                m_i = end == std::string::npos ? m_s.size() : end + 3;
                continue;
            }
            if (m_s.compare(m_i, 2, "<?") == 0 || m_s.compare(m_i, 2, "<!") == 0) {
                const std::size_t end = m_s.find('>', m_i);
                m_i = end == std::string::npos ? m_s.size() : end + 1;
                continue;
            }
            return parse_tag(tag);
        }
        return false;
    }

private:
    bool parse_tag(Tag& tag) {
        tag = Tag{};
        ++m_i;
        if (m_i < m_s.size() && m_s[m_i] == '/') {
            tag.closing = true;
            ++m_i;
        }
        const std::size_t start = m_i;
        while (m_i < m_s.size() && !std::isspace(static_cast<unsigned char>(m_s[m_i])) && m_s[m_i] != '>' && m_s[m_i] != '/') ++m_i;
        tag.name = m_s.substr(start, m_i - start);
        while (m_i < m_s.size()) {
            while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
            if (m_i >= m_s.size()) break;
            if (m_s[m_i] == '>') { ++m_i; return true; }
            if (m_s[m_i] == '/') {
                tag.self_closing = true;
                ++m_i;
                continue;
            }
            const std::size_t key_start = m_i;
            while (m_i < m_s.size() && m_s[m_i] != '=' && m_s[m_i] != '>' && !std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
            std::string key = m_s.substr(key_start, m_i - key_start);
            while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
            std::string value;
            if (m_i < m_s.size() && m_s[m_i] == '=') {
                ++m_i;
                while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
                if (m_i < m_s.size() && (m_s[m_i] == '"' || m_s[m_i] == '\'')) {
                    const char q = m_s[m_i++];
                    const std::size_t end = m_s.find(q, m_i);
                    value = m_s.substr(m_i, end == std::string::npos ? std::string::npos : end - m_i);
                    m_i = end == std::string::npos ? m_s.size() : end + 1;
                }
            }
            if (!key.empty()) tag.attrs.emplace_back(std::move(key), std::move(value));
        }
        return true;
    }
};

int guess_weight(const std::string& name) {
    const std::string n = lower(name);
    if (n.find("thin") != std::string::npos || n.find("hairline") != std::string::npos) return 100;
    if (n.find("extralight") != std::string::npos || n.find("ultralight") != std::string::npos) return 200;
    if (n.find("light") != std::string::npos) return 300;
    if (n.find("medium") != std::string::npos) return 500;
    if (n.find("semibold") != std::string::npos || n.find("demibold") != std::string::npos) return 600;
    if (n.find("extrabold") != std::string::npos || n.find("ultrabold") != std::string::npos) return 800;
    if (n.find("black") != std::string::npos || n.find("heavy") != std::string::npos) return 900;
    if (n.find("bold") != std::string::npos) return 700;
    return 400;
}

} // namespace

std::string AndroidFontFile::key() const {
    std::string k = path;
    k += '#';
    k += std::to_string(index);
    for (const auto& [tag, value] : axes) {
        k += '|';
        k += std::to_string(tag);
        k += '=';
        k += std::to_string(static_cast<int>(std::lround(value * 100.0f)));
    }
    return k;
}

std::string android_resolve_font_path(const std::string& file) {
    const std::string f = trim(file);
    if (f.empty()) return std::string();
    if (f.front() == '/') return file_exists(f) ? f : std::string();
    for (const char* dir : { "/system/fonts/", "/product/fonts/", "/system/product/fonts/", "/system_ext/fonts/" }) {
        const std::string p = std::string(dir) + f;
        if (file_exists(p)) return p;
    }
    return std::string();
}

AndroidFontCatalog& AndroidFontCatalog::instance() {
    static AndroidFontCatalog catalog;
    return catalog;
}

bool AndroidFontCatalog::parse_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string data = buffer.str();
    if (data.empty()) return false;

    XmlScanner xml(data);
    Tag tag;
    std::string text;
    std::unique_ptr<AndroidFontFamily> family;
    std::unique_ptr<AndroidFontFamily> group;
    bool in_font = false;
    AndroidFontFile font;
    std::string font_text;
    std::size_t added = 0;

    auto finish_family = [&]() {
        if (!family) return;
        if (group) {
            for (AndroidFontFile& f : family->fonts) group->fonts.push_back(std::move(f));
        } else if (!family->fonts.empty()) {
            if (!family->name.empty()) m_named.push_back(std::move(family));
            else m_fallbacks.push_back(std::move(family));
            ++added;
        }
        family.reset();
    };

    while (true) {
        const bool more = xml.next(tag, text);
        if (in_font) font_text += text;
        if (!more) break;
        if (tag.name == "family-list") {
            if (tag.closing) {
                if (group && !group->fonts.empty()) {
                    m_named.push_back(std::move(group));
                    ++added;
                }
                group.reset();
            } else if (!tag.self_closing) {
                group = std::make_unique<AndroidFontFamily>();
                group->name = lower(tag.attr("name"));
            }
            continue;
        }
        if (tag.name == "family") {
            if (tag.closing) {
                finish_family();
            } else {
                finish_family();
                family = std::make_unique<AndroidFontFamily>();
                family->name = lower(tag.attr("name"));
                family->lang = tag.attr("lang");
                family->variant = tag.attr("variant");
                if (tag.self_closing) finish_family();
            }
            continue;
        }
        if (tag.name == "font") {
            if (tag.closing) {
                if (in_font) {
                    font.path = android_resolve_font_path(font_text);
                    if (!font.path.empty() && family) family->fonts.push_back(font);
                }
                in_font = false;
            } else if (!tag.self_closing) {
                in_font = true;
                font = AndroidFontFile{};
                font_text.clear();
                const std::string w = tag.attr("weight");
                if (!w.empty()) font.weight = std::max(1, std::min(1000, std::atoi(w.c_str())));
                font.italic = lower(tag.attr("style")) == "italic";
                const std::string idx = tag.attr("index");
                if (!idx.empty()) font.index = std::max(0, std::atoi(idx.c_str()));
            }
            continue;
        }
        if (tag.name == "axis" && in_font) {
            const std::string t = tag.attr("tag");
            const std::string v = tag.attr("stylevalue");
            if (t.size() == 4 && !v.empty()) font.axes.emplace_back(tag_of(t), static_cast<float>(std::atof(v.c_str())));
            continue;
        }
        if (tag.name == "alias" && !tag.closing) {
            AndroidFontAlias a;
            a.name = lower(tag.attr("name"));
            a.to = lower(tag.attr("to"));
            const std::string w = tag.attr("weight");
            if (!w.empty()) a.weight = std::atoi(w.c_str());
            if (!a.name.empty() && !a.to.empty()) m_aliases.push_back(std::move(a));
            continue;
        }
    }
    finish_family();
    return added > 0;
}

void AndroidFontCatalog::scan_directory() {
    DIR* dir = ::opendir("/system/fonts");
    if (!dir) return;
    std::vector<std::string> files;
    while (dirent* e = ::readdir(dir)) {
        const std::string name = e->d_name;
        const std::string l = lower(name);
        if (l.size() > 4 && (l.compare(l.size() - 4, 4, ".ttf") == 0 || l.compare(l.size() - 4, 4, ".otf") == 0 || l.compare(l.size() - 4, 4, ".ttc") == 0)) files.push_back(name);
    }
    ::closedir(dir);
    std::sort(files.begin(), files.end());
    auto family_named = [&](const char* name) -> AndroidFontFamily& {
        for (auto& f : m_named) if (f->name == name) return *f;
        m_named.push_back(std::make_unique<AndroidFontFamily>());
        m_named.back()->name = name;
        return *m_named.back();
    };
    for (const std::string& name : files) {
        AndroidFontFile f;
        f.path = "/system/fonts/" + name;
        f.weight = guess_weight(name);
        f.italic = lower(name).find("italic") != std::string::npos;
        const std::string l = lower(name);
        if (l.rfind("roboto-", 0) == 0 || l.rfind("droidsans.", 0) == 0 || l.rfind("droidsans-", 0) == 0) family_named("sans-serif").fonts.push_back(f);
        else if (l.rfind("notoserif-", 0) == 0 || l.rfind("droidserif", 0) == 0) family_named("serif").fonts.push_back(f);
        else if (l.rfind("droidsansmono", 0) == 0 || l.rfind("cutivemono", 0) == 0) family_named("monospace").fonts.push_back(f);
        else {
            auto fam = std::make_unique<AndroidFontFamily>();
            fam->fonts.push_back(f);
            m_fallbacks.push_back(std::move(fam));
        }
    }
}

void AndroidFontCatalog::load() {
    if (m_loaded) return;
    m_loaded = true;
    bool ok = false;
    for (const char* path : { "/system/etc/font_fallback.xml", "/system/etc/fonts.xml", "/system/etc/system_fonts.xml" }) {
        if (parse_file(path)) { ok = true; break; }
    }
    if (ok && file_exists("/system/etc/fallback_fonts.xml") && m_fallbacks.empty()) parse_file("/system/etc/fallback_fonts.xml");
    if (!ok) scan_directory();
}

const AndroidFontFamily* AndroidFontCatalog::find_family(const std::string& name, int& alias_weight) const {
    alias_weight = 0;
    for (const auto& f : m_user) if (f->name == name) return f.get();
    for (const auto& f : m_named) if (f->name == name) return f.get();
    std::string target = name;
    for (int hop = 0; hop < 4; ++hop) {
        bool moved = false;
        for (const AndroidFontAlias& a : m_aliases) {
            if (a.name != target) continue;
            target = a.to;
            if (a.weight > 0 && alias_weight == 0) alias_weight = a.weight;
            moved = true;
            break;
        }
        if (!moved) break;
        for (const auto& f : m_named) if (f->name == target) return f.get();
    }
    return nullptr;
}

AndroidFontPick AndroidFontCatalog::best_of(const AndroidFontFamily& family, int weight, bool italic) noexcept {
    AndroidFontPick pick;
    const AndroidFontFile* best = nullptr;
    int best_score = 1 << 30;
    for (const AndroidFontFile& f : family.fonts) {
        int score = std::abs(f.weight - weight) * 2;
        if (weight >= 400 && f.weight < weight) score += 1;
        if (weight < 400 && f.weight > weight) score += 1;
        if (f.italic != italic) score += 10000;
        if (score < best_score) {
            best_score = score;
            best = &f;
        }
    }
    pick.font = best;
    if (best) {
        pick.embolden = weight >= 600 && best->weight + 150 < weight;
        pick.slant = italic && !best->italic ? 0.2f : 0.0f;
    }
    return pick;
}

std::vector<AndroidFontPick> AndroidFontCatalog::candidates(const std::string& families, int weight, bool italic) {
    std::lock_guard<std::mutex> lock(m_mutex);
    load();
    std::vector<AndroidFontPick> out;
    std::vector<std::string> seen;
    auto add = [&](const AndroidFontPick& p) {
        if (!p.font) return;
        const std::string k = p.font->key();
        if (std::find(seen.begin(), seen.end(), k) != seen.end()) return;
        seen.push_back(k);
        out.push_back(p);
    };
    bool want_emoji = false;
    std::size_t at = 0;
    while (at <= families.size()) {
        std::size_t comma = families.find(',', at);
        if (comma == std::string::npos) comma = families.size();
        std::string name = lower(trim(families.substr(at, comma - at)));
        at = comma + 1;
        if (name.empty()) continue;
        if (name == "system-ui" || name == "ui-sans-serif" || name == "-apple-system" || name == "blinkmacsystemfont" || name == "sans") name = "sans-serif";
        else if (name == "ui-serif") name = "serif";
        else if (name == "ui-monospace" || name == "mono") name = "monospace";
        else if (name == "ui-rounded") name = "sans-serif";
        if (name == "emoji") { want_emoji = true; continue; }
        int alias_weight = 0;
        if (const AndroidFontFamily* fam = find_family(name, alias_weight)) add(best_of(*fam, alias_weight > 0 ? alias_weight : weight, italic));
    }
    if (want_emoji) {
        for (const auto& f : m_fallbacks) {
            if (f->lang.find("Zsye") != std::string::npos) add(best_of(*f, weight, italic));
        }
    }
    int unused = 0;
    if (const AndroidFontFamily* sans = find_family("sans-serif", unused)) add(best_of(*sans, weight, italic));
    for (const auto& f : m_fallbacks) {
        if (f->variant == "compact") continue;
        add(best_of(*f, weight, italic));
    }
    if (out.empty()) {
        for (const auto& f : m_named) add(best_of(*f, weight, italic));
    }
    return out;
}

bool AndroidFontCatalog::add_user_font(const std::string& path, int index, const std::string& family, int weight, bool italic) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!file_exists(path)) return false;
    const std::string name = lower(trim(family));
    AndroidFontFamily* fam = nullptr;
    for (auto& f : m_user) if (f->name == name) fam = f.get();
    if (!fam) {
        m_user.push_back(std::make_unique<AndroidFontFamily>());
        fam = m_user.back().get();
        fam->name = name;
    }
    AndroidFontFile f;
    f.path = path;
    f.index = index;
    f.weight = weight;
    f.italic = italic;
    for (const AndroidFontFile& existing : fam->fonts) if (existing.key() == f.key()) return true;
    fam->fonts.push_back(std::move(f));
    return true;
}

std::vector<std::string> AndroidFontCatalog::family_names() {
    std::lock_guard<std::mutex> lock(m_mutex);
    load();
    std::vector<std::string> out;
    for (const auto& f : m_user) out.push_back(f->name);
    for (const auto& f : m_named) out.push_back(f->name);
    for (const AndroidFontAlias& a : m_aliases) out.push_back(a.name);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // OS_ANDROID
