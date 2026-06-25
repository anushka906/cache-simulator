# Cache Simulator in C++

A configurable, read-only, direct-mapped cache simulator. Reads memory-address traces and reports cache hits, misses, and hit rate.

## Features

- Configurable cache capacity and block size.
- Unsigned 64-bit addresses in decimal or hexadecimal.
- Access-by-access mapping with `--verbose` and CSV output with `--csv`.
- Automated correctness tests and 36 reproducible experiment configurations.

## Build and run

Requires a C++14 compiler. Python 3.8+ is optional for CLI tests and experiments.

**Windows (PowerShell with GCC on PATH):**

```powershell
.\scripts\build.ps1
.\build\cache_sim.exe --trace traces/example.trace --verbose
.\build\cache_tests.exe
python tests/test_cli.py build/cache_sim.exe
```

**CMake (Windows, Linux, or macOS; CMake 3.16+):**

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake places the executable at `build/cache_sim` on typical Linux/macOS builds or `build/Release/cache_sim.exe` with Visual Studio.

## Usage

```text
cache_sim --trace FILE [--cache-size BYTES] [--block-size BYTES] [--verbose | --csv]
```

Defaults: 64-byte cache, 16-byte blocks. Sizes must be nonzero powers of two, the block must fit in the cache, and at most 1,048,576 lines are allowed.

Trace files contain one byte address per line, in decimal or `0x` hexadecimal. Blank lines and `#` comments are supported. Each entry represents a one-byte read.

```text
block = address / block_size
index = block % number_of_lines
tag   = block / number_of_lines
```

A valid line with a matching tag is a hit; otherwise, the requested block replaces that line. The [example trace](traces/example.trace) produces 2 hits and 4 misses with the default configuration: **33.33% hit rate**.

## Experiments

```powershell
python scripts/run_experiments.py build/cache_sim.exe
```

Adjust the executable path for your build. The script compares sequential, repeated, conflicting, and strided accesses across cache capacities and block sizes. Results are saved to [results/cache_analysis.csv](results/cache_analysis.csv).

This model tracks cache metadata only. It does not model writes, associativity, prefetching, coherence, or execution timing; hit rates do not represent measured application speedups.
