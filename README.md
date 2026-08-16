<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/branding/logo-dark.svg">
    <img alt="FS Organizer" src="assets/branding/logo-light.svg" width="420">
  </picture>
</div>

# FS Organizer

An addon manager for Microsoft Flight Simulator on Windows. Your addons live in a
folder of your own, outside the simulator, and turning one on or off creates or
removes an NTFS junction. No files move during normal use, so switching tens of
gigabytes of content takes a moment.

Built for MSFS 2024, and works with 2020 through the same mechanism.

## Download

Take the latest zip from
[Releases](https://github.com/brunofgmag/fs-organizer/releases), unpack it
anywhere and run `fs-organizer.exe`. There is no installer and nothing else to
install. The app can watch for new versions and apply one on its way out.

## How it works

The real files live in a library, a folder you choose and organise into
categories. Enabling an addon creates a link inside one of the simulator's
destination folders pointing at the library; disabling removes that link and
nothing else. Whether an addon is enabled is never stored anywhere: it is read
back from disk on every scan, so the app and the simulator cannot disagree.

If you already use MSFS Addons Linker, its configuration can be read and
proposed as libraries and categories for you to confirm, and its `.preset` files
come across with it.

## What it does

**Library** is where you spend the time. Your addons appear by category, a tick
turns one on and clearing it turns the addon off, you can drag them between
categories, and the ones you no longer want go to the Recycle Bin or straight
out.

**Destinations** shows the simulator's own folders and says where each entry came
from: what the app manages, what some other program left there, and what points
nowhere. Loose folders can be imported into the library, and dead links
repaired.

**Simulator** lists what starts along with the simulator, from its `EXE.xml`, and
the packages it already carries.

**Presets** are named sets of addons. Before you apply one it says what it would
turn on, what it would turn off, and what is already as it asks.

**Documents** collects the PDFs your addons ship. Manuals stay apart from charts,
and each one reopens on the page you stopped at.

**Quarantine** keeps the copy that lost a conflict until you are sure, so it can
still go back.

**Diagnostics** answers for the disk. How much space each category and each addon
takes, what the last scan ran into, which dependencies a manifest declares, which
airports more than one scenery covers, and a guided search for the addon that
brings the simulator down.

**Journal** records everything that wrote, and undoes the batch it wrote.

The interface speaks English and Brazilian Portuguese, follows the Windows light
or dark theme, and changes language without a restart.

## Manual

The user manual is in [`docs/`](docs/), in
[English](docs/fs-organizer-en.pdf) and
[Brazilian Portuguese](docs/fs-organizer-pt_BR.pdf). Presets come first in both,
because that is the part of the program nobody gets right by trial.

## Contributing

Building it, running the suite and the packaging are all in
[CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

GPL v2. See [LICENSE](LICENSE).

The interface is set in [Archivo](https://github.com/Omnibus-Type/Archivo),
embedded under the SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
