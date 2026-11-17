# Project-owned editor prototype

**Outcome:** the project-owned editor architecture is demonstrated in an isolated Qt 6 Widgets prototype. A piece-table model outperformed a gap buffer for the measured random edits, while the gap buffer was faster for tightly local repeated typing. The recommended production direction is an Editor Engine with a piece-table text model and viewport-only rendering. The exact indexed piece-tree and line-index implementation remains provisional until tested on the agreed low-end reference device.

This is a technical prototype, not a production editor and not evidence that the V1 performance budget is met.

## Prototype contents

All prototype source is under [`benchmarks/editor_prototype/`](../benchmarks/editor_prototype/), outside the production `src/` tree.

- `GapBuffer`: contiguous storage with a movable gap.
- `PieceTable`: original/add buffers and a flat piece descriptor vector.
- `editor_widget_prototype`: Qt-painted viewport using the piece table, fixed-width font metrics, line-start index, caret, selection, mouse positioning, wheel scrolling, insertion/deletion, and chunked file loading.
- `buffer_bench`: deterministic 50 MiB fixture generation, buffer measurements, and self-tests.

The widget deliberately excludes syntax highlighting, wrapping, undo, multi-cursor, robust Unicode cursor semantics, file management, and production-quality error handling. Its line-start vector shifts affected offsets after edits in O(number of indexed lines); this is a measured prototype shortcut, not the proposed final index. The prototype does not add behavior to the production application.

## Environment and method

| Item | Value |
|---|---|
| Host | Windows NT 10.0.26300.0; 16 logical processors exposed to this process |
| Compiler | MSYS2 MinGW-w64 GCC 15.2.0 |
| CMake / generator | CMake 4.1.0 / Ninja |
| Qt | Qt 6.9.1 Widgets |
| Rendering mode | Qt `offscreen` platform plugin; raster painting, no visible display or vsync measurement |
| Reference device | Not identified; CPU model and physical RAM were unavailable through the managed host |

The benchmark targets were compiled with `-O2` (or `/O2` on MSVC). The 50 MiB fixture is deterministic ASCII text with 64 characters plus LF per line (65 bytes per line), for 52,428,800 bytes and 806,597 indexed lines. Loads use 1 MiB chunks for the buffer comparison and 256 KiB chunks through `QFile`/`QTimer` in the widget. Qt GUI metrics below are medians of three runs unless stated otherwise. Buffer measurements are also three runs with the same fixture and random seed.

The process-launch sampling used ten separate app processes and measured with PowerShell from process creation to the app's first-content log entry. This includes polling overhead and is a process-launch proxy, not a true cold-start-to-typeable-window measurement. In-process timings begin before `QApplication` construction. All samples ran with file caches warm after fixture generation.

## Text-buffer candidates

The benchmark compares a simple gap buffer with a flat-vector piece table. It also evaluates a rope/tree-backed alternative conceptually; that third structure was not implemented or measured.

| 50 MiB workload | Gap buffer | Flat piece table |
|---|---:|---:|
| 1 MiB chunked load, median | 8.04 ms | 10.46 ms |
| Allocated buffer capacity after load | 50.00 MiB | 50.78 MiB |
| First insert/delete at document center (includes initial gap movement) | 1.96 ms | 1.5 µs |
| 10,000 local insert/delete pairs, per-pair mean | 11.5 ns | 165 ns |
| 500 sequential insert/delete pairs | 2.6 µs | 78.3 µs |
| 1,000 persistent random insertions, total | 1,142 ms | 1.95 ms |
| Random insertion p95, per operation | 2,830 µs | 3.2 µs |
| 1,000 random 256-byte range copies | 0.75 ms | 1.01 ms |
| Allocated buffer capacity after the random insertions | 50.00 MiB | 50.83 MiB |
| Piece descriptors after the random insertions | 1 | 2,001 |

The local typing and sequential workloads favor the gap buffer. Moving the gap and retaining edits at distant positions favor the piece table by a wide margin. The piece table carries about 0.8 MiB more buffer capacity in this setup and pays somewhat more for local edits and range lookup. Its flat descriptor vector scales linearly with fragmentation; the 2,001-piece result is still a small data set, so it does not justify shipping a flat-vector index for unbounded editing.

