#!/usr/bin/env python3
"""Mutation testing for a header-only library: does the test suite notice a broken header?

    mutate.py SET [NAME ...]         run the mutants of tools/mutants/SET.txt (or only those named)
    mutate.py --list SET             print each mutant and the tests that should kill it
    mutate.py --check SET            only check that every search string matches, without building

A SET is a file of mutants (tools/mutants/SET.txt, or a path). Each mutant is one or more edits to
the library's header, applied to a temporary copy of the repository: CMakeLists.txt, test/, examples/
and the headers at the top level are copied, the edited header replaces the original, and CMake
configures and builds a fresh tree from the copy, so the tests are the ones CTest runs, with their
own flags and arguments. A mutant is killed when one of its tests fails (a nonzero exit, a failed
check, a crash or a timeout), and survives when all of them pass. A survivor is a test that does not
guard the code the edit broke, or an edit that does not change what the code computes.

The mutants file format is documented in docs/MUTATION.md. Run the tool under `nice -n 19`; it runs
at most --jobs mutants at a time, each building one target at a time.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import dataclasses
import glob
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
SETS = TOOLS / "mutants"
# What a copy of the repository holds: enough for CMake to configure and build the tests.
COPY = ("CMakeLists.txt", "test", "examples", "*.hh")
# Output lines worth showing as the reason a test failed, most telling first, and CTest's own
# lines, which never are.
REASONS = (
    "Assertion",
    "runtime error",
    "FAIL",
    "ERROR",
    "error:",
    "Sanitizer",
    "Timeout",
    "Abort",
    "Segmentation",
)
BOILERPLATE = (
    "The following tests",
    "tests passed",
    "Errors while running",
    "Total Test time",
    "Test project",
    "Test #",
)
STATUSES = ("killed", "survived", "equivalent", "did-not-apply", "build-error")


class FormatError(Exception):
    """A mutants file that does not follow the format."""


@dataclasses.dataclass
class Edit:
    """One replacement: every occurrence of a string or regex becomes the replacement text."""

    pattern: str
    regex: bool
    repl: str = ""
    count: int = 1
    line: int = 0


@dataclasses.dataclass
class Mutant:
    name: str
    line: int
    what: str = ""
    edits: list[Edit] = dataclasses.field(default_factory=list)
    tests: list[str] = dataclasses.field(default_factory=list)
    equivalent: str = ""


@dataclasses.dataclass
class MutantSet:
    path: Path
    header: str = ""
    tests: list[str] = dataclasses.field(default_factory=list)
    mutants: list[Mutant] = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class Result:
    """The outcome of one mutant: status is one of the STATUSES."""

    name: str
    status: str
    detail: str = ""


def unescape(text: str, where: str) -> str:
    """Resolve \\n, \\t, \\\\ and \\xHH in a search or replacement string."""
    out: list[str] = []
    i = 0
    while i < len(text):
        c = text[i]
        i += 1
        if c != "\\":
            out.append(c)
            continue
        e = text[i : i + 1]
        i += 1
        if e == "n":
            out.append("\n")
        elif e == "t":
            out.append("\t")
        elif e == "\\":
            out.append("\\")
        elif e == "x" and re.fullmatch(r"[0-9a-fA-F]{2}", text[i : i + 2]):
            out.append(chr(int(text[i : i + 2], 16)))
            i += 2
        else:
            raise FormatError(f"{where}: unknown escape \\{e}")
    return "".join(out)


def parse(path: Path) -> MutantSet:
    """Read a mutants file; docs/MUTATION.md describes the format."""
    result = MutantSet(path)
    mutant: Mutant | None = None
    edit: Edit | None = None
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        where = f"{path}:{number}"
        key, sep, value = line.partition(":")
        key, value = key.strip(), value.strip()
        if not sep:
            raise FormatError(f"{where}: expected 'key: value'")
        if key == "header" and mutant is None:
            result.header = value
        elif key == "tests" and mutant is None:
            result.tests = value.split()
        elif key == "mutant":
            if not re.fullmatch(r"[A-Za-z0-9_.+-]+", value):
                raise FormatError(f"{where}: bad mutant name {value!r}")
            mutant, edit = Mutant(value, number), None
            result.mutants.append(mutant)
        elif mutant is None:
            raise FormatError(f"{where}: '{key}' before the first mutant")
        elif key == "what":
            mutant.what = value
        elif key == "tests":
            mutant.tests = value.split()
        elif key == "equivalent":
            mutant.equivalent = value
        elif key in ("find", "regex"):
            pattern = value if key == "regex" else unescape(value, where)
            if not pattern:
                raise FormatError(f"{where}: empty search string")
            edit = Edit(pattern, key == "regex", line=number)
            mutant.edits.append(edit)
        elif key == "count" and edit is not None:
            edit.count = int(value)
        elif key == "repl" and edit is not None:
            edit.repl = unescape(value, where)
        else:
            raise FormatError(f"{where}: unexpected '{key}'")
    if not result.header:
        raise FormatError(f"{path}: no 'header:' line")
    names = [m.name for m in result.mutants]
    for name in sorted({n for n in names if names.count(n) > 1}):
        raise FormatError(f"{path}: mutant {name} appears twice")
    for m in result.mutants:
        if not m.edits:
            raise FormatError(f"{path}:{m.line}: mutant {m.name} has no find or regex")
        m.tests = m.tests or list(result.tests)
        if not m.tests:
            raise FormatError(f"{path}:{m.line}: mutant {m.name} lists no tests")
    return result


def apply(text: str, mutant: Mutant) -> tuple[str | None, str]:
    """The text with the mutant's edits applied in order, or None and why one does not apply."""
    original = text
    for edit in mutant.edits:
        found = (
            len(re.findall(edit.pattern, text))
            if edit.regex
            else text.count(edit.pattern)
        )
        if found != edit.count:
            shown = edit.pattern.replace("\n", "\\n").replace("\t", "\\t")[:60]
            return (
                None,
                f"line {edit.line}: {found} matches of '{shown}', expected {edit.count}",
            )
        if edit.regex:
            text = re.sub(edit.pattern, lambda _m, r=edit.repl: r, text)
        else:
            text = text.replace(edit.pattern, edit.repl)
    if text == original:
        return None, "the edits leave the header as it was"
    return text, ""


