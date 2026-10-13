#ifndef FIZMO_COLLECTIONS_BTREE_HPP
#define FIZMO_COLLECTIONS_BTREE_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "hash.hpp"

namespace fizmo {
namespace collections {
namespace detail {

template <class Value>
constexpr std::size_t btree_auto_degree() noexcept {
    const std::size_t per_node = 256 / sizeof(Value);
    const std::size_t t = (per_node + 1) / 2;
    return t < 2 ? 2 : (t > 64 ? 64 : t);
}

template <class K>
struct BTreeSetKeyOf {
    using stage_type = K;
    static const K& get(const K& v) noexcept { return v; }
};

template <class K, class V>
struct BTreeMapKeyOf {
    using stage_type = std::pair<K, V>;
    static const K& get(const std::pair<const K, V>& v) noexcept { return v.first; }
    static const K& get(const std::pair<K, V>& v) noexcept { return v.first; }
};

struct BTreeNoSummary {};

template <class S>
struct BTreeSummarySlot {
    S summary{};
};

template <>
struct BTreeSummarySlot<BTreeNoSummary> {};

template <class A, class V, class = void>
struct bind_augment {
    using type = A;
};

template <class A, class V>
struct bind_augment<A, V, std::void_t<typename A::template bind<V>>> {
    using type = typename A::template bind<V>;
};

template <class A, class V>
struct augment_policy {
    using type = typename bind_augment<A, V>::type;
    using summary_type = typename type::summary_type;
    static constexpr bool enabled = true;
};

template <class V>
struct augment_policy<void, V> {
    using type = void;
    using summary_type = BTreeNoSummary;
    static constexpr bool enabled = false;
};

template <class T, class = void>
struct btree_equality_comparable : std::false_type {};

template <class T>
struct btree_equality_comparable<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>> : std::true_type {};

template <class Value, std::size_t Cap, class S>
struct BTreeNode : BTreeSummarySlot<S> {
    BTreeNode*    parent = nullptr;
    std::size_t   size = 0;
    std::uint16_t count = 0;
    std::uint16_t slot = 0;
    bool          leaf = true;
    alignas(Value) unsigned char storage[sizeof(Value) * Cap];

    Value* values() noexcept { return reinterpret_cast<Value*>(storage); }
    const Value* values() const noexcept { return reinterpret_cast<const Value*>(storage); }
    Value& value(std::size_t i) noexcept { return values()[i]; }
    const Value& value(std::size_t i) const noexcept { return values()[i]; }
};

template <class Value, std::size_t Cap, class S>
struct BTreeInternalNode : BTreeNode<Value, Cap, S> {
    BTreeNode<Value, Cap, S>* children[Cap + 1];
};

template <class Tree, bool Const>
class BTreeIterator {
    template <class, bool> friend class BTreeIterator;

    using Node = typename Tree::Node;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = typename Tree::value_type;
    using difference_type   = std::ptrdiff_t;
    using pointer           = typename std::conditional<Const, const value_type*, value_type*>::type;
    using reference         = typename std::conditional<Const, const value_type&, value_type&>::type;

    BTreeIterator() = default;
    BTreeIterator(Node* node, std::size_t pos, const Tree* tree) noexcept : m_node(node), m_pos(pos), m_tree(tree) {}

    template <bool C = Const, typename std::enable_if<C, int>::type = 0>
    BTreeIterator(const BTreeIterator<Tree, false>& o) noexcept : m_node(o.m_node), m_pos(o.m_pos), m_tree(o.m_tree) {}

    reference operator*() const { return m_node->value(m_pos); }
    pointer operator->() const { return std::addressof(m_node->value(m_pos)); }

    BTreeIterator& operator++() { Tree::advance(m_node, m_pos); return *this; }
    BTreeIterator operator++(int) { BTreeIterator t = *this; ++*this; return t; }
    BTreeIterator& operator--() { Tree::retreat(m_node, m_pos, m_tree); return *this; }
    BTreeIterator operator--(int) { BTreeIterator t = *this; --*this; return t; }

    friend bool operator==(const BTreeIterator& a, const BTreeIterator& b) noexcept { return a.m_node == b.m_node && a.m_pos == b.m_pos; }
    friend bool operator!=(const BTreeIterator& a, const BTreeIterator& b) noexcept { return !(a == b); }

    Node* node() const noexcept { return m_node; }
    std::size_t position() const noexcept { return m_pos; }
    const Tree* owner() const noexcept { return m_tree; }

private:
    Node*       m_node = nullptr;
    std::size_t m_pos = 0;
    const Tree* m_tree = nullptr;
};

template <class Key, class Value, class KeyOf, class Compare, bool Multi, class Augment, std::size_t Degree>
class BTreeTable {
    using AugInfo = augment_policy<Augment, Value>;

public:
    using augment_type = typename AugInfo::type;
    using summary_type = typename AugInfo::summary_type;
    static constexpr bool augmented = AugInfo::enabled;

    static constexpr std::size_t min_degree = Degree == 0 ? btree_auto_degree<Value>() : Degree;
    static constexpr std::size_t node_capacity = 2 * min_degree - 1;

    static_assert(min_degree >= 2, "BTree minimum degree must be at least 2");
    static_assert(node_capacity <= 0xFFFF, "BTree node capacity is too large");

    using Node           = BTreeNode<Value, node_capacity, summary_type>;
    using Internal       = BTreeInternalNode<Value, node_capacity, summary_type>;
    using value_type     = Value;
    using stage_type     = typename KeyOf::stage_type;
    using size_type      = std::size_t;
    using iterator       = BTreeIterator<BTreeTable, false>;
    using const_iterator = BTreeIterator<BTreeTable, true>;

    struct Cursor {
        Node*     node;
        size_type pos;
    };

    BTreeTable() = default;
    explicit BTreeTable(const Compare& c) : m_cmp(c) {}

    BTreeTable(const BTreeTable& o) : m_cmp(o.m_cmp) {
        if (!o.m_root) return;

        try {
            m_root = o.m_root->leaf ? new_leaf() : new_internal();
            clone_into(m_root, o.m_root);
        } catch (...) {
            clear();
            throw;
        }
    }

    BTreeTable(BTreeTable&& o) noexcept : m_root(o.m_root), m_cmp(std::move(o.m_cmp)) { o.m_root = nullptr; }

    BTreeTable& operator=(const BTreeTable& o) {
        if (this != &o) {
            BTreeTable t(o);
            swap(t);
        }

        return *this;
    }

    BTreeTable& operator=(BTreeTable&& o) noexcept {
        if (this != &o) {
            clear();
            m_root = o.m_root;
            m_cmp = std::move(o.m_cmp);
            o.m_root = nullptr;
        }

        return *this;
    }

    ~BTreeTable() { clear(); }

    void swap(BTreeTable& o) noexcept {
        std::swap(m_root, o.m_root);
        std::swap(m_cmp, o.m_cmp);
    }

    size_type size() const noexcept { return m_root ? m_root->size : 0; }
    bool empty() const noexcept { return size() == 0; }
    const Compare& compare() const noexcept { return m_cmp; }

    size_type height() const noexcept {
        size_type h = 0;

        for (const Node* n = m_root; n; n = n->leaf ? nullptr : child(n, 0)) ++h;

        return h;
    }

    void clear() noexcept {
        destroy_tree(m_root);
        m_root = nullptr;
    }

    Cursor first() const noexcept {
        if (empty()) return { nullptr, 0 };
        Node* n = m_root;
        while (!n->leaf) n = child(n, 0);
        return { n, 0 };
    }

    Cursor last() const noexcept {
        if (empty()) return { nullptr, 0 };
        Node* n = m_root;
        while (!n->leaf) n = child(n, n->count);
        return { n, size_type(n->count) - 1 };
    }

    iterator make_iterator(Cursor c) noexcept { return iterator(c.node, c.pos, this); }
    const_iterator make_iterator(Cursor c) const noexcept { return const_iterator(c.node, c.pos, this); }

    template <class K>
    Cursor lower_bound(const K& key) const {
        Cursor r{ nullptr, 0 };

        for (Node* n = m_root; n;) {
            const size_type i = lower_pos(n, key);
            if (i < n->count) r = { n, i };
            if (n->leaf) break;
            n = child(n, i);
        }

        return r;
    }

    template <class K>
    Cursor upper_bound(const K& key) const {
        Cursor r{ nullptr, 0 };

        for (Node* n = m_root; n;) {
            const size_type i = upper_pos(n, key);
            if (i < n->count) r = { n, i };
            if (n->leaf) break;
            n = child(n, i);
        }

        return r;
    }

    template <class K>
    Cursor find(const K& key) const {
        for (Node* n = m_root; n;) {
            const size_type i = lower_pos(n, key);
            if (i < n->count && !m_cmp(key, key_at(n, i))) {
                if (!Multi) return { n, i };
                return lower_bound(key);
            }
            if (n->leaf) break;
            n = child(n, i);
        }

        return { nullptr, 0 };
    }

