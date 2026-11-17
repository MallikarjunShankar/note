# Open technical decisions

The specification identifies ten decisions as open. Current recommendation and required gate:

| Decision | Initial direction | Before final commitment |
|---|---|---|
| Programming language | C++20 | Validate on supported compilers/platforms and measure the app baseline. |
| GUI framework | Qt 6 Widgets | Confirm licensing/distribution needs and startup/idle-memory budget on reference hardware. |
| Text editing | Undecided: custom viewport editor vs Scintilla-based component | Prototype representative typing, selection, multi-cursor, large-file and theme behavior; check ownership/boundaries. |
| Text rendering | Qt text/font primitives and viewport-only drawing are the custom-path candidate | Compare against editor component and verify sub-16 ms provisional update target. |
| Text buffer | Undecided | Measure a piece table and/or rope against expected editing, retrieval, undo, multi-cursor and large-file workloads before selecting. Avoid duplicating the full document. |
| Syntax highlighting | Data-defined language rules with incremental line-based processing is the product constraint | Select or implement a tokenizer only after checking language coverage, worst-case behavior, and data-driven extensions. Do not adopt an analysis/LSP stack. |
| Configuration format | Undecided | Choose a small, validated format suitable for app-owned settings; no requirement for human-editable files. |
| Theme format | Data-only format, distinct from settings; exact encoding undecided | Choose stable versioning and safe defaults for missing/invalid values. |
| Recovery format | Separate from original files; exact format undecided | Decide atomicity, retention/cleanup, and write strategy when recovery is implemented. |
| Build/packaging | CMake | Validate Qt deployment and native packaging for Windows, Linux, and macOS. |

## Benchmark gate

Before committing to the long-term editor component or claiming the budget, name the low-end reference device and record cold start, idle memory, keystroke-to-screen latency, viewport scrolling/rendering, and progressive opening of a 50 MB file. Compare serious editor candidates with the same small workload. Section 7.0 targets remain provisional until the project owner confirms them.
