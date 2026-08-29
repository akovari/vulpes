# ADR 0027: SQLite-resident application definition and migrations

- Status: accepted
- Date: 2026-08-28

## Context

Vulpes applications should deploy as one ordinary SQLite-compatible file, but
an arbitrary database must remain immediately usable without metadata or an
implicit write. Presentation metadata already has a UI-neutral model; it needs
a versioned persistence boundary before app mode, named reports, or menus can
depend on it.

## Decision

Application metadata uses reserved `_app_*` SQLite tables. `_app_schema` owns a
single positive schema version. The current version is 5:

- version 1 introduces settings, named forms, and form-field presentation and
  lookup overrides;
- version 2 introduces named views, reports, commands, menus, and menu items.
- version 3 introduces ordered Lua business-logic hook definitions.
- version 4 introduces named dashboard screens and ordered command links.
- version 5 introduces named view-local saved filters with typed values and an
  optional default filter for a view.

`migrate_application_metadata` is the only schema-creation/upgrade entry point.
It applies ordered migrations in one SQLite transaction and is idempotent at
the current version. A future version is rejected rather than guessed or
downgraded. Migration is explicit: `load_application_definition` checks for
`_app_schema` and returns an empty definition for an ordinary database without
creating any table.

The loader owns all query results and translates rows into
`ApplicationDefinition`. It parses lookup search fields from a JSON array,
enforces bounded numeric values and nullable booleans, and converts field-format
names to the existing enum. It then validates every referenced table, field,
form, command, menu item, and screen item against inspected SQLite schema. The
semantic definition contains no terminal coordinates, frontend types, or raw
SQL execution path for dashboard links.

Default form definitions are projected into the existing
`ApplicationMetadata` presentation model. This lets generated forms and grids
consume persisted enhancements through the same API as programmatically
supplied metadata.

## Consequences

- A `.vulpes` application remains an SQLite database readable by ordinary
  SQLite tools.
- Opening a database cannot silently convert it into a Vulpes application.
- Metadata schema evolution has an auditable transaction boundary and rejects
  unsupported future files.
- `_app_*` names are reserved for Vulpes; application-mode navigation must hide
  them while low-level schema inspection may still report SQLite facts.
- App-mode command/menu execution and report rendering are separate consumers
  of the validated definition and do not belong in the loader.
