#include "BTreeFile.hpp"

#include <stdexcept>

namespace Algorithm::BinaryTree {

/**
     * @brief Insere um item individual na árvore binária em disco.
     *
     * Navega a partir da raiz (índice 0). Compara a chave do item com a do nó
     * atual:
     * - Se menor e o nó não possui filho esquerdo (`left == 0`), adiciona o
     * novo nó com `AppendNode`, atualiza o ponteiro `left` do nó pai e o salva
     * com `WriteNode`.
     * - Se maior e o nó não possui filho direito (`right == 0`), adiciona o
     * novo nó com `AppendNode`, atualiza o ponteiro `right` do nó pai e o salva
     * com `WriteNode`.
     * - Se a chave já existir no nó, a inserção é encerrada sem duplicar.
     * - Se o nó atual possui filho à esquerda ou à direita, a função recursivamente ou iterativamente continua a busca no nó filho correspondente até encontrar a posição correta para inserção.
     * @param file Fluxo de leitura/escrita do arquivo da árvore binária.
     * @param item Item a ser inserido.
     * @param pageIndex Índice da página do arquivo de dados onde o item se
     * encontra.
     * @param lastNodeIndex Referência para o índice do último nó da árvore.
     */
// 2
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

        // O ponteiro nulo marca a posição vazia. Cria ali o novo nó e liga ele ao pai, persistindo a alteração do pai no arquivo.
        const Node newNode{.key = item.key, .pageIndex = pageIndex, .left = 0, .right = 0};
        childIndex = AppendNode(file, newNode, lastNodeIndex);
        WriteNode(file, currentIndex, currentNode);
        return;
    }
}

/**
 * @brief Constrói o arquivo completo da árvore binária em disco a partir do
 * arquivo de dados.
 *
 * @param input Arquivo de dados de entrada.
 * @param path Caminho onde o arquivo da árvore binária será criado.
 */
// 2
void BTreeFile::BuildFile(File& input, const std::string& path) {
    // Fecha qualquer árvore previamente carregada para que a nova versão possa ser criada sem manter um fluxo antigo aberto.
    file_.close();
    file_.clear();

    // Abre para leitura e escrita binária. `trunc` descarta uma árvore antiga.
    std::fstream output(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
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

}  // namespace Algorithm::BinaryTree