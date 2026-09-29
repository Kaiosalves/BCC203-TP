#include "ISA.hpp"

using namespace Algorithm::IndexedSequentialAccess;

std::optional<Item> ISA::Search(int key) {
    if (!input_) {
        return std::nullopt;
    }

    // consulta o índice em disco para descobrir em qual página procurar a
    // chave.
    const auto indexEntry = cache_.Search(key);
    if (!indexEntry.has_value()) {
        return std::nullopt;
    }

    const uint64_t pageIndex = indexEntry->pageIndex;

    // quantidade de itens no arquivo
    const uint64_t quantity = input_->quantity();

    // Calcula quantas páginas existem, incluindo uma possível última página
    // parcial
    const uint64_t pageCount = (quantity / PAGE_SIZE) +
                               static_cast<uint64_t>(quantity % PAGE_SIZE != 0);
    if (pageIndex >= pageCount) {
        return std::nullopt;
    }

    // Carrega do arquivo de dados a página indicada pelo índice.
    const uint64_t firstItem = pageIndex * PAGE_SIZE;
    const auto page = input_->GetPageAt(static_cast<std::size_t>(pageIndex));
    const uint64_t remainingItems = quantity - firstItem;
    const auto itemCount = static_cast<std::size_t>(
        remainingItems < PAGE_SIZE ? remainingItems : PAGE_SIZE);

    // Percorre linearmente apenas os itens válidos desta página.
    for (std::size_t i = 0; i < itemCount; ++i) {
        if (page[i].key == key) {
            return page[i];
        }
    }

    return std::nullopt;
}

ISA::ISA(const std::shared_ptr<File>& input) : input_(input), cache_(*input) {}
