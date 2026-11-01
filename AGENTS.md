# AGENTS

Notes for agents and contributors working in this repo.

## Repo map

- `base_x.hh` — the codec. One class, `BaseX`, plus the alphabet factory structs
  (`Base2`, `Base8`, `Base11`, `Base16`, `Base32`, `Base36`, `Base58`, `Base59`,
  `Base62`, `Base64`, `Base66`). Header-only.
- `test/test.cc` — smoke test: encode/decode round-trips across base16/base32/
  base58/base62/base64 plus the check and checksum paths, Bitcoin Core's and the XRP Ledger's base58
  vectors for the `leading_zeros` presets, and the RFC 4648 and block-size cases.
- `test/golden.cc`, `test/golden/encode.txt` — the golden-output test. The file has one line per
  case: the text each preset encodes for a fixed set of inputs (empty, zero bytes, runs of 0xff,
  powers of 256, leading zero bytes, random bytes from a generator written in the test, `uinteger_t`
  values, and long inputs kept as the length and hash of their text), or the exception type where
  `encode` throws. CTest recomputes every line on the SIMD and the scalar code and compares, checks
  that each text decodes back as its preset's semantics say, and checks that the test's preset table
  and `base_x.hh` list the same presets. It reports its failures itself, so it checks under `NDEBUG`.
  `base_x_golden --generate` writes the file.
- `bench/bench.cc` — encode and decode timings for the base58, base62, base64 and base16
  presets, and base58 against a classic byte-wise codec, libbase58 and Firedancer when
  built with `-DHAVE_LIBBASE58` / `-DHAVE_FD_BASE58` (build lines at the top of the file).
  Not in CTest: timings are not tests.
- `bench/results/` — the raw `bench` output behind the README table, one directory per machine and date,
  each with a `README.md` (machine, compiler, versions, commits, and the `summarize.py` command that
  prints the table from the files).
- `tools/mutate.py`, `tools/mutants/*.txt` — mutation testing. A mutants file lists edits to `base_x.hh`
  (`leading_zeros`: the count of zero bytes and digits, the gates and the presets; `simd`: the AArch64
  table-lookup paths), each with the CTest tests that should fail on it; the tool applies one edit at a
  time to a temporary copy of the repository, builds and runs those tests through CMake, and reports
  each mutant killed, survived or not applicable. The format, options and sets are in
  `docs/MUTATION.md`.
- `bench/ab.sh`, `bench/summarize.py` — `ab.sh` runs two `base_x.hh` (git refs or directories)
  over `bench.cc` interleaved, A B A B ..., or one against itself with `-a`, with the
  `uinteger_t.hh` directory from `-u`; `summarize.py` prints the best time per row, A against
  B with the spreads, or the README's base58 table (`--readme`). The reproduction commands and
  the A/A and A/B protocol are in `docs/BENCHMARKING.md`.
- `test/fuzz_decode.cc` — fuzz target for `decode` and `is_valid` (libFuzzer, or `-DFUZZ_STANDALONE`
  for a fixed-seed driver that CTest runs): strict presets answer to a model (Bitcoin's and Ripple's
  base58 with leading zero digits as zero bytes), the block-padded (RFC 4648)
  presets to a bit-level one, and every preset has to hold up and agree with itself.
- `CMakeLists.txt` — defines the `base_x` `INTERFACE` target (which fetches the
  `uinteger_t` dependency via FetchContent) and a `ctest` test.
- `LICENSE` — MIT, Copyright (c) 2017,2019 German Mendez Bravo (Kronuz).

