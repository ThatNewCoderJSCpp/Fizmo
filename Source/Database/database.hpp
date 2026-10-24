#ifndef FIZMO_DATABASE_DATABASE_HPP
#define FIZMO_DATABASE_DATABASE_HPP

#include "cache.hpp"
#include "graph.hpp"
#include "partition.hpp"
#include "query.hpp"
#include "storage.hpp"
#include "table.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace database {

struct DatabaseOptions {
    std::string   path;
    SyncMode      sync = SyncMode::Normal;
    std::size_t   cache_entries = 512;
    std::size_t   cache_rows = 200000;
    std::uint64_t checkpoint_bytes = 64ull << 20;
    bool          auto_checkpoint = true;
};

struct DatabaseStats {
    std::size_t   tables = 0;
    std::size_t   rows = 0;
    std::size_t   memory_bytes = 0;
    CacheStats    cache;
    std::uint64_t wal_bytes = 0;
    std::uint64_t queries = 0;
    std::uint64_t commits = 0;
    std::uint64_t rollbacks = 0;
    std::uint64_t checkpoints = 0;
    std::size_t   partitioned_tables = 0;
    std::size_t   partitions = 0;
};

class Database;

class Transaction {
public:
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&&) = delete;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    Table& create_table(const std::string& name, Schema schema);
    void create_partitioned_table(const std::string& name, Schema schema, PartitionOptions options);
    bool drop_table(const std::string& name);
    Index& create_index(const std::string& table, IndexDef def);
    bool drop_index(const std::string& table, const std::string& name);

    void set_partition_options(const std::string& table, PartitionOptions options);
    bool drop_partition(const std::string& table, const std::string& partition);
    std::size_t drop_partitions_before(const std::string& table, const Value& key);
    std::vector<PartitionInfo> enforce_retention(const std::string& table);

    RowId insert(const std::string& table, const NamedValues& values);
    RowId insert_row(const std::string& table, const Row& values);
    std::size_t insert_many(const std::string& table, const std::vector<Row>& rows);
    bool update_row(const std::string& table, RowId row, const NamedValues& changes);
    std::size_t update(const std::string& table, const Expr& where, const NamedValues& changes, const std::vector<Value>& params = {});
    bool remove_row(const std::string& table, RowId row);
    std::size_t remove(const std::string& table, const Expr& where, const std::vector<Value>& params = {});
    std::size_t clear(const std::string& table);

    ResultSet query(const Query& q, const std::vector<Value>& params = {}) const;
    const Table& table(const std::string& name) const;

    void commit();
    void rollback();
    bool active() const noexcept { return m_active; }

private:
    friend class Database;

    enum class UndoKind : std::uint8_t { Insert, Erase, Update, CreateTable, DropTable, CreateIndex, DropIndex, Clear, CreatePartitioned, DropPartitioned, AddPartition, RemovePartition, SetOptions, Newest };

    struct Undo {
        UndoKind                              kind = UndoKind::Insert;
        std::string                           table;
        std::string                           child;
        RowId                                 slot = kNoRow;
        Row                                   row;
        std::vector<std::pair<RowId, Row>>    rows;
        std::unique_ptr<Table>                dropped;
        std::unique_ptr<PartitionedTable>     parent;
        std::unique_ptr<PartitionPart>        part;
        std::shared_ptr<PartitionOptions>     options;
        Value                                 value;
        IndexDef                              def;
    };

    explicit Transaction(Database& db);

    Database*                           m_db = nullptr;
    std::unique_lock<std::shared_mutex> m_lock;
    std::vector<Undo>                   m_undo;
    std::vector<std::string>            m_wal;
    bool                                m_active = false;
    std::uint64_t                       m_id = 0;

    Table& writable(const std::string& name);
    void log(std::string record);
    void ensure_active() const;

    PartitionedTable& partitioned(const std::string& name);
    RowId insert_into(Table& t, const Row& prepared, const std::string& shown);
    bool update_in(Table& t, RowId row, const Assignments& changes, const std::string& shown);
    bool remove_in(Table& t, RowId row);
    RowId route(PartitionedTable& pt, const Row& prepared, const std::vector<bool>& generated, bool moving);
    std::size_t add_partition(PartitionedTable& pt, const Value& key);
    void remove_partition(PartitionedTable& pt, std::size_t index, DropReason reason, std::vector<PartitionInfo>* out, bool notify);
    std::vector<PartitionInfo> enforce(PartitionedTable& pt);
    void check_unique_across(PartitionedTable& pt, const Row& row, std::size_t skip, const std::vector<bool>& generated) const;
    std::vector<std::pair<Table*, std::vector<RowId>>> gather(PartitionedTable& pt, const Expr& where, const std::vector<Value>& params);
    std::size_t update_partitioned(PartitionedTable& pt, const Expr& where, const NamedValues& changes, const std::vector<Value>& params);
    std::size_t remove_partitioned(PartitionedTable& pt, const Expr& where, const std::vector<Value>& params);
    void observe(PartitionedTable& pt, const Row& row);
};

