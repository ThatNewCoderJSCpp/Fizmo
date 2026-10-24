#include "fizmo_library.hpp"
#include "http_guard.hpp"
#include "../Node/loop.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string normalize_ip(std::string ip) {
    ip = trim(ip);
    if (ip.rfind("::ffff:", 0) == 0 && ip.find('.') != std::string::npos) ip = ip.substr(7);
    if (ip.size() > 2 && ip.front() == '[') {
        const std::size_t end = ip.find(']');
        if (end != std::string::npos) ip = ip.substr(1, end - 1);
    }
    return ip;
}

bool unsafe_method(const std::string& m) {
    return !(m == "GET" || m == "HEAD" || m == "OPTIONS" || m == "TRACE");
}

std::int64_t ceil_seconds(std::chrono::milliseconds ms) {
    return static_cast<std::int64_t>((ms.count() + 999) / 1000);
}

std::vector<std::string> split_list(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t pos = 0;

    while (pos <= s.size()) {
        const std::size_t next = s.find(sep, pos);
        const std::string part = trim(s.substr(pos, next == std::string::npos ? std::string::npos : next - pos));
        if (!part.empty()) out.push_back(part);
        if (next == std::string::npos) break;
        pos = next + 1;
    }

    return out;
}

std::string join_set(const std::set<std::string>& s) {
    std::string out;

    for (const auto& v : s) {
        if (!out.empty()) out += ',';
        out += v;
    }

    return out;
}

std::string origin_host(const std::string& origin) {
    const std::size_t scheme = origin.find("://");
    std::string rest = scheme == std::string::npos ? origin : origin.substr(scheme + 3);
    const std::size_t slash = rest.find('/');
    if (slash != std::string::npos) rest = rest.substr(0, slash);
    return lower(rest);
}

void apply_rate_headers(http::ServerResponse& res, const RateLimitDecision& d) {
    res.set_header("RateLimit-Limit", std::to_string(static_cast<std::int64_t>(d.limit)));
    res.set_header("RateLimit-Remaining", std::to_string(static_cast<std::int64_t>(std::floor(d.remaining))));
    res.set_header("RateLimit-Reset", std::to_string(ceil_seconds(d.reset_after)));
}

} // namespace

FailurePolicy default_login_account_policy() {
    FailurePolicy p;
    p.window = std::chrono::minutes(15);
    p.max_failures = 5;
    p.min_samples = 0;
    p.lockout = std::chrono::minutes(1);
    p.lockout_multiplier = 2.0;
    p.max_lockout = std::chrono::hours(1);
    p.slowdown_after = 2;
    p.slowdown_step = std::chrono::milliseconds(500);
    p.max_slowdown = std::chrono::milliseconds(5000);
    return p;
}

struct Guard::State {
    GuardOptions                                                     opt;
    std::shared_ptr<Logger>                                          logger;
    RateLimiter                                                      global;
    std::vector<std::pair<RouteLimit, std::unique_ptr<RateLimiter>>> routes;
    FailureTracker                                                   failures;
    SlidingWindowLimiter                                             login_ip;
    FailureTracker                                                   login_accounts;
    FingerprintRegistry                                              fingerprints;
    SessionStore                                                     sessions;
    DeviceRegistry                                                   devices;
    OwnershipGuard                                                   ownership;
    IdempotencyStore                                                 idempotency;
    ErrorTracker                                                     errors;
    BudgetMonitor                                                    budgets;
    std::mutex                                                       timer_m;
    Timer                                                            maintenance;

    explicit State(GuardOptions o, std::shared_ptr<Logger> l)
        : opt(std::move(o)),
          logger(l ? std::move(l) : std::make_shared<Logger>()),
          global(opt.global_limit),
          failures(opt.failure_policy),
          login_ip(opt.login_ip_window),
          login_accounts(opt.login_account_policy),
          fingerprints(opt.fingerprint_policy),
          sessions(opt.session_policy),
          devices(opt.device_policy),
          idempotency(opt.idempotency_policy),
          errors(opt.error_policy) {
        for (const auto& r : opt.route_limits) {
            routes.emplace_back(r, std::make_unique<RateLimiter>(r.policy));
        }

        errors.set_logger(logger);
    }

    void sweep() {
        global.sweep();
        for (auto& r : routes) r.second->sweep();
        failures.sweep();
        login_ip.sweep();
        login_accounts.sweep();
        sessions.sweep();
        devices.sweep();
        idempotency.sweep();
    }

