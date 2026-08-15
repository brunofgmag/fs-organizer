"""Trims the dead ground off the bottom of a screen shot.

`fsorg-shot` writes every screen at one window size, so a screen whose content
stops halfway down carries a band of empty ground that reads as a mistake once
the shot is a figure in a manual. This removes that band and nothing else: it
scans rows from the bottom, stops at the first row that is not a single flat
colour, and keeps a small margin below it so the figure does not look cropped.

    python manual/trim-figures.py manual/figures

It rewrites the PNGs in place and prints what each one lost.
"""

import sys
from pathlib import Path

from PIL import Image

MARGIN = 12
LEAST_WORTH_CROPPING = 24


def flat(row):
    first = row[0]
    return all(pixel == first for pixel in row)


def content_ends_at(image):
    pixels = image.load()
    width, height = image.size

    for y in range(height - 1, -1, -1):
        if not flat([pixels[x, y] for x in range(0, width, 4)]):
            return y

    return height - 1


def trim(path):
    with Image.open(path) as image:
        image = image.convert("RGB")
        width, height = image.size
        wanted = min(height, content_ends_at(image) + 1 + MARGIN)

        if height - wanted < LEAST_WORTH_CROPPING:
            print(f"{path.name}: {width}x{height}, nothing to trim")
            return

        image.crop((0, 0, width, wanted)).save(path)
        print(f"{path.name}: {width}x{height} -> {width}x{wanted}")


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 1

    root = Path(argv[1])

    for path in sorted(root.rglob("*.png")):
        trim(path)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
