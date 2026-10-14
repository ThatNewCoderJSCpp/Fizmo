#ifndef FIZMO_GRAPHS_CYCLES_HPP
#define FIZMO_GRAPHS_CYCLES_HPP

#include "search.hpp"

#include <optional>

namespace fizmo {
namespace graphs {

class Visited {
public:
    Visited() = default;
    explicit Visited(std::size_t count) : m_seen(count, false) {}

    template <class W>
    explicit Visited(const Traversable<W>& g) : Visited(g.node_count()) {}

    bool visit(NodeId id) {
        if (id >= m_seen.size()) m_seen.resize(id + 1, false);
        if (m_seen[id]) return false;
        m_seen[id] = true;
        m_order.push_back(id);
        return true;
    }

    bool seen(NodeId id) const { return id < m_seen.size() && m_seen[id]; }
    bool operator[](NodeId id) const { return seen(id); }

    bool forget(NodeId id) {
        if (!seen(id)) return false;
        m_seen[id] = false;
        m_order.erase(std::find(m_order.begin(), m_order.end(), id));
        return true;
    }

    std::size_t count() const { return m_order.size(); }
    bool empty() const { return m_order.empty(); }
    const std::vector<NodeId>& order() const { return m_order; }

    void clear() {
        std::fill(m_seen.begin(), m_seen.end(), false);
        m_order.clear();
    }

    void reset(std::size_t count) {
        m_seen.assign(count, false);
        m_order.clear();
    }

private:
    std::vector<bool>   m_seen;
    std::vector<NodeId> m_order;
};

enum class EdgeKind {
    tree = 0,
    back,
    forward,
    cross
};

template <class W = double>
struct Exploration {
    std::vector<NodeId>                       order;
    std::vector<NodeId>                       finish_order;
    std::vector<NodeId>                       parent;
    std::vector<std::size_t>                  depth;
    std::vector<std::size_t>                  discovery;
    std::vector<W>                            step_cost;
    std::vector<std::pair<Edge<W>, EdgeKind>> edges;
    std::vector<Edge<W>>                      loops;
    std::vector<W>                            loop_cost;
    Visited                                   visited;

    bool reached(NodeId id) const { return visited.seen(id); }
    bool has_loop() const { return !loops.empty(); }
    std::size_t loop_count() const { return loops.size(); }

    Path<W> loop(std::size_t i = 0) const {
        if (i >= loops.size()) return Path<W>();
        const Edge<W>&      e    = loops[i];
        W                   cost = loop_cost[i];
        std::vector<NodeId> nodes;
        for (NodeId at = e.from; at != e.to; at = parent[at]) {
            nodes.push_back(at);
            cost = cost + step_cost[at];
        }
        nodes.push_back(e.to);
        std::reverse(nodes.begin(), nodes.end());
        nodes.push_back(e.to);
        return Path<W>(std::move(nodes), cost);
    }

    std::vector<Path<W>> all_loops() const {
        std::vector<Path<W>> out;
        out.reserve(loops.size());
        for (std::size_t i = 0; i < loops.size(); ++i) out.push_back(loop(i));
        return out;
    }

    Path<W> path_to(NodeId id) const {
        if (!reached(id)) return Path<W>();
        std::vector<NodeId> nodes;
        W                   cost = W(0);
        for (NodeId at = id; at != invalid_node; at = parent[at]) {
            nodes.push_back(at);
            if (parent[at] != invalid_node) cost = cost + step_cost[at];
        }
        std::reverse(nodes.begin(), nodes.end());
        return Path<W>(std::move(nodes), cost);
    }

