// Golden-output test: the text every preset encodes for a fixed set of inputs, kept in
// test/golden/encode.txt. The text of a preset is a stored format for the programs that keep it
// (ids, UUIDs), so a change in it has to be deliberate: regenerate the file and record the change in
// CHANGELOG.md.
//
//   base_x_golden <golden-file> [<base_x.hh>]   recompute and compare (what CTest runs); with the
//                                               header, also check that every preset in it is covered
//   base_x_golden --generate > test/golden/encode.txt
//
// Standalone: c++ -std=c++17 -I. -I<uinteger_t dir> test/golden.cc
//
// The test reports its failures itself rather than asserting, so it checks in a build with NDEBUG.
// The inputs come from a fixed-seed generator written here (std:: distributions differ between
// libraries), so they, and the file, are the same everywhere. Besides the text, every case checks
// that the text decodes back as the preset's documented semantics say.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "base_x.hh"

namespace {

struct Preset {
	const char* name;
	const BaseX& (*make)();
	// The bytes decode back exactly, leading zero bytes included: the leading_zeros and the block-padded
	// presets. The others encode the number, so a decode drops the leading zero bytes, and all-zero bytes
	// come back as one zero byte.
	bool keeps;
};

#define PRESET(Base, name, keeps) {#Base "::" #name, Base::name, keeps}

const Preset presets[] = {
	PRESET(Base2, base2, false), PRESET(Base2, base2chk, false),
	PRESET(Base8, base8, false), PRESET(Base8, base8chk, false),
	PRESET(Base11, base11, false), PRESET(Base11, base11chk, false),
	PRESET(Base16, base16, false), PRESET(Base16, base16chk, false), PRESET(Base16, rfc4648, true),
	PRESET(Base32, base32, false), PRESET(Base32, base32chk, false), PRESET(Base32, crockford, false),
	PRESET(Base32, crockfordchk, false), PRESET(Base32, rfc4648, true), PRESET(Base32, rfc4648hex, true),
	PRESET(Base36, base36, false), PRESET(Base36, base36chk, false),
	PRESET(Base58, base58, false), PRESET(Base58, base58chk, false), PRESET(Base58, bitcoin, true),
	PRESET(Base58, bitcoinchk, false), PRESET(Base58, ripple, true), PRESET(Base58, ripplechk, false),
	PRESET(Base58, flickr, false), PRESET(Base58, flickrchk, false),
	PRESET(Base59, base59, false), PRESET(Base59, base59chk, false), PRESET(Base59, dubaluchk, false),
	PRESET(Base62, base62, false), PRESET(Base62, base62chk, false), PRESET(Base62, inverted, false),
	PRESET(Base62, invertedchk, false),
	PRESET(Base64, base64, false), PRESET(Base64, base64chk, false), PRESET(Base64, url, false),
	PRESET(Base64, urlchk, false), PRESET(Base64, rfc4648, true), PRESET(Base64, rfc4648url, true),
	PRESET(Base64, rfc4648url_unpadded, true),
	PRESET(Base66, base66, false), PRESET(Base66, base66chk, false),
};
constexpr std::size_t preset_count = sizeof(presets) / sizeof(presets[0]);

// SplitMix64: a fixed sequence for a seed, on every platform.
struct Rng {
	std::uint64_t state;

	explicit Rng(std::uint64_t seed) : state(seed) {}

	std::uint64_t next() {
		std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}

	// A number in [lo, hi]. The modulo bias does not matter here; the sequence does.
	std::size_t between(std::size_t lo, std::size_t hi) {
		return lo + static_cast<std::size_t>(next() % (hi - lo + 1));
	}

	// Random bytes with zero and 0xff over-represented, so that runs and interior zero bytes show up.
	std::string bytes(std::size_t n) {
		std::string s(n, '\0');
		for (auto& c : s) {
			const auto r = next();
			const auto kind = r & 7;
			c = static_cast<char>(kind == 0 ? 0x00 : kind == 1 ? 0xff : (r >> 8) & 0xff);
		}
		return s;
	}
};

std::string to_hex(const std::string& s) {
	static const char digits[] = "0123456789abcdef";
	std::string out;
	for (unsigned char c : s) {
		out.push_back(digits[c >> 4]);
		out.push_back(digits[c & 15]);
	}
	return out;
}

std::string from_hex(const std::string& hex) {
	std::string out;
	for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
		const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
		out.push_back(static_cast<char>(nibble(hex[i]) * 16 + nibble(hex[i + 1])));
	}
	return out;
}

