# Architecture

## Direction

Dependencies point inward and downward:

```text
frontend (TUI; GUI/web later)
  -> semantic UI and application runtime
    -> dataset/cursor model
      -> db abstractions
        -> SQLite C API

terminal backend -> virtual screen and normalized input
widgets          -> virtual screen and normalized input
```

The application runtime must not include terminal backend headers. Widgets must
not include SQLite headers. Only `src/db` may include `sqlite3.h`; public headers
forward-declare opaque SQLite types where ownership requires it.

## Source layout

- `include/vulpes/core`: shared errors and application-level primitives.
- `ApplicationRuntime` in `core` translates parsed semantic commands into data
  results; it has no terminal or widget dependency.
- `include/vulpes/db`, `src/db`: SQLite ownership, values, statements,
  transactions, and UI-independent schema discovery.
- `include/vulpes/model`, `src/model`: datasets, fields, relationships, and
  validation (Phase 2; created when behavior is implemented).
- `include/vulpes/ui`, `src/ui`: semantic widgets (Phase 3).
- `include/vulpes/terminal`, `src/terminal`: virtual screen, normalized input,
  and platform backends.
- `include/vulpes/appmeta`, `src/appmeta`: UI-neutral optional application
  definitions, validation, and explicit versioned SQLite-resident loading.
- `examples/inventory`: generic framework dogfood; never a source of inventory
  special cases in the runtime.

Directories are added when they contain working code. This avoids empty modules
that imply stability or design decisions not yet earned.

Workspace document surfaces own UI-level state such as a dataset/grid/form or
SQL editor/result grid. The workspace shell owns tabs and routes normalized
events to the active surface. Neither layer knows terminal escape sequences;
database operations remain inside `Dataset` or the explicit SQL-console
boundary.

`WindowManager` owns document identity, the active tab, modal priority, and a
tab-local status string. Explicit window closure is confirmation-gated by the
workspace; document Escape remains a semantic back action. Replacing the open
database resets all non-workspace tabs before a new database is exposed, which
prevents stale datasets from crossing connection boundaries.

`DocumentSession` is the direct-mode terminal host for a single
`DocumentSurface`. Consequently `vulpes database.db --table customers` and the
workspace Browse command run the same browse/form/filter implementation; the
same is true of direct and workspace SQL consoles. It also gives reduced
terminal sizes a deterministic warning frame and preserves Escape/Ctrl+C as
clean exits while waiting for a resize. Workspace and direct modes continue to
present that frame and consume input after a resize, so a temporary small
terminal cannot produce a busy loop or a stale screen.

All semantic surfaces select `ThemeRole` values rather than defining RGB colors
inside menus, grids, forms, tabs, or status bars. `midnight` is the default
theme and `high-contrast` is a deliberately high-contrast alternative. A theme
reference is injected through the workspace into hosted documents and controls;
direct document sessions receive the same selected theme. Themes affect
presentation only; they never change action IDs, document state, or the data
model.

`FocusRing` owns cyclic focus order for semantic controls and skips unavailable
items. Forms use it to avoid read-only fields, and destructive confirmations
use it for their buttons. The controls retain responsibility for their own key
semantics, so the focus model remains independent of a terminal implementation.

`LineEditor` owns one UTF-8 line and a byte-offset cursor that is always
kept on a code-point boundary. It provides semantic insert/delete/movement and a
display-cell viewport, so prompts and generated fields share editing behavior
without a terminal cursor or backend dependency. Rendering uses a logical caret
cell and follows long values horizontally. Both editors own normalized selection
ranges, Unicode-aware word movement, and configurable insert/overwrite state.
Clipboard access is injected through `core::Clipboard`; the production adapter
uses dacap/clip while tests use memory implementations. CPP-Terminal bulk-paste
events become a distinct `PasteEvent`, preventing pasted text from being routed
as keyboard commands. `MultilineEditor` applies the same
boundary policy to SQL source while adding logical line spans, desired
display-column preservation, line splitting/joining, indentation, page movement,
stable two-axis viewport state, and a bounded 100-state/4 MiB undo journal.
`SqlConsole` separately owns a bounded 100-entry executed-command history. See
ADRs 0019, 0020, and 0023.

