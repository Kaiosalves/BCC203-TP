#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "../Common.hpp"

namespace Algorithm::BStarTree {
class IBStarTreeData {
public:
    /**
     * @brief Entrada armazenada nos nós folha (Data) da árvore B*.
     *
     * Contém a chave inteira de busca e o índice da página no arquivo de dados
     * onde o item reside.
     */
    struct Entry {
        int key = 0;
        uint64_t pageIndex = 0;
    };

    /**
     * @brief Estrutura de um nó da árvore B*.
     *
     * A árvore B* diferencia nós internos (Index), que contêm apenas chaves e
     * apontadores para nós filhos, de nós folhas (Data), que contêm as entradas
     * reais com chave e pageIndex, além de um apontador para a próxima página
     * folha.
     */
    struct Node {
        enum Type : uint8_t {
            Index,
            Data,
        };

        struct Index {
            std::array<int, PAGE_SIZE> indexes{};
            std::array<int64_t, PAGE_SIZE + 1> nodePos{};
        };

        struct Data {
            std::array<Entry, PAGE_SIZE> entries{};
            int64_t nextData = -1;
        };

        Type type;
        union {
            struct Index index;
            struct Data data;
        };
        uint64_t size = 0;

        /**
         * @brief Verifica se o nó é um nó folha (tipo Data).
         *
         * @return true se o tipo for Data; false caso contrário.
         */
        // 1
        [[nodiscard]] bool isLeaf() const { return this->type == Type::Data; }

        /**
         * @brief Construtor padrão do nó. Inicializa como nó de dados vazio.
         */
        // 1
        Node() : Node(Type::Data) {}

        /**
         * @brief Construtor com especificação do tipo do nó.
         *
         * @param type Tipo do nó (Index ou Data).
         */
        // 1
        explicit Node(Type type) : type(type) {
            if (type == Type::Index) {
                this->index.indexes.fill(0);
                this->index.nodePos.fill(-1);
            } else {
                this->data.entries.fill(Entry{});
                this->data.nextData = -1;
            }
        }
    };

    IBStarTreeData() = default;
    virtual ~IBStarTreeData() = default;

    /**
     * @brief Realiza a pesquisa de uma chave na árvore B*.
     *
     * Percorre a árvore B* a partir do nó raiz navegando pelos nós de índice
     * até encontrar o nó folha (Data) onde a chave reside ou constatar que ela
     * não existe.
     *
     * @param key Chave numérica a ser procurada.
     * @return std::optional<Node> O nó folha contendo a chave, ou std::nullopt.
     */
    // 3
    virtual std::optional<Node> Search(int key) = 0;
};
}  // namespace Algorithm::BStarTree
