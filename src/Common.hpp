#pragma once

#include <cstdint>
#include <string>

inline constexpr int PAGE_SIZE = 100;

class Log {
public:
    Log() = delete;

    static void Info(const std::string& msg);
    static void Error(const std::string& msg);

    inline static bool enableInfo = false;
};

enum class Phase : uint8_t {
    Preprocessing,
    Search,
};

struct PhaseMetrics {
    uint64_t diskReads = 0;
    uint64_t keyComparisons = 0;
};

class Metrics {
public:
    Metrics() = delete;

    static void SetPhase(Phase phase) { currentPhase_ = phase; }
    static Phase GetPhase() { return currentPhase_; }
    static void Reset() {
        preprocess_ = {};
        search_ = {};
        currentPhase_ = Phase::Preprocessing;
    }

    static void RecordDiskRead(uint64_t count = 1) {
        if (currentPhase_ == Phase::Preprocessing) {
            preprocess_.diskReads += count;
        } else {
            search_.diskReads += count;
        }
    }

    static void RecordKeyComparison(uint64_t count = 1) {
        if (currentPhase_ == Phase::Preprocessing) {
            preprocess_.keyComparisons += count;
        } else {
            search_.keyComparisons += count;
        }
    }

    static const PhaseMetrics& Preprocessing() { return preprocess_; }
    static const PhaseMetrics& Search() { return search_; }

private:
    inline static Phase currentPhase_ = Phase::Preprocessing;
    inline static PhaseMetrics preprocess_{};
    inline static PhaseMetrics search_{};
};
