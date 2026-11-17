# Project instructions for Note

`PROJECT_SPEC.md` is the authoritative product specification. Read it before changing product scope or beginning a system implementation. Appendix A is unapproved and must not be implemented unless the project owner explicitly approves it.

## Product goal and scope

- The target is a complete, polished, launch-ready V1, not merely an MVP or prototype.
- V1 includes all agreed features, reliable file handling and recovery, cross-platform validation, performance testing, error handling, complete user/developer documentation, and release packaging.
- Keep Note a lightweight, file-oriented code editor, not an IDE. Do not add excluded functionality such as an embedded terminal, project explorer, autocomplete, diagnostics, build tools, Git integration, plugins, or browser runtime.
- Do not expand the specification without explicit owner approval. Appendix A remains unapproved.

## Architecture constraints

- Keep UI, editor engine, document management, language, customisation, commands, search, recovery, and platform integration responsibilities separate as described in the specification.
- The editor engine must not depend on the GUI framework except through the agreed drawing interface, and must never perform file I/O.
- Keep one authoritative owner for each state category. UI and document metadata must not duplicate editor state.
- Prefer incremental, viewport-first work. Do not process unchanged full documents or run idle polling/background work without need.
- Preserve atomic file-save behavior and failure reporting when persistence is implemented.
- Do not lock provisional performance targets or unresolved technical choices without measurement and owner confirmation.

## Repository safety and Git

- Maintain a Git repository throughout development. If `.git` is absent, inspect the existing files first, initialize Git, add an appropriate `.gitignore`, and preserve all existing work.
- Inspect repository status and relevant files before editing, deleting, moving, resetting, or replacing anything. Never overwrite, discard, clean, reset, or otherwise destroy existing work without first inspecting it and confirming the intended scope.
- Keep `.gitignore` limited to generated build products, local IDE/user settings, and other reproducible machine-specific output. Never ignore project source, required resources, or documentation to hide changes.
- Commit meaningful milestones after successful relevant validation. Do not commit a milestone as validated when its tests or build have not passed. If a user explicitly requests an initial repository snapshot before validation, commit it as an unverified baseline and state that limitation.
- Review staged changes before each commit; include only the intended milestone and preserve unrelated user changes.

## Status and documentation

- Keep documentation, implementation status, and the development plan current as systems are implemented, partially implemented, completed, or deferred.
- Maintain `docs/STATUS.md` with a clear distinction between **Complete**, **Partial**, and **Not started** for each system and important technical decision. Explain what is included in partial work and what remains.
- Update relevant architecture, system, data model, interface, design, and development documentation when implementation changes their truth. Do not leave plans claiming work is pending after it is complete, or describe planned behavior as implemented.
- Do not mark a system complete until its implementation and relevant tests pass. Keep it partial if required behavior, validation, or platform coverage remains outstanding.
- V1 is not complete until all agreed features and the V1 requirements above have been implemented and validated, and release packaging and documentation are ready.

## Development and quality rules

- Make the smallest change that advances the current development phase, while continuing toward the complete V1 scope.
- Do not create empty modules or directories merely to mirror the logical architecture.
- Avoid unnecessary dependencies, wrappers, and abstractions. Justify each dependency against startup, memory, responsiveness, and distribution cost.
- Keep platform-specific behavior behind common interfaces.
- Handle missing or invalid configuration gracefully; failed saves must not mark documents as saved.
- Add focused tests and benchmarks alongside implemented systems. Do not claim provisional budgets are met without measurements on the agreed reference device.
- Before implementation, inspect existing files and interfaces, identify dependencies, and report affected files and verification results.
- Do not silently change architectural decisions. Record proposals and obtain owner approval when a decision changes the agreed architecture or product scope.

## Current foundation

- C++20, Qt 6 Widgets/Core, and CMake are the selected foundation for continued development. The project-owned editor with an indexed piece-table model remains provisional, based on the isolated prototype and production measurements in `docs/EDITOR_PROTOTYPE.md` and `docs/PERFORMANCE.md`.
- The production application remains a minimal window shell. Phase 02 now provides the production UTF-8 text buffer, document model/manager, Qt Core file service, automated tests, and a separate benchmark target; it does not yet provide an editing UI, tab system, progressive loading, or full document-management UX. The separate viewport prototype is not production functionality, and neither prototype nor current-host measurements establish the provisional low-end budget or cross-platform readiness.
- Consult `docs/STATUS.md` for the authoritative implementation status.
