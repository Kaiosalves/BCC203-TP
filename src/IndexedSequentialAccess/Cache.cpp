#include "Cache.hpp"

#include <iostream>
#include <stdexcept>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"

using namespace Algorithm::IndexedSequentialAccess;

Cache::Cache(File& input) {
    std::string const cachePath = GetCachePath(input);
    Log::Info("IndexedSequentialAccess: initializing index cache for " +
              input.path());

    if (!TryLoadExistingCache(cachePath, input)) {
        Log::Info(
            "IndexedSequentialAccess: existing cache not valid or missing, "
            "building new cache at " +
            cachePath);
        BuildCache(input, cachePath);
        this->file_.open(cachePath, std::ios::binary);
        if (!this->file_.is_open()) {
            Log::Error(
                "IndexedSequentialAccess: failed to open cache file after "
                "build: " +
                cachePath);
        } else {
            Log::Info(
                "IndexedSequentialAccess: successfully opened cache file: " +
                cachePath);
        }
    } else {
        Log::Info("IndexedSequentialAccess: using existing valid cache: " +
                  cachePath);
    }
}

Cache::~Cache() {
    if (this->file_.is_open()) {
        this->file_.close();
    }
}

void Cache::BuildCache(File& input, const std::string& cachePath) {
    Log::Info("IndexedSequentialAccess: building cache file: " + cachePath);
    std::ofstream cacheFile(cachePath, std::ios::binary);
    if (!cacheFile.is_open()) {
        Log::Error("IndexedSequentialAccess: failed to create cache file: " +
                   cachePath);
        return;
    }
    // Escrevendo dados sobre os últimos acessos
    Metadata meta;
    meta.lastModification = input.lastModification();
    meta.size = input.size();
    cacheFile.write(reinterpret_cast<char*>(&meta), sizeof(Metadata));
    if (!cacheFile) {
        Log::Error(
            "IndexedSequentialAccess: failed to write metadata header to: " +
            cachePath);
        return;
    }

    const uint64_t totalPages = (input.quantity() + PAGE_SIZE - 1) / PAGE_SIZE;
    const uint64_t logInterval =
        totalPages <= 100 ? 1 : std::max(uint64_t{1}, totalPages / 20);

    size_t index = 0;
    std::array<Item, PAGE_SIZE> page;

    Entry tmp;
    while (index * PAGE_SIZE < input.quantity() && !input.eof()) {
        // Lê a proxima página
        page = input.GetNextPage();

        // Grava a entrada no arquivo de cache
        tmp.key = page[0].key;
        tmp.pageIndex = index;

        cacheFile.write(reinterpret_cast<char*>(&tmp), sizeof(Entry));
        if (!cacheFile) {
            Log::Error(
                "IndexedSequentialAccess: failed to write entry at page "
                "index " +
                std::to_string(index) + " to: " + cachePath);
            return;
        }

        if ((index + 1) % logInterval == 0 || index + 1 == totalPages) {
            Log::Info(
                "IndexedSequentialAccess: building cache - processed "
                "page " +
                std::to_string(index + 1) + "/" + std::to_string(totalPages) +
                " (indexed key " + std::to_string(tmp.key) + ")");
        }

        index++;
    }
    cacheFile.close();
    if (cacheFile.fail()) {
        Log::Error(
            "IndexedSequentialAccess: failed to close cache file properly: " +
            cachePath);
        return;
    }
    Log::Info("IndexedSequentialAccess: index cache built successfully with " +
              std::to_string(index) + " entries");
}

bool Cache::ValidateCache(const File& input) {
    // reposiciona o ponteiro pro inicio do arquivo de cache
    this->file_.seekg(0, std::ifstream::beg);

    // le e armazena os metadados do disco(tamanho e lastmodification)
    Metadata cachedMetadata;

    if (!this->file_.read(reinterpret_cast<char*>(&cachedMetadata),
                          sizeof(Metadata))) {
        Log::Error(
            "IndexedSequentialAccess: failed to read metadata from cache file");
        this->file_.clear();
        return false;
    }
    // variaveis que verificam se o cache tem os mesmos metadados
    bool const sameModificationTime =
        (cachedMetadata.lastModification == input.lastModification());

    bool const sameSize = (cachedMetadata.size == input.size());

    if (!sameModificationTime || !sameSize) {
        Log::Info(
            "IndexedSequentialAccess: validation failed, input file modified "
            "or resized");
        return false;
    }

    Log::Info("IndexedSequentialAccess: validation succeeded");
    return true;
}

