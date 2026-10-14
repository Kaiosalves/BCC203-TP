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

/**
 * @brief Localiza a posição de busca de uma chave dentro de um nó ordenado.
 *
 * Executa busca binária nas entradas do nó.
 *
 * @param node Nó da árvore B.
 * @param key Chave buscada.
 * @param found Saída indicando se a chave existe exatamente no nó.
 * @return size_t Índice da entrada correspondente (se found for true) ou o
 *         índice do ponteiro do filho onde a chave deve residir.
 */
// 2
size_t BTreeFile::FindKeyIndex(const BTreeFile::Node& node, int key,
                               bool& found) {
    // Busca a primeira posição que tem chave  maior ou igual à chave buscada.
    // Essa posição indica o filho por onde a busca deve continuar,
    // caso a chave não esteja neste nó.
    size_t first = 0;
    auto last = static_cast<size_t>(node.size);

    while (first < last) {
        size_t const middle = first + ((last - first) / 2);

        if (node.entries[middle].key < key) {
            first = middle + 1;
        } else {
            last = middle;
        }
    }

    found = first < node.size && node.entries[first].key == key;
    return first;
}

/**
 * @brief Insere uma entrada e seu ponteiro à direita em um nó que não está
 * cheio.
 *
 * Desloca as entradas e ponteiros maiores mantendo a ordenação.
 *
 * @param node Nó com espaço disponível (size < PAGE_SIZE).
 * @param entry Entrada a ser inserida.
 * @param rightChild Ponteiro para o filho à direita associado à entrada (-1
 * se folha).
 */
// 2
void BTreeFile::InsertIntoNonFullNode(IBTreeData::Node& node,
                                      const IBTreeData::Entry& entry,
                                      int64_t rightChild) {
    if (node.size >= PAGE_SIZE) {
        throw std::logic_error("Cannot insert into a full B-tree node");
    }

    // Encontra a posição em que a nova entrada mantem as chaves ordenadas
    auto insertionIndex = static_cast<size_t>(node.size);
    while (insertionIndex > 0 &&
           node.entries[insertionIndex - 1].key > entry.key) {
        --insertionIndex;
    }

    // Abre espaço para a nova entrada, deslocando as entradas maiores a direita
    for (auto index = static_cast<size_t>(node.size); index > insertionIndex;
         --index) {
        node.entries[index] = node.entries[index - 1];
    }

    // Desloca os ponteiros a direita da posição de insercao para preservar
    // a associacao entre cada entrada e seus filhos esquerdo e direito.
    size_t const lastChildIndex = static_cast<size_t>(node.size) + 1;
    for (size_t index = lastChildIndex; index > insertionIndex + 1; --index) {
        node.nodePos[index] = node.nodePos[index - 1];
    }

    node.entries[insertionIndex] = entry;
    node.nodePos[insertionIndex + 1] = rightChild;
    ++node.size;
}

/**
 * @brief Realiza a divisão (split) de um nó cheio ao receber uma nova
 * entrada.
 *
 * Divide as (PAGE_SIZE + 1) entradas em duas metades: a inferior permanece
 * no nó atual, a entrada central é promovida para o pai, e a metade
 * superior é salva em um novo nó irmão.
 *
 * @param file Fluxo de arquivo para persistência dos nós.
 * @param node Nó atual que atingiu a capacidade máxima.
 * @param nodeIndex Índice do nó atual no arquivo.
 * @param entry Entrada que causou o transbordamento.
 * @param rightChildIndex Ponteiro direito associado à nova entrada.
 * @param promotedEntry Saída contendo a entrada promovida para o nó pai.
 * @param createdSiblingIndex Saída contendo o índice do novo nó irmão
 * criado.
 * @param lastNodeIndex Referência para o controle do índice do último nó no
 * arquivo.
 */
