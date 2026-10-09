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
    std::uint32_t as_uint(std::uint32_t fallback = 0) const noexcept;
    const std::string& as_string() const noexcept { static const std::string none; return m_type == Type::String ? m_string : none; }
    std::string as_string(const std::string& fallback) const { return m_type == Type::String ? m_string : fallback; }

    std::size_t size() const noexcept;

    template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>>
    const Value& operator[](I index) const noexcept {
        if (m_type != Type::Array || index < 0 || static_cast<std::size_t>(index) >= m_array->size()) return null_value();
        return (*m_array)[static_cast<std::size_t>(index)];
    }

    const Value& operator[](const std::string& key) const noexcept;

    const Value& operator[](const char* key) const noexcept { return (*this)[std::string(key)]; }

    bool has(const std::string& key) const noexcept;

    const Array& items() const noexcept { static const Array none; return m_type == Type::Array ? *m_array : none; }
    const Object& members() const noexcept { static const Object none; return m_type == Type::Object ? *m_object : none; }

    Value& push_back(Value v);

    Value& set(const std::string& key, Value v);

    std::string dump(int indent = -1) const {
        std::string out;
        write(out, indent, 0);
        return out;
    }

private:
    static void write_string(std::string& out, const std::string& s);

    void write(std::string& out, int indent, int depth) const;
};

class Parser {
private:
    const char* m_p;
    const char* m_end;
    const char* m_begin;
    std::string m_error;
    int         m_depth = 0;

    void fail(const char* what);

    void skip() {
        while (m_p < m_end && (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r')) ++m_p;
    }

    bool literal(const char* word);

    static void utf8(std::string& out, std::uint32_t cp);

    bool hex4(std::uint32_t& out);

    bool string(std::string& out);

    bool number(Value& out);

    bool value(Value& out);

public:
    Parser(const char* data, std::size_t size) : m_p(data), m_end(data + size), m_begin(data) {}

    bool parse(Value& out);

    const std::string& error() const noexcept { return m_error; }
};

 bool parse(const std::string& text, Value& out, std::string* error = nullptr);

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
