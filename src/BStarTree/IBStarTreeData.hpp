#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "../Common.hpp"

namespace Algorithm::BStarTree {
class IBStarTreeData {
public:
    struct Entry {
        int key = 0;
        uint64_t pageIndex = 0;
    };

    struct Node {
        enum Type : uint8_t {
            Index,
            Data,
        };

        struct Index {
            std::array<int, PAGE_SIZE> indexes{};
            std::array<int64_t, PAGE_SIZE + 1> nodePos{};
        };

        struct Data {
            std::array<Entry, PAGE_SIZE> entries{};
            int64_t nextData = -1;
        };

        Type type;
        union {
            struct Index index;
            struct Data data;
        };
        uint64_t size = 0;

        [[nodiscard]] bool isLeaf() const { return this->type == Type::Data; }

        Node() : Node(Type::Data) {}

        explicit Node(Type type) : type(type) {
            if (type == Type::Index) {
                this->index.indexes.fill(0);
                this->index.nodePos.fill(-1);
            } else {
                this->data.entries.fill(Entry{});
                this->data.nextData = -1;
            }
        }
    };

    IBStarTreeData() = default;
    virtual ~IBStarTreeData() = default;

    virtual std::optional<Node> Search(int key) = 0;
};
}  // namespace Algorithm::BStarTree
