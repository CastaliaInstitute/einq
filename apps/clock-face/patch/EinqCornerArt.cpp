#include "EinqCornerArt.h"

#include "EinqCornerArtData.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {
constexpr int kMargin = 8;
constexpr int kContentGap = 8;

uint8_t seasonMaskForMonth(const int month) {
  if (month == 11 || month <= 1) return 1 << 0;
  if (month <= 4) return 1 << 1;
  if (month <= 7) return 1 << 2;
  return 1 << 3;
}

struct tm selectionTime() {
  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);
  if (localTime.tm_year + 1900 >= 2024) return localTime;

  // Offline first boot has Unix epoch time. Use the firmware build date until
  // NTP/RTC supplies a real date so a July build does not display winter art.
  static constexpr char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  localTime = {};
  for (int month = 0; month < 12; ++month) {
    if (strncmp(months + month * 3, __DATE__, 3) == 0) {
      localTime.tm_mon = month;
      break;
    }
  }
  localTime.tm_mday = atoi(__DATE__ + 4);
  localTime.tm_year = atoi(__DATE__ + 7) - 1900;
  localTime.tm_hour = 12;
  mktime(&localTime);  // Populate tm_yday.
  return localTime;
}

const EinqCornerArtData::ThemeAsset& selectTheme() {
  using namespace EinqCornerArtData;
  const struct tm localTime = selectionTime();
  const uint8_t season = seasonMaskForMonth(localTime.tm_mon);

  size_t eligibleCount = 0;
  for (size_t i = 0; i < kThemeCount; ++i) {
    if ((kThemes[i].seasonMask & season) != 0) ++eligibleCount;
  }
  if (eligibleCount == 0) return kThemes[0];

  // Offset by one so the seasonal feature occupies even-numbered day indices
  // while the all-season fallback occupies odd ones.
  size_t selected = (static_cast<size_t>(localTime.tm_yday) + 1) % eligibleCount;
  for (size_t i = 0; i < kThemeCount; ++i) {
    if ((kThemes[i].seasonMask & season) == 0) continue;
    if (selected-- == 0) return kThemes[i];
  }
  return kThemes[0];
}

void drawCorner(GfxRenderer& renderer, const EinqCornerArtData::CornerAsset& corner, int originX, int originY) {
  for (int y = 0; y < corner.height; ++y) {
    const int sourceY = corner.flipY ? corner.height - 1 - y : y;
    for (int x = 0; x < corner.width; ++x) {
      const int sourceX = corner.flipX ? corner.width - 1 - x : x;
      const uint8_t bits = pgm_read_byte(&corner.bitmap[sourceY * corner.rowBytes + (sourceX >> 3)]);
      if ((bits & (0x80 >> (sourceX & 7))) != 0) {
        renderer.drawPixel(originX + x, originY + y, true);
      }
    }
  }
}
}  // namespace

namespace EinqCornerArt {

const char* currentThemeId() { return selectTheme().id; }

void drawFourCorners(GfxRenderer& renderer, const int pageWidth, const int pageHeight) {
  const auto& theme = selectTheme();
  const auto& bottomLeft = theme.corners[0];
  const auto& bottomRight = theme.corners[1];
  const auto& topLeft = theme.corners[2];
  const auto& topRight = theme.corners[3];
  drawCorner(renderer, bottomLeft, kMargin, pageHeight - kMargin - bottomLeft.height);
  drawCorner(renderer, bottomRight, pageWidth - kMargin - bottomRight.width,
             pageHeight - kMargin - bottomRight.height);
  drawCorner(renderer, topLeft, kMargin, kMargin);
  drawCorner(renderer, topRight, pageWidth - kMargin - topRight.width, kMargin);
}

void contentInsets(const bool top, int& left, int& right) {
  const auto& corners = selectTheme().corners;
  left = kMargin + corners[top ? 2 : 0].width + kContentGap;
  right = kMargin + corners[top ? 3 : 1].width + kContentGap;
}

}  // namespace EinqCornerArt
