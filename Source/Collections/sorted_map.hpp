#ifndef FIZMO_COLLECTIONS_SORTED_MAP_HPP
#define FIZMO_COLLECTIONS_SORTED_MAP_HPP

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "hash.hpp"
#include "sorted_tree.hpp"

namespace fizmo {
namespace collections {
namespace detail {

template <class K, class V>
struct PairKeyOf {
    static const K& get(const std::pair<const K, V>& v) noexcept { return v.first; }
};

template <class K>
struct IdentityKeyOf {
    static const K& get(const K& v) noexcept { return v; }
};

template <class K, class V, class Compare, bool Multi>
class SortedMapBase {
protected:
    using Tree = AvlTree<K, std::pair<const K, V>, PairKeyOf<K, V>, Compare, Multi>;
    using Node = typename Tree::Node;
    Tree m_t;

public:
    using key_type       = K;
    using mapped_type    = V;
    using value_type     = std::pair<const K, V>;
    using size_type      = std::size_t;
    using key_compare    = Compare;
    using iterator       = typename Tree::iterator;
    using const_iterator = typename Tree::const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    SortedMapBase() = default;
    explicit SortedMapBase(const Compare& c) : m_t(c) {}

    size_type size() const noexcept { return m_t.size(); }
    bool empty() const noexcept { return m_t.empty(); }
    explicit operator bool() const noexcept { return !empty(); }
    void clear() noexcept { m_t.clear(); }

    iterator begin() noexcept { return m_t.begin(); }
    iterator end() noexcept { return m_t.end(); }
    const_iterator begin() const noexcept { return m_t.begin(); }
    const_iterator end() const noexcept { return m_t.end(); }
    const_iterator cbegin() const noexcept { return m_t.begin(); }
    const_iterator cend() const noexcept { return m_t.end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    iterator find(const K& key) { return m_t.make_iterator(m_t.find(key)); }
    const_iterator find(const K& key) const { return m_t.make_iterator(m_t.find(key)); }
    bool contains(const K& key) const { return m_t.find(key) != nullptr; }
    size_type count(const K& key) const { return m_t.count(key); }

    iterator lower_bound(const K& key) { return m_t.make_iterator(m_t.lower_bound(key)); }
    const_iterator lower_bound(const K& key) const { return m_t.make_iterator(m_t.lower_bound(key)); }
    iterator upper_bound(const K& key) { return m_t.make_iterator(m_t.upper_bound(key)); }
    const_iterator upper_bound(const K& key) const { return m_t.make_iterator(m_t.upper_bound(key)); }
    std::pair<iterator, iterator> equal_range(const K& key) { return { lower_bound(key), upper_bound(key) }; }
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }

    iterator floor(const K& key) { return m_t.make_iterator(m_t.floor_node(key)); }
    const_iterator floor(const K& key) const { return m_t.make_iterator(m_t.floor_node(key)); }
    iterator ceiling(const K& key) { return lower_bound(key); }
    const_iterator ceiling(const K& key) const { return lower_bound(key); }
    iterator lower(const K& key) { return m_t.make_iterator(m_t.below_node(key)); }
    const_iterator lower(const K& key) const { return m_t.make_iterator(m_t.below_node(key)); }
    iterator higher(const K& key) { return upper_bound(key); }
    const_iterator higher(const K& key) const { return upper_bound(key); }

    value_type& first() { return checked(m_t.first())->value; }
    const value_type& first() const { return checked(m_t.first())->value; }
    value_type& last() { return checked(m_t.last())->value; }
    const value_type& last() const { return checked(m_t.last())->value; }

    std::pair<K, V> pop_first() { return take(checked(m_t.first())); }
    std::pair<K, V> pop_last() { return take(checked(m_t.last())); }

    value_type& at_index(size_type i) {
        Node* n = m_t.select(normalize_index(i));
        return n->value;
    }

    const value_type& at_index(size_type i) const {
        Node* n = m_t.select(normalize_index(i));
        return n->value;
    }