    std::string token_of(http::IncomingMessage& req, bool* from_cookie) const {
        const std::string cookie = sessions.token_from_cookie_header(req.header("cookie"));

        if (!cookie.empty()) {
            if (from_cookie) *from_cookie = true;
            return cookie;
        }

        const std::string auth = req.header("authorization");

        if (auth.size() > 7 && lower(auth.substr(0, 7)) == "bearer ") {
            if (from_cookie) *from_cookie = false;
            return trim(auth.substr(7));
        }

        if (from_cookie) *from_cookie = false;
        return {};
    }
};

Guard::Guard(GuardOptions options, std::shared_ptr<Logger> logger)
    : m_state(std::make_shared<State>(std::move(options), std::move(logger))) {}

RequestContext* Guard::context(http::IncomingMessage& req) {
    if (!req) return nullptr;
    auto* p = std::any_cast<std::shared_ptr<RequestContext>>(&req.data());
    return p ? p->get() : nullptr;
}

std::string Guard::client_ip(http::IncomingMessage& req) const {
    const net::Socket sock = req.socket();
    const std::string remote = normalize_ip(sock ? sock.remote_address() : std::string());
    const GuardOptions& o = m_state->opt;
    if (!o.trust_proxy) return remote;
    if (!o.trusted_proxies.empty() && !o.trusted_proxies.count(remote)) return remote;
    std::vector<std::string> chain;

    for (const auto& h : req.headers().get_all("x-forwarded-for")) {
        for (auto& part : split_list(h, ',')) chain.push_back(normalize_ip(part));
    }

    if (chain.empty()) {
        const std::string real = normalize_ip(req.header("x-real-ip"));
        return real.empty() ? remote : real;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (o.trusted_proxies.empty()) return *it;
        if (!o.trusted_proxies.count(*it)) return *it;
    }

    return chain.front();
}

void Guard::send_error(http::ServerResponse& res, int status, const std::string& code, const std::string& request_id) {
    if (!res || res.writable_ended()) return;

    if (res.headers_sent()) {
        res.end();
        return;
    }

    std::string body = "{\"error\":\"" + json_escape(code) + "\"";
    if (!request_id.empty()) body += ",\"request_id\":\"" + json_escape(request_id) + "\"";
    body += "}";
    res.set_header("Cache-Control", "no-store");
    res.json(body, status);
}

void Guard::send_stored(http::ServerResponse& res, const StoredResponse& stored) {
    if (!res || res.writable_ended()) return;
    res.set_status_code(stored.status);

    for (const auto& h : stored.headers) {
        if (http::Headers::iequals(h.first, "set-cookie")) continue;
        res.set_header(h.first, h.second);
    }

    res.end(DataView(stored.body));
}

