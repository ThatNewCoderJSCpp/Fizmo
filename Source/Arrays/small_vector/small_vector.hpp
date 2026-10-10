#ifndef FIZMO_SMALL_VECTOR_CONTAINER_HPP
#define FIZMO_SMALL_VECTOR_CONTAINER_HPP

#include "../dynamic_array/dynamic_array.hpp"
#include "impl/storage.hpp"
#include "../common/ops/access_ops.hpp"
#include "../common/ops/container_ops.hpp"

namespace fizmo {

template <typename T, std::size_t N>
class SmallVector;

namespace arrays {
namespace detail {

template <typename T, std::size_t N>
using SmallVectorBase =
    ContainerOps<SmallVector<T, N>, T,
    FunctionalOps<SmallVector<T, N>, T,
    EditOps<SmallVector<T, N>, T,
    InplaceOps<SmallVector<T, N>, T,
    QueryOps<SmallVector<T, N>, T,
    AccessOps<SmallVector<T, N>, T,
    InlineStorage<T, N>>>>>>>;

} // namespace detail
} // namespace arrays

template <typename T, std::size_t N = 8>
class SmallVector : public arrays::detail::SmallVectorBase<T, N> {
    static_assert(N > 0, "SmallVector inline capacity must be >= 1");
    using Base = arrays::detail::SmallVectorBase<T, N>;
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
    using value_type_array = SmallVector<T, N>;
    template <typename U>
    using rebind = SmallVector<U, N>;
    static constexpr std::size_t inline_capacity = N;

    using Base::Base;
    using Base::operator=;
    using Base::swap;
    using Base::copy_from;

    SmallVector() noexcept = default;

    SmallVector(const SmallVector& other) : Base() { copy_construct_from(other.m_data, other.m_size); }

    SmallVector(SmallVector&& other) noexcept(ET::relocatable || std::is_nothrow_move_constructible<T>::value) : Base() { this->fz_steal(other); }

    ~SmallVector() = default;

    SmallVector& operator=(const SmallVector& other) {
        if (this == &other) return *this;
        assign_from(other.m_data, other.m_size);
        return *this;
    }

    SmallVector& operator=(SmallVector&& other) noexcept(ET::relocatable || std::is_nothrow_move_constructible<T>::value) {
        if (this == &other) return *this;
        this->fz_free();
        this->fz_steal(other);
        return *this;
    }

    SmallVector& operator=(std::initializer_list<T> init) {
        assign_from(init.begin(), init.size());
        return *this;
    }

    static SmallVector with_capacity(std::size_t capacity) {
        SmallVector a;
        a.reserve(capacity);
        return a;
    }

    bool is_using_inline_storage() const noexcept { return this->fz_is_inline(); }

    bool reserve(std::size_t new_capacity) {
        if (new_capacity <= this->m_capacity) return false;
        this->fz_reserve_exact(new_capacity);
        return true;
    }
    void reserve_more(std::size_t additional) { this->fz_reserve_total(this->m_size + additional); }
    bool ensure_capacity(std::size_t min_capacity) {
        if (min_capacity <= this->m_capacity) return false;
        this->fz_reserve_total(min_capacity);
        return true;
    }
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

    bool resize(std::size_t new_size) {
        if (new_size <= this->m_size) { arrays::detail::destroy_n(this->m_data + new_size, this->m_size - new_size); this->m_size = new_size; return true; }
        this->fz_reserve_total(new_size);
        arrays::detail::construct_default_n(this->m_data + this->m_size, new_size - this->m_size);
        this->m_size = new_size;
        return true;
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U>, bool>::type
    resize(std::size_t new_size, U&& value) {
        if (new_size <= this->m_size) { arrays::detail::destroy_n(this->m_data + new_size, this->m_size - new_size); this->m_size = new_size; return true; }
        const T copy(static_cast<U&&>(value));
        this->fz_reserve_total(new_size);
        arrays::detail::construct_fill_n(this->m_data + this->m_size, new_size - this->m_size, copy);
        this->m_size = new_size;
        return true;
    }

    template <typename U = T>
    bool resize_uninit(std::size_t new_size) {
        static_assert(std::is_trivially_copyable<U>::value, "resize_uninit requires a trivially copyable T");
        if (new_size > this->m_capacity) this->fz_reserve_total(new_size);
        this->m_size = new_size;
        return true;
    }

    template <typename U = T>
    bool assign_uninit(std::size_t new_size) {
        static_assert(std::is_trivially_copyable<U>::value, "assign_uninit requires a trivially copyable T");
        if (new_size > this->m_capacity) {
            this->fz_free();
            this->fz_reserve_exact(new_size);
        }
        this->m_size = new_size;
        return true;
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
    bool shrink() { return this->fz_shrink(); }
    bool shrink_to_fit() { return this->fz_shrink(); }

    T* release() {
        T* out = new T[this->m_size ? this->m_size : 1];
        arrays::detail::move_assign_n(out, this->m_data, this->m_size);
        this->fz_free();
        return out;
    }

    void swap(SmallVector& other) noexcept(ET::relocatable || std::is_nothrow_move_constructible<T>::value) { this->fz_swap(other); }
    friend void swap(SmallVector& a, SmallVector& b) noexcept(ET::relocatable || std::is_nothrow_move_constructible<T>::value) { a.fz_swap(b); }

    void copy_from(const SmallVector& src) { if (this != &src) assign_from(src.m_data, src.m_size); }

    void assign_from(const T* src, std::size_t n) {
        if (n > this->m_capacity) {
            SmallVector tmp;
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

#endif // FIZMO_SMALL_VECTOR_CONTAINER_HPP
