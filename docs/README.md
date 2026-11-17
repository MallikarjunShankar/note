# Note development documentation

`PROJECT_SPEC.md` at the repository root is authoritative. This folder records the technical foundation and implementation sequence; it does not change the product scope. Appendix A remains unapproved.

## Current state

The CMake C++20/Qt 6 Widgets application now provides basic document and tab lifecycle workflows over the production document foundation, including chunked asynchronous file opening with progress and cancellation. It remains partial: the central view is a bounded, read-only preview, and external-change handling, recovery, cross-platform/reference-device validation remain outstanding. Current performance numbers are host-specific evidence, not production guarantees or proof of the low-end budget.

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