    Cursor prev(Cursor c) const noexcept {
        retreat(c.node, c.pos, this);
        return c;
    }

    Cursor next(Cursor c) const noexcept {
        advance(c.node, c.pos);
        return c;
    }

    template <class Pred>
    Cursor first_where(Pred& pred) const {
        Cursor r{ nullptr, 0 };

        for (Node* n = m_root; n;) {
            size_type lo = 0;
            size_type hi = n->count;

            while (lo < hi) {
                const size_type mid = lo + (hi - lo) / 2;

                if (pred(static_cast<const Value&>(n->value(mid)))) {
                    hi = mid;
                } else {
                    lo = mid + 1;
                }
            }

            if (lo < n->count) r = { n, lo };
            if (n->leaf) break;
            n = child(n, lo);
        }

        return r;
    }

    size_type rank(Cursor c) const noexcept {
        if (!c.node) return size();
        const Node* n = c.node;
        size_type r = c.pos;

        if (!n->leaf) {
            for (size_type j = 0; j <= c.pos; ++j) r += child(n, j)->size;
        }

        while (n->parent) {
            const Node* p = n->parent;
            const size_type s = n->slot;
            r += s;
            for (size_type j = 0; j < s; ++j) r += child(p, j)->size;
            n = p;
        }

        return r;
    }

    Cursor select(size_type r) const noexcept {
        if (r >= size()) return { nullptr, 0 };
        Node* n = m_root;

        for (;;) {
            if (n->leaf) return { n, r };

            for (size_type i = 0;; ++i) {
                const size_type s = child(n, i)->size;

                if (r < s) {
                    n = child(n, i);
                    break;
                }

                if (r == s) return { n, i };
                r -= s + 1;
            }
        }
    }

    template <class V>
    std::pair<Cursor, bool> insert_unique(V&& v) {
        const Cursor existing = find(KeyOf::get(v));
        if (existing.node) return { existing, false };
        return { insert_multi(std::forward<V>(v)), true };
    }

    template <class V>
    Cursor insert_multi(V&& v) {
        const auto& key = KeyOf::get(v);
        prepare_root();
        Node* x = m_root;

        for (;;) {
            size_type i = upper_pos(x, key);
            if (x->leaf) return place(x, i, std::forward<V>(v));
            Node* c = child(x, i);

            if (c->count == node_capacity) {
                split_child(x, i);
                if (!m_cmp(key, key_at(x, i))) ++i;
            }

            x = child(x, i);
        }
    }

    template <class V>
    std::pair<Cursor, bool> insert(V&& v) {
        if (Multi) return { insert_multi(std::forward<V>(v)), true };
        return insert_unique(std::forward<V>(v));
    }

    void erase_rank(size_type r) noexcept {
        remove_rank(m_root, r, nullptr, 0);
        shrink_root();
    }

    stage_type take_rank(size_type r) {
        const Cursor c = select(r);
        stage_type out(std::move(c.node->value(c.pos)));
        erase_rank(r);
        return out;
    }

    template <class It>
    void build_from(It first, size_type n) {
        clear();
        if (n == 0) return;
        size_type h = 1;
        while (n > subtree_max(h)) ++h;

        try {
            m_root = h == 1 ? new_leaf() : new_internal();
            fill(m_root, first, n, h);
        } catch (...) {
            clear();
            throw;
        }
    }

    bool same_key(const Key& a, const Key& b) const { return !m_cmp(a, b) && !m_cmp(b, a); }

    void stage_sorted(std::vector<stage_type>& v) const {
        std::stable_sort(v.begin(), v.end(), [this](const stage_type& a, const stage_type& b) { return m_cmp(KeyOf::get(a), KeyOf::get(b)); });
        if (!Multi) dedupe(v);
    }

    void dedupe(std::vector<stage_type>& v) const {
        v.erase(std::unique(v.begin(), v.end(), [this](const stage_type& a, const stage_type& b) { return same_key(KeyOf::get(a), KeyOf::get(b)); }), v.end());
    }

    bool is_sorted_for_build(const std::vector<stage_type>& v) const {
        for (size_type i = 1; i < v.size(); ++i) {
            const Key& a = KeyOf::get(v[i - 1]);
            const Key& b = KeyOf::get(v[i]);
            if (m_cmp(b, a)) return false;
            if (!Multi && !m_cmp(a, b)) return false;
        }

        return true;
    }

    void drain_into(std::vector<stage_type>& out, const std::vector<char>* keep) {
        out.reserve(out.size() + size());
        size_type idx = 0;

        for (Cursor c = first(); c.node; advance(c.node, c.pos), ++idx) {
            if (!keep || (*keep)[idx]) out.emplace_back(std::move(c.node->value(c.pos)));
        }

        clear();
    }

    template <class It>
    void merge_insert(It first, It last) {
        std::vector<stage_type> incoming;
        for (; first != last; ++first) incoming.emplace_back(*first);
        if (incoming.empty()) return;
        std::stable_sort(incoming.begin(), incoming.end(), [this](const stage_type& a, const stage_type& b) { return m_cmp(KeyOf::get(a), KeyOf::get(b)); });

        if (incoming.size() < size() / 8 + 16) {
            for (auto& v : incoming) insert(std::move(v));
            return;
        }

        std::vector<stage_type> mine;

        if constexpr (std::is_copy_constructible<stage_type>::value) {
            mine.reserve(size());
            for (Cursor c = this->first(); c.node; advance(c.node, c.pos)) mine.emplace_back(c.node->value(c.pos));
        } else {
            drain_into(mine, nullptr);
        }

        std::vector<stage_type> merged;
        merged.reserve(mine.size() + incoming.size());
        std::merge(std::make_move_iterator(mine.begin()), std::make_move_iterator(mine.end()), std::make_move_iterator(incoming.begin()), std::make_move_iterator(incoming.end()), std::back_inserter(merged), [this](const stage_type& a, const stage_type& b) { return m_cmp(KeyOf::get(a), KeyOf::get(b)); });
        if (!Multi) dedupe(merged);
        build_from(std::make_move_iterator(merged.begin()), merged.size());
    }

    bool validate() const {
        if (!m_root) return true;
        if (m_root->parent) return false;
        size_type leaf_depth = 0;
        if (!validate_node(m_root, 1, leaf_depth)) return false;
        const Node* prev_node = nullptr;
        size_type prev_pos = 0;
        size_type seen = 0;

        for (Cursor c = first(); c.node; advance(c.node, c.pos)) {
            if (prev_node) {
                const Key& a = key_at(prev_node, prev_pos);
                const Key& b = key_at(c.node, c.pos);
                if (m_cmp(b, a)) return false;
                if (!Multi && !m_cmp(a, b)) return false;
            }

            prev_node = c.node;
            prev_pos = c.pos;
            ++seen;
        }

        return seen == size();
    }

    static void advance(Node*& n, size_type& p) noexcept {
        if (!n->leaf) {
            n = child(n, p + 1);
            while (!n->leaf) n = child(n, 0);
            p = 0;
            return;
        }

        if (++p < n->count) return;

        while (n->parent) {
            const size_type s = n->slot;
            n = n->parent;

            if (s < n->count) {
                p = s;
                return;
            }
        }

        n = nullptr;
        p = 0;
    }

    static void retreat(Node*& n, size_type& p, const BTreeTable* t) noexcept {
        if (!n) {
            const Cursor c = t->last();
            n = c.node;
            p = c.pos;
            return;
        }

        if (!n->leaf) {
            n = child(n, p);
            while (!n->leaf) n = child(n, n->count);
            p = size_type(n->count) - 1;
            return;
        }

        if (p > 0) {
            --p;
            return;
        }

        while (n->parent) {
            const size_type s = n->slot;
            n = n->parent;

            if (s > 0) {
                p = s - 1;
                return;
            }
        }

        n = nullptr;
        p = 0;
    }

    static const Key& key_at(const Node* n, size_type i) noexcept { return KeyOf::get(n->value(i)); }

    summary_type summary() const {
        if (!m_root) return augment_type::identity();
        return m_root->summary;
    }

    summary_type fold(size_type lo, size_type hi) const {
        if (hi > size()) hi = size();
        if (!m_root || lo >= hi) return augment_type::identity();
        return fold_ranks(m_root, lo, hi);
    }

    template <class May, class Match>
    Cursor first_match(size_type lo, size_type hi, May& may, Match& match) const {
        if (hi > size()) hi = size();
        if (!m_root || lo >= hi || !may(static_cast<const summary_type&>(m_root->summary))) return { nullptr, 0 };
        return first_in_ranks(m_root, lo, hi, may, match);
    }

    template <class May, class Match>
    Cursor last_match(size_type lo, size_type hi, May& may, Match& match) const {
        if (hi > size()) hi = size();
        if (!m_root || lo >= hi || !may(static_cast<const summary_type&>(m_root->summary))) return { nullptr, 0 };
        return last_in_ranks(m_root, lo, hi, may, match);
    }

