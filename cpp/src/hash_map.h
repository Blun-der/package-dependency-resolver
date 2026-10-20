// Package Dependency Resolver - hash table with separate chaining
//
// Every bucket is a singly linked list of the entries whose hash lands there.
// The table doubles when it is more than 75% full, so chains stay short and
// insert / find / erase are O(1) on average (O(n) only if every key collides).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace pdr {

// FNV-1a for strings; the splitmix64 finaliser for integers.
struct Hasher {
    size_t operator()(const std::string& s) const {
        std::uint64_t h = 14695981039346656037ull;
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ull;
        }
        return static_cast<size_t>(h);
    }
    size_t operator()(std::uint64_t x) const {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return static_cast<size_t>(x ^ (x >> 31));
    }
};

template <typename K, typename V, typename Hash = Hasher>
class HashMap {
    struct Node {
        K key;
        V value;
        Node* next;
    };

public:
    explicit HashMap(Hash hash = Hash()) : buckets_(16, nullptr), hash_(hash) {}
    ~HashMap() { clear(); }

    HashMap(const HashMap& other) : buckets_(other.buckets_.size(), nullptr), hash_(other.hash_) {
        other.forEach([this](const K& k, const V& v) { insert(k, v); });
    }
    HashMap(HashMap&& other) noexcept
        : buckets_(std::move(other.buckets_)), size_(other.size_), hash_(other.hash_) {
        other.buckets_.assign(16, nullptr);
        other.size_ = 0;
    }
    HashMap& operator=(HashMap other) noexcept {  // copy-and-swap
        std::swap(buckets_, other.buckets_);
        std::swap(size_, other.size_);
        std::swap(hash_, other.hash_);
        return *this;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // Adds key -> value if key is new. Returns false (and keeps the old value) if it was there.
    bool insert(const K& key, const V& value) {
        if (find(key)) return false;
        if (size_ + 1 > buckets_.size() * 3 / 4) grow();
        Node*& head = buckets_[indexOf(key)];
        head = new Node{key, value, head};  // push at the front of the chain
        size_++;
        return true;
    }

    // Adds or overwrites.
    void set(const K& key, const V& value) {
        if (V* v = find(key)) *v = value;
        else insert(key, value);
    }

    V* find(const K& key) {
        for (Node* n = buckets_[indexOf(key)]; n; n = n->next)
            if (n->key == key) return &n->value;
        return nullptr;
    }
    const V* find(const K& key) const { return const_cast<HashMap*>(this)->find(key); }
    bool contains(const K& key) const { return find(key) != nullptr; }

    bool erase(const K& key) {
        for (Node** link = &buckets_[indexOf(key)]; *link; link = &(*link)->next) {
            if ((*link)->key == key) {
                Node* dead = *link;
                *link = dead->next;  // unlink from the chain
                delete dead;
                size_--;
                return true;
            }
        }
        return false;
    }

    void clear() {
        for (Node*& head : buckets_) {
            while (head) {
                Node* next = head->next;
                delete head;
                head = next;
            }
        }
        size_ = 0;
    }

    template <typename F>
    void forEach(F f) const {
        for (Node* head : buckets_)
            for (Node* n = head; n; n = n->next) f(n->key, n->value);
    }

    // For measuring the table in the report.
    size_t bucketCount() const { return buckets_.size(); }
    size_t longestChain() const {
        size_t longest = 0;
        for (Node* head : buckets_) {
            size_t len = 0;
            for (Node* n = head; n; n = n->next) len++;
            if (len > longest) longest = len;
        }
        return longest;
    }

private:
    // The bucket count is a power of two, so "hash mod buckets" is a bit mask.
    size_t indexOf(const K& key) const { return hash_(key) & (buckets_.size() - 1); }

    // Double the buckets and relink every node; no entry is copied.
    void grow() {
        std::vector<Node*> old(buckets_.size() * 2, nullptr);
        std::swap(old, buckets_);
        for (Node* head : old) {
            while (head) {
                Node* next = head->next;
                Node*& slot = buckets_[indexOf(head->key)];
                head->next = slot;
                slot = head;
                head = next;
            }
        }
    }

    std::vector<Node*> buckets_;
    size_t size_ = 0;
    Hash hash_;
};

template <typename K, typename Hash = Hasher>
class HashSet {
public:
    bool insert(const K& key) { return map_.insert(key, true); }  // false if it was already there
    bool contains(const K& key) const { return map_.contains(key); }
    bool erase(const K& key) { return map_.erase(key); }
    size_t size() const { return map_.size(); }
    template <typename F>
    void forEach(F f) const {
        map_.forEach([&f](const K& k, bool) { f(k); });
    }

private:
    HashMap<K, bool, Hash> map_;
};

}  // namespace pdr
