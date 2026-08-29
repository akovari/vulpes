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

The current metadata schema version is 5. Its tables are:

- `_app_schema`: one metadata schema-version row;
- `_app_settings`: application key/value settings, including the optional
  `title` shown on the app home;
- `_app_forms` and `_app_form_fields`: named forms, default forms, labels,
  ordering, visibility, read-only policy, formats, and relationship lookups;
- `_app_views`: named application views over an existing SQLite table or view;
- `_app_view_filters` and `_app_view_filter_terms`: ordered named, typed saved
  filters owned by a view;
- `_app_reports`: named, bounded, read-only SQL queries;
- `_app_commands`: semantic command names and their Vulpes command text; and
- `_app_menus` and `_app_menu_items`: ordered app-home menus that reference
  named commands;
- `_app_scripts`: ordered optional Lua business-logic hooks; and
- `_app_screens` and `_app_screen_items`: named dashboards with ordered action
  links to named commands.

Use lowercase ASCII letters, digits, hyphens, and underscores for command and
screen names.
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
screens
screen <name>
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
tables. A screen marked `is_default = 1` opens as the initial dashboard. Its
items render as an ordered keyboard-selectable action list and dispatch only
their referenced named commands through the normal runtime; screen metadata
contains no terminal coordinates or SQL. Databases without application metadata
retain the ordinary schema-first workspace. The `.vulpes` extension is
recommended for deployment but does not change SQLite compatibility or loader
behavior.

## Screens and dashboards

Screens provide the first reusable application-dashboard model. They are
semantic navigation definitions rather than fixed TUI layouts, so a future GUI
or web frontend can render the same screen as buttons, cards, or a navigation
panel. The current TUI presents a framed ordered list. Use a named command for
each action rather than duplicating command text or SQL in the screen item:

```sql
INSERT INTO _app_commands(name, label, command)
VALUES ('products', 'Products', 'browse products');

INSERT INTO _app_screens(name, label, description, is_default)
VALUES ('home', 'Inventory', 'Choose an inventory task.', 1);

INSERT INTO _app_screen_items(screen_name, position, label, description, command_name)
VALUES ('home', 0, 'Products', 'Browse the product catalogue.', 'products');
```

`_app_screens.is_default` is unique, so an application has zero or one default
dashboard. Names, ordering, labels, descriptions, and command references are
validated when metadata loads. Future screen designers can enhance this model
with control types and data bindings without invalidating the coordinate-free
navigation contract.

## Named view filters

Views can own reusable, compound filters without storing a SQL fragment. A
filter has an optional text search and ordered comparisons whose values retain
their SQLite type (`NULL`, integer, real, text, or blob). A view can select one
of its filters as `default_filter_name`; Vulpes applies it whenever the view is
opened, in either the workspace or direct browse mode. The metadata loader
validates every field and comparison against the view's inspected schema before
the definition reaches the UI.

```sql
INSERT INTO _app_views(name, table_name, label, default_filter_name)
VALUES ('active-products', 'products', 'Active products', 'in-stock');

INSERT INTO _app_view_filters(view_name, name, position)
VALUES ('active-products', 'in-stock', 0);

INSERT INTO _app_view_filter_terms(
  view_name, filter_name, position, field_name, comparison, value_kind, integer_value
)
VALUES ('active-products', 'in-stock', 0, 'quantity', 'greater', 'integer', 0);
```

The `comparison` values are `equal`, `not_equal`, `less`, `less_equal`,
`greater`, and `greater_equal`. The `value_kind` must correspond to exactly one
value column, except `null`, which has no value column and is valid only with
`equal` or `not_equal`. These tables are a persistence boundary for portable
view definitions; later authoring UI will edit them through validated models
rather than direct SQL.

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
