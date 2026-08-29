# Vulpes release notes

## 0.1.0 — unreleased

This is the first planned Vulpes release. It is not publishable until the
platform and terminal-host verification items in `TODO.md` are complete.

### Highlights

- Local SQLite browsing, schema inspection, typed sorting/filtering/search, and
  transactional generated record editing.
- Keyboard-first workspace with menus, dialogs, tabs, command palette, Unicode
  rendering, themes, and Czech localization.
- Schema-driven relationship lookup and drill-down without application-specific
  code.
- Optional versioned SQLite-resident application metadata for forms, views,
  menus, commands, reports, and `.vulpes` application mode.
- Named read-only reports and safe exports to CSV, JSON, text, HTML, and
  Unicode-capable PDF.
- CMake/CPack ZIP archives with resolved non-system runtime libraries,
  translations, SHA-256 sidecars, operational documentation, notice inventory,
  and reviewed license texts.

### Upgrade and compatibility

Vulpes continues to open ordinary SQLite databases without modifying them.
`--migrate-app` is explicit and only upgrades Vulpes’ reserved `_app_*`
metadata tables. Back up and verify an application file before migration or
runtime upgrade; see [operations.md](operations.md).

### Release limitations

- The archive process has been smoke-tested on Windows x64 only. Linux and
  macOS archives require native build, extraction, runtime, and terminal-host
  evidence before publication.
- Windows code signing, macOS notarization, graphical installers, and
  distribution-native Linux packages are not included.
- Lua scripting, GUI/web renderers, networking, visual form design, and a
  plugin ecosystem remain deliberately deferred.
