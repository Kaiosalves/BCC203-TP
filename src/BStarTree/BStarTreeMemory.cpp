#include "BStarTreeMemory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

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

size_t BStarTreeMemory::FindKeyIndex(const IBStarTreeData::Node& node, int key,
                                     bool& found) {
    size_t first = 0;
    auto last = static_cast<size_t>(node.size);

    if (node.type == IBStarTreeData::Node::Type::Data) {
        // Nas folhas, encontra a primeira entrada cuja chave seja maior ou
        // igual a buscada. Essa posicao também serve como posição de inserção.
        while (first < last) {
            size_t const middle = first + ((last - first) / 2);

            if (node.data.entries[middle].key < key) {
                first = middle + 1;
            } else {
                last = middle;
            }
        }

        found = first < node.size && node.data.entries[first].key == key;
        return first;
    }

    // Em nós de indice, as chaves sao separadores: uma chave igual ao
    // separador segue para o filho à direita dele.
    while (first < last) {
        size_t const middle = first + ((last - first) / 2);

        if (key < node.index.indexes[middle]) {
            last = middle;
        } else {
            first = middle + 1;
        }
    }

    // Nós de índice guardam separadores, não entradas de dados.
    found = false;
    return first;
}

/**
 * @brief Insere uma entrada em um nó folha (Data) que não está cheio.
 *
 * @param node Nó de dados com espaço disponível (size < PAGE_SIZE).
 * @param entry Entrada a ser inserida.
 */
// 2
void BStarTreeMemory::InsertIntoNonFullDataNode(
    IBStarTreeData::Node& node, const IBStarTreeData::Entry& entry) {
    if (node.type != IBStarTreeData::Node::Type::Data) {
        throw std::logic_error("Cannot insert a data entry into an index node");
    }

    if (node.size >= PAGE_SIZE) {
        throw std::logic_error("Cannot insert into a full B-star data node");
    }

    // Encontra a posição ordenada da chave sem duplicar ela
    bool keyAlreadyExists = false;
    size_t const insertionIndex =
        FindKeyIndex(node, entry.key, keyAlreadyExists);
    if (keyAlreadyExists) {
        return;
    }

    // Abre espaço deslocando as entradas maiores uma posição para a direita.
    for (auto index = static_cast<size_t>(node.size); index > insertionIndex;
         --index) {
        node.data.entries[index] = node.data.entries[index - 1];
    }

    node.data.entries[insertionIndex] = entry;
    ++node.size;
}

/**
 * @brief Insere uma chave e seu índice de filho direito em um nó de índice
 * que não está cheio.
 *
 * @param node Nó de índice com espaço disponível (size < PAGE_SIZE).
 * @param key Chave a ser inserida.
 * @param rightChild Índice do filho à direita associado à chave.
 */
// 2
void BStarTreeMemory::InsertIntoNonFullIndexNode(IBStarTreeData::Node& node,
                                                 int key, int64_t rightChild) {
    if (node.type != IBStarTreeData::Node::Type::Index) {
        throw std::logic_error("Cannot insert an index key into a data node");
    }

    if (node.size >= PAGE_SIZE) {
        throw std::logic_error("Cannot insert into a full B-star index node");
    }

    // Encontra onde a nova chave separadora deve ficar em ordem crescente.
    size_t insertionIndex = 0;
    while (insertionIndex < node.size &&
           node.index.indexes[insertionIndex] < key) {
        ++insertionIndex;
    }

    // Não adicione novamente uma chave separadora que já esteja no nó.
    if (insertionIndex < node.size &&
        node.index.indexes[insertionIndex] == key) {
        return;
    }

    // Abre espaço para a chave deslocando as chaves maiores à direita.
    for (auto index = static_cast<size_t>(node.size); index > insertionIndex;
         --index) {
        node.index.indexes[index] = node.index.indexes[index - 1];
    }

    // Desloque os filhos à direita. O novo filho fica logo após a chave.
    size_t const lastChildIndex = static_cast<size_t>(node.size) + 1;
    for (size_t index = lastChildIndex; index > insertionIndex + 1; --index) {
        node.index.nodePos[index] = node.index.nodePos[index - 1];
    }

    node.index.indexes[insertionIndex] = key;
    node.index.nodePos[insertionIndex + 1] = rightChild;
    ++node.size;
}

