#ifndef FIZMO_ARRAYS_WHERE_PROXY_HPP
#define FIZMO_ARRAYS_WHERE_PROXY_HPP

#include "ops/functional_ops.hpp"

namespace fizmo {

template <typename Container>
class BasicWhereProxy {
public:
    using container_type = typename std::remove_const<Container>::type;
    using value_type = typename container_type::value_type;
    using element_type = typename std::conditional<std::is_const<Container>::value, const value_type, value_type>::type;
    using index_list = DynamicArray<std::size_t>;

    class iterator {
        element_type*      m_base;
        const std::size_t* m_idx;

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename BasicWhereProxy::value_type;
        using reference = element_type&;
        using pointer = element_type*;
        using iterator_category = std::random_access_iterator_tag;

        iterator(element_type* base, const std::size_t* idx) noexcept : m_base(base), m_idx(idx) {}
        reference operator*() const noexcept { return m_base[*m_idx]; }
        pointer operator->() const noexcept { return m_base + *m_idx; }
        reference operator[](difference_type n) const noexcept { return m_base[m_idx[n]]; }
        iterator& operator++() noexcept { ++m_idx; return *this; }
        iterator operator++(int) noexcept { iterator t(*this); ++m_idx; return t; }
        iterator& operator--() noexcept { --m_idx; return *this; }
        iterator operator--(int) noexcept { iterator t(*this); --m_idx; return t; }
        iterator& operator+=(difference_type n) noexcept { m_idx += n; return *this; }
        iterator& operator-=(difference_type n) noexcept { m_idx -= n; return *this; }
        friend iterator operator+(iterator it, difference_type n) noexcept { it += n; return it; }
        friend iterator operator-(iterator it, difference_type n) noexcept { it -= n; return it; }
        friend difference_type operator-(const iterator& a, const iterator& b) noexcept { return a.m_idx - b.m_idx; }
        friend bool operator==(const iterator& a, const iterator& b) noexcept { return a.m_idx == b.m_idx; }
        friend bool operator!=(const iterator& a, const iterator& b) noexcept { return a.m_idx != b.m_idx; }
        friend bool operator<(const iterator& a, const iterator& b) noexcept { return a.m_idx < b.m_idx; }
        std::size_t index() const noexcept { return *m_idx; }
    };

private:
    Container& m_arr;
    index_list m_indices;

    element_type* base() const noexcept { return m_arr.data(); }

    void normalize() {
        if (!m_indices.is_sorted()) m_indices.sort();
        m_indices.unique();
        while (!m_indices.empty() && m_indices.back() >= m_arr.size()) m_indices.remove_back();
    }

    template <typename Keep>
    BasicWhereProxy& keep_positions(Keep keep) {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) if (keep(i, m_indices[i])) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

    container_type rebuild_with_inserts(const value_type& value, bool after) {
        container_type out;
        out.reserve(m_arr.size() + m_indices.size());
        std::size_t j = 0;
        value_type* d = m_arr.data();
        for (std::size_t i = 0; i < m_arr.size(); ++i) {
            const bool hit = j < m_indices.size() && m_indices[j] == i;
            if (hit && !after) out.push_back(value);
            out.push_back(static_cast<value_type&&>(d[i]));
            if (hit && after) out.push_back(value);
            if (hit) ++j;
        }
        return out;
    }

public:
    BasicWhereProxy(Container& arr, index_list&& indices) : m_arr(arr), m_indices(static_cast<index_list&&>(indices)) { normalize(); }
    BasicWhereProxy(Container& arr, const index_list& indices) : m_arr(arr), m_indices(indices) { normalize(); }

    Container& array() const noexcept { return m_arr; }

    iterator begin() const noexcept { return iterator(base(), m_indices.data()); }
    iterator end() const noexcept { return iterator(base(), m_indices.data() + m_indices.size()); }

