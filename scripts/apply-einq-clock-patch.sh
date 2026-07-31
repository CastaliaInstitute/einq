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

python3 "$ROOT/scripts/generate-setup-assets.py"
python3 "$ROOT/scripts/generate-corner-art.py"
if [[ -f "$ROOT/.vendor/cards/editorial/catalog.json" ]]; then
  python3 "$ROOT/scripts/generate-card-catalog.py"
fi

mkdir -p "$CP/src/activities/einq" "$CP/src/einq-ble" "$CP/src/einq-schedule" "$CP/src/einq-glyph" "$CP/src/einq-cotd" "$CP/src/einq-ota" "$CP/src/einq-wifi" "$CP/src/einq-config" "$CP/src/einq-room" "$CP/src/einq-home" "$CP/src/einq-auth"
cp "$PATCH/EinqClockActivity.h" "$PATCH/EinqClockActivity.cpp" "$PATCH/EinqCornerArt.h" "$PATCH/EinqCornerArt.cpp" \
  "$PATCH/EinqCornerArtData.h" "$PATCH/EinqCardCatalogData.h" \
  "$PATCH/EinqWifiSetupActivity.h" "$PATCH/EinqWifiSetupActivity.cpp" \
  "$PATCH/EinqAuthActivity.h" "$PATCH/EinqAuthActivity.cpp" "$CP/src/activities/einq/"
cp "$ROOT/firmware/einq-ble/"*.h "$ROOT/firmware/einq-ble/"*.cpp "$CP/src/einq-ble/"
cp "$ROOT/firmware/einq-schedule/"*.h "$ROOT/firmware/einq-schedule/"*.cpp "$CP/src/einq-schedule/"
cp "$ROOT/firmware/einq-glyph/"*.h "$ROOT/firmware/einq-glyph/"*.cpp "$CP/src/einq-glyph/"
cp "$ROOT/firmware/einq-cotd/"*.h "$ROOT/firmware/einq-cotd/"*.cpp "$CP/src/einq-cotd/"
cp "$ROOT/firmware/einq-ota/"*.h "$ROOT/firmware/einq-ota/"*.cpp "$CP/src/einq-ota/"
cp "$ROOT/firmware/einq-wifi/"*.h "$ROOT/firmware/einq-wifi/"*.cpp "$CP/src/einq-wifi/"
cp "$ROOT/firmware/einq-config/"*.h "$ROOT/firmware/einq-config/"*.cpp "$CP/src/einq-config/"
rm -f "$CP/src/einq-room/test_room_resolver.cpp"
cp "$ROOT/firmware/einq-room/EinqRoomResolver.h" "$ROOT/firmware/einq-room/EinqRoomResolver.cpp" \
  "$ROOT/firmware/einq-room/EinqRoomScanner.h" "$ROOT/firmware/einq-room/EinqRoomScanner.cpp" "$CP/src/einq-room/"
cp "$ROOT/firmware/einq-home/"*.h "$ROOT/firmware/einq-home/"*.cpp "$CP/src/einq-home/"
cp "$ROOT/firmware/einq-auth/"*.h "$ROOT/firmware/einq-auth/"*.cpp "$CP/src/einq-auth/"

# Select QR versions by actual encoder capacity and preserve the mandatory
# four-module white quiet zone. This is needed for Castalia's 314-byte pairing
# URL, which does not fit the old helper's hard-coded version 10.
QR_CPP="$CP/src/util/QrUtils.cpp"
if ! grep -q 'kQuietModules' "$QR_CPP"; then
  python3 - "$QR_CPP" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
old = '''  int version = 4;
  if (len > 114) version = 10;
  if (len > 395) version = 20;
  if (len > 1066) version = 30;
  if (len > 2110) version = 40;

  // Make sure we have a large enough buffer on the heap to avoid blowing the stack
  uint32_t bufferSize = qrcode_getBufferSize(version);
  auto qrcodeBytes = std::make_unique<uint8_t[]>(bufferSize);

  QRCode qrcode;
  // Initialize the QR code. We use ECC_LOW for max capacity.
  int8_t res = qrcode_initText(&qrcode, qrcodeBytes.get(), version, ECC_LOW, payload);
'''
new = '''  static constexpr uint16_t kByteCapacity[] = {
      0, 17, 32, 53, 78, 106, 134, 154, 192, 230, 271, 321, 367, 425,
      458, 520, 586, 644, 718, 792, 858, 929, 1003, 1091, 1171, 1273,
      1367, 1465, 1528, 1628, 1732, 1840, 1952, 2068, 2188, 2303, 2431,
      2563, 2699, 2809, 2953};
  constexpr int kMaxVersion = 40;
  int version = 1;
  while (version < kMaxVersion && len > kByteCapacity[version]) ++version;
  uint32_t bufferSize = qrcode_getBufferSize(version);
  auto qrcodeBytes = std::make_unique<uint8_t[]>(bufferSize);

  QRCode qrcode;
  int8_t res = qrcode_initText(&qrcode, qrcodeBytes.get(), version, ECC_LOW, payload);
'''
if old not in text:
    raise SystemExit('QrUtils.cpp: version-selection block not found')
