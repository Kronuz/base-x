/*
uint256_t.hh
An unsigned 256 bit integer type for C++

Copyright (c) 2017 German Mendez Bravo (Kronuz) @ german dot mb at gmail.com
Copyright (c) 2013 - 2017 Jason Lee @ calccrypto at gmail.com

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

With much help from Auston Sterling

Thanks to Stefan Deigmüller for finding
a bug in operator*.

Thanks to François Dessenne for convincing me
to do a general rewrite of this class.

Converted to header-only by German Mendez Bravo (Kronuz)
*/

#ifndef __UINT256_T__
#define __UINT256_T__

#include <array>
#include <string>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

class uint256_t;

namespace std {  // This is probably not a good idea
	// Give uint256_t type traits
	template <> struct is_arithmetic <uint256_t> : std::true_type {};
	template <> struct is_integral   <uint256_t> : std::true_type {};
	template <> struct is_unsigned   <uint256_t> : std::true_type {};
}

class uint256_t {
	private:
		std::array<uint64_t, 4> number;

	public:
		// Constructors
		uint256_t() { }
		uint256_t(const uint256_t& rhs)
			: number(rhs.number) { }
		uint256_t(uint256_t&& rhs)
			: number(std::move(rhs.number)) { }

		template <typename S0, typename = typename std::enable_if<std::is_integral<S0>::value>::type>
		uint256_t(const S0 & rhs0)
			: number(std::array<uint64_t, 4>({{ static_cast<uint64_t>(rhs0), 0, 0, 0 }})) { }

		template <typename S0, typename S1, typename = typename std::enable_if<std::is_integral<S0>::value>::type>
		uint256_t(const S0 & rhs0, const S1 & rhs1)
			: number(std::array<uint64_t, 4>({{ static_cast<uint64_t>(rhs1), static_cast<uint64_t>(rhs0), 0, 0 }})) { }

		template <typename S0, typename S1, typename S2, typename = typename std::enable_if<std::is_integral<S0>::value>::type>
		uint256_t(const S0 & rhs0, const S1 & rhs1, const S2 & rhs2)
			: number(std::array<uint64_t, 4>({{ static_cast<uint64_t>(rhs2), static_cast<uint64_t>(rhs1), static_cast<uint64_t>(rhs0), 0 }})) { }

		template <typename S0, typename S1, typename S2, typename S3, typename = typename std::enable_if<std::is_integral<S0>::value>::type>
		uint256_t(const S0 & rhs0, const S1 & rhs1, const S2 & rhs2, const S3 & rhs3)
			: number(std::array<uint64_t, 4>({{ static_cast<uint64_t>(rhs3), static_cast<uint64_t>(rhs2), static_cast<uint64_t>(rhs1), static_cast<uint64_t>(rhs0) }})) { }

		uint256_t(const char* bytes, size_t size, size_t base)
			: number(std::array<uint64_t, 4>({{ 0, 0, 0, 0 }})) {
			if (base >= 2 && base <= 36) {
				for (; size; --size, ++bytes) {
					uint8_t d = std::tolower(*bytes);
					if (std::isdigit(d)) { // 0-9
						d -= '0';
					} else {
						d -= 'a' - 10;
					}
					if (d >= base) {
						throw std::runtime_error("Error: Not a digit in base " + std::to_string(base) + ": '" + std::string(1, *bytes) + "'");
					}
					*this = (*this * base) + d;
				}
			} else if (base == 256) {
				if (size > 32) {
					bytes += size - 32 + 1;
					size = 32;
				}
				char buf[32];
				std::memset(buf + size, 0, sizeof(buf) - size);
				for (; size; --size, ++bytes) {
					buf[size - 1] = *bytes;
				}
				number[3] = *(reinterpret_cast<uint64_t*>(buf) + 3);
				number[2] = *(reinterpret_cast<uint64_t*>(buf) + 2);
				number[1] = *(reinterpret_cast<uint64_t*>(buf) + 1);
				number[0] = *(reinterpret_cast<uint64_t*>(buf));
			} else {
				throw std::runtime_error("Error: Cannot convert from base " + std::to_string(base));
			}
		}

		template <typename T, std::size_t N>
		uint256_t(T (&s)[N], size_t base=10)
			: uint256_t(s, N - 1, base) { }

		template <typename T>
		uint256_t(const std::vector<T>& bytes, size_t base=10)
			: uint256_t(bytes.data(), bytes.size(), base) { }

