#ifndef FIZMO_COLLECTIONS_ITERATOR_HPP
#define FIZMO_COLLECTIONS_ITERATOR_HPP

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace fizmo {
namespace collections {

template <class Reference>
class ArrowProxy {
public:
    explicit ArrowProxy(Reference r) : m_value(std::move(r)) {}
    const typename std::remove_reference<Reference>::type* operator->() const noexcept { return std::addressof(m_value); }

private:
    typename std::remove_reference<Reference>::type m_value;
};

namespace iterator_detail {

template <class Reference, bool = std::is_lvalue_reference<Reference>::value>
struct arrow {
    using pointer = typename std::add_pointer<typename std::remove_reference<Reference>::type>::type;
    static pointer get(Reference r) noexcept { return std::addressof(r); }
};

template <class Reference>
struct arrow<Reference, false> {
    using pointer = ArrowProxy<Reference>;
    static pointer get(Reference r) { return pointer(std::move(r)); }
};

template <class Container, class Reference, class = void>
struct value_of {
    using type = typename std::remove_cv<typename std::remove_reference<Reference>::type>::type;
};

template <class Container, class Reference>
struct value_of<Container, Reference, decltype(void(sizeof(typename Container::value_type)))> {
    using type = typename Container::value_type;
};

} // namespace iterator_detail

template <class T, bool Reverse = false>
class ContiguousIterator {
    template <class, bool> friend class ContiguousIterator;

    using mutable_type = typename std::remove_const<T>::type;

    template <class U>
    using enable_const_conversion = typename std::enable_if<std::is_const<T>::value && std::is_same<U, mutable_type>::value>::type;

    static constexpr std::ptrdiff_t step(std::ptrdiff_t n) noexcept { return Reverse ? -n : n; }

public:
    using iterator_category = std::random_access_iterator_tag;
#if defined(__cpp_lib_ranges)
    using iterator_concept  = typename std::conditional<Reverse, std::random_access_iterator_tag, std::contiguous_iterator_tag>::type;
#endif
    using value_type        = mutable_type;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    constexpr ContiguousIterator() noexcept : m_ptr(nullptr) {}
    constexpr explicit ContiguousIterator(T* ptr) noexcept : m_ptr(ptr) {}

    template <class U, class = enable_const_conversion<U>>
    constexpr ContiguousIterator(const ContiguousIterator<U, Reverse>& other) noexcept : m_ptr(other.m_ptr) {}

    constexpr T* get() const noexcept { return m_ptr; }
    constexpr T* base_pointer() const noexcept { return m_ptr; }

    constexpr reference operator*() const noexcept { return Reverse ? *(m_ptr - 1) : *m_ptr; }
    constexpr pointer operator->() const noexcept { return Reverse ? m_ptr - 1 : m_ptr; }
    constexpr reference operator[](difference_type n) const noexcept { return m_ptr[Reverse ? -1 - n : n]; }

    constexpr ContiguousIterator& operator++() noexcept { m_ptr += step(1); return *this; }
    constexpr ContiguousIterator& operator--() noexcept { m_ptr -= step(1); return *this; }
    constexpr ContiguousIterator operator++(int) noexcept { ContiguousIterator t(*this); m_ptr += step(1); return t; }
    constexpr ContiguousIterator operator--(int) noexcept { ContiguousIterator t(*this); m_ptr -= step(1); return t; }
    constexpr ContiguousIterator& operator+=(difference_type n) noexcept { m_ptr += step(n); return *this; }
    constexpr ContiguousIterator& operator-=(difference_type n) noexcept { m_ptr -= step(n); return *this; }

    friend constexpr ContiguousIterator operator+(ContiguousIterator it, difference_type n) noexcept { return ContiguousIterator(it.m_ptr + step(n)); }
    friend constexpr ContiguousIterator operator+(difference_type n, ContiguousIterator it) noexcept { return ContiguousIterator(it.m_ptr + step(n)); }
    friend constexpr ContiguousIterator operator-(ContiguousIterator it, difference_type n) noexcept { return ContiguousIterator(it.m_ptr - step(n)); }
    friend constexpr difference_type operator-(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return step(a.m_ptr - b.m_ptr); }

    friend constexpr bool operator==(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return a.m_ptr == b.m_ptr; }
    friend constexpr bool operator!=(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return a.m_ptr != b.m_ptr; }
    friend constexpr bool operator<(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return Reverse ? b.m_ptr < a.m_ptr : a.m_ptr < b.m_ptr; }
    friend constexpr bool operator>(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return b < a; }
    friend constexpr bool operator<=(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return !(b < a); }
    friend constexpr bool operator>=(const ContiguousIterator& a, const ContiguousIterator& b) noexcept { return !(a < b); }

private:
    T* m_ptr;
};

template <class T> using ArrayIterator             = ContiguousIterator<T, false>;
template <class T> using ConstArrayIterator        = ContiguousIterator<const T, false>;
template <class T> using ReverseArrayIterator      = ContiguousIterator<T, true>;
template <class T> using ConstReverseArrayIterator = ContiguousIterator<const T, true>;

template <class Derived, class T>
class ContiguousIterable {
    Derived& self() noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

public:
    using iterator               = ArrayIterator<T>;
    using const_iterator         = ConstArrayIterator<T>;
    using reverse_iterator       = ReverseArrayIterator<T>;
    using const_reverse_iterator = ConstReverseArrayIterator<T>;

    iterator begin() noexcept { return iterator(self().data()); }
    iterator end() noexcept { return iterator(self().data() + self().size()); }
    const_iterator begin() const noexcept { return const_iterator(self().data()); }
    const_iterator end() const noexcept { return const_iterator(self().data() + self().size()); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(self().data() + self().size()); }
    reverse_iterator rend() noexcept { return reverse_iterator(self().data()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(self().data() + self().size()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(self().data()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }
};

template <class Container, bool Const = false>
class IndexIterator {
    template <class, bool> friend class IndexIterator;

public:
    using owner_type        = typename std::conditional<Const, const Container, Container>::type;
    using size_type         = std::size_t;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using reference         = decltype(std::declval<owner_type&>()[std::declval<size_type>()]);
    using value_type        = typename iterator_detail::value_of<typename std::remove_const<Container>::type, reference>::type;
    using pointer           = typename iterator_detail::arrow<reference>::pointer;

    IndexIterator() = default;
    IndexIterator(owner_type* owner, size_type index) noexcept : m_owner(owner), m_index(index) {}

    template <bool C = Const, typename std::enable_if<C, int>::type = 0>
    IndexIterator(const IndexIterator<Container, false>& o) noexcept : m_owner(o.m_owner), m_index(o.m_index) {}

    reference operator*() const { return (*m_owner)[m_index]; }
    pointer operator->() const { return iterator_detail::arrow<reference>::get((*m_owner)[m_index]); }
    reference operator[](difference_type n) const { return (*m_owner)[offset(n)]; }

    IndexIterator& operator++() noexcept { ++m_index; return *this; }
    IndexIterator operator++(int) noexcept { IndexIterator t = *this; ++m_index; return t; }
    IndexIterator& operator--() noexcept { --m_index; return *this; }
    IndexIterator operator--(int) noexcept { IndexIterator t = *this; --m_index; return t; }
    IndexIterator& operator+=(difference_type n) noexcept { m_index = offset(n); return *this; }
    IndexIterator& operator-=(difference_type n) noexcept { m_index = offset(-n); return *this; }

    friend IndexIterator operator+(IndexIterator a, difference_type n) noexcept { return a += n; }
    friend IndexIterator operator+(difference_type n, IndexIterator a) noexcept { return a += n; }
    friend IndexIterator operator-(IndexIterator a, difference_type n) noexcept { return a -= n; }
    friend difference_type operator-(const IndexIterator& a, const IndexIterator& b) noexcept { return static_cast<difference_type>(a.m_index) - static_cast<difference_type>(b.m_index); }

    friend bool operator==(const IndexIterator& a, const IndexIterator& b) noexcept { return a.m_index == b.m_index && a.m_owner == b.m_owner; }
    friend bool operator!=(const IndexIterator& a, const IndexIterator& b) noexcept { return !(a == b); }
    friend bool operator<(const IndexIterator& a, const IndexIterator& b) noexcept { return a.m_index < b.m_index; }
    friend bool operator>(const IndexIterator& a, const IndexIterator& b) noexcept { return b < a; }
    friend bool operator<=(const IndexIterator& a, const IndexIterator& b) noexcept { return !(b < a); }
    friend bool operator>=(const IndexIterator& a, const IndexIterator& b) noexcept { return !(a < b); }

    owner_type* owner() const noexcept { return m_owner; }
    size_type index() const noexcept { return m_index; }

private:
    size_type offset(difference_type n) const noexcept { return static_cast<size_type>(static_cast<difference_type>(m_index) + n); }

    owner_type* m_owner = nullptr;
    size_type   m_index = 0;
};

template <class Owner, class Node, class Value, bool Const = false>
class NodeIterator {
    template <class, class, class, bool> friend class NodeIterator;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = Value;
    using difference_type   = std::ptrdiff_t;
    using pointer           = typename std::conditional<Const, const Value*, Value*>::type;
    using reference         = typename std::conditional<Const, const Value&, Value&>::type;

    NodeIterator() = default;
    NodeIterator(Node* node, const Owner* owner) noexcept : m_node(node), m_owner(owner) {}

    template <bool C = Const, typename std::enable_if<C, int>::type = 0>
    NodeIterator(const NodeIterator<Owner, Node, Value, false>& o) noexcept : m_node(o.m_node), m_owner(o.m_owner) {}

    reference operator*() const { return Owner::node_value(m_node); }
    pointer operator->() const { return std::addressof(Owner::node_value(m_node)); }

    NodeIterator& operator++() { m_node = Owner::next_node(m_node, m_owner); return *this; }
    NodeIterator operator++(int) { NodeIterator t = *this; ++*this; return t; }
    NodeIterator& operator--() { m_node = Owner::prev_node(m_node, m_owner); return *this; }
    NodeIterator operator--(int) { NodeIterator t = *this; --*this; return t; }

    friend bool operator==(const NodeIterator& a, const NodeIterator& b) noexcept { return a.m_node == b.m_node; }
    friend bool operator!=(const NodeIterator& a, const NodeIterator& b) noexcept { return a.m_node != b.m_node; }

    Node* node() const noexcept { return m_node; }
    const Owner* owner() const noexcept { return m_owner; }

private:
    Node*        m_node = nullptr;
    const Owner* m_owner = nullptr;
};

template <class Value, class Step = std::ptrdiff_t>
class SequenceIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = Value;
    using difference_type   = std::ptrdiff_t;
    using pointer           = ArrowProxy<Value>;
    using reference         = Value;

    SequenceIterator() = default;
    SequenceIterator(Value value, Step step) noexcept : m_value(value), m_step(step) {}

    Value operator*() const noexcept { return m_value; }
    pointer operator->() const { return pointer(m_value); }
    Value operator[](difference_type n) const noexcept { return static_cast<Value>(m_value + m_step * static_cast<Step>(n)); }

    SequenceIterator& operator++() noexcept { m_value = static_cast<Value>(m_value + m_step); return *this; }
    SequenceIterator operator++(int) noexcept { SequenceIterator t = *this; ++*this; return t; }
    SequenceIterator& operator--() noexcept { m_value = static_cast<Value>(m_value - m_step); return *this; }
    SequenceIterator operator--(int) noexcept { SequenceIterator t = *this; --*this; return t; }
    SequenceIterator& operator+=(difference_type n) noexcept { m_value = static_cast<Value>(m_value + m_step * static_cast<Step>(n)); return *this; }
    SequenceIterator& operator-=(difference_type n) noexcept { m_value = static_cast<Value>(m_value - m_step * static_cast<Step>(n)); return *this; }

    friend SequenceIterator operator+(SequenceIterator a, difference_type n) noexcept { return a += n; }
    friend SequenceIterator operator+(difference_type n, SequenceIterator a) noexcept { return a += n; }
    friend SequenceIterator operator-(SequenceIterator a, difference_type n) noexcept { return a -= n; }
    friend difference_type operator-(const SequenceIterator& a, const SequenceIterator& b) noexcept { return static_cast<difference_type>((a.m_value - b.m_value) / a.m_step); }

    friend bool operator==(const SequenceIterator& a, const SequenceIterator& b) noexcept { return a.m_value == b.m_value; }
    friend bool operator!=(const SequenceIterator& a, const SequenceIterator& b) noexcept { return a.m_value != b.m_value; }
    friend bool operator<(const SequenceIterator& a, const SequenceIterator& b) noexcept { return a.m_step > Step(0) ? a.m_value < b.m_value : a.m_value > b.m_value; }
    friend bool operator>(const SequenceIterator& a, const SequenceIterator& b) noexcept { return b < a; }
    friend bool operator<=(const SequenceIterator& a, const SequenceIterator& b) noexcept { return !(b < a); }
    friend bool operator>=(const SequenceIterator& a, const SequenceIterator& b) noexcept { return !(a < b); }

    Step step() const noexcept { return m_step; }

private:
    Value m_value{};
    Step  m_step = Step(1);
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_ITERATOR_HPP
