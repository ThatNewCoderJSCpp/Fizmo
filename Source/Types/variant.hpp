#ifndef FIZMO_VARIANT_HPP
#define FIZMO_VARIANT_HPP

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fizmo {

class bad_variant_access : public std::runtime_error {
public:
    bad_variant_access() : std::runtime_error("Bad variant access") {}
};

struct Monostate {};

constexpr bool operator==(Monostate, Monostate) noexcept { return true; }
constexpr bool operator!=(Monostate, Monostate) noexcept { return false; }
constexpr bool operator<(Monostate, Monostate) noexcept { return false; }

inline constexpr std::size_t variant_npos = static_cast<std::size_t>(-1);

namespace variant_detail {

template <std::size_t I, class... Ts>
struct type_at;

template <std::size_t I, class T, class... Rest>
struct type_at<I, T, Rest...> {
    using type = typename type_at<I - 1, Rest...>::type;
};

template <class T, class... Rest>
struct type_at<0, T, Rest...> {
    using type = T;
};

template <class T, class... Ts>
struct index_of;

template <class T>
struct index_of<T> : std::integral_constant<std::size_t, variant_npos> {};

template <class T, class First, class... Rest>
struct index_of<T, First, Rest...> : std::integral_constant<std::size_t, std::is_same<T, First>::value ? 0 : (index_of<T, Rest...>::value == variant_npos ? variant_npos : index_of<T, Rest...>::value + 1)> {};

template <class T, class... Ts>
struct count_of : std::integral_constant<std::size_t, 0> {};

template <class T, class First, class... Rest>
struct count_of<T, First, Rest...> : std::integral_constant<std::size_t, (std::is_same<T, First>::value ? 1 : 0) + count_of<T, Rest...>::value> {};

template <class U, class... Ts>
struct constructible_count : std::integral_constant<std::size_t, 0> {};

template <class U, class First, class... Rest>
struct constructible_count<U, First, Rest...> : std::integral_constant<std::size_t, (std::is_constructible<First, U>::value ? 1 : 0) + constructible_count<U, Rest...>::value> {};

template <class U, class... Ts>
struct first_constructible : std::integral_constant<std::size_t, variant_npos> {};

template <class U, class First, class... Rest>
struct first_constructible<U, First, Rest...> : std::integral_constant<std::size_t, std::is_constructible<First, U>::value ? 0 : (first_constructible<U, Rest...>::value == variant_npos ? variant_npos : first_constructible<U, Rest...>::value + 1)> {};

template <class U, class... Ts>
struct select_alternative {
    using D = typename std::decay<U>::type;
    static constexpr std::size_t exact = index_of<D, Ts...>::value;
    static constexpr std::size_t value = exact != variant_npos ? (count_of<D, Ts...>::value == 1 ? exact : variant_npos) : (constructible_count<U, Ts...>::value == 1 ? first_constructible<U, Ts...>::value : variant_npos);
};

template <class... Ts>
struct max_of;

template <>
struct max_of<> : std::integral_constant<std::size_t, 1> {};

template <class T, class... Rest>
struct max_of<T, Rest...> : std::integral_constant<std::size_t, (sizeof(T) > max_of<Rest...>::value ? sizeof(T) : max_of<Rest...>::value)> {};

template <class... Ts>
struct align_of;

template <>
struct align_of<> : std::integral_constant<std::size_t, 1> {};

template <class T, class... Rest>
struct align_of<T, Rest...> : std::integral_constant<std::size_t, (alignof(T) > align_of<Rest...>::value ? alignof(T) : align_of<Rest...>::value)> {};

} // namespace variant_detail

template <class... Ts>
class Variant;

template <class V>
struct variant_size;

template <class... Ts>
struct variant_size<Variant<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <std::size_t I, class V>
struct variant_alternative;

template <std::size_t I, class... Ts>
struct variant_alternative<I, Variant<Ts...>> {
    using type = typename variant_detail::type_at<I, Ts...>::type;
};

template <class... Ts>
class Variant {
    static_assert(sizeof...(Ts) > 0, "Variant requires at least one alternative");

    template <std::size_t I>
    using alt = typename variant_detail::type_at<I, Ts...>::type;

    using First = alt<0>;

public:
    template <class D = First, typename std::enable_if<std::is_default_constructible<D>::value, int>::type = 0>
    Variant() noexcept(std::is_nothrow_default_constructible<First>::value) {
        ::new (static_cast<void*>(&m_storage)) First();
        m_index = 0;
    }

    Variant(const Variant& o) {
        if (o.m_index != variant_npos) copy_from(o);
    }

    Variant(Variant&& o) noexcept(all_nothrow_move()) {
        if (o.m_index != variant_npos) move_from(std::move(o));
    }

    template <class U, std::size_t I = variant_detail::select_alternative<U&&, Ts...>::value, typename std::enable_if<
        !std::is_same<typename std::decay<U>::type, Variant>::value && I != variant_npos, int>::type = 0>
    Variant(U&& v) {
        ::new (static_cast<void*>(&m_storage)) alt<I>(std::forward<U>(v));
        m_index = I;
    }

    ~Variant() { destroy(); }

    Variant& operator=(const Variant& o) {
        if (this == &o) return *this;
        Variant tmp(o);
        destroy();
        if (tmp.m_index != variant_npos) move_from(std::move(tmp));
        return *this;
    }

    Variant& operator=(Variant&& o) noexcept(all_nothrow_move()) {
        if (this == &o) return *this;
        destroy();
        if (o.m_index != variant_npos) move_from(std::move(o));
        return *this;
    }

    template <class U, std::size_t I = variant_detail::select_alternative<U&&, Ts...>::value, typename std::enable_if<
        !std::is_same<typename std::decay<U>::type, Variant>::value && I != variant_npos, int>::type = 0>
    Variant& operator=(U&& v) {
        if (m_index == I) {
            *ptr<alt<I>>() = std::forward<U>(v);
            return *this;
        }

        alt<I> tmp(std::forward<U>(v));
        destroy();
        ::new (static_cast<void*>(&m_storage)) alt<I>(std::move(tmp));
        m_index = I;
        return *this;
    }

    std::size_t index() const noexcept { return m_index; }
    bool valueless_by_exception() const noexcept { return m_index == variant_npos; }

    template <class T, class... Args>
    T& emplace(Args&&... args) {
        constexpr std::size_t I = variant_detail::index_of<T, Ts...>::value;
        static_assert(I != variant_npos, "Type not in variant");
        return emplace<I>(std::forward<Args>(args)...);
    }

    template <std::size_t I, class... Args>
    alt<I>& emplace(Args&&... args) {
        static_assert(I < sizeof...(Ts), "Variant index out of range");
        destroy();
        ::new (static_cast<void*>(&m_storage)) alt<I>(std::forward<Args>(args)...);
        m_index = I;
        return *ptr<alt<I>>();
    }

    template <class T>
    bool holds() const noexcept { return m_index == variant_detail::index_of<T, Ts...>::value; }

    void swap(Variant& o) {
        if (this == &o) return;
        Variant tmp(std::move(o));
        o = std::move(*this);
        *this = std::move(tmp);
    }

    template <class T>
    T* raw() noexcept { return ptr<T>(); }

    template <class T>
    const T* raw() const noexcept { return ptr<T>(); }

private:
    alignas(variant_detail::align_of<Ts...>::value) unsigned char m_storage[variant_detail::max_of<Ts...>::value];
    std::size_t m_index = variant_npos;

    static constexpr bool all_nothrow_move() {
        bool r = true;
        (void)std::initializer_list<int>{ (r = r && std::is_nothrow_move_constructible<Ts>::value, 0)... };
        return r;
    }

    template <class T>
    T* ptr() noexcept { return std::launder(reinterpret_cast<T*>(&m_storage)); }

    template <class T>
    const T* ptr() const noexcept { return std::launder(reinterpret_cast<const T*>(&m_storage)); }

    template <std::size_t I = 0>
    void destroy() noexcept {
        if constexpr (I < sizeof...(Ts)) {
            if (m_index == I) {
                using A = alt<I>;
                ptr<A>()->~A();
                m_index = variant_npos;
                return;
            }

            destroy<I + 1>();
        }
    }

    template <std::size_t I = 0>
    void copy_from(const Variant& o) {
        if constexpr (I < sizeof...(Ts)) {
            if (o.m_index == I) {
                ::new (static_cast<void*>(&m_storage)) alt<I>(*o.template ptr<alt<I>>());
                m_index = I;
                return;
            }

            copy_from<I + 1>(o);
        }
    }

    template <std::size_t I = 0>
    void move_from(Variant&& o) {
        if constexpr (I < sizeof...(Ts)) {
            if (o.m_index == I) {
                ::new (static_cast<void*>(&m_storage)) alt<I>(std::move(*o.template ptr<alt<I>>()));
                m_index = I;
                return;
            }

            move_from<I + 1>(std::move(o));
        }
    }

    template <class... Us> friend class Variant;
};

template <std::size_t I, class... Ts>
typename variant_detail::type_at<I, Ts...>::type& get(Variant<Ts...>& v) {
    if (v.index() != I) throw bad_variant_access();
    return *v.template raw<typename variant_detail::type_at<I, Ts...>::type>();
}

template <std::size_t I, class... Ts>
const typename variant_detail::type_at<I, Ts...>::type& get(const Variant<Ts...>& v) {
    if (v.index() != I) throw bad_variant_access();
    return *v.template raw<typename variant_detail::type_at<I, Ts...>::type>();
}

template <std::size_t I, class... Ts>
typename variant_detail::type_at<I, Ts...>::type&& get(Variant<Ts...>&& v) { return std::move(get<I>(v)); }

template <class T, class... Ts>
T& get(Variant<Ts...>& v) {
    static_assert(variant_detail::count_of<T, Ts...>::value == 1, "T must occur exactly once in the variant");
    return get<variant_detail::index_of<T, Ts...>::value>(v);
}

template <class T, class... Ts>
const T& get(const Variant<Ts...>& v) {
    static_assert(variant_detail::count_of<T, Ts...>::value == 1, "T must occur exactly once in the variant");
    return get<variant_detail::index_of<T, Ts...>::value>(v);
}

template <std::size_t I, class... Ts>
typename variant_detail::type_at<I, Ts...>::type* get_if(Variant<Ts...>* v) noexcept {
    return (v && v->index() == I) ? v->template raw<typename variant_detail::type_at<I, Ts...>::type>() : nullptr;
}

template <std::size_t I, class... Ts>
const typename variant_detail::type_at<I, Ts...>::type* get_if(const Variant<Ts...>* v) noexcept {
    return (v && v->index() == I) ? v->template raw<typename variant_detail::type_at<I, Ts...>::type>() : nullptr;
}

template <class T, class... Ts>
T* get_if(Variant<Ts...>* v) noexcept { return get_if<variant_detail::index_of<T, Ts...>::value>(v); }

template <class T, class... Ts>
const T* get_if(const Variant<Ts...>* v) noexcept { return get_if<variant_detail::index_of<T, Ts...>::value>(v); }

template <class T, class... Ts>
bool holds_alternative(const Variant<Ts...>& v) noexcept { return v.template holds<T>(); }

namespace variant_detail {

template <std::size_t I, class R, class Visitor, class V>
R visit_at(Visitor&& vis, V&& var) {
    using Var = typename std::decay<V>::type;

    if constexpr (I < variant_size<Var>::value) {
        if (var.index() == I) return std::forward<Visitor>(vis)(get<I>(std::forward<V>(var)));
        return visit_at<I + 1, R>(std::forward<Visitor>(vis), std::forward<V>(var));
    } else {
        throw bad_variant_access();
    }
}

template <std::size_t I, class... Ts>
bool equal_at(const Variant<Ts...>& a, const Variant<Ts...>& b) {
    if constexpr (I < sizeof...(Ts)) {
        if (a.index() == I) return get<I>(a) == get<I>(b);
        return equal_at<I + 1>(a, b);
    } else {
        return true;
    }
}

template <std::size_t I, class... Ts>
bool less_at(const Variant<Ts...>& a, const Variant<Ts...>& b) {
    if constexpr (I < sizeof...(Ts)) {
        if (a.index() == I) return get<I>(a) < get<I>(b);
        return less_at<I + 1>(a, b);
    } else {
        return false;
    }
}

template <std::size_t I, class... Ts>
std::size_t hash_at(const Variant<Ts...>& v) {
    if constexpr (I < sizeof...(Ts)) {
        using T = typename type_at<I, Ts...>::type;
        if (v.index() == I) return std::hash<T>()(get<I>(v)) ^ (I * 0x9e3779b97f4a7c15ULL);
        return hash_at<I + 1>(v);
    } else {
        return 0;
    }
}

} // namespace variant_detail

template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& vis, Variant<Ts...>& var) {
    using R = decltype(std::forward<Visitor>(vis)(get<0>(var)));
    return variant_detail::visit_at<0, R>(std::forward<Visitor>(vis), var);
}

