// Package Dependency Resolver - trie over path segments
//
// Stores folder and file patterns ("fad/utils", "src/ui/App.tsx") the way a
// file system stores folders: one node per name. Walking a file's path down
// the trie finds every stored pattern that contains the file, shallowest
// first, in O(depth of the path) hash map lookups, however many patterns
// there are. Used for the architecture rules and the extra entry points.
#pragma once

#include <string>
#include <vector>

#include "hash_map.h"
#include "util.h"

namespace pdr {

class PathTrie {
public:
    PathTrie() : nodes_(1) {}

    // Stores `id` at the pattern's node. "fad/utils/" and "fad/utils" are the same pattern.
    void add(const std::string& pattern, int id) {
        int node = 0;
        for (const std::string& part : split(pattern, '/')) {
            if (part.empty() || part == ".") continue;
            if (const int* next = nodes_[node].kids.find(part)) {
                node = *next;
            } else {
                const int created = static_cast<int>(nodes_.size());
                nodes_[node].kids.insert(part, created);
                nodes_.emplace_back();  // may move nodes_: only indexes are kept
                node = created;
            }
        }
        nodes_[node].ids.push_back(id);
    }

    // Every id whose pattern contains `path`, shallowest pattern first. The
    // last part also matches without its extension, so "fad/utils" contains
    // the module file "fad/utils.py" as well as the folder "fad/utils/".
    std::vector<int> matches(const std::string& path) const {
        std::vector<int> out(nodes_[0].ids);
        const std::vector<std::string> parts = split(path, '/');
        int node = 0;
        for (size_t i = 0; i < parts.size(); i++) {
            const int* next = nodes_[node].kids.find(parts[i]);
            if (!next && i + 1 == parts.size()) {
                const size_t dot = parts[i].rfind('.');
                if (dot != std::string::npos && dot > 0) next = nodes_[node].kids.find(parts[i].substr(0, dot));
            }
            if (!next) break;
            node = *next;
            out.insert(out.end(), nodes_[node].ids.begin(), nodes_[node].ids.end());
        }
        return out;
    }

private:
    struct Node {
        HashMap<std::string, int> kids;  // folder or file name -> index in nodes_
        std::vector<int> ids;
    };
    std::vector<Node> nodes_;  // nodes_[0] is the project folder itself
};

}  // namespace pdr
