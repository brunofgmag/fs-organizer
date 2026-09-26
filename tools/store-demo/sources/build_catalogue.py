import base64
import datetime as dt
import hashlib
import json
import pathlib
import sys

import bgl

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent
DEMO = OUT / "demo"

ANCHOR = dt.date(2026, 9, 25)
DRIVE = "F:"
LIBRARY = DRIVE + r"\MSFS Library"
SIMULATOR = DRIVE + r"\MSFS 2024"
COMMUNITY = SIMULATOR + r"\Community"
COMMUNITY2024 = SIMULATOR + r"\Community2024"
PROGRAMS = DRIVE + r"\Programs"
LIBRARY_2020 = DRIVE + r"\MSFS 2020 Library"
COMMUNITY_2020 = DRIVE + r"\MSFS 2020\Community"
LIBRARY_QUARANTINE = LIBRARY + r"\_fsorganizer-quarantine"
DESTINATION_QUARANTINE = SIMULATOR + r"\_fsorganizer-quarantine"

LIBRARY_ID = "{5b1e7c2a-9d4f-4e8b-a3c6-2f0d8e71b945}"
LIBRARY_2020_ID = "{c93a0f64-1b7e-4d25-8f3a-6e4b2d90a1c7}"
PROFILE = "msfs2024"

MiB = 1024 * 1024
GiB = 1024 * MiB

STUDIOS = [
    {"name": "Halcyon Flightworks", "prefix": "halcyon", "makes": "airliners and their house liveries"},
    {"name": "Cobalt Ridge Simulations", "prefix": "cobaltridge", "makes": "piston GA aircraft, a flying-club livery pack, a tyre mod"},
    {"name": "Fernhill Simulations", "prefix": "fernhill", "makes": "utility turboprops and a cockpit-lighting mod"},
    {"name": "Quillon Aerospace", "prefix": "quillon", "makes": "2024-native aircraft and their liveries"},
    {"name": "Paperwing Studio", "prefix": "paperwing", "makes": "liveries for fictional airlines, one wing mod"},
    {"name": "Tessellate Airports", "prefix": "tessellate", "makes": "large international airports"},
    {"name": "Harbourlight Scenery", "prefix": "harbourlight", "makes": "coastal regional airports"},
    {"name": "Ironbark Airfields", "prefix": "ironbark", "makes": "Australian and New Zealand airports"},
    {"name": "Wrenfield Simulations", "prefix": "wrenfield", "makes": "Alpine airports and landmarks"},
    {"name": "Lanternfish Design", "prefix": "lanternfish", "makes": "night lighting for airports and cabins"},
    {"name": "Saltmarsh Sound Lab", "prefix": "saltmarsh", "makes": "engine and ambience sound packs"},
    {"name": "Pinecrest Traffic Works", "prefix": "pinecrest", "makes": "AI traffic models, liveries, schedules and an injector"},
    {"name": "Vireo Avionics", "prefix": "vireo", "makes": "an EFB, a hub program, navigation data, an avionics mod"},
    {"name": "Brightwater Simworks", "prefix": "brightwater", "makes": "a bridge program and camera presets"},
    {"name": "Stratoform", "prefix": "stratoform", "makes": "cloud textures and a live-weather engine"},
]

AIRLINES = ["Bluefjord Air", "Solenne Airways", "Sunfield Airways", "Veluna Airlines"]

CATEGORIES = [
    {"name": "Aircraft", "destination": "Community", "pinned": False},
    {"name": "Aircraft (2024)", "destination": "Community2024", "pinned": True},
    {"name": "Aircraft Mods", "destination": "Community", "pinned": False},
    {"name": "Liveries", "destination": "Community", "pinned": False},
    {"name": "Sceneries", "destination": "Community", "pinned": False},
    {"name": "Sounds", "destination": "Community", "pinned": False},
    {"name": "Traffic", "destination": "Community", "pinned": False},
    {"name": "Utilities", "destination": "Community", "pinned": False},
]

MODEL = {
    "halcyon-aircraft-a320neo": "Halcyon_A320neo",
    "halcyon-aircraft-a321neo": "Halcyon_A321neo",
    "halcyon-aircraft-737-800": "Halcyon_737-800",
    "halcyon-aircraft-757-200": "Halcyon_757-200",
    "cobaltridge-aircraft-c172": "CobaltRidge_C172",
    "cobaltridge-aircraft-pa28-181": "CobaltRidge_PA28-181",
    "cobaltridge-aircraft-c152-trainer": "CobaltRidge_C152",
    "fernhill-aircraft-dhc6-300": "Fernhill_DHC6-300",
    "fernhill-aircraft-pc12-47e": "Fernhill_PC12-47E",
    "fernhill-aircraft-dhc8-400": "Fernhill_DHC8-400",
    "quillon-aircraft-a330-900": "Quillon_A330-900",
    "quillon-aircraft-atr72-600": "Quillon_ATR72-600",
    "quillon-aircraft-e175": "Quillon_E175",
    "asobo-aircraft-b737max": "Asobo_B737_MAX",
}

SIM_BASE = [{"name": "fs-base-aircraft-common", "package_version": "0.1.0"},
            {"name": "asobo-vcockpits-instruments", "package_version": "0.1.0"}]
AIRLINER_BASE = SIM_BASE + [{"name": "asobo-vcockpits-instruments-airliners", "package_version": "0.1.0"}]


def A(folder, category, title, creator, version, size, kind, **extra):
    record = {"folder": folder, "category": category, "title": title, "creator": creator,
              "version": version, "size": size, "kind": kind, "enabled": True}
    record.update(extra)
    return record


