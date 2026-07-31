#!/usr/bin/env python3
"""Generate a compact firmware catalog from CastaliaInstitute/cards."""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / ".vendor/cards/editorial/catalog.json"
DEFAULT_OUTPUT = ROOT / "apps/clock-face/patch/EinqCardCatalogData.h"


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(source: Path, output: Path) -> None:
    document = json.loads(source.read_text())
    cards = document.get("cards", [])
    if not cards:
        raise SystemExit(f"{source}: catalog has no cards")

    domains = sorted({str(card.get("domain", "")) for card in cards})
    domain_index = {domain: index for index, domain in enumerate(domains)}
    version = re.sub(r"[^A-Za-z0-9._-]", "", str(document.get("version", "")))

    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "namespace EinqCardCatalogData {",
        "struct Card {",
        "  const char* slug;",
        "  const char* token;",
        "  uint8_t domain;",
        "};",
        f'constexpr char kVersion[] = {c_string(version)};',
        "constexpr const char* kDomains[] = {",
    ]
    lines.extend(f"    {c_string(domain)}," for domain in domains)
    lines.extend(["};", "constexpr Card kCards[] = {"])
    for card in cards:
        slug = str(card.get("slug", "")).strip()
        token = str(card.get("token", "")).strip()
        domain = str(card.get("domain", "")).strip()
        if not slug or not token or domain not in domain_index:
            raise SystemExit(f"{source}: invalid card entry {card!r}")
        lines.append(
            f"    {{{c_string(slug)}, {c_string(token)}, {domain_index[domain]}}},"
        )
    lines.extend(
        [
            "};",
            "constexpr size_t kCardCount = sizeof(kCards) / sizeof(kCards[0]);",
            "}  // namespace EinqCardCatalogData",
            "",
        ]
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines))
    print(f"Wrote {output}: {len(cards)} cards, {len(domains)} domains")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    generate(args.input.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
