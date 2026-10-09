#ifndef FIZMO_JSON_HPP
#define FIZMO_JSON_HPP

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace json {

enum class Type : std::uint8_t { Null = 0, Bool, Number, String, Array, Object };

class Value {
public:
    using Array  = std::vector<Value>;
    using Member = std::pair<std::string, Value>;
    using Object = std::vector<Member>;

private:
    Type                    m_type = Type::Null;
    bool                    m_bool = false;
    double                  m_number = 0.0;
    std::string             m_string;
    std::shared_ptr<Array>  m_array;
    std::shared_ptr<Object> m_object;

    static const Value& null_value() { static const Value v; return v; }

public:
    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : m_type(Type::Bool), m_bool(b) {}
    template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value>>
    Value(T n) : m_type(Type::Number), m_number(static_cast<double>(n)) {}
    Value(const char* s) : m_type(Type::String), m_string(s ? s : "") {}
    Value(std::string s) : m_type(Type::String), m_string(std::move(s)) {}
    Value(Array a) : m_type(Type::Array), m_array(std::make_shared<Array>(std::move(a))) {}
    Value(Object o) : m_type(Type::Object), m_object(std::make_shared<Object>(std::move(o))) {}

    static Value array() { return Value(Array{}); }
    static Value object() { return Value(Object{}); }

    Type type() const noexcept { return m_type; }
    bool is_null() const noexcept { return m_type == Type::Null; }
    bool is_bool() const noexcept { return m_type == Type::Bool; }
    bool is_number() const noexcept { return m_type == Type::Number; }
    bool is_string() const noexcept { return m_type == Type::String; }
    bool is_array() const noexcept { return m_type == Type::Array; }
    bool is_object() const noexcept { return m_type == Type::Object; }

    bool as_bool(bool fallback = false) const noexcept { return m_type == Type::Bool ? m_bool : fallback; }
    double as_number(double fallback = 0.0) const noexcept { return m_type == Type::Number ? m_number : fallback; }
    float as_float(float fallback = 0.0f) const noexcept { return m_type == Type::Number ? static_cast<float>(m_number) : fallback; }
    std::int64_t as_int(std::int64_t fallback = 0) const noexcept { return m_type == Type::Number ? static_cast<std::int64_t>(m_number) : fallback; }
    std::uint32_t as_uint(std::uint32_t fallback = 0) const noexcept { return m_type == Type::Number && m_number >= 0.0 ? static_cast<std::uint32_t>(m_number) : fallback; }
    const std::string& as_string() const noexcept { static const std::string none; return m_type == Type::String ? m_string : none; }
    std::string as_string(const std::string& fallback) const { return m_type == Type::String ? m_string : fallback; }

    std::size_t size() const noexcept {
        if (m_type == Type::Array) return m_array->size();
        if (m_type == Type::Object) return m_object->size();
        return 0;
    }

    template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>>
    const Value& operator[](I index) const noexcept {
        if (m_type != Type::Array || index < 0 || static_cast<std::size_t>(index) >= m_array->size()) return null_value();
        return (*m_array)[static_cast<std::size_t>(index)];
    }

    const Value& operator[](const std::string& key) const noexcept {
        if (m_type != Type::Object) return null_value();
        for (const Member& m : *m_object) if (m.first == key) return m.second;
        return null_value();
    }

    const Value& operator[](const char* key) const noexcept { return (*this)[std::string(key)]; }

    bool has(const std::string& key) const noexcept {
        if (m_type != Type::Object) return false;
        for (const Member& m : *m_object) if (m.first == key) return true;
        return false;
    }

    const Array& items() const noexcept { static const Array none; return m_type == Type::Array ? *m_array : none; }
    const Object& members() const noexcept { static const Object none; return m_type == Type::Object ? *m_object : none; }

    Value& push_back(Value v) {
        if (m_type != Type::Array) { *this = array(); }
        m_array->push_back(std::move(v));
        return m_array->back();
    }

    Value& set(const std::string& key, Value v) {
        if (m_type != Type::Object) { *this = object(); }
        for (Member& m : *m_object) if (m.first == key) { m.second = std::move(v); return m.second; }
        m_object->emplace_back(key, std::move(v));
        return m_object->back().second;
    }

    std::string dump(int indent = -1) const {
        std::string out;
        write(out, indent, 0);
        return out;
    }

private:
    static void write_string(std::string& out, const std::string& s) {
        out.push_back('"');
        for (unsigned char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                    else out.push_back(static_cast<char>(c));
            }
        }
        out.push_back('"');
    }

    void write(std::string& out, int indent, int depth) const {
        auto newline = [&](int d) { if (indent >= 0) { out.push_back('\n'); out.append(static_cast<std::size_t>(indent * d), ' '); } };

        switch (m_type) {
            case Type::Null:   out += "null"; break;
            case Type::Bool:   out += m_bool ? "true" : "false"; break;
            case Type::Number: {
                if (!std::isfinite(m_number)) { out += "null"; break; }
                char buf[32];
                if (m_number == std::floor(m_number) && std::abs(m_number) < 1e15) std::snprintf(buf, sizeof(buf), "%.0f", m_number);
                else std::snprintf(buf, sizeof(buf), "%.17g", m_number);
                out += buf;
                break;
            }
            case Type::String: write_string(out, m_string); break;
            case Type::Array:
                out.push_back('[');
                for (std::size_t i = 0; i < m_array->size(); ++i) {
                    if (i) out.push_back(',');
                    newline(depth + 1);
                    (*m_array)[i].write(out, indent, depth + 1);
                }
                if (!m_array->empty()) newline(depth);
                out.push_back(']');
                break;
            case Type::Object:
                out.push_back('{');
                for (std::size_t i = 0; i < m_object->size(); ++i) {
                    if (i) out.push_back(',');
                    newline(depth + 1);
                    write_string(out, (*m_object)[i].first);
                    out += indent >= 0 ? ": " : ":";
                    (*m_object)[i].second.write(out, indent, depth + 1);
                }
                if (!m_object->empty()) newline(depth);
                out.push_back('}');
                break;
        }
    }
};

