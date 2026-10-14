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
    explicit Cache(File& input);

    ~Cache();

    std::optional<Entry> Search(int key);

private:
    std::ifstream file_;

    // Check existing cache file data with it input
    // the checked fields are last modification time and size
    bool ValidateCache(const File& input);

    bool TryLoadExistingCache(const std::string& cachePath, const File& input);

    static void BuildCache(File& input, const std::string& cachePath);

    static void WritePageFile(const std::string& cachePath, const File& input);

    static std::string GetCachePath(const File& input);
};
}  // namespace Algorithm::IndexedSequentialAccess
