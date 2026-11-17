# Development plan

This plan preserves the dependency order in section 9 of `PROJECT_SPEC.md`. The current work establishes build configuration and a bare shell only; it does not count as editor feature implementation.

1. **Foundation (current):** CMake/Qt application shell; finish stack and editor-component benchmark decision. No editor behavior yet.
2. **Document foundation:** benchmark harness and baseline; choose buffer structure; document model, text buffer, document/tab management, atomic file service, native open/save dialogs.
3. **Core editor:** drawing surface and minimal renderer; insertion/deletion, navigation, selection, undo/redo, indentation, pairing, multiple cursors.
4. **Editor presentation:** line numbers, active-line and indentation cues, whitespace, occurrences, wrapping, relative numbering.
5. **Language system:** data-driven language registry/detection, initial language definitions, incremental viewport-first highlighting.
6. **Customisation:** settings/theme ownership and storage, theme editor/live preview, typography/layout and configurable keybindings.
7. **Commands and search:** unified commands/input/shortcuts/palette and active-document find/replace.
8. **Session and recovery:** tab/session restoration and debounced, dirty-only recovery snapshots.
9. **Platform integration:** drag/drop, external app/terminal launch, external modification detection and OS validation.
10. **Validation and tuning:** verify behavior and performance targets on supported systems; tune large-file degradation thresholds.

Do not add Appendix A items without explicit approval. Performance is measured during development, not deferred to the final phase.
