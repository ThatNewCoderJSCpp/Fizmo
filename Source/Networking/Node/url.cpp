#include "fizmo_library.hpp"
#include "url.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace fizmo {
namespace networking {

namespace {

const char kHex[] = "0123456789ABCDEF";

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percent_decode(const std::string& s, bool plus_as_space) {
    std::string out;
    out.reserve(s.size());

    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        if (c == '%' && i + 2 < s.size()) {
            const int hi = hex_value(s[i + 1]);
            const int lo = hex_value(s[i + 2]);

            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }

        out.push_back(plus_as_space && c == '+' ? ' ' : c);
    }

    return out;
}

template <typename Keep>
std::string percent_encode(const std::string& s, Keep keep) {
    std::string out;
    out.reserve(s.size());

    for (unsigned char c : s) {
        if (keep(c)) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 15]);
        }
    }

    return out;
}

bool is_c0_or_space(unsigned char c) noexcept { return c <= 0x20; }

bool in_c0_control_set(unsigned char c) noexcept { return c < 0x20 || c >= 0x7F; }

bool in_fragment_set(unsigned char c) noexcept { return in_c0_control_set(c) || c == ' ' || c == '"' || c == '<' || c == '>' || c == '`'; }

bool in_query_set(unsigned char c) noexcept { return in_c0_control_set(c) || c == ' ' || c == '"' || c == '#' || c == '<' || c == '>'; }

bool in_special_query_set(unsigned char c) noexcept { return in_query_set(c) || c == '\''; }

bool in_path_set(unsigned char c) noexcept { return in_query_set(c) || c == '?' || c == '`' || c == '{' || c == '}' || c == '^'; }

bool in_userinfo_set(unsigned char c) noexcept {
    return in_path_set(c) || c == '/' || c == ':' || c == ';' || c == '=' || c == '@' || (c >= '[' && c <= ']') || c == '|';
}

bool is_form_safe(unsigned char c) noexcept { return std::isalnum(c) || c == '*' || c == '-' || c == '.' || c == '_'; }

std::string form_encode(const std::string& s) {
    std::string out;

    for (unsigned char c : s) {
        if (is_form_safe(c)) out.push_back(static_cast<char>(c));
        else if (c == ' ') out.push_back('+');
        else { out.push_back('%'); out.push_back(kHex[c >> 4]); out.push_back(kHex[c & 15]); }
    }

    return out;
}

std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool is_special_scheme(const std::string& s) noexcept {
    return s == "http" || s == "https" || s == "ws" || s == "wss" || s == "ftp" || s == "file";
}

bool is_single_dot(const std::string& seg) {
    return seg == "." || to_lower(seg) == "%2e";
}

bool is_double_dot(const std::string& seg) {
    const std::string l = to_lower(seg);
    return l == ".." || l == ".%2e" || l == "%2e." || l == "%2e%2e";
}

std::string normalize_path(const std::string& raw, bool special) {
    std::string path = raw;
    if (special) std::replace(path.begin(), path.end(), '\\', '/');
    std::vector<std::string> out;
    std::size_t i = (!path.empty() && path[0] == '/') ? 1 : 0;
    bool trailing = false;

    while (true) {
        const std::size_t slash = path.find('/', i);
        const std::string seg = path.substr(i, slash == std::string::npos ? std::string::npos : slash - i);
        const bool last = slash == std::string::npos;

        if (is_double_dot(seg)) {
            if (!out.empty()) out.pop_back();
            trailing = true;
        } else if (is_single_dot(seg)) {
            trailing = true;
        } else {
            out.push_back(seg);
            trailing = false;
        }

        if (last) break;
        i = slash + 1;
    }

    std::string result;
    for (auto& s : out) { result.push_back('/'); result += s; }
    if (trailing) result.push_back('/');
    if (result.empty()) result = "/";
    return percent_encode(result, [](unsigned char c) { return !in_path_set(c); });
}

bool valid_host_char(unsigned char c) noexcept {
    if (c <= 0x20 || c == 0x7F) return false;
    switch (c) {
        case '#': case '%': case '/': case ':': case '<': case '>': case '?': case '@': case '[': case '\\': case ']': case '^': case '|':
            return false;
        default:
            return true;
    }
}

