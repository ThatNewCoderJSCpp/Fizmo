#ifndef FIZMO_GUARD_OWNERSHIP_HPP
#define FIZMO_GUARD_OWNERSHIP_HPP

#include "guard_types.hpp"

#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct Principal {
    std::string           user_id;
    std::set<std::string> roles;

    bool anonymous() const noexcept { return user_id.empty(); }
    bool has_role(const std::string& role) const { return roles.count(role) != 0; }
};

enum class Action {
    Read,
    Create,
    Update,
    Delete,
    Share
};

const char* to_string(Action action) noexcept;

enum class AccessResult {
    Allowed,
    NotFound,
    Unauthenticated
};

struct AccessDecision {
    AccessResult result = AccessResult::NotFound;
    std::string  reason;

    bool allowed() const noexcept { return result == AccessResult::Allowed; }
    explicit operator bool() const noexcept { return allowed(); }
    int http_status() const noexcept { return result == AccessResult::Allowed ? 200 : (result == AccessResult::Unauthenticated ? 401 : 404); }
};

struct ResourceInfo {
    std::string           owner;
    std::set<std::string> readers;
    std::set<std::string> writers;
    bool                  is_public = false;
};

class OwnershipGuard {
public:
    using Lookup = std::function<std::optional<ResourceInfo>(const std::string& id)>;
    using Rule   = std::function<std::optional<bool>(const Principal& who, Action action, const ResourceInfo& info)>;
    using DenyCallback = std::function<void(const Principal& who, const std::string& type, const std::string& id, Action action, const AccessDecision& decision)>;

    OwnershipGuard() = default;

    void register_type(const std::string& type, Lookup lookup);
    void add_rule(const std::string& type, Rule rule);
    void set_admin_role(const std::string& role);
    void on_deny(DenyCallback cb);

    AccessDecision check(const Principal& who, const std::string& type, const std::string& id, Action action) const;
    AccessDecision check(const Principal& who, const ResourceInfo& info, Action action) const;

    template <typename T, typename Owner>
    std::vector<T> filter(const Principal& who, const std::vector<T>& items, Owner owner_of) const {
        std::vector<T> out;

        for (const auto& item : items) {
            if (owner_of(item) == who.user_id || (!m_admin_role.empty() && who.has_role(m_admin_role))) out.push_back(item);
        }

        return out;
    }

private:
    struct Type {
        Lookup            lookup;
        std::vector<Rule> rules;
    };

    mutable std::mutex                    m_mutex;
    std::unordered_map<std::string, Type> m_types;
    std::string                           m_admin_role;
    DenyCallback                          m_on_deny;

    AccessDecision evaluate(const Principal& who, const ResourceInfo& info, Action action, const std::vector<Rule>& rules) const;
};

class IdSigner {
public:
    explicit IdSigner(std::string secret, std::size_t tag_bytes = 12);

    std::string encode(const std::string& type, const std::string& internal_id) const;
    std::optional<std::string> decode(const std::string& type, const std::string& public_id) const;

private:
    std::string m_secret;
    std::size_t m_tag_bytes;

    std::vector<std::uint8_t> tag(const std::string& type, const std::string& internal_id) const;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_OWNERSHIP_HPP
