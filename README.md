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
- **Fast.** A 32-byte key encodes in base58 in 50 ns and decodes in 52 on an Apple M4 Pro,
  where libbase58 takes 1.2 µs and 0.18 µs ([Performance](#-performance)).
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

`bench/bench.cc` times base58 (`Base58::bitcoin()`) on random inputs of 32, 64 and 256 bytes, in
nanoseconds per call (lower is faster), the best of seven rounds of at least 10 ms, then the best
of two runs. `base-x` calls `encode` and `decode` as a user does, each returning a new
`std::string`; `into a string` is the same codec writing into one string that keeps its capacity,
as the other codecs write into a caller's buffer. `Firedancer` is the fixed-size base58 codec of
[Firedancer](https://github.com/firedancer-io/firedancer) v26.10.5 (`6286f99`), which takes 32 and
64 bytes only (`-` elsewhere), and `libbase58` is
[libbase58](https://github.com/bitcoin/libbase58) v0.1.4 (`16c2527`). The benchmark checks every
codec's output against base-x's before it times a row. Apple clang 17.0.0 with `-std=c++17 -O2`
for `bench.cc` and `-O2` for the C codecs.

Apple M4 Pro, macOS 26.7:

| bytes | operation | base-x | into a string | Firedancer | libbase58 |
|---:|---|---:|---:|---:|---:|
| 32 | encode | 49.7 | 38.6 | 41.4 | 1,234 |
| 32 | decode | 52.1 | 35.7 | 44.0 | 179 |
| 64 | encode | 86.1 | 73.3 | 95.6 | 5,174 |
| 64 | decode | 69.6 | 53.0 | 105 | 591 |
| 256 | encode | 509 | 490 | - | 90,734 |
| 256 | decode | 362 | 349 | - | 7,672 |

Intel Core i9-9980HK, macOS 15.8:

| bytes | operation | base-x | into a string | Firedancer | libbase58 |
|---:|---|---:|---:|---:|---:|
| 32 | encode | 149 | 97.4 | 71.8 | 1,633 |
| 32 | decode | 205 | 150 | 91.2 | 232 |
| 64 | encode | 268 | 212 | 157 | 6,726 |
| 64 | decode | 295 | 241 | 285 | 762 |
| 256 | encode | 1,486 | 1,367 | - | 104,611 |
| 256 | decode | 1,436 | 1,304 | - | 14,096 |

At 32 bytes base-x as called takes 1.2 times Firedancer's time on the M4 Pro (0.8 to 0.9 writing
into a string) and 2.1 to 2.2 times on the i9-9980HK (1.4 to 1.6 writing into a string). At 64
bytes it takes 0.7 to 0.9 times Firedancer's time on the M4 Pro and 1.0 to 1.7 times on the
i9-9980HK. Firedancer covers 32 and 64 bytes only, and libbase58 is slower than base-x at every
size.

The comparators are built from shallow clones of their tags; the build lines are in
[docs/BENCHMARKING.md](docs/BENCHMARKING.md), and the unedited runs are in
[bench/results/](bench/results/):

```sh
./basex_bench 32 64 256 compare
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
