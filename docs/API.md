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
BaseX::leading_zeros   // each leading zero byte is a leading zero digit (Bitcoin's base58)
```

`leading_zeros` and `block_padding` do not combine (the constructor asserts). `Base58::bitcoin()`
and `Base58::ripple()` have `leading_zeros`; no other preset does, `Base58::bitcoinchk()` and
`ripplechk()` included.

## encode

```cpp
// All overloads default Result to std::string. The in-place forms write into
// an existing result; the value forms return a fresh one.
template <typename Result = std::string> Result encode(const uinteger_t& num) const;
template <typename Result = std::string> Result encode(const unsigned char* data, std::size_t size) const;
template <typename Result = std::string> Result encode(const char* data, std::size_t size) const;
template <typename Result = std::string> Result encode(std::string_view binary) const;
template <typename Result, typename T, std::size_t N> Result encode(T (&&s)[N]) const;  // rvalue array
```

`encode` takes binary input (a `uinteger_t`, a buffer, a `std::string_view`, or
an rvalue array) and returns its text representation in this alphabet. Passing
an integer literal encodes that integer. A string literal is an lvalue, so it takes the
`std::string_view` form and ends at its first NUL byte: `encode("\0\0abc")` encodes no bytes. Pass
binary data with its size. The `Result` of `encode` is a `std::basic_string`; a `std::vector`
result compiles for `decode` only.

The `rfc4648*` presets encode the bytes of a buffer: a leading zero byte is a leading zero
digit (`Base64::rfc4648()` writes `AAAAAAAAAQ==` for six zero bytes and `0x01`, as Python's
`base64.b64encode` does), and an empty buffer is the empty text. A `uinteger_t` stands for its
shortest byte string, one byte for zero. `Base58::bitcoin()` and `Base58::ripple()`
(`leading_zeros`) write the Bitcoin way: a `0x00` byte at the start of the buffer is one leading
zero digit (`1` in Bitcoin's alphabet, `r` in Ripple's), the other bytes are written as one number,
and an empty buffer is the empty text (the bytes `00 00 61 62 63` are `11ZiCa`, two zero bytes are
`11`). A `uinteger_t`
has no leading zero bytes: zero is the one zero digit, and any other number is its digits alone.
The other alphabets encode the numeric value, which
has no leading zero digits, and an empty buffer throws `std::invalid_argument`. An `rfc4648*`
encode throws `std::length_error` when 8 times the byte count does not fit a `std::size_t`
(512 MiB or more where it is 32 bits).

The in-place forms append to `result` and leave what it already holds alone. The
padding and the checksum cover only the text that call appended, so the appended
text is the same as the fresh result of the value form.

## decode

```cpp
template <typename Result = std::string> Result decode(const char* encoded, std::size_t size) const;
template <typename Result = std::string> Result decode(std::string_view encoded) const;
template <typename Result, typename T, std::size_t N> Result decode(T (&&s)[N]) const;
void decode(uinteger_t& result, const char* encoded, std::size_t size) const;
```

`decode` reverses `encode`. With `Result = std::string` (the default) it returns
the original bytes; with an integral `Result` it returns the decoded integer, truncated with no
overflow error to the low bits that fit the type, at most one limb (64 bits by default). The
`rfc4648*` presets return exactly the bytes their digits hold, leading zero bytes
included (eight bits of digits make a byte; the digits past the last whole byte, the
dangling bits of base64 or the last digit of an odd count of hex digits, are dropped),
and the integral and `uinteger_t` forms return the number. `Base58::bitcoin()` and
`Base58::ripple()` return a zero byte for each digit at the start of the text (after the
characters the alphabet skips), then the bytes of the number the rest spells; the empty text
is no bytes, and a text of zero digits is that many zero bytes (`11` is two). The integral
and `uinteger_t` forms return the number, so the leading zero digits carry no value there. It
throws `std::invalid_argument` on an invalid character, a failed check digit, a
failed checksum, or input shorter than the check and checksum characters its
alphabet needs (an empty string for any `*chk` alphabet). When it throws, `result` is
left as it was, in the byte, integral and `uinteger_t` forms alike.

A character that is not a digit is invalid wherever it appears, a NUL byte and an
extended character included (an extended character is valid only as a check
character). The characters an alphabet translates to nothing (the `-` of
Crockford base32, the line breaks of RFC 4648) are skipped, except in the last
positions of a check or checksum alphabet, where those characters are read. An
alphabet with padding ends the digits at the first padding character; only padding
and skipped characters may follow it, and any other character, a digit included,
throws, as `is_valid` rejects it.

## is_valid

```cpp
bool is_valid(const char* encoded, std::size_t size) const;
bool is_valid(std::string_view encoded) const;
template <typename T, std::size_t N> bool is_valid(T (&&s)[N]) const;
```

`is_valid` reports whether a string is well formed for the alphabet. It never
throws. A string is well formed when it holds:

1. digits, and the characters the alphabet ignores, in any mix;
2. then the padding character, if the alphabet has one (`Base64::rfc4648()` text
   such as `YWI=`), repeated; only ignored characters may follow it;
3. then the check character of a `with_check` alphabet, and then the checksum
   character of a `with_checksum` one, each a digit or one of the alphabet's
   extended characters (`*~$=U` for `Base32::crockfordchk()`), and the checksum
   holds.

A string shorter than its check and checksum characters is not valid, an empty
one included. `is_valid` does not recompute the value of a check character, so
`decode` can still throw `Invalid check` on a `Base32::crockfordchk()` string that
`is_valid` accepts. For every other alphabet `is_valid` and `decode` agree: a string
is valid exactly when `decode` accepts it.

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
|   58 | `Base58::bitcoin()`    | `123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz` (`leading_zeros`)
|   58 | `Base58::ripple()`     | `rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz` (`leading_zeros`)
|   58 | `Base58::flickr()`     | `123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ`
|   59 | `Base59::base59()`     | `23456789abcdef...XYZ` (ambiguous chars translated)
|   62 | `Base62::base62()`     | `0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`
|   62 | `Base62::inverted()`   | `0123456789abcdef...XYZ` (lowercase first)
|   64 | `Base64::base64()`     | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/`
|   64 | `Base64::url()`        | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_`
|   64 | `Base64::rfc4648()`    | base64 with `=` block padding
|   66 | `Base66::base66()`     | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~`

