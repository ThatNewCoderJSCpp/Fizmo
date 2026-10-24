#include "fizmo_library.hpp"
#include "database.hpp"
#include "../Crypto/hash.hpp"

#include <algorithm>
#include <filesystem>

namespace fizmo {
namespace database {

namespace {

constexpr const char kSnapshotMagicV1[8] = { 'F', 'Z', 'D', 'B', '0', '0', '0', '1' };
constexpr const char kSnapshotMagic[8] = { 'F', 'Z', 'D', 'B', '0', '0', '0', '2' };

std::string table_record(WalOp op, const std::string& table) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(op));
    w.str(table);
    return std::move(w.data());
}

std::string txn_record(WalOp op, std::uint64_t id) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(op));
    w.u64(id);
    return std::move(w.data());
}

std::string insert_record(const std::string& table, RowId slot, const Row& row) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::Insert));
    w.str(table);
    w.u32(slot);
    w.row(row);
    return std::move(w.data());
}

std::string delete_record(const std::string& table, RowId slot) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::Delete));
    w.str(table);
    w.u32(slot);
    return std::move(w.data());
}

std::string update_record(const std::string& table, RowId slot, const Assignments& changes) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::Update));
    w.str(table);
    w.u32(slot);
    w.u32(static_cast<std::uint32_t>(changes.size()));

    for (const auto& c : changes) {
        w.u32(static_cast<std::uint32_t>(c.first));
        w.value(c.second);
    }

    return std::move(w.data());
}

std::string add_partition_record(const std::string& parent, const PartitionPart& p) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::AddPartition));
    w.str(parent);
    w.str(p.table->name());
    w.u64(p.seq);
    w.str(p.label);
    w.value(p.lo);
    w.value(p.hi);
    w.value(p.value);
    w.i64(p.created_ms);
    return std::move(w.data());
}

std::string remove_partition_record(const std::string& parent, const std::string& child) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::RemovePartition));
    w.str(parent);
    w.str(child);
    return std::move(w.data());
}

std::string options_record(const std::string& parent, const PartitionOptions& o) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::SetPartitionOptions));
    w.str(parent);
    write_partition_options(w, o);
    return std::move(w.data());
}

bool auto_index(const Table& t, const std::string& name) {
    return name == "pk_" + t.name() || name.rfind("uq_" + t.name() + "_", 0) == 0;
}

void write_rows(ByteWriter& w, const Table& t) {
    w.u32(static_cast<std::uint32_t>(t.slot_count()));
    w.u32(static_cast<std::uint32_t>(t.size()));
    t.for_each([&](RowId r) {
        w.u32(r);
        w.row(t.row(r));
    });
}

void read_rows(ByteReader& r, Table& t) {
    const std::uint32_t slots = r.u32();
    const std::uint32_t rows = r.u32();
    (void)slots;

    for (std::uint32_t k = 0; k < rows; ++k) {
        const RowId slot = r.u32();
        t.insert_at(slot, r.row());
    }
}

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty() || from == to) return;
    std::size_t at = 0;

    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
}

const Index* twin_index(const Table& t, const Index& model) noexcept {
    for (const auto& i : t.indexes()) {
        if (i->unique() == model.unique() && i->kind() == model.kind() && i->columns() == model.columns()) return i.get();
    }

    return nullptr;
}

std::vector<bool> generated_from(const Schema& schema, const NamedValues& values) {
    std::vector<bool> out(schema.size(), false);

    for (std::size_t i = 0; i < schema.size(); ++i) {
        if (!schema[i].auto_increment) continue;
        bool given = false;

        for (const auto& kv : values) {
            if (kv.first == schema[i].name && !kv.second.is_null()) { given = true; break; }
        }

        out[i] = !given;
    }

    return out;
}

std::vector<bool> generated_from(const Schema& schema, const Row& values) {
    std::vector<bool> out(schema.size(), false);
    for (std::size_t i = 0; i < schema.size(); ++i) out[i] = schema[i].auto_increment && (i >= values.size() || values[i].is_null());
    return out;
}

} // namespace

std::string encode_create_table(const std::string& name, const Schema& schema) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::CreateTable));
    w.str(name);
    w.schema(schema);
    return std::move(w.data());
}

std::string encode_create_partitioned(const std::string& name, const Schema& schema, const PartitionOptions& options) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(WalOp::CreatePartitioned));
    w.str(name);
    w.schema(schema);
    write_partition_options(w, options);
    return std::move(w.data());
}

std::string encode_index(WalOp op, const std::string& table, const IndexDef& def) {
    ByteWriter w;
    w.u8(static_cast<std::uint8_t>(op));
    w.str(table);
    if (op == WalOp::CreateIndex) w.index_def(def);
    else w.str(def.name);
    return std::move(w.data());
}

Transaction::Transaction(Database& db) : m_db(&db), m_lock(db.m_mutex), m_active(true), m_id(db.m_next_txn++) {}

Transaction::Transaction(Transaction&& other) noexcept
    : m_db(other.m_db), m_lock(std::move(other.m_lock)), m_undo(std::move(other.m_undo)), m_wal(std::move(other.m_wal)), m_active(other.m_active), m_id(other.m_id) {
    other.m_active = false;
    other.m_db = nullptr;
}

Transaction::~Transaction() {
    if (m_active) {
        try {
            rollback();
        } catch (...) {
        }
    }
}

void Transaction::ensure_active() const {
    if (!m_active) throw DatabaseError("the transaction has already finished");
}

Table& Transaction::writable(const std::string& name) {
    ensure_active();
    Table* t = m_db->find_locked(name);
    if (!t) throw DatabaseError("no table named '" + name + "'");
    return *t;
}

PartitionedTable& Transaction::partitioned(const std::string& name) {
    ensure_active();
    return m_db->partitioned_locked(name);
}

void Transaction::log(std::string rec) {
    if (m_db->persistent()) m_wal.push_back(std::move(rec));
}

const Table& Transaction::table(const std::string& name) const {
    ensure_active();
    return m_db->table_locked(name);
}

Table& Transaction::create_table(const std::string& name, Schema schema) {
    ensure_active();
    if (m_db->name_taken(name)) throw DatabaseError("table '" + name + "' already exists");
    auto t = std::make_unique<Table>(name, schema);
    Table& ref = *t;
    m_db->m_tables.emplace(name, std::move(t));
    Undo u;
    u.kind = UndoKind::CreateTable;
    u.table = name;
    m_undo.push_back(std::move(u));
    log(encode_create_table(name, schema));
    return ref;
}

void Transaction::create_partitioned_table(const std::string& name, Schema schema, PartitionOptions options) {
    ensure_active();
    if (m_db->name_taken(name)) throw DatabaseError("table '" + name + "' already exists");
    auto pt = std::make_unique<PartitionedTable>(name, std::move(schema), std::move(options));
    PartitionedTable& ref = *pt;
    m_db->m_partitioned.emplace(name, std::move(pt));
    Undo u;
    u.kind = UndoKind::CreatePartitioned;
    u.table = name;
    m_undo.push_back(std::move(u));
    log(encode_create_partitioned(name, ref.schema(), ref.options()));
}

