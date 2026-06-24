# Architecture

`base-x` is two headers. `base_x.hh` holds the `BaseX` codec and the alphabet
factory structs; `uinteger_t.hh` holds the arbitrary-precision unsigned integer
the codec runs on. This document covers the encoding scheme, the two encoding
paths, the optional check/checksum/padding features, and the complexity.

## The core idea

Encoding treats the whole input as one big unsigned integer and rewrites it in
the target base. `encode` converts its argument to a `uinteger_t` (base 256 over
the input bytes) and then repeatedly extracts digits in the alphabet's base,
mapping each digit to a character via the alphabet table. Decoding runs the
reverse: it folds each character's ordinal back into a `uinteger_t`, then renders
that integer back to base-256 bytes (or to an integral type if asked).

Because the value is a single integer, the scheme is base-agnostic. The same
code path serves base2 through base66; only the alphabet and a few derived
constants change. The cost is that the output is the numeric value, not a
byte-aligned block encoding, so these alphabets are not drop-in replacements for
standard hex or base64.

## Alphabet tables

A `BaseX` carries two 256-entry lookup tables built at construction
(`base_x.hh:38`): `_chr` maps a digit (0..base-1) to its character, and `_ord`
maps a byte back to its digit, with out-of-alphabet bytes set to `alphabet_base`
(an out-of-range sentinel). The constructor is `constexpr`, so every factory
encoder is built at compile time.

The constructor takes four strings: the `alphabet`, an `extended` set (extra
characters appended after the alphabet, used by the check feature), a
`padding_string`, and a `translate` set. Translation lets several characters
decode to the same digit. Crockford base32, for example, maps `0`, `O` and `o`
to the same value and ignores `-`. When `ignore_case` is set, the constructor
also wires the opposite-case letter to the same digit.

A handful of constants are derived once at construction (`base_x.hh:44`):
`alphabet_base` is the base, `alphabet_base_bits` is nonzero only when the base
is a power of two, `alphabet_base_mask` is `alphabet_base - 1`, and `block_size`
is set only when `block_padding` is requested.

## Two encoding paths

`encode(Result&, const uinteger_t&)` (`base_x.hh:134`) branches on whether the
base is a power of two.

- Power-of-two bases (`alphabet_base_bits != 0`) take the fast path
  (`base_x.hh:149`). The value's words are walked and sliced into
  `alphabet_base_bits`-wide chunks with shifts and a mask
  (`(v >> shift) & alphabet_base_mask`), no division at all. This is what makes
  base16/base32/base64 cheap. Leftover high zero digits are trimmed afterward
  with a `find_if` from the back.
- Non-power-of-two bases (base58, base62, base59, ...) take the long-division
  path (`base_x.hh:178`). It repeatedly calls `quotient.divmod(base)` and pushes
  the remainder digit until the quotient is zero. This is the classic base
  conversion and the more expensive of the two.

Both paths push digits least-significant first, then `std::reverse` the result
(`base_x.hh:188`). The output buffer is reserved up front to `num_sz * base_size`
(`base_x.hh:148`) so the pushes don't reallocate.

## Padding, check, and checksum

Three optional features hang off the flags, applied after the core digits are
produced:

- `block_padding` (RFC 4648 style). When set, `block_size` is nonzero and the
  encoder left-shifts the value so its bit length lands on a block boundary
  (`base_x.hh:138`), then appends the padding character so the text length is a
  multiple of `padding_size` (`base_x.hh:189`). Decoding undoes the shift with
  `result >>= (bp & 7)` and stops at the first padding character. This is the
  path the `rfc4648*` presets use to match the standard.
- `with_check` appends a single check character: the value mod the full table
  size, drawn from the alphabet plus the extended set (`base_x.hh:194`). Decoding
  recomputes it and throws if it disagrees.
- `with_checksum` appends a checksum character computed from the digit sum and
  the length (`base_x.hh:199`). Decoding folds the same sum back in and throws if
  the total is not zero mod the size. `is_valid` performs the same check without
  decoding.

The check and checksum sums are accumulated during the main loop (`sum`,
`sumsz`) so the feature costs little beyond the trailing character.

## Decoding

`decode(uinteger_t&, const char*, size_t)` (`base_x.hh:265`) mirrors encoding. It
first subtracts the trailing check/checksum characters from the length, then
walks the remaining characters. Power-of-two bases fold each digit in with
`(result << alphabet_base_bits) | d`; other bases use `result * base + d`.
Characters whose ordinal is negative are ignored (the translate-to-negative
trick), and characters at or above `alphabet_base` raise
`std::invalid_argument`. After the loop it reverses any block-padding shift and
verifies the check and checksum if present.

The string-returning overloads then call `uinteger_t::str(256)` to turn the
integer back into bytes; the integral overloads return the integer directly.

## Complexity

Let `n` be the input size in bytes and `m` the output length.

- Power-of-two encode/decode is linear: each word is sliced into fixed-width
  chunks with shifts and masks, no division.
- Non-power-of-two encode is dominated by repeated `divmod`, which is the
  super-linear part. Each `divmod` is linear in the current quotient size, and
  there are `O(m)` of them, so the conversion is roughly `O(n * m)` in the worst
  case. Decode is the symmetric multiply-accumulate.
- The output buffer is reserved once, so encoding does not reallocate during the
  digit pushes; the trailing `std::reverse` is linear.

## Design decisions

- One integer, one code path per base family. Treating the input as a
  `uinteger_t` keeps the codec tiny and base-agnostic at the cost of not being
  byte-aligned.
- Compile-time alphabets. The `constexpr` constructor builds the lookup tables
  and derived constants at compile time, so each factory encoder is a constant
  with no runtime setup.
- Power-of-two fast path. Detecting `alphabet_base_bits` and switching to
  shift/mask avoids `divmod` for the bases where it isn't needed.
- Translation and case-folding live in the `_ord` table, so decoding ambiguous
  or mixed-case input is a plain table lookup with no special-casing in the hot
  loop.

## Limitations

- Not byte-aligned. The default alphabets do not match stock hex/base64; only the
  `rfc4648*` presets do, via the block-padding path.
- The whole value is materialized as a `uinteger_t`; there is no streaming over
  input that does not fit in memory.
- Leading zero bytes encode as part of the integer value, not as fixed leading
  characters, so the output is not interchangeable with byte-preserving base58
  variants.
- `decode` throws on bad input; callers that must avoid exceptions should screen
  with `is_valid` first.
