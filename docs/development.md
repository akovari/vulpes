# Development

## Workflow

1. Configure with the platform preset.
2. Build and run all tests.
3. Keep changes within one architectural boundary when practical.
4. Add unit tests for every core behavior and regression.
5. Update `TODO.md` when scope or sequencing changes.

Enable `VULPES_WARNINGS_AS_ERRORS` in CI once all supported compiler versions
produce a clean and stable warning set.

## Code conventions

- C++23; RAII and explicit ownership.
- No raw SQLite handles outside `src/db`.
- No terminal escape sequences outside terminal backends.
- No database access from widgets.
- Dataset identifiers must originate in `TableSchema`; values must be bound rather
  than interpolated. Free-form SQL is reserved for the SQL console boundary.
- UTF-8 at external text boundaries and `char32_t` for logical screen glyphs.
- Exceptions may cross implementation layers, but frontend boundaries convert
  `vulpes::Error` into user-facing structured errors.
- Platform-specific sources must be isolated and selected by CMake.

## Testing

Database tests use `:memory:` unless file behavior is the subject. UI rendering
tests compare logical cells. Terminal integration tests use a fake backend and
normalized events. End-to-end tests must use temporary database copies.

CI builds Windows, Ubuntu, and macOS on every push and pull request. Sanitizer,
coverage, fuzz, and static-analysis jobs are tracked in `TODO.md` rather than
being implied by the initial scaffold.
