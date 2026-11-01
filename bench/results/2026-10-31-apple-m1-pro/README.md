# Apple M1 Pro, 2026-10-31

The timings behind the base58 table in the [README](../../../README.md#-performance).

| | |
|---|---|
| machine | Apple M1 Pro, arm64 |
| OS | macOS (version not recorded) |
| compiler | Apple clang 21.0.0 (clang-2100.3.33.1) |
| flags | `-std=c++17 -O2` for `bench.cc`, `-O2` for the C codecs |
| base-x | `9adf6bd` ("Fold the RFC 4648 and decode entries into the dated changelog section") |
| uinteger_t | `6ba8ffa` ("Note the eight-limb addition in the changelog"), the `uinteger_t.hh` the build used |
| libbase58, Firedancer | each built from a shallow clone of its default branch; the revisions are not recorded |

The measured header is `9adf6bd`'s `base_x.hh`, built with that commit's `bench/bench.cc`.

## Files

| file | what it is |
|---|---|
| `basex.txt` | one run of `./bench 16 32 64 256 1024`: the codec table for every preset, then the base58 comparison table at the same sizes |
| `basex_compare.txt` | one run of `./bench 32 64 compare`: the comparison table at 32 and 64 bytes |
| `host.txt` | the machine, CPU, compiler, the commits and the uptime and load before and after the runs |

Both runs are `bench.cc`'s output, unedited, from one build:

```sh
cc -O2 -c "$LIBBASE58/base58.c" -o libbase58.o
cc -O2 -c "$FIREDANCER/src/ballet/base58/fd_base58.c" -o fd_base58.o
c++ -std=c++17 -O2 -I. -I"$UT" -DHAVE_LIBBASE58 -I"$LIBBASE58" \
    -DHAVE_FD_BASE58 -I"$FIREDANCER/src/ballet/base58" \
    bench/bench.cc libbase58.o fd_base58.o -o bench
./bench 16 32 64 256 1024 > basex.txt
./bench 32 64 compare > basex_compare.txt
```

`UT` holds `uinteger_t.hh`. [docs/BENCHMARKING.md](../../../docs/BENCHMARKING.md#reproducing-the-readme-table)
describes getting libbase58 and Firedancer.

## The README table

From this directory:

```sh
python3 ../../summarize.py --readme basex.txt basex_compare.txt
```

prints the table of the README's Performance section: each cell is the minimum over the two files.
