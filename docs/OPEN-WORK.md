# Open work

What base-x does not do yet, behavior that is open to change, where it is slow, and what its tests
do not reach. Each item is a fact about the code as it is, with the measurement behind it where there
is one. Conversion work belongs in uinteger_t, whose
[docs/OPEN-WORK.md](https://github.com/Kronuz/uinteger_t/blob/master/docs/OPEN-WORK.md) lists its own.

## Behavior

- **A failed check leaves the `uinteger_t` written.** `decode(uinteger_t&, ...)` assigns the value
  before it verifies the check and checksum characters, so on "Invalid check", "Invalid checksum" or a
  bad character in those positions its argument holds the decoded value. The comment on
  `decode_value` says it leaves the result as it was; the forms that return bytes or an integral do.
- **Padding with a checksum disagrees with itself.** In an alphabet built with both `block_padding` and
  `with_checksum`, `encode` counts the padding characters in the checksum's length and `decode` and
  `is_valid` do not, so the text fails its own decode whenever padding is written (one byte `a` in a
  base64 alphabet with `====` padding and a checksum encodes as `YQ==U`, which decodes to "Invalid
  checksum"). No preset combines them, and the constructor does not assert against it.
- **`encode` into a `std::vector` does not compile.** The padding step calls `result.append(p)`, which a
  vector lacks, in a run-time branch that every `Result` compiles; `is_result` admits vectors. `decode`
  into a vector works.
- **The flag conflict is only an assert.** Under `NDEBUG` an alphabet built with both `leading_zeros`
  and `block_padding` encodes the bytes `00 00 12 34` as `1234` and decodes that to two bytes. No preset
  combines them.
- **`decode<Integral>` truncates.** It returns the low limb of the value, with no overflow error: a
  9-byte value decoded into `unsigned long long` loses its top byte, and at 32-bit limbs every type
  gets 32 bits.
- **Block-padded decoding is lenient.** The `rfc4648*` presets drop digits past the last whole byte,
  accept any amount of padding or none (`YQ`, `YQ=`, `YQ======` all decode to `a`), accept text of
  padding alone (`====` is no bytes), and shift out nonzero trailing bits (`YR==` decodes to `a`).
  Python's `base64` rejects all of these, as it rejects an odd count of hex digits, where
  `Base16::rfc4648()` drops the last one (`ABC` decodes to the byte `AB`). Rejecting each is a one-line
  check.
- **Error messages copy the raw byte.** "Invalid character" quotes the byte as it is, so a NUL cuts
  `what()` short and a control byte reaches any log that records the message; printing it in hex
  would keep the message clean.
- **A string literal goes through `std::string_view`.** A literal is an lvalue, so `encode("\0\0abc")`
  takes the `std::string_view` overload, which stops at the first NUL (it encodes no bytes). The
  `T (&&)[N]` overloads take only rvalue arrays. Pass binary data with its size.
- **Allocation failures.** `encode` into a non-empty result gives the basic guarantee: a failure in the
  zero-digit insert leaves digit values (not characters) after the caller's text, and one in the
  padding, check or checksum step leaves the text without them. The block-padded and `leading_zeros`
  paths of `decode(Result&, ...)` clear the result before they allocate, so a failure after good text
  can leave it empty or partial.

## Performance

- **The table paths are AArch64-only.** x86-64 maps digits one at a time: 11 ns of a 142.5 ns 32-byte
  base58 encode on an Intel Core i9-9980HK. An SSSE3 `pshufb` map, behind a target attribute and a
  run-time CPU check, would take about 3 ns (ARCHITECTURE.md breaks down the rest).
- **Power-of-two decoding** costs 3.0 ns per byte for a 1 KiB `Base64::base64()` decode on an Apple M1
  Pro, against 1.1 to encode: uinteger_t's `from_digits` packs one digit at a time there.
- **Two passes over the digits.** `encode` maps each digit after `digits()` writes it, and `decode`
  collects digits into a buffer (256 bytes on the stack) that `from_digits` then checks again (about 3 ns
  of a 32-byte decode on an Apple M4 Pro). An alphabet-mapped digit API in uinteger_t would remove the
  4.5 ns map of a 32-byte encode and the 2.6 ns scan of a decode there, and the 11 ns scalar map of a
  142.5 ns encode on an Intel Core i9-9980HK.
- **x86 conversion.** Not tried: `mulx`/`adcx` (needs BMI2 and ADX dispatch, and the whole product is
  17.7 ns of the 142.5), `_strip_zeros` without its `memmove` (4.9 ns), and building decode chunks with a
  fixed digit count (13.4 against 27 ns for the chunks of a 32-byte decode on the Intel, about 12 ns of
  its 190).
- **The 25-byte decode cell** of `bench/bench.cc` reads 50 or 80 ns on an Apple M4 Pro for the same
  code, depending on the heap's layout; compare it across runs, not within one.

## Tests

- **Release CTest checks nothing in `test/test.cc`.** CMake's Release flags define `NDEBUG`, which
  compiles out its asserts, so `base_x` and `base_x_scalar` pass there whatever the code does;
  `base_x_golden` and `base_x_fuzz_decode` check with their own reports in every build type.
- `test/test.cc` pins 13 of Bitcoin Core's 21 base58 vectors. The missing eight: 80 zero bytes (`1`
  forty times), the same followed by `01`, a run of 100 or more zero bytes followed by
  `ec39d04c37e71e5d591881f6`, the 256 bytes `00 01 ... ff`, and `271F359E`, `271F359F`, `271F35A0` and
  `271F35A1`. All of them pass on both `leading_zeros` presets; the long zero runs also cross the
  256-digit stack buffer.
- The fuzz target puts 0 to 7 zero digits in front of its text, so mutants that cap the zero count at
  255 or keep it in a byte survive it; only `test/test.cc`'s runs of 255 to 1000 kill them.
- The fuzz target passes `std::string::data()`, whose NUL terminator makes a one-byte over-read an
  in-bounds read for ASan; copying the text into an exact-size `new char[n]` would expose it.
- No test alphabet combines `with_check` and `with_checksum`. No test proves that the constructor's
  asserts fire (the flag pair, a padding character that is a digit). An unknown byte at the checksum
  position is caught only by the broad agreement loop of `test/test.cc`: it adds `size`, which is 0
  modulo `size`, so a check of the range is what rejects it.
- The checksum sums are 64-bit, and no test reaches a sum past 2^31 (where a 32-bit sum would wrap):
  that takes about 40 million characters.
- No test runs on a big-endian host, a 32-bit target or MSVC.
