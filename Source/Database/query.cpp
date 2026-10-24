#include "fizmo_library.hpp"
#include "query.hpp"
#include "../Collections/hash_map.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>

namespace fizmo {
namespace database {

const char* to_string(Aggregate agg) noexcept {
    switch (agg) {
        case Aggregate::CountAll:      return "COUNT(*)";
        case Aggregate::Count:         return "COUNT";
        case Aggregate::CountDistinct: return "COUNT DISTINCT";
        case Aggregate::Sum:           return "SUM";
        case Aggregate::Avg:           return "AVG";
        case Aggregate::Min:           return "MIN";
        case Aggregate::Max:           return "MAX";
        default:                       return "";
    }
}

Query::Query(std::string table, std::string alias) : m_table(std::move(table)), m_alias(std::move(alias)) {}

Query& Query::as(std::string alias) { m_alias = std::move(alias); return *this; }
Query& Query::where(Expr condition) { m_where = m_where && condition; return *this; }

Query& Query::join(std::string table, Expr left_column, Expr right_column, JoinKind kind, std::string alias) {
    if (m_joins.size() + 1 >= 8) throw DatabaseError("a query can join at most 7 tables");
    m_joins.push_back(JoinSpec{ std::move(table), std::move(alias), std::move(left_column), std::move(right_column), kind });
    return *this;
}

Query& Query::left_join(std::string table, Expr left_column, Expr right_column, std::string alias) {
    return join(std::move(table), std::move(left_column), std::move(right_column), JoinKind::Left, std::move(alias));
}

Query& Query::select(std::vector<Expr> columns) {
    for (auto& c : columns) m_select.push_back(SelectItem{ std::move(c), std::string(), Aggregate::None });
    return *this;
}

Query& Query::select(Expr expr, std::string alias) {
    m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::None });
    return *this;
}

Query& Query::select(std::initializer_list<const char*> columns) {
    for (const char* c : columns) m_select.push_back(SelectItem{ col(c), std::string(), Aggregate::None });
    return *this;
}

Query& Query::select_all() { m_select.clear(); return *this; }
Query& Query::count(std::string alias) { m_select.push_back(SelectItem{ lit(1), std::move(alias), Aggregate::CountAll }); return *this; }
Query& Query::count(Expr expr, std::string alias) { m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::Count }); return *this; }
Query& Query::count_distinct(Expr expr, std::string alias) { m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::CountDistinct }); return *this; }
Query& Query::sum(Expr expr, std::string alias) { m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::Sum }); return *this; }
Query& Query::avg(Expr expr, std::string alias) { m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::Avg }); return *this; }
Query& Query::min(Expr expr, std::string alias) { m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::Min }); return *this; }
Query& Query::max(Expr expr, std::string alias) { m_select.push_back(SelectItem{ std::move(expr), std::move(alias), Aggregate::Max }); return *this; }
Query& Query::group_by(Expr expr) { m_group.push_back(std::move(expr)); return *this; }
Query& Query::having(Expr condition) { m_having = m_having && condition; return *this; }
Query& Query::order_by(Expr expr, SortOrder order) { m_order.push_back(OrderTerm{ std::move(expr), order }); return *this; }
Query& Query::limit(std::size_t n) { m_limit = n; return *this; }
Query& Query::offset(std::size_t n) { m_offset = n; return *this; }
Query& Query::distinct(bool on) { m_distinct = on; return *this; }
Query& Query::cache(bool on) { m_cache = on; return *this; }

bool Query::has_aggregates() const noexcept {
    if (!m_group.empty()) return true;
    for (const auto& s : m_select) if (s.aggregate != Aggregate::None) return true;
    return false;
}

void Query::fingerprint(std::string& out) const {
    out += "T" + std::to_string(m_table.size()) + ":" + m_table + "|" + m_alias + "|W";
    m_where.fingerprint(out);

    for (const auto& j : m_joins) {
        out += "|J" + std::to_string(static_cast<int>(j.kind)) + std::to_string(j.table.size()) + ":" + j.table + "|" + j.alias + "|";
        j.left.fingerprint(out);
        j.right.fingerprint(out);
    }

    out += "|S";

    for (const auto& s : m_select) {
        out += std::to_string(static_cast<int>(s.aggregate)) + std::to_string(s.alias.size()) + ":" + s.alias;
        s.expr.fingerprint(out);
    }

    out += "|G";
    for (const auto& g : m_group) g.fingerprint(out);
    out += "|H";
    m_having.fingerprint(out);
    out += "|O";

    for (const auto& o : m_order) {
        out += o.order == SortOrder::Asc ? 'a' : 'd';
        o.expr.fingerprint(out);
    }

    out += "|L" + (m_limit ? std::to_string(*m_limit) : std::string("-")) + "|F" + std::to_string(m_offset) + (m_distinct ? "|D" : "");
}

std::vector<std::string> Query::referenced_tables() const {
    std::vector<std::string> out{ m_table };
    for (const auto& j : m_joins) out.push_back(j.table);
    return out;
}

std::optional<std::size_t> ResultSet::column_index(const std::string& name) const noexcept {
    for (std::size_t i = 0; i < columns.size(); ++i) if (columns[i] == name) return i;

    for (std::size_t i = 0; i < columns.size(); ++i) {
        const std::size_t dot = columns[i].rfind('.');
        if (dot != std::string::npos && columns[i].compare(dot + 1, std::string::npos, name) == 0) return i;
    }

    return std::nullopt;
}

const Value& ResultSet::at(std::size_t row, const std::string& column) const {
    const auto c = column_index(column);
    if (!c) throw DatabaseError("result has no column '" + column + "'");
    return rows.at(row).at(*c);
}

Value ResultSet::scalar() const {
    if (rows.empty() || rows[0].empty()) return Value();
    return rows[0][0];
}

std::string ResultSet::to_json() const {
    std::string out = "[";

    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (r) out += ",";
        out += "{";

        for (std::size_t c = 0; c < columns.size(); ++c) {
            if (c) out += ",";
            out += Value(columns[c]).to_json() + ":" + rows[r][c].to_json();
        }

        out += "}";
    }

    return out + "]";
}

std::string ResultSet::to_text(std::size_t max_rows) const {
    std::vector<std::size_t> width(columns.size());
    const std::size_t shown = std::min(rows.size(), max_rows);
    for (std::size_t c = 0; c < columns.size(); ++c) width[c] = columns[c].size();

    for (std::size_t r = 0; r < shown; ++r) {
        for (std::size_t c = 0; c < columns.size(); ++c) width[c] = std::max(width[c], std::min<std::size_t>(rows[r][c].to_string().size(), 40));
    }

    auto cell = [&](const std::string& s, std::size_t w) {
        std::string t = s.size() > 40 ? s.substr(0, 37) + "..." : s;
        return t + std::string(w > t.size() ? w - t.size() : 0, ' ');
    };

    std::string out;
    for (std::size_t c = 0; c < columns.size(); ++c) out += (c ? " | " : "") + cell(columns[c], width[c]);
    out += "\n";
    for (std::size_t c = 0; c < columns.size(); ++c) out += (c ? "-+-" : "") + std::string(width[c], '-');
    out += "\n";

    for (std::size_t r = 0; r < shown; ++r) {
        for (std::size_t c = 0; c < columns.size(); ++c) out += (c ? " | " : "") + cell(rows[r][c].to_string(), width[c]);
        out += "\n";
    }

    if (rows.size() > shown) out += "(" + std::to_string(rows.size() - shown) + " more rows)\n";
    return out;
}

namespace {

constexpr std::size_t kMaxSources = 8;
constexpr int kOutputSource = -2;
struct Tuple {
    std::array<RowId, kMaxSources>         row;
    std::array<std::uint32_t, kMaxSources> part;

    Tuple() noexcept { row.fill(kNoRow); part.fill(0); }
    RowId& operator[](std::size_t i) noexcept { return row[i]; }
    const RowId& operator[](std::size_t i) const noexcept { return row[i]; }
    void fill(RowId v) noexcept { row.fill(v); part.fill(0); }
};

struct Source {
    const Table*              table = nullptr;
    std::string               name;
    std::string               alias;
    std::vector<const Table*> parts;
    int                       view = -1;
};

Source single_source(const Table* t, std::string name, std::string alias) {
    Source s;
    s.table = t;
    s.name = std::move(name);
    s.alias = std::move(alias);
    s.parts.push_back(t);
    return s;
}

struct Bound {
    Op                 op = Op::Literal;
    int                src = -1;
    std::size_t        col = 0;
    Value              value;
    std::vector<Bound> args;
    std::shared_ptr<collections::HashMap<std::string, char>> set;
    bool               list_has_null = false;

    bool is_column() const noexcept { return op == Op::Column && src >= 0; }
    bool is_literal() const noexcept { return op == Op::Literal; }
};

void collect_sources(const Bound& b, std::uint32_t& mask) {
    if (b.op == Op::Column && b.src >= 0) mask |= 1u << b.src;
    for (const auto& a : b.args) collect_sources(a, mask);
}

std::uint32_t sources_of(const Bound& b) {
    std::uint32_t m = 0;
    collect_sources(b, m);
    return m;
}

class Binder {
public:
    Binder(const std::vector<Source>& sources, const std::vector<Value>& params) : m_sources(sources), m_params(params) {}

    void set_output(const std::vector<std::string>* names) { m_output = names; }

    Bound bind(const ExprPtr& e) const {
        if (!e) throw DatabaseError("empty expression");
        Bound b;
        b.op = e->op;

        switch (e->op) {
            case Op::Literal:
                b.value = e->value;
                return b;
            case Op::Param:
                if (e->param >= m_params.size()) throw DatabaseError("missing value for parameter ?" + std::to_string(e->param));
                b.op = Op::Literal;
                b.value = m_params[e->param];
                return b;
            case Op::Column:
                resolve(e->name, b);
                return b;
            default:
                break;
        }

        for (const auto& a : e->args) b.args.push_back(bind(a));

        if ((b.op == Op::In || b.op == Op::NotIn) && b.args.size() > 9) {
            bool all_literal = true;
            for (std::size_t i = 1; i < b.args.size(); ++i) if (!b.args[i].is_literal()) { all_literal = false; break; }

            if (all_literal) {
                b.set = std::make_shared<collections::HashMap<std::string, char>>();
                b.set->reserve(b.args.size());

                for (std::size_t i = 1; i < b.args.size(); ++i) {
                    const Value& v = b.args[i].value;
                    if (v.is_null()) { b.list_has_null = true; continue; }
                    b.set->insert_or_assign(set_key(v), 1);
                }
            }
        }

        return b;
    }

    static std::string set_key(const Value& v) {
        if (v.is_real()) {
            const double d = v.get_real();
            if (std::trunc(d) == d && d >= -9007199254740992.0 && d <= 9007199254740992.0) return encode_key(Value(static_cast<std::int64_t>(d)));
        }

        if (v.is_bool()) return encode_key(Value(v.get_bool() ? 1 : 0));
        return encode_key(v);
    }

private:
    const std::vector<Source>&       m_sources;
    const std::vector<Value>&        m_params;
    const std::vector<std::string>*  m_output = nullptr;

