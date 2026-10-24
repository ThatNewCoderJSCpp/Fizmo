#include "fizmo_library.hpp"
#include "partition.hpp"
#include "storage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fizmo {
namespace database {

namespace {

std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

std::int64_t floor_mod(std::int64_t a, std::int64_t b) noexcept {
    return a - floor_div(a, b) * b;
}

std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) noexcept {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

void civil_from_days(std::int64_t z, std::int64_t& y, unsigned& m, unsigned& d) noexcept {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
}

std::int64_t month_start(std::int64_t month_index, std::int64_t per_second) {
    const std::int64_t y = 1970 + floor_div(month_index, 12);
    const unsigned m = static_cast<unsigned>(floor_mod(month_index, 12)) + 1;
    return days_from_civil(y, m, 1) * 86400 * per_second;
}

std::int64_t month_index_of(std::int64_t key, std::int64_t per_second) {
    const std::int64_t days = floor_div(floor_div(key, per_second), 86400);
    std::int64_t y = 0;
    unsigned m = 0, d = 0;
    civil_from_days(days, y, m, d);
    return (y - 1970) * 12 + static_cast<std::int64_t>(m) - 1;
}

std::string format_time(std::int64_t seconds, int parts) {
    const std::int64_t days = floor_div(seconds, 86400);
    const std::int64_t rem = seconds - days * 86400;
    std::int64_t y = 0;
    unsigned m = 0, d = 0;
    civil_from_days(days, y, m, d);
    char buf[64];

    switch (parts) {
        case 1: std::snprintf(buf, sizeof(buf), "%04lld", static_cast<long long>(y)); break;
        case 2: std::snprintf(buf, sizeof(buf), "%04lld-%02u", static_cast<long long>(y), m); break;
        case 3: std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u", static_cast<long long>(y), m, d); break;
        case 4: std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02uT%02lld", static_cast<long long>(y), m, d, static_cast<long long>(rem / 3600)); break;
        case 5: std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02uT%02lld:%02lld", static_cast<long long>(y), m, d, static_cast<long long>(rem / 3600), static_cast<long long>(rem % 3600 / 60)); break;
        default: std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02uT%02lld:%02lld:%02lld", static_cast<long long>(y), m, d, static_cast<long long>(rem / 3600), static_cast<long long>(rem % 3600 / 60), static_cast<long long>(rem % 60)); break;
    }

    return buf;
}

Value subtract(const Value& a, const Value& b) {
    if (a.is_int() && b.is_int()) {
        const std::int64_t x = a.get_int(), y = b.get_int();
        if (y > 0 && x < std::numeric_limits<std::int64_t>::min() + y) return Value(std::numeric_limits<std::int64_t>::min());
        if (y < 0 && x > std::numeric_limits<std::int64_t>::max() + y) return Value(std::numeric_limits<std::int64_t>::max());
        return Value(x - y);
    }

    return Value(a.as_real() - b.as_real());
}

RetentionAnchor effective_anchor(const PartitionOptions& o) noexcept {
    if (o.anchor != RetentionAnchor::Auto) return o.anchor;
    return o.unit == TimeUnit::None ? RetentionAnchor::Newest : RetentionAnchor::Clock;
}

} // namespace

const char* to_string(PartitionKind kind) noexcept {
    return kind == PartitionKind::List ? "LIST" : "RANGE";
}

const char* to_string(TimeUnit unit) noexcept {
    switch (unit) {
        case TimeUnit::Seconds:      return "seconds";
        case TimeUnit::Milliseconds: return "milliseconds";
        case TimeUnit::Microseconds: return "microseconds";
        default:                     return "none";
    }
}

const char* to_string(DropReason reason) noexcept {
    switch (reason) {
        case DropReason::Age:   return "age";
        case DropReason::Count: return "count";
        case DropReason::Rows:  return "rows";
        case DropReason::Bytes: return "bytes";
        case DropReason::Empty: return "empty";
        default:                return "manual";
    }
}

std::int64_t units_per_second(TimeUnit unit) noexcept {
    switch (unit) {
        case TimeUnit::Milliseconds: return 1000;
        case TimeUnit::Microseconds: return 1000000;
        default:                     return 1;
    }
}

std::int64_t wall_clock_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

PartitionOptions PartitionOptions::range(std::string column, Value width, Value origin) {
    PartitionOptions o;
    o.column = std::move(column);
    o.kind = PartitionKind::Range;
    o.width = std::move(width);
    o.origin = std::move(origin);
    return o;
}

PartitionOptions PartitionOptions::list(std::string column) {
    PartitionOptions o;
    o.column = std::move(column);
    o.kind = PartitionKind::List;
    return o;
}

PartitionOptions PartitionOptions::hourly(std::string column, TimeUnit unit) {
    PartitionOptions o = range(std::move(column), Value(3600 * units_per_second(unit)));
    o.unit = unit;
    return o;
}

PartitionOptions PartitionOptions::daily(std::string column, TimeUnit unit) {
    PartitionOptions o = range(std::move(column), Value(86400 * units_per_second(unit)));
    o.unit = unit;
    return o;
}

PartitionOptions PartitionOptions::weekly(std::string column, TimeUnit unit) {
    PartitionOptions o = range(std::move(column), Value(7 * 86400 * units_per_second(unit)), Value(4 * 86400 * units_per_second(unit)));
    o.unit = unit;
    return o;
}

PartitionOptions PartitionOptions::monthly(std::string column, TimeUnit unit, std::uint32_t months) {
    PartitionOptions o = range(std::move(column), Value(1));
    o.unit = unit;
    o.months = months;
    return o;
}

PartitionOptions PartitionOptions::yearly(std::string column, TimeUnit unit) {
    return monthly(std::move(column), unit, 12);
}

PartitionOptions& PartitionOptions::keep_for(Value span) { retention = std::move(span); return *this; }
PartitionOptions& PartitionOptions::keep_forever() { retention.reset(); return *this; }
PartitionOptions& PartitionOptions::keep_partitions(std::size_t n) { max_partitions = n; return *this; }
PartitionOptions& PartitionOptions::keep_rows(std::size_t n) { max_rows = n; return *this; }
PartitionOptions& PartitionOptions::keep_bytes(std::size_t n) { max_bytes = n; return *this; }
PartitionOptions& PartitionOptions::relative_to_newest(bool on) { anchor = on ? RetentionAnchor::Newest : RetentionAnchor::Auto; return *this; }
PartitionOptions& PartitionOptions::relative_to_clock() { anchor = RetentionAnchor::Clock; return *this; }
PartitionOptions& PartitionOptions::reject_late_rows() { late_rows = LateRowPolicy::Reject; return *this; }
PartitionOptions& PartitionOptions::ignore_late_rows() { late_rows = LateRowPolicy::Ignore; return *this; }
PartitionOptions& PartitionOptions::drop_when_empty(bool on) { drop_empty = on; return *this; }
PartitionOptions& PartitionOptions::when_dropped(DropHook hook) { on_drop = std::move(hook); return *this; }
PartitionOptions& PartitionOptions::use_clock(Clock c) { clock = std::move(c); return *this; }
PartitionOptions& PartitionOptions::starting_at(Value o) { origin = std::move(o); return *this; }

bool PartitionOptions::uses_clock() const noexcept {
    return retention.has_value() && effective_anchor(*this) == RetentionAnchor::Clock;
}

bool PartitionOptions::has_limits() const noexcept {
    return retention.has_value() || max_partitions || max_rows || max_bytes || drop_empty;
}

void write_partition_options(ByteWriter& w, const PartitionOptions& o) {
    w.u8(static_cast<std::uint8_t>(o.kind));
    w.str(o.column);
    w.value(o.width);
    w.value(o.origin);
    w.u32(o.months);
    w.u8(static_cast<std::uint8_t>(o.unit));
    w.u8(o.retention ? 1 : 0);
    w.value(o.retention ? *o.retention : Value());
    w.u8(static_cast<std::uint8_t>(o.anchor));
    w.u64(o.max_partitions);
    w.u64(o.max_rows);
    w.u64(o.max_bytes);
    w.u8(static_cast<std::uint8_t>(o.late_rows));
    w.u8(o.drop_empty ? 1 : 0);
}

PartitionOptions read_partition_options(ByteReader& r) {
    PartitionOptions o;
    o.kind = static_cast<PartitionKind>(r.u8());
    o.column = r.str();
    o.width = r.value();
    o.origin = r.value();
    o.months = r.u32();
    o.unit = static_cast<TimeUnit>(r.u8());
    const bool has = r.u8() != 0;
    Value ret = r.value();
    if (has) o.retention = std::move(ret);
    o.anchor = static_cast<RetentionAnchor>(r.u8());
    o.max_partitions = static_cast<std::size_t>(r.u64());
    o.max_rows = static_cast<std::size_t>(r.u64());
    o.max_bytes = static_cast<std::size_t>(r.u64());
    o.late_rows = static_cast<LateRowPolicy>(r.u8());
    o.drop_empty = r.u8() != 0;
    return o;
}

PartitionedTable::PartitionedTable(std::string name, Schema schema, PartitionOptions options)
    : m_name(std::move(name)), m_template(std::make_unique<Table>(m_name, std::move(schema))) {
    m_uid = m_template->uid();
    validate_options(options);
    m_options = std::move(options);
}

void PartitionedTable::validate_options(PartitionOptions& o) {
    const auto col = m_template->schema().find(o.column);
    if (!col) throw DatabaseError("partition column '" + o.column + "' is not in table '" + m_name + "'");
    m_key = *col;
    const ColumnType type = m_template->schema()[*col].type;
    PartitionOptions& w = o;

    if (o.kind == PartitionKind::Range) {
        if (type != ColumnType::Integer && type != ColumnType::Real) throw DatabaseError("range partitions need an INTEGER or REAL column; '" + o.column + "' is " + to_string(type));

        if (o.months) {
            if (type != ColumnType::Integer) throw DatabaseError("monthly partitions need an INTEGER time column");
            if (o.unit == TimeUnit::None) throw DatabaseError("monthly partitions need a time unit");
        } else {
            if (!o.width.is_number() || o.width.as_real() <= 0) throw DatabaseError("partition width must be a positive number");
            w.width = coerce_to(type, o.width, o.column);
            if (type == ColumnType::Integer && w.width.get_int() <= 0) throw DatabaseError("partition width must be a positive whole number");
            if (!o.origin.is_number()) throw DatabaseError("partition origin must be a number");
            w.origin = coerce_to(type, o.origin, o.column);
        }
    } else {
        if (o.months) throw DatabaseError("list partitions don't take a month count");
        if (o.retention) throw DatabaseError("keep_for needs range partitions; list partitions can use keep_partitions, keep_rows or keep_bytes");
    }

    if (o.retention) {
        if (!o.retention->is_number() || o.retention->as_real() <= 0) throw DatabaseError("keep_for needs a positive span");
        w.retention = coerce_to(type, *o.retention, o.column);
    }

    if (o.anchor == RetentionAnchor::Clock && o.unit == TimeUnit::None && !o.clock) throw DatabaseError("retention by the clock needs a time unit or a clock");
}

void PartitionedTable::set_options(PartitionOptions o) {
    if (o.column != m_options.column || o.kind != m_options.kind || o.months != m_options.months || o.unit != m_options.unit) throw DatabaseError("the partition column, kind and unit of '" + m_name + "' can't change");
    PartitionOptions check = o;
    validate_options(check);
    if (m_options.kind == PartitionKind::Range && !m_options.months && (compare(check.width, m_options.width) != 0 || compare(check.origin, m_options.origin) != 0)) throw DatabaseError("the partition width of '" + m_name + "' can't change");
    m_options = std::move(check);
    ++m_layout;
}

std::string PartitionedTable::key_for(const Value& key) const {
    if (!by_range()) return encode_key(key);
    Value lo, hi;
    bounds_for(key, lo, hi);
    return encode_key(lo);
}

void PartitionedTable::bounds_for(const Value& key, Value& lo, Value& hi) const {
    const ColumnType type = schema()[m_key].type;

    if (m_options.months) {
        const std::int64_t per = units_per_second(m_options.unit);
        const std::int64_t mi = month_index_of(key.as_int(), per);
        const std::int64_t m = static_cast<std::int64_t>(m_options.months);
        const std::int64_t start = floor_div(mi, m) * m;
        lo = Value(month_start(start, per));
        hi = Value(month_start(start + m, per));
        return;
    }

    if (type == ColumnType::Integer) {
        const std::int64_t k = key.as_int();
        const std::int64_t w = m_options.width.get_int();
        const std::int64_t o = m_options.origin.get_int();
        if ((o > 0 && k < std::numeric_limits<std::int64_t>::min() + o) || (o < 0 && k > std::numeric_limits<std::int64_t>::max() + o)) throw DatabaseError("partition key is out of range");
        const std::int64_t q = floor_div(k - o, w);
        const std::int64_t l = q * w + o;
        lo = Value(l);
        hi = l <= std::numeric_limits<std::int64_t>::max() - w ? Value(l + w) : Value();
        return;
    }

    const double k = key.as_real();
    if (std::isnan(k) || std::isinf(k)) throw DatabaseError("partition key must be a finite number");
    const double w = m_options.width.as_real();
    const double o = m_options.origin.as_real();
    double q = std::floor((k - o) / w);
    double l = o + q * w;
    if (k < l) { q -= 1; l = o + q * w; }
    if (k >= l + w) { q += 1; l = o + q * w; }
    lo = Value(l);
    hi = Value(l + w);
}

std::string PartitionedTable::label_for(const Value& lo, const Value& hi, const Value& value) const {
    if (!by_range()) return value.to_string();

    if (m_options.unit != TimeUnit::None) {
        const std::int64_t per = units_per_second(m_options.unit);
        const std::int64_t start = lo.is_int() ? lo.get_int() : static_cast<std::int64_t>(std::floor(lo.as_real()));
        const std::int64_t secs = floor_div(start, per);

        if (m_options.months) return format_time(secs, m_options.months % 12 == 0 ? 1 : 2);

        const std::int64_t width = m_options.width.is_int() ? m_options.width.get_int() : static_cast<std::int64_t>(m_options.width.as_real());

        if (width % per == 0 && floor_mod(start, per) == 0) {
            const std::int64_t ws = width / per;
            if (ws % 86400 == 0 && floor_mod(secs, 86400) == 0) return format_time(secs, 3);
            if (ws % 3600 == 0 && floor_mod(secs, 3600) == 0) return format_time(secs, 4);
            if (ws % 60 == 0 && floor_mod(secs, 60) == 0) return format_time(secs, 5);
            return format_time(secs, 6);
        }
    }

    return "[" + lo.to_string() + ", " + (hi.is_null() ? std::string("max") : hi.to_string()) + ")";
}

std::optional<std::size_t> PartitionedTable::find(const Value& key) const {
    const auto it = m_lookup.find(key_for(key));
    if (it == m_lookup.end()) return std::nullopt;
    return it->second;
}

std::optional<std::size_t> PartitionedTable::find_child(const std::string& table) const {
    for (std::size_t i = 0; i < m_parts.size(); ++i) if (m_parts[i].table->name() == table) return i;
    return std::nullopt;
}

std::optional<std::size_t> PartitionedTable::find_named(const std::string& name) const {
    for (std::size_t i = 0; i < m_parts.size(); ++i) if (m_parts[i].label == name || m_parts[i].table->name() == name) return i;
    return std::nullopt;
}

PartitionPart PartitionedTable::make_part(const Value& key, std::uint64_t seq, std::string table) const {
    PartitionPart p;
    p.seq = seq;

    if (by_range()) {
        bounds_for(key, p.lo, p.hi);
        p.key = encode_key(p.lo);
    } else {
        p.value = key;
        p.key = encode_key(key);
    }

    p.label = label_for(p.lo, p.hi, p.value);
    p.created_ms = wall_clock_ms();
    p.table = std::make_unique<Table>(std::move(table), schema());
    for (const auto& d : m_indexes) p.table->create_index(d);
    return p;
}

PartitionPart PartitionedTable::restore_part(std::string table, std::uint64_t seq, std::string label, Value lo, Value hi, Value value, std::int64_t created_ms, bool with_indexes) const {
    PartitionPart p;
    p.seq = seq;
    p.label = std::move(label);
    p.lo = std::move(lo);
    p.hi = std::move(hi);
    p.value = std::move(value);
    p.key = by_range() ? encode_key(p.lo) : encode_key(p.value);
    p.created_ms = created_ms;
    p.table = std::make_unique<Table>(std::move(table), schema());
    if (with_indexes) for (const auto& d : m_indexes) p.table->create_index(d);
    return p;
}

std::size_t PartitionedTable::attach(PartitionPart part) {
    reserve_seq(part.seq);
    std::size_t at = m_parts.size();

    for (std::size_t i = 0; i < m_parts.size(); ++i) {
        const bool before = by_range() ? part.key < m_parts[i].key : part.seq < m_parts[i].seq;
        if (before) { at = i; break; }
    }

    m_parts.insert(m_parts.begin() + static_cast<std::ptrdiff_t>(at), std::move(part));
    rebuild_lookup();
    ++m_layout;
    return at;
}

PartitionPart PartitionedTable::detach(std::size_t index) {
    PartitionPart p = std::move(m_parts.at(index));
    m_parts.erase(m_parts.begin() + static_cast<std::ptrdiff_t>(index));
    rebuild_lookup();
    ++m_layout;
    return p;
}

void PartitionedTable::rebuild_lookup() {
    m_lookup.clear();
    for (std::size_t i = 0; i < m_parts.size(); ++i) m_lookup[m_parts[i].key] = i;
}

std::int64_t PartitionedTable::now() const {
    if (m_options.clock) return m_options.clock();
    const auto t = std::chrono::system_clock::now().time_since_epoch();

    switch (m_options.unit) {
        case TimeUnit::Milliseconds: return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
        case TimeUnit::Microseconds: return std::chrono::duration_cast<std::chrono::microseconds>(t).count();
        default:                     return std::chrono::duration_cast<std::chrono::seconds>(t).count();
    }
}

std::optional<Value> PartitionedTable::cutoff(const Value* incoming) const {
    if (!m_options.retention || !by_range()) return std::nullopt;
    const bool real = schema()[m_key].type == ColumnType::Real;
    Value base;

    if (effective_anchor(m_options) == RetentionAnchor::Clock) {
        const std::int64_t n = now();
        base = real ? Value(static_cast<double>(n)) : Value(n);
    } else {
        base = m_newest;
        if (incoming && !incoming->is_null() && (base.is_null() || compare(*incoming, base) > 0)) base = *incoming;
        if (base.is_null()) return std::nullopt;
    }

    return subtract(base, *m_options.retention);
}

bool PartitionedTable::expired(const PartitionPart& part, const std::optional<Value>& cut) const {
    return cut && !part.hi.is_null() && compare(part.hi, *cut) <= 0;
}

bool PartitionedTable::is_late(const Value& key) const {
    const auto cut = cutoff(&key);
    if (!cut) return false;
    Value lo, hi;
    bounds_for(key, lo, hi);
    return !hi.is_null() && compare(hi, *cut) <= 0;
}

std::vector<std::size_t> PartitionedTable::visible() const {
    const auto cut = cutoff();
    std::vector<std::size_t> out;
    out.reserve(m_parts.size());
    for (std::size_t i = 0; i < m_parts.size(); ++i) if (!expired(m_parts[i], cut)) out.push_back(i);
    return out;
}

void PartitionedTable::observe(const Row& row) {
    const Value& k = row[m_key];
    if (by_range() && !k.is_null() && (m_newest.is_null() || compare(k, m_newest) > 0)) m_newest = k;

    for (const auto& a : m_template->auto_values()) {
        const Value& v = row[a.first];
        if (v.is_int() && v.get_int() >= a.second && v.get_int() < std::numeric_limits<std::int64_t>::max()) m_template->set_auto_value(a.first, v.get_int() + 1);
    }
}

void PartitionedTable::recompute_newest() {
    m_newest = Value();
    if (!by_range()) return;

    for (auto it = m_parts.rbegin(); it != m_parts.rend(); ++it) {
        const Table& t = *it->table;
        if (t.size() == 0) continue;
        t.for_each([&](RowId r) {
            const Value v = t.get(r, m_key);
            if (!v.is_null() && (m_newest.is_null() || compare(v, m_newest) > 0)) m_newest = v;
        });
        return;
    }
}

std::size_t PartitionedTable::rows() const noexcept {
    std::size_t n = 0;
    for (const auto& p : m_parts) n += p.table->size();
    return n;
}

std::size_t PartitionedTable::memory_bytes() const noexcept {
    std::size_t n = 0;
    for (const auto& p : m_parts) n += p.table->stats().memory_bytes;
    return n;
}

PartitionInfo PartitionedTable::info(const PartitionPart& part, DropReason reason) const {
    PartitionInfo i;
    i.name = part.label;
    i.table = part.table->name();
    i.parent = m_name;
    i.expired = expired(part, cutoff());
    i.lo = part.lo;
    i.hi = part.hi;
    i.value = part.value;
    i.rows = part.table->size();
    i.memory_bytes = part.table->stats().memory_bytes;
    i.created_ms = part.created_ms;
    i.reason = reason;
    return i;
}

TableStats PartitionedTable::stats() const {
    TableStats s;
    s.indexes = m_template->indexes().size();
    s.version = m_layout;

    for (const auto& p : m_parts) {
        const TableStats t = p.table->stats();
        s.rows += t.rows;
        s.slots += t.slots;
        s.free_slots += t.free_slots;
        s.memory_bytes += t.memory_bytes;
        s.version += t.version;
    }

    return s;
}

} // namespace database
} // namespace fizmo
