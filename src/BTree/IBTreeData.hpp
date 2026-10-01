#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "../Common.hpp"

namespace Algorithm::BTree {
class IBTreeData {
public:
    /**
     * @brief Entrada armazenada em cada nó da árvore B.
     *
     * Contém a chave inteira de busca e o índice da página no arquivo de dados
     * onde o item reside.
     */
    struct Entry {
        int key = 0;
        uint64_t pageIndex = 0;
    };

    /**
     * @brief Estrutura de um nó da árvore B.
     *
     * Cada nó armazena até PAGE_SIZE entradas ordenadas por chave e até
     * (PAGE_SIZE + 1) ponteiros (índices) para nós filhos.
     */
    struct Node {
        std::array<Entry, PAGE_SIZE> entries{};
        std::array<int64_t, PAGE_SIZE + 1> nodePos{};
        uint64_t size = 0;

        /**
         * @brief Verifica se o nó é um nó folha.
         *
         * Em uma árvore B balanceada, um nó é folha quando não possui nós
         * filhos (todos os ponteiros são -1).
         *
         * @return true se o nó for folha; false caso contrário.
         */
        // 1
        [[nodiscard]] bool isLeaf() const { return this->nodePos[0] == -1; }

        /**
         * @brief Construtor padrão do nó.
         *
         * Inicializa o tamanho como 0 e preenche todos os ponteiros de filhos
         * com -1.
         */
        // 1
        Node() { this->nodePos.fill(-1); }
    };

    IBTreeData() = default;
    virtual ~IBTreeData() = default;

    /**
     * @brief Realiza a pesquisa de uma chave na árvore B.
     *
     * Percorre a árvore B a partir do nó raiz até encontrar o nó contendo
     * a chave ou atingir uma folha sem sucesso.
     *
     * @param key Chave numérica a ser procurada.
     * @return std::optional<Node> O nó que contém a chave, ou std::nullopt.
     */
    // 3
    virtual std::optional<Node> Search(int key) = 0;
};
}  // namespace Algorithm::BTree
