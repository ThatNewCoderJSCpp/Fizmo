#ifndef FIZMO_TLS_SESSION_CACHE_HPP
#define FIZMO_TLS_SESSION_CACHE_HPP

#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

class TLSSessionCache {
public:
    using clock = std::chrono::system_clock;

    explicit TLSSessionCache(std::size_t max_entries = 1024, std::chrono::seconds ttl = std::chrono::hours(2));

    static std::string make_key(const std::string& host, std::uint16_t port);

    void store(const std::string& key, std::vector<std::uint8_t> session);
    bool lookup(const std::string& key, std::vector<std::uint8_t>& session);
    void remove(const std::string& key);
    void clear();
    std::size_t prune();

    void set_capacity(std::size_t max_entries);
    void set_ttl(std::chrono::seconds ttl);

    std::size_t size() const;
    std::size_t capacity() const;
    std::chrono::seconds ttl() const;

    std::uint64_t hits() const noexcept;
    std::uint64_t misses() const noexcept;
    double hit_rate() const noexcept;

    std::vector<std::uint8_t> serialize() const;
    bool deserialize(const std::vector<std::uint8_t>& data);
    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    struct Entry {
        std::string               key;
        std::vector<std::uint8_t> session;
        clock::time_point         expires;
    };

    using List = std::list<Entry>;

    mutable std::mutex                              m_mutex;
    List                                            m_lru;
    std::unordered_map<std::string, List::iterator> m_index;
    std::size_t                                     m_capacity;
    std::chrono::seconds                            m_ttl;
    std::uint64_t                                   m_hits = 0;
    std::uint64_t                                   m_misses = 0;

    void evict_locked();
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_SESSION_CACHE_HPP
