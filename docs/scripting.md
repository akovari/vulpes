# Lua business logic

Vulpes applications can store small Lua business rules in the same ordinary
SQLite file as their schema and application metadata. Lua is optional: an
ordinary SQLite database and a metadata-defined application continue to work
without scripts.

Scripts are application code, not end-user customization. Review a database's
scripts before opening it, and do not treat Vulpes' restricted Lua environment
as a security boundary for an untrusted `.vulpes` file.

## Enable script metadata

Metadata migration is explicit and upgrades `_app_schema` to version 4:

```powershell
.\build\windows-msvc\Debug\vulpes.exe app.vulpes --migrate-app
```

It creates this table without changing business tables:

```text
_app_scripts(
  name TEXT PRIMARY KEY,
  hook TEXT NOT NULL,
  table_name TEXT,
  command_name TEXT,
  source TEXT NOT NULL,
  position INTEGER NOT NULL
)
```

Script names and command scopes use lowercase ASCII letters, digits, hyphens,
and underscores. Scripts run in ascending `position`, then name order.

## Hooks

| Hook | Scope | Data | Timing |
| --- | --- | --- | --- |
| `on_open` | application | `context` | After metadata loads when Vulpes opens an application |
| `before_insert` | one business table | writable `record`, `context` | Inside the insert transaction, before SQL runs |
| `before_update` | one business table | writable `record`, `context` | Inside the update transaction, before SQL runs |
| `after_update` | one business table | read-only `record`, `context` | After the update SQL, before commit |
| `before_delete` | one business table | read-only `record`, `context` | Inside the delete transaction, before SQL runs |
| `on_command` | optional top-level command name | `context` | Before semantic command dispatch |

`table_name` is required for record hooks and must name an ordinary inspected
SQLite table. It is forbidden for `on_open` and `on_command`. `command_name` is
optional only for `on_command`; without it, the hook observes all submitted
commands. Named-command expansion is not a second command event: a menu item
that submits `run products` has command context `run`.

An error from any record hook rolls back the write. The generated form retains
its unsaved draft after a rejected insert/update, so a user can correct it.
`after_update` can reject a just-applied update and thereby roll it back, but
cannot alter its record; normalize derived values in `before_update` instead.

## Record and context

Record hooks receive a plain Lua table called `record` with the current draft
or selected row. Writable hooks can assign normal writable SQLite fields:

```lua
if record.balance ~= nil and record.balance < 0 then
    error("Balance cannot be negative")
end

record.name = string.upper(record.name)
```

The bridge supports `nil`, booleans, Lua integers, floating-point numbers, and
strings. `nil` becomes SQLite `NULL` for an existing field. A field omitted from
a new record remains omitted so SQLite defaults still apply. BLOB fields are
not exposed. Scripts cannot add arbitrary fields or change generated, hidden,
or protected primary-key fields.

All hooks receive `context`:

```lua
assert(context.hook == "before_insert")
assert(context.table == "customer")
```

`on_command` additionally provides `context.command`:

```lua
if context.command == "browse" then
    error("Browsing is closed for maintenance")
end
```

Use normal Lua `error("message")` or `assert(condition, "message")` to stop a
hook. Vulpes wraps the resulting message as a structured script error; it does
not translate application-authored error text.

## Example metadata

This normalization/validation rule runs before a `customer` insert:

```sql
INSERT INTO _app_scripts(name, hook, table_name, command_name, source, position)
VALUES (
  'normalize-customer',
  'before_insert',
  'customer',
  NULL,
  'if record.balance ~= nil and record.balance < 0 then
       error("Balance cannot be negative")
   end
   record.name = string.upper(record.name)',
  10
);
```

The `source` column is ordinary SQLite TEXT. Use a parameterized statement or
your SQLite tool's normal literal escaping rules when programmatically storing
a script.

## Limits and deliberately absent capabilities

Every invocation uses a new Lua state with a 1 MiB memory budget and a
100,000-instruction budget. Vulpes opens only basic Lua language functions plus
`math`, `string`, `table`, and `utf8`. It does not provide `io`, `os`, `package`,
`debug`, `require`, `dofile`, `loadfile`, `load`, or `print`.

There is deliberately no raw SQLite handle, SQL execution, filesystem access,
networking, shell access, UI coordinate access, module system, persistent Lua
globals, or native object exposure. The resource limits help bound mistakes;
they do not make malicious application code safe. A future host capability must
be narrow, documented, tested, and reviewed through a new ADR.

See [ADR 0030](adr/0030-lua-business-logic-hooks.md) for the architecture,
transaction, and trust model.
