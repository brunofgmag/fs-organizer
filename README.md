# FS Organizer

An addon manager for Microsoft Flight Simulator on Windows. Your addons live in a
folder of your own, outside the simulator, and turning one on or off creates or
removes an NTFS junction. No files move during normal use, so swapping tens of
gigabytes of content is instant.

Built for MSFS 2024, and works with 2020 through the same mechanism.

Early development. There is no usable release yet.

## How it works

The real files live in a library, a folder you choose and organise into
categories. Enabling an addon creates a link inside one of the simulator's
destination folders pointing at the library; disabling removes that link and
nothing else. Whether an addon is enabled is never stored anywhere: it is read
back from disk on every scan, so the app and the simulator cannot disagree.

## Project status

The link engine and its destructive guards are covered by tests. State detection,
the addon tree and the interface are not built yet.

The specification, the architecture decisions and the development plan are kept
outside this repository.

## Building

Requires Windows 10 or 11 x64, Visual Studio 2022 and Qt 6.8 or newer
(`msvc2022_64` kit).

```powershell
./build.ps1 -Config Release -RunTests
```

The script finds Qt under `C:\Qt` on its own. Set `QT_ROOT_DIR` first to point it
at a different kit.

To run the tests alone, optionally filtered by name:

```powershell
./run-tests.ps1 -Filter linking-engine
```

The domain layer is plain C++ with no Qt and no Win32, so it builds anywhere for
testing:

```bash
cmake -S . -B build/tests-only -DFSORG_TESTS_ONLY=ON
cmake --build build/tests-only
ctest --test-dir build/tests-only --output-on-failure
```

## Contributing

Commits follow [Conventional Commits](https://www.conventionalcommits.org), and
the hook in `.githooks/commit-msg` rejects anything else. The pre-commit hook
builds and runs the tests before letting a code change through. Both install
themselves the first time CMake configures the project.

Code, commits and documentation are written in English. Only the user interface
is translated, currently into English and Brazilian Portuguese.

## Licence

GPL v2. See [LICENSE](LICENSE).
