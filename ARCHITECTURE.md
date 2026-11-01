# Architecture

`base-x` is two headers. `base_x.hh` holds the `BaseX` codec and the alphabet
factory structs; `uinteger_t.hh` holds the arbitrary-precision unsigned integer
the codec runs on. This document covers the encoding scheme, how encode and decode
use the integer's digit API, the optional check/checksum/padding features, and the
complexity.

## The core idea

Encoding treats the whole input as one big unsigned integer and rewrites it in
the target base. `encode` converts its argument to a `uinteger_t` (base 256 over
the input bytes) and then asks it for its digits in the alphabet's base,
mapping each digit to a character via the alphabet table. Decoding runs the
reverse: it maps each character to its digit, builds a `uinteger_t` from the digits, then renders
that integer back to base-256 bytes (or to an integral type if asked).

Because the value is a single integer, the scheme is base-agnostic. The same
code path serves base2 through base66; only the alphabet and a few derived
constants change. The cost is that the output is the numeric value, not a
byte-aligned block encoding, so these alphabets are not drop-in replacements for
standard hex or base64. The `rfc4648*` presets are: they encode the bytes (see
Padding, check, and checksum), leading zero bytes included. `Base58::bitcoin()` and `ripple()` encode
the number too, then restore the leading zero bytes as leading zero digits (`leading_zeros`, below).

## Alphabet tables

A `BaseX` carries lookup tables built at construction: `_chr` maps a digit (0..base-1) to its character, and `_ord`
maps a byte back to its digit, with out-of-alphabet bytes set to `size`, which no
character has (the extended characters take `alphabet_base` up to `size - 1`). `_ord8` repeats `_ord` for the 128 ASCII
bytes as bytes, the digit or 0xff for any other, which is the table the 16-at-a-time scan reads. The
constructor is `constexpr`, so every factory encoder is built at compile time.

The constructor takes four strings: the `alphabet`, an `extended` set (extra
characters appended after the alphabet, used by the check feature), a
`padding_string`, and a `translate` set. Translation lets several characters
decode to the same digit. Crockford base32, for example, maps `0`, `O` and `o`
to the same value and ignores `-`. When `ignore_case` is set, the constructor
also wires the opposite-case letter to the same digit.

A handful of constants are derived once at construction:
`alphabet_base` is the base, `size` is the alphabet plus the extended set, and
`block_size` is set only when `block_padding` is requested.

## Encoding through the digit API

`encode` (through `encode_value`) asks the value for its digits with
`uinteger_t::digits(result, alphabet_base)`, which appends them as byte values
`0..base-1`, most significant first, then maps each through `_chr` in place. A
`with_checksum` alphabet adds up the digits in the same pass. For the others, an
alphabet of up to 64 characters on AArch64 maps 16 digits at a time with table lookups
(`vqtbl4q_u8` over the first 64 entries of `_chr`; the last block overlaps the one before it, so
nothing outside the digits is read or written), and every other build maps one digit at
a time. `BASEX_NO_SIMD` selects the one-at-a-time code everywhere. The base-conversion work lives in uinteger_t, so
base-x gets every speedup there:

- Power-of-two bases (base16/base32/base64) are sliced out of the limbs with shifts
  and a mask, no division. This is what makes them cheap.
- Other bases (base58, base62, base59, ...) are written as chunks of the largest
  power of the base that fits a limb, cut from a fixed-point fraction by
  multiplications (no divisions), then split into digits with fixed-point
  multiplies. This is the classic base conversion and the more expensive of the two;
  uinteger_t describes it.

The kernel writes the digit bytes straight into `result` when it is a one-byte
container, and the digits are never copied: `encode` maps them in place. The
checksum sum is only accumulated for `with_checksum` alphabets.

## Padding, check, and checksum

Four optional features hang off the flags, applied after the core digits are
produced:

- `block_padding` (RFC 4648 style). When set, `block_size` is nonzero and the encoder works
  from the input's byte count `n` (a buffer's length; a `uinteger_t`'s shortest byte string, one byte
  for zero). The text has `want = ceil(8 n / block_size)` digits: the value is shifted left by
  `want block_size - 8 n` bits, so its bits end on a digit boundary, and the digits it lacks at the
  front are zeros inserted ahead of them. That one insert gives the leading zero digit of a small
  first byte and the leading zero digits of leading zero bytes, so the text is what Python's
  `base64` module writes whatever the limb alignment. No bytes are no text. The padding character is
  then appended so the text length is a multiple of `padding_size`. Decoding takes the byte count back
  from the digit count (`count block_size` bits, 8 to a byte), undoes the shift with
  `result >>= (bits & 7)`, stops the digits at the first padding character, and writes exactly that many
  bytes, zero bytes in front included. Digits past the last whole byte are dropped. This is the
  path the `rfc4648*` presets use to match the standard.
- `leading_zeros` (Bitcoin style, never with `block_padding`: the constructor asserts). The number
  is the input without its leading zero bytes, so it carries no length. `encode` counts those bytes,
  `z`, with a scan that stops at the first nonzero one (a cost of `z` byte tests, and the number is built
  from the bytes after them), converts the rest as usual, and inserts `z` zero digits ahead of its digits
  before the characters are mapped. The padding, the checksum and the check character therefore see them
  like any digit: they count in the length, and add nothing to the sum or the value. Input that is all
  zeros is `z` zero digits, and no bytes are no text (the numeric alphabets write one zero digit for a
  zero number and throw for no bytes). A `uinteger_t` argument is its shortest byte string, so it has
  no leading zero bytes: zero is one zero digit and any other number its digits alone. `decode` counts
  the leading zero digits among the collected digits (ignored characters are not digits, and a
  translated look-alike of the zero digit is one), converts the digits after them, and writes that many
  zero bytes ahead of the bytes of the number, none for zero. The `uinteger_t` and integral forms return
  the number alone. `is_valid` is unchanged: a zero digit is a digit. `Base58::bitcoin()` and
  `ripple()` carry the flag (the XRP Ledger writes its addresses the same way, with its own alphabet).
