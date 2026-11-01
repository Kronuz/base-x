# Mutation testing

A test proves a property only if breaking the code it guards makes it fail. `tools/mutate.py` checks
that: it breaks the header one edit at a time, runs the tests that should notice, and reports the
edits no test catches.
[TESTING.md](TESTING.md) lists the tests and the other checks a change passes.

```sh
nice -n 19 tools/mutate.py leading_zeros              # every mutant of tools/mutants/leading_zeros.txt
nice -n 19 tools/mutate.py leading_zeros enc-count-none gate-enc-never   # only those named
tools/mutate.py --list leading_zeros                  # the mutants and the tests that should kill each
tools/mutate.py --check leading_zeros                 # only check that every edit applies; no build
```

It needs Python 3 (standard library only), CMake, a C++17 compiler and, when present, Ninja. CMake
fetches `uinteger_t` (see `CMakeLists.txt`); to build offline, or against another `uinteger_t`, pass a
checkout with `--define FETCHCONTENT_SOURCE_DIR_UINTEGER_T=/path/to/uinteger_t`. The tool runs at most
`-j` mutants at a time (default 4), each building one test at a time, so `nice -n 19` keeps a shared
machine usable. On an Apple Silicon Mac it builds for arm64 even when Python itself is a translated
x86_64 build (a translated Python would otherwise hand CMake the x86_64 architecture).

## How a mutant is run

A mutant is one or more textual edits to `base_x.hh`. The tool copies `CMakeLists.txt`, `test/`,
`examples/` and the headers to a temporary directory, replaces the header with the edited one,
configures a fresh CMake tree there and, for each CTest test the mutant names, builds that test's
executable and runs it through `ctest`. The tests are the ones CI runs, with the flags and arguments
`CMakeLists.txt` gives them. A mutant

- is **killed** when one of its tests fails: a nonzero exit, a failed check, a sanitizer report, a
  crash or a timeout (the first failing test stops the mutant);
- **survives** when all of its tests pass: no test guards what the edit broke, or the edit does not
  change what the code computes (a mutant marked `equivalent:` carries that argument, and its survival
  is expected);
- **does not apply** when a search string does not match the header the expected number of times;
- **does not build** when the edited header or a test fails to compile; the mutant is invalid.

Before the first mutant the tool builds and runs the named tests on the unmodified header, so a kill
means the edit broke something. The exit status is 0 when every mutant not marked `equivalent:` was
killed.

The build is Debug with `-O1`. `--build-type Release` builds with `NDEBUG`, which turns `assert` off
in `test/test.cc`; the golden test checks without it. `--cxx-flags` replaces the flags (for a
sanitizer: `--cxx-flags '-O1 -fsanitize=undefined -fno-sanitize-recover=undefined'`), `--timeout`
bounds a test (600 s) and `--keep --work DIR` keeps the trees to look at a build.

## The mutants files

`tools/mutants/SET.txt` has one file per group of code:

| set | what the mutants break | mutants | killed | survive |
|---|---|---:|---:|---|
| `leading_zeros` | the flag: the count of zero bytes on encode and of zero digits on decode, where the zero digits go, what the checksum and padding cover, and which presets carry it | 39 | 38 | 1 equivalent |
| `simd` | the AArch64 table-lookup scan of decode and map of encode, and the result handling around them | 9 | 9 | |

The counts are from Debug builds with `-O1` (the default) on an Apple M4 Pro, where the `simd` mutants
change compiled code; on another architecture the seven that edit the AArch64 blocks survive. Every
`leading_zeros` mutant that is killed dies in `test/test.cc`. The one equivalent mutant,
`enc-rest-not-skipped`, leaves the zero bytes in the number: its constructor drops them anyway.

A file is lines of `key: value`; blank lines and lines starting with `#` are ignored. Before the first
mutant, `header:` names the file the edits apply to and `tests:` the CTest tests every mutant runs
(its default). Then, per mutant:

```
mutant: enc-count-short
what: encode: the count stops one byte early
find: \t\t\twhile (zeros < decoded_size && !decoded[zeros]) {
repl: \t\t\twhile (zeros + 1 < decoded_size && !decoded[zeros]) {
```

| key | meaning |
|---|---|
| `mutant:` | the name, letters, digits and `_ . + -`; it starts a mutant |
| `what:` | one line on what the edit breaks |
| `find:` | an exact string; it must occur exactly `count` times (default 1) |
| `regex:` | a Python regular expression instead of `find:`, matched on the whole file |
| `count:` | how many matches the preceding `find:` or `regex:` must have |
| `repl:` | the text that replaces every match; a literal, with no group references |
| `tests:` | the CTest tests that should kill it, in the order to run them, instead of the file's |
| `equivalent:` | why it survives, for an edit that does not change what the code computes |

A mutant has one or more `find:` (or `regex:`) and `repl:` pairs, applied in order. `find:` and `repl:`
take `\n`, `\t`, `\\` and `\xHH`; the value is stripped, so write leading or trailing spaces as `\x20`.
Put the cheap tests that kill most mutants first, since the first failure ends the mutant.

## Keeping the sets current

After editing `base_x.hh`, run `--check` on the sets of the code you touched: a search string that no
longer matches once is reported as not applying, and the entry is updated to the new code. After
changing the code a set covers, run it: a survivor is either a test to strengthen or an edit to argue
equivalent. Add a mutant for each branch, flag and boundary you add, with the test that should fail.
A new preset has no mutant of its own: the golden test (`test/golden.cc`) fails on a changed alphabet.
