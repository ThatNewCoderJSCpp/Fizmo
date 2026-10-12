#ifndef FIZMO_GRAPHS_GRAPH_HPP
#define FIZMO_GRAPHS_GRAPH_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace graphs {

using NodeId = std::size_t;

constexpr NodeId invalid_node = std::numeric_limits<NodeId>::max();

template <class W = double>
struct Edge {
    NodeId from   = invalid_node;
    NodeId to     = invalid_node;
    W      weight = W(1);

    Edge() = default;
    Edge(NodeId f, NodeId t, W w = W(1)) : from(f), to(t), weight(std::move(w)) {}
};

template <class W = double>
class Traversable {
public:
    using weight_type = W;
    using EdgeVisitor = std::function<void(const Edge<W>&)>;

    virtual ~Traversable() = default;

    virtual std::size_t node_count() const = 0;
    virtual void for_each_edge(NodeId from, const EdgeVisitor& visit) const = 0;

    virtual bool contains(NodeId id) const { return id < node_count(); }
    virtual bool is_directed() const { return true; }
    virtual bool passable(NodeId) const { return true; }
    virtual W edge_cost(const Edge<W>& e) const { return e.weight; }
    virtual W heuristic(NodeId, NodeId) const { return W(0); }

    std::vector<Edge<W>> edges_from(NodeId from) const {
        std::vector<Edge<W>> out;
        for_each_edge(from, [&](const Edge<W>& e) { out.push_back(e); });
        return out;
    }

    std::vector<NodeId> neighbors(NodeId from) const {
        std::vector<NodeId> out;
        for_each_edge(from, [&](const Edge<W>& e) { out.push_back(e.to); });
        return out;
    }

    std::size_t out_degree(NodeId from) const {
        std::size_t n = 0;
        for_each_edge(from, [&](const Edge<W>&) { ++n; });
        return n;
    }
};

template <class T, class W = double>
class Node {
public:
    using value_type  = T;
    using weight_type = W;

    Node() = default;
    explicit Node(T value) : m_value(std::move(value)) {}
    virtual ~Node() = default;

    NodeId id() const { return m_id; }

    T&       value()       { return m_value; }
    const T& value() const { return m_value; }

    const std::vector<Edge<W>>& edges() const { return m_edges; }
    std::size_t degree() const { return m_edges.size(); }

    bool connects_to(NodeId other) const {
        for (const Edge<W>& e : m_edges) {
            if (e.to == other) return true;
        }
        return false;
    }

    const Edge<W>* edge_to(NodeId other) const {
        for (const Edge<W>& e : m_edges) {
            if (e.to == other) return &e;
        }
        return nullptr;
    }

    virtual bool passable() const { return true; }
    virtual W cost(const Edge<W>& e) const { return e.weight; }
    virtual W heuristic(const Node&) const { return W(0); }
    virtual std::string label() const { return std::to_string(m_id); }
    virtual void on_connected(const Edge<W>&) {}
    virtual void on_disconnected(NodeId) {}

private:
    template <class, class, class> friend class Graph;

    NodeId               m_id = invalid_node;
    T                    m_value{};
    std::vector<Edge<W>> m_edges;
};

template <class T, class W = double, class N = Node<T, W>>
class Graph : public Traversable<W> {
    static_assert(std::is_base_of<Node<T, W>, N>::value, "Graph nodes must derive from fizmo::graphs::Node<T, W>");

public:
    using node_type   = N;
    using value_type  = T;
    using weight_type = W;
    using EdgeVisitor = typename Traversable<W>::EdgeVisitor;

    explicit Graph(bool directed = false) : m_directed(directed) {}
    virtual ~Graph() = default;

    Graph(Graph&&) noexcept            = default;
    Graph& operator=(Graph&&) noexcept = default;

    bool directed() const { return m_directed; }

    template <class... Args>
    NodeId add_node(Args&&... args) { return add(std::unique_ptr<N>(new N(std::forward<Args>(args)...))); }

    NodeId add(std::unique_ptr<N> node) {
        const NodeId id = m_nodes.size();
        node->m_id      = id;
        m_nodes.push_back(std::move(node));
        on_node_added(id);
        return id;
    }

    virtual void add_edge(NodeId from, NodeId to, W weight = W(1)) {
        check(from);
        check(to);
        link(from, to, weight);
        if (!m_directed && from != to) link(to, from, weight);
    }

    virtual bool remove_edge(NodeId from, NodeId to) {
        check(from);
        check(to);
        const bool removed = unlink(from, to);
        if (!m_directed && from != to) unlink(to, from);
        return removed;
    }

    void set_weight(NodeId from, NodeId to, W weight) {
        check(from);
        check(to);
        bool found = false;
        for (Edge<W>& e : m_nodes[from]->m_edges) {
            if (e.to == to) {
                e.weight = weight;
                found    = true;
            }
        }
        if (!found) link(from, to, weight);
        if (!m_directed && from != to) {
            bool back = false;
            for (Edge<W>& e : m_nodes[to]->m_edges) {
                if (e.to == from) {
                    e.weight = weight;
                    back     = true;
                }
            }
            if (!back) link(to, from, weight);
        }
    }

