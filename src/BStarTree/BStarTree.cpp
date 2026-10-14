#include "BStarTree.hpp"

#include <algorithm>
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
    : input_(input), tree_(std::make_unique<BStarTreeMemory>(input)) {
    Log::Info("BStarTree: initialized for file " + input_.path());
}

std::optional<IBStarTreeData::Entry> BStarTree::FindEntryInNode(
    const IBStarTreeData::Node& node, int key) {
    for (size_t i = 0; i < node.size; ++i) {
        Metrics::RecordKeyComparison();
        if (node.data.entries[i].key == key) {
            return node.data.entries[i];
        }
    }
    return std::nullopt;
}

std::optional<Item> BStarTree::Search(int key) {
    Log::Info("BStarTree: starting search for key " + std::to_string(key));
    // Busca o nó que contém a entrada desejada
    std::optional<IBStarTreeData::Node> const node = this->tree_->Search(key);
    if (!node.has_value()) {
        Log::Info("BStarTree: key " + std::to_string(key) +
                  " not found in B* tree index");
        return std::nullopt;
    }

    std::optional<IBStarTreeData::Entry> const entry =
        FindEntryInNode(*node, key);
    if (!entry.has_value()) {
        Log::Info("BStarTree: key " + std::to_string(key) +
                  " not found in leaf node entries");
        return std::nullopt;
    }

    Log::Info("BStarTree: key " + std::to_string(key) +
              " found in B* tree index at page " +
              std::to_string(entry->pageIndex) + ", fetching page from file");
    std::array<Item, PAGE_SIZE> const page = input_.GetPageAt(entry->pageIndex);

    uint64_t const firstItem = entry->pageIndex * PAGE_SIZE;
    uint64_t const remaining =
        input_.quantity() > firstItem ? input_.quantity() - firstItem : 0;
    size_t const itemCount =
        std::min(static_cast<uint64_t>(PAGE_SIZE), remaining);

    std::optional<Item> const item = SearchInPage(page, key, itemCount);
    if (item.has_value()) {
        Log::Info("BStarTree: key " + std::to_string(key) +
                  " confirmed in page " + std::to_string(entry->pageIndex));
        return item;
    }

    Log::Error("BStarTree: key " + std::to_string(key) +
               " found in index pointing to page " +
               std::to_string(entry->pageIndex) +
               ", but item is not present in data page");
    return std::nullopt;
}

std::optional<Item> BStarTree::SearchInPage(
    const std::array<Item, PAGE_SIZE>& page, int key, size_t itemCount) {
    for (size_t i = 0; i < itemCount; ++i) {
        Metrics::RecordKeyComparison();
        if (page[i].key == key) {
            return page[i];
        }
    }
    return std::nullopt;
}

}  // namespace Algorithm::BStarTree
