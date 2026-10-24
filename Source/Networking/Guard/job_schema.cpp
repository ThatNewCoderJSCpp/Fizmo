#include "fizmo_library.hpp"
#include "job_schema.hpp"

#include <cmath>
#include <exception>
#include <set>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

constexpr std::size_t kMaxIssues = 32;
constexpr std::size_t kMaxKeyBytes = 256;
constexpr double kMaxSafeInteger = 9007199254740991.0;

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
bool is_alpha(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_alnum(char c) noexcept { return is_digit(c) || is_alpha(c); }
bool is_hex(char c) noexcept { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

bool digits_at(const std::string& s, std::size_t at, std::size_t n, int& out) {
    if (at + n > s.size()) return false;
    out = 0;

    for (std::size_t i = at; i < at + n; ++i) {
        if (!is_digit(s[i])) return false;
        out = out * 10 + (s[i] - '0');
    }

    return true;
}

bool valid_date(const std::string& s, std::size_t at) {
    int y = 0, m = 0, d = 0;
    if (!digits_at(s, at, 4, y) || s.size() < at + 10 || s[at + 4] != '-' || !digits_at(s, at + 5, 2, m) || s[at + 7] != '-' || !digits_at(s, at + 8, 2, d)) return false;
    if (m < 1 || m > 12 || d < 1) return false;
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    const int limit = m == 2 && leap ? 29 : days[m - 1];
    return d <= limit;
}

bool valid_email(const std::string& s) {
    if (s.size() > 254) return false;
    const std::size_t at = s.find('@');
    if (at == std::string::npos || at == 0 || at > 64 || s.find('@', at + 1) != std::string::npos) return false;
    static const std::string special = ".!#$%&'*+/=?^_`{|}~-";

    for (std::size_t i = 0; i < at; ++i) {
        const char c = s[i];
        if (!is_alnum(c) && special.find(c) == std::string::npos) return false;
        if (c == '.' && (i == 0 || i + 1 == at || s[i - 1] == '.')) return false;
    }

    const std::string domain = s.substr(at + 1);
    if (domain.empty() || domain.find('.') == std::string::npos) return false;
    std::size_t start = 0;

    for (;;) {
        const std::size_t dot = domain.find('.', start);
        const std::string label = domain.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
        for (char c : label) if (!is_alnum(c) && c != '-') return false;

        if (dot == std::string::npos) {
            if (label.size() < 2) return false;
            for (char c : label) if (!is_alpha(c)) return false;
            return true;
        }

        start = dot + 1;
    }
}

bool valid_url(const std::string& s) {
    if (s.size() > 2048) return false;
    std::size_t at = 0;
    if (s.compare(0, 7, "http://") == 0) at = 7;
    else if (s.compare(0, 8, "https://") == 0) at = 8;
    else return false;

    const std::size_t end = s.find_first_of("/?#", at);
    const std::string authority = s.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (authority.empty() || authority.find('@') != std::string::npos) return false;
    std::string host = authority;

    if (host.front() == '[') {
        const std::size_t close = host.find(']');
        if (close == std::string::npos) return false;
        for (std::size_t i = 1; i < close; ++i) if (!is_hex(host[i]) && host[i] != ':' && host[i] != '.') return false;
        host = host.substr(close + 1);
        if (!host.empty() && host.front() != ':') return false;
        if (!host.empty()) for (std::size_t i = 1; i < host.size(); ++i) if (!is_digit(host[i])) return false;
    } else {
        const std::size_t colon = host.find(':');

        if (colon != std::string::npos) {
            const std::string port = host.substr(colon + 1);
            if (port.empty() || port.size() > 5) return false;
            for (char c : port) if (!is_digit(c)) return false;
            host = host.substr(0, colon);
        }

        if (host.empty() || host.front() == '.' || host.back() == '.' || host.front() == '-') return false;
        for (char c : host) if (!is_alnum(c) && c != '.' && c != '-') return false;
    }

    for (std::size_t i = at; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x20 || c == 0x7f || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' || c == '{' || c == '|' || c == '}') return false;
    }

    return true;
}

bool valid_datetime(const std::string& s) {
    if (!valid_date(s, 0) || s.size() < 20 || (s[10] != 'T' && s[10] != 't')) return false;
    int hh = 0, mm = 0, ss = 0;
    if (!digits_at(s, 11, 2, hh) || s[13] != ':' || !digits_at(s, 14, 2, mm) || s[16] != ':' || !digits_at(s, 17, 2, ss)) return false;
    if (hh > 23 || mm > 59 || ss > 60) return false;
    std::size_t i = 19;

    if (i < s.size() && s[i] == '.') {
        const std::size_t start = ++i;
        while (i < s.size() && is_digit(s[i])) ++i;
        if (i == start || i - start > 9) return false;
    }

    if (i >= s.size()) return false;
    if ((s[i] == 'Z' || s[i] == 'z') && i + 1 == s.size()) return true;
    if (s[i] != '+' && s[i] != '-') return false;
    int oh = 0, om = 0;
    if (!digits_at(s, i + 1, 2, oh) || i + 3 >= s.size() || s[i + 3] != ':' || !digits_at(s, i + 4, 2, om)) return false;
    return i + 6 == s.size() && oh <= 23 && om <= 59;
}

const char* type_name(const json::Value& v) noexcept {
    switch (v.type()) {
        case json::Type::Null:   return "null";
        case json::Type::Bool:   return "a boolean";
        case json::Type::Number: return "a number";
        case json::Type::String: return "a string";
        case json::Type::Array:  return "an array";
        default:                 return "an object";
    }
}

const char* wanted_name(FieldType t) noexcept {
    switch (t) {
        case FieldType::Null:    return "null";
        case FieldType::Boolean: return "a boolean";
        case FieldType::Integer: return "a whole number";
        case FieldType::Number:  return "a number";
        case FieldType::String:  return "a string";
        case FieldType::Array:   return "an array";
        case FieldType::Object:  return "an object";
        default:                 return "any value";
    }
}

bool is_identifier(const std::string& s) noexcept {
    if (s.empty() || !(is_alpha(s[0]) || s[0] == '_')) return false;
    for (char c : s) if (!is_alnum(c) && c != '_') return false;
    return true;
}

std::string child(const std::string& path, const std::string& key) {
    if (is_identifier(key)) return path + "." + key;
    return path + "[" + json::Value(key).dump() + "]";
}

std::string index_path(const std::string& path, std::size_t i) {
    return path + "[" + std::to_string(i) + "]";
}

std::string number_text(double d) {
    return json::Value(d).dump();
}

bool has_control(const std::string& s, bool allow_whitespace) noexcept {
    for (unsigned char c : s) {
        if (c == 0x7f) return true;
        if (c < 0x20 && !(allow_whitespace && (c == '\t' || c == '\n' || c == '\r'))) return true;
    }

    return false;
}

const json::Value* member(const json::Value& obj, const std::string& name) {
    for (const auto& m : obj.members()) if (m.first == name) return &m.second;
    return nullptr;
}

std::string join(const std::vector<std::string>& names) {
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", " : "") + names[i];
    return out;
}

} // namespace

