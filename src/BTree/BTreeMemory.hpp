#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "IBTreeData.hpp"

namespace Algorithm::BTree {
class BTreeMemory : public IBTreeData {
public:
    /**
     * @brief Construtor da classe BTreeMemory.
     *
     * Inicializa a árvore B em memória principal (RAM) a partir do arquivo
     * binário de dados fornecido, realizando a leitura sequencial e inserção
     * de todos os itens.
     *
     * @param input Referência para o arquivo binário de dados de entrada.
     */
    // 1
    explicit BTreeMemory(File& input);

    /**
     * @brief Construtor padrão da classe BTreeMemory.
     *
     * Inicializa uma árvore B vazia em memória principal.
     */
    // 1
    BTreeMemory();

    /**
     * @brief Destrutor padrão da classe BTreeMemory.
     */
    // 1
    ~BTreeMemory() override = default;

    /**
     * @brief Realiza a pesquisa de uma chave na árvore B em memória RAM.
     *
     * Percorre a árvore B a partir do nó raiz navegando pelos nós residentes
     * em memória até encontrar o nó contendo a chave ou atingir uma folha sem
     * sucesso.
     *
     * @param key Chave numérica a ser procurada.
     * @return std::optional<Node> O nó que contém a chave, ou std::nullopt.
     */
    // 3
    std::optional<Node> Search(int key) override;

private:
    std::vector<Node> nodes_;
    int64_t rootIndex_ = -1;

    /**
     * @brief Adiciona um novo nó ao vetor de nós em memória.
     *
     * @param node Nó a ser armazenado na memória.
     * @return uint64_t Novo índice atribuído ao nó no vetor.
     */
    // 1
    uint64_t AppendNode(const Node& node);

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
    // 3
    static size_t FindKeyIndex(const Node& node, int key, bool& found);

    /**
     * @brief Insere uma entrada e seu índice de filho à direita em um nó que
     * não está cheio.
     *
     * Desloca as entradas e ponteiros maiores mantendo a ordenação.
     *
     * @param node Nó com espaço disponível (size < PAGE_SIZE).
     * @param entry Entrada a ser inserida.
     * @param rightChild Índice para o filho à direita associado à entrada (-1
     * se folha).
     */
    // 3
    static void InsertIntoNonFullNode(Node& node, const Entry& entry,
                                      int64_t rightChild);

    /**
     * @brief Realiza a divisão (split) de um nó cheio ao receber uma nova
     * entrada em memória.
     *
     * Divide as (PAGE_SIZE + 1) entradas em duas metades: a inferior permanece
     * no nó atual, a entrada central é promovida para o pai, e a metade
     * superior é salva em um novo nó irmão alocado em memória.
     *
     * @param nodeIndex Índice do nó atual que atingiu a capacidade máxima.
     * @param entry Entrada que causou o transbordamento.
     * @param rightChildIndex Índice do filho direito associado à nova entrada.
     * @param promotedEntry Saída contendo a entrada promovida para o nó pai.
     * @param createdSiblingIndex Saída contendo o índice do novo nó irmão
     * criado.
     */
    // 3
    void SplitNode(uint64_t nodeIndex, const Entry& entry,
                   int64_t rightChildIndex, Entry& promotedEntry,
                   int64_t& createdSiblingIndex);

    /**
     * @brief Função auxiliar recursiva para inserção descendente na árvore B em
     * memória.
     *
     * @param currentNodeIndex Índice do nó atual na recursão.
     * @param entryToInsert Entrada a ser inserida.
     * @param promotedEntry Saída para entrada promovida se houver divisão do nó
     * filho.
     * @param newChildIndex Saída para índice do novo nó criado na divisão.
     * @return true se o nó atual foi dividido e requer inserção no pai; false
     * caso contrário.
     */
    // 3
    bool InsertInternal(int64_t currentNodeIndex, const Entry& entryToInsert,
                        Entry& promotedEntry, int64_t& newChildIndex);

    /**
     * @brief Insere um item individual na árvore B em memória.
     *
     * Inicia a inserção a partir da raiz e, se a raiz for dividida, cria uma
     * nova raiz, aumentando a altura da árvore.
     *
     * @param item Item a ser inserido.
     * @param pageIndex Índice da página do arquivo de dados onde o item se
     * encontra.
     */
    // 3
    void InsertItem(const Item& item, uint64_t pageIndex);

    /**
     * @brief Insere os itens de uma página de dados na árvore B em memória.
     *
     * @param page Array contendo os itens da página.
     * @param pageIndex Índice da página de dados.
     * @param itemCount Quantidade de itens válidos na página.
     */
    // 1
    void InsertPage(const std::array<Item, PAGE_SIZE>& page, uint64_t pageIndex,
                    size_t itemCount);

    /**
     * @brief Popula a árvore B lendo sequencialmente as páginas do arquivo de
     * dados para a memória.
     *
     * @param input Arquivo de dados de entrada.
     */
    // 1
    void PopulateTree(File& input);
};
}  // namespace Algorithm::BTree
