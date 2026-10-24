#ifndef FIZMO_DATABASE_VALUE_HPP
#define FIZMO_DATABASE_VALUE_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace fizmo {
namespace database {

class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConstraintError : public DatabaseError {
public:
    using DatabaseError::DatabaseError;
};

class TypeError : public DatabaseError {
public:
    using DatabaseError::DatabaseError;
};

enum class ValueType : std::uint8_t {
    Null    = 0,
    Boolean = 1,
    Integer = 2,
    Real    = 3,
    Text    = 4,
    Blob    = 5
};

const char* to_string(ValueType type) noexcept;

struct Blob {
    std::string bytes;

    Blob() = default;
    explicit Blob(std::string b) : bytes(std::move(b)) {}
    Blob(const std::uint8_t* data, std::size_t size) : bytes(reinterpret_cast<const char*>(data), size) {}
    explicit Blob(const std::vector<std::uint8_t>& v) : bytes(v.begin(), v.end()) {}

    bool operator==(const Blob& o) const noexcept { return bytes == o.bytes; }
    bool operator!=(const Blob& o) const noexcept { return bytes != o.bytes; }
};

class Value {
public:
    Value() noexcept = default;
    Value(std::nullptr_t) noexcept {}
    Value(bool v) : m_data(v) {}
    Value(double v) : m_data(v) {}
    Value(float v) : m_data(static_cast<double>(v)) {}
    Value(std::string v) : m_data(std::move(v)) {}
    Value(const char* v) : m_data(std::string(v ? v : "")) {}
    Value(Blob v) : m_data(std::move(v)) {}

    template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
    Value(T v) : m_data(static_cast<std::int64_t>(v)) {}

    static Value null() noexcept { return Value(); }
    static Value blob(std::string bytes) { return Value(Blob(std::move(bytes))); }

    ValueType type() const noexcept { return static_cast<ValueType>(m_data.index()); }
    bool is_null() const noexcept { return m_data.index() == 0; }
    bool is_bool() const noexcept { return m_data.index() == 1; }
    bool is_int() const noexcept { return m_data.index() == 2; }
    bool is_real() const noexcept { return m_data.index() == 3; }
    bool is_number() const noexcept { return m_data.index() == 2 || m_data.index() == 3; }
    bool is_text() const noexcept { return m_data.index() == 4; }
    bool is_blob() const noexcept { return m_data.index() == 5; }

    bool as_bool() const;
    std::int64_t as_int() const;
    double as_real() const;
    const std::string& as_text() const;
    const Blob& as_blob() const;

    bool get_bool() const noexcept { return std::get<1>(m_data); }
    std::int64_t get_int() const noexcept { return std::get<2>(m_data); }
    double get_real() const noexcept { return std::get<3>(m_data); }
    const std::string& get_text() const noexcept { return std::get<4>(m_data); }
    const Blob& get_blob() const noexcept { return std::get<5>(m_data); }

    double numeric() const noexcept;
    std::string to_string() const;
    std::string to_json() const;
    std::size_t hash() const noexcept;

    friend int compare(const Value& a, const Value& b) noexcept;
    friend bool operator==(const Value& a, const Value& b) noexcept { return compare(a, b) == 0; }
    friend bool operator!=(const Value& a, const Value& b) noexcept { return compare(a, b) != 0; }
    friend bool operator<(const Value& a, const Value& b) noexcept { return compare(a, b) < 0; }
    friend bool operator<=(const Value& a, const Value& b) noexcept { return compare(a, b) <= 0; }
    friend bool operator>(const Value& a, const Value& b) noexcept { return compare(a, b) > 0; }
    friend bool operator>=(const Value& a, const Value& b) noexcept { return compare(a, b) >= 0; }

private:
    std::variant<std::monostate, bool, std::int64_t, double, std::string, Blob> m_data;
};

int compare(const Value& a, const Value& b) noexcept;

struct ValueHash {
    std::size_t operator()(const Value& v) const noexcept { return v.hash(); }
};

struct ValueLess {
    bool operator()(const Value& a, const Value& b) const noexcept { return compare(a, b) < 0; }
};

using Row = std::vector<Value>;

void encode_key_part(std::string& out, const Value& v);
std::string encode_key(const Value& v);
std::string encode_key(const std::vector<Value>& parts);
std::string key_successor(std::string prefix);
std::string encode_text_prefix(const std::string& prefix);

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_VALUE_HPP