struct JobSchema::Walk {
    std::vector<SchemaIssue> issues;

    bool full() const noexcept { return issues.size() >= kMaxIssues; }

    void add(std::string path, std::string message) {
        if (full()) return;
        issues.push_back(SchemaIssue{ std::move(path), std::move(message) });
        if (full()) issues.push_back(SchemaIssue{ "", "too many problems; stopped checking" });
    }
};

std::string describe(const std::vector<SchemaIssue>& issues) {
    std::string out;
    for (std::size_t i = 0; i < issues.size(); ++i) out += (i ? "; " : "") + issues[i].to_string();
    return out;
}

const char* to_string(FieldType type) noexcept {
    switch (type) {
        case FieldType::Null:    return "null";
        case FieldType::Boolean: return "boolean";
        case FieldType::Integer: return "integer";
        case FieldType::Number:  return "number";
        case FieldType::String:  return "string";
        case FieldType::Array:   return "array";
        case FieldType::Object:  return "object";
        default:                 return "any";
    }
}

const char* to_string(StringFormat format) noexcept {
    switch (format) {
        case StringFormat::Identifier: return "identifier";
        case StringFormat::Email:      return "email address";
        case StringFormat::Uuid:       return "UUID";
        case StringFormat::Url:        return "http(s) URL";
        case StringFormat::Hex:        return "hex string";
        case StringFormat::Base64:     return "base64 string";
        case StringFormat::Digits:     return "string of digits";
        case StringFormat::Date:       return "date (YYYY-MM-DD)";
        case StringFormat::DateTime:   return "date and time (ISO 8601)";
        default:                       return "string";
    }
}

