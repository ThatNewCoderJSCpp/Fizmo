#ifndef FIZMO_VARIANT_HPP
#define FIZMO_VARIANT_HPP

#include "../Basic/basic_includes.hpp"
#include "../Basic/fizmo_defines.hpp"

namespace fizmo {

class bad_variant_access : public std::runtime_error {
public:
    bad_variant_access() noexcept : std::runtime_error("Bad variant access") {}
    virtual const char* what() const noexcept override {
        return "Bad variant access";
    }
};

template <size_t I, typename... Types>
struct type_at;

template <size_t I, typename T, typename... Rest>
struct type_at<I, T, Rest...> {
    using type = typename type_at<I - 1, Rest...>::type;
};

template <typename T, typename... Rest>
struct type_at<0, T, Rest...> {
    using type = T;
};

template <typename T, typename... Types>
struct index_of;

template <typename T, typename First, typename... Rest>
struct index_of<T, First, Rest...> {
    static constexpr size_t value = std::is_same<T, First>::value ? 0 : index_of<T, Rest...>::value + 1;
};

template <typename T>
struct index_of<T> {
    static constexpr size_t value = 0;
};

template <typename T, typename... Types>
struct contains;

template <typename T>
struct contains<T> {
    static constexpr bool value = false;
};

template <typename T, typename First, typename... Rest>
struct contains<T, First, Rest...> {
    static constexpr bool value = std::is_same<T, First>::value || contains<T, Rest...>::value;
};

template <typename... Types>
struct max_size;

template <typename T>
struct max_size<T> {
    static constexpr size_t value = sizeof(T);
};

template <typename T, typename... Rest>
struct max_size<T, Rest...> {
    static constexpr size_t value = sizeof(T) > max_size<Rest...>::value ? sizeof(T) : max_size<Rest...>::value;
};

template <typename... Types>
struct max_align;

template <typename T>
struct max_align<T> {
    static constexpr size_t value = alignof(T);
};

template <typename T, typename... Rest>
struct max_align<T, Rest...> {
    static constexpr size_t value = alignof(T) > max_align<Rest...>::value ? alignof(T) : max_align<Rest...>::value;
};

template <typename... Types>
class Variant {
private:
    static constexpr size_t storage_size = max_size<Types...>::value;
    static constexpr size_t storage_align = max_align<Types...>::value;
    
    typename std::aligned_storage<storage_size, storage_align>::type storage_;
    size_t type_index_;
    
    template <typename T>
    T* get_ptr() {
        return reinterpret_cast<T*>(&storage_);
    }
    
    template <typename T>
    const T* get_ptr() const {
        return reinterpret_cast<const T*>(&storage_);
    }
    
    template <size_t I>
    void destroy() {
        if (I == type_index_) {
            using T = typename type_at<I, Types...>::type;
            get_ptr<T>()->~T();
        } else {
            destroy_impl<I + 1>();
        }
    }
    
    template <size_t I>
    typename std::enable_if<(I < sizeof...(Types))>::type destroy_impl() {
        destroy<I>();
    }
    
    template <size_t I>
    typename std::enable_if<(I >= sizeof...(Types))>::type destroy_impl() {
    }
    
    void destroy_current() {
        if (type_index_ < sizeof...(Types)) {
            destroy_impl<0>();
        }
    }
    
    template <size_t I>
    void copy_construct(const Variant& other) {
        if (I == other.type_index_) {
            using T = typename type_at<I, Types...>::type;
            new (&storage_) T(*other.get_ptr<T>());
        } else {
            copy_construct_impl<I + 1>(other);
        }
    }
    
    template <size_t I>
    typename std::enable_if<(I < sizeof...(Types))>::type copy_construct_impl(const Variant& other) {
        copy_construct<I>(other);
    }
    
    template <size_t I>
    typename std::enable_if<(I >= sizeof...(Types))>::type copy_construct_impl(const Variant& other) {
    }
    
    template <size_t I>
    void move_construct(Variant&& other) {
        if (I == other.type_index_) {
            using T = typename type_at<I, Types...>::type;
            new (&storage_) T(std::move(*other.get_ptr<T>()));
        } else {
            move_construct_impl<I + 1>(std::move(other));
        }
    }
    
    template <size_t I>
    typename std::enable_if<(I < sizeof...(Types))>::type move_construct_impl(Variant&& other) {
        move_construct<I>(std::move(other));
    }
    
