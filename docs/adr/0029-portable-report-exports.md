# ADR 0029: Portable report exports

- Status: accepted
- Date: 2026-08-29

## Context

Reports need useful delivery formats without turning the TUI into a report
designer or leaking rendering dependencies into application metadata. Exports
must work for named reports and one-off read-only queries, preserve ordinary
SQLite data where a machine-readable format is requested, and never silently
write an incomplete result.

## Decision

`report::export_result` accepts an owned `SqlResult` and an explicit
destination, format, overwrite policy, title, and locale. It is independent of
the terminal, widgets, and metadata loader. `export_query` first calls the
existing single-statement, read-only `Database::run_query` boundary, then uses
the same result exporter.

The supported formats are CSV, JSON, plain text, HTML, and PDF. CSV and JSON
are machine-readable: integers and reals use invariant representations, NULL
is empty in CSV and `null` in JSON, and BLOBs use an explicit hexadecimal
representation. JSON represents results as `columns` plus positional `rows`,
so duplicate SQLite column labels remain unambiguous. Text, HTML, and PDF are
human-readable and use the requested ICU locale for numeric display; text and
HTML safely escape their respective delimiters. All output is UTF-8 without a
BOM. Export rejects invalid UTF-8 and NUL in user-visible text instead of
creating a corrupt or ambiguous output file.

PDF uses the small PDFio C library, fetched at a reviewed PDFio 1.6.4 commit as
a private CMake target. A reviewed Roboto Regular font from its examples is
embedded in the Vulpes binary, so PDF text is Unicode-capable without relying
on a host font installation. The renderer produces landscape, paginated tables
and repeats the title and headers on each page. PDFio remains wholly inside the
report implementation; no public Vulpes API exposes it.

An export refuses a truncated result. Callers choose a bounded row limit before
query execution, and must raise it deliberately if a complete export is
required. Existing files are rejected unless `overwrite` is explicit. A write
first goes to a uniquely named sibling temporary file; an overwrite moves the
old destination to a sibling backup, renames the completed temporary file into
place, then removes the backup. If the final rename fails, Vulpes attempts to
restore the old destination. After an interrupted process, sibling files named
`.vulpes-tmp-*` and `.vulpes-backup-*` are intentionally left inspectable for
manual recovery rather than being silently discarded.

Named application reports are exported with the semantic command:

```text
export <report> <format> <path> [overwrite]
```

The command parser, runtime, command palette, and direct `--command` path all
share that command. The non-interactive `--query`, `--output`, `--format`,
`--row-limit`, and `--overwrite` options offer the same safe path for one
read-only SQL query.

## Consequences

- Export has deterministic data, encoding, locale, and replacement semantics.
- A report cannot bypass `Database::run_query` or execute arbitrary writes.
- Future GUI and web frontends can reuse the same exporter without terminal
  coordinates or terminal output parsing.
- PDF adds a focused implementation dependency and embedded-font licensing
  obligations, which release packaging must include alongside existing notices.
