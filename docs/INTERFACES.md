# Document foundation interfaces

These are the small Phase 02 interfaces available under `src/`. Qt is confined to `documents::FileService`; `editor::TextBuffer`, `Document`, and `DocumentManager` have no GUI-framework dependency. They are not UI or tab-bar interfaces.

## Text buffer (`editor::TextBuffer`)

- `sizeBytes()`, `lineCount()`, `revision()`, `pieceCount()`, and `estimatedStorageBytes()` provide size and diagnostic data.
- `text()`, `textRange(offset, length)`, `lineText(line)`, `lineRange(line)`, and `snapshot()` provide read access. Ranges use UTF-8 byte offsets, require Unicode scalar boundaries, and exclude the LF separator from a line range.
- `insert(offset, utf8, bareLineEnding)`, `erase(offset, length)`, and `replace(offset, length, utf8, bareLineEnding)` mutate text and increment revision for a real change. Incoming CRLF and CR are canonicalized while their line-ending tags are retained. Bare LF uses the supplied default style.
- Invalid UTF-8, out-of-range values, and boundaries inside a Unicode scalar fail with standard exceptions. The buffer is not internally synchronized; callers serialize access.

## Document and manager

- `Document::buffer()` provides a reference; the manager owns its lifetime. `Document::metadata()` is read-only. `modified()` compares text revision with the last successful save revision.
- `DocumentManager::createNew()` creates an untitled, empty document. `registerOpened(buffer, metadata)` takes ownership of a file-loaded buffer and metadata, rejecting null buffers, pathless opens, and duplicate open paths.
- `find(id)`, `setActiveDocument(id)`, `activeDocument()`, `size()`, and `close(id, discardModified)` manage document lookup/lifetime. Closing a dirty document requires explicit discard; there is no UI prompt here.
- `findByPath(path, except)` returns the already registered identity for canonical/equivalent paths. It lets Open activate an existing instance and lets Save As reject a path owned by a different open document.

## Tab manager and application workflow

- `documents::TabManager` stores a stable ordered list of document IDs and provides add/remove/index lookup. It owns no documents, active state, metadata, or contents.
- `application::DocumentWorkflow` coordinates the existing manager, tab order and file service. It provides create, open, activate, save, Save As, and close operations with explicit success, duplicate, cancellation, missing-path, not-found, and failure statuses.
- Closing a dirty document requires a caller-supplied Save/Discard/Cancel choice. Save As for an untitled document requires a path; empty paths represent cancellation. Save failures and open errors do not remove tabs or mark content saved.
- The Qt Main Window owns dialog and prompt presentation only. Its menu and keyboard shortcuts use the same `QAction` connections. There is no command registry in the current foundation, so no parallel command system is introduced.
- `ui::DocumentPreview` shows at most 16 KiB from the active document in a read-only widget. It is a temporary UI projection, never a source of text edits or document persistence.

## File service (`documents::FileService`)

- `open(path)` returns an `OpenFileResult` with a buffer and metadata on success, otherwise a diagnostic string. Detection accepts UTF-8 (with or without BOM) and BOM-marked UTF-16LE/BE. Invalid UTF-8/UTF-16 and UTF-32 are rejected; legacy encodings are never guessed.
- `save(document)` requires an associated path; `saveAs(document, path)` writes to a new target. Both return `FileOperationResult` and update document persistence state only after a successful commit.
- Saves use `QSaveFile` with direct-write fallback disabled. Bytes are written to a temporary file in the target directory and the target is replaced only by `commit()`. Read/write/commit errors are returned to the caller.

## Dependencies and thread contract

The implementation adds no external dependency beyond Qt 6 Core already included in the chosen Qt stack. The file service is synchronous and reads the full file before returning. Callers must not invoke it on a latency-sensitive UI thread for large files; asynchronous/progressive integration is outstanding. The text buffer and document metadata are single-threaded and require external serialization if later used across threads.