bool Transaction::drop_table(const std::string& name) {
    ensure_active();

    auto pit = m_db->m_partitioned.find(name);

    if (pit != m_db->m_partitioned.end()) {
        Undo u;
        u.kind = UndoKind::DropPartitioned;
        u.table = name;
        m_db->detach_children(*pit->second);
        u.parent = std::move(pit->second);
        m_db->m_partitioned.erase(pit);
        m_undo.push_back(std::move(u));
        log(table_record(WalOp::DropTable, name));
        return true;
    }

    auto it = m_db->m_tables.find(name);
    if (it == m_db->m_tables.end()) return false;
    Undo u;
    u.kind = UndoKind::DropTable;
    u.table = name;
    u.dropped = std::move(it->second);
    m_db->m_tables.erase(it);
    m_undo.push_back(std::move(u));
    log(table_record(WalOp::DropTable, name));
    return true;
}

Index& Transaction::create_index(const std::string& table, IndexDef def) {
    ensure_active();

    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) {
        Index& model = pt->templ().create_index(def);
        const IndexDef d = model.def();
        std::size_t done = 0;

        try {
            if (d.unique) {
                const auto& cols = model.columns();
                const bool local = std::find(cols.begin(), cols.end(), pt->key_column()) != cols.end();

                if (!local && pt->size() > 1) {
                    collections::HashMap<std::string, char> seen;

                    for (const auto& part : pt->parts()) {
                        const Table& t = *part.table;
                        t.for_each([&](RowId r) {
                            bool has_null = false;
                            const std::string key = t.index_key(model, r, &has_null);
                            if (!has_null && !seen.insert_or_assign(key, 1).second) throw ConstraintError("cannot create unique index '" + d.name + "': duplicate values exist");
                        });
                    }
                }
            }

            for (auto& part : pt->parts()) {
                part.table->create_index(d);
                ++done;
            }
        } catch (...) {
            for (std::size_t k = 0; k < done; ++k) pt->parts()[k].table->drop_index(d.name);
            pt->templ().drop_index(d.name);
            throw;
        }

        pt->index_defs().push_back(d);
        Undo u;
        u.kind = UndoKind::CreateIndex;
        u.table = table;
        u.def = d;
        m_undo.push_back(std::move(u));
        log(encode_index(WalOp::CreateIndex, table, d));
        return *pt->templ().index(d.name);
    }

    Table& t = writable(table);
    Index& idx = t.create_index(def);
    Undo u;
    u.kind = UndoKind::CreateIndex;
    u.table = table;
    u.def = idx.def();
    m_undo.push_back(std::move(u));
    log(encode_index(WalOp::CreateIndex, table, idx.def()));
    return idx;
}

bool Transaction::drop_index(const std::string& table, const std::string& name) {
    ensure_active();

    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) {
        const Index* idx = pt->templ().index(name);
        if (!idx) return false;
        const IndexDef d = idx->def();
        auto& defs = pt->index_defs();
        const bool user = std::any_of(defs.begin(), defs.end(), [&](const IndexDef& x) { return x.name == name; });
        if (!user) throw DatabaseError("index '" + name + "' belongs to a key and can't be dropped");
        pt->templ().drop_index(name);
        for (auto& part : pt->parts()) part.table->drop_index(name);
        defs.erase(std::remove_if(defs.begin(), defs.end(), [&](const IndexDef& x) { return x.name == name; }), defs.end());
        Undo u;
        u.kind = UndoKind::DropIndex;
        u.table = table;
        u.def = d;
        m_undo.push_back(std::move(u));
        IndexDef only;
        only.name = name;
        log(encode_index(WalOp::DropIndex, table, only));
        return true;
    }

    Table& t = writable(table);
    const Index* idx = t.index(name);
    if (!idx) return false;
    Undo u;
    u.kind = UndoKind::DropIndex;
    u.table = table;
    u.def = idx->def();
    t.drop_index(name);
    m_undo.push_back(std::move(u));
    IndexDef d;
    d.name = name;
    log(encode_index(WalOp::DropIndex, table, d));
    return true;
}

RowId Transaction::insert_into(Table& t, const Row& prepared, const std::string& shown) {
    RowId slot = kNoRow;

    try {
        slot = t.insert(prepared);
    } catch (ConstraintError& e) {
        std::string msg = e.what();
        replace_all(msg, t.name(), shown);
        throw ConstraintError(msg);
    }

    Undo u;
    u.kind = UndoKind::Insert;
    u.table = t.name();
    u.slot = slot;
    m_undo.push_back(std::move(u));
    if (m_db->persistent()) log(insert_record(t.name(), slot, t.row(slot)));
    return slot;
}

bool Transaction::update_in(Table& t, RowId row, const Assignments& a, const std::string& shown) {
    if (!t.live(row)) return false;
    Row before = t.row(row);

    try {
        t.update(row, a);
    } catch (ConstraintError& e) {
        std::string msg = e.what();
        replace_all(msg, t.name(), shown);
        throw ConstraintError(msg);
    }

    Undo u;
    u.kind = UndoKind::Update;
    u.table = t.name();
    u.slot = row;
    u.row = std::move(before);
    m_undo.push_back(std::move(u));

    if (m_db->persistent()) {
        Assignments stored;
        for (const auto& c : a) stored.emplace_back(c.first, t.get(row, c.first));
        log(update_record(t.name(), row, stored));
    }

    return true;
}

bool Transaction::remove_in(Table& t, RowId row) {
    if (!t.live(row)) return false;
    Row before = t.row(row);
    t.erase(row);
    Undo u;
    u.kind = UndoKind::Erase;
    u.table = t.name();
    u.slot = row;
    u.row = std::move(before);
    m_undo.push_back(std::move(u));
    log(delete_record(t.name(), row));
    return true;
}

void Transaction::observe(PartitionedTable& pt, const Row& row) {
    const Value before = pt.newest();
    pt.observe(row);

    if (compare(before, pt.newest()) != 0 || before.is_null() != pt.newest().is_null()) {
        Undo u;
        u.kind = UndoKind::Newest;
        u.table = pt.name();
        u.value = before;
        m_undo.push_back(std::move(u));
    }
}

void Transaction::check_unique_across(PartitionedTable& pt, const Row& row, std::size_t skip, const std::vector<bool>& generated) const {
    if (pt.size() <= 1 && skip < pt.size()) return;
    const std::size_t kc = pt.key_column();

    for (const auto& idx : pt.templ().indexes()) {
        if (!idx->unique()) continue;
        const auto& cols = idx->columns();
        if (std::find(cols.begin(), cols.end(), kc) != cols.end()) continue;
        bool all_generated = true;
        for (std::size_t c : cols) if (!generated[c]) { all_generated = false; break; }
        if (all_generated) continue;
        bool has_null = false;
        const std::string key = pt.templ().index_key(*idx, row, &has_null);
        if (has_null) continue;

        for (std::size_t i = 0; i < pt.size(); ++i) {
            if (i == skip) continue;
            const Index* other = twin_index(*pt.parts()[i].table, *idx);

            if (other && other->would_conflict(key, kNoRow)) {
                std::string names;
                for (std::size_t c : cols) names += (names.empty() ? "" : ", ") + pt.name() + "." + pt.schema()[c].name;
                throw ConstraintError("UNIQUE constraint failed: " + names);
            }
        }
    }
}

