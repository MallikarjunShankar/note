# System boundaries

The ten systems below follow section 5 of `PROJECT_SPEC.md`. This is a boundary reference, not a request to pre-create modules.

| System | Owns | Must not own/do |
|---|---|---|
| Application Core | Startup, lifecycle, shared app state, bootstrap configuration discovery | Editing, rendering policy, settings ownership, file operations |
| User Interface | Window, menus, tabs, editor view, status, settings UI, framework-specific drawing surface | Document storage, editing algorithms, file operations, syntax logic |
| Editor Engine | Text buffer, cursors, selections, editing, indentation, pairing, undo/redo, viewport draw decisions | Filesystem access; GUI-framework dependency beyond drawing interface |
| Document Management | Document instances/metadata, tabs, file service and persistence | Editing algorithms or syntax highlighting |
| Language System | Language registry/detection, syntax definitions, highlighting/cache | Completion, diagnostics, compilation, language servers |
| Customisation | Themes, settings, typography/layout preferences, keybindings | Editing document contents |
| Command and Input | Input routing, command registry/execution, palette and shortcuts | Implementation of underlying file/edit/platform actions |
| Search | Active-document matching, navigation, replacement | Cross-file or project-wide search |
| Session and Recovery | Session state and separate recovery snapshots | Normal file persistence |
| Platform Integration | Native dialogs, filesystem/path, external app and terminal launch adapters | Platform-specific leakage into editor/language/customisation |

Authoritative state ownership: text buffer owns text; cursor manager owns cursors; selection manager owns selections; undo/redo manager owns history; document manager owns document metadata/references; tab manager owns tab order; language system owns language identity; highlighter owns syntax state; theme/settings/keybinding managers own their respective data; command registry owns commands; `SessionRecoveryService` owns persisted session metadata and recovery snapshots, while Application Core supplies lifecycle callbacks; document management owns file persistence.

All systems are added incrementally as concrete requirements need them. No empty system modules are part of this foundation.

## Phase 02 implementation boundary

Production `editor::TextBuffer` stores canonical UTF-8 using an indexed piece table; it owns text and line-ending tags and has no Qt dependency. `BracketManager` provides configurable pair completion and on-demand nested matching without a parser. Document Management contains `Document`, `DocumentManager`, an ID-only `TabManager`, and a Qt Core `FileService` with chunked UTF-8/BOM and BOM-marked UTF-16 decoding and `QSaveFile` persistence. The manager owns document and buffer lifetimes; documents hold non-owning buffer references and file metadata only. The application-level `DocumentWorkflow` coordinates these interfaces, dispatches private worker results to the UI thread, and publishes only complete documents. The Main Window presents tabs and per-document `EditorView`s, which compose framework-independent cursor, selection, editing, undo/redo, and bracket managers with the Qt viewport renderer; input includes Alt+Click multi-cursor editing. File actions/dialogs, close prompts, and cancellable progress remain application-level. The bounded read-only `DocumentPreview` is retained as a legacy component but is no longer the active view. Platform Integration includes shared Unicode path conversion, a `QFileSystemWatcher` adapter over open documents, and external-application/system-terminal launch services. The workflow hashes file bytes off the UI thread to validate watcher events and before writes; OS watcher events are hints, not proof of content state.

The production subsystem remains partial: the preview is not an editor; async loading reports read progress but does not expose a partially built document; the piece-table index build is monolithic; filesystem notifications can be missed and require OS-specific validation; content inspection reads and hashes full files; session/recovery durability and cross-platform validation remain. The application integrates settings/theme/keybinding state, session/recovery lifecycle and clean/abnormal quit handling, but complete bootstrap, resource and platform validation remain. Tab order is owned by `TabManager`, active document identity by `DocumentManager`, and UI presents both. Do not move file I/O into the Editor Engine or make the Document Manager a tab-bar implementation.

The command, customisation and search packages share this boundary: `CommandRegistry` owns command metadata/execution, `KeybindingManager` owns persisted shortcut overrides, `SettingsManager` owns preferences, `ThemeManager` owns themes and transient previews, and `SearchEngine/MatchManager` own active-document match discovery. Existing Qt actions remain the shared interaction adapters; the palette and context menu execute registered commands rather than implementing parallel behavior. The editor routes registered key events through `ShortcutDispatcher` before preserving existing widget-input fallbacks.

## System 09 implementation boundary

`application::SessionRecoveryService` is constructed and started by Application Core. It reads/writes session metadata separately from documents, and receives snapshots of tab order, active index and cursor byte offsets from the UI through a provider. The service does not own document or editor state. One shared periodic timer and a resettable debounce timer persist the session and snapshot only modified buffers whose revision has not already been captured. Recovery snapshots are written atomically to the application data directory, not to source paths. Startup recovery creates new untitled documents; restoring a source document remains an ordinary Document Workflow open operation. Normal file persistence remains with Document Management.

The recovery format is versioned and checksummed. Text and line-ending tags are streamed from the piece table during snapshot writes; incomplete/corrupt snapshots are rejected, and a prior committed snapshot is preserved if a replacement is interrupted. System 09 remains partial until the full application lifecycle, UI choices, platform behavior and durability failure cases have been validated.
