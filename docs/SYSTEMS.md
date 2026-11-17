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

Authoritative state ownership: text buffer owns text; cursor manager owns cursors; selection manager owns selections; undo/redo manager owns history; document manager owns document metadata/references; tab manager owns tab order; language system owns language identity; highlighter owns syntax state; theme/settings/keybinding managers own their respective data; command registry owns commands; session manager owns session data; recovery manager owns snapshots; document management owns file persistence.

All systems are added incrementally as concrete requirements need them. No empty system modules are part of this foundation.

## Phase 02 implementation boundary

Production `editor::TextBuffer` stores canonical UTF-8 using an indexed piece table; it owns text and line-ending tags and has no Qt dependency. Document Management contains `Document`, `DocumentManager`, an ID-only `TabManager`, and a Qt Core `FileService` with UTF-8/BOM and BOM-marked UTF-16 handling and `QSaveFile` persistence. The manager owns document and buffer lifetimes; documents hold non-owning buffer references and file metadata only. The application-level `DocumentWorkflow` coordinates these interfaces and does not own text. The Main Window presents tabs, file actions/dialogs, close prompts, and a bounded read-only preview.

The production subsystem remains partial: the preview is not an editor; loading is synchronous; there is no asynchronous/progressive open, external-modification detection, recovery, or cross-platform validation. Tab order is owned by `TabManager`, active document identity by `DocumentManager`, and UI presents both. Do not move file I/O into the Editor Engine or make the Document Manager a tab-bar implementation.
