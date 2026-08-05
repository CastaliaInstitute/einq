#!/usr/bin/env python3
"""Atomically install bounded gateway segments on an attached eINQ device."""

from __future__ import annotations

import argparse
import json
import time
import zlib
from pathlib import Path
from typing import Callable

import serial


SEGMENTS = ("core", "astrology-self", "astrology-synastry", "daily-1", "daily-2", "daily-3", "daily-4", "scriptorium")


def wait_for_line(port: serial.Serial, prefix: bytes, timeout: float = 15) -> bytes:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().rstrip(b"\r\n")
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"did not receive {prefix.decode(errors='replace')}")


def open_without_reset(path: str) -> serial.Serial:
    port = serial.Serial(port=None, baudrate=115200, timeout=1, dsrdtr=False, rtscts=False)
    port.dtr = False
    port.rts = False
    port.port = path
    port.open()
    return port


def open_when_available(
    path: str,
    *,
    timeout: float = 20,
    opener: Callable[[str], serial.Serial] = open_without_reset,
) -> serial.Serial:
    """Wait through ESP USB re-enumeration without asserting DTR/RTS."""
    deadline = time.monotonic() + timeout
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            return opener(path)
        except (OSError, serial.SerialException) as exc:
            last_error = exc
            time.sleep(0.25)
    raise TimeoutError(f"serial port {path} did not become available") from last_error


def compact_payload(path: Path) -> bytes:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path}: segment must be a JSON object")
    return json.dumps(value, separators=(",", ":"), ensure_ascii=True).encode("utf-8")


def install(port: serial.Serial, segment: str, payload: bytes) -> None:
    checksum = zlib.crc32(payload) & 0xFFFFFFFF
    command = f"CMD:EINQ_CACHE_BEGIN:{segment}\t{len(payload)}\t{checksum:08x}\n".encode()
    port.write(command)
    port.flush()
    ready = wait_for_line(port, b"EINQ_CACHE_BEGIN_")
    if not ready.startswith(b"EINQ_CACHE_BEGIN_OK:"):
        raise RuntimeError(ready.decode(errors="replace"))

    for offset in range(0, len(payload), 64):
        chunk = payload[offset : offset + 64]
        command = b"CMD:EINQ_CACHE_CHUNK:" + chunk.hex().encode("ascii") + b"\n"
        written = port.write(command)
        if written != len(command):
            raise RuntimeError(f"short serial write: {written}/{len(command)}")
        port.flush()
        result = wait_for_line(port, b"EINQ_CACHE_CHUNK_")
        if not result.startswith(b"EINQ_CACHE_CHUNK_OK:"):
            raise RuntimeError(result.decode(errors="replace"))

    port.write(b"CMD:EINQ_CACHE_END\n")
    port.flush()
    result = wait_for_line(port, b"EINQ_CACHE_END_")
    if not result.startswith(b"EINQ_CACHE_END_OK:"):
        raise RuntimeError(result.decode(errors="replace"))
    print(result.decode())


def install_with_reconnect(
    path: str,
    segment: str,
    payload: bytes,
    *,
    open_wait: float,
    attempts: int = 2,
    opener: Callable[[str], serial.Serial] = open_without_reset,
) -> None:
    """Install one idempotent segment, reopening if CDC restarts mid-ack."""
    last_error: Exception | None = None
    for attempt in range(1, attempts + 1):
        try:
            with open_when_available(path, opener=opener) as port:
                time.sleep(max(0, open_wait))
                port.reset_input_buffer()
                install(port, segment, payload)
            return
        except (OSError, serial.SerialException, TimeoutError) as exc:
            last_error = exc
            if attempt < attempts:
                print(f"{segment}: serial restarted; retrying atomic install ({attempt + 1}/{attempts})")
    raise RuntimeError(f"failed to install {segment} after {attempts} attempts") from last_error


def reload_with_reconnect(
    path: str,
    *,
    open_wait: float,
    opener: Callable[[str], serial.Serial] = open_without_reset,
) -> None:
    with open_when_available(path, opener=opener) as port:
        time.sleep(max(0, open_wait))
        port.reset_input_buffer()
        port.write(b"CMD:EINQ_CACHE_RELOAD\n")
        port.flush()
        result = wait_for_line(port, b"EINQ_CACHE_RELOAD_")
        if result != b"EINQ_CACHE_RELOAD_OK":
            raise RuntimeError(result.decode(errors="replace"))
        print(result.decode())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--segment", action="append", nargs=2, metavar=("NAME", "JSON"), required=True)
    parser.add_argument("--open-wait", type=float, default=4)
    args = parser.parse_args()

    supplied = [(name, Path(path)) for name, path in args.segment]
    unknown = [name for name, _ in supplied if name not in SEGMENTS]
    if unknown:
        parser.error(f"unknown segment(s): {', '.join(unknown)}")

    for name, path in supplied:
        install_with_reconnect(
            args.port,
            name,
            compact_payload(path),
            open_wait=args.open_wait,
        )
    reload_with_reconnect(args.port, open_wait=args.open_wait)


if __name__ == "__main__":
    main()
