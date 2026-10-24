#ifndef FIZMO_GUARD_SESSION_STORE_HPP
#define FIZMO_GUARD_SESSION_STORE_HPP

#include "guard_types.hpp"
#include "cookies.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct SessionPolicy {
    std::chrono::seconds idle_timeout     = std::chrono::hours(2);
    std::chrono::seconds absolute_timeout = std::chrono::hours(24 * 30);
    std::chrono::seconds rotate_every     = std::chrono::hours(1);
    std::size_t          max_per_user     = 50;
    std::size_t          token_bytes      = 32;
    std::string          cookie_name      = "__Host-session";
    CookieOptions        cookie;
};

struct Session {
    std::string                        id;
    std::string                        user;
    std::string                        device_id;
    std::string                        csrf_token;
    WallTime                           created{};
    WallTime                           last_seen{};
    WallTime                           rotated{};
    WallTime                           idle_expires{};
    WallTime                           absolute_expires{};
    std::map<std::string, std::string> attributes;

    std::string attribute(const std::string& key, const std::string& fallback = {}) const;
};

struct IssuedSession {
    std::string token;
    Session     session;
};

enum class RevokeReason {
    Logout,
    LogoutEverywhere,
    Rotated,
    Expired,
    Evicted,
    DeviceRevoked,
    Administrative
};

const char* to_string(RevokeReason reason) noexcept;

class SessionStore {
public:
    using RevokeCallback = std::function<void(const Session& session, RevokeReason reason)>;

    explicit SessionStore(SessionPolicy policy = SessionPolicy(), WallNowFn now = default_wall_now());

    IssuedSession create(const std::string& user, const std::string& device_id = {}, std::map<std::string, std::string> attributes = {});
    std::optional<Session> validate(const std::string& token, bool touch = true);
    std::optional<Session> peek(const std::string& token) const;
    bool needs_rotation(const Session& session) const;
    std::optional<IssuedSession> rotate(const std::string& token);

    bool revoke(const std::string& token, RevokeReason reason = RevokeReason::Logout);
    bool revoke_id(const std::string& session_id, RevokeReason reason = RevokeReason::Administrative);
    std::size_t revoke_user(const std::string& user, const std::string& except_session_id = {}, RevokeReason reason = RevokeReason::LogoutEverywhere);
    std::size_t revoke_device(const std::string& user, const std::string& device_id, RevokeReason reason = RevokeReason::DeviceRevoked);
    std::size_t sweep();

    bool set_attribute(const std::string& token, const std::string& key, const std::string& value);
    bool remove_attribute(const std::string& token, const std::string& key);
    bool verify_csrf(const std::string& token, const std::string& csrf_token) const;

    std::vector<Session> sessions(const std::string& user) const;
    std::size_t size() const;

    std::string set_cookie_header(const IssuedSession& issued) const;
    std::string clear_cookie_header() const;
    std::string token_from_cookie_header(const std::string& cookie_header) const;

    std::vector<std::pair<std::string, Session>> snapshot() const;
    void restore(const std::vector<std::pair<std::string, Session>>& hashed_sessions);

    void on_revoke(RevokeCallback cb);
    const SessionPolicy& policy() const noexcept { return m_policy; }

private:
    SessionPolicy                                             m_policy;
    WallNowFn                                                 m_now;
    mutable std::mutex                                        m_mutex;
    std::unordered_map<std::string, Session>                  m_by_hash;
    std::unordered_map<std::string, std::string>              m_hash_by_id;
    RevokeCallback                                            m_on_revoke;

    bool expired(const Session& s, WallTime now) const noexcept;
    void erase_locked(const std::string& hash, RevokeReason reason, std::vector<std::pair<Session, RevokeReason>>& removed);
    void notify(const std::vector<std::pair<Session, RevokeReason>>& removed);
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_SESSION_STORE_HPP
