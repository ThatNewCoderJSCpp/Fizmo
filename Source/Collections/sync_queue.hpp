#ifndef FIZMO_COLLECTIONS_SYNC_QUEUE_HPP
#define FIZMO_COLLECTIONS_SYNC_QUEUE_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "deque.hpp"
#include "priority_queue.hpp"

namespace fizmo {
namespace collections {
namespace detail {

template <class T>
struct FifoPolicy {
    Deque<T> c;
    void put(T&& v) { c.push_back(std::move(v)); }
    T take() { return c.take_front(); }
    std::size_t size() const noexcept { return c.size(); }
    void clear() noexcept { c.clear(); }
};

template <class T>
struct LifoPolicy {
    Deque<T> c;
    void put(T&& v) { c.push_back(std::move(v)); }
    T take() { return c.take_back(); }
    std::size_t size() const noexcept { return c.size(); }
    void clear() noexcept { c.clear(); }
};

template <class T, class Compare>
struct PriorityPolicy {
    PriorityQueue<T, Compare> c;
    void put(T&& v) { c.push(std::move(v)); }
    T take() { return c.pop(); }
    std::size_t size() const noexcept { return c.size(); }
    void clear() noexcept { c.clear(); }
};

} // namespace detail

template <class T, class Policy>
class BasicSyncQueue {
public:
    using value_type = T;
    using size_type  = std::size_t;

    explicit BasicSyncQueue(size_type maxsize = 0) : m_max(maxsize) {}

    BasicSyncQueue(const BasicSyncQueue&) = delete;
    BasicSyncQueue& operator=(const BasicSyncQueue&) = delete;

    void put(T v) {
        std::unique_lock<std::mutex> lk(m_m);
        m_not_full.wait(lk, [this] { return m_closed || m_max == 0 || m_p.size() < m_max; });
        if (m_closed) return;
        m_p.put(std::move(v));
        ++m_unfinished;
        m_not_empty.notify_one();
    }

    template <class Rep, class Period>
    bool put_for(T v, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lk(m_m);
        if (!m_not_full.wait_for(lk, timeout, [this] { return m_closed || m_max == 0 || m_p.size() < m_max; })) return false;
        if (m_closed) return false;
        m_p.put(std::move(v));
        ++m_unfinished;
        m_not_empty.notify_one();
        return true;
    }

    bool try_put(T v) {
        std::lock_guard<std::mutex> lk(m_m);
        if (m_closed || (m_max != 0 && m_p.size() >= m_max)) return false;
        m_p.put(std::move(v));
        ++m_unfinished;
        m_not_empty.notify_one();
        return true;
    }

    bool get(T& out) {
        std::unique_lock<std::mutex> lk(m_m);
        m_not_empty.wait(lk, [this] { return m_closed || m_p.size() > 0; });
        if (m_p.size() == 0) return false;
        out = m_p.take();
        m_not_full.notify_one();
        return true;
    }

    T get() {
        T v;
        if (!get(v)) throw std::runtime_error("SyncQueue closed");
        return v;
    }

    template <class Rep, class Period>
    bool get_for(T& out, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lk(m_m);
        if (!m_not_empty.wait_for(lk, timeout, [this] { return m_closed || m_p.size() > 0; })) return false;
        if (m_p.size() == 0) return false;
        out = m_p.take();
        m_not_full.notify_one();
        return true;
    }

    bool try_get(T& out) {
        std::lock_guard<std::mutex> lk(m_m);
        if (m_p.size() == 0) return false;
        out = m_p.take();
        m_not_full.notify_one();
        return true;
    }

    void task_done() {
        std::lock_guard<std::mutex> lk(m_m);
        if (m_unfinished == 0) throw std::logic_error("task_done called too many times");
        if (--m_unfinished == 0) m_all_done.notify_all();
    }

    void join() {
        std::unique_lock<std::mutex> lk(m_m);
        m_all_done.wait(lk, [this] { return m_unfinished == 0; });
    }

    void close() {
        std::lock_guard<std::mutex> lk(m_m);
        m_closed = true;
        m_not_empty.notify_all();
        m_not_full.notify_all();
    }

    bool closed() const {
        std::lock_guard<std::mutex> lk(m_m);
        return m_closed;
    }

    size_type size() const {
        std::lock_guard<std::mutex> lk(m_m);
        return m_p.size();
    }

    bool empty() const { return size() == 0; }

    bool full() const {
        std::lock_guard<std::mutex> lk(m_m);
        return m_max != 0 && m_p.size() >= m_max;
    }

    size_type maxsize() const noexcept { return m_max; }

    void clear() {
        std::lock_guard<std::mutex> lk(m_m);
        m_p.clear();
        m_not_full.notify_all();
    }

private:
    mutable std::mutex      m_m;
    std::condition_variable m_not_empty;
    std::condition_variable m_not_full;
    std::condition_variable m_all_done;
    Policy                  m_p;
    size_type               m_max = 0;
    size_type               m_unfinished = 0;
    bool                    m_closed = false;
};

template <class T>
using SyncQueue = BasicSyncQueue<T, detail::FifoPolicy<T>>;

template <class T>
using SyncLifoQueue = BasicSyncQueue<T, detail::LifoPolicy<T>>;

template <class T, class Compare = std::greater<T>>
using SyncPriorityQueue = BasicSyncQueue<T, detail::PriorityPolicy<T, Compare>>;

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_SYNC_QUEUE_HPP
