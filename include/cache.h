#ifndef CACHE_SIMULATOR_CACHE_H
#define CACHE_SIMULATOR_CACHE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cachesim {

struct Statistics {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t accesses() const { return hits + misses; }
    double hit_rate() const {
        return accesses() == 0 ? 0.0 : 100.0 * static_cast<double>(hits) / accesses();
    }
};

struct AccessResult {
    bool hit;
    bool replacement;
    std::uint64_t block_number;
    std::size_t index;
    std::uint64_t tag;
    std::uint64_t offset;
};

// A read-only, direct-mapped cache. Addresses and sizes are measured in bytes.
// Only tags and validity are stored: actual memory data and timing are not modelled.
class DirectMappedCache {
public:
    DirectMappedCache(std::uint64_t capacity_bytes, std::uint64_t block_bytes);
    AccessResult access(std::uint64_t address);
    void reset();
    const Statistics& statistics() const { return stats_; }
    std::size_t line_count() const { return lines_.size(); }
    std::uint64_t capacity_bytes() const { return capacity_bytes_; }
    std::uint64_t block_bytes() const { return block_bytes_; }

private:
    struct Line {
        bool valid = false;
        std::uint64_t tag = 0;
    };
    static std::size_t checked_line_count(std::uint64_t capacity, std::uint64_t block);
    std::uint64_t capacity_bytes_;
    std::uint64_t block_bytes_;
    std::vector<Line> lines_;
    Statistics stats_;
};

} // namespace cachesim
#endif
