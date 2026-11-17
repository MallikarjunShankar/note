# Development plan

This plan preserves the dependency order in section 9 of `PROJECT_SPEC.md`. The isolated architecture prototype is complete as a research milestone; it does not count as implementation of production editor features.

Phase 02 remains active. Do not treat the production document data layer as complete or begin Phase 03 until the remaining Phase 02 integration and validation gates below are handled.

1. **Foundation (complete as a prototype):** CMake/Qt application shell and isolated editor/buffer prototype. Provisional hardware profile, budgets and validation method are recorded in [`PERFORMANCE.md`](PERFORMANCE.md). Name the low-end reference device and validate the provisional structure there.
2. **Document foundation (partial):** production text buffer, Document Model/Manager, ID-only Tab Manager, application workflow controller, native Open/Save dialogs, basic tab switching and close prompts, encoding/line-ending preservation, atomic File Service, automated tests, and a 1/10/50/100 MiB comparison harness are implemented. The central preview is read-only and bounded; it is not the production editor. Remaining in this phase: progressive/background large-file open, reference-device baselines, large-file/long-line thresholds, and cross-platform validation. Use [`PERFORMANCE.md`](PERFORMANCE.md); software-constrained runs are supplementary only.
3. **Core editor:** drawing surface and minimal renderer; insertion/deletion, navigation, selection, undo/redo, indentation, pairing, multiple cursors.
4. **Editor presentation:** line numbers, active-line and indentation cues, whitespace, occurrences, wrapping, relative numbering.
5. **Language system:** data-driven language registry/detection, initial language definitions, incremental viewport-first highlighting.
6. **Customisation:** settings/theme ownership and storage, theme editor/live preview, typography/layout and configurable keybindings.
7. **Commands and search:** unified commands/input/shortcuts/palette and active-document find/replace.
8. **Session and recovery:** tab/session restoration and debounced, dirty-only recovery snapshots.
9. **Platform integration:** drag/drop, external app/terminal launch, external modification detection and OS validation.
10. **Validation and tuning:** verify behavior and performance targets on supported systems; run repeatable stress and resource-constrained tests, validate on physical low-end hardware, and tune large-file degradation thresholds.

Do not add Appendix A items without explicit approval. Performance is measured during development, not deferred to the final phase.