ADDONS = [
    A("cobaltridge-aircraft-c172", "Aircraft", "C172 Classic", "Cobalt Ridge Simulations", "2.3.1", 1.42 * GiB, "aircraft",
      manufacturer="Cessna", deps=SIM_BASE, docs=["pilot-guide"]),
    A("cobaltridge-aircraft-pa28-181", "Aircraft", "PA-28-181", "Cobalt Ridge Simulations", "1.4.0", 1.18 * GiB, "aircraft",
      manufacturer="Piper", deps=SIM_BASE),
    A("fernhill-aircraft-dhc6-300", "Aircraft", "DHC-6-300", "Fernhill Simulations", "1.1.2", 2.07 * GiB, "aircraft",
      manufacturer="de Havilland Canada", deps=SIM_BASE),
    A("fernhill-aircraft-pc12-47e", "Aircraft", "PC-12/47E", "Fernhill Simulations", "2.0.0", 1.66 * GiB, "aircraft",
      manufacturer="Pilatus", deps=SIM_BASE, enabled=False),
    A("halcyon-aircraft-737-800", "Aircraft", "Halcyon 737-800", "Halcyon Flightworks", "3.0.2", 7.84 * GiB, "aircraft",
      manufacturer="Boeing", deps=AIRLINER_BASE, docs=["737-introduction"]),
    A("halcyon-aircraft-a320neo", "Aircraft", "A320neo", "Halcyon Flightworks", "1.8.4", 8.93 * GiB, "aircraft",
      manufacturer="Airbus", deps=AIRLINER_BASE, docs=["a320neo-fcom", "a320neo-quick-start"],
      wasm=("halcyon_a320_systems.wasm", 188743680)),
    A("halcyon-aircraft-a321neo", "Aircraft", "A321neo", "Halcyon Flightworks", "1.8.4", 6.21 * GiB, "aircraft",
      manufacturer="Airbus", deps=AIRLINER_BASE, enabled=False),

    A("fernhill-aircraft-dhc8-400", "Aircraft (2024)", "DHC-8-400", "Fernhill Simulations", "1.2.6", 5.48 * GiB, "aircraft",
      manufacturer="De Havilland", deps=AIRLINER_BASE, builder=2024),
    A("halcyon-aircraft-757-200", "Aircraft (2024)", "757-200", "Halcyon Flightworks", "1.0.1", 9.62 * GiB, "aircraft",
      manufacturer="Boeing", deps=AIRLINER_BASE, builder=2024),
    A("quillon-aircraft-a330-900", "Aircraft (2024)", "A330-900", "Quillon Aerospace", "1.2.0", 11.37 * GiB, "aircraft",
      manufacturer="Airbus", deps=AIRLINER_BASE, builder=2024, docs=["a330-user-guide"],
      wasm=("quillon_a339_fms.wasm", 142606336)),
    A("quillon-aircraft-atr72-600", "Aircraft (2024)", "ATR 72-600", "Quillon Aerospace", "1.0.3", 4.12 * GiB, "aircraft",
      manufacturer="ATR", deps=AIRLINER_BASE, builder=2024),
    A("quillon-aircraft-e175", "Aircraft (2024)", "E175 (Early Access)", "Quillon Aerospace", "0.9.8", 4.66 * GiB, "aircraft",
      manufacturer="Embraer", deps=AIRLINER_BASE, builder=2024, enabled=False),

    A("cobaltridge-mod-pa28-tundra-tires", "Aircraft Mods", "PA-28-181 Tundra Tyres", "Cobalt Ridge Simulations", "1.0.2",
      38.4 * MiB, "mod", deps=[{"name": "cobaltridge-aircraft-pa28-181", "package_version": "1.3.0"}],
      model="cobaltridge-aircraft-pa28-181", writes="model/tundra_gear.gltf"),
    A("fernhill-mod-b737max-cockpit-lights", "Aircraft Mods", "737 MAX Cockpit Flood Lights", "Fernhill Simulations",
      "1.1.0", 12.7 * MiB, "mod", deps=[{"name": "asobo-aircraft-b737max", "package_version": "0.1.0"}],
      model="asobo-aircraft-b737max", writes="model/cockpit_flood.xml"),
    A("lanternfish-mod-a320neo-cabin-lights", "Aircraft Mods", "A320neo Cabin Mood Lighting", "Lanternfish Design", "2.0.1",
      64.2 * MiB, "mod", deps=[{"name": "halcyon-aircraft-a320neo", "package_version": "1.8.0"}],
      model="halcyon-aircraft-a320neo", writes="model/cabin_lighting.xml"),
    A("paperwing-mod-737-800-wing-flex", "Aircraft Mods", "737-800 Wing Flex", "Paperwing Studio", "0.4.0", 21.9 * MiB,
      "mod", deps=[{"name": "halcyon-aircraft-737-800", "package_version": "3.0.0"}],
      model="halcyon-aircraft-737-800", writes="model/wing_flex.xml", enabled=False),
    A("vireo-mod-c172-avionics-bezel", "Aircraft Mods", "C172 Avionics Bezel", "Vireo Avionics", "1.3.0", 17.3 * MiB, "mod",
      deps=[{"name": "cobaltridge-aircraft-c172", "package_version": "2.3.0"}],
      model="cobaltridge-aircraft-c172", writes="panel/vireo_bezel.cfg"),

    A("cobaltridge-livery-c172-club-pack", "Liveries", "C172 Classic Flying Club Pack", "Cobalt Ridge Simulations", "1.2.0",
      312 * MiB, "livery", model="cobaltridge-aircraft-c172", livery="club-pack"),
    A("halcyon-livery-a320neo-house-colours", "Liveries", "A320neo Halcyon House Colours", "Halcyon Flightworks", "1.0.0",
      148 * MiB, "livery", model="halcyon-aircraft-a320neo", livery="house-colours"),
    A("paperwing-livery-737-800-bluefjord-air", "Liveries", "737-800 Bluefjord Air", "Paperwing Studio", "1.1.0", 176 * MiB,
      "livery", model="halcyon-aircraft-737-800", livery="bluefjord-air"),
    A("paperwing-livery-737-800-sunfield-airways", "Liveries", "737-800 Sunfield Airways", "Paperwing Studio", "1.0.3",
      181 * MiB, "livery", model="halcyon-aircraft-737-800", livery="sunfield-airways"),
    A("paperwing-livery-737-800-veluna-airlines", "Liveries", "737-800 Veluna Airlines", "Paperwing Studio", "1.0.0",
      169 * MiB, "livery", model="halcyon-aircraft-737-800", livery="veluna-airlines", enabled=False),
    A("paperwing-livery-a320neo-bluefjord-air", "Liveries", "A320neo Bluefjord Air", "Paperwing Studio", "1.2.1", 164 * MiB,
      "livery", model="halcyon-aircraft-a320neo", livery="bluefjord-air"),
    A("paperwing-livery-a320neo-solenne-airways", "Liveries", "A320neo Solenne Airways", "Paperwing Studio", "1.2.0",
      158 * MiB, "livery", model="halcyon-aircraft-a320neo", livery="solenne-airways"),
    A("paperwing-livery-a320neo-sunfield-airways", "Liveries", "A320neo Sunfield Airways", "Paperwing Studio", "1.1.4",
      161 * MiB, "livery", model="halcyon-aircraft-a320neo", livery="sunfield-airways", enabled=False),
    A("paperwing-livery-a321neo-veluna-airlines", "Liveries", "A321neo Veluna Airlines", "Paperwing Studio", "1.0.2",
      172 * MiB, "livery", model="halcyon-aircraft-a321neo", livery="veluna-airlines", enabled=False),
    A("paperwing-livery-atr72-sunfield-regional", "Liveries", "ATR 72-600 Sunfield Regional", "Paperwing Studio", "1.0.0",
      96 * MiB, "livery", model="quillon-aircraft-atr72-600", livery="sunfield-regional", builder=2024),
    A("paperwing-livery-b737max-solenne-airways", "Liveries", "737 MAX 8 Solenne Airways", "Paperwing Studio", "1.0.1",
      143 * MiB, "livery", model="asobo-aircraft-b737max", livery="solenne-airways", builder=2024),
    A("paperwing-livery-e175-veluna-connect", "Liveries", "E175 Veluna Connect", "Paperwing Studio", "0.9.0", 88 * MiB,
      "livery", model="quillon-aircraft-e175", livery="veluna-connect", builder=2024, enabled=False),
    A("quillon-livery-a330-900-bluefjord-air", "Liveries", "A330-900 Bluefjord Air", "Quillon Aerospace", "1.0.0", 236 * MiB,
      "livery", model="quillon-aircraft-a330-900", livery="bluefjord-air", builder=2024, pinned="Community2024"),
    A("quillon-livery-a330-900-solenne-airways", "Liveries", "A330-900 Solenne Airways", "Quillon Aerospace", "1.0.0",
      241 * MiB, "livery", model="quillon-aircraft-a330-900", livery="solenne-airways", builder=2024, pinned="Community2024"),

    A("harbourlight-airport-egpe-inverness", "Sceneries", "EGPE Inverness", "Harbourlight Scenery", "1.0.4",
      1.12 * GiB, "scenery", airports=["EGPE"]),
    A("harbourlight-airport-eick-cork", "Sceneries", "EICK Cork Airport", "Harbourlight Scenery", "1.1.0", 1.38 * GiB,
      "scenery", airports=["EICK"]),
    A("harbourlight-airport-enbr-bergen", "Sceneries", "ENBR Bergen Airport Flesland", "Harbourlight Scenery", "1.0.0",
      1.64 * GiB, "scenery", airports=["ENBR"], builder=2024),
    A("ironbark-airport-nzqn-queenstown", "Sceneries", "NZQN Queenstown Airport", "Ironbark Airfields", "2.1.0", 2.26 * GiB,
      "scenery", airports=["NZQN"]),
    A("ironbark-airport-ybbn-brisbane", "Sceneries", "YBBN Brisbane Airport", "Ironbark Airfields", "1.3.1", 3.05 * GiB,
      "scenery", airports=["YBBN"]),
    A("ironbark-airport-ymml-melbourne", "Sceneries", "YMML Melbourne Airport", "Ironbark Airfields", "1.0.2", 2.81 * GiB,
      "scenery", airports=["YMML"], enabled=False),
    A("lanternfish-airport-lppt-night-lights", "Sceneries", "LPPT Lisbon Night Lighting", "Lanternfish Design", "1.0.0",
      212 * MiB, "scenery", airports=["LPPT"]),
    A("tessellate-airport-kbos-boston", "Sceneries", "KBOS Boston Logan International", "Tessellate Airports", "1.4.2",
      3.87 * GiB, "scenery", airports=["KBOS"], builder=2024),
    A("tessellate-airport-lemd-madrid", "Sceneries", "LEMD Adolfo Suarez Madrid-Barajas", "Tessellate Airports", "2.0.0",
      4.44 * GiB, "scenery", airports=["LEMD"], builder=2024),
    A("tessellate-airport-lpfr-faro", "Sceneries", "LPFR Faro Airport", "Tessellate Airports", "1.3.2", 1.96 * GiB,
      "scenery", airports=["LPFR"], docs=["lpfr-scenery-manual"], charts=True),
    A("tessellate-airport-lppt-lisbon", "Sceneries", "LPPT Humberto Delgado Lisbon", "Tessellate Airports", "2.2.1",
      3.58 * GiB, "scenery", airports=["LPPT"], builder=2024),
    A("tessellate-airport-sbgr-guarulhos", "Sceneries", "SBGR Sao Paulo-Guarulhos International", "Tessellate Airports",
      "1.1.0", 4.02 * GiB, "scenery", airports=["SBGR"], enabled=False),
    A("wrenfield-airport-lowi-innsbruck", "Sceneries", "LOWI Innsbruck Airport", "Wrenfield Simulations", "3.0.1",
      2.49 * GiB, "scenery", airports=["LOWI"]),
    A("wrenfield-airport-lows-salzburg", "Sceneries", "LOWS Salzburg W. A. Mozart", "Wrenfield Simulations", "1.2.0",
      1.73 * GiB, "scenery", airports=["LOWS"]),
    A("wrenfield-airport-lszh-zurich", "Sceneries", "LSZH Zurich Airport", "Wrenfield Simulations", "2.4.0", 4.71 * GiB,
      "scenery", airports=["LSZH"], builder=2024),
    A("wrenfield-scenery-alpine-landmarks", "Sceneries", "Alpine Landmarks", "Wrenfield Simulations", "1.0.5", 2.18 * GiB,
      "scenery", airports=[]),

    A("saltmarsh-sound-737-800-cfm56", "Sounds", "737-800 CFM56 Sound Pack", "Saltmarsh Sound Lab", "1.2.0", 486 * MiB,
      "sound", model="halcyon-aircraft-737-800", deps=[{"name": "halcyon-aircraft-737-800", "package_version": "3.0.0"}]),
    A("saltmarsh-sound-a320neo-leap1a", "Sounds", "A320neo LEAP-1A Sound Pack", "Saltmarsh Sound Lab", "1.1.3", 512 * MiB,
      "sound", model="halcyon-aircraft-a320neo", deps=[{"name": "halcyon-aircraft-a320neo", "package_version": "1.8.0"}]),
    A("saltmarsh-sound-ambience-airports", "Sounds", "Airport Ambience", "Saltmarsh Sound Lab", "2.0.0", 734 * MiB,
      "ambience"),
    A("saltmarsh-sound-c172-piston", "Sounds", "C172 Piston Sound Pack", "Saltmarsh Sound Lab", "1.0.4", 228 * MiB,
      "sound", model="cobaltridge-aircraft-c172", deps=[{"name": "cobaltridge-aircraft-c172", "package_version": "2.0.0"}]),

    A("pinecrest-traffic-flightplans-s26", "Traffic", "Schedules Summer 2026", "Pinecrest Traffic Works", "2026.3.1",
      94 * MiB, "traffic-data"),
    A("pinecrest-traffic-liveries-europe", "Traffic", "AI Liveries Europe", "Pinecrest Traffic Works", "4.2.0", 6.34 * GiB,
      "traffic-models"),
    A("pinecrest-traffic-models-narrowbody", "Traffic", "AI Models Narrowbody", "Pinecrest Traffic Works", "4.1.2",
      3.91 * GiB, "traffic-models"),
    A("pinecrest-traffic-models-widebody", "Traffic", "AI Models Widebody", "Pinecrest Traffic Works", "4.1.2", 4.87 * GiB,
      "traffic-models"),

    A("brightwater-util-bridge", "Utilities", "Brightwater Bridge", "Brightwater Simworks", "3.4.0", 146 * MiB, "utility",
      exe=r"bin\BrightwaterBridge.exe", wasm=("brightwater_bridge.wasm", 2359296)),
    A("brightwater-util-camera-presets", "Utilities", "Camera Presets", "Brightwater Simworks", "1.6.2", 3.2 * MiB, "utility"),
    A("lanternfish-lights-apron-floods", "Utilities", "Apron Flood Lights", "Lanternfish Design", "1.4.0", 58.6 * MiB,
      "utility"),
    A("pinecrest-util-traffic-injector", "Utilities", "Traffic Injector", "Pinecrest Traffic Works", "2.2.0", 71.4 * MiB,
      "utility", exe=r"tools\PinecrestInjector.exe", wasm=("pinecrest_injector.wasm", 5242880)),
    A("stratoform-weather-cloud-textures", "Utilities", "Cloud Textures HD", "Stratoform", "1.9.0", 1.27 * GiB, "utility",
      enabled=False),
    A("vireo-efb-tablet", "Utilities", "Vireo EFB", "Vireo Avionics", "2.4.0", 212 * MiB, "utility",
      docs=["vireo-efb-guide"], wasm=("vireo_efb.wasm", 12582912), enabled=False, conflict=True),
    A("vireo-navdata-airac-2609", "Utilities", "Navigation Data AIRAC 2609", "Vireo Avionics", "2609.1.0", 386 * MiB,
      "navdata", airports=["LPPT", "LPFR", "LEMD", "LOWI", "LSZH", "EICK", "KBOS", "YBBN"]),
]

