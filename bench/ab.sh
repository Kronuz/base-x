#!/bin/sh
# Interleaved A/B of two base_x.hh headers over bench/bench.cc: A B A B ... for ROUNDS rounds.
# See docs/BENCHMARKING.md.
set -eu

usage() {
	cat <<'EOF'
Usage: bench/ab.sh [options] A B [BYTES...] [FILTERS...]
       bench/ab.sh [options] -a A [BYTES...] [FILTERS...]

Builds bench/bench.cc against two base_x.hh (the same bench.cc on both sides) and alternates them,
A B A B ..., for ROUNDS rounds. Each side's output goes to OUTDIR/a_N.txt and OUTDIR/b_N.txt, and
bench/summarize.py prints the comparison. A and B are each a directory holding base_x.hh, a header
file, or a git ref of this repository (its base_x.hh is taken with git show), e.g. "HEAD" against
"." (the working tree). BYTES and FILTERS go to bench.cc: sizes are input lengths in bytes and a
filter is part of a codec name (default: 16 20 25 32 48 64 256 1024 and every codec).

Options:
  -a         A/A: run header A on both sides, to learn the noise (B is not given); the two sides
             are the same binary
  -r ROUNDS  rounds of A and B (default 3)
  -o OUTDIR  output directory (default bench-out/ab)
  -u DIR     directory holding uinteger_t.hh (env UINTEGER_T; default: build/_deps/uinteger_t-src,
             where CMake fetches it)
  -s FILE    benchmark source (default: bench.cc beside this script)
  -h         show this help

Environment: CXX (default c++), BENCH_CXXFLAGS (extra compiler flags, e.g. -march=native).
Run it on a quiet machine, under nice if the machine is shared.

Example: bench/ab.sh -u ../uinteger_t -r 3 -o out master HEAD 16 32 64 256 Base58::bitcoin
EOF
}

die() {
	printf '%s: %s\n' "${0##*/}" "$*" >&2
	exit 1
}

here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
CXX=${CXX:-c++}
BENCH_CXXFLAGS=${BENCH_CXXFLAGS:-}
ut=${UINTEGER_T:-}
rounds=3
out=bench-out/ab
src=$here/bench.cc
aa=0
while getopts ar:o:u:s:h opt; do
	case $opt in
	a) aa=1 ;;
	r) rounds=$OPTARG ;;
	o) out=$OPTARG ;;
	u) ut=$OPTARG ;;
	s) src=$OPTARG ;;
	h) usage; exit 0 ;;
	*) usage >&2; exit 2 ;;
	esac
done
shift $((OPTIND - 1))

case $rounds in
'' | *[!0-9]* | 0) die "-r takes a positive number, not '$rounds'" ;;
esac
if [ -z "$ut" ] && [ -f "$root/build/_deps/uinteger_t-src/uinteger_t.hh" ]; then
	ut=$root/build/_deps/uinteger_t-src
fi
[ -n "$ut" ] || die "say where uinteger_t.hh is: -u DIR or UINTEGER_T=DIR"
[ -f "$ut/uinteger_t.hh" ] || die "no uinteger_t.hh in $ut"
[ -f "$src" ] || die "no benchmark source $src"
if [ "$aa" = 1 ]; then
	[ $# -ge 1 ] || { usage >&2; exit 2; }
	a=$1
	b=$1
	shift 1
else
	[ $# -ge 2 ] || { usage >&2; exit 2; }
	a=$1
	b=$2
	shift 2
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/base-x-bench.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# resolve ARG DIR: copy ARG's base_x.hh to DIR and describe it in $desc.
resolve() {
	mkdir -p "$2"
	if [ -d "$1" ] && [ -f "$1/base_x.hh" ]; then
		cp "$1/base_x.hh" "$2/base_x.hh"
		desc="$1/base_x.hh"
	elif [ -f "$1" ]; then
		cp "$1" "$2/base_x.hh"
		desc=$1
	elif rev=$(git -C "$root" rev-parse --verify --quiet "$1^{commit}" 2>/dev/null); then
		git -C "$root" show "$1:base_x.hh" > "$2/base_x.hh"
		desc="$1 (commit $(git -C "$root" rev-parse --short "$rev"))"
	else
		die "$1 is not a directory holding base_x.hh, a header file or a git ref"
	fi
}

# build DIR OUT: compile bench.cc against DIR/base_x.hh and the chosen uinteger_t.hh.
build() {
	# shellcheck disable=SC2086
	$CXX -std=c++17 -O2 "-I$1" "-I$ut" $BENCH_CXXFLAGS "$src" -o "$2"
}

resolve "$a" "$tmp/a"
desc_a=$desc
resolve "$b" "$tmp/b"
desc_b=$desc

printf 'building %s\n' "$src" >&2
build "$tmp/a" "$tmp/bench_a"
if [ "$aa" = 1 ]; then
	cp "$tmp/bench_a" "$tmp/bench_b"
else
	build "$tmp/b" "$tmp/bench_b"
fi

cpu=
if [ -r /proc/cpuinfo ]; then
	cpu=$(sed -n 's/^model name[[:space:]]*:[[:space:]]*//p' /proc/cpuinfo | head -n 1)
fi
if [ -z "$cpu" ] && command -v sysctl >/dev/null 2>&1; then
	cpu=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || true)
fi
ut_id=
if ut_rev=$(git -C "$ut" rev-parse --short HEAD 2>/dev/null); then
	ut_id=" (commit $ut_rev)"
fi

mkdir -p "$out"
rm -f "$out"/a_*.txt "$out"/b_*.txt
{
	printf 'date: %s\n' "$(date -u '+%Y-%m-%d %H:%M:%SZ')"
	printf 'system: %s\n' "$(uname -srm)"
	printf 'cpu: %s\n' "${cpu:-unknown}"
	# shellcheck disable=SC2086
	printf 'compiler: %s\n' "$($CXX --version 2>&1 | head -n 1)"
	printf 'flags: -std=c++17 -O2 %s\n' "$BENCH_CXXFLAGS"
	printf 'uinteger_t: %s%s\n' "$ut" "$ut_id"
	printf 'A: %s\n' "$desc_a"
	printf 'B: %s\n' "$desc_b"
	if [ "$aa" = 1 ]; then printf 'mode: A/A (both sides are the same binary)\n'; fi
	printf 'rounds: %s\n' "$rounds"
	printf 'args: %s\n' "$*"
	printf 'load before: %s\n' "$(uptime)"
} > "$out/host.txt"

r=1
while [ "$r" -le "$rounds" ]; do
	for side in a b; do
		printf 'round %s of %s: %s\n' "$r" "$rounds" "$side" >&2
		"$tmp/bench_$side" "$@" > "$out/${side}_$r.txt"
	done
	r=$((r + 1))
done
printf 'load after: %s\n' "$(uptime)" >> "$out/host.txt"

printf 'wrote %s rounds to %s\n' "$rounds" "$out" >&2
if command -v python3 >/dev/null 2>&1; then
	python3 "$here/summarize.py" --ab "$out"
fi
