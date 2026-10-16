#ifndef FIZMO_GRAPHS_SEARCH_HPP
#define FIZMO_GRAPHS_SEARCH_HPP

#include "path.hpp"

#include <deque>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <initializer_list>

namespace fizmo {
namespace graphs {

constexpr std::size_t unreachable = std::numeric_limits<std::size_t>::max();

namespace gdetail {

template <class Visit>
bool call_visit(Visit& visit, NodeId id, std::size_t depth) {
    using R = decltype(visit(id, depth));
    if constexpr (std::is_same<R, bool>::value) {
        return visit(id, depth);
    } else {
        visit(id, depth);
        return true;
    }
}

template <class W>
Path<W> rebuild(const Traversable<W>& g, const std::vector<NodeId>& parent, NodeId start, NodeId goal) {
    std::vector<NodeId> nodes;
    for (NodeId at = goal; at != invalid_node; at = parent[at]) {
        nodes.push_back(at);
        if (at == start) break;
    }
    if (nodes.empty() || nodes.back() != start) return Path<W>();
    std::reverse(nodes.begin(), nodes.end());
    Path<W> p(start);
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        W    step = W(1);
        bool got  = false;
        g.for_each_edge(nodes[i - 1], [&](const Edge<W>& e) {
            if (!got && e.to == nodes[i]) {
                step = g.edge_cost(e);
                got  = true;
            }
        });
        p.extend(nodes[i], step);
    }
    return p;
}

template <class W>
struct QueueItem {
    W      cost;
    NodeId id;
};

template <class W>
struct QueueOrder {
    bool operator()(const QueueItem<W>& a, const QueueItem<W>& b) const { return b.cost < a.cost; }
};

} // namespace gdetail

template <class W, class Visit>
std::vector<NodeId> breadth_first_search(const Traversable<W>& g, NodeId start, Visit&& visit) {
    std::vector<NodeId> order;
    if (!g.contains(start) || !g.passable(start)) return order;
    std::vector<std::size_t> depth(g.node_count(), unreachable);
    std::deque<NodeId>       queue;
    depth[start] = 0;
    queue.push_back(start);

    while (!queue.empty()) {
        const NodeId at = queue.front();
        queue.pop_front();
        order.push_back(at);
        if (!gdetail::call_visit(visit, at, depth[at])) break;
        g.for_each_edge(at, [&](const Edge<W>& e) {
            if (e.to < depth.size() && depth[e.to] == unreachable) {
                depth[e.to] = depth[at] + 1;
                queue.push_back(e.to);
            }
        });
    }

    return order;
}

template <class W>
std::vector<NodeId> breadth_first_search(const Traversable<W>& g, NodeId start) {
    return breadth_first_search(g, start, [](NodeId, std::size_t) {});
}

template <class W, class Visit>
std::vector<NodeId> depth_first_search(const Traversable<W>& g, NodeId start, Visit&& visit) {
    std::vector<NodeId> order;
    if (!g.contains(start) || !g.passable(start)) return order;
    std::vector<bool>                               seen(g.node_count(), false);
    std::vector<std::pair<NodeId, std::size_t>>     stack{{start, 0}};

    while (!stack.empty()) {
        const auto top = stack.back();
        stack.pop_back();
        if (seen[top.first]) continue;
        seen[top.first] = true;
        order.push_back(top.first);
        if (!gdetail::call_visit(visit, top.first, top.second)) break;
        const std::vector<NodeId> next = g.neighbors(top.first);
        for (auto it = next.rbegin(); it != next.rend(); ++it) {
            if (*it < seen.size() && !seen[*it]) stack.emplace_back(*it, top.second + 1);
        }
    }

    return order;
}

template <class W>
std::vector<NodeId> depth_first_search(const Traversable<W>& g, NodeId start) {
    return depth_first_search(g, start, [](NodeId, std::size_t) {});
}

struct Traversal {
    std::vector<NodeId>      order;
    std::vector<std::size_t> depth;
    std::vector<NodeId>      parent;
    bool                     has_cycle = false;
    std::vector<NodeId>      cycle;