- `with_check` appends a single check character: the value mod the full table
  size, drawn from the alphabet plus the extended set. Decoding
  recomputes it and throws if it disagrees.
- `with_checksum` appends a checksum character computed from the digit sum and
  the length. Decoding folds the same sum back in and throws if
  the total is not zero mod the size. `is_valid` performs the same check without
  decoding.

The checksum sum is accumulated while the digits are mapped (encode) and in one pass over the
collected digits (decode), only for `with_checksum` alphabets; the check character is the value
mod `size` and needs no sum. The other alphabets pay for neither.
The padding and the checksum length count only the text this `encode` call
appended, so encoding into a non-empty `result` leaves its contents alone.

`is_valid` walks the text with the same framing as decode, without converting it:
digits and ignored characters, then padding (nothing but padding and ignored characters after
it), then the check and checksum characters, each below `size`. It verifies the
checksum but does not recompute a check character's value; that needs the integer.

## Decoding

`decode(uinteger_t&, const char*, size_t)` mirrors encoding. It
first subtracts the trailing check/checksum characters from the length, then
walks the remaining characters once, collecting each digit's value into a buffer
(256 bytes on the stack, a heap array for longer input). Where the alphabet has up to 64
characters, text of 16 or more characters first goes through `to_digits`, which turns 16 characters at a time into digits
with two chained `vqtbx4q_u8` lookups into `_ord8` (0xff for any byte that is not a digit); the last
block overlaps the one before it. The first block that holds a byte that is not a digit stops it, and that
block and the text after it go to the loop below. One comparison per character
finds the rare ones: a character whose ordinal is not below `alphabet_base` leaves
the loop, and then characters whose ordinal is negative are ignored (the
translate-to-negative trick), the padding character (only in an alphabet that has
one) ends the digits (a loop then reads the rest of the text, which may hold only padding and
ignored characters, before anything is converted), and any other, a NUL byte included, raises
`std::invalid_argument`. The digit sum is added up afterwards, and only for checksum
alphabets. The collected digits go to `uinteger_t::from_digits`, which
packs bits directly for power-of-two bases (linear) and builds a chunk of digits with
SWAR multiplies, then folds four chunks per step, for the others. After that it reverses any block-padding shift and
verifies the check and checksum if present.

The string-returning overloads turn the integer back into bytes with
`uinteger_t::digits(result, 256)`, which writes the limbs big-endian into the result; a
non-empty `result` is replaced, an empty one receives the bytes directly. In a block-padded
alphabet the byte count is the digit count's (`count block_size / 8`), and the bytes are
zero-padded in front to it, so leading zero bytes come back (a zero value is that many zero bytes). In a
`leading_zeros` alphabet the digits after the leading zero digits go to `from_digits` (a text of zero
digits is the value zero), and the result is a zero byte for each of them followed by the number's bytes.
The integral overloads return the integer directly. Undoing the block-padding shift is skipped when the alphabet has none.

## Complexity

Let `n` be the input size in bytes and `m` the output length.

- Power-of-two encode/decode is linear: digits are sliced out of, or packed into,
  the limbs with shifts and masks, no division.
- Non-power-of-two encode and decode are quadratic in the input size up to 8192 bits
  (1 KiB): about one limb product per limb of input for each limb-sized chunk of digits.
  Above that, uinteger_t converts through a tree of divisions and products, O(M(n) log n)
  with M its multiplication. The work sits in uinteger_t.
- `encode` appends to `result`; the digits are written in place and mapped in place.

## Design decisions

- One integer, one code path per base family. Treating the input as a
  `uinteger_t` keeps the codec tiny and base-agnostic at the cost of not being
  byte-aligned.
- Compile-time alphabets. The `constexpr` constructor builds the lookup tables
  and derived constants at compile time, so each factory encoder is a constant
  with no runtime setup.
- Conversion lives in uinteger_t. base-x only maps digits through its alphabet, so
  the codec gets the power-of-two fast path and the subquadratic conversion tree with
  no code of its own.
- The scalar loop is the one place that classifies a byte that is not a digit (invalid,
  ignored or padding), so the table-lookup scan reports what the scalar code reports: it only converts the
  leading run of digits and hands the rest over.
- Translation and case-folding live in the `_ord` table, so decoding ambiguous
  or mixed-case input is a plain table lookup with no special-casing in the hot
  loop.

## Limitations

- Not byte-aligned. The default alphabets do not match stock hex/base64; only the
  `rfc4648*` presets do, via the block-padding path.
- The whole value is materialized as a `uinteger_t`; there is no streaming over
  input that does not fit in memory.
- `Base58::bitcoin()` and `ripple()` keep leading zero bytes as leading zero digits (`leading_zeros`); every
  other alphabet but the `rfc4648*` presets encodes them as part of the integer value, not as fixed
  leading characters, so its output is not interchangeable with byte-preserving base58 variants.
  `bitcoinchk()` and `ripplechk()` are among them. Base58Check's double SHA-256 checksum is not here.
- `decode` throws on bad input. `is_valid` screens alphabet, padding and checksum
  without throwing but does not recompute a check character (`crockfordchk`), so it
  does not rule out every `decode` exception.
