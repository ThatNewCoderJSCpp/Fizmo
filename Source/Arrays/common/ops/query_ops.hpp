#ifndef FIZMO_ARRAYS_OPS_QUERY_OPS_HPP
#define FIZMO_ARRAYS_OPS_QUERY_OPS_HPP

#include "../algorithms/search.hpp"
#include "../algorithms/sort.hpp"
#include "../algorithms/hash.hpp"
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace fizmo {
namespace arrays {
namespace detail {

FIZMO_ARRAY_INLINE bool clamp_range(std::size_t& start, std::size_t& end_idx, std::size_t n) noexcept {
    if (start >= n || start >= end_idx) return false;
    if (end_idx > n) end_idx = n;
    return true;
}

template <typename T>
struct IndexArrayOf {
    using type = DynamicArray<std::size_t>;
};

template <typename T>
using index_array_t = typename IndexArrayOf<T>::type;

template <typename T>
struct FrequencyArrayOf {
    using type = DynamicArray<std::pair<typename std::remove_const<T>::type, std::size_t>>;
};

template <typename C, typename T, typename = void>
struct is_contiguous_of : std::false_type {};

template <typename C, typename T>
struct is_contiguous_of<C, T, void_t<decltype(std::declval<const C&>().data()), decltype(std::declval<const C&>().size())>>
    : std::integral_constant<bool, std::is_convertible<decltype(std::declval<const C&>().data()), const T*>::value> {};

template <typename Derived, typename T, typename Base>
class QueryOps : public Base {
protected:
    const T* q_ptr() const noexcept { return this->fz_cptr(); }
    std::size_t q_len() const noexcept { return this->fz_len(); }

    template <typename C>
    static const T* seq_data(const C& c) noexcept { return c.data(); }
    template <typename C>
    static std::size_t seq_size(const C& c) noexcept { return static_cast<std::size_t>(c.size()); }
    static const T* seq_data(std::initializer_list<T> c) noexcept { return c.begin(); }
    static std::size_t seq_size(std::initializer_list<T> c) noexcept { return c.size(); }

public:
    std::size_t find(const T& value) const noexcept {
        const std::size_t i = simd::find(q_ptr(), q_len(), value);
        return i < q_len() ? i : npos;
    }

    std::size_t find(const T& value, std::size_t start) const noexcept {
        if (start >= q_len()) return npos;
        const std::size_t i = simd::find(q_ptr() + start, q_len() - start, value);
        return i < q_len() - start ? start + i : npos;
    }

    std::size_t find(const T& value, std::size_t start, std::size_t end_idx) const noexcept {
        if (!clamp_range(start, end_idx, q_len())) return npos;
        const std::size_t i = simd::find(q_ptr() + start, end_idx - start, value);
        return i < end_idx - start ? start + i : npos;
    }

    template <typename Pred>
    std::size_t find_if(Pred pred) const { return find_if(pred, 0); }

    template <typename Pred>
    std::size_t find_if(Pred pred, std::size_t start) const {
        const T* d = q_ptr();
        for (std::size_t i = start, n = q_len(); i < n; ++i) if (pred(d[i])) return i;
        return npos;
    }

    std::size_t find_last(const T& value) const noexcept {
        const std::size_t i = simd::rfind(q_ptr(), q_len(), value);
        return i < q_len() ? i : npos;
    }

    std::size_t find_last(const T& value, std::size_t before) const noexcept {
        if (before > q_len()) before = q_len();
        const std::size_t i = simd::rfind(q_ptr(), before, value);
        return i < before ? i : npos;
    }

    template <typename Pred>
    std::size_t find_last_if(Pred pred) const { return find_last_if(pred, q_len()); }

    template <typename Pred>
    std::size_t find_last_if(Pred pred, std::size_t before) const {
        if (before > q_len()) before = q_len();
        const T* d = q_ptr();
        for (std::size_t i = before; i > 0; --i) if (pred(d[i - 1])) return i - 1;
        return npos;
    }

    std::size_t index_of(const T& value) const noexcept { return find(value); }

    std::size_t count_of(const T& value) const noexcept { return simd::count(q_ptr(), q_len(), value); }

    std::size_t count_of(const T& value, std::size_t start, std::size_t end_idx) const noexcept {
        if (!clamp_range(start, end_idx, q_len())) return 0;
        return simd::count(q_ptr() + start, end_idx - start, value);
    }

    template <typename Pred>
    std::size_t count_if(Pred pred) const {
        std::size_t c = 0;
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) if (pred(d[i])) ++c;
        return c;
    }

    template <typename Pred>
    std::size_t count_if(Pred pred, std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, q_len())) return 0;
        std::size_t c = 0;
        const T* d = q_ptr();
        for (std::size_t i = start; i < end_idx; ++i) if (pred(d[i])) ++c;
        return c;
    }

    bool contains(const T& value) const noexcept { return find(value) != npos; }
    bool contains(const T& value, std::size_t start, std::size_t end_idx) const noexcept { return find(value, start, end_idx) != npos; }

    template <typename Pred>
    bool contains_if(Pred pred) const { return find_if(pred) != npos; }

    index_array_t<T> find_indices(const T& value) const {
        index_array_t<T> idx;
        const T* d = q_ptr();
        const std::size_t n = q_len();
        std::size_t at = 0;
        while (at < n) {
            const std::size_t f = simd::find(d + at, n - at, value);
            if (f >= n - at) break;
            idx.push_back(at + f);
            at += f + 1;
        }
        return idx;
    }

    template <typename Pred>
    index_array_t<T> find_indices_if(Pred pred) const {
        index_array_t<T> idx;
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) if (pred(d[i])) idx.push_back(i);
        return idx;
    }

    template <typename Pred>
    bool all_of(Pred pred) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) if (!pred(d[i])) return false;
        return true;
    }

    template <typename Pred>
    bool any_of(Pred pred) const { return find_if(pred) != npos; }

    template <typename Pred>
    bool none_of(Pred pred) const { return find_if(pred) == npos; }

    const T& min() const noexcept { return q_ptr()[min_element_index()]; }
    template <typename Compare>
    const T& min(Compare comp) const { return q_ptr()[min_element_index(comp)]; }
    const T& max() const noexcept { return q_ptr()[max_element_index()]; }
    template <typename Compare>
    const T& max(Compare comp) const { return q_ptr()[max_element_index(comp)]; }

    std::pair<const T&, const T&> min_max() const noexcept {
        const std::pair<std::size_t, std::size_t> p = min_max_index();
        return { q_ptr()[p.first], q_ptr()[p.second] };
    }

    template <typename Compare>
    std::pair<const T&, const T&> min_max(Compare comp) const {
        const std::pair<std::size_t, std::size_t> p = min_max_index(comp);
        return { q_ptr()[p.first], q_ptr()[p.second] };
    }

    std::size_t min_element_index() const noexcept { return min_element_index(std::size_t(0), q_len(), Less{}); }
    template <typename Compare>
    std::size_t min_element_index(Compare comp) const { return min_element_index(std::size_t(0), q_len(), comp); }
    std::size_t min_element_index(std::size_t start, std::size_t end_idx) const noexcept { return min_element_index(start, end_idx, Less{}); }

    template <typename Compare>
    std::size_t min_element_index(std::size_t start, std::size_t end_idx, Compare comp) const {
        if (!clamp_range(start, end_idx, q_len())) return start < q_len() ? start : 0;
        const T* d = q_ptr();
        std::size_t idx = start;
        for (std::size_t i = start + 1; i < end_idx; ++i) if (comp(d[i], d[idx])) idx = i;
        return idx;
    }

    std::size_t max_element_index() const noexcept { return max_element_index(std::size_t(0), q_len(), Less{}); }
    template <typename Compare>
    std::size_t max_element_index(Compare comp) const { return max_element_index(std::size_t(0), q_len(), comp); }
    std::size_t max_element_index(std::size_t start, std::size_t end_idx) const noexcept { return max_element_index(start, end_idx, Less{}); }

    template <typename Compare>
    std::size_t max_element_index(std::size_t start, std::size_t end_idx, Compare comp) const {
        if (!clamp_range(start, end_idx, q_len())) return start < q_len() ? start : 0;
        const T* d = q_ptr();
        std::size_t idx = start;
        for (std::size_t i = start + 1; i < end_idx; ++i) if (comp(d[idx], d[i])) idx = i;
        return idx;
    }

    std::pair<std::size_t, std::size_t> min_max_index() const noexcept { return min_max_index(Less{}); }

    template <typename Compare>
    std::pair<std::size_t, std::size_t> min_max_index(Compare comp) const {
        const T* d = q_ptr();
        std::size_t lo = 0, hi = 0;
        for (std::size_t i = 1, n = q_len(); i < n; ++i) {
            if (comp(d[i], d[lo])) lo = i;
            if (comp(d[hi], d[i])) hi = i;
        }
        return { lo, hi };
    }

    bool is_sorted() const { return algo::is_sorted(q_ptr(), q_len(), Less{}); }
    template <typename Compare>
    bool is_sorted(Compare comp) const { return algo::is_sorted(q_ptr(), q_len(), comp); }
    bool is_sorted(std::size_t start, std::size_t end_idx) const { return is_sorted(start, end_idx, Less{}); }
    template <typename Compare>
    bool is_sorted(std::size_t start, std::size_t end_idx, Compare comp) const {
        if (!clamp_range(start, end_idx, q_len())) return true;
        return algo::is_sorted(q_ptr() + start, end_idx - start, comp);
    }
    std::size_t is_sorted_until() const { return algo::is_sorted_until(q_ptr(), q_len(), Less{}); }
    template <typename Compare>
    std::size_t is_sorted_until(Compare comp) const { return algo::is_sorted_until(q_ptr(), q_len(), comp); }

    bool binary_search(const T& value) const { return algo::binary_search(q_ptr(), q_len(), value, Less{}); }
    template <typename Compare>
    bool binary_search(const T& value, Compare comp) const { return algo::binary_search(q_ptr(), q_len(), value, comp); }
    bool binary_search(const T& value, std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, q_len())) return false;
        return algo::binary_search(q_ptr() + start, end_idx - start, value, Less{});
    }

    std::size_t lower_bound(const T& value) const { return algo::lower_bound(q_ptr(), q_len(), value, Less{}); }
    template <typename Compare>
    std::size_t lower_bound(const T& value, Compare comp) const { return algo::lower_bound(q_ptr(), q_len(), value, comp); }
    std::size_t lower_bound(const T& value, std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, q_len())) return start < q_len() ? start : q_len();
        return start + algo::lower_bound(q_ptr() + start, end_idx - start, value, Less{});
    }

    std::size_t upper_bound(const T& value) const { return algo::upper_bound(q_ptr(), q_len(), value, Less{}); }
    template <typename Compare>
    std::size_t upper_bound(const T& value, Compare comp) const { return algo::upper_bound(q_ptr(), q_len(), value, comp); }
    std::size_t upper_bound(const T& value, std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, q_len())) return start < q_len() ? start : q_len();
        return start + algo::upper_bound(q_ptr() + start, end_idx - start, value, Less{});
    }

    std::pair<std::size_t, std::size_t> equal_range(const T& value) const { return { lower_bound(value), upper_bound(value) }; }
    template <typename Compare>
    std::pair<std::size_t, std::size_t> equal_range(const T& value, Compare comp) const { return { lower_bound(value, comp), upper_bound(value, comp) }; }

    std::size_t fibonacci_search(const T& value) const { return algo::fibonacci_search(q_ptr(), q_len(), value, Less{}); }
    template <typename Compare>
    std::size_t fibonacci_search(const T& value, Compare comp) const { return algo::fibonacci_search(q_ptr(), q_len(), value, comp); }
    std::size_t fibonacci_search(const T& value, std::size_t start, std::size_t end_idx) const { return fibonacci_search(value, start, end_idx, Less{}); }
    template <typename Compare>
    std::size_t fibonacci_search(const T& value, std::size_t start, std::size_t end_idx, Compare comp) const {
        if (!clamp_range(start, end_idx, q_len())) return npos;
        const std::size_t i = algo::fibonacci_search(q_ptr() + start, end_idx - start, value, comp);
        return i == npos ? npos : start + i;
    }

    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, std::size_t>::type
    interpolation_search(const T& value) const { return algo::interpolation_search(q_ptr(), q_len(), value); }

    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, std::size_t>::type
    interpolation_search(const T& value, std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, q_len())) return npos;
        const std::size_t i = algo::interpolation_search(q_ptr() + start, end_idx - start, value);
        return i == npos ? npos : start + i;
    }

    std::size_t exponential_search(const T& value) const { return algo::exponential_search(q_ptr(), q_len(), value, Less{}); }
    template <typename Compare>
    std::size_t exponential_search(const T& value, Compare comp) const { return algo::exponential_search(q_ptr(), q_len(), value, comp); }

    template <typename Acc, typename Pred, typename Op>
    Acc accumulate_if(Acc init, Pred pred, Op op) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) if (pred(d[i])) init = op(static_cast<Acc&&>(init), d[i]);
        return init;
    }

    template <typename Acc, typename Op>
    Acc accumulate(Acc init, Op op) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) init = op(static_cast<Acc&&>(init), d[i]);
        return init;
    }

    template <typename Acc>
    Acc accumulate(Acc init) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) init = init + d[i];
        return init;
    }

    template <typename U, typename Func>
    U reduce(U init, Func func) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) init = func(static_cast<U&&>(init), d[i]);
        return init;
    }

    template <typename Func>
    T reduce(Func func) const {
        if (q_len() == 0) return empty_value("reduce");
        const T* d = q_ptr();
        T acc = d[0];
        for (std::size_t i = 1, n = q_len(); i < n; ++i) acc = func(static_cast<T&&>(acc), d[i]);
        return acc;
    }

    template <typename U, typename Func>
    U fold_right(U init, Func func) const {
        const T* d = q_ptr();
        for (std::size_t i = q_len(); i > 0; --i) init = func(d[i - 1], static_cast<U&&>(init));
        return init;
    }

    template <typename Func>
    T fold_right(Func func) const {
        if (q_len() == 0) return empty_value("fold_right");
        const T* d = q_ptr();
        T acc = d[q_len() - 1];
        for (std::size_t i = q_len() - 1; i > 0; --i) acc = func(d[i - 1], static_cast<T&&>(acc));
        return acc;
    }

    template <typename U = T>
    typename std::enable_if<detail::has_plus<U>::value, U>::type sum() const { return sum(std::size_t(0), q_len()); }

    template <typename U = T>
    typename std::enable_if<detail::has_plus<U>::value, U>::type sum(std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, q_len())) return U{};
        const T* d = q_ptr();
        U acc = d[start];
        for (std::size_t i = start + 1; i < end_idx; ++i) acc = acc + d[i];
        return acc;
    }

    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, U>::type product() const {
        U acc = U(1);
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) acc = static_cast<U>(acc * d[i]);
        return acc;
    }

    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, double>::type mean() const {
        if (q_len() == 0) return 0.0;
        long double acc = 0.0L;
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) acc += static_cast<long double>(d[i]);
        return static_cast<double>(acc / static_cast<long double>(q_len()));
    }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, bool>::type starts_with(const C& prefix) const {
        const std::size_t m = seq_size(prefix);
        return m <= q_len() && equal_n(q_ptr(), seq_data(prefix), m);
    }
    bool starts_with(std::initializer_list<T> prefix) const { return prefix.size() <= q_len() && equal_n(q_ptr(), prefix.begin(), prefix.size()); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, bool>::type ends_with(const C& suffix) const {
        const std::size_t m = seq_size(suffix);
        return m <= q_len() && equal_n(q_ptr() + (q_len() - m), seq_data(suffix), m);
    }
    bool ends_with(std::initializer_list<T> suffix) const { return suffix.size() <= q_len() && equal_n(q_ptr() + (q_len() - suffix.size()), suffix.begin(), suffix.size()); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, std::size_t>::type find_subsequence(const C& needle) const {
        return algo::find_subsequence(q_ptr(), q_len(), seq_data(needle), seq_size(needle));
    }
    std::size_t find_subsequence(std::initializer_list<T> needle) const { return algo::find_subsequence(q_ptr(), q_len(), needle.begin(), needle.size()); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, std::size_t>::type find_last_subsequence(const C& needle) const {
        return algo::rfind_subsequence(q_ptr(), q_len(), seq_data(needle), seq_size(needle));
    }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, bool>::type contains_subsequence(const C& needle) const { return find_subsequence(needle) != npos; }
    bool contains_subsequence(std::initializer_list<T> needle) const { return find_subsequence(needle) != npos; }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, std::size_t>::type count_subsequence(const C& needle) const {
        const std::size_t m = seq_size(needle);
        if (m == 0) return 0;
        std::size_t count = 0, at = 0;
        while (at + m <= q_len()) {
            const std::size_t i = algo::find_subsequence(q_ptr() + at, q_len() - at, seq_data(needle), m);
            if (i == npos) break;
            ++count;
            at += i + m;
        }
        return count;
    }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, int>::type compare(const C& other) const {
        const T* a = q_ptr();
        const T* b = seq_data(other);
        const std::size_t na = q_len(), nb = seq_size(other);
        const std::size_t n = na < nb ? na : nb;
        for (std::size_t i = 0; i < n; ++i) {
            if (a[i] < b[i]) return -1;
            if (b[i] < a[i]) return 1;
        }
        return na < nb ? -1 : (na > nb ? 1 : 0);
    }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, bool>::type equals(const C& other) const {
        return seq_size(other) == q_len() && equal_n(q_ptr(), seq_data(other), q_len());
    }

    template <typename C, typename Pred>
    typename std::enable_if<is_contiguous_of<C, T>::value, bool>::type equals(const C& other, Pred eq) const {
        if (seq_size(other) != q_len()) return false;
        const T* a = q_ptr();
        const T* b = seq_data(other);
        for (std::size_t i = 0, n = q_len(); i < n; ++i) if (!eq(a[i], b[i])) return false;
        return true;
    }

    template <typename U = T>
    typename std::enable_if<hashing::Hasher<U>::available, std::uint64_t>::type hash() const noexcept {
        if (std::is_integral<U>::value || std::is_enum<U>::value) return hashing::bytes(q_ptr(), q_len() * sizeof(T));
        std::uint64_t h = hashing::mix64(q_len());
        const T* d = q_ptr();
        hashing::Hasher<U> hs;
        for (std::size_t i = 0, n = q_len(); i < n; ++i) h = hashing::combine(h, hs(d[i]));
        return h;
    }

    template <typename Func>
    const Derived& for_each(Func func) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) func(d[i]);
        return static_cast<const Derived&>(*this);
    }

    template <typename Func>
    const Derived& for_each_indexed(Func func) const {
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) func(i, d[i]);
        return static_cast<const Derived&>(*this);
    }

    template <typename Func>
    const Derived& for_each(std::size_t start, std::size_t end_idx, Func func) const {
        if (!clamp_range(start, end_idx, q_len())) return static_cast<const Derived&>(*this);
        const T* d = q_ptr();
        for (std::size_t i = start; i < end_idx; ++i) func(d[i]);
        return static_cast<const Derived&>(*this);
    }

    typename FrequencyArrayOf<T>::type frequency() const { return frequency_impl(hashing::is_hashable<typename std::remove_const<T>::type>()); }

    DynamicArray<typename std::remove_const<T>::type> to_array() const { return DynamicArray<typename std::remove_const<T>::type>(q_ptr(), q_ptr() + q_len()); }

