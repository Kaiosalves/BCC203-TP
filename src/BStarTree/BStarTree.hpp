#pragma once

#include <array>
#include <memory>
#include <optional>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "BStarTreeMemory.hpp"
#include "IBStarTreeData.hpp"

namespace Algorithm::BStarTree {
class BStarTree {
public:
    explicit BStarTree(File& input);

    ~BStarTree() = default;

    std::optional<Item> Search(int key);

private:
    File& input_;
    std::unique_ptr<IBStarTreeData> tree_;

    static std::optional<IBStarTreeData::Entry> FindEntryInNode(
        const IBStarTreeData::Node& node, int key);

    static std::optional<Item> SearchInPage(
        const std::array<Item, PAGE_SIZE>& page, int key,
        size_t itemCount = PAGE_SIZE);
};
}  // namespace Algorithm::BStarTree
