// Times base-x encode and decode, and compares base58 with other codecs when asked.
//
// Build (UT is a uinteger_t checkout; run from the repository root):
//   c++ -std=c++17 -O2 -I. -I$UT bench/bench.cc -o bench
// With the comparisons. LIBBASE58 and FIREDANCER are checkouts of libbase58 (MIT) and
// Firedancer (Apache-2.0); neither is vendored here. Leave out -mavx2 so Firedancer
// takes its portable path:
//   cc -O2 -c $LIBBASE58/base58.c -o libbase58.o
//   cc -O2 -c $FIREDANCER/src/ballet/base58/fd_base58.c -o fd_base58.o
//   c++ -std=c++17 -O2 -I. -I$UT -DHAVE_LIBBASE58 -I$LIBBASE58 \
//       -DHAVE_FD_BASE58 -I$FIREDANCER/src/ballet/base58 \
//       bench/bench.cc libbase58.o fd_base58.o -o bench
// (-DHAVE_LIBBASE58 and -DHAVE_FD_BASE58 are independent; use either alone.)
// Run: ./bench [sizes...] [filters...], e.g. ./bench 32 64 compare
// Sizes are input lengths in bytes (default 16 20 25 32 48 64 256 1024). A filter is a
// substring of a codec name (base58, rfc4648, ...) or "compare" for the base58 table.
//
// Inputs are random with a nonzero first byte, from a pool of 64 per size that the calls
// cycle through. Every codec in the comparison is checked against base-x's bitcoin()
// alphabet before it is timed. The base-x columns call the API as a user does: encode
// returns a std::string, decode returns a std::string. "bx reuse" encodes into one string
// that keeps its capacity, as the other codecs write into a caller's buffer.
//
// The classic codec below is the textbook quadratic base58 (as Bitcoin-style codecs do it),
// written for this file; MIT, like the rest of the repository.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include "base_x.hh"
#ifdef HAVE_LIBBASE58
#include "libbase58.h"
#endif
#ifdef HAVE_FD_BASE58
extern "C" {
#include "fd_base58.h"
}
#endif

using Clock = std::chrono::steady_clock;

static constexpr int rounds = 7;          // the result is the best of this many
static constexpr double round_ms = 10;    // each round runs for at least this long
static constexpr std::size_t pool_size = 64;

static volatile std::size_t sink;

// Nanoseconds per call of f(i), best of several rounds. i counts calls, so f can cycle
// through its inputs; it returns a value that depends on its output.
template <typename F>
static double bench(F&& f) {
	// Calibrate the calls per round; this also warms up the lazy tables and the caches.
	std::size_t n = 1;
	for (;;) {
		auto s = Clock::now();
		for (std::size_t i = 0; i < n; ++i) sink = f(i);
		if (std::chrono::duration<double, std::milli>(Clock::now() - s).count() >= round_ms) break;
		n *= 2;
	}
	double best = 1e300;
	for (int round = 0; round < rounds; ++round) {
		auto s = Clock::now();
		for (std::size_t i = 0; i < n; ++i) sink = f(i);
		double ns = std::chrono::duration<double, std::nano>(Clock::now() - s).count();
		best = std::min(best, ns / static_cast<double>(n));
	}
	return best;
}

[[noreturn]] static void mismatch(const char* what, std::size_t bytes) {
	std::fprintf(stderr, "bench: %s differs from base-x at %zu bytes\n", what, bytes);
	std::exit(1);
}

// The classic codec: Bitcoin-style base58 by repeated multiplication over a byte array.
// Encoding keeps the base-58 digits in an array and, for each input byte, multiplies
// them by 256 and adds the byte (dividing the carry by 58 along the way). Decoding keeps
// the bytes and, for each character, multiplies them by 58 and adds the digit (dividing
// the carry by 256). Both are quadratic in the length. Leading zero bytes and leading '1'
// characters correspond one to one, as in Bitcoin.
struct Classic {
	static constexpr const char* alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
	signed char ord[256];
	std::vector<unsigned char> scratch;

	Classic() {
		std::memset(ord, -1, sizeof ord);
		for (int i = 0; i < 58; ++i) ord[static_cast<unsigned char>(alphabet[i])] = static_cast<signed char>(i);
	}

	// out takes up to size * 138 / 100 + 2 characters; returns the length.
	std::size_t encode(const unsigned char* in, std::size_t size, char* out) {
		std::size_t zeros = 0;
		while (zeros < size && !in[zeros]) ++zeros;
		scratch.resize(size * 138 / 100 + 2);
		unsigned char* digits = scratch.data();
		std::size_t len = 0;
		for (std::size_t i = zeros; i < size; ++i) {
			unsigned carry = in[i];
			for (std::size_t j = 0; j < len; ++j) {
				carry += static_cast<unsigned>(digits[j]) << 8;
				digits[j] = static_cast<unsigned char>(carry % 58);
				carry /= 58;
			}
			for (; carry; carry /= 58) digits[len++] = static_cast<unsigned char>(carry % 58);
		}
		char* p = out;
		for (; zeros; --zeros) *p++ = alphabet[0];
		while (len) *p++ = alphabet[digits[--len]];
		return static_cast<std::size_t>(p - out);
	}

