#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "Common.hpp"
#include "Item.hpp"

class File {
public:
    File(const std::string& path, uint64_t quantity);
    ~File();

    std::array<Item, PAGE_SIZE> GetNextPage();
    std::array<Item, PAGE_SIZE> GetPageAt(size_t index);

    std::string path() const;
    std::filesystem::file_time_type lastModification() const;
    uint64_t size() const;
    uint64_t quantity() const;
    bool eof();

private:
    std::ifstream file_;
    std::string path_;

    uint64_t quantity_;
};
