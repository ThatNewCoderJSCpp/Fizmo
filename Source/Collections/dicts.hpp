#ifndef FIZMO_COLLECTIONS_DICTS_HPP
#define FIZMO_COLLECTIONS_DICTS_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <list>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "hash_map.hpp"

namespace fizmo {
namespace collections {

template <class K, class Count = std::size_t, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class Counter {
    using Map = HashMap<K, Count, HashFn, KeyEq>;

public:
    using key_type       = K;
    using count_type     = Count;
    using value_type     = std::pair<K, Count>;
    using size_type      = std::size_t;
    using const_iterator = typename Map::const_iterator;
    using iterator       = const_iterator;

    Counter() = default;

    Counter(std::initializer_list<K> init) { update(init); }
    Counter(std::initializer_list<value_type> init) { for (const auto& kv : init) add(kv.first, kv.second); }

    template <class It, typename std::enable_if<!std::is_integral<It>::value, int>::type = 0>
    Counter(It first, It last) { for (; first != last; ++first) add(*first); }

    size_type size() const noexcept { return m_map.size(); }
    bool empty() const noexcept { return m_map.empty(); }
    explicit operator bool() const noexcept { return !empty(); }

    const_iterator begin() const noexcept { return m_map.begin(); }
    const_iterator end() const noexcept { return m_map.end(); }

    Count count(const K& key) const { return m_map.get(key, Count(0)); }
    Count operator[](const K& key) const { return count(key); }
    Count& at(const K& key) { return m_map[key]; }
    bool contains(const K& key) const { return m_map.contains(key); }

    void add(const K& key, Count n = Count(1)) {
        if (n == Count(0)) return;
        m_map[key] += n;
    }

    void set(const K& key, Count n) {
        if (n == Count(0)) m_map.erase(key);
        else m_map.insert_or_assign(key, n);
    }

    void subtract(const K& key, Count n = Count(1)) {
        Count* c = m_map.get_ptr(key);

        if (std::is_unsigned<Count>::value) {
            if (!c) return;
            if (*c <= n) m_map.erase(key);
            else *c -= n;
            return;
        }

        if (!c) {
            m_map.insert_or_assign(key, Count(0) - n);
            return;
        }

        *c -= n;
    }

    template <class C>
    void update(const C& items) { for (const auto& k : items) add(k); }

    void update(const Counter& o) { for (const auto& kv : o) add(kv.first, kv.second); }

    template <class C>
    void subtract_all(const C& items) { for (const auto& k : items) subtract(k); }

    void subtract_all(const Counter& o) { for (const auto& kv : o) subtract(kv.first, kv.second); }

    size_type erase(const K& key) { return m_map.erase(key); }
    void clear() noexcept { m_map.clear(); }

    Count total() const {
        Count t = Count(0);
        for (const auto& kv : m_map) t += kv.second;
        return t;
    }

    std::vector<value_type> most_common(size_type n = static_cast<size_type>(-1)) const {
        std::vector<value_type> v(m_map.begin(), m_map.end());
        const auto by_count = [](const value_type& a, const value_type& b) { return a.second > b.second; };

        if (n < v.size()) {
            std::partial_sort(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n), v.end(), by_count);
            v.resize(n);
        } else {
            std::stable_sort(v.begin(), v.end(), by_count);
        }

        return v;
    }

    std::vector<value_type> least_common(size_type n = static_cast<size_type>(-1)) const {
        std::vector<value_type> v = most_common();
        std::reverse(v.begin(), v.end());
        if (n < v.size()) v.resize(n);
        return v;
    }

    std::vector<K> elements() const {
        std::vector<K> out;

        for (const auto& kv : m_map) {
            if (kv.second <= Count(0)) continue;
            for (Count i = Count(0); i < kv.second; ++i) out.push_back(kv.first);
        }

        return out;
    }

    std::vector<K> keys() const { return m_map.keys(); }

    Counter positive() const {
        Counter r;
        for (const auto& kv : m_map) if (kv.second > Count(0)) r.m_map.insert(kv);
        return r;
    }

    friend Counter operator+(const Counter& a, const Counter& b) {
        Counter r = a;
        r.update(b);
        return r.positive();
    }

