#ifndef FIZMO_DATABASE_CACHE_HPP
#define FIZMO_DATABASE_CACHE_HPP

#include "../Collections/hash.hpp"
#include "../Collections/hash_map.hpp"

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <utility>

namespace fizmo {
namespace database {

struct CacheStats {
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t evictions = 0;
    std::size_t entries = 0;
    std::size_t weight = 0;
};

template <typename K, typename V>
class LruCache {
public:
    explicit LruCache(std::size_t max_entries = 256, std::size_t max_weight = 0) : m_max_entries(max_entries), m_max_weight(max_weight) {}

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto* it = m_index.get_ptr(key);

        if (!it) {
            ++m_stats.misses;
            return std::nullopt;
        }

        m_order.splice(m_order.begin(), m_order, *it);
        ++m_stats.hits;
        return (*it)->value;
    }

    void put(const K& key, V value, std::size_t weight = 1) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_max_entries == 0) return;
        if (m_max_weight && weight > m_max_weight) return;
        auto* it = m_index.get_ptr(key);

        if (it) {
            m_weight -= (*it)->weight;
            (*it)->value = std::move(value);
            (*it)->weight = weight;
            m_weight += weight;
            m_order.splice(m_order.begin(), m_order, *it);
        } else {
            m_order.push_front(Node{ key, std::move(value), weight });
            m_index.insert_or_assign(key, m_order.begin());
            m_weight += weight;
        }

        while (!m_order.empty() && (m_order.size() > m_max_entries || (m_max_weight && m_weight > m_max_weight))) {
            Node& last = m_order.back();
            m_weight -= last.weight;
            m_index.erase(last.key);
            m_order.pop_back();
            ++m_stats.evictions;
        }
    }

    bool erase(const K& key) {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto* it = m_index.get_ptr(key);
        if (!it) return false;
        m_weight -= (*it)->weight;
        m_order.erase(*it);
        m_index.erase(key);
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_order.clear();
        m_index.clear();
        m_weight = 0;
    }

    void resize(std::size_t max_entries, std::size_t max_weight) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_max_entries = max_entries;
            m_max_weight = max_weight;
        }

        if (max_entries == 0) clear();
    }

    CacheStats stats() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        CacheStats s = m_stats;
        s.entries = m_order.size();
        s.weight = m_weight;
        return s;
    }

private:
    struct Node {
        K           key;
        V           value;
        std::size_t weight = 1;
    };

    using List = std::list<Node>;

    std::size_t                                        m_max_entries;
    std::size_t                                        m_max_weight;
    std::size_t                                        m_weight = 0;
    mutable std::mutex                                 m_mutex;
    List                                               m_order;
    collections::HashMap<K, typename List::iterator>   m_index;
    CacheStats                                         m_stats;
};

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_CACHE_HPP
