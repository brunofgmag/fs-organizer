import html
import pathlib
import subprocess
import sys
import tempfile

from pypdf import PdfReader

HERE = pathlib.Path(__file__).resolve().parent
DOCS = HERE / "docs"
PDF = HERE.parent / "demo" / "files" / "pdf"
CHROME = pathlib.Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe")
PROFILE = tempfile.mkdtemp(prefix="store-shots-chrome-")

E = html.escape


def page(title, accent, body, css="manual.css", extra_css=""):
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{E(title)}</title>
<link rel="stylesheet" href="{css}">
<style>:root {{ --accent: {accent[0]}; --accent-soft: {accent[1]}; }}{extra_css}</style>
</head>
<body>
{body}
</body>
</html>
"""


def cover(studio, product, document, art, edition, notice="For simulation use only."):
    return f"""<section class="cover">
<div class="band"><div class="studio">{E(studio)}</div><div class="product">{E(product)}</div><div class="doc">{E(document)}</div></div>
<div class="art">{art}</div>
<div><div class="meta"><span>{E(edition)}</span><span>{E(studio)}</span></div><div class="notice">{E(notice)}</div></div>
</section>"""


def jet_art(color, engines=2, tail=""):
    engines_svg = "".join(
        f'<ellipse cx="{x}" cy="{y}" rx="16" ry="7" fill="{color}" opacity="0.85"/>'
        for x, y in ([(215, 150), (365, 150)] if engines == 2 else [(205, 150), (375, 150), (250, 158), (330, 158)]))
    return f"""<svg width="150mm" height="95mm" viewBox="0 0 580 300" xmlns="http://www.w3.org/2000/svg">
<defs><linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{color}" stop-opacity="0.10"/><stop offset="1" stop-color="{color}" stop-opacity="0.02"/></linearGradient></defs>
<rect x="0" y="0" width="580" height="300" fill="url(#sky)"/>
<path d="M290 40 C300 40 306 60 306 90 L306 128 L520 176 L520 192 L306 170 L302 236 L346 262 L346 274 L290 262 L234 274 L234 262 L278 236 L274 170 L60 192 L60 176 L274 128 L274 90 C274 60 280 40 290 40 Z" fill="{color}"/>
{engines_svg}
<text x="290" y="292" text-anchor="middle" font-family="Bahnschrift, Segoe UI" font-size="13" fill="{color}" letter-spacing="3">{E(tail)}</text>
</svg>"""


def prop_art(color, tail=""):
    return f"""<svg width="150mm" height="95mm" viewBox="0 0 580 300" xmlns="http://www.w3.org/2000/svg">
