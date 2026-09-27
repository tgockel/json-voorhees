#!/usr/bin/env bash
#
# Build Release, run benchmark rows (parse_index by default), print a table of medians.
#
# Usage:
#   scripts/perf-bench.sh                          # parse_index rows, one trial each
#   scripts/perf-bench.sh -n 3                     # three trials per row (medians shown)
#   scripts/perf-bench.sh -f parse_index/canada    # rows under benchmark/parse_index/canada
#   scripts/perf-bench.sh -f extract               # every extraction row
#   scripts/perf-bench.sh -f extract/citm          # extraction rows over citm_catalog.json
#
# -f is read from just after "benchmark/", so it names the group of rows before the
# input: "-f canada" matches nothing, and a filter which matches nothing is an error.
#
# Notes:
#   - canada.json (2.2 MB) is the only input large enough for stable numbers at the
#     default 100 iterations baked into the test runner. Smaller inputs swing by
#     2x+ between trials at the microsecond scale; treat them as smoke tests, not
#     measurements.
#   - The "parse_index/" rows time stage 1 alone (tape build). The "string/" and
#     "ifstream/" rows time the full parse() including extract to jsonv::value.
#     parse_index is only ~9% of full parse() on canada.json — see
#     .agents/perf-baseline.txt for context.
#   - The "extract/<case>/<pipeline>" rows time extraction to a C++ type three ways
#     over the same input: "parse_then_extract" is parse() to a jsonv::value and
#     then extract from that, "from_text" extracts off the parse index without a
#     value, and "from_value" extracts from a value parsed before the clock starts.
#     They run 10 iterations rather than 100, to keep the check target quick.
#
set -euo pipefail

cd "$(dirname "$0")/.."

trials=1
filter=""
while getopts "n:f:h" opt; do
    case "$opt" in
        n) trials="$OPTARG" ;;
        f) filter="$OPTARG" ;;
        h)
            sed -n '2,/^set -euo/p' "$0" | sed -n '/^#/p'
            exit 0
            ;;
        *) exit 2 ;;
    esac
done

if [ ! -x build/jsonv-tests ]; then
    echo "Configuring Release build..." >&2
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJSONV_BUILD_TESTS=ON >/dev/null
fi
cmake --build build -j --target jsonv-tests >/dev/null

filter_arg="benchmark/parse_index/"
if [ -n "$filter" ]; then
    filter_arg="benchmark/${filter}"
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

for ((t=1; t<=trials; t++)); do
    ./build/jsonv-tests "$filter_arg" \
        | awk -F'"' '
            /benchmark\// {
                # benchmark name is the token after "TEST: "
                match($0, /benchmark\/[^ ]+/)
                name = substr($0, RSTART, RLENGTH)
                # $10 is the mean ISO8601 duration: "PT0.001791089S"
                # ($1=prefix, $2=total, $3=:, $4=duration, $5=,, $6=count, $7=:N,, $8=mean, $9=:, $10=duration)
                print name, $10
            }
          ' \
        >> "$tmp"
done

if [ ! -s "$tmp" ]; then
    echo "no benchmark rows match '${filter_arg}'" >&2
    exit 1
fi

# Aggregate: median (or only value) per benchmark name.
awk '
{
    name=$1
    # "PT0.001791089S" → 1.791089 ms
    val=$2
    sub(/^PT/, "", val); sub(/S$/, "", val)
    seconds = val + 0
    ms = seconds * 1000.0
    n[name]++
    times[name, n[name]] = ms
}
END {
    for (name in n) {
        cnt = n[name]
        # Sort the times for this name (small N; insertion sort).
        for (i = 1; i <= cnt; i++) {
            sorted[i] = times[name, i]
        }
        for (i = 2; i <= cnt; i++) {
            key = sorted[i]; j = i - 1
            while (j >= 1 && sorted[j] > key) { sorted[j+1] = sorted[j]; j-- }
            sorted[j+1] = key
        }
        median = (cnt % 2 == 1) ? sorted[(cnt+1)/2] : (sorted[cnt/2] + sorted[cnt/2+1]) / 2.0
        spread = sorted[cnt] - sorted[1]
        printf "%-60s  median=%9.4f ms  spread=%8.4f ms  (n=%d)\n", name, median, spread, cnt
    }
}
' "$tmp" | sort
