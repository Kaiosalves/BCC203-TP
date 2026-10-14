#include "BTree.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "BTreeFile.hpp"
#include "BTreeMemory.hpp"
#include "IBTreeData.hpp"

namespace Algorithm::BTree {

BTree::BTree(File& input, bool useDisk) : input_(input) {
    if (useDisk) {
        this->tree_ = std::make_unique<BTreeFile>(input);
    } else {
        this->tree_ = std::make_unique<BTreeMemory>(input);
    }
    Log::Info("BTree: initialized for file " + input_.path());
}

std::optional<IBTreeData::Entry> BTree::FindEntryInNode(
    const IBTreeData::Node& node, int key) {
    for (size_t i = 0; i < node.size; i++) {
        Metrics::RecordKeyComparison();
        if (node.entries[i].key == key) {
            return node.entries[i];
        }
    }
    return std::nullopt;
}

std::optional<Item> BTree::SearchInPage(const std::array<Item, PAGE_SIZE>& page,
                                        int key, size_t itemCount) {
    for (size_t i = 0; i < itemCount; i++) {
        Metrics::RecordKeyComparison();
        if (page[i].key == key) {
            return page[i];
        }
    }
    return std::nullopt;
}

std::optional<Item> BTree::Search(int key) {
    Log::Info("BTree: starting search for key " + std::to_string(key));
    // Busca o nó desejado na cache
    std::optional<IBTreeData::Node> const node = this->tree_->Search(key);
    if (!node) {
        Log::Info("BTree: key " + std::to_string(key) +
                  " not found in B-tree index");
        return std::nullopt;
    }

    // varre o nó e retorna o indice da pagina da chave correpondente
    std::optional<IBTreeData::Entry> const entry = FindEntryInNode(*node, key);
    if (!entry) {
        Log::Info("BTree: key " + std::to_string(key) +
                  " not found in node entries");
        return std::nullopt;
    }

    // Busca a página correpondende ao indice da entrada
    uint64_t const pageIdx = entry->pageIndex;
    Log::Info("BTree: key " + std::to_string(key) +
              " found in B-tree index at page " + std::to_string(pageIdx) +
              ", fetching page from file");
    std::array<Item, PAGE_SIZE> const page = input_.GetPageAt(pageIdx);

    uint64_t const firstItem = pageIdx * PAGE_SIZE;
    uint64_t const remaining =
        input_.quantity() > firstItem ? input_.quantity() - firstItem : 0;
    size_t const itemCount =
        std::min(static_cast<uint64_t>(PAGE_SIZE), remaining);

    // retorna o item completo se ele existir na pagina
    std::optional<Item> const result = SearchInPage(page, key, itemCount);
    if (result) {
        Log::Info("BTree: key " + std::to_string(key) + " confirmed in page " +
                  std::to_string(pageIdx));
        return result;
    }

    Log::Error("BTree: key " + std::to_string(key) +
               " found in index pointing to page " + std::to_string(pageIdx) +
               ", but item is not present in data page");
    return std::nullopt;
}

}  // namespace Algorithm::BTree