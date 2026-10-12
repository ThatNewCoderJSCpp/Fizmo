#ifndef FIZMO_COLLECTIONS_PRIORITY_QUEUE_HPP
#define FIZMO_COLLECTIONS_PRIORITY_QUEUE_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

#include "hash_map.hpp"

namespace fizmo {
namespace collections {

template <class T, class Compare = std::less<T>>
class PriorityQueue {
public:
    using value_type = T;
    using size_type  = std::size_t;

    PriorityQueue() = default;
    explicit PriorityQueue(const Compare& c) : m_cmp(c) {}

    PriorityQueue(std::initializer_list<T> init, const Compare& c = Compare()) : m_heap(init), m_cmp(c) { std::make_heap(m_heap.begin(), m_heap.end(), m_cmp); }

    template <class It>
    PriorityQueue(It first, It last, const Compare& c = Compare()) : m_heap(first, last), m_cmp(c) { std::make_heap(m_heap.begin(), m_heap.end(), m_cmp); }

    size_type size() const noexcept { return m_heap.size(); }
    bool empty() const noexcept { return m_heap.empty(); }
    explicit operator bool() const noexcept { return !empty(); }

    const T& top() const {
        if (m_heap.empty()) throw std::out_of_range("PriorityQueue is empty");
        return m_heap.front();
    }

    const T& peek() const { return top(); }

    void push(const T& v) {
        m_heap.push_back(v);
        std::push_heap(m_heap.begin(), m_heap.end(), m_cmp);
    }

    void push(T&& v) {
        m_heap.push_back(std::move(v));
        std::push_heap(m_heap.begin(), m_heap.end(), m_cmp);
    }

    template <class... Args>
    void emplace(Args&&... args) {
        m_heap.emplace_back(std::forward<Args>(args)...);
        std::push_heap(m_heap.begin(), m_heap.end(), m_cmp);
    }

    void put(const T& v) { push(v); }
    void put(T&& v) { push(std::move(v)); }

    T pop() {
        if (m_heap.empty()) throw std::out_of_range("PriorityQueue is empty");
        std::pop_heap(m_heap.begin(), m_heap.end(), m_cmp);
        T v = std::move(m_heap.back());
        m_heap.pop_back();
        return v;
    }

    T get() { return pop(); }

    T push_pop(T v) {
        if (!m_heap.empty() && m_cmp(v, m_heap.front())) {
            std::swap(v, m_heap.front());
            sift_down(0);
        }

        return v;
    }

    T replace(T v) {
        if (m_heap.empty()) throw std::out_of_range("PriorityQueue is empty");
        std::swap(v, m_heap.front());
        sift_down(0);
        return v;
    }

    std::vector<T> drain_sorted() {
        std::vector<T> out;
        out.reserve(m_heap.size());
        while (!m_heap.empty()) out.push_back(pop());
        return out;
    }

    template <class Pred>
    size_type erase_if(Pred pred) {
        const auto it = std::remove_if(m_heap.begin(), m_heap.end(), pred);
        const size_type n = static_cast<size_type>(m_heap.end() - it);
        m_heap.erase(it, m_heap.end());
        if (n) std::make_heap(m_heap.begin(), m_heap.end(), m_cmp);
        return n;
    }

    void clear() noexcept { m_heap.clear(); }
    void reserve(size_type n) { m_heap.reserve(n); }

    const std::vector<T>& data() const noexcept { return m_heap; }
    auto begin() const noexcept { return m_heap.begin(); }
    auto end() const noexcept { return m_heap.end(); }

private:
    std::vector<T> m_heap;
    Compare        m_cmp;

