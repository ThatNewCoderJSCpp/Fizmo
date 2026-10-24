#include "fizmo_library.hpp"
#include "index.hpp"

#include <algorithm>

namespace fizmo {
namespace database {

HashIndex::HashIndex(IndexDef def, std::vector<std::size_t> columns) : Index(std::move(def), std::move(columns)) {}

bool HashIndex::would_conflict(const std::string& key, RowId except) const {
    if (!m_def.unique) return false;
    const Posting* p = m_map.get_ptr(key);
    if (!p) return false;
    if (p->head != except) return true;
    return !p->rest.empty();
}

void HashIndex::insert(const std::string& key, RowId row) {
    auto r = m_map.try_emplace(key);
    Posting& p = r.first->second;

    if (r.second || p.head == kNoRow) p.head = row;
    else p.rest.push_back(row);

    ++m_entries;
}

void HashIndex::erase(const std::string& key, RowId row) {
    auto it = m_map.find(key);
    if (it == m_map.end()) return;
    Posting& p = it->second;

    if (p.head == row) {
        if (p.rest.empty()) {
            m_map.erase(key);
        } else {
            p.head = p.rest.back();
            p.rest.pop_back();
        }

        --m_entries;
        return;
    }

    auto f = std::find(p.rest.begin(), p.rest.end(), row);

    if (f != p.rest.end()) {
        *f = p.rest.back();
        p.rest.pop_back();
        --m_entries;
    }
}

void HashIndex::lookup(const std::string& key, std::vector<RowId>& out) const {
    const Posting* p = m_map.get_ptr(key);
    if (!p) return;
    out.push_back(p->head);
    out.insert(out.end(), p->rest.begin(), p->rest.end());
}

RowId HashIndex::first(const std::string& key) const {
    const Posting* p = m_map.get_ptr(key);
    return p ? p->head : kNoRow;
}

std::size_t HashIndex::count(const std::string& key) const {
    const Posting* p = m_map.get_ptr(key);
    return p ? 1 + p->rest.size() : 0;
}

void HashIndex::clear() {
    m_map.clear();
    m_entries = 0;
}

void HashIndex::bulk_load(std::vector<std::pair<std::string, RowId>> entries) {
    clear();
    m_map.reserve(entries.size());
    for (auto& e : entries) insert(e.first, e.second);
}

std::size_t HashIndex::memory_bytes() const noexcept {
    std::size_t n = m_map.size() * (sizeof(std::pair<std::string, Posting>) + 8) * 5 / 4;
    for (const auto& kv : m_map) n += kv.second.rest.capacity() * sizeof(RowId) + (kv.first.capacity() > 15 ? kv.first.capacity() : 0);
    return n;
}

OrderedIndex::OrderedIndex(IndexDef def, std::vector<std::size_t> columns) : Index(std::move(def), std::move(columns)) {}

bool OrderedIndex::would_conflict(const std::string& key, RowId except) const {
    if (!m_def.unique) return false;
    auto it = m_tree.lower_bound(Entry(key, 0));

    for (; it != m_tree.end() && it->first == key; ++it) {
        if (it->second != except) return true;
    }

    return false;
}

void OrderedIndex::insert(const std::string& key, RowId row) {
    if (count(key) == 0) ++m_distinct;
    m_tree.insert(Entry(key, row));
}

void OrderedIndex::erase(const std::string& key, RowId row) {
    if (m_tree.erase(Entry(key, row)) && count(key) == 0 && m_distinct) --m_distinct;
}

void OrderedIndex::lookup(const std::string& key, std::vector<RowId>& out) const {
    for (auto it = m_tree.lower_bound(Entry(key, 0)); it != m_tree.end() && it->first == key; ++it) out.push_back(it->second);
}

RowId OrderedIndex::first(const std::string& key) const {
    auto it = m_tree.lower_bound(Entry(key, 0));
    return it != m_tree.end() && it->first == key ? it->second : kNoRow;
}

std::size_t OrderedIndex::count(const std::string& key) const {
    return m_tree.count_range(Entry(key, 0), Entry(key, kNoRow));
}

std::size_t OrderedIndex::count_range(const KeyBound& lo, const KeyBound& hi) const {
    const Entry a = lower_entry(lo);
    if (!hi.present) return m_tree.size() - m_tree.bisect_left(a);
    const Entry b = upper_entry(hi);
    if (!(a < b)) return 0;
    return m_tree.count_range(a, b);
}

std::size_t OrderedIndex::count_prefix(const std::string& prefix) const {
    const std::string next = key_successor(prefix);
    return count_range(KeyBound::at(prefix, true), next.empty() ? KeyBound::none() : KeyBound::at(next, false));
}

void OrderedIndex::clear() {
    m_tree.clear();
    m_distinct = 0;
}

void OrderedIndex::bulk_load(std::vector<std::pair<std::string, RowId>> entries) {
    std::sort(entries.begin(), entries.end());
    entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
    m_distinct = 0;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i == 0 || entries[i].first != entries[i - 1].first) ++m_distinct;
    }

    m_tree = Tree::from_sorted(std::move(entries));
}

std::size_t OrderedIndex::memory_bytes() const noexcept {
    std::size_t n = m_tree.size() * (sizeof(Entry) + 4);
    for (const auto& e : m_tree) if (e.first.capacity() > 15) n += e.first.capacity();
    return n;
}

std::unique_ptr<Index> make_index(IndexDef def, std::vector<std::size_t> columns) {
    if (def.kind == IndexKind::Hash) return std::make_unique<HashIndex>(std::move(def), std::move(columns));
    return std::make_unique<OrderedIndex>(std::move(def), std::move(columns));
}

} // namespace database
} // namespace fizmo
