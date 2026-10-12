#ifndef FIZMO_COLLECTIONS_MISC_HPP
#define FIZMO_COLLECTIONS_MISC_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "hash_map.hpp"
#include "iterator.hpp"

namespace fizmo {
namespace collections {

template <class K, class V, class HashFn = Hash<K>, class KeyEq = std::equal_to<K>>
class LRUCache {
    using List = std::list<std::pair<K, V>>;

public:
    using size_type = std::size_t;
    using iterator  = typename List::const_iterator;

    explicit LRUCache(size_type capacity) : m_cap(capacity) {
        if (capacity == 0) throw std::invalid_argument("LRUCache capacity must be positive");
    }

    size_type size() const noexcept { return m_list.size(); }
    size_type capacity() const noexcept { return m_cap; }
    bool empty() const noexcept { return m_list.empty(); }
    bool contains(const K& key) const { return m_index.contains(key); }

    iterator begin() const noexcept { return m_list.begin(); }
    iterator end() const noexcept { return m_list.end(); }

    V* get(const K& key) {
        auto* it = m_index.get_ptr(key);
        if (!it) return nullptr;
        m_list.splice(m_list.begin(), m_list, *it);
        return &(*it)->second;
    }

    const V* peek(const K& key) const {
        auto* it = m_index.get_ptr(key);
        return it ? &(*it)->second : nullptr;
    }

    V get_or(const K& key, const V& fallback) {
        V* v = get(key);
        return v ? *v : fallback;
    }

    void put(const K& key, V value) {
        auto* it = m_index.get_ptr(key);

        if (it) {
            (*it)->second = std::move(value);
            m_list.splice(m_list.begin(), m_list, *it);
            return;
        }

        m_list.emplace_front(key, std::move(value));
        m_index.insert({ key, m_list.begin() });

        while (m_list.size() > m_cap) {
            if (m_on_evict) m_on_evict(m_list.back().first, m_list.back().second);
            m_index.erase(m_list.back().first);
            m_list.pop_back();
        }
    }

    template <class F>
    V& get_or_compute(const K& key, F&& make) {
        V* v = get(key);
        if (v) return *v;
        put(key, make());
        return m_list.front().second;
    }

    bool erase(const K& key) {
        auto* it = m_index.get_ptr(key);
        if (!it) return false;
        m_list.erase(*it);
        m_index.erase(key);
        return true;
    }

    void set_capacity(size_type cap) {
        if (cap == 0) throw std::invalid_argument("LRUCache capacity must be positive");
        m_cap = cap;

        while (m_list.size() > m_cap) {
            if (m_on_evict) m_on_evict(m_list.back().first, m_list.back().second);
            m_index.erase(m_list.back().first);
            m_list.pop_back();
        }
    }

    void on_evict(std::function<void(const K&, V&)> f) { m_on_evict = std::move(f); }

    void clear() noexcept {
        m_list.clear();
        m_index.clear();
    }

private:
    size_type                                                     m_cap;
    List                                                          m_list;
    HashMap<K, typename List::iterator, HashFn, KeyEq>            m_index;
    std::function<void(const K&, V&)>                             m_on_evict;
};

template <class A, class B, class HashA = Hash<A>, class HashB = Hash<B>>
class BiMap {
public:
    using size_type = std::size_t;

    BiMap() = default;
    BiMap(std::initializer_list<std::pair<A, B>> init) { for (const auto& p : init) insert(p.first, p.second); }

    size_type size() const noexcept { return m_fwd.size(); }
    bool empty() const noexcept { return m_fwd.empty(); }

    auto begin() const noexcept { return m_fwd.begin(); }
    auto end() const noexcept { return m_fwd.end(); }

    bool insert(const A& a, const B& b) {
        if (m_fwd.contains(a) || m_rev.contains(b)) return false;
        m_fwd.insert({ a, b });
        m_rev.insert({ b, a });
        return true;
    }

    void assign(const A& a, const B& b) {
        erase_left(a);
        erase_right(b);
        insert(a, b);
    }

    const B& at_left(const A& a) const { return m_fwd.at(a); }
    const A& at_right(const B& b) const { return m_rev.at(b); }
    const B* find_left(const A& a) const { return m_fwd.get_ptr(a); }
    const A* find_right(const B& b) const { return m_rev.get_ptr(b); }
    bool contains_left(const A& a) const { return m_fwd.contains(a); }
    bool contains_right(const B& b) const { return m_rev.contains(b); }

    bool erase_left(const A& a) {
        const B* b = m_fwd.get_ptr(a);
        if (!b) return false;
        m_rev.erase(*b);
        m_fwd.erase(a);
        return true;
    }