EXTERNAL = [
    {"folder": "vireo-hub-core", "title": "Vireo Hub Core", "creator": "Vireo Avionics", "version": "5.0.3",
     "size": 41.8 * MiB, "target": PROGRAMS + r"\Vireo Avionics\Vireo Hub\Package\vireo-hub-core",
     "why": "installed by the Vireo Hub setup, which keeps the package in its own program folder and links it into Community",
     "wasm": ("vireo_hub.wasm", 3145728)},
    {"folder": "stratoform-weather-live-engine", "title": "Stratoform Live Weather", "creator": "Stratoform",
     "version": "2.8.1", "size": 23.5 * MiB,
     "target": PROGRAMS + r"\Stratoform\Live Weather\Package\stratoform-weather-live-engine",
     "why": "the live-weather program's own package, linked in by its installer",
     "wasm": ("stratoform_live.wasm", 7340032)},
]

UNMANAGED = [
    {"folder": "harbourlight-airport-lpma-madeira", "title": "LPMA Madeira Cristiano Ronaldo International",
     "creator": "Harbourlight Scenery", "version": "1.0.0", "size": 1.21 * GiB, "kind": "scenery", "airports": ["LPMA"],
     "why": "downloaded yesterday and unzipped straight into Community, not imported yet"},
    {"folder": "paperwing-livery-atr72-veluna-connect", "title": "ATR 72-600 Veluna Connect", "creator": "Paperwing Studio",
     "version": "1.0.0", "size": 94 * MiB, "kind": "livery", "model": "quillon-aircraft-atr72-600",
     "livery": "veluna-connect", "builder": 2024, "why": "a new livery, unzipped into Community, not imported yet"},
    {"folder": "vireo-efb-tablet", "title": "Vireo EFB", "creator": "Vireo Avionics", "version": "2.5.1", "size": 219 * MiB,
     "kind": "utility", "why": "the Vireo updater replaced the link with a real folder holding 2.5.1: the library still "
                               "holds 2.4.0, so this is the one conflict"},
]

