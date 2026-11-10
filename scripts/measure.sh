#!/bin/bash
# Full measurement series for the report, about 50 minutes in total.
#
# Usage (from the repository root, after building):
#     scripts/measure.sh [output dir, default results/v2]
#
# Every scenario is compared REPS times with a new random seed each time; the
# solutions in one comparison share a seed. Repetitions go round the scenarios,
# so a slow period of the machine does not hit only one of them.
set -eu

BIN=build/dining_philosophers_problem
OUT=${1:-results/v2}
REPS=${REPS:-5}
TIME=60

# A sleeping machine ruins a run, see the report.
if [ -z "${CAFFEINATED:-}" ] && command -v caffeinate >/dev/null; then
    CAFFEINATED=1 exec caffeinate -ims "$0" "$@"
fi

mkdir -p "$OUT/logs"

for rep in $(seq 1 "$REPS"); do
    "$BIN" 5 --time "$TIME" --n 5           --out "$OUT" > "$OUT/logs/n5_$rep.log"
    "$BIN" 5 --time "$TIME" --n 5 --glutton --out "$OUT" > "$OUT/logs/glutton_$rep.log"
    "$BIN" 5 --time "$TIME" --n 50          --out "$OUT" > "$OUT/logs/n50_$rep.log"
done

# Deadlock demo: only the outcome counts here, so the runs of one setting go in parallel.
for setting in "5 0-60" "5 0-300" "5 0-600" "5 350-600" "50 0-60" "50 0-600" "50 350-600"; do
    read -r n pause <<< "$setting"
    for rep in $(seq 1 "$REPS"); do
        "$BIN" 4 --time "$TIME" --n "$n" --pause "$pause" --out "$OUT" \
            > "$OUT/logs/deadlock_n${n}_pause${pause}_$rep.log" &
    done
    wait
done
