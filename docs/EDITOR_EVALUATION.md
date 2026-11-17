# Editor implementation evaluation

**Status:** comparative review completed; runtime benchmarks not run.
**Scope:** a project-owned editor with a custom viewport renderer, compared with Scintilla integrated through QScintilla and Qt 6. No complete editor or document-management implementation is included in this evaluation.

## Executive recommendation

**Recommendation: keep C++20 and Qt 6, and proceed with a project-owned editor engine and viewport renderer, subject to a focused prototype before committing to its buffer design.** This is the more demanding path, but it aligns directly with Note's ownership and rendering boundaries, gives the project control over its buffer and theme model, and avoids making QScintilla's version and licensing constraints part of the product foundation.

Treat this as an architecture recommendation, not a demonstrated performance win. The project-owned path has the highest implementation and maintenance cost, and may not be the fastest way to reach a usable editor. If the immediate project objective changes to reducing time-to-usable-editor, QScintilla remains a credible alternative only after a license decision, a real Qt 6 build, and a documented adjustment to how the Editor Engine and Drawing Surface boundaries are implemented.

**No timing or memory numbers are reported.** The required reference low-end device has not been named, and this environment has CMake and a C++ compiler but no Qt 6 or QScintilla installation. Comparing source-level capabilities is practical; a fair GUI benchmark is not. Installing a GUI stack or substituting a non-GUI microbenchmark would not produce comparable evidence. The performance targets in section 7.0 remain provisional.

## Candidates

### A. Project-owned text editor and custom viewport renderer

The project owns the text buffer, cursor and selection state, undo history, editing rules, layout decisions, syntax spans, and renderer. Qt provides the window system, input events, font/text shaping primitives, and drawing surface implementation. The editor engine communicates draw work through the framework-agnostic interface required by the specification.

This candidate is not equivalent to putting a `QPlainTextEdit` inside the application. Qt documents `QPlainTextEdit` as optimized for large documents and quick input, but also cautions that especially large files may make the application unresponsive or run out of memory. A `QPlainTextEdit`-based prototype would be a useful control measurement, but it is not the proposed custom-renderer architecture and does not, by itself, supply Note's full multi-cursor behavior or project-owned buffer ownership. [Qt `QPlainTextEdit`](https://doc.qt.io/qt-6/qplaintextedit.html) [Qt guidance for large text documents](https://doc.qt.io/qt-6/richtext-advanced-processing.html)

### B. Scintilla integrated with Qt 6

For this comparison, “Scintilla integrated with Qt 6” means **QScintilla**, the Qt port of Scintilla. Scintilla provides an editor control, document storage/editing APIs, styling and selection features; QScintilla supplies Qt integration. It can reuse a substantial set of mature editor behavior instead of implementing it in Note.

There is a product-fit and maintenance caveat: QScintilla's current published source package is 2.14.1, and the project maintainer has described it as effectively in maintenance mode with its Scintilla core at 3.10.1 and no immediate plan for a major upgrade. Scintilla 5 moved lexers into Lexilla, so current Scintilla/Lexilla capabilities cannot be assumed to exist in QScintilla. [QScintilla introduction and license](https://riverbankcomputing.com/software/qscintilla/intro) [QScintilla downloads](https://riverbankcomputing.com/software/qscintilla/download) [QScintilla maintainer note](https://www.riverbankcomputing.com/pipermail/qscintilla/2024-June/001636.html) [Scintilla documentation: Lexilla split](https://www.scintilla.org/ScintillaDoc.html)

## Comparison