<rect x="0" y="0" width="580" height="300" fill="{color}" opacity="0.05"/>
<rect x="80" y="112" width="420" height="16" rx="8" fill="{color}"/>
<path d="M290 50 C300 50 305 70 305 100 L305 210 L340 236 L340 248 L290 240 L240 248 L240 236 L275 210 L275 100 C275 70 280 50 290 50 Z" fill="{color}"/>
<rect x="286" y="34" width="8" height="22" fill="{color}"/><rect x="250" y="40" width="80" height="4" rx="2" fill="{color}" opacity="0.7"/>
<text x="290" y="290" text-anchor="middle" font-family="Bahnschrift, Segoe UI" font-size="13" fill="{color}" letter-spacing="3">{E(tail)}</text>
</svg>"""


def checklist(title, rows):
    body = "".join(f'<div class="row"><span class="item">{E(item)}</span><span class="dots"></span>'
                   f'<span class="action">{E(action)}</span></div>' for item, action in rows)
    return f'<div class="checklist"><div class="title">{E(title)}</div>{body}</div>'


def table(head, rows, numeric=()):
    th = "".join(f'<th class="{"num" if i in numeric else ""}">{E(h)}</th>' for i, h in enumerate(head))
    tr = "".join("<tr>" + "".join(f'<td class="{"num" if i in numeric else ""}">{E(str(c))}</td>' for i, c in enumerate(row))
                 + "</tr>" for row in rows)
    return f"<table><thead><tr>{th}</tr></thead><tbody>{tr}</tbody></table>"


def para(*texts):
    return "".join(f"<p>{E(t)}</p>" for t in texts)


def boxed(kind, text):
    return f'<div class="{kind}"><b>{kind}</b>{E(text)}</div>'


def overhead_figure(color):
    cells = []
    labels = [["ENG 1 FIRE", "HYD", "ENG 2 FIRE"], ["ADIRS", "ELEC", "FUEL"], ["AIR COND", "ANTI ICE", "EXT LT"]]
    for row, line in enumerate(labels):
        for column, label in enumerate(line):
            x, y = 20 + column * 180, 20 + row * 70
            cells.append(f'<rect x="{x}" y="{y}" width="160" height="56" rx="4" fill="none" stroke="{color}" stroke-width="1.5"/>'
                         f'<text x="{x + 80}" y="{y + 20}" text-anchor="middle" font-size="11" font-family="Bahnschrift" fill="{color}">{label}</text>'
                         + "".join(f'<rect x="{x + 18 + k * 44}" y="{y + 30}" width="34" height="16" rx="2" fill="{color}" opacity="{0.25 + 0.2 * ((k + row + column) % 3)}"/>'
                                   for k in range(3)))
    return f'<svg width="160mm" height="58mm" viewBox="0 0 560 230" xmlns="http://www.w3.org/2000/svg">{"".join(cells)}</svg>'


def fcom():
    accent = ("#1f4e8c", "#e8eef7")
    c = "#1f4e8c"
    body = cover("Halcyon Flightworks", "A320neo", "Flight Crew Operating Manual", jet_art(c, tail="HALCYON A320NEO"),
                 "Revision 1.8 · September 2026",
                 "For simulation use only. Not for use in real-world flight operations.")
    body += """<div class="page-title">Contents</div><div class="toc">""" + "".join(
        f'<div class="line"><span class="n">{n}</span><span class="t">{E(t)}</span></div>'
        for n, t in [("1", "General"), ("2", "Aircraft Systems"), ("3", "Normal Procedures"), ("4", "Performance"),
                     ("5", "Limitations")]) + "</div>"
    body += para("This manual describes the Halcyon A320neo as it is simulated. It follows the order a crew meets the "
                 "aircraft: what it is, how its systems behave, how it is flown on a normal day, what it can lift and "
                 "where its limits are.",
                 "Values are rounded for readability. Where the simulation departs from the published type data, the "
                 "text says so.")
    body += boxed("note", "Every procedure in chapter 3 is also available as an interactive checklist on the EFB.")

    body += '<h1><span class="number">1</span> General</h1>'
    body += "<h2>Introduction</h2>" + para(
        "The A320neo is a twin-engine, single-aisle airliner with fly-by-wire controls and a two-crew cockpit. The "
        "Halcyon model simulates the CFM LEAP-1A variant with sharklets, a two-class cabin for 174 passengers and the "
        "full set of normal and abnormal procedures a line crew uses.",
        "The flight deck is modelled for both seats. Every switch, knob and guarded button is clickable, and the "
        "overhead panel follows the dark cockpit philosophy: in normal operation no light is on.")
    body += "<h2>Principal dimensions</h2>" + table(["Item", "Value"], [
        ("Overall length", "37.57 m"), ("Wingspan (with sharklets)", "35.80 m"), ("Height", "11.76 m"),
        ("Wheel track", "7.59 m"), ("Cabin width", "3.70 m"), ("Turning radius (wingtip)", "20.4 m")])
    body += "<h2>Cockpit layout</h2>" + '<div class="figure">' + overhead_figure(c) + \
            '<div class="caption">Figure 1-1 · Overhead panel, simplified</div></div>'
    body += para("The overhead panel groups the systems by area, fore to aft: engine fire, hydraulics and electrics in the "
                 "centre, air conditioning and anti-ice aft. Lights on a pushbutton mean the crew must act; a white "
                 "OFF means a system was deliberately switched off.")
    body += "<h2>How to read this manual</h2>" + para(
        "Chapters open with a short description and then list the items in the order they are checked. Items in "
        "capitals are the labels printed on the panel. Notes add information; cautions protect the aircraft; warnings "
        "protect the people on board.")
    body += boxed("caution", "The simulated engines reach operating temperature in about two minutes. Applying takeoff "
                  "thrust earlier shortens their simulated life and raises a maintenance message on the EFB.")

    body += '<h1><span class="number">2</span> Aircraft Systems</h1>'
    for name, text, rows in [
        ("Electrical", "Two engine-driven generators and the APU generator feed two independent AC networks. The "
                       "batteries keep the essential buses alive while the aircraft is cold and dark.",
         [("GEN 1, GEN 2", "90 kVA each"), ("APU GEN", "90 kVA"), ("BAT 1, BAT 2", "23 Ah, 25.5 V minimum"),
          ("Static inverter", "Essential AC on batteries only")]),
        ("Hydraulic", "Green, blue and yellow systems at 3,000 psi. The power transfer unit keeps green and yellow "
                      "pressure balanced when one engine is shut down.",
         [("Green", "Engine 1 pump, PTU"), ("Blue", "Electric pump, RAT"), ("Yellow", "Engine 2 pump, electric pump"),
          ("PTU", "Automatic above 500 psi difference")]),
        ("Fuel", "Two wing tanks and a centre tank. The centre tank feeds first while the wing pumps stay on to keep "
                 "the engines supplied if the centre pumps fail.",
         [("Wing tanks", "13,700 kg total"), ("Centre tank", "6,500 kg"), ("Crossfeed", "Manual, one valve"),
          ("Unusable fuel", "About 60 kg")]),
        ("Air conditioning", "Two packs cooled by ram air supply three zones. Hot air is trimmed per zone from the "
                             "cockpit panel.",
         [("Packs", "2, automatic flow control"), ("Zones", "Cockpit, forward cabin, aft cabin"),
          ("Temperature range", "18 to 30 °C")]),
        ("Autoflight", "Two flight guidance computers drive the autopilot and autothrust. The FCU sets targets; the "
                       "flight mode annunciator shows what the aircraft is really doing.",
         [("AP 1, AP 2", "Either one engaged, both for autoland"), ("A/THR", "Active between idle and CL detent"),
          ("Modes", "SPD, HDG, ALT, V/S, FPA, LOC, G/S, LAND")]),
    ]:
        body += f"<h2>{E(name)}</h2>" + para(text) + table(["Component", "Value"], rows)
        body += para("The simulation models the normal and failed states of every component in the table. Failures can "
                     "be triggered from the EFB failures page or left to occur at random with the wear setting.")

    body += '<h1><span class="number">3</span> Normal Procedures</h1>'
    body += para("Normal procedures are flown from memory and confirmed with the checklist. The pilot flying calls, the "
                 "pilot monitoring responds and moves the control. The flows below are grouped by phase.")
    body += checklist("Cockpit preparation", [("BAT 1, BAT 2", "AUTO"), ("EXT PWR", "ON"), ("ADIRS 1, 2, 3", "NAV"),
                                              ("NAV and LOGO lights", "ON"), ("Fuel quantity", "CHECK"),
                                              ("Oxygen crew supply", "ON"), ("FMS", "INITIALISED"),
                                              ("Takeoff data", "SET AND CROSS-CHECKED")])
    body += checklist("Before start", [("Cockpit preparation", "COMPLETED"), ("Seat belts sign", "ON"),
                                       ("Doors", "CLOSED"), ("Beacon", "ON"), ("Thrust levers", "IDLE"),
                                       ("Parking brake", "AS REQUIRED"), ("Transponder", "AUTO")])
    body += boxed("note", "The APU bleed can stay on for engine start. Switch it off after both engines are stable to "
                  "save fuel.")
    body += "<h2>Engine start</h2>" + para(
        "Engine 2 is started first so the yellow system powers the brakes while the ground crew is still near the "
        "nose. The start is automatic: the FADEC opens the start valve, adds fuel at the right N2 and aborts on its own "
        "if the temperature or the rotation is wrong.")
    body += checklist("Engine start", [("ENG MODE selector", "IGN/START"), ("ENG 2 master switch", "ON"),
                                       ("N2, EGT, fuel flow", "MONITOR"), ("ENG 1 master switch", "ON"),
                                       ("ENG MODE selector", "NORM"), ("APU bleed", "OFF")])
    body += "<h2>Taxi and takeoff</h2>" + para(
        "Taxi on the nose-wheel tiller with a maximum of 30 knots in a straight line and 10 knots in turns. Before "
        "entering the runway, check the takeoff configuration with the T.O CONFIG button.")
    body += checklist("Before takeoff", [("Flight controls", "CHECKED"), ("Flaps", "SET"), ("T.O CONFIG", "TEST"),
                                         ("Cabin", "READY"), ("Strobe lights", "ON"), ("TCAS", "TA/RA")])

    body += '<h1><span class="number">4</span> Performance</h1>'
    body += para("The tables give approximate takeoff speeds for a dry runway at sea level with CONF 1+F and TOGA "
                 "thrust. The EFB performance page computes exact values for the day and should be preferred.")
    body += table(["Takeoff mass (t)", "V1 (kt)", "VR (kt)", "V2 (kt)", "Field length (m)"],
                  [(56, 126, 127, 132, 1560), (60, 131, 132, 137, 1690), (64, 136, 137, 142, 1830),
                   (68, 141, 142, 146, 1990), (72, 146, 147, 151, 2170), (76, 151, 152, 156, 2370),
                   (79, 154, 155, 159, 2520)], numeric=(0, 1, 2, 3, 4))
    body += "<h2>Landing</h2>" + table(["Landing mass (t)", "VLS CONF FULL (kt)", "VAPP (kt)", "Landing distance (m)"],
                                       [(52, 118, 123, 1180), (56, 122, 127, 1240), (60, 126, 131, 1310),
                                        (64, 130, 135, 1390), (67, 133, 138, 1450)], numeric=(0, 1, 2, 3))
    body += boxed("caution", "Add 5 knots to VAPP in gusty wind, and never less than a third of the steady headwind.")

    body += '<h1><span class="number">5</span> Limitations</h1>'
    body += table(["Limit", "Value"], [
        ("Maximum takeoff mass", "79,000 kg"), ("Maximum landing mass", "67,400 kg"), ("Maximum zero fuel mass", "64,300 kg"),
        ("VMO / MMO", "350 kt / M 0.82"), ("Maximum operating altitude", "39,800 ft"),
        ("Maximum crosswind, takeoff and landing", "38 kt"), ("Maximum tailwind", "15 kt"),
        ("Runway slope", "±2 %"), ("Maximum taxi speed in a turn", "10 kt")])
    body += boxed("warning", "Exceeding the limits above in the simulation triggers the corresponding structural or "
                  "engine failure when wear is enabled.")
    return page("Halcyon A320neo FCOM", accent, body)


def quick_start():
    accent = ("#1f4e8c", "#e8eef7")
    c = "#1f4e8c"
    body = cover("Halcyon Flightworks", "A320neo", "Quick Start Guide", jet_art(c, tail="QUICK START"),
                 "Version 1.8.4 · September 2026")
    steps = [
        ("Welcome", "Thank you for flying the Halcyon A320neo. This guide takes you from a cold and dark aircraft to "
                    "the runway in about twenty minutes. The full procedures are in the Flight Crew Operating Manual."),
        ("Installing", "Copy or link the halcyon-aircraft-a320neo folder into the simulator's Community folder. Keep "
                       "the folder name exactly as it is: the livery packs look for it by name."),
        ("First flight", "Start at a gate with the engines off. Open the EFB with the tablet button on the left side "
                         "panel and choose a fuel and payload preset."),
        ("Cold and dark", "Batteries to AUTO, external power ON, and wait for the EFB to show the aircraft as ready."),
        ("Aligning the IRS", "Set the three ADIRS selectors to NAV. Alignment takes seven minutes, or fifteen seconds "
                             "with fast alignment on the EFB."),
        ("Programming the FMS", "Enter the origin and destination, load the flight plan from the EFB and complete "
                                "the performance page with the speeds it proposes."),
        ("Pushback and start", "Ask for pushback from the EFB ground page. Start engine 2, then engine 1, with the "
                               "ENG MODE selector on IGN/START."),
        ("Taxi", "Release the parking brake, add a little thrust and steer with the tiller. Keep 30 knots at most."),
        ("Takeoff", "Line up, set the thrust levers to FLX or TOGA and let the autothrust take over at 80 knots."),
        ("Climb and cruise", "Engage AP 1 above 100 ft. The aircraft follows the managed profile to cruise level."),
        ("Approach and landing", "Start down at the top of descent the FMS shows and activate the approach phase ten "
                                 "miles before the deceleration point. Arm the ground spoilers, select autobrake LO or "
                                 "MED and fly the aircraft to the touchdown zone."),
    ]
    for index, (title, text) in enumerate(steps, start=1):
        body += f'<div class="page-title">{index:02d} · {E(title)}</div>'
        body += f'<div class="big-step"><div class="n">{index}</div><div>{para(text)}</div></div>'
        body += para("The EFB shows the same step with the switches highlighted in the virtual cockpit. Press "
                     "the arrow on the tablet to move on, or open the checklist page to tick each item yourself.")
        body += table(["Control", "Where"], [("EFB", "Left side panel"), ("Checklist", "EFB · Checklists"),
                                             ("Ground services", "EFB · Ground")])
    body += '<div class="page-title">12 · Troubleshooting</div>'
    body += table(["What you see", "Why", "What to do"], [
        ("The EFB stays black", "The aircraft has no power yet", "Batteries to AUTO, then external power ON"),
        ("The FMS rejects the flight plan", "The navigation data is older than the plan", "Update the navigation data and load the plan again"),
        ("The engines do not start", "The APU bleed is off or the ENG MODE is on NORM", "APU bleed ON, ENG MODE to IGN/START"),
        ("Liveries are missing", "The livery pack is not in the same destination as the aircraft", "Enable both in the same Community folder"),
        ("The aircraft pulls to one side on taxi", "Asymmetric thrust or a brake left on", "Check both thrust levers and release the parking brake"),
        ("The autopilot does not engage", "A flight director or the ADIRS are off", "FD ON and ADIRS on NAV, then try again"),
        ("Sounds are missing inside", "A sound pack from another developer is overriding the files", "Disable the sound pack and check again"),
    ])
    body += boxed("note", "Most problems are fixed by starting the flight again from the gate with the aircraft cold and "
                  "dark. Your EFB settings are kept.")
    body += '<div class="page-title">13 · Getting help</div>' + para(
        "The support forum answers questions within two working days. Include the version shown on the EFB about page "
        "and the list of other add-ons you had enabled.")
    body += '<div class="page-title">14 · Credits</div>' + para(
        "Flight model, systems, 3D, textures and sound: the Halcyon Flightworks team. Testing: the Halcyon beta group.")
    body += '<div class="page-title">15 · Legal</div>' + para(
        "This product is for simulation only. It is not endorsed by, and has no connection with, any aircraft "
        "manufacturer or airline.")
    return page("Halcyon A320neo Quick Start Guide", accent, body)


def single_chapter(studio, product, document, accent, art, edition, chapter, sections):
    body = cover(studio, product, document, art, edition)
    body += f"<h1>{E(chapter)}</h1>"
    for title, paragraphs, rows in sections:
        body += f"<h2>{E(title)}</h2>" + para(*paragraphs)
        if rows:
            body += table(["Item", "Value"], rows)
    return page(f"{studio} {product} {document}", accent, body)


def pilot_guide():
    return single_chapter(
        "Cobalt Ridge Simulations", "C172 Classic", "Pilot Guide", ("#2446a8", "#e9edf9"),
        prop_art("#2446a8", "C172 CLASSIC"), "Version 2.3 · 2026", "C172 Classic Pilot Guide", [
            ("About the aircraft", ["A four-seat, high-wing trainer with a carburetted piston engine and steam gauges.",
                                    "The Classic model reproduces a well-used club aircraft, dents and all."],
             [("Engine", "160 hp, four cylinders"), ("Fuel", "40 US gal usable"), ("Cruise", "110 kt TAS")]),
            ("Before flight", ["Walk around the aircraft on the EFB and drain the fuel sumps.",
                               "Check the oil level and the tyre pressure before every flight."], None),
            ("Starting the engine", ["Mixture rich, carburettor heat cold, prime three strokes in cold weather.",
                                     "Throttle open a quarter inch, clear the propeller, master on, key to START."],
             [("Oil pressure", "Green within 30 s"), ("Idle", "About 800 rpm")]),
            ("Takeoff and climb", ["Flaps up for a normal takeoff. Rotate at 55 kt and climb at 74 kt."],
             [("Vr", "55 kt"), ("Vy", "74 kt"), ("Vx", "62 kt")]),
            ("Landing", ["Downwind at 80 kt, base at 70 kt, final at 65 kt with full flaps."],
             [("Vref full flaps", "65 kt"), ("Stall full flaps", "40 kt")]),
        ])


def boeing_intro():
    return single_chapter(
        "Halcyon Flightworks", "737-800", "Introduction", ("#1f4e8c", "#e8eef7"),
        jet_art("#1f4e8c", tail="HALCYON 737-800"), "Version 3.0 · 2026", "737-800 Introduction", [
            ("Welcome", ["The Halcyon 737-800 models the short-field, winglet-equipped variant with a 189-seat cabin.",
                         "This introduction covers installation, the EFB and the first flight."], None),
            ("Installation", ["Keep the folder name halcyon-aircraft-737-800. Livery packs and the sound pack look for it."],
             None),
            ("The EFB", ["Weights, fuel, ground services and failures are set from the tablet on the captain's side."],
             [("Open", "Click the tablet"), ("Pin", "Right-click the tablet")]),
            ("Your first flight", ["Start cold and dark at a gate and follow the tutorial on the EFB."], None),
        ])


def a330_guide():
    accent = ("#0f6b6b", "#e4f2f1")
    c = "#0f6b6b"
    body = cover("Quillon Aerospace", "A330-900", "User Guide", jet_art(c, tail="QUILLON A330-900"),
                 "Version 1.2 · August 2026")
    chapters = [
        ("Getting started", ["Install the package in Community2024: it is built for Microsoft Flight Simulator 2024 and "
                             "uses its native systems.", "Start the first flight from the gate with the tablet open."]),
        ("Cockpit tour", ["The cockpit follows the A330 family layout with the neo's updated displays. Every panel is "
                          "clickable and the tablet explains each one."]),
        ("Systems", ["Electrical, hydraulic, fuel and pressurisation systems are simulated with their failures."]),
        ("Flying the aircraft", ["Long-haul operation: step climbs, fuel planning and ETOPS alternates on the FMS."]),
        ("Support", ["Report problems with the version on the tablet about page and a list of your other add-ons."]),
    ]
    for title, texts in chapters:
        body += f"<h1>{E(title)}</h1>" + para(*texts)
        body += table(["Item", "Value"], [("Range", "7,200 nm"), ("Engines", "2 × 72,000 lbf"),
                                          ("Seats", "287, three classes"), ("MTOW", "251 t")])
        body += para("The tablet mirrors every item of this chapter with the relevant switches highlighted in the "
                     "virtual cockpit, so the guide can stay closed during the flight.")
    return page("Quillon A330-900 User Guide", accent, body)


def scenery_manual():
    accent = ("#20626a", "#e3f0f1")
    art = """<svg width="150mm" height="95mm" viewBox="0 0 580 300" xmlns="http://www.w3.org/2000/svg">
