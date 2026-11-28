# Building and releasing on Windows

## Current release

The repository version is recorded in `VERSION`. The current version is **1.0.0**.

The Windows distribution includes two packages:

- **Portable ZIP:** extract and run `note.exe`.
- **Windows installer:** provides per-user or all-users installation, Start Menu shortcut, optional Desktop shortcut, and an uninstaller.

The release workflow creates downloadable artifacts. It does not publish a GitHub Release, create a tag, or push.

To publish a release:

1. Run the Release package workflow successfully.
2. Download its `note-windows-x64` artifact.
3. Verify the ZIP and installer against their respective SHA-256 checksums.
4. Open the repository's GitHub Releases page and choose **Draft a new release**.
5. Create a tag matching the version in `VERSION`, targeting `master`.
6. Use the title `NOTE <version>`.
7. Attach the ZIP, installer, and both checksum files.
8. Include the generated release notes.
9. Publish the release.

Verify that the tag, package filenames, executable version, and release title agree.

## Toolchain

NOTE is built with [MSYS2](https://www.msys2.org/) in its UCRT64 environment. Install MSYS2, then, in an MSYS2 shell:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base
```

`qt6-base` provides Qt and `windeployqt6`. `gcc` brings `binutils` (`objdump`, used to find DLL dependencies).

You also need:

- Git for Windows on `PATH`.
- Windows PowerShell 5.1 or later.
- Inno Setup 7, including `ISCC.exe`, to build the Windows installer.

The release script searches `PATH` and standard Inno Setup installation locations. Use `-InnoSetupCompiler <path>` if it is installed elsewhere.

| Tool | Required | Last release build used |
|---|---|---|
| GCC (UCRT64) | C++20 | 16.2.0 (`mingw-w64-ucrt-x86_64-gcc 16.2.0-7`) |
| CMake | 3.24 or later | 4.5.1 |
| Ninja | any | 1.13.2 |
| Qt | 6.5 or later | 6.12.0 (`qt6-base 6.12.0-2`) |
| Windows PowerShell | 5.1 or later | 5.1.26100 |
| Inno Setup | 7 | 7.1.1 |

MSYS2 packages roll forward. Older versions than those in the last column are untested.

## Version

`VERSION` holds the only application version number, for example `1.0.0`. CMake reads it for the project version, the `note.exe` version resource, and `RELEASE_NOTES.md` (from `packaging/RELEASE_NOTES.md.in`).

The release script uses it for the package names and installer version. To release a new version, update `VERSION` and the release notes template, then commit.

## Release package

Run from the repository with a clean working tree and no NOTE instance running:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\release.ps1
```

Options:

- `-Msys2Root <folder>`: MSYS2 installation folder if it is not detected automatically.
- `-InnoSetupCompiler <path>`: explicit path to `ISCC.exe` if it is not found automatically.
- `-OutputRoot <folder>`: output directory. Defaults to `note-release` beside the repository and must be outside it.
- `-Jobs <n>`: parallel build jobs. Defaults to 2.

The script stops with exit code 1 on the first failure. It:

1. Checks `VERSION`, the toolchain, and Inno Setup, then refuses to proceed with a dirty Git tree.
2. Configures and builds a clean Release build with tests enabled, then runs every test.
3. Runs `cmake --install`: `windeployqt6` copies Qt and its `platforms` and `styles` plugins, and `cmake/deploy_windows.cmake` adds the toolchain DLLs imported by `note.exe` and its plugins.
4. Checks that `note.exe` reports the version in `VERSION`.
5. Copies the licence files of every MSYS2 package that owns a bundled DLL into `licenses\`, together with `THIRD-PARTY-NOTICES.txt`.
6. Creates the portable ZIP and its SHA-256 checksum.
7. Compiles the Windows installer using `packaging/Note.iss` and creates its SHA-256 checksum.
8. Extracts the ZIP and starts `note.exe` with only Windows folders on `PATH`. The check requires the main window to appear, rejects DLLs loaded from MSYS2, and closes the application. During the check, `%APPDATA%\Note` is renamed to `Note.release-check-parked` and restored afterwards. If a run is interrupted, rename it back manually.

### Output

The output directory contains:

- `Note-<version>-windows-x64.zip`: portable application package.
- `Note-<version>-windows-x64.zip.sha256`: ZIP checksum.
- `Note-<version>-windows-x64-Setup.exe`: Windows installer.
- `Note-<version>-windows-x64-Setup.exe.sha256`: installer checksum.
- `Note-<version>-RELEASE_NOTES.md`: release notes.

The portable ZIP contains a `Note-<version>` folder with `note.exe`, required DLLs and Qt plugins, `LICENSE.txt`, `RELEASE_NOTES.md`, `THIRD-PARTY-NOTICES.txt`, and `licenses\`.

The installer supports per-user and all-users installation. It creates a Start Menu shortcut, offers an optional Desktop shortcut, and offers to launch NOTE after installation. It includes an uninstaller.

Uninstalling NOTE does not intentionally remove user documents, settings, session state, or recovery data stored outside the installation directory.

`work\<version>\` contains the build tree, staging folder, and launch-check copy. It is deleted at the start of the next run for the same version.

A clean build with two jobs takes roughly 15 minutes on a 4 GB laptop. Actual time depends on the machine and build environment.

## Continuous integration

- `.github/workflows/ci.yml` runs on every push and pull request. It performs a Debug build and runs the tests on `windows-latest` with MSYS2 UCRT64.
- `.github/workflows/release.yml` runs only when manually started through Actions > Release package > Run workflow.
- The release workflow installs Inno Setup, runs `scripts/release.ps1`, and uploads the ZIP, installer, checksums, and release notes as the `note-windows-x64` artifact.

The workflow does not publish a GitHub Release, create a tag, or push changes.