// 2
void BTreeFile::SplitNode(std::fstream& file, IBTreeData::Node& node,
                          uint64_t nodeIndex, const IBTreeData::Entry& entry,
                          int64_t rightChildIndex,
                          IBTreeData::Entry& promotedEntry,
                          int64_t& createdSiblingIndex,
                          uint64_t& lastNodeIndex) {
    if (node.size != PAGE_SIZE) {
        throw std::logic_error("Split requires a full B-tree node");
    }

    // Reserva espaço para todas as entradas e filhos depois da insercao
    std::array<IBTreeData::Entry, PAGE_SIZE + 1> entriesAfterInsertion{};
    std::array<int64_t, PAGE_SIZE + 2> childrenAfterInsertion{};

    // Encontra a posicao da nova entrada dentro das entradas ordenadas
    auto insertionIndex = static_cast<size_t>(node.size);
    while (insertionIndex > 0 &&
           node.entries[insertionIndex - 1].key > entry.key) {
        --insertionIndex;
    }

    // Copia as entradas anteriores à posição de insercao.
    for (size_t index = 0; index < insertionIndex; ++index) {
        entriesAfterInsertion[index] = node.entries[index];
    }

    // Insere a nova entrada e copia as entradas restantes uma posicao adiante.
    entriesAfterInsertion[insertionIndex] = entry;
    for (size_t index = insertionIndex; index < node.size; ++index) {
        entriesAfterInsertion[index + 1] = node.entries[index];
    }

    // Os filhos até a posição de inserção ficam nas mesmas posições.
    for (size_t index = 0; index <= insertionIndex; ++index) {
        childrenAfterInsertion[index] = node.nodePos[index];
    }

    // O novo filho direito fica depois da entrada inserida.
    childrenAfterInsertion[insertionIndex + 1] = rightChildIndex;

    // Os filhos que estavam à direita da inserção avançam uma posição.
    for (size_t index = insertionIndex + 1; index <= node.size; ++index) {
        childrenAfterInsertion[index + 1] = node.nodePos[index];
    }

    // Promove a entrada central e calcula quantas entradas ficam em cada nó.
    size_t const totalEntries = static_cast<size_t>(node.size) + 1;
    size_t const middleIndex = totalEntries / 2;
    promotedEntry = entriesAfterInsertion[middleIndex];

    size_t const leftSize = middleIndex;
    size_t const rightSize = totalEntries - middleIndex - 1;

    // Reconstroi o nó original com a metade esquerda.
    node.entries.fill(IBTreeData::Entry{});
    node.nodePos.fill(-1);
    node.size = leftSize;

    for (size_t index = 0; index < leftSize; ++index) {
        node.entries[index] = entriesAfterInsertion[index];
    }
    for (size_t index = 0; index <= leftSize; ++index) {
        node.nodePos[index] = childrenAfterInsertion[index];
    }

    // Monta o novo irmão com a metade direita e seus ponteiros.
    IBTreeData::Node sibling;
    sibling.size = rightSize;

    for (size_t index = 0; index < rightSize; ++index) {
        sibling.entries[index] = entriesAfterInsertion[middleIndex + 1 + index];
    }
    for (size_t index = 0; index <= rightSize; ++index) {
        sibling.nodePos[index] =
            childrenAfterInsertion[middleIndex + 1 + index];
    }

    // Grava o novo irmão ao final do arquivo e atualiza o nó original.
    createdSiblingIndex =
        static_cast<int64_t>(AppendNode(file, sibling, lastNodeIndex));
    WriteNode(file, nodeIndex, node);
}

/**
 * @brief Função auxiliar recursiva para inserção descendente na árvore B.
 *
 * @param file Fluxo de arquivo da árvore B.
 * @param currentNodeIndex Índice do nó atual na recursão.
 * @param entryToInsert Entrada a ser inserida.
 * @param promotedEntry Saída para entrada promovida se houver divisão do nó
 * filho.
 * @param newChildIndex Saída para índice do novo nó criado na divisão.
 * @param lastNodeIndex Referência para o controle de nós no arquivo.
 * @return true se o nó atual foi dividido e requer inserção no pai; false
 * caso contrário.
 */
