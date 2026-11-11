#ifndef FIZMO_GRAPHS_PARALLEL_SEARCH_HPP
#define FIZMO_GRAPHS_PARALLEL_SEARCH_HPP

#include "search.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fizmo {
namespace graphs {

struct SearchOptions {
    unsigned    threads     = 1;
    std::size_t max_depth   = unreachable;
    std::size_t max_expand  = unreachable;
    bool        shortest    = false;
    std::size_t share_below = 2;

    static SearchOptions single() { return SearchOptions(); }

    static SearchOptions parallel(unsigned thread_count = 0) {
        SearchOptions o;
        o.threads = thread_count;
        return o;
    }
};

struct SearchStats {
    std::size_t expanded   = 0;
    std::size_t loops      = 0;
    std::size_t collisions = 0;
    std::size_t revisits   = 0;
    unsigned    runners    = 1;
    int         winner     = -1;
};

inline unsigned search_threads(unsigned requested) {
    if (requested == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        requested         = hw == 0 ? 4u : hw;
    }
    return std::max(1u, std::min(requested, 256u));
}

namespace gdetail {

class ClaimTable {
public:
    static constexpr std::uint32_t none = std::numeric_limits<std::uint32_t>::max();

    explicit ClaimTable(std::size_t count)
        : m_depth(new std::atomic<std::uint32_t>[count]),
          m_runner(count, -1),
          m_parent(count, invalid_node),
          m_count(count) {
        for (std::size_t i = 0; i < count; ++i) m_depth[i].store(none, std::memory_order_relaxed);
    }

    bool claim(NodeId node, NodeId parent, std::uint32_t depth, int runner, std::uint32_t& was_depth, int& was_runner) {
        was_depth  = none;
        was_runner = -1;
        if (node >= m_count) return false;
        if (m_depth[node].load(std::memory_order_acquire) <= depth) {
            std::lock_guard<std::mutex> lock(stripe(node));
            was_depth  = m_depth[node].load(std::memory_order_relaxed);
            was_runner = m_runner[node];
            return false;
        }
        std::lock_guard<std::mutex> lock(stripe(node));
        const std::uint32_t cur = m_depth[node].load(std::memory_order_relaxed);
        was_depth  = cur;
        was_runner = cur == none ? -1 : m_runner[node];
        if (cur <= depth) return false;
        m_parent[node] = parent;
        m_runner[node] = runner;
        m_depth[node].store(depth, std::memory_order_release);
        return true;
    }

    std::uint32_t depth(NodeId node) const { return node < m_count ? m_depth[node].load(std::memory_order_acquire) : none; }

    std::vector<NodeId> parents() const { return m_parent; }

    std::vector<std::size_t> depths() const {
        std::vector<std::size_t> out(m_count, unreachable);
        for (std::size_t i = 0; i < m_count; ++i) {
            const std::uint32_t d = m_depth[i].load(std::memory_order_relaxed);
            if (d != none) out[i] = d;
        }
        return out;
    }

private:
    static constexpr std::size_t stripes = 256;

    std::mutex& stripe(NodeId node) { return m_locks[(node * 0x9E3779B97F4A7C15ULL >> 56) % stripes]; }

    std::unique_ptr<std::atomic<std::uint32_t>[]> m_depth;
    std::vector<int>                              m_runner;
    std::vector<NodeId>                           m_parent;
    std::size_t                                   m_count;
    std::mutex                                    m_locks[stripes];
};

class LevelBarrier {
public:
    explicit LevelBarrier(unsigned count) : m_count(count) {}

