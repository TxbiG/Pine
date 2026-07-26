#!/usr/bin/env python3
"""End-to-end Pine regression runner."""

from __future__ import annotations

import argparse
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(command, *, expect=0, stdout=None):
    result = subprocess.run(
        [str(part) for part in command],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE if stdout is None else stdout,
        stderr=subprocess.PIPE,
    )
    if (result.returncode == expect):
        return result
    raise AssertionError(
        f"command returned {result.returncode}, expected {expect}: {' '.join(map(str, command))}\n"
        f"stdout:\n{result.stdout or ''}\nstderr:\n{result.stderr}"
    )


def compare(command, expected: Path):
    result = run(command)
    actual = result.stdout.replace("\r\n", "\n")
    wanted = expected.read_text(encoding="utf-8").replace("\r\n", "\n")
    if actual != wanted:
        raise AssertionError(f"snapshot mismatch for {expected}\n--- expected ---\n{wanted}\n--- actual ---\n{actual}")


def find_c_compiler():
    configured = os.environ.get("CC")
    if configured:
        return configured
    for name in ("cc", "gcc", "clang"):
        path = shutil.which(name)
        if path:
            return path
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pine", required=True, type=Path)
    args = parser.parse_args()
    pine = args.pine.resolve()

    for example in sorted((ROOT / "examples").glob("*.pine")):
        run([pine, "transpile", example])

    feature = ROOT / "tests/cases/core_features.pine"
    generated = ""
    for positive in sorted((ROOT / "tests/cases").glob("*.pine")):
        if positive.name.startswith("invalid_"):
            continue
        result = run([pine, "transpile", positive])
        run([pine, "check", positive])
        run([pine, "ir", positive])
        for target in ("x86_64", "aarch64", "riscv64"):
            native = run([pine, "native", positive, "--target", target])
            if f"target {target}\n" not in native.stdout:
                raise AssertionError(f"native output did not select {target}")
        if positive == feature:
            generated = result.stdout

    for invalid in sorted((ROOT / "tests/cases").glob("invalid_*.pine")):
        result = subprocess.run([pine, "transpile", invalid], cwd=ROOT)
        if result.returncode == 0:
            raise AssertionError(f"invalid program unexpectedly succeeded: {invalid}")

    targets = run([pine, "targets"]).stdout
    for target in ("x86_64", "aarch64", "riscv64"):
        if target not in targets:
            raise AssertionError(f"missing target from `pine targets`: {target}")
    alias = run([pine, "native", feature, "--target=arm64"])
    if "target aarch64\n" not in alias.stdout:
        raise AssertionError("arm64 alias did not select aarch64")
    run([pine, "native", feature, "--target", "mips64"], expect=1)

    handoff = ROOT / "examples/native_ir_handoff.pine"
    compare([pine, "ir", handoff], ROOT / "examples/native_ir_handoff.expected.ir")
    compare([pine, "native", handoff], ROOT / "examples/native_ir_handoff.expected.native")

    compiler = find_c_compiler()
    if compiler:
        with tempfile.TemporaryDirectory(prefix="pine-tests-") as directory:
            c_file = Path(directory) / "core_features.c"
            exe_file = Path(directory) / ("core_features.exe" if os.name == "nt" else "core_features")
            c_file.write_text(generated, encoding="utf-8")
            run([compiler, c_file, "-o", exe_file])
            run([exe_file])

    print("Pine regression tests passed")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
