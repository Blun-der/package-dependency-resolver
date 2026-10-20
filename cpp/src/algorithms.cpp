#include "algorithms.h"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <set>
#include <utility>

#include "hash_map.h"
#include "min_heap.h"

namespace pdr {

namespace {

// Tarjan's algorithm, starting from `nodes` in that order and following an
// edge v -> w only when follow(v, w) is true, so the cycle-fix search can try
// the graph with some links removed.
template <typename Follow>
std::vector<Component> tarjan(const Graph& graph, const std::vector<int>& nodes, Follow follow) {
    const int n = graph.size();
    std::vector<int> index(n, -1), low(n, 0);
    std::vector<bool> onStack(n, false);
    std::vector<int> stack;                      // files on the current DFS path
    std::vector<std::pair<int, size_t>> work;    // explicit call stack: (node, next edge)
    std::vector<Component> sccs;
    int counter = 0;

    auto visit = [&](int v) {
        index[v] = low[v] = counter++;
        stack.push_back(v);
        onStack[v] = true;
        work.push_back({v, 0});
    };

    for (int s : nodes) {
        if (index[s] != -1) continue;
        visit(s);
        while (!work.empty()) {
            const int v = work.back().first;
            const std::vector<Edge>& out = graph.out(v);
            if (work.back().second < out.size()) {
                const int w = out[work.back().second++].to;
                if (!follow(v, w)) continue;
                if (index[w] == -1) visit(w);
                else if (onStack[w]) low[v] = std::min(low[v], index[w]);
                continue;
            }
            // all edges of v done: "return" to the caller
            work.pop_back();
            if (!work.empty()) {
                const int u = work.back().first;
                low[u] = std::min(low[u], low[v]);
            }
            if (low[v] == index[v]) {  // v is the root of a component
                Component comp;
                int w;
                do {
                    w = stack.back();
                    stack.pop_back();
                    onStack[w] = false;
                    comp.push_back(w);
                } while (w != v);
                std::sort(comp.begin(), comp.end(),
                          [&](int a, int b) { return graph.name(a) < graph.name(b); });
                sccs.push_back(comp);
            }
        }
    }
    return sccs;
}

}  // namespace

std::vector<Component> tarjanSCC(const Graph& graph) {
    return tarjan(graph, graph.nodesByName(), [](int, int) { return true; });
}

std::optional<std::vector<Link>> cycleBreakingPlan(const Graph& graph, const Component& component,
                                                   size_t maxEdges) {
    // Copy the component into its own small graph, so every trial run of
    // Tarjan's costs O(size of the component), not O(size of the project).
    std::vector<bool> inside(graph.size(), false);
    for (int v : component) inside[v] = true;
    Graph loop;
    for (int v : component) loop.addNode(graph.name(v));
    for (int v : component)
        for (const Edge& e : graph.out(v))
            if (inside[e.to]) loop.addEdge(graph.name(v), graph.name(e.to), e.evidence);
    if (static_cast<size_t>(loop.edgeCount()) > maxEdges) return std::nullopt;
    const std::vector<int> nodes = loop.nodesByName();

    // Instability over the whole project, not just the loop: 0 = only imported, 1 = only imports.
    auto instability = [&](int loopId) {
        const int v = graph.id(loop.name(loopId));
        const double in = static_cast<double>(graph.in(v).size()), out = static_cast<double>(graph.out(v).size());
        return in + out > 0 ? out / (in + out) : 0.0;
    };

    HashSet<std::uint64_t> removed;
    auto follow = [&](int v, int w) { return !removed.contains(linkKey(v, w)); };
    // number of files that are still part of some loop
    auto filesInLoops = [&](const std::vector<Component>& sccs) {
        size_t count = 0;
        for (const Component& c : sccs) count += c.size() > 1 ? c.size() : 0;
        return count;
    };

    std::vector<Link> plan;
    for (;;) {
        const std::vector<Component> sccs = tarjan(loop, nodes, follow);
        if (filesInLoops(sccs) == 0) return plan;

        std::optional<Link> best;
        size_t bestLeft = 0;
        double bestViolation = 0;
        for (const Component& c : sccs) {
            if (c.size() < 2) continue;
            std::vector<bool> sameLoop(loop.size(), false);
            for (int v : c) sameLoop[v] = true;
            for (int v : c) {  // components are sorted by name, so ties keep the alphabetical first
                for (const Edge& e : loop.out(v)) {
                    if (!sameLoop[e.to] || removed.contains(linkKey(v, e.to))) continue;
                    removed.insert(linkKey(v, e.to));  // try without this link
                    const size_t left = filesInLoops(tarjan(loop, nodes, follow));
                    removed.erase(linkKey(v, e.to));
                    const double violation = instability(e.to) - instability(v);
                    if (!best || left < bestLeft || (left == bestLeft && violation > bestViolation + 1e-12)) {
                        best = Link{v, e.to};
                        bestLeft = left;
                        bestViolation = violation;
                    }
                }
            }
        }
        removed.insert(linkKey(best->first, best->second));
        plan.push_back({graph.id(loop.name(best->first)), graph.id(loop.name(best->second))});
    }
}

std::vector<int> shortestPath(const Graph& graph, int from, int to, const std::vector<bool>* allowed) {
    if (from < 0 || to < 0) return {};
    const int unseen = -2, root = -1;
    std::vector<int> parent(graph.size(), unseen);
    parent[from] = root;
    std::vector<int> queue{from};  // FIFO queue: `head` is the front
    for (size_t head = 0; head < queue.size(); head++) {
        const int v = queue[head];
        for (const Edge& e : graph.out(v)) {
            const int w = e.to;
            if (allowed && !(*allowed)[w]) continue;
            if (w == to && v != to) {
                std::vector<int> path{w};
                for (int x = v; x != root; x = parent[x]) path.push_back(x);
                std::reverse(path.begin(), path.end());
                return path;
            }
            if (parent[w] == unseen) {
                parent[w] = v;
                queue.push_back(w);
            }
        }
    }
    return {};
}

std::vector<int> cycleIn(const Graph& graph, const Component& component) {
    std::vector<bool> allowed(graph.size(), false);
    for (int v : component) allowed[v] = true;
    return shortestPath(graph, component[0], component[0], &allowed);
}

std::vector<int> reachable(const Graph& graph, int start, bool reverse) {
    std::vector<bool> seen(graph.size(), false);
    seen[start] = true;
    std::vector<int> queue{start};
    for (size_t head = 0; head < queue.size(); head++) {
        const int v = queue[head];
        for (const Edge& e : reverse ? graph.in(v) : graph.out(v)) {
            if (!seen[e.to]) {
                seen[e.to] = true;
                queue.push_back(e.to);
            }
        }
    }
    queue.erase(queue.begin());  // drop start itself
    return queue;
}

namespace {

// How many nodes each node reaches, following out-edges (reverse = false) or
// in-edges. `sccs` must be in dependencies-first order (as tarjanSCC gives).
std::vector<int> countReach(const Graph& graph, const std::vector<Component>& sccs,
                            const std::vector<int>& compOf, bool reverse, size_t memoryLimit) {
    const int n = graph.size();
    const int comps = static_cast<int>(sccs.size());
    // Each component gets a row of `words` 64-bit words: bit k set = node
    // (first + k) of the current slice is in the component or reached from
    // it. Every file's count is then the bits of its row minus itself (a file
    // in a loop does reach itself, but BFS does not count the start). All rows
    // together stay under memoryLimit; small projects take everything in one slice.
    const size_t maxWords = std::max<size_t>(1, memoryLimit / 8 / std::max(comps, 1));
    const size_t words = std::min<size_t>((n + 63) / 64, maxWords);
    const int slice = static_cast<int>(words * 64);
    std::vector<std::uint64_t> rows(static_cast<size_t>(comps) * words);
    std::vector<int> count(n, 0);

    // Components to visit so that every component's links are done first:
    // dependencies first when following imports, the other way round for importers.
    std::vector<int> visitOrder(comps);
    for (int i = 0; i < comps; i++) visitOrder[i] = reverse ? comps - 1 - i : i;

    for (int first = 0; first < n; first += slice) {
        std::fill(rows.begin(), rows.end(), 0);
        auto setBit = [&](int comp, int node) {
            if (node < first || node >= first + slice) return;
            const int k = node - first;
            rows[comp * words + k / 64] |= std::uint64_t{1} << (k % 64);
        };
        for (int c : visitOrder) {
            std::uint64_t* row = &rows[c * words];
            for (int f : sccs[c]) setBit(c, f);
            for (int f : sccs[c]) {
                for (const Edge& e : reverse ? graph.in(f) : graph.out(f)) {
                    const int d = compOf[e.to];
                    if (d == c) continue;
                    const std::uint64_t* other = &rows[d * words];
                    for (size_t w = 0; w < words; w++) row[w] |= other[w];
                }
            }
            int bits = 0;
            for (size_t w = 0; w < words; w++) bits += static_cast<int>(std::bitset<64>(row[w]).count());
            for (int f : sccs[c]) count[f] += bits;
        }
    }
    for (int& c : count) c--;  // the file itself
    return count;
}

}  // namespace

ReachCounts reachCounts(const Graph& graph, size_t memoryLimit) {
    const std::vector<Component> sccs = tarjanSCC(graph);
    std::vector<int> compOf(graph.size());
    for (size_t i = 0; i < sccs.size(); i++)
        for (int f : sccs[i]) compOf[f] = static_cast<int>(i);
    return {countReach(graph, sccs, compOf, false, memoryLimit), countReach(graph, sccs, compOf, true, memoryLimit)};
}

BuildOrder buildOrder(const Graph& graph, const std::vector<Component>& sccs) {
    const size_t units = sccs.size();
    BuildOrder result;
    result.compOf.assign(graph.size(), -1);
    for (size_t i = 0; i < units; i++)
        for (int f : sccs[i]) result.compOf[f] = static_cast<int>(i);

    std::vector<std::set<int>> deps(units);        // unit -> units it depends on
    std::vector<std::set<int>> dependents(units);  // unit -> units depending on it
    for (int a = 0; a < graph.size(); a++) {
        for (const Edge& e : graph.out(a)) {
            const int ca = result.compOf[a], cb = result.compOf[e.to];
            if (ca != cb) {
                deps[ca].insert(cb);
                dependents[cb].insert(ca);
            }
        }
    }

    std::vector<size_t> remaining(units);
    std::vector<int> layer(units, 0);
    auto less = [&](int x, int y) { return graph.name(sccs[x][0]) < graph.name(sccs[y][0]); };
    MinHeap<int, decltype(less)> heap(less);
    for (size_t i = 0; i < units; i++) {
        remaining[i] = deps[i].size();
        if (remaining[i] == 0) heap.push(static_cast<int>(i));
    }
    while (!heap.empty()) {
        const int c = heap.pop();
        result.order.push_back({sccs[c], sccs[c].size() > 1, layer[c]});
        for (int d : dependents[c]) {
            layer[d] = std::max(layer[d], layer[c] + 1);
            if (--remaining[d] == 0) heap.push(d);
        }
    }

    result.layerOf.assign(graph.size(), 0);
    for (size_t i = 0; i < units; i++)
        for (int f : sccs[i]) result.layerOf[f] = layer[i];
    return result;
}

}  // namespace pdr
