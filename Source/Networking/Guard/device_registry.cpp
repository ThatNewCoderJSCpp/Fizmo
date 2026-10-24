#include "fizmo_library.hpp"
#include "device_registry.hpp"
#include "tls_fingerprint.hpp"
#include "../../Crypto/encoding.hpp"
#include "../../Crypto/secure_memory.hpp"

#include <algorithm>

namespace fizmo {
namespace networking {
namespace guard {

const char* to_string(DeviceStatus status) noexcept {
    switch (status) {
        case DeviceStatus::Known:   return "known";
        case DeviceStatus::Changed: return "changed";
        default:                    return "new";
    }
}

DeviceRegistry::DeviceRegistry(DevicePolicy policy, WallNowFn now)
    : m_policy(std::move(policy)), m_now(now ? std::move(now) : default_wall_now()) {}

DeviceRecord* DeviceRegistry::locate(const std::string& user, const std::string& device_id) {
    const auto it = m_by_user.find(user);
    if (it == m_by_user.end()) return nullptr;

    for (auto& d : it->second) {
        if (d.id == device_id) return &d;
    }

    return nullptr;
}

const DeviceRecord* DeviceRegistry::locate(const std::string& user, const std::string& device_id) const {
    return const_cast<DeviceRegistry*>(this)->locate(user, device_id);
}

DeviceCheck DeviceRegistry::identify(const std::string& user, const std::string& device_token, const DeviceSignals& signals) {
    const WallTime now = m_now();
    const std::string family = to_string(classify_user_agent(signals.user_agent));
    DeviceCheck check;
    DeviceRecord copy;
    DeviceCallback cb;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto& list = m_by_user[user];
        DeviceRecord* found = nullptr;

        if (!device_token.empty()) {
            const std::string h = hash_secret(device_token);

            for (auto& d : list) {
                if (crypto::secure_equals(d.token_hash, h)) { found = &d; break; }
            }
        }

        if (found) {
            check.device_id = found->id;

            if (!signals.ja4.empty() && !found->ja4.empty() && signals.ja4 != found->ja4) check.changes.push_back("tls fingerprint");
            if (family != found->family) check.changes.push_back("client family");
            if (!signals.ip.empty() && signals.ip != found->last_ip) check.changes.push_back("ip address");

            const bool material = std::any_of(check.changes.begin(), check.changes.end(), [](const std::string& c) { return c != "ip address"; });
            check.status = material ? DeviceStatus::Changed : DeviceStatus::Known;
            if (material && m_policy.distrust_on_change) found->trusted = false;

            found->last_seen = now;
            if (!signals.ip.empty()) found->last_ip = signals.ip;
            if (!signals.ja4.empty()) found->ja4 = signals.ja4;
            if (!signals.user_agent.empty()) found->user_agent = signals.user_agent;
            found->family = family;
            check.trusted = found->trusted;

            if (material) {
                copy = *found;
                cb = m_on_changed;
            }
        } else {
            if (m_policy.max_devices_per_user && list.size() >= m_policy.max_devices_per_user) {
                auto victim = list.end();

                for (auto it = list.begin(); it != list.end(); ++it) {
                    if (it->trusted) continue;
                    if (victim == list.end() || it->last_seen < victim->last_seen) victim = it;
                }

                if (victim == list.end()) {
                    victim = std::min_element(list.begin(), list.end(), [](const DeviceRecord& a, const DeviceRecord& b) { return a.last_seen < b.last_seen; });
                }

                if (victim != list.end()) list.erase(victim);
            }

            DeviceRecord d;
            d.id = crypto::uuid_v4();
            d.user = user;
            check.issued_token = crypto::random_token_base64url(m_policy.token_bytes);
            d.token_hash = hash_secret(check.issued_token);
            d.ja4 = signals.ja4;
            d.user_agent = signals.user_agent;
            d.family = family;
            d.first_ip = signals.ip;
            d.last_ip = signals.ip;
            d.first_seen = now;
            d.last_seen = now;
            list.push_back(d);

            check.status = DeviceStatus::New;
            check.device_id = d.id;
            check.trusted = false;
            copy = d;
            cb = m_on_new;
        }
    }

    if (cb) cb(copy, check);
    return check;
}

bool DeviceRegistry::trust(const std::string& user, const std::string& device_id, const std::string& label) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DeviceRecord* d = locate(user, device_id);
    if (!d) return false;
    d->trusted = true;
    if (!label.empty()) d->label = label;
    return true;
}

bool DeviceRegistry::distrust(const std::string& user, const std::string& device_id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DeviceRecord* d = locate(user, device_id);
    if (!d) return false;
    d->trusted = false;
    return true;
}

bool DeviceRegistry::rename(const std::string& user, const std::string& device_id, const std::string& label) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DeviceRecord* d = locate(user, device_id);
    if (!d) return false;
    d->label = label;
    return true;
}

bool DeviceRegistry::revoke(const std::string& user, const std::string& device_id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_user.find(user);
    if (it == m_by_user.end()) return false;
    auto& list = it->second;
    const auto before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const DeviceRecord& d) { return d.id == device_id; }), list.end());
    return list.size() != before;
}

std::size_t DeviceRegistry::revoke_all(const std::string& user, const std::string& except_device_id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_user.find(user);
    if (it == m_by_user.end()) return 0;
    auto& list = it->second;
    const auto before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const DeviceRecord& d) { return d.id != except_device_id; }), list.end());
    return before - list.size();
}

std::size_t DeviceRegistry::sweep() {
    const WallTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::size_t removed = 0;

    for (auto it = m_by_user.begin(); it != m_by_user.end();) {
        auto& list = it->second;
        const auto before = list.size();
        list.erase(std::remove_if(list.begin(), list.end(), [&](const DeviceRecord& d) { return now - d.last_seen > m_policy.forget_after; }), list.end());
        removed += before - list.size();
        if (list.empty()) it = m_by_user.erase(it);
        else ++it;
    }

    return removed;
}

std::vector<DeviceRecord> DeviceRegistry::devices(const std::string& user) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_user.find(user);
    return it == m_by_user.end() ? std::vector<DeviceRecord>() : it->second;
}

std::optional<DeviceRecord> DeviceRegistry::find(const std::string& user, const std::string& device_id) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    const DeviceRecord* d = locate(user, device_id);
    if (!d) return std::nullopt;
    return *d;
}

std::optional<DeviceRecord> DeviceRegistry::find_by_token(const std::string& user, const std::string& device_token) const {
    if (device_token.empty()) return std::nullopt;
    const std::string h = hash_secret(device_token);
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_by_user.find(user);
    if (it == m_by_user.end()) return std::nullopt;

    for (const auto& d : it->second) {
        if (crypto::secure_equals(d.token_hash, h)) return d;
    }

    return std::nullopt;
}

std::vector<DeviceRecord> DeviceRegistry::snapshot() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<DeviceRecord> out;

    for (const auto& kv : m_by_user) {
        out.insert(out.end(), kv.second.begin(), kv.second.end());
    }

    return out;
}

void DeviceRegistry::restore(const std::vector<DeviceRecord>& records) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_by_user.clear();

    for (const auto& r : records) {
        m_by_user[r.user].push_back(r);
    }
}

void DeviceRegistry::on_new_device(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_new = std::move(cb);
}

void DeviceRegistry::on_changed_device(DeviceCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_changed = std::move(cb);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
