// Fuzz target for BaseX::decode and is_valid.
//
// libFuzzer: clang++ -std=c++17 -O1 -g -fsanitize=fuzzer,address,undefined -I. test/fuzz_decode.cc
// Standalone (a fixed-seed driver, run by CTest): c++ -std=c++17 -O1 -DFUZZ_STANDALONE -I. test/fuzz_decode.cc
//   ./a.out [inputs]
//
// The first input byte picks a preset and the rest is the text to decode. A plain preset (no
// padding, check, checksum or ignored characters) answers to a model that shares nothing with the
// library: text of alphabet characters decodes to what byte-wise schoolbook conversion gives, any
// other text throws std::invalid_argument naming its first character outside the alphabet and its
// position. A block-padded preset (the RFC 4648 ones) answers to a bit-level model: the digits' bits
// cut into whole bytes, text after the padding rejected. Every preset must not crash or read outside the
// text, and a value it decodes must encode back to a text that decodes to the same value.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "base_x.hh"

namespace {

using Factory = const BaseX& (*)();

const Factory presets[] = {
	// Strict: no case folding, translation, padding, check or checksum.
	Base2::base2, Base8::base8, Base58::base58, Base58::bitcoin, Base58::ripple, Base58::flickr,
	Base62::base62, Base62::inverted, Base64::base64, Base64::url,
	// Folding, translation, check and checksum: they only have to hold up and agree with themselves.
	Base11::base11, Base16::base16, Base32::base32, Base36::base36, Base59::base59, Base66::base66,
	Base2::base2chk, Base16::base16chk, Base32::base32chk, Base36::base36chk, Base58::bitcoinchk, Base62::base62chk,
	Base64::base64chk, Base66::base66chk, Base32::crockford, Base32::crockfordchk, Base59::dubaluchk,
	// Block padding: the value is shifted by the padding, so a decoded value is not the text's number.
	Base16::rfc4648, Base32::rfc4648, Base32::rfc4648hex, Base64::rfc4648, Base64::rfc4648url, Base64::rfc4648url_unpadded,
};
constexpr std::size_t plain_presets = 10;
constexpr std::size_t unpadded_presets = 27;
constexpr std::size_t preset_count = sizeof(presets) / sizeof(presets[0]);

struct Model {
	std::string alphabet;  // digit -> character
	unsigned base = 0;

