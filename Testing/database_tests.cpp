#include "../Source/Database/db.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace fizmo::database;

static int g_pass = 0, g_fail = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static std::mt19937_64 rng(12345);
static std::int64_t rnd(std::int64_t lo, std::int64_t hi) { return std::uniform_int_distribution<std::int64_t>(lo, hi)(rng); }

static const char* kWords[] = { "apple", "apricot", "banana", "berry", "cherry", "chili", "date", "fig", "grape", "kiwi", "lemon", "lime", "mango", "melon", "olive", "pear", "pepper", "plum" };

static Value random_text() {
    if (rnd(0, 12) == 0) return Value();
    std::string s = kWords[rnd(0, 17)];
    if (rnd(0, 3) == 0) s += std::to_string(rnd(0, 9));
    return Value(s);
}

static Row random_row() {
    Row r(6);
    r[0] = Value();
    r[1] = rnd(0, 9) == 0 ? Value() : Value(rnd(-20, 60));
    r[2] = rnd(0, 9) == 0 ? Value() : Value(static_cast<double>(rnd(-1000, 1000)) / 8.0);
    r[3] = random_text();
    r[4] = rnd(0, 7) == 0 ? Value() : Value(rnd(0, 1) == 1);
    r[5] = Value(rnd(0, 4));
    return r;
}

static Schema fuzz_schema() {
    return Schema{ integer("id").primary().autoincrement(), integer("a"), real("b"), text("c"), boolean("d"), integer("g").not_null() };
}

struct Ref {
    enum Kind { Cmp, InList, Between, IsNull, NotNull, Like, And, Or, Not };
    Kind                kind = Cmp;
    Op                  op = Op::Eq;
    std::size_t         col = 0;
    Value               v, v2;
    std::vector<Value>  list;
    std::string         pattern;
    std::vector<Ref>    kids;
};

static const char* kCols[] = { "id", "a", "b", "c", "d", "g" };

static Value literal_for(std::size_t col) {
    switch (col) {
        case 0: return Value(rnd(-5, 3200));
        case 1: return rnd(0, 6) == 0 ? Value(static_cast<double>(rnd(-200, 600)) / 10.0) : Value(rnd(-25, 65));
        case 2: return rnd(0, 4) == 0 ? Value(rnd(-130, 130)) : Value(static_cast<double>(rnd(-1000, 1000)) / 8.0);
        case 3: { Value t = random_text(); return t.is_null() ? Value("kiwi") : t; }
        case 4: return Value(rnd(0, 1) == 1);
        default: return Value(rnd(-1, 5));
    }
}

static Ref random_pred(int depth) {
    Ref r;
    const int pick = static_cast<int>(rnd(0, depth > 0 ? 11 : 7));
    r.col = static_cast<std::size_t>(rnd(0, 5));

    switch (pick) {
        case 0: case 1: case 2: {
            static const Op ops[] = { Op::Eq, Op::Ne, Op::Lt, Op::Le, Op::Gt, Op::Ge };
            r.kind = Ref::Cmp;
            r.op = ops[rnd(0, 5)];
            r.v = literal_for(r.col);
            break;
        }
        case 3:
            r.kind = Ref::InList;
            for (int i = 0, n = static_cast<int>(rnd(1, 14)); i < n; ++i) r.list.push_back(literal_for(r.col));
            break;
        case 4:
            r.kind = Ref::Between;
            r.v = literal_for(r.col);
            r.v2 = literal_for(r.col);
            if (compare(r.v, r.v2) > 0 && rnd(0, 3)) std::swap(r.v, r.v2);
            break;
        case 5:
            r.kind = rnd(0, 1) ? Ref::IsNull : Ref::NotNull;
            break;
        case 6:
        case 7: {
            r.kind = Ref::Like;
            r.col = 3;
            static const char* pats[] = { "ap%", "b%", "c%y", "%an%", "l_me", "pe%", "%", "plum", "m%n", "gra%", "_i%", "chil%" };
            r.pattern = pats[rnd(0, 11)];
            break;
        }
        case 8: case 9:
            r.kind = Ref::And;
            r.kids = { random_pred(depth - 1), random_pred(depth - 1) };
            break;
        case 10:
            r.kind = Ref::Or;
            r.kids = { random_pred(depth - 1), random_pred(depth - 1) };
            break;
        default:
            r.kind = Ref::Not;
            r.kids = { random_pred(depth - 1) };
            break;
    }

    return r;
}

static Expr to_expr(const Ref& r) {
    const Expr c = col(kCols[r.col]);

    switch (r.kind) {
        case Ref::Cmp:
            switch (r.op) {
                case Op::Eq: return c == r.v;
                case Op::Ne: return c != r.v;
                case Op::Lt: return c < r.v;
                case Op::Le: return c <= r.v;
                case Op::Gt: return c > r.v;
                default:     return c >= r.v;
            }
        case Ref::InList:  return c.in(r.list);
        case Ref::Between: return c.between(r.v, r.v2);
        case Ref::IsNull:  return c.is_null();
        case Ref::NotNull: return c.is_not_null();
        case Ref::Like:    return c.like(r.pattern);
        case Ref::And:     return to_expr(r.kids[0]) && to_expr(r.kids[1]);
        case Ref::Or:      return to_expr(r.kids[0]) || to_expr(r.kids[1]);
        default:           return !to_expr(r.kids[0]);
    }
}

static int ref_cmp(const Value& a, const Value& b) {
    const bool an = a.is_number() || a.is_bool(), bn = b.is_number() || b.is_bool();
    if (an && bn && !a.is_bool() && !b.is_bool()) {
        const long double x = a.is_int() ? static_cast<long double>(a.get_int()) : a.get_real();
        const long double y = b.is_int() ? static_cast<long double>(b.get_int()) : b.get_real();
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    return compare(a, b);
}

static Tri ref_eval(const Ref& r, const Row& row) {
    const Value& v = row[r.col];

    switch (r.kind) {
        case Ref::Cmp: {
            if (v.is_null() || r.v.is_null()) return Tri::Unknown;
            const int c = ref_cmp(v, r.v);
            switch (r.op) {
                case Op::Eq: return tri_of(c == 0);
                case Op::Ne: return tri_of(c != 0);
                case Op::Lt: return tri_of(c < 0);
                case Op::Le: return tri_of(c <= 0);
                case Op::Gt: return tri_of(c > 0);
                default:     return tri_of(c >= 0);
            }
        }
        case Ref::InList: {
            if (v.is_null()) return Tri::Unknown;
            for (const auto& x : r.list) if (ref_cmp(v, x) == 0) return Tri::True;
            return Tri::False;
        }
        case Ref::Between:
            if (v.is_null()) return Tri::Unknown;
            return tri_of(ref_cmp(v, r.v) >= 0 && ref_cmp(v, r.v2) <= 0);
        case Ref::IsNull:  return tri_of(v.is_null());
        case Ref::NotNull: return tri_of(!v.is_null());
        case Ref::Like:
            if (v.is_null()) return Tri::Unknown;
            return tri_of(like_match(v.get_text(), r.pattern, false));
        case Ref::And: return tri_and(ref_eval(r.kids[0], row), ref_eval(r.kids[1], row));
        case Ref::Or:  return tri_or(ref_eval(r.kids[0], row), ref_eval(r.kids[1], row));
        default:       return tri_not(ref_eval(r.kids[0], row));
    }
}

static std::vector<Row> dump(const Table& t) {
    std::vector<Row> rows;
    t.for_each([&](RowId r) { rows.push_back(t.row(r)); });
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return compare(a[0], b[0]) < 0; });
    return rows;
}

static std::vector<Row> dump_db(const Database& db, const std::string& table) {
    return db.read(table, [](const Table& t) { return dump(t); });
}

