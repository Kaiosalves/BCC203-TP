#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "IBStarTreeData.hpp"

namespace Algorithm::BStarTree {
class BStarTreeMemory : public IBStarTreeData {
public:
    /**
     * @brief Construtor da classe BStarTreeMemory.
     *
     * Inicializa a árvore B* em memória principal (RAM) a partir do arquivo
     * binário de dados fornecido, realizando a leitura sequencial e inserção
     * de todos os itens.
     *
     * @param input Referência para o arquivo binário de dados de entrada.
     */
    // 1
    explicit BStarTreeMemory(File& input);

    /**
     * @brief Construtor padrão da classe BStarTreeMemory.
     *
     * Inicializa uma árvore B* vazia em memória principal.
     */
    // 1
    BStarTreeMemory();

    /**
     * @brief Destrutor padrão da classe BStarTreeMemory.
     */
    // 1
    ~BStarTreeMemory() override = default;

    /**
     * @brief Realiza a pesquisa de uma chave na árvore B* em memória RAM.
     *
     * Percorre a árvore B* a partir do nó raiz navegando pelos nós de índice
     * em memória até encontrar o nó folha (Data) onde a chave reside ou
     * constatar que ela não existe.
     *
     * @param key Chave numérica a ser procurada.
     * @return std::optional<Node> O nó folha contendo a chave, ou std::nullopt.
     */
    // 3
    std::optional<Node> Search(int key) override;

private:
    std::vector<Node> nodes_;
    int64_t rootIndex_ = -1;

    /**
     * @brief Adiciona um novo nó ao vetor de nós em memória.
     *
     * @param node Nó a ser gravado em memória.
     * @return uint64_t Novo índice atribuído ao nó no vetor.
     */
    // 1
    uint64_t AppendNode(const Node& node);

    /**
     * @brief Localiza a posição de busca ou inserção de uma chave dentro de um
     * nó ordenado.
     *
     * @param node Nó da árvore B* (Index ou Data).
     * @param key Chave buscada.
     * @param found Saída indicando se a chave existe exatamente no nó.
     * @return size_t Para nós Data, índice da entrada ou de inserção. Para nós
     *         Index, índice do ponteiro do filho onde a chave deve residir.
     */
    // 2
    static size_t FindKeyIndex(const Node& node, int key, bool& found);

    /**
     * @brief Insere uma entrada em um nó folha (Data) que não está cheio.
     *
     * @param node Nó de dados com espaço disponível (size < PAGE_SIZE).
     * @param entry Entrada a ser inserida.
     */
    // 2
    static void InsertIntoNonFullDataNode(Node& node, const Entry& entry);

    /**
     * @brief Insere uma chave e seu índice de filho direito em um nó de índice
     * que não está cheio.
     *
     * @param node Nó de índice com espaço disponível (size < PAGE_SIZE).
     * @param key Chave a ser inserida.
     * @param rightChild Índice do filho à direita associado à chave.
     */
    // 2
    static void InsertIntoNonFullIndexNode(Node& node, int key,
                                           int64_t rightChild);

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
    void SplitDataNode(uint64_t nodeIndex, const Entry& entry,
                       Entry& promotedEntry, int64_t& createdSiblingIndex);

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
    void SplitIndexNode(uint64_t nodeIndex, int key, int64_t rightChildIndex,
                        Entry& promotedEntry, int64_t& createdSiblingIndex);

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
    bool InsertInternal(int64_t currentNodeIndex, const Entry& entryToInsert,
                        Entry& promotedEntry, int64_t& newChildIndex);

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
    void InsertItem(const Item& item, uint64_t pageIndex);

    /**
     * @brief Insere os itens de uma página de dados na árvore B* em memória.
     *
     * @param page Array contendo os itens da página.
     * @param pageIndex Índice da página de dados.
     * @param itemCount Quantidade de itens válidos na página.
     */
    // 1
    void InsertPage(const std::array<Item, PAGE_SIZE>& page, uint64_t pageIndex,
                    size_t itemCount);

    /**
     * @brief Popula a árvore B* lendo sequencialmente as páginas do arquivo de
     * dados para a memória.
     *
     * @param input Arquivo de dados de entrada.
     */
    // 1
    void PopulateTree(File& input);
};
}  // namespace Algorithm::BStarTree