    size_type index_of(const K& key) const {
        Node* n = m_t.find(key);
        if (!n) throw std::out_of_range("Sorted container: key not present");
        return m_t.rank(n);
    }

    size_type bisect_left(const K& key) const { return m_t.rank_of_bound(m_t.lower_bound(key)); }
    size_type bisect_right(const K& key) const { return m_t.rank_of_bound(m_t.upper_bound(key)); }

    iterator erase(const_iterator it) {
        Node* n = it.node();
        Node* nx = Tree::successor(n);
        m_t.erase(n);
        return m_t.make_iterator(nx);
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) first = erase(first);
        return m_t.make_iterator(last.node());
    }

    size_type erase(const K& key) { return m_t.erase_key(key); }

    template <class Pred>
    size_type erase_if(Pred pred) {
        size_type n = 0;

        for (auto it = cbegin(); it != cend();) {
            if (pred(*it)) {
                it = erase(it);
                ++n;
            } else {
                ++it;
            }
        }

        return n;
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

    std::vector<std::pair<K, V>> items() const { return std::vector<std::pair<K, V>>(begin(), end()); }

    std::pair<const_iterator, const_iterator> range(const K& lo, const K& hi) const { return { lower_bound(lo), lower_bound(hi) }; }

    key_compare key_comp() const { return m_t.compare(); }

protected:
    static Node* checked(Node* n) {
        if (!n) throw std::out_of_range("Sorted container is empty");
        return n;
    }

    size_type normalize_index(size_type i) const {
        if (i >= size()) throw std::out_of_range("Sorted container index out of range");
        return i;
    }

    std::pair<K, V> take(Node* n) {
        std::pair<K, V> v(n->value.first, std::move(n->value.second));
        m_t.erase(n);
        return v;
    }
};

template <class K, class Compare, bool Multi>
class SortedSetBase {
protected:
    using Tree = AvlTree<K, K, IdentityKeyOf<K>, Compare, Multi>;
    using Node = typename Tree::Node;
    Tree m_t;

public:
    using key_type       = K;
    using value_type     = K;
    using size_type      = std::size_t;
    using key_compare    = Compare;
    using iterator       = typename Tree::const_iterator;
    using const_iterator = typename Tree::const_iterator;
    using reverse_iterator       = std::reverse_iterator<const_iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    SortedSetBase() = default;
    explicit SortedSetBase(const Compare& c) : m_t(c) {}

    size_type size() const noexcept { return m_t.size(); }
    bool empty() const noexcept { return m_t.empty(); }
    explicit operator bool() const noexcept { return !empty(); }
    void clear() noexcept { m_t.clear(); }

