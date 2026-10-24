#ifndef FIZMO_GUARD_QUERY_HPP
#define FIZMO_GUARD_QUERY_HPP

#include "guard_types.hpp"

#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

using SqlNull  = std::nullptr_t;
using SqlBlob  = std::vector<std::uint8_t>;
using SqlValue = std::variant<SqlNull, bool, std::int64_t, double, std::string, SqlBlob>;

class QueryError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

enum class StatementMode {
    Strict,
    AllowLiterals
};

class Statement {
public:
    Statement() = default;
    explicit Statement(std::string sql, StatementMode mode = StatementMode::Strict);
    Statement(std::string sql, std::vector<SqlValue> params, StatementMode mode = StatementMode::Strict);

    Statement& bind(SqlValue value);
    Statement& bind_null() { return bind(SqlValue(nullptr)); }
    Statement& bind_all(const std::vector<SqlValue>& values);

    template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
    Statement& bind(T value) { return bind(SqlValue(static_cast<std::int64_t>(value))); }

    template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    Statement& bind(T value) { return bind(SqlValue(static_cast<double>(value))); }

    Statement& bind(bool value) { return bind(SqlValue(value)); }
    Statement& bind(const char* value) { return value ? bind(SqlValue(std::string(value))) : bind_null(); }
    Statement& bind(const std::string& value) { return bind(SqlValue(value)); }

    Statement& append(const std::string& fragment, const std::vector<SqlValue>& values = {});
    Statement& append_in(const std::string& column, const std::vector<SqlValue>& values);
    Statement& append_identifier(const std::string& name);

    const std::string& sql() const noexcept { return m_sql; }
    const std::vector<SqlValue>& params() const noexcept { return m_params; }
    std::size_t placeholder_count() const noexcept { return m_placeholders; }
    bool complete() const noexcept { return m_params.size() == m_placeholders; }
    void require_complete() const;
    StatementMode mode() const noexcept { return m_mode; }

    std::string shape() const;
    std::string debug_string(bool redact = true) const;

    static std::string placeholders(std::size_t count);
    static std::string identifier(const std::string& name);
    static std::size_t count_placeholders(const std::string& sql, StatementMode mode);

private:
    std::string           m_sql;
    std::vector<SqlValue> m_params;
    std::size_t           m_placeholders = 0;
    StatementMode         m_mode = StatementMode::Strict;
};

std::string normalize_query_shape(const std::string& sql);

enum class QueryWarningKind {
    RepeatedShape,
    Slow,
    TooMany
};

const char* to_string(QueryWarningKind kind) noexcept;

struct QueryWarning {
    QueryWarningKind          kind = QueryWarningKind::RepeatedShape;
    std::string               shape;
    std::size_t               count = 0;
    std::chrono::microseconds total{0};
    std::string               message;
};

struct QueryRecorderPolicy {
    std::size_t               repeat_threshold = 5;
    std::chrono::microseconds slow_query{200000};
    std::size_t               max_queries = 100;
};

class QueryRecorder {
public:
    explicit QueryRecorder(QueryRecorderPolicy policy = QueryRecorderPolicy());

    void record(const Statement& statement, std::chrono::microseconds duration = std::chrono::microseconds(0));
    void record_shape(const std::string& sql, std::chrono::microseconds duration = std::chrono::microseconds(0));
    std::vector<QueryWarning> warnings() const;
    std::size_t total() const;
    std::size_t count(const std::string& shape) const;
    std::chrono::microseconds total_time() const;
    void clear();

    const QueryRecorderPolicy& policy() const noexcept { return m_policy; }

private:
    struct ShapeStats {
        std::size_t               count = 0;
        std::chrono::microseconds total{0};
        std::chrono::microseconds slowest{0};
    };

    QueryRecorderPolicy                         m_policy;
    mutable std::mutex                          m_mutex;
    std::unordered_map<std::string, ShapeStats> m_shapes;
    std::vector<std::string>                    m_order;
    std::size_t                                 m_total = 0;
    std::chrono::microseconds                   m_time{0};
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_QUERY_HPP
