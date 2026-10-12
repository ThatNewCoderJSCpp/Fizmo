#ifndef FIZMO_ARRAYS_OPS_ACCESS_OPS_HPP
#define FIZMO_ARRAYS_OPS_ACCESS_OPS_HPP

#include "../traits.hpp"
#include "../memory/allocator.hpp"
#include "../../../Collections/iterator.hpp"
#include <stdexcept>
#include <string>

namespace fizmo {
namespace arrays {
namespace detail {

template <typename Derived, typename T, typename Base>
class AccessOps : public Base {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = collections::ArrayIterator<T>;
    using const_iterator = collections::ConstArrayIterator<T>;
    using reverse_iterator = collections::ReverseArrayIterator<T>;
    using const_reverse_iterator = collections::ConstReverseArrayIterator<T>;

    std::size_t size() const noexcept { return this->m_size; }
    std::size_t capacity() const noexcept { return this->m_capacity; }
    T* data() noexcept { return this->m_data; }
    const T* data() const noexcept { return this->m_data; }
    std::size_t max_index() const noexcept { return this->m_size - 1; }
    std::size_t middle_index() const noexcept { return this->m_size / 2; }
    bool empty() const noexcept { return this->m_size == 0; }
    std::size_t max_size() const noexcept { return memory::max_elements<T>(); }
    std::size_t remaining_capacity() const noexcept { return this->m_capacity - this->m_size; }
    bool is_valid_index(std::size_t index) const noexcept { return index < this->m_size; }

    T& operator[](std::size_t index) noexcept { return this->m_data[index]; }
    const T& operator[](std::size_t index) const noexcept { return this->m_data[index]; }
    T* at(std::size_t index) noexcept { return index < this->m_size ? &this->m_data[index] : nullptr; }
    const T* at(std::size_t index) const noexcept { return index < this->m_size ? &this->m_data[index] : nullptr; }

    T get(std::size_t index) const {
        if (index >= this->m_size) throw std::out_of_range("array get: index " + std::to_string(index) + " out of range for size " + std::to_string(this->m_size));
        return this->m_data[index];
    }

    T& checked(std::size_t index) {
        if (index >= this->m_size) throw std::out_of_range("array checked: index " + std::to_string(index) + " out of range for size " + std::to_string(this->m_size));
        return this->m_data[index];
    }

    const T& checked(std::size_t index) const {
        if (index >= this->m_size) throw std::out_of_range("array checked: index " + std::to_string(index) + " out of range for size " + std::to_string(this->m_size));
        return this->m_data[index];
    }

    T get_or(std::size_t index, const T& default_value = T{}) const { return index < this->m_size ? this->m_data[index] : default_value; }
    T& front() noexcept { return this->m_data[0]; }
    const T& front() const noexcept { return this->m_data[0]; }
    T& middle() noexcept { return this->m_data[this->m_size / 2]; }
    const T& middle() const noexcept { return this->m_data[this->m_size / 2]; }
    T& back() noexcept { return this->m_data[this->m_size - 1]; }
    const T& back() const noexcept { return this->m_data[this->m_size - 1]; }

    collections::ArrayIterator<T> begin() noexcept { return collections::ArrayIterator<T>(this->m_data); }
    collections::ArrayIterator<T> end() noexcept { return collections::ArrayIterator<T>(this->m_data + this->m_size); }
    collections::ConstArrayIterator<T> begin() const noexcept { return collections::ConstArrayIterator<T>(this->m_data); }
    collections::ConstArrayIterator<T> end() const noexcept { return collections::ConstArrayIterator<T>(this->m_data + this->m_size); }
    collections::ConstArrayIterator<T> cbegin() const noexcept { return collections::ConstArrayIterator<T>(this->m_data); }
    collections::ConstArrayIterator<T> cend() const noexcept { return collections::ConstArrayIterator<T>(this->m_data + this->m_size); }
    collections::ReverseArrayIterator<T> rbegin() noexcept { return collections::ReverseArrayIterator<T>(this->m_data + this->m_size); }
    collections::ReverseArrayIterator<T> rend() noexcept { return collections::ReverseArrayIterator<T>(this->m_data); }
    collections::ConstReverseArrayIterator<T> rbegin() const noexcept { return collections::ConstReverseArrayIterator<T>(this->m_data + this->m_size); }
    collections::ConstReverseArrayIterator<T> rend() const noexcept { return collections::ConstReverseArrayIterator<T>(this->m_data); }
    collections::ConstReverseArrayIterator<T> crbegin() const noexcept { return collections::ConstReverseArrayIterator<T>(this->m_data + this->m_size); }
    collections::ConstReverseArrayIterator<T> crend() const noexcept { return collections::ConstReverseArrayIterator<T>(this->m_data); }

protected:
    T* fz_ptr() noexcept { return this->m_data; }
    const T* fz_cptr() const noexcept { return this->m_data; }
    std::size_t fz_len() const noexcept { return this->m_size; }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_OPS_ACCESS_OPS_HPP