class Parser {
private:
    const char* m_p;
    const char* m_end;
    const char* m_begin;
    std::string m_error;
    int         m_depth = 0;

    void fail(const char* what) {
        if (!m_error.empty()) return;
        std::size_t line = 1, col = 1;
        for (const char* q = m_begin; q < m_p && q < m_end; ++q) { if (*q == '\n') { ++line; col = 1; } else ++col; }
        m_error = std::string(what) + " at line " + std::to_string(line) + ", column " + std::to_string(col);
    }

    void skip() {
        while (m_p < m_end && (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r')) ++m_p;
    }

    bool literal(const char* word) {
        const std::size_t n = std::strlen(word);
        if (static_cast<std::size_t>(m_end - m_p) < n || std::strncmp(m_p, word, n) != 0) { fail("invalid literal"); return false; }
        m_p += n;
        return true;
    }

    static void utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    }

    bool hex4(std::uint32_t& out) {
        if (m_end - m_p < 4) { fail("truncated \\u escape"); return false; }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = *m_p++;
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<std::uint32_t>(c - 'A' + 10);
            else { fail("bad \\u escape"); return false; }
        }
        return true;
    }

    bool string(std::string& out) {
        ++m_p;
        while (m_p < m_end && *m_p != '"') {
            const char c = *m_p++;
            if (static_cast<unsigned char>(c) < 0x20) { fail("control character in string"); return false; }
            if (c != '\\') { out.push_back(c); continue; }
            if (m_p >= m_end) { fail("truncated escape"); return false; }
            const char e = *m_p++;
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && m_end - m_p >= 6 && m_p[0] == '\\' && m_p[1] == 'u') {
                        m_p += 2;
                        std::uint32_t lo = 0;
                        if (!hex4(lo)) return false;
                        if (lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else { utf8(out, 0xFFFD); cp = lo; }
                    }
                    if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
                    utf8(out, cp);
                    break;
                }
                default: fail("bad escape"); return false;
            }
        }
        if (m_p >= m_end) { fail("unterminated string"); return false; }
        ++m_p;
        return true;
    }

    bool number(Value& out) {
        const char* start = m_p;
        if (m_p < m_end && *m_p == '-') ++m_p;
        if (m_p >= m_end || !(*m_p >= '0' && *m_p <= '9')) { fail("invalid number"); return false; }
        while (m_p < m_end && ((*m_p >= '0' && *m_p <= '9') || *m_p == '.' || *m_p == 'e' || *m_p == 'E' || *m_p == '+' || *m_p == '-')) ++m_p;
        const std::string text(start, m_p);
        char* endp = nullptr;
        const double d = std::strtod(text.c_str(), &endp);
        if (!endp || *endp != '\0') { fail("invalid number"); return false; }
        out = Value(d);
        return true;
    }

    bool value(Value& out) {
        skip();
        if (m_p >= m_end) { fail("unexpected end of input"); return false; }
        if (++m_depth > 512) { fail("nesting too deep"); return false; }
        bool ok = true;

        switch (*m_p) {
            case 'n': ok = literal("null"); out = Value(); break;
            case 't': ok = literal("true"); out = Value(true); break;
            case 'f': ok = literal("false"); out = Value(false); break;
            case '"': { std::string s; ok = string(s); out = Value(std::move(s)); break; }
            case '[': {
                ++m_p;
                Value::Array arr;
                skip();
                if (m_p < m_end && *m_p == ']') { ++m_p; out = Value(std::move(arr)); break; }
                for (;;) {
                    Value v;
                    if (!value(v)) { ok = false; break; }
                    arr.push_back(std::move(v));
                    skip();
                    if (m_p < m_end && *m_p == ',') { ++m_p; continue; }
                    if (m_p < m_end && *m_p == ']') { ++m_p; break; }
                    fail("expected , or ]");
                    ok = false;
                    break;
                }
                if (ok) out = Value(std::move(arr));
                break;
            }
            case '{': {
                ++m_p;
                Value::Object obj;
                skip();
                if (m_p < m_end && *m_p == '}') { ++m_p; out = Value(std::move(obj)); break; }
                for (;;) {
                    skip();
                    if (m_p >= m_end || *m_p != '"') { fail("expected key"); ok = false; break; }
                    std::string key;
                    if (!string(key)) { ok = false; break; }
                    skip();
                    if (m_p >= m_end || *m_p != ':') { fail("expected :"); ok = false; break; }
                    ++m_p;
                    Value v;
                    if (!value(v)) { ok = false; break; }
                    obj.emplace_back(std::move(key), std::move(v));
                    skip();
                    if (m_p < m_end && *m_p == ',') { ++m_p; continue; }
                    if (m_p < m_end && *m_p == '}') { ++m_p; break; }
                    fail("expected , or }");
                    ok = false;
                    break;
                }
                if (ok) out = Value(std::move(obj));
                break;
            }
            default: ok = number(out); break;
        }

        --m_depth;
        return ok;
    }

public:
    Parser(const char* data, std::size_t size) : m_p(data), m_end(data + size), m_begin(data) {}

    bool parse(Value& out) {
        if (!value(out)) return false;
        skip();
        if (m_p != m_end) { fail("trailing characters"); return false; }
        return true;
    }

    const std::string& error() const noexcept { return m_error; }
};

inline bool parse(const std::string& text, Value& out, std::string* error = nullptr) {
    Parser p(text.data(), text.size());
    const bool ok = p.parse(out);
    if (!ok && error) *error = p.error();
    return ok;
}

inline bool parse(const char* data, std::size_t size, Value& out, std::string* error = nullptr) {
    Parser p(data, size);
    const bool ok = p.parse(out);
    if (!ok && error) *error = p.error();
    return ok;
}

inline Value parse_or_null(const std::string& text) {
    Value v;
    if (!parse(text, v)) return Value();
    return v;
}

} // namespace json
} // namespace fizmo

#endif // FIZMO_JSON_HPP
