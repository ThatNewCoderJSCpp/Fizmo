#ifndef FIZMO_GUARD_HTTP_GUARD_HPP
#define FIZMO_GUARD_HTTP_GUARD_HPP

#include "guard_types.hpp"
#include "rate_limiter.hpp"
#include "failure_tracker.hpp"
#include "tls_fingerprint.hpp"
#include "device_registry.hpp"
#include "cookies.hpp"
#include "session_store.hpp"
#include "ownership.hpp"
#include "query.hpp"
#include "idempotency.hpp"
#include "logger.hpp"
#include "error_tracker.hpp"
#include "budget_monitor.hpp"
#include "../Node/http.hpp"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct RouteLimit {
    std::string     method;
    std::string     path_prefix;
    RateLimitPolicy policy;
    double          cost = 1.0;
    std::string     name;
};

struct SecurityHeaders {
    bool        enabled = true;
    std::string content_security_policy = "default-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self'";
    std::string frame_options = "DENY";
    std::string referrer_policy = "strict-origin-when-cross-origin";
    std::string permissions_policy = "camera=(), microphone=(), geolocation=()";
    std::string hsts = "max-age=31536000; includeSubDomains";
    std::string cross_origin_opener_policy = "same-origin";
};

FailurePolicy default_login_account_policy();

struct GuardOptions {
    bool                      trust_proxy = false;
    std::set<std::string>     trusted_proxies;

    bool                      rate_limit = true;
    RateLimitPolicy           global_limit = RateLimitPolicy::per_second(10.0, 40.0);
    std::vector<RouteLimit>   route_limits;
    bool                      rate_limit_headers = true;

    bool                      track_failures = true;
    FailurePolicy             failure_policy;
    WindowPolicy              login_ip_window = WindowPolicy{ 20, std::chrono::minutes(15), 100000 };
    FailurePolicy             login_account_policy = default_login_account_policy();

    FingerprintPolicy         fingerprint_policy;
    bool                      block_fingerprint_blocklist = true;
    bool                      block_high_risk = false;

    SecurityHeaders           headers;
    bool                      no_store_by_default = true;

    bool                      access_log = true;
    bool                      accept_client_request_id = false;
    std::string               request_id_header = "X-Request-Id";

    SessionPolicy             session_policy;
    DevicePolicy              device_policy;
    IdempotencyPolicy         idempotency_policy;
    ErrorTrackerPolicy        error_policy;
    QueryRecorderPolicy       query_policy;

    std::string               csrf_header = "X-CSRF-Token";
    std::set<std::string>     allowed_origins;
    std::size_t               max_body = 1u << 20;
    std::chrono::seconds      maintenance_interval = std::chrono::seconds(60);
};

struct RequestContext {
    explicit RequestContext(QueryRecorderPolicy query_policy = QueryRecorderPolicy()) : queries(query_policy) {}

    std::string            request_id;
    std::string            client_ip;
    std::string            method;
    std::string            path;
    std::string            user_agent;
    SteadyTime             started{};
    WallTime               received{};
    bool                   encrypted = false;
    TLSFingerprint         tls;
    FingerprintVerdict     verdict;
    std::optional<Session> session;
    std::string            session_token;
    Principal              principal;
    std::string            device_id;
    QueryRecorder          queries;
    Logger                 log;

    bool authenticated() const noexcept { return session.has_value(); }
};

struct LoginCheck {
    bool                      allowed = true;
    int                       status = 200;
    std::chrono::milliseconds retry_after{0};
    std::chrono::milliseconds delay{0};
    std::string               reason;

    explicit operator bool() const noexcept { return allowed; }
};

struct LoginResult {
    IssuedSession issued;
    DeviceCheck   device;
};

using IdempotentHandler = std::function<void(const std::string& body, std::function<void(StoredResponse)> reply)>;

class Guard {
public:
    explicit Guard(GuardOptions options = GuardOptions(), std::shared_ptr<Logger> logger = nullptr);

    http::RequestHandler wrap(http::RequestHandler handler) const;
    static RequestContext* context(http::IncomingMessage& req);

    std::string client_ip(http::IncomingMessage& req) const;

    std::optional<Session> authenticate(http::IncomingMessage& req) const;
    bool require_auth(http::IncomingMessage& req, http::ServerResponse& res) const;
    bool require_csrf(http::IncomingMessage& req, http::ServerResponse& res) const;
    bool require_same_origin(http::IncomingMessage& req, http::ServerResponse& res) const;
    bool authorize(http::IncomingMessage& req, http::ServerResponse& res, const std::string& type, const std::string& id, Action action) const;

    LoginCheck check_login(http::IncomingMessage& req, const std::string& account) const;
    void login_failed(http::IncomingMessage& req, const std::string& account) const;
    LoginResult login(http::IncomingMessage& req, http::ServerResponse& res, const std::string& user, std::set<std::string> roles = {}) const;
    void logout(http::IncomingMessage& req, http::ServerResponse& res, bool everywhere = false, ClearSiteData clear = ClearSiteData::Cookies | ClearSiteData::Storage) const;

    void idempotent(http::IncomingMessage& req, http::ServerResponse& res, IdempotentHandler handler) const;
    static void send_stored(http::ServerResponse& res, const StoredResponse& stored);
    static void send_error(http::ServerResponse& res, int status, const std::string& code, const std::string& request_id = {});

    void start_maintenance() const;
    void stop_maintenance() const;
    void sweep() const;

    RateLimiter&         rate_limiter() const;
    FailureTracker&      failures() const;
    SlidingWindowLimiter& login_ip_limiter() const;
    FailureTracker&      login_failures() const;
    FingerprintRegistry& fingerprints() const;
    SessionStore&        sessions() const;
    DeviceRegistry&      devices() const;
    OwnershipGuard&      ownership() const;
    IdempotencyStore&    idempotency() const;
    ErrorTracker&        errors() const;
    BudgetMonitor&       budgets() const;
    Logger&              logger() const;
    const GuardOptions&  options() const;

    struct State;

private:
    std::shared_ptr<State> m_state;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_HTTP_GUARD_HPP