bool parse_host(const std::string& input, bool special, std::string& out, std::string* error) {
    if (input.empty()) {
        if (special) { if (error) *error = "Empty host"; return false; }
        out.clear();
        return true;
    }

    if (input.front() == '[') {
        if (input.back() != ']' || input.size() < 3) { if (error) *error = "Invalid IPv6 address"; return false; }

        for (std::size_t i = 1; i + 1 < input.size(); ++i) {
            const char c = input[i];
            if (!(std::isxdigit(static_cast<unsigned char>(c)) || c == ':' || c == '.')) { if (error) *error = "Invalid IPv6 address"; return false; }
        }

        out = to_lower(input);
        return true;
    }

    const std::string decoded = special ? to_lower(percent_decode(input, false)) : input;

    for (unsigned char c : decoded) {
        if (special && !valid_host_char(c)) { if (error) *error = "Invalid host"; return false; }
        if (!special && (c <= 0x20 || c == '#' || c == '/' || c == '<' || c == '>' || c == '?' || c == '@' || c == '[' || c == '\\' || c == ']' || c == '^' || c == '|')) {
            if (error) *error = "Invalid host";
            return false;
        }
    }

    out = special ? decoded : percent_encode(decoded, [](unsigned char c) { return !in_c0_control_set(c); });
    return true;
}

std::string clean_input(const std::string& in) {
    std::size_t b = 0;
    std::size_t e = in.size();
    while (b < e && is_c0_or_space(static_cast<unsigned char>(in[b]))) ++b;
    while (e > b && is_c0_or_space(static_cast<unsigned char>(in[e - 1]))) --e;
    std::string out;
    out.reserve(e - b);

    for (std::size_t i = b; i < e; ++i) {
        if (in[i] == '\t' || in[i] == '\n' || in[i] == '\r') continue;
        out.push_back(in[i]);
    }

    return out;
}

bool split_scheme(const std::string& s, std::string& scheme, std::string& rest) {
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) return false;

    for (std::size_t i = 1; i < s.size(); ++i) {
        const char c = s[i];
        if (c == ':') { scheme = to_lower(s.substr(0, i)); rest = s.substr(i + 1); return true; }
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.')) return false;
    }

    return false;
}

} // namespace

URLSearchParams::URLSearchParams(const std::string& query) {
    std::string q = query;
    if (!q.empty() && q[0] == '?') q.erase(0, 1);
    std::size_t i = 0;

    while (i <= q.size()) {
        std::size_t amp = q.find('&', i);
        if (amp == std::string::npos) amp = q.size();
        const std::string part = q.substr(i, amp - i);

        if (!part.empty()) {
            const std::size_t eq = part.find('=');
            if (eq == std::string::npos) m_entries.emplace_back(percent_decode(part, true), std::string());
            else m_entries.emplace_back(percent_decode(part.substr(0, eq), true), percent_decode(part.substr(eq + 1), true));
        }

        i = amp + 1;
    }
}

void URLSearchParams::append(const std::string& name, const std::string& value) { m_entries.emplace_back(name, value); }

void URLSearchParams::remove(const std::string& name) {
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const Entry& e) { return e.first == name; }), m_entries.end());
}

void URLSearchParams::remove(const std::string& name, const std::string& value) {
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const Entry& e) { return e.first == name && e.second == value; }), m_entries.end());
}

void URLSearchParams::set(const std::string& name, const std::string& value) {
    bool found = false;
    std::vector<Entry> out;

    for (auto& e : m_entries) {
        if (e.first != name) { out.push_back(e); continue; }
        if (found) continue;
        found = true;
        out.emplace_back(name, value);
    }

    if (!found) out.emplace_back(name, value);
    m_entries.swap(out);
}

bool URLSearchParams::has(const std::string& name) const {
    for (auto& e : m_entries) if (e.first == name) return true;
    return false;
}

std::string URLSearchParams::get(const std::string& name, const std::string& fallback) const {
    for (auto& e : m_entries) if (e.first == name) return e.second;
    return fallback;
}

std::vector<std::string> URLSearchParams::get_all(const std::string& name) const {
    std::vector<std::string> out;
    for (auto& e : m_entries) if (e.first == name) out.push_back(e.second);
    return out;
}

void URLSearchParams::sort() {
    std::stable_sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) { return a.first < b.first; });
}

std::string URLSearchParams::to_string() const {
    std::string out;

    for (auto& e : m_entries) {
        if (!out.empty()) out.push_back('&');
        out += form_encode(e.first);
        out.push_back('=');
        out += form_encode(e.second);
    }

    return out;
}

std::uint16_t URL::default_port(const std::string& scheme) noexcept {
    if (scheme == "http" || scheme == "ws") return 80;
    if (scheme == "https" || scheme == "wss") return 443;
    if (scheme == "ftp") return 21;
    return 0;
}

URL::URL(const std::string& input) {
    std::string error;
    if (!parse(input, *this, &error)) throw std::invalid_argument("Invalid URL \"" + input + "\": " + error);
}

