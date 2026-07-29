#!/usr/bin/env python3
"""Pack manifest-driven corner themes into a single embedded 1bpp header."""

import argparse
import json
import re
from pathlib import Path

from PIL import Image

DEFAULT_ROOT = Path(__file__).resolve().parents[1]

SEASONS = {
    "winter": 1 << 0,
    "spring": 1 << 1,
    "summer": 1 << 2,
    "autumn": 1 << 3,
    "all": 0x0F,
}
POSITIONS = ("bottom_left", "bottom_right", "top_left", "top_right")
DEFAULT_FLIPS = {
    "bottom_left": (False, False),
    "bottom_right": (True, False),
    "top_left": (False, True),
    "top_right": (True, True),
}


def symbol(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_")


def pack_image(source: Path, max_size: int) -> tuple[int, int, int, bytearray]:
    image = Image.open(source).convert("RGBA")
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        raise SystemExit(f"{source}: no visible artwork")

    image = image.crop(bbox)
    scale = min(max_size / image.width, max_size / image.height)
    image = image.resize(
        (round(image.width * scale), round(image.height * scale)),
        Image.Resampling.LANCZOS,
    )

    alpha = image.getchannel("A")
    width, height = image.size
    row_bytes = (width + 7) // 8
    packed = bytearray(row_bytes * height)
    for y in range(height):
        for x in range(width):
            if alpha.getpixel((x, y)) >= 96:
                packed[y * row_bytes + x // 8] |= 0x80 >> (x & 7)
    return width, height, row_bytes, packed


def generate(root: Path, manifest_path: Path, output: Path) -> None:
    manifest = json.loads(manifest_path.read_text())
    max_size = int(manifest.get("max_size", 176))
    entries = manifest["themes"]
    if not entries:
        raise SystemExit(f"{manifest_path}: at least one theme is required")

    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "namespace EinqCornerArtData {",
        "struct CornerAsset {",
        "  const uint8_t* bitmap;",
        "  uint16_t width;",
        "  uint16_t height;",
        "  uint16_t rowBytes;",
        "  bool flipX;",
        "  bool flipY;",
        "};",
        "struct ThemeAsset {",
        "  const char* id;",
        "  CornerAsset corners[4];  // bottom-left, bottom-right, top-left, top-right",
        "  uint8_t seasonMask;",
        "};",
        "",
    ]

    # Pack every unique source only once, even when several positions reuse it.
    source_data = {}
    source_order = []
    resolved_themes = []
    for entry in entries:
        master = entry.get("source")
        overrides = entry.get("corners", {})
        if master is None and any(position not in overrides for position in POSITIONS):
            raise SystemExit(f"{entry['id']}: provide source or all four corner overrides")

        corners = []
        for position in POSITIONS:
            if position in overrides:
                source_name = overrides[position]
                flip_x = flip_y = False
            else:
                source_name = master
                flip_x, flip_y = DEFAULT_FLIPS[position]

            source = root / source_name
            key = source.resolve()
            if key not in source_data:
                source_data[key] = pack_image(source, max_size)
                source_order.append((key, source_name))
            corners.append((key, flip_x, flip_y))

        mask = 0
        for season in entry.get("seasons", ["all"]):
            if season not in SEASONS:
                raise SystemExit(f"{entry['id']}: unknown season {season!r}")
            mask |= SEASONS[season]
        resolved_themes.append((entry["id"], corners, mask))

    source_symbols = {}
    total = 0
    for index, (key, source_name) in enumerate(source_order):
        width, height, row_bytes, packed = source_data[key]
        name = f"kImage{index}_{symbol(Path(source_name).stem)}"
        source_symbols[key] = name
        lines.append(f"constexpr uint8_t {name}[] PROGMEM = {{")
        for offset in range(0, len(packed), 16):
            chunk = ", ".join(f"0x{value:02x}" for value in packed[offset : offset + 16])
            lines.append(f"    {chunk},")
        lines += ["};", ""]
        total += len(packed)

    lines.append("constexpr ThemeAsset kThemes[] = {")
    for theme_id, corners, mask in resolved_themes:
        lines.append(f'    {{"{theme_id}", {{')
        for key, flip_x, flip_y in corners:
            width, height, row_bytes, _ = source_data[key]
            name = source_symbols[key]
            lines.append(
                f"      {{{name}, {width}, {height}, {row_bytes}, "
                f"{str(flip_x).lower()}, {str(flip_y).lower()}}},"
            )
        lines.append(f"    }}, 0x{mask:02x}}},")
    lines += [
        "};",
        "constexpr size_t kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);",
        "}  // namespace EinqCornerArtData",
        "",
    ]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines))

    print(
        f"Wrote {output}: {len(resolved_themes)} theme(s), "
        f"{len(source_order)} unique image(s), {total} bytes"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = args.manifest or root / "assets/corner-themes/manifest.json"
    output = args.output or root / "apps/clock-face/patch/EinqCornerArtData.h"
    generate(root, manifest.resolve(), output.resolve())


if __name__ == "__main__":
    main()