http::RequestHandler Guard::wrap(http::RequestHandler handler) const {
    std::shared_ptr<State> st = m_state;
    const Guard self = *this;

    return [st, self, handler](http::IncomingMessage req, http::ServerResponse res) {
        const GuardOptions& o = st->opt;
        auto ctx = std::make_shared<RequestContext>(o.query_policy);
        ctx->started = SteadyClock::now();
        ctx->received = WallClock::now();
        ctx->method = req.method();
        ctx->path = req.path();
        ctx->user_agent = req.header("user-agent");
        ctx->client_ip = self.client_ip(req);

        const std::string supplied = req.header(o.request_id_header);
        bool supplied_ok = o.accept_client_request_id && !supplied.empty() && supplied.size() <= 64;

        if (supplied_ok) {
            for (unsigned char c : supplied) {
                if (!(std::isalnum(c) || c == '-' || c == '_' || c == '.')) { supplied_ok = false; break; }
            }
        }

        ctx->request_id = supplied_ok ? supplied : Logger::new_request_id();
        ctx->log = st->logger->child({ field("request_id", ctx->request_id), field("ip", ctx->client_ip) });
        req.data() = ctx;
        res.set_header(o.request_id_header, ctx->request_id);

        const net::Socket sock = req.socket();
        ctx->encrypted = sock && sock.encrypted();

        if (o.headers.enabled) {
            const SecurityHeaders& h = o.headers;
            res.set_header("X-Content-Type-Options", "nosniff");
            if (!h.frame_options.empty()) res.set_header("X-Frame-Options", h.frame_options);
            if (!h.referrer_policy.empty()) res.set_header("Referrer-Policy", h.referrer_policy);
            if (!h.permissions_policy.empty()) res.set_header("Permissions-Policy", h.permissions_policy);
            if (!h.cross_origin_opener_policy.empty()) res.set_header("Cross-Origin-Opener-Policy", h.cross_origin_opener_policy);
            if (!h.content_security_policy.empty()) res.set_header("Content-Security-Policy", h.content_security_policy);
            if (ctx->encrypted && !h.hsts.empty()) res.set_header("Strict-Transport-Security", h.hsts);
        }

        if (o.no_store_by_default) res.set_header("Cache-Control", "no-store");

        security::ClientHelloInfo hello;

        if (ctx->encrypted && sock.client_hello(hello)) {
            ctx->tls = fingerprint(hello);
            ctx->verdict = st->fingerprints.assess(ctx->tls, ctx->user_agent);
        } else {
            ctx->verdict = st->fingerprints.assess_plaintext(ctx->user_agent);
        }

        std::weak_ptr<http::ServerResponse::State> weak_res = res.state();
        std::weak_ptr<RequestContext> weak_ctx = ctx;

        res.on_finish([st, weak_res, weak_ctx] {
            auto c = weak_ctx.lock();
            auto rs = weak_res.lock();
            if (!c || !rs) return;
            http::ServerResponse r(rs);
            const int status = r.status_code();
            const GuardOptions& op = st->opt;
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(SteadyClock::now() - c->started);
            const std::string ip_key = "ip:" + c->client_ip;

            if (op.track_failures && status != 429) {
                if (status >= 400 && status < 500) st->failures.record_failure(ip_key);
                else if (status < 400) st->failures.record_success(ip_key);
            }

            st->budgets.add("http.requests", 1.0);
            if (status >= 500) st->budgets.add("http.server_errors", 1.0);
            if (status == 429 || status == 403) st->budgets.add("http.rejected", 1.0);

            for (const auto& w : c->queries.warnings()) {
                c->log.warn(w.message, { field("query_warning", to_string(w.kind)), field("query_shape", w.shape), field("query_count", static_cast<std::uint64_t>(w.count)), field("path", c->path) });
            }

            if (op.access_log) {
                LogFields f = {
                    field("method", c->method),
                    field("path", c->path),
                    field("status", status),
                    field("duration_ms", static_cast<double>(elapsed.count()) / 1000.0),
                    field("ua", c->user_agent.substr(0, 200)),
                    field("risk", to_string(c->verdict.level)),
                    field("queries", static_cast<std::uint64_t>(c->queries.total()))
                };

                if (!c->tls.empty()) f.push_back(field("ja4", c->tls.ja4));
                if (c->session) f.push_back(field("user", c->session->user));
                const LogLevel lvl = status >= 500 ? LogLevel::Error : (status >= 400 ? LogLevel::Warn : LogLevel::Info);
                c->log.log(lvl, "request", std::move(f));
            }
        });

        if (ctx->verdict.blocked() && o.block_fingerprint_blocklist) {
            send_error(res, 403, "forbidden", ctx->request_id);
            return;
        }

        if (ctx->verdict.likely_bot() && o.block_high_risk) {
            send_error(res, 403, "forbidden", ctx->request_id);
            return;
        }

        if (o.track_failures) {
            const FailureStatus fs = st->failures.status("ip:" + ctx->client_ip);

            if (fs.locked) {
                res.set_header("Retry-After", std::to_string(std::max<std::int64_t>(1, ceil_seconds(fs.lock_remaining))));
                send_error(res, 429, "too_many_failures", ctx->request_id);
                return;
            }
        }

        if (o.rate_limit) {
            const std::string key = "ip:" + ctx->client_ip;
            RateLimitDecision d = st->global.consume(key);
            RateLimitDecision tightest = d;

            if (d.allowed) {
                for (auto& r : st->routes) {
                    const RouteLimit& rl = r.first;
                    if (!rl.method.empty() && !http::Headers::iequals(rl.method, ctx->method)) continue;
                    if (!rl.path_prefix.empty() && ctx->path.rfind(rl.path_prefix, 0) != 0) continue;
                    const RateLimitDecision rd = r.second->consume(key, rl.cost);
                    if (!rd.allowed || rd.remaining < tightest.remaining) tightest = rd;

                    if (!rd.allowed) {
                        d = rd;
                        st->global.refund(key);
                        break;
                    }
                }
            }

            if (o.rate_limit_headers) apply_rate_headers(res, tightest);

            if (!d.allowed) {
                res.set_header("Retry-After", std::to_string(std::max<std::int64_t>(1, ceil_seconds(d.retry_after))));
                send_error(res, 429, "rate_limited", ctx->request_id);
                return;
            }
        }

        if (!handler) {
            send_error(res, 404, "not_found", ctx->request_id);
            return;
        }

        try {
            handler(req, res);
        } catch (...) {
            const std::string group = st->errors.capture_current("http " + ctx->method + " " + ctx->path, ctx->request_id);
            (void)group;
            send_error(res, 500, "internal_error", ctx->request_id);
        }
    };
}

