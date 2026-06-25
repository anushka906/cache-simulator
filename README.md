# Cache Simulator in C++

A configurable, read-only, direct-mapped cache simulator for studying memory locality and cache conflicts. Give it a trace of byte addresses; it reports hits, misses, and the mapping of each access. Everything runs in software, with no FPGA or GPU required.

The C++ simulator has no third-party dependencies. Python 3.8+ is optional for command-line tests and experiments.

## Features

- Configurable cache capacity and block size, expressed in bytes.
- 64-bit unsigned byte addresses in decimal or hexadecimal.
- Verbose access-by-access mapping and machine-readable CSV summaries.
- Tests for conflicts, block boundaries, resets, invalid inputs, and maximum addresses.
- An independent reference comparison covering 36,000 deterministic randomized accesses.
- A reproducible experiment across four workloads and nine cache configurations.

## Build and run

Use a C++14 compiler such as GCC, Clang, or MSVC. Run commands from this project directory.

### Windows: PowerShell and GCC

```powershell
.\scripts\build.ps1
.\build\cache_sim.exe --trace traces/example.trace --verbose
.\build\cache_tests.exe
python tests/test_cli.py build/cache_sim.exe
python scripts/run_experiments.py build/cache_sim.exe
```

If GCC is not on PATH, pass its executable explicitly:

```powershell
.\scripts\build.ps1 -Compiler 'C:\MinGW\bin\g++.exe'
```

### Linux/macOS: direct compiler commands

```sh
mkdir -p build
g++ -std=c++14 -O2 -Wall -Wextra -Wpedantic -I include src/cache.cpp src/main.cpp -o build/cache_sim
g++ -std=c++14 -O2 -Wall -Wextra -Wpedantic -I include src/cache.cpp tests/cache_tests.cpp -o build/cache_tests
./build/cache_sim --trace traces/example.trace --verbose
./build/cache_tests
python3 tests/test_cli.py build/cache_sim
python3 scripts/run_experiments.py build/cache_sim
```

### CMake: portable alternative

With CMake 3.16+ installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For Visual Studio generators the executable is usually `build/Release/cache_sim.exe`; pass that path to the experiment script. CMake includes the Python CLI tests when it finds a Python interpreter. GitHub Actions is configured to build and test on Windows and Linux; its first remote run is separate from local verification.

## Command-line interface

```text
cache_sim --trace FILE [--cache-size BYTES] [--block-size BYTES] [--verbose | --csv]
```

| Option | Meaning | Default |
|---|---|---|
| `--trace FILE` | Read addresses from this file | Required |
| `--cache-size N` | Cache data capacity in bytes | 64 |
| `--block-size N` | Bytes fetched on a miss | 16 |
| `--verbose` | Print the mapping and outcome for every access | Off |
| `--csv` | Print a CSV header and summary row | Off |
| `--help` | Print usage information | Off |

Sizes must be nonzero powers of two, the block must fit in the cache, and at most 1,048,576 lines are allowed to bound simulator memory use. The capacity represents modelled data storage; metadata overhead is not counted. Sizes use decimal input. Addresses may use decimal or `0x`/`0X` hexadecimal input; a leading zero does not imply octal.

Traces contain one address per line. Empty lines and `#` comments, including trailing comments, are accepted. Each trace entry represents a one-byte read. Negative numbers, values outside the unsigned 64-bit range, and extra tokens are rejected with a line number. Errors return exit code 1; successful runs return 0. Empty traces report a 0.00% hit rate by convention.

## How the cache works

```text
byte address
     |
     v
block number = address / block size
     |
     +--> index = block number % number of lines
     |
     +--> tag   = block number / number of lines

selected line is valid AND its tag matches?
     | yes                         | no
     v                             v
    HIT                      MISS: install new tag
```

The block offset is `address % block_size`. With power-of-two sizes, these calculations correspond to splitting the address into `[tag | index | offset]` bits. For a 64-byte cache with 16-byte blocks there are four lines, two index bits, and four offset bits.

Each line stores a validity bit and a tag. An empty line must miss even when its initial tag equals the requested tag. On a miss, the whole memory block is conceptually fetched, so later addresses inside that block can hit. The simulator only stores metadata, not block contents.

