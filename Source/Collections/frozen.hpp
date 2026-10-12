#ifndef FIZMO_COLLECTIONS_FROZEN_HPP
#define FIZMO_COLLECTIONS_FROZEN_HPP

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "hash.hpp"
#include "hash_map.hpp"

namespace fizmo {
namespace collections {

template <class T>
class FrozenList {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using const_iterator         = ConstArrayIterator<T>;
    using iterator               = const_iterator;
    using const_reverse_iterator = ConstReverseArrayIterator<T>;
    using reverse_iterator       = const_reverse_iterator;

    FrozenList() : m_data(empty_storage()) {}
    FrozenList(std::initializer_list<T> init) : FrozenList(std::vector<T>(init)) {}
    explicit FrozenList(std::vector<T> v) : m_data(std::make_shared<const std::vector<T>>(std::move(v))), m_len(m_data->size()) {}

    template <class It, typename std::enable_if<!std::is_integral<It>::value, int>::type = 0>
    FrozenList(It first, It last) : FrozenList(std::vector<T>(first, last)) {}

    size_type size() const noexcept { return m_len; }
    bool empty() const noexcept { return m_len == 0; }
    explicit operator bool() const noexcept { return m_len != 0; }

    const T* data() const noexcept { return m_data->data() + m_off; }
    const_iterator begin() const noexcept { return const_iterator(data()); }
    const_iterator end() const noexcept { return const_iterator(data() + m_len); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(data() + m_len); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(data()); }

    const T& operator[](size_type i) const noexcept { return data()[i]; }

    const T& at(std::ptrdiff_t i) const { return data()[resolve(i)]; }

    const T& front() const { return at(0); }
    const T& back() const { return at(-1); }

    FrozenList slice(std::ptrdiff_t start, std::ptrdiff_t stop, std::ptrdiff_t step = 1) const {
        if (step == 0) throw std::invalid_argument("FrozenList::slice: step cannot be zero");
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(m_len);
        clamp_bounds(start, stop, step, n);

        if (step == 1) {
            FrozenList r = *this;
            r.m_off = m_off + static_cast<size_type>(start);
            r.m_len = stop > start ? static_cast<size_type>(stop - start) : 0;
            return r;
        }

        std::vector<T> out;
        if (step > 0) for (std::ptrdiff_t i = start; i < stop; i += step) out.push_back(data()[i]);
        else for (std::ptrdiff_t i = start; i > stop; i += step) out.push_back(data()[i]);
        return FrozenList(std::move(out));
    }

    FrozenList slice(std::ptrdiff_t start) const { return slice(start, static_cast<std::ptrdiff_t>(m_len)); }

    size_type count(const T& v) const { return static_cast<size_type>(std::count(begin(), end(), v)); }

    size_type index(const T& v) const {
        const T* p = std::find(begin(), end(), v);
        if (p == end()) throw std::out_of_range("FrozenList::index: value not present");
        return static_cast<size_type>(p - begin());
    }

    bool contains(const T& v) const { return std::find(begin(), end(), v) != end(); }

    FrozenList with(std::ptrdiff_t i, T v) const {
        std::vector<T> out = to_vector();
        out[resolve(i)] = std::move(v);
        return FrozenList(std::move(out));
    }

    FrozenList appended(T v) const {
        std::vector<T> out = to_vector();
        out.push_back(std::move(v));
        return FrozenList(std::move(out));
    }

    FrozenList reversed() const { return FrozenList(std::vector<T>(rbegin(), rend())); }

    template <class Compare = std::less<T>>
    FrozenList sorted(Compare cmp = Compare()) const {
        std::vector<T> out = to_vector();
        std::stable_sort(out.begin(), out.end(), cmp);
        return FrozenList(std::move(out));
    }

    std::vector<T> to_vector() const { return std::vector<T>(begin(), end()); }

    friend FrozenList operator+(const FrozenList& a, const FrozenList& b) {
        std::vector<T> out;
        out.reserve(a.size() + b.size());
        out.insert(out.end(), a.begin(), a.end());
        out.insert(out.end(), b.begin(), b.end());
        return FrozenList(std::move(out));
    }

    friend FrozenList operator*(const FrozenList& a, size_type n) {
        std::vector<T> out;
        out.reserve(a.size() * n);
        for (size_type i = 0; i < n; ++i) out.insert(out.end(), a.begin(), a.end());
        return FrozenList(std::move(out));
    }

    friend bool operator==(const FrozenList& a, const FrozenList& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const FrozenList& a, const FrozenList& b) { return !(a == b); }
    friend bool operator<(const FrozenList& a, const FrozenList& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend bool operator>(const FrozenList& a, const FrozenList& b) { return b < a; }
    friend bool operator<=(const FrozenList& a, const FrozenList& b) { return !(b < a); }
    friend bool operator>=(const FrozenList& a, const FrozenList& b) { return !(a < b); }

private:
    std::shared_ptr<const std::vector<T>> m_data;
    size_type                             m_off = 0;
    size_type                             m_len = 0;

    static std::shared_ptr<const std::vector<T>> empty_storage() {
        static const std::shared_ptr<const std::vector<T>> e = std::make_shared<const std::vector<T>>();
        return e;
    }

    size_type resolve(std::ptrdiff_t i) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(m_len);
        if (i < 0) i += n;
        if (i < 0 || i >= n) throw std::out_of_range("FrozenList index out of range");
        return static_cast<size_type>(i);
    }