    bool erase_right(const B& b) {
        const A* a = m_rev.get_ptr(b);
        if (!a) return false;
        m_fwd.erase(*a);
        m_rev.erase(b);
        return true;
    }

    const HashMap<A, B, HashA>& left() const noexcept { return m_fwd; }
    const HashMap<B, A, HashB>& right() const noexcept { return m_rev; }

    void clear() noexcept {
        m_fwd.clear();
        m_rev.clear();
    }

private:
    HashMap<A, B, HashA> m_fwd;
    HashMap<B, A, HashB> m_rev;
};

class DisjointSet {
public:
    using size_type = std::size_t;

    DisjointSet() = default;
    explicit DisjointSet(size_type n) { reset(n); }

    void reset(size_type n) {
        m_parent.resize(n);
        m_rank.assign(n, 0);
        m_size.assign(n, 1);
        for (size_type i = 0; i < n; ++i) m_parent[i] = i;
        m_sets = n;
    }

    size_type add() {
        m_parent.push_back(m_parent.size());
        m_rank.push_back(0);
        m_size.push_back(1);
        ++m_sets;
        return m_parent.size() - 1;
    }

    size_type size() const noexcept { return m_parent.size(); }
    size_type set_count() const noexcept { return m_sets; }

    size_type find(size_type x) {
        check(x);
        size_type root = x;
        while (m_parent[root] != root) root = m_parent[root];

        while (m_parent[x] != root) {
            const size_type next = m_parent[x];
            m_parent[x] = root;
            x = next;
        }

        return root;
    }

    bool unite(size_type a, size_type b) {
        a = find(a);
        b = find(b);
        if (a == b) return false;
        if (m_rank[a] < m_rank[b]) std::swap(a, b);
        m_parent[b] = a;
        m_size[a] += m_size[b];
        if (m_rank[a] == m_rank[b]) ++m_rank[a];
        --m_sets;
        return true;
    }

    bool connected(size_type a, size_type b) { return find(a) == find(b); }
    size_type set_size(size_type x) { return m_size[find(x)]; }

    std::vector<std::vector<size_type>> groups() {
        HashMap<size_type, std::vector<size_type>> g;
        for (size_type i = 0; i < m_parent.size(); ++i) g[find(i)].push_back(i);
        std::vector<std::vector<size_type>> out;
        out.reserve(g.size());
        for (auto& kv : g) out.push_back(std::move(kv.second));
        return out;
    }

private:
    std::vector<size_type>     m_parent;
    std::vector<std::uint8_t>  m_rank;
    std::vector<size_type>     m_size;
    size_type                  m_sets = 0;

    void check(size_type x) const {
        if (x >= m_parent.size()) throw std::out_of_range("DisjointSet element out of range");
    }
};

class DynamicBitset {
public:
    using size_type  = std::size_t;
    using word_type  = std::uint64_t;
    using value_type = bool;

    class reference {
    public:
        reference(word_type& w, word_type mask) : m_w(w), m_mask(mask) {}
        operator bool() const noexcept { return (m_w & m_mask) != 0; }
        reference& operator=(bool v) noexcept {
            if (v) m_w |= m_mask;
            else m_w &= ~m_mask;
            return *this;
        }
        reference& operator=(const reference& o) noexcept { return *this = static_cast<bool>(o); }
        reference& flip() noexcept {
            m_w ^= m_mask;
            return *this;
        }

    private:
        word_type& m_w;
        word_type  m_mask;
    };

    DynamicBitset() = default;
    explicit DynamicBitset(size_type n, bool value = false) { resize(n, value); }

    explicit DynamicBitset(const std::string& bits) {
        resize(bits.size());
        for (size_type i = 0; i < bits.size(); ++i) set(bits.size() - 1 - i, bits[i] == '1');
    }

    size_type size() const noexcept { return m_bits; }
    bool empty() const noexcept { return m_bits == 0; }

    void resize(size_type n, bool value = false) {
        const size_type old = m_bits;
        m_words.resize((n + 63) / 64, value ? ~word_type(0) : 0);

        if (value && old < n && old % 64 != 0) m_words[old / 64] |= ~word_type(0) << (old % 64);
        m_bits = n;
        trim();
    }

    void push_back(bool v) {
        resize(m_bits + 1);
        set(m_bits - 1, v);
    }

    bool test(size_type i) const {
        check(i);
        return (m_words[i / 64] >> (i % 64)) & 1;
    }

    bool operator[](size_type i) const noexcept { return (m_words[i / 64] >> (i % 64)) & 1; }
    reference operator[](size_type i) noexcept { return reference(m_words[i / 64], word_type(1) << (i % 64)); }

