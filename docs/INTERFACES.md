# Document foundation interfaces

These are the small Phase 02 interfaces available under `src/`. Qt is confined to persistence/platform/UI boundaries; `editor::TextBuffer`, `Document`, and `DocumentManager` have no GUI-framework dependency. These are not UI or tab-bar interfaces.

## Text buffer (`editor::TextBuffer`)

- `sizeBytes()`, `lineCount()`, `revision()`, `pieceCount()`, and `estimatedStorageBytes()` provide size and diagnostic data.
- `text()`, `textRange(offset, length)`, `lineText(line)`, `lineRange(line)`, `isScalarBoundary(offset)`, `byteAt(offset)`, and `snapshot()` provide read access. Ranges use UTF-8 byte offsets, require Unicode scalar boundaries, and exclude the LF separator from a line range.
- `visitSegments(visitor)` traverses the buffer in document order without assembling a full-text copy. Each callback receives a UTF-8 view and the corresponding line-ending tags; views are valid only during the callback, and returning `false` stops traversal.
- `insert(offset, utf8, bareLineEnding)`, `erase(offset, length)`, and `replace(offset, length, utf8, bareLineEnding)` mutate text and increment revision for a real change. Incoming CRLF and CR are canonicalized while their line-ending tags are retained. Bare LF uses the supplied default style.
- Invalid UTF-8, out-of-range values, and boundaries inside a Unicode scalar fail with standard exceptions. The buffer is not internally synchronized; callers serialize access.

## Cursor manager (`editor::CursorManager`)

- `CursorManager` accepts a `TextBuffer` reference and owns only cursor state. It does not contain document text or perform file I/O.
- Primary and secondary positions are UTF-8 byte offsets. `setPrimaryCursor`, `moveToOffset`, `addSecondaryCursor`, and line/column navigation clamp positions to document bounds and Unicode scalar boundaries.
- `moveLeft`/`moveRight` move every active cursor horizontally. `moveUp`/`moveDown` move every active cursor vertically and retain a preferred byte column across shorter lines.
- `moveToLineColumn(line, column)` uses 0-based line and UTF-8 byte-column coordinates. Empty lines, end-of-line positions, trailing newline lines, and the final line are supported.
- `primaryOffset`, `primaryLocation`, `cursorCount`, `secondaryCursorCount`, `secondaryCursorOffsets`, and `secondaryCursorOffset` provide read access.
- `clearSecondaryCursors`, `removeSecondaryCursor`, `setSecondaryCursor`, and `syncToBuffer` manage secondary cursor state and stale offsets.
- `onTextReplaced(offset, removedBytes, insertedBytes)` adjusts cursor offsets after a completed buffer replacement. This is a cursor-state notification only; the manager does not insert or delete text.

## Selection Manager (`editor::SelectionManager`)

- `SelectionManager` accepts a `TextBuffer` reference and owns only selection state. Each selection stores an anchor and active endpoint as UTF-8 byte offsets.
- `setSelection`, `addSelection`, `removeSelection`, `setAnchor`, and `setActive` clamp endpoints to document bounds and Unicode scalar boundaries. `normalizedRange` returns document-order `[begin, end)` coordinates; `hasSelection` distinguishes empty selections.
- `clearSelection` collapses one selection to its active endpoint; `clearSelections` removes all selections. `syncToBuffer` clamps state after external buffer changes.
- `onTextReplaced(offset, removedBytes, insertedBytes)` applies an explicit endpoint transformation after a completed `TextBuffer` replacement. Insertions at an endpoint shift it forward; endpoints inside a removed range map to the replacement end; invalid edit coordinates throw rather than being guessed.
- Selection state is independent of `CursorManager`; an editing or input controller supplies cursor endpoints when extending or moving selections.

## Editing Engine (`editor::EditingEngine`)

- `EditingEngine` accepts a mutable `TextBuffer` and references to the existing `CursorManager` and `SelectionManager`; it owns no text, cursor, or selection state.
- `insertText`, `backspace`, `deleteForward`, and `insertNewline` operate on all cursor positions. Backspace and forward delete remove a corresponding non-empty selection and otherwise delete adjacent text, allowing mixed multi-cursor selections and empty cursors in one validated batch. `replaceSelections` replaces every managed selection (including empty selections used as insertion cursors); `deleteSelections` removes only non-empty ranges.
- `insertTextAtCursorIndices` supports a validated subset of active cursors for operations such as skipping existing closing characters. All resulting offset transforms still go through the engine and buffer managers.
- Operations use UTF-8 scalar boundaries and preserve `TextBuffer` line-ending metadata. Multiple edits are validated before mutation, reject duplicate/overlapping ranges, and apply in descending document order so offsets remain stable. Ambiguous edits throw `std::invalid_argument` rather than being discarded.
- Editing methods return `EditBatch` records containing each offset and removed/inserted text. These records are the integration boundary for the future Undo/Redo Manager; no undo history is owned here.
- `CursorManager::setSecondaryCursor` updates an existing secondary cursor without transferring cursor ownership to the editing engine.

