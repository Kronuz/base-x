# AGENTS

Notes for agents and contributors working in this repo.

## Repo map

- `base_x.hh` — the codec. One class, `BaseX`, plus the alphabet factory structs
  (`Base2`, `Base8`, `Base11`, `Base16`, `Base32`, `Base36`, `Base58`, `Base59`,
  `Base62`, `Base64`, `Base66`). Header-only.
- `test/test.cc` — smoke test: encode/decode round-trips across base16/base32/
  base58/base62/base64 plus the check and checksum paths.
- `CMakeLists.txt` — defines the `base_x` `INTERFACE` target (which fetches the
  `uinteger_t` dependency via FetchContent) and a `ctest` test.
- `LICENSE` — MIT, Copyright (c) 2017,2019 German Mendez Bravo (Kronuz).

The library depends on [uinteger_t](https://github.com/Kronuz/uinteger_t)
(`base_x.hh` does `#include "uinteger_t.hh"`); it is fetched by CMake, not
vendored here.
- `README.md` — usage, alphabet catalog, API reference.
- `ARCHITECTURE.md` — encoding scheme, the two encode paths, check/checksum/
  padding, complexity.
- `tests/` — the legacy googletest suite (`tests/tests.cc`) and its Makefile,
  kept for the broader vector coverage. The CMake/`test/` smoke test is the one
  CI-light path that needs no external dependency.

## Build & run the test

```sh
c++ -std=c++17 -I. test/test.cc -o /tmp/basex_test && /tmp/basex_test
# or:
cmake -B build && cmake --build build && ctest --test-dir build
```

The smoke test prints `base-x OK: ...` and exits 0 on success. It uses `assert`,
so build without `NDEBUG`. The full `tests/` suite needs googletest on the
include path (see `tests/Makefile`).

## Conventions

- C++17. The code uses `std::string_view`, `constexpr` constructors, and the
  power-of-two fast path.
- Header-only (`base_x.hh`); there is no `.cc` to compile for the library itself.
  It depends on the `uinteger_t` library, fetched by CMake (not vendored here).
- Indentation is tabs, matching the existing source.
- Use double quotes in code blocks per the docs' style.

## Invariants

- `_chr` maps digit -> character; `_ord` maps byte -> digit, with non-alphabet
  bytes set to the sentinel `alphabet_base` and translate-to-ignore bytes set
  negative. Decoding relies on both: `>= alphabet_base` is an error, `< 0` is
  ignored.
- The factory encoders are `constexpr` and built once at compile time. Don't make
  the constructor non-`constexpr`.
- `alphabet_base_bits` is nonzero only for power-of-two bases and selects the
  shift/mask encode path. Everything else goes through `divmod`.
- `block_size` is nonzero only when `block_padding` is set. The RFC 4648 presets
  depend on this; the plain alphabets must keep it zero.
- `decode` and the checked encoders throw `std::invalid_argument` on bad input,
  bad check digit, or bad checksum. `is_valid` performs the checksum test without
  throwing.

## How to extend

- Adding an alphabet means adding a `static const BaseX&` factory to the matching
  base struct (or a new struct), built with the right flags, alphabet, extended
  set, padding, and translate strings. Mirror an existing one.
- The check feature draws its extra character from the alphabet plus the
  `extended` set, so a `with_check` alphabet needs enough extended characters to
  cover `size = alphabet + extended`.
- Add a matching round-trip (and, for vectors you can pin, an exact-output)
  assertion to `test/test.cc`. Prefer real vectors from `tests/tests.cc`.

## Traps

- These alphabets are not byte-aligned standards. `Base16::base16()` /
  `Base64::base64()` are not stock hex/base64; only the `rfc4648*` presets are.
  Don't "fix" them to match a standard library.
- The codec encodes the numeric value, so leading zero bytes are not preserved as
  leading characters the way Bitcoin base58check expects. This is by design.
- `ignore_case` affects decoding only. Encoding always emits the alphabet's own
  casing.
- `base_x.hh` is versioned together with Xapiand's vendored copy under
  `Xapiand/src/`. When de-staling, diff against that copy; it is the more
  actively maintained one. Keep the MIT header intact when editing. (`uinteger_t`
  is its own repo now and is de-staled there, not here.)
- `Base58::dubaluchk()`'s alphabet and `Base64`'s `rfc4648url_unpadded()` came
  from the Xapiand sync; they are intentional, not stray edits.

## Provenance

This code was extracted from
[Xapiand](https://github.com/Kronuz/Xapiand), where `src/base_x.hh` is vendored.
That copy is the more actively maintained one; this repo tracks it. The
arbitrary-precision integer it uses lives in its own repo,
[uinteger_t](https://github.com/Kronuz/uinteger_t), depended on rather than
vendored. Keep the MIT header intact when editing.
