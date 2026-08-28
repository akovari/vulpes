# Localization

Vulpes keeps stable message keys in code and translates only at presentation
boundaries. Commands, actions, field names, and database identifiers are never
localized identifiers.

Semantic UI widgets receive their visible instructions from their caller rather
than hard-coding English: the browse footer, prompts, record-form instructions,
destructive dialogs, terminal-size warning, and all workspace/menu/status text
therefore use catalog messages. Workspace menu mnemonics are the first character
of each translated menu-bar label, so the Czech catalog uses `Alt+S` for
`Soubor`; `F10` remains a locale-independent way to enter the menu bar. Schema
field names are still the default labels; metadata-provided localized labels are
a later layer.

## Catalog format

Catalogs are UTF-8 JSON files with a BCP-47 locale and a flat message map:

```json
{
  "locale": "cs",
  "messages": {
    "database.tables": "Tabulky a pohledy",
    "error.unknown_table": "Tabulka nebo pohled '{name}' neexistuje."
  }
}
```

The loader rejects malformed documents, non-string keys/values, and catalogs
larger than 1 MiB. It selects an exact locale first, then the language subtag,
then English. Missing translations therefore remain usable during incremental
translation work.

Use the shipped Czech catalog explicitly during development:

```powershell
.\build\windows-msvc\Debug\vulpes.exe inventory.db --locale cs-CZ --catalog translations\cs.json
```

Named placeholders such as `{name}` are substituted by the presentation caller.
Plural/select grammar is intentionally deferred until the project adopts ICU and
defines its user-visible formatting policy.
