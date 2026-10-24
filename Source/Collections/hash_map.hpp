#ifndef FIZMO_COLLECTIONS_HASH_MAP_HPP
#define FIZMO_COLLECTIONS_HASH_MAP_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "hash.hpp"
#include "iterator.hpp"

namespace fizmo {
namespace collections {
namespace detail {

template <class V>
struct key_of {
    static const V& get(const V& v) noexcept { return v; }
};

template <class K, class M>
struct key_of<std::pair<K, M>> {
    static const K& get(const std::pair<K, M>& v) noexcept { return v.first; }
};

template <class Key, class Value, class HashFn, class KeyEq>
class DenseTable {
public:
    using key_type   = Key;
    using value_type = Value;
    using size_type  = std::size_t;

    static constexpr std::uint32_t kDistInc = 1u << 8;
    static constexpr std::uint32_t kFpMask = kDistInc - 1;
    static constexpr size_type npos = static_cast<size_type>(-1);

    struct Bucket {
        std::uint32_t dist_fp = 0;
        std::uint32_t index = 0;
    };

    DenseTable() = default;
    explicit DenseTable(const HashFn& h, const KeyEq& e = KeyEq()) : m_hash(h), m_eq(e) {}

    DenseTable(const DenseTable&) = default;
    DenseTable& operator=(const DenseTable&) = default;

    DenseTable(DenseTable&& o) noexcept : m_values(std::move(o.m_values)), m_buckets(std::move(o.m_buckets)), m_shift(o.m_shift), m_hash(std::move(o.m_hash)), m_eq(std::move(o.m_eq)), m_max_load(o.m_max_load) {
        o.m_values.clear();
        o.m_buckets.clear();
        o.m_shift = 64;
    }

    DenseTable& operator=(DenseTable&& o) noexcept {
        if (this != &o) {
            m_values = std::move(o.m_values);
            m_buckets = std::move(o.m_buckets);
            m_shift = o.m_shift;
            m_hash = std::move(o.m_hash);
            m_eq = std::move(o.m_eq);
            m_max_load = o.m_max_load;
            o.m_values.clear();
            o.m_buckets.clear();
            o.m_shift = 64;
        }

        return *this;
    }

    std::vector<Value>&       values() noexcept { return m_values; }
    const std::vector<Value>& values() const noexcept { return m_values; }

    size_type size() const noexcept { return m_values.size(); }
    size_type bucket_count() const noexcept { return m_buckets.size(); }
    float load_factor() const noexcept { return m_buckets.empty() ? 0.0f : static_cast<float>(m_values.size()) / static_cast<float>(m_buckets.size()); }
    float max_load_factor() const noexcept { return m_max_load; }

    void max_load_factor(float f) {
        m_max_load = std::min(0.95f, std::max(0.1f, f));
        reserve(size());
    }

    const HashFn& hash_function() const noexcept { return m_hash; }
    const KeyEq& key_eq() const noexcept { return m_eq; }

    template <class K>
    size_type find_index(const K& key) const {
        if (m_values.empty()) return npos;
        const std::uint64_t h = hash_of(key);
        std::uint32_t dist_fp = kDistInc | static_cast<std::uint32_t>(h & kFpMask);
        size_type b = bucket_of(h);

        for (;;) {
            const Bucket& bk = m_buckets[b];
            if (bk.dist_fp == dist_fp && m_eq(key_of<Value>::get(m_values[bk.index]), key)) return bk.index;
            if (bk.dist_fp < dist_fp) return npos;
            dist_fp += kDistInc;
            b = next(b);
        }
    }

    template <class K, class Make>
    std::pair<size_type, bool> find_or_insert(const K& key, Make&& make) {
        const size_type found = find_index(key);
        if (found != npos) return { found, false };
        grow_buckets(m_values.size() + 1);
        const size_type idx = m_values.size();
        m_values.emplace_back(make());

        try {
            place(hash_of(key_of<Value>::get(m_values.back())), static_cast<std::uint32_t>(idx));
        } catch (...) {
            m_values.pop_back();
            throw;
        }

        return { idx, true };
    }

