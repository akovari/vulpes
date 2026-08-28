# Localization

Vulpes keeps stable message keys in code and translates only at presentation
boundaries. Commands, actions, field names, and database identifiers are never
localized identifiers.

Semantic UI widgets receive their visible instructions from their caller rather
than hard-coding English: the browse footer, prompts, record-form instructions,
destructive dialogs, terminal-size warning, and all workspace/menu/status text
therefore use catalog messages. Menu mnemonics use adjacent `.mnemonic` keys
containing exactly one Unicode code point; they are not inferred from translated
labels. Workspace construction verifies that each mnemonic occurs in its label
after Unicode case folding and is unique within its menu scope. The Czech
catalog deliberately uses `Alt+S` for `Soubor`; `F10` remains a
locale-independent way to enter the menu bar. Schema
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

Catalog values use ICU MessageFormat syntax. Named arguments are typed strings,
integers, floating-point values, or select booleans; plural rules therefore
receive numbers rather than preformatted strings. Keep each plural or select
choice as a complete translatable message:

```json
{
  "workspace.command_tables": "{count, plural, one {Refreshed # table or view.} other {Refreshed # tables and views.}}"
}
```

The Czech catalog supplies its CLDR `one`, `few`, and `other` branches and a
translated `select` message for database access mode. Exact and language-only
catalog matches use the requested regional locale. If the message itself falls
back to English, English plural grammar is used so fallback text remains
grammatical.

ICU treats ASCII apostrophes as MessageFormat quoting syntax around braces. Use
typographic apostrophes or the quotation marks customary for the language in
human-facing prose. Catalog patterns are parsed when loaded, and malformed
patterns are reported as metadata errors before an affected screen is opened.

`core::LocaleFormatter` centralizes CLDR number, currency, date, time, and
date-time display. Currency always requires an explicit uppercase ISO 4217 code;
dates and times always use an explicit IANA time zone. These policies affect
display only. Vulpes does not parse localized text back into SQLite or infer a
date from a declared type until application metadata explicitly annotates the
field. See ADR 0025.
