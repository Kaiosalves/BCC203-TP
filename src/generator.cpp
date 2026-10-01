#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Common.hpp"
#include "Item.hpp"

struct Args {
    std::filesystem::path outputPath;
    int quantity;
    int swaps;
};

int main(int argc, char* argv[]) {
    Log::enableInfo = true;

    Args args{
        .outputPath = std::filesystem::path(argv[0]).root_directory(),
        .quantity = 2000000,
        .swaps = 1000000,
    };

    auto currentArgIdx = 1;
    while (currentArgIdx < argc) {
        auto currentArg = std::string(argv[currentArgIdx]);

        if (currentArg == "-o" && currentArgIdx + 1 < argc) {
            args.outputPath = std::filesystem::path(argv[currentArgIdx + 1]);
            currentArgIdx += 2;
            continue;
        }

        if (currentArg == "-q" && currentArgIdx + 1 < argc) {
            args.quantity = std::atoi(argv[currentArgIdx + 1]);
            currentArgIdx += 2;
            continue;
        }

        if (currentArg == "-s" && currentArgIdx + 1 < argc) {
            args.swaps = std::atoi(argv[currentArgIdx + 1]);
            currentArgIdx += 2;
            continue;
        }

        Log::Error("USAGE: " + std::string(argv[0]) +
                   " [-o PATH] [-q QUANTITY] [-s SWAPS]");
        return 0;
    }

    if (args.quantity <= 0) {
        Log::Error("<quantity> must be greater than 0");
        return -1;
    }

    if (args.swaps < 0) {
        Log::Error("<swaps> must be greater than or equal to 0");
        return -1;
    }

    std::error_code ec;
    if (!args.outputPath.empty() && !std::filesystem::exists(args.outputPath)) {
        std::filesystem::create_directories(args.outputPath, ec);
        if (ec) {
            Log::Error("Failed to create output directory: " +
                       args.outputPath.string());
            return -1;
        }
    }

    Log::Info("Starting data generation in " + args.outputPath.string() +
              " (quantity=" + std::to_string(args.quantity) +
              ", swaps=" + std::to_string(args.swaps) + ")");

    std::srand(std::time({}));

    const auto ascPath = args.outputPath / "items_ascending.bin";
    Log::Info("Generating ascending file: " + ascPath.string());

    std::fstream firstFile(ascPath, std::ios::binary | std::ios::in |
                                        std::ios::out | std::ios::trunc);
    if (!firstFile.is_open()) {
        Log::Error("Failed to open file: " + ascPath.string());
        return -1;
    }

    Item item;
    item.text.fill('\0');
    for (int i = 0; i < args.quantity; i++) {
        item.key = i;
        item.value = std::rand();
        firstFile.write(reinterpret_cast<char*>(&item), sizeof(Item));
        if (args.quantity >= 500000 &&
            ((i + 1) % (args.quantity / 5) == 0 || i + 1 == args.quantity)) {
            Log::Info(
                "Generating ascending file progress: " + std::to_string(i + 1) +
                "/" + std::to_string(args.quantity) + " items (" +
                std::to_string((static_cast<int64_t>(i + 1) * 100) /
                               args.quantity) +
                "%)");
        }
    }
    firstFile.flush();
    Log::Info("Ascending file generated successfully (" +
              std::to_string(args.quantity) + " items)");

    const auto descPath = args.outputPath / "items_descending.bin";
    Log::Info("Generating descending file: " + descPath.string());

    std::ofstream secondFile(descPath, std::ios::binary | std::ios::trunc);
    if (!secondFile.is_open()) {
        Log::Error("Failed to open file: " + descPath.string());
        return -1;
    }

    std::array<Item, PAGE_SIZE> items;
    firstFile.clear();

    const int numPages = args.quantity / PAGE_SIZE;
    const int remainder = args.quantity % PAGE_SIZE;
    if (remainder > 0) {
        std::vector<Item> remItems(remainder);
        firstFile.seekg(-static_cast<std::streamoff>(remainder * sizeof(Item)),
                        std::ios::end);
        firstFile.read(reinterpret_cast<char*>(remItems.data()),
                       remainder * sizeof(Item));
        std::ranges::reverse(remItems);
        secondFile.write(reinterpret_cast<char*>(remItems.data()),
                         remainder * sizeof(Item));
    }
    for (int i = 0; i < numPages; i++) {
        firstFile.seekg(-static_cast<std::streamoff>(
                            (remainder + ((i + 1) * PAGE_SIZE)) * sizeof(Item)),
                        std::ios::end);
        firstFile.read(reinterpret_cast<char*>(items.data()), sizeof(items));

        std::ranges::reverse(items);

        secondFile.write(reinterpret_cast<char*>(items.data()), sizeof(items));

        if (numPages >= 5000 &&
            ((i + 1) % (numPages / 5) == 0 || i + 1 == numPages)) {
            Log::Info(
                "Generating descending file progress: " +
                std::to_string(i + 1) + "/" + std::to_string(numPages) +
                " pages (" +
                std::to_string((static_cast<int64_t>(i + 1) * 100) / numPages) +
                "%)");
        }
    }

    firstFile.close();
    secondFile.close();
    Log::Info("Descending file generated successfully (" +
              std::to_string(args.quantity) + " items)");

    const auto shufPath = args.outputPath / "items_shuffled.bin";
    Log::Info("Generating shuffled file: " + shufPath.string());

    std::filesystem::copy_file(
        ascPath, shufPath, std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        Log::Error("Failed to copy ascending file to shuffled file: " +
                   ec.message());
        return -1;
    }

    Log::Info("Applying " + std::to_string(args.swaps) +
              " swaps to shuffled file");

    std::fstream thirdFile(shufPath,
                           std::ios::binary | std::ios::in | std::ios::out);
    if (!thirdFile.is_open()) {
        Log::Error("Failed to open file: " + shufPath.string());
        return -1;
    }

    for (int i = 0; i < args.swaps; i++) {
        auto first = std::rand() % args.quantity;
        auto second = std::rand() % args.quantity;

        if (first == second) {
            second += (second < args.quantity - 1) ? 1 : -1;
        }

        Item firstItem;
        Item secondItem;

        thirdFile.seekg(static_cast<std::streamoff>(first) * sizeof(Item),
                        std::ios::beg);
        thirdFile.read(reinterpret_cast<char*>(&firstItem), sizeof(Item));

        thirdFile.seekg(static_cast<std::streamoff>(second) * sizeof(Item),
                        std::ios::beg);
        thirdFile.read(reinterpret_cast<char*>(&secondItem), sizeof(Item));

        thirdFile.seekp(static_cast<std::streamoff>(first) * sizeof(Item),
                        std::ios::beg);
        thirdFile.write(reinterpret_cast<char*>(&secondItem), sizeof(Item));

        thirdFile.seekp(static_cast<std::streamoff>(second) * sizeof(Item),
                        std::ios::beg);
        thirdFile.write(reinterpret_cast<char*>(&firstItem), sizeof(Item));

        if (args.swaps >= 10000 &&
            ((i + 1) % (args.swaps / 10) == 0 || i + 1 == args.swaps)) {
            Log::Info("Applying swaps progress: " + std::to_string(i + 1) +
                      "/" + std::to_string(args.swaps) + " swaps (" +
                      std::to_string((static_cast<int64_t>(i + 1) * 100) /
                                     args.swaps) +
                      "%)");
        }
    }

    thirdFile.close();
    Log::Info("Shuffled file generated successfully (" +
              std::to_string(args.swaps) + " swaps applied)");

    Log::Info("Data generation completed successfully");
    return 0;
}