std::size_t Transaction::add_partition(PartitionedTable& pt, const Value& key) {
    std::uint64_t seq = pt.next_seq();
    while (m_db->name_taken(pt.child_name(seq))) ++seq;
    PartitionPart part = pt.make_part(key, seq, pt.child_name(seq));
    const std::string child = part.table->name();
    log(add_partition_record(pt.name(), part));
    const std::size_t at = pt.attach(std::move(part));
    m_db->m_children[child] = { &pt, pt.parts()[at].table.get() };
    Undo u;
    u.kind = UndoKind::AddPartition;
    u.table = pt.name();
    u.child = child;
    m_undo.push_back(std::move(u));
    return at;
}

void Transaction::remove_partition(PartitionedTable& pt, std::size_t index, DropReason reason, std::vector<PartitionInfo>* out, bool notify) {
    PartitionInfo info = pt.info(pt.parts().at(index), reason);
    if (notify && pt.options().on_drop) pt.options().on_drop(info, *pt.parts()[index].table);
    PartitionPart part = pt.detach(index);
    m_db->m_children.erase(info.table);
    log(remove_partition_record(pt.name(), info.table));
    Undo u;
    u.kind = UndoKind::RemovePartition;
    u.table = pt.name();
    u.child = info.table;
    u.part = std::make_unique<PartitionPart>(std::move(part));
    m_undo.push_back(std::move(u));
    if (out) out->push_back(std::move(info));
}

std::vector<PartitionInfo> Transaction::enforce(PartitionedTable& pt) {
    std::vector<PartitionInfo> out;
    const PartitionOptions& o = pt.options();

    if (o.retention) {
        const auto cut = pt.cutoff();

        for (std::size_t i = 0; i < pt.size();) {
            if (pt.expired(pt.parts()[i], cut)) remove_partition(pt, i, DropReason::Age, &out, true);
            else ++i;
        }
    }

    if (o.drop_empty) {
        for (std::size_t i = 0; i < pt.size();) {
            if (pt.parts()[i].table->size() == 0) remove_partition(pt, i, DropReason::Empty, &out, true);
            else ++i;
        }
    }

    if (o.max_partitions) {
        while (pt.size() > o.max_partitions) remove_partition(pt, 0, DropReason::Count, &out, true);
    }

    if (o.max_rows) {
        while (pt.size() > 1 && pt.rows() > o.max_rows) remove_partition(pt, 0, DropReason::Rows, &out, true);
    }

    if (o.max_bytes) {
        while (pt.size() > 1 && pt.memory_bytes() > o.max_bytes) remove_partition(pt, 0, DropReason::Bytes, &out, true);
    }

    return out;
}

RowId Transaction::route(PartitionedTable& pt, const Row& prepared, const std::vector<bool>& generated, bool moving) {
    const Value& key = prepared[pt.key_column()];
    if (key.is_null()) throw ConstraintError("NOT NULL constraint failed: " + pt.name() + "." + pt.key_name() + " (it picks the partition)");

    if (pt.is_late(key)) {
        if (moving || pt.options().late_rows == LateRowPolicy::Reject) {
            throw ConstraintError("row with " + pt.key_name() + " = " + key.to_string() + " is older than '" + pt.name() + "' keeps");
        }

        return kNoRow;
    }

    bool created = false;
    std::optional<std::size_t> at = pt.find(key);

    if (!at) {
        at = add_partition(pt, key);
        created = true;
    }

    check_unique_across(pt, prepared, *at, generated);
    const RowId slot = insert_into(*pt.parts()[*at].table, prepared, pt.name());
    observe(pt, prepared);
    if (created && !moving && pt.options().has_limits()) enforce(pt);
    return slot;
}

RowId Transaction::insert(const std::string& table, const NamedValues& values) {
    ensure_active();

    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) {
        const std::vector<bool> generated = generated_from(pt->schema(), values);
        const Row prepared = pt->templ().prepare_row(values);
        return route(*pt, prepared, generated, false);
    }

    Table& t = writable(table);
    const RowId slot = t.insert(values);
    Undo u;
    u.kind = UndoKind::Insert;
    u.table = table;
    u.slot = slot;
    m_undo.push_back(std::move(u));
    if (m_db->persistent()) log(insert_record(table, slot, t.row(slot)));
    return slot;
}

RowId Transaction::insert_row(const std::string& table, const Row& values) {
    ensure_active();

    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) {
        const std::vector<bool> generated = generated_from(pt->schema(), values);
        const Row prepared = pt->templ().prepare_row(values);
        return route(*pt, prepared, generated, false);
    }

    Table& t = writable(table);
    const RowId slot = t.insert(values);
    Undo u;
    u.kind = UndoKind::Insert;
    u.table = table;
    u.slot = slot;
    m_undo.push_back(std::move(u));
    if (m_db->persistent()) log(insert_record(table, slot, t.row(slot)));
    return slot;
}

std::size_t Transaction::insert_many(const std::string& table, const std::vector<Row>& rows) {
    ensure_active();

    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) {
        m_undo.reserve(m_undo.size() + rows.size());
        std::size_t n = 0;

        for (const auto& r : rows) {
            const std::vector<bool> generated = generated_from(pt->schema(), r);
            const Row prepared = pt->templ().prepare_row(r);
            if (route(*pt, prepared, generated, false) != kNoRow) ++n;
        }

        return n;
    }

    Table& t = writable(table);
    m_undo.reserve(m_undo.size() + rows.size());

    for (const auto& r : rows) {
        const RowId slot = t.insert(r);
        Undo u;
        u.kind = UndoKind::Insert;
        u.table = table;
        u.slot = slot;
        m_undo.push_back(std::move(u));
        if (m_db->persistent()) log(insert_record(table, slot, t.row(slot)));
    }

    return rows.size();
}

bool Transaction::update_row(const std::string& table, RowId row, const NamedValues& changes) {
    ensure_active();
    if (m_db->find_partitioned_locked(table)) throw DatabaseError("rows of partitioned table '" + table + "' are changed by condition; use update(table, where, changes)");
    Table& t = writable(table);
    if (!t.live(row)) return false;
    Assignments a;
    for (const auto& kv : changes) a.emplace_back(t.column_index(kv.first), kv.second);
    return update_in(t, row, a, table);
}

std::vector<std::pair<Table*, std::vector<RowId>>> Transaction::gather(PartitionedTable& pt, const Expr& where, const std::vector<Value>& params) {
    PartitionView v;
    m_db->view_locked(pt, v);
    std::vector<std::pair<Table*, std::vector<RowId>>> work;

    for (std::size_t id : prune_partitions(v, where, params)) {
        Table* t = m_db->find_any_locked(v.parts[id].table->name());
        work.emplace_back(t, matching_rows(*t, where, params));
    }

    return work;
}