std::optional<Session> Guard::authenticate(http::IncomingMessage& req) const {
    RequestContext* ctx = context(req);
    if (ctx && ctx->session) return ctx->session;
    const std::string token = m_state->token_of(req, nullptr);
    if (token.empty()) return std::nullopt;
    auto s = m_state->sessions.validate(token);
    if (!s) return std::nullopt;

    if (ctx) {
        ctx->session = s;
        ctx->session_token = token;
        ctx->principal.user_id = s->user;
        ctx->principal.roles.clear();
        for (const auto& r : split_list(s->attribute("roles"), ',')) ctx->principal.roles.insert(r);
        ctx->device_id = s->device_id;
    }

    return s;
}

bool Guard::require_auth(http::IncomingMessage& req, http::ServerResponse& res) const {
    RequestContext* ctx = context(req);
    const std::string rid = ctx ? ctx->request_id : std::string();
    bool from_cookie = false;
    const std::string token = m_state->token_of(req, &from_cookie);
    auto s = authenticate(req);

    if (!s) {
        if (from_cookie && !token.empty()) res.append_header("Set-Cookie", m_state->sessions.clear_cookie_header());
        send_error(res, 401, "unauthenticated", rid);
        return false;
    }

    if (from_cookie && m_state->sessions.needs_rotation(*s)) {
        auto rotated = m_state->sessions.rotate(token);

        if (rotated) {
            res.append_header("Set-Cookie", m_state->sessions.set_cookie_header(*rotated));
            res.set_header(m_state->opt.csrf_header, rotated->session.csrf_token);

            if (ctx) {
                ctx->session = rotated->session;
                ctx->session_token = rotated->token;
            }
        }
    }

    return true;
}

bool Guard::require_csrf(http::IncomingMessage& req, http::ServerResponse& res) const {
    if (!unsafe_method(req.method())) return true;
    RequestContext* ctx = context(req);
    const std::string rid = ctx ? ctx->request_id : std::string();
    bool from_cookie = false;
    const std::string token = m_state->token_of(req, &from_cookie);
    if (!from_cookie || token.empty()) return true;
    const std::string sent = req.header(m_state->opt.csrf_header);
    const std::string current = ctx && ctx->session_token.size() ? ctx->session_token : token;

    if (!m_state->sessions.verify_csrf(current, sent)) {
        send_error(res, 403, "csrf_failed", rid);
        return false;
    }

    return true;
}

bool Guard::require_same_origin(http::IncomingMessage& req, http::ServerResponse& res) const {
    if (!unsafe_method(req.method())) return true;
    RequestContext* ctx = context(req);
    const std::string rid = ctx ? ctx->request_id : std::string();
    const std::string site = lower(req.header("sec-fetch-site"));

    if (site == "cross-site") {
        send_error(res, 403, "cross_site_request", rid);
        return false;
    }

    std::string origin = req.header("origin");
    if (origin.empty() || origin == "null") origin = req.header("referer");
    if (origin.empty()) return site.empty() || site == "same-origin" || site == "none";

    const std::string host = origin_host(origin);
    const std::string own = lower(req.header("host"));
    if (!host.empty() && host == own) return true;

    for (const auto& allowed : m_state->opt.allowed_origins) {
        if (origin_host(allowed) == host) return true;
    }

    send_error(res, 403, "cross_site_request", rid);
    return false;
}

bool Guard::authorize(http::IncomingMessage& req, http::ServerResponse& res, const std::string& type, const std::string& id, Action action) const {
    RequestContext* ctx = context(req);
    authenticate(req);
    Principal who = ctx ? ctx->principal : Principal();

    if (!ctx) {
        if (auto s = authenticate(req)) who.user_id = s->user;
    }

    const AccessDecision d = m_state->ownership.check(who, type, id, action);
    if (d.allowed()) return true;

    if (ctx) ctx->log.warn("access denied", { field("resource", type), field("action", to_string(action)), field("reason", d.reason), field("user", who.user_id) });
    send_error(res, d.http_status(), d.result == AccessResult::Unauthenticated ? "unauthenticated" : "not_found", ctx ? ctx->request_id : std::string());
    return false;
}

