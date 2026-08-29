# Backup, recovery, compatibility, and upgrades

## User-local workspace preferences

Personal workspace preferences are deliberately separate from a database's
portable `_app_*` metadata. They contain recent databases and presentation/input
defaults for one host. The default file is `Vulpes/settings.json` below Windows
`%APPDATA%`, `~/Library/Application Support` on macOS, and
`$XDG_CONFIG_HOME/vulpes` (or `~/.config/vulpes`) on Linux. Use
`--config <path>` to use an isolated or portable settings file.

The current format is version 2. Version 1 recent-database files load with
safe defaults and are rewritten as version 2 when Vulpes updates the recent
list. A newer configuration version is rejected rather than being interpreted
or rewritten by an older Vulpes installation.

```json
{
  "version": 2,
  "locale": "cs-CZ",
  "theme": "high-contrast",
  "default_dataset_page_size": 48,
  "recent_databases": ["C:/data/inventory.vulpes"],
  "key_bindings": [
    {"action": "record.edit", "key": "f9", "ctrl": false, "alt": false, "shift": false},
    {"action": "application.menu", "key": "character", "character": "m", "alt": true, "ctrl": false, "shift": false}
  ]
}
```

`key_bindings` overlay the built-in mappings. Each entry uses a stable action
identifier and a normalized key name (`character`, `enter`, `escape`, arrows,
navigation keys, `insert`, `delete`, or `f1` through `f12`). A character entry
must contain exactly one UTF-8 code point, and no physical key combination may
appear twice. Invalid or unknown binding data is rejected rather than guessed.

For a single launch, `--locale`, `--theme`, and `--page-size` take precedence
over the settings file and do not persist changes. Page size must be between 1
and 1000 rows.

Vulpes databases are ordinary SQLite databases. The `.vulpes` suffix is a
convention only: any SQLite tool can inspect business tables and Vulpes’
reserved `_app_*` metadata tables.

## Back up before changing data or metadata

Always make a verified backup before schema changes, application-metadata
migration, bulk SQL, or upgrading the Vulpes runtime. The recommended approach
is SQLite’s [online backup command](https://www.sqlite.org/backup.html), which
produces a consistent copy while the database is in use:

```powershell
sqlite3 company.vulpes ".backup company-backup.sqlite"
sqlite3 company-backup.sqlite "PRAGMA integrity_check;"
```

[`VACUUM INTO`](https://www.sqlite.org/lang_vacuum.html) is also useful when
the database is openable and a compact snapshot is desired:

```powershell
sqlite3 company.vulpes "VACUUM INTO 'company-backup.sqlite';"
sqlite3 company-backup.sqlite "PRAGMA integrity_check;"
```

Keep backups outside the working directory when practical and test that they
open before proceeding. Neither Vulpes metadata migration nor record-level
transactions replace a backup.

Do not copy just the main database file while another process is using
[WAL mode](https://www.sqlite.org/wal.html). Prefer `.backup` or `VACUUM INTO`.
If a fully closed database must be copied at the filesystem level, preserve its
`-wal` and `-shm` sidecars with the main file and reopen the copy with SQLite
before treating it as a backup.

## Recovery

Start with a copy of the damaged file. Use SQLite’s integrity check on the copy:

```powershell
sqlite3 suspect.vulpes "PRAGMA integrity_check;"
```

If the check fails, preserve the original and use the
[SQLite recovery guidance](https://www.sqlite.org/recovery.html) and tools
appropriate to the installed SQLite version. Do not run modifying commands
against the only copy. A clean, verified backup is the preferred recovery
source.

Vulpes exports are written to a sibling temporary file before becoming the
destination. An interrupted overwrite can leave `.vulpes-tmp-<n>` (candidate
new export) and `.vulpes-backup-<n>` (candidate old export) beside the target.
Inspect both files, move the desired one into place, and only then remove the
leftovers. Details are in [exporting.md](exporting.md).

## Compatibility

Opening an ordinary SQLite database in Vulpes does not add tables or metadata.
`--migrate-app` is the only operation that creates or upgrades `_app_*` tables,
and it runs its ordered migrations in one transaction. It does not change
business-table definitions. Ordinary SQLite clients can continue to read,
query, back up, and modify the database; application metadata is an optional
enhancement.

Vulpes rejects application metadata from a newer schema version rather than
guessing a downgrade. A newer Vulpes runtime can migrate older supported
metadata, but never run a migration on the sole copy of an application file.

## Upgrade procedure

1. Close Vulpes and other writers.
2. Create and verify a SQLite backup.
3. Install a new Vulpes archive alongside the prior version; do not overwrite a
   known-good installation during evaluation.
4. Open the backup first and run the normal browse/report workflows.
5. If application metadata requires migration, run `vulpes app.vulpes
   --migrate-app`, then repeat the verification on the migrated copy.
6. Keep the prior archive and verified backup until the upgraded application has
   been used successfully.

The Vulpes runtime has no automatic database rollback feature. Restoring the
verified SQLite backup is the supported rollback path.
