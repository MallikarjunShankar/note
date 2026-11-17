# Architecture and initial stack evaluation

## Product constraints

Note prioritises low startup time, low idle memory and responsive editing over customisation. It must run on Windows, Linux and macOS, render only the viewport, update incrementally, support large files with graceful degradation, and avoid embedded browser runtimes. The numerical performance budget in section 7.0 of `PROJECT_SPEC.md` is provisional until a low-end reference device is named.

## Candidate evaluation

| Candidate | Strengths | Trade-offs for Note |
|---|---|---|
| C++20 + Qt 6 Widgets | Mature cross-platform desktop UI, native platform integration, strong text/font/drawing primitives, broad deployment tooling. C++ has low runtime overhead and predictable startup/memory characteristics. | Qt adds a meaningful binary/runtime footprint. A custom viewport editor and buffer remain substantial work; Qt's general text widgets do not by themselves meet all specified multi-cursor and large-file requirements. |
| Rust + egui/wgpu or another Rust GUI | Strong memory safety and lean native binaries; Rust is suitable for an incremental buffer and renderer. | GUI ecosystem/platform polish and native dialogs/accessibility require validation; a custom editor and cross-platform integration still need significant work. GPU rendering can add complexity and power/memory cost without being necessary for a text editor. |
| C++ + Scintilla-based editor component | Mature, responsive code-editing component with substantial editing and lexer capability; can shorten time to a usable editor. | Couples editor behavior/rendering to a third-party component and its API. Must verify that theme control, data-driven language definitions, line-based incremental highlighting, architecture boundaries, and packaging fit the specification. |

## Recommendation

Use **C++20 + Qt 6 Widgets + CMake** for the application foundation. Build a native desktop application without a web runtime. Keep Qt-dependent windowing and drawing in the UI layer; keep document, language, and editor policies behind project-owned interfaces. Avoid a Qt Quick/QML or GPU requirement for the initial path.

For the editor implementation, run a short, disposable comparison of **Qt's text primitives with a project-owned viewport renderer** versus **a Scintilla-based component** before committing to the long-term editor architecture. The custom path gives stronger architectural control and predictable data-driven highlighting, but requires more editor engineering. Scintilla may reach usability sooner and perform well, but could constrain ownership and renderer boundaries. Benchmark startup, idle memory, typing latency, viewport rendering, and a 50 MB file on the eventual reference device. The choice is not yet final.

This is a recommendation, not evidence that the provisional performance budget has been met. Qt's added footprint is a real cost; the early baseline must confirm whether it is acceptable. If not, revisit the GUI candidate before building dependent systems.

## Initial structure

- `src/main.cpp`: minimal Qt application shell only.
- `CMakeLists.txt`: C++20 executable and Qt Widgets dependency.
- `docs/`: decisions, boundaries, and implementation sequence.

Add directories only when the first real component in that system is implemented. This follows the specification's warning against empty scaffolding.
