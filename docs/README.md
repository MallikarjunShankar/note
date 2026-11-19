# Note development documentation

`PROJECT_SPEC.md` at the repository root is authoritative. This folder records the technical foundation and implementation sequence; it does not change the product scope. Appendix A remains unapproved.

## Current state

The CMake C++20/Qt 6 Widgets application implements the Phase 02 document workflows, including background open/reload, progress and cancellation, external-change notifications, fingerprint-based save conflict protection, drag/drop, and platform launch helpers. Phase 02 remains partial pending focused verification, cross-platform validation, and performance benchmarking. The Main Window now uses per-document project-owned editing views; recovery and remaining V1 editor features are outstanding. Current performance numbers are host-specific evidence, not production guarantees or proof of the low-end budget.

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
