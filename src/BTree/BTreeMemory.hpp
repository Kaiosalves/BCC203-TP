#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "IBTreeData.hpp"

namespace Algorithm::BTree {
class BTreeMemory : public IBTreeData {
public:
    explicit BTreeMemory(File& input);

    BTreeMemory();

    ~BTreeMemory() override = default;

    std::optional<Node> Search(int key) override;

private:
    std::vector<Node> nodes_;
    int64_t rootIndex_ = -1;

    uint64_t AppendNode(const Node& node);

    static size_t FindKeyIndex(const Node& node, int key, bool& found);

    static void InsertIntoNonFullNode(Node& node, const Entry& entry,
                                      int64_t rightChild);

    void SplitNode(uint64_t nodeIndex, const Entry& entry,
                   int64_t rightChildIndex, Entry& promotedEntry,
                   int64_t& createdSiblingIndex);

    bool InsertInternal(int64_t currentNodeIndex, const Entry& entryToInsert,
                        Entry& promotedEntry, int64_t& newChildIndex);

    void InsertItem(const Item& item, uint64_t pageIndex);

    void InsertPage(const std::array<Item, PAGE_SIZE>& page, uint64_t pageIndex,
                    size_t itemCount);

    void PopulateTree(File& input);
};
}  // namespace Algorithm::BTree