    std::pair<size_type, bool> insert_value(Value&& v) {
        const size_type found = find_index(key_of<Value>::get(v));
        if (found != npos) return { found, false };
        grow_buckets(m_values.size() + 1);
        const size_type idx = m_values.size();
        m_values.push_back(std::move(v));
        place(hash_of(key_of<Value>::get(m_values.back())), static_cast<std::uint32_t>(idx));
        return { idx, true };
    }

    size_type erase_index(size_type idx) {
        const std::uint64_t h = hash_of(key_of<Value>::get(m_values[idx]));
        size_type b = locate(h, idx);
        size_type nb = next(b);

        while (m_buckets[nb].dist_fp >= 2 * kDistInc) {
            m_buckets[b].dist_fp = m_buckets[nb].dist_fp - kDistInc;
            m_buckets[b].index = m_buckets[nb].index;
            b = nb;
            nb = next(nb);
        }

        m_buckets[b] = Bucket();
        const size_type last = m_values.size() - 1;

        if (idx != last) {
            const std::uint64_t lh = hash_of(key_of<Value>::get(m_values[last]));
            m_buckets[locate(lh, last)].index = static_cast<std::uint32_t>(idx);
            m_values[idx] = std::move(m_values[last]);
        }

        m_values.pop_back();
        return idx;
    }

    template <class K>
    bool erase_key(const K& key) {
        const size_type i = find_index(key);
        if (i == npos) return false;
        erase_index(i);
        return true;
    }

    void clear() noexcept {
        m_values.clear();
        std::fill(m_buckets.begin(), m_buckets.end(), Bucket());
    }

    void reserve(size_type n) {
        grow_buckets(n);
        m_values.reserve(n);
    }

    void grow_buckets(size_type n) {
        size_type need = m_buckets.empty() ? 8 : m_buckets.size();
        while (static_cast<float>(n) > static_cast<float>(need) * m_max_load) need <<= 1;
        if (need != m_buckets.size()) rebuild(need);
    }

    void rehash(size_type count) {
        size_type need = 8;
        while (need < count || static_cast<float>(m_values.size()) > static_cast<float>(need) * m_max_load) need <<= 1;
        rebuild(need);
    }

    void shrink_to_fit() {
        m_values.shrink_to_fit();
        size_type need = 8;
        while (static_cast<float>(m_values.size()) > static_cast<float>(need) * m_max_load) need <<= 1;
        if (need < m_buckets.size()) rebuild(need);
    }

    void swap(DenseTable& o) noexcept {
        using std::swap;
        swap(m_values, o.m_values);
        swap(m_buckets, o.m_buckets);
        swap(m_shift, o.m_shift);
        swap(m_hash, o.m_hash);
        swap(m_eq, o.m_eq);
        swap(m_max_load, o.m_max_load);
    }

private:
    std::vector<Value>  m_values;
    std::vector<Bucket> m_buckets;
    unsigned            m_shift = 64;
    HashFn              m_hash;
    KeyEq               m_eq;
    float               m_max_load = 0.8f;

    template <class K>
    std::uint64_t hash_of(const K& k) const { return mix64(static_cast<std::uint64_t>(m_hash(k))); }

    size_type bucket_of(std::uint64_t h) const noexcept { return m_shift >= 64 ? 0 : static_cast<size_type>(h >> m_shift); }
    size_type next(size_type b) const noexcept { return (b + 1) & (m_buckets.size() - 1); }

    size_type locate(std::uint64_t h, size_type idx) const {
        size_type b = bucket_of(h);
        while (m_buckets[b].index != idx || m_buckets[b].dist_fp == 0) b = next(b);
        return b;
    }

    void place(std::uint64_t h, std::uint32_t idx) {
        std::uint32_t dist_fp = kDistInc | static_cast<std::uint32_t>(h & kFpMask);
        size_type b = bucket_of(h);

        while (m_buckets[b].dist_fp >= dist_fp) {
            dist_fp += kDistInc;
            b = next(b);
        }

        Bucket carry{ dist_fp, idx };

        while (m_buckets[b].dist_fp != 0) {
            std::swap(carry, m_buckets[b]);
            carry.dist_fp += kDistInc;
            b = next(b);
        }

        m_buckets[b] = carry;
    }

