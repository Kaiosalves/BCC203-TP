#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "../File.hpp"

/*
** How the cache file will be organized
** input last modification time | input size | 0-entry | ... | n-entry
*/

namespace Algorithm::IndexedSequentialAccess {
class Cache {
public:
    struct Entry {
        int key;
        uint64_t pageIndex;
    };

    struct Metadata {
        std::filesystem::file_time_type lastModification;
        uint64_t size;
    };

    // The explicity keyword is "optional", if removed the change is that the
    // compilar can explicity convert a File into a Cache
    // 1
    /**
     * @brief Construtor da classe Cache.
     *
     * Determina o caminho do arquivo de cache via `GetCachePath` e tenta
     * reaproveitar um arquivo existente e válido chamando
     * `TryLoadExistingCache`. Se o cache não existir ou estiver
     * inconsistente/desatualizado, constrói um novo índice chamando
     * `BuildCache`.
     *
     * @param input Referência para o arquivo de dados original.
     */
    explicit Cache(File& input);

    // 1
    /**
     * @brief Destrutor da classe Cache.
     *
     * Garante o fechamento do fluxo de leitura do arquivo de cache (`file_`)
     * caso ele permaneça aberto.
     */
    ~Cache();

    // 1
    /**
     * @brief Realiza busca binária diretamente no arquivo de índice/cache em
     * disco.
     *
     * Calcula o número total de entradas a partir do tamanho do arquivo menos
     * os metadados. Aplica a busca binária externa navegando pelos
     * deslocamentos dos registros `Entry` no arquivo (`sizeof(Metadata) + mid *
     * sizeof(Entry)`):
     * - Se a chave da entrada coincidir com a chave alvo, retorna a `Entry`
     * encontrada.
     * - Se a chave alvo for maior, ajusta a pesquisa para a metade superior.
     * - Se a chave alvo for menor, ajusta a pesquisa para a metade inferior.
     *
     * @param key Chave numérica a ser localizada no índice.
     * @return std::optional<Entry> Estrutura contendo a chave e o índice da
     * página no arquivo de dados, ou std::nullopt se não encontrada.
     */
    std::optional<Entry> Search(int key);

private:
    std::ifstream file_;

    // Check existing cache file data with it input
    // the checked fields are last modification time and size
    // 1
    /**
     * @brief Valida a correspondência entre o arquivo de cache e o arquivo de
     * dados.
     *
     * Lê a estrutura de metadados no início do arquivo de cache e compara a
     * data de modificação e o tamanho em bytes com os valores do arquivo
     * `input`.
     *
     * @param input Arquivo de dados original para comparação.
     * @return true Se o arquivo de cache for compatível e atualizado; false
     * caso contrário.
     */
    bool ValidateCache(const File& input);

    // 1
    /**
     * @brief Tenta carregar e validar um arquivo de cache existente em disco.
     *
     * Verifica a existência física do arquivo e valida seus metadados com
     * `ValidateCache`. Mantém o arquivo aberto em `file_` se válido.
     *
     * @param cachePath Caminho do arquivo de cache a ser verificado.
     * @param input Arquivo de dados original correspondente.
     * @return true Se o arquivo existir e for validado com sucesso; false caso
     * contrário.
     */
    bool TryLoadExistingCache(const std::string& cachePath, const File& input);

    // 2
    /**
     * @brief Constrói o índice ordenado externo (arquivo de cache) completo.
     *
     * Orquestra as etapas de ordenação externa:
     * 1. Gera arquivos temporários ordenados para cada página com
     * `CreateSortedPageFiles`.
     * 2. Intercala todos os arquivos parciais gerando o cache final com
     * `MergePageFiles`.
     * 3. Remove os arquivos intermediários temporários com `CleanupPageFiles`.
     * 4. Abre o arquivo de cache final consolidado em modo binário em `file_`.
     *
     * @param input Arquivo de dados de entrada.
     * @param cachePath Caminho final onde o arquivo de cache será gravado.
     */
    void BuildCache(File& input, const std::string& cachePath);

    // 3
    /**
     * @brief Cria e grava em disco um arquivo temporário com as entradas
     * ordenadas de uma página.
     *
     * Converte os itens da página em estruturas `Entry` com o `pageIndex`
     * correspondente, ordena as entradas em memória por chave via
     * `std::ranges::sort` e salva o array resultante em formato binário no
     * arquivo de caminho `pagePath`.
     * +
     * Executa a intercalação balanceada (*k-way merge*):
     * - Grava os metadados do arquivo de entrada no início do arquivo de cache
     * consolidado.
     * - Utiliza uma fila de prioridade (*min-heap*) para obter sequencialmente
     * o menor registro dentre as cabeças de leitura dos arquivos de cada página
     * (`RunReader`).
     * - Grava o menor elemento no arquivo final e avança a leitura no arquivo
     * de origem até processar todos os registros de todas as páginas.
     *
     * @param cachePath Caminho de destino do arquivo de cache final
     * consolidado.
     * @param input Arquivo de dados original para escrita dos metadados.
     */
    static void WritePageFile(const std::string& cachePath, const File& input);

    // 1
    /**
     * @brief Retorna o caminho padrão do arquivo de
     * cache para um arquivo de entrada.
     *
     * Anexa a extensão ".cache_01" ao caminho do
     * arquivo original.
     *
     * @param input Arquivo de dados original.
     * @return std::string Caminho completo do
     * arquivo de cache consolidado.
     */
    static std::string GetCachePath(const File& input);
};
}  // namespace Algorithm::IndexedSequentialAccess
