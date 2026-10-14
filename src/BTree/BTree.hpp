#pragma once

#include <array>
#include <memory>
#include <optional>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "BTreeFile.hpp"
#include "IBTreeData.hpp"

namespace Algorithm::BTree {
class BTree {
public:
    explicit BTree(File& input, bool useDisk = true);

    ~BTree() = default;

    std::optional<Item> Search(int key);

private:
    File& input_;
    std::unique_ptr<IBTreeData> tree_;

    static std::optional<IBTreeData::Entry> FindEntryInNode(
        const IBTreeData::Node& node, int key);

    static std::optional<Item> SearchInPage(
        const std::array<Item, PAGE_SIZE>& page, int key,
        size_t itemCount = PAGE_SIZE);
};
}  // namespace Algorithm::BTree
