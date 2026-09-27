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

namespace Algorithm::BinaryTree {
class BTreeFile {
public:
    struct Node {
        int key;
        uint64_t pageIndex;
        uint64_t left = 0;
        uint64_t right = 0;
    };

    struct Metadata {
        std::filesystem::file_time_type lastModification;
        uint64_t size;
    };

    /**
     * @brief Construtor da classe BTreeFile.
     *
     * Obtém o caminho do arquivo de árvore através de `GetFilePath` e tenta
     * reaproveitar um arquivo existente e válido via `TryLoadExistingFile`.
     * Se o arquivo não existir ou for inválido/desatualizado, constrói a
     * árvore do zero chamando `BuildFile`.
     *
     * @param input Referência para o arquivo de entrada com os dados originais.
     */
    // 1
    explicit BTreeFile(File& input);

    /**
     * @brief Destrutor da classe BTreeFile.
     *
     * Fecha o fluxo de leitura do arquivo binário (`file_`) caso esteja aberto,
     * liberando o descritor de arquivo do sistema.
     */
    // 1
    ~BTreeFile();

    /**
     * @brief Realiza a pesquisa de um registro pela chave na árvore binária em
     * disco.
     *
     * Valida se o arquivo possui metadados e nós. Inicia a busca a partir do
     * nó raiz (índice 0) e percorre a árvore binária em disco lendo os nós
     * com `ReadNode`:
     * - Se a chave for igual à do nó, retorna o item correspondente.
     * - Se a chave buscada for menor, navega para o filho à esquerda (`left`).
     * - Se a chave buscada for maior, navega para o filho à direita (`right`).
     * Se atingir um ponteiro nulo (0) ou houver falha de leitura, a busca é
     * finalizada.
     *
     * @param key Chave numérica a ser procurada.
     * @return std::optional<Node> O nó encontrado caso a chave exista;
     *         caso contrário, std::nullopt (vazio).
     */
    // 3
    std::optional<Node> Search(int key);

private:
    std::ifstream file_;

    /**
     * @brief Gera o caminho do arquivo binário da árvore a partir do arquivo de
     * entrada.
     *
     * Acrescenta o sufixo ".binarytree" ao caminho original do arquivo de
     * dados.
     *
     * @param input Arquivo de dados de entrada.
     * @return std::string Caminho completo do arquivo da árvore binária.
     */
    // 1
    static std::string GetFilePath(const File& input);

    /**
     * @brief Calcula o deslocamento (offset em bytes) de um nó no arquivo da
     * árvore.
     *
     * O offset é computado somando o tamanho dos metadados (`sizeof(Metadata)`)
     * ao índice do nó multiplicado pelo tamanho de um nó (`nodeIndex *
     * sizeof(Node)`).
     *
     * @param nodeIndex Índice numérico do nó na árvore.
     * @return std::streamoff Posição em bytes do nó em relação ao início do
     * arquivo.
     */
    // 1
    static std::streamoff GetNodeOffset(uint64_t nodeIndex);

    /**
     * @brief Escreve um nó binário em uma posição específica do arquivo.
     *
     * Posiciona o ponteiro de gravação do arquivo (`seekp`) no offset calculado
     * por `GetNodeOffset(nodeIndex)` e grava os bytes da estrutura `Node`.
     *
     * @param file Fluxo de arquivo aberto em modo de escrita binária.
     * @param nodeIndex Índice onde o nó deve ser gravado.
     * @param node Dados do nó a serem persistidos.
     */
    // 1
    static void WriteNode(std::fstream& file, uint64_t nodeIndex,
                          const Node& node);

    /**
     * @brief Lê um nó binário do arquivo a partir de seu índice.
     *
     * Posiciona o ponteiro de leitura (`seekg`) no offset do nó correspondente
     * e lê os dados binários para dentro da estrutura `node`.
     *
     * @param file Fluxo de entrada aberto em modo binário.
     * @param nodeIndex Índice do nó a ser lido.
     * @param node Referência de saída onde o nó lido será armazenado.
     * @return true Se o nó foi lido com sucesso; false caso contrário.
     */
    // 1
    static bool ReadNode(std::istream& file, uint64_t nodeIndex, Node& node);