std::size_t Transaction::update_partitioned(PartitionedTable& pt, const Expr& where, const NamedValues& changes, const std::vector<Value>& params) {
    const Schema& schema = pt.schema();
    const std::size_t kc = pt.key_column();
    Assignments a;
    std::vector<bool> changed(schema.size(), false);

    for (const auto& kv : changes) {
        const std::size_t c = schema.index_of(kv.first);
        a.emplace_back(c, kv.second);
        changed[c] = true;
    }

    bool across = false;

    for (const auto& idx : pt.templ().indexes()) {
        if (!idx->unique()) continue;
        const auto& cols = idx->columns();
        if (std::find(cols.begin(), cols.end(), kc) != cols.end()) continue;
        for (std::size_t c : cols) if (changed[c]) { across = true; break; }
    }

    const std::vector<bool> none(schema.size(), false);
    const auto work = gather(pt, where, params);
    const std::size_t before_parts = pt.size();
    std::size_t n = 0;

    for (const auto& w : work) {
        Table& t = *w.first;

        for (RowId r : w.second) {
            if (!t.live(r)) continue;
            Row next = t.row(r);
            for (const auto& c : a) next[c.first] = schema.coerce(c.first, c.second);
            const auto self = pt.find_child(t.name());

            if (changed[kc]) {
                if (next[kc].is_null()) throw ConstraintError("NOT NULL constraint failed: " + pt.name() + "." + pt.key_name() + " (it picks the partition)");

                if (self && pt.key_for(next[kc]) != pt.parts()[*self].key) {
                    const Row prepared = pt.templ().prepare_row(next);
                    remove_in(t, r);
                    route(pt, prepared, none, true);
                    ++n;
                    continue;
                }
            }

            if (across) check_unique_across(pt, next, self ? *self : pt.size(), none);
            update_in(t, r, a, pt.name());
            if (changed[kc]) observe(pt, next);
            ++n;
        }
    }

    if (pt.size() != before_parts || pt.options().drop_empty) {
        if (pt.options().has_limits()) enforce(pt);
    }

    return n;
}

std::size_t Transaction::update(const std::string& table, const Expr& where, const NamedValues& changes, const std::vector<Value>& params) {
    ensure_active();
    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) return update_partitioned(*pt, where, changes, params);
    Table& t = writable(table);
    const std::vector<RowId> rows = matching_rows(t, where, params);
    for (RowId r : rows) update_row(table, r, changes);
    return rows.size();
}

bool Transaction::remove_row(const std::string& table, RowId row) {
    ensure_active();
    if (m_db->find_partitioned_locked(table)) throw DatabaseError("rows of partitioned table '" + table + "' are removed by condition; use remove(table, where)");
    Table& t = writable(table);
    return remove_in(t, row);
}

std::size_t Transaction::remove_partitioned(PartitionedTable& pt, const Expr& where, const std::vector<Value>& params) {
    const auto work = gather(pt, where, params);
    std::size_t n = 0;

    for (const auto& w : work) {
        for (RowId r : w.second) if (remove_in(*w.first, r)) ++n;
    }

    if (n && pt.options().drop_empty) enforce(pt);
    return n;
}

std::size_t Transaction::remove(const std::string& table, const Expr& where, const std::vector<Value>& params) {
    ensure_active();
    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) return remove_partitioned(*pt, where, params);
    Table& t = writable(table);
    const std::vector<RowId> rows = matching_rows(t, where, params);
    for (RowId r : rows) remove_row(table, r);
    return rows.size();
}

std::size_t Transaction::clear(const std::string& table) {
    ensure_active();

    if (PartitionedTable* pt = m_db->find_partitioned_locked(table)) {
        const std::size_t n = pt->rows();
        while (pt->size()) remove_partition(*pt, 0, DropReason::Manual, nullptr, false);
        return n;
    }

    Table& t = writable(table);
    Undo u;
    u.kind = UndoKind::Clear;
    u.table = table;
    t.for_each([&](RowId r) { u.rows.emplace_back(r, t.row(r)); });
    const std::size_t n = u.rows.size();
    t.clear();
    m_undo.push_back(std::move(u));
    log(table_record(WalOp::Clear, table));
    return n;
}

void Transaction::set_partition_options(const std::string& table, PartitionOptions options) {
    PartitionedTable& pt = partitioned(table);
    if (!options.on_drop) options.on_drop = pt.options().on_drop;
    if (!options.clock) options.clock = pt.options().clock;
    auto old = std::make_shared<PartitionOptions>(pt.options());
    pt.set_options(std::move(options));
    Undo u;
    u.kind = UndoKind::SetOptions;
    u.table = table;
    u.options = std::move(old);
    m_undo.push_back(std::move(u));
    log(options_record(table, pt.options()));
    if (pt.options().has_limits()) enforce(pt);
}

bool Transaction::drop_partition(const std::string& table, const std::string& partition) {
    PartitionedTable& pt = partitioned(table);
    const auto at = pt.find_named(partition);
    if (!at) return false;
    remove_partition(pt, *at, DropReason::Manual, nullptr, true);
    return true;
}

std::size_t Transaction::drop_partitions_before(const std::string& table, const Value& key) {
    PartitionedTable& pt = partitioned(table);
    if (!pt.by_range()) throw DatabaseError("drop_partitions_before needs range partitions");
    std::size_t n = 0;

    for (std::size_t i = 0; i < pt.size();) {
        const Value& hi = pt.parts()[i].hi;

        if (!hi.is_null() && compare(hi, key) <= 0) {
            remove_partition(pt, i, DropReason::Manual, nullptr, true);
            ++n;
        } else {
            ++i;
        }
    }

    return n;
}

std::vector<PartitionInfo> Transaction::enforce_retention(const std::string& table) {
    return enforce(partitioned(table));
}

ResultSet Transaction::query(const Query& q, const std::vector<Value>& params) const {
    ensure_active();
    return m_db->run_query(q, params, nullptr);
}

void Transaction::commit() {
    ensure_active();

    if (m_db->persistent() && !m_wal.empty()) {
        std::vector<std::string> batch;
        batch.reserve(m_wal.size() + 2);
        batch.push_back(txn_record(WalOp::Begin, m_id));
        for (auto& r : m_wal) batch.push_back(std::move(r));
        batch.push_back(txn_record(WalOp::Commit, m_id));
        m_db->m_wal.append_batch(batch);
    }

    m_wal.clear();
    m_undo.clear();
    m_active = false;
    ++m_db->m_commits;

    if (m_db->persistent() && m_db->m_options.auto_checkpoint && m_db->m_wal.size() > m_db->m_options.checkpoint_bytes) {
        m_db->checkpoint_locked();
    }

    m_lock.unlock();
}

