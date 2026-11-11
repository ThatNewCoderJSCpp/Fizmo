# Multi-threaded graph search

`#include <graphs.hpp>` has breadth-first and depth-first search for any `fizmo::graphs::Traversable` (every `Graph<T, W>` is one). Each search can run on one thread, as before, or on several. Passing a `SearchOptions` picks which:

```cpp
using namespace fizmo::graphs;

Graph<std::string> g;
// ... add nodes and edges ...

Path<double> a = breadth_first_path(g, start, goal);                                  // one thread, as before
Path<double> b = breadth_first_path(g, start, goal, SearchOptions::parallel());       // one runner per CPU core
Path<double> c = depth_first_path(g, start, goal, SearchOptions::parallel(4));        // four runners

SearchStats stats;
Path<double> d = breadth_first_path(g, start, [](NodeId id) { return id % 100 == 7; }, SearchOptions::parallel(8), &stats);
```

The functions without `SearchOptions` are unchanged. The ones with it:

| Function | Gives |
|---|---|
| `breadth_first_path(g, start, goal or test, options, stats*)` | the path with the fewest steps |
| `depth_first_path(g, start, goal or test, options, stats*)` | a path (the first one found, or the shortest with `shortest`) |
| `breadth_first_traversal(g, start, options, stats*)` | a `Traversal`: depth and parent of every node reached |
| `hop_distances(g, start, options, stats*)` | steps from `start` to every node (`unreachable` if none) |
| `reachable(g, from, to, options, stats*)` | whether there's a way |
| `reachable_depth_first(g, from, to, options, stats*)` | the same, searched depth first |

## Options

| `SearchOptions` | Default | Does |
|---|---|---|
| `threads` | `1` | how many runners; `0` is one per CPU core (`SearchOptions::parallel()`), `1` is single-threaded (`SearchOptions::single()`) |
| `max_depth` | no limit | stop looking past this many steps |
| `max_expand` | no limit | stop after looking at this many nodes |
| `shortest` | `false` | depth first: keep searching for the shortest path instead of stopping at the first one |
| `share_below` | `2` | depth first: when the shared pool of branches gets shorter than this (plus idle runners), a busy runner hands one of its branches over |

Breadth first with one thread and no limits calls the original single-threaded function, so its order and results are exactly the same as before.

## How the runners stay correct

Every runner writes into one shared claim table. For each node it holds the depth, the parent and which runner reached it.

- **Runners meeting.** When two runners reach the same node, the one with the shorter path keeps it. If both paths are the same length, whichever claimed it first keeps it, and the other runner leaves that branch.
- **Race conditions.** A claim is checked and written under a per-node lock (256 striped locks, so runners rarely wait), and the depth is also kept in an atomic so most checks don't lock at all. Every parent link points to a node one step closer to the start, so following parents from any node always ends at the start, even when runners overwrite each other's claims.
- **Breadth first** goes one level at a time. All runners take slices of the current level, and a barrier separates the levels.
- **Depth first** gives each runner its own stack and an on-path set: pointers to every node on the branch it's walking. A step back onto a node in that set is a loop, so it is skipped and counted instead of walked forever. When a runner hands a branch to an idle runner, the branch carries the nodes above it, so the new runner still sees loops back into them.

`SearchStats` reports what happened:

| Field | Is |
|---|---|
| `expanded` | nodes looked at |
| `loops` | times a runner walked back into its own branch |
| `collisions` | times a runner reached a node another runner already had |
| `revisits` | times a runner reached a node it (or a shorter path) already had |
| `runners` | how many ran |
| `winner` | which runner found the goal (`-1` if none) |

The graph is only read while a search runs, so don't change it from another thread meanwhile. A goal test passed as a function is called from several threads at once, so it must be safe to call that way (a plain comparison is).

`Testing/graph_search_tests.cpp` (`FIZMO_BUILD_GRAPH_TESTS=ON`) checks the paths against the single-threaded ones and runs clean under ThreadSanitizer.
