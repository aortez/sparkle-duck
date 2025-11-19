#!/bin/bash
# Analyze swap behavior from sparkle-duck.log

LOG_FILE="${1:-sparkle-duck.log}"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: Log file '$LOG_FILE' not found"
    exit 1
fi

echo "========================================"
echo "SWAP ANALYSIS: $LOG_FILE"
echo "========================================"
echo

# Swap Checks
echo "=== Swap Checks ==="
h_checks=$(grep -c "Horizontal swap check" "$LOG_FILE")
h_passed=$(grep -c "Horizontal swap check.*swap_ok: true" "$LOG_FILE")
v_checks=$(grep -c "Vertical swap check" "$LOG_FILE")
v_passed=$(grep -c "Vertical swap check.*swap_ok: true" "$LOG_FILE")

echo "Horizontal checks: $h_checks"
echo "  Passed: $h_passed ($(awk "BEGIN {printf \"%.1f\", 100*$h_passed/$h_checks}")%)"
echo "Vertical checks: $v_checks"
echo "  Passed: $v_passed ($(awk "BEGIN {printf \"%.1f\", 100*$v_passed/$v_checks}")%)"
echo

# Actual Swaps Executed
echo "=== Actual Swaps Executed ==="
h_swaps=$(grep -cE "SWAP:.*direction: (1,0|-1,0)" "$LOG_FILE" || echo 0)
v_swaps=$(grep -cE "SWAP:.*direction: (0,1|0,-1)" "$LOG_FILE" || echo 0)

echo "Horizontal swaps: $h_swaps"
echo "Vertical swaps: $v_swaps"
echo

# Material Pair Analysis
echo "=== Horizontal Swaps by Material Pair ==="
echo "Checks passed (pressure test):"
grep "Horizontal swap check.*swap_ok: true" "$LOG_FILE" | \
    sed 's/.*check: \([A-Z]*\) <-> \([A-Z]*\).*/\1 <-> \2/' | \
    sort | uniq -c | sort -rn | head -10 | \
    awk -v total="$h_passed" '{printf "  %-20s %6d  (%5.1f%%)\n", $2" "$3" "$4, $1, 100*$1/total}'

echo "Actually executed:"
h_total=$h_swaps
grep -E "SWAP:.*direction: (1,0|-1,0)" "$LOG_FILE" | \
    sed 's/.*SWAP: \([A-Z]*\) <-> \([A-Z]*\).*/\1 <-> \2/' | \
    sort | uniq -c | sort -rn | head -10 | \
    awk -v total="$h_total" '{printf "  %-20s %6d  (%5.1f%%)\n", $2" "$3" "$4, $1, 100*$1/total}'
echo

echo "=== Vertical Swaps by Material Pair ==="
echo "Checks passed (density test):"
grep "Vertical swap check.*swap_ok: true" "$LOG_FILE" | \
    sed 's/.*check: \([A-Z]*\) <-> \([A-Z]*\).*/\1 <-> \2/' | \
    sort | uniq -c | sort -rn | head -10 | \
    awk -v total="$v_passed" '{printf "  %-20s %6d  (%5.1f%%)\n", $2" "$3" "$4, $1, 100*$1/total}'

echo "Actually executed:"
v_total=$v_swaps
grep -E "SWAP:.*direction: (0,1|0,-1)" "$LOG_FILE" | \
    sed 's/.*SWAP: \([A-Z]*\) <-> \([A-Z]*\).*/\1 <-> \2/' | \
    sort | uniq -c | sort -rn | head -10 | \
    awk -v total="$v_total" '{printf "  %-20s %6d  (%5.1f%%)\n", $2" "$3" "$4, $1, 100*$1/total}'
echo

# Check for specific material swaps
echo "=== Special Cases ==="
wall_checks=$(grep -c "Horizontal swap check.*WALL" "$LOG_FILE")
wall_passed=$(grep -c "Horizontal swap check.*WALL.*swap_ok: true" "$LOG_FILE")
wall_swaps=$(grep -cE "SWAP:.*WALL.*direction: (1,0|-1,0)" "$LOG_FILE" || echo 0)

echo "WALL horizontal checks: $wall_checks"
echo "  Passed pressure: $wall_passed"
echo "  Actually swapped: $wall_swaps"
echo

# Energy statistics
echo "=== Energy Statistics ==="
echo "Average swap energy (first 100):"
grep "Swap approved" "$LOG_FILE" | head -100 | \
    grep -oE "Energy: [0-9.]+" | \
    grep -oE "[0-9.]+" | \
    awk '{sum+=$1; count++} END {printf "  %.2f (n=%d)\n", sum/count, count}'

echo
echo "Max velocities seen:"
grep "SWAP complete" "$LOG_FILE" | \
    grep -oE "Vel: [0-9.]+" | \
    grep -oE "[0-9.]+" | \
    sort -n | tail -5 | \
    awk '{printf "  %.2f\n", $1}'

echo
echo "========================================"
