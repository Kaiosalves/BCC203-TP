#pragma once

#include <array>
#include <memory>
#include <optional>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "BStarTreeMemory.hpp"
#include "IBStarTreeData.hpp"

namespace Algorithm::BStarTree {
class BStarTree {
public:
    /**
     * @brief Construtor da classe BStarTree.
     *
     * Inicializa a árvore B* em memória principal a partir do arquivo
     * binário de dados fornecido (BStarTreeMemory).
     *
     * @param input Referência para o arquivo binário de dados de entrada.
     */
    // 3
    explicit BStarTree(File& input);

    /**
     * @brief Destrutor padrão da classe BStarTree.
     */
    ~BStarTree() = default;

    /**
     * @brief Realiza a busca de um item pela chave informada na árvore B*.
     *
     * Consulta primeiramente a estrutura da árvore B* através de
     * `tree_->Search(key)` para determinar em qual página do arquivo de dados a
     * chave reside:
     * - Se a chave não for encontrada na árvore B*, encerra a busca retornando
     * vazio.
     * - Se for encontrada, recupera o `pageIndex` contido na entrada do nó
     * folha e carrega essa página específica com `input_.GetPageAt(...)`.
     * - Realiza a busca na página carregada em memória principal.
     *
     * @param key Chave numérica inteira a ser buscada.
     * @return std::optional<Item> Contém o item encontrado se a chave existir;
     *         caso contrário, retorna std::nullopt.
     */
    // 3
    std::optional<Item> Search(int key);

private:
    File& input_;
    std::unique_ptr<IBStarTreeData> tree_;

    /**
     * @brief Localiza uma entrada com a chave especificada dentro de um nó
     * folha da árvore B*.
     *
     * @param node Nó recuperado da árvore B*.
     * @param key Chave procurada.
     * @return std::optional<IBStarTreeData::Entry> A entrada contendo a chave e
     * pageIndex, ou std::nullopt caso não esteja presente no nó.
     */
    // 3
    static std::optional<IBStarTreeData::Entry> FindEntryInNode(
        const IBStarTreeData::Node& node, int key);

    /**
     * @brief Realiza a busca pelo item desejado dentro de uma página carregada
     * em memória.
     *
     * @param page Array contendo os itens da página carregada.
     * @param key Chave numérica procurada.
     * @return std::optional<Item> O item encontrado ou std::nullopt.
     */
    // 3
    static std::optional<Item> SearchInPage(
        const std::array<Item, PAGE_SIZE>& page, int key);
};
}  // namespace Algorithm::BStarTree