    const_iterator begin() const noexcept { return m_t.begin(); }
    const_iterator end() const noexcept { return m_t.end(); }
    const_iterator cbegin() const noexcept { return m_t.begin(); }
    const_iterator cend() const noexcept { return m_t.end(); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    const_iterator find(const K& key) const { return m_t.make_iterator(m_t.find(key)); }
    bool contains(const K& key) const { return m_t.find(key) != nullptr; }
    size_type count(const K& key) const { return m_t.count(key); }

    const_iterator lower_bound(const K& key) const { return m_t.make_iterator(m_t.lower_bound(key)); }
    const_iterator upper_bound(const K& key) const { return m_t.make_iterator(m_t.upper_bound(key)); }
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const { return { lower_bound(key), upper_bound(key) }; }
    const_iterator floor(const K& key) const { return m_t.make_iterator(m_t.floor_node(key)); }
    const_iterator ceiling(const K& key) const { return lower_bound(key); }
    const_iterator lower(const K& key) const { return m_t.make_iterator(m_t.below_node(key)); }
    const_iterator higher(const K& key) const { return upper_bound(key); }

    const K& first() const { return checked(m_t.first())->value; }
    const K& last() const { return checked(m_t.last())->value; }

    K pop_first() { return take(checked(m_t.first())); }
    K pop_last() { return take(checked(m_t.last())); }

    const K& at_index(size_type i) const {
        if (i >= size()) throw std::out_of_range("Sorted container index out of range");
        return m_t.select(i)->value;
    }

    const K& operator[](size_type i) const { return at_index(i); }

    K pop_index(size_type i) {
        if (i >= size()) throw std::out_of_range("Sorted container index out of range");
        return take(m_t.select(i));
    }

    size_type index_of(const K& key) const {
        Node* n = m_t.find(key);
        if (!n) throw std::out_of_range("Sorted container: key not present");
        return m_t.rank(n);
    }

    size_type bisect_left(const K& key) const { return m_t.rank_of_bound(m_t.lower_bound(key)); }
    size_type bisect_right(const K& key) const { return m_t.rank_of_bound(m_t.upper_bound(key)); }

    template <class... Args>
    std::pair<const_iterator, bool> emplace(Args&&... args) {
        const auto r = m_t.emplace(std::forward<Args>(args)...);
        return { m_t.make_iterator(r.first), r.second };
    }

    std::pair<const_iterator, bool> insert(const K& key) { return emplace(key); }
    std::pair<const_iterator, bool> insert(K&& key) { return emplace(std::move(key)); }

    template <class It>
    void insert(It first, It last) {
        for (; first != last; ++first) emplace(*first);
    }

    template <class C>
    void update(const C& other) {
        for (const auto& k : other) emplace(k);
    }

    bool add(const K& key) { return insert(key).second; }

    const_iterator erase(const_iterator it) {
        Node* n = it.node();
        Node* nx = Tree::successor(n);
        m_t.erase(n);
        return m_t.make_iterator(nx);
    }

    size_type erase(const K& key) { return m_t.erase_key(key); }

    bool discard(const K& key) {
        Node* n = m_t.find(key);
        if (!n) return false;
        m_t.erase(n);
        return true;
    }

    void remove(const K& key) {
        if (!discard(key)) throw std::out_of_range("Sorted container: key not present");
    }

    template <class Pred>
    size_type erase_if(Pred pred) {
        size_type n = 0;

        for (auto it = cbegin(); it != cend();) {
            if (pred(*it)) {
                it = erase(it);
                ++n;
            } else {
                ++it;
            }
        }

        return n;
    }

    std::pair<const_iterator, const_iterator> range(const K& lo, const K& hi) const { return { lower_bound(lo), lower_bound(hi) }; }

    std::vector<K> to_vector() const { return std::vector<K>(begin(), end()); }

    key_compare key_comp() const { return m_t.compare(); }

protected:
    static Node* checked(Node* n) {
        if (!n) throw std::out_of_range("Sorted container is empty");
        return n;
    }

    K take(Node* n) {
        K v = std::move(n->value);
        m_t.erase(n);
        return v;
    }
};

} // namespace detail

template <class K, class V, class Compare = std::less<K>>
class SortedMap : public detail::SortedMapBase<K, V, Compare, false> {
    using Base = detail::SortedMapBase<K, V, Compare, false>;

public:
    using typename Base::iterator;
    using typename Base::value_type;
    using typename Base::size_type;

    SortedMap() = default;
    explicit SortedMap(const Compare& c) : Base(c) {}
    SortedMap(std::initializer_list<std::pair<K, V>> init) { for (const auto& kv : init) insert(kv); }

    template <class It>
    SortedMap(It first, It last) { for (; first != last; ++first) insert(*first); }

    V& operator[](const K& key) { return this->m_t.find_or_emplace(key, [&] { return value_type(key, V()); }).first->value.second; }

    V& at(const K& key) {
        auto* n = this->m_t.find(key);
        if (!n) throw std::out_of_range("SortedMap::at: key not found");
        return n->value.second;
    }

    const V& at(const K& key) const {
        auto* n = this->m_t.find(key);
        if (!n) throw std::out_of_range("SortedMap::at: key not found");
        return n->value.second;
    }

    V get(const K& key, const V& fallback = V()) const {
        auto* n = this->m_t.find(key);
        return n ? n->value.second : fallback;
    }

