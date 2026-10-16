<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/branding/logo-dark.svg">
    <img alt="FS Organizer" src="assets/branding/logo-light.svg" width="420">
  </picture>
</div>

# FS Organizer

An addon manager for Microsoft Flight Simulator on Windows and Linux. Your addons
live in a folder of your own, outside the simulator, and turning one on or off
creates or removes a link: an NTFS junction on Windows, a symbolic link on Linux.
No files move during normal use, so switching tens of gigabytes of content takes
a moment.

Built for MSFS 2024, and works with 2020 through the same mechanism.

## Download

Take the latest zip from
[Releases](https://github.com/brunofgmag/fs-organizer/releases), unpack it
anywhere and run `fs-organizer.exe`. There is no installer and nothing else to
install. The app can check for new versions and install one when you close it.

On Linux, take `fs-organizer-<version>-x86_64.AppImage` from the same page, make it
executable and run it. The `.sha256` file beside it checks the download. When the
folder the AppImage sits in is writable, the app downloads a new version, checks
it against its checksum and replaces itself the next time you close it. In any
other case it only tells you that a new version exists. Each time it starts, it
adds itself and its icon to your desktop's application menu.

## Linux

FS Organizer runs natively on Linux and works with MSFS 2020 and MSFS 2024 run
by Steam through Proton. It looks for Steam in `~/.local/share/Steam` and
`~/.steam/steam`, follows every library listed there, and reads each
simulator's Proton prefix. It does not support Steam installed as a Flatpak,
whose sandbox would not let the simulator see a library outside the folders it
allows.

The simulator writes its own paths in Windows form inside the prefix, in
`UserCfg.opt` and `EXE.xml`, and the app translates them through the drive
letters of that prefix. It reads an MSFS Addons Linker setup from the
`ProgramData` folder of each prefix.

The AppImage is built on Ubuntu 24.04 and needs an x86_64 system with glibc 2.39
or newer.

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

**Library** lists your addons by category. Tick one to turn it on and clear the
tick to turn it off. To install a new addon, drop a `.zip`, `.rar` or `.7z`
file, or a folder, onto the window and choose the category it goes to. You can
drag addons between categories, and send the ones you no longer want to the
Recycle Bin (the Trash on Linux) or delete them for good.

**Destinations** shows the simulator's own folders and says where each entry came
from: what the app manages, what some other program left there, and which links
are broken. Loose folders can be imported into the library, and broken links
repaired.

**Simulator** lists what starts along with the simulator, from its `EXE.xml`, and
the packages it already carries.

**Presets** are named sets of addons. Before you apply one it says what it would
turn on, what it would turn off, and what is already as it asks.

**Documents** collects the PDFs your addons ship. Manuals stay apart from charts,
and each one reopens on the page you stopped at.

**Quarantine** holds the copy you did not keep when two copies of an addon
conflicted. It stays there until you restore or discard it.

**Diagnostics** shows what the app found on disk: how much space each category
and each addon takes, what the last scan ran into, which dependencies a manifest
declares and which airports more than one scenery covers. It also has a guided
search for the addon that crashes the simulator.

**Journal** lists every operation that changed files on disk, and you can undo
the last change in one step.

The interface speaks English and Brazilian Portuguese, follows the light or dark
theme of the system, and changes language without a restart.

## Manual

The user manual is in [`docs/`](docs/), in
[English](docs/fs-organizer-en.pdf) and
[Brazilian Portuguese](docs/fs-organizer-pt_BR.pdf). Both start with presets,
the part of the program that is hardest to work out by trying.

## Contributing

Building it, running the suite and the packaging are all in
[CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

GPL v2. See [LICENSE](LICENSE).

The interface is set in [Archivo](https://github.com/Omnibus-Type/Archivo),
embedded under the SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