def run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd, cwd=cwd, capture_output=True, text=True, errors="replace", check=False
    )


def reason(output: str) -> str:
    """The line of a failed build or test that says why, shortened."""
    lines = [
        re.sub(r"\S*/src/", "", ln.strip()) for ln in output.splitlines() if ln.strip()
    ]
    lines = [
        ln
        for ln in lines
        if not any(ln.startswith(b) or b in ln[:40] for b in BOILERPLATE)
    ]
    for word in REASONS:
        for ln in lines:
            if word in ln:
                return ln[:120]
    return lines[-1][:120] if lines else ""


def native_flags() -> list[str]:
    """CMake flags that build for the machine's own architecture on an Apple Silicon Mac.

    A translated (x86_64) interpreter hands that architecture to the programs it starts, and CMake
    would then build x86_64 code, which leaves out the NEON paths and runs under translation.
    """
    if sys.platform != "darwin":
        return []
    done = run(["sysctl", "-n", "sysctl.proc_translated"], ROOT)
    return ["-DCMAKE_OSX_ARCHITECTURES=arm64"] if done.stdout.strip() == "1" else []


class Tree:
    """A copy of the repository with one header replaced, configured and built by CMake."""

    def __init__(self, work: Path, header: str, text: str, opts: argparse.Namespace):
        self.src = work / "src"
        self.build = work / "build"
        self.opts = opts
        self.src.mkdir(parents=True)
        for pattern in COPY:
            for item in glob.glob(str(ROOT / pattern)):
                path = Path(item)
                if path.is_dir():
                    shutil.copytree(path, self.src / path.name)
                else:
                    shutil.copy2(path, self.src / path.name)
        (self.src / header).write_text(text, encoding="utf-8")
        self.targets: dict[str, str] = {}

    def configure(self) -> str:
        """Configure the tree and map each CTest test to its executable's target; the error or ''."""
        cmd = ["cmake", "-S", str(self.src), "-B", str(self.build)]
        if shutil.which("ninja"):
            cmd += ["-G", "Ninja"]
        cmd += native_flags()
        cmd += [f"-DCMAKE_BUILD_TYPE={self.opts.build_type}"]
        if self.opts.cxx_flags:
            cmd += [f"-DCMAKE_CXX_FLAGS={self.opts.cxx_flags}"]
        cmd += [f"-D{d}" for d in self.opts.define]
        done = run(cmd, self.src)
        if done.returncode:
            return reason(done.stdout + done.stderr)
        # CTest names an executable that is not built yet only in CTestTestfile.cmake.
        listing = (self.build / "CTestTestfile.cmake").read_text(encoding="utf-8")
        for name, exe in re.findall(
            r'^add_test\(([^\s"]+) "([^"]+)"', listing, re.MULTILINE
        ):
            self.targets[name] = Path(exe).name
        return ""

    def test(self, name: str) -> tuple[str, str]:
        """Build and run one CTest test: ('pass' | 'fail' | 'build', detail)."""
        built = run(
            ["cmake", "--build", ".", "--target", self.targets[name]], self.build
        )
        if built.returncode:
            return "build", reason(built.stdout + built.stderr)
        ran = run(
            [
                "ctest",
                "-R",
                f"^{re.escape(name)}$",
                "--output-on-failure",
                "--timeout",
                str(self.opts.timeout),
            ],
            self.build,
        )
        if ran.returncode == 0:
            return "pass", ""
        return "fail", reason(ran.stdout + ran.stderr)


