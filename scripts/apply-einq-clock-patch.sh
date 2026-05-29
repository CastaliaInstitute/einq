#!/usr/bin/env bash
# Copy Castalia Einq clock demo into the CrossPoint tree.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CP="${CROSSPOINT_DIR:-$ROOT/.vendor/crosspoint-reader}"
PATCH="$ROOT/apps/clock-face/patch"
EINQ_VERSION_FILE="$ROOT/firmware/einq-version"
EINQ_VERSION="${EINQ_VERSION:-}"
if [[ -z "$EINQ_VERSION" && -f "$EINQ_VERSION_FILE" ]]; then
  EINQ_VERSION="$(tr -d '[:space:]' <"$EINQ_VERSION_FILE")"
fi

if [[ ! -d "$CP/src" ]]; then
  echo "error: CrossPoint not found at $CP (clone with git submodule or set CROSSPOINT_DIR)" >&2
  exit 1
fi

mkdir -p "$CP/src/activities/einq" "$CP/src/einq-ble" "$CP/src/einq-schedule" "$CP/src/einq-glyph" "$CP/src/einq-cotd" "$CP/src/einq-ota"
cp "$PATCH/EinqClockActivity.h" "$PATCH/EinqClockActivity.cpp" "$CP/src/activities/einq/"
cp "$ROOT/firmware/einq-ble/"*.h "$ROOT/firmware/einq-ble/"*.cpp "$CP/src/einq-ble/"
cp "$ROOT/firmware/einq-schedule/"*.h "$ROOT/firmware/einq-schedule/"*.cpp "$CP/src/einq-schedule/"
cp "$ROOT/firmware/einq-glyph/"*.h "$ROOT/firmware/einq-glyph/"*.cpp "$CP/src/einq-glyph/"
cp "$ROOT/firmware/einq-cotd/"*.h "$ROOT/firmware/einq-cotd/"*.cpp "$CP/src/einq-cotd/"
cp "$ROOT/firmware/einq-ota/"*.h "$ROOT/firmware/einq-ota/"*.cpp "$CP/src/einq-ota/"

PIO_INI="$CP/platformio.ini"
if ! grep -q 'NimBLE-Arduino' "$PIO_INI"; then
  python3 - "$PIO_INI" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
needle = "  links2004/WebSockets @ 2.7.3\n"
insert = needle + "  h2zero/NimBLE-Arduino @ ^2.2.0\n"
if needle not in text:
    raise SystemExit("platformio.ini: could not find lib_deps anchor")
path.write_text(text.replace(needle, insert, 1))
print("patched platformio.ini (NimBLE-Arduino)")
PY
fi

# Home menu: add Einq Clock entry (idempotent markers).
HOME_CPP="$CP/src/activities/home/HomeActivity.cpp"
HOME_H="$CP/src/activities/home/HomeActivity.h"

if ! grep -q 'EinqClockActivity' "$HOME_CPP"; then
  python3 - "$HOME_CPP" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
text = text.replace(
    '#include "fontIds.h"\n',
    '#include "fontIds.h"\n#include "activities/einq/EinqClockActivity.h"\n',
)
text = text.replace(
    "  int count = 4;  // File Browser, Recents, File transfer, Settings\n",
    "  int count = 5;  // File Browser, Recents, File transfer, Settings, Einq Clock\n",
)
text = text.replace(
    '  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),\n'
    '                                        tr(STR_SETTINGS_TITLE)};\n'
    '  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Settings};\n',
    '  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),\n'
    '                                        "Einq Clock", tr(STR_SETTINGS_TITLE)};\n'
    '  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Transfer, Settings};\n',
)
old = """    const int fileTransferIdx = idx++;
    const int settingsIdx = idx;

    if (selectorIndex < recentBooks.size()) {
"""
new = """    const int fileTransferIdx = idx++;
    const int einqClockIdx = idx++;
    const int settingsIdx = idx;

    if (selectorIndex < recentBooks.size()) {
"""
text = text.replace(old, new)
text = text.replace(
    """    } else if (menuSelectedIndex == fileTransferIdx) {
      onFileTransferOpen();
    } else if (menuSelectedIndex == settingsIdx) {
      onSettingsOpen();
    }
""",
    """    } else if (menuSelectedIndex == fileTransferIdx) {
      onFileTransferOpen();
    } else if (menuSelectedIndex == einqClockIdx) {
      onEinqClockOpen();
    } else if (menuSelectedIndex == settingsIdx) {
      onSettingsOpen();
    }
""",
)
path.write_text(text)
print("patched HomeActivity.cpp")
PY
fi

