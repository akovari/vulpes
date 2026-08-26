# Vulpes task list

Checkboxes describe completion, not intent. Items are ordered by dependency and
product risk. A milestone is not complete until its tests and documentation pass
on Windows, Linux, and macOS.

## M0 — Repository foundation

- [x] C++23/CMake project and version command.
- [x] Pinned vcpkg manifest for SQLite and Catch2.
- [x] Windows, Linux, and macOS CI skeleton.
- [x] Architecture, development, scope, and build documentation.
- [ ] Confirm the first CI run on all platforms.
- [x] Add enforced `clang-format` configuration, CMake targets, and CI check.
- [x] Add an opt-in pre-commit hook for staged C/C++ formatting.
- [ ] Add reviewed `clang-tidy` checks and static-analysis presets.
- [ ] Add release metadata generated from Git tags.

## M1 — Safe SQLite foundation

- [x] Movable RAII `Database`, `Statement`, and `Transaction`.
- [x] SQLite `NULL`, integer, real, text, and blob `Value` storage.
- [x] Positional and named parameter binding.
- [x] Basic table/view, column, and foreign-key introspection.
- [x] Add row/name lookup without exposing statement lifetimes.
- [x] Discover unique constraints, indexes, generated/hidden columns, composite
  keys, and foreign-key actions.
- [x] Classify constraint errors and preserve extended SQLite result codes.
- [x] Test blobs, empty values, invalid UTF-8 policy, nested transaction policy,
  busy handling, read-only mode, and move semantics.
- [x] Add safe identifier quoting as an internal shared utility.

## M2 — Dataset/cursor model

- [x] Write an ADR for row identity, stable ordering, and pagination fallback.
- [x] Implement read-only datasets with fields, current row, first/next/previous,
  last, refresh, and bounded paging.
- [x] Implement typed sort/filter/search specifications with bound values; never
  accept unchecked identifier or SQL fragments from UI code.
- [ ] Prefer keyset pagination for stable unique orderings.
- [x] Add insert/edit modes, dirty tracking, validation, save, cancel, and delete.
- [x] Cover no-primary-key tables and views with explicit capability flags.

## M3 — Terminal and semantic UI foundation

- [x] `ScreenBuffer`, styles, normalized key/resize events, and backend interface.
- [x] Spike Unicode grapheme segmentation/display width and record an ADR.
- [x] Implement deterministic screen diffing independent of byte encoding.
- [x] Implement `TestTerminal`, a pure ANSI encoder, and an isolated native
  `ConsoleTerminal` adapter for browse navigation.
- [ ] Handle resize, raw-mode restoration, Ctrl+C, terminal failure, and redirected
  standard streams on all platforms.
- [ ] Implement focus, measure/layout, container, label, and status bar.
- [ ] Map configurable keys to semantic action IDs.

## M4 — First vertical slice: browse

- [x] Implement static grid layout and logical-cell tests.
- [x] Bind grid rows to a read-only dataset.
- [x] Add selected-row state, vertical navigation, and dataset paging.
- [x] Add horizontal grid navigation and scrolling.
- [x] Add command dispatcher with `help`, `tables`, `schema`, `browse`, and `quit`.
- [ ] Add safe sorting, filtering, searching, and refresh.
- [ ] Reuse Grid for a multiline SQL console.

## Cross-cutting — localization

- [x] Define stable message keys, locale fallback, and named message arguments.
- [x] Use CLI11 for process-level command-line parsing.
- [x] Add external UTF-8 catalog loading and locale selection configuration.
- [ ] Add ICU-backed plural/select message formatting with the first translated
  application catalog.

## M5 — Edit workflow and generated forms

- [ ] Implement text, number, checkbox, button, dialog, and generated record form.
- [ ] Add insert/edit/delete confirmation and transaction boundaries.
- [ ] Map constraint/validation failures to fields without losing edits.
- [ ] Infer boolean-like fields conservatively.
- [ ] Infer foreign-key display fields (`name`, `title`, `description`, `code`) and
  implement a relationship lookup.
- [ ] Validate the complete workshop success scenario on all platforms.

## M6 — Inventory dogfood and 0.1 packaging

- [x] Add generic inventory example schema and low-stock view.
- [ ] Add representative seed data and scripted acceptance scenario.
- [ ] Exercise product edit, relationship navigation, stock movement, and report.
- [ ] Fix framework weaknesses without inventory-specific runtime code.
- [ ] Package standalone artifacts for Windows, Linux, and macOS.
- [ ] Document backup, recovery, compatibility, and upgrade behavior.

## Post-0.1 — deliberately deferred

- [ ] Metadata schema and application mode.
- [ ] Named reports and export formats.
- [ ] Deliberate Lua API and lifecycle hooks.
- [ ] GUI/web renderers, networking, designer, and extension ecosystem only after
  the core browse/edit workflow is demonstrably strong.
