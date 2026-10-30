#!/usr/bin/env python3
"""Xbox title/save image (XPR0 .xbx, DXT1 or linear A8R8G8B8) -> Windows .ico (PNG entries) and a preview .png.

    python tools/xbx_to_ico.py TitleImage.xbx res/hppoa.ico [preview.png]
    python tools/xbx_to_ico.py game/default.xbe hppoa.ico     (the title image in the XBE's $$XTIMAGE section)

The build runs the second form (CMakeLists.txt), so the icon comes from the player's own game files and no game
art is kept in the repository.

The .ico holds 16, 24, 32, 48, 64, 128 and 256 px images: downscales are box filtered, 256 is the picture
doubled smoothly (titles ship 128 px at most). Standard library only (MSYS2's python has no Pillow).
"""
import struct
import sys
import zlib


def rgb565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def decode_dxt1(data, w, h):
    px = [[(0, 0, 0, 0)] * w for _ in range(h)]
    o = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            c0, c1, bits = struct.unpack_from("<HHI", data, o)
            o += 8
            a, b = rgb565(c0), rgb565(c1)
            if c0 > c1:
                pal = [a + (255,), b + (255,),
                       tuple((2 * x + y) // 3 for x, y in zip(a, b)) + (255,),
                       tuple((x + 2 * y) // 3 for x, y in zip(a, b)) + (255,)]
            else:
                pal = [a + (255,), b + (255,), tuple((x + y) // 2 for x, y in zip(a, b)) + (255,), (0, 0, 0, 0)]
            for i in range(16):
                y, x = by + i // 4, bx + i % 4
                if y < h and x < w:
                    px[y][x] = pal[(bits >> (2 * i)) & 3]
    return px


def unswizzle_index(x, y, w, h):
    """Xbox swizzled (Morton) texel order for a linear format."""
    i = bit = 0
    m = 1
    while (1 << bit) < w or (1 << bit) < h:
        if (1 << bit) < w:
            if x & (1 << bit):
                i |= m
            m <<= 1
        if (1 << bit) < h:
            if y & (1 << bit):
                i |= m
            m <<= 1
        bit += 1
    return i


def xbe_section(raw, want):
    """a section's raw bytes from an XBE image, or None"""
    base, count, headers = (struct.unpack_from("<I", raw, o)[0] for o in (0x104, 0x11C, 0x120))
    for i in range(count):
        h = headers - base + 56 * i
        _, _, _, raw_addr, raw_size, name_addr = struct.unpack_from("<6I", raw, h)
        n = name_addr - base
        name = raw[n:raw.index(b"\0", n)].decode("ascii", "replace")
        if name == want:
            return raw[raw_addr:raw_addr + raw_size]
    return None


def load_xbx(path):
    raw = open(path, "rb").read()
    if raw[:4] == b"XBEH":
        raw = xbe_section(raw, "$$XTIMAGE")
        if not raw:
            sys.exit(f"{path}: no $$XTIMAGE section (no title image)")
    if raw[:4] != b"XPR0":
        sys.exit(f"{path}: not an XPR0 file")
    header = struct.unpack_from("<I", raw, 8)[0]
    fmt = struct.unpack_from("<I", raw, 24)[0]
    kind = (fmt >> 8) & 0xFF
    w, h = 1 << ((fmt >> 20) & 15), 1 << ((fmt >> 24) & 15)
    data = raw[header:]
    if kind == 0x0C:  # DXT1
        return decode_dxt1(data, w, h), w, h
    if kind in (0x06, 0x07):  # A8R8G8B8 / X8R8G8B8, swizzled
        px = [[None] * w for _ in range(h)]
        for y in range(h):
            for x in range(w):
                b, g, r, a = data[4 * unswizzle_index(x, y, w, h):][:4]
                px[y][x] = (r, g, b, a if kind == 0x06 else 255)
        return px, w, h
    sys.exit(f"{path}: texture format 0x{kind:02X} not handled")


def resample(px, w, h, n):
    """box filter down, bilinear up; colour weighted by alpha so transparent texels don't darken edges"""
    out = [[None] * n for _ in range(n)]
    if n <= w:
        f = w // n
        for y in range(n):
            for x in range(n):
                acc = [0, 0, 0, 0]
                for yy in range(y * f, y * f + f):
                    for xx in range(x * f, x * f + f):
                        r, g, b, a = px[yy][xx]
                        acc[0] += r * a; acc[1] += g * a; acc[2] += b * a; acc[3] += a
                a = acc[3]
                out[y][x] = (acc[0] // a, acc[1] // a, acc[2] // a, a // (f * f)) if a else (0, 0, 0, 0)
        return out
    s = (w - 1) / (n - 1)
    for y in range(n):
        for x in range(n):
            fx, fy = x * s, y * s
            x0, y0 = int(fx), int(fy)
            x1, y1 = min(x0 + 1, w - 1), min(y0 + 1, h - 1)
            tx, ty = fx - x0, fy - y0
            acc = [0.0] * 4
            for (xx, yy, wt) in ((x0, y0, (1 - tx) * (1 - ty)), (x1, y0, tx * (1 - ty)),
                                 (x0, y1, (1 - tx) * ty), (x1, y1, tx * ty)):
                r, g, b, a = px[yy][xx]
                acc[0] += r * a * wt; acc[1] += g * a * wt; acc[2] += b * a * wt; acc[3] += a * wt
            a = acc[3]
            out[y][x] = (round(acc[0] / a), round(acc[1] / a), round(acc[2] / a), round(a)) if a else (0, 0, 0, 0)
    return out


def png(px, n):
    rows = b"".join(b"\0" + bytes(c for p in row for c in p) for row in px)
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    px, w, h = load_xbx(sys.argv[1])
    if w != h:
        sys.exit("non-square image")
    sizes = [16, 24, 32, 48, 64, 128, 256]
    images = [png(resample(px, w, h, n) if n != w else px, n) for n in sizes]
    head = struct.pack("<HHH", 0, 1, len(images))
    off = 6 + 16 * len(images)
    entries = b""
    for n, im in zip(sizes, images):
        entries += struct.pack("<BBBBHHII", n % 256, n % 256, 0, 0, 1, 32, len(im), off)
        off += len(im)
    with open(sys.argv[2], "wb") as f:
        f.write(head + entries + b"".join(images))
    if len(sys.argv) > 3:
        with open(sys.argv[3], "wb") as f:
            f.write(png(resample(px, w, h, 256), 256))
    print(f"{sys.argv[2]}: {w}x{h} source, sizes {sizes}")


if __name__ == "__main__":
    main()
