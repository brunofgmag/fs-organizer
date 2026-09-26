import argparse
import base64
import ctypes
import datetime as dt
import json
import msvcrt
import os
import pathlib
import re
import shutil
import subprocess
import sys
import _winapi
from ctypes import wintypes

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent
CATALOGUE = json.loads((OUT / "catalogue.json").read_text(encoding="utf-8"))

FSCTL_SET_SPARSE = 0x900C4
FILE_ATTRIBUTE_HIDDEN = 0x2
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.DeviceIoControl.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD, wintypes.LPVOID,
                                     wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
kernel32.DeviceIoControl.restype = wintypes.BOOL
kernel32.SetFileAttributesW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
kernel32.SetFilePointerEx.argtypes = [wintypes.HANDLE, ctypes.c_longlong, ctypes.POINTER(ctypes.c_longlong), wintypes.DWORD]
kernel32.SetFilePointerEx.restype = wintypes.BOOL
kernel32.SetEndOfFile.argtypes = [wintypes.HANDLE]
kernel32.SetEndOfFile.restype = wintypes.BOOL
kernel32.GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
kernel32.GetCompressedFileSizeW.restype = wintypes.DWORD
MOST_A_SPARSE_FILE_MAY_OCCUPY = 64 * 1024


def allocated_bytes(path):
    high = wintypes.DWORD(0)
    low = kernel32.GetCompressedFileSizeW(str(path), ctypes.byref(high))
    return (high.value << 32) | low


def sparse_file(path, size):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as handle:
        native = msvcrt.get_osfhandle(handle.fileno())
        returned = wintypes.DWORD(0)
        if not kernel32.DeviceIoControl(native, FSCTL_SET_SPARSE, None, 0, None, 0, ctypes.byref(returned), None):
            raise ctypes.WinError(ctypes.get_last_error())
        if not kernel32.SetFilePointerEx(native, size, None, 0) or not kernel32.SetEndOfFile(native):
            raise ctypes.WinError(ctypes.get_last_error())
    if path.stat().st_size != size or allocated_bytes(path) > MOST_A_SPARSE_FILE_MAY_OCCUPY:
        raise SystemExit(f"{path} is not sparse ({allocated_bytes(path)} bytes on disk): stopping before the disk fills")


def hidden_empty(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"")
    kernel32.SetFileAttributesW(str(path), FILE_ATTRIBUTE_HIDDEN)


def on_drive(windows_path, drive, root):
    relative = windows_path[len(CATALOGUE["drive"]["letter"]) + 1:]
    return pathlib.Path(drive + "\\") / relative if drive else root / "drive" / relative


def retarget(text, drive):
    return text.replace(CATALOGUE["drive"]["letter"] + "\\", drive + "\\")


def write_files(folder, entry, drive):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "manifest.json").write_text(json.dumps(entry["manifest"], indent=4) + "\n", encoding="utf-8")
    for file in entry["files"]:
        target = folder / pathlib.PureWindowsPath(file["path"])
        target.parent.mkdir(parents=True, exist_ok=True)
        kind = file["kind"]
        if kind == "text":
            target.write_text(retarget(file["text"], drive), encoding="utf-8")
        elif kind == "sparse":
            sparse_file(target, file["bytes"])
        elif kind == "bgl":
            target.write_bytes(base64.b64decode(file["base64"]))
        elif kind in ("pdf", "copy"):
            shutil.copyfile(OUT / file["source"], target)
        elif kind == "empty":
            target.write_bytes(b"")
        else:
            raise ValueError(kind)


def junction(link, target):
    link.parent.mkdir(parents=True, exist_ok=True)
    _winapi.CreateJunction(str(target), str(link))


def shift_iso(text, days):
    moment = dt.datetime.fromisoformat(text.replace("Z", "+00:00")) + dt.timedelta(days=days)
    return moment.strftime("%Y-%m-%dT%H:%M:%SZ")


def local_moment(days_ago, clock):
    day = dt.date.today() - dt.timedelta(days=days_ago)
    hour, minute = (int(part) for part in clock.split(":"))
    return dt.datetime(day.year, day.month, day.day, hour, minute).timestamp()


