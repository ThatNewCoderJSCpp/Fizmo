#ifndef FIZMO_DYNAMIC_ARRAY_HPP
#define FIZMO_DYNAMIC_ARRAY_HPP

#include "impl/storage.hpp"
#include "../common/ops/access_ops.hpp"
#include "../common/ops/container_ops.hpp"

namespace fizmo {

template <typename T>
class DynamicArray;

namespace arrays {
namespace detail {

template <typename T>
using DynamicArrayBase =
    ContainerOps<DynamicArray<T>, T,
    FunctionalOps<DynamicArray<T>, T,
    EditOps<DynamicArray<T>, T,
    InplaceOps<DynamicArray<T>, T,
    QueryOps<DynamicArray<T>, T,
    AccessOps<DynamicArray<T>, T,
    HeapStorage<T>>>>>>>;

} // namespace detail
} // namespace arrays

template <typename T>
class DynamicArray : public arrays::detail::DynamicArrayBase<T> {
    using Base = arrays::detail::DynamicArrayBase<T>;
    using ET = arrays::detail::ElementTraits<T>;

    template <typename D, typename V, typename B>
    friend class arrays::detail::AccessOps;
    template <typename D, typename V, typename B>
    friend class arrays::detail::QueryOps;
    template <typename D, typename V, typename B>
    friend class arrays::detail::InplaceOps;
    template <typename D, typename V, typename B>
    friend class arrays::detail::EditOps;
    template <typename D, typename V, typename B>
    friend class arrays::detail::FunctionalOps;
    template <typename D, typename V, typename B>
    friend class arrays::detail::ContainerOps;
    template <typename C>
    friend class fizmo::BasicWhereProxy;

    void copy_construct_from(const T* src, std::size_t n) {
        if (n == 0) return;
        this->fz_reserve_exact(n);
        arrays::detail::construct_copy_n(this->m_data, src, n);
        this->m_size = n;
    }

public:
    using value_type = T;
    using value_type_array = DynamicArray<T>;
    template <typename U>
    using rebind = DynamicArray<U>;

    using Base::Base;
    using Base::operator=;
    using Base::swap;
    using Base::copy_from;

    DynamicArray() noexcept = default;

    DynamicArray(const DynamicArray& other) : Base() { copy_construct_from(other.m_data, other.m_size); }

    DynamicArray(DynamicArray&& other) noexcept : Base() { this->fz_steal(other); }

    ~DynamicArray() = default;

    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) return *this;
        assign_from(other.m_data, other.m_size);
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this == &other) return *this;
        this->fz_free();
        this->fz_steal(other);
        return *this;
    }

    DynamicArray& operator=(std::initializer_list<T> init) {
        assign_from(init.begin(), init.size());
        return *this;
    }

    static DynamicArray with_capacity(std::size_t capacity) {
        DynamicArray a;
        a.reserve(capacity);
        return a;
    }

    void reserve(std::size_t new_capacity) { this->fz_reserve_exact(new_capacity); }
    void reserve_more(std::size_t additional) { this->fz_reserve_total(this->m_size + additional); }
    void ensure_capacity(std::size_t min_capacity) { this->fz_reserve_total(min_capacity); }
    bool needs_reallocation(std::size_t count = 1) const noexcept { return count > this->m_capacity - this->m_size; }

    bool try_reserve(std::size_t new_capacity) noexcept {
#if defined(FIZMO_ARRAYS_EXCEPTIONS)
        try { this->fz_reserve_exact(new_capacity); return true; }
        catch (...) { return false; }
#else
        this->fz_reserve_exact(new_capacity);
        return this->m_capacity >= new_capacity;
#endif
    }

    void resize(std::size_t new_size) {
        if (new_size <= this->m_size) { arrays::detail::destroy_n(this->m_data + new_size, this->m_size - new_size); this->m_size = new_size; return; }
        this->fz_reserve_total(new_size);
        arrays::detail::construct_default_n(this->m_data + this->m_size, new_size - this->m_size);
        this->m_size = new_size;
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U>>::type
    resize(std::size_t new_size, U&& value) {
        if (new_size <= this->m_size) { arrays::detail::destroy_n(this->m_data + new_size, this->m_size - new_size); this->m_size = new_size; return; }
        const T copy(static_cast<U&&>(value));
        this->fz_reserve_total(new_size);
        arrays::detail::construct_fill_n(this->m_data + this->m_size, new_size - this->m_size, copy);
        this->m_size = new_size;
    }

    template <typename U = T>
    void resize_uninit(std::size_t new_size) {
        static_assert(std::is_trivially_copyable<U>::value, "resize_uninit requires a trivially copyable T");
        if (new_size > this->m_capacity) this->fz_reserve_total(new_size);
        this->m_size = new_size;
    }

    template <typename U = T>
    void assign_uninit(std::size_t new_size) {
        static_assert(std::is_trivially_copyable<U>::value, "assign_uninit requires a trivially copyable T");
        if (new_size > this->m_capacity) {
            this->fz_free();
            this->fz_reserve_exact(new_size);
        }
        this->m_size = new_size;
    }

    void set_size_unchecked(std::size_t new_size) noexcept(std::is_nothrow_default_constructible<T>::value) {
        if (!std::is_trivially_copyable<T>::value || !std::is_trivially_destructible<T>::value) {
            if (new_size < this->m_size) arrays::detail::destroy_n(this->m_data + new_size, this->m_size - new_size);
            else if (new_size > this->m_size) arrays::detail::construct_default_n(this->m_data + this->m_size, new_size - this->m_size);
        }
        this->m_size = new_size;
    }

    void clear() noexcept { this->fz_clear(); }
    void deallocate() noexcept { this->fz_free(); }
    void shrink() { this->fz_shrink(); }
    void shrink_to_fit() { this->fz_shrink(); }

    T* release() {
        if (!this->m_data) return nullptr;
        T* out = new T[this->m_size ? this->m_size : 1];
        arrays::detail::move_assign_n(out, this->m_data, this->m_size);
        this->fz_free();
        return out;
    }

    void swap(DynamicArray& other) noexcept { this->fz_swap(other); }
    friend void swap(DynamicArray& a, DynamicArray& b) noexcept { a.fz_swap(b); }

    void copy_from(const DynamicArray& src) { if (this != &src) assign_from(src.m_data, src.m_size); }

    void assign_from(const T* src, std::size_t n) {
        if (n > this->m_capacity) {
            DynamicArray tmp;
            tmp.copy_construct_from(src, n);
            this->fz_swap(tmp);
            return;
        }
        const std::size_t common = n < this->m_size ? n : this->m_size;
        arrays::detail::copy_assign_n(this->m_data, src, common);
        if (n > this->m_size) arrays::detail::construct_copy_n(this->m_data + this->m_size, src + this->m_size, n - this->m_size);
        else arrays::detail::destroy_n(this->m_data + n, this->m_size - n);
        this->m_size = n;
    }
};

} // namespace fizmo

#include "../common/where_proxy.hpp"
#include "../common/stream.hpp"

#endif // FIZMO_DYNAMIC_ARRAY_HPP