	// out takes up to size bytes; returns the length, or SIZE_MAX for a character not in the alphabet.
	std::size_t decode(const char* in, std::size_t size, unsigned char* out) {
		std::size_t zeros = 0;
		while (zeros < size && in[zeros] == alphabet[0]) ++zeros;
		scratch.resize(size);
		unsigned char* bytes = scratch.data();
		std::size_t len = 0;
		for (std::size_t i = zeros; i < size; ++i) {
			int d = ord[static_cast<unsigned char>(in[i])];
			if (d < 0) return SIZE_MAX;
			unsigned carry = static_cast<unsigned>(d);
			for (std::size_t j = 0; j < len; ++j) {
				carry += static_cast<unsigned>(bytes[j]) * 58;
				bytes[j] = static_cast<unsigned char>(carry & 0xff);
				carry >>= 8;
			}
			for (; carry; carry >>= 8) bytes[len++] = static_cast<unsigned char>(carry & 0xff);
		}
		unsigned char* p = out;
		for (; zeros; --zeros) *p++ = 0;
		while (len) *p++ = bytes[--len];
		return static_cast<std::size_t>(p - out);
	}
};

struct Codec {
	const char* name;
	const BaseX& (*get)();
};

static const Codec codecs[] = {
	{"Base58::base58", Base58::base58},
	{"Base58::bitcoin", Base58::bitcoin},
	{"Base62::base62", Base62::base62},
	{"Base64::base64", Base64::base64},
	{"Base64::rfc4648", Base64::rfc4648},
	{"Base16::rfc4648", Base16::rfc4648},
};

using Bytes = std::vector<unsigned char>;

static std::size_t digest(const std::string& s) {
	return s.size() + static_cast<unsigned char>(s[0]);
}

int main(int argc, char** argv) {
	std::vector<std::size_t> sizes;
	std::vector<std::string> filters;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] >= '0' && argv[i][0] <= '9') {
			sizes.push_back(static_cast<std::size_t>(std::atoi(argv[i])));
		} else {
			filters.push_back(argv[i]);
		}
	}
	if (sizes.empty()) sizes = {16, 20, 25, 32, 48, 64, 256, 1024};
	auto want = [&](const char* name) {
		if (filters.empty()) return true;
		for (const auto& f : filters) {
			if (std::string(name).find(f) != std::string::npos) return true;
		}
		return false;
	};

	std::printf("# base-x bench, best of %d rounds of at least %.0f ms; ns per call\n", rounds, round_ms);
#ifdef __VERSION__
	std::printf("# %s\n", __VERSION__);
