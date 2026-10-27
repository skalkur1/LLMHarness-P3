// src/tools/tool_registry.cpp
//
// Hash table with separate chaining. Each bucket is a singly linked list of
// Nodes; buckets_ holds the head of each list.

#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}

ToolRegistry::~ToolRegistry() = default;

// FNV-1a, 64-bit.
std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

// bucket_count is always a power of two, so & gives the same result as %.
std::size_t ToolRegistry::index_for(std::string_view key, std::size_t bucket_count) noexcept {
    return hash(key) & (bucket_count - 1);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) return false;

    std::string name = tool->name();
    if (find(name) != nullptr) return false;

    double load_after = static_cast<double>(size_ + 1) / buckets_.size();
    if (load_after > kMaxLoad) {
        rehash(buckets_.size() * 2);
    }

    auto node = std::make_unique<Node>();
    node->e.name = std::move(name);
    node->e.tool = std::move(tool);

    std::size_t i = index_for(node->e.name, buckets_.size());
    node->next = std::move(buckets_[i]);
    buckets_[i] = std::move(node);
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const Node* cur = buckets_[index_for(name, buckets_.size())].get();
    while (cur != nullptr) {
        if (cur->e.name == name) return cur->e.tool.get();
        cur = cur->next.get();
    }
    return nullptr;
}

// Unlinks every node from the old buckets and links it into its new bucket.
// Only unique_ptrs move; the Nodes and Tools stay where they are in memory.
void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<std::unique_ptr<Node>> fresh(new_bucket_count);

    for (std::unique_ptr<Node>& head : buckets_) {
        while (head != nullptr) {
            std::unique_ptr<Node> node = std::move(head);
            head = std::move(node->next);

            std::size_t i = index_for(node->e.name, new_bucket_count);
            node->next = std::move(fresh[i]);
            fresh[i] = std::move(node);
        }
    }
    buckets_ = std::move(fresh);
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const std::unique_ptr<Node>& head : buckets_) {
        const Node* cur = head.get();
        while (cur != nullptr) {
            fn(*cur->e.tool);
            cur = cur->next.get();
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / buckets_.size();
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const std::unique_ptr<Node>& head : buckets_) {
        std::size_t length = 0;
        const Node* cur = head.get();
        while (cur != nullptr) {
            ++length;
            cur = cur->next.get();
        }
        if (length > longest) longest = length;
    }
    return longest;
}