static void test_values() {
    std::vector<Value> vals;
    for (int i = 0; i < 400; ++i) vals.push_back(Value(rnd(-1000000, 1000000) * (rnd(0, 1) ? 1 : 1000000000)));
    for (int i = 0; i < 400; ++i) vals.push_back(Value(static_cast<double>(rnd(-100000, 100000)) / 7.0));
    vals.push_back(Value(0.0));
    vals.push_back(Value(-0.0));
    for (int i = 0; i < 200; ++i) { Value t = random_text(); if (!t.is_null()) vals.push_back(t); }
    vals.push_back(Value(std::string("a\0b", 3)));
    vals.push_back(Value(std::string("a")));
    vals.push_back(Value(std::string("a\0", 2)));
    bool ordered = true;

    for (std::size_t i = 0; i < vals.size(); ++i) {
        for (std::size_t j = 0; j < vals.size(); j += 7) {
            if (vals[i].type() != vals[j].type()) continue;
            const int c = compare(vals[i], vals[j]);
            const int e = encode_key(vals[i]).compare(encode_key(vals[j]));
            if ((c < 0) != (e < 0) || (c == 0) != (e == 0)) { ordered = false; std::printf("  order mismatch %s vs %s\n", vals[i].to_string().c_str(), vals[j].to_string().c_str()); }
        }
    }

    CHECK(ordered);
    CHECK(Value(3) == Value(3.0));
    CHECK(Value(3).hash() == Value(3.0).hash());
    CHECK(Value(2) < Value(2.5));
    CHECK(Value() < Value(false));
    CHECK(Value(true) < Value(-100));
    CHECK(Value(1e30) > Value(std::int64_t(9000000000000000000)));
    CHECK(Value("abc") < Value::blob("a"));
    CHECK(key_successor(std::string("ab\xff\xff")) == "ac");
    CHECK(like_match("banana", "b%n_", false));
    CHECK(!like_match("banana", "b%x", false));
    CHECK(like_match("50%", "50\\%", false));
    CHECK(like_match("BaNaNa", "ban%", true));
    std::string prefix;
    bool exact = false;
    CHECK(like_prefix("app%", prefix, exact) && prefix == "app" && exact);
    CHECK(like_prefix("a_c%", prefix, exact) && prefix == "a" && !exact);
    CHECK(!like_prefix("%x", prefix, exact));
    CHECK(arithmetic(Op::Add, Value(std::int64_t(9223372036854775807LL)), Value(1)).is_real());
    CHECK(arithmetic(Op::Div, Value(7), Value(2)) == Value(3));
    CHECK(arithmetic(Op::Div, Value(7), Value(0)).is_null());
}