bool valid_utf8(const std::string& s) noexcept {
    const auto* p = reinterpret_cast<const unsigned char*>(s.data());
    const std::size_t n = s.size();
    std::size_t i = 0;

    while (i < n) {
        const unsigned char c = p[i];

        if (c < 0x80) { ++i; continue; }

        std::size_t len = 0;
        std::uint32_t cp = 0;

        if ((c & 0xe0) == 0xc0) { len = 2; cp = c & 0x1f; }
        else if ((c & 0xf0) == 0xe0) { len = 3; cp = c & 0x0f; }
        else if ((c & 0xf8) == 0xf0) { len = 4; cp = c & 0x07; }
        else return false;

        if (i + len > n) return false;

        for (std::size_t k = 1; k < len; ++k) {
            if ((p[i + k] & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (p[i + k] & 0x3f);
        }

        if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)) return false;
        if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        i += len;
    }

    return true;
}

std::size_t utf8_length(const std::string& s) noexcept {
    std::size_t n = 0;
    for (unsigned char c : s) if ((c & 0xc0) != 0x80) ++n;
    return n;
}

bool matches_format(const std::string& s, StringFormat format) noexcept {
    switch (format) {
        case StringFormat::None:       return true;
        case StringFormat::Identifier: return is_identifier(s);
        case StringFormat::Email:      return valid_email(s);
        case StringFormat::Url:        return valid_url(s);
        case StringFormat::Date:       return s.size() == 10 && valid_date(s, 0);
        case StringFormat::DateTime:   return valid_datetime(s);
        case StringFormat::Uuid: {
            if (s.size() != 36) return false;

            for (std::size_t i = 0; i < 36; ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
                else if (!is_hex(s[i])) return false;
            }

            return true;
        }
        case StringFormat::Hex: {
            if (s.empty()) return false;
            for (char c : s) if (!is_hex(c)) return false;
            return true;
        }
        case StringFormat::Digits: {
            if (s.empty()) return false;
            for (char c : s) if (!is_digit(c)) return false;
            return true;
        }
        case StringFormat::Base64: {
            if (s.empty() || s.size() % 4 != 0) return false;
            std::size_t pad = 0;

            for (std::size_t i = 0; i < s.size(); ++i) {
                const char c = s[i];

                if (c == '=') {
                    ++pad;
                    if (i + 3 < s.size()) return false;
                } else if (pad || !(is_alnum(c) || c == '+' || c == '/')) {
                    return false;
                }
            }

            return pad <= 2;
        }
    }

    return false;
}

FieldRule FieldRule::any() { return FieldRule(FieldType::Any); }
FieldRule FieldRule::string() { return FieldRule(FieldType::String); }
FieldRule FieldRule::integer() { return FieldRule(FieldType::Integer); }
FieldRule FieldRule::number() { return FieldRule(FieldType::Number); }
FieldRule FieldRule::boolean() { return FieldRule(FieldType::Boolean); }