    void resolve(const std::string& name, Bound& b) const {
        if (m_output) {
            for (std::size_t i = 0; i < m_output->size(); ++i) {
                if ((*m_output)[i] == name) { b.src = kOutputSource; b.col = i; return; }
            }
        }

        const std::size_t dot = name.find('.');

        if (dot != std::string::npos) {
            const std::string qual = name.substr(0, dot), cname = name.substr(dot + 1);

            for (std::size_t s = 0; s < m_sources.size(); ++s) {
                const Source& src = m_sources[s];
                if (src.alias == qual || (src.alias.empty() && src.name == qual) || src.name == qual) {
                    const auto c = src.table->schema().find(cname);
                    if (!c) throw DatabaseError("table '" + src.name + "' has no column '" + cname + "'");
                    b.src = static_cast<int>(s);
                    b.col = *c;
                    return;
                }
            }

            if (m_output) {
                for (std::size_t i = 0; i < m_output->size(); ++i) {
                    if ((*m_output)[i] == cname) { b.src = kOutputSource; b.col = i; return; }
                }
            }

            throw DatabaseError("unknown table or alias '" + qual + "' in '" + name + "'");
        }

        int found = -1;

        for (std::size_t s = 0; s < m_sources.size(); ++s) {
            const auto c = m_sources[s].table->schema().find(name);

            if (c) {
                if (found >= 0) throw DatabaseError("column '" + name + "' is ambiguous; qualify it like 'table." + name + "'");
                found = static_cast<int>(s);
                b.col = *c;
            }
        }

        if (found < 0) throw DatabaseError("no column named '" + name + "'");
        b.src = found;
    }
};

struct Ctx {
    const std::vector<Source>* sources = nullptr;
    const Tuple*               tuple = nullptr;
    const Row*                 output = nullptr;
};

const Table& table_at(const Ctx& ctx, std::size_t s) noexcept {
    return *(*ctx.sources)[s].parts[(*ctx.tuple).part[s]];
}

Value column_value(const Bound& b, const Ctx& ctx) {
    if (b.src == kOutputSource) return ctx.output ? (*ctx.output)[b.col] : Value();
    const RowId slot = (*ctx.tuple)[static_cast<std::size_t>(b.src)];
    if (slot == kNoRow) return Value();
    return table_at(ctx, static_cast<std::size_t>(b.src)).column(b.col).get(slot);
}

Value eval(const Bound& b, const Ctx& ctx);
Tri test(const Bound& b, const Ctx& ctx);

Tri column_vs_literal(Op op, const ColumnData& c, RowId slot, const Value& lit) {
    if (slot == kNoRow || c.is_null(slot) || lit.is_null()) return Tri::Unknown;
    int cmp = 0;

    switch (c.type()) {
        case ColumnType::Integer:
            if (lit.is_int()) { const auto x = c.int_at(slot), y = lit.get_int(); cmp = x < y ? -1 : (x > y ? 1 : 0); break; }
            return compare_values(op, c.get(slot), lit);
        case ColumnType::Real:
            if (lit.is_real() || lit.is_int()) {
                const double x = c.real_at(slot), y = lit.as_real();
                if (std::isnan(x) || std::isnan(y)) return compare_values(op, c.get(slot), lit);
                cmp = x < y ? -1 : (x > y ? 1 : 0);
                break;
            }
            return compare_values(op, c.get(slot), lit);
        case ColumnType::Text:
            if (lit.is_text()) { const int r = c.bytes_at(slot).compare(lit.get_text()); cmp = r < 0 ? -1 : (r > 0 ? 1 : 0); break; }
            return compare_values(op, c.get(slot), lit);
        default:
            return compare_values(op, c.get(slot), lit);
    }

    switch (op) {
        case Op::Eq: return tri_of(cmp == 0);
        case Op::Ne: return tri_of(cmp != 0);
        case Op::Lt: return tri_of(cmp < 0);
        case Op::Le: return tri_of(cmp <= 0);
        case Op::Gt: return tri_of(cmp > 0);
        default:     return tri_of(cmp >= 0);
    }
}

Tri in_list(const Bound& b, const Ctx& ctx) {
    const Value v = eval(b.args[0], ctx);
    if (v.is_null()) return Tri::Unknown;

    if (b.set) {
        if (b.set->get_ptr(Binder::set_key(v))) return Tri::True;
        return b.list_has_null ? Tri::Unknown : Tri::False;
    }

    bool unknown = false;

    for (std::size_t i = 1; i < b.args.size(); ++i) {
        const Value item = eval(b.args[i], ctx);
        if (item.is_null()) { unknown = true; continue; }
        if (compare(v, item) == 0) return Tri::True;
    }

    return unknown ? Tri::Unknown : Tri::False;
}

Tri test(const Bound& b, const Ctx& ctx) {
    switch (b.op) {
        case Op::And: {
            const Tri a = test(b.args[0], ctx);
            if (a == Tri::False) return Tri::False;
            return tri_and(a, test(b.args[1], ctx));
        }
        case Op::Or: {
            const Tri a = test(b.args[0], ctx);
            if (a == Tri::True) return Tri::True;
            return tri_or(a, test(b.args[1], ctx));
        }
        case Op::Not: return tri_not(test(b.args[0], ctx));
        case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge: {
            const Bound& l = b.args[0];
            const Bound& r = b.args[1];

            if (l.is_column() && r.is_literal()) {
                const RowId slot = (*ctx.tuple)[static_cast<std::size_t>(l.src)];
                return column_vs_literal(b.op, table_at(ctx, static_cast<std::size_t>(l.src)).column(l.col), slot, r.value);
            }

            if (r.is_column() && l.is_literal()) {
                const RowId slot = (*ctx.tuple)[static_cast<std::size_t>(r.src)];
                return column_vs_literal(flip_comparison(b.op), table_at(ctx, static_cast<std::size_t>(r.src)).column(r.col), slot, l.value);
            }

            return compare_values(b.op, eval(l, ctx), eval(r, ctx));
        }
        case Op::In: return in_list(b, ctx);
        case Op::NotIn: return tri_not(in_list(b, ctx));
        case Op::Between: {
            const Value v = eval(b.args[0], ctx);
            return tri_and(compare_values(Op::Ge, v, eval(b.args[1], ctx)), compare_values(Op::Le, v, eval(b.args[2], ctx)));
        }
        case Op::IsNull: {
            const Bound& a = b.args[0];
            if (a.is_column()) {
                const RowId slot = (*ctx.tuple)[static_cast<std::size_t>(a.src)];
                return tri_of(slot == kNoRow || table_at(ctx, static_cast<std::size_t>(a.src)).column(a.col).is_null(slot));
            }
            return tri_of(eval(a, ctx).is_null());
        }
        case Op::IsNotNull: {
            const Bound& a = b.args[0];
            if (a.is_column()) {
                const RowId slot = (*ctx.tuple)[static_cast<std::size_t>(a.src)];
                return tri_of(slot != kNoRow && !table_at(ctx, static_cast<std::size_t>(a.src)).column(a.col).is_null(slot));
            }
            return tri_of(!eval(a, ctx).is_null());
        }
        case Op::Like:
        case Op::ILike: {
            const Value pat = eval(b.args[1], ctx);
            if (pat.is_null()) return Tri::Unknown;
            const std::string& p = pat.as_text();
            const Bound& a = b.args[0];

            if (a.is_column()) {
                const ColumnData& c = table_at(ctx, static_cast<std::size_t>(a.src)).column(a.col);
                const RowId slot = (*ctx.tuple)[static_cast<std::size_t>(a.src)];
                if (slot == kNoRow || c.is_null(slot)) return Tri::Unknown;
                if (c.type() == ColumnType::Text) return tri_of(like_match(c.bytes_at(slot), p, b.op == Op::ILike));
            }

            const Value v = eval(a, ctx);
            if (v.is_null()) return Tri::Unknown;
            return tri_of(like_match(v.is_text() ? v.get_text() : v.to_string(), p, b.op == Op::ILike));
        }
        default: {
            const Value v = eval(b, ctx);
            if (v.is_null()) return Tri::Unknown;
            if (v.is_bool()) return tri_of(v.get_bool());
            if (v.is_number()) return tri_of(v.numeric() != 0.0);
            throw TypeError("expression is not a condition: " + v.to_string());
        }
    }
}

Value eval(const Bound& b, const Ctx& ctx) {
    switch (b.op) {
        case Op::Literal: return b.value;
        case Op::Column:  return column_value(b, ctx);
        case Op::Add: case Op::Sub: case Op::Mul: case Op::Div: case Op::Mod:
            return arithmetic(b.op, eval(b.args[0], ctx), eval(b.args[1], ctx));
        case Op::Neg: case Op::Abs: case Op::Lower: case Op::Upper: case Op::Length:
            return apply_function(b.op, { eval(b.args[0], ctx) });
        case Op::Coalesce: {
            for (const auto& a : b.args) {
                Value v = eval(a, ctx);
                if (!v.is_null()) return v;
            }
            return Value();
        }
        default: {
            const Tri t = test(b, ctx);
            return t == Tri::Unknown ? Value() : Value(t == Tri::True);
        }
    }
}

void flatten_and(const Bound& b, std::vector<const Bound*>& out) {
    if (b.op == Op::And) {
        flatten_and(b.args[0], out);
        flatten_and(b.args[1], out);
    } else {
        out.push_back(&b);
    }
}

std::optional<Value> exact_coerce(ColumnType type, const Value& v) {
    if (v.is_null()) return std::nullopt;

    try {
        Value c = coerce_to(type, v, std::string());
        if (compare(c, v) != 0 && !(type == ColumnType::Boolean)) return std::nullopt;
        return c;
    } catch (const TypeError&) {
        return std::nullopt;
    }
}

struct Interval {
    std::optional<Value> lo, hi;
    bool                 lo_incl = true, hi_incl = true;
    bool                 empty = false;

    void tighten_lo(const Value& v, bool incl) {
        if (!lo || compare(v, *lo) > 0 || (compare(v, *lo) == 0 && !incl)) { lo = v; lo_incl = incl; }
    }

    void tighten_hi(const Value& v, bool incl) {
        if (!hi || compare(v, *hi) < 0 || (compare(v, *hi) == 0 && !incl)) { hi = v; hi_incl = incl; }
    }

