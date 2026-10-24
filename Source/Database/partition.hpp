#ifndef FIZMO_DATABASE_PARTITION_HPP
#define FIZMO_DATABASE_PARTITION_HPP

#include "table.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <vector>

namespace fizmo {
namespace database {

class ByteWriter;
class ByteReader;

enum class PartitionKind : std::uint8_t {
    Range = 1,
    List  = 2
};

enum class TimeUnit : std::uint8_t {
    None         = 0,
    Seconds      = 1,
    Milliseconds = 2,
    Microseconds = 3
};

enum class RetentionAnchor : std::uint8_t {
    Auto   = 0,
    Clock  = 1,
    Newest = 2
};

enum class LateRowPolicy : std::uint8_t {
    Reject = 1,
    Ignore = 2
};

enum class DropReason : std::uint8_t {
    Age    = 1,
    Count  = 2,
    Rows   = 3,
    Bytes  = 4,
    Empty  = 5,
    Manual = 6
};

const char* to_string(PartitionKind kind) noexcept;
const char* to_string(TimeUnit unit) noexcept;
const char* to_string(DropReason reason) noexcept;

struct PartitionInfo {
    std::string  name;
    std::string  table;
    std::string  parent;
    Value        lo;
    Value        hi;
    Value        value;
    std::size_t  rows = 0;
    std::size_t  memory_bytes = 0;
    std::int64_t created_ms = 0;
    DropReason   reason = DropReason::Manual;
    bool         expired = false;
};

std::int64_t units_per_second(TimeUnit unit) noexcept;

template <typename Rep, typename Period>
std::int64_t to_time_units(std::chrono::duration<Rep, Period> d, TimeUnit unit) {
    switch (unit) {
        case TimeUnit::Milliseconds: return std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::milli>>(d).count();
        case TimeUnit::Microseconds: return std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::micro>>(d).count();
        case TimeUnit::Seconds:      return std::chrono::duration_cast<std::chrono::duration<std::int64_t>>(d).count();
        default:                     throw DatabaseError("a time span needs a partition with a time unit; use keep_for(Value) for plain numbers");
    }
}

struct PartitionOptions {
    using DropHook = std::function<void(const PartitionInfo& info, const Table& rows)>;
    using Clock    = std::function<std::int64_t()>;

    std::string          column;
    PartitionKind        kind = PartitionKind::Range;
    Value                width = Value(1);
    Value                origin = Value(0);
    std::uint32_t        months = 0;
    TimeUnit             unit = TimeUnit::None;
    std::optional<Value> retention;
    RetentionAnchor      anchor = RetentionAnchor::Auto;
    std::size_t          max_partitions = 0;
    std::size_t          max_rows = 0;
    std::size_t          max_bytes = 0;
    LateRowPolicy        late_rows = LateRowPolicy::Reject;
    bool                 drop_empty = false;
    DropHook             on_drop;
    Clock                clock;

    static PartitionOptions range(std::string column, Value width, Value origin = Value(0));
    static PartitionOptions list(std::string column);
    static PartitionOptions hourly(std::string column, TimeUnit unit = TimeUnit::Seconds);
    static PartitionOptions daily(std::string column, TimeUnit unit = TimeUnit::Seconds);
    static PartitionOptions weekly(std::string column, TimeUnit unit = TimeUnit::Seconds);
    static PartitionOptions monthly(std::string column, TimeUnit unit = TimeUnit::Seconds, std::uint32_t months = 1);
    static PartitionOptions yearly(std::string column, TimeUnit unit = TimeUnit::Seconds);

    template <typename Rep, typename Period>
    static PartitionOptions time(std::string column, std::chrono::duration<Rep, Period> width, TimeUnit unit = TimeUnit::Seconds) {
        PartitionOptions o = range(std::move(column), Value(to_time_units(width, unit)));
        o.unit = unit;
        return o;
    }

    PartitionOptions& keep_for(Value span);

    template <typename Rep, typename Period>
    PartitionOptions& keep_for(std::chrono::duration<Rep, Period> span) {
        return keep_for(Value(to_time_units(span, unit)));
    }

    PartitionOptions& keep_forever();
    PartitionOptions& keep_partitions(std::size_t n);
    PartitionOptions& keep_rows(std::size_t n);
    PartitionOptions& keep_bytes(std::size_t n);
    PartitionOptions& relative_to_newest(bool on = true);
    PartitionOptions& relative_to_clock();
    PartitionOptions& reject_late_rows();
    PartitionOptions& ignore_late_rows();
    PartitionOptions& drop_when_empty(bool on = true);
    PartitionOptions& when_dropped(DropHook hook);
    PartitionOptions& use_clock(Clock clock);
    PartitionOptions& starting_at(Value origin);