	explicit Model(const BaseX& enc) {
		for (unsigned v = 0;; ++v) {
			const std::string one = enc.encode(uinteger_t(v));
			if (v && one.size() > 1) {
				base = v;
				return;
			}
			alphabet += one;
		}
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

// A block-padded preset: its digits are bits_per_digit bits each, the text is those bits cut into
// whole bytes (the rest is dropped), padding ends the digits, and only padding and line breaks follow it.
struct BlockModel {
	const char* alphabet;
	unsigned bits_per_digit;
	char padding;        // '\0' for none
	bool ignore_breaks;  // '\n' and '\r' are skipped

	// Whether the text decodes; the bytes, or the message of the first character that does not.
	bool decode(const std::string& text, std::string& bytes, std::string& message) const {
		std::string bits;
		bool padded = false;
		for (std::size_t i = 0; i < text.size(); ++i) {
			const char c = text[i];
			const char* at = c ? std::strchr(alphabet, c) : nullptr;
			if (at && !padded) {
				const unsigned digit = static_cast<unsigned>(at - alphabet);
				for (unsigned b = bits_per_digit; b--;) bits.push_back(static_cast<char>('0' + ((digit >> b) & 1)));
			} else if (!at && padding && c == padding) {
				padded = true;
			} else if (!at && ignore_breaks && (c == '\n' || c == '\r')) {
			} else {
				message = std::string("Error: Invalid character: '") + c + "' at " + std::to_string(i);
				return false;
			}
		}
		bytes.clear();
		for (std::size_t i = 0; i + 8 <= bits.size(); i += 8) {
			unsigned byte = 0;
			for (std::size_t b = 0; b < 8; ++b) byte = byte * 2 + static_cast<unsigned>(bits[i + b] - '0');
			bytes.push_back(static_cast<char>(byte));
		}
		return true;
	}
};

const BlockModel block_models[] = {
	{"0123456789ABCDEF", 4, '\0', false},
	{"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", 5, '=', true},
	{"0123456789ABCDEFGHIJKLMNOPQRSTUV", 5, '=', true},
	{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", 6, '=', true},
	{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", 6, '=', true},
	{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", 6, '\0', true},
};

const Model& model_of(std::size_t index) {
	static std::vector<Model> models = [] {
		std::vector<Model> m;
		for (std::size_t i = 0; i < plain_presets; ++i) m.emplace_back(presets[i]());
		return m;
	}();
	return models[index];
}

// The characters a preset writes, for building texts that mostly decode.
const std::string& charset_of(std::size_t index) {
	static std::vector<std::string> sets = [] {
		std::vector<std::string> v;
		std::mt19937_64 g(5);
		for (std::size_t i = 0; i < preset_count; ++i) {
			std::string set;
			for (int round = 0; round < 6; ++round) {
				std::string input(100, '\0');
				for (auto& c : input) c = static_cast<char>(g());
				input[0] = 1;
				for (char c : presets[i]().encode(input)) {
					if (set.find(c) == std::string::npos) set.push_back(c);
				}
			}
			v.push_back(set);
		}
		return v;
	}();
	return sets[index];
}

void check(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "fuzz_decode: %s\n", what);
		std::abort();
	}
}

void run(std::size_t index, const std::string& text) {
	const BaseX& enc = presets[index]();
	bool accepted = true;
	uinteger_t value;
	std::string message;
	try {
		enc.decode(value, text.data(), text.size());
	} catch (const std::invalid_argument& e) {
		accepted = false;
		message = e.what();
	}
	std::string bytes;
	bool bytes_ok = true;
	try {
		bytes = enc.decode(text.data(), text.size());
	} catch (const std::invalid_argument&) {
		bytes_ok = false;
	}
	check(accepted == bytes_ok, "decode to a value and to a string disagree");
	const bool valid = enc.is_valid(text.data(), text.size());

	if (index < plain_presets) {
		const Model& model = model_of(index);
		const std::size_t bad = text.find_first_not_of(model.alphabet);
		if (bad == std::string::npos) {
			check(accepted, "a text of alphabet characters was rejected");
			check(bytes == model.decode(text), "decode differs from the model");
			check(valid, "is_valid rejects a text decode accepts");
		} else {
			check(!accepted, "a text with a character outside the alphabet was accepted");
			// what() is a C string, so a NUL byte in the message ends it there.
			const std::string expected = std::string("Error: Invalid character: '") + text[bad] + "' at " + std::to_string(bad);
			check(message == std::string(expected.c_str()), "wrong message for a bad character");
			check(!valid, "is_valid accepts a text decode rejects");
		}
	}
	if (index >= unpadded_presets) {
		const BlockModel& model = block_models[index - unpadded_presets];
		std::string expected, expected_message;
		const bool expect = model.decode(text, expected, expected_message);
		check(accepted == expect, "a block-padded text is accepted or rejected against the model");
		check(valid == expect, "is_valid and the model disagree on a block-padded text");
		if (expect) {
			check(bytes == expected, "block-padded decode differs from the model");
			check(enc.decode(enc.encode(bytes)) == bytes, "bytes do not survive encode and decode");
		} else {
			check(message == std::string(expected_message.c_str()), "wrong message for a bad character in a block-padded text");
		}
	}
	if (accepted && value && index < unpadded_presets) {  // zero has no check characters, so it does not decode back from its text
		const std::string again = enc.encode(value);
		uinteger_t back;
		enc.decode(back, again.data(), again.size());
		check(back == value, "a decoded value does not survive encode and decode");
	}
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if (!size) return 0;
	run(data[0] % preset_count, std::string(reinterpret_cast<const char*>(data) + 1, size - 1));
	return 0;
}

#ifdef FUZZ_STANDALONE
int main(int argc, char** argv) {
	const long inputs = argc > 1 ? std::atol(argv[1]) : 20000;
	std::mt19937_64 rng(20261031);
	static const char junk[] = "-\n\r =_.~!*$\xff\x80zZ0OIl1/+AaB";
	for (long n = 0; n < inputs; ++n) {
		const std::size_t index = rng() % preset_count;
		const std::string& chars = charset_of(index);
		std::string text;
		// Mostly alphabet characters, from short to past the 256-character stack buffer.
		const std::size_t length = rng() % 8 ? rng() % 70 : rng() % 600;
		for (std::size_t i = 0; i < length; ++i) text.push_back(chars[rng() % chars.size()]);
		if (index >= unpadded_presets && rng() % 2) {
			// A block-padded text that is what encode writes for bytes with leading zeros, to edit.
			std::string raw(rng() % 41, '\0');
			for (std::size_t i = rng() % 11; i < raw.size(); ++i) raw[i] = static_cast<char>(rng());
			text = presets[index]().encode(raw);
		}
		const int edits = static_cast<int>(rng() % 4);
		for (int e = 0; e < edits && !text.empty(); ++e) {
			switch (rng() % 4) {
				case 0: text[rng() % text.size()] = junk[rng() % (sizeof junk - 1)]; break;
				case 1: text.insert(rng() % text.size(), 1, junk[rng() % (sizeof junk - 1)]); break;
				case 2: text[rng() % text.size()] = static_cast<char>(rng()); break;
				default: text.resize(text.size() - 1); break;
			}
		}
		std::string input(1, static_cast<char>(index));
		LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>((input + text).data()), 1 + text.size());
	}
	std::printf("fuzz_decode: %ld inputs ok\n", inputs);
	return 0;
}
#endif