text = text.replace(old, new, 1)
old = '''    // Determine the optimal pixel size.
    const int maxDim = std::min(bounds.width, bounds.height);

    int px = maxDim / qrcode.size;
    if (px < 1) px = 1;

    // Calculate centering X and Y
    const int qrDisplaySize = qrcode.size * px;
    const int xOff = bounds.x + (bounds.width - qrDisplaySize) / 2;
    const int yOff = bounds.y + (bounds.height - qrDisplaySize) / 2;
'''
new = '''    constexpr int kQuietModules = 4;
    const int maxDim = std::min(bounds.width, bounds.height);
    int px = maxDim / (qrcode.size + 2 * kQuietModules);
    if (px < 1) px = 1;

    const int totalDisplaySize = (qrcode.size + 2 * kQuietModules) * px;
    const int xOff = bounds.x + (bounds.width - totalDisplaySize) / 2 + kQuietModules * px;
    const int yOff = bounds.y + (bounds.height - totalDisplaySize) / 2 + kQuietModules * px;
'''
if old not in text:
    raise SystemExit('QrUtils.cpp: sizing block not found')
path.write_text(text.replace(old, new, 1))
print('patched QrUtils.cpp (capacity + quiet zone)')
PY
fi

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

if ! grep -q -- '-DEINQ_KEEP_AWAKE=1' "$PIO_INI"; then
  python3 - "$PIO_INI" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
needle = "  -DARDUINO_USB_CDC_ON_BOOT=1\n"
if needle not in text:
    raise SystemExit("platformio.ini: could not find USB CDC build flag")
path.write_text(text.replace(needle, needle + "  -DEINQ_KEEP_AWAKE=1\n", 1))
print("patched platformio.ini (development keep-awake)")
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

if ! grep -q 'Development keep-awake overrides USB power sleep' "$MAIN_CPP"; then
  python3 - "$MAIN_CPP" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
old = """    case HalGPIO::WakeupReason::AfterUSBPower:
      // If USB power caused a cold boot, go back to sleep
      LOG_DBG("MAIN", "Wakeup reason: After USB Power");
      powerManager.startDeepSleep(gpio);
      break;
"""
new = """    case HalGPIO::WakeupReason::AfterUSBPower:
      // Development keep-awake overrides USB power sleep so the console and radios stay reachable.
#if EINQ_KEEP_AWAKE
      LOG_INF("MAIN", "Wakeup reason: After USB Power; development keep-awake");
      break;
#else
      LOG_DBG("MAIN", "Wakeup reason: After USB Power");
      powerManager.startDeepSleep(gpio);
      break;
#endif
"""
if old not in text:
    raise SystemExit("main.cpp: could not find AfterUSBPower branch")
path.write_text(text.replace(old, new, 1))
print("patched main.cpp (USB development keep-awake)")
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
markers = (
    "  void goHome(HomeMenuItem initialMenuItem = HomeMenuItem::NONE);\n",
    "  void goHome();\n",
)
for marker in markers:
    if marker in text:
        text = text.replace(marker, marker + "  void goToEinqClock();\n", 1)
        break
else:
    raise SystemExit("ActivityManager.h: could not find goHome() declaration")
if "goToEinqClock" not in text:
    raise SystemExit("ActivityManager.h: goToEinqClock not added")
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
if '#include "einq/EinqClockActivity.h"' not in text:
    text = text.replace(
        '#include "home/HomeActivity.h"\n',
        '#include "home/HomeActivity.h"\n#include "einq/EinqClockActivity.h"\n',
        1,
    )
insert = (
    "void ActivityManager::goToEinqClock() {\n"
    "  replaceActivity(std::make_unique<EinqClockActivity>(renderer, mappedInput));\n"
    "}\n\n"
)
anchor = "void ActivityManager::pushActivity("
if anchor not in text:
    raise SystemExit("ActivityManager.cpp: could not find pushActivity anchor")
if insert.strip() not in text:
    text = text.replace(anchor, insert + anchor, 1)
if "goToEinqClock" not in text:
    raise SystemExit("ActivityManager.cpp: goToEinqClock not added")
path.write_text(text)
print("patched ActivityManager")
PY
fi

if ! grep -q 'goToEinqClock' "$AM_H" || ! grep -q 'goToEinqClock' "$AM_CPP"; then
  echo "error: ActivityManager goToEinqClock patch failed" >&2
  exit 1
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

# Home menu: Einq WiFi setup (after Einq Clock entry).
if grep -q 'Einq Clock' "$HOME_CPP" && ! grep -q 'Einq WiFi' "$HOME_CPP"; then
  python3 - "$HOME_CPP" "$HOME_H" <<'PY'
