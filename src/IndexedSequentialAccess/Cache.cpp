#include "Cache.hpp"

#include <iostream>
#include <stdexcept>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"

using namespace Algorithm::IndexedSequentialAccess;

Cache::Cache(File& input) {
    std::string const cachePath = GetCachePath(input);
    Log::Info("Cache: initializing index cache for " + input.path());

    if (!TryLoadExistingCache(cachePath, input)) {
        Log::Info(
            "Cache: existing cache not valid or missing, building new cache "
            "at " +
            cachePath);
        BuildCache(input, cachePath);
        this->file_.open(cachePath, std::ios::binary);
        if (!this->file_.is_open()) {
            Log::Error("Cache: failed to open cache file after build: " +
                       cachePath);
        } else {
            Log::Info("Cache: successfully opened cache file: " + cachePath);
        }
    } else {
        Log::Info("Cache: using existing valid cache: " + cachePath);
    }
}

Cache::~Cache() {
    if (this->file_.is_open()) {
        this->file_.close();
    }
}

void Cache::BuildCache(File& input, const std::string& cachePath) {
    Log::Info("Cache: building cache file: " + cachePath);
    std::ofstream cacheFile(cachePath, std::ios::binary);
    if (!cacheFile.is_open()) {
        Log::Error("Cache: failed to create cache file: " + cachePath);
        return;
    }
    // Escrevendo dados sobre os últimos acessos
    Metadata meta;
    meta.lastModification = input.lastModification();
    meta.size = input.size();
    cacheFile.write(reinterpret_cast<char*>(&meta), sizeof(Metadata));
    if (!cacheFile) {
        Log::Error("Cache: failed to write metadata header to: " + cachePath);
        return;
    }

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
            Log::Error("Cache: failed to write entry at page index " +
                       std::to_string(index) + " to: " + cachePath);
            return;
        }
        index++;
    }
    cacheFile.close();
    if (cacheFile.fail()) {
        Log::Error("Cache: failed to close cache file properly: " + cachePath);
        return;
    }
    Log::Info("Cache: index cache built successfully with " +
              std::to_string(index) + " entries");
}

bool Cache::ValidateCache(const File& input) {
    // reposiciona o ponteiro pro inicio do arquivo de cache
    this->file_.seekg(0, std::ifstream::beg);

    // le e armazena os metadados do disco(tamanho e lastmodification)
    Metadata cachedMetadata;

    if (!this->file_.read(reinterpret_cast<char*>(&cachedMetadata),
                          sizeof(Metadata))) {
        Log::Error("Cache: failed to read metadata from cache file");
        this->file_.clear();
        return false;
    }
    // variaveis que verificam se o cache tem os mesmos metadados
    bool const sameModificationTime =
        (cachedMetadata.lastModification == input.lastModification());

    bool const sameSize = (cachedMetadata.size == input.size());

    if (!sameModificationTime || !sameSize) {
        Log::Info("Cache: validation failed, input file modified or resized");
        return false;
    }

    Log::Info("Cache: validation succeeded");
    return true;
}

bool Cache::TryLoadExistingCache(const std::string& cachePath,
                                 const File& input) {
    if (!std::filesystem::exists(cachePath)) {
        Log::Info("Cache: file not found: " + cachePath);
        return false;
    }

    this->file_.open(cachePath, std::ios::binary);

    if (!this->file_.is_open()) {
        Log::Error("Cache: failed to open existing cache file: " + cachePath);
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
    Log::Info("Cache: searching for key " + std::to_string(key));
    // Verifica se o arq de indice esta aberto
    if (!this->file_.is_open()) {
        Log::Error("Cache: search failed, cache file is not open");
        return std::nullopt;
    }

    // calcula o tam total do arquivo e ve se tem pelo menos os metadados
    this->file_.seekg(0, std::ifstream::end);
    auto const fileSize = this->file_.tellg();

    auto const sizeInBytes = static_cast<uint64_t>(fileSize);

    if (sizeInBytes < sizeof(Metadata)) {
        Log::Error("Cache: file size (" + std::to_string(sizeInBytes) +
                   " bytes) is smaller than metadata header");
        return std::nullopt;
    }

    // calcula numero de paginas
    uint64_t const totalEntries =
        (sizeInBytes - sizeof(Metadata)) / sizeof(Entry);
    if (totalEntries == 0) {
        Log::Info("Cache: cache contains 0 entries");
        return std::nullopt;
    }

    Log::Info("Cache: binary search over " + std::to_string(totalEntries) +
              " entries");

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
            Log::Error("Cache: failed to read entry at index " +
                       std::to_string(mid));
            return std::nullopt;
        }

        Log::Info("Cache: binary search mid=" + std::to_string(mid) +
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
        Log::Info("Cache: found candidate page " +
                  std::to_string(result->pageIndex) + " with index key " +
                  std::to_string(result->key));
    } else {
        Log::Info("Cache: key " + std::to_string(key) +
                  " is smaller than all indexed keys");
    }

    return result;
}