std::string hex64(std::uint64_t v) {
	std::string s;
	for (int shift = 60; shift >= 0; shift -= 4) s.push_back("0123456789abcdef"[(v >> shift) & 15]);
	return s;
}

std::uint64_t fnv1a(const std::string& s) {
	std::uint64_t h = 0xcbf29ce484222325ULL;
	for (unsigned char c : s) {
		h = (h ^ c) * 0x100000001b3ULL;
	}
	return h;
}

// The number of the bytes, built by shifts and additions alone.
uinteger_t number_of(const std::string& bytes) {
	uinteger_t v;
	for (unsigned char c : bytes) {
		v = (v << 8) + uinteger_t(static_cast<unsigned>(c));
	}
	return v;
}

// The shortest byte string of the number the bytes make: no leading zero bytes, one zero byte for zero.
std::string shortest(const std::string& bytes) {
	const auto first = bytes.find_first_not_of('\0');
	return first == std::string::npos ? std::string(1, '\0') : bytes.substr(first);
}

struct Case {
	bool number;        // encode the number of the bytes as a uinteger_t, not the bytes
	std::string label;  // the input column of the file
	std::string data;   // the bytes

	// The output column holds the length and hash of the text for the long inputs, not the text.
	bool digest() const {
		return data.size() > digest_above;
	}
	static constexpr std::size_t digest_above = 32;
};

// The input set, the same for every preset.
std::vector<Case> make_cases() {
	std::vector<Case> cases;
	std::set<std::string> seen;
	const auto add_hex = [&](const std::string& data) {
		if (seen.insert(data).second) {
			cases.push_back({false, data.empty() ? "\"\"" : to_hex(data), data});
		}
	};

	// The small edges: empty, zero bytes, a first byte of zero, one byte.
	for (const char* hex : {"", "00", "0000", "0001", "01", "ff", "7f", "80", "0100", "ff00", "00ff", "010000"}) {
		add_hex(from_hex(hex));
	}
	// Bytes that are all zero, of several lengths: the number zero, whatever the byte count.
	for (std::size_t n : {3, 4, 5, 8, 9, 16, 32}) {
		add_hex(std::string(n, '\0'));
	}
	// All 0xff, 1 to 64 bytes: a carry through every limb boundary.
	for (std::size_t n = 1; n <= 64; ++n) {
		cases.push_back({false, "ff:" + std::to_string(n), std::string(n, '\xff')});
	}
	// A one and n zero bytes: the powers of 256.
	for (std::size_t n : {1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63}) {
		add_hex(std::string(1, '\x01') + std::string(n, '\0'));
	}
	Rng rng(0x62617365782d676fULL);
	// One to three leading zero bytes, then up to 32 bytes that start with a nonzero one.
	for (std::size_t zeros = 1; zeros <= 3; ++zeros) {
		for (int i = 0; i < 4; ++i) {
			std::string tail = rng.bytes(rng.between(1, 32));
			if (!tail[0]) tail[0] = static_cast<char>(1 + rng.between(0, 254));
			add_hex(std::string(zeros, '\0') + tail);
		}
	}
	// Random bytes, 1 to 64 of them; and the sizes of the ids that are stored: UUIDs (16 bytes) and 8-byte ids.
	for (int i = 0; i < 24; ++i) {
		add_hex(rng.bytes(rng.between(1, 64)));
	}
	for (int i = 0; i < 8; ++i) {
		add_hex(rng.bytes(16));
	}
	for (int i = 0; i < 6; ++i) {
		add_hex(rng.bytes(8));
	}

	// Numbers: zero, one, around the limb sizes, large. Each one is its shortest byte string.
	const auto add_number = [&](const std::string& data) {
		cases.push_back({true, "u:" + to_hex(data), data});
	};
	for (const char* hex : {"00", "01", "02", "ff", "0100", "ffffffff", "0100000000", "7fffffffffffffff",
			"ffffffffffffffff", "010000000000000000", "010000000000000001", "ffffffffffffffffffffffffffffffff",
			"0100000000000000000000000000000000"}) {
		add_number(from_hex(hex));
	}
	std::string wide = rng.bytes(24);
	wide[0] = '\x5a';
	add_number(wide);
	add_number(std::string(32, '\xff'));
	add_number(std::string(1, '\x01') + std::string(63, '\0'));

	// Over 32 bytes the file keeps the length and hash of the text: 100 to 300 bytes, and over the 8192
	// bits where uinteger_t's conversion changes algorithm (the tree of divisions). `ff:n` is n bytes of
	// 0xff; `rand:n` is n bytes of the generator seeded with n.
	cases.push_back({false, "ff:2048", std::string(2048, '\xff')});
	for (std::size_t n : {100, 129, 200, 256, 300, 1100, 3000}) {
		cases.push_back({false, "rand:" + std::to_string(n), Rng(n).bytes(n)});
	}
	return cases;
}

