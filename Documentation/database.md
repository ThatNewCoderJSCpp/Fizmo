# Database

`#include <database.hpp>` (or `FIZMO_DATABASE` with `fizmo/includes.hpp`) gives an embedded database: typed tables, indexes, a query planner, transactions, a write-ahead log and graph searches. Everything is in `fizmo::database`, and it links as `fizmo::db`.

```cpp
using namespace fizmo::database;

auto db = Database::open("data/recipes");          // or Database db; for memory only
db->create_table("recipes", Schema{
    integer("id").primary().autoincrement(),
    text("title").not_null(),
    integer("owner").not_null(),
    real("rating"),
    integer("minutes").defaults(Value(30)),
});
db->create_index("recipes", { "owner" }, IndexKind::Hash);
db->create_index("recipes", { "rating" });         // ordered by default

db->insert("recipes", { { "title", "Soup" }, { "owner", 1 }, { "rating", 4.5 } });

ResultSet top = db->query(Query("recipes")
    .select({ "title", "rating" })
    .where(col("owner") == 1 && col("rating") >= 4.0)
    .order_by_desc(col("rating"))
    .limit(10));

top.at(0, "title");      // Value("Soup")
top.stats.plan;          // "INDEX RANGE ix_recipes_rating | ..."
```

## Values and columns

A `Value` is NULL, `bool`, `int64`, `double`, text (`std::string`) or a `Blob`. Columns are `integer`, `real`, `text`, `blob` or `boolean`; a value that doesn't fit its column is a `TypeError` (`3.0` goes into an integer column, `3.5` doesn't). NULL sorts first; integers and reals compare by value, so `Value(3) == Value(3.0)`.

| Column option | Does |
|---|---|
| `.primary()` | primary key (implies `not_null`); gets a unique hash index |
| `.autoincrement()` | an integer that counts up when left out |
| `.not_null()` | rejects NULL (`ConstraintError`) |
| `.unique_key()` | rejects duplicates; NULLs don't count |
| `.defaults(v)` | used when the column is left out |

## Queries

Conditions are built from `col("name")`, values and `param(i)`: `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`, `+ - * / %`, `.in({...})`, `.not_in`, `.between(a, b)`, `.is_null()`, `.is_not_null()`, `.like("ab%")`, `.ilike(...)`, `.starts_with(...)`, `.lower()`, `.upper()`, `.length()`, `.abs()`, `.coalesce(v)`. Comparisons with NULL are unknown, as in SQL.

`Query` also has `join(table, left, right)`, `left_join`, `count()`, `sum`, `avg`, `min`, `max`, `count_distinct`, `group_by`, `having`, `distinct`, `offset` and `cache(false)`. `db.update(table, where, changes)` and `db.remove(table, where)` use the same planner to find their rows. Values are always bound, never pasted into text, so there is nothing to inject.

## What makes it fast

| Technique | Where it helps |
|---|---|
| Columnar storage with typed arrays and null bitmaps | scans read one column as plain `int64`/`double` arrays |
| Bitmask filtering, 64 rows per step | simple conditions run without building `Value`s |
| Zone maps (min/max per 1024 rows) | range scans skip blocks that can't match |
| Hash indexes (robin-hood `collections::HashMap`) | `=` and `IN` in O(1) |
| Ordered indexes (`collections::BTreeSet`, B-tree node splitting) | ranges, prefixes (`LIKE 'ab%'`), `ORDER BY` and `MIN`/`MAX` |
| Composite indexes with order-preserving key encoding | `a = ? AND b > ?` in one lookup; keys compare with `memcmp` |
| Exact cost estimates (B-tree rank counting) | the planner picks the cheapest index; `COUNT` over a range never touches rows |
| Index union | `a = 1 OR b = 2` merges two lookups instead of scanning |
| Early stop and top-N heaps | `ORDER BY ... LIMIT n` walks an index and stops, or keeps only `n` rows |
| Hash joins and index joins | joins build or probe a hash table instead of nested scans |
| Result cache (LRU, versioned) | a repeated query is a lookup; any write to its tables invalidates it |
| Bulk index builds | `create_index` sorts once and builds the tree bottom-up |
| CSR graph cache | `graph()` builds adjacency arrays once per table version |

`db.explain(query)` and `ResultSet::stats` show the plan. With 200,000 rows on a small 2-core machine, a hash lookup took 8 µs against 270 µs for a scan, a range `COUNT` 2 µs against 250 µs, and `ORDER BY price LIMIT 10` 6 µs from the index against 3.8 ms without one.

## Transactions

```cpp
db->transaction([&](Transaction& tx) {
    tx.update("accounts", col("id") == 1, { { "balance", 40 } });
    tx.update("accounts", col("id") == 2, { { "balance", 110 } });
});   // commits; an exception rolls everything back
```

`db.begin()` gives a `Transaction` to commit or roll back by hand; one that is destroyed while open rolls back. Writers are serialized and readers run in parallel (`std::shared_mutex`). Rollback undoes inserts, updates, deletes, `clear`, and created or dropped tables and indexes.

## Files