    void check() {
        if (lo && hi) {
            const int c = compare(*lo, *hi);
            if (c > 0 || (c == 0 && !(lo_incl && hi_incl))) empty = true;
        }
    }
};

bool bound_value_for(ColumnType type, const Value& v, bool is_lower, bool incl, Value& out, bool& out_incl) {
    if (v.is_null()) return false;

    if (type == ColumnType::Integer) {
        if (v.is_int()) { out = v; out_incl = incl; return true; }
        if (!v.is_real()) return false;
        const double d = v.get_real();
        if (std::isnan(d)) return false;
        if (d >= 9.2e18 || d <= -9.2e18) return false;

        if (is_lower) {
            const double c = incl ? std::ceil(d) : std::floor(d) + 1.0;
            out = Value(static_cast<std::int64_t>(c));
        } else {
            const double f = incl ? std::floor(d) : std::ceil(d) - 1.0;
            out = Value(static_cast<std::int64_t>(f));
        }

        out_incl = true;
        return true;
    }

    if (type == ColumnType::Real) {
        if (!v.is_number()) return false;
        const double d = v.as_real();
        if (std::isnan(d)) return false;
        if (v.is_int() && static_cast<std::int64_t>(d) != v.get_int()) return false;
        out = Value(d);
        out_incl = incl;
        return true;
    }

    if (type == ColumnType::Text && v.is_text()) { out = v; out_incl = incl; return true; }
    if (type == ColumnType::Blob && v.is_blob()) { out = v; out_incl = incl; return true; }
    return false;
}

struct Constraint {
    bool                     has_eq = false;
    std::vector<Value>       eq;
    std::vector<std::size_t> eq_preds;
    Interval                 range;
    bool                     has_range = false;
    std::vector<std::size_t> range_preds;
    bool                     is_null = false;
    std::size_t              null_pred = 0;
    std::string              prefix;
    bool                     has_prefix = false;
    bool                     prefix_exact = false;
    std::size_t              prefix_pred = 0;
    bool                     impossible = false;
};

enum class PathKind {
    Empty,
    FullScan,
    Points,
    Range,
    Union
};

struct AccessPath {
    PathKind                 kind = PathKind::FullScan;
    const Index*             index = nullptr;
    std::vector<std::string> keys;
    KeyBound                 lo, hi;
    double                   est = 0;
    std::vector<std::size_t> consumed;
    std::vector<AccessPath>  parts;
    bool                     reverse = false;
    bool                     ordered_output = false;
    std::string              note;
};

std::string describe_path(const AccessPath& p) {
    switch (p.kind) {
        case PathKind::Empty:    return "EMPTY (condition can never match)";
        case PathKind::FullScan: return "FULL SCAN with zone maps";
        case PathKind::Points:   return std::string(p.index->kind() == IndexKind::Hash ? "HASH LOOKUP " : "INDEX LOOKUP ") + p.index->name() + " x" + std::to_string(p.keys.size());
        case PathKind::Range:    return std::string("INDEX RANGE ") + p.index->name() + (p.reverse ? " (reverse)" : "");
        default: {
            std::string s = "INDEX UNION (";
            for (std::size_t i = 0; i < p.parts.size(); ++i) s += (i ? ", " : "") + describe_path(p.parts[i]);
            return s + ")";
        }
    }
}

class Planner {
public:
    Planner(const Table& table, const std::vector<const Bound*>& preds) : m_table(table), m_preds(preds) {
        m_cons.resize(table.schema().size());
        m_used.assign(table.schema().size(), false);
        for (std::size_t i = 0; i < preds.size(); ++i) analyze(i, *preds[i]);
    }

    bool impossible() const {
        for (std::size_t c = 0; c < m_cons.size(); ++c) if (m_used[c] && m_cons[c].impossible) return true;
        return false;
    }

    AccessPath best() const {
        AccessPath scan;
        scan.kind = PathKind::FullScan;
        scan.est = static_cast<double>(m_table.size());

        if (impossible()) {
            AccessPath e;
            e.kind = PathKind::Empty;
            for (std::size_t i = 0; i < m_preds.size(); ++i) e.consumed.push_back(i);
            return e;
        }

        AccessPath best = scan;
        double best_cost = scan.est + 2.0;

        for (const auto& idx : m_table.indexes()) {
            AccessPath p;
            if (!path_for(*idx, p)) continue;
            const double cost = p.est * 1.5 + 1.0;

            if (cost < best_cost || (cost == best_cost && p.consumed.size() > best.consumed.size())) {
                best = p;
                best_cost = cost;
            }
        }

        if (best.kind == PathKind::FullScan) {
            for (std::size_t i = 0; i < m_preds.size(); ++i) {
                AccessPath u;
                if (union_path(*m_preds[i], u) && u.est * 1.5 + 4.0 < best_cost) {
                    best = u;
                    best_cost = u.est * 1.5 + 4.0;
                }
            }
        }

        return best;
    }

    bool ordered_path(std::size_t column, bool descending, const AccessPath& chosen, AccessPath& out) const {
        const Index* idx = nullptr;

        for (const auto& i : m_table.indexes()) {
            if (i->ordered() && i->columns().front() == column) {
                if (!idx || i->columns().size() < idx->columns().size()) idx = i.get();
            }
        }

        if (!idx) return false;

        if (chosen.kind == PathKind::Range && chosen.index == idx) {
            out = chosen;
            out.reverse = descending;
            out.ordered_output = true;
            return true;
        }

        out = AccessPath();
        out.kind = PathKind::Range;
        out.index = idx;
        out.reverse = descending;
        out.ordered_output = true;
        out.est = static_cast<double>(m_table.size());

        if (m_used[column]) {
            AccessPath r;
            if (range_for(*idx, 0, std::string(), r) && r.kind == PathKind::Range) {
                r.reverse = descending;
                r.ordered_output = true;
                out = r;
            }
        }

        return true;
    }

    const std::vector<Constraint>& constraints() const noexcept { return m_cons; }

    bool covering_path(AccessPath& out) const {
        bool found = false;

        for (const auto& idx : m_table.indexes()) {
            AccessPath p;
            if (!path_for(*idx, p)) continue;
            std::vector<std::size_t> c = p.consumed;
            std::sort(c.begin(), c.end());
            c.erase(std::unique(c.begin(), c.end()), c.end());
            if (c.size() != m_preds.size()) continue;
            if (!found || p.est < out.est) { out = p; found = true; }
        }

        return found;
    }

private:
    const Table&                     m_table;
    const std::vector<const Bound*>& m_preds;
    std::vector<Constraint>          m_cons;
    std::vector<bool>                m_used;

    ColumnType type_of(std::size_t c) const { return m_table.schema()[c].type; }

    void add_eq(std::size_t c, std::vector<Value> vals, std::size_t pred) {
        Constraint& k = m_cons[c];
        m_used[c] = true;

        if (k.has_eq) {
            std::vector<Value> keep;
            for (const auto& v : k.eq) {
                for (const auto& w : vals) if (compare(v, w) == 0) { keep.push_back(v); break; }
            }
            k.eq = std::move(keep);
        } else {
            std::sort(vals.begin(), vals.end(), ValueLess());
            vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
            k.eq = std::move(vals);
            k.has_eq = true;
        }

        if (k.eq.empty()) k.impossible = true;
        k.eq_preds.push_back(pred);
    }

    void analyze(std::size_t i, const Bound& b) {
        if (sources_of(b) != 1u) return;

        if (is_comparison(b.op) && b.op != Op::Ne) {
            const Bound* c = nullptr;
            const Bound* l = nullptr;
            Op op = b.op;

            if (b.args[0].is_column() && b.args[1].is_literal()) { c = &b.args[0]; l = &b.args[1]; }
            else if (b.args[1].is_column() && b.args[0].is_literal()) { c = &b.args[1]; l = &b.args[0]; op = flip_comparison(op); }
            if (!c) return;

            const std::size_t col = c->col;
            m_used[col] = true;

            if (l->value.is_null()) {
                m_cons[col].impossible = true;
                return;
            }

            if (op == Op::Eq) {
                const auto v = exact_coerce(type_of(col), l->value);
                if (!v) {
                    if (l->value.is_number() && (type_of(col) == ColumnType::Integer || type_of(col) == ColumnType::Real)) m_cons[col].impossible = true;
                    return;
                }
                add_eq(col, { *v }, i);
                return;
            }

            Value bv;
            bool incl = false;
            const bool lower = op == Op::Gt || op == Op::Ge;
            if (!bound_value_for(type_of(col), l->value, lower, op == Op::Ge || op == Op::Le, bv, incl)) return;
            Constraint& k = m_cons[col];
            if (lower) k.range.tighten_lo(bv, incl);
            else k.range.tighten_hi(bv, incl);
            k.range.check();
            if (k.range.empty) k.impossible = true;
            k.has_range = true;
            k.range_preds.push_back(i);
            return;
        }

        if (b.op == Op::Between && b.args[0].is_column() && b.args[1].is_literal() && b.args[2].is_literal()) {
            const std::size_t col = b.args[0].col;
            m_used[col] = true;
            if (b.args[1].value.is_null() || b.args[2].value.is_null()) { m_cons[col].impossible = true; return; }
            Value lo, hi;
            bool li = true, hi_i = true;
            if (!bound_value_for(type_of(col), b.args[1].value, true, true, lo, li)) return;
            if (!bound_value_for(type_of(col), b.args[2].value, false, true, hi, hi_i)) return;
            Constraint& k = m_cons[col];
            k.range.tighten_lo(lo, li);
            k.range.tighten_hi(hi, hi_i);
            k.range.check();
            if (k.range.empty) k.impossible = true;
            k.has_range = true;
            k.range_preds.push_back(i);
            return;
        }

        if (b.op == Op::In && b.args[0].is_column()) {
            const std::size_t col = b.args[0].col;
            std::vector<Value> vals;

            for (std::size_t a = 1; a < b.args.size(); ++a) {
                if (!b.args[a].is_literal()) return;
                const auto v = exact_coerce(type_of(col), b.args[a].value);
                if (v) vals.push_back(*v);
            }

            add_eq(col, std::move(vals), i);
            return;
        }

        if (b.op == Op::IsNull && b.args[0].is_column()) {
            const std::size_t col = b.args[0].col;
            m_used[col] = true;
            m_cons[col].is_null = true;
            m_cons[col].null_pred = i;
            return;
        }

        if (b.op == Op::Like && b.args[0].is_column() && b.args[1].is_literal() && b.args[1].value.is_text() && type_of(b.args[0].col) == ColumnType::Text) {
            std::string prefix;
            bool exact = false;
            if (!like_prefix(b.args[1].value.get_text(), prefix, exact)) return;
            Constraint& k = m_cons[b.args[0].col];
            m_used[b.args[0].col] = true;

            if (!k.has_prefix || prefix.size() > k.prefix.size()) {
                k.prefix = prefix;
                k.prefix_exact = exact;
                k.prefix_pred = i;
                k.has_prefix = true;
            }
        }
    }

    double count_of(const Index& idx, const std::string& key) const {
        return static_cast<double>(idx.count(key));
    }

