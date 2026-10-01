#include "BTree.hpp"

#include <algorithm>

#include "../Common.hpp"

using namespace Algorithm::BinaryTree;

BTree::BTree(File& input) : input_(input), file_(input) {
    Log::Info("BinaryTree: initialized for file " + input_.path());
}

std::optional<Item> BTree::Search(int key) {
    Log::Info("BinaryTree: starting search for key " + std::to_string(key));
    // procura o no chamando a busca em disco
    std::optional<BTreeFile::Node> const node = this->file_.Search(key);

    if (!node.has_value()) {
        Log::Info("BinaryTree: key " + std::to_string(key) +
                  " not found in binary tree index");
        return std::nullopt;
    }

    Log::Info("BinaryTree: key " + std::to_string(key) +
              " found in binary tree index at page " +
              std::to_string(node->pageIndex) + ", fetching page from file");

    // tras a pagina do arquivo pra memoria
    std::array<Item, PAGE_SIZE> const page =
        this->input_.GetPageAt(node->pageIndex);

    const uint64_t firstItem = node->pageIndex * PAGE_SIZE;
    const uint64_t remainingItems = this->input_.quantity() > firstItem
                                        ? this->input_.quantity() - firstItem
                                        : 0;
    const size_t itemCount =
        std::min(static_cast<uint64_t>(PAGE_SIZE), remainingItems);

    // loop buscando o item dentro da pagina
    for (size_t i = 0; i < itemCount; ++i) {
        Metrics::RecordKeyComparison();
        if (page[i].key == key) {
            Log::Info("BinaryTree: key " + std::to_string(key) +
                      " confirmed in page " + std::to_string(node->pageIndex) +
                      " at position " + std::to_string(i));
            return page[i];
        }
    }

    Log::Error("BinaryTree: key " + std::to_string(key) +
               " found in index pointing to page " +
               std::to_string(node->pageIndex) +
               ", but item is not present in data page");
    return std::nullopt;
}
