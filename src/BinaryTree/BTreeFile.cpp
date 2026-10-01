#include "BTreeFile.hpp"

#include <algorithm>
#include <stdexcept>

#include "../Common.hpp"

using namespace Algorithm::BinaryTree;

BTreeFile::BTreeFile(File& input) {
    std::string const path = GetFilePath(input);
    Log::Info("BTreeFile: checking existing binary tree file: " + path);

    if (!TryLoadExistingFile(path, input)) {
        Log::Info(
            "BTreeFile: existing binary tree not found or invalid, building "
            "new file: " +
            path);
        BuildFile(input, path);
    } else {
        Log::Info("BTreeFile: loaded existing binary tree file successfully: " +
                  path);
    }
}

// Destrutor
BTreeFile::~BTreeFile() {
    if (this->file_.is_open()) {
        this->file_.close();
    }
}

void BTreeFile::InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, uint64_t& lastNodeIndex) {
    uint64_t currentIndex = 0;

    // Cada iteração lê do disco o nó que deve ser comparado.
    while (true) {
        Node currentNode{};
        if (!ReadNode(file, currentIndex, currentNode)) {
            // Não há uma raiz válida (ou ocorreu uma falha de leitura).
            Log::Error("BTreeFile: failed to read node " +
                       std::to_string(currentIndex) + " while inserting item " +
                       std::to_string(item.key));
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
    Log::Info("BTreeFile: starting build of binary tree file: " + path);
    // Fecha qualquer árvore previamente carregada para que a nova versão possa
    // ser criada sem manter um fluxo antigo aberto.
    file_.close();
    file_.clear();

    // Abre para leitura e escrita binária. `trunc` descarta uma árvore antiga.
    std::fstream output(path, std::ios::binary | std::ios::in | std::ios::out |
                                  std::ios::trunc);
    if (!output.is_open()) {
        Log::Error("BTreeFile: failed to create binary tree file: " + path);
        throw std::runtime_error("Failed to create binary tree file: " + path);
    }

    // Os metadados permitem verificar depois se a árvore corresponde ao
    // arquivo de dados que lhe deu origem.
    WriteMetadata(output, input);
    if (!output) {
        Log::Error("BTreeFile: failed to write binary tree metadata to: " +
                   path);
        throw std::runtime_error("Failed to write binary tree metadata");
    }

    // Um arquivo vazio não possui raiz. Para arquivos com dados, grava a raiz
    // e começa a numeração dos nós a partir dela (índice zero).
    if (input.quantity() > 0) {
        Log::Info("BTreeFile: initializing root node from median item");
        InitializeRoot(output, input);
        if (!output) {
            Log::Error("BTreeFile: failed to initialize binary tree root");
            throw std::runtime_error("Failed to initialize binary tree root");
        }

        uint64_t lastNodeIndex = 0;
        Log::Info("BTreeFile: populating binary tree with " +
                  std::to_string(input.quantity()) + " items");
        PopulateTree(output, input, lastNodeIndex);
        if (!output) {
            Log::Error("BTreeFile: failed to populate binary tree file");
            throw std::runtime_error("Failed to populate binary tree file");
        }
        Log::Info("BTreeFile: tree populated successfully with " +
                  std::to_string(lastNodeIndex + 1) + " nodes");
    }

    // Garante que todos os bytes tenham sido enviados ao disco antes de fechar.
    output.flush();
    if (!output) {
        Log::Error("BTreeFile: failed to flush binary tree file: " + path);
        throw std::runtime_error("Failed to flush binary tree file");
    }

    output.close();
    if (output.fail()) {
        Log::Error("BTreeFile: failed to close binary tree file: " + path);
        throw std::runtime_error("Failed to close binary tree file");
    }

    // Reabre a árvore em modo somente leitura para as operações de busca.
    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
        Log::Error("BTreeFile: failed to reopen binary tree file: " + path);
        throw std::runtime_error("Failed to reopen binary tree file: " + path);
    }
    Log::Info("BTreeFile: binary tree file built and reopened successfully: " +
              path);
}

void BTreeFile::WriteNode(std::fstream& file, uint64_t nodeIndex,
                          const Node& node) {
    file.seekg(GetNodeOffset(nodeIndex));
    file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
    if (!file) {
        Log::Error("BTreeFile: failed to write node at index " +
                   std::to_string(nodeIndex));
    }
}

bool BTreeFile::ReadNode(std::istream& file, uint64_t nodeIndex, Node& node) {
    if (!file.eof()) {
        file.seekg(GetNodeOffset(nodeIndex));
        if (file.eof()) {
            return false;
        }
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        if (file.fail()) {
            Log::Error("BTreeFile: read failed at node index " +
                       std::to_string(nodeIndex));
            return false;
        }
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

    Log::Error("BTreeFile: failed to seek to append node at index " +
               std::to_string(lastNodeIndex));
    return 0;
}

void BTreeFile::WriteMetadata(std::fstream& file, const File& input) {
    // Reiniciando arquivo
    file.clear();
    file.seekg(0, std::ios::beg);
    file.seekp(0, std::ios::beg);
    // Buscando metadados
    Metadata tmp;
    tmp.lastModification = input.lastModification();
    tmp.size = input.size();
    // escrevendo os metadados na arvore
    file.write(reinterpret_cast<char*>(&tmp), sizeof(Metadata));
    if (!file) {
        Log::Error("BTreeFile: failed to write metadata header");
    }
}

void BTreeFile::InitializeRoot(std::fstream& file, File& input) {
    // calcula onde a página central está
    uint64_t const itemMeio = input.quantity() / 2;
    uint64_t const pageIndex = itemMeio / PAGE_SIZE;

    uint64_t const offsetNaPag =
        itemMeio % PAGE_SIZE;  // item central da página do meio
    std::array<Item, PAGE_SIZE> page;
    page = input.GetPageAt(pageIndex);
    // inicializando valores
    Node noRaiz;
    noRaiz.key = page[offsetNaPag].key;
    noRaiz.pageIndex = pageIndex;
    noRaiz.left = 0;
    noRaiz.right = 0;
    Log::Info("BTreeFile: root node initialized with key " +
              std::to_string(noRaiz.key) + " (page " +
              std::to_string(pageIndex) + ", offset " +
              std::to_string(offsetNaPag) + ")");
    // Escreve no final do aquivo
    WriteNode(file, 0, noRaiz);
}

void BTreeFile::InsertPage(std::fstream& file,
                           const std::array<Item, PAGE_SIZE>& page,
                           uint64_t pageIndex, size_t itemCount,
                           uint64_t& lastNodeIndex) {
    for (size_t i = 0; i < itemCount; i++) {
        InsertItem(file, page[i], pageIndex, lastNodeIndex);
    }
}

void BTreeFile::PopulateTree(std::fstream& file, File& input,
                             uint64_t& lastNodeIndex) {
    lastNodeIndex = 0;
    uint64_t pageIndex = 0;
    std::array<Item, PAGE_SIZE> page{};
    while (pageIndex * PAGE_SIZE < input.quantity() && !input.eof()) {
        page = input.GetNextPage();
        uint64_t const firstItem = pageIndex * PAGE_SIZE;
        uint64_t const remaining = input.quantity() - firstItem;
        size_t const itemCount = static_cast<size_t>(
            std::min(static_cast<uint64_t>(PAGE_SIZE), remaining));
        InsertPage(file, page, pageIndex, itemCount, lastNodeIndex);
        pageIndex++;
    }
}

std::optional<BTreeFile::Node> BTreeFile::Search(int key) {
    if (!this->file_.is_open()) {
        Log::Error("BTreeFile: search failed, binary tree file is not open");
        return std::nullopt;
    }
    Log::Info("BTreeFile: starting search for key " + std::to_string(key));
    // inicia pela raiz
    uint64_t currentIndex = 0;
    Node currentNode;

    while (currentIndex != 0 ||
           GetNodeOffset(currentIndex) ==
               static_cast<std::streamoff>(sizeof(Metadata))) {
        if (!ReadNode(this->file_, currentIndex, currentNode)) {
            Log::Error("BTreeFile: failed to read node at index " +
                       std::to_string(currentIndex) + " during search");
            return std::nullopt;
        }

        Log::Info("BTreeFile: visiting node " + std::to_string(currentIndex) +
                  " (key=" + std::to_string(currentNode.key) +
                  ", page=" + std::to_string(currentNode.pageIndex) + ")");

        if (key == currentNode.key) {
            Log::Info("BTreeFile: key " + std::to_string(key) +
                      " matched at node " + std::to_string(currentIndex) +
                      " (page=" + std::to_string(currentNode.pageIndex) + ")");
            return currentNode;
        }
        // se for menor, filho da esquerda, se maior, filho da direita
        if (key < currentNode.key) {
            if (currentNode.left == 0) {
                Log::Info("BTreeFile: left child is empty (0), key " +
                          std::to_string(key) + " not in tree");
                break;
            }
            currentIndex = currentNode.left;
        } else {
            if (currentNode.right == 0) {
                Log::Info("BTreeFile: right child is empty (0), key " +
                          std::to_string(key) + " not in tree");
                break;
            }
            currentIndex = currentNode.right;
        }
    }

    Log::Info("BTreeFile: search finished, key " + std::to_string(key) +
              " not found");
    return std::nullopt;
}

std::string BTreeFile::GetFilePath(const File& input) {
    return input.path() + ".binarytree";
}

bool BTreeFile::TryLoadExistingFile(const std::string& path,
                                    const File& input) {
    if (!std::filesystem::exists(path)) {
        Log::Info("BTreeFile: binary tree file does not exist: " + path);
        return false;
    }

    this->file_.open(path, std::ios::in | std::ios::binary);

    if (!this->file_.is_open()) {
        Log::Error("BTreeFile: failed to open existing binary tree file: " +
                   path);
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

    // le e armazena os metadados do disco(tamanho e lastmodification)
    Metadata metadata;

    if (!this->file_.read(reinterpret_cast<char*>(&metadata),
                          sizeof(Metadata))) {
        Log::Error(
            "BTreeFile: failed to read metadata header from binary tree file");
        this->file_.clear();
        return false;
    }
    // variaveis que verificam se o cache tem os mesmos metadados
    bool const sameModificationTime =
        (metadata.lastModification == input.lastModification());

    bool const sameSize = (metadata.size == input.size());

    if (!sameModificationTime || !sameSize) {
        Log::Info(
            "BTreeFile: metadata mismatch between data file and binary tree "
            "file");
        return false;
    }

    Log::Info("BTreeFile: metadata successfully validated");
    return true;
}