/**
 * @brief Realiza a divisão (split) de um nó folha (Data) cheio em memória.
 *
 * @param nodeIndex Índice do nó folha atual que atingiu a capacidade
 * máxima.
 * @param entry Entrada que causou o transbordamento.
 * @param promotedEntry Saída contendo a entrada promovida com a chave
 * divisora.
 * @param createdSiblingIndex Saída contendo o índice do novo nó folha irmão
 * criado.
 */
// 2
void BStarTreeMemory::SplitDataNode(uint64_t nodeIndex,
                                    const IBStarTreeData::Entry& entry,
                                    IBStarTreeData::Entry& promotedEntry,
                                    int64_t& createdSiblingIndex) {
    if (nodeIndex >= this->nodes_.size()) {
        throw std::out_of_range("B-star data node index is out of range");
    }

    IBStarTreeData::Node& originalNode = this->nodes_[nodeIndex];
    if (originalNode.type != IBStarTreeData::Node::Type::Data) {
        throw std::logic_error("Cannot split a non-data B-star node");
    }

    if (originalNode.size != PAGE_SIZE) {
        throw std::logic_error("SplitDataNode requires a full data node");
    }

    // Combina a nova entrada com as entradas existentes mantendo a ordem
    std::array<IBStarTreeData::Entry, PAGE_SIZE + 1> entriesAfterInsertion{};
    bool keyAlreadyExists = false;
    size_t const insertionIndex =
        FindKeyIndex(originalNode, entry.key, keyAlreadyExists);

    if (keyAlreadyExists) {
        throw std::logic_error(
            "Cannot split a data node to insert a duplicate key");
    }

    for (size_t index = 0; index < insertionIndex; ++index) {
        entriesAfterInsertion[index] = originalNode.data.entries[index];
    }

    entriesAfterInsertion[insertionIndex] = entry;

    for (size_t index = insertionIndex; index < originalNode.size; ++index) {
        entriesAfterInsertion[index + 1] = originalNode.data.entries[index];
    }

    // Divide o conjunto em duas metades e usa a menor chave da direita como
    // separador para que buscas com essa chave sigam o novo irmão
    size_t const totalEntries = static_cast<size_t>(originalNode.size) + 1;
    size_t const rightStartIndex = totalEntries / 2;
    size_t const leftSize = rightStartIndex;
    size_t const rightSize = totalEntries - rightStartIndex;

    IBStarTreeData::Node sibling(IBStarTreeData::Node::Type::Data);
    sibling.size = rightSize;

    originalNode.data.entries.fill(IBStarTreeData::Entry{});
    originalNode.size = leftSize;

    for (size_t index = 0; index < leftSize; ++index) {
        originalNode.data.entries[index] = entriesAfterInsertion[index];
    }

    for (size_t index = 0; index < rightSize; ++index) {
        sibling.data.entries[index] =
            entriesAfterInsertion[rightStartIndex + index];
    }

    promotedEntry = sibling.data.entries[0];

    // Insere o novo nó na lista encadeada das folhas e atualiza o anterior
    sibling.data.nextData = originalNode.data.nextData;
    createdSiblingIndex = static_cast<int64_t>(this->AppendNode(sibling));
    this->nodes_[nodeIndex].data.nextData = createdSiblingIndex;
}

/**
 * @brief Realiza a divisão (split) de um nó interno (Index) cheio em
 * memória.
 *
 * @param nodeIndex Índice do nó de índice atual que atingiu a capacidade
 * máxima.
 * @param key Chave promovida do filho que causou o transbordamento.
 * @param rightChildIndex Índice do filho direito associado à nova chave.
 * @param promotedEntry Saída contendo a entrada promovida para o pai.
 * @param createdSiblingIndex Saída contendo o índice do novo nó irmão
 * criado.
 */