    template <class May, class Match, class Fn>
    size_type for_each_match(May& may, Match& match, Fn& fn) const {
        if (!m_root || !may(static_cast<const summary_type&>(m_root->summary))) return 0;
        return visit_matches(m_root, may, match, fn);
    }

    void refresh_from(Cursor c) { refresh_path(c.node); }

    template <class Fn>
    std::pair<Cursor, bool> modify(Cursor c, Fn& fn) {
        try {
            fn(c.node->value(c.pos));
        } catch (...) {
            settle(c);
            throw;
        }

        return settle(c);
    }

private:
    std::pair<Cursor, bool> settle(Cursor c) {
        const Key& k = key_at(c.node, c.pos);
        const Cursor p = prev(c);
        const Cursor n = next(c);
        bool bad = (p.node && m_cmp(k, key_at(p.node, p.pos))) || (n.node && m_cmp(key_at(n.node, n.pos), k));
        if (!Multi && !bad) bad = (p.node && !m_cmp(key_at(p.node, p.pos), k)) || (n.node && !m_cmp(k, key_at(n.node, n.pos)));

        if (!bad) {
            refresh_path(c.node);
            return { c, true };
        }

        stage_type v = take_rank(rank(c));
        return insert(std::move(v));
    }

    Node*   m_root = nullptr;
    Compare m_cmp;

    static Node*& child(Node* n, size_type i) noexcept { return static_cast<Internal*>(n)->children[i]; }
    static Node* child(const Node* n, size_type i) noexcept { return static_cast<const Internal*>(n)->children[i]; }

    static Node* new_leaf() { return new Node(); }

    static Node* new_internal() {
        Internal* n = new Internal();
        n->leaf = false;
        std::fill(n->children, n->children + node_capacity + 1, nullptr);
        return n;
    }

    static void free_node(Node* n) noexcept {
        if (n->leaf) {
            delete n;
        } else {
            delete static_cast<Internal*>(n);
        }
    }

    static void destroy_tree(Node* n) noexcept {
        if (!n) return;
        for (size_type i = 0; i < n->count; ++i) n->value(i).~Value();

        if (!n->leaf) {
            for (size_type i = 0; i <= n->count; ++i) destroy_tree(child(n, i));
        }

        free_node(n);
    }

    static void set_child(Node* n, size_type i, Node* c) noexcept {
        child(n, i) = c;
        c->parent = n;
        c->slot = static_cast<std::uint16_t>(i);
    }

    static void relocate(Node* dn, size_type di, Node* sn, size_type si) noexcept {
        ::new (static_cast<void*>(dn->values() + di)) Value(std::move(sn->value(si)));
        sn->value(si).~Value();
    }

    static void shift_values_right(Node* n, size_type from) noexcept {
        for (size_type j = n->count; j > from; --j) relocate(n, j, n, j - 1);
    }

    static void shift_values_left(Node* n, size_type from) noexcept {
        for (size_type j = from; j + 1 < n->count; ++j) relocate(n, j, n, j + 1);
    }

    static void shift_children_right(Node* n, size_type from) noexcept {
        for (size_type j = size_type(n->count) + 1; j > from; --j) set_child(n, j, child(n, j - 1));
    }

    static void shift_children_left(Node* n, size_type from) noexcept {
        for (size_type j = from; j < n->count; ++j) set_child(n, j, child(n, j + 1));
    }

    template <class K>
    size_type lower_pos(const Node* n, const K& key) const {
        size_type lo = 0;
        size_type hi = n->count;

        while (lo < hi) {
            const size_type mid = lo + (hi - lo) / 2;

            if (m_cmp(key_at(n, mid), key)) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }

        return lo;
    }

    template <class K>
    size_type upper_pos(const Node* n, const K& key) const {
        size_type lo = 0;
        size_type hi = n->count;

        while (lo < hi) {
            const size_type mid = lo + (hi - lo) / 2;

            if (m_cmp(key, key_at(n, mid))) {
                hi = mid;
            } else {
                lo = mid + 1;
            }
        }

        return lo;
    }

    void prepare_root() {
        if (!m_root) {
            m_root = new_leaf();
            return;
        }

        if (m_root->count < node_capacity) return;
        Node* r = new_internal();
        Node* old = m_root;
        set_child(r, 0, old);
        r->size = old->size;
        m_root = r;

        try {
            split_child(r, 0);
            refresh(r);
        } catch (...) {
            m_root = old;
            old->parent = nullptr;
            old->slot = 0;
            free_node(r);
            throw;
        }
    }

    template <class V>
    Cursor place(Node* x, size_type i, V&& v) {
        shift_values_right(x, i);

        try {
            ::new (static_cast<void*>(x->values() + i)) Value(std::forward<V>(v));
        } catch (...) {
            shift_values_left(x, i);

            if (x == m_root && x->count == 0) {
                free_node(x);
                m_root = nullptr;
            }

            throw;
        }

        ++x->count;

        for (Node* p = x; p; p = p->parent) {
            ++p->size;
            refresh(p);
        }

        return { x, i };
    }

    void split_child(Node* x, size_type i) {
        Node* y = child(x, i);
        Node* z = y->leaf ? new_leaf() : new_internal();
        const size_type t = min_degree;

        for (size_type j = 0; j + 1 < t; ++j) relocate(z, j, y, t + j);
        z->count = static_cast<std::uint16_t>(t - 1);
        z->size = t - 1;

        if (!y->leaf) {
            for (size_type j = 0; j < t; ++j) {
                set_child(z, j, child(y, t + j));
                z->size += child(z, j)->size;
            }
        }

        shift_values_right(x, i);
        relocate(x, i, y, t - 1);
        shift_children_right(x, i + 1);
        set_child(x, i + 1, z);
        y->count = static_cast<std::uint16_t>(t - 1);
        ++x->count;
        y->size -= z->size + 1;
        refresh(y);
        refresh(z);
    }

    static summary_type compute_summary(const Node* n) {
        if constexpr (augmented) {
            summary_type s = augment_type::identity();

            if (n->leaf) {
                for (size_type i = 0; i < n->count; ++i) s = augment_type::combine(s, augment_type::of(n->value(i)));
            } else {
                s = child(n, 0)->summary;

                for (size_type i = 0; i < n->count; ++i) {
                    s = augment_type::combine(s, augment_type::of(n->value(i)));
                    s = augment_type::combine(s, child(n, i + 1)->summary);
                }
            }

            return s;
        } else {
            return summary_type{};
        }
    }

    static void refresh(Node* n) {
        if constexpr (augmented) {
            n->summary = compute_summary(n);
        }
    }

    static void refresh_path(Node* n) {
        if constexpr (augmented) {
            for (; n; n = n->parent) refresh(n);
        }
    }

    template <class May, class Match>
    static Cursor first_in_ranks(Node* n, size_type lo, size_type hi, May& may, Match& match) {
        size_type base = 0;

        for (size_type i = 0; i <= n->count; ++i) {
            if (base >= hi) break;

            if (!n->leaf) {
                Node* c = child(n, i);
                const size_type s = c->size;

                if (base + s > lo && may(static_cast<const summary_type&>(c->summary))) {
                    const size_type clo = lo > base ? lo - base : 0;
                    const size_type chi = hi - base < s ? hi - base : s;
                    const Cursor r = first_in_ranks(c, clo, chi, may, match);
                    if (r.node) return r;
                }

                base += s;
            }

            if (i < n->count) {
                if (base >= hi) break;
                if (base >= lo && match(static_cast<const Value&>(n->value(i)))) return { n, i };
                ++base;
            }
        }

        return { nullptr, 0 };
    }

    template <class May, class Match>
    static Cursor last_in_ranks(Node* n, size_type lo, size_type hi, May& may, Match& match) {
        size_type base = n->size;

        for (size_type k = size_type(n->count) + 1; k-- > 0;) {
            if (base <= lo) break;

            if (!n->leaf) {
                Node* c = child(n, k);
                const size_type s = c->size;
                const size_type start = base - s;

                if (start < hi && may(static_cast<const summary_type&>(c->summary))) {
                    const size_type clo = lo > start ? lo - start : 0;
                    const size_type chi = hi - start < s ? hi - start : s;
                    const Cursor r = last_in_ranks(c, clo, chi, may, match);
                    if (r.node) return r;
                }

                base = start;
            }

            if (k > 0) {
                if (base <= lo) break;
                const size_type rk = base - 1;
                if (rk < hi && match(static_cast<const Value&>(n->value(k - 1)))) return { n, k - 1 };
                base = rk;
            }
        }

        return { nullptr, 0 };
    }

    template <class May, class Match, class Fn>
    static size_type visit_matches(const Node* n, May& may, Match& match, Fn& fn) {
        size_type hits = 0;

        for (size_type i = 0; i <= n->count; ++i) {
            if (!n->leaf) {
                const Node* c = child(n, i);
                if (may(static_cast<const summary_type&>(c->summary))) hits += visit_matches(c, may, match, fn);
            }

            if (i < n->count && match(n->value(i))) {
                fn(n->value(i));
                ++hits;
            }
        }

        return hits;
    }

