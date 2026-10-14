#pragma once

#include <optional>

#include "../File.hpp"
#include "../Item.hpp"
#include "Cache.hpp"

namespace Algorithm::IndexedSequentialAccess {
class ISA {
public:
    explicit ISA(const std::shared_ptr<File>& input);

    ~ISA() = default;

    std::optional<Item> Search(int key);

private:
    std::shared_ptr<File> input_;
    Cache cache_;
};
}  // namespace Algorithm::IndexedSequentialAccess
