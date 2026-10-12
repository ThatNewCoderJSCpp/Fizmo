#ifndef FIZMO_GRAPHS_PATH_HPP
#define FIZMO_GRAPHS_PATH_HPP

#include "graph.hpp"

namespace fizmo {
namespace graphs {

template <class W = double>
class Path {
public:
    using weight_type    = W;
    using const_iterator = typename std::vector<NodeId>::const_iterator;

    Path() : m_cost(W(0)) {}
    explicit Path(NodeId start) : m_nodes{start}, m_cost(W(0)) {}
    Path(std::vector<NodeId> nodes, W cost) : m_nodes(std::move(nodes)), m_cost(std::move(cost)) {}

    bool found() const { return !m_nodes.empty(); }
    explicit operator bool() const { return found(); }
    bool empty() const { return m_nodes.empty(); }

    std::size_t size() const { return m_nodes.size(); }
    std::size_t length() const { return m_nodes.empty() ? 0 : m_nodes.size() - 1; }

    const W& cost() const { return m_cost; }
    const std::vector<NodeId>& nodes() const { return m_nodes; }

    NodeId front() const { return m_nodes.front(); }
    NodeId back() const { return m_nodes.back(); }
    NodeId operator[](std::size_t i) const { return m_nodes[i]; }

    const_iterator begin() const { return m_nodes.begin(); }
    const_iterator end() const { return m_nodes.end(); }

    bool contains(NodeId id) const { return std::find(m_nodes.begin(), m_nodes.end(), id) != m_nodes.end(); }

    bool simple() const {
        std::vector<NodeId> s = m_nodes;
        std::sort(s.begin(), s.end());
        return std::adjacent_find(s.begin(), s.end()) == s.end();
    }

    Path& extend(NodeId next, const W& step_cost = W(1)) {
        m_nodes.push_back(next);
        if (m_nodes.size() > 1) m_cost = m_cost + step_cost;
        return *this;
    }

    Path reversed() const {
        std::vector<NodeId> r(m_nodes.rbegin(), m_nodes.rend());
        return Path(std::move(r), m_cost);
    }

    std::vector<Edge<W>> edges(const Traversable<W>& graph) const {
        std::vector<Edge<W>> out;
        for (std::size_t i = 0; i + 1 < m_nodes.size(); ++i) {
            bool found_edge = false;
            graph.for_each_edge(m_nodes[i], [&](const Edge<W>& e) {
                if (!found_edge && e.to == m_nodes[i + 1]) {
                    out.push_back(e);
                    found_edge = true;
                }
            });
            if (!found_edge) return std::vector<Edge<W>>();
        }
        return out;
    }

    bool valid_in(const Traversable<W>& graph) const { return m_nodes.size() < 2 || edges(graph).size() + 1 == m_nodes.size(); }

    W total_weight(const Traversable<W>& graph) const {
        W total = W(0);
        for (const Edge<W>& e : edges(graph)) total = total + graph.edge_cost(e);
        return total;
    }

    W probability(const Traversable<W>& graph) const {
        W p = W(1);
        const std::vector<Edge<W>> es = edges(graph);
        if (es.size() + 1 != m_nodes.size() && m_nodes.size() > 1) return W(0);
        for (const Edge<W>& e : es) p = p * e.weight;
        return p;
    }

    template <class T, class N>
    std::vector<T> values(const Graph<T, W, N>& graph) const {
        std::vector<T> out;
        out.reserve(m_nodes.size());
        for (NodeId id : m_nodes) out.push_back(graph.value(id));
        return out;
    }

    bool operator==(const Path& o) const { return m_nodes == o.m_nodes; }
    bool operator!=(const Path& o) const { return m_nodes != o.m_nodes; }

private:
    std::vector<NodeId> m_nodes;
    W                   m_cost;
};

template <class W = double>
using Chain = Path<W>;

} // namespace graphs
} // namespace fizmo

#endif // FIZMO_GRAPHS_PATH_HPP
