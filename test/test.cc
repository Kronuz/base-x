// Smoke test for the standalone base-x library.
// Build: c++ -std=c++17 -I.. test.cc -o test && ./test
#include <cassert>
#include <cstdio>
#include <string>

#include "base_x.hh"

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
	}

	// A large payload round-trips in every family; bitwise bases decode in one pass.
	{
		std::string big(4096, '\0');
		for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<char>(i * 131 + (i >> 5) + 1);
		big[0] = '\x7b';
		for (const BaseX& enc : {Base16::base16(), Base32::base32(), Base64::base64(), Base64::rfc4648(), Base58::base58(), Base62::base62chk()}) {
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
	}

	std::printf("base-x OK: base58/base62/base16/base32/base64 round-trips + checksums passed\n");
	return 0;
}