FieldRule FieldRule::array(FieldRule items) {
    FieldRule r(FieldType::Array);
    r.m_items = std::make_shared<FieldRule>(std::move(items));
    return r;
}

FieldRule FieldRule::object(JobSchema fields) {
    FieldRule r(FieldType::Object);
    r.m_fields = std::make_shared<JobSchema>(std::move(fields));
    return r;
}

FieldRule& FieldRule::one_of(std::initializer_list<const char*> values) {
    m_one_of.clear();
    for (const char* v : values) m_one_of.emplace_back(v);
    return *this;
}

JobSchema JobSchema::object() {
    return JobSchema();
}

JobSchema JobSchema::text(std::size_t max_bytes) {
    JobSchema s;
    s.m_kind = Kind::Text;
    s.m_max_bytes = max_bytes;
    return s;
}

JobSchema JobSchema::empty() {
    JobSchema s;
    s.m_kind = Kind::Empty;
    s.m_max_bytes = 0;
    return s;
}

JobSchema& JobSchema::field(std::string name, FieldRule rule) {
    for (auto& f : m_fields) {
        if (f.first == name) { f.second = std::move(rule); return *this; }
    }

    m_fields.emplace_back(std::move(name), std::move(rule));
    return *this;
}

JobSchema& JobSchema::at_least_one_of(std::vector<std::string> names) {
    m_groups.push_back(Group{ GroupKind::AtLeastOne, std::move(names) });
    return *this;
}

JobSchema& JobSchema::only_one_of(std::vector<std::string> names) {
    m_groups.push_back(Group{ GroupKind::OnlyOne, std::move(names) });
    return *this;
}

JobSchema& JobSchema::all_or_none_of(std::vector<std::string> names) {
    m_groups.push_back(Group{ GroupKind::AllOrNone, std::move(names) });
    return *this;
}

std::vector<SchemaIssue> JobSchema::validate(const std::string& payload) const {
    Walk w;

    if (m_kind == Kind::Empty) {
        if (!payload.empty()) w.add("$", "this job takes no payload");
        return w.issues;
    }

    if (payload.size() > m_max_bytes) {
        w.add("$", "payload is " + std::to_string(payload.size()) + " bytes; the limit is " + std::to_string(m_max_bytes));
        return w.issues;
    }

    if (m_kind == Kind::Text) {
        if (!valid_utf8(payload)) w.add("$", "payload is not valid UTF-8");
        else if (has_control(payload, true)) w.add("$", "payload has control characters");
        return w.issues;
    }

    json::Value root;
    std::string error;

    if (!json::parse(payload.data(), payload.size(), root, &error)) {
        w.add("$", "payload is not valid JSON" + (error.empty() ? std::string() : ": " + error));
        return w.issues;
    }

    return validate_value(root);
}

std::vector<SchemaIssue> JobSchema::validate_value(const json::Value& root) const {
    Walk w;

    if (m_kind == Kind::Empty) {
        if (!root.is_null()) w.add("$", "this job takes no payload");
        return w.issues;
    }

    if (m_kind == Kind::Text) {
        if (!root.is_string()) w.add("$", "payload must be text");
        return w.issues;
    }

    check_limits(root, "$", 1, w);
    if (w.full()) return w.issues;

    if (!root.is_object()) {
        w.add("$", std::string("payload must be a JSON object, not ") + type_name(root));
        return w.issues;
    }

    check_object(root, "$", w);
    return w.issues;
}