## Undo/Redo Manager (`editor::UndoRedoManager`)

- `UndoRedoManager` accepts the existing `TextBuffer`, `CursorManager`, `SelectionManager`, and `EditingEngine`; it owns only reversible transaction history.
- The editing methods mirror `EditingEngine` and record insertion, deletion, newline, and selection replacement as atomic transactions. `undo` and `redo` replay edit batches without creating new history entries, then restore the captured cursor and selection states.
- `beginTransaction`/`endTransaction` provide explicit grouping boundaries; the final cursor/selection state is captured at `endTransaction`. Single-scalar typing and consecutive backspaces may group only when the editor state is unchanged between operations; cursor or selection movement and incompatible operations prevent grouping.
- History memory is accounted as edit-record metadata, UTF-8 payloads, line-ending metadata, and captured cursor/selection state. `setMemoryLimit` evicts oldest complete transactions; an individual transaction larger than the limit is safely omitted.
- Edit records retain `TextSnapshot` payloads so undo/redo restores exact UTF-8 text and LF/CRLF/CR metadata. No full-document snapshots are used.

## Commands and keybindings

- `commands::CommandRegistry` owns stable command identifiers, metadata, callbacks and enabled-state checks. `CommandExecutor` delegates to the same registry.
- `commands::KeybindingManager` stores default and user shortcut mappings and persisted INI overrides. `ShortcutDispatcher` resolves key events through the same manager and executes registered commands.
- Existing Qt `QAction` instances remain interaction adapters. Menus, toolbar items, context menus and the command palette route through the registry; they do not maintain separate command implementations.

## Customisation

- `customisation::SettingsManager` owns validated application preferences and persists them safely through Qt INI storage.
- `customisation::ThemeManager` provides built-in themes, validates and loads JSON custom themes, and supports import/export. Theme data may reserve future syntax-category colours but no highlighting runtime is implemented.
- The settings UI applies typography, layout and editing preferences to existing editor views. Theme application changes renderer/UI colours without recreating document buffers.

## Search

- `search::SearchEngine` searches the active `TextBuffer` in bounded UTF-8 chunks. It supports literal and Qt regular-expression modes, case sensitivity and whole-word matching, with invalid-pattern and bounded-abort reporting.
- Match offsets are UTF-8 byte coordinates. `ui::SearchPanel` navigates matches, sends selected ranges to `EditorView`, highlights ranges in `EditorRenderer`, and performs individual or all replacements through the existing undo/redo transaction.

## Editor Renderer (`ui::EditorRenderer`)

- `EditorRenderer` is a Qt Widgets `QAbstractScrollArea` that accepts non-owning references to a `TextBuffer`, `CursorManager`, and `SelectionManager`. It owns viewport presentation state only and performs no editing or file I/O.
- `EditorRenderConfiguration` centralizes font, tab width, line height, line-number mode, current-line highlighting, indentation guides, whitespace visibility, matching occurrences, wrapping, and their foreground/background colors. Presentation options are in-memory renderer configuration; persistent settings and theme ownership remain future work.
- `visibleLineRange`, `scrollToLine`, and `setHorizontalOffset` expose viewport positioning. Painting requests only visible line ranges plus one small guard row; it does not materialize the document.
- `cursorGeometry` and `selectionGeometry` expose scalar-safe caret and single/multiline selection geometry. `offsetAtPosition` maps viewport coordinates to UTF-8 scalar boundaries; `ensureCursorVisible` scrolls a caret into view, and `refreshFromModel` updates viewport and scrollbar state after external edits.
- Absolute or relative line numbers, active-line highlighting, indentation guides, visible spaces/tabs/line endings, and exact identifier-like occurrence highlighting are supported. Occurrence matching is case-sensitive, syntax-agnostic, and limited to visible row windows.
- Optional wrapping breaks at whitespace where possible and otherwise at UTF-8 scalar boundaries. Wrapped hit-testing, cursor/selection geometry, and wheel scrolling follow visual rows. The vertical scrollbar still represents logical lines rather than total wrapped rows, so its thumb is not proportional to wrapped document height.
- Tabs use configurable tab stops and Unicode text is measured through Qt font metrics. Painting limits line processing to the viewport plus a small guard row; wrapped layout still processes each visible logical line in full, so exceptionally long lines can be expensive.
- `setMatchingBrackets` supplies cached matching offsets; matching outlines are painted only on their visible lines. The renderer never performs bracket searches itself.
- The renderer and presentation behavior are tested offscreen. A Release offscreen benchmark measures repeated viewport paints on a deterministic 100,000-line fixture; neither test nor benchmark establishes real-display latency or performance targets.