    void rebuild(size_type count) {
        m_buckets.assign(count, Bucket());
        unsigned bits = 0;
        while ((size_type(1) << bits) < count) ++bits;
        m_shift = 64 - bits;
        for (size_type i = 0; i < m_values.size(); ++i) place(hash_of(key_of<Value>::get(m_values[i])), static_cast<std::uint32_t>(i));
    }
};

} // namespace detail

template <class K, class V, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class HashMap {
    using Table = detail::DenseTable<K, std::pair<K, V>, HashFn, KeyEq>;

public:
    using key_type        = K;
    using mapped_type     = V;
    using value_type      = std::pair<K, V>;
    using size_type       = std::size_t;
    using hasher          = HashFn;
    using key_equal       = KeyEq;
    using iterator        = ArrayIterator<value_type>;
    using const_iterator  = ConstArrayIterator<value_type>;

    HashMap() = default;
    explicit HashMap(size_type bucket_hint, const HashFn& h = HashFn(), const KeyEq& e = KeyEq()) : m_t(h, e) { m_t.reserve(bucket_hint); }
    HashMap(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

    template <class It>
    HashMap(It first, It last) { insert(first, last); }

    size_type size() const noexcept { return m_t.size(); }
    bool empty() const noexcept { return m_t.size() == 0; }
    explicit operator bool() const noexcept { return !empty(); }

    iterator begin() noexcept { return iterator(m_t.values().data()); }
    iterator end() noexcept { return iterator(m_t.values().data() + m_t.values().size()); }
    const_iterator begin() const noexcept { return const_iterator(m_t.values().data()); }
    const_iterator end() const noexcept { return const_iterator(m_t.values().data() + m_t.values().size()); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    V& operator[](const K& key) { return m_t.values()[m_t.find_or_insert(key, [&] { return value_type(key, V()); }).first].second; }
    V& operator[](K&& key) {
        const size_type i = m_t.find_index(key);
        if (i != Table::npos) return m_t.values()[i].second;
        return m_t.values()[m_t.insert_value(value_type(std::move(key), V())).first].second;
    }

    V& at(const K& key) {
        const size_type i = m_t.find_index(key);
        if (i == Table::npos) throw std::out_of_range("HashMap::at: key not found");
        return m_t.values()[i].second;
    }

    const V& at(const K& key) const {
        const size_type i = m_t.find_index(key);
        if (i == Table::npos) throw std::out_of_range("HashMap::at: key not found");
        return m_t.values()[i].second;
    }

    V get(const K& key, const V& fallback = V()) const {
        const size_type i = m_t.find_index(key);
        return i == Table::npos ? fallback : m_t.values()[i].second;
    }

    V* get_ptr(const K& key) {
        const size_type i = m_t.find_index(key);
        return i == Table::npos ? nullptr : &m_t.values()[i].second;
    }

    const V* get_ptr(const K& key) const {
        const size_type i = m_t.find_index(key);
        return i == Table::npos ? nullptr : &m_t.values()[i].second;
    }

    iterator find(const K& key) {
        const size_type i = m_t.find_index(key);
        return i == Table::npos ? end() : begin() + static_cast<std::ptrdiff_t>(i);
    }

    const_iterator find(const K& key) const {
        const size_type i = m_t.find_index(key);
        return i == Table::npos ? end() : begin() + static_cast<std::ptrdiff_t>(i);
    }

    bool contains(const K& key) const { return m_t.find_index(key) != Table::npos; }
    size_type count(const K& key) const { return contains(key) ? 1 : 0; }

    std::pair<iterator, bool> insert(const value_type& v) {
        const auto r = m_t.find_or_insert(v.first, [&] { return v; });
        return { begin() + static_cast<std::ptrdiff_t>(r.first), r.second };
    }

    std::pair<iterator, bool> insert(value_type&& v) {
        const auto r = m_t.insert_value(std::move(v));
        return { begin() + static_cast<std::ptrdiff_t>(r.first), r.second };
    }

    template <class It>
    void insert(It first, It last) {
        for (; first != last; ++first) insert(value_type(*first));
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return insert(value_type(std::forward<Args>(args)...)); }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) {
        const auto r = m_t.find_or_insert(key, [&] { return value_type(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<Args>(args)...)); });
        return { begin() + static_cast<std::ptrdiff_t>(r.first), r.second };
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& value) {
        const size_type i = m_t.find_index(key);

        if (i != Table::npos) {
            m_t.values()[i].second = std::forward<M>(value);
            return { begin() + static_cast<std::ptrdiff_t>(i), false };
        }

        return insert(value_type(key, std::forward<M>(value)));
    }

    V& setdefault(const K& key, const V& value = V()) { return m_t.values()[m_t.find_or_insert(key, [&] { return value_type(key, value); }).first].second; }

    size_type erase(const K& key) { return m_t.erase_key(key) ? 1 : 0; }

    iterator erase(const_iterator it) {
        const size_type i = static_cast<size_type>(it - cbegin());
        m_t.erase_index(i);
        return begin() + static_cast<std::ptrdiff_t>(i);
    }

    V pop(const K& key) {
        const size_type i = m_t.find_index(key);
        if (i == Table::npos) throw std::out_of_range("HashMap::pop: key not found");
        V v = std::move(m_t.values()[i].second);
        m_t.erase_index(i);
        return v;
    }

    V pop(const K& key, const V& fallback) {
        const size_type i = m_t.find_index(key);
        if (i == Table::npos) return fallback;
        V v = std::move(m_t.values()[i].second);
        m_t.erase_index(i);
        return v;
    }

    value_type popitem() {
        if (empty()) throw std::out_of_range("HashMap::popitem: map is empty");
        value_type v = std::move(m_t.values().back());
        m_t.erase_index(size() - 1);
        return v;
    }

    template <class Pred>
    size_type erase_if(Pred pred) {
        size_type n = 0;

        for (size_type i = 0; i < size();) {
            if (pred(m_t.values()[i])) {
                m_t.erase_index(i);
                ++n;
            } else {
                ++i;
            }
        }

        return n;
    }

    template <class Other>
    void update(const Other& other) {
        for (const auto& kv : other) insert_or_assign(kv.first, kv.second);
    }

    std::vector<K> keys() const {
        std::vector<K> out;
        out.reserve(size());
        for (const auto& kv : *this) out.push_back(kv.first);
        return out;
    }

    std::vector<V> values() const {
        std::vector<V> out;
        out.reserve(size());
        for (const auto& kv : *this) out.push_back(kv.second);
        return out;
    }

    const std::vector<value_type>& items() const noexcept { return m_t.values(); }

    void clear() noexcept { m_t.clear(); }
    void reserve(size_type n) { m_t.reserve(n); }
    void rehash(size_type n) { m_t.rehash(n); }
    void shrink_to_fit() { m_t.shrink_to_fit(); }
    size_type bucket_count() const noexcept { return m_t.bucket_count(); }
    float load_factor() const noexcept { return m_t.load_factor(); }
    float max_load_factor() const noexcept { return m_t.max_load_factor(); }
    void max_load_factor(float f) { m_t.max_load_factor(f); }
    hasher hash_function() const { return m_t.hash_function(); }
    key_equal key_eq() const { return m_t.key_eq(); }

    void swap(HashMap& o) noexcept { m_t.swap(o.m_t); }

    friend bool operator==(const HashMap& a, const HashMap& b) {
        if (a.size() != b.size()) return false;

        for (const auto& kv : a) {
            const V* v = b.get_ptr(kv.first);
            if (!v || !(*v == kv.second)) return false;
        }

        return true;
    }

    friend bool operator!=(const HashMap& a, const HashMap& b) { return !(a == b); }

    friend HashMap operator|(HashMap a, const HashMap& b) {
        a.update(b);
        return a;
    }

    HashMap& operator|=(const HashMap& b) {
        update(b);
        return *this;
    }

private:
    Table m_t;
};

template <class K, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class HashSet {
    using Table = detail::DenseTable<K, K, HashFn, KeyEq>;

public:
    using key_type       = K;
    using value_type     = K;
    using size_type      = std::size_t;
    using hasher         = HashFn;
    using key_equal      = KeyEq;
    using iterator       = ConstArrayIterator<K>;
    using const_iterator = ConstArrayIterator<K>;

    HashSet() = default;
    explicit HashSet(size_type bucket_hint, const HashFn& h = HashFn(), const KeyEq& e = KeyEq()) : m_t(h, e) { m_t.reserve(bucket_hint); }
    HashSet(std::initializer_list<K> init) { insert(init.begin(), init.end()); }

    template <class It>
    HashSet(It first, It last) { insert(first, last); }

    size_type size() const noexcept { return m_t.size(); }
    bool empty() const noexcept { return m_t.size() == 0; }
    explicit operator bool() const noexcept { return !empty(); }

    const_iterator begin() const noexcept { return const_iterator(m_t.values().data()); }
    const_iterator end() const noexcept { return const_iterator(m_t.values().data() + m_t.values().size()); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    bool contains(const K& key) const { return m_t.find_index(key) != Table::npos; }
    size_type count(const K& key) const { return contains(key) ? 1 : 0; }

    const_iterator find(const K& key) const {
        const size_type i = m_t.find_index(key);
        return i == Table::npos ? end() : begin() + static_cast<std::ptrdiff_t>(i);
    }

    std::pair<const_iterator, bool> insert(const K& key) {
        const auto r = m_t.find_or_insert(key, [&] { return key; });
        return { begin() + static_cast<std::ptrdiff_t>(r.first), r.second };
    }

    std::pair<const_iterator, bool> insert(K&& key) {
        const auto r = m_t.insert_value(std::move(key));
        return { begin() + static_cast<std::ptrdiff_t>(r.first), r.second };
    }

    template <class It>
    void insert(It first, It last) {
        for (; first != last; ++first) insert(K(*first));
    }

    template <class... Args>
    std::pair<const_iterator, bool> emplace(Args&&... args) { return insert(K(std::forward<Args>(args)...)); }

    bool add(const K& key) { return insert(key).second; }

    size_type erase(const K& key) { return m_t.erase_key(key) ? 1 : 0; }

    const_iterator erase(const_iterator it) {
        const size_type i = static_cast<size_type>(it - cbegin());
        m_t.erase_index(i);
        return begin() + static_cast<std::ptrdiff_t>(i);
    }

    bool discard(const K& key) { return m_t.erase_key(key); }

    void remove(const K& key) {
        if (!m_t.erase_key(key)) throw std::out_of_range("HashSet::remove: key not present");
    }

    K pop() {
        if (empty()) throw std::out_of_range("HashSet::pop: set is empty");
        K v = std::move(m_t.values().back());
        m_t.erase_index(size() - 1);
        return v;
    }

    template <class Pred>
    size_type erase_if(Pred pred) {
        size_type n = 0;

        for (size_type i = 0; i < size();) {
            if (pred(m_t.values()[i])) {
                m_t.erase_index(i);
                ++n;
            } else {
                ++i;
            }
        }

        return n;
    }

    template <class C>
    void update(const C& other) {
        for (const auto& k : other) insert(k);
    }

    HashSet union_with(const HashSet& o) const {
        HashSet r = *this;
        r.update(o);
        return r;
    }

    HashSet intersection(const HashSet& o) const {
        const HashSet& small = size() <= o.size() ? *this : o;
        const HashSet& large = size() <= o.size() ? o : *this;
        HashSet r;
        for (const auto& k : small) if (large.contains(k)) r.insert(k);
        return r;
    }

    HashSet difference(const HashSet& o) const {
        HashSet r;
        for (const auto& k : *this) if (!o.contains(k)) r.insert(k);
        return r;
    }

    HashSet symmetric_difference(const HashSet& o) const {
        HashSet r = difference(o);
        for (const auto& k : o) if (!contains(k)) r.insert(k);
        return r;
    }

    bool is_subset(const HashSet& o) const {
        if (size() > o.size()) return false;
        for (const auto& k : *this) if (!o.contains(k)) return false;
        return true;
    }

    bool is_superset(const HashSet& o) const { return o.is_subset(*this); }

    bool is_disjoint(const HashSet& o) const {
        const HashSet& small = size() <= o.size() ? *this : o;
        const HashSet& large = size() <= o.size() ? o : *this;
        for (const auto& k : small) if (large.contains(k)) return false;
        return true;
    }

    std::vector<K> to_vector() const { return m_t.values(); }

    void clear() noexcept { m_t.clear(); }
    void reserve(size_type n) { m_t.reserve(n); }
    void rehash(size_type n) { m_t.rehash(n); }
    void shrink_to_fit() { m_t.shrink_to_fit(); }
    size_type bucket_count() const noexcept { return m_t.bucket_count(); }
    float load_factor() const noexcept { return m_t.load_factor(); }
    float max_load_factor() const noexcept { return m_t.max_load_factor(); }
    void max_load_factor(float f) { m_t.max_load_factor(f); }
    hasher hash_function() const { return m_t.hash_function(); }
    key_equal key_eq() const { return m_t.key_eq(); }

    void swap(HashSet& o) noexcept { m_t.swap(o.m_t); }

    friend bool operator==(const HashSet& a, const HashSet& b) { return a.size() == b.size() && a.is_subset(b); }
    friend bool operator!=(const HashSet& a, const HashSet& b) { return !(a == b); }
    friend bool operator<=(const HashSet& a, const HashSet& b) { return a.is_subset(b); }
    friend bool operator>=(const HashSet& a, const HashSet& b) { return b.is_subset(a); }
    friend bool operator<(const HashSet& a, const HashSet& b) { return a.size() < b.size() && a.is_subset(b); }
    friend bool operator>(const HashSet& a, const HashSet& b) { return b < a; }
    friend HashSet operator|(const HashSet& a, const HashSet& b) { return a.union_with(b); }
    friend HashSet operator&(const HashSet& a, const HashSet& b) { return a.intersection(b); }
    friend HashSet operator-(const HashSet& a, const HashSet& b) { return a.difference(b); }
    friend HashSet operator^(const HashSet& a, const HashSet& b) { return a.symmetric_difference(b); }
    HashSet& operator|=(const HashSet& b) { update(b); return *this; }
    HashSet& operator&=(const HashSet& b) { *this = intersection(b); return *this; }
    HashSet& operator-=(const HashSet& b) { for (const auto& k : b) discard(k); return *this; }
    HashSet& operator^=(const HashSet& b) { *this = symmetric_difference(b); return *this; }

private:
    Table m_t;
};

template <class K, class V, class H = Hash<K>, class E = std::equal_to<K>>
using UnorderedMap = HashMap<K, V, H, E>;

template <class K, class H = Hash<K>, class E = std::equal_to<K>>
using UnorderedSet = HashSet<K, H, E>;

template <class K, class V, class H = Hash<K>, class E = std::equal_to<K>>
using Dict = HashMap<K, V, H, E>;

template <class K, class H = Hash<K>, class E = std::equal_to<K>>
using Set = HashSet<K, H, E>;

template <class K, class V, class H, class E>
void swap(HashMap<K, V, H, E>& a, HashMap<K, V, H, E>& b) noexcept { a.swap(b); }

template <class K, class H, class E>
void swap(HashSet<K, H, E>& a, HashSet<K, H, E>& b) noexcept { a.swap(b); }

template <class K, class H, class E>
struct Hash<HashSet<K, H, E>> {
    std::size_t operator()(const HashSet<K, H, E>& s) const {
        std::uint64_t acc = 0;
        for (const auto& k : s) acc += mix64(static_cast<std::uint64_t>(Hash<K>()(k)));
        return static_cast<std::size_t>(mix64(acc ^ s.size()));
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_HASH_MAP_HPP