QUARANTINE = [
    {"folder": "cobaltridge-aircraft-c152-trainer", "title": "C152 Trainer", "creator": "Cobalt Ridge Simulations",
     "version": "1.1.0", "size": 780 * MiB, "side": "library", "origin": LIBRARY + r"\Aircraft\cobaltridge-aircraft-c152-trainer",
     "days_ago": 15, "at": "23:11:42", "manufacturer": "Cessna", "kind": "aircraft"},
    {"folder": "tessellate-airport-lpfr-faro", "title": "LPFR Faro Airport", "creator": "Tessellate Airports",
     "version": "1.3.0", "size": 1.94 * GiB, "side": "destination", "origin": COMMUNITY + r"\tessellate-airport-lpfr-faro",
     "days_ago": 9, "at": "21:22:07", "kind": "scenery", "airports": ["LPFR"]},
]

DEFAULT_PACKAGES = [
    ("fs24-asobo-aircraft-b737max", "Activated"),
    ("fs24-asobo-aircraft-c172sp-as1000", "Activated"),
    ("fs24-asobo-airport-kjfk-new-york-jfk", "Activated"),
    ("fs24-asobo-airport-lowi-innsbruck", "UserDisabled"),
    ("fs24-asobo-airport-lpma-madeira", "UserDisabled"),
    ("fs24-asobo-airport-nzqn-queenstown", "Activated"),
    ("fs24-asobo-jetways", "Activated"),
    ("fs24-asobo-modellib-airport-generic", "Activated"),
    ("fs24-asobo-vcockpits-instruments", "Activated"),
    ("fs24-asobo-vcockpits-instruments-airliners", "Activated"),
    ("fs24-fcr-embedded", "Activated"),
    ("fs24-fs-base", "Activated"),
    ("fs24-fs-base-aircraft-common", "Activated"),
    ("fs24-microsoft-airport-eick-cork", "Activated"),
    ("fs24-microsoft-airport-lows-salzburg", "Activated"),
    ("fs24-microsoft-airport-lpfr-faro", "Activated"),
    ("fs24-microsoft-airport-lszh-zurich", "UserDisabled"),
]

STALE_IN_CONTENT_XML = ["cobaltridge-aircraft-c152-trainer", "halcyon-aircraft-a321neo", "tessellate-airport-sbgr-guarulhos"]

STARTUP = [
    {"name": "Brightwater Bridge", "disabled": False, "path": COMMUNITY + r"\brightwater-util-bridge\bin\BrightwaterBridge.exe",
     "commandLine": "-minimized", "reach": "inside an addon (brightwater-util-bridge, enabled)"},
    {"name": "Stratoform Live Weather", "disabled": False,
     "path": PROGRAMS + r"\Stratoform\Live Weather\StratoformLive.exe", "reach": "outside your addons"},
    {"name": "Pinecrest Traffic Injector", "disabled": True,
     "path": COMMUNITY + r"\pinecrest-util-traffic-injector\tools\PinecrestInjector.exe",
     "reach": "inside an addon (pinecrest-util-traffic-injector, enabled), switched off five days ago"},
]

PROGRAM_FILES = [
    PROGRAMS + r"\Stratoform\Live Weather\StratoformLive.exe",
    PROGRAMS + r"\Vireo Avionics\Vireo Hub\VireoHub.exe",
]

DOCUMENT_FILES = {
    "pilot-guide": r"Manual\Pilot Guide.pdf",
    "737-introduction": r"Documents\737-800 Introduction.pdf",
    "a320neo-fcom": r"Documents\A320neo FCOM.pdf",
    "a320neo-quick-start": r"Documents\Quick Start Guide.pdf",
    "a330-user-guide": r"Docs\A330-900 User Guide.pdf",
    "lpfr-scenery-manual": r"Manual\Scenery Manual.pdf",
    "vireo-efb-guide": r"docs\Vireo EFB Guide.pdf",
}

CHARTS = [
    {"chart_id": "LPFR_ADC", "chart_type": "AGC", "chart_name": "Aerodrome chart", "source": "lpfr-adc"},
    {"chart_id": "LPFR_APC", "chart_type": "APC", "chart_name": "Aprons and stands", "source": "lpfr-apc"},
    {"chart_id": "LPFR_VAC", "chart_type": "VAC", "chart_name": "Visual approach", "source": "lpfr-vac"},
]

READ_DOCUMENTS = [
    {"addon": "halcyon-aircraft-a320neo", "document": DOCUMENT_FILES["a320neo-fcom"], "page": 7, "favourite": True,
     "bookmarks": [{"page": 8, "name": "Takeoff speeds"}]},
    {"addon": "halcyon-aircraft-737-800", "document": DOCUMENT_FILES["737-introduction"], "page": 3, "favourite": False,
     "bookmarks": []},
    {"addon": "cobaltridge-aircraft-c172", "document": DOCUMENT_FILES["pilot-guide"], "page": 2, "favourite": False,
     "bookmarks": []},
]


def by_folder(folder):
    return next(addon for addon in ADDONS if addon["folder"] == folder)


def category(addon):
    return next(entry for entry in CATEGORIES if entry["name"] == addon["category"])


def destination_name(addon):
    return addon.get("pinned") or category(addon)["destination"]


def destination_path(name):
    return {"Community": COMMUNITY, "Community2024": COMMUNITY2024}[name]