    std::vector<Edge<W>> edges_of(EdgeKind kind) const {
        std::vector<Edge<W>> out;
        for (const auto& p : edges) {
            if (p.second == kind) out.push_back(p.first);
        }
        return out;
    }
};

namespace gdetail {

template <class W>
void prepare(const Traversable<W>& g, Exploration<W>& out) {
    const std::size_t n = g.node_count();
    out.parent.assign(n, invalid_node);
    out.depth.assign(n, unreachable);
    out.discovery.assign(n, unreachable);
    out.step_cost.assign(n, W(0));
    out.visited.reset(n);
}

template <class W>
void explore_tree(const Traversable<W>& g, NodeId root, Exploration<W>& out, std::vector<unsigned char>& state, bool first_only) {
    struct Frame {
        NodeId               id;
        std::vector<Edge<W>> edges;
        std::size_t          next;
        bool                 skipped_parent;
    };

    const bool         directed = g.is_directed();
    std::vector<Frame> stack;

    auto enter = [&](NodeId id, NodeId from, std::size_t d, const W& cost) {
        state[id]         = 1;
        out.parent[id]    = from;
        out.depth[id]     = d;
        out.discovery[id] = out.order.size();
        out.step_cost[id] = cost;
        out.visited.visit(id);
        out.order.push_back(id);
        stack.push_back(Frame{id, g.edges_from(id), 0, false});
    };

    enter(root, invalid_node, 0, W(0));

    while (!stack.empty()) {
        Frame& f = stack.back();
        if (f.next == f.edges.size()) {
            state[f.id] = 2;
            out.finish_order.push_back(f.id);
            stack.pop_back();
            continue;
        }

        const Edge<W> e  = f.edges[f.next++];
        const NodeId  at = f.id;
        if (e.to >= state.size()) continue;

        if (!directed && e.to == out.parent[at] && !f.skipped_parent) {
            f.skipped_parent = true;
            continue;
        }

        if (state[e.to] == 0) {
            out.edges.emplace_back(e, EdgeKind::tree);
            enter(e.to, at, out.depth[at] + 1, g.edge_cost(e));
        } else if (state[e.to] == 1) {
            out.edges.emplace_back(e, EdgeKind::back);
            out.loops.push_back(e);
            out.loop_cost.push_back(g.edge_cost(e));
            if (first_only) return;
        } else if (directed) {
            out.edges.emplace_back(e, out.discovery[at] < out.discovery[e.to] ? EdgeKind::forward : EdgeKind::cross);
        }
    }
}

} // namespace gdetail

template <class W>
Exploration<W> explore(const Traversable<W>& g, NodeId start) {
    Exploration<W> out;
    gdetail::prepare(g, out);
    if (!g.contains(start) || !g.passable(start)) return out;
    std::vector<unsigned char> state(g.node_count(), 0);
    gdetail::explore_tree(g, start, out, state, false);
    return out;
}

template <class W>
Exploration<W> explore(const Traversable<W>& g) {
    Exploration<W> out;
    gdetail::prepare(g, out);
    std::vector<unsigned char> state(g.node_count(), 0);
    for (NodeId s = 0; s < g.node_count(); ++s) {
        if (state[s] == 0 && g.passable(s)) gdetail::explore_tree(g, s, out, state, false);
    }
    return out;
}

template <class W>
Path<W> find_cycle(const Traversable<W>& g) {
    Exploration<W> out;
    gdetail::prepare(g, out);
    std::vector<unsigned char> state(g.node_count(), 0);
    for (NodeId s = 0; s < g.node_count() && out.loops.empty(); ++s) {
        if (state[s] == 0 && g.passable(s)) gdetail::explore_tree(g, s, out, state, true);
    }
    return out.loop();
}

template <class W>
Path<W> find_cycle_from(const Traversable<W>& g, NodeId start) {
    Exploration<W> out;
    gdetail::prepare(g, out);
    if (!g.contains(start) || !g.passable(start)) return Path<W>();
    std::vector<unsigned char> state(g.node_count(), 0);
    gdetail::explore_tree(g, start, out, state, true);
    return out.loop();
}

template <class W>
bool has_cycle_from(const Traversable<W>& g, NodeId start) { return find_cycle_from(g, start).found(); }

template <class W>
std::vector<Path<W>> find_cycles(const Traversable<W>& g) { return explore(g).all_loops(); }

namespace implicit {

template <class Key, class Hash = KeyHash>
class VisitedSet {
public:
    bool visit(const Key& key) {
        if (!m_seen.insert(key).second) return false;
        m_order.push_back(key);
        return true;
    }

    bool seen(const Key& key) const { return m_seen.count(key) != 0; }

    bool forget(const Key& key) {
        if (m_seen.erase(key) == 0) return false;
        m_order.erase(std::find(m_order.begin(), m_order.end(), key));
        return true;
    }

    std::size_t count() const { return m_order.size(); }
    bool empty() const { return m_order.empty(); }
    const std::vector<Key>& order() const { return m_order; }

    void clear() {
        m_seen.clear();
        m_order.clear();
    }

private:
    std::unordered_set<Key, Hash> m_seen;
    std::vector<Key>              m_order;
};

enum class TraceEnd {
    loop = 0,
    stopped,
    limit
};

template <class Key>
struct Trace {
    std::vector<Key> cells;
    std::size_t      loop_begin = unreachable;
    TraceEnd         end        = TraceEnd::stopped;

    bool looped() const { return end == TraceEnd::loop; }
    std::size_t loop_length() const { return looped() ? cells.size() - loop_begin : 0; }
    std::size_t tail_length() const { return looped() ? loop_begin : cells.size(); }

