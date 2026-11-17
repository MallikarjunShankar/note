# Note development documentation

`PROJECT_SPEC.md` at the repository root is authoritative. This folder records the technical foundation and implementation sequence; it does not change the product scope. Appendix A remains unapproved.

## Current state

The repository began with only the project specification. The CMake C++20/Qt 6 Widgets shell now builds on the current Windows environment. The production document foundation is implemented with automated tests and a benchmark harness, but remains partial pending UI integration, progressive file opening, and cross-platform/reference-device validation. The isolated editor prototype and all current performance numbers remain research evidence, not production guarantees or proof of the low-end budget.

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
- [Open technical decisions](DECISIONS.md)
- [Development plan](DEVELOPMENT_PLAN.md)