static void test_table_basics() {
    Database db;
    db.create_table("users", Schema{ integer("id").primary().autoincrement(), text("email").not_null().unique_key(), text("name"), integer("age").defaults(Value(18)), boolean("admin").defaults(Value(false)) });
    const RowId a = db.insert("users", { { "email", "a@x.com" }, { "name", "Ann" } });
    db.insert("users", { { "email", "b@x.com" }, { "name", "Bob" }, { "age", 40 } });
    db.insert("users", { { "id", 10 }, { "email", "c@x.com" } });
    const RowId d = db.insert("users", { { "email", "d@x.com" } });
    auto row = db.get("users", { 11 });
    CHECK(row && (*row)[1] == Value("d@x.com"));
    CHECK(db.read("users", [&](const Table& t) { return t.get(a, "age"); }) == Value(18));
    CHECK(db.read("users", [&](const Table& t) { return t.get(d, "id"); }) == Value(11));
    bool threw = false;
    try { db.insert("users", { { "email", "a@x.com" } }); } catch (const ConstraintError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { db.insert("users", { { "name", "nobody" } }); } catch (const ConstraintError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { db.insert("users", { { "email", "e@x.com" }, { "age", "old" } }); } catch (const TypeError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { db.update("users", col("email") == "b@x.com", { { "email", "a@x.com" } }); } catch (const ConstraintError&) { threw = true; }
    CHECK(threw);
    CHECK(db.count("users") == 4);
    CHECK(db.update("users", col("age") == 18, { { "age", 21 } }) == 3);
    CHECK(db.count("users", col("age") == 21) == 3);
    CHECK(db.remove("users", col("name").is_null()) == 2);
    CHECK(db.count("users") == 2);
    const RowId e = db.insert("users", { { "email", "e@x.com" } });
    CHECK(e == 3 || e == 2);
    CHECK(db.get("users", { 12 }).has_value());
    auto rs = db.query(Query("users").select({ "name", "age" }).where(col("email") == "b@x.com"));
    CHECK(rs.size() == 1 && rs.at(0, "age") == Value(40));
    std::printf("  plan: %s\n", rs.stats.plan.c_str());
    CHECK(rs.stats.used_index);
}

static void test_fuzz() {
    Database db;
    db.create_table("t", fuzz_schema());
    db.create_table("plain", fuzz_schema());
    db.create_index("t", { "a" }, IndexKind::Ordered);
    db.create_index("t", { "c" }, IndexKind::Ordered);
    db.create_index("t", { "b" }, IndexKind::Ordered);
    db.create_index("t", { "d" }, IndexKind::Hash);
    db.create_index("t", { "g", "a" }, IndexKind::Ordered);
    db.create_index("t", { "c", "g" }, IndexKind::Hash);
    std::vector<Row> rows;
    for (int i = 0; i < 3000; ++i) rows.push_back(random_row());
    db.insert_many("t", rows);
    db.insert_many("plain", rows);
    int queries = 0, mismatches = 0, indexed = 0;

    auto mutate = [&]() {
        for (int k = 0; k < 120; ++k) {
            const int what = static_cast<int>(rnd(0, 2));

            if (what == 0) {
                const Row r = random_row();
                db.insert_row("t", r);
                db.insert_row("plain", r);
            } else if (what == 1) {
                const std::int64_t id = rnd(1, 3400);
                db.remove("t", col("id") == id);
                db.remove("plain", col("id") == id);
            } else {
                const std::int64_t id = rnd(1, 3400);
                const Row r = random_row();
                const NamedValues ch = { { "a", r[1] }, { "c", r[3] }, { "b", r[2] } };
                db.update("t", col("id") == id, ch);
                db.update("plain", col("id") == id, ch);
            }
        }

        db.maintain();
    };

    for (int round = 0; round < 6; ++round) {
        const auto truth = dump_db(db, "plain");

        for (int q = 0; q < 250; ++q) {
            const Ref ref = random_pred(static_cast<int>(rnd(0, 3)));
            const Expr where = to_expr(ref);
            std::vector<std::int64_t> expect;
            for (const auto& r : truth) if (ref_eval(ref, r) == Tri::True) expect.push_back(r[0].get_int());

            Query query("t");
            query.where(where).select({ "id" });
            const int shape = static_cast<int>(rnd(0, 3));
            std::size_t lim = 0;

            if (shape == 1) {
                query.order_by(col(kCols[rnd(1, 5)]), rnd(0, 1) ? SortOrder::Asc : SortOrder::Desc).order_by(col("id"));
            } else if (shape == 2) {
                query.order_by(col("a"), rnd(0, 1) ? SortOrder::Asc : SortOrder::Desc).order_by(col("id"));
                lim = static_cast<std::size_t>(rnd(0, 30));
                query.limit(lim).offset(static_cast<std::size_t>(rnd(0, 5)));
            }

            if (shape == 3 && rnd(0, 1)) {
                const char* oc = kCols[rnd(1, 3)];
                const SortOrder dir = rnd(0, 1) ? SortOrder::Asc : SortOrder::Desc;
                Query a("t"), b("plain");
                a.where(where).select({ "id", oc }).order_by(col(oc), dir).limit(static_cast<std::size_t>(rnd(1, 40))).cache(false);
                b.where(where).select({ "id", oc }).order_by(col(oc), dir).limit(*a.limit_value()).cache(false);
                const ResultSet ra = db.query(a), rb = db.query(b);
                bool same = ra.size() == rb.size();
                for (std::size_t i = 0; same && i < ra.size(); ++i) same = ra[i][1] == rb[i][1];
                ++queries;
                if (ra.stats.used_index) ++indexed;

                if (!same) {
                    ++mismatches;
                    if (mismatches <= 5) std::printf("  ORDER MISMATCH %s by %s\n    plan %s | %s\n", where.to_string().c_str(), oc, ra.stats.plan.c_str(), rb.stats.plan.c_str());
                }
            }

            if (rnd(0, 1)) query.cache(false);
            const ResultSet got = db.query(query);
            Query pq("plain");
            pq.where(where).select({ "id" });
            for (const auto& o : query.ordering()) pq.order_by(o.expr, o.order);
            if (query.limit_value()) pq.limit(*query.limit_value());
            pq.offset(query.offset_value()).cache(false);
            const ResultSet plain = db.query(pq);
            ++queries;
            if (got.stats.used_index) ++indexed;
            bool ok = got.rows.size() == plain.rows.size();
            for (std::size_t i = 0; ok && (shape == 1 || shape == 2) && i < got.rows.size(); ++i) ok = got.rows[i][0] == plain.rows[i][0];

            if (shape != 2) {
                std::vector<std::int64_t> ids;
                for (const auto& r : got.rows) ids.push_back(r[0].get_int());
                std::sort(ids.begin(), ids.end());
                std::sort(expect.begin(), expect.end());
                if (ids != expect) ok = false;
            }

            const Value cnt = db.query(Query("t").where(where).count()).scalar();
            if (cnt != Value(static_cast<std::int64_t>(expect.size()))) ok = false;

            if (!ok) {
                ++mismatches;
                if (mismatches <= 5) {
                    std::printf("  MISMATCH shape %d %s\n    plan %s | plain %s\n    got %zu plain %zu expect %zu count %s\n", shape, where.to_string().c_str(), got.stats.plan.c_str(), plain.stats.plan.c_str(), got.rows.size(), plain.rows.size(), expect.size(), cnt.to_string().c_str());
                    for (std::size_t i = 0; i < std::min<std::size_t>(got.rows.size(), 8); ++i) std::printf("    %s/%s", got.rows[i][0].to_string().c_str(), i < plain.rows.size() ? plain.rows[i][0].to_string().c_str() : "-");
                    std::printf("\n");
                }
            }
        }

        mutate();
    }

    std::printf("  %d random queries, %d used an index, %d mismatches\n", queries, indexed, mismatches);
    CHECK(mismatches == 0);
    CHECK(indexed > queries / 4);
    CHECK(dump_db(db, "t") == dump_db(db, "plain"));
}

static void test_aggregates_joins() {
    Database db;
    db.create_table("users", Schema{ integer("id").primary(), text("name").not_null() });
    db.create_table("recipes", Schema{ integer("id").primary().autoincrement(), integer("owner").not_null(), text("title").not_null(), real("rating"), integer("minutes") });
    db.create_table("tags", Schema{ integer("recipe"), text("tag") });
    db.create_index("recipes", { "owner" }, IndexKind::Hash);
    db.insert_many("users", { { 1, "ann" }, { 2, "bob" }, { 3, "cat" } });
    std::vector<Row> recipes;
    for (int i = 0; i < 60; ++i) recipes.push_back({ Value(), Value(rnd(1, 2)), Value("r" + std::to_string(i)), i % 7 == 0 ? Value() : Value(static_cast<double>(rnd(1, 50)) / 10.0), Value(rnd(5, 90)) });
    db.insert_many("recipes", recipes);
    for (int i = 0; i < 80; ++i) db.insert_row("tags", { Value(rnd(1, 70)), Value(kWords[rnd(0, 4)]) });

    const ResultSet g = db.query(Query("recipes").select(col("owner"), "owner").count().sum(col("minutes"), "total").avg(col("rating"), "avg").min(col("rating"), "lo").max(col("minutes"), "hi").group_by(col("owner")).order_by(col("owner")));
    std::map<std::int64_t, std::vector<Row>> by;
    for (const auto& r : db.select("recipes")) by[r[1].get_int()].push_back(r);
    bool agg_ok = g.size() == by.size();
    std::size_t gi = 0;

    for (const auto& kv : by) {
        std::int64_t total = 0, hi = -1;
        double sum = 0;
        int n = 0;
        Value lo;

        for (const auto& r : kv.second) {
            total += r[4].get_int();
            hi = std::max(hi, r[4].get_int());
            if (!r[3].is_null()) { sum += r[3].get_real(); ++n; if (lo.is_null() || r[3] < lo) lo = r[3]; }
        }

        if (gi >= g.size()) { agg_ok = false; break; }
        const Row& row = g[gi++];
        if (row[0] != Value(kv.first) || row[1] != Value(static_cast<std::int64_t>(kv.second.size())) || row[2] != Value(total) || row[5] != Value(hi) || row[4] != lo) agg_ok = false;
        if (std::abs(row[3].as_real() - sum / n) > 1e-9) agg_ok = false;
    }

    CHECK(agg_ok);
    std::printf("  %s", g.to_text().c_str());

    const ResultSet having = db.query(Query("recipes").select(col("owner"), "owner").count("n").group_by(col("owner")).having(col("n") > 1000));
    CHECK(having.empty());
    CHECK(db.query(Query("recipes").count().where(col("minutes") > 1000)).scalar() == Value(0));
    CHECK(db.query(Query("recipes").max(col("minutes"))).scalar() == db.query(Query("recipes").select({ "minutes" }).order_by_desc(col("minutes")).limit(1)).scalar());

    const ResultSet j = db.query(Query("recipes", "r").join("users", col("r.owner"), col("u.id"), JoinKind::Inner, "u").select(col("r.title"), "title").select(col("u.name"), "name").where(col("u.name") == "bob").order_by(col("r.id")));
    std::size_t expect = 0;
    for (const auto& r : db.select("recipes")) if (r[1] == Value(2)) ++expect;
    CHECK(j.size() == expect && (j.empty() || j.at(0, "name") == Value("bob")));
    std::printf("  join plan: %s\n", j.stats.plan.c_str());

    const ResultSet lj = db.query(Query("recipes").left_join("tags", col("recipes.id"), col("tags.recipe")).select(col("recipes.id"), "id").select(col("tags.tag"), "tag"));
    std::size_t brute = 0;
    const auto tags = db.select("tags");

    for (const auto& r : db.select("recipes")) {
        std::size_t m = 0;
        for (const auto& t : tags) if (t[0] == r[0]) ++m;
        brute += m ? m : 1;
    }

    CHECK(lj.size() == brute);
    std::printf("  left join plan: %s (%zu rows)\n", lj.stats.plan.c_str(), lj.size());

    const ResultSet uj = db.query(Query("users").join("recipes", col("users.id"), col("recipes.owner")).select(col("users.name"), "name").count("n").group_by(col("users.name")).order_by(col("n"), SortOrder::Desc));
    CHECK(uj.size() == 2);
    const ResultSet d = db.query(Query("tags").select({ "tag" }).distinct().order_by(col("tag")));
    std::set<std::string> distinct_tags;
    for (const auto& t : tags) distinct_tags.insert(t[1].get_text());
    CHECK(d.size() == distinct_tags.size());
}

static void test_transactions_cache() {
    Database db;
    db.create_table("acct", Schema{ integer("id").primary(), integer("balance").not_null(), text("owner") });
    db.insert_many("acct", { { 1, 100, "a" }, { 2, 50, "b" }, { 3, 0, "c" } });
    const auto before = dump_db(db, "acct");

    try {
        db.transaction([&](Transaction& tx) {
            tx.update("acct", col("id") == 1, { { "balance", 40 } });
            tx.update("acct", col("id") == 2, { { "balance", 110 } });
            tx.remove("acct", col("id") == 3);
            tx.insert("acct", { { "id", 4 }, { "balance", 9 } });
            tx.create_index("acct", IndexDef{ "ix_owner", { "owner" }, IndexKind::Ordered, false });
            tx.insert("acct", { { "id", 1 }, { "balance", 1 } });
        });
    } catch (const ConstraintError&) {
    }

    CHECK(dump_db(db, "acct") == before);
    CHECK(db.read("acct", [](const Table& t) { return t.index("ix_owner") == nullptr; }));

    {
        Transaction tx = db.begin();
        tx.update("acct", col("id") == 1, { { "balance", 70 } });
        tx.update("acct", col("id") == 2, { { "balance", 80 } });
        CHECK(tx.query(Query("acct").sum(col("balance"))).scalar() == Value(150));
        tx.commit();
    }

    CHECK(db.query(Query("acct").sum(col("balance"))).scalar() == Value(150));

    Query q = Query("acct").where(col("balance") > 10).select({ "id" });
    const ResultSet first = db.query(q);
    const ResultSet second = db.query(q);
    CHECK(!first.stats.cache_hit && second.stats.cache_hit);
    db.insert("acct", { { "id", 9 }, { "balance", 500 } });
    const ResultSet third = db.query(q);
    CHECK(!third.stats.cache_hit && third.size() == first.size() + 1);
    Statement st = db.prepare(Query("acct").where(col("balance") > param(0)).count());
    CHECK(st.run({ 60 }).scalar() == Value(3));
    CHECK(st.run({ 1000 }).scalar() == Value(0));
    std::size_t hooks = 0;
    db.on_query([&](const Query&, const QueryStats&) { ++hooks; });
    db.query(q);
    CHECK(hooks == 1);
    std::printf("  cache: %zu hits, %zu misses\n", db.stats().cache.hits, db.stats().cache.misses);
}

static void test_persistence() {
    const std::string dir = (std::filesystem::temp_directory_path() / "fizmo_db_test").string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string path = dir + "/app";
    std::vector<Row> expected;

    {
        auto db = Database::open(path);
        db->create_table("notes", Schema{ integer("id").primary().autoincrement(), text("body"), real("score"), blob("data") });
        db->create_index("notes", { "score" });
        for (int i = 0; i < 500; ++i) db->insert("notes", { { "body", "note " + std::to_string(i) }, { "score", i * 0.5 }, { "data", Value::blob(std::string("\x00\x01\xff", 3)) } });
        db->remove("notes", col("id") % 3 == 0);
        db->update("notes", col("score") > 200.0, { { "body", "big" } });
        try { db->transaction([&](Transaction& tx) { tx.insert("notes", { { "body", "ghost" } }); throw DatabaseError("abort"); }); } catch (const DatabaseError&) {}
        expected = dump_db(*db, "notes");
    }

    {
        auto db = Database::open(path);
        CHECK(dump_db(*db, "notes") == expected);
        CHECK(db->read("notes", [](const Table& t) { return t.index("ix_notes_score") != nullptr; }));
        db->checkpoint();
        db->insert("notes", { { "body", "after checkpoint" } });
        expected = dump_db(*db, "notes");
        CHECK(expected.back()[0].get_int() > 500);
    }

    {
        auto db = Database::open(path);
        CHECK(dump_db(*db, "notes") == expected);
        db->insert("notes", { { "body", "will be torn" } });
    }

    const auto wal = path + ".wal";
    const auto size = std::filesystem::file_size(wal);
    std::filesystem::resize_file(wal, size - 5);

    {
        auto db = Database::open(path);
        CHECK(dump_db(*db, "notes") == expected);
        db->insert("notes", { { "body", "fresh" } });
        expected = dump_db(*db, "notes");
    }

    {
        auto db = Database::open(path);
        CHECK(dump_db(*db, "notes") == expected);
        CHECK(db->count("notes", col("body") == "fresh") == 1);
    }

    std::filesystem::remove_all(dir);
}

static void test_graph() {
    Database db;
    db.create_table("edges", Schema{ text("src"), text("dst"), real("w") });
    db.insert_many("edges", { { "a", "b", 1.0 }, { "b", "c", 2.0 }, { "a", "c", 5.0 }, { "c", "d", 1.0 }, { "d", "e", 1.0 }, { "x", "y", 1.0 } });
    GraphOptions o;
    o.from = "src";
    o.to = "dst";
    auto g = db.graph("edges", o);
    CHECK(g->node_count() == 7 && g->edge_count() == 6);
    auto bfs = g->bfs("a");
    CHECK(bfs.size() == 5 && bfs[0].node == Value("a") && bfs.back().node == Value("e") && bfs.back().depth == 3);
    auto dfs = g->dfs("a");
    CHECK(dfs.size() == 5);
    auto path = g->shortest_path("a", "e");
    CHECK(path && path.nodes.size() == 4 && path.cost == 3.0);
    CHECK(!g->shortest_path("a", "x"));
    CHECK(g->reachable("b", "e") && !g->reachable("e", "a"));
    CHECK(g->components().size() == 2);
    auto topo = g->topological_order();
    CHECK(topo && topo->size() == 7);
    o.weight = "w";
    auto wg = db.graph("edges", o);
    auto wp = wg->shortest_path("a", "c");
    CHECK(wp && wp.cost == 3.0 && wp.nodes.size() == 3);
    CHECK(db.graph("edges", o) == wg);
    db.insert_row("edges", { "e", "a", 1.0 });
    auto again = db.graph("edges", o);
    CHECK(again != wg && !again->topological_order());
}

static bool throws(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const DatabaseError&) {
        return true;
    }
    return false;
}

static bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

static void test_partitions_time() {
    const std::int64_t day = 86400;
    const std::int64_t t0 = 1792800000;
    std::int64_t now = t0 + 12 * 3600;
    Database db;
    std::vector<PartitionInfo> dropped_by_hook;

    db.create_partitioned_table("events", Schema{ integer("id").primary().autoincrement(), integer("ts").not_null(), text("kind"), integer("user") },
        PartitionOptions::daily("ts").keep_for(std::chrono::hours(24 * 3)).use_clock([&] { return now; }).when_dropped([&](const PartitionInfo& i, const Table&) { dropped_by_hook.push_back(i); }));
    db.create_index("events", { "ts" });
    db.create_index("events", { "user" }, IndexKind::Hash);
    CHECK(db.is_partitioned("events"));
    CHECK(db.has_table("events"));
    CHECK(db.partitions("events").empty());

    for (int d = -2; d <= 0; ++d) {
        for (int k = 0; k < 10; ++k) db.insert("events", { { "ts", t0 + d * day + k * 3600 }, { "kind", k % 2 ? "click" : "view" }, { "user", k % 3 } });
    }

    const auto parts = db.partitions("events");
    CHECK(parts.size() == 3);
    CHECK(parts.size() == 3 && parts[0].name == "2026-10-22" && parts[2].name == "2026-10-24" && parts[1].rows == 10);
    CHECK(db.count("events") == 30);
    CHECK(db.tables() == std::vector<std::string>{ "events" });
    CHECK(throws([&] { db.insert("events", { { "ts", t0 - 5 * day } }); }));
    CHECK(db.count("events") == 30);
    CHECK(db.partitions("events").size() == 3);
    CHECK(throws([&] { db.insert("events", { { "kind", "x" } }); }));

    auto one_day = db.query(Query("events").where(col("ts") >= t0 - day && col("ts") < t0).count());
    CHECK(one_day.scalar() == Value(10));
    CHECK(has(one_day.stats.plan, "PARTITIONS 1 of 3"));
    std::printf("  plan: %s\n", one_day.stats.plan.c_str());

    auto latest = db.query(Query("events").order_by_desc(col("ts")).limit(3));
    CHECK(latest.size() == 3 && latest.at(0, "ts") == Value(t0 + 9 * 3600) && latest.at(2, "ts") == Value(t0 + 7 * 3600));
    CHECK(has(latest.stats.plan, "PARTITION BY PARTITION") && has(latest.stats.plan, "STOP AFTER 3"));
    std::printf("  plan: %s\n", latest.stats.plan.c_str());
    auto oldest = db.query(Query("events").order_by(col("ts")).limit(12).offset(8));
    CHECK(oldest.size() == 12 && oldest.at(0, "ts") == Value(t0 - 2 * day + 8 * 3600) && oldest.at(2, "ts") == Value(t0 - day));

    auto ids = db.query(Query("events").select({ "id" }).order_by(col("id")));
    bool increasing = ids.size() == 30;
    for (std::size_t i = 0; increasing && i < ids.size(); ++i) increasing = ids[i][0] == Value(static_cast<std::int64_t>(i + 1));
    CHECK(increasing);
    CHECK(throws([&] { db.insert("events", { { "id", 5 }, { "ts", t0 } }); }));
    CHECK(db.get("events", { 15 }).has_value() && (*db.get("events", { 15 }))[1] == Value(t0 - day + 4 * 3600));
    CHECK(db.query(Query("events").where(col("user") == 1).count()).scalar() == Value(9));
    CHECK(db.query(Query("events").min(col("ts"))).scalar() == Value(t0 - 2 * day));
    CHECK(db.query(Query("events").max(col("ts"))).scalar() == Value(t0 + 9 * 3600));
    CHECK(db.table_stats("events").rows == 30);
    CHECK(throws([&] { db.insert("events__p1", { { "ts", t0 } }); }));
    CHECK(throws([&] { auto tx = db.begin(); tx.update_row("events", 0, { { "kind", "x" } }); }));
    CHECK(throws([&] { db.graph("events"); }));

    db.create_table("users", Schema{ integer("id").primary(), text("name") });
    db.insert_row("users", { 0, "ann" });
    db.insert_row("users", { 1, "bo" });
    db.insert_row("users", { 2, "cy" });
    auto j1 = db.query(Query("users").join("events", col("users.id"), col("events.user")).where(col("users.name") == "bo").count());
    CHECK(j1.scalar() == Value(9));
    std::printf("  plan: %s\n", j1.stats.plan.c_str());
    auto j2 = db.query(Query("events").join("users", col("events.user"), col("users.id")).where(col("events.ts") >= t0 && col("users.name") != "cy").count());
    CHECK(j2.scalar() == Value(7) && has(j2.stats.plan, "PARTITIONS 1 of 3"));
    auto g = db.query(Query("events").select({ "kind" }).count("n").group_by(col("kind")).order_by(col("kind")));
    CHECK(g.size() == 2 && g.at(0, "n") == Value(15) && g.at(1, "kind") == Value("view"));

    const Query cached = Query("events").count();
    CHECK(db.query(cached).scalar() == Value(30));
    CHECK(db.query(cached).stats.cache_hit);
    now += 2 * day;
    CHECK(db.query(cached).scalar() == Value(20));
    CHECK(db.partitions("events").size() == 3 && db.partitions("events")[0].expired);
    const auto swept = db.enforce_retention();
    CHECK(swept.size() == 1 && swept[0].reason == DropReason::Age && swept[0].name == "2026-10-22" && swept[0].rows == 10);
    CHECK(dropped_by_hook.size() == 1 && dropped_by_hook[0].rows == 10);
    CHECK(db.partitions("events").size() == 2 && db.count("events") == 20);
    CHECK(db.get("events", { 3 }) == std::nullopt);

    CHECK(db.update("events", col("id") == 25, { { "ts", t0 + day + 60 } }) == 1);
    CHECK(db.partitions("events").size() == 3 && db.partitions("events")[2].name == "2026-10-25" && db.partitions("events")[2].rows == 1);
    CHECK(db.count("events", col("ts") >= t0 + day) == 1);
    CHECK(db.get("events", { 25 }).has_value());
    CHECK(throws([&] { db.update("events", col("id") == 25, { { "ts", t0 - 9 * day } }); }));
    CHECK(db.count("events", col("id") == 25 && col("ts") == t0 + day + 60) == 1);
    CHECK(throws([&] { db.update("events", col("id") == 26, { { "id", 12 } }); }));
    CHECK(db.update("events", col("kind") == "click", { { "kind", "tap" } }) == 10);
    CHECK(db.count("events", col("kind") == "tap") == 10);

    {
        auto tx = db.begin();
        tx.insert("events", { { "ts", t0 + 3 * day } });
        tx.insert("events", { { "ts", t0 + 4 * day } });
        tx.rollback();
    }

    CHECK(db.partitions("events").size() == 3 && db.count("events") == 20);

    const std::size_t gone = db.drop_partitions_before("events", Value(t0));
    CHECK(gone == 1 && db.count("events") == 10);
    CHECK(db.drop_partition("events", "2026-10-25") && db.count("events") == 9);
    CHECK(!db.drop_partition("events", "1999-01-01"));
    CHECK(dropped_by_hook.size() == 3);

    db.set_partition_options("events", PartitionOptions::daily("ts").keep_for(std::chrono::hours(48)).ignore_late_rows());
    CHECK(db.partitions("events").size() == 1);
    CHECK(db.insert("events", { { "ts", t0 - day } }) == kNoRow);
    CHECK(db.count("events") == 9);
    CHECK(throws([&] { db.set_partition_options("events", PartitionOptions::hourly("ts")); }));

    db.set_partition_options("events", PartitionOptions::daily("ts").drop_when_empty());
    CHECK(db.remove("events", col("ts") >= t0) == 9);
    CHECK(db.partitions("events").empty() && db.count("events") == 0);
    CHECK(db.query(Query("events").order_by(col("ts")).limit(5)).empty());
    CHECK(db.query(Query("events").max(col("ts"))).scalar().is_null());
    CHECK(db.insert("events", { { "ts", t0 } }) != kNoRow);
    CHECK(db.query(Query("events").select({ "id" })).at(0, "id") == Value(33));
    CHECK(db.drop_table("events") && !db.has_table("events"));
}

static void test_partitions_shapes() {
    Database db;
    db.create_partitioned_table("orders", Schema{ integer("id").primary().autoincrement(), text("region").not_null(), integer("cents") },
        PartitionOptions::list("region").keep_partitions(3));
    db.create_index("orders", { "cents" });
    const char* regions[] = { "east", "west", "north", "south" };

    for (int i = 0; i < 40; ++i) db.insert("orders", { { "region", regions[i % 3] }, { "cents", i * 10 } });

    CHECK(db.partitions("orders").size() == 3);
    auto west = db.query(Query("orders").where(col("region") == "west").count());
    CHECK(west.scalar() == Value(13) && has(west.stats.plan, "PARTITIONS 1 of 3"));
    auto two = db.query(Query("orders").where(col("region").in({ "east", "north", "mars" })).count());
    CHECK(two.scalar() == Value(27) && has(two.stats.plan, "PARTITIONS 2 of 3"));
    auto pre = db.query(Query("orders").where(col("region").like("no%")).count());
    CHECK(pre.scalar() == Value(13) && has(pre.stats.plan, "PARTITIONS 1 of 3"));
    auto none = db.query(Query("orders").where(col("region") == "mars").count());
    CHECK(none.scalar() == Value(0) && has(none.stats.plan, "PARTITIONS 0 of 3"));
    db.insert("orders", { { "region", "south" }, { "cents", 1 } });
    const auto lp = db.partitions("orders");
    CHECK(lp.size() == 3 && lp[0].name == "west" && lp[2].name == "south");
    CHECK(db.count("orders") == 27);
    CHECK(db.update("orders", col("region") == "west", { { "region", "south" } }) == 13);
    CHECK(db.count("orders", col("region") == "south") == 14 && db.partitions("orders").size() == 3 && db.partitions("orders")[0].rows == 0);

    db.set_partition_options("orders", PartitionOptions::list("region").keep_rows(10));
    CHECK(db.partitions("orders").size() == 1 && db.count("orders") == 14);

    db.create_partitioned_table("monthly", Schema{ integer("ts").not_null(), integer("v") }, PartitionOptions::monthly("ts", TimeUnit::Milliseconds));
    const std::int64_t dec31 = 1798675200LL * 1000 + 3600000LL * 23;
    db.insert("monthly", { { "ts", dec31 }, { "v", 1 } });
    db.insert("monthly", { { "ts", dec31 + 3600000LL } , { "v", 2 } });
    const auto mp = db.partitions("monthly");
    CHECK(mp.size() == 2 && mp[0].name == "2026-12" && mp[1].name == "2027-01" && mp[1].lo == Value(1798761600LL * 1000));

    db.create_partitioned_table("yearly", Schema{ integer("ts").not_null() }, PartitionOptions::yearly("ts"));
    db.insert("yearly", { { "ts", 1798675200LL } });
    CHECK(db.partitions("yearly").size() == 1 && db.partitions("yearly")[0].name == "2026");

    db.create_partitioned_table("weekly", Schema{ integer("ts").not_null() }, PartitionOptions::weekly("ts"));
    db.insert("weekly", { { "ts", 1792281600LL + 3600 } });
    CHECK(db.partitions("weekly")[0].lo == Value(1792281600LL - 6 * 86400) && db.partitions("weekly")[0].name == "2026-10-12");
    db.insert("weekly", { { "ts", 1792368000LL } });
    CHECK(db.partitions("weekly").size() == 2 && db.partitions("weekly")[1].name == "2026-10-19");

    db.create_partitioned_table("hours", Schema{ integer("ts").not_null() }, PartitionOptions::hourly("ts"));
    db.insert("hours", { { "ts", 1792800000LL + 7200 + 5 } });
    CHECK(db.partitions("hours")[0].name == "2026-10-24T02");

    db.create_partitioned_table("nums", Schema{ integer("n").not_null(), text("s") }, PartitionOptions::range("n", 100).keep_for(Value(300)));
    for (int n = 0; n < 1000; n += 50) db.insert("nums", { { "n", n } });
    CHECK(db.partitions("nums").size() == 4 && db.partitions("nums")[0].name == "[600, 700)");
    CHECK(throws([&] { db.insert("nums", { { "n", 550 } }); }));
    CHECK(db.insert("nums", { { "n", 650 } }) != kNoRow);
    db.insert("nums", { { "n", 1150 } });
    CHECK(db.partitions("nums").size() == 3 && db.partitions("nums")[0].name == "[800, 900)" && db.count("nums") == 5);
    CHECK(throws([&] { db.insert("nums", { { "n", -5 } }); }));

    db.create_partitioned_table("neg", Schema{ integer("n").not_null() }, PartitionOptions::range("n", 10));
    db.insert("neg", { { "n", -1 } });
    db.insert("neg", { { "n", -10 } });
    db.insert("neg", { { "n", -11 } });
    CHECK(db.partitions("neg").size() == 2 && db.partitions("neg")[0].lo == Value(-20) && db.partitions("neg")[1].hi == Value(0));
    CHECK(db.count("neg", col("n").between(-10, -1)) == 2);

    db.create_partitioned_table("reals", Schema{ real("x").not_null() }, PartitionOptions::range("x", 0.5));
    for (double x : { 0.1, 0.4, 0.6, 1.49, -0.2 }) db.insert("reals", { { "x", x } });
    CHECK(db.partitions("reals").size() == 4 && db.count("reals", col("x") < 0.5) == 3);
    CHECK(has(db.query(Query("reals").where(col("x") > 1.0).count()).stats.plan, "PARTITIONS 1 of 4"));

    db.create_partitioned_table("cap", Schema{ integer("n").not_null(), text("pad") }, PartitionOptions::range("n", 10).keep_bytes(1));
    for (int n = 0; n < 50; ++n) db.insert("cap", { { "n", n }, { "pad", std::string(64, 'x') } });
    CHECK(db.partitions("cap").size() == 1 && db.partitions("cap")[0].lo == Value(40));

    db.create_table("plain_t", Schema{ integer("a") });
    CHECK(throws([&] { db.create_partitioned_table("plain_t", Schema{ integer("a") }, PartitionOptions::range("a", 5)); }));
    CHECK(throws([&] { db.create_partitioned_table("bad1", Schema{ text("a") }, PartitionOptions::range("a", 5)); }));
    CHECK(throws([&] { db.create_partitioned_table("bad2", Schema{ integer("a") }, PartitionOptions::range("a", 0)); }));
    CHECK(throws([&] { db.create_partitioned_table("bad3", Schema{ integer("a") }, PartitionOptions::range("b", 5)); }));
    CHECK(throws([&] { db.create_partitioned_table("bad4", Schema{ text("a") }, PartitionOptions::list("a").keep_for(Value(3))); }));
    CHECK(throws([&] { db.create_partitioned_table("bad5", Schema{ integer("a") }, PartitionOptions::range("a", 5).relative_to_clock().keep_for(Value(3))); }));
    CHECK(throws([&] { db.partitions("plain_t"); }));
    CHECK(throws([&] { PartitionOptions::range("a", 5).keep_for(std::chrono::hours(1)); }));
    CHECK(db.stats().partitioned_tables == 9);
}

static void test_partitions_persistence() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "fizmo_db_partition_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string path = (dir / "p").string();
    const std::int64_t t0 = 1792800000;
    std::int64_t now = t0;
    auto clock = [&] { return now; };

    {
        auto db = Database::open(path);
        db->create_partitioned_table("log", Schema{ integer("id").primary().autoincrement(), integer("ts").not_null(), text("msg") }, PartitionOptions::daily("ts").keep_for(std::chrono::hours(48)).use_clock(clock));
        db->create_index("log", { "msg" });
        for (int d = -1; d <= 0; ++d) for (int k = 0; k < 5; ++k) db->insert("log", { { "ts", t0 + d * 86400 + k }, { "msg", "m" + std::to_string(k) } });
        db->checkpoint();
        db->insert("log", { { "ts", t0 + 86400 } });
        db->update("log", col("id") == 1, { { "ts", t0 + 2 } });
        db->remove("log", col("id") == 2);
        now = t0 + 2 * 86400;
        CHECK(db->enforce_retention("log").size() == 1);
        db->insert("log", { { "ts", now }, { "msg", "late" } });
    }

    {
        auto db = Database::open(path);
        db->set_partition_clock("log", clock);
        CHECK(db->is_partitioned("log"));
        const auto parts = db->partitions("log");
        CHECK(parts.size() == 3 && parts[0].name == "2026-10-24" && parts[2].name == "2026-10-26");
        CHECK(db->count("log") == 8);
        CHECK(db->count("log", col("msg") == "m3") == 1);
        CHECK(has(db->explain(Query("log").where(col("msg") == "m3")), "ix_log_msg"));
        const Value id = db->query(Query("log").max(col("id"))).scalar();
        db->insert("log", { { "ts", now } });
        CHECK(db->query(Query("log").max(col("id"))).scalar() == Value(id.as_int() + 1));
        CHECK(db->get("log", { 1 }).has_value());
        db->checkpoint();
    }

    {
        auto db = Database::open(path);
        CHECK(db->count("log") == 9 && db->partitions("log").size() == 3);
        CHECK(db->partition_options("log").retention.has_value());
        CHECK(throws([&] { db->insert("log", { { "id", 1 }, { "ts", t0 + 86400 } }); }));
    }

    fs::remove_all(dir);
}

static void test_partition_sweeper() {
    std::atomic<std::int64_t> now{ 1792800000 };
    Database db;
    std::atomic<int> drops{ 0 };
    db.create_partitioned_table("pings", Schema{ integer("ts").not_null() }, PartitionOptions::hourly("ts").keep_for(std::chrono::hours(2)).use_clock([&] { return now.load(); }).when_dropped([&](const PartitionInfo&, const Table&) { ++drops; }));
    for (int h = 0; h < 3; ++h) db.insert("pings", { { "ts", now.load() - h * 3600 } });
    CHECK(db.partitions("pings").size() == 3);
    db.start_retention(std::chrono::milliseconds(10));
    CHECK(db.retention_running());
    now += 5 * 3600;
    bool gone = false;

    for (int i = 0; i < 300 && !gone; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        gone = db.partitions("pings").empty();
    }

    CHECK(gone && drops.load() == 3);
    db.stop_retention();
    CHECK(!db.retention_running());
    db.start_retention(std::chrono::milliseconds(5));
}

static void test_partitions_fuzz() {
    Database db;
    db.create_table("plain", fuzz_schema());
    db.create_partitioned_table("pl", fuzz_schema(), PartitionOptions::list("g"));
    db.create_partitioned_table("pr", fuzz_schema(), PartitionOptions::range("id", 97));
    db.create_partitioned_table("pg", fuzz_schema(), PartitionOptions::range("g", 2));

    for (const char* t : { "pl", "pr", "pg" }) {
        db.create_index(t, { "a" }, IndexKind::Ordered);
        db.create_index(t, { "c" }, IndexKind::Ordered);
        db.create_index(t, { "id" }, IndexKind::Ordered);
        db.create_index(t, { "d" }, IndexKind::Hash);
        db.create_index(t, { "g", "a" }, IndexKind::Ordered);
    }

    std::vector<Row> rows;
    for (int i = 0; i < 2500; ++i) rows.push_back(random_row());
    for (const char* t : { "plain", "pl", "pr", "pg" }) db.insert_many(t, rows);
    const char* tables[] = { "pl", "pr", "pg" };
    int queries = 0, mismatches = 0;

    auto ids_of = [](const ResultSet& r) {
        std::vector<std::int64_t> v;
        for (const auto& row : r.rows) v.push_back(row[0].get_int());
        return v;
    };

    for (int round = 0; round < 5; ++round) {
        for (int q = 0; q < 200; ++q) {
            const Ref ref = random_pred(static_cast<int>(rnd(0, 3)));
            const Expr where = to_expr(ref);
            const int shape = static_cast<int>(rnd(0, 4));

            auto build = [&](const char* t) {
                Query query(t);
                query.where(where).select({ "id" }).cache(false);
                return query;
            };

            Query base = build("plain");
            std::vector<Query> others;
            for (const char* t : tables) others.push_back(build(t));
            const SortOrder dir = rnd(0, 1) ? SortOrder::Asc : SortOrder::Desc;
            const std::size_t lim = static_cast<std::size_t>(rnd(0, 25));
            const std::size_t off = static_cast<std::size_t>(rnd(0, 4));
            const char* oc = kCols[rnd(1, 5)];

            auto shape_it = [&](Query& query) {
                if (shape == 1) query.order_by(col(oc), dir).order_by(col("id"));
                if (shape == 2) query.order_by(col("id"), dir).limit(lim).offset(off);
                if (shape == 3) query.order_by(col("g"), dir).order_by(col("id"), dir).limit(lim);
                if (shape == 4) query.order_by(col("a"), dir).order_by(col("id")).limit(lim).offset(off);
            };

            shape_it(base);
            for (auto& o : others) shape_it(o);
            const auto want = ids_of(db.query(base));
            const Value want_count = db.query(Query("plain").where(where).count().cache(false)).scalar();

            for (std::size_t k = 0; k < others.size(); ++k) {
                ResultSet got = db.query(others[k]);
                auto ids = ids_of(got);
                auto expect = want;

                if (shape == 0) {
                    std::sort(ids.begin(), ids.end());
                    std::sort(expect.begin(), expect.end());
                }

                const Value cnt = db.query(Query(tables[k]).where(where).count().cache(false)).scalar();
                ++queries;

                if (ids != expect || cnt != want_count) {
                    ++mismatches;
                    if (mismatches <= 5) std::printf("  MISMATCH %s shape %d %s\n    plan %s\n    got %zu want %zu count %s/%s\n", tables[k], shape, where.to_string().c_str(), got.stats.plan.c_str(), ids.size(), expect.size(), cnt.to_string().c_str(), want_count.to_string().c_str());
                }
            }

            if (q % 20 == 0) {
                Query ga = Query("plain").select({ "g" }).count("n").sum(col("a"), "s").min(col("c"), "lo").group_by(col("g")).where(where).order_by(col("g")).cache(false);
                const ResultSet a = db.query(ga);

                for (const char* t : tables) {
                    Query gb = Query(t).select({ "g" }).count("n").sum(col("a"), "s").min(col("c"), "lo").group_by(col("g")).where(where).order_by(col("g")).cache(false);
                    const ResultSet b = db.query(gb);
                    ++queries;
                    if (a.rows != b.rows) { ++mismatches; if (mismatches <= 5) std::printf("  AGG MISMATCH %s %s\n", t, where.to_string().c_str()); }
                }
            }
        }

        for (int k = 0; k < 150; ++k) {
            const int what = static_cast<int>(rnd(0, 3));

            if (what == 0) {
                const Row r = random_row();
                for (const char* t : { "plain", "pl", "pr", "pg" }) db.insert_row(t, r);
            } else if (what == 1) {
                const std::int64_t id = rnd(1, 2800);
                for (const char* t : { "plain", "pl", "pr", "pg" }) db.remove(t, col("id") == id);
            } else if (what == 2) {
                const std::int64_t id = rnd(1, 2800);
                const Row r = random_row();
                const NamedValues ch = { { "a", r[1] }, { "c", r[3] }, { "g", r[5] } };
                for (const char* t : { "plain", "pl", "pr", "pg" }) db.update(t, col("id") == id, ch);
            } else {
                const std::int64_t lo = rnd(1, 2800);
                const Value g(rnd(0, 4));
                for (const char* t : { "plain", "pl", "pr", "pg" }) db.update(t, col("id").between(lo, lo + 40), { { "g", g } });
            }
        }

        db.maintain();
    }

    auto all = [&](const char* t) { return db.query(Query(t).order_by(col("id")).cache(false)).rows; };
    const auto truth = all("plain");
    for (const char* t : tables) CHECK(all(t) == truth);
    std::printf("  %d random queries over 3 partitioned tables, %d mismatches; pl has %zu parts, pr %zu, pg %zu\n", queries, mismatches, db.partitions("pl").size(), db.partitions("pr").size(), db.partitions("pg").size());
    CHECK(mismatches == 0);
}

template <typename Fn>
static double time_ms(Fn&& fn, int reps = 1) {
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; ++i) fn();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / reps;
}

