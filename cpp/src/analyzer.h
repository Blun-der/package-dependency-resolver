// Package Dependency Resolver - full project analysis
//
// Pipeline:  files -> parse imports -> resolve to files -> graph
//            load-time graph -> Tarjan SCC (cycles) -> Kahn + min-heap (build order)
//            graph -> BFS (paths, impact) ; Trie (file search)
#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "graph.h"
#include "trie.h"
#include "util.h"

namespace pdr {

struct SourceFile {
    std::string path;  // relative to the project, "/" separated
    std::string content;
};

struct AnalyzeOptions {
    // Drop a folder name that every path starts with ("myproject/a.py" -> "a.py").
    bool stripRoot = false;
};

struct ImportRef {
    enum class Type { File, External, Broken };
    Type type;
    std::string target;
    int line;
    std::string text;
};

struct FileInfo {
    std::string path;
    Lang lang = Lang::None;
    int lines = 0;
    std::vector<ImportRef> imports;
    std::set<std::string> externals;
};

struct Step {  // one hop of a chain: `from` imports `to` on `line`
    std::string from, to;
    int line;
    std::string text;
};

struct Cycle {
    std::vector<std::string> files;  // every file in the loop group, sorted
    std::vector<std::string> loop;   // one concrete loop: a -> b -> ... -> a
    std::vector<Step> steps;
    std::vector<Step> fix;           // imports to remove so no loop is left
    bool fixTooLarge = false;        // too many links to search for a fix
};

struct OrderUnit {
    std::vector<std::string> files;
    bool cyclic;
    int layer;
};

struct Metrics {
    int fanOut = 0;        // files it imports directly
    int fanIn = 0;         // files importing it directly
    int dependents = 0;    // files that break if it changes (direct or indirect)
    int dependencies = 0;  // files it needs (direct or indirect)
    int layer = 0;
    bool inCycle = false;
};

struct ExternalPackage {
    std::string name;
    std::vector<std::string> files;
    bool builtin;
    Lang lang;
};

struct BrokenImport {
    std::string file, spec;
    int line;
    std::string text;
};

struct Stats {
    int files = 0, edges = 0, importStatements = 0, cycles = 0, filesInCycles = 0;
    int externals = 0, thirdParty = 0, broken = 0, layers = 0;
    int pythonFiles = 0, jsFiles = 0, cppFiles = 0;
    int deferredImports = 0, typeCheckingImports = 0;  // left out of cycle search
    double ms = 0;
};

struct Result {
    std::string rootName;
    Graph graph;      // every import
    Graph loadGraph;  // only imports that run while modules load (same node ids);
                      // cycles and build order come from this one
    std::vector<FileInfo> fileInfo;  // indexed by graph node id
    std::vector<Metrics> metrics;    // indexed by graph node id
    std::vector<Cycle> cycles;
    std::vector<OrderUnit> order;
    std::vector<std::string> critical;     // most dependents first
    std::vector<std::string> entryPoints;  // files nobody imports
    std::vector<ExternalPackage> externals;
    std::vector<BrokenImport> broken;
    Trie trie;
    Stats stats;

    const Metrics& metricsOf(const std::string& file) const { return metrics[graph.id(file)]; }
    const FileInfo& infoOf(const std::string& file) const { return fileInfo[graph.id(file)]; }
};

struct PathExplanation {
    std::vector<std::string> path;
    std::vector<Step> steps;
};

// Reads every .py / .js / .ts / .c / .h / .cpp ... file under `folder`, skipping ignored folders.
std::vector<SourceFile> loadProject(const std::string& folder);

Result analyze(std::vector<SourceFile> files, const AnalyzeOptions& options = {});

// Why does `from` depend on `to`? Shortest chain, with the import line of each hop.
std::optional<PathExplanation> explainPath(const Result& result, const std::string& from, const std::string& to);

// Every file that is affected when `file` changes, sorted.
std::vector<std::string> impactOf(const Result& result, const std::string& file);

}  // namespace pdr
