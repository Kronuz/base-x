# Benchmarking

How the README's base58 timings are produced and reproduced, and how to tell whether a change to
`base_x.hh` made the codec faster.

| file | what it does |
|---|---|
| `bench/bench.cc` | times encode and decode for the base58, base62, base64 and base16 presets, and base58 against other codecs |
| `bench/ab.sh` | builds `bench.cc` against two versions of `base_x.hh` and runs them interleaved |
| `bench/summarize.py` | reads their output: the best time per row, A against B, the README table |
| `bench/results/` | the raw output behind the README table, one directory per machine and date |

`ab.sh` is POSIX `sh` and `summarize.py` is Python 3 (standard library only); both run on macOS and
Linux.

## What `bench.cc` measures

```sh
c++ -std=c++17 -O2 -I. -I"$UT" bench/bench.cc -o bench
./bench [bytes...] [filters...]
```

`UT` is a directory holding `uinteger_t.hh`: CMake fetches one into `build/_deps/uinteger_t-src`
(its pin is in `CMakeLists.txt`), or use a checkout of
[uinteger_t](https://github.com/Kronuz/uinteger_t). Run the commands from the repository root.
Sizes are input lengths in bytes (default 16,
20, 25, 32, 48, 64, 256 and 1024); a filter is a substring of a codec name (`Base58::bitcoin`,
`rfc4648`, ...) or `compare` for the base58 comparison table.

The inputs are random with a nonzero first byte, from a pool of 64 per size that the calls cycle
through. A time is nanoseconds per call, the best of 7 rounds of at least 10 ms each, printed to
one decimal. The codec table has one row per codec and size: `enc ns` and `dec ns` call the API as
a user does (`encode` returns a `std::string`, `decode` returns a `std::string`), and `enc reuse`
and `dec reuse` write into one string that keeps its capacity, as the other codecs write into a
caller's buffer.

The `compare` table times base58 `Base58::bitcoin()` against a textbook quadratic codec written
in `bench.cc` (`classic`), and against libbase58 and Firedancer when they are built in. Every
codec is checked against base-x's output before it is timed. Its columns are, in order, `enc bx`,
`bx reuse` (encode into a string), `classic`, `libb58` and `firedncr`, then `dec bx`, `classic`,
`libb58`, `firedncr` and `bx reuse` (decode into a string); `-` marks a codec that is not built
in or does not take the size.

## Reproducing the README table

The table is the best of two runs of `./bench 32 64 256 compare` on base58 against
[libbase58](https://github.com/bitcoin/libbase58) (MIT) and
[Firedancer](https://github.com/firedancer-io/firedancer)'s fixed-size codec (Apache-2.0), which
takes 32 and 64 bytes only. Neither is vendored. Their versions change the numbers, so note each
checkout's `git rev-parse --short HEAD` with the result. Leave out `-mavx2` and `-march` flags so
Firedancer takes its portable path.

```sh
git clone --depth 1 https://github.com/bitcoin/libbase58
git clone --depth 1 --filter=blob:none --sparse https://github.com/firedancer-io/firedancer
git -C firedancer sparse-checkout set src/ballet src/util
LIBBASE58=$PWD/libbase58 FIREDANCER=$PWD/firedancer UT=/path/to/uinteger_t

cc -O2 -c $LIBBASE58/base58.c -o libbase58.o
cc -O2 -c $FIREDANCER/src/ballet/base58/fd_base58.c -o fd_base58.o
c++ -std=c++17 -O2 -I. -I$UT -DHAVE_LIBBASE58 -I$LIBBASE58 \
    -DHAVE_FD_BASE58 -I$FIREDANCER/src/ballet/base58 \
    bench/bench.cc libbase58.o fd_base58.o -o bench

mkdir -p out/readme
./bench 32 64 256 compare > out/readme/run_1.txt
./bench 32 64 256 compare > out/readme/run_2.txt
python3 bench/summarize.py --readme out/readme
```

The last command prints the table in the README's format: the minimum of each cell over the two
runs, with `base-x` the `enc bx` and `dec bx` columns, `into a string` the reuse columns,
`Firedancer` and `libbase58` theirs. `-DHAVE_LIBBASE58` and `-DHAVE_FD_BASE58` are independent;
use either alone. Run it on a quiet machine (see below). The README's numbers come from an Apple
M1 Pro with Apple clang at `-O2`; other compilers, library versions and CPUs give other absolute
times, and the ratios to the other codecs are the part that carries over.

`bench/results/` holds the runs behind the README, one directory per machine and date:
[`2026-10-31-apple-m1-pro`](../bench/results/2026-10-31-apple-m1-pro/) for the table and
[`2026-10-31-intel-i9-9980hk`](../bench/results/2026-10-31-intel-i9-9980hk/) for the Intel
paragraph. Each has the unedited `bench` output, the machine, compiler and commits, and the
`summarize.py` command that prints the README table from the files. To record a measurement, copy
the output directory there (its `host.txt` carries the machine and the commit) and add a
`README.md` with the versions and the command; remove host names and paths first.

Without `--readme`, `summarize.py out/readme` prints every row with its spread over the runs;
`--sizes 32,64` and `--filter Base58::bitcoin` select rows, and `--stat median` takes the median
instead of the minimum.

## Telling whether a change is faster

A change to `base_x.hh` is measured against `master` with an A/A run first and an interleaved A/B
run second, on a quiet machine. `ab.sh` builds the same `bench.cc` against both headers, with the
same compiler, flags and `uinteger_t.hh`, and alternates them, so drift in the machine's speed
reaches both sides. It needs `-u DIR` (or `UINTEGER_T=DIR`) for the `uinteger_t.hh` directory, and
takes `build/_deps/uinteger_t-src` when that exists.

**A quiet machine.** Nothing else heavy runs while the benchmark does. On a shared host, take
the machine's exclusive hold if it has one; `nice -n 19` keeps the benchmark from slowing others
but does not quiet them. `host.txt` in the output directory records the load average before and
after.

**A/A first.** The noise of a machine is what the same build shows against itself:

```sh
bench/ab.sh -a -u "$UT" -r 3 -o out/aa master 16 32 64 256 Base58::bitcoin
```

The two sides are one binary, run alternately three times (A B A B A B). The `spread` columns and
the largest `|B/A - 1|` are the noise of those rows on that machine, and on a quiet machine every
verdict is blank. A `faster` or `slower` row means the machine is noisier than the spreads of
three rounds show: rerun with more rounds (`-r`) or on a quieter machine. Take the largest
`|B/A - 1|` of the rows being compared as the floor for the A/B run that follows.

**Then A/B.** The change interleaved against `master`:

```sh
bench/ab.sh -u "$UT" -r 3 -o out/ab master HEAD 16 32 64 256 Base58::bitcoin
```

A and B are each a git ref of this repository (its `base_x.hh` is taken with `git show`), a
directory holding `base_x.hh`, or a header file; `HEAD` against `.` compares a commit with the
working tree. The sizes are the input lengths users hit: 16 and 32 bytes for keys and hashes, 64,
and a few hundred. The filters name the codecs the change touches.

**Reading the result.** `summarize.py --ab` prints a row for each codec, size and metric (`enc`,
`dec`, `enc_reuse`, `dec_reuse`): A and B (the minimum over the rounds, or the median with
`--stat median`), the spread of each side, `B/A` (below 1, B is faster) and a verdict. The spread
of a side is `(max - min) / min` over its rounds. A row is `faster` or `slower` when `B/A` is off
1 by more than the larger of the two spreads, the threshold (`--threshold`, 3% by default) and
the resolution of the printed times (0.1 ns).

**When a change counts.** A change stays only if it is faster beyond the spread, which means
about 3% or more on the rows it targets; 10% counts without argument. A difference inside the
A/A spread is noise. Report the minimum (or median) per row together with the A/A spread.

Both sides use the same `uinteger_t.hh`, so `ab.sh` measures `base_x.hh` alone. The conversion
itself is `uinteger_t::digits()` and `from_digits()`; changes to it are measured in that
repository, which has the same scripts and
[its own guide](https://github.com/Kronuz/uinteger_t/blob/master/docs/BENCHMARKING.md).
