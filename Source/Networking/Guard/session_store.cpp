#include "fizmo_library.hpp"
#include "session_store.hpp"
#include "../../Crypto/encoding.hpp"
#include "../../Crypto/secure_memory.hpp"

#include <algorithm>

namespace fizmo {
namespace networking {
namespace guard {

std::string Session::attribute(const std::string& key, const std::string& fallback) const {
    const auto it = attributes.find(key);
    return it == attributes.end() ? fallback : it->second;
}

const char* to_string(RevokeReason reason) noexcept {
    switch (reason) {
        case RevokeReason::Logout:           return "logout";
        case RevokeReason::LogoutEverywhere: return "logout_everywhere";
        case RevokeReason::Rotated:          return "rotated";
        case RevokeReason::Expired:          return "expired";
        case RevokeReason::Evicted:          return "evicted";
        case RevokeReason::DeviceRevoked:    return "device_revoked";
        default:                             return "administrative";
    }
}

SessionStore::SessionStore(SessionPolicy policy, WallNowFn now)
    : m_policy(std::move(policy)), m_now(now ? std::move(now) : default_wall_now()) {}

bool SessionStore::expired(const Session& s, WallTime now) const noexcept {
    return now >= s.idle_expires || now >= s.absolute_expires;
}

void SessionStore::erase_locked(const std::string& hash, RevokeReason reason, std::vector<std::pair<Session, RevokeReason>>& removed) {
    const auto it = m_by_hash.find(hash);
    if (it == m_by_hash.end()) return;
    m_hash_by_id.erase(it->second.id);
    removed.emplace_back(std::move(it->second), reason);
    m_by_hash.erase(it);
}

void SessionStore::notify(const std::vector<std::pair<Session, RevokeReason>>& removed) {
    RevokeCallback cb;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        cb = m_on_revoke;
    }

    if (!cb) return;

    for (const auto& r : removed) {
        cb(r.first, r.second);
    }
}

IssuedSession SessionStore::create(const std::string& user, const std::string& device_id, std::map<std::string, std::string> attributes) {
    const WallTime now = m_now();
    IssuedSession issued;
    issued.token = crypto::random_token_base64url(m_policy.token_bytes);
    Session& s = issued.session;
    s.id = crypto::uuid_v4();
    s.user = user;
    s.device_id = device_id;
    s.csrf_token = crypto::random_token_base64url(24);
    s.created = now;
    s.last_seen = now;
    s.rotated = now;
    s.idle_expires = now + m_policy.idle_timeout;
    s.absolute_expires = now + m_policy.absolute_timeout;
    s.attributes = std::move(attributes);

    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);

        if (m_policy.max_per_user) {
            std::vector<std::pair<WallTime, std::string>> mine;

            for (const auto& kv : m_by_hash) {
                if (kv.second.user == user) mine.emplace_back(kv.second.last_seen, kv.first);
            }

            if (mine.size() >= m_policy.max_per_user) {
                std::sort(mine.begin(), mine.end());
                const std::size_t excess = mine.size() - m_policy.max_per_user + 1;

                for (std::size_t i = 0; i < excess; ++i) {
                    erase_locked(mine[i].second, RevokeReason::Evicted, removed);
                }
            }
        }

        const std::string h = hash_secret(issued.token);
        m_hash_by_id[s.id] = h;
        m_by_hash[h] = s;
    }

    notify(removed);
    return issued;
}

std::optional<Session> SessionStore::validate(const std::string& token, bool touch) {
    if (token.empty()) return std::nullopt;
    const WallTime now = m_now();
    const std::string h = hash_secret(token);
    std::vector<std::pair<Session, RevokeReason>> removed;
    std::optional<Session> result;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_by_hash.find(h);

        if (it != m_by_hash.end()) {
            if (expired(it->second, now)) {
                erase_locked(h, RevokeReason::Expired, removed);
            } else {
                if (touch) {
                    it->second.last_seen = now;
                    it->second.idle_expires = std::min(now + m_policy.idle_timeout, it->second.absolute_expires);
                }

                result = it->second;
            }
        }
    }

    notify(removed);
    return result;
}

std::optional<Session> SessionStore::peek(const std::string& token) const {
    if (token.empty()) return std::nullopt;
    const WallTime now = m_now();
    const std::string h = hash_secret(token);
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_hash.find(h);
    if (it == m_by_hash.end() || expired(it->second, now)) return std::nullopt;
    return it->second;
}

bool SessionStore::needs_rotation(const Session& session) const {
    if (m_policy.rotate_every.count() <= 0) return false;
    return m_now() - session.rotated >= m_policy.rotate_every;
}