    bool range_for(const Index& idx, std::size_t k, const std::string& prefix, AccessPath& p) const {
        const auto* ord = static_cast<const OrderedIndex*>(&idx);
        const std::size_t col = idx.columns()[k];
        const Constraint& c = m_cons[col];
        p.kind = PathKind::Range;
        p.index = &idx;

        if (c.has_range) {
            if (c.range.lo) {
                std::string key = prefix;
                encode_key_part(key, *c.range.lo);

                if (c.range.lo_incl) {
                    p.lo = KeyBound::at(key, true);
                } else {
                    const std::string s = key_successor(key);
                    if (s.empty()) { p.kind = PathKind::Empty; p.est = 0; return true; }
                    p.lo = KeyBound::at(s, true);
                }
            } else {
                p.lo = KeyBound::at(prefix + "\x02", true);
            }

            if (c.range.hi) {
                std::string key = prefix;
                encode_key_part(key, *c.range.hi);

                if (c.range.hi_incl) {
                    const std::string s = key_successor(key);
                    p.hi = s.empty() ? KeyBound::none() : KeyBound::at(s, false);
                } else {
                    p.hi = KeyBound::at(key, false);
                }
            } else if (!prefix.empty()) {
                const std::string s = key_successor(prefix);
                p.hi = s.empty() ? KeyBound::none() : KeyBound::at(s, false);
            } else {
                p.hi = KeyBound::none();
            }

            p.consumed.insert(p.consumed.end(), c.range_preds.begin(), c.range_preds.end());
        } else if (c.has_prefix && k + 1 == idx.columns().size()) {
            const std::string pre = prefix + encode_text_prefix(c.prefix);
            p.lo = KeyBound::at(pre, true);
            const std::string s = key_successor(pre);
            p.hi = s.empty() ? KeyBound::none() : KeyBound::at(s, false);
            if (c.prefix_exact) p.consumed.push_back(c.prefix_pred);
        } else if (c.is_null) {
            std::string key = prefix;
            encode_key_part(key, Value());
            p.lo = KeyBound::at(key, true);
            const std::string s = key_successor(key);
            p.hi = s.empty() ? KeyBound::none() : KeyBound::at(s, false);
            p.consumed.push_back(c.null_pred);
        } else if (!prefix.empty()) {
            p.lo = KeyBound::at(prefix, true);
            const std::string s = key_successor(prefix);
            p.hi = s.empty() ? KeyBound::none() : KeyBound::at(s, false);
        } else {
            return false;
        }

        p.est = static_cast<double>(ord->count_range(p.lo, p.hi));
        return true;
    }

    bool path_for(const Index& idx, AccessPath& p) const {
        const auto& cols = idx.columns();
        std::size_t k = 0;
        std::string prefix;
        std::vector<std::size_t> consumed;

        while (k < cols.size()) {
            const Constraint& c = m_cons[cols[k]];
            if (!(c.has_eq && c.eq.size() == 1)) break;
            encode_key_part(prefix, c.eq[0]);
            consumed.insert(consumed.end(), c.eq_preds.begin(), c.eq_preds.end());
            ++k;
        }

        if (k == cols.size()) {
            p.kind = PathKind::Points;
            p.index = &idx;
            p.keys = { prefix };
            p.est = count_of(idx, prefix);
            p.consumed = consumed;
            return true;
        }

        const Constraint& next = m_cons[cols[k]];

        if (next.has_eq && k + 1 == cols.size()) {
            p.kind = PathKind::Points;
            p.index = &idx;
            p.est = 0;

            for (const auto& v : next.eq) {
                std::string key = prefix;
                encode_key_part(key, v);
                p.est += count_of(idx, key);
                p.keys.push_back(std::move(key));
            }

            p.consumed = consumed;
            p.consumed.insert(p.consumed.end(), next.eq_preds.begin(), next.eq_preds.end());
            return true;
        }

        if (next.is_null && k + 1 == cols.size() && !idx.ordered()) {
            std::string key = prefix;
            encode_key_part(key, Value());
            p.kind = PathKind::Points;
            p.index = &idx;
            p.keys = { key };
            p.est = count_of(idx, key);
            p.consumed = consumed;
            p.consumed.push_back(next.null_pred);
            return true;
        }

        if (!idx.ordered()) return false;
        if (!range_for(idx, k, prefix, p)) return false;
        p.consumed.insert(p.consumed.end(), consumed.begin(), consumed.end());
        return true;
    }

    bool union_path(const Bound& b, AccessPath& out) const {
        if (b.op != Op::Or) return false;
        std::vector<const Bound*> branches;
        std::vector<const Bound*> stack{ &b };

        while (!stack.empty()) {
            const Bound* n = stack.back();
            stack.pop_back();
            if (n->op == Op::Or) { stack.push_back(&n->args[1]); stack.push_back(&n->args[0]); }
            else branches.push_back(n);
        }

        out = AccessPath();
        out.kind = PathKind::Union;
        out.est = 0;

        for (const Bound* br : branches) {
            std::vector<const Bound*> one{ br };
            Planner sub(m_table, one);
            AccessPath p = sub.best();
            if (p.kind == PathKind::FullScan || p.kind == PathKind::Union) return false;
            out.est += p.est;
            out.parts.push_back(std::move(p));
        }

        return !out.parts.empty();
    }
};

bool part_may_match(const PartitionView& v, const PartitionView::Part& part, const Constraint& c) {
    if (c.impossible || c.is_null) return false;

    auto holds = [&](const Value& x) {
        if (!v.by_range) return compare(x, part.value) == 0;
        if (compare(x, part.lo) < 0) return false;
        return part.hi.is_null() || compare(x, part.hi) < 0;
    };

    if (c.has_eq) {
        bool any = false;
        for (const auto& x : c.eq) if (holds(x)) { any = true; break; }
        if (!any) return false;
    }

    if (c.has_range) {
        const Interval& r = c.range;

        if (v.by_range) {
            if (r.lo && !part.hi.is_null() && compare(part.hi, *r.lo) <= 0) return false;

            if (r.hi) {
                const int k = compare(part.lo, *r.hi);
                if (k > 0 || (k == 0 && !r.hi_incl)) return false;
            }
        } else {
            if (r.lo) {
                const int k = compare(part.value, *r.lo);
                if (k < 0 || (k == 0 && !r.lo_incl)) return false;
            }

            if (r.hi) {
                const int k = compare(part.value, *r.hi);
                if (k > 0 || (k == 0 && !r.hi_incl)) return false;
            }
        }
    }

    if (c.has_prefix && !v.by_range && part.value.is_text()) {
        if (part.value.get_text().compare(0, c.prefix.size(), c.prefix) != 0) return false;
    }

    return true;
}

void covered_preds(const PartitionView& v, const PartitionView::Part& part, const Constraint& c, std::vector<std::size_t>& out) {
    if (c.impossible || c.is_null || c.has_prefix) return;

    if (!v.by_range) {
        if (!c.has_eq || c.has_range) return;
        for (const auto& x : c.eq) {
            if (compare(x, part.value) == 0) { out.insert(out.end(), c.eq_preds.begin(), c.eq_preds.end()); return; }
        }
        return;
    }

    if (!c.has_range || c.has_eq) return;
    const Interval& r = c.range;

    if (r.lo) {
        const int k = compare(part.lo, *r.lo);
        if (k < 0 || (k == 0 && !r.lo_incl)) return;
    }

    if (r.hi && (part.hi.is_null() || compare(part.hi, *r.hi) > 0)) return;
    out.insert(out.end(), c.range_preds.begin(), c.range_preds.end());
}

struct ColumnFilter {
    enum Kind { IntRange, RealRange, TextEq, TextRange, TextPrefix, TextLike, IntSet, TextSet, IsNull, NotNull, BoolEq };

