#ifndef FIZMO_COLLECTIONS_TRIE_HPP
#define FIZMO_COLLECTIONS_TRIE_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace collections {

struct TrieExact {
    char operator()(char c) const noexcept { return c; }
};

struct TrieIgnoreCase {
    char operator()(char c) const noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
};

template <class V, class Fold = TrieExact>
class TrieMap {
    struct Node {
        std::vector<std::pair<unsigned char, std::unique_ptr<Node>>> children;
        std::optional<V> value;
        std::size_t count = 0;

        Node* child(unsigned char c) const noexcept {
            auto it = std::lower_bound(children.begin(), children.end(), c, [](const auto& p, unsigned char k) { return p.first < k; });
            return it != children.end() && it->first == c ? it->second.get() : nullptr;
        }

        Node& child_or_add(unsigned char c) {
            auto it = std::lower_bound(children.begin(), children.end(), c, [](const auto& p, unsigned char k) { return p.first < k; });
            if (it != children.end() && it->first == c) return *it->second;
            return *children.insert(it, { c, std::make_unique<Node>() })->second;
        }

        std::unique_ptr<Node> clone() const {
            auto n = std::make_unique<Node>();
            n->value = value;
            n->count = count;
            n->children.reserve(children.size());
            for (const auto& [c, kid] : children) n->children.emplace_back(c, kid->clone());
            return n;
        }
    };

public:
    using key_type = std::string;
    using mapped_type = V;
    using size_type = std::size_t;

    TrieMap() : m_root(std::make_unique<Node>()) {}
    TrieMap(std::initializer_list<std::pair<std::string, V>> items) : TrieMap() {
        for (const auto& [k, v] : items) insert_or_assign(k, v);
    }
    TrieMap(const TrieMap& other) : m_root(other.m_root->clone()), m_fold(other.m_fold) {}
    TrieMap(TrieMap&&) noexcept = default;
    TrieMap& operator=(const TrieMap& other) {
        if (this != &other) {
            m_root = other.m_root->clone();
            m_fold = other.m_fold;
        }
        return *this;
    }
    TrieMap& operator=(TrieMap&&) noexcept = default;

    size_type size() const noexcept { return m_root->count; }
    bool empty() const noexcept { return m_root->count == 0; }
    explicit operator bool() const noexcept { return !empty(); }
    void clear() { m_root = std::make_unique<Node>(); }

    bool insert(std::string_view key, V value) {
        Node* n = walk_create(key);
        if (n->value) return false;
        n->value = std::move(value);
        bump(key, 1);
        return true;
    }

    bool insert_or_assign(std::string_view key, V value) {
        Node* n = walk_create(key);
        const bool fresh = !n->value;
        n->value = std::move(value);
        if (fresh) bump(key, 1);
        return fresh;
    }

    V& operator[](std::string_view key) {
        Node* n = walk_create(key);
        if (!n->value) {
            n->value.emplace();
            bump(key, 1);
        }
        return *n->value;
    }

    V* find(std::string_view key) noexcept {
        Node* n = walk(key);
        return n && n->value ? &*n->value : nullptr;
    }
    const V* find(std::string_view key) const noexcept {
        const Node* n = walk(key);
        return n && n->value ? &*n->value : nullptr;
    }
    bool contains(std::string_view key) const noexcept { return find(key) != nullptr; }

    V& at(std::string_view key) {
        if (V* v = find(key)) return *v;
        throw std::out_of_range("fizmo::TrieMap::at: no such key");
    }
    const V& at(std::string_view key) const {
        if (const V* v = find(key)) return *v;
        throw std::out_of_range("fizmo::TrieMap::at: no such key");
    }