    template <size_t I>
    typename std::enable_if<(I >= sizeof...(Types))>::type move_construct_impl(Variant&& other) {
    }
    
public:
    Variant() : type_index_(0) {
        using FirstType = typename type_at<0, Types...>::type;
        new (&storage_) FirstType();
    }
    
    Variant(const Variant& other) : type_index_(other.type_index_) {
        copy_construct_impl<0>(other);
    }
    
    Variant(Variant&& other) noexcept : type_index_(other.type_index_) {
        move_construct_impl<0>(std::move(other));
    }
    
    template <typename T, typename = typename std::enable_if<contains<typename std::decay<T>::type, Types...>::value>::type>
    Variant(T&& value) : type_index_(index_of<typename std::decay<T>::type, Types...>::value) {
        using DecayedT = typename std::decay<T>::type;
        new (&storage_) DecayedT(std::forward<T>(value));
    }
    
    ~Variant() {
        destroy_current();
    }
    
    Variant& operator=(const Variant& other) {
        if (this != &other) {
            destroy_current();
            type_index_ = other.type_index_;
            copy_construct_impl<0>(other);
        }
        return *this;
    }
    
    Variant& operator=(Variant&& other) noexcept {
        if (this != &other) {
            destroy_current();
            type_index_ = other.type_index_;
            move_construct_impl<0>(std::move(other));
        }
        return *this;
    }
    
    template <typename T>
    typename std::enable_if<contains<typename std::decay<T>::type, Types...>::value, Variant&>::type
    operator=(T&& value) {
        using DecayedT = typename std::decay<T>::type;
        destroy_current();
        type_index_ = index_of<DecayedT, Types...>::value;
        new (&storage_) DecayedT(std::forward<T>(value));
        return *this;
    }
    
    size_t index() const noexcept {
        return type_index_;
    }
    
    bool valueless_by_exception() const noexcept {
        return type_index_ >= sizeof...(Types);
    }
    
    template <typename T, typename... Args>
    T& emplace(Args&&... args) {
        static_assert(contains<T, Types...>::value, "Type not in variant");
        destroy_current();
        type_index_ = index_of<T, Types...>::value;
        new (&storage_) T(std::forward<Args>(args)...);
        return *get_ptr<T>();
    }
    
    template <size_t I, typename... Args>
    typename type_at<I, Types...>::type& emplace(Args&&... args) {
        using T = typename type_at<I, Types...>::type;
        destroy_current();
        type_index_ = I;
        new (&storage_) T(std::forward<Args>(args)...);
        return *get_ptr<T>();
    }
    
    template <size_t I, typename... Ts>
    friend typename type_at<I, Ts...>::type& get(Variant<Ts...>& v);
    
    template <size_t I, typename... Ts>
    friend const typename type_at<I, Ts...>::type& get(const Variant<Ts...>& v);
    
    template <typename T, typename... Ts>
    friend T& get(Variant<Ts...>& v);
    
    template <typename T, typename... Ts>
    friend const T& get(const Variant<Ts...>& v);
    
    template <size_t I, typename... Ts>
    friend typename type_at<I, Ts...>::type* get_if(Variant<Ts...>* v) noexcept;
    
    template <size_t I, typename... Ts>
    friend const typename type_at<I, Ts...>::type* get_if(const Variant<Ts...>* v) noexcept;
    
    template <typename T, typename... Ts>
    friend T* get_if(Variant<Ts...>* v) noexcept;
    