<rect width="580" height="300" fill="#20626a" opacity="0.06"/>
<g transform="rotate(-8 290 150)"><rect x="40" y="170" width="500" height="26" fill="#20626a"/>
<g stroke="#ffffff" stroke-width="3" stroke-dasharray="18 14"><line x1="60" y1="183" x2="520" y2="183"/></g>
<rect x="130" y="96" width="260" height="46" fill="#20626a" opacity="0.55"/><rect x="200" y="60" width="18" height="36" fill="#20626a"/>
<g fill="#20626a" opacity="0.35">""" + "".join(f'<rect x="{140 + i * 24}" y="146" width="14" height="18"/>' for i in range(10)) + """</g></g>
<text x="290" y="285" text-anchor="middle" font-family="Bahnschrift" font-size="13" fill="#20626a" letter-spacing="3">LPFR · FARO</text></svg>"""
    body = cover("Tessellate Airports", "LPFR Faro", "Scenery Manual", art, "Version 1.3.2 · 2026")
    for title, texts in [
        ("Introduction", ["Faro Airport serves the Algarve coast of southern Portugal. This scenery models the whole "
                          "airport with its terminal, apron, cargo area and the lagoon to the south."]),
        ("Installation", ["Keep the folder in your Community folder and disable any other scenery of LPFR, including "
                          "the one that comes with the simulator, so the two do not overlap."]),
        ("Features", ["Ground polygons from recent imagery, custom terminal interiors, animated jetways and "
                      "night lighting tuned for approaches over the water."]),
        ("Performance", ["Textures are streamed in three levels of detail. On systems with 8 GB of video memory, "
                         "choose the medium texture preset in the scenery configurator."]),
        ("Credits", ["Modelling and texturing by the Tessellate Airports team."]),
    ]:
        body += f"<h1>{E(title)}</h1>" + para(*texts)
    return page("Tessellate LPFR Scenery Manual", accent, body)


def efb_guide():
    accent = ("#2e7d4f", "#e6f3eb")
    art = """<svg width="120mm" height="90mm" viewBox="0 0 400 300" xmlns="http://www.w3.org/2000/svg">