def main():
    parser = argparse.ArgumentParser(description="Writes the FS Organizer store-shot demo installation.")
    parser.add_argument("--root", required=True, help="Folder that will hold drive\\, local\\ and roaming\\.")
    parser.add_argument("--drive", default="F:", help="Drive letter to subst onto <root>\\drive. Empty to skip subst.")
    arguments = parser.parse_args()

    root = pathlib.Path(arguments.root).resolve()
    drive = arguments.drive.rstrip("\\")
    if (root / "drive").exists():
        sys.exit(f"{root} already holds a demo: delete it (junctions first) before writing another")
    (root / "drive").mkdir(parents=True)
    if drive:
        subprocess.run(["subst", drive, str(root / "drive")], check=True)

    days = (dt.date.today() - dt.date.fromisoformat(CATALOGUE["anchorDate"])).days
    paths = CATALOGUE["paths"]

    for category in CATALOGUE["categories"]:
        hidden_empty(on_drive(category["marker"], drive, root))

    for addon in CATALOGUE["library"]:
        folder = on_drive(addon["libraryPath"], drive, root)
        write_files(folder, addon, drive)
        if addon["junction"]:
            junction(on_drive(addon["junction"], drive, root), folder)

    for name in ("Community", "Community2024"):
        on_drive(paths["destinations"][name], drive, root).mkdir(parents=True, exist_ok=True)

    for external in CATALOGUE["destinationEntries"]["external"]:
        target = on_drive(external["path"], drive, root)
        write_files(target, external, drive)
        junction(on_drive(paths["destinations"]["Community"], drive, root) / external["folder"], target)

    for unmanaged in CATALOGUE["destinationEntries"]["unmanaged"]:
        write_files(on_drive(unmanaged["path"], drive, root), unmanaged, drive)

    for item in CATALOGUE["quarantine"]:
        write_files(on_drive(item["path"], drive, root), item, drive)
        sidecar = item["sidecar"]["text"]
        quarantined = int(re.search(r"quarantined=(\d+)", sidecar).group(1)) + days * 86400000
        sidecar = re.sub(r"quarantined=\d+", f"quarantined={quarantined}", sidecar)
        on_drive(item["sidecar"]["path"], drive, root).write_text(retarget(sidecar, drive), encoding="utf-8")

    for program in CATALOGUE["programFiles"]:
        target = on_drive(program, drive, root)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"")

    second = paths["secondProfile"]
    on_drive(second["library"], drive, root).mkdir(parents=True, exist_ok=True)
    on_drive(second["destination"], drive, root).mkdir(parents=True, exist_ok=True)

    state = root / "local" / "fs-organizer"
    shutil.copytree(OUT / "demo" / "state" / "fs-organizer", state)
    for text_file in state.rglob("*.json"):
        text_file.write_text(retarget(text_file.read_text(encoding="utf-8"), drive), encoding="utf-8")
    journal = state / "journal" / "operations.jsonl"
    lines = []
    for line in journal.read_text(encoding="utf-8").splitlines():
        record = json.loads(line)
        record["timestamp"] = shift_iso(record["timestamp"], days)
        lines.append(retarget(json.dumps(record, separators=(",", ":"), sort_keys=True, ensure_ascii=False), drive))
    journal.write_text("\n".join(lines) + "\n", encoding="utf-8")
    presets = CATALOGUE["state"]["presets"]
    for name, spec in presets.items():
        moment = local_moment(spec["modifiedDaysAgo"], "20:40")
        os.utime(state / "presets" / "msfs2024" / f"{name}.json", (moment, moment))
    moment = local_moment(CATALOGUE["state"]["returnPreset"]["modifiedDaysAgo"], "20:41")
    os.utime(state / "presets" / "msfs2024.return.json", (moment, moment))

    simulator = root / "roaming" / "Microsoft Flight Simulator 2024"
    shutil.copytree(OUT / "demo" / "simulator" / "Microsoft Flight Simulator 2024", simulator)
    for name in ("EXE.xml", "UserCfg.opt"):
        text = (simulator / name).read_bytes().decode("utf-8")
        (simulator / name).write_bytes(retarget(text, drive).encode("utf-8"))
    report = simulator / "Report-loading.toml"
    text = report.read_bytes().decode("utf-8")
    text = re.sub(r"TimeUTC=(\S+)", lambda m: "TimeUTC=" + shift_iso(m.group(1), days), text)
    report.write_bytes(text.encode("utf-8"))
    modified = CATALOGUE["simulator"]["contentXml"]["modified"]
    moment = local_moment(modified["daysAgo"], modified["localTime"])
    for name in ("Content.xml", "EXE.xml", "UserCfg.opt"):
        os.utime(simulator / name, (moment, moment))

    print(f"demo written under {root}, dates shifted by {days} days")
    print(f"library {on_drive(paths['library'], drive, root)}")
    print(f'fsorg-shot --state "{state}" --simulator "{simulator}"')


if __name__ == "__main__":
    main()