template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& vis, const Variant<Ts...>& var) {
    using R = decltype(std::forward<Visitor>(vis)(get<0>(var)));
    return variant_detail::visit_at<0, R>(std::forward<Visitor>(vis), var);
}

template <class Visitor, class... Ts>
decltype(auto) visit(Visitor&& vis, Variant<Ts...>&& var) {
    using R = decltype(std::forward<Visitor>(vis)(get<0>(std::move(var))));
    return variant_detail::visit_at<0, R>(std::forward<Visitor>(vis), std::move(var));
}

template <class... Fs>
struct Overloaded : Fs... {
    using Fs::operator()...;
};

template <class... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

template <class... Ts>
bool operator==(const Variant<Ts...>& a, const Variant<Ts...>& b) { return a.index() == b.index() && variant_detail::equal_at<0>(a, b); }

template <class... Ts>
bool operator!=(const Variant<Ts...>& a, const Variant<Ts...>& b) { return !(a == b); }

template <class... Ts>
bool operator<(const Variant<Ts...>& a, const Variant<Ts...>& b) {
    if (b.valueless_by_exception()) return false;
    if (a.valueless_by_exception()) return true;
    if (a.index() != b.index()) return a.index() < b.index();
    return variant_detail::less_at<0>(a, b);
}

template <class... Ts>
bool operator>(const Variant<Ts...>& a, const Variant<Ts...>& b) { return b < a; }

template <class... Ts>
bool operator<=(const Variant<Ts...>& a, const Variant<Ts...>& b) { return !(b < a); }

template <class... Ts>
bool operator>=(const Variant<Ts...>& a, const Variant<Ts...>& b) { return !(a < b); }

template <class... Ts>
void swap(Variant<Ts...>& a, Variant<Ts...>& b) { a.swap(b); }

} // namespace fizmo

namespace std {

template <class... Ts>
struct hash<fizmo::Variant<Ts...>> {
    std::size_t operator()(const fizmo::Variant<Ts...>& v) const { return fizmo::variant_detail::hash_at<0>(v); }
};

template <>
struct hash<fizmo::Monostate> {
    std::size_t operator()(fizmo::Monostate) const noexcept { return 0x5ca1ab1eu; }
};

} // namespace std

#endif // FIZMO_VARIANT_HPP
