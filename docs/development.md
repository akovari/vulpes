# Development

## Workflow

1. On Windows, run `.\scripts\dev.ps1`; it defaults to the incremental Debug
   configure/build, formatting check, and test loop.
2. Use `.\scripts\dev.ps1 build` for quick compile iterations and
   `.\scripts\dev.ps1 test -CTestRegex <pattern>` for focused CTest runs.
3. Keep changes within one architectural boundary when practical.
4. Add unit tests for every core behavior and regression.
5. Update `TODO.md` when scope or sequencing changes.

The wrapper discovers and initializes the newest Visual Studio x64 toolchain,
bundled vcpkg, and LLVM formatter/analyzer when necessary. It delegates to the
checked-in CMake presets; it is not an alternative build definition. Linux and
macOS contributors use the corresponding presets directly.

Routine local development is Debug-only. Run `.\scripts\dev.ps1 release` for a
release or milestone gate, not after every edit. Use `-Fresh` only when CMake's
generated graph really needs rebuilding; ordinary invocations remain
incremental.

Repository-wide contributor and agent invariants live in `AGENTS.md`.

## Formatting

`.clang-format` is the canonical style definition. It uses `clang-format` to
enforce layout and regroup includes as project headers, other quoted headers, and
system/third-party headers. Do not hand-sort includes.

Run `clang-format -i --style=file <files>` to format selected files. When
`clang-format` is on `PATH` during CMake configuration, use the `format` and
`format-check` targets instead. Visual Studio 2026 includes `clang-format`;
its x64 executable is under `VC\Tools\Llvm\x64\bin` in a standard install.

The `Format` GitHub workflow uses clang-format directly on every push and pull
request. It does not run clang-tidy: static analysis needs a configured
compilation database and is covered by the dedicated target below.

### Static analysis

`.clang-tidy` defines a deliberately narrow reviewed baseline: correctness and
performance checks that do not impose stylistic churn or a project-wide
ownership model prematurely. CMake exposes it as the `tidy` target when
`clang-tidy` is available:

```powershell
cmake --preset windows-tidy
cmake --build --preset windows-tidy
cmake --build build/windows-tidy --target tidy
```

The dedicated single-config preset avoids multi-config compilation-database
entries that refer to configuration-specific compiler module files. It is kept
separate from normal builds so contributors can upgrade or review its policy
explicitly.

### Pre-commit hook

The repository includes a [pre-commit](https://pre-commit.com/) configuration
that applies `clang-format` only to staged C/C++ source files. It updates the
working tree and stops the commit when formatting changes were made; review and
stage those changes, then commit again. CI still rejects unformatted code, so
the local hook is a fast feedback mechanism rather than the only safeguard.

Install Python and the `pre-commit` command, ensure `clang-format` is on `PATH`,
then run:

```powershell
python -m pip install --user pre-commit
pre-commit install
pre-commit run --all-files
```

On a standard Visual Studio 2026 installation, add
`$env:ProgramFiles\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin`
to `PATH` for the current shell if `clang-format` is not already available.
The hook deliberately uses the local formatter rather than downloading an
unrelated toolchain, which keeps its behavior aligned with CMake and the IDE.

Enable `VULPES_WARNINGS_AS_ERRORS` in CI once all supported compiler versions
produce a clean and stable warning set.

## Versioning

Use annotated or lightweight tags named `vMAJOR.MINOR.PATCH`. CMake converts
the nearest tag, commit distance, abbreviated commit, and dirty state into the
generated runtime version; see ADR 0017. Do not edit the generated header or
introduce a separate release-script version. Source-archive builds without a
`.git` directory may pass `-DVULPES_BUILD_VERSION_OVERRIDE=<version>`.

## Code conventions

- C++23; RAII and explicit ownership.
- No raw SQLite handles outside `src/db`.
- No terminal escape sequences outside terminal backends.
- No database access from widgets.
- Dataset identifiers must originate in `TableSchema`; values must be bound rather
  than interpolated. Free-form SQL is reserved for the SQL console boundary.
- UTF-8 at external text boundaries and `char32_t` for logical screen glyphs.
- Never use human-readable UI text as an identifier. Use stable action/message
  keys and localize at the presentation boundary.
- Exceptions may cross implementation layers, but frontend boundaries convert
  `vulpes::Error` into user-facing structured errors.
- Platform-specific sources must be isolated and selected by CMake.

## Testing

Database tests use `:memory:` unless file behavior is the subject. UI rendering
tests compare logical cells. Terminal integration tests use a fake backend and
normalized events. End-to-end tests must use temporary database copies.

`presentation_matrix_test.cpp` renders the workspace, menus, prompts, directory
browser, browse grid and overlays, generated forms, SQL editor, schema view,
diagnostics, and minimum-size warning at 40x10, 80x25, and 160x45 cells for both
English/Czech and midnight/high-contrast presentation. Windows display scaling
is intentionally represented by the terminal's reported cell dimensions:
Vulpes owns no pixel coordinates or DPI conversion. Physical host verification
remains required for the input/backend items explicitly left unchecked in
`TODO.md`.

CI builds Windows, Ubuntu, and macOS on every push and pull request. Sanitizer,
coverage, fuzz, and static-analysis jobs are tracked in `TODO.md` rather than
being implied by the initial scaffold.
