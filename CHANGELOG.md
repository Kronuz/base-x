# Changelog

What changed in base-x, newest first. There are no release tags: each section is dated
and links the commits on `master`.

## 2026-10-31

A performance and correctness pass. Timings are at -O2 on Apple Silicon.

### Added

- `bench/bench.cc`: encode and decode timings for the base58, base62, base64 and base16
  presets at 16 to 1024 bytes, and base58 against libbase58 and Firedancer ([979180c]).

### Changed

- Bytes that are all zero encode as the value zero does in the presets that write the numeric value:
  one zero digit, and no check character after it. `Base16::base16()` of one zero byte is `0` and
  `Base64::base64()`'s is `A` (both were the empty text), and `Base59::dubaluchk()`'s is `z` (it was
  `zN`); uinteger_t parsed the byte to an untrimmed zero limb. The old and the new texts decode to
  one zero byte.
- `Base58::bitcoin()` and `Base58::ripple()` write each leading zero byte as one leading zero digit (`1` in
  Bitcoin's alphabet, `r` in Ripple's) and read each leading zero digit back as a zero byte, as Bitcoin's
  base58 and the XRP Ledger do, through a new `BaseX::leading_zeros` flag. The text of an input with leading
  zero bytes changes: the bytes `00 00 61 62 63` are `11ZiCa` (they were `ZiCa`) and two zero bytes are `11`
  (they were `1`). The decode of a text that starts with the zero digit changes: `11ZiCa` is `00 00 61 62 63`
  (it was `61 62 63`) and `11` is two zero bytes (it was one); `decode(encode(x))` is `x` for any bytes.
  An empty buffer encodes as the empty text (it threw), and the empty text decodes to no bytes (it gave
  one zero byte). Input without a leading zero byte encodes as before, and a `uinteger_t` argument (zero
  is `1`, `r`) and the integral and `uinteger_t` forms of `decode`, which return the number, do not
  change. No other preset changes, `bitcoinchk()` and `ripplechk()` included; they keep the flag off
  ([27be05e]).
- The RFC 4648 presets (`Base16::rfc4648()`, `Base32::rfc4648()`, `rfc4648hex()`, `Base64::rfc4648()`,
  `rfc4648url()`, `rfc4648url_unpadded()`) are byte-exact against Python's `base64` module: a leading
  zero byte is a leading zero digit and decodes back to a zero byte. The text used to keep 0 or 8 of them,
  by limb alignment: six zero bytes then `0x01` gave `AQ==`, where Python gives `AAAAAAAAAQ==`. An empty
  buffer encodes as the empty text (it threw), and the empty text decodes to no bytes (it gave one zero
  byte). A `uinteger_t` argument encodes as its shortest byte string, one byte for zero: `0` is `AA==`
  (it was `A`) ([2507a82]).
- `Base16::rfc4648()` writes two digits per byte (`0x01` is `01`, it was `1`) and decodes by bytes: an odd
  count of digits drops the last one (`ABC` is `0xAB`; it was the number `0x0ABC`). The dangling bits of
  base64 and base32 text were dropped already ([2507a82]).
- `decode` rejects text after the padding, as `is_valid` does: only padding and ignored characters may follow
  it, and a digit or any other character throws `std::invalid_argument` (`YWI=junk` decoded as `ab`). The
  result is left as it was. `is_valid` and `decode` now agree on every text, but for the value of a
  `Base32::crockfordchk()` check character, which `is_valid` does not recompute ([d3d9f23]).
- With a check or checksum character after padding, `decode` reads it after all the padding and ignored
  characters before it; it read the first of them ([d3d9f23]).
- `decode` counts the bits of digits in `std::size_t`; the count was a 32-bit `unsigned`
  ([2507a82]).
- An RFC 4648 `encode` throws `std::length_error` when 8 times the byte count does not fit a
  `std::size_t` (512 MiB or more on a 32-bit one); the digit count would wrap ([b468653]).
- `encode` and `decode` run on uinteger_t's `digits()` and `from_digits()`, so a hand-copied
  `uinteger_t.hh` must have them. CMake pins uinteger_t 57ea9d5 ([1687e9a], [a3c0e0d]).
