#!/usr/bin/env python3
"""Run the repository formatter with portable local-tool discovery."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys


def _configured_formatter(repository: Path) -> Path | None:
    for cache in (
        repository / "build" / "windows-msvc" / "CMakeCache.txt",
        repository / "build" / "windows-tidy" / "CMakeCache.txt",
    ):
        if not cache.is_file():
            continue
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            prefix = "VULPES_CLANG_FORMAT:FILEPATH="
            if line.startswith(prefix):
                candidate = Path(line.removeprefix(prefix))
                if candidate.is_file():
                    return candidate
    return None


def _visual_studio_formatter() -> Path | None:
    installations: list[Path] = []
    configured = os.environ.get("VSINSTALLDIR")
    if configured:
        installations.append(Path(configured))

    vswhere_candidates = []
    for variable in ("ProgramFiles(x86)", "ProgramFiles"):
        program_files = os.environ.get(variable)
        if program_files:
            vswhere_candidates.append(
                Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
            )
    for vswhere in vswhere_candidates:
        if not vswhere.is_file():
            continue
        result = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode == 0 and result.stdout.strip():
            installations.append(Path(result.stdout.strip()))
            break

    for installation in installations:
        candidate = installation / "VC" / "Tools" / "Llvm" / "x64" / "bin" / "clang-format.exe"
        if candidate.is_file():
            return candidate
    return None


def find_formatter(repository: Path) -> Path:
    override = os.environ.get("VULPES_CLANG_FORMAT")
    if override:
        candidate = Path(override)
        if candidate.is_file():
            return candidate
        raise RuntimeError(f"VULPES_CLANG_FORMAT does not name a file: {candidate}")

    for name in ("clang-format", "clang-format-22"):
        candidate = shutil.which(name)
        if candidate:
            return Path(candidate)

    candidate = _configured_formatter(repository)
    if candidate:
        return candidate
    if os.name == "nt":
        candidate = _visual_studio_formatter()
        if candidate:
            return candidate

    raise RuntimeError(
        "clang-format was not found. Install it, configure Vulpes once, or set "
        "VULPES_CLANG_FORMAT to its executable path."
    )


def main(arguments: list[str]) -> int:
    if not arguments:
        return 0
    repository = Path(__file__).resolve().parent.parent
    try:
        formatter = find_formatter(repository)
    except RuntimeError as error:
        print(f"clang-format hook: {error}", file=sys.stderr)
        return 1

    # Keep below Windows' process command-line limit while preserving a single
    # deterministic formatter invocation for each bounded group.
    batch: list[str] = []
    batch_length = 0
    for filename in arguments:
        if batch and batch_length + len(filename) + 1 > 24_000:
            result = subprocess.run([str(formatter), "-i", "--style=file", *batch], check=False)
            if result.returncode != 0:
                return result.returncode
            batch = []
            batch_length = 0
        batch.append(filename)
        batch_length += len(filename) + 1
    return subprocess.run([str(formatter), "-i", "--style=file", *batch], check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