LoginCheck Guard::check_login(http::IncomingMessage& req, const std::string& account) const {
    LoginCheck out;
    RequestContext* ctx = context(req);
    const std::string ip = ctx ? ctx->client_ip : client_ip(req);
    const RateLimitDecision d = m_state->login_ip.hit("login:" + ip);

    if (!d.allowed) {
        out.allowed = false;
        out.status = 429;
        out.retry_after = d.retry_after;
        out.reason = "too many login attempts from this address";
        return out;
    }

    const FailureStatus fs = m_state->login_accounts.status("acct:" + lower(account));

    if (fs.locked) {
        out.allowed = false;
        out.status = 429;
        out.retry_after = fs.lock_remaining;
        out.reason = "account temporarily locked";
        return out;
    }

    out.delay = fs.suggested_delay;
    return out;
}

void Guard::login_failed(http::IncomingMessage& req, const std::string& account) const {
    RequestContext* ctx = context(req);
    const std::string ip = ctx ? ctx->client_ip : client_ip(req);
    const FailureStatus fs = m_state->login_accounts.record_failure("acct:" + lower(account));
    m_state->failures.record_failure("ip:" + ip);
    if (ctx) ctx->log.warn("login failed", { field("failures", fs.failures), field("locked", fs.locked) });
}

LoginResult Guard::login(http::IncomingMessage& req, http::ServerResponse& res, const std::string& user, std::set<std::string> roles) const {
    State& st = *m_state;
    RequestContext* ctx = context(req);
    LoginResult out;
    st.login_accounts.record_success("acct:" + lower(user), true);
    const std::string ip = ctx ? ctx->client_ip : client_ip(req);

    bool from_cookie = false;
    const std::string old = st.token_of(req, &from_cookie);
    if (!old.empty()) st.sessions.revoke(old, RevokeReason::Rotated);

    DeviceSignals sig;
    sig.ip = ip;
    sig.user_agent = req.header("user-agent");
    if (ctx) sig.ja4 = ctx->tls.ja4;
    const std::string device_cookie = cookie_value(req.header("cookie"), st.opt.device_policy.cookie_name);
    out.device = st.devices.identify(user, device_cookie, sig);

    std::map<std::string, std::string> attrs;
    if (!roles.empty()) attrs["roles"] = join_set(roles);
    out.issued = st.sessions.create(user, out.device.device_id, std::move(attrs));
    res.append_header("Set-Cookie", st.sessions.set_cookie_header(out.issued));

    if (!out.device.issued_token.empty()) {
        CookieOptions dc = st.opt.session_policy.cookie;
        dc.max_age = std::chrono::duration_cast<std::chrono::seconds>(st.opt.device_policy.forget_after);
        res.append_header("Set-Cookie", make_set_cookie(st.opt.device_policy.cookie_name, out.device.issued_token, dc));
    }
    res.set_header(st.opt.csrf_header, out.issued.session.csrf_token);

    if (ctx) {
        ctx->session = out.issued.session;
        ctx->session_token = out.issued.token;
        ctx->principal.user_id = user;
        ctx->principal.roles = roles;
        ctx->device_id = out.device.device_id;
        ctx->log.info("login", { field("user", user), field("device", to_string(out.device.status)), field("trusted_device", out.device.trusted) });
    }

    return out;
}

void Guard::logout(http::IncomingMessage& req, http::ServerResponse& res, bool everywhere, ClearSiteData clear) const {
    State& st = *m_state;
    RequestContext* ctx = context(req);
    const std::string token = ctx && !ctx->session_token.empty() ? ctx->session_token : st.token_of(req, nullptr);

    if (!token.empty()) {
        const auto s = st.sessions.peek(token);
        if (everywhere && s) st.sessions.revoke_user(s->user, {}, RevokeReason::LogoutEverywhere);
        else st.sessions.revoke(token, RevokeReason::Logout);
        if (ctx && s) ctx->log.info(everywhere ? "logout everywhere" : "logout", { field("user", s->user) });
    }

    res.append_header("Set-Cookie", st.sessions.clear_cookie_header());

    if (has_flag(clear, ClearSiteData::Cookies)) {
        res.append_header("Set-Cookie", make_expired_cookie(st.opt.device_policy.cookie_name, st.opt.session_policy.cookie));
    }

    const std::string csd = clear_site_data_header(clear);
    if (!csd.empty()) res.set_header("Clear-Site-Data", csd);
    res.set_header("Cache-Control", "no-store");

    if (ctx) {
        ctx->session.reset();
        ctx->session_token.clear();
        ctx->principal = Principal();
    }
}