    static summary_type fold_ranks(const Node* n, size_type lo, size_type hi) {
        if (lo == 0 && hi == n->size) return n->summary;
        summary_type acc = augment_type::identity();
        size_type base = 0;

        for (size_type i = 0; i <= n->count && base < hi; ++i) {
            if (!n->leaf) {
                const Node* c = child(n, i);
                const size_type s = c->size;

                if (lo < base + s && hi > base) {
                    const size_type clo = lo > base ? lo - base : 0;
                    const size_type chi = hi - base < s ? hi - base : s;
                    acc = augment_type::combine(acc, fold_ranks(c, clo, chi));
                }

                base += s;
            }

            if (i < n->count) {
                if (base >= lo && base < hi) acc = augment_type::combine(acc, augment_type::of(n->value(i)));
                ++base;
            }
        }

        return acc;
    }

    static void take_value(Node* x, size_type i, Node* sink, size_type sink_pos) noexcept {
        if (sink) {
            relocate(sink, sink_pos, x, i);
        } else {
            x->value(i).~Value();
        }
    }

    void remove_rank(Node* x, size_type r, Node* sink, size_type sink_pos) noexcept {
        const size_type t = min_degree;

        for (;;) {
            --x->size;

            if (x->leaf) {
                take_value(x, r, sink, sink_pos);
                shift_values_left(x, r);
                --x->count;
                refresh_path(x);
                return;
            }

            size_type i = 0;
            bool here = false;

            for (;; ++i) {
                const size_type s = child(x, i)->size;
                if (r < s) break;

                if (r == s) {
                    here = true;
                    break;
                }

                r -= s + 1;
            }

            if (here) {
                Node* y = child(x, i);
                Node* z = child(x, i + 1);

                if (y->count >= t) {
                    take_value(x, i, sink, sink_pos);
                    remove_rank(y, y->size - 1, x, i);
                    return;
                }

                if (z->count >= t) {
                    take_value(x, i, sink, sink_pos);
                    remove_rank(z, 0, x, i);
                    return;
                }

                const size_type left = y->size;
                merge_children(x, i);
                x = y;
                r = left;
                continue;
            }

            if (child(x, i)->count < t) {
                Node* left = i > 0 ? child(x, i - 1) : nullptr;
                Node* right = i < x->count ? child(x, i + 1) : nullptr;

                if (left && left->count >= t) {
                    r += rotate_right(x, i - 1);
                } else if (right && right->count >= t) {
                    rotate_left(x, i);
                } else if (right) {
                    merge_children(x, i);
                } else {
                    r += left->size + 1;
                    merge_children(x, i - 1);
                    --i;
                }
            }

            x = child(x, i);
        }
    }

    static size_type rotate_right(Node* x, size_type k) noexcept {
        Node* l = child(x, k);
        Node* c = child(x, k + 1);
        size_type moved = 0;

        shift_values_right(c, 0);
        relocate(c, 0, x, k);

        if (!c->leaf) {
            shift_children_right(c, 0);
            Node* m = child(l, l->count);
            set_child(c, 0, m);
            moved = m->size;
        }

        relocate(x, k, l, size_type(l->count) - 1);
        --l->count;
        ++c->count;
        l->size -= moved + 1;
        c->size += moved + 1;
        refresh(l);
        return moved + 1;
    }

    static void rotate_left(Node* x, size_type i) noexcept {
        Node* c = child(x, i);
        Node* r = child(x, i + 1);
        size_type moved = 0;

        relocate(c, c->count, x, i);

        if (!c->leaf) {
            Node* m = child(r, 0);
            set_child(c, size_type(c->count) + 1, m);
            moved = m->size;
            shift_children_left(r, 0);
        }

        relocate(x, i, r, 0);
        shift_values_left(r, 0);
        ++c->count;
        --r->count;
        r->size -= moved + 1;
        c->size += moved + 1;
        refresh(r);
    }

    static void merge_children(Node* x, size_type i) noexcept {
        Node* y = child(x, i);
        Node* z = child(x, i + 1);
        const size_type yc = y->count;

        relocate(y, yc, x, i);
        for (size_type j = 0; j < z->count; ++j) relocate(y, yc + 1 + j, z, j);

        if (!y->leaf) {
            for (size_type j = 0; j <= z->count; ++j) set_child(y, yc + 1 + j, child(z, j));
        }

        y->count = static_cast<std::uint16_t>(yc + 1 + z->count);
        y->size += 1 + z->size;
        shift_values_left(x, i);
        shift_children_left(x, i + 1);
        --x->count;
        z->count = 0;
        free_node(z);
    }

    void shrink_root() noexcept {
        while (m_root && m_root->count == 0) {
            if (m_root->leaf) {
                free_node(m_root);
                m_root = nullptr;
                return;
            }

            Node* c = child(m_root, 0);
            free_node(m_root);
            m_root = c;
            c->parent = nullptr;
            c->slot = 0;
        }
    }

    static size_type subtree_max(size_type h) noexcept {
        const size_type limit = std::numeric_limits<size_type>::max();
        const size_type fanout = 2 * min_degree;
        size_type m = 1;

        for (size_type j = 0; j < h; ++j) {
            if (m > limit / fanout) return limit;
            m *= fanout;
        }

        return m - 1;
    }

    template <class It>
    void fill(Node* node, It& it, size_type n, size_type h) {
        node->size = n;

        if (h == 1) {
            for (size_type k = 0; k < n; ++k) {
                ::new (static_cast<void*>(node->values() + k)) Value(*it);
                ++it;
                ++node->count;
            }

            refresh(node);
            return;
        }

        const size_type per_child = subtree_max(h - 1) + 1;
        size_type c = n / per_child + 1;
        const size_type min_c = node->parent ? min_degree : 2;
        if (c < min_c) c = min_c;

        const size_type rem = n - (c - 1);
        const size_type base = rem / c;
        const size_type extra = rem % c;

        for (size_type j = 0; j < c; ++j) {
            if (j > 0) {
                ::new (static_cast<void*>(node->values() + (j - 1))) Value(*it);
                ++it;
                node->count = static_cast<std::uint16_t>(j);
            }

            Node* ch = h - 1 == 1 ? new_leaf() : new_internal();
            set_child(node, j, ch);
            fill(ch, it, base + (j < extra ? 1 : 0), h - 1);
        }

        refresh(node);
    }

    void clone_into(Node* dst, const Node* src) {
        for (size_type i = 0; i < src->count; ++i) {
            ::new (static_cast<void*>(dst->values() + i)) Value(src->value(i));
            ++dst->count;
        }

        dst->size = src->size;

        if constexpr (augmented) {
            dst->summary = src->summary;
        }

        if (src->leaf) return;

        for (size_type i = 0; i <= src->count; ++i) {
            const Node* sc = child(src, i);
            Node* dc = sc->leaf ? new_leaf() : new_internal();
            set_child(dst, i, dc);
            clone_into(dc, sc);
        }
    }

    bool validate_node(const Node* n, size_type depth, size_type& leaf_depth) const {
        if (n != m_root && n->count < min_degree - 1) return false;
        if (n->count > node_capacity) return false;
        size_type total = n->count;

        if (n->leaf) {
            if (leaf_depth == 0) leaf_depth = depth;
            if (leaf_depth != depth) return false;
        } else {
            for (size_type i = 0; i <= n->count; ++i) {
                const Node* c = child(n, i);
                if (!c || c->parent != n || c->slot != i) return false;
                if (i > 0 && m_cmp(key_at(c, 0), key_at(n, i - 1))) return false;
                if (i < n->count && m_cmp(key_at(n, i), key_at(c, size_type(c->count) - 1))) return false;
                if (!validate_node(c, depth + 1, leaf_depth)) return false;
                total += c->size;
            }
        }

        if constexpr (augmented && btree_equality_comparable<summary_type>::value) {
            if (!(compute_summary(n) == n->summary)) return false;
        }

        return total == n->size;
    }
};

template <class Key, class Value, class KeyOf, class Compare, bool Multi, class Augment, std::size_t Degree, bool IsSet>
class BTreeBase {
protected:
    using Table  = BTreeTable<Key, Value, KeyOf, Compare, Multi, Augment, Degree>;
    using Cursor = typename Table::Cursor;
    using stage_type = typename Table::stage_type;
    Table m_t;

public:
    using key_type       = Key;
    using value_type     = Value;
    using size_type      = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare    = Compare;
    using const_iterator = typename Table::const_iterator;
    using iterator       = typename std::conditional<IsSet || Table::augmented, const_iterator, typename Table::iterator>::type;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type min_degree = Table::min_degree;
    static constexpr size_type node_capacity = Table::node_capacity;
    static constexpr bool augmented = Table::augmented;

    using augment_type = typename Table::augment_type;
    using summary_type = typename Table::summary_type;

    BTreeBase() = default;
    explicit BTreeBase(const Compare& c) : m_t(c) {}

