#include "trie.h"

#include "util.h"

namespace pdr {

Trie::Trie() : nodes_(1) {}

void Trie::insert(const std::string& key, const std::string& value) {
    // The same value is usually inserted twice in a row (file name, then full
    // path), so reuse its slot instead of storing it again.
    if (values_.empty() || values_.back() != value) values_.push_back(value);
    const int v = static_cast<int>(values_.size()) - 1;

    int node = 0;
    for (char ch : toLower(key)) {
        auto it = nodes_[node].kids.find(ch);
        int next;
        if (it == nodes_[node].kids.end()) {
            next = static_cast<int>(nodes_.size());
            nodes_[node].kids.emplace(ch, next);
            nodes_.emplace_back();  // may move nodes_, so look nodes up by index only
        } else {
            next = it->second;
        }
        node = next;
        std::vector<int>& ids = nodes_[node].values;
        if (ids.empty() || ids.back() != v) ids.push_back(v);
    }
}

std::vector<std::string> Trie::search(const std::string& prefix, size_t limit) const {
    int node = 0;
    for (char ch : toLower(prefix)) {
        auto it = nodes_[node].kids.find(ch);
        if (it == nodes_[node].kids.end()) return {};
        node = it->second;
    }
    std::vector<std::string> out;
    for (int v : nodes_[node].values) {
        if (out.size() >= limit) break;
        out.push_back(values_[v]);
    }
    return out;
}

}  // namespace pdr
