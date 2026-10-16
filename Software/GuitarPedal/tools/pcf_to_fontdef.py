#!/usr/bin/env python3
"""Converts a fixed-width X11 bitmap font (a .pcf or .pcf.gz file, such as the public-domain misc-fixed fonts in /usr/share/fonts/X11/misc) into a libDaisy FontDef for the OLED
(the format of util/oled_fonts.c: the characters 32 to 126, a row per 16-bit word, the leftmost pixel in the top bit).

  python3 tools/pcf_to_fontdef.py /usr/share/fonts/X11/misc/8x13.pcf.gz Font_8x13 UI/font_8x13

writes UI/font_8x13.cpp and UI/font_8x13.h and prints a preview of some text. Needs Pillow (PIL).
"""
import gzip
import os
import sys
from PIL import PcfFontFile

FIRST, LAST = 32, 126


def load(path):
    fp = gzip.open(path, "rb") if path.endswith(".gz") else open(path, "rb")
    return PcfFontFile.PcfFontFile(fp)


def glyph_rows(font, code, width, height):
    """The rows of one character as 16-bit words: the glyph image is the cell (width x height) with the glyph in it."""
    image = font.glyph[code][3]
    if image.size != (width, height):
        raise SystemExit("character %r is %s, not %dx%d: not a fixed cell font?" % (chr(code), image.size, width, height))
    rows = []
    for y in range(height):
        word = 0
        for x in range(width):
            if image.getpixel((x, y)):
                word |= 0x8000 >> x
        rows.append(word)
    return rows


def preview(font, width, height, text):
    for y in range(height):
        line = ""
        for ch in text:
            word = glyph_rows(font, ord(ch), width, height)[y]
            line += "".join("#" if word & (0x8000 >> x) else "." for x in range(width))
        print(line)


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    path, name, base = sys.argv[1:4]
    font = load(path)
    width, height = font.glyph[ord("A")][3].size
    copyright_text = font.info.get(b"COPYRIGHT", b"").decode("latin-1")
    font_name = font.info.get(b"FONT", b"").decode("latin-1")
    array = "Font" + name[len("Font_"):] if name.startswith("Font_") else name + "Data"
    lines = []
    for code in range(FIRST, LAST + 1):
        rows = glyph_rows(font, code, width, height)
        lines.append("    " + ", ".join("0x%04X" % r for r in rows) + ",  // " + (chr(code) if chr(code) != "\\" else "backslash"))
    guard = os.path.basename(base).upper() + "_H"
    with open(base + ".h", "w") as f:
        f.write("// %s: a %dx%d bitmap font for the OLED (see tools/pcf_to_fontdef.py).\n#pragma once\n#ifndef %s\n#define %s\n\n#include \"daisy_seed.h\"\n\nusing namespace daisy;\n\n#ifndef %s_PROVIDED_BY_THE_HOST_STUB // (the host tests' stub of libDaisy defines the font itself)\nextern FontDef %s;\n#endif\n\n#endif\n" % (name, width, height, guard, guard, name.upper(), name))
    with open(base + ".cpp", "w") as f:
        f.write("// %s, converted with tools/pcf_to_fontdef.py from the X11 font %s (%s).\n// The characters 32 to 126, %d rows of 16 bits each, the leftmost pixel in the top bit.\n" % (name, font_name, copyright_text, height))
        f.write("#include \"%s\"\n\nusing namespace daisy;\n\nstatic const uint16_t %s[] = {\n" % (os.path.basename(base) + ".h", array))
        f.write("\n".join(lines) + "\n};\n\nFontDef %s = {%d, %d, %s};\n" % (name, width, height, array))
    print("%s: %dx%d, %d characters, %s" % (name, width, height, LAST - FIRST + 1, copyright_text))
    preview(font, width, height, "Matchless SC30")


main()
