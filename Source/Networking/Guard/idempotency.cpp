#include "fizmo_library.hpp"
#include "idempotency.hpp"
#include "../../Crypto/hash.hpp"
#include "../../Crypto/encoding.hpp"

namespace fizmo {
namespace networking {
namespace guard {

const char* to_string(IdempotencyState state) noexcept {
    switch (state) {
        case IdempotencyState::Started:    return "started";
        case IdempotencyState::InProgress: return "in_progress";
        case IdempotencyState::Replay:     return "replay";
        case IdempotencyState::Mismatch:   return "mismatch";
        default:                           return "invalid";
    }
}

int IdempotencyBegin::conflict_status() const noexcept {
    switch (state) {
        case IdempotencyState::InProgress: return 409;
        case IdempotencyState::Mismatch:   return 422;
        case IdempotencyState::Invalid:    return 400;
        case IdempotencyState::Replay:     return response ? response->status : 200;
        default:                           return 0;
    }
}

IdempotencyStore::IdempotencyStore(IdempotencyPolicy policy, NowFn now)
    : m_policy(policy), m_now(now ? std::move(now) : default_now()) {}

std::string IdempotencyStore::request_fingerprint(const std::string& method, const std::string& path, const std::string& body) {
    crypto::SHA256 h;
    h.update(method);
    const std::uint8_t sep = 0;
    h.update(&sep, 1);
    h.update(path);
    h.update(&sep, 1);
    h.update(body);
    const auto d = h.finish();
    return crypto::to_hex(d.data(), d.size());
}

bool IdempotencyStore::valid_key(const std::string& key) const noexcept {
    if (key.empty() || key.size() > m_policy.max_key_length) return false;

    for (unsigned char c : key) {
        if (c < 0x21 || c > 0x7e) return false;
    }

    return true;
}

std::string IdempotencyStore::compose(const std::string& scope, const std::string& key) {
    return std::to_string(scope.size()) + ":" + scope + "|" + key;
}

bool IdempotencyStore::stale(const Entry& e, SteadyTime now) const noexcept {
    if (e.done) return now >= e.expires;
    return now - e.started >= m_policy.lock_timeout;
}

IdempotencyBegin IdempotencyStore::begin(const std::string& scope, const std::string& key, const std::string& fingerprint) {
    IdempotencyBegin out;

    if (!valid_key(key)) {
        out.state = IdempotencyState::Invalid;
        return out;
    }

    const SteadyTime now = m_now();
    const std::string k = compose(scope, key);
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_entries.find(k);

    if (it != m_entries.end() && stale(it->second, now)) {
        m_entries.erase(it);
        it = m_entries.end();
    }

    if (it == m_entries.end()) {
        if (m_entries.size() >= m_policy.max_entries) {
            for (auto e = m_entries.begin(); e != m_entries.end();) {
                if (stale(e->second, now)) e = m_entries.erase(e);
                else ++e;
            }
        }

        if (m_entries.size() >= m_policy.max_entries) {
            out.state = IdempotencyState::InProgress;
            return out;
        }

        Entry e;
        e.fingerprint = fingerprint;
        e.started = now;
        m_entries.emplace(k, std::move(e));
        out.state = IdempotencyState::Started;
        return out;
    }

    if (it->second.fingerprint != fingerprint) {
        out.state = IdempotencyState::Mismatch;
        return out;
    }

    if (!it->second.done) {
        out.state = IdempotencyState::InProgress;
        return out;
    }

    out.state = IdempotencyState::Replay;
    out.response = it->second.response;
    return out;
}

bool IdempotencyStore::complete(const std::string& scope, const std::string& key, StoredResponse response) {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(compose(scope, key));
    if (it == m_entries.end()) return false;

    if (response.body.size() > m_policy.max_body) {
        m_entries.erase(it);
        return false;
    }

    it->second.done = true;
    it->second.expires = now + m_policy.ttl;
    it->second.response = std::move(response);
    return true;
}

void IdempotencyStore::abandon(const std::string& scope, const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(compose(scope, key));
    if (it != m_entries.end() && !it->second.done) m_entries.erase(it);
}

std::optional<StoredResponse> IdempotencyStore::lookup(const std::string& scope, const std::string& key) const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(compose(scope, key));
    if (it == m_entries.end() || !it->second.done || stale(it->second, now)) return std::nullopt;
    return it->second.response;
}

std::size_t IdempotencyStore::sweep() {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::size_t removed = 0;

    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (stale(it->second, now)) {
            it = m_entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

std::size_t IdempotencyStore::size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_entries.size();
}

void IdempotencyStore::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_entries.clear();
}

} // namespace guard
} // namespace networking
} // namespace fizmo
