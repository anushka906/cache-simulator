#include "cache.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void example_trace() {
    cachesim::DirectMappedCache cache(64, 16);
    const std::vector<std::uint64_t> addresses{0, 4, 16, 0, 64, 0};
    const std::vector<bool> expected{false, true, false, true, false, false};
    for (std::size_t i = 0; i < addresses.size(); ++i)
        require(cache.access(addresses[i]).hit == expected[i], "example hit/miss mismatch");
    require(cache.statistics().hits == 2 && cache.statistics().misses == 4, "example totals");
    require(std::abs(cache.statistics().hit_rate() - 100.0 / 3) < 1e-9, "example hit rate");
}
void block_boundaries() {
    cachesim::DirectMappedCache cache(64, 16);
    require(!cache.access(15).hit, "first block should miss");
    const auto same = cache.access(0);
    require(same.hit && same.offset == 0, "same block should hit");
    const auto next = cache.access(16);
    require(!next.hit && next.index == 1 && next.offset == 0, "next block boundary");
    require(cache.access(31).hit, "last byte in second block");
}
void conflicts_and_tags() {
    cachesim::DirectMappedCache cache(64, 16);
    const auto cold = cache.access(0);
    require(!cold.hit && !cold.replacement, "cold miss is not replacement");
    const auto conflict = cache.access(64);
    require(!conflict.hit && conflict.replacement && conflict.index == 0 && conflict.tag == 1,
            "conflict must replace a different tag");
    require(!cache.access(0).hit, "evicted block must miss");
    cachesim::DirectMappedCache larger(128, 16);
    larger.access(0); larger.access(64);
    require(larger.access(0).hit && larger.access(64).hit, "larger cache separates these blocks");
}
void empty_and_reset() {
    cachesim::DirectMappedCache cache(64, 16);
    require(cache.statistics().accesses() == 0 && cache.statistics().hit_rate() == 0, "empty stats");
    cache.access(0); cache.access(0);
    cache.reset();
    require(cache.statistics().accesses() == 0, "reset counters");
    require(!cache.access(0).hit, "reset validity");
}
void one_line_and_max_address() {
    cachesim::DirectMappedCache cache(16, 16);
    require(!cache.access(0).hit && cache.access(15).hit, "one-line spatial reuse");
    require(!cache.access(16).hit && !cache.access(0).hit, "one-line replacement");
    const auto max = std::numeric_limits<std::uint64_t>::max();
    require(!cache.access(max).hit && cache.access(max).hit, "maximum unsigned address");
    const auto detail = cache.access(max);
    require(detail.offset == 15 && detail.tag == max / 16, "maximum address decomposition");
    cachesim::DirectMappedCache byte_cache(4, 1);
    require(!byte_cache.access(3).hit && byte_cache.access(3).hit, "one-byte block");
}
void invalid_configuration() {
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> invalid{
        {0,16}, {64,0}, {63,16}, {64,3}, {16,32}, {1ULL << 21,1}};
    for (const auto& config : invalid) {
        bool rejected = false;
        try { cachesim::DirectMappedCache cache(config.first, config.second); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid configuration accepted");
    }
}
// Independent oracle stores the complete resident memory block number, not tags.
void randomized_reference_comparison() {
    std::mt19937_64 random(2026);
    for (const auto capacity : {16ULL, 64ULL, 256ULL}) {
        for (const auto block_size : {1ULL, 4ULL, 16ULL}) {
            cachesim::DirectMappedCache cache(capacity, block_size);
            const auto count = static_cast<std::size_t>(capacity / block_size);
            std::vector<bool> valid(count, false);
            std::vector<std::uint64_t> resident(count, 0);
            std::uint64_t expected_hits = 0;
            for (int i = 0; i < 4000; ++i) {
                // Alternate broad address coverage with a small, reusable working set.
                const auto address = i % 2 ? random() : random() % 64;
                const auto memory_block = address / block_size;
                const auto slot = static_cast<std::size_t>(memory_block % count);
                const bool expected = valid[slot] && resident[slot] == memory_block;
                const auto actual = cache.access(address);
                require(actual.hit == expected, "random oracle hit/miss disagreement");
                require(actual.replacement == (valid[slot] && !expected), "replacement disagreement");
                expected_hits += expected ? 1 : 0;
                valid[slot] = true;
                resident[slot] = memory_block;
            }
            require(cache.statistics().accesses() == 4000 && cache.statistics().hits == expected_hits,
                    "random oracle totals disagreement");
        }
    }
}
}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"hand-calculated example", example_trace}, {"block boundaries", block_boundaries},
        {"conflicts and tags", conflicts_and_tags}, {"empty cache and reset", empty_and_reset},
        {"one line and maximum address", one_line_and_max_address},
        {"invalid configurations", invalid_configuration},
        {"36,000 accesses against independent reference", randomized_reference_comparison}};
    int failures = 0;
    for (const auto& test : tests) {
        try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
        catch (const std::exception& error) {
            ++failures; std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " test groups passed\n";
    return failures == 0 ? 0 : 1;
}