#endif
	std::printf("# comparisons: classic"
#ifdef HAVE_LIBBASE58
		", libbase58"
#endif
#ifdef HAVE_FD_BASE58
		", firedancer"
#endif
		"\n");

	std::mt19937_64 g(1);
	auto inputs = [&](std::size_t size) {
		std::vector<Bytes> pool(pool_size, Bytes(size));
		for (auto& in : pool) {
			for (auto& b : in) b = static_cast<unsigned char>(g());
			in[0] = static_cast<unsigned char>(1 + g() % 255);
		}
		return pool;
	};

	// Every codec: encode and decode, after checking the round trip.
	bool header = false;
	for (const auto& codec : codecs) {
		if (!want(codec.name)) continue;
		if (!header) {
			std::printf("\n%-16s %6s %10s %9s %10s %9s\n", "codec", "bytes", "enc ns", "enc ns/B", "dec ns", "dec ns/B");
			header = true;
		}
		const BaseX& c = codec.get();
		for (auto size : sizes) {
			auto pool = inputs(size);
			std::vector<std::string> text(pool_size);
			for (std::size_t i = 0; i < pool_size; ++i) {
				text[i] = c.encode(pool[i].data(), size);
				std::string back = c.decode(text[i].data(), text[i].size());
				if (back.size() != size || std::memcmp(back.data(), pool[i].data(), size)) mismatch(codec.name, size);
			}
			double enc = bench([&](std::size_t i) { return digest(c.encode(pool[i % pool_size].data(), size)); });
			double dec = bench([&](std::size_t i) {
				const auto& s = text[i % pool_size];
				return digest(c.decode(s.data(), s.size()));
			});
			double bytes = static_cast<double>(size);
			std::printf("%-16s %6zu %10.1f %9.2f %10.1f %9.2f\n", codec.name, size, enc, enc / bytes, dec, dec / bytes);
			std::fflush(stdout);
		}
	}

	// The base58 comparison, on the Bitcoin alphabet, which every codec here shares.
	if (!want("compare")) return 0;
	const BaseX& bx = Base58::bitcoin();
	Classic classic;
	std::printf("\n%-8s | %9s %9s %9s %9s %9s | %9s %9s %9s %9s\n", "bytes", "enc bx", "bx reuse", "classic", "libb58",
		"firedncr", "dec bx", "classic", "libb58", "firedncr");
	for (auto size : sizes) {
		auto pool = inputs(size);
		std::vector<std::string> text(pool_size);
		const std::size_t cap = size * 138 / 100 + 8;
		std::vector<char> out(cap);
		Bytes bin(size + 8);
		[[maybe_unused]] const bool fd = size == 32 || size == 64;
		for (std::size_t i = 0; i < pool_size; ++i) {
			const unsigned char* in = pool[i].data();
			text[i] = bx.encode(in, size);
			const auto& ref = text[i];
			if (classic.encode(in, size, out.data()) != ref.size() || std::memcmp(out.data(), ref.data(), ref.size())) mismatch("classic encode", size);
			if (classic.decode(ref.data(), ref.size(), bin.data()) != size || std::memcmp(bin.data(), in, size)) mismatch("classic decode", size);
#ifdef HAVE_LIBBASE58
			std::size_t sz = cap;
			if (!b58enc(out.data(), &sz, in, size) || sz != ref.size() + 1 || std::memcmp(out.data(), ref.data(), ref.size())) mismatch("libbase58 encode", size);
			sz = size;
			if (!b58tobin(bin.data(), &sz, ref.data(), ref.size()) || sz != size || std::memcmp(bin.data(), in, size)) mismatch("libbase58 decode", size);
#endif
#ifdef HAVE_FD_BASE58
			if (fd) {
				char f[FD_BASE58_ENCODED_64_SZ];
				ulong fl = 0;
				if (size == 32) fd_base58_encode_32(in, &fl, f); else fd_base58_encode_64(in, &fl, f);
				if (fl != ref.size() || std::memcmp(f, ref.data(), fl)) mismatch("firedancer encode", size);
				if (!(size == 32 ? fd_base58_decode_32(ref.c_str(), bin.data()) : fd_base58_decode_64(ref.c_str(), bin.data())) || std::memcmp(bin.data(), in, size)) mismatch("firedancer decode", size);
			}
#endif
		}

		const double none = -1;
		double e_bx = bench([&](std::size_t i) { return digest(bx.encode(pool[i % pool_size].data(), size)); });
		std::string reuse;
		double e_re = bench([&](std::size_t i) {
			reuse.clear();
			bx.encode(reuse, pool[i % pool_size].data(), size);
			return digest(reuse);
		});
		double e_cl = bench([&](std::size_t i) {
			auto n = classic.encode(pool[i % pool_size].data(), size, out.data());
			return n + static_cast<unsigned char>(out[0]);
		});
		double e_lb = none, e_fd = none;
#ifdef HAVE_LIBBASE58
		e_lb = bench([&](std::size_t i) {
			std::size_t sz = cap;
			b58enc(out.data(), &sz, pool[i % pool_size].data(), size);
			return sz + static_cast<unsigned char>(out[0]);
		});
#endif
#ifdef HAVE_FD_BASE58
		if (fd) {
			char f[FD_BASE58_ENCODED_64_SZ];
			e_fd = bench([&](std::size_t i) {
				ulong fl;
				const unsigned char* in = pool[i % pool_size].data();
				if (size == 32) fd_base58_encode_32(in, &fl, f); else fd_base58_encode_64(in, &fl, f);
				return static_cast<std::size_t>(fl) + static_cast<unsigned char>(f[0]);
			});
		}
#endif
		double d_bx = bench([&](std::size_t i) {
			const auto& s = text[i % pool_size];
			return digest(bx.decode(s.data(), s.size()));
		});
		double d_cl = bench([&](std::size_t i) {
			const auto& s = text[i % pool_size];
			return classic.decode(s.data(), s.size(), bin.data()) + bin[0];
		});
		double d_lb = none, d_fd = none;
#ifdef HAVE_LIBBASE58
		d_lb = bench([&](std::size_t i) {
			const auto& s = text[i % pool_size];
			std::size_t sz = size;
			b58tobin(bin.data(), &sz, s.data(), s.size());
			return sz + bin[0];
		});
#endif
#ifdef HAVE_FD_BASE58
		if (fd) {
			d_fd = bench([&](std::size_t i) {
				const auto& s = text[i % pool_size];
				if (size == 32) fd_base58_decode_32(s.c_str(), bin.data()); else fd_base58_decode_64(s.c_str(), bin.data());
				return static_cast<std::size_t>(bin[0]);
			});
		}
#endif
		auto cell = [](double v) {
			char buf[32];
			if (v < 0) std::snprintf(buf, sizeof buf, "%9s", "-"); else std::snprintf(buf, sizeof buf, "%9.1f", v);
			return std::string(buf);
		};
		std::printf("%-8zu | %s %s %s %s %s | %s %s %s %s\n", size, cell(e_bx).c_str(), cell(e_re).c_str(), cell(e_cl).c_str(),
			cell(e_lb).c_str(), cell(e_fd).c_str(), cell(d_bx).c_str(), cell(d_cl).c_str(), cell(d_lb).c_str(), cell(d_fd).c_str());
		std::fflush(stdout);
	}
}