    bool reached(NodeId id) const { return id < depth.size() && depth[id] != unreachable; }
    std::size_t depth_of(NodeId id) const { return id < depth.size() ? depth[id] : unreachable; }

    std::vector<NodeId> path_to(NodeId id) const {
        std::vector<NodeId> out;
        if (!reached(id)) return out;
        for (NodeId at = id; at != invalid_node; at = parent[at]) out.push_back(at);
        std::reverse(out.begin(), out.end());
        return out;
    }
};

template <class W>
Traversal breadth_first_traversal(const Traversable<W>& g, NodeId start) {
    Traversal out;
    const std::size_t n = g.node_count();
    out.depth.assign(n, unreachable);
    out.parent.assign(n, invalid_node);
    if (!g.contains(start) || !g.passable(start)) return out;
    std::deque<NodeId> queue{start};
    out.depth[start] = 0;

    while (!queue.empty()) {
        const NodeId at = queue.front();
        queue.pop_front();
        out.order.push_back(at);
        g.for_each_edge(at, [&](const Edge<W>& e) {
            if (e.to < n && out.depth[e.to] == unreachable) {
                out.depth[e.to]  = out.depth[at] + 1;
                out.parent[e.to] = at;
                queue.push_back(e.to);
            }
        });
    }

    return out;
}

template <class W>
Traversal depth_first_traversal(const Traversable<W>& g, NodeId start) {
    Traversal out;
    const std::size_t n = g.node_count();
    out.depth.assign(n, unreachable);
    out.parent.assign(n, invalid_node);
    if (!g.contains(start) || !g.passable(start)) return out;
    const bool directed = g.is_directed();
    std::vector<unsigned char> state(n, 0);
    std::vector<std::pair<NodeId, std::vector<Edge<W>>>> stack;
    std::vector<std::size_t>   cursor;
    std::vector<bool>          skipped_parent;

    auto enter = [&](NodeId id, NodeId from, std::size_t d) {
        state[id]      = 1;
        out.depth[id]  = d;
        out.parent[id] = from;
        out.order.push_back(id);
        stack.emplace_back(id, g.edges_from(id));
        cursor.push_back(0);
        skipped_parent.push_back(false);
    };

    auto record_cycle = [&](NodeId from, NodeId to) {
        if (out.has_cycle) return;
        out.has_cycle = true;
        std::vector<NodeId> loop;
        for (NodeId at = from; at != invalid_node; at = out.parent[at]) {
            loop.push_back(at);
            if (at == to) break;
        }
        std::reverse(loop.begin(), loop.end());
        loop.push_back(to);
        out.cycle = loop;
    };

    enter(start, invalid_node, 0);
    while (!stack.empty()) {
        const NodeId at   = stack.back().first;
        std::size_t& i    = cursor.back();
        const auto&  next = stack.back().second;
        if (i < next.size()) {
            const Edge<W> e = next[i++];
            if (e.to >= n || !g.passable(e.to)) continue;
            if (!directed && e.to == out.parent[at] && !skipped_parent.back()) {
                skipped_parent.back() = true;
                continue;
            }
            if (state[e.to] == 0) {
                enter(e.to, at, out.depth[at] + 1);
            } else if (state[e.to] == 1) {
                record_cycle(at, e.to);
            }
        } else {
            state[at] = 2;
            stack.pop_back();
            cursor.pop_back();
            skipped_parent.pop_back();
        }
    }

    return out;
}

template <class W>
std::vector<NodeId> find_cycle(const Traversable<W>& g) {
    const std::size_t n = g.node_count();
    std::vector<bool> covered(n, false);
    for (NodeId s = 0; s < n; ++s) {
        if (covered[s] || !g.passable(s)) continue;
        const Traversal t = depth_first_traversal(g, s);
        if (t.has_cycle) return t.cycle;
        for (NodeId id : t.order) covered[id] = true;
    }
    return std::vector<NodeId>();
}

template <class W>
std::vector<NodeId> depth_first_postorder(const Traversable<W>& g, NodeId start) {
    std::vector<NodeId> order;
    if (!g.contains(start)) return order;
    std::vector<bool>                                         seen(g.node_count(), false);
    std::vector<std::pair<NodeId, std::vector<NodeId>>>       stack;
    seen[start] = true;
    stack.emplace_back(start, g.neighbors(start));
    std::vector<std::size_t> cursor{0};

    while (!stack.empty()) {
        auto&        frame = stack.back();
        std::size_t& i     = cursor.back();
        if (i < frame.second.size()) {
            const NodeId next = frame.second[i++];
            if (next < seen.size() && !seen[next]) {
                seen[next] = true;
                stack.emplace_back(next, g.neighbors(next));
                cursor.push_back(0);
            }
        } else {
            order.push_back(frame.first);
            stack.pop_back();
            cursor.pop_back();
        }
    }

    return order;
}

template <class W>
std::vector<std::size_t> hop_distances(const Traversable<W>& g, NodeId start) {
    std::vector<std::size_t> depth(g.node_count(), unreachable);
    breadth_first_search(g, start, [&](NodeId id, std::size_t d) { depth[id] = d; });
    return depth;
}

template <class W>
bool reachable(const Traversable<W>& g, NodeId from, NodeId to) {
    bool hit = false;
    breadth_first_search(g, from, [&](NodeId id, std::size_t) {
        if (id == to) hit = true;
        return !hit;
    });
    return hit;
}

template <class W, class Goal, typename std::enable_if<!std::is_convertible<Goal, NodeId>::value, int>::type = 0>
Path<W> breadth_first_path(const Traversable<W>& g, NodeId start, Goal&& is_goal) {
    if (!g.contains(start) || !g.passable(start)) return Path<W>();
    std::vector<NodeId> parent(g.node_count(), invalid_node);
    std::vector<bool>   seen(g.node_count(), false);
    std::deque<NodeId>  queue{start};
    seen[start] = true;

    while (!queue.empty()) {
        const NodeId at = queue.front();
        queue.pop_front();
        if (is_goal(at)) return gdetail::rebuild(g, parent, start, at);
        g.for_each_edge(at, [&](const Edge<W>& e) {
            if (e.to < seen.size() && !seen[e.to]) {
                seen[e.to]   = true;
                parent[e.to] = at;
                queue.push_back(e.to);
            }
        });
    }

    return Path<W>();
}

template <class W>
Path<W> breadth_first_path(const Traversable<W>& g, NodeId start, NodeId goal) {
    return breadth_first_path(g, start, [goal](NodeId id) { return id == goal; });
}

template <class W, class Goal, typename std::enable_if<!std::is_convertible<Goal, NodeId>::value, int>::type = 0>
Path<W> depth_first_path(const Traversable<W>& g, NodeId start, Goal&& is_goal) {
    if (!g.contains(start) || !g.passable(start)) return Path<W>();
    std::vector<NodeId> parent(g.node_count(), invalid_node);
    std::vector<bool>   seen(g.node_count(), false);
    std::vector<NodeId> stack{start};

    while (!stack.empty()) {
        const NodeId at = stack.back();
        stack.pop_back();
        if (seen[at]) continue;
        seen[at] = true;
        if (is_goal(at)) return gdetail::rebuild(g, parent, start, at);
        const std::vector<NodeId> next = g.neighbors(at);
        for (auto it = next.rbegin(); it != next.rend(); ++it) {
            if (*it < seen.size() && !seen[*it]) {
                parent[*it] = at;
                stack.push_back(*it);
            }
        }
    }

    return Path<W>();
}

template <class W>
Path<W> depth_first_path(const Traversable<W>& g, NodeId start, NodeId goal) {
    return depth_first_path(g, start, [goal](NodeId id) { return id == goal; });
}

template <class W, class Heuristic>
Path<W> a_star(const Traversable<W>& g, NodeId start, NodeId goal, Heuristic&& h) {
    if (!g.contains(start) || !g.contains(goal) || !g.passable(start)) return Path<W>();
    const std::size_t   n = g.node_count();
    std::vector<W>      best(n, W(0));
    std::vector<bool>   known(n, false);
    std::vector<bool>   done(n, false);
    std::vector<NodeId> parent(n, invalid_node);
    std::priority_queue<gdetail::QueueItem<W>, std::vector<gdetail::QueueItem<W>>, gdetail::QueueOrder<W>> open;
    known[start] = true;
    open.push({h(start), start});

    while (!open.empty()) {
        const NodeId at = open.top().id;
        open.pop();
        if (done[at]) continue;
        done[at] = true;
        if (at == goal) return gdetail::rebuild(g, parent, start, goal);
        g.for_each_edge(at, [&](const Edge<W>& e) {
            if (e.to >= n || done[e.to]) return;
            const W cand = best[at] + g.edge_cost(e);
            if (!known[e.to] || cand < best[e.to]) {
                known[e.to]  = true;
                best[e.to]   = cand;
                parent[e.to] = at;
                open.push({cand + h(e.to), e.to});
            }
        });
    }

    return Path<W>();
}

template <class W>
Path<W> a_star(const Traversable<W>& g, NodeId start, NodeId goal) {
    return a_star(g, start, goal, [&g, goal](NodeId id) { return g.heuristic(id, goal); });
}

template <class W>
Path<W> shortest_path(const Traversable<W>& g, NodeId start, NodeId goal) {
    return a_star(g, start, goal, [](NodeId) { return W(0); });
}

template <class W>
struct Distances {
    std::vector<W>      distance;
    std::vector<bool>   reached;
    std::vector<NodeId> parent;

