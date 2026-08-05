#!/usr/bin/env python3

import importlib.util
import unittest
from pathlib import Path
from unittest import mock

import serial


MODULE_PATH = Path(__file__).with_name("install-einq-home-cache.py")
SPEC = importlib.util.spec_from_file_location("cache_installer", MODULE_PATH)
assert SPEC and SPEC.loader
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


class FakePort:
    def __init__(self, name: str):
        self.name = name

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def reset_input_buffer(self):
        pass


class ReconnectTests(unittest.TestCase):
    @mock.patch.object(installer.time, "sleep", return_value=None)
    def test_waits_through_usb_reenumeration(self, _sleep) -> None:
        calls = iter([serial.SerialException("gone"), OSError("missing"), FakePort("ready")])

        def opener(_path):
            value = next(calls)
            if isinstance(value, Exception):
                raise value
            return value

        port = installer.open_when_available("/dev/example", timeout=1, opener=opener)
        self.assertEqual(port.name, "ready")

    @mock.patch.object(installer.time, "sleep", return_value=None)
    def test_retries_an_idempotent_segment_after_lost_ack(self, _sleep) -> None:
        ports = iter([FakePort("first"), FakePort("second")])
        attempts = []

        def fake_install(port, segment, payload):
            attempts.append((port.name, segment, payload))
            if len(attempts) == 1:
                raise TimeoutError("ack lost during restart")

        with mock.patch.object(installer, "install", side_effect=fake_install):
            installer.install_with_reconnect(
                "/dev/example",
                "astrology-self",
                b"{}",
                open_wait=0,
                opener=lambda _path: next(ports),
            )

        self.assertEqual([item[0] for item in attempts], ["first", "second"])


if __name__ == "__main__":
    unittest.main()