// 2
void BStarTreeMemory::SplitIndexNode(uint64_t nodeIndex, int key,
                                     int64_t rightChildIndex,
                                     IBStarTreeData::Entry& promotedEntry,
                                     int64_t& createdSiblingIndex) {
    if (nodeIndex >= this->nodes_.size()) {
        throw std::out_of_range("B-star index node index is out of range");
    }

    IBStarTreeData::Node const& originalNode = this->nodes_[nodeIndex];
    if (originalNode.type != IBStarTreeData::Node::Type::Index) {
        throw std::logic_error("Cannot split a non-index B-star node");
    }

    if (originalNode.size != PAGE_SIZE) {
        throw std::logic_error("SplitIndexNode requires a full index node");
    }

    // Cria vetores temporários para as PAGE_SIZE + 1 chaves e seus filhos.
    std::array<int, PAGE_SIZE + 1> keysAfterInsertion{};
    std::array<int64_t, PAGE_SIZE + 2> childrenAfterInsertion{};

    // Localiza a posição ordenada da chave promovida pelo filho.
    size_t insertionIndex = 0;
    while (insertionIndex < originalNode.size &&
           originalNode.index.indexes[insertionIndex] < key) {
        ++insertionIndex;
    }

    if (insertionIndex < originalNode.size &&
        originalNode.index.indexes[insertionIndex] == key) {
        throw std::logic_error(
            "Cannot split an index node with a duplicate key");
    }

    // Copia as chaves menores, insere a nova chave e copia as restantes.
    for (size_t index = 0; index < insertionIndex; ++index) {
        keysAfterInsertion[index] = originalNode.index.indexes[index];
    }

    keysAfterInsertion[insertionIndex] = key;

    for (size_t index = insertionIndex; index < originalNode.size; ++index) {
        keysAfterInsertion[index + 1] = originalNode.index.indexes[index];
    }

    // Mantem a esquerda o filho da posição de inserção e coloca o novo
    // filho a direita da chave inserida.
    for (size_t index = 0; index <= insertionIndex; ++index) {
        childrenAfterInsertion[index] = originalNode.index.nodePos[index];
    }

    childrenAfterInsertion[insertionIndex + 1] = rightChildIndex;

    for (size_t index = insertionIndex + 1; index <= originalNode.size;
         ++index) {
        childrenAfterInsertion[index + 1] = originalNode.index.nodePos[index];
    }

    // Promove a chave central. Ao contrario das folhas, ela sai dos dois nós
    size_t const totalKeys = static_cast<size_t>(originalNode.size) + 1;
    size_t const middleIndex = totalKeys / 2;
    promotedEntry = IBStarTreeData::Entry{
        .key = keysAfterInsertion[middleIndex], .pageIndex = 0};

    size_t const leftSize = middleIndex;
    size_t const rightSize = totalKeys - middleIndex - 1;

    IBStarTreeData::Node leftNode(IBStarTreeData::Node::Type::Index);
    leftNode.size = leftSize;

    for (size_t index = 0; index < leftSize; ++index) {
        leftNode.index.indexes[index] = keysAfterInsertion[index];
    }
    for (size_t index = 0; index <= leftSize; ++index) {
        leftNode.index.nodePos[index] = childrenAfterInsertion[index];
    }

    IBStarTreeData::Node sibling(IBStarTreeData::Node::Type::Index);
    sibling.size = rightSize;

    for (size_t index = 0; index < rightSize; ++index) {
        sibling.index.indexes[index] =
            keysAfterInsertion[middleIndex + 1 + index];
    }
    for (size_t index = 0; index <= rightSize; ++index) {
        sibling.index.nodePos[index] =
            childrenAfterInsertion[middleIndex + 1 + index];
    }

    // Acrescenta o irmão ao vetor e substitui o nó original pela metade
    // esquerda. A atribuição após AppendNode evita usar uma referência
    // invalidada.
    createdSiblingIndex = static_cast<int64_t>(this->AppendNode(sibling));
    this->nodes_[nodeIndex] = leftNode;
}

/**
 * @brief Função auxiliar recursiva para inserção descendente na árvore B*
 * em memória.
 *
 * @param currentNodeIndex Índice do nó atual na recursão.
 * @param entryToInsert Entrada a ser inserida.
 * @param promotedEntry Saída para entrada promovida se houver divisão.
 * @param newChildIndex Saída para índice do novo nó criado na divisão.
 * @return true se o nó atual foi dividido e requer inserção no pai; false
 * caso contrário.
 */
