#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "../Common.hpp"

namespace Algorithm::BTree {
class IBTreeData {
public:
    struct Entry {
        int key = 0;
        uint64_t pageIndex = 0;
    };

    struct Node {
        std::array<Entry, PAGE_SIZE> entries{};
        std::array<int64_t, PAGE_SIZE + 1> nodePos{};
        uint64_t size = 0;

        [[nodiscard]] bool isLeaf() const { return this->nodePos[0] == -1; }

        Node() { this->nodePos.fill(-1); }
    };

    IBTreeData() = default;
    virtual ~IBTreeData() = default;

    virtual std::optional<Node> Search(int key) = 0;
};
}  // namespace Algorithm::BTree
