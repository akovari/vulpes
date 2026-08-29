# Exporting report and query results

Vulpes exports complete, owned query results without using the terminal
renderer. It supports CSV, JSON, plain text, HTML, and PDF.

## Named reports

After an application database has defined a named report, run its semantic
export command from the command palette or with `--command`:

```powershell
.\build\windows-msvc\Debug\vulpes.exe inventory.vulpes --command "export low-stock csv low-stock.csv"
.\build\windows-msvc\Debug\vulpes.exe inventory.vulpes --command "export low-stock pdf low-stock.pdf overwrite"
```

`overwrite` is the only way to replace an existing destination. Quote a path
containing spaces. Inside quoted command arguments, Windows path separators are
preserved; only `\\` and `\"` have special escaping meaning.

## One read-only SQL query

Use `--query` with `--output` for a one-statement, read-only query. Vulpes
opens the database read-only, rejects SQL scripts and write statements, and
defaults the format from the output extension:

```powershell
.\build\windows-msvc\Debug\vulpes.exe inventory.db `
  --query "SELECT name, reorder_level FROM products ORDER BY name" `
  --output products.json

.\build\windows-msvc\Debug\vulpes.exe inventory.db `
  --query "SELECT name, quantity FROM low_stock ORDER BY name" `
  --output low-stock.pdf --format pdf --locale cs-CZ
```

The maximum is 100,000 rows by default. Set an explicit limit from 1 through
100,000 when needed:

```powershell
.\build\windows-msvc\Debug\vulpes.exe inventory.db `
  --query "SELECT * FROM products" --output products.csv --row-limit 50000
```

If the result reaches its limit, Vulpes reports an error and writes no export.
Choose a sufficiently high limit deliberately rather than accepting a partial
file.

## Format contract

All formats are UTF-8 without a BOM. Titles, column names, and SQLite text
values must be valid UTF-8 and cannot contain NUL; Vulpes fails before writing
when that contract is violated.

| Format | Intended use | Values |
| --- | --- | --- |
| CSV | Spreadsheets and data exchange | RFC-style CRLF rows and quote escaping; invariant numbers; NULL is empty; BLOB is `X'HEX'`. |
| JSON | Typed interchange | `{ "columns": [...], "rows": [[...]] }`; NULL is `null`; integers/reals retain JSON types; BLOB is `{ "$blob": "HEX" }`. |
| Text | Human-readable tabular text | Tab-delimited with control characters escaped; localized numbers; NULL is the literal `NULL`. |
| HTML | Browser-ready human-readable table | A complete UTF-8 document with escaped cell values and localized numbers. |
| PDF | Portable printable table | Landscape, paginated Unicode table with an embedded font and localized numbers. |

CSV and JSON retain machine-oriented representations regardless of `--locale`.
Locale affects numeric display in text, HTML, and PDF only.

## Safe file replacement and recovery

Vulpes writes a temporary file beside the destination and only names it as the
final destination after output succeeds. It refuses an existing destination
unless `overwrite` is supplied. During an overwrite it preserves the previous
file as a sibling backup until the new file is in place.

If a process or filesystem failure interrupts this sequence, inspect sibling
files ending in `.vulpes-tmp-<n>` and `.vulpes-backup-<n>`. The temporary file
is the candidate new export and the backup is the candidate old export; inspect
their contents and move the desired one into place manually. Do not delete them
until the destination has been verified.
