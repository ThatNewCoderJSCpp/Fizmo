#ifndef FIZMO_ARRAYS_VIEWS_ARRAY_VIEW_HPP
#define FIZMO_ARRAYS_VIEWS_ARRAY_VIEW_HPP

#include "../ops/inplace_ops.hpp"
#include "../../array_iterators.hpp"

namespace fizmo {
namespace arrays {
namespace detail {

template <typename T>
class ViewBase {
protected:
    const T*    m_data = nullptr;
    std::size_t m_size = 0;

    const T* fz_cptr() const noexcept { return m_data; }
    std::size_t fz_len() const noexcept { return m_size; }
};

template <typename T>
class SpanBase {
protected:
    T*          m_data = nullptr;
    std::size_t m_size = 0;

    T* fz_ptr() noexcept { return m_data; }
    const T* fz_cptr() const noexcept { return m_data; }
    std::size_t fz_len() const noexcept { return m_size; }
};

} // namespace detail
} // namespace arrays

template <typename T>
class ArraySpan;

template <typename T>
class ArrayView : public arrays::detail::QueryOps<ArrayView<T>, T, arrays::detail::ViewBase<T>> {
public:
    using value_type = T;
    using value_type_array = DynamicArray<T>;
    using iterator = ConstArrayIterator<T>;
    using const_iterator = ConstArrayIterator<T>;

    ArrayView() noexcept = default;
    ArrayView(const T* data, std::size_t size) noexcept { this->m_data = data; this->m_size = size; }
    ArrayView(std::initializer_list<T> init) noexcept { this->m_data = init.begin(); this->m_size = init.size(); }

    template <typename C, typename = typename std::enable_if<arrays::detail::is_contiguous_of<C, T>::value && !std::is_same<typename std::decay<C>::type, ArrayView>::value>::type>
    ArrayView(const C& c) noexcept { this->m_data = c.data(); this->m_size = static_cast<std::size_t>(c.size()); }

    std::size_t size() const noexcept { return this->m_size; }
    bool empty() const noexcept { return this->m_size == 0; }
    const T& operator[](std::size_t index) const noexcept { return index < this->m_size ? this->m_data[index] : this->m_data[this->m_size - 1]; }
    const T* at(std::size_t index) const noexcept { return index < this->m_size ? &this->m_data[index] : nullptr; }
    const T& front() const noexcept { return this->m_data[0]; }
    const T& back() const noexcept { return this->m_data[this->m_size - 1]; }
    const T* data() const noexcept { return this->m_data; }
    ConstArrayIterator<T> begin() const noexcept { return ConstArrayIterator<T>(this->m_data); }
    ConstArrayIterator<T> end() const noexcept { return ConstArrayIterator<T>(this->m_data + this->m_size); }
    ConstArrayIterator<T> cbegin() const noexcept { return begin(); }
    ConstArrayIterator<T> cend() const noexcept { return end(); }
    ConstReverseArrayIterator<T> rbegin() const noexcept { return ConstReverseArrayIterator<T>(this->m_data + this->m_size); }
    ConstReverseArrayIterator<T> rend() const noexcept { return ConstReverseArrayIterator<T>(this->m_data); }

    ArrayView subview(std::size_t start, std::size_t end_idx) const noexcept {
        if (!arrays::detail::clamp_range(start, end_idx, this->m_size)) return ArrayView();
        return ArrayView(this->m_data + start, end_idx - start);
    }

    ArrayView first(std::size_t n) const noexcept { return ArrayView(this->m_data, n < this->m_size ? n : this->m_size); }
    ArrayView last(std::size_t n) const noexcept { const std::size_t c = n < this->m_size ? n : this->m_size; return ArrayView(this->m_data + (this->m_size - c), c); }
    ArrayView drop_front(std::size_t n) const noexcept { return n >= this->m_size ? ArrayView() : ArrayView(this->m_data + n, this->m_size - n); }
    ArrayView drop_back(std::size_t n) const noexcept { return n >= this->m_size ? ArrayView() : ArrayView(this->m_data, this->m_size - n); }

    template <typename C>
    typename std::enable_if<arrays::detail::is_contiguous_of<C, T>::value, bool>::type operator==(const C& other) const { return this->equals(other); }
    template <typename C>
    typename std::enable_if<arrays::detail::is_contiguous_of<C, T>::value, bool>::type operator!=(const C& other) const { return !this->equals(other); }
};

template <typename T>
class ArraySpan : public arrays::detail::InplaceOps<ArraySpan<T>, T, arrays::detail::QueryOps<ArraySpan<T>, T, arrays::detail::SpanBase<T>>> {
public:
    using value_type = T;
    using value_type_array = DynamicArray<T>;
    using iterator = ArrayIterator<T>;
    using const_iterator = ConstArrayIterator<T>;

    ArraySpan() noexcept = default;
    ArraySpan(T* data, std::size_t size) noexcept { this->m_data = data; this->m_size = size; }

    template <typename C, typename = typename std::enable_if<arrays::detail::is_mutable_contiguous_of<C, T>::value && !std::is_same<typename std::decay<C>::type, ArraySpan>::value>::type>
    ArraySpan(C& c) noexcept { this->m_data = c.data(); this->m_size = static_cast<std::size_t>(c.size()); }

    std::size_t size() const noexcept { return this->m_size; }
    bool empty() const noexcept { return this->m_size == 0; }
    T& operator[](std::size_t index) const noexcept { return this->m_data[index]; }
    T* at(std::size_t index) const noexcept { return index < this->m_size ? &this->m_data[index] : nullptr; }
    T& front() const noexcept { return this->m_data[0]; }
    T& back() const noexcept { return this->m_data[this->m_size - 1]; }
    T* data() const noexcept { return this->m_data; }
    ArrayIterator<T> begin() const noexcept { return ArrayIterator<T>(this->m_data); }
    ArrayIterator<T> end() const noexcept { return ArrayIterator<T>(this->m_data + this->m_size); }
    ReverseArrayIterator<T> rbegin() const noexcept { return ReverseArrayIterator<T>(this->m_data + this->m_size); }
    ReverseArrayIterator<T> rend() const noexcept { return ReverseArrayIterator<T>(this->m_data); }

    ArraySpan subspan(std::size_t start, std::size_t end_idx) const noexcept {
        if (!arrays::detail::clamp_range(start, end_idx, this->m_size)) return ArraySpan();
        return ArraySpan(this->m_data + start, end_idx - start);
    }

    ArrayView<T> view() const noexcept { return ArrayView<T>(this->m_data, this->m_size); }
    operator ArrayView<T>() const noexcept { return view(); }
};

} // namespace fizmo

#endif // FIZMO_ARRAYS_VIEWS_ARRAY_VIEW_HPP
