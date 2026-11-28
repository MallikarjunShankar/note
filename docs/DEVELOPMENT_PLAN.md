# Development plan

This document records NOTE's current post-release-candidate direction. PROJECT_SPEC.md remains authoritative for product scope.

## Product direction

NOTE is a lightweight, minimalistic, reliable, and highly customisable text editor.

Its priorities are:

1. Lightweight performance and minimalism.
2. Visual and behavioural customisation.
3. Configurable commands and keyboard interaction.
4. Reliable document management and recovery.

NOTE is not an IDE. Windows is the initial distribution target. Shared systems should preserve platform-independent boundaries.

## Current position

The integrated Windows V1 implementation has reached release-candidate packaging.

The 1.0.0-rc.1 Windows x64 package was built through the release workflow, passed its automated release checks, and was manually extracted and tested.

The implementation includes:

- A project-owned UTF-8 editor engine and viewport renderer.
- Document creation, opening, editing, saving, and tab workflows.
- Undo/redo, multiple cursors, indentation, and bracket/quote pairing.
- Settings, custom themes, theme editing, and keybinding customisation.
- A command registry, command palette, and shared shortcut dispatch.
- Active-document search and replacement.
- Session restoration and unsaved-document crash recovery.
- Windows file integration and portable release packaging.

This release-candidate milestone does not mean every system or product acceptance target is complete.

## Remaining validation

### Performance and hardware

- Select and approve a low-end reference device.
- Confirm the provisional performance budget.
- Validate startup, idle memory, typing latency, large-file opening, and scrolling.
- Measure real-display frame pacing and input-to-photon behavior.
- Address demonstrated performance failures without compromising correctness.

See [PERFORMANCE.md](PERFORMANCE.md).

### Platform integration

- Validate native dialogs and file operations on supported platforms.
- Validate drag/drop and external application/terminal launching.
- Test Linux and macOS builds and runtime behavior before claiming support for those platforms.

### Accessibility and usability

- Evaluate keyboard-only operation on a real display.
- Assess screen-reader accessibility.
- Validate HiDPI and multi-monitor behavior.

### Maintenance

- Keep automated tests and release workflows passing.
- Update documentation alongside behavioral or architectural changes.
- Keep changes focused and avoid unnecessary dependencies or refactoring.

## Explicitly deferred: Language System

Phase 05 and System 05 remain deferred until after V1.

The deferred scope includes:

- Language detection.
- Language registry.
- Syntax definitions.
- Syntax highlighting.
- Incremental highlighting.

No syntax-highlighting runtime is required for the current release candidate.

## Scope control

Items in PROJECT_SPEC.md Appendix A remain unapproved unless explicitly accepted by the project owner. Do not silently promote proposed features into V1 scope.

Performance budgets remain provisional until the reference device and thresholds are approved.

## Release infrastructure

The Windows release workflow is manually triggered through GitHub Actions. It builds and tests a clean Release configuration, deploys runtime dependencies, generates the portable ZIP and Windows installer with SHA-256 checksums, and performs an isolated launch check.

See [RELEASE.md](RELEASE.md) for the release procedure.
