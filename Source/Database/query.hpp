#ifndef FIZMO_DATABASE_QUERY_HPP
#define FIZMO_DATABASE_QUERY_HPP

#include "expr.hpp"
#include "table.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace database {

enum class JoinKind : std::uint8_t {
    Inner,
    Left
};

enum class SortOrder : std::uint8_t {
    Asc,
    Desc
};

enum class Aggregate : std::uint8_t {
    None,
    CountAll,
    Count,
    CountDistinct,
    Sum,
    Avg,
    Min,
    Max
};

const char* to_string(Aggregate agg) noexcept;

struct SelectItem {
    Expr        expr;
    std::string alias;
    Aggregate   aggregate = Aggregate::None;
};

struct OrderTerm {
    Expr      expr;
    SortOrder order = SortOrder::Asc;
};

struct JoinSpec {
    std::string table;
    std::string alias;
    Expr        left;
    Expr        right;
    JoinKind    kind = JoinKind::Inner;
};

class Query {
public:
    Query() = default;
    explicit Query(std::string table, std::string alias = {});

    Query& as(std::string alias);
    Query& where(Expr condition);
    Query& join(std::string table, Expr left_column, Expr right_column, JoinKind kind = JoinKind::Inner, std::string alias = {});
    Query& left_join(std::string table, Expr left_column, Expr right_column, std::string alias = {});
    Query& select(std::vector<Expr> columns);
    Query& select(Expr expr, std::string alias);
    Query& select(std::initializer_list<const char*> columns);
    Query& select_all();
    Query& count(std::string alias = "count");
    Query& count(const char* alias) { return count(std::string(alias)); }
    Query& count(Expr expr, std::string alias = {});
    Query& count_distinct(Expr expr, std::string alias = {});
    Query& sum(Expr expr, std::string alias = {});
    Query& avg(Expr expr, std::string alias = {});
    Query& min(Expr expr, std::string alias = {});
    Query& max(Expr expr, std::string alias = {});
    Query& group_by(Expr expr);
    Query& having(Expr condition);
    Query& order_by(Expr expr, SortOrder order = SortOrder::Asc);
    Query& order_by_desc(Expr expr) { return order_by(std::move(expr), SortOrder::Desc); }
    Query& limit(std::size_t n);
    Query& offset(std::size_t n);
    Query& distinct(bool on = true);
    Query& cache(bool on);

    const std::string& table() const noexcept { return m_table; }
    const std::string& alias() const noexcept { return m_alias; }
    const Expr& condition() const noexcept { return m_where; }
    const Expr& having_condition() const noexcept { return m_having; }
    const std::vector<JoinSpec>& joins() const noexcept { return m_joins; }
    const std::vector<SelectItem>& items() const noexcept { return m_select; }
    const std::vector<Expr>& groups() const noexcept { return m_group; }
    const std::vector<OrderTerm>& ordering() const noexcept { return m_order; }
    std::optional<std::size_t> limit_value() const noexcept { return m_limit; }
    std::size_t offset_value() const noexcept { return m_offset; }
    bool is_distinct() const noexcept { return m_distinct; }
    bool cacheable() const noexcept { return m_cache; }
    bool has_aggregates() const noexcept;

    void fingerprint(std::string& out) const;
    std::vector<std::string> referenced_tables() const;

private:
    std::string             m_table;
    std::string             m_alias;
    Expr                    m_where;
    Expr                    m_having;
    std::vector<JoinSpec>   m_joins;
    std::vector<SelectItem> m_select;
    std::vector<Expr>       m_group;
    std::vector<OrderTerm>  m_order;
    std::optional<std::size_t> m_limit;
    std::size_t             m_offset = 0;
    bool                    m_distinct = false;
    bool                    m_cache = true;
};

struct QueryStats {
    std::string               plan;
    std::size_t               rows_examined = 0;
    std::size_t               index_probes = 0;
    std::size_t               zones_skipped = 0;
    bool                      used_index = false;
    bool                      cache_hit = false;
    std::chrono::microseconds elapsed{0};
};

class ResultSet {
public:
    std::vector<std::string> columns;
    std::vector<Row>         rows;
    QueryStats               stats;

    std::size_t size() const noexcept { return rows.size(); }
    bool empty() const noexcept { return rows.empty(); }
    const Row& operator[](std::size_t i) const { return rows[i]; }
    std::vector<Row>::const_iterator begin() const noexcept { return rows.begin(); }
    std::vector<Row>::const_iterator end() const noexcept { return rows.end(); }

    std::optional<std::size_t> column_index(const std::string& name) const noexcept;
    const Value& at(std::size_t row, const std::string& column) const;
    Value scalar() const;
    std::string to_json() const;
    std::string to_text(std::size_t max_rows = 50) const;
};

using TableResolver = std::function<const Table*(const std::string& name)>;

struct PartitionView {
    struct Part {
        const Table* table = nullptr;
        Value        lo;
        Value        hi;
        Value        value;
    };

    const Table*      schema_table = nullptr;
    std::size_t       column = 0;
    bool              by_range = true;
    std::size_t       total = 0;
    std::vector<Part> parts;
};

using PartitionResolver = std::function<bool(const std::string& name, PartitionView& out)>;

struct ExecOptions {
    std::size_t max_rows = 0;
};

ResultSet execute_query(const Query& query, const TableResolver& tables, const std::vector<Value>& params = {}, const ExecOptions& options = ExecOptions());
ResultSet execute_query(const Query& query, const TableResolver& tables, const PartitionResolver& partitions, const std::vector<Value>& params = {}, const ExecOptions& options = ExecOptions());
std::vector<RowId> matching_rows(const Table& table, const Expr& condition, const std::vector<Value>& params = {}, QueryStats* stats = nullptr, std::size_t limit = 0);
std::string explain_query(const Query& query, const TableResolver& tables, const std::vector<Value>& params = {});
std::string explain_query(const Query& query, const TableResolver& tables, const PartitionResolver& partitions, const std::vector<Value>& params = {});
std::vector<std::size_t> prune_partitions(const PartitionView& view, const Expr& condition, const std::vector<Value>& params = {});

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_QUERY_HPP
