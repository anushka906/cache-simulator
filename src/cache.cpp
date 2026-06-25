#include "cache.h"

#include <algorithm>
#include <stdexcept>

namespace cachesim {
namespace {
bool is_power_of_two(std::uint64_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}
}

std::size_t DirectMappedCache::checked_line_count(std::uint64_t capacity,
                                                 std::uint64_t block) {
    if (!is_power_of_two(capacity) || !is_power_of_two(block)) {
        throw std::invalid_argument("cache size and block size must be nonzero powers of two");
    }
    if (block > capacity) {
        throw std::invalid_argument("block size cannot exceed cache size");
    }
    // Prevent accidental huge allocations from command-line input.
    constexpr std::uint64_t max_lines = 1ULL << 20;
    const auto count = capacity / block;
    if (count > max_lines) {
        throw std::invalid_argument("cache configuration exceeds the limit of 1,048,576 lines");
    }
    return static_cast<std::size_t>(count);
}

DirectMappedCache::DirectMappedCache(std::uint64_t capacity_bytes,
                                     std::uint64_t block_bytes)
    : capacity_bytes_(capacity_bytes), block_bytes_(block_bytes),
      lines_(checked_line_count(capacity_bytes, block_bytes)) {}

AccessResult DirectMappedCache::access(std::uint64_t address) {
    const auto block = address / block_bytes_;
    const auto index = static_cast<std::size_t>(block % lines_.size());
    const auto tag = block / lines_.size();
    auto& line = lines_[index];
    const bool hit = line.valid && line.tag == tag;
    const bool replacement = line.valid && !hit;
    if (hit) {
        ++stats_.hits;
    } else {
        ++stats_.misses;
        line.valid = true;
        line.tag = tag;
    }
    return {hit, replacement, block, index, tag, address % block_bytes_};
}

void DirectMappedCache::reset() {
    std::fill(lines_.begin(), lines_.end(), Line{});
    stats_ = Statistics{};
}
} // namespace cachesim
