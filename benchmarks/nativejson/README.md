# nativejson-benchmark

Runs [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) against this checkout
of JSON Voorhees. It's a separate CMake project rather than an option on the main build, because it
downloads the harness and the competing libraries at configure time.

```bash
$> cmake -S benchmarks/nativejson -B build/nativejson -G Ninja -DCMAKE_BUILD_TYPE=Release
$> cmake --build build/nativejson --parallel 4
$> ctest --test-dir build/nativejson --output-on-failure
$> (cd build/nativejson/run && ../nativejson-benchmark --performance-only)
```

`ctest` runs `--verify-only`. Every library has to parse each data file to RapidJSON's reference
statistics and survive a stringify round trip. The program exits 0 regardless of the outcome, so
the test fails on the words it prints instead.

Run `nativejson-benchmark` from `run/`, because it reads `../data/`. Pass one of these:

| Flag                 | Writes, in `run/`                                    |
|----------------------|------------------------------------------------------|
| `--verify-only`      | nothing (unless a library fails, then its output)    |
| `--performance-only` | `performance_<cpu>_<os>_<compiler>.csv`              |
| `--conformance-only` | `conformance.csv`                                    |

With no flag it runs all three. With a multi-config generator, the program is
`<config>/nativejson-benchmark` (run it as `../Release/nativejson-benchmark`), and the jsonstat
programs are next to it in `<config>/jsonstat/`. Upstream renders the CSVs to charts with the PHP pages under its
`result/` directory.

To read the results in the terminal, `report.py` prints the newest performance CSV as Markdown
tables. There's one table per operation, with a row per library (fastest first) and a column per
data file. Add `--memory` for peak memory while parsing, or pass CSV files or directories to read
something else:

```bash
$> benchmarks/nativejson/report.py
```

```
### Parse (ms)

| Library             | canada.json | citm_catalog.json | twitter.json | total |
|---------------------|------------:|------------------:|-------------:|------:|
| simdjson            |        2.43 |              0.58 |         0.23 |  3.24 |
| RapidJSON (C++)     |        3.21 |              1.81 |         1.23 |  6.25 |
...
```

`build/nativejson` sits inside the `build/` tree that `scripts/perf-bench.sh` uses. The two don't
interfere.

## What gets built

| Library       | Version                                   | Adapter                                |
|---------------|-------------------------------------------|----------------------------------------|
| JSON Voorhees | this checkout, static, LTO                | `jsonvtest.cpp`                        |
| RapidJSON     | master `24b5e7a` (1.1.0 no longer builds) | upstream `src/tests/rapidjsontest.cpp` |
| nlohmann      | 3.12.0                                    | upstream `src/tests/nlohmanntest.cpp`  |
| simdjson      | 5.0.5                                     | `simdjsontest.cpp` (fixed copy)        |
| Boost.JSON    | the installed Boost, if found             | `boostjsontest.cpp` (fixed copy)       |

The harness is pinned to upstream's last commit, `478d572` (2022). Every download is pinned by
SHA-256 in `CMakeLists.txt`. RapidJSON can't be removed: the harness uses it to compute the
reference statistics and reads no data without it. Leaving out Boost just drops its rows.

The jsonv adapter parses with `parse_options::create_strict()`, so the jsonchecker documents that
are meant to fail do fail.

Everything is compiled with `-march=native` like upstream's build. Turn that off with
`-DNATIVEJSON_MARCH_NATIVE=OFF`.

The "Code size" rows are the file sizes of the `jsonstat/jsonstat_<adapter>` programs. Each one
parses a file and counts its values with a single library.

The memory columns count `operator new` and `delete` everywhere. They count `malloc`, `realloc`
and `free` only where `memorystat.h` turns them into macros. That covers the header-only libraries,
which are compiled into the adapters. jsonv is compiled with the header force-included too,
because `parse_index` allocates its tape with `std::malloc`. `memorystat_passthrough.cpp` supplies
those functions, uncounted, to `jsonstat_jsonvtest`.

## Changes to upstream

- `main.cpp`: configure copies it into the build directory with one line changed. `BenchCodeSize`
  took the jsonstat suffix from the first `_` in the program's name and passed NULL to `printf`
  when there wasn't one.
- `memorystat.h`: it `#define`s `malloc` and friends and then includes `<stdlib.h>`, which no
  longer compiles against current libstdc++. The build force-includes `<stdlib.h>` and
  `<string.h>` ahead of it.
- `simdjsontest.cpp` and `boostjsontest.cpp`: these are copies. Each one says at the top what it
  changes.
