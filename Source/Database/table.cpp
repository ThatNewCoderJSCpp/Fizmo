#include "fizmo_library.hpp"
#include "table.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <set>

namespace fizmo {
namespace database {

namespace {

std::atomic<std::uint64_t> g_table_uids{0};

} // namespace

Table::Table(std::string name, Schema schema) : m_name(std::move(name)), m_schema(std::move(schema)), m_uid(++g_table_uids) {
    if (!valid_identifier(m_name)) throw DatabaseError("invalid table name '" + m_name + "'");
    if (m_schema.size() == 0) throw DatabaseError("table '" + m_name + "' needs at least one column");
    m_schema.validate();

    for (const auto& c : m_schema.columns()) m_columns.emplace_back(c.type);

    for (std::size_t i = 0; i < m_schema.size(); ++i) {
        if (m_schema[i].auto_increment) m_auto[i] = 1;
    }

    const auto pk = m_schema.primary_key();

    if (!pk.empty()) {
        IndexDef d;
        d.name = "pk_" + m_name;
        d.kind = IndexKind::Hash;
        d.unique = true;
        for (auto c : pk) d.columns.push_back(m_schema[c].name);
        m_primary = &create_index(d);
    }

    for (std::size_t i = 0; i < m_schema.size(); ++i) {
        if (m_schema[i].unique && !(pk.size() == 1 && pk[0] == i)) {
            IndexDef d;
            d.name = "uq_" + m_name + "_" + m_schema[i].name;
            d.kind = IndexKind::Hash;
            d.unique = true;
            d.columns = { m_schema[i].name };
            create_index(d);
        }
    }

    m_schema_version = 1;
}

std::vector<std::size_t> Table::resolve_columns(const std::vector<std::string>& names) const {
    std::vector<std::size_t> out;
    for (const auto& n : names) out.push_back(m_schema.index_of(n));
    return out;
}

void Table::ensure_slots(std::size_t n) {
    if (n <= m_capacity) return;
    const std::size_t cap = std::max<std::size_t>({ n, m_capacity * 2, std::size_t(64) });
    for (auto& c : m_columns) c.resize(cap);
    m_live.resize(cap);
    m_capacity = cap;
}

RowId Table::allocate() {
    if (!m_free.empty()) {
        const RowId s = m_free.back();
        m_free.pop_back();
        return s;
    }

    if (m_slot_count >= kNoRow - 1) throw DatabaseError("table '" + m_name + "' is full");
    ensure_slots(m_slot_count + 1);
    return static_cast<RowId>(m_slot_count++);
}

std::string Table::index_key(const Index& index, const Row& row, bool* has_null) const {
    std::string key;
    bool nulls = false;

    for (std::size_t c : index.columns()) {
        const Value& v = row[c];
        if (v.is_null()) nulls = true;
        encode_key_part(key, v);
    }

    if (has_null) *has_null = nulls;
    return key;
}

std::string Table::index_key(const Index& index, RowId slot, bool* has_null) const {
    std::string key;
    bool nulls = false;

    for (std::size_t c : index.columns()) {
        const Value v = m_columns[c].get(slot);
        if (v.is_null()) nulls = true;
        encode_key_part(key, v);
    }

    if (has_null) *has_null = nulls;
    return key;
}

Row Table::prepare_row(const Row& values) {
    if (values.size() > m_schema.size()) throw DatabaseError("too many values for table '" + m_name + "'");
    Row out(m_schema.size());

    for (std::size_t i = 0; i < m_schema.size(); ++i) {
        const ColumnDef& c = m_schema[i];
        const bool provided = i < values.size();
        Value v = provided ? m_schema.coerce(i, values[i]) : Value();

        if (!provided && c.default_value) v = m_schema.coerce(i, *c.default_value);

        if (c.auto_increment && v.is_null()) v = Value(m_auto[i]);

        if (v.is_null() && !c.nullable) throw ConstraintError("NOT NULL constraint failed: " + m_name + "." + c.name);
        out[i] = std::move(v);
    }

    return out;
}

Row Table::prepare_row(const NamedValues& values) {
    Row positional(m_schema.size());
    std::vector<bool> given(m_schema.size(), false);

    for (const auto& kv : values) {
        const std::size_t i = m_schema.index_of(kv.first);
        if (given[i]) throw DatabaseError("column '" + kv.first + "' given twice");
        given[i] = true;
        positional[i] = kv.second;
    }

    Row out(m_schema.size());

    for (std::size_t i = 0; i < m_schema.size(); ++i) {
        const ColumnDef& c = m_schema[i];
        Value v = given[i] ? m_schema.coerce(i, positional[i]) : (c.default_value ? m_schema.coerce(i, *c.default_value) : Value());

        if (c.auto_increment && v.is_null()) v = Value(m_auto[i]);

        if (v.is_null() && !c.nullable) throw ConstraintError("NOT NULL constraint failed: " + m_name + "." + c.name);
        out[i] = std::move(v);
    }

    return out;
}

void Table::check_unique(const Row& prepared, RowId except) const {
    for (const auto& idx : m_indexes) {
        if (!idx->unique()) continue;
        bool has_null = false;
        const std::string key = index_key(*idx, prepared, &has_null);

        if (!has_null && idx->would_conflict(key, except)) {
            std::string cols;
            for (std::size_t c : idx->columns()) cols += (cols.empty() ? "" : ", ") + m_name + "." + m_schema[c].name;
            throw ConstraintError("UNIQUE constraint failed: " + cols);
        }
    }
}

void Table::write_row(RowId slot, const Row& prepared) {
    for (std::size_t i = 0; i < m_columns.size(); ++i) m_columns[i].set(slot, prepared[i]);

    for (auto& a : m_auto) {
        const Value& v = prepared[a.first];
        if (v.is_int() && v.get_int() >= a.second && v.get_int() < std::numeric_limits<std::int64_t>::max()) a.second = v.get_int() + 1;
    }
}

void Table::add_to_indexes(RowId slot) {
    for (auto& idx : m_indexes) idx->insert(index_key(*idx, slot), slot);
}

void Table::remove_from_indexes(RowId slot) {
    for (auto& idx : m_indexes) idx->erase(index_key(*idx, slot), slot);
}

RowId Table::insert(const Row& values) {
    const Row prepared = prepare_row(values);
    check_unique(prepared, kNoRow);
    const RowId slot = allocate();
    write_row(slot, prepared);
    add_to_indexes(slot);
    m_live.set(slot);
    ++m_live_count;
    ++m_version;
    return slot;
}

RowId Table::insert(const NamedValues& values) {
    const Row prepared = prepare_row(values);
    check_unique(prepared, kNoRow);
    const RowId slot = allocate();
    write_row(slot, prepared);
    add_to_indexes(slot);
    m_live.set(slot);
    ++m_live_count;
    ++m_version;
    return slot;
}

RowId Table::insert_at(RowId slot, const Row& values) {
    if (slot == kNoRow) throw DatabaseError("invalid slot");
    if (live(slot)) throw DatabaseError("slot already in use in table '" + m_name + "'");
    const Row prepared = prepare_row(values);
    check_unique(prepared, kNoRow);

    if (slot >= m_slot_count) {
        ensure_slots(static_cast<std::size_t>(slot) + 1);

        for (std::size_t s = m_slot_count; s < slot; ++s) m_free.push_back(static_cast<RowId>(s));
        m_slot_count = static_cast<std::size_t>(slot) + 1;
    } else {
        auto it = std::find(m_free.begin(), m_free.end(), slot);
        if (it != m_free.end()) m_free.erase(it);
    }

    write_row(slot, prepared);
    add_to_indexes(slot);
    m_live.set(slot);
    ++m_live_count;
    ++m_version;
    return slot;
}

bool Table::erase(RowId slot) {
    if (!live(slot)) return false;
    remove_from_indexes(slot);
    for (auto& c : m_columns) c.clear_slot(slot);
    m_live.reset(slot);
    m_free.push_back(slot);
    --m_live_count;
    ++m_version;
    return true;
}

bool Table::update(RowId slot, const Assignments& changes) {
    if (!live(slot)) return false;
    if (changes.empty()) return true;
    Row next = row(slot);
    std::vector<bool> changed(m_schema.size(), false);

    for (const auto& ch : changes) {
        if (ch.first >= m_schema.size()) throw DatabaseError("column index out of range");
        const ColumnDef& c = m_schema[ch.first];
        Value v = m_schema.coerce(ch.first, ch.second);
        if (v.is_null() && !c.nullable) throw ConstraintError("NOT NULL constraint failed: " + m_name + "." + c.name);
        next[ch.first] = std::move(v);
        changed[ch.first] = true;
    }

    std::vector<Index*> touched;

    for (auto& idx : m_indexes) {
        for (std::size_t c : idx->columns()) {
            if (changed[c]) { touched.push_back(idx.get()); break; }
        }
    }

    for (Index* idx : touched) {
        if (!idx->unique()) continue;
        bool has_null = false;
        const std::string key = index_key(*idx, next, &has_null);

        if (!has_null && idx->would_conflict(key, slot)) {
            std::string cols;
            for (std::size_t c : idx->columns()) cols += (cols.empty() ? "" : ", ") + m_name + "." + m_schema[c].name;
            throw ConstraintError("UNIQUE constraint failed: " + cols);
        }
    }

    for (Index* idx : touched) idx->erase(index_key(*idx, slot), slot);

    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        if (changed[i]) m_columns[i].set(slot, next[i]);
    }

