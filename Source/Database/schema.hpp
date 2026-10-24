#ifndef FIZMO_DATABASE_SCHEMA_HPP
#define FIZMO_DATABASE_SCHEMA_HPP

#include "value.hpp"

#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace database {

enum class ColumnType : std::uint8_t {
    Boolean = 1,
    Integer = 2,
    Real    = 3,
    Text    = 4,
    Blob    = 5
};

const char* to_string(ColumnType type) noexcept;
ValueType value_type_of(ColumnType type) noexcept;

struct ColumnDef {
    std::string          name;
    ColumnType           type = ColumnType::Text;
    bool                 nullable = true;
    bool                 unique = false;
    bool                 primary_key = false;
    bool                 auto_increment = false;
    std::optional<Value> default_value;

    ColumnDef() = default;
    ColumnDef(std::string n, ColumnType t) : name(std::move(n)), type(t) {}

    ColumnDef& not_null() { nullable = false; return *this; }
    ColumnDef& unique_key() { unique = true; return *this; }
    ColumnDef& primary() { primary_key = true; nullable = false; return *this; }
    ColumnDef& autoincrement() { auto_increment = true; return *this; }
    ColumnDef& defaults(Value v) { default_value = std::move(v); return *this; }
};

ColumnDef integer(std::string name);
ColumnDef real(std::string name);
ColumnDef text(std::string name);
ColumnDef blob(std::string name);
ColumnDef boolean(std::string name);

enum class IndexKind : std::uint8_t {
    Hash    = 1,
    Ordered = 2
};

const char* to_string(IndexKind kind) noexcept;

struct IndexDef {
    std::string              name;
    std::vector<std::string> columns;
    IndexKind                kind = IndexKind::Ordered;
    bool                     unique = false;

    IndexDef() = default;
    IndexDef(std::string n, std::vector<std::string> cols, IndexKind k = IndexKind::Ordered, bool u = false)
        : name(std::move(n)), columns(std::move(cols)), kind(k), unique(u) {}
};

class Schema {
public:
    Schema() = default;
    Schema(std::initializer_list<ColumnDef> columns);
    explicit Schema(std::vector<ColumnDef> columns);

    Schema& add(ColumnDef column);
    std::size_t size() const noexcept { return m_columns.size(); }
    const ColumnDef& operator[](std::size_t i) const { return m_columns[i]; }
    const ColumnDef& column(std::size_t i) const { return m_columns.at(i); }
    const std::vector<ColumnDef>& columns() const noexcept { return m_columns; }
    std::optional<std::size_t> find(const std::string& name) const noexcept;
    std::size_t index_of(const std::string& name) const;
    std::vector<std::size_t> primary_key() const;
    void validate() const;

    Value coerce(std::size_t column, const Value& v) const;

private:
    std::vector<ColumnDef> m_columns;
};

Value coerce_to(ColumnType type, const Value& v, const std::string& column_name);
bool valid_identifier(const std::string& name) noexcept;

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_SCHEMA_HPP
