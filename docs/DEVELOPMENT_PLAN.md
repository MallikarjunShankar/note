# Development plan

This plan reflects the approved revised NOTE V1 direction. `PROJECT_SPEC.md` remains authoritative.

NOTE V1 is a lightweight, minimalistic, reliable and highly customisable text editor. Its priorities are:

1. Lightweight performance.
2. Extensive visual and behavioural customisation.
3. Configurable commands and keyboard interaction.
4. Reliable document management and recovery.

NOTE is not intended to become an IDE. Windows is the initial V1 distribution target, while shared systems remain platform-independent and preserve Linux/macOS compatibility.

Performance is fundamental throughout development. Sensible, contained optimisations may be made when justified by actual implementation work. Do not create dedicated optimisation phases before the integrated V1 exists. Comprehensive profiling and optimisation are reserved for integrated V1 validation.

## Current position

The isolated architecture prototype is complete as a research milestone; it is not production editor functionality.

The application currently has partial implementation for the application shell, document lifecycle, text editing and platform-facing file behavior:

- CMake/C++20/Qt 6 production shell work is in place on the development Windows host.
- Production UTF-8 piece-table editing, document ownership, tab identity, workflow, native dialogs and chunked Qt file service behavior exist.
- Project-owned viewport rendering, cursor/selection management, undo/redo, pairing, indentation and presentation settings are in progress.
- Focused Windows Debug tests pass, but reference-device, cross-platform and real-display validation remain incomplete.
- System 09 now has atomic session persistence, path/order/active/cursor restoration, unclean-run detection, separate dirty-buffer snapshots and explicit recovery choices integrated with Application Core. UI lifecycle, platform and durability validation remain.
- Systems 06–08 have integrated implementation in progress; broader UI and runtime validation remain.

Measured current-host performance and technical evidence remain in [`PERFORMANCE.md`](PERFORMANCE.md). Historical partial or complete evidence does not waive unresolved acceptance gates.

## Revised remaining work packages

Implementation proceeds in the following integrated packages. Dependencies must be completed within earlier packages before later integration claims are made.

### 1. Commands, customisation and search

**Systems:** 06, 07 and 08.

- Complete settings and theme ownership and persistence.
- Implement theme editing, live preview, import/export and invalid-value fallback.
- Complete typography, layout and editing-preference behavior.
- Complete the command registry/executor and unify existing Qt actions behind it.
- Implement input routing, configurable shortcut dispatch, shortcut conflict handling and command palette behavior.
- Implement active-document find/replace, search options and bounded regular-expression handling.

**Dependencies:** stable application lifecycle, document/editor state and usable theme/settings tokens.

### 2. Session and recovery

**System:** 09, integrated with application lifecycle.

- Implement session restoration and tab/session state publication.
- Implement separate, debounced dirty-only recovery snapshots.
- Integrate recovery and session behavior with startup, shutdown, close prompts and document opening.
- Test crash continuity, stale recovery data, file identity and unsaved-state edge cases.

**Dependencies:** complete document lifecycle and application-owned bootstrap/lifecycle state.

### 3. Core completion and platform integration

**Systems:** outstanding work in 01–04 and 10.

- Complete application bootstrap, shared state, resource handling and lifecycle coordination.
- Complete UI behavior, persistent presentation settings, wrapped scrollbar behavior and real-view validation.
- Complete core editor behavior, indentation/configuration and reference-device validation.
- Complete document-management edge cases, progressive usability and platform validation.
- Complete drag/drop, external-app behavior and filesystem integration behind common interfaces.
- Build and smoke-test shared behavior without Windows-only assumptions.

**Dependencies:** all prior work uses existing ownership boundaries and does not move document/editor/file state into the UI or engine.

### 4. V1 integration, validation and final performance optimisation

- Integrate customisation, commands/search, recovery and core document/editor behavior.
- Run document-integrity, lifecycle, recovery, customisation-persistence and end-to-end workflow tests.
- Complete user/developer documentation and cross-platform validation.
- Reserve comprehensive profiling for this stage.
- Measure and optimise startup, idle resource use, typing latency, rendering, search, opening, saving and large/long-line behavior against the owner-confirmed reference-device budget.
- Resolve regressions found by measured validation; no unmeasured dedicated optimisation phase replaces this integration stage.

**Dependencies:** integrated packages 1–3 and the complete V1 feature set.

### 5. Windows packaging and release preparation

- Prepare Windows packaging and release artifacts.
- Validate installation, first run, resources, updates where applicable and launch behavior.
- Run packaged-release smoke tests and release verification.
- Finalise release and distribution documentation.

**Dependencies:** package 4 acceptance and confirmed Windows build/toolchain behavior.

## Explicitly deferred: Language System

**Phase 05 and System 05 are deferred until after V1.**

The remaining work does not include:

- Language detection.
- Language registry.
- Syntax highlighting.
- Syntax definitions.
- Incremental highlighting.

The customisation architecture may retain data structures for future syntax-colour categories, but V1 requires no syntax-highlighting runtime.

## Owner decisions still outstanding

- Name and approve the low-end reference device and confirm the provisional performance budget.
- Approve a configuration-file format and theme-file format.
- Approve the recovery storage format.
- Approve the build and packaging technology details.
- Approve or reject each proposal in `PROJECT_SPEC.md` Appendix A before it enters V1 scope.

No Appendix A item is approved by this plan.