    template <typename T, typename... Ts>
    friend const T* get_if(const Variant<Ts...>* v) noexcept;
};

template <size_t I, typename... Types>
typename type_at<I, Types...>::type& get(Variant<Types...>& v) {
    if (v.type_index_ != I) {
        throw bad_variant_access();
    }
    using T = typename type_at<I, Types...>::type;
    return *v.template get_ptr<T>();
}

template <size_t I, typename... Types>
const typename type_at<I, Types...>::type& get(const Variant<Types...>& v) {
    if (v.type_index_ != I) {
        throw bad_variant_access();
    }
    using T = typename type_at<I, Types...>::type;
    return *v.template get_ptr<T>();
}

template <typename T, typename... Types>
T& get(Variant<Types...>& v) {
    constexpr size_t I = index_of<T, Types...>::value;
    if (v.type_index_ != I) {
        throw bad_variant_access();
    }
    return *v.template get_ptr<T>();
}

template <typename T, typename... Types>
const T& get(const Variant<Types...>& v) {
    constexpr size_t I = index_of<T, Types...>::value;
    if (v.type_index_ != I) {
        throw bad_variant_access();
    }
    return *v.template get_ptr<T>();
}

template <size_t I, typename... Types>
typename type_at<I, Types...>::type* get_if(Variant<Types...>* v) noexcept {
    if (v && v->type_index_ == I) {
        using T = typename type_at<I, Types...>::type;
        return v->template get_ptr<T>();
    }
    return nullptr;
}

template <size_t I, typename... Types>
const typename type_at<I, Types...>::type* get_if(const Variant<Types...>* v) noexcept {
    if (v && v->type_index_ == I) {
        using T = typename type_at<I, Types...>::type;
        return v->template get_ptr<T>();
    }
    return nullptr;
}

template <typename T, typename... Types>
T* get_if(Variant<Types...>* v) noexcept {
    constexpr size_t I = index_of<T, Types...>::value;
    if (v && v->type_index_ == I) {
        return v->template get_ptr<T>();
    }
    return nullptr;
}

template <typename T, typename... Types>
const T* get_if(const Variant<Types...>* v) noexcept {
    constexpr size_t I = index_of<T, Types...>::value;
    if (v && v->type_index_ == I) {
        return v->template get_ptr<T>();
    }
    return nullptr;
}

template <typename T, typename... Types>
bool holds_alternative(const Variant<Types...>& v) noexcept {
    return v.index() == index_of<T, Types...>::value;
}

template <size_t I, typename... Types>
typename std::enable_if<(I < sizeof...(Types)), bool>::type
variant_equals_impl(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
    if (lhs.index() == I) {
        return get<I>(lhs) == get<I>(rhs);
    }
    return variant_equals_impl<I + 1, Types...>(lhs, rhs);
}

template <size_t I, typename... Types>
typename std::enable_if<(I >= sizeof...(Types)), bool>::type
variant_equals_impl(const Variant<Types...>&, const Variant<Types...>&) {
    return true;
}

template <typename... Types>
bool operator==(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
    if (lhs.index() != rhs.index()) {
        return false;
    }
    return variant_equals_impl<0, Types...>(lhs, rhs);
}

template <typename... Types>
bool operator!=(const Variant<Types...>& lhs, const Variant<Types...>& rhs) {
    return !(lhs == rhs);
}

template <typename Visitor, size_t I, typename... Types>
typename std::enable_if<(I < sizeof...(Types)), decltype(std::declval<Visitor>()(get<0>(std::declval<Variant<Types...>&>())))>::type
visit_impl(Visitor&& vis, Variant<Types...>& var) {
    if (var.index() == I) {
        return std::forward<Visitor>(vis)(get<I>(var));
    }
    return visit_impl<Visitor, I + 1, Types...>(std::forward<Visitor>(vis), var);
}

template <typename Visitor, size_t I, typename... Types>
typename std::enable_if<(I >= sizeof...(Types)), decltype(std::declval<Visitor>()(get<0>(std::declval<Variant<Types...>&>())))>::type
visit_impl(Visitor&&, Variant<Types...>&) {
    throw bad_variant_access();
}

template <typename Visitor, typename... Types>
decltype(std::declval<Visitor>()(get<0>(std::declval<Variant<Types...>&>())))
visit(Visitor&& vis, Variant<Types...>& var) {
    return visit_impl<Visitor, 0, Types...>(std::forward<Visitor>(vis), var);
}

template <typename Visitor, size_t I, typename... Types>
typename std::enable_if<(I < sizeof...(Types)), decltype(std::declval<Visitor>()(get<0>(std::declval<const Variant<Types...>&>())))>::type
visit_impl(Visitor&& vis, const Variant<Types...>& var) {
    if (var.index() == I) {
        return std::forward<Visitor>(vis)(get<I>(var));
    }
    return visit_impl<Visitor, I + 1, Types...>(std::forward<Visitor>(vis), var);
}

template <typename Visitor, size_t I, typename... Types>
typename std::enable_if<(I >= sizeof...(Types)), decltype(std::declval<Visitor>()(get<0>(std::declval<const Variant<Types...>&>())))>::type
visit_impl(Visitor&&, const Variant<Types...>&) {
    throw bad_variant_access();
}

template <typename Visitor, typename... Types>
decltype(std::declval<Visitor>()(get<0>(std::declval<const Variant<Types...>&>())))
visit(Visitor&& vis, const Variant<Types...>& var) {
    return visit_impl<Visitor, 0, Types...>(std::forward<Visitor>(vis), var);
}

} // namespace fizmo

#endif // FIZMO_VARIANT_HPP