void Transaction::rollback() {
    ensure_active();
    m_active = false;

    for (auto it = m_undo.rbegin(); it != m_undo.rend(); ++it) {
        Undo& u = *it;

        switch (u.kind) {
            case UndoKind::Insert:
                if (Table* t = m_db->find_any_locked(u.table)) t->erase(u.slot);
                break;
            case UndoKind::Erase:
                if (Table* t = m_db->find_any_locked(u.table)) t->insert_at(u.slot, u.row);
                break;
            case UndoKind::Update:
                if (Table* t = m_db->find_any_locked(u.table)) {
                    Assignments a;
                    for (std::size_t c = 0; c < u.row.size(); ++c) a.emplace_back(c, u.row[c]);
                    t->update(u.slot, a);
                }
                break;
            case UndoKind::CreateTable:
                m_db->m_tables.erase(u.table);
                break;
            case UndoKind::DropTable:
                m_db->m_tables[u.table] = std::move(u.dropped);
                break;
            case UndoKind::CreateIndex:
                m_db->apply_index(u.table, u.def, false);
                break;
            case UndoKind::DropIndex:
                m_db->apply_index(u.table, u.def, true);
                break;
            case UndoKind::Clear:
                if (Table* t = m_db->find_any_locked(u.table)) {
                    for (const auto& r : u.rows) t->insert_at(r.first, r.second);
                }
                break;
            case UndoKind::CreatePartitioned: {
                auto pit = m_db->m_partitioned.find(u.table);

                if (pit != m_db->m_partitioned.end()) {
                    m_db->detach_children(*pit->second);
                    m_db->m_partitioned.erase(pit);
                }
                break;
            }
            case UndoKind::DropPartitioned:
                m_db->attach_children(*u.parent);
                m_db->m_partitioned[u.table] = std::move(u.parent);
                break;
            case UndoKind::AddPartition:
                if (PartitionedTable* pt = m_db->find_partitioned_locked(u.table)) {
                    if (const auto at = pt->find_child(u.child)) pt->detach(*at);
                }
                m_db->m_children.erase(u.child);
                break;
            case UndoKind::RemovePartition:
                if (PartitionedTable* pt = m_db->find_partitioned_locked(u.table)) {
                    const std::size_t at = pt->attach(std::move(*u.part));
                    m_db->m_children[u.child] = { pt, pt->parts()[at].table.get() };
                }
                break;
            case UndoKind::SetOptions:
                if (PartitionedTable* pt = m_db->find_partitioned_locked(u.table)) pt->set_options(*u.options);
                break;
            case UndoKind::Newest:
                if (PartitionedTable* pt = m_db->find_partitioned_locked(u.table)) pt->set_newest(u.value);
                break;
        }
    }

    m_undo.clear();
    m_wal.clear();
    ++m_db->m_rollbacks;
    m_lock.unlock();
}

Statement::Statement(Database& db, Query q) : m_db(&db), m_query(std::move(q)) {
    m_query.fingerprint(m_fingerprint);
}

ResultSet Statement::run(const std::vector<Value>& params) const {
    if (!m_db) throw DatabaseError("empty statement");
    std::shared_lock<std::shared_mutex> lk(m_db->m_mutex);
    return m_db->run_query(m_query, params, &m_fingerprint);
}

Database::Database(DatabaseOptions options) : m_options(std::move(options)), m_cache(m_options.cache_entries, m_options.cache_rows) {
    if (persistent()) load();
}

Database::~Database() {
    try {
        stop_retention();
    } catch (...) {
    }

    try {
        if (persistent() && m_wal.is_open()) m_wal.close();
    } catch (...) {
    }
}

std::unique_ptr<Database> Database::open(const std::string& path, SyncMode sync) {
    DatabaseOptions o;
    o.path = path;
    o.sync = sync;
    return std::make_unique<Database>(o);
}

Transaction Database::begin() {
    return Transaction(*this);
}

Table* Database::find_locked(const std::string& name) noexcept {
    auto it = m_tables.find(name);
    return it == m_tables.end() ? nullptr : it->second.get();
}

const Table* Database::find_locked(const std::string& name) const noexcept {
    auto it = m_tables.find(name);
    return it == m_tables.end() ? nullptr : it->second.get();
}

Table* Database::find_any_locked(const std::string& name) noexcept {
    if (Table* t = find_locked(name)) return t;
    auto it = m_children.find(name);
    return it == m_children.end() ? nullptr : it->second.second;
}

PartitionedTable* Database::find_partitioned_locked(const std::string& name) const noexcept {
    auto it = m_partitioned.find(name);
    return it == m_partitioned.end() ? nullptr : it->second.get();
}

PartitionedTable& Database::partitioned_locked(const std::string& name) const {
    PartitionedTable* pt = find_partitioned_locked(name);
    if (pt) return *pt;
    if (find_locked(name)) throw DatabaseError("table '" + name + "' is not partitioned");
    throw DatabaseError("no table named '" + name + "'");
}

bool Database::name_taken(const std::string& name) const noexcept {
    return m_tables.count(name) || m_partitioned.count(name) || m_children.count(name);
}

const Table& Database::table_locked(const std::string& name) const {
    const Table* t = find_locked(name);
    if (t) return *t;
    if (find_partitioned_locked(name)) throw DatabaseError("table '" + name + "' is partitioned; query it, or use partitions() to see its parts");
    throw DatabaseError("no table named '" + name + "'");
}

void Database::view_locked(const PartitionedTable& pt, PartitionView& out) const {
    out.schema_table = &pt.templ();
    out.column = pt.key_column();
    out.by_range = pt.by_range();
    out.total = pt.size();
    out.parts.clear();

    for (std::size_t i : pt.visible()) {
        const PartitionPart& p = pt.parts()[i];
        out.parts.push_back(PartitionView::Part{ p.table.get(), p.lo, p.hi, p.value });
    }
}

void Database::attach_children(PartitionedTable& pt) {
    for (auto& p : pt.parts()) m_children[p.table->name()] = { &pt, p.table.get() };
}

void Database::detach_children(PartitionedTable& pt) {
    for (auto& p : pt.parts()) m_children.erase(p.table->name());
}

void Database::apply_index(const std::string& table, const IndexDef& def, bool create) {
    if (PartitionedTable* pt = find_partitioned_locked(table)) {
        auto& defs = pt->index_defs();

        if (create) {
            if (!pt->templ().index(def.name)) pt->templ().create_index(def);
            for (auto& p : pt->parts()) if (!p.table->index(def.name)) p.table->create_index(def);
            if (std::none_of(defs.begin(), defs.end(), [&](const IndexDef& d) { return d.name == def.name; })) defs.push_back(def);
        } else {
            pt->templ().drop_index(def.name);
            for (auto& p : pt->parts()) p.table->drop_index(def.name);
            defs.erase(std::remove_if(defs.begin(), defs.end(), [&](const IndexDef& d) { return d.name == def.name; }), defs.end());
        }

        return;
    }

    Table* t = find_locked(table);
    if (!t) return;
    if (create) t->create_index(def);
    else t->drop_index(def.name);
}

TableResolver Database::resolver() const {
    return [this](const std::string& name) { return find_locked(name); };
}

PartitionResolver Database::partition_resolver() const {
    return [this](const std::string& name, PartitionView& out) {
        const PartitionedTable* pt = find_partitioned_locked(name);
        if (!pt) return false;
        view_locked(*pt, out);
        return true;
    };
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> Database::signature(const Query& q) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> out;

    for (const auto& name : q.referenced_tables()) {
        if (const Table* t = find_locked(name)) {
            out.emplace_back(t->uid(), t->version());
        } else if (const PartitionedTable* pt = find_partitioned_locked(name)) {
            out.emplace_back(pt->uid(), pt->layout_version());

            for (std::size_t i : pt->visible()) {
                const Table& c = *pt->parts()[i].table;
                out.emplace_back(c.uid(), c.version());
            }
        } else {
            out.emplace_back(0, 0);
        }
    }

    return out;
}

