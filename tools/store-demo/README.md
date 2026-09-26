# Store demo

A made-up installation for taking the flightsim.to screenshots. It has 62 add-ons from 15 invented studios, the simulator files that go with them, the app's own state and the PDFs those add-ons carry. The only real package names in it are the simulator's own default packages.

## What is here

| Path | What it is |
|---|---|
| `catalogue.json` | The whole demo, entry by entry: add-ons, folders, sizes, files, destinations, quarantine, the simulator's default packages. |
| `demo/state/fs-organizer/` | Settings, journal and presets, laid out like `%LOCALAPPDATA%\fs-organizer`. |
| `demo/simulator/Microsoft Flight Simulator 2024/` | `UserCfg.opt`, `Content.xml`, `EXE.xml` and `Report-loading.toml`. |
| `demo/files/` | The PDFs, the BGL airport records and the chart catalogue the add-ons carry. |
| `sources/build_catalogue.py` | Writes `catalogue.json` and everything under `demo/` except the PDFs. |
| `sources/build_docs.py`, `sources/docs/` | The documents' HTML, printed to PDF with headless Chrome. |
| `sources/bgl.py` | Writes and reads the BGL airport records. |
| `sources/materialize_demo.py` | Builds the demo on disk. |

The demo's paths all start with `F:\`. The library is `F:\MSFS Library`, and the destinations are `F:\MSFS 2024\Community` and `F:\MSFS 2024\Community2024`.

## Build it on disk

```
python tools/store-demo/sources/materialize_demo.py --root <empty folder> --drive F:
```

- The script maps `F:` to `<root>\drive` with `subst`, so every path on screen starts with `F:\`. If `F:` is taken, pick another letter and the script rewrites the paths in every text file.
- It writes the state to `<root>\local\fs-organizer` and the simulator folder to `<root>\roaming\Microsoft Flight Simulator 2024`. The last line it prints is the pair of `fsorg-shot` options that point at them.
- The large add-on files are sparse, so the 127.83 GiB the app measures take no disk space. The script checks each one and stops if a file takes more than 64 KiB on disk. Don't grow a file with Python's `truncate()` on Windows: it writes zeros.
- The files are dated as if today were 2026-09-25. The script moves the journal, the quarantine records and the loading report forward to today, and sets the preset and `Content.xml` modification times relative to today.

Run `build_catalogue.py` and `build_docs.py` only after you change the demo itself. Their output is already committed.

## Take the shots

Close the simulator first, because the app changes several screens while it runs. The tool is `build\release\Release\fsorg-shot.exe`, and it needs Qt on the `PATH`:

```powershell
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
$env:QT_SCALE_FACTOR = "1.2"
fsorg-shot -t dark -l en -s 1344x756 --edition flightsim-to -S halcyon-aircraft-a320neo `
    --state "<root>\local\fs-organizer" --simulator "<root>\roaming\Microsoft Flight Simulator 2024" -o <run A folder>
fsorg-shot -t dark -l en -s 1344x756 --edition flightsim-to -S vireo-efb-tablet `
    --state "<root>\local\fs-organizer" --simulator "<root>\roaming\Microsoft Flight Simulator 2024" -o <run B folder>
```

- Pick `QT_SCALE_FACTOR` so that the Windows scale times the factor is 1.5. At 125 % that is 1.2, and every window shot comes out at 2016 × 1134.
- Check the `window` line in the log. It should read `window 1344x756 at scale 1.5`.
- `--state` and `--simulator` replace the folders Windows reports, and the tool still works on a disposable copy of the state. With `--state` it skips `27-community-import`, `21-library-deep-root` and `30-library-shared-airports`, because those dialogs are drawn from names written into the tool.
- `--edition flightsim-to` builds the updates and the Documents page the way that edition does. `-l en` also formats numbers in English (`8.93 GiB`).

For the listing, take `02-community` from run B and these from run A: `01-library`, `22-simulator-packages`, `03b-presets-plan`, `31-diagnostics-bisection`, `28-documents-outline`, `26-documents`, `09-diagnostics-size`, `19-simulator-startup` and `04-journal`.

## Remove it

```
subst F: /D
rmdir /s /q <root>
```

Run `rmdir` from `cmd`. It deletes the junctions without following them, and every junction points inside `<root>` anyway. Then `subst` with no arguments should print nothing.