void JobSchema::check_limits(const json::Value& v, const std::string& path, std::size_t depth, Walk& w) const {
    if (w.full()) return;

    if (depth > m_max_depth) {
        w.add(path, "nesting is deeper than " + std::to_string(m_max_depth) + " levels");
        return;
    }

    switch (v.type()) {
        case json::Type::String: {
            const std::string& s = v.as_string();
            if (!valid_utf8(s)) w.add(path, "is not valid UTF-8");
            else if (utf8_length(s) > m_max_string) w.add(path, "is longer than " + std::to_string(m_max_string) + " characters");
            break;
        }
        case json::Type::Array: {
            const auto& items = v.items();

            if (items.size() > m_max_array) {
                w.add(path, "has " + std::to_string(items.size()) + " items; the limit is " + std::to_string(m_max_array));
                return;
            }

            for (std::size_t i = 0; i < items.size() && !w.full(); ++i) check_limits(items[i], index_path(path, i), depth + 1, w);
            break;
        }
        case json::Type::Object: {
            const auto& members = v.members();

            if (members.size() > m_max_array) {
                w.add(path, "has " + std::to_string(members.size()) + " fields; the limit is " + std::to_string(m_max_array));
                return;
            }

            std::set<std::string> seen;

            for (const auto& m : members) {
                if (w.full()) return;
                const std::string at = child(path, m.first);

                if (!valid_utf8(m.first) || has_control(m.first, false)) { w.add(path, "has a field name that isn't clean text"); continue; }
                if (m.first.size() > kMaxKeyBytes) { w.add(path, "has a field name longer than " + std::to_string(kMaxKeyBytes) + " bytes"); continue; }
                if (!seen.insert(m.first).second) { w.add(at, "appears more than once"); continue; }
                check_limits(m.second, at, depth + 1, w);
            }

            break;
        }
        case json::Type::Number:
            if (!std::isfinite(v.as_number())) w.add(path, "is not a finite number");
            break;
        default:
            break;
    }
}

void JobSchema::check_object(const json::Value& obj, const std::string& path, Walk& w) const {
    for (const auto& f : m_fields) {
        if (w.full()) return;
        const json::Value* v = member(obj, f.first);
        const std::string at = child(path, f.first);

        if (!v) {
            if (f.second.m_required) w.add(at, "is required");
            continue;
        }

        check_field(f.second, *v, at, w);
    }

    if (!m_extra) {
        for (const auto& m : obj.members()) {
            if (w.full()) return;
            bool known = false;
            for (const auto& f : m_fields) if (f.first == m.first) { known = true; break; }
            if (!known) w.add(child(path, m.first), "is not an allowed field");
        }
    }

    for (const auto& g : m_groups) {
        std::size_t present = 0;

        for (const auto& name : g.names) {
            const json::Value* v = member(obj, name);
            if (v && !v->is_null()) ++present;
        }

        if (g.kind == GroupKind::AtLeastOne && present == 0) w.add(path, "needs at least one of " + join(g.names));
        if (g.kind == GroupKind::OnlyOne && present > 1) w.add(path, "can have only one of " + join(g.names));
        if (g.kind == GroupKind::AllOrNone && present != 0 && present != g.names.size()) w.add(path, "needs all of " + join(g.names) + ", or none of them");
    }

    for (const auto& fn : m_checks) {
        if (w.full()) return;

        try {
            const std::string msg = fn(obj);
            if (!msg.empty()) w.add(path, msg);
        } catch (const std::exception& e) {
            w.add(path, std::string("check failed: ") + e.what());
        } catch (...) {
            w.add(path, "check failed");
        }
    }
}