<rect x="70" y="20" width="260" height="260" rx="22" fill="#2e7d4f"/><rect x="88" y="40" width="224" height="210" rx="6" fill="#ffffff"/>
<g fill="#2e7d4f" opacity="0.8"><rect x="104" y="58" width="90" height="60" rx="6"/><rect x="206" y="58" width="90" height="60" rx="6"/>
<rect x="104" y="130" width="90" height="60" rx="6"/><rect x="206" y="130" width="90" height="60" rx="6"/></g>
<rect x="104" y="202" width="192" height="30" rx="6" fill="#2e7d4f" opacity="0.35"/></svg>"""
    body = cover("Vireo Avionics", "Vireo EFB", "User Guide", art, "Version 2.4 · 2026")
    for title, texts in [("Overview", ["A tablet for any aircraft: charts, weights, checklists and a moving map."]),
                         ("Installing", ["Link vireo-efb-tablet into Community and start Vireo Hub once."]),
                         ("Apps", ["Maps, Weights, Checklists, Notes and Settings, each on its own tile."]),
                         ("Updating", ["Vireo Hub checks for a new version when the simulator starts."])]:
        body += f"<h1>{E(title)}</h1>" + para(*texts)
    return page("Vireo EFB User Guide", accent, body)


CHART_CSS = """
@page { size: A5; margin: 7mm; }
html, body { margin: 0; padding: 0; }
body { font-family: "Bahnschrift", "Segoe UI", Arial, sans-serif; color: #111; font-size: 8pt; }
.frame { border: 1.2pt solid #111; height: 192mm; overflow: hidden; display: flex; flex-direction: column; }
.head { display: grid; grid-template-columns: 1fr auto; border-bottom: 1.2pt solid #111; }
.head .left { padding: 2mm 3mm; } .head .code { font-size: 15pt; font-weight: 700; letter-spacing: 0.06em; }
.head .name { font-size: 9pt; } .head .right { border-left: 1.2pt solid #111; padding: 2mm 3mm; text-align: right; }
.head .type { font-size: 13pt; font-weight: 700; } .head .small { font-size: 7pt; color: #333; }
.freq { display: grid; grid-template-columns: repeat(4, 1fr); border-bottom: 0.8pt solid #111; }
.freq div { padding: 1.2mm 2mm; border-right: 0.5pt solid #111; } .freq div:last-child { border-right: none; }
.freq b { display: block; font-size: 6.5pt; letter-spacing: 0.08em; } .drawing { flex: 1; min-height: 0; position: relative; }
.drawing > svg { position: absolute; inset: 0; width: 100%; height: 100%; }
.foot { border-top: 0.8pt solid #111; display: flex; justify-content: space-between; padding: 1.2mm 3mm; font-size: 6.5pt; }
"""


def chart(kind, title, drawing, freqs):
    freq = "".join(f"<div><b>{E(a)}</b>{E(b)}</div>" for a, b in freqs)
    body = f"""<div class="frame">
<div class="head"><div class="left"><div class="code">LPFR / FAO</div><div class="name">FARO · PORTUGAL · ELEV 24 FT</div></div>
<div class="right"><div class="type">{E(kind)}</div><div class="small">{E(title)}</div></div></div>
<div class="freq">{freq}</div>
<div class="drawing">{drawing}</div>
<div class="foot"><span>Tessellate Airports · scenery chart set 1.3</span><span>SIMULATION USE ONLY · NOT FOR NAVIGATION</span></div>
</div>"""
    return f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><title>LPFR {E(kind)}</title>
<style>{CHART_CSS}</style></head><body>{body}</body></html>"""


def adc():
    stands = "".join(
        f'<rect x="{122 + i * 22}" y="92" width="16" height="26" fill="none" stroke="#111" stroke-width="0.8"/>'
        f'<text x="{130 + i * 22}" y="88" font-size="7" text-anchor="middle">{i + 1}</text>' for i in range(12))
    svg = f"""<svg width="100%" height="100%" viewBox="0 0 420 560" xmlns="http://www.w3.org/2000/svg" font-family="Bahnschrift, Segoe UI">
<rect width="420" height="560" fill="#ffffff"/>
<g stroke="#9ab" stroke-width="0.5" opacity="0.6">{''.join(f'<line x1="{x}" y1="0" x2="{x}" y2="560"/>' for x in range(0, 421, 60))}{''.join(f'<line x1="0" y1="{y}" x2="420" y2="{y}"/>' for y in range(0, 561, 60))}</g>
<path d="M0 470 C80 440 160 500 240 470 C320 440 380 480 420 460 L420 560 L0 560 Z" fill="#dce8f2"/>
<text x="300" y="520" font-size="9" fill="#46607a" font-style="italic">Ria Formosa lagoon</text>
<g transform="translate(210 300) rotate(8) scale(0.86) translate(-210 -300)">
<rect x="30" y="286" width="360" height="22" fill="#b9b9b9" stroke="#111" stroke-width="1"/>
<g stroke="#ffffff" stroke-width="2" stroke-dasharray="10 8"><line x1="50" y1="297" x2="370" y2="297"/></g>
<text x="40" y="327" font-size="11" font-weight="700">10</text><text x="366" y="280" font-size="11" font-weight="700">28</text>
<text x="210" y="330" font-size="8" text-anchor="middle">RWY 10/28 · 2490 × 45 m · ASPH</text>
<g fill="none" stroke="#111" stroke-width="1.4">
<path d="M70 286 L70 240 L330 240 L330 286"/><path d="M150 240 L150 120"/><path d="M250 240 L250 120"/><path d="M200 240 L200 286"/>
</g>
<g font-size="8" font-weight="700"><text x="58" y="262">A</text><text x="138" y="200">B</text><text x="203" y="262">C</text><text x="238" y="200">D</text><text x="334" y="262">E</text></g>
<rect x="110" y="60" width="285" height="62" fill="#efefef" stroke="#111" stroke-width="0.8"/>
<text x="252" y="50" font-size="8" text-anchor="middle">MAIN APRON</text>
{stands}
<rect x="170" y="4" width="150" height="26" fill="#d0d0d0" stroke="#111" stroke-width="0.8"/><text x="245" y="21" font-size="8" text-anchor="middle">TERMINAL</text>
<circle cx="100" cy="150" r="7" fill="none" stroke="#111"/><text x="100" y="170" font-size="7" text-anchor="middle">TWR</text>
<rect x="330" y="150" width="40" height="30" fill="#e3e3e3" stroke="#111" stroke-width="0.8"/><text x="350" y="194" font-size="7" text-anchor="middle">CARGO</text>
</g>
<g transform="translate(370 410)"><circle r="22" fill="none" stroke="#111"/><path d="M0 -20 L6 0 L0 -6 L-6 0 Z" fill="#111"/><text y="-26" font-size="9" text-anchor="middle" font-weight="700">N</text></g>
<g transform="translate(24 500)" font-size="7"><rect width="100" height="4" fill="#111"/><rect x="50" width="50" height="4" fill="#fff" stroke="#111" stroke-width="0.5"/><text y="14">0</text><text x="92" y="14">500 m</text></g>
<text x="24" y="40" font-size="7">ARP 37°00'53"N 007°57'56"W</text><text x="24" y="52" font-size="7">VAR 1°W</text>
</svg>"""
    return chart("AGC", "Aerodrome chart", svg,
                 [("ATIS", "124.925"), ("DELIVERY", "121.975"), ("GROUND", "121.750"), ("TOWER", "120.750")])


def apc():
    rows = "".join(f'<tr><td>{i + 1}</td><td>37°00\'{38 + i // 3:02d}.{(i * 7) % 10}"N</td><td>007°58\'{2 + i:02d}.{(i * 3) % 10}"W</td>'
                   f'<td>{"A321" if i < 4 else ("A320" if i < 9 else "B738")}</td></tr>' for i in range(12))
    drawing = f"""<div style="padding:4mm 5mm"><svg width="100%" viewBox="0 0 400 200" xmlns="http://www.w3.org/2000/svg" font-family="Bahnschrift">
<rect x="10" y="30" width="380" height="120" fill="#efefef" stroke="#111"/>
{''.join(f'<g><rect x="{20 + i * 31}" y="40" width="24" height="44" fill="none" stroke="#111"/><text x="{32 + i * 31}" y="100" font-size="9" text-anchor="middle" font-weight="700">{i + 1}</text><line x1="{32 + i * 31}" y1="46" x2="{32 + i * 31}" y2="84" stroke="#e0a800" stroke-width="1.5"/></g>' for i in range(12))}
<rect x="60" y="0" width="260" height="26" fill="#d0d0d0" stroke="#111"/><text x="190" y="17" font-size="9" text-anchor="middle">TERMINAL</text>
<line x1="10" y1="170" x2="390" y2="170" stroke="#111" stroke-width="1.4"/><text x="200" y="190" font-size="8" text-anchor="middle">TWY B · TWY D</text></svg>
<table style="width:100%;border-collapse:collapse;font-size:7.5pt;margin-top:3mm" border="1" cellpadding="3">
<tr style="background:#eee"><th>STAND</th><th>LAT</th><th>LONG</th><th>MAX</th></tr>{rows}</table></div>"""
    return chart("APC", "Aprons and stands", drawing,
                 [("GROUND", "121.750"), ("APRON", "131.550"), ("DOCKING", "VISUAL"), ("DE-ICING", "N/A")])


def vac():
    svg = """<svg width="100%" height="100%" viewBox="0 0 420 560" xmlns="http://www.w3.org/2000/svg" font-family="Bahnschrift">
<rect width="420" height="560" fill="#f7f4ea"/>
<path d="M0 330 C90 300 150 360 240 330 C320 300 380 340 420 320 L420 560 L0 560 Z" fill="#cfe0ee"/>
<path d="M0 120 C60 90 140 150 220 110 C300 80 360 120 420 100" fill="none" stroke="#b58b4b" stroke-width="1"/>
<path d="M0 160 C80 140 150 190 230 150 C300 120 370 160 420 140" fill="none" stroke="#b58b4b" stroke-width="1"/>
<g transform="rotate(8 210 280)"><rect x="160" y="272" width="100" height="10" fill="#555"/></g>
<circle cx="210" cy="278" r="90" fill="none" stroke="#7a3fa0" stroke-width="1.2" stroke-dasharray="6 4"/>
<text x="210" y="178" font-size="8" fill="#7a3fa0" text-anchor="middle">CTR FARO · SFC-2000 ft</text>
<g fill="#7a3fa0" font-size="8"><path d="M60 200 l8 -14 l8 14 z"/><text x="50" y="214">VRP N (Loule)</text>
<path d="M340 190 l8 -14 l8 14 z"/><text x="318" y="204">VRP E (Olhao)</text>
<path d="M200 470 l8 -14 l8 14 z"/><text x="178" y="484">VRP S (Barra)</text></g>
<g stroke="#111" stroke-width="1.2" fill="none" marker-end="url(#a)"><path d="M68 200 C110 240 150 260 176 270"/><path d="M348 190 C310 230 270 260 248 272"/></g>
<defs><marker id="a" markerWidth="8" markerHeight="8" refX="6" refY="4" orient="auto"><path d="M0 0 L8 4 L0 8 Z" fill="#111"/></marker></defs>
<text x="20" y="30" font-size="9" font-weight="700">VFR ARRIVALS</text><text x="20" y="44" font-size="8">Report VRP, enter CTR at 1000 ft</text>
</svg>"""
    return chart("VAC", "Visual approach chart", svg,
                 [("INFO", "123.500"), ("APPROACH", "119.400"), ("TOWER", "120.750"), ("CIRCUIT", "1000 FT")])


DOCUMENTS = {
    "a320neo-fcom": (fcom, True),
    "a320neo-quick-start": (quick_start, False),
    "737-introduction": (boeing_intro, True),
    "pilot-guide": (pilot_guide, True),
    "a330-user-guide": (a330_guide, True),
    "lpfr-scenery-manual": (scenery_manual, True),
    "vireo-efb-guide": (efb_guide, True),
    "lpfr-adc": (adc, False),
    "lpfr-apc": (apc, False),
    "lpfr-vac": (vac, False),
}


def render(name, outline):
    source = DOCS / f"{name}.html"
    target = PDF / f"{name}.pdf"
    arguments = [str(CHROME), "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--no-first-run",
                 f"--user-data-dir={PROFILE}", f"--print-to-pdf={target}"]
    if outline:
        arguments.append("--generate-pdf-document-outline")
    arguments.append(source.resolve().as_uri())
    subprocess.run(arguments, check=True, capture_output=True)
    reader = PdfReader(str(target))
    top = [item for item in reader.outline if not isinstance(item, list)]
    return len(reader.pages), [item["/Title"] for item in top]


def main():
    PDF.mkdir(parents=True, exist_ok=True)
    for name, (build, outline) in DOCUMENTS.items():
        (DOCS / f"{name}.html").write_text(build(), encoding="utf-8")
    for name, (_, outline) in DOCUMENTS.items():
        pages, top = render(name, outline)
        print(f"{name}: {pages} pages, {len(top)} top-level outline items {top}", file=sys.stderr)


if __name__ == "__main__":
    main()
