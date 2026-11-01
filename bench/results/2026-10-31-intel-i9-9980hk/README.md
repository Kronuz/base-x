# Intel Core i9-9980HK, 2026-10-31

The timings behind the Intel Core i9-9980HK paragraph of the [README](../../../README.md#-performance).

| | |
|---|---|
| machine | Intel Core i9-9980HK at 2.40 GHz, x86_64 (AVX2) |
| OS | macOS (version not recorded) |
| compiler | Apple clang 17.0.0 (clang-1700.6.4.2) |
| flags | `-std=c++17 -O2` for `bench.cc`, `-O2` for the C codecs; `-march=haswell` as well for `basex_compare_avx.txt` |
| base-x | `9adf6bd` ("Fold the RFC 4648 and decode entries into the dated changelog section") |
| uinteger_t | `6ba8ffa` ("Note the eight-limb addition in the changelog"), the `uinteger_t.hh` the build used |
| libbase58, Firedancer | each built from a shallow clone of its default branch; the revisions are not recorded |

The measured header is `9adf6bd`'s `base_x.hh`, built with that commit's `bench/bench.cc`.

## Files

| file | what it is |
|---|---|
| `basex.txt` | one run of `./bench 16 32 64 256 1024`: the codec table for every preset, then the base58 comparison table at the same sizes |
| `basex_compare.txt` | one run of `./bench 32 64 compare`: the comparison table at 32 and 64 bytes |
| `basex_compare_avx.txt` | the same run from a build with `-march=haswell`: `bench.cc` and Firedancer compiled for AVX2, so Firedancer takes its AVX2 path (libbase58 stays portable) |
| `host.txt` | the machine, CPU, compiler, the commits and the uptime and load before and after the runs |

The runs are `bench.cc`'s output, unedited. The portable build is

```sh
cc -O2 -c "$LIBBASE58/base58.c" -o libbase58.o
cc -O2 -c "$FIREDANCER/src/ballet/base58/fd_base58.c" -o fd_base58.o
c++ -std=c++17 -O2 -I. -I"$UT" -DHAVE_LIBBASE58 -I"$LIBBASE58" \
    -DHAVE_FD_BASE58 -I"$FIREDANCER/src/ballet/base58" \
    bench/bench.cc libbase58.o fd_base58.o -o bench
./bench 16 32 64 256 1024 > basex.txt
./bench 32 64 compare > basex_compare.txt
```

and the AVX2 build adds `-march=haswell` to the compile of `fd_base58.c` and of `bench.cc`, then runs
`./bench 32 64 compare > basex_compare_avx.txt`. `UT` holds `uinteger_t.hh`.
[docs/BENCHMARKING.md](../../../docs/BENCHMARKING.md#reproducing-the-readme-table) describes getting
libbase58 and Firedancer.

## The numbers in the README

The README's Intel paragraph quotes `basex_compare.txt`: 32-byte encode 143.5 ns against Firedancer's
69.6, and 64-byte decode 277.2 against 287.1. This machine's table in the README's format:

```sh
python3 ../../summarize.py --readme basex.txt basex_compare.txt
```

takes the minimum of each cell over the two files; `basex_compare_avx.txt` alone gives the table
with Firedancer on AVX2.
