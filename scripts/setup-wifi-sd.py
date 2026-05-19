#!/usr/bin/env python3
"""Write CrossPoint WiFi credentials to the X4 SD card (/.crosspoint/wifi.json).

CrossPoint does not use ESP-IDF WiFi NVS (WiFi.persistent is false). Credentials live on
the SD card, XOR-obfuscated with the device MAC when possible.

Examples:
  ./scripts/setup-wifi-sd.py
  ./scripts/setup-wifi-sd.py --mount /Volumes/X4
  EINQ_PORT=/dev/cu.usbmodem1301 ./scripts/setup-wifi-sd.py --from-device
"""
from __future__ import annotations

import argparse
import base64
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "apps/clock-face/sd/.crosspoint/wifi.json"
WIFI_REL = Path(".crosspoint/wifi.json")


def parse_mac(text: str) -> bytes:
    hexes = re.findall(r"[0-9a-fA-F]{2}", text)
    if len(hexes) < 6:
        raise ValueError(f"need 6-byte MAC, got: {text!r}")
    return bytes(int(h, 16) for h in hexes[:6])


def obfuscate_password(password: str, mac: bytes) -> str:
    key = mac[:6]
    raw = password.encode()
    xored = bytes(b ^ key[i % len(key)] for i, b in enumerate(raw))
    return base64.b64encode(xored).decode("ascii")


def read_mac_from_esptool(port: str) -> bytes:
    proc = subprocess.run(
        ["esptool.py", "-p", port, "read_mac"],
        capture_output=True,
        text=True,
        check=False,
    )
    out = proc.stdout + proc.stderr
    m = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", out)
    if not m:
        raise RuntimeError(f"could not read MAC from {port}:\n{out}")
    return parse_mac(m.group(1))


def build_wifi_json(ssid: str, password: str, mac: bytes | None) -> dict:
    cred: dict = {"ssid": ssid}
    if mac is not None:
        cred["password_obf"] = obfuscate_password(password, mac)
    else:
        cred["password"] = password
    return {
        "lastConnectedSsid": ssid,
        "credentials": [cred],
    }


def write_wifi(path: Path, doc: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ssid", default="The Chateau", help="WiFi SSID")
    ap.add_argument("--password", default="thechateau", help="WiFi password")
    ap.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUT,
        help=f"staging file (default: {DEFAULT_OUT})",
    )
    ap.add_argument(
        "--mount",
        type=Path,
        help="SD volume mount point; also writes <mount>/.crosspoint/wifi.json",
    )
    ap.add_argument("--mac", help="device MAC aa:bb:cc:dd:ee:ff (for password_obf)")
    ap.add_argument(
        "--from-device",
        action="store_true",
        help="read MAC via esptool (EINQ_PORT or auto-detect ESP32-C3)",
    )
    ap.add_argument("--port", default=None, help="serial port for --from-device")
    args = ap.parse_args()

    mac: bytes | None = None
    if args.mac:
        mac = parse_mac(args.mac)
    elif args.from_device:
        port = args.port
        if not port:
            import os

            port = os.environ.get("EINQ_PORT")
        if not port:
            for candidate in sorted(Path("/dev").glob("cu.usbmodem*")):
                probe = subprocess.run(
                    ["esptool.py", "-p", str(candidate), "chip_id"],
                    capture_output=True,
                    text=True,
                )
                if "ESP32-C3" in probe.stdout + probe.stderr:
                    port = str(candidate)
                    break
        if not port:
            print("error: no ESP32-C3 port; set --port or EINQ_PORT", file=sys.stderr)
            return 1
        print(f"reading MAC from {port}")
        mac = read_mac_from_esptool(port)

    doc = build_wifi_json(args.ssid, args.password, mac)
    write_wifi(args.output, doc)

    if args.mount:
        sd_path = args.mount / WIFI_REL
        write_wifi(sd_path, doc)
        print(f"copy complete on SD volume {args.mount}")

    if not args.mount:
        print()
        print("SD card not mounted. When the X4 SD is available on your Mac:")
        print(f"  ./scripts/setup-wifi-sd.py --mount /Volumes/YOUR_SD --mac {mac.hex(':') if mac else '...'}")
        print("or:")
        print(f"  mkdir -p /Volumes/YOUR_SD/.crosspoint")
        print(f"  cp {args.output} /Volumes/YOUR_SD/.crosspoint/wifi.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