    std::vector<Key> loop() const {
        if (!looped()) return std::vector<Key>();
        return std::vector<Key>(cells.begin() + static_cast<std::ptrdiff_t>(loop_begin), cells.end());
    }

    std::vector<Key> tail() const {
        return std::vector<Key>(cells.begin(), cells.begin() + static_cast<std::ptrdiff_t>(tail_length()));
    }
};

namespace cdetail {

template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

} // namespace cdetail

template <class Key, class Step, class Hash = KeyHash>
Trace<Key> trace(const Key& start, Step&& step, std::size_t max_steps = unreachable) {
    using R = typename std::decay<decltype(step(start))>::type;
    Trace<Key>                                 out;
    std::unordered_map<Key, std::size_t, Hash> index;
    Key                                        at = start;

    while (true) {
        const auto found = index.find(at);
        if (found != index.end()) {
            out.loop_begin = found->second;
            out.end        = TraceEnd::loop;
            return out;
        }
        if (out.cells.size() >= max_steps) {
            out.end = TraceEnd::limit;
            return out;
        }
        index.emplace(at, out.cells.size());
        out.cells.push_back(at);
        if constexpr (cdetail::is_optional<R>::value) {
            R next = step(at);
            if (!next) {
                out.end = TraceEnd::stopped;
                return out;
            }
            at = std::move(*next);
        } else {
            at = step(at);
        }
    }
}

struct LoopInfo {
    std::size_t tail_length = 0;
    std::size_t length      = 0;

    bool found() const { return length != 0; }
    explicit operator bool() const { return found(); }
};

template <class Key, class Step>
LoopInfo find_loop(const Key& start, Step&& step, std::size_t max_steps = unreachable) {
    LoopInfo    out;
    std::size_t power = 1;
    std::size_t lam   = 1;
    std::size_t steps = 0;
    Key         tortoise = start;
    Key         hare     = step(start);

    while (!(tortoise == hare)) {
        if (++steps >= max_steps) return out;
        if (power == lam) {
            tortoise = hare;
            power *= 2;
            lam = 0;
        }
        hare = step(hare);
        ++lam;
    }

    tortoise = start;
    hare     = start;
    for (std::size_t i = 0; i < lam; ++i) hare = step(hare);
    std::size_t mu = 0;
    while (!(tortoise == hare)) {
        tortoise = step(tortoise);
        hare     = step(hare);
        ++mu;
    }

    out.tail_length = mu;
    out.length      = lam;
    return out;
}

template <class Key, class Neighbors, class Hash = KeyHash>
std::vector<Key> find_cycle(const Key& start, Neighbors&& neighbors, bool directed = true, std::size_t max_nodes = unreachable) {
    struct Frame {
        Key              key;
        std::vector<Key> next;
        std::size_t      cursor;
        bool             skipped_parent;
    };

    std::unordered_map<Key, unsigned char, Hash> state;
    std::unordered_map<Key, Key, Hash>           parent;
    std::vector<Frame>                           stack;
    std::size_t                                  expanded = 0;

    auto enter = [&](const Key& key) {
        state[key] = 1;
        ++expanded;
        const auto& n = neighbors(key);
        stack.push_back(Frame{key, std::vector<Key>(std::begin(n), std::end(n)), 0, false});
    };

    enter(start);

    while (!stack.empty()) {
        Frame& f = stack.back();
        if (f.cursor == f.next.size()) {
            state[f.key] = 2;
            stack.pop_back();
            continue;
        }

        const Key  to  = f.next[f.cursor++];
        const Key  at  = f.key;
        const auto pit = parent.find(at);

        if (!directed && pit != parent.end() && pit->second == to && !f.skipped_parent) {
            f.skipped_parent = true;
            continue;
        }

        const auto sit = state.find(to);
        if (sit == state.end()) {
            if (expanded >= max_nodes) continue;
            parent[to] = at;
            enter(to);
        } else if (sit->second == 1) {
            std::vector<Key> cycle;
            for (Key k = at; !(k == to); k = parent.find(k)->second) cycle.push_back(k);
            cycle.push_back(to);
            std::reverse(cycle.begin(), cycle.end());
            cycle.push_back(to);
            return cycle;
        }
    }

    return std::vector<Key>();
}

template <class Key, class Neighbors, class Hash = KeyHash>
bool has_cycle(const Key& start, Neighbors&& neighbors, bool directed = true, std::size_t max_nodes = unreachable) {
    return !find_cycle<Key, Neighbors, Hash>(start, std::forward<Neighbors>(neighbors), directed, max_nodes).empty();
}

} // namespace implicit

} // namespace graphs
} // namespace fizmo

#endif // FIZMO_GRAPHS_CYCLES_HPP
