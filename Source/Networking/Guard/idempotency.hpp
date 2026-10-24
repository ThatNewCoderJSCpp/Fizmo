#ifndef FIZMO_GUARD_IDEMPOTENCY_HPP
#define FIZMO_GUARD_IDEMPOTENCY_HPP

#include "guard_types.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct StoredResponse {
    int                                              status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string                                      body;
};

enum class IdempotencyState {
    Started,
    InProgress,
    Replay,
    Mismatch,
    Invalid
};

const char* to_string(IdempotencyState state) noexcept;

struct IdempotencyBegin {
    IdempotencyState              state = IdempotencyState::Started;
    std::optional<StoredResponse> response;

    bool should_run() const noexcept { return state == IdempotencyState::Started; }
    int conflict_status() const noexcept;
};

struct IdempotencyPolicy {
    std::chrono::seconds ttl          = std::chrono::hours(24);
    std::chrono::seconds lock_timeout = std::chrono::seconds(60);
    std::size_t          max_entries  = 100000;
    std::size_t          max_body     = 1u << 20;
    std::size_t          max_key_length = 255;
};

class IdempotencyStore {
public:
    explicit IdempotencyStore(IdempotencyPolicy policy = IdempotencyPolicy(), NowFn now = default_now());

    static std::string request_fingerprint(const std::string& method, const std::string& path, const std::string& body);
    bool valid_key(const std::string& key) const noexcept;

    IdempotencyBegin begin(const std::string& scope, const std::string& key, const std::string& fingerprint);
    bool complete(const std::string& scope, const std::string& key, StoredResponse response);
    void abandon(const std::string& scope, const std::string& key);
    std::optional<StoredResponse> lookup(const std::string& scope, const std::string& key) const;
    std::size_t sweep();
    std::size_t size() const;
    void clear();

    const IdempotencyPolicy& policy() const noexcept { return m_policy; }

private:
    struct Entry {
        std::string                   fingerprint;
        bool                          done = false;
        SteadyTime                    started{};
        SteadyTime                    expires{};
        std::optional<StoredResponse> response;
    };

    IdempotencyPolicy                      m_policy;
    NowFn                                  m_now;
    mutable std::mutex                     m_mutex;
    std::unordered_map<std::string, Entry> m_entries;

    static std::string compose(const std::string& scope, const std::string& key);
    bool stale(const Entry& e, SteadyTime now) const noexcept;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_IDEMPOTENCY_HPP