`Database::open(path)` keeps two files: `path.fzdb`, a checksummed snapshot, and `path.wal`, a write-ahead log of committed transactions (CRC32 per record). Opening replays the log on top of the snapshot and cuts off a torn last record, so a crash loses at most the transaction that was being written. `checkpoint()` writes a new snapshot and empties the log; it also happens on its own once the log passes `checkpoint_bytes` (64 MB). `SyncMode::Full` calls `fsync` on every commit, `Normal` (the default) flushes, `Off` leaves it to the OS.

## Graphs

```cpp
GraphOptions o;
o.from = "src";
o.to = "dst";
o.weight = "cost";        // optional: Dijkstra instead of BFS
auto g = db->graph("edges", o);

g->bfs("a");                  // visit order with depths and parents
g->dfs("a", 3);               // at most 3 steps deep
g->shortest_path("a", "z");   // bidirectional BFS, or Dijkstra with weights
g->components();              // union-find
g->topological_order();       // empty optional when there is a cycle
```

## Partitions

A partitioned table keeps its rows in separate parts by one column, usually a time. Queries look only at the parts their condition can match, and old data is removed by dropping whole parts instead of deleting rows one by one.

```cpp
db->create_partitioned_table("events", Schema{
    integer("id").primary().autoincrement(),
    integer("ts").not_null(),
    text("kind"),
}, PartitionOptions::daily("ts")                     // one part per UTC day, ts in seconds
       .keep_for(std::chrono::hours(24 * 30))         // how far back to keep
       .keep_partitions(40)                           // and never more than 40 parts
       .when_dropped([](const PartitionInfo& p, const Table& rows) { archive(p, rows); }));

db->create_index("events", { "ts" });                 // every part gets the index
db->insert("events", { { "ts", now }, { "kind", "click" } });
db->query(Query("events").where(col("ts") >= now - 3600).count());
// "PARTITIONS 1 of 30 | COUNT from INDEX RANGE ix_events_ts"
db->start_retention(std::chrono::minutes(1));         // drop expired parts in the background
```

### How far back each part goes

| Builder | Parts |
|---|---|
| `hourly(col, unit)`, `daily`, `weekly` (weeks start on Monday) | fixed spans of time |
| `monthly(col, unit, months = 1)`, `yearly` | calendar months, so they follow month lengths |
| `time(col, duration, unit)` | any span, such as `std::chrono::minutes(15)` |
| `range(col, width, origin = 0)` | plain numbers in steps of `width` (INTEGER or REAL columns) |
| `list(col)` | one part per distinct value, such as a region or a customer |

`unit` says what the column holds: `TimeUnit::Seconds` (the default), `Milliseconds` or `Microseconds`. Time parts are named by when they start, in UTC: `2026-10-24`, `2026-10-24T13`, `2026-10`, `2026`. A part is made the first time a row lands in it. The partition column can't be NULL.

### When parts are removed

| Option | Removes |
|---|---|
| `keep_for(span)` | parts that end before now minus `span`. Whole parts go, so a row lives between `span` and `span` plus one part's width |
| `relative_to_newest()` | measures `keep_for` back from the newest row instead of the clock; the default for plain numbers |
| `keep_partitions(n)` | the oldest parts beyond `n` |
| `keep_rows(n)`, `keep_bytes(n)` | the oldest parts while the table is over the limit (the newest part always stays) |
| `drop_when_empty()` | parts left empty by deletes |
| `reject_late_rows()` (the default), `ignore_late_rows()` | what happens to a row older than `keep_for`: it throws `ConstraintError`, or `insert` returns `kNoRow` |

Limits are applied when a new part starts, when the options change, on `enforce_retention()`, and by the background sweep from `start_retention(every)`. Parts that have passed `keep_for` stop showing up in queries right away, even before they're dropped. `when_dropped` runs before each part goes, with its rows, so they can be archived; it runs while the database is locked, so it must not call back into the same database.

`partitions(table)` lists the parts (name, bounds, rows, bytes, `expired`). `drop_partition(table, name)` and `drop_partitions_before(table, key)` remove parts by hand, and `set_partition_options` changes the limits later (not the column or the width). `use_clock` and `when_dropped` aren't saved to disk; set them again after opening with `set_partition_clock` and `on_partition_drop`.

### What stays the same

Rows keep their auto-increment ids across parts, and primary keys and unique indexes are checked across all parts. `update` moves a row to another part when its partition column changes. Joins, aggregates, `ORDER BY`, transactions, the result cache and the write-ahead log all work as on a plain table. Rows are addressed by condition rather than `RowId`, so `update_row`, `remove_row` and `read` are for plain tables only.

### Speed

| Technique | Where it helps |
|---|---|
| Partition pruning | conditions on the partition column skip parts that can't match |
| Covered parts | a part that lies wholly inside the condition skips that check, so a `COUNT` over it is just its size |
| Ordered walk | `ORDER BY` the partition column with `LIMIT` reads parts in order and stops early |
| Index counts | `COUNT` over a range reads B-tree counts in each part |
| Dropping parts | removing a day of data frees one table instead of deleting its rows |

With 300,000 rows over 30 daily parts, counting a week with an extra filter took 0.27 ms against 5.4 ms on a plain table with a `ts` index. Removing the oldest two days took 1.7 ms against 44 ms for a `DELETE`.

## Watching queries

`db.on_query(fn)` calls `fn(query, stats)` after every query, so `networking::guard::QueryRecorder` or a logger can flag N+1 patterns and slow queries.