static void benchmarks() {
    const int n = 200000;
    Database db;
    db.create_table("items", Schema{ integer("id").primary().autoincrement(), integer("owner").not_null(), integer("price"), text("name"), real("score") });
    std::vector<Row> rows;
    rows.reserve(n);
    for (int i = 0; i < n; ++i) rows.push_back({ Value(), Value(rnd(1, 5000)), Value(rnd(1, 100000)), Value(std::string(kWords[rnd(0, 17)]) + std::to_string(i)), Value(static_cast<double>(rnd(0, 1000000)) / 100.0) });
    const double insert_ms = time_ms([&] { db.insert_many("items", rows); });
    std::printf("  insert %d rows: %.1f ms (%.0f rows/s)\n", n, insert_ms, n / (insert_ms / 1000.0));
    db.set_cache(0, 0);

    auto run = [&](const char* label, const Query& q, int reps) {
        ResultSet last;
        const double ms = time_ms([&] { last = db.query(q); }, reps);
        std::printf("  %-44s %9.3f ms  rows=%-6zu %s\n", label, ms, last.size(), last.stats.plan.c_str());
        return ms;
    };

    const double scan_owner = run("owner = 42 (full scan)", Query("items").where(col("owner") == 42), 20);
    const double scan_range = run("price BETWEEN 500 AND 600 (full scan)", Query("items").where(col("price").between(500, 600)), 20);
    const double scan_sort = run("ORDER BY price LIMIT 10 (scan + top-N)", Query("items").order_by(col("price")).limit(10), 10);
    const double scan_count = run("COUNT price > 99000 (scan)", Query("items").where(col("price") > 99000).count(), 20);
    run("name LIKE 'kiwi1%' (scan)", Query("items").where(col("name").like("kiwi1%")), 10);
    const double idx_ms = time_ms([&] {
        db.create_index("items", { "owner" }, IndexKind::Hash);
        db.create_index("items", { "price" }, IndexKind::Ordered);
        db.create_index("items", { "name" }, IndexKind::Ordered);
    });
    std::printf("  build 3 indexes over %d rows: %.1f ms\n", n, idx_ms);
    const double idx_owner = run("owner = 42 (hash index)", Query("items").where(col("owner") == 42), 2000);
    const double idx_range = run("price BETWEEN 500 AND 600 (ordered index)", Query("items").where(col("price").between(500, 600)), 2000);
    const double idx_sort = run("ORDER BY price LIMIT 10 (index order)", Query("items").order_by(col("price")).limit(10), 2000);
    const double idx_count = run("COUNT price > 99000 (index count)", Query("items").where(col("price") > 99000).count(), 20000);
    run("name LIKE 'kiwi1%' (index prefix)", Query("items").where(col("name").like("kiwi1%")), 200);
    run("owner IN (1..40)", Query("items").where(col("owner").in({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40 })), 500);
    run("owner = 7 OR owner = 9 (index union)", Query("items").where(col("owner") == 7 || col("owner") == 9), 2000);
    run("PK lookup id = 123456", Query("items").where(col("id") == 123456), 20000);
    run("MIN(price) (index endpoint)", Query("items").min(col("price")), 20000);
    db.set_cache(512, 200000);
    const Query cq = Query("items").where(col("score") > 9990.0).order_by(col("score"));
    const double miss = run("score > 9990 ORDER BY score (cold)", cq, 1);
    const double hit = run("same query again (result cache)", cq, 2000);
    std::printf("  speedups: hash %.0fx, range %.0fx, top-N %.0fx, count %.0fx, cache %.0fx\n", scan_owner / idx_owner, scan_range / idx_range, scan_sort / idx_sort, scan_count / idx_count, miss / std::max(hit, 1e-6));
    CHECK(idx_owner * 5 < scan_owner && idx_range * 5 < scan_range && idx_sort * 5 < scan_sort);
}

