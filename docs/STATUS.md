# Implementation status

This is the current status ledger for Note. “Complete” means implemented and relevant tests/validation have passed; planned behavior and documentation alone do not qualify. See `PROJECT_SPEC.md` for the full requirements and `docs/DEVELOPMENT_PLAN.md` for dependency order.

## Project foundation

| Area | Status | Evidence / remaining work |
|---|---|---|
| Repository and build configuration | **Partial** | Git baseline is established. CMake/C++20/Qt 6.9.1 production shell builds on the current Windows host; Linux/macOS deployment and V1 release packaging remain. |
| Application shell | **Partial** | QApplication/QMainWindow integrates New/Open/Save/Save As/Close, native dialogs, document tabs, close prompts, per-document editing views, and System 09 startup/shutdown coordination through a separate session/recovery service. Full lifecycle, cross-platform and V1 validation remain. |
| Technology evaluation | **Partial** | Isolated project-owned widget and gap/piece buffer benchmarks are recorded in `docs/EDITOR_PROTOTYPE.md`. QScintilla was not measured; the low-end reference device and cross-platform validation remain outstanding. |
| Editor architecture decision | **Partial** | Project-owned viewport renderer is provisionally selected. The production buffer now uses an indexed implicit-treap piece table with newline indexes, but worst-case fragmentation, long-line and reference-device behavior remain. |
| Hardware targets and performance budgets | **Partial** | The development host is characterized and startup, idle memory, 1/10/50 MiB async-open, and 1/10/50/100 MiB FileService measurements are recorded in `docs/PERFORMANCE.md`. A 50 MiB copy-reduction comparison lowered sampled async-open peak private bytes by 32.9%; startup and idle memory still miss provisional values on this host. Values remain unconfirmed; the owner has not selected a reference device, and real-display, low-end, and cross-platform validation remain. |
| Phase 02 document foundation | **Partial** | Implementation includes production buffer/document ownership, ID-only tab order, lifecycle workflows, native dialogs, save/close prompts, encoding/line-ending handling, atomic persistence, chunked async open/reload, cancellation/progress, external-change monitoring with surfaced inspection errors, fingerprint-based save conflict protection, drag/drop opening, and external app/terminal launch. TextBuffer adopts initial snapshot storage and FileService streams buffer segments to avoid full-document copies during open/save. Focused regression tests cover external modifications, save conflict preservation, confirmed reload, missing-file detection, and segment traversal. The Windows Debug build and current CTest suite pass (15/15); reference-device and cross-platform validation, actual GUI key-to-paint/frame timing, and progressive content availability remain. Do not call Phase 02 verified or complete. |
| V1 release readiness | **Not started** | No agreed editor features, file handling, recovery, platform validation, performance verification, or release packaging are complete. |

## Revised V1 development sequence

The remaining work is organised into integrated packages:

1. **Commands, customisation and search** — Systems 06, 07 and 08.
2. **Session and recovery** — System 09 integrated with application lifecycle.
3. **Core completion and platform integration** — outstanding work in Systems 01–04 and 10.
4. **V1 integration, validation and final performance optimisation**.
5. **Windows packaging and release preparation**.

The Language System (Phase 05 and System 05) is deferred until after V1 and is **Not started** by policy, not by an omission in implementation.

## Outstanding owner decisions

- Select the low-end reference device and confirm the provisional performance budgets.
- Approve or reject Appendix A proposals: deeper personalisation, additional editing basics and post-V1 language coverage/detection.
- Approve configuration, theme and recovery storage formats.
- Approve build and packaging technology decisions for the Windows release.

## System status

