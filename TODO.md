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
- [ ] Add formatting (`clang-format`) and static analysis presets.
- [ ] Add release metadata generated from Git tags.

## M1 — Safe SQLite foundation

- [x] Movable RAII `Database`, `Statement`, and `Transaction`.
- [x] SQLite `NULL`, integer, real, text, and blob `Value` storage.
- [x] Positional and named parameter binding.
- [x] Basic table/view, column, and foreign-key introspection.
- [ ] Add row/name lookup without exposing statement lifetimes.
- [ ] Discover unique constraints, indexes, generated/hidden columns, composite
  keys, and foreign-key actions.
- [ ] Classify constraint errors and preserve extended SQLite result codes.
- [ ] Test blobs, empty values, invalid UTF-8 policy, nested transaction policy,
  busy handling, read-only mode, and move semantics.
- [ ] Add safe identifier quoting as an internal shared utility.

## M2 — Dataset/cursor model

- [ ] Write an ADR for row identity, stable ordering, and pagination fallback.
- [ ] Implement read-only datasets with fields, current row, first/next/previous,
  last, refresh, and bounded paging.
- [ ] Implement typed sort/filter/search specifications with bound values; never
  accept unchecked identifier or SQL fragments from UI code.
- [ ] Prefer keyset pagination for stable unique orderings.
- [ ] Add insert/edit modes, dirty tracking, validation, save, cancel, and delete.
- [ ] Cover no-primary-key tables and views with explicit capability flags.

## M3 — Terminal and semantic UI foundation

- [x] `ScreenBuffer`, styles, normalized key/resize events, and backend interface.
- [ ] Spike Unicode grapheme segmentation/display width and record an ADR.
- [ ] Implement deterministic screen diffing independent of byte encoding.
- [ ] Implement `TestTerminal`, then ANSI and Windows backends.
- [ ] Handle resize, raw-mode restoration, Ctrl+C, terminal failure, and redirected
  standard streams on all platforms.
- [ ] Implement focus, measure/layout, container, label, and status bar.
- [ ] Map configurable keys to semantic action IDs.

## M4 — First vertical slice: browse

- [ ] Implement static grid layout and logical-cell tests.
- [ ] Bind grid rows to a read-only dataset.
- [ ] Add selection, vertical/horizontal scrolling, and paging.
- [ ] Add command dispatcher with `help`, `tables`, `schema`, `browse`, and `quit`.
- [ ] Add safe sorting, filtering, searching, and refresh.
- [ ] Reuse Grid for a multiline SQL console.

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
