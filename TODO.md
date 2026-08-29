# Vulpes remaining delivery roadmap

This is a forward-looking checklist: completed work is intentionally omitted.
Use the source tree, ADRs, release notes, and Git history for completed design
and implementation evidence. A task is checked only after its stated outcome
has been implemented, tested at the appropriate boundary, and documented.

## M0 — 0.1 platform and release gates

- [ ] Confirm successful CI runs on Windows, Linux, and macOS using the
  supported compiler matrix.
- [ ] Verify CPP-Terminal input, resize notifications, raw-mode restoration,
  Ctrl+C cleanup, and initialized-terminal failure handling on a real host for
  every supported platform.
- [ ] Complete Windows input verification on Windows Terminal and a supported
  legacy console host: Escape, arrows, function keys, Alt mnemonics, Ctrl+C,
  resize, Unicode input, and terminal restoration.
- [ ] Complete Linux and macOS input verification in supported local terminals
  and SSH: Escape ambiguity, arrows, function keys, Ctrl+C, resize, Unicode
  input, and terminal restoration.
- [ ] Validate the complete workshop success scenario through the interactive
  UI on Windows, Linux, and macOS.
- [ ] Build, extract, and exercise a tagged standalone Release archive outside
  the build tree on every supported platform.
- [ ] Implement and independently verify Windows installer/signing, macOS
  installer/notarization, and Linux archive/package-signing flows with
  production credentials and clean native-host evidence.

## M7 — FoxPro-style workspace and data environment

M7 strengthens the core database-development workflow before the visual RAD
builder. Every definition remains semantic and frontend-independent: no
terminal coordinates, ANSI sequences, or backend-specific events may enter
application metadata.

- [ ] Write end-to-end acceptance scenarios for creating a blank `.vulpes`
  application, defining data, building a form/menu/report, entering records,
  and packaging the result without direct `_app_*` edits.
- [ ] Persist named, compound saved filters through application/view metadata,
  retaining typed bound values, clear ownership, and safe loading validation.
- [ ] Add persistent browse/view definitions for field order, widths, frozen
  fields, calculated fields, sort/filter/search defaults, formats, and saved
  filters.
- [ ] Turn the workspace document model into a keyboard-first window manager:
  movable/resizable semantic windows, activate/next/previous/window-list
  commands, cascade/tile arrangements, reliable z-order, and modal ownership.
- [ ] Persist and restore an interrupted workspace session: open documents,
  active database, window arrangement, and recoverable unsaved drafts.
- [ ] Provide a visual database explorer for tables, views, indexes, foreign
  keys, metadata definitions, reports, scripts, commands, and screens.
- [ ] Add a safe schema designer for tables, columns, defaults, constraints,
  indexes, and foreign keys, backed by transactional migration plans, previews,
  backups, explicit destructive-change confirmation, and rollback guidance.
- [ ] Add a query/view designer that produces inspectable SQLite SQL, supports
  joins, filters, ordering, grouping, and parameters, and distinguishes
  read-only result queries from editable table datasets.
- [ ] Define a semantic data-environment model for forms and screens: named
  datasets, aliases, relationships, query parameters, ordering, refresh rules,
  and lifecycle ownership.
- [ ] Bind forms, screens, and reports through the data environment rather than
  direct table names, while retaining generated-schema defaults for ordinary
  SQLite databases.
- [ ] Add a data-environment editor for datasets, relationships, lookup display
  fields, and refresh rules without raw metadata SQL.
- [ ] Implement CSV and JSON import workflows with type preview, mapping,
  validation/error reporting, transaction policy, and deterministic tests.
- [ ] Complete visual, keyboard, localization, and high-contrast review at
  40x10, 80x25, and wide terminals for English and Czech, including real
  Windows display-scaling evidence.

## M8 — RAD application builder

M8 makes Vulpes capable of authoring a useful local application from within
Vulpes. SQL remains an expert path, but authors must not need to create or edit
reserved metadata rows by hand.

- [ ] Add an application/project manager to create, open, upgrade, validate,
  clone, back up, restore, and inspect `.vulpes` applications while preserving
  ordinary SQLite compatibility.
- [ ] Add application-settings editing for title, locale, theme, startup
  command, semantic key bindings, and versioned metadata migrations.