## Bracket Manager (`editor::BracketManager`)

- The framework-independent manager supports individually configurable `()`, `[]`, `{}`, `""`, and `''` pairs. `shouldPairAt` enforces scalar-safe offsets and the specified no-quote-pairing-after-a-word-character rule.
- `matchingAtCursor` finds a nested `()`, `[]`, or `{}` match adjacent to the caret without copying the document. The UI requests matching only when the cursor or document revision changes and caches the result for painting. Worst-case search can traverse the span to a matching bracket; no language parser or comment/string index is implemented.
- `emptyPairAtCursor` identifies enabled adjacent pairs for atomic paired deletion.

## Editor View (`ui::EditorView`)

- Each open document has one `EditorView`, created and owned by the Main Window's editor stack. It borrows the document-owned buffer and owns that document's `CursorManager`, `SelectionManager`, `EditingEngine`, and `UndoRedoManager`; the renderer borrows those interfaces.
- The view handles text input, Backspace/Delete, Enter with inherited indentation, Tab insertion, arrow/Home/End navigation, Shift selection, Select All, copy/cut/paste, undo/redo, mouse cursor placement, drag selection, and double-click word selection. Alt+Click toggles a secondary cursor; ordinary click returns to one cursor. Editing is routed through `UndoRedoManager` and `EditingEngine`; document contents remain owned by the existing document/buffer layer.
- Paired insertion, existing-closer skipping, and empty-pair deletion use `BracketManager` and undo transactions. Multi-cursor typing, deletion, bracket pairing, and independent selections operate in one edit batch; conflicting duplicate/overlapping cursors are coalesced by retaining the primary/earlier cursor, while the engine rejects ambiguous edit ranges before mutation. Multi-selection clipboard text is joined with LF.
- Switching tabs retains each live view and its cursor, selection, and history. Closing a document deletes its view. Successful in-place reloads increment `Document::contentGeneration`; the Main Window replaces the view so stale editor offsets and history cannot survive replaced contents.
- The view reports edits to the Main Window for dirty-state presentation. Saving continues through `DocumentWorkflow`; no editor view performs file I/O. Open, Save, Save As, conflict handling, and unsaved-close decisions remain owned by existing document workflows and UI actions.
- The input layer currently provides basic indentation and four-space Tab insertion. It does not provide language-aware quote/comment context, the full command/keybinding system, configurable indentation settings, or syntax services. Offscreen tests cover integration flows but do not validate native display behavior or large-file latency.

## Document and manager

- `Document::buffer()` provides a reference; the manager owns its lifetime. `Document::metadata()` is read-only. `modified()` compares text revision with the last successful save revision.
- `DocumentManager::createNew()` creates an untitled, empty document. `registerOpened(buffer, metadata)` takes ownership of a file-loaded buffer and metadata, rejecting null buffers, pathless opens, and duplicate open paths.
- `find(id)`, `setActiveDocument(id)`, `activeDocument()`, `size()`, and `close(id, discardModified)` manage document lookup/lifetime. Closing a dirty document requires explicit discard; there is no UI prompt here.
- `findByPath(path, except)` returns the already registered identity for canonical/equivalent paths. It lets Open activate an existing instance and lets Save As reject a path owned by a different open document.

## Tab manager and application workflow

