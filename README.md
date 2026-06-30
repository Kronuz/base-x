# base-x

![constexpr](https://img.shields.io/badge/constexpr-compile--time-blue)

BaseX encoder / decoder for C++.

## What it is

`base-x` is a header-only library that encodes and decodes binary data in an
arbitrary base, given the alphabet for that base. It treats the input as one big
unsigned integer and rewrites it in the target base, so it works for base2 all
the way up to base66 and anything in between. It ships ready-made alphabets for
the common ones (base16, base32, base58, base62, base64 and more), including
Bitcoin, Ripple, Flickr, Crockford and the RFC 4648 variants.

On top of plain base conversion it supports optional check digits and
checksums, case-insensitive alphabets, character translation (so visually
ambiguous glyphs decode to the same value), and RFC 4648 style block padding.

It is two headers: `base_x.hh` for the codec and the alphabet catalog, and
`uinteger_t.hh` for the arbitrary-precision unsigned integer it builds on.

## When to use it / when not

Use it when you need compact, human-friendly text for binary blobs (IDs, hashes,
keys) and you care about the exact alphabet. base58 and base62 avoid the
padding, `+`, `/` and look-alike characters that make base64 awkward in URLs and
when read aloud. The check-digit and checksum modes catch transcription errors,
which is handy for short identifiers people type or copy.

Skip it if you need standard, byte-aligned hex or base64 output. Because it
encodes the whole input as a single integer (long division, not fixed-size
blocks), the default alphabets are not interchangeable with `xxd` or a stock
base64 library. The RFC 4648 presets (`Base16::rfc4648()`, `Base32::rfc4648()`,
`Base64::rfc4648()`) do produce standard, padded output through the block-padding
path, but the plain `base16()` / `base64()` alphabets do not. If you want
streaming over data that does not fit in memory, this is the wrong tool: it
materializes the whole value at once.

## Install

Header-only, but it depends on
[uinteger_t](https://github.com/Kronuz/uinteger_t): `base_x.hh` does
`#include "uinteger_t.hh"` for the arbitrary-precision integer it builds on.
The codec itself is just one header:

```cpp
#include "base_x.hh"
```

The clean way to get both is CMake. The `base_x` `INTERFACE` target fetches
`uinteger_t` and propagates it transitively, so linking `base_x` puts both
`base_x.hh` and `uinteger_t.hh` on your include path:

```cmake
include(FetchContent)
FetchContent_Declare(
  base_x
  GIT_REPOSITORY https://github.com/Kronuz/base-x.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(base_x)

target_link_libraries(your_target PRIVATE base_x)   # uinteger_t comes along
```

The `base_x` target adds the include directory, links `uinteger_t`, and requests
`cxx_std_17`. If you'd rather drop the headers in by hand, put both `base_x.hh`
and `uinteger_t.hh` on your include path. C++17 is required.

## Usage

```cpp
#include <iostream>
#include "base_x.hh"

int main() {
    auto encoded = Base58::base58().encode("Hello world!");
    std::cout << encoded << std::endl;

    auto decoded = Base58::base58().decode(encoded);
    std::cout << decoded << std::endl;  // => Hello world!
    return 0;
}
```

Each alphabet is a static factory that returns a shared `const BaseX&`. Pick the
one you want and call `encode` / `decode` on it:

```cpp
Base62::base62().encode("Hello world!");   // => "T8dgcjRGuYUueWht"
Base16::base16().encode("ABC");            // => "414243"
Base32::crockford().encode(519571);        // => "FVCK"
```

The check variants append a check character that `decode` verifies and
`is_valid` can test without decoding:

```cpp
auto s = Base62::base62chk().encode("Hello world!");  // => "T8dgcjRGuYUueWhtE"
Base62::base62chk().is_valid(s);                       // => true
Base62::base62chk().decode(s);                         // => "Hello world!"
```

## Alphabets

A selection of the built-in factories. Each lives on a struct named for its base
(`Base58`, `Base62`, etc.) and most have a `*chk()` checksum twin.

| Base | Factory                | Alphabet
|------|------------------------|----------
|    2 | `Base2::base2()`       | `01`
|    8 | `Base8::base8()`       | `01234567`
|   16 | `Base16::base16()`     | `0123456789abcdef`
|   16 | `Base16::rfc4648()`    | `0123456789ABCDEF`
|   32 | `Base32::base32()`     | `0123456789abcdefghijklmnopqrstuv`
|   32 | `Base32::crockford()`  | `0123456789ABCDEFGHJKMNPQRSTVWXYZ`
|   32 | `Base32::rfc4648()`    | `ABCDEFGHIJKLMNOPQRSTUVWXYZ234567`
|   36 | `Base36::base36()`     | `0123456789abcdefghijklmnopqrstuvwxyz`
|   58 | `Base58::base58()`     | `0123456789ABCDEF...uv` (GMP order)
|   58 | `Base58::bitcoin()`    | `123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz`
|   58 | `Base58::ripple()`     | `rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz`
|   58 | `Base58::flickr()`     | `123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ`
|   59 | `Base59::base59()`     | `23456789abcdef...XYZ` (ambiguous chars translated)
|   62 | `Base62::base62()`     | `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`
|   62 | `Base62::inverted()`   | `0123456789abcdef...XYZ` (lowercase first)
|   64 | `Base64::base64()`     | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/`
|   64 | `Base64::url()`        | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_`
|   64 | `Base64::rfc4648()`    | base64 with `=` block padding
|   66 | `Base66::base66()`     | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~`

There are also `base11`, `base16chk`, `Base58::dubaluchk()`,
`Base64::rfc4648url()` / `rfc4648url_unpadded()`, and check twins for most of the
above. See the factory structs at the bottom of `base_x.hh` for the full list.

## API reference

Everything hangs off one class:

```cpp
class BaseX;
```

You rarely construct it directly. The factories build `constexpr BaseX`
encoders for you. The flags an alphabet can be built with:

```cpp
BaseX::ignore_case     // decode is case-insensitive
BaseX::with_checksum   // append/verify a trailing checksum character
BaseX::with_check      // append/verify a trailing check character
BaseX::block_padding   // RFC 4648 style fixed-block padding
```

### encode

```cpp
// All overloads default Result to std::string. The in-place forms write into
// an existing result; the value forms return a fresh one.
template <typename Result = std::string> Result encode(const uinteger_t& num) const;
template <typename Result = std::string> Result encode(const unsigned char* data, std::size_t size) const;
template <typename Result = std::string> Result encode(const char* data, std::size_t size) const;
template <typename Result = std::string> Result encode(std::string_view binary) const;
template <typename Result, typename T, std::size_t N> Result encode(T (&&s)[N]) const;  // string literal
```

`encode` takes binary input (a `uinteger_t`, a buffer, a `std::string_view`, or
a string literal) and returns its text representation in this alphabet. Passing
an integer literal encodes that integer.

### decode

```cpp
template <typename Result = std::string> Result decode(const char* encoded, std::size_t size) const;
template <typename Result = std::string> Result decode(std::string_view encoded) const;
template <typename Result, typename T, std::size_t N> Result decode(T (&&s)[N]) const;
void decode(uinteger_t& result, const char* encoded, std::size_t size) const;
```

`decode` reverses `encode`. With `Result = std::string` (the default) it returns
the original bytes; with an integral `Result` it returns the decoded integer. It
throws `std::invalid_argument` on an invalid character, a failed check digit, or
a failed checksum.

### is_valid

```cpp
bool is_valid(const char* encoded, std::size_t size) const;
bool is_valid(std::string_view encoded) const;
template <typename T, std::size_t N> bool is_valid(T (&&s)[N]) const;
```

`is_valid` reports whether a string only contains alphabet characters and, for
checksum alphabets, whether the checksum holds. It never throws.

## Build & test

Header-only, so there's nothing to compile for use. CMake fetches the
`uinteger_t` dependency and runs the smoke test:

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

To compile the test directly, put `uinteger_t.hh` on the include path too (it is
not vendored here):

```sh
c++ -std=c++17 -I. -I/path/to/uinteger_t test/test.cc -o /tmp/basex_test && /tmp/basex_test
```

The test prints `base-x OK: ...` and exits 0 on success. It uses `assert`, so
build without `NDEBUG`.

## Notes & caveats

- The default alphabets are not byte-aligned standards. `Base16::base16()` and
  `Base64::base64()` are not the same as stock hex or base64; they encode the
  whole input as one integer. Use the `rfc4648*` presets when you need standard,
  padded output.
- Because encoding goes through `uinteger_t`, a leading zero byte does not round
  into a leading alphabet character the way Bitcoin base58check expects. These
  alphabets encode the numeric value, not a fixed-width byte string.
- `decode` and the checked encoders throw `std::invalid_argument`. If you cannot
  afford exceptions on bad input, gate with `is_valid` first.
- `ignore_case` only affects decoding; encoding always emits the alphabet's own
  casing.

## Examples

[`examples/demo.cc`](examples/demo.cc) is a runnable tour. A top-level CMake build
produces it next to the test:

```sh
cmake -B build && cmake --build build && ./build/base_x_demo
```

It encodes one short payload across base16/32/58/62/64 and decodes each back so
you can watch the same bytes get rewritten in a different base per row; lines up
the four base58 variants (GMP, bitcoin, ripple, flickr) to show they are the same
number in different look-alike-safe glyphs; encodes an integer literal (the
classic `FVCK` Crockford vector); contrasts the plain `base64()` integer encoding
with the standard, `=`-padded `rfc4648()` output for the same byte; flips a
character to show a check-digit alphabet catch it via `is_valid` without throwing;
and round-trips a raw 16-byte binary blob.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand), where `base_x.hh` is
vendored under `src/`. That copy is the more actively maintained one; this repo
tracks it. The arbitrary-precision integer it builds on lives in its own repo,
[uinteger_t](https://github.com/Kronuz/uinteger_t), which this library depends on
rather than vendoring.

## License

MIT. Copyright (c) 2017,2019 German Mendez Bravo (Kronuz). See [LICENSE](LICENSE).
The [uinteger_t](https://github.com/Kronuz/uinteger_t) dependency carries its own
license.