struct Outcome {
	bool threw = false;
	std::string text;  // the encoded text, or the name of the exception type
};

Outcome encode(const BaseX& enc, const Case& c) {
	Outcome o;
	try {
		if (c.number) {
			o.text = enc.encode(number_of(c.data));
		} else {
			o.text = enc.encode(std::string_view(c.data));
		}
	} catch (const std::invalid_argument&) {
		o = {true, "invalid_argument"};
	} catch (const std::length_error&) {
		o = {true, "length_error"};
	} catch (const std::out_of_range&) {
		o = {true, "out_of_range"};
	} catch (const std::logic_error&) {
		o = {true, "logic_error"};
	} catch (const std::bad_alloc&) {
		o = {true, "bad_alloc"};
	} catch (const std::exception&) {
		o = {true, "exception"};
	} catch (...) {
		o = {true, "unknown"};
	}
	return o;
}

// The output column of the file.
std::string output_token(const Case& c, const Outcome& o) {
	if (o.threw) return "throw:" + o.text;
	if (c.digest()) return std::to_string(o.text.size()) + ":" + hex64(fnv1a(o.text));
	return o.text.empty() ? "\"\"" : o.text;
}

// A long text cut for a message.
std::string clip(const std::string& s, std::size_t n = 100) {
	return s.size() > n ? s.substr(0, n) + "..." : s;
}

// Reports failures: the first few in full, then a count.
struct Failures {
	std::size_t count = 0;
	std::size_t shown = 0;
	static constexpr std::size_t show = 12;

	void add(const std::string& message) {
		++count;
		if (shown < show) {
			++shown;
			std::fprintf(stderr, "FAIL %s\n", message.c_str());
		}
	}
	void finish() const {
		if (count > shown) std::fprintf(stderr, "... and %zu more\n", count - shown);
	}
};

// What the text of a case has to decode back to, as the preset's semantics say: a number's shortest byte
// string (one zero byte for zero); the bytes of a case, exactly or as their number.
void check_round_trip(const Preset& preset, const Case& c, const Outcome& o, Failures& failures) {
	const std::string where = std::string(preset.name) + " " + clip(c.label, 72);
	if (o.threw) {
		// The numeric presets have no number for no bytes; nothing else throws.
		if (!(c.data.empty() && !preset.keeps)) {
			failures.add(where + ": encode threw " + o.text);
		}
		return;
	}
	const BaseX& enc = preset.make();
	const std::string& text = o.text;
	if (!enc.is_valid(text)) {
		failures.add(where + ": is_valid rejects its own text " + clip(text, 40));
	}
	const std::string want = c.number || !preset.keeps ? shortest(c.data) : c.data;
	try {
		const std::string got = enc.decode(text);
		if (got != want) {
			failures.add(where + ": decodes to " + clip(to_hex(got), 72) + ", want " + clip(to_hex(want), 72));
		}
		if (!c.data.empty()) {
			uinteger_t value;
			enc.decode(value, text.data(), text.size());
			if (value != number_of(c.data)) {
				failures.add(where + ": decodes to another number");
			}
		}
	} catch (const std::exception& e) {
		failures.add(where + ": decode throws " + e.what());
	}
}

// "Base16::rfc4648" for each `static const BaseX& rfc4648()` in the struct Base16 of the header.
std::set<std::string> header_presets(const std::string& path, bool& ok) {
	std::set<std::string> names;
	std::ifstream in(path);
	ok = static_cast<bool>(in);
	std::string line, current;
	while (std::getline(in, line)) {
		if (line.compare(0, 7, "struct ") == 0) {
			current = line.substr(7, line.find_first_of(" {", 7) - 7);
		}
		const std::string marker = "static const BaseX& ";
		const auto at = line.find(marker);
		if (at != std::string::npos && !current.empty()) {
			const auto start = at + marker.size();
			names.insert(current + "::" + line.substr(start, line.find('(', start) - start));
		}
	}
	return names;
}

// One case of one preset, encoded.
struct Row {
	const Preset* preset;
	const Case* input;
	Outcome outcome;
	std::string key;     // the preset and the input: how the file finds the line
	std::string output;  // the output column
};

