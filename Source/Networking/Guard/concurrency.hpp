#ifndef FIZMO_GUARD_CONCURRENCY_HPP
#define FIZMO_GUARD_CONCURRENCY_HPP

#include "guard_types.hpp"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

class KeyedMutex {
public:
    class Lock {
    public:
        Lock() = default;
        Lock(Lock&& other) noexcept : m_owner(other.m_owner), m_key(std::move(other.m_key)) { other.m_owner = nullptr; }
        Lock& operator=(Lock&& other) noexcept;
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        ~Lock() { unlock(); }

        void unlock();
        bool owns_lock() const noexcept { return m_owner != nullptr; }
        explicit operator bool() const noexcept { return owns_lock(); }
        const std::string& key() const noexcept { return m_key; }

    private:
        friend class KeyedMutex;
        Lock(KeyedMutex* owner, std::string key) : m_owner(owner), m_key(std::move(key)) {}

        KeyedMutex* m_owner = nullptr;
        std::string m_key;
    };

    KeyedMutex() = default;
    KeyedMutex(const KeyedMutex&) = delete;
    KeyedMutex& operator=(const KeyedMutex&) = delete;

    Lock lock(const std::string& key);
    Lock try_lock(const std::string& key);
    Lock try_lock_for(const std::string& key, std::chrono::milliseconds timeout);
    std::vector<Lock> lock_all(std::vector<std::string> keys);
    bool locked(const std::string& key) const;
    std::size_t size() const;

    template <typename Fn>
    auto with(const std::string& key, Fn&& fn) -> decltype(fn()) {
        Lock l = lock(key);
        return fn();
    }

private:
    struct Slot {
        bool        held = false;
        std::size_t waiters = 0;
    };

    mutable std::mutex                    m_mutex;
    std::condition_variable               m_cv;
    std::unordered_map<std::string, Slot> m_slots;

    void release(const std::string& key);
};

template <typename T>
class Versioned {
public:
    struct Snapshot {
        T             value;
        std::uint64_t version = 0;
    };

    Versioned() = default;
    explicit Versioned(T value, std::uint64_t version = 1) : m_value(std::move(value)), m_version(version) {}

    Snapshot get() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return Snapshot{ m_value, m_version };
    }

    std::uint64_t version() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_version;
    }

    std::optional<std::uint64_t> compare_and_set(std::uint64_t expected_version, T value) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_version != expected_version) return std::nullopt;
        m_value = std::move(value);
        return ++m_version;
    }

    template <typename Fn>
    std::optional<std::uint64_t> update_if(std::uint64_t expected_version, Fn&& fn) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_version != expected_version) return std::nullopt;
        fn(m_value);
        return ++m_version;
    }

    template <typename Fn>
    std::uint64_t update(Fn&& fn) {
        std::lock_guard<std::mutex> lk(m_mutex);
        fn(m_value);
        return ++m_version;
    }

private:
    mutable std::mutex m_mutex;
    T                  m_value{};
    std::uint64_t      m_version = 1;
};

template <typename Fn>
auto retry_on_conflict(Fn&& attempt, int max_attempts = 5) -> decltype(attempt()) {
    decltype(attempt()) result{};

    for (int i = 0; i < max_attempts; ++i) {
        result = attempt();
        if (result) return result;
    }

    return result;
}

template <typename V>
class SingleFlight {
public:
    std::shared_future<V> run(const std::string& key, std::function<V()> work, bool* shared = nullptr) {
        std::shared_ptr<std::promise<V>> promise;
        std::shared_future<V> future;

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            const auto it = m_calls.find(key);

            if (it != m_calls.end()) {
                if (shared) *shared = true;
                return it->second;
            }

            promise = std::make_shared<std::promise<V>>();
            future = promise->get_future().share();
            m_calls.emplace(key, future);
        }

        if (shared) *shared = false;

        try {
            promise->set_value(work());
        } catch (...) {
            promise->set_exception(std::current_exception());
        }

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_calls.erase(key);
        }

        return future;
    }

    V get(const std::string& key, std::function<V()> work) {
        return run(key, std::move(work)).get();
    }

    std::size_t in_flight() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_calls.size();
    }

private:
    mutable std::mutex                                     m_mutex;
    std::unordered_map<std::string, std::shared_future<V>> m_calls;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_CONCURRENCY_HPP
