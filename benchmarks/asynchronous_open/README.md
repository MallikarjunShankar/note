# Asynchronous open benchmark

Build a Release tree with testing enabled, then run `build/note_async_open_bench.exe`.

The harness creates a deterministic 50 MiB UTF-8 fixture with an 80-byte line cadence, opens it through `DocumentWorkflow::openAsync`, records time to first byte-progress callback and complete document publication, and runs a 1 ms `QCoreApplication` timer to observe event-loop heartbeat count and maximum gap. The fixture is written immediately before the run, so the read is warm-cache. This measures the core event loop only; it does not measure QWidget paint latency, input-to-photon, or process memory. Repeat in fresh processes and report every run; do not treat one result as a product budget pass.
