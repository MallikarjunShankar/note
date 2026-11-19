# Document foundation interfaces

These are the small Phase 02 interfaces available under `src/`. Qt is confined to persistence/platform/UI boundaries; `editor::TextBuffer`, `Document`, and `DocumentManager` have no GUI-framework dependency. These are not UI or tab-bar interfaces.

## Text buffer (`editor::TextBuffer`)

- `sizeBytes()`, `lineCount()`, `revision()`, `pieceCount()`, and `estimatedStorageBytes()` provide size and diagnostic data.
- `text()`, `textRange(offset, length)`, `lineText(line)`, `lineRange(line)`, `isScalarBoundary(offset)`, `byteAt(offset)`, and `snapshot()` provide read access. Ranges use UTF-8 byte offsets, require Unicode scalar boundaries, and exclude the LF separator from a line range.
- `insert(offset, utf8, bareLineEnding)`, `erase(offset, length)`, and `replace(offset, length, utf8, bareLineEnding)` mutate text and increment revision for a real change. Incoming CRLF and CR are canonicalized while their line-ending tags are retained. Bare LF uses the supplied default style.
- Invalid UTF-8, out-of-range values, and boundaries inside a Unicode scalar fail with standard exceptions. The buffer is not internally synchronized; callers serialize access.

## Cursor manager (`editor::CursorManager`)

- `CursorManager` accepts a `TextBuffer` reference and owns only cursor state. It does not contain document text or perform file I/O.
- Primary and secondary positions are UTF-8 byte offsets. `setPrimaryCursor`, `moveToOffset`, `addSecondaryCursor`, and line/column navigation clamp positions to document bounds and Unicode scalar boundaries.
- `moveLeft`/`moveRight` move every active cursor horizontally. `moveUp`/`moveDown` move every active cursor vertically and retain a preferred byte column across shorter lines.
- `moveToLineColumn(line, column)` uses 0-based line and UTF-8 byte-column coordinates. Empty lines, end-of-line positions, trailing newline lines, and the final line are supported.
- `primaryOffset`, `primaryLocation`, `cursorCount`, `secondaryCursorCount`, `secondaryCursorOffsets`, and `secondaryCursorOffset` provide read access.
- `clearSecondaryCursors`, `removeSecondaryCursor`, and `syncToBuffer` manage secondary cursor state and stale offsets.
- `onTextReplaced(offset, removedBytes, insertedBytes)` adjusts cursor offsets after a completed buffer replacement. This is a cursor-state notification only; the manager does not insert or delete text.

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
- The Qt Main Window owns dialog and prompt presentation only. Its menu and keyboard shortcuts use the same `QAction` connections. There is no command registry in the current foundation, so no parallel command system is introduced.
- `ui::DocumentPreview` shows at most 16 KiB from the active document in a read-only widget. It is a temporary UI projection, never a source of text edits or document persistence.

## File service (`documents::FileService`)

- `open(path)` returns an `OpenFileResult` with a buffer and metadata on success, otherwise a diagnostic string. Its `OpenOptions` overload reads fixed-size chunks (default 256 KiB), tracks decoder and line-ending state across boundaries, reports bytes read, checks cancellation between chunks, and verifies file size and modification time after reading. Detection accepts UTF-8 (with or without BOM) and BOM-marked UTF-16LE/BE. Invalid UTF-8/UTF-16 and UTF-32 are rejected; legacy encodings are never guessed.
- `save(document)` requires an associated path; `saveAs(document, path)` writes to a new target. Both return `FileOperationResult` and update document persistence state only after a successful commit.
- Saves use `QSaveFile` with direct-write fallback disabled. Bytes are written to a temporary file in the target directory and the target is replaced only by `commit()`. Read/write/commit errors are returned to the caller.

## Dependencies and thread contract

The implementation adds no external dependency beyond the selected Qt stack. The workflow runs file reading, decoding, line normalization, index construction and watcher-driven fingerprint inspection on a two-worker process-lifetime thread pool. Workers own private results; document/tab registration and UI callbacks happen on the workflow's Qt thread. Cancellation is checked between read chunks and after index construction; the current index constructor is monolithic, so cancellation during that stage takes effect after construction returns. Shutdown marks open requests cancelled and suppresses later callbacks without blocking the UI; inspection jobs may finish their current full-file hash. The File Service hashes exact bytes with SHA-256, including a final verification pass after open. Before save it compares the current on-disk hash with the loaded/saved baseline; Save As requires confirmation and rechecks the destination before atomic commit. This catches same-size edits and coarse timestamps, but a small race remains between the final check and replacement, and synchronous pre-save hashing can delay the UI for large files. Errors during background inspection do not get reported as modifications; activation or a later event retries. The text buffer and document metadata remain single-threaded.
