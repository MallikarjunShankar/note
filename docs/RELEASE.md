# Building and releasing on Windows

## Toolchain

Note is built with [MSYS2](https://www.msys2.org/) in its UCRT64 environment. Install MSYS2, then, in an MSYS2 shell:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base
```

`qt6-base` provides Qt and `windeployqt6`. `gcc` brings `binutils` (`objdump`, used to find DLL dependencies). You also need Git for Windows on `PATH` and Windows PowerShell 5.1 or later.

| Tool | Required | Last release build used |
|---|---|---|
| GCC (UCRT64) | C++20 | 16.2.0 (`mingw-w64-ucrt-x86_64-gcc 16.2.0-7`) |
| CMake | 3.24 or later (checked) | 4.5.1 |
| Ninja | any | 1.13.2 |
| Qt | 6.5 or later (checked) | 6.12.0 (`qt6-base 6.12.0-2`) |
| Windows PowerShell | 5.1 or later | 5.1.26100 |

MSYS2 packages roll forward. Older versions than those in the last column are untested.

## Version

`VERSION` holds the only version number, for example `1.0.0-rc.1`. CMake reads it for the project version, the `note.exe` version resource and `RELEASE_NOTES.md` (from `packaging/RELEASE_NOTES.md.in`). The release script uses it for the package name. To release a new version, edit `VERSION` and the release notes template, then commit.

## Release package

From the repository, with a clean working tree and no Note running:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\release.ps1
```

Options: `-Msys2Root <folder>` if MSYS2 is not found automatically (it reads the MSYS2 uninstall registry entry, then looks for a UCRT64 `g++` on `PATH`); `-OutputRoot <folder>` (default: `note-release` beside the repository; must be outside it); `-Jobs <n>` (default 2).

The script stops with exit code 1 on the first failure. In order, it:

1. Checks `VERSION` and the toolchain, and refuses a dirty Git tree.
2. Configures and builds a clean Release build with tests, then runs every test.
3. Runs `cmake --install`: `windeployqt6` copies Qt and its `platforms` and `styles` plugins, and `cmake/deploy_windows.cmake` adds every toolchain DLL that `note.exe` and the plugins import.
4. Checks that `note.exe` reports the `VERSION` value, and copies the licence files of every MSYS2 package that owns a bundled DLL into `licenses\`, with `THIRD-PARTY-NOTICES.txt`.
5. Writes the zip and its checksum.
6. Unzips the package and starts `note.exe` from it, with only Windows folders on `PATH`. The check requires the main window to appear, rejects any DLL loaded from MSYS2, and closes the app. During the check, `%APPDATA%\Note` is renamed to `Note.release-check-parked` and restored afterwards. If a run is interrupted, rename it back by hand.

Output in `OutputRoot`:

- `Note-<version>-windows-x64.zip`: a `Note-<version>` folder with `note.exe`, DLLs, `LICENSE.txt`, `RELEASE_NOTES.md`, `THIRD-PARTY-NOTICES.txt` and `licenses\`.
- `Note-<version>-windows-x64.zip.sha256`: `<sha256>  <zip name>`.
- `Note-<version>-RELEASE_NOTES.md`.
- `work\<version>\`: build tree, staging folder and launch-check copy. This is deleted at the start of the next run for the same version.

A clean build with two jobs takes roughly 15 minutes on a 4 GB laptop.

## Continuous integration

- `.github/workflows/ci.yml` runs on every push and pull request: a Debug build and all tests on `windows-latest` with MSYS2 UCRT64.
- `.github/workflows/release.yml` runs only when started by hand (Actions > Release package > Run workflow). It runs `scripts/release.ps1` and uploads the zip, checksum and release notes as a workflow artifact. It does not publish a release, create a tag or push.