| System | Status | Remaining work |
|---|---|---|
| 01 Application Core | **Partial** | The entry point owns the document workflow and session/recovery service lifetimes, establishes the application identity before resolving per-user storage, and connects application quit to recovery preservation. Settings, theme and keybinding managers are integrated through the UI; no resource bundle is currently required. Full bootstrap/error-path, lifecycle, cross-platform and release validation remain. |
| 02 User Interface | **Partial** | Main Window integrates per-document editor views with keyboard and mouse input, viewport rendering, tab switching, dirty presentation, document workflows, persistent settings/theme application, shared-action context menus, command palette, and find/replace UI. Theme preview is transient and Cancel restores the saved appearance. Wrapped-line scrollbar proportions are indexed in debounced 64-line chunks and covered by offscreen tests. Real-display, accessibility, platform and large-document validation remain. `DocumentPreview` remains in the build but is no longer the active editor surface. |
| 03 Editor Engine | **Partial** | Production UTF-8 piece-table buffer with indexed piece/newline metadata, move-adopting constructors, segment-view traversal, and focused tests exists in `src/editor/`. Cursor/selection managers own scalar-safe multiple cursor and selection state; the editing engine batches multi-cursor edits, handles mixed selection/deletion and targeted cursor insertion, configurable selected-line indentation/outdentation, and rejects ambiguous ranges before mutation. Undo/Redo records grouped atomic transactions and restores document, cursor, and selection state. `BracketManager` supports configurable pairing for parentheses, square/curly brackets, and quotes, nested matching, closer skipping, paired deletion, and quote-after-word suppression. `EditorView` routes registered shortcuts through the shared dispatcher while retaining existing editor input fallbacks. Reference-device, cross-platform and broader editing/performance validation remain. |
| 04 Document Management | **Partial** | Production Document Model/Manager, ID-only tab ordering, application workflow, and Qt Core File Service support chunked background reads/reloads, progress/cancellation, UTF-8/BOM, BOM-marked UTF-16, mixed LF/CRLF/CR preservation, duplicate-path activation, segment-streamed saves, atomic commits, SHA-256 disk fingerprints, conflict rejection and safe reload publication. Focused tests cover lifecycle, external changes, conflict preservation, reload identity, missing files, and segment traversal. File-inspection errors are now reported to the UI rather than silently ignored. Formal platform and reference-device large-file validation remain. |
| 05 Language System | **Deferred** | Deferred until after V1. V1 does not implement detection, registry, definitions, highlighting or cache; syntax-colour categories may be reserved in customisation data structures only. |
| 06 Customisation | **Partial** | Settings and INI persistence, built-in/custom JSON themes, atomic import/export, transient theme preview, theme-name validation, settings dialog, typography, layout, editing preferences and live keybinding/settings application are implemented. Remaining work includes broader keybinding coverage, complete toolbar configuration UX, individual pairing controls, theme reset/search polish and cross-platform runtime validation. |
| 07 Command and Input | **Partial** | A stable command registry/executor, Keybinding Manager, Shortcut Dispatcher, command palette and shared-action context menus are integrated with existing Qt actions. Editor shortcuts route through the dispatcher with existing input fallbacks retained; presentation action checked states follow persisted settings. End-to-end shortcut-conflict UX and native keyboard-only/accessibility validation remain. |
| 08 Search | **Partial** | Active-document find/replace UI, plain and regular-expression modes, case/whole-word options, match highlighting and undoable individual/all replacement are implemented. Remaining work includes stronger chunk-level regex timeout protection, cross-chunk Unicode edge validation and complete UI/navigation tests. |
| 09 Session and Recovery | **Partial** | Application startup restores saved path order, active tab and cursor offsets; inaccessible paths are skipped with a status message. Session state uses atomic JSON. Modified buffers are debounced and periodically snapshotted separately using atomic, checksummed recovery files, with segment streaming to avoid full-document copies during save. Startup detects an unclean prior run and offers recover-as-untitled, discard or later; original files are never replaced. Focused storage tests cover normal/abnormal runs, multiple and large documents, corruption/interruption, deferred and repeated recovery, and external changes to originals. UI lifecycle, crash injection, platform behavior and broader durability validation remain. |
| 10 Platform Integration | **Partial** | Native Qt Open/Save dialogs, Unicode path conversion, `QFileSystemWatcher` file/directory monitoring with refresh, dropped-file opening, and external-app/system-terminal launch are implemented behind platform-facing helpers. Launch adapters validate target paths and report failure; regression coverage verifies Unicode round-trips and invalid targets without launching external processes. Windows build/test only; actual native launches, Linux/macOS behavior and watcher reliability require validation. |

No system is marked **Complete** in this baseline.
