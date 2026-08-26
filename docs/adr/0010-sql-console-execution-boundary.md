# ADR 0010: SQL console execution boundary

- Status: accepted
- Date: 2026-08-26

## Context

Vulpes needs an arbitrary SQL console, but raw SQLite handles and statement
lifetimes must remain confined to the database layer. SQL scripts may contain
both write statements and queries.

## Decision

`db::Database::run_sql(script, row_limit)` prepares and executes each statement
in order. It sums affected rows for non-tabular statements and returns owned
column names and rows from the final row-producing statement. Rows are limited
to 1,000 by default, with `truncated` recording when more rows existed.

## Consequences

- The future console can show results without managing `sqlite3_stmt*`.
- Complete scripts work, while the first UI can keep a compact one-result
  presentation.
- Transactions, parameter prompts, multiple displayed result sets, history,
  and export need dedicated console behavior rather than database-layer hacks.
