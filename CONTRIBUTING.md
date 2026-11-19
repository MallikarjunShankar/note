# Contributing to NOTE

Thank you for your interest in contributing to NOTE.

NOTE is a lightweight, minimalistic, customisable text editor. Contributions should help improve its performance, reliability, usability, and maintainability without compromising its core principles.

This document explains how to contribute to the project.

## 1. Before You Contribute

Before starting work:

1. Read [PROJECT_SPEC.md](PROJECT_SPEC.md) to understand the product requirements and project boundaries.
2. Read [AGENTS.md](AGENTS.md) for development instructions and repository-specific rules.
3. Review [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) to understand the application architecture.
4. Check [docs/STATUS.md](docs/STATUS.md) and [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md) for current development progress.

For substantial changes, open an issue or discussion before implementation.

Avoid duplicating existing work or introducing changes outside the project's intended scope.

## 2. What You Can Contribute

Contributions are welcome in the following areas:

- Performance optimisation.
- Bug fixes and reliability improvements.
- Editor functionality and usability.
- Rendering and input improvements.
- Memory and resource efficiency.
- Accessibility improvements.
- Documentation and developer experience.
- Automated tests and regression coverage.
- Windows distribution and release infrastructure.
- Cross-platform compatibility.

All contributions must remain consistent with the project specification.

### Out of Scope

NOTE is a text editor, not an IDE.

The following are outside the intended product scope:

- Embedded terminals.
- Project explorers.
- Code autocomplete and diagnostics.
- Integrated build systems.
- Built-in Git integration.
- Plugin systems.
- Embedded browser runtimes.

Do not introduce these features without an explicit change to the project specification.

## 3. Development Environment

NOTE uses the following technologies:

| Component | Technology |
|---|---|
| Language | C++20 |
| UI framework | Qt 6 Widgets |
| Build system | CMake |
| Testing | CTest |
| Version control | Git |

A compatible C++20 compiler, CMake installation, and Qt 6 development environment are required.

### Building the Project

Clone the repository:

```bash
git clone https://github.com/MallikarjunShankar/note.git
cd note
```

Configure:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

If your environment requires a specific CMake generator or Qt installation path, configure those according to your local setup.

## 4. Development Guidelines

All contributions must follow these principles.

### Architecture

- Preserve the separation between the framework-independent editor engine and the Qt UI layer.
- Keep core editing and document logic independent of the UI framework.
- Avoid unnecessary dependencies.
- Keep components focused and responsibilities clearly defined.
- Prefer existing project interfaces and abstractions over introducing competing implementations.

### Performance

Performance is a core requirement, not an optional improvement.

- Avoid unnecessary allocations and document copies.
- Avoid blocking the UI thread with expensive operations.
- Keep rendering limited to the necessary viewport.
- Consider memory usage when working with large documents.
- Measure performance changes rather than relying on assumptions.
- Never sacrifice correctness or data integrity for benchmark improvements.

### Correctness and Data Integrity

- Preserve UTF-8 and supported encoding behaviour.
- Preserve line-ending information.
- Maintain document integrity during editing and saving.
- Preserve undo/redo correctness.
- Handle errors explicitly.
- Avoid silent data loss.
- Add regression tests for bug fixes.

### Code Quality

- Write clear, maintainable C++20.
- Use descriptive names.
- Keep functions and classes focused.
- Avoid unnecessary abstraction and premature optimisation.
- Follow the conventions established in the surrounding code.
- Do not introduce unrelated refactoring into focused changes.

## 5. Testing Requirements

Every contribution must be tested appropriately.

Depending on the change, this may include:

- Unit tests.
- Integration tests.
- Regression tests.
- Document encoding and line-ending tests.
- Editing and undo/redo tests.
- File opening and saving tests.
- Performance benchmarks.
- Build verification.

Before submitting a pull request:

1. Build the project successfully.
2. Run the relevant tests.
3. Run the complete existing test suite.
4. Check for regressions.
5. Run `git diff --check`.

Performance-related contributions should include reproducible before-and-after measurements whenever possible.

Do not claim tests or benchmarks passed unless they were actually executed.

## 6. Submitting Changes

### Branches

Create a separate branch for your contribution.

```bash
git checkout master
git pull origin master
git checkout -b fix/short-description
```

Suggested branch naming:

- `feature/description`
- `fix/description`
- `performance/description`
- `docs/description`
- `test/description`

### Commits

Write clear, descriptive commit messages.

Examples:

```text
Fix cursor movement at document boundaries
Reduce large-document memory overhead
Improve viewport rendering performance
Add document encoding regression tests
Update development documentation
```

Keep commits focused on related changes.

### Pull Requests

Submit a pull request against the `master` branch.

A pull request should include:

- A concise description of the changes.
- The reason for the changes.
- Relevant issue references.
- Tests performed.
- Performance measurements, where applicable.
- Screenshots or recordings for relevant visual changes.
- Known limitations or outstanding concerns.

Keep pull requests focused and avoid combining unrelated changes.

## 7. Bug Reports

When reporting a bug, provide as much of the following information as possible:

- A clear description of the problem.
- Steps to reproduce it.
- Expected behaviour.
- Actual behaviour.
- Operating system and version.
- NOTE version or commit.
- Relevant logs or error messages.
- Sample files when necessary and safe to share.

For performance issues, include the document size, approximate reproduction steps, and available measurements.

## 8. Feature Requests

Feature requests should explain:

- The problem being addressed.
- Why the proposed functionality is useful.
- How it fits NOTE's intended purpose.
- Any relevant implementation considerations.

Feature requests are subject to the project's scope and design principles.

Not every suggested feature will necessarily be accepted.

## 9. Documentation

Documentation improvements are welcome.

Keep documentation consistent with the actual implementation.

When changing architecture, interfaces, development progress, or performance characteristics, update the relevant documentation.

Do not mark planned functionality as implemented or unverified performance targets as achieved.

## 10. Community Standards

Contributors are expected to:

- Be respectful and constructive.
- Discuss technical disagreements professionally.
- Provide useful and actionable feedback.
- Respect other contributors' time and work.
- Avoid harassment, discrimination, and personal attacks.

The goal is to maintain a welcoming and productive open-source community.

## 11. Licensing

NOTE is distributed under the MIT License.

By submitting a contribution, you agree that your contribution will be distributed under the project's MIT License, unless explicitly agreed otherwise.

You must have the legal right to submit the code, documentation, or other materials you contribute.

Do not submit proprietary code, copyrighted material without permission, or content whose licensing is incompatible with the project.

See [LICENSE](LICENSE) for the full license text.

## 12. Final Notes

NOTE prioritises simplicity, performance, reliability, and customisation.

Contributions should strengthen these principles rather than introduce unnecessary complexity.

Thank you for helping improve NOTE.