Table& Database::create_table(const std::string& name, Schema schema) {
    return transaction([&](Transaction& tx) -> Table& { return tx.create_table(name, std::move(schema)); });
}

void Database::create_partitioned_table(const std::string& name, Schema schema, PartitionOptions options) {
    transaction([&](Transaction& tx) { tx.create_partitioned_table(name, std::move(schema), std::move(options)); });
}

bool Database::drop_table(const std::string& name) {
    return transaction([&](Transaction& tx) { return tx.drop_table(name); });
}

void Database::create_index(const std::string& table, IndexDef def) {
    transaction([&](Transaction& tx) { tx.create_index(table, std::move(def)); });
}

void Database::create_index(const std::string& table, const std::vector<std::string>& columns, IndexKind kind, bool unique, const std::string& name) {
    IndexDef d;
    d.name = name;
    d.columns = columns;
    d.kind = kind;
    d.unique = unique;
    create_index(table, std::move(d));
}

bool Database::drop_index(const std::string& table, const std::string& name) {
    return transaction([&](Transaction& tx) { return tx.drop_index(table, name); });
}

RowId Database::insert(const std::string& table, const NamedValues& values) {
    return transaction([&](Transaction& tx) { return tx.insert(table, values); });
}

RowId Database::insert_row(const std::string& table, const Row& values) {
    return transaction([&](Transaction& tx) { return tx.insert_row(table, values); });
}

std::size_t Database::insert_many(const std::string& table, const std::vector<Row>& rows) {
    return transaction([&](Transaction& tx) { return tx.insert_many(table, rows); });
}

std::size_t Database::update(const std::string& table, const Expr& where, const NamedValues& changes, const std::vector<Value>& params) {
    return transaction([&](Transaction& tx) { return tx.update(table, where, changes, params); });
}

std::size_t Database::remove(const std::string& table, const Expr& where, const std::vector<Value>& params) {
    return transaction([&](Transaction& tx) { return tx.remove(table, where, params); });
}

void Database::notify(const Query& q, const QueryStats& stats) const {
    QueryHook hook;

    {
        std::lock_guard<std::mutex> lk(m_hook_mutex);
        hook = m_hook;
    }

    if (hook) hook(q, stats);
}

ResultSet Database::run_query(const Query& q, const std::vector<Value>& params, const std::string* fingerprint) const {
    ++m_queries;
    const bool use_cache = q.cacheable() && m_options.cache_entries > 0;
    std::string key;

    if (use_cache) {
        if (fingerprint) key = *fingerprint;
        else q.fingerprint(key);
        key += "|P";
        for (const auto& p : params) encode_key_part(key, p);
        const auto hit = m_cache.get(key);

        if (hit) {
            if (hit->versions == signature(q)) {
                ResultSet out = *hit->result;
                out.stats.cache_hit = true;
                out.stats.elapsed = std::chrono::microseconds(0);
                notify(q, out.stats);
                return out;
            }

            m_cache.erase(key);
        }
    }

    ResultSet rs = execute_query(q, resolver(), partition_resolver(), params);

    if (use_cache) {
        CachedResult c;
        c.versions = signature(q);
        c.result = std::make_shared<const ResultSet>(rs);
        m_cache.put(key, std::move(c), rs.rows.size() + 1);
    }

    notify(q, rs.stats);
    return rs;
}

ResultSet Database::query(const Query& q, const std::vector<Value>& params) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    return run_query(q, params, nullptr);
}

ResultSet Database::select(const std::string& table, const Expr& where, std::optional<std::size_t> limit) const {
    Query q(table);
    if (where) q.where(where);
    if (limit) q.limit(*limit);
    return query(q);
}

std::optional<Row> Database::get(const std::string& table, const std::vector<Value>& key) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);

    if (const PartitionedTable* pt = find_partitioned_locked(table)) {
        if (!pt->templ().primary_index()) throw DatabaseError("table '" + table + "' has no primary key");

        for (std::size_t i : pt->visible()) {
            const Table& t = *pt->parts()[i].table;
            const RowId r = t.find_by_key(key);
            if (r != kNoRow) return t.row(r);
        }

        return std::nullopt;
    }

    const Table& t = table_locked(table);
    const RowId r = t.find_by_key(key);
    if (r == kNoRow) return std::nullopt;
    return t.row(r);
}

std::size_t Database::count(const std::string& table, const Expr& where) const {
    Query q(table);
    if (where) q.where(where);
    q.count();
    const Value v = query(q).scalar();
    return v.is_int() ? static_cast<std::size_t>(v.get_int()) : 0;
}

std::string Database::explain(const Query& q, const std::vector<Value>& params) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    return explain_query(q, resolver(), partition_resolver(), params);
}

Statement Database::prepare(Query q) {
    return Statement(*this, std::move(q));
}

std::shared_ptr<const Graph> Database::graph(const std::string& edge_table, const GraphOptions& options) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    const Table& t = table_locked(edge_table);
    const std::string key = edge_table + "|" + options.from + "|" + options.to + "|" + options.weight + (options.directed ? "|d" : "|u");

    {
        std::lock_guard<std::mutex> g(m_graph_mutex);
        auto it = m_graphs.find(key);
        if (it != m_graphs.end() && it->second.uid == t.uid() && it->second.version == t.version()) return it->second.graph;
    }

    auto built = std::make_shared<const Graph>(t, options);
    std::lock_guard<std::mutex> g(m_graph_mutex);
    m_graphs[key] = CachedGraph{ built, t.uid(), t.version() };
    return built;
}

bool Database::is_partitioned(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    return find_partitioned_locked(name) != nullptr;
}

std::vector<PartitionInfo> Database::partitions(const std::string& table) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    const PartitionedTable& pt = partitioned_locked(table);
    std::vector<PartitionInfo> out;
    out.reserve(pt.size());
    for (const auto& p : pt.parts()) out.push_back(pt.info(p));
    return out;
}

PartitionOptions Database::partition_options(const std::string& table) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    return partitioned_locked(table).options();
}

void Database::set_partition_options(const std::string& table, PartitionOptions options) {
    transaction([&](Transaction& tx) { tx.set_partition_options(table, std::move(options)); });
}

void Database::on_partition_drop(const std::string& table, PartitionOptions::DropHook hook) {
    std::unique_lock<std::shared_mutex> lk(m_mutex);
    partitioned_locked(table).set_drop_hook(std::move(hook));
}

void Database::set_partition_clock(const std::string& table, PartitionOptions::Clock clock) {
    std::unique_lock<std::shared_mutex> lk(m_mutex);
    partitioned_locked(table).set_clock(std::move(clock));
}

bool Database::drop_partition(const std::string& table, const std::string& partition) {
    return transaction([&](Transaction& tx) { return tx.drop_partition(table, partition); });
}

std::size_t Database::drop_partitions_before(const std::string& table, const Value& key) {
    return transaction([&](Transaction& tx) { return tx.drop_partitions_before(table, key); });
}

