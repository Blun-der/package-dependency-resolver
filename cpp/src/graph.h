// Package Dependency Resolver - directed graph
//
// Files are nodes, imports are edges: an edge A -> B means "A imports B".
// Stored as adjacency lists (sparse graph: O(V + E) space instead of the
// O(V^2) of a matrix), plus a reverse adjacency list so "who imports B?"
// is as cheap as "what does A import?".
//
// Every adjacency list is kept sorted by file name, so all algorithms that
// walk the graph give the same, alphabetical answer on every run.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hash_map.h"

namespace pdr {

// Where an import link comes from in the source code.
struct Evidence {
    int line = 0;
    std::string text;
};

struct Edge {
    int to;             // the other end (the importer, in the reverse list)
    Evidence evidence;
};

// One number per link, for hash sets of links: (from, to) -> 64 bits.
inline std::uint64_t linkKey(int from, int to) {
    return (static_cast<std::uint64_t>(from) << 32) | static_cast<std::uint32_t>(to);
}

class Graph {
public:
    int addNode(const std::string& name);  // returns the id (existing or new)
    // Adds A -> B once; a second import of the same file keeps the first evidence.
    void addEdge(const std::string& from, const std::string& to, const Evidence& evidence);
    void addEdge(int from, int to, const Evidence& evidence);

    int size() const { return static_cast<int>(names_.size()); }
    int edgeCount() const { return edges_; }
    int id(const std::string& name) const;  // -1 if there is no such node
    bool has(const std::string& name) const { return id(name) >= 0; }
    const std::string& name(int id) const { return names_[id]; }

    const std::vector<Edge>& out(int id) const { return adj_[id]; }   // files `id` imports
    const std::vector<Edge>& in(int id) const { return radj_[id]; }   // files importing `id`
    const Evidence* edge(int from, int to) const;  // nullptr if there is no such edge

    std::vector<int> nodesByName() const;
    std::vector<std::string> names(const std::vector<int>& ids) const;
    std::vector<std::string> outNames(const std::string& name) const;

private:
    void insertSorted(std::vector<Edge>& list, int to, const Evidence& evidence);

    std::vector<std::string> names_;
    HashMap<std::string, int> ids_;  // name -> id in O(1) on average
    std::vector<std::vector<Edge>> adj_;
    std::vector<std::vector<Edge>> radj_;
    int edges_ = 0;
};

}  // namespace pdr
