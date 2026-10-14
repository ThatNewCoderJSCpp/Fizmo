#ifndef FIZMO_COLLECTIONS_LINKED_LIST_HPP
#define FIZMO_COLLECTIONS_LINKED_LIST_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fizmo {
namespace collections {

template <class T>
class LinkedList;

template <class T, bool Const>
class ListIterator;

template <class T>
class ListNode {
public:
    T&       value() noexcept { return m_value; }
    const T& value() const noexcept { return m_value; }

    ListNode*       next() noexcept { return m_next; }
    const ListNode* next() const noexcept { return m_next; }
    ListNode*       prev() noexcept { return m_prev; }
    const ListNode* prev() const noexcept { return m_prev; }

private:
    friend class LinkedList<T>;

    template <class... Args>
    explicit ListNode(Args&&... args) : m_value(std::forward<Args>(args)...) {}

    T         m_value;
    ListNode* m_prev = nullptr;
    ListNode* m_next = nullptr;
};

template <class T, bool Const>
class ListIterator {
    using node_type = typename std::conditional<Const, const ListNode<T>, ListNode<T>>::type;
    using list_type = typename std::conditional<Const, const LinkedList<T>, LinkedList<T>>::type;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = typename std::conditional<Const, const T*, T*>::type;
    using reference         = typename std::conditional<Const, const T&, T&>::type;

    ListIterator() = default;
    ListIterator(node_type* node, list_type* list) noexcept : m_node(node), m_list(list) {}

    template <bool C = Const, typename std::enable_if<C, int>::type = 0>
    ListIterator(const ListIterator<T, false>& o) noexcept : m_node(o.m_node), m_list(o.m_list) {}

    reference operator*() const { return m_node->value(); }
    pointer operator->() const { return &m_node->value(); }

    ListIterator& operator++() {
        m_node = m_node->next();
        return *this;
    }

    ListIterator operator++(int) {
        ListIterator t = *this;
        ++*this;
        return t;
    }

    ListIterator& operator--() {
        m_node = m_node != nullptr ? m_node->prev() : m_list->tail();
        return *this;
    }

    ListIterator operator--(int) {
        ListIterator t = *this;
        --*this;
        return t;
    }

    node_type* node() const noexcept { return m_node; }

    friend bool operator==(const ListIterator& a, const ListIterator& b) noexcept { return a.m_node == b.m_node; }
    friend bool operator!=(const ListIterator& a, const ListIterator& b) noexcept { return a.m_node != b.m_node; }

private:
    template <class, bool> friend class ListIterator;
    friend class LinkedList<T>;

    node_type* m_node = nullptr;
    list_type* m_list = nullptr;
};

template <class T>
class LinkedList {
public:
    using value_type             = T;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = T&;
    using const_reference        = const T&;
    using node_type              = ListNode<T>;
    using iterator               = ListIterator<T, false>;
    using const_iterator         = ListIterator<T, true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type npos = std::numeric_limits<size_type>::max();

    LinkedList() = default;

    LinkedList(std::initializer_list<T> init) : LinkedList(init.begin(), init.end()) {}

    template <class It, typename std::enable_if<!std::is_integral<It>::value, int>::type = 0>
    LinkedList(It first, It last) {
        for (; first != last; ++first) push_back(*first);
    }

    explicit LinkedList(size_type n, const T& value = T()) {
        for (size_type i = 0; i < n; ++i) push_back(value);
    }

    LinkedList(const LinkedList& o) {
        for (const T& v : o) push_back(v);
    }

