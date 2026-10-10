#ifndef FIZMO_ARRAYS_OPS_FUNCTIONAL_OPS_HPP
#define FIZMO_ARRAYS_OPS_FUNCTIONAL_OPS_HPP

#include "edit_ops.hpp"
#include "../views/array_view.hpp"

namespace fizmo {

template <typename Container>
class BasicWhereProxy;

namespace arrays {
namespace detail {

template <typename Derived, typename T, typename Base>
class FunctionalOps : public Base {
protected:
    template <typename U>
    using rebind_t = typename rebind_array<Derived, U>::type;

    const Derived& cself() const noexcept { return static_cast<const Derived&>(*this); }
    Derived& self() noexcept { return static_cast<Derived&>(*this); }

    template <typename Out, typename Src>
    static Out copy_out(const Src* src, std::size_t n) {
        Out out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) out.push_back(src[i]);
        return out;
    }

    template <typename C>
    struct value_of { using type = typename std::decay<decltype(*std::declval<const C&>().data())>::type; };

public:
    ArrayView<T> view(std::size_t start, std::size_t end_idx) const noexcept {
        if (!clamp_range(start, end_idx, this->m_size)) return ArrayView<T>(nullptr, 0);
        return ArrayView<T>(this->m_data + start, end_idx - start);
    }

    ArrayView<T> view() const noexcept { return ArrayView<T>(this->m_data, this->m_size); }

    ArraySpan<T> span(std::size_t start, std::size_t end_idx) noexcept {
        if (!clamp_range(start, end_idx, this->m_size)) return ArraySpan<T>(nullptr, 0);
        return ArraySpan<T>(this->m_data + start, end_idx - start);
    }

    ArraySpan<T> span() noexcept { return ArraySpan<T>(this->m_data, this->m_size); }

    Derived slice(std::size_t start, std::size_t end_idx) const {
        if (!clamp_range(start, end_idx, this->m_size)) return Derived();
        return Derived(this->m_data + start, this->m_data + end_idx);
    }

    Derived slice() const { return Derived(cself()); }

    Derived slice(std::size_t start, std::size_t end_idx, std::size_t step) const {
        if (step == 0 || !clamp_range(start, end_idx, this->m_size)) return Derived();
        Derived out;
        out.reserve((end_idx - start + step - 1) / step);
        for (std::size_t i = start; i < end_idx; i += step) out.push_back(this->m_data[i]);
        return out;
    }

    Derived sorted() const { Derived c(cself()); c.sort(); return c; }
    template <typename Compare>
    Derived sorted(Compare comp) const { Derived c(cself()); c.sort(comp); return c; }
    Derived stable_sorted() const { Derived c(cself()); c.stable_sort(); return c; }
    template <typename Compare>
    Derived stable_sorted(Compare comp) const { Derived c(cself()); c.stable_sort(comp); return c; }
    Derived reversed() const {
        Derived out;
        out.reserve(this->m_size);
        for (std::size_t i = this->m_size; i > 0; --i) out.push_back(this->m_data[i - 1]);
        return out;
    }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value, Derived>::type concat(const C& other) const {
        Derived out;
        out.reserve(this->m_size + static_cast<std::size_t>(other.size()));
        out.append(this->m_data, this->m_size);
        out.append(other.data(), static_cast<std::size_t>(other.size()));
        return out;
    }

    rebind_t<Derived> split(const T& delimiter, bool keep_empty = true) const {
        rebind_t<Derived> parts;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= this->m_size; ++i) {
            if (i == this->m_size || this->m_data[i] == delimiter) {
                if (keep_empty || i > start) parts.push_back(Derived(this->m_data + start, this->m_data + i));
                start = i + 1;
            }
        }
        return parts;
    }

    template <typename Pred>
    rebind_t<Derived> split_if(Pred pred, bool keep_empty = true) const {
        rebind_t<Derived> parts;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= this->m_size; ++i) {
            if (i == this->m_size || pred(this->m_data[i])) {
                if (keep_empty || i > start) parts.push_back(Derived(this->m_data + start, this->m_data + i));
                start = i + 1;
            }
        }
        return parts;
    }

