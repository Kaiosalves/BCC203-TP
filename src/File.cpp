#include "File.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "Common.hpp"

File::File(const std::string& path, uint64_t quantity)
    : path_(path), quantity_(quantity) {
    this->file_ = std::ifstream(path, std::ios::binary);

    if (!this->file_.is_open()) {
        throw std::runtime_error("Failed to open file");
    }
}

File::~File() { this->file_.close(); }

std::array<Item, PAGE_SIZE> File::GetNextPage() {
    std::array<Item, PAGE_SIZE> page{};

    if (this->eof()) {
        Log::Error("Invalid input file access index");
        return page;
    }

    auto const pos = this->file_.tellg();
    uint64_t toRead = PAGE_SIZE;
    if (pos >= 0) {
        uint64_t const currentItem = static_cast<uint64_t>(pos) / sizeof(Item);
        if (currentItem < this->quantity_) {
            uint64_t const remaining = this->quantity_ - currentItem;
            toRead = std::min(static_cast<uint64_t>(PAGE_SIZE), remaining);
        } else {
            toRead = 0;
        }
    }

    if (toRead > 0) {
        this->file_.read(reinterpret_cast<char*>(page.data()),
                         sizeof(Item) * toRead);
    }

    return page;
}

std::array<Item, PAGE_SIZE> File::GetPageAt(size_t index) {
    Log::Info("Reading page " + std::to_string(index) + " from file (" +
              this->path_ + ")");
    std::array<Item, PAGE_SIZE> page{};

    if (this->file_.eof() || this->file_.fail()) {
        this->file_.clear();
    }

    auto oldPos = this->file_.tellg();

    if (index * PAGE_SIZE >= this->quantity_) {
        Log::Error("Invalid input file access index");
        return page;
    }

    this->file_.seekg(sizeof(Item) * PAGE_SIZE * index, std::ifstream::beg);
    page = this->GetNextPage();

    if (oldPos >= 0) {
        this->file_.seekg(oldPos);
    }
    return page;
}

std::string File::path() const { return this->path_; }

std::filesystem::file_time_type File::lastModification() const {
    return std::filesystem::last_write_time(this->path_);
}

uint64_t File::size() const { return std::filesystem::file_size(this->path_); }

uint64_t File::quantity() const { return this->quantity_; }

bool File::eof() {
    if (this->file_.eof()) {
        return true;
    }

    auto const pos = this->file_.tellg();
    if (pos < 0) {
        return true;
    }

    return (static_cast<uint64_t>(pos) / sizeof(Item)) >= this->quantity_;
}
