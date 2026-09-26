import struct

SIGNATURE = 0x19920201
AIRPORT_SECTION = 3
HEADER = 56
SECTION_ENTRY = 20
SUBSECTION_ENTRY = 16
RECORD = 128
BASE = 38
FIRST_DIGIT = 2
FIRST_LETTER = 12

RECORD_FORMATS = {
    "fs20": {"record": 0x56, "at": 40, "shift": 5},
    "fs24": {"record": 0x113, "at": 76, "shift": 6},
}


def packed_code(code, shift):
    value = 0
    for character in code.upper():
        if character.isdigit():
            digit = FIRST_DIGIT + ord(character) - ord("0")
        elif "A" <= character <= "Z":
            digit = FIRST_LETTER + ord(character) - ord("A")
        else:
            raise ValueError(f"{code} carries a character no airport code carries")
        value = value * BASE + digit
    packed = value << shift
    if packed >= 1 << 32:
        raise ValueError(f"{code} does not fit a 32-bit identifier with a {shift}-bit shift")
    return packed


def code_from(packed, shift):
    left = packed >> shift
    code = ""
    while left > 0:
        digit = left % BASE
        if digit < FIRST_DIGIT:
            return ""
        code = (chr(ord("0") + digit - FIRST_DIGIT) if digit < FIRST_LETTER else chr(ord("A") + digit - FIRST_LETTER)) + code
        left //= BASE
    return code


def airport_file(codes, generation="fs20"):
    layout = RECORD_FORMATS[generation]
    section = HEADER
    subsection = section + SECTION_ENTRY
    first_record = subsection + SUBSECTION_ENTRY
    size = first_record + RECORD * len(codes)
    data = bytearray(size)

    struct.pack_into("<I", data, 0, SIGNATURE)
    struct.pack_into("<I", data, 4, HEADER)
    struct.pack_into("<I", data, 20, 1)

    struct.pack_into("<I", data, section, AIRPORT_SECTION)
    struct.pack_into("<I", data, section + 8, 1)
    struct.pack_into("<I", data, section + 12, subsection)

    struct.pack_into("<I", data, subsection + 4, len(codes))
    struct.pack_into("<I", data, subsection + 8, first_record)
    struct.pack_into("<I", data, subsection + 12, RECORD * len(codes))

    for index, code in enumerate(codes):
        at = first_record + index * RECORD
        struct.pack_into("<H", data, at, layout["record"])
        struct.pack_into("<I", data, at + 2, RECORD)
        struct.pack_into("<I", data, at + layout["at"], packed_code(code, layout["shift"]))

    return bytes(data)


def read_codes(data):
    if len(data) < 24 or struct.unpack_from("<I", data, 0)[0] != SIGNATURE:
        return None
    header = struct.unpack_from("<I", data, 4)[0]
    sections = struct.unpack_from("<I", data, 20)[0]
    known = {layout["record"]: layout for layout in RECORD_FORMATS.values()}
    codes = []
    for section in range(sections):
        entry = header + section * SECTION_ENTRY
        if struct.unpack_from("<I", data, entry)[0] != AIRPORT_SECTION:
            continue
        subsections = struct.unpack_from("<I", data, entry + 8)[0]
        subsections_at = struct.unpack_from("<I", data, entry + 12)[0]
        for subsection in range(subsections):
            where = subsections_at + subsection * SUBSECTION_ENTRY
            records = struct.unpack_from("<I", data, where + 4)[0]
            at = struct.unpack_from("<I", data, where + 8)[0]
            for _ in range(records):
                kind = struct.unpack_from("<H", data, at)[0]
                size = struct.unpack_from("<I", data, at + 2)[0]
                if kind in known:
                    layout = known[kind]
                    codes.append(code_from(struct.unpack_from("<I", data, at + layout["at"])[0], layout["shift"]))
                at += size
    return codes


if __name__ == "__main__":
    fixture = {("EHAM", 5): 0x01BA5181, ("LPMA", 5): 0x027BBA01, ("RCTP", 6): 0x0626E941}
    for (code, shift), expected in fixture.items():
        produced = packed_code(code, shift)
        same = produced >> shift == expected >> shift and code_from(expected, shift) == code
        print(f"{code} shift {shift}: {produced:#010x} against the fixture's {expected:#010x}, "
              f"identifier bits {'agree' if same else 'DISAGREE'} (the low {shift} bits are not the code)")
        assert same
    for generation in RECORD_FORMATS:
        sample = airport_file(["LPFR", "LPPT"], generation)
        assert read_codes(sample) == ["LPFR", "LPPT"], read_codes(sample)
        print(f"{generation}: {len(sample)} bytes, codes read back {read_codes(sample)}")
