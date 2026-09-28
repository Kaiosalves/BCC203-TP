#include "ISA.hpp"

namespace Algorithm::IndexedSequentialAccess {

ISA::ISA(std::shared_ptr<File> input) : input_(input), cache_(*input) {}
}  // namespace Algorithm::IndexedSequentialAccess