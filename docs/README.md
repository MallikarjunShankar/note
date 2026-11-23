# Note development documentation

`PROJECT_SPEC.md` at the repository root is authoritative. This folder records the technical foundation and implementation sequence; it does not change the product scope. Appendix A remains unapproved.

## Current state

The CMake C++20/Qt 6 Widgets application has partial implementations across document workflows, per-document editing views, shared commands/keybindings, customisation/search, and session recovery. Configurable indentation, wrapped-row scrollbar indexing, persistent presentation settings, transient theme preview, and platform-facing file workflows have focused Windows/offscreen coverage. V1 remains incomplete pending broader validation, owner decisions and release preparation. Current performance numbers are host-specific evidence, not production guarantees or proof of the low-end budget.

## Documents

- [Implementation status](STATUS.md)
- [Architecture and stack evaluation](ARCHITECTURE.md)
- [Hardware targets and performance validation](PERFORMANCE.md)
- [Editor implementation comparison](EDITOR_EVALUATION.md)
- [Project-owned editor prototype and measurements](EDITOR_PROTOTYPE.md)
- [Document data models](DATA_MODELS.md)
- [Document subsystem interfaces](INTERFACES.md)
- [Production document benchmarks](../benchmarks/document_foundation/README.md)
- [System boundaries](SYSTEMS.md)
- [Default shortcuts](DEFAULT_SHORTCUTS.md)
- [Open technical decisions](DECISIONS.md)
- [Development plan](DEVELOPMENT_PLAN.md)
