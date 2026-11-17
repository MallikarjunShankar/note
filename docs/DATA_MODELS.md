# Production document data models

This records the Phase 02 data ownership implemented in `src/editor/` and `src/documents/`. It does not add cursor, selection, undo, tab-order, recovery, or UI state.

## Text buffer

`editor::TextBuffer` is the sole owner of document text. It stores UTF-8 and exposes offsets and lengths as UTF-8 byte coordinates. Edit and range boundaries must also be Unicode scalar boundaries; an operation that splits a multibyte UTF-8 character is rejected. Combining-mark/grapheme navigation is a later cursor/rendering concern.

Internally, line endings are normalized to one LF byte. A parallel source-backed tag records the original LF, CRLF, or CR form for every logical newline. The buffer snapshot contains canonical UTF-8 plus the ordered line-ending tags, so a save can reproduce mixed line endings exactly. A new bare LF inserted by an eventual editing controller can use the document's preferred line-ending convention; explicitly supplied CRLF or CR input retains its own form.

The representation is a piece table over immutable original UTF-8 storage and append-only added UTF-8 storage. An implicit treap indexes piece descriptors by subtree byte length and logical newline count. Each source has sorted newline byte offsets and corresponding line-ending tags; piece newline counts are found by rank queries. Adjacent contiguous pieces from the same source coalesce at edit boundaries. Line count is logical LF count plus one, including the empty final line after a trailing newline. `lineRange` excludes its separator.

The treap provides expected logarithmic piece-sequence edits and indexed line lookup, with range retrieval proportional to traversed pieces plus returned bytes. It does not provide a strict worst-case logarithmic guarantee; priority balancing, high-fragmentation behavior, and 100 MB memory growth remain validation points. `estimatedStorageBytes()` is a structure-payload estimate, not process RSS or allocator overhead.

## Document

`documents::Document` contains a manager-assigned process-local identity, a non-owning reference to its buffer, and metadata: optional path, encoding, preferred line ending, whether the loaded/current file uses mixed endings, and last known byte-size/modified-time information. It owns no text and performs no file I/O.

The modified state is derived from the buffer's monotonically increasing revision compared with the last successfully persisted revision. A new document has no path, UTF-8 without BOM, LF preference, and an empty buffer at a saved revision. A successful save associates its target path and updates the persisted revision only if the buffer still has the revision that was written. A failed save leaves the prior association and modified state intact.

## Ownership and lifetime

`DocumentManager` owns each open `Document` and its `TextBuffer` in one entry. The document's buffer reference is valid only while that entry remains registered. A close request for modified content returns `confirmationRequired` unless the caller explicitly chooses to discard. The manager assigns IDs and rejects a second registration for the same existing filesystem path. It tracks one active document ID but does not own tab ordering.

`FileService` returns an opened buffer and metadata; callers transfer both to `DocumentManager` when registration succeeds. Buffer text, document metadata, open-document ownership, and file persistence each have one owner.
