#include "fizmo_library.hpp"
#include "ownership.hpp"
#include "../../Crypto/hmac.hpp"
#include "../../Crypto/encoding.hpp"
#include "../../Crypto/secure_memory.hpp"

#include <stdexcept>

namespace fizmo {
namespace networking {
namespace guard {

const char* to_string(Action action) noexcept {
    switch (action) {
        case Action::Read:   return "read";
        case Action::Create: return "create";
        case Action::Update: return "update";
        case Action::Delete: return "delete";
        default:             return "share";
    }
}

void OwnershipGuard::register_type(const std::string& type, Lookup lookup) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_types[type].lookup = std::move(lookup);
}

void OwnershipGuard::add_rule(const std::string& type, Rule rule) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_types[type].rules.push_back(std::move(rule));
}

void OwnershipGuard::set_admin_role(const std::string& role) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_admin_role = role;
}

void OwnershipGuard::on_deny(DenyCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_deny = std::move(cb);
}

AccessDecision OwnershipGuard::evaluate(const Principal& who, const ResourceInfo& info, Action action, const std::vector<Rule>& rules) const {
    AccessDecision d;

    for (const auto& rule : rules) {
        const auto verdict = rule(who, action, info);

        if (verdict) {
            d.result = *verdict ? AccessResult::Allowed : AccessResult::NotFound;
            d.reason = *verdict ? "allowed by rule" : "denied by rule";
            return d;
        }
    }

    if (!m_admin_role.empty() && who.has_role(m_admin_role)) {
        d.result = AccessResult::Allowed;
        d.reason = "admin";
        return d;
    }

    if (action == Action::Read && info.is_public) {
        d.result = AccessResult::Allowed;
        d.reason = "public";
        return d;
    }

    if (who.anonymous()) {
        d.result = AccessResult::Unauthenticated;
        d.reason = "not signed in";
        return d;
    }

    if (!info.owner.empty() && info.owner == who.user_id) {
        d.result = AccessResult::Allowed;
        d.reason = "owner";
        return d;
    }

    if ((action == Action::Update) && info.writers.count(who.user_id)) {
        d.result = AccessResult::Allowed;
        d.reason = "writer";
        return d;
    }

    if (action == Action::Read && (info.readers.count(who.user_id) || info.writers.count(who.user_id))) {
        d.result = AccessResult::Allowed;
        d.reason = "reader";
        return d;
    }

    d.result = AccessResult::NotFound;
    d.reason = "not the owner";
    return d;
}

AccessDecision OwnershipGuard::check(const Principal& who, const ResourceInfo& info, Action action) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return evaluate(who, info, action, {});
}

AccessDecision OwnershipGuard::check(const Principal& who, const std::string& type, const std::string& id, Action action) const {
    Lookup lookup;
    std::vector<Rule> rules;
    DenyCallback deny;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_types.find(type);

        if (it == m_types.end() || !it->second.lookup) {
            AccessDecision d;
            d.result = AccessResult::NotFound;
            d.reason = "unregistered resource type";
            return d;
        }

        lookup = it->second.lookup;
        rules = it->second.rules;
        deny = m_on_deny;
    }

    AccessDecision d;
    const auto info = lookup(id);

    if (!info) {
        d.result = who.anonymous() ? AccessResult::Unauthenticated : AccessResult::NotFound;
        d.reason = "missing";
    } else {
        std::lock_guard<std::mutex> lk(m_mutex);
        d = evaluate(who, *info, action, rules);
    }

    if (!d.allowed() && deny) deny(who, type, id, action, d);
    return d;
}

IdSigner::IdSigner(std::string secret, std::size_t tag_bytes) : m_secret(std::move(secret)), m_tag_bytes(tag_bytes) {
    if (m_secret.size() < 16) throw std::invalid_argument("IdSigner secret must be at least 16 bytes");
    if (m_tag_bytes < 8 || m_tag_bytes > 32) throw std::invalid_argument("IdSigner tag must be 8 to 32 bytes");
}

std::vector<std::uint8_t> IdSigner::tag(const std::string& type, const std::string& internal_id) const {
    crypto::HMAC<crypto::SHA256> mac(m_secret);
    mac.update(type);
    const std::uint8_t sep = 0;
    mac.update(&sep, 1);
    mac.update(internal_id);
    const auto d = mac.finish();
    return std::vector<std::uint8_t>(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(m_tag_bytes));
}

std::string IdSigner::encode(const std::string& type, const std::string& internal_id) const {
    std::vector<std::uint8_t> raw(internal_id.begin(), internal_id.end());
    const auto t = tag(type, internal_id);
    raw.insert(raw.end(), t.begin(), t.end());
    return crypto::base64url_encode(raw);
}

std::optional<std::string> IdSigner::decode(const std::string& type, const std::string& public_id) const {
    std::vector<std::uint8_t> raw;
    if (!crypto::base64url_decode(public_id, raw) || raw.size() < m_tag_bytes) return std::nullopt;
    const std::string id(raw.begin(), raw.end() - static_cast<std::ptrdiff_t>(m_tag_bytes));
    const std::vector<std::uint8_t> given(raw.end() - static_cast<std::ptrdiff_t>(m_tag_bytes), raw.end());
    if (!crypto::secure_equals(given, tag(type, id))) return std::nullopt;
    return id;
}

} // namespace guard
} // namespace networking
} // namespace fizmo
