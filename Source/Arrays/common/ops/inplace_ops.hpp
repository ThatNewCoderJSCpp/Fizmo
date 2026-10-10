#ifndef FIZMO_ARRAYS_OPS_INPLACE_OPS_HPP
#define FIZMO_ARRAYS_OPS_INPLACE_OPS_HPP

#include "query_ops.hpp"
#include "../algorithms/sort_integer.hpp"
#include "../algorithms/sort_named.hpp"
#include "../algorithms/parallel.hpp"
#include "../../../Random/random_std_int.hpp"
#include <string>

namespace fizmo {
namespace arrays {
namespace detail {

template <typename C, typename T, typename = void>
struct is_mutable_contiguous_of : std::false_type {};

template <typename C, typename T>
struct is_mutable_contiguous_of<C, T, void_t<decltype(std::declval<C&>().data()), decltype(std::declval<const C&>().size())>>
    : std::integral_constant<bool, std::is_same<decltype(std::declval<C&>().data()), T*>::value> {};

template <typename Derived, typename T, typename Base>
class InplaceOps : public Base {
protected:
    T* m_ptr() noexcept { return this->fz_ptr(); }
    const T* m_cptr() const noexcept { return this->fz_cptr(); }
    std::size_t m_len() const noexcept { return this->fz_len(); }
    Derived& self() noexcept { return static_cast<Derived&>(*this); }

    template <typename Fn>
    void ranged(std::size_t start, std::size_t end_idx, Fn fn) {
        if (!clamp_range(start, end_idx, m_len())) return;
        fn(m_ptr() + start, end_idx - start);
    }

    template <typename U = T>
    using enable_integral = typename std::enable_if<std::is_integral<U>::value && !std::is_same<U, bool>::value>::type;

    template <typename U = T>
    using enable_radix = typename std::enable_if<algo::RadixKey<U>::supported && std::is_integral<U>::value>::type;

public:
    using Base::for_each;
    using Base::for_each_indexed;

    void sort() { algo::fast_sort(m_ptr(), m_len(), Less{}); }
    template <typename Compare>
    void sort(Compare comp) { algo::fast_sort(m_ptr(), m_len(), comp); }
    void sort(std::size_t start, std::size_t end_idx) { sort(start, end_idx, Less{}); }
    template <typename Compare>
    void sort(std::size_t start, std::size_t end_idx, Compare comp) { ranged(start, end_idx, [&](T* p, std::size_t n) { algo::fast_sort(p, n, comp); }); }
    void sort_descending() { algo::fast_sort(m_ptr(), m_len(), Greater{}); }

    void stable_sort() { algo::fast_stable_sort(m_ptr(), m_len(), Less{}); }
    template <typename Compare>
    void stable_sort(Compare comp) { algo::fast_stable_sort(m_ptr(), m_len(), comp); }
    void stable_sort(std::size_t start, std::size_t end_idx) { stable_sort(start, end_idx, Less{}); }
    template <typename Compare>
    void stable_sort(std::size_t start, std::size_t end_idx, Compare comp) { ranged(start, end_idx, [&](T* p, std::size_t n) { algo::fast_stable_sort(p, n, comp); }); }

    template <typename Key>
    void sort_by(Key key) { algo::fast_sort(m_ptr(), m_len(), [&](const T& a, const T& b) { return key(a) < key(b); }); }
    template <typename Key>
    void stable_sort_by(Key key) { algo::fast_stable_sort(m_ptr(), m_len(), [&](const T& a, const T& b) { return key(a) < key(b); }); }

    void parallel_sort(unsigned threads = 0) { algo::parallel_sort(m_ptr(), m_len(), Less{}, threads); }
    template <typename Compare>
    void parallel_sort(Compare comp, unsigned threads = 0) { algo::parallel_sort(m_ptr(), m_len(), comp, threads); }
    void parallel_stable_sort(unsigned threads = 0) { algo::parallel_sort(m_ptr(), m_len(), Less{}, threads, true); }
    template <typename Compare>
    void parallel_stable_sort(Compare comp, unsigned threads = 0) { algo::parallel_sort(m_ptr(), m_len(), comp, threads, true); }