std::vector<PartitionInfo> Database::enforce_retention(const std::string& table) {
    return transaction([&](Transaction& tx) { return tx.enforce_retention(table); });
}

std::vector<PartitionInfo> Database::enforce_retention() {
    return transaction([&](Transaction& tx) {
        std::vector<PartitionInfo> all;

        for (auto& kv : m_partitioned) {
            if (!kv.second->options().has_limits()) continue;
            auto some = tx.enforce_retention(kv.first);
            for (auto& i : some) all.push_back(std::move(i));
        }

        return all;
    });
}

void Database::start_retention(std::chrono::milliseconds every) {
    stop_retention();
    if (every.count() <= 0) throw DatabaseError("the retention interval must be positive");

    {
        std::lock_guard<std::mutex> lk(m_sweep_mutex);
        m_sweep_stop = false;
    }

    m_sweeper = std::thread([this, every]() {
        for (;;) {
            std::unique_lock<std::mutex> lk(m_sweep_mutex);
            if (m_sweep_cv.wait_for(lk, every, [this]() { return m_sweep_stop; })) return;
            lk.unlock();

            try {
                enforce_retention();
            } catch (...) {
            }
        }
    });
}

void Database::stop_retention() {
    {
        std::lock_guard<std::mutex> lk(m_sweep_mutex);
        m_sweep_stop = true;
    }

    m_sweep_cv.notify_all();
    if (m_sweeper.joinable()) m_sweeper.join();
}

bool Database::retention_running() const {
    return m_sweeper.joinable();
}

bool Database::has_table(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    return find_locked(name) != nullptr || find_partitioned_locked(name) != nullptr;
}

std::vector<std::string> Database::tables() const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    std::vector<std::string> out;
    for (const auto& kv : m_tables) out.push_back(kv.first);
    for (const auto& kv : m_partitioned) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

Schema Database::schema(const std::string& table) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    if (const PartitionedTable* pt = find_partitioned_locked(table)) return pt->schema();
    return table_locked(table).schema();
}

TableStats Database::table_stats(const std::string& table) const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    if (const PartitionedTable* pt = find_partitioned_locked(table)) return pt->stats();
    return table_locked(table).stats();
}

void Database::maintain() {
    std::unique_lock<std::shared_mutex> lk(m_mutex);
    for (auto& kv : m_tables) kv.second->maintain();
    for (auto& kv : m_partitioned) for (auto& p : kv.second->parts()) p.table->maintain();
}

void Database::set_cache(std::size_t entries, std::size_t rows) {
    m_options.cache_entries = entries;
    m_options.cache_rows = rows;
    m_cache.resize(entries, rows);
}

void Database::clear_cache() {
    m_cache.clear();
    std::lock_guard<std::mutex> g(m_graph_mutex);
    m_graphs.clear();
}

void Database::on_query(QueryHook hook) {
    std::lock_guard<std::mutex> lk(m_hook_mutex);
    m_hook = std::move(hook);
}

DatabaseStats Database::stats() const {
    std::shared_lock<std::shared_mutex> lk(m_mutex);
    DatabaseStats s;
    s.tables = m_tables.size() + m_partitioned.size();

    for (const auto& kv : m_tables) {
        const TableStats ts = kv.second->stats();
        s.rows += ts.rows;
        s.memory_bytes += ts.memory_bytes;
    }

    s.partitioned_tables = m_partitioned.size();

    for (const auto& kv : m_partitioned) {
        const TableStats ts = kv.second->stats();
        s.rows += ts.rows;
        s.memory_bytes += ts.memory_bytes;
        s.partitions += kv.second->size();
    }

    s.cache = m_cache.stats();
    s.wal_bytes = m_wal.size();
    s.queries = m_queries;
    s.commits = m_commits;
    s.rollbacks = m_rollbacks;
    s.checkpoints = m_checkpoints;
    return s;
}

std::string Database::build_snapshot() const {
    ByteWriter w;
    w.data().append(kSnapshotMagic, 8);
    w.u32(static_cast<std::uint32_t>(m_tables.size()));

    for (const auto& kv : m_tables) {
        const Table& t = *kv.second;
        w.str(t.name());
        w.schema(t.schema());
        std::vector<IndexDef> user;
        for (const auto& i : t.indexes()) if (!auto_index(t, i->name())) user.push_back(i->def());
        w.u32(static_cast<std::uint32_t>(user.size()));
        for (const auto& d : user) w.index_def(d);
        w.u32(static_cast<std::uint32_t>(t.auto_values().size()));

        for (const auto& a : t.auto_values()) {
            w.u32(static_cast<std::uint32_t>(a.first));
            w.i64(a.second);
        }

        write_rows(w, t);
    }

    w.u32(static_cast<std::uint32_t>(m_partitioned.size()));

    for (const auto& kv : m_partitioned) {
        const PartitionedTable& pt = *kv.second;
        w.str(pt.name());
        w.schema(pt.schema());
        write_partition_options(w, pt.options());
        w.u32(static_cast<std::uint32_t>(pt.index_defs().size()));
        for (const auto& d : pt.index_defs()) w.index_def(d);
        w.u32(static_cast<std::uint32_t>(pt.templ().auto_values().size()));

        for (const auto& a : pt.templ().auto_values()) {
            w.u32(static_cast<std::uint32_t>(a.first));
            w.i64(a.second);
        }

        w.value(pt.newest());
        w.u64(pt.next_seq());
        w.u32(static_cast<std::uint32_t>(pt.size()));

        for (const auto& p : pt.parts()) {
            w.str(p.table->name());
            w.u64(p.seq);
            w.str(p.label);
            w.value(p.lo);
            w.value(p.hi);
            w.value(p.value);
            w.i64(p.created_ms);
            write_rows(w, *p.table);
        }
    }

    const std::uint32_t crc = crypto::crc32(w.data().data(), w.data().size());
    w.u32(crc);
    return std::move(w.data());
}