    size_type size() const noexcept { return m_t.size(); }
    bool empty() const noexcept { return m_t.empty(); }
    explicit operator bool() const noexcept { return !empty(); }
    void clear() noexcept { m_t.clear(); }
    size_type height() const noexcept { return m_t.height(); }
    key_compare key_comp() const { return m_t.compare(); }

    iterator begin() noexcept { return it(m_t.first()); }
    iterator end() noexcept { return it({ nullptr, 0 }); }
    const_iterator begin() const noexcept { return cit(m_t.first()); }
    const_iterator end() const noexcept { return cit({ nullptr, 0 }); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    iterator find(const Key& key) { return it(m_t.find(key)); }
    const_iterator find(const Key& key) const { return cit(m_t.find(key)); }
    bool contains(const Key& key) const { return m_t.find(key).node != nullptr; }

    size_type count(const Key& key) const {
        if (!Multi) return contains(key) ? 1 : 0;
        return m_t.rank(m_t.upper_bound(key)) - m_t.rank(m_t.lower_bound(key));
    }

    iterator lower_bound(const Key& key) { return it(m_t.lower_bound(key)); }
    const_iterator lower_bound(const Key& key) const { return cit(m_t.lower_bound(key)); }
    iterator upper_bound(const Key& key) { return it(m_t.upper_bound(key)); }
    const_iterator upper_bound(const Key& key) const { return cit(m_t.upper_bound(key)); }
    std::pair<iterator, iterator> equal_range(const Key& key) { return { lower_bound(key), upper_bound(key) }; }
    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const { return { lower_bound(key), upper_bound(key) }; }

    iterator floor(const Key& key) { return it(before(m_t.upper_bound(key))); }
    const_iterator floor(const Key& key) const { return cit(before(m_t.upper_bound(key))); }
    iterator ceiling(const Key& key) { return lower_bound(key); }
    const_iterator ceiling(const Key& key) const { return lower_bound(key); }
    iterator lower(const Key& key) { return it(before(m_t.lower_bound(key))); }
    const_iterator lower(const Key& key) const { return cit(before(m_t.lower_bound(key))); }
    iterator higher(const Key& key) { return upper_bound(key); }
    const_iterator higher(const Key& key) const { return upper_bound(key); }

    std::pair<iterator, iterator> range(const Key& lo, const Key& hi) { return { lower_bound(lo), lower_bound(hi) }; }
    std::pair<const_iterator, const_iterator> range(const Key& lo, const Key& hi) const { return { lower_bound(lo), lower_bound(hi) }; }

    size_type count_range(const Key& lo, const Key& hi) const {
        const size_type a = m_t.rank(m_t.lower_bound(lo));
        const size_type b = m_t.rank(m_t.lower_bound(hi));
        return b > a ? b - a : 0;
    }

    size_type bisect_left(const Key& key) const { return m_t.rank(m_t.lower_bound(key)); }
    size_type bisect_right(const Key& key) const { return m_t.rank(m_t.upper_bound(key)); }

    size_type index_of(const Key& key) const {
        const Cursor c = m_t.find(key);
        if (!c.node) throw std::out_of_range("BTree: key not present");
        return m_t.rank(c);
    }

    size_type index_of(const_iterator pos) const { return m_t.rank({ pos.node(), pos.position() }); }

    iterator nth(size_type i) { return it(m_t.select(i)); }
    const_iterator nth(size_type i) const { return cit(m_t.select(i)); }

    template <class Pred>
    iterator first_where(Pred pred) { return it(m_t.first_where(pred)); }

    template <class Pred>
    const_iterator first_where(Pred pred) const { return cit(m_t.first_where(pred)); }

    template <class Pred>
    iterator last_where(Pred pred) { return it(last_true(pred)); }

    template <class Pred>
    const_iterator last_where(Pred pred) const { return cit(last_true(pred)); }

    template <class Pred>
    iterator partition_point(Pred pred) {
        auto neg = [&pred](const Value& v) { return !pred(v); };
        return it(m_t.first_where(neg));
    }

    template <class Pred>
    const_iterator partition_point(Pred pred) const {
        auto neg = [&pred](const Value& v) { return !pred(v); };
        return cit(m_t.first_where(neg));
    }

    template <class Pred>
    size_type index_where(Pred pred) const { return m_t.rank(m_t.first_where(pred)); }

    template <class Pred>
    size_type count_where(Pred pred) const { return size() - index_where(pred); }

    template <class Pred>
    iterator find_if(Pred pred) {
        for (auto i = begin(); i != end(); ++i) {
            if (pred(static_cast<const Value&>(*i))) return i;
        }

        return end();
    }

    template <class Pred>
    const_iterator find_if(Pred pred) const {
        for (auto i = begin(); i != end(); ++i) {
            if (pred(*i)) return i;
        }

        return end();
    }

    template <class Pred>
    iterator find_last_if(Pred pred) {
        for (auto i = rbegin(); i != rend(); ++i) {
            if (pred(static_cast<const Value&>(*i))) return std::prev(i.base());
        }

        return end();
    }

    template <class Pred>
    const_iterator find_last_if(Pred pred) const {
        for (auto i = rbegin(); i != rend(); ++i) {
            if (pred(*i)) return std::prev(i.base());
        }

        return end();
    }

    template <class Pred>
    iterator find_if(const Key& lo, const Key& hi, Pred pred) {
        const iterator stop = lower_bound(hi);

        for (auto i = lower_bound(lo); i != stop; ++i) {
            if (pred(static_cast<const Value&>(*i))) return i;
        }

        return end();
    }

    template <class Pred>
    const_iterator find_if(const Key& lo, const Key& hi, Pred pred) const {
        const const_iterator stop = lower_bound(hi);

        for (auto i = lower_bound(lo); i != stop; ++i) {
            if (pred(*i)) return i;
        }

        return end();
    }

    template <class Pred>
    size_type count_if(Pred pred) const {
        size_type n = 0;
        for (const auto& v : *this) n += pred(v) ? 1 : 0;
        return n;
    }

    template <class Pred>
    bool any_of(Pred pred) const { return find_if(pred) != end(); }

    template <class Pred>
    bool all_of(Pred pred) const {
        for (const auto& v : *this) {
            if (!pred(v)) return false;
        }

        return true;
    }

    template <class Pred>
    bool none_of(Pred pred) const { return !any_of(pred); }

    template <class Pred>
    std::vector<Value> filter(Pred pred) const {
        std::vector<Value> out;

        for (const auto& v : *this) {
            if (pred(v)) out.push_back(v);
        }

        return out;
    }

    template <class Pred>
    std::vector<Value> filter(const Key& lo, const Key& hi, Pred pred) const {
        std::vector<Value> out;
        const const_iterator stop = lower_bound(hi);

        for (auto i = lower_bound(lo); i != stop; ++i) {
            if (pred(*i)) out.push_back(*i);
        }

        return out;
    }

    template <class Fn>
    void for_each(Fn fn) {
        for (auto i = begin(); i != end(); ++i) fn(*i);
    }

    template <class Fn>
    void for_each(Fn fn) const {
        for (const auto& v : *this) fn(v);
    }

    template <class Pred, class Fn>
    size_type for_each_where(Pred pred, Fn fn) {
        size_type n = 0;

        for (auto i = begin(); i != end(); ++i) {
            if (pred(static_cast<const Value&>(*i))) {
                fn(*i);
                ++n;
            }
        }

        return n;
    }

    template <class Pred, class Fn>
    size_type for_each_where(Pred pred, Fn fn) const {
        size_type n = 0;

        for (const auto& v : *this) {
            if (pred(v)) {
                fn(v);
                ++n;
            }
        }

        return n;
    }

    template <class Pred>
    size_type erase_if(Pred pred) {
        std::vector<char> keep;
        keep.reserve(size());
        size_type removed = 0;

        for (const auto& v : static_cast<const BTreeBase&>(*this)) {
            const bool drop = static_cast<bool>(pred(v));
            keep.push_back(drop ? 0 : 1);
            removed += drop ? 1 : 0;
        }

        if (removed == 0) return 0;
        std::vector<stage_type> kept;
        m_t.drain_into(kept, &keep);
        m_t.build_from(std::make_move_iterator(kept.begin()), kept.size());
        return removed;
    }

    iterator erase(const_iterator pos) {
        const size_type r = m_t.rank({ pos.node(), pos.position() });
        m_t.erase_rank(r);
        return it(m_t.select(r));
    }

    iterator erase(const_iterator first, const_iterator last) {
        const size_type a = m_t.rank({ first.node(), first.position() });
        const size_type b = m_t.rank({ last.node(), last.position() });
        if (b <= a) return it(m_t.select(a));

        if (b - a > size() / 4) {
            std::vector<char> keep(size(), 1);
            std::fill(keep.begin() + static_cast<difference_type>(a), keep.begin() + static_cast<difference_type>(b), 0);
            std::vector<stage_type> kept;
            m_t.drain_into(kept, &keep);
            m_t.build_from(std::make_move_iterator(kept.begin()), kept.size());
        } else {
            for (size_type k = a; k < b; ++k) m_t.erase_rank(a);
        }

        return it(m_t.select(a));
    }

    size_type erase(const Key& key) {
        if (!Multi) return discard(key) ? 1 : 0;
        const size_type a = m_t.rank(m_t.lower_bound(key));
        const size_type b = m_t.rank(m_t.upper_bound(key));
        for (size_type k = a; k < b; ++k) m_t.erase_rank(a);
        return b - a;
    }

    bool discard(const Key& key) {
        const Cursor c = m_t.find(key);
        if (!c.node) return false;
        m_t.erase_rank(m_t.rank(c));
        return true;
    }

    void remove(const Key& key) {
        if (!discard(key)) throw std::out_of_range("BTree: key not present");
    }

    stage_type pop_first() {
        if (empty()) throw std::out_of_range("BTree is empty");
        return m_t.take_rank(0);
    }

    stage_type pop_last() {
        if (empty()) throw std::out_of_range("BTree is empty");
        return m_t.take_rank(size() - 1);
    }

    stage_type pop_index(size_type i) {
        if (i >= size()) throw std::out_of_range("BTree index out of range");
        return m_t.take_rank(i);
    }

    std::vector<stage_type> to_vector() const { return std::vector<stage_type>(begin(), end()); }

    template <class It>
    void insert(It first, It last) { m_t.merge_insert(first, last); }

    template <class C>
    void update(const C& other) {
        using std::begin;
        using std::end;
        m_t.merge_insert(begin(other), end(other));
    }

    void assign_sorted(std::vector<stage_type> values) {
        if (!m_t.is_sorted_for_build(values)) throw std::invalid_argument("BTree::assign_sorted: input is not sorted (or has duplicates for a unique container)");
        m_t.build_from(std::make_move_iterator(values.begin()), values.size());
    }

    bool validate() const { return m_t.validate(); }

    summary_type summary() const {
        static_assert(augmented, "BTree::summary requires an Augment policy");
        return m_t.summary();
    }

    summary_type summarize(const Key& lo, const Key& hi) const {
        static_assert(augmented, "BTree::summarize requires an Augment policy");
        return m_t.fold(bisect_left(lo), bisect_left(hi));
    }

    summary_type summarize_index(size_type first, size_type last) const {
        static_assert(augmented, "BTree::summarize_index requires an Augment policy");
        return m_t.fold(first, last);
    }

    template <class May>
    const_iterator find_first(May may) const {
        auto match = exact(may);
        return find_first(may, match);
    }

    template <class May, class Match>
    const_iterator find_first(May may, Match match) const {
        static_assert(augmented, "BTree::find_first requires an Augment policy");
        return cit(m_t.first_match(0, size(), may, match));
    }

    template <class May>
    const_iterator find_first(const Key& lo, const Key& hi, May may) const {
        auto match = exact(may);
        return find_first(lo, hi, may, match);
    }

    template <class May, class Match>
    const_iterator find_first(const Key& lo, const Key& hi, May may, Match match) const {
        static_assert(augmented, "BTree::find_first requires an Augment policy");
        return cit(m_t.first_match(bisect_left(lo), bisect_left(hi), may, match));
    }

    template <class May>
    const_iterator find_last(May may) const {
        auto match = exact(may);
        return find_last(may, match);
    }

    template <class May, class Match>
    const_iterator find_last(May may, Match match) const {
        static_assert(augmented, "BTree::find_last requires an Augment policy");
        return cit(m_t.last_match(0, size(), may, match));
    }

    template <class May>
    const_iterator find_next(const_iterator after, May may) const {
        auto match = exact(may);
        return find_next(after, may, match);
    }

    template <class May, class Match>
    const_iterator find_next(const_iterator after, May may, Match match) const {
        static_assert(augmented, "BTree::find_next requires an Augment policy");
        if (after == end()) return end();
        return cit(m_t.first_match(m_t.rank(cursor(after)) + 1, size(), may, match));
    }

    template <class May>
    const_iterator find_prev(const_iterator before, May may) const {
        auto match = exact(may);
        return find_prev(before, may, match);
    }

    template <class May, class Match>
    const_iterator find_prev(const_iterator before, May may, Match match) const {
        static_assert(augmented, "BTree::find_prev requires an Augment policy");
        return cit(m_t.last_match(0, m_t.rank(cursor(before)), may, match));
    }

    template <class May, class Fn>
    size_type for_each_match(May may, Fn fn) const {
        auto match = exact(may);
        return for_each_match(may, match, fn);
    }

    template <class May, class Match, class Fn>
    size_type for_each_match(May may, Match match, Fn fn) const {
        static_assert(augmented, "BTree::for_each_match requires an Augment policy");
        return m_t.for_each_match(may, match, fn);
    }

    template <class May>
    std::vector<Value> find_all(May may) const {
        auto match = exact(may);
        return find_all(may, match);
    }

    template <class May, class Match>
    std::vector<Value> find_all(May may, Match match) const {
        std::vector<Value> out;
        for_each_match(may, match, [&out](const Value& v) { out.push_back(v); });
        return out;
    }

    template <class May>
    size_type count_all(May may) const {
        auto match = exact(may);
        return count_all(may, match);
    }

    template <class May, class Match>
    size_type count_all(May may, Match match) const {
        return for_each_match(may, match, [](const Value&) {});
    }

protected:
    static Cursor cursor(const_iterator i) noexcept { return { i.node(), i.position() }; }

    template <class May>
    static auto exact(May& may) {
        return [&may](const Value& v) {
            const summary_type s = augment_type::of(v);
            return static_cast<bool>(may(s));
        };
    }

    iterator it(Cursor c) noexcept { return m_t.make_iterator(c); }
    const_iterator cit(Cursor c) const noexcept { return m_t.make_iterator(c); }

    Cursor before(Cursor c) const noexcept { return m_t.prev(c); }

    template <class Pred>
    Cursor last_true(Pred& pred) const {
        auto neg = [&pred](const Value& v) { return !pred(v); };
        return before(m_t.first_where(neg));
    }

    template <class It>
    void construct_from(It first, It last) {
        std::vector<stage_type> v;
        for (; first != last; ++first) v.emplace_back(*first);
        m_t.stage_sorted(v);
        m_t.build_from(std::make_move_iterator(v.begin()), v.size());
    }

    const Value& checked_value(Cursor c) const {
        if (!c.node) throw std::out_of_range("BTree is empty");
        return c.node->value(c.pos);
    }

    Value& checked_value(Cursor c) {
        if (!c.node) throw std::out_of_range("BTree is empty");
        return c.node->value(c.pos);
    }
};

} // namespace detail

namespace augment {

template <auto Member>
struct Field {
    template <class V>
    decltype(auto) operator()(const V& v) const {
        if constexpr (std::is_invocable<decltype(Member), const V&>::value) {
            return std::invoke(Member, v);
        } else {
            return std::invoke(Member, v.second);
        }
    }
};

template <class Proj, class V>
using projected_t = typename std::decay<typename std::invoke_result<const Proj&, const V&>::type>::type;

template <class Proj>
struct Min {
    template <class V>
    struct bind {
        using summary_type = projected_t<Proj, V>;
        static_assert(std::numeric_limits<summary_type>::is_specialized, "augment::Min needs a projection whose type has std::numeric_limits");
        static summary_type identity() { return std::numeric_limits<summary_type>::max(); }
        static summary_type of(const V& v) { return Proj()(v); }
        static summary_type combine(const summary_type& a, const summary_type& b) { return b < a ? b : a; }
    };
};

template <class Proj>
struct Max {
    template <class V>
    struct bind {
        using summary_type = projected_t<Proj, V>;
        static_assert(std::numeric_limits<summary_type>::is_specialized, "augment::Max needs a projection whose type has std::numeric_limits");
        static summary_type identity() { return std::numeric_limits<summary_type>::lowest(); }
        static summary_type of(const V& v) { return Proj()(v); }
        static summary_type combine(const summary_type& a, const summary_type& b) { return a < b ? b : a; }
    };
};

template <class Proj>
struct Sum {
    template <class V>
    struct bind {
        using summary_type = projected_t<Proj, V>;
        static summary_type identity() { return summary_type{}; }
        static summary_type of(const V& v) { return Proj()(v); }
        static summary_type combine(const summary_type& a, const summary_type& b) { return a + b; }
    };
};

template <class T>
struct MinMaxSummary {
    T min;
    T max;

