#include "Cache.hpp"

#include <iostream>
#include <stdexcept>

#include "File.hpp"
#include "Item.hpp"

using namespace Algorithm::IndexedSequentialAccess;

Cache::Cache(File& input) {
    std::string const cachePath = GetCachePath(input);

    if (!TryLoadExistingCache(cachePath, input)) {
        BuildCache(input, cachePath);
    }
}

Cache::~Cache() {
    if (this->file_.is_open()) {
        this->file_.close();
    }
}

void Cache::BuildCache(File& input, const std::string& cachePath) {
    std::ofstream cacheFile(cachePath, std::ios::binary);
    if (!cacheFile.is_open()) {
        Log::Error("Não foi possível gerar a cache!");
        return;
    }
    // Escrevendo dados sobre os últimos acessos
    Metadata meta;
    meta.lastModification = input.lastModification();
    meta.size = input.size();
    cacheFile.write(reinterpret_cast<char*>(&meta), sizeof(Metadata));

    size_t index = 0;
    std::array<Item, PAGE_SIZE> page;

    Entry tmp;
    while (true) {
        // Lê a proxima página
        page = input.GetNextPage();
        if (input.eof()) {
            break;
        }
        // Grava a entrada no arquivo de cache
        tmp.key = page[0].key;
        tmp.pageIndex = index;

        cacheFile.write(reinterpret_cast<char*>(&tmp), sizeof(Entry));
        index++;
    }
    cacheFile.close();
    // this->file_(cachePath, std::ios::binary);
}

bool Cache::ValidateCache(const File& input) {
    // reposiciona o ponteiro pro inicio do arquivo de cache
    this->file_.seekg(0, std::ifstream::beg);

    // le e armazena os metadados do disco(tamanho e lastmodification)
    Metadata m;

    if (!this->file_.read((char*)&m, sizeof(Metadata))) {
        this->file_.clear();
        return false;
    }
    // variaveis que verificam se o cache tem os mesmos metadados
    bool sameModificationTime =
        (m.lastModification == input.lastModification());

    bool sameSize = (m.size == input.size());

    // retorna verdadeiro se eles tem os mesmos metadados, falso se nao
    return sameModificationTime && sameSize;
}

bool Cache::TryLoadExistingCache(const std::string& cachePath,
                                 const File& input) {
    if (!std::filesystem::exists(cachePath)) return false;

    this->file_.open(cachePath, std::ios::binary);

    if (!this->file_.is_open()) return false;

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
    // Verifica se o arq de indice esta aberto
    if (!this->file_.is_open()) {
        return std::nullopt;
    }

    // calcula o tam total do arquivo e ve se tem pelo menos os metadados
    this->file_.seekg(0, std::ifstream::end);
    auto const fileSize = this->file_.tellg();

    uint64_t const sizeInBytes = static_cast<uint64_t>(fileSize);

    if (sizeInBytes < sizeof(Metadata)) {
        return std::nullopt;
    }

    // calcula numero de paginas
    uint64_t const totalEntries =
        (sizeInBytes - sizeof(Metadata)) / sizeof(Entry);
    if (totalEntries == 0) {
        return std::nullopt;
    }

    int64_t lower = 0;
    int64_t higher = totalEntries - 1;

    std::optional<Entry> result = std::nullopt;

    // loop de busca
    while (lower <= higher) {
        int64_t mid = lower + (higher - lower) / 2;
        // deslocamento para a pag intermediaria
        std::streampos const offset = sizeof(Metadata) + (mid * sizeof(Entry));
        this->file_.seekg(offset, std::ifstream::beg);

        Entry entry;
        this->file_.read((char*)&entry, sizeof(Entry));

        if (entry.key <= key) {
            result = entry;
            // procura na metade superior
            lower = mid + 1;
        } else {
            // procura na metade inferior
            higher = mid - 1;
        }
    }
    return result;
}
