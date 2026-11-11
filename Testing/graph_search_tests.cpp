#include "../Source/Graphs/graphs.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace fizmo::graphs;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

class Cell : public Node<int> {
public:
    explicit Cell(int v) : Node<int>(v) {}
    bool wall = false;
    bool passable() const override { return !wall; }
};

using Blocked = Graph<int, double, Cell>;

Blocked grid(int w, int h) {
    Blocked g(false);
    for (int i = 0; i < w * h; ++i) g.add_node(i);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const NodeId id = static_cast<NodeId>(y * w + x);
            if (x + 1 < w) g.add_edge(id, id + 1);
            if (y + 1 < h) g.add_edge(id, id + static_cast<NodeId>(w));
        }
    }
    return g;
}

template <class G>
bool valid(const G& g, const Path<double>& p, NodeId from, NodeId to) {
    if (!p || p.front() != from || p.back() != to) return false;
    for (std::size_t i = 1; i < p.size(); ++i) {
        if (!g.has_edge(p[i - 1], p[i])) return false;
    }
    return p.simple();
}

void test_breadth_first() {
    Blocked g = grid(60, 60);
    for (int y = 0; y < 55; ++y) g.node(static_cast<NodeId>(y * 60 + 30)).wall = true;
    const NodeId from = 0;
    const NodeId to   = 60 * 60 - 1;
    const Path<double> one = breadth_first_path(g, from, to);
    for (unsigned t : {1u, 2u, 4u, 8u, 0u}) {
        SearchStats s;
        const Path<double> p = breadth_first_path(g, from, to, SearchOptions::parallel(t), &s);
        check(valid(g, p, from, to), "parallel breadth first finds a real path with " + std::to_string(t) + " threads");
        check(p.length() == one.length(), "parallel breadth first is as short as single-threaded (" + std::to_string(p.length()) + " vs " + std::to_string(one.length()) + ")");
        check(s.runners == search_threads(t) && s.winner >= 0, "breadth first reports its runners and winner");
    }
    const Traversal a = breadth_first_traversal(g, from);
    const Traversal b = breadth_first_traversal(g, from, SearchOptions::parallel(6));
    check(a.depth == b.depth, "parallel hop distances match single-threaded");
    check(a.order.size() == b.order.size() && b.path_to(to).size() == a.path_to(to).size(), "parallel traversal reaches the same nodes");
    check(!breadth_first_path(g, from, NodeId(30), SearchOptions::parallel(4)), "walls stay walls");
    SearchStats s;
    SearchOptions limited = SearchOptions::parallel(4);
    limited.max_depth = 5;
    check(!breadth_first_path(g, from, to, limited, &s), "max depth stops the search");
}

void test_depth_first() {
    Blocked g = grid(40, 40);
    const NodeId from = 0;
    const NodeId to   = 40 * 40 - 1;
    for (unsigned t : {1u, 3u, 8u}) {
        SearchStats s;
        const Path<double> p = depth_first_path(g, from, to, SearchOptions::parallel(t), &s);
        check(valid(g, p, from, to), "depth first finds a real, loop-free path with " + std::to_string(t) + " threads");
        check(s.runners == t, "depth first runs the runners asked for");
    }
    SearchOptions shortest = SearchOptions::parallel(4);
    shortest.shortest = true;
    Blocked small = grid(7, 7);
    SearchStats s;
    const Path<double> p = depth_first_path(small, NodeId(0), NodeId(48), shortest, &s);
    check(valid(small, p, 0, 48) && p.length() == 12, "shortest depth first gives the shortest path (" + std::to_string(p.length()) + ")");
    check(s.collisions + s.revisits > 0, "runners that meet are counted");

    Graph<int> ring(true);
    for (int i = 0; i < 6; ++i) ring.add_node(i);
    for (int i = 0; i < 6; ++i) ring.add_edge(NodeId(i), NodeId((i + 1) % 6));
    ring.add_node(6);
    SearchStats r;
    check(!depth_first_path(ring, NodeId(0), NodeId(6), SearchOptions::parallel(4), &r), "a loop with no way out ends instead of spinning");
    check(r.loops > 0, "running back into its own branch counts as a loop");
    const Path<double> q = depth_first_path(ring, NodeId(0), NodeId(4), SearchOptions());
    check(q && q.length() == 4, "single-threaded depth first still works with options");
    check(reachable(ring, NodeId(2), NodeId(1), SearchOptions::parallel(2)) && !reachable(ring, NodeId(0), NodeId(6), SearchOptions::parallel(2)), "parallel reachable");
}

void test_big() {
    Graph<int> g(true);
    const int n = 200000;
    for (int i = 0; i < n; ++i) g.add_node(i);
    for (int i = 0; i < n; ++i) {
        g.add_edge(NodeId(i), NodeId((i * 7 + 3) % n));
        g.add_edge(NodeId(i), NodeId((i + 1) % n));
        g.add_edge(NodeId(i), NodeId((i * 13 + 5) % n));
    }
    const Path<double> one = breadth_first_path(g, NodeId(0), NodeId(n / 2 + 17));
    const Path<double> many = breadth_first_path(g, NodeId(0), NodeId(n / 2 + 17), SearchOptions::parallel(0));
    check(one && many && one.length() == many.length(), "big graph: parallel breadth first agrees");
    const std::vector<std::size_t> d1 = hop_distances(g, NodeId(0));
    const std::vector<std::size_t> d2 = hop_distances(g, NodeId(0), SearchOptions::parallel(0));
    check(d1 == d2, "big graph: parallel hop distances agree");
}

} // namespace

int main() {
    for (int round = 0; round < 5; ++round) {
        test_breadth_first();
        test_depth_first();
    }
    test_big();
    std::printf("%d/%d checks passed\n", g_passed, g_passed + g_failed);
    return g_failed == 0 ? 0 : 1;
}
