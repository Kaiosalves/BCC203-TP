#include "ISA.hpp"

#include "../Common.hpp"

using namespace Algorithm::IndexedSequentialAccess;

ISA::ISA(const std::shared_ptr<File>& input) : input_(input), cache_(*input) {
    if (!input_) {
        Log::Error("ISA: initialized with null file pointer");
    } else {
        Log::Info("ISA: initialized for file " + input_->path());
    }
}

std::optional<Item> ISA::Search(int key) {
    if (!input_) {
        Log::Error("ISA: cannot search, input file is null");
        return std::nullopt;
    }

    Log::Info("ISA: starting search for key " + std::to_string(key));

    // consulta o índice em disco para descobrir em qual página procurar a
    // chave.
    const auto indexEntry = cache_.Search(key);
    if (!indexEntry.has_value()) {
        Log::Info("ISA: key " + std::to_string(key) +
                  " not found in index cache");
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
        Log::Error("ISA: pageIndex " + std::to_string(pageIndex) +
                   " exceeds page count " + std::to_string(pageCount));
        return std::nullopt;
    }

    // Carrega do arquivo de dados a página indicada pelo índice.
    const uint64_t firstItem = pageIndex * PAGE_SIZE;
    const auto page = input_->GetPageAt(static_cast<std::size_t>(pageIndex));
    const uint64_t remainingItems = quantity - firstItem;
    const auto itemCount = static_cast<std::size_t>(
        remainingItems < PAGE_SIZE ? remainingItems : PAGE_SIZE);

    Log::Info("ISA: linearly searching " + std::to_string(itemCount) +
              " items in page " + std::to_string(pageIndex));

    // Percorre linearmente apenas os itens válidos desta página.
    for (std::size_t i = 0; i < itemCount; ++i) {
        if (page[i].key == key) {
            Log::Info("ISA: key " + std::to_string(key) + " found at page " +
                      std::to_string(pageIndex) + ", offset " +
                      std::to_string(i));
            return page[i];
        }
    }

    Log::Info("ISA: key " + std::to_string(key) + " not found in page " +
              std::to_string(pageIndex));
    return std::nullopt;
}
