// Smoke test for the standalone base-x library.
// Build: c++ -std=c++17 -I.. test.cc -o test && ./test
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>

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
				if (at == good.size() && good.back() == '=') continue;  // decode ignores what follows the padding
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
		// with one character replaced, inserted, deleted or cut off (an unpadded alphabet exactly;
		// padded ones only reject what decode rejects, since decode ignores text after the padding;
		// the check alphabet only accepts what decode accepts, since is_valid does not recompute the check).
		std::string pool;
		for (int c = 32; c < 127; ++c) pool.push_back(static_cast<char>(c));
		pool += std::string("\0\n\r\x80\xff", 5);
		for (Factory make : all_presets) {
			const BaseX& enc = make();
			const bool padded = enc.encode(std::string_view("a")).back() == '=';
			const bool checked = &enc == &Base32::crockfordchk();
			auto agree = [&](const std::string& text) {
				bool accepted = true;
				try { enc.decode(text); } catch (const std::invalid_argument&) { accepted = false; }
				const bool valid = enc.is_valid(text);
				if (checked) assert(!accepted || valid);
				else if (padded) assert(!valid || accepted);
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

	std::printf("base-x OK: base58/base62/base16/base32/base64 round-trips + checksums passed\n");
	return 0;
}