`WindowFrame` renders opaque Unicode terminal windows with an optional clipped
drop shadow; `Button` renders a focusable action affordance. Prompts,
confirmations, directory selection, record forms, and the SQL editor compose
the frame instead of implementing borders independently. Pop-up menus use the
same visual language while keeping their item state in `Workspace`. Menu labels
and explicit mnemonic code points enter together through `WorkspaceText`;
construction validates occurrence and per-scope uniqueness, and Unicode-aware
lookup drives both rendering and activation. See ADR 0022.
`WindowManager` owns document lifetime, tab selection, dirty markers, and the
workspace modal-title stack. Document surfaces use the reusable owning
`WindowStack<Content>` for heterogeneous overlays; browse routes input only to
the top form, prompt, or confirmation layer while rendering the full stack.
This makes nested lookup and related-record drill-down windows possible without
adding parallel optional-modal flags. See ADRs 0018 and 0024.

`Grid` computes bounded preferred widths from field names and the current owned
page, then distributes remaining cells without changing dataset paging. It uses
separate header, ordinary-row, selected-row, focused-cell, and footer roles.
Per-field width overrides remain presentation state inside the Grid and are
adjusted through semantic actions. `GridOptions` can pin leading columns, add
read-only calculations over owned rows, and request typed aggregate summaries;
these are semantic projections, not terminal coordinates or widget SQL.
`Dataset` owns the active typed saved-filter presets and the safe aggregate-query
boundary. A metadata-defined view can supply portable presets and one default;
the frontend copies them into the Dataset at document creation, so the Dataset
still neither reads `_app_*` tables nor constructs SQL from presentation text.
The renderer keeps the selected row within a viewport smaller than the dataset
page, reports absolute row/column position, and derives border overflow/thumb
markers without exposing terminal behavior to the dataset.
Localized `GridText` supplies empty-state and position labels. See ADRs 0021
and 0033.
Record forms derive a viewport from the focused field when a schema contains
more fields than the current window can show. These are rendering policies;
they do not leak into the dataset model.

`StatusBar` renders either a localized status message or shortcut hints using
dedicated semantic theme roles. It owns no command mapping; action IDs remain
the responsibility of the input layer.

`Widget`, `Constraints`, `Label`, and `Container` provide the small shared
measurement/layout vocabulary. Containers hold non-owning child references and
arrange logical rectangles only; semantic screens retain control ownership and
event routing. Specialized controls can adopt the contract incrementally. See
ADR 0016.

`WorkspacePreferences` is a versioned, user-local JSON file outside the SQLite
application. It persists a bounded recent-database list plus host presentation
defaults (locale, theme, dataset page size, and semantic key-binding overrides)
without altering ordinary SQLite compatibility or becoming a second application
format. The command line takes precedence over those defaults. It uses the
platform configuration directory by default and accepts an explicit path for
portable/test use. SQLite-resident `_app_*` metadata remains the separate,
portable definition of an application; it must not read or write host
preferences. See ADR 0031.

`Database` retains its SQLite open mode. Read-only connections make table
datasets non-editable before a UI action can create a form or delete
confirmation; the browse surface changes its footer accordingly. Workspace
Open, Open read-only, and Create map explicitly to SQLite read/write,
read-only, and read/write-create modes.

The optional `DirectoryBrowser` is a semantic, read-only filesystem navigator.
It returns a selected path to the workspace but never opens SQLite or changes a
file itself. This preserves manual path entry for known, UNC, and otherwise
non-browsable locations; see [ADR 0012](adr/0012-directory-browser.md).

## Initial decisions

### C++23 with a conservative feature surface

C++23 is the language mode because current supported compilers implement it and
MSVC 18 is the primary local toolchain. Public APIs initially use broadly
implemented vocabulary types so platform support remains practical.

Build identity is generated from `vMAJOR.MINOR.PATCH` Git tags and exposed as
typed constants. Untagged commits receive a SemVer-compatible development
suffix, while source archives fall back deterministically or provide an
explicit packaging override. See ADR 0017.

### vcpkg manifest dependencies

The checked-in registry baseline makes dependency resolution reproducible.
SQLite, Catch2, utf8proc, CLI11, nlohmann/json, and dacap/clip each have a narrow boundary:
database access, tests, Unicode cell handling, process argument parsing, and
external message-catalog parsing, and cross-platform UTF-8 clipboard access.
CPP-Terminal is fetched at a
pinned commit because it is unavailable in the pinned vcpkg registry; it owns
host-terminal raw mode and event transport behind `terminal::ConsoleTerminal`.
Terminal rendering remains behind Vulpes abstractions rather than a
framework-specific widget model.

