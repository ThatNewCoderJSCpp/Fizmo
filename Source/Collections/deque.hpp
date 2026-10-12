#ifndef FIZMO_COLLECTIONS_DEQUE_HPP
#define FIZMO_COLLECTIONS_DEQUE_HPP

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "hash.hpp"
#include "iterator.hpp"

namespace fizmo {
namespace collections {

template <class T>
class Deque {
public:
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;

    using iterator               = IndexIterator<Deque, false>;
    using const_iterator         = IndexIterator<Deque, true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    Deque() = default;

    explicit Deque(size_type n, const T& value = T()) {
        reserve(n);
        for (size_type i = 0; i < n; ++i) push_back(value);
    }

    Deque(std::initializer_list<T> init) : Deque(init.begin(), init.end()) {}

    template <class It, typename std::enable_if<!std::is_integral<It>::value, int>::type = 0>
    Deque(It first, It last) {
        for (; first != last; ++first) push_back(*first);
    }

    Deque(const Deque& o) {
        reserve(o.m_size);
        for (const auto& v : o) push_back(v);
    }

    Deque(Deque&& o) noexcept : m_buf(o.m_buf), m_cap(o.m_cap), m_head(o.m_head), m_size(o.m_size) {
        o.m_buf = nullptr;
        o.m_cap = o.m_head = o.m_size = 0;
    }

    Deque& operator=(const Deque& o) {
        if (this != &o) {
            Deque t(o);
            swap(t);
        }
        return *this;
    }

    Deque& operator=(Deque&& o) noexcept {
        if (this != &o) {
            Deque t(std::move(o));
            swap(t);
        }
        return *this;
    }

    ~Deque() {
        clear();
        release(m_buf, m_cap);
    }

    void swap(Deque& o) noexcept {
        std::swap(m_buf, o.m_buf);
        std::swap(m_cap, o.m_cap);
        std::swap(m_head, o.m_head);
        std::swap(m_size, o.m_size);
    }

    size_type size() const noexcept { return m_size; }
    size_type capacity() const noexcept { return m_cap; }
    bool empty() const noexcept { return m_size == 0; }
    explicit operator bool() const noexcept { return m_size != 0; }

    T& operator[](size_type i) noexcept { return m_buf[slot(i)]; }
    const T& operator[](size_type i) const noexcept { return m_buf[slot(i)]; }

    T& at(size_type i) {
        if (i >= m_size) throw std::out_of_range("Deque::at");
        return (*this)[i];
    }

    const T& at(size_type i) const {
        if (i >= m_size) throw std::out_of_range("Deque::at");
        return (*this)[i];
    }

    T& front() { return check_nonempty(), (*this)[0]; }
    const T& front() const { return check_nonempty(), (*this)[0]; }
    T& back() { return check_nonempty(), (*this)[m_size - 1]; }
    const T& back() const { return check_nonempty(), (*this)[m_size - 1]; }

    template <class... Args>
    T& emplace_back(Args&&... args) {
        grow_for(m_size + 1);
        T* p = m_buf + slot(m_size);
        ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
        ++m_size;
        return *p;
    }

    template <class... Args>
    T& emplace_front(Args&&... args) {
        grow_for(m_size + 1);
        const size_type h = (m_head + m_cap - 1) & (m_cap - 1);
        ::new (static_cast<void*>(m_buf + h)) T(std::forward<Args>(args)...);
        m_head = h;
        ++m_size;
        return m_buf[h];
    }

    void push_back(const T& v) { emplace_back(v); }
    void push_back(T&& v) { emplace_back(std::move(v)); }
    void push_front(const T& v) { emplace_front(v); }
    void push_front(T&& v) { emplace_front(std::move(v)); }
    void append(const T& v) { emplace_back(v); }
    void append(T&& v) { emplace_back(std::move(v)); }
    void appendleft(const T& v) { emplace_front(v); }
    void appendleft(T&& v) { emplace_front(std::move(v)); }