void Guard::idempotent(http::IncomingMessage& req, http::ServerResponse& res, IdempotentHandler handler) const {
    std::shared_ptr<State> st = m_state;
    RequestContext* ctx = context(req);
    const std::string rid = ctx ? ctx->request_id : std::string();
    const std::string key = req.header("idempotency-key");
    std::string scope = ctx && ctx->session ? "user:" + ctx->session->user : "ip:" + (ctx ? ctx->client_ip : client_ip(req));
    const std::string method = req.method();
    const std::string path = req.path();
    std::weak_ptr<http::ServerResponse::State> weak = res.state();

    req.body([st, handler, key, scope, method, path, weak, rid](const std::error_code& ec, std::string body) {
        auto rs = weak.lock();
        if (!rs) return;
        http::ServerResponse r(rs);

        if (ec) {
            send_error(r, 413, "body_too_large", rid);
            return;
        }

        if (key.empty()) {
            handler(body, [weak](StoredResponse out) {
                if (auto s = weak.lock()) {
                    http::ServerResponse rr(s);
                    send_stored(rr, out);
                }
            });
            return;
        }

        const std::string fp = IdempotencyStore::request_fingerprint(method, path, body);
        const IdempotencyBegin b = st->idempotency.begin(scope, key, fp);

        switch (b.state) {
            case IdempotencyState::Replay:
                r.set_header("Idempotent-Replayed", "true");
                send_stored(r, *b.response);
                return;
            case IdempotencyState::InProgress:
                r.set_header("Retry-After", "1");
                send_error(r, 409, "request_in_progress", rid);
                return;
            case IdempotencyState::Mismatch:
                send_error(r, 422, "idempotency_key_reused", rid);
                return;
            case IdempotencyState::Invalid:
                send_error(r, 400, "invalid_idempotency_key", rid);
                return;
            default:
                break;
        }

        auto replied = std::make_shared<bool>(false);

        r.on_close([st, scope, key, replied] {
            if (!*replied) st->idempotency.abandon(scope, key);
        });

        try {
            handler(body, [st, scope, key, weak, replied](StoredResponse out) {
                if (*replied) return;
                *replied = true;
                if (out.status >= 500) st->idempotency.abandon(scope, key);
                else st->idempotency.complete(scope, key, out);

                if (auto s = weak.lock()) {
                    http::ServerResponse rr(s);
                    send_stored(rr, out);
                }
            });
        } catch (...) {
            st->idempotency.abandon(scope, key);
            st->errors.capture_current("idempotent " + method + " " + path, rid);
            send_error(r, 500, "internal_error", rid);
        }
    }, st->opt.max_body);
}

void Guard::sweep() const {
    m_state->sweep();
}

void Guard::start_maintenance() const {
    std::weak_ptr<State> weak = m_state;
    std::lock_guard<std::mutex> lk(m_state->timer_m);
    m_state->maintenance.cancel();
    const auto every = std::chrono::duration_cast<std::chrono::milliseconds>(m_state->opt.maintenance_interval);

    m_state->maintenance = set_interval([weak] {
        if (auto st = weak.lock()) st->sweep();
    }, every.count() > 0 ? every : std::chrono::milliseconds(60000));
}

void Guard::stop_maintenance() const {
    std::lock_guard<std::mutex> lk(m_state->timer_m);
    m_state->maintenance.cancel();
}

RateLimiter& Guard::rate_limiter() const { return m_state->global; }
FailureTracker& Guard::failures() const { return m_state->failures; }
SlidingWindowLimiter& Guard::login_ip_limiter() const { return m_state->login_ip; }
FailureTracker& Guard::login_failures() const { return m_state->login_accounts; }
FingerprintRegistry& Guard::fingerprints() const { return m_state->fingerprints; }
SessionStore& Guard::sessions() const { return m_state->sessions; }
DeviceRegistry& Guard::devices() const { return m_state->devices; }
OwnershipGuard& Guard::ownership() const { return m_state->ownership; }
IdempotencyStore& Guard::idempotency() const { return m_state->idempotency; }
ErrorTracker& Guard::errors() const { return m_state->errors; }
BudgetMonitor& Guard::budgets() const { return m_state->budgets; }
Logger& Guard::logger() const { return *m_state->logger; }
const GuardOptions& Guard::options() const { return m_state->opt; }

} // namespace guard
} // namespace networking
} // namespace fizmo