- CMake pins uinteger_t [a383e37](https://github.com/Kronuz/uinteger_t/commit/a383e37), with its
  quotient-only division and eight-limb addition.
- `is_valid` accepts `=` padding (`YWI=`) and the extended check characters `*~$=U` of
  `Base32::crockfordchk()`, as `decode` does, and rejects a digit after padding ([f702d6d]).
- `is_valid` answers differently for text that ends in an ignored character in six presets,
  `crockfordchk` and `dubaluchk` among them ([f702d6d], [ecbfb44]).
- `is_valid` does not recompute a `with_check` character, so `decode` can still throw on
  `Base32::crockfordchk()` text it accepts; the docs no longer say otherwise ([ecbfb44]).

### Fixed

- `decode` into a `uinteger_t` leaves it as it was when the text fails its check or checksum
  character; it held the decoded value.
- The RFC 4648 base32 and base64 presets misaligned every input of 32 bytes or more: 148 of
  240 random inputs differed from Python's `base64` module. All 240 match now ([c3965ec]).
- The RFC 4648 presets dropped a zero leading digit at any size, so a small first byte lost
  it (0x01 must encode as `AQ==`) ([c3965ec]).
- `encode` into a non-empty result measured padding and checksum over the whole buffer: after
  `xy`, `Base64::rfc4648()` wrote `xyYQ` for `a` instead of `xyYQ==` ([d1fda10]).
- A NUL byte ended `decode` without an error in alphabets without padding (`ab\0cd` decoded
  as `ab`). It throws `std::invalid_argument` ([36dd216]).
- `Base32::crockfordchk()` read an unknown byte in the check position as `*`, so `10?` and
  `10\0` decoded as 32. They throw ([712c643]).
- Checksum sums were `int` and overflowed at about 34 million base64 characters. They are
  64-bit ([10a5f0b]).
- `decode` of an empty string in a `*chk` alphabet asked for about 2^64 bytes and threw
  `std::bad_alloc`. It throws `std::invalid_argument` ([10a5f0b]).

### Performance

- Bases that are not powers of two convert a limb of digits per pass: base58 of a 32-byte key
  encodes about 10x and decodes about 16x faster, 2 KiB inputs 12x and 14x ([8572a00]).
- Power-of-two bases decode in linear time: base64 decode of 2 KiB, 500 to 7.6 us
  ([1687e9a], [8935fe6]).
- uinteger_t's division-free kernels and a decode loop with one comparison per character:
  32-byte base58 encode 203 to 73 ns, decode 152 to 95 ns ([2c21ec4], [2820abb]).
- 32- and 64-byte keys do not allocate (uinteger_t's inline buffer), and inputs above
  8192 bits take uinteger_t's subquadratic conversion ([a3c0e0d]).
- For scale, 32-byte base58 encode takes 73.5 ns, against 42.8 ns for Firedancer, 1220 ns
  for libbase58 and 879 ns for a classic byte-wise codec ([979180c]).

[8572a00]: https://github.com/Kronuz/base-x/commit/8572a00
[1687e9a]: https://github.com/Kronuz/base-x/commit/1687e9a
[10a5f0b]: https://github.com/Kronuz/base-x/commit/10a5f0b
[8935fe6]: https://github.com/Kronuz/base-x/commit/8935fe6
[c3965ec]: https://github.com/Kronuz/base-x/commit/c3965ec
[2c21ec4]: https://github.com/Kronuz/base-x/commit/2c21ec4
[2820abb]: https://github.com/Kronuz/base-x/commit/2820abb
[d1fda10]: https://github.com/Kronuz/base-x/commit/d1fda10
[36dd216]: https://github.com/Kronuz/base-x/commit/36dd216
[712c643]: https://github.com/Kronuz/base-x/commit/712c643
[f702d6d]: https://github.com/Kronuz/base-x/commit/f702d6d
[ecbfb44]: https://github.com/Kronuz/base-x/commit/ecbfb44
[a3c0e0d]: https://github.com/Kronuz/base-x/commit/a3c0e0d
[979180c]: https://github.com/Kronuz/base-x/commit/979180c
[2507a82]: https://github.com/Kronuz/base-x/commit/2507a82
[d3d9f23]: https://github.com/Kronuz/base-x/commit/d3d9f23
[b468653]: https://github.com/Kronuz/base-x/commit/b468653
[27be05e]: https://github.com/Kronuz/base-x/commit/27be05e