    void pop_back() {
        check_nonempty();
        m_buf[slot(m_size - 1)].~T();
        --m_size;
    }

    void pop_front() {
        check_nonempty();
        m_buf[m_head].~T();
        m_head = (m_head + 1) & (m_cap - 1);
        --m_size;
    }

    T take_back() {
        check_nonempty();
        T v = std::move(back());
        pop_back();
        return v;
    }

    T take_front() {
        check_nonempty();
        T v = std::move(front());
        pop_front();
        return v;
    }

    template <class It>
    void extend(It first, It last) {
        for (; first != last; ++first) push_back(*first);
    }

    template <class C>
    void extend(const C& c) { extend(std::begin(c), std::end(c)); }

    template <class It>
    void extendleft(It first, It last) {
        for (; first != last; ++first) push_front(*first);
    }

    template <class C>
    void extendleft(const C& c) { extendleft(std::begin(c), std::end(c)); }

    void rotate(std::ptrdiff_t n) {
        if (m_size < 2) return;
        const std::ptrdiff_t s = static_cast<std::ptrdiff_t>(m_size);
        n %= s;
        if (n < 0) n += s;
        if (n == 0) return;

        if (m_size == m_cap) {
            m_head = (m_head + m_cap - static_cast<size_type>(n)) & (m_cap - 1);
            return;
        }

        if (static_cast<size_type>(n) <= m_size / 2) {
            for (std::ptrdiff_t i = 0; i < n; ++i) push_front(take_back());
        } else {
            for (std::ptrdiff_t i = 0; i < s - n; ++i) push_back(take_front());
        }
    }

    void reverse() { std::reverse(begin(), end()); }

    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        const size_type i = pos.index();
        if (i > m_size) throw std::out_of_range("Deque::insert");

        if (i < m_size / 2) {
            emplace_front(std::forward<Args>(args)...);
            for (size_type k = 0; k < i; ++k) std::swap((*this)[k], (*this)[k + 1]);
        } else {
            emplace_back(std::forward<Args>(args)...);
            for (size_type k = m_size - 1; k > i; --k) std::swap((*this)[k], (*this)[k - 1]);
        }

        return iterator(this, i);
    }

    iterator insert(const_iterator pos, const T& v) { return emplace(pos, v); }
    iterator insert(const_iterator pos, T&& v) { return emplace(pos, std::move(v)); }
    void insert(size_type index, const T& v) { emplace(cbegin() + static_cast<difference_type>(index), v); }

    iterator erase(const_iterator pos) {
        const size_type i = pos.index();
        if (i >= m_size) throw std::out_of_range("Deque::erase");

        if (i < m_size / 2) {
            for (size_type k = i; k > 0; --k) std::swap((*this)[k], (*this)[k - 1]);
            pop_front();
        } else {
            for (size_type k = i; k + 1 < m_size; ++k) std::swap((*this)[k], (*this)[k + 1]);
            pop_back();
        }

        return iterator(this, i);
    }

    iterator erase(const_iterator first, const_iterator last) {
        const size_type a = first.index();
        const size_type n = last.index() - first.index();
        for (size_type k = 0; k < n; ++k) erase(cbegin() + static_cast<difference_type>(a));
        return iterator(this, a);
    }

    bool remove(const T& v) {
        for (size_type i = 0; i < m_size; ++i) {
            if ((*this)[i] == v) {
                erase(cbegin() + static_cast<difference_type>(i));
                return true;
            }
        }

        return false;
    }

    size_type count(const T& v) const { return static_cast<size_type>(std::count(begin(), end(), v)); }

    size_type index(const T& v) const {
        for (size_type i = 0; i < m_size; ++i) if ((*this)[i] == v) return i;
        throw std::out_of_range("Deque::index: value not present");
    }

    bool contains(const T& v) const { return std::find(begin(), end(), v) != end(); }

    void clear() noexcept {
        for (size_type i = 0; i < m_size; ++i) m_buf[slot(i)].~T();
        m_size = 0;
        m_head = 0;
    }