// 2
bool BTreeFile::InsertInternal(std::fstream& file, int64_t currentNodeIndex,
                               const IBTreeData::Entry& entryToInsert,
                               IBTreeData::Entry& promotedEntry,
                               int64_t& newChildIndex,
                               uint64_t& lastNodeIndex) {
    if (currentNodeIndex < 0) {
        throw std::logic_error("Invalid B-tree node index during insertion");
    }

    auto const nodeIndex = static_cast<uint64_t>(currentNodeIndex);
    IBTreeData::Node currentNode;

    // Carrega do arquivo o nó em que esta etapa da inserção está trabalhando
    if (!ReadNode(file, nodeIndex, currentNode)) {
        throw std::runtime_error(
            "Failed to read a B-tree node during insertion");
    }

    // Descobre se a entrada já existe e qual posição deve ser analisada.
    bool keyAlreadyExists = false;
    size_t const entryIndex =
        FindKeyIndex(currentNode, entryToInsert.key, keyAlreadyExists);

    // Chaves repetidas não devem ser inseridas novamente.
    if (keyAlreadyExists) {
        return false;
    }

    // Em uma folha, a entrada pode ser inserida diretamente se tiver espaço.
    if (currentNode.isLeaf()) {
        if (currentNode.size < PAGE_SIZE) {
            InsertIntoNonFullNode(currentNode, entryToInsert, -1);
            WriteNode(file, nodeIndex, currentNode);
            return false;
        }

        // Se a folha estiver cheia, faz a divisao e devolve a entrada
        // promovida.
        SplitNode(file, currentNode, nodeIndex, entryToInsert, -1,
                  promotedEntry, newChildIndex, lastNodeIndex);
        return true;
    }

    // Em um nó interno, continua a inserção pelo filho correspondente.
    int64_t const childIndex = currentNode.nodePos[entryIndex];
    if (childIndex < 0) {
        throw std::runtime_error("Internal B-tree node is missing a child");
    }

    bool const childWasSplit =
        InsertInternal(file, childIndex, entryToInsert, promotedEntry,
                       newChildIndex, lastNodeIndex);

    // Sem divisão no filho, este nó não precisa ser modificado.
    if (!childWasSplit) {
        return false;
    }

    // O filho foi dividido, entao insere a entrada promovida se tiver espaço.
    if (currentNode.size < PAGE_SIZE) {
        InsertIntoNonFullNode(currentNode, promotedEntry, newChildIndex);
        WriteNode(file, nodeIndex, currentNode);
        return false;
    }

    // Se este nó também estiver cheio, divida e passe a promoção adiante
    IBTreeData::Entry promotedByCurrentNode;
    int64_t siblingCreatedByCurrentNode = -1;
    SplitNode(file, currentNode, nodeIndex, promotedEntry, newChildIndex,
              promotedByCurrentNode, siblingCreatedByCurrentNode,
              lastNodeIndex);

    promotedEntry = promotedByCurrentNode;
    newChildIndex = siblingCreatedByCurrentNode;
    return true;
}

/**
 * @brief Insere um item individual na árvore B.
 *
 * Inicia a inserção a partir da raiz e, se a raiz for dividida, cria uma
 * nova raiz, aumentando a altura da árvore.
 *
 * @param file Fluxo do arquivo em disco.
 * @param item Item a ser inserido.
 * @param pageIndex Índice da página do arquivo de dados onde o item se
 * encontra.
 * @param rootIndex Referência para o índice da raiz atual.
 * @param lastNodeIndex Referência para o controle de nós no arquivo.
 */
// 2
void BTreeFile::InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, int64_t& rootIndex,
                           uint64_t& lastNodeIndex) {
    IBTreeData::Entry entryToInsert;
    entryToInsert.key = item.key;
    entryToInsert.pageIndex = pageIndex;

    // índice -1 representa uma arvore sem raiz
    if (rootIndex == -1) {
        IBTreeData::Node root;
        InsertIntoNonFullNode(root, entryToInsert, -1);

        // O primeiro no da arvore ocupa o índice zero do arquivo
        WriteNode(file, 0, root);
        if (!file) {
            throw std::runtime_error("Failed to write the initial B-tree root");
        }

        rootIndex = 0;
        lastNodeIndex = 0;
        return;
    }

    if (rootIndex < 0) {
        throw std::logic_error("Invalid B-tree root index during insertion");
    }

    // Insere a entrada a partir da raiz atual.
    IBTreeData::Entry promotedEntry;
    int64_t newChildIndex = -1;
    bool const rootWasSplit =
        InsertInternal(file, rootIndex, entryToInsert, promotedEntry,
                       newChildIndex, lastNodeIndex);

    if (!rootWasSplit) {
        return;
    }

    // A raiz foi dividida, entao cria uma nova raiz apontando para as duas
    // metades.
    if (newChildIndex < 0) {
        throw std::logic_error("Root split did not create a right child");
    }

    IBTreeData::Node newRoot;
    newRoot.entries[0] = promotedEntry;
    newRoot.nodePos[0] = rootIndex;
    newRoot.nodePos[1] = newChildIndex;
    newRoot.size = 1;

    rootIndex = static_cast<int64_t>(AppendNode(file, newRoot, lastNodeIndex));
    if (!file) {
        throw std::runtime_error("Failed to write the new B-tree root");
    }
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