    bool erase(std::string_view key) {
        std::vector<std::pair<Node*, unsigned char>> path;
        Node* n = m_root.get();
        for (char raw : key) {
            const unsigned char c = static_cast<unsigned char>(m_fold(raw));
            Node* next = n->child(c);
            if (!next) return false;
            path.emplace_back(n, c);
            n = next;
        }
        if (!n->value) return false;
        n->value.reset();
        --m_root->count;
        for (auto& [parent, c] : path) --parent->child(c)->count;
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            Node* parent = it->first;
            Node* kid = parent->child(it->second);
            if (kid->count != 0 || kid->value || !kid->children.empty()) break;
            auto pos = std::lower_bound(parent->children.begin(), parent->children.end(), it->second, [](const auto& p, unsigned char k) { return p.first < k; });
            parent->children.erase(pos);
        }
        return true;
    }

    bool has_prefix(std::string_view prefix) const noexcept {
        const Node* n = walk(prefix);
        return n && n->count > 0;
    }

    size_type count_prefix(std::string_view prefix) const noexcept {
        const Node* n = walk(prefix);
        return n ? n->count : 0;
    }

    template <class Fn>
    void for_each_prefix(std::string_view prefix, Fn&& fn) const {
        const Node* n = walk(prefix);
        if (!n) return;
        std::string key;
        for (char c : prefix) key += m_fold(c);
        visit(*n, key, fn);
    }

    template <class Fn>
    void for_each(Fn&& fn) const {
        std::string key;
        visit(*m_root, key, fn);
    }

    std::vector<std::string> keys_with_prefix(std::string_view prefix, size_type limit = static_cast<size_type>(-1)) const {
        std::vector<std::string> out;
        for_each_prefix(prefix, [&](const std::string& k, const V&) {
            out.push_back(k);
            return out.size() < limit;
        });
        return out;
    }

    std::vector<std::pair<std::string, const V*>> entries_with_prefix(std::string_view prefix, size_type limit = static_cast<size_type>(-1)) const {
        std::vector<std::pair<std::string, const V*>> out;
        for_each_prefix(prefix, [&](const std::string& k, const V& v) {
            out.emplace_back(k, &v);
            return out.size() < limit;
        });
        return out;
    }

    std::vector<std::string> keys() const { return keys_with_prefix(std::string_view()); }

    std::string complete(std::string_view prefix) const {
        const Node* n = walk(prefix);
        std::string out;
        for (char c : prefix) out += m_fold(c);
        if (!n) return out;
        while (!n->value && n->children.size() == 1) {
            out += static_cast<char>(n->children.front().first);
            n = n->children.front().second.get();
        }
        return out;
    }

    std::optional<std::pair<size_type, const V*>> longest_prefix_of(std::string_view text) const noexcept {
        const Node* n = m_root.get();
        std::optional<std::pair<size_type, const V*>> best;
        if (n->value) best = std::make_pair(size_type(0), &*n->value);
        for (size_type i = 0; i < text.size(); ++i) {
            n = n->child(static_cast<unsigned char>(m_fold(text[i])));
            if (!n) break;
            if (n->value) best = std::make_pair(i + 1, &*n->value);
        }
        return best;
    }

    std::vector<std::pair<std::string, size_type>> nearest(std::string_view key, size_type max_distance, size_type limit = 10) const {
        std::string folded;
        for (char c : key) folded += m_fold(c);
        std::vector<std::pair<std::string, size_type>> out;
        std::vector<size_type> row(folded.size() + 1);
        for (size_type i = 0; i <= folded.size(); ++i) row[i] = i;
        std::string path;
        for (const auto& [c, kid] : m_root->children) near_visit(*kid, static_cast<char>(c), folded, row, max_distance, path, out);
        if (m_root->value && folded.size() <= max_distance) out.emplace_back(std::string(), folded.size());
        std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        if (out.size() > limit) out.resize(limit);
        return out;
    }

