# ADR 0023: Text selection, clipboard, paste, and history

## Status

Accepted.

## Context

Prompts, generated fields, and the SQL console already shared cursor-aware UTF-8
editors, but lacked selection, word movement, clipboard exchange, bulk-paste
normalization, overwrite mode, and recoverable SQL editing. Implementing those
features independently in each surface would produce inconsistent behavior and
would couple widgets to operating-system APIs.

## Decision

`LineEditor` and `MultilineEditor` own byte-offset cursors and normalized
selection anchors that always remain on UTF-8 code-point boundaries. Shift
extends a selection, Ctrl+Left/Right moves by Unicode letter/mark/number words,
Ctrl+A selects all, and Insert switches between insert and overwrite modes when
the editor configuration permits it.

Vulpes uses the classic terminal-safe clipboard bindings Ctrl+Insert,
Shift+Delete, and Shift+Insert. A small `core::Clipboard` interface is injected
into UI composition roots. Its production implementation delegates UTF-8 text
exchange to the MIT-licensed dacap/clip library; tests use in-memory adapters.
No widget includes host clipboard headers.

CPP-Terminal copy/paste payloads normalize to a distinct owning `PasteEvent`.
Editors insert the payload atomically, normalize CRLF, and discard unsupported
control characters. A pasted escape sequence is therefore data, not a series of
application commands.

`MultilineEditor` snapshots changed states into an undo journal bounded to 100
states and 4 MiB of source text. Redo is discarded by a divergent edit.
`SqlConsole` owns a separate 100-entry, adjacent-deduplicated command history;
Ctrl+Up/Down navigates it without losing the current draft.

## Consequences

- All current text-entry surfaces share selection and paste semantics.
- Future frontends can provide a different clipboard implementation without
  changing editor state or command routing.
- Copy failure is a non-destructive no-op; paste failure leaves text unchanged.
- Undo snapshots favor simple deterministic behavior over complex operation
  coalescing. The explicit byte and state bounds cap memory use.
- Ctrl+C remains the application quit binding. Ctrl+Insert avoids an ambiguous
  copy-versus-quit rule in terminal hosts.
