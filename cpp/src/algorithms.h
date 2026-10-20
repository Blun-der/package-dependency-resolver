// Package Dependency Resolver - graph algorithms
#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "graph.h"

namespace pdr {

using Component = std::vector<int>;  // node ids, sorted by name

// Tarjan's strongly connected components, iterative (no recursion limit).
// Every component with 2+ files is a group of files that import each other
// in a loop. A component comes out only after every component it imports,
// so the list is in dependencies-first order. O(V + E).
std::vector<Component> tarjanSCC(const Graph& graph);

// For every file, how many files it needs (directly or not) and how many
// files need it: the same numbers as one BFS per file, O(V * (V + E)), but
// computed with bitsets on the graph of components, which has no loops.
// Going through the components dependencies first, a component reaches its
// own files (if it is a loop) plus everything each imported component reaches:
// one OR of 64-bit words per link handles 64 files at once. The files are
// done in slices so memory stays small, giving O(V / 64 * (V + E)) time and
// O(components * slice) memory.
struct ReachCounts {
    std::vector<int> dependencies;  // node id -> files it needs
    std::vector<int> dependents;    // node id -> files that need it
};
ReachCounts reachCounts(const Graph& graph, size_t memoryLimit = 64u << 20);  // bytes of bitsets

// Which imports to remove so a group of files that import each other in a
// loop has no loop left. Greedy: repeatedly remove the one import that leaves
// the fewest files still in a loop. Ties go to the import that most breaks the
// Stable Dependencies Principle (R. C. Martin): with instability
// I = fan-out / (fan-in + fan-out), a file should not import a file that is
// less stable than itself, so prefer cutting A -> B with the largest I(B) - I(A).
// Then alphabetical. Finding the smallest
// possible set is the "minimum feedback arc set" problem, which is NP-hard,
// so this gives a good plan, not always the smallest one.
// O(k * E * (V + E)) for k removals inside the component; empty optional if
// the component has more than `maxEdges` links (too slow to try them all).
using Link = std::pair<int, int>;  // (from, to)
std::optional<std::vector<Link>> cycleBreakingPlan(const Graph& graph, const Component& component,
                                                   size_t maxEdges = 2000);

// Shortest chain of imports from -> ... -> to, found with BFS. `allowed`
// optionally restricts the nodes visited. Empty if there is no chain.
// With from == to it finds the shortest loop through that node.
std::vector<int> shortestPath(const Graph& graph, int from, int to,
                              const std::vector<bool>* allowed = nullptr);

// One concrete loop inside a strongly connected component: start -> ... -> start
std::vector<int> cycleIn(const Graph& graph, const Component& component);

// Every node reachable from start (BFS), start itself excluded.
// With reverse = true it follows importers instead: "who breaks if start changes?"
std::vector<int> reachable(const Graph& graph, int start, bool reverse);

struct BuildUnit {
    std::vector<int> files;  // one file, or every file of a cycle
    bool cyclic = false;
    int layer = 0;           // 0 = imports nothing, k = 1 + max layer of its dependencies
};

struct BuildOrder {
    std::vector<BuildUnit> order;
    std::vector<int> layerOf;  // node id -> layer
    std::vector<int> compOf;   // node id -> index of its component
};

// Kahn's algorithm with a min-heap on the condensed graph (each cycle
// collapsed into one unit). Dependencies come before the files that use them.
// O(V log V + E).
BuildOrder buildOrder(const Graph& graph, const std::vector<Component>& sccs);

}  // namespace pdr
