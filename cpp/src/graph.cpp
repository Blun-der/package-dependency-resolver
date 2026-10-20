#include "graph.h"

#include <algorithm>

namespace pdr {

int Graph::addNode(const std::string& name) {
    if (const int* existing = ids_.find(name)) return *existing;
    const int id = size();
    ids_.insert(name, id);
    names_.push_back(name);
    adj_.emplace_back();
    radj_.emplace_back();
    return id;
}

void Graph::addEdge(const std::string& from, const std::string& to, const Evidence& evidence) {
    const int a = addNode(from);
    const int b = addNode(to);
    addEdge(a, b, evidence);
}

void Graph::addEdge(int from, int to, const Evidence& evidence) {
    if (edge(from, to)) return;
    insertSorted(adj_[from], to, evidence);
    insertSorted(radj_[to], from, evidence);
    edges_++;
}

int Graph::id(const std::string& name) const {
    const int* found = ids_.find(name);
    return found ? *found : -1;
}

const Evidence* Graph::edge(int from, int to) const {
    const std::vector<Edge>& list = adj_[from];
    // binary search: the list is sorted by the name of the target
    auto it = std::lower_bound(list.begin(), list.end(), names_[to],
                               [this](const Edge& e, const std::string& n) { return names_[e.to] < n; });
    return it != list.end() && it->to == to ? &it->evidence : nullptr;
}

void Graph::insertSorted(std::vector<Edge>& list, int to, const Evidence& evidence) {
    auto it = std::lower_bound(list.begin(), list.end(), names_[to],
                               [this](const Edge& e, const std::string& n) { return names_[e.to] < n; });
    list.insert(it, Edge{to, evidence});
}

std::vector<int> Graph::nodesByName() const {
    std::vector<int> ids(names_.size());
    for (int i = 0; i < size(); i++) ids[i] = i;
    std::sort(ids.begin(), ids.end(), [this](int a, int b) { return names_[a] < names_[b]; });
    return ids;
}

std::vector<std::string> Graph::names(const std::vector<int>& ids) const {
    std::vector<std::string> out;
    out.reserve(ids.size());
    for (int id : ids) out.push_back(names_[id]);
    return out;
}

std::vector<std::string> Graph::outNames(const std::string& name) const {
    std::vector<std::string> out;
    const int a = id(name);
    if (a < 0) return out;
    for (const Edge& e : adj_[a]) out.push_back(names_[e.to]);
    return out;
}

}  // namespace pdr
