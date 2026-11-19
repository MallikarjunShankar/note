# Development plan

This plan preserves the dependency order in section 9 of `PROJECT_SPEC.md`. The isolated architecture prototype is complete as a research milestone; it does not count as implementation of production editor features.

Phase 02 implementation is addressed but remains **Partial pending its dedicated verification and benchmarking milestone**. Do not begin Phase 03 until that verification gate is complete.

1. **Foundation (complete as a prototype):** CMake/Qt application shell and isolated editor/buffer prototype. Provisional hardware profile, budgets and validation method are recorded in [`PERFORMANCE.md`](PERFORMANCE.md). Name the low-end reference device and validate the provisional structure there.
2. **Document foundation (implementation addressed; verification pending):** production text buffer, Document Model/Manager, ID-only Tab Manager, workflow, native dialogs, tab switching and close prompts, encoding/line-ending preservation, atomic File Service, chunked asynchronous open/reload, progress/cancellation, external modification watcher and fingerprint checks, safe conflict/reload flows, drag/drop, and external app/terminal launch are implemented. The central preview remains read-only and bounded; it is not the production editor. Next: focused regression coverage and lifecycle verification, cross-platform testing, reference-device baselines, large-file/long-line thresholds, and performance validation. Use [`PERFORMANCE.md`](PERFORMANCE.md); software-constrained runs are supplementary only.
3. **Core editor:** drawing surface and minimal renderer; insertion/deletion, selection, undo/redo, indentation, pairing. Cursor navigation, multi-cursor state management, the framework-independent Selection Manager, Editing Engine, and Undo/Redo Manager are implemented under `src/editor/`; renderer, indentation, pairing, and broader editing integration remain.
4. **Editor presentation:** line numbers, active-line and indentation cues, whitespace, occurrences, wrapping, relative numbering.
5. **Language system:** data-driven language registry/detection, initial language definitions, incremental viewport-first highlighting.
6. **Customisation:** settings/theme ownership and storage, theme editor/live preview, typography/layout and configurable keybindings.
7. **Commands and search:** unified commands/input/shortcuts/palette and active-document find/replace.
8. **Session and recovery:** tab/session restoration and debounced, dirty-only recovery snapshots.
9. **Platform integration:** implementation is in place for drag/drop, external app/terminal launch, and external modification detection; validate behavior on Windows/Linux/macOS and address platform differences found.
10. **Phase 02 verification and tuning:** focused lifecycle/conflict tests, repeatable stress and performance tests, physical low-end validation, cross-platform builds/behavior, and large-file degradation thresholds. This gate is required before Phase 03.

Do not add Appendix A items without explicit approval. Performance is measured during development, not deferred to the final phase.
