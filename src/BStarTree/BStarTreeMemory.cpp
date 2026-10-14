#include "BStarTreeMemory.hpp"

#include <array>
#include <cstdint>
#include <optional>

#include "../Common.hpp"

namespace Algorithm::BStarTree {

BStarTreeMemory::BStarTreeMemory() {
    // forca os nos comecarem zerados
    this->nodes_.clear();
}

BStarTreeMemory::BStarTreeMemory(File& input) {
    // chama a funcao para povoar a arvore
    this->PopulateTree(input);
}

std::optional<IBStarTreeData::Node> BStarTreeMemory::Search(int key) {
    if (rootIndex_ < 0) {
        return std::nullopt;
    }

    IBStarTreeData::Node currentNode;
    int64_t currentIndex = rootIndex_;
    // desce na arvore até encontrar o nó folha desejado
    while (currentIndex >= 0 &&
           static_cast<size_t>(currentIndex) < this->nodes_.size()) {
        currentNode = nodes_[currentIndex];
        if (currentNode.isLeaf()) {
            return currentNode;
        }

        bool found = false;
        size_t const chdIndex = FindKeyIndex(currentNode, key, found);

        currentIndex = currentNode.index.nodePos[chdIndex];
    }
    return std::nullopt;
}

size_t BStarTreeMemory::FindKeyIndex(const Node& node, int key, bool& found) {
    found = false;

    if (node.isLeaf()) {
        for (uint64_t i = 0; i < node.size; i++) {
            if (node.data.entries[i].key == key) {
                found = true;
                return i;
            }
            if (node.data.entries[i].key > key) {
                return i;
            }
        }
    } else {
        for (uint64_t i = 0; i < node.size; i++) {
            if (node.index.indexes[i] > key) {
                return i;
            }
        }
    }
    return node.size;
}

uint64_t BStarTreeMemory::AppendNode(const Node& node) {
    // insere o no no final do vetor
    this->nodes_.push_back(node);

    // retorna o indice do no
    return static_cast<uint64_t>(this->nodes_.size() - 1);
}

void BStarTreeMemory::InsertPage(const std::array<Item, PAGE_SIZE>& page,
                                 uint64_t pageIndex, size_t itemCount) {
    // percorre os itens da pagina
    for (size_t i = 0; i < itemCount; i++) {
        // insere o item na arvore
        this->InsertItem(page[i], pageIndex);
    }
}

void BStarTreeMemory::PopulateTree(File& input) {
    uint64_t pageIndex = 0;

    // varre o arquivo ate o final
    while (!input.eof()) {
        std::array<Item, PAGE_SIZE> const page = input.GetNextPage();

        // insere a pagina, que insere os itens na arvore na memoria
        this->InsertPage(page, pageIndex, PAGE_SIZE);
        pageIndex++;
    }
}

}  // namespace Algorithm::BStarTree