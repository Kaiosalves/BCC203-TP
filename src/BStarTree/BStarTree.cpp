#include "BStarTree.hpp"

#include <array>
#include <memory>
#include <optional>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "BStarTreeMemory.hpp"
#include "IBStarTreeData.hpp"

namespace Algorithm::BStarTree {

BStarTree::BStarTree(File& input)
    : input_(input), tree_(std::make_unique<BStarTreeMemory>(input)) {}

std::optional<IBStarTreeData::Entry> BStarTree::FindEntryInNode(
    const IBStarTreeData::Node& node, int key) {
    for (const auto& entry : node.data.entries) {
        if (entry.key == key) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<Item> BStarTree::Search(int key) {
    // Busca o nó que contém a entrada desejada
    std::optional<IBStarTreeData::Node> node = this->tree_->Search(key);
    if (node.has_value()) {
        std::optional<IBStarTreeData::Entry> entry =
            FindEntryInNode(*node, key);
        if (entry.has_value()) {
            std::array<Item, PAGE_SIZE> const page =
                input_.GetPageAt(entry->pageIndex);
            std::optional<Item> item = SearchInPage(page, key);
            if (item.has_value()) {
                return item;
            }
        }
    }
    return std::nullopt;
}

std::optional<Item> BStarTree::SearchInPage(
    const std::array<Item, PAGE_SIZE>& page, int key) {
    for (const auto& item : page) {
        if (item.key == key) {
            return item;
        }
    }
    return std::nullopt;
}

}  // namespace Algorithm::BStarTree
