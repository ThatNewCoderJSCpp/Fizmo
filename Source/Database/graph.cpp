#include "fizmo_library.hpp"
#include "graph.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <queue>

namespace fizmo {
namespace database {

Graph::Graph(const Table& edges, const GraphOptions& options) : m_directed(options.directed), m_version(edges.version()) {
    const std::size_t from = edges.column_index(options.from);
    const std::size_t to = edges.column_index(options.to);
    const bool weighted = !options.weight.empty();
    const std::size_t wcol = weighted ? edges.column_index(options.weight) : 0;
    m_weighted = weighted;

    auto intern = [&](const Value& v) -> NodeId {
        const std::string key = encode_key(v);
        auto r = m_ids.try_emplace(key, static_cast<NodeId>(m_nodes.size()));
        if (r.second) m_nodes.push_back(v);
        return r.first->second;
    };

    std::vector<std::pair<NodeId, NodeId>> list;
    std::vector<double> weights;
    list.reserve(edges.size());

    edges.for_each([&](RowId r) {
        const Value a = edges.get(r, from);
        const Value b = edges.get(r, to);
        if (a.is_null() || b.is_null()) return;
        double w = 1.0;

        if (weighted) {
            const Value wv = edges.get(r, wcol);
            if (wv.is_null()) return;
            w = wv.numeric();
            if (std::isnan(w)) return;
            if (w < 0) throw DatabaseError("graph weights cannot be negative");
        }

        list.emplace_back(intern(a), intern(b));
        weights.push_back(w);
    });

    const std::size_t n = m_nodes.size();
    const std::size_t count = m_directed ? list.size() : list.size() * 2;
    m_offsets.assign(n + 1, 0);
    m_targets.resize(count);
    if (weighted) m_weights.resize(count);

    for (const auto& e : list) {
        ++m_offsets[e.first + 1];
        if (!m_directed) ++m_offsets[e.second + 1];
    }

    for (std::size_t i = 0; i < n; ++i) m_offsets[i + 1] += m_offsets[i];
    std::vector<std::uint32_t> cursor(m_offsets.begin(), m_offsets.end() - 1);

    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto& e = list[i];
        const std::uint32_t p = cursor[e.first]++;
        m_targets[p] = e.second;
        if (weighted) m_weights[p] = weights[i];

        if (!m_directed) {
            const std::uint32_t q = cursor[e.second]++;
            m_targets[q] = e.first;
            if (weighted) m_weights[q] = weights[i];
        }
    }