    void nth_element(std::size_t k) { algo::nth_element(m_ptr(), m_len(), k, Less{}); }
    template <typename Compare>
    void nth_element(std::size_t k, Compare comp) { algo::nth_element(m_ptr(), m_len(), k, comp); }
    void nth_element(std::size_t start, std::size_t end_idx, std::size_t k) { nth_element(start, end_idx, k, Less{}); }
    template <typename Compare>
    void nth_element(std::size_t start, std::size_t end_idx, std::size_t k, Compare comp) {
        ranged(start, end_idx, [&](T* p, std::size_t n) { if (k >= start && k - start < n) algo::nth_element(p, n, k - start, comp); });
    }

    void partial_sort(std::size_t k) { algo::partial_sort(m_ptr(), m_len(), k, Less{}); }
    template <typename Compare>
    void partial_sort(std::size_t k, Compare comp) { algo::partial_sort(m_ptr(), m_len(), k, comp); }
    void partial_sort(std::size_t start, std::size_t end_idx, std::size_t k) { partial_sort(start, end_idx, k, Less{}); }
    template <typename Compare>
    void partial_sort(std::size_t start, std::size_t end_idx, std::size_t k, Compare comp) { ranged(start, end_idx, [&](T* p, std::size_t n) { algo::partial_sort(p, n, k, comp); }); }

#define FIZMO_ARRAY_NAMED_SORT(name, fn)                                                                                         \
    void name() { fn(m_ptr(), m_len(), Less{}); }                                                                                \
    template <typename Compare>                                                                                                   \
    void name(Compare comp) { fn(m_ptr(), m_len(), comp); }                                                                      \
    void name(std::size_t start, std::size_t end_idx) { name(start, end_idx, Less{}); }                                         \
    template <typename Compare>                                                                                                   \
    void name(std::size_t start, std::size_t end_idx, Compare comp) { ranged(start, end_idx, [&](T* p, std::size_t n) { fn(p, n, comp); }); }

    FIZMO_ARRAY_NAMED_SORT(bubble_sort, algo::bubble_sort)
    FIZMO_ARRAY_NAMED_SORT(selection_sort, algo::selection_sort)
    FIZMO_ARRAY_NAMED_SORT(cocktail_sort, algo::cocktail_sort)
    FIZMO_ARRAY_NAMED_SORT(merge_sort, algo::stable_sort)
    FIZMO_ARRAY_NAMED_SORT(quick_sort, algo::quick_sort)
    FIZMO_ARRAY_NAMED_SORT(shell_sort, algo::shell_sort)
    FIZMO_ARRAY_NAMED_SORT(gnome_sort, algo::gnome_sort)
    FIZMO_ARRAY_NAMED_SORT(bitonic_sort, algo::bitonic_sort)
    FIZMO_ARRAY_NAMED_SORT(introsort, algo::pdq_sort)

#undef FIZMO_ARRAY_NAMED_SORT

    void insertion_sort() { Less c; algo::insertion_sort(m_ptr(), m_len(), c); }
    template <typename Compare>
    void insertion_sort(Compare comp) { algo::insertion_sort(m_ptr(), m_len(), comp); }
    void insertion_sort(std::size_t start, std::size_t end_idx) { insertion_sort(start, end_idx, Less{}); }
    template <typename Compare>
    void insertion_sort(std::size_t start, std::size_t end_idx, Compare comp) { ranged(start, end_idx, [&](T* p, std::size_t n) { algo::insertion_sort(p, n, comp); }); }

    void heap_sort() { Less c; algo::heap_sort(m_ptr(), m_len(), c); }
    template <typename Compare>
    void heap_sort(Compare comp) { algo::heap_sort(m_ptr(), m_len(), comp); }
    void heap_sort(std::size_t start, std::size_t end_idx) { heap_sort(start, end_idx, Less{}); }
    template <typename Compare>
    void heap_sort(std::size_t start, std::size_t end_idx, Compare comp) { ranged(start, end_idx, [&](T* p, std::size_t n) { algo::heap_sort(p, n, comp); }); }

