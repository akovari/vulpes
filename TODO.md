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
- [x] Add reviewed `clang-tidy` checks and a static-analysis target.
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
- [x] Prefer keyset pagination for stable unique orderings.
- [x] Add insert/edit modes, dirty tracking, validation, save, cancel, and delete.
- [x] Cover no-primary-key tables and views with explicit capability flags.

## M3 — Terminal and semantic UI foundation

- [x] `ScreenBuffer`, styles, normalized key/resize events, and backend interface.
- [x] Spike Unicode grapheme segmentation/display width and record an ADR.
- [x] Implement deterministic screen diffing independent of byte encoding.
- [x] Implement `TestTerminal`, a pure ANSI encoder, and an isolated native
  `ConsoleTerminal` adapter for browse navigation.
- [x] Adopt CPP-Terminal behind the `Terminal` adapter with deterministic
  normalization tests for keyboard chords and resize events.
- [ ] Verify CPP-Terminal keyboard input, resize notifications, and cleanup on
  supported Windows, Linux, and macOS hosts.
- [ ] Handle resize, raw-mode restoration, Ctrl+C, terminal failure, and redirected
  standard streams on all platforms.
- [ ] Complete Windows raw-input verification for Escape, arrows, Ctrl+C, function
  keys, Alt chords, and terminal restoration on Windows Terminal and legacy hosts.
- [ ] Complete Linux/macOS raw-input verification for Escape ambiguity, resize,
  Ctrl+C, UTF-8, and terminal restoration.
- [ ] Implement focus traversal, measure/layout, container, label, button, and
  reusable status-bar primitives.
- [ ] Add theme tokens, high-contrast palette tests, and shortcut/mnemonic
  rendering without hard-coded colors in individual screens.
- [ ] Add terminal capability/redirected-stream diagnostics and a user-facing
  terminal failure screen.
- [x] Map configurable keys to semantic action IDs.

## M4 — First vertical slice: browse

- [x] Implement static grid layout and logical-cell tests.
- [x] Bind grid rows to a read-only dataset.
- [x] Add selected-row state, vertical navigation, and dataset paging.
- [x] Add horizontal grid navigation and scrolling.
- [x] Add command dispatcher with `help`, `tables`, `schema`, `browse`, and `quit`.
- [x] Add safe sorting, filtering, searching, and refresh through typed dataset
  APIs and a reusable prompt widget.
- [x] Add bounded, owned SQL-script result execution for the console boundary.
- [x] Reuse Grid for a multiline SQL console.

## M4.5 — Workspace shell

- [ ] Start without arguments in a keyboard-first workspace shell.
- [ ] Add File, Database, View, Window, and Help menus with stable actions.
- [ ] Open or create SQLite databases through a portable path-entry dialog.
- [ ] Manage active document tabs, modal input, focus, and a status bar.
- [ ] Host browse and SQL-console surfaces as workspace documents.
- [ ] Make File-menu Escape/Left/Right/Alt mnemonic behavior reliable on all
  supported Windows terminal hosts.
- [ ] Implement the Database, View, Window, and Help menus rather than only the
  File-menu prototype.
- [ ] Add document tabs, active-document switching, close confirmation, and a
  tab-local title/status model.
- [ ] Add a command window/palette using the existing command dispatcher.
- [ ] Add recent databases, safe create/open mode selection, read-only indication,
  and persistent workspace preferences.
- [ ] Design an optional directory browser separately from portable path entry.

## Cross-cutting — localization

- [x] Define stable message keys, locale fallback, and named message arguments.
- [x] Use CLI11 for process-level command-line parsing.
- [x] Add external UTF-8 catalog loading and locale selection configuration.
- [ ] Add ICU-backed plural/select message formatting with the first translated
  application catalog.
- [ ] Localize all workspace/menu/status/help text and add Czech catalog coverage.
- [ ] Add locale-aware date, time, number, and currency display policies.

## M5 — Edit workflow and generated forms

- [x] Implement a schema-generated text, number, checkbox, and read-only record
  form with keyboard save/cancel.
- [ ] Add button and dialog widgets, including destructive-action confirmation.
- [x] Add transactional insert/edit/delete handling and destructive delete
  confirmation.
- [x] Map constraint/validation failures to fields without losing edits.
- [x] Infer boolean-like fields conservatively.
- [x] Infer foreign-key display fields (`name`, `title`, `description`, `code`) and
  implement a bounded keyboard relationship lookup.
- [ ] Add searchable, metadata-configurable relationship lookups and related-record
  drill-down.
- [ ] Validate the complete workshop success scenario on all platforms.
- [ ] Add field labels, ordering, hidden/read-only overrides, formats, and lookup
  behavior from optional application metadata.
- [ ] Add date/time display/edit controls only after an explicit SQLite annotation
  policy is defined.

## M6 — Inventory dogfood and 0.1 packaging

- [x] Add generic inventory example schema and low-stock view.
- [x] Add representative seed data and scripted acceptance scenario.
- [ ] Exercise product edit, relationship navigation, stock movement, and report.
- [ ] Fix framework weaknesses without inventory-specific runtime code.
- [ ] Package standalone artifacts for Windows, Linux, and macOS.
- [ ] Document backup, recovery, compatibility, and upgrade behavior.
- [ ] Add versioned installer/archive conventions, checksums, licenses, and release
  notes for each supported platform.

## Post-0.1 — deliberately deferred

- [ ] Define and migrate SQLite-resident `_app_*` metadata tables.
- [ ] Load metadata as enhancements to ordinary SQLite schema introspection.
- [ ] Support metadata-defined form labels, field order, visibility, read-only
  state, formatting, views, commands, menus, reports, and settings.
- [ ] Launch a metadata-defined application from a `.vulpes` SQLite file while
  retaining ordinary SQLite-tool compatibility.

## Reports and export

- [ ] Define named SQL-query report metadata and workspace commands.
- [ ] Render reports through Grid with sorting/navigation shared with browse.
- [ ] Export report/query results to CSV, JSON, plain text, HTML, and PDF.
- [ ] Define encoding, locale, overwrite, and error-handling behavior for exports.

## Lua business logic

- [ ] Select/package Lua and define a deliberately small, safe host API.
- [ ] Add lifecycle hooks: `on_open`, `before_insert`, `before_update`,
  `after_update`, `before_delete`, and `on_command`.
- [ ] Define script storage, error presentation, transaction interaction, and
  testing/sandbox policy.
- [ ] GUI/web renderers, networking, designer, and extension ecosystem only after
  the core browse/edit workflow is demonstrably strong.