from pathlib import Path
import sys
home_cpp = Path(sys.argv[1])
home_h = Path(sys.argv[2])
text = home_cpp.read_text()
text = text.replace(
    '#include "activities/einq/EinqClockActivity.h"\n',
    '#include "activities/einq/EinqClockActivity.h"\n#include "activities/einq/EinqWifiSetupActivity.h"\n',
    1,
)
text = text.replace(
    '  int count = 5;  // File Browser, Recents, File transfer, Settings, Einq Clock\n',
    '  int count = 6;  // File Browser, Recents, File transfer, Settings, Einq Clock, Einq WiFi\n',
    1,
)
text = text.replace(
    '                                        "Einq Clock", tr(STR_SETTINGS_TITLE)};\n'
    '  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Transfer, Settings};\n',
    '                                        "Einq Clock", "Einq WiFi", tr(STR_SETTINGS_TITLE)};\n'
    '  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Transfer, Transfer, Settings};\n',
    1,
)
text = text.replace(
    """    const int einqClockIdx = idx++;
    const int settingsIdx = idx;
""",
    """    const int einqClockIdx = idx++;
    const int einqWifiIdx = idx++;
    const int settingsIdx = idx;
""",
    1,
)
text = text.replace(
    """    } else if (menuSelectedIndex == einqClockIdx) {
      onEinqClockOpen();
    } else if (menuSelectedIndex == settingsIdx) {
""",
    """    } else if (menuSelectedIndex == einqClockIdx) {
      onEinqClockOpen();
    } else if (menuSelectedIndex == einqWifiIdx) {
      onEinqWifiOpen();
    } else if (menuSelectedIndex == settingsIdx) {
""",
    1,
)
home_cpp.write_text(text)
print("patched HomeActivity.cpp (Einq WiFi)")

h = home_h.read_text()
if "onEinqWifiOpen" not in h:
    h = h.replace("  void onEinqClockOpen();\n", "  void onEinqClockOpen();\n  void onEinqWifiOpen();\n", 1)
    home_h.write_text(h)
    print("patched HomeActivity.h (Einq WiFi)")
PY
fi

if grep -q 'onEinqWifiOpen' "$HOME_H" && ! grep -q 'void HomeActivity::onEinqWifiOpen' "$HOME_CPP"; then
  cat >>"$HOME_CPP" <<'EOF'

void HomeActivity::onEinqWifiOpen() {
  startActivityForResult(std::make_unique<EinqWifiSetupActivity>(renderer, mappedInput), nullptr);
}
EOF
  echo "added HomeActivity::onEinqWifiOpen"
fi

# Keep the two Einq rows in CrossPoint Home navigable. They sit between File
# Transfer and Settings, so both activation and Settings' index must account
# for them. This normalization targets current CrossPoint's enum-based menu
# implementation and is idempotent on repeated patch runs.
python3 - "$HOME_CPP" "$HOME_H" <<'PY'
from pathlib import Path
import sys

home_cpp = Path(sys.argv[1])
home_h = Path(sys.argv[2])

text = home_cpp.read_text()
needle = """    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
"""
replacement = """    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    const int einqClockIndex = 3 + (hasOpdsServers ? 1 : 0);
    if (menuIndex == einqClockIndex) {
      onEinqClockOpen();
      return;
    }
    if (menuIndex == einqClockIndex + 1) {
      onEinqWifiOpen();
      return;
    }
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
"""
if needle in text:
    text = text.replace(needle, replacement, 1)
elif "const int einqClockIndex = 3 + (hasOpdsServers ? 1 : 0);" not in text:
    raise SystemExit("HomeActivity.cpp: could not normalize Einq menu activation")
home_cpp.write_text(text)

text = home_h.read_text()
text = text.replace(
    """    if (item == HomeMenuItem::FILE_TRANSFER) return i;
    ++i;
    if (item == HomeMenuItem::SETTINGS_MENU) return i;
""",
    """    if (item == HomeMenuItem::FILE_TRANSFER) return i;
    i += 3;  // Einq Clock, Einq WiFi, then Settings
    if (item == HomeMenuItem::SETTINGS_MENU) return i;
""",
    1,
)
text = text.replace(
    """    if (hasOpdsUrl && idx == i++) return HomeMenuItem::OPDS_BROWSER;
    if (idx == i++) return HomeMenuItem::FILE_TRANSFER;
    if (idx == i) return HomeMenuItem::SETTINGS_MENU;
""",
    """    if (hasOpdsUrl && idx == i++) return HomeMenuItem::OPDS_BROWSER;
    if (idx == i++) return HomeMenuItem::FILE_TRANSFER;
    i += 2;  // Einq Clock and Einq WiFi
    if (idx == i) return HomeMenuItem::SETTINGS_MENU;
""",
    1,
)
if "i += 3;  // Einq Clock, Einq WiFi, then Settings" not in text or \
        "i += 2;  // Einq Clock and Einq WiFi" not in text:
    raise SystemExit("HomeActivity.h: could not normalize Einq menu indices")
home_h.write_text(text)
print("normalized CrossPoint Home Einq navigation")
PY

echo "Einq patch applied (clock + BLE GATT; auto-start on boot; Back → CrossPoint home)"
