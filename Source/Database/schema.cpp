#include "fizmo_library.hpp"
#include "schema.hpp"

#include <cctype>
#include <cmath>
#include <set>

namespace fizmo {
namespace database {

const char* to_string(ColumnType type) noexcept {
    switch (type) {
        case ColumnType::Boolean: return "BOOLEAN";
        case ColumnType::Integer: return "INTEGER";
        case ColumnType::Real:    return "REAL";
        case ColumnType::Text:    return "TEXT";
        default:                  return "BLOB";
    }
}

ValueType value_type_of(ColumnType type) noexcept {
    return static_cast<ValueType>(static_cast<std::uint8_t>(type));
}

const char* to_string(IndexKind kind) noexcept {
    return kind == IndexKind::Hash ? "HASH" : "ORDERED";
}

ColumnDef integer(std::string name) { return ColumnDef(std::move(name), ColumnType::Integer); }
ColumnDef real(std::string name) { return ColumnDef(std::move(name), ColumnType::Real); }
ColumnDef text(std::string name) { return ColumnDef(std::move(name), ColumnType::Text); }
ColumnDef blob(std::string name) { return ColumnDef(std::move(name), ColumnType::Blob); }
ColumnDef boolean(std::string name) { return ColumnDef(std::move(name), ColumnType::Boolean); }

bool valid_identifier(const std::string& name) noexcept {
    if (name.empty() || name.size() > 128) return false;
    if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) return false;

    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }

    return true;
}

Value coerce_to(ColumnType type, const Value& v, const std::string& column_name) {
    if (v.is_null()) return v;

    auto fail = [&]() -> Value {
        throw TypeError("column '" + column_name + "' is " + to_string(type) + " but got " + to_string(v.type()) + " (" + v.to_string() + ")");
    };

    switch (type) {
        case ColumnType::Integer:
            if (v.is_int()) return v;
            if (v.is_bool()) return Value(v.get_bool() ? 1 : 0);
            if (v.is_real()) {
                const double d = v.get_real();
                if (std::trunc(d) == d && d >= -9223372036854775808.0 && d < 9223372036854775808.0) return Value(static_cast<std::int64_t>(d));
            }
            return fail();
        case ColumnType::Real:
            if (v.is_real()) return v;
            if (v.is_int()) return Value(static_cast<double>(v.get_int()));
            return fail();
        case ColumnType::Boolean:
            if (v.is_bool()) return v;
            if (v.is_int() && (v.get_int() == 0 || v.get_int() == 1)) return Value(v.get_int() == 1);
            return fail();
        case ColumnType::Text:
            if (v.is_text()) return v;
            return fail();
        default:
            if (v.is_blob()) return v;
            if (v.is_text()) return Value::blob(v.get_text());
            return fail();
    }
}

Schema::Schema(std::initializer_list<ColumnDef> columns) : m_columns(columns) { validate(); }
Schema::Schema(std::vector<ColumnDef> columns) : m_columns(std::move(columns)) { validate(); }

Schema& Schema::add(ColumnDef column) {
    m_columns.push_back(std::move(column));

    try {
        validate();
    } catch (...) {
        m_columns.pop_back();
        throw;
    }

    return *this;
}

std::optional<std::size_t> Schema::find(const std::string& name) const noexcept {
    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        if (m_columns[i].name == name) return i;
    }

    return std::nullopt;
}

std::size_t Schema::index_of(const std::string& name) const {
    const auto i = find(name);
    if (!i) throw DatabaseError("no column named '" + name + "'");
    return *i;
}

std::vector<std::size_t> Schema::primary_key() const {
    std::vector<std::size_t> out;

    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        if (m_columns[i].primary_key) out.push_back(i);
    }

    return out;
}

void Schema::validate() const {
    std::set<std::string> names;

    for (const auto& c : m_columns) {
        if (!valid_identifier(c.name)) throw DatabaseError("invalid column name '" + c.name + "'");
        if (!names.insert(c.name).second) throw DatabaseError("duplicate column '" + c.name + "'");
        if (c.auto_increment && c.type != ColumnType::Integer) throw DatabaseError("auto increment column '" + c.name + "' must be INTEGER");
        if (c.primary_key && c.nullable) throw DatabaseError("primary key column '" + c.name + "' cannot be nullable");
        if (c.default_value) coerce_to(c.type, *c.default_value, c.name);
    }
}

Value Schema::coerce(std::size_t column, const Value& v) const {
    const ColumnDef& c = m_columns.at(column);
    return coerce_to(c.type, v, c.name);
}

} // namespace database
} // namespace fizmo