    static void clamp_bounds(std::ptrdiff_t& start, std::ptrdiff_t& stop, std::ptrdiff_t step, std::ptrdiff_t n) {
        const auto adjust = [n, step](std::ptrdiff_t v) {
            if (v < 0) {
                v += n;
                if (v < 0) v = step < 0 ? -1 : 0;
            } else if (v >= n) {
                v = step < 0 ? n - 1 : n;
            }

            return v;
        };

        start = adjust(start);
        stop = adjust(stop);
    }
};

template <class T>
using ImmutableSequence = FrozenList<T>;

template <class T>
using ImmutableList = FrozenList<T>;

template <class T>
struct Hash<FrozenList<T>> {
    std::size_t operator()(const FrozenList<T>& l) const {
        std::size_t h = l.size();
        for (const auto& v : l) h = hash_combine(h, Hash<T>()(v));
        return h;
    }
};

template <class T, class HashFn = Hash<T>, class KeyEq = std::equal_to<T>>
class FrozenSet {
    using Set = HashSet<T, HashFn, KeyEq>;

public:
    using value_type     = T;
    using size_type      = std::size_t;
    using const_iterator = typename Set::const_iterator;
    using iterator       = const_iterator;

    FrozenSet() : m_set(std::make_shared<const Set>()) { m_hash = compute_hash(*m_set); }
    FrozenSet(std::initializer_list<T> init) : FrozenSet(Set(init)) {}
    explicit FrozenSet(Set s) : m_set(std::make_shared<const Set>(std::move(s))) { m_hash = compute_hash(*m_set); }

    template <class It, typename std::enable_if<!std::is_integral<It>::value, int>::type = 0>
    FrozenSet(It first, It last) : FrozenSet(Set(first, last)) {}

    size_type size() const noexcept { return m_set->size(); }
    bool empty() const noexcept { return m_set->empty(); }
    explicit operator bool() const noexcept { return !empty(); }

    const_iterator begin() const noexcept { return m_set->begin(); }
    const_iterator end() const noexcept { return m_set->end(); }

    bool contains(const T& v) const { return m_set->contains(v); }
    size_type count(const T& v) const { return m_set->count(v); }

    FrozenSet union_with(const FrozenSet& o) const { return FrozenSet(m_set->union_with(*o.m_set)); }
    FrozenSet intersection(const FrozenSet& o) const { return FrozenSet(m_set->intersection(*o.m_set)); }
    FrozenSet difference(const FrozenSet& o) const { return FrozenSet(m_set->difference(*o.m_set)); }
    FrozenSet symmetric_difference(const FrozenSet& o) const { return FrozenSet(m_set->symmetric_difference(*o.m_set)); }
    bool is_subset(const FrozenSet& o) const { return m_set->is_subset(*o.m_set); }
    bool is_superset(const FrozenSet& o) const { return m_set->is_superset(*o.m_set); }
    bool is_disjoint(const FrozenSet& o) const { return m_set->is_disjoint(*o.m_set); }

    FrozenSet with(const T& v) const {
        if (contains(v)) return *this;
        Set s = *m_set;
        s.insert(v);
        return FrozenSet(std::move(s));
    }

    FrozenSet without(const T& v) const {
        if (!contains(v)) return *this;
        Set s = *m_set;
        s.discard(v);
        return FrozenSet(std::move(s));
    }

    const Set& as_set() const noexcept { return *m_set; }
    Set to_set() const { return *m_set; }
    std::vector<T> to_vector() const { return m_set->to_vector(); }
    std::size_t hash() const noexcept { return m_hash; }

    friend bool operator==(const FrozenSet& a, const FrozenSet& b) { return a.m_set == b.m_set || (a.m_hash == b.m_hash && *a.m_set == *b.m_set); }
    friend bool operator!=(const FrozenSet& a, const FrozenSet& b) { return !(a == b); }
    friend bool operator<=(const FrozenSet& a, const FrozenSet& b) { return a.is_subset(b); }
    friend bool operator>=(const FrozenSet& a, const FrozenSet& b) { return b.is_subset(a); }
    friend bool operator<(const FrozenSet& a, const FrozenSet& b) { return a.size() < b.size() && a.is_subset(b); }
    friend bool operator>(const FrozenSet& a, const FrozenSet& b) { return b < a; }
    friend FrozenSet operator|(const FrozenSet& a, const FrozenSet& b) { return a.union_with(b); }
    friend FrozenSet operator&(const FrozenSet& a, const FrozenSet& b) { return a.intersection(b); }
    friend FrozenSet operator-(const FrozenSet& a, const FrozenSet& b) { return a.difference(b); }
    friend FrozenSet operator^(const FrozenSet& a, const FrozenSet& b) { return a.symmetric_difference(b); }

private:
    std::shared_ptr<const Set> m_set;
    std::size_t                m_hash = 0;

    static std::size_t compute_hash(const Set& s) { return Hash<Set>()(s); }
};

template <class T>
using ImmutableSet = FrozenSet<T>;

template <class T, class H, class E>
struct Hash<FrozenSet<T, H, E>> {
    std::size_t operator()(const FrozenSet<T, H, E>& s) const noexcept { return s.hash(); }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_FROZEN_HPP
