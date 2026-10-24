#include "fizmo_library.hpp"
#include "cookies.hpp"

#include <stdexcept>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

bool is_token_char(unsigned char c) noexcept {
    if (c <= 0x20 || c >= 0x7f) return false;

    switch (c) {
        case '(': case ')': case '<': case '>': case '@': case ',': case ';': case ':':
        case '\\': case '"': case '/': case '[': case ']': case '?': case '=': case '{': case '}':
            return false;
        default:
            return true;
    }
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

bool safe_attribute(const std::string& s) noexcept {
    for (unsigned char c : s) {
        if (c < 0x20 || c == 0x7f || c == ';') return false;
    }

    return true;
}

} // namespace

bool valid_cookie_name(const std::string& name) noexcept {
    if (name.empty()) return false;

    for (unsigned char c : name) {
        if (!is_token_char(c)) return false;
    }

    return true;
}

bool valid_cookie_value(const std::string& value) noexcept {
    for (unsigned char c : value) {
        if (c < 0x21 || c == '"' || c == ',' || c == ';' || c == '\\' || c >= 0x7f) return false;
    }

    return true;
}

std::string make_set_cookie(const std::string& name, const std::string& value, const CookieOptions& options) {
    if (!valid_cookie_name(name)) throw std::invalid_argument("invalid cookie name");
    if (!valid_cookie_value(value)) throw std::invalid_argument("invalid cookie value");
    if (!safe_attribute(options.path) || !safe_attribute(options.domain)) throw std::invalid_argument("invalid cookie attribute");

    const bool host_prefix = name.rfind("__Host-", 0) == 0;
    const bool secure_prefix = name.rfind("__Secure-", 0) == 0;
    if ((host_prefix || secure_prefix) && !options.secure) throw std::invalid_argument("__Host- and __Secure- cookies must be Secure");
    if (host_prefix && (!options.domain.empty() || options.path != "/")) throw std::invalid_argument("__Host- cookies need Path=/ and no Domain");
    if (options.same_site == SameSite::None && !options.secure) throw std::invalid_argument("SameSite=None cookies must be Secure");

    std::string out = name + "=" + value;
    if (!options.path.empty()) out += "; Path=" + options.path;
    if (!options.domain.empty()) out += "; Domain=" + options.domain;

    if (options.max_age) {
        const auto secs = options.max_age->count() < 0 ? 0 : options.max_age->count();
        out += "; Max-Age=" + std::to_string(secs);
        if (secs == 0) out += "; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
    }

    if (options.secure) out += "; Secure";
    if (options.http_only) out += "; HttpOnly";

    switch (options.same_site) {
        case SameSite::Strict: out += "; SameSite=Strict"; break;
        case SameSite::Lax:    out += "; SameSite=Lax"; break;
        case SameSite::None:   out += "; SameSite=None"; break;
    }

    if (options.partitioned) out += "; Partitioned";
    return out;
}

std::string make_expired_cookie(const std::string& name, const CookieOptions& options) {
    CookieOptions o = options;
    o.max_age = std::chrono::seconds(0);
    return make_set_cookie(name, "", o);
}

std::map<std::string, std::string> parse_cookie_header(const std::string& header) {
    std::map<std::string, std::string> out;
    std::size_t pos = 0;

    while (pos <= header.size()) {
        const std::size_t semi = header.find(';', pos);
        const std::string part = header.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
        const std::size_t eq = part.find('=');

        if (eq != std::string::npos) {
            const std::string name = trim(part.substr(0, eq));
            std::string value = trim(part.substr(eq + 1));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
            if (!name.empty() && out.find(name) == out.end()) out.emplace(name, value);
        }

        if (semi == std::string::npos) break;
        pos = semi + 1;
    }

    return out;
}

std::string cookie_value(const std::string& header, const std::string& name) {
    const auto all = parse_cookie_header(header);
    const auto it = all.find(name);
    return it == all.end() ? std::string() : it->second;
}

std::string clear_site_data_header(ClearSiteData what) {
    if (what == ClearSiteData::All) return "\"*\"";
    std::string out;
    auto add = [&](const char* v) { if (!out.empty()) out += ", "; out += v; };
    if (has_flag(what, ClearSiteData::Cookies)) add("\"cookies\"");
    if (has_flag(what, ClearSiteData::Storage)) add("\"storage\"");
    if (has_flag(what, ClearSiteData::Cache)) add("\"cache\"");
    if (has_flag(what, ClearSiteData::ExecutionContexts)) add("\"executionContexts\"");
    return out;
}

} // namespace guard
} // namespace networking
} // namespace fizmo
