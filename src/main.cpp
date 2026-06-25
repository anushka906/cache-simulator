#include "cache.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
const char* usage =
    "Cache Simulator - read-only, direct-mapped cache\n"
    "Usage: cache_sim --trace FILE [--cache-size BYTES] [--block-size BYTES]\n"
    "                 [--verbose | --csv]\n"
    "Defaults: --cache-size 64 --block-size 16\n"
    "Sizes must be powers of two; at most 1,048,576 cache lines.\n"
    "Trace: one unsigned decimal or 0x-prefixed hexadecimal byte address per line.\n"
    "Blank lines and # comments are allowed. Each access reads one byte.\n"
    "--verbose  Show block, index, tag, offset, and hit/miss for each access.\n"
    "--csv      Print one CSV header and one summary row.\n"
    "--help     Show this help.\n";

std::uint64_t parse_unsigned(const std::string& text, bool allow_hex) {
    const bool hex = allow_hex && text.size() > 2 && text[0] == '0' &&
                     (text[1] == 'x' || text[1] == 'X');
    const std::size_t start = hex ? 2 : 0;
    if (text.empty() || !std::all_of(text.begin() + start, text.end(),
        [hex](unsigned char c) { return hex ? std::isxdigit(c) != 0 : std::isdigit(c) != 0; })) {
        throw std::invalid_argument("expected an unsigned " +
            std::string(allow_hex ? "decimal or hexadecimal address" : "decimal size"));
    }
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(text, &consumed, hex ? 16 : 10);
        if (consumed != text.size() || value > std::numeric_limits<std::uint64_t>::max()) {
            throw std::out_of_range("address");
        }
        return static_cast<std::uint64_t>(value);
    } catch (const std::out_of_range&) {
        throw std::invalid_argument("number exceeds the unsigned 64-bit range");
    }
}

struct Options {
    std::string trace;
    std::uint64_t capacity = 64;
    std::uint64_t block = 16;
    bool verbose = false;
    bool csv = false;
    bool help = false;
};

Options parse_options(int argc, char* argv[]) {
    Options options;
    std::set<std::string> seen;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (!seen.insert(arg).second) throw std::invalid_argument("duplicate option: " + arg);
        if (arg == "--help") options.help = true;
        else if (arg == "--verbose") options.verbose = true;
        else if (arg == "--csv") options.csv = true;
        else if (arg == "--trace" || arg == "--cache-size" || arg == "--block-size") {
            if (++i == argc) throw std::invalid_argument("missing value for " + arg);
            const std::string value = argv[i];
            if (arg == "--trace") options.trace = value;
            else if (arg == "--cache-size") options.capacity = parse_unsigned(value, false);
            else options.block = parse_unsigned(value, false);
        } else throw std::invalid_argument("unknown option: " + arg);
    }
    if (!options.help && options.trace.empty()) throw std::invalid_argument("--trace FILE is required");
    if (options.csv && options.verbose) throw std::invalid_argument("--csv and --verbose cannot be combined");
    return options;
}
}

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) { std::cout << usage; return 0; }
        cachesim::DirectMappedCache cache(options.capacity, options.block);
        std::ifstream input(options.trace);
        if (!input) throw std::runtime_error("cannot open trace: " + options.trace);
        std::string line;
        std::size_t line_number = 0;
        if (options.verbose) std::cout << "address block index tag offset result replacement\n";
        while (std::getline(input, line)) {
            ++line_number;
            const auto comment = line.find('#');
            if (comment != std::string::npos) line.erase(comment);
            std::istringstream tokens(line);
            std::string token, extra;
            if (!(tokens >> token)) continue;
            if (tokens >> extra) throw std::runtime_error("line " + std::to_string(line_number) +
                                                         ": expected one address per line");
            std::uint64_t address;
            try { address = parse_unsigned(token, true); }
            catch (const std::exception& error) {
                throw std::runtime_error("line " + std::to_string(line_number) + ": " + error.what());
            }
            const auto result = cache.access(address);
            if (options.verbose) {
                std::cout << address << ' ' << result.block_number << ' ' << result.index << ' '
                          << result.tag << ' ' << result.offset << ' '
                          << (result.hit ? "HIT" : "MISS") << ' '
                          << (result.replacement ? "yes" : "no") << '\n';
            }
        }
        if (input.bad()) throw std::runtime_error("failed while reading trace");
        const auto& stats = cache.statistics();
        std::cout << std::fixed << std::setprecision(2);
        if (options.csv) {
            std::cout << "cache_bytes,block_bytes,lines,accesses,hits,misses,hit_rate_percent\n"
                      << cache.capacity_bytes() << ',' << cache.block_bytes() << ',' << cache.line_count()
                      << ',' << stats.accesses() << ',' << stats.hits << ',' << stats.misses
                      << ',' << stats.hit_rate() << '\n';
        } else {
            std::cout << "Cache: " << cache.capacity_bytes() << " bytes; block: " << cache.block_bytes()
                      << " bytes; lines: " << cache.line_count() << '\n'
                      << "Accesses: " << stats.accesses() << "\nHits: " << stats.hits
                      << "\nMisses: " << stats.misses << "\nHit rate: " << stats.hit_rate() << "%\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
