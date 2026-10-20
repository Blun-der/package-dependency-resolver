// Package Dependency Resolver - benchmark
//
//   make bench      -> build/bench.csv and the charts in build/
//
// Measures how the running time grows with the size of the project, to check
// the complexity claims of the report against real numbers:
//
//   graph      Tarjan's SCC, Kahn's build order, one BFS     expected O(V + E)
//   hashmap    our HashMap against std::unordered_map        expected O(1) per operation
//   ranking    critical files: one BFS per file vs bitsets   O(V * (V + E)) vs O(V / 64 * (V + E))
//   analyze    the whole pipeline on generated Python files
//
// The projects are generated with a fixed seed, so every run measures the same
// graphs. Each time is the median of several runs.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "../src/algorithms.h"
#include "../src/analyzer.h"
#include "../src/hash_map.h"

using namespace pdr;

namespace {

// Median wall time of `runs` calls, in milliseconds.
double medianMs(int runs, const std::function<void()>& fn) {
    std::vector<double> times;
    for (int i = 0; i < runs; i++) {
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

// A project shaped like real code: packages of 10 files. A file imports 0-2
// earlier files of its own package and 1-3 files of earlier packages. In one
// package out of 20, the first two files import each other (a circular import).
struct Link {
    int from, to;
};

std::vector<Link> generateLinks(int files, std::mt19937& rng) {
    const int perPackage = 10;
    std::vector<Link> links;
    for (int f = 0; f < files; f++) {
        const int pkg = f / perPackage, pos = f % perPackage;
        const int local = std::min(pos, static_cast<int>(rng() % 3));
        for (int k = 0; k < local; k++) links.push_back({f, pkg * perPackage + static_cast<int>(rng() % pos)});
        if (pkg > 0) {
            const int outside = 1 + static_cast<int>(rng() % 3);
            for (int k = 0; k < outside; k++) links.push_back({f, static_cast<int>(rng() % (pkg * perPackage))});
        }
        if (pkg % 20 == 0 && pos == 1) links.push_back({f - 1, f});  // file 0 -> file 1 -> file 0
        if (pkg % 20 == 0 && pos == 1) links.push_back({f, f - 1});
    }
    return links;
}

std::string moduleName(int f) { return "p" + std::to_string(f / 10) + "/m" + std::to_string(f % 10); }

Graph buildGraph(int files, const std::vector<Link>& links) {
    Graph g;
    for (int f = 0; f < files; f++) g.addNode(moduleName(f) + ".py");
    for (const Link& l : links) g.addEdge(l.from, l.to, {1, ""});
    return g;
}

// The same shape as Python source files: "from p3 import m7" per link.
std::vector<SourceFile> generateSources(int files, const std::vector<Link>& links) {
    std::vector<SourceFile> out;
    for (int f = 0; f < files; f++) {
        if (f % 10 == 0) out.push_back({"p" + std::to_string(f / 10) + "/__init__.py", ""});
        out.push_back({moduleName(f) + ".py", ""});
    }
    for (const Link& l : links) {
        const std::string to = moduleName(l.to);
        const size_t slash = to.find('/');
        // files are in creation order: 1 __init__.py per 10 modules, before them
        out[l.from + l.from / 10 + 1].content += "from " + to.substr(0, slash) + " import " + to.substr(slash + 1) + "\n";
    }
    return out;
}

void benchGraph(std::FILE* csv) {
    std::cerr << "\n  Graph algorithms (median of 5 runs)\n"
              << "        files      links   tarjan ms    kahn ms     bfs ms\n";
    for (int files : {10000, 25000, 50000, 100000, 200000, 400000, 600000, 800000, 1000000}) {
        std::mt19937 rng(42);
        const std::vector<Link> links = generateLinks(files, rng);
        const Graph g = buildGraph(files, links);
        std::vector<Component> sccs;
        const double tarjan = medianMs(5, [&] { sccs = tarjanSCC(g); });
        const double kahn = medianMs(5, [&] { buildOrder(g, sccs); });
        // BFS over the importers of the very first file reaches most of the project
        const double bfs = medianMs(5, [&] { reachable(g, 0, true); });
        std::fprintf(csv, "graph,%d,%d,%.3f,%.3f,%.3f\n", files, g.edgeCount(), tarjan, kahn, bfs);
        std::fprintf(stderr, "  %11d %10d %11.2f %10.2f %10.2f\n", files, g.edgeCount(), tarjan, kahn, bfs);
    }
}

void benchHashMap(std::FILE* csv) {
    std::cerr << "\n  Hash table: insert n keys, then look up all n and n missing ones (median of 5 runs)\n"
              << "            n   ours insert   std insert   ours find    std find\n";
    for (int n : {10000, 50000, 100000, 250000, 500000, 1000000, 2000000}) {
        std::vector<std::string> keys, missing;
        for (int i = 0; i < n; i++) {
            keys.push_back("src/pkg" + std::to_string(i % 997) + "/module_" + std::to_string(i) + ".py");
            missing.push_back("lib/other_" + std::to_string(i) + ".py");
        }
        long sink = 0;  // keeps the compiler from removing the lookups
        HashMap<std::string, int> ours;
        std::unordered_map<std::string, int> stl;
        const double oursInsert = medianMs(5, [&] {
            ours = HashMap<std::string, int>();
            for (int i = 0; i < n; i++) ours.insert(keys[i], i);
        });
        const double stdInsert = medianMs(5, [&] {
            stl = std::unordered_map<std::string, int>();
            for (int i = 0; i < n; i++) stl.emplace(keys[i], i);
        });
        const double oursFind = medianMs(5, [&] {
            for (int i = 0; i < n; i++) sink += ours.find(keys[i]) ? 1 : 0;
            for (int i = 0; i < n; i++) sink += ours.find(missing[i]) ? 1 : 0;
        });
        const double stdFind = medianMs(5, [&] {
            for (int i = 0; i < n; i++) sink += stl.count(keys[i]);
            for (int i = 0; i < n; i++) sink += stl.count(missing[i]);
        });
        if (sink < 0) std::cerr << sink;
        std::fprintf(csv, "hashmap,%d,%.3f,%.3f,%.3f,%.3f\n", n, oursInsert, stdInsert, oursFind, stdFind);
        std::fprintf(stderr, "  %11d %13.2f %12.2f %11.2f %11.2f\n", n, oursInsert, stdInsert, oursFind, stdFind);
    }
}

void benchRanking(std::FILE* csv) {
    std::cerr << "\n  Critical-files ranking: one BFS each way per file vs bitsets (median of 3 runs)\n"
              << "        files      links      bfs ms   bitset ms\n";
    for (int files : {1000, 2500, 5000, 10000, 20000, 40000, 60000, 80000, 100000}) {
        std::mt19937 rng(42);
        const Graph g = buildGraph(files, generateLinks(files, rng));
        double bfs = -1;  // too slow past 20,000 files to be worth waiting for
        if (files <= 20000) {
            bfs = medianMs(3, [&] {
                for (int v = 0; v < files; v++) {
                    reachable(g, v, true);
                    reachable(g, v, false);
                }
            });
        }
        const double bits = medianMs(3, [&] { reachCounts(g); });
        std::fprintf(csv, "ranking,%d,%d,%.3f,%.3f\n", files, g.edgeCount(), bfs, bits);
        std::fprintf(stderr, "  %11d %10d %11.1f %11.1f\n", files, g.edgeCount(), bfs, bits);
    }
}

void benchAnalyze(std::FILE* csv) {
    std::cerr << "\n  Whole analysis of generated Python projects (median of 3 runs)\n"
              << "        files      links    total ms\n";
    for (int files : {1000, 5000, 10000, 25000, 50000, 75000, 100000}) {
        std::mt19937 rng(42);
        const std::vector<Link> links = generateLinks(files, rng);
        const std::vector<SourceFile> sources = generateSources(files, links);
        Result r;
        const double total = medianMs(3, [&] { r = analyze(sources); });
        std::fprintf(csv, "analyze,%d,%d,%.3f\n", r.stats.files, r.stats.edges, total);
        std::fprintf(stderr, "  %11d %10d %11.1f\n", r.stats.files, r.stats.edges, total);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::FILE* csv = argc > 1 ? std::fopen(argv[1], "w") : stdout;
    if (!csv) {
        std::cerr << "bench: cannot write " << argv[1] << "\n";
        return 2;
    }
    std::fprintf(csv, "# graph,files,links,tarjan_ms,kahn_ms,bfs_ms\n"
                      "# hashmap,n,ours_insert_ms,std_insert_ms,ours_find_ms,std_find_ms\n"
                      "# ranking,files,links,bfs_ms (-1 = not run),bitset_ms\n"
                      "# analyze,files,links,total_ms\n");
    benchGraph(csv);
    benchHashMap(csv);
    benchRanking(csv);
    benchAnalyze(csv);
    if (csv != stdout) std::fclose(csv);
    return 0;
}
