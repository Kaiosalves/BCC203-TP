#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>

#include "../Common.hpp"
#include "../File.hpp"
#include "../Item.hpp"

namespace Algorithm::BinaryTree {
class BTreeFile {
public:
    struct Node {
        int key;
        uint64_t pageIndex;
        uint64_t left = 0;
        uint64_t right = 0;
    };

    struct Metadata {
        std::filesystem::file_time_type lastModification;
        uint64_t size;
    };

    explicit BTreeFile(File& input);

    ~BTreeFile();

    std::optional<Node> Search(int key);

private:
    std::ifstream file_;

    static std::string GetFilePath(const File& input);

    static std::streamoff GetNodeOffset(uint64_t nodeIndex);

    static void WriteNode(std::fstream& file, uint64_t nodeIndex,
                          const Node& node);

    static bool ReadNode(std::istream& file, uint64_t nodeIndex, Node& node);

    static uint64_t AppendNode(std::fstream& file, const Node& node,
                               uint64_t& lastNodeIndex);

    static void WriteMetadata(std::fstream& file, const File& input);

    static void InitializeRoot(std::fstream& file, File& input);

    static void PopulateTree(std::fstream& file, File& input,
                             uint64_t& lastNodeIndex);

    static void InsertPage(std::fstream& file,
                           const std::array<Item, PAGE_SIZE>& page,
                           uint64_t pageIndex, size_t itemCount,
                           uint64_t& lastNodeIndex);

    static void InsertItem(std::fstream& file, const Item& item,
                           uint64_t pageIndex, uint64_t& lastNodeIndex);

    bool TryLoadExistingFile(const std::string& path, const File& input);

    bool ValidateFile(const File& input);

    void BuildFile(File& input, const std::string& path);
};
}  // namespace Algorithm::BinaryTree
