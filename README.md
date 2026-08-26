# Vulpes

Vulpes is a local-first, keyboard-driven RAD environment that turns an ordinary
SQLite schema into an application. The first frontend is a TUI; its database,
application, semantic UI, and rendering layers are deliberately independent.

The repository is pre-alpha. It currently provides a C++23 build,
cross-platform CI, RAII SQLite primitives, schema introspection, normalized
input, a virtual screen buffer, a keyboard-driven `--table` browse view, and a
transactional dataset editing model. Record-editing controls and the in-app
command window are the next frontend milestones.

## Prerequisites

- CMake 3.31 or newer
- Ninja 1.12 or newer
- Git
- vcpkg, with `VCPKG_ROOT` set to its absolute installation directory
- A current compiler: MSVC 18, GCC 14+, or Clang/AppleClang 18+

On Windows, run commands from an **x64 Native Tools Command Prompt for VS 2026**
so `cl.exe` is available. Visual Studio 2026 includes vcpkg; on a default
Community installation it can be selected for the current shell with:

```powershell
$env:VCPKG_ROOT = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
```

Alternatively, clone and bootstrap vcpkg separately. CMake manifest mode installs
SQLite and Catch2 automatically. The `sqlite3` CLI is optional and is used only
by the example-database command below.

## Build and test on Windows

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug
ctest --preset windows-debug
.\build\windows-msvc\Debug\vulpes.exe --version
```

To exercise the first schema-listing vertical slice after creating a database:

```powershell
sqlite3 inventory.db ".read examples/inventory/schema.sql"
.\build\windows-msvc\Debug\vulpes.exe inventory.db
```

See [docs/architecture.md](docs/architecture.md), [docs/development.md](docs/development.md),
and [TODO.md](TODO.md) before changing subsystem boundaries or choosing work.

## Project status

Version 0.1 is pre-alpha. APIs are expected to evolve and no data-modifying UI
exists yet. Database backups remain the user's responsibility.

## License

MIT. See [LICENSE](LICENSE).
