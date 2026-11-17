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

Use the primary development machine for routine development benchmarks and record its actual hardware before interpreting results. The minimum profile above is a compatibility target, not a recommended workstation specification; this project does not set a separate numeric recommended profile. The current managed host's CPU model and physical RAM were not exposed, so its benchmark results cannot be used to claim a known development hardware baseline.

The recorded editor prototype ran on Windows NT 10.0.26300.0, with 16 logical processors exposed, MSYS2 MinGW-w64 GCC 15.2.0, CMake 4.1.0/Ninja, and Qt 6.9.1. The managed host did not expose its CPU model or physical RAM. Prototype GUI measurements used Qt's offscreen plugin. This is measurement environment metadata; it is not the minimum profile and must not be described as a representative development-machine hardware specification. See [`EDITOR_PROTOTYPE.md`](EDITOR_PROTOTYPE.md) for the full setup and results.

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

Physical low-end hardware, true cold startup, real-display frame pacing, production behavior, 100 MB scenarios, tab-count stress, long-line thresholds, and cross-platform release builds remain outstanding.
