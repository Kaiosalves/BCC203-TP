#include "BTreeFile.hpp"

#include <optional>
#include <stdexcept>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"

namespace Algorithm::BTree {

BTreeFile::BTreeFile(File& input) {
    std::string const path = GetFilePath(input);

    if (!TryLoadExistingFile(path, input)) {
        BuildFile(input, path);
    }
}

BTreeFile::~BTreeFile() {
    if (this->file_.is_open()) {
        this->file_.close();
    }
}

std::string BTreeFile::GetFilePath(const File& input) {
    return input.path() + ".btree";
}

void BTreeFile::WriteMetadata(std::fstream& file, const File& input,
                              int64_t rootIndex) {
    // Reiniciando arquivo
    file.clear();
    file.seekg(0, std::ios::beg);
    file.seekp(0, std::ios::beg);
    // Buscando metadados
    Metadata tmp;
    tmp.lastModification = input.lastModification();
    tmp.size = input.size();
    tmp.rootIndex = rootIndex;
    // escrevendo os metadados na arvore
    file.write(reinterpret_cast<char*>(&tmp), sizeof(Metadata));
}

std::streamoff BTreeFile::GetNodeOffset(uint64_t nodeIndex) {
    return static_cast<std::streamoff>((nodeIndex * sizeof(Node)) +
                                       sizeof(Metadata));
}

bool BTreeFile::TryLoadExistingFile(const std::string& path,
                                    const File& input) {
    this->file_.open(path, std::ios::in | std::ios::binary);

    if (!this->file_.is_open()) {
        return false;
    }

    if (!ValidateFile(input)) {
        this->file_.close();
        return false;
    }

    return true;
}

bool BTreeFile::ValidateFile(const File& input) {
    // reposiciona o ponteiro pro inicio do arquivo de cache
    this->file_.seekg(0, std::ifstream::beg);

    // le e armazena os metadados do disco (tamanho e lastmodification)
    Metadata m;

    if (!this->file_.read(reinterpret_cast<char*>(&m), sizeof(Metadata))) {
        this->file_.clear();
        return false;
    }
    // variaveis que verificam se o cache tem os mesmos metadados
    bool const sameModificationTime =
        (m.lastModification == input.lastModification());
    bool const sameSize = (m.size == input.size());

    // retorna verdadeiro se eles tem os mesmos metadados, falso se nao
    return sameModificationTime && sameSize;
}

void BTreeFile::WriteNode(std::fstream& file, uint64_t nodeIndex,
                          const Node& node) {
    file.seekp(GetNodeOffset(nodeIndex));
    file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
}

bool BTreeFile::ReadNode(std::istream& file, uint64_t nodeIndex, Node& node) {
    std::streamoff const offset = GetNodeOffset(nodeIndex);

    file.seekg(offset, std::ios::beg);

    file.read(reinterpret_cast<char*>(&node), sizeof(Node));

    return !file.fail();
}

uint64_t BTreeFile::AppendNode(std::fstream& file, const Node& node,
                               uint64_t& lastNodeIndex) {
    lastNodeIndex++;

    WriteNode(file, lastNodeIndex, node);

    return lastNodeIndex;
}

size_t BTreeFile::FindKeyIndex(const Node& node, int key, bool& found) {
    found = false;
    for (size_t i = 0; i < node.size; i++) {
        if (node.entries[i].key == key) {
            found = true;
            return i;
        }
        if (node.entries[i].key > key) {
            return i;
        }
    }
    return node.size;
}

std::optional<BTreeFile::Node> BTreeFile::Search(int key) {
    if (rootIndex_ < 0) {
        return std::nullopt;
    }

    int64_t currentIndex = rootIndex_;
    Node currentNode;
    bool found = false;
    size_t idx = 0;

    while (currentIndex >= 0) {
        // lê um novo nó a partir do indice
        if (!ReadNode(this->file_, static_cast<uint64_t>(currentIndex),
                      currentNode)) {
            return std::nullopt;
        }
        // Busca a chave dentro do nó
        idx = FindKeyIndex(currentNode, key, found);
        // Achou? retorna o nó atual
        if (found) {
            return currentNode;
        }
        // Se não encontrou o item e o nó for folha o item não existe
        if (currentNode.isLeaf()) {
            return std::nullopt;
        }
        // se não continua caminhando pelos nós
        currentIndex = currentNode.nodePos[idx];
    }
    return std::nullopt;
}

}  // namespace Algorithm::BTree