    bool empty() const noexcept { return max < min; }
    bool overlaps(const T& lo, const T& hi) const noexcept { return !(max < lo) && !(hi < min); }

    friend bool operator==(const MinMaxSummary& a, const MinMaxSummary& b) { return a.min == b.min && a.max == b.max; }
    friend bool operator!=(const MinMaxSummary& a, const MinMaxSummary& b) { return !(a == b); }
};

template <class Proj>
struct MinMax {
    template <class V>
    struct bind {
        using value_type = projected_t<Proj, V>;
        using summary_type = MinMaxSummary<value_type>;
        static_assert(std::numeric_limits<value_type>::is_specialized, "augment::MinMax needs a projection whose type has std::numeric_limits");
        static summary_type identity() { return { std::numeric_limits<value_type>::max(), std::numeric_limits<value_type>::lowest() }; }

        static summary_type of(const V& v) {
            const value_type x = Proj()(v);
            return { x, x };
        }

        static summary_type combine(const summary_type& a, const summary_type& b) { return { b.min < a.min ? b.min : a.min, a.max < b.max ? b.max : a.max }; }
    };
};

template <class A, class B>
struct Both {
    template <class V>
    struct bind {
        using first_policy = typename detail::bind_augment<A, V>::type;
        using second_policy = typename detail::bind_augment<B, V>::type;
        using summary_type = std::pair<typename first_policy::summary_type, typename second_policy::summary_type>;
        static summary_type identity() { return { first_policy::identity(), second_policy::identity() }; }
        static summary_type of(const V& v) { return { first_policy::of(v), second_policy::of(v) }; }
        static summary_type combine(const summary_type& a, const summary_type& b) { return { first_policy::combine(a.first, b.first), second_policy::combine(a.second, b.second) }; }
    };
};

template <auto Member> using MinOf = Min<Field<Member>>;
template <auto Member> using MaxOf = Max<Field<Member>>;
template <auto Member> using SumOf = Sum<Field<Member>>;
template <auto Member> using MinMaxOf = MinMax<Field<Member>>;

} // namespace augment

template <class T, class Compare = std::less<T>, class Augment = void, std::size_t Degree = 0>
class BTreeSet : public detail::BTreeBase<T, T, detail::BTreeSetKeyOf<T>, Compare, false, Augment, Degree, true> {
    using Base = detail::BTreeBase<T, T, detail::BTreeSetKeyOf<T>, Compare, false, Augment, Degree, true>;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using Base::insert;
    using Base::erase;