void JobSchema::check_field(const FieldRule& rule, const json::Value& v, const std::string& path, Walk& w) const {
    if (w.full()) return;

    if (v.is_null()) {
        if (rule.m_nullable || rule.m_type == FieldType::Null || rule.m_type == FieldType::Any) return;
        w.add(path, std::string("must be ") + wanted_name(rule.m_type) + ", not null");
        return;
    }

    bool ok = true;

    switch (rule.m_type) {
        case FieldType::Any:     break;
        case FieldType::Null:    ok = false; break;
        case FieldType::Boolean: ok = v.is_bool(); break;
        case FieldType::Number:  ok = v.is_number(); break;
        case FieldType::String:  ok = v.is_string(); break;
        case FieldType::Array:   ok = v.is_array(); break;
        case FieldType::Object:  ok = v.is_object(); break;
        case FieldType::Integer: {
            ok = v.is_number();

            if (ok) {
                const double d = v.as_number();

                if (std::trunc(d) != d) {
                    w.add(path, "must be a whole number, not " + number_text(d));
                    return;
                }

                if (std::fabs(d) > kMaxSafeInteger) {
                    w.add(path, "is too large to store exactly");
                    return;
                }
            }

            break;
        }
    }

    if (!ok) {
        w.add(path, std::string("must be ") + wanted_name(rule.m_type) + ", not " + type_name(v));
        return;
    }

    if (v.is_number()) {
        const double d = v.as_number();
        if (rule.m_min && d < *rule.m_min) w.add(path, "must be at least " + number_text(*rule.m_min));
        if (rule.m_max && d > *rule.m_max) w.add(path, "must be at most " + number_text(*rule.m_max));
        if (rule.m_min_exclusive && d <= *rule.m_min_exclusive) w.add(path, "must be more than " + number_text(*rule.m_min_exclusive));
    }

    if (v.is_string()) {
        const std::string& s = v.as_string();

        if (!rule.m_allow_control && has_control(s, true)) {
            w.add(path, "has control characters");
        } else {
            const std::size_t n = utf8_length(s);
            if (rule.m_min_len && n < *rule.m_min_len) w.add(path, *rule.m_min_len == 1 ? std::string("can't be empty") : "must be at least " + std::to_string(*rule.m_min_len) + " characters");
            if (rule.m_max_len && n > *rule.m_max_len) w.add(path, "must be at most " + std::to_string(*rule.m_max_len) + " characters");
            if (rule.m_format != StringFormat::None && !matches_format(s, rule.m_format)) w.add(path, std::string("must be a valid ") + to_string(rule.m_format));
        }
    }

    if (!rule.m_one_of.empty()) {
        const std::string mine = v.dump();
        bool found = false;
        for (const auto& c : rule.m_one_of) if (c.dump() == mine) { found = true; break; }

        if (!found) {
            std::vector<std::string> names;
            for (const auto& c : rule.m_one_of) names.push_back(c.dump());
            w.add(path, "must be one of " + join(names));
        }
    }

    if (v.is_array()) {
        const auto& items = v.items();
        if (rule.m_min_items && items.size() < *rule.m_min_items) w.add(path, *rule.m_min_items == 1 ? std::string("can't be empty") : "needs at least " + std::to_string(*rule.m_min_items) + " items");
        if (rule.m_max_items && items.size() > *rule.m_max_items) w.add(path, "can have at most " + std::to_string(*rule.m_max_items) + " items");

        if (rule.m_unique_items) {
            std::set<std::string> seen;

            for (std::size_t i = 0; i < items.size(); ++i) {
                if (!seen.insert(items[i].dump()).second) { w.add(index_path(path, i), "repeats an earlier item"); break; }
            }
        }

        if (rule.m_items) {
            for (std::size_t i = 0; i < items.size() && !w.full(); ++i) check_field(*rule.m_items, items[i], index_path(path, i), w);
        }
    }

    if (v.is_object() && rule.m_fields) rule.m_fields->check_object(v, path, w);

    for (const auto& fn : rule.m_checks) {
        if (w.full()) return;

        try {
            const std::string msg = fn(v);
            if (!msg.empty()) w.add(path, msg);
        } catch (const std::exception& e) {
            w.add(path, std::string("check failed: ") + e.what());
        } catch (...) {
            w.add(path, "check failed");
        }
    }
}

} // namespace guard
} // namespace networking
} // namespace fizmo
