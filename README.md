# NOTE

**A lightweight, minimal, and highly customisable code editor.**

NOTE is an open-source code editor focused on simplicity, performance, and customisation. It aims to provide an uncluttered coding experience while retaining the essential tools developers need.

The project is being built with a modular architecture, making it possible to extend and improve individual systems without compromising the overall application.

> **Status: Early development**
>
> NOTE is actively being developed. It is not yet ready for everyday use. Features, interfaces, and internal architecture may change as development progresses.

## Vision

NOTE aims to provide:

- **Lightweight performance** — fast startup, responsive editing, and efficient resource usage.
- **Minimalism** — a clean interface without unnecessary IDE complexity.
- **Customisation** — extensive control over editor appearance, themes, typography, and behaviour.
- **Essential coding conveniences** — practical editing features without turning the application into a full IDE.
- **Extensibility** — modular systems that can evolve independently over time.

## Technology

NOTE is built using:

- **C++20**
- **Qt 6 Widgets**
- **CMake**

The application is designed as a native desktop application without an embedded browser runtime.

## Project Status

NOTE is under active development.

Current development priorities include:

- Core document and file management.
- Production text editing engine.
- Cursor and selection management.
- Syntax highlighting.
- Essential coding conveniences.
- Theme and customisation system.
- Command palette and keyboard shortcuts.
- Session restoration and recovery.

These are development goals, not a claim that all features are currently implemented.

## Building from Source

### Requirements

- A C++20-compatible compiler.
- CMake.
- Qt 6 with Widgets.
- A supported build system, such as Ninja or Make.

### Build

Clone the repository:

```bash
git clone https://github.com/MallikarjunShankar/note.git
cd note
```

Configure the project:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build --config Release
```

Run the resulting executable from the build output directory.

**Note:** Build configuration may require your Qt 6 installation to be discoverable by CMake.

## Contributing

Contributions are welcome.

NOTE is being developed with a focus on maintainability, modularity, and performance. Contributions should respect the project's architecture and established development conventions.

Before contributing:

1. Check existing issues and discussions.
2. Discuss substantial changes before implementation.
3. Keep changes focused and independently reviewable.
4. Include relevant tests.
5. Ensure the project builds successfully.

More detailed contribution guidelines will be added as the project develops.

## Project Philosophy

NOTE is intended to be a code editor, not an IDE.

The project prioritises:

- Simplicity over unnecessary complexity.
- Performance over excessive features.
- Customisation without architectural bloat.
- Clear module boundaries.
- Long-term maintainability.

## License

NOTE is licensed under the [MIT License](LICENSE).

You are free to use, modify, distribute, and build upon the project under the terms of that license.

## Repository

GitHub: https://github.com/MallikarjunShankar/note