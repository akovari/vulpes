# Vulpes contributor instructions

These instructions apply to the whole repository.

## Start here

- Read `README.md`, `docs/architecture.md`, `docs/development.md`, and the
  relevant ADRs before changing a subsystem boundary.
- Treat `TODO.md` as the authoritative delivery checklist. A checkbox records
  verified completion, not intent or partial implementation.
- Preserve ordinary SQLite compatibility and the separation between database,
  dataset/application semantics, semantic UI, virtual screen, and terminal
  backend layers.

## Development loop

- On Windows, use `./scripts/dev.ps1` for the routine incremental Debug loop.
  Its default `check` task configures, builds, checks formatting, and runs tests.
- Use `./scripts/dev.ps1 build` while iterating and
  `./scripts/dev.ps1 test -CTestRegex <pattern>` for a focused CTest run.
- Do not run a Release build during ordinary local iteration. Use
  `./scripts/dev.ps1 release` only for release/milestone verification or when a
  user explicitly requests it.
- Run `./scripts/dev.ps1 format` after C/C++ edits. Run the `tidy` task for a
  coherent implementation tranche before committing when practical.
- Keep CMake presets usable directly on Linux and macOS; do not hide required
  build behavior exclusively in the Windows wrapper.

## Code and tests

- Use C++23, RAII, explicit ownership, and modest readable abstractions.
- Keep raw SQLite handles inside `src/db` and terminal-library/platform details
  inside terminal adapters. Widgets must not execute database operations.
- Use stable semantic action/message identifiers. Localize human-facing text at
  presentation boundaries; never use translated text as program identity.
- Add deterministic tests for behavior and regressions. Prefer in-memory or
  temporary-copy databases, logical screen-cell assertions, and fake terminals.
- Update architecture, user, localization, and operational documentation when
  behavior or policy changes.
- Never mark a platform/manual verification task complete using only a unit test
  or an untested inference. Record the actual host evidence.

## Worktree and delivery

- Preserve unrelated user changes and untracked databases. In particular, do
  not stage, modify, or delete a root-level `inventory.db` unless explicitly
  requested.
- Do not commit generated build trees, local settings, temporary databases, or
  credentials.
- Keep commits reviewable and scoped. Before each commit, inspect the staged
  diff, run the proportionate Debug checks, update `TODO.md`, and push the commit
  when the active request asks for pushed changes.