def library_path(addon):
    return LIBRARY + "\\" + addon["category"] + "\\" + addon["folder"]


def builder(record):
    return "Microsoft Flight Simulator 2024" if record.get("builder") == 2024 else None


def generation(record):
    return "communityfs24" if record.get("builder") == 2024 else "communityfs20"


def content_type(kind):
    return {"aircraft": "AIRCRAFT", "mod": "AIRCRAFT", "livery": "LIVERY", "scenery": "SCENERY", "sound": "MISC",
            "ambience": "MISC", "traffic-data": "MISC", "traffic-models": "AIRCRAFT", "utility": "MISC",
            "navdata": "MISC"}[kind]


def order_hint(record):
    kind = record["kind"]
    if kind == "scenery":
        return "CUSTOM_AIRPORT" if record.get("airports") else "CUSTOM_SCENERY"
    if kind == "navdata":
        return "CUSTOM_NAVDATA"
    if kind in ("mod", "sound"):
        return "CUSTOM_SIMOBJECTS_PATCH"
    return None


def jitter(folder, size):
    digest = int(hashlib.sha256(folder.encode()).hexdigest()[:8], 16)
    return int(size) + digest % 1048573


def manifest_of(record, total):
    manifest = {
        "dependencies": record.get("deps", []),
        "content_type": content_type(record["kind"]),
        "title": record["title"],
        "manufacturer": record.get("manufacturer", ""),
        "creator": record["creator"],
        "package_version": record["version"],
        "minimum_game_version": "1.4.20" if record.get("builder") == 2024 else "1.37.19",
        "release_notes": {"neutral": {"LastUpdate": "", "OlderHistory": ""}},
    }
    if builder(record):
        manifest["builder"] = builder(record)
    if order_hint(record):
        manifest["package_order_hint"] = order_hint(record)
    manifest["total_package_size"] = f"{total:020d}"
    return manifest


def model_folder(folder):
    return MODEL[folder]


def text_file(path, text):
    return {"path": path, "kind": "text", "text": text}


def sparse(path, size):
    return {"path": path, "kind": "sparse", "bytes": int(size)}


def files_of(record, total):
    folder = record["folder"]
    kind = record["kind"]
    files = [text_file("layout.json", '{\n    "content": []\n}\n')]
    small = 0

    if kind == "aircraft":
        model = model_folder(folder)
        root = rf"SimObjects\Airplanes\{model}"
        files += [
            text_file(rf"{root}\aircraft.cfg",
                      f"[VERSION]\nmajor = 1\nminor = 0\n\n[GENERAL]\natc_type = \"{record.get('manufacturer', '')}\"\n"
                      f"atc_model = \"{record['title']}\"\n\n[FLTSIM.0]\ntitle = \"{record['creator']} {record['title']}\"\n"),
            text_file(rf"{root}\panel\panel.cfg", "[VERSION]\nmajor = 1\nminor = 0\n\n[VCockpit01]\nsize_mm = 1024, 768\n"),
            text_file(rf"{root}\sound\sound.xml", "<SoundInfo Version=\"0.1\">\n</SoundInfo>\n"),
        ]
        bulk = [(rf"{root}\model\{model}_EXTERIOR_LOD00.bin", 0.46), (rf"{root}\model\{model}_INTERIOR.bin", 0.21),
                (rf"{root}\texture\{model}_ALBD.ktx2", 0.33)]
    elif kind == "livery":
        model = model_folder(record["model"])
        root = rf"SimObjects\Airplanes\{model}\liveries\{folder.split('-')[0]}\{record['livery']}"
        files += [
            text_file(rf"{root}\livery.cfg", f"[VERSION]\nmajor = 1\nminor = 0\n\n[GENERAL]\nname = \"{record['title']}\"\n"),
            text_file(rf"{root}\livery.json",
                      json.dumps({"name": record["title"], "productPackage": record["model"]}, indent=4) + "\n"),
        ]
        bulk = [(rf"{root}\texture\{record['livery'].upper()}_ALBD.ktx2", 1.0)]
    elif kind == "mod":
        model = model_folder(record["model"])
        files += [text_file(rf"SimObjects\Airplanes\{model}\{record['writes']}",
                            "<!-- " + record["title"] + " -->\n")]
        bulk = [(rf"SimObjects\Airplanes\{model}\texture\{folder.split('-')[0].upper()}_MOD.ktx2", 1.0)]
    elif kind == "sound":
        model = model_folder(record["model"])
        files += [text_file(rf"SimObjects\Airplanes\{model}\sound\sound.xml",
                            "<SoundInfo Version=\"0.1\">\n  <!-- " + record["title"] + " -->\n</SoundInfo>\n")]
        bulk = [(rf"SimObjects\Airplanes\{model}\sound\{folder.split('-', 2)[2].upper()}.PC.PCK", 1.0)]
    elif kind == "ambience":
        bulk = [(r"SoundAI\ambience\AIRPORT_AMBIENCE.PC.PCK", 1.0)]
    elif kind == "scenery":
        codes = record.get("airports", [])
        region = folder.split("-")[0]
        for code in codes:
            files.append({"path": rf"scenery\{region}\{code.lower()}\{code}_AIRPORT.bgl", "kind": "bgl",
                          "airports": [code], "record": "fs24" if record.get("builder") == 2024 else "fs20"})
        name = codes[0].lower() if codes else "landmarks"
        bulk = [(rf"scenery\global\texture\{name}_ground_albd.png.dds", 0.62),
                (rf"scenery\global\modellib\{name}_objects.bin", 0.38)]
    elif kind == "navdata":
        files.append({"path": r"scenery\navdata\airac2609\AIRPORTS.bgl", "kind": "bgl",
                      "airports": record["airports"], "record": "fs20"})
        bulk = [(r"scenery\navdata\airac2609\NAVAIDS.bin", 1.0)]
    elif kind == "traffic-data":
        bulk = [(r"traffic\s26\flightplans.bin", 1.0)]
    elif kind == "traffic-models":
        bulk = [(rf"SimObjects\Airplanes\{folder.split('-', 2)[2].upper()}\model\PACK.bin", 0.55),
                (rf"SimObjects\Airplanes\{folder.split('-', 2)[2].upper()}\texture\PACK.ktx2", 0.45)]
    else:
        bulk = [(r"html_ui\Pages\VCockpit\Instruments\package.bin", 1.0)]

    for document in record.get("docs", []):
        files.append({"path": DOCUMENT_FILES[document], "kind": "pdf", "source": f"demo/files/pdf/{document}.pdf"})
    if record.get("charts"):
        files.append({"path": r"Charts\LPFR\catalogue.json", "kind": "copy", "source": "demo/files/charts/LPFR/catalogue.json"})
        for chart in CHARTS:
            files.append({"path": rf"Charts\LPFR\{chart['chart_id']}.pdf", "kind": "pdf",
                          "source": f"demo/files/pdf/{chart['source']}.pdf"})
    if record.get("exe"):
        files.append({"path": record["exe"], "kind": "empty"})
    if record.get("wasm"):
        files.append(sparse(rf"SimObjects\Airplanes\{folder}\panel\{record['wasm'][0]}", 1.5 * MiB))

    for entry in files:
        if entry["kind"] == "text":
            small += len(entry["text"].encode())
        elif entry["kind"] == "sparse":
            small += entry["bytes"]
    remaining = max(total - small - 64 * 1024, 1024)
    for index, (path, share) in enumerate(bulk):
        size = int(remaining * share) if index < len(bulk) - 1 else remaining - sum(
            int(remaining * part) for _, part in bulk[:-1])
        files.append(sparse(path, size))
    return files