    BTreeSet() = default;
    explicit BTreeSet(const Compare& c) : Base(c) {}
    BTreeSet(std::initializer_list<T> init, const Compare& c = Compare()) : Base(c) { this->construct_from(init.begin(), init.end()); }

    template <class It, class = typename std::iterator_traits<It>::iterator_category>
    BTreeSet(It first, It last, const Compare& c = Compare()) : Base(c) { this->construct_from(first, last); }

    static BTreeSet from_sorted(std::vector<T> values, const Compare& c = Compare()) {
        BTreeSet s(c);
        s.assign_sorted(std::move(values));
        return s;
    }

    template <class... Args>
    std::pair<const_iterator, bool> emplace(Args&&... args) {
        const auto r = this->m_t.insert_unique(T(std::forward<Args>(args)...));
        return { this->cit(r.first), r.second };
    }

    std::pair<const_iterator, bool> insert(const T& v) {
        const auto r = this->m_t.insert_unique(v);
        return { this->cit(r.first), r.second };
    }

    std::pair<const_iterator, bool> insert(T&& v) {
        const auto r = this->m_t.insert_unique(std::move(v));
        return { this->cit(r.first), r.second };
    }

    void insert(std::initializer_list<T> init) { this->m_t.merge_insert(init.begin(), init.end()); }

    bool add(const T& v) { return insert(v).second; }
    bool add(T&& v) { return insert(std::move(v)).second; }

    template <class Fn>
    std::pair<const_iterator, bool> modify(const_iterator pos, Fn fn) {
        const auto r = this->m_t.modify(this->cursor(pos), fn);
        return { this->cit(r.first), r.second };
    }

    template <class Fn>
    bool modify(const T& key, Fn fn) {
        const auto c = this->m_t.find(key);
        if (!c.node) return false;
        this->m_t.modify(c, fn);
        return true;
    }

    const T& first() const { return this->checked_value(this->m_t.first()); }
    const T& last() const { return this->checked_value(this->m_t.last()); }

    const T& at_index(size_type i) const {
        if (i >= this->size()) throw std::out_of_range("BTree index out of range");
        const auto c = this->m_t.select(i);
        return c.node->value(c.pos);
    }

    const T& operator[](size_type i) const { return at_index(i); }

    BTreeSet union_with(const BTreeSet& o) const { return combine(o, 0); }
    BTreeSet intersection(const BTreeSet& o) const { return combine(o, 1); }
    BTreeSet difference(const BTreeSet& o) const { return combine(o, 2); }
    BTreeSet symmetric_difference(const BTreeSet& o) const { return combine(o, 3); }

    bool is_subset(const BTreeSet& o) const { return std::includes(o.begin(), o.end(), this->begin(), this->end(), this->key_comp()); }
    bool is_superset(const BTreeSet& o) const { return o.is_subset(*this); }

    bool is_disjoint(const BTreeSet& o) const {
        const BTreeSet& small = this->size() <= o.size() ? *this : o;
        const BTreeSet& big = this->size() <= o.size() ? o : *this;

        for (const auto& v : small) {
            if (big.contains(v)) return false;
        }

        return true;
    }

    void swap(BTreeSet& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const BTreeSet& a, const BTreeSet& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const BTreeSet& a, const BTreeSet& b) { return !(a == b); }
    friend bool operator<(const BTreeSet& a, const BTreeSet& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend bool operator>(const BTreeSet& a, const BTreeSet& b) { return b < a; }
    friend bool operator<=(const BTreeSet& a, const BTreeSet& b) { return !(b < a); }
    friend bool operator>=(const BTreeSet& a, const BTreeSet& b) { return !(a < b); }
    friend BTreeSet operator|(const BTreeSet& a, const BTreeSet& b) { return a.union_with(b); }
    friend BTreeSet operator&(const BTreeSet& a, const BTreeSet& b) { return a.intersection(b); }
    friend BTreeSet operator-(const BTreeSet& a, const BTreeSet& b) { return a.difference(b); }
    friend BTreeSet operator^(const BTreeSet& a, const BTreeSet& b) { return a.symmetric_difference(b); }

private:
    BTreeSet combine(const BTreeSet& o, int op) const {
        std::vector<T> v;
        const Compare c = this->key_comp();

        switch (op) {
        case 0: std::set_union(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), c); break;
        case 1: std::set_intersection(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), c); break;
        case 2: std::set_difference(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), c); break;
        default: std::set_symmetric_difference(this->begin(), this->end(), o.begin(), o.end(), std::back_inserter(v), c); break;
        }

        BTreeSet r(c);
        r.m_t.build_from(std::make_move_iterator(v.begin()), v.size());
        return r;
    }
};

template <class T, class Compare = std::less<T>, class Augment = void, std::size_t Degree = 0>
class BTreeMultiSet : public detail::BTreeBase<T, T, detail::BTreeSetKeyOf<T>, Compare, true, Augment, Degree, true> {
    using Base = detail::BTreeBase<T, T, detail::BTreeSetKeyOf<T>, Compare, true, Augment, Degree, true>;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using Base::insert;
    using Base::erase;

    BTreeMultiSet() = default;
    explicit BTreeMultiSet(const Compare& c) : Base(c) {}
    BTreeMultiSet(std::initializer_list<T> init, const Compare& c = Compare()) : Base(c) { this->construct_from(init.begin(), init.end()); }

    template <class It, class = typename std::iterator_traits<It>::iterator_category>
    BTreeMultiSet(It first, It last, const Compare& c = Compare()) : Base(c) { this->construct_from(first, last); }

    static BTreeMultiSet from_sorted(std::vector<T> values, const Compare& c = Compare()) {
        BTreeMultiSet s(c);
        s.assign_sorted(std::move(values));
        return s;
    }

    template <class... Args>
    const_iterator emplace(Args&&... args) { return this->cit(this->m_t.insert_multi(T(std::forward<Args>(args)...))); }

    const_iterator insert(const T& v) { return this->cit(this->m_t.insert_multi(v)); }
    const_iterator insert(T&& v) { return this->cit(this->m_t.insert_multi(std::move(v))); }
    void insert(std::initializer_list<T> init) { this->m_t.merge_insert(init.begin(), init.end()); }

    void add(const T& v) { insert(v); }
    void add(T&& v) { insert(std::move(v)); }

    template <class Fn>
    const_iterator modify(const_iterator pos, Fn fn) { return this->cit(this->m_t.modify(this->cursor(pos), fn).first); }

    template <class Fn>
    bool modify(const T& key, Fn fn) {
        const auto c = this->m_t.find(key);
        if (!c.node) return false;
        this->m_t.modify(c, fn);
        return true;
    }

    const T& first() const { return this->checked_value(this->m_t.first()); }
    const T& last() const { return this->checked_value(this->m_t.last()); }

    const T& at_index(size_type i) const {
        if (i >= this->size()) throw std::out_of_range("BTree index out of range");
        const auto c = this->m_t.select(i);
        return c.node->value(c.pos);
    }

    const T& operator[](size_type i) const { return at_index(i); }