void Database::load_snapshot(const std::string& bytes) {
    if (bytes.size() < 16) throw DatabaseError("'" + snapshot_path() + "' is not a fizmo database file");
    const bool v1 = bytes.compare(0, 8, std::string(kSnapshotMagicV1, 8)) == 0;
    const bool v2 = bytes.compare(0, 8, std::string(kSnapshotMagic, 8)) == 0;
    if (!v1 && !v2) throw DatabaseError("'" + snapshot_path() + "' is not a fizmo database file");
    ByteReader tail(bytes.data() + bytes.size() - 4, 4);
    if (crypto::crc32(bytes.data(), bytes.size() - 4) != tail.u32()) throw DatabaseError("'" + snapshot_path() + "' failed its checksum");
    ByteReader r(bytes.data() + 8, bytes.size() - 12);
    const std::uint32_t tables = r.u32();

    for (std::uint32_t i = 0; i < tables; ++i) {
        const std::string name = r.str();
        Schema schema = r.schema();
        auto t = std::make_unique<Table>(name, schema);
        const std::uint32_t nidx = r.u32();
        std::vector<IndexDef> user;
        for (std::uint32_t k = 0; k < nidx; ++k) user.push_back(r.index_def());
        const std::uint32_t nauto = r.u32();
        std::vector<std::pair<std::size_t, std::int64_t>> autos;

        for (std::uint32_t k = 0; k < nauto; ++k) {
            const std::uint32_t c = r.u32();
            autos.emplace_back(c, r.i64());
        }

        read_rows(r, *t);
        for (const auto& d : user) t->create_index(d);
        for (const auto& a : autos) t->set_auto_value(a.first, a.second);
        m_tables.emplace(name, std::move(t));
    }

    if (v1) return;
    const std::uint32_t parents = r.u32();

    for (std::uint32_t i = 0; i < parents; ++i) {
        const std::string name = r.str();
        Schema schema = r.schema();
        PartitionOptions options = read_partition_options(r);
        auto pt = std::make_unique<PartitionedTable>(name, std::move(schema), std::move(options));
        const std::uint32_t ndefs = r.u32();
        std::vector<IndexDef> defs;
        for (std::uint32_t k = 0; k < ndefs; ++k) defs.push_back(r.index_def());
        for (const auto& d : defs) pt->templ().create_index(d);
        const std::uint32_t nauto = r.u32();

        for (std::uint32_t k = 0; k < nauto; ++k) {
            const std::uint32_t c = r.u32();
            pt->templ().set_auto_value(c, r.i64());
        }

        pt->set_newest(r.value());
        pt->reserve_seq(r.u64() - 1);
        const std::uint32_t nparts = r.u32();

        for (std::uint32_t k = 0; k < nparts; ++k) {
            std::string child = r.str();
            const std::uint64_t seq = r.u64();
            std::string label = r.str();
            Value lo = r.value();
            Value hi = r.value();
            Value value = r.value();
            const std::int64_t created = r.i64();
            PartitionPart part = pt->restore_part(std::move(child), seq, std::move(label), std::move(lo), std::move(hi), std::move(value), created, false);
            read_rows(r, *part.table);
            for (const auto& d : defs) part.table->create_index(d);
            pt->attach(std::move(part));
        }

        pt->index_defs() = std::move(defs);
        attach_children(*pt);
        m_partitioned.emplace(name, std::move(pt));
    }
}

void Database::apply_wal(WalOp op, ByteReader& r) {
    auto missing = []() { return DatabaseError("write-ahead log refers to a missing table"); };

    switch (op) {
        case WalOp::CreateTable: {
            const std::string name = r.str();
            Schema schema = r.schema();
            m_tables[name] = std::make_unique<Table>(name, schema);
            break;
        }
        case WalOp::CreatePartitioned: {
            const std::string name = r.str();
            Schema schema = r.schema();
            PartitionOptions options = read_partition_options(r);
            m_partitioned[name] = std::make_unique<PartitionedTable>(name, std::move(schema), std::move(options));
            break;
        }
        case WalOp::DropTable: {
            const std::string name = r.str();
            auto pit = m_partitioned.find(name);

            if (pit != m_partitioned.end()) {
                detach_children(*pit->second);
                m_partitioned.erase(pit);
            } else {
                m_tables.erase(name);
            }
            break;
        }
        case WalOp::CreateIndex: {
            const std::string name = r.str();
            const IndexDef def = r.index_def();
            if (!find_locked(name) && !find_partitioned_locked(name)) throw missing();
            apply_index(name, def, true);
            break;
        }
        case WalOp::DropIndex: {
            const std::string name = r.str();
            IndexDef def;
            def.name = r.str();
            if (!find_locked(name) && !find_partitioned_locked(name)) throw missing();
            apply_index(name, def, false);
            break;
        }
        case WalOp::AddPartition: {
            PartitionedTable* pt = find_partitioned_locked(r.str());
            if (!pt) throw missing();
            std::string child = r.str();
            const std::uint64_t seq = r.u64();
            std::string label = r.str();
            Value lo = r.value();
            Value hi = r.value();
            Value value = r.value();
            const std::int64_t created = r.i64();
            const std::string name = child;
            const std::size_t at = pt->attach(pt->restore_part(std::move(child), seq, std::move(label), std::move(lo), std::move(hi), std::move(value), created, true));
            m_children[name] = { pt, pt->parts()[at].table.get() };
            break;
        }
        case WalOp::RemovePartition: {
            PartitionedTable* pt = find_partitioned_locked(r.str());
            if (!pt) throw missing();
            const std::string child = r.str();
            if (const auto at = pt->find_child(child)) pt->detach(*at);
            m_children.erase(child);
            break;
        }
        case WalOp::SetPartitionOptions: {
            PartitionedTable* pt = find_partitioned_locked(r.str());
            if (!pt) throw missing();
            PartitionOptions o = read_partition_options(r);
            o.on_drop = pt->options().on_drop;
            o.clock = pt->options().clock;
            pt->set_options(std::move(o));
            break;
        }
        case WalOp::Insert: {
            const std::string name = r.str();
            Table* t = find_any_locked(name);
            if (!t) throw missing();
            const RowId slot = r.u32();
            const Row row = r.row();
            t->insert_at(slot, row);
            auto c = m_children.find(name);
            if (c != m_children.end()) c->second.first->observe(row);
            break;
        }
        case WalOp::Delete: {
            Table* t = find_any_locked(r.str());
            if (!t) throw missing();
            t->erase(r.u32());
            break;
        }
        case WalOp::Update: {
            const std::string name = r.str();
            Table* t = find_any_locked(name);
            if (!t) throw missing();
            const RowId slot = r.u32();
            const std::uint32_t n = r.u32();
            Assignments a;

            for (std::uint32_t i = 0; i < n; ++i) {
                const std::uint32_t c = r.u32();
                a.emplace_back(c, r.value());
            }

            t->update(slot, a);
            auto c = m_children.find(name);
            if (c != m_children.end()) c->second.first->observe(t->row(slot));
            break;
        }
        case WalOp::Clear: {
            Table* t = find_any_locked(r.str());
            if (t) t->clear();
            break;
        }
        default:
            throw DatabaseError("write-ahead log has an unknown record");
    }
}

void Database::load() {
    std::unique_lock<std::shared_mutex> lk(m_mutex);
    std::string bytes;
    if (read_whole_file(snapshot_path(), bytes) && !bytes.empty()) load_snapshot(bytes);

    if (file_exists(wal_path())) {
        std::uint64_t good = 0;
        WriteAheadLog::replay(wal_path(), [this](WalOp op, ByteReader& r) { apply_wal(op, r); }, &good);
        std::error_code ec;
        const auto size = std::filesystem::file_size(wal_path(), ec);
        if (!ec && size > good) std::filesystem::resize_file(wal_path(), good, ec);
    }

    m_wal.open(wal_path(), m_options.sync);
}

void Database::checkpoint_locked() {
    if (!persistent()) return;
    const std::string bytes = build_snapshot();
    if (!atomic_write_file(snapshot_path(), bytes, m_options.sync != SyncMode::Off)) throw DatabaseError("writing the snapshot '" + snapshot_path() + "' failed");
    m_wal.truncate();
    ++m_checkpoints;
}

void Database::checkpoint() {
    std::unique_lock<std::shared_mutex> lk(m_mutex);
    checkpoint_locked();
}

} // namespace database
} // namespace fizmo
