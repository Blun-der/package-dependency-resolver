// Package Dependency Resolver - import resolution
//
// Turns an import ("app.core", "./App", "react", "graph.h") into a file of the project,
// an outside package, or a broken import. All lookups are hash map / hash set
// lookups, O(1) on average.
#pragma once

#include <string>
#include <vector>

#include "hash_map.h"
#include "parser.h"

namespace pdr {

struct ResolveContext {
    HashSet<std::string> fileSet;                // every project file
    HashMap<std::string, std::string> pyIndex;   // dotted module name -> file
    // C/C++: every path ending -> the files with that ending, so that
    // "util/str.h" finds src/util/str.h and include/util/str.h.
    HashMap<std::string, std::vector<std::string>> cSuffixes;
};

struct Resolution {
    enum class Kind { File, External, Broken };
    Kind kind;
    std::string value;  // file path, package name, or the import as written
};

ResolveContext makeContext(const std::vector<std::string>& sortedPaths);
std::vector<Resolution> resolvePython(const Import& imp, const std::string& fromFile, const ResolveContext& ctx);
std::vector<Resolution> resolveJs(const Import& imp, const std::string& fromFile, const ResolveContext& ctx);
std::vector<Resolution> resolveCpp(const Import& imp, const std::string& fromFile, const ResolveContext& ctx);

}  // namespace pdr