    N&       node(NodeId id)       { check(id); return *m_nodes[id]; }
    const N& node(NodeId id) const { check(id); return *m_nodes[id]; }

    N&       operator[](NodeId id)       { return node(id); }
    const N& operator[](NodeId id) const { return node(id); }

    T&       value(NodeId id)       { return node(id).value(); }
    const T& value(NodeId id) const { return node(id).value(); }

    std::size_t size() const { return m_nodes.size(); }
    bool empty() const { return m_nodes.empty(); }

    std::size_t edge_count() const {
        std::size_t n = 0;
        for (const auto& p : m_nodes) n += p->m_edges.size();
        return m_directed ? n : n / 2;
    }

    bool has_edge(NodeId from, NodeId to) const { return contains(from) && m_nodes[from]->connects_to(to); }

    const Edge<W>* edge(NodeId from, NodeId to) const { return contains(from) ? m_nodes[from]->edge_to(to) : nullptr; }

    W weight(NodeId from, NodeId to) const {
        const Edge<W>* e = edge(from, to);
        if (e == nullptr) throw std::out_of_range("fizmo::graphs::Graph::weight: no such edge");
        return e->weight;
    }

    std::vector<NodeId> find(const T& value) const {
        std::vector<NodeId> out;
        for (const auto& p : m_nodes) {
            if (p->value() == value) out.push_back(p->id());
        }
        return out;
    }

    NodeId find_first(const T& value) const {
        for (const auto& p : m_nodes) {
            if (p->value() == value) return p->id();
        }
        return invalid_node;
    }

    std::vector<NodeId> predecessors(NodeId id) const {
        std::vector<NodeId> out;
        for (const auto& p : m_nodes) {
            if (p->connects_to(id)) out.push_back(p->id());
        }
        return out;
    }

    std::size_t in_degree(NodeId id) const { return predecessors(id).size(); }

    std::vector<Edge<W>> all_edges() const {
        std::vector<Edge<W>> out;
        for (const auto& p : m_nodes) {
            for (const Edge<W>& e : p->m_edges) {
                if (m_directed || e.from <= e.to) out.push_back(e);
            }
        }
        return out;
    }

    std::size_t node_count() const override { return m_nodes.size(); }

    void for_each_edge(NodeId from, const EdgeVisitor& visit) const override {
        if (!contains(from)) return;
        for (const Edge<W>& e : m_nodes[from]->m_edges) {
            if (m_nodes[e.to]->passable()) visit(e);
        }
    }

    bool contains(NodeId id) const override { return id < m_nodes.size(); }
    bool is_directed() const override { return m_directed; }
    bool passable(NodeId id) const override { return contains(id) && m_nodes[id]->passable(); }
    W edge_cost(const Edge<W>& e) const override { return m_nodes[e.from]->cost(e); }
    W heuristic(NodeId from, NodeId goal) const override { return m_nodes[from]->heuristic(*m_nodes[goal]); }

    void clear() {
        m_nodes.clear();
    }

protected:
    virtual void on_node_added(NodeId) {}

    void check(NodeId id) const {
        if (id >= m_nodes.size()) throw std::out_of_range("fizmo::graphs::Graph: node id out of range");
    }

private:
    void link(NodeId from, NodeId to, const W& weight) {
        m_nodes[from]->m_edges.emplace_back(from, to, weight);
        m_nodes[from]->on_connected(m_nodes[from]->m_edges.back());
    }

    bool unlink(NodeId from, NodeId to) {
        auto&      v      = m_nodes[from]->m_edges;
        const auto before = v.size();
        v.erase(std::remove_if(v.begin(), v.end(), [&](const Edge<W>& e) { return e.to == to; }), v.end());
        const bool removed = v.size() != before;
        if (removed) m_nodes[from]->on_disconnected(to);
        return removed;
    }

    std::vector<std::unique_ptr<N>> m_nodes;
    bool                            m_directed = false;
};

template <class T, class W = double, class N = Node<T, W>>
class DirectedGraph : public Graph<T, W, N> {
public:
    DirectedGraph() : Graph<T, W, N>(true) {}
};

template <class W = double>
class FunctionGraph : public Traversable<W> {
public:
    using EdgeVisitor  = typename Traversable<W>::EdgeVisitor;
    using EdgeFunction = std::function<void(NodeId, const EdgeVisitor&)>;

    FunctionGraph(std::size_t count, EdgeFunction edges, bool directed = true) : m_count(count), m_edges(std::move(edges)), m_directed(directed) {}

    static FunctionGraph from_neighbors(std::size_t count, std::function<std::vector<NodeId>(NodeId)> neighbors, bool directed = true) {
        return FunctionGraph(count, [neighbors](NodeId from, const EdgeVisitor& visit) {
            for (NodeId to : neighbors(from)) visit(Edge<W>(from, to, W(1)));
        }, directed);
    }

    std::size_t node_count() const override { return m_count; }
    void for_each_edge(NodeId from, const EdgeVisitor& visit) const override { m_edges(from, visit); }
    bool is_directed() const override { return m_directed; }

private:
    std::size_t  m_count;
    EdgeFunction m_edges;
    bool         m_directed;
};

} // namespace graphs
} // namespace fizmo

#endif // FIZMO_GRAPHS_GRAPH_HPP
