# Testing

How a change to `base_x.hh` is verified: which check catches which failure, how to run each one,
and how the timings in the README are reproduced. [AGENTS.md](../AGENTS.md) maps the files,
[BENCHMARKING.md](BENCHMARKING.md) holds the timing protocol, [MUTATION.md](MUTATION.md) the mutation
tool, and [OPEN-WORK.md](OPEN-WORK.md) what no test reaches. The base conversion itself is
`uinteger_t::digits()` and `from_digits()`, which
[uinteger_t's tests](https://github.com/Kronuz/uinteger_t/blob/master/docs/TESTING.md) cover.

## The checks and what each catches

`CMakeLists.txt` defines five CTest tests and fetches uinteger_t at the commit it pins. The smoke test
and the golden test are built twice, since the code has two paths: the table-lookup scan and map of
decode and encode run on AArch64 for alphabets of up to 64 characters, and `BASEX_NO_SIMD` turns
them off.

| check | CTest tests or command | what it is there to catch |
|---|---|---|
| smoke test | `base_x`, `base_x_scalar` (`test/test.cc`; the second with `BASEX_NO_SIMD`) | a wrong encode or decode: every plain preset against a byte-wise schoolbook model that shares no code with the library, Bitcoin Core's and the XRP Ledger's base58 vectors, RFC 4648 vectors from Python's `base64`, the check and checksum characters, `is_valid` against `decode` on edited texts, appending into a non-empty result, the `block_digits` guard at 16- and 32-bit sizes; it checks with `assert`, so only without `NDEBUG` |
| golden output | `base_x_golden`, `base_x_golden_scalar` (`test/golden.cc` against `test/golden/encode.txt`) | a change in the text any preset encodes (41 presets, 169 cases each, 6929 lines), a text that does not decode back as its preset's semantics say, a preset of `base_x.hh` missing from the test's table |
| decode fuzz | `base_x_fuzz_decode` (`test/fuzz_decode.cc` built with `FUZZ_STANDALONE`: 20000 inputs, seed 20261031) | a decode that disagrees with a model (the strict presets with a schoolbook one, the `leading_zeros` presets with Bitcoin's rule, the RFC 4648 presets bit by bit), that crashes or reads outside the text, a value that does not survive encode and decode, `is_valid` and `decode` disagreeing |
| libFuzzer | `test/fuzz_decode.cc` without the define; not in CTest or CI | the same properties on inputs libFuzzer grows by coverage |
| mutation | `tools/mutate.py` | a test that passes whatever the code does |
| legacy suite | `tests/` (GoogleTest; not built by CMake or CI; `tests/Makefile`) | the broader vectors of the original suite |

Every test but the smoke test reports its failures itself, so it checks under `NDEBUG` too. The text
of a preset is a stored format (Xapiand keeps ids and UUIDs in three of them), so the golden file is
the oracle of identity: a line that changes is a stored text that changes.

## Build types and sanitizers

`.github/workflows/ci.yml` calls the shared workflow
[`cmake.yml`](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml) of
cpp-libraries on every push to `master`, every pull request, and nightly. Each job builds every
target and runs CTest:

| job | runner, compiler | build type and flags |
|---|---|---|
| `linux-x86_64-gcc`, `linux-x86_64-clang`, `linux-arm64-gcc` | Ubuntu 24.04, GCC or Clang | RelWithDebInfo, `-O2 -g -D_GLIBCXX_ASSERTIONS` |
| `macos-arm64`, `macos-x86_64` | macOS 15, Apple clang | RelWithDebInfo, `-O2 -g -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE` |
| `freebsd-x86_64` | FreeBSD 14.3 in a VM | RelWithDebInfo, `-O2 -g` |
| `linux-asan-ubsan` | Ubuntu 24.04, Clang | Debug, `-fsanitize=address,undefined -fno-sanitize-recover=undefined -D_GLIBCXX_ASSERTIONS` |
| `linux-tsan` | Ubuntu 24.04, Clang | RelWithDebInfo, `-O1 -g -fsanitize=thread -fno-omit-frame-pointer` |

The RelWithDebInfo flags replace CMake's `-O2 -g -DNDEBUG`, so `assert` stays on in every job,
`test/test.cc`'s and the constructor's. The two AArch64 jobs (`linux-arm64-gcc`, `macos-arm64`) are
the ones where `base_x` and `base_x_scalar` run different code. The tests take the same arguments in
every build type; there is no light mode. A Release build defines `NDEBUG`, and `base_x` and
`base_x_scalar` then pass without checking anything.

**On macOS.** With Apple clang 17 on macOS 26 (Apple M4 Pro), an empty program built with
`-fsanitize=address` hangs at startup and one built with `-fsanitize=thread` crashes, so ASan and
TSan run in CI. Locally, run UBSan with libc++ hardening, run Release, and build and link every
target with `-fsanitize=thread`:

```sh
cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CXX_FLAGS=-O1 -g -Wall -Wextra -fsanitize=undefined -fno-sanitize-recover=undefined -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined
cmake --build build-ubsan --parallel 4 && ctest --test-dir build-ubsan --output-on-failure --parallel 4

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 4 && ctest --test-dir build-release --output-on-failure --parallel 4

cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g -fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
cmake --build build-tsan --parallel 4      # link only
```

Each configure fetches uinteger_t at the pin;
`-DFETCHCONTENT_SOURCE_DIR_UINTEGER_T=$PWD/build-ubsan/_deps/uinteger_t-src` reuses the first copy,
and the path of another checkout builds against that one.

Apple clang has no libFuzzer runtime; Homebrew's LLVM does. Without ASan the fuzzer runs locally:

```sh
UT=build-ubsan/_deps/uinteger_t-src
"$(brew --prefix llvm)/bin/clang++" -std=c++17 -O1 -g -fsanitize=fuzzer,undefined -I. -I"$UT" \
  test/fuzz_decode.cc -o fuzz_decode
./fuzz_decode -runs=20000
```

**On Linux.** The CI jobs run as they are. With `CC=clang CXX=clang++`, the sanitizer job is

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined
cmake --build build-asan --parallel 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan --output-on-failure --parallel 2
```

and the TSan job is the `build-tsan` tree above, run with `TSAN_OPTIONS="halt_on_error=1
second_deadlock_stack=1" ctest --test-dir build-tsan --output-on-failure`. The libFuzzer target
takes ASan there too: `clang++ -std=c++17 -O1 -g -fsanitize=fuzzer,address,undefined -I. -I"$UT"
test/fuzz_decode.cc`.

## Trying a change

In this order, each step on the change as it will land:

1. **Build without warnings** under `-Wall -Wextra`, with Clang and GCC (the `build-ubsan` tree above
   has them on).
2. **CTest** in the Debug UBSan tree and in Release, and the TSan link. On AArch64 `base_x` and
   `base_x_golden` take the table-lookup paths and the `_scalar` builds the scalar code; on x86-64
   both run the scalar code.
3. **The golden output.** `base_x_golden` passes on an unchanged file. For a deliberate output change,
   regenerate the file, read its diff line by line, and record the change in CHANGELOG.md:

   ```sh
   build-ubsan/base_x_golden --generate > test/golden/encode.txt
   git diff test/golden/encode.txt
   ```

   A new preset also goes in the `presets` table of `test/golden.cc`. Without CMake the test builds
   with `c++ -std=c++17 -I. -I"$UT" test/golden.cc -o golden` and runs as
   `./golden test/golden/encode.txt base_x.hh`.
4. **The fuzz.** CTest runs the fixed seed (`base_x_fuzz_decode N` runs N inputs); a change to decode,
   `is_valid` or the tables also gets a libFuzzer session.
5. **The mutant set** of the code touched, `nice -n 19 tools/mutate.py leading_zeros` or `simd`
   ([MUTATION.md](MUTATION.md)). Every mutant not argued equivalent is killed; a new branch, flag or
   boundary gets a mutant and the test that kills it. The `simd` set changes compiled code only on
   AArch64.
6. **A new uinteger_t pin** runs all of the above against the new commit. The golden test is what
   catches a change in the conversion's text; a uinteger_t change that can alter text runs it before
   it lands.
7. **The speed**, for a change meant to be faster: an A/A run, then an interleaved A/B of `bench.cc`
   against `master` on a quiet machine, with `bench/ab.sh -u "$UT"` ([BENCHMARKING.md](BENCHMARKING.md)),
   at 16, 32, 64 and a few hundred bytes. The change counts if it is faster by about 3% or more on the
   rows it targets and by more than the A/A spread; 10% counts without argument.

## Reproducing the experiments

**The README table** comes from `bench/results/2026-10-31-apple-m1-pro/` (`basex.txt` and
`basex_compare.txt`, unedited `bench` output, with `host.txt` and a `README.md` holding the build
line), and the Intel paragraph from `bench/results/2026-10-31-intel-i9-9980hk/`, which adds
`basex_compare_avx.txt` with Firedancer on AVX2. The table prints from the committed files:

```sh
cd bench/results/2026-10-31-apple-m1-pro
python3 ../../summarize.py --readme basex.txt basex_compare.txt
```

A new run needs libbase58 and Firedancer beside the build; BENCHMARKING.md (Reproducing the README
table) has the clones, the build line and `./bench 32 64 256 compare`. `bench/results/` holds only
the runs behind the README: the per-piece breakdowns in ARCHITECTURE.md and OPEN-WORK.md have no
committed raw output.

**The golden file** is the output of `base_x_golden --generate`, and the same on every platform: the
inputs come from a generator written in the test, since `std::` distributions differ between
standard libraries.

**The mutant counts** in MUTATION.md are `nice -n 19 tools/mutate.py leading_zeros` and
`nice -n 19 tools/mutate.py simd`, in the tool's default Debug `-O1` build on an Apple M4 Pro; on
x86-64 the seven `simd` mutants that edit the AArch64 blocks survive. `tools/mutate.py --check SET`
confirms that every edit still applies after a change to `base_x.hh`. To run a set against another
uinteger_t, pass `--define FETCHCONTENT_SOURCE_DIR_UINTEGER_T=/path/to/uinteger_t`.

**The fuzz count** is CTest's `base_x_fuzz_decode 20000` at seed 20261031; the seed is fixed in the
standalone driver.
