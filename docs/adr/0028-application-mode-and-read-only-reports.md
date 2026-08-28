# ADR 0028: Application mode and read-only reports

- Status: accepted
- Date: 2026-08-28

## Context

Persisted definitions become useful only when the runtime can launch named
forms, views, commands, menus, and reports without coupling them to terminal
coordinates. Named SQL reports also need a tighter security boundary than the
deliberately arbitrary SQL console.

## Decision

The command parser recognizes `forms`, `form`, `views`, `view`, `reports`,
`report`, and `run` as stable semantic commands. `ApplicationRuntime` receives
an optional validated `ApplicationDefinition`, resolves those names, hides
reserved `_app_*` tables from application navigation, and returns semantic
responses. Named commands are recursively dispatched through the same parser
with a fixed depth limit. Frontends never execute command strings themselves.

`Workspace` renders validated application menus on its home document. Selecting
an item returns `run <name>` to the normal host dispatcher. Named form/view
responses create ordinary `BrowseDocument` surfaces with an owned presentation
override; named reports create `ReportDocument` surfaces. Tabs, normalized
input, themes, and window management remain shared with development mode.

`Database::run_query` is the report execution boundary. It accepts exactly one
prepared statement, requires SQLite's `sqlite3_stmt_readonly` classification and
at least one result column, and owns at most the configured number of rows.
`ReportDocument` presents those rows through Grid and exposes navigation only.
The SQL console continues to use the separate arbitrary-script boundary.

An explicit database path without a direct `--table`, `--command`, or `--sql`
request launches the workspace with that database already open. A loaded
application definition selects the app home; an ordinary database retains the
schema-first home. `.vulpes` remains a convention rather than a dispatch based
on filename alone.

## Consequences

- TUI, future GUI, and future web frontends can consume the same command
  responses and application definition.
- Metadata menus cannot bypass runtime validation or execute raw SQL.
- Reports cannot modify application data, even on a read/write connection.
- Application and database-development modes share one runtime and document
  implementation rather than diverging products.