- `documents::TabManager` stores a stable ordered list of document IDs and provides add/remove/index lookup. It owns no documents, active state, metadata, or contents.
- `application::DocumentWorkflow` coordinates the existing manager, tab order and file service. It provides create, synchronous open for non-UI callers, asynchronous open/reload with progress/completion callbacks, cancellation, activate, save, Save As, close, shutdown, and external-change refresh operations. Async requests use stable request IDs; exact/canonical pending paths coalesce, and registered duplicates activate the existing document. A platform `FileMonitor` reports native watcher hints; the workflow validates current content fingerprints on its bounded worker pool and notifies the UI through a document-ID/state callback.
- `platform::FileMonitor` wraps Qt `QFileSystemWatcher`, monitoring existing open files and their parent directories, then rebuilding watch registrations after events or application activation. The parent-directory watch catches deletion, moves and atomic replacement; the workflow determines actual state by file inspection. Watcher registration can fail or events can be lost, so activation and pre-save inspection are fallback checks.
- `platform::pathToQString` / `pathFromQString` centralize Windows wide-path and POSIX UTF-8 conversions. `platform::openInExternalApplication` and `openTerminalInDirectory` isolate OS launch behavior.
- Closing a dirty document requires a caller-supplied Save/Discard/Cancel choice. Save As for an untitled document requires a path; empty paths represent cancellation. Save failures and open errors do not remove tabs or mark content saved.
- The Qt Main Window owns dialog and prompt presentation, tab-to-view lifecycle, and editor presentation. Menus and keyboard shortcuts use the shared command registry, keybinding manager and Qt action adapters; text input remains in `EditorView`.
- `ui::DocumentPreview` shows at most 16 KiB from the active document in a read-only widget. It is a temporary UI projection, never a source of text edits or document persistence.

## File service (`documents::FileService`)

- `open(path)` returns an `OpenFileResult` with a buffer and metadata on success, otherwise a diagnostic string. Its `OpenOptions` overload reads fixed-size chunks (default 256 KiB), tracks decoder and line-ending state across boundaries, reports bytes read, checks cancellation between chunks, and verifies file size and modification time after reading. Detection accepts UTF-8 (with or without BOM) and BOM-marked UTF-16LE/BE. Invalid UTF-8/UTF-16 and UTF-32 are rejected; legacy encodings are never guessed.
- `save(document)` requires an associated path; `saveAs(document, path)` writes to a new target. Both return `FileOperationResult` and update document persistence state only after a successful commit.
- Saves use `QSaveFile` with direct-write fallback disabled. Bytes are written to a temporary file in the target directory and the target is replaced only by `commit()`. Read/write/commit errors are returned to the caller.

## Session and recovery (`application::SessionRecoveryService`)

- Application Core constructs the service with the application data directory and calls `start()` before creating the Main Window. `SessionState` contains ordered document paths, stable recovery keys, byte-coordinate cursor offsets, the active tab index and whether the previous shutdown was clean.
- The Main Window supplies session and modified-document providers. `scheduleSessionSave()` and `scheduleRecoverySnapshot()` reset one shared debounce timer; a single periodic timer also persists both categories. Clean documents and unchanged buffer revisions do not produce recovery writes.
- Session metadata is versioned JSON written through `QSaveFile`. Recovery records are separate versioned binary files containing source-path context, canonical UTF-8 bytes, line-ending tags and a SHA-256 integrity digest. `TextBuffer::visitSegments` streams document content during snapshot writes; no complete text snapshot is assembled.
- `recoveries()` rejects malformed, truncated, invalid-UTF-8 or checksum-mismatched records. `removeRecovery(key)` prevents the same snapshot being offered repeatedly. Recovering content creates a new untitled document; it never writes to the original path. `finishNormally(state)` removes snapshots only after successful normal lifecycle resolution, while the explicit Later choice retains recovery data and an unclean marker for the next launch.
- Missing/inaccessible session paths are reported and skipped during ordinary workflow restoration. File contents continue to load through `DocumentWorkflow`; the recovery service does not bypass Document Management.

## Dependencies and thread contract

The implementation adds no external dependency beyond the selected Qt stack. The workflow runs file reading, decoding, line normalization, index construction and watcher-driven fingerprint inspection on a two-worker process-lifetime thread pool. Workers own private results; document/tab registration and UI callbacks happen on the workflow's Qt thread. Cancellation is checked between read chunks and after index construction; the current index constructor is monolithic, so cancellation during that stage takes effect after construction returns. Shutdown marks open requests cancelled and suppresses later callbacks without blocking the UI; inspection jobs may finish their current full-file hash. The File Service hashes exact bytes with SHA-256, including a final verification pass after open. Before save it compares the current on-disk hash with the loaded/saved baseline; Save As requires confirmation and rechecks the destination before atomic commit. This catches same-size edits and coarse timestamps, but a small race remains between the final check and replacement, and synchronous pre-save hashing can delay the UI for large files. Errors during background inspection do not get reported as modifications; activation or a later event retries. The text buffer and document metadata remain single-threaded.
