# base-x

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/base-x/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/base-x/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Header-only base-N encoding for C++17: base58, base62, Crockford base32, base64 or any
alphabet, with check characters. It reads the input as one big
[uinteger_t](https://github.com/Kronuz/uinteger_t) and writes it in the alphabet's base, with
built-in alphabets from base 2 to base 66. Reach for it for compact, human-friendly IDs, keys
and hashes.

## ✨ Highlights

- **Any alphabet.** Ready-made Bitcoin, Ripple, and Flickr base58, base62,
  Crockford base32, URL-safe base64, the RFC 4648 variants, and more; or bring
  your own.
- **Fast.** A 32-byte key encodes in base58 in 74 ns and decodes in 72 on an Apple M1 Pro,
  where libbase58 takes 2.1 µs and 0.5 µs ([Performance](#-performance)).
- **Check characters.** The `*chk()` alphabets append a check character that
  catches transcription errors; `is_valid` tests without decoding.
- **Forgiving decoding.** Case-insensitive alphabets and translated look-alikes:
  Crockford base32 reads `O` as `0`, `I` and `L` as `1`, and skips `-`.
- **Standard output when you need it.** The `rfc4648()` presets produce padded,
  byte-aligned hex, base32, and base64, byte for byte what Python's `base64` module
  writes, leading zero bytes included.
- **Header-only**, on [uinteger_t](https://github.com/Kronuz/uinteger_t)'s
  arbitrary-precision integers.

## 🚀 Quick start

```cpp
#include "base_x.hh"

#include <iostream>

int main() {
	auto id = Base62::base62().encode("Hello world!");    // "T8dgcjRGuYUueWht"
	std::cout << id << '\n' << Base62::base62().decode(id) << '\n';

	auto checked = Base62::base62chk().encode("Hello world!");
	std::cout << Base62::base62chk().is_valid(checked) << '\n';   // 1

	std::cout << Base32::crockford().encode(519571) << '\n';      // "FVCK"
}
```

[`examples/demo.cc`](examples/demo.cc) tours the alphabets, check characters, and RFC 4648
padding.

## 📦 Installation

Requires a C++17 compiler and CMake 3.14 or newer. CMake fetches
[uinteger_t](https://github.com/Kronuz/uinteger_t) at a pinned commit. CI builds it on Linux
(x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(base_x
  GIT_REPOSITORY https://github.com/Kronuz/base-x.git
  GIT_TAG        919698efc7042da21158247e73c49c80f795b0be)  # master tracks the tip
FetchContent_MakeAvailable(base_x)

target_link_libraries(my_app PRIVATE base_x::base_x)   # brings uinteger_t along
```

Then `#include "base_x.hh"`. Without CMake, put both `base_x.hh` and `uinteger_t.hh` on your
include path.

## 🎯 When to use it

Use it for short, readable, URL-safe identifiers in the exact alphabet you choose, and for
Bitcoin and XRP Ledger base58 of arbitrary bytes: `Base58::bitcoin()` and `ripple()` write each
leading zero byte as a leading zero digit and read it back.

Not for:

- Interchange with `xxd` or a stock base64 library through the plain `base16()` and `base64()`
  alphabets: they encode the input as one integer. Use the `rfc4648()` presets.
- Base58Check's checksum: there is no double SHA-256 here; the `*chk()` alphabets add one check
  character of base-x's own.
- Streaming data larger than memory: the whole value is converted at once.

## ⚡ Performance

Nanoseconds per base58 call on an Apple M1 Pro (Apple clang 21, `-O2`), the best of seven
rounds of [`bench/bench.cc`](bench/bench.cc), against [Firedancer](https://github.com/firedancer-io/firedancer)'s
fixed-size codec (32 and 64 bytes only) and [libbase58](https://github.com/bitcoin/libbase58).
`encode` and `decode` return a new `std::string`; the "into a string" column writes into one
string that keeps its capacity, as the other codecs write into a caller's buffer.

| bytes | operation | base-x | into a string | Firedancer | libbase58 |
|---:|---|---:|---:|---:|---:|
| 32 | encode | 74.3 | 52.0 | 48.5 | 2,086 |
| 32 | decode | 71.6 | 51.6 | 63.3 | 496 |
| 64 | encode | 134 | 114 | 112 | 9,300 |
| 64 | decode | 98.2 | 78.2 | 198 | 1,749 |
| 256 | encode | 806 | 774 | - | 159,990 |
| 256 | decode | 537 | 507 | - | 14,182 |

On an Intel Core i9-9980HK (Apple clang 17, same flags) base-x encodes in about twice
Firedancer's time (32 bytes: 144 ns against 70) and decodes 64 bytes in about the same time
(277 ns against 287). The table is the best of two runs of the benchmark built with both
comparators (shallow clones of their default branches; the build lines are in
[docs/BENCHMARKING.md](docs/BENCHMARKING.md)):

```sh
./bench 32 64 256 compare
```

## 📚 Documentation

- [docs/API.md](docs/API.md): `encode`, `decode`, `is_valid`, every built-in alphabet, and the
  caveats.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the codec, check characters and padding work.
- [docs/BENCHMARKING.md](docs/BENCHMARKING.md): reproducing the performance table and judging a
  change.
- [docs/OPEN-WORK.md](docs/OPEN-WORK.md): behavior open to change, the slow paths, and what the
  tests do not reach.
- [CHANGELOG.md](CHANGELOG.md): changes by date.
- [AGENTS.md](AGENTS.md): repository map and traps.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#base-x): include
  line, target and key API.

## 🛠️ Building and testing

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The tests use `assert`, so build without `NDEBUG`. `build/base_x_demo` runs the tour. The
benchmark is not a CTest target; it builds by hand, and the lines that add libbase58 and
Firedancer are at the top of [`bench/bench.cc`](bench/bench.cc).

## 🤝 Contributing

Read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
before opening a pull request; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2017-2026 Germán Méndez Bravo. See [LICENSE](LICENSE).