def run_mutant(
    mset: MutantSet, mutant: Mutant, header: str, work: Path, opts: argparse.Namespace
) -> Result:
    text, why = apply(header, mutant)
    if text is None:
        return Result(mutant.name, "did-not-apply", why)
    tree = Tree(work / mutant.name, mset.header, text, opts)
    error = tree.configure()
    if error:
        return Result(mutant.name, "build-error", f"configure: {error}")
    for name in mutant.tests:
        state, detail = tree.test(name)
        if state == "build":
            return Result(mutant.name, "build-error", f"{name}: {detail}")
        if state == "fail":
            return Result(mutant.name, "killed", f"{name}: {detail}")
    return Result(
        mutant.name,
        "equivalent" if mutant.equivalent else "survived",
        mutant.equivalent,
    )


def baseline_test(
    mset: MutantSet, name: str, header: str, work: Path, opts: argparse.Namespace
) -> str:
    """Build and run one test on the unmodified header in its own tree; the failure, or ''."""
    tree = Tree(work / f"baseline-{name}", mset.header, header, opts)
    error = tree.configure()
    if error:
        return f"configure: {error}"
    state, detail = tree.test(name)
    shutil.rmtree(work / f"baseline-{name}", ignore_errors=True)
    return (
        "" if state == "pass" else f"{name} {state}s on the unmodified header: {detail}"
    )


def baseline(
    mset: MutantSet,
    mutants: list[Mutant],
    header: str,
    work: Path,
    opts: argparse.Namespace,
) -> str:
    """Check that the tests the mutants name exist and pass on the unmodified header; the failure, or ''."""
    wanted = sorted({t for m in mutants for t in m.tests})
    probe = Tree(work / "baseline", mset.header, header, opts)
    error = probe.configure()
    shutil.rmtree(work / "baseline", ignore_errors=True)
    if error:
        return f"configure: {error}"
    unknown = [t for t in wanted if t not in probe.targets]
    if unknown:
        return f"no such CTest test: {' '.join(unknown)}"
    if opts.baseline:
        with concurrent.futures.ThreadPoolExecutor(opts.jobs) as pool:
            failures = pool.map(
                lambda t: baseline_test(mset, t, header, work, opts), wanted
            )
            return next((f for f in failures if f), "")
    return ""


def summary(results: list[Result]) -> str:
    count = {s: sum(r.status == s for r in results) for s in STATUSES}
    survived = count["survived"] + count["equivalent"]
    return (
        f"{len(results)} mutants: {count['killed']} killed, {survived} survived "
        f"({count['equivalent']} argued equivalent), {count['did-not-apply']} did not apply, "
        f"{count['build-error']} build errors"
    )


