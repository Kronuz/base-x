# base-x

[![CI](https://github.com/Kronuz/base-x/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/base-x/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Encode and decode binary data in any base from 2 to 66, in C++17: base58,
base62, base32 Crockford, base64, and dozens more, with optional check
characters. For compact, human-friendly IDs, keys, and hashes.

![base-x: bytes become one big integer, rewritten in the alphabet's base](docs/base-x.svg)

## ✨ Highlights

- **Any alphabet.** Ready-made Bitcoin, Ripple, and Flickr base58, base62,
  Crockford base32, URL-safe base64, the RFC 4648 variants, and more; or bring
  your own.
- **Check characters.** The `*chk()` alphabets append a check character that
  catches transcription errors; `is_valid` tests without decoding.
- **Forgiving decoding.** Case-insensitive alphabets and translated look-alikes:
  Crockford base32 reads `O` as `0`, `I` and `L` as `1`, and skips `-`.
- **Standard output when you need it.** The `rfc4648()` presets produce padded,
  byte-aligned hex, base32, and base64.
- **Header-only**, on [uinteger_t](https://github.com/Kronuz/uinteger_t)'s
  arbitrary-precision integers.

## 📦 Installation

Requires a C++17 compiler; CMake 3.14 or newer for the CMake route, which fetches
`uinteger_t` for you. CI builds it on Linux (x86_64, arm64), macOS (arm64,
x86_64), and FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(base_x
  GIT_REPOSITORY https://github.com/Kronuz/base-x.git
  GIT_TAG        master)
FetchContent_MakeAvailable(base_x)

target_link_libraries(my_app PRIVATE base_x::base_x)   # brings uinteger_t along
```

Without CMake, put both `base_x.hh` and `uinteger_t.hh` on your include path.

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

[`examples/demo.cc`](examples/demo.cc) tours the alphabets, check characters,
and RFC 4648 padding.

## 🎯 When to use it

- ✅ Short, readable, URL-safe identifiers, with the exact alphabet you choose.
- ❌ Interchange with `xxd` or a stock base64 library through the plain
  `base16()`/`base64()` alphabets: they encode the input as one integer. Use the
  `rfc4648()` presets.
- ❌ Bitcoin-compatible base58check of arbitrary bytes: leading zero bytes are not
  kept as leading characters.
- ❌ Streaming data larger than memory: the whole value is converted at once.

## 📚 Documentation

- [docs/API.md](docs/API.md): `encode`, `decode`, `is_valid`, every built-in
  alphabet, and the caveats.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the codec, check digits, and padding
  work.
- [AGENTS.md](AGENTS.md): repository map and traps.

## 🛠️ Building and testing

```sh
cmake -B build && cmake --build build && ctest --test-dir build
./build/base_x_demo
```

The tests use `assert`, so build without `NDEBUG`.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) first. CI must
pass on every platform.

## 📄 License

MIT, Copyright (c) 2017 Germán Méndez Bravo. See [LICENSE](LICENSE).
