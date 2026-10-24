#include "../Source/Networking/networking.hpp"

#include <future>

#include <atomic>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>

using namespace fizmo::networking;
using namespace fizmo::networking::guard;
using namespace std::chrono_literals;
namespace json = fizmo::json;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

struct FakeClock {
    SteadyTime t = SteadyClock::now();
    WallTime w = WallClock::now();
    NowFn now() { return [this] { return t; }; }
    WallNowFn wall() { return [this] { return w; }; }
    void advance(SteadyClock::duration d) { t += d; w += std::chrono::duration_cast<WallClock::duration>(d); }
};

void test_rate_limiter() {
    FakeClock c;
    RateLimitPolicy p = RateLimitPolicy::per_second(1.0, 3.0);
    RateLimiter rl(p, c.now());
    CHECK(rl.consume("a"));
    CHECK(rl.consume("a"));
    CHECK(rl.consume("a"));
    auto d = rl.consume("a");
    CHECK(!d.allowed);
    CHECK(d.retry_after >= 999ms && d.retry_after <= 1001ms);
    CHECK(rl.consume("b"));
    c.advance(1s);
    CHECK(rl.consume("a"));
    CHECK(!rl.consume("a"));
    p.initial_tokens = 1;
    RateLimiter slow(p, c.now());
    CHECK(slow.consume("x"));
    CHECK(!slow.consume("x"));
    CHECK(!slow.consume("y", 5.0));

    SlidingWindowLimiter sw(WindowPolicy{ 2, std::chrono::seconds(10), 100 }, c.now());
    CHECK(sw.hit("k"));
    c.advance(4s);
    CHECK(sw.hit("k"));
    auto w = sw.hit("k");
    CHECK(!w.allowed && w.retry_after >= 5999ms && w.retry_after <= 6001ms);
    c.advance(6s);
    CHECK(sw.hit("k"));
}

void test_failures() {
    FakeClock c;
    FailurePolicy p;
    p.max_failures = 3;
    p.min_samples = 0;
    p.lockout = std::chrono::seconds(10);
    p.slowdown_after = 2;
    FailureTracker ft(p, c.now());
    int lockouts = 0;
    ft.on_lockout([&](const std::string&, const FailureStatus&) { ++lockouts; });
    ft.record_failure("ip");
    auto s = ft.record_failure("ip");
    CHECK(!s.locked && s.suggested_delay.count() > 0);
    s = ft.record_failure("ip");
    CHECK(s.locked && s.lock_remaining == 10000ms && lockouts == 1);
    c.advance(11s);
    CHECK(!ft.is_locked("ip"));
    ft.record_failure("ip"); ft.record_failure("ip");
    s = ft.record_failure("ip");
    CHECK(s.locked && s.strikes == 2 && s.lock_remaining == 20000ms);
    ft.unlock("ip");
    CHECK(!ft.is_locked("ip"));
}