def addon_entry(record):
    total = jitter(record["folder"], record["size"])
    destination = destination_name(record)
    entry = {
        "folder": record["folder"],
        "category": record["category"],
        "libraryPath": library_path(record),
        "enabled": record["enabled"],
        "destination": destination,
        "pinned": "addon" if record.get("pinned") else ("category" if category(record)["pinned"] else None),
        "junction": (destination_path(destination) + "\\" + record["folder"]) if record["enabled"] else None,
        "contentXml": generation(record) + "-" + record["folder"] if record["enabled"] else None,
        "sizeBytes": total,
        "airports": record.get("airports", []),
        "documents": [DOCUMENT_FILES[d] for d in record.get("docs", [])],
        "charts": ["Charts\LPFR\\" + c["chart_id"] + ".pdf" for c in CHARTS] if record.get("charts") else [],
        "startupExecutable": record.get("exe"),
        "wasmModule": record["wasm"][0] if record.get("wasm") else None,
        "conflict": bool(record.get("conflict")),
        "manifest": manifest_of(record, total),
        "files": files_of(record, total),
    }
    return entry


def outside_entry(record, place, where):
    total = jitter(record["folder"], record["size"])
    shaped = dict(record)
    shaped.setdefault("kind", "utility")
    return {
        "folder": record["folder"],
        "where": where,
        "path": place,
        "why": record.get("why", ""),
        "sizeBytes": total,
        "manifest": manifest_of(shaped, total),
        "files": files_of(shaped, total),
    }


def utc(days_ago, at):
    day = ANCHOR - dt.timedelta(days=days_ago)
    return f"{day.isoformat()}T{at}Z"


def epoch_ms(days_ago, at):
    moment = dt.datetime.fromisoformat(f"{(ANCHOR - dt.timedelta(days=days_ago)).isoformat()}T{at}+00:00")
    return int(moment.timestamp() * 1000)


def link(kind, folder, days_ago, at, library_id=LIBRARY_ID):
    addon = by_folder(folder)
    return {"addon": folder, "failure": "none", "kind": kind, "libraryId": library_id,
            "source": library_path(addon), "target": destination_path(destination_name(addon)) + "\\" + folder,
            "timestamp": utc(days_ago, at)}


def file_step(kind, folder, source, target, days_ago, at, library_id=LIBRARY_ID, **extra):
    record = {"addon": folder, "kind": kind, "libraryId": library_id, "result": "completed", "source": source,
              "target": target, "timestamp": utc(days_ago, at)}
    record.update(extra)
    return record


def journal():
    madrid = by_folder("tessellate-airport-lemd-madrid")
    arriving = COMMUNITY + r"\tessellate-airport-lemd-madrid"
    staged = library_path(madrid) + ".fsorg-partial"
    trainer = QUARANTINE[0]
    faro = QUARANTINE[1]
    records = [
        file_step("importCopyToStaging", madrid["folder"], arriving, staged, 24, "22:02:31"),
        file_step("importVerifyStaging", madrid["folder"], staged, library_path(madrid), 24, "22:04:12"),
        file_step("importMoveIntoPlace", madrid["folder"], staged, library_path(madrid), 24, "22:04:12"),
        file_step("importRemoveSource", madrid["folder"], arriving, library_path(madrid), 24, "22:04:13"),
        link("enable", madrid["folder"], 24, "22:04:13"),
        {"addon": trainer["folder"], "failure": "none", "kind": "disable", "libraryId": LIBRARY_ID,
         "source": trainer["origin"], "target": COMMUNITY + "\\" + trainer["folder"], "timestamp": utc(15, "23:10:58")},
        file_step("quarantineFromLibrary", trainer["folder"], trainer["origin"],
                  LIBRARY_QUARANTINE + "\\" + trainer["folder"], 15, trainer["at"]),
        file_step("quarantineFromDestination", faro["folder"], faro["origin"],
                  DESTINATION_QUARANTINE + "\\" + faro["folder"], 9, faro["at"], library_id=""),
        link("enable", faro["folder"], 9, "21:22:08"),
        link("enable", "paperwing-livery-a320neo-solenne-airways", 6, "20:05:44"),
        {"addon": "", "kind": "turnOffTheStartupEntry", "label": STARTUP[2]["name"], "libraryId": "",
         "result": "completed", "source": "", "target": STARTUP[2]["path"], "timestamp": utc(5, "23:30:19")},
        link("disable", "halcyon-aircraft-a320neo", 4, "22:31:02"),
        link("enable", "halcyon-aircraft-a320neo", 4, "22:52:47"),
        link("disable", "stratoform-weather-cloud-textures", 3, "00:15:36"),
        link("disable", "paperwing-livery-a320neo-sunfield-airways", 3, "00:15:36"),
        link("enable", "harbourlight-airport-enbr-bergen", 2, "22:48:10"),
        link("enable", "lanternfish-airport-lppt-night-lights", 1, "23:02:55"),
        link("enable", "wrenfield-airport-lows-salzburg", 1, "23:03:40"),
        link("disable", "quillon-aircraft-e175", 0, "13:14:05"),
        link("disable", "paperwing-livery-e175-veluna-connect", 0, "13:14:05"),
    ]
    return records


def preset(name, folders, startup=None, governs=False):
    return {
        "entries": [{"action": "enable", "folderName": folder, "libraryId": LIBRARY_ID} for folder in folders],
        "governsStartup": governs,
        "name": name,
        "startupEntries": [{"action": action, "path": path} for path, action in (startup or [])],
    }


def presets():
    enabled = [a["folder"] for a in ADDONS if a["enabled"]]
    airline = [f for f in enabled if not any(key in f for key in ("c172", "pa28", "dhc6", "cobaltridge", "tundra"))]
    airline += ["halcyon-aircraft-a321neo", "paperwing-livery-a321neo-veluna-airlines",
                "paperwing-livery-737-800-veluna-airlines"]
    ga = ["cobaltridge-aircraft-c172", "cobaltridge-aircraft-pa28-181", "fernhill-aircraft-dhc6-300",
          "cobaltridge-mod-pa28-tundra-tires", "vireo-mod-c172-avionics-bezel", "cobaltridge-livery-c172-club-pack",
          "saltmarsh-sound-c172-piston", "saltmarsh-sound-ambience-airports", "harbourlight-airport-egpe-inverness",
          "harbourlight-airport-eick-cork", "harbourlight-airport-enbr-bergen", "ironbark-airport-nzqn-queenstown",
          "wrenfield-airport-lowi-innsbruck", "wrenfield-airport-lows-salzburg", "wrenfield-scenery-alpine-landmarks",
          "lanternfish-lights-apron-floods", "brightwater-util-camera-presets", "vireo-navdata-airac-2609"]
    long_haul = ["quillon-aircraft-a330-900", "halcyon-aircraft-757-200", "quillon-livery-a330-900-bluefjord-air",
                 "quillon-livery-a330-900-solenne-airways", "tessellate-airport-lppt-lisbon",
                 "lanternfish-airport-lppt-night-lights", "tessellate-airport-kbos-boston",
                 "tessellate-airport-sbgr-guarulhos", "tessellate-airport-lemd-madrid",
                 "pinecrest-traffic-models-widebody", "pinecrest-traffic-models-narrowbody",
                 "pinecrest-traffic-liveries-europe", "pinecrest-traffic-flightplans-s26",
                 "pinecrest-util-traffic-injector", "brightwater-util-bridge", "vireo-navdata-airac-2609",
                 "saltmarsh-sound-ambience-airports"]
    bridge, weather, injector = (entry["path"] for entry in STARTUP)
    before_long_haul = [f for f in enabled if f not in ("lanternfish-airport-lppt-night-lights",
                                                          "wrenfield-airport-lows-salzburg",
                                                          "harbourlight-airport-enbr-bergen")] + ["quillon-aircraft-e175"]
    return {
        "Airline Ops": (preset("Airline Ops", airline, [(bridge, "enable"), (weather, "enable"), (injector, "enable")],
                               True), 12),
        "GA Weekend": (preset("GA Weekend", ga), 20),
        "Long Haul": (preset("Long Haul", long_haul, [(bridge, "enable"), (weather, "enable"), (injector, "enable")],
                             True), 3),
        "__return__": (preset("", before_long_haul, governs=True), 3),
    }