class Statement {
public:
    Statement() = default;
    ResultSet run(const std::vector<Value>& params = {}) const;
    const Query& query() const noexcept { return m_query; }

private:
    friend class Database;
    Statement(Database& db, Query q);

    Database*   m_db = nullptr;
    Query       m_query;
    std::string m_fingerprint;
};

class Database {
public:
    using QueryHook = std::function<void(const Query& query, const QueryStats& stats)>;

    explicit Database(DatabaseOptions options = DatabaseOptions());
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    static std::unique_ptr<Database> open(const std::string& path, SyncMode sync = SyncMode::Normal);

    Transaction begin();

    template <typename Fn>
    auto transaction(Fn&& fn) -> decltype(fn(std::declval<Transaction&>())) {
        Transaction tx = begin();

        if constexpr (std::is_void<decltype(fn(tx))>::value) {
            fn(tx);
            if (tx.active()) tx.commit();
        } else {
            decltype(auto) result = fn(tx);
            if (tx.active()) tx.commit();
            return result;
        }
    }

    Table& create_table(const std::string& name, Schema schema);
    void create_partitioned_table(const std::string& name, Schema schema, PartitionOptions options);
    bool drop_table(const std::string& name);
    void create_index(const std::string& table, IndexDef def);
    void create_index(const std::string& table, const std::vector<std::string>& columns, IndexKind kind = IndexKind::Ordered, bool unique = false, const std::string& name = {});
    bool drop_index(const std::string& table, const std::string& name);

    RowId insert(const std::string& table, const NamedValues& values);
    RowId insert_row(const std::string& table, const Row& values);
    std::size_t insert_many(const std::string& table, const std::vector<Row>& rows);
    std::size_t update(const std::string& table, const Expr& where, const NamedValues& changes, const std::vector<Value>& params = {});
    std::size_t remove(const std::string& table, const Expr& where, const std::vector<Value>& params = {});

    ResultSet query(const Query& q, const std::vector<Value>& params = {}) const;
    ResultSet select(const std::string& table, const Expr& where = Expr(), std::optional<std::size_t> limit = std::nullopt) const;
    std::optional<Row> get(const std::string& table, const std::vector<Value>& key) const;
    std::size_t count(const std::string& table, const Expr& where = Expr()) const;
    std::string explain(const Query& q, const std::vector<Value>& params = {}) const;
    Statement prepare(Query q);

    std::shared_ptr<const Graph> graph(const std::string& edge_table, const GraphOptions& options = GraphOptions()) const;

    bool is_partitioned(const std::string& name) const;
    std::vector<PartitionInfo> partitions(const std::string& table) const;
    PartitionOptions partition_options(const std::string& table) const;
    void set_partition_options(const std::string& table, PartitionOptions options);
    void on_partition_drop(const std::string& table, PartitionOptions::DropHook hook);
    void set_partition_clock(const std::string& table, PartitionOptions::Clock clock);
    bool drop_partition(const std::string& table, const std::string& partition);
    std::size_t drop_partitions_before(const std::string& table, const Value& key);
    std::vector<PartitionInfo> enforce_retention();
    std::vector<PartitionInfo> enforce_retention(const std::string& table);
    void start_retention(std::chrono::milliseconds every = std::chrono::milliseconds(60000));
    void stop_retention();
    bool retention_running() const;

