// Smoke test for the standalone base-x library.
// Build: c++ -std=c++17 -I.. test.cc -o test && ./test
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#define BASEX_PUBLIC_IMPLEMENTATION  // the size arithmetic of the encoder is private
#include "base_x.hh"

using Factory = const BaseX& (*)();

// Every preset the library defines.
static const Factory all_presets[] = {
	Base2::base2, Base2::base2chk, Base8::base8, Base8::base8chk, Base11::base11, Base11::base11chk,
	Base16::base16, Base16::base16chk, Base16::rfc4648, Base32::base32, Base32::base32chk,
	Base32::crockford, Base32::crockfordchk, Base32::rfc4648, Base32::rfc4648hex, Base36::base36,
	Base36::base36chk, Base58::base58, Base58::base58chk, Base58::bitcoin, Base58::bitcoinchk,
	Base58::ripple, Base58::ripplechk, Base58::flickr, Base58::flickrchk, Base59::base59,
	Base59::base59chk, Base59::dubaluchk, Base62::base62, Base62::base62chk, Base62::inverted,
	Base62::invertedchk, Base64::base64, Base64::base64chk, Base64::url, Base64::urlchk,
	Base64::rfc4648, Base64::rfc4648url, Base64::rfc4648url_unpadded, Base66::base66, Base66::base66chk,
};

// Base conversion by byte-wise schoolbook arithmetic over a digit-to-character table, sharing nothing
// with the library. By default the text is the number's alone (one zero digit for no digits, one zero
// byte for no bytes); with `keep` it is the leading_zeros convention: each leading zero byte is a leading
// zero digit and each leading zero digit a zero byte, so no bytes is no text and no text no bytes.
struct Schoolbook {
	std::string alphabet;  // digit -> character
	unsigned base = 0;
	bool keep = false;

	std::string encode(const std::string& bytes) const {
		std::size_t zeros = 0;
		while (keep && zeros < bytes.size() && !bytes[zeros]) ++zeros;
		std::vector<unsigned> digits;  // least significant first
		for (std::size_t i = zeros; i < bytes.size(); ++i) {
			unsigned carry = static_cast<unsigned char>(bytes[i]);
			for (auto& d : digits) {
				carry += d * 256u;
				d = carry % base;
				carry /= base;
			}
			for (; carry; carry /= base) digits.push_back(carry % base);
		}
		std::string out(zeros, alphabet[0]);
		for (auto i = digits.size(); i--;) out.push_back(alphabet[digits[i]]);
		return out.empty() && !keep ? std::string(1, alphabet[0]) : out;
	}
	std::string decode(const std::string& text) const {
		std::size_t zeros = 0;
		while (keep && zeros < text.size() && text[zeros] == alphabet[0]) ++zeros;
		std::vector<unsigned> bytes;  // least significant first
		for (std::size_t i = zeros; i < text.size(); ++i) {
			unsigned carry = static_cast<unsigned>(alphabet.find(text[i]));
			for (auto& b : bytes) {
				carry += b * base;
				b = carry & 255u;
				carry >>= 8;
			}
			for (; carry; carry >>= 8) bytes.push_back(carry & 255u);
		}
		std::string out(zeros, '\0');
		for (auto i = bytes.size(); i--;) out.push_back(static_cast<char>(bytes[i]));
		return out.empty() && !keep ? std::string(1, '\0') : out;
	}
};