    DynamicArray<ArrayView<T>> chunks(std::size_t chunk_size) const {
        DynamicArray<ArrayView<T>> out;
        if (chunk_size == 0) return out;
        out.reserve((this->m_size + chunk_size - 1) / chunk_size);
        for (std::size_t i = 0; i < this->m_size; i += chunk_size) {
            const std::size_t n = this->m_size - i < chunk_size ? this->m_size - i : chunk_size;
            out.push_back(ArrayView<T>(this->m_data + i, n));
        }
        return out;
    }

    DynamicArray<ArrayView<T>> windows(std::size_t window_size) const {
        DynamicArray<ArrayView<T>> out;
        if (window_size == 0 || window_size > this->m_size) return out;
        out.reserve(this->m_size - window_size + 1);
        for (std::size_t i = 0; i + window_size <= this->m_size; ++i) out.push_back(ArrayView<T>(this->m_data + i, window_size));
        return out;
    }

    Derived distinct() const { return distinct_impl(hashing::is_hashable<T>()); }

    template <typename Pred>
    Derived distinct(Pred eq) const {
        Derived result;
        result.reserve(this->m_size);
        for (std::size_t i = 0; i < this->m_size; ++i) {
            bool found = false;
            for (std::size_t j = 0; j < result.size(); ++j) if (eq(this->m_data[i], result.data()[j])) { found = true; break; }
            if (!found) result.push_back(this->m_data[i]);
        }
        return result;
    }

    Derived union_with(const Derived& other) const {
        Derived result = distinct();
        const std::size_t extra = other.size();
        if (extra == 0) return result;
        if (use_hash(result.size() + extra)) {
            DynamicArray<T> keys;
            keys.reserve(result.size() + extra);
            keys.append(result.data(), result.size());
            hashing::IndexSet<T> seen(keys.data(), keys.capacity());
            hashing::Hasher<T> h;
            for (std::size_t i = 0; i < keys.size(); ++i) seen.insert_new(i, h(keys[i]));
            for (std::size_t i = 0; i < extra; ++i) {
                const T& v = other.data()[i];
                const std::uint64_t hv = h(v);
                if (seen.find(v, hv) != npos) continue;
                keys.push_back(v);
                seen.insert_new(keys.size() - 1, hv);
                result.push_back(v);
            }
            return result;
        }
        for (std::size_t i = 0; i < extra; ++i) if (!result.contains(other.data()[i])) result.push_back(other.data()[i]);
        return result;
    }

    Derived intersect(const Derived& other) const { return filter_membership(other, true); }
    Derived difference(const Derived& other) const { return filter_membership(other, false); }

    template <typename Pred>
    std::pair<Derived, Derived> partition(Pred pred) const {
        Derived matching, non_matching;
        for (std::size_t i = 0; i < this->m_size; ++i) {
            if (pred(this->m_data[i])) matching.push_back(this->m_data[i]);
            else non_matching.push_back(this->m_data[i]);
        }
        return { static_cast<Derived&&>(matching), static_cast<Derived&&>(non_matching) };
    }

    template <typename Func>
    rebind_t<Derived> group_by(Func func) const {
        using K = typename std::decay<decltype(func(std::declval<const T&>()))>::type;
        return group_by_impl<K>(func, hashing::is_hashable<K>());
    }

    template <typename Pred>
    Derived filter(Pred pred) const {
        Derived result;
        for (std::size_t i = 0; i < this->m_size; ++i) if (pred(this->m_data[i])) result.push_back(this->m_data[i]);
        return result;
    }

    template <typename Pred>
    Derived filter_indexed(Pred pred) const {
        Derived result;
        for (std::size_t i = 0; i < this->m_size; ++i) if (pred(i, this->m_data[i])) result.push_back(this->m_data[i]);
        return result;
    }

    template <typename Func>
    auto map(Func func) const -> rebind_t<typename std::decay<decltype(func(std::declval<const T&>()))>::type> {
        rebind_t<typename std::decay<decltype(func(std::declval<const T&>()))>::type> result;
        result.reserve(this->m_size);
        for (std::size_t i = 0; i < this->m_size; ++i) result.emplace_back(func(this->m_data[i]));
        return result;
    }

    template <typename Func>
    auto map_indexed(Func func) const -> rebind_t<typename std::decay<decltype(func(std::size_t{}, std::declval<const T&>()))>::type> {
        rebind_t<typename std::decay<decltype(func(std::size_t{}, std::declval<const T&>()))>::type> result;
        result.reserve(this->m_size);
        for (std::size_t i = 0; i < this->m_size; ++i) result.emplace_back(func(i, this->m_data[i]));
        return result;
    }

