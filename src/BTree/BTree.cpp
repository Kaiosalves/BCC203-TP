#include "BTree.hpp"

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
}

std::optional<IBTreeData::Entry> BTree::FindEntryInNode(
    const IBTreeData::Node& node, int key) {
    for (size_t i = 0; i < node.size; i++) {
        if (node.entries[i].key == key) {
            return node.entries[i];
        }
    }
    return std::nullopt;
}

std::optional<Item> BTree::SearchInPage(const std::array<Item, PAGE_SIZE>& page,
                                        int key) {
    for (size_t i = 0; i < PAGE_SIZE; i++) {
        if (page.at(i).key == key) {
            return page.at(i);
        }
    }
    return std::nullopt;
}

std::optional<Item> BTree::Search(int key) {
    // Busca o nó desejado na cache
    std::optional<IBTreeData::Node> node = this->tree_->Search(key);
    if (!node) {
        return std::nullopt;
    }
    // varre o nó e retorna o indice da pagina da chave correpondente
    std::optional<IBTreeData::Entry> entry = FindEntryInNode(*node, key);
    if (!entry) {
        return std::nullopt;
    }
    // Busca a página correpondende ao indice da entrada
    uint64_t const pageIdx = entry->pageIndex;
    auto page = input_.GetPageAt(pageIdx);
    // retorna o item completo se ele existir na pagina
    return SearchInPage(page, key);
}

}  // namespace Algorithm::BTree