| Requirement | Project-owned engine + custom viewport | QScintilla + Qt 6 |
|---|---|---|
| **Startup time** | Qt startup is common to both. A small custom engine can avoid initializing a general-purpose lexer or unused editor features, but there is no measured baseline. More code does not inherently mean slower startup if loaded lazily. | Also pays Qt startup. The editor library adds code and initialization; whether that is material is unknown. Reuse can reduce Note's own startup work. Measure the packaged app, not library size alone. |
| **Idle memory** | Potentially the lowest floor because the project selects its buffer, layout caches, and syntax data. Actual memory depends heavily on the buffer and Qt allocations. A naive gap buffer/string plus copied line tables could be worse. | Adds Scintilla's document and styling structures. Scintilla's documentation states each byte has a styling byte in the normal model and describes a “styles none” document option that can save significant memory when styling is disabled. With syntax enabled, this extra storage is relevant; actual 50 MB usage remains unmeasured. [Scintilla document options](https://www.scintilla.org/ScintillaDoc.html) |
| **Typing latency** | Direct control over edit transactions and invalidation makes low latency attainable, but input handling, cursor updates, undo grouping, shaping, and repaint scheduling all become Note responsibilities. This is the highest implementation risk until measured. | Mature editing paths reduce implementation risk. Multiple selections can apply typing/navigation to all carets. But QScintilla's older core and wrapper behavior need hands-on validation. No measured latency claim. [Scintilla multiple-selection API](https://www.scintilla.org/ScintillaDoc.html) |
| **Scrolling and rendering** | Can strictly lay out and draw only visible lines, matching the specification. Must implement line indexing, vertical positioning, wrapping, invalidation, clipping, selection/caret painting and font metrics. Long lines and wrapping are difficult edge cases. | Scintilla already supplies its editor viewport, scrolling, margins, wrapping and styling paths. This significantly reduces custom renderer work. The rendering model is owned by the component, however, and is not Note's framework-agnostic Drawing Surface pipeline. Actual frame behavior on Note's target devices is unknown. |
| **Progressive opening of 50 MB** | Can expose a visible viewport before indexing/highlighting the entire file, provided file decoding, line indexing and buffer insertion are designed for streaming/chunking. This is considerable engineering and must be verified with the selected buffer. | Scintilla has document APIs and large-document options, and can avoid styling allocation when styling is disabled. QScintilla's older core may still have distinct large-file behavior. Progressive loading and responsiveness must be prototyped; neither the 50 MB target nor no-stall behavior is established by capability documentation alone. |
| **Incremental syntax highlighting** | Best fit for the required contract: data-driven language definitions, line-based invalidation, viewport-first work and deferred remainder processing can be designed explicitly. A bounded tokenizer/state propagation system must be built and guarded against pathological lines. | Scintilla has lexer/container styling mechanisms, range-based styling and idle styling concepts. However, QScintilla's core predates the Scintilla 5/Lexilla split. Supplying all required language coverage as data without coupling to built-in C++ lexer code needs a prototype and could require a custom adapter or lexer. [Scintilla styling and lexer APIs](https://www.scintilla.org/MyScintillaDoc.html) |
| **Multiple cursors** | Fully controllable and can match the required editing semantics, but correctness across overlapping edits, undo/redo, selection transforms, mouse actions and line changes is costly. | Strong advantage: Scintilla exposes multiple selections and optional typing/navigation across all selections. Integrate and verify the desired conventions rather than reimplementing them. |
| **Custom theme integration** | Theme tokens can be the native renderer input. Any supported editor/interface color or font setting can map directly to project-owned drawing commands without reconstructing buffers. | Scintilla exposes extensive style, caret, selection, indicator, margin and element colors. Theme coverage is broad, but must be mapped to Scintilla's style/element system and checked for gaps or platform defaults. Theme changes should be cheap, though this requires measurement. |
| **Appearance and behavior control** | Maximum control over line numbers, current-line display, whitespace, guides, matching occurrences, wrapping, caret behavior and future agreed settings. Also maximum responsibility for consistency and edge cases. | Many requested editor decorations and behaviors are configurable in the component. Changes outside its API or native rendering assumptions may require workarounds, forks, or upstream maintenance. Do not treat unapproved Appendix A requests as requirements. |
| **Cross-platform compatibility** | Qt supplies the platform windowing, input and font primitives; Note must validate its own renderer and input behavior on Windows, Linux and macOS. | QScintilla runs on Qt-supported platforms, reducing OS-specific editor rendering work. Still validate builds, deployment, dialogs and behavior on all three OSes. QScintilla's published development-platform statement lists Windows 11, Linux and macOS. [Riverbank supported platforms](https://en.riverbankcomputing.com/commercial/pyqt) |
| **Implementation complexity** | Very high. Buffer, editing semantics, rendering, input, selection, undo, large-file operation and accessibility/platform edge cases form a substantial product subsystem. The sequence and acceptance criteria must remain small. | Medium initially: mature editor behavior is ready to integrate. Complexity shifts to wrapping its API cleanly, theming, lexer compatibility, packaging, license compliance and maintaining the dependency. |
| **Maintenance burden** | The project owns every bug and regression, but can evolve the architecture and data model without waiting on an upstream API. Requires sustained editor expertise and tests. | Less core editor code to maintain, but a dependency on a project in maintenance mode with an older Scintilla base. A fork or custom compatibility layer could transfer maintenance back to Note. QScintilla is GPLv3 or commercial; confirm licensing compatibility before adopting it. [QScintilla license](https://riverbankcomputing.com/software/qscintilla/intro) |

## Architectural implications

### If the project-owned renderer is selected

The existing System 03 / System 02 boundary can remain as written: the Editor Renderer decides what is visible and emits framework-agnostic drawing instructions; a Qt-backed Drawing Surface implements those instructions in the UI layer. The text buffer, cursor, selection and undo history remain under the ownership prescribed in section 6.

The major risk is scope: do not implement every renderer decoration and editing convenience at once. The first prototype should answer whether a small buffer and viewport can provide responsive insertion, cursor movement, selection, line layout, and repaint without full-document layout. Keep wrapping and highlighting out of that first measurement unless needed to compare candidate costs.

### If QScintilla is selected

Scintilla internally owns the editable document, styling, selection/cursor state, and undo machinery. To avoid duplicate state, those become authoritative in the adopted editor component; Note's Document Model must hold references/handles and metadata rather than copying the buffer or cursor state. The application can still own document orchestration and file persistence.

The renderer boundary would need a deliberate adaptation: QScintilla itself performs framework-bound viewport rendering, so the framework-agnostic Drawing Surface is no longer the drawing path for editor text. The project should document this as a concrete Editor Engine implementation behind an adapter, with UI integration separated from document-management policy. Do not mirror live text/cursor/history in a second project-owned copy.

Before choosing QScintilla, verify that its styling/lexer interface can meet data-driven language definitions and incremental viewport-first highlighting without a permanent local fork, and make an explicit GPLv3/commercial licensing decision. QScintilla is not a drop-in way to retain every current renderer boundary unchanged.

## Benchmark protocol for the next pass

The reference device is still unspecified in section 7.0. Once named, build two minimal applications against the same Qt 6 release, compiler, optimization mode, OS, font, window size, power mode and input workload. Pin the dependency versions and record them with results. Do not compare a debug build against a release build.

Use identical fixtures and a fresh process per sample:

1. **Cold start:** launch to a visible, input-ready editor window; report median and p95 over at least 20 starts, separating first-run shader/cache effects if any.
2. **Idle memory:** open the same small source file, wait for a fixed idle period, report process private working set/RSS and peak memory.
3. **Typing:** inject a deterministic sequence of single-character and newline edits at fixed document positions; measure input-to-present latency with platform tracing or a high-speed external capture. Report median, p95 and p99.
4. **Scrolling:** use a generated 50 MB source-like file with fixed line lengths and a reproducible scroll trace; record frame-time distribution and dropped frames.
5. **Progressive open:** time launch/open through first visible text, usable viewport, and full indexing; sample UI responsiveness and peak memory throughout. State whether syntax highlighting is enabled.
6. **Highlighting:** edit one line near the start, middle and end of the same file; measure time to correct visible styling and background catch-up, ensuring unchanged regions are not rescanned unnecessarily.
7. **Theme update:** change representative editor, syntax, caret, selection and margin colors while the file is open; verify no buffer recreation or visible pause.

Run both candidates with highlighting off and on. Record the exact 50 MB file generator/seed and platform counters. A benchmark should not be described as satisfying the budget until the project owner confirms the provisional thresholds.

## Findings and decision record

- **Measured comparison:** none. No Qt 6 or QScintilla development installation is present in the current environment, so a comparable build/run was unavailable.
- **Documented capability comparison:** Scintilla supplies multiple-selection, lexer/container styling, style customization, and large-document options; Qt `QPlainTextEdit` is optimized for plain text but Qt warns about very large document responsiveness/memory. These facts identify candidate strengths and risks, not measured Note behavior.
- **Recommendation:** prefer a project-owned viewport renderer for architectural fit and long-term control, but gate its implementation on a small same-device prototype. If that prototype cannot meet the provisional input/scroll budget or consumes too much engineering time, revisit QScintilla with the architectural and license conditions above. Do not claim the 50 MB or 16 ms targets are met yet.