    template <typename Func>
    auto flat_map(Func func) const -> rebind_t<typename std::decay<decltype(func(std::declval<const T&>()))>::type::value_type> {
        using Inner = typename std::decay<decltype(func(std::declval<const T&>()))>::type;
        using U = typename Inner::value_type;
        rebind_t<U> result;
        for (std::size_t i = 0; i < this->m_size; ++i) {
            Inner part = func(this->m_data[i]);
            result.reserve(result.size() + static_cast<std::size_t>(part.size()));
            for (auto& v : part) result.emplace_back(static_cast<U&&>(v));
        }
        return result;
    }

    template <typename U = T>
    typename std::enable_if<is_custom_array_v<U>, typename U::value_type_array>::type
    flatten() const {
        using Inner = typename U::value_type_array;
        std::size_t total = 0;
        for (std::size_t i = 0; i < this->m_size; ++i) total += this->m_data[i].size();
        Inner result;
        result.reserve(total);
        for (std::size_t i = 0; i < this->m_size; ++i) result.append(this->m_data[i].data(), this->m_data[i].size());
        return result;
    }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, typename value_of<C>::type>::value, rebind_t<std::pair<T, typename value_of<C>::type>>>::type
    zip(const C& other) const {
        using B = typename value_of<C>::type;
        const std::size_t len = this->m_size < static_cast<std::size_t>(other.size()) ? this->m_size : static_cast<std::size_t>(other.size());
        rebind_t<std::pair<T, B>> result;
        result.reserve(len);
        for (std::size_t i = 0; i < len; ++i) result.emplace_back(this->m_data[i], other.data()[i]);
        return result;
    }

    template <typename CA, typename CB>
    static typename std::enable_if<is_custom_array_v<CA> && is_custom_array_v<CB>, rebind_t<std::pair<typename CA::value_type, typename CB::value_type>>>::type
    zip(const CA& a, const CB& b) {
        using A = typename CA::value_type;
        using B = typename CB::value_type;
        const std::size_t len = a.size() < b.size() ? a.size() : b.size();
        rebind_t<std::pair<A, B>> result;
        result.reserve(len);
        for (std::size_t i = 0; i < len; ++i) result.emplace_back(a[i], b[i]);
        return result;
    }

    template <typename CA, typename CB, typename Func>
    static auto zip_with(const CA& a, const CB& b, Func func)
        -> rebind_t<typename std::decay<decltype(func(std::declval<const typename CA::value_type&>(), std::declval<const typename CB::value_type&>()))>::type> {
        using U = typename std::decay<decltype(func(std::declval<const typename CA::value_type&>(), std::declval<const typename CB::value_type&>()))>::type;
        const std::size_t len = a.size() < b.size() ? a.size() : b.size();
        rebind_t<U> result;
        result.reserve(len);
        for (std::size_t i = 0; i < len; ++i) result.emplace_back(func(a[i], b[i]));
        return result;
    }

    template <typename Pred>
    Derived take_while(Pred pred) const {
        std::size_t n = 0;
        while (n < this->m_size && pred(this->m_data[n])) ++n;
        return Derived(this->m_data, this->m_data + n);
    }

    template <typename Pred>
    Derived drop_while(Pred pred) const {
        std::size_t n = 0;
        while (n < this->m_size && pred(this->m_data[n])) ++n;
        return Derived(this->m_data + n, this->m_data + this->m_size);
    }

    template <typename U, typename Func>
    rebind_t<U> scan(U init, Func func) const {
        rebind_t<U> result;
        result.reserve(this->m_size);
        for (std::size_t i = 0; i < this->m_size; ++i) {
            init = func(static_cast<U&&>(init), this->m_data[i]);
            result.push_back(init);
        }
        return result;
    }

    template <typename Func>
    Derived scan(Func func) const {
        Derived result;
        if (this->m_size == 0) return result;
        result.reserve(this->m_size);
        T acc = this->m_data[0];
        result.push_back(acc);
        for (std::size_t i = 1; i < this->m_size; ++i) {
            acc = func(static_cast<T&&>(acc), this->m_data[i]);
            result.push_back(acc);
        }
        return result;
    }

