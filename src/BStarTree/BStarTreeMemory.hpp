#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "IBStarTreeData.hpp"

namespace Algorithm::BStarTree {
class BStarTreeMemory : public IBStarTreeData {
public:
    explicit BStarTreeMemory(File& input);

    BStarTreeMemory();

    ~BStarTreeMemory() override = default;

    std::optional<Node> Search(int key) override;

private:
    std::vector<Node> nodes_;
    int64_t rootIndex_ = -1;

    uint64_t AppendNode(const Node& node);

    static size_t FindKeyIndex(const Node& node, int key, bool& found);

    static void InsertIntoNonFullDataNode(Node& node, const Entry& entry);

    static void InsertIntoNonFullIndexNode(Node& node, int key,
                                           int64_t rightChild);

    void SplitDataNode(uint64_t nodeIndex, const Entry& entry,
                       Entry& promotedEntry, int64_t& createdSiblingIndex);

    void SplitIndexNode(uint64_t nodeIndex, int key, int64_t rightChildIndex,
                        Entry& promotedEntry, int64_t& createdSiblingIndex);

    bool InsertInternal(int64_t currentNodeIndex, const Entry& entryToInsert,
                        Entry& promotedEntry, int64_t& newChildIndex);

    void InsertItem(const Item& item, uint64_t pageIndex);

    void InsertPage(const std::array<Item, PAGE_SIZE>& page, uint64_t pageIndex,
                    size_t itemCount);

    void PopulateTree(File& input);
};
}  // namespace Algorithm::BStarTree
