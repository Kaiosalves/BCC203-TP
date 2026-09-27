#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "Common.hpp"
#include "File.hpp"
#include "IndexedSequentialAccess/ISA.hpp"

enum Method : uint8_t {
    IndexedSequencialAccess = 1,
    BinaryTree = 2,
    BTree = 3,
    BStarTree = 4,
};

enum FileType : uint8_t {
    Ascending = 1,
    Descending = 2,
    Unordered = 3,
};

int main(int argc, char* argv[]) {
    constexpr int requiredArgC = 5;
    if (argc < requiredArgC) {
        Log::Error("Use pesquisa <method> <quantity> <situation> <key> [-P]");
        return -1;
    }

    constexpr int optionalLastArgPos = 5;
    if (argc == (optionalLastArgPos + 1) &&
        std::string(argv[optionalLastArgPos]) == std::string("-P")) {
        Log::enableInfo = true;
    }

    long int const quantity = std::atol(argv[2]);
    constexpr int maxFileSize = 2000000;
    if (quantity > maxFileSize || quantity <= 0) {
        Log::Error(
            "<quantity> must be a value grater than 0 and equal or "
            "less than 2000000");
        return -1;
    }

    int const situationValue = std::atoi(argv[3]);
    if (situationValue < FileType::Ascending ||
        situationValue > FileType::Unordered) {
        Log::Error(
            "<situation> must be one of these: 1 -> ascending file; 2 -> "
            "descending file; 3 -> unordered file");
        return -1;
    }

    std::string fileTypeName;
    switch (static_cast<FileType>(situationValue)) {
        case Ascending:
            fileTypeName = "items_ascending.bin";
            break;
        case Descending:
            fileTypeName = "items_descending.bin";
            break;
        case Unordered:
            fileTypeName = "items_shuffled.bin";
            break;
    }

    // std::filesystem is used because the old implementation had an issue with
    // the location of the executable
    // The generator create the file at ./tmp but the old code searched at
    // ./build/tmp
    // I've update it to handle these two cases.
    std::filesystem::path filePath = "tmp/" + fileTypeName;
    if (!std::filesystem::exists(filePath)) {
        const auto execPath = std::filesystem::path(argv[0]);
        const auto candidateParent =
            execPath.parent_path() / ".." / "tmp" / fileTypeName;
        const auto candidateSame =
            execPath.parent_path() / "tmp" / fileTypeName;

        if (std::filesystem::exists(candidateParent)) {
            filePath = candidateParent;
        } else if (std::filesystem::exists(candidateSame)) {
            filePath = candidateSame;
        }
    }
    std::shared_ptr<File> const file =
        std::make_shared<File>(filePath.string(), quantity);

    int const key = std::atoi(argv[4]);
    if (key < 0 || key > maxFileSize - 1) {
        Log::Error("<key> must be greater or equal to 0 and less than 2000000");
        return -1;
    }

    auto method = std::atoi(argv[1]);
    if (method == Method::IndexedSequencialAccess) {
        if (situationValue != 1) {
            Log::Error(
                "Input file state is unsuported for Indexed Sequencial Access");
            return -1;
        }

        Log::Info("Starting Indexed Sequential Access search for key " +
                  std::to_string(key));
        const auto preprocessStart = std::chrono::high_resolution_clock::now();
        auto isa = Algorithm::IndexedSequentialAccess::ISA(file);
        const auto preprocessEnd = std::chrono::high_resolution_clock::now();

        const auto searchStart = std::chrono::high_resolution_clock::now();
        const auto res = isa.Search(key);
        const auto searchEnd = std::chrono::high_resolution_clock::now();

        const std::chrono::duration<double, std::milli> preprocessDuration =
            preprocessEnd - preprocessStart;
        const std::chrono::duration<double, std::milli> searchDuration =
            searchEnd - searchStart;

        if (!res.has_value()) {
            Log::Info("Search finished: key " + std::to_string(key) +
                      " not found");
            std::cout << "Key not found\n";
        } else {
            Log::Info("Search finished: key " + std::to_string(key) + " found");
            std::cout << "Found key: " << res.value() << "\n";
        }

        std::cout << "Preprocessing time: " << preprocessDuration.count()
                  << " ms\n";
        std::cout << "Search time: " << searchDuration.count() << " ms\n";
        return 0;
    }

    Log::Error(
        "Error: <method>, must be one of these: 1 -> indexed "
        "sequencial access; 2 -> binary tree; 3 -> B tree; 4 -> B* "
        "tree");
    return -1;
}
