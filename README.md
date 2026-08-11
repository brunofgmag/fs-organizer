<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/branding/logo-dark.svg">
    <img alt="FS Organizer" src="assets/branding/logo-light.svg" width="420">
  </picture>
</div>

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

The core works end to end: the addon tree turns addons on and off through real
junctions, the Community screen classifies and repairs entries, importing moves
physical folders into the library, and presets capture and apply named sets. The
interface has its own visual identity, following the Windows light or dark theme.
An options screen behind the header gear covers profiles, destinations and
libraries without reopening the first-run wizard, and chooses between directory
junction and symbolic link. A configuration saved by MSFS Addons Linker can be
read and proposed as libraries and categories for you to confirm, and its
`.preset` files come across with it. The app checks GitHub Releases in three
modes and applies an update on the way out, only a single copy of it runs at a
time, and `build.ps1` produces a verified package that runs without Qt
installed.

Past that first release the app learned to answer for what is on disk and to
touch it. A diagnostics screen reports what the scan found, sums the disk each
category and each addon occupies, and reads the declared dependencies of a
manifest against the packages the simulator actually has. Addons can be deleted
from the library by the Recycle Bin or for good, with the route refused up front
and with a reason when it will not work. An addon another program installed can
be taken over into the library and given back to that program later, and when
the two copies disagree the loser goes to a quarantine you can restore from.
Every operation that writes lands in a journal that can undo the batch it wrote.

The app also reads and writes the `EXE.xml` of the simulator, which is the file
that decides what starts with it. The Simulator tab lists those entries and
turns them on and off, disabling an addon that carries a live entry warns before
it does both as one operation, and a preset says whether it is already satisfied
and what it would change before you apply it. It writes a return preset first
and refuses the whole application if it cannot.

The interface speaks English and Brazilian Portuguese, and the Language tab
switches between them without a restart. Both catalogues ship inside the
executable.

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

The source list lives in `cmake/`, not in globs. Adding or removing a file means
editing a `.cmake` there, and a build directory configured before that edit will
not know the file exists. `build.ps1` reconfigures the directory it builds, but
`cmake --build <dir>` on its own does not, so the other directories keep linking
against a stale project and fail with `LNK2019` on a symbol whose source is
sitting right there. Reconfigure each one you still use:

```powershell
cmake -S . -B build/debug
cmake -S . -B build/tests-only -DFSORG_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
```

`build.ps1` warns when it finds a build directory older than `cmake/`.

## Contributing

Commits follow [Conventional Commits](https://www.conventionalcommits.org), and
the hook in `.githooks/commit-msg` rejects anything else. The pre-commit hook
builds and runs the tests before letting a code change through. Both install
themselves the first time CMake configures the project.

Code, commits and documentation are written in English. The user interface is
the exception: its phrases are written in English and translated through
`i18n/app_en.ts` and `i18n/app_pt_BR.ts`.

### Development tools

Seven programs under `tools/` build alongside the app and answer questions the
test suite cannot. None of them ship. Six only read. The seventh, `fsorg-delete`,
can delete for real, and only when you pass `--go`. Ask any of them for `--help`,
which is the copy that stays current.

`fsorg-probe` reads a real simulator setup and prints what the app would see
from it: how many entries each destination holds, how each one is classified,
and whether the simulator is running.

```powershell
cmake --build build/release --config Release --target fsorg-probe
./build/release/Release/fsorg-probe.exe --library "D:\MSFS 2024"
```

`fsorg-shot` builds the real widgets against your real configuration and writes
a PNG of every screen, so you can look at a change without driving the app by
hand. It takes `--out` for the folder, `--theme` for `dark`, `light` or
`system`, `--size` as `WIDTHxHEIGHT`, and `--lang` for `en` or `pt_BR`.
`--select` takes the folder name of an addon and selects it before the shots
that carry a context panel. `--batch` takes a row count and selects that many
top rows instead, which is the only way to get the batch panel into a picture,
and it wins over `--select`. Run it with `QT_SCALE_FACTOR=1.25` to match Windows
scaling.

```powershell
cmake --build build/debug --target fsorg-shot
./build/debug/Debug/fsorg-shot.exe --out shots --theme dark --lang pt_BR
```

`fsorg-timing` measures paint cost, with `--journal-scroll` and `--app-journal`
for the two cases that got slow before. Nothing in the test suite guards paint
cost, so this is the only guard there is, and it is run by hand.

`fsorg-packages` finds the package list of every profile the way the app finds
it, says when each list was written and how much it carries, and answers
`--package` for a name you give it. Ask by the bare name: the generation
prefixes belong to the adapter, not to the caller.

`fsorg-size` sums the disk each category and each addon occupies, against the
real installation and through the same application service the diagnostics
screen uses. Progress advances on one line while it walks, and Ctrl+C stops it,
and what comes out is reported as incomplete rather than passed off as a total.
`--stop-after <n>` takes that same cancellation path without anyone pressing
anything.

`fsorg-delete` deletes addons through the same application service the Library
screen uses, by the Recycle Bin or for good. Without `--go` it plans, prints and
writes nothing, which is how you read the guards before they have a button. It
reads your real `settings.json` and never saves it back.

`fsorg-startup` finds the `EXE.xml` of every profile the way the app finds it,
beside the `UserCfg.opt` and by the exact name, and lists the startup entries
with their switch and their target. It never writes, neither the file nor the
backup: that file belongs to the user, and the write is proved against a copy in
a temporary folder instead.

### Packaging

`build.ps1` deploys the Qt runtime, prunes what the app does not load, copies the
MSVC runtime beside the executable and then refuses the result if anything is
missing or if anything pruned came back. The three scripts behind that are
`tools/prune-deployment.ps1`, `tools/deploy-msvc-runtime.ps1` and
`tools/verify-deployment.ps1`, and the last one runs on its own too:

```powershell
./tools/verify-deployment.ps1 -DeploymentDir build/release/bin -Configuration Release
```

A Release package is 21 files and about 27 MiB. `.github/workflows/deploy.yml`
builds it on `main`, hashes it and publishes the zip with its `.sha256` to a
GitHub Release.

## Licence

GPL v2. See [LICENSE](LICENSE).

The interface is set in [Archivo](https://github.com/Omnibus-Type/Archivo),
embedded under the SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
