#ifndef FIZMO_DATABASE_TABLE_HPP
#define FIZMO_DATABASE_TABLE_HPP

#include "column_store.hpp"
#include "index.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace database {

using NamedValues = std::vector<std::pair<std::string, Value>>;
using Assignments = std::vector<std::pair<std::size_t, Value>>;

struct TableStats {
    std::size_t rows = 0;
    std::size_t slots = 0;
    std::size_t free_slots = 0;
    std::size_t indexes = 0;
    std::size_t memory_bytes = 0;
    std::uint64_t version = 0;
};

class Table {
public:
    Table(std::string name, Schema schema);

    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    const std::string& name() const noexcept { return m_name; }
    const Schema& schema() const noexcept { return m_schema; }
    std::size_t size() const noexcept { return m_live_count; }
    std::size_t slot_count() const noexcept { return m_slot_count; }
    bool live(RowId slot) const noexcept { return slot < m_slot_count && m_live.test(slot); }
    const Bitmap& live_rows() const noexcept { return m_live; }
    std::uint64_t version() const noexcept { return m_version; }
    std::uint64_t uid() const noexcept { return m_uid; }
    std::uint64_t schema_version() const noexcept { return m_schema_version; }

    RowId insert(const Row& values);
    RowId insert(const NamedValues& values);
    RowId insert_at(RowId slot, const Row& values);
    bool erase(RowId slot);
    bool update(RowId slot, const Assignments& changes);
    bool update(RowId slot, const NamedValues& changes);

    Row row(RowId slot) const;
    Value get(RowId slot, std::size_t column) const;
    Value get(RowId slot, const std::string& column) const { return get(slot, m_schema.index_of(column)); }
    const ColumnData& column(std::size_t i) const { return m_columns.at(i); }
    std::size_t column_index(const std::string& name) const { return m_schema.index_of(name); }

    Row prepare_row(const Row& values);
    Row prepare_row(const NamedValues& values);
    void check_unique(const Row& prepared, RowId except) const;

    Index& create_index(IndexDef def);
    bool drop_index(const std::string& name);
    Index* index(const std::string& name) noexcept;
    const Index* index(const std::string& name) const noexcept;
    const std::vector<std::unique_ptr<Index>>& indexes() const noexcept { return m_indexes; }
    const Index* best_index_for(std::size_t column, bool need_order) const noexcept;
    const Index* primary_index() const noexcept { return m_primary; }

    RowId find_by_key(const std::vector<Value>& key) const;
    std::string index_key(const Index& index, RowId slot, bool* has_null = nullptr) const;
    std::string index_key(const Index& index, const Row& row, bool* has_null = nullptr) const;

    template <typename Fn>
    void for_each(Fn&& fn) const { m_live.for_each(std::forward<Fn>(fn)); }

    std::int64_t next_auto_value(std::size_t column) const;
    void set_auto_value(std::size_t column, std::int64_t next);
    const std::map<std::size_t, std::int64_t>& auto_values() const noexcept { return m_auto; }

    void maintain();
    void clear();
    TableStats stats() const;
    void bump_version() noexcept { ++m_version; }

private:
    std::string                         m_name;
    Schema                              m_schema;
    std::vector<ColumnData>             m_columns;
    Bitmap                              m_live;
    std::vector<RowId>                  m_free;
    std::size_t                         m_slot_count = 0;
    std::size_t                         m_capacity = 0;
    std::size_t                         m_live_count = 0;
    std::vector<std::unique_ptr<Index>> m_indexes;
    const Index*                        m_primary = nullptr;
    std::map<std::size_t, std::int64_t> m_auto;
    std::uint64_t                       m_version = 1;
    std::uint64_t                       m_schema_version = 1;
    std::uint64_t                       m_uid = 0;

    RowId allocate();
    void ensure_slots(std::size_t n);
    void write_row(RowId slot, const Row& prepared);
    void add_to_indexes(RowId slot);
    void remove_from_indexes(RowId slot);
    std::vector<std::size_t> resolve_columns(const std::vector<std::string>& names) const;
};

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_TABLE_HPP
