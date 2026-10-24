#ifndef FIZMO_GUARD_JOB_SCHEMA_HPP
#define FIZMO_GUARD_JOB_SCHEMA_HPP

#include "../../Util Hpp/json.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct SchemaIssue {
    std::string path;
    std::string message;

    std::string to_string() const { return path.empty() ? message : path + ": " + message; }
};

std::string describe(const std::vector<SchemaIssue>& issues);

enum class FieldType : std::uint8_t {
    Any,
    Null,
    Boolean,
    Integer,
    Number,
    String,
    Array,
    Object
};

const char* to_string(FieldType type) noexcept;

enum class StringFormat : std::uint8_t {
    None,
    Identifier,
    Email,
    Uuid,
    Url,
    Hex,
    Base64,
    Digits,
    Date,
    DateTime
};

const char* to_string(StringFormat format) noexcept;

bool valid_utf8(const std::string& s) noexcept;
std::size_t utf8_length(const std::string& s) noexcept;
bool matches_format(const std::string& s, StringFormat format) noexcept;

class JobSchema;

class FieldRule {
public:
    using Check = std::function<std::string(const json::Value& value)>;

    FieldRule() = default;
    explicit FieldRule(FieldType type) : m_type(type) {}

    static FieldRule any();
    static FieldRule string();
    static FieldRule integer();
    static FieldRule number();
    static FieldRule boolean();
    static FieldRule array(FieldRule items);
    static FieldRule object(JobSchema fields);

    FieldRule& required(bool on = true) { m_required = on; return *this; }
    FieldRule& optional() { m_required = false; return *this; }
    FieldRule& nullable(bool on = true) { m_nullable = on; return *this; }
    FieldRule& min(double v) { m_min = v; return *this; }
    FieldRule& max(double v) { m_max = v; return *this; }
    FieldRule& between(double lo, double hi) { m_min = lo; m_max = hi; return *this; }
    FieldRule& positive() { m_min_exclusive = 0.0; return *this; }
    FieldRule& min_length(std::size_t n) { m_min_len = n; return *this; }
    FieldRule& max_length(std::size_t n) { m_max_len = n; return *this; }
    FieldRule& length(std::size_t lo, std::size_t hi) { m_min_len = lo; m_max_len = hi; return *this; }
    FieldRule& non_empty() { if (!m_min_len || *m_min_len < 1) m_min_len = 1; if (!m_min_items || *m_min_items < 1) m_min_items = 1; return *this; }
    FieldRule& min_items(std::size_t n) { m_min_items = n; return *this; }
    FieldRule& max_items(std::size_t n) { m_max_items = n; return *this; }
    FieldRule& unique_items(bool on = true) { m_unique_items = on; return *this; }
    FieldRule& one_of(std::vector<json::Value> values) { m_one_of = std::move(values); return *this; }
    FieldRule& one_of(std::initializer_list<const char*> values);
    FieldRule& format(StringFormat f) { m_format = f; return *this; }
    FieldRule& allow_control_chars(bool on = true) { m_allow_control = on; return *this; }
    FieldRule& check(Check fn) { m_checks.push_back(std::move(fn)); return *this; }

    FieldType type() const noexcept { return m_type; }
    bool is_required() const noexcept { return m_required; }

private:
    friend class JobSchema;

    FieldType                  m_type = FieldType::Any;
    bool                       m_required = true;
    bool                       m_nullable = false;
    std::optional<double>      m_min;
    std::optional<double>      m_max;
    std::optional<double>      m_min_exclusive;
    std::optional<std::size_t> m_min_len;
    std::optional<std::size_t> m_max_len;
    std::optional<std::size_t> m_min_items;
    std::optional<std::size_t> m_max_items;
    bool                       m_unique_items = false;
    std::vector<json::Value>   m_one_of;
    StringFormat               m_format = StringFormat::None;
    bool                       m_allow_control = false;
    std::shared_ptr<FieldRule> m_items;
    std::shared_ptr<JobSchema> m_fields;
    std::vector<Check>         m_checks;
};

class JobSchema {
public:
    using Check = std::function<std::string(const json::Value& payload)>;

    enum class Kind : std::uint8_t {
        Object,
        Text,
        Empty
    };

    JobSchema() = default;

    static JobSchema object();
    static JobSchema text(std::size_t max_bytes = 64 * 1024);
    static JobSchema empty();

    JobSchema& field(std::string name, FieldRule rule);
    JobSchema& allow_extra_fields(bool on = true) { m_extra = on; return *this; }
    JobSchema& max_bytes(std::size_t n) { m_max_bytes = n; return *this; }
    JobSchema& max_depth(std::size_t n) { m_max_depth = n; return *this; }
    JobSchema& max_string_length(std::size_t n) { m_max_string = n; return *this; }
    JobSchema& max_array_items(std::size_t n) { m_max_array = n; return *this; }
    JobSchema& at_least_one_of(std::vector<std::string> names);
    JobSchema& only_one_of(std::vector<std::string> names);
    JobSchema& all_or_none_of(std::vector<std::string> names);
    JobSchema& check(Check fn) { m_checks.push_back(std::move(fn)); return *this; }

    Kind kind() const noexcept { return m_kind; }
    std::size_t byte_limit() const noexcept { return m_max_bytes; }
    const std::vector<std::pair<std::string, FieldRule>>& fields() const noexcept { return m_fields; }

    std::vector<SchemaIssue> validate(const std::string& payload) const;
    std::vector<SchemaIssue> validate_value(const json::Value& payload) const;
    bool accepts(const std::string& payload) const { return validate(payload).empty(); }

private:
    friend class FieldRule;

    enum class GroupKind : std::uint8_t { AtLeastOne, OnlyOne, AllOrNone };

    struct Group {
        GroupKind                kind = GroupKind::AtLeastOne;
        std::vector<std::string> names;
    };

    struct Walk;

    Kind                                        m_kind = Kind::Object;
    std::vector<std::pair<std::string, FieldRule>> m_fields;
    bool                                        m_extra = false;
    std::size_t                                 m_max_bytes = 64 * 1024;
    std::size_t                                 m_max_depth = 16;
    std::size_t                                 m_max_string = 16 * 1024;
    std::size_t                                 m_max_array = 1000;
    std::vector<Group>                          m_groups;
    std::vector<Check>                          m_checks;

    void check_object(const json::Value& obj, const std::string& path, Walk& w) const;
    void check_field(const FieldRule& rule, const json::Value& v, const std::string& path, Walk& w) const;
    void check_limits(const json::Value& v, const std::string& path, std::size_t depth, Walk& w) const;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_JOB_SCHEMA_HPP
