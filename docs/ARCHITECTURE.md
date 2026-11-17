# Architecture and initial stack evaluation

## Product constraints

Note prioritises low startup time, low idle memory and responsive editing over customisation. It must run on Windows, Linux and macOS, render only the viewport, update incrementally, support large files with graceful degradation, and avoid embedded browser runtimes. The numerical performance budget in section 7.0 of `PROJECT_SPEC.md` is provisional until a low-end reference device is named.

## Candidate evaluation

| Candidate | Strengths | Trade-offs for Note |
|---|---|---|
| C++20 + Qt 6 Widgets | Mature cross-platform desktop UI, native platform integration, strong text/font/drawing primitives, broad deployment tooling. C++ has low runtime overhead and predictable startup/memory characteristics. | Qt adds a meaningful binary/runtime footprint. A custom viewport editor and buffer remain substantial work; Qt's general text widgets do not by themselves meet all specified multi-cursor and large-file requirements. |
| Rust + egui/wgpu or another Rust GUI | Strong memory safety and lean native binaries; Rust is suitable for an incremental buffer and renderer. | GUI ecosystem/platform polish and native dialogs/accessibility require validation; a custom editor and cross-platform integration still need significant work. GPU rendering can add complexity and power/memory cost without being necessary for a text editor. |
| C++ + Scintilla-based editor component | Mature, responsive code-editing component with substantial editing and lexer capability; can shorten time to a usable editor. | Couples editor behavior/rendering to a third-party component and its API. Must verify that theme control, data-driven language definitions, line-based incremental highlighting, architecture boundaries, and packaging fit the specification. |

## Recommendation and prototype result

Use **C++20 + Qt 6 Widgets + CMake** for the application foundation. Build a native desktop application without a web runtime. Keep Qt-dependent windowing and drawing in the UI layer; keep document, language, and editor policies behind project-owned interfaces. Avoid a Qt Quick/QML or GPU requirement for the initial path.

The isolated project-owned prototype in `docs/EDITOR_PROTOTYPE.md` now builds with Qt 6.9.1 and demonstrates a custom-painted viewport, basic editing, caret movement, selection, scrolling, and progressive 50 MiB loading on the current Windows host. A gap buffer and flat piece table were benchmarked under the same buffer workloads. The piece-table model performed substantially better for persistent random edits, while the gap buffer retained an advantage for local repeated edits.

The recommended V1 direction is a project-owned Editor Engine and viewport renderer over a piece-table storage model, using a balanced piece index and indexed line metadata rather than the prototype's flat piece vector and line-start vector. This retains the specified state ownership and framework-agnostic Drawing Surface boundary. Qt 6 is now build-verified on this Windows environment.

The recommendation is technically informed but still provisional: measurements used the Qt offscreen plugin on a host whose CPU model and physical RAM were unavailable. They do not establish the provisional low-end performance budget, real-display frame pacing, or Linux/macOS compatibility. Confirm the indexed piece structure and budget on the named reference device before declaring the architecture complete.

## Initial structure

- `src/main.cpp`: minimal Qt application shell only.
- `CMakeLists.txt`: C++20 executable and Qt Widgets dependency.
- `benchmarks/editor_prototype/`: isolated editor and buffer experiments, not production code.
- `docs/`: decisions, measurements, boundaries, and implementation sequence.

Add directories only when the first real component in that system is implemented. This follows the specification's warning against empty scaffolding.