    Kind                  kind = IntRange;
    std::size_t           col = 0;
    std::int64_t          ilo = std::numeric_limits<std::int64_t>::min();
    std::int64_t          ihi = std::numeric_limits<std::int64_t>::max();
    double                dlo = -std::numeric_limits<double>::infinity();
    double                dhi = std::numeric_limits<double>::infinity();
    bool                  lo_incl = true, hi_incl = true;
    std::string           text, text_hi;
    bool                  ci = false;
    collections::HashMap<std::int64_t, char> iset;
    collections::HashMap<std::string, char>  tset;
};

bool compile_filter(const Table& t, const Bound& b, ColumnFilter& f) {
    if (sources_of(b) != 1u) return false;

    auto col_lit = [&](const Bound*& c, const Bound*& l, Op& op) {
        if (b.args.size() < 2) return false;
        op = b.op;
        if (b.args[0].is_column() && b.args[1].is_literal()) { c = &b.args[0]; l = &b.args[1]; return true; }
        if (b.args[1].is_column() && b.args[0].is_literal()) { c = &b.args[1]; l = &b.args[0]; op = flip_comparison(op); return true; }
        return false;
    };

    if (b.op == Op::IsNull || b.op == Op::IsNotNull) {
        if (!b.args[0].is_column()) return false;
        f.kind = b.op == Op::IsNull ? ColumnFilter::IsNull : ColumnFilter::NotNull;
        f.col = b.args[0].col;
        return true;
    }

    if (is_comparison(b.op) && b.op != Op::Ne) {
        const Bound* c = nullptr;
        const Bound* l = nullptr;
        Op op;
        if (!col_lit(c, l, op)) return false;
        if (l->value.is_null()) return false;
        const ColumnType type = t.schema()[c->col].type;
        f.col = c->col;

        if (type == ColumnType::Integer) {
            if (!l->value.is_int()) return false;
            const std::int64_t v = l->value.get_int();
            f.kind = ColumnFilter::IntRange;

            switch (op) {
                case Op::Eq: f.ilo = f.ihi = v; break;
                case Op::Lt: if (v == std::numeric_limits<std::int64_t>::min()) return false; f.ihi = v - 1; break;
                case Op::Le: f.ihi = v; break;
                case Op::Gt: if (v == std::numeric_limits<std::int64_t>::max()) return false; f.ilo = v + 1; break;
                default:     f.ilo = v; break;
            }

            return true;
        }

        if (type == ColumnType::Real) {
            if (!l->value.is_number()) return false;
            const double v = l->value.as_real();
            if (std::isnan(v) || (l->value.is_int() && static_cast<std::int64_t>(v) != l->value.get_int())) return false;
            f.kind = ColumnFilter::RealRange;

            switch (op) {
                case Op::Eq: f.dlo = f.dhi = v; break;
                case Op::Lt: f.dhi = v; f.hi_incl = false; break;
                case Op::Le: f.dhi = v; break;
                case Op::Gt: f.dlo = v; f.lo_incl = false; break;
                default:     f.dlo = v; break;
            }

            return true;
        }

        if (type == ColumnType::Text && l->value.is_text()) {
            if (op == Op::Eq) {
                f.kind = ColumnFilter::TextEq;
                f.text = l->value.get_text();
                return true;
            }

            f.kind = ColumnFilter::TextRange;
            f.text = l->value.get_text();
            f.lo_incl = op == Op::Ge || op == Op::Gt;
            f.hi_incl = op == Op::Ge || op == Op::Le;
            return true;
        }

        if (type == ColumnType::Boolean && l->value.is_bool() && op == Op::Eq) {
            f.kind = ColumnFilter::BoolEq;
            f.ilo = l->value.get_bool() ? 1 : 0;
            return true;
        }

        return false;
    }

    if (b.op == Op::Between && b.args[0].is_column() && b.args[1].is_literal() && b.args[2].is_literal()) {
        const ColumnType type = t.schema()[b.args[0].col].type;
        if (type != ColumnType::Integer && type != ColumnType::Real) return false;
        Value lo, hi;
        bool li = true, hi_i = true;
        if (!bound_value_for(type, b.args[1].value, true, true, lo, li)) return false;
        if (!bound_value_for(type, b.args[2].value, false, true, hi, hi_i)) return false;
        f.col = b.args[0].col;

        if (type == ColumnType::Integer) {
            f.kind = ColumnFilter::IntRange;
            f.ilo = lo.get_int();
            f.ihi = hi.get_int();
        } else {
            f.kind = ColumnFilter::RealRange;
            f.dlo = lo.get_real();
            f.dhi = hi.get_real();
        }

        return true;
    }

    if ((b.op == Op::Like || b.op == Op::ILike) && b.args[0].is_column() && b.args[1].is_literal() && b.args[1].value.is_text()) {
        if (t.schema()[b.args[0].col].type != ColumnType::Text) return false;
        f.col = b.args[0].col;
        std::string prefix;
        bool exact = false;

        if (b.op == Op::Like && like_prefix(b.args[1].value.get_text(), prefix, exact) && exact) {
            f.kind = ColumnFilter::TextPrefix;
            f.text = prefix;
            return true;
        }

        f.kind = ColumnFilter::TextLike;
        f.text = b.args[1].value.get_text();
        f.ci = b.op == Op::ILike;
        return true;
    }

    if (b.op == Op::In && b.args[0].is_column()) {
        const ColumnType type = t.schema()[b.args[0].col].type;
        f.col = b.args[0].col;

        if (type == ColumnType::Integer) {
            f.kind = ColumnFilter::IntSet;

            for (std::size_t i = 1; i < b.args.size(); ++i) {
                if (!b.args[i].is_literal()) return false;
                const auto v = exact_coerce(type, b.args[i].value);
                if (v) f.iset.insert_or_assign(v->get_int(), 1);
            }

            return true;
        }

        if (type == ColumnType::Text) {
            f.kind = ColumnFilter::TextSet;

            for (std::size_t i = 1; i < b.args.size(); ++i) {
                if (!b.args[i].is_literal()) return false;
                if (b.args[i].value.is_text()) f.tset.insert_or_assign(b.args[i].value.get_text(), 1);
            }

            return true;
        }
    }

    return false;
}

bool zone_may_match(const ColumnFilter& f, const ColumnData& c, std::size_t z) {
    if (!c.has_zones() || z >= c.zone_count()) return true;
    const Zone& zone = c.zone(z);

    switch (f.kind) {
        case ColumnFilter::IsNull:   return zone.nulls > 0;
        case ColumnFilter::NotNull:  return zone.values > 0;
        case ColumnFilter::IntRange:
        case ColumnFilter::BoolEq: {
            if (zone.values == 0) return false;
            const std::int64_t lo = f.kind == ColumnFilter::BoolEq ? f.ilo : f.ilo;
            const std::int64_t hi = f.kind == ColumnFilter::BoolEq ? f.ilo : f.ihi;
            return !(zone.int_max < lo || zone.int_min > hi);
        }
        case ColumnFilter::RealRange: {
            if (zone.values == 0) return false;
            if (zone.has_nan) return true;
            if (zone.real_max < f.dlo || (zone.real_max == f.dlo && !f.lo_incl)) return false;
            if (zone.real_min > f.dhi || (zone.real_min == f.dhi && !f.hi_incl)) return false;
            return true;
        }
        default:
            return true;
    }
}

std::uint64_t apply_filter(const ColumnFilter& f, const ColumnData& c, std::size_t word, std::uint64_t mask) {
    std::uint64_t keep = 0;
    const std::uint64_t null_word = c.nulls().words()[word];
    std::uint64_t m = mask;

    if (f.kind == ColumnFilter::IsNull) return mask & null_word;
    if (f.kind == ColumnFilter::NotNull) return mask & ~null_word;
    m &= ~null_word;
    const std::size_t base = word * 64;

    switch (f.kind) {
        case ColumnFilter::IntRange: {
            const std::int64_t* data = c.ints() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                const std::int64_t v = data[b];
                if (v >= f.ilo && v <= f.ihi) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::BoolEq: {
            const std::int64_t* data = c.ints() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                if (data[b] == f.ilo) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::RealRange: {
            const double* data = c.reals() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                const double v = data[b];
                const bool lo_ok = f.lo_incl ? v >= f.dlo : v > f.dlo;
                const bool hi_ok = f.hi_incl ? v <= f.dhi : v < f.dhi;
                if (lo_ok && hi_ok) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::TextEq: {
            const std::string* data = c.texts() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                if (data[b].size() == f.text.size() && data[b] == f.text) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::TextRange: {
            const std::string* data = c.texts() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                const int r = data[b].compare(f.text);
                const bool ok = f.lo_incl ? (f.hi_incl ? r >= 0 : r > 0) : (f.hi_incl ? r <= 0 : r < 0);
                if (ok) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::TextPrefix: {
            const std::string* data = c.texts() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                if (data[b].size() >= f.text.size() && data[b].compare(0, f.text.size(), f.text) == 0) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::TextLike: {
            const std::string* data = c.texts() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                if (like_match(data[b], f.text, f.ci)) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::IntSet: {
            const std::int64_t* data = c.ints() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                if (f.iset.get_ptr(data[b])) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        case ColumnFilter::TextSet: {
            const std::string* data = c.texts() + base;
            while (m) {
                const unsigned b = count_trailing_zeros(m);
                if (f.tset.get_ptr(data[b])) keep |= std::uint64_t(1) << b;
                m &= m - 1;
            }
            return keep;
        }
        default:
            return mask;
    }
}

struct MainPlan {
    AccessPath                 path;
    std::vector<const Bound*>  residual;
    std::vector<ColumnFilter>  filters;
    std::vector<const Bound*>  generic;
};

MainPlan plan_main(const Table& t, const std::vector<const Bound*>& preds, const std::vector<std::size_t>& pred_ids, const std::vector<OrderTerm>* order, const Bound* order_bound, std::optional<std::size_t> limit, std::size_t offset, bool count_only = false) {
    MainPlan mp;
    std::vector<const Bound*> local;
    for (std::size_t id : pred_ids) local.push_back(preds[id]);
    Planner planner(t, local);
    mp.path = planner.best();

    if (count_only && !local.empty() && (mp.path.kind == PathKind::FullScan || mp.path.kind == PathKind::Union)) {
        AccessPath cover;
        if (planner.covering_path(cover)) mp.path = cover;
    }

    if (order && order_bound && order->size() == 1 && order_bound->is_column() && order_bound->src == 0) {
        AccessPath op;
        const bool desc = (*order)[0].order == SortOrder::Desc;

        if (planner.ordered_path(order_bound->col, desc, mp.path, op)) {
            const double n = std::max(1.0, static_cast<double>(t.size()));
            const double best_rows = std::max(1.0, mp.path.est);
            const double sort_cost = best_rows * std::log2(best_rows + 1.0) * 0.25;
            double cost_order = op.est * 1.5;

            if (limit && op.ordered_output) {
                const double want = static_cast<double>(*limit + offset);
                const double sel = std::max(1e-9, best_rows / n);
                cost_order = std::min(cost_order, (want / sel) * 1.5);
            }

            const double cost_best = (mp.path.kind == PathKind::FullScan ? best_rows : best_rows * 1.5) + sort_cost;
            if (mp.path.kind != PathKind::Empty && (cost_order < cost_best || (op.index == mp.path.index && mp.path.kind == PathKind::Range))) mp.path = op;
        }
    }

    std::vector<bool> consumed(local.size(), false);

    if (mp.path.kind != PathKind::Union) {
        for (std::size_t c : mp.path.consumed) consumed[c] = true;
    }

    for (std::size_t i = 0; i < local.size(); ++i) {
        if (consumed[i]) continue;
        mp.residual.push_back(local[i]);
        ColumnFilter f;
        if (mp.path.kind == PathKind::FullScan && compile_filter(t, *local[i], f)) mp.filters.push_back(std::move(f));
        else mp.generic.push_back(local[i]);
    }

    if (mp.path.kind != PathKind::FullScan) {
        mp.filters.clear();
        mp.generic = mp.residual;
    }

    return mp;
}

template <typename Emit>
bool scan_full(const Table& t, const MainPlan& mp, const std::vector<Source>& sources, std::uint32_t part, QueryStats& stats, Emit&& emit) {
    const auto& live = t.live_rows().words();
    std::vector<std::size_t> zone_filters;

    for (std::size_t i = 0; i < mp.filters.size(); ++i) {
        if (t.column(mp.filters[i].col).has_zones()) zone_filters.push_back(i);
    }

    constexpr std::size_t kWordsPerZone = kZoneRows / 64;
    Tuple tuple;
    tuple.part[0] = part;
    Ctx ctx{ &sources, &tuple, nullptr };
    const std::size_t words = (t.slot_count() + 63) / 64;

    for (std::size_t w = 0; w < words; ++w) {
        if (w % kWordsPerZone == 0 && !zone_filters.empty()) {
            const std::size_t z = w / kWordsPerZone;
            bool skip = false;

            for (std::size_t fi : zone_filters) {
                if (!zone_may_match(mp.filters[fi], t.column(mp.filters[fi].col), z)) { skip = true; break; }
            }

            if (skip) {
                ++stats.zones_skipped;
                w += kWordsPerZone - 1;
                continue;
            }
        }

        std::uint64_t mask = live[w];
        if (!mask) continue;
        stats.rows_examined += popcount64(mask);

        for (const auto& f : mp.filters) {
            mask = apply_filter(f, t.column(f.col), w, mask);
            if (!mask) break;
        }

        while (mask) {
            const unsigned b = count_trailing_zeros(mask);
            mask &= mask - 1;
            const RowId slot = static_cast<RowId>(w * 64 + b);
            tuple[0] = slot;
            bool ok = true;

            for (const Bound* g : mp.generic) {
                if (test(*g, ctx) != Tri::True) { ok = false; break; }
            }

            if (ok && !emit(slot)) return false;
        }
    }

    return true;
}

void gather_part(const AccessPath& p, Bitmap& out, QueryStats& stats) {
    if (p.kind == PathKind::Points) {
        std::vector<RowId> rows;

        for (const auto& k : p.keys) {
            ++stats.index_probes;
            p.index->lookup(k, rows);
        }

        for (RowId r : rows) out.set(r);
    } else if (p.kind == PathKind::Range) {
        ++stats.index_probes;
        static_cast<const OrderedIndex*>(p.index)->scan(p.lo, p.hi, true, [&](RowId r, const std::string&) { out.set(r); return true; });
    }
}

template <typename Emit>
bool run_path(const Table& t, const AccessPath& p, const MainPlan& mp, const std::vector<Source>& sources, std::uint32_t part, QueryStats& stats, Emit&& emit) {
    Tuple tuple;
    tuple.part[0] = part;
    Ctx ctx{ &sources, &tuple, nullptr };

    auto check = [&](RowId slot) {
        tuple[0] = slot;
        ++stats.rows_examined;
        for (const Bound* g : mp.generic) if (test(*g, ctx) != Tri::True) return false;
        return true;
    };

    switch (p.kind) {
        case PathKind::Empty:
            return true;
        case PathKind::FullScan:
            return scan_full(t, mp, sources, part, stats, emit);
        case PathKind::Points: {
            std::vector<RowId> rows;

            for (const auto& k : p.keys) {
                ++stats.index_probes;
                p.index->lookup(k, rows);
            }

            if (p.keys.size() > 1 || rows.size() > 1) {
                std::sort(rows.begin(), rows.end());
                rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
            }

            for (RowId r : rows) if (check(r) && !emit(r)) return false;
            return true;
        }
        case PathKind::Range: {
            const auto* ord = static_cast<const OrderedIndex*>(p.index);
            ++stats.index_probes;
            bool go = true;

            ord->scan(p.lo, p.hi, !p.reverse, [&](RowId r, const std::string&) {
                if (check(r) && !emit(r)) { go = false; return false; }
                return true;
            });

            return go;
        }
        default: {
            Bitmap seen(t.slot_count());
            for (const auto& piece : p.parts) gather_part(piece, seen, stats);
            return seen.for_each_until([&](RowId r) { return !check(r) || emit(r); });
        }
    }
}

struct AggState {
    std::size_t  count = 0;
    std::int64_t isum = 0;
    double       dsum = 0.0;
    bool         real = false;
    bool         overflow = false;
    Value        best;
    bool         has_best = false;
    std::shared_ptr<collections::HashMap<std::string, char>> seen;
};

void agg_feed(Aggregate a, AggState& s, const Value& v) {
    if (a == Aggregate::CountAll) { ++s.count; return; }
    if (v.is_null()) return;

    switch (a) {
        case Aggregate::Count:
            ++s.count;
            break;
        case Aggregate::CountDistinct:
            if (!s.seen) s.seen = std::make_shared<collections::HashMap<std::string, char>>();
            if (s.seen->insert_or_assign(Binder::set_key(v), 1).second) ++s.count;
            break;
        case Aggregate::Sum:
        case Aggregate::Avg:
            ++s.count;

            if (v.is_int() && !s.real && !s.overflow) {
                const std::int64_t x = v.get_int();
                if ((x > 0 && s.isum > std::numeric_limits<std::int64_t>::max() - x) || (x < 0 && s.isum < std::numeric_limits<std::int64_t>::min() - x)) {
                    s.overflow = true;
                    s.dsum = static_cast<double>(s.isum) + static_cast<double>(x);
                } else {
                    s.isum += x;
                }
            } else {
                if (!s.real && !s.overflow) s.dsum = static_cast<double>(s.isum);
                s.real = true;
                s.dsum += v.numeric();
            }
            break;
        case Aggregate::Min:
            if (!s.has_best || compare(v, s.best) < 0) { s.best = v; s.has_best = true; }
            break;
        case Aggregate::Max:
            if (!s.has_best || compare(v, s.best) > 0) { s.best = v; s.has_best = true; }
            break;
        default:
            break;
    }
}

Value agg_result(Aggregate a, const AggState& s) {
    switch (a) {
        case Aggregate::CountAll:
        case Aggregate::Count:
        case Aggregate::CountDistinct:
            return Value(static_cast<std::int64_t>(s.count));
        case Aggregate::Sum:
            if (s.count == 0) return Value();
            if (s.real || s.overflow) return Value(s.dsum);
            return Value(s.isum);
        case Aggregate::Avg:
            if (s.count == 0) return Value();
            return Value((s.real || s.overflow ? s.dsum : static_cast<double>(s.isum)) / static_cast<double>(s.count));
        case Aggregate::Min:
        case Aggregate::Max:
            return s.has_best ? s.best : Value();
        default:
            return Value();
    }
}

std::string item_name(const SelectItem& s, const std::vector<Source>& sources) {
    if (!s.alias.empty()) return s.alias;
    const auto& n = s.expr.node();

    if (s.aggregate != Aggregate::None) {
        if (s.aggregate == Aggregate::CountAll) return "count";
        std::string base = n && n->op == Op::Column ? n->name : s.expr.to_string();
        std::string agg = to_string(s.aggregate);
        for (auto& c : agg) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : agg) if (c == ' ') c = '_';
        return agg + "_" + base;
    }

    if (n && n->op == Op::Column) return n->name;
    (void)sources;
    return s.expr.to_string();
}

struct Prepared {
    std::vector<Source>         sources;
    std::vector<PartitionView>  views;
    std::vector<Bound>          where_parts;
    std::vector<const Bound*>   preds;
    std::vector<std::uint32_t>  pred_sources;
    std::vector<Bound>          join_left, join_right;
    std::vector<Bound>          items;
    std::vector<Aggregate>      aggs;
    std::vector<std::string>    names;
    std::vector<Bound>          groups;
    std::vector<Bound>          order;
    std::vector<bool>           order_on_output;
    Bound                       having;
    bool                        has_having = false;
    Bound                       where_root;
    bool                        has_where = false;
    bool                        aggregating = false;
};

void prepare(const Query& q, const TableResolver& resolve, const PartitionResolver& presolve, const std::vector<Value>& params, Prepared& p) {
    auto add_source = [&](const std::string& name, const std::string& alias) {
        PartitionView v;

        if (presolve && presolve(name, v)) {
            Source s;
            s.table = v.schema_table;
            s.name = name;
            s.alias = alias;
            for (const auto& part : v.parts) s.parts.push_back(part.table);
            if (s.parts.empty()) s.parts.push_back(v.schema_table);
            s.view = static_cast<int>(p.views.size());
            p.views.push_back(std::move(v));
            p.sources.push_back(std::move(s));
            return;
        }

        const Table* t = resolve ? resolve(name) : nullptr;
        if (!t) throw DatabaseError("no table named '" + name + "'");
        p.sources.push_back(single_source(t, name, alias));
    };

    add_source(q.table(), q.alias());
    for (const auto& j : q.joins()) add_source(j.table, j.alias);
    if (p.sources.size() > kMaxSources) throw DatabaseError("a query can use at most " + std::to_string(kMaxSources) + " tables");

    for (std::size_t a = 0; a < p.sources.size(); ++a) {
        for (std::size_t b = a + 1; b < p.sources.size(); ++b) {
            const std::string na = p.sources[a].alias.empty() ? p.sources[a].name : p.sources[a].alias;
            const std::string nb = p.sources[b].alias.empty() ? p.sources[b].name : p.sources[b].alias;
            if (na == nb) throw DatabaseError("table '" + na + "' appears twice; give one of them an alias");
        }
    }

    Binder binder(p.sources, params);

    if (q.condition()) {
        p.where_root = binder.bind(q.condition().node());
        p.has_where = true;
        flatten_and(p.where_root, p.preds);
        for (const Bound* b : p.preds) p.pred_sources.push_back(sources_of(*b));
    }

    for (std::size_t j = 0; j < q.joins().size(); ++j) {
        Bound l = binder.bind(q.joins()[j].left.node());
        Bound r = binder.bind(q.joins()[j].right.node());
        const std::uint32_t lm = sources_of(l), rm = sources_of(r);
        const std::uint32_t self = 1u << (j + 1);

        if (rm != self && lm == self) std::swap(l, r);
        if (!(r.is_column() && r.src == static_cast<int>(j + 1))) throw DatabaseError("join on '" + q.joins()[j].table + "' must compare a column of that table");
        if (sources_of(l) & ~((1u << (j + 1)) - 1)) throw DatabaseError("join on '" + q.joins()[j].table + "' can only use tables joined before it");
        p.join_left.push_back(std::move(l));
        p.join_right.push_back(std::move(r));
    }

    p.aggregating = q.has_aggregates();

    if (q.items().empty()) {
        if (p.aggregating) throw DatabaseError("a grouped query needs select items");

        for (std::size_t s = 0; s < p.sources.size(); ++s) {
            const Schema& sc = p.sources[s].table->schema();
            const std::string qual = p.sources[s].alias.empty() ? p.sources[s].name : p.sources[s].alias;

            for (std::size_t c = 0; c < sc.size(); ++c) {
                Bound b;
                b.op = Op::Column;
                b.src = static_cast<int>(s);
                b.col = c;
                p.items.push_back(b);
                p.aggs.push_back(Aggregate::None);
                p.names.push_back(p.sources.size() > 1 ? qual + "." + sc[c].name : sc[c].name);
            }
        }
    } else {
        for (const auto& it : q.items()) {
            p.items.push_back(binder.bind(it.expr.node()));
            p.aggs.push_back(it.aggregate);
            p.names.push_back(item_name(it, p.sources));
        }
    }

    for (const auto& g : q.groups()) p.groups.push_back(binder.bind(g.node()));

    Binder out_binder(p.sources, params);
    out_binder.set_output(&p.names);

    for (const auto& o : q.ordering()) {
        if (p.aggregating) {
            p.order.push_back(out_binder.bind(o.expr.node()));
            p.order_on_output.push_back(true);
        } else {
            try {
                p.order.push_back(binder.bind(o.expr.node()));
                p.order_on_output.push_back(false);
            } catch (const DatabaseError&) {
                p.order.push_back(out_binder.bind(o.expr.node()));
                p.order_on_output.push_back(true);
            }
        }
    }

    if (q.having_condition()) {
        if (!p.aggregating) throw DatabaseError("HAVING needs GROUP BY or an aggregate");
        p.having = out_binder.bind(q.having_condition().node());
        p.has_having = true;
    }
}

struct JoinPlan {
    std::vector<const Index*>                               indexes;
    std::shared_ptr<collections::HashMap<std::string, std::vector<std::uint64_t>>> table;
    std::vector<const Bound*>                               right_filters;
    std::vector<const Bound*>                               after;
};

std::string join_key(const Value& v, ColumnType type) {
    const auto c = exact_coerce(type, v);
    if (!c) return std::string();
    return encode_key(*c);
}

} // namespace

ResultSet execute_query(const Query& q, const TableResolver& resolve, const std::vector<Value>& params, const ExecOptions& options) {
    return execute_query(q, resolve, PartitionResolver(), params, options);
}

ResultSet execute_query(const Query& q, const TableResolver& resolve, const PartitionResolver& presolve, const std::vector<Value>& params, const ExecOptions& options) {
    const auto started = std::chrono::steady_clock::now();
    ResultSet rs;
    Prepared p;
    prepare(q, resolve, presolve, params, p);
    rs.columns = p.names;
    QueryStats& stats = rs.stats;
    const Source& main_src = p.sources[0];
    const std::size_t nsrc = p.sources.size();
    const PartitionView* main_view = main_src.view >= 0 ? &p.views[static_cast<std::size_t>(main_src.view)] : nullptr;

    auto finish = [&]() {
        stats.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    };

    std::vector<std::size_t> main_preds;
    std::vector<std::vector<std::size_t>> stage_preds(nsrc);

    for (std::size_t i = 0; i < p.preds.size(); ++i) {
        const std::uint32_t m = p.pred_sources[i];
        if (m == 1u || m == 0u) { main_preds.push_back(i); continue; }
        std::size_t hi = 0;
        for (std::size_t s = 0; s < nsrc; ++s) if (m & (1u << s)) hi = s;
        stage_preds[hi].push_back(i);
    }

    std::vector<std::uint32_t> part_ids;
    std::vector<std::vector<std::size_t>> part_preds;
    std::string part_note;
    std::size_t covered_parts = 0;

    if (main_view) {
        std::vector<const Bound*> local;
        for (std::size_t i : main_preds) local.push_back(p.preds[i]);
        Planner pruner(*main_view->schema_table, local);
        const Constraint& c = pruner.constraints()[main_view->column];

        for (std::size_t i = 0; i < main_view->parts.size(); ++i) {
            if (!part_may_match(*main_view, main_view->parts[i], c)) continue;
            part_ids.push_back(static_cast<std::uint32_t>(i));
            std::vector<std::size_t> skip;
            covered_preds(*main_view, main_view->parts[i], c, skip);
            std::vector<std::size_t> keep;

            for (std::size_t k = 0; k < main_preds.size(); ++k) {
                if (std::find(skip.begin(), skip.end(), k) == skip.end()) keep.push_back(main_preds[k]);
            }

            if (keep.size() < main_preds.size()) ++covered_parts;
            part_preds.push_back(std::move(keep));
        }

        part_note = "PARTITIONS " + std::to_string(part_ids.size()) + " of " + std::to_string(main_view->total) + " | ";
        if (covered_parts) part_note += std::to_string(covered_parts) + " fully inside the condition | ";
    } else {
        part_ids.push_back(0);
        part_preds.push_back(main_preds);
    }

    bool no_part_filters = true;
    for (const auto& pp : part_preds) if (!pp.empty()) { no_part_filters = false; break; }

    auto part_table = [&](std::uint32_t id) -> const Table& { return *main_src.parts[id]; };

    const bool simple_count = p.aggregating && p.groups.empty() && p.items.size() == 1 && p.aggs[0] == Aggregate::CountAll && nsrc == 1 && !p.has_having && !q.is_distinct() && q.offset_value() == 0;

    if (simple_count && (main_preds.empty() || (main_view && no_part_filters))) {
        std::size_t n = 0;
        for (std::uint32_t id : part_ids) n += part_table(id).size();
        rs.rows.push_back(Row{ Value(static_cast<std::int64_t>(n)) });
        stats.plan = part_note + (main_view ? "COUNT from partition sizes" : "COUNT from table size");
        finish();
        return rs;
    }

    if (p.aggregating && p.groups.empty() && nsrc == 1 && main_preds.empty() && !p.has_having && p.items.size() == 1 && (p.aggs[0] == Aggregate::Min || p.aggs[0] == Aggregate::Max) && p.items[0].is_column()) {
        const std::size_t c = p.items[0].col;
        const bool want_min = p.aggs[0] == Aggregate::Min;
        const Index* model = nullptr;

        for (const auto& idx : main_src.table->indexes()) {
            if (idx->ordered() && idx->columns().front() == c) { model = idx.get(); break; }
        }

        if (model) {
            Value best;
            bool ok = true;

            for (std::uint32_t id : part_ids) {
                const Table& t = part_table(id);
                const Index* idx = t.index(model->name());
                if (!idx || !idx->ordered()) { ok = false; break; }
                const auto* ord = static_cast<const OrderedIndex*>(idx);
                Value found;
                ord->scan(KeyBound::at("\x02", true), KeyBound::none(), want_min, [&](RowId r, const std::string&) {
                    found = t.get(r, c);
                    return found.is_null();
                });
                if (!found.is_null() && (best.is_null() || (want_min ? compare(found, best) < 0 : compare(found, best) > 0))) best = found;
            }

            if (ok) {
                rs.rows.push_back(Row{ best });
                stats.plan = part_note + (want_min ? "MIN" : "MAX") + " from index " + model->name();
                stats.used_index = true;
                stats.index_probes = part_ids.size();
                finish();
                return rs;
            }
        }
    }

    const bool needs_sort = !p.order.empty() && !p.aggregating;
    const Bound* order0 = (!p.order.empty() && !p.order_on_output[0] && !p.aggregating && nsrc == 1) ? &p.order[0] : nullptr;
    const bool key_order = main_view && order0 && main_view->by_range && p.order.size() == 1 && order0->is_column() && order0->src == 0 && order0->col == main_view->column;
    const Bound* plan_order = (main_view && !key_order) ? nullptr : order0;
    std::vector<MainPlan> plans;
    plans.reserve(part_ids.size());

    for (std::size_t k = 0; k < part_ids.size(); ++k) {
        plans.push_back(plan_main(part_table(part_ids[k]), p.preds, part_preds[k], plan_order ? &q.ordering() : nullptr, plan_order, q.limit_value(), q.offset_value(), simple_count));
    }

    std::string path_desc;

    if (plans.empty()) {
        path_desc = "NO PARTITIONS";
    } else {
        const MainPlan& first = plans[0];
        path_desc = describe_path(first.path);
        if (!first.filters.empty()) path_desc += " | " + std::to_string(first.filters.size()) + " column filter(s)";
        if (!first.generic.empty()) path_desc += " | " + std::to_string(first.generic.size()) + " row filter(s)";

        for (std::size_t i = 1; i < plans.size(); ++i) {
            if (plans[i].path.kind != first.path.kind || plans[i].path.index == nullptr) continue;
            if (first.path.index && plans[i].path.index->name() != first.path.index->name()) { path_desc += " | plans differ by partition"; break; }
        }
    }

    for (const auto& mp : plans) {
        if (mp.path.kind != PathKind::FullScan && mp.path.kind != PathKind::Empty) stats.used_index = true;
    }

    stats.plan = part_note + path_desc;
    bool all_ordered = !plans.empty();
    for (const auto& mp : plans) if (!mp.path.ordered_output) { all_ordered = false; break; }
    const bool sorted_by_path = all_ordered && order0 && p.order.size() == 1 && (!main_view || key_order);
    const bool descending_walk = sorted_by_path && q.ordering()[0].order == SortOrder::Desc;

    if (simple_count) {
        std::size_t n = 0;
        bool exact = true;

        for (std::size_t pi = 0; pi < plans.size(); ++pi) {
            const MainPlan& mp = plans[pi];
            if (!mp.generic.empty() || !mp.filters.empty()) { exact = false; break; }

            if (mp.path.kind == PathKind::FullScan) {
                n += part_table(part_ids[pi]).size();
            } else if (mp.path.kind == PathKind::Points) {
                for (const auto& k : mp.path.keys) n += mp.path.index->count(k);
            } else if (mp.path.kind == PathKind::Range) {
                n += static_cast<const OrderedIndex*>(mp.path.index)->count_range(mp.path.lo, mp.path.hi);
            } else if (mp.path.kind != PathKind::Empty) {
                exact = false;
                break;
            }
        }

        if (exact) {
            rs.rows.push_back(Row{ Value(static_cast<std::int64_t>(n)) });
            stats.plan = part_note + "COUNT from " + path_desc;
            finish();
            return rs;
        }
    }

    std::vector<JoinPlan> joins(q.joins().size());

    for (std::size_t j = 0; j < q.joins().size(); ++j) {
        const std::size_t src = j + 1;
        const Source& right = p.sources[src];
        JoinPlan& jp = joins[j];
        const std::size_t rcol = p.join_right[j].col;

        for (std::size_t i : stage_preds[src]) {
            if (p.pred_sources[i] == (1u << src) && q.joins()[j].kind == JoinKind::Inner) jp.right_filters.push_back(p.preds[i]);
            jp.after.push_back(p.preds[i]);
        }

        for (const Table* t : right.parts) {
            const Index* ix = t->best_index_for(rcol, false);
            if (!ix || ix->columns().size() != 1) { jp.indexes.clear(); break; }
            jp.indexes.push_back(ix);
        }

        if (jp.indexes.empty()) {
            jp.table = std::make_shared<collections::HashMap<std::string, std::vector<std::uint64_t>>>();
            std::size_t total = 0;
            for (const Table* t : right.parts) total += t->size();
            jp.table->reserve(total);
            Tuple tuple;
            Ctx ctx{ &p.sources, &tuple, nullptr };

            for (std::uint32_t pi = 0; pi < right.parts.size(); ++pi) {
                const Table& rt = *right.parts[pi];
                tuple.part[src] = pi;

                rt.for_each([&](RowId r) {
                    tuple[src] = r;
                    for (const Bound* f : jp.right_filters) if (test(*f, ctx) != Tri::True) return;
                    if (rt.column(rcol).is_null(r)) return;
                    (*jp.table)[encode_key(rt.get(r, rcol))].push_back((static_cast<std::uint64_t>(pi) << 32) | r);
                });
            }

            stats.plan += " | HASH JOIN " + right.name;
        } else {
            stats.plan += " | INDEX JOIN " + right.name + " via " + jp.indexes.front()->name();
        }

        if (right.view >= 0) stats.plan += " across " + std::to_string(p.views[static_cast<std::size_t>(right.view)].parts.size()) + " partition(s)";
        if (q.joins()[j].kind == JoinKind::Left) stats.plan += " (left)";
    }

    const std::size_t want = q.limit_value() ? *q.limit_value() + q.offset_value() : std::numeric_limits<std::size_t>::max();
    const bool streaming = !p.aggregating && (!needs_sort || sorted_by_path) && !q.is_distinct();
    std::vector<Tuple> collected;
    std::size_t produced = 0;
    const bool any_output_order = std::find(p.order_on_output.begin(), p.order_on_output.end(), true) != p.order_on_output.end();
    const bool topn = needs_sort && !sorted_by_path && q.limit_value() && !q.is_distinct() && !p.aggregating && !any_output_order && want > 0 && want <= 100000;

    struct HeapItem {
        Row         key;
        std::size_t seq = 0;
        Tuple       tuple;
    };

    std::vector<HeapItem> heap;
    Row scratch;
    std::size_t seq = 0;

    auto key_less = [&](const Row& a, std::size_t sa, const Row& b, std::size_t sb) {
        for (std::size_t t = 0; t < p.order.size(); ++t) {
            const int c = compare(a[t], b[t]);
            if (c != 0) return q.ordering()[t].order == SortOrder::Asc ? c < 0 : c > 0;
        }
        return sa < sb;
    };

    auto heap_less = [&](const HeapItem& a, const HeapItem& b) { return key_less(a.key, a.seq, b.key, b.seq); };

    collections::HashMap<std::string, std::size_t> group_index;
    std::vector<std::vector<AggState>> group_states;
    std::vector<Tuple> group_first;

    Tuple tuple;
    tuple.fill(kNoRow);
    Ctx ctx{ &p.sources, &tuple, nullptr };

    auto project = [&](const Tuple& t) {
        Ctx c{ &p.sources, &t, nullptr };
        Row row;
        row.reserve(p.items.size());
        for (const auto& it : p.items) row.push_back(eval(it, c));
        return row;
    };

    auto sink = [&](const Tuple& t) -> bool {
        if (p.aggregating) {
            Ctx c{ &p.sources, &t, nullptr };
            std::string key;
            for (const auto& g : p.groups) encode_key_part(key, eval(g, c));
            auto ins = group_index.try_emplace(key, group_states.size());

            if (ins.second) {
                group_states.emplace_back(p.items.size());
                group_first.push_back(t);
            }

            auto& states = group_states[ins.first->second];

            for (std::size_t i = 0; i < p.items.size(); ++i) {
                if (p.aggs[i] == Aggregate::None) continue;
                agg_feed(p.aggs[i], states[i], p.aggs[i] == Aggregate::CountAll ? Value() : eval(p.items[i], c));
            }

            return true;
        }

        if (streaming) {
            ++produced;
            if (produced <= q.offset_value()) return true;
            rs.rows.push_back(project(t));
            if (options.max_rows && rs.rows.size() > options.max_rows) throw DatabaseError("query returned more than " + std::to_string(options.max_rows) + " rows");
            return produced < want;
        }

        if (topn) {
            Ctx c{ &p.sources, &t, nullptr };
            scratch.clear();
            for (const auto& o : p.order) scratch.push_back(eval(o, c));
            const std::size_t my = seq++;

            if (heap.size() < want) {
                heap.push_back(HeapItem{ scratch, my, t });
                std::push_heap(heap.begin(), heap.end(), heap_less);
            } else if (key_less(scratch, my, heap.front().key, heap.front().seq)) {
                std::pop_heap(heap.begin(), heap.end(), heap_less);
                heap.back().key.swap(scratch);
                heap.back().seq = my;
                heap.back().tuple = t;
                std::push_heap(heap.begin(), heap.end(), heap_less);
            }

            return true;
        }

        collected.push_back(t);
        return true;
    };

    std::function<bool(std::size_t)> expand = [&](std::size_t j) -> bool {
        if (j == joins.size()) return sink(tuple);
        const std::size_t src = j + 1;
        const JoinPlan& jp = joins[j];
        const Value lv = eval(p.join_left[j], ctx);
        const ColumnType rtype = p.sources[src].table->schema()[p.join_right[j].col].type;
        bool matched = false;

        auto visit = [&](std::uint32_t pi, RowId r) -> bool {
            tuple[src] = r;
            tuple.part[src] = pi;
            ++stats.rows_examined;
            for (const Bound* f : jp.after) if (test(*f, ctx) != Tri::True) return true;
            matched = true;
            return expand(j + 1);
        };

        auto reset = [&]() {
            tuple[src] = kNoRow;
            tuple.part[src] = 0;
        };

        if (!lv.is_null()) {
            const std::string key = join_key(lv, rtype);

            if (!key.empty()) {
                if (!jp.indexes.empty()) {
                    std::vector<RowId> rows;

                    for (std::uint32_t pi = 0; pi < jp.indexes.size(); ++pi) {
                        ++stats.index_probes;
                        rows.clear();
                        jp.indexes[pi]->lookup(key, rows);
                        for (RowId r : rows) if (!visit(pi, r)) { reset(); return false; }
                    }
                } else if (const auto* list = jp.table->get_ptr(key)) {
                    for (std::uint64_t e : *list) {
                        if (!visit(static_cast<std::uint32_t>(e >> 32), static_cast<RowId>(e & 0xffffffffu))) { reset(); return false; }
                    }
                }
            }
        }

        reset();

        if (!matched && q.joins()[j].kind == JoinKind::Left) {
            bool ok = true;
            for (const Bound* f : jp.after) if (test(*f, ctx) != Tri::True) { ok = false; break; }
            if (ok) return expand(j + 1);
        }

        return true;
    };

    if (!(q.limit_value() && *q.limit_value() == 0)) {
        const std::size_t n = plans.size();

        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t at = descending_walk ? n - 1 - k : k;
            const std::uint32_t pid = part_ids[at];

            const bool go = run_path(part_table(pid), plans[at].path, plans[at], p.sources, pid, stats, [&](RowId r) {
                tuple.fill(kNoRow);
                tuple[0] = r;
                tuple.part[0] = pid;
                return expand(0);
            });

            if (!go) break;
        }
    }

    if (p.aggregating) {
        if (group_states.empty() && p.groups.empty()) {
            group_states.emplace_back(p.items.size());
            Tuple none;
            none.fill(kNoRow);
            group_first.push_back(none);
        }

        for (std::size_t g = 0; g < group_states.size(); ++g) {
            Ctx c{ &p.sources, &group_first[g], nullptr };
            Row row;

            for (std::size_t i = 0; i < p.items.size(); ++i) {
                if (p.aggs[i] == Aggregate::None) row.push_back(group_first[g][0] == kNoRow ? Value() : eval(p.items[i], c));
                else row.push_back(agg_result(p.aggs[i], group_states[g][i]));
            }

            if (p.has_having) {
                Ctx hc{ &p.sources, &group_first[g], &row };
                if (test(p.having, hc) != Tri::True) continue;
            }

            rs.rows.push_back(std::move(row));
        }

        if (!p.order.empty()) {
            std::vector<std::pair<Row, std::size_t>> keyed;
            keyed.reserve(rs.rows.size());

            for (std::size_t i = 0; i < rs.rows.size(); ++i) {
                Ctx oc{ &p.sources, &group_first[0], &rs.rows[i] };
                Row k;
                for (const auto& o : p.order) k.push_back(eval(o, oc));
                keyed.emplace_back(std::move(k), i);
            }

            std::stable_sort(keyed.begin(), keyed.end(), [&](const auto& a, const auto& b) {
                for (std::size_t t = 0; t < p.order.size(); ++t) {
                    const int c = compare(a.first[t], b.first[t]);
                    if (c != 0) return q.ordering()[t].order == SortOrder::Asc ? c < 0 : c > 0;
                }
                return false;
            });

            std::vector<Row> sorted;
            sorted.reserve(keyed.size());
            for (auto& k : keyed) sorted.push_back(std::move(rs.rows[k.second]));
            rs.rows = std::move(sorted);
            stats.plan += " | SORT groups";
        }

        stats.plan += " | HASH AGGREGATE " + std::to_string(group_states.size()) + " group(s)";
    } else if (!streaming) {
        if (topn) {
            std::sort_heap(heap.begin(), heap.end(), heap_less);
            collected.clear();
            for (const auto& h : heap) collected.push_back(h.tuple);
            stats.plan += " | TOP-" + std::to_string(want) + " HEAP";
        } else if (needs_sort && !sorted_by_path) {
            std::vector<std::pair<Row, std::size_t>> keyed;
            keyed.reserve(collected.size());

            for (std::size_t i = 0; i < collected.size(); ++i) {
                Row proj;
                const bool need_proj = std::find(p.order_on_output.begin(), p.order_on_output.end(), true) != p.order_on_output.end();
                if (need_proj) proj = project(collected[i]);
                Ctx oc{ &p.sources, &collected[i], need_proj ? &proj : nullptr };
                Row k;
                for (const auto& o : p.order) k.push_back(eval(o, oc));
                keyed.emplace_back(std::move(k), i);
            }

            auto less = [&](const std::pair<Row, std::size_t>& a, const std::pair<Row, std::size_t>& b) {
                for (std::size_t t = 0; t < p.order.size(); ++t) {
                    const int c = compare(a.first[t], b.first[t]);
                    if (c != 0) return q.ordering()[t].order == SortOrder::Asc ? c < 0 : c > 0;
                }
                return a.second < b.second;
            };

            if (q.limit_value() && !q.is_distinct() && want < keyed.size()) {
                std::partial_sort(keyed.begin(), keyed.begin() + static_cast<std::ptrdiff_t>(want), keyed.end(), less);
                keyed.resize(want);
                stats.plan += " | TOP-" + std::to_string(want) + " SORT";
            } else {
                std::sort(keyed.begin(), keyed.end(), less);
                stats.plan += " | SORT";
            }

            std::vector<Tuple> ordered;
            ordered.reserve(keyed.size());
            for (const auto& k : keyed) ordered.push_back(collected[k.second]);
            collected = std::move(ordered);
        }

        collections::HashMap<std::string, char> seen;
        std::size_t skipped = 0;

        for (const auto& t : collected) {
            Row row = project(t);

            if (q.is_distinct()) {
                if (!seen.insert_or_assign(encode_key(row), 1).second) continue;
            }

            if (skipped < q.offset_value()) { ++skipped; continue; }
            rs.rows.push_back(std::move(row));
            if (q.limit_value() && rs.rows.size() >= *q.limit_value()) break;
        }

        if (q.is_distinct()) stats.plan += " | DISTINCT";
    }

    if (p.aggregating && (q.offset_value() || q.limit_value() || q.is_distinct())) {
        std::vector<Row> out;
        collections::HashMap<std::string, char> seen;
        std::size_t skipped = 0;

        for (auto& row : rs.rows) {
            if (q.is_distinct() && !seen.insert_or_assign(encode_key(row), 1).second) continue;
            if (skipped < q.offset_value()) { ++skipped; continue; }
            out.push_back(std::move(row));
            if (q.limit_value() && out.size() >= *q.limit_value()) break;
        }

        rs.rows = std::move(out);
    }

    if (sorted_by_path) stats.plan += main_view && plans.size() > 1 ? " | ORDER FROM INDEX, PARTITION BY PARTITION" : " | ORDER FROM INDEX";
    if (streaming && q.limit_value()) stats.plan += " | STOP AFTER " + std::to_string(want);
    finish();
    return rs;
}

std::vector<RowId> matching_rows(const Table& table, const Expr& condition, const std::vector<Value>& params, QueryStats* stats_out, std::size_t limit) {
    std::vector<Source> sources{ single_source(&table, table.name(), std::string()) };
    Binder binder(sources, params);
    Bound root;
    std::vector<const Bound*> preds;
    std::vector<std::size_t> ids;

    if (condition) {
        root = binder.bind(condition.node());
        flatten_and(root, preds);
        for (std::size_t i = 0; i < preds.size(); ++i) {
            if (sources_of(*preds[i]) & ~1u) throw DatabaseError("condition refers to another table");
            ids.push_back(i);
        }
    }

    QueryStats local;
    QueryStats& stats = stats_out ? *stats_out : local;
    MainPlan mp = plan_main(table, preds, ids, nullptr, nullptr, std::nullopt, 0);
    stats.plan = describe_path(mp.path);
    stats.used_index = mp.path.kind != PathKind::FullScan && mp.path.kind != PathKind::Empty;
    std::vector<RowId> out;

    run_path(table, mp.path, mp, sources, 0, stats, [&](RowId r) {
        out.push_back(r);
        return limit == 0 || out.size() < limit;
    });

    return out;
}

std::string explain_query(const Query& query, const TableResolver& tables, const std::vector<Value>& params) {
    return explain_query(query, tables, PartitionResolver(), params);
}

std::string explain_query(const Query& query, const TableResolver& tables, const PartitionResolver& partitions, const std::vector<Value>& params) {
    Query probe = query;
    probe.limit(0);
    const ResultSet r = execute_query(probe, tables, partitions, params);
    return r.stats.plan;
}

std::vector<std::size_t> prune_partitions(const PartitionView& view, const Expr& condition, const std::vector<Value>& params) {
    std::vector<std::size_t> out;

    if (!condition) {
        for (std::size_t i = 0; i < view.parts.size(); ++i) out.push_back(i);
        return out;
    }

    std::vector<Source> sources{ single_source(view.schema_table, view.schema_table->name(), std::string()) };
    Binder binder(sources, params);
    const Bound root = binder.bind(condition.node());
    std::vector<const Bound*> preds;
    flatten_and(root, preds);
    Planner pruner(*view.schema_table, preds);
    const Constraint& c = pruner.constraints()[view.column];

    for (std::size_t i = 0; i < view.parts.size(); ++i) {
        if (part_may_match(view, view.parts[i], c)) out.push_back(i);
    }

    return out;
}

} // namespace database
} // namespace fizmo
