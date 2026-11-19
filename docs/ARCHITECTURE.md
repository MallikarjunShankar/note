# Architecture and initial stack evaluation

## Product constraints

Note prioritises low startup time, low idle memory and responsive editing over customisation. It must run on Windows, Linux and macOS, render only the viewport, update incrementally, support large files with graceful degradation, and avoid embedded browser runtimes. The numerical performance budget in section 7.0 of `PROJECT_SPEC.md` is provisional until a low-end reference device is named. [`PERFORMANCE.md`](PERFORMANCE.md) records the provisional engineering hardware profile, measurement budgets, and repeatable validation method; it does not replace the product spec.

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

- `src/main.cpp`: starts the Qt application and owns the `DocumentWorkflow` service lifetime.
- `src/application/document_workflow.*`: coordinates document creation/open/save/close, the Document Manager, and tab order without owning document content.
- `src/ui/main_window.*`: File actions, native dialogs, tab bar, close prompts, status presentation, and per-document editor-view lifecycle.
- `src/ui/editor_view.*`: per-document Qt input adapter composing the framework-independent cursor, selection, editing, and undo/redo managers with the renderer; owns editor state for the lifetime of an open document.
- `src/ui/document_preview.*`: bounded, read-only legacy projection retained in the build; the main window now uses the project-owned editor view instead.
- `src/ui/editor_renderer.*`: project-owned Qt Widgets viewport that paints visible buffer lines, selections, and cursors from non-owning editor-manager references and supports hit testing and scrolling.
- `src/editor/text_buffer.*`: framework-independent UTF-8 piece-table buffer with an indexed implicit-treap piece sequence and newline metadata.
- `src/documents/`: document metadata/manager, ID-only `TabManager`, and Qt Core file service using atomic `QSaveFile` writes.
- `CMakeLists.txt`: C++20 executable and Qt Widgets dependency.
- `benchmarks/editor_prototype/`: isolated editor and buffer experiments, not production code.
- `benchmarks/document_foundation/`: production buffer/file-service comparison harness.
- `tests/documents/`: deterministic document subsystem tests.
- `docs/`: decisions, measurements, boundaries, and implementation sequence.

Phase 02 implementation details and measurements are in [`DATA_MODELS.md`](DATA_MODELS.md), [`INTERFACES.md`](INTERFACES.md), and [`PERFORMANCE.md`](PERFORMANCE.md). File reads and decoding run in bounded background tasks. The worker keeps incomplete buffers private and the workflow publishes only after validation. A Qt `QFileSystemWatcher` adapter monitors open files and parent directories, while content fingerprints verify changes; activation and pre-save checks provide a fallback when native watch events are lost. Reloads are asynchronous and replace contents in the manager-owned buffer object only after validation and a revision check. Atomic saves verify the expected disk fingerprint before commit; Save As requires explicit replacement confirmation and rechecks the target. Platform path conversion and external application/terminal launching are isolated in `src/platform/`. The production Main Window now displays per-document `EditorView`s; the bounded read-only `DocumentPreview` remains only as a retained legacy component. Formal lifecycle/conflict regression tests, OS-specific validation, real-display performance measurement, recovery, and remaining editor features are still outstanding.

Add directories only when the first real component in that system is implemented. This follows the specification's warning against empty scaffolding.