private:
    Node* walk(std::string_view key) const noexcept {
        Node* n = m_root.get();
        for (char raw : key) {
            n = n->child(static_cast<unsigned char>(m_fold(raw)));
            if (!n) return nullptr;
        }
        return n;
    }

    Node* walk_create(std::string_view key) {
        Node* n = m_root.get();
        for (char raw : key) n = &n->child_or_add(static_cast<unsigned char>(m_fold(raw)));
        return n;
    }

    void bump(std::string_view key, long delta) {
        Node* n = m_root.get();
        n->count = static_cast<size_type>(static_cast<long>(n->count) + delta);
        for (char raw : key) {
            n = n->child(static_cast<unsigned char>(m_fold(raw)));
            n->count = static_cast<size_type>(static_cast<long>(n->count) + delta);
        }
    }

    template <class Fn>
    bool visit(const Node& n, std::string& key, Fn& fn) const {
        if (n.value) {
            if constexpr (std::is_same_v<decltype(fn(key, *n.value)), bool>) {
                if (!fn(key, *n.value)) return false;
            } else {
                fn(key, *n.value);
            }
        }
        for (const auto& [c, kid] : n.children) {
            key.push_back(static_cast<char>(c));
            const bool go_on = visit(*kid, key, fn);
            key.pop_back();
            if (!go_on) return false;
        }
        return true;
    }

    void near_visit(const Node& n, char c, const std::string& key, const std::vector<size_type>& prev, size_type max_distance, std::string& path, std::vector<std::pair<std::string, size_type>>& out) const {
        std::vector<size_type> row(key.size() + 1);
        row[0] = prev[0] + 1;
        size_type smallest = row[0];
        for (size_type i = 1; i <= key.size(); ++i) {
            const size_type sub = prev[i - 1] + (key[i - 1] == c ? 0 : 1);
            row[i] = std::min({ row[i - 1] + 1, prev[i] + 1, sub });
            smallest = std::min(smallest, row[i]);
        }
        path.push_back(c);
        if (n.value && row[key.size()] <= max_distance) out.emplace_back(path, row[key.size()]);
        if (smallest <= max_distance) {
            for (const auto& [cc, kid] : n.children) near_visit(*kid, static_cast<char>(cc), key, row, max_distance, path, out);
        }
        path.pop_back();
    }

    std::unique_ptr<Node> m_root;
    Fold m_fold{};
};

template <class Fold = TrieExact>
class TrieSet {
    struct Unit {};

public:
    using size_type = std::size_t;

    TrieSet() = default;
    TrieSet(std::initializer_list<std::string_view> words) {
        for (std::string_view w : words) insert(w);
    }

    size_type size() const noexcept { return m_map.size(); }
    bool empty() const noexcept { return m_map.empty(); }
    void clear() { m_map.clear(); }
    bool insert(std::string_view word) { return m_map.insert(word, Unit{}); }
    bool erase(std::string_view word) { return m_map.erase(word); }
    bool contains(std::string_view word) const noexcept { return m_map.contains(word); }
    bool has_prefix(std::string_view prefix) const noexcept { return m_map.has_prefix(prefix); }
    size_type count_prefix(std::string_view prefix) const noexcept { return m_map.count_prefix(prefix); }
    std::vector<std::string> with_prefix(std::string_view prefix, size_type limit = static_cast<size_type>(-1)) const { return m_map.keys_with_prefix(prefix, limit); }
    std::vector<std::string> words() const { return m_map.keys(); }
    std::string complete(std::string_view prefix) const { return m_map.complete(prefix); }
    std::optional<size_type> longest_prefix_of(std::string_view text) const noexcept {
        if (auto hit = m_map.longest_prefix_of(text)) return hit->first;
        return std::nullopt;
    }
    std::vector<std::pair<std::string, size_type>> nearest(std::string_view word, size_type max_distance, size_type limit = 10) const { return m_map.nearest(word, max_distance, limit); }

    template <class Fn>
    void for_each_prefix(std::string_view prefix, Fn&& fn) const {
        m_map.for_each_prefix(prefix, [&](const std::string& k, const Unit&) {
            if constexpr (std::is_same_v<decltype(fn(k)), bool>) return fn(k);
            else {
                fn(k);
                return true;
            }
        });
    }

private:
    TrieMap<Unit, Fold> m_map;
};

} // namespace collections

template <class V, class Fold = collections::TrieExact>
using TrieMap = collections::TrieMap<V, Fold>;
template <class Fold = collections::TrieExact>
using TrieSet = collections::TrieSet<Fold>;
using Trie = collections::TrieSet<>;

} // namespace fizmo

#endif // FIZMO_COLLECTIONS_TRIE_HPP
