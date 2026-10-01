#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"
#include "IBTreeData.hpp"

namespace Algorithm::BTree {
class BTreeFile : public IBTreeData {
public:
    /**
     * @brief Metadados gravados no cabeçalho do arquivo da árvore B em disco.
     */
    struct Metadata {
        std::filesystem::file_time_type lastModification;
        uint64_t size;
        int64_t rootIndex;
    };

    /**
     * @brief Construtor da classe BTreeFile.
     *
     * Obtém o caminho do arquivo de índice através de `GetFilePath` e tenta
     * carregar um arquivo existente via `TryLoadExistingFile`. Se não existir
     * ou for inválido, constrói a árvore chamando `BuildFile`.
     *
     * @param input Referência para o arquivo binário de dados de entrada.
     */
    // 1
    explicit BTreeFile(File& input);

    /**
     * @brief Destrutor da classe BTreeFile.
     *
     * Fecha o fluxo de leitura do arquivo em disco (`file_`).
     */
    // 1
    ~BTreeFile() override;

    /**
     * @brief Realiza a pesquisa de uma chave na árvore B em disco.
     *
     * Percorre a árvore B a partir do nó raiz carregando os nós necessários
     * até encontrar o nó contendo a chave ou atingir uma folha sem sucesso.
     *
     * @param key Chave numérica a ser procurada.
     * @return std::optional<Node> O nó que contém a chave, ou std::nullopt.
     */
    // 3
    std::optional<Node> Search(int key) override;

private:
    std::ifstream file_;
    int64_t rootIndex_ = 0;

    /**
     * @brief Gera o caminho do arquivo binário da árvore B a partir do arquivo
     * de entrada.
     *
     * @param input Arquivo de dados de entrada.
     * @return std::string Caminho completo com sufixo ".btree".
     */
    // 1
    static std::string GetFilePath(const File& input);

    /**
     * @brief Calcula a posição em bytes (offset) de um nó no arquivo da árvore
     * B.
     *
     * @param nodeIndex Índice numérico do nó.
     * @return std::streamoff Deslocamento em bytes em relação ao início do
     * arquivo.
     */
    // 1
    static std::streamoff GetNodeOffset(uint64_t nodeIndex);

    /**
     * @brief Tenta carregar e validar um arquivo de árvore B pré-existente.
     *
     * @param path Caminho do arquivo da árvore B.
     * @param input Arquivo de entrada original para validação de integridade.
     * @return true se o arquivo existe e é válido; false caso contrário.
     */
    // 1
    bool TryLoadExistingFile(const std::string& path, const File& input);

    /**
     * @brief Valida os metadados do arquivo da árvore B comparando com o
     * arquivo original.
     *
     * @param input Arquivo de entrada original.
     * @return true se os metadados forem compatíveis; false caso contrário.
     */
    // 1
    bool ValidateFile(const File& input);

    /**
     * @brief Constrói o arquivo da árvore B em disco a partir do arquivo de
     * dados.
     *
     * @param input Arquivo de dados de entrada.
     * @param path Caminho do arquivo a ser criado.
     */
    // 2
    void BuildFile(File& input, const std::string& path);

    /**
     * @brief Grava um nó na posição especificada do arquivo em disco.
     *
     * @param file Fluxo de arquivo aberto em modo de escrita binária.
     * @param nodeIndex Índice do nó a ser gravado.
     * @param node Estrutura do nó a ser persistida.
     */
    // 1
    static void WriteNode(std::fstream& file, uint64_t nodeIndex,
                          const Node& node);

    /**
     * @brief Lê um nó do arquivo em disco a partir de seu índice.
     *
     * @param file Fluxo de entrada aberto em modo binário.
     * @param nodeIndex Índice do nó a ser lido.
     * @param node Referência para armazenar o nó lido.
     * @return true se a leitura foi bem-sucedida; false caso contrário.
     */
    // 1
    static bool ReadNode(std::istream& file, uint64_t nodeIndex, Node& node);

    /**
     * @brief Adiciona um novo nó ao final do arquivo e atualiza o contador de
     * nós.
     *
     * @param file Fluxo aberto em modo de escrita.
     * @param node Nó a ser gravado.
     * @param lastNodeIndex Referência para o índice do último nó gravado.
     * @return uint64_t Novo índice atribuído ao nó.
     */
    // 1
    static uint64_t AppendNode(std::fstream& file, const Node& node,
                               uint64_t& lastNodeIndex);

    /**
     * @brief Grava a estrutura de metadados no início do arquivo de árvore B.
     *
     * @param file Fluxo aberto para escrita.
     * @param input Arquivo original de dados.
     * @param rootIndex Índice do nó raiz no momento da gravação.
     */
    // 1
    static void WriteMetadata(std::fstream& file, const File& input,
                              int64_t rootIndex);

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
    static size_t FindKeyIndex(const Node& node, int key, bool& found);

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
    static void InsertIntoNonFullNode(Node& node, const Entry& entry,
                                      int64_t rightChild);

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
    static void SplitNode(std::fstream& file, Node& node, uint64_t nodeIndex,
                          const Entry& entry, int64_t rightChildIndex,
                          Entry& promotedEntry, int64_t& createdSiblingIndex,
                          uint64_t& lastNodeIndex);

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
    static bool InsertInternal(std::fstream& file, int64_t currentNodeIndex,
                               const Entry& entryToInsert, Entry& promotedEntry,
                               int64_t& newChildIndex, uint64_t& lastNodeIndex);

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
    static void InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, int64_t& rootIndex,
                           uint64_t& lastNodeIndex);

    /**
     * @brief Insere os itens de uma página de dados na árvore B.
     *
     * @param file Fluxo do arquivo em disco.
     * @param page Array contendo os itens da página.
     * @param pageIndex Índice da página de dados.
     * @param itemCount Quantidade de itens válidos na página.
     * @param rootIndex Referência para o índice da raiz.
     * @param lastNodeIndex Referência para o controle de nós no arquivo.
     */
    // 2
    static void InsertPage(
        std::fstream& file, const std::array<Item, PAGE_SIZE>& page,
        uint64_t pageIndex,  // NOLINT(bugprone-easily-swappable-parameters)
        size_t itemCount, int64_t& rootIndex, uint64_t& lastNodeIndex);

    /**
     * @brief Popula a árvore B lendo sequencialmente as páginas do arquivo de
     * dados.
     *
     * @param file Fluxo do arquivo da árvore B.
     * @param input Arquivo de dados de entrada.
     * @param rootIndex Referência para o índice da raiz.
     * @param lastNodeIndex Referência para o controle de nós no arquivo.
     */
    // 2
    static void PopulateTree(std::fstream& file, File& input,
                             int64_t& rootIndex, uint64_t& lastNodeIndex);
};
}  // namespace Algorithm::BTree