URL::URL(const std::string& input, const std::string& base) {
    URL b(base);
    std::string error;
    if (!parse(input, b, *this, &error)) throw std::invalid_argument("Invalid URL \"" + input + "\": " + error);
}

URL::URL(const std::string& input, const URL& base) {
    std::string error;
    if (!parse(input, base, *this, &error)) throw std::invalid_argument("Invalid URL \"" + input + "\": " + error);
}

bool URL::parse(const std::string& raw, URL& out, std::string* error) {
    const std::string input = clean_input(raw);
    std::string scheme;
    std::string rest;

    if (!split_scheme(input, scheme, rest)) {
        if (error) *error = "Missing scheme";
        return false;
    }

    URL u;
    u.m_scheme = scheme;
    const bool special = is_special_scheme(scheme);
    std::size_t pos = 0;

    if (special) {
        while (pos < rest.size() && (rest[pos] == '/' || rest[pos] == '\\')) ++pos;
        u.m_has_authority = true;
    } else if (rest.compare(0, 2, "//") == 0) {
        pos = 2;
        u.m_has_authority = true;
    }

    if (u.m_has_authority) {
        std::size_t end = pos;

        while (end < rest.size()) {
            const char c = rest[end];
            if (c == '/' || c == '?' || c == '#' || (special && c == '\\')) break;
            ++end;
        }

        std::string authority = rest.substr(pos, end - pos);
        pos = end;
        const std::size_t at = authority.rfind('@');

        if (at != std::string::npos) {
            const std::string userinfo = authority.substr(0, at);
            authority.erase(0, at + 1);
            const std::size_t colon = userinfo.find(':');
            auto keep = [](unsigned char c) { return !in_userinfo_set(c); };
            u.m_username = percent_encode(userinfo.substr(0, colon), keep);
            if (colon != std::string::npos) u.m_password = percent_encode(userinfo.substr(colon + 1), keep);
        }

        std::string host_part = authority;
        std::string port_part;
        bool has_port = false;

        if (!authority.empty() && authority[0] == '[') {
            const std::size_t close = authority.find(']');
            if (close == std::string::npos) { if (error) *error = "Invalid IPv6 address"; return false; }
            host_part = authority.substr(0, close + 1);

            if (close + 1 < authority.size()) {
                if (authority[close + 1] != ':') { if (error) *error = "Invalid host"; return false; }
                port_part = authority.substr(close + 2);
                has_port = true;
            }
        } else {
            const std::size_t colon = authority.rfind(':');

            if (colon != std::string::npos) {
                host_part = authority.substr(0, colon);
                port_part = authority.substr(colon + 1);
                has_port = true;
            }
        }

        if (has_port && !port_part.empty()) {
            unsigned long value = 0;

            for (char c : port_part) {
                if (!std::isdigit(static_cast<unsigned char>(c))) { if (error) *error = "Invalid port"; return false; }
                value = value * 10 + static_cast<unsigned long>(c - '0');
                if (value > 65535) { if (error) *error = "Invalid port"; return false; }
            }

            if (!(special && value == default_port(scheme))) u.m_port = std::to_string(value);
        }

        if (!parse_host(host_part, special && scheme != "file", u.m_hostname, error)) return false;
    }

    std::size_t qpos = rest.find('?', pos);
    std::size_t hpos = rest.find('#', pos);
    if (qpos != std::string::npos && hpos != std::string::npos && qpos > hpos) qpos = std::string::npos;
    const std::size_t path_end = std::min(qpos, hpos);
    const std::string path = rest.substr(pos, path_end == std::string::npos ? std::string::npos : path_end - pos);

    if (special || u.m_has_authority || (!path.empty() && path[0] == '/')) {
        if (special || !path.empty()) u.m_path = normalize_path(path, special);
        if (!special && u.m_has_authority && path.empty()) u.m_path.clear();
    } else {
        u.m_opaque = true;
        u.m_path = percent_encode(path, [](unsigned char c) { return !in_c0_control_set(c); });
    }

    if (qpos != std::string::npos) {
        const std::size_t qend = hpos == std::string::npos ? rest.size() : hpos;
        const std::string q = rest.substr(qpos + 1, qend - qpos - 1);
        u.m_has_query = true;
        u.m_query = special ? percent_encode(q, [](unsigned char c) { return !in_special_query_set(c); }) : percent_encode(q, [](unsigned char c) { return !in_query_set(c); });
    }

    if (hpos != std::string::npos) {
        const std::string f = rest.substr(hpos + 1);
        u.m_fragment = "#" + percent_encode(f, [](unsigned char c) { return !in_fragment_set(c); });
        if (u.m_fragment == "#") u.m_fragment.clear();
    }

    u.m_params = URLSearchParams(u.m_query);
    u.m_valid = true;
    out = std::move(u);
    return true;
}

