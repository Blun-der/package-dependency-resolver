// Package Dependency Resolver - trie for file search
//
// Keys are lower-cased file names and paths. Every node remembers the values
// stored below it, so a prefix search costs O(length of the prefix).
#pragma once

#include <map>
#include <string>
#include <vector>

namespace pdr {

class Trie {
public:
    Trie();
    void insert(const std::string& key, const std::string& value);
    // Values whose key starts with `prefix`, in the order they were inserted.
    std::vector<std::string> search(const std::string& prefix, size_t limit = 20) const;

private:
    struct Node {
        std::map<char, int> kids;  // character -> index in nodes_
        std::vector<int> values;   // indexes in values_
    };
    std::vector<Node> nodes_;  // nodes_[0] is the root
    std::vector<std::string> values_;
};

}  // namespace pdr