    std::size_t count() const noexcept { return m_indices.size(); }
    std::size_t size() const noexcept { return m_indices.size(); }
    bool empty() const noexcept { return m_indices.empty(); }
    explicit operator bool() const noexcept { return !m_indices.empty(); }
    std::size_t index(std::size_t n) const noexcept { return m_indices[n]; }
    element_type& operator[](std::size_t n) const noexcept { return base()[m_indices[n]]; }
    std::size_t first_index() const noexcept { return m_indices.empty() ? npos : m_indices.front(); }
    std::size_t last_index() const noexcept { return m_indices.empty() ? npos : m_indices.back(); }
    bool contains_index(std::size_t array_index) const noexcept { return m_indices.binary_search(array_index); }

    ArrayView<std::size_t> indices() const noexcept { return ArrayView<std::size_t>(m_indices.data(), m_indices.size()); }
    index_list indices_copy() const { return index_list(m_indices); }

    index_list release_indices() noexcept {
        index_list out = static_cast<index_list&&>(m_indices);
        m_indices.deallocate();
        return out;
    }

    index_list extract_indices() noexcept {
        index_list out = static_cast<index_list&&>(m_indices);
        m_indices.clear();
        return out;
    }

    BasicWhereProxy& first(std::size_t n = 1) { if (n < m_indices.size()) m_indices.resize(n); return *this; }
    BasicWhereProxy& take(std::size_t n) { return first(n); }

    BasicWhereProxy& last(std::size_t n = 1) {
        if (n < m_indices.size()) m_indices = m_indices.slice(m_indices.size() - n, m_indices.size());
        return *this;
    }

    BasicWhereProxy& range(std::size_t start, std::size_t end_idx) { return keep_positions([&](std::size_t, std::size_t ai) { return ai >= start && ai < end_idx; }); }

    BasicWhereProxy& skip(std::size_t n = 1) {
        if (n >= m_indices.size()) m_indices.clear();
        else m_indices.remove_front(n);
        return *this;
    }

    BasicWhereProxy& skip_last(std::size_t n = 1) {
        m_indices.remove_back(n);
        return *this;
    }

    BasicWhereProxy& at(std::size_t n) {
        if (n < m_indices.size()) { m_indices[0] = m_indices[n]; m_indices.resize(1); }
        else m_indices.clear();
        return *this;
    }

    BasicWhereProxy& slice(std::size_t start, std::size_t end_idx) {
        if (start >= m_indices.size()) { m_indices.clear(); return *this; }
        if (end_idx > m_indices.size()) end_idx = m_indices.size();
        if (start >= end_idx) { m_indices.clear(); return *this; }
        m_indices = m_indices.slice(start, end_idx);
        return *this;
    }

    template <typename Pred>
    BasicWhereProxy& where(Pred pred) { return keep_positions([&](std::size_t, std::size_t ai) { return static_cast<bool>(pred(base()[ai])); }); }

    BasicWhereProxy& where(const value_type& value) {
        const value_type v(value);
        return keep_positions([&](std::size_t, std::size_t ai) { return base()[ai] == v; });
    }

    template <typename Pred>
    BasicWhereProxy& where_not(Pred pred) { return keep_positions([&](std::size_t, std::size_t ai) { return !pred(base()[ai]); }); }

    template <typename Pred>
    BasicWhereProxy& where_indexed(Pred pred) { return keep_positions([&](std::size_t, std::size_t ai) { return static_cast<bool>(pred(ai, base()[ai])); }); }

    template <typename Pred>
    BasicWhereProxy& or_where(Pred pred) {
        index_list merged;
        merged.reserve(m_arr.size());
        std::size_t j = 0;
        for (std::size_t i = 0; i < m_arr.size(); ++i) {
            const bool had = j < m_indices.size() && m_indices[j] == i;
            if (had) ++j;
            if (had || pred(base()[i])) merged.push_back(i);
        }
        m_indices = static_cast<index_list&&>(merged);
        return *this;
    }

    BasicWhereProxy& invert() {
        index_list out;
        out.reserve(m_arr.size() - m_indices.size());
        std::size_t j = 0;
        for (std::size_t i = 0; i < m_arr.size(); ++i) {
            if (j < m_indices.size() && m_indices[j] == i) { ++j; continue; }
            out.push_back(i);
        }
        m_indices = static_cast<index_list&&>(out);
        return *this;
    }

