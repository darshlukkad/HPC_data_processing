#!/bin/bash

BINARY="../build/phase1/phase1"
DATASET="../dataset/nyc_311_2020_2026.csv"
RUNS=10
LOG="benchmark_results.txt"

echo "Phase 1 Benchmark — $RUNS runs" | tee "$LOG"
echo "Binary : $BINARY"               | tee -a "$LOG"
echo "Dataset: $DATASET"              | tee -a "$LOG"
echo "Date   : $(date)"               | tee -a "$LOG"
echo "----------------------------------------" | tee -a "$LOG"

for i in $(seq 1 $RUNS); do
    echo ""                           | tee -a "$LOG"
    echo "=== Run $i ==="             | tee -a "$LOG"
    "$BINARY" "$DATASET"              | tee -a "$LOG"
done

echo ""                                                        | tee -a "$LOG"
echo "========================================"              | tee -a "$LOG"
echo "Summary (ms)"                                         | tee -a "$LOG"
echo ""                                                        | tee -a "$LOG"

for op in "load" "searchByZip" "searchByDate" "searchByBoundingBox"; do
    grep "\[$op\]" "$LOG" | awk '{print $(NF-1)}' | awk -v op="$op" '
    BEGIN { sum=0; min=1e18; max=0; n=0 }
    {
        val = $1+0
        sum += val; n++
        if (val < min) min = val
        if (val > max) max = val
    }
    END {
        avg = sum/n
        printf "--- %-20s  runs=%d  avg=%10.2f ms  min=%10.2f ms  max=%10.2f ms\n", op, n, avg, min, max
    }' | tee -a "$LOG"
done
