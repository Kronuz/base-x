/*
base_x.hh
BaseX encoder / decoder for C++

Copyright (c) 2017 German Mendez Bravo (Kronuz) @ german dot mb at gmail.com

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

#include <algorithm>

#include "uinteger_t.hh"

#define IGNORE_CASE    1
#define BLOCK_PADDING  2


class BaseX {
	char _chr[256];
	unsigned char _ord[256];

	const int base;
	const unsigned base_size;
	const unsigned base_bits;
	const unsigned block_size;
	const uinteger_t::digit base_mask;

	const char& chr(int ord) const {
		return _chr[ord];
	}

	const unsigned char& ord(int chr) const {
		return _ord[chr];
	}

public:
	template <typename A, std::size_t alphabet_size, typename I, std::size_t ignored_size>
	constexpr BaseX(A (&alphabet)[alphabet_size], I (&ignored)[ignored_size], int flags = 0) :
		_chr(),
		_ord(),
		base(alphabet_size - 1),
		base_size(uinteger_t::base_size(base)),
		base_bits(uinteger_t::base_bits(base)),
		block_size((flags & BLOCK_PADDING) ? base_bits : 0),
		base_mask(base - 1)
	{
		for (auto i = 256; i; --i) {
			_chr[i - 1] = 0;
			_ord[i - 1] = 0xff;
		}
		for (auto i = ignored_size - 1; i; --i) {
			auto ch = ignored[i - 1];
			_ord[(int)ch] = 0;
		}
		for (auto i = base; i; --i) {
			auto ch = alphabet[i - 1];
			_chr[i - 1] = ch;
			_ord[(int)ch] = i - 1;
			if (flags & IGNORE_CASE) {
				if (ch >= 'A' && ch <='Z') {
					_ord[(int)ch - 'A' + 'a'] = i - 1;
				} else if (ch >= 'a' && ch <='z') {
					_ord[(int)ch - 'a' + 'A'] = i - 1;
				}
			}
		}
	}

	// Get string representation of value
	template <typename Result = std::string>
	void encode(Result& result, const uinteger_t& num, bool checksum = false) const {
		auto num_sz = num.size();
		if (num_sz) {
			int sum = 0;
			auto alphabet_base = base;
			result.reserve(num_sz * base_size);
			int bp = 0;
			if (block_size) {
				bp = ((num.bits() + 7) & 0xf8) % block_size;
				bp = bp ? block_size - bp : 0;
			}
			uinteger_t quotient = num << bp;
			if (base_bits) {
				std::size_t shift = 0;
				auto ptr = reinterpret_cast<const uinteger_t::half_digit*>(quotient.data());
				uinteger_t::digit v = *ptr++;
				v <<= uinteger_t::half_digit_bits;
				for (auto i = num_sz * 2 - 1; i; --i) {
					v >>= uinteger_t::half_digit_bits;
					v |= (static_cast<uinteger_t::digit>(*ptr++) << uinteger_t::half_digit_bits);
					do {
						auto d = static_cast<int>((v >> shift) & base_mask);
						sum ^= d;
						result.push_back(chr(d));
						shift += base_bits;
					} while (shift <= uinteger_t::half_digit_bits);
					shift -= uinteger_t::half_digit_bits;
				}
				v >>= (shift + uinteger_t::half_digit_bits);
				while (v) {
					auto d = static_cast<int>(v & base_mask);
					sum ^= d;
					result.push_back(chr(d));
					v >>= base_bits;
				}
				auto s = chr(0);
				auto rit_f = std::find_if(result.rbegin(), result.rend(), [s](const char& c) { return c != s; });
				result.resize(result.rend() - rit_f); // shrink
			} else {
				uinteger_t uint_base = alphabet_base;
				do {
					auto r = quotient.divmod(uint_base);
					auto d = static_cast<int>(r.second);
					sum ^= d;
					result.push_back(chr(d));
					quotient = std::move(r.first);
				} while (quotient);
			}
			std::reverse(result.begin(), result.end());
			if (checksum) {
				auto sz = result.size();
				sum ^= (sz / base) % base;
				sum ^= (sz % base);
				result.push_back(chr(sum));
			}
		} else {
			result.push_back(chr(0));
		}
	}

	template <typename Result = std::string>
	Result encode(uinteger_t num, bool checksum = false) const {
		Result result;
		encode(result, num, checksum);
		return result;
	}

	template <typename Result = std::string>
	void encode(Result& result, const char* bytes, size_t size, bool checksum = false) const {
		encode(result, uinteger_t(bytes, size, 256), checksum);
	}

	template <typename Result = std::string>
	Result encode(const char* bytes, size_t size, bool checksum = false) const {
		Result result;
		encode(result, uinteger_t(bytes, size, 256), checksum);
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N>
	void encode(Result& result, T (&s)[N], bool checksum = false) const {
		encode(result, s, N - 1, checksum);
	}

	template <typename Result = std::string, typename T, std::size_t N>
	Result encode(T (&s)[N], bool checksum = false) const {
		Result result;
		encode(result, s, N - 1, checksum);
		return result;
	}

	template <typename Result = std::string>
	void encode(Result& result, const std::string& binary, bool checksum = false) const {
		return encode(result, binary.data(), binary.size(), checksum);
	}

	template <typename Result = std::string>
	Result encode(const std::string& binary, bool checksum = false) const {
		Result result;
		encode(result, binary.data(), binary.size(), checksum);
		return result;
	}

	void decode(uinteger_t& result, const char* encoded, std::size_t encoded_size, bool checksum = false) const {
		result = 0;
		int sum = 0;
		if (checksum) {
			auto sz = encoded_size - 1;
			sum ^= (sz / base) % base;
			sum ^= (sz % base);
			--encoded_size;
		}
		int bp = 0;
		if (block_size) {
			bp = (encoded_size * block_size) % 8;
		}
		if (base_bits) {
			for (; encoded_size; --encoded_size, ++encoded) {
				auto d = ord(static_cast<int>(*encoded));
				sum ^= d;
				if (d >= base) {
					throw std::invalid_argument("Error: Invalid character: '" + std::string(1, *encoded) + "' at " + std::to_string(encoded_size));
				}
				result = (result << base_bits) | d;
			}
		} else {
			uinteger_t uint_base = base;
			for (; encoded_size; --encoded_size, ++encoded) {
				auto d = ord(static_cast<int>(*encoded));
				if (d >= base) {
					throw std::invalid_argument("Error: Invalid character: '" + std::string(1, *encoded) + "' at " + std::to_string(encoded_size));
				}
				sum ^= d;
				result = (result * uint_base) + d;
			}
		}

		result >>= bp;

		if (checksum) {
			auto d = ord(static_cast<int>(*encoded));
			if (d >= base) {
				throw std::invalid_argument("Error: Invalid character: '" + std::string(1, *encoded) + "' at " + std::to_string(encoded_size));
			}
			sum ^= d;
			if (sum) {
				throw std::invalid_argument("Error: Invalid checksum");
			}
		}
	}

	template <typename Result, typename = typename std::enable_if<!std::is_integral<Result>::value>::type>
	void decode(Result& result, const char* encoded, size_t encoded_size, bool checksum = false) const {
		uinteger_t num;
		decode(num, encoded, encoded_size, checksum);
		result = num.template str<Result>(256);
	}

	template <typename Result>
	Result decode(const char* encoded, size_t encoded_size, bool checksum = false) const {
		Result result;
		decode(result, encoded, encoded_size, checksum);
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N>
	void decode(Result& result, T (&s)[N], bool checksum = false) const {
		decode(result, s, N - 1, checksum);
	}

	template <typename Result = std::string, typename T, std::size_t N>
	Result decode(T (&s)[N], bool checksum = false) const {
		Result result;
		decode(result, s, N - 1, checksum);
		return result;
	}

	template <typename Result = std::string>
	void decode(Result& result, const std::string& encoded, bool checksum = false) const {
		decode(result, encoded.data(), encoded.size(), checksum);
	}

	template <typename Result = std::string>
	Result decode(const std::string& encoded, bool checksum = false) const {
		Result result;
		decode(result, encoded.data(), encoded.size(), checksum);
		return result;
	}

	bool is_valid(const char* encoded, size_t encoded_size, bool checksum = false) const {
		int sum = 0;
		if (checksum) {
			auto sz = encoded_size - 1;
			sum ^= (sz / base) % base;
			sum ^= (sz % base);
		}
		for (; encoded_size; --encoded_size, ++encoded) {
			auto d = ord(static_cast<int>(*encoded));
			if (d >= base) {
				return false;
			}
			sum ^= d;
		}
		if (checksum && sum) {
			return false;
		}
		return true;
	}

	template <typename T, std::size_t N>
	bool is_valid(T (&s)[N], bool checksum = false) const {
		return is_valid(s, N - 1, checksum);
	}

	bool is_valid(const std::string& encoded, bool checksum = false) const {
		return is_valid(encoded.data(), encoded.size(), checksum);
	}
};

// base2
namespace base2 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base2() {
		static constexpr BaseX encoder("01", "");
		return encoder;
	}
}

// base8
namespace base8 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base8() {
		static constexpr BaseX encoder("01234567", "");
		return encoder;
	}
}

// base11
namespace base11 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base11() {
		static constexpr BaseX encoder("0123456789a", "", IGNORE_CASE);
		return encoder;
	}
}

// base16
namespace base16 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base16() {
		static constexpr BaseX encoder("0123456789abcdef", "", IGNORE_CASE);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& rfc4648() {
		static constexpr BaseX encoder("0123456789ABCDEF", "= \n\r\t", IGNORE_CASE);
		return encoder;
	}
}

// base32
namespace base32 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base32() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", "", IGNORE_CASE);
		return encoder;
	}
	const BaseX& hex() {
		static constexpr BaseX encoder("0123456789ABCDEFGHIJKLMNOPQRSTUV", "", IGNORE_CASE);
		return encoder;
	}
	const BaseX& rfc4648() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", "= \n\r\t", IGNORE_CASE | BLOCK_PADDING);
		return encoder;
	}
	const BaseX& rfc4648hex() {
		static constexpr BaseX encoder("0123456789ABCDEFGHIJKLMNOPQRSTUV", "= \n\r\t", IGNORE_CASE | BLOCK_PADDING);
		return encoder;
	}
	const BaseX& crockford() {
		static constexpr BaseX encoder("0123456789ABCDEFGHJKMNPQRSTVWXYZ", "", IGNORE_CASE);
		return encoder;
	}
}

// base36
namespace base36 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base36() {
		static constexpr BaseX encoder("0123456789abcdefghijklmnopqrstuvwxyz", "", IGNORE_CASE);
		return encoder;
	}
}

// base58
namespace base58 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& gmp() {
		static constexpr BaseX encoder("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& bitcoin() {
		static constexpr BaseX encoder("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& ripple() {
		static constexpr BaseX encoder("rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& flickr() {
		static constexpr BaseX encoder("123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& base58() {
		return bitcoin<uinteger_t>();
	}
}

// base62
namespace base62 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& inverted() {
		static constexpr BaseX encoder("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& base62() {
		static constexpr BaseX encoder("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", "");
		return encoder;
	}
}

// base64
namespace base64 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base64() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& url() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", "");
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& rfc4648() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", "= \n\r\t", BLOCK_PADDING);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& rfc4648url() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", "= \n\r\t", BLOCK_PADDING);
		return encoder;
	}
}

// base66
namespace base66 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base66() {
		static constexpr BaseX encoder("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~", "");
		return encoder;
	}
}

#endif
