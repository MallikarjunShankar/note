# Production document foundation benchmarks

This benchmark compares the isolated prototype gap buffer and flat piece table with the production indexed piece treap. It also measures production File Service open and atomic Save As. It is a diagnostic benchmark, not an application responsiveness or low-end acceptance test. Results and caveats are recorded in [`docs/PERFORMANCE.md`](../../docs/PERFORMANCE.md#phase-02-document-foundation-benchmarks).

## Build and run

From the repository root:

```powershell
cmake -S . -B build/phase02 -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/phase02 --parallel
ctest --test-dir build/phase02 --output-on-failure
build/phase02/note_document_bench.exe 1 10 50 100
```

Arguments are fixture sizes in MiB. Without arguments, the executable runs 1, 10, 50, and 100 MiB, then compares all three buffers on an 8 MiB single-line fixture to exercise long-line behavior. Each run uses a fixed ASCII fixture/seed and reports construction, edit, range retrieval, line lookup, estimated structure storage, file open, and atomic save timings. Run the executable at least five times when producing comparative results; record the environment, build flags, file-cache state, and medians. The production component's storage estimate omits allocator overhead and process memory.

The benchmark deliberately does not create or implement a UI, cursor manager, tab system, syntax highlighter, background loader, or production editor behavior.