    std::pair<iterator, bool> insert(const std::pair<K, V>& kv) {
        const auto r = this->m_t.find_or_emplace(kv.first, [&] { return value_type(kv.first, kv.second); });
        return { this->m_t.make_iterator(r.first), r.second };
    }

    std::pair<iterator, bool> insert(std::pair<K, V>&& kv) {
        auto* existing = this->m_t.find(kv.first);
        if (existing) return { this->m_t.make_iterator(existing), false };
        const auto r = this->m_t.emplace(std::move(kv.first), std::move(kv.second));
        return { this->m_t.make_iterator(r.first), r.second };
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) {
        const auto r = this->m_t.find_or_emplace(key, [&] { return value_type(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<Args>(args)...)); });
        return { this->m_t.make_iterator(r.first), r.second };
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& value) {
        auto* existing = this->m_t.find(key);

        if (existing) {
            existing->value.second = std::forward<M>(value);
            return { this->m_t.make_iterator(existing), false };
        }

        return insert(std::pair<K, V>(key, std::forward<M>(value)));
    }

    V& setdefault(const K& key, const V& value = V()) { return this->m_t.find_or_emplace(key, [&] { return value_type(key, value); }).first->value.second; }

    V pop(const K& key) {
        auto* n = this->m_t.find(key);
        if (!n) throw std::out_of_range("SortedMap::pop: key not found");
        V v = std::move(n->value.second);
        this->m_t.erase(n);
        return v;
    }

    V pop(const K& key, const V& fallback) {
        auto* n = this->m_t.find(key);
        if (!n) return fallback;
        V v = std::move(n->value.second);
        this->m_t.erase(n);
        return v;
    }

    template <class Other>
    void update(const Other& other) {
        for (const auto& kv : other) insert_or_assign(kv.first, kv.second);
    }

    void swap(SortedMap& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const SortedMap& a, const SortedMap& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const SortedMap& a, const SortedMap& b) { return !(a == b); }
    friend bool operator<(const SortedMap& a, const SortedMap& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend bool operator>(const SortedMap& a, const SortedMap& b) { return b < a; }
    friend bool operator<=(const SortedMap& a, const SortedMap& b) { return !(b < a); }
    friend bool operator>=(const SortedMap& a, const SortedMap& b) { return !(a < b); }
};

template <class K, class V, class Compare = std::less<K>>
class SortedMultiMap : public detail::SortedMapBase<K, V, Compare, true> {
    using Base = detail::SortedMapBase<K, V, Compare, true>;

public:
    using typename Base::iterator;
    using typename Base::value_type;

    SortedMultiMap() = default;
    explicit SortedMultiMap(const Compare& c) : Base(c) {}
    SortedMultiMap(std::initializer_list<std::pair<K, V>> init) { for (const auto& kv : init) insert(kv); }

    iterator insert(const std::pair<K, V>& kv) { return this->m_t.make_iterator(this->m_t.emplace(kv.first, kv.second).first); }
    iterator insert(std::pair<K, V>&& kv) { return this->m_t.make_iterator(this->m_t.emplace(std::move(kv.first), std::move(kv.second)).first); }

    template <class... Args>
    iterator emplace(Args&&... args) { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    std::vector<V> get_all(const K& key) const {
        std::vector<V> out;
        for (auto r = this->equal_range(key); r.first != r.second; ++r.first) out.push_back(r.first->second);
        return out;
    }

    void swap(SortedMultiMap& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const SortedMultiMap& a, const SortedMultiMap& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const SortedMultiMap& a, const SortedMultiMap& b) { return !(a == b); }
};

template <class K, class Compare = std::less<K>>
class SortedSet : public detail::SortedSetBase<K, Compare, false> {
    using Base = detail::SortedSetBase<K, Compare, false>;

public:
    SortedSet() = default;
    explicit SortedSet(const Compare& c) : Base(c) {}
    SortedSet(std::initializer_list<K> init) { this->insert(init.begin(), init.end()); }

