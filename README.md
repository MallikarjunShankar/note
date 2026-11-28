# NOTE

**A lightweight, minimal, reliable, and highly customisable text editor.**

NOTE is an open-source desktop text editor focused on simplicity, performance, reliability, and customisation. It provides a focused editing environment without the complexity of an IDE.

**Current release:** 1.0.1 (Windows x64).

The initial distribution target is Windows. The application is built with C++20, Qt 6 Widgets, and CMake.

## Features

- Plain-text editing with multiple tabs and multiple cursors.
- Undo and redo, smart indentation, and bracket/quote pairing.
- Find and replace with regular-expression, case-sensitive, and whole-word options.
- Custom themes, including a visual theme editor.
- Configurable typography, layout, editing preferences, and keyboard shortcuts.
- Command palette, menus, and context menus.
- Session restoration and crash recovery for unsaved documents.
- UTF-8 and supported encoding/line-ending preservation.
- Atomic file saving and external-modification detection.
- Opening files in external applications and terminals.

NOTE is a text editor, not an IDE. It does not include an embedded terminal, project explorer, autocomplete, diagnostics, build system, or integrated Git interface.

Syntax highlighting and language detection are deferred until after V1.

## Download

Windows x64 release packages are distributed through:

[NOTE GitHub Releases](https://github.com/MallikarjunShankar/note/releases)

For the current release candidate, use the matching Windows x64 package and SHA-256 checksum from the release or workflow artifact.

The Windows installer provides a standard installation. The portable ZIP remains available for users who prefer not to install NOTE.

### Portable usage

1. Download the Windows x64 ZIP.
2. Verify its SHA-256 checksum.
3. Extract the archive.
4. Run `note.exe`.

The package does not require a separate installer.

User settings, themes, session state, and recovery data are stored under %APPDATA%\Note.

## Building from source

### Requirements

- C++20-compatible compiler.
- CMake 3.24 or later.
- Qt 6.5 or later with Widgets.
- Ninja or another supported build system.

### Build

```powershell
git clone https://github.com/MallikarjunShankar/note.git
cd note

cmake -S . -B build
cmake --build build --config Release
```

Run tests:

```powershell
ctest --test-dir build --output-on-failure
```

For the documented Windows release toolchain, packaging, and CI workflow, see [docs/RELEASE.md](docs/RELEASE.md).

## Project status

The integrated Windows release candidate has been built and manually tested as a packaged application.

The project is still subject to outstanding validation, including physical low-end hardware, real-display performance, accessibility, and Linux/macOS runtime behavior. See [docs/STATUS.md](docs/STATUS.md) for the implementation ledger and [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for measured performance and limitations.

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md), [PROJECT_SPEC.md](PROJECT_SPEC.md), and [AGENTS.md](AGENTS.md) before making substantial changes.

Contributions should preserve NOTE's lightweight architecture, reliability, performance, and customisation goals.

## Project principles

- Simplicity over unnecessary complexity.
- Performance over excessive features.
- Customisation without architectural bloat.
- Clear module boundaries.
- Long-term maintainability.

## License

NOTE is distributed under the [MIT License](LICENSE).

Bundled third-party libraries retain their respective licenses.

## Repository

[GitHub: MallikarjunShankar/note](https://github.com/MallikarjunShankar/note)