    void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        const std::size_t gen = m_generation;
        if (++m_waiting == m_count) {
            m_waiting = 0;
            ++m_generation;
            m_cv.notify_all();
            return;
        }
        m_cv.wait(lock, [&] { return gen != m_generation; });
    }

private:
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    unsigned                m_count;
    unsigned                m_waiting    = 0;
    std::size_t             m_generation = 0;
};

template <class Fn>
void run_on_threads(unsigned count, Fn& fn) {
    if (count <= 1) {
        fn(0);
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve(count - 1);
    for (unsigned i = 1; i < count; ++i) pool.emplace_back([&fn, i] { fn(static_cast<int>(i)); });
    fn(0);
    for (std::thread& t : pool) t.join();
}

template <class W, class Goal>
NodeId parallel_breadth_first(const Traversable<W>& g, NodeId start, Goal& is_goal, const SearchOptions& o, ClaimTable& table, SearchStats& stats, bool stop_at_goal) {
    const unsigned runners = search_threads(o.threads);
    stats.runners          = runners;
    std::uint32_t was_depth = 0;
    int           was_runner = -1;
    table.claim(start, invalid_node, 0, 0, was_depth, was_runner);
    if (stop_at_goal && is_goal(start)) {
        stats.winner = 0;
        return start;
    }
    const std::size_t           n = g.node_count();
    std::vector<NodeId>         frontier{start};
    std::vector<std::vector<NodeId>> next(runners);
    std::atomic<std::size_t>    cursor{0};
    std::atomic<std::size_t>    expanded{0};
    std::atomic<std::size_t>    collisions{0};
    std::atomic<std::size_t>    revisits{0};
    std::atomic<bool>           stop{false};
    std::atomic<bool>           finished{false};
    std::mutex                  goal_mutex;
    NodeId                      goal  = invalid_node;
    std::uint32_t               level = 0;
    LevelBarrier                barrier(runners);

    auto worker = [&](int me) {
        while (true) {
            barrier.wait();
            if (finished.load()) return;
            const std::size_t chunk = std::max<std::size_t>(1, frontier.size() / (static_cast<std::size_t>(runners) * 8));
            while (!stop.load(std::memory_order_relaxed)) {
                const std::size_t begin = cursor.fetch_add(chunk);
                if (begin >= frontier.size()) break;
                const std::size_t end = std::min(frontier.size(), begin + chunk);
                for (std::size_t i = begin; i < end && !stop.load(std::memory_order_relaxed); ++i) {
                    if (expanded.fetch_add(1) >= o.max_expand) {
                        stop.store(true);
                        break;
                    }
                    const NodeId u = frontier[i];
                    g.for_each_edge(u, [&](const Edge<W>& e) {
                        if (stop.load(std::memory_order_relaxed)) return;
                        const NodeId v = e.to;
                        if (v >= n || !g.passable(v)) return;
                        std::uint32_t wd = 0;
                        int           wr = -1;
                        if (!table.claim(v, u, level + 1, me, wd, wr)) {
                            if (wr >= 0 && wr != me && wd == level + 1) collisions.fetch_add(1, std::memory_order_relaxed);
                            else revisits.fetch_add(1, std::memory_order_relaxed);
                            return;
                        }
                        if (stop_at_goal && is_goal(v)) {
                            std::lock_guard<std::mutex> lock(goal_mutex);
                            if (goal == invalid_node) {
                                goal         = v;
                                stats.winner = me;
                            }
                            stop.store(true);
                            return;
                        }
                        next[static_cast<std::size_t>(me)].push_back(v);
                    });
                }
            }
            barrier.wait();
            if (me == 0) {
                frontier.clear();
                for (std::vector<NodeId>& part : next) {
                    frontier.insert(frontier.end(), part.begin(), part.end());
                    part.clear();
                }
                cursor.store(0);
                ++level;
                if (stop.load() || frontier.empty() || level >= o.max_depth) finished.store(true);
            }
        }
    };
    run_on_threads(runners, worker);
    stats.expanded   = std::min(expanded.load(), o.max_expand);
    stats.collisions = collisions.load();
    stats.revisits   = revisits.load();
    return goal;
}

template <class W, class Goal>
NodeId parallel_depth_first(const Traversable<W>& g, NodeId start, Goal& is_goal, const SearchOptions& o, ClaimTable& table, SearchStats& stats) {
    struct Seed {
        NodeId              node;
        std::uint32_t       depth;
        std::vector<NodeId> above;
    };
    struct Frame {
        NodeId              node;
        NodeId              parent;
        std::uint32_t       depth;
        std::vector<NodeId> next;
        std::size_t         cursor   = 0;
        bool                expanded = false;
    };
    const unsigned runners = search_threads(o.threads);
    stats.runners          = runners;
    const std::size_t n    = g.node_count();
    const std::uint32_t depth_limit = o.max_depth >= ClaimTable::none ? ClaimTable::none - 1 : static_cast<std::uint32_t>(o.max_depth);
    std::uint32_t was_depth = 0;
    int           was_runner = -1;
    table.claim(start, invalid_node, 0, 0, was_depth, was_runner);
    std::mutex                 pool_mutex;
    std::condition_variable    pool_cv;
    std::deque<Seed>           pool;
    pool.push_back(Seed{start, 0, {}});
    unsigned                   idle = 0;
    bool                       done = false;
    std::atomic<unsigned>      waiting{0};
    std::atomic<bool>          stop{false};
    std::atomic<std::size_t>   expanded{0};
    std::atomic<std::size_t>   loops{0};
    std::atomic<std::size_t>   collisions{0};
    std::atomic<std::size_t>   revisits{0};
    std::atomic<std::uint32_t> best{ClaimTable::none};
    std::mutex                 goal_mutex;
    NodeId                     goal = invalid_node;

    auto halt = [&] {
        stop.store(true);
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool_cv.notify_all();
    };
    auto take = [&](Seed& out) {
        std::unique_lock<std::mutex> lock(pool_mutex);
        ++idle;
        waiting.store(idle);
        while (pool.empty() && !done && !stop.load()) {
            if (idle == runners) {
                done = true;
                pool_cv.notify_all();
                break;
            }
            pool_cv.wait(lock);
        }
        if (pool.empty() || stop.load()) return false;
        --idle;
        waiting.store(idle);
        out = std::move(pool.front());
        pool.pop_front();
        return true;
    };
    auto give = [&](Seed&& s) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool.push_back(std::move(s));
        pool_cv.notify_one();
    };
    auto hungry = [&] {
        if (runners <= 1 || waiting.load(std::memory_order_relaxed) == 0) return false;
        std::lock_guard<std::mutex> lock(pool_mutex);
        return pool.size() < o.share_below + idle;
    };
    auto reached = [&](NodeId at, std::uint32_t depth, int me) {
        {
            std::lock_guard<std::mutex> lock(goal_mutex);
            if (goal == invalid_node || depth < best.load()) {
                goal         = at;
                stats.winner = me;
                best.store(depth);
            }
        }
        if (!o.shortest) halt();
    };

    auto worker = [&](int me) {
        std::vector<Frame>         stack;
        std::unordered_set<NodeId> on_path;
        Seed                       seed{start, 0, {}};
        while (!stop.load() && take(seed)) {
            if (o.shortest && table.depth(seed.node) < seed.depth) continue;
            stack.clear();
            on_path.clear();
            stack.push_back(Frame{seed.node, seed.above.empty() ? invalid_node : seed.above.back(), seed.depth, {}, 0, false});
            on_path.insert(seed.above.begin(), seed.above.end());
            on_path.insert(seed.node);
            while (!stack.empty() && !stop.load(std::memory_order_relaxed)) {
                Frame& f = stack.back();
                if (!f.expanded) {
                    f.expanded = true;
                    if (is_goal(f.node)) {
                        reached(f.node, f.depth, me);
                        on_path.erase(f.node);
                        stack.pop_back();
                        continue;
                    }
                    if (expanded.fetch_add(1) >= o.max_expand) {
                        halt();
                        break;
                    }
                    g.for_each_edge(f.node, [&f](const Edge<W>& e) { f.next.push_back(e.to); });
                }
                if ((o.shortest && table.depth(f.node) < f.depth) || f.cursor >= f.next.size()) {
                    on_path.erase(f.node);
                    stack.pop_back();
                    continue;
                }
                const NodeId        v    = f.next[f.cursor++];
                const NodeId        from = f.node;
                const NodeId        back = f.parent;
                const std::uint32_t d    = f.depth + 1;
                if (v >= n || !g.passable(v)) continue;
                if (on_path.count(v)) {
                    if (v != back || g.is_directed()) loops.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (d > depth_limit || d >= best.load(std::memory_order_relaxed)) continue;
                std::uint32_t wd = 0;
                int           wr = -1;
                if (!table.claim(v, from, d, me, wd, wr)) {
                    if (wr >= 0 && wr != me) collisions.fetch_add(1, std::memory_order_relaxed);
                    else revisits.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (wr >= 0) {
                    if (wr != me) collisions.fetch_add(1, std::memory_order_relaxed);
                    else revisits.fetch_add(1, std::memory_order_relaxed);
                    if (!o.shortest) continue;
                }
                if (stack.size() > 1 && hungry()) {
                    Seed handed{v, d, seed.above};
                    for (const Frame& up : stack) handed.above.push_back(up.node);
                    give(std::move(handed));
                    continue;
                }
                stack.push_back(Frame{v, from, d, {}, 0, false});
                on_path.insert(v);
            }
        }
    };
    run_on_threads(runners, worker);
    stats.expanded   = std::min(expanded.load(), o.max_expand);
    stats.loops      = loops.load();
    stats.collisions = collisions.load();
    stats.revisits   = revisits.load();
    return goal;
}

template <class W>
Path<W> claimed_path(const Traversable<W>& g, const ClaimTable& table, NodeId start, NodeId goal) {
    if (goal == invalid_node) return Path<W>();
    return rebuild(g, table.parents(), start, goal);
}

} // namespace gdetail

template <class W, class Goal, typename std::enable_if<!std::is_convertible<Goal, NodeId>::value, int>::type = 0>
Path<W> breadth_first_path(const Traversable<W>& g, NodeId start, Goal&& is_goal, const SearchOptions& options, SearchStats* stats = nullptr) {
    SearchStats local;
    SearchStats& s = stats ? *stats : local;
    s = SearchStats();
    if (search_threads(options.threads) == 1 && options.max_depth == unreachable && options.max_expand == unreachable) {
        s.runners = 1;
        Path<W> p = breadth_first_path(g, start, is_goal);
        if (p) s.winner = 0;
        return p;
    }
    if (!g.contains(start) || !g.passable(start)) return Path<W>();
    gdetail::ClaimTable table(g.node_count());
    const NodeId goal = gdetail::parallel_breadth_first(g, start, is_goal, options, table, s, true);
    return gdetail::claimed_path(g, table, start, goal);
}

template <class W>
Path<W> breadth_first_path(const Traversable<W>& g, NodeId start, NodeId goal, const SearchOptions& options, SearchStats* stats = nullptr) {
    return breadth_first_path(g, start, [goal](NodeId id) { return id == goal; }, options, stats);
}

template <class W, class Goal, typename std::enable_if<!std::is_convertible<Goal, NodeId>::value, int>::type = 0>
Path<W> depth_first_path(const Traversable<W>& g, NodeId start, Goal&& is_goal, const SearchOptions& options, SearchStats* stats = nullptr) {
    SearchStats local;
    SearchStats& s = stats ? *stats : local;
    s = SearchStats();
    if (!g.contains(start) || !g.passable(start)) return Path<W>();
    gdetail::ClaimTable table(g.node_count());
    const NodeId goal = gdetail::parallel_depth_first(g, start, is_goal, options, table, s);
    return gdetail::claimed_path(g, table, start, goal);
}

template <class W>
Path<W> depth_first_path(const Traversable<W>& g, NodeId start, NodeId goal, const SearchOptions& options, SearchStats* stats = nullptr) {
    return depth_first_path(g, start, [goal](NodeId id) { return id == goal; }, options, stats);
}

template <class W>
Traversal breadth_first_traversal(const Traversable<W>& g, NodeId start, const SearchOptions& options, SearchStats* stats = nullptr) {
    SearchStats local;
    SearchStats& s = stats ? *stats : local;
    s = SearchStats();
    if (search_threads(options.threads) == 1 && options.max_depth == unreachable && options.max_expand == unreachable) {
        s.runners = 1;
        return breadth_first_traversal(g, start);
    }
    Traversal out;
    const std::size_t n = g.node_count();
    out.depth.assign(n, unreachable);
    out.parent.assign(n, invalid_node);
    if (!g.contains(start) || !g.passable(start)) return out;
    gdetail::ClaimTable table(n);
    auto never = [](NodeId) { return false; };
    gdetail::parallel_breadth_first(g, start, never, options, table, s, false);
    out.depth  = table.depths();
    out.parent = table.parents();
    for (NodeId i = 0; i < n; ++i) {
        if (out.depth[i] != unreachable) out.order.push_back(i);
    }
    std::stable_sort(out.order.begin(), out.order.end(), [&](NodeId a, NodeId b) { return out.depth[a] < out.depth[b]; });
    return out;
}

template <class W>
std::vector<std::size_t> hop_distances(const Traversable<W>& g, NodeId start, const SearchOptions& options, SearchStats* stats = nullptr) {
    return breadth_first_traversal(g, start, options, stats).depth;
}

template <class W>
bool reachable(const Traversable<W>& g, NodeId from, NodeId to, const SearchOptions& options, SearchStats* stats = nullptr) {
    return breadth_first_path(g, from, to, options, stats).found();
}

template <class W>
bool reachable_depth_first(const Traversable<W>& g, NodeId from, NodeId to, const SearchOptions& options = SearchOptions(), SearchStats* stats = nullptr) {
    return depth_first_path(g, from, to, options, stats).found();
}

} // namespace graphs
} // namespace fizmo

#endif // FIZMO_GRAPHS_PARALLEL_SEARCH_HPP
