# Intel Core i9-9980HK, 2026-11-01

The timings behind the Intel Core i9-9980HK table of the [README](../../../README.md#-performance).

| | |
|---|---|
| machine | Intel Core i9-9980HK at 2.40 GHz, x86_64 |
| OS | macOS 15.8.1 |
| compiler | Apple clang 17.0.0 (clang-1700.6.4.2) |
| flags | `-std=c++17 -O2` for `bench.cc`, `-O2` for the C codecs; no `-march`, so Firedancer takes its portable path |
| base-x | `aac8555`, whose `base_x.hh` and `bench/bench.cc` are those of `2d6835a` |
| uinteger_t | `291dab8`, the `uinteger_t.hh` the build used |
| libbase58 | v0.1.4 (`16c2527`), shallow clone of the tag |
| Firedancer | v26.10.5 (`6286f99`), shallow sparse clone of the tag (`src/ballet`, `src/util`) |

## Files

| file | what it is |
|---|---|
| `basex.txt`, `basex2.txt` | two runs of `./basex_bench 32 64 256 compare`, unedited |
| `host.txt` | the machine, compiler, commits and the load before and after |

The build is the one in [docs/BENCHMARKING.md](../../../docs/BENCHMARKING.md#reproducing-the-readme-table).
The README table prints from the files, the minimum of each cell over the two runs:

```sh
python3 ../../summarize.py --readme basex.txt basex2.txt
```
