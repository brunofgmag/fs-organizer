# Contributing to FS Organizer

This is for people who want to build the app or change it. The
[README](README.md) is the one that says what it does.

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

The domain, application and viewmodel layers include neither Qt nor Win32
headers, and the `no-win32-in-the-core` test is what keeps it that way. It runs
with the rest of the suite. There is one build configuration, so those tests are
built and run along with everything else.

The source list lives in `cmake/`, not in globs. Adding or removing a file means
editing a `.cmake` there, and a build directory configured before that edit will
not know the file exists. `build.ps1` reconfigures the directory it builds, but
`cmake --build <dir>` on its own does not, so the other directories keep linking
against a stale project and fail with `LNK2019` on a symbol whose source is
sitting right there. Reconfigure each one you still use:

```powershell
cmake -S . -B build/debug
```

`build.ps1` warns when it finds a build directory older than `cmake/`.

## Commits and language

Commits follow [Conventional Commits](https://www.conventionalcommits.org), and
the hook in `.githooks/commit-msg` rejects anything else. The pre-commit hook
builds and runs the tests before letting a code change through. Both install
themselves the first time CMake configures the project.

Code, commits and documentation are written in English. The user interface is
the exception: its phrases are written in English and translated through
`i18n/app_en.ts` and `i18n/app_pt_BR.ts`.

## Development tools

Nine programs under `tools/` build alongside the app and answer questions the
test suite cannot. None of them ship. Only `fsorg-delete` deletes anything,
and only when you pass `--go`. Ask any of them for `--help`, which is the copy
that stays current.

None of them run without Qt on the `PATH`, and the build does not put it there.
The binary comes out fine and then dies on a missing DLL, or prints nothing at
all:

```powershell
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
```

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
for the two cases that got slow before. The suite counts how many times the paint
asks the font, which is not the same as time. This is the only place milliseconds
are measured, and it is run by hand.

`fsorg-bgl` answers the airport code of a scenery, read from inside the BGL and
never from the folder name. Given `--library` it walks a library root through
the same scan the app uses and answers per addon, separating an addon that
carries no airport record from one whose record was there and did not decode,
and it prints the groups of addons that share a code. Given a file or a folder
it answers per BGL instead.

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

`fsorg-docs` indexes the PDFs of every addon and separates chart from document,
through the same application service the documentation panel uses. It measures the
`chart_id` match from both sides: how many catalogue entries met a file, and how
many files no entry names. The last line says how long it took to read the chart
version out of each PDF title.

## Packaging

`build.ps1` deploys the Qt runtime, prunes what the app does not load, copies the
MSVC runtime beside the executable and then refuses the result if anything is
missing or if anything pruned came back. The three scripts behind that are
`tools/prune-deployment.ps1`, `tools/deploy-msvc-runtime.ps1` and
`tools/verify-deployment.ps1`, and the last one runs on its own too:

```powershell
./tools/verify-deployment.ps1 -DeploymentDir build/release/bin -Configuration Release
```

A Release package is 23 files and about 33 MiB. `.github/workflows/deploy.yml`
builds it on `main`, hashes it and publishes the zip with its `.sha256` to a
GitHub Release.
