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

## Manual

The user manual is in [`docs/`](docs/), in
[English](docs/fs-organizer-en.pdf) and
[Brazilian Portuguese](docs/fs-organizer-pt_BR.pdf). Presets come first in both,
because that is the part of the program nobody gets right by trial. The LaTeX it
is built from lives in [`manual/`](manual/).

## Contributing

Building it, running the suite, the tools under `tools/` and the packaging
are all in [CONTRIBUTING.md](CONTRIBUTING.md). Commits follow Conventional
Commits, and a hook rejects anything else.

## Licence

GPL v2. See [LICENSE](LICENSE).

The interface is set in [Archivo](https://github.com/Omnibus-Type/Archivo),
embedded under the SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