    template <typename U = T, typename = enable_integral<U>>
    void counting_sort() { algo::counting_sort(m_ptr(), m_len(), false); }
    template <typename U = T, typename = enable_integral<U>>
    void counting_sort(std::size_t start, std::size_t end_idx) { ranged(start, end_idx, [](T* p, std::size_t n) { algo::counting_sort(p, n, false); }); }
    template <typename U = T, typename = enable_integral<U>>
    void counting_sort_desc() { algo::counting_sort(m_ptr(), m_len(), true); }
    template <typename U = T, typename = enable_integral<U>>
    void counting_sort_desc(std::size_t start, std::size_t end_idx) { ranged(start, end_idx, [](T* p, std::size_t n) { algo::counting_sort(p, n, true); }); }

    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_lsd() { algo::radix_sort_lsd(m_ptr(), m_len()); }
    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_lsd(std::size_t start, std::size_t end_idx) { ranged(start, end_idx, [](T* p, std::size_t n) { algo::radix_sort_lsd(p, n); }); }
    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_lsd_desc() { algo::radix_sort_lsd(m_ptr(), m_len()); reverse_n(m_ptr(), m_len()); }
    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_lsd_desc(std::size_t start, std::size_t end_idx) { ranged(start, end_idx, [](T* p, std::size_t n) { algo::radix_sort_lsd(p, n); reverse_n(p, n); }); }

    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_msd() { algo::radix_sort_msd(m_ptr(), m_len()); }
    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_msd(std::size_t start, std::size_t end_idx) { ranged(start, end_idx, [](T* p, std::size_t n) { algo::radix_sort_msd(p, n); }); }
    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_msd_desc() { algo::radix_sort_msd(m_ptr(), m_len()); reverse_n(m_ptr(), m_len()); }
    template <typename U = T, typename = enable_radix<U>>
    void radix_sort_msd_desc(std::size_t start, std::size_t end_idx) { ranged(start, end_idx, [](T* p, std::size_t n) { algo::radix_sort_msd(p, n); reverse_n(p, n); }); }

    void reverse() noexcept { reverse_n(m_ptr(), m_len()); }
    void reverse(std::size_t start, std::size_t end_idx) noexcept { ranged(start, end_idx, [](T* p, std::size_t n) { reverse_n(p, n); }); }

    void rotate_left(std::size_t n = 1) { rotate_left_n(m_ptr(), m_len(), n); }
    void rotate_right(std::size_t n = 1) { if (m_len() > 1) rotate_left_n(m_ptr(), m_len(), m_len() - n % m_len()); }
    void rotate_left(std::size_t start, std::size_t end_idx, std::size_t n) { ranged(start, end_idx, [n](T* p, std::size_t len) { rotate_left_n(p, len, n); }); }
    void rotate_right(std::size_t start, std::size_t end_idx, std::size_t n) { ranged(start, end_idx, [n](T* p, std::size_t len) { if (len > 1) rotate_left_n(p, len, len - n % len); }); }

    void shuffle() { shuffle(std::size_t(0), m_len()); }
    void shuffle(std::size_t start, std::size_t end_idx) {
        ranged(start, end_idx, [](T* p, std::size_t n) {
            for (std::size_t i = n - 1; i > 0; --i) {
                const std::size_t j = random_int<std::size_t>(0, i);
                if (j != i) swap_values(p[i], p[j]);
            }
        });
    }

    void unsecure_shuffle() { unsecure_shuffle(std::size_t(0), m_len()); }
    void unsecure_shuffle(std::size_t start, std::size_t end_idx) {
        ranged(start, end_idx, [](T* p, std::size_t n) {
            for (std::size_t i = n - 1; i > 0; --i) {
                const std::size_t j = unsecure_random_int<std::size_t>(0, i);
                if (j != i) swap_values(p[i], p[j]);
            }
        });
    }

    template <typename Pred>
    std::size_t partition_in_place(Pred pred) { return algo::partition(m_ptr(), m_len(), pred); }
    template <typename Pred>
    std::size_t partition_in_place(std::size_t start, std::size_t end_idx, Pred pred) {
        if (!clamp_range(start, end_idx, m_len())) return start;
        return start + algo::partition(m_ptr() + start, end_idx - start, pred);
    }
    template <typename Pred>
    std::size_t stable_partition_in_place(Pred pred) { return algo::stable_partition(m_ptr(), m_len(), pred); }
    template <typename Pred>
    std::size_t stable_partition_in_place(std::size_t start, std::size_t end_idx, Pred pred) {
        if (!clamp_range(start, end_idx, m_len())) return start;
        return start + algo::stable_partition(m_ptr() + start, end_idx - start, pred);
    }