    bool reachable(NodeId id) const { return id < reached.size() && reached[id]; }
};

template <class W>
Distances<W> shortest_distances(const Traversable<W>& g, NodeId start) {
    const std::size_t n = g.node_count();
    Distances<W>      out{std::vector<W>(n, W(0)), std::vector<bool>(n, false), std::vector<NodeId>(n, invalid_node)};
    if (!g.contains(start)) return out;
    std::vector<bool> done(n, false);
    std::priority_queue<gdetail::QueueItem<W>, std::vector<gdetail::QueueItem<W>>, gdetail::QueueOrder<W>> open;
    out.reached[start] = true;
    open.push({W(0), start});

    while (!open.empty()) {
        const NodeId at = open.top().id;
        open.pop();
        if (done[at]) continue;
        done[at] = true;
        g.for_each_edge(at, [&](const Edge<W>& e) {
            if (e.to >= n || done[e.to]) return;
            const W cand = out.distance[at] + g.edge_cost(e);
            if (!out.reached[e.to] || cand < out.distance[e.to]) {
                out.reached[e.to]  = true;
                out.distance[e.to] = cand;
                out.parent[e.to]   = at;
                open.push({cand, e.to});
            }
        });
    }

    return out;
}

template <class W>
std::vector<Path<W>> all_simple_paths(const Traversable<W>& g, NodeId start, NodeId goal, std::size_t max_length = unreachable) {
    std::vector<Path<W>> out;
    if (!g.contains(start) || !g.contains(goal)) return out;
    std::vector<bool>                            on_path(g.node_count(), false);
    std::vector<NodeId>                          nodes{start};
    std::vector<W>                               costs{W(0)};
    std::vector<std::vector<Edge<W>>>            pending{g.edges_from(start)};
    std::vector<std::size_t>                     cursor{0};
    on_path[start] = true;

    if (start == goal) {
        out.emplace_back(nodes, W(0));
        return out;
    }

    while (!pending.empty()) {
        std::size_t& i = cursor.back();
        if (i < pending.back().size() && nodes.size() - 1 < max_length) {
            const Edge<W> e = pending.back()[i++];
            if (e.to >= on_path.size() || on_path[e.to]) continue;
            const W c = costs.back() + g.edge_cost(e);
            if (e.to == goal) {
                std::vector<NodeId> found = nodes;
                found.push_back(goal);
                out.emplace_back(std::move(found), c);
                continue;
            }
            on_path[e.to] = true;
            nodes.push_back(e.to);
            costs.push_back(c);
            pending.push_back(g.edges_from(e.to));
            cursor.push_back(0);
        } else {
            on_path[nodes.back()] = false;
            nodes.pop_back();
            costs.pop_back();
            pending.pop_back();
            cursor.pop_back();
        }
    }

    return out;
}

template <class W>
std::vector<std::vector<NodeId>> connected_components(const Traversable<W>& g) {
    const std::size_t                n = g.node_count();
    std::vector<std::vector<NodeId>> adj(n);
    for (NodeId a = 0; a < n; ++a) {
        g.for_each_edge(a, [&](const Edge<W>& e) {
            if (e.to < n) {
                adj[a].push_back(e.to);
                adj[e.to].push_back(a);
            }
        });
    }
    std::vector<bool>                seen(n, false);
    std::vector<std::vector<NodeId>> out;
    for (NodeId s = 0; s < n; ++s) {
        if (seen[s] || !g.passable(s)) continue;
        std::vector<NodeId> comp;
        std::vector<NodeId> stack{s};
        seen[s] = true;
        while (!stack.empty()) {
            const NodeId at = stack.back();
            stack.pop_back();
            comp.push_back(at);
            for (NodeId nb : adj[at]) {
                if (!seen[nb]) {
                    seen[nb] = true;
                    stack.push_back(nb);
                }
            }
        }
        std::sort(comp.begin(), comp.end());
        out.push_back(std::move(comp));
    }
    return out;
}

template <class W>
bool is_connected(const Traversable<W>& g) { return connected_components(g).size() <= 1; }

template <class W>
std::vector<std::vector<NodeId>> strongly_connected_components(const Traversable<W>& g) {
    const std::size_t                n = g.node_count();
    std::vector<std::size_t>         index(n, unreachable), low(n, 0);
    std::vector<bool>                on_stack(n, false);
    std::vector<NodeId>              stack;
    std::vector<std::vector<NodeId>> out;
    std::size_t                      counter = 0;

    for (NodeId root = 0; root < n; ++root) {
        if (index[root] != unreachable) continue;
        std::vector<std::pair<NodeId, std::vector<NodeId>>> call{{root, g.neighbors(root)}};
        std::vector<std::size_t>                            pos{0};
        index[root] = low[root] = counter++;
        stack.push_back(root);
        on_stack[root] = true;

        while (!call.empty()) {
            const NodeId v = call.back().first;
            std::size_t& i = pos.back();
            if (i < call.back().second.size()) {
                const NodeId w = call.back().second[i++];
                if (w >= n) continue;
                if (index[w] == unreachable) {
                    index[w] = low[w] = counter++;
                    stack.push_back(w);
                    on_stack[w] = true;
                    call.emplace_back(w, g.neighbors(w));
                    pos.push_back(0);
                } else if (on_stack[w]) {
                    low[v] = std::min(low[v], index[w]);
                }
            } else {
                if (low[v] == index[v]) {
                    std::vector<NodeId> comp;
                    NodeId              w;
                    do {
                        w = stack.back();
                        stack.pop_back();
                        on_stack[w] = false;
                        comp.push_back(w);
                    } while (w != v);
                    std::sort(comp.begin(), comp.end());
                    out.push_back(std::move(comp));
                }
                call.pop_back();
                pos.pop_back();
                if (!call.empty()) {
                    const NodeId parent = call.back().first;
                    low[parent] = std::min(low[parent], low[v]);
                }
            }
        }
    }

    return out;
}

template <class W>
std::vector<NodeId> topological_sort(const Traversable<W>& g) {
    const std::size_t        n = g.node_count();
    std::vector<std::size_t> indeg(n, 0);
    for (NodeId a = 0; a < n; ++a) {
        g.for_each_edge(a, [&](const Edge<W>& e) {
            if (e.to < n) ++indeg[e.to];
        });
    }
    std::deque<NodeId> ready;
    for (NodeId a = 0; a < n; ++a) {
        if (indeg[a] == 0) ready.push_back(a);
    }
    std::vector<NodeId> order;
    while (!ready.empty()) {
        const NodeId at = ready.front();
        ready.pop_front();
        order.push_back(at);
        g.for_each_edge(at, [&](const Edge<W>& e) {
            if (e.to < n && --indeg[e.to] == 0) ready.push_back(e.to);
        });
    }
    if (order.size() != n) order.clear();
    return order;
}

template <class W>
bool has_cycle(const Traversable<W>& g) {
    const std::size_t n = g.node_count();
    if (g.is_directed()) return topological_sort(g).size() != n && n > 0;
    std::vector<NodeId> parent(n, invalid_node);
    std::vector<bool>   seen(n, false);

    for (NodeId s = 0; s < n; ++s) {
        if (seen[s]) continue;
        std::vector<NodeId> stack{s};
        seen[s] = true;
        while (!stack.empty()) {
            const NodeId at = stack.back();
            stack.pop_back();
            bool         skipped_parent = false;
            bool         cycle          = false;
            g.for_each_edge(at, [&](const Edge<W>& e) {
                if (cycle || e.to >= n) return;
                if (e.to == parent[at] && !skipped_parent) {
                    skipped_parent = true;
                    return;
                }
                if (e.to == at || seen[e.to]) {
                    cycle = true;
                    return;
                }
                seen[e.to]   = true;
                parent[e.to] = at;
                stack.push_back(e.to);
            });
            if (cycle) return true;
        }
    }

    return false;
}

namespace implicit {

namespace hdetail {

inline std::size_t mix(std::size_t seed, std::size_t h) { return seed ^ (h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2)); }

template <class T, class = void>
struct std_hashable : std::false_type {};

template <class T>
struct std_hashable<T, decltype(void(std::hash<T>()(std::declval<const T&>())))> : std::true_type {};

template <class T>
std::size_t hash_value(const T& v);

template <class A, class B>
std::size_t hash_pair(const std::pair<A, B>& p) { return mix(hash_value(p.first), hash_value(p.second)); }

template <class T, std::size_t... I>
std::size_t hash_tuple(const T& t, std::index_sequence<I...>) {
    std::size_t seed = 0;
    (void)std::initializer_list<int>{(seed = mix(seed, hash_value(std::get<I>(t))), 0)...};
    return seed;
}

template <class T>
struct hasher {
    static std::size_t get(const T& v) {
        if constexpr (std_hashable<T>::value) {
            return std::hash<T>()(v);
        } else {
            std::size_t seed = 0;
            for (const auto& x : v) seed = mix(seed, hash_value(x));
            return seed;
        }
    }
};

template <class A, class B>
struct hasher<std::pair<A, B>> {
    static std::size_t get(const std::pair<A, B>& p) { return hash_pair(p); }
};

template <class... T>
struct hasher<std::tuple<T...>> {
    static std::size_t get(const std::tuple<T...>& t) { return hash_tuple(t, std::index_sequence_for<T...>()); }
};

template <class T>
std::size_t hash_value(const T& v) { return hasher<T>::get(v); }

} // namespace hdetail

struct KeyHash {
    template <class T>
    std::size_t operator()(const T& v) const { return hdetail::hash_value(v); }
};

template <class Key, class Neighbors, class Goal, class Hash = KeyHash>
std::vector<Key> breadth_first_path(const Key& start, Neighbors&& neighbors, Goal&& is_goal, std::size_t max_nodes = unreachable) {
    std::unordered_map<Key, Key, Hash> parent;
    std::unordered_set<Key, Hash>      seen{start};
    std::deque<Key>                    queue{start};
    std::size_t                        expanded = 0;

    while (!queue.empty() && expanded < max_nodes) {
        const Key at = queue.front();
        queue.pop_front();
        ++expanded;
        if (is_goal(at)) {
            std::vector<Key> out{at};
            for (auto it = parent.find(at); it != parent.end(); it = parent.find(it->second)) out.push_back(it->second);
            std::reverse(out.begin(), out.end());
            return out;
        }
        for (const Key& next : neighbors(at)) {
            if (seen.insert(next).second) {
                parent.emplace(next, at);
                queue.push_back(next);
            }
        }
    }

    return std::vector<Key>();
}

template <class Key, class Neighbors, class Visit, class Hash = KeyHash>
std::size_t breadth_first_search(const Key& start, Neighbors&& neighbors, Visit&& visit, std::size_t max_nodes = unreachable) {
    std::unordered_map<Key, std::size_t, Hash> depth{{start, 0}};
    std::deque<Key>                             queue{start};
    std::size_t                                 visited = 0;

    while (!queue.empty() && visited < max_nodes) {
        const Key at = queue.front();
        queue.pop_front();
        ++visited;
        const std::size_t d = depth[at];
        using R = decltype(visit(at, d));
        if constexpr (std::is_same<R, bool>::value) {
            if (!visit(at, d)) break;
        } else {
            visit(at, d);
        }
        for (const Key& next : neighbors(at)) {
            if (depth.emplace(next, d + 1).second) queue.push_back(next);
        }
    }

    return visited;
}

template <class Key, class Neighbors, class Goal, class Hash = KeyHash>
std::vector<Key> depth_first_path(const Key& start, Neighbors&& neighbors, Goal&& is_goal, std::size_t max_nodes = unreachable) {
    std::unordered_map<Key, Key, Hash> parent;
    std::unordered_set<Key, Hash>      seen;
    std::vector<Key>                   stack{start};
    std::size_t                        expanded = 0;

    while (!stack.empty() && expanded < max_nodes) {
        const Key at = stack.back();
        stack.pop_back();
        if (!seen.insert(at).second) continue;
        ++expanded;
        if (is_goal(at)) {
            std::vector<Key> out{at};
            for (auto it = parent.find(at); it != parent.end(); it = parent.find(it->second)) out.push_back(it->second);
            std::reverse(out.begin(), out.end());
            return out;
        }
        const auto next = neighbors(at);
        for (auto it = std::rbegin(next); it != std::rend(next); ++it) {
            if (!seen.count(*it)) {
                parent[*it] = at;
                stack.push_back(*it);
            }
        }
    }

    return std::vector<Key>();
}

template <class Key, class Neighbors, class Cost, class Goal, class Heuristic, class Hash = KeyHash>
std::vector<Key> a_star(const Key& start, Neighbors&& neighbors, Cost&& cost, Goal&& is_goal, Heuristic&& h, std::size_t max_nodes = unreachable) {
    using W = typename std::decay<decltype(cost(start, start))>::type;
    struct Item {
        W   f;
        Key key;
    };
    struct Order {
        bool operator()(const Item& a, const Item& b) const { return b.f < a.f; }
    };
    std::unordered_map<Key, W, Hash>   best{{start, W(0)}};
    std::unordered_map<Key, Key, Hash> parent;
    std::unordered_set<Key, Hash>      closed;
    std::priority_queue<Item, std::vector<Item>, Order> open;
    open.push({h(start), start});
    std::size_t expanded = 0;

    while (!open.empty() && expanded < max_nodes) {
        const Key at = open.top().key;
        open.pop();
        if (!closed.insert(at).second) continue;
        ++expanded;
        if (is_goal(at)) {
            std::vector<Key> out{at};
            for (auto it = parent.find(at); it != parent.end(); it = parent.find(it->second)) out.push_back(it->second);
            std::reverse(out.begin(), out.end());
            return out;
        }
        const W base = best[at];
        for (const Key& next : neighbors(at)) {
            if (closed.count(next)) continue;
            const W    cand = base + cost(at, next);
            const auto it   = best.find(next);
            if (it == best.end() || cand < it->second) {
                best[next]   = cand;
                parent[next] = at;
                open.push({cand + h(next), next});
            }
        }
    }

    return std::vector<Key>();
}

} // namespace implicit

} // namespace graphs
} // namespace fizmo

#endif // FIZMO_GRAPHS_SEARCH_HPP
