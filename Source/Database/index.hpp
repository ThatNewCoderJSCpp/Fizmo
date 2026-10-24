#ifndef FIZMO_DATABASE_INDEX_HPP
#define FIZMO_DATABASE_INDEX_HPP

#include "column_store.hpp"
#include "../Collections/btree.hpp"
#include "../Collections/hash_map.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace database {

struct KeyBound {
    std::string key;
    bool        inclusive = true;
    bool        present = false;

    static KeyBound none() { return KeyBound(); }
    static KeyBound at(std::string k, bool incl) { KeyBound b; b.key = std::move(k); b.inclusive = incl; b.present = true; return b; }
};

class Index {
public:
    Index(IndexDef def, std::vector<std::size_t> columns) : m_def(std::move(def)), m_columns(std::move(columns)) {}
    virtual ~Index() = default;

    const IndexDef& def() const noexcept { return m_def; }
    const std::string& name() const noexcept { return m_def.name; }
    const std::vector<std::size_t>& columns() const noexcept { return m_columns; }
    bool unique() const noexcept { return m_def.unique; }
    IndexKind kind() const noexcept { return m_def.kind; }
    bool ordered() const noexcept { return m_def.kind == IndexKind::Ordered; }

    virtual bool would_conflict(const std::string& key, RowId except) const = 0;
    virtual void insert(const std::string& key, RowId row) = 0;
    virtual void erase(const std::string& key, RowId row) = 0;
    virtual void lookup(const std::string& key, std::vector<RowId>& out) const = 0;
    virtual RowId first(const std::string& key) const = 0;
    virtual std::size_t count(const std::string& key) const = 0;
    virtual std::size_t entries() const noexcept = 0;
    virtual std::size_t distinct() const noexcept = 0;
    virtual void clear() = 0;
    virtual void bulk_load(std::vector<std::pair<std::string, RowId>> entries) = 0;
    virtual std::size_t memory_bytes() const noexcept = 0;

protected:
    IndexDef                 m_def;
    std::vector<std::size_t> m_columns;
};

class HashIndex final : public Index {
public:
    HashIndex(IndexDef def, std::vector<std::size_t> columns);

    bool would_conflict(const std::string& key, RowId except) const override;
    void insert(const std::string& key, RowId row) override;
    void erase(const std::string& key, RowId row) override;
    void lookup(const std::string& key, std::vector<RowId>& out) const override;
    RowId first(const std::string& key) const override;
    std::size_t count(const std::string& key) const override;
    std::size_t entries() const noexcept override { return m_entries; }
    std::size_t distinct() const noexcept override { return m_map.size(); }
    void clear() override;
    void bulk_load(std::vector<std::pair<std::string, RowId>> entries) override;
    std::size_t memory_bytes() const noexcept override;

    template <typename Fn>
    void for_each_row(const std::string& key, Fn&& fn) const {
        const Posting* p = m_map.get_ptr(key);
        if (!p) return;
        fn(p->head);
        for (RowId r : p->rest) fn(r);
    }

private:
    struct Posting {
        RowId              head = kNoRow;
        std::vector<RowId> rest;
    };

    collections::HashMap<std::string, Posting> m_map;
    std::size_t                                m_entries = 0;
};

class OrderedIndex final : public Index {
public:
    using Entry = std::pair<std::string, RowId>;
    using Tree  = collections::BTreeSet<Entry>;

    OrderedIndex(IndexDef def, std::vector<std::size_t> columns);

    bool would_conflict(const std::string& key, RowId except) const override;
    void insert(const std::string& key, RowId row) override;
    void erase(const std::string& key, RowId row) override;
    void lookup(const std::string& key, std::vector<RowId>& out) const override;
    RowId first(const std::string& key) const override;
    std::size_t count(const std::string& key) const override;
    std::size_t entries() const noexcept override { return m_tree.size(); }
    std::size_t distinct() const noexcept override { return m_distinct; }
    void clear() override;
    void bulk_load(std::vector<std::pair<std::string, RowId>> entries) override;
    std::size_t memory_bytes() const noexcept override;

    std::size_t count_range(const KeyBound& lo, const KeyBound& hi) const;
    std::size_t count_prefix(const std::string& prefix) const;

    template <typename Fn>
    void scan(const KeyBound& lo, const KeyBound& hi, bool forward, Fn&& fn) const {
        const Entry a = lower_entry(lo);
        const Entry b = upper_entry(hi);
        if (hi.present && !(a < b)) return;
        auto first = m_tree.lower_bound(a);
        auto last = hi.present ? m_tree.lower_bound(b) : m_tree.end();

        if (forward) {
            for (auto it = first; it != last; ++it) {
                if (!fn(it->second, it->first)) return;
            }
        } else {
            auto it = last;

            while (it != first) {
                --it;
                if (!fn(it->second, it->first)) return;
            }
        }
    }

    template <typename Fn>
    void scan_prefix(const std::string& prefix, bool forward, Fn&& fn) const {
        const std::string next = key_successor(prefix);
        scan(KeyBound::at(prefix, true), next.empty() ? KeyBound::none() : KeyBound::at(next, false), forward, std::forward<Fn>(fn));
    }

    const Tree& tree() const noexcept { return m_tree; }

private:
    Tree        m_tree;
    std::size_t m_distinct = 0;

    static Entry lower_entry(const KeyBound& b) { return b.present ? Entry(b.key, b.inclusive ? 0 : kNoRow) : Entry(std::string(), 0); }
    static Entry upper_entry(const KeyBound& b) { return b.present ? Entry(b.key, b.inclusive ? kNoRow : 0) : Entry(std::string(), 0); }
};

std::unique_ptr<Index> make_index(IndexDef def, std::vector<std::size_t> columns);

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_INDEX_HPP
