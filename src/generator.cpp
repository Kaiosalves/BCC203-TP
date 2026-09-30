#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "./Item.hpp"

struct Args {
    std::filesystem::path outputPath;
    int quantity;
    int swaps;
};

int main(int argc, char* argv[]) {
    Args args{
        .outputPath = std::filesystem::path(argv[0]).root_directory(),
        .quantity = 2000000,
        .swaps = 1000000,
    };

    auto currentArgIdx = 1;
    while (currentArgIdx < argc) {
        auto currentArg = std::string(argv[currentArgIdx]);

        if (currentArg == "-o") {
            args.outputPath = std::filesystem::path(argv[currentArgIdx + 1]);
            currentArgIdx += 2;
            continue;
        }

        if (currentArg == "-q") {
            args.quantity = std::atoi(argv[currentArgIdx + 1]);
            currentArgIdx += 2;
            continue;
        }

        if (currentArg == "-s") {
            args.swaps = std::atoi(argv[currentArgIdx + 1]);
            currentArgIdx += 2;
            continue;
        }

        std::cout << "USAGE: " << argv[0]
                  << " [-o PATH] [-q QUANTITY] [-s SWAPS]\n";
        return 0;
    }

    std::srand(std::time({}));

    std::fstream firstFile(
        args.outputPath / "items_ascending.bin",
        std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

    Item item;
    item.text.fill('\0');
    for (int i = 0; i < args.quantity; i++) {
        item.key = i;
        item.value = std::rand();
        firstFile.write(reinterpret_cast<char*>(&item), sizeof(Item));
    }

    std::ofstream secondFile(args.outputPath / "items_descending.bin",
                             std::ios::binary | std::ios::trunc);

    std::array<Item, 100> items;
    for (int i = 0; i < args.quantity / 100; i++) {
        firstFile.seekg(-i * sizeof(items), std::ios::end);
        firstFile.read(reinterpret_cast<char*>(&items), sizeof(items));

        std::ranges::reverse(items);

        secondFile.write(reinterpret_cast<char*>(&items), sizeof(items));
    }

    firstFile.close();
    secondFile.close();

    std::filesystem::copy_file(
        args.outputPath / "items_ascending.bin",
        args.outputPath / "items_shuffled.bin",
        std::filesystem::copy_options::overwrite_existing);

    std::fstream thirdFile(args.outputPath / "items_ascending.bin",
                           std::ios::binary | std::ios::in | std::ios::out);

    for (int i = 0; i < args.swaps; i++) {
        auto first = std::rand() % args.quantity;
        auto second = std::rand() % args.quantity;

        if (first == second) {
            second += (second < args.quantity - 1) ? 1 : -1;
        }

        Item firstItem;
        Item secondItem;

        thirdFile.seekg(first * sizeof(Item), std::ios::beg);
        thirdFile.read(reinterpret_cast<char*>(&firstItem), sizeof(Item));

        thirdFile.seekg(second * sizeof(Item), std::ios::beg);
        thirdFile.read(reinterpret_cast<char*>(&secondItem), sizeof(Item));

        thirdFile.seekg(first * sizeof(Item), std::ios::beg);
        thirdFile.write(reinterpret_cast<char*>(&secondItem), sizeof(Item));

        thirdFile.seekg(second * sizeof(Item), std::ios::beg);
        thirdFile.write(reinterpret_cast<char*>(&firstItem), sizeof(Item));
    }

    return 0;
}