    void swap(BTreeMultiSet& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const BTreeMultiSet& a, const BTreeMultiSet& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const BTreeMultiSet& a, const BTreeMultiSet& b) { return !(a == b); }
    friend bool operator<(const BTreeMultiSet& a, const BTreeMultiSet& b) { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
};

template <class K, class V, class Compare = std::less<K>, class Augment = void, std::size_t Degree = 0>
class BTreeMap : public detail::BTreeBase<K, std::pair<const K, V>, detail::BTreeMapKeyOf<K, V>, Compare, false, Augment, Degree, false> {
    using Base = detail::BTreeBase<K, std::pair<const K, V>, detail::BTreeMapKeyOf<K, V>, Compare, false, Augment, Degree, false>;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using typename Base::value_type;
    using mapped_type = V;
    using mapped_ref = typename std::conditional<Base::augmented, const V&, V&>::type;
    using value_ref = typename std::conditional<Base::augmented, const value_type&, value_type&>::type;
    using Base::insert;
    using Base::erase;

    BTreeMap() = default;
    explicit BTreeMap(const Compare& c) : Base(c) {}
    BTreeMap(std::initializer_list<std::pair<K, V>> init, const Compare& c = Compare()) : Base(c) { this->construct_from(init.begin(), init.end()); }

    template <class It, class = typename std::iterator_traits<It>::iterator_category>
    BTreeMap(It first, It last, const Compare& c = Compare()) : Base(c) { this->construct_from(first, last); }

    static BTreeMap from_sorted(std::vector<std::pair<K, V>> values, const Compare& c = Compare()) {
        BTreeMap m(c);
        m.assign_sorted(std::move(values));
        return m;
    }

    std::pair<iterator, bool> insert(const std::pair<K, V>& kv) {
        const auto r = this->m_t.insert_unique(kv);
        return { this->it(r.first), r.second };
    }

    std::pair<iterator, bool> insert(std::pair<K, V>&& kv) {
        const auto r = this->m_t.insert_unique(std::move(kv));
        return { this->it(r.first), r.second };
    }

    void insert(std::initializer_list<std::pair<K, V>> init) { this->m_t.merge_insert(init.begin(), init.end()); }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) {
        const auto c = this->m_t.find(key);
        if (c.node) return { this->it(c), false };
        return { this->it(this->m_t.insert_multi(std::pair<K, V>(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<Args>(args)...)))), true };
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(K&& key, Args&&... args) {
        const auto c = this->m_t.find(key);
        if (c.node) return { this->it(c), false };
        return { this->it(this->m_t.insert_multi(std::pair<K, V>(std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple(std::forward<Args>(args)...)))), true };
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& value) {
        const auto c = this->m_t.find(key);

        if (c.node) {
            c.node->value(c.pos).second = std::forward<M>(value);
            this->m_t.refresh_from(c);
            return { this->it(c), false };
        }

        return { this->it(this->m_t.insert_multi(std::pair<K, V>(key, std::forward<M>(value)))), true };
    }

    V& operator[](const K& key) {
        static_assert(!Base::augmented, "BTreeMap::operator[] hands out a mutable reference that would bypass the augmentation; use try_emplace, insert_or_assign or modify");
        return const_cast<V&>(try_emplace(key).first->second);
    }

    V& operator[](K&& key) {
        static_assert(!Base::augmented, "BTreeMap::operator[] hands out a mutable reference that would bypass the augmentation; use try_emplace, insert_or_assign or modify");
        return const_cast<V&>(try_emplace(std::move(key)).first->second);
    }

    template <class Fn>
    iterator modify(const_iterator pos, Fn fn) {
        auto wrap = [&fn](value_type& kv) { fn(kv.second); };
        return this->it(this->m_t.modify(this->cursor(pos), wrap).first);
    }

    template <class Fn>
    bool modify(const K& key, Fn fn) {
        const auto c = this->m_t.find(key);
        if (!c.node) return false;
        auto wrap = [&fn](value_type& kv) { fn(kv.second); };
        this->m_t.modify(c, wrap);
        return true;
    }

    mapped_ref at(const K& key) {
        const auto c = this->m_t.find(key);
        if (!c.node) throw std::out_of_range("BTreeMap::at: key not present");
        return c.node->value(c.pos).second;
    }

    const V& at(const K& key) const {
        const auto c = this->m_t.find(key);
        if (!c.node) throw std::out_of_range("BTreeMap::at: key not present");
        return c.node->value(c.pos).second;
    }

    V get(const K& key, V fallback = V()) const {
        const auto c = this->m_t.find(key);
        return c.node ? c.node->value(c.pos).second : fallback;
    }

    value_ref first() { return this->checked_value(this->m_t.first()); }
    const value_type& first() const { return this->checked_value(this->m_t.first()); }
    value_ref last() { return this->checked_value(this->m_t.last()); }
    const value_type& last() const { return this->checked_value(this->m_t.last()); }

    value_ref at_index(size_type i) {
        if (i >= this->size()) throw std::out_of_range("BTree index out of range");
        const auto c = this->m_t.select(i);
        return c.node->value(c.pos);
    }

    const value_type& at_index(size_type i) const {
        if (i >= this->size()) throw std::out_of_range("BTree index out of range");
        const auto c = this->m_t.select(i);
        return c.node->value(c.pos);
    }

    std::vector<K> keys() const {
        std::vector<K> out;
        out.reserve(this->size());
        for (const auto& kv : *this) out.push_back(kv.first);
        return out;
    }

    std::vector<V> values() const {
        std::vector<V> out;
        out.reserve(this->size());
        for (const auto& kv : *this) out.push_back(kv.second);
        return out;
    }

    void swap(BTreeMap& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const BTreeMap& a, const BTreeMap& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const BTreeMap& a, const BTreeMap& b) { return !(a == b); }
};

template <class K, class V, class Compare = std::less<K>, class Augment = void, std::size_t Degree = 0>
class BTreeMultiMap : public detail::BTreeBase<K, std::pair<const K, V>, detail::BTreeMapKeyOf<K, V>, Compare, true, Augment, Degree, false> {
    using Base = detail::BTreeBase<K, std::pair<const K, V>, detail::BTreeMapKeyOf<K, V>, Compare, true, Augment, Degree, false>;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using typename Base::value_type;
    using mapped_type = V;
    using Base::insert;
    using Base::erase;

    BTreeMultiMap() = default;
    explicit BTreeMultiMap(const Compare& c) : Base(c) {}
    BTreeMultiMap(std::initializer_list<std::pair<K, V>> init, const Compare& c = Compare()) : Base(c) { this->construct_from(init.begin(), init.end()); }

    template <class It, class = typename std::iterator_traits<It>::iterator_category>
    BTreeMultiMap(It first, It last, const Compare& c = Compare()) : Base(c) { this->construct_from(first, last); }

    iterator insert(const std::pair<K, V>& kv) { return this->it(this->m_t.insert_multi(kv)); }
    iterator insert(std::pair<K, V>&& kv) { return this->it(this->m_t.insert_multi(std::move(kv))); }
    void insert(std::initializer_list<std::pair<K, V>> init) { this->m_t.merge_insert(init.begin(), init.end()); }

    template <class... Args>
    iterator emplace(Args&&... args) { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    std::vector<V> get_all(const K& key) const {
        std::vector<V> out;
        for (auto r = this->equal_range(key); r.first != r.second; ++r.first) out.push_back(r.first->second);
        return out;
    }

    void swap(BTreeMultiMap& o) noexcept { this->m_t.swap(o.m_t); }

    friend bool operator==(const BTreeMultiMap& a, const BTreeMultiMap& b) { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const BTreeMultiMap& a, const BTreeMultiMap& b) { return !(a == b); }
};

template <class T, class Compare = std::less<T>, class Augment = void, std::size_t Degree = 0>
using BTree = BTreeSet<T, Compare, Augment, Degree>;

template <class T, class Augment, class Compare = std::less<T>, std::size_t Degree = 0>
using AugmentedBTree = BTreeSet<T, Compare, Augment, Degree>;

template <class T, class C, class A, std::size_t D>
void swap(BTreeSet<T, C, A, D>& a, BTreeSet<T, C, A, D>& b) noexcept { a.swap(b); }

template <class T, class C, class A, std::size_t D>
void swap(BTreeMultiSet<T, C, A, D>& a, BTreeMultiSet<T, C, A, D>& b) noexcept { a.swap(b); }

template <class K, class V, class C, class A, std::size_t D>
void swap(BTreeMap<K, V, C, A, D>& a, BTreeMap<K, V, C, A, D>& b) noexcept { a.swap(b); }

template <class K, class V, class C, class A, std::size_t D>
void swap(BTreeMultiMap<K, V, C, A, D>& a, BTreeMultiMap<K, V, C, A, D>& b) noexcept { a.swap(b); }

template <class T, class C, class A, std::size_t D>
struct Hash<BTreeSet<T, C, A, D>> {
    std::size_t operator()(const BTreeSet<T, C, A, D>& s) const {
        std::size_t h = s.size();
        for (const auto& k : s) h = hash_combine(h, Hash<T>()(k));
        return h;
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_BTREE_HPP