    if (m_directed) {
        m_roffsets.assign(n + 1, 0);
        m_rtargets.resize(list.size());
        for (const auto& e : list) ++m_roffsets[e.second + 1];
        for (std::size_t i = 0; i < n; ++i) m_roffsets[i + 1] += m_roffsets[i];
        std::vector<std::uint32_t> rc(m_roffsets.begin(), m_roffsets.end() - 1);
        for (const auto& e : list) m_rtargets[rc[e.second]++] = e.first;
    }
}

Graph::NodeId Graph::id_of(const Value& v) const {
    const NodeId* id = m_ids.get_ptr(encode_key(v));
    return id ? *id : kNone;
}

template <typename Fn>
void Graph::for_out(NodeId n, Fn&& fn) const {
    for (std::uint32_t i = m_offsets[n]; i < m_offsets[n + 1]; ++i) fn(m_targets[i], m_weighted ? m_weights[i] : 1.0);
}

template <typename Fn>
void Graph::for_in(NodeId n, Fn&& fn) const {
    if (!m_directed) { for_out(n, fn); return; }
    for (std::uint32_t i = m_roffsets[n]; i < m_roffsets[n + 1]; ++i) fn(m_rtargets[i], 1.0);
}

bool Graph::contains(const Value& node) const {
    return id_of(node) != kNone;
}

std::vector<Value> Graph::neighbors(const Value& node) const {
    std::vector<Value> out;
    const NodeId n = id_of(node);
    if (n == kNone) return out;
    for_out(n, [&](NodeId t, double) { out.push_back(m_nodes[t]); });
    return out;
}

std::vector<Value> Graph::incoming(const Value& node) const {
    std::vector<Value> out;
    const NodeId n = id_of(node);
    if (n == kNone) return out;
    for_in(n, [&](NodeId t, double) { out.push_back(m_nodes[t]); });
    return out;
}

std::size_t Graph::out_degree(const Value& node) const {
    const NodeId n = id_of(node);
    return n == kNone ? 0 : m_offsets[n + 1] - m_offsets[n];
}

std::size_t Graph::in_degree(const Value& node) const {
    const NodeId n = id_of(node);
    if (n == kNone) return 0;
    if (!m_directed) return m_offsets[n + 1] - m_offsets[n];
    return m_roffsets[n + 1] - m_roffsets[n];
}

std::vector<GraphVisit> Graph::bfs(const Value& start, std::size_t max_depth, std::size_t limit) const {
    std::vector<GraphVisit> out;
    const NodeId s = id_of(start);
    if (s == kNone) return out;
    std::vector<std::uint32_t> depth(m_nodes.size(), std::numeric_limits<std::uint32_t>::max());
    std::vector<NodeId> parent(m_nodes.size(), kNone);
    std::vector<NodeId> frontier{ s };
    depth[s] = 0;
    out.push_back(GraphVisit{ m_nodes[s], 0, Value() });
    std::size_t head = 0;

    while (head < frontier.size()) {
        const NodeId n = frontier[head++];
        if (depth[n] >= max_depth) continue;
        bool stop = false;

        for_out(n, [&](NodeId t, double) {
            if (stop || depth[t] != std::numeric_limits<std::uint32_t>::max()) return;
            depth[t] = depth[n] + 1;
            parent[t] = n;
            frontier.push_back(t);
            out.push_back(GraphVisit{ m_nodes[t], depth[t], m_nodes[n] });
            if (limit && out.size() >= limit) stop = true;
        });

        if (stop) break;
    }

    return out;
}

std::vector<GraphVisit> Graph::dfs(const Value& start, std::size_t max_depth, std::size_t limit) const {
    std::vector<GraphVisit> out;
    const NodeId s = id_of(start);
    if (s == kNone) return out;
    std::vector<char> seen(m_nodes.size(), 0);
    struct Frame { NodeId node; std::uint32_t next; std::uint32_t depth; };
    std::vector<Frame> stack;
    stack.push_back(Frame{ s, m_offsets[s], 0 });
    seen[s] = 1;
    out.push_back(GraphVisit{ m_nodes[s], 0, Value() });

    while (!stack.empty()) {
        Frame& f = stack.back();

        if (f.depth >= max_depth || f.next >= m_offsets[f.node + 1]) {
            stack.pop_back();
            continue;
        }

        const NodeId t = m_targets[f.next++];
        if (seen[t]) continue;
        seen[t] = 1;
        out.push_back(GraphVisit{ m_nodes[t], f.depth + 1, m_nodes[f.node] });
        if (limit && out.size() >= limit) break;
        stack.push_back(Frame{ t, m_offsets[t], f.depth + 1 });
    }

    return out;
}

GraphPath Graph::shortest_path(const Value& from, const Value& to) const {
    GraphPath result;
    const NodeId s = id_of(from), g = id_of(to);
    if (s == kNone || g == kNone) return result;

    if (s == g) {
        result.found = true;
        result.nodes.push_back(m_nodes[s]);
        return result;
    }

    const std::size_t n = m_nodes.size();

    if (m_weighted) {
        std::vector<double> dist(n, std::numeric_limits<double>::infinity());
        std::vector<NodeId> parent(n, kNone);
        using Item = std::pair<double, NodeId>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        dist[s] = 0.0;
        heap.emplace(0.0, s);

        while (!heap.empty()) {
            const double d = heap.top().first;
            const NodeId u = heap.top().second;
            heap.pop();
            if (d > dist[u]) continue;
            if (u == g) break;

            for_out(u, [&](NodeId v, double w) {
                const double nd = d + w;

                if (nd < dist[v]) {
                    dist[v] = nd;
                    parent[v] = u;
                    heap.emplace(nd, v);
                }
            });
        }

        if (dist[g] == std::numeric_limits<double>::infinity()) return result;
        for (NodeId v = g; v != kNone; v = parent[v]) result.nodes.push_back(m_nodes[v]);
        std::reverse(result.nodes.begin(), result.nodes.end());
        result.cost = dist[g];
        result.found = true;
        return result;
    }

    std::vector<NodeId> pf(n, kNone), pb(n, kNone);
    std::vector<char> sf(n, 0), sb(n, 0);
    std::vector<NodeId> qf{ s }, qb{ g };
    sf[s] = 1;
    sb[g] = 1;
    NodeId meet = kNone;

    while (!qf.empty() && !qb.empty() && meet == kNone) {
        const bool forward = qf.size() <= qb.size();
        std::vector<NodeId> next;

        for (NodeId u : forward ? qf : qb) {
            auto visit = [&](NodeId v, double) {
                if (meet != kNone) return;

                if (forward) {
                    if (sf[v]) return;
                    sf[v] = 1;
                    pf[v] = u;
                    if (sb[v]) { meet = v; return; }
                } else {
                    if (sb[v]) return;
                    sb[v] = 1;
                    pb[v] = u;
                    if (sf[v]) { meet = v; return; }
                }

                next.push_back(v);
            };

            if (forward) for_out(u, visit);
            else for_in(u, visit);
            if (meet != kNone) break;
        }

        if (forward) qf.swap(next);
        else qb.swap(next);
    }

    if (meet == kNone) return result;
    std::vector<NodeId> path;
    for (NodeId v = meet; v != kNone; v = pf[v]) path.push_back(v);
    std::reverse(path.begin(), path.end());
    for (NodeId v = pb[meet]; v != kNone; v = pb[v]) path.push_back(v);
    for (NodeId v : path) result.nodes.push_back(m_nodes[v]);
    result.cost = static_cast<double>(path.size() - 1);
    result.found = true;
    return result;
}

bool Graph::reachable(const Value& from, const Value& to, std::size_t max_depth) const {
    if (max_depth == kUnlimited) return shortest_path(from, to).found;
    const NodeId g = id_of(to);
    if (g == kNone) return false;

    for (const auto& v : bfs(from, max_depth)) {
        if (compare(v.node, m_nodes[g]) == 0) return true;
    }

    return false;
}

std::vector<std::vector<Value>> Graph::components() const {
    const std::size_t n = m_nodes.size();
    std::vector<NodeId> parent(n);
    std::vector<std::uint32_t> size(n, 1);
    std::iota(parent.begin(), parent.end(), 0);

    std::function<NodeId(NodeId)> find = [&](NodeId x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    for (NodeId u = 0; u < n; ++u) {
        for_out(u, [&](NodeId v, double) {
            NodeId a = find(u), b = find(v);
            if (a == b) return;
            if (size[a] < size[b]) std::swap(a, b);
            parent[b] = a;
            size[a] += size[b];
        });
    }

    collections::HashMap<NodeId, std::size_t> slot;
    std::vector<std::vector<Value>> out;

    for (NodeId u = 0; u < n; ++u) {
        const NodeId r = find(u);
        auto ins = slot.try_emplace(r, out.size());
        if (ins.second) out.emplace_back();
        out[ins.first->second].push_back(m_nodes[u]);
    }

    std::sort(out.begin(), out.end(), [](const std::vector<Value>& a, const std::vector<Value>& b) { return a.size() > b.size(); });
    return out;
}

std::optional<std::vector<Value>> Graph::topological_order() const {
    if (!m_directed) throw DatabaseError("topological order needs a directed graph");
    const std::size_t n = m_nodes.size();
    std::vector<std::uint32_t> indeg(n, 0);
    for (NodeId t : m_targets) ++indeg[t];
    std::vector<NodeId> queue;
    queue.reserve(n);
    for (NodeId u = 0; u < n; ++u) if (indeg[u] == 0) queue.push_back(u);
    std::size_t head = 0;

    while (head < queue.size()) {
        const NodeId u = queue[head++];
        for_out(u, [&](NodeId v, double) { if (--indeg[v] == 0) queue.push_back(v); });
    }

    if (queue.size() != n) return std::nullopt;
    std::vector<Value> out;
    out.reserve(n);
    for (NodeId u : queue) out.push_back(m_nodes[u]);
    return out;
}

} // namespace database
} // namespace fizmo