    bool uses_clock() const noexcept;
    bool has_limits() const noexcept;
};

void write_partition_options(ByteWriter& w, const PartitionOptions& o);
PartitionOptions read_partition_options(ByteReader& r);

std::int64_t wall_clock_ms() noexcept;

struct PartitionPart {
    std::unique_ptr<Table> table;
    std::uint64_t          seq = 0;
    std::string            label;
    std::string            key;
    Value                  lo;
    Value                  hi;
    Value                  value;
    std::int64_t           created_ms = 0;
};

class PartitionedTable {
public:
    PartitionedTable(std::string name, Schema schema, PartitionOptions options);

    PartitionedTable(const PartitionedTable&) = delete;
    PartitionedTable& operator=(const PartitionedTable&) = delete;

    const std::string& name() const noexcept { return m_name; }
    const Schema& schema() const noexcept { return m_template->schema(); }
    const PartitionOptions& options() const noexcept { return m_options; }
    void set_options(PartitionOptions options);
    std::size_t key_column() const noexcept { return m_key; }
    const std::string& key_name() const noexcept { return m_options.column; }
    bool by_range() const noexcept { return m_options.kind == PartitionKind::Range; }

    Table& templ() noexcept { return *m_template; }
    const Table& templ() const noexcept { return *m_template; }
    std::uint64_t uid() const noexcept { return m_uid; }
    std::uint64_t layout_version() const noexcept { return m_layout; }

    std::vector<PartitionPart>& parts() noexcept { return m_parts; }
    const std::vector<PartitionPart>& parts() const noexcept { return m_parts; }
    std::size_t size() const noexcept { return m_parts.size(); }

    std::string key_for(const Value& key) const;
    void bounds_for(const Value& key, Value& lo, Value& hi) const;
    std::string label_for(const Value& lo, const Value& hi, const Value& value) const;
    std::optional<std::size_t> find(const Value& key) const;
    std::optional<std::size_t> find_child(const std::string& table) const;
    std::optional<std::size_t> find_named(const std::string& name) const;
    PartitionPart make_part(const Value& key, std::uint64_t seq, std::string table) const;
    PartitionPart restore_part(std::string table, std::uint64_t seq, std::string label, Value lo, Value hi, Value value, std::int64_t created_ms, bool with_indexes = true) const;
    void set_drop_hook(PartitionOptions::DropHook hook) { m_options.on_drop = std::move(hook); }
    void set_clock(PartitionOptions::Clock clock) { m_options.clock = std::move(clock); ++m_layout; }
    std::size_t attach(PartitionPart part);
    PartitionPart detach(std::size_t index);
    std::string child_name(std::uint64_t seq) const { return m_name + "__p" + std::to_string(seq); }
    std::uint64_t next_seq() const noexcept { return m_next_seq; }
    void reserve_seq(std::uint64_t seq) noexcept { if (seq >= m_next_seq) m_next_seq = seq + 1; }

    std::int64_t now() const;
    std::optional<Value> cutoff(const Value* incoming = nullptr) const;
    bool expired(const PartitionPart& part, const std::optional<Value>& cutoff) const;
    bool is_late(const Value& key) const;
    std::vector<std::size_t> visible() const;

    void observe(const Row& row);
    const Value& newest() const noexcept { return m_newest; }
    void set_newest(Value v) { m_newest = std::move(v); }
    void recompute_newest();

    std::vector<IndexDef>& index_defs() noexcept { return m_indexes; }
    const std::vector<IndexDef>& index_defs() const noexcept { return m_indexes; }

    std::size_t rows() const noexcept;
    std::size_t memory_bytes() const noexcept;
    PartitionInfo info(const PartitionPart& part, DropReason reason = DropReason::Manual) const;
    TableStats stats() const;

private:
    std::string                      m_name;
    PartitionOptions                 m_options;
    std::unique_ptr<Table>           m_template;
    std::size_t                      m_key = 0;
    std::vector<PartitionPart>       m_parts;
    std::map<std::string, std::size_t> m_lookup;
    std::vector<IndexDef>            m_indexes;
    Value                            m_newest;
    std::uint64_t                    m_uid = 0;
    std::uint64_t                    m_layout = 1;
    std::uint64_t                    m_next_seq = 1;

    void validate_options(PartitionOptions& o);
    void rebuild_lookup();
};

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_PARTITION_HPP