def settings():
    overrides = [{"destination": COMMUNITY2024, "libraryId": LIBRARY_ID, "relativePath": "Aircraft (2024)"}]
    overrides += [{"destination": COMMUNITY2024, "libraryId": LIBRARY_ID,
                   "relativePath": a["category"] + "\\" + a["folder"]} for a in ADDONS if a.get("pinned")]
    return {
        "activeProfileId": PROFILE,
        "coexistingAirports": [],
        "documents": READ_DOCUMENTS,
        "dragMovesCharts": True,
        "dragMovesDocuments": True,
        "language": "en",
        "linkType": "junction",
        "managePackageList": True,
        "manageStartupEntries": True,
        "profiles": [
            {"defaultDestination": COMMUNITY, "destinationOverrides": overrides, "destinations": [COMMUNITY, COMMUNITY2024],
             "externalOrigins": [], "id": PROFILE,
             "libraries": [{"id": LIBRARY_ID, "label": "MSFS 2024", "path": LIBRARY}], "variant": "MSFS2024"},
            {"defaultDestination": COMMUNITY_2020, "destinationOverrides": [], "destinations": [COMMUNITY_2020],
             "externalOrigins": [], "id": "msfs2020",
             "libraries": [{"id": LIBRARY_2020_ID, "label": "MSFS 2020", "path": LIBRARY_2020}], "variant": "MSFS2020"},
        ],
        "updateMode": "notify",
        "verifyWithHash": False,
        "wheelZoomsCharts": True,
        "wheelZoomsDocuments": False,
    }


def unprefixed(name):
    for prefix in ("communityfs24-", "communityfs20-", "fs24-", "fs20-"):
        if name.startswith(prefix):
            return name[len(prefix):]
    return name


def content_xml():
    rows = [(name, state) for name, state in DEFAULT_PACKAGES]
    rows += [(generation(a) + "-" + a["folder"], "Activated") for a in ADDONS if a["enabled"]]
    rows += [("communityfs20-" + e["folder"], "Activated") for e in EXTERNAL]
    rows += [(generation(u) + "-" + u["folder"], "Activated") for u in UNMANAGED]
    stale = {q["folder"]: q for q in QUARANTINE}
    for folder in STALE_IN_CONTENT_XML:
        record = stale.get(folder) or by_folder(folder)
        rows.append((generation(record) + "-" + folder, "Activated"))
    rows.sort(key=lambda row: unprefixed(row[0]).lower())
    body = "".join(f'\t<Package name="{name}" active="{state}"/>\n' for name, state in rows)
    return "<Packages>\n" + body + "</Packages>\n", rows


def exe_xml():
    lines = ['<?xml version="1.0" encoding="utf-8"?>', '<SimBase.Document Type="Launch" version="1,0">',
             "  <Descr>Launch</Descr>", "  <Filename>EXE.xml</Filename>", "  <Disabled>False</Disabled>",
             "  <Launch.ManualLoad>False</Launch.ManualLoad>"]
    for entry in STARTUP:
        lines += ["  <Launch.Addon>", f"    <Name>{entry['name']}</Name>",
                  f"    <Disabled>{'True' if entry['disabled'] else 'False'}</Disabled>", f"    <Path>{entry['path']}</Path>"]
        if entry.get("commandLine"):
            lines.append(f"    <CommandLine>{entry['commandLine']}</CommandLine>")
        lines.append("  </Launch.Addon>")
    lines.append("</SimBase.Document>")
    return "\r\n".join(lines) + "\r\n"


def loading_report():
    modules = [(a["wasm"][0], a["folder"], a["wasm"][1]) for a in ADDONS if a.get("wasm") and a["enabled"]]
    modules += [(e["wasm"][0], e["folder"], e["wasm"][1]) for e in EXTERNAL]
    modules.append(("vireo_efb.wasm", "vireo-efb-tablet", 12582912))
    modules.append(("fcr_embedded_system.wasm", "fcr-embedded", 589824))
    modules.sort(key=lambda row: -row[2])
    lines = ["[Engine]", "ReportVersion=10", "ExitApp=false", f"TimeUTC={utc(1, '01:47:12')}",
             'BuildVersion="1.7.35.0"', "[Wasm_Modules]",
             'Format="Handle,DebugName,PackageName,MemorySize,Status,DirtyFrame,DirtyErrorCode,DirtyJob,CurrentStep,LastStateUpdate"']
    handle = 4294967296 * len(modules)
    for index, (module, package, memory) in enumerate(modules):
        lines.append(f'{index}=[{handle + 13},"{module}","{package}",{memory},"Ready","-","-","-","Idle",40417]')
        handle -= 4294967296
    lines += ["[MemoryTrace]", "Peak=0", "[FlightSimulator_Packages]"]
    registered = [(unprefixed(name), "Market") for name, state in DEFAULT_PACKAGES
                  if state == "Activated" and "-airport-" in name and "modellib" not in name]
    registered += [(a["folder"], "Community") for a in ADDONS if a["enabled"]]
    registered += [(e["folder"], "Community") for e in EXTERNAL]
    registered += [(u["folder"], "Community") for u in UNMANAGED]
    for name, origin in registered:
        version = ".".join(f"{int(part):03d}" for part in (by_version(name) + ".0.0.0").split(".")[:4])
        lines.append(f'{name}=["manifest={version} cds=000.000.000.000","{origin}"]')
    lines += ["[FlightSimulator_PublishingGroups]", 'cu07="1/1"']
    return "\r\n".join(lines) + "\r\n", len(registered), modules


def by_version(name):
    for record in ADDONS + EXTERNAL + UNMANAGED:
        if record["folder"] == name:
            return "".join(ch for ch in record["version"] if ch.isdigit() or ch == ".")
    return "2.0.1"


USERCFG = """Version 66
{Video
\tAdapter "Display adapter"
\tMonitor 0
\tWindowed 0
\tFullscreenBorderless 1
\tResolution 2560 1440
}
{Sound
\tMasterVolume 0.800000
}
{Graphics
\tPreset HIGH_END
}
InstalledPackagesPath "F:\\MSFS 2024\""""


