# ADR 0031: Versioned user-local workspace preferences

## Status

Accepted.

## Context

Vulpes needs personal defaults for presentation and keyboard workflow without
turning an ordinary SQLite database into a host-specific application format.
The original settings file recorded only recent databases and had no explicit
upgrade path. It could not persist the configurable semantic key bindings
introduced by ADR 0011.

## Decision

`core::WorkspacePreferences` owns a versioned JSON document in the conventional
user configuration directory, or at an explicit `--config` path. Version 2
stores a bounded recent-database list, locale, theme name, default dataset page
size, and normalized key-binding overrides. Version 1 recent-file documents
load with built-in defaults and save as version 2 on the next write.

Key bindings contain stable `ActionId` strings and normalized key/modifier
values. They must be unambiguous: unknown actions/keys, malformed UTF-8
character bindings, and duplicate physical keys are rejected. The workspace
starts with the built-in `ActionMap` and overlays the configured bindings.

CLI values take precedence over host preferences. Specifically, `--locale`,
`--theme`, and `--page-size` override the configured defaults for the current
process. They do not rewrite the settings file.

Workspace preferences are not SQLite application metadata. `_app_*` tables
continue to describe portable application behavior for a database; they do not
store a machine's recent files, personal theme, locale, or key bindings.

## Consequences

- Existing recent-file configurations remain usable without manual migration.
- A portable/test invocation can select an isolated settings document with
  `--config`.
- New themes and actions require a deliberate compatibility policy before being
  written into preferences.
- Future configuration UI may edit this document through the same validated
  Vulpes-owned model; it must not expose terminal escape sequences or translated
  text as identifiers.
