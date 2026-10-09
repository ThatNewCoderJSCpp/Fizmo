#ifndef FIZMO_ARRAYS_OPS_EDIT_OPS_HPP
#define FIZMO_ARRAYS_OPS_EDIT_OPS_HPP

#include "inplace_ops.hpp"
#include "../algorithms/hash.hpp"

namespace fizmo {
namespace arrays {
namespace detail {

template <typename Derived, typename T, typename Base>
class EditOps : public Base {
protected:
    using ET = ElementTraits<T>;

    Derived& self() noexcept { return static_cast<Derived&>(*this); }

    template <typename U>
    using enable_convertible = typename std::enable_if<are_compatible_types<T, U> && !std::is_same<typename std::decay<U>::type, T>::value>::type;

    void ensure_room(std::size_t extra) {
        if (extra > this->m_capacity - this->m_size) this->fz_reserve_total(this->m_size + extra);
    }

    template <typename Fill>
    void insert_gap(std::size_t index, std::size_t count, Fill fill) {
        if (count == 0) return;
        if (index > this->m_size) index = this->m_size;
        ensure_room(count);
        T* d = this->m_data;
        open_gap(d, this->m_size, index, count);
        std::size_t done = 0;
#if defined(FIZMO_ARRAYS_EXCEPTIONS)
        try {
            for (; done < count; ++done) fill(d + index + done, done);
        } catch (...) {
            destroy_n(d + index, done);
            close_gap(d, this->m_size + count, index, count);
            throw;
        }
#else
        for (; done < count; ++done) fill(d + index + done, done);
#endif
        this->m_size += count;
    }

    void erase_range(std::size_t start, std::size_t end_idx) noexcept {
        if (start >= end_idx) return;
        T* d = this->m_data;
        destroy_n(d + start, end_idx - start);
        close_gap(d, this->m_size, start, end_idx - start);
        this->m_size -= end_idx - start;
    }

    void truncate(std::size_t new_size) noexcept {
        if (new_size >= this->m_size) return;
        destroy_n(this->m_data + new_size, this->m_size - new_size);
        this->m_size = new_size;
    }

    T take_out(std::size_t index) {
        T value(static_cast<T&&>(this->m_data[index]));
        return value;
    }

    using IndexList = index_array_t<T>;

    static void set_length(Derived& d, std::size_t n) noexcept { static_cast<EditOps&>(d).m_size = n; }

    template <typename Pred>
    void collect_matching(Pred& pred, std::size_t count, Direction dir, IndexList& out) const {
        const T* d = this->m_data;
        const std::size_t n = this->m_size;
        if (count == 0 || dir == Direction::All) {
            for (std::size_t i = 0; i < n; ++i) if (pred(d[i])) out.push_back(i);
            return;
        }
        if (dir == Direction::Front) {
            for (std::size_t i = 0; i < n && out.size() < count; ++i) if (pred(d[i])) out.push_back(i);
            return;
        }
        for (std::size_t i = n; i > 0 && out.size() < count; --i) if (pred(d[i - 1])) out.push_back(i - 1);
        out.reverse();
    }

public:
    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (this->m_size == this->m_capacity) {
            T tmp(static_cast<Args&&>(args)...);
            this->fz_reserve_total(this->m_size + 1);
            construct_element(this->m_data + this->m_size, static_cast<T&&>(tmp));
        } else {
            construct_element(this->m_data + this->m_size, static_cast<Args&&>(args)...);
        }
        return this->m_data[this->m_size++];
    }

    template <typename... Args>
    T& emplace_front(Args&&... args) { return emplace(0, static_cast<Args&&>(args)...); }

    template <typename... Args>
    T& emplace(std::size_t index, Args&&... args) {
        if (index >= this->m_size) return emplace_back(static_cast<Args&&>(args)...);
        T tmp(static_cast<Args&&>(args)...);
        insert_gap(index, 1, [&](T* p, std::size_t) { construct_element(p, static_cast<T&&>(tmp)); });
        return this->m_data[index];
    }

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(static_cast<T&&>(value)); }

    template <typename U, typename = enable_convertible<U>>
    void push_back(U&& value) { emplace_back(static_cast<T>(static_cast<U&&>(value))); }

    void push_back(const Derived& arr) { append(arr.data(), arr.size()); }