if ! grep -q 'onEinqClockOpen' "$HOME_H"; then
  python3 - "$HOME_H" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
text = text.replace(
    "  void onOpdsBrowserOpen();\n",
    "  void onOpdsBrowserOpen();\n  void onEinqClockOpen();\n",
)
path.write_text(text)
print("patched HomeActivity.h")
PY
fi

if ! grep -q 'onEinqClockOpen' "$HOME_CPP" || ! grep -q 'EinqClockActivity' "$HOME_CPP"; then
  :
fi

if ! grep -q 'void HomeActivity::onEinqClockOpen' "$HOME_CPP"; then
  cat >>"$HOME_CPP" <<'EOF'

void HomeActivity::onEinqClockOpen() {
  startActivityForResult(std::make_unique<EinqClockActivity>(renderer, mappedInput), nullptr);
}
EOF
fi

# Auto-start Einq on boot (normal wake paths that used to call goHome()).
MAIN_CPP="$CP/src/main.cpp"
if ! grep -q 'goToEinqClock' "$MAIN_CPP"; then
  python3 - "$MAIN_CPP" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
text = text.replace(
    '#include "activities/ActivityManager.h"\n',
    '#include "activities/ActivityManager.h"\n#include "activities/einq/EinqClockActivity.h"\n',
    1,
)
text = text.replace("    activityManager.goHome();", "    activityManager.goToEinqClock();")
path.write_text(text)
print("patched main.cpp (boot → Einq)")
PY
fi

AM_CPP="$CP/src/activities/ActivityManager.cpp"
AM_H="$CP/src/activities/ActivityManager.h"
if ! grep -q 'goToEinqClock' "$AM_H"; then
  python3 - "$AM_H" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
text = text.replace("  void goHome();", "  void goHome();\n  void goToEinqClock();", 1)
path.write_text(text)
print("patched ActivityManager.h")
PY
fi
if ! grep -q 'goToEinqClock' "$AM_CPP"; then
  python3 - "$AM_CPP" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
text = text.replace(
    '#include "home/HomeActivity.h"\n',
    '#include "home/HomeActivity.h"\n#include "einq/EinqClockActivity.h"\n',
)
text = text.replace(
    "void ActivityManager::goHome() { replaceActivity(std::make_unique<HomeActivity>(renderer, mappedInput)); }\n",
    "void ActivityManager::goHome() { replaceActivity(std::make_unique<HomeActivity>(renderer, mappedInput)); }\n\n"
    "void ActivityManager::goToEinqClock() {\n"
    "  replaceActivity(std::make_unique<EinqClockActivity>(renderer, mappedInput));\n"
    "}\n",
)
path.write_text(text)
print("patched ActivityManager")
PY
fi

# OTA: GitHub releases on CastaliaInstitute/einq (asset must be named firmware.bin).
OTA_CPP="$CP/src/network/OtaUpdater.cpp"
if ! grep -q 'CastaliaInstitute/einq' "$OTA_CPP"; then
  python3 - "$OTA_CPP" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
old = (
    'constexpr char latestReleaseUrl[] = '
    '"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/latest";'
)
new = (
    'constexpr char latestReleaseUrl[] = '
    '"https://api.github.com/repos/CastaliaInstitute/einq/releases/latest";'
)
text = path.read_text()
if old not in text:
    raise SystemExit("OtaUpdater.cpp: expected upstream release URL")
path.write_text(text.replace(old, new, 1))
print("patched OtaUpdater.cpp (OTA → CastaliaInstitute/einq)")
PY
fi

if [[ -n "$EINQ_VERSION" ]]; then
  python3 - "$CP/platformio.ini" "$EINQ_VERSION" <<'PY'
from pathlib import Path
import re
import sys
path = Path(sys.argv[1])
version = sys.argv[2]
text = path.read_text()
text, n = re.subn(r"(?m)^version = .+$", f"version = {version}", text, count=1, flags=re.MULTILINE)
if n != 1:
    raise SystemExit("platformio.ini: could not set [crosspoint] version")
path.write_text(text)
print(f"set CrossPoint/Einq firmware version to {version}")
PY
fi

echo "Einq patch applied (clock + BLE GATT; auto-start on boot; Back → CrossPoint home)"
