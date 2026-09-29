#include "BTreeFile.hpp"

#include <stdexcept>

#include "../Common.hpp"
// #include "File.hpp"
// #include "Item.hpp"

using namespace Algorithm::BinaryTree;

void BTreeFile::InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, uint64_t& lastNodeIndex) {
    uint64_t currentIndex = 0;

    // Cada iteração lê do disco o nó que deve ser comparado.
    while (true) {
        Node currentNode{};
        if (!ReadNode(file, currentIndex, currentNode)) {
            // Não há uma raiz válida (ou ocorreu uma falha de leitura).
            return;
        }

        if (item.key == currentNode.key) {
            // Chaves repetidas não criam nós adicionais.
            return;
        }

        // Decide qual ramo seguir conforme a ordenação da árvore binária.
        uint64_t& childIndex =
            item.key < currentNode.key ? currentNode.left : currentNode.right;
        if (childIndex != 0) {
            // O filho existe: continua a busca a partir do índice dele.
            currentIndex = childIndex;
            continue;
        }

        // O ponteiro nulo marca a posição vazia. Cria ali o novo nó e liga ele
        // ao pai, persistindo a alteração do pai no arquivo.
        const Node newNode{
            .key = item.key, .pageIndex = pageIndex, .left = 0, .right = 0};
        childIndex = AppendNode(file, newNode, lastNodeIndex);
        WriteNode(file, currentIndex, currentNode);
        return;
    }
}

std::streamoff BTreeFile::GetNodeOffset(uint64_t nodeIndex) {
    return static_cast<std::streamoff>((nodeIndex * sizeof(Node)) +
                                       sizeof(Metadata));
}

void BTreeFile::BuildFile(File& input, const std::string& path) {
    // Fecha qualquer árvore previamente carregada para que a nova versão possa
    // ser criada sem manter um fluxo antigo aberto.
    file_.close();
    file_.clear();

    // Abre para leitura e escrita binária. `trunc` descarta uma árvore antiga.
    std::fstream output(path, std::ios::binary | std::ios::in | std::ios::out |
                                  std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to create binary tree file: " + path);
    }

    // Os metadados permitem verificar depois se a árvore corresponde ao
    // arquivo de dados que lhe deu origem.
    WriteMetadata(output, input);
    if (!output) {
        throw std::runtime_error("Failed to write binary tree metadata");
    }

    // Um arquivo vazio não possui raiz. Para arquivos com dados, grava a raiz
    // e começa a numeração dos nós a partir dela (índice zero).
    if (input.quantity() > 0) {
        InitializeRoot(output, input);
        if (!output) {
            throw std::runtime_error("Failed to initialize binary tree root");
        }

        uint64_t lastNodeIndex = 0;
        PopulateTree(output, input, lastNodeIndex);
        if (!output) {
            throw std::runtime_error("Failed to populate binary tree file");
        }
    }

    // Garante que todos os bytes tenham sido enviados ao disco antes de fechar.
    output.flush();
    if (!output) {
        throw std::runtime_error("Failed to flush binary tree file");
    }

    output.close();
    if (output.fail()) {
        throw std::runtime_error("Failed to close binary tree file");
    }

    // Reabre a árvore em modo somente leitura para as operações de busca.
    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to reopen binary tree file: " + path);
    }
}

void BTreeFile::WriteNode(std::fstream& file, uint64_t nodeIndex,
                          const Node& node) {
    file.seekg(GetNodeOffset(nodeIndex));
    file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
}

bool BTreeFile::ReadNode(std::istream& file, uint64_t nodeIndex, Node& node) {
    if (!file.eof()) {
        file.seekg(GetNodeOffset(nodeIndex));
        if (file.eof()) {
            return false;
        }
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        return true;
    }

    return false;
}

uint64_t BTreeFile::AppendNode(std::fstream& file, const Node& node,
                               uint64_t& lastNodeIndex) {
    lastNodeIndex++;  // incrementa o contador
    file.seekg(GetNodeOffset(lastNodeIndex));
    if (!file.eof()) {
        WriteNode(file, lastNodeIndex, node);
        return lastNodeIndex;
    }

    return 0;
}

void BTreeFile::WriteMetadata(std::fstream& file, const File& input) {
    // Reiniciando arquivo
    file.clear();
    file.seekg(0, std::ios::beg);
    file.seekp(0, std::ios::beg);
    // Buscando  metadados
    Metadata tmp;
    tmp.lastModification = input.lastModification();
    tmp.size = input.size();
    // escrevendo os metadados na arvore
    file.write(reinterpret_cast<char*>(&tmp), sizeof(Metadata));
}

void BTreeFile::InitializeRoot(std::fstream& file, File& input) {
    // calcula onde a página central está
    uint64_t const itemMeio = input.quantity() / 2;
    u_int64_t const pageIndex = itemMeio / PAGE_SIZE;

    u_int64_t const offsetNaPag =
        itemMeio % PAGE_SIZE;  // item central da página do meio
    std::array<Item, PAGE_SIZE> page;
    page = input.GetPageAt(pageIndex);
    // inicializando valores
    Node noRaiz;
    noRaiz.key = page[offsetNaPag].key;
    noRaiz.pageIndex = pageIndex;
    noRaiz.left = 0;
    noRaiz.right = 0;
    // Escreve no final do aquivo
    WriteNode(file, 0, noRaiz);
}

void BTreeFile::InsertPage(std::fstream& file,
                           const std::array<Item, PAGE_SIZE>& page,
                           uint64_t pageIndex, uint64_t& lastNodeIndex) {
    for (int i = 0; i < PAGE_SIZE; i++) {
        InsertItem(file, page[i], pageIndex, lastNodeIndex);
    }
}

void BTreeFile::PopulateTree(std::fstream& file, File& input,
                             uint64_t& lastNodeIndex) {
    lastNodeIndex = 0;
    u_int64_t pageIndex = 0;
    std::array<Item, PAGE_SIZE> page;
    while (!input.eof()) {
        page = input.GetNextPage();
        InsertPage(file, page, pageIndex, lastNodeIndex);
        pageIndex++;
    }
}