void test_ja4() {
    security::ClientHelloInfo h;
    h.legacy_version = 0x0303;
    h.supported_versions = { 0x0a0a, 0x0304, 0x0303 };
    h.cipher_suites = { 0x1a1a, 0x1301, 0x1302, 0x1303, 0xc02b, 0xc02f, 0xc02c, 0xc030, 0xcca9, 0xcca8, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035 };
    h.extensions = { 0x2a2a, 0x0000, 0x0017, 0xff01, 0x000a, 0x000b, 0x0023, 0x0010, 0x0005, 0x000d, 0x0012, 0x0033, 0x002d, 0x002b, 0x001b, 0x4469, 0x0015, 0x3a3a };
    h.signature_algorithms = { 0x0403, 0x0804, 0x0401, 0x0503, 0x0805, 0x0501, 0x0806, 0x0601 };
    h.server_name = "example.com";
    h.alpn_protocols = { "h2", "http/1.1" };
    const TLSFingerprint fp = fingerprint(h);
    std::printf("  ja4     = %s\n  ja4_raw = %s\n", fp.ja4.c_str(), fp.ja4_raw.c_str());
    CHECK(fp.ja4_a == "t13d1516h2");
    CHECK(fp.ja4_b == "8daaf6152771");
    CHECK(fp.ja4_c == "e5627efa2ab1");
    CHECK(fp.uses_grease);

    FingerprintRegistry reg;
    auto v = reg.assess(fp, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36");
    CHECK(v.level == RiskLevel::Low);
    TLSFingerprint fake = fp;
    fake.uses_grease = false;
    fake.alpn = "http/1.1";
    v = reg.assess(fake, "Mozilla/5.0 Chrome/130.0 Safari/537.36");
    CHECK(v.level == RiskLevel::High);
    v = reg.assess(fp, "python-requests/2.31");
    CHECK(v.level >= RiskLevel::Medium);
    reg.block(fp.ja4, "test");
    CHECK(reg.assess(fp, "x").blocked());
    CHECK(classify_user_agent("curl/8.0") == ClientFamily::Automation);
    CHECK(classify_user_agent("Mozilla/5.0 (Windows NT 10.0; rv:131.0) Gecko/20100101 Firefox/131.0") == ClientFamily::Firefox);
}

void test_devices() {
    FakeClock c;
    DeviceRegistry reg(DevicePolicy(), c.wall());
    int news = 0;
    reg.on_new_device([&](const DeviceRecord&, const DeviceCheck&) { ++news; });
    auto a = reg.identify("u1", "", { "ja4a", "Mozilla/5.0 Firefox/131.0", "1.1.1.1" });
    CHECK(a.is_new() && !a.issued_token.empty() && a.needs_verification() && news == 1);
    auto b = reg.identify("u1", a.issued_token, { "ja4a", "Mozilla/5.0 Firefox/131.0", "2.2.2.2" });
    CHECK(b.status == DeviceStatus::Known && b.device_id == a.device_id && b.issued_token.empty());
    CHECK(reg.trust("u1", a.device_id, "laptop"));
    b = reg.identify("u1", a.issued_token, { "ja4b", "Mozilla/5.0 Chrome/130.0", "2.2.2.2" });
    CHECK(b.status == DeviceStatus::Changed && b.trusted);
    auto other = reg.identify("u2", a.issued_token, { "ja4a", "", "" });
    CHECK(other.is_new());
    CHECK(reg.devices("u1").size() == 1);
    CHECK(reg.revoke("u1", a.device_id) && reg.devices("u1").empty());
}

void test_sessions_cookies() {
    FakeClock c;
    SessionPolicy p;
    p.idle_timeout = std::chrono::minutes(30);
    p.absolute_timeout = std::chrono::hours(2);
    p.rotate_every = std::chrono::minutes(10);
    SessionStore store(p, c.wall());
    std::vector<std::string> revoked;
    store.on_revoke([&](const Session&, RevokeReason r) { revoked.push_back(to_string(r)); });
    auto s = store.create("alice", "dev1", { { "roles", "admin" } });
    CHECK(s.token.size() >= 40);
    CHECK(store.validate(s.token).has_value());
    CHECK(!store.validate("nope").has_value());
    CHECK(store.verify_csrf(s.token, s.session.csrf_token));
    CHECK(!store.verify_csrf(s.token, "wrong"));
    c.advance(std::chrono::minutes(11));
    auto v = store.validate(s.token);
    CHECK(v && store.needs_rotation(*v));
    auto r = store.rotate(s.token);
    CHECK(r && r->session.id == s.session.id && r->token != s.token);
    CHECK(!store.validate(s.token));
    CHECK(store.validate(r->token));
    c.advance(std::chrono::minutes(31));
    CHECK(!store.validate(r->token));
    auto s2 = store.create("alice");
    auto s3 = store.create("alice");
    auto s4 = store.create("bob");
    CHECK(store.revoke_user("alice", s3.session.id) == 1);
    CHECK(!store.validate(s2.token) && store.validate(s3.token) && store.validate(s4.token));
    CHECK(store.revoke(s3.token));
    CHECK(!store.validate(s3.token));
    CHECK(revoked.size() == 3);
    for (int i = 0; i < 60; ++i) store.create("spam");
    CHECK(store.sessions("spam").size() == p.max_per_user);
    c.advance(std::chrono::hours(3));
    CHECK(store.sweep() > 0 && store.size() == 0);

    const std::string cookie = store.set_cookie_header(s4);
    std::printf("  set-cookie = %s\n", cookie.c_str());
    CHECK(cookie.find("__Host-session=") == 0 && cookie.find("; Secure") != std::string::npos && cookie.find("HttpOnly") != std::string::npos);
    CHECK(store.clear_cookie_header().find("Max-Age=0") != std::string::npos);
    CHECK(store.token_from_cookie_header("a=1; __Host-session=abc ; b=2") == "abc");
    bool threw = false;
    try { CookieOptions o; o.secure = false; make_set_cookie("__Host-x", "1", o); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { make_set_cookie("x", "bad;value"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    CHECK(clear_site_data_header(ClearSiteData::Cookies | ClearSiteData::Storage) == "\"cookies\", \"storage\"");
    CHECK(clear_site_data_header(ClearSiteData::All) == "\"*\"");
}

void test_ownership() {
    OwnershipGuard g;
    std::map<std::string, ResourceInfo> recipes = { { "1", { "alice", {}, {}, false } }, { "2", { "bob", { "alice" }, {}, false } }, { "3", { "bob", {}, {}, true } } };
    g.register_type("recipe", [&](const std::string& id) -> std::optional<ResourceInfo> { auto it = recipes.find(id); if (it == recipes.end()) return std::nullopt; return it->second; });
    g.set_admin_role("admin");
    int denies = 0;
    g.on_deny([&](const Principal&, const std::string&, const std::string&, Action, const AccessDecision&) { ++denies; });
    Principal alice{ "alice", {} }, bob{ "bob", {} }, anon, admin{ "root", { "admin" } };
    CHECK(g.check(alice, "recipe", "1", Action::Update));
    CHECK(g.check(bob, "recipe", "1", Action::Read).result == AccessResult::NotFound);
    CHECK(g.check(alice, "recipe", "2", Action::Read));
    CHECK(!g.check(alice, "recipe", "2", Action::Delete));
    CHECK(g.check(anon, "recipe", "3", Action::Read));
    CHECK(g.check(anon, "recipe", "1", Action::Read).result == AccessResult::Unauthenticated);
    CHECK(g.check(alice, "recipe", "999", Action::Read).http_status() == 404);
    CHECK(g.check(admin, "recipe", "1", Action::Delete));
    CHECK(!g.check(alice, "nope", "1", Action::Read));
    CHECK(denies >= 4);

    IdSigner signer("0123456789abcdef-secret");
    const std::string pub = signer.encode("recipe", "42");
    CHECK(signer.decode("recipe", pub) == std::optional<std::string>("42"));
    CHECK(!signer.decode("user", pub));
    std::string tampered = pub;
    tampered[0] = tampered[0] == 'A' ? 'B' : 'A';
    CHECK(!signer.decode("recipe", tampered));
}

void test_query() {
    Statement s("SELECT * FROM recipes WHERE owner_id = ? AND id = ?");
    CHECK(s.placeholder_count() == 2 && !s.complete());
    s.bind(7).bind("abc");
    CHECK(s.complete());
    bool threw = false;
    try { s.bind(1); } catch (const QueryError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { Statement bad("SELECT * FROM users WHERE name = 'bob'"); } catch (const QueryError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { Statement bad("SELECT 1; DROP TABLE users"); } catch (const QueryError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { Statement bad("SELECT 1 -- hi"); } catch (const QueryError&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { Statement::identifier("name; DROP"); } catch (const QueryError&) { threw = true; }
    CHECK(threw);
    Statement q("SELECT id FROM steps WHERE");
    q.append_in("recipe_id", { SqlValue(std::int64_t(1)), SqlValue(std::int64_t(2)), SqlValue(std::int64_t(3)) });
    q.append("AND \"order\" > ?", { SqlValue(std::int64_t(0)) });
    CHECK(q.placeholder_count() == 4 && q.complete());
    std::printf("  debug  = %s\n  shape  = %s\n", q.debug_string().c_str(), q.shape().c_str());
    CHECK(normalize_query_shape("SELECT * FROM t WHERE id IN (?, ?)") == normalize_query_shape("select *  from t where id in (?,?,?,?)"));
    CHECK(normalize_query_shape("select * from t where id = 5") == normalize_query_shape("SELECT * FROM t WHERE id = ?"));
    Statement trailing("SELECT 1;");
    CHECK(trailing.placeholder_count() == 0);

    QueryRecorder rec;
    for (int i = 0; i < 6; ++i) rec.record(Statement("SELECT * FROM steps WHERE recipe_id = ?", { SqlValue(std::int64_t(i)) }));
    rec.record(Statement("SELECT * FROM recipes"), 300ms);
    auto w = rec.warnings();
    CHECK(w.size() == 2);
    CHECK(w[0].kind == QueryWarningKind::RepeatedShape && w[0].count == 6);
    CHECK(w[1].kind == QueryWarningKind::Slow);
}

void test_concurrency() {
    KeyedMutex km;
    int counter = 0;
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i) ts.emplace_back([&] { for (int j = 0; j < 2000; ++j) { auto l = km.lock("acct:1"); ++counter; } });
    for (auto& t : ts) t.join();
    CHECK(counter == 16000);
    CHECK(km.size() == 0);
    {
        auto l = km.lock("x");
        CHECK(!km.try_lock("x"));
        CHECK(!km.try_lock_for("x", 20ms));
        CHECK(km.try_lock("y"));
    }
    CHECK(km.try_lock("x"));
    auto all = km.lock_all({ "b", "a", "b" });
    CHECK(all.size() == 2);

    Versioned<int> balance(100);
    auto snap = balance.get();
    CHECK(balance.compare_and_set(snap.version, 90));
    CHECK(!balance.compare_and_set(snap.version, 80));
    CHECK(balance.get().value == 90);

    SingleFlight<int> sf;
    std::atomic<int> runs{0};
    std::vector<std::thread> callers;
    std::atomic<int> sum{0};
    for (int i = 0; i < 6; ++i) callers.emplace_back([&] { sum += sf.get("k", [&] { ++runs; std::this_thread::sleep_for(50ms); return 5; }); });
    for (auto& t : callers) t.join();
    CHECK(sum == 30);
    CHECK(runs < 6);
    std::printf("  singleflight ran %d time(s) for 6 callers\n", runs.load());
}

void test_idempotency() {
    FakeClock c;
    IdempotencyStore st(IdempotencyPolicy(), c.now());
    const std::string fp = IdempotencyStore::request_fingerprint("POST", "/pay", "{\"amount\":5}");
    auto b = st.begin("user:1", "key-1", fp);
    CHECK(b.should_run());
    CHECK(st.begin("user:1", "key-1", fp).state == IdempotencyState::InProgress);
    st.complete("user:1", "key-1", StoredResponse{ 201, { { "Content-Type", "application/json" } }, "{\"ok\":true}" });
    b = st.begin("user:1", "key-1", fp);
    CHECK(b.state == IdempotencyState::Replay && b.response->status == 201);
    CHECK(st.begin("user:1", "key-1", "other").state == IdempotencyState::Mismatch);
    CHECK(st.begin("user:2", "key-1", fp).should_run());
    st.abandon("user:2", "key-1");
    CHECK(st.begin("user:2", "key-1", fp).should_run());
    CHECK(st.begin("u", "", fp).state == IdempotencyState::Invalid);
    c.advance(std::chrono::hours(25));
    CHECK(st.begin("user:1", "key-1", fp).should_run());
}

void test_logger_errors() {
    Logger log(LogLevel::Debug);
    auto mem = std::make_shared<MemorySink>();
    log.add_sink(mem);
    log.child({ field("request_id", "r1") }).info("login", { field("user", "alice"), field("password", "hunter2"), field("Authorization", "Bearer abc"), field("note", "Bearer xyz"), field("n", 3) });
    log.debug("quote \"x\"\nnewline");
    const auto lines = mem->lines();
    CHECK(lines.size() == 2);
    std::printf("  log    = %s\n", lines[0].c_str());
    CHECK(lines[0].find("hunter2") == std::string::npos && lines[0].find("abc") == std::string::npos && lines[0].find("xyz") == std::string::npos);
    CHECK(lines[0].find("\"request_id\":\"r1\"") != std::string::npos && lines[0].find("\"n\":3") != std::string::npos);
    CHECK(lines[1].find("\\n") != std::string::npos);
    log.set_level(LogLevel::Warn);
    log.info("hidden");
    CHECK(mem->lines().size() == 2);

    FakeClock c;
    ErrorTrackerPolicy p;
    p.spike_threshold = 5;
    ErrorTracker et(p, c.now(), c.wall());
    std::vector<ErrorAlertKind> alerts;
    et.on_alert([&](ErrorAlertKind k, const ErrorGroup&) { alerts.push_back(k); });
    std::string g1, g2;
    for (int i = 0; i < 6; ++i) {
        try { throw std::runtime_error("recipe " + std::to_string(1000 + i) + " not found"); }
        catch (...) { g1 = et.capture_current("load_recipe", "req" + std::to_string(i)); }
    }
    g2 = et.capture(ErrorEvent{ "db", "timeout", "query", "", {} });
    CHECK(et.groups().size() == 2);
    CHECK(et.group(g1)->count == 6 && et.group(g1)->type == "std::runtime_error");
    CHECK(std::count(alerts.begin(), alerts.end(), ErrorAlertKind::NewGroup) == 2);
    CHECK(std::count(alerts.begin(), alerts.end(), ErrorAlertKind::Spike) == 1);
    et.resolve(g2);
    et.capture(ErrorEvent{ "db", "timeout", "query", "", {} });
    CHECK(alerts.back() == ErrorAlertKind::Regression);
}

void test_jobs_budgets() {
    JobQueue q(2);
    std::atomic<int> flaky_runs{0}, good{0};
    q.register_handler("flaky", [&](const JobContext& ctx) {
        ++flaky_runs;
        if (ctx.attempt < 3) return JobResult::retry("not yet", 10ms);
        return JobResult::ok();
    });
    q.register_handler("broken", [&](const JobContext&) -> JobResult { throw std::runtime_error("boom"); });
    q.register_handler("good", [&](const JobContext&) { ++good; return JobResult::ok(); });
    std::atomic<int> dead{0};
    q.on_dead([&](const JobRecord&) { ++dead; });
    q.start();
    JobOptions o;
    o.retry.max_attempts = 5;
    q.enqueue("flaky", "", o);
    JobOptions b;
    b.retry.max_attempts = 2;
    b.retry.initial = 5ms;
    const std::string bid = q.enqueue("broken", "", b);
    JobOptions u;
    u.unique_key = "email:alice";
    u.delay = 30ms;
    const std::string u1 = q.enqueue("good", "", u);
    const std::string u2 = q.enqueue("good", "", u);
    CHECK(u1 == u2);
    for (int i = 0; i < 100 && (flaky_runs < 3 || dead < 1 || good < 1); ++i) std::this_thread::sleep_for(10ms);
    CHECK(flaky_runs == 3);
    CHECK(dead == 1 && good == 1);
    auto rec = q.find(bid);
    CHECK(rec && rec->state == JobState::Dead && rec->attempts == 2 && rec->last_error.find("boom") != std::string::npos);
    CHECK(q.dead_letters().size() == 1);
    q.stop();

    FakeClock c;
    BudgetMonitor bm(c.now());
    std::vector<std::string> alerts;
    bm.on_alert([&](const BudgetAlert& a) { alerts.push_back(a.metric + ":" + to_string(a.level)); });
    bm.set_budget({ "email.sent", 10, std::chrono::hours(24), 0.8, true, "emails" });
    for (int i = 0; i < 7; ++i) bm.add("email.sent");
    CHECK(alerts.empty());
    bm.add("email.sent");
    CHECK(alerts.size() == 1 && alerts[0] == "email.sent:warning");
    bm.add("email.sent"); bm.add("email.sent");
    CHECK(alerts.size() == 2 && alerts[1] == "email.sent:exceeded");
    CHECK(!bm.consume("email.sent"));
    bm.add("email.sent");
    CHECK(alerts.size() == 2);
    c.advance(std::chrono::hours(25));
    CHECK(bm.consume("email.sent"));
    bm.set_gauge_threshold("jobs.queued", 100, 1000);
    bm.set_gauge("jobs.queued", 50);
    bm.set_gauge("jobs.queued", 150);
    CHECK(alerts.back() == "jobs.queued:warning");
}

static bool rejected_with(const EnqueueResult& r, const std::string& path, const std::string& words) {
    if (r.ok) return false;
    for (const auto& i : r.issues) if (i.path == path && i.message.find(words) != std::string::npos) return true;
    std::printf("    issues were: %s\n", r.message().c_str());
    return false;
}

void test_job_checks() {
    {
        JobQueue q(1);
        q.register_handler("noop", [](const JobContext&) { return JobResult::ok(); });
        JobOptions o;
        o.retry.max_attempts = 0;
        CHECK(rejected_with(q.try_enqueue("noop", "", o), "retry.max_attempts", "at least 1"));
        o.retry.max_attempts = kMaxJobAttempts + 1;
        CHECK(rejected_with(q.try_enqueue("noop", "", o), "retry.max_attempts", "the limit is 100"));
        o.retry.max_attempts = kMaxJobAttempts;
        CHECK(q.try_enqueue("noop", "", o).ok);
        JobQueueLimits l;
        l.max_attempts = 10;
        q.set_limits(l);
        o.retry.max_attempts = 11;
        CHECK(rejected_with(q.try_enqueue("noop", "", o), "retry.max_attempts", "the limit is 10"));
        o.retry.max_attempts = 3;
        o.retry.multiplier = 0.5;
        CHECK(rejected_with(q.try_enqueue("noop", "", o), "retry.multiplier", "from 1 to 10"));
        o.retry.multiplier = 2.0;
        o.retry.maximum = std::chrono::hours(48);
        CHECK(rejected_with(q.try_enqueue("noop", "", o), "retry.maximum", "the limit"));
        l.max_attempts = 0;
        bool threw = false;
        try { q.set_limits(l); } catch (const JobRejected&) { threw = true; }
        CHECK(threw);
        l.max_attempts = 101;
        threw = false;
        try { q.set_limits(l); } catch (const JobRejected&) { threw = true; }
        CHECK(threw);
    }

    {
        JobQueue q(1);
        std::atomic<int> fails{0}, goods{0};
        std::vector<std::string> order;
        std::mutex om;
        q.register_handler("always_fails", [&](const JobContext& ctx) {
            ++fails;
            return JobResult::retry("attempt " + std::to_string(ctx.attempt) + " of " + std::to_string(ctx.max_attempts) + (ctx.last_attempt() ? " (last)" : ""), 15ms);
        });
        q.register_handler("good", [&](const JobContext& ctx) {
            ++goods;
            std::lock_guard<std::mutex> g(om);
            order.push_back(ctx.payload);
            return JobResult::ok();
        });
        q.register_handler("fatal", [&](const JobContext&) { return JobResult::fail("bad input"); });
        std::atomic<int> dead_cb{0};
        q.on_dead([&](const JobRecord& r) { if (r.state == JobState::Dead) ++dead_cb; });
        JobOptions three;
        three.retry.max_attempts = 3;
        const std::string fid = q.enqueue("always_fails", "", three);
        for (int i = 0; i < 5; ++i) q.enqueue("good", std::to_string(i));
        q.start();
        for (int i = 0; i < 200 && (goods < 5 || q.dead_count() < 1); ++i) std::this_thread::sleep_for(5ms);
        CHECK(goods == 5 && fails == 3);
        CHECK(q.dead_count() == 1 && dead_cb == 1);
        auto rec = q.find(fid);
        CHECK(rec && rec->state == JobState::Dead && rec->attempts == 3 && rec->errors.size() == 3 && rec->last_error == "attempt 3 of 3 (last)");
        CHECK(q.stats().queued == 0 && q.stats().dead == 1);
        CHECK(q.retry_dead(fid));
        CHECK(q.dead_count() == 0);
        for (int i = 0; i < 200 && q.dead_count() < 1; ++i) std::this_thread::sleep_for(5ms);
        CHECK(fails == 6 && q.dead_count() == 1);
        CHECK(q.discard_dead(fid) && q.dead_count() == 0 && !q.find(fid));
        JobOptions five;
        five.retry.max_attempts = 5;
        const std::string fatal = q.enqueue("fatal", "", five);
        for (int i = 0; i < 200 && q.dead_count() < 1; ++i) std::this_thread::sleep_for(5ms);
        CHECK(q.find(fatal) && q.find(fatal)->attempts == 1 && q.find(fatal)->state == JobState::Dead);
        q.stop();
        JobQueueLimits l;
        l.dead_letter_capacity = 2;
        q.set_limits(l);
        q.start();
        for (int i = 0; i < 4; ++i) q.enqueue("fatal");
        for (int i = 0; i < 200 && q.stats().dead_evicted < 3; ++i) std::this_thread::sleep_for(5ms);
        CHECK(q.dead_count() == 2 && q.stats().dead_evicted == 3);
        CHECK(q.clear_dead() == 2);
        q.stop();
    }

    {
        JobQueue q(1);
        std::vector<std::string> rejected_names;
        q.on_rejected([&](const std::string& name, const std::vector<SchemaIssue>&) { rejected_names.push_back(name); });
        JobSchema attachment;
        attachment.field("name", FieldRule::string().non_empty().max_length(100)).field("bytes", FieldRule::integer().between(1, 1000000));
        JobSchema email;
        email.field("to", FieldRule::string().format(StringFormat::Email))
            .field("subject", FieldRule::string().length(1, 200))
            .field("body", FieldRule::string().max_length(5000).optional())
            .field("priority", FieldRule::string().one_of({ "low", "high" }).optional())
            .field("attachments", FieldRule::array(FieldRule::object(attachment)).max_items(3).optional())
            .check([](const json::Value& v) { return v["subject"].as_string() == "spam" ? std::string("subject is not allowed") : std::string(); });
        q.register_handler("email", [](const JobContext&) { return JobResult::ok(); }, email);

        CHECK(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","attachments":[{"name":"a.txt","bytes":12}]})").ok);
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com",)"), "$", "not valid JSON"));
        CHECK(rejected_with(q.try_enqueue("email", R"(["ann@example.com"])"), "$", "must be a JSON object, not an array"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"subject":"Hi"})"), "$.to", "is required"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@","subject":"Hi"})"), "$.to", "valid email"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","cc":"bo@example.com"})"), "$.cc", "not an allowed field"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":5})"), "$.subject", "must be a string, not a number"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"a@b.co","to":"c@d.co","subject":"Hi"})"), "$.to", "more than once"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":""})"), "$.subject", "can't be empty"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","priority":"urgent"})"), "$.priority", "one of \"low\", \"high\""));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","attachments":[{"name":"a","bytes":1.5}]})"), "$.attachments[0].bytes", "whole number"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","attachments":[{"name":"a","bytes":0}]})"), "$.attachments[0].bytes", "at least 1"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","attachments":[{},{},{},{}]})"), "$.attachments", "at most 3 items"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi\u0007"})"), "$.subject", "control characters"));
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"spam"})"), "$", "subject is not allowed"));
        CHECK(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi","body":"line one\nline two"})").ok);
        const auto many = q.try_enqueue("email", R"({"to":5,"subject":[],"x":1,"y":2})");
        CHECK(!many.ok && many.issues.size() == 4);

        bool threw = false;
        try {
            q.enqueue("email", "{}");
        } catch (const JobRejected& e) {
            threw = e.issues().size() == 2 && e.job() == "email" && std::string(e.what()).find("$.to: is required") != std::string::npos;
        }
        CHECK(threw);
        CHECK(q.check("email", R"({"to":"ann@example.com","subject":"Hi"})").empty());
        CHECK(q.stats().queued == 2);

        CHECK(rejected_with(q.try_enqueue("nobody", ""), "name", "no handler is registered"));
        CHECK(rejected_with(q.try_enqueue("has space", ""), "name", "letters, digits"));
        JobOptions o;
        o.delay = std::chrono::hours(24 * 30);
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi"})", o), "delay", "the limit"));
        o.delay = 0ms;
        o.priority = 5000;
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi"})", o), "priority", "from -1000 to 1000"));
        o.priority = 0;
        o.unique_key = std::string(300, 'k');
        CHECK(rejected_with(q.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi"})", o), "unique_key", "longer than 256"));
        CHECK(rejected_with(q.try_enqueue("email", "{\"to\":\"" + std::string(70000, 'a') + "\"}"), "$", "the queue's limit"));
        CHECK(q.stats().rejected == rejected_names.size() && rejected_names.size() >= 20);

        q.register_handler("ping", [](const JobContext&) { return JobResult::ok(); }, JobSchema::empty());
        CHECK(q.try_enqueue("ping").ok);
        CHECK(rejected_with(q.try_enqueue("ping", "x"), "$", "no payload"));
        q.register_handler("note", [](const JobContext&) { return JobResult::ok(); }, JobSchema::text(20));
        CHECK(q.try_enqueue("note", "hello\nthere").ok);
        CHECK(rejected_with(q.try_enqueue("note", "bad\xff"), "$", "UTF-8"));
        CHECK(rejected_with(q.try_enqueue("note", std::string(21, 'a')), "$", "the limit is 20"));
        CHECK(rejected_with(q.try_enqueue("note", "ring\a"), "$", "control"));

        JobSchema pay;
        pay.field("card_token", FieldRule::string().optional()).field("bank_token", FieldRule::string().optional()).only_one_of({ "card_token", "bank_token" }).at_least_one_of({ "card_token", "bank_token" })
            .field("from", FieldRule::string().format(StringFormat::Date).optional()).field("to", FieldRule::string().format(StringFormat::Date).optional()).all_or_none_of({ "from", "to" })
            .field("id", FieldRule::string().format(StringFormat::Uuid).optional()).field("hook", FieldRule::string().format(StringFormat::Url).optional())
            .field("at", FieldRule::string().format(StringFormat::DateTime).optional()).field("blob", FieldRule::string().format(StringFormat::Base64).optional())
            .field("tags", FieldRule::array(FieldRule::string()).unique_items().optional()).field("amount", FieldRule::number().positive().optional());
        q.register_handler("pay", [](const JobContext&) { return JobResult::ok(); }, pay);
        CHECK(q.try_enqueue("pay", R"({"card_token":"t","from":"2028-02-29","to":"2028-03-01","id":"123e4567-e89b-12d3-a456-426614174000","hook":"https://api.example.com:8443/cb?x=1","at":"2026-10-24T10:42:00.5-05:00","blob":"aGk=","tags":["a","b"],"amount":0.01})").ok);
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","bank_token":"b"})"), "$", "only one of card_token, bank_token"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({})"), "$", "at least one of"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","from":"2026-01-01"})"), "$", "all of from, to"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","from":"2026-02-29","to":"2026-03-01"})"), "$.from", "valid date"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","id":"123e4567e89b12d3a456426614174000"})"), "$.id", "valid UUID"));
        CHECK(rejected_with(q.try_enqueue("pay", R"j({"card_token":"t","hook":"javascript:alert(1)"})j"), "$.hook", "http(s) URL"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","hook":"https://user@evil.example.com/"})"), "$.hook", "http(s) URL"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","at":"2026-10-24 10:42"})"), "$.at", "ISO 8601"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","blob":"aGk"})"), "$.blob", "base64"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","tags":["a","a"]})"), "$.tags[1]", "repeats"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","amount":0})"), "$.amount", "more than 0"));
        CHECK(rejected_with(q.try_enqueue("pay", R"({"card_token":"t","amount":1e300,"tags":[1]})"), "$.tags[0]", "must be a string"));

        JobSchema loose;
        loose.allow_extra_fields().max_depth(6).max_array_items(5).max_string_length(10);
        q.register_handler("loose", [](const JobContext&) { return JobResult::ok(); }, loose);
        CHECK(q.try_enqueue("loose", R"({"a":{"b":[1,2,3]},"c":"short"})").ok);
        CHECK(rejected_with(q.try_enqueue("loose", R"({"a":[[[[[[1]]]]]]})"), "$.a[0][0][0][0][0]", "deeper than 6"));
        CHECK(rejected_with(q.try_enqueue("loose", R"({"a":[1,2,3,4,5,6]})"), "$.a", "the limit is 5"));
        CHECK(rejected_with(q.try_enqueue("loose", R"({"a":"this is too long"})"), "$.a", "longer than 10"));
        CHECK(rejected_with(q.try_enqueue("loose", "{\"a\":\"bad\xff\"}"), "$.a", "UTF-8"));

        JobQueueLimits l;
        l.require_schema = true;
        l.max_queued = q.stats().queued + 1;
        q.set_limits(l);
        q.register_handler("bare", [](const JobContext&) { return JobResult::ok(); });
        CHECK(rejected_with(q.try_enqueue("bare"), "name", "no payload schema"));
        CHECK(q.try_enqueue("ping").ok);
        CHECK(rejected_with(q.try_enqueue("ping"), "", "queue is full"));
        JobOptions dup;
        dup.unique_key = "only-once";
        l.max_queued = 100;
        q.set_limits(l);
        const auto d1 = q.try_enqueue("ping", "", dup);
        const auto d2 = q.try_enqueue("ping", "", dup);
        CHECK(d1.ok && d2.ok && d2.duplicate && d1.id == d2.id);
    }

    CHECK(valid_utf8("héllo ✓") && !valid_utf8("\xc0\xaf") && !valid_utf8("\xed\xa0\x80") && utf8_length("héllo ✓") == 7);
    CHECK(matches_format("a@b.io", StringFormat::Email) && !matches_format("a..b@c.io", StringFormat::Email) && !matches_format("a@b", StringFormat::Email));
    CHECK(matches_format("[::1]:80", StringFormat::None) && matches_format("http://[::1]:8080/x", StringFormat::Url) && !matches_format("http://exa mple.com", StringFormat::Url));
}

