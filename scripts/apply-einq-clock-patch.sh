#!/usr/bin/env bash
# Copy Castalia Einq clock demo into the CrossPoint tree.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CP="${CROSSPOINT_DIR:-$ROOT/.vendor/crosspoint-reader}"
PATCH="$ROOT/apps/clock-face/patch"

if [[ ! -d "$CP/src" ]]; then
  echo "error: CrossPoint not found at $CP (clone with git submodule or set CROSSPOINT_DIR)" >&2
  exit 1
fi

mkdir -p "$CP/src/activities/einq"
cp "$PATCH/EinqClockActivity.h" "$PATCH/EinqClockActivity.cpp" "$CP/src/activities/einq/"

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

echo "Einq clock patch applied (auto-start on boot; Back → CrossPoint home)"