def sidecar(entry):
    return f"version=1\norigin={entry['origin']}\nquarantined={epoch_ms(entry['days_ago'], entry['at'])}\n"


def write(path, text, newline="\n"):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline=newline) as handle:
        handle.write(text)


def main():
    library = [addon_entry(a) for a in ADDONS]
    external = [outside_entry(e, e["target"], "external") for e in EXTERNAL]
    unmanaged = [outside_entry(u, COMMUNITY + "\\" + u["folder"], "unmanaged") for u in UNMANAGED]
    quarantine = []
    for q in QUARANTINE:
        place = (LIBRARY_QUARANTINE if q["side"] == "library" else DESTINATION_QUARANTINE) + "\\" + q["folder"]
        shaped = outside_entry(q, place, "quarantine")
        shaped["origin"] = q["origin"]
        shaped["sidecar"] = {"path": place + ".fsorg-origin", "text": sidecar(q)}
        quarantine.append(shaped)

    content, content_rows = content_xml()
    report, registered, modules = loading_report()
    preset_files = presets()

    state = DEMO / "state" / "fs-organizer"
    write(state / "settings.json", json.dumps(settings(), indent=4, sort_keys=True, ensure_ascii=False) + "\n")
    write(state / "journal" / "operations.jsonl",
          "".join(json.dumps(r, separators=(",", ":"), sort_keys=True, ensure_ascii=False) + "\n" for r in journal()))
    for name, (body, _) in preset_files.items():
        target = state / "presets" / (f"{PROFILE}.return.json" if name == "__return__" else f"{PROFILE}/{name}.json")
        write(target, json.dumps(body, indent=4, sort_keys=True, ensure_ascii=False) + "\n")

    simulator = DEMO / "simulator" / "Microsoft Flight Simulator 2024"
    write(simulator / "UserCfg.opt", USERCFG, newline="\r\n")
    write(simulator / "Content.xml", content)
    write(simulator / "EXE.xml", exe_xml(), newline="")
    write(simulator / "Report-loading.toml", report, newline="")

    charts = {"icao": "LPFR", "catalogue": [{k: c[k] for k in ("chart_id", "chart_type", "chart_name")} for c in CHARTS]}
    write(DEMO / "files" / "charts" / "LPFR" / "catalogue.json", json.dumps(charts, indent=4) + "\n")

    bgl_dir = DEMO / "files" / "bgl"
    bgl_dir.mkdir(parents=True, exist_ok=True)
    for group in (library, unmanaged, quarantine):
        for entry in group:
            for file in entry["files"]:
                if file["kind"] == "bgl":
                    data = bgl.airport_file(file["airports"], file["record"])
                    assert bgl.read_codes(data) == file["airports"]
                    name = f"{entry['folder']}__{pathlib.PureWindowsPath(file['path']).name}"
                    (bgl_dir / name).write_bytes(data)
                    file["source"] = f"demo/files/bgl/{name}"
                    file["base64"] = base64.b64encode(data).decode()

    catalogue = {
        "about": "Demo installation for the flightsim.to screenshots of FS Organizer. Every add-on, studio and airline is "
                 "fictional; the only real package names are the simulator's own, in simulator.defaultPackages.",
        "anchorDate": ANCHOR.isoformat(),
        "dateRule": "Every date in this folder is written as if today were anchorDate. Shift journal timestamps, sidecar "
                    "'quarantined' values, file modification times and the loading report's TimeUTC by the whole number "
                    "of days between anchorDate and the day the demo is generated.",
        "drive": {"letter": DRIVE, "how": "subst F: <demo root>\\drive, so every path the screens print starts with F:\\"},
        "paths": {"library": LIBRARY, "libraryLabel": "MSFS 2024", "libraryId": LIBRARY_ID, "simulatorRoot": SIMULATOR,
                  "destinations": {"Community": COMMUNITY, "Community2024": COMMUNITY2024},
                  "defaultDestination": "Community", "libraryQuarantine": LIBRARY_QUARANTINE,
                  "destinationQuarantine": DESTINATION_QUARANTINE, "programs": PROGRAMS,
                  "secondProfile": {"variant": "MSFS2020", "library": LIBRARY_2020, "destination": COMMUNITY_2020,
                                    "note": "inactive profile, both folders created empty"},
                  "stateFolder": "demo/state/fs-organizer -> stands in for %LOCALAPPDATA%\\fs-organizer",
                  "simulatorUserFolder": "demo/simulator/Microsoft Flight Simulator 2024 -> stands in for "
                                         "%APPDATA%\\Microsoft Flight Simulator 2024 (Steam edition layout)"},
        "studios": STUDIOS,
        "airlines": AIRLINES,
        "categories": [dict(c, path=LIBRARY + "\\" + c["name"], marker=LIBRARY + "\\" + c["name"] + "\\.fsorg-category")
                       for c in CATEGORIES],
        "library": library,
        "destinationEntries": {"external": external, "unmanaged": unmanaged},
        "quarantine": quarantine,
        "programFiles": PROGRAM_FILES,
        "simulator": {
            "defaultPackages": [{"name": n, "active": s} for n, s in DEFAULT_PACKAGES],
            "defaultPackagesMeasuredFrom": [r"E:\Flight Simulator 2024\StreamedPackages (1115 folders)",
                                            r"%APPDATA%\Microsoft Flight Simulator 2024\<account>\Content.xml (2008 entries)"],
            "contentXml": {"file": "demo/simulator/Microsoft Flight Simulator 2024/Content.xml", "entries": len(content_rows),
                           "order": "sorted by the package name without its generation prefix, as the real list is",
                           "modified": {"daysAgo": 1, "localTime": "22:58"}},
            "exeXml": {"file": "demo/simulator/Microsoft Flight Simulator 2024/EXE.xml", "entries": STARTUP},
            "loadingReport": {"file": "demo/simulator/Microsoft Flight Simulator 2024/Report-loading.toml",
                              "packagesRegistered": registered, "modules": [m[0] for m in modules]},
            "userCfg": "demo/simulator/Microsoft Flight Simulator 2024/UserCfg.opt",
        },
        "state": {
            "settings": "demo/state/fs-organizer/settings.json",
            "journal": "demo/state/fs-organizer/journal/operations.jsonl",
            "presets": {name: {"file": f"demo/state/fs-organizer/presets/{PROFILE}/{name}.json", "modifiedDaysAgo": days}
                        for name, (_, days) in preset_files.items() if name != "__return__"},
            "returnPreset": {"file": f"demo/state/fs-organizer/presets/{PROFILE}.return.json",
                             "modifiedDaysAgo": preset_files["__return__"][1]},
        },
        "totals": {
            "addons": len(library),
            "enabled": sum(1 for a in library if a["enabled"]),
            "disabled": sum(1 for a in library if not a["enabled"]),
            "destinationEntries": sum(1 for a in library if a["enabled"]) + len(external) + len(unmanaged),
            "librarySizeBytes": sum(a["sizeBytes"] for a in library),
        },
    }
    write(OUT / "catalogue.json", json.dumps(catalogue, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps(catalogue["totals"]), f"{catalogue['totals']['librarySizeBytes'] / GiB:.2f} GiB",
          f"content.xml {len(content_rows)} entries, loading report {registered} packages", file=sys.stderr)


if __name__ == "__main__":
    main()