		uint256_t(const std::string& bytes, size_t base=10)
			: uint256_t(bytes.data(), bytes.size(), base) { }

		//  RHS input args only

		// Assignment Operator
		uint256_t& operator=(const uint256_t& rhs) {
			number = rhs.number;
			return *this;
		}
		uint256_t& operator=(uint256_t&& rhs) {
			number = std::move(rhs.number);
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator=(const T & rhs) {
			number[3] = 0;
			number[2] = 0;
			number[1] = 0;
			number[0] = rhs;
			return *this;
		}

		// Typecast Operators
		explicit operator bool() const {
			return static_cast<bool>(number[0] | number[1] | number[2] | number[3]);
		}
		explicit operator unsigned char() const {
			return static_cast<unsigned char>(number[0]);
		}
		explicit operator unsigned short() const {
			return static_cast<unsigned short>(number[0]);
		}
		explicit operator unsigned int() const {
			return static_cast<unsigned int>(number[0]);
		}
		explicit operator unsigned long() const {
			return static_cast<unsigned long>(number[0]);
		}
		explicit operator unsigned long long() const {
			return static_cast<unsigned long long>(number[0]);
		}
		explicit operator char() const {
			return static_cast<char>(number[0]);
		}
		explicit operator short() const {
			return static_cast<short>(number[0]);
		}
		explicit operator int() const {
			return static_cast<int>(number[0]);
		}
		explicit operator long() const {
			return static_cast<long>(number[0]);
		}
		explicit operator long long() const {
			return static_cast<long long>(number[0]);
		}

		// Bitwise Operators
		uint256_t operator&(const uint256_t& rhs) const {
			return uint256_t(
				number[3] & rhs.number[3],
				number[2] & rhs.number[2],
				number[1] & rhs.number[1],
				number[0] & rhs.number[0]
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator&(const T & rhs) const {
			return uint256_t(number[0] & static_cast<uint64_t>(rhs));
		}

		uint256_t& operator&=(const uint256_t& rhs) {
			number[0] &= rhs.number[0];
			number[1] &= rhs.number[1];
			number[2] &= rhs.number[2];
			number[3] &= rhs.number[3];
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator&=(const T & rhs) {
			number[0] &= rhs;
			number[1] = 0;
			number[2] = 0;
			number[3] = 0;
			return *this;
		}

		uint256_t operator|(const uint256_t& rhs) const {
			return uint256_t(
				number[3] | rhs.number[3],
				number[2] | rhs.number[2],
				number[1] | rhs.number[1],
				number[0] | rhs.number[0]
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator|(const T & rhs) const {
			return uint256_t(
				number[3],
				number[2],
				number[1],
				number[0] | static_cast<uint64_t>(rhs)
			);
		}

		uint256_t& operator|=(const uint256_t& rhs) {
			number[0] |= rhs.number[0];
			number[1] |= rhs.number[1];
			number[2] |= rhs.number[2];
			number[3] |= rhs.number[3];
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator|=(const T & rhs) {
			number[0] &= rhs;
			return *this;
		}

		uint256_t operator^(const uint256_t& rhs) const {
			return uint256_t(
				number[3] ^ rhs.number[3],
				number[2] ^ rhs.number[2],
				number[1] ^ rhs.number[1],
				number[0] ^ rhs.number[0]
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator^(const T & rhs) const {
			return uint256_t(
				number[3],
				number[2],
				number[1],
				number[0] ^ static_cast<uint64_t>(rhs)
			);
		}

		uint256_t& operator^=(const uint256_t& rhs) {
			number[0] ^= rhs.number[0];
			number[1] ^= rhs.number[1];
			number[2] ^= rhs.number[2];
			number[3] ^= rhs.number[3];
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator^=(const T & rhs) {
			number[0] ^= rhs;
			return *this;
		}

		uint256_t operator~() const {
			return uint256_t(
				~number[3],
				~number[2],
				~number[1],
				~number[0]
			);
		}

		// Bit Shift Operators
		uint256_t operator<<(const uint256_t& rhs) const {
			uint64_t shift = rhs.number[0];
			if (rhs.number[3] || rhs.number[2] || rhs.number[1] || shift >= 256) {
				return uint256_0();
			} else if (shift == 0) {
				return *this;
			} else if (shift == 64) {
				return uint256_t(
					number[2],
					number[1],
					number[0],
					0
				);
			} else if (shift == 128) {
				return uint256_t(
					number[1],
					number[0],
					0,
					0
				);
			} else if (shift == 192) {
				return uint256_t(
					number[0],
					0,
					0,
					0
				);
			} else if (shift < 64) {
				return uint256_t(
					(number[3] << shift) | (number[2] >> (64 - shift)),
					(number[2] << shift) | (number[1] >> (64 - shift)),
					(number[1] << shift) | (number[0] >> (64 - shift)),
					number[0] << shift
				);
			} else if (shift < 128) {
				shift -= 64;
				return uint256_t(
					(number[2] << shift) | (number[1] >> (64 - shift)),
					(number[1] << shift) | (number[0] >> (64 - shift)),
					number[0] << shift,
					0
				);
			} else if (shift < 192) {
				shift -= 128;
				return uint256_t(
					(number[1] << shift) | (number[0] >> (64 - shift)),
					number[0] << shift,
					0,
					0
				);
			} else if (shift < 256) {
				shift -= 192;
				return uint256_t(
					number[0] << shift,
					0,
					0,
					0
				);
			} else {
				return uint256_0();
			}
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator<<(const T & rhs) const {
			return *this << uint256_t(rhs);
		}

		uint256_t& operator<<=(const uint256_t& rhs) {
			*this = *this << rhs;
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator<<=(const T & rhs) {
			*this = *this << uint256_t(rhs);
			return *this;
		}

		uint256_t operator>>(const uint256_t& rhs) const {
			uint64_t shift = rhs.number[0];
			if (rhs.number[3] || rhs.number[2] || rhs.number[1] || shift >= 256) {
				return uint256_0();
			} else if (shift == 0) {
				return *this;
			} else if (shift == 64) {
				return uint256_t(
					0,
					number[3],
					number[2],
					number[1]
				);
			} else if (shift == 128) {
				return uint256_t(
					0,
					0,
					number[3],
					number[2]
				);
			} else if (shift == 192) {
				return uint256_t(
					0,
					0,
					0,
					number[3]
				);
			} else if (shift < 64) {
				return uint256_t(
					(number[3] >> shift),
					(number[2] >> shift) | (number[3] << (64 - shift)),
					(number[1] >> shift) | (number[2] << (64 - shift)),
					(number[0] >> shift) | (number[1] << (64 - shift))
				);
			} else if (shift < 128) {
				shift -= 64;
				return uint256_t(
					0,
					(number[3] >> shift),
					(number[2] >> shift) | (number[3] << (64 - shift)),
					(number[1] >> shift) | (number[2] << (64 - shift))
				);
			} else if (shift < 192) {
				shift -= 128;
				return uint256_t(
					0,
					0,
					(number[3] >> shift),
					(number[2] >> shift) | (number[3] << (64 - shift))
				);
			} else if (shift < 256) {
				shift -= 192;
				return uint256_t(
					0,
					0,
					0,
					(number[3] >> shift)
				);
			} else {
				return uint256_0();
			}
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator>>(const T & rhs) const {
			return *this >> uint256_t(rhs);
		}

		uint256_t& operator>>=(const uint256_t& rhs) {
			*this = *this >> rhs;
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator>>=(const T & rhs) {
			*this = *this >> uint256_t(rhs);
			return *this;
		}

		// Logical Operators
		bool operator!() const {
			return !static_cast<bool>(number[0] | number[1] | number[2] | number[3]);
		}

		bool operator&&(const uint256_t& rhs) const {
			return static_cast<bool>(*this) && rhs;
		}

		bool operator||(const uint256_t& rhs) const {
			return static_cast<bool>(*this) || rhs;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator&&(const T & rhs) {
			return static_cast<bool>(*this) && rhs;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator||(const T & rhs) {
			return static_cast<bool>(*this) || rhs;
		}

		// Comparison Operators
		bool operator==(const uint256_t& rhs) const {
			return (
				number[3] == rhs.number[3] &&
				number[2] == rhs.number[2] &&
				number[1] == rhs.number[1] &&
				number[0] == rhs.number[0]
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator==(const T & rhs) const {
			return (
				!number[3] &&
				!number[2] &&
				!number[1] &&
				number[0] == static_cast<uint64_t>(rhs)
			);
		}

		bool operator!=(const uint256_t& rhs) const {
			return (
				number[3] != rhs.number[3] ||
				number[2] != rhs.number[2] ||
				number[1] != rhs.number[1] ||
				number[0] != rhs.number[0]
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator!=(const T & rhs) const {
			return (
				number[3] |
				number[2] |
				number[1] |
				number[0] != static_cast<uint64_t>(rhs)
			);
		}

		bool operator>(const uint256_t& rhs) const {
			return (
				number[3] > rhs.number[3] || (
					number[3] == rhs.number[3] && (
						number[2] > rhs.number[2] || (
							number[2] == rhs.number[2] && (
								number[1] > rhs.number[1] || (
									number[1] == rhs.number[1] && (
										number[0] > rhs.number[0]
									)
								)
							)
						)
					)
				)
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator>(const T & rhs) const {
			return (
				number[3] ||
				number[2] ||
				number[1] ||
				(number[0] > static_cast<uint64_t>(rhs))
			);
		}

		bool operator<(const uint256_t& rhs) const {
			return (
				number[3] < rhs.number[3] || (
					number[3] == rhs.number[3] && (
						number[2] < rhs.number[2] || (
							number[2] == rhs.number[2] && (
								number[1] < rhs.number[1] || (
									number[1] == rhs.number[1] && (
										number[0] < rhs.number[0]
									)
								)
							)
						)
					)
				)
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator<(const T & rhs) const {
			return (
				!number[3] &&
				!number[2] &&
				!number[1] &&
				number[0] < static_cast<uint64_t>(rhs)
			);
		}

		bool operator>=(const uint256_t& rhs) const {
			return (
				number[3] > rhs.number[3] || (
					number[3] == rhs.number[3] && (
						number[2] > rhs.number[2] || (
							number[2] == rhs.number[2] && (
								number[1] > rhs.number[1] || (
									number[1] == rhs.number[1] && (
										number[0] >= rhs.number[0]
									)
								)
							)
						)
					)
				)
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator>=(const T & rhs) const {
			return (
				number[3] ||
				number[2] ||
				number[1] ||
				(number[0] >= static_cast<uint64_t>(rhs))
			);
		}

		bool operator<=(const uint256_t& rhs) const {
			return (
				number[3] < rhs.number[3] || (
					number[3] == rhs.number[3] && (
						number[2] < rhs.number[2] || (
							number[2] == rhs.number[2] && (
								number[1] < rhs.number[1] || (
									number[1] == rhs.number[1] && (
										number[0] <= rhs.number[0]
									)
								)
							)
						)
					)
				)
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		bool operator<=(const T & rhs) const {
			return (
				!number[3] &&
				!number[2] &&
				!number[1] &&
				number[0] <= static_cast<uint64_t>(rhs)
			);
		}

		// Arithmetic Operators
		uint256_t operator+(const uint256_t& rhs) const {
			auto n0 = number[0] + rhs.number[0];
			auto n1 = number[1] + rhs.number[1];
			auto n2 = number[2] + rhs.number[2];
			auto n3 = number[3] + rhs.number[3];
			return uint256_t(
				n3 + (n2 < number[2]),
				n2 + (n1 < number[1]),
				n1 + (n0 < number[0]),
				n0
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator+(const T & rhs) const {
			auto n0 = number[0] + static_cast<uint64_t>(rhs);
			return uint256_t(
				number[3],
				number[2],
				number[1] + (n0 < number[0]),
				n0
			);
		}

		uint256_t& operator+=(const uint256_t& rhs) {
			number[3] += rhs.number[3] + ((number[2] + rhs.number[2]) < number[2]);
			number[2] += rhs.number[2] + ((number[1] + rhs.number[1]) < number[1]);
			number[1] += rhs.number[1] + ((number[0] + rhs.number[0]) < number[0]);
			number[0] += rhs.number[0];
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator+=(const T & rhs) {
			number[1] += ((number[0] + rhs) < number[0]);
			number[0] += rhs;
			return *this;
		}

		uint256_t operator-(const uint256_t& rhs) const {
			auto n0 = number[0] - rhs.number[0];
			auto n1 = number[1] - rhs.number[1];
			auto n2 = number[2] - rhs.number[2];
			auto n3 = number[3] - rhs.number[3];
			return uint256_t(
				n3 - (n2 > number[2]),
				n2 - (n1 > number[1]),
				n1 - (n0 > number[0]),
				n0
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator-(const T & rhs) const {
			auto n0 = number[0] - static_cast<uint64_t>(rhs);
			return uint256_t(
				number[3],
				number[2],
				number[1] - (n0 > number[0]),
				n0
			);
		}

		uint256_t& operator-=(const uint256_t& rhs) {
			*this = *this - rhs;
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator-=(const T & rhs) {
			*this = *this - rhs;
			return *this;
		}

		uint256_t operator*(const uint256_t& rhs) const {
			// split values into 8 32-bit parts
			uint64_t lhs32[8] = {
				number[3] >> 32,
				number[3] & 0xffffffff,
				number[2] >> 32,
				number[2] & 0xffffffff,
				number[1] >> 32,
				number[1] & 0xffffffff,
				number[0] >> 32,
				number[0] & 0xffffffff,
			};
			uint64_t rhs32[8] = {
				rhs.number[3] >> 32,
				rhs.number[3] & 0xffffffff,
				rhs.number[2] >> 32,
				rhs.number[2] & 0xffffffff,
				rhs.number[1] >> 32,
				rhs.number[1] & 0xffffffff,
				rhs.number[0] >> 32,
				rhs.number[0] & 0xffffffff,
			};
			uint64_t products[8][8];

			// multiply each component of the values
			for (int y = 7; y >= 0; --y) {
				for (int x = 7; x >= 0; --x) {
					products[7 - x][y] = lhs32[x] * rhs32[y];
				}
			}

			uint64_t result[8];

			// first row
			result[0] = (products[0][7] & 0xffffffff);
			result[1] = (products[0][6] & 0xffffffff) + (products[0][7] >> 32);
			result[2] = (products[0][5] & 0xffffffff) + (products[0][6] >> 32);
			result[3] = (products[0][4] & 0xffffffff) + (products[0][5] >> 32);
			result[4] = (products[0][3] & 0xffffffff) + (products[0][4] >> 32);
			result[5] = (products[0][2] & 0xffffffff) + (products[0][3] >> 32);
			result[6] = (products[0][1] & 0xffffffff) + (products[0][2] >> 32);
			result[7] = (products[0][0] & 0xffffffff) + (products[0][1] >> 32);

			// second row
			result[1] += (products[1][7] & 0xffffffff);
			result[2] += (products[1][6] & 0xffffffff) + (products[1][7] >> 32);
			result[3] += (products[1][5] & 0xffffffff) + (products[1][6] >> 32);
			result[4] += (products[1][4] & 0xffffffff) + (products[1][5] >> 32);
			result[5] += (products[1][3] & 0xffffffff) + (products[1][4] >> 32);
			result[6] += (products[1][2] & 0xffffffff) + (products[1][3] >> 32);
			result[7] += (products[1][1] & 0xffffffff) + (products[1][2] >> 32);

			// third row
			result[2] += (products[2][7] & 0xffffffff);
			result[3] += (products[2][6] & 0xffffffff) + (products[2][7] >> 32);
			result[4] += (products[2][5] & 0xffffffff) + (products[2][6] >> 32);
			result[5] += (products[2][4] & 0xffffffff) + (products[2][5] >> 32);
			result[6] += (products[2][3] & 0xffffffff) + (products[2][4] >> 32);
			result[7] += (products[2][1] & 0xffffffff) + (products[2][3] >> 32);

			// fourth row
			result[3] += (products[3][7] & 0xffffffff);
			result[4] += (products[3][6] & 0xffffffff) + (products[3][7] >> 32);
			result[5] += (products[3][5] & 0xffffffff) + (products[3][6] >> 32);
			result[6] += (products[3][4] & 0xffffffff) + (products[3][5] >> 32);
			result[7] += (products[3][3] & 0xffffffff) + (products[3][4] >> 32);

			// fifth row
			result[4] += (products[4][7] & 0xffffffff);
			result[5] += (products[4][6] & 0xffffffff) + (products[4][7] >> 32);
			result[6] += (products[4][5] & 0xffffffff) + (products[4][6] >> 32);
			result[7] += (products[4][4] & 0xffffffff) + (products[4][5] >> 32);

			// sixth row
			result[5] += (products[5][7] & 0xffffffff);
			result[6] += (products[5][6] & 0xffffffff) + (products[5][7] >> 32);
			result[7] += (products[5][5] & 0xffffffff) + (products[5][6] >> 32);

			// seventh row
			result[6] += (products[6][7] & 0xffffffff);
			result[7] += (products[6][6] & 0xffffffff) + (products[6][7] >> 32);

			// eight row
			result[7] += (products[7][7] & 0xffffffff);

			// move carry to next digit
			result[1] += result[0] >> 32;
			result[2] += result[1] >> 32;
			result[3] += result[2] >> 32;
			result[4] += result[3] >> 32;
			result[5] += result[4] >> 32;
			result[6] += result[5] >> 32;
			result[7] += result[6] >> 32;

			// remove carry from current digit
			result[0] &= 0xffffffff;
			result[1] &= 0xffffffff;
			result[2] &= 0xffffffff;
			result[3] &= 0xffffffff;
			result[4] &= 0xffffffff;
			result[5] &= 0xffffffff;
			result[6] &= 0xffffffff;
			result[7] &= 0xffffffff;

			// combine components
			return uint256_t(
				(result[7] << 32) | result[6],
				(result[5] << 32) | result[4],
				(result[3] << 32) | result[2],
				(result[1] << 32) | result[0]
			);
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator*(const T & rhs) const {
			return *this * uint256_t(rhs);
		}

		uint256_t& operator*=(const uint256_t& rhs) {
			*this = *this * rhs;
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator*=(const T & rhs) {
			*this = *this * uint256_t(rhs);
			return *this;
		}

		static const uint256_t uint256_0() {
			static uint256_t uint256_0(0);
			return uint256_0;
		}

		static const uint256_t uint256_1() {
			static uint256_t uint256_1(1);
			return uint256_1;
		}

		std::pair<uint256_t, uint256_t> divmod(const uint256_t& rhs) const {
			// Save some calculations /////////////////////

			if (rhs == uint256_0()) {
				throw std::domain_error("Error: division or modulus by 0");
			} else if (rhs == uint256_1()) {
				return std::make_pair(*this, uint256_0());
			} else if (*this == rhs) {
				return std::make_pair(uint256_1(), uint256_0());
			} else if ((*this == uint256_0()) || (*this < rhs)) {
				return std::make_pair(uint256_0(), *this);
			}

			std::pair<uint256_t, uint256_t> qr(uint256_0(), uint256_0());
			for (size_t x = bits(); x > 0; --x) {
				qr.first  <<= uint256_1();
				qr.second <<= uint256_1();

				if ((*this >> (x - 1U)) & 1) {
					++qr.second;
				}

				if (qr.second >= rhs) {
					qr.second -= rhs;
					++qr.first;
				}
			}
			return qr;
		}

		uint256_t operator/(const uint256_t& rhs) const {
			return divmod(rhs).first;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator/(const T & rhs) const {
			return *this / uint256_t(rhs);
		}

		uint256_t& operator/=(const uint256_t& rhs) {
			*this = *this / rhs;
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator/=(const T & rhs) {
			*this = *this / uint256_t(rhs);
			return *this;
		}

		uint256_t operator%(const uint256_t& rhs) const {
			return divmod(rhs).second;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t operator%(const T & rhs) const {
			return *this % uint256_t(rhs);
		}

		uint256_t& operator%=(const uint256_t& rhs) {
			*this = *this % rhs;
			return *this;
		}

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint256_t& operator%=(const T & rhs) {
			*this = *this % uint256_t(rhs);
			return *this;
		}

		// Increment Operator
		uint256_t& operator++() {
			return *this += uint256_1();
		}
		uint256_t operator++(int) {
			uint256_t temp(*this);
			++*this;
			return temp;
		}

		// Decrement Operator
		uint256_t& operator--() {
			return *this -= uint256_1();
		}
		uint256_t operator--(int) {
			uint256_t temp(*this);
			--*this;
			return temp;
		}

		// Nothing done since promotion doesn't work here
		uint256_t operator+() const {
			return *this;
		}

		// two's complement
		uint256_t operator-() const {
			return ~*this + uint256_1();
		}

		// Get private values
		const uint64_t& operator[](size_t idx) const {
			return number[idx];
		}

		// Get bitsize of value
		size_t bits() const {
			size_t out = 0;
			uint64_t ms;
			if (number[3]) {
				out = 192;
				ms = number[3];
			} else if (number[2]) {
				out = 128;
				ms = number[2];
			} else if (number[1]) {
				out = 64;
				ms = number[1];
			} else {
				ms = number[0];
			}
			while (ms) {
				ms >>= 1;
				++out;
			}
			return out;
		}

		// Get string representation of value
		template <typename Result = std::string>
		Result str(size_t base = 10) const {
			if (base >= 2 && base <= 36) {
				Result result;
				if (!*this) {
					result.push_back('0');
				} else {
					std::pair<uint256_t, uint256_t> qr(*this, uint256_0());
					do {
						qr = qr.first.divmod(base);
						if (qr.second < 10) {
							result.push_back((uint8_t)qr.second + '0');
						} else {
							result.push_back((uint8_t)qr.second + 'a' - 10);
						}
					} while (qr.first);
				}
				std::reverse(result.begin(), result.end());
				return result;
			} else if (base == 256) {
				char buf[32];
				*(reinterpret_cast<uint64_t*>(buf)) = number[0];
				*(reinterpret_cast<uint64_t*>(buf) + 1) = number[1];
				*(reinterpret_cast<uint64_t*>(buf) + 2) = number[2];
				*(reinterpret_cast<uint64_t*>(buf) + 3) = number[3];
				auto ptr = buf + 32;
				const auto min_buf = buf + 1;
				while (ptr != min_buf && !*--ptr);
				auto length = ptr - buf + 1;
				std::reverse(buf, ptr + 1);
				return Result(buf, buf + length);
			} else {
				throw std::invalid_argument("Base must be in the range [2, 36]");
			}
		}
};

namespace std {  // This is probably not a good idea
	// Make it work with std::string()
	inline std::string to_string(uint256_t& num) {
		return num.str();
	}
	inline const std::string to_string(const uint256_t& num) {
		return num.str();
	}
};

// lhs type T as first arguemnt
// If the output is not a bool, casts to type T

// Bitwise Operators
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator&(const T & lhs, const uint256_t& rhs) {
	return rhs & lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator&=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(rhs & lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator|(const T & lhs, const uint256_t& rhs) {
	return rhs | lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator|=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(rhs | lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator^(const T & lhs, const uint256_t& rhs) {
	return rhs ^ lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator^=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(rhs ^ lhs);
}

// Bitshift operators
inline uint256_t operator<<(const bool     & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const uint8_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const uint16_t & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const uint32_t & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const uint64_t & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const int8_t   & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const int16_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const int32_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}
inline uint256_t operator<<(const int64_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) << rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator<<=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(uint256_t(lhs) << rhs);
}

inline uint256_t operator>>(const bool     & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const uint8_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const uint16_t & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const uint32_t & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const uint64_t & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const int8_t   & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const int16_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const int32_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}
inline uint256_t operator>>(const int64_t  & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) >> rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator>>=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(uint256_t(lhs) >> rhs);
}

// Comparison Operators
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator==(const T & lhs, const uint256_t& rhs) {
	return rhs == lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator!=(const T & lhs, const uint256_t& rhs) {
	return rhs != lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator>(const T & lhs, const uint256_t& rhs) {
	return rhs < lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator<(const T & lhs, const uint256_t& rhs) {
	return rhs > lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator>=(const T & lhs, const uint256_t& rhs) {
	return rhs <= lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator<=(const T & lhs, const uint256_t& rhs) {
	return rhs >= lhs;
}

// Arithmetic Operators
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator+(const T & lhs, const uint256_t& rhs) {
	return rhs + lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator+=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(rhs + lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator-(const T & lhs, const uint256_t& rhs) {
	return -(rhs - lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator-=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(-(rhs - lhs));
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator*(const T & lhs, const uint256_t& rhs) {
	return rhs * lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator*=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(rhs * lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator/(const T & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) / rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator/=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(uint256_t(lhs) / rhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint256_t operator%(const T & lhs, const uint256_t& rhs) {
	return uint256_t(lhs) % rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator%=(T & lhs, const uint256_t& rhs) {
	return lhs = static_cast<T>(uint256_t(lhs) % rhs);
}

// IO Operator
inline std::ostream& operator<<(std::ostream & stream, const uint256_t& rhs) {
	if (stream.flags() & stream.oct) {
		stream << rhs.str(8);
	} else if (stream.flags() & stream.dec) {
		stream << rhs.str(10);
	} else if (stream.flags() & stream.hex) {
		stream << rhs.str(16);
	}
	return stream;
}

#endif