    friend Counter operator-(const Counter& a, const Counter& b) {
        Counter r;

        for (const auto& kv : a) {
            const Count o = b.count(kv.first);
            if (kv.second > o) r.m_map.insert_or_assign(kv.first, kv.second - o);
        }

        return r;
    }

    friend Counter operator|(const Counter& a, const Counter& b) {
        Counter r = a.positive();
        for (const auto& kv : b) if (kv.second > r.count(kv.first)) r.m_map.insert_or_assign(kv.first, kv.second);
        return r;
    }

    friend Counter operator&(const Counter& a, const Counter& b) {
        Counter r;

        for (const auto& kv : a) {
            const Count m = std::min(kv.second, b.count(kv.first));
            if (m > Count(0)) r.m_map.insert_or_assign(kv.first, m);
        }

        return r;
    }

    friend bool operator==(const Counter& a, const Counter& b) {
        for (const auto& kv : a) if (b.count(kv.first) != kv.second) return false;
        for (const auto& kv : b) if (a.count(kv.first) != kv.second) return false;
        return true;
    }

    friend bool operator!=(const Counter& a, const Counter& b) { return !(a == b); }

    const Map& map() const noexcept { return m_map; }

private:
    Map m_map;
};

template <class K, class V, class Factory = std::function<V()>, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class DefaultDict : public HashMap<K, V, HashFn, KeyEq> {
    using Base = HashMap<K, V, HashFn, KeyEq>;

public:
    DefaultDict() : m_factory(default_factory()) {}
    explicit DefaultDict(Factory f) : m_factory(std::move(f)) {}
    DefaultDict(Factory f, std::initializer_list<std::pair<K, V>> init) : Base(init), m_factory(std::move(f)) {}

    V& operator[](const K& key) {
        V* v = this->get_ptr(key);
        if (v) return *v;
        return this->try_emplace(key, m_factory()).first->second;
    }

    const Factory& default_factory_fn() const noexcept { return m_factory; }
    void set_default_factory(Factory f) { m_factory = std::move(f); }

private:
    Factory m_factory;

    static Factory default_factory() {
        if constexpr (std::is_constructible<Factory, V (*)()>::value) {
            return Factory(+[]() { return V(); });
        } else {
            return Factory();
        }
    }
};

template <class K, class V, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class OrderedDict {
    using List = std::list<std::pair<K, V>>;
    using Index = HashMap<K, typename List::iterator, HashFn, KeyEq>;

public:
    using key_type       = K;
    using mapped_type    = V;
    using value_type     = std::pair<K, V>;
    using size_type      = std::size_t;
    using iterator       = typename List::iterator;
    using const_iterator = typename List::const_iterator;
    using reverse_iterator       = typename List::reverse_iterator;
    using const_reverse_iterator = typename List::const_reverse_iterator;

    OrderedDict() = default;
    OrderedDict(std::initializer_list<value_type> init) { for (const auto& kv : init) insert(kv); }

    template <class It>
    OrderedDict(It first, It last) { for (; first != last; ++first) insert(*first); }

    OrderedDict(const OrderedDict& o) : m_list(o.m_list) { reindex(); }
    OrderedDict(OrderedDict&& o) noexcept = default;

    OrderedDict& operator=(const OrderedDict& o) {
        if (this != &o) {
            m_list = o.m_list;
            reindex();
        }

        return *this;
    }

    OrderedDict& operator=(OrderedDict&& o) noexcept = default;

    size_type size() const noexcept { return m_list.size(); }
    bool empty() const noexcept { return m_list.empty(); }
    explicit operator bool() const noexcept { return !empty(); }

    iterator begin() noexcept { return m_list.begin(); }
    iterator end() noexcept { return m_list.end(); }
    const_iterator begin() const noexcept { return m_list.begin(); }
    const_iterator end() const noexcept { return m_list.end(); }
    reverse_iterator rbegin() noexcept { return m_list.rbegin(); }
    reverse_iterator rend() noexcept { return m_list.rend(); }
    const_reverse_iterator rbegin() const noexcept { return m_list.rbegin(); }
    const_reverse_iterator rend() const noexcept { return m_list.rend(); }

    V& operator[](const K& key) {
        auto* it = m_index.get_ptr(key);
        if (it) return (*it)->second;
        m_list.emplace_back(key, V());
        m_index.insert({ key, std::prev(m_list.end()) });
        return m_list.back().second;
    }

    V& at(const K& key) {
        auto* it = m_index.get_ptr(key);
        if (!it) throw std::out_of_range("OrderedDict::at: key not found");
        return (*it)->second;
    }

    const V& at(const K& key) const {
        auto* it = m_index.get_ptr(key);
        if (!it) throw std::out_of_range("OrderedDict::at: key not found");
        return (*it)->second;
    }

    V get(const K& key, const V& fallback = V()) const {
        auto* it = m_index.get_ptr(key);
        return it ? (*it)->second : fallback;
    }

    bool contains(const K& key) const { return m_index.contains(key); }
    size_type count(const K& key) const { return contains(key) ? 1 : 0; }

    iterator find(const K& key) {
        auto* it = m_index.get_ptr(key);
        return it ? *it : m_list.end();
    }

    const_iterator find(const K& key) const {
        auto* it = m_index.get_ptr(key);
        return it ? const_iterator(*it) : m_list.end();
    }

    std::pair<iterator, bool> insert(const value_type& kv) {
        auto* it = m_index.get_ptr(kv.first);
        if (it) return { *it, false };
        m_list.push_back(kv);
        m_index.insert({ kv.first, std::prev(m_list.end()) });
        return { std::prev(m_list.end()), true };
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& value) {
        auto* it = m_index.get_ptr(key);

        if (it) {
            (*it)->second = std::forward<M>(value);
            return { *it, false };
        }

        return insert(value_type(key, std::forward<M>(value)));
    }

    V& setdefault(const K& key, const V& value = V()) { return insert(value_type(key, value)).first->second; }

    size_type erase(const K& key) {
        auto* it = m_index.get_ptr(key);
        if (!it) return 0;
        m_list.erase(*it);
        m_index.erase(key);
        return 1;
    }

    iterator erase(const_iterator pos) {
        m_index.erase(pos->first);
        return m_list.erase(pos);
    }

    V pop(const K& key) {
        auto* it = m_index.get_ptr(key);
        if (!it) throw std::out_of_range("OrderedDict::pop: key not found");
        V v = std::move((*it)->second);
        m_list.erase(*it);
        m_index.erase(key);
        return v;
    }

    value_type popitem(bool last = true) {
        if (empty()) throw std::out_of_range("OrderedDict::popitem: dictionary is empty");
        auto it = last ? std::prev(m_list.end()) : m_list.begin();
        value_type v = std::move(*it);
        m_index.erase(v.first);
        m_list.erase(it);
        return v;
    }

    void move_to_end(const K& key, bool last = true) {
        auto* it = m_index.get_ptr(key);
        if (!it) throw std::out_of_range("OrderedDict::move_to_end: key not found");
        m_list.splice(last ? m_list.end() : m_list.begin(), m_list, *it);
    }

    value_type& front() { return checked().front(); }
    value_type& back() { return checked().back(); }

    template <class Other>
    void update(const Other& other) { for (const auto& kv : other) insert_or_assign(kv.first, kv.second); }

    std::vector<K> keys() const {
        std::vector<K> out;
        out.reserve(size());
        for (const auto& kv : m_list) out.push_back(kv.first);
        return out;
    }

    std::vector<V> values() const {
        std::vector<V> out;
        out.reserve(size());
        for (const auto& kv : m_list) out.push_back(kv.second);
        return out;
    }

    void clear() noexcept {
        m_list.clear();
        m_index.clear();
    }

    friend bool operator==(const OrderedDict& a, const OrderedDict& b) { return a.m_list == b.m_list; }
    friend bool operator!=(const OrderedDict& a, const OrderedDict& b) { return !(a == b); }

private:
    List  m_list;
    Index m_index;

    List& checked() {
        if (m_list.empty()) throw std::out_of_range("OrderedDict is empty");
        return m_list;
    }

    void reindex() {
        m_index.clear();
        m_index.reserve(m_list.size());
        for (auto it = m_list.begin(); it != m_list.end(); ++it) m_index.insert({ it->first, it });
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_DICTS_HPP
