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
	// it. The numeric alphabets write 1 whatever the zero count; the RFC presets write every byte (Python).
	{
		for (std::size_t zeros = 0; zeros <= 20; ++zeros) {
			std::string bytes(zeros, '\0');
			bytes.push_back('\x01');
			const uinteger_t value(bytes.data(), bytes.size(), 256);
			assert(value == 1 && !(value > 1) && value.bits() == 1);
			assert(Base58::bitcoin().encode(bytes) == "2" && Base58::bitcoin().encode(value) == "2");
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
		struct Model {
			std::string alphabet;  // digit -> character
			unsigned base;
			std::string encode(const std::string& bytes) const {
				std::vector<unsigned> digits;  // least significant first
				for (unsigned char byte : bytes) {
					unsigned carry = byte;
					for (auto& d : digits) {
						carry += d * 256u;
						d = carry % base;
						carry /= base;
					}
					for (; carry; carry /= base) digits.push_back(carry % base);
				}
				std::string out;
				for (auto i = digits.size(); i--;) out.push_back(alphabet[digits[i]]);
				return out.empty() ? std::string(1, alphabet[0]) : out;
			}
			std::string decode(const std::string& text) const {
				std::vector<unsigned> bytes;  // least significant first
				for (char c : text) {
					unsigned carry = static_cast<unsigned>(alphabet.find(c));
					for (auto& b : bytes) {
						carry += b * base;
						b = carry & 255u;
						carry >>= 8;
					}
					for (; carry; carry >>= 8) bytes.push_back(carry & 255u);
				}
				std::string out;
				for (auto i = bytes.size(); i--;) out.push_back(static_cast<char>(bytes[i]));
				return out.empty() ? std::string(1, '\0') : out;
			}
		};
		std::uint32_t seed = 12345;
		auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
		const Factory plain[] = {Base2::base2, Base8::base8, Base11::base11, Base16::base16, Base32::base32,
		                         Base32::crockford, Base36::base36, Base58::base58, Base58::bitcoin, Base58::ripple, Base58::flickr,
		                         Base59::base59, Base62::base62, Base62::inverted, Base64::base64, Base64::url, Base66::base66};
		for (Factory make : plain) {
			const BaseX& enc = make();
			Model model;
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
					std::string value = bytes.substr(std::min(bytes.find_first_not_of('\0'), bytes.size() - 1));
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

	std::printf("base-x OK: base58/base62/base16/base32/base64 round-trips + checksums passed\n");
	return 0;
}