### SQLite threading and ownership

One `Database` owns one connection and is movable, not copyable. Statements are
movable, not copyable, and cannot outlive their database. This lifetime rule will
be made mechanically explicit if asynchronous execution is later introduced.
Foreign keys and extended result codes are enabled per connection; busy timeout
is initially five seconds and will become configuration. SQLite TEXT values are
kept as raw byte strings by the database layer, including invalid UTF-8, to
preserve SQLite semantics. The terminal boundary is responsible for safe display
of externally supplied text.

### Dataset paging and editing

The dataset layer owns paging, row identity, filters, ordering, editing state,
and owning row snapshots. Widgets request logical rows and never assemble SQL.
Stable single-column primary-key and non-null unique-order datasets use keyset
paging; composite, nullable, and non-unique orderings retain bounded OFFSET
pages. The strategy remains behind the dataset boundary.
Dataset writes are transactional and schema-validated; see ADR 0001 and ADR
0005.

### Generated record forms

`ui::RecordForm` turns a dataset draft and schema fields into semantic controls.
It has no SQLite dependency: it invokes only dataset edit operations and renders
only to a `ScreenBuffer`. Form field labels currently default to schema names;
metadata and localized labels will be injected rather than baked into layout.
The initial inference is intentionally conservative: binary and generated data
are read-only, numeric declarations get numeric parsing, and boolean hints are
limited to explicit types and well-known field names. See ADR 0006.

On a failed save, the form preserves the dataset draft and maps a validation or
constraint error to the named schema field when SQLite supplies one. It selects
and marks that field; ambiguous failures are attributed only when exactly one
editable field changed.

### Destructive confirmation

`ui::ConfirmationDialog` is a reusable semantic dialog whose default selection
is cancel. It has no database dependency and receives all visible strings from
the presentation caller. Browse uses it before delegating deletion to
`Dataset::erase`; the dataset remains responsible for transactional execution
and stable primary-key identity. See ADR 0008.

### Relationship lookups

`Dataset::lookup_options` discovers a field's foreign-key schema, resolves a
display field with conservative name heuristics or validated metadata, and
returns a bounded owned key/label list. Search predicates cover only validated
referenced fields and bind all user text. `RecordForm` renders the label but
persists the key. Enter opens `RelationshipLookup`; F2 composes a read-only
`RelatedRecordView` above it on the document's semantic window stack. Widgets do
not query SQLite. See ADR 0009 and ADR 0026.

### Application presentation metadata

`appmeta::ApplicationMetadata` enhances inspected schema with table/field labels,
field order and visibility, additional read-only policy, explicit display
formats, and relationship lookup policy. Validation rejects unknown objects,
unsafe lookup fields, invalid limits, incomplete currency policy, and ambiguous
temporal annotations. Forms and grids consume this model without knowing where
it was stored. `ApplicationDefinition` owns named forms, views, commands, menus,
screens, reports, and settings loaded from reserved `_app_*` tables. Screens
are ordered semantic command links with no terminal coordinates.
`ScreenDocument` renders that model as a TUI navigation list and returns only a
selected command name; the workspace host re-dispatches it through
`ApplicationRuntime`. Loading an ordinary database is read-only and returns an
empty definition; explicit transactional migrations own all metadata schema
changes. UI code consumes the semantic model and never queries metadata tables.
See ADRs 0026, 0027, and 0032.

### Commands

The command parser produces stable `CommandId` values. `ApplicationRuntime`
validates command arity and resolves schema objects, then returns a semantic
`CommandResponse` for the caller to localize and render. The executable's
`--command` option is a non-interactive adapter. The workspace `Ctrl+P`
command palette uses the same parser/runtime, then opens semantic schema,
browse, report, or SQL documents rather than executing SQLite through a widget.
Named forms, screens, views, reports, and recursively bounded commands resolve from an
optional `ApplicationDefinition`. Metadata menu items return `run <name>` to
this same boundary; they do not invoke documents or SQL directly. In app mode,
reserved `_app_*` tables are hidden from normal navigation while low-level
SQLite introspection remains complete. See ADR 0028.

### Terminal rendering

