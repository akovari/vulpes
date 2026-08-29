# Application metadata and app mode

Vulpes application files are ordinary SQLite databases. `.vulpes` is a
conventional extension, not a custom format. Business tables, Vulpes metadata,
and named report queries can therefore be inspected and backed up with standard
SQLite tools.

## Initialize or upgrade metadata

Metadata creation is always explicit. For an existing database:

```powershell
.\build\windows-msvc\Debug\vulpes.exe inventory.db --migrate-app
```

This transactionally creates or upgrades the reserved `_app_*` tables to the
runtime's current schema version. It never creates a missing database, does not
alter business tables, is safe to repeat, and refuses metadata from a newer
runtime version. Merely opening an ordinary database performs no migration and
no metadata write.

The current metadata schema version is 3. Its tables are:

- `_app_schema`: one metadata schema-version row;
- `_app_settings`: application key/value settings, including the optional
  `title` shown on the app home;
- `_app_forms` and `_app_form_fields`: named forms, default forms, labels,
  ordering, visibility, read-only policy, formats, and relationship lookups;
- `_app_views`: named application views over an existing SQLite table or view;
- `_app_reports`: named, bounded, read-only SQL queries;
- `_app_commands`: semantic command names and their Vulpes command text; and
- `_app_menus` and `_app_menu_items`: ordered app-home menus that reference
  named commands; and
- `_app_scripts`: ordered optional Lua business-logic hooks.

Use lowercase ASCII letters, digits, hyphens, and underscores for command names.
Lookup search fields are stored as a JSON string array. Boolean metadata values
are `0`, `1`, or `NULL` when an override is absent. Field formats are
`automatic`, `text`, `number`, `boolean`, `date`, `time`, `date_time`, or
`currency`; the temporal and currency requirements in
[ADR 0026](adr/0026-application-presentation-metadata-and-relationships.md)
still apply.

See [scripting.md](scripting.md) for hook scope, record values, transaction
semantics, script limits, and the trusted-code policy.

## Commands and launch behavior

The command palette and `--command` share these application commands:

```text
forms
form <name>
views
view <name>
reports
report <name>
export <report> <format> <path> [overwrite]
run <command-name>
```

A named command may target any built-in or another named command. Recursion is
bounded and a cycle becomes a structured application error. App menus route
through `run`, so they do not bypass command validation.

Running `vulpes database.db` now opens that database in the workspace. When a
validated application definition and menus are present, the workspace shows the
application title and menu items instead of exposing `_app_*` implementation
tables. Databases without application metadata retain the ordinary schema-first
workspace. The `.vulpes` extension is recommended for deployment but does not
change SQLite compatibility or loader behavior.

## Reports

Each report contains exactly one SQLite statement. The runtime asks SQLite to
verify that the prepared statement is read-only and requires it to return
columns. Multiple statements and write/DDL statements are rejected even when
the database connection itself is writable. Results are owned, bounded by the
report's `row_limit` (1 through 100,000), and displayed through the same Grid as
SQL and browse results.

The `export` command routes the same named, bounded report through the
terminal-independent exporter. It accepts `csv`, `json`, `text`, `html`, or
`pdf`; an existing output path requires its explicit `overwrite` argument. See
[exporting.md](exporting.md) for encoding, locale, and recovery behavior.

## Inventory example

After creating and seeding `inventory.db`, initialize metadata and load the
shipped definition:

```powershell
.\build\windows-msvc\Debug\vulpes.exe inventory.db --migrate-app
sqlite3 inventory.db ".read examples/inventory/application.sql"
Move-Item inventory.db inventory.vulpes
.\build\windows-msvc\Debug\vulpes.exe inventory.vulpes
```

The example adds Products, Stock movements, and Low-stock report commands using
only the public metadata model. Its acceptance test loads the same SQL and
executes the report through `ApplicationRuntime` and the read-only query
boundary.
