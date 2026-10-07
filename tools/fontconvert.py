#!/usr/bin/env python3
"""Convertit une police TrueType en police Adafruit GFX (meme format que fontconvert.c).

Usage : fontconvert.py <ttf> <taille_pt> <nom> [premier] [dernier] [--chars "0123456789"]
Ecrit l'en-tete sur la sortie standard. DPI 141 comme l'outil d'Adafruit.
"""
import argparse
import sys

import freetype

DPI = 141


def principal():
    p = argparse.ArgumentParser()
    p.add_argument("ttf")
    p.add_argument("taille", type=int)
    p.add_argument("nom")
    p.add_argument("premier", type=int, nargs="?", default=32)
    p.add_argument("dernier", type=int, nargs="?", default=126)
    p.add_argument("--chars", help="jeu explicite de caracteres (contigu apres tri, les trous sont vides)")
    a = p.parse_args()

    face = freetype.Face(a.ttf)
    face.set_char_size(a.taille << 6, 0, DPI, DPI)

    if a.chars:
        codes = sorted(set(ord(c) for c in a.chars))
        premier, dernier = codes[0], codes[-1]
        voulus = set(codes)
    else:
        premier, dernier = a.premier, a.dernier
        voulus = set(range(premier, dernier + 1))

    bitmap = bytearray()
    glyphes = []
    for code in range(premier, dernier + 1):
        if code not in voulus:
            glyphes.append((len(bitmap), 0, 0, 0, 0, 0))
            continue
        face.load_char(chr(code), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
        g = face.glyph
        bm = g.bitmap
        largeur, hauteur, pas = bm.width, bm.rows, bm.pitch
        offset = len(bitmap)
        bits = []
        for y in range(hauteur):
            for x in range(largeur):
                octet = bm.buffer[y * pas + (x >> 3)]
                bits.append((octet >> (7 - (x & 7))) & 1)
        for i in range(0, len(bits), 8):
            paquet = bits[i:i + 8]
            valeur = 0
            for b in paquet:
                valeur = (valeur << 1) | b
            valeur <<= 8 - len(paquet)
            bitmap.append(valeur)
        glyphes.append((offset, largeur, hauteur, g.advance.x >> 6, g.bitmap_left, 1 - g.bitmap_top))

    y_advance = face.size.height >> 6
    out = sys.stdout
    out.write("#pragma once\n\n")
    out.write(f"// Genere par tools/fontconvert.py depuis {a.ttf.split('/')[-1]} a {a.taille} pt, caracteres {premier}..{dernier}\n")
    out.write("#include <gfxfont.h>\n\n")
    out.write(f"static const uint8_t {a.nom}Bitmaps[] PROGMEM = {{\n")
    for i in range(0, len(bitmap), 16):
        out.write("  " + ", ".join(f"0x{b:02X}" for b in bitmap[i:i + 16]) + ",\n")
    out.write("};\n\n")
    out.write(f"static const GFXglyph {a.nom}Glyphs[] PROGMEM = {{\n")
    for code, (offset, w, h, adv, xo, yo) in zip(range(premier, dernier + 1), glyphes):
        c = chr(code) if 32 <= code < 127 else ""
        out.write(f"  {{{offset}, {w}, {h}, {adv}, {xo}, {yo}}},  // 0x{code:02X}{' ' + repr(c) if c else ''}\n")
    out.write("};\n\n")
    out.write(f"static const GFXfont {a.nom} PROGMEM = {{(uint8_t*){a.nom}Bitmaps, (GFXglyph*){a.nom}Glyphs, 0x{premier:02X}, 0x{dernier:02X}, {y_advance}}};\n")
    sys.stderr.write(f"{a.nom}: {len(bitmap)} octets de bitmap, {dernier - premier + 1} glyphes, yAdvance {y_advance}\n")


if __name__ == "__main__":
    principal()