bool URL::parse(const std::string& raw, const URL& base, URL& out, std::string* error) {
    const std::string input = clean_input(raw);
    std::string scheme;
    std::string rest;

    if (split_scheme(input, scheme, rest)) {
        const bool rel_special = is_special_scheme(scheme) && base.valid() && base.m_scheme == scheme && (rest.empty() || (rest[0] != '/' && rest[0] != '\\'));
        if (!rel_special) return parse(input, out, error);
        return parse(rest, base, out, error);
    }

    if (!base.valid()) { if (error) *error = "Invalid base URL"; return false; }
    const bool special = base.is_special();

    if (base.m_opaque) {
        if (!input.empty() && input[0] == '#') {
            URL u = base;
            u.m_fragment = input.size() > 1 ? "#" + percent_encode(input.substr(1), [](unsigned char c) { return !in_fragment_set(c); }) : std::string();
            out = u;
            return true;
        }

        if (error) *error = "Cannot resolve relative URL against an opaque base";
        return false;
    }

    auto is_slash = [special](char c) { return c == '/' || (special && c == '\\'); };
    std::string authority;

    if (base.m_has_authority) {
        authority = "//";

        if (!base.m_username.empty() || !base.m_password.empty()) {
            authority += base.m_username;
            if (!base.m_password.empty()) authority += ":" + base.m_password;
            authority += "@";
        }

        authority += base.host();
    }

    const std::string prefix = base.m_scheme + ":";

    if (input.size() >= 2 && is_slash(input[0]) && is_slash(input[1])) return parse(prefix + input, out, error);
    if (!input.empty() && is_slash(input[0])) return parse(prefix + authority + input, out, error);

    const std::string base_query = base.m_has_query ? "?" + base.m_query : std::string();

    if (input.empty()) return parse(prefix + authority + base.m_path + base_query, out, error);
    if (input[0] == '?') return parse(prefix + authority + base.m_path + input, out, error);
    if (input[0] == '#') return parse(prefix + authority + base.m_path + base_query + input, out, error);

    std::string dir = base.m_path;
    const std::size_t slash = dir.rfind('/');
    dir = slash == std::string::npos ? std::string("/") : dir.substr(0, slash + 1);
    return parse(prefix + authority + dir + input, out, error);
}

bool URL::is_special() const noexcept { return is_special_scheme(m_scheme); }

std::string URL::host() const {
    if (m_port.empty()) return m_hostname;
    return m_hostname + ":" + m_port;
}

std::string URL::bare_hostname() const {
    if (m_hostname.size() >= 2 && m_hostname.front() == '[' && m_hostname.back() == ']') return m_hostname.substr(1, m_hostname.size() - 2);
    return m_hostname;
}

std::uint16_t URL::port_number() const noexcept {
    if (m_port.empty()) return default_port(m_scheme);
    return static_cast<std::uint16_t>(std::stoul(m_port));
}

std::string URL::search() const {
    if (m_params_dirty) {
        const std::string q = m_params.to_string();
        return q.empty() ? std::string() : "?" + q;
    }

    if (!m_has_query || m_query.empty()) return {};
    return "?" + m_query;
}

std::string URL::href() const {
    if (!m_valid) return {};
    std::string out = m_scheme + ":";

    if (m_has_authority) {
        out += "//";

        if (!m_username.empty() || !m_password.empty()) {
            out += m_username;
            if (!m_password.empty()) out += ":" + m_password;
            out += "@";
        }

        out += host();
    }

    out += m_path;

    if (m_params_dirty) out += search();
    else if (m_has_query) out += "?" + m_query;

    out += m_fragment;
    return out;
}

std::string URL::origin() const {
    if (!m_valid || !is_special() || m_scheme == "file") return "null";
    return m_scheme + "://" + host();
}

std::string URL::path_and_query() const {
    std::string p = m_path.empty() ? std::string("/") : m_path;
    if (m_params_dirty) return p + search();
    if (m_has_query) return p + "?" + m_query;
    return p;
}

URLSearchParams& URL::search_params() {
    m_params_dirty = true;
    return m_params;
}

