#ifndef FIZMO_DATABASE_GRAPH_HPP
#define FIZMO_DATABASE_GRAPH_HPP

#include "table.hpp"
#include "../Collections/hash_map.hpp"

#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace database {

struct GraphOptions {
    std::string from = "from";
    std::string to = "to";
    std::string weight;
    bool        directed = true;
};

struct GraphVisit {
    Value       node;
    std::size_t depth = 0;
    Value       parent;
};

struct GraphPath {
    std::vector<Value> nodes;
    double             cost = 0.0;
    bool               found = false;

    explicit operator bool() const noexcept { return found; }
};

class Graph {
public:
    static constexpr std::size_t kUnlimited = std::numeric_limits<std::size_t>::max();

    Graph() = default;
    Graph(const Table& edges, const GraphOptions& options);

    std::size_t node_count() const noexcept { return m_nodes.size(); }
    std::size_t edge_count() const noexcept { return m_targets.size(); }
    std::uint64_t source_version() const noexcept { return m_version; }
    bool contains(const Value& node) const;

    std::vector<Value> neighbors(const Value& node) const;
    std::vector<Value> incoming(const Value& node) const;
    std::size_t out_degree(const Value& node) const;
    std::size_t in_degree(const Value& node) const;

    std::vector<GraphVisit> bfs(const Value& start, std::size_t max_depth = kUnlimited, std::size_t limit = 0) const;
    std::vector<GraphVisit> dfs(const Value& start, std::size_t max_depth = kUnlimited, std::size_t limit = 0) const;
    GraphPath shortest_path(const Value& from, const Value& to) const;
    bool reachable(const Value& from, const Value& to, std::size_t max_depth = kUnlimited) const;
    std::vector<std::vector<Value>> components() const;
    std::optional<std::vector<Value>> topological_order() const;
    bool has_cycle() const { return !topological_order().has_value(); }

private:
    using NodeId = std::uint32_t;
    static constexpr NodeId kNone = std::numeric_limits<NodeId>::max();

    std::vector<Value>                         m_nodes;
    collections::HashMap<std::string, NodeId>  m_ids;
    std::vector<std::uint32_t>                 m_offsets;
    std::vector<NodeId>                        m_targets;
    std::vector<double>                        m_weights;
    std::vector<std::uint32_t>                 m_roffsets;
    std::vector<NodeId>                        m_rtargets;
    bool                                       m_weighted = false;
    bool                                       m_directed = true;
    std::uint64_t                              m_version = 0;

    NodeId id_of(const Value& v) const;
    template <typename Fn> void for_out(NodeId n, Fn&& fn) const;
    template <typename Fn> void for_in(NodeId n, Fn&& fn) const;
};

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_GRAPH_HPP