    LinkedList(LinkedList&& o) noexcept : m_head(o.m_head), m_tail(o.m_tail), m_size(o.m_size) {
        o.m_head = o.m_tail = nullptr;
        o.m_size            = 0;
    }

    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) {
            LinkedList t(o);
            swap(t);
        }
        return *this;
    }

    LinkedList& operator=(LinkedList&& o) noexcept {
        if (this != &o) {
            LinkedList t(std::move(o));
            swap(t);
        }
        return *this;
    }

    LinkedList& operator=(std::initializer_list<T> init) {
        LinkedList t(init);
        swap(t);
        return *this;
    }

    ~LinkedList() { clear(); }

    void swap(LinkedList& o) noexcept {
        std::swap(m_head, o.m_head);
        std::swap(m_tail, o.m_tail);
        std::swap(m_size, o.m_size);
    }

    size_type size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    explicit operator bool() const noexcept { return m_size != 0; }

    node_type*       head() noexcept { return m_head; }
    const node_type* head() const noexcept { return m_head; }
    node_type*       tail() noexcept { return m_tail; }
    const node_type* tail() const noexcept { return m_tail; }

    T& front() { return check_nonempty(), m_head->m_value; }
    const T& front() const { return check_nonempty(), m_head->m_value; }
    T& back() { return check_nonempty(), m_tail->m_value; }
    const T& back() const { return check_nonempty(), m_tail->m_value; }

    T& at(size_type i) { return node_at(i)->m_value; }
    const T& at(size_type i) const { return const_cast<LinkedList*>(this)->node_at(i)->m_value; }
    T& operator[](size_type i) { return at(i); }
    const T& operator[](size_type i) const { return at(i); }

    node_type* node_at(size_type i) {
        if (i >= m_size) throw std::out_of_range("LinkedList::at");
        node_type* n;
        if (i < m_size / 2) {
            n = m_head;
            for (size_type k = 0; k < i; ++k) n = n->m_next;
        } else {
            n = m_tail;
            for (size_type k = m_size - 1; k > i; --k) n = n->m_prev;
        }
        return n;
    }

    template <class... Args>
    T& emplace_back(Args&&... args) { return link_before(nullptr, make(std::forward<Args>(args)...))->m_value; }

    template <class... Args>
    T& emplace_front(Args&&... args) { return link_before(m_head, make(std::forward<Args>(args)...))->m_value; }

    void push_back(const T& v) { emplace_back(v); }
    void push_back(T&& v) { emplace_back(std::move(v)); }
    void push_front(const T& v) { emplace_front(v); }
    void push_front(T&& v) { emplace_front(std::move(v)); }
    void append(const T& v) { emplace_back(v); }
    void append(T&& v) { emplace_back(std::move(v)); }
    void prepend(const T& v) { emplace_front(v); }
    void prepend(T&& v) { emplace_front(std::move(v)); }

    void pop_back() {
        check_nonempty();
        destroy(unlink(m_tail));
    }

    void pop_front() {
        check_nonempty();
        destroy(unlink(m_head));
    }

    T take_back() {
        check_nonempty();
        T v = std::move(m_tail->m_value);
        pop_back();
        return v;
    }

    T take_front() {
        check_nonempty();
        T v = std::move(m_head->m_value);
        pop_front();
        return v;
    }

    template <class... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        return iterator(link_before(mutable_node(pos.m_node), make(std::forward<Args>(args)...)), this);
    }

    iterator insert(const_iterator pos, const T& v) { return emplace(pos, v); }
    iterator insert(const_iterator pos, T&& v) { return emplace(pos, std::move(v)); }

    template <class... Args>
    node_type* emplace_after(node_type* node, Args&&... args) {
        return link_before(node != nullptr ? node->m_next : m_head, make(std::forward<Args>(args)...));
    }

    template <class... Args>
    node_type* emplace_before(node_type* node, Args&&... args) { return link_before(node, make(std::forward<Args>(args)...)); }

    node_type* insert_after(node_type* node, const T& v) { return emplace_after(node, v); }
    node_type* insert_after(node_type* node, T&& v) { return emplace_after(node, std::move(v)); }
    node_type* insert_before(node_type* node, const T& v) { return emplace_before(node, v); }
    node_type* insert_before(node_type* node, T&& v) { return emplace_before(node, std::move(v)); }

    iterator erase(const_iterator pos) {
        node_type* n    = mutable_node(pos.m_node);
        node_type* next = n->m_next;
        destroy(unlink(n));
        return iterator(next, this);
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) first = erase(first);
        return iterator(mutable_node(last.m_node), this);
    }

    node_type* erase(node_type* node) {
        node_type* next = node->m_next;
        destroy(unlink(node));
        return next;
    }

    size_type remove(const T& value) {
        return remove_if([&](const T& v) { return v == value; });
    }

    template <class Pred>
    size_type remove_if(Pred pred) {
        size_type  removed = 0;
        node_type* n       = m_head;
        while (n != nullptr) {
            node_type* next = n->m_next;
            if (pred(n->m_value)) {
                destroy(unlink(n));
                ++removed;
            }
            n = next;
        }
        return removed;
    }

    void clear() noexcept {
        node_type* n = m_head;
        while (n != nullptr) {
            node_type* next = n->m_next;
            delete n;
            n = next;
        }
        m_head = m_tail = nullptr;
        m_size          = 0;
    }

    iterator find(const T& value) { return iterator(find_node(value), this); }
    const_iterator find(const T& value) const { return const_iterator(const_cast<LinkedList*>(this)->find_node(value), this); }

    node_type* find_node(const T& value) {
        for (node_type* n = m_head; n != nullptr; n = n->m_next) {
            if (n->m_value == value) return n;
        }
        return nullptr;
    }

    template <class Pred>
    iterator find_if(Pred pred) {
        for (node_type* n = m_head; n != nullptr; n = n->m_next) {
            if (pred(n->m_value)) return iterator(n, this);
        }
        return end();
    }

    bool contains(const T& value) const { return const_cast<LinkedList*>(this)->find_node(value) != nullptr; }

    size_type index_of(const T& value) const {
        size_type i = 0;
        for (const node_type* n = m_head; n != nullptr; n = n->m_next, ++i) {
            if (n->m_value == value) return i;
        }
        return npos;
    }

    size_type count(const T& value) const {
        size_type c = 0;
        for (const node_type* n = m_head; n != nullptr; n = n->m_next) {
            if (n->m_value == value) ++c;
        }
        return c;
    }

    void reverse() noexcept {
        for (node_type* n = m_head; n != nullptr; n = n->m_prev) std::swap(n->m_next, n->m_prev);
        std::swap(m_head, m_tail);
    }

    void move_to_front(node_type* node) noexcept {
        if (node == m_head) return;
        link_before(m_head, unlink(node));
    }

    void move_to_back(node_type* node) noexcept {
        if (node == m_tail) return;
        link_before(nullptr, unlink(node));
    }

    void splice(const_iterator pos, LinkedList& other) {
        if (&other == this || other.empty()) return;
        node_type* at = mutable_node(pos.m_node);
        node_type* before = at != nullptr ? at->m_prev : m_tail;
        other.m_head->m_prev = before;
        other.m_tail->m_next = at;
        if (before != nullptr) before->m_next = other.m_head;
        else m_head = other.m_head;
        if (at != nullptr) at->m_prev = other.m_tail;
        else m_tail = other.m_tail;
        m_size += other.m_size;
        other.m_head = other.m_tail = nullptr;
        other.m_size                = 0;
    }

    void splice(const_iterator pos, LinkedList&& other) { splice(pos, other); }

    void concat(LinkedList& other) { splice(end(), other); }
    void concat(LinkedList&& other) { splice(end(), other); }

    template <class It>
    void extend(It first, It last) {
        for (; first != last; ++first) push_back(*first);
    }

    template <class C>
    void extend(const C& c) { extend(std::begin(c), std::end(c)); }

    template <class Compare = std::less<T>>
    void sort(Compare cmp = Compare()) {
        if (m_size < 2) return;
        m_head = merge_sort(m_head, m_size, cmp);
        relink();
    }

    template <class Eq = std::equal_to<T>>
    size_type unique(Eq eq = Eq()) {
        size_type removed = 0;
        if (m_head == nullptr) return removed;
        node_type* n = m_head;
        while (n->m_next != nullptr) {
            if (eq(n->m_value, n->m_next->m_value)) {
                destroy(unlink(n->m_next));
                ++removed;
            } else {
                n = n->m_next;
            }
        }
        return removed;
    }

    template <class Compare = std::less<T>>
    bool is_sorted(Compare cmp = Compare()) const {
        for (const node_type* n = m_head; n != nullptr && n->m_next != nullptr; n = n->m_next) {
            if (cmp(n->m_next->m_value, n->m_value)) return false;
        }
        return true;
    }

    iterator begin() noexcept { return iterator(m_head, this); }
    iterator end() noexcept { return iterator(nullptr, this); }
    const_iterator begin() const noexcept { return const_iterator(m_head, this); }
    const_iterator end() const noexcept { return const_iterator(nullptr, this); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    friend bool operator==(const LinkedList& a, const LinkedList& b) {
        return a.m_size == b.m_size && std::equal(a.begin(), a.end(), b.begin());
    }

    friend bool operator!=(const LinkedList& a, const LinkedList& b) { return !(a == b); }

    friend bool operator<(const LinkedList& a, const LinkedList& b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }

private:
    template <class... Args>
    static node_type* make(Args&&... args) { return new node_type(std::forward<Args>(args)...); }

    static void destroy(node_type* n) noexcept { delete n; }

    static node_type* mutable_node(const node_type* n) noexcept { return const_cast<node_type*>(n); }

    void check_nonempty() const {
        if (m_size == 0) throw std::out_of_range("LinkedList: empty list");
    }

    node_type* link_before(node_type* at, node_type* n) noexcept {
        node_type* before = at != nullptr ? at->m_prev : m_tail;
        n->m_prev         = before;
        n->m_next         = at;
        if (before != nullptr) before->m_next = n;
        else m_head = n;
        if (at != nullptr) at->m_prev = n;
        else m_tail = n;
        ++m_size;
        return n;
    }

    node_type* unlink(node_type* n) noexcept {
        if (n->m_prev != nullptr) n->m_prev->m_next = n->m_next;
        else m_head = n->m_next;
        if (n->m_next != nullptr) n->m_next->m_prev = n->m_prev;
        else m_tail = n->m_prev;
        n->m_prev = n->m_next = nullptr;
        --m_size;
        return n;
    }

    template <class Compare>
    static node_type* merge_sort(node_type* head, size_type n, Compare& cmp) {
        if (n <= 1) {
            head->m_next = nullptr;
            return head;
        }
        const size_type half = n / 2;
        node_type*      mid  = head;
        for (size_type i = 0; i < half; ++i) mid = mid->m_next;
        node_type* a = merge_sort(head, half, cmp);
        node_type* b = merge_sort(mid, n - half, cmp);
        return merge(a, b, cmp);
    }

    template <class Compare>
    static node_type* merge(node_type* a, node_type* b, Compare& cmp) {
        node_type* first = nullptr;
        node_type* last  = nullptr;
        while (a != nullptr && b != nullptr) {
            node_type* take;
            if (cmp(b->m_value, a->m_value)) {
                take = b;
                b    = b->m_next;
            } else {
                take = a;
                a    = a->m_next;
            }
            if (last != nullptr) last->m_next = take;
            else first = take;
            last = take;
        }
        node_type* rest = a != nullptr ? a : b;
        if (last != nullptr) last->m_next = rest;
        else first = rest;
        return first;
    }

    void relink() noexcept {
        node_type* prev = nullptr;
        for (node_type* n = m_head; n != nullptr; n = n->m_next) {
            n->m_prev = prev;
            prev      = n;
        }
        m_tail = prev;
    }

    node_type* m_head = nullptr;
    node_type* m_tail = nullptr;
    size_type  m_size = 0;
};