    Derived& fill(const T& value) { fill_assign_n(m_ptr(), m_len(), value); return self(); }
    Derived& fill(std::size_t start, std::size_t end_idx, const T& value) { ranged(start, end_idx, [&](T* p, std::size_t n) { fill_assign_n(p, n, value); }); return self(); }

    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, Derived&>::type iota(U start = U(0), U step = U(1)) {
        T* d = m_ptr();
        for (std::size_t i = 0, n = m_len(); i < n; ++i) { d[i] = start; start = static_cast<U>(start + step); }
        return self();
    }

    template <typename Func>
    Derived& generate(Func func) {
        T* d = m_ptr();
        for (std::size_t i = 0, n = m_len(); i < n; ++i) d[i] = func();
        return self();
    }

    template <typename Func>
    Derived& generate(std::size_t start, std::size_t end_idx, Func func) {
        ranged(start, end_idx, [&](T* p, std::size_t n) { for (std::size_t i = 0; i < n; ++i) p[i] = func(); });
        return self();
    }

    template <typename Func>
    Derived& transform_in_place(Func func) {
        T* d = m_ptr();
        for (std::size_t i = 0, n = m_len(); i < n; ++i) d[i] = func(d[i]);
        return self();
    }

    template <typename Func>
    Derived& transform_in_place(std::size_t start, std::size_t end_idx, Func func) {
        ranged(start, end_idx, [&](T* p, std::size_t n) { for (std::size_t i = 0; i < n; ++i) p[i] = func(p[i]); });
        return self();
    }

    template <typename Func>
    Derived& for_each(Func func) {
        T* d = m_ptr();
        for (std::size_t i = 0, n = m_len(); i < n; ++i) func(d[i]);
        return self();
    }

    template <typename Func>
    Derived& for_each_indexed(Func func) {
        T* d = m_ptr();
        for (std::size_t i = 0, n = m_len(); i < n; ++i) func(i, d[i]);
        return self();
    }

    template <typename Func>
    Derived& for_each(std::size_t start, std::size_t end_idx, Func func) {
        ranged(start, end_idx, [&](T* p, std::size_t n) { for (std::size_t i = 0; i < n; ++i) func(p[i]); });
        return self();
    }

    template <typename Func>
    Derived& parallel_for_each(Func func, unsigned threads = 0) {
        algo::parallel_for(m_ptr(), m_len(), [&](std::size_t, T& v) { func(v); }, threads);
        return self();
    }

    template <typename Func>
    Derived& parallel_for_each_indexed(Func func, unsigned threads = 0) {
        algo::parallel_for(m_ptr(), m_len(), [&](std::size_t i, T& v) { func(i, v); }, threads, true);
        return self();
    }

    void reset() { T* d = m_ptr(); for (std::size_t i = 0, n = m_len(); i < n; ++i) d[i] = T{}; }
    void deep_reset() { T* d = m_ptr(); for (std::size_t i = 0, n = m_len(); i < n; ++i) fizmo::detail::deep_reset_element(d[i]); }

    void replace(const T& value) { fill_assign_n(m_ptr(), m_len(), value); }