int main() {
	// base58 round-trip on an ASCII payload.
	{
		auto encoded = Base58::base58().encode("Hello world!");
		auto decoded = Base58::base58().decode(encoded);
		assert(decoded == "Hello world!");
	}

	// base62 round-trip plus a known vector (from the gtest suite).
	{
		auto encoded = Base62::base62().encode("Hello world!");
		assert(encoded == "T8dgcjRGuYUueWht");
		auto decoded = Base62::base62().decode(encoded);
		assert(decoded == "Hello world!");
	}

	// base16 is straightforward hex of the bytes.
	{
		assert(Base16::base16().encode("ABC") == "414243");
		assert(Base16::base16().decode("414243") == "ABC");
	}

	// Checksum variants append a check character and validate it.
	{
		auto chk = Base62::base62chk().encode("Hello world!");
		assert(chk == "T8dgcjRGuYUueWhtE");
		assert(Base62::base62chk().decode(chk) == "Hello world!");
		assert(Base62::base62chk().is_valid("T8dgcjRGuYUueWhtE") == true);
		assert(Base62::base62chk().is_valid("Some random text!") == false);
	}

	// base64 round-trip, including the checksum flavor.
	{
		assert(Base64::base64().encode("Hello world!") == "SGVsbG8gd29ybGQh");
		assert(Base64::base64().decode("SGVsbG8gd29ybGQh") == "Hello world!");
	}

	// Crockford base32 encodes integers too (FVCK is the classic vector).
	{
		assert(Base32::crockford().encode(519571) == "FVCK");
	}

	// Round-trip a binary payload (a UUID's worth of bytes) across alphabets.
	{
		std::string payload("\330\105\140\310\23\117\21\346\241\342\64\66\73\322\155\256");
		for (const BaseX& enc : {Base58::base58(), Base58::bitcoin(), Base62::base62()}) {
			auto encoded = enc.encode(payload);
			auto decoded = enc.decode(encoded);
			assert(decoded == payload);
		}
	}

	// RFC 4648 vectors, padded and block-aligned, through the bit-packing decode.
	{
		assert(Base64::rfc4648().encode("foobar") == "Zm9vYmFy");
		assert(Base64::rfc4648().encode("fooba") == "Zm9vYmE=");
		assert(Base64::rfc4648().decode("Zm9vYmE=") == "fooba");
		assert(Base32::rfc4648().encode("foobar") == "MZXW6YTBOI======");
		assert(Base32::rfc4648().decode("MZXW6YTBOI======") == "foobar");

		// RFC 4648 at 32 bytes and up, and with a zero leading digit (vectors from Python's base64).
		const std::string one("\x01", 1);
		assert(Base64::rfc4648().encode(one) == "AQ==" && Base64::rfc4648().decode("AQ==") == one);
		assert(Base32::rfc4648().encode(one) == "AE======" && Base32::rfc4648().decode("AE======") == one);
		std::string b33("\x03", 1), b48;
		for (int i = 100; i < 132; ++i) b33.push_back(static_cast<char>(i));
		for (int i = 1; i < 49; ++i) b48.push_back(static_cast<char>(i));
		assert(Base64::rfc4648().encode(b33) == "A2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKD");
		assert(Base32::rfc4648().encode(b33) == "ANSGKZTHNBUWU23MNVXG64DROJZXI5LWO54HS6T3PR6X474AQGBIG===");
		assert(Base64::rfc4648().encode(b48) == "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8w");
		assert(Base32::rfc4648().encode(b48) == "AEBAGBAFAYDQQCIKBMGA2DQPCAIREEYUCULBOGAZDINRYHI6D4QCCIRDEQSSMJZIFEVCWLBNFYXTA===");
		for (const std::string* in : {&b33, &b48}) {
			assert(Base64::rfc4648().decode(Base64::rfc4648().encode(*in)) == *in);
			assert(Base32::rfc4648().decode(Base32::rfc4648().encode(*in)) == *in);
			assert(Base32::rfc4648hex().decode(Base32::rfc4648hex().encode(*in)) == *in);
			assert(Base64::rfc4648url().decode(Base64::rfc4648url().encode(*in)) == *in);
		}
	}

	// W16: the RFC 4648 presets are byte-exact, as Python's base64 module is: a leading zero byte is a
	// leading zero digit whatever the limb alignment, and no bytes are no text. Vectors from Python
	// (b16encode, b32encode, b32hexencode, b64encode, urlsafe_b64encode, urlsafe_b64encode without the '=').
	{
		auto from_hex = [](const std::string& hex) {
			std::string bytes;
			for (std::size_t i = 0; i < hex.size(); i += 2) bytes.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
			return bytes;
		};
		const BaseX* const presets[] = {&Base16::rfc4648(), &Base32::rfc4648(), &Base32::rfc4648hex(), &Base64::rfc4648(),
		                                &Base64::rfc4648url(), &Base64::rfc4648url_unpadded()};
		struct Vector { const char* hex; const char* text[6]; };
		static const Vector vectors[] = {
			{"", {"", "", "", "", "", ""}},
			{"00", {"00", "AA======", "00======", "AA==", "AA==", "AA"}},
			{"0000", {"0000", "AAAA====", "0000====", "AAA=", "AAA=", "AAA"}},
			{"000000", {"000000", "AAAAA===", "00000===", "AAAA", "AAAA", "AAAA"}},
			{"00000000", {"00000000", "AAAAAAA=", "0000000=", "AAAAAA==", "AAAAAA==", "AAAAAA"}},
			{"0001", {"0001", "AAAQ====", "000G====", "AAE=", "AAE=", "AAE"}},
			{"0000ff", {"0000FF", "AAAP6===", "000FU===", "AAD/", "AAD_", "AAD_"}},
			{"00ff00", {"00FF00", "AD7QA===", "03VG0===", "AP8A", "AP8A", "AP8A"}},
			{"0f", {"0F", "B4======", "1S======", "Dw==", "Dw==", "Dw"}},
			{"ff", {"FF", "74======", "VS======", "/w==", "_w==", "_w"}},
			// z zero bytes then 0x01, around the limb size: the output used to keep 0 or 8 of them.
			{"00000000000001", {"00000000000001", "AAAAAAAAAAAQ====", "00000000000G====", "AAAAAAAAAQ==", "AAAAAAAAAQ==", "AAAAAAAAAQ"}},
			{"0000000000000001", {"0000000000000001", "AAAAAAAAAAAAC===", "0000000000002===", "AAAAAAAAAAE=", "AAAAAAAAAAE=", "AAAAAAAAAAE"}},
			{"000000000000000001", {"000000000000000001", "AAAAAAAAAAAAAAI=", "000000000000008=", "AAAAAAAAAAAB", "AAAAAAAAAAAB", "AAAAAAAAAAAB"}},
			{"00000000000000000001", {"00000000000000000001", "AAAAAAAAAAAAAAAB", "0000000000000001", "AAAAAAAAAAAAAQ==", "AAAAAAAAAAAAAQ==", "AAAAAAAAAAAAAQ"}},
			{"0000000000000000000000000000000001020304", {"0000000000000000000000000000000001020304", "AAAAAAAAAAAAAAAAAAAAAAAAAAAQEAYE", "000000000000000000000000000G40O4", "AAAAAAAAAAAAAAAAAAAAAAECAwQ=", "AAAAAAAAAAAAAAAAAAAAAAECAwQ=", "AAAAAAAAAAAAAAAAAAAAAAECAwQ"}},
			{"000000000000000000000000000000000000000000000000000000000000000000", {"000000000000000000000000000000000000000000000000000000000000000000", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA===", "00000000000000000000000000000000000000000000000000000===", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}},
		};
		for (const Vector& v : vectors) {
			const std::string bytes = from_hex(v.hex);
			for (int i = 0; i < 6; ++i) {
				assert(presets[i]->encode(bytes) == v.text[i]);
				assert(presets[i]->decode(v.text[i]) == bytes);
			}
		}

		// The same by construction: bits in groups of the digit width, the last group filled with zeros,
		// then '=' to whole blocks; every count of leading zero bytes, the all-zero inputs included.
		struct Spec { const BaseX& enc; const char* alphabet; unsigned bits; unsigned block; };
		const Spec specs[] = {
			{Base16::rfc4648(), "0123456789ABCDEF", 4, 1},
			{Base32::rfc4648(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", 5, 8},
			{Base32::rfc4648hex(), "0123456789ABCDEFGHIJKLMNOPQRSTUV", 5, 8},
			{Base64::rfc4648(), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", 6, 4},
			{Base64::rfc4648url(), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", 6, 4},
			{Base64::rfc4648url_unpadded(), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", 6, 1},
		};
		auto model = [](const Spec& spec, const std::string& bytes) {
			std::string bits, text;
			for (unsigned char c : bytes) {
				for (int i = 7; i >= 0; --i) bits.push_back(static_cast<char>('0' + ((c >> i) & 1)));
			}
			while (bits.size() % spec.bits) bits.push_back('0');
			for (std::size_t i = 0; i < bits.size(); i += spec.bits) {
				unsigned digit = 0;
				for (unsigned j = 0; j < spec.bits; ++j) digit = digit * 2 + static_cast<unsigned>(bits[i + j] - '0');
				text.push_back(spec.alphabet[digit]);
			}
			while (text.size() % spec.block) text.push_back('=');
			return text;
		};
		std::uint32_t seed = 4242;
		for (const Spec& spec : specs) {
			for (std::size_t zeros = 0; zeros <= 20; ++zeros) {
				for (std::size_t tail = 0; tail <= 21; ++tail) {
					std::string bytes(zeros, '\0');
					for (std::size_t i = 0; i < tail; ++i) {
						seed = seed * 1664525u + 1013904223u;
						bytes.push_back(i % 5 == 4 ? '\0' : static_cast<char>(seed >> 16));
					}
					const std::string text = model(spec, bytes);
					assert(spec.enc.encode(bytes) == text);
					assert(spec.enc.decode(text) == bytes);
					assert(spec.enc.is_valid(text));
				}
			}
		}

		// The number overload writes the shortest byte string, one byte for zero; the integer decode
		// reads the number, so the leading zero bytes carry no value there.
		assert(Base64::rfc4648().encode(uinteger_t(0)) == "AA==");
		assert(Base64::rfc4648().encode(uinteger_t(1)) == "AQ==");
		assert(Base16::rfc4648().encode(uinteger_t(255)) == "FF");
		assert(Base16::rfc4648().encode(uinteger_t(256)) == "0100");
		assert(Base32::rfc4648().encode(uinteger_t(0)) == "AA======");
		assert(Base64::rfc4648().decode<unsigned>("AAAB") == 1);
		assert(Base16::rfc4648().decode<unsigned>("0001") == 1);
		uinteger_t number(9);
		Base64::rfc4648().decode(number, "AAAB", 4);
		assert(number == 1);

		// Appending keeps what is there and pads the appended text alone; decoding into a Result that
		// holds something replaces it, a string or a vector, with every byte of the text.
		std::string out = "xy";
		Base64::rfc4648().encode(out, std::string_view("\0\0", 2));
		assert(out == "xyAAA=");
		Base16::rfc4648().encode(out, std::string_view("\0\x0a", 2));
		assert(out == "xyAAA=000A");
		out = "stale";
		Base64::rfc4648().decode(out, "AAAB", 4);
		assert(out == std::string("\0\0\x01", 3));
		std::vector<unsigned char> bytes(5, 9);
		Base16::rfc4648().decode(bytes, "00FF", 4);
		assert((bytes == std::vector<unsigned char>{0, 255}));
		out = "stale";
		Base64::rfc4648().decode(out, "", 0);
		assert(out.empty());

		// Digits past the last whole byte are dropped: the dangling bits of base64 (as ever), and the
		// last digit of an odd count of hex digits.
		assert(Base64::rfc4648().decode("YWJ") == "ab");
		assert(Base64::rfc4648().decode("Q").empty());
		assert(Base16::rfc4648().decode("ABC") == "\xab");
		assert(Base16::rfc4648().decode("A").empty());
	}

	// A1: a parsed byte string of 8 or more zero bytes then 0x01 is the number 1. It once kept a zero limb
	// (it compared unequal to 1, had 65 bits, and sized an RFC 4648 text from them), and base58 never showed
	// it. The numeric alphabets write 1 whatever the zero count, the leading_zeros presets write a zero digit for
	// each zero byte before the 1, and the RFC presets write every byte (Python).
	{
		for (std::size_t zeros = 0; zeros <= 20; ++zeros) {
			std::string bytes(zeros, '\0');
			bytes.push_back('\x01');
			const uinteger_t value(bytes.data(), bytes.size(), 256);
			assert(value == 1 && !(value > 1) && value.bits() == 1);
			assert(Base58::bitcoin().encode(bytes) == std::string(zeros, '1') + "2" && Base58::bitcoin().encode(value) == "2");
			assert(Base58::base58().encode(bytes) == "1" && Base62::base62().encode(bytes) == "1");
			assert(Base16::base16().encode(bytes) == "1" && Base64::base64().encode(bytes) == "B");
			assert(Base64::rfc4648().encode(value) == "AQ==" && Base16::rfc4648().encode(value) == "01");
		}
		struct Case { std::size_t zeros; const char* b64; const char* b32; const char* b16; };
		static const Case cases[] = {
			{8, "AAAAAAAAAAAB", "AAAAAAAAAAAAAAI=", "000000000000000001"},
			{15, "AAAAAAAAAAAAAAAAAAAAAQ==", "AAAAAAAAAAAAAAAAAAAAAAAAAE======", "00000000000000000000000000000001"},
			{16, "AAAAAAAAAAAAAAAAAAAAAAE=", "AAAAAAAAAAAAAAAAAAAAAAAAAAAQ====", "0000000000000000000000000000000001"},
			{17, "AAAAAAAAAAAAAAAAAAAAAAAB", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAC===", "000000000000000000000000000000000001"},
		};
		for (const Case& c : cases) {
			std::string bytes(c.zeros, '\0');
			bytes.push_back('\x01');
			assert(Base64::rfc4648().encode(bytes) == c.b64 && Base64::rfc4648().decode(c.b64) == bytes);
			assert(Base32::rfc4648().encode(bytes) == c.b32 && Base32::rfc4648().decode(c.b32) == bytes);
			assert(Base16::rfc4648().encode(bytes) == c.b16 && Base16::rfc4648().decode(c.b16) == bytes);
		}
	}

	// W16: the digit count of nbytes bytes is worked out in size_t, where 8 nbytes wraps at 512 MiB on a
	// 32-bit one and gave wrong text without an error. block_digits throws std::length_error for what does
	// not fit its Size, and encode runs it at size_t; that guard cannot be reached at a 64-bit size_t, so it
	// runs here at 16 and 32 bits against the same arithmetic in unsigned long long, around the limit and below it.
	{
		auto check = [](auto size_tag, unsigned long long nbytes, unsigned bits) {
			using Size = decltype(size_tag);
			const unsigned long long max = std::numeric_limits<Size>::max();
			Size want = 7, bp = 7;
			bool threw = false;
			try {
				BaseX::block_digits<Size>(static_cast<Size>(nbytes), bits, want, bp);
			} catch (const std::length_error&) {
				threw = true;
			}
			assert(threw == (8 * nbytes + (bits - 1) > max));
			if (!threw) {
				const unsigned long long digits = (8 * nbytes + bits - 1) / bits;
				assert(want == digits && bp == digits * bits - 8 * nbytes && bp < bits);
			}
		};
		auto check_bytes = [](auto size_tag, unsigned long long bits) {
			using Size = decltype(size_tag);
			assert(BaseX::block_bytes<Size>(static_cast<Size>(bits)) == (bits ? bits / 8 + (bits % 8 != 0) : 1));
		};
		for (unsigned bits = 1; bits <= 8; ++bits) {
			for (unsigned long long n = 0; n <= 65535; ++n) check(std::uint16_t(), n, bits);
			for (unsigned long long n = 0; n <= 300; ++n) check(std::uint32_t(), n, bits);
			for (unsigned long long n = (1ull << 29) - 24; n <= (1ull << 29) + 24; ++n) check(std::uint32_t(), n, bits);
			for (unsigned long long n = 0xffffffffull - 24; n <= 0xffffffffull; ++n) check(std::uint32_t(), n, bits);
			for (unsigned long long n = 0; n <= 100; ++n) check(std::size_t(), n, bits);
		}
		std::uint32_t want32 = 0, bp32 = 0;
		bool threw = false;
		try { BaseX::block_digits<std::uint32_t>(1u << 29, 6, want32, bp32); } catch (const std::length_error&) { threw = true; }
		assert(threw);  // 2^29 bytes are 2^32 bits
		BaseX::block_digits<std::uint32_t>((1u << 29) - 1, 6, want32, bp32);
		assert(want32 == 715827882 && bp32 == 4);  // 2^32 - 8 bits in 6-bit digits: 715827881 and 2 bits over, so 715827882 and 4 to fill
		std::size_t want = 0, bp = 0;
		BaseX::block_digits<std::size_t>(1000, 6, want, bp);
		assert(want == 1334 && bp == 4);  // 8000 bits in 6-bit digits: 1333 and 2 bits over, so 1334 and 4 to fill
		for (unsigned long long bits = 0; bits <= 65535; ++bits) check_bytes(std::uint16_t(), bits);
		for (unsigned long long bits = 0xffffffffull - 70; bits <= 0xffffffffull; ++bits) check_bytes(std::uint32_t(), bits);
		assert(BaseX::block_bytes<std::uint32_t>(0xffffffffu) == (1u << 29));  // (bits + 7) / 8 wraps to 0
	}

	// W16: decode reads nothing but padding and ignored characters after the padding, as is_valid does;
	// a digit or any other character there throws, and the result is left as it was.
	{
		const BaseX& b64 = Base64::rfc4648();
		assert(b64.decode("YWI=") == "ab");
		for (const char* good : {"YWI=", "YWI==", "YWI=\n", "YQ==\r\n", "YQ=\n=", "YQ=\n=\n", "YQ=\n\r=\n"}) {
			assert(b64.is_valid(good));
			assert(b64.decode(good) == std::string(good[1] == 'Q' ? "a" : "ab"));
		}
		for (const char* bad : {"YWI=junk", "YWI=Y", "YQ==Y", "YQ=\n=Y", "YWI==x", "YQ==\x01", "YQ==\xff", "YQ=-=", "Y=Q=", "YQ=a="}) {
			assert(!b64.is_valid(bad));
			bool threw = false;
			try { b64.decode(bad); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw);
			uinteger_t value(7);
			threw = false;
			try { b64.decode(value, bad, std::strlen(bad)); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw && value == 7);
			std::string out = "keep";
			threw = false;
			try { b64.decode(out, bad, std::strlen(bad)); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw && out == "keep");
			threw = false;
			try { b64.decode<unsigned>(bad); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw);
		}
		// The message names the first character that does not belong, and where it is.
		auto message = [](const BaseX& enc, const std::string& text) {
			try { enc.decode(text); } catch (const std::invalid_argument& e) { return std::string(e.what()); }
			return std::string("no error");
		};
		assert(message(b64, "YWI=junk") == "Error: Invalid character: 'j' at 4");
		assert(message(b64, "YQ==\n=Y") == "Error: Invalid character: 'Y' at 6");
		assert(message(Base32::rfc4648(), "MY======X") == "Error: Invalid character: 'X' at 8");
		assert(message(Base32::rfc4648hex(), "CO======7") == "Error: Invalid character: '7' at 8");
		assert(message(Base64::rfc4648url(), "YQ==_") == "Error: Invalid character: '_' at 4");
		// A character that is not a digit is not padding either, in an alphabet without any.
		assert(message(Base64::rfc4648url_unpadded(), "YQ=") == "Error: Invalid character: '=' at 2");
		assert(message(Base16::rfc4648(), "41=") == "Error: Invalid character: '=' at 2");

		// A check character after the padding is read after every padding and ignored character before it.
		const BaseX custom(BaseX::with_check, "01234567", "89", "=", "\n");
		const std::string checked = custom.encode(std::string_view("a"));
		const std::string padded = checked.substr(0, checked.size() - 1) + "=\n=" + checked.back();
		assert(custom.is_valid(padded) && custom.decode(padded) == "a");
	}

	// A large payload round-trips in every family; bitwise bases decode in one pass.
	{
		std::string big(4096, '\0');
		for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<char>(i * 131 + (i >> 5) + 1);
		big[0] = '\x7b';
		for (const BaseX& enc : {Base16::base16(), Base32::base32(), Base64::base64(), Base64::url(), Base58::base58(), Base62::base62chk(),
		                         Base32::rfc4648(), Base32::rfc4648hex(), Base64::rfc4648(), Base64::rfc4648url()}) {
			assert(enc.decode(enc.encode(big)) == big);
		}
	}

	// A character outside the alphabet throws; so does a corrupted checksum.
	{
		bool threw = false;
		try { Base64::base64().decode("SGVsbG8*d29ybGQh"); } catch (const std::invalid_argument&) { threw = true; }
		assert(threw);
		threw = false;
		try { Base62::base62chk().decode("T8dgcjRGuYUueWhtF"); } catch (const std::invalid_argument&) { threw = true; }
		assert(threw);
		// A byte >= 0x80 is not in any alphabet (and must not index the table below its start).
		for (const BaseX& enc : {Base64::base64(), Base58::base58(), Base16::base16(), Base62::base62chk()}) {
			for (unsigned char byte : {0x80, 0xff}) {
				std::string text = "SGVs";
				text.push_back(static_cast<char>(byte));
				text += "bG8";
				threw = false;
				try { enc.decode(text); } catch (const std::invalid_argument&) { threw = true; }
				assert(threw);
			}
		}
	}

	// Input shorter than the check characters throws std::invalid_argument; it must not
	// try to allocate a wrapped length.
	{
		const std::string empty;
		for (const BaseX& enc : {Base62::base62chk(), Base32::crockfordchk(), Base64::base64chk()}) {
			bool threw = false;
			try { enc.decode(empty); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw);
			threw = false;
			try { enc.decode<std::uint64_t>(empty); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw);
		}
	}

	// Decode lengths around the 256-character stack buffer, with and without ignored
	// characters, round-tripped. Crockford ignores '-'.
	{
		struct Case { const BaseX& enc; int base; bool ignores_dash; };
		for (const Case& c : {Case{Base32::crockford(), 32, true}, Case{Base58::base58(), 58, false}, Case{Base64::base64(), 64, false}, Case{Base16::base16(), 16, false}}) {
			std::string alphabet;
			for (int v = 0; v < c.base; ++v) alphabet += c.enc.encode(uinteger_t(v));
			for (std::size_t length : {255u, 256u, 257u, 258u, 600u}) {
				for (bool with_ignored : {false, true}) {
					if (with_ignored && !c.ignores_dash) continue;
					std::string text, digits;
					for (std::size_t i = 0; i < length; ++i) {
						if (with_ignored && i % 5 == 4 && i + 1 < length) {
							text.push_back('-');
						} else {
							char ch = alphabet[(i == 0 ? 1 : (i * 7 + 3)) % c.base];
							text.push_back(ch);
							digits.push_back(ch);
						}
					}
					assert(text.size() == length);
					assert(c.enc.decode(text) == c.enc.decode(digits));
					assert(c.enc.encode(c.enc.decode(digits)) == digits);
				}
			}
		}
	}

	// W9 F3: encode into a non-empty result appends exactly what a fresh encode returns. The
	// padding and the checksum cover the appended text, not the whole buffer, at every prefix
	// length (the padding repeats every 4 or 8 characters).
	{
		assert(Base64::rfc4648().encode("a") == "YQ==");
		std::string out = "xy";
		Base64::rfc4648().encode(out, std::string_view("a"));
		assert(out == "xyYQ==");
		out = "xy";
		Base58::bitcoinchk().encode(out, std::string_view("\x21", 1));
		assert(out == "xy" + Base58::bitcoinchk().encode(std::string_view("\x21", 1)));
		assert(out == "xyaR");  // the fresh text "aR" carries its own checksum
		assert(Base58::bitcoinchk().decode(out.substr(2)) == "\x21");

		for (const BaseX& enc : {Base2::base2chk(), Base16::base16chk(), Base32::base32chk(), Base36::base36chk(),
		                         Base58::bitcoinchk(), Base62::base62chk(), Base64::base64chk(), Base66::base66chk(),
		                         Base32::crockfordchk(), Base32::rfc4648(), Base32::rfc4648hex(), Base64::rfc4648(),
		                         Base64::rfc4648url()}) {
			for (std::size_t length = 1; length <= 24; ++length) {
				std::string payload;
				for (std::size_t i = 0; i < length; ++i) payload.push_back(static_cast<char>((i + 1) * 37 + length * 11));
				payload[0] = static_cast<char>(1 + length * 7 % 250);
				const std::string fresh = enc.encode(payload);
				for (std::size_t prefix = 0; prefix <= 9; ++prefix) {
					std::string text(prefix, 'x');
					enc.encode(text, payload.data(), payload.size());
					assert(text == std::string(prefix, 'x') + fresh);
					assert(enc.decode(text.substr(prefix)) == payload);
				}
			}
		}
	}

	// W9 F6: a byte outside the table is not a check character. It shared the ordinal of the first
	// extended character, so "10?" read as "10*" (and decoded as 32) whenever the check value was 32.
	{
		const BaseX& enc = Base32::crockfordchk();
		assert(enc.encode(32) == "10*");
		for (int byte = 0; byte < 256; ++byte) {
			const std::string text = std::string("10") + static_cast<char>(byte);
			bool decoded = true;
			try { decoded = enc.decode<unsigned>(text) == 32; } catch (const std::invalid_argument&) { decoded = false; }
			assert(decoded == (byte == '*'));
		}
	}

	// W9 F5: a NUL byte is an invalid character, not the end of the text, in alphabets without
	// padding (the padding character was '\0' there, so decode stopped at the NUL and ignored the rest).
	{
		const std::string nul("ab\0cd", 5);
		bool threw = false;
		try { Base62::base62().decode(nul); } catch (const std::invalid_argument&) { threw = true; }
		assert(threw);
		threw = false;
		try { Base62::base62().decode<unsigned long long>(nul); } catch (const std::invalid_argument&) { threw = true; }
		assert(threw);
		assert(!Base62::base62().is_valid(nul));
		for (const BaseX& enc : {Base2::base2(), Base8::base8(), Base16::base16(), Base32::crockford(), Base32::crockfordchk(),
		                         Base36::base36chk(), Base58::bitcoin(), Base58::bitcoinchk(), Base62::base62(), Base64::base64(),
		                         Base64::base64chk(), Base66::base66(), Base64::rfc4648url_unpadded(), Base64::rfc4648(), Base32::rfc4648()}) {
			const std::string good = enc.encode(std::string_view("Hello world!"));
			for (std::size_t at : {std::size_t(0), good.size() / 2, good.size()}) {
				std::string text = good;
				text.insert(at, 1, '\0');
				assert(!enc.is_valid(text));
				uinteger_t value(7);
				threw = false;
				try { enc.decode(value, text.data(), text.size()); } catch (const std::invalid_argument&) { threw = true; }
				assert(threw);
				if (at < good.size()) assert(value == 7);  // left as it was (a bad check character throws after the assignment)
			}
		}
	}

	// W9 F4: is_valid accepts what encode writes, padding and the extended characters a check
	// character can be, and still rejects those anywhere else.
	{
		assert(Base64::rfc4648().is_valid("YWI="));
		assert(Base64::rfc4648().is_valid("YQ=="));
		assert(Base64::rfc4648().is_valid("YQ==\n"));  // ignored characters may follow the padding
		assert(!Base64::rfc4648().is_valid("YQ==Y"));  // a digit may not
		assert(!Base64::rfc4648().is_valid("Y=Q="));
		assert(!Base64::base64().is_valid("YQ=="));  // no padding in this alphabet

		// Values 32..36 have a check character from the extended set: 10* 11~ 12$ ...
		assert(Base32::crockfordchk().encode(32) == "10*");
		assert(Base32::crockfordchk().is_valid("10*") && Base32::crockfordchk().decode<unsigned>("10*") == 32);
		assert(!Base32::crockfordchk().is_valid("1U0"));  // an extended character is not a digit
		assert(!Base32::crockfordchk().is_valid(""));  // too short for its check character, as for decode
		{
			// The last character of a check alphabet is a digit, a translated digit or an extended character,
			// in either case; nothing else (an ignored character, or a byte outside the table).
			const std::string valid_last = "0123456789ABCDEFGHJKMNPQRSTVWXYZ*~$=UOIL";
			for (int byte = 0; byte < 256; ++byte) {
				const std::string text = std::string("10") + static_cast<char>(byte);
				const bool last_is_valid = byte < 128 && valid_last.find(static_cast<char>(std::toupper(byte))) != std::string::npos;
				assert(Base32::crockfordchk().is_valid(text) == last_is_valid);
			}
		}
		for (unsigned v = 1; v < 1600; ++v) {
			const auto text = Base32::crockfordchk().encode(v);
			assert(Base32::crockfordchk().is_valid(text));
			assert(Base32::crockfordchk().decode<unsigned>(text) == v);
		}

		// A checksum is read from the last character, even where that character would be ignored.
		{
			const auto text = Base59::base59chk().encode(std::string_view("Hello world!"));
			assert(Base59::base59chk().is_valid(text));
			assert(!Base59::base59chk().is_valid(text + "l"));
			bool threw = false;
			try { Base59::base59chk().decode(text + "l"); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw);
		}

		// Every preset: is_valid accepts the text encode writes, and agrees with decode on that text
		// with one character replaced, inserted, deleted or cut off (exactly; the check alphabet only
		// accepts what decode accepts, since is_valid does not recompute the check).
		std::string pool;
		for (int c = 32; c < 127; ++c) pool.push_back(static_cast<char>(c));
		pool += std::string("\0\n\r\x80\xff", 5);
		for (Factory make : all_presets) {
			const BaseX& enc = make();
			const bool checked = &enc == &Base32::crockfordchk();
			auto agree = [&](const std::string& text) {
				bool accepted = true;
				try { enc.decode(text); } catch (const std::invalid_argument&) { accepted = false; }
				const bool valid = enc.is_valid(text);
				if (checked) assert(!accepted || valid);
				else assert(valid == accepted);
			};
			for (std::size_t length : {1u, 5u, 11u}) {
				std::string payload;
				for (std::size_t i = 0; i < length; ++i) payload.push_back(static_cast<char>((i + 1) * 53 + length * 3));
				payload[0] = static_cast<char>(1 + length * 11);
				const std::string text = enc.encode(payload);
				assert(enc.is_valid(text));
				agree(text);
				for (std::size_t i = 0; i < text.size(); ++i) {
					agree(text.substr(0, i));
					agree(text.substr(0, i) + text.substr(i + 1));
					for (char c : pool) {
						std::string replaced = text;
						replaced[i] = c;
						agree(replaced);
						agree(text.substr(0, i) + c + text.substr(i));
					}
				}
			}
		}
	}

	// W12: the codec against a model with nothing in common with it: byte-wise schoolbook base
	// conversion over a digit-to-character table read back through encode of one-digit values. The
	// plain presets (no padding, check or checksum), texts of every length around 16 characters
	// (the table-lookup block) and a bad character at every position of them. This runs with and
	// without BASEX_NO_SIMD in CTest, so the vector and the scalar paths answer to the same model.
	{
		std::uint32_t seed = 12345;
		auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
		const Factory plain[] = {Base2::base2, Base8::base8, Base11::base11, Base16::base16, Base32::base32,
		                         Base32::crockford, Base36::base36, Base58::base58, Base58::bitcoin, Base58::ripple, Base58::flickr,
		                         Base59::base59, Base62::base62, Base62::inverted, Base64::base64, Base64::url, Base66::base66};
		for (Factory make : plain) {
			const BaseX& enc = make();
			Schoolbook model;
			model.keep = &enc == &Base58::bitcoin() || &enc == &Base58::ripple();  // the leading_zeros presets
			for (unsigned v = 0;; ++v) {
				const std::string one = enc.encode(uinteger_t(v));
				if (v && one.size() > 1) {
					model.base = v;
					break;
				}
				model.alphabet += one;
			}
			// A character that is neither a digit nor ignored, whichever the alphabet.
			std::string bad;
			for (char c : {'\x01', '\x7f', '\x80', '\xff', '\x1b'}) {
				if (model.alphabet.find(c) == std::string::npos) bad.push_back(c);
			}
			assert(bad.size() >= 3);
			for (std::size_t length = 1; length <= 70; ++length) {
				for (int shape = 0; shape < 4; ++shape) {
					std::string bytes(length, '\0');
					for (auto& b : bytes) b = static_cast<char>(next());
					if (shape == 0) bytes[0] = static_cast<char>(1 + next() % 255);
					if (shape == 1) { for (std::size_t i = 0; i < length && i < 9; ++i) bytes[i] = '\0'; }  // leading zero bytes
					if (shape == 2) bytes.assign(length, '\xff');
					const std::string text = enc.encode(bytes);
					assert(text == model.encode(bytes));
					const std::string value = model.keep ? bytes : bytes.substr(std::min(bytes.find_first_not_of('\0'), bytes.size() - 1));
					assert(enc.decode(text) == model.decode(text));
					assert(enc.decode(text) == value);
					// A digit-only text with leading zero digits (the scan converts them like any other).
					std::string zeros = std::string(1 + next() % 20, model.alphabet[0]) + text;
					assert(enc.decode(zeros) == model.decode(zeros));
					for (std::size_t at = 0; at < text.size(); ++at) {
						const char c = bad[next() % bad.size()];
						std::string broken = text;
						broken[at] = c;
						std::string what;
						try { enc.decode(broken); } catch (const std::invalid_argument& e) { what = e.what(); }
						assert(what == std::string("Error: Invalid character: '") + c + "' at " + std::to_string(at));
						assert(!enc.is_valid(broken));
					}
				}
			}
		}
	}

	// Decoding into a Result replaces what it holds, for an empty one and a full one, a string and
	// a vector, and leaves it alone when the text is bad.
	{
		const std::string payload = "Hello world! Hello world! Hello world!";
		for (const BaseX& enc : {Base58::bitcoin(), Base62::base62(), Base64::base64(), Base16::base16(), Base58::bitcoinchk()}) {
			const std::string text = enc.encode(payload);
			std::string out;
			enc.decode(out, text.data(), text.size());
			assert(out == payload);
			out = "stale text that is not empty";
			enc.decode(out, text.data(), text.size());
			assert(out == payload);
			std::vector<unsigned char> bytes(3, 7);
			enc.decode(bytes, text.data(), text.size());
			assert(std::string(bytes.begin(), bytes.end()) == payload);
			out = "keep";
			const std::string bad = text.substr(0, text.size() / 2) + '\x01' + text.substr(text.size() / 2);
			bool threw = false;
			try { enc.decode(out, bad.data(), bad.size()); } catch (const std::invalid_argument&) { threw = true; }
			assert(threw && out == "keep");
		}
	}

	// LZ: Bitcoin's base58 is the bytes as one big number plus a leading zero digit for each leading zero
	// byte, and back. Base58::bitcoin() and Base58::ripple() have the leading_zeros flag; no other preset does.
	// The vectors are Bitcoin Core's base58_encode_decode.json (MIT) and the XRP Ledger's addresses, which a
	// reference written from the definition (no base-x code) agrees with.
	{
		auto from_hex = [](const char* hex) {
			std::string bytes;
			for (; hex[0] && hex[1]; hex += 2) bytes.push_back(static_cast<char>(std::stoi(std::string(hex, 2), nullptr, 16)));
			return bytes;
		};
		auto throws = [](auto&& call) {
			try { call(); } catch (const std::invalid_argument&) { return true; }
			return false;
		};
		const std::string btc_alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
		const std::string xrp_alphabet = "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz";
		const BaseX& btc = Base58::bitcoin();
		const BaseX& xrp = Base58::ripple();

		// Which presets keep leading zero bytes as zero digits: the two, and not the numeric ones. (The RFC 4648
		// presets keep them too, as a block of bits: they are told apart by their factories.)
		auto is_block = [](Factory make) {
			return make == Base16::rfc4648 || make == Base32::rfc4648 || make == Base32::rfc4648hex ||
			       make == Base64::rfc4648 || make == Base64::rfc4648url || make == Base64::rfc4648url_unpadded;
		};
		auto keeps_zeros = [](const BaseX& enc) { return enc.encode(std::string_view("\0\x01", 2)) != enc.encode(std::string_view("\x01", 1)); };
		for (Factory make : all_presets) {
			if (!is_block(make)) assert(keeps_zeros(make()) == (make == Base58::bitcoin || make == Base58::ripple));
		}
		struct Vector { const char* hex; const char* text; };
		static const Vector core[] = {
			{"", ""},
			{"61", "2g"},
			{"626262", "a3gV"},
			{"636363", "aPEr"},
			{"73696d706c792061206c6f6e6720737472696e67", "2cFupjhnEsSn59qHXstmK2ffpLv2"},
			{"00eb15231dfceb60925886b67d065299925915aeb172c06647", "1NS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L"},
			{"516b6fcd0f", "ABnLTmg"},
			{"bf4f89001e670274dd", "3SEo3LWLoPntC"},
			{"572e4794", "3EFU7m"},
			{"ecac89cad93923c02321", "EJDM8drfXA6uyA"},
			{"10c8511e", "Rt5zm"},
			{"00000000000000000000", "1111111111"},
			{"000111d38e5fc9071ffcd20b4a763cc9ae4f252bb4e48fd66a835e252ada93ff480d6dd43dc62a641155a5",
			 "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"},
		};
		for (const Vector& v : core) {
			const std::string bytes = from_hex(v.hex);
			assert(btc.encode(bytes) == v.text);
			assert(btc.decode(v.text) == bytes);
			assert(btc.is_valid(v.text));
			// Ripple's alphabet is a permutation of Bitcoin's: the text is Bitcoin's, a character at a time.
			std::string mapped;
			for (const char* c = v.text; *c; ++c) mapped.push_back(xrp_alphabet[btc_alphabet.find(*c)]);
			assert(xrp.encode(bytes) == mapped && xrp.decode(mapped) == bytes && xrp.is_valid(mapped));
		}
		// XRPL addresses: a type byte and the account id, then four checksum bytes. The account id of all
		// zeros is twenty-one 'r' and a checksum.
		static const Vector xrpl[] = {
			{"00000000000000000000000000000000000000000094a00911", "rrrrrrrrrrrrrrrrrrrrrhoLvTp"},
			{"0000000000000000000000000000000000000000019d35b5b9", "rrrrrrrrrrrrrrrrrrrrBZbvji"},
			{"00b5f762798a53d543a014caf8b297cff8f2f937e8bf32ba9f", "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"},
		};
		for (const Vector& v : xrpl) {
			assert(xrp.encode(from_hex(v.hex)) == v.text && xrp.decode(v.text) == from_hex(v.hex));
		}

		// Every form of encode and decode.
		const std::string abc("\0\0abc", 5);
		const std::string zeros2("\0\0", 2);
		assert(btc.encode(abc) == "11ZiCa" && btc.encode(std::string_view(abc)) == "11ZiCa");
		assert(btc.encode(abc.data(), abc.size()) == "11ZiCa");
		assert(btc.encode(reinterpret_cast<const unsigned char*>(abc.data()), abc.size()) == "11ZiCa");
		assert(btc.decode("11ZiCa") == abc && btc.decode("11ZiCa", 6) == abc);
		assert(btc.decode(std::string_view("11ZiCa")) == abc);
		assert(btc.encode("abc") == "ZiCa");  // no leading zero byte: the text of the number

		// All zeros, no bytes, and runs past the 256-digit stack buffer of decode.
		assert(btc.encode(zeros2) == "11" && btc.decode("11") == zeros2);
		assert(xrp.encode(zeros2) == "rr" && xrp.decode("rr") == zeros2);
		assert(btc.encode(std::string(1, '\0')) == "1" && btc.decode("1") == std::string(1, '\0'));
		assert(btc.encode(std::string()).empty() && btc.decode("").empty() && xrp.encode(std::string()).empty() && xrp.decode("").empty());
		assert(btc.encode(std::string_view()).empty() && btc.encode(static_cast<const char*>(nullptr), 0).empty() && btc.decode(nullptr, 0).empty());
		assert(btc.is_valid("") && btc.is_valid("1") && btc.is_valid("11") && !btc.is_valid("0") && !btc.is_valid("1l"));
		for (std::size_t n : {0u, 1u, 2u, 7u, 8u, 9u, 255u, 256u, 257u, 258u, 300u, 1000u}) {
			assert(btc.encode(std::string(n, '\0')) == std::string(n, '1') && btc.decode(std::string(n, '1')) == std::string(n, '\0'));
			assert(xrp.encode(std::string(n, '\0')) == std::string(n, 'r') && xrp.decode(std::string(n, 'r')) == std::string(n, '\0'));
			// A tail that is not zero: the zeros come back in front of it.
			assert(btc.decode(std::string(n, '1') + "2g") == std::string(n, '\0') + "a");
			assert(btc.encode(std::string(n, '\0') + "a") == std::string(n, '1') + "2g");
		}

		// A number has no leading zero bytes: zero is one zero digit, and the number readers drop them.
		assert(btc.encode(uinteger_t(0)) == "1" && btc.encode(uinteger_t(1)) == "2" && btc.encode(uinteger_t(58)) == "21");
		assert(xrp.encode(uinteger_t(0)) == "r" && xrp.encode(uinteger_t(1)) == "p" && xrp.encode(uinteger_t(58)) == "pr");
		const std::string one("\0\0\x01", 3);
		assert(btc.encode(uinteger_t(one.data(), one.size(), 256)) == "2" && btc.encode(one) == "112");
		uinteger_t number(9);
		btc.decode(number, "112", 3);
		assert(number == 1);
		btc.decode(number, "11", 2);
		assert(number == 0);
		assert(btc.decode<unsigned>("112") == 1 && btc.decode<unsigned>("11") == 0 && btc.decode<unsigned>("") == 0);
		assert(btc.decode<uinteger_t>("112") == 1 && xrp.decode<unsigned>("rrp") == 1);

		// Decoding into a Result replaces what it holds, and leaves it alone for a bad text.
		std::string out = "stale";
		btc.decode(out, "11ZiCa", 6);
		assert(out == abc);
		btc.decode(out, "", 0);
		assert(out.empty());
		out = "keep";
		assert(throws([&] { btc.decode(out, "11\x01", 3); }) && out == "keep");
		assert(throws([&] { btc.decode(out, "110", 3); }) && out == "keep");  // '0' is not in the alphabet
		std::vector<unsigned char> bytes(4, 9);
		btc.decode(bytes, "112g", 4);
		assert((bytes == std::vector<unsigned char>{0, 0, 0x61}));
		assert((btc.decode<std::vector<unsigned char>>("1111") == std::vector<unsigned char>(4, 0)));
		assert((btc.encode<std::string>(std::string_view("\0\0\x61", 3)) == "112g"));

		// Appending: the text written is what a fresh encode returns, whatever the buffer holds.
		for (const BaseX* enc : {&btc, &xrp}) {
			for (std::size_t zeros = 0; zeros <= 5; ++zeros) {
				for (std::size_t length = 0; length <= 6; ++length) {
					std::string payload(zeros, '\0');
					for (std::size_t i = 0; i < length; ++i) payload.push_back(static_cast<char>(0x40 + i * 29));
					const std::string fresh = enc->encode(payload);
					for (std::size_t prefix = 0; prefix <= 3; ++prefix) {
						std::string text(prefix, 'x');
						enc->encode(text, payload.data(), payload.size());
						assert(text == std::string(prefix, 'x') + fresh);
					}
				}
			}
		}

		// The numeric alphabets keep writing the number alone, the same twin of Bitcoin's alphabet included.
		const BaseX twin(0, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", "", "", "");
		assert(twin.encode(abc) == "ZiCa" && twin.decode("11ZiCa") == "abc" && twin.decode("11") == std::string(1, '\0'));
		assert(twin.encode(std::string(5, '\0') + "a") == "2g" && twin.decode("") == std::string(1, '\0'));
		assert(throws([&] { twin.encode(std::string()); }));
		for (Factory make : all_presets) {
			if (is_block(make) || make == Base58::bitcoin || make == Base58::ripple) continue;
			const BaseX& enc = make();
			assert(enc.encode(abc) == enc.encode(std::string_view("abc")));
			assert(enc.decode(enc.encode(abc)) == "abc");
		}
		{
			// decode_value reports the leading zero bytes of a text: one for each leading zero digit where
			// the flag is set, none in the numeric alphabets (a block-padded one reports its bytes instead).
			uinteger_t n;
			assert(btc.decode_value(n, "11ab", 4) == 2 && xrp.decode_value(n, "rrab", 4) == 2);
			assert(btc.decode_value(n, "11", 2) == 2 && btc.decode_value(n, "ab", 2) == 0 && btc.decode_value(n, "", 0) == 0);
			assert(twin.decode_value(n, "11ab", 4) == 0 && Base58::flickr().decode_value(n, "11ab", 4) == 0);
			assert(Base58::base58().decode_value(n, "00ab", 4) == 0 && Base64::rfc4648().decode_value(n, "AAAB", 4) == 3);
		}

		// Random bytes after zero to five leading zero bytes, of every length, against the model.
		std::uint32_t seed = 4242;
		auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
		for (const BaseX* enc : {&btc, &xrp}) {
			const std::string& alphabet = enc == &btc ? btc_alphabet : xrp_alphabet;
			const Schoolbook model{alphabet, 58, true};
			for (std::size_t zeros = 0; zeros <= 5; ++zeros) {
				for (std::size_t length = 0; length <= 70; ++length) {
					for (int round = 0; round < 3; ++round) {
						std::string payload(zeros, '\0');
						for (std::size_t i = 0; i < length; ++i) payload.push_back(static_cast<char>(next()));
						if (round == 0 && length) payload[zeros] = static_cast<char>(1 + next() % 255);
						const std::string text = enc->encode(payload);
						std::size_t lead = 0;
						while (lead < payload.size() && !payload[lead]) ++lead;
						assert(text == model.encode(payload) && enc->decode(text) == payload && enc->is_valid(text));
						assert(text.compare(0, lead, std::string(lead, alphabet[0])) == 0);
						assert(text.size() == lead || text[lead] != alphabet[0]);
						assert(model.decode(text) == payload);
						// The number overload and decoding to a number read what follows the zeros.
						uinteger_t value;
						enc->decode(value, text.data(), text.size());
						assert(lead == payload.size() || value == uinteger_t(payload.data() + lead, payload.size() - lead, 256));
						assert(enc->encode(value) == (value ? text.substr(lead) : std::string(1, alphabet[0])));
					}
				}
			}
		}

		// The flag with the other flags and with ignored characters, on alphabets of the owner's making.
		// Padding, check and checksum cover the zero digits like any other digit.
		const BaseX padded(BaseX::leading_zeros, "0123456789abcdefghijklmnopqrstuv", "", "====", "");
		assert(padded.encode(std::string_view("\0\0\0", 3)) == "000=" && padded.decode("000=") == std::string("\0\0\0", 3));
		assert(padded.encode(std::string_view("\0\0\0\0", 4)) == "0000" && padded.decode("0000") == std::string("\0\0\0\0", 4));
		assert(padded.encode(one) == "001=" && padded.decode("001=") == one && padded.decode("001") == one);
		assert(padded.encode(std::string()).empty() && padded.decode("").empty() && padded.decode("====").empty());
		const BaseX checked(BaseX::leading_zeros | BaseX::with_check, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", "", "", "");
		assert(checked.encode(one) == "1122" && checked.decode("1122") == one && checked.is_valid("1122"));
		assert(checked.encode(zeros2) == "111" && checked.decode("111") == zeros2);
		assert(checked.encode(std::string()) == "1" && checked.decode("1").empty());  // the check of no number is the zero digit
		assert(throws([&] { checked.decode("1123"); }) && !throws([&] { checked.decode("1111"); }));  // the check of three zero digits is one
		const BaseX summed(BaseX::leading_zeros | BaseX::with_checksum, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", "", "", "");
		assert(summed.encode(one) == "112w" && summed.decode("112w") == one && summed.is_valid("112w"));
		assert(throws([&] { summed.decode("112x"); }) && !summed.is_valid("112x"));
		assert(summed.encode(std::string()) == "1" && summed.decode("1").empty());
		assert(summed.encode(zeros2) != summed.encode(std::string(1, '\0')) && summed.decode(summed.encode(zeros2)) == zeros2);
		// Ignored characters and case before and between the leading zero digits.
		const BaseX lower(BaseX::leading_zeros | BaseX::ignore_case, "abcdefghijklmnopqrstuvwxyz234567", "", "", " -");
		assert(lower.encode(one) == "aab");
		for (const char* text : {"aab", "AAB", "aAb", " -a-A b", "a  A-b", "--aab--", "-A-a-B"}) {
			assert(lower.is_valid(text) && lower.decode(text) == one);
		}
		assert(lower.decode("- -b") == "\x01" && lower.decode("- -").empty() && lower.decode("-a-") == std::string(1, '\0'));
		const BaseX alias(BaseX::leading_zeros, "0123456789abcdefghijklmnopqrstuvwxyz", "", "", " -0_");
		assert(alias.decode("__a") == std::string("\0\0\x0a", 3) && alias.decode(" _ - 0_a") == std::string("\0\0\0\x0a", 4));
		assert(alias.decode("a__") == alias.decode("a00") && alias.decode("a__") == "\x32\xa0");  // a zero digit after another is a digit
		// The same for every flagged instance: zero to five leading zero bytes round-trip.
		for (const BaseX* enc : {&padded, &checked, &summed, &lower, &alias}) {
			for (std::size_t zeros = 0; zeros <= 5; ++zeros) {
				for (std::size_t length = 0; length <= 40; ++length) {
					std::string payload(zeros, '\0');
					for (std::size_t i = 0; i < length; ++i) payload.push_back(static_cast<char>(next()));
					const std::string text = enc->encode(payload);
					assert(enc->is_valid(text) && enc->decode(text) == payload);
					for (std::size_t prefix = 0; prefix <= 3; ++prefix) {
						std::string appended(prefix, 'x');
						enc->encode(appended, payload.data(), payload.size());
						assert(appended == std::string(prefix, 'x') + text);
					}
				}
			}
		}
	}

	std::printf("base-x OK: base58/base62/base16/base32/base64 round-trips + checksums passed\n");
	return 0;
}