void test_debounce() {
    start_background(1);
    std::atomic<int> fired{0};
    Debouncer d([&] { ++fired; }, DebounceOptions{ 60ms, false, true, std::chrono::milliseconds(0) });
    for (int i = 0; i < 5; ++i) { d.call(); std::this_thread::sleep_for(10ms); }
    std::this_thread::sleep_for(150ms);
    CHECK(fired == 1);

    std::atomic<int> lead{0};
    Debouncer l([&] { ++lead; }, DebounceOptions{ 60ms, true, false, std::chrono::milliseconds(0) });
    for (int i = 0; i < 5; ++i) l.call();
    std::this_thread::sleep_for(120ms);
    CHECK(lead == 1);

    std::atomic<int> maxed{0};
    Debouncer m([&] { ++maxed; }, DebounceOptions{ 50ms, false, true, std::chrono::milliseconds(120) });
    for (int i = 0; i < 20; ++i) { m.call(); std::this_thread::sleep_for(20ms); }
    std::this_thread::sleep_for(150ms);
    CHECK(maxed >= 3);
    std::printf("  max_wait debounce fired %d times over 400ms of calls\n", maxed.load());

    std::atomic<int> thr{0};
    Throttler t([&] { ++thr; }, 100ms);
    for (int i = 0; i < 10; ++i) { t.call(); std::this_thread::sleep_for(10ms); }
    std::this_thread::sleep_for(150ms);
    CHECK(thr == 2);

    std::mutex km;
    std::vector<std::string> keys;
    KeyedDebouncer kd([&](const std::string& k) { std::lock_guard<std::mutex> lk(km); keys.push_back(k); }, DebounceOptions{ 40ms, false, true, std::chrono::milliseconds(0) });
    kd.call("a"); kd.call("b"); kd.call("a");
    std::this_thread::sleep_for(120ms);
    CHECK(keys.size() == 2 && kd.pending() == 0);

    Debounced<std::string> save([&](std::string s) { std::lock_guard<std::mutex> lk(km); keys.push_back("save:" + s); }, DebounceOptions{ 30ms, false, true, std::chrono::milliseconds(0) });
    save("v1"); save("v2"); save("v3");
    std::this_thread::sleep_for(100ms);
    CHECK(keys.back() == "save:v3");

    ChangeSignal sig;
    std::thread waiter([&] { std::uint64_t v = 0; CHECK(sig.wait_for_change(0, 1000ms, &v) && v == 1); });
    std::this_thread::sleep_for(20ms);
    sig.notify();
    waiter.join();
    CHECK(!sig.wait_for_change(1, 10ms));
}