    void replace(T&& value) {
        const std::size_t n = m_len();
        if (n == 0) return;
        T* d = m_ptr();
        for (std::size_t i = 0; i + 1 < n; ++i) d[i] = value;
        d[n - 1] = static_cast<T&&>(value);
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U> && !std::is_same<typename std::decay<U>::type, T>::value>::type
    replace(U&& value) { replace(static_cast<T>(static_cast<U&&>(value))); }

    void replace(std::size_t index, const T& value) { m_ptr()[index] = value; }
    void replace(std::size_t index, T&& value) { m_ptr()[index] = static_cast<T&&>(value); }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U> && !std::is_same<typename std::decay<U>::type, T>::value>::type
    replace(std::size_t index, U&& value) { m_ptr()[index] = static_cast<T>(static_cast<U&&>(value)); }

    void replace(std::size_t start, std::size_t end_idx, const T& value) { ranged(start, end_idx, [&](T* p, std::size_t n) { fill_assign_n(p, n, value); }); }

    void replace(std::size_t start, std::size_t end_idx, T&& value) {
        ranged(start, end_idx, [&](T* p, std::size_t n) {
            for (std::size_t i = 0; i + 1 < n; ++i) p[i] = value;
            p[n - 1] = static_cast<T&&>(value);
        });
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U> && !std::is_same<typename std::decay<U>::type, T>::value && !std::is_same<typename std::decay<U>::type, Derived>::value>::type
    replace(std::size_t start, std::size_t end_idx, U&& value) {
        const T converted = static_cast<T>(static_cast<U&&>(value));
        replace(start, end_idx, converted);
    }

    void replace(std::size_t start, std::size_t end_idx, const Derived& arr) {
        ranged(start, end_idx, [&](T* p, std::size_t n) {
            const std::size_t c = n < arr.size() ? n : arr.size();
            copy_assign_n(p, arr.data(), c);
        });
    }

    void replace(std::size_t start, std::size_t end_idx, Derived&& arr) {
        ranged(start, end_idx, [&](T* p, std::size_t n) {
            const std::size_t c = n < arr.size() ? n : arr.size();
            move_assign_n(p, arr.data(), c);
        });
    }

    std::size_t replace_by_value(const T& old_value, const T& new_value, std::size_t count = 0, Direction dir = Direction::All) {
        const T replacement(new_value);
        const T target(old_value);
        return replace_matching([&](const T& v) { return v == target; }, replacement, count, dir);
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U> && !std::is_same<typename std::decay<U>::type, T>::value, std::size_t>::type
    replace_by_value(const T& old_value, U&& new_value, std::size_t count = 0, Direction dir = Direction::All) {
        const T converted = static_cast<T>(static_cast<U&&>(new_value));
        return replace_by_value(old_value, converted, count, dir);
    }

    template <typename Pred>
    std::size_t replace_if(Pred pred, const T& new_value, std::size_t count = 0, Direction dir = Direction::All) {
        const T replacement(new_value);
        return replace_matching(pred, replacement, count, dir);
    }

    void swap(std::size_t i, std::size_t j) noexcept { if (i != j) swap_values(m_ptr()[i], m_ptr()[j]); }
    void swap(std::size_t i, Derived& other, std::size_t j) noexcept { if (&other != &self() || i != j) swap_values(m_ptr()[i], other.data()[j]); }

    void swap_ranges(std::size_t a_start, std::size_t b_start, std::size_t count) noexcept {
        if (a_start == b_start || count == 0) return;
        T* d = m_ptr();
        if (a_start + count <= b_start || b_start + count <= a_start) { swap_ranges_n(d + a_start, d + b_start, count); return; }
        for (std::size_t i = 0; i < count; ++i) swap_values(d[a_start + i], d[b_start + i]);
    }

    void swap_ranges(std::size_t a_start, Derived& other, std::size_t b_start, std::size_t count) noexcept {
        if ((&other == &self() && a_start == b_start) || count == 0) return;
        if (&other == &self()) { swap_ranges(a_start, b_start, count); return; }
        swap_ranges_n(m_ptr() + a_start, other.data() + b_start, count);
    }

    void swap_ranges(std::size_t a_start, std::size_t a_end, Derived& other, std::size_t b_start, std::size_t b_end) {
        const std::size_t n1 = a_end - a_start;
        const std::size_t n2 = b_end - b_start;
        if (n1 != n2) {
            throw std::invalid_argument(
                "swap_ranges unequal ranges. *this range = [" + std::to_string(a_start) + ", " + std::to_string(a_end) + "] = " + std::to_string(n1) +
                "\nOther range = [" + std::to_string(b_start) + ", " + std::to_string(b_end) + "] = " + std::to_string(n2));
        }
        swap_ranges(a_start, other, b_start, n1);
    }

    void swap(const T& a, Derived& other, const T& b) {
        const index_array_t<T> ia = this->find_indices(a);
        const index_array_t<T> ib = other.find_indices(b);
        const std::size_t n = ia.size() < ib.size() ? ia.size() : ib.size();
        for (std::size_t i = 0; i < n; ++i) swap_values(m_ptr()[ia[i]], other.data()[ib[i]]);
    }

    void copy_to(std::size_t src, Derived& dest_arr, std::size_t dst) const { dest_arr.data()[dst] = m_cptr()[src]; }
    void copy_to(std::size_t src_start, std::size_t count, Derived& dest_arr, std::size_t dst_start) const { copy_assign_n(dest_arr.data() + dst_start, m_cptr() + src_start, count); }
    void copy_to(std::size_t src_start, std::size_t count, T* dest) const { copy_assign_n(dest, m_cptr() + src_start, count); }

    template <typename C>
    typename std::enable_if<is_mutable_contiguous_of<C, T>::value && !std::is_same<C, Derived>::value>::type
    copy_to(std::size_t src_start, std::size_t count, C& dest, std::size_t dst_start) const { copy_assign_n(dest.data() + dst_start, m_cptr() + src_start, count); }

    void copy_from(Derived& src_arr, std::size_t src, std::size_t dst) { m_ptr()[dst] = src_arr.data()[src]; }
    void copy_from(const Derived& src_arr, std::size_t src_start, std::size_t count, std::size_t dst_start) { copy_assign_n(m_ptr() + dst_start, src_arr.data() + src_start, count); }
    void copy_from(const T* src, std::size_t count, std::size_t dst_start) { copy_assign_n(m_ptr() + dst_start, src, count); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value && !std::is_same<C, Derived>::value>::type
    copy_from(const C& src, std::size_t src_start, std::size_t count, std::size_t dst_start) { copy_assign_n(m_ptr() + dst_start, src.data() + src_start, count); }

    void copy_within(std::size_t src_start, std::size_t count, std::size_t dst_start) { if (count && src_start != dst_start) copy_assign_n(m_ptr() + dst_start, m_cptr() + src_start, count); }

    void move_to(std::size_t src, Derived& dest_arr, std::size_t dst) noexcept { dest_arr.data()[dst] = static_cast<T&&>(m_ptr()[src]); }
    void move_to(std::size_t src_start, std::size_t count, Derived& dest_arr, std::size_t dst_start) noexcept { move_assign_n(dest_arr.data() + dst_start, m_ptr() + src_start, count); }
    void move_to(std::size_t src_start, std::size_t count, T* dest) noexcept { move_assign_n(dest, m_ptr() + src_start, count); }

    template <typename C>
    typename std::enable_if<is_mutable_contiguous_of<C, T>::value && !std::is_same<C, Derived>::value>::type
    move_to(std::size_t src_start, std::size_t count, C& dest, std::size_t dst_start) noexcept { move_assign_n(dest.data() + dst_start, m_ptr() + src_start, count); }

    void move_from(Derived& src_arr, std::size_t src, std::size_t dst) noexcept { m_ptr()[dst] = static_cast<T&&>(src_arr.data()[src]); }
    void move_from(Derived& src_arr, std::size_t src_start, std::size_t count, std::size_t dst_start) noexcept { move_assign_n(m_ptr() + dst_start, src_arr.data() + src_start, count); }
    void move_from(T* src, std::size_t count, std::size_t dst_start) noexcept { move_assign_n(m_ptr() + dst_start, src, count); }

    template <typename C>
    typename std::enable_if<is_mutable_contiguous_of<C, T>::value && !std::is_same<C, Derived>::value>::type
    move_from(C& src, std::size_t src_start, std::size_t count, std::size_t dst_start) noexcept { move_assign_n(m_ptr() + dst_start, src.data() + src_start, count); }

    void move_within(std::size_t src_start, std::size_t count, std::size_t dst_start) noexcept { if (count && src_start != dst_start) move_assign_n(m_ptr() + dst_start, m_ptr() + src_start, count); }

    void replace_at(const std::size_t* idx, std::size_t idx_count, const T& value) {
        const T v(value);
        T* d = m_ptr();
        for (std::size_t i = 0; i < idx_count; ++i) d[idx[i]] = v;
    }

private:
    template <typename Pred>
    std::size_t replace_matching(Pred pred, const T& value, std::size_t count, Direction dir) {
        T* d = m_ptr();
        const std::size_t n = m_len();
        if (count == 0 || dir == Direction::All) {
            std::size_t c = 0;
            for (std::size_t i = 0; i < n; ++i) if (pred(d[i])) { d[i] = value; ++c; }
            return c;
        }
        std::size_t total = 0;
        for (std::size_t i = 0; i < n; ++i) if (pred(d[i])) ++total;
        const std::size_t take = count < total ? count : total;
        std::size_t c = 0;
        if (dir == Direction::Front) {
            for (std::size_t i = 0; i < n && c < take; ++i) if (pred(d[i])) { d[i] = value; ++c; }
        } else {
            for (std::size_t i = n; i > 0 && c < take; --i) if (pred(d[i - 1])) { d[i - 1] = value; ++c; }
        }
        return c;
    }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_OPS_INPLACE_OPS_HPP
