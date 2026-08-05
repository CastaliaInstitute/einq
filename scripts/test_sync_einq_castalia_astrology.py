#!/usr/bin/env python3

import datetime as dt
import importlib.util
import json
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("sync-einq-castalia-astrology.py")
SPEC = importlib.util.spec_from_file_location("castalia_sync", MODULE_PATH)
assert SPEC and SPEC.loader
sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sync)


class CastaliaAstrologySyncTests(unittest.TestCase):
    def test_local_issue_date_handles_midnight_boundary(self) -> None:
        now = dt.datetime(2026, 8, 5, 2, tzinfo=dt.timezone.utc)
        self.assertEqual(sync.issue_date(now, "America/New_York"), "2026-08-04")
        self.assertEqual(sync.issue_date(now, "UTC"), "2026-08-05")

    def test_builds_bounded_verified_segments(self) -> None:
        row = {
            "issue_date": "2026-08-05",
            "payload": {
                "selfWeather": {"title": "Sun square Sun", "summary": "Self detail", "valid": True},
                "synastryWeather": {"title": "Household", "summary": "Pair detail", "valid": True},
                "astrologyMeta": {
                    "generatedAt": "2026-08-05T04:00:00+00:00",
                    "sourceDigestSha256": "a" * 64,
                    "ephemerisMode": "SWIEPH",
                    "engineDataRelease": "aloistr/swisseph@test-release",
                    "feedCheck": {"status": "passed"},
                },
            },
        }
        segments = sync.astrology_segments(row)
        self.assertEqual(set(segments), {"astrology-self", "astrology-synastry"})
        self.assertEqual(json.loads(segments["astrology-self"])["date"], "2026-08-05")
        self.assertEqual(json.loads(segments["astrology-self"])["selfWeather"]["title"], "Sun square Sun")
        self.assertEqual(json.loads(segments["astrology-synastry"])["astrologyMeta"]["feedCheck"]["status"], "passed")

    def test_rejects_unverified_edition(self) -> None:
        row = {
            "issue_date": "2026-08-05",
            "payload": {
                "astrologyMeta": {"sourceDigestSha256": "a" * 64, "feedCheck": {"status": "not-run"}}
            },
        }
        with self.assertRaisesRegex(ValueError, "not backed"):
            sync.astrology_segments(row)

    def test_rejects_unpinned_or_non_swieph_edition(self) -> None:
        meta = {
            "sourceDigestSha256": "a" * 64,
            "feedCheck": {"status": "passed"},
            "ephemerisMode": "MOSEPH",
            "engineDataRelease": "aloistr/swisseph@test-release",
        }
        row = {"issue_date": "2026-08-05", "payload": {"astrologyMeta": meta}}
        with self.assertRaisesRegex(ValueError, "pinned SWIEPH"):
            sync.astrology_segments(row)

        meta["ephemerisMode"] = "SWIEPH"
        meta["engineDataRelease"] = ""
        with self.assertRaisesRegex(ValueError, "pinned SWIEPH"):
            sync.astrology_segments(row)


if __name__ == "__main__":
    unittest.main()