There are also `base11`, `base16chk`, `Base59::dubaluchk()`,
`Base64::rfc4648url()` / `rfc4648url_unpadded()`, and check twins for most of the
above. See the factory structs at the bottom of `base_x.hh` for the full list.

## Notes & caveats

- The default alphabets are not byte-aligned standards. `Base16::base16()` and
  `Base64::base64()` are not the same as stock hex or base64; they encode the
  whole input as one integer. Use the `rfc4648*` presets when you need standard,
  padded output; they are byte-exact against Python's `base64` module.
- A leading zero byte becomes a leading alphabet character only in the `leading_zeros`
  presets, `Base58::bitcoin()` and `Base58::ripple()` (Bitcoin's convention, which the
  XRP Ledger shares): `decode(encode(x))` returns `x` for any bytes. The other alphabets
  encode the numeric value, not a fixed-width byte string, so the bytes `00 00 61 62 63` and
  `61 62 63` have the same text and decode returns `61 62 63`; the `rfc4648*` presets keep every byte. Neither
  `bitcoinchk()` nor `ripplechk()` has the flag, and neither is Base58Check (the checksum is
  base-x's one character, not four bytes of a double SHA-256).
- `decode` and the checked encoders throw `std::invalid_argument`. `is_valid` checks
  the alphabet, padding and checksum without throwing, but not a `with_check`
  character's value (`Base32::crockfordchk()`), so `decode` can still throw on text
  `is_valid` accepts there; catch the exception if bad input must not escape.
- `ignore_case` only affects decoding; encoding always emits the alphabet's own
  casing.
- On AArch64, alphabets of up to 64 characters convert 16 digits or characters at a
  time with table lookups. Define `BASEX_NO_SIMD` to build the scalar code instead; the
  results are identical.
- A string-like `Result` passed to `decode(Result&, ...)` is replaced by the decoded bytes
  (an empty one is filled directly), and is left as it was when the text is bad.