    BasicWhereProxy& union_with(const BasicWhereProxy& other) {
        index_list out;
        out.reserve(m_indices.size() + other.m_indices.size());
        std::size_t a = 0, b = 0;
        while (a < m_indices.size() || b < other.m_indices.size()) {
            if (b >= other.m_indices.size() || (a < m_indices.size() && m_indices[a] < other.m_indices[b])) out.push_back(m_indices[a++]);
            else if (a >= m_indices.size() || other.m_indices[b] < m_indices[a]) out.push_back(other.m_indices[b++]);
            else { out.push_back(m_indices[a++]); ++b; }
        }
        m_indices = static_cast<index_list&&>(out);
        return *this;
    }

    BasicWhereProxy& intersect_with(const BasicWhereProxy& other) {
        return keep_positions([&](std::size_t, std::size_t ai) { return other.contains_index(ai); });
    }

    BasicWhereProxy& even() { return keep_positions([](std::size_t pos, std::size_t) { return pos % 2 == 0; }); }
    BasicWhereProxy& odd() { return keep_positions([](std::size_t pos, std::size_t) { return pos % 2 == 1; }); }

    BasicWhereProxy& step(std::size_t n) {
        if (n <= 1) return *this;
        return keep_positions([n](std::size_t pos, std::size_t) { return pos % n == 0; });
    }

    BasicWhereProxy& except(const index_list& positions) {
        index_list sorted_positions(positions);
        sorted_positions.sort();
        return keep_positions([&](std::size_t pos, std::size_t) { return !sorted_positions.binary_search(pos); });
    }

    container_type values() const {
        container_type result;
        result.reserve(m_indices.size());
        for (std::size_t i = 0; i < m_indices.size(); ++i) result.push_back(base()[m_indices[i]]);
        return result;
    }

    container_type to_array() const { return values(); }

    element_type* first_value() const noexcept { return m_indices.empty() ? nullptr : &base()[m_indices.front()]; }
    element_type* last_value() const noexcept { return m_indices.empty() ? nullptr : &base()[m_indices.back()]; }

    template <typename Func>
    BasicWhereProxy& for_each(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) func(base()[m_indices[i]]);
        return *this;
    }