The library depends on [uinteger_t](https://github.com/Kronuz/uinteger_t)
(`base_x.hh` does `#include "uinteger_t.hh"`); it is fetched by CMake, not
vendored here.
- `README.md` — what it is, install, quick start.
- `docs/API.md` — the API reference, the alphabet catalog, and caveats.
  `docs/BENCHMARKING.md` — what `bench.cc`
  measures, how to reproduce the README table, and the A/A and A/B protocol. `docs/MUTATION.md` — the
  mutation testing tool and its mutants files.
- `CMakeLists.txt` — the `base_x` target (alias `base_x::base_x`).
- `ARCHITECTURE.md` — encoding scheme, encode and decode through uinteger_t's
  digit API, check/checksum/padding, complexity, and the measurements behind the table paths
  and the decode order. `docs/OPEN-WORK.md` lists the behavior open to change, the slow paths
  and what the tests do not reach.
- `tests/` — the legacy googletest suite (`tests/testcases/tests.cc`, not built
  by CMake or CI) and its Makefile,
  kept for the broader vector coverage. The CMake/`test/` smoke test is the one
  CI-light path that needs no external dependency.

## Build & run the test

```sh
c++ -std=c++17 -I. test/test.cc -o /tmp/basex_test && /tmp/basex_test
# or:
cmake -B build && cmake --build build && ctest --test-dir build
```

The smoke test prints `base-x OK: ...` and exits 0 on success. It uses `assert`,
so build without `NDEBUG`: a Release CTest build defines it, and `base_x` and `base_x_scalar`
then pass without checking anything. The golden test does not: it checks in a Release build too, and prints
`base-x golden OK: ...` (standalone: `c++ -std=c++17 -I. -I<uinteger_t dir> test/golden.cc -o
/tmp/golden && /tmp/golden test/golden/encode.txt base_x.hh`). The full `tests/` suite needs
googletest on the include path (see `tests/Makefile`).

## Conventions

- C++17. The code uses `std::string_view` and `constexpr` constructors.
- Header-only (`base_x.hh`); there is no `.cc` to compile for the library itself.
  It depends on the `uinteger_t` library, fetched by CMake (not vendored here).
- Indentation is tabs, matching the existing source.
- Use double quotes in code blocks per the docs' style.

## Invariants

- `_chr` maps digit -> character; `_ord` maps byte -> digit, with non-alphabet
  bytes set to the sentinel `size` (alphabet plus extended characters, so it is no
  character's ordinal; the extended characters take `alphabet_base` up to `size - 1`)
  and translate-to-ignore bytes set negative. Decoding relies on all of these: an
  ordinal `>= alphabet_base` is not a digit (it is an error, or the padding
  character), `< 0` is ignored, and a check or checksum character is valid only
  below `size`.
- The factory encoders are `constexpr` and built once at compile time. Don't make
  the constructor non-`constexpr`.
- Base conversion is not in this repo: `encode` calls `uinteger_t::digits()` and
  `decode` calls `uinteger_t::from_digits()`, and base-x only maps digits through
  `_chr`/`_ord`. Speedups to the conversion belong in uinteger_t (a conversion loop here
  would copy the library's and drift from it); bump the pin in `CMakeLists.txt` when base-x
  needs a new uinteger_t API.
- `_ord8` is `_ord` for the 128 ASCII bytes as bytes (the digit, 0xff for any other); the 16-at-a-time
  scan reads it and the scalar loop in `decode` stays the only code that classifies a byte that is not a
  digit. Both table-lookup paths are for alphabets of up to 64 characters on AArch64;
  `BASEX_NO_SIMD` turns them off, and CTest runs the test and the golden test both ways.
- `block_size` is nonzero only when `block_padding` is set. The RFC 4648 presets
  depend on this; the plain alphabets must keep it zero.
- `leading_zeros` works where bytes become digits and back: the byte `encode` counts the leading zero
  bytes and `encode_value` writes that many zero digits before the number's; `decode_value` counts the
  leading zero digits and returns the count, which the `Result` form of `decode` turns into zero bytes.
  The count goes in front of the digits before they are mapped, so padding, checksum and check cover
  it. The constructor asserts the flag never combines with `block_padding`. Alphabets without the flag
  skip the scans and ignore the count (the `uinteger_t` form passes one for zero and nothing reads it),
  so their output does not depend on it.
- `decode` and the checked encoders throw `std::invalid_argument` on bad input,
  bad check digit, or bad checksum. `is_valid` checks the same framing and the
  checksum without throwing; it does not recompute a check character's value.
- The padding character is never a digit or an ignored character (the constructor
  asserts it), and an alphabet without padding has none: its `padding` member is
  `'\0'`, so test `padding_size` before comparing with it.
- `encode` into a non-empty `Result` appends; the padding and the checksum cover
  only the appended text (measure from `first`, never `result.size()`).

## How to extend

- Adding an alphabet means adding a `static const BaseX&` factory to the matching
  base struct (or a new struct), built with the right flags, alphabet, extended
  set, padding, and translate strings. Mirror an existing one.
- The check feature draws its extra character from the alphabet plus the
  `extended` set, so a `with_check` alphabet needs enough extended characters to
  cover `size = alphabet + extended`.
- Add a matching round-trip (and, for vectors you can pin, an exact-output)
  assertion to `test/test.cc`. Prefer real vectors from `tests/tests.cc`.
- A new preset also goes in the `presets` table of `test/golden.cc` (`keeps` is true when it has
  `leading_zeros` or `block_padding`), and the golden file is regenerated; `base_x_golden` fails
  while the table and `base_x.hh` list different presets.

## Traps

- These alphabets are not byte-aligned standards. `Base16::base16()` /
  `Base64::base64()` are not stock hex/base64; only the `rfc4648*` presets are.
  Don't "fix" them to match a standard library.
- The codec encodes the numeric value, so a leading zero byte is not a leading character unless the
  alphabet says so, and two flags do. `block_padding`: the `rfc4648*` presets encode the input's
  bytes, so a leading zero byte is a leading zero digit and decode returns it. `leading_zeros`:
  `Base58::bitcoin()` and `ripple()` write one zero digit for each leading zero byte and the number of
  the rest after them, and decode reverses it (Bitcoin's convention); no bytes are no text, and a
  `uinteger_t` argument has no leading zero bytes. The flags never combine (the constructor asserts),
  and `bitcoinchk()`, `ripplechk()` and every other preset keep the numeric text.
- A block-padded alphabet takes its digit count from the input's byte count
  (`encode_value`), and `decode` takes the byte count from the digit count; a
  `uinteger_t` argument stands for its shortest byte string (one byte for zero). The digit
  count is worked out in `block_digits`, a template on the size type that throws
  `std::length_error` where 8 times the byte count would wrap; `test/test.cc` runs it at 16 and
  32 bits (the guard cannot be reached at a 64-bit `size_t`), reaching it with
  `BASEX_PUBLIC_IMPLEMENTATION` (`BASEX_PRIVATE` stands for `private`).
- `ignore_case` affects decoding only. Encoding always emits the alphabet's own
  casing.
- A string literal is an lvalue: `encode("\0\0abc")` takes the `std::string_view` overload and stops
  at the first NUL. The `T (&&)[N]` overloads take only rvalue arrays. Tests and examples with binary
  bytes pass a `std::string_view` with its size.
- `encode` does not compile for a `std::vector` result: the padding step calls `result.append`, in
  a run-time branch every `Result` compiles. `decode` into a vector works.
- `decode_value` assigns its `uinteger_t` before it verifies the check and checksum characters, so
  every public form of `decode` passes it a local and assigns the caller's result only after it
  returns; a failed check leaves the caller's value as it was. Keep it that way when adding a form.
- The constructor's checks (the `leading_zeros` and `block_padding` pair, a padding character that
  is a digit) are asserts, so an alphabet built against them under `NDEBUG` encodes wrong text
  silently. An alphabet with both `block_padding` and `with_checksum` passes them and still fails its
  own decode: `encode` counts the padding in the checksum's length and `decode` does not. No preset
  combines either pair.
- `base_x.hh` is versioned together with Xapiand's vendored copy under
  `Xapiand/src/`. When de-staling, diff against that copy; it is the more
  actively maintained one. Keep the MIT header intact when editing. (`uinteger_t`
  is its own repo now and is de-staled there, not here.)
- The text of every preset is a stored format: Xapiand keeps `Base59::dubaluchk()` UUIDs,
  `Base62::inverted()` node ids and `Base64::rfc4648url_unpadded()` document ids. An output change in
  any preset (an alphabet, a flag, the conversion in `uinteger_t`) fails `base_x_golden` until
  `test/golden/encode.txt` is regenerated with `build/base_x_golden --generate >
  test/golden/encode.txt`, and it needs a `CHANGELOG.md` entry. Regenerate only for a deliberate
  change, and read the diff of the file: each changed line is a stored text that changes.
- `Base59::dubaluchk()`'s alphabet and `Base64`'s `rfc4648url_unpadded()` came
  from the Xapiand sync; they are intentional, not stray edits.

## Rules of thumb

- A change to any preset's output is a format change: users store the encoded text (Xapiand stores
  some). Before landing, encode the same inputs with every preset on master and on the branch and
  compare the text.
- Keep the scalar decode loop to one `_ord` lookup and one comparison per character: translation,
  case folding and every byte that is not a digit live in the table, and a rare byte leaves the loop.
- A test proves a property only if a mutant of the code it guards fails it: break that code with one
  edit (a flipped comparison, a skipped check) and watch the test fail. `tools/mutate.py` does that for
  a file of edits (`tools/mutants/`, `docs/MUTATION.md`) and lists the mutants no test kills: run the set
  of the code you change, and add a mutant for each branch you add.
- Time a change with an A/A run first (the same build against itself, for the noise), then an
  interleaved A/B of `bench/bench.cc` on a quiet machine. Keep it only if it is faster by about 3% or
  more and by more than the A/A spread.

## Provenance

This code was extracted from
[Xapiand](https://github.com/Kronuz/Xapiand), where `src/base_x.hh` is vendored.
That copy is the more actively maintained one; this repo tracks it. The
arbitrary-precision integer it uses lives in its own repo,
[uinteger_t](https://github.com/Kronuz/uinteger_t), depended on rather than
vendored. Keep the MIT header intact when editing.
