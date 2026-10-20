// Package Dependency Resolver - architecture rules (pdr.rules)
//
//   # comments start with #; paths are folders or files of the project
//   layers  fad/main  fad/daemon  fad/scheduler|fad/watcher  fad/utils   # top layer first
//   forbid  fad/utils -> fad/actions
//   entry   scripts                                        # more start points for --unused
//
// layers:  a file may import its own layer and the layers below it, never one
//          above it ("fad/utils" must not import "fad/daemon"). Paths joined
//          with | share a layer.
// forbid:  no file under the first path may import a file under the second.
// entry:   files under this path count as start points when looking for
//          unused files (for example the modules a library offers its users).
#pragma once

#include <string>
#include <vector>

#include "graph.h"

namespace pdr {

struct Rules {
    struct Forbid {
        std::string from, to;
        int line;
    };
    struct Layers {
        std::vector<std::vector<std::string>> levels;  // top layer first; each holds one or more paths
        int line;
    };
    std::string source = "pdr.rules";  // file name used in messages
    std::vector<Forbid> forbids;
    std::vector<Layers> layers;
    std::vector<std::string> entries;
    std::vector<int> entryLines;

    bool empty() const { return forbids.empty() && layers.empty() && entries.empty(); }
};

// Throws std::runtime_error("pdr.rules:3: ...") on a line it does not understand.
Rules parseRules(const std::string& text, const std::string& source);

struct RuleViolation {
    std::string file;    // the importing file
    int line;            // of the import
    std::string text;    // the import as written
    std::string target;  // the imported file
    std::string rule;    // "fad/utils must not import fad/actions"
    int ruleLine;        // line of the rule in pdr.rules
};

struct RuleCheck {
    std::string source;  // the rules file, for "pdr.rules:7" in messages
    std::vector<RuleViolation> violations;
    std::vector<std::string> warnings;  // "pdr.rules:4: warning: 'fad/utilz' matches no file"
};

// Checks every import link against the layers and forbid rules. O(E * matches
// per file) after one walk of the path trie per file.
RuleCheck checkRules(const Graph& graph, const Rules& rules);

}  // namespace pdr