static http::FetchResponse call(const std::string& url, const std::string& method = "GET", http::Headers headers = {}, const std::string& body = {}) {
    std::promise<http::FetchResponse> p;
    auto f = p.get_future();
    http::FetchOptions o;
    o.method = method;
    o.headers = headers;
    o.body = body;
    o.secure.reject_unauthorized = false;
    o.use_agent = false;
    http::fetch(url, o, [&p](const std::error_code& ec, http::FetchResponse r) {
        if (ec) std::printf("  fetch error: %s\n", ec.message().c_str());
        p.set_value(std::move(r));
    });
    if (f.wait_for(10s) != std::future_status::ready) { std::printf("  fetch timed out\n"); return {}; }
    return f.get();
}

static std::string session_cookie(const http::FetchResponse& r) {
    for (const auto& c : r.headers.get_all("set-cookie")) {
        if (c.rfind("__Host-session=", 0) == 0) return c.substr(0, c.find(';'));
    }
    return {};
}

void test_https(const std::string& cert_file, const std::string& key_file) {
    GuardOptions opt;
    opt.global_limit = RateLimitPolicy::per_second(0.001, 20);
    opt.route_limits.push_back(RouteLimit{ "POST", "/login", RateLimitPolicy::per_minute(5, 5), 1.0, "login" });
    auto logger = std::make_shared<Logger>(LogLevel::Info);
    auto mem = std::make_shared<MemorySink>(500);
    logger->add_sink(mem);
    Guard guard(opt, logger);
    std::atomic<int> charges{0};

    auto handler = [&](http::IncomingMessage req, http::ServerResponse res) {
        RequestContext* ctx = Guard::context(req);
        const std::string path = req.path();

        if (path == "/fp") {
            res.json("{\"ja4\":\"" + ctx->tls.ja4 + "\",\"risk\":\"" + to_string(ctx->verdict.level) + "\"}");
        } else if (path == "/login" && req.method() == "POST") {
            req.discard();
            const LoginCheck check = guard.check_login(req, "alice");
            if (!check) { Guard::send_error(res, check.status, "locked", ctx->request_id); return; }
            guard.login(req, res, "alice", { "cook" });
            res.json("{\"ok\":true}");
        } else if (path == "/me") {
            if (!guard.require_auth(req, res)) return;
            if (!guard.require_csrf(req, res)) return;
            res.json("{\"user\":\"" + ctx->session->user + "\"}");
        } else if (path == "/logout") {
            req.discard();
            guard.logout(req, res);
            res.json("{\"ok\":true}");
        } else if (path == "/pay") {
            guard.idempotent(req, res, [&](const std::string& body, std::function<void(StoredResponse)> reply) {
                const int n = ++charges;
                reply(StoredResponse{ 201, { { "Content-Type", "application/json" } }, "{\"charge\":" + std::to_string(n) + ",\"body\":" + body + "}" });
            });
        } else if (path == "/boom") {
            throw std::runtime_error("kaboom 123");
        } else {
            Guard::send_error(res, 404, "not_found", ctx->request_id);
        }
    };

    https::ServerOptions so;
    so.secure.cert_file = cert_file;
    so.secure.key_file = key_file;
    http::Server server = https::create_server(so, guard.wrap(handler));
    std::promise<void> ready;
    server.listen(0, "127.0.0.1", [&] { ready.set_value(); });
    if (ready.get_future().wait_for(5s) != std::future_status::ready) { std::printf("  server did not start\n"); ++g_fail; return; }
    const std::string base = "https://127.0.0.1:" + std::to_string(server.port());
    std::printf("  listening on %s\n", base.c_str());

    auto r = call(base + "/fp");
    std::printf("  /fp -> %d %s\n", r.status, r.body.c_str());
    CHECK(r.status == 200);
    CHECK(r.body.find("\"ja4\":\"t13") != std::string::npos);
    CHECK(!r.headers.get("x-request-id").empty());
    CHECK(r.headers.get("x-content-type-options") == "nosniff");
    CHECK(!r.headers.get("strict-transport-security").empty());
    CHECK(r.headers.get("cache-control") == "no-store");
    CHECK(!r.headers.get("ratelimit-remaining").empty());

    r = call(base + "/me");
    CHECK(r.status == 401);

    r = call(base + "/login", "POST");
    const std::string cookie = session_cookie(r);
    const std::string csrf = r.headers.get("x-csrf-token");
    std::printf("  /login -> %d cookies=%zu csrf=%s\n", r.status, r.headers.get_all("set-cookie").size(), csrf.empty() ? "missing" : "present");
    CHECK(r.status == 200 && !cookie.empty() && !csrf.empty());
    CHECK(r.headers.get_all("set-cookie").size() == 2);

    r = call(base + "/me", "GET", { { "Cookie", cookie } });
    CHECK(r.status == 200 && r.body.find("alice") != std::string::npos);
    r = call(base + "/me", "POST", { { "Cookie", cookie } });
    CHECK(r.status == 403 && r.body.find("csrf_failed") != std::string::npos);
    r = call(base + "/me", "POST", { { "Cookie", cookie }, { "X-CSRF-Token", csrf } });
    CHECK(r.status == 200);

    r = call(base + "/logout", "POST", { { "Cookie", cookie } });
    std::printf("  /logout -> %d clear-site-data=%s\n", r.status, r.headers.get("clear-site-data").c_str());
    CHECK(r.headers.get("clear-site-data") == "\"cookies\", \"storage\"");
    bool expired = false;
    for (const auto& c : r.headers.get_all("set-cookie")) if (c.rfind("__Host-session=;", 0) == 0 && c.find("Max-Age=0") != std::string::npos) expired = true;
    CHECK(expired);
    r = call(base + "/me", "GET", { { "Cookie", cookie } });
    CHECK(r.status == 401);

    r = call(base + "/pay", "POST", { { "Idempotency-Key", "order-77" }, { "Content-Type", "application/json" } }, "{\"amount\":5}");
    auto r2 = call(base + "/pay", "POST", { { "Idempotency-Key", "order-77" }, { "Content-Type", "application/json" } }, "{\"amount\":5}");
    auto r3 = call(base + "/pay", "POST", { { "Idempotency-Key", "order-77" } }, "{\"amount\":6}");
    std::printf("  /pay -> %d %s | replay %d %s | changed body %d\n", r.status, r.body.c_str(), r2.status, r2.headers.get("idempotent-replayed").c_str(), r3.status);
    CHECK(r.status == 201 && r2.status == 201 && r.body == r2.body && charges == 1);
    CHECK(r2.headers.get("idempotent-replayed") == "true");
    CHECK(r3.status == 422);

    r = call(base + "/boom");
    std::printf("  /boom -> %d %s\n", r.status, r.body.c_str());
    CHECK(r.status == 500 && r.body.find("request_id") != std::string::npos && r.body.find("kaboom") == std::string::npos);
    CHECK(guard.errors().groups().size() == 1);

    for (int i = 0; i < 5; ++i) call(base + "/login", "POST");
    r = call(base + "/login", "POST");
    CHECK(r.status == 429 && !r.headers.get("retry-after").empty());

    int limited = 0;
    for (int i = 0; i < 25 && !limited; ++i) {
        r = call(base + "/fp");
        if (r.status == 429) limited = i + 1;
    }
    std::printf("  global limit hit after %d more requests, Retry-After=%s\n", limited, r.headers.get("retry-after").c_str());
    CHECK(limited > 0 && !r.headers.get("retry-after").empty());

    std::this_thread::sleep_for(100ms);
    const auto lines = mem->lines();
    bool leaked = false, has_access = false;
    const std::string token = cookie.substr(cookie.find('=') + 1);
    for (const auto& l : lines) {
        if (l.find(token) != std::string::npos) leaked = true;
        if (l.find("\"msg\":\"request\"") != std::string::npos && l.find("\"ja4\":\"t13") != std::string::npos) has_access = true;
    }
    if (!lines.empty()) std::printf("  sample log: %s\n", lines.front().c_str());
    CHECK(!leaked && has_access);

    std::promise<void> closed;
    server.close([&](const std::error_code&) { closed.set_value(); });
    server.close_all_connections();
    closed.get_future().wait_for(3s);
}

int main(int argc, char** argv) {
    std::printf("rate limiter\n"); test_rate_limiter();
    std::printf("failures\n"); test_failures();
    std::printf("ja4\n"); test_ja4();
    std::printf("devices\n"); test_devices();
    std::printf("sessions + cookies\n"); test_sessions_cookies();
    std::printf("ownership\n"); test_ownership();
    std::printf("query\n"); test_query();
    std::printf("concurrency\n"); test_concurrency();
    std::printf("idempotency\n"); test_idempotency();
    std::printf("logger + errors\n"); test_logger_errors();
    std::printf("jobs + budgets\n"); test_jobs_budgets();
    std::printf("job checks + dead letters\n"); test_job_checks();
    std::printf("debounce + signals\n"); test_debounce();
    if (argc >= 3) {
        std::printf("https end to end\n");
        test_https(argv[1], argv[2]);
    } else {
        std::printf("https end to end skipped (pass cert.pem key.pem to run it)\n");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    stop_background();
    return g_fail ? 1 : 0;
}
