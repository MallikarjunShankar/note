# Implementation status

This is the current status ledger for Note. “Complete” means implemented and relevant tests/validation have passed; planned behavior and documentation alone do not qualify. See `PROJECT_SPEC.md` for the full requirements and `docs/DEVELOPMENT_PLAN.md` for dependency order.

## Project foundation

| Area | Status | Evidence / remaining work |
|---|---|---|
| Repository and build configuration | **Partial** | Git baseline is established. CMake/C++20/Qt 6.9.1 production shell builds on the current Windows host; Linux/macOS deployment and V1 release packaging remain. |
| Application shell | **Partial** | QApplication/QMainWindow now integrates New/Open/Save/Save As/Close, native dialogs, document tabs, close prompts, and a bounded read-only preview. Lifecycle services, configuration, settings/themes, command registry, and production editing view remain. |
| Technology evaluation | **Partial** | Isolated project-owned widget and gap/piece buffer benchmarks are recorded in `docs/EDITOR_PROTOTYPE.md`. QScintilla was not measured; the low-end reference device and cross-platform validation remain outstanding. |
| Editor architecture decision | **Partial** | Project-owned viewport renderer is provisionally selected. The production buffer now uses an indexed implicit-treap piece table with newline indexes, but worst-case fragmentation, long-line and reference-device behavior remain. |
| Hardware targets and performance budgets | **Partial** | Provisional minimum profile and repeatable measurement/stress methodology are documented in `docs/PERFORMANCE.md`. Values remain unconfirmed; no physical low-end validation or production performance claim has been made. |
| Phase 02 document foundation | **Partial** | Implementation includes production buffer/document ownership, ID-only tab order, lifecycle workflows, native dialogs, save/close prompts, encoding/line-ending handling, atomic persistence, chunked async open/reload, cancellation/progress, external-change monitoring, fingerprint-based save conflict protection, drag/drop opening, and external app/terminal launch. This milestone has build and launch smoke validation only; focused regression tests, complete lifecycle verification, cross-platform validation, and performance benchmarking remain. Do not call Phase 02 verified or complete. |
| V1 release readiness | **Not started** | No agreed editor features, file handling, recovery, platform validation, performance verification, or release packaging are complete. |

## System status

| System | Status | Remaining work |
|---|---|---|
| 01 Application Core | **Partial** | The entry point owns the document workflow lifetime and launches the Main Window. Full lifecycle coordination, application-wide state, bootstrap configuration, and resource handling remain. |
| 02 User Interface | **Partial** | Main Window provides a minimal File menu, tabs, status display, native file dialogs, close prompts, and a bounded read-only preview. Settings UI, context menus, production drawing surface/editor view, and full theme integration remain. |
| 03 Editor Engine | **Partial** | Production UTF-8 piece-table buffer with indexed piece/newline metadata and focused tests exists in `src/editor/`. The framework-independent Cursor Manager now owns a primary cursor and secondary cursors, with UTF-8 scalar-safe byte offsets, clamped line/column navigation, horizontal/vertical movement, preferred vertical columns, and post-replacement offset adjustment. The Selection Manager now owns scalar-safe anchor/active endpoints, normalized forward/backward ranges, empty and multiple selections, clearing, and explicit post-replacement transformations with focused tests. The Editing Engine now performs scalar-safe insertion/deletion/newline/selection replacement through those managers, validates multi-edit conflicts, applies edits deterministically, and returns reversible edit records. The Undo/Redo Manager now records grouped bounded transactions, restores exact text metadata and cursor/selection state, and invalidates redo after new edits. Renderer interfaces, indentation, pairing, syntax, and reference-device validation remain. |
| 04 Document Management | **Partial** | Production Document Model/Manager, ID-only tab ordering, application workflow, and Qt Core File Service support chunked background reads/reloads, progress/cancellation, UTF-8/BOM, BOM-marked UTF-16, mixed LF/CRLF/CR preservation, duplicate-path activation, Save As, atomic saves, SHA-256 disk fingerprints, conflict rejection and safe reload publication. Focused tests are still required for these new paths; formal platform and large-file validation remain. |
| 05 Language System | **Not started** | Registry, data-driven detection/definitions, incremental highlighting, and cache remain. |
| 06 Customisation | **Not started** | Settings, themes, typography/layout preferences, persistence, theme editor, and keybindings remain. |
| 07 Command and Input | **Partial** | File actions currently use shared Qt `QAction` dispatch and standard shortcuts. The specified command registry/executor, configurable shortcut dispatcher, command palette, general input routing, and context-menu integration remain. |
| 08 Search | **Not started** | Active-document find/replace, options, bounded regex handling, and match navigation remain. |
| 09 Session and Recovery | **Not started** | Session restoration and separate, debounced recovery snapshots remain. |
| 10 Platform Integration | **Partial** | Native Qt Open/Save dialogs, Unicode path conversion, `QFileSystemWatcher` file/directory monitoring with refresh, dropped-file opening, and external-app/system-terminal launch are implemented behind platform-facing helpers. Windows build/smoke only; Linux/macOS behavior and native watcher reliability require validation. |

No system is marked **Complete** in this baseline.
