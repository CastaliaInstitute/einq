#!/usr/bin/env python3
"""Probe Einq BLE display GATT (read state, optional write command)."""

from __future__ import annotations

import argparse
import asyncio
import json
import sys

SERVICE = "a1b2c3d4-e5f6-4789-a012-3456789abcde"
DISPLAY_STATE = "a1b2c3d4-e5f6-4789-a012-3456789abc01"
DISPLAY_CMD = "a1b2c3d4-e5f6-4789-a012-3456789abc02"


async def run(show: str | None, wait_notify: float) -> int:
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError:
        print("Install bleak: pip install bleak", file=sys.stderr)
        return 1

    device = None
    async for d in BleakScanner.discover(timeout=8.0):
        if d.name and "Einq" in d.name:
            device = d
            break
    if device is None:
        print("No BLE device named Einq found", file=sys.stderr)
        return 1

    print(f"Connecting to {device.name} ({device.address})")
    async with BleakClient(device) as client:
        if show:
            payload = show.encode("utf-8")
            await client.write_gatt_char(DISPLAY_CMD, payload, response=False)
            print(f"Wrote display_cmd: {show}")

        raw = await client.read_gatt_char(DISPLAY_STATE)
        text = raw.decode("utf-8", errors="replace")
        print("display:", json.dumps(json.loads(text), indent=2))

        if wait_notify > 0:

            def on_notify(_handle: int, data: bytearray) -> None:
                print("notify:", data.decode("utf-8", errors="replace"))

            await client.start_notify(DISPLAY_STATE, on_notify)
            print(f"Listening for NOTIFY ({wait_notify}s)…")
            await asyncio.sleep(wait_notify)
            await client.stop_notify(DISPLAY_STATE)

    return 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--show",
        metavar="JSON",
        help='Write display_cmd JSON, e.g. \'{"mode":"message","line1":"Hi"}\'',
    )
    parser.add_argument(
        "--wait-notify",
        type=float,
        default=0.0,
        help="Seconds to listen for display NOTIFY after read/write",
    )
    args = parser.parse_args()
    raise SystemExit(asyncio.run(run(args.show, args.wait_notify)))


if __name__ == "__main__":
    main()