def find_set(name: str) -> Path:
    for candidate in (Path(name), SETS / f"{name}.txt"):
        if candidate.is_file():
            return candidate
    sets = ", ".join(sorted(p.stem for p in SETS.glob("*.txt")))
    sys.exit(f"mutate.py: no mutants file {name} (the sets are {sets})")


def main() -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    ap.add_argument("set", help="a mutants file, or the name of one in tools/mutants")
    ap.add_argument("names", nargs="*", help="run only these mutants")
    ap.add_argument(
        "--list", action="store_true", help="print the mutants and their tests"
    )
    ap.add_argument(
        "--check", action="store_true", help="only check that the edits apply"
    )
    ap.add_argument(
        "-j", "--jobs", type=int, default=4, help="mutants at a time (default 4)"
    )
    ap.add_argument(
        "--build-type",
        default="Debug",
        help="CMAKE_BUILD_TYPE (default Debug, the light tests)",
    )
    ap.add_argument("--cxx-flags", default="-O1", help="CMAKE_CXX_FLAGS (default -O1)")
    ap.add_argument(
        "--define",
        action="append",
        default=[],
        metavar="VAR=VALUE",
        help="another -D for CMake",
    )
    ap.add_argument(
        "--timeout", type=int, default=600, help="seconds a test may run (default 600)"
    )
    ap.add_argument(
        "--no-baseline",
        dest="baseline",
        action="store_false",
        help="skip the run on the unmodified header",
    )
    ap.add_argument(
        "--work", type=Path, help="directory for the builds (default a temporary one)"
    )
    ap.add_argument("--keep", action="store_true", help="keep the builds")
    opts = ap.parse_args()

    try:
        mset = parse(find_set(opts.set))
    except FormatError as e:
        sys.exit(f"mutate.py: {e}")
    unknown = [n for n in opts.names if n not in {m.name for m in mset.mutants}]
    if unknown:
        sys.exit(f"mutate.py: no such mutant: {' '.join(unknown)}")
    mutants = [m for m in mset.mutants if not opts.names or m.name in opts.names]
    if opts.list:
        for m in mutants:
            print(
                f"{m.name:32} {' '.join(m.tests)}"
                + (f"  (equivalent: {m.equivalent})" if m.equivalent else "")
            )
            print(f"    {m.what}")
        return 0
    header = (ROOT / mset.header).read_text(encoding="utf-8")

    if opts.check:
        bad = 0
        for m in mutants:
            text, why = apply(header, m)
            if text is None:
                bad += 1
                print(f"{m.name:32} DID NOT APPLY  {why}")
        print(f"{len(mutants)} mutants, {bad} do not apply")
        return int(bad > 0)

    work = opts.work or Path(tempfile.mkdtemp(prefix="mutate-"))
    work.mkdir(parents=True, exist_ok=True)
    results: list[Result] = []
    try:
        failure = baseline(mset, mutants, header, work, opts)
        if failure:
            sys.exit(f"mutate.py: baseline: {failure}")
        with concurrent.futures.ThreadPoolExecutor(opts.jobs) as pool:
            futures = [
                pool.submit(run_mutant, mset, m, header, work, opts) for m in mutants
            ]
            for future in concurrent.futures.as_completed(futures):
                r = future.result()
                results.append(r)
                label = {
                    "killed": "KILLED",
                    "survived": "SURVIVED",
                    "equivalent": "survived (equivalent)",
                }.get(r.status, r.status.upper().replace("-", " "))
                print(f"{r.name:32} {label:22} {r.detail}", flush=True)
                if not opts.keep:
                    shutil.rmtree(work / r.name, ignore_errors=True)
    finally:
        if not opts.keep and not opts.work:
            shutil.rmtree(work, ignore_errors=True)
    print(summary(results))
    return int(
        any(r.status in ("survived", "did-not-apply", "build-error") for r in results)
    )


if __name__ == "__main__":
    sys.exit(main())
