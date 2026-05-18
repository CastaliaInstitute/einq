#!/usr/bin/env python3
"""Generate Einq clock demo BMP for CrossPoint custom sleep screen (480x800, 1-bit)."""
from __future__ import annotations

import argparse
import struct
from datetime import datetime
from pathlib import Path


def write_mono_bmp(path: Path, width: int, height: int, rows) -> None:
    row_bytes = ((width + 31) // 32) * 4
    pixel_bytes = row_bytes * height
    file_size = 62 + pixel_bytes
    with path.open("wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<I", file_size))
        f.write(struct.pack("<HH", 0, 0))
        f.write(struct.pack("<I", 62))
        f.write(struct.pack("<I", 40))
        f.write(struct.pack("<iiHHIIiiII", width, height, 1, 1, 0, pixel_bytes, 2835, 2835, 2, 2))
        for row in rows:
            padded = row + bytes(row_bytes - len(row))
            f.write(padded)


def render_face(width: int, height: int, now: datetime) -> list[bytes]:
    """Simple bitmap: large time block + weekday (bitmap font 8x16 scaled)."""
    row_bytes = ((width + 31) // 32) * 4
    buf = bytearray(row_bytes * height)

    def set_px(x: int, y: int, on: bool) -> None:
        if x < 0 or y < 0 or x >= width or y >= height:
            return
        row = height - 1 - y
        idx = row * row_bytes + (x // 8)
        bit = 7 - (x % 8)
        if on:
            buf[idx] |= 1 << bit
        else:
            buf[idx] &= ~(1 << bit)

    def fill_rect(x0: int, y0: int, x1: int, y1: int, on: bool) -> None:
        for y in range(y0, y1):
            for x in range(x0, x1):
                set_px(x, y, on)

    time_s = now.strftime("%H:%M")
    day_s = now.strftime("%A")
    date_s = now.strftime("%Y-%m-%d")

    fill_rect(0, 0, width, height, False)
    glyphs = {
        "0": ["0110", "1001", "1001", "1001", "0110"],
        "1": ["0010", "0110", "0010", "0010", "0111"],
        "2": ["0110", "1001", "0010", "0100", "1111"],
        "3": ["1110", "0001", "0110", "0001", "1110"],
        "4": ["1001", "1001", "1111", "0001", "0001"],
        "5": ["1111", "1000", "1110", "0001", "1110"],
        "6": ["0110", "1000", "1110", "1001", "0110"],
        "7": ["1111", "0001", "0010", "0100", "0100"],
        "8": ["0110", "1001", "0110", "1001", "0110"],
        "9": ["0110", "1001", "0111", "0001", "0110"],
        ":": ["000", "010", "000", "010", "000"],
        "A": ["0110", "1001", "1111", "1001", "1001"],
        "E": ["1111", "1000", "1110", "1000", "1111"],
        "M": ["10001", "11011", "10101", "10001", "10001"],
        "d": ["00110", "01001", "01001", "01001", "11111"],
        "y": ["10001", "10001", "01111", "00010", "11100"],
        "-": ["00000", "00000", "11111", "00000", "00000"],
    }

    def draw_text(text: str, x: int, y: int, scale: int, invert: bool) -> None:
        cx = x
        for ch in text:
            g = glyphs.get(ch, glyphs["-"])
            gh = len(g)
            gw = len(g[0])
            for row_i, row in enumerate(g):
                for col_i, bit in enumerate(row):
                    if bit == "1":
                        fill_rect(cx + col_i * scale, y + row_i * scale, cx + (col_i + 1) * scale, y + (row_i + 1) * scale, invert)
            cx += (gw + 1) * scale

    draw_text(time_s, 70, 120, 14, True)
    draw_text(day_s[:3].upper(), 120, 420, 8, True)
    draw_text(date_s, 100, 520, 6, False)

    rows = []
    for y in range(height):
        start = (height - 1 - y) * row_bytes
        rows.append(bytes(buf[start : start + row_bytes]))
    return rows


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("-o", "--output", type=Path, default=Path("apps/clock-face/sd/sleep.bmp"))
    p.add_argument("--width", type=int, default=480)
    p.add_argument("--height", type=int, default=800)
    args = p.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    rows = render_face(args.width, args.height, datetime.now())
    write_mono_bmp(args.output, args.width, args.height, rows)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
