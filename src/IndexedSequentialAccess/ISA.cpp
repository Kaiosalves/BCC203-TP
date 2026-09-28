#include "ISA.hpp"

<<<<<<< HEAD
    // 2
    /**
     * @brief Executa a pesquisa de um registro pela chave usando Acesso
     * Sequencial Indexado.
     *
     * Consulta primeiramente a tabela de índice em disco através de
     * `cache_.Search(key)` para localizar em qual página do arquivo de dados a
     * chave reside:
     * - Se a chave não for encontrada no índice, encerra a busca retornando
     * vazio.
     * - Se for encontrada, recupera o `pageIndex` correspondente e carrega essa
     * página específica do arquivo de dados para a memória principal com
     * `input_->GetPageAt(...)`.
     * - Realiza a busca linear nos itens contidos na página em memória.
     *
     * @param key Chave numérica inteira a ser pesquisada.
     * @return std::optional<Item> O item correspondente caso encontrado na
     * página; caso contrário, std::nullopt (vazio).
     */
std::optional<Item> Algorithm::IndexedSequentialAccess::ISA::Search(int key) {
    if (!input_) {
        return std::nullopt;
    }

    // consulta o índice em disco para descobrir em qual página procurar a chave.
    const auto indexEntry = cache_.Search(key);
    if (!indexEntry.has_value()) {
        return std::nullopt;
    }

    const uint64_t pageIndex = indexEntry->pageIndex;

    // quantidade de itens no arquivo
    const uint64_t quantity = input_->quantity();

    // Calcula quantas páginas existem, incluindo uma possível última página parcial
    const uint64_t pageCount = quantity / PAGE_SIZE + (quantity % PAGE_SIZE != 0);
    if (pageIndex >= pageCount) {
        return std::nullopt;
    }

    // Carrega do arquivo de dados a página indicada pelo índice.
    const uint64_t firstItem = pageIndex * PAGE_SIZE;
    const auto page = input_->GetPageAt(static_cast<std::size_t>(pageIndex));
    const uint64_t remainingItems = quantity - firstItem;
    const std::size_t itemCount = static_cast<std::size_t>(
        remainingItems < PAGE_SIZE ? remainingItems : PAGE_SIZE);

    // Percorre linearmente apenas os itens válidos desta página.
    for (std::size_t i = 0; i < itemCount; ++i) {
        if (page[i].key == key) {
            return page[i];
        }
    }

    return std::nullopt;
}
=======
namespace Algorithm::IndexedSequentialAccess {

ISA::ISA(std::shared_ptr<File> input) : input_(input), cache_(*input) {}
}  // namespace Algorithm::IndexedSequentialAccess
>>>>>>> master
