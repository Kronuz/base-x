// A runnable tour of base-x.
//
// Build (when this repo is the top-level project):
//   cmake -B build && cmake --build build && ./build/base_x_demo
//
// base-x treats the input as one big unsigned integer and rewrites it in the
// target base, so the same code path serves base16 through base66. This demo
// encodes a short payload across several alphabets and decodes it back to show
// the round-trip, then walks the distinctive bits: the look-alike-safe base58
// variants, integer encoding, RFC 4648 padded output versus the plain alphabet,
// the check-digit twins, and is_valid() screening before a throwing decode.
#include <cstdio>
#include <string>
#include <string_view>

#include "base_x.hh"

static void rule(const char* title) {
	std::printf("\n\033[1m── %s ──\033[0m\n", title);
}

// Print a byte string as two-digit hex, so binary payloads are legible as text
// instead of as raw (and possibly unprintable) bytes.
static std::string as_hex(std::string_view s) {
	std::string out;
	out.reserve(s.size() * 3);
	char buf[4];
	for (unsigned char c : s) {
		std::snprintf(buf, sizeof(buf), "%02x ", c);
		out += buf;
	}
	if (!out.empty()) out.pop_back();
	return out;
}

int main() {
	std::puts("base-x demo  (one integer, rewritten in many bases)");

	// --- 1. one payload, several alphabets, each a round-trip ----------------
	rule("encode a string across alphabets, then decode back");
	std::string_view payload = "Hello world!";
	std::printf("  input        : \"%.*s\"\n", (int)payload.size(), payload.data());
	std::printf("  input bytes  : %s\n", as_hex(payload).c_str());
	std::putc('\n', stdout);
	const std::pair<const char*, const BaseX&> codecs[] = {
		{"base16  ", Base16::base16()},
		{"base32  ", Base32::base32()},
		{"base58  ", Base58::base58()},
		{"base62  ", Base62::base62()},
		{"base64  ", Base64::base64()},
	};
	for (auto& [name, enc] : codecs) {
		auto encoded = enc.encode(payload);
		auto decoded = enc.decode(encoded);
		std::printf("  %s : %-24s -> decode -> \"%s\"%s\n",
			name, encoded.c_str(), decoded.c_str(),
			decoded == payload ? "" : "  (MISMATCH)");
	}
	std::puts("  (same source bytes, just rewritten in a different base each row)");

	// --- 2. base58 variants: same value, different look-alike-safe glyphs ----
	rule("base58 alphabets pick glyphs that survive being read aloud");
	std::printf("  payload      : \"%.*s\"\n", (int)payload.size(), payload.data());
	const std::pair<const char*, const BaseX&> b58[] = {
		{"base58 (GMP)", Base58::base58()},
		{"bitcoin     ", Base58::bitcoin()},
		{"ripple      ", Base58::ripple()},
		{"flickr      ", Base58::flickr()},
	};
	for (auto& [name, enc] : b58) {
		auto encoded = enc.encode(payload);
		std::printf("  %s : %s\n", name, encoded.c_str());
	}
	std::puts("  (each base58 row is the same number; only the digit glyphs differ)");

	// --- 3. encode an integer, not just bytes --------------------------------
	rule("an integer literal encodes as its numeric value");
	std::printf("  encode(519571) crockford base32 : %s   (the classic FVCK vector)\n",
		Base32::crockford().encode(519571).c_str());
	// Round-trip the larger value 0x48656c6c6f, which spells "Hello" as bytes,
	// to show the encoder really walks the numeric value of the input.
	auto rt = Base58::base58().encode("Hello");
	std::printf("  encode(\"Hello\") base58          : %s -> decode -> \"%s\"\n",
		rt.c_str(), Base58::base58().decode(rt).c_str());

	// --- 4. the plain alphabet is NOT byte-aligned; rfc4648 is ---------------
	rule("plain base64 encodes the integer; rfc4648() matches the standard");
	std::string_view one = "A";
	std::printf("  input              : \"%.*s\"  (%s)\n",
		(int)one.size(), one.data(), as_hex(one).c_str());
	std::printf("  base64()           : %-8s (the input as one integer)\n",
		Base64::base64().encode(one).c_str());
	std::printf("  base64 rfc4648()   : %-8s (block-padded with '=', == a stock base64)\n",
		Base64::rfc4648().encode(one).c_str());
	std::printf("  base16 rfc4648()   : %-8s (== stock uppercase hex of the byte)\n",
		Base16::rfc4648().encode(one).c_str());

	// --- 5. check digits catch a typo, and is_valid() never throws -----------
	rule("the *chk twins append a check character that decode verifies");
	auto plain = Base62::base62().encode(payload);
	auto checked = Base62::base62chk().encode(payload);
	std::printf("  base62()          : %s\n", plain.c_str());
	std::printf("  base62chk()       : %s   (one extra check char)\n", checked.c_str());
	std::printf("  is_valid(checked)             : %s\n",
		Base62::base62chk().is_valid(checked) ? "true" : "false");
	// Corrupt the last data character; the trailing check digit no longer agrees.
	std::string corrupted = checked;
	corrupted[corrupted.size() - 2] ^= 1;
	std::printf("  is_valid(one char flipped)    : %s   (caught without throwing)\n",
		Base62::base62chk().is_valid(corrupted) ? "true" : "false");
	std::printf("  is_valid(\"Some random text!\") : %s\n",
		Base62::base62chk().is_valid("Some random text!") ? "true" : "false");

	// --- 6. round-trip raw binary across alphabets ---------------------------
	rule("round-trip a raw binary blob (a UUID's worth of bytes)");
	std::string blob("\330\105\140\310\23\117\21\346\241\342\64\66\73\322\155\256");
	std::printf("  16 bytes     : %s\n", as_hex(blob).c_str());
	for (auto& [name, enc] : b58) {
		auto encoded = enc.encode(blob);
		auto decoded = enc.decode(encoded);
		std::printf("  %s : %-24s %s\n", name, encoded.c_str(),
			decoded == blob ? "(round-trips)" : "(MISMATCH)");
	}

	std::puts("\ndone.");
	return 0;
}