URL& URL::set_protocol(const std::string& value) {
    std::string v = to_lower(value);
    if (!v.empty() && v.back() == ':') v.pop_back();
    if (v.empty() || is_special_scheme(v) != is_special()) return *this;
    m_scheme = v;
    if (!m_port.empty() && static_cast<unsigned long>(std::stoul(m_port)) == default_port(v)) m_port.clear();
    return *this;
}

URL& URL::set_username(const std::string& value) {
    if (!m_has_authority || m_hostname.empty()) return *this;
    m_username = percent_encode(value, [](unsigned char c) { return !in_userinfo_set(c); });
    return *this;
}

URL& URL::set_password(const std::string& value) {
    if (!m_has_authority || m_hostname.empty()) return *this;
    m_password = percent_encode(value, [](unsigned char c) { return !in_userinfo_set(c); });
    return *this;
}

URL& URL::set_hostname(const std::string& value) {
    std::string h;
    if (m_opaque || !parse_host(value, is_special() && m_scheme != "file", h, nullptr)) return *this;
    m_hostname = h;
    m_has_authority = true;
    return *this;
}

URL& URL::set_port(const std::string& value) {
    if (m_hostname.empty() || m_scheme == "file") return *this;
    if (value.empty()) { m_port.clear(); return *this; }
    unsigned long v = 0;
    std::size_t i = 0;

    for (; i < value.size() && std::isdigit(static_cast<unsigned char>(value[i])); ++i) {
        v = v * 10 + static_cast<unsigned long>(value[i] - '0');
        if (v > 65535) return *this;
    }

    if (i == 0) return *this;
    m_port = (is_special() && v == default_port(m_scheme)) ? std::string() : std::to_string(v);
    return *this;
}

URL& URL::set_pathname(const std::string& value) {
    if (m_opaque) return *this;
    std::string v = value;
    if (v.empty() || (v[0] != '/' && !(is_special() && v[0] == '\\'))) v = "/" + v;
    m_path = normalize_path(v, is_special());
    return *this;
}

URL& URL::set_search(const std::string& value) {
    std::string v = value;
    if (!v.empty() && v[0] == '?') v.erase(0, 1);
    m_params_dirty = false;

    if (v.empty()) {
        m_has_query = false;
        m_query.clear();
        m_params.clear();
        return *this;
    }

    m_has_query = true;
    m_query = is_special() ? percent_encode(v, [](unsigned char c) { return !in_special_query_set(c); }) : percent_encode(v, [](unsigned char c) { return !in_query_set(c); });
    m_params = URLSearchParams(m_query);
    return *this;
}

URL& URL::set_hash(const std::string& value) {
    std::string v = value;
    if (!v.empty() && v[0] == '#') v.erase(0, 1);
    m_fragment = v.empty() ? std::string() : "#" + percent_encode(v, [](unsigned char c) { return !in_fragment_set(c); });
    return *this;
}

namespace querystring {

std::string escape(const std::string& s) { return encode_uri_component(s); }

std::string unescape(const std::string& s) { return percent_decode(s, false); }

std::vector<std::pair<std::string, std::string>> parse(const std::string& query, char separator, char equals) {
    std::vector<std::pair<std::string, std::string>> out;
    std::size_t i = 0;

    while (i <= query.size()) {
        std::size_t sep = query.find(separator, i);
        if (sep == std::string::npos) sep = query.size();
        const std::string part = query.substr(i, sep - i);

        if (!part.empty()) {
            const std::size_t eq = part.find(equals);
            if (eq == std::string::npos) out.emplace_back(percent_decode(part, true), std::string());
            else out.emplace_back(percent_decode(part.substr(0, eq), true), percent_decode(part.substr(eq + 1), true));
        }

        i = sep + 1;
    }

    return out;
}

std::string stringify(const std::vector<std::pair<std::string, std::string>>& entries, char separator, char equals) {
    std::string out;

    for (auto& e : entries) {
        if (!out.empty()) out.push_back(separator);
        out += encode_uri_component(e.first);
        out.push_back(equals);
        out += encode_uri_component(e.second);
    }

    return out;
}

} // namespace querystring

std::string encode_uri_component(const std::string& s) {
    return percent_encode(s, [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')';
    });
}

std::string decode_uri_component(const std::string& s) { return percent_decode(s, false); }

std::string encode_uri(const std::string& s) {
    return percent_encode(s, [](unsigned char c) {
        if (std::isalnum(c)) return true;
        switch (c) {
            case '-': case '_': case '.': case '!': case '~': case '*': case '\'': case '(': case ')':
            case ';': case ',': case '/': case '?': case ':': case '@': case '&': case '=': case '+': case '$': case '#':
                return true;
            default:
                return false;
        }
    });
}

} // namespace networking
} // namespace fizmo
