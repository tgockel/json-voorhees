#!/usr/bin/env bash
#
# Build Release, run parse_index benchmarks, print a table with means.
#
# Usage:
#   scripts/perf-bench.sh                # one trial per input
#   scripts/perf-bench.sh -n 3           # three trials per input (medians shown)
#   scripts/perf-bench.sh -f canada      # restrict to inputs whose filename contains 'canada'
#
# Notes:
#   - canada.json (2.2 MB) is the only input large enough for stable numbers at the
#     default 100 iterations baked into the test runner. Smaller inputs swing by
#     2x+ between trials at the microsecond scale; treat them as smoke tests, not
#     measurements.
#   - The "parse_index/" rows time stage 1 alone (tape build). The "string/" and
#     "ifstream/" rows time the full parse() including extract to jsonv::value.
#     parse_index is only ~11% of full parse() on canada.json — see
#     .agents/perf-baseline.txt for context.
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
    filter_arg="benchmark/parse_index/${filter}"
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
        printf "%-50s  median=%9.4f ms  spread=%8.4f ms  (n=%d)\n", name, median, spread, cnt
    }
}
' "$tmp" | sort
