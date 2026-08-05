#!/usr/bin/env python3
"""Install today's verified Castalia Supabase astrology on an attached eINQ."""

from __future__ import annotations

import argparse
import datetime as dt
import importlib.util
import json
import os
import re
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any
from zoneinfo import ZoneInfo


INSTALLER_PATH = Path(__file__).with_name("install-einq-home-cache.py")
SPEC = importlib.util.spec_from_file_location("einq_cache_installer", INSTALLER_PATH)
assert SPEC and SPEC.loader
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


def load_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip().strip('"').strip("'")
    return values


def issue_date(now: dt.datetime, timezone: str) -> str:
    return now.astimezone(ZoneInfo(timezone)).date().isoformat()


def fetch_edition(
    base: str,
    key: str,
    username: str,
    date: str,
    *,
    opener=urllib.request.urlopen,
) -> dict[str, Any]:
    params = urllib.parse.urlencode(
        {
            "username": f"eq.{username}",
            "issue_date": f"eq.{date}",
            "select": "issue_date,source_schema,effective_at,payload",
        }
    )
    request = urllib.request.Request(
        f"{base.rstrip('/')}/rest/v1/castalia_device_daily_editions?{params}",
        headers={"apikey": key, "authorization": f"Bearer {key}"},
    )
    with opener(request, timeout=20) as response:
        rows = json.load(response)
    if len(rows) != 1:
        raise RuntimeError(f"expected one Castalia edition for {username} on {date}; found {len(rows)}")
    return rows[0]


def astrology_segments(row: dict[str, Any], profile: str = "parent") -> dict[str, bytes]:
    payload = row.get("payload") if isinstance(row.get("payload"), dict) else {}
    meta = payload.get("astrologyMeta") if isinstance(payload.get("astrologyMeta"), dict) else {}
    digest = str(meta.get("sourceDigestSha256") or "")
    feed = meta.get("feedCheck") if isinstance(meta.get("feedCheck"), dict) else {}
    if not re.fullmatch(r"[0-9a-f]{64}", digest) or feed.get("status") != "passed":
        raise ValueError("Castalia edition is not backed by a verified private source and ephemeris feed")
    common = {
        "schema": "castalia.device.daily.v1",
        "date": row.get("issue_date"),
        "generatedAt": meta.get("generatedAt"),
        "profile": profile,
        "astrologyMeta": meta,
    }
    segments = {
        "astrology-self": {**common, "selfWeather": payload.get("selfWeather")},
        "astrology-synastry": {**common, "synastryWeather": payload.get("synastryWeather")},
    }
    for name, value in segments.items():
        reading_key = "selfWeather" if name == "astrology-self" else "synastryWeather"
        reading = value.get(reading_key)
        if not isinstance(reading, dict) or not reading.get("valid"):
            raise ValueError(f"Castalia edition has no valid {reading_key}")
    return {
        name: json.dumps(value, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
        for name, value in segments.items()
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--username", default="dcmcshan")
    parser.add_argument("--timezone", default="America/New_York")
    parser.add_argument("--date", help="Override local issue date (YYYY-MM-DD)")
    parser.add_argument("--env-file", type=Path, required=True)
    parser.add_argument("--open-wait", type=float, default=2)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    values = {**load_env(args.env_file), **os.environ}
    base = values.get("CASTALIA_SUPABASE_URL") or values.get("SUPABASE_URL") or values.get("NEXT_PUBLIC_SUPABASE_URL")
    key = values.get("CASTALIA_SUPABASE_SERVICE_ROLE_KEY") or values.get("SUPABASE_SERVICE_ROLE_KEY")
    if not base or not key:
        raise RuntimeError("Castalia Supabase URL and service-role key are required")
    date = args.date or issue_date(dt.datetime.now(dt.timezone.utc), args.timezone)
    row = fetch_edition(base, key, args.username, date)
    segments = astrology_segments(row)
    meta = row["payload"]["astrologyMeta"]
    print(
        f"Castalia {date}: {row['payload']['selfWeather']['title']} · "
        f"{row['payload']['synastryWeather']['title']} · source {meta['sourceDigestSha256'][:12]}"
    )
    if args.dry_run:
        return
    for name, payload in segments.items():
        installer.install_with_reconnect(
            args.port,
            name,
            payload,
            open_wait=args.open_wait,
        )
    try:
        installer.reload_with_reconnect(args.port, open_wait=args.open_wait)
    except RuntimeError:
        print("Cache reload needs a clean heap; rebooting to activate verified segments")
        installer.reset_with_reconnect(args.port, open_wait=args.open_wait)


if __name__ == "__main__":
    main()