    template <class It>
    SortedSet(It first, It last) { this->insert(first, last); }

    SortedSet union_with(const SortedSet& o) const {
        SortedSet r(this->key_comp());
        std::vector<K> v;
        std::set_union(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), this->key_comp());
        r.insert(v.begin(), v.end());
        return r;
    }

    SortedSet intersection(const SortedSet& o) const {
        SortedSet r(this->key_comp());
        std::vector<K> v;
        std::set_intersection(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), this->key_comp());
        r.insert(v.begin(), v.end());
        return r;
    }

    SortedSet difference(const SortedSet& o) const {
        SortedSet r(this->key_comp());
        std::vector<K> v;
        std::set_difference(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), this->key_comp());
        r.insert(v.begin(), v.end());
        return r;
    }

    SortedSet symmetric_difference(const SortedSet& o) const {
        SortedSet r(this->key_comp());
        std::vector<K> v;
        std::set_symmetric_difference(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), this->key_comp());
        r.insert(v.begin(), v.end());
        return r;
    }

    bool is_subset(const SortedSet& o) const { return std::includes(o.begin(), o.end(), this->begin(), this->end(), this->key_comp()); }
    bool is_superset(const SortedSet& o) const { return o.is_subset(*this); }
    bool is_disjoint(const SortedSet& o) const { return intersection(o).empty(); }

    void swap(SortedSet& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const SortedSet& a, const SortedSet& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const SortedSet& a, const SortedSet& b) { return !(a == b); }
    friend bool operator<(const SortedSet& a, const SortedSet& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend bool operator>(const SortedSet& a, const SortedSet& b) { return b < a; }
    friend bool operator<=(const SortedSet& a, const SortedSet& b) { return !(b < a); }
    friend bool operator>=(const SortedSet& a, const SortedSet& b) { return !(a < b); }
    friend SortedSet operator|(const SortedSet& a, const SortedSet& b) { return a.union_with(b); }
    friend SortedSet operator&(const SortedSet& a, const SortedSet& b) { return a.intersection(b); }
    friend SortedSet operator-(const SortedSet& a, const SortedSet& b) { return a.difference(b); }
    friend SortedSet operator^(const SortedSet& a, const SortedSet& b) { return a.symmetric_difference(b); }
};

template <class K, class Compare = std::less<K>>
class SortedMultiSet : public detail::SortedSetBase<K, Compare, true> {
    using Base = detail::SortedSetBase<K, Compare, true>;

public:
    SortedMultiSet() = default;
    explicit SortedMultiSet(const Compare& c) : Base(c) {}
    SortedMultiSet(std::initializer_list<K> init) { this->insert(init.begin(), init.end()); }

    template <class It>
    SortedMultiSet(It first, It last) { this->insert(first, last); }

    void swap(SortedMultiSet& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const SortedMultiSet& a, const SortedMultiSet& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const SortedMultiSet& a, const SortedMultiSet& b) { return !(a == b); }
    friend bool operator<(const SortedMultiSet& a, const SortedMultiSet& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
};

template <class K, class Compare = std::less<K>>
using SortedList = SortedMultiSet<K, Compare>;

template <class K, class V, class Compare = std::less<K>>
using OrderedMap = SortedMap<K, V, Compare>;

template <class K, class Compare = std::less<K>>
using OrderedSet = SortedSet<K, Compare>;

template <class K, class V, class C>
void swap(SortedMap<K, V, C>& a, SortedMap<K, V, C>& b) noexcept { a.swap(b); }

template <class K, class C>
void swap(SortedSet<K, C>& a, SortedSet<K, C>& b) noexcept { a.swap(b); }

template <class K, class C>
struct Hash<SortedSet<K, C>> {
    std::size_t operator()(const SortedSet<K, C>& s) const {
        std::size_t h = s.size();
        for (const auto& k : s) h = hash_combine(h, Hash<K>()(k));
        return h;
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_SORTED_MAP_HPP