**Buffer recommendation:** use a piece-table storage model, but index pieces in a balanced sequence tree with subtree byte counts. Keep line lookup metadata in the same or a coordinated augmented index so cursor-to-line lookup does not require rescanning document text. This preserves the fast append-only edit storage measured here while addressing the flat vector's O(piece-count) lookup/update cost. The tree choice, compaction policy, and worst-case fragmented workload still need a prototype and a reference-device benchmark. A rope remains a viable alternative if those measurements favor it; this milestone does not claim it was benchmarked.

## Qt viewport and 50 MiB measurements

The widget uses a custom `QWidget`/`QPainter` viewport. It lays out and paints visible lines only, and its file loader yields to the Qt event loop between 256 KiB chunks.

| Measurement | Result |
|---|---:|
| In-process time to first content, median of 3 | 99 ms (96–101 ms) |
| In-process time to first paint, median of 3 | 98 ms (96–101 ms) |
| Separate process launch to first content, 10 runs | 176.6 ms median; 171.3–208.7 ms range |
| 50 MiB progressive load to complete line index, median of 3 | 193 ms (189–200 ms) |
| 100 synthetic key events to paint, 50 MiB file, median of run medians | 2.89 ms; run p95 values 3.08–3.46 ms |
| 100 synthetic key events to paint, 4,279-byte/100-line file | 1.82 ms median; 2.02 ms p95 |
| Top-to-bottom scripted scroll to paint, 200 transitions, median of run medians | 1.50 ms median; run p95 values 1.54–1.61 ms |
| `paintEvent` drawing 63 visible lines, median of run medians | 1.53 ms median; run p95 values 1.61–1.73 ms |
| Small-file idle memory, 50 samples at 100 ms intervals | 24.6 MiB working set; 38.7 MiB private bytes (median and p95) |

These are measured results from this host and the offscreen Qt plugin. They establish that the prototype can display and edit a 50 MiB file progressively on this machine. They do not measure a real display's input-to-photon latency, frame pacing, GPU/compositor overhead, low-end hardware behavior, or cold disk startup. The offscreen renderer's synchronous paint time must not be presented as a 60 fps or no-dropped-frame result.

The 50 MiB `PieceTable` reserves source capacity plus add-buffer capacity. The line-start vector contains over 806,000 entries and adds several MiB; the small-file idle-memory result does not represent the 50 MiB document. Text shaping is limited to Qt's fixed-width font metrics and `drawText`; wrapping, tabs, bidi behavior, and general Unicode cursor movement are not validated.

## Validation performed

- C++20 stdin compile probe: passed.
- CMake configuration and build of `buffer_bench`: passed.
- Randomized differential buffer test: passed for 2,000 insert/delete operations each on the gap buffer and piece table.
- Qt 6 widget prototype build: passed.
- Qt offscreen interaction test: passed for text insertion, backspace/newline, horizontal/vertical cursor movement, keyboard and mouse selection, wheel scrolling, and painting.
- Production shell CMake configure/build: passed with Qt 6.9.1 on this Windows host.
- 50 MiB chunked buffer load, local/random edits, viewport-range copies, progressive widget load, scripted scrolling, typing, and painting: measured as above.

Not validated: Linux/macOS builds, any low-end reference device, real-display frame timing, syntax highlighting, multi-cursor behavior, large-file memory budget, production error handling, or QScintilla runtime performance.

## Recommended V1 architecture

1. Retain C++20, Qt 6 Widgets, and CMake. The application shell and isolated Qt prototype build on this Windows environment.
2. Keep editing policies and authoritative text/cursor/selection/history state in the Editor Engine. Keep Qt-specific widgets and drawing-surface implementation in the UI system.
3. Use a piece-table model as the provisional document storage choice, with a balanced piece index rather than the benchmark's flat descriptor vector. Avoid a second full text copy for rendering or document metadata.
4. Use a viewport-only renderer. Request line text and syntax spans only for visible rows, and invalidate only affected regions. The prototype validates plain-text rows, caret, selection, and scroll; it does not yet validate wrapping or syntax rendering.
5. Add the piece-tree and line-index implementation in small, measurable steps. Record typing and scroll benchmarks on the named low-end device before accepting the structure or setting large-file thresholds.

This recommendation supports the project-owned architecture over QScintilla for Note's state ownership and drawing boundaries. QScintilla was not built or benchmarked in this milestone; the prior evaluation's licensing/version concerns remain separately relevant.