std::vector<Row> compute(const std::vector<Case>& cases) {
	std::vector<Row> rows;
	for (const auto& preset : presets) {
		const BaseX& enc = preset.make();
		for (const auto& c : cases) {
			Row row{&preset, &c, encode(enc, c), std::string(preset.name) + " " + c.label, ""};
			row.output = output_token(c, row.outcome);
			rows.push_back(std::move(row));
		}
	}
	return rows;
}

const char header[] =
	"# base-x golden output: what every preset encodes for a fixed set of inputs.\n"
	"# Generated by `base_x_golden --generate`; do not edit by hand. A change in any line is a change in\n"
	"# the stored format of a preset: regenerate the file and record the change in CHANGELOG.md.\n"
	"#\n"
	"# One line per case: <preset> <input> <output>.\n"
	"#   input   hex bytes (\"\" for none); u:<hex> the number of those bytes, as a uinteger_t;\n"
	"#           ff:<n> n bytes of 0xff; rand:<n> n bytes of the test's SplitMix64 generator seeded with n.\n"
	"#   output  the text (\"\" for the empty text); throw:<type> when encode throws; for an input of\n"
	"#           more than 32 bytes, <chars>:<fnv1a-64>, the length and hash of the text.\n";

}  // namespace

int main(int argc, char** argv) {
	const auto cases = make_cases();
	const auto rows = compute(cases);

	if (argc == 2 && !std::strcmp(argv[1], "--generate")) {
		std::fputs(header, stdout);
		for (const auto& row : rows) {
			std::printf("%s %s\n", row.key.c_str(), row.output.c_str());
		}
		return 0;
	}
	if (argc < 2 || argc > 3) {
		std::fprintf(stderr, "usage: %s <golden-file> [<base_x.hh>] | --generate\n", argv[0]);
		return 2;
	}

	Failures failures;

	// The golden file: key (preset and input) to output.
	std::map<std::string, std::string> golden;
	{
		std::ifstream in(argv[1]);
		if (!in) {
			std::fprintf(stderr, "cannot read %s\n", argv[1]);
			return 2;
		}
		std::string line;
		std::size_t number = 0;
		while (std::getline(in, line)) {
			++number;
			if (line.empty() || line[0] == '#') continue;
			const auto second = line.find(' ', line.find(' ') + 1);
			if (line.find(' ') == std::string::npos || second == std::string::npos || line.find(' ', second + 1) != std::string::npos) {
				failures.add("line " + std::to_string(number) + " of the golden file is not <preset> <input> <output>");
				continue;
			}
			if (!golden.emplace(line.substr(0, second), line.substr(second + 1)).second) {
				failures.add("line " + std::to_string(number) + " of the golden file repeats " + line.substr(0, second));
			}
		}
	}

	// Every case against the file, and against the semantics of the preset.
	std::size_t compared = 0;
	for (const auto& row : rows) {
		const auto found = golden.find(row.key);
		if (found == golden.end()) {
			failures.add(clip(row.key) + ": not in the golden file (got " + clip(row.output) + ")");
		} else {
			++compared;
			if (found->second != row.output) {
				failures.add(clip(row.key) + ":\n     golden " + clip(found->second) + "\n     got    " + clip(row.output) +
					(row.input->digest() && !row.outcome.threw ? "\n     text   " + clip(row.outcome.text) : ""));
			}
			golden.erase(found);
		}
		check_round_trip(*row.preset, *row.input, row.outcome, failures);
	}
	for (const auto& entry : golden) {
		failures.add(clip(entry.first) + ": in the golden file, not in the test's input set");
	}

	// Every preset of the header is in the table above, and the other way around.
	if (argc == 3) {
		bool ok = false;
		const auto names = header_presets(argv[2], ok);
		if (!ok) {
			std::fprintf(stderr, "cannot read %s\n", argv[2]);
			return 2;
		}
		std::set<std::string> covered;
		for (const auto& preset : presets) covered.insert(preset.name);
		for (const auto& name : names) {
			if (!covered.count(name)) failures.add(name + ": a preset of base_x.hh that the golden test does not cover");
		}
		for (const auto& name : covered) {
			if (!names.count(name)) failures.add(name + ": in the golden test, not in base_x.hh");
		}
	}

	failures.finish();
	if (failures.count) {
		std::fprintf(stderr, "base-x golden FAILED: %zu failures (regenerate with --generate only for a deliberate output change)\n", failures.count);
		return 1;
	}
	std::printf("base-x golden OK: %zu presets, %zu cases each, %zu lines compared\n", preset_count, cases.size(), compared);
	return 0;
}