Widgets render semantic cells into `ScreenBuffer`. A backend diffs frames and
encodes cells for the host. utf8proc supplies current Unicode cell-width data and
UTF-8 decoding. Extended grapheme layout is deliberately deferred; see ADR 0002.
Every ANSI style run starts with an SGR reset before applying its complete
semantic style. This prevents stateful underline, bold, or reverse attributes
from leaking from a highlighted cell into later cells or rows.
`ConsoleTerminal` is the current native adapter and is replaceable; see ADR 0004.
The `--terminal-diagnostics` document hosts the same adapter and reports only
normalized `KeyEvent` and `ResizeEvent` values. It provides a repeatable manual
verification surface without leaking CPP-Terminal types outside the terminal
boundary.

### Localization and display formatting

`core::Localizer` resolves external UTF-8 catalogs and formats typed named
arguments with ICU MessageFormat. `LocalizedMessage` owns a resolved pattern and
locale for workspace text that is bound now but formatted later; widgets never
parse placeholders themselves. `core::LocaleFormatter` applies pinned ICU/CLDR
number, currency, and date/time presentation while exposing no ICU types.
Grids use it for integer and real display plus metadata-annotated currency and
temporal fields. Temporal TEXT values use the strict date/time/RFC 3339 storage
contract in ADR 0026; formatting never mutates stored values. See ADR 0025.

`detect_console_capabilities` runs before `ConsoleTerminal` constructs its
CPP-Terminal session. Interactive modes require terminal-connected standard
input and output; redirected modes fail with plain structured errors before raw
mode or ANSI output. The non-interactive `--terminal-capabilities` command
reports the same decision. See ADR 0015.

### Browse query controls

The browse frontend uses a semantic `TextPrompt` widget for text search and
field filters. The widget owns text editing only; the frontend parses a small
comparison prefix grammar and calls `Dataset::search`, `where`, `order_by`, or
`refresh`. This keeps SQLite identifiers and values parameterized through the
dataset model. User-facing prompt and footer text comes from the `Localizer`,
so adding a translation does not alter commands, actions, or database names.

### Semantic actions and key bindings

`core::ActionMap` maps normalized `KeyEvent` values to stable `ActionId`
values such as `record.edit` and `dataset.refresh`. Browse controllers receive
only actions, so neither Windows virtual keys nor ANSI sequences become part of
application behavior. The built-in mapping supplies the documented browse keys;
callers can replace individual bindings through `ActionMap::bind`. User-local
configuration persists overrides as normalized keys and stable action IDs, not
translated labels or terminal escape sequences. See ADRs 0011 and 0031.

### SQL console boundary

`Database::run_sql` is the sole arbitrary-SQL boundary. It keeps the SQLite C
API internal, executes a complete script, and returns an owned `SqlResult` for
the final statement with columns. Result rows are bounded to protect terminal
renderers from accidental unbounded queries. `ui::SqlConsole` owns multiline
editing only; the frontend performs execution, converts an owned `SqlResult` to
`ui::GridRows`, and reuses `Grid` for result rendering. Neither widget performs
SQLite work. Once a result Grid exists, the document owns semantic editor/result
focus. `document.switch_pane` (F7 by default) changes which pane consumes
navigation actions; an unfocused selection remains visible without using the
focused-cell theme role.

Named reports use the separate `Database::run_query` boundary. It accepts one
SQLite-classified read-only statement with result columns, rejects scripts and
writes, and owns a bounded result. `ReportDocument` delegates navigation and
rendering to Grid. See ADR 0028.

`report::export_result` consumes that owned result outside of the UI layer and
writes CSV, JSON, text, HTML, or PDF. It owns output validation, locale-aware
human-readable formatting, temporary-file replacement, and recovery-safe
overwrite handling. PDFio and the embedded Unicode font are private report
implementation details; they are not terminal, widget, or application-metadata
dependencies. See ADR 0029.

### Lua business logic

`script::Runtime` is an optional application extension that consumes validated
`script::Definition` values from SQLite-resident metadata. Lua implementation
details stay private to `src/script`; datasets know only the owned-data
`model::DatasetLifecycle` contract. The runtime gives each hook a fresh,
resource-bounded interpreter and a plain record/context table, never a raw
SQLite handle, UI widget, terminal object, filesystem, or network capability.
Dataset write transactions enclose record hooks and SQL, while
`ApplicationRuntime` owns top-level command hooks. See ADR 0030.

## Deferred decisions

- Terminal library versus small native ANSI/Windows backends.
- Unicode segmentation and display-width library.
- Dataset public API and concurrency model.

Each requires a focused cross-platform spike and an ADR before adoption.
