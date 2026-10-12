#!/usr/bin/env bash
# Follow-up experiments explaining why speedup levels off (report section 7).
#
# Usage: scripts/run_experiments.sh <build_dir>
# Writes results/experiments.csv and results/membw.csv.
#
# CPU numbering is for the Intel Core Ultra 9 285 used in the report:
# CPUs 0-7 are performance cores, CPUs 8-23 are efficiency cores.

set -euo pipefail

BUILD=${1:-build}
OUT=results/experiments.csv
P_CORES=0-7
E_CORES=8-23
HUMAN=datasets/human_genome.fna
WHEAT=datasets/wheat_genome.fna

mkdir -p results
rm -f "$OUT"

echo "== Memory bandwidth"
"$BUILD/membw" 4e9 > results/membw.csv
cat results/membw.csv

echo "== Scan rate on each core type (1 GB, threads pinned)"
taskset -c "$P_CORES" "$BUILD/bm_bench" "$HUMAN" "$OUT" --name P-cores --threads 1,8 --sizes 1e9
taskset -c "$E_CORES" "$BUILD/bm_bench" "$HUMAN" "$OUT" --name E-cores --threads 1,8,16 --sizes 1e9

echo "== Task granularity at 24 threads"
for k in 1 2 4 8 16; do
    "$BUILD/bm_bench" "$HUMAN" "$OUT" --name human --threads 24 --sizes 1e9,4e9 --tasks-per-thread "$k"
done
for k in 1 4; do
    "$BUILD/bm_bench" "$WHEAT" "$OUT" --name wheat --threads 24 --sizes 2e10 --tasks-per-thread "$k"
done

echo "Done: $OUT"

echo "== Clock frequency of busy cores (1 vs 8 performance cores, 1 vs 16 efficiency cores)"
# Keeps the given cores busy with one spinning process each and samples the
# clock of the first one, to see how much the clock drops as more cores work.
sample_freq() {  # <first cpu> <number of cpus>
    local pids=()
    for ((c = $1; c < $1 + $2; c++)); do
        taskset -c "$c" awk 'BEGIN { while (1) x++ }' &
        pids+=($!)
    done
    sleep 3
    local samples=()
    for _ in $(seq 10); do
        samples+=("$(cat /sys/devices/system/cpu/cpu$1/cpufreq/scaling_cur_freq)")
        sleep 0.3
    done
    kill "${pids[@]}"
    wait "${pids[@]}" 2>/dev/null || true
    printf '%s\n' "${samples[@]}" | sort -n | awk -v c="$1" -v n="$2" \
        '{a[NR]=$1} END { printf "%s,%s,%.0f\n", c, n, a[int((NR+1)/2)]/1000 }'
}
{
    echo "first_cpu,busy_cpus,median_mhz"
    sample_freq 0 1
    sample_freq 0 8
    sample_freq 8 1
    sample_freq 8 16
    sample_freq 0 24
} > results/frequency.csv
cat results/frequency.csv
