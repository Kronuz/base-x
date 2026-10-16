# base-x API reference

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

## encode

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

## decode

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

## is_valid

```cpp
bool is_valid(const char* encoded, std::size_t size) const;
bool is_valid(std::string_view encoded) const;
template <typename T, std::size_t N> bool is_valid(T (&&s)[N]) const;
```

`is_valid` reports whether a string only contains alphabet characters and, for
checksum alphabets, whether the checksum holds. It never throws.

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