    for (Index* idx : touched) idx->insert(index_key(*idx, slot), slot);

    for (auto& a : m_auto) {
        const Value& v = next[a.first];
        if (changed[a.first] && v.is_int() && v.get_int() >= a.second && v.get_int() < std::numeric_limits<std::int64_t>::max()) a.second = v.get_int() + 1;
    }

    ++m_version;
    return true;
}

bool Table::update(RowId slot, const NamedValues& changes) {
    Assignments a;
    for (const auto& kv : changes) a.emplace_back(m_schema.index_of(kv.first), kv.second);
    return update(slot, a);
}

Row Table::row(RowId slot) const {
    Row out;
    out.reserve(m_columns.size());
    for (const auto& c : m_columns) out.push_back(c.get(slot));
    return out;
}

Value Table::get(RowId slot, std::size_t column) const {
    return m_columns.at(column).get(slot);
}

Index& Table::create_index(IndexDef def) {
    if (def.name.empty()) {
        def.name = "ix_" + m_name;
        for (const auto& c : def.columns) def.name += "_" + c;
    }

    if (!valid_identifier(def.name)) throw DatabaseError("invalid index name '" + def.name + "'");
    if (index(def.name)) throw DatabaseError("index '" + def.name + "' already exists");
    if (def.columns.empty()) throw DatabaseError("index '" + def.name + "' needs at least one column");
    std::vector<std::size_t> cols = resolve_columns(def.columns);
    std::set<std::size_t> seen(cols.begin(), cols.end());
    if (seen.size() != cols.size()) throw DatabaseError("index '" + def.name + "' repeats a column");

    auto idx = make_index(def, cols);
    std::vector<std::pair<std::string, RowId>> entries;
    entries.reserve(m_live_count);
    std::vector<std::string> unique_keys;

    m_live.for_each([&](RowId slot) {
        bool has_null = false;
        std::string key = index_key(*idx, slot, &has_null);
        if (def.unique && !has_null) unique_keys.push_back(key);
        entries.emplace_back(std::move(key), slot);
    });

    if (def.unique) {
        std::sort(unique_keys.begin(), unique_keys.end());
        if (std::adjacent_find(unique_keys.begin(), unique_keys.end()) != unique_keys.end()) throw ConstraintError("cannot create unique index '" + def.name + "': duplicate values exist");
    }

    idx->bulk_load(std::move(entries));
    m_indexes.push_back(std::move(idx));
    ++m_schema_version;
    return *m_indexes.back();
}

bool Table::drop_index(const std::string& name) {
    for (auto it = m_indexes.begin(); it != m_indexes.end(); ++it) {
        if ((*it)->name() == name) {
            if (it->get() == m_primary) throw DatabaseError("cannot drop the primary key index");
            m_indexes.erase(it);
            ++m_schema_version;
            return true;
        }
    }

    return false;
}

Index* Table::index(const std::string& name) noexcept {
    for (auto& i : m_indexes) if (i->name() == name) return i.get();
    return nullptr;
}

const Index* Table::index(const std::string& name) const noexcept {
    for (const auto& i : m_indexes) if (i->name() == name) return i.get();
    return nullptr;
}

const Index* Table::best_index_for(std::size_t column, bool need_order) const noexcept {
    const Index* best = nullptr;
    int best_score = -1;

    for (const auto& i : m_indexes) {
        if (i->columns().front() != column) continue;
        if (need_order && !i->ordered()) continue;
        int score = 10;
        if (i->columns().size() == 1) score += 5;
        if (!need_order && i->kind() == IndexKind::Hash) score += 3;
        if (i->unique()) score += 2;
        if (score > best_score) { best = i.get(); best_score = score; }
    }

    return best;
}

RowId Table::find_by_key(const std::vector<Value>& key) const {
    if (!m_primary) throw DatabaseError("table '" + m_name + "' has no primary key");
    if (key.size() != m_primary->columns().size()) throw DatabaseError("primary key has " + std::to_string(m_primary->columns().size()) + " columns");
    std::string k;

    for (std::size_t i = 0; i < key.size(); ++i) {
        encode_key_part(k, m_schema.coerce(m_primary->columns()[i], key[i]));
    }

    return m_primary->first(k);
}

std::int64_t Table::next_auto_value(std::size_t column) const {
    const auto it = m_auto.find(column);
    return it == m_auto.end() ? 0 : it->second;
}

void Table::set_auto_value(std::size_t column, std::int64_t next) {
    if (m_auto.count(column)) m_auto[column] = next;
}

void Table::maintain() {
    for (auto& c : m_columns) {
        if (c.has_zones() && c.stale_writes() > m_slot_count / 4 + 64) c.rebuild_zones(m_live);
    }
}

void Table::clear() {
    for (auto& c : m_columns) c = ColumnData(c.type());
    m_live = Bitmap();
    m_free.clear();
    m_slot_count = 0;
    m_capacity = 0;
    m_live_count = 0;
    for (auto& i : m_indexes) i->clear();
    ++m_version;
}

TableStats Table::stats() const {
    TableStats s;
    s.rows = m_live_count;
    s.slots = m_slot_count;
    s.free_slots = m_free.size();
    s.indexes = m_indexes.size();
    s.version = m_version;
    for (const auto& c : m_columns) s.memory_bytes += c.memory_bytes();
    for (const auto& i : m_indexes) s.memory_bytes += i->memory_bytes();
    return s;
}

} // namespace database
} // namespace fizmo