    using iterator       = IndexIterator<DynamicBitset, false>;
    using const_iterator = IndexIterator<DynamicBitset, true>;

    iterator begin() noexcept { return iterator(this, 0); }
    iterator end() noexcept { return iterator(this, m_bits); }
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    const_iterator end() const noexcept { return const_iterator(this, m_bits); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    DynamicBitset& set(size_type i, bool v = true) {
        check(i);
        if (v) m_words[i / 64] |= word_type(1) << (i % 64);
        else m_words[i / 64] &= ~(word_type(1) << (i % 64));
        return *this;
    }

    DynamicBitset& set() noexcept {
        std::fill(m_words.begin(), m_words.end(), ~word_type(0));
        trim();
        return *this;
    }

    DynamicBitset& reset(size_type i) { return set(i, false); }

    DynamicBitset& reset() noexcept {
        std::fill(m_words.begin(), m_words.end(), word_type(0));
        return *this;
    }

    DynamicBitset& flip(size_type i) {
        check(i);
        m_words[i / 64] ^= word_type(1) << (i % 64);
        return *this;
    }

    DynamicBitset& flip() noexcept {
        for (auto& w : m_words) w = ~w;
        trim();
        return *this;
    }

    size_type count() const noexcept {
        size_type n = 0;
        for (word_type w : m_words) n += static_cast<size_type>(popcount(w));
        return n;
    }

    bool any() const noexcept {
        for (word_type w : m_words) if (w) return true;
        return false;
    }

    bool none() const noexcept { return !any(); }
    bool all() const noexcept { return count() == m_bits; }

    size_type find_first() const noexcept { return find_next_from(0); }
    size_type find_next(size_type i) const noexcept { return find_next_from(i + 1); }

    DynamicBitset& operator&=(const DynamicBitset& o) {
        same(o);
        for (size_type i = 0; i < m_words.size(); ++i) m_words[i] &= o.m_words[i];
        return *this;
    }

    DynamicBitset& operator|=(const DynamicBitset& o) {
        same(o);
        for (size_type i = 0; i < m_words.size(); ++i) m_words[i] |= o.m_words[i];
        return *this;
    }

    DynamicBitset& operator^=(const DynamicBitset& o) {
        same(o);
        for (size_type i = 0; i < m_words.size(); ++i) m_words[i] ^= o.m_words[i];
        return *this;
    }

    DynamicBitset operator~() const {
        DynamicBitset r = *this;
        return r.flip();
    }

    friend DynamicBitset operator&(DynamicBitset a, const DynamicBitset& b) { return a &= b; }
    friend DynamicBitset operator|(DynamicBitset a, const DynamicBitset& b) { return a |= b; }
    friend DynamicBitset operator^(DynamicBitset a, const DynamicBitset& b) { return a ^= b; }
    friend bool operator==(const DynamicBitset& a, const DynamicBitset& b) { return a.m_bits == b.m_bits && a.m_words == b.m_words; }
    friend bool operator!=(const DynamicBitset& a, const DynamicBitset& b) { return !(a == b); }

    std::string to_string() const {
        std::string s(m_bits, '0');
        for (size_type i = 0; i < m_bits; ++i) if ((*this)[i]) s[m_bits - 1 - i] = '1';
        return s;
    }

    const std::vector<word_type>& words() const noexcept { return m_words; }

    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    std::vector<word_type> m_words;
    size_type              m_bits = 0;

    static int popcount(word_type w) noexcept {
        int n = 0;
        while (w) {
            w &= w - 1;
            ++n;
        }
        return n;
    }

    static int ctz(word_type w) noexcept {
        int n = 0;
        while (!(w & 1)) {
            w >>= 1;
            ++n;
        }
        return n;
    }

    void trim() noexcept {
        if (m_bits % 64 != 0 && !m_words.empty()) m_words.back() &= (word_type(1) << (m_bits % 64)) - 1;
    }

    void check(size_type i) const {
        if (i >= m_bits) throw std::out_of_range("DynamicBitset index out of range");
    }

    void same(const DynamicBitset& o) const {
        if (o.m_bits != m_bits) throw std::invalid_argument("DynamicBitset sizes differ");
    }

    size_type find_next_from(size_type i) const noexcept {
        if (i >= m_bits) return npos;
        size_type w = i / 64;
        word_type cur = m_words[w] & (~word_type(0) << (i % 64));

        for (;;) {
            if (cur) {
                const size_type r = w * 64 + static_cast<size_type>(ctz(cur));
                return r < m_bits ? r : npos;
            }

            if (++w >= m_words.size()) return npos;
            cur = m_words[w];
        }
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_MISC_HPP