- [ ] Add screen/dashboard management: create, rename, duplicate, delete, set
  startup, and navigate named semantic screens without coordinate metadata.
- [ ] Add screen/dashboard design with semantic groups, summaries, navigation,
  actions, responsive layout hints, preview, undo/redo, and validation.
- [ ] Add form management: create generated forms, select a table/data source,
  choose a default form, duplicate/rename/delete forms, and validate references
  before saving.
- [ ] Build a semantic form designer with preview and undo/redo. It must edit
  field order, groups/sections, labels, help text, visibility, read-only state,
  widths, formats, tabs, and responsive layout hints rather than persist TUI
  coordinates.
- [ ] Expand form controls deliberately: multiline text, date/time, currency,
  checkbox, lookup/combo/list, radio group, command button, calculated display,
  and read-only summary fields, each with schema/metadata mapping and tests.
- [ ] Add a validation-rule editor for required/range/pattern/list/cross-field
  rules, localized user-facing messages, and a clear boundary between declarative
  validation and Lua business logic.
- [ ] Add command-button and form/screen-action editing using stable semantic
  action identifiers; support save/cancel/new/delete/navigate/open-form/
  open-screen/run-command without terminal key details in metadata.
- [ ] Add view management for named browses, including field selection,
  presentation, parameters, read-only policy, default navigation targets, and
  all M7 Grid capabilities.
- [ ] Add menu and command editors with ordering, separators, localized labels,
  explicit mnemonics, enabled-state validation, shortcut assignment, and cycle
  detection.
- [ ] Add report management for named SQL reports, parameters, result limits,
  titles, export defaults, and application-menu/command integration.
- [ ] Add a visual report-layout designer with semantic bands, headers, detail
  fields, grouping, totals, page settings, print/preview output, and export
  mappings; keep Grid reports as the simple default.
- [ ] Add application preview/run mode that hides authoring controls, supports
  returning to design mode, and never mutates data merely by previewing.
- [ ] Provide metadata validation, reference diagnostics, safe repair/migration
  operations, and a human-readable application manifest/export for review and
  version control.
- [ ] Dogfood the builder by recreating the inventory application exclusively
  through Vulpes, then compare its metadata and workflows against the scripted
  example.

## M9 — Application development and operations tools

- [ ] Add a Lua script manager with create/rename/delete/reorder/enable actions,
  hook scopes, source validation, trusted-code warnings, and metadata-reference
  diagnostics.
- [ ] Add a Unicode-safe Lua editor with syntax highlighting, search/replace,
  go-to diagnostic, undo/redo, and unsaved-change recovery.
- [ ] Add a deterministic hook test runner and error navigator that expose hook
  context, transaction outcome, source location, and retained record drafts.
- [ ] Design a debugger only after the editor and test runner prove useful;
  document breakpoint, stepping, inspection, security, and cross-frontend
  semantics before implementation.
- [ ] Add application-level error log and diagnostics views with redaction,
  copy/export, structured categories, and user-actionable recovery paths.
- [ ] Add backup/restore UI, integrity checks, migration-history inspection, and
  recovery drills using ordinary SQLite tooling.

## M10 — Release-quality RAD runtime

- [ ] Complete an M8 application-builder acceptance pass on Windows, Linux, and
  macOS: create blank and existing-database applications; define data, forms,
  screens, menus, validation, scripts, and reports; enter records; and run the
  result without raw metadata SQL.
- [ ] Publish a compatibility/support matrix covering terminal hosts,
  architectures, locale/input limitations, upgrade paths, and recovery policy.
- [ ] Establish automated and manual accessibility review for keyboard-only
  operation, high contrast, screen-reader-adjacent terminal behavior, and
  localized overflow/error presentation.

## Future frontend and extension ecosystem

- [ ] Define and prove a frontend contract with a non-TUI renderer that consumes
  the same application and data semantics without terminal concepts.
- [ ] Define a small, versioned, capability-limited native C++ extension
  contract, including process isolation/trust policy, compatibility guarantees,
  tests, and packaging; do not expose raw runtime internals.
- [ ] Evaluate a native desktop GUI frontend only after M8 validates the
  semantic metadata and window model.
- [ ] Evaluate a web frontend, networking, multi-user coordination, and plugins
  only after the local single-user RAD workflow is proven strong; none may
  compromise the SQLite-first deployment model.