namespace list_detail {

template <class N, class = void>
struct has_next_function : std::false_type {};

template <class N>
struct has_next_function<N, decltype(void(std::declval<N&>().next()))> : std::true_type {};

struct NextOf {
    template <class N>
    auto operator()(N* n) const {
        if constexpr (has_next_function<N>::value) {
            return n->next();
        } else {
            return n->next;
        }
    }
};

template <class N, class Next>
N* meeting_point(N* head, Next& next) {
    N* slow = head;
    N* fast = head;
    while (fast != nullptr) {
        fast = next(fast);
        if (fast == nullptr) return nullptr;
        fast = next(fast);
        slow = next(slow);
        if (fast == slow) return fast;
    }
    return nullptr;
}

} // namespace list_detail

template <class N, class Next = list_detail::NextOf>
bool has_loop(N* head, Next next = Next()) {
    return list_detail::meeting_point(head, next) != nullptr;
}

template <class N, class Next = list_detail::NextOf>
N* loop_start(N* head, Next next = Next()) {
    N* meet = list_detail::meeting_point(head, next);
    if (meet == nullptr) return nullptr;
    N* a = head;
    while (a != meet) {
        a    = next(a);
        meet = next(meet);
    }
    return a;
}

template <class N, class Next = list_detail::NextOf>
std::size_t loop_length(N* head, Next next = Next()) {
    N* meet = list_detail::meeting_point(head, next);
    if (meet == nullptr) return 0;
    std::size_t len = 1;
    for (N* n = next(meet); n != meet; n = next(n)) ++len;
    return len;
}

template <class T>
void swap(LinkedList<T>& a, LinkedList<T>& b) noexcept { a.swap(b); }

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_LINKED_LIST_HPP
