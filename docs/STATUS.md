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
| Phase 02 document foundation | **Partial** | Production buffer, document ownership, ID-only tab order, workflow controller, native dialogs, save/close prompts, encoding/line-ending handling, atomic File Service, deterministic tests, and benchmark comparisons are implemented. Progressive loading, reference hardware, large-file thresholds, and other-platform validation remain. |
| V1 release readiness | **Not started** | No agreed editor features, file handling, recovery, platform validation, performance verification, or release packaging are complete. |

## System status

| System | Status | Remaining work |
|---|---|---|
| 01 Application Core | **Partial** | The entry point owns the document workflow lifetime and launches the Main Window. Full lifecycle coordination, application-wide state, bootstrap configuration, and resource handling remain. |
| 02 User Interface | **Partial** | Main Window provides a minimal File menu, tabs, status display, native file dialogs, close prompts, and a bounded read-only preview. Settings UI, context menus, production drawing surface/editor view, and full theme integration remain. |
| 03 Editor Engine | **Partial** | Production UTF-8 piece-table buffer with indexed piece/newline metadata and focused tests exists in `src/editor/`. Cursor/selection ownership, undo/redo, renderer interfaces, indentation, pairing, multi-cursor, syntax, and reference-device validation remain. |
| 04 Document Management | **Partial** | Production Document Model/Manager, ID-only tab ordering, application workflow, and Qt Core File Service support UTF-8/BOM, BOM-marked UTF-16, mixed LF/CRLF/CR preservation, duplicate-path activation, Save As, and atomic saves; tests cover lifecycle, failures, and round trips. Progressive/background opening, external-modification handling, drag/drop, and cross-platform validation remain. |
| 05 Language System | **Not started** | Registry, data-driven detection/definitions, incremental highlighting, and cache remain. |
| 06 Customisation | **Not started** | Settings, themes, typography/layout preferences, persistence, theme editor, and keybindings remain. |
| 07 Command and Input | **Partial** | File actions currently use shared Qt `QAction` dispatch and standard shortcuts. The specified command registry/executor, configurable shortcut dispatcher, command palette, general input routing, and context-menu integration remain. |
| 08 Search | **Not started** | Active-document find/replace, options, bounded regex handling, and match navigation remain. |
| 09 Session and Recovery | **Not started** | Session restoration and separate, debounced recovery snapshots remain. |
| 10 Platform Integration | **Partial** | Native Qt Open/Save dialogs and platform-aware path conversion are used by the current UI/File Service. A dedicated platform adapter, drag/drop, external application/terminal launch, and supported-OS validation remain. |

No system is marked **Complete** in this baseline.
