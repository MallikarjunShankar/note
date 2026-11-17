# Implementation status

This is the current status ledger for Note. “Complete” means implemented and relevant tests/validation have passed; planned behavior and documentation alone do not qualify. See `PROJECT_SPEC.md` for the full requirements and `docs/DEVELOPMENT_PLAN.md` for dependency order.

## Project foundation

| Area | Status | Evidence / remaining work |
|---|---|---|
| Repository and build configuration | **Partial** | CMake project and C++20 Qt Widgets target exist. Git baseline is being established. Build has not been verified because Qt 6 is not installed in the current environment. |
| Application shell | **Partial** | Minimal QApplication/QMainWindow shell exists. Lifecycle, application state, configuration loading, settings/theme managers, command registry, and usable editor window remain. |
| Technology evaluation | **Partial** | Comparative review is recorded in `docs/EDITOR_EVALUATION.md`. Runtime prototypes and measurements remain outstanding; editor implementation and buffer are undecided. |
| V1 release readiness | **Not started** | No agreed editor features, file handling, recovery, platform validation, performance verification, or release packaging are complete. |

## System status

| System | Status | Remaining work |
|---|---|---|
| 01 Application Core | **Partial** | Only minimal process/window startup exists; lifecycle, state, bootstrap configuration, and resource handling remain. |
| 02 User Interface | **Partial** | Only a blank main window exists; specified menus, tabs, editor view, status bar, settings UI, context menus, and drawing surface remain. |
| 03 Editor Engine | **Not started** | Buffer, cursor/selection ownership, editing, undo/redo, renderer, indentation, pairing, and multi-cursor behavior remain. |
| 04 Document Management | **Not started** | Document/tab models, file service, atomic persistence, encoding/line endings, external modification handling, and drag/drop remain. |
| 05 Language System | **Not started** | Registry, data-driven detection/definitions, incremental highlighting, and cache remain. |
| 06 Customisation | **Not started** | Settings, themes, typography/layout preferences, persistence, theme editor, and keybindings remain. |
| 07 Command and Input | **Not started** | Input routing, registry/executor, shortcuts, palette, and context-menu integration remain. |
| 08 Search | **Not started** | Active-document find/replace, options, bounded regex handling, and match navigation remain. |
| 09 Session and Recovery | **Not started** | Session restoration and separate, debounced recovery snapshots remain. |
| 10 Platform Integration | **Not started** | Native dialogs, filesystem/path adapters, external application/terminal launch, and OS validation remain. |

No system is marked **Complete** in this baseline.
