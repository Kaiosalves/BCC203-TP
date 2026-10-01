#include "BTree.hpp"

#include "../Common.hpp"

using namespace Algorithm::BinaryTree;

BTree::BTree(File& input): input_(input), file_(input){}

std::optional<Item> BTree::Search(int key) {
// procura o no chamando a busca em disco
    std::optional<BTreeFile::Node> const node = this->file_.Search(key);

    if (!node.has_value()) {
        return std::nullopt;
    }

//tras a pagina do arquivo pra memoria
    std::array<Item, PAGE_SIZE> const page = this->input_.GetPageAt(node->pageIndex);

// loop buscando o item dentro da pagina
    for (const auto& item : page) {
        if (item.key == key) {
            return item;
        }
    }

    return std::nullopt;
}
