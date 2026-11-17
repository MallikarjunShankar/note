# Implementation status

This is the current status ledger for Note. “Complete” means implemented and relevant tests/validation have passed; planned behavior and documentation alone do not qualify. See `PROJECT_SPEC.md` for the full requirements and `docs/DEVELOPMENT_PLAN.md` for dependency order.

## Project foundation

| Area | Status | Evidence / remaining work |
|---|---|---|
| Repository and build configuration | **Partial** | Git baseline is established. CMake/C++20/Qt 6.9.1 production shell builds on the current Windows host; Linux/macOS deployment and V1 release packaging remain. |
| Application shell | **Partial** | Minimal QApplication/QMainWindow shell exists. Lifecycle, application state, configuration loading, settings/theme managers, command registry, and usable editor window remain. |
| Technology evaluation | **Partial** | Isolated project-owned widget and gap/piece buffer benchmarks are recorded in `docs/EDITOR_PROTOTYPE.md`. QScintilla was not measured; the low-end reference device and cross-platform validation remain outstanding. |
| Editor architecture decision | **Partial** | Project-owned viewport renderer is provisionally selected. The production buffer now uses an indexed implicit-treap piece table with newline indexes, but worst-case fragmentation, long-line and reference-device behavior remain. |
| Hardware targets and performance budgets | **Partial** | Provisional minimum profile and repeatable measurement/stress methodology are documented in `docs/PERFORMANCE.md`. Values remain unconfirmed; no physical low-end validation or production performance claim has been made. |
| Phase 02 document foundation | **Partial** | Production buffer, document ownership, encoding/line-ending handling, atomic File Service, deterministic tests, and benchmark comparisons are implemented. UI/tab/dialog integration, progressive loading, reference hardware, and other-platform validation remain. |
| V1 release readiness | **Not started** | No agreed editor features, file handling, recovery, platform validation, performance verification, or release packaging are complete. |

## System status

| System | Status | Remaining work |
|---|---|---|
| 01 Application Core | **Partial** | Only minimal process/window startup exists; lifecycle, state, bootstrap configuration, and resource handling remain. |
| 02 User Interface | **Partial** | Production still has only a blank main window. The isolated prototype demonstrates a custom-painted viewport; specified menus, tabs, status bar, settings UI, context menus, production drawing surface, and integration remain. |
| 03 Editor Engine | **Partial** | Production UTF-8 piece-table buffer with indexed piece/newline metadata and focused tests exists in `src/editor/`. Cursor/selection ownership, undo/redo, renderer interfaces, indentation, pairing, multi-cursor, syntax, and reference-device validation remain. |
| 04 Document Management | **Partial** | Production Document Model/Manager and Qt Core File Service support UTF-8/BOM, BOM-marked UTF-16, mixed LF/CRLF/CR preservation, Save As, and atomic saves; tests cover failures and round trips. Tab ordering/UI, native dialogs, progressive/background opening, external-modification handling, and drag/drop remain. |
| 05 Language System | **Not started** | Registry, data-driven detection/definitions, incremental highlighting, and cache remain. |
| 06 Customisation | **Not started** | Settings, themes, typography/layout preferences, persistence, theme editor, and keybindings remain. |
| 07 Command and Input | **Not started** | Input routing, registry/executor, shortcuts, palette, and context-menu integration remain. |
| 08 Search | **Not started** | Active-document find/replace, options, bounded regex handling, and match navigation remain. |
| 09 Session and Recovery | **Not started** | Session restoration and separate, debounced recovery snapshots remain. |
| 10 Platform Integration | **Not started** | Native dialogs, filesystem/path adapters, external application/terminal launch, and OS validation remain. |

No system is marked **Complete** in this baseline.