    void sift_down(size_type i) {
        const size_type n = m_heap.size();

        for (;;) {
            size_type best = i;
            const size_type l = 2 * i + 1;
            const size_type r = l + 1;
            if (l < n && m_cmp(m_heap[best], m_heap[l])) best = l;
            if (r < n && m_cmp(m_heap[best], m_heap[r])) best = r;
            if (best == i) return;
            std::swap(m_heap[i], m_heap[best]);
            i = best;
        }
    }
};

template <class T>
using MaxPriorityQueue = PriorityQueue<T, std::less<T>>;

template <class T>
using MinPriorityQueue = PriorityQueue<T, std::greater<T>>;

template <class T>
using MaxHeap = MaxPriorityQueue<T>;

template <class T>
using MinHeap = MinPriorityQueue<T>;

template <class K, class P, class Compare = std::less<P>, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class IndexedPriorityQueue {
public:
    using key_type      = K;
    using priority_type = P;
    using value_type    = std::pair<K, P>;
    using size_type     = std::size_t;

    IndexedPriorityQueue() = default;
    explicit IndexedPriorityQueue(const Compare& c) : m_cmp(c) {}

    size_type size() const noexcept { return m_heap.size(); }
    bool empty() const noexcept { return m_heap.empty(); }
    explicit operator bool() const noexcept { return !empty(); }
    bool contains(const K& key) const { return m_pos.contains(key); }

    const value_type& top() const {
        if (m_heap.empty()) throw std::out_of_range("IndexedPriorityQueue is empty");
        return m_heap.front();
    }

    const P& priority(const K& key) const { return m_heap[m_pos.at(key)].second; }

    bool push(const K& key, const P& p) {
        if (m_pos.contains(key)) return false;
        m_heap.emplace_back(key, p);
        m_pos.insert({ key, m_heap.size() - 1 });
        sift_up(m_heap.size() - 1);
        return true;
    }

    void set(const K& key, const P& p) {
        size_type* i = m_pos.get_ptr(key);

        if (!i) {
            push(key, p);
            return;
        }

        const size_type idx = *i;
        m_heap[idx].second = p;
        sift_up(idx);
        sift_down(m_pos.at(key));
    }

    bool improve(const K& key, const P& p) {
        size_type* i = m_pos.get_ptr(key);

        if (!i) {
            push(key, p);
            return true;
        }

        if (!m_cmp(m_heap[*i].second, p)) return false;
        m_heap[*i].second = p;
        sift_up(*i);
        return true;
    }

    value_type pop() {
        if (m_heap.empty()) throw std::out_of_range("IndexedPriorityQueue is empty");
        value_type v = m_heap.front();
        remove_at(0);
        return v;
    }

    bool erase(const K& key) {
        size_type* i = m_pos.get_ptr(key);
        if (!i) return false;
        remove_at(*i);
        return true;
    }

    void clear() noexcept {
        m_heap.clear();
        m_pos.clear();
    }

private:
    std::vector<value_type>                    m_heap;
    HashMap<K, size_type, HashFn, KeyEq>       m_pos;
    Compare                                    m_cmp;

    void swap_nodes(size_type a, size_type b) {
        std::swap(m_heap[a], m_heap[b]);
        m_pos[m_heap[a].first] = a;
        m_pos[m_heap[b].first] = b;
    }

    void sift_up(size_type i) {
        while (i > 0) {
            const size_type p = (i - 1) / 2;
            if (!m_cmp(m_heap[p].second, m_heap[i].second)) return;
            swap_nodes(i, p);
            i = p;
        }
    }

    void sift_down(size_type i) {
        const size_type n = m_heap.size();

        for (;;) {
            size_type best = i;
            const size_type l = 2 * i + 1;
            const size_type r = l + 1;
            if (l < n && m_cmp(m_heap[best].second, m_heap[l].second)) best = l;
            if (r < n && m_cmp(m_heap[best].second, m_heap[r].second)) best = r;
            if (best == i) return;
            swap_nodes(i, best);
            i = best;
        }
    }

    void remove_at(size_type i) {
        const size_type last = m_heap.size() - 1;
        if (i != last) swap_nodes(i, last);
        m_pos.erase(m_heap.back().first);
        m_heap.pop_back();

        if (i < m_heap.size()) {
            sift_up(i);
            sift_down(i);
        }
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_PRIORITY_QUEUE_HPP