    bool has_table(const std::string& name) const;
    std::vector<std::string> tables() const;
    Schema schema(const std::string& table) const;
    TableStats table_stats(const std::string& table) const;

    template <typename Fn>
    auto read(const std::string& table, Fn&& fn) const -> decltype(fn(std::declval<const Table&>())) {
        std::shared_lock<std::shared_mutex> lk(m_mutex);
        return fn(table_locked(table));
    }

    void checkpoint();
    void maintain();
    void set_cache(std::size_t entries, std::size_t rows);
    void clear_cache();
    void on_query(QueryHook hook);
    DatabaseStats stats() const;
    const DatabaseOptions& options() const noexcept { return m_options; }
    bool persistent() const noexcept { return !m_options.path.empty(); }

private:
    friend class Transaction;
    friend class Statement;

    struct CachedResult {
        std::shared_ptr<const ResultSet>                    result;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> versions;
    };

    struct CachedGraph {
        std::shared_ptr<const Graph> graph;
        std::uint64_t                uid = 0;
        std::uint64_t                version = 0;
    };

    DatabaseOptions                                  m_options;
    mutable std::shared_mutex                        m_mutex;
    std::map<std::string, std::unique_ptr<Table>>    m_tables;
    mutable LruCache<std::string, CachedResult>      m_cache;
    mutable std::mutex                               m_graph_mutex;
    mutable std::map<std::string, CachedGraph>       m_graphs;
    WriteAheadLog                                    m_wal;
    std::atomic<std::uint64_t>                       m_next_txn{1};
    mutable std::atomic<std::uint64_t>               m_queries{0};
    std::atomic<std::uint64_t>                       m_commits{0};
    std::atomic<std::uint64_t>                       m_rollbacks{0};
    std::atomic<std::uint64_t>                       m_checkpoints{0};
    mutable std::mutex                               m_hook_mutex;
    QueryHook                                        m_hook;
    std::map<std::string, std::unique_ptr<PartitionedTable>> m_partitioned;
    std::map<std::string, std::pair<PartitionedTable*, Table*>> m_children;
    std::thread                                      m_sweeper;
    std::mutex                                       m_sweep_mutex;
    std::condition_variable                          m_sweep_cv;
    bool                                             m_sweep_stop = false;

    const Table& table_locked(const std::string& name) const;
    Table* find_locked(const std::string& name) noexcept;
    const Table* find_locked(const std::string& name) const noexcept;
    Table* find_any_locked(const std::string& name) noexcept;
    PartitionedTable* find_partitioned_locked(const std::string& name) const noexcept;
    PartitionedTable& partitioned_locked(const std::string& name) const;
    bool name_taken(const std::string& name) const noexcept;
    void view_locked(const PartitionedTable& pt, PartitionView& out) const;
    void attach_children(PartitionedTable& pt);
    void detach_children(PartitionedTable& pt);
    void apply_index(const std::string& table, const IndexDef& def, bool create);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> signature(const Query& q) const;
    TableResolver resolver() const;
    PartitionResolver partition_resolver() const;
    ResultSet run_query(const Query& q, const std::vector<Value>& params, const std::string* fingerprint) const;
    void notify(const Query& q, const QueryStats& stats) const;

    std::string snapshot_path() const { return m_options.path + ".fzdb"; }
    std::string wal_path() const { return m_options.path + ".wal"; }
    void load();
    void load_snapshot(const std::string& bytes);
    void apply_wal(WalOp op, ByteReader& r);
    std::string build_snapshot() const;
    void checkpoint_locked();
};

std::string encode_create_table(const std::string& name, const Schema& schema);
std::string encode_create_partitioned(const std::string& name, const Schema& schema, const PartitionOptions& options);
std::string encode_index(WalOp op, const std::string& table, const IndexDef& def);

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_DATABASE_HPP
