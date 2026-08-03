#!/usr/bin/env python3
"""Build contemporary brush glyph studies for the Maya Tzolk'in calendar.

This is a symbolic Emojinq set, not a reconstruction of archaeological
Maya hieroglyphs. It uses the 20 day names and 13 tones of the 260-day count
as a stable supplementary Private Use Area collection.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


INK = "#262522"
PUA_START = 0xF1400


def svg(label: str, body: str) -> str:
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128" role="img" aria-label="{label}" data-pua="mayan-calendar" data-ink-stroke-system="brush-study-v1">
  <title>{label} — Emojinq Maya calendar brush study</title>
  <g fill="none" stroke="{INK}" stroke-linecap="round" stroke-linejoin="round">{body}</g>
</svg>
'''


def path(d: str, width: float = 4.2, fill: str = "none") -> str:
    return f'<path d="{d}" stroke-width="{width}" fill="{fill}" data-ink-stroke="tapered"/>'


def dot(x: int, y: int, r: int = 4) -> str:
    return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{INK}" stroke="none"/>'


DAY_SIGNS = {
    "imix": path("M28 67 C39 42 55 31 72 40 C88 48 99 67 91 87 C82 106 53 106 36 91 C27 83 24 75 28 67 M43 54 C48 46 59 44 66 50 M38 73 C51 68 64 71 76 82"),
    "ik": path("M30 66 C43 54 52 47 63 39 C74 31 86 35 96 47 M35 91 C49 78 59 71 70 62 C82 52 91 56 99 67 M63 39 L70 28 M70 62 L76 52"),
    "akbal": path("M35 101 V45 C35 29 51 21 64 29 C77 21 93 29 93 45 V101 M35 61 H93 M49 101 V74 C49 66 57 61 64 61 C71 61 79 66 79 74 V101"),
    "kan": path("M64 102 C47 92 39 72 44 52 C48 35 57 26 64 20 C71 26 80 35 84 52 C89 72 81 92 64 102 M64 20 V102 M50 67 C57 61 71 61 78 67"),
    "chikchan": path("M31 96 C42 80 43 64 35 49 C48 51 56 44 64 30 C72 44 80 51 93 49 C85 64 86 80 97 96 C82 88 73 86 64 96 C55 86 46 88 31 96 M64 30 V96"),
    "kimi": path("M64 25 C45 25 33 39 36 57 C38 72 48 87 64 101 C80 87 90 72 92 57 C95 39 83 25 64 25 M49 54 H57 M71 54 H79 M51 72 C58 78 70 78 77 72 M56 91 H72"),
    "manik": path("M40 103 C34 86 34 68 43 51 C50 38 59 28 64 24 C69 28 78 38 85 51 C94 68 94 86 88 103 M64 24 V103 M47 58 C54 64 74 64 81 58 M47 78 C54 84 74 84 81 78"),
    "lamat": path("M64 18 L70 53 L105 53 L76 72 L86 106 L64 85 L42 106 L52 72 L23 53 L58 53 Z M64 31 V85"),
    "muluk": path("M34 55 C45 45 83 45 94 55 L88 91 C76 101 52 101 40 91 Z M34 55 C43 68 85 68 94 55 M47 77 C54 84 74 84 81 77"),
    "ok": path("M31 73 C42 57 59 51 76 58 C91 64 98 78 91 91 C85 103 69 103 61 92 C54 83 61 71 70 75 C77 78 78 85 73 89 M31 73 C27 65 30 57 37 53"),
    "chuwen": path("M32 79 C43 64 48 45 64 38 C80 45 85 64 96 79 M41 53 C49 39 59 27 70 31 C80 35 78 48 69 52 C59 57 51 50 53 42 M47 82 C55 91 73 91 81 82"),
    "eb": path("M31 91 C42 82 53 80 64 85 C75 90 86 88 97 79 M31 64 C42 55 53 53 64 58 C75 63 86 61 97 52 M64 28 V93 M52 39 H76"),
    "ben": path("M42 103 V32 M64 103 V25 M86 103 V36 M42 50 C50 58 56 58 64 50 M64 43 C72 51 78 51 86 43 M34 103 H94"),
    "ix": path("M31 95 C40 78 42 57 53 46 C62 37 76 38 84 47 C94 58 91 78 98 95 C84 88 76 88 64 98 C53 88 45 88 31 95 M47 62 C53 54 59 54 64 62 C69 54 75 54 81 62 M55 75 H73"),
    "men": path("M32 91 C38 65 47 39 64 28 C81 39 90 65 96 91 C82 80 74 75 64 76 C54 75 46 80 32 91 M64 28 V76 M50 49 C57 55 71 55 78 49"),
    "kib": path("M64 20 C52 33 45 48 45 65 C45 83 54 97 64 106 C74 97 83 83 83 65 C83 48 76 33 64 20 M52 61 C58 56 70 56 76 61 M52 76 C58 81 70 81 76 76 M58 91 H70"),
    "kaban": path("M25 79 C42 73 52 79 64 88 C76 97 86 102 103 96 M29 48 C43 53 54 49 64 39 C74 29 85 27 99 34 M64 39 V88 M46 61 C54 68 74 68 82 61"),
    "etznab": path("M64 17 L91 54 L78 108 L50 108 L37 54 Z M37 54 H91 M50 82 H78 M64 17 V108"),
    "kawak": path("M33 36 C45 48 49 62 45 79 C42 92 49 101 60 104 M95 36 C83 48 79 62 83 79 C86 92 79 101 68 104 M64 23 V106 M52 54 C58 60 70 60 76 54"),
    "ajaw": path("M64 18 C84 18 99 36 99 59 C99 82 84 102 64 102 C44 102 29 82 29 59 C29 36 44 18 64 18 M64 39 C75 39 83 48 83 59 C83 70 75 79 64 79 C53 79 45 70 45 59 C45 48 53 39 64 39 M64 8 V18 M64 102 V116"),
}


def tone_body(number: int) -> str:
    bars, dots = divmod(number, 5)
    parts = []
    for index in range(bars):
        y = 78 - index * 13
        parts.append(path(f"M35 {y} H93", 5.2))
    for index in range(dots):
        x = 48 + index * 16
        y = 30 if bars else 76
        parts.append(dot(x, y, 5))
    return "".join(parts)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("assets/emojinq/pua/mayan-calendar"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    entries = []
    offset = 0
    for slug, body in DAY_SIGNS.items():
        label = slug.replace("-", " ").title()
        filename = f"day-sign-{slug}.svg"
        (args.output / filename).write_text(svg(f"Tzolk'in day sign {label}", body), encoding="utf-8")
        entries.append({"name": f"mayan-day-sign-{slug}", "label": f"Day Signs · {label}", "category": "Mayan Calendar · Day Signs", "source": filename, "codepoints": [PUA_START + offset]})
        offset += 1
    for number in range(1, 14):
        filename = f"tone-{number:02d}.svg"
        (args.output / filename).write_text(svg(f"Tzolk'in tone {number}", tone_body(number)), encoding="utf-8")
        entries.append({"name": f"mayan-tone-{number:02d}", "label": f"Tones · {number}", "category": "Mayan Calendar · Tones", "source": filename, "codepoints": [PUA_START + offset]})
        offset += 1
    (args.output / "manifest.json").write_text(json.dumps({"pua_start": PUA_START, "entries": entries}, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(entries)} Mayan calendar PUA SVGs to {args.output}")


if __name__ == "__main__":
    main()
