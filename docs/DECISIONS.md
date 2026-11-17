# Open technical decisions

The specification identifies ten decisions as open. Current recommendation and required gate:

| Decision | Initial direction | Before final commitment |
|---|---|---|
| Programming language | C++20 (selected for the foundation) | Windows build validated; Linux/macOS compiler and packaging validation remain. |
| GUI framework | Qt 6 Widgets (selected for the foundation) | Qt 6.9.1 shell/prototype builds on Windows. Cross-platform deployment and reference-device performance remain. |
| Text editing | Project-owned Editor Engine and viewport renderer (recommended) | Isolated prototype passes basic interaction checks. Production behavior and low-end performance remain unvalidated; QScintilla was not benchmarked. |
| Text rendering | Custom viewport widget backed by Qt text/drawing primitives | Plain text, selection, caret and scrolling prototype validated offscreen. Wrapping, syntax spans, and real-display timing remain. |
| Text buffer | Indexed piece table with an implicit treap and source-backed newline indexes selected provisionally | Five-run gap/flat-piece/treap comparisons at 1/10/50/100 MiB are recorded in `docs/PERFORMANCE.md`. The tree had low random-edit and line-query latency under 1,000 random edits but higher construction and index memory than the flat table. Worst-case treap depth, allocator RSS, reference-device behavior, and alternate tree implementations remain unvalidated. |
| Syntax highlighting | Data-defined language rules with incremental line-based processing is the product constraint | Select or implement a tokenizer only after checking language coverage, worst-case behavior, and data-driven extensions. Do not adopt an analysis/LSP stack. |
| Configuration format | Undecided | Choose a small, validated format suitable for app-owned settings; no requirement for human-editable files. |
| Theme format | Data-only format, distinct from settings; exact encoding undecided | Choose stable versioning and safe defaults for missing/invalid values. |
| Recovery format | Separate from original files; exact format undecided | Decide atomicity, retention/cleanup, and write strategy when recovery is implemented. |
| Build/packaging | CMake | Validate Qt deployment and native packaging for Windows, Linux, and macOS. |

## Benchmark gate

The editor architecture prototype is complete on this Windows host; see `docs/EDITOR_PROTOTYPE.md` for the actual setup, tests, and measurements. A 50 MiB asynchronous-open Core event-loop benchmark is now recorded in `docs/PERFORMANCE.md`; before claiming the performance budget, name the low-end reference device and validate the open flow with the actual GUI, true cold start, idle memory, keystroke-to-screen latency, and real-display scrolling/rendering. Validate Linux/macOS builds and exercise the production treap under high fragmentation; compare a worst-case-balanced tree if depth or tail latency is unstable. Section 7.0 targets remain provisional until the project owner confirms them.

The production document foundation now has a five-run Release benchmark comparison and deterministic unit tests. These results help choose an indexed piece-table model but do not validate the low-end budget. `TextBuffer` uses UTF-8 byte offsets with scalar-boundary validation and canonical LF plus per-newline encoding metadata; see `docs/DATA_MODELS.md` and `docs/INTERFACES.md`. Phase 02 also uses Qt's filesystem watcher, exact-content SHA-256 baselines, atomic `QSaveFile`, asynchronous reload, and platform launch/path helpers. This is implementation-ready on the current Windows build only; formal lifecycle/conflict testing and Windows/Linux/macOS behavior verification remain required. Full-file fingerprint reads (including synchronous pre-save validation) need performance measurement before the 50 MB target can be assessed.

## Hardware and performance budget status

[`PERFORMANCE.md`](PERFORMANCE.md) defines a provisional 2-core/4-thread, 4 GB RAM minimum engineering profile and supplementary development-machine working targets. These do not change the stricter provisional product targets in `PROJECT_SPEC.md` §7.0 (300 ms startup and 50 MB idle memory among them). Neither profile is confirmed by the owner or validated on physical low-end hardware. Keep measurements advisory until the reference device and product budgets are confirmed.
