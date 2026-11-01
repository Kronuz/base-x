/*
base_x.hh
BaseX encoder / decoder for C++

Copyright (c) 2017,2019 German Mendez Bravo (Kronuz) @ german dot mb at gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef __BASE_X__H_
#define __BASE_X__H_

#include <cassert>          // for assert
#include <algorithm>        // for std::find_if, std::reverse
#include <cstdint>          // for std::int64_t
#include <cstring>          // for std::memcpy
#include <limits>           // for std::numeric_limits
#include <memory>           // for std::unique_ptr
#include <stdexcept>        // for std::invalid_argument, std::length_error
#include <string>           // for std::string
#include <string_view>      // for std::string_view
#include <type_traits>      // for std::enable_if_t

#include "uinteger_t.hh"

// A test defines BASEX_PUBLIC_IMPLEMENTATION to reach the private members of the class.
// BASEX_PRIVATE stands for `private`.
#ifdef BASEX_PUBLIC_IMPLEMENTATION
#define BASEX_PRIVATE public
#else
#define BASEX_PRIVATE private
#endif

// Alphabets of up to 64 characters map digits to characters and back 16 at a time with
// table lookups (NEON here); BASEX_NO_SIMD selects the scalar code everywhere.
#if !defined BASEX_NO_SIMD && defined __aarch64__ && defined __ARM_NEON
#include <arm_neon.h>
#define BASEX_NEON 1
#endif


class BaseX {
	char _chr[256];
	int _ord[256];
	unsigned char _ord8[128];  // _ord for the ASCII bytes: the digit, or 0xff for any other (the table lookups)

	const int size;
	const int alphabet_base;
	const unsigned block_size;
	const unsigned padding_size;
	const char padding;
	const int flags;

	constexpr char chr(unsigned char ord) const {
		return _chr[ord];
	}

	constexpr int ord(unsigned char chr) const {
		return _ord[chr];
	}

	// Replaces the n digits at p (each below alphabet_base) with their characters.
	template <typename C>
	void to_chars(C* p, std::size_t n, std::false_type) const {
		for (std::size_t i = 0; i < n; ++i) {
			p[i] = static_cast<C>(chr(static_cast<unsigned char>(p[i])));
		}
	}

	template <typename C>
	void to_chars(C* ptr, std::size_t n, std::true_type) const {
		unsigned char* p = reinterpret_cast<unsigned char*>(ptr);
#ifdef BASEX_NEON
		if (alphabet_base <= 64 && n >= 16) {
			uint8x16x4_t table;
			for (int i = 0; i < 4; ++i) {
				table.val[i] = vld1q_u8(reinterpret_cast<const std::uint8_t*>(_chr) + 16 * i);
			}
			// The last 16 digits go first: they overlap the blocks when n is not a multiple of 16.
			const uint8x16_t last = vqtbl4q_u8(table, vld1q_u8(p + n - 16));
			for (std::size_t i = 0; i <= n - 16; i += 16) {
				vst1q_u8(p + i, vqtbl4q_u8(table, vld1q_u8(p + i)));
			}
			vst1q_u8(p + n - 16, last);
			return;
		}
#endif
		for (std::size_t i = 0; i < n; ++i) {
			p[i] = static_cast<unsigned char>(chr(p[i]));
		}
	}

	// Converts the characters s[0, n) to digits at d for as long as they are digits, 16 at a time,
	// and returns how many it converted; the first character that is not a digit, and the text after
	// it, are left to the caller. Reads only inside s[0, n) and writes only d[0, n).
	std::size_t to_digits(const char* s, std::size_t n, unsigned char* d) const {
#ifdef BASEX_NEON
		if (alphabet_base <= 64 && n >= 16) {
			uint8x16x4_t lo, hi;
			for (int i = 0; i < 4; ++i) {
				lo.val[i] = vld1q_u8(_ord8 + 16 * i);
				hi.val[i] = vld1q_u8(_ord8 + 64 + 16 * i);
			}
			const uint8x16_t invalid = vdupq_n_u8(0xff);
			const uint8x16_t half = vdupq_n_u8(64);
			const unsigned char below = static_cast<unsigned char>(alphabet_base);
			// An index past a table's 64 entries leaves the lane as it was: 0xff, then.
			auto lookup = [&](uint8x16_t v) {
				return vqtbx4q_u8(vqtbx4q_u8(invalid, lo, v), hi, vsubq_u8(v, half));
			};
			std::size_t i = 0;
			for (; i <= n - 16; i += 16) {
				const uint8x16_t r = lookup(vld1q_u8(reinterpret_cast<const std::uint8_t*>(s) + i));
				if (vmaxvq_u8(r) >= below) {
					return i;
				}
				vst1q_u8(d + i, r);
			}
			if (i < n) {
				// The last 16 characters overlap the ones just converted.
				const uint8x16_t r = lookup(vld1q_u8(reinterpret_cast<const std::uint8_t*>(s) + n - 16));
				if (vmaxvq_u8(r) < below) {
					vst1q_u8(d + n - 16, r);
					return n;
				}
			}
			return i;
		}
#else
		(void)s; (void)n; (void)d;
#endif
		return 0;
	}

public:
	static constexpr int ignore_case =   (1 << 0);
	static constexpr int with_checksum = (1 << 1);
	static constexpr int with_check =    (1 << 2);
	static constexpr int block_padding = (1 << 3);
	static constexpr int leading_zeros = (1 << 4);  // each leading zero byte is a leading zero digit, as in Bitcoin's base58

	template <std::size_t alphabet_size1, std::size_t extended_size1, std::size_t padding_size1, std::size_t translate_size1>
	constexpr BaseX(int flgs, const char (&alphabet)[alphabet_size1], const char (&extended)[extended_size1], const char (&padding_string)[padding_size1], const char (&translate)[translate_size1]) :
		_chr(),
		_ord(),
		_ord8(),
		size(alphabet_size1 - 1 + extended_size1 - 1),
		alphabet_base(alphabet_size1 - 1),
		block_size((flgs & BaseX::block_padding) ? uinteger_t::base_bits(alphabet_base) : 0),
		padding_size(padding_size1 - 1),
		padding(padding_size ? padding_string[0] : '\0'),
		flags(flgs)
	{
		for (int c = 0; c < 256; ++c) {
			_chr[c] = 0;
			_ord[c] = size;  // not a digit, an extended character or ignored; alphabet_base is the first extended character
		}
		for (int cp = 0; cp < alphabet_base; ++cp) {
			auto ch = alphabet[cp];
			_chr[cp] = ch;
			assert(_ord[(unsigned char)ch] == size);  // Duplicate character in the alphabet
			_ord[(unsigned char)ch] = cp;
			if (flags & BaseX::ignore_case) {
				if (ch >= 'A' && ch <='Z') {
					_ord[(unsigned char)ch - 'A' + 'a'] = cp;
				} else if (ch >= 'a' && ch <='z') {
					_ord[(unsigned char)ch - 'a' + 'A'] = cp;
				}
			}
		}
		for (std::size_t i = 0; i < extended_size1 - 1; ++i) {
			auto ch = extended[i];
			auto cp = alphabet_base + i;
			_chr[cp] = ch;
			assert(_ord[(unsigned char)ch] == size); // Duplicate character in the extended alphabet
			_ord[(unsigned char)ch] = cp;
			if (flags & BaseX::ignore_case) {
				if (ch >= 'A' && ch <='Z') {
					_ord[(unsigned char)ch - 'A' + 'a'] = cp;
				} else if (ch >= 'a' && ch <='z') {
					_ord[(unsigned char)ch - 'a' + 'A'] = cp;
				}
			}
		}
		int cp = -1;
		for (std::size_t i = 0; i < translate_size1 - 1; ++i) {
			auto ch = translate[i];
			auto ncp = _ord[(unsigned char)ch];
			if (ncp >= alphabet_base) {
				assert(_ord[(unsigned char)ch] == size); // Invalid translation character (a digit or an extended character)
				_ord[(unsigned char)ch] = cp;
				if (flags & BaseX::ignore_case) {
					if (ch >= 'A' && ch <='Z') {
						_ord[(unsigned char)ch - 'A' + 'a'] = cp;
					} else if (ch >= 'a' && ch <='z') {
						_ord[(unsigned char)ch - 'a' + 'A'] = cp;
					}
				}
			} else {
				cp = ncp;
			}
		}
		// block_padding already writes a digit for every bit of every byte, leading zero bytes included.
		assert(!((flags & BaseX::block_padding) && (flags & BaseX::leading_zeros)));
		// decode finds the padding character by the out-of-alphabet sentinel, so it must not be a digit or ignored.
		assert(!padding_size || _ord[(unsigned char)padding] == size);
		for (int c = 0; c < 128; ++c) {
			_ord8[c] = (_ord[c] >= 0 && _ord[c] < alphabet_base) ? static_cast<unsigned char>(_ord[c]) : static_cast<unsigned char>(0xff);
		}
	}

BASEX_PRIVATE:
	// The digits that nbytes bytes make at bits_per_digit bits a digit, and the zero bits that fill the
	// last one. Throws std::length_error when 8 nbytes would not fit Size (512 MiB at a 32-bit size_t):
	// the arithmetic would wrap and give the wrong text. A template so that a test can run it at a narrower Size.
	template <typename Size>
	static void block_digits(Size nbytes, unsigned bits_per_digit, Size& want, Size& bp) {
		if (nbytes > (std::numeric_limits<Size>::max() - (bits_per_digit - 1)) / 8) {
			throw std::length_error("Error: Too many bytes");
		}
		const Size bits = static_cast<Size>(nbytes * 8);
		want = static_cast<Size>((bits + bits_per_digit - 1) / bits_per_digit);
		bp = static_cast<Size>(want * bits_per_digit - bits);
	}

	// The bytes of the shortest byte string of a value of `bits` bits: one byte for zero, as digits() writes it.
	template <typename Size>
	static Size block_bytes(Size bits) {
		return bits ? static_cast<Size>(bits / 8 + (bits % 8 != 0)) : static_cast<Size>(1);
	}

	// Appends the text of `input`, the value of `nbytes` bytes (it fits in them), to result. A block-padded
	// alphabet writes a digit for every bit of those bytes, so a leading zero byte is a leading zero digit
	// and no bytes are no text; one with leading_zeros writes `zeros` zero digits ahead of the number (the
	// caller counts the leading zero bytes) and, with no zeros and no number, no text; the other alphabets
	// write the number and ignore `nbytes` and `zeros` (zero is a lone zero digit there). Throws
	// std::length_error for a block-padded alphabet when 8 nbytes does not fit a size_t.
	template <typename Result>
	void encode_value(Result& result, const uinteger_t& input, std::size_t nbytes, std::size_t zeros = 0) const {
		std::size_t want = 0;  // the digit count the byte count implies, for a block-padded alphabet
		std::size_t bp = 0;    // zero bits that fill the last digit
		uinteger_t quotient;
		if (block_size) {
			block_digits<std::size_t>(nbytes, block_size, want, bp);
			if (bp) {
				quotient = input << bp;
			}
		}
		const uinteger_t& num = bp ? quotient : input;
		if (block_size || (flags & BaseX::leading_zeros) || static_cast<bool>(num)) {
			std::int64_t sum = 0;
			const auto first = result.size();
			if (num) {
				num.digits(result, alphabet_base);
			}
			if (block_size) {
				// Block-padded encodings carry every input bit: the digits the value lacks are leading zeros.
				const auto have = result.size() - first;
				if (have < want) {
					result.insert(result.begin() + first, want - have, 0);
				}
			} else if (zeros) {
				// Only a leading_zeros alphabet writes them (zero is a lone digit in the others, below). They count
				// in the padding and the checksum like any other digit, and decode counts them back.
				result.insert(result.begin() + first, zeros, 0);
			}
			const auto count = result.size() - first;
			auto ptr = result.data() + first;
			if (flags & BaseX::with_checksum) {
				for (std::size_t i = 0; i < count; ++i) {
					const auto d = static_cast<unsigned char>(ptr[i]);
					ptr[i] = chr(d);
					sum += d;
				}
			} else {
				using C = typename Result::value_type;
				to_chars(ptr, count, std::integral_constant<bool, std::is_integral<C>::value && sizeof(C) == 1 && !std::is_same<C, bool>::value>());
			}
			if (padding_size) {
				Result p;
				p.resize((padding_size - (count % padding_size)) % padding_size, padding);
				result.append(p);
			}
			if (flags & BaseX::with_check) {
				auto chk = static_cast<int>(num % size);
				result.push_back(chr(chk));
				sum += chk;
			}
			if (flags & BaseX::with_checksum) {
				auto sz = result.size() - first;
				sz = (sz + sz / size) % size;
				sum += sz;
				sum = (size - sum % size) % size;
				result.push_back(chr(sum));
			}
		} else {
			result.push_back(chr(0));
		}
	}

public:
	// Appends the text of the number. A block-padded alphabet writes the bytes of its shortest
	// byte string, one byte for zero (as digits(result, 256) does); so does one with leading_zeros:
	// zero is one zero byte, a zero digit, and any other number has no leading zero byte.
	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	void encode(Result& result, const uinteger_t& input) const {
		encode_value(result, input, block_bytes<std::size_t>(input.bits()), static_cast<bool>(input) ? 0 : 1);
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	Result encode(const uinteger_t& num) const {
		Result result;
		encode(result, num);
		return result;
	}

	// Appends the text of the bytes. An alphabet with leading_zeros writes a zero digit for every leading
	// zero byte and the number of the rest after them (no bytes, no text); one with block_padding writes
	// the digits of every bit; the others write the number, and no bytes throw (there is no number).
	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	void encode(Result& result, const unsigned char* decoded, std::size_t decoded_size) const {
		if (flags & BaseX::leading_zeros) {
			std::size_t zeros = 0;
			while (zeros < decoded_size && !decoded[zeros]) {
				++zeros;
			}
			const std::size_t rest = decoded_size - zeros;
			encode_value(result, rest ? uinteger_t(decoded + zeros, rest, 256) : uinteger_t(), 0, zeros);
		} else if (!decoded_size && block_size) {
			encode_value(result, uinteger_t(), 0);  // no bytes, no digits
		} else {
			encode_value(result, uinteger_t(decoded, decoded_size, 256), decoded_size);
		}
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	Result encode(const unsigned char* decoded, std::size_t decoded_size) const {
		Result result;
		encode(result, decoded, decoded_size);
		return result;
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	void encode(Result& result, const char* decoded, std::size_t decoded_size) const {
		encode(result, reinterpret_cast<const unsigned char*>(decoded), decoded_size);
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	Result encode(const char* decoded, std::size_t decoded_size) const {
		Result result;
		encode(result, reinterpret_cast<const unsigned char*>(decoded), decoded_size);
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	void encode(Result& result, T (&&s)[N]) const {
		encode(result, s, N - 1);
	}

	template <typename Result = std::string, typename T, std::size_t N, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	Result encode(T (&&s)[N]) const {
		Result result;
		encode(result, s, N - 1);
		return result;
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	void encode(Result& result, std::string_view binary) const {
		return encode(result, binary.data(), binary.size());
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value>>
	Result encode(std::string_view binary) const {
		Result result;
		encode(result, binary.data(), binary.size());
		return result;
	}

BASEX_PRIVATE:
	// Sets result to the number in the text and returns how many bytes it stands for in a block-padded
	// alphabet, one for every 8 bits of digits, or how many leading zero bytes in one with leading_zeros,
	// one for every leading zero digit (0 in the others). Throws, leaving result as it was, on bad text.
	std::size_t decode_value(uinteger_t& result, const char* encoded, std::size_t encoded_size) const {
		std::int64_t sum = 0;
		std::int64_t sumsz = 0;
		int direction = 1;

		const std::size_t trailing = ((flags & BaseX::with_checksum) ? 1 : 0) + ((flags & BaseX::with_check) ? 1 : 0);
		if (encoded_size < trailing) {
			throw std::invalid_argument("Error: Encoded string is too short");
		}
		auto sz = encoded_size - trailing;

		// Collect the digits (skipping ignored characters), then convert them at once.
		unsigned char stack_digits[256];
		std::unique_ptr<unsigned char[]> heap_digits;
		unsigned char* digits = stack_digits;
		if (sz > sizeof(stack_digits)) {
			heap_digits.reset(new unsigned char[sz]);
			digits = heap_digits.get();
		}
		std::size_t count = 0;
		{
			// A run of digit characters converts 16 at a time; the loop below takes the rest.
			const auto done = to_digits(encoded, sz, digits);
			count = done;
			sz -= done;
			encoded += done;
		}
		for (;;) {
			// One check per character covers an invalid, an ignored and the padding character
			// (the padding character is never a digit, so it ends the digits here too).
			for (; sz; --sz, encoded += direction) {
				const auto d = ord(static_cast<int>(*encoded));
				if (static_cast<unsigned>(d) >= static_cast<unsigned>(alphabet_base)) break;
				digits[count++] = static_cast<unsigned char>(d);
			}
			if (!sz) break;
			const auto c = *encoded;
			if (padding_size && c == padding) break;  // alphabets without padding have none to match
			if (ord(static_cast<int>(c)) >= 0) {
				throw std::invalid_argument("Error: Invalid character: '" + std::string(1, c) + "' at " + std::to_string(encoded_size - sz));
			}
			--sz;  // ignored character
			encoded += direction;
		}
		if (flags & BaseX::with_checksum) {
			for (std::size_t i = 0; i < count; ++i) sum += digits[i];
		}
		sumsz = static_cast<std::int64_t>(count);
		// The padding ends the digits, and only padding and ignored characters may follow it, as in is_valid.
		for (; sz; --sz, ++encoded) {
			const auto c = *encoded;
			if (c != padding && ord(static_cast<int>(c)) >= 0) {
				throw std::invalid_argument("Error: Invalid character: '" + std::string(1, c) + "' at " + std::to_string(encoded_size - sz));
			}
		}
		const std::size_t bits = count * block_size;  // zero in an alphabet without block padding
		// The leading zero digits (ignored characters are not in digits): each stands for a zero byte.
		std::size_t zeros = 0;
		if (flags & BaseX::leading_zeros) {
			while (zeros < count && !digits[zeros]) {
				++zeros;
			}
		}
		result = uinteger_t::from_digits(digits + zeros, count - zeros, alphabet_base);

		if (bits & 7) {
			result >>= (bits & 7);
		}

		if (flags & BaseX::with_check) {
			auto c = *encoded;
			auto d = ord(static_cast<int>(c));
			if (d < 0 || d >= size) {
				throw std::invalid_argument("Error: Invalid character: '" + std::string(1, c) + "' at " + std::to_string(encoded_size - sz));
			}
			auto chk = static_cast<int>(result % size);
			if (d != chk) {
				throw std::invalid_argument("Error: Invalid check");
			}
			sum += chk;
			++sumsz;
			++encoded;
		}

		if (flags & BaseX::with_checksum) {
			auto c = *encoded;
			auto d = ord(static_cast<int>(c));
			if (d < 0 || d >= size) {
				throw std::invalid_argument("Error: Invalid character: '" + std::string(1, c) + "' at " + std::to_string(encoded_size - sz));
			}
			sum += d;
			sum += (sumsz + sumsz / size) % size;
			if (sum % size) {
				throw std::invalid_argument("Error: Invalid checksum");
			}
		}
		return block_size ? bits >> 3 : zeros;
	}

public:
	void decode(uinteger_t& result, const char* encoded, std::size_t encoded_size) const {
		decode_value(result, encoded, encoded_size);
	}

	template <typename Result, typename = typename std::enable_if_t<uinteger_t::is_result<Result>::value>>
	void decode(Result& result, const char* encoded, std::size_t encoded_size) const {
		uinteger_t num;
		const auto nbytes = decode_value(num, encoded, encoded_size);
		if (block_size) {
			// A block-padded text stands for exactly the bytes its digits hold: leading zero bytes included.
			result.clear();
			if (num) {
				num.digits(result, 256);
			}
			if (result.size() < nbytes) {
				result.insert(result.begin(), nbytes - result.size(), 0);
			}
		} else if (flags & BaseX::leading_zeros) {
			// Each leading zero digit is a zero byte; the number's bytes follow them.
			result.clear();
			if (nbytes) {
				result.resize(nbytes, 0);
			}
			if (num) {
				num.digits(result, 256);
			}
		} else if (result.empty()) {
			num.digits(result, 256);  // appends to nothing: the bytes go straight into the result
		} else {
			result = num.template str<Result>(256);
		}
	}

	// Decode straight into a native integer Result, reversing the integer encode.
	// is_result and is_integral are disjoint, so this never collides with the
	// string-like overload above. Without it the value-returning decode<Integral>
	// below would have no reference-form to delegate to (it failed to compile).
	template <typename Result, typename = typename std::enable_if_t<std::is_integral<Result>::value>, typename = void>
	void decode(Result& result, const char* encoded, std::size_t encoded_size) const {
		uinteger_t num;
		decode(num, encoded, encoded_size);
		result = static_cast<Result>(num);
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value or std::is_integral<Result>::value>>
	Result decode(const char* encoded, std::size_t encoded_size) const {
		Result result;
		decode(result, encoded, encoded_size);
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N, typename = std::enable_if_t<uinteger_t::is_result<Result>::value or std::is_integral<Result>::value>>
	void decode(Result& result, T (&&s)[N]) const {
		decode(result, s, N - 1);
	}

	template <typename Result = std::string, typename T, std::size_t N, typename = std::enable_if_t<uinteger_t::is_result<Result>::value or std::is_integral<Result>::value>>
	Result decode(T (&&s)[N]) const {
		Result result;
		decode(result, s, N - 1);
		return result;
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value or std::is_integral<Result>::value>>
	void decode(Result& result, std::string_view encoded) const {
		decode(result, encoded.data(), encoded.size());
	}

	template <typename Result = std::string, typename = std::enable_if_t<uinteger_t::is_result<Result>::value or std::is_integral<Result>::value>>
	Result decode(std::string_view encoded) const {
		Result result;
		decode(result, encoded.data(), encoded.size());
		return result;
	}

	// Whether the text is well formed for this alphabet: only digits and ignored characters, then
	// any padding; the check character (when there is one) last but for the checksum character, either
	// one a digit or an extended character; and the checksum holds. It does not recompute the value
	// of a check character, so decode can still reject a text that this accepts.
	bool is_valid(const char* encoded, std::size_t encoded_size) const {
		const std::size_t trailing = ((flags & BaseX::with_checksum) ? 1 : 0) + ((flags & BaseX::with_check) ? 1 : 0);
		if (encoded_size < trailing) {
			return false;
		}
		const auto sz = encoded_size - trailing;
		std::int64_t sum = 0;
		std::int64_t sumsz = 0;
		bool padded = false;
		for (std::size_t i = 0; i < sz; ++i) {
			const auto c = encoded[i];
			const auto d = ord(static_cast<int>(c));
			if (static_cast<unsigned>(d) < static_cast<unsigned>(alphabet_base)) {
				if (padded) return false;  // a digit after the padding
				sum += d;
				++sumsz;
			} else if (d >= 0) {
				if (!padding_size || c != padding) return false;
				padded = true;
			}  // else an ignored character
		}
		const char* tail = encoded + sz;
		if (flags & BaseX::with_check) {
			const auto d = ord(static_cast<int>(*tail++));
			if (d < 0 || d >= size) return false;
			sum += d;
			++sumsz;
		}
		if (flags & BaseX::with_checksum) {
			const auto d = ord(static_cast<int>(*tail));
			if (d < 0 || d >= size) return false;
			sum += d;
			sum += (sumsz + sumsz / size) % size;
			if (sum % size) return false;
		}
		return true;
	}

	template <typename T, std::size_t N>
	bool is_valid(T (&&s)[N]) const {
		return is_valid(s, N - 1);
	}

	bool is_valid(std::string_view encoded) const {
		return is_valid(encoded.data(), encoded.size());
	}
};

// base2
struct Base2 {
	static const BaseX& base2() {
		static constexpr BaseX encoder(0, "01", "", "", "");
		return encoder;
	}
	static const BaseX& base2chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "01", "", "", "");
		return encoder;
	}
};

// base8
struct Base8 {
	static const BaseX& base8() {
		static constexpr BaseX encoder(0, "01234567", "", "", "");
		return encoder;
	}
	static const BaseX& base8chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "01234567", "", "", "");
		return encoder;
	}
};

// base11
struct Base11 {
	static const BaseX& base11() {
		static constexpr BaseX encoder(BaseX::ignore_case, "0123456789a", "", "", "");
		return encoder;
	}
	static const BaseX& base11chk() {
		static constexpr BaseX encoder(BaseX::ignore_case | BaseX::with_checksum, "0123456789a", "", "", "");
		return encoder;
	}
};

// base16
struct Base16 {
	static const BaseX& base16() {
		static constexpr BaseX encoder(BaseX::ignore_case, "0123456789abcdef", "", "", "");
		return encoder;
	}
	static const BaseX& base16chk() {
		static constexpr BaseX encoder(BaseX::ignore_case | BaseX::with_checksum, "0123456789abcdef", "", "", "");
		return encoder;
	}
	static const BaseX& rfc4648() {
		static constexpr BaseX encoder(BaseX::block_padding, "0123456789ABCDEF", "", "", "");
		return encoder;
	}
};

// base32
struct Base32 {
	static const BaseX& base32() {
		static constexpr BaseX encoder(BaseX::ignore_case, "0123456789abcdefghijklmnopqrstuv", "", "", "");
		return encoder;
	}
	static const BaseX& base32chk() {
		static constexpr BaseX encoder(BaseX::ignore_case | BaseX::with_checksum, "0123456789abcdefghijklmnopqrstuv", "", "", "");
		return encoder;
	}
	static const BaseX& crockford() {
		static constexpr BaseX encoder(BaseX::ignore_case, "0123456789ABCDEFGHJKMNPQRSTVWXYZ", "", "", "-0O1IL");
		return encoder;
	}
	static const BaseX& crockfordchk() {
		static constexpr BaseX encoder(BaseX::ignore_case | BaseX::with_check, "0123456789ABCDEFGHJKMNPQRSTVWXYZ", "*~$=U", "", "-0O1IL");
		return encoder;
	}
	static const BaseX& rfc4648() {
		static constexpr BaseX encoder(BaseX::block_padding, "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", "", "========", "\n\r");
		return encoder;
	}
	static const BaseX& rfc4648hex() {
		static constexpr BaseX encoder(BaseX::block_padding, "0123456789ABCDEFGHIJKLMNOPQRSTUV", "", "========", "\n\r");
		return encoder;
	}
};

// base36
struct Base36 {
	static const BaseX& base36() {
		static constexpr BaseX encoder(BaseX::ignore_case, "0123456789abcdefghijklmnopqrstuvwxyz", "", "", "");
		return encoder;
	}
	static const BaseX& base36chk() {
		static constexpr BaseX encoder(BaseX::ignore_case | BaseX::with_checksum, "0123456789abcdefghijklmnopqrstuvwxyz", "", "", "");
		return encoder;
	}
};

// base58
struct Base58 {
	static const BaseX& base58() {
		static constexpr BaseX encoder(0, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv", "", "", "");
		return encoder;
	}
	static const BaseX& base58chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv", "", "", "");
		return encoder;
	}
	static const BaseX& bitcoin() {
		static constexpr BaseX encoder(BaseX::leading_zeros, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", "", "", "");
		return encoder;
	}
	static const BaseX& bitcoinchk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", "", "", "");
		return encoder;
	}
	static const BaseX& ripple() {
		static constexpr BaseX encoder(BaseX::leading_zeros, "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz", "", "", "");
		return encoder;
	}
	static const BaseX& ripplechk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz", "", "", "");
		return encoder;
	}
	static const BaseX& flickr() {
		static constexpr BaseX encoder(0, "123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ", "", "", "");
		return encoder;
	}
	static const BaseX& flickrchk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ", "", "", "");
		return encoder;
	}
};

// base59
struct Base59 {
	static const BaseX& base59() {
		static constexpr BaseX encoder(0, "23456789abcdefghijklmnopqrstuvwxyzABCDEFGHJKLMNOPQRSTUVWXYZ", "", "", "l1IO0");
		return encoder;
	}
	static const BaseX& base59chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "23456789abcdefghijklmnopqrstuvwxyzABCDEFGHJKLMNOPQRSTUVWXYZ", "", "", "l1IO0");
		return encoder;
	}
	static const BaseX& dubaluchk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "zGLUAC2EwdDRrkWBatmscxyYlg6jhP7K53TibenZpMVuvoO9H4XSQq8FfJN", "", "", "~l1IO0");
		return encoder;
	}
};

// base62
struct Base62 {
	static const BaseX& base62() {
		static constexpr BaseX encoder(0, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", "", "", "");
		return encoder;
	}
	static const BaseX& base62chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", "", "", "");
		return encoder;
	}
	static const BaseX& inverted() {
		static constexpr BaseX encoder(0, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", "", "", "");
		return encoder;
	}
	static const BaseX& invertedchk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", "", "", "");
		return encoder;
	}
};

// base64
struct Base64 {
	static const BaseX& base64() {
		static constexpr BaseX encoder(0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", "", "", "");
		return encoder;
	}
	static const BaseX& base64chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", "", "", "");
		return encoder;
	}
	static const BaseX& url() {
		static constexpr BaseX encoder(0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", "", "", "");
		return encoder;
	}
	static const BaseX& urlchk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", "", "", "");
		return encoder;
	}
	static const BaseX& rfc4648() {
		static constexpr BaseX encoder(BaseX::block_padding, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", "", "====", "\n\r");
		return encoder;
	}
	static const BaseX& rfc4648url() {
		static constexpr BaseX encoder(BaseX::block_padding, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", "", "====", "\n\r");
		return encoder;
	}
	static const BaseX& rfc4648url_unpadded() {
		static constexpr BaseX encoder(BaseX::block_padding, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", "", "", "\n\r");
		return encoder;
	}
};

// base66
struct Base66 {
	static const BaseX& base66() {
		static constexpr BaseX encoder(0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~", "", "", "");
		return encoder;
	}
	static const BaseX& base66chk() {
		static constexpr BaseX encoder(BaseX::with_checksum, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~", "", "", "");
		return encoder;
	}
};

#endif