    void reserve(size_type n) {
        if (n > m_cap) reallocate(round_up(n));
    }

    void shrink_to_fit() {
        if (m_size == 0) {
            release(m_buf, m_cap);
            m_buf = nullptr;
            m_cap = 0;
            m_head = 0;
            return;
        }

        const size_type c = round_up(m_size);
        if (c < m_cap) reallocate(c);
    }

    void resize(size_type n, const T& value = T()) {
        while (m_size > n) pop_back();
        while (m_size < n) push_back(value);
    }

    iterator begin() noexcept { return iterator(this, 0); }
    iterator end() noexcept { return iterator(this, m_size); }
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    const_iterator end() const noexcept { return const_iterator(this, m_size); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    friend bool operator==(const Deque& a, const Deque& b) { return a.m_size == b.m_size && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const Deque& a, const Deque& b) { return !(a == b); }
    friend bool operator<(const Deque& a, const Deque& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend bool operator>(const Deque& a, const Deque& b) { return b < a; }
    friend bool operator<=(const Deque& a, const Deque& b) { return !(b < a); }
    friend bool operator>=(const Deque& a, const Deque& b) { return !(a < b); }

private:
    T*        m_buf = nullptr;
    size_type m_cap = 0;
    size_type m_head = 0;
    size_type m_size = 0;

    size_type slot(size_type i) const noexcept { return (m_head + i) & (m_cap - 1); }

    void check_nonempty() const {
        if (m_size == 0) throw std::out_of_range("Deque is empty");
    }

    static size_type round_up(size_type n) {
        size_type c = 8;
        while (c < n) c <<= 1;
        return c;
    }

    static void release(T* p, size_type cap) noexcept {
        if (p) std::allocator<T>().deallocate(p, cap);
    }

    void grow_for(size_type n) {
        if (n > m_cap) reallocate(round_up(n > 2 * m_cap ? n : 2 * m_cap));
    }

    void reallocate(size_type cap) {
        T* nb = std::allocator<T>().allocate(cap);
        size_type moved = 0;

        try {
            for (; moved < m_size; ++moved) ::new (static_cast<void*>(nb + moved)) T(std::move_if_noexcept(m_buf[slot(moved)]));
        } catch (...) {
            for (size_type i = 0; i < moved; ++i) nb[i].~T();
            std::allocator<T>().deallocate(nb, cap);
            throw;
        }

        for (size_type i = 0; i < m_size; ++i) m_buf[slot(i)].~T();
        release(m_buf, m_cap);
        m_buf = nb;
        m_cap = cap;
        m_head = 0;
    }
};

template <class T>
void swap(Deque<T>& a, Deque<T>& b) noexcept { a.swap(b); }

template <class T, class Container = Deque<T>>
class Queue {
public:
    using value_type     = T;
    using size_type      = std::size_t;
    using container_type = Container;

    Queue() = default;
    Queue(std::initializer_list<T> init) : m_c(init) {}
    explicit Queue(Container c) : m_c(std::move(c)) {}

    void push(const T& v) { m_c.push_back(v); }
    void push(T&& v) { m_c.push_back(std::move(v)); }
    void put(const T& v) { m_c.push_back(v); }
    void put(T&& v) { m_c.push_back(std::move(v)); }

    template <class... Args>
    T& emplace(Args&&... args) { return m_c.emplace_back(std::forward<Args>(args)...); }

    T& front() { return m_c.front(); }
    const T& front() const { return m_c.front(); }
    T& back() { return m_c.back(); }
    const T& back() const { return m_c.back(); }
    T& peek() { return m_c.front(); }
    const T& peek() const { return m_c.front(); }

    void pop() { m_c.pop_front(); }
    T get() { return m_c.take_front(); }

    size_type size() const noexcept { return m_c.size(); }
    bool empty() const noexcept { return m_c.empty(); }
    explicit operator bool() const noexcept { return !m_c.empty(); }
    void clear() noexcept { m_c.clear(); }

    auto begin() const { return m_c.begin(); }
    auto end() const { return m_c.end(); }

    const Container& container() const noexcept { return m_c; }

    friend bool operator==(const Queue& a, const Queue& b) { return a.m_c == b.m_c; }
    friend bool operator!=(const Queue& a, const Queue& b) { return !(a == b); }

private:
    Container m_c;
};

template <class T, class Container = Deque<T>>
class Stack {
public:
    using value_type     = T;
    using size_type      = std::size_t;
    using container_type = Container;

    Stack() = default;
    Stack(std::initializer_list<T> init) : m_c(init) {}
    explicit Stack(Container c) : m_c(std::move(c)) {}

    void push(const T& v) { m_c.push_back(v); }
    void push(T&& v) { m_c.push_back(std::move(v)); }
    void put(const T& v) { m_c.push_back(v); }
    void put(T&& v) { m_c.push_back(std::move(v)); }

    template <class... Args>
    T& emplace(Args&&... args) { return m_c.emplace_back(std::forward<Args>(args)...); }

    T& top() { return m_c.back(); }
    const T& top() const { return m_c.back(); }
    T& peek() { return m_c.back(); }
    const T& peek() const { return m_c.back(); }

    void pop() { m_c.pop_back(); }
    T get() { return m_c.take_back(); }

    size_type size() const noexcept { return m_c.size(); }
    bool empty() const noexcept { return m_c.empty(); }
    explicit operator bool() const noexcept { return !m_c.empty(); }
    void clear() noexcept { m_c.clear(); }

    auto begin() const { return m_c.begin(); }
    auto end() const { return m_c.end(); }

    const Container& container() const noexcept { return m_c; }

    friend bool operator==(const Stack& a, const Stack& b) { return a.m_c == b.m_c; }
    friend bool operator!=(const Stack& a, const Stack& b) { return !(a == b); }

private:
    Container m_c;
};

template <class T>
using FifoQueue = Queue<T>;

template <class T>
using LifoQueue = Stack<T>;

template <class T>
class RingBuffer {
public:
    using value_type = T;
    using size_type  = std::size_t;

    explicit RingBuffer(size_type capacity) : m_cap(capacity) {
        if (capacity == 0) throw std::invalid_argument("RingBuffer capacity must be positive");
        m_d.reserve(capacity);
    }

    size_type capacity() const noexcept { return m_cap; }
    size_type size() const noexcept { return m_d.size(); }
    bool empty() const noexcept { return m_d.empty(); }
    bool full() const noexcept { return m_d.size() == m_cap; }

    bool push(const T& v) {
        const bool dropped = full();
        if (dropped) m_d.pop_front();
        m_d.push_back(v);
        return dropped;
    }

    bool push(T&& v) {
        const bool dropped = full();
        if (dropped) m_d.pop_front();
        m_d.push_back(std::move(v));
        return dropped;
    }

    T pop() { return m_d.take_front(); }
    T pop_newest() { return m_d.take_back(); }

    T& oldest() { return m_d.front(); }
    const T& oldest() const { return m_d.front(); }
    T& newest() { return m_d.back(); }
    const T& newest() const { return m_d.back(); }

    T& operator[](size_type i) noexcept { return m_d[i]; }
    const T& operator[](size_type i) const noexcept { return m_d[i]; }
    T& at(size_type i) { return m_d.at(i); }
    const T& at(size_type i) const { return m_d.at(i); }

    void clear() noexcept { m_d.clear(); }

    auto begin() { return m_d.begin(); }
    auto end() { return m_d.end(); }
    auto begin() const { return m_d.begin(); }
    auto end() const { return m_d.end(); }

private:
    size_type m_cap;
    Deque<T>  m_d;
};

template <class T>
struct Hash<Deque<T>> {
    std::size_t operator()(const Deque<T>& d) const {
        std::size_t h = d.size();
        for (const auto& v : d) h = hash_combine(h, Hash<T>()(v));
        return h;
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_DEQUE_HPP