// 2
bool BStarTreeMemory::InsertInternal(int64_t currentNodeIndex,
                                     const IBStarTreeData::Entry& entryToInsert,
                                     IBStarTreeData::Entry& promotedEntry,
                                     int64_t& newChildIndex) {
    if (currentNodeIndex < 0 ||
        static_cast<uint64_t>(currentNodeIndex) >= this->nodes_.size()) {
        throw std::out_of_range("B-star node index is out of range");
    }

    auto const nodeIndex = static_cast<uint64_t>(currentNodeIndex);
    IBStarTreeData::Node currentNode = this->nodes_[nodeIndex];

    if (currentNode.size > PAGE_SIZE) {
        throw std::logic_error("B-star node contains too many keys");
    }

    // Procura a chave no nó atual ou determina a posição/filho a seguir.
    bool keyAlreadyExists = false;
    size_t const keyIndex =
        FindKeyIndex(currentNode, entryToInsert.key, keyAlreadyExists);

    if (currentNode.type == IBStarTreeData::Node::Type::Data) {
        // A chave já está na folha, então a árvore não precisa ser alterada.
        if (keyAlreadyExists) {
            return false;
        }

        if (currentNode.size < PAGE_SIZE) {
            InsertIntoNonFullDataNode(currentNode, entryToInsert);
            this->nodes_[nodeIndex] = currentNode;
            return false;
        }

        // Uma folha cheia é dividida e envia seu separador ao nó pai.
        SplitDataNode(nodeIndex, entryToInsert, promotedEntry, newChildIndex);
        return true;
    }

    // Nós indice não armazenam os registros, entao encaminha a inserção ao
    // filho.
    int64_t const childIndex = currentNode.index.nodePos[keyIndex];
    if (childIndex < 0) {
        throw std::logic_error("B-star index node is missing a child");
    }

    bool const childWasSplit =
        InsertInternal(childIndex, entryToInsert, promotedEntry, newChildIndex);

    if (!childWasSplit) {
        return false;
    }

    // Se tiver espaço, coloque no nó atual a chave e o novo filho direito.
    if (currentNode.size < PAGE_SIZE) {
        InsertIntoNonFullIndexNode(currentNode, promotedEntry.key,
                                   newChildIndex);
        this->nodes_[nodeIndex] = currentNode;
        return false;
    }

    // Se nó também estiver cheio, divide ele e faca a nova promoção.
    int const keyToPromote = promotedEntry.key;
    IBStarTreeData::Entry promotionFromCurrentNode;
    int64_t siblingCreatedByCurrentNode = -1;

    SplitIndexNode(nodeIndex, keyToPromote, newChildIndex,
                   promotionFromCurrentNode, siblingCreatedByCurrentNode);

    promotedEntry = promotionFromCurrentNode;
    newChildIndex = siblingCreatedByCurrentNode;
    return true;
}

/**
 * @brief Insere um item individual na árvore B* em memória.
 *
 * Inicia a inserção a partir da raiz e, se a raiz for dividida, cria uma
 * nova raiz (Index), aumentando a altura da árvore.
 *
 * @param item Item a ser inserido.
 * @param pageIndex Índice da página do arquivo de dados onde o item se
 * encontra.
 */
// 2
void BStarTreeMemory::InsertItem(const Item& item, uint64_t pageIndex) {
    IBStarTreeData::Entry const entryToInsert{.key = item.key,
                                              .pageIndex = pageIndex};

    // Quando a árvore está vazia, cria sua primeira folha que vai ser a raiz.
    if (this->rootIndex_ == -1) {
        IBStarTreeData::Node root(IBStarTreeData::Node::Type::Data);
        InsertIntoNonFullDataNode(root, entryToInsert);

        this->rootIndex_ = static_cast<int64_t>(this->AppendNode(root));
        return;
    }

    if (this->rootIndex_ < 0 ||
        static_cast<uint64_t>(this->rootIndex_) >= this->nodes_.size()) {
        throw std::logic_error("B-star root index is invalid");
    }

    // Insere a nova entrada a partir da raiz atual
    IBStarTreeData::Entry promotedEntry;
    int64_t newChildIndex = -1;
    bool const rootWasSplit = InsertInternal(this->rootIndex_, entryToInsert,
                                             promotedEntry, newChildIndex);

    if (!rootWasSplit) {
        return;
    }

    if (newChildIndex < 0) {
        throw std::logic_error("B-star root split did not create a sibling");
    }

    // Se a raiz foi dividida, cria uma nova raiz indice com os dois filhos
    IBStarTreeData::Node newRoot(IBStarTreeData::Node::Type::Index);
    newRoot.index.indexes[0] = promotedEntry.key;
    newRoot.index.nodePos[0] = this->rootIndex_;
    newRoot.index.nodePos[1] = newChildIndex;
    newRoot.size = 1;

    this->rootIndex_ = static_cast<int64_t>(this->AppendNode(newRoot));
}

}  // namespace Algorithm::BStarTree