std::optional<IssuedSession> SessionStore::rotate(const std::string& token) {
    if (token.empty()) return std::nullopt;
    const WallTime now = m_now();
    const std::string old_hash = hash_secret(token);
    IssuedSession issued;
    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_by_hash.find(old_hash);
        if (it == m_by_hash.end()) return std::nullopt;

        if (expired(it->second, now)) {
            erase_locked(old_hash, RevokeReason::Expired, removed);
        } else {
            Session s = it->second;
            m_by_hash.erase(it);
            issued.token = crypto::random_token_base64url(m_policy.token_bytes);
            s.rotated = now;
            s.last_seen = now;
            s.idle_expires = std::min(now + m_policy.idle_timeout, s.absolute_expires);
            s.csrf_token = crypto::random_token_base64url(24);
            const std::string h = hash_secret(issued.token);
            m_hash_by_id[s.id] = h;
            m_by_hash[h] = s;
            issued.session = s;
        }
    }

    if (!removed.empty()) {
        notify(removed);
        return std::nullopt;
    }

    return issued;
}

bool SessionStore::revoke(const std::string& token, RevokeReason reason) {
    if (token.empty()) return false;
    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        erase_locked(hash_secret(token), reason, removed);
    }

    notify(removed);
    return !removed.empty();
}

bool SessionStore::revoke_id(const std::string& session_id, RevokeReason reason) {
    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_hash_by_id.find(session_id);
        if (it != m_hash_by_id.end()) erase_locked(std::string(it->second), reason, removed);
    }

    notify(removed);
    return !removed.empty();
}

std::size_t SessionStore::revoke_user(const std::string& user, const std::string& except_session_id, RevokeReason reason) {
    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::vector<std::string> hashes;

        for (const auto& kv : m_by_hash) {
            if (kv.second.user == user && kv.second.id != except_session_id) hashes.push_back(kv.first);
        }

        for (const auto& h : hashes) erase_locked(h, reason, removed);
    }

    notify(removed);
    return removed.size();
}

std::size_t SessionStore::revoke_device(const std::string& user, const std::string& device_id, RevokeReason reason) {
    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::vector<std::string> hashes;

        for (const auto& kv : m_by_hash) {
            if (kv.second.user == user && kv.second.device_id == device_id) hashes.push_back(kv.first);
        }

        for (const auto& h : hashes) erase_locked(h, reason, removed);
    }

    notify(removed);
    return removed.size();
}

std::size_t SessionStore::sweep() {
    const WallTime now = m_now();
    std::vector<std::pair<Session, RevokeReason>> removed;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::vector<std::string> hashes;

        for (const auto& kv : m_by_hash) {
            if (expired(kv.second, now)) hashes.push_back(kv.first);
        }

        for (const auto& h : hashes) erase_locked(h, RevokeReason::Expired, removed);
    }

    notify(removed);
    return removed.size();
}

bool SessionStore::set_attribute(const std::string& token, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_hash.find(hash_secret(token));
    if (it == m_by_hash.end()) return false;
    it->second.attributes[key] = value;
    return true;
}

bool SessionStore::remove_attribute(const std::string& token, const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_hash.find(hash_secret(token));
    if (it == m_by_hash.end()) return false;
    return it->second.attributes.erase(key) != 0;
}

bool SessionStore::verify_csrf(const std::string& token, const std::string& csrf_token) const {
    if (token.empty() || csrf_token.empty()) return false;
    const auto s = peek(token);
    return s && crypto::secure_equals(s->csrf_token, csrf_token);
}

std::vector<Session> SessionStore::sessions(const std::string& user) const {
    const WallTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<Session> out;

    for (const auto& kv : m_by_hash) {
        if (kv.second.user == user && !expired(kv.second, now)) out.push_back(kv.second);
    }

    std::sort(out.begin(), out.end(), [](const Session& a, const Session& b) { return a.last_seen > b.last_seen; });
    return out;
}

std::size_t SessionStore::size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_by_hash.size();
}

std::string SessionStore::set_cookie_header(const IssuedSession& issued) const {
    CookieOptions o = m_policy.cookie;
    if (!o.max_age) o.max_age = std::chrono::duration_cast<std::chrono::seconds>(issued.session.absolute_expires - m_now());
    return make_set_cookie(m_policy.cookie_name, issued.token, o);
}

std::string SessionStore::clear_cookie_header() const {
    return make_expired_cookie(m_policy.cookie_name, m_policy.cookie);
}

std::string SessionStore::token_from_cookie_header(const std::string& cookie_header) const {
    return cookie_value(cookie_header, m_policy.cookie_name);
}

std::vector<std::pair<std::string, Session>> SessionStore::snapshot() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return std::vector<std::pair<std::string, Session>>(m_by_hash.begin(), m_by_hash.end());
}

void SessionStore::restore(const std::vector<std::pair<std::string, Session>>& hashed_sessions) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_by_hash.clear();
    m_hash_by_id.clear();

    for (const auto& kv : hashed_sessions) {
        m_by_hash[kv.first] = kv.second;
        m_hash_by_id[kv.second.id] = kv.first;
    }
}

void SessionStore::on_revoke(RevokeCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_revoke = std::move(cb);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