bool Cache::TryLoadExistingCache(const std::string& cachePath,
                                 const File& input) {
    if (!std::filesystem::exists(cachePath)) {
        Log::Info("IndexedSequentialAccess: file not found: " + cachePath);
        return false;
    }

    this->file_.open(cachePath, std::ios::binary);

    if (!this->file_.is_open()) {
        Log::Error(
            "IndexedSequentialAccess: failed to open existing cache file: " +
            cachePath);
        return false;
    }

    if (!this->ValidateCache(input)) {
        this->file_.close();
        return false;
    }

    return true;
}

std::string Cache::GetCachePath(const File& input) {
    return input.path() + ".cache_01";
}

std::optional<Cache::Entry> Cache::Search(int key) {
    Log::Info("IndexedSequentialAccess: searching for key " +
              std::to_string(key));
    // Verifica se o arq de indice esta aberto
    if (!this->file_.is_open()) {
        Log::Error(
            "IndexedSequentialAccess: search failed, cache file is not open");
        return std::nullopt;
    }

    // calcula o tam total do arquivo e ve se tem pelo menos os metadados
    this->file_.seekg(0, std::ifstream::end);
    auto const fileSize = this->file_.tellg();

    auto const sizeInBytes = static_cast<uint64_t>(fileSize);

    if (sizeInBytes < sizeof(Metadata)) {
        Log::Error("IndexedSequentialAccess: file size (" +
                   std::to_string(sizeInBytes) +
                   " bytes) is smaller than metadata header");
        return std::nullopt;
    }

    // calcula numero de paginas
    uint64_t const totalEntries =
        (sizeInBytes - sizeof(Metadata)) / sizeof(Entry);
    if (totalEntries == 0) {
        Log::Info("IndexedSequentialAccess: cache contains 0 entries");
        return std::nullopt;
    }

    Log::Info("IndexedSequentialAccess: binary search over " +
              std::to_string(totalEntries) + " entries");

    int64_t lower = 0;
    int64_t higher = static_cast<int64_t>(totalEntries) - 1;

    std::optional<Entry> result = std::nullopt;

    // loop de busca
    while (lower <= higher) {
        int64_t const mid = lower + ((higher - lower) / 2);
        // deslocamento para a pag intermediaria
        std::streampos const offset =
            static_cast<std::streamoff>(sizeof(Metadata)) +
            static_cast<std::streamoff>(mid * sizeof(Entry));
        this->file_.seekg(offset, std::ifstream::beg);

        Entry entry;
        if (!this->file_.read(reinterpret_cast<char*>(&entry), sizeof(Entry))) {
            Log::Error(
                "IndexedSequentialAccess: failed to read entry at index " +
                std::to_string(mid));
            return std::nullopt;
        }

        Log::Info("IndexedSequentialAccess: binary search mid=" +
                  std::to_string(mid) +
                  ", entry.key=" + std::to_string(entry.key) +
                  ", pageIndex=" + std::to_string(entry.pageIndex));

        if (entry.key <= key) {
            result = entry;
            // procura na metade superior
            lower = mid + 1;
        } else {
            // procura na metade inferior
            higher = mid - 1;
        }
    }

    if (result.has_value()) {
        Log::Info("IndexedSequentialAccess: found candidate page " +
                  std::to_string(result->pageIndex) + " with index key " +
                  std::to_string(result->key));
    } else {
        Log::Info("IndexedSequentialAccess: key " + std::to_string(key) +
                  " is smaller than all indexed keys");
    }

    return result;
}