    void push_back(Derived&& arr) {
        if (&arr == &self()) { append(arr.data(), arr.size()); return; }
        const std::size_t n = arr.size();
        if (n == 0) return;
        ensure_room(n);
        construct_move_n(this->m_data + this->m_size, arr.data(), n);
        this->m_size += n;
    }

    void push_front(const T& value) { insert(0, value); }
    void push_front(T&& value) { insert(0, static_cast<T&&>(value)); }

    template <typename U, typename = enable_convertible<U>>
    void push_front(U&& value) { insert(0, static_cast<T>(static_cast<U&&>(value))); }

    void push_front(const Derived& arr) { insert(0, arr.data(), arr.size()); }
    void push_front(Derived&& arr) { insert(0, static_cast<Derived&&>(arr)); }

    void insert(std::size_t index, const T& value) {
        if (points_into(&value, this->m_data, this->m_size)) {
            T tmp(value);
            insert_gap(index, 1, [&](T* p, std::size_t) { construct_element(p, static_cast<T&&>(tmp)); });
            return;
        }
        insert_gap(index, 1, [&](T* p, std::size_t) { construct_element(p, value); });
    }

    void insert(std::size_t index, T&& value) {
        if (points_into(&value, this->m_data, this->m_size)) {
            T tmp(static_cast<T&&>(value));
            insert_gap(index, 1, [&](T* p, std::size_t) { construct_element(p, static_cast<T&&>(tmp)); });
            return;
        }
        insert_gap(index, 1, [&](T* p, std::size_t) { construct_element(p, static_cast<T&&>(value)); });
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U> && !std::is_same<typename std::decay<U>::type, T>::value && !std::is_same<typename std::decay<U>::type, Derived>::value>::type
    insert(std::size_t index, U&& value) { insert(index, static_cast<T>(static_cast<U&&>(value))); }

    void insert(std::size_t index, const Derived& arr) { insert(index, arr.data(), arr.size()); }

    void insert(std::size_t index, Derived&& arr) {
        if (&arr == &self()) { insert(index, arr.data(), arr.size()); return; }
        T* src = arr.data();
        insert_gap(index, arr.size(), [&](T* p, std::size_t i) { construct_element(p, static_cast<T&&>(src[i])); });
    }

    void insert(std::size_t index, const T* src, std::size_t count) {
        if (count == 0) return;
        if (points_into(src, this->m_data, this->m_capacity) || points_into(src + count - 1, this->m_data, this->m_capacity)) {
            DynamicArray<T> copy(src, src + count);
            insert(index, copy.data(), count);
            return;
        }
        insert_gap(index, count, [&](T* p, std::size_t i) { construct_element(p, src[i]); });
    }

    void insert(std::size_t index, const Derived& src, std::size_t src_start, std::size_t count) {
        if (src_start >= src.size()) return;
        if (count > src.size() - src_start) count = src.size() - src_start;
        insert(index, src.data() + src_start, count);
    }

    template <typename It, typename = typename std::enable_if<is_iterator_like<It>::value && !std::is_pointer<It>::value>::type>
    void insert(std::size_t index, It first, It last) {
        DynamicArray<T> tmp(first, last);
        insert(index, tmp.data(), tmp.size());
    }

    void insert(std::size_t index, std::initializer_list<T> values) { insert(index, values.begin(), values.size()); }

    void insert_n(std::size_t index, std::size_t count, const T& value) {
        const T v(value);
        insert_gap(index, count, [&](T* p, std::size_t) { construct_element(p, v); });
    }

    void insert_unordered(std::size_t index, const T& value) {
        T tmp(value);
        insert_unordered(index, static_cast<T&&>(tmp));
    }

    void insert_unordered(std::size_t index, T&& value) {
        if (index >= this->m_size) { emplace_back(static_cast<T&&>(value)); return; }
        T tmp(static_cast<T&&>(value));
        ensure_room(1);
        construct_element(this->m_data + this->m_size, static_cast<T&&>(this->m_data[index]));
        this->m_data[index] = static_cast<T&&>(tmp);
        ++this->m_size;
    }

    template <typename U, typename = enable_convertible<U>>
    void insert_unordered(std::size_t index, U&& value) { insert_unordered(index, static_cast<T>(static_cast<U&&>(value))); }

    void append(const T* src, std::size_t count) {
        if (count == 0) return;
        if (points_into(src, this->m_data, this->m_size)) {
            const std::size_t offset = static_cast<std::size_t>(src - this->m_data);
            ensure_room(count);
            T* d = this->m_data;
            for (std::size_t i = 0; i < count; ++i) construct_element(d + this->m_size + i, d[offset + i]);
            this->m_size += count;
            return;
        }
        ensure_room(count);
        construct_copy_n(this->m_data + this->m_size, src, count);
        this->m_size += count;
    }

    template <typename It, typename = typename std::enable_if<is_iterator_like<It>::value && !std::is_pointer<It>::value>::type>
    void append(It first, It last) {
        if (is_random_access_like<It>::value) {
            const std::size_t n = static_cast<std::size_t>(last - first);
            ensure_room(n);
            this->m_size += construct_copy_range(this->m_data + this->m_size, first, last);
            return;
        }
        for (; !(first == last); ++first) emplace_back(*first);
    }

    void append(std::initializer_list<T> values) { append(values.begin(), values.size()); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value>::type append(const C& c) { append(c.data(), static_cast<std::size_t>(c.size())); }

    template <typename C>
    typename std::enable_if<!is_contiguous_of<C, T>::value && is_range_of<C, T>::value>::type append(const C& c) { append(c.begin(), c.end()); }

    void append_n(std::size_t count, const T& value) {
        if (count == 0) return;
        const T v(value);
        ensure_room(count);
        construct_fill_n(this->m_data + this->m_size, count, v);
        this->m_size += count;
    }

    void prepend(const T* src, std::size_t count) { insert(0, src, count); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value>::type prepend(const C& c) { insert(0, c.data(), static_cast<std::size_t>(c.size())); }

    void assign(std::size_t count, const T& value) {
        const T v(value);
        this->fz_clear();
        append_n(count, v);
    }

    void assign(const T* src, std::size_t count) {
        if (points_into(src, this->m_data, this->m_size)) {
            DynamicArray<T> copy(src, src + count);
            assign(copy.data(), count);
            return;
        }
        const std::size_t common = count < this->m_size ? count : this->m_size;
        copy_assign_n(this->m_data, src, common);
        if (count < this->m_size) truncate(count);
        else append(src + common, count - common);
    }

    template <typename It, typename = typename std::enable_if<is_iterator_like<It>::value && !std::is_pointer<It>::value>::type>
    void assign(It first, It last) {
        this->fz_clear();
        append(first, last);
    }

    void assign(std::initializer_list<T> values) { assign(values.begin(), values.size()); }

    template <typename C>
    typename std::enable_if<is_contiguous_of<C, T>::value && !std::is_same<C, Derived>::value>::type assign(const C& c) { assign(c.data(), static_cast<std::size_t>(c.size())); }

    T pop_back() {
        T value(static_cast<T&&>(this->m_data[this->m_size - 1]));
        truncate(this->m_size - 1);
        return value;
    }

    Derived pop_back(std::size_t from) {
        if (from >= this->m_size) return Derived();
        return pop(from, this->m_size);
    }

    Derived pop_back_count(std::size_t count) {
        if (count == 0) return Derived();
        if (count > this->m_size) count = this->m_size;
        return pop(this->m_size - count, this->m_size);
    }

    T pop_front() { return pop(std::size_t(0)); }

    Derived pop_front(std::size_t end_idx) {
        if (end_idx == 0) return Derived();
        return pop(std::size_t(0), end_idx);
    }

    Derived pop_front_count(std::size_t count) {
        if (count == 0) return Derived();
        return pop(std::size_t(0), count);
    }

    T pop(std::size_t index) {
        T value(static_cast<T&&>(this->m_data[index]));
        erase_range(index, index + 1);
        return value;
    }

    Derived pop(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx) return Derived();
        if (end_idx > this->m_size) end_idx = this->m_size;
        if (start >= end_idx) return Derived();
        Derived result;
        result.reserve(end_idx - start);
        construct_move_n(result.data(), this->m_data + start, end_idx - start);
        set_length(result, end_idx - start);
        erase_range(start, end_idx);
        return result;
    }

    T pop_unordered(std::size_t index) {
        T value(static_cast<T&&>(this->m_data[index]));
        const std::size_t last = this->m_size - 1;
        if (index != last) this->m_data[index] = static_cast<T&&>(this->m_data[last]);
        truncate(last);
        return value;
    }

    Derived pop_unordered(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx) return Derived();
        if (end_idx > this->m_size) end_idx = this->m_size;
        if (start >= end_idx) return Derived();
        const std::size_t count = end_idx - start;
        Derived result;
        result.reserve(count);
        construct_move_n(result.data(), this->m_data + start, count);
        set_length(result, count);
        const std::size_t after = this->m_size - end_idx;
        const std::size_t backfill = count < after ? count : after;
        move_assign_n(this->m_data + start, this->m_data + (this->m_size - backfill), backfill);
        truncate(this->m_size - count);
        return result;
    }

    Derived pop_by_value(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        const T target(value);
        return pop_if([&](const T& v) { return v == target; }, count, dir);
    }

    template <typename Pred>
    Derived pop_if(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        IndexList idx;
        collect_matching(pred, count, dir, idx);
        return pop_at(idx.data(), idx.size());
    }

    Derived pop_by_value_unordered(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        const T target(value);
        return pop_if_unordered([&](const T& v) { return v == target; }, count, dir);
    }

    template <typename Pred>
    Derived pop_if_unordered(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        IndexList idx;
        collect_matching(pred, count, dir, idx);
        return pop_at_unordered(idx.data(), idx.size());
    }

    void remove_back() { truncate(this->m_size - 1); }
    void remove_back(std::size_t count) { truncate(count >= this->m_size ? 0 : this->m_size - count); }
    void remove_front() { erase_range(0, 1); }
    void remove_front(std::size_t count) { erase_range(0, count < this->m_size ? count : this->m_size); }
    void remove(std::size_t index) { erase_range(index, index + 1); }

    void remove_range(std::size_t start, std::size_t end_idx) {
        if (end_idx > this->m_size) end_idx = this->m_size;
        erase_range(start, end_idx);
    }

    void remove_unordered(std::size_t index) {
        const std::size_t last = this->m_size - 1;
        if (index != last) this->m_data[index] = static_cast<T&&>(this->m_data[last]);
        truncate(last);
    }

    std::size_t remove_by_value(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        const T target(value);
        return remove_if([&](const T& v) { return v == target; }, count, dir);
    }

    template <typename Pred>
    std::size_t remove_if(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        if (count == 0 || dir == Direction::All) {
            T* d = this->m_data;
            const std::size_t n = this->m_size;
            std::size_t write = 0;
            while (write < n && !pred(d[write])) ++write;
            for (std::size_t read = write + 1; read < n; ++read) {
                if (!pred(d[read])) d[write++] = static_cast<T&&>(d[read]);
            }
            const std::size_t removed = n - (write < n ? write : n);
            truncate(write < n ? write : n);
            return removed;
        }
        IndexList idx;
        collect_matching(pred, count, dir, idx);
        remove_at(idx.data(), idx.size());
        return idx.size();
    }

    template <typename Pred>
    std::size_t retain_if(Pred pred) { return remove_if([&](const T& v) { return !pred(v); }); }

    std::size_t remove_by_value_unordered(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        const T target(value);
        return remove_if_unordered([&](const T& v) { return v == target; }, count, dir);
    }

    template <typename Pred>
    std::size_t remove_if_unordered(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        IndexList idx;
        collect_matching(pred, count, dir, idx);
        remove_at_unordered(idx.data(), idx.size());
        return idx.size();
    }

    void remove_at(const std::size_t* idx, std::size_t idx_count) {
        if (idx_count == 0) return;
        T* d = this->m_data;
        const std::size_t n = this->m_size;
        std::size_t j = 0;
        std::size_t write = idx[0];
        for (std::size_t read = idx[0]; read < n; ++read) {
            if (j < idx_count && read == idx[j]) { ++j; while (j < idx_count && idx[j] == read) ++j; }
            else d[write++] = static_cast<T&&>(d[read]);
        }
        truncate(write);
    }

    void remove_at_unordered(const std::size_t* idx, std::size_t idx_count) {
        for (std::size_t i = idx_count; i > 0; --i) {
            const std::size_t index = idx[i - 1];
            if (index >= this->m_size) continue;
            if (i < idx_count && idx[i] == index) continue;
            remove_unordered(index);
        }
    }

    Derived pop_at(const std::size_t* idx, std::size_t idx_count) {
        Derived result;
        if (idx_count == 0) return result;
        result.reserve(idx_count);
        T* d = this->m_data;
        const std::size_t n = this->m_size;
        std::size_t j = 0, write = idx[0];
        for (std::size_t read = idx[0]; read < n; ++read) {
            if (j < idx_count && read == idx[j]) {
                result.emplace_back(static_cast<T&&>(d[read]));
                ++j;
                while (j < idx_count && idx[j] == read) ++j;
            } else {
                d[write++] = static_cast<T&&>(d[read]);
            }
        }
        truncate(write);
        return result;
    }

    Derived pop_at_unordered(const std::size_t* idx, std::size_t idx_count) {
        Derived result;
        result.reserve(idx_count);
        for (std::size_t i = idx_count; i > 0; --i) {
            const std::size_t index = idx[i - 1];
            if (index >= this->m_size) continue;
            if (i < idx_count && idx[i] == index) continue;
            result.emplace_back(pop_unordered(index));
        }
        return result;
    }

    void take(Derived& src) {
        if (&src == &self() || src.size() == 0) return;
        push_back(static_cast<Derived&&>(src));
        src.clear();
    }

    void take(std::size_t index, Derived& src) {
        if (&src == &self() || src.size() == 0) return;
        insert(index, static_cast<Derived&&>(src));
        src.clear();
    }

    void take(Derived& src, std::size_t src_index) {
        if (src_index >= src.size()) return;
        if (&src == &self()) { T v = pop(src_index); emplace_back(static_cast<T&&>(v)); return; }
        emplace_back(src.pop(src_index));
    }

    void take(Derived& src, std::size_t src_index, std::size_t dst_index) {
        if (src_index >= src.size()) return;
        T value = src.pop(src_index);
        insert(dst_index, static_cast<T&&>(value));
    }

    void take_range(Derived& src, std::size_t start, std::size_t end_idx) {
        if (start >= end_idx || start >= src.size()) return;
        Derived extracted = src.pop(start, end_idx);
        push_back(static_cast<Derived&&>(extracted));
    }

    void take_range(Derived& src, std::size_t start, std::size_t end_idx, std::size_t dst_index) {
        if (start >= end_idx || start >= src.size()) return;
        Derived extracted = src.pop(start, end_idx);
        insert(dst_index, static_cast<Derived&&>(extracted));
    }

    void unique() {
        if (this->m_size <= 1) return;
        T* d = this->m_data;
        std::size_t write = 1;
        for (std::size_t i = 1, n = this->m_size; i < n; ++i) {
            if (!(d[i] == d[write - 1])) {
                if (write != i) d[write] = static_cast<T&&>(d[i]);
                ++write;
            }
        }
        truncate(write);
    }

    template <typename Pred>
    void unique(Pred pred) {
        if (this->m_size <= 1) return;
        T* d = this->m_data;
        std::size_t write = 1;
        for (std::size_t i = 1, n = this->m_size; i < n; ++i) {
            if (!pred(d[write - 1], d[i])) {
                if (write != i) d[write] = static_cast<T&&>(d[i]);
                ++write;
            }
        }
        truncate(write);
    }

    void deduplicate() { deduplicate_impl(hashing::is_hashable<T>()); }

    template <typename Pred>
    void deduplicate(Pred eq) {
        T* d = this->m_data;
        std::size_t write = 0;
        for (std::size_t i = 0, n = this->m_size; i < n; ++i) {
            bool seen = false;
            for (std::size_t j = 0; j < write; ++j) if (eq(d[j], d[i])) { seen = true; break; }
            if (seen) continue;
            if (write != i) d[write] = static_cast<T&&>(d[i]);
            ++write;
        }
        truncate(write);
    }

private:
    void deduplicate_impl(std::true_type) {
        T* d = this->m_data;
        const std::size_t n = this->m_size;
        if (n < kHashSetThreshold) { deduplicate_impl(std::false_type()); return; }
        hashing::IndexSet<T> seen(d, n);
        hashing::Hasher<T> h;
        std::size_t write = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t hv = h(d[i]);
            if (seen.find(d[i], hv) != npos) continue;
            if (write != i) d[write] = static_cast<T&&>(d[i]);
            seen.insert_new(write, hv);
            ++write;
        }
        truncate(write);
    }

    void deduplicate_impl(std::false_type) {
        T* d = this->m_data;
        std::size_t write = 0;
        for (std::size_t i = 0, n = this->m_size; i < n; ++i) {
            bool seen = false;
            for (std::size_t j = 0; j < write; ++j) if (d[j] == d[i]) { seen = true; break; }
            if (seen) continue;
            if (write != i) d[write] = static_cast<T&&>(d[i]);
            ++write;
        }
        truncate(write);
    }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_OPS_EDIT_OPS_HPP
