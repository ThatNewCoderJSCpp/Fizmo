#ifndef FIZMO_GUARD_DEVICE_REGISTRY_HPP
#define FIZMO_GUARD_DEVICE_REGISTRY_HPP

#include "guard_types.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct DeviceSignals {
    std::string ja4;
    std::string user_agent;
    std::string ip;
};

struct DeviceRecord {
    std::string id;
    std::string user;
    std::string label;
    std::string token_hash;
    std::string ja4;
    std::string user_agent;
    std::string family;
    std::string first_ip;
    std::string last_ip;
    WallTime    first_seen{};
    WallTime    last_seen{};
    bool        trusted = false;
};

enum class DeviceStatus {
    Known,
    Changed,
    New
};

const char* to_string(DeviceStatus status) noexcept;

struct DeviceCheck {
    DeviceStatus             status = DeviceStatus::New;
    std::string              device_id;
    std::string              issued_token;
    bool                     trusted = false;
    std::vector<std::string> changes;

    bool is_new() const noexcept { return status == DeviceStatus::New; }
    bool needs_verification() const noexcept { return !trusted; }
};

struct DevicePolicy {
    std::size_t          max_devices_per_user = 20;
    bool                 distrust_on_change   = false;
    std::chrono::hours   forget_after         = std::chrono::hours(24 * 180);
    std::size_t          token_bytes          = 32;
    std::string          cookie_name          = "__Host-device";
};

class DeviceRegistry {
public:
    using DeviceCallback = std::function<void(const DeviceRecord& device, const DeviceCheck& check)>;

    explicit DeviceRegistry(DevicePolicy policy = DevicePolicy(), WallNowFn now = default_wall_now());

    DeviceCheck identify(const std::string& user, const std::string& device_token, const DeviceSignals& signals);
    bool trust(const std::string& user, const std::string& device_id, const std::string& label = {});
    bool distrust(const std::string& user, const std::string& device_id);
    bool rename(const std::string& user, const std::string& device_id, const std::string& label);
    bool revoke(const std::string& user, const std::string& device_id);
    std::size_t revoke_all(const std::string& user, const std::string& except_device_id = {});
    std::size_t sweep();

    std::vector<DeviceRecord> devices(const std::string& user) const;
    std::optional<DeviceRecord> find(const std::string& user, const std::string& device_id) const;
    std::optional<DeviceRecord> find_by_token(const std::string& user, const std::string& device_token) const;

    std::vector<DeviceRecord> snapshot() const;
    void restore(const std::vector<DeviceRecord>& records);

    void on_new_device(DeviceCallback cb);
    void on_changed_device(DeviceCallback cb);
    const DevicePolicy& policy() const noexcept { return m_policy; }

private:
    DevicePolicy                                                   m_policy;
    WallNowFn                                                      m_now;
    mutable std::mutex                                             m_mutex;
    std::unordered_map<std::string, std::vector<DeviceRecord>>     m_by_user;
    DeviceCallback                                                 m_on_new;
    DeviceCallback                                                 m_on_changed;

    DeviceRecord* locate(const std::string& user, const std::string& device_id);
    const DeviceRecord* locate(const std::string& user, const std::string& device_id) const;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_DEVICE_REGISTRY_HPP
