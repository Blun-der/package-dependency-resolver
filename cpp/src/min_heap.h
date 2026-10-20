// Package Dependency Resolver - binary min-heap
//
// Used by Kahn's algorithm to always pick the alphabetically first ready
// unit, so the build order is stable. push and pop are O(log n).
#pragma once

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pdr {

template <typename T, typename Less = std::less<T>>
class MinHeap {
public:
    explicit MinHeap(Less less = Less()) : less_(less) {}

    bool empty() const { return a_.empty(); }
    size_t size() const { return a_.size(); }

    void push(T x) {
        a_.push_back(std::move(x));
        size_t i = a_.size() - 1;
        while (i > 0) {  // sift up
            const size_t parent = (i - 1) / 2;
            if (!less_(a_[i], a_[parent])) break;
            std::swap(a_[i], a_[parent]);
            i = parent;
        }
    }

    T pop() {
        if (a_.empty()) throw std::out_of_range("pop from an empty heap");
        T top = std::move(a_[0]);
        a_[0] = std::move(a_.back());
        a_.pop_back();
        size_t i = 0;
        for (;;) {  // sift down
            const size_t l = 2 * i + 1, r = l + 1;
            size_t m = i;
            if (l < a_.size() && less_(a_[l], a_[m])) m = l;
            if (r < a_.size() && less_(a_[r], a_[m])) m = r;
            if (m == i) break;
            std::swap(a_[i], a_[m]);
            i = m;
        }
        return top;
    }

private:
    std::vector<T> a_;  // a_[0] is the smallest; children of i are 2i+1 and 2i+2
    Less less_;
};

}  // namespace pdr