A direct-mapped cache gives each block exactly one possible location, so it needs no LRU replacement policy. An occupied line holding a different tag is replaced. `replacement=yes` describes that event; it does not classify the miss as compulsory, capacity, or conflict.

## Walk through the example

For 64 cache bytes and 16-byte blocks:

| Address | Block | Index | Tag | Offset | Result |
|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | Miss: empty line |
| 4 | 0 | 0 | 0 | 4 | Hit: same block |
| 16 | 1 | 1 | 0 | 0 | Miss: another empty line |
| 0 | 0 | 0 | 0 | 0 | Hit |
| 64 | 4 | 0 | 1 | 0 | Miss: replaces block 0 |
| 0 | 0 | 0 | 0 | 0 | Miss: block 0 was evicted |

Expected summary:

```text
Cache: 64 bytes; block: 16 bytes; lines: 4
Accesses: 6
Hits: 2
Misses: 4
Hit rate: 33.33%
```

## Reproduce the experiments

```powershell
python scripts/run_experiments.py build/cache_sim.exe --output results/cache_analysis.csv
```

The script generates synthetic traces, executes the C++ program, and combines its CSV summaries. Each workload/configuration starts with an empty cache and contains 64 reads. It evaluates capacities of 64, 128, and 256 bytes with block sizes of 8, 16, and 32 bytes, producing 36 result rows.

| Workload | Address sequence | What it illustrates |
|---|---|---|
| `sequential` | 0, 4, ..., 252, once | Spatial locality |
| `repeated_working_set` | 0, 4, ..., 60, repeated four times | Reuse of a resident working set |
| `conflicting_pair` | 0, 64, repeated 32 times | Direct-mapped conflicts |
| `stride_64` | 0, 64, ..., 960, repeated four times | A sparse working set with poor mapping |

Selected results, with a 16-byte block in every row:

| Workload | Cache bytes | Hits | Misses | Hit rate |
|---|---:|---:|---:|---:|
| Sequential | 64 | 48 | 16 | 75.00% |
| Sequential | 128 | 48 | 16 | 75.00% |
| Repeated working set | 64 | 60 | 4 | 93.75% |
| Conflicting pair | 64 | 0 | 64 | 0.00% |
| Conflicting pair | 128 | 62 | 2 | 96.88% |
| Stride 64 | 256 | 0 | 64 | 0.00% |

The sequential workload reads four addresses from each 16-byte block: one miss followed by three hits. A larger cache does not improve this single pass because there is no reuse of older blocks.

The conflicting pair maps both blocks to line 0 with 64 cache bytes, making every access miss. At 128 cache bytes, addresses 0 and 64 map to different lines. Only the initial two accesses miss.

The repeated working set has four distinct 16-byte blocks that fit in the 64-byte cache. Only the first access to each block misses. For `stride_64`, the 256-byte cache uses just four of its sixteen line indices, with four different blocks competing for each used index. This explains why counting the distinct blocks alone is insufficient to predict hits.

See [the generated CSV](results/cache_analysis.csv) for every configuration. These are synthetic hit-rate results, not CPU/GPU speedups. Larger blocks improve the sequential example's hit rate, but this model does not charge for transferring the additional bytes.

## Project structure

```text
include/cache.h              Public cache API and result structures
src/cache.cpp                Address mapping, lookup, statistics, reset
src/main.cpp                 Command-line and trace parsing, output
tests/cache_tests.cpp        Core correctness and independent reference tests
tests/test_cli.py            Black-box command-line tests
scripts/build.ps1           GCC build for Windows
scripts/run_experiments.py   Generate traces and collect simulator results
traces/                     Small examples to verify by hand
results/cache_analysis.csv  Reproducible experiment output
.github/workflows/ci.yml     Windows/Linux build-and-test workflow
```

## Scope and limitations

This is one simplified cache level with byte reads, immediate block allocation on a miss, and no prefetching. It does not model writes, dirty bits, associativity, virtual addresses, coherence, multiple cores, latency, bandwidth, or execution time. It does not classify misses into the three Cs. It is not a model of a particular NVIDIA GPU or CPU.

Potential extensions are 2-way associativity with LRU, write policies, or an explicitly documented latency model. They are future work, not implemented features.