    template <typename Func>
    BasicWhereProxy& for_each_indexed(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) func(m_indices[i], base()[m_indices[i]]);
        return *this;
    }

    template <typename Func>
    BasicWhereProxy& transform(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) func(base()[m_indices[i]]);
        return *this;
    }

    template <typename Func>
    BasicWhereProxy& apply(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) { element_type& e = base()[m_indices[i]]; e = func(e); }
        return *this;
    }

    template <typename Func>
    auto map(Func func) const -> DynamicArray<typename std::decay<decltype(func(std::declval<const value_type&>()))>::type> {
        DynamicArray<typename std::decay<decltype(func(std::declval<const value_type&>()))>::type> out;
        out.reserve(m_indices.size());
        for (std::size_t i = 0; i < m_indices.size(); ++i) out.emplace_back(func(base()[m_indices[i]]));
        return out;
    }

    template <typename Pred>
    std::size_t count_if(Pred pred) const {
        std::size_t c = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) if (pred(base()[m_indices[i]])) ++c;
        return c;
    }

    template <typename Pred>
    bool all_of(Pred pred) const { return count_if(pred) == m_indices.size(); }
    template <typename Pred>
    bool any_of(Pred pred) const { for (std::size_t i = 0; i < m_indices.size(); ++i) if (pred(base()[m_indices[i]])) return true; return false; }
    template <typename Pred>
    bool none_of(Pred pred) const { return !any_of(pred); }

    template <typename U = value_type>
    typename std::enable_if<arrays::detail::has_plus<U>::value, U>::type sum() const {
        if (m_indices.empty()) return U{};
        U acc = base()[m_indices[0]];
        for (std::size_t i = 1; i < m_indices.size(); ++i) acc = acc + base()[m_indices[i]];
        return acc;
    }

    template <typename Compare = arrays::Less>
    std::size_t min_index(Compare comp = Compare()) const {
        if (m_indices.empty()) return npos;
        std::size_t best = m_indices[0];
        for (std::size_t i = 1; i < m_indices.size(); ++i) if (comp(base()[m_indices[i]], base()[best])) best = m_indices[i];
        return best;
    }

    template <typename Compare = arrays::Less>
    std::size_t max_index(Compare comp = Compare()) const {
        if (m_indices.empty()) return npos;
        std::size_t best = m_indices[0];
        for (std::size_t i = 1; i < m_indices.size(); ++i) if (comp(base()[best], base()[m_indices[i]])) best = m_indices[i];
        return best;
    }

    element_type* min_value() const noexcept { const std::size_t i = min_index(); return i == npos ? nullptr : &base()[i]; }
    element_type* max_value() const noexcept { const std::size_t i = max_index(); return i == npos ? nullptr : &base()[i]; }

    std::size_t remove() {
        const std::size_t n = m_indices.size();
        m_arr.remove_at(m_indices.data(), n);
        return n;
    }

    std::size_t remove_unordered() {
        const std::size_t n = m_indices.size();
        m_arr.remove_at_unordered(m_indices.data(), n);
        return n;
    }

    container_type pop() { return m_arr.pop_at(m_indices.data(), m_indices.size()); }
    container_type extract() { return pop(); }
    container_type pop_unordered() { return m_arr.pop_at_unordered(m_indices.data(), m_indices.size()); }

    void replace(const value_type& value) { m_arr.replace_at(m_indices.data(), m_indices.size(), value); }
    void fill(const value_type& value) { replace(value); }

    template <typename U>
    typename std::enable_if<are_compatible_types<value_type, U> && !std::is_same<typename std::decay<U>::type, value_type>::value>::type
    replace(U&& value) {
        const value_type converted = static_cast<value_type>(static_cast<U&&>(value));
        replace(converted);
    }

    container_type swap_with(const value_type& value) {
        const value_type v(value);
        container_type old_values;
        old_values.reserve(m_indices.size());
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            old_values.push_back(static_cast<value_type&&>(base()[m_indices[i]]));
            base()[m_indices[i]] = v;
        }
        return old_values;
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<value_type, U> && !std::is_same<typename std::decay<U>::type, value_type>::value, container_type>::type
    swap_with(U&& value) {
        const value_type converted = static_cast<value_type>(static_cast<U&&>(value));
        return swap_with(converted);
    }

    BasicWhereProxy& shuffle() {
        const std::size_t n = m_indices.size();
        for (std::size_t i = n > 0 ? n - 1 : 0; i > 0; --i) {
            const std::size_t j = random_int<std::size_t>(0, i);
            if (j != i) arrays::detail::swap_values(base()[m_indices[i]], base()[m_indices[j]]);
        }
        return *this;
    }

    BasicWhereProxy& sort() { return sort(arrays::Less{}); }

    template <typename Compare>
    BasicWhereProxy& sort(Compare comp) { return gather_apply([&](container_type& tmp) { tmp.sort(comp); }); }

    BasicWhereProxy& stable_sort() { return stable_sort(arrays::Less{}); }

    template <typename Compare>
    BasicWhereProxy& stable_sort(Compare comp) { return gather_apply([&](container_type& tmp) { tmp.stable_sort(comp); }); }

    template <typename Key>
    BasicWhereProxy& sort_by(Key key) { return gather_apply([&](container_type& tmp) { tmp.sort_by(key); }); }

    BasicWhereProxy& rotate_left(std::size_t n = 1) { return gather_apply([&](container_type& tmp) { tmp.rotate_left(n); }); }
    BasicWhereProxy& rotate_right(std::size_t n = 1) { return gather_apply([&](container_type& tmp) { tmp.rotate_right(n); }); }

    BasicWhereProxy& reverse() {
        const std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        std::size_t lo = 0, hi = n - 1;
        while (lo < hi) { arrays::detail::swap_values(base()[m_indices[lo]], base()[m_indices[hi]]); ++lo; --hi; }
        return *this;
    }

    template <typename Dest>
    void copy_to(Dest& dest) const {
        dest.reserve(dest.size() + m_indices.size());
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.push_back(base()[m_indices[i]]);
    }

    template <typename Dest>
    void copy_to(Dest& dest, std::size_t dst_index) const { dest.insert(dst_index, values()); }

    template <typename Dest>
    void move_to(Dest& dest) {
        const std::size_t n = m_indices.size();
        if (n == 0) return;
        dest.reserve(dest.size() + n);
        container_type moved = m_arr.pop_at(m_indices.data(), n);
        for (std::size_t i = 0; i < moved.size(); ++i) dest.push_back(static_cast<value_type&&>(moved[i]));
    }

    template <typename Dest>
    void move_to(Dest& dest, std::size_t dst_index) {
        container_type moved = m_arr.pop_at(m_indices.data(), m_indices.size());
        dest.insert(dst_index, static_cast<container_type&&>(moved));
    }

    void insert_before(const value_type& value) {
        if (m_indices.empty()) return;
        const value_type v(value);
        container_type rebuilt = rebuild_with_inserts(v, false);
        m_arr = static_cast<container_type&&>(rebuilt);
    }

    void insert_after(const value_type& value) {
        if (m_indices.empty()) return;
        const value_type v(value);
        container_type rebuilt = rebuild_with_inserts(v, true);
        m_arr = static_cast<container_type&&>(rebuilt);
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<value_type, U> && !std::is_same<typename std::decay<U>::type, value_type>::value>::type
    insert_before(U&& value) { insert_before(static_cast<value_type>(static_cast<U&&>(value))); }

    template <typename U>
    typename std::enable_if<are_compatible_types<value_type, U> && !std::is_same<typename std::decay<U>::type, value_type>::value>::type
    insert_after(U&& value) { insert_after(static_cast<value_type>(static_cast<U&&>(value))); }

    void insert_unordered(const value_type& value) {
        if (m_indices.empty()) return;
        const value_type v(value);
        for (std::size_t i = 0; i < m_indices.size(); ++i) m_arr.insert_unordered(m_indices[i], v);
    }

private:
    template <typename Fn>
    BasicWhereProxy& gather_apply(Fn fn) {
        const std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        container_type tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) tmp.push_back(static_cast<value_type&&>(base()[m_indices[i]]));
        fn(tmp);
        for (std::size_t i = 0; i < n; ++i) base()[m_indices[i]] = static_cast<value_type&&>(tmp[i]);
        return *this;
    }
};