private:
    typename FrequencyArrayOf<T>::type frequency_impl(std::true_type) const {
        const T* d = q_ptr();
        const std::size_t n = q_len();
        if (n < kHashSetThreshold) return frequency_impl(std::false_type());
        typename FrequencyArrayOf<T>::type result;
        hashing::IndexSet<typename std::remove_const<T>::type> seen(d, n / 2 + 1);
        hashing::Hasher<typename std::remove_const<T>::type> h;
        index_array_t<T> slot_of;
        slot_of.resize_uninit(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t hv = h(d[i]);
            const std::size_t first = seen.find(d[i], hv);
            if (first == npos) {
                seen.insert_new(i, hv);
                slot_of[i] = result.size();
                result.emplace_back(d[i], std::size_t(1));
            } else {
                ++result[slot_of[first]].second;
            }
        }
        return result;
    }

    typename FrequencyArrayOf<T>::type frequency_impl(std::false_type) const {
        typename FrequencyArrayOf<T>::type result;
        const T* d = q_ptr();
        for (std::size_t i = 0, n = q_len(); i < n; ++i) {
            bool found = false;
            for (std::size_t j = 0; j < result.size(); ++j) {
                if (result[j].first == d[i]) { ++result[j].second; found = true; break; }
            }
            if (!found) result.emplace_back(d[i], std::size_t(1));
        }
        return result;
    }

    static T empty_value(const char* what) {
        return empty_value_impl(what, std::is_default_constructible<T>());
    }
    static T empty_value_impl(const char*, std::true_type) { return T{}; }
    static T empty_value_impl(const char* what, std::false_type) { throw std::out_of_range(std::string(what) + " on an empty array"); }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_OPS_QUERY_OPS_HPP
