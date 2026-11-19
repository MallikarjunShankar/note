# Hardware targets and performance validation

**Status:** provisional engineering guidance. This document does not change the product requirements or their provisional status in [`PROJECT_SPEC.md`](../PROJECT_SPEC.md#70-performance-budget-provisional). Physical low-end hardware validation has not been performed.

## Hardware profiles

### Provisional minimum engineering target

| Component | Target |
|---|---|
| CPU | 2-core, 4-thread x86-64 processor |
| RAM | 4 GB system memory |
| Storage | SSD preferred; HDD supported |
| GPU | Integrated graphics |
| Display | 1366 × 768 |
| Operating systems | Windows 10/11, mainstream Linux distributions, and macOS versions supported by the selected Qt release |

This is a planning target, not a compatibility claim. The project has no dedicated low-end test machine and has not validated NOTE on this profile. OS support still requires build, packaging, and behavior validation for each claimed release platform.

### Recommended development environment and measured host

Use the primary development machine for routine development benchmarks and record its actual hardware before interpreting results. The minimum profile above is a compatibility target, not a recommended workstation specification; this project does not set a separate numeric recommended profile.

The current development host is an AMD Ryzen 7 7435HS (8 cores/16 logical processors), 24 GiB RAM, Windows 10 Home Single Language as reported by WMI (NT 10.0.26300.0, build 26300.9974), and a 512 GB Micron NVMe SSD (NTFS, approximately 205.6 GB free at measurement time). Performance runs used MSYS2 MinGW-w64 GCC 15.2.0, Qt 6.9.1, CMake 4.1.0, Ninja, and Release configuration unless stated otherwise. GPU, display/refresh rate, and power mode were not recorded. This machine is **not** an owner-approved reference device. The owner has not selected a reference device; the provisional minimum profile remains unvalidated.

The earlier editor prototype also ran on Windows NT 10.0.26300.0 with 16 logical processors exposed, GCC 15.2.0, CMake 4.1.0/Ninja, and Qt 6.9.1; its GUI measurements used Qt's offscreen plugin. See [`EDITOR_PROTOTYPE.md`](EDITOR_PROTOTYPE.md) for its setup and results.

## Budgets and acceptance status

The numeric values in §7.0 of the product spec remain the provisional product acceptance targets: cold start to a typeable window under 300 ms, idle memory under 50 MB with one small file, typing update under 16 ms, and scrolling without dropped frames in a 50 MB file. They have not been confirmed by the owner and are not shown as achieved. The following additional engineering targets do not replace or relax those values.

| Metric | Working target / required behavior | Measurement definition and repeatable method | Gate |
|---|---|---|---|
| Startup | Under 2 seconds on the primary development machine | Fresh process launch to a visible, input-ready window; report process launch and first usable window separately. Run at least 20 samples, report median and p95, and identify warm/cold cache conditions. | Provisional development target; not a replacement for the spec's under-300-ms low-end target. |
| Typing latency | Below 16 ms at p95 | Timestamp a deterministic key event and the corresponding painted update. Test small and 50/100 MB documents at fixed positions. Report median, p95, p99, and event-to-paint separately from physical input-to-photon. | Provisional target matching the spec's numeric target; physical presentation timing remains required for acceptance. |
| Idle memory | Below 100 MB with a small document | After startup and opening a fixed small fixture, wait 60 seconds without interaction; sample private bytes and working set/RSS, report median, p95, and peak. | Provisional development target; stricter spec target remains under 50 MB on the reference low-end device. |
| Large-file responsiveness | Remain usable with files around 50–100 MB | Record time to first visible text, time to interactive viewport, time to complete loading/indexing, peak memory, and typing/scroll latency during loading and after it. Test representative short-line, typical, and very-long-line fixtures. | Qualitative provisional goal until product-specific latency and memory thresholds are confirmed; responsiveness during opening is required by the spec. |
| Scrolling/rendering | Do not process the entire document on every frame | Profile a fixed scroll trace; record viewport work, frame-time distribution and dropped frames on a real display. Confirm visible-row rendering and bounded incremental work through instrumentation/code review. | Viewport-bounded work is an architectural requirement. Frame-rate budget is provisional in spec §7.0 and awaits real-display low-end validation. |
| Idle CPU | Negligible when no work is occurring | After the same 60-second idle warm-up, sample process CPU time for at least 60 seconds; report average use as a fraction of one logical core and any periodic wakeups/background tasks. | No polling or needless idle work is a spec requirement; a numeric CPU ceiling is not yet agreed. Treat measured values as trend data, not pass/fail. |

No provisional value becomes a formal regression threshold until the project owner confirms the budget and reference device. After confirmation, budget misses fail validation unless an explicit product decision changes the target. Avoid translating prototype timings into production guarantees.

## Repeatable validation method

For every result, record commit SHA, date, OS/version, CPU model and core configuration, RAM, storage, GPU, display/resolution/refresh rate, power mode, compiler, build type and flags, CMake/generator, Qt version/platform plugin, fixture generator/version/seed, file size/shape, and whether filesystem caches were warm. Use an optimized release-like build for performance numbers; keep debug results separate. Run each scenario in a fresh process where applicable, collect at least 20 samples for startup and latency distributions, and use at least 5 runs for longer load/scroll scenarios. Report raw samples or an attached machine-readable result along with median, p95 and p99 as applicable. Do not average away outliers without explaining them.

Use deterministic fixtures and fixed interaction traces so revisions can be compared. The large-file set should include 50 MiB and 100 MiB files, many short lines, representative source-like lines, and a very long line. State whether syntax highlighting is enabled. Keep first visible content, interactive viewport, full read, line indexing, and highlighting catch-up as separate timings. A synthetic widget event-to-paint measure is only a proxy; use platform tracing or high-speed capture for input-to-photon claims, and a visible display with refresh/vsync for frame-pacing claims.

| Area | Required observations |
|---|---|
| Startup/open | Process launch to usable window; open-to-first-content, open-to-interactive, and full load/index completion; responsiveness and peak memory during progressive open. |
| Editing | Fixed insertion, deletion, replacement, and navigation workloads at start/middle/end; latency distribution, allocation counts/bytes where available, and resulting document integrity. |
| Scrolling/rendering | Fixed viewport and scroll trace on large fixtures; frame-time distribution, dropped frames, paint duration, visible rows processed, and whether work scales with viewport or whole document. |
| Memory | Idle private bytes and working set/RSS; document-size scaling; peak memory during opening/editing; allocation volume and retained caches where instrumentation permits. |
| Highlighting | Incremental edits near start/middle/end; time until visible styles are correct and background catch-up; confirm unchanged regions are not reprocessed unnecessarily. |
| Idle | CPU-time delta, wakeups/timers, and memory samples during a fixed idle interval after opening a small file. |
| Theme/configuration | Repeated theme and settings changes with a document open; verify affected UI updates without buffer recreation, and record latency/allocations. |

### Stress scenarios

Run the scenarios as systems become implemented; do not treat this list as permission to implement extra product scope:

- 50 MiB and 100 MiB documents with varied line distributions, including extreme line lengths.
- Multiple simultaneously open tabs at representative and high counts; record total and per-document memory and tab-switch latency. Record the exact counts used; no product tab limit is implied here.
- Repeated local and distant insertions/deletions, undo/redo once implemented, and long edit sessions to expose fragmentation and index growth.
- Large selections and multi-cursor edits once those specified editor behaviors exist; validate correctness as well as latency and allocations.
- Repeated theme and configuration updates with open documents, checking for accidental buffer recreation or unbounded retained state.

### Resource-constrained testing

Where available, use OS facilities to constrain CPU quota/affinity and memory for supplementary runs (for example, Linux cgroups/containers or Windows job/resource controls). Record the exact mechanism and limits. These controls do not reproduce an older CPU's per-core performance, memory bandwidth, real storage latency, integrated GPU/compositor behavior, thermals, or platform-specific scheduling. Such runs are diagnostic only and cannot substitute for tests on the provisional minimum physical profile.

### Regression tracking

Keep the procedure, fixture generator, seeds, build configuration, and instrumentation stable across comparisons. Record results with the commit SHA and all environment details above; compare against the previous result from the same machine and configuration. Investigate a repeatable change greater than 10% in median or p95 (or a new correctness/resource failure) across at least three comparable runs. This 10% value is an investigation trigger to account for noise, not a product budget or automatic pass/fail limit. Investigate isolated outliers; do not promote one-off best-case numbers to a baseline. Once the reference hardware and product budgets are confirmed, maintain explicit baselines and fail CI/release validation on agreed budget regressions.

## Existing evidence and limitations

[`EDITOR_PROTOTYPE.md`](EDITOR_PROTOTYPE.md) records actual isolated prototype measurements: 50 MiB progressive line indexing median 193 ms, process launch to first-content log median 176.6 ms over 10 runs (a launch proxy, not cold start to a typeable window), synthetic 50 MiB key-to-paint median-run median 2.89 ms, offscreen scroll/paint timings, and small-file idle memory of 24.6 MiB working set / 38.7 MiB private bytes. These are prototype results on the described host and offscreen plugin. They do not establish production performance, true input-to-photon or real-display scrolling, the minimum hardware target, or Linux/macOS compatibility. The 50 MiB document memory was not represented by the small-file idle-memory sample.

Physical low-end hardware, true cold startup, real-display frame pacing, multi-tab stress, long-line thresholds, and cross-platform release builds remain outstanding. The production background-open path and process memory are measured below, but its Qt Core timer is only an event-loop proxy; it does not measure a visible GUI's input-to-photon response, progressive content display, or minimum-hardware behavior.

## Phase 02 final verification results

Measurements below were made on the development host described above, not on a reference device. The provisional product targets are unchanged. A measured miss is reported as **Fail** on this host; it is not a claim about the untested reference profile.

| Metric | Provisional target | Measured result | Status |
|---|---|---|---|
| Startup | Cold start to typeable window under 300 ms | 20 fresh application processes; filesystem cache was not cleared. First visible window: median 642.7 ms, p95 699.6 ms. Ctrl+N/title change as the usable-editor proxy: median 680.0 ms, p95 739.6 ms. These are warm-cache launch measurements, not cold-start results. | **Fail** |
| Idle memory | Under 50 MB with one small file open | After 60 seconds idle, 10 samples at about 1-second intervals: working set median 81.21 MiB (range 81.21–81.24, nearest-rank p95 81.24), private bytes median 50.14 MiB (range 50.14–50.18, nearest-rank p95 50.18). Idle CPU time was 0.109 seconds over about 70 seconds. | **Fail** |
| Keystroke-to-screen | Under 16 ms | No real-widget key-to-paint or physical input-to-photon latency measurement was made. | **Not measured** |
| Scrolling | No dropped frames in a 50 MB file | No real-display scroll trace or frame-pacing measurement was made. Existing offscreen renderer tests/benchmarks do not establish this target. | **Not validated on reference hardware** |
| Opening a 50 MB file | Window responsive; content usable progressively | The `DocumentWorkflow::openAsync` Core benchmark completes in 844–857 ms with first read progress at 11–14 ms, but publishes only a complete buffer; it does not measure visible GUI responsiveness or progressively usable text. | **Fail** |
| Theme/settings changes | No buffer recreation or visible stall | Settings/theme application is not implemented, so this behavior was not measured. | **Not measured** |

Startup samples below are milliseconds from process launch; the editor-ready proxy is the elapsed time until Ctrl+N changed the window title. The reported p95 uses linear interpolation between ordered samples.

| Run | First visible window | Editor-ready proxy |
|---:|---:|---:|
| 1 | 697.0 | 739.3 |
| 2 | 704.3 | 745.4 |
| 3 | 659.5 | 701.9 |
| 4 | 632.0 | 664.3 |
| 5 | 633.3 | 662.7 |
| 6 | 618.0 | 650.4 |
| 7 | 641.3 | 684.6 |
| 8 | 630.2 | 660.3 |
| 9 | 653.1 | 697.9 |
| 10 | 644.0 | 675.3 |
| 11 | 626.5 | 658.1 |
| 12 | 669.3 | 700.4 |
| 13 | 699.3 | 729.8 |
| 14 | 696.7 | 728.6 |
| 15 | 671.9 | 703.4 |
| 16 | 658.0 | 688.5 |
| 17 | 620.0 | 669.0 |
| 18 | 613.9 | 662.3 |
| 19 | 626.6 | 658.0 |
| 20 | 616.8 | 648.8 |
| **Median** | **642.7** | **680.0** |
| **p95 (linear)** | **699.6** | **739.6** |

The idle sample is a single GUI-process run with one small text fixture; the fixture's exact byte count was not retained. The private-bytes median slightly exceeds 50 MiB (about 52.6 decimal MB), and the working set is higher still. Ten closely spaced samples are descriptive, not a stable cross-run percentile. The CPU result is trend data only because no numeric idle-CPU budget is specified.

The actual app's startup measurements distinguish first visible window from the chosen usable-editor proxy. The filesystem cache was not cleared, so true cold startup remains **not measured**. The current host is not the reference device, therefore even these host failures do not complete reference-device acceptance.

## Asynchronous file opening

`benchmarks/asynchronous_open/async_open_bench.cpp` accepts 1, 10, or 50 MiB (or another positive size up to 1024 MiB), creates a deterministic short-line fixture, opens it through `DocumentWorkflow::openAsync`, records first read progress and final publication, and runs a 1 ms Qt Core heartbeat. Windows process working set and private bytes are sampled every 20 ms to limit instrumentation overhead. Build the Release target `note_async_open_bench`; run it with the size in MiB. Fixtures are written immediately before opening and have warm filesystem cache. The heartbeat excludes QWidget painting and physical display response. Completion means publication of the full buffer; this is not progressive partial-document display.

Five fresh-process runs per size on the current host produced:

| Size | Run | First progress | Full open/publication | Maximum heartbeat gap |
|---:|---:|---:|---:|---:|
| 1 MiB | 1 | 10 ms | 30 ms | 2 ms |
| 1 MiB | 2 | 14 ms | 34 ms | 2 ms |
| 1 MiB | 3 | 16 ms | 38 ms | 2 ms |
| 1 MiB | 4 | 11 ms | 33 ms | 2 ms |
| 1 MiB | 5 | 11 ms | 31 ms | 2 ms |
| 1 MiB | **Median** | **11 ms** | **33 ms** | **2 ms** |
| 10 MiB | 1 | 11 ms | 184 ms | 2 ms |
| 10 MiB | 2 | 11 ms | 186 ms | 2 ms |
| 10 MiB | 3 | 12 ms | 190 ms | 2 ms |
| 10 MiB | 4 | 12 ms | 184 ms | 2 ms |
| 10 MiB | 5 | 11 ms | 181 ms | 2 ms |
| 10 MiB | **Median** | **11 ms** | **184 ms** | **2 ms** |
| 50 MiB | 1 | 11 ms | 857 ms | 2 ms |
| 50 MiB | 2 | 11 ms | 844 ms | 2 ms |
| 50 MiB | 3 | 11 ms | 846 ms | 3 ms |
| 50 MiB | 4 | 14 ms | 854 ms | 2 ms |
| 50 MiB | 5 | 12 ms | 849 ms | 2 ms |
| 50 MiB | **Median** | **11 ms** | **849 ms** | **2 ms** |

Five samples are not enough to claim a meaningful p95; ranges and medians are reported instead. The timer gap remained at most 3 ms during these runs, which only shows that this Core event loop serviced callbacks. It does not prove a responsive GUI while opening.

| Size | Sampled peak working set (median; range) | Sampled peak private bytes (median; range) | After-settle working set / private bytes (median) | Buffer estimated storage |
|---:|---:|---:|---:|---:|
| 1 MiB | 16.44 MiB (16.42–17.43) | 37.75 MiB (36.75–37.99) | 16.43 / 35.98 MiB | 1.14 MiB |
| 10 MiB | 29.16 MiB (27.16–33.98) | 55.34 MiB (47.94–56.05) | 26.18 / 45.43 MiB | 11.13 MiB |
| 50 MiB | 118.32 MiB (116.00–122.46) | 141.50 MiB (139.49–149.52) | 70.75 / 93.83 MiB | 59.00 MiB |

Peaks are sampled every 20 ms and may miss short-lived maxima. Buffer storage is the TextBuffer estimate, not process memory; it excludes allocator and application overhead.

Raw per-process memory samples (MiB):

| Size | Run | Peak working set | Peak private bytes | After-settle working set | After-settle private bytes |
|---:|---:|---:|---:|---:|---:|
| 1 MiB | 1 | 16.42 | 37.75 | 16.43 | 35.97 |
| 1 MiB | 2 | 16.44 | 36.76 | 16.45 | 35.99 |
| 1 MiB | 3 | 17.43 | 37.99 | 16.43 | 35.98 |
| 1 MiB | 4 | 17.42 | 37.99 | 16.43 | 35.98 |
| 1 MiB | 5 | 16.44 | 36.75 | 16.45 | 35.99 |
| 10 MiB | 1 | 33.98 | 55.34 | 26.18 | 45.43 |
| 10 MiB | 2 | 27.16 | 56.05 | 26.17 | 45.40 |
| 10 MiB | 3 | 27.18 | 47.94 | 26.19 | 45.43 |
| 10 MiB | 4 | 33.73 | 55.31 | 26.16 | 45.41 |
| 10 MiB | 5 | 29.16 | 55.96 | 26.18 | 45.43 |
| 50 MiB | 1 | 116.00 | 139.49 | 71.87 | 95.09 |
| 50 MiB | 2 | 122.46 | 149.52 | 70.75 | 93.82 |
| 50 MiB | 3 | 118.83 | 141.49 | 70.70 | 93.80 |
| 50 MiB | 4 | 118.32 | 143.50 | 70.71 | 93.83 |
| 50 MiB | 5 | 117.20 | 141.50 | 70.77 | 93.83 |

### Why the historical 50 MiB result differs

An earlier five-run benchmark recorded 50 MiB completion times of 285, 286, 286, 287, and 288 ms (median 286 ms), with first progress at 8–10 ms. That benchmark came from the asynchronous-open implementation before commit `9cd8fd5` added content-fingerprint verification. The current `FileService::open` hashes the bytes while reading, then performs a second full-file inspection/hash after building the buffer to detect changes that preserve size and timestamp. The current warm-cache median is 849 ms (844–857 ms), roughly 3 times the historical median. The code-path change plausibly explains a substantial part of the increase, but the old and new measurements were not run as a controlled same-build A/B; the difference must not be attributed solely to hashing.

The second read protects data integrity and was not removed. There is also a post-publication background inspection for monitoring; it is not included in the benchmark's completion timestamp. A safer way to avoid redundant full-file I/O would require a separately designed and tested file-identity/consistency strategy; none was introduced here. Save paths also hash existing files before atomic replacement, and `writeDocument` currently takes a full TextSnapshot copy. These costs deserve further profiling, but no production tuning was made because removing or changing checks without an equivalent integrity guarantee would be unsafe. Current saves retain `QSaveFile` atomicity and final fingerprint validation.

Cancellation is checked between read chunks and after index construction; the current index build itself is not interruptible. The production workflow still publishes only after a complete buffer is built. Progressive partial-document display and actual GUI responsiveness during a 50 MiB open remain unimplemented/unvalidated.

## Phase 02 document foundation benchmarks

The following five-run Release results are the historical comparison from `benchmarks/document_foundation/document_bench.cpp`, captured before commit `9cd8fd5` added file-content fingerprint verification. They compare flat prototype buffers with the production indexed piece treap and preserve useful buffer-structure evidence, but the FileService open/save columns are not current performance. The benchmark record at the time did not include CPU model, physical RAM, display, or storage device; the current host details above must not be retroactively treated as verified run metadata. Fixtures were deterministic ASCII source-like data with 61-byte lines and a final fill; file reads followed fixture creation with warm OS caches. Values below are medians of five runs. Per-operation p95 is computed from deterministic operations within a run.

| Fixture | Indexed buffer build | Estimated indexed storage after edits | Historical FileService open | Historical atomic Save As | Random insert p95 | Random delete p95 | 64 KiB range read p95 | Line lookup p95 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 MiB | 3.057 ms | 1.964 MiB | 16.601 ms | 4.824 ms | 0.003 ms | 0.004 ms | 0.008 ms | 0.001 ms |
| 10 MiB | 29.178 ms | 12.933 MiB | 64.442 ms | 13.354 ms | 0.004 ms | 0.004 ms | 0.005 ms | 0.001 ms |
| 50 MiB | 145.309 ms | 59.683 MiB | 288.714 ms | 63.826 ms | 0.005 ms | 0.005 ms | 0.004 ms | 0.001 ms |
| 100 MiB | 288.446 ms | 118.683 MiB | 564.750 ms | 108.128 ms | 0.005 ms | 0.005 ms | 0.005 ms | 0.001 ms |

Five fresh-process Release runs of the current FileService benchmark produced the following file-operation times. Fixtures are deterministic 61-byte-line ASCII text and are written before each open, so the file reads are warm-cache. `Atomic Save As` writes to a new path; it does not measure saving over an existing fingerprinted file. Values are milliseconds; five-run medians and ranges are given rather than an unsupported p95.

| Size | FileService open runs | Median (range) | Atomic Save As runs | Median (range) |
|---:|---|---:|---|---:|
| 1 MiB | 26.748, 28.835, 30.981, 28.934, 27.436 | 28.835 (26.748–30.981) | 12.761, 9.596, 13.158, 19.196, 8.855 | 12.761 (8.855–19.196) |
| 10 MiB | 177.168, 174.701, 174.234, 173.933, 178.206 | 174.701 (173.933–178.206) | 68.940, 71.398, 67.818, 76.256, 67.079 | 68.940 (67.079–76.256) |
| 50 MiB | 856.942, 870.853, 847.678, 845.550, 867.033 | 856.942 (845.550–870.853) | 321.869, 331.301, 312.254, 318.760, 320.067 | 320.067 (312.254–331.301) |
| 100 MiB | 1704.046, 1693.432, 1721.787, 1712.542, 1701.227 | 1704.046 (1693.432–1721.787) | 645.998, 629.465, 633.423, 647.480, 626.445 | 633.423 (626.445–647.480) |

Each run included 200 sequential character insertions and removal, 200 local insert/delete pairs, a 256 KiB paste/delete, 1,000 random insertions followed by 1,000 independent random deletions, 100 range reads, and (for the production buffer) 1,000 indexed line lookups. Reported storage is the buffer's payload estimate, including source capacities, newline offsets/tags, and piece-node sizes; it excludes allocator overhead and is not process RSS. The two prototype candidates expose no indexed line lookup, so that comparison is production-only.

| Fixture | Candidate | Build | Estimated storage | 256 KiB paste/delete | Random insert p95 | Random delete p95 | 64 KiB read p95 |
|---:|---|---:|---:|---:|---:|---:|---:|
| 50 MiB | Flat gap | 12.152 ms | 100.008 MiB | 22.152 ms | 2.672 ms | 2.598 ms | 0.124 ms |
| 50 MiB | Flat piece table | 6.369 ms | 50.849 MiB | 0.065 ms | 0.003 ms | 0.009 ms | 0.006 ms |
| 50 MiB | Indexed piece treap | 145.309 ms | 59.683 MiB | 0.691 ms | 0.005 ms | 0.005 ms | 0.004 ms |
| 100 MiB | Flat gap | 23.980 ms | 200.008 MiB | 43.407 ms | 5.249 ms | 5.228 ms | 0.134 ms |
| 100 MiB | Flat piece table | 12.712 ms | 101.631 MiB | 0.049 ms | 0.003 ms | 0.009 ms | 0.006 ms |
| 100 MiB | Indexed piece treap | 288.446 ms | 118.683 MiB | 0.764 ms | 0.005 ms | 0.005 ms | 0.005 ms |

The gap buffer performs well for local edits but moves large spans for distant edits. The flat piece table has the lowest measured build cost and storage, and very fast random edits in this 1,000-edit workload, but has no scalable piece/line index and its prototype operations scan descriptors. The indexed treap pays for UTF-8 validation, normalization, newline metadata, and indexed structure construction; in exchange it exposes logarithmic expected piece/line queries and keeps random edit costs low under the measured fragmentation. This evidence supports the current provisional production choice, not a proven advantage over a balanced AVL/B-tree sequence or a guarantee under adversarial fragmentation.

An additional five-run 8 MiB single-line fixture checks behavior when there are no line breaks. Median build times were 2.123 ms for the gap buffer, 1.068 ms for the flat piece table, and 22.539 ms for the indexed treap. The treap reported one line, 8.683 MiB estimated storage, 0.001 ms p95 for its 1,000 random inserts and deletes, and 0.005 ms p95 for 64 KiB range reads. This fixture measures buffer/index behavior only; it does not measure shaping, painting, or interactive navigation across an extreme line.

The current FileService open/save path is measured above. Current opening uses bounded chunk reads on a background worker, hashes content while reading, then re-reads and hashes the file after indexing to verify consistency. External-change inspections run in the workflow worker pool, while saving an existing document performs synchronous fingerprint checks and may delay the calling UI path for large files; the new-path Save As timings above do not quantify that existing-file save path. A small race remains between the final expected-fingerprint read and atomic replacement because this portable API does not provide a compare-and-swap replace operation. None of these measurements establish key-to-screen latency, real-display scrolling, the reference-device budget, or HDD/cross-platform behavior.

## Phase 04 editor presentation benchmark

`benchmarks/editor_presentation/editor_presentation_bench.cpp` renders a deterministic 4,600,000-byte fixture containing 100,000 repeated short lines (100,001 logical lines including the trailing empty line). It enables indentation guides, visible whitespace, matching occurrences, and wrapping, warms up for 10 frames, then measures 100 offscreen viewport renders into a reused image. `lastPaintedVisualRowCount()` reports row work in the final frame. Build with `cmake -S . -B build-presentation-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`, then build target `note_editor_presentation_bench`; run with `QT_QPA_PLATFORM=offscreen`.

Five fresh-process Release runs on 2026-11-19 used a worktree based on commit `9432b4c` with the Phase 04 renderer changes. The host was Windows NT 10.0.26300.0 with MinGW-w64 GCC 15.2.0, Qt 6.9.1, and Ninja; CPU model and physical RAM were not recorded for those runs. Each run painted 56 visual rows per frame. The median of run medians was **6.89 ms** and the median of run-level p95 values was **7.22 ms** per offscreen viewport paint. The host display/GPU is not represented by the offscreen plugin, no frame pacing or physical input-to-photon latency was measured, and the result is not comparable to the product target. The fixture uses short lines; it does not measure long-line layout degradation or memory retained by a real display session. This is diagnostic evidence only and does not establish the provisional latency target or low-end performance.