    /**
     * @brief Adiciona um novo nó ao final do arquivo da árvore.
     *
     * Incrementa o contador `lastNodeIndex`, grava o nó na nova posição através
     * de `WriteNode` e retorna o novo índice criado.
     *
     * @param file Fluxo de arquivo aberto em modo de escrita.
     * @param node Estrutura do nó a ser gravado.
     * @param lastNodeIndex Referência para o contador do índice do último nó.
     * @return uint64_t Índice atribuído ao nó recém-adicionado.
     */
    // 1
    static uint64_t AppendNode(std::fstream& file, const Node& node,
                               uint64_t& lastNodeIndex);

    /**
     * @brief Grava a estrutura de metadados no início do arquivo de árvore.
     *
     * Registra no offset 0 a data/hora da última modificação e o tamanho em
     * bytes do arquivo de entrada original para validação posterior de
     * consistência.
     *
     * @param file Fluxo de arquivo aberto para escrita.
     * @param input Arquivo de entrada do qual os metadados serão extraídos.
     */
    // 1
    static void WriteMetadata(std::fstream& file, const File& input);

    /**
     * @brief Inicializa o nó raiz da árvore binária no arquivo (índice 0).
     *
     * Obtém a página central do arquivo de dados para escolher o primeiro item
     * dessa página como raiz da árvore, promovendo um melhor equilíbrio inicial
     * da árvore binária, gravando-o no índice 0.
     *
     * @param file Fluxo de arquivo onde a raiz será gravada.
     * @param input Arquivo de entrada de onde o item raiz será lido.
     */
    // 2
    static void InitializeRoot(std::fstream& file, File& input);

    /**
     * @brief Popula a árvore binária com todos os itens do arquivo de dados.
     *
     * Percorre sequencialmente o arquivo de entrada lendo página por página via
     * `GetNextPage()` até atingir `eof()`, inserindo cada página na árvore
     * por meio de `InsertPage`.
     *
     * @param file Fluxo do arquivo de árvore aberto para leitura e escrita.
     * @param input Arquivo de dados de entrada.
     * @param lastNodeIndex Referência para o controle do índice do último nó.
     */
    // 2
    static void PopulateTree(std::fstream& file, File& input,
                             uint64_t& lastNodeIndex);

    /**
     * @brief Insere todos os itens de uma página na árvore binária em disco.
     *
     * Itera sobre os itens contidos no array da página em memória e chama
     * `InsertItem` individualmente para cada um deles.
     *
     * @param file Fluxo do arquivo de árvore.
     * @param page Array contendo os itens da página carregada em memória.
     * @param pageIndex Índice da página correspondente no arquivo de dados.
     * @param lastNodeIndex Referência para o índice do último nó da árvore.
     */
    // 2
    static void InsertPage(std::fstream& file,
                           const std::array<Item, PAGE_SIZE>& page,
                           uint64_t pageIndex, uint64_t& lastNodeIndex);

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
    static void InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, uint64_t& lastNodeIndex);

    /**
     * @brief Tenta carregar e validar um arquivo de árvore binária
     * pré-existente.
     *
     * Verifica se o arquivo existe no disco, abre-o em modo binário e valida
     * seus metadados com `ValidateFile`. Em caso de sucesso, mantém o fluxo
     * `file_` aberto e retorna true. Caso contrário, retorna false.
     *
     * @param path Caminho do arquivo da árvore binária.
     * @param input Arquivo de entrada original para validação de integridade.
     * @return true Se o arquivo existente foi validado e carregado; false caso
     * contrário.
     */
    // 1
    bool TryLoadExistingFile(const std::string& path, const File& input);

    /**
     * @brief Valida se o arquivo da árvore corresponde ao arquivo de dados
     * original.
     *
     * Lê os metadados do início do arquivo de árvore e verifica se o timestamp
     * de modificação e o tamanho em bytes são idênticos aos do arquivo `input`.
     *
     * @param input Arquivo de entrada original.
     * @return true Se os metadados coincidirem com o arquivo original; false
     * caso contrário.
     */
    // 1
    bool ValidateFile(const File& input);

    /**
     * @brief Constrói o arquivo completo da árvore binária em disco a partir do
     * arquivo de dados.
     *
     * Cria ou trunca o arquivo binário no caminho indicado, grava os metadados
     * via `WriteMetadata`, inicializa o nó raiz através de `InitializeRoot`,
     * popula a árvore chamando `PopulateTree`, fecha o fluxo de escrita e
     * reabre o arquivo como somente leitura em `file_`.
     *
     * @param input Arquivo de dados de entrada.
     * @param path Caminho onde o arquivo da árvore binária será criado.
     */
    // 2
    void BuildFile(File& input, const std::string& path);
};
}  // namespace Algorithm::BinaryTree
