# Isolated editor architecture prototype

This experiment is separate from the production `src/` tree. It contains a GUI-independent buffer benchmark and, when Qt 6 Widgets is available, a small custom-painted editor viewport. It is not production editor code.

## Build

```powershell
cmake -S benchmarks/editor_prototype -B build/editor-prototype -G Ninja
cmake --build build/editor-prototype --target buffer_bench
```

If Qt 6 Widgets is found, CMake also creates `editor_widget_prototype`. The widget target is optional so the buffer implementations can still be compiled and evaluated when the GUI development package is unavailable.

## Run

```powershell
build/editor-prototype/buffer_bench.exe --self-test
build/editor-prototype/editor_widget_prototype.exe --self-test build/editor-prototype/widget-test.log
build/editor-prototype/buffer_bench.exe
build/editor-prototype/buffer_bench.exe path\to\fixture.txt
```

Without an input path, the benchmark creates an exact 50 MiB deterministic text fixture in the operating system's temporary directory. It compares a gap buffer and flat piece table under chunked load, local insert/delete, sequential typing, random persistent insertions, and fixed-size viewport reads.

The widget app accepts a text-file path and progressively reads it in 256 KiB chunks while servicing the Qt event loop. It implements only basic text display, caret movement, selection, mouse positioning, wheel scrolling, insertion and deletion. It intentionally lacks production-grade Unicode cursor indexing, undo, wrapping, syntax highlighting, and file management. Its line-start vector shifts offsets after edits in O(line-count) time; production may need a tree-based line index.

The widget self-test requires an offscreen Qt platform and a log path. Set `QT_QPA_PLATFORM=offscreen` and `QT_PLUGIN_PATH` to the Qt installation's plugin directory before running it.

The widget prototype must not be used as the production editor or as evidence for a performance budget until it builds and the Qt measurements are run on the named reference device.