    Derived prefix_sum() const {
        Derived result;
        if (this->m_size == 0) return result;
        result.reserve(this->m_size);
        T acc = this->m_data[0];
        result.push_back(acc);
        for (std::size_t i = 1; i < this->m_size; ++i) {
            acc = acc + this->m_data[i];
            result.push_back(acc);
        }
        return result;
    }

    BasicWhereProxy<Derived> where(const T& value);

    template <typename Pred>
    BasicWhereProxy<Derived> where(Pred pred);

    BasicWhereProxy<Derived> where_indices(index_array_t<T> indices);

    BasicWhereProxy<const Derived> where(const T& value) const;

    template <typename Pred>
    BasicWhereProxy<const Derived> where(Pred pred) const;

private:
    static bool use_hash(std::size_t n) noexcept { return hashing::is_hashable<T>::value && n >= kHashSetThreshold; }

    Derived distinct_impl(std::true_type) const {
        if (this->m_size < kHashSetThreshold) return distinct_impl(std::false_type());
        Derived result;
        result.reserve(this->m_size);
        hashing::IndexSet<T> seen(this->m_data, this->m_size);
        hashing::Hasher<T> h;
        for (std::size_t i = 0; i < this->m_size; ++i) {
            const std::uint64_t hv = h(this->m_data[i]);
            if (seen.find(this->m_data[i], hv) != npos) continue;
            seen.insert_new(i, hv);
            result.push_back(this->m_data[i]);
        }
        return result;
    }

    Derived distinct_impl(std::false_type) const {
        Derived result;
        result.reserve(this->m_size);
        for (std::size_t i = 0; i < this->m_size; ++i) if (!result.contains(this->m_data[i])) result.push_back(this->m_data[i]);
        return result;
    }

    Derived filter_membership(const Derived& other, bool keep_present) const {
        Derived result;
        if (use_hash(this->m_size + other.size())) {
            hashing::IndexSet<T> in_other(other.data(), other.size());
            hashing::Hasher<T> h;
            for (std::size_t i = 0; i < other.size(); ++i) in_other.insert(i, h(other.data()[i]));
            hashing::IndexSet<T> emitted(this->m_data, this->m_size);
            for (std::size_t i = 0; i < this->m_size; ++i) {
                const T& v = this->m_data[i];
                const std::uint64_t hv = h(v);
                if ((in_other.find(v, hv) != npos) != keep_present) continue;
                if (emitted.find(v, hv) != npos) continue;
                emitted.insert_new(i, hv);
                result.push_back(v);
            }
            return result;
        }
        for (std::size_t i = 0; i < this->m_size; ++i) {
            const T& v = this->m_data[i];
            if (other.contains(v) == keep_present && !result.contains(v)) result.push_back(v);
        }
        return result;
    }

    template <typename K, typename Func>
    rebind_t<Derived> group_by_impl(Func& func, std::true_type) const {
        rebind_t<Derived> groups;
        DynamicArray<K> keys;
        keys.reserve(this->m_size);
        hashing::IndexSet<K> index(keys.data(), 16);
        hashing::Hasher<K> h;
        for (std::size_t i = 0; i < this->m_size; ++i) {
            K key = func(this->m_data[i]);
            const std::uint64_t hv = h(key);
            index.reset_keys(keys.data());
            const std::size_t g = index.find(key, hv);
            if (g == npos) {
                keys.push_back(static_cast<K&&>(key));
                index.reset_keys(keys.data());
                index.insert_new(keys.size() - 1, hv);
                groups.push_back(Derived());
                groups.back().push_back(this->m_data[i]);
            } else {
                groups[g].push_back(this->m_data[i]);
            }
        }
        return groups;
    }

    template <typename K, typename Func>
    rebind_t<Derived> group_by_impl(Func& func, std::false_type) const {
        rebind_t<Derived> groups;
        DynamicArray<K> keys;
        for (std::size_t i = 0; i < this->m_size; ++i) {
            K key = func(this->m_data[i]);
            std::size_t g = npos;
            for (std::size_t k = 0; k < keys.size(); ++k) if (keys[k] == key) { g = k; break; }
            if (g == npos) {
                keys.push_back(static_cast<K&&>(key));
                groups.push_back(Derived());
                groups.back().push_back(this->m_data[i]);
            } else {
                groups[g].push_back(this->m_data[i]);
            }
        }
        return groups;
    }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_OPS_FUNCTIONAL_OPS_HPP
