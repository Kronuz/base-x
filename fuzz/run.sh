#!/bin/sh
set -eu
binary=$1
run=$2
seconds=${3:-1700}
mkdir -p "$run/corpus" "$run/artifacts"
cp fuzz/corpus/* "$run/corpus/"
set +e
{ /usr/bin/time -p "$binary" "$run/corpus" -artifact_prefix="$run/artifacts/" -seed=20261113 -max_len=2048 -max_total_time="$seconds" -print_final_stats=1; } > "$run/output.log" 2>&1
result=$?
set -e
cat "$run/output.log"
exit "$result"