static void partition_benchmarks() {
    const std::int64_t day = 86400, t0 = 1792800000;
    const int days = 30, per_day = 10000;
    std::int64_t now = t0 + day;
    Database db;
    db.set_cache(0, 0);
    db.create_table("flat", Schema{ integer("id").primary().autoincrement(), integer("ts").not_null(), integer("user"), real("ms") });
    db.create_partitioned_table("parts", Schema{ integer("id").primary().autoincrement(), integer("ts").not_null(), integer("user"), real("ms") }, PartitionOptions::daily("ts").keep_for(std::chrono::hours(24 * 40)).use_clock([&] { return now; }));
    db.create_index("flat", { "ts" });
    db.create_index("parts", { "ts" });
    std::vector<Row> rows;
    rows.reserve(days * per_day);
    for (int d = days - 1; d >= 0; --d) for (int k = 0; k < per_day; ++k) rows.push_back({ Value(), Value(t0 - d * day + rnd(0, day - 1)), Value(rnd(1, 2000)), Value(static_cast<double>(rnd(1, 5000)) / 10.0) });
    const int n = static_cast<int>(rows.size());
    const double flat_ins = time_ms([&] { db.insert_many("flat", rows); });
    const double part_ins = time_ms([&] { db.insert_many("parts", rows); });
    std::printf("  insert %d rows: plain %.1f ms (%.0f rows/s), %d daily partitions %.1f ms (%.0f rows/s)\n", n, flat_ins, n / (flat_ins / 1000.0), days, part_ins, n / (part_ins / 1000.0));

    auto run = [&](const char* label, const Query& q, int reps) {
        ResultSet last;
        const double ms = time_ms([&] { last = db.query(q); }, reps);
        std::printf("  %-46s %9.3f ms  rows=%-6zu %s\n", label, ms, last.size(), last.stats.plan.c_str());
        return ms;
    };

    const Expr week = col("ts") >= t0 - 6 * day + 3600 && col("ts") < t0 + day;
    run("COUNT last ~7 days (plain, ts index)", Query("flat").where(week).count(), 200);
    run("COUNT last ~7 days (partitioned)", Query("parts").where(week).count(), 200);
    const Expr week_users = week && col("user") < 100;
    const double fw = run("last ~7 days AND user < 100 (plain)", Query("flat").where(week_users).count(), 20);
    const double pw = run("last ~7 days AND user < 100 (partitioned)", Query("parts").where(week_users).count(), 20);
    const Expr hour = col("ts") >= t0 - 3 * day && col("ts") < t0 - 3 * day + 3600;
    run("one hour, 3 days ago (plain)", Query("flat").where(hour).select({ "id" }), 200);
    run("one hour, 3 days ago (partitioned)", Query("parts").where(hour).select({ "id" }), 200);
    run("newest 20 (plain)", Query("flat").order_by_desc(col("ts")).limit(20), 2000);
    run("newest 20 (partitioned)", Query("parts").order_by_desc(col("ts")).limit(20), 2000);
    const double del = time_ms([&] { db.remove("flat", col("ts") < t0 - 27 * day); });
    std::size_t dropped = 0;
    const double drop = time_ms([&] {
        now += 12 * day;
        dropped = db.enforce_retention("parts").size();
    });
    std::printf("  remove the oldest 2 days: plain DELETE %.2f ms, partitions dropped (%zu) %.3f ms (%.0fx)\n", del, dropped, drop, del / std::max(drop, 1e-6));
    std::printf("  filtered week: partitioned %.1fx faster\n", fw / std::max(pw, 1e-6));
    CHECK(dropped == 2 && db.count("parts") == db.count("flat"));
}

int main(int argc, char** argv) {
    const bool bench = argc > 1 && std::string(argv[1]) == "--bench";
    if (const char* seed = std::getenv("FIZMO_DB_SEED")) rng.seed(std::strtoull(seed, nullptr, 10));
    std::printf("values\n"); test_values();
    std::printf("table basics\n"); test_table_basics();
    std::printf("random differential queries\n"); test_fuzz();
    std::printf("aggregates + joins\n"); test_aggregates_joins();
    std::printf("transactions + cache\n"); test_transactions_cache();
    std::printf("persistence + crash recovery\n"); test_persistence();
    std::printf("graphs\n"); test_graph();
    std::printf("partitions by time\n"); test_partitions_time();
    std::printf("partition shapes\n"); test_partitions_shapes();
    std::printf("partition persistence\n"); test_partitions_persistence();
    std::printf("retention sweeper\n"); test_partition_sweeper();
    std::printf("partitions vs plain table\n"); test_partitions_fuzz();

    if (bench) {
        std::printf("benchmarks\n");
        benchmarks();
        partition_benchmarks();
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