namespace arrays {
namespace detail {

template <typename Derived, typename T, typename Base>
inline BasicWhereProxy<Derived> FunctionalOps<Derived, T, Base>::where(const T& value) {
    return BasicWhereProxy<Derived>(self(), this->find_indices(value));
}

template <typename Derived, typename T, typename Base>
template <typename Pred>
inline BasicWhereProxy<Derived> FunctionalOps<Derived, T, Base>::where(Pred pred) {
    return BasicWhereProxy<Derived>(self(), this->find_indices_if(pred));
}

template <typename Derived, typename T, typename Base>
inline BasicWhereProxy<Derived> FunctionalOps<Derived, T, Base>::where_indices(index_array_t<T> indices) {
    return BasicWhereProxy<Derived>(self(), static_cast<index_array_t<T>&&>(indices));
}

template <typename Derived, typename T, typename Base>
inline BasicWhereProxy<const Derived> FunctionalOps<Derived, T, Base>::where(const T& value) const {
    return BasicWhereProxy<const Derived>(cself(), this->find_indices(value));
}

template <typename Derived, typename T, typename Base>
template <typename Pred>
inline BasicWhereProxy<const Derived> FunctionalOps<Derived, T, Base>::where(Pred pred) const {
    return BasicWhereProxy<const Derived>(cself(), this->find_indices_if(pred));
}

} // namespace detail
} // namespace arrays

template <typename T>
using WhereProxy = BasicWhereProxy<DynamicArray<T>>;

template <typename T, std::size_t N>
using SmallVectorWhereProxy = BasicWhereProxy<SmallVector<T, N>>;

} // namespace fizmo

#endif // FIZMO_ARRAYS_WHERE_PROXY_